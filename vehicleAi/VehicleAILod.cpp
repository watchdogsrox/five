#include "VehicleAILod.h"

// class header
#include "VehicleAI/VehicleAiLod.h"

// Rage headers
#include "grcore/debugdraw.h"

// Game headers
#include "Vehicles/Vehicle.h"
#include "Vehicles/vehicle_channel.h"
#include "VehicleAI/VehicleIntelligence.h"

AI_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

CVehicleAILod::CVehicleAILod()
: m_LodFlags(AL_DefaultLodFlags)
, m_BlockedLodFlags(AL_DefaultMissionBlockedLodFlags)
, m_TimeslicedUpdateThisFrame(false)
{
}

CVehicleAILod::~CVehicleAILod()
{
}

eVehicleDummyMode CVehicleAILod::GetDummyMode() const
{
	if( IsLodFlagSet(AL_LodDummy) )
	{
		Assert(!IsLodFlagSet(AL_LodSuperDummy));
		return VDM_DUMMY;
	}
	else if( IsLodFlagSet(AL_LodSuperDummy) )
	{
		return VDM_SUPERDUMMY;
	}
	else
	{
		return VDM_REAL;
	}
}
