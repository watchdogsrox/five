
//
// camera/cinematic/CinematicInVehicleWantedContext.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//
#include "ai/EntityScanner.h"
#include "Peds/PedIntelligence.h"
#include "camera/cinematic/context/CinematicInVehicleWantedContext.h"
#include "camera/system/CameraMetadata.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"
#include "camera/cinematic/shot/CinematicTrainShot.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "system/control.h"

#include "Peds/ped.h"

INSTANTIATE_RTTI_CLASS(camCinematicInVehicleWantedContext,0x42D6FA11)

CAMERA_OPTIMISATIONS()

camCinematicInVehicleWantedContext::camCinematicInVehicleWantedContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleWantedContextMetadata&>(metadata))
, m_WasWanted(false)
{

}

bool camCinematicInVehicleWantedContext::IsValid(bool shouldConsiderControlInput, bool shouldConsiderViewMode) const
{
	if(camInterface::IsDeathFailEffectActive() && !NetworkInterface::IsInSpectatorMode())
	{
		return false;
	}

	if(IsPlayerWanted())
	{	
		if(ShouldActivateCinematicMode(shouldConsiderControlInput, shouldConsiderViewMode))	
		{
			return true;
		}
	}
	return false;
}

bool camCinematicInVehicleWantedContext::ShouldActivateCinematicMode(bool shouldConsiderControlInput, bool shouldConsiderViewMode) const
{
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
	camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 

	if(CStuntJumpManager::IsAStuntjumpInProgress())
	{
		return false;
	}

	if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
	{
		//the follow vehicle camera is not valid
		const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();

		if(followVehicleCamera)
		{
			//the follow vehicle is also not valid
			const CEntity* followVehicle = followVehicleCamera->GetAttachParent();
			if(!followVehicle || !followVehicle->GetIsTypeVehicle())
			{
				return false;
			}

			const CVehicle* pVehicle = static_cast<const CVehicle*>(followVehicle); 
			const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();

			if(!targetVehicleModelInfo->ShouldUseCinematicViewMode())
			{
				return false; 
			}
		}
		else
		{
			return false; 
		}

		if(!shouldConsiderControlInput)
		{
			//Bypass all further checks, relating to control input.
			return true;
		}

		if(shouldConsiderViewMode)
		{
			const camControlHelper* controlHelper = followVehicleCamera->GetControlHelper();
			if(controlHelper)
			{
				const s32 desiredViewMode = controlHelper->GetViewMode();
				if (desiredViewMode == camControlHelperMetadataViewMode::CINEMATIC)
				{
					return true;
				}
			}

		}

		if(cinematicDirector.IsCinematicInputActive())
		{
			return true; 
		}
	}

	return false; 
}

bool camCinematicInVehicleWantedContext::IsPlayerWanted() const
{
	const CPed* followPed = camInterface::FindFollowPed();	
	if(followPed && followPed->GetPlayerWanted())
	{
		if(followPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
		{
			return true; 
		}

	}
	return false; 
}

void camCinematicInVehicleWantedContext::PreUpdate()
{
	if(IsPlayerWanted())
	{	
		const CPed* followPed = camInterface::FindFollowPed();	
		
		if(followPed && followPed->IsNetworkClone())
		{
			CPed* nonConstPed = const_cast<CPed*>(followPed); 

			followPed->GetPedIntelligence()->GetPedScanner()->ScanForEntitiesInRange(*nonConstPed, false);
		}

		m_WasWanted = true; 
	}
}


void camCinematicInVehicleWantedContext::ClearContext()
{
	m_WasWanted = false; 
	camBaseCinematicContext::ClearContext(); 
}

bool camCinematicInVehicleWantedContext::CanActivateUsingQuickToggle()
{
	if(camBaseCinematicContext::CanActivateUsingQuickToggle())
	{
		camGameplayDirector& gameplayDirector				= camInterface::GetGameplayDirector();
		const camFollowVehicleCamera* followVehicleCamera	= gameplayDirector.GetFollowVehicleCamera();
		if(followVehicleCamera)
		{
			const camControlHelper* controlHelper = followVehicleCamera->GetControlHelper();
			if(controlHelper)
			{
				const s32 desiredViewMode = controlHelper->GetViewMode();
				if(desiredViewMode != camControlHelperMetadataViewMode::CINEMATIC)
				{
					return true;
				}
			}
		}
	}

	return false; 
}
