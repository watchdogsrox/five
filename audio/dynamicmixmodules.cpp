#include "dynamicmixmodules.h"
#include "dynamicmixer.h"
#include "scriptaudioentity.h"
#include "vehiclecollisionaudio.h"

#include "vehicles/vehicle.h"
#include "scene/world/GameWorld.h"

#include "debugaudio.h"

AUDIO_DYNAMICMIXING_OPTIMISATIONS()

// Helper functions ----------------------------------------------------------------

static f32 GetPlayerVehVelocity()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(playerVeh)
	{
		return playerVeh->GetVelocity().Mag();
	}
	return 0.f;
}

static f32 GetPlayerVehAirtime()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(playerVeh)
	{
		audVehicleAudioEntity * vehAudio = playerVeh->GetVehicleAudioEntity();
		if(vehAudio)
		{
			return (f32)(fwTimer::GetTimeInMilliseconds() - vehAudio->GetLastTimeOnGround());
		}
	}
	return 0.f;
}

static f32 GetPlayerVehUpsideDown()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(playerVeh)
	{
		if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR &&
			(playerVeh->IsUpsideDown()))
		{
			return 1.f;
		}
	}
	return 0.f;
}


// Scene Variable Mix Module -------------------------------------------------------------------------------

void audSceneVariableMixModule::Init(audDynMixModule * module)
{
	module->SetModuleCurve(module->m_SceneVariableSettings->InputOutputCurve);
	Update(module);
}

void audSceneVariableMixModule::Update(audDynMixModule * module)
{
	const SceneVariableModuleSettings * settings = module->m_SceneVariableSettings;

	f32 inputValue = 0.f;

	switch(settings->Input)
	{
	case INPUT_PLAYER_VEH_VELOCITY:
		{
			inputValue = GetPlayerVehVelocity();
		}
		break;
	case INPUT_PLAYER_VEH_AIRTIME:
		{
			inputValue = GetPlayerVehAirtime();
		}
		break;
	case INPUT_PLAYER_VEH_ROLL:
		{
			inputValue = GetPlayerVehUpsideDown();
		}
		break;
	default:
		break;
	}

	f32 outputValue = module->GetModuleCurve()->CalculateRescaledValue(0.f, 1.f, settings->ScaleMin, settings->ScaleMax, inputValue);
	module->GetScene()->SetVariableValue(settings->SceneVariable, outputValue);
}

void audSceneVariableMixModule::Shutdown(audDynMixModule * UNUSED_PARAM(module))
{

}

// Vehicle collision mix module ----------------------------------------------------------

void audVehicleCollisionMixModule::Init(audDynMixModule * UNUSED_PARAM(module))
{

}

void audVehicleCollisionMixModule::Update(audDynMixModule * UNUSED_PARAM(module))
{

}

void audVehicleCollisionMixModule::Shutdown(audDynMixModule * UNUSED_PARAM(module))
{

}

void audVehicleCollisionMixModule::Process(audDynMixModule * module, audVehicleCollisionAudio * collisionAudio)
{
	audVehicleCollisionContext * playContext = collisionAudio->GetCollisionContextList().GetEvents();
	bool vehicleOnLeftSide = false, buildingOnLeftSide = false;
	bool vehicleOnRightSide = false, buildingOnRightSide = false;

	while(playContext)
	{
		if(playContext->GetTypeFlag(AUD_VEH_COLLISION_LEFTSIDE) && playContext->otherEntity->GetIsTypeBuilding())
		{
			buildingOnLeftSide = true;
		}
		if(playContext->GetTypeFlag(AUD_VEH_COLLISION_RIGHTSIDE) && playContext->otherEntity->GetIsTypeBuilding())
		{
			buildingOnRightSide = true;
		}
		if(playContext->GetTypeFlag(AUD_VEH_COLLISION_LEFTSIDE) && playContext->otherEntity->GetIsTypeVehicle())
		{
			vehicleOnLeftSide = true;
		}
		if(playContext->GetTypeFlag(AUD_VEH_COLLISION_RIGHTSIDE) && playContext->otherEntity->GetIsTypeVehicle())
		{
			vehicleOnRightSide = true;
		}
	}

	switch(module->m_VehicleCollisionModuleSettings->Input)
	{
	case VEH_VEH_SIDES:
		if(vehicleOnLeftSide && vehicleOnRightSide)
		{
			//We're sandwiched between two vehicles
			//Assert(0); 
		}
		break;
	case VEH_BUILDING_SIDES:
		if((buildingOnRightSide && vehicleOnLeftSide) || (buildingOnLeftSide && vehicleOnRightSide))
		{
			//We're being squashed into a building wheeeeeeee
			//Assert(0); 
		}
	}

}

// Scene Transition Mix Module -------------------------------------------------------------------------------------------------

void audSceneTransitionMixModule::Init(audDynMixModule * UNUSED_PARAM(module))
{

}

void audSceneTransitionMixModule::Update(audDynMixModule * module)
{
	const SceneTransitionModuleSettings * settings = module->m_SceneTransitionSettings; 

	f32 inputValue = 0.f;
	switch(settings->Input)
	{
	case INPUT_PLAYER_VEH_VELOCITY:
		{
			inputValue = GetPlayerVehVelocity();
		}
		break;
	case INPUT_PLAYER_VEH_AIRTIME:
		{
			inputValue = GetPlayerVehAirtime();
		}
		break;
	case INPUT_PLAYER_VEH_ROLL:
		{
			inputValue = GetPlayerVehUpsideDown();
		}
		break;
	default:
		break;
	}
}

void audSceneTransitionMixModule::Shutdown(audDynMixModule * UNUSED_PARAM(module))
{

}


// audDynamicMixModuleInterface -------------------------------------------------------

audDynamicMixModuleInterface::audDynamicMixModuleInterface()
{
	sysMemSet(&m_VehiclePostCollisionMods[0], 0, sizeof(m_VehiclePostCollisionMods));
}

void audDynamicMixModuleInterface::AddVehicleCollisionModule(audDynMixModule * module)
{
	for(int i=0; i < k_max_mods; i++)
	{
		if(!m_VehiclePostCollisionMods[i])
		{
			m_VehiclePostCollisionMods[i] = module;
			return;
		}
	}
	naAssertf(0, "Ran out of room in m_VehiclePostCollisionMods");
}

void audDynamicMixModuleInterface::RemoveVehicleCollisionModule(audDynMixModule * module)
{
	for(int i=0; i < k_max_mods; i++)
	{
		if(m_VehiclePostCollisionMods[i] == module)
		{
			m_VehiclePostCollisionMods[i] = NULL;
		}
	}

	naAssertf(0, "Couldn't find module %s to remove from m_VehiclePostCollisionMods", module->GetModuleName());
}


void audDynamicMixModuleInterface::ProcessVehiclePostCollision(audVehicleCollisionAudio * collisionAudio)
{
	for(int i=0; i < k_max_mods; i++)
	{
		if(m_VehiclePostCollisionMods[i])
		{
			audVehicleAudioEntity * vehicleAudio = collisionAudio->GetParent();
			if(!m_VehiclePostCollisionMods[i]->GetMixGroup() ||
				m_VehiclePostCollisionMods[i]->GetMixGroup()->GetIndex() == vehicleAudio->GetEnvironmentGroup()->GetMixGroupIndex())
			{
				audVehicleCollisionMixModule::Process(m_VehiclePostCollisionMods[i], collisionAudio);
			}
		}
	}
}

