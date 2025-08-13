//
// camera/gameplay/aim/FirstPersonAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/FirstPersonAimCamera.h"

#include "input/headtracking.h"

#include "camera/CamInterface.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/StickyAimHelper.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "scene/portals/Portal.h"
#include "stats/StatsInterface.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFirstPersonAimCamera,0x5F536AAF)

camFirstPersonAimCamera::camFirstPersonAimCamera(const camBaseObjectMetadata& metadata)
: camAimCamera(metadata)
, m_Metadata(static_cast<const camFirstPersonAimCameraMetadata&>(metadata))
, m_StickyAimHelper(NULL)
{
	TryAndCreateOrDestroyStickyAimHelper();
}

camFirstPersonAimCamera::~camFirstPersonAimCamera()
{
	if (m_StickyAimHelper)
	{
		delete m_StickyAimHelper;
		m_StickyAimHelper = NULL;
	}
}

bool camFirstPersonAimCamera::ShouldMakeAttachedEntityInvisible() const
{
	return m_Metadata.m_ShouldMakeAttachedEntityInvisible;
}

bool camFirstPersonAimCamera::ShouldMakeAttachedPedHeadInvisible() const
{
	return m_Metadata.m_ShouldMakeAttachedPedHeadInvisible;
}

bool camFirstPersonAimCamera::ShouldDisplayReticule() const
{
	return m_Metadata.m_ShouldDisplayReticule;
}

bool camFirstPersonAimCamera::Update()
{
	TryAndCreateOrDestroyStickyAimHelper();

	if (m_StickyAimHelper && CanUpdateStickyAimHelper())
	{
		m_StickyAimHelper->Update(m_Frame.GetPosition(), m_Frame.GetFront(), m_Frame.GetWorldMatrix(), false);
	}

	bool hasSucceeded = camAimCamera::Update();
	if(hasSucceeded)
	{
		UpdateInteriorCollision();
		if (!rage::ioHeadTracking::IsMotionTrackingEnabled())
			UpdateShake();
	}

	if (m_StickyAimHelper && CanUpdateStickyAimHelper())
	{
		m_StickyAimHelper->PostCameraUpdate(m_Frame.GetPosition(), m_Frame.GetFront());
	}

	return hasSucceeded;
}

void camFirstPersonAimCamera::UpdatePosition()
{
	//NOTE: m_AttachParent has already been validated.
	const Matrix34 targetMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
	Vector3 relativeAttachPosition;
	ComputeRelativeAttachPosition(targetMatrix, relativeAttachPosition);

	TransformAttachPositionToWorldSpace(targetMatrix, relativeAttachPosition, m_AttachPosition);

	//Position the camera at the attach position.
	m_Frame.SetPosition(m_AttachPosition);
}

void camFirstPersonAimCamera::UpdateOrientation()
{
	float heading;
	float pitch;
	float roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	if (m_StickyAimHelper && CanUpdateStickyAimHelper())
	{
		// Sticky aim needs to update before look around offsets are applied.
		m_StickyAimHelper->UpdateOrientation(heading, pitch,  m_Frame.GetPosition(), m_Frame.GetWorldMatrix());
	}

	//if (!rage::ioHeadTracking::IsMotionTrackingEnabled())
		ApplyOverriddenOrientation(heading, pitch);

	//NOTE: We scale the look-around offsets based up the current FOV, as this allows the look-around input to be responsive at minimum zoom without
	//being too fast at maximum zoom.
	float fovOrientationScaling = m_Frame.GetFov() / g_DefaultFov;

	//NOTE: m_ControlHelper has already been validated.
	float lookAroundHeadingOffset	= m_ControlHelper->GetLookAroundHeadingOffset();
	lookAroundHeadingOffset			*= fovOrientationScaling;
	float lookAroundPitchOffset		= m_ControlHelper->GetLookAroundPitchOffset();
	lookAroundPitchOffset			*= fovOrientationScaling;

	heading	+= lookAroundHeadingOffset;
	pitch	+= lookAroundPitchOffset;

	//NOTE: m_AttachParent has already been validated.
	const Matrix34 targetMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
	//if (!rage::ioHeadTracking::IsMotionTrackingEnabled())
		ApplyOrientationLimits(targetMatrix, heading, pitch);

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch);
}

void camFirstPersonAimCamera::UpdateShake()
{
	// Scale aiming shake based on player's shooting ability.
	if (m_Metadata.m_ShakeRef != 0  && !rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		camBaseFrameShaker* pFrameShaker = FindFrameShaker(m_Metadata.m_ShakeRef);
		if (pFrameShaker)
		{
			const float shootingAbility	= StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY"));
			const float amplitudeScale	= RampValueSafe(shootingAbility, m_Metadata.m_ShakeFirstPersonShootingAbilityLimits.x,
																		 m_Metadata.m_ShakeFirstPersonShootingAbilityLimits.y,
																		 m_Metadata.m_ShakeAmplitudeScalingForShootingAbilityLimits.x,
																		 m_Metadata.m_ShakeAmplitudeScalingForShootingAbilityLimits.y);
			pFrameShaker->SetAmplitude(amplitudeScale);
		}
	}
}

void camFirstPersonAimCamera::TryAndCreateOrDestroyStickyAimHelper()
{
	if (!m_StickyAimHelper && CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(camInterface::FindFollowPed()))
	{
		const camStickyAimHelperMetadata* stickyAimMetadata = camFactory::FindObjectMetadata<camStickyAimHelperMetadata>(m_Metadata.m_StickyAimHelperRef.GetHash());
		if(stickyAimMetadata)
		{
			m_StickyAimHelper = camFactory::CreateObject<camStickyAimHelper>(*stickyAimMetadata);
			cameraAssertf(m_StickyAimHelper, "A third-person aim camera (name: %s, hash: %u) failed to create a sticky aim helper (name: %s, hash: %u)",
				GetName(), GetNameHash(), SAFE_CSTRING(stickyAimMetadata->m_Name.GetCStr()), stickyAimMetadata->m_Name.GetHash());
		}	
	}
	else if (m_StickyAimHelper && !CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(camInterface::FindFollowPed()))
	{
		delete m_StickyAimHelper;
		m_StickyAimHelper = NULL;
	}
}
