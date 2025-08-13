//
// camera/cinematic/CinematicPositionCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "CinematicPositionCamera.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/cinematicdirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/LookAheadHelper.h"
#include "camera/helpers/LookAtDampingHelper.h"
#include "camera/system/CameraMetadata.h"
#include "fwmaths/random.h"
#include "peds/ped.h"
#include "scene/Entity.h"
#include "vehicles/vehicle.h"

INSTANTIATE_RTTI_CLASS(camCinematicPositionCamera,0x27A450BB)

CAMERA_OPTIMISATIONS()



camCinematicPositionCamera::camCinematicPositionCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicPositionCameraMetadata&>(metadata))
, m_LookAtPosition(g_InvalidPosition)
, m_fov(45.0f)
, m_ShotTimeSpentOccluded(0)
, m_StartTime(fwTimer::GetCamTimeInMilliseconds())
, m_Mode(-1)
, m_InVehicleLookAtDamper(NULL)
, m_OnFootLookAtDamper(NULL)
, m_InVehicleLookAheadHelper(NULL)
, m_canUpdate(false)
, m_shouldUseAttachParent(true)
, m_ShouldStartZoomedIn(fwRandom::GetRandomTrueFalse())
, m_ShouldZoom(fwRandom::GetRandomTrueFalse())
, m_HaveComputedBaseOrbitPosition(false)
, m_CanUseDampedLookAtHelpers(false)
, m_CanUseLookAheadHelper(false)
{
}

void camCinematicPositionCamera::ComputeCamPositionRelativeToBaseOrbitPosition()
{
	//use the attach parent else use the 
	if(m_shouldUseAttachParent)
	{	
		m_intialPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition()); 
		m_intialHeading = m_AttachParent->GetTransform().GetHeading(); 
	}

	m_CamPosition = ComputePositionRelativeToAttachParent();

	const camLookAtDampingHelperMetadata* pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(m_Metadata.m_InVehicleLookAtDampingRef.GetHash());

	if(pLookAtHelperMetaData)
	{
		m_InVehicleLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_InVehicleLookAtDamper, "Cinematic Position Camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}


	pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(m_Metadata.m_OnFootLookAtDampingRef.GetHash());

	if(pLookAtHelperMetaData)
	{
		m_OnFootLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_OnFootLookAtDamper, "Cinematic Position Camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}

	const camLookAheadHelperMetadata* pLookAheadHelperMetaData = camFactory::FindObjectMetadata<camLookAheadHelperMetadata>(m_Metadata.m_InVehicleLookAheadRef.GetHash());

	if(pLookAheadHelperMetaData)
	{
		m_InVehicleLookAheadHelper = camFactory::CreateObject<camLookAheadHelper>(*pLookAheadHelperMetaData);
		cameraAssertf(m_InVehicleLookAheadHelper, "Cinematic Position Camera (name: %s, hash: %u) cannot create a look ahead helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAheadHelperMetaData->m_Name.GetCStr()), pLookAheadHelperMetaData->m_Name.GetHash());
	}
}

camCinematicPositionCamera::~camCinematicPositionCamera()
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
}

Vector3 camCinematicPositionCamera::ComputePositionRelativeToAttachParent()
{
	m_HaveComputedBaseOrbitPosition = true; 

	float desiredHeading = ComputeIntialDesiredHeading(); 

	Vector3 front = VEC3_ZERO;
	float orbitDistance = ComputeOrbitDistance(); 

	camFrame::ComputeFrontFromHeadingAndPitch(desiredHeading, m_Metadata.m_PitchLimits.x * DtoR, front); 

	Vector3 cameraPosition = m_intialPosition - (front * orbitDistance); 

	return cameraPosition;
}

void camCinematicPositionCamera::RandomiseInitialHeading()
{
	if(m_Mode > -1)
	{
		m_intialHeading = m_AttachParent->GetTransform().GetHeading();
		
		float AngleDelta = TWO_PI / (float)GetNumOfModes(); 
		
		float startSangle = AngleDelta * float(m_Mode); 

		float RandomAngle = fwRandom::GetRandomNumberInRange( 0.0f, AngleDelta);
		
		m_intialHeading += startSangle + RandomAngle; 
	}
}

float camCinematicPositionCamera::ComputeOrbitDistance()
{
	float orbitRadius = 0.0f;
	
	if(m_AttachParent)
	{
		orbitRadius = m_AttachParent->GetBaseModelInfo()->GetBoundingSphereRadius() * m_Metadata.m_OrbitDistanceScalar; 
	}

	if(orbitRadius < m_Metadata.m_MinOrbitRadius)
	{
		orbitRadius = m_Metadata.m_MinOrbitRadius; 
	}

	return orbitRadius;

}

float camCinematicPositionCamera::ComputeIntialDesiredHeading()
{
	float angle = 0.0f; 

	if(m_LookAtTarget != m_AttachParent)
	{
		Vector3 lookAtPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 

		Vector3 front = lookAtPosition - m_intialPosition; 

		front.Normalize(); 

		Vector3 parentFront = VEC3_ZERO;

		if(m_shouldUseAttachParent)
		{
			parentFront = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetB());
		}
		else
		{
			camFrame::ComputeFrontFromHeadingAndPitch(m_intialHeading, 0.0f, parentFront); 
		}

		parentFront.Normalize(); 

		angle = parentFront.AngleZ(front); 

		angle += m_intialHeading; 

	}
	else
	{
		RandomiseInitialHeading(); 
		angle += m_intialHeading; 
	}
	
	angle = fwAngle::LimitRadianAngle(angle); 
	
	return angle; 
}


bool camCinematicPositionCamera::IsValid()
{
	return m_canUpdate; 
}

bool camCinematicPositionCamera::ShouldUpdate()
{
	if(m_NumUpdatesPerformed > 0)	
	{
		if(!m_canUpdate)
		{
			return false; 
		}
	}
///check possible 
	if(m_shouldUseAttachParent && !m_AttachParent)
	{
		m_canUpdate = false; 
		return false; 
	}
	
	if(!m_LookAtTarget)
	{
		m_canUpdate = false; 
		return false; 
	}

	return true; 
}

void camCinematicPositionCamera::OverrideCamOrbitBasePosition(const Vector3& position, float heading) 
{
	m_intialPosition = position; 
	m_intialHeading = heading;
	m_shouldUseAttachParent = false; 
}

bool camCinematicPositionCamera::Update()
{
	if(!ShouldUpdate())
	{
		return m_canUpdate; 
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

bool camCinematicPositionCamera::UpdateShot()
{	
	if(!m_LookAtTarget)
	{
		return false; 
	}

	m_LookAtPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

	//push the look at position dependent on the direction of travel
	if (m_LookAtTarget->GetIsTypeVehicle() && m_CanUseLookAheadHelper && m_InVehicleLookAheadHelper)
	{
		Vector3 lookAheadPositionOffset = VEC3_ZERO;
		const CPhysical* lookAtTargetPhysical = static_cast<const CPhysical*>(m_LookAtTarget.Get());
		m_InVehicleLookAheadHelper->UpdateConsideringLookAtTarget(lookAtTargetPhysical, m_NumUpdatesPerformed == 0, lookAheadPositionOffset);
		m_LookAtPosition += lookAheadPositionOffset;
	}

	//compute the front vector from the heading 
	if(m_NumUpdatesPerformed == 0 && m_LookAtTarget)
	{	
		if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, m_CamPosition))
		{
			m_canUpdate = false; 
			return m_canUpdate;
		}
	}

	if(m_HaveComputedBaseOrbitPosition)
	{
		if(!UpdateCollision(m_CamPosition, m_intialPosition))
		{
			return false;  
		}
	}

	Vector3 targetPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
	if(!UpdateOcclusion(m_CamPosition, targetPosition))
	{
		m_canUpdate = false;
		return false; 
	}

	if(IsClipping(m_CamPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
	{
		return false; 
	}

	if(IsTargetTooFarAway(m_CamPosition, targetPosition))
	{
		return false; 
	}

	Vector3 front = VEC3_ZERO;
	front = m_LookAtPosition - m_CamPosition; 
	front.Normalize(); 

	m_Frame.SetWorldMatrixFromFront(front);

	m_Frame.SetPosition(m_CamPosition); 

	UpdateZoom();

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

	return true; 
}

bool camCinematicPositionCamera::UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	bool haveLineOfSightToLookAt = true; 
	
	bool hasHit = ComputeOcclusion(cameraPosition, lookAtPosition); 


	if(m_NumUpdatesPerformed == 0 && hasHit)
	{
		haveLineOfSightToLookAt = false;
	}

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

bool camCinematicPositionCamera::UpdateCollision(Vector3& cameraDesiredPosition, const Vector3& lookAtPosition)
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

	return shouldUpdateThisFrame; 
}

bool camCinematicPositionCamera::ComputeCollision(Vector3& cameraPosition, const Vector3&  lookAtPosition, const u32 flags)
{

	Vector3 desiredCameraPosition; 
	bool hasHit = false;


	// Constrain the camera against the world
	WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(flags);
	constrainCapsuleTest.SetIsDirected(false);
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);

	if(m_shouldUseAttachParent && m_AttachParent)
	{
		constrainCapsuleTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
	}

	constrainCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForConstrain);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
	
	return hasHit; 
}

bool camCinematicPositionCamera::IsClipping(const Vector3& position, const u32 flags)
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
	sphereTest.SetSphere(position, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForClippingTest);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	return WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
}

bool camCinematicPositionCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	const CPed* pPed = camInterface::FindFollowPed(); 
	bool hasHit = false; 
	if(pPed)
	{
		WorldProbe::CShapeTestProbeDesc occluderCapsuleTest;
		WorldProbe::CShapeTestFixedResults<> occluderShapeTestResults;
		occluderCapsuleTest.SetResultsStructure(&occluderShapeTestResults);
		//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
		occluderCapsuleTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
		occluderCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
		occluderCapsuleTest.SetContext(WorldProbe::LOS_Camera);
		if(m_LookAtTarget)
		{
			occluderCapsuleTest.SetExcludeEntity(m_LookAtTarget, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
		}
		occluderCapsuleTest.SetStartAndEnd(cameraPosition, lookAtPosition);
		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(occluderCapsuleTest);
	}
	return hasHit; 
}

bool camCinematicPositionCamera::IsTargetTooFarAway(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	float distanceToTarget = cameraPosition.Dist2(lookAtPosition); 
	bool shouldTerminate = false; 

	if(distanceToTarget > m_Metadata.m_MaxDistanceToTerminate * m_Metadata.m_MaxDistanceToTerminate)
	{
		shouldTerminate = true;
	}
	
	return shouldTerminate; 
}

void camCinematicPositionCamera::UpdateZoom()
{
	u32 time = fwTimer::GetCamTimeInMilliseconds();

	if(m_ShouldStartZoomedIn && !m_ShouldZoom && (m_ShotTimeSpentOccluded > (m_Metadata.m_MaxTimeToSpendOccluded/ 4)))
	{
		//Apply a zoom out to indicate that we lost the target.
		m_StartTime = time;
		m_ShouldZoom = true;
	}

	if(m_ShouldStartZoomedIn && (m_NumUpdatesPerformed == 0))
	{
		m_fov = m_Metadata.m_MaxZoomFov; 
	}
	else if(m_ShouldZoom)
	{
		u32 timeSinceStart = time - m_StartTime;
		float t		= (float)timeSinceStart / (float)m_Metadata.m_ZoomDuration;
		t			= Clamp(t, 0.0f, 1.0f);
		m_fov	= m_ShouldStartZoomedIn ? Lerp(SlowInOut(t), m_Metadata.m_MaxZoomFov, g_DefaultFov) :
			Lerp(SlowInOut(t), g_DefaultFov, m_Metadata.m_MaxZoomFov);
	}

	m_Frame.SetFov(m_fov);
}
