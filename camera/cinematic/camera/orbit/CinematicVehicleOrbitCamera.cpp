//
// camera/cinematic/CinematicVehicleOrbitCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "CinematicVehicleOrbitCamera.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/cinematicdirector.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/Collision.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "fwmaths/random.h"
#include "peds/ped.h"
#include "scene/Entity.h"
#include "vehicles/vehicle.h"
#include "vehicles/trailer.h"

INSTANTIATE_RTTI_CLASS(camCinematicVehicleOrbitCamera,0x34D973ED)
INSTANTIATE_RTTI_CLASS(camCinematicVehicleLowOrbitCamera,0xDEB1CCC5)

CAMERA_OPTIMISATIONS()

camCinematicVehicleOrbitCamera::camCinematicVehicleOrbitCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicVehicleOrbitCameraMetadata&>(metadata))
, m_orbitDistance(0.0f)
, m_desiredOrbitAngle(0.0f)
, m_intialOrbitAngle(0.0f) 
, m_shotStartTime(0.0f)
, m_shotTotalDuration(0.0f)
, m_Pitch(0.0f)
, m_fov(45.0f)
, m_hitLastFrame(1.0f)
, m_hitDeltaThisFrame(0.0f)
, m_ShotTimeSpentOccluded(0.0f)
, m_TimeAttachParentPitchIsOutOfLimits(0.0f)
, m_shotId(0)
, m_Direction(0)
, m_canUpdate(false)
{
}

void camCinematicVehicleOrbitCamera::Init()
{
	const u32 shotTypeOffset = fwRandom::GetRandomNumberInRange( 1, m_Metadata.m_InitialCameraSettingsList.GetCount());
	m_shotId = ( m_shotId + shotTypeOffset ) % m_Metadata.m_InitialCameraSettingsList.GetCount();

	//initialise the heading angle and the delta angle
	m_intialOrbitAngle = m_Metadata.m_InitialCameraSettingsList[m_shotId].m_Heading * DtoR; 
	
	m_intialOrbitAngle = fwAngle::LimitRadianAngle(m_intialOrbitAngle); 

	m_desiredOrbitAngle = m_Metadata.m_InitialCameraSettingsList[m_shotId].m_HeadingDelta * DtoR;

	m_desiredOrbitAngle += m_intialOrbitAngle; 

	m_desiredOrbitAngle = fwAngle::LimitRadianAngle(m_desiredOrbitAngle); 

	//randomise the direction
	m_Direction = fwRandom::GetRandomNumberInRange(0, 2);	

	//pitch
	m_Pitch = m_Metadata.m_PitchLimits.x * DtoR; //fwRandom::GetRandomNumberInRange( m_Metadata.m_PitchLimits.x , m_Metadata.m_PitchLimits.y); 

	//compute the orbit distance based on the pitch and the targets bounds.
	//NOTE: We explicitly query the third-person orbit distance for the medium third-person view mode, for consistency.
	float distance;
	if (camInterface::GetGameplayDirector().IsUsingVehicleTurret(true))
	{
		if(cameraVerifyf(camInterface::GetGameplayDirector().GetThirdPersonAimCamera(), "Could not get a ThirdPersonAim camera for cinematic vehicle orbit camera distance"))
		{
			camInterface::GetGameplayDirector().GetThirdPersonAimCamera()->ComputeDesiredOrbitDistanceLimits(m_orbitDistance, distance, m_Pitch, true, true,
				(s32)camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM);
		}
	}
	else
	{
		if(cameraVerifyf(camInterface::GetGameplayDirector().GetFollowVehicleCamera(), "Could not get a FollowVehicle camera for cinematic vehicle orbit camera distance"))
		{
			camInterface::GetGameplayDirector().GetFollowVehicleCamera()->ComputeDesiredOrbitDistanceLimits(m_orbitDistance, distance, m_Pitch, true, true,
				(s32)camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM);
		}
	}

	//fov
	m_fov = m_Metadata.m_FovLimits.x;
	
	//time
	m_shotStartTime = (float)fwTimer::GetTimeInMilliseconds(); 

	m_shotTotalDuration = fwRandom::GetRandomNumberInRange( m_Metadata.m_ShotDurationLimits.x , m_Metadata.m_ShotDurationLimits.y); 
}

bool camCinematicVehicleOrbitCamera::IsValid()
{
	return m_canUpdate; 
}

bool camCinematicVehicleOrbitCamera::ShouldUpdate()
{
	if(m_NumUpdatesPerformed > 0)	
	{
		if(!m_canUpdate)
		{
			return false; 
		}
	}

	if(!m_AttachParent)
	{
		m_canUpdate = false; 
		return false; 
	}

	if((!camInterface::GetGameplayDirector().GetFollowVehicleCamera() && !camInterface::GetGameplayDirector().IsUsingVehicleTurret())
		|| (!camInterface::GetGameplayDirector().GetThirdPersonAimCamera() && camInterface::GetGameplayDirector().IsUsingVehicleTurret()))
	{
		m_canUpdate = false; 
		return false; 
	}

	return true; 
}

bool camCinematicVehicleOrbitCamera::Update()
{
	if(!ShouldUpdate())
	{
		return false; 
	}

	if(m_NumUpdatesPerformed == 0 && m_canUpdate == false)
	{
		Init(); 
	}

	if(UpdateShot())
	{
		m_canUpdate = true; 
	}
	else
	{
		m_canUpdate = false; 
	}

	return m_canUpdate; 
}

bool camCinematicVehicleOrbitCamera::UpdateShot()
{	
	if(m_AttachParent && (camInterface::GetGameplayDirector().GetFollowVehicleCamera() || camInterface::GetGameplayDirector().IsUsingVehicleTurret(true)))
	{
		const Vector3& lookAtPosition = camInterface::GetGameplayDirector().IsUsingVehicleTurret(true) ? camInterface::GetGameplayDirector().GetThirdPersonAimCamera()->GetBasePivotPosition() : camInterface::GetGameplayDirector().GetFollowVehicleCamera()->GetBasePivotPosition(); 

		float desiredHeading = ComputeDesiredHeading(); 
		
		//compute the front vector from the heading 
		Vector3 front = VEC3_ZERO;

		camFrame::ComputeFrontFromHeadingAndPitch(desiredHeading, m_Pitch, front); 

		Vector3 cameraPosition = lookAtPosition - (front * m_orbitDistance); 
		
		if(m_NumUpdatesPerformed == 0)
		{	
			if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, cameraPosition))
			{
				m_canUpdate = false; 
				return m_canUpdate;
			}
		}


		if(!UpdateCollision(cameraPosition, lookAtPosition))
		{
			return false;  
		}
		
		if(!UpdateOcclusion(cameraPosition, lookAtPosition))
		{
			return false; 
		}
		
		if(IsClipping(cameraPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
		{
			return false; 
		}

		m_Frame.SetWorldMatrixFromFront(front);

		m_Frame.SetPosition(cameraPosition); 

		m_Frame.SetFov(m_fov); 

		if(!IsAttachParentPitchSuitable())
		{
			return false; 
		}

		if(((float)(fwTimer::GetTimeInMilliseconds())) - m_shotStartTime > m_shotTotalDuration)
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

bool camCinematicVehicleOrbitCamera::IsAttachParentPitchSuitable()
{
	bool isPitchInBounds = true; 
	if(m_AttachParent && m_AttachParent->GetIsTypeVehicle())
	{
		float pitch = abs(m_AttachParent->GetTransform().GetPitch()*RtoD);

		if(m_NumUpdatesPerformed == 0)
		{
			if(pitch > m_Metadata.m_MaxAttachParentPitch)
			{
				isPitchInBounds = false;
			}
		}
		else
		{	
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 
			
			if(pitch > m_Metadata.m_MaxAttachParentPitch && pVehicle->HasContactWheels())
			{
				m_TimeAttachParentPitchIsOutOfLimits += fwTimer::GetCamTimeInMilliseconds() - fwTimer::GetCamPrevElapsedTimeInMilliseconds();  
			}
			else
			{
				m_TimeAttachParentPitchIsOutOfLimits = 0;
			}

			if(m_TimeAttachParentPitchIsOutOfLimits > m_Metadata.m_MaxTimeAttachParentPitched)
			{
				isPitchInBounds = false;
			}
		}
	}
	return isPitchInBounds; 
}

float camCinematicVehicleOrbitCamera::ComputeDesiredHeading()
{
	float desiredHeading = 0.0f; 
	
	if(m_AttachParent)	
	{
		float vehicleHeading = m_AttachParent->GetTransform().GetHeading();  

		float currentTime = (float)(fwTimer::GetTimeInMilliseconds()); 

		float shotPhase = (currentTime - m_shotStartTime) / m_shotTotalDuration; 

		//reverse the phase which reveres the direction
		if(m_Direction == 0)
		{
			shotPhase = 1.0f - shotPhase; 
		}

		shotPhase = Clamp(shotPhase, 0.0f , 1.0f); 

		desiredHeading = fwAngle::LerpTowards(m_intialOrbitAngle + vehicleHeading, m_desiredOrbitAngle + vehicleHeading, shotPhase);

		desiredHeading = fwAngle::LimitRadianAngle(desiredHeading); 
	}
	
	return desiredHeading; 
}

bool camCinematicVehicleOrbitCamera::UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	bool haveLineOfSightToLookAt = true; 
	
	const bool hasHit = ComputeOcclusion(cameraPosition, lookAtPosition); 

	if(hasHit)
	{
		m_ShotTimeSpentOccluded += fwTimer::GetCamTimeInMilliseconds() - fwTimer::GetCamPrevElapsedTimeInMilliseconds();  
	}
	else
	{
		m_ShotTimeSpentOccluded = 0;
	}

	if(m_ShotTimeSpentOccluded > m_Metadata.m_MaxTimeToSpendOccluded)
	{
		haveLineOfSightToLookAt = false;
	}

	return haveLineOfSightToLookAt; 
}

bool camCinematicVehicleOrbitCamera::UpdateCollision(Vector3& cameraDesiredPosition, const Vector3& lookAtPosition)
{
	bool shouldUpdateThisFrame = true; 
	
	//Compute collision against all entites first time around only update if the camera is not constrained in any way.
	if(m_NumUpdatesPerformed == 0)
	{
		if(ComputeCollision(cameraDesiredPosition, lookAtPosition, camCollision::GetCollisionIncludeFlags()))
		{
			shouldUpdateThisFrame = false; 
		}
	}
	else
	{
		ComputeCollision(cameraDesiredPosition, lookAtPosition, ArchetypeFlags::GTA_MAP_TYPE_MOVER);

		//terminate if the hit delta is too large in one frame or the  camera is getting too close.
		if(abs(m_hitDeltaThisFrame > m_Metadata.m_MaxLerpDeltaBeforeTerminate) || lookAtPosition.Dist(cameraDesiredPosition) <  m_orbitDistance * m_Metadata.m_PercentageOfOrbitDistanceToTerminate  )
		{
			shouldUpdateThisFrame = false;  
		}
	}

	return shouldUpdateThisFrame; 
}

bool camCinematicVehicleOrbitCamera::ComputeCollision(Vector3& cameraPosition, const Vector3&  lookAtPosition, const u32 flags)
{
	const u32 numberOfEntitesToExclude = 2; 

	Vector3 desiredCameraPosition; 
	bool hasHit = false;

	const CPed* pPed = camInterface::FindFollowPed(); 

	if(pPed && m_AttachParent)
	{
		const CEntity* excludeInstances[numberOfEntitesToExclude];

		excludeInstances[0] = pPed; 
		excludeInstances[1] = m_AttachParent.Get(); 

		// Constrain the camera against the world
		WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
		WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

		constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
		constrainCapsuleTest.SetIncludeFlags(flags);
		constrainCapsuleTest.SetIsDirected(true);
		constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
		constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
		constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);
		constrainCapsuleTest.SetExcludeEntities(excludeInstances, numberOfEntitesToExclude, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 

		constrainCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForConstrain);

		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
		const float hitTValue = hasHit ? constrainShapeTestResults[0].GetHitTValue() : 1.0f;

		m_hitDeltaThisFrame = m_hitLastFrame - hitTValue; 

		cameraPosition = Lerp(hitTValue, lookAtPosition, cameraPosition);

		m_hitLastFrame = hitTValue; 
	}
	return hasHit; 
}

bool camCinematicVehicleOrbitCamera::IsClipping(const Vector3& position, const u32 flags)
{
	float  waterHeight; 

	if(camCollision::ComputeWaterHeightAtPosition(position, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight, m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(position.z <  waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true; 
		}
	}

	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
	sphereTest.SetIncludeFlags(flags); 
	//sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	sphereTest.SetSphere(position, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForClippingTest);

	return WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
}

bool camCinematicVehicleOrbitCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	const u32 numberOfEntitesToExclude = 2; 
	
	const CPed* pPed = camInterface::FindFollowPed(); 
	bool hasHit = false; 
	if(pPed)
	{
		const CEntity* excludeInstances[numberOfEntitesToExclude];
		
		excludeInstances[0] = pPed; 
		excludeInstances[1] = m_AttachParent.Get(); 

		WorldProbe::CShapeTestCapsuleDesc occluderCapsuleTest;
		WorldProbe::CShapeTestFixedResults<> occluderShapeTestResults;
		occluderCapsuleTest.SetResultsStructure(&occluderShapeTestResults);
		//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
		occluderCapsuleTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
		occluderCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
		occluderCapsuleTest.SetContext(WorldProbe::LOS_Camera);
		occluderCapsuleTest.SetExcludeEntities(excludeInstances, numberOfEntitesToExclude, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 

		occluderCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForOcclusionTest);
		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(occluderCapsuleTest);
	}
	return hasHit; 
}

//*********************************************************************************************************

camCinematicVehicleLowOrbitCamera::camCinematicVehicleLowOrbitCamera(const camBaseObjectMetadata& metadata)
	: camBaseCamera(metadata)
	, m_Metadata(static_cast<const camCinematicVehicleLowOrbitCameraMetadata&>(metadata))
	, m_orbitDistance(0.0f)
	, m_intialOrbitAngle(0.0f) 
	, m_Pitch(0.0f)
	, m_fov(45.0f)
	, m_hitLastFrame(1.0f)
	, m_hitDeltaThisFrame(0.0f)
	, m_ShotTimeSpentOccluded(0.0f)
	, m_TimeAttachParentPitchIsOutOfLimits(0.0f)
	, m_MinHeightAboveGround(0.0f)
	, m_LastFramPitch(0.0f)
	, m_lastFrameTrailerHeadingDelta(0.0f)
	, m_shotId(0)
	, m_Direction(0)
	, m_canUpdate(false)
	, m_HasTrailerOnInit(false)
	, m_shouldCameraLookLeft(false)
{
}

void camCinematicVehicleLowOrbitCamera::Init()
{
	const u32 shotTypeOffset = fwRandom::GetRandomNumberInRange( 1, m_Metadata.m_InitialCameraSettingsList.GetCount());
	m_shotId = ( m_shotId + shotTypeOffset ) % m_Metadata.m_InitialCameraSettingsList.GetCount();

	//initialise the heading angle and the delta angle
	m_intialOrbitAngle = m_Metadata.m_InitialCameraSettingsList[m_shotId].m_Heading * DtoR; 

	//decide which side of the vehicle to look at, thise will need to be extended if looking backwards
	if((m_intialOrbitAngle >= 0.0f  && m_intialOrbitAngle <= (90.0f * DtoR)))
	{
		m_shouldCameraLookLeft = true; 
	}

	m_intialOrbitAngle = fwAngle::LimitRadianAngle(m_intialOrbitAngle); 

	//pitch
	m_Pitch = m_Metadata.m_PitchLimits.x * DtoR;  

	//fov
	m_fov = m_Metadata.m_FovLimits.x;

	if(camInterface::GetGameplayDirector().GetAttachedTrailer() || camInterface::GetGameplayDirector().GetTowedVehicle())
	{
		m_HasTrailerOnInit = true; 
	}
}

bool camCinematicVehicleLowOrbitCamera::IsValid()
{
	return m_canUpdate; 
}

bool camCinematicVehicleLowOrbitCamera::ShouldUpdate()
{
	if(m_NumUpdatesPerformed > 0)	
	{
		if(!m_canUpdate)
		{
			return false; 
		}
	}

	if(!m_AttachParent)
	{
		m_canUpdate = false; 
		return false; 
	}

	if(!camInterface::GetGameplayDirector().GetFollowVehicleCamera())
	{
		m_canUpdate = false; 
		return false; 
	}

	return true; 
}

bool camCinematicVehicleLowOrbitCamera::Update()
{
	if(!ShouldUpdate())
	{
		return false; 
	}

	if(m_NumUpdatesPerformed == 0 && m_canUpdate == false)
	{
		Init(); 
	}

	if(UpdateShot())
	{
		m_canUpdate = true; 
	}
	else
	{
		m_canUpdate = false; 
	}
	
	return m_canUpdate; 
}

bool camCinematicVehicleLowOrbitCamera::UpdateCameraPositionRelativeToGround(Vector3 &CamPos)
{
	bool hasHit = false; 
	bool isConstrainedToGround = false; 

	Vector3 UpTestStartPos = CamPos; 
	Vector3 UpTestEndPos = CamPos; 
	UpTestEndPos.z = CamPos.z + m_Metadata.m_MaxProbeHeightForGround;  

	WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	constrainCapsuleTest.SetIsDirected(true);
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);

	constrainCapsuleTest.SetCapsule(UpTestStartPos, UpTestEndPos, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForGroundTest);

	///grcDebugDraw::Line(UpTestStartPos, UpTestEndPos, Color_red);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
	float hitTValue = hasHit ? constrainShapeTestResults[0].GetHitTValue() : 1.0f;

	if(hasHit)
	{
		UpTestEndPos.z = CamPos.z + (m_Metadata.m_MaxProbeHeightForGround * hitTValue); 
	}

	UpTestStartPos.z -= m_MinHeightAboveGround;  

	constrainShapeTestResults.Reset(); 
	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	constrainCapsuleTest.SetIsDirected(true);
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);

	constrainCapsuleTest.SetCapsule(UpTestEndPos, UpTestStartPos, (m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForGroundTest) - camCollision::GetMinRadiusReductionBetweenInterativeShapetests());

	//grcDebugDraw::Line(UpTestStartPos, UpTestEndPos, Color_blue);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);

	if(hasHit)
	{
		if(CamPos.z < constrainShapeTestResults[0].GetHitPosition().z + m_MinHeightAboveGround)
		{
			CamPos.z = constrainShapeTestResults[0].GetHitPosition().z + m_MinHeightAboveGround; 
			isConstrainedToGround = true; 
		}
		//grcDebugDraw::Sphere(constrainShapeTestResults[0].GetHitPosition(), 0.3f, Color_red);
	}

	return isConstrainedToGround; 
}

void camCinematicVehicleLowOrbitCamera::UpdateLookAt(const Vector3& cameraPosition)
{
	const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

	Vector3 BoundBoxMin = vehicle->GetBaseModelInfo() ? vehicle->GetBaseModelInfo()->GetBoundingBoxMin() : vehicle->GetBoundingBoxMin();
	BoundBoxMin.z = 0.0f; 

	Vector3 WorldPos; 

	Vector3 CornerOfLookAtTarget = VEC3_ZERO; 

	float FovRatioToApply = m_Metadata.m_FovRatioToApplyForLookAt; 

	if(!m_shouldCameraLookLeft)
	{
		BoundBoxMin.x *= -1.0f; 
		FovRatioToApply *= -1.0f; 
	}

	Matrix34 VehicleNormalMat; 	

	Matrix34 VehicleMat = MAT34V_TO_MATRIX34(m_AttachParent->GetTransform().GetMatrix()); 

	float vehicleHeading = camFrame::ComputeHeadingFromMatrix(VehicleMat); 

	Vector3 HeadingFront; 

	//compute the word matrix of the vehicle with out any roll or pitch
	camFrame::ComputeFrontFromHeadingAndPitch(vehicleHeading, 0.0f, HeadingFront); 

	camFrame::ComputeWorldMatrixFromFront(HeadingFront, VehicleNormalMat); 

	VehicleNormalMat.d = MAT34V_TO_MATRIX34(vehicle->GetMatrix()).d; 

	//compute the look at in world space
	VehicleNormalMat.Transform(BoundBoxMin, CornerOfLookAtTarget); 

//	grcDebugDraw::Sphere(CornerOfLookAtTarget, 0.3f, Color_yellow);

	//now get the front from the camera to the vehicle
	Vector3 frontCamToVehicle =  CornerOfLookAtTarget - cameraPosition; 

	frontCamToVehicle.NormalizeSafe(YAXIS);

	Matrix34 WorldMat; 
	camFrame::ComputeWorldMatrixFromFront(frontCamToVehicle, WorldMat); 

	//Rotate the camera away from the look at point but try and keep that point on screen 

	const CViewport* viewport	= gVpMan.GetGameViewport();
	const float aspectRatio		= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	const float horizontalFov	= m_fov * aspectRatio;

	WorldMat.RotateZ(DtoR * horizontalFov * FovRatioToApply); 

	m_Frame.SetWorldMatrix(WorldMat); 

}

void camCinematicVehicleLowOrbitCamera::ComputeCameraZwithRespectToVehicle(Vector3& campos)
{
	const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

	Matrix34 VehicleMat;
	VehicleMat.Identity(); 

	VehicleMat.d = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()); 

	Vector3 VehicleWorldMin; 

	Vector3 BoundBoxMin = vehicle->GetBaseModelInfo() ? vehicle->GetBaseModelInfo()->GetBoundingBoxMin() : vehicle->GetBoundingBoxMin();
	VehicleMat.Transform(BoundBoxMin, VehicleWorldMin); 

	Vector3 BoundBoxMax = vehicle->GetBaseModelInfo() ? vehicle->GetBaseModelInfo()->GetBoundingBoxMax() : vehicle->GetBoundingBoxMax();
	float vehicleheight = BoundBoxMax.z - BoundBoxMin.z; 

	m_MinHeightAboveGround = vehicleheight * m_Metadata.m_MinHeightAboveGroundAsBoundBoundPercentage; 

	campos.z = VehicleWorldMin.z + (vehicleheight * m_Metadata.m_CamHeightAboveGroundAsBoundBoxPercentage); 
}

float camCinematicVehicleLowOrbitCamera::ComputeWorldPitchWithRespectToVehiclePosition(const Vector3& CamPos)
{
	const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

	Vector3 VehiclePos = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()); 

	Vector3 CameraToVehicle = VehiclePos - CamPos;

	CameraToVehicle.NormalizeSafe(YAXIS); 

	float pitch = camFrame::ComputePitchFromFront(CameraToVehicle); 

	return pitch; 

}

bool camCinematicVehicleLowOrbitCamera::UpdateShot()
{	
	if(m_AttachParent && camInterface::GetGameplayDirector().GetFollowVehicleCamera())
	{
		if(m_AttachParent->GetIsTypeVehicle())
		{
			const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());
			if(!IsVehicleOrientationValid(*attachVehicle))
			{
				//Displayf("IsVehicleOrientationValid");
				return false; 
			}
		}

		const CBaseModelInfo* attachParentBaseModelInfo = m_AttachParent->GetBaseModelInfo();

		Vector3 boundBoxMin = attachParentBaseModelInfo ? attachParentBaseModelInfo->GetBoundingBoxMin() : m_AttachParent->GetBoundingBoxMin();
		Vector3 boundBoxMax = attachParentBaseModelInfo ? attachParentBaseModelInfo->GetBoundingBoxMax() : m_AttachParent->GetBoundingBoxMax();

		const float boundingBoxFlatDiagonal	= (boundBoxMax - boundBoxMin).XYMag();
		const float projectedWidth			= boundingBoxFlatDiagonal * rage::Cosf(m_intialOrbitAngle);

		const CViewport* viewport			= gVpMan.GetGameViewport();
		const float aspectRatio				= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
		const float horizontalFov			= m_fov * aspectRatio;

		m_orbitDistance						= projectedWidth / (2.0f * m_Metadata.m_ProjectedVehicleWidthToScreenRatio * rage::Tanf(horizontalFov * 0.5f * DtoR));

		const Vector3& basePivotPosition = camInterface::GetGameplayDirector().GetFollowVehicleCamera()->GetBasePivotPosition(); 

		float desiredHeading = ComputeDesiredHeading(); 

		if(m_NumUpdatesPerformed > 0)
		{
			//Ensure that we blend to the desired orientation over the shortest angle.
			const float dampedHeadingOnPreviousUpdate = m_OrbitHeadingSpring.GetResult();
			const float desiredOrbitHeadingDelta = desiredHeading - dampedHeadingOnPreviousUpdate;
			if(desiredOrbitHeadingDelta > PI)
			{
				desiredHeading -= TWO_PI;
			}
			else if(desiredOrbitHeadingDelta < -PI)
			{
				desiredHeading += TWO_PI;
			}
		}

		const float headingSpringConstant = (m_NumUpdatesPerformed == 0) ? 0.0f : m_Metadata.m_OrbitHeadingSpringConstant;
		float headingToApply = m_OrbitHeadingSpring.Update(desiredHeading, headingSpringConstant, m_Metadata.m_OrbitHeadingSpringDampingRatio);
		headingToApply = fwAngle::LimitRadianAngle(headingToApply);
		m_OrbitHeadingSpring.OverrideResult(headingToApply);

		//compute the front vector from the heading 
		Vector3 front;
		camFrame::ComputeFrontFromHeadingAndPitch(headingToApply, m_Pitch, front); 

		Vector3 Offset = VEC3_ZERO; 

		Vector3 cameraPosition = basePivotPosition - (front * m_orbitDistance); 

		if(m_NumUpdatesPerformed == 0)
		{	
			if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, cameraPosition))
			{
				//Displayf("IsCameraPositionValidWithRespectToTheLine"); 
				m_canUpdate = false; 
				return m_canUpdate;
			}
		}

		//compute a percentage of the bounding box height to position the camera as well as the minimum height above the ground.
		ComputeCameraZwithRespectToVehicle(cameraPosition); 

		//get world pitch between camera and vehicle
		float prefixUpPitch = ComputeWorldPitchWithRespectToVehiclePosition(cameraPosition) * RtoD; 

		Vector3 camPosFixedUp = cameraPosition;

		//update the camera position if being constrained 
		UpdateCameraPositionRelativeToGround(camPosFixedUp);
		
		//Now get the same pith post fixup.
		float postfixUpPitch = ComputeWorldPitchWithRespectToVehiclePosition(camPosFixedUp) * RtoD;

		const float parentZ = m_AttachParent->GetTransform().GetPosition().GetZf();

		//if the fixed up pitch and not fixed up pitch
		if((m_LastFramPitch - postfixUpPitch)  < m_Metadata.m_MaxPitchDelta)
		{
			const float desiredRelativeFixedUpZ	= camPosFixedUp.z - parentZ;
			const float springConstant			= (m_NumUpdatesPerformed == 0) ? 0.0f : ((desiredRelativeFixedUpZ >= m_CameraPosZSpring.GetResult()) ?
													m_Metadata.m_HighGroundCollisionSpringConst : m_Metadata.m_LowGroundCollisionSpringConst);
			const float relativeFixedUpZToApply	= m_CameraPosZSpring.Update(desiredRelativeFixedUpZ, springConstant);

			cameraPosition.z	= parentZ + relativeFixedUpZToApply;
			m_LastFramPitch		= postfixUpPitch;
		}
		else if(m_NumUpdatesPerformed == 0)
		{
			return false;
		}
		else
		{
			const float relativePreFixUpZ = cameraPosition.z - parentZ;
			m_CameraPosZSpring.Reset(relativePreFixUpZ); 
			m_LastFramPitch = prefixUpPitch; 
		}

		UpdateLookAt(cameraPosition); 

		if(!UpdateCollision(cameraPosition, basePivotPosition))
		{
			//Displayf("UpdateCollision"); 
			return false;  
		}

		m_Frame.SetPosition(cameraPosition); 

		m_Frame.SetFov(m_fov); 


		if(!UpdateOcclusion(cameraPosition, basePivotPosition))
		{
			//Displayf("UpdateOcclusion"); 
			return false; 
		}

		if(IsClipping(cameraPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
		{
			//Displayf("IsClipping");
			return false; 
		}

		if(ShouldTerminateShotEarly())
		{
			//Displayf("ShouldTerminateShotEarly");
			return false; 
		}

		if(m_AttachParent->GetIsPhysical())
		{
			const CPhysical* attachParentPhysical = static_cast<const CPhysical*>(m_AttachParent.Get());
			UpdateSpeedRelativeShake(*attachParentPhysical, m_Metadata.m_InaccuracyShakeSettings, m_InaccuracyShakeSpring);
			UpdateSpeedRelativeShake(*attachParentPhysical, m_Metadata.m_HighSpeedShakeSettings, m_HighSpeedShakeSpring);
		}
	}
	else
	{
		//Displayf("no attach parent or follow vehicle camera");
		return false; 
	}

	return true; 
}

bool camCinematicVehicleLowOrbitCamera::ShouldTerminateForChangeOfTrailerStatus()
{
	if(!m_ShouldTerminateForTrailerChange)	
	{
		bool haveAttachedTrailer = false; 

		const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

		//if(camInterface::GetGameplayDirector().GetAttachedTrailer() || camInterface::GetGameplayDirector().GetTowedVehicle())
		if(vehicle->GetAttachedTrailer() || vehicle->GetEntityBeingTowed()) 
		{
			haveAttachedTrailer= true; 
		}

		if( m_HasTrailerOnInit != haveAttachedTrailer)
		{
			m_ShouldTerminateForTrailerChange =  true; 
		}
	}
	
	return m_ShouldTerminateForTrailerChange; 
}

bool camCinematicVehicleLowOrbitCamera::ShouldTerminateShotEarly()
{
	bool shouldTerminateEarly = false; 
	
	if(ShouldTerminateForChangeOfTrailerStatus() || ShouldTerminateForReversing())
	{
		m_TimeToTerminateShotEarly += fwTimer::GetCamTimeStepInMilliseconds();  
	}
	else
	{
		m_TimeToTerminateShotEarly = 0;
	}

	if(m_TimeToTerminateShotEarly > m_Metadata.m_MaxTimeToSpendOccluded)
	{
		shouldTerminateEarly = true;
	}

	return shouldTerminateEarly; 
}

bool camCinematicVehicleLowOrbitCamera::ShouldTerminateForReversing()
{
	bool shouldTerminateForReversing = false; 

	const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

	const Vector3& attachParentVelocity	= vehicle->GetVelocity();
	Vector3 attachParentFront			= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetForward());
	float attachParentSpeedToConsider	= attachParentVelocity.Dot(attachParentFront);

	if(m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* attachVehicle		= static_cast<const CVehicle*>(m_AttachParent.Get());
		const float attachVehicleThrottle	= attachVehicle->GetThrottle();
		if(attachVehicleThrottle < -SMALL_FLOAT && attachParentSpeedToConsider < 0.0f)
		{
			shouldTerminateForReversing = true;  
		}
		else
		{
			shouldTerminateForReversing = false; 
		}
	}

	return shouldTerminateForReversing; 
}


bool camCinematicVehicleLowOrbitCamera::IsVehicleOrientationValid(const CVehicle& vehicle)
{
	Matrix34 VehicleMat = MAT34V_TO_MATRIX34(vehicle.GetTransform().GetMatrix()); 
	float heading;
	float pitch;
	float roll;
	camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(VehicleMat, heading, pitch, roll);

	const bool isValid = (Abs(pitch) <= (m_Metadata.m_MaxPitch * DtoR)) && (Abs(roll) <= (m_Metadata.m_MaxRoll * DtoR));

	return isValid; 
}


float camCinematicVehicleLowOrbitCamera::ComputeDesiredHeading()
{
	float desiredHeading = 0.0f; 

	if(m_AttachParent)	
	{
		//float vehicleHeading = m_AttachParent->GetTransform().GetHeading();  
		const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 
		
		float TrailerHeading = 0.0f; 

		if((vehicle->GetAttachedTrailer() || vehicle->GetEntityBeingTowed()) && !ShouldTerminateForChangeOfTrailerStatus() )
		{
						
			Matrix34 VehicleMat;
			if(vehicle->GetAttachedTrailer())
			{
				VehicleMat = MAT34V_TO_MATRIX34(vehicle->GetAttachedTrailer()->GetTransform().GetMatrix()); 
			}
			else
			{
				VehicleMat = MAT34V_TO_MATRIX34(vehicle->GetEntityBeingTowed()->GetTransform().GetMatrix()); 
			}

			TrailerHeading = camFrame::ComputeHeadingFromMatrix(VehicleMat); 

			Matrix34 ParentMat;
			ParentMat = MAT34V_TO_MATRIX34(m_AttachParent->GetTransform().GetMatrix()); 
			float vehicleHeading = camFrame::ComputeHeadingFromMatrix(ParentMat); 

			float HeadingDelta =  vehicleHeading - TrailerHeading; 
	
			if(HeadingDelta > PI)
			{
				HeadingDelta -= TWO_PI;
			}
			else if(HeadingDelta < -PI)
			{
				HeadingDelta += TWO_PI;
			}
			
			bool ApplySmoothing = false; 

			if(m_shouldCameraLookLeft)
			{
				if(HeadingDelta > 0.0f)
				{
					ApplySmoothing = true; 
				}
			}
			else
			{
				if(HeadingDelta < 0.0f)
				{
					ApplySmoothing = true; 
				}
			}

			if(ApplySmoothing)
			{
				float HeadingDeltaMulitplier = RampValue(abs(HeadingDelta), 0.0f, m_Metadata.m_AngleRangeToApplySmoothingForAttachedTrailer * DtoR, 1.0f, 0.0f); 

				HeadingDeltaMulitplier = SlowIn(HeadingDeltaMulitplier); 

				HeadingDelta *= HeadingDeltaMulitplier; 
			}
			
			HeadingDelta *= 0.5f; 

			m_lastFrameTrailerHeadingDelta = HeadingDelta; 

			desiredHeading = vehicleHeading + m_intialOrbitAngle - HeadingDelta ;

		}
		else
		{
			Matrix34 VehicleMat;
			VehicleMat = MAT34V_TO_MATRIX34(m_AttachParent->GetTransform().GetMatrix()); 
			float vehicleHeading = camFrame::ComputeHeadingFromMatrix(VehicleMat); 

			desiredHeading = vehicleHeading + m_intialOrbitAngle - m_lastFrameTrailerHeadingDelta;
		}
	

		desiredHeading = fwAngle::LimitRadianAngle(desiredHeading); 
	}

	return desiredHeading; 
}

bool camCinematicVehicleLowOrbitCamera::UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	bool haveLineOfSightToLookAt = true; 

	const bool hasHit = ComputeOcclusion(cameraPosition, lookAtPosition); 

	if(hasHit)
	{
		m_ShotTimeSpentOccluded += fwTimer::GetCamTimeStepInMilliseconds();  
	}
	else
	{
		m_ShotTimeSpentOccluded = 0;
	}

	if(m_ShotTimeSpentOccluded > m_Metadata.m_MaxTimeToSpendOccluded)
	{
		haveLineOfSightToLookAt = false;
	}

	return haveLineOfSightToLookAt; 
}

bool camCinematicVehicleLowOrbitCamera::UpdateCollision(Vector3& cameraDesiredPosition, const Vector3& lookAtPosition)
{
	bool shouldUpdateThisFrame = true; 

	//Compute collision against all entites first time around only update if the camera is not constrained in any way.
	if(m_NumUpdatesPerformed == 0)
	{
		if(ComputeCollision(cameraDesiredPosition, lookAtPosition, camCollision::GetCollisionIncludeFlags()))
		{
			shouldUpdateThisFrame = false; 
		}
	}
	else
	{
		Vector3 baseCameraPosition = cameraDesiredPosition; 

		ComputeCollision(cameraDesiredPosition, lookAtPosition, ArchetypeFlags::GTA_MAP_TYPE_MOVER); 

		//terminate if the hit delta is too large in one frame or the  camera is getting too close.
		if(abs(m_hitDeltaThisFrame > m_Metadata.m_MaxLerpDeltaBeforeTerminate) || lookAtPosition.Dist(cameraDesiredPosition) <  m_orbitDistance * m_Metadata.m_PercentageOfOrbitDistanceToTerminate  )
		{
			shouldUpdateThisFrame = false;  
		}
	}

	return shouldUpdateThisFrame; 
}

bool camCinematicVehicleLowOrbitCamera::ComputeCollision(Vector3& cameraPosition, const Vector3&  lookAtPosition, const u32 flags)
{
	const u32 numberOfEntitesToExclude = 3; 

	Vector3 desiredCameraPosition; 
	bool hasHit = false;

	const CPed* pPed = camInterface::FindFollowPed(); 

	if(pPed && m_AttachParent)
	{
		const CEntity* excludeInstances[numberOfEntitesToExclude];

		excludeInstances[0] = pPed; 
		excludeInstances[1] = m_AttachParent.Get(); 

		if(camInterface::GetGameplayDirector().GetAttachedTrailer())
		{
			excludeInstances[2] = camInterface::GetGameplayDirector().GetAttachedTrailer(); 
		}
		else if(camInterface::GetGameplayDirector().GetTowedVehicle())
		{
			excludeInstances[2] = camInterface::GetGameplayDirector().GetTowedVehicle(); 
		}
		else
		{
			excludeInstances[2] = NULL; 
		}
		// Constrain the camera against the world
		WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
		WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

		constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
		constrainCapsuleTest.SetIncludeFlags(flags);
		constrainCapsuleTest.SetIsDirected(true);
		constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
		constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
		constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);
		constrainCapsuleTest.SetExcludeEntities(excludeInstances, numberOfEntitesToExclude, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 

		constrainCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForConstrain);

		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
		float hitTValue = hasHit ? constrainShapeTestResults[0].GetHitTValue() : 1.0f;

		m_hitDeltaThisFrame = m_hitLastFrame - hitTValue; 

		if(GetNumUpdatesPerformed() == 0)
		{
			m_CollisionPushSpring.Reset(1.0f ); 
		}

		if(hitTValue < m_hitLastFrame)	
		{
			m_CollisionPushSpring.Reset(hitTValue); 
		}
		else
		{
			m_CollisionPushSpring.Update(hitTValue, m_Metadata.m_CollisionSpringConst, 1.0f, true); 
			hitTValue = m_CollisionPushSpring.GetResult(); 
		}

		cameraPosition = Lerp(hitTValue, lookAtPosition, cameraPosition);

		m_hitLastFrame = hitTValue; 
	}
	return hasHit; 
}

bool camCinematicVehicleLowOrbitCamera::IsClipping(const Vector3& position, const u32 flags)
{
	float  waterHeight; 

	if(camCollision::ComputeWaterHeightAtPosition(position, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight, m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(position.z <  waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true; 
		}
	}

	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
	sphereTest.SetIncludeFlags(flags); 
	//sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	sphereTest.SetSphere(position, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForClippingTest);

	const bool isClipping = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
	if(isClipping)
	{
		return true;
	}

	//If we are ignoring occlusion with a trailer or towed vehicle, we must ensure that this position is not inside a possible BVH interior space associated with this.

	const CVehicle* attachedTrailerOrTowedVehicle = camInterface::GetGameplayDirector().GetAttachedTrailer();
	if(!attachedTrailerOrTowedVehicle)
	{
		attachedTrailerOrTowedVehicle = camInterface::GetGameplayDirector().GetTowedVehicle();
	}

	if(!attachedTrailerOrTowedVehicle)
	{
		return false;
	}

	WorldProbe::CShapeTestProbeDesc probeTest;
	WorldProbe::CShapeTestFixedResults<1> probeTestResults;
	probeTest.SetResultsStructure(&probeTestResults);
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	probeTest.SetContext(WorldProbe::LOS_Camera);
	probeTest.SetIsDirected(true);

	probeTest.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	probeTest.SetIncludeEntity(attachedTrailerOrTowedVehicle, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

	Vector3 endPosition(position);
	endPosition.AddScaled(ZAXIS, -m_Metadata.m_DistanceToTestDownForTrailersOrTowedVehiclesForClippingTest);

	probeTest.SetStartAndEnd(position, endPosition);

	const bool hasHitVehicle = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);

	return hasHitVehicle;
}

bool camCinematicVehicleLowOrbitCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	const u32 numberOfEntitesToExclude = 3; 

	const CPed* pPed = camInterface::FindFollowPed(); 
	bool hasHit = false; 
	if(pPed)
	{
		const CEntity* excludeInstances[numberOfEntitesToExclude];

		excludeInstances[0] = pPed; 
		excludeInstances[1] = m_AttachParent.Get(); 

		if(camInterface::GetGameplayDirector().GetAttachedTrailer())
		{
			excludeInstances[2] = camInterface::GetGameplayDirector().GetAttachedTrailer(); 
		}
		else if(camInterface::GetGameplayDirector().GetTowedVehicle())
		{
			excludeInstances[2] = camInterface::GetGameplayDirector().GetTowedVehicle(); 
		}
		else
		{
			excludeInstances[2] = NULL; 
		}

		WorldProbe::CShapeTestCapsuleDesc occluderCapsuleTest;
		WorldProbe::CShapeTestFixedResults<> occluderShapeTestResults;
		occluderCapsuleTest.SetResultsStructure(&occluderShapeTestResults);
		//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
		occluderCapsuleTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
		occluderCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
		occluderCapsuleTest.SetContext(WorldProbe::LOS_Camera);
		occluderCapsuleTest.SetExcludeEntities(excludeInstances, numberOfEntitesToExclude, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 

		occluderCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForOcclusionTest);
		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(occluderCapsuleTest);
	}
	return hasHit; 
}

void camCinematicVehicleLowOrbitCamera::UpdateSpeedRelativeShake(const CPhysical& parentPhysical, const camSpeedRelativeShakeSettingsMetadata& settings, camDampedSpring& speedSpring)
{
	const u32 shakeHash = settings.m_ShakeRef.GetHash();
	if(shakeHash == 0)
	{
		//No shake was specified in metadata.
		return;
	}

	camBaseFrameShaker* frameShaker = FindFrameShaker(shakeHash);
	if(!frameShaker)
	{
		//Try to kick-off the specified shake.
		frameShaker = Shake(shakeHash, 0.0f);
		if(!frameShaker)
		{
			return;
		}
	}

	//Consider the forward and backward speed only.
	const Vector3 parentFront		= VEC3V_TO_VECTOR3(parentPhysical.GetTransform().GetB());
	const Vector3& parentVelocity	= parentPhysical.GetVelocity();
	float forwardBackwardSpeed		= DotProduct(parentVelocity, parentFront);
	forwardBackwardSpeed			= Abs(forwardBackwardSpeed);
	const float desiredSpeedFactor	= SlowInOut(RampValueSafe(forwardBackwardSpeed, settings.m_MinForwardSpeed, settings.m_MaxForwardSpeed, 0.0f, 1.0f));

	const float springConstant		= (m_NumUpdatesPerformed == 0) ? 0.0f : settings.m_SpringConstant;
	float speedFactorToApply		= speedSpring.Update(desiredSpeedFactor, springConstant);
	speedFactorToApply				= Clamp(speedFactorToApply, 0.0f, 1.0f); //For safety.

	frameShaker->SetAmplitude(speedFactorToApply);
}

		/*if((abs(camPosFixedUp.z) - m_LastFrameZ)  < m_Metadata.m_MaxPitchDelta)
		{
			if(!m_wasBigDeltaLastFrame)
			{
				m_CameraPosZSpring.Update(camPosFixedUp.z, springconst, springdamping, true); 

				camPosFixedUp.z = m_CameraPosZSpring.GetResult(); 

				cameraPosition = camPosFixedUp; 

				if(rage::IsNearZero(m_CameraPosZSpring.GetVelocity(), SMALL_FLOAT) &&  springconst == springconstlow)
				{
					m_shouldUpdateSpring = false; 
				}
			}
			else
			{
				m_CameraPosZSpring.Reset(cameraPosition.z); 
			}

			m_wasBigDeltaLastFrame = false; 
			
		}
		else
		{
			m_wasBigDeltaLastFrame = true; 
		}
		*/

//void camCinematicVehicleLowOrbitCamera::ScaleSpringConstWithRespectToVelocity(float &SpringConst)
//{
//	const CVehicle* vehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 
//
//	const Vector3& attachParentVelocity	= vehicle->GetVelocity();
//	Vector3 attachParentFront			= VEC3V_TO_VECTOR3(vehicle->GetTransform().GetForward());
//	float attachParentSpeedToConsider	= attachParentVelocity.Dot(attachParentFront);
//
//	TUNE_FLOAT(springdampingmaxforward, 1.05f, 0.f, 10.f, 0.1f)
//		TUNE_FLOAT(springdampingmaxback, 1.5f, 0.f, 10.f, 0.1f)
//
//	Displayf("velocity: %f", attachParentSpeedToConsider); 
//
//	if(attachParentSpeedToConsider < 0.0f)
//	{
//		SpringConst = RampValueSafe(abs(attachParentSpeedToConsider), 5.0f, 15.0f, springdampingmaxback, 1.0f); 
//	}
//	else
//	{
//		SpringConst = RampValueSafe(attachParentSpeedToConsider, 10.0f, 30.0f, springdampingmaxforward, 1.0f); 
//	}
//}