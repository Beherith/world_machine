#pragma once
#include "Core\filter.h"

// A practical example filter. This device is act
class Pillars :
	public Filter
{
public:
	Pillars(void);
	virtual ~Pillars(void);
	virtual bool Load(std::istream &in);
	virtual bool Save(std::ostream &out);

	virtual char *GetDescription() { return "Makes pillars in voronoi noise";};
	virtual char *GetTypeName() { return "Pillars"; };
	virtual char *GetInputName(int slot){
		if (slot==0) return "Primary heightfield input";
		if (slot==1) return "Voronoi cells input";
		if (slot==2) return "Pillar strength mask";
	};

	virtual bool Activate(BuildContext &context);
	void walk(float curr,int x, int y, int height, int width, int * avgx, int * avgy, size_t* pcnt, HFPointer hf, HFPointer hf2, HFPointer hf3, float pheight,float pillarstrength,int depth);

// Unless you need some special UI elements, I highly recommend _NOT_ overriding RunUI(), and letting your parameters be set by way
// of the generic dialog manager... this is also much easier and faster to develop.
//	virtual bool RunUI();
};
