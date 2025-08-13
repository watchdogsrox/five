//
// camera/cinematic/shot/camBaseCinematicShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/BaseCinematicShot.h"
#include "camera/cinematic/context/CinematicScriptContext.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/context/CinematicInVehicleFirstPersonContext.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/system/CameraMetadata.h"

#include "vehicles/vehicle.h"
#include "peds/ped.h"

INSTANTIATE_RTTI_CLASS(camBaseCinematicShot,0xCE7F4CBE)

CAMERA_OPTIMISATIONS()

camBaseCinematicShot::camBaseCinematicShot(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camCinematicShotMetadata&>(metadata))
, m_UpdatedCamera(NULL)
, m_AttachEntity(NULL)
, m_LookAtEntity(NULL)
, m_pContext(NULL)
, m_CurrentMode(0)
, m_ShotStartTime(0)
, m_ShotEndTime(0)
, m_Priority(0)
{
}

void camBaseCinematicShot::Init(camBaseCinematicContext* pContext, u32 Priority, float Probability)
{
	m_pContext = pContext; 
	m_Priority = Priority; 
	m_ProbabilityWeighting = Probability; 
}

bool camBaseCinematicShot::CreateAndTestCamera(u32 CameraRef)
{
	bool isValid = false;

	if(CanCreate())
	{
		if(m_UpdatedCamera == NULL)
		{
			if(cameraVerifyf(!m_UpdatedCamera, "Attempted to overwrite an existing cinematic camera"))
			{
				m_UpdatedCamera = camFactory::CreateObject<camBaseCamera>(CameraRef);
			}

			if(cameraVerifyf(m_UpdatedCamera, "Failed to create a cinematic camera (hash: %u)", CameraRef ))
			{
				u32 numOfModes = m_UpdatedCamera->GetNumOfModes(); 
				
				if(numOfModes > 0)
				{
					atVector<u32>shots; 

					for (int i = 0; i < numOfModes; ++i)
					{
						shots.PushAndGrow(i);
					}
					
						for (int i = shots.GetCount() - 1; i > 0; --i) 
						{
							//if there are only two random numbers lets not always swap oterwise we alwasy get the same result
							bool swapNumbers = true; 

							if(shots.GetCount() == 2)
							{
								if(fwRandom::GetRandomNumberInRange(0 , 10) > 5)
								{
									swapNumbers = false; 
								}
							}

							if(swapNumbers)
							{
								// generate random index
								int w = rand()%i;
								// swap items
								int t = shots[i];
								shots[i] = shots[w];
								shots[w] = t;
							}
						
						}

					for(int i = 0; i < shots.GetCount(); i++)
					{
						if(m_UpdatedCamera == NULL)
						{
							m_UpdatedCamera = camFactory::CreateObject<camBaseCamera>(CameraRef);
						}
						m_CurrentMode = shots[i]; 

						InitCamera();
						isValid = m_UpdatedCamera->BaseUpdate(camInterface::GetCinematicDirector().GetFrameNonConst());

						if(!isValid)
						{
							//The test failed, so clean-up this camera.
							DeleteCamera(); 
						}
						else
						{
							m_ShotStartTime = fwTimer::GetTimeInMilliseconds(); 
							break; 
						}
					}
				}
				else
				{
					InitCamera();
					if( m_pContext && m_pContext->GetIsClassId(camCinematicInVehicleFirstPersonContext::GetStaticClassId()) &&
						m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) )
					{
						camCinematicInVehicleFirstPersonContext* pVehicleFirstPersonContext = static_cast<camCinematicInVehicleFirstPersonContext*>(m_pContext);
						camCinematicMountedCamera* pMountedCamera = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get());
						if(pMountedCamera->IsFirstPersonCamera())
						{
							pVehicleFirstPersonContext->PreCameraUpdate(pMountedCamera);
						}
					}
					isValid = m_UpdatedCamera->BaseUpdate(camInterface::GetCinematicDirector().GetFrameNonConst());

					if( isValid && m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) )
					{
						camCinematicMountedCamera* pMountedCamera = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get());
						if(pMountedCamera->IsFirstPersonCamera() && pMountedCamera->IsVehicleTurretCamera())
						{
							// The following updates the turret and then updates the camera a second time so camera position is correct.
							camInterface::GetCinematicDirector().DoSecondUpdateForTurretCamera(m_UpdatedCamera, false);
						}
					}

					if(!isValid)
					{
						//The test failed, so clean-up this camera.
						DeleteCamera(); 
					}
					else
					{
						m_ShotStartTime = fwTimer::GetTimeInMilliseconds(); 
					}
				}
			}
		}
		else
		{
			isValid = m_UpdatedCamera->IsValid();

			if(!isValid)
			{
				//The test failed, so clean-up this camera.
				DeleteCamera(); 
			}
		}
	}
	return isValid;
}

bool camBaseCinematicShot::IsValidForGameState() const
{
	//If we're in a submarine, we override the water check
	const CVehicle* followVehicle	= camInterface::GetGameplayDirector().GetFollowVehicle();
	const bool isFollowVehicleASub	= followVehicle && (followVehicle->InheritsFromSubmarine() || followVehicle->InheritsFromSubmarineCar());

	if(!m_Metadata.m_IsVaildUnderWater && !isFollowVehicleASub)
	{
		if(IsFollowPedTargetUnderWater() )
		{
			return false; 
		}
	}

	return true; 
}

bool camBaseCinematicShot::IsFollowPedTargetUnderWater() const 
{
	if(m_Metadata.m_IsPlayerAttachEntity || m_Metadata.m_IsPlayerLookAtEntity)
	{
		camThirdPersonCamera* pFollowCam = camInterface::GetGameplayDirector().GetThirdPersonCamera(); 

		if(pFollowCam)
		{
			if(pFollowCam->IsBuoyant())
			{
				return false;
			}
			else
			{
				return true; 
			}
		}
		else
		{
			return false; 
		}
	}

	return false; 
}	


const CEntity* camBaseCinematicShot::ComputeAttachEntity() const
{
	const CEntity* pEnt = NULL; 
	if(m_Metadata.m_IsPlayerAttachEntity)
	{
		camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState	= gameplayDirector.GetVehicleEntryExitState();
		
		
		const CPed* followPed = camInterface::FindFollowPed();
		
		
		if(!cameraVerifyf(followPed, "The cinematic camera director does not have a valid follow ped"))
		{
			pEnt = NULL;
			return pEnt;
		}

		const CVehicle* targetVehicle = NULL;

		const camFollowVehicleCamera* followVehicleCamera = gameplayDirector.GetFollowVehicleCamera();
		if(followVehicleCamera)
		{
			const CEntity* followVehicle = followVehicleCamera->GetAttachParent();
			if(followVehicle && followVehicle->GetIsTypeVehicle())
			{
				 targetVehicle = static_cast<const CVehicle*>(followVehicle);
			}
		}
		//B*2082308: Get target vehicle from third person aim camera if using a turret (we use the third person aim camera for this, not the vehicle follow camera)
		else if (!followVehicleCamera && camInterface::GetGameplayDirector().IsUsingVehicleTurret(true))
		{
			targetVehicle = followPed->GetVehiclePedInside();
		}
#if FPS_MODE_SUPPORTED
		else if(gameplayDirector.GetFirstPersonShooterCamera())
		{
			targetVehicle = followPed->GetMyVehicle();
		}
		else
		{
			const bool shouldUseFirstPersonDriveBy = gameplayDirector.ComputeShouldUseFirstPersonDriveBy();
			if(shouldUseFirstPersonDriveBy)
			{
				const camThirdPersonAimCamera* thirdPersonAimCamera = gameplayDirector.GetThirdPersonAimCamera();
				if(thirdPersonAimCamera)
				{
					const CEntity* attachParent = thirdPersonAimCamera->GetAttachParent();
					if(attachParent && attachParent->GetIsTypeVehicle())
					{
						targetVehicle = static_cast<const CVehicle*>(attachParent);
					}
				}
			}
		}
#endif // FPS_MODE_SUPPORTED

		if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE)
		{
			pEnt = targetVehicle;
		}
		else
		{
			pEnt = followPed; 
		}
	}
	return pEnt; 
}

const CEntity* camBaseCinematicShot::ComputeLookAtEntity() const
{
	const CEntity* pEnt = NULL; 
	if(m_Metadata.m_IsPlayerLookAtEntity)
	{
		camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState	= gameplayDirector.GetVehicleEntryExitState();


		const CPed* followPed = camInterface::FindFollowPed();


		if(!cameraVerifyf(followPed, "The cinematic camera director does not have a valid follow ped"))
		{
			pEnt = NULL;
			return pEnt;
		}

		const CVehicle* targetVehicle = NULL;

		const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();
		if(followVehicleCamera)
		{
			const CEntity* followVehicle = followVehicleCamera->GetAttachParent();
			if(followVehicle && followVehicle->GetIsTypeVehicle())
			{
				targetVehicle = static_cast<const CVehicle*>(followVehicle);
			}
		}
		else if (!followVehicleCamera && camInterface::GetGameplayDirector().IsUsingVehicleTurret(true))
		{
			targetVehicle = followPed->GetMyVehicle();
		}
#if FPS_MODE_SUPPORTED
		else if(camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())
		{
			targetVehicle = followPed->GetMyVehicle();
		}
#endif // FPS_MODE_SUPPORTED

		if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE)
		{
			pEnt = targetVehicle;
		}
		else
		{
			pEnt = followPed; 
		}
	}
	return pEnt; 
}

bool camBaseCinematicShot::CanCreate()
{
	if(m_AttachEntity || m_LookAtEntity)
	{
		return true;
	}

	return false; 
}

void camBaseCinematicShot::SetAttachEntity(const CEntity* pEntity)
{
	m_AttachEntity = pEntity; 
}

void camBaseCinematicShot::SetLookAtEntity(const CEntity* pEntity)
{
	m_LookAtEntity = pEntity; 
}

void camBaseCinematicShot::SetAttachEntity()
{
	m_AttachEntity = ComputeAttachEntity(); 
}

void camBaseCinematicShot::SetLookAtEntity()
{
	m_LookAtEntity = ComputeLookAtEntity(); 
}


bool camBaseCinematicShot::IsValid() const
{	
	if(!IsValidForGameState())
	{
		return false; 
	}

	if(!m_AttachEntity && !m_LookAtEntity)
	{
		return false; 
	}
	
	//return here if the parent context is the scrip context, dont care about the shot metadata
	if(GetContext()->GetClassId() == camCinematicScriptContext::GetStaticClassId())
	{
		return true;
	}
	
	if(m_Metadata.m_IsPlayerAttachEntity)
	{
		const CEntity* pEnt = ComputeAttachEntity(); 

		if( pEnt != m_AttachEntity)
		{
			return false; 
		}
	}
	
	if(m_Metadata.m_IsPlayerLookAtEntity)
	{
		const CEntity* pEnt = ComputeLookAtEntity(); 
	
		if( pEnt != m_LookAtEntity)
		{
			return false; 
		}
	}

	bool IsAttachEntityValid = false; 
	bool IsLookAtEntityValid = false; 
	 
	if(m_AttachEntity)
	{
		if(m_AttachEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get()); 
			if(IsValidForVehicleType(pVehicle) && IsValidForCustomVehicleSettings(pVehicle))
			{
				IsAttachEntityValid = true; 
			}
		}
		else
		{
			IsAttachEntityValid = true; 
		}
	}

	if(m_LookAtEntity)
	{
		if(m_LookAtEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get()); 
			if(IsValidForVehicleType(pVehicle) && IsValidForCustomVehicleSettings(pVehicle))
			{
				IsLookAtEntityValid = true; 
			}
		}
		else
		{
			IsLookAtEntityValid = true; 
		}

	}

	return IsAttachEntityValid || IsLookAtEntityValid; 
}

bool camBaseCinematicShot::IsValidForVehicleType(const CVehicle* pVehicle)const  
{
	if(pVehicle)
	{
		if(m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::ALL))
		{
			return true; 
		}

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || (!pVehicle->GetIsInWater() && (pVehicle->InheritsFromAmphibiousAutomobile() || pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)))
		{
			if(m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::CAR))
			{
				return true; 
			}
		}

		if(pVehicle->InheritsFromAutogyro())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::AUTOGYRO); 
		}

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::BIKE); 
		}

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::BICYCLE); 
		}

		if(pVehicle->InheritsFromBoat() || (pVehicle->GetIsInWater() && pVehicle->InheritsFromAmphibiousAutomobile()))
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::BOAT); 
		}

		if(pVehicle->InheritsFromTrain())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::TRAIN); 
		}

		if(pVehicle->InheritsFromHeli())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::HELI); 
		}

		if(pVehicle->InheritsFromPlane())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::PLANE); 
		}

		if(pVehicle->InheritsFromSubmarine())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::SUBMARINE); 
		}

		if(pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::QUADBIKE); 
		}

		if(pVehicle->InheritsFromTrailer())
		{
			return m_Metadata.m_VehicleTypes.IsSet(camCinematicInVehicleContextFlags::TRAILER); 
		}
	}
	return false; 
}

bool camBaseCinematicShot::IsValidForCustomVehicleSettings(const CVehicle* pVehicle) const
{
	if(pVehicle)
	{
		const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();
			
		if(targetVehicleModelInfo)
		{
			u32 vehicleModelHash = targetVehicleModelInfo->GetModelNameHash(); 

			const camVehicleCustomSettingsMetadata* pCustomVehicleSettings = camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash); 

			if(pCustomVehicleSettings && pCustomVehicleSettings->m_InvalidCinematicShotRefSettings.m_ShouldConsiderData)
			{
				for(int i = 0; i < pCustomVehicleSettings->m_InvalidCinematicShotRefSettings.m_InvalidCinematcShot.GetCount(); i++)
				{
					if(GetNameHash() == pCustomVehicleSettings->m_InvalidCinematicShotRefSettings.m_InvalidCinematcShot[i].GetHash())
					{
						return false;
					}
				}
			}
		}
	}
	return true; 
}


void camBaseCinematicShot::DeleteCamera(bool ResetCameraEndTime)
{
	if(m_UpdatedCamera)
	{
		if(ResetCameraEndTime)
		{
			m_ShotEndTime = 0; 
		}
		else
		{
			m_ShotEndTime = fwTimer::GetTimeInMilliseconds(); 
		}
		delete m_UpdatedCamera; 
		m_UpdatedCamera = NULL;
	}
}

void camBaseCinematicShot::ClearShot(bool ResetCameraEndTime)
{	
	DeleteCamera(ResetCameraEndTime); 
	m_AttachEntity = NULL; 
	m_LookAtEntity = NULL; 
}
