#include "stdafx.h"
#include "Pillars.h"
#include "Core\HField.h"

// You must furnish your own Maker and Killer functions. The Maker should (obviously) be unique for each device,
// while the Killer can be shared among plugins in the same DLL. The basic idea is to get around the sometimes-messy
// problem of DLL<->Client heap management. The framework will call your maker and killers as appropriate.

Device *PillarsMaker() { return new Pillars; };
void PillarsKiller(Device *spr) { delete spr; };

// What you MUST DO in the constructor of your device:
// 1) Set the Nametag, Maker and Killer in the lifeptrs struct.
// What you SHOULD do in the constructor:
// 2) Call SetLinks() with the # of inputs and outputs your device is going to use
//      ...if you dont do #2, you will use the default # of inputs your parent class assigns
Pillars::Pillars(void)
{
	lifeptrs.maker = PillarsMaker;
	lifeptrs.killer = PillarsKiller;
	strncpy(lifeptrs.nametag, "PIL1", 4);
	SetLinks(3, 1);
// This is how you would add a parameter to a device that is managed by the generic dlg system
// This adds a floating point parameter. You can add floats, ints, bools, or enums.
	AddParam(Parameter("Dummy Parameter", 0.0f, 0.0f, 1.0f));
}

// This is a convenient way to access your device parameters
// Note that accessing parameters is really quite slow due to the string searching for the param name. 
// Do NOT call inside of loops if you care about performance -- just retrieve the value once and cache it.
#define INV_DUMMY			ParmFRef("Dummy Parameter")


Pillars::~Pillars(void)
{

}

// You are REQUIRED to write out and read in your 4-byte Device ID tag, as shown below. It is also recommended that your device
// have an internal version number that you keep track of to allow backwards-compatibility if you ever change your Device save layout...
// Also, make sure you call your super class's Load and Save functions as shown here. Failure to do so will cause major problems.
bool Pillars::Load(std::istream &in) {
	char tag[5];
	in.read( tag, 4);
	if (strncmp(tag, lifeptrs.nametag, 4) == 0) {
		int ver = 0;
		in.read((char*) &ver, 1);
		return Filter::Load(in);
	}
	else
		return false;
};

bool Pillars::Save(std::ostream &out) {
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
bool Pillars::Activate(BuildContext &context) {
	HFPointer hf_main = HF(RetrieveInput(0, context) );	// Use the RetrieveData(),StoreData() pair to get and set your heightfields
	HFPointer hf_voronoi = HF(RetrieveInput(1, context) );	// Use the RetrieveData(),StoreData() pair to get and set your heightfields
	HFPointer hf_pillarstrength = HF(RetrieveInput(2, context) );	// Use the RetrieveData(),StoreData() pair to get and set your heightfields
	HFPointer hf_walkassist =  GetNewHF(context);

	// This is important! An input CAN RETURN NULL if it is not connected, or the input is turned off. So always check and bail out if its not there
	if (!hf_voronoi)
		return false;	
	if (!hf_main)
		return false;
	if (!hf_walkassist)
		return false;

	//if (!hf_pillarstrength)
		//return false;
// ** example: if we actually DID anything with our dummy param, this is how you would get it
	float dummy = INV_DUMMY;

// ** example: If you wanted to access the data by coordinate instead of by index, here's how you would do it:
	int width = hf_voronoi->w();
	int height = hf_voronoi->h();
	//float dummy_height = (*hf_voronoi)[Coord(0,0)];	// use the coordinate version of the hf_voronoi array operator
	//size_t area = hf_voronoi->area(); // use size_t for heightfield iterating as it is possible for the index to exceed 32bits..

// heights are always in the range 0..1., it also doesn't care about world space location, so we can just do a simple iteration over the entire data field:

	float curr=-1;
	bool panic=false;
	for (size_t i=0; i<width; i++){
		for (size_t j=0; j<height;j++){ 
			size_t pos=j*height+i;

			if((*hf_voronoi)[pos]<0){
				continue;
			}else{
				curr=(*hf_voronoi)[pos];
				int avgx=i;
				int avgy=j;
				size_t pcnt=1;
				walk(curr, i,j,height,width,&avgx,&avgy,&pcnt,hf_voronoi,hf_main,hf_walkassist,-1,0,0);
				int phx=int(avgx/pcnt);
				int phy=int(avgy/pcnt);
				if (phx<0 || phx>=width || phy<0 || phy>= height){panic=true; break;};
				float pillarheight=(*hf_main)[ phx+ height*phy];
				float pillarstrength= 1.0;
				if (hf_pillarstrength){ //we sample the pillar strength at the same point as we source the pillar heights from. 
					pillarstrength=(*hf_pillarstrength)[ phx+ height*phy];
				}
				
				walk(curr, i,j,height,width,&avgx,&avgy,&pcnt,hf_voronoi,hf_main,hf_walkassist, pillarheight,pillarstrength, 0);
			}
		}
		if (panic) break;
	}


	hf_main->ClampRange();
	StoreData(hf_main,0, context); // pass the heightfield to our output stage.

// Success!
	return true;
}
void Pillars::walk(float curr,int x, int y, int height, int width, int * avgx, int * avgy, size_t* pcnt, HFPointer hf_voronoi, HFPointer hf_main, HFPointer hf_walkassist, float pheight, float pillarstrength, int depth){
	depth++;
	
	if (pheight<0){ // first walk
		if ((*hf_voronoi)[x +y*height] ==curr && ((*hf_walkassist)[x +y*height]>=0)){
			(*hf_walkassist)[x +y*height]-=2.0;
		
			*avgx+=x;
			*avgy+=y;
			*pcnt+=1;
			if (depth>2500) return; //MSVC2010 default max stack depth is 256kb, and with 60 bytes of stuff on the stack per call, this maxes at about 4500 calls. 2500 is a sensible max.
			if (x+1< width) walk(curr,min(x+1,width-1) ,y,height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,pillarstrength,depth);
			if (y+1<height) walk(curr,x,min(y+1,height-1),height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,pillarstrength,depth);
			//if (x>0)		walk(curr,x-1,y,height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,depth); //THERE IS NO TURNING BACK! We assume (hopefully) that all voronoi shapes are CONVEX!
			if (y>0)		walk(curr,x,max(0,y-1),height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,pillarstrength,depth);
		}
	}
	else{ //second walk
		if ((*hf_voronoi)[x +y*height] ==curr && (*hf_walkassist)[x +y*height] <-1.0 &&  (*hf_walkassist)[x +y*height] >=-4.0){
			(*hf_walkassist)[x +y*height]-=10.0;
			if (depth>2000) return;
			if (x+1< width) walk(curr,min(x+1,width-1) ,y,height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,pillarstrength,depth);
			if (y+1<height) walk(curr,x,min(y+1,height-1),height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,pillarstrength,depth);
			//if (x>0)		walk(curr,x-1,y,height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,depth); //
			if (y>0)		walk(curr,x,max(0,y-1),height, width, avgx,avgy,pcnt,hf_voronoi,hf_main,hf_walkassist,pheight,pillarstrength,depth);
			float originalheight=(*hf_main)[x +y*height];
			(*hf_main)[x +y*height]=(pheight*pillarstrength)+originalheight*(1.0f-pillarstrength);
		}
	}
}
