//
// camera/cinematic/shot/camInVehicleShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicInVehicleShot.h"

#include "fwmaths/random.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedPartCamera.h"
#include "camera/cinematic/camera/orbit/CinematicVehicleOrbitCamera.h"
#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicVehicleTrackingCamera.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "frontend/MobilePhone.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "system/control.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicles/metadata/VehicleLayoutInfo.h"
#include "vehicles/metadata/VehicleSeatInfo.h"
#include "vehicles/vehicle.h"

INSTANTIATE_RTTI_CLASS(camHeliTrackingShot,0x439598B1)
INSTANTIATE_RTTI_CLASS(camVehicleOrbitShot,0x38FA712C)
INSTANTIATE_RTTI_CLASS(camVehicleLowOrbitShot,0x129698CA)
INSTANTIATE_RTTI_CLASS(camCameraManShot,0x4437E7B6)
INSTANTIATE_RTTI_CLASS(camCraningCameraManShot,0x77ae33e5)
INSTANTIATE_RTTI_CLASS(camCinematicVehiclePartShot,0x2B5A5DB1)
INSTANTIATE_RTTI_CLASS(camCinematicVehicleBonnetShot,0xA231072E)
INSTANTIATE_RTTI_CLASS(camCinematicVehicleConvertibleRoofShot,0xA1FEDCBB)
INSTANTIATE_RTTI_CLASS(camCinematicInVehicleCrashShot, 0xbd6c94ed)

CAMERA_OPTIMISATIONS()

camHeliTrackingShot::camHeliTrackingShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicHeliTrackingShotMetadata&>(metadata))
, m_HeliTrackingAccuracyMode(0)
, m_DesiredTrackingAccuracyMode(0)
{
}

void camHeliTrackingShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicHeliChaseCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity.Get());
			
			camCinematicHeliChaseCamera* pCam = static_cast<camCinematicHeliChaseCamera*>(m_UpdatedCamera.Get()); 

			if(m_LookAtEntity->GetIsTypeVehicle())
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());

				if(pVehicle->GetIsAircraft())
				{
					const u32 shotTypeOffset = fwRandom::GetRandomNumberInRange( 1, camCinematicHeliChaseCamera::NUM_OF_ACCURACY_MODES);
					m_DesiredTrackingAccuracyMode = ( m_HeliTrackingAccuracyMode + shotTypeOffset ) % camCinematicHeliChaseCamera::NUM_OF_ACCURACY_MODES;
					
					pCam->SetTrackingAccuracyMode(m_DesiredTrackingAccuracyMode); 
				}
			}

			
			
			pCam->SetMode(m_CurrentMode); 
			pCam->SetCanUseLookAtDampingHelpers(true);
			pCam->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}

void camHeliTrackingShot::Update()
{
	if(m_UpdatedCamera && m_UpdatedCamera->GetNumUpdatesPerformed() > 0)
	{
		m_HeliTrackingAccuracyMode = m_DesiredTrackingAccuracyMode; 
	}
}

bool camHeliTrackingShot::CanCreate()
{
	if(m_LookAtEntity)
	{
		bool isTargetInside	= m_LookAtEntity->m_nFlags.bInMloRoom;
		
		if(isTargetInside)
		{
			return false; 
		}
		return true;
	}

	return false; 
}

camVehicleOrbitShot::camVehicleOrbitShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicVehicleOrbitShotMetadata&>(metadata))
{
}

void camVehicleOrbitShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicVehicleOrbitCamera::GetStaticClassId()))
		{
			camCinematicVehicleOrbitCamera* pCam = static_cast<camCinematicVehicleOrbitCamera*>(m_UpdatedCamera.Get()); 
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			m_UpdatedCamera->LookAt(m_LookAtEntity); 
			pCam->Init(); 
			pCam->SetShotId(m_CurrentMode); 
		}
	}
}

bool camVehicleOrbitShot::CanCreate()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();

		if(!gameplayDirector.GetFollowPed()->GetIsDrivingVehicle())
		{
			return true;
		}
	}
	return false;
}

camVehicleLowOrbitShot::camVehicleLowOrbitShot(const camBaseObjectMetadata& metadata)
	: camBaseCinematicShot(metadata)
	, m_Metadata(static_cast<const camCinematicVehicleLowOrbitShotMetadata&>(metadata))
{
}

bool camVehicleLowOrbitShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValid())
	{
		return false;
	}

	if(camCinematicDirector::IsSpectatingCopsAndCrooksEscape())
	{
		return false;
	}

	return true;
}

void camVehicleLowOrbitShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicVehicleLowOrbitCamera::GetStaticClassId()))
		{
			camCinematicVehicleLowOrbitCamera* pCam = static_cast<camCinematicVehicleLowOrbitCamera*>(m_UpdatedCamera.Get()); 
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			m_UpdatedCamera->LookAt(m_LookAtEntity); 
			pCam->Init(); 
			pCam->SetShotId(m_CurrentMode); 
		}
	}
}

bool camVehicleLowOrbitShot::CanCreate()
{
	if(m_AttachEntity && m_LookAtEntity /*&& camInterface::GetCinematicDirector().HaveUpdatedACinematicCamera()*/)
	{
		return true;
	}
	return false;
}


camCameraManShot::camCameraManShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicCameraManShotMetadata&>(metadata))
{
}

bool camCameraManShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValid())
	{
		return false;
	}

	if(camCinematicDirector::IsSpectatingCopsAndCrooksEscape())
	{
		return false;
	}

	return true;
}

void camCameraManShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicCamManCamera::GetStaticClassId()))
		{
			camCinematicCamManCamera* camManCamera = static_cast<camCinematicCamManCamera*>(m_UpdatedCamera.Get()); 
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			camManCamera->SetMode(m_CurrentMode);
			camManCamera->SetCanUseLookAtDampingHelpers(true);
			camManCamera->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}

bool camCameraManShot::CanCreate()
{
	if(m_LookAtEntity)	
	{
		return true; 
	}
	return false; 
}


camCraningCameraManShot::camCraningCameraManShot(const camBaseObjectMetadata& metadata)
	: camCameraManShot(metadata)
	, m_Metadata(static_cast<const camCinematicCraningCameraManShotMetadata&>(metadata))
	, m_TimeSinceVehicleMovedForward(0)
{
}

void camCraningCameraManShot::InitCamera()
{
	camCameraManShot::InitCamera();

	if(m_LookAtEntity && m_UpdatedCamera->GetIsClassId(camCinematicCamManCamera::GetStaticClassId()))
	{
		camCinematicCamManCamera* camManCamera = static_cast<camCinematicCamManCamera*>(m_UpdatedCamera.Get());
		camManCamera->SetCanUseLookAtDampingHelpers(m_Metadata.m_ShouldUseLookAtDampingHelpers);
		camManCamera->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		camManCamera->SetMaxShotDuration(static_cast<const u32>(m_Metadata.m_ShotDurationLimits.y));

		const float chanceToUseVehicleRelativeStartPosition = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		if (chanceToUseVehicleRelativeStartPosition <= m_Metadata.m_ProbabilityToUseVehicleRelativeStartPosition)
		{
			camManCamera->SetVehicleRelativeBoomShot(m_Metadata.m_MaxTotalHeightChange, m_Metadata.m_VehicleRelativePositionMultiplier,
														m_Metadata.m_VehicleRelativePositionHeight, m_Metadata.m_ScanRadius);
		}
		else
		{
			camManCamera->SetBoomShot(m_Metadata.m_MaxTotalHeightChange);
		}
	}
}

bool camCraningCameraManShot::IsValid() const
{
	if (!camCameraManShot::IsValid())
	{
		return false;
	}

	//invalid if not moved forward within time limit
	if (m_LookAtEntity && m_LookAtEntity->GetIsTypeVehicle())
	{
		if (m_TimeSinceVehicleMovedForward >= m_Metadata.m_MaxTimeSinceVehicleMovedForward)
		{
			return false;
		}
	}

	if(camCinematicDirector::IsSpectatingCopsAndCrooksEscape())
	{
		return false;
	}

	return true;
}

void camCraningCameraManShot::PreUpdate(bool isCurrentShot)
{
	if(isCurrentShot)
	{
		//increment time vehicle has not been moving forward for
		if (m_LookAtEntity && m_LookAtEntity->GetIsTypeVehicle() && m_Metadata.m_ShouldOnlyTriggerIfVehicleMovingForward)
		{
			const CVehicle* lookAtVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());
			const float forwardSpeed = DotProduct(lookAtVehicle->GetVelocity(), VEC3V_TO_VECTOR3(lookAtVehicle->GetTransform().GetB()));
			if (forwardSpeed < SMALL_FLOAT)
			{
				const u32 time = fwTimer::GetCamTimeStepInMilliseconds();
				m_TimeSinceVehicleMovedForward += time;
			}
			else
			{
				m_TimeSinceVehicleMovedForward = 0;
			}
		}
	}
	else
	{
		m_TimeSinceVehicleMovedForward = 0;
	}
}

bool camCraningCameraManShot::CanCreate()
{
	if (!camCameraManShot::CanCreate())
	{
		return false;
	}

	//only start this shot if the vehicle is moving
	if (m_LookAtEntity->GetIsTypeVehicle() && m_Metadata.m_ShouldOnlyTriggerIfVehicleMovingForward)
	{
		const CVehicle* lookAtVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());
		const float forwardSpeed = DotProduct(lookAtVehicle->GetVelocity(), VEC3V_TO_VECTOR3(lookAtVehicle->GetTransform().GetB()));
		if (forwardSpeed < SMALL_FLOAT)
		{
			return false;
		}
	}

	return true;
}


camCinematicVehiclePartShot::camCinematicVehiclePartShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicVehiclePartShotMetadata&>(metadata))
, m_HistoryInsertIndex(0)
, m_IsValid(true)
, m_InAirTimer(0)
{
	m_CameraHash = m_Metadata.m_CameraRef.GetHash();

	for (int i = 0; i < MAX_CINEMATIC_MOUNTED_PART_HISTORY; i++)
	{
		m_HistoryList[i] = 0;
	}
}

void camCinematicVehiclePartShot::InitCamera()
{
	if(m_UpdatedCamera->GetIsClassId(camCinematicMountedPartCamera::GetStaticClassId()))
	{
		if(m_AttachEntity)
		{
			m_UpdatedCamera->AttachTo(m_AttachEntity);
		}

		if(m_LookAtEntity)
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			if (m_UpdatedCamera->GetAttachParent() == NULL)
			{
				m_UpdatedCamera->AttachTo(m_LookAtEntity);
			}
		}
	}
}

bool camCinematicVehiclePartShot::IsValid() const
{
	if (camBaseCinematicShot::IsValid() && m_IsValid)
	{
		const CVehicle* pAttachVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get());
		if (!m_AttachEntity || !m_AttachEntity->GetIsTypeVehicle())
		{
			if (!m_LookAtEntity || !m_LookAtEntity->GetIsTypeVehicle())
			{
				return false;
			}
			else
			{
				pAttachVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());
			}
		}

		if(camCinematicDirector::IsSpectatingCopsAndCrooksEscape())
		{
			return false;
		}

		return true;
	}

	return false;
}

bool camCinematicVehiclePartShot::IsNonAircraftVehicleInTheAir(const CVehicle* pVehicle)
{
	if(pVehicle && pVehicle->GetIsTypeVehicle())
	{
		if(!pVehicle->GetIsAircraft())
		{
			if(const_cast<CVehicle*>(pVehicle)->IsInAir())
			{
				return true; 
			}
		}
	}
	return false;
}

void camCinematicVehiclePartShot::PreUpdate(bool isCurrentShot)
{
	if(isCurrentShot && !camInterface::GetCinematicDirector().ShouldUsePreferedShot())	
	{
		if(m_LookAtEntity && m_LookAtEntity->GetIsTypeVehicle())
		{
			const CVehicle* pAttachVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());
			if(IsNonAircraftVehicleInTheAir(pAttachVehicle))
			{
				m_InAirTimer += fwTimer::GetTimeStepInMilliseconds(); 
				
				if( m_InAirTimer > m_Metadata.m_MaxInAirTimer)
				{
					m_IsValid = false;
					return; 
				}
			}
			else
			{
				m_InAirTimer = 0; 
			}

		}
		else
		{
			m_InAirTimer = 0; 
		}
	}
	else
	{
		m_InAirTimer = 0; 
	}
	
	if(isCurrentShot)
	{
		if(m_LookAtEntity && m_LookAtEntity->GetIsTypeVehicle())
		{
			const CVehicle* pAttachVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());
			
			if(pAttachVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || (!pAttachVehicle->GetIsInWater() && (pAttachVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || pAttachVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)))
			{
				//check that the vehicle is reversing and terminate the shot after min duration
				if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedPartCamera::GetStaticClassId()))
				{
					camCinematicMountedPartCamera* pCam = static_cast<camCinematicMountedPartCamera*>(m_UpdatedCamera.Get());
					
					CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
					if(control)
					{
						float fSpeed = DotProduct(pAttachVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pAttachVehicle->GetTransform().GetB()));

						bool ShouldTerminateDueToFacingTheWrongDirection = false;

						//traveling backwards but the current camera is looking forwards
					
						if(fSpeed < 0.0f && !pCam->IsValidForReversing() && control->GetVehicleBrake().IsDown() )
						{
							ShouldTerminateDueToFacingTheWrongDirection = true;
						}

						//traveling forwards but the current camera is looking backwards
						if(fSpeed > 0.0f && pCam->IsValidForReversing() && control->GetVehicleAccelerate().IsDown())
						{
							ShouldTerminateDueToFacingTheWrongDirection = true; 
						}

						if(ShouldTerminateDueToFacingTheWrongDirection)
						{
							if(fwTimer::GetTimeInMilliseconds() - m_ShotStartTime > m_Metadata.m_ShotDurationLimits.x * m_Metadata.m_MinShotTimeScalarForAbortingShots)
							{
								m_IsValid = false;
								return; 
							}
						}
					}
				}
			}
		}
	}

	m_IsValid = true; 
}

const u32 c_uRearWindowCamera = ATSTRINGHASH("CAR_REAR_WINDOW_CAMERA", 0x0751ea71f);
bool camCinematicVehiclePartShot::CreateAndTestCamera(u32 UNUSED_PARAM(hash))
{
	// Instead of using the camera hash (which is a dummy value anyway),
	// go through vehicle's list of valid cameras and select one.
	const CVehicle* pAttachVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get());
	if (!m_AttachEntity || !m_AttachEntity->GetIsTypeVehicle())
	{
		if (!m_LookAtEntity || !m_LookAtEntity->GetIsTypeVehicle())
		{
			return false;
		}
		else
		{
			pAttachVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get());
		}
	}
	
	if(pAttachVehicle)
	{
		if(!camInterface::GetCinematicDirector().ShouldUsePreferedShot())
		{
			if(IsNonAircraftVehicleInTheAir(pAttachVehicle))
			{
				return false; 
			}
		}
	}

	if (pAttachVehicle)
	{
		u32 newCamera = 0;

		int cameraCount = (pAttachVehicle->GetVehicleModelInfo()) ? pAttachVehicle->GetVehicleModelInfo()->GetNumCinematicPartCameras() : 0;
		 
		int cameraIndex = fwRandom::GetRandomNumberInRange((int)0, cameraCount-1);
		u32 maxCameraHistory= cameraCount > 2 ? cameraCount-2 :((cameraCount > 1) ? cameraCount-1 : 0);
		
		bool bRejectedAny = false;
		bool bAllowCamera = true;
		
		for (int counter = 0; counter < cameraCount; counter++)
		{
			newCamera = pAttachVehicle->GetVehicleModelInfo()->GetCinematicPartCameraHash(cameraIndex);
			bAllowCamera  = !IsInHistoryList(newCamera, maxCameraHistory)|| newCamera == c_uRearWindowCamera;
			bRejectedAny |= !bAllowCamera;
			if (bAllowCamera)
			{
				if (camBaseCinematicShot::CreateAndTestCamera(newCamera))
				{
					m_CameraHash = newCamera;
					AddToHistory(newCamera, maxCameraHistory);
					return true;
				}
			}

			cameraIndex++;
			if (cameraIndex >= cameraCount)
			{
				cameraIndex = 0;
			}
		}

		// No valid camera found, go through history list (starting with oldest)
		// and try one of those cameras.
		if (bRejectedAny)
		{
 			if (maxCameraHistory > MAX_CINEMATIC_MOUNTED_PART_HISTORY)
			{
				maxCameraHistory = MAX_CINEMATIC_MOUNTED_PART_HISTORY;
			}

			cameraIndex = m_HistoryInsertIndex;
			for (int counter = 0; counter < maxCameraHistory; counter++)
			{
				newCamera = m_HistoryList[cameraIndex];
				if (newCamera != 0 && camBaseCinematicShot::CreateAndTestCamera(newCamera))
				{
					m_CameraHash = newCamera;
					AddToHistory(newCamera, maxCameraHistory);
					return true;
				}

				cameraIndex++;
				if (cameraIndex >= maxCameraHistory)
				{
					cameraIndex = 0;
				}
			}
		}
	}

	return false;
}

void camCinematicVehiclePartShot::AddToHistory(u32 camera, u32 maxHistory)
{
	if (maxHistory > MAX_CINEMATIC_MOUNTED_PART_HISTORY)
	{
		maxHistory = MAX_CINEMATIC_MOUNTED_PART_HISTORY;
	}

	bool bAdd = (maxHistory > 0);
	for (int i = 0; i < maxHistory; i++)
	{
		if (m_HistoryList[i] == camera)
		{
			bAdd = false;
			break;
		}
	}

	if (bAdd)
	{
		m_HistoryList[m_HistoryInsertIndex] = camera;
		m_HistoryInsertIndex++;
		if (m_HistoryInsertIndex >= maxHistory)
		{
			m_HistoryInsertIndex = 0;
		}
	}
}

bool camCinematicVehiclePartShot::IsInHistoryList(u32 camera, u32 maxHistory)
{
	if (maxHistory > MAX_CINEMATIC_MOUNTED_PART_HISTORY)
	{
		maxHistory = MAX_CINEMATIC_MOUNTED_PART_HISTORY;
	}

	for (int i = 0; i < maxHistory; i++)
	{
		if (m_HistoryList[i] == camera)
		{
			return true;
		}
	}

	return false;
}

camCinematicVehicleBonnetShot::camCinematicVehicleBonnetShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicVehicleBonnetShotMetadata&>(metadata))
, m_SeatIndex(-1)
{
}

bool camCinematicVehicleBonnetShot::ComputeShouldAdjustForRollCage() const
{
	if(!m_AttachEntity)
	{
		return false;
	}

	if (!m_AttachEntity->GetIsTypeVehicle())
	{
		return false;
	}

	const CVehicle* attachVehicle						= static_cast<const CVehicle*>(m_AttachEntity.Get()); 
	const CVehicleVariationInstance& vehicleVariation	= attachVehicle->GetVariationInstance();
	const u8 rollCageModIndex							= vehicleVariation.GetModIndex(VMT_CHASSIS);

	if (rollCageModIndex != INVALID_MOD)
	{
		if (const CVehicleKit* vehicleKit = vehicleVariation.GetKit())
		{
			const CVehicleModVisible& vehicleModVisible = vehicleKit->GetVisibleMods()[rollCageModIndex];
			if (vehicleModVisible.GetType() == VMT_CHASSIS)
			{
				return true;
			}
		}
	}

	return false;
}

void camCinematicVehicleBonnetShot::GetSeatSpecificCameraOffset(Vector3& cameraOffset) const
{
	cameraOffset = VEC3_ZERO;

	if(!m_AttachEntity)
	{
		return;
	}

	if (!m_AttachEntity->GetIsTypeVehicle())
	{
		return;
	}

	const CVehicle* attachVehicle						= static_cast<const CVehicle*>(m_AttachEntity.Get());
	const CVehicleModelInfo* vehicleModelInfo			= attachVehicle->GetVehicleModelInfo();
	const u32 vehicleModelHash							= vehicleModelInfo ? vehicleModelInfo->GetModelNameHash() : 0;
	const camVehicleCustomSettingsMetadata* settings	= vehicleModelHash > 0 ? camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash) : NULL;

	int seatIndex = camInterface::FindFollowPed()->GetAttachCarSeatIndex(); 
	if (settings && seatIndex > -1)
	{
		if (settings->m_SeatSpecificCamerasSettings.m_ShouldConsiderData)
		{
			for (int i = 0; i < settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera.GetCount(); ++i)
			{
				if ((u32)seatIndex == settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_SeatIndex )
				{
					cameraOffset = settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_PovCameraOffset;
					return;
				}
			}
		}
	}

	const CPed* followPed			= camInterface::FindFollowPed();
	const bool isFollowPedDriver	= followPed && followPed->GetIsDrivingVehicle();
	if (!isFollowPedDriver)
	{
		const CVehicleSeatInfo* seatInfo = attachVehicle->GetSeatInfo(seatIndex);
		if (seatInfo && !seatInfo->GetIsFrontSeat())
		{
			cameraOffset = vehicleModelInfo->GetPovRearPassengerCameraOffset();
		}
		else
		{
			cameraOffset = vehicleModelInfo->GetPovPassengerCameraOffset();
		}
	}
}

void camCinematicVehicleBonnetShot::InitCamera()
{
	if(m_AttachEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			m_SeatIndex = camInterface::FindFollowPed()->GetAttachCarSeatIndex(); 

			camCinematicMountedCamera* cinematicMountedCamera = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get());

			//check the roll cage adjustment and seat specific settings here, for ordering issues causing this to update a frame late on camera start
			const bool shouldAdjustForRollCage = ComputeShouldAdjustForRollCage();
			cinematicMountedCamera->SetShouldAdjustRelativeAttachPositionForRollCage(shouldAdjustForRollCage);

			Vector3 cameraOffset;
			GetSeatSpecificCameraOffset(cameraOffset);
			cinematicMountedCamera->SetSeatSpecificCameraOffset(cameraOffset);

#if FPS_MODE_SUPPORTED
			// In first person mode, force first person gameplay camera to be active in a vehicle, since cinematic pov camera will override it.
			// This allows us to keep updating the first person gameplay camera to we can blend the transition to the pov camera.
			//NOTE: We only interpolate from the FPS camera if it's already rendering, otherwise we can observe undesirable interpolation behaviour when cutting from other cameras,
			//such as after cutscenes.
			//NOTE: We also only interpolate during normal vehicle entry conditions, as we don't want to observe this when teleporting into a vehicle.
			camFirstPersonShooterCamera* fpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
			if(fpsCamera && /*!fpsCamera->IsEnteringTurretSeat() &&*/ camInterface::IsRenderingCamera(*fpsCamera) && cinematicMountedCamera->IsFirstPersonCamera())
			{
				const s32 vehicleEntryExitStateOnPreviousUpdate = camInterface::GetGameplayDirector().GetVehicleEntryExitStateOnPreviousUpdate();
				if((vehicleEntryExitStateOnPreviousUpdate == camGameplayDirector::ENTERING_VEHICLE) || (vehicleEntryExitStateOnPreviousUpdate == camGameplayDirector::INSIDE_VEHICLE))
				{
					m_UpdatedCamera->InterpolateFromCamera(*fpsCamera, m_Metadata.m_BlendTimeFromFirstPersonShooterCamera);
				}
				else if(fpsCamera->IsUsingAnimatedData())
				{
					cinematicMountedCamera->GetFrameNonConst().CloneFrom( fpsCamera->GetFrame() );
					cinematicMountedCamera->SetWasUsingAnimatedData();
				}
			}
			else if( camInterface::GetGameplayDirector().IsUsingVehicleTurret(false) &&
					 cinematicMountedCamera->IsFirstPersonCamera() &&
					 camInterface::GetCinematicDirector().HaveUpdatedACinematicCamera() &&
					(camInterface::GetCinematicDirector().IsRenderedCameraInsideVehicle() ||
					 camInterface::GetCinematicDirector().IsRenderedCameraVehicleTurret()) )
			{
				// Doing seat shuffle from pov camera to turret pov camera, interpolate when switching to turret camera.
				m_UpdatedCamera->InterpolateFromFrame(camInterface::GetCinematicDirector().GetFrame(), m_Metadata.m_BlendTimeFromFirstPersonShooterCamera);
			}
#endif // FPS_MODE_SUPPORTED
		}
	}
}

bool camCinematicVehicleBonnetShot::IsValid() const
{
	if (!camBaseCinematicShot::IsValid())
	{
		return false;
	}

	if(camCinematicDirector::IsSpectatingCopsAndCrooksEscape())
	{
		return false;
	}

	const camGameplayDirector& gameplayDirector				= camInterface::GetGameplayDirector();
	const CPed* followPed									= gameplayDirector.GetFollowPed();
	const CPedIntelligence* followPedIntelligence			= followPed ? followPed->GetPedIntelligence() : NULL;
	const CQueriableInterface* followPedQueriableInterface	= followPedIntelligence ? followPedIntelligence->GetQueriableInterface() : NULL;	
	if(followPedQueriableInterface)
	{
		if(followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT))
		{
			return false;
		}
	}

	//Invalidate when the POV camera is delayed, due to things like starting the engine and hot-wiring.
	const bool shouldDelayVehiclePovCamera = gameplayDirector.ShouldDelayVehiclePovCamera()
		FPS_MODE_SUPPORTED_ONLY( && (!m_UpdatedCamera || !m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId())) );
	if(shouldDelayVehiclePovCamera)
	{
		return false;
	}

	bool bVehicleUsingSameHashForBonnetAndPovCamera = false;
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle())
	{
		const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get());
		const CVehicleModelInfo* attachVehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
		if(attachVehicleModelInfo)
		{
			bVehicleUsingSameHashForBonnetAndPovCamera = (attachVehicleModelInfo->GetBonnetCameraNameHash() == attachVehicleModelInfo->GetPovCameraNameHash());
		#if __ASSERT
			if(!bVehicleUsingSameHashForBonnetAndPovCamera)
			{
				// Verify that the camera metadata is not the same type for bonnet and pov cameras.  If they are the same type,
				// then avoid re-creating the camera.  This means that changing the bonnet hood camera option won't force the proper
				// camera to be used.  Instead, the correct camera will be created the next time player gets into/onto vehicle.
				const camCinematicMountedCameraMetadata* metadataBonnet = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(attachVehicleModelInfo->GetBonnetCameraNameHash());
				const camCinematicMountedCameraMetadata* metadataPov    = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(attachVehicleModelInfo->GetPovCameraNameHash());
				//bVehicleUsingSameHashForBonnetAndPovCamera = (metadataBonnet && metadataPov && 
				//											  metadataBonnet->m_FirstPersonCamera == metadataPov->m_FirstPersonCamera);
				if(metadataBonnet && metadataPov)
				{
					// On second thought, make this as assert, so we will always fix via metadata.
					cameraAssertf(metadataBonnet->m_FirstPersonCamera != metadataPov->m_FirstPersonCamera, "Bonnet camera and Pov camera hash are not the same but both are the same type, fix vehicle metadata");
				}
			}
		#endif // __ASSERT
		}

		//Invalidate if the vehicle is wrecked or submerged.
		const bool isAttachVehicleDead = ((attachVehicle->GetStatus() == STATUS_WRECKED) || attachVehicle->m_nVehicleFlags.bIsDrowning);
		if(isAttachVehicleDead)
		{
			return false;
		}

		//Invalidate when the follow ped does not have a valid seat index.
		if(followPed)
		{
			const CVehicleLayoutInfo* attachVehicleLayout = attachVehicle->GetLayoutInfo();
			if(attachVehicleLayout)
			{
				const int seatIndex = followPed->GetAttachCarSeatIndex();
				if(seatIndex == -1)
				{
					return false;
				}
			}
		}
	}

	const bool c_bUseBonnetCamera = camCinematicDirector::ms_UseBonnetCamera || camInterface::GetGameplayDirector().IsForcingFirstPersonBonnet();
	const camCinematicMountedCamera* mountedCamera	= (m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId())) ?
														static_cast<const camCinematicMountedCamera*>(m_UpdatedCamera.Get()) : NULL;
	if( mountedCamera && !bVehicleUsingSameHashForBonnetAndPovCamera )
	{
		// Skip test to validate camera for bonnet/pov mode if vehicle is using same camera for both.
		// For bonnet camera, first person mounted camera is allowed for pov turret cameras.
		if( ( c_bUseBonnetCamera &&  mountedCamera->IsFirstPersonCamera() && !mountedCamera->IsVehicleTurretCamera() ) ||
		    (!c_bUseBonnetCamera && !mountedCamera->IsFirstPersonCamera()) )
		{
			return false;
		}
	}

	const bool shouldTerminateForLookBehind			= ComputeShouldTerminateForLookBehind(mountedCamera);

	return !shouldTerminateForLookBehind; 
}


bool camCinematicVehicleBonnetShot::CanCreate()
{
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle() && (fwTimer::GetTimeInMilliseconds() - m_ShotEndTime) >  m_Metadata.m_MinTimeToCreateAfterCameraFailure)
	{	
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get()); 
		
		const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();
		if(targetVehicleModelInfo && targetVehicleModelInfo->GetBonnetCameraNameHash() != 0)
		{
			const bool shouldTerminateForLookBehind = ComputeShouldTerminateForLookBehind();
			return !shouldTerminateForLookBehind;
		}
	} 
	return false;
}


void camCinematicVehicleBonnetShot::PreUpdate(bool IsCurrentShot)
{
	if(IsCurrentShot)
	{
		if(camInterface::FindFollowPed())
		{
			m_SeatIndex = camInterface::FindFollowPed()->GetAttachCarSeatIndex(); 
		}

		//check for roll cage mod, and let the camera know
		if(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			camCinematicMountedCamera* cinematicMountedCamera = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get());
			const bool shouldAdjustForRollCage = ComputeShouldAdjustForRollCage();
			cinematicMountedCamera->SetShouldAdjustRelativeAttachPositionForRollCage(shouldAdjustForRollCage);

			Vector3 cameraOffset;
			GetSeatSpecificCameraOffset(cameraOffset);
			cinematicMountedCamera->SetSeatSpecificCameraOffset(cameraOffset);
		}
	}
	else
	{
		m_SeatIndex = -1; 
	}
}

bool camCinematicVehicleBonnetShot::ComputeShouldTerminateForLookBehind(const camCinematicMountedCamera* mountedCamera) const
{
	//NOTE: We terminate the shot to look behind in the gameplay camera in some cases. This allows user to see backwards when this is impractical in first-person.

	const bool isRenderingMobilePhoneCamera = CPhoneMgr::CamGetState();
	if(isRenderingMobilePhoneCamera)
	{
		return false;
	}

	const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();
	if(!followVehicleCamera)
	{
		return false;
	}

	const camControlHelper* controlHelper = followVehicleCamera->GetControlHelper();
	if(!controlHelper || !controlHelper->IsLookingBehind())
	{
		return false;
	}

	//Always terminate for look-behind in aircraft that's not a heli
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle() &&
		static_cast<const CVehicle*>(m_AttachEntity.Get())->GetIsAircraft() &&
		!static_cast<const CVehicle*>(m_AttachEntity.Get())->GetIsHeli())
	{
		return true;
	}

	bool shouldTerminateForLookBehind = false;
	if(mountedCamera)
	{
		shouldTerminateForLookBehind = mountedCamera->ShouldLookBehindInGameplayCamera();
	}
	else
	{
		//We don't have a active camera to query, so we'll need to fall back to the metadata.
		const u32 cameraHash = GetCameraRef();
		if(cameraHash)
		{
			const camCinematicMountedCameraMetadata* mountedCameraMetadata	= camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(cameraHash);
			shouldTerminateForLookBehind									= (mountedCameraMetadata && mountedCameraMetadata->m_ShouldLookBehindInGameplayCamera);
		}
	}

	return shouldTerminateForLookBehind;
}

bool camCinematicVehicleBonnetShot::HasSeatChanged() const
{
	if(camInterface::FindFollowPed())
	{
		if(m_SeatIndex > -1 )
		{
			//allow the shot to tun again if we swapped seats instantaneously
			if(m_SeatIndex != camInterface::FindFollowPed()->GetAttachCarSeatIndex())
			{
				return true; 
			}
		}
	}
	return false; 
}

u32 camCinematicVehicleBonnetShot::GetCameraRef() const
{
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle())
	{	
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get()); 

		const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();
#if __BANK		
		u32 camHash = camInterface::GetCinematicDirector().GetOverriddenBonnetCamera(); 
		
		if(camHash != 0)
		{
			return camHash; 
		}
#endif
		const u32 povTurretCameraHash = camInterface::ComputePovTurretCameraHash(pVehicle);

		if(targetVehicleModelInfo)
		{
			const CPed* pFollowPed = camInterface::FindFollowPed();

			u32 vehicleModelHash = targetVehicleModelInfo->GetModelNameHash(); 
			const camVehicleCustomSettingsMetadata* pCustomVehicleSettings = camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash); 

			int seatIndex = pFollowPed->GetAttachCarSeatIndex(); 
			if(seatIndex > -1)
			{
				if(pCustomVehicleSettings && pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_ShouldConsiderData)
				{
					for(int i=0; i <pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera.GetCount(); i++)
					{
						if((u32)seatIndex == pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_SeatIndex )
						{
							if(pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_DisablePovForThisSeat)
							{
								return 0;
							}
							break;
						}
					}
				}
			}

			if(camCinematicDirector::ms_UseBonnetCamera || camInterface::GetGameplayDirector().IsForcingFirstPersonBonnet())
			{
				if(povTurretCameraHash != 0)
				{
					return povTurretCameraHash;
				}

				// Override bonnet camera for the RCBandito depending on chassis mod
				if (MI_CAR_RCBANDITO.IsValid() && pVehicle->GetModelIndex() == MI_CAR_RCBANDITO)
				{
					const CVehicleVariationInstance& variation = pVehicle->GetVehicleDrawHandler().GetVariationInstance();
					bool bVariation = false;
					u8 modIndex = variation.GetModIndexForType(VMT_CHASSIS, pVehicle, bVariation);

					if (modIndex >= 8 && modIndex <= 11)
					{
						return atHashWithStringNotFinal("RC_BONNET_CAMERA_HIGH", 0x46219A2A);
					}
					else if (modIndex >= 12)
					{
						return atHashWithStringNotFinal("RC_BONNET_CAMERA_HIGH_CLOSE", 0x14BD6CE3);
					}
				}

				return targetVehicleModelInfo->GetBonnetCameraNameHash(); 
			}
			else
			{
				if(seatIndex > -1)
				{
					if(pCustomVehicleSettings && pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_ShouldConsiderData)
					{
						for(int i=0; i <pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera.GetCount(); i++)
						{
							if((u32)seatIndex == pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_SeatIndex )
							{
								const u32 customCameraHash = pCustomVehicleSettings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_PovCameraHash;
								if (customCameraHash != 0)
								{
									return customCameraHash; 
								}
								break;
							}
						}
					}

					//If the ped is holding on to the sides of a vehicle e.g. firetruck or swat vehicles we use a custom camera
					const CVehicleLayoutInfo* attachVehicleLayout	= pVehicle->GetLayoutInfo();
					const CVehicleSeatAnimInfo* animSeatInfo		= attachVehicleLayout->GetSeatAnimationInfo(seatIndex);
					if (animSeatInfo && animSeatInfo->GetKeepCollisionOnWhenInVehicle() && (m_Metadata.m_HangingOnCameraRef != 0))
					{
						return m_Metadata.m_HangingOnCameraRef;
					}
					else if(animSeatInfo)
					{
						const CVehicleDriveByInfo* driveByInfo = animSeatInfo->GetDriveByInfo();
						if(driveByInfo && driveByInfo->GetPovDriveByCamera() != 0)
						{
							return driveByInfo->GetPovDriveByCamera().GetHash();
						}
					}

					if(povTurretCameraHash != 0)
					{
						return povTurretCameraHash;
					}
				}
				return targetVehicleModelInfo->GetPovCameraNameHash(); 
			}
		
		}
	}

	return 0;
}

void camCinematicVehicleBonnetShot::ClearShot(bool ResetCameraEndTime)
{
	camBaseCinematicShot::ClearShot(ResetCameraEndTime); 
	m_SeatIndex = -1; 
	m_IsRenderingPOVcamera = false; 
}

camCinematicVehicleConvertibleRoofShot::camCinematicVehicleConvertibleRoofShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicVehicleConvertibleRoofShotMetadata&>(metadata))
, m_InitialRoofState(-1)
, m_IsReset(false)
, m_CanActivate(false)
, m_ValidForVelocity(false)
, m_CanCreate(false)
{
}

bool camCinematicVehicleConvertibleRoofShot::IsValid() const
{
	if(camBaseCinematicShot::IsValid())
	{
		if(camCinematicDirector::IsSpectatingCopsAndCrooksEscape())
		{
			return false;
		}

		if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get());
			if(pVehicle)
			{
				return m_ValidForVelocity; 	
			}
		}
	}
	return false; 
}

bool camCinematicVehicleConvertibleRoofShot::CanCreate()
{
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle() && m_CanCreate)
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get());
		if(pVehicle)
		{
			if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
			{	
				if(m_CanActivate)
				{
					return true; 
				}
			}
		}
	}
	return false; 
}

void camCinematicVehicleConvertibleRoofShot::InitCamera()
{
	if(m_AttachEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicVehicleTrackingCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			m_UpdatedCamera->LookAt(m_LookAtEntity); 
			((camCinematicVehicleTrackingCamera*)m_UpdatedCamera.Get())->Init();
		}
	}
}

void camCinematicVehicleConvertibleRoofShot::PreUpdate(bool UNUSED_PARAM(OnlyUpdateIfNotCurrentShot))
{
	if(m_UpdatedCamera)
	{
		m_CanCreate = false; 
	}
	
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
				float Velocity = targetVehicle->GetVelocity().Mag2(); 

				if(Velocity < 0.05f)
				{
					if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISED ||
						targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED ||
						targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING ||  
						targetVehicle->GetConvertibleRoofState()  == CTaskVehicleConvertibleRoof::STATE_RAISING )
					{
						m_ValidForVelocity = true; 
					}
				}
				else
				{
					m_ValidForVelocity = false; 
				}
				
				if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISED ||
					targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED )
				{
					m_IsReset = true;
					m_InitialRoofState = -1; 
					m_CanCreate = true; 
				}
				
				if(m_IsReset && m_InitialRoofState == -1)
				{
					if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING)
					{
						m_InitialRoofState = targetVehicle->GetConvertibleRoofState(); 
					}
					
					if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING)
					{
						m_InitialRoofState = targetVehicle->GetConvertibleRoofState(); 
					}
				}

				//check that the lowering/raising state hasnt changed direction
				if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING && m_InitialRoofState == CTaskVehicleConvertibleRoof::STATE_RAISING)
				{
					cameraDebugf3("%d - targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING && m_InitialRoofState == CTaskVehicleConvertibleRoof::STATE_RAISING", fwTimer::GetFrameCount());
					m_IsReset = false; 	
				}

				if(targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING&& m_InitialRoofState == CTaskVehicleConvertibleRoof::STATE_LOWERING)
				{
					cameraDebugf3("%d - targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING&& m_InitialRoofState == CTaskVehicleConvertibleRoof::STATE_LOWERING", fwTimer::GetFrameCount());
					m_IsReset = false; 	
				}
				
				float roofProgress = targetVehicle->GetConvertibleRoofProgress(); 

				if(m_IsReset)
				{
					if((targetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING && roofProgress < 0.3f)||
						(targetVehicle->GetConvertibleRoofState()  == CTaskVehicleConvertibleRoof::STATE_RAISING && roofProgress > 0.7f))
					{
						m_CanActivate = true; 
					}
					else
					{
						cameraDebugf3("Not at the right phase, fwTimer::GetFrameCount()");
						m_CanActivate = false; 
					}
				}
				else
				{
					m_CanActivate = false; 
				}
			}
		}

	}
}

//crash camera

camCinematicInVehicleCrashShot::camCinematicInVehicleCrashShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicInVehicleCrashShotMetadata&>(metadata))
, m_TerminateTimer(0)
, m_IsValid(false)
{
}


void camCinematicInVehicleCrashShot::PreUpdate(bool UNUSED_PARAM(updateCurrent))
{
	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const s32 vehicleEntryExitState			= gameplayDirector.GetVehicleEntryExitState();

	if(vehicleEntryExitState == camGameplayDirector::INSIDE_VEHICLE )
	{
		//the follow vehicle camera is not valid
		const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();

		if(!followVehicleCamera)
		{
			m_IsValid = false;
			return;
		}

		//the follow vehicle is also not valid
		const CEntity* followVehicle = followVehicleCamera->GetAttachParent();

		if(!followVehicle || !followVehicle->GetIsTypeVehicle())
		{
			return;
		}

		const CVehicle* pVehicle = static_cast<const CVehicle*>(followVehicle);
		Vector3 VehicleVel = pVehicle->GetVelocity(); 

		if(pVehicle->GetNumContactWheels() == 0 && !m_IsValid)
		{
			m_WheelOnGroundTimer = 0; 
			
			Vector3 velocityLocal = VEC3_ZERO; 

			Matrix34 VehicleMat; 

			VehicleMat = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix()); 

			float roll = 0.0f;

			roll = camFrame::ComputeRollFromMatrix(VehicleMat); 

			if(pVehicle->GetFrameCollisionHistory() && pVehicle->GetFrameCollisionHistory()->GetNumCollidedEntities() > 0 && VehicleVel.XYMag2() > (m_Metadata.m_MinVelocity * m_Metadata.m_MinVelocity))
			{
				if(Abs(roll * RtoD) > m_Metadata.m_MinRoll)
				{
					m_IsValid = true; 
					m_ShotValidTimer = 0; 
				}
			}

		}
		else if (pVehicle->GetNumContactWheels() == pVehicle->GetNumWheels())
		{
			if(m_IsValid)
			{
				m_WheelOnGroundTimer += fwTimer::GetTimeStepInMilliseconds();

				if(m_WheelOnGroundTimer > m_Metadata.m_TimeToTerminateForWheelsOnTheGround)
				{
					m_IsValid = false; 

				}
			}
		}
		else if(m_IsValid)
		{
			if(VehicleVel.XYMag2() < m_Metadata.m_MinVelocityToTerminate * m_Metadata.m_MinVelocityToTerminate)
			{	
				if(m_IsValid)
				{
					m_TerminateTimer += fwTimer::GetTimeStepInMilliseconds();

					if(m_TerminateTimer > m_Metadata.m_TimeToTerminate )
					{
						m_IsValid = false; 
					}
				}
			}
			else
			{
				m_TerminateTimer = 0; 
			}
		}
	
	}
	else
	{
		m_IsValid = false;
	}

	if(m_IsValid && m_UpdatedCamera == NULL)
	{
		m_ShotValidTimer += fwTimer::GetTimeStepInMilliseconds();
		if( m_ShotValidTimer > 250)
		{
			m_IsValid = false;
		}
	}

}

bool camCinematicInVehicleCrashShot::CanCreate()
{
	u32 LastShotDuration = m_ShotEndTime - m_ShotStartTime; 
	
	if(m_IsValid && m_LookAtEntity && (((fwTimer::GetTimeInMilliseconds() - m_ShotEndTime) >  m_Metadata.m_MinTimeToCreateAfterShotFailure) || LastShotDuration == 0))
	{	
		return true; 
	}
	return false;
};

void camCinematicInVehicleCrashShot::ClearShot(bool UNUSED_PARAM(clearShot))
{	
	camBaseCinematicShot::ClearShot(false); 
	
	m_IsValid = false; 
}

bool camCinematicInVehicleCrashShot::IsValid() const
{
	return m_IsValid; 
}

void camCinematicInVehicleCrashShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicCamManCamera::GetStaticClassId()))
		{
			camCinematicCamManCamera* camManCamera = static_cast<camCinematicCamManCamera*>(m_UpdatedCamera.Get()); 
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			camManCamera->SetShouldScaleFovForLookAtTargetBounds(true); 
			camManCamera->SetMode(m_CurrentMode);
			camManCamera->SetCanUseLookAtDampingHelpers(true);
			camManCamera->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}
