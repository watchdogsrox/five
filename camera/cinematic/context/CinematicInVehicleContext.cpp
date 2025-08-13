//
// camera/cinematic/CinematicAnimatedCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/CinematicInVehicleContext.h"
#include "camera/system/CameraMetadata.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"
#include "camera/cinematic/shot/CinematicTrainShot.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "vehicles/vehicle.h"
#include "network/NetworkInterface.h"
#include "peds/ped.h"
#include "peds/pedIntelligence.h"
#include "system/control.h"
#include "task/Default/TaskPlayer.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicles/train.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "task/Vehicle/TaskExitVehicle.h"

INSTANTIATE_RTTI_CLASS(camCinematicInVehicleContext,0xB2D9D264)
INSTANTIATE_RTTI_CLASS(camCinematicInVehicleConvertibleRoofContext,0xDA075DC1)
INSTANTIATE_RTTI_CLASS(camCinematicInTrainContext,0x55D14F19)
INSTANTIATE_RTTI_CLASS(camCinematicInTrainAtStationContext,0x49BEAA0A)
INSTANTIATE_RTTI_CLASS(camCinematicInVehicleMultiplayerPassengerContext,0xf6925c26)
INSTANTIATE_RTTI_CLASS(camCinematicInVehicleCrashContext,0x73e27426)

CAMERA_OPTIMISATIONS()

XPARAM(noIdleCam);

camCinematicInVehicleContext::camCinematicInVehicleContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleContextMetadata&>(metadata))
, m_PreviousIdleModeInvalidationTime(0)
, m_CinematicHeliDesiredPathHeading(0.0f)
{

}

void camCinematicInVehicleContext::InvalidateIdleMode()
{
	m_PreviousIdleModeInvalidationTime = fwTimer::GetTimeInMilliseconds();
}

bool camCinematicInVehicleContext::Update()
{
	return camBaseCinematicContext::Update(); 
}

void camCinematicInVehicleContext::PreUpdate()
{
	const bool isIdle = ComputeIsIdle();
	if(!isIdle)
	{
		m_PreviousIdleModeInvalidationTime = fwTimer::GetTimeInMilliseconds();
	}
}

bool camCinematicInVehicleContext::ComputeIsIdle() const
{
	//Invalid in single-player
	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Invalid when driving.
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const CPed* followPed					= gameplayDirector.GetFollowPed();
	if(followPed && followPed->GetIsDrivingVehicle())
	{
		return false;
	}
	
	//reset if the player is wanted dont allow idle if wanted
	if(followPed && followPed->GetPlayerWanted())
	{
		if(followPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
		{
			return false; 
		}

	}

	//Invalid if there was control input in the last second.
	CControl* control	= gameplayDirector.GetActiveControl();
	const bool isIdle	= (!control || (control->InputHowLongAgo() > 1000));

	return isIdle;
}

bool camCinematicInVehicleContext::IsValid(bool shouldConsiderControlInput, bool shouldConsiderViewMode) const
{
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
	camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 

	const CPed* followPed = camInterface::FindFollowPed();
	
	if(followPed && followPed->GetPlayerWanted())
	{
		if(followPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
		{
			return false; 
		}
	}
	else
	{
		return false; 
	}
	if(CStuntJumpManager::IsAStuntjumpInProgress())
	{
		return false;
	}

	if(camInterface::IsDeathFailEffectActive() && !NetworkInterface::IsInSpectatorMode())
	{
		return false;
	}

	if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
	{
		//the follow vehicle camera is not valid
		const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();
		
		//B*2082308: Allow cinematic camera if we're in a vehicle turret (we use the third person aim camera for this, not the vehicle follow camera)
		const camThirdPersonAimCamera* thirdPersonAimCamera = !followVehicleCamera && camInterface::GetGameplayDirector().IsUsingVehicleTurret(true) ? camInterface::GetGameplayDirector().GetThirdPersonAimCamera() : NULL;

#if FPS_MODE_SUPPORTED
        //B*2330081: Allow cinematic camera if we're in a vehicle in FPS mode.
        const camFirstPersonShooterCamera* firstPersonCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		if(followVehicleCamera || thirdPersonAimCamera || firstPersonCamera)
#else
        if(followVehicleCamera || thirdPersonAimCamera )
#endif
		{
			//the follow vehicle is also not valid
			const CEntity* followVehicle = NULL;
			if (followVehicleCamera)
			{
				followVehicle = followVehicleCamera->GetAttachParent();
			}
			else if (thirdPersonAimCamera)
			{
				// Attach parent for the third person camera is the ped, so get the vehicle from the ped not the camera
				followVehicle = static_cast<CEntity*>(followPed->GetVehiclePedInside());
			}
#if FPS_MODE_SUPPORTED
            else if(firstPersonCamera)
            {
                if( firstPersonCamera->GetAttachParent()->GetIsTypePed() )
                {
                    const CPed* pPed = static_cast<const CPed*>(firstPersonCamera->GetAttachParent());
                    followVehicle = pPed->GetVehiclePedInside();
                }
                else
                {
                    followVehicle = firstPersonCamera->GetAttachParent();
                }
            }
#endif

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

#if FPS_MODE_SUPPORTED
			// B*2080811: Don't allow cinematic camera while using mobile phone in FPS mode (as we hide the HUD on screen).
			if (gameplayDirector.IsFirstPersonModeEnabled() && followPed && followPed->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone))
			{
				return false;
			}
#endif	// FPS_MODE_SUPPORTED
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

		if(cinematicDirector.IsCinematicInputActive())
		{
			return true;
		}
		else if(shouldConsiderViewMode)
		{
			const camControlHelper* controlHelper = NULL;
			if (followVehicleCamera)
			{
				controlHelper = followVehicleCamera->GetControlHelper();
			}
			else if (thirdPersonAimCamera)
			{
				controlHelper = thirdPersonAimCamera->GetControlHelper();
			}

			if(controlHelper)
			{
				const s32 desiredViewMode = controlHelper->GetViewMode();
				if (desiredViewMode == camControlHelperMetadataViewMode::CINEMATIC)
				{
					return true;
				}
			}
		}

		if(!cinematicDirector.IsVehicleIdleModeDisabledThisUpdate() && !PARAM_noIdleCam.Get())
		{
			//Has enough idle time passed to activate the idle mode?
			const u32 time				= fwTimer::GetTimeInMilliseconds();	
			const u32 idleTimeExpired	= time - m_PreviousIdleModeInvalidationTime;
			if(idleTimeExpired >= cinematicDirector.GetTimeBeforeCinematicVehicleIdleCam())
			{	
				return true;
			}
		}
	}

	return false; 
}

camCinematicInVehicleConvertibleRoofContext::camCinematicInVehicleConvertibleRoofContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleConvertibleRoofContextMetadata&>(metadata))
{

}

bool camCinematicInVehicleConvertibleRoofContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();
	if(followVehicleCamera)
	{
		const CEntity* followVehicle = followVehicleCamera->GetAttachParent();
		if(followVehicle && followVehicle->GetIsTypeVehicle())
		{
			const CVehicle* targetVehicle = NULL;

			targetVehicle = static_cast<const CVehicle*>(followVehicle);

			if(targetVehicle && targetVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
			{
				if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISED ||
					targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED ||
					targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING ||  
					targetVehicle->GetConvertibleRoofState()  == CTaskVehicleConvertibleRoof::STATE_RAISING )
				{
					return true; 
				}
			}
		}
	}
	return false;
}

// Train

camCinematicInTrainContext::camCinematicInTrainContext(const camBaseObjectMetadata& metadata)
: camCinematicInVehicleContext(metadata)
, m_Metadata(static_cast<const camCinematicInTrainContextMetadata&>(metadata))
{

}

bool camCinematicInTrainContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	//camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	//const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
	//camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 

	const CPed* followPed = camInterface::FindFollowPed();

	if(followPed)
	{
		const CQueriableInterface* PedQueriableInterface = followPed->GetPedIntelligence()->GetQueriableInterface();

		if(PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_ON_FOOT))
		{
			const CTaskPlayerOnFoot* PlayerOnFootTask	= static_cast<const CTaskPlayerOnFoot*>(followPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
			if(PlayerOnFootTask && PlayerOnFootTask->GetState() == CTaskPlayerOnFoot::
				STATE_RIDE_TRAIN)
			{
				const CTrain* pTrain = GetTrainPlayerIsIn(); 
				if(pTrain)
				{
					const CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario *>(
						PlayerOnFootTask->FindSubTaskOfType(CTaskTypes::TASK_USE_SCENARIO));
					if(!pTaskUseScenario || (pTaskUseScenario->GetState() != CTaskUseScenario::State_Exit))
					{
						return true;
					}
				}
			}
		}
	}
	return false; 
}

const CTrain* camCinematicInTrainContext::GetTrainPlayerIsIn() const
{
	const CPed* followPed = camInterface::FindFollowPed();

	if(followPed)
	{

		//Ensure the entity is valid.
		const CEntity* pEntity = followPed->GetGroundPhysical();
		if(!pEntity)
		{
			pEntity = (const CEntity *)followPed->GetAttachParent();
			if(!pEntity)
			{
				return NULL;
			}
		}

		//Ensure the entity is physical.
		if(!pEntity->GetIsPhysical())
		{
			return NULL;
		}

		//Ensure the physical is a vehicle.
		const CPhysical* pPhysical = static_cast<const CPhysical *>(pEntity);
		if(!pPhysical->GetIsTypeVehicle())
		{
			return NULL;
		}

		//Ensure the vehicle is a train.
		const CVehicle* pVehicle = static_cast<const CVehicle *>(pPhysical);
		if(!pVehicle->InheritsFromTrain())
		{
			return NULL;
		}
		
		const CTrain* pTrain = static_cast<const CTrain *>(pVehicle);

		//Ensure the player is inside the train.
		spdAABB boundBox;
		if(!pTrain->GetBoundBox(boundBox).ContainsPoint(followPed->GetTransform().GetPosition()))
		{
			return NULL;;
		}

		//TODO: HACK:	The metro train bounding box extends well over the top of the car, and we want to 
		//				be able to stand on top of it and move around.  Inside, though, we want to disable control.
		static atHashWithStringNotFinal s_hMetroTrain("metrotrain",0x33C9E158);
		if(pTrain->GetArchetype()->GetModelNameHash() == s_hMetroTrain.GetHash())
		{
			if(followPed->GetTransform().GetPosition().GetZf() > (pTrain->GetTransform().GetPosition().GetZf() + 2.0f))
			{
				return NULL;;
			}
		}
		return pTrain; 
	}
	return NULL; 
}

//Train at station

camCinematicInTrainAtStationContext::camCinematicInTrainAtStationContext(const camBaseObjectMetadata& metadata)
: camCinematicInTrainContext(metadata)
, m_Metadata(static_cast<const camCinematicInTrainAtStationContextMetadata&>(metadata))
, m_TimeTrainLeftStation(0)
, m_stationState(NONE)
, m_HaveGotTimeTrainLeftStation(false)
, m_bTrainIsApproachingStation(false)
, m_bIsTrainLeavingStation(false)
{
}

void camCinematicInTrainAtStationContext::PreUpdate()
{
	const CPed* followPed = camInterface::FindFollowPed();

	if(followPed)
	{
		const CTrain* pVehicle = GetTrainPlayerIsIn(); 
		
		if(pVehicle && pVehicle->GetIsTypeVehicle())
		{
			const CTrain* pTrain = static_cast<const CTrain*>(pVehicle); 
			
			if(pTrain->GetTrainState() == CTrain::TS_LeavingStation)
			{
				m_TimeTrainLeftStation = fwTimer::GetCamTimeInMilliseconds(); 
				m_stationState = LEAVING_STATION; 
			}
			
			switch (m_stationState)
			{
				case LEAVING_STATION:
				{
					if(followPed)
					{
						const CQueriableInterface* PedQueriableInterface = followPed->GetPedIntelligence()->GetQueriableInterface();

						if(PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_ON_FOOT))
						{
							const CTaskPlayerOnFoot* PlayerOnFootTask	= static_cast<const CTaskPlayerOnFoot*>(followPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
							if(PlayerOnFootTask && PlayerOnFootTask->GetState() == CTaskPlayerOnFoot::STATE_RIDE_TRAIN)
							{
								const CTrain* pTrain = GetTrainPlayerIsIn(); 
								if(pTrain && pTrain->GetIsTypeVehicle())
								{
									m_bIsTrainLeavingStation = IsTrainLeavingTheStation();

									if(!m_bIsTrainLeavingStation)
									{
										m_stationState = ARRIVING_STATION; 
									}
								}
							}
							else
							{
								m_stationState = NONE; 
							}
						}
						else
						{
							m_stationState = NONE; 
						}

					}
					else
					{
						m_stationState = NONE; 
					}
				}
				break; 

				case ARRIVING_STATION:
					{
						if(IsTrainApproachingStation(pTrain))
						{
							m_bTrainIsApproachingStation = true;
						}

						const CPed* followPed = camInterface::FindFollowPed();

						if(followPed)
						{
							const CQueriableInterface* PedQueriableInterface = followPed->GetPedIntelligence()->GetQueriableInterface();

							if(PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_ON_FOOT))
							{
								const CTaskPlayerOnFoot* PlayerOnFootTask	= static_cast<const CTaskPlayerOnFoot*>(followPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
								if(PlayerOnFootTask && PlayerOnFootTask->GetState() != CTaskPlayerOnFoot::STATE_RIDE_TRAIN)
								{
									m_bTrainIsApproachingStation = false;
									m_stationState = NONE; 
								}
							}
							else
							{
								m_stationState = NONE; 
							}
						}
						else
						{
							m_stationState = NONE; 
						}
					}
					break; 
			
				default:
					{
						m_bTrainIsApproachingStation = false; 
						m_bIsTrainLeavingStation = false; 
					}
					break; 
			};
		}
	}
}


bool camCinematicInTrainAtStationContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	//camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	//const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
	//camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 

	const CPed* followPed = camInterface::FindFollowPed();

	if(followPed)
	{
		if(m_bTrainIsApproachingStation)
		{
			return true; 
		}
		
		if(m_bIsTrainLeavingStation)
		{
			return true;
		}
	}
	return false;
}

bool camCinematicInTrainAtStationContext::IsTrainLeavingTheStation() const 
{
	if(fwTimer::GetCamTimeInMilliseconds() - m_TimeTrainLeftStation <  m_Metadata.m_TimeLeavingStation)
	{
		return true; 
	}
	return false; 
}

bool camCinematicInTrainAtStationContext::IsTrainApproachingStation(const CTrain* pTrain) const
{
	if(pTrain)
	{	
		const CTrain* pEngine = pTrain->FindEngine(pTrain); 
		
		if(pEngine)
		{
			u32 time = (u32)(pEngine->m_travelDistToNextStation / pEngine->m_trackForwardSpeed *1000.0f); 

			if(time < m_Metadata.m_TimeApproachingStation)
			{
				return true; 
			}
		}
	}
	return false; 
}

//Multi player passenger

camCinematicInVehicleMultiplayerPassengerContext::camCinematicInVehicleMultiplayerPassengerContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleMultiplayerPassengerContextMetadata&>(metadata))
{
}

bool camCinematicInVehicleMultiplayerPassengerContext::IsValid(bool shouldConsiderControlInput, bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	if(NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInSpectatorMode())
	{
		camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();
		camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector(); 

		const CPed* followPed = camInterface::FindFollowPed();

		if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
		{
			if(followPed && followPed->GetPedIntelligence())
			{
				const CQueriableInterface* PedQueriableInterface = followPed->GetPedIntelligence()->GetQueriableInterface();

				if(PedQueriableInterface && PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
				{
					return false;
				}
			}	

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

				if(!followPed->GetIsDrivingVehicle())
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(followVehicle); 
					
					const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();

					int seatIndex = followPed->GetAttachCarSeatIndex(); 
					
					if(targetVehicleModelInfo)
					{
						u32 vehicleModelHash = targetVehicleModelInfo->GetModelNameHash(); 

						const camVehicleCustomSettingsMetadata* pCustomVehicleSettings = camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash); 

						if(pCustomVehicleSettings && pCustomVehicleSettings->m_MultiplayerPassengerCameraSettings.m_ShouldConsiderData)
						{	
							if(pCustomVehicleSettings->m_MultiplayerPassengerCameraSettings.m_InvalidSeatIndices.Find((u32)seatIndex) >= 0)
							{
								return false;
							}
						}
						else
						{
							return false;
						}
					}
				}
				else
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

			if(cinematicDirector.IsCinematicInputActive())
			{
				return true;
			}
		}
	}
	return false; 
}

bool camCinematicInVehicleMultiplayerPassengerContext::Update()
{
	return camBaseCinematicContext::Update(); 
}


//crash Camera
camCinematicInVehicleCrashContext::camCinematicInVehicleCrashContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleCrashContextMetadata&>(metadata))
, m_bCanActivate(false)
{
}

bool camCinematicInVehicleCrashContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();

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

		return true;
			
	}
	return false; 
}
