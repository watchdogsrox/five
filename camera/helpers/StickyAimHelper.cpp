//
// camera/helpers/StickyAimHelper.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/StickyAimHelper.h"

#include "fwsys/timer.h"

#include "camera/CamInterface.h"
#include "camera/helpers/Envelope.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/viewports/ViewportManager.h"
#include "Peds/Ped.h" 
#include "Peds/PlayerInfo.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "Weapons/Info/WeaponInfo.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camStickyAimHelper,0xeb511f85)

camStickyAimHelper::camStickyAimHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camStickyAimHelperMetadata&>(metadata))
, m_CanUseStickyAim(false)
, m_StickyAimGroundEntity(nullptr)
, m_BlendingOutStickyAimTarget(nullptr)
, m_StickyAimTarget(nullptr)
, m_StickyReticlePosition(g_InvalidPosition)
, m_PreviousReticlePosition(g_InvalidPosition)
, m_StickyAimRotation(0.0f, 0.0f)
, m_HasStickyTarget(false)
, m_ChangedStickyTargetThisUpdate(false)
, m_IsLockedOn(false)
, m_TimeSetStickyTarget(0)
, m_LastStickyTargetTime(0)
, m_CameraRotationNoStickTime(0)
, m_PreviousStickyTargetTransformPosition(g_InvalidPosition)
, m_LookAroundInput(0.0f)
, m_StickyAimEnvelope(nullptr)
, m_StickyAimEnvelopeLevel(0.0f)
{
	ResetStickyAimParams();

	const camEnvelopeMetadata* envelopeStickyAimMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_StickyAimEnvelopeRef.GetHash());
	if(envelopeStickyAimMetadata)
	{
		m_StickyAimEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeStickyAimMetadata);
		if(cameraVerifyf(m_StickyAimEnvelope, "A sticky aim helper (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeStickyAimMetadata->m_Name.GetCStr()), envelopeStickyAimMetadata->m_Name.GetHash()))
		{
			m_StickyAimEnvelope->SetUseGameTime(true);
		}
	}
}

camStickyAimHelper::~camStickyAimHelper()
{
	if(m_StickyAimEnvelope)
	{
		delete m_StickyAimEnvelope;
	}
}

bool camStickyAimHelper::Update(const Vector3& basePivotPosition, const Vector3& baseFront, const Matrix34& worldMatrix, const bool isLockedOn, const bool isAiming)
{
	const CPed* followPed = camInterface::FindFollowPed();

	m_CanUseStickyAim = isAiming && UpdateInternal(followPed, basePivotPosition, baseFront, worldMatrix, isLockedOn);
	if (!m_CanUseStickyAim)
	{
		ResetStickyAimParams();
	}

	if (followPed)
	{
		m_StickyAimGroundEntity = followPed->GetGroundPhysical();
	}

	return m_CanUseStickyAim;
}

bool camStickyAimHelper::UpdateInternal(const CPed* followPed, const Vector3& basePivotPosition, const Vector3& baseFront, const Matrix34& worldMatrix, const bool isLockedOn)
{
	// Contains logic from RDR camStickyAimModifier::PreUpdate/camStickyAimModifier::Update.
	if (!ShouldProcessStickyAim(followPed, m_LastStickyTargetTime))
	{
		return false;
	}

	// Reset sticky aim if ground entity changes.
	CEntity* groundPhysical = followPed->GetGroundPhysical();
	if (m_StickyAimGroundEntity != groundPhysical)
	{
		return false;
	}

	m_IsLockedOn = isLockedOn;

	/* This code doesn't seem to be needed on V
	// Need to update the previous reticle position if on a ground entity.
	if (m_pGroundEnt && m_vBasePivotPositionGroundRelative != g_InvalidPosition && m_vStickyReticlePosition != g_InvalidPosition)
	{
		const CEntity* pStickyAimTarget = GetCurrentStickyAimTarget();
		if (pStickyAimTarget)
		{
			const Vec3V vCameraPosition = m_pGroundEnt->GetTransform().Transform(m_vBasePivotPositionGroundRelative).GetRough();
			const Vec3V vPreviousReticlePositionWorld = camStickyAimModifier::TransformPositionRelativeToUprightTarget(pStickyAimTarget, m_vStickyReticlePosition);
			const float fMag = (vPreviousReticlePositionWorld - vCameraPosition).Mag();
			const Vec3V vUpdatedPreviousReticlePosition = vCameraPosition + (m_vPreviousFront * fMag);
			m_vPreviousReticlePosition = camStickyAimModifier::UnTransformPositionRelativeToUprightTarget(pStickyAimTarget, vUpdatedPreviousReticlePosition);
		}
	}*/

	const bool hadStickyTarget = m_HasStickyTarget;

	m_HasStickyTarget = ComputeStickyAimTarget(followPed, basePivotPosition, baseFront, worldMatrix, m_TimeSetStickyTarget);

	// Store time we lost the sticky target (so we don't immediately re-stick, ie bounced out of the limits).
	if (hadStickyTarget && !m_HasStickyTarget)
	{
		m_LastStickyTargetTime = fwTimer::GetTimeInMilliseconds();
	}
	
	m_LookAroundInput = 0.0f;
	const CControl* pControl = followPed->GetControlFromPlayer();
	if (pControl)
	{
		// Cache time we last provided look around input.
		if (abs(pControl->GetPedAimWeaponLeftRight().GetNorm()) > SMALL_FLOAT || abs(pControl->GetPedAimWeaponUpDown().GetNorm()) > SMALL_FLOAT)
		{
			m_CameraRotationNoStickTime = fwTimer::GetTimeInMilliseconds();
		}

		// Cache look around input.
		m_LookAroundInput = Vector2(pControl->GetPedAimWeaponLeftRight().GetNorm(), -pControl->GetPedAimWeaponUpDown().GetNorm()).Mag();
	}

	UpdateStickyAimEnvelopes(followPed, worldMatrix.d, worldMatrix.b);

	const CEntity* currentStickyAimTarget = GetCurrentStickyAimTarget();
	if (!currentStickyAimTarget)
	{
		return false;
	}

	// Reset modifier if target has warped significantly in 1 frame.
	if (currentStickyAimTarget && !m_ChangedStickyTargetThisUpdate)
	{
		const float warpTolerance = m_StickyAimTarget ? m_Metadata.m_TargetWarpToleranceNormal : m_Metadata.m_TargetWarpToleranceBlendingOut;

		// Check the sticky positions.
		if (m_StickyReticlePosition != g_InvalidPosition && m_PreviousReticlePosition != g_InvalidPosition)
		{
			const Vector3 currentReticlePositionWorld = TransformPositionRelativeToUprightTarget(currentStickyAimTarget, m_StickyReticlePosition);
			const Vector3 previousReticlePositionWorld = TransformPositionRelativeToUprightTarget(currentStickyAimTarget, m_PreviousReticlePosition);
			if (!currentReticlePositionWorld.IsClose(previousReticlePositionWorld, warpTolerance))
			{
				return false;
			}
		}

		// Check the actual target positions.
		if (m_PreviousStickyTargetTransformPosition != g_InvalidPosition)
		{
			if (!VEC3V_TO_VECTOR3(currentStickyAimTarget->GetTransform().GetPosition()).IsClose(m_PreviousStickyTargetTransformPosition, warpTolerance))
			{
				return false;
			}
		}

		// CPED_RESET_FLAG_HasBeenTeleportedThisFrame doesnt exist on GTA V.
		/*// Ped is actually being teleported this frame.
		if (currentStickyAimTarget->GetIsTypePed())
		{
			const CPed* pCurrentStickyAimTargetPed = static_cast<const CPed*>(pCurrentStickyAimTarget);
			if (pCurrentStickyAimTargetPed->GetIntelligenceComponentReadOnly()->GetPedResetFlag(CPED_RESET_FLAG_HasBeenTeleportedThisFrame))
			{
				ResetStickyAimParams();
				return false;
			}
		}*/
	}

	if (currentStickyAimTarget)
	{
		m_PreviousStickyTargetTransformPosition = VEC3V_TO_VECTOR3(currentStickyAimTarget->GetTransform().GetPosition());
	}

	return true;
}

bool camStickyAimHelper::ShouldProcessStickyAim(const CPed* attachPed, const u32 lastStickyTargetTime)
{
#if __BANK
	TUNE_GROUP_BOOL(STICKY_AIM, bForceDisable, false);
	if (bForceDisable)
	{
		return false;
	}
#endif	// __BANK

	if (!attachPed || !CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(attachPed) || !attachPed->IsPlayer())
	{
		return false;
	}

#if RSG_PC
	// Don't use sticky aim if using mouse input.
	const CControl* pControl = attachPed->GetControlFromPlayer();
	if(pControl && pControl->UseRelativeLook())
	{
		return false;
	}
#endif	// RSG_PC

	// Logic below from camStickyAimModifier::CanUseStickyAiming:
	u32 uCooldownTime = fwTimer::GetTimeInMilliseconds() - m_Metadata.m_LastStickyCooldownMS;
	if (lastStickyTargetTime > uCooldownTime)
	{
		return false;
	}

	const CPlayerInfo* playerInfo = attachPed->GetPlayerInfo();
	if (playerInfo)
	{
		const CPlayerPedTargeting& playerTargeting = playerInfo->GetTargeting();
		if (playerTargeting.GetCurrentTargetingMode() == CPlayerPedTargeting::TargetingMode::None)
		{
			return false;
		}

		// CPlayerPedTargeting::GetTimeLastFreeAimingAtTargetableObject doesnt exist on GTA V, would need to add this if this code is needed.
		/*if (NetworkInterface::IsGameInProgress())
		{
			// B*5531806: Prevent sticky aim if player was recently free aiming at a targetable object in MP.
			const u32 uTimeLastFreeAimingAtTargetableObject = pPlayerTargeting->GetTimeLastFreeAimingAtTargetableObject();
			if (CTimeHelpers::GetTimeSince(uTimeLastFreeAimingAtTargetableObject) < rBaseMetadata.m_FreeAimingTargetableObjectMinTimeMS)
			{
				return false;
			}
		}*/
	}

	const CWeaponInfo* weaponInfo = attachPed->GetWeaponManager() ? attachPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if(weaponInfo)
	{
		if ((!weaponInfo->GetFlag(CWeaponInfoFlags::Gun) && !weaponInfo->GetFlag(CWeaponInfoFlags::Thrown)) || weaponInfo->GetIsMelee())
		{
			return false;
		}
	}

	if (!NetworkInterface::IsGameInProgress())
	{
		const CPlayerSpecialAbility* specialAbility = attachPed->GetSpecialAbility();
		if (specialAbility && specialAbility->IsActive())
		{
			// No sticky aim when using special abilities in single-player, it just interferes and the camera is slow enough anyway.
			return false;
		}
	}

	return true;
}

bool camStickyAimHelper::ComputeStickyAimTarget(const CPed* attachPed, const Vector3 cameraPosition, const Vector3 cameraFront, const Matrix34& mCameraWorldMat, const u32 timeSetStickyTarget)
{
	const CPlayerInfo* playerInfo = attachPed->GetPlayerInfo();
	if (playerInfo)
	{
		const CEntity* actualStickyAimTarget = GetStickyAimTarget();
		const CEntity* currentStickyAimTarget = GetCurrentStickyAimTarget();

		const CPlayerPedTargeting& playerTargeting = playerInfo->GetTargeting();
		const CEntity* prospectiveStickyTarget = playerTargeting.GetLockOnTarget() ? playerTargeting.GetLockOnTarget() : playerTargeting.GetFreeAimAssistTarget();
		if (prospectiveStickyTarget)
		{
			Vector3 prospectiveStickyPosition(Vector3::ZeroType);

			Vector3 vTargetPosToCam = VEC3V_TO_VECTOR3(prospectiveStickyTarget->GetTransform().GetPosition()) - cameraPosition;
			float fDistance = vTargetPosToCam.Mag();
			prospectiveStickyPosition = (cameraPosition + (cameraFront * fDistance));

			bool bSelectedNewTarget = false;
			const bool bTargetChanged = (currentStickyAimTarget && prospectiveStickyTarget != currentStickyAimTarget);
			if (bTargetChanged)
			{
				// Don't allow target switching for a short amount of time after setting a new target.
				const bool bCantChangeTargetDueToTimer = fwTimer::GetTimeInMilliseconds() < (timeSetStickyTarget + m_Metadata.m_TimeToSwitchStickyTargetsMS);

				bool bCantChangeTargetDueToNotProvidingAimInput = false;
				bool bCantChangeTargetDueToNotAimingTowardsNewTarget = false;
				if (actualStickyAimTarget)
				{
					const CControl* pControl = attachPed->GetControlFromPlayer();
					if (pControl)
					{
						const Vector3 vAimInput(pControl->GetPedAimWeaponLeftRight().GetNorm(), 0.0f, -pControl->GetPedAimWeaponUpDown().GetNorm());
						if (vAimInput.Mag() < SMALL_FLOAT)
						{
							// Don't allow target switching if player isn't providing any aim input at all.
							bCantChangeTargetDueToNotProvidingAimInput = true;
						}
						else 
						{
							// Don't allow target switching if player is actively trying to aim towards the existing target.
							Vector3 vAimInputNormalized = vAimInput;
							vAimInputNormalized.Normalize();
							const Vector3 vActualTargetPosition = VEC3V_TO_VECTOR3(actualStickyAimTarget->GetTransform().GetPosition());
							const Vector3 vNewTargetPosition = VEC3V_TO_VECTOR3(prospectiveStickyTarget->GetTransform().GetPosition());

							Vector3 vActualTargetPositionCamRelative = vActualTargetPosition;
							mCameraWorldMat.UnTransform(vActualTargetPositionCamRelative);
							vActualTargetPositionCamRelative.y = 0.0f;
							vActualTargetPositionCamRelative.Normalize();

							Vector3 vNewTargetPositionCamRelative = vNewTargetPosition;
							mCameraWorldMat.UnTransform(vNewTargetPositionCamRelative);
							vNewTargetPositionCamRelative.y = 0.0f;
							vNewTargetPositionCamRelative.Normalize();

							const float fDotToActual = vAimInputNormalized.Dot(vActualTargetPositionCamRelative);
							const float fDotToNew = vAimInputNormalized.Dot(vNewTargetPositionCamRelative);
							if (fDotToActual > fDotToNew)
							{
								bCantChangeTargetDueToNotAimingTowardsNewTarget = true;
							}
						}
					}
				}

				// Allow immediate switch if actualStickyAimTarget is invalid (ie we're blending out the sticky aim target).
				if (actualStickyAimTarget && (bCantChangeTargetDueToTimer || bCantChangeTargetDueToNotProvidingAimInput || bCantChangeTargetDueToNotAimingTowardsNewTarget))
				{
					// Maintain previous sticky target.
					prospectiveStickyTarget = actualStickyAimTarget;

					Vector3 vOldTargetPosToCam = VEC3V_TO_VECTOR3(prospectiveStickyTarget->GetTransform().GetPosition()) - cameraPosition;
					float fDistance = vOldTargetPosToCam.Mag();
					prospectiveStickyPosition = (cameraPosition + (cameraFront * fDistance));
				}
				else
				{
					// Reset params before setting new target (don't reset the blend envelope).
					bSelectedNewTarget = true;
					ResetStickyAimParams(false, false);
				}

			}
			else if (!actualStickyAimTarget && prospectiveStickyTarget)
			{
				bSelectedNewTarget = true;
			}

			const bool isPositionValid = IsProspectiveStickyTargetAndPositionValid(attachPed, prospectiveStickyTarget);
			if (isPositionValid)
			{
				const Vector3 stickyReticlePosition = UnTransformPositionRelativeToUprightTarget(prospectiveStickyTarget, prospectiveStickyPosition);
				SetStickyAimTargetInternal(prospectiveStickyTarget, currentStickyAimTarget, stickyReticlePosition, bSelectedNewTarget);
				return true;
			}
		}
	}

	ResetStickyAimTargetDueToFailedFindTarget();
	return false;
}

void camStickyAimHelper::ResetStickyAimTargetDueToFailedFindTarget()
{
	m_StickyAimTarget = nullptr;
	if (!m_BlendingOutStickyAimTarget)
	{
		m_StickyReticlePosition = g_InvalidPosition;
		m_PreviousReticlePosition = g_InvalidPosition;
	}
}

void camStickyAimHelper::UpdateOrientation(float& orbitHeading, float& orbitPitch, const Vector3& basePivotPosition, const Matrix34& worldMatrix)
{
	const CEntity* stickyAimTarget = GetCurrentStickyAimTarget();
	if (!m_CanUseStickyAim || !stickyAimTarget)
	{
		return;
	}

	m_StickyAimRotation.Zero();

	const CPed* followPed = camInterface::FindFollowPed();

	Vector2 vStickyAimRotation = ProcessStickyAimRotation(
		followPed
		, stickyAimTarget
		, m_StickyReticlePosition
		, m_PreviousReticlePosition
		, orbitHeading
		, orbitPitch
		, m_LookAroundInput
		, basePivotPosition
		, worldMatrix
		, m_CameraRotationNoStickTime
		, m_StickyAimCameraRotationHeadingModifierSpring
		, m_StickyAimCameraRotationPitchModifierSpring);

	if (!m_IsLockedOn)
	{
		// Apply blend level
		vStickyAimRotation *= m_StickyAimEnvelopeLevel;

		orbitHeading += vStickyAimRotation.x;
		orbitPitch += vStickyAimRotation.y;

		m_StickyAimRotation = vStickyAimRotation;
	}
}

Vector2 camStickyAimHelper::ProcessStickyAimRotation(
	const CPed* pAttachPed
	, const CEntity* pTargetEnt
	, const Vector3 vStickyReticlePosition
	, const Vector3 vPreviousStickyReticlePosition
	, const float fCurrentHeading
	, const float fCurrentPitch
	, const float fLookAroundInput
	, const Vector3 vCameraPosition
	, const Matrix34& mCameraWorldMat
	, const u32 uRotationNoStickTime
	, camDampedSpring& rRotationHeadingModifierSpring
	, camDampedSpring& rRotationPitchModifierSpring)
{
	Vector2 stickyAimRotation(0.0f, 0.0f);

	const Vector3 vCurrentPositionWorld = camStickyAimHelper::TransformPositionRelativeToUprightTarget(pTargetEnt, vStickyReticlePosition);

	if (vPreviousStickyReticlePosition != g_InvalidPosition)
	{
		const bool bIsLookingOrMoving = IsPlayerLooking(fLookAroundInput, uRotationNoStickTime) || IsPlayerMoving(pAttachPed);

		const Vector3 vPreviousReticlePositionWorld = camStickyAimHelper::TransformPositionRelativeToUprightTarget(pTargetEnt, vPreviousStickyReticlePosition);

		const Vector2 vRotationLerpModifier = camStickyAimHelper::CalculateStickyAimCameraRotationLerpModifier(
			pAttachPed
			, pTargetEnt
			, mCameraWorldMat
			, bIsLookingOrMoving
			, rRotationHeadingModifierSpring
			, rRotationPitchModifierSpring);

		const float fDistanceToTarget = (VEC3V_TO_VECTOR3(pTargetEnt->GetTransform().GetPosition())- vCameraPosition).Mag();
		const float fNormalisedDistanceToTarget = NormaliseInRange(fDistanceToTarget, m_Metadata.m_BaseRotationDistanceNear, m_Metadata.m_BaseRotationDistanceFar);
		const float fBaseRotation = rage::Lerp(fNormalisedDistanceToTarget, m_Metadata.m_BaseRotationNear, m_Metadata.m_BaseRotationFar);

		// Ramp down rotation when we hit the camera pitch limits (Americas TODO: add tunables in data).
		float fRotationPitchLimitsLerpModifier = 1.0f;
		TUNE_GROUP_BOOL(STICKY_AIM, bEnableCameraRotationPitchLimitModifier, true);
		if (bEnableCameraRotationPitchLimitModifier)
		{
			TUNE_GROUP_FLOAT(STICKY_AIM, fRotationPitchLimitsMin, 70.0f, -180.0f, 180.0f, 0.01f);
			TUNE_GROUP_FLOAT(STICKY_AIM, fRotationPitchLimitsMax, 75.0f, -180.0f, 180.0f, 0.01f);
			TUNE_GROUP_FLOAT(STICKY_AIM, fRotationPitchLimitsMinModifier, 1.0f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(STICKY_AIM, fRotationPitchLimitsMaxModifier, 0.0f, 0.0f, 1.0f, 0.01f);
			const float fCurrentPitchDegrees = fCurrentPitch * RtoD;
			const float fNormalizedPitch = NormaliseInRange(abs(fCurrentPitchDegrees), fRotationPitchLimitsMin, fRotationPitchLimitsMax);
			fRotationPitchLimitsLerpModifier = rage::Lerp(fNormalizedPitch, fRotationPitchLimitsMinModifier, fRotationPitchLimitsMaxModifier);
		}

		// Apply modifiers to default rotation lerp value.
		float fRotationHeadingLerp = (fBaseRotation * vRotationLerpModifier.x) * fRotationPitchLimitsLerpModifier;
		float fRotationPitchLerp = (fBaseRotation * vRotationLerpModifier.y) * fRotationPitchLimitsLerpModifier;

		// Never apply 100% rotation.
		fRotationHeadingLerp = rage::Clamp(fRotationHeadingLerp, 0.0f, m_Metadata.m_MaxHeadingRotationClamp);
		fRotationPitchLerp = rage::Clamp(fRotationPitchLerp, 0.0f, m_Metadata.m_MaxPitchRotationClamp);

		Vector3 vDirection(0.0f, 0.0f, 0.0f);
		camFrame::ComputeFrontFromHeadingAndPitch(fCurrentHeading, fCurrentPitch, vDirection);
		const float fCurrentMag	= Max((vCurrentPositionWorld - vCameraPosition).Mag(), 1.0f);
		const Vector3 vCurrentPosition = vCameraPosition + (vDirection * fCurrentMag);
		const Vector3 vDesiredHeadingPos = rage::Lerp(fRotationHeadingLerp, vCurrentPosition, vPreviousReticlePositionWorld);
		const Vector3 vDesiredPitchPos = rage::Lerp(fRotationPitchLerp, vCurrentPosition, vPreviousReticlePositionWorld);

		Vector3 vToDesiredHeading = (vDesiredHeadingPos - vCameraPosition);
		vToDesiredHeading.Normalize();
		Vector3 vToDesiredPitch	= (vDesiredPitchPos - vCameraPosition);
		vToDesiredPitch.Normalize();

		float fDesiredHeading = 0.0f;
		float fIgnoredPitch	= 0.0f;
		camFrame::ComputeHeadingAndPitchFromFront(vToDesiredHeading, fDesiredHeading, fIgnoredPitch);

		float fDesiredPitch = 0.0f;
		float fIgnoredHeading = 0.0f;
		camFrame::ComputeHeadingAndPitchFromFront(vToDesiredPitch, fIgnoredHeading, fDesiredPitch);

		const float fHeadingRotationToDesired = fwAngle::LimitRadianAngle(fDesiredHeading - fCurrentHeading) * m_Metadata.m_HeadingRotationWeighting;
		const float fPitchRotationToDesired = fwAngle::LimitRadianAngleForPitch(fDesiredPitch - fCurrentPitch) * m_Metadata.m_PitchRotationWeighting;

		stickyAimRotation.x = fHeadingRotationToDesired;
		stickyAimRotation.y = fPitchRotationToDesired;

#if __BANK
		TUNE_GROUP_BOOL(STICKY_AIM, bRenderRotationDebug, false);
		if (bRenderRotationDebug)
		{
			grcDebugDraw::Sphere(vCurrentPosition, 0.25f, Color_green, false);
			grcDebugDraw::Sphere(vPreviousReticlePositionWorld, 0.25f, Color_red, false);
			Vector3 vDesiredPositionWorld(vDesiredHeadingPos.x, vDesiredHeadingPos.y, vDesiredPitchPos.z);
			grcDebugDraw::Sphere(vDesiredPositionWorld, 0.25f, Color_purple, false);
		}
#endif	// __BANK
	}

	return stickyAimRotation;
}

Vector2 camStickyAimHelper::CalculateStickyAimCameraRotationLerpModifier(
  const CPed* attachPed 
, const CEntity* targetEntity
, const Matrix34& mCameraWorldMat 
, const bool bIsPlayerLookingOrMoving 
, camDampedSpring& rRotationHeadingModifierSpring 
, camDampedSpring& rRotationPitchModifierSpring)
{
	Vector2 vRotationLerpModifier(1.0f, 1.0f);


	// Calculate distance modifier.
	const Vector3 vTargetPosition = VEC3V_TO_VECTOR3(targetEntity->GetTransform().GetPosition());
	const float fDistanceToTarget = Mag(attachPed->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(vTargetPosition)).Getf();

	float fDistanceModifier = 1.0f;
	float fDistanceScale = 1.0f;
	if (fDistanceToTarget < m_Metadata.m_MaxStickyAimDistanceClose)
	{
		// "Close" distance modifier.
		fDistanceScale = Min(fDistanceToTarget / m_Metadata.m_MaxStickyAimDistanceClose, 1.0f);
		fDistanceModifier = Lerp(fDistanceScale, m_Metadata.m_RotationDistanceModifierClose, m_Metadata.m_RotationDistanceModifierMin);
	}
	else
	{
		// Normal distance modifier.
		fDistanceScale = Min((fDistanceToTarget - m_Metadata.m_MaxStickyAimDistanceClose) / (m_Metadata.m_MaxStickyAimDistance - m_Metadata.m_MaxStickyAimDistanceClose), 1.0f);
		fDistanceModifier = Lerp(fDistanceScale, m_Metadata.m_RotationDistanceModifierMin, m_Metadata.m_RotationDistanceModifierMax);
	}

	// Calculate relative velocity modifier.
	float fRelativeVelocityModifier = 1.0f;
	if (targetEntity->GetIsPhysical())
	{
		const CPhysical* targetPhysical = static_cast<const CPhysical*>(targetEntity);
		Vector3 vTargetVelocity = targetPhysical->GetVelocity();
		mCameraWorldMat.UnTransform3x3(vTargetVelocity);

		Vector3 vAttachPedVelocity = attachPed->GetVelocity();
		mCameraWorldMat.UnTransform3x3(vAttachPedVelocity);

		float fVelScale = Abs(vTargetVelocity.x - vAttachPedVelocity.x) / m_Metadata.m_MaxRelativeVelocity;
		if (fVelScale <= 1.0f)
		{
			fVelScale = Clamp(fVelScale, 0.0f, 1.0f);
			fRelativeVelocityModifier = Lerp(fVelScale, m_Metadata.m_RotationRelativeVelocityModifierMin, m_Metadata.m_RotationRelativeVelocityModifierMax);
		}
		else
		{
			fVelScale = Clamp((m_Metadata.m_MaxRelativeVelocity - fVelScale) / m_Metadata.m_MaxRelativeVelocity, 0.0f, 1.0f);
			fRelativeVelocityModifier = Lerp(fVelScale, m_Metadata.m_RotationRelativeVelocityModifierMin, m_Metadata.m_RotationRelativeVelocityModifierMax);
		}
	}

	Vector2 vTargetPositionScreenSpace = camStickyAimHelper::TransformWorldToScreen(vTargetPosition);
	Vector2 vReticlePositionScreen(0.5f, 0.5f);	//uiReticleHelper::GetReticle().GetScreenPosition();
	// We only care about heading differences here, so move the target Y position to match the reticle position.
	vTargetPositionScreenSpace.y = vReticlePositionScreen.y; 

	// Ramp up rotation when the reticle is close to the centre of the target (ie within the target's capsule).
	float fReticleProximityModifier = 1.0f;
	if (m_Metadata.m_EnableReticleDistanceRotationModifier)
	{
		const float fReticleDistanceThreshold = rage::Lerp(fDistanceScale, m_Metadata.m_RotationReticleDistanceNearThreshold, m_Metadata.m_RotationReticleDistanceFarThreshold);
		if (fReticleDistanceThreshold > 0.0f)
		{
			const float fReticleDistanceModifierMax = rage::Lerp(fDistanceScale, m_Metadata.m_RotationReticleDistanceModifierMax_Near, m_Metadata.m_RotationReticleDistanceModifierMax_Far);
			const float fReticleDistanceToPed = abs(vTargetPositionScreenSpace.x - vReticlePositionScreen.x);
			const float tValue = rage::Clamp(1.0f - (fReticleDistanceToPed / fReticleDistanceThreshold), 0.0f, 1.0f);
			fReticleProximityModifier = rage::Lerp(tValue, m_Metadata.m_RotationReticleDistanceModifierMin, fReticleDistanceModifierMax);
		}
	}

	// Ramp up rotation when the reticle is being pushed towards the target.
	float fInputDirectionModifier = 1.0f;
	if (m_Metadata.m_EnableInputDirectionModifier)
	{
		const CControl* pControl = attachPed->GetControlFromPlayer();
		if (pControl)
		{
			Vector3 vAimInput(pControl->GetPedAimWeaponLeftRight().GetNorm(), -pControl->GetPedAimWeaponUpDown().GetNorm(), 0.0f);
			if (vAimInput.Mag() > SMALL_FLOAT)
			{
				const Vector2 vReticleToTarget2D = vTargetPositionScreenSpace - vReticlePositionScreen;
				Vector3 vReticleToTarget(vReticleToTarget2D.x, vReticleToTarget2D.y, 0.0f);
				vReticleToTarget.Normalize();
				vAimInput.Normalize();

				const float fAimDotX = vAimInput.x * vReticleToTarget.x;
				if (fAimDotX > m_Metadata.m_RotationInputDirectionMinDot)
				{
					const float fAimDotScale = NormaliseInRange(fAimDotX, m_Metadata.m_RotationInputDirectionMinDot, 1.0f);
					fInputDirectionModifier = Lerp(fAimDotScale, m_Metadata.m_RotationInputDirectionModifierMin, m_Metadata.m_RotationInputDirectionModifierMax);
				}
			}
		}
	}

	// Ramp down rotation when the target is moving into the reticle.
	float fTargetMovingIntoReticleModifier = 1.0f;
	if (m_Metadata.m_EnableTargetMovingIntoReticleModifier && targetEntity->GetIsPhysical())
	{
		const CPhysical* targetPhysical = static_cast<const CPhysical*>(targetEntity);
		const Vector3 vTargetVelocity = targetPhysical->GetVelocity();
		const float fVelocity = vTargetVelocity.Mag();
		if (fVelocity > SMALL_FLOAT)
		{
			const float fCameraDistanceToTarget = (mCameraWorldMat.d - vTargetPosition).Mag();
			Vector3 vTargetVelocityNormalized = vTargetVelocity;
			vTargetVelocityNormalized.Normalize();
			const Vector3 vCurrentReticlePosition = mCameraWorldMat.d + (mCameraWorldMat.b * fCameraDistanceToTarget);
			Vector3 vTargetToReticlePosition = vTargetPosition - vCurrentReticlePosition;
			vTargetToReticlePosition.z = 0.0f;	// Ignore the Z delta.
			Vector3 vTargetToReticlePositionNormalized = vTargetToReticlePosition;
			vTargetToReticlePositionNormalized.Normalize();

			const float fDot = (vTargetToReticlePositionNormalized.Dot(vTargetVelocityNormalized));
			if (fDot < 0.0f)
			{
				const float fTargetMovementDotScale = NormaliseInRange(fDot, -1.0f, 0.0f);
				fTargetMovingIntoReticleModifier = Lerp(fTargetMovementDotScale, m_Metadata.m_TargetMovingIntoReticleModifierMin, m_Metadata.m_TargetMovingIntoReticleModifierMax);
			}
		}
	}

	float fNotLookingOrMovingModifier = 1.0f;
	if (m_Metadata.m_EnableNotLookingOrMovingModifier && !bIsPlayerLookingOrMoving)
	{
		fNotLookingOrMovingModifier = m_Metadata.m_NotLookingOrMovingModifier;
	}

	float fTargetVaultingPitchModifier = 1.0f;
	if (targetEntity->GetIsTypePed())
	{
		const CPed* targetPed = static_cast<const CPed*>(targetEntity);
		if (targetPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
		{
			fTargetVaultingPitchModifier = m_Metadata.m_TargetVaultingPitchModifier;
		}
	}

	vRotationLerpModifier *= fDistanceModifier;
	vRotationLerpModifier *= fRelativeVelocityModifier;
	vRotationLerpModifier *= fReticleProximityModifier;
	vRotationLerpModifier *= fInputDirectionModifier;
	vRotationLerpModifier *= fTargetMovingIntoReticleModifier;
	//vRotationLerpModifier *= fNotLookingOrMovingModifier;

	// Apply Heading/Pitch-specific modifiers.
	vRotationLerpModifier.y *= fTargetVaultingPitchModifier;

	// Dampen the rotation modifiers.
	if (m_Metadata.m_DampenRotationLerpModifiers)
	{
		rRotationHeadingModifierSpring.Update(vRotationLerpModifier.x, m_Metadata.m_RotationLerpModifierSpringConstant, m_Metadata.m_RotationLerpModifierSpringDampingRatio);
		rRotationPitchModifierSpring.Update(vRotationLerpModifier.y, m_Metadata.m_RotationLerpModifierSpringConstant, m_Metadata.m_RotationLerpModifierSpringDampingRatio);
		vRotationLerpModifier.x = rRotationHeadingModifierSpring.GetResult();
		vRotationLerpModifier.y = rRotationPitchModifierSpring.GetResult();
	}
	return vRotationLerpModifier;
}

bool camStickyAimHelper::CanApplyCameraRotation(const CPed* attachPed, const u32 rotationNoStickTime, const float lookAroundInput)
{
	const bool bIsLooking = m_Metadata.m_RequiresLookAroundInput ? IsPlayerLooking(lookAroundInput, rotationNoStickTime) : true;
	const bool bPlayerIsMoving = m_Metadata.m_RequiresPlayerMovement ? IsPlayerMoving(attachPed) : true;
	const bool bCanApplCameraRotation = m_Metadata.m_RequiresEitherLookAroundOrPlayerMovement ? (bPlayerIsMoving || bIsLooking) : (bIsLooking && bPlayerIsMoving);
	return bCanApplCameraRotation;
}

bool camStickyAimHelper::IsPlayerLooking(const float lookAroundInput, const u32 rotationNoStickTime)
{
	return (Abs(lookAroundInput) > SMALL_FLOAT || (rotationNoStickTime + m_Metadata.m_RotationNoStickTimeMS > fwTimer::GetTimeInMilliseconds()));
}

bool camStickyAimHelper::IsPlayerMoving(const CPed* attachPed)
{
	if (attachPed)
	{
		const CControl* pControl = attachPed->GetControlFromPlayer();
		if (pControl)
		{
			const float fPlayerVelocity = MagXY(Subtract(VECTOR3_TO_VEC3V(attachPed->GetVelocity()), attachPed->GetGroundVelocity())).Getf();
			if (abs(pControl->GetPedWalkLeftRight().GetNorm()) < SMALL_FLOAT && (fPlayerVelocity < m_Metadata.m_VelThresholdForPlayerMovement))
			{
				return false;
			}

			return true;
		}
	}

	return false;
}

float camStickyAimHelper::NormaliseInRange(const float fIn, const float fMin, const float fMax)
{
	const float fDelta		= fMax - fMin;
	const float fRelative	= fIn - fMin;
	const float fNormalised = ( ( fDelta != 0.0f ) && ( fRelative != 0.0f ) ) ? Clamp ( fRelative / fDelta, 0.0f, 1.0f ): 0.0f;
	return fNormalised;
}

Vector2 camStickyAimHelper::TransformWorldToScreen(Vector3 worldPosition)
{
	const CViewport* pGameViewport = gVpMan.GetGameViewport();
	rage::grcViewport viewportCopy = pGameViewport->GetGrcViewport();

	Vector3 screenPosition = VEC3V_TO_VECTOR3(viewportCopy.Transform(VECTOR3_TO_VEC3V(worldPosition)));

	//use VideoResManager screen size so we can handle non-native 4K resolutions (aka PS4 checkered rendering)
	const float displayWidth = static_cast<float>(VideoResManager::GetSceneWidth());
	const float displayHeight = static_cast<float>(VideoResManager::GetSceneHeight());

	const Vector3 windowSizeInv(1.0f / displayWidth, 1.0f / displayHeight, 1.0f);
	screenPosition *= windowSizeInv;

	const Vector3 cameraFront = camInterface::GetFront();

	Vector3 worldPositionToCam = worldPosition - camInterface::GetPos();
	worldPositionToCam.NormalizeSafe(-cameraFront); //pick up the inverse of the camera front so we end up with a negative dot product

	//store the dot product in the z component, we can use this information to immediately figure out
	//if a point is behind of the camera.
	screenPosition.z = worldPositionToCam.Dot(cameraFront);

	return Vector2(screenPosition.x, screenPosition.y);
}

void camStickyAimHelper::ResetStickyAimRotationParams(bool bResetPreviousPosition)
{
	m_StickyReticlePosition = g_InvalidPosition;

	if (bResetPreviousPosition)
	{
		m_PreviousReticlePosition = g_InvalidPosition;

		// Only reset spring if we're resetting the previous position (ie we're not switching targets).
		m_StickyAimCameraRotationHeadingModifierSpring.Reset();
		m_StickyAimCameraRotationPitchModifierSpring.Reset();
	}

	m_StickyAimRotation.Zero();
	m_CameraRotationNoStickTime = 0;	
}

void camStickyAimHelper::PostCameraUpdate(const Vector3& basePivotPosition, const Vector3& baseFront)
{
	if (!m_CanUseStickyAim)
	{
		return;
	}

	const CEntity* stickyAimTarget = GetCurrentStickyAimTarget();
	if (stickyAimTarget)
	{
		// This code doesn't seem to be needed on V
		/*if (m_GroundEnt)
		{
			// Cache base pivot position relative to ground entity. Needed in Update to cache accurate m_vPreviousReticlePosition.
			Vec3V vBasePivotPositionGroundRelative = m_pSourceCamera->GetBasePivotPosition();
			m_pGroundEnt->GetTransform().UnTransform(vBasePivotPositionGroundRelative, vBasePivotPositionGroundRelative);
			m_vBasePivotPositionGroundRelative = vBasePivotPositionGroundRelative;
		}*/

		if (m_StickyReticlePosition != g_InvalidPosition)
		{
			const Vector3 vCameraPosition = basePivotPosition;
			const Vector3 vPreviousReticlePositionWorld = camStickyAimHelper::TransformPositionRelativeToUprightTarget(stickyAimTarget, m_StickyReticlePosition);
			const float fMag = (vPreviousReticlePositionWorld - vCameraPosition).Mag();
			const Vector3 updatedPreviousReticlePosition = vCameraPosition + (baseFront * fMag);
			m_PreviousReticlePosition = camStickyAimHelper::UnTransformPositionRelativeToUprightTarget(stickyAimTarget, updatedPreviousReticlePosition);
		}
	}
}

void camStickyAimHelper::FixupPositionOffsetsFromRagdoll(const CPed* targetPed, const Vector3& vPrevPosition, const Vector3& vNewPosition)
{
	const CEntity* currentStickyAimTarget = GetCurrentStickyAimTarget();
	if (currentStickyAimTarget && currentStickyAimTarget->GetIsTypePed())
	{
		const CPed* currentStickyAimTargetPed = static_cast<const CPed*>(currentStickyAimTarget);
		if (currentStickyAimTargetPed == targetPed)
		{
			// Don't need to do this if using the pelvis position.
			if (!ShouldUsePelvisPositionForLocalPositionCaching(targetPed))
			{
				Matrix34 mPrevMatUpdated;
				mPrevMatUpdated.Identity();
				mPrevMatUpdated.d = vPrevPosition;
				Matrix34 mNewMatUpdated;
				mNewMatUpdated.Identity();
				mNewMatUpdated.d = vNewPosition;

				if (m_StickyReticlePosition != g_InvalidPosition)
				{
					Vector3 vStickyReticlePositionWorld = m_StickyReticlePosition;
					mPrevMatUpdated.Transform(vStickyReticlePositionWorld);
					mNewMatUpdated.UnTransform(vStickyReticlePositionWorld, m_StickyReticlePosition);
				}

				if (m_PreviousReticlePosition != g_InvalidPosition)
				{
					Vector3 vPreviousReticlePositionWorld = m_PreviousReticlePosition;
					mPrevMatUpdated.Transform(vPreviousReticlePositionWorld);
					mNewMatUpdated.UnTransform(vPreviousReticlePositionWorld, m_PreviousReticlePosition);
				}
			}
		}
	}
}

bool camStickyAimHelper::IsProspectiveStickyTargetAndPositionValid(const CPed* attachPed, const CEntity* prospectiveTarget)
{
	if (!prospectiveTarget)
	{
		return false;
	}

	if (!attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) && !attachPed->GetPedResetFlag( CPED_RESET_FLAG_IsAiming))
	{
		return false;
	}

	const CPed* targetPed = nullptr;
	if (prospectiveTarget->GetIsTypePed())
	{
		targetPed = static_cast<const CPed*>(prospectiveTarget);
	}

	if (targetPed)
	{
		if (targetPed->GetIsInVehicle() && !targetPed->IsCollisionEnabled())
		{
			return false;
		}		
	}

	// Check normal targeting lock-on behaviour. E.g. is dead, friendly etc to prevent sticky aim against these targets.
	const u64 targetFlags = CPedTargetEvaluator::DEFAULT_TARGET_FLAGS | CPedTargetEvaluator::TS_ALLOW_LOCKON_EVEN_IF_DISABLED;
	CPedTargetEvaluator::tFindTargetParams findTargetTestParams;
	if(!CPedTargetEvaluator::CheckBasicTargetExclusions(attachPed, prospectiveTarget, targetFlags, m_Metadata.m_MaxStickyAimDistance, m_Metadata.m_MaxStickyAimDistance))
	{
		return false;
	}

	return true;
}

void camStickyAimHelper::SetStickyAimTargetInternal(const CEntity* newStickyAimTarget, const CEntity* previousStickyAimTarget, const Vector3 stickyReticlePosition, const bool selectedNewTarget)
{
	// Swapped to a new target; update m_vPreviousReticlePosition to be relative to the new target (if possible).
	if (selectedNewTarget)
	{
		m_TimeSetStickyTarget = fwTimer::GetTimeInMilliseconds();

		if (previousStickyAimTarget)
		{
			m_ChangedStickyTargetThisUpdate = true;

			if (m_PreviousReticlePosition != g_InvalidPosition)
			{
				const Vector3 vPreviousReticlePositionWorld = TransformPositionRelativeToUprightTarget(previousStickyAimTarget, m_PreviousReticlePosition);
				m_PreviousReticlePosition = UnTransformPositionRelativeToUprightTarget(newStickyAimTarget, vPreviousReticlePositionWorld);
			}
			else
			{
				m_PreviousReticlePosition = stickyReticlePosition;
			}
		}
	}

	m_StickyReticlePosition = stickyReticlePosition;
	m_StickyAimTarget = newStickyAimTarget;
	m_BlendingOutStickyAimTarget = newStickyAimTarget;
}

void camStickyAimHelper::UpdateStickyAimEnvelopes(const CPed* attachPed, const Vector3& cameraPosition, const Vector3& cameraFront)
{
	if(m_StickyAimEnvelope)
	{
		const bool bShouldStart = m_HasStickyTarget && CanApplyCameraRotation(attachPed, m_CameraRotationNoStickTime, m_LookAroundInput);

		if (m_IsLockedOn && bShouldStart)
		{
			m_StickyAimEnvelope->Start(1.0f);
		}

		m_StickyAimEnvelope->AutoStartStop(bShouldStart);

		m_StickyAimEnvelopeLevel = m_StickyAimEnvelope->Update();
		m_StickyAimEnvelopeLevel = SlowInOut(m_StickyAimEnvelopeLevel);
	}
	else
	{
		m_StickyAimEnvelopeLevel = m_StickyAimEnvelope ? 1.0f : 0.0f;
	}

	// Clear blend out target when envelope is blended out.
	if (!m_HasStickyTarget && m_StickyAimEnvelopeLevel < SMALL_FLOAT)
	{
		m_BlendingOutStickyAimTarget = nullptr;
	}

	// Clear blend out target if it goes behind the camera.
	if (!m_StickyAimTarget && m_BlendingOutStickyAimTarget)
	{
		Vector3 vCameraToTarget = (VEC3V_TO_VECTOR3(m_BlendingOutStickyAimTarget->GetTransform().GetPosition()) - cameraPosition);
		vCameraToTarget.Normalize();
		const float fDot = vCameraToTarget.Dot(cameraFront);
		if (fDot < 0.0f)
		{
			m_BlendingOutStickyAimTarget = nullptr;
		}
	}
}

const CEntity* camStickyAimHelper::GetCurrentStickyAimTarget() const
{
	if (m_StickyAimTarget)
	{
		return m_StickyAimTarget;
	}
	else if (m_BlendingOutStickyAimTarget)
	{
		return m_BlendingOutStickyAimTarget;
	}

	return nullptr;
}

void camStickyAimHelper::ResetStickyAimParams(bool bResetEnvelopeAndCamera, bool bResetPreviousPosition)
{
	m_StickyAimTarget = nullptr;
	m_StickyAimGroundEntity = nullptr;

	m_HasStickyTarget = false;
	m_ChangedStickyTargetThisUpdate = false;
	m_IsLockedOn = false;

	m_TimeSetStickyTarget = 0;
	m_LastStickyTargetTime = 0;

	m_PreviousStickyTargetTransformPosition = g_InvalidPosition;

	m_LookAroundInput = 0.0f;

	if (bResetEnvelopeAndCamera)
	{
		if (m_StickyAimEnvelope)
		{
			m_StickyAimEnvelope->Stop();
		}

		m_StickyAimEnvelopeLevel = 0.0f;
	}

	ResetStickyAimRotationParams(bResetPreviousPosition);
}

Vector3 camStickyAimHelper::TransformPositionRelativeToUprightTarget(const CEntity* stickyTarget, const Vector3 position)
{
	Vector3 transformedPosition = position;
	if (stickyTarget)
	{
		Matrix34 mTransformMat;
		mTransformMat.Identity();
		mTransformMat.d = VEC3V_TO_VECTOR3(stickyTarget->GetTransform().GetPosition());

		if (ShouldUsePelvisPositionForLocalPositionCaching(stickyTarget))
		{
			const CPed* stickyTargetPed = static_cast<const CPed*>(stickyTarget);
			Matrix34 mWorldBoneMatrix;
			stickyTargetPed->GetGlobalMtx(stickyTargetPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);
			mTransformMat.d = mWorldBoneMatrix.d;
		}

		mTransformMat.Transform(position, transformedPosition);
	}

	return transformedPosition;
}

Vector3 camStickyAimHelper::UnTransformPositionRelativeToUprightTarget(const CEntity* stickyTarget, const Vector3 position)
{
	Vector3 unTransformedPosition = position;
	if (stickyTarget)
	{
		Matrix34 mTransformMat;
		mTransformMat.Identity();
		mTransformMat.d = VEC3V_TO_VECTOR3(stickyTarget->GetTransform().GetPosition());

		if (ShouldUsePelvisPositionForLocalPositionCaching(stickyTarget))
		{
			const CPed* stickyTargetPed = static_cast<const CPed*>(stickyTarget);
			Matrix34 mWorldBoneMatrix;
			stickyTargetPed->GetGlobalMtx(stickyTargetPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);
			mTransformMat.d = mWorldBoneMatrix.d;
		}

		mTransformMat.UnTransform(position, unTransformedPosition);
	}

	return unTransformedPosition;

}

bool camStickyAimHelper::ShouldUsePelvisPositionForLocalPositionCaching(const CEntity* stickyTarget)
{
	if (stickyTarget && stickyTarget->GetIsTypePed())
	{
		const CPed* stickyTargetPed = static_cast<const CPed*>(stickyTarget);

		// Target is climbing (so the mover is offset), use the target's pelvis position.
		if (stickyTargetPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
		{
			return true;
		}
	}

	return false;
}