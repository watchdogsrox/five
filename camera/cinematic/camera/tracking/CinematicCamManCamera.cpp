//
// camera/cinematic/CinematicCamManCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"

#include "profile/group.h"
#include "profile/page.h"

#include "camera/cinematic/cinematicdirector.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/InconsistentBehaviourZoomHelper.h"
#include "camera/helpers/LookAheadHelper.h"
#include "camera/helpers/LookAtDampingHelper.h"
#include "camera/system/CameraMetadata.h"
#include "game/ModelIndices.h"
#include "pathserver/pathserver.h"
#include "renderer/Water.h"
#include "vehicles/vehicle.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicCamManCamera,0xA828E29D)

PF_PAGE(camCinematicCamManCameraPage, "Camera: Cinematic Camera Man Camera");

PF_GROUP(camCinematicCamManCameraMetrics);
PF_LINK(camCinematicCamManCameraPage, camCinematicCamManCameraMetrics);

PF_VALUE_FLOAT(fullAmpl,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(desiredAmpl,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(dampedAmpl,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(headRatio,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(headingAmpl,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(pitchAmpl,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(rollAmpl,		camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(fovAmpl,			camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(desiredTurbulenceAmpl,	camCinematicCamManCameraMetrics);
PF_VALUE_FLOAT(dampedTurbulenceAmpl,	camCinematicCamManCameraMetrics);

#if __BANK
float camCinematicCamManCamera::ms_DebugUncertaintyShakeAmplitude	= 1.0f;
float camCinematicCamManCamera::ms_DebugHeadingRatio					= 0.5f;
float camCinematicCamManCamera::ms_DebugTurbulenceVehicleScale		= 1.0f;
float camCinematicCamManCamera::ms_DebugTurbulenceDistanceScale		= 0.5f;
bool camCinematicCamManCamera::ms_DebugSetUncertaintyShakeAmplitude	= false;
bool camCinematicCamManCamera::ms_DebugSetTurbulenceShakeAmplitude	= false;
bool camCinematicCamManCamera::ms_DebugSetShakeRatio				= false;
#endif // __BANK

camCinematicCamManCamera::camCinematicCamManCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicCameraManCameraMetadata&>(metadata))
, m_LookAtPosition(g_InvalidPosition)
, m_VehicleRelativeBoomShotPositionMultiplier(0.0f, 0.0f)
, m_StartTime(fwTimer::GetCamTimeInMilliseconds())
, m_Mode(0)
, m_PreviousTimeAtLostStaticLosToTarget(0)
, m_PreviousTimeAtLostDynamicLosToTarget(0)
, m_MaxShotDuration(0)
, m_BoomShotTimeAtStart(0)
, m_BoomShotCameraHeightAtStart(0.0f)
, m_BoomShotMininumHeight(0.0f)
, m_BoomShotTotalHeightChange(0.0f)
, m_MaxFov(m_Metadata.m_MaxZoomFov)
, m_MinFov(m_Metadata.m_MinZoomFov)
, m_CurrentFov(0.0f)
, m_TargetFov(0.0f)
, m_VehicleRelativeBoomShotScanRadius(0.0f)
, m_HeadingOnPreviousUpdate(0.0f)
, m_PitchOnPreviousUpdate(0.0f)
, m_RollOnPreviousUpdate(0.0f)
, m_FovOnPreviousUpdate(0.0f)
, m_HasAcquiredTarget(false)
, m_ShouldStartZoomedIn(fwRandom::GetRandomTrueFalse())
, m_ShouldZoom(fwRandom::GetRandomTrueFalse())
, m_ShouldElevatePosition(false)
, m_CanUpdate(true)
, m_IsActive(false)
, m_ShouldScaleFovForLookAtTargetBounds(m_Metadata.m_ShouldScaleFovForLookAtTargetBounds)
, m_CanUseDampedLookAtHelpers(false)
, m_ShouldUseVehicleRelativeBoomShot(false)
, m_CanUseLookAheadHelper(false)
, m_ReactedToInconsistentBehaviourLastUpdate(false)
, m_ZoomTriggeredFromInconsistency(false)
, m_HasLostStaticLosToTarget(false)
, m_HasLostDynamicLosToTarget(false)
, m_InVehicleLookAtDamper(NULL)
, m_OnFootLookAtDamper(NULL)
, m_InVehicleLookAheadHelper(NULL)
, m_InconsistentBehaviourZoomHelper(NULL)
{
	const camLookAtDampingHelperMetadata* pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(m_Metadata.m_InVehicleLookAtDampingRef.GetHash());

	if(pLookAtHelperMetaData)
	{
		m_InVehicleLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_InVehicleLookAtDamper, "Camera Man camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}
	
	
	pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(m_Metadata.m_OnFootLookAtDampingRef.GetHash());
	
	if(pLookAtHelperMetaData)
	{
		m_OnFootLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_OnFootLookAtDamper, "Camera Man camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}

	const camLookAheadHelperMetadata* pLookAheadHelperMetaData = camFactory::FindObjectMetadata<camLookAheadHelperMetadata>(m_Metadata.m_InVehicleLookAheadRef.GetHash());

	if(pLookAheadHelperMetaData)
	{
		m_InVehicleLookAheadHelper = camFactory::CreateObject<camLookAheadHelper>(*pLookAheadHelperMetaData);
		cameraAssertf(m_InVehicleLookAheadHelper, "Camera Man Camera (name: %s, hash: %u) cannot create a look ahead helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAheadHelperMetaData->m_Name.GetCStr()), pLookAheadHelperMetaData->m_Name.GetHash());
	}

	const camInconsistentBehaviourZoomHelperMetadata* inconsistentBehaviourZoomHelper = camFactory::FindObjectMetadata<camInconsistentBehaviourZoomHelperMetadata>(m_Metadata.m_InconsistentBehaviourZoomHelperRef.GetHash());

	if(inconsistentBehaviourZoomHelper)
	{
		m_InconsistentBehaviourZoomHelper = camFactory::CreateObject<camInconsistentBehaviourZoomHelper>(*inconsistentBehaviourZoomHelper);
		cameraAssertf(m_InconsistentBehaviourZoomHelper, "Camera Man Camera (name: %s, hash: %u) cannot create an inconsistent behaviour zoom helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(inconsistentBehaviourZoomHelper->m_Name.GetCStr()), inconsistentBehaviourZoomHelper->m_Name.GetHash());
	}
}

camCinematicCamManCamera::~camCinematicCamManCamera()
{
	if(m_InVehicleLookAtDamper)
	{
		delete m_InVehicleLookAtDamper; 
	}

	if(m_OnFootLookAtDamper)
	{
		delete m_OnFootLookAtDamper; 
	}

	if(m_InVehicleLookAheadHelper)
	{
		delete m_InVehicleLookAheadHelper; 
	}

	if (m_InconsistentBehaviourZoomHelper)
	{
		delete m_InconsistentBehaviourZoomHelper;
	}
}

void camCinematicCamManCamera::ExtendCameraPositionIfCloseToVehicleFront(const Matrix34& LookAtTargetMat, float Heading, float &scanRadius, float& scanDistance )
{
	if(m_Metadata.m_ShouldExtendRelativePositionForVelocity)		
	{
		if(m_LookAtTarget->GetIsPhysical())
		{
			const CPhysical* pPhys = static_cast<const CPhysical*>(m_LookAtTarget.Get()); 

			Vector3 velocity = pPhys->GetVelocity(); 

			if(velocity.Mag2() >= VERY_SMALL_FLOAT)
			{
				velocity.Normalize(); 

				//vehicle relative velocity vector
				LookAtTargetMat.UnTransform3x3(velocity);

				float relativeVelocityHeading = camFrame::ComputeHeadingFromFront(velocity); 

				Heading = fwAngle::LimitRadianAngle(Heading - relativeVelocityHeading);
			}	
		}

	}

	if(Abs(Heading) > PI - (m_Metadata.m_MinHeadingDeltaToExtendCameraPosition * DtoR)  )
	{
		scanRadius = scanRadius * m_Metadata.m_ScanRadiusScalar; 
		scanDistance = scanDistance * m_Metadata.m_ScanDistanceScalar; 
	}
};
bool camCinematicCamManCamera::Update()
{
	if(!m_HasAcquiredTarget)
	{
		cameraDebugf3("camCinematicCamManCamera: Failed: !m_HasAcquiredTarget"); 
		m_CanUpdate = false; 
		return false;
	}

	if(m_LookAtTarget.Get() == NULL) //Safety check.
	{
		cameraDebugf3("camCinematicCamManCamera: Failed: m_HasAcquiredTarget = false;"); 
		m_HasAcquiredTarget = false;
		m_CanUpdate = false; 
		return false;
	}

	if(m_LookAtTarget->GetIsTypeVehicle())
	{
		const CVehicle* targetVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get());
		if(!cameraVerifyf((targetVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN) && (targetVehicle->GetModelIndex() != MI_CAR_CABLECAR),
			"A cinematic camera-man camera is targetting a train or a cable car"))
		{
			m_CanUpdate = false;
			return false; //Trains should be handled by camCinematicTrainTrack.
		}
	}

	if(NetworkInterface::IsGameInProgress() && m_LookAtTarget->GetIsTypePed() && static_cast<const CPed*>(m_LookAtTarget.Get())->IsNetworkClone())
	{
		if(NetworkInterface::IsEntityHiddenNetworkedPlayer(m_LookAtTarget))
		{
			m_CanUpdate = false;
			return false; //Spectating a player that is hidden under the map
		}
	}

	Vector3 cameraPosition	= m_Frame.GetPosition();
	m_LookAtPosition		= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

	//push the look at position dependent on the direction of travel
	if (m_LookAtTarget->GetIsTypeVehicle() && m_CanUseLookAheadHelper && m_InVehicleLookAheadHelper)
	{
		Vector3 lookAheadPositionOffset = VEC3_ZERO;
		const CPhysical* lookAtTargetPhysical = static_cast<const CPhysical*>(m_LookAtTarget.Get());
		m_InVehicleLookAheadHelper->UpdateConsideringLookAtTarget(lookAtTargetPhysical, m_NumUpdatesPerformed == 0, lookAheadPositionOffset);
		m_LookAtPosition += lookAheadPositionOffset;
	}

	if(!m_IsActive)
	{
		Vector3 searchPos	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
		float scanRadius	= m_Metadata.m_ScanRadius;
		float scanDistance	= m_Metadata.m_ScanDistance;
		
		if (m_ShouldUseVehicleRelativeBoomShot)
		{
			const Vector3 searchPositionOffset	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetA()) * m_VehicleRelativeBoomShotPositionMultiplier.x
												+ VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetB()) * m_VehicleRelativeBoomShotPositionMultiplier.y;
			const Vector3 lookAtTargetPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
			searchPos		= lookAtTargetPosition + searchPositionOffset;
			scanRadius		= m_VehicleRelativeBoomShotScanRadius;
		}
		else
		{
			camCinematicDirector::eSideOfTheActionLine sideOfTheActionLine = camInterface::GetCinematicDirector().GetSideOfActionLine(m_LookAtTarget); 

			Matrix34 LookAtTargetMat = MAT34V_TO_MATRIX34(m_LookAtTarget->GetTransform().GetMatrix());
			float VehicleHeading = camFrame::ComputeHeadingFromMatrix(LookAtTargetMat);

			Vector3	LookAtForward = LookAtTargetMat.b;
			//compute the vector from the camera pos to parent
			Vector3 vLastToLookAt = LookAtTargetMat.d - camInterface::GetCinematicDirector().GetLastValidCinematicCameraPosition();
			vLastToLookAt.NormalizeSafe(); 
			//compute the relative angle between this and vehicle front
			float heading = fabsf(LookAtForward.AngleZ(vLastToLookAt)); 

			float DesiredHeading = 0.0f;
			switch(sideOfTheActionLine)
			{
			case camCinematicDirector::ACTION_LINE_ANY:
				{
					float RandomHeading = RandomiseInitialHeading(); 
					ExtendCameraPositionIfCloseToVehicleFront(LookAtTargetMat, RandomHeading, scanRadius, scanDistance );
					DesiredHeading = fwAngle::LimitRadianAngle(VehicleHeading + RandomHeading);
				}
				break; 
			case camCinematicDirector::ACTION_LINE_LEFT:
				{
					//heading with respect target
					float RandomHeading = RandomiseInitialHeading(heading, m_Metadata.m_MinAngleBetweenShots* DtoR); 
					ExtendCameraPositionIfCloseToVehicleFront(LookAtTargetMat, RandomHeading, scanRadius, scanDistance );
					DesiredHeading = fwAngle::LimitRadianAngle(VehicleHeading - RandomHeading);
				}
				break;
			case camCinematicDirector::ACTION_LINE_RIGHT:
				{
					//heading with respect target
					float RandomHeading = RandomiseInitialHeading(heading, m_Metadata.m_MinAngleBetweenShots* DtoR);
					ExtendCameraPositionIfCloseToVehicleFront(LookAtTargetMat, RandomHeading, scanRadius, scanDistance ); 
					DesiredHeading = fwAngle::LimitRadianAngle(VehicleHeading + RandomHeading);
				}
				break;
			}

			Vector3 Front(VEC3_ZERO); 
			camFrame::ComputeFrontFromHeadingAndPitch(DesiredHeading, 0.0f, Front);
			Front.NormalizeSafe(); 

			searchPos = LookAtTargetMat.d  - (Front * (scanRadius + scanDistance)); 
		}

		if(m_Metadata.m_UseGroundProbe)
		{
			bool bHasSucceded = false; 
			float groundz  = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, searchPos, &bHasSucceded);
			
			if(!bHasSucceded)
			{
				cameraDebugf3("camCinematicCamManCamera: Failed: using ground probe but failed"); 
				m_CanUpdate = false;
				return false; 
			}			
			searchPos.z = groundz; 
		}
		
		EGetClosestPosRet pathServerResult = ENoPositionFound; 

		if(m_Metadata.m_UseRadiusCheckToComputeNavMeshPos)
		{
			pathServerResult = CPathServer::GetClosestPositionForPed(searchPos, cameraPosition, scanRadius);
		}
		else
		{
			Vector3 NavMeshPos; 
			if(CPathServer::TestNavMeshPolyUnderPosition(searchPos, NavMeshPos, true, false))
			{
				cameraPosition = searchPos;
				
				cameraPosition.z = NavMeshPos.z; 
				
				if(Abs(searchPos.z - cameraPosition.z) < scanRadius * 2.0f )
				{
					pathServerResult = EPositionFoundOnMainMap; 
				}
			}
		}

		if(pathServerResult != ENoPositionFound)
		{
			float WaterHeight = 0.0f; 

			float FixedUpCameraHeight = cameraPosition.z; 

			bool HaveValidWaterHeight = false; 

			float FixedUpWaterHeight = -MINMAX_MAX_FLOAT_VAL; 
			
			m_ShouldElevatePosition = fwRandom::GetRandomTrueFalse();

			if(m_ShouldElevatePosition) //Sometimes we will have it low down...
			{
				FixedUpCameraHeight = cameraPosition.z += m_Metadata.m_ElevationHeightOffset;
			}

			if(camCollision::ComputeWaterHeightAtPosition(cameraPosition, m_Metadata.m_MaxDistanceForWaterClippingTest, WaterHeight,  m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
			{
				FixedUpWaterHeight = WaterHeight + m_Metadata.m_HeightOffsetAboveWater; 
			}

			static Vector3 ms_scanOffset(0.0f, 0.0f, -10.0f);

			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(cameraPosition, cameraPosition+ms_scanOffset);
			probeDesc.SetExcludeEntity(m_LookAtTarget, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
			probeDesc.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
			probeDesc.SetContext(WorldProbe::LOS_Camera);
			
			//water is higher than the navmesh lets use that
			if(FixedUpWaterHeight > FixedUpCameraHeight)
			{
				cameraPosition.z = FixedUpWaterHeight; 
				HaveValidWaterHeight = true; 
			}
			else
			{
				cameraPosition.z = FixedUpCameraHeight; 
			}

			//no valid world prob and no water height reject this position
			if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc) && !HaveValidWaterHeight)
			{
				//Nothing below this position indicates it is below the map - reject it.
				m_CanUpdate = false;
				pathServerResult	= ENoPositionFound;
			}
			
			if(pathServerResult != ENoPositionFound)
			{
				if (m_ShouldUseVehicleRelativeBoomShot)
				{
					//make sure we're behind the look at target
					const Vector3 camPositionToLookAtPosition	= m_LookAtPosition - cameraPosition;
					const Vector3 lookAtForward					= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetB());
					const float dotProduct						= camPositionToLookAtPosition.Dot(lookAtForward);
					if (dotProduct < -SMALL_FLOAT)
					{
						m_CanUpdate = false;
						return false;
					}
					else
					{
						//if using vehicle relative boom shot, bump the camera up
						cameraPosition.z += m_BoomShotMininumHeight;

					}
				}

				if(!m_ShouldElevatePosition)
				{
					m_Frame.SetFov(m_Metadata.m_MinZoomFov); //Might not get used though
				}


				if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, cameraPosition))
				{
					m_CanUpdate = false;
				}
			}

			if (!IsNearZero(m_BoomShotTotalHeightChange) && m_CanUpdate)
			{
				const bool canUseBoomShot = ComputeCanUseBoomShot(cameraPosition);

				if (!canUseBoomShot)
				{
					m_CanUpdate = false;
				}
				else
				{
					if (m_BoomShotTotalHeightChange < -SMALL_FLOAT)
					{
						//falling down so start from the top
						cameraPosition += ZAXIS * Abs(m_BoomShotTotalHeightChange);
					}
					m_BoomShotCameraHeightAtStart	= cameraPosition.z;
					m_BoomShotTimeAtStart			= fwTimer::GetCamTimeInMilliseconds();
				}

			}

			if(m_CanUpdate)
			{
				m_IsActive = true;
			}
		}
		else
		{
			m_CanUpdate = false;
		}
	}

	if(!m_CanUpdate)
	{
		return false;
	}

	if (!IsNearZero(m_BoomShotTotalHeightChange))
	{
		//get blend level
		const u32 time		= fwTimer::GetCamTimeInMilliseconds();
		float blendLevel	= RampValueSafe(static_cast<float>(time),
								static_cast<float>(m_BoomShotTimeAtStart), static_cast<float>(m_BoomShotTimeAtStart + m_MaxShotDuration),
								0.0f, 1.0f);
		blendLevel			= SlowInOut(SlowInOut(blendLevel));

		if (blendLevel > SMALL_FLOAT)
		{
			//apply height change
			const float offset	= blendLevel * m_BoomShotTotalHeightChange;
			cameraPosition.z	= m_BoomShotCameraHeightAtStart + offset;
		}
	}

	Vector3 front = m_LookAtPosition - cameraPosition;
	front.Normalize();

	m_Frame.SetWorldMatrixFromFront(front);
	m_Frame.SetPosition(cameraPosition);

	if(!ComputeIsPositionValid())
	{
		//Simply flag that we lost the target, so that this camera will report that it's not happy on the next update.
		//NOTE: We still allow this update to proceed as it is better to render an imperfect frame from this camera than have the discontinuity of no
		//cinematic camera frame.
		m_CanUpdate = false;
	}
	
	if(m_ShouldScaleFovForLookAtTargetBounds)
	{	
		ScaleFovBasedOnLookAtTargetBounds(cameraPosition); 
	}
	else
	{
		UpdateZoom(cameraPosition);
	}

	//apply damped look at 
	if(m_CanUseDampedLookAtHelpers)
	{
		if(m_LookAtTarget->GetIsTypeVehicle())
		{
			if(m_InVehicleLookAtDamper)
			{
				Matrix34 WorldMat(m_Frame.GetWorldMatrix()); 

				m_InVehicleLookAtDamper->Update(WorldMat, m_Frame.GetFov(), m_NumUpdatesPerformed == 0); 

				m_Frame.SetWorldMatrix(WorldMat);
			}
		}
		else
		{
			if(m_OnFootLookAtDamper)
			{
				Matrix34 WorldMat(m_Frame.GetWorldMatrix()); 

				m_OnFootLookAtDamper->Update(WorldMat, m_Frame.GetFov(), m_NumUpdatesPerformed == 0); 

				m_Frame.SetWorldMatrix(WorldMat);
			}
		}
	}

	UpdateShake();

	return m_CanUpdate;
}

void camCinematicCamManCamera::ScaleFovBasedOnLookAtTargetBounds(const Vector3& CameraPos)
{	
	const CVehicle* targetVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get());

	float boundRadius = targetVehicle->GetBoundRadius() * m_Metadata.m_BoundsRadiusScaleForMaxFov; 

	float DistanceToTarget = VEC3V_TO_VECTOR3(targetVehicle->GetTransform().GetPosition()).Dist(CameraPos); 
	
	float Fov = rage::Atan2f( boundRadius , DistanceToTarget) * 2.0f; 

	Fov = Clamp(Fov, m_Metadata.m_MaxZoomFov * DtoR, m_Metadata.m_MinZoomFov* DtoR); 
	
	DampFov(Fov); 

	m_Frame.SetFov(Fov * RtoD);
}

void camCinematicCamManCamera::DampFov(float& Fov)
{	
	if(GetNumUpdatesPerformed() == 0)
	{
		m_FovSpring.Reset(Fov); 
	}
	else
	{
		m_FovSpring.Update(Fov, m_Metadata.m_FovSpringConstant, 1.0f, true); 

		Fov = m_FovSpring.GetResult(); 
	}
}


float camCinematicCamManCamera::RandomiseInitialHeading()
{
	float Heading = 0.0f; 

	float AngleDelta = TWO_PI / (float)GetNumOfModes(); 

	float startSangle = AngleDelta * float(m_Mode); 

	float RandomAngle = fwRandom::GetRandomNumberInRange( 0.0f, AngleDelta);

	Heading = startSangle + RandomAngle; 
	
	return fwAngle::LimitRadianAngle(Heading); 
}

float camCinematicCamManCamera::RandomiseInitialHeading(float ForbiddenAngle, float MinAngleDelta)
{
	
	float ForbiddenMinAngle = ForbiddenAngle - MinAngleDelta; 
	float ForbiddenMaxAngle = ForbiddenAngle + MinAngleDelta; 

	//check that the angle is valid
	
	ForbiddenMinAngle = Clamp(ForbiddenMinAngle, 0.0f, PI); 
	ForbiddenMaxAngle = Clamp(ForbiddenMaxAngle, 0.0f, PI); 

	float MaxRange =  PI - ForbiddenMaxAngle; 
	float MinRange = ForbiddenMinAngle; 

	bool IsMaxRangeValid = MaxRange > 15.0f * DtoR; 
	bool IsMinRangeValid = MinRange > 15.0f * DtoR; 
	
	//compute the total range
	float TotalRange; 

	if(IsMaxRangeValid && IsMinRangeValid)	
	{
		TotalRange = MaxRange + MinRange; 
	}
	else if(IsMaxRangeValid)
	{
		TotalRange = MaxRange; 
	}
	else
	{
		TotalRange = MinRange; 
	}
	
	//compute the angle step size

	float AngleStepSize = TotalRange / GetNumOfModes(); 

	float DesiredAngle = AngleStepSize * m_Mode; 
	
	if(IsMaxRangeValid && IsMinRangeValid)	
	{
		//if the desired is greater than the 
		if(DesiredAngle > MinRange)
		{
			DesiredAngle = (DesiredAngle - MinRange) + ForbiddenMaxAngle; 
		}
	}
	else if(IsMaxRangeValid)
	{
		DesiredAngle = ForbiddenMaxAngle + DesiredAngle; 
	}

	return  DesiredAngle; 
}

bool camCinematicCamManCamera::ComputeCanUseBoomShot( const Vector3& cameraPosition ) const
{
	WorldProbe::CShapeTestCapsuleDesc verticalCapsuleTest;

	verticalCapsuleTest.SetIncludeFlags(camCollision::GetCollisionIncludeFlagsForClippingTests());
	verticalCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	verticalCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	verticalCapsuleTest.SetIsDirected(false);

	//we test from camera position up. if we fall down, we'll start from the top.
	const Vector3 endPosition = cameraPosition + (ZAXIS * Abs(m_BoomShotTotalHeightChange));
	verticalCapsuleTest.SetCapsule(cameraPosition, endPosition, m_Metadata.m_CollisionRadius);
	
	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(verticalCapsuleTest);

	return !hasHit;
}

bool camCinematicCamManCamera::IsClipping(const Vector3& position, const u32 flags)
{
	float  waterHeight; 
	
	if(camCollision::ComputeWaterHeightAtPosition(position, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight, m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(position.z <  waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true; 
		}
	}

	float collisionRadius = m_ShouldElevatePosition ? m_Metadata.m_CollisionRadiusElevated : m_Metadata.m_CollisionRadius;
	
	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	sphereTest.SetIncludeFlags(flags); 
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(position, collisionRadius* m_Metadata.m_RadiusScalingForClippingTest);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	bool results = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest); 

	return results;
}

CEntity* camCinematicCamManCamera::GetVehiclePedIsExitingEnteringOrIn(const CPed* pPed)
{
	CEntity* pExcludeEntity = NULL; 

	if(pPed->GetIsEnteringVehicle())
	{
		pExcludeEntity = pPed->GetVehiclePedEntering(); 
	}
	else if(pPed->IsExitingVehicle())
	{
		pExcludeEntity = static_cast<CEntity*>(pPed->GetAttachParent()); 
	}
	else if(pPed->GetIsInVehicle())
	{
		pExcludeEntity = pPed->GetMyVehicle(); 
	}

	return pExcludeEntity; 
}

void camCinematicCamManCamera::SetBoomShot(float totalHeightChange)
{
	totalHeightChange = Abs(totalHeightChange);

	if (totalHeightChange < SMALL_FLOAT)
	{
		m_BoomShotTotalHeightChange = 0.0f;
		return;
	}

	const bool shouldMoveUp = fwRandom::GetRandomTrueFalse();
	m_BoomShotTotalHeightChange = shouldMoveUp ? totalHeightChange : -totalHeightChange;
}

void camCinematicCamManCamera::SetVehicleRelativeBoomShot(float totalHeightChange, Vector2 vehicleRelativePositionMultiplier,
														  float minimumHeight, float scanRadius)
{
	totalHeightChange = Abs(totalHeightChange);

	if (totalHeightChange < SMALL_FLOAT)
	{
		m_BoomShotTotalHeightChange = 0.0f;
		return;
	}

	//always move up when doing this boom shot
	m_BoomShotTotalHeightChange					= totalHeightChange;

	const float targetRadius					= m_LookAtTarget->GetBoundRadius();
	m_VehicleRelativeBoomShotPositionMultiplier	= vehicleRelativePositionMultiplier * targetRadius;
	m_BoomShotMininumHeight						= minimumHeight;
	m_VehicleRelativeBoomShotScanRadius			= scanRadius;
	m_ShouldUseVehicleRelativeBoomShot			= true;
}

bool camCinematicCamManCamera::ComputeIsPositionValid()
{
	if(m_CanUpdate == false)
	{
		return false; 
	}
	
	const Vector3 targetPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
	const Vector3& cameraPosition	= m_Frame.GetPosition();
	Vector3 delta					= targetPosition - cameraPosition;
	float distanceToTarget;
	if(m_Metadata.m_ShouldConsiderZDistanceToTarget)	
	{
		distanceToTarget = delta.Mag();
	}
	else
	{
		distanceToTarget = delta.XYMag();
	}

	//Check the target is with range:
	float SpeedDistanceScalar = 1.0f; 

	if(m_LookAtTarget->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get()); 
		
		if(pVehicle)
		{
			float fSpeed = DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));

			if(pVehicle->InheritsFromBike())
			{
				SpeedDistanceScalar = RampValueSafe (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, m_Metadata.m_MaxDistanceFailureVelocityScalarForBike);
			}
			else
			{
				SpeedDistanceScalar = RampValueSafe (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, m_Metadata.m_MaxDistanceFailureVelocityScalar);
			}
			
		}
	}
	
	if((distanceToTarget<m_Metadata.m_MinDistanceToTarget) || (distanceToTarget > m_Metadata.m_MaxDistanceToTarget * SpeedDistanceScalar))
	{
		cameraDebugf3("camCinematicCamManCamera: Failed: distance to target %f, allowed distance %f", distanceToTarget, m_Metadata.m_MaxDistanceToTarget * SpeedDistanceScalar); 
		return false;
	}

	if(IsClipping(cameraPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
	{
		cameraDebugf3("camCinematicCamManCamera: Failed: distance to clipping"); 
		return false; 
	}

	//We don't want the camera outside looking inside or vice versa.
	s32 dummyRoomIdx;
	CInteriorInst* pIntInst = NULL;
	CPortalTracker::ProbeForInterior(cameraPosition, pIntInst, dummyRoomIdx, NULL, 10.0f);
	bool isCameraInside = (pIntInst != NULL);

	bool isTargetInside = m_LookAtTarget->m_nFlags.bInMloRoom;

	if(isTargetInside != isCameraInside)
	{
		return false;
	}

	u32 time = fwTimer::GetCamTimeInMilliseconds();

	CEntity* pExcludeEntity = NULL; 

	if(m_LookAtTarget->GetIsTypePed())
	{
		const CPed* pPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		pExcludeEntity = GetVehiclePedIsExitingEnteringOrIn(pPed); 
	}

	const u32 staticIncludeFlags	= static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);
	m_HasLostStaticLosToTarget		= !camCollision::ComputeIsLineOfSightClear(targetPosition, cameraPosition, m_LookAtTarget, staticIncludeFlags, pExcludeEntity);
	m_HasLostDynamicLosToTarget		= !camCollision::ComputeIsLineOfSightClear(targetPosition, cameraPosition, m_LookAtTarget, camCollision::GetCollisionIncludeFlags(), pExcludeEntity);

	if(!m_HasLostStaticLosToTarget)
	{
		m_PreviousTimeAtLostStaticLosToTarget	= time;
	}
	if(!m_HasLostDynamicLosToTarget)
	{
		m_PreviousTimeAtLostDynamicLosToTarget	= time;
	}

	if(m_NumUpdatesPerformed == 0)
	{
		if (m_HasLostStaticLosToTarget || m_HasLostDynamicLosToTarget)
		{
			cameraDebugf3("camCinematicCamManCamera: Failed: distance to Los frame 0"); 
			//Stop right away if we catch out of LOS early on.
			return false;
		}
	}
	else
	{
		const u32 timeOutOfStaticLos	= time - m_PreviousTimeAtLostStaticLosToTarget;
		const u32 timeOutOfDynamicLos	= time - m_PreviousTimeAtLostDynamicLosToTarget;
		if(timeOutOfStaticLos > m_Metadata.m_MaxTimeOutOfStaticLos || timeOutOfDynamicLos > m_Metadata.m_MaxTimeOutOfDynamicLos)
		{
			cameraDebugf3("camCinematicCamManCamera: Failed: distance to Los"); 
			return false;
		}
	}

	return true;
}


float camCinematicCamManCamera::ComputeFov(const Vector3& position, const float boundRadiusScale) const
{
	float fov = m_Metadata.m_MaxZoomFov;

	if(m_LookAtTarget)
	{
		const float boundRadius = m_LookAtTarget->GetBoundRadius() * boundRadiusScale;
		const Vector3 targetPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

		const float distanceToTarget = targetPosition.Dist(position);

		float desiredFov = rage::Atan2f(boundRadius, distanceToTarget) * 2.0f;
		desiredFov = desiredFov * RtoD;

		fov = Clamp(desiredFov, m_Metadata.m_MaxZoomFov, m_Metadata.m_MinZoomFov);
	}

	return fov;
}

void camCinematicCamManCamera::ComputeMaxFovBasedOnTargetVelocity(float &MaxFov)
{	
	if(m_LookAtTarget && m_LookAtTarget->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get()); 

		if(pVehicle)
		{
			float fSpeed = pVehicle->GetVelocity().Mag();

			float TValue = RampValueSafe (Abs(fSpeed), m_Metadata.m_MinSpeedForHighFovScalar, m_Metadata.m_MaxSpeedForHighFovScalar, 0.0f, 1.0f);

			MaxFov = Lerp(TValue, m_MaxFov, m_Metadata.m_HighSpeedMaxZoomFov); 
		}

		DampFov(MaxFov); 
	}
}

void camCinematicCamManCamera::TriggerZoom(float InitialFov, float TargetFov, u32 ZoomDuration)
{
	m_CurrentFov = InitialFov; 
	m_TargetFov = TargetFov; 
	m_ZoomDuration = ZoomDuration; 
	m_CurrentZoomTime = 0; 
	m_ShouldZoom = true; 
}

void camCinematicCamManCamera::UpdateZoom(const Vector3& position)
{
	if(m_NumUpdatesPerformed == 0)
	{
		m_MinFov = ComputeFov(position, m_Metadata.m_BoundsRadiusScaleForMinFov);
		m_MaxFov = ComputeFov(position, m_Metadata.m_BoundsRadiusScaleForMaxFov);
	}

	float DesiredFov = m_Frame.GetFov(); 

	if(m_NumUpdatesPerformed == 0)
	{
		bool startZoomedIn = fwRandom::GetRandomTrueFalse(); 
		bool shouldZoomInAtStart = fwRandom::GetRandomTrueFalse();

		if(startZoomedIn)
		{
			DesiredFov = m_MaxFov; 
		}
		else
		{
			DesiredFov = m_MinFov;

			if(shouldZoomInAtStart)
			{
				u32 ZoomDuration = fwRandom::GetRandomNumberInRange((s32)m_Metadata.m_MinInitialZoomDuration, (s32)m_Metadata.m_MaxInitialZoomDuration);
				TriggerZoom(m_MinFov, m_MaxFov, ZoomDuration); 
			}
		}
	}

	UpdateZoomForInconsistencies(position);

	if(m_ShouldZoom)
	{
		float zoomT = (float)(m_CurrentZoomTime) / (float)m_ZoomDuration;
		zoomT = Clamp(zoomT, 0.0f, 1.0f);
		const float zoomTimeToConsider = m_ZoomTriggeredFromInconsistency ? SlowOut(SlowOut(zoomT)) : SlowInOut(zoomT);
		DesiredFov = Lerp(zoomTimeToConsider, m_CurrentFov, m_TargetFov);

		m_CurrentZoomTime += fwTimer::GetCamTimeStepInMilliseconds(); 

		if(m_CurrentZoomTime > m_ZoomDuration)
		{
			m_ShouldZoom = false; 
			m_CurrentZoomTime = 0; 
		}
	}

	//zooming from inconsistency ignores these restrictions
	if (!m_ZoomTriggeredFromInconsistency)
	{
		float MaxFov = m_MaxFov; 
		float MinFov = m_MinFov;

		ComputeMaxFovBasedOnTargetVelocity(MaxFov); 

		if(m_MinFov < MaxFov)
		{
			MinFov = MaxFov; 
		}

		DesiredFov = Clamp(DesiredFov, MaxFov , MinFov); 
	}

	m_Frame.SetFov(DesiredFov); 
}

void camCinematicCamManCamera::UpdateZoomForInconsistencies(const Vector3& position)
{
	if (!m_InconsistentBehaviourZoomHelper)
	{
		return;
	}

	const bool hasLosToTarget = !m_HasLostStaticLosToTarget && !m_HasLostDynamicLosToTarget;
	const bool shouldReactToInconsistentBehaviour = m_InconsistentBehaviourZoomHelper->UpdateShouldReactToInconsistentBehaviour(m_LookAtTarget, m_Frame, hasLosToTarget);

	if (shouldReactToInconsistentBehaviour && !m_ReactedToInconsistentBehaviourLastUpdate)
	{
		const float initialFov			= m_Frame.GetFov();

		const float boundRadiusScale	= m_InconsistentBehaviourZoomHelper->GetBoundsRadiusScale();
		const float maxInconsistentFov	= m_InconsistentBehaviourZoomHelper->GetMaxFov();

		float fovToUse					= ComputeFov(position, boundRadiusScale);
		fovToUse						= Clamp(fovToUse, m_Metadata.m_MaxZoomFov, maxInconsistentFov);

		const float minFovDifference	= m_InconsistentBehaviourZoomHelper->GetMinFovDifference();
		if (initialFov < (fovToUse - minFovDifference))
		{
			const float targetFov		= fovToUse;
			const u32 zoomDuration		= m_InconsistentBehaviourZoomHelper->GetZoomDuration();
			TriggerZoom(initialFov, targetFov, zoomDuration);
			m_ZoomTriggeredFromInconsistency = true;
		}
		else
		{
			//disable any currently running zoom
			m_ShouldZoom = false; 
			m_CurrentZoomTime = 0; 
		}
	}

	m_ReactedToInconsistentBehaviourLastUpdate = shouldReactToInconsistentBehaviour;
}

//There is duplicate UpdateShake and UpdateCameraOperatorShakeAmplitudes functions in CinematicHeliChaseCamera.
//If you change anything here, change it there too. This ideally needs to be refactored into a helper when there's time.
void camCinematicCamManCamera::UpdateShake()
{
	const camCinematicCameraOperatorShakeUncertaintySettings& uncertaintySettings = m_Metadata.m_CameraOperatorShakeSettings.m_UncertaintySettings;
	const camCinematicCameraOperatorShakeTurbulenceSettings& turbulenceSettings = m_Metadata.m_CameraOperatorShakeSettings.m_TurbulenceSettings;

	if (!uncertaintySettings.m_XShakeRef && !uncertaintySettings.m_ZShakeRef && !turbulenceSettings.m_ShakeRef)
	{
		return;
	}

	float xUncertaintyAmplitude;
	float zUncertaintyAmplitude;
	float turbulenceAmplitude;
	UpdateCameraOperatorShakeAmplitudes(xUncertaintyAmplitude, zUncertaintyAmplitude, turbulenceAmplitude);
	
	if (uncertaintySettings.m_XShakeRef)
	{
		if (camBaseFrameShaker* shake = FindFrameShaker(uncertaintySettings.m_XShakeRef))
		{
			shake->SetAmplitude(xUncertaintyAmplitude);
		}
		else
		{
			Shake(uncertaintySettings.m_XShakeRef, xUncertaintyAmplitude);
		}
	}

	if (uncertaintySettings.m_ZShakeRef)
	{
		if (camBaseFrameShaker* shake = FindFrameShaker(uncertaintySettings.m_ZShakeRef))
		{
			shake->SetAmplitude(zUncertaintyAmplitude);
		}
		else
		{
			Shake(uncertaintySettings.m_ZShakeRef, zUncertaintyAmplitude);
		}
	}

	if (turbulenceSettings.m_ShakeRef)
	{
		if (camBaseFrameShaker* shake = FindFrameShaker(turbulenceSettings.m_ShakeRef))
		{
			shake->SetAmplitude(turbulenceAmplitude);
		}
		else
		{
			Shake(turbulenceSettings.m_ShakeRef, turbulenceAmplitude);
		}
	}
}

void camCinematicCamManCamera::UpdateCameraOperatorShakeAmplitudes(float& xUncertaintyAmplitude, float& zUncertaintyAmplitude, float& turbulenceAmplitude)
{
	xUncertaintyAmplitude = 0.0f;
	zUncertaintyAmplitude = 0.0f;
	turbulenceAmplitude = 0.0f;

	float heading;
	float pitch;
	float roll;
	camFrame::ComputeHeadingPitchAndRollFromMatrix(m_Frame.GetWorldMatrix(), heading, pitch, roll);
	float fov = m_Frame.GetFov();
	if (m_NumUpdatesPerformed == 0)
	{
		m_HeadingOnPreviousUpdate	= heading;
		m_PitchOnPreviousUpdate		= pitch;
		m_RollOnPreviousUpdate		= roll;
		m_FovOnPreviousUpdate		= fov;
	}

	//Uncertainty shake
	const camCinematicCameraOperatorShakeUncertaintySettings& uncertaintySettings = m_Metadata.m_CameraOperatorShakeSettings.m_UncertaintySettings;

	const float invTimeStep			= fwTimer::GetInvTimeStep();

	const float headingSpeed		= fwAngle::LimitRadianAngle(heading - m_HeadingOnPreviousUpdate) * invTimeStep;
	const float headingAmplitude	= RampValueSafe(Abs(headingSpeed), uncertaintySettings.m_AbsHeadingSpeedRange.x, uncertaintySettings.m_AbsHeadingSpeedRange.y, 0.0f, 1.0f);
	PF_SET(headingAmpl, headingAmplitude);

	const float pitchSpeed			= fwAngle::LimitRadianAngle(pitch - m_PitchOnPreviousUpdate) * invTimeStep;
	const float pitchAmplitude		= RampValueSafe(Abs(pitchSpeed), uncertaintySettings.m_AbsPitchSpeedRange.x, uncertaintySettings.m_AbsPitchSpeedRange.y, 0.0f, 1.0f);
	PF_SET(pitchAmpl, pitchAmplitude);

	const float rollSpeed			= fwAngle::LimitRadianAngle(roll - m_RollOnPreviousUpdate) * invTimeStep;
	const float rollAmplitude		= RampValueSafe(Abs(rollSpeed), uncertaintySettings.m_AbsRollSpeedRange.x, uncertaintySettings.m_AbsRollSpeedRange.y, 0.0f, 1.0f);
	PF_SET(rollAmpl, rollAmplitude);

	const float invCamTimeStep		= fwTimer::GetCamInvTimeStep();
	const float fovSpeed			= (fov - m_FovOnPreviousUpdate) * invCamTimeStep;
	const float fovAmplitude		= RampValueSafe(Abs(fovSpeed), uncertaintySettings.m_AbsFovSpeedRange.x, uncertaintySettings.m_AbsFovSpeedRange.y, 0.0f, 1.0f);
	PF_SET(fovAmpl, fovAmplitude);

	m_HeadingOnPreviousUpdate		= heading;
	m_PitchOnPreviousUpdate			= pitch;
	m_RollOnPreviousUpdate			= roll;
	m_FovOnPreviousUpdate			= fov;

	//add together to get overall amplitude
	float desiredUncertaintyAmplitude = Clamp(headingAmplitude + pitchAmplitude + rollAmplitude + fovAmplitude, 0.0f, 1.0f);
	desiredUncertaintyAmplitude		*= uncertaintySettings.m_ScalingFactor;

#if __BANK
	if (ms_DebugSetUncertaintyShakeAmplitude)
	{
		desiredUncertaintyAmplitude	= ms_DebugUncertaintyShakeAmplitude * uncertaintySettings.m_ScalingFactor;
	}
#endif // __BANK

	const bool shouldCutSpring					= (m_NumUpdatesPerformed == 0)
													|| m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition)
													|| m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float uncertaintyAmplitudeLastUpdate	= m_CameraOperatorUncertaintySpring.GetResult();
	const float uncertaintySpringConstant		= shouldCutSpring ? 0.0f :
													(desiredUncertaintyAmplitude > uncertaintyAmplitudeLastUpdate) ?
													uncertaintySettings.m_IncreasingSpringConstant : uncertaintySettings.m_DecreasingSpringConstant;
	const float uncertaintyAmplitude			= m_CameraOperatorUncertaintySpring.Update(desiredUncertaintyAmplitude, uncertaintySpringConstant, uncertaintySettings.m_SpringDampingRatio, true);
	PF_SET(desiredAmpl, desiredUncertaintyAmplitude);
	PF_SET(dampedAmpl, uncertaintyAmplitude);
	PF_SET(fullAmpl, 1.0f);

	//get ratio between heading and pitch
	const float totalSpeed						= Abs(headingSpeed) + Abs(pitchSpeed);

	const float headingRatioLastUpdate			= m_CameraOperatorHeadingRatioSpring.GetResult();
	float desiredHeadingRatio					= (m_NumUpdatesPerformed == 0) ? 0.5f :
													(totalSpeed > uncertaintySettings.m_HeadingAndPitchThreshold) ? (Abs(headingSpeed) / totalSpeed) : headingRatioLastUpdate;
	desiredHeadingRatio							= Clamp(desiredHeadingRatio, 0.0f, 1.0f);

	const float pitchRatioLastUpdate			= m_CameraOperatorPitchRatioSpring.GetResult();
	float desiredPitchRatio						= (m_NumUpdatesPerformed == 0) ? 0.5f :
													(totalSpeed > uncertaintySettings.m_HeadingAndPitchThreshold) ? (Abs(pitchSpeed) / totalSpeed) : pitchRatioLastUpdate;
	desiredPitchRatio							= Clamp(desiredPitchRatio, 0.0f, 1.0f);

	const float ratioSpringConstant				= shouldCutSpring ? 0.0f : uncertaintySettings.m_RatioSpringConstant;
	
	const float headingRatio					= m_CameraOperatorHeadingRatioSpring.Update(desiredHeadingRatio, ratioSpringConstant, uncertaintySettings.m_RatioSpringDampingRatio, true);
	const float pitchRatio						= m_CameraOperatorPitchRatioSpring.Update(desiredPitchRatio, ratioSpringConstant, uncertaintySettings.m_RatioSpringDampingRatio, true);
	PF_SET(headRatio, headingRatio);

	//apply ratio to each axis's amplitude
	xUncertaintyAmplitude			= uncertaintyAmplitude * pitchRatio;
	zUncertaintyAmplitude			= uncertaintyAmplitude * headingRatio;

#if __BANK
	if (ms_DebugSetShakeRatio)
	{
		xUncertaintyAmplitude		= uncertaintyAmplitude * (1.0f - ms_DebugHeadingRatio);
		zUncertaintyAmplitude		= uncertaintyAmplitude * ms_DebugHeadingRatio;
	}
#endif // __BANK

	//Turbulence shake
	const camCinematicCameraOperatorShakeTurbulenceSettings& turbulenceSettings = m_Metadata.m_CameraOperatorShakeSettings.m_TurbulenceSettings;

	const Vector3 cameraPosition	= m_Frame.GetPosition();
	const Vector3 lookAtPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
	const float distanceToTarget	= cameraPosition.Dist(lookAtPosition);
	float distanceScale				= RampValueSafe(distanceToTarget, turbulenceSettings.m_DistanceRange.x, turbulenceSettings.m_DistanceRange.y, 1.0f, 0.0f);
	distanceScale					= SlowOut(distanceScale);

	const CVehicle* targetVehicle	= m_LookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_LookAtTarget.Get()) : NULL;
	const Vector3 vehicleVelocity	= targetVehicle ? targetVehicle->GetVelocity() : VEC3_ZERO;
	const float vehicleSpeed		= vehicleVelocity.Mag();
	const float speedScale			= RampValueSafe(vehicleSpeed, turbulenceSettings.m_AbsVehicleSpeedRange.x, turbulenceSettings.m_AbsVehicleSpeedRange.y, 0.0f, 1.0f);

	float desiredTurbulenceAmplitude = distanceScale * speedScale;

#if __BANK
	if (ms_DebugSetTurbulenceShakeAmplitude)
	{
		desiredTurbulenceAmplitude	= ms_DebugTurbulenceVehicleScale * ms_DebugTurbulenceDistanceScale;
	}
#endif // __BANK

	const float turbulenceAmplitudeLastUpdate	= m_CameraOperatorTurbulenceSpring.GetResult();
	const float turbulenceSpringConstant		= shouldCutSpring ? 0.0f :
													(desiredTurbulenceAmplitude > turbulenceAmplitudeLastUpdate) ?
													turbulenceSettings.m_IncreasingSpringConstant : turbulenceSettings.m_DecreasingSpringConstant;
	turbulenceAmplitude							= m_CameraOperatorTurbulenceSpring.Update(desiredTurbulenceAmplitude, turbulenceSpringConstant, turbulenceSettings.m_SpringDampingRatio, true);
	PF_SET(desiredTurbulenceAmpl, desiredTurbulenceAmplitude);
	PF_SET(dampedTurbulenceAmpl, turbulenceAmplitude);
}

bool camCinematicCamManCamera::IsValid() 
{ 
	return m_CanUpdate;
}

void camCinematicCamManCamera::LookAt(const CEntity* target)
{
	if(target != m_LookAtTarget.Get())
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	camBaseCamera::LookAt(target);

	if(target)
	{
		m_HasAcquiredTarget = true;
	}
}

#if __BANK
void camCinematicCamManCamera::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Cinematic Camera Man camera");
	{
		bank.AddToggle("Force uncertainty shake amplitude", &ms_DebugSetUncertaintyShakeAmplitude);
		bank.AddSlider("Uncertainty shake amplitude", &ms_DebugUncertaintyShakeAmplitude, 0.0f, 1.0f, 0.01f);
		bank.AddToggle("Force shake ratio", &ms_DebugSetShakeRatio);
		bank.AddSlider("Shake ratio", &ms_DebugHeadingRatio, 0.0f, 1.0f, 0.01f);
		bank.AddToggle("Force turbulence shake amplitude", &ms_DebugSetTurbulenceShakeAmplitude);
		bank.AddSlider("Turbulence shake vehicle scale", &ms_DebugTurbulenceVehicleScale, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Turbulence distance scale", &ms_DebugTurbulenceDistanceScale, 0.0f, 1.0f, 0.01f);
	}
	bank.PopGroup(); //Cinematic Camera Man camera
}
#endif // __BANK
