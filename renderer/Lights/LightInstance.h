#ifndef LIGHT_INSTANCE_H_
#define LIGHT_INSTANCE_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers

// game headers
#include "scene/2dEffect.h"

// framework headers
#include "entity/extensionlist.h"
#include "fwtl/pool.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

// =============================================================================================== //
// CLASS
// =============================================================================================== //
class CLightInstance : public fwExtension
{
public:

	EXTENSIONID_DECL(CLightInstance, fwExtension);
	FW_REGISTER_CLASS_POOL(CLightInstance);

	// Variables
	CLightAttr m_lightAttr;
	
	// Functions
	CLightInstance(const CLightAttr *lightAttr);
	~CLightInstance();
};

#endif