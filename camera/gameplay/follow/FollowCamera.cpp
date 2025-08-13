//
// camera/gameplay/follow/FollowCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/follow/FollowCamera.h"

#include "profile/group.h"
#include "profile/page.h"

#include "fwmaths/angle.h"
#include "fwsys/timer.h"
#include "fwmaths/Vector.h"

#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "peds/PedIntelligence.h"
#include "scene/Physical.h"
#include "scene/world/GameWorldHeightMap.h"
#include "system/EndUserBenchmark.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFollowCamera,0xEDB4063)

PF_PAGE(camFollowCameraPage, "Camera: Follow Camera");

PF_GROUP(camFollowCameraMetrics);
PF_LINK(camFollowCameraPage, camFollowCameraMetrics);

PF_VALUE_FLOAT(desiredPullAroundHeading, camFollowCameraMetrics);
PF_VALUE_FLOAT(dampedPullAroundHeading, camFollowCameraMetrics);
PF_VALUE_FLOAT(desiredPullAroundPitch, camFollowCameraMetrics);
PF_VALUE_FLOAT(dampedPullAroundPitch, camFollowCameraMetrics);

PF_VALUE_FLOAT(upwardSpeedScalingOnGroundBlendLevel, camFollowCameraMetrics);
PF_VALUE_FLOAT(upwardSpeedScalingInAirBlendLevel, camFollowCameraMetrics);

const float g_DofFNumberOfLensForDeathFailEffect	= 16.0f;
const float g_DofSubjectMagPowerForDeathFailEffect	= 0.7f;


camFollowCamera::camFollowCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonCamera(metadata)
, m_Metadata(static_cast<const camFollowCameraMetadata&>(metadata))
, m_AttachParentInAirEnvelope(NULL)
, m_AttachParentUpwardSpeedScalingOnGroundEnvelope(NULL)
, m_AttachParentUpwardSpeedScalingInAirEnvelope(NULL)
, m_AimBehaviourEnvelope(NULL)
, m_AttachParentInAirBlendLevel(0.0f)
, m_AttachParentUpwardSpeedScalingOnGroundBlendLevel(0.0f)
, m_AttachParentUpwardSpeedScalingInAirBlendLevel(0.0f)
, m_AimBehaviourBlendLevel(0.0)
, m_ShouldIgnoreAttachParentMovementForOrientation(m_Metadata.m_ShouldIgnoreAttachParentMovementForOrientation)
, m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate(false)
, m_ShouldCacheOrientationForTurretOnPreviousUpdate(false)
, m_CachedHeadingForTurret(0.0f)
, m_CachedPitchForTurret(0.0f)
{
	const camEnvelopeMetadata* envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_AttachParentInAirEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_AttachParentInAirEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_AttachParentInAirEnvelope, "A follow camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_AttachParentInAirEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_AttachParentUpwardSpeedScalingOnGroundEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_AttachParentUpwardSpeedScalingOnGroundEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_AttachParentUpwardSpeedScalingOnGroundEnvelope,
			"A follow camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)", GetName(), GetNameHash(),
			SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_AttachParentUpwardSpeedScalingOnGroundEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_AttachParentUpwardSpeedScalingInAirEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_AttachParentUpwardSpeedScalingInAirEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_AttachParentUpwardSpeedScalingInAirEnvelope,
			"A follow camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)", GetName(), GetNameHash(),
			SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_AttachParentUpwardSpeedScalingInAirEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_AimBehaviourEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_AimBehaviourEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_AimBehaviourEnvelope,
			"A follow camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)", GetName(), GetNameHash(),
			SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_AimBehaviourEnvelope->SetUseGameTime(true);
		}
	}
}

camFollowCamera::~camFollowCamera()
{
	if(m_AttachParentInAirEnvelope)
	{
		delete m_AttachParentInAirEnvelope;
	}

	if(m_AttachParentUpwardSpeedScalingOnGroundEnvelope)
	{
		delete m_AttachParentUpwardSpeedScalingOnGroundEnvelope;
	}

	if(m_AttachParentUpwardSpeedScalingInAirEnvelope)
	{
		delete m_AttachParentUpwardSpeedScalingInAirEnvelope;
	}

	if(m_AimBehaviourEnvelope)
	{
		delete m_AimBehaviourEnvelope;
	}
}

void camFollowCamera::PersistAimBehaviour()
{
	if(m_AimBehaviourEnvelope)
	{
		m_AimBehaviourEnvelope->Start(1.0f);
	}
}

bool camFollowCamera::Update()
{
	UpdateAttachParentInAirBlendLevel();

	UpdateAimBehaviourBlendLevel();

	const bool hasSucceeded = camThirdPersonCamera::Update();

	UpdateWaterBobShake();

	m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate = false;

	return hasSucceeded;
}

void camFollowCamera::UpdateAttachParentInAirBlendLevel()
{
	const bool isAttachParentInAir = ComputeIsAttachParentInAir();

	if(m_AttachParentInAirEnvelope)
	{
		if(m_NumUpdatesPerformed == 0)
		{
			if(isAttachParentInAir)
			{
				m_AttachParentInAirEnvelope->Start(1.0f);
			}
			else
			{
				m_AttachParentInAirEnvelope->Stop();
			}
		}
		else
		{
			m_AttachParentInAirEnvelope->AutoStartStop(isAttachParentInAir);
		}

		m_AttachParentInAirBlendLevel = m_AttachParentInAirEnvelope->Update();
		m_AttachParentInAirBlendLevel = Clamp(m_AttachParentInAirBlendLevel, 0.0f, 1.0f);
		m_AttachParentInAirBlendLevel = SlowInOut(m_AttachParentInAirBlendLevel);
	}
	else
	{
		//We don't have an envelope, so just use the input state directly.
		m_AttachParentInAirBlendLevel = isAttachParentInAir ? 1.0f : 0.0f;
	}
}

void camFollowCamera::UpdateAimBehaviourBlendLevel()
{
	if(m_AimBehaviourEnvelope)
	{
		m_AimBehaviourBlendLevel = m_AimBehaviourEnvelope->Update();
		m_AimBehaviourBlendLevel = Clamp(m_AimBehaviourBlendLevel, 0.0f, 1.0f);
	}
	else
	{
		m_AimBehaviourBlendLevel = 0.0f;
	}
}

void camFollowCamera::UpdateWaterBobShake()
{
	u32 shakeWaterBobHash = m_Metadata.m_WaterBobShakeRef;
	if (shakeWaterBobHash == 0)
	{
		return;
	}

	bool bShouldApplyShake = !IsBuoyant();

	camBaseFrameShaker* pWaterBobber = FindFrameShaker(shakeWaterBobHash);
	if (!pWaterBobber && bShouldApplyShake)
	{
		pWaterBobber = Shake(shakeWaterBobHash, 1.0f);
	}
	else if (pWaterBobber)
	{
		if (bShouldApplyShake)
		{
			// TODO: scale amplitude based on water depth.
		}
		else
		{
			pWaterBobber->Release();
		}
	}
}

float camFollowCamera::ComputeAttachParentRollForBasePivotPosition()
{
	float attachParentRollToApply = camThirdPersonCamera::ComputeAttachParentRollForBasePivotPosition();

	//Blend out the roll when the attach parent is in the air, in order to avoid eccentric camera behaviour.
	attachParentRollToApply *= (1.0f - m_AttachParentInAirBlendLevel);

	return attachParentRollToApply;
}

void camFollowCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	camThirdPersonCamera::ComputeOrbitOrientation(orbitHeading, orbitPitch);

	//NOTE: We must update the blend levels for upward speed scaling whether or not we are ignoring parent movement, as the envelope for this
	//effect must be kept up to date.
	UpdateAttachParentUpwardSpeedScalingOnGroundBlendLevel();
	UpdateAttachParentUpwardSpeedScalingInAirBlendLevel();

	const s32 vehicleEntryExitState					= camInterface::GetGameplayDirector().GetVehicleEntryExitState();
	const bool isFollowPedEnteringOrExitingVehicle	= (vehicleEntryExitState == camGameplayDirector::ENTERING_VEHICLE) ||
														(vehicleEntryExitState == camGameplayDirector::EXITING_VEHICLE);

	const bool isFollowPedEnteringHeliTurret		= camInterface::GetGameplayDirector().GetFollowPed() && 
														camInterface::GetGameplayDirector().GetFollowVehicle() &&
														camInterface::GetGameplayDirector().GetFollowVehicle()->InheritsFromHeli() &&
														camInterface::GetGameplayDirector().IsPedInOrEnteringTurretSeat(
															camInterface::GetGameplayDirector().GetFollowVehicle(),
															*camInterface::GetGameplayDirector().GetFollowPed());

	const bool isRidingMinitank = camInterface::GetGameplayDirector().IsAttachVehicleMiniTank();

	const CPed* pFollowPed = camInterface::GetGameplayDirector().GetFollowPed();
	const CPedWeaponManager* pWeaponManager = pFollowPed ? pFollowPed->GetWeaponManager() : NULL;
	const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager ? pWeaponManager->GetEquippedWeaponInfo() : NULL;
	const bool bDisablePullAroundCameraForWeapon = (pEquippedWeaponInfo && pEquippedWeaponInfo->GetDisableCameraPullAround()) || isRidingMinitank;

	//NOTE: Parent movement is explicitly ignored when the follow ped is entering or exiting a vehicle.
	if(!m_ShouldIgnoreAttachParentMovementForOrientation && !m_ShouldIgnoreAttachParentMovementForOrientationThisUpdate && !bDisablePullAroundCameraForWeapon &&
		(!isFollowPedEnteringOrExitingVehicle || isFollowPedEnteringHeliTurret))
	{
		const float initialOrbitHeading	= orbitHeading;
		const float initialOrbitPitch	= orbitPitch;

		//NOTE: m_ControlHelper has already been validated.
		const bool isLookingBehind		= m_ControlHelper->IsLookingBehind();
		const bool isViewModeBlending	= m_ControlHelper->IsViewModeBlending();

		//NOTE: We bypass the basic follow behaviour when looking behind and apply only pull-around, which is tailored to look-behind.
		//NOTE: We also bypass the basic follow behaviour when blending between view modes, in order to observe the full change in framing.
		if( !isLookingBehind && 
	#if RSG_PC
			!EndUserBenchmarks::IsBenchmarkScriptRunning() &&
	#endif
			!isViewModeBlending )
		{
			Vector3 baseFollowPositionForFront = m_BasePosition;

			float lookAroundInputEnvelopeLevel = m_ControlHelper->GetLookAroundInputEnvelopeLevel();

			//Limit the speed of the attach parent when computing the follow front, so that the follow orientation speed does not increase indefinitely.
			const float attachParentSpeed			= m_BaseAttachVelocityToConsider.Mag();
			float desiredAttachParentSpeed			= Min(attachParentSpeed, m_Metadata.m_MaxMoveSpeedForFollowOrientation);
			//Apply a multiplier to the attach parent speed when looking around, as this effectively increases the look-around limits w.r.t
			//the direction of movement.
			float lookAroundParentSpeedMultiplier	= RampValue(lookAroundInputEnvelopeLevel, 0.0f, 1.0f, 1.0f, m_Metadata.m_MaxLookAroundMoveSpeedMultiplier);
			desiredAttachParentSpeed				*= lookAroundParentSpeedMultiplier;

			float attachParentSpeedDelta			= attachParentSpeed - desiredAttachParentSpeed;
			Vector3 attachParentMoveDirection(m_BaseAttachVelocityToConsider);
			attachParentMoveDirection.NormalizeSafe();
			Vector3 attachParentVelocityDelta		= attachParentMoveDirection * attachParentSpeedDelta;

			//Additionally, scale the effect of any vertical movement on the follow behaviour of the camera.
			Vector3 desiredAttachParentVelocity		= attachParentMoveDirection * desiredAttachParentSpeed;
			const float verticalMoveSpeedToIgnore	= ComputeAttachParentVerticalMoveSpeedToIgnore(desiredAttachParentVelocity);
			attachParentVelocityDelta.z				+= verticalMoveSpeedToIgnore;

			const float gameTimeStep				= fwTimer::GetTimeStep();
			Vector3 attachParentPositionDelta		= attachParentVelocityDelta * gameTimeStep;
			baseFollowPositionForFront				+= attachParentPositionDelta;

			Vector3 orbitFront = m_BasePivotPosition - baseFollowPositionForFront;
			orbitFront.NormalizeSafe();

			if(m_NumUpdatesPerformed > 0)
			{
				Matrix34 previousMatrix;
				camFrame::ComputeWorldMatrixFromFront(m_BaseFront, previousMatrix);
				Quaternion previousOrientation;
				previousOrientation.FromMatrix34(previousMatrix);

				Matrix34 currentMatrix;
				camFrame::ComputeWorldMatrixFromFront(orbitFront, currentMatrix);
				Quaternion currentOrientation;
				currentOrientation.FromMatrix34(currentMatrix);

				float followBlendLevel = 1.0f;

			#if KEYBOARD_MOUSE_SUPPORT
				if (!m_Metadata.m_ShouldPullAroundWhenUsingMouse && m_ControlHelper->WasUsingKeyboardMouseLastInput())
				{
					lookAroundInputEnvelopeLevel = 1.0f;
				}
			#endif // KEYBOARD_MOUSE_SUPPORT

				if(camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this))
				{
					lookAroundInputEnvelopeLevel = 0.0f;
				}

				//Blend in a hard limit on the follow front's orientation delta when there is look-around input. This reduces the extent to which
				//movement of the attach parent affects the follow orientation, allowing greater freedom for looking around.
				if(lookAroundInputEnvelopeLevel >= SMALL_FLOAT)
				{
					const float timeStep				= fwTimer::GetCamTimeStep();
					float maxOrientationDelta			= m_Metadata.m_MaxMoveOrientationSpeedDuringLookAround * DtoR * timeStep;
					float maxLookAroundOrientationSpeed	= m_ControlHelper->ComputeMaxLookAroundOrientationSpeed();
					float maxLookAroundOrientationDelta	= maxLookAroundOrientationSpeed * timeStep;

					float orientationDeltaRatio;
					if(maxOrientationDelta >= SMALL_FLOAT)
					{
						float desiredOrientationDelta	= previousOrientation.RelAngle(currentOrientation);

						float orientationDeltaToApply	= RampValueSafe(desiredOrientationDelta, 0.0f, maxOrientationDelta, 0.0f, maxLookAroundOrientationDelta);
						orientationDeltaRatio			= (desiredOrientationDelta > 0.0f) ? (orientationDeltaToApply / desiredOrientationDelta) : 0.0f;
						orientationDeltaRatio			= Clamp(orientationDeltaRatio, 0.0f, 1.0f);
					}
					else
					{
						orientationDeltaRatio			= 0.0f;
					}

#if KEYBOARD_MOUSE_SUPPORT
					if (m_ControlHelper->WasUsingKeyboardMouseLastInput())
					{
						followBlendLevel = 1.0f - lookAroundInputEnvelopeLevel;
					}
					else
#endif // KEYBOARD_MOUSE_SUPPORT
					{
						followBlendLevel = RampValue(lookAroundInputEnvelopeLevel, 0.0f, 1.0f, 1.0f, orientationDeltaRatio);
					}
				}

				//NOTE: The follow behaviour is blended out when a hint is active, otherwise it can interfere with blend in and out of the hint behaviour.
				if(m_HintHelper)
				{
					const float hintBlendLevel	= camInterface::GetGameplayDirector().GetHintBlend();
					followBlendLevel			*= (1.0f - hintBlendLevel);
				}

				const float customFollowBlendLevel	= ComputeCustomFollowBlendLevel();
				followBlendLevel					*= customFollowBlendLevel;

				Quaternion blendedOrientation;
				blendedOrientation.SlerpNear(followBlendLevel, previousOrientation, currentOrientation);

				Matrix34 blendedMatrix;
				blendedMatrix.FromQuaternion(blendedOrientation);
				orbitFront = blendedMatrix.b;
			}

			ApplyConeLimitToOrbitOrientation(orbitFront);

			camFrame::ComputeHeadingAndPitchFromFront(orbitFront, orbitHeading, orbitPitch);

			//Lock the follow pitch if the attach parent is approaching the camera (at a reasonable speed) in order to avoid 'pushing' the
			//camera upwards or downwards.
			//NOTE: We only consider whether the attach parent is approaching in the XY plane here, as we don't want to lock the pitch if the
			//parent is falling w.r.t. the camera.
			Vector3 attachParentMoveSpeedXY(m_BaseAttachVelocityToConsider);
			attachParentMoveSpeedXY.z = 0.0f;
			Vector3 attachParentMoveDirectionXY(attachParentMoveSpeedXY);
			attachParentMoveDirectionXY.Normalize();
			float attachParentMoveFollowFrontDot	= attachParentMoveDirectionXY.Dot(orbitFront);
			float moveSpeedApproachingCamera		= -attachParentMoveFollowFrontDot * attachParentMoveSpeedXY.Mag2();

			const float minAttachParentApproachSpeedForPitchLock = ComputeMinAttachParentApproachSpeedForPitchLock();
			const bool shouldLockPitch				= (moveSpeedApproachingCamera >= minAttachParentApproachSpeedForPitchLock);
			if(shouldLockPitch)
			{
				//Use the previous frame's follow pitch as a starting point, rather than the movement-based value.
				orbitPitch = initialOrbitPitch;
			}

			if(m_Metadata.m_ShouldLockHeading)
			{
				orbitHeading = initialOrbitHeading; 
			}
		}

		float blendLevelForTurretEntry;
		if(ComputeTurretEntryOrientationBlendLevel(blendLevelForTurretEntry))
		{
			// We disable control input until whilst the turret orientation blend is active
			GetControlHelper()->IgnoreLookAroundInputThisUpdate();

			if(!m_ShouldCacheOrientationForTurretOnPreviousUpdate)
			{
				m_CachedHeadingForTurret	= orbitHeading;
				m_CachedPitchForTurret		= orbitPitch;

				m_ShouldCacheOrientationForTurretOnPreviousUpdate = true;

				cameraDebugf3("Caching orientation for turret");
			}

			float desiredHeading;
			float desiredPitch;
			ComputeTurretEntryOrientation(desiredHeading, desiredPitch);

			//Ensure that we blend to the target orientation over the shortest angle.
			const float desiredHeadingDelta = desiredHeading - orbitHeading;
			if(desiredHeadingDelta > PI)
			{
				desiredHeading -= TWO_PI;
			}
			else if(desiredHeadingDelta < -PI)
			{
				desiredHeading += TWO_PI;
			}			

			const float slowInOutBlendLevel = SlowInOut(blendLevelForTurretEntry);

			orbitHeading = fwAngle::LerpTowards(m_CachedHeadingForTurret, desiredHeading, slowInOutBlendLevel);
			orbitPitch = fwAngle::LerpTowards(m_CachedPitchForTurret, desiredPitch, slowInOutBlendLevel);			
		}
		else
		{
			m_ShouldCacheOrientationForTurretOnPreviousUpdate = false;

#if FPS_MODE_SUPPORTED		
			if (!(camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch() && camInterface::GetCutsceneDirector().IsPerformingFirstPersonBlendOut()))
			{
				ApplyPullAroundOrientationOffsets(orbitHeading, orbitPitch);
			}
#else
			ApplyPullAroundOrientationOffsets(orbitHeading, orbitPitch);
#endif
		}

		//Blend towards persisting the follow orientation from the previous update based upon the aim behaviour envelope.
		if(m_AimBehaviourBlendLevel >= SMALL_FLOAT)
		{
			orbitHeading	= fwAngle::LerpTowards(orbitHeading, initialOrbitHeading, m_AimBehaviourBlendLevel);
			orbitPitch		= fwAngle::LerpTowards(orbitPitch, initialOrbitPitch, m_AimBehaviourBlendLevel);
		}
	}
}

float camFollowCamera::ComputeMinAttachParentApproachSpeedForPitchLock() const
{
	return m_Metadata.m_MinAttachParentApproachSpeedForPitchLock;
}

void camFollowCamera::UpdateAttachParentUpwardSpeedScalingOnGroundBlendLevel()
{
	const bool isAttachParentMovingUpwards			= (m_BaseAttachVelocityToConsider.z >= SMALL_FLOAT);
	const bool isAttachParentInAir					= ComputeIsAttachParentInAir();
	const bool isAttachParentMovingUpwardsOnGround	= isAttachParentMovingUpwards && !isAttachParentInAir;

	if(m_AttachParentUpwardSpeedScalingOnGroundEnvelope)
	{
		if(m_NumUpdatesPerformed == 0)
		{
			if(isAttachParentMovingUpwardsOnGround)
			{
				m_AttachParentUpwardSpeedScalingOnGroundEnvelope->Start(1.0f);
			}
			else
			{
				m_AttachParentUpwardSpeedScalingOnGroundEnvelope->Stop();
			}
		}
		else
		{
			m_AttachParentUpwardSpeedScalingOnGroundEnvelope->AutoStartStop(isAttachParentMovingUpwardsOnGround);
		}

		m_AttachParentUpwardSpeedScalingOnGroundBlendLevel = m_AttachParentUpwardSpeedScalingOnGroundEnvelope->Update();
		m_AttachParentUpwardSpeedScalingOnGroundBlendLevel = Clamp(m_AttachParentUpwardSpeedScalingOnGroundBlendLevel, 0.0f, 1.0f);
		m_AttachParentUpwardSpeedScalingOnGroundBlendLevel = SlowInOut(m_AttachParentUpwardSpeedScalingOnGroundBlendLevel);
	}
	else
	{
		//We don't have an envelope, so just use the input state directly.
		m_AttachParentUpwardSpeedScalingOnGroundBlendLevel = isAttachParentMovingUpwardsOnGround ? 1.0f : 0.0f;
	}

	PF_SET(upwardSpeedScalingOnGroundBlendLevel, m_AttachParentUpwardSpeedScalingOnGroundBlendLevel);
}

void camFollowCamera::UpdateAttachParentUpwardSpeedScalingInAirBlendLevel()
{
	const bool isAttachParentMovingUpwards		= (m_BaseAttachVelocityToConsider.z >= SMALL_FLOAT);
	const bool isAttachParentInAir				= ComputeIsAttachParentInAir();
	const bool isAttachParentMovingUpwardsInAir	= isAttachParentMovingUpwards && isAttachParentInAir;

	if(m_AttachParentUpwardSpeedScalingInAirEnvelope)
	{
		if(m_NumUpdatesPerformed == 0)
		{
			if(isAttachParentMovingUpwardsInAir)
			{
				m_AttachParentUpwardSpeedScalingInAirEnvelope->Start(1.0f);
			}
			else
			{
				m_AttachParentUpwardSpeedScalingInAirEnvelope->Stop();
			}
		}
		else
		{
			m_AttachParentUpwardSpeedScalingInAirEnvelope->AutoStartStop(isAttachParentMovingUpwardsInAir);
		}

		m_AttachParentUpwardSpeedScalingInAirBlendLevel = m_AttachParentUpwardSpeedScalingInAirEnvelope->Update();
		m_AttachParentUpwardSpeedScalingInAirBlendLevel = Clamp(m_AttachParentUpwardSpeedScalingInAirBlendLevel, 0.0f, 1.0f);
		m_AttachParentUpwardSpeedScalingInAirBlendLevel = SlowInOut(m_AttachParentUpwardSpeedScalingInAirBlendLevel);
	}
	else
	{
		//We don't have an envelope, so just use the input state directly.
		m_AttachParentUpwardSpeedScalingInAirBlendLevel = isAttachParentMovingUpwardsInAir ? 1.0f : 0.0f;
	}

	PF_SET(upwardSpeedScalingInAirBlendLevel, m_AttachParentUpwardSpeedScalingInAirBlendLevel);
}

float camFollowCamera::ComputeAttachParentVerticalMoveSpeedToIgnore(const Vector3& attachParentVelocity) const
{
	const float attachParentVerticalMoveSpeed = attachParentVelocity.z;

	const float verticalMoveSpeedScaling	= ComputeVerticalMoveSpeedScaling(attachParentVelocity);
	float verticalMoveSpeedToIgnore			= attachParentVerticalMoveSpeed * (1.0f - verticalMoveSpeedScaling);

	//Apply special scaling for upward movement when the attach parent is on the ground (with appropriate blending of this effect.)
	verticalMoveSpeedToIgnore				+= m_AttachParentUpwardSpeedScalingOnGroundBlendLevel * attachParentVerticalMoveSpeed *
												(1.0f - m_Metadata.m_UpwardMoveSpeedScalingOnGround);

	//Apply special scaling for upward movement when the attach parent is in the air (with appropriate blending of this effect.)
	verticalMoveSpeedToIgnore				+= m_AttachParentUpwardSpeedScalingInAirBlendLevel * attachParentVerticalMoveSpeed *
												(1.0f - m_Metadata.m_UpwardMoveSpeedScalingInAir);

	return verticalMoveSpeedToIgnore;
}

float camFollowCamera::ComputeVerticalMoveSpeedScaling(const Vector3& attachParentVelocity) const
{
	const float attachParentSpeed			= attachParentVelocity.Mag();
	const float verticalMoveSpeedScaling	= RampValueSafe(attachParentSpeed, m_Metadata.m_SpeedLimitsForVerticalMoveSpeedScaling.x,
												m_Metadata.m_SpeedLimitsForVerticalMoveSpeedScaling.y, m_Metadata.m_VerticalMoveSpeedScaling,
												m_Metadata.m_VerticalMoveSpeedScalingAtMaxSpeed);

	return verticalMoveSpeedScaling;
}

void camFollowCamera::ApplyConeLimitToOrbitOrientation(Vector3& orbitFront)
{
	const camFollowCameraMetadataFollowOrientationConing& coneSettings = m_Metadata.m_FollowOrientationConing;

	//NOTE: A negative max cone angle indicates that no limiting is required.
	if(coneSettings.m_MaxAngle >= 0.0f)
	{
		//Make the follow front tend towards being within a predefined elliptical cone.

		Matrix34 coneMatrix(m_AttachParentMatrix);
		coneMatrix.RotateZ(coneSettings.m_HeadingOffset * DtoR);
		coneMatrix.RotateLocalX(coneSettings.m_PitchOffset * DtoR);

		Quaternion coneOrientation;
		coneOrientation.FromMatrix34(coneMatrix);

		Matrix34 followWorldMatrix;
		camFrame::ComputeWorldMatrixFromFront(orbitFront, followWorldMatrix);
		Quaternion followOrientation;
		followOrientation.FromMatrix34(followWorldMatrix);

		float orientationDelta = coneOrientation.RelAngle(followOrientation);

		//Calculate the max orientation delta for the elliptical cone.

		//Calculate the angle of the (cone-)relative follow front about the local Y-axis.
		Vector3 relativeFollowFrontXZ;
		coneMatrix.UnTransform3x3(orbitFront, relativeFollowFrontXZ);
		float theta			= rage::Atan2f(-relativeFollowFrontXZ.x, relativeFollowFrontXZ.z);

		float b				= coneSettings.m_AspectRatio;
		float bSq			= b * b;
		float tanTheta		= rage::Tanf(theta);
		float tanSqTheta	= tanTheta * tanTheta;
		float xSq			= bSq / (bSq + tanSqTheta);
		float ySq			= 1.0f / ((1.0f / bSq) + (1.0f / tanSqTheta));
		float coneScaling	= rage::Sqrtf(xSq + ySq);

		if(coneSettings.m_AspectRatio > 1.0f)
		{
			//Ensure the major axis of the ellipse is of unit length.
			coneScaling		/= coneSettings.m_AspectRatio;
		}

		float coneAngle		= coneScaling * coneSettings.m_MaxAngle * DtoR;
		if(orientationDelta > coneAngle)
		{
			//Rotate towards the edge of the cone.
			Quaternion desiredOrientation;
			float interpolationFactor = coneAngle / orientationDelta;
			desiredOrientation.SlerpNear(interpolationFactor, coneOrientation, followOrientation);

			Quaternion orientationToApply;
			//Cheaply make the smoothing frame-rate independent.
			float smoothRate	= coneSettings.m_SmoothRate * 30.0f * fwTimer::GetCamTimeStep();
			smoothRate			= Min(smoothRate, 1.0f);
			orientationToApply.SlerpNear(smoothRate, followOrientation, desiredOrientation);

			Matrix34 worldMatrixToApply;
			worldMatrixToApply.FromQuaternion(orientationToApply);

			orbitFront = worldMatrixToApply.b;
		}
	}
}

//TODO: Tidy this monster.
void camFollowCamera::ApplyPullAroundOrientationOffsets(float& orbitHeading, float& orbitPitch)
{
	camFollowCameraMetadataPullAroundSettings pullAroundSettings;
	ComputePullAroundSettings(pullAroundSettings);

#if KEYBOARD_MOUSE_SUPPORT
	if (!m_Metadata.m_ShouldPullAroundWhenUsingMouse && m_ControlHelper->WasUsingKeyboardMouseLastInput())
	{
		return;
	}
#endif

	if(camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this))
	{
		return;
	}

	//NOTE: The pull-around effect is blended out when there is look-around input, otherwise it would interfere with the look-around.
	//NOTE: We also blend-out this effect when the attach parent is in the air, as free rotation can result in unnatural pull-around behaviour.
	//NOTE: m_ControlHelper has already been validated.
	const float lookAroundEnvelopeLevel	= m_ControlHelper->GetLookAroundInputEnvelopeLevel();
	float pullAroundBlendLevel			= (1.0f - lookAroundEnvelopeLevel);
	if(pullAroundSettings.m_ShouldBlendOutWhenAttachParentIsInAir)
	{
		pullAroundBlendLevel *= (1.0f - m_AttachParentInAirBlendLevel);
	}
	else if(pullAroundSettings.m_ShouldBlendOutWhenAttachParentIsOnGround)
	{
		pullAroundBlendLevel *= m_AttachParentInAirBlendLevel;
	}

	//NOTE: The pull-around effect can optionally be blended out when we blend out the effect of the attach parent matrix on the camera orbit orientation,
	//as this suggests we believe the attach parent matrix may be unstable.
	if(pullAroundSettings.m_ShouldBlendWithAttachParentMatrixForRelativeOrbitBlend)
	{
		const float attachParentMatrixForRelativeOrbitBlendLevel	= m_AttachParentMatrixForRelativeOrbitSpring.GetResult();
		pullAroundBlendLevel										*= attachParentMatrixForRelativeOrbitBlendLevel;
	}

	//NOTE: The pull-around effect is blended out when a hint is active, otherwise it can interfere with blend in and out of the hint behaviour.
	if(m_HintHelper)
	{
		const float hintBlendLevel	= camInterface::GetGameplayDirector().GetHintBlend();
		pullAroundBlendLevel		*= (1.0f - hintBlendLevel);
	}

	Vector3 attachParentVelocityToConsider;
	ComputeAttachParentVelocityToConsiderForPullAround(attachParentVelocityToConsider);

	float desiredHeading;
	float desiredPitch;
	ComputeDesiredPullAroundOrientation(attachParentVelocityToConsider, orbitHeading, orbitPitch, desiredHeading, desiredPitch);

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		//Convert the orbit orientation and desired orientation to be parent-relative.

		Vector3 orbitFront;
		camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, orbitFront);

		m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(orbitFront);

		camFrame::ComputeHeadingAndPitchFromFront(orbitFront, orbitHeading, orbitPitch);

		Vector3 desiredFront;
		camFrame::ComputeFrontFromHeadingAndPitch(desiredHeading, desiredPitch, desiredFront);

		m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(desiredFront);

		camFrame::ComputeHeadingAndPitchFromFront(desiredFront, desiredHeading, desiredPitch);
	}

	const float attachParentSpeed	= attachParentVelocityToConsider.Mag();

	float moveSpeedRatioForHeading	= RampValueSafe(attachParentSpeed, pullAroundSettings.m_HeadingPullAroundMinMoveSpeed,
										pullAroundSettings.m_HeadingPullAroundMaxMoveSpeed, 0.0f, 1.0f);
	moveSpeedRatioForHeading		= SlowInOut(moveSpeedRatioForHeading);

	float moveSpeedRatioForPitch	= RampValueSafe(attachParentSpeed, pullAroundSettings.m_PitchPullAroundMinMoveSpeed,
										pullAroundSettings.m_PitchPullAroundMaxMoveSpeed, 0.0f, 1.0f);
	moveSpeedRatioForPitch			= SlowInOut(moveSpeedRatioForPitch);

	if(m_Metadata.m_ShouldPullAroundUsingSimpleSpringDamping)
	{
		const float blendLevelForHeading	= pullAroundBlendLevel * moveSpeedRatioForHeading;
		desiredHeading						= fwAngle::LerpTowards(orbitHeading, desiredHeading, blendLevelForHeading);

		const float blendLevelForPitch		= pullAroundBlendLevel * moveSpeedRatioForPitch;
		desiredPitch						= fwAngle::LerpTowards(orbitPitch, desiredPitch, blendLevelForPitch);

		//Ensure that we blend to the target orientation over the shortest angle.
		const float desiredHeadingDelta = desiredHeading - orbitHeading;
		if(desiredHeadingDelta > PI)
		{
			desiredHeading -= TWO_PI;
		}
		else if(desiredHeadingDelta < -PI)
		{
			desiredHeading += TWO_PI;
		}

		m_HeadingPullAroundSpring.OverrideResult(orbitHeading);
		orbitHeading	= m_HeadingPullAroundSpring.Update(desiredHeading, pullAroundSettings.m_HeadingPullAroundSpringConstant,
							pullAroundSettings.m_HeadingPullAroundSpringDampingRatio);

		PF_SET(desiredPullAroundHeading, desiredHeading);
		PF_SET(dampedPullAroundHeading, orbitHeading);

		m_PitchPullAroundSpring.OverrideResult(orbitPitch);
		orbitPitch		= m_PitchPullAroundSpring.Update(desiredPitch, pullAroundSettings.m_PitchPullAroundSpringConstant,
							pullAroundSettings.m_PitchPullAroundSpringDampingRatio);

		PF_SET(desiredPullAroundPitch, desiredPitch);
		PF_SET(dampedPullAroundPitch, orbitPitch);
	}
	else
	{
		float desiredHeadingOffset	= 0.0f;
		float desiredPitchOffset	= 0.0f;

		if(pullAroundBlendLevel >= SMALL_FLOAT)
		{
			float gameTimeStep				= fwTimer::GetTimeStep();
			desiredHeadingOffset			= pullAroundSettings.m_HeadingPullAroundSpeedAtMaxMoveSpeed * DtoR * gameTimeStep *
												moveSpeedRatioForHeading;
			float relativeHeading			= fwAngle::LimitRadianAngle(orbitHeading - desiredHeading);
			float absRelativeHeading		= Abs(relativeHeading);
			//NOTE: The heading pull-around effect increases as the absolute error approaches 90deg and remains at the maximum level beyond 90deg.
			float maxHeadingErrorScaling	= (absRelativeHeading > HALF_PI) ? 1.0f : (1.0f - rage::Cosf(2.0f * relativeHeading)) / 2.0f;
			//Blend the amount of error scaling we apply, as some cameras may wish for a more uniform response.
			float headingErrorScaling		= Lerp(pullAroundSettings.m_HeadingPullAroundErrorScalingBlendLevel, 1.0f, maxHeadingErrorScaling);

			desiredHeadingOffset			*= headingErrorScaling * -Sign(relativeHeading);
			desiredHeadingOffset			*= pullAroundBlendLevel;
			desiredHeadingOffset			= Clamp(desiredHeadingOffset, -absRelativeHeading, absRelativeHeading); //Don't overshoot the desired heading.

			desiredPitchOffset				= pullAroundSettings.m_PitchPullAroundSpeedAtMaxMoveSpeed * DtoR * gameTimeStep *
												moveSpeedRatioForPitch;
			float relativePitch				= fwAngle::LimitRadianAngle(orbitPitch - desiredPitch);
			float absRelativePitch			= Abs(relativePitch);
			//NOTE: The pitch pull-around effect increases as the absolute error approaches 90deg.
			float maxPitchErrorScaling		= (1.0f - rage::Cosf(2.0f * relativePitch)) / 2.0f;
			//Blend the amount of error scaling we apply, as some cameras may wish for a more uniform response.
			float pitchErrorScaling			= Lerp(pullAroundSettings.m_PitchPullAroundErrorScalingBlendLevel, 1.0f, maxPitchErrorScaling);

			desiredPitchOffset				*= pitchErrorScaling * -Sign(relativePitch);
			desiredPitchOffset				*= pullAroundBlendLevel;
			desiredPitchOffset				= Clamp(desiredPitchOffset, -absRelativePitch, absRelativePitch); //Don't overshoot the desired pitch.
		}

		const float headingOffset	= m_HeadingPullAroundSpring.Update(desiredHeadingOffset, pullAroundSettings.m_HeadingPullAroundSpringConstant,
										pullAroundSettings.m_HeadingPullAroundSpringDampingRatio);
		orbitHeading				+= headingOffset;

		const float pitchOffset		= m_PitchPullAroundSpring.Update(desiredPitchOffset, pullAroundSettings.m_PitchPullAroundSpringConstant,
										pullAroundSettings.m_PitchPullAroundSpringDampingRatio);
		orbitPitch					+= pitchOffset;
	}

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		//Convert the orbit orientation back into world space.

		Vector3 orbitFront;
		camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, orbitFront);

		m_AttachParentMatrixToConsiderForRelativeOrbit.Transform3x3(orbitFront);

		camFrame::ComputeHeadingAndPitchFromFront(orbitFront, orbitHeading, orbitPitch);
	}
}

void camFollowCamera::ComputePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const
{
	//Apply custom pull-around settings when looking behind.
	//NOTE: m_ControlHelper has already been validated.
	const bool isLookingBehind = m_ControlHelper->IsLookingBehind();
	if(isLookingBehind)
	{
		ComputeLookBehindPullAroundSettings(settings);
	}
	else
	{
		ComputeBasePullAroundSettings(settings);
	}
}

void camFollowCamera::ComputeLookBehindPullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const
{
	settings = m_Metadata.m_PullAroundSettingsForLookBehind;
}

void camFollowCamera::ComputeBasePullAroundSettings(camFollowCameraMetadataPullAroundSettings& settings) const
{
	settings = m_Metadata.m_PullAroundSettings;
}

void camFollowCamera::ComputeAttachParentVelocityToConsiderForPullAround(Vector3& attachParentVelocityToConsider) const
{
	if(m_Metadata.m_ShouldConsiderAttachParentLocalXYVelocityForPullAround)
	{
		ComputeAttachParentLocalXYVelocity(attachParentVelocityToConsider);
	}
	else if(m_Metadata.m_ShouldConsiderAttachParentForwardSpeedForPullAround)
	{
		const float velocityDotFront	= m_BaseAttachVelocityToConsider.Dot(m_AttachParentMatrix.b);
		const float forwardSpeed		= Max(velocityDotFront, 0.0f);
		attachParentVelocityToConsider.SetScaled(m_AttachParentMatrix.b, forwardSpeed);
	}
	else
	{
		attachParentVelocityToConsider = m_BaseAttachVelocityToConsider;
	}
}

void camFollowCamera::ComputeDesiredPullAroundOrientation(const Vector3& attachParentVelocityToConsider, float UNUSED_PARAM(orbitHeading),
	float UNUSED_PARAM(orbitPitch), float& desiredHeading, float& desiredPitch) const
{
	const bool shouldPullAroundToBasicAttachParentMatrix = ComputeShouldPullAroundToBasicAttachParentMatrix();

	//Pull-around to either the target's front or move direction, as specified in metadata.
	if(!shouldPullAroundToBasicAttachParentMatrix)
	{
		Vector3 pullAroundTargetFront;

		//NOTE: We pull-around to the attach parent front when looking behind, in order to tend towards a reverse angle, irrespective of
		//the direction of movement of the parent.
		//NOTE: m_ControlHelper has already been validated.
		if(m_Metadata.m_ShouldPullAroundToAttachParentFront || m_ControlHelper->IsLookingBehind())
		{
			pullAroundTargetFront = m_AttachParentMatrix.b;
		}
		else
		{
			pullAroundTargetFront = attachParentVelocityToConsider;
			pullAroundTargetFront.NormalizeSafe(m_AttachParentMatrix.b);
		}

		camFrame::ComputeHeadingAndPitchFromFront(pullAroundTargetFront, desiredHeading, desiredPitch);
	}
	else
	{
		//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix. This
		//reduces the scope for the parent matrix to be upside-down in world space, as this can flip the decomposed parent heading.
		const Matrix34 attachParentMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
		camFrame::ComputeHeadingAndPitchFromMatrix(attachParentMatrix, desiredHeading, desiredPitch);
	}
	
}

void camFollowCamera::ComputeTurretEntryOrientation(float& desiredHeading, float& desiredPitch) const
{
	bool hasSetDesiredOrientation = false;

	desiredHeading = 0.0f;
	desiredPitch = 0.0f;

	// The following applies to heli turrets only, we set the desired orientation based on the currently active tasks
	CPed* followPed = const_cast<CPed*>(camInterface::GetGameplayDirector().GetFollowPed());
	const CPedIntelligence* pedIntelligence	= followPed ? followPed->GetPedIntelligence() : NULL;
	if(pedIntelligence)
	{							
		s32 seatIndex = -1;
		CVehicle* vehicle = NULL;		

		CTaskEnterVehicle* enterVehicleTask					= static_cast<CTaskEnterVehicle*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_ENTER_VEHICLE));	
		CTaskExitVehicle* exitVehicleTask					= static_cast<CTaskExitVehicle*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_EXIT_VEHICLE));	
		CTaskInVehicleSeatShuffle* inVehicleSeatShuffleTask = static_cast<CTaskInVehicleSeatShuffle*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));			

		if(inVehicleSeatShuffleTask)
		{
			vehicle		= const_cast<CVehicle*>(camInterface::GetGameplayDirector().GetFollowVehicle());
			seatIndex	= inVehicleSeatShuffleTask->GetTargetSeatIndex();				
		}	
		else if(enterVehicleTask)
		{								
			vehicle		= enterVehicleTask->GetVehicle();							
			seatIndex	= vehicle->GetEntryExitPoint(enterVehicleTask->GetTargetEntryPoint())->GetSeat(SA_directAccessSeat);
		}		
		else if(exitVehicleTask)
		{
			vehicle		= exitVehicleTask->GetVehicle();
			seatIndex	= exitVehicleTask->GetTargetSeat();							
		}

		if(seatIndex != -1 && vehicle && vehicle->InheritsFromHeli())
		{								
			const CSeatManager* seatManager = vehicle->GetSeatManager();
			if(seatManager) 
			{							
				const CVehicleWeaponMgr* weaponManager = vehicle->GetVehicleWeaponMgr();
				if(weaponManager)
				{							
					const CTurret* turret = weaponManager->GetFirstTurretForSeat(seatIndex);
					if(turret)
					{																												
						Matrix34 turretMatrixWorld;
						turret->GetTurretMatrixWorld(turretMatrixWorld, vehicle);

						Matrix34 vehicleMatrix	= MAT34V_TO_MATRIX34(vehicle->GetMatrix());
						float vehicleHeading	= camFrame::ComputeHeadingFromMatrix(vehicleMatrix);

						Vector3 turretPosition; 
						vehicleMatrix.UnTransform(turretMatrixWorld.d, turretPosition);

						float sign = Sign(turretPosition.x);						
						if(sign < 0.0f)
						{
							desiredHeading = vehicleHeading - HALF_PI;
						}
						else
						{
							desiredHeading = vehicleHeading + HALF_PI;
						}

						desiredHeading	= fwAngle::LimitRadianAngleSafe(desiredHeading);
						desiredPitch	= 0;

						hasSetDesiredOrientation = true;						
					}
				}
			}
		}	
	}

	// Ensure that the code path above is evaluated. This should never assert if ComputeTurretEntryOrientationBlendLevel has the same task tree as ComputeTurretEntryOrientation.
	// There's an issue in-mission where CLEAR_PED_TASKS_IMMEDIATELY comes from script in a race condition.
	// We guard against this by checking the vehicle is not wrecked. If this assert fires then something has changed in script and CLEAR_PED_TASKS_IMMEDIATELY is happening earlier.
	cameraAssertf(hasSetDesiredOrientation, "ComputeTurretEntryOrientation has not set an orientation"); 
}

bool camFollowCamera::ComputeShouldPullAroundToBasicAttachParentMatrix() const
{
	return m_Metadata.m_ShouldPullAroundToBasicAttachParentMatrix;
}

float camFollowCamera::ComputeDesiredRoll(const Matrix34& orbitMatrix)
{
	float desiredRoll;

	if(m_Metadata.m_RollSettings.m_ShouldApplyRoll)
	{
		//NOTE: This method of computing the appropriate roll to apply given an arbitrary camera orientation relative to the attach parent
		//cannot handle the parent roll (after scaling and limiting) exceeding +/-90 degrees, as one way or another a discontinuity will exist
		//when the roll is applied independently of the rest of the camera update.

		//Rescale the roll using Sine, such that the roll considered will peak at +/-90 degrees and diminish beyond this, which helps to reduce
		//instability when the attach parent barrel rolls.
		const float attachParentRoll			= m_Frame.ComputeRollFromMatrix(m_AttachParentMatrix);
		const float rescaledAttachParentRoll	= HALF_PI * Sinf(attachParentRoll);

		//Scale the roll based upon the forward speed of the attach parent.
		const Vector3 attachParentFront	= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetB());
		const float forwardSpeed		= DotProduct(m_BaseAttachVelocityToConsider, attachParentFront);

		float desiredSpeedFactor;
		const float speedRange = m_Metadata.m_RollSettings.m_MaxForwardSpeed - m_Metadata.m_RollSettings.m_MinForwardSpeed;
		if(speedRange >= SMALL_FLOAT)
		{
			desiredSpeedFactor	= (forwardSpeed - m_Metadata.m_RollSettings.m_MinForwardSpeed) / speedRange;
			desiredSpeedFactor	= Clamp(desiredSpeedFactor, 0.0f, 1.0f);
			desiredSpeedFactor	= SlowInOut(desiredSpeedFactor);
		}
		else
		{
			desiredSpeedFactor	= (forwardSpeed >= m_Metadata.m_RollSettings.m_MaxForwardSpeed) ? 1.0f : 0.0f;
		}

		float rollToConsider	= rescaledAttachParentRoll * desiredSpeedFactor * m_Metadata.m_RollSettings.m_RollAngleScale; 
		rollToConsider			= Clamp(rollToConsider, -m_Metadata.m_RollSettings.m_MaxRoll * DtoR, m_Metadata.m_RollSettings.m_MaxRoll * DtoR); 

		const bool shouldCutToDesiredRoll = ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
		m_RollSpring.Update(rollToConsider, shouldCutToDesiredRoll ? 0.0f : m_Metadata.m_RollSettings.m_RollSpringConstant,
			m_Metadata.m_RollSettings.m_RollSpringDampRatio); 

		const float maxRollToApply			= m_RollSpring.GetResult();
		const Vector3& cameraFront			= m_Frame.GetFront();
		const float cameraDotAttachParent	= cameraFront.Dot(m_AttachParentMatrix.b);
		desiredRoll							= maxRollToApply * cameraDotAttachParent;
		desiredRoll							= fwAngle::LimitRadianAngle(desiredRoll);
	}
	else
	{
		desiredRoll = camThirdPersonCamera::ComputeDesiredRoll(orbitMatrix);
	}

	return desiredRoll;
}

float camFollowCamera::ComputeDesiredFov()
{
	float desiredFov = camThirdPersonCamera::ComputeDesiredFov();

	//Scale the FOV upwards when the attach parent is at a high altitude.

	const camFollowCameraMetadataHighAltitudeZoomSettings& settings = m_Metadata.m_HighAltitudeZoomSettings;

	const bool isHighAltitudeFovScalingEnabled = camInterface::GetGameplayDirector().IsHighAltitudeFovScalingEnabled();

	//NOTE: High altitude FOV scaling is inhibited within interiors.
	float desiredAltitudeFactor = 0.0f;
	if(isHighAltitudeFovScalingEnabled && !m_AttachParent->m_nFlags.bInMloRoom)
	{
		//Use the local minimum height of the world height map as the nominal ground height.
		const float minWorldAltitude	= CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(m_BaseAttachPosition.x, m_BaseAttachPosition.y);
		const float altitudeDelta		= m_BaseAttachPosition.z - minWorldAltitude;
		desiredAltitudeFactor			= SlowInOut(RampValueSafe(altitudeDelta, settings.m_MinAltitudeDelta, settings.m_MaxAltitudeDelta, 0.0f, 1.0f));
	}

	const bool wasRenderingThisCameraOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldCut								= (!wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	m_HighAltitudeZoomSpring.Update(desiredAltitudeFactor, shouldCut ? 0.0f : settings.m_SpringConstant, settings.m_SpringDampingRatio);

	const float maxFovToApply = m_Metadata.m_BaseFov * settings.m_MaxBaseFovScaling;
	if(maxFovToApply >= (desiredFov + SMALL_FLOAT))
	{
		const float altitudeFactorToApply	= Clamp(m_HighAltitudeZoomSpring.GetResult(), 0.0f, 1.0f); //For safety.
		desiredFov							= Lerp(altitudeFactorToApply, desiredFov, maxFovToApply);
	}

	return desiredFov;
}

const CEntity* camFollowCamera::ComputeDofOverriddenFocusEntity() const
{
	const CEntity* focusEntity = camThirdPersonCamera::ComputeDofOverriddenFocusEntity();

	//Focus on the attach parent whenever the death/fail effect is active.
	if(m_AttachParent)
	{
		const bool isDeathFailEffectActive = camInterface::IsDeathFailEffectActive();
		if(isDeathFailEffectActive)
		{
			focusEntity = m_AttachParent;
		}
	}

	return focusEntity;
}

void camFollowCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camThirdPersonCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	//Modify the DOF settings whenever the death/fail effect is active.
	const bool isDeathFailEffectActive = camInterface::IsDeathFailEffectActive();
	if(isDeathFailEffectActive)
	{
		settings.m_FNumberOfLens = g_DofFNumberOfLensForDeathFailEffect;
		settings.m_SubjectMagnificationPowerFactorNearFar.Set(g_DofSubjectMagPowerForDeathFailEffect, g_DofSubjectMagPowerForDeathFailEffect);
	}
}

void camFollowCamera::BlendPullAroundSettings(float blendLevel, const camFollowCameraMetadataPullAroundSettings& a,
	const camFollowCameraMetadataPullAroundSettings& b, camFollowCameraMetadataPullAroundSettings& result)
{
	result.m_ShouldBlendOutWhenAttachParentIsInAir		= a.m_ShouldBlendOutWhenAttachParentIsInAir || b.m_ShouldBlendOutWhenAttachParentIsInAir;
	result.m_ShouldBlendOutWhenAttachParentIsOnGround	= a.m_ShouldBlendOutWhenAttachParentIsOnGround || b.m_ShouldBlendOutWhenAttachParentIsOnGround;
	result.m_HeadingPullAroundMinMoveSpeed				= Lerp(blendLevel, a.m_HeadingPullAroundMinMoveSpeed, b.m_HeadingPullAroundMinMoveSpeed);
	result.m_HeadingPullAroundMaxMoveSpeed				= Lerp(blendLevel, a.m_HeadingPullAroundMaxMoveSpeed, b.m_HeadingPullAroundMaxMoveSpeed);
	result.m_HeadingPullAroundSpeedAtMaxMoveSpeed		= Lerp(blendLevel, a.m_HeadingPullAroundSpeedAtMaxMoveSpeed, b.m_HeadingPullAroundSpeedAtMaxMoveSpeed);
	result.m_HeadingPullAroundErrorScalingBlendLevel	= Lerp(blendLevel, a.m_HeadingPullAroundErrorScalingBlendLevel, b.m_HeadingPullAroundErrorScalingBlendLevel);
	result.m_HeadingPullAroundSpringConstant			= Lerp(blendLevel, a.m_HeadingPullAroundSpringConstant, b.m_HeadingPullAroundSpringConstant);
	result.m_HeadingPullAroundSpringDampingRatio		= Lerp(blendLevel, a.m_HeadingPullAroundSpringDampingRatio, b.m_HeadingPullAroundSpringDampingRatio);
	result.m_PitchPullAroundMinMoveSpeed				= Lerp(blendLevel, a.m_PitchPullAroundMinMoveSpeed, b.m_PitchPullAroundMinMoveSpeed);
	result.m_PitchPullAroundMaxMoveSpeed				= Lerp(blendLevel, a.m_PitchPullAroundMaxMoveSpeed, b.m_PitchPullAroundMaxMoveSpeed);
	result.m_PitchPullAroundSpeedAtMaxMoveSpeed			= Lerp(blendLevel, a.m_PitchPullAroundSpeedAtMaxMoveSpeed, b.m_PitchPullAroundSpeedAtMaxMoveSpeed);
	result.m_PitchPullAroundErrorScalingBlendLevel		= Lerp(blendLevel, a.m_PitchPullAroundErrorScalingBlendLevel, b.m_PitchPullAroundErrorScalingBlendLevel);
	result.m_PitchPullAroundSpringConstant				= Lerp(blendLevel, a.m_PitchPullAroundSpringConstant, b.m_PitchPullAroundSpringConstant);
	result.m_PitchPullAroundSpringDampingRatio			= Lerp(blendLevel, a.m_PitchPullAroundSpringDampingRatio, b.m_PitchPullAroundSpringDampingRatio);
}

bool camFollowCamera::ComputeTurretEntryOrientationBlendLevel(float& blendLevelForTurretEntry) const
{	
	const CPed* attachPed = camInterface::GetGameplayDirector().GetFollowPed();	
	const CPedIntelligence* pedIntelligence	= attachPed ? attachPed->GetPedIntelligence() : NULL;
	if(pedIntelligence)
	{							
		CTaskEnterVehicle* enterVehicleTask					= static_cast<CTaskEnterVehicle*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_ENTER_VEHICLE));	
		CTaskExitVehicle* exitVehicleTask					= static_cast<CTaskExitVehicle*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_EXIT_VEHICLE));	
		CTaskInVehicleSeatShuffle* inVehicleSeatShuffleTask = static_cast<CTaskInVehicleSeatShuffle*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));			

		if(inVehicleSeatShuffleTask &&
			camInterface::GetGameplayDirector().GetFollowVehicle() &&
			camInterface::GetGameplayDirector().GetFollowVehicle()->InheritsFromHeli() &&
			camGameplayDirector::IsTurretSeat(camInterface::GetGameplayDirector().GetFollowVehicle(), inVehicleSeatShuffleTask->GetTargetSeatIndex()))
		{
			float phase = Clamp(inVehicleSeatShuffleTask->GetTimeRunning(), 0.0f, 1.0f);			
			blendLevelForTurretEntry = Clamp(phase, 0.0f, 1.0f);
			return true;
		}
		else if(enterVehicleTask &&
			enterVehicleTask->GetVehicle() &&
			enterVehicleTask->GetVehicle()->InheritsFromHeli() &&
			enterVehicleTask->GetSubTask() && enterVehicleTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE_SEAT &&
			camGameplayDirector::IsTurretSeat(enterVehicleTask->GetVehicle(), enterVehicleTask->GetTargetSeat()))
		{			
			float phase = Clamp(enterVehicleTask->GetSubTask()->GetTimeRunning(), 0.0f, 1.0f);									
			blendLevelForTurretEntry = Clamp(phase, 0.0f, 1.0f);			
			return true;
		}
		else if(exitVehicleTask &&
			exitVehicleTask->GetVehicle() &&
			exitVehicleTask->GetVehicle()->InheritsFromHeli() &&			
			exitVehicleTask->GetVehicle()->GetStatus() != STATUS_WRECKED &&			
			camGameplayDirector::IsTurretSeat(exitVehicleTask->GetVehicle(), exitVehicleTask->GetTargetSeat()))				
		{						
			float phase = Clamp(exitVehicleTask->GetTimeRunning(), 0.0f, 1.0f);
			blendLevelForTurretEntry = Clamp(phase, 0.0f, 1.0f);			
			return true;
		}		
	}	

	return false;
}
