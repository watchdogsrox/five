//
// camera/cinematic/CinematicWaterCrashCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "CinematicWaterCrashCamera.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/system/CameraMetadata.h"
#include "camera/cinematic/context/CinematicWaterCrashContext.h"
#include "fwmaths/random.h"
#include "peds/ped.h"
#include "scene/Entity.h"
#include "vehicles/vehicle.h"

INSTANTIATE_RTTI_CLASS(camCinematicWaterCrashCamera,0x84F7B3E8)

CAMERA_OPTIMISATIONS()

camCinematicWaterCrashCamera::camCinematicWaterCrashCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicWaterCrashCameraMetadata&>(metadata))
, m_pContext(NULL)
, m_initialZOffset(0.0f)
, m_zOffset(0.0f)
, m_boundRadius(0.0f)
, m_ShotTimeSpentOccluded(0)
, m_StartTime(fwTimer::GetCamTimeInMilliseconds())
, m_canUpdate(false)
, m_shouldUseAttachParent(true)
, m_catchUpToGameplay(false)
{
}

void camCinematicWaterCrashCamera::Init()
{
	m_CamPosition = ComputePositionRelativeToAttachParent();
	m_initialZOffset = m_CamPosition.z;
}

Vector3 camCinematicWaterCrashCamera::ComputePositionRelativeToAttachParent()
{
	Vector3 vIntialPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition()); 
	float fIntialHeading = m_AttachParent->GetTransform().GetHeading();
	//float AngleDelta = 0.0f; 
	s32 index = 0;

	m_boundRadius = m_LookAtTarget->GetBoundRadius() * m_Metadata.m_RadiusScale;

	Vector3 front = VEC3_ZERO;
	float orbitDistance = m_Metadata.m_DistanceToTarget + m_boundRadius;
	camFrame::ComputeFrontFromHeadingAndPitch(fIntialHeading, m_Metadata.m_PitchLimits.x * DtoR, front);
	Vector3 zeroPosition = vIntialPosition - (front * orbitDistance);

	//if (GetNumOfModdes() > 0)
	{
		// TODO: metadata?
		// Trying to keep camera behind plane, so it is easier to blend to gameplay camera.
		const u32 uTotalAnglesToTest = 5;
		float anglesToTest[uTotalAnglesToTest] = { 180.0f, -150.0f, 150.0f, -120.0f, 120.0f };

		//AngleDelta = TWO_PI / (float)GetNumOfModes(); 
		index = fwRandom::GetRandomNumberInRange((int)0, uTotalAnglesToTest); //GetNumOfModes());

		Vector3 cameraPosition = VEC3_ZERO;
		//for (s32 i = 0; i < GetNumOfModes(); i++)
		for (s32 i = 0; i < uTotalAnglesToTest; i++)
		{
			float angleToTest = anglesToTest[index] * DtoR; //AngleDelta * float(index);
			float headingToTest = fIntialHeading + angleToTest;
			headingToTest = fwAngle::LimitRadianAngleSafe(headingToTest);

			camFrame::ComputeFrontFromHeadingAndPitch(headingToTest, m_Metadata.m_PitchLimits.x * DtoR, front); 
			Vector3 cameraPosition = vIntialPosition - (front * orbitDistance);
			Vector3 dropPosition   = cameraPosition;
			dropPosition.z += m_Metadata.m_DropDistance;

			// Validate that camera path is clear, otherwise, we need to choose a different angle.
			if (!ComputeCollision(cameraPosition, dropPosition, camCollision::GetCollisionIncludeFlags()))
			{
				fIntialHeading = headingToTest;
				return cameraPosition;
			}

			index++;
			if (index >= uTotalAnglesToTest) //GetNumOfModes())
			{
				index = 0;
			}
		}

		// Test failsafe angles.
		const u32 uTotalExtraAnglesToTest = 4;
		float extraAnglesToTest[uTotalExtraAnglesToTest] = { 90.0f, -90.0f, 75.0f, -75.0f };
		index = 0;

		for (s32 i = 0; i < uTotalExtraAnglesToTest; i++)
		{
			float angleToTest = extraAnglesToTest[index] * DtoR;
			float headingToTest = fIntialHeading + angleToTest;
			headingToTest = fwAngle::LimitRadianAngleSafe(headingToTest);

			camFrame::ComputeFrontFromHeadingAndPitch(headingToTest, m_Metadata.m_PitchLimits.x * DtoR, front); 
			Vector3 cameraPosition = vIntialPosition - (front * orbitDistance);
			Vector3 dropPosition   = cameraPosition;
			dropPosition.z += m_Metadata.m_DropDistance;

			// Validate that camera path is clear, otherwise, we need to choose a different angle.
			if (!ComputeCollision(cameraPosition, dropPosition, camCollision::GetCollisionIncludeFlags()))
			{
				return cameraPosition;
			}

			index++;
			if (index >= uTotalExtraAnglesToTest)
			{
				index = 0;
			}
		}
	}

	return zeroPosition;
}

bool camCinematicWaterCrashCamera::IsValid()
{
	return m_canUpdate; 
}

bool camCinematicWaterCrashCamera::ShouldUpdate()
{
	if(m_NumUpdatesPerformed > 0)	
	{
		if(!m_canUpdate)
		{
			return false; 
		}
	}

	//check possible 
	if(m_shouldUseAttachParent && !m_AttachParent)
	{
		m_canUpdate = false; 
		return false; 
	}

	return true; 
}

bool camCinematicWaterCrashCamera::Update()
{
	if(!ShouldUpdate())
	{
		return m_canUpdate;
	}

	if(UpdateCamera())
	{
		m_canUpdate = true;
	}
	else
	{
		m_canUpdate = false;
		if (m_pContext && (m_pContext->GetClassId() == camCinematicWaterCrashContext::GetStaticClassId()))
		{
			// Camera failed, so fail the context so we don't try to restart the water crash.
			camCinematicWaterCrashContext* pContext = static_cast<camCinematicWaterCrashContext*>(m_pContext); 
			pContext->ClearTarget();
		}
	}

	return m_canUpdate; 
}

bool camCinematicWaterCrashCamera::UpdateCamera()
{	
	if(m_LookAtTarget)
	{
		const Vector3& lookAtPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 

		UpdateZOffset();

		Vector3 front = lookAtPosition - m_CamPosition;
		float fDistance = front.Mag();
		if (fDistance > SMALL_FLOAT)
		{
			front.Scale(1.0f/fDistance);

			float minDistance = m_Metadata.m_DistanceToTarget + m_boundRadius;
			float maxDistance = m_Metadata.m_DistanceToTarget * 2.00f + m_boundRadius;	// TODO: metadata?

			fDistance = Clamp(fDistance, minDistance, maxDistance);
			m_CamPosition = lookAtPosition - (front * fDistance);
		}

		Vector3 finalPosition = m_CamPosition;
		finalPosition.z = m_initialZOffset + m_zOffset;

		if (!UpdateCollision(finalPosition, lookAtPosition))
		{
			return false;
		}

		if (!UpdateOcclusion(finalPosition, lookAtPosition))
		{
			return false;
		}

		if (IsClipping(	finalPosition,
						(ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
		{
			return false;
		}

		front = lookAtPosition - finalPosition;
		front.Normalize();

		m_Frame.SetWorldMatrixFromFront(front);

		m_Frame.SetFov(m_Metadata.m_ZoomFov);

		camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();
		bool bIsInVehicle = (followVehicleCamera != NULL);

	#if 0
		if(m_pContext && (m_pContext->GetClassId() == camCinematicWaterCrashContext::GetStaticClassId()))
		{
			if (!bIsInVehicle ||
				m_StartTime + m_Metadata.m_DropDuration + m_Metadata.m_RiseDuration < fwTimer::GetCamTimeInMilliseconds())
			{
				camCinematicWaterCrashContext* pContext = static_cast<camCinematicWaterCrashContext*>(m_pContext); 
				u32 uTimeToBlend = pContext->GetContextDuration();
				if (uTimeToBlend >  (m_Metadata.m_DropDuration + m_Metadata.m_RiseDuration))
				{
					 uTimeToBlend -= (m_Metadata.m_DropDuration + m_Metadata.m_RiseDuration);
				}
				else
				{
					uTimeToBlend = 0;
				}
				const camFrame& gameplayFrame = camInterface::GetGameplayDirector().GetFrame();
				Vector3 vGameplayCameraPosition = gameplayFrame.GetPosition();

				float fInterp = (float)(fwTimer::GetCamTimeInMilliseconds() - (m_StartTime + m_Metadata.m_DropDuration + m_Metadata.m_RiseDuration)) /
										uTimeToBlend;
				fInterp = Clamp(fInterp, 0.0f, 1.0f);
				fInterp = SlowInOut(fInterp);

				Matrix34 finalWorldMatrix;
				camFrame::SlerpOrientation(fInterp, m_Frame.GetWorldMatrix(), gameplayFrame.GetWorldMatrix(), finalWorldMatrix);
				finalPosition.Lerp(fInterp, finalPosition, vGameplayCameraPosition);
				float fFov = Lerp(fInterp, m_Metadata.m_ZoomFov, gameplayFrame.GetFov());

				m_Frame.SetWorldMatrix(finalWorldMatrix);

				m_Frame.SetFov(fFov);
			}
		}
	#else
		if ( m_catchUpToGameplay ||
			(m_pContext && (m_pContext->GetClassId() == camCinematicWaterCrashContext::GetStaticClassId())) )
		{
			camCinematicWaterCrashContext* pContext = (!m_catchUpToGameplay) ? static_cast<camCinematicWaterCrashContext*>(m_pContext) : NULL;
			if (!bIsInVehicle && pContext != NULL)
			{
				m_catchUpToGameplay = true;
				pContext->ClearTarget();		// tell context to terminate, so we go back to gameplay camera, which will handle the blend.
				m_pContext = NULL;
			}
			else if (m_catchUpToGameplay ||
					 m_StartTime + pContext->GetContextDuration() < fwTimer::GetCamTimeInMilliseconds())
			{
				// When movement duration ends, use catch up helper instead of blending to gameplay camera ourselves.
				// benefit: camera will not blend to gameplay until target moves too far away.
				////camInterface::GetGameplayDirector().CatchUpFromFrame(m_Frame, m_Metadata.m_CatchUpDistance);

				if (!m_catchUpToGameplay && pContext != NULL)
				{
					m_catchUpToGameplay = true;
					pContext->ClearTarget();		// tell context to terminate, so we go back to gameplay camera, which will handle the blend.
					m_pContext = NULL;

					//if (m_Metadata.m_BlendDuration > 0 && followVehicleCamera)
					//{
					//	followVehicleCamera->InterpolateFromCamera(*this, m_Metadata.m_BlendDuration, true);
					//}
				}
			}
		}
	#endif

		m_Frame.SetPosition(finalPosition);
	}

	return true;
}

bool camCinematicWaterCrashCamera::UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
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

bool camCinematicWaterCrashCamera::UpdateCollision(Vector3& cameraDesiredPosition, const Vector3& lookAtPosition)
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

bool camCinematicWaterCrashCamera::ComputeCollision(Vector3& cameraPosition, const Vector3&  lookAtPosition, const u32 flags)
{
	//Vector3 desiredCameraPosition;
	bool hasHit = false;

	// Constrain the camera against the world
	WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(flags);
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);
	constrainCapsuleTest.SetIsDirected(false);

	if(m_shouldUseAttachParent && m_AttachParent)
	{
		constrainCapsuleTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
	}

	constrainCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
	
	return hasHit; 
}

bool camCinematicWaterCrashCamera::IsClipping(const Vector3& position, const u32 flags)
{
	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
	sphereTest.SetIncludeFlags(flags); 
	//sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(position, m_Metadata.m_CollisionRadius);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	return WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
}

bool camCinematicWaterCrashCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
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

s32 CompareIntersections(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b)
{
	return (a->GetHitTValue() < b->GetHitTValue()) ? -1 : 1;
}

void camCinematicWaterCrashCamera::UpdateZOffset()
{
	u32 currentTime = fwTimer::GetCamTimeInMilliseconds();

	if (m_StartTime + m_Metadata.m_DropDuration > currentTime)
	{
		float fInterp = (float)(currentTime - m_StartTime) / m_Metadata.m_DropDuration;
		fInterp = Clamp(fInterp, 0.0f, 1.0f);
		fInterp = SlowInOut(fInterp);

		m_zOffset = Lerp(fInterp, 0.0f, m_Metadata.m_DropDistance);
	}
	else if (m_StartTime + m_Metadata.m_DropDuration + m_Metadata.m_RiseDuration > currentTime)
	{
		float fInterp = (float)(currentTime - (m_StartTime + m_Metadata.m_DropDuration)) / m_Metadata.m_RiseDuration;
		fInterp = Clamp(fInterp, 0.0f, 1.0f);
		fInterp = SlowInOut(fInterp);

		m_zOffset = Lerp(fInterp, m_Metadata.m_DropDistance, m_Metadata.m_RiseDistance);
	}
	else
	{
		m_zOffset = m_Metadata.m_RiseDistance;
	}


	// Clamp z offset based on collision.
	Vector3 startPosition = m_CamPosition;
	Vector3 endPosition = m_CamPosition;
	startPosition.z = m_initialZOffset;
	endPosition.z   = m_initialZOffset + m_zOffset;

	// Constrain the camera against the world
	WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(camCollision::GetCollisionIncludeFlags());
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);
	constrainCapsuleTest.SetIsDirected(true);

	if(m_shouldUseAttachParent && m_AttachParent)
	{
		constrainCapsuleTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
	}

	constrainCapsuleTest.SetCapsule(startPosition, endPosition, m_Metadata.m_CollisionRadius);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
	if (hasHit && constrainShapeTestResults.GetNumHits() > 0)
	{
		if (constrainShapeTestResults.GetNumHits() > 1)
		{
			constrainShapeTestResults.SortHits(CompareIntersections);
		}

		float fNewZOffset = 0.0f;
		for (WorldProbe::ResultIterator it = constrainShapeTestResults.begin(); it < constrainShapeTestResults.last_result(); ++it)
		{
			if (it->GetHitDetected())
			{
				fNewZOffset = Lerp(it->GetHitTValue(), 0.0f, m_zOffset);
				fNewZOffset += m_Metadata.m_CollisionRadius;
				if (fNewZOffset > m_initialZOffset)
				{
					m_zOffset = m_initialZOffset;
				}
				else if (fNewZOffset > m_zOffset)
				{
					m_zOffset = fNewZOffset;
				}
				break;
			}
		}
	}
}
