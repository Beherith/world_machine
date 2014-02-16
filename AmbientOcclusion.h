#pragma once
#include "Core\filter.h"

// A practical example filter. This device is act
class AmbientOcclusion :
	public Filter
{
public:
	AmbientOcclusion(void);
	virtual ~AmbientOcclusion(void);
	virtual bool Load(std::istream &in);
	virtual bool Save(std::ostream &out);

	virtual char *GetDescription() { return "Performs Ambient Occlusion";};
	virtual char *GetTypeName() { return "AmbientOcclusion"; };

	virtual bool Activate(BuildContext &context);
	
// Unless you need some special UI elements, I highly recommend _NOT_ overriding RunUI(), and letting your parameters be set by way
// of the generic dialog manager... this is also much easier and faster to develop.
//	virtual bool RunUI();
};
