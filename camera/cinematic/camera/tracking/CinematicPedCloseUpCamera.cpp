//
// camera/cinematic/CinematicPedCloseUpCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/tracking/CinematicPedCloseUpCamera.h"

#include "camera/cinematic/cinematicdirector.h"
#include "camera/CamInterface.h"
#include "camera/helpers/Collision.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "physics/WorldProbe/worldprobe.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicPedCloseUpCamera,0xB07EF7BD)


camCinematicPedCloseUpCamera::camCinematicPedCloseUpCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicPedCloseUpCameraMetadata&>(metadata))
, m_IsFramingValid(false)
, m_ShotTimeSpentOccluded(0)
{
}

bool camCinematicPedCloseUpCamera::Update()
{
	if(!m_LookAtTarget.Get() || !m_LookAtTarget->GetIsTypePed()) //Safety check.
	{
		m_IsFramingValid = false;
		return false;
	}

	if(!m_AttachParent)
	{
		//Fall back to attaching to the look-at target.
		m_AttachParent = m_LookAtTarget;
	}
	
	Vector3 attachPosition;
	ComputeAttachPosition(attachPosition);

	Vector3 lookAtPosition;
	ComputeLookAtPosition(lookAtPosition);

	if(m_NumUpdatesPerformed == 0)
	{
		//NOTE: The camera position is locked on the initial update.
		m_IsFramingValid = UpdateCameraPosition(attachPosition, lookAtPosition);
		if(!m_IsFramingValid)
		{
			return false;
		}
	}

	if(!UpdateOcclusion(m_Frame.GetPosition(), lookAtPosition))
	{
		m_IsFramingValid = false;
		return false; 
	}
	
	if(IsTargetTooFarAway(m_Frame.GetPosition(), lookAtPosition))
	{
		m_IsFramingValid = false; 
		return false; 
	}

	m_IsFramingValid = !ComputeIsCameraClipping();
	if(!m_IsFramingValid)
	{
		return false;
	}

	UpdateCameraOrientation(attachPosition, lookAtPosition);

	m_Frame.SetFov(m_Metadata.m_BaseFov);
	m_Frame.SetNearClip(m_Metadata.m_BaseNearClip);

	return m_IsFramingValid;
}

void camCinematicPedCloseUpCamera::ComputeAttachPosition(Vector3& attachPosition) const
{
	if(m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());

		//Default to looking at the root bone.
		const eAnimBoneTag attachBoneTag = (m_Metadata.m_AttachBoneTag >= 0) ? (eAnimBoneTag)m_Metadata.m_AttachBoneTag : BONETAG_ROOT;

		Matrix34 boneMatrix;
		attachPed->GetBoneMatrix(boneMatrix, attachBoneTag);

		attachPosition = boneMatrix.d;
	}
	else
	{
		attachPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
	}

	Vector3 attachOffset(m_Metadata.m_AttachOffset);
	if(m_Metadata.m_ShouldApplyLookAtOffsetInLocalSpace)
	{
		Matrix34 attachParentMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetTransform().GetMatrix());
		attachParentMatrix.Transform3x3(attachOffset);
	}

	attachPosition += attachOffset;
}

void camCinematicPedCloseUpCamera::ComputeLookAtPosition(Vector3& lookAtPosition) const
{
	const CPed* lookAtPed = static_cast<const CPed*>(m_LookAtTarget.Get());

	//Default to looking at the root bone.
	eAnimBoneTag lookAtBoneTag = (m_Metadata.m_LookAtBoneTag >= 0) ? (eAnimBoneTag)m_Metadata.m_LookAtBoneTag : BONETAG_ROOT;

	Matrix34 boneMatrix;
	lookAtPed->GetBoneMatrix(boneMatrix, lookAtBoneTag);

	lookAtPosition = boneMatrix.d;

	Vector3 lookAtOffset(m_Metadata.m_LookAtOffset);
	if(m_Metadata.m_ShouldApplyLookAtOffsetInLocalSpace)
	{
		Matrix34 lookAtPedMatrix = MAT34V_TO_MATRIX34(lookAtPed->GetTransform().GetMatrix());
		lookAtPedMatrix.Transform3x3(lookAtOffset);
	}

	lookAtPosition += lookAtOffset;
}

bool camCinematicPedCloseUpCamera::UpdateCameraPosition(const Vector3& attachPosition, const Vector3& lookAtPosition)
{
	const CPed* followPed = camInterface::FindFollowPed();
	if(!followPed)
	{
		return false;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_RAGDOLL_TYPE);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	Vector3 lineOfAction;
	if(m_LookAtTarget != m_AttachParent)
	{
		const Vector3 attachParentPosition	= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		const Vector3 lookAtTargetPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

		lineOfAction = lookAtTargetPosition - attachParentPosition;
		lineOfAction.NormalizeSafe(YAXIS);
	}
	else if(m_LookAtTarget != followPed)
	{
		const Vector3 followPedPosition		= VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition());
		const Vector3 lookAtTargetPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

		lineOfAction = lookAtTargetPosition - followPedPosition;
		lineOfAction.NormalizeSafe(YAXIS);
	}
	else
	{
		//We are attach to and looking at the same ped, so use this ped's forward vector as the line of action.
		lineOfAction = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetForward());
	}

	const float lineOfActionHeading						= camFrame::ComputeHeadingFromFront(lineOfAction);

	const float gameplayCameraHeading					=  camInterface::GetHeading(); 
	
	const float lineOfActionToGameplayHeadingDelta		= fwAngle::LimitRadianAngle(gameplayCameraHeading - lineOfActionHeading);
	const float lineOfActionToGameplayHeadingDeltaSign	= (lineOfActionToGameplayHeadingDelta >= 0.0f) ? 1.0f : -1.0f;

	const float minLineOfActionToShotHeadingDelta		= m_Metadata.m_MinLineOfActionToShotHeadingDelta * DtoR;
	const float maxLineOfActionToShotHeadingDelta		= m_Metadata.m_MaxLineOfActionToShotHeadingDelta * DtoR;

	const float headingSweepRange		= Max(maxLineOfActionToShotHeadingDelta - minLineOfActionToShotHeadingDelta, 0.0f);
	const float headingSweepStepSize	= lineOfActionToGameplayHeadingDeltaSign * headingSweepRange /
											(float)(m_Metadata.m_NumHeadingSweepIterations - 1);

	Vector3 bestCameraPosition;
	float maxOrbitDistance = 0.0f;

	float orbitHeading	= fwAngle::LimitRadianAngle(lineOfActionHeading + (lineOfActionToGameplayHeadingDeltaSign *
							minLineOfActionToShotHeadingDelta));

	for(int i=0; i<m_Metadata.m_NumHeadingSweepIterations; i++)
	{
		//Ensure that our heading meets the minimum angle delta w.r.t. the gameplay frame.
		float orbitHeadingToApply;
		const float gameplayCameraRelativeOrbitHeading	= fwAngle::LimitRadianAngle(orbitHeading - gameplayCameraHeading);
		const float minGameplayToShotHeadingDelta		= m_Metadata.m_MinGameplayToShotHeadingDelta * DtoR;
		if(Abs(gameplayCameraRelativeOrbitHeading) < minGameplayToShotHeadingDelta)
		{
			const float gameplayCameraRelativeOrbitHeadingToApply	= (gameplayCameraRelativeOrbitHeading >= 0.0f) ?
																		minGameplayToShotHeadingDelta : -minGameplayToShotHeadingDelta;
			orbitHeadingToApply = fwAngle::LimitRadianAngle(gameplayCameraHeading + gameplayCameraRelativeOrbitHeadingToApply);
		}
		else
		{
			orbitHeadingToApply = orbitHeading;
		}

		const float orbitPitch = DtoR * fwRandom::GetRandomNumberInRange(m_Metadata.m_MinOrbitPitch, m_Metadata.m_MaxOrbitPitch);

		Vector3 orbitFront;
		camFrame::ComputeFrontFromHeadingAndPitch(orbitHeadingToApply, orbitPitch, orbitFront);

		Vector3 cameraPosition = attachPosition - (orbitFront * m_Metadata.m_IdealOrbitDistance);
		
		if(m_NumUpdatesPerformed == 0)
		{	
			if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, cameraPosition))
			{
				continue;
			}
		}

		//Constrain the camera towards the attach position under collision.
		capsuleTest.SetCapsule(attachPosition, cameraPosition, m_Metadata.m_CollisionTestRadius);
		capsuleTest.SetIsDirected(true);
		if(m_AttachParent == m_LookAtTarget)
		{
			capsuleTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}
		else
		{
			const CEntity* entitiesToExclude[2] = {m_AttachParent.Get(), m_LookAtTarget.Get()};
			capsuleTest.SetExcludeEntities(entitiesToExclude, 2, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}

		const bool hasHit		= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		const float hitTValue	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

		cameraPosition.Lerp(hitTValue, attachPosition, cameraPosition);

		const float actualOrbitDistance = attachPosition.Dist(cameraPosition);
		if(actualOrbitDistance > maxOrbitDistance)
		{
			//Check that we have a clear line-of-sight to the look-at position from the proposed camera position, but only if the look-at
			//position is sufficiently displaced from the attach position.
			bool hasClearLosToLookAtPosition;
			const float attachToLookAtDistance2 = attachPosition.Dist2(lookAtPosition);
			if(attachToLookAtDistance2 >= (m_Metadata.m_MinAttachToLookAtDistanceForLosTest * m_Metadata.m_MinAttachToLookAtDistanceForLosTest))
			{
				capsuleTest.SetCapsule(cameraPosition, lookAtPosition, m_Metadata.m_LosTestRadius);
				capsuleTest.SetIsDirected(false);
				capsuleTest.SetExcludeEntity(m_LookAtTarget, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
				hasClearLosToLookAtPosition = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
			}
			else
			{
				hasClearLosToLookAtPosition = true;
			}

			if(hasClearLosToLookAtPosition)
			{
				bestCameraPosition	= cameraPosition;
				maxOrbitDistance	= actualOrbitDistance;
			}
		}

		orbitHeading = fwAngle::LimitRadianAngle(orbitHeading + headingSweepStepSize);
	}

	const bool isPositionValid = (maxOrbitDistance >= m_Metadata.m_MinOrbitDistance);
	if(isPositionValid)
	{
		m_Frame.SetPosition(bestCameraPosition);
	}

	return isPositionValid;
}

bool camCinematicPedCloseUpCamera::ComputeIsCameraClipping() const
{
	const Vector3& cameraPosition = m_Frame.GetPosition();

	float  waterHeight; 

	if(camCollision::ComputeWaterHeightAtPosition(cameraPosition, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight, m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(cameraPosition.z <  waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true; 
		}
	}

	WorldProbe::CShapeTestSphereDesc sphereTest;
	sphereTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_RAGDOLL_TYPE);
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(cameraPosition, m_Metadata.m_ClippingTestRadius);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	const bool isClipping = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);

	return isClipping;
}

void camCinematicPedCloseUpCamera::UpdateCameraOrientation(const Vector3& attachPosition, const Vector3& lookAtPosition)
{
	Vector3 lookAtPositionWithLerp;
	lookAtPositionWithLerp.Lerp(m_Metadata.m_AttachToLookAtBlendLevel, attachPosition, lookAtPosition);

	const Vector3& cameraPosition	= m_Frame.GetPosition();
	Vector3 desiredLookAtFront		= lookAtPositionWithLerp - cameraPosition;
	desiredLookAtFront.NormalizeSafe();

	float desiredLookAtHeading;
	float desiredLookAtPitch;
	camFrame::ComputeHeadingAndPitchFromFront(desiredLookAtFront, desiredLookAtHeading, desiredLookAtPitch);

	if(!IsNearZero(m_Metadata.m_MaxLeadingLookHeadingOffset))
	{
		//Apply an additional heading offset to provide a leading look.
		const float frontDotTargetPedRight		= desiredLookAtFront.Dot(VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetA()));
		float leadingLookHeadingOffset	= frontDotTargetPedRight * m_Metadata.m_MaxLeadingLookHeadingOffset * DtoR;
		
		if(m_NumUpdatesPerformed == 0)
		{
			m_LeadingLookSpring.Reset(leadingLookHeadingOffset); 
		}
		else
		{
			m_LeadingLookSpring.Update(leadingLookHeadingOffset, m_Metadata.m_LeadingLookSpringConstant); 
		}

		leadingLookHeadingOffset = m_LeadingLookSpring.GetResult(); 
		desiredLookAtHeading += leadingLookHeadingOffset;

	}

	//Ensure that we blend to the desired heading over the shortest angle.
	const float currentLookAtHeading		= m_LookAtHeadingAlignmentSpring.GetResult();
	const float desiredLookAtHeadingDelta	= desiredLookAtHeading - currentLookAtHeading;
	if(desiredLookAtHeadingDelta > PI)
	{
		desiredLookAtHeading -= TWO_PI;
	}
	else if(desiredLookAtHeadingDelta < -PI)
	{
		desiredLookAtHeading += TWO_PI;
	}

	if(m_NumUpdatesPerformed == 0)
	{
		m_LookAtHeadingAlignmentSpring.Reset(desiredLookAtHeading);
	}
	else
	{
		m_LookAtHeadingAlignmentSpring.Update(desiredLookAtHeading, m_Metadata.m_LookAtAlignmentSpringConstant);
	}

	if(m_NumUpdatesPerformed == 0)
	{
		m_LookAtPitchAlignmentSpring.Reset(desiredLookAtPitch);
	}
	else
	{
		m_LookAtPitchAlignmentSpring.Update(desiredLookAtPitch, m_Metadata.m_LookAtAlignmentSpringConstant);
	}

	const float lookAtHeading	= m_LookAtHeadingAlignmentSpring.GetResult();
	const float lookAtPitch		= m_LookAtPitchAlignmentSpring.GetResult();

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(lookAtHeading, lookAtPitch);
}

bool camCinematicPedCloseUpCamera::IsValid()
{ 
	return m_IsFramingValid;
}


bool camCinematicPedCloseUpCamera::IsTargetTooFarAway(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	float distanceToTarget = cameraPosition.Dist2(lookAtPosition); 
	bool shouldTerminate = false; 
	if(m_NumUpdatesPerformed == 0)
	{
		if(distanceToTarget > (m_Metadata.m_MaxDistanceToTerminate * m_Metadata.m_MaxDistanceToTerminate) / 2.0f)
		{
			shouldTerminate = true;
		}
	}
	else
	{
		if(distanceToTarget > m_Metadata.m_MaxDistanceToTerminate * m_Metadata.m_MaxDistanceToTerminate)
		{
			shouldTerminate = true;
		}
	}
	return shouldTerminate; 
}

bool camCinematicPedCloseUpCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
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

bool camCinematicPedCloseUpCamera::UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	bool haveLineOfSightToLookAt = true; 

	bool hasHit = ComputeOcclusion(cameraPosition, lookAtPosition); 


	if(m_NumUpdatesPerformed == 0 && hasHit)
	{
		haveLineOfSightToLookAt = false;
	}

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