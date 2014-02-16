#include "stdafx.h"
#include "AmbientOcclusion.h"
#include "Core\HField.h"
#include "Core\SimpleRand.h"

// You must furnish your own Maker and Killer functions. The Maker should (obviously) be unique for each device,
// while the Killer can be shared among plugins in the same DLL. The basic idea is to get around the sometimes-messy
// problem of DLL<->Client heap management. The framework will call your maker and killers as appropriate.

Device *AmbientOcclusionMaker() { return new AmbientOcclusion; };
void AmbientOcclusionKiller(Device *spr) { delete spr; };

// What you MUST DO in the constructor of your device:
// 1) Set the Nametag, Maker and Killer in the lifeptrs struct.
// What you SHOULD do in the constructor:
// 2) Call SetLinks() with the # of inputs and outputs your device is going to use
//      ...if you dont do #2, you will use the default # of inputs your parent class assigns
AmbientOcclusion::AmbientOcclusion(void)
{
	lifeptrs.maker = AmbientOcclusionMaker;
	lifeptrs.killer = AmbientOcclusionKiller;
	strncpy(lifeptrs.nametag, "AO01", 4);
	SetLinks(1, 1);
// This is how you would add a parameter to a device that is managed by the generic dlg system
// This adds a floating point parameter. You can add floats, ints, bools, or enums.
	AddParam(Parameter("Ray count", 256, 1, 4096));
	AddParam(Parameter("Max ray distance", 128, 1, 1024));
	AddParam(Parameter("Min ray distance", 10, 0, 512));
	AddParam(Parameter("Map Height", 513.0f,0.0f, 1024.0f));
	AddParam(Parameter("Clamp edge heights", false));
	
	params.GetParam(0)->setHelpString("Choose the number of rays to cast per pixel"); 
	params.GetParam(1)->setHelpString("Maximum distance (in pixels) a ray will travel"); 
	params.GetParam(2)->setHelpString("Minimum distance before a ray hit is checked (useful for edge highlighting)"); 
	params.GetParam(3)->setHelpString("Map height scaling factor (in pixels)"); 
	params.GetParam(4)->setHelpString("Clamp the outside of the terraint to the edges of the terrain"); 
	
}

// This is a convenient way to access your device parameters
// Note that accessing parameters is really quite slow due to the string searching for the param name. 
// Do NOT call inside of loops if you care about performance -- just retrieve the value once and cache it.
#define RAYCNT			ParmIRef("Ray count")
#define MAXRAY			ParmIRef("Max ray distance")
#define MINRAY			ParmIRef("Min ray distance")
#define CLAMPEDGE		ParmBRef("Clamp edge heights")
#define MAPHEIGHT		ParmFRef("Map Height")


AmbientOcclusion::~AmbientOcclusion(void)
{

}

// You are REQUIRED to write out and read in your 4-byte Device ID tag, as shown below. It is also recommended that your device
// have an internal version number that you keep track of to allow backwards-compatibility if you ever change your Device save layout...
// Also, make sure you call your super class's Load and Save functions as shown here. Failure to do so will cause major problems.
bool AmbientOcclusion::Load(std::istream &in) {
	char tag[5];
	in.read( tag, 4);
	if (strncmp(tag, lifeptrs.nametag, 4) == 0) {
		int ver = 1;
		in.read((char*) &ver, 1);
		return Filter::Load(in);
	}
	else
		return false;
};

bool AmbientOcclusion::Save(std::ostream &out) {
	out.write( lifeptrs.nametag, 4);
	int ver = 1;
	out.write((char*) &ver, 1);
	return Filter::Save(out);
};

// The Activate function is where you work your magic. You should do four things in the Activate function:
// 1) Retrieve your inputs by calling RetrieveData(input#)
// 2) Do whatever you will do on it...
// 3) Store the data to make it available to your outputs
// 4) Return true to indicate successful activation, or false if there was an error.
bool AmbientOcclusion::Activate(BuildContext &context) {
	HFPointer hf = HF(RetrieveInput(0, context) );	// Use the RetrieveData(),StoreData() pair to get and set your heightfields

		// This is important! An input CAN RETURN NULL if it is not connected, or the input is turned off. So always check and bail out if its not there
	if (!hf)
		return false;
	HFPointer hfao = GetNewHF(context);

	int width = hf->w();
	int height = hf->h();
	//float dummy_height = (*hf)[Coord(0,0)];	// use the coordinate version of the hf array operator
	//size_t area = hf->area(); // use size_t for heightfield iterating as it is possible for the index to exceed 32bits..

	float * randomVectors; //random vectors

// heights are always in the range 0..1., it also doesn't care about world space location, so we can just do a simple iteration over the entire data field:
	int raycnt = RAYCNT;
	int maxray = MAXRAY;
	int minray = MINRAY;
	float mapheight = MAPHEIGHT;
	float currentHeight=0.0;
	bool clampedge = CLAMPEDGE;
	WorldMachine::SimpleRand SR;
	

	randomVectors=new float[raycnt*3];

	for (int i =0; i<raycnt; i++){
		float len =2;
		while (len >1){ //if we discard any vectors that are longer than 1 in our [2,2,2] sized cube, then we get an even distribution of random vectors in the sphere.
			randomVectors[3*i]= 2.0f* SR.nextF() - 1.0f;
			randomVectors[3*i+1]=2.0f* SR.nextF() - 1.0f;
			randomVectors[3*i+2]=2.0f* SR.nextF() - 1.0f;
			len=sqrt(randomVectors[3*i]*randomVectors[3*i]+randomVectors[3*i+1]*randomVectors[3*i+1]+randomVectors[3*i+2]*randomVectors[3*i+2]);
		}
		//normalization of the vectors onto the unit sphere
		randomVectors[3*i]=randomVectors[3*i]/len;
		randomVectors[3*i+1]=randomVectors[3*i+1]/len;
		randomVectors[3*i+2]=randomVectors[3*i+2]/len;
		//TODO: randomize vector lengths between [1, 0.5] so that the log search along the ray is distributed evenly, and not in clamshells
	}
	
	for (size_t x = 0; x < width; x++){
		context.ReportDeviceProgress(this, x, width);  
		for (size_t y = 0; y < height; y++){
			size_t pos = y*width+x;
			currentHeight=(*hf)[pos];
			int occlusion=raycnt;
			for (int r=0; r<raycnt; r++){ //ordo linear with ray count
				float rx=randomVectors[3*r];
				float ry=randomVectors[3*r+1];
				float rz=randomVectors[3*r+2];
				for (int l=minray; l<maxray;l=l*2){ //ordo log maxray with ray length 
					//might be faster if we check the most distant positions in the rays first
					int lx=x+rx*l; //ray x position
					int ly=y+ry*l; //ray y position
					float lz=currentHeight*mapheight+rz*l; //ray height
					if (lz <0){ //if ray height <0 the ray is obviously occluded
						occlusion--;
						break;
					}
					if (clampedge){ //clamp edge values if the option is selected
						lx=min(max(0,lx),width-1);
						ly=min(max(0,ly),height-1);
					}else{		
						if (lx<0 || lx>width || ly<0 || ly>height){
							if (rz<0) occlusion--; //if we are out of bounds, and the vector points down (-z), count it as occluded
							break;
						}	
					}
					float rayh=lz/mapheight;
					if( rayh < (*hf)[size_t(ly*width+lx)]){
						occlusion--;
						break;
					}
				}
			}
			(*hfao)[pos]= (float(occlusion)/ float(raycnt));
		} 
	}
	
	delete randomVectors;

	hfao->ClampRange();
	StoreData(hfao,0, context); // pass the heightfield to our output stage.

// Success!
	return true;
}
