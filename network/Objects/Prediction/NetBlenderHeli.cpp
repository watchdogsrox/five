//
// name:		NetBlenderHeli.cpp
// written by:	Daniel Yelland
// description:	Network blender used by helicopters (and planes)
#include "network/objects/prediction/NetBlenderHeli.h"

CompileTimeAssert(sizeof(CNetBlenderHeli) <= LARGEST_NET_PHYSICAL_BLENDER_CLASS);

NETWORK_OPTIMISATIONS()

// ===========================================================================================================================
// CNetBlenderHeli
// ===========================================================================================================================
CNetBlenderHeli::CNetBlenderHeli(CVehicle *vehicle, CHeliBlenderData *blenderData) :
CNetBlenderVehicle(vehicle, blenderData)
{
}

CNetBlenderHeli::~CNetBlenderHeli()
{
}
