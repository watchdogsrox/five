//
// camera/cinematic/CinematicTwoShotCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/tracking/CinematicTwoShotCamera.h"

#include "camera/cinematic/cinematicdirector.h"
#include "camera/CamInterface.h"
#include "camera/helpers/Collision.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicTwoShotCamera,0x2679ee2f)

#if __BANK
bool	camCinematicTwoShotCamera::ms_DebugRender = false;
#endif // __BANK

camCinematicTwoShotCamera::camCinematicTwoShotCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicTwoShotCameraMetadata&>(metadata))
, m_IsFramingValid(false)
, m_IsCameraOnRight(false)
, m_LockCameraPosition(false)
{
}

bool camCinematicTwoShotCamera::Update()
{
	const Vector3 attachEntityPosition = Vector3(m_AttachEntityPositionSpringX.GetResult(), m_AttachEntityPositionSpringY.GetResult(), m_AttachEntityPositionSpringZ.GetResult());

	if(!m_LookAtTarget.Get() || !m_LookAtTarget->GetIsTypePed() || attachEntityPosition.IsClose(g_InvalidPosition, SMALL_FLOAT)) //Safety check.
	{
		m_IsFramingValid = false;
		return false;
	}
	
	const Vector3 basePivotPosition	= attachEntityPosition;

	Vector3 lookAtPosition;
	ComputeLookAtPosition(lookAtPosition);

	if (ComputeIsTargetTooClose(basePivotPosition, lookAtPosition))
	{
#if __BANK
		if (ms_DebugRender)
		{
			grcDebugDraw::Sphere(basePivotPosition, 0.18f, Color_pink, true, 30 * 8);
		}
#endif //__BANK
		m_IsFramingValid = false;
		return false;
	}

	float orbitHeading;
	float orbitPitch;
	ComputeOrbitOrientation(basePivotPosition, lookAtPosition, true, orbitHeading, orbitPitch);

	//we freeze camera's position if it's clipped and we've said to do so in metadata
	Vector3 cameraPosition = m_Frame.GetPosition();
	if (!m_LockCameraPosition)
	{
		const bool isPositionValid = ComputeCameraPosition(basePivotPosition, orbitHeading, orbitPitch, cameraPosition);
		
		if (!isPositionValid)
		{
#if __BANK
			if (ms_DebugRender)
			{
				grcDebugDraw::Sphere(cameraPosition, m_Metadata.m_ClippingTestRadius, Color_orange, true, 30 * 8);
			}
#endif //__BANK

			if (m_NumUpdatesPerformed == 0 || !m_Metadata.m_ShouldLockCameraPositionOnClipping)
			{
				m_IsFramingValid		= false;
				return false;
			}
			else
			{
				m_LockCameraPosition	= true;
				cameraPosition			= m_Frame.GetPosition();
			}
		}
	}

	//recalculate orbit orientation
	ComputeOrbitOrientation(cameraPosition, lookAtPosition, false, orbitHeading, orbitPitch);

	m_Frame.SetPosition(cameraPosition);
	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(orbitHeading, orbitPitch, 0.0f);

	//occlusion test only happens first frame
	if (m_NumUpdatesPerformed == 0)
	{
		const camGameplayDirector::tCoverSettings& coverSettings = camInterface::GetGameplayDirector().GetFollowPedCoverSettings();
		const bool playerOccluded = !coverSettings.m_IsInCover && ComputeOcclusion(cameraPosition, lookAtPosition); //we don't care if the player is in cover
		const bool targetOccluded = ComputeOcclusion(cameraPosition, attachEntityPosition);

		if(playerOccluded || targetOccluded)
		{
#if __BANK
			if (ms_DebugRender)
			{
				grcDebugDraw::Sphere(cameraPosition, m_Metadata.m_LosTestRadius, Color_red, true, 30 * 8);
			}
#endif //__BANK
			m_IsFramingValid = false;
			return false;
		}
	}

	if(ComputeIsCameraClipping(cameraPosition))
	{
#if __BANK
		if (ms_DebugRender)
		{
			grcDebugDraw::Sphere(cameraPosition, m_Metadata.m_ClippingTestRadius, Color_blue, true, 30 * 8);
		}
#endif //__BANK
		m_IsFramingValid = false;
		return false;
	}

	const float fovLastUpdate = (m_NumUpdatesPerformed == 0) ? m_Metadata.m_BaseFov : m_Frame.GetFov();
	const float fov = ComputeFov(fovLastUpdate);
	m_Frame.SetFov(fov);

	m_Frame.SetNearClip(m_Metadata.m_BaseNearClip);
	m_Frame.SetCameraTimeScaleThisUpdate(m_Metadata.m_TimeScale);

	m_IsFramingValid = true;
	return m_IsFramingValid;
}

void camCinematicTwoShotCamera::ComputeLookAtPosition(Vector3& lookAtPosition) const
{
	//NOTE: m_LookAtTarget and m_LookAtTarget->GetIsTypePed() have already been validated.
	const CPed* lookAtPed = static_cast<const CPed*>(m_LookAtTarget.Get());
	const eAnimBoneTag lookAtBoneTag = (m_Metadata.m_LookAtBoneTag >= 0) ? (eAnimBoneTag)m_Metadata.m_LookAtBoneTag : BONETAG_ROOT;

	Matrix34 boneMatrix;
	lookAtPed->GetBoneMatrix(boneMatrix, lookAtBoneTag);

	lookAtPosition = boneMatrix.d;
}

bool camCinematicTwoShotCamera::ComputeIsTargetTooClose(const Vector3& basePivotPosition, const Vector3& lookAtPosition) const
{
	Vector3 lockOnDelta				= lookAtPosition - basePivotPosition;
	const float distanceToTarget	= lockOnDelta.Mag2();

	if(distanceToTarget < m_Metadata.m_MinDistanceForLockOn * m_Metadata.m_MinDistanceForLockOn)
	{
		return true;
	}

	return false;
}

void camCinematicTwoShotCamera::ComputeOrbitOrientation(const Vector3& cameraPosition, const Vector3& lookAtPosition, const bool shouldComputeOrbitRelativePivotOffset,
														   float& orbitHeading, float& orbitPitch) const
{
	const Vector3 lockOnDelta		= lookAtPosition - cameraPosition;

	Vector3 lockOnOrbitFront;
	lockOnOrbitFront.Normalize(lockOnDelta);

	if (shouldComputeOrbitRelativePivotOffset)
	{
		Matrix34 lockOnOrbitMatrix(Matrix34::IdentityType);
		camFrame::ComputeWorldMatrixFromFront(lockOnOrbitFront, lockOnOrbitMatrix);

		Vector3 orbitRelativePivotOffset(VEC3_ZERO);
		ComputeOrbitRelativePivotOffset(orbitRelativePivotOffset);

		const float distanceToTarget	= lockOnDelta.Mag();

		const float sinHeadingDelta		= orbitRelativePivotOffset.x / distanceToTarget;
		const float headingDelta		= AsinfSafe(sinHeadingDelta);

		const float sinPitchDelta		= -orbitRelativePivotOffset.z / distanceToTarget;
		const float pitchDelta			= AsinfSafe(sinPitchDelta);

		lockOnOrbitMatrix.RotateLocalZ(headingDelta);
		lockOnOrbitMatrix.RotateLocalX(pitchDelta);

		camFrame::ComputeHeadingAndPitchFromMatrix(lockOnOrbitMatrix, orbitHeading, orbitPitch);
	}
	else
	{
		camFrame::ComputeHeadingAndPitchFromFront(lockOnOrbitFront, orbitHeading, orbitPitch);
	}

	const CViewport* viewport		= gVpMan.GetGameViewport();
	const float aspectRatio			= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	const float verticalFov			= m_Metadata.m_BaseFov;
	const float horizontalFov		= verticalFov * aspectRatio;

	//Apply a bias in screen space.
	orbitPitch						+= DtoR * verticalFov * m_Metadata.m_ScreenHeightRatioBiasForSafeZone;
	orbitHeading					+= DtoR * horizontalFov * m_Metadata.m_ScreenWidthRatioBiasForSafeZone * (m_IsCameraOnRight ? -1.0f : 1.0f); //flipped
}

void camCinematicTwoShotCamera::ComputePivotPosition(const Matrix34& orbitMatrix, Vector3& pivotPosition) const
{
	//Apply an orbit-relative offset about base pivot position.
	Vector3 orbitRelativeOffset;
	ComputeOrbitRelativePivotOffset(orbitRelativeOffset);

	orbitMatrix.Transform(orbitRelativeOffset, pivotPosition); //Transform to world space.
}

void camCinematicTwoShotCamera::ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const
{
	const float relativeSideOffset		= m_IsCameraOnRight ? m_Metadata.m_CameraRelativeLeftSideOffset : m_Metadata.m_CameraRelativeRightSideOffset; //flipped
	const float relativeVerticalOffset	= m_Metadata.m_CameraRelativeVerticalOffset;

	orbitRelativeOffset.Set(relativeSideOffset, 0.0f, relativeVerticalOffset);
}

bool camCinematicTwoShotCamera::ComputeCameraPosition(const Vector3& basePivotPosition, const float orbitHeading, const float orbitPitch, Vector3& cameraPosition)
{
	//calculate ideal camera position
	Matrix34 orbitMatrix;
	camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(orbitHeading, orbitPitch, 0.0f, orbitMatrix);

	orbitMatrix.d				= basePivotPosition;

	Vector3 pivotPosition;
	ComputePivotPosition(orbitMatrix, pivotPosition);

	const Vector3 orbitFront(orbitMatrix.b);

	cameraPosition				= pivotPosition - (orbitFront * m_Metadata.m_OrbitDistance);

	//force camera position to be a minimum height above the ground
	Vector3 groundProbeStart	= cameraPosition;
	groundProbeStart.z			+= m_Metadata.m_HeightAboveCameraForGroundProbe;
	Vector3 groundProbeEnd		= cameraPosition;
	groundProbeEnd.z			-= m_Metadata.m_HeightBelowCameraForGroundProbe;

	WorldProbe::CShapeTestProbeDesc groundProbeTest;
	WorldProbe::CShapeTestFixedResults<> groundProbeTestResults;
	groundProbeTest.SetResultsStructure(&groundProbeTestResults);
	groundProbeTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	groundProbeTest.SetIsDirected(true);
	groundProbeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	groundProbeTest.SetContext(WorldProbe::LOS_Camera);
	groundProbeTest.SetStartAndEnd(groundProbeStart, groundProbeEnd);

	const bool hasHit			= WorldProbe::GetShapeTestManager()->SubmitTest(groundProbeTest);

	//if we haven't found the ground, the ideal camera position is too high
	if (!hasHit)
	{
		return false;
	}

	const float groundHeight	= Lerp(groundProbeTestResults[0].GetHitTValue(), groundProbeStart.z, groundProbeEnd.z);
	const float desiredZ		= Max(cameraPosition.z, groundHeight + m_Metadata.m_MinHeightAboveGround);

	const bool shouldCutSpring = ((m_NumUpdatesPerformed == 0) ||
									m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	cameraPosition.z			= m_CameraPositionSpringZ.Update(desiredZ, shouldCutSpring ? 0.0f : m_Metadata.m_HeightSpringConstant,
									m_Metadata.m_HeightSpringDampingRatio);

	//testing clipping between old position and new position
	if (m_NumUpdatesPerformed > 0)
	{
		const bool isClipping	= ComputeIsCameraClipping(m_Frame.GetPosition(), &cameraPosition);
		return !isClipping;
	}

	return true;
}

bool camCinematicTwoShotCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition) const
{
	WorldProbe::CShapeTestCapsuleDesc occluderCapsuleTest;

	//We don't test against peds or ragdolls - the chances are good they'll soon move, and it works well with the chaotic nature of this shot.
	occluderCapsuleTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_FOLIAGE_TYPE) & ~ArchetypeFlags::GTA_PED_TYPE);
	occluderCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	occluderCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	occluderCapsuleTest.SetCapsule(cameraPosition, lookAtPosition, m_Metadata.m_LosTestRadius);
	occluderCapsuleTest.SetIsDirected(false);

	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(occluderCapsuleTest);

	return hasHit;
}

bool camCinematicTwoShotCamera::ComputeIsCameraClipping(const Vector3& cameraPosition, const Vector3* newCameraPosition) const
{
	float waterHeight;
	if(camCollision::ComputeWaterHeightAtPosition(cameraPosition, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight, m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(cameraPosition.z < waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true;
		}
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_FOLIAGE_TYPE);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetIsDirected(false);
	capsuleTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	if (newCameraPosition)
	{
		capsuleTest.SetCapsule(cameraPosition, *newCameraPosition, m_Metadata.m_ClippingTestRadius);
	}
	else
	{
		capsuleTest.SetCapsule(cameraPosition, cameraPosition, m_Metadata.m_ClippingTestRadius);
	}

	const bool isClipping = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

	return isClipping;
}

float camCinematicTwoShotCamera::ComputeFov(const float fovLastUpdate) const
{
	const float timeStep	= fwTimer::GetCamTimeStep();
	const float fovChange	= timeStep * m_Metadata.m_FovScalingSpeed;

	const float fov			= fovLastUpdate + fovChange;

	return fov;
}

bool camCinematicTwoShotCamera::IsValid()
{ 
	return m_IsFramingValid;
}

void camCinematicTwoShotCamera::SetAttachEntityPosition(const Vector3& newPosition)
{
	//the attach entity position is damped
	if (m_NumUpdatesPerformed == 0 || m_Metadata.m_ShouldTrackTarget)
	{
		const bool shouldCutSpring = ((m_NumUpdatesPerformed == 0) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
		
		m_AttachEntityPositionSpringX.Update(newPosition.x, shouldCutSpring ? 0.0f : m_Metadata.m_AttachEntityPositionSpringConstant,
												m_Metadata.m_AttachEntityPositionSpringDampingRatio);
		m_AttachEntityPositionSpringY.Update(newPosition.y, shouldCutSpring ? 0.0f : m_Metadata.m_AttachEntityPositionSpringConstant,
												m_Metadata.m_AttachEntityPositionSpringDampingRatio);
		m_AttachEntityPositionSpringZ.Update(newPosition.z, shouldCutSpring ? 0.0f : m_Metadata.m_AttachEntityPositionSpringConstant,
												m_Metadata.m_AttachEntityPositionSpringDampingRatio);
	}
}

#if __BANK
void camCinematicTwoShotCamera::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Cinematic Two Shot camera");
	{
		bank.AddToggle("Should debug render", &ms_DebugRender);
	}
	bank.PopGroup(); //Cinematic Two Shot camera
}
#endif // __BANK