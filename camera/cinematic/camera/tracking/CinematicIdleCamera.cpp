//
// camera/cinematic/CinematicIdleCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/tracking/CinematicIdleCamera.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/system/CameraMetadata.h"
#include "Peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicIdleCamera,0x54EFFF6D)

static const int MAX_QUADRANT_AVOIDANCE_ITERATIONS = 20;

camCinematicIdleCamera::camCinematicIdleCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicIdleCameraMetadata&>(metadata))
,m_ShotType(MEDIUM_SHOT)
,m_ShotEndTime(0)
,m_ShotTimeSpentOccluded(0)
,m_IsActive(false)
,m_DesiredLookAtPosition(VEC3_ZERO)
{
}

void camCinematicIdleCamera::Init(const camFrame& frame, const CEntity* target)
{
	if(target == NULL)
	{
		return; 
	}

	if(!m_IsActive)
	{
		camBaseCamera::LookAt(target); 
		if(!CutToNewShot(frame))
		{
			m_IsActive = false; 
			return;
		}
		m_IsActive = true; 
	}
}

bool camCinematicIdleCamera::IsValid()
{
	return m_IsActive; 
}

bool camCinematicIdleCamera::Update()
{
	if(m_LookAtTarget.Get() == NULL ) //Safety check.
	{
		m_IsActive = false; 
		return false;
	}

	bool shouldCutToNewShot = false; 
	
	if (!shouldCutToNewShot)
	{
		//Change the camera if it is clipping. 
		WorldProbe::CShapeTestSphereDesc sphereTest;
		//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
		sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
		sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		sphereTest.SetContext(WorldProbe::LOS_Camera);
		sphereTest.SetSphere(GetFrame().GetPosition(), m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForOcclusionTest);
		sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		shouldCutToNewShot = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);

		if(shouldCutToNewShot)
			cameraDebugf1("changed due to clipping %d", fwTimer::GetCamFrameCount()); 
	}

	if(!shouldCutToNewShot)
	{			
		//Change the camera if it is occluded for too long. 
		const bool hasHit = ComputeOcclusion(m_DesiredLookAtPosition, GetFrame().GetPosition()); 

		if(hasHit)
		{
			m_ShotTimeSpentOccluded += fwTimer::GetCamTimeInMilliseconds() - fwTimer::GetCamPrevElapsedTimeInMilliseconds(); 
		}

		if(m_ShotTimeSpentOccluded > (u32)(m_Metadata.m_MaxTimeToSpendOccluded * 1000.0f))
		{
			shouldCutToNewShot = true; 
			if(shouldCutToNewShot)
				cameraDebugf1("changed due to occlusion %d", fwTimer::GetCamFrameCount()); 
		}	
	}

	if ( shouldCutToNewShot )
	{
		m_IsActive = false; 
	}
	
	return m_IsActive;
}



bool camCinematicIdleCamera::ComputeOcclusion(const Vector3& vLookat, const Vector3& vCamPosition)
{
	const CPed* followPed = NULL; 
	if(m_LookAtTarget->GetIsTypePed())
	{
		followPed = (CPed*)m_LookAtTarget.Get();
	}
	else
	{
		return false; 
	}
	atArray<const phInst*> excludeInstances;

	WorldProbe::CShapeTestCapsuleDesc occluderCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> occluderShapeTestResults;
	occluderCapsuleTest.SetResultsStructure(&occluderShapeTestResults);
	//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
	occluderCapsuleTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	occluderCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	occluderCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	//occluderCapsuleTest.SetExcludeEntity(followPed, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 

	camCinematicAnimatedCamera::AddEntityToExceptionList(followPed, excludeInstances); 

	const int numExcludeInstances = excludeInstances.GetCount();
	if(numExcludeInstances > 0)
	{
		occluderCapsuleTest.SetExcludeInstances(excludeInstances.GetElements(), numExcludeInstances);
	}

	occluderCapsuleTest.SetCapsule(vLookat, vCamPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForOcclusionTest);
	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(occluderCapsuleTest);

	return hasHit; 
}

bool camCinematicIdleCamera::ComputeDesiredLookAtPosition()
{
	const CPed* followPed = NULL; 
	if(m_LookAtTarget->GetIsTypePed())
	{
		followPed = (CPed*)m_LookAtTarget.Get();
	}
	else
	{
		return false; 
	}

	Vector3 headPosition;
	if(followPed->GetBonePosition(headPosition, BONETAG_HEAD))
	{
		Vector3 RootPosition;

		RootPosition = VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition()); 

		//use the root position and head z values, because the the head translates to much
		m_DesiredLookAtPosition = RootPosition; 

		m_DesiredLookAtPosition.z = headPosition.z; 

		return true;
	}

	return false;
}

bool camCinematicIdleCamera::CutToNewShot(const camFrame& Frame)
{
	const CPed* followPed = NULL; 
	if(m_LookAtTarget->GetIsTypePed())
	{
		followPed = (CPed*)m_LookAtTarget.Get();
	}
	else
	{
		return false; 
	}

	if(!ComputeDesiredLookAtPosition())
	{
		return false; 
	}

	const Matrix34 & followPedTransform = MAT34V_TO_MATRIX34(followPed->GetTransform().GetMatrix());

	// Determine a random Heading angle that observes the minimum delta, in order to avoid a jump cu
	const float currentHeading = Frame.ComputeHeading();

	const float minHeadingBetweenShots = DtoR * m_Metadata.m_MinHeadingDeltaBetweenShots;
	const float HeadingRange = 2.0f * ( PI - minHeadingBetweenShots );

	const float followPedHeading = followPed->GetCurrentHeading();

	const float minHeadingDeltaFromActorQuadrants = DtoR *  m_Metadata.m_MinHeadingDeltaFromActorQuadrants;

	for ( int i = 0; i < m_Metadata.m_MaxShotSelectionIterations; i++ )
	{
		// Determine a random shot type, avoiding the current type
		const u32 shotTypeOffset = fwRandom::GetRandomNumberInRange( 1, NUM_SHOT_TYPES);
		const u32 shotType = ( m_ShotType + shotTypeOffset ) % NUM_SHOT_TYPES;

		camCinematicIdleShots shot;
		switch( shotType )
		{
		case WIDE_SHOT:
			shot = m_Metadata.m_WideShot;
			break;

		case MEDIUM_SHOT:
			shot = m_Metadata.m_MediumShot;
			break;

			// case CLOSE_UP_SHOT:
		default:
			shot = m_Metadata.m_CloseUpShot;
			break;
		}

		// Determine a random, but compatible, Heading angle and ensure that it is not too close to a 90 degree
		// multiple w.r.t. the followPed orientation
		// NOTE: We tolerate a quadrant-aligned angle if we failed to find a solution in a sensible number of iterations
		float desiredHeading =0.0f;
		for ( int quadIteration = 0; quadIteration < MAX_QUADRANT_AVOIDANCE_ITERATIONS; quadIteration++ )
		{
			const float HeadingDeltaWithinRange = fwRandom::GetRandomNumberInRange( 0.0f, HeadingRange );
			desiredHeading = currentHeading + minHeadingBetweenShots + HeadingDeltaWithinRange;
			desiredHeading = fwAngle::LimitRadianAngle( desiredHeading );

			// Ensure that this random Heading angle is not too close to a 90 degree multiple w.r.t. the followPed

			float followPedRelativeHeading = desiredHeading - followPedHeading;
			followPedRelativeHeading = fwAngle::LimitRadianAngle( followPedRelativeHeading );
			followPedRelativeHeading += PI;

			//make sure wrap works cause it doesn't like negative values
			const float modHeading = Wrap( followPedRelativeHeading, 0.0f, HALF_PI );

			if ( ( modHeading >= minHeadingDeltaFromActorQuadrants ) &&
				( modHeading <= ( HALF_PI - minHeadingDeltaFromActorQuadrants ) ) )
			{
				break;
			}
		}

		//Determine a random pitch, within sensible limits
		const float desiredPitch = fwRandom::GetRandomNumberInRange( DtoR*shot.m_PitchLimits.x , DtoR*shot.m_PitchLimits.y );

		Vector3 desiredForward;
		camFrame::ComputeFrontFromHeadingAndPitch(desiredHeading, desiredPitch, desiredForward); 

		// Determine a random camera distance back from the look-at, within the limits for the shot
		const float desiredDistance = fwRandom::GetRandomNumberInRange( shot.m_DistanceLimits.x, shot.m_DistanceLimits.y);

		Vector3 desiredCameraPosition = m_DesiredLookAtPosition - ( desiredDistance * desiredForward );

		// Constrain the camera against the world
		WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
		WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

		constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
		constrainCapsuleTest.SetIncludeFlags(camCollision::GetCollisionIncludeFlags());
		constrainCapsuleTest.SetIsDirected(true);
		constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
		constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
		constrainCapsuleTest.SetExcludeEntity(followPed, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 

		constrainCapsuleTest.SetCapsule(m_DesiredLookAtPosition, desiredCameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForConstrain);

		bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
		const float hitTValue	= hasHit ? constrainShapeTestResults[0].GetHitTValue() : 1.0f;

		//TODO: Apply damping when the root position is moving away from the fall back position.
		desiredCameraPosition = Lerp(hitTValue, m_DesiredLookAtPosition, desiredCameraPosition);

		// Ensure that there is still a reasonable distance between the camera and the look-at.
		const float distToLookAt = desiredCameraPosition.Dist2( m_DesiredLookAtPosition );
		if ( distToLookAt < m_Metadata.m_CloseUpShot.m_DistanceLimits.x * m_Metadata.m_CloseUpShot.m_DistanceLimits.x  )
		{
			continue;
		}

		//check for the camera being occluded
		if (ComputeOcclusion(m_DesiredLookAtPosition, desiredCameraPosition))
		{
			continue; 
		}
		
		// Note that we also use the look-at position as the root
		//Vector3 Look = m_DesiredLookAtPosition - desiredCameraPosition; 

		m_Frame.SetWorldMatrixFromFront(desiredForward);
		m_Frame.SetPosition(desiredCameraPosition);

		// Determine the correct screen position for the desired camera distance from the look-at position
		const float distanceToLookAt = desiredCameraPosition.Dist( m_DesiredLookAtPosition );
		const float overallMinDistance = m_Metadata.m_CloseUpShot.m_DistanceLimits.x;
		const float overallMaxDistance = m_Metadata.m_WideShot.m_DistanceLimits.y;
		const float distanceFraction = Clamp( ( distanceToLookAt - overallMinDistance ) /
			( overallMaxDistance - overallMinDistance ),
			0.f, 1.f );

		float heading, pitch; 

		//calculate the pitch to apply to keep the players head near the same postion on screen
		m_Frame.ComputeHeadingAndPitch(heading, pitch);

		float ScreenRatio = Lerp(distanceFraction, m_Metadata.m_ScreenHeightRatio.x, m_Metadata.m_ScreenHeightRatio.y); 
	
		float fov = m_Frame.GetFov(); 

		pitch -= DtoR * fov * ScreenRatio;
	
		pitch = Clamp(pitch, -89.0f*DtoR, 89.0f*DtoR); 

		// Apply an additional Heading rotation to provide a leading look
		const float forwardDotfollowPedLeft = desiredForward.Dot( followPedTransform.a );

		float LeadingShot = fwRandom::GetRandomNumberInRange( m_Metadata.m_LeadingLookScreenRotation.x , m_Metadata.m_LeadingLookScreenRotation.y);

		heading += forwardDotfollowPedLeft * LeadingShot;
		
		heading = fwAngle::LimitRadianAngle(heading); 

		//Apply the new pitch and heading values to the create a new frame positon
		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch); 

		// Focus the DOF post-effect
		// NOTE: This algorithm is high on artistic license and low on realistic lens simulation
	/*	if ( m_Metadata.m_PostEffect != NULL )
		{
			PostProcessEffectsAction * action = m_Metadata.m_PostEffect->GetActions()[0].GetPointer();
			PostProcessEffectsDofAction * dofAction = action ? action->DynamicCast< PostProcessEffectsDofAction >() : NULL;
			if ( dofAction )
			{
				dofAction->m_DofFocus = distanceToLookAt;

				const float depthOfField = distanceToLookAt * m_Metadata.m_DistanceScalingFactorForDof;
				dofAction->m_DofNearFocus = distanceToLookAt - ( depthOfField / 3.f );
				dofAction->m_DofFarFocus = distanceToLookAt + ( depthOfField * 2.f / 3.f );

				dofAction->m_DofNear = dofAction->m_DofNearFocus / m_Metadata.m_FocusToOutOfFocusDepthScaling;
				dofAction->m_DofFar = dofAction->m_DofFarFocus * m_Metadata.m_FocusToOutOfFocusDepthScaling;
			}
		}*/

		// Determine a random shot duration, within sensible limits
		float shotTime = fwRandom::GetRandomNumberInRange( m_Metadata.m_ShotDurationLimits.x , m_Metadata.m_ShotDurationLimits.y )* 1000.0f ; 
		u32 currentTime =  fwTimer::GetCamTimeInMilliseconds();
		m_ShotEndTime = currentTime + ((u32)shotTime);

		m_ShotType = shotType;
		m_ShotTimeSpentOccluded = 0;

		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

		return true;
	}
	
	cameraDebugf1("failed to find shot"); 

	return false;
}