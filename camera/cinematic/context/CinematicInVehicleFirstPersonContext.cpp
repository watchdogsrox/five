//
// camera/cinematic/camCinematicInVehicleFirstPersonContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicInVehicleFirstPersonContext.h"
#include "camera/cinematic/shot/CinematicInVehicleShot.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "Peds/Ped.h" 

INSTANTIATE_RTTI_CLASS(camCinematicInVehicleFirstPersonContext,0x3151FECC)

CAMERA_OPTIMISATIONS()

camCinematicInVehicleFirstPersonContext::camCinematicInVehicleFirstPersonContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleFirstPersonContextMetadata&>(metadata))
, m_fSeatSwitchSpringResult(0.0f)
, m_fSeatSwitchSpringVelocity(0.0f)
, m_IsValid(true)
, m_bIsMountedCameraDataValid(false)
{
}

void camCinematicInVehicleFirstPersonContext::PreUpdate()
{
	camBaseCinematicShot* pShot = GetCurrentShot(); 

	if(pShot && pShot->GetIsClassId(camCinematicVehicleBonnetShot::GetStaticClassId()))
	{
		camCinematicVehicleBonnetShot* pBonnet = static_cast<camCinematicVehicleBonnetShot*>(pShot); 

		if(pBonnet->HasSeatChanged())
		{
			m_IsValid = false; 
		}
	}
}

bool camCinematicInVehicleFirstPersonContext::IsValid(bool shouldConsiderControlInput, bool shouldConsiderViewMode) const
{
	if(!m_IsValid)
	{
		return false; 
	}
	
	if(!shouldConsiderControlInput || !shouldConsiderViewMode)
	{
		return false;
	}

	if(camInterface::IsDeathFailEffectActive() && !NetworkInterface::IsInSpectatorMode())
	{
		return false;
	}

	// Modified from naEnvironment::AreWeAPassengerInATaxi()
	const CPed* pPed = camInterface::FindFollowPed();
	if(pPed && pPed->GetPlayerInfo() && pPed->GetVehiclePedInside())
	{
		if(CVehicle::IsTaxiModelId(pPed->GetVehiclePedInside()->GetModelId()))
		{
			// see if the player has a car drive task, otherwise they are just a passenger
			if(pPed->GetAttachCarSeatIndex() > 1 && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
			{
				// Do not switch to vehicle camera if still entering a taxi.
				return false;
			}
		}
	}


	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
#if FPS_MODE_SUPPORTED
	const bool shouldUseFirstPersonDriveBy	= gameplayDirector.ComputeShouldUseFirstPersonDriveBy();
#else // FPS_MODE_SUPPORTED
	const bool shouldUseFirstPersonDriveBy	= false;
#endif // FPS_MODE_SUPPORTED

	const bool bPuttingOnHelmet = pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet);

	const camThirdPersonAimCamera* thirdPersonAimCamera = gameplayDirector.GetThirdPersonAimCamera();

	camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 
	if (!cinematicDirector.IsFirstPersonInVehicleDisabledThisUpdate() && (!cinematicDirector.IsCinematicInputActive() || (shouldUseFirstPersonDriveBy && thirdPersonAimCamera) || bPuttingOnHelmet))
	{
		const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
		if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE)
		{
		#if FPS_MODE_SUPPORTED
			if( gameplayDirector.IsFirstPersonModeEnabled() && gameplayDirector.GetFirstPersonShooterCamera() )
			{
				const camFirstPersonShooterCamera* pFpsCamera = gameplayDirector.GetFirstPersonShooterCamera();
				if( !pFpsCamera->IsEnteringTurretSeat() || (pFpsCamera->IsEnteringTurretSeat() && pFpsCamera->IsBlendingForVehicle()) )
				{
					// If trying to be in first person camera, enable in car cinematic camera when in a vehicle
					// unless we are entering turret seat.
					return true;
				}
			}
		#endif // FPS_MODE_SUPPORTED

			const camControlHelper* controlHelper = NULL;

			const camFollowVehicleCamera* followVehicleCamera = gameplayDirector.GetFollowVehicleCamera();
			if(followVehicleCamera)
			{
				const bool shouldForceFirstPersonViewMode = followVehicleCamera->ShouldForceFirstPersonViewMode();
				if(shouldForceFirstPersonViewMode)
				{
					return true;
				}

				controlHelper = followVehicleCamera->GetControlHelper();
			}
			else if(shouldUseFirstPersonDriveBy && thirdPersonAimCamera)
			{
				controlHelper = thirdPersonAimCamera->GetControlHelper();
			}

			if(controlHelper == NULL && gameplayDirector.IsUsingVehicleTurret(true) && thirdPersonAimCamera && thirdPersonAimCamera->GetIsClassId( camThirdPersonPedAimCamera::GetStaticClassId() ))
			{
				const camThirdPersonPedAimCamera* pThirdPersonPedAimCamera = (const camThirdPersonPedAimCamera*)thirdPersonAimCamera;
				if(pThirdPersonPedAimCamera->GetIsTurretCam())
				{
					controlHelper = pThirdPersonPedAimCamera->GetControlHelper();
				}
			}

			if(controlHelper)
			{
				const s32 desiredViewMode = controlHelper->GetViewMode();
				if(desiredViewMode == camControlHelperMetadataViewMode::FIRST_PERSON || camInterface::GetGameplayDirector().IsForcingFirstPersonBonnet())
				{
					return true; 
				}
			}
		}
	}
	return false; 
}

void camCinematicInVehicleFirstPersonContext::PreCameraUpdate(camCinematicMountedCamera* pMountedCamera)
{
	if(m_bIsMountedCameraDataValid && pMountedCamera != NULL)
	{
		pMountedCamera->GetSeatSwitchSpringNonConst().OverrideResult( m_fSeatSwitchSpringResult ); 
		pMountedCamera->GetSeatSwitchSpringNonConst().OverrideVelocity( m_fSeatSwitchSpringVelocity );
		pMountedCamera->ResetAllowSpringCutOnFirstUpdate();
		m_bIsMountedCameraDataValid = false;
	}
}

void camCinematicInVehicleFirstPersonContext::ClearContext()
{
	if(GetCamera() && GetCamera()->GetIsClassId( camCinematicMountedCamera::GetStaticClassId() ))
	{
		const camCinematicMountedCamera* pMountedCamera = static_cast<const camCinematicMountedCamera*>(GetCamera());
		m_bIsMountedCameraDataValid = true;
		m_fSeatSwitchSpringResult = pMountedCamera->GetSeatSwitchSpring().GetResult();
		m_fSeatSwitchSpringVelocity = pMountedCamera->GetSeatSwitchSpring().GetVelocity();
	}

	camBaseCinematicContext::ClearContext(); 
	m_IsValid = true; 
}

