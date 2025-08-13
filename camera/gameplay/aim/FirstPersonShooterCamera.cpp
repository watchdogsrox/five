//
// camera/gameplay/aim/FirstPersonShooterCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/FirstPersonShooterCamera.h"

#if FPS_MODE_SUPPORTED
#include "fwanimation/AnimDirector.h"

#include "Animation/EventTags.h"
#include "camera/CamInterface.h"
#include "camera/camera_channel.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowPedCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/HintHelper.h"
#include "camera/helpers/StickyAimHelper.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "game/ModelIndices.h"
#include "Ik/solvers/RootSlopeFixupSolver.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "system/ControlMgr.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "task/Default/TaskPlayer.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "task/Motion/Locomotion/TaskInWater.h"
#include "Task/Movement/Climbing/TaskDropDown.h"
#include "task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "task/Movement/TaskFall.h"
#include "Task/Movement/TaskParachute.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/TaskHelpers.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Weapons/TaskBomb.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/planes.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PedWeapons/PlayerPedTargeting.h"

#include "profile/group.h"
#include "profile/page.h"

#if __BANK
#include "input/mapper.h"
#endif // __BANK

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFirstPersonShooterCamera,0x5D1804BC)

PF_PAGE(camFpsCameraHeadingPage, "Camera: FPS Camera Heading");
PF_PAGE(camFpsCameraPitchPage, "Camera: FPS Camera Pitch");
PF_PAGE(camFpsCameraAnimPage, "Camera: FPS Camera Anim");

PF_GROUP(camFpsCameraHeadingMetrics);
PF_LINK(camFpsCameraHeadingPage, camFpsCameraHeadingMetrics);
PF_GROUP(camFpsCameraPitchMetrics);
PF_LINK(camFpsCameraPitchPage, camFpsCameraPitchMetrics);

PF_VALUE_FLOAT(ControlHelper_heading, camFpsCameraHeadingMetrics);
PF_VALUE_FLOAT(Base_Heading_Input, camFpsCameraHeadingMetrics);
PF_VALUE_FLOAT(Raw_Mouse_X_Input, camFpsCameraHeadingMetrics);
PF_VALUE_FLOAT(ControlHelper_pitch, camFpsCameraPitchMetrics);
PF_VALUE_FLOAT(Base_Pitch_Input, camFpsCameraPitchMetrics);
PF_VALUE_FLOAT(Raw_Mouse_Y_Input, camFpsCameraPitchMetrics);

PF_GROUP(camFpsCameraAnimMetrics);
PF_GROUP(camFpsCameraAnimBlendMetrics);
PF_LINK(camFpsCameraAnimPage, camFpsCameraAnimMetrics);
PF_LINK(camFpsCameraAnimPage, camFpsCameraAnimBlendMetrics);

PF_VALUE_FLOAT(AnimPosX, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimPosY, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimPosZ, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimYaw, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimPitch, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimRoll, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimFov, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimLimitXMin, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimLimitXMax, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimLimitYMin, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimLimitYMax, camFpsCameraAnimMetrics);
PF_VALUE_FLOAT(AnimWeight, camFpsCameraAnimMetrics);

PF_VALUE_FLOAT(AnimInterp, camFpsCameraAnimBlendMetrics);
PF_VALUE_INT(AnimEnabled, camFpsCameraAnimBlendMetrics);
PF_VALUE_INT(AnimWasEnabled, camFpsCameraAnimBlendMetrics);

// Disable modifying the near clip value as near clip scanner has been disabled for next gen.
#define CAM_MODIFY_NEAR_CLIP_WHEN_LOOKING_DOWN									0

BankUInt32 c_ExtraVehicleEntryDelay = 400;
BankUInt32 c_HotwireBlendDelay = 1500;
BankUInt32 c_TurretEntryDelay = 1500;

REGISTER_TUNE_GROUP_FLOAT(fGroundAvoidBlendOutAngle, 0.5f);
REGISTER_TUNE_GROUP_FLOAT(fGroundAvoidMaxAngle, HALF_PI);
REGISTER_TUNE_GROUP_FLOAT(fGroundAvoidClipMaxAngle, QUARTER_PI);

REGISTER_TUNE_GROUP_FLOAT(fVehicleSeatBlendDistance, 1.00f);
REGISTER_TUNE_GROUP_FLOAT(fMinPitchForHotwiring, -28.0f);

REGISTER_TUNE_GROUP_FLOAT(INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, -0.3f);
REGISTER_TUNE_GROUP_FLOAT(CAMERA_PROBE_LENGTH, 1.0f);
REGISTER_TUNE_GROUP_FLOAT(FINAL_Z_OFFSET_FOR_CAMERA_PROBE, 0.15f);
REGISTER_TUNE_GROUP_FLOAT(Z_INCREMENT_BETWEEN_CAMERA_PROBES, 0.1f);
REGISTER_TUNE_GROUP_INT( c_CarJackBlendOutLimitDuration, 750);
REGISTER_TUNE_GROUP_FLOAT(MAX_PEEK_ROLL, 5.0f);


camFirstPersonShooterCamera::camFirstPersonShooterCamera(const camBaseObjectMetadata& metadata)
: camFirstPersonAimCamera(metadata)
, m_Metadata(static_cast<const camFirstPersonShooterCameraMetadata&>(metadata))
, m_PreviousRelativeCameraPosition( rage::V_ZERO )
, m_PreviousCameraPosition(Vector3::ZeroType)
, m_PreviousRelativeAttachPosition(g_InvalidRelativePosition)
, m_PovCameraPosition(Vector3::ZeroType)
, m_RelativeHeadOffset(Vector3::ZeroType)
, m_RelativeCoverOffset(Vector3::ZeroType)
, m_CoverPeekOffset(Vector3::ZeroType)
, m_ObjectSpacePosition(Vector3::ZeroType)
, m_RelativeFallbackPosition(Vector3::ZeroType)
, m_GroundNormal(g_UnitUp)
, m_CurrentScopeOffset(Vector3::ZeroType)
, m_CurrentScopeRotationOffset(Vector3::ZeroType)
, m_CurrentNonScopeOffset(Vector3::ZeroType)
, m_CurrentNonScopeRotationOffset(Vector3::ZeroType)
, m_CurrentThirdPersonOffset(Vector3::ZeroType)
, m_MeleeTargetBoneRelativeOffset(g_InvalidRelativePosition)
, m_LookDownRelativeOffset(m_Metadata.m_UnarmedLookDownRelativeOffset)
, m_CurrentControlHelper(NULL)
, m_AimControlHelper(NULL)
, m_LookBehindEnvelope(NULL)
, m_HintHelper(NULL)
, m_pMeleeTarget(NULL)
, m_pJackedPed(NULL)
, m_pVehicle(NULL)
, m_PovVehicleCameraMetadata(NULL)
, m_CameraHeading(0.0f)
, m_CameraPitch(0.0f)
, m_SavedHeading(0.0f)
, m_SavedPitch(0.0f)
, m_PreviousAnimatedHeading(FLT_MAX)
, m_PreviousAnimatedPitch(FLT_MAX)
, m_ExitVehicleHeading(0.0f)
, m_ExitVehiclePitch(0.0f)
, m_ExitVehicleRoll(0.0f)
, m_AttachParentHeading(0.0f)
, m_AttachParentPitch(0.0f)
, m_DesiredFov(m_Metadata.m_BaseFov)
, m_AnimatedFov(0.0f)
, m_AnimatedFovOnPreviousFrame(0.0f)
, m_BlendedAnimatedFov(0.0f)
, m_AnimatedFovForBlending(0.0f)
, m_AnimatedWeight(0.0f)
, m_GroundAvoidLookDirection(0.0f)
, m_GroundAvoidCurrentAngle(0.0f)
, m_ScopeFov(0.0f)
, m_AimFovMin(0.0f)
, m_AimFovMax(0.0f)
, m_RelativeHeading(0.0f)
, m_RelativePitch(0.0f)
, m_PreviousRelativeHeading(0.0f)
, m_PreviousRelativePitch(0.0f)
, m_MeleeRelativeHeading(0.0f)
, m_MeleeRelativePitch(0.0f)
, m_SavedRelativeHeading(0.0f)
, m_SavedRelativePitch(0.0f)
, m_IdleHeading(0.0f)
, m_IdlePitch(0.0f)
, m_IdleRelativeHeading(0.0f)
, m_IdleRelativePitch(0.0f)
, m_MotionBlurPreviousAttachParentHealth(0.0f)
, m_MotionBlurPeakAttachParentDamage(0.0f)
, m_RelativePitchOffset(0.0f)							// TODO: remove if not used
, m_ShoesOffset(0.0f)
, m_PreviousCameraFov(m_Metadata.m_BaseFov)
, m_VehSeatInterp(0.0f)
, m_VehicleSeatBlendDistance(-1.0f)
, m_ExitInterpRate(1.0f)
, m_TaskPlayerState(0)
, m_VehicleSeatIndex(-1)
, m_VehicleTargetSeatIndex(-1)
, m_VehicleInitialSeatIndex(-1)
, m_AnimBlendInDuration(-1)
, m_AnimBlendOutDuration(-1)
, m_StopTimeOfProneMeleeAttackLook(0)
, m_StartTakeOutPhoneTime(0)
, m_StartPutAwayPhoneTime(0)
, m_HeadSpineBlendInTime(0)
, m_HeadSpineRagdollBlendTime(0)
, m_AnimBlendInStartTime(0)
, m_AnimBlendOutStartTime(0)
, m_MotionBlurForAttachParentDamageStartTime(0)
, m_DeathShakeHash(0)
, m_CoverHeadingCorrectIdleInputTime(0)
, m_ExtraVehicleEntryDelayStartTime(0)
, m_BlendoutHeadOffsetForCarJackStart(0)
, m_StickyAimTargetPosition(g_InvalidPosition)
, m_StickyAimEnvelope(NULL)
, m_StickyAimEnvelopeLevel(0.0f)
, m_StickyAimHeadingRate(0.0f)
, m_fTimePushingLookAroundHeading(0.0f)
, m_fTimePushingLookAroundPitch(0.0f)
, m_TimeSetStickyAimTarget(0)
, m_CarJackStartTime(0)
, m_AnimatedFovBlendInStartTime(0)
, m_AnimatedFovBlendOutStartTime(0)
#if KEYBOARD_MOUSE_SUPPORT
, m_LastMouseInputTime(0)
#endif
, m_LockOnTargetPosition(g_InvalidPosition)
, m_LockOnTargetPositionLocal(g_InvalidPosition)
, m_LockOnFineAimOffset(VEC3_ZERO)
, m_LockOnEnvelope(NULL)
, m_LockOnTargetBlendStartTime(0)
, m_LockOnTargetBlendDuration(0)
, m_LockOnEnvelopeLevel(0.0f)
, m_LockOnTargetBlendLevelOnPreviousUpdate(0.0f)
, m_fExtraLookBehindPitchLimitDegrees(0.0f)
, m_ScopeOffsetSquareMagnitude(0.0f)
, m_AnimatedCameraBlendLevel(0.0f)
, m_AnimatedCameraHeadingAccumulator(0.0f)
, m_AnimatedCameraPitchAccumulator(0.0f)
, m_VehicleEntryExitPushAheadOffset(0.0f)
, m_fCustomPitchBlendingAccumulator(0.0f)
, m_BlendFovDelta(0.0f)
, m_PovCameraNearClip(0.05f)
, m_StealthOffsetBlendingAccumulator(0.0f)
, m_SprintBreakOutHeading(0.0f)
, m_SprintBreakOutHoldAtEndTime(0.0f)
, m_SprintBreakOutTotalTime(0.0f)
, m_SprintBreakOutType(SBOST_SPRINTING)
, m_CoverHeadingLastAngleOffset(HALF_PI)
, m_AnimatedBlendOutPhaseStartLevelPitch(-1.0f)
, m_RelativePeekRoll(0.0f)
, m_EntryHeadingLimitScale(1.0f)
, m_CoverVaultStartHeading(FLT_MAX)
, m_AccumulatedInputTimeWhileSprinting(0.0f)
, m_BlendFovDeltaStartTime(0)
, m_BlendFovDeltaDuration(1000)
, m_BaseFov(m_Metadata.m_BaseFov)
, m_AllowStrafe( true )
, m_AttachedIsUnarmed( true )
, m_AttachedIsRNGOnlyWeapon( false)
, m_AttachedIsThrownSpecialWeapon(false)
, m_bSteerCameraWithLeftStick( false )
, m_WasClimbingLadder( false )
, m_WasClimbing( false )
, m_bUsingMobile( false )
, m_WasUsingMobile( false )
, m_bMobileAtEar(false)
, m_bParachuting( false )
, m_bWasParachuting( false )
, m_bParachutingOpenedChute( false )
, m_bDiving( false )
, m_bWasDiving( false )
, m_bPlantingBomb( false )
, m_bWasPlantingBomb( false )
, m_WasDoingMelee( false )
, m_MeleeAttack( false )
, m_DoingStealthKill( false )
, m_WasStealthKill( false )
, m_bWasFalling( false )
, m_bWasUsingHeadSpineOrientation( false )
, m_WasInCover( false )
, m_CoverVaultWasFacingLeft( false )
, m_CoverVaultWasFacingRight( false )
, m_bHaveMovedCamDuringCoverEntry( false )
, m_IsAiming( false )
, m_IsScopeAiming( false )
, m_StartedAimingWithInputThisUpdate( false )
, m_ShowReticle( false )
, m_IsLockedOn(false)
, m_HasStickyTarget(false)
, m_ShouldBlendLockOnTargetPosition(false)
, m_WeaponBlocked( false )
, m_WeaponReloading( false )
, m_bUseAnimatedFirstPersonCamera( false )
, m_bWasUsingAnimatedFirstPersonCamera( false )
, m_BlendOrientation( false )
, m_WasCarSlide( false )
, m_WasDropdown( false )
, m_bWasCustomSituation( false )
, m_LookAroundDuringHeadSpineOrient( false )
, m_LookDownDuringHeadSpineOrient( false )
, m_bWasHotwiring( false )
, m_BlendFromHotwiring( false )
, m_VehicleDistanceReset( false )
, m_bLookingBehind( false )
, m_bWasLookingBehind( false )
, m_bLookBehindCancelledForAiming( false )
, m_ShouldRotateRightForLookBehind( false )
, m_bWaitForLookBehindUp(false)
, m_bIgnoreHeadingLimits( false )
, m_bDoingLookBehindTransition( false )
, m_EnteringVehicle( false )
, m_ExitingVehicle( false )
, m_JackedFromVehicle( false )
, m_AllowLookAtVehicleJacker( false )
, m_IsCarJacking( false )
, m_IsCarJackingIgnoreThreshold( false )
, m_VehicleHangingOn( false )
, m_bEnteringTurretSeat( false )
, m_DidSeatShuffle( false )
, m_IsSeatShuffling( false )
, m_EnterGoToDoor( false )
, m_EnteringSeat( false )
, m_EnterVehicleAlign( false )
, m_EnterOpenDoor( false )
, m_EnterPickUpBike( false )
, m_EnterJackingPed( false )
, m_WasEnteringVehicle( false )
, m_ClosingDoorFromOutside( false )
, m_PovClonedDataValid( false )
, m_CarJackVictimCrossedThreshold( false )
, m_ResetEntryExitSprings( false )
, m_CoverHeadingCorrectionState( COVER_HEADING_CORRECTION_INVALID )
, m_CoverHeadingCorrectionPreviousState( COVER_HEADING_CORRECTION_INVALID )
, m_bAnimatedCameraBlendInComplete( false )
, m_bUsingHeadAttachOffsetInVehicle( false )
, m_bEnterBlending( false )
, m_bEnterBlendComplete( false )
, m_MovingUpLadder( true )
, m_UpdateAnimCamDuringBlendout( false )
, m_ActualAnimatedYawSpringInput( true )
, m_ActualAnimatedPitchSpringInput( true )
, m_ActualAnimatedYawSpringInputChanged( false )
, m_AnimatedYawSpringInput( true )
, m_AnimatedPitchSpringInput( true )
, m_TreatNMTasksAsTakingFullDamage( false )
, m_TakingOffHelmet( false )
, m_RunningAnimTask( false )
, m_WasRunningAnimTaskWithAnimatedCamera( false )
, m_AnimTaskUseHeadSpine( false )
, m_TaskControllingMovement( false )
, m_AnimatedCameraStayBlocked( false )
, m_PlayerGettingUp( false )
, m_SlopeScrambling( false )
, m_CombatRolling( false )
, m_WasCombatRolling( false )
, m_bHighFalling( false )
, m_TaskFallFalling( false )
, m_TaskFallLandingRoll( false )
, m_ForceLevelPitchOnAnimatedCamExit(false)
, m_TaskFallParachute( false )
, m_SprintBreakOutTriggered( false )
, m_SprintBreakOutFinished( false )
, m_IsIdleCameraActive( false )
, m_ScriptPreventingOrientationResetWhenRenderingAnotherCamera(false)
#if RSG_PC
, m_TripleHead( false )
#endif
#if KEYBOARD_MOUSE_SUPPORT
, m_PredictedOrientationValid( false )
#endif
, m_bShouldGoIntoFallbackMode( false )
, m_bStealthTransition( false )
, m_CoverHeadingCorrectionDetectedStickInput(false)
, m_SwitchedWeaponWithWeaponWheelUp( false )
, m_CameraRelativeAnimatedCameraInput( false )
{
	m_AimControlHelper = camFactory::CreateObject<camControlHelper>(m_Metadata.m_AimControlHelperRef.GetHash());
	cameraAssertf(m_AimControlHelper, "An aim camera (name: %s, hash: %u) failed to create a control helper (name: %s, hash: %u)",
		GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_AimControlHelperRef.GetCStr()), m_Metadata.m_AimControlHelperRef.GetHash());

	const camEnvelopeMetadata* envelopeLookBehindMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LookBehindEnvelopeRef.GetHash());
	if(envelopeLookBehindMetadata)
	{
		m_LookBehindEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeLookBehindMetadata);
		cameraAssertf(m_LookBehindEnvelope, "A first-person aim camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeLookBehindMetadata->m_Name.GetCStr()), envelopeLookBehindMetadata->m_Name.GetHash());
	}

	const camEnvelopeMetadata* envelopeLockOnMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LockOnEnvelopeRef.GetHash());
	if(envelopeLockOnMetadata)
	{
		m_LockOnEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeLockOnMetadata);
		if(cameraVerifyf(m_LockOnEnvelope, "A first-person aim camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeLockOnMetadata->m_Name.GetCStr()), envelopeLockOnMetadata->m_Name.GetHash()))
		{
			m_LockOnEnvelope->SetUseGameTime(true);
		}
	}

	const camEnvelopeMetadata* envelopeStickyAimMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_StickyAimEnvelopeRef.GetHash());
	if(envelopeStickyAimMetadata)
	{
		m_StickyAimEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeStickyAimMetadata);
		if(cameraVerifyf(m_StickyAimEnvelope, "A first-person aim camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeStickyAimMetadata->m_Name.GetCStr()), envelopeStickyAimMetadata->m_Name.GetHash()))
		{
			m_StickyAimEnvelope->SetUseGameTime(true);
		}
	}

	m_AnimatedCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
	m_AnimatedCamSpringPitchLimitsRadians = m_Metadata.m_OrientationSpring.m_PitchLimits;
	m_CurrentCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
	m_CurrentCamSpringPitchLimitsRadians = m_Metadata.m_OrientationSpring.m_PitchLimits;
	m_DesiredAnimatedCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
	m_DesiredAnimatedCamSpringPitchLimitsRadians = m_Metadata.m_OrientationSpring.m_PitchLimits;

	m_MeleeRelativeHeadingSpring.Reset(0.0f);
	m_MeleeRelativePitchSpring.Reset(0.0f);

	// Pointer used to track which control helper we are using, do not delete this pointer.
	m_CurrentControlHelper = m_ControlHelper;
}

camFirstPersonShooterCamera::~camFirstPersonShooterCamera()
{
	if(m_LookBehindEnvelope)
	{
		delete m_LookBehindEnvelope;
		m_LookBehindEnvelope = NULL;
	}

	if(m_LockOnEnvelope)
	{
		delete m_LockOnEnvelope;
		m_LockOnEnvelope = NULL;
	}

	if(m_StickyAimEnvelope)
	{
		delete m_StickyAimEnvelope;
		m_StickyAimEnvelope = NULL;
	}

	if(m_AimControlHelper)
	{
		delete m_AimControlHelper;
		m_AimControlHelper = NULL;
	}

	if(m_HintHelper)
	{
		delete m_HintHelper;
		m_HintHelper = NULL;
	}
}

bool camFirstPersonShooterCamera::Update()
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypePed()) //Safety check.
	{
		const CPed* attachedPed = camInterface::FindFollowPed();
		if(!attachedPed)
		{
			return false;
		}

		AttachTo(attachedPed);
	}

	CPed* attachedPed = const_cast<CPed*>(static_cast<const CPed*>(m_AttachParent.Get()));
	const CWeaponInfo* pEquippedWeaponInfo = (attachedPed->GetWeaponManager() && attachedPed->GetWeaponManager()->GetEquippedWeapon()) ? attachedPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo() : NULL;

	if(m_Metadata.m_ShouldTorsoIkLimitsOverrideOrbitPitchLimits && !attachedPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
	{
		//Allow the IK pitch limits to override the defaults defined in metadata.
		//NOTE: We do not currently do this during climbing, as the limits are zeroed, which would not allow any pitching.
		m_DesiredMinPitch	= attachedPed->GetIkManager().GetTorsoMinPitch();
		m_DesiredMaxPitch	= attachedPed->GetIkManager().GetTorsoMaxPitch();
	}
	// B*2039173 - Use larger min/max pitch range when swimming underwater to allow player to swim through tighter spaces (gives us a similar pitch range to TPS)
	else if (attachedPed->GetIsFPSSwimmingUnderwater() && attachedPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask))
	{
		m_DesiredMinPitch = -CTaskMotionDiving::ms_fMaxPitch;
		m_DesiredMaxPitch = CTaskMotionDiving::ms_fMaxPitch;
	}
	else if (m_ShowReticle && pEquippedWeaponInfo && pEquippedWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_RAYMINIGUN", 0xB62D1F67))
	{
		// url:bugstar:5518472 - Overriding min/max pitch on RAYMINIGUN weapon so that we can't aim wider than third person when in FPS mode, which triggers bullet bending on tracers
		TUNE_GROUP_FLOAT(CAM_FPS, fRayMinigunMinPitch, -45.0f, -90.0f, 0.0f, 0.01f);
		TUNE_GROUP_FLOAT(CAM_FPS, fRayMinigunMaxPitch, 45.0f, 0.0f, 90.0f, 0.01f);
		m_DesiredMinPitch = fRayMinigunMinPitch * DtoR;
		m_DesiredMaxPitch = fRayMinigunMaxPitch * DtoR;
	}
	else
	{
		m_DesiredMinPitch = m_Metadata.m_MinPitch * DtoR;
		m_DesiredMaxPitch = m_Metadata.m_MaxPitch * DtoR;
	}

	// Lerp to the desired min/max pitches
	static dev_float fPitchLerpRate = 0.15f;
	if (m_DesiredMinPitch != m_MinPitch)
	{
		m_MinPitch = Lerp(fPitchLerpRate, m_MinPitch, m_DesiredMinPitch);
	}
	if (m_DesiredMaxPitch != m_MaxPitch)
	{
		m_MaxPitch = Lerp(fPitchLerpRate, m_MaxPitch, m_DesiredMaxPitch);
	}	

	CreateOrDestroyHintHelper();

	UpdateTaskState(attachedPed);
	UpdateWeaponState(attachedPed);
	UpdateVehicleState(attachedPed);
	UpdateStrafeState();
	UpdateAttachPedPelvisOffsetSpring();
	UpdateBaseFov();
	UpdateAnimatedCameraData(attachedPed);
	UpdateLookBehind(attachedPed);
	CloneCinematicCamera();						// must be done after UpdateAnimatedCameraData is called.

	bool hasSucceeded	= camFirstPersonAimCamera::Update();

	UpdateConstrainToFallBackPosition(*attachedPed);

	UpdateDofAimingBehaviour();
	UpdateDofMobileBehaviour();
	UpdateDofHighCoverBehaviour();
	UpdateDofParachuteBehaviour();

	if(m_NumUpdatesPerformed == 0)
	{
		attachedPed->UpdateMovementMode();
	}

	// This motion blur update adds onto one generated by CAimCamera.
	UpdateMotionBlur();

	if( attachedPed )
	{
		// For blending, save the current camera position relative to player's mover.
		Mat34V pedMatrix = attachedPed->GetMatrix();
		m_PreviousRelativeCameraPosition = UnTransformFull( pedMatrix, RCC_VEC3V(m_Frame.GetPosition()) );
	}

	m_IsIdleCameraActive = false;

	// Reset
	if (m_bStealthTransition && (m_StealthOffsetBlendingAccumulator == 0.0f) && (rage::Abs(m_AttachPedPelvisOffsetSpring.GetResult()) < SMALL_FLOAT))
	{
		m_bStealthTransition = false;
	}

	if (m_StickyAimHelper)
	{
		m_StickyAimHelper->PostCameraUpdate(m_Frame.GetPosition(), m_Frame.GetFront());
	}

	return hasSucceeded;
}

void camFirstPersonShooterCamera::CreateOrDestroyHintHelper()
{
	if(camInterface::GetGameplayDirector().IsHintActive())
	{
		if(!m_HintHelper)
		{
			const camHintHelperMetadata* hintHelperMetadata =
				camFactory::FindObjectMetadata<camHintHelperMetadata>(m_Metadata.m_HintHelperRef.GetHash());

			if(hintHelperMetadata)
			{
				//NOTE: We only allow the hint helper to be overridden if we already have a valid hint helper defined for this camera.
				const atHashWithStringNotFinal& overriddenHintHelperRef = camInterface::GetGameplayDirector().GetHintHelperToCreate();
				if(overriddenHintHelperRef.GetHash() > 0)
				{
					const camHintHelperMetadata* overriddenHintHelperMetadata =
						camFactory::FindObjectMetadata<camHintHelperMetadata>(overriddenHintHelperRef.GetHash());

					if(cameraVerifyf(overriddenHintHelperMetadata,
						"A first-person shooter camera (name: %s, hash: %u) failed to find the metadata for an overridden hint helper (name: %s, hash: %u)",
						GetName(), GetNameHash(), SAFE_CSTRING(overriddenHintHelperRef.GetCStr()), overriddenHintHelperRef.GetHash()))
					{
						hintHelperMetadata = overriddenHintHelperMetadata;
					}
				}

				m_HintHelper = camFactory::CreateObject<camHintHelper>(*hintHelperMetadata);
				cameraAssertf(m_HintHelper, "A first-person shooter camera (name: %s, hash: %u) failed to create a hint helper (name: %s, hash: %u)",
					GetName(), GetNameHash(), SAFE_CSTRING(hintHelperMetadata->m_Name.GetCStr()), hintHelperMetadata->m_Name.GetHash());
			}
		}
	}
	else if(m_HintHelper)
	{
		delete m_HintHelper; 
		m_HintHelper = NULL; 
	}
}

void camFirstPersonShooterCamera::TransformAttachPositionToCameraSpace(const Vector3& relativeAttachPosition, Vector3& attachPosition, bool UNUSED_PARAM(bMakeCameraMatrixUpright), const Matrix34* pCameraMatrix)
{
	Matrix34 worldMatrix = (pCameraMatrix) ? *pCameraMatrix : m_Frame.GetWorldMatrix();
	////if(bMakeCameraMatrixUpright)
	////{
	////	worldMatrix.b.z = 0.0f;
	////	worldMatrix.c = rage::ZAXIS;
	////	worldMatrix.b.NormalizeSafe();
	////	worldMatrix.a.Cross( worldMatrix.b, worldMatrix.c );
	////}

	worldMatrix.Transform3x3(relativeAttachPosition, attachPosition);
}

void camFirstPersonShooterCamera::UpdateDofAimingBehaviour()
{
	const float desiredAimingLevel	= (m_IsAiming && !m_WeaponBlocked) ? 1.0f : 0.0f;
	const bool shouldCutSpring		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_AimingBlendSpringConstant;

	m_DofAimingBlendSpring.Update(desiredAimingLevel, springConstant);
}

void camFirstPersonShooterCamera::UpdateDofMobileBehaviour()
{
	const float desiredMobileLevel	= m_bUsingMobile ? 1.0f : 0.0f;
	const bool shouldCutSpring		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
		m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_MobileBlendSpringConstant;

	m_DofMobileBlendSpring.Update(desiredMobileLevel, springConstant);
}

void camFirstPersonShooterCamera::UpdateDofHighCoverBehaviour()
{	
	if(!(m_AttachParent && m_AttachParent->GetIsTypePed()))
	{		
		return;
	}

	const CPed* attachedPed = static_cast<const CPed*>(m_AttachParent.Get());

	bool shouldIncreaseNearDof = false;		

	if(!CTaskCover::CanUseThirdPersonCoverInFirstPerson(*attachedPed))
	{		
		const CPedIntelligence* pedIntelligence = attachedPed->GetPedIntelligence();

		if(pedIntelligence)
		{			
			const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_COVER));

			if(pCoverTask)
			{			
				bool isInHighCover = pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint);

				bool isAimingOrFiring = false;

				if(pCoverTask->GetSubTask() && pCoverTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_IN_COVER)
				{					
					if(	pCoverTask->GetSubTask()->GetState() == CTaskInCover::State_AimIntro ||
						pCoverTask->GetSubTask()->GetState() == CTaskInCover::State_Aim ||
						pCoverTask->GetSubTask()->GetState() == CTaskInCover::State_AimOutro ||
						pCoverTask->GetSubTask()->GetState() == CTaskInCover::State_BlindFiring)
					{
						isAimingOrFiring = true;
					}
				}	

				shouldIncreaseNearDof =  isInHighCover && !isAimingOrFiring;
			}
		}
	}

	// Aiming state of cover tasks
	const float desiredHighCoverLevel	= shouldIncreaseNearDof ? 1.0f : 0.0f;
	const bool shouldCutSpring			= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
		m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant			= shouldCutSpring ? 0.0f : m_Metadata.m_HighCoverBlendSpringConstant;

	m_DofHighCoverBlendSpring.Update(desiredHighCoverLevel, springConstant);
}

void camFirstPersonShooterCamera::UpdateDofParachuteBehaviour()
{
	if(!(m_AttachParent && m_AttachParent->GetIsTypePed()))
	{		
		return;
	}

	const CPed* attachedPed = static_cast<const CPed*>(m_AttachParent.Get());

	const CPedIntelligence* pedIntelligence = attachedPed->GetPedIntelligence();

	bool isGunAimingOrFiring = false;

	if(pedIntelligence)
	{			
		const CTaskGun* pGunTask = static_cast<const CTaskGun*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_GUN));

		if(pGunTask)
		{			
			isGunAimingOrFiring = pGunTask->GetIsAiming() || pGunTask->GetIsFiring();
		}
	}
	
	const float desiredParachuteLevel	= (m_bParachutingOpenedChute && !isGunAimingOrFiring && !m_bUsingMobile) ? 1.0f : 0.0f;
	const bool shouldCutSpring			= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
		m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant			= shouldCutSpring ? 0.0f : m_Metadata.m_ParachuteBlendSpringConstant;

	m_DofParachuteBlendSpring.Update(desiredParachuteLevel, springConstant);
}

void camFirstPersonShooterCamera::UpdateTaskState(const CPed* attachPed)
{
	m_AnimTaskUseHeadSpine = false;
	m_TaskControllingMovement = false;
	m_CombatRolling = false;
	m_TaskFallFalling = false;
	m_TaskFallLandingRoll = false;
	m_TaskFallParachute = false;
#if RSG_PC
	m_TripleHead = false;
#endif

	bool bIsRunningAnimTask = false;
	if( attachPed && attachPed->GetPedIntelligence() )
	{
		bool bAvoidCameraRelativeAnimatedCameraInput = false;
		const CPedIntelligence* pedIntelligence = attachPed->GetPedIntelligence();

		// Do not check secondary task tree.
		CTaskSynchronizedScene* pTaskSyncScene = (CTaskSynchronizedScene*)(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
		if( pTaskSyncScene && pTaskSyncScene->GetState() == CTaskSynchronizedScene::State_RunningScene )
		{
			bIsRunningAnimTask = true;
			bAvoidCameraRelativeAnimatedCameraInput = (	pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("mini@prostitutes@sexnorm_veh_first_person", 0xF72790A8) ||
										                pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("mini@prostitutes@sexlow_veh_first_person", 0x3A68E23A)  ||
                                                        pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@rail@standing@male@variant_01@", 0x76274850) ||
                                                        pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@rail@standing@female@variant_01@", 0x4F508A68) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@bow@male@variation_01@", 0xD422E903) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@bow@female@variation_01@", 0x883E1A2B) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@jacuzzi@seated@female@variation_01@", 0xB5DE9795) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@jacuzzi@seated@female@variation_02@", 0x3F948FCB) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@jacuzzi@seated@female@variation_03@", 0x65505C3A) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@jacuzzi@seated@male@variation_01@", 0x60D7BDC5) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@jacuzzi@seated@male@variation_02@", 0xF3D8E2E9) ||
														pTaskSyncScene->GetDictionary().GetHash() == ATSTRINGHASH("anim@amb@yacht@jacuzzi@seated@male@variation_03@", 0x57201B7A));
		}

		// Do not check secondary task tree.
		CTaskUseScenario* pTaskScenario = (CTaskUseScenario*)(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
		if( pTaskScenario )
		{
			////m_AnimTaskUseHeadSpine = (pTaskScenario->GetState() != CTaskUseScenario::State_PlayAmbients);
			bIsRunningAnimTask = true;
		}

		// Do not check secondary task tree.
		CTaskScriptedAnimation* pTaskScriptedAnim = (CTaskScriptedAnimation*)(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
		if( pTaskScriptedAnim )
		{
			////m_AnimTaskUseHeadSpine = true;
			bIsRunningAnimTask = true;
		}
		else
		{
			pTaskScriptedAnim = (CTaskScriptedAnimation*)(pedIntelligence->FindTaskSecondaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
			if( pTaskScriptedAnim )
			{
				////m_AnimTaskUseHeadSpine = true;
				bIsRunningAnimTask = true;
			}
		}

		// HACK: do not set m_RunningAnimTask on the frame that there is no task, so m_CameraRelativeAnimatedCameraInput is true for an extra frame.
		// NOTE: m_RunningAnimTask is set properly at the end of this function so it is not true for an extra frame.
		m_RunningAnimTask |= bIsRunningAnimTask;
		TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, bDbgForceRelativeInputAboutCameraForward, false);
		TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, bDbgDisableRelativeInputAboutCameraForward, false);

        m_CameraRelativeAnimatedCameraInput = ((!bAvoidCameraRelativeAnimatedCameraInput && m_RunningAnimTask) || bDbgForceRelativeInputAboutCameraForward) && !bDbgDisableRelativeInputAboutCameraForward;

		CTask* pTakeOffHelmetTask = (pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_TAKE_OFF_HELMET));
		if( pTakeOffHelmetTask )
		{
			m_TakingOffHelmet = true;
		}

		//CTaskComplexControlMovement* pTaskControlMovement = (CTaskComplexControlMovement*)(pedIntelligence->FindTaskPrimaryByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
		if( m_HintHelper == NULL )
		{
			// If running a hint helper, don't rotate to ped heading, just let the hint helper do its job.
			const CTask* pTaskControlMovement = (const CTask*)(pedIntelligence->FindTaskPrimaryByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
			if( pTaskControlMovement )
			{
				m_TaskControllingMovement = true;
			}
		}

		const CTask* pTaskCombatRoll = (const CTask*)(pedIntelligence->FindTaskActiveMotionByType(CTaskTypes::TASK_COMBAT_ROLL));
		if( pTaskCombatRoll )
		{
			m_CombatRolling = true;
			m_WasCombatRolling = true;
		}

		const CTask* pFallTask = (const CTask*)pedIntelligence->FindTaskByType(CTaskTypes::TASK_FALL);
		if( pFallTask )
		{
			m_TaskFallFalling     = (pFallTask->GetState() == CTaskFall::State_Fall || pFallTask->GetState() == CTaskFall::State_Initial);
			m_TaskFallLandingRoll = (pFallTask->GetState() == CTaskFall::State_Landing);
			m_TaskFallParachute = (pFallTask->GetState() == CTaskFall::State_Parachute);
			if(!m_TaskFallLandingRoll && m_TaskFallParachute)
			{
				const CTask* pParachuteTask = (const CTask*)pedIntelligence->FindTaskByType(CTaskTypes::TASK_PARACHUTE);
				m_TaskFallLandingRoll = (pParachuteTask && pParachuteTask->GetState() == CTaskParachute::State_Landing);
			}
		}

		const CTaskNMHighFall* pTaskHighFall = (const CTaskNMHighFall*)pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_NM_HIGH_FALL);
		m_bHighFalling = (pTaskHighFall != NULL);

		if(m_TaskFallLandingRoll)
		{
			m_ForceLevelPitchOnAnimatedCamExit = true;
		}

		if (!CTaskCover::CanUseThirdPersonCoverInFirstPerson(*attachPed))
		{
			const CTaskCover* pCoverTask = (const CTaskCover*)pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_COVER);
			if (pCoverTask)
			{
				if (attachPed->GetPlayerInfo() && !attachPed->GetPlayerInfo()->IsAiming())
				{
					if(m_CurrentControlHelper)
					{
						m_CurrentControlHelper->SetFirstPersonAimSensitivityLimitThisUpdate( m_Metadata.m_CoverSettings.m_AimSensitivityClampWhileInCover );
					}
				}
			}
		}

		TUNE_GROUP_FLOAT(CAM_FPS_MELEE, fDistanceToClearTarget, 2.0f, 0.10f, 10.0f, 0.01f);
		bool bWasMeleeAttack = m_MeleeAttack;
		const CTaskMelee* pMeleeTask = pedIntelligence->GetTaskMelee();
		if( pMeleeTask )
		{
			const CTaskMeleeActionResult* pMeleeActionResult = pedIntelligence->GetTaskMeleeActionResult();
			if( pMeleeTask->GetTargetEntity() && pMeleeTask->GetTargetEntity()->GetIsTypePed() )
			{
				m_pMeleeTarget = (CPed*)pMeleeTask->GetTargetEntity();
			}
			else
			{
				if( pMeleeActionResult && pMeleeActionResult->GetTargetEntity() && pMeleeActionResult->GetTargetEntity()->GetIsTypePed() )
				{
					m_pMeleeTarget = (CPed*)pMeleeActionResult->GetTargetEntity();
				}
			}

			m_MeleeAttack = (pMeleeTask->GetState() == CTaskMelee::State_MeleeAction || 
							 pMeleeTask->GetState() == CTaskMelee::State_WaitForMeleeActionDecide ||
							 pMeleeTask->GetState() == CTaskMelee::State_WaitForMeleeAction);

			m_DoingStealthKill = false;
			const CActionResult* pActionResult = pMeleeActionResult ? pMeleeActionResult->GetActionResult() : NULL;
			if( pActionResult )
			{
				m_DoingStealthKill = pActionResult->GetIsAStealthKill() || pActionResult->GetIsATakedown(); 
			}

			if( !bWasMeleeAttack && m_MeleeAttack )
			{
				m_StopTimeOfProneMeleeAttackLook = 0;

				// Do not reset relative heding when melee attack starts, it will be blended out when it ends.
				// Problem is when interrupting an attack, resetting will cause a pop.
				////m_MeleeRelativeHeadingSpring.Reset();
				////m_MeleeRelativePitchSpring.Reset();
				////m_MeleeRelativeHeading = 0.0f;
				////m_MeleeRelativePitch = 0.0f;
			}
		}
		else
		{
			m_MeleeAttack = false;

			// If we still have a melee target, clear it if we get too far away.
			if(m_pMeleeTarget)
			{
				Vec3V vMeleeTargetPos = m_pMeleeTarget->GetMatrix().GetCol3();
				Vec3V vPlayerPos      = attachPed->GetMatrix().GetCol3();
				ScalarV distance = MagSquared(vMeleeTargetPos - vPlayerPos);
				if( IsTrue(IsGreaterThan(distance, ScalarV(rage::square(fDistanceToClearTarget)))) )
				{
					m_pMeleeTarget.Reset(NULL);
				}
				else if(!m_IsLockedOn)
				{
					CControl& rControl = CControlMgr::GetMainPlayerControl(true);
					Vector2 vLeftStick( rControl.GetPedWalkLeftRight().GetNorm(), -rControl.GetPedWalkUpDown().GetNorm());
					Vector2 vRightStick(rControl.GetPedLookLeftRight().GetNorm(), -rControl.GetPedLookUpDown().GetNorm());
					const bool bCancelForStickInput = (vLeftStick.Mag2() > rage::square(0.01f) || vRightStick.Mag2() > rage::square(0.01f));
					if(bCancelForStickInput)
						m_pMeleeTarget.Reset(NULL);
				}
			}
		}

		m_PlayerGettingUp = pedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP, true);

		m_bStealthTransition |= !attachPed->GetMotionData()->GetIsUsingStealthInFPS() && (attachPed->GetMotionData()->GetWasUsingStealthInFPS() || attachPed->GetIkManager().IsFirstPersonStealthTransition());
	}

	m_RunningAnimTask = bIsRunningAnimTask;

#if RSG_PC
		m_TripleHead = camInterface::IsTripleHeadDisplay();
#endif
}

void camFirstPersonShooterCamera::UpdateWeaponState(const CPed* attachedPed)
{
	m_AttachedIsUnarmed = true;
	m_AttachedIsUsingMelee = false;
	m_AttachedIsRNGOnlyWeapon = false;
	m_AttachedIsThrownSpecialWeapon = false;

	if (attachedPed)
	{
		m_WeaponBlocked = false;
		m_WeaponReloading = false;
		if( attachedPed->GetPedIntelligence() )
		{
			if(CNewHud::IsWeaponWheelActive() && !m_SwitchedWeaponWithWeaponWheelUp)
			{
				m_SwitchedWeaponWithWeaponWheelUp = attachedPed->GetMotionData() && attachedPed->GetMotionData()->GetWasFPSUnholster();
			}
			else if(!CNewHud::IsWeaponWheelActive())
			{
				m_SwitchedWeaponWithWeaponWheelUp = false;
			}

			const CTaskWeaponBlocked* pWeaponBlockedTask = (const CTaskWeaponBlocked*)( attachedPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_WEAPON_BLOCKED ) );
			m_WeaponBlocked = (pWeaponBlockedTask != NULL);

			const CTaskReloadGun* pReloadTask = (const CTaskReloadGun*)( attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN) );
			m_WeaponReloading = pReloadTask != NULL && !pReloadTask->IsReloadFlagSet(RELOAD_IDLE);
		}

		const CPedWeaponManager* pWeaponManager = attachedPed->GetWeaponManager();
		if( pWeaponManager )
		{
			Vector3 desiredScopeOffset(Vector3::ZeroType);
			Vector3 desiredScopeRotOffset(Vector3::ZeroType);
			Vector3 desiredNonScopeOffset(Vector3::ZeroType);
			Vector3 desiredNonScopeRotOffset(Vector3::ZeroType);
			Vector3 desiredThirdPersonOffset(Vector3::ZeroType);
			const CWeaponInfo* pEquippedWeaponInfo = (pWeaponManager->GetEquippedWeapon()) ? pWeaponManager->GetEquippedWeapon()->GetWeaponInfo() : NULL ;
			if( pEquippedWeaponInfo )
			{
				Vector3 setScopeOffset(Vector3::ZeroType);
				Vector3 setScopeRotOffset(Vector3::ZeroType);
				Vector3 setNonScopeOffset(Vector3::ZeroType);
				Vector3 setNonScopeRotOffset(Vector3::ZeroType);

				m_AimFovMin = pEquippedWeaponInfo->GetFirstPersonAimFovMin();
				m_AimFovMax = pEquippedWeaponInfo->GetFirstPersonAimFovMax();

				if( pEquippedWeaponInfo->GetEnableFPSRNGOnly() || pEquippedWeaponInfo->GetEnableFPSIdleOnly())
				{
					m_AttachedIsRNGOnlyWeapon = true;
				}

				m_AttachedIsThrownSpecialWeapon = ( pEquippedWeaponInfo->GetWeaponWheelSlot() == WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL );

				// Don't consider objects (ie mobile phone) as unarmed. This was causing m_AllowStrafe to be set to false (we force strafe for phone in FPS mode).
				if ( (pEquippedWeaponInfo->GetIsUnarmed() || pEquippedWeaponInfo->GetIsMeleeFist()) && pEquippedWeaponInfo->GetHash() != ATSTRINGHASH("OBJECT", 0x39958261))
				{
					m_AttachedIsUnarmed = true;
					m_ScopeFov			= 0.0f;
				}
				else if( pEquippedWeaponInfo->GetIsMelee() )
				{
					m_AttachedIsUnarmed = false;
					m_AttachedIsUsingMelee	= true;
					m_ScopeFov				= 0.0f;
				}
				else
				{
					m_AttachedIsUnarmed	= false;

					const CWeapon* pEquippedWeapon = pWeaponManager->GetEquippedWeapon();
					if (!pEquippedWeapon || !pEquippedWeapon->GetScopeComponent())
					{
						setScopeOffset		= pEquippedWeaponInfo->GetFirstPersonScopeOffset();
						setScopeRotOffset	= pEquippedWeaponInfo->GetFirstPersonScopeRotationOffset();
						m_ScopeFov			= pEquippedWeaponInfo->GetFirstPersonScopeFov();
					}
					else
					{
						bool bFoundFPScopeAttachmentData = false;
						const CWeaponComponentScope* pScopeComponent = pEquippedWeapon->GetScopeComponent();
						if (pScopeComponent && pScopeComponent->GetInfo())
						{
							const CWeaponInfo::FPScopeAttachData& scopeAttachData = pEquippedWeaponInfo->GetFirstPersonScopeAttachmentData();
							for (u32 i = 0; i < scopeAttachData.GetCount(); i++)
							{
								CWeaponInfo::sFirstPersonScopeAttachmentData scopeData = scopeAttachData[i];
								if (scopeData.m_Name == pScopeComponent->GetInfo()->GetHash())
								{
									bFoundFPScopeAttachmentData = true;
									
									setScopeOffset		= scopeData.m_FirstPersonScopeAttachmentOffset;
									setScopeRotOffset	= scopeData.m_FirstPersonScopeAttachmentRotationOffset;
									m_ScopeFov			= scopeData.m_FirstPersonScopeAttachmentFov;

									break;
								}
							}
						}

						if (!bFoundFPScopeAttachmentData)
						{
							setScopeOffset		= pEquippedWeaponInfo->GetFirstPersonScopeAttachmentOffset();
							setScopeRotOffset	= pEquippedWeaponInfo->GetFirstPersonScopeAttachmentRotationOffset();
							m_ScopeFov			= pEquippedWeaponInfo->GetFirstPersonScopeAttachmentFov();
						}

						if(m_ScopeFov == 0.0f)
						{
							m_ScopeFov		= pEquippedWeaponInfo->GetFirstPersonScopeFov();
						}
					}

					GetFirstPersonNonScopeOffsets(attachedPed, pEquippedWeaponInfo, setNonScopeOffset, setNonScopeRotOffset);
					GetFirstPersonAsThirdPersonOffset(attachedPed, pEquippedWeaponInfo, desiredThirdPersonOffset);
				}

				const bool bAllowCustomFraming = m_bUsingMobile || (!m_WeaponBlocked && !m_WeaponReloading);
				if( bAllowCustomFraming && attachedPed->GetMotionData() )
				{
					if( attachedPed->GetMotionData()->GetIsFPSScope() || camInterface::GetGameplayDirector().GetJustSwitchedFromSniperScope())
					{
						desiredScopeOffset = setScopeOffset;
						desiredScopeRotOffset = setScopeRotOffset;
						m_ScopeOffsetSquareMagnitude = desiredScopeOffset.Mag2();
					}
					else
					{
						desiredNonScopeOffset = setNonScopeOffset;
						desiredNonScopeRotOffset = setNonScopeRotOffset;
					}
				}
			}

			const bool c_BlendingOut = desiredScopeOffset.IsZero();
			float interpRate = (c_BlendingOut) ? m_Metadata.m_ScopeOffsetBlendOut : m_Metadata.m_ScopeOffsetBlendIn;
			if(pEquippedWeaponInfo && pEquippedWeaponInfo->GetDisableFPSScope() && attachedPed->GetPlayerInfo() && attachedPed->GetPlayerInfo()->GetInFPSScopeState())
			{
				TUNE_GROUP_FLOAT(CAM_FPS, fScopeOffsetBlendOutModifierWhenDisableFPSScope, 5.5f, 1.0f, 10.0f, 0.1f);
				interpRate *= fScopeOffsetBlendOutModifierWhenDisableFPSScope;
			}

			if(!c_BlendingOut || !m_WeaponReloading)
			{
				interpRate *= (30.0f * fwTimer::GetCamTimeStep());
			}
			else
			{
				// Railgun blend out from scope mode is too fast for railgun, so using a scale of 1.0 for that weapon.
				TUNE_GROUP_FLOAT(CAM_FPS, fWeaponReloadScopeOffsetBlendOutModifier, 5.0f, 0.001f, 10.0f, 0.001f);
				const bool c_UseSlowBlendout = pEquippedWeaponInfo && (	pEquippedWeaponInfo->GetHash() == WEAPONTYPE_DLC_RAILGUN.GetHash() ||
																		pEquippedWeaponInfo->GetHash() == WEAPONTYPE_DLC_COMBATPDW.GetHash() );
				float fBlendoutModifier = (!c_UseSlowBlendout) ? fWeaponReloadScopeOffsetBlendOutModifier : 1.0f;
				interpRate *= fBlendoutModifier;
				float fTimeScale = (30.0f * fwTimer::GetCamTimeStep());
				if(fTimeScale > 1.0f)
					interpRate *= (30.0f * fwTimer::GetCamTimeStep());
			}
			interpRate = Clamp(interpRate, 0.0f, 1.0f);

			if(m_NumUpdatesPerformed == 0 || camInterface::GetGameplayDirector().GetJustSwitchedFromSniperScope())
			{
				interpRate = 1.0f;
			}

			m_CurrentScopeOffset.Lerp(interpRate, desiredScopeOffset);
			m_CurrentScopeRotationOffset.Lerp(interpRate, desiredScopeRotOffset);
			m_CurrentNonScopeOffset.Lerp(interpRate, desiredNonScopeOffset);
			m_CurrentNonScopeRotationOffset.Lerp(interpRate, desiredNonScopeRotOffset);
			m_CurrentThirdPersonOffset.Lerp(interpRate, desiredThirdPersonOffset);

			// TODO: pop to new offset when changin' weapons?
			////if(m_IsAiming && m_DesiredScopeForwardOffset != desiredScopeOffset.GetY())
			////{
			////	m_CurrentScopeForwardOffset = m_ScopeOffset.GetY();
			////}
			////m_DesiredScopeForwardOffset = desiredScopeOffset.GetY();
		}
	}
}

void camFirstPersonShooterCamera::UpdateVehicleState(const CPed* attachedPed)
{
	float fPreviousVehSeatInterp = m_VehSeatInterp;
	bool bPreviousVehicleJackedState = m_JackedFromVehicle;
	m_bUsingHeadAttachOffsetInVehicle = false;
	m_JackedFromVehicle = false;
	m_EnterGoToDoor = false;
	m_EnteringSeat = false;
	m_EnterVehicleAlign = false;
	m_EnterPickUpBike = false;
	m_EnterJackingPed = false;
	m_EnterOpenDoor = false;
	m_IsABikeOrJetski = false;
	m_ClosingDoorFromOutside = false;
	m_IsSeatShuffling = false;
	m_EntryHeadingLimitScale = 0.70f;
	const CTaskEnterVehicle* pEnterVehicleTask = (const CTaskEnterVehicle*)attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
	const CTaskExitVehicle*  pExitVehicleTask  = (const CTaskExitVehicle*) attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
	const CTaskInVehicleSeatShuffle* pShuffleTask = (const CTaskInVehicleSeatShuffle*)attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);

	float heading;
	float pitch;
	float roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	s32 previousSeatIndex = m_VehicleSeatIndex;
	if( pEnterVehicleTask != NULL || (pExitVehicleTask == NULL && pShuffleTask != NULL) )
	{
		// Reset when enter vehicle starts.
		if(!m_EnteringVehicle)
		{
			// Do not clamp until we are within the valid range, so we blend to the range.
			m_ClampVehicleEntryHeading = false;
			m_ClampVehicleEntryPitch   = false;
			m_VehicleDistanceReset = false;
			m_CarJackVictimCrossedThreshold = false;
			m_PushAheadPitchSpring.Reset();
			m_PushAheadDistanceSpring.Reset();
			m_EnterVehicleLookOffsetSpring.Reset();
			m_VehicleEntryHeadingLimitSpring.Reset();
			m_VehicleEntryPitchLimitSpring.Reset();
			m_VehicleHeadingSpring.Reset(heading);
			m_VehiclePitchSpring.Reset(pitch);
			m_VehicleCarJackHeadingSpring.Reset(heading);
			m_VehicleCarJackPitchSpring.Reset(pitch);
			m_VehicleSeatBlendHeadingSpring.Reset(heading);
			m_VehicleSeatBlendPitchSpring.Reset(pitch);
			m_VehicleSeatBlendDistance = -1.0f;
			m_CarJackStartTime = 0;
			m_ResetEntryExitSprings = true;
			m_bEnterBlendComplete = false;
			m_bEnteringTurretSeat = false;
		}

		if(pEnterVehicleTask)
		{
			if(pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_ShuffleToSeat)
			{
				m_DidSeatShuffle = true;
				m_IsSeatShuffling = true;
			}

			m_EnterPickUpBike   = (pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_PickUpBike || pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_PullUpBike);
			m_EnterOpenDoor     = (pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_OpenDoor);
			m_EnterVehicleAlign = (pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_Align);
			m_EnterGoToDoor     = (pEnterVehicleTask->GetState() <  CTaskEnterVehicle::State_OpenDoor) && !m_EnterVehicleAlign;
			m_EnteringSeat      = (pEnterVehicleTask->GetState() >= CTaskEnterVehicle::State_EnterSeat);
			m_EnterJackingPed   = (pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_JackPedFromOutside);

			m_pVehicle = pEnterVehicleTask->GetVehicle();
			m_pJackedPed = pEnterVehicleTask->GetJackedPed();
			if(m_pJackedPed)
			{
				const CTaskExitVehicleSeat* pJackedPedExitVehicleSeatTask = (const CTaskExitVehicleSeat*)(m_pJackedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
				if(!pJackedPedExitVehicleSeatTask ||
					pJackedPedExitVehicleSeatTask->GetState() == CTaskExitVehicleSeat::State_Finish)
				{
					m_pJackedPed = NULL;
				}
			}

			m_IsCarJackingIgnoreThreshold = m_pJackedPed != NULL && m_EnterJackingPed;
			m_IsCarJacking = !m_CarJackVictimCrossedThreshold && m_IsCarJackingIgnoreThreshold;
			if(m_IsCarJacking && m_BlendoutHeadOffsetForCarJackStart == 0)
			{
				m_BlendoutHeadOffsetForCarJackStart = fwTimer::GetTimeInMilliseconds();
			}

			m_VehicleSeatIndex = 0;
			if(!m_DidSeatShuffle)
			{
				if(m_pVehicle && m_pVehicle->IsEntryIndexValid(pEnterVehicleTask->GetTargetEntryPoint()))
				{
					const CEntryExitPoint *pEntryExitPoint = m_pVehicle->GetEntryExitPoint(pEnterVehicleTask->GetTargetEntryPoint());

					m_VehicleSeatIndex = pEntryExitPoint->GetSeat(SA_directAccessSeat); 

					if(pEntryExitPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
					{
						for (s32 i=0; i<pEntryExitPoint->GetSeatAccessor().GetNumSeatsAccessible(); i++)
						{
							if( pEntryExitPoint->GetSeatAccessor().GetSeatByIndex(i) == pEnterVehicleTask->GetTargetSeat() )
							{
								m_VehicleSeatIndex = pEnterVehicleTask->GetTargetSeat();
							}
						}
					}
				}
			}
			else
			{
				m_VehicleSeatIndex = pEnterVehicleTask->GetTargetSeat();
			}

			m_VehicleTargetSeatIndex = pEnterVehicleTask->GetTargetSeat();
			m_bEnteringTurretSeat = m_pVehicle && m_pVehicle->IsEntryIndexValid(m_VehicleTargetSeatIndex) && camGameplayDirector::IsTurretSeat(m_pVehicle, m_VehicleTargetSeatIndex);
			if(!m_EnteringVehicle)
			{
				m_VehicleInitialSeatIndex = m_VehicleSeatIndex;
			}
		}
		else if(pShuffleTask)
		{
			if(pShuffleTask->GetState() == CTaskInVehicleSeatShuffle::State_Shuffle)
			{
				m_DidSeatShuffle = true;
				m_IsSeatShuffling = true;
			}
			m_pVehicle = pShuffleTask->GetVehiclePtr();

			m_VehicleSeatIndex = pShuffleTask->GetTargetSeatIndex();
			m_VehicleTargetSeatIndex = pShuffleTask->GetTargetSeatIndex();
			m_bEnteringTurretSeat = m_pVehicle && m_pVehicle->IsEntryIndexValid(m_VehicleTargetSeatIndex) && camGameplayDirector::IsTurretSeat(m_pVehicle, m_VehicleTargetSeatIndex);
			if(!m_EnteringVehicle)
			{
				m_VehicleInitialSeatIndex = m_VehicleSeatIndex;
			}
		}

		m_EnteringVehicle = true;
		m_ExitingVehicle  = false;
	}
	else if( pExitVehicleTask != NULL )
	{
		// Reset when exit vehicle starts.
		if(!m_ExitingVehicle)
		{
			m_PushAheadPitchSpring.Reset();
			m_PushAheadDistanceSpring.Reset();
			m_EnterVehicleLookOffsetSpring.Reset();
			m_VehicleEntryHeadingLimitSpring.Reset();
			m_VehicleEntryPitchLimitSpring.Reset();
			m_VehicleEntryExitPushAheadOffset = 0.0f;
			m_VehicleHeadingSpring.Reset(heading);
			m_VehiclePitchSpring.Reset(pitch);
			m_VehicleCarJackHeadingSpring.Reset(heading);
			m_VehicleCarJackPitchSpring.Reset(pitch);
			m_ExitInterpRate = 0.0f;
			m_VehicleSeatBlendDistance = -1.0f;
			m_ResetEntryExitSprings = true;
			m_bEnteringTurretSeat = false;
		}
		m_ClampVehicleEntryHeading = true;
		m_ClampVehicleEntryPitch   = true;
		m_pVehicle = pExitVehicleTask->GetVehicle();
		m_VehicleSeatIndex = pExitVehicleTask->GetTargetSeat();
		// not setting this was unintentional, but it work better without it. (skips the vehicle seat interpolation on exit and uses head orientation for exit)
		////m_VehicleTargetSeatIndex = m_VehicleSeatIndex;
		m_EnteringVehicle = false;
		m_bEnterBlending = false;
		m_bEnterBlendComplete = false;
		m_ExtraVehicleEntryDelayStartTime = 0;
		m_CarJackStartTime = 0;
		m_ExitingVehicle = true;
		m_DidSeatShuffle = false;
		m_IsCarJacking = false;
		m_IsCarJackingIgnoreThreshold = false;

		const CTaskExitVehicleSeat* pExitVehicleSeatTask = (const CTaskExitVehicleSeat*) attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT);
		m_JackedFromVehicle = (pExitVehicleSeatTask && pExitVehicleSeatTask->GetState() == CTaskExitVehicleSeat::State_BeJacked);

		const CTask* pCloseDoorFromOutside = attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_OUTSIDE);
		m_ClosingDoorFromOutside = (pCloseDoorFromOutside != NULL);

		// Setup when camera looks at vehicle jacker here, so we can optionally
		if(!bPreviousVehicleJackedState && m_JackedFromVehicle)
			m_AllowLookAtVehicleJacker = true;
	}
	else
	{
		if(m_ExitingVehicle || (!m_EnteringVehicle && !m_bEnterBlending && !m_ExitingVehicle))
		{
			m_VehicleTargetSeatIndex = -1;
			m_VehicleInitialSeatIndex = -1;
			if(m_ExitInterpRate == 1.0f)
			{
				m_VehicleSeatIndex = -1;
				m_pVehicle = NULL;
			}
		}

		// If enter vehicle task ended and we are not using the enter timer, then reset m_bEnterBlending as we are done.
		if( m_bEnterBlending && !m_bUseAnimatedFirstPersonCamera && m_ExtraVehicleEntryDelayStartTime == 0 &&
			m_pVehicle.Get() && !m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired)
		{
			m_bEnterBlending = false;
			m_bEnterBlendComplete = true;
		}

		m_ClampVehicleEntryHeading = true;
		m_ClampVehicleEntryPitch   = true;
		m_ExitingVehicle = false;
		m_EnteringVehicle = false;
		m_PovClonedDataValid = false;
		m_CarJackVictimCrossedThreshold = false;
		m_VehicleDistanceReset = false;
		m_DidSeatShuffle = false;
		m_IsCarJacking = false;
		m_IsCarJackingIgnoreThreshold = false;
		m_AllowLookAtVehicleJacker = false;
		m_BlendoutHeadOffsetForCarJackStart = 0;
		m_CarJackStartTime = 0;
		//m_bEnteringTurretSeat = false;					// do not reset here, so we still know we are entering a turret seat after enter vehicle task ends.
		//m_VehicleEntryExitPushAheadOffset = 0.0f;			// do not reset here, we are still blending it out.

		if(!attachedPed->GetIsInVehicle())
		{
			m_VehicleHangingOn = false;
		}
	}

	INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fVehicleSeatBlendDistance, 0.0f, 5.0f, 0.05f);
	INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fMinPitchForHotwiring, -60.0f, 0.0f, 1.0f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fHeloVehicleSeatBlendDistance, 0.60f, 0.0f, 5.0f, 0.05f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fLowVehicleSeatBlendDistance, 0.75f, 0.0f, 5.0f, 0.05f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fBodhiVehicleSeatBlendDistance, 0.75f, 0.0f, 5.0f, 0.05f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fAircraftVehicleSeatShuffleBlendDistance, 0.60f, 0.0f, 5.0f, 0.05f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fBikeEnterBlendDistance, 0.60f, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fBikeExitBlendDistance, 0.50f, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fMaxDistanceBehindSeat, -0.35f, -2.0f, 0.0f, 0.005f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fVehicleSeatBlendVerticalDistance, 1.0f, 0.0f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fVehicleSeatBlendVerticalDistanceForLowVehicles, 2.0f, 0.0f, 20.0f, 0.01f);
	if(m_pVehicle && m_VehicleSeatIndex >= 0 && m_VehicleTargetSeatIndex >= 0)
	{
		const bool IsAPlaneOrHelo = m_pVehicle && m_pVehicle->GetIsAircraft();
		const bool IsALargePlane = IsAPlaneOrHelo && ((CPlane*)m_pVehicle.Get())->IsLargePlane();
		const CVehicleModelInfo* vehicleModelInfo = m_pVehicle->GetVehicleModelInfo();
		m_PovVehicleCameraMetadata = NULL;
		if(vehicleModelInfo)
		{
			if(vehicleModelInfo->GetPovCameraNameHash() != 0)
			{
				m_PovVehicleCameraMetadata = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(vehicleModelInfo->GetPovCameraNameHash());
			}

			const u32 turretCameraHash = camInterface::ComputePovTurretCameraHash(m_pVehicle);

			if( m_pVehicle && attachedPed &&
				(!m_IsSeatShuffling || !m_pVehicle->IsEntryIndexValid(m_VehicleTargetSeatIndex) || camGameplayDirector::IsTurretSeat(m_pVehicle, m_VehicleTargetSeatIndex)) &&
				camInterface::GetGameplayDirector().IsPedInOrEnteringTurretSeat(m_pVehicle, *attachedPed, false) &&
				turretCameraHash != 0 )
			{
				m_PovVehicleCameraMetadata = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(turretCameraHash);
			}
		}

		if( m_PovVehicleCameraMetadata )
		{
			const CModelSeatInfo* pModelSeatInfo = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
			if(pModelSeatInfo)
			{
				m_IsABikeOrJetski = (m_pVehicle->GetIsSmallDoubleSidedAccessVehicle());
				const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleTargetSeatIndex );	// was m_VehicleSeatIndex
				Matrix34 seatMtx;
				m_pVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);
				int boneIndex;
				const bool hasValidBoneIndex = attachedPed->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_ROOT, boneIndex);
				float   fDotSeatForward = 0.0f;
				float   fDotVertical = 0.0f;
				Vector3 vDeltaRight;
				Vector3 vVerticalDelta;
				Vector3 positionPed = VEC3V_TO_VECTOR3(attachedPed->GetMatrix().GetCol3());

				Vector3 vDelta2d = positionPed - seatMtx.d;
				vDelta2d.z = 0.0f;

				if(hasValidBoneIndex)
				{
					Matrix34 worldBoneMatrix;
					attachedPed->GetGlobalMtx(boneIndex, worldBoneMatrix);
					positionPed = worldBoneMatrix.d;
					vDeltaRight = positionPed - seatMtx.d;
					fDotSeatForward = vDeltaRight.Dot(seatMtx.b);
					float fDot = vDeltaRight.Dot(seatMtx.a);
					fDotVertical = vDeltaRight.Dot(seatMtx.c);
					vDeltaRight = seatMtx.a * fDot;				// Projection of vector from seat to ped bone, resolved against seat's right vector.
					vVerticalDelta = seatMtx.c * fDotVertical;
				}
				else
				{
					vDeltaRight = vDelta2d;
					vVerticalDelta = (positionPed - seatMtx.d);
					vVerticalDelta.x = 0.0f;
					vVerticalDelta.y = 0.0f;
				}

				float fDefaultVehicleSeatDistance = fVehicleSeatBlendDistance;
				if(m_IsABikeOrJetski && !m_pVehicle->GetIsJetSki())
				{
					fDefaultVehicleSeatDistance = (m_ExitingVehicle) ? fBikeExitBlendDistance : fBikeEnterBlendDistance;
				}
				else if(IsALargePlane)
				{
					fDefaultVehicleSeatDistance *= 1.5f;
				}
				else if( m_pVehicle->GetVehicleModelInfo() && 
						((m_pVehicle->GetVehicleModelInfo()->GetMinSeatHeight() < 0.900f) ||
						 (m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_ALLOW_OBJECT_LOW_LOD_COLLISION) ) ||
						 m_pVehicle->InheritsFromBoat() || 
						 m_pVehicle->GetIsJetSki() ||
						 IsAPlaneOrHelo) )
				{
					m_EntryHeadingLimitScale = 0.45f;
					fDefaultVehicleSeatDistance = (!IsAPlaneOrHelo) ? fLowVehicleSeatBlendDistance : fHeloVehicleSeatBlendDistance;
					if(m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_ALLOW_OBJECT_LOW_LOD_COLLISION) )
						fDefaultVehicleSeatDistance = fBodhiVehicleSeatBlendDistance;

					if(m_DidSeatShuffle && IsAPlaneOrHelo)
						fDefaultVehicleSeatDistance = fAircraftVehicleSeatShuffleBlendDistance;
				}

				if(m_VehicleSeatBlendDistance < 0.0f)
				{
					m_VehicleSeatBlendDistance = fDefaultVehicleSeatDistance;
				}

				if( m_ExtraVehicleEntryDelayStartTime != 0 )
				{
					const float duration = (float)( (m_bEnteringTurretSeat) ? c_TurretEntryDelay : (m_BlendFromHotwiring) ? c_HotwireBlendDelay : c_ExtraVehicleEntryDelay );
					float fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_ExtraVehicleEntryDelayStartTime) / duration;
					fInterp = Clamp(fInterp, 0.0f, 1.0f);
					fInterp = SlowInOut(fInterp);
					m_VehSeatInterp = Lerp(fInterp, m_VehSeatInterp, 1.0f);
				}
				else if( m_EnteringVehicle || m_ExitingVehicle || m_bEnterBlending )
				{
					const bool isEnteringFormulaCar = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_FORMULA_VEHICLE); 
					const bool isLowVehicle = isEnteringFormulaCar;

					const bool isPedInSeatProximity2d = vDelta2d.Mag2() <= rage::square(fDefaultVehicleSeatDistance);
					const bool isPedInSeatProximity = vDeltaRight.Mag2() <= rage::square(fDefaultVehicleSeatDistance);
					const bool isPedInSeatProximityVertical = vVerticalDelta.Mag2() <= rage::square(isLowVehicle ? fVehicleSeatBlendVerticalDistanceForLowVehicles : fVehicleSeatBlendVerticalDistance);
					const bool isJacking = m_IsCarJacking && m_pJackedPed != nullptr;
					const bool isPickingUpBike = pEnterVehicleTask && pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_PickUpBike;
					const bool isPedAlignedToSeat = fDotSeatForward >= fMaxDistanceBehindSeat && fDotSeatForward <= fDefaultVehicleSeatDistance;

					if( (m_ExitingVehicle || isPedInSeatProximity2d) &&
						!isJacking &&
						!isPickingUpBike &&
						isPedAlignedToSeat &&
						isPedInSeatProximity &&
						isPedInSeatProximityVertical)
					{
						float fDistance = vDeltaRight.Mag();
						if(!m_VehicleDistanceReset && fPreviousVehSeatInterp == 0.0f)
						{
							// Planes get close to seat before triggering blend, if we clamp then we pop.
							m_VehicleSeatBlendDistance = fDistance;
							m_VehicleDistanceReset = true;
						}

						const CVehicleSeatAnimInfo* seatAnimInfo = (m_pVehicle) ? m_pVehicle->GetSeatAnimationInfo(m_VehicleSeatIndex) : NULL;
						////m_VehicleHangingOn = (seatAnimInfo && seatAnimInfo->GetNMConstraintFlags() != 0);
						m_VehicleHangingOn = (seatAnimInfo && seatAnimInfo->GetKeepCollisionOnWhenInVehicle());

						m_VehSeatInterp = (m_VehicleSeatBlendDistance - fDistance) / (m_VehicleSeatBlendDistance);
						if(m_VehicleHangingOn)
						{
							// Remap so 0-0.7 to 0-1.0 as hanging on "seat" doesn't quite reach the seat bone.
							m_VehSeatInterp = RampValue(m_VehSeatInterp, 0.0f, 0.70f, 0.0f, 1.0f);
						}
						else
						{
							m_VehSeatInterp = Clamp(m_VehSeatInterp, 0.0f, 1.0f);
							m_VehSeatInterp = SlowInOut(m_VehSeatInterp);
						}

						if(m_EnteringVehicle || m_bEnterBlending)
						{
							m_bEnterBlending = !m_VehicleHangingOn && !m_bEnterBlendComplete;
							if(m_IsABikeOrJetski || (fPreviousVehSeatInterp > 0.75f && previousSeatIndex == m_VehicleSeatIndex))
							{
								// TO be safe: Past a certain point, no not allow interpolation move to go backwards
								m_VehSeatInterp = Max(m_VehSeatInterp, fPreviousVehSeatInterp);
							}
						}
					}
					else
					{
						// Once we have started blending to pov camera based on seat distance, there is no going back.
						// Fixes entering Titan, which was failing the 2d distance test (getting up to 1.25m away from seat while in plane)
						if( m_pVehicle && m_pVehicle->GetIsAircraft() )
						{
							if( IsAPlaneOrHelo )//&& vDelta.Mag2() <= rage::square(fDefaultVehicleSeatDistance) )
							{
								m_VehSeatInterp = fPreviousVehSeatInterp;
							}
						}
						else
						{
							m_VehSeatInterp = 0.0f;
						}

						if(!m_EnteringVehicle && !m_ExitingVehicle && m_bEnterBlending && m_ExtraVehicleEntryDelayStartTime == 0)
						{
							if(vDeltaRight.Mag2() > rage::square(fDefaultVehicleSeatDistance*1.50f))
							{
								cameraWarningf("During vehicle entry blend, ped is too far away from seat, abort entry behaviour.");
								m_bEnterBlending = false;
								m_bEnterBlendComplete = true;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		if(!m_ExitingVehicle)
		{
			m_pVehicle = NULL;
			m_VehicleSeatIndex = -1;
		}
		m_PovVehicleCameraMetadata = NULL;
		m_VehicleTargetSeatIndex = -1;
		m_VehicleDistanceReset = false;
		m_VehSeatInterp = 0.0f;
		m_bEnterBlending = false;
		m_bEnterBlendComplete = false;
		m_ExtraVehicleEntryDelayStartTime = 0;
		m_BlendoutHeadOffsetForCarJackStart = 0;
		m_VehicleEntryExitPushAheadOffset = 0.0f;

		if(!attachedPed->GetIsInVehicle())
		{
			m_VehicleHangingOn = false;
		}
	}

	if( m_CurrentControlHelper && (m_EnteringVehicle || m_ExitingVehicle || m_bEnterBlending) )
	{
		// B* 2014777: Changing view mode when entering vehicle is weird as we are in first person camera even though in a vehicle.
		// The view mode context is a bit weird as we are getting into vehicle, but using on foot context.  Gameplay director
		// updates following vehicle state and there is code to force first person when entering/exiting vehicle.
		// TODO: Maybe, it is better to check vehicle view mode context when entering/exiting.  Though, I think that will be weirder.
		m_CurrentControlHelper->IgnoreViewModeInputThisUpdate();
	}
}

void camFirstPersonShooterCamera::UpdateControlHelper()
{
	// Setup which control helper we are using.
	if( m_IsAiming && !m_bSteerCameraWithLeftStick && m_AimControlHelper && m_CurrentControlHelper != m_AimControlHelper )
	{
		if(m_CurrentControlHelper)
			m_AimControlHelper->CloneSpeeds( *m_CurrentControlHelper );
		m_CurrentControlHelper = m_AimControlHelper;
	}
	else if( !m_IsAiming && m_ControlHelper && m_CurrentControlHelper != m_ControlHelper )
	{
		if(m_CurrentControlHelper)
			m_ControlHelper->CloneSpeeds( *m_CurrentControlHelper );
		m_CurrentControlHelper = m_ControlHelper;
	}

	// Handle spectating.
	BANK_ONLY( TUNE_GROUP_BOOL(CAM_FPS, bFakeSpectatorMode, false); )
	if( m_CurrentControlHelper && (NetworkInterface::IsInSpectatorMode()  BANK_ONLY( || bFakeSpectatorMode)) )
	{
		m_CurrentControlHelper->IgnoreLookBehindInputThisUpdate();
		m_CurrentControlHelper->IgnoreLookAroundInputThisUpdate();
		m_CurrentControlHelper->IgnoreAccurateModeInputThisUpdate();
	}

	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const bool isActiveGameplayCamera		= gameplayDirector.IsActiveCamera(this);
	if(isActiveGameplayCamera)
	{
		//NOTE: Only the active gameplay camera is allowed to update the view mode.
		m_CurrentControlHelper->SetShouldUpdateViewModeThisUpdate();

		const bool isInterpolating = IsInterpolating();
		if(isInterpolating)
		{
			//Inhibit view-mode changes to prevent bouncy interpolation behaviour.
			m_CurrentControlHelper->IgnoreViewModeInputThisUpdate();
		}
	}

	if( gameplayDirector.GetSwitchHelper() || (m_HintHelper && (gameplayDirector.GetHintBlend() >= SMALL_FLOAT)) )
	{
		//Inhibit look-behind during Switch behaviour and whilst a hint is influencing the orientation.
		m_CurrentControlHelper->SetLookBehindState(false);
		m_CurrentControlHelper->IgnoreLookBehindInputThisUpdate();
	}

	const bool isRenderingCinematicCamera = camInterface::GetCinematicDirector().IsRendering();
	const bool isRenderingFirstPersonVehicleCamera = camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera();
	if( isRenderingCinematicCamera && (!m_bUseAnimatedFirstPersonCamera || !isRenderingFirstPersonVehicleCamera) )
	{
		//Ignore look-around input whenever a cinematic camera is rendering to avoid confusing the user.
		m_CurrentControlHelper->IgnoreLookAroundInputThisUpdate();
	}

	const bool wasRenderingThisCameraOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldSkipViewModeBlend					= (!wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	if(shouldSkipViewModeBlend)
	{
		m_CurrentControlHelper->SkipViewModeBlendThisUpdate();
	}

	//NOTE: m_AttachParent has already been validated.
	m_CurrentControlHelper->Update(*m_AttachParent);
}

void camFirstPersonShooterCamera::UpdateStrafeState()
{
	m_AllowStrafe = true;
	
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());

	TUNE_GROUP_BOOL(CAM_FPS, bEnableSprintBreakout, true);
	if(attachPed && 
		bEnableSprintBreakout &&
		(!attachPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride) || attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping)) &&
		attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT) && 
		!attachPed->GetPedResetFlag(CPED_RESET_FLAG_DisableIdleExtraHeadingChange))
	{
		if(attachPed->GetPedResetFlag(CPED_RESET_FLAG_DoFPSSprintBreakOut) || attachPed->GetPedResetFlag(CPED_RESET_FLAG_DoFPSJumpBreakOut))
		{
			ioValue::ReadOptions options	= ioValue::NO_DEAD_ZONE;
			CControl& rControl = CControlMgr::GetMainPlayerControl(true);
			Vector2 vLeftStick(rControl.GetPedWalkLeftRight().GetNorm(options), -rControl.GetPedWalkUpDown().GetNorm(options));

			if( ioValue::RequiresDeadZone(rControl.GetPedWalkLeftRight().GetSource()) )
			{
				// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
				float deadzonedInput = -rControl.GetPedWalkLeftRight().GetNorm();
				if(deadzonedInput == 0.0f)
					vLeftStick.x = 0.0f;
			}

			if( ioValue::RequiresDeadZone(rControl.GetPedWalkUpDown().GetSource()) )
			{
				// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
				float deadzonedInput = -rControl.GetPedWalkUpDown().GetUnboundNorm();
				if(deadzonedInput == 0.0f)
					vLeftStick.y = 0.0f;
			}

			float inputMag = rage::ioAddRoundDeadZone(vLeftStick.x, vLeftStick.y, ioValue::DEFAULT_DEAD_ZONE_VALUE);
			if(inputMag > rage::square(0.01f))
			{
				// We weren't running the loco task before, but now we are - sprint break out
				// Calculate heading for sprint break out

				const camFirstPersonShooterCameraMetadataSprintBreakOutSettings& sprintBreakOutSettings = m_Metadata.m_SprintBreakOutSettingsList[m_SprintBreakOutType];
				float fDesiredHeading = Atan2f(-vLeftStick.x, vLeftStick.y);
				if(Abs(fDesiredHeading) >= sprintBreakOutSettings.m_SprintBreakOutHeadingAngleMin*DtoR)
				{
					m_SprintBreakOutType = attachPed->GetPedResetFlag(CPED_RESET_FLAG_DoFPSJumpBreakOut) ? SBOST_JUMPING : SBOST_SPRINTING;
					if(Verifyf(m_SprintBreakOutType < m_Metadata.m_SprintBreakOutSettingsList.GetCount(), "m_SprintBreakOutType %d has no entry in metadata", m_SprintBreakOutType))
					{
						m_SprintBreakOutTriggered = true;
						m_SprintBreakOutFinished = false;

						fDesiredHeading += attachPed->GetCurrentHeading();
						fDesiredHeading  = fwAngle::LimitRadianAngle(fDesiredHeading);
						m_SprintBreakOutHeading = fDesiredHeading;
						m_SprintBreakOutHoldAtEndTime = 0.f;
						m_SprintBreakOutTotalTime = 0.f;
					}
				}
			}
		}
	}
	else
	{
		m_SprintBreakOutTriggered = false;
		m_SprintBreakOutFinished = false;
	}

	// When using left stick to steer while sprinting, right stick input drops out of sprint back to strafing
	CControl& rControl = CControlMgr::GetMainPlayerControl(true);
	Vector2 vRightStick(rControl.GetPedLookLeftRight().GetNorm(), -rControl.GetPedLookUpDown().GetNorm());
	const bool bTouchedRightStick = vRightStick.Mag2() >= (0.10f*0.10f);

	if(m_SprintBreakOutTriggered)
	{
		if(bTouchedRightStick)
		{
			m_SprintBreakOutTriggered = false;
			m_SprintBreakOutFinished = true;
		}
		else
		{
			const camFirstPersonShooterCameraMetadataSprintBreakOutSettings& sprintBreakOutSettings = m_Metadata.m_SprintBreakOutSettingsList[m_SprintBreakOutType];
			if(Abs(SubtractAngleShorter(m_CameraHeading, m_SprintBreakOutHeading)) <= sprintBreakOutSettings.m_SprintBreakOutHeadingFinishAngle)
			{
				m_SprintBreakOutHoldAtEndTime += fwTimer::GetCamTimeStep();
				if(m_SprintBreakOutHoldAtEndTime >= sprintBreakOutSettings.m_SprintBreakOutHoldAtEndTime)
				{
					m_SprintBreakOutTriggered = false;
					m_SprintBreakOutFinished = true;
				}
			}
			else
			{
				m_SprintBreakOutHoldAtEndTime = 0.f;
			}

			m_SprintBreakOutTotalTime += fwTimer::GetCamTimeStep();
			if(m_SprintBreakOutTotalTime >= sprintBreakOutSettings.m_SprintBreakMaxTime)
			{
				// Quit if out of time
				m_SprintBreakOutTriggered = false;
				m_SprintBreakOutFinished = true;
			}
		}
	}

	if( camInterface::GetGameplayDirector().IsFirstPersonShooterLeftStickControl() && attachPed && attachPed->GetMotionData() && !m_IsAiming )
	{
		// B*2754665 - Dont apply sprinting corrections while parachuting in FP
		bool bParachuting = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting);
		bool bSprinting = !bParachuting && attachPed->GetMotionData()->GetIsDesiredSprinting(attachPed->GetMotionData()->GetFPSCheckRunInsteadOfSprint());

		bool bUsingKeyboardMouse = false;
		bool bMouseInputThisUpdate = false;
	#if KEYBOARD_MOUSE_SUPPORT
		// This could actually be the steam controller as well that acts like a mouse.
		bUsingKeyboardMouse = rControl.UseRelativeLook();
		if(bUsingKeyboardMouse)
		{
			// Could look directly at ioMouse::GetDX(), but this way don't have to worry about remapping.
		////#if RSG_EXTRA_MOUSE_SUPPORT
		////	const ioValue::ValueType c_MouseInputThreshold = 3.0f/127.0f; // see ioValue::LEGACY_AXIS_SHIFT and ioValue::LEGACY_AXIS_SCALE
		////#else
		////	const ioValue::ValueType c_MouseInputThreshold = (ioValue::ValueType)3;
		////#endif
		////	bMouseInputThisUpdate =	(rControl.GetPedLookLeftRight().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && 
		////								(rControl.GetPedLookLeftRight().GetValue() >  c_MouseInputThreshold || 
		////								 rControl.GetPedLookLeftRight().GetValue() < -c_MouseInputThreshold)) ||
		////							(rControl.GetPedLookUpDown().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && 
		////								(rControl.GetPedLookUpDown().GetValue() >  c_MouseInputThreshold ||
		////								 rControl.GetPedLookUpDown().GetValue() < -c_MouseInputThreshold));
			bMouseInputThisUpdate =	(rControl.IsRelativeLookSource(rControl.GetPedLookLeftRight().GetSource()) && 
										rControl.GetPedLookLeftRight().GetValue() != (ioValue::ValueType)0) ||
									(rControl.IsRelativeLookSource(rControl.GetPedLookUpDown().GetSource()) && 
										rControl.GetPedLookUpDown().GetValue() != (ioValue::ValueType)0);
			if(bMouseInputThisUpdate)
			{
				m_LastMouseInputTime = fwTimer::GetTimeInMilliseconds();
			}
			else
			{
				const u32 c_MinTimeToAllowLeftStickInput = 100;
				bMouseInputThisUpdate = (m_LastMouseInputTime + c_MinTimeToAllowLeftStickInput > fwTimer::GetTimeInMilliseconds());
			}
		}
	#endif	//KEYBOARD_MOUSE_SUPPORT

		bool bAllowSprinting = !camInterface::GetGameplayDirector().IsFirstPersonShooterLeftStickControl() || !bTouchedRightStick;
		bool bUseRightStick = false;

		if(WhenSprintingUseRightStick(attachPed, rControl))
		{
			bAllowSprinting = true;
			bUseRightStick = true;
		}

		if(bSprinting && (bAllowSprinting || bUsingKeyboardMouse) && !m_bLookingBehind)
		{
			bool bFPSSwimmingWithTPSControls = false;
			if(attachPed->GetIsFPSSwimming() && attachPed->GetControlFromPlayer())
			{
				eControlLayout controlLayout = attachPed->GetControlFromPlayer()->GetActiveLayout();
				bool bValidControlLayout = (controlLayout == STANDARD_TPS_LAYOUT || controlLayout == TRIGGER_SWAP_TPS_LAYOUT || controlLayout == SOUTHPAW_TPS_LAYOUT || controlLayout == SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT);
				bFPSSwimmingWithTPSControls = bValidControlLayout;
			}

			if(bUseRightStick && !bFPSSwimmingWithTPSControls)
			{
				m_bSteerCameraWithLeftStick = false;
			}
			else if(!bMouseInputThisUpdate && (m_SprintBreakOutFinished ||  !m_SprintBreakOutTriggered || bFPSSwimmingWithTPSControls || !bEnableSprintBreakout))
			{
				m_bSteerCameraWithLeftStick = true;
				m_CurrentControlHelper->SetUseLeftStickInputThisUpdate();
			}
			else
			{
				m_bSteerCameraWithLeftStick = false;
			}
	
			// If in first person shooter camera and strafing is desired, force aiming if weapon is equipped.
			m_AllowStrafe = false;
		}
		else
		{
			m_bSteerCameraWithLeftStick = false;
		}
	}
	else
	{
		m_bSteerCameraWithLeftStick = false;
	}
}

void camFirstPersonShooterCamera::UpdateAnimatedCameraData(const CPed* attachPed)
{
	m_bUseAnimatedFirstPersonCamera = false;
	m_UpdateAnimCamDuringBlendout = false;
	m_AnimatedCameraBlockTag = false;
	if(!m_bWasUsingAnimatedFirstPersonCamera)
	{
		// Reset if animated camera was not running on previous update.
		m_AnimatedRelativeAttachOffset.ZeroComponents();
		m_AnimatedOrientation = QuatV(V_IDENTITY);
	}
	TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, bEnableAnimatedLimits, true);
	TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fHeadingLimitScale, 1.0f, 0.10f, 10.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fDefaultPitchLimitScale,   1.0f, 0.10f, 10.0f, 0.10f);

	float fPitchLimitScale = attachPed->GetPedResetFlag(CPED_RESET_FLAG_FPSPlantingBombOnFloor) ? 0.25f :  fDefaultPitchLimitScale;

	BANK_ONLY( TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, bDebugForceAnimatedCameraWeightToOne, false); )
	TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, bAnimatedCameraStartBlendOutWhenWeightLessThanOne, true);
	if( attachPed->GetSkeleton() != NULL && camInterface::GetGameplayDirector().IsFirstPersonAnimatedDataEnabled() )
	{
		// Init in case data is missing.  GetFirstPersonCameraDOFs initialized the offset and orientation, so no need to do that here.
		Vec3V vSavedOffset = m_AnimatedRelativeAttachOffset;
		QuatV qSavedOrientation = m_AnimatedOrientation;
		float fPreviousAnimatedWeight = m_AnimatedWeight;

		float fMinX = 0.0f;
		float fMinY = 0.0f;
		float fTmpMinZ, fTmpMaxZ;
		float fMaxX = 0.0f;
		float fMaxY = 0.0f;
		m_AnimatedWeight = 0.0f;
		m_AnimatedFovOnPreviousFrame = m_AnimatedFov;
		m_AnimatedFov = 0.0f;
		m_bUseAnimatedFirstPersonCamera = attachPed->GetFirstPersonCameraDOFs(m_AnimatedRelativeAttachOffset, m_AnimatedOrientation, fMinX, fMaxX, fMinY, fMaxY, fTmpMinZ, fTmpMaxZ, m_AnimatedWeight, m_AnimatedFov);

#if __BANK
		{
			Mat34V mCam;
			Mat34VFromQuatV(mCam, m_AnimatedOrientation);
			const Matrix34 camMatrix = MAT34V_TO_MATRIX34(mCam);

			float heading, pitch, roll;
			camFrame::ComputeHeadingPitchAndRollFromMatrix(camMatrix, heading, pitch, roll);

			PF_SET(AnimYaw, heading*RtoD);
			PF_SET(AnimPitch, pitch*RtoD);
			PF_SET(AnimRoll, roll*RtoD);
			PF_SET(AnimPosX, m_AnimatedRelativeAttachOffset.GetX().Getf());
			PF_SET(AnimPosY, m_AnimatedRelativeAttachOffset.GetY().Getf());
			PF_SET(AnimPosZ, m_AnimatedRelativeAttachOffset.GetZ().Getf());
			PF_SET(AnimLimitXMin, fMinX);
			PF_SET(AnimLimitXMax, fMaxX);
			PF_SET(AnimLimitYMin, fMinY);
			PF_SET(AnimLimitYMax, fMaxY);
			PF_SET(AnimFov, m_AnimatedFov);
			PF_SET(AnimWeight, m_AnimatedWeight);
		}
#endif 

		// HACK: for combat roll (and table games), force animated weight to one
		float fTrueAnimatedWeight = m_AnimatedWeight;
		if(m_bUseAnimatedFirstPersonCamera && (m_CombatRolling || IsTableGamesOrIsInterpolatingToTableGamesCamera()) && m_AnimatedWeight < 1.0f && m_bAnimatedCameraBlendInComplete)
		{
			m_AnimatedWeight = 1.0f;
		}

		// HACK: When exiting vehicle, put on helmet animated camera is interfering with exit behaviour, so terminate it.
		if( m_ExitingVehicle && attachPed->GetPedIntelligence() )
		{
			const CTaskMotionInAutomobile* pMotionAutomobileTask = (const CTaskMotionInAutomobile*)attachPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE);
			if(pMotionAutomobileTask && pMotionAutomobileTask->GetState() == CTaskMotionInAutomobile::State_PutOnHelmet)
			{
				m_bUseAnimatedFirstPersonCamera = false;
				m_bWasUsingAnimatedFirstPersonCamera = false;
			}
		}

		//In table games (for safety) we want to allow hints to run even if an animated camera is running, so we block the animated camera if a hint is active and not blending out
		const camEnvelope* tableGamesHintEnvelope = GetTableGamesHintEnvelope();
		if (tableGamesHintEnvelope && !tableGamesHintEnvelope->IsInReleasePhase())
		{
			m_bUseAnimatedFirstPersonCamera = false;
		}

		if(bEnableAnimatedLimits)
		{
			fMinX *= fPitchLimitScale;
			fMaxX *= fPitchLimitScale;
			fMinY *= fHeadingLimitScale;
			fMaxY *= fHeadingLimitScale;
			if(fHeadingLimitScale > 1.0f)
			{
				fMinY = Max(fMinY, m_Metadata.m_MinRelativeHeading);
				fMaxY = Min(fMaxY, m_Metadata.m_MaxRelativeHeading);
			}
		}
		else //if(!bEnableAnimatedLimits)
		{
			fMinY = m_Metadata.m_MinRelativeHeading;
			fMaxY = m_Metadata.m_MaxRelativeHeading;
			fMinX = m_MinPitch*RtoD;
			fMaxX = m_MaxPitch*RtoD;
		}

		BANK_ONLY( if(bDebugForceAnimatedCameraWeightToOne && m_bUseAnimatedFirstPersonCamera) { m_AnimatedWeight = 1.0f; } )

		// Tags can disable animated camera data, so store the data if we have tags and assign them later.
		bool bFoundAllowedBlockedTag = false;
		bool bFoundInputOptionsTag = false;
		bool TagSpringInputYaw = true;
		bool TagSpringInputPitch = true;
		s32 TagBlendInDuration = -1;
		s32 TagBlendOutDuration = -1;
		fwAnimDirector* pAnimDirector = attachPed->GetAnimDirector();
		if(pAnimDirector)
		{
			const CClipEventTags::CFirstPersonCameraEventTag* pPropCamera =
				static_cast<const CClipEventTags::CFirstPersonCameraEventTag*>(pAnimDirector->FindPropertyInMoveNetworks(CClipEventTags::FirstPersonCamera));
			if(pPropCamera)
			{
				m_bUseAnimatedFirstPersonCamera = pPropCamera->GetAllowed();
				if(m_bUseAnimatedFirstPersonCamera)
				{
					bFoundAllowedBlockedTag = true;
					m_AnimatedCameraStayBlocked = false;
					float duration = pPropCamera->GetBlendInDuration();
					TagBlendInDuration  = (s32)(duration * 1000.0f);
					m_AnimBlendOutDuration = -1;
				}
				else
				{
					bFoundAllowedBlockedTag = true;
					m_AnimatedCameraBlockTag = true;
					m_AnimatedCameraStayBlocked = true;
					float duration = pPropCamera->GetBlendOutDuration();
					m_AnimBlendInDuration  = -1;
					TagBlendOutDuration = (s32)(duration * 1000.0f);
				}
			}

			const CClipEventTags::CFirstPersonCameraInputEventTag* pPropInput =
				static_cast<const CClipEventTags::CFirstPersonCameraInputEventTag*>(pAnimDirector->FindPropertyInMoveNetworks(CClipEventTags::FirstPersonCameraInput));
			if(pPropInput)
			{
				bFoundInputOptionsTag = true;
				TagSpringInputYaw   = pPropInput->GetYawSpringInput();
				TagSpringInputPitch = pPropInput->GetPitchSpringInput();
			}
		}

		// Protection against anim with block tag blending out.  Block tag no longer found and the blend out
		// is treated as a blend in when it shouldn't be.  So, if we were blocked and no longer find a tag
		// then if we get data and it is NOT blending in, (i.e. weight value is decreasing) ignore it.
		if(m_AnimatedCameraStayBlocked && !bFoundAllowedBlockedTag && fPreviousAnimatedWeight <= m_AnimatedWeight)
		{
			m_AnimatedCameraStayBlocked = false;
		}

		if(bAnimatedCameraStartBlendOutWhenWeightLessThanOne && (m_AnimatedCameraStayBlocked || m_bAnimatedCameraBlendInComplete))
		{
			m_bUseAnimatedFirstPersonCamera &= (m_AnimatedWeight > (1.0f - SMALL_FLOAT));
		}
		else
		{
			m_bUseAnimatedFirstPersonCamera &= (m_AnimatedWeight > SMALL_FLOAT);
		}

		if(m_AnimBlendOutStartTime != 0 && m_bWasUsingAnimatedFirstPersonCamera && m_bUseAnimatedFirstPersonCamera)
		{
			m_bWasUsingAnimatedFirstPersonCamera = false;
			m_AnimBlendOutStartTime = 0;
		}

		if(!m_bWasUsingAnimatedFirstPersonCamera && m_bUseAnimatedFirstPersonCamera)
		{
			// Set defaults in case tags are not set.
			m_AnimBlendInDuration = -1;
			m_AnimBlendOutDuration = -1;
			m_AnimatedYawSpringInput = true;
			m_AnimatedPitchSpringInput = true;
			m_ActualAnimatedYawSpringInputChanged = false;
			m_ActualAnimatedYawSpringInput = m_AnimatedYawSpringInput;
			m_ActualAnimatedPitchSpringInput = m_AnimatedPitchSpringInput;
			m_PreviousAnimatedHeading = FLT_MAX;
			m_PreviousAnimatedPitch = FLT_MAX;
		}

		if(bFoundAllowedBlockedTag)
		{
			m_AnimBlendInDuration  = TagBlendInDuration;
			m_AnimBlendOutDuration = TagBlendOutDuration;
			if( fTrueAnimatedWeight >  (1.0f - SMALL_FLOAT) &&
				TagBlendOutDuration > 0 &&
				TagBlendInDuration  < 0 )
			{
				// If we are blending out due to block tag and weight is still 1.0, keep updating camera frame during blendout.
				m_UpdateAnimCamDuringBlendout = true;
			}
		}

		// If we are blending out, stop updating the animated data, so we freeze the last good position for the blend.
		// However, if the animated weight is 1.0, keep updating the animated camera frame.
		if(m_bWasUsingAnimatedFirstPersonCamera && !m_bUseAnimatedFirstPersonCamera && !m_UpdateAnimCamDuringBlendout)
		{
			// Restore last read camera data so we can re-create the last frame for interpolation.
			m_AnimatedRelativeAttachOffset = vSavedOffset;
			m_AnimatedOrientation = qSavedOrientation;
			m_AnimatedFov = m_AnimatedFovOnPreviousFrame;
		}

		//update the fov blend here because this is just after the last update on the m_AnimatedFov.
		UpdateBlendedAnimatedFov();

		if(bFoundInputOptionsTag)
		{
			m_AnimatedYawSpringInput   = TagSpringInputYaw;
			m_AnimatedPitchSpringInput = TagSpringInputPitch;
			m_ActualAnimatedYawSpringInput = m_AnimatedYawSpringInput;
			m_ActualAnimatedPitchSpringInput = m_AnimatedPitchSpringInput;
		}

#if RSG_PC
		// Force spring input off, so moving camera using mouse does not spring back to center, should still be limited by camera limits.
		CControl& rControl = CControlMgr::GetMainPlayerControl(false);
		if(rControl.UseRelativeLook())
		{
			m_AnimatedYawSpringInput   = false;
			m_AnimatedPitchSpringInput = false;
		}
		else
		{
			m_AnimatedYawSpringInput = m_ActualAnimatedYawSpringInput;
			m_AnimatedPitchSpringInput = m_ActualAnimatedPitchSpringInput;
		}
#endif // RSG_PC

		//disable the input spring back to center when in table games (except slot machines!)
		static const u32 slotMachineCameraName	= ATSTRINGHASH("CASINO_SLOT_MACHINE_CAMERA", 0x1EE8CB4C);
		const u32 tableGamesCameraName			= camInterface::GetGameplayDirector().GetTableGamesCameraName();
		const bool isSlotMachineCamera			= (tableGamesCameraName == slotMachineCameraName);
		if (IsTableGamesCamera() && !isSlotMachineCamera)
		{
			m_AnimatedYawSpringInput			= false;
			m_AnimatedPitchSpringInput			= false;
			m_ActualAnimatedYawSpringInput		= false;
			m_ActualAnimatedPitchSpringInput	= false;
		}

		//special table games blended fov
		float animatedFovToConsider	= m_AnimatedFov;
		if (IsTableGamesCamera())
		{
			animatedFovToConsider	= m_BlendedAnimatedFov;
		}

		m_ActualAnimatedYawSpringInputChanged = (m_ActualAnimatedYawSpringInput != m_AnimatedYawSpringInput);

		if( m_bUseAnimatedFirstPersonCamera || (m_bWasUsingAnimatedFirstPersonCamera && m_UpdateAnimCamDuringBlendout) )
		{
			// Clamp pitch limits to the camera limits.
			fMinX = Max(fMinX, m_MinPitch*RtoD);
			fMaxX = Min(fMaxX, m_MaxPitch*RtoD);

			// Reduce pitch limits by half (vertical) fov so camera does not look outside of limits.
			float fCurrentFov = (animatedFovToConsider != 0.0f) ? animatedFovToConsider : m_Frame.GetFov();
			fMinX = Min(fMinX + fCurrentFov*0.50f, 0.0f);
			fMaxX = Max(fMaxX - fCurrentFov*0.50f, 0.0f);

			const CViewport* viewport	= gVpMan.GetGameViewport();
			const float aspectRatio		= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
			const float horizontalFov	= fCurrentFov * aspectRatio;

			// Reduce yaw limits by half (horizontal) fov so camera does not look outside of limits.
			TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, c_bRestrictCameraFrustumToLimits, false);
			if(c_bRestrictCameraFrustumToLimits)
			{
				fMinY = Min(fMinY + horizontalFov*0.50f, 0.0f);
				fMaxY = Max(fMaxY - horizontalFov*0.50f, 0.0f);
			}

			// Override input limits if animated data is valid.
			// Y is yaw limits, but they want negative to be left, so have to reverse and negate them.  X is pitch limits.
			m_DesiredAnimatedCamSpringHeadingLimitsRadians.Set(-fMaxY, -fMinY);
			m_DesiredAnimatedCamSpringPitchLimitsRadians.Set(fMinX, fMaxX);
		}
		else if(!m_bWasUsingAnimatedFirstPersonCamera)
		{
			m_AnimatedCameraHeadingAccumulator = 0.0f;		// Reset
			m_AnimatedCameraPitchAccumulator   = 0.0f;
			m_ActualAnimatedYawSpringInput     = true;
			m_ActualAnimatedPitchSpringInput   = true;
		}

		UpdateAnimatedCameraLimits(attachPed);
	}
}

void camFirstPersonShooterCamera::UpdateAnimatedCameraLimits(const CPed* attachPed)
{
	if(attachPed == NULL)
		return;

	// Not worried about position, but need the final animated camera orientation so we can generate the camera limits.
	Mat34V mPed, mCam;
	if( !camInterface::GetGameplayDirector().IsFirstPersonAnimatedDataRelativeToMover() )
	{
		int rootIdx = 0;
		attachPed->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_ROOT, rootIdx);
		crSkeleton* pSkeleton = attachPed->GetSkeleton();
		pSkeleton->GetGlobalMtx(rootIdx, mPed);
	}
	else
	{
		mPed = attachPed->GetTransform().GetMatrix();
	}

	Mat34VFromQuatV(mCam, m_AnimatedOrientation);
	Transform(mCam, mPed, mCam);
	////Matrix34 animatedCameraMatrix = MAT34V_TO_MATRIX34( mCam );
	const CTaskClimbLadder* pClimbLadderTask  = (const CTaskClimbLadder*)attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER);
	const bool bUsingLadder = (pClimbLadderTask != NULL);

	if( m_bUseAnimatedFirstPersonCamera || (m_bWasUsingAnimatedFirstPersonCamera && m_UpdateAnimCamDuringBlendout))
	{
		m_AnimatedCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
		m_AnimatedCamSpringPitchLimitsRadians.Set(m_MinPitch*RtoD, m_MaxPitch*RtoD);

		// Make initial pitch limits relative to animated camera pitch, since that is how they are applied in UseAnimatedCameraData.
		float pitch = camFrame::ComputePitchFromMatrix(RCC_MATRIX34(mCam));
		m_AnimatedCamSpringPitchLimitsRadians.x = m_AnimatedCamSpringPitchLimitsRadians.x - pitch*RtoD;
		m_AnimatedCamSpringPitchLimitsRadians.y = m_AnimatedCamSpringPitchLimitsRadians.y - pitch*RtoD;

		float fInterpLevel = (!bUsingLadder && !m_SlopeScrambling /*&& !m_PlayerGettingUp*/) ? m_AnimatedCameraBlendLevel : 1.0f;
		m_AnimatedCamSpringHeadingLimitsRadians.Lerp(fInterpLevel, m_AnimatedCamSpringHeadingLimitsRadians, m_DesiredAnimatedCamSpringHeadingLimitsRadians);
		m_AnimatedCamSpringPitchLimitsRadians.Lerp(  fInterpLevel, m_AnimatedCamSpringPitchLimitsRadians,   m_DesiredAnimatedCamSpringPitchLimitsRadians);

		if(m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera)
		{
			m_CurrentCamSpringHeadingLimitsRadians = m_AnimatedCamSpringHeadingLimitsRadians;
			m_CurrentCamSpringPitchLimitsRadians   = m_AnimatedCamSpringPitchLimitsRadians;
		}
	}
	else if(m_bWasUsingAnimatedFirstPersonCamera)
	{
		m_AnimatedCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
		m_AnimatedCamSpringPitchLimitsRadians.Set(m_MinPitch*RtoD, m_MaxPitch*RtoD);

		// Make final pitch limits relative to camera heading, since that is how they are applied in UseAnimatedCameraData.
		float pitch = camFrame::ComputePitchFromMatrix(RCC_MATRIX34(mCam));
		m_AnimatedCamSpringPitchLimitsRadians.x = m_AnimatedCamSpringPitchLimitsRadians.x - pitch*RtoD;
		m_AnimatedCamSpringPitchLimitsRadians.y = m_AnimatedCamSpringPitchLimitsRadians.y - pitch*RtoD;

		float fInterpLevel = (!bUsingLadder /*&& !m_PlayerGettingUp*/) ? m_AnimatedCameraBlendLevel : 1.0f;
		m_AnimatedCamSpringHeadingLimitsRadians.Lerp(fInterpLevel, m_AnimatedCamSpringHeadingLimitsRadians, m_DesiredAnimatedCamSpringHeadingLimitsRadians);
		m_AnimatedCamSpringPitchLimitsRadians.Lerp(  fInterpLevel, m_AnimatedCamSpringPitchLimitsRadians,   m_DesiredAnimatedCamSpringPitchLimitsRadians);
	}
}

void camFirstPersonShooterCamera::UpdateConstrainToFallBackPosition(const CPed& attachPed)
{
	bool forceConstrainToFallbackPosition = false;

#if RSG_PC
	const bool isAttachPedInCover		= attachPed.GetPedIntelligence() && attachPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER);
	forceConstrainToFallbackPosition	|= camInterface::IsTripleHeadDisplay() && isAttachPedInCover;
#endif	
	
	if(m_Metadata.m_ShouldConstrainToFallBackPosition && (!attachPed.GetPedResetFlag(CPED_RESET_FLAG_DisableCameraConstraintFallBackThisFrame) || forceConstrainToFallbackPosition))
	{
		//NOTE: m_AttachParent has already been validated.
		Matrix34 targetMatrix = MAT34V_TO_MATRIX34(attachPed.GetMatrix());

		// Ensure that our desired camera position is safe by sweeping a sphere from a fall back position and constraining the position when intersecting.
		Vector3 defaultRelativeFallbackPosition(m_Metadata.m_RelativeCollisionFallBackPosition);
		//Fall back to the centre of the ped's head when the mover/capsule position is not valid, such as when ragdolling.
		//TODO: Blend between the alternate fall back positions (probably via caching the offset relative to the head bone matrix, which should always be valid.)
		const bool shouldApplyRagdollBehaviour = camFollowPedCamera::ComputeShouldApplyRagdollBehavour(attachPed, true);

		if(!shouldApplyRagdollBehaviour && !m_EnteringVehicle && !m_ExitingVehicle && !m_bEnterBlending && !attachPed.GetIsFPSSwimming() && !attachPed.GetPedIntelligence()->IsPedClimbing())
		{
			m_RelativeFallbackPosition = defaultRelativeFallbackPosition;
		}

		Vector3 fallBackPosition;
		targetMatrix.Transform(m_RelativeFallbackPosition, fallBackPosition);

		WorldProbe::CShapeTestCapsuleDesc capsuleTest;
		WorldProbe::CShapeTestFixedResults<1> capsuleTestResults;
		capsuleTest.SetResultsStructure(&capsuleTestResults);
		capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		capsuleTest.SetContext(WorldProbe::LOS_Camera);
		capsuleTest.SetIsDirected(true);

		const u32 collisionIncludeFlags = camCollision::GetCollisionIncludeFlagsForClippingTests();
		capsuleTest.SetIncludeFlags(collisionIncludeFlags);

		const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();
		if(followVehicle)
		{
			//Ignore the follow vehicle in addition to the attach parent.
			const CEntity* entitiesToIgnore[2];

			//NOTE: m_AttachParent has already been validated.
			entitiesToIgnore[0] = m_AttachParent;
			entitiesToIgnore[1] = followVehicle;

			capsuleTest.SetExcludeEntities(entitiesToIgnore, 2, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}
		else
		{
			//NOTE: m_AttachParent has already been validated.
			capsuleTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}

		float maxSafeCollisionTestRadius = m_Metadata.m_MaxCollisionFallBackTestRadius;
		float radius	= attachPed.GetCurrentMainMoverCapsuleRadius();
		float safeRadiusReductionRatio = m_Metadata.m_MinSafeRadiusReductionWithinPedMoverCapsule / radius;
		radius			-= radius * safeRadiusReductionRatio;
		if(radius >= SMALL_FLOAT)
		{
			maxSafeCollisionTestRadius = Min(radius, maxSafeCollisionTestRadius);
		}

		// If we are ragdolling or just starting to get up while our head was very close to the ground then
		// use a capsule probe radius that corresponds to the ped's head bound size
		// While ragdolled the head bound determines how close the camera can get to other collision bounds
		// This means the camera will be able to safely get within the head bound's size of other collision
		// Until we've blended sufficiently back to an animated state (ideally to within our ped capsule) we need to keep using this reduced size 
		INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS, fGroundAvoidBlendOutAngle, 0.0f, PI, 0.05f);
		if(attachPed.GetUsingRagdoll() || m_TaskFallLandingRoll || (m_PlayerGettingUp && Abs(m_GroundAvoidCurrentAngle) > fGroundAvoidBlendOutAngle))
		{
			fragInstNMGta* pRagdollInst = attachPed.GetRagdollInst();
			if(pRagdollInst != NULL && pRagdollInst->GetCached())
			{
				phBoundComposite* pCompositeBounds = pRagdollInst->GetCacheEntry()->GetBound();
				s32 iHeadComponent = pRagdollInst->GetComponentFromBoneIndex(attachPed.GetBoneIndexFromBoneTag(BONETAG_HEAD));
				if(pCompositeBounds != NULL && iHeadComponent != -1)
				{
					Vec3V halfWidth;
					Vec3V center;
					pCompositeBounds->GetBound(iHeadComponent)->GetBoundingBoxHalfWidthAndCenter(halfWidth, center);
					float radius = Min(halfWidth.GetXf(), halfWidth.GetYf(), halfWidth.GetZf());
					radius -= radius * safeRadiusReductionRatio;
					maxSafeCollisionTestRadius = Min(radius, maxSafeCollisionTestRadius);
				}
			}
		}

		capsuleTest.SetCapsule(fallBackPosition, m_Frame.GetPosition(), maxSafeCollisionTestRadius);

		m_bShouldGoIntoFallbackMode		= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		const float desiredBlendLevel	= m_bShouldGoIntoFallbackMode ? (1.0f - capsuleTestResults[0].GetHitTValue()) : 0.0f;

		//NOTE: We must cut to a greater blend level in order to avoid clipping into collision.
		const float blendLevelOnPreviousUpdate	= m_CollisionFallBackBlendSpring.GetResult();
		const bool shouldCutSpring				= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
			m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || (desiredBlendLevel > blendLevelOnPreviousUpdate));
		const float springConstant				= shouldCutSpring ? 0.0f : m_Metadata.m_CollisionFallBackBlendSpringConstant;
		float blendLevelToApply					= m_CollisionFallBackBlendSpring.Update(desiredBlendLevel, springConstant, m_Metadata.m_CollisionFallBackBlendSpringDampingRatio, true);

		// Do not use back up position if the camera is animated and player is playing a script driven animation
		if(m_RunningAnimTask)
		{
			blendLevelToApply *= Clamp(1.0f - m_AnimatedCameraBlendLevel, 0.0f, 1.0f);
		}

		// Update distance
		const Vector3 objectSpaceToFallback(m_RelativeFallbackPosition - m_ObjectSpacePosition);
		const float desiredDistance = objectSpaceToFallback.Mag() * desiredBlendLevel;
		m_CollisionFallBackDistanceSpring.Update(desiredDistance, springConstant, m_Metadata.m_CollisionFallBackBlendSpringDampingRatio, true);

		// Apply
		Vector3 cameraPosition;
		cameraPosition.Lerp(blendLevelToApply, m_Frame.GetPosition(), fallBackPosition);
		m_ObjectSpacePosition.Lerp(blendLevelToApply, m_ObjectSpacePosition, m_RelativeFallbackPosition);
		m_Frame.SetPosition(cameraPosition);
	}
	else
	{
		m_CollisionFallBackBlendSpring.Reset();
		m_CollisionFallBackDistanceSpring.Reset();
	}
}

void camFirstPersonShooterCamera::UpdateHeadingPitchSpringLimits()
{
	Vector2 desiredCamSpringHeadingLimitsRadians;
	Vector2 desiredCamSpringPitchPitchRadians;

	if( m_bUseAnimatedFirstPersonCamera || m_bWasUsingAnimatedFirstPersonCamera )
	{
		desiredCamSpringHeadingLimitsRadians = m_AnimatedCamSpringHeadingLimitsRadians;
		desiredCamSpringPitchPitchRadians    = m_AnimatedCamSpringPitchLimitsRadians;
	}
	else if(m_bParachuting)
	{
		if(m_bParachutingOpenedChute)
		{
			if(m_bMobileAtEar)
			{
				desiredCamSpringHeadingLimitsRadians = m_Metadata.m_HeadingSpringLimitForMobilePhoneAtEar;
				desiredCamSpringPitchPitchRadians    = m_Metadata.m_OrientationSpring.m_PitchLimits;
			}
			else if(m_bUsingMobile)
			{
				desiredCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
				desiredCamSpringPitchPitchRadians    = m_Metadata.m_OrientationSpring.m_PitchLimits;
			}
			else
			{
				desiredCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpringParachute.m_HeadingLimits;
				desiredCamSpringPitchPitchRadians    = m_Metadata.m_OrientationSpringParachute.m_PitchLimits;
			}
		}
		else
		{
			desiredCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpringSkyDive.m_HeadingLimits;
			desiredCamSpringPitchPitchRadians    = m_Metadata.m_OrientationSpringSkyDive.m_PitchLimits;
		}
	}
	else
	{
		desiredCamSpringHeadingLimitsRadians = m_Metadata.m_OrientationSpring.m_HeadingLimits;
		desiredCamSpringPitchPitchRadians    = m_Metadata.m_OrientationSpring.m_PitchLimits;
	}

	float blendLevelToApply = (m_NumUpdatesPerformed > 0) ? Max(m_Metadata.m_OrientationSpringBlend, m_AnimatedCameraBlendLevel) : 1.0f;
	m_CurrentCamSpringHeadingLimitsRadians = Lerp(blendLevelToApply, m_CurrentCamSpringHeadingLimitsRadians, desiredCamSpringHeadingLimitsRadians);
	m_CurrentCamSpringPitchLimitsRadians   = Lerp(blendLevelToApply, m_CurrentCamSpringPitchLimitsRadians, desiredCamSpringPitchPitchRadians);

	if(m_VehSeatInterp > SMALL_FLOAT)
	{
		m_CurrentCamSpringHeadingLimitsRadians.Lerp(m_VehSeatInterp, m_CurrentCamSpringHeadingLimitsRadians, Vector2(0.0f, 0.0f));
	}
}

void camFirstPersonShooterCamera::GetHeadingPitchSpringLimits(Vector2 &springHeadingLimitsRadians, Vector2 &springPitchLimitsRadians)
{
	springHeadingLimitsRadians = m_CurrentCamSpringHeadingLimitsRadians;
	springPitchLimitsRadians = m_CurrentCamSpringPitchLimitsRadians;

	springHeadingLimitsRadians.Scale(DtoR);
	springPitchLimitsRadians.Scale(DtoR);
}

void camFirstPersonShooterCamera::UpdateAttachPedPelvisOffsetSpring()
{
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	float desiredOffset = attachPed->GetIkManager().GetPelvisDeltaZForCamera(true);
	const bool shouldCut = ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || m_bStealthTransition || attachPed->GetIkManager().IsFirstPersonModeTransition());
	float fPelvisOffsetSpringConstant = shouldCut ? 0.0f : m_Metadata.m_AttachPedPelvisOffsetSpringConstant;
	if (attachPed && attachPed->GetIsInCover())
	{
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PELVIS_SPRING_CONSTANT, 0.0f, 0.0f, 200.0f, 1.0f);
		fPelvisOffsetSpringConstant = PELVIS_SPRING_CONSTANT;
	}
	m_AttachPedPelvisOffsetSpring.Update(desiredOffset, fPelvisOffsetSpringConstant, m_Metadata.m_AttachPedPelvisOffsetSpringDampingRatio);
}

void camFirstPersonShooterCamera::UpdateZoom()
{
	m_PreviousCameraFov = m_Frame.GetFov();
	m_StartedAimingWithInputThisUpdate = false;			// Init

	TUNE_GROUP_FLOAT(CAM_FPS, c_MinScopeOffsetMagnitudePercentForReticle, 0.30f, 0.0f, 1.0f, 0.001f);
	const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
	const bool bCanChangeAimState = !CNewHud::IsWeaponWheelActive() || m_SwitchedWeaponWithWeaponWheelUp;
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, FORCE_UPDATE_ZOOM_WHEN_IN_FPS, true);
	const bool bFlaggedAsAiming = FORCE_UPDATE_ZOOM_WHEN_IN_FPS && (attachPed && attachPed->GetMotionData()->GetIsFPSScope());
	bool bIsAttachedPedAiming = (bCanChangeAimState) ? (attachPed && !m_WeaponBlocked && (CPlayerInfo::IsAiming(true) || attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) || bFlaggedAsAiming)) : m_IsAiming;
	bool bIsAttachedPedFiring = attachPed && CPlayerInfo::IsFiring_s();

	const bool isPlayerJumpingOrLanding = (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding));
	const bool isPlayerParachuting = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting); //B*2743122
	const bool isRagdollBehaviour = camFollowPedCamera::ComputeShouldApplyRagdollBehavour(*attachPed, true);
	const bool blockFovChangeAndHideReticle = (isPlayerJumpingOrLanding && !isPlayerParachuting) || isRagdollBehaviour || m_AttachedIsThrownSpecialWeapon || m_ExitingVehicle;

	const CWeaponInfo* pEquippedWeaponInfo = (attachPed->GetWeaponManager() && attachPed->GetWeaponManager()->GetEquippedWeapon()) ? attachPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo() : NULL ;
	bool bAllowReticleForThrownWeapon = false;
	if(pEquippedWeaponInfo)
	{
		bAllowReticleForThrownWeapon = (pEquippedWeaponInfo->GetIsThrownWeapon() ||  attachPed->GetPedResetFlag(CPED_RESET_FLAG_ThrowingProjectile)) && CPlayerInfo::IsAiming(false);
	}

	TUNE_GROUP_FLOAT(CAM_FPS, c_nSprintAimInputThresholdInSeconds, 0.15f, 0.00f, 3.00f, 0.0005f);

	float fFovBlend = m_Metadata.m_AimFovBlend * 30.0f * fwTimer::GetCamTimeStep();
	if( m_bUsingMobile )
	{
		fFovBlend = m_Metadata.m_PhoneFovBlend * 30.0f * fwTimer::GetCamTimeStep();
		m_DesiredFov  = m_Metadata.m_PhoneFov;
		m_ShowReticle = false;
		m_IsAiming    = false;
		m_IsScopeAiming = false;
	}
	else if( !m_AttachedIsUnarmed && !m_AttachedIsUsingMelee && !m_MeleeAttack && !m_AttachedIsRNGOnlyWeapon && bIsAttachedPedAiming && !blockFovChangeAndHideReticle )
	{
		const bool bSoftFiringOnly = bCanChangeAimState && attachPed && !CPlayerInfo::IsAiming(false) && CPlayerInfo::IsSoftFiring();
		if(!m_IsAiming && !bSoftFiringOnly)
		{
			// Handle transition to aiming state.
			Shake( m_Metadata.m_ZoomInShakeRef );
			m_StartedAimingWithInputThisUpdate = (m_AccumulatedInputTimeWhileSprinting >= c_nSprintAimInputThresholdInSeconds);
		}

		const bool bWeaponHasFirstPersonScope = attachPed->GetWeaponManager() ? attachPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope() : false;
		if( attachPed->GetMotionData()->GetIsFPSScope() && bWeaponHasFirstPersonScope)
		{
			// disable the aim fov blend for sniper rifle, B* 2044575
			// DLC musket weapon also uses group_sniper, but is in the shotgun section of weapon wheel
			m_ShowReticle = false;
			m_IsAiming    = true;
			m_IsScopeAiming = false;
		}
		else if( attachPed && attachPed->GetMotionData() &&
			attachPed->GetMotionData()->GetIsFPSScope() &&
			m_ScopeFov >= 1.0f )
		{
			m_DesiredFov  = m_ScopeFov;
			m_ShowReticle = m_TaskFallParachute && !m_TaskFallLandingRoll;
			m_IsAiming    = true;
			m_IsScopeAiming = true;

			if(m_CombatRolling)
			{
				fFovBlend = m_Metadata.m_CombatRollScopeFovBlend * 30.0f * fwTimer::GetCamTimeStep();
			}
		}
		else
		{
			float fAimFov = m_Metadata.m_AimFov;
			if (m_AimFovMin > 0.0f && m_AimFovMax >= m_AimFovMin)
			{
				const float fFovSlider = ComputeFovSlider();
				fAimFov = Lerp(fFovSlider, m_AimFovMin, m_AimFovMax);
			}

			m_DesiredFov  = fAimFov;
			m_ShowReticle = (m_CurrentScopeOffset.Mag2() <= (m_ScopeOffsetSquareMagnitude*c_MinScopeOffsetMagnitudePercentForReticle));
			m_IsAiming    = true;
			m_IsScopeAiming = false;
		}

		if(bSoftFiringOnly)
		{
			// Don't change fov for soft firing. (aka half pull right trigger)
			m_DesiredFov = m_BaseFov;

			if(attachPed->GetMotionData()->GetUsingStealth())
			{
				fFovBlend = m_Metadata.m_StealthFovBlend * 30.0f * fwTimer::GetCamTimeStep();
				m_DesiredFov = m_BaseFov + m_Metadata.m_StealthFovOffset;
			}
		}
	}
	else
	{
		if(m_IsAiming)
		{
			// Handle transition to non-aiming state.
		}

		m_DesiredFov = m_BaseFov;
		if(attachPed->GetVehiclePedInside())
		{
			ComputeInitialVehicleCameraFov(m_DesiredFov);
		}

		if(attachPed->GetMotionData()->GetUsingStealth())
		{
			fFovBlend = m_Metadata.m_StealthFovBlend * 30.0f * fwTimer::GetCamTimeStep();
			m_DesiredFov = m_BaseFov + m_Metadata.m_StealthFovOffset;
		}

		m_IsAiming    = false;
		m_IsScopeAiming = false;
		if(bCanChangeAimState)
		{
			m_ShowReticle = !attachPed->GetMotionData()->GetIsFPSIdle() && !attachPed->GetMotionData()->GetIsFPSScope() && (!m_bSteerCameraWithLeftStick || bIsAttachedPedFiring) && (!blockFovChangeAndHideReticle || bAllowReticleForThrownWeapon);
		}
	}

	if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover))
	{
		m_ShowReticle = false;
	}
	else if (attachPed->GetIsInCover())
	{
		const CTaskInCover* pInCoverTask = static_cast<const CTaskInCover*>(attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
		const bool bWasScopedWhenAiming = pInCoverTask ? pInCoverTask->GetWasScopedDuringAim() : false;
		
		if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_DisableReticuleInCoverThisFrame))
		{
			m_ShowReticle = false;
		}
		else if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsPeekingFromCover) || (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverOutroToPeek) && !bWasScopedWhenAiming))
		{
			m_ShowReticle = true;
		}
		else if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsBlindFiring))
		{
			m_ShowReticle = false;
		}
		else if (!attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsPeekingFromCover) &&
			!attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) &&
			!attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
		{
			m_ShowReticle = false;
		}
		else if(attachPed->GetMotionData() && attachPed->GetMotionData()->GetIsFPSIdle())
		{
			m_ShowReticle = false;
		}
		else if (attachPed->GetMotionData() && !attachPed->GetMotionData()->GetIsFPSScope())
		{
			m_ShowReticle = true;
		}
	}

	const bool bAllowCustomFraming = m_bUsingMobile || (!m_WeaponBlocked && !m_WeaponReloading);
	if( !bAllowCustomFraming )
	{
		// Maintain aiming state, but restore default fov.
		m_DesiredFov  = m_BaseFov;
		m_ShowReticle = false;
	}

	// Hide reticle for firework launcher. Requested by B*2015784
	const CWeaponInfo* followPedWeaponInfo = camInterface::GetGameplayDirector().GetFollowPedWeaponInfo();
	if(followPedWeaponInfo && followPedWeaponInfo->GetHash() == WEAPONTYPE_DLC_FIREWORK)
	{
		m_ShowReticle = false;
	}

	////if( m_EnteringVehicle || m_ExitingVehicle || m_bEnterBlending || m_BlendFromHotwiring || m_VehicleHangingOn )
	////if( m_bEnterBlending || m_BlendFromHotwiring )
	////{
	////	ComputeInitialVehicleCameraFov( m_DesiredFov );
	////}

	if (IsTableGamesCamera())
	{
		//in table games ignore the m_bUseAnimatedFirstPersonCamera and just use the animated weight as indicator.
		if (m_AnimatedWeight > SMALL_FLOAT && m_BlendedAnimatedFov > SMALL_FLOAT)
		{
			cameraAssertf((m_BlendedAnimatedFov == 0.0f || m_BlendedAnimatedFov >= 30.0f), "First person animated fov is set below 30 degrees (%.2f), is this intentional?", m_BlendedAnimatedFov);
			m_DesiredFov = m_BlendedAnimatedFov;
		}
	}
	else
	{
		if (m_bUseAnimatedFirstPersonCamera && m_AnimatedFov > SMALL_FLOAT)
		{
			cameraAssertf((m_AnimatedFov == 0.0f || m_AnimatedFov >= 30.0f), "First person animated fov is set below 30 degrees (%.2f), is this intentional?", m_AnimatedFov);
			m_DesiredFov = m_AnimatedFov;
		}
	}

	const bool c_DisableFovBlendForVehicle = false; //m_VehicleHangingOn && !m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera;
	const bool bInstant = !m_ExitingVehicle && !m_IsIdleCameraActive && ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	if( Unlikely(bInstant) )
	{
		m_Frame.SetFov( m_DesiredFov );
	}
	else if( bCanChangeAimState && !c_DisableFovBlendForVehicle && Abs(m_DesiredFov - m_Frame.GetFov()) > SMALL_FLOAT )
	{
		float fCurrentFov = Lerp(fFovBlend, m_Frame.GetFov(), m_DesiredFov);
		m_Frame.SetFov( fCurrentFov );
	}

	UpdateCustomFovBlend( m_DesiredFov );
	
	if (m_HintHelper && IsTableGamesCamera()) //table games camera check for safety, we added zoom support to first person hints late
	{
		m_HintHelper->ComputeHintZoomScalar(m_DesiredFov);
		m_Frame.SetFov(m_DesiredFov);
	}

	m_ShowReticle &= (!m_bUseAnimatedFirstPersonCamera && !m_bEnteringTurretSeat);
}

void camFirstPersonShooterCamera::StartCustomFovBlend(float fovDelta, u32 duration)
{
	m_BlendFovDelta = fovDelta;
	m_BlendFovDeltaDuration = duration;
	m_BlendFovDeltaStartTime = fwTimer::GetCamTimeInMilliseconds();
}

void camFirstPersonShooterCamera::UpdateCustomFovBlend(float fCurrentFov)
{
	if(m_BlendFovDeltaStartTime != 0)
	{
		if(m_BlendFovDeltaStartTime + m_BlendFovDeltaDuration >= fwTimer::GetCamTimeInMilliseconds())
		{
			float fInterp = (float)(m_BlendFovDeltaStartTime + m_BlendFovDeltaDuration - fwTimer::GetCamTimeInMilliseconds()) / (float)m_BlendFovDeltaDuration;

			fInterp = Clamp(fInterp, 0.0f, 1.0f);
			//fInterp = SlowIn(fInterp);
			fInterp = CircularEaseIn(fInterp);

			m_Frame.SetFov( fCurrentFov + m_BlendFovDelta * fInterp );
		}
		else
		{
			m_BlendFovDeltaStartTime = 0;
		}
	}
}

void camFirstPersonShooterCamera::ComputeRelativeAttachPositionInternal(const Matrix34& targetMatrix, Vector3& relativeAttachPosition, Vector3& relativeFallbackPosition)
{
	//Attach to an offset from a ped bone.
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	CPed* attachedPed = (CPed*)m_AttachParent.Get();

	//Compute the basic relative offset, which is derived from metadata settings.
	camFirstPersonAimCamera::ComputeRelativeAttachPosition(targetMatrix, m_RelativeHeadOffset);

	if(attachedPed->GetPrimaryMotionTask())
	{
		fwMvClipSetId strafingClipID = attachedPed->GetPrimaryMotionTask()->GetDefaultAimingStrafingClipSet(false);
		// if the player is in ballistic suit, use armoured offset, B* 2030915
		if(strafingClipID == CLIP_SET_MOVE_STRAFE_BALLISTIC)
		{
			m_RelativeHeadOffset = m_Metadata.m_AttachRelativeOffsetArmoured;
		}
	}

#if KEYBOARD_MOUSE_SUPPORT
	float heading, pitch, roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	m_PredictedOrientationValid = false;
	float lookAroundHeadingOffset = 0.0f;
	float lookAroundPitchOffset = 0.0f;
	if(m_CurrentControlHelper && m_CurrentControlHelper->WasUsingKeyboardMouseLastInput())
	{
		const bool bCustomSituation = m_bWasCustomSituation;		// This will be off by 1 frame.
		const bool bUseHeadSpineBoneOrientation = ComputeUseHeadSpineBoneOrientation( attachedPed, false );
		const bool c_AnimatedCameraBlendingOut = !m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera;
		if( (!bCustomSituation || (m_bUsingMobile && !m_bParachutingOpenedChute) || m_bWasUsingHeadSpineOrientation) &&
			(!m_bUseAnimatedFirstPersonCamera || (!c_AnimatedCameraBlendingOut && !m_ActualAnimatedYawSpringInput)) &&
			(!m_IsLockedOn || c_AnimatedCameraBlendingOut) &&
				!bUseHeadSpineBoneOrientation )
		{
			const float fovOrientationScaling = m_Frame.GetFov() / g_DefaultFov;
			lookAroundHeadingOffset	  = m_CurrentControlHelper->GetLookAroundHeadingOffset();
			lookAroundHeadingOffset	 *= fovOrientationScaling;
			lookAroundPitchOffset	  = m_CurrentControlHelper->GetLookAroundPitchOffset();
			lookAroundPitchOffset	 *= fovOrientationScaling;

			float currentPitch = pitch;
			heading = fwAngle::LimitRadianAngleSafe(heading+lookAroundHeadingOffset);
			lookAroundHeadingOffset = fwAngle::LimitRadianAngleSafe(lookAroundHeadingOffset);
			pitch = Clamp(currentPitch + lookAroundPitchOffset, m_MinPitch, m_MaxPitch);
			lookAroundPitchOffset = pitch - currentPitch;

			if(lookAroundHeadingOffset != 0.0f || lookAroundPitchOffset != 0.0f)
			{
				// Relative head offset is transformed by camera orientation, so we need to generate the matrix based on the predicted heading/pitch.
				camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, m_matPredictedOrientation);
				m_PredictedOrientationValid = true;
			}
		}
	}
#else
	float pitch = m_Frame.ComputePitch();
#endif

	TUNE_GROUP_FLOAT(CAM_FPS, kForceStealthDisableBlendRate, 5.0f, 0.0f, 30.0f, 0.1f);
	TUNE_GROUP_FLOAT(CAM_FPS, kStealthTransitionBlendRate, 14.0f, 0.0f, 30.0f, 0.1f);
	bool bThrowingProjectile = attachedPed->GetPedResetFlag(CPED_RESET_FLAG_ThrowingProjectile);
	const bool bEnteringOrInCover = attachedPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover) || attachedPed->GetIsInCover();
	const CTaskClimbLadder* pClimbLadderTask  = (attachedPed) ? (const CTaskClimbLadder*)attachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER) : NULL;
	const bool c_ForceStealthDisable = bThrowingProjectile || pClimbLadderTask != NULL || bEnteringOrInCover || m_bLookingBehind || m_MeleeAttack || m_WasDoingMelee;
	const float fStealthOffsetBlendingSpeed = m_bStealthTransition ? kStealthTransitionBlendRate : (c_ForceStealthDisable ? kForceStealthDisableBlendRate : 1.0f);
	const bool c_UsingStealth = attachedPed->GetMotionData()->GetUsingStealth() && !c_ForceStealthDisable;
	const float fStealthOffsetBlendingTarget = c_UsingStealth ? 1.0f : 0.0f;

	if(m_NumUpdatesPerformed > 0)
	{
		m_StealthOffsetBlendingAccumulator = Clamp(fStealthOffsetBlendingTarget, 
												m_StealthOffsetBlendingAccumulator - fStealthOffsetBlendingSpeed * fwTimer::GetCamTimeStep(),
												m_StealthOffsetBlendingAccumulator + fStealthOffsetBlendingSpeed * fwTimer::GetCamTimeStep());
	}
	else
	{
		m_StealthOffsetBlendingAccumulator = fStealthOffsetBlendingTarget;
	}

	if(m_bLookingBehind)
	{
		// Techinically, this is a pop, but it occurs during the very fast turnaround and is not noticeable, even for a 15cm difference.
		m_StealthOffsetBlendingAccumulator = 0.0f;
	}

	Vector3 vAttachRelativeOffsetStealth(m_Metadata.m_AttachRelativeOffsetStealth);
	if (!attachedPed->IsMale())
	{
		// Override stealth offset for females to address camera clipping with character posing
		vAttachRelativeOffsetStealth.SetZ(0.03f);
	}
#if RSG_PC
	else if(m_TripleHead)
	{
		vAttachRelativeOffsetStealth = m_Metadata.m_AttachRelativeOffsetStealthTripleHead;
	}
#endif

	m_RelativeHeadOffset.Lerp(m_StealthOffsetBlendingAccumulator, vAttachRelativeOffsetStealth);

	Vector3 originalRelativeHeadOffset = m_RelativeHeadOffset;

	TUNE_GROUP_INT(CAM_FPS_ENTER_EXIT, sBlendoutHeadOffsetForCarJackDuration, 150, 0, 5000, 10);
	TUNE_GROUP_FLOAT(CAM_FPS, fLookDownOffsetBlendRate, 0.085f, 0.0f, 1.0f, 0.001f);
	const bool c_IsPlayerSurfaceSwimming = attachedPed->GetIsSwimming() && !m_IsCarJackingIgnoreThreshold && !attachedPed->GetIsFPSSwimmingUnderwater();
	Vector3 desiredLookDownRelativeOffset = (!m_AttachedIsUnarmed) ? m_Metadata.m_LookDownRelativeOffset : m_Metadata.m_UnarmedLookDownRelativeOffset;
	if(!c_IsPlayerSurfaceSwimming)
	{
		if( (m_LookDownRelativeOffset - desiredLookDownRelativeOffset).Mag2() > SMALL_FLOAT )
		{
			const float fOffsetBlend = fLookDownOffsetBlendRate * 30.0f * fwTimer::GetCamTimeStep();
			m_LookDownRelativeOffset.Lerp(fOffsetBlend, desiredLookDownRelativeOffset);
		}
		else
		{
			m_LookDownRelativeOffset = desiredLookDownRelativeOffset;
		}
	}
	else
	{
		m_LookDownRelativeOffset = m_Metadata.m_SwimmingLookDownRelativeOffset;
	}

	if (m_Metadata.m_MinPitch <= -1.0f && pitch < 0.0f && (!m_ExitingVehicle || !m_pVehicle.Get() || !m_pVehicle->InheritsFromPlane()))
	{
		float fInterp = Min(pitch / (m_Metadata.m_MinPitch * DtoR), 1.0f);
		m_RelativeHeadOffset.Lerp(fInterp, m_LookDownRelativeOffset);

		//
		if( m_IsCarJacking && m_BlendoutHeadOffsetForCarJackStart != 0 )
		{
			float fInterpBlendout = (float)(fwTimer::GetTimeInMilliseconds() - m_BlendoutHeadOffsetForCarJackStart) / (float)sBlendoutHeadOffsetForCarJackDuration;
			fInterpBlendout = Clamp(fInterpBlendout, 0.0f, 1.0f);
			m_RelativeHeadOffset.Lerp(fInterpBlendout, originalRelativeHeadOffset);
		}
	}

	if(m_bUseAnimatedFirstPersonCamera || m_bWasUsingAnimatedFirstPersonCamera)
	{
		// Blending in/out animated camera, blend out/in the relative head offset.
		// When blending out, m_AnimatedCameraBlendLevel goes from 1.0 to 0.0.
		m_RelativeHeadOffset.Lerp(m_AnimatedCameraBlendLevel, VEC3V_TO_VECTOR3(m_AnimatedRelativeAttachOffset));
	}

	if(c_IsPlayerSurfaceSwimming)
	{
		float moveBlendRatio = attachedPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();
		Vector3 vSwimmingRelativeOffset = Lerp(Min(moveBlendRatio / MOVEBLENDRATIO_RUN, 1.0f), m_Metadata.m_SwimmingStillRelativeOffset, m_Metadata.m_SwimmingRunRelativeOffset);
		m_RelativeHeadOffset += vSwimmingRelativeOffset;
	}

	if(!camInterface::GetGameplayDirector().IsApplyRelativeOffsetInCameraSpace())
		relativeAttachPosition = m_RelativeHeadOffset;
	else
		relativeAttachPosition.Zero();

	relativeFallbackPosition = relativeAttachPosition;

	if(m_Metadata.m_AttachBoneTag >= 0)	//A negative bone tag indicates that the ped matrix should be used.
	{
		int headBoneIndex;
		const bool hasValidHeadBone = attachedPed->GetSkeletonData().ConvertBoneIdToIndex((u16)m_Metadata.m_AttachBoneTag, headBoneIndex);
		if(hasValidHeadBone)
		{
			Matrix34 localBoneMatrix = attachedPed->GetObjectMtx(headBoneIndex);

		#if KEYBOARD_MOUSE_SUPPORT
			// Ped is updated first, so rotate head bone based on how much the camera will rotate this update.
			if(lookAroundHeadingOffset != 0.0f || lookAroundPitchOffset != 0.0f)
			{
				// Head bone is y forward, x up and z left, so rotate to z up
				localBoneMatrix.RotateLocalY(90.0f * DtoR);
				if(lookAroundHeadingOffset != 0.0f)
				{
					// Rotate position to get a predicted head bone position.
					localBoneMatrix.RotateFullZ( lookAroundHeadingOffset );
				}
				if(Abs(lookAroundPitchOffset) > SMALL_FLOAT)
				{
					// Do not rotate position as it takes time to rotate the head pitch; just rotating the orientation gives a better position.
					localBoneMatrix.RotateLocalX( lookAroundPitchOffset );
				}
				localBoneMatrix.RotateLocalY(-90.0f * DtoR);
			}
		#endif // KEYBOARD_MOUSE_SUPPORT

			// If we have root slope fixup IK being applied then we need to transform the head bone by the fixup amount.  The fixup amount will be a frame delayed
			// since the IK solve happens after the camera update but data that's a frame old is better than no data at all...
			const CRootSlopeFixupIkSolver* pRootSlopeFixupIkSolver = (const CRootSlopeFixupIkSolver*)(attachedPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
			if (pRootSlopeFixupIkSolver != NULL && pRootSlopeFixupIkSolver->GetBlend() > 0.0f)
			{
				Transform(RC_MAT34V(localBoneMatrix), RCC_MAT34V(localBoneMatrix), pRootSlopeFixupIkSolver->GetHeadTransformMatrix());
			}

			bool bClearRelativeBoneXY = true;
			if (m_Metadata.m_IsOrientationRelativeToAttached)
			{
				//bool bInVehicle = attachPed->GetIsInVehicle();
				bClearRelativeBoneXY = false;
				//bClearRelativeBoneXY = (bInVehicle || attachPed->GetIsParachuting());
				//if (bInVehicle)
				//{
				//	const CVehicle* pVehicle = attachPed->GetMyVehicle();
				//	bClearRelativeBoneXY &= !(pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike());
				//}
			}

			Vector3 relativeBonePosition = localBoneMatrix.d;
			relativeFallbackPosition = relativeBonePosition;
			if( bClearRelativeBoneXY )
			{
				//NOTE: Only use the z-component of the ped-relative bone position, as we don't want the camera to pick up any other movement.
				relativeBonePosition.x = 0.0f;
				relativeBonePosition.y = 0.0f;
			}

			relativeAttachPosition += relativeBonePosition;
		}
	}

	float fRelativeAttachPosSmoothRate = GetRelativeAttachPositionSmoothRate(*attachedPed);
	if(fRelativeAttachPosSmoothRate < 1.0f && m_PreviousRelativeAttachPosition != g_InvalidRelativePosition)
	{
		//Smooth the target-relative attach position to reduce movement 'noise'.
		//Cheaply make the smoothing frame-rate independent.
		float smoothRate = Min(fRelativeAttachPosSmoothRate * 30.0f * fwTimer::GetCamTimeStep(), 1.0f);
		relativeAttachPosition.Lerp(smoothRate, m_PreviousRelativeAttachPosition, relativeAttachPosition);
	}
}

void camFirstPersonShooterCamera::UpdatePosition()
{
	m_PreviousCameraPosition = m_Frame.GetPosition();

	const float fPreviousObjectHeight = m_ObjectSpacePosition.z;

	//NOTE: m_AttachParent has already been validated.
	Matrix34 targetMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

	CPed* attachPed = (m_AttachParent->GetIsTypePed()) ? (CPed*)m_AttachParent.Get() : NULL;
	if( attachPed && attachPed->GetHasJustLeftVehicle() &&
		(!attachPed->GetPedIntelligence() || !attachPed->GetPedIntelligence()->GetQueriableInterface() ||
		 !attachPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE)) )
	{
		// TODO: figure out how to get proper position when player is detached from car for prostitute anims. (task scenario)
	}

	ProcessRelativeAttachOffset(attachPed);

	ComputeAttachPedPelvisOffsetForShoes(attachPed);

	Vector3 relativeAttachPosition;
	ComputeRelativeAttachPositionInternal(targetMatrix, relativeAttachPosition, m_RelativeFallbackPosition);

	const bool c_TransitionForLookBehind = (m_bLookingBehind || m_bIgnoreHeadingLimits) && !IsClose(m_RelativeHeading, m_Metadata.m_LookBehindAngle*DtoR, 0.01f);
	if( !camInterface::GetGameplayDirector().IsApplyRelativeOffsetInCameraSpace() ||
		(m_ExitingVehicle && m_pVehicle.Get() && !m_pVehicle->InheritsFromPlane()) ||
		c_TransitionForLookBehind )
	{
		relativeAttachPosition += m_RelativeCoverOffset;
		if(camInterface::GetGameplayDirector().IsApplyRelativeOffsetInCameraSpace())
		{
			relativeAttachPosition += m_RelativeHeadOffset;			// Head offset not already added, so do it now.
		}
		m_ObjectSpacePosition = relativeAttachPosition;
		m_PreviousRelativeAttachPosition = relativeAttachPosition;

		TransformAttachPositionToWorldSpace(targetMatrix, relativeAttachPosition, m_AttachPosition);
	}
	else
	{
		m_ObjectSpacePosition = relativeAttachPosition;
		m_PreviousRelativeAttachPosition = relativeAttachPosition;
		TransformAttachPositionToWorldSpace(targetMatrix, relativeAttachPosition, m_AttachPosition);

		// These offset are applied in camera space, but the camera orientation used is 1 frame behind
		// as orientation requires position (for lockon calculations) so position is updated before orientation.
		Vector3 attachOffset;
	#if KEYBOARD_MOUSE_SUPPORT
		TransformAttachPositionToCameraSpace(m_RelativeCoverOffset+m_RelativeHeadOffset, attachOffset, true, m_PredictedOrientationValid ? &m_matPredictedOrientation : NULL);
	#else
		TransformAttachPositionToCameraSpace(m_RelativeCoverOffset+m_RelativeHeadOffset, attachOffset, true);
	#endif

		Vector3 objectSpaceOffset;
		m_AttachPosition += attachOffset;
		attachOffset += m_CurrentScopeOffset.GetY() * m_Frame.GetFront();
		targetMatrix.UnTransform3x3(attachOffset, objectSpaceOffset);
		m_ObjectSpacePosition += objectSpaceOffset;
	}

	m_AttachPosition.z += m_ShoesOffset;
	m_ObjectSpacePosition.z += m_ShoesOffset;

	//Pelvis offset
	const float pelvisSpringResult = m_AttachPedPelvisOffsetSpring.GetResult();
	m_AttachPosition.z += pelvisSpringResult;
	m_ObjectSpacePosition.z += pelvisSpringResult;

	//World space reticule tweak for cover
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_RETICULE_TUNE, USE_CAMERA_OFFSET_WHEN_PEEKING, true);
	if (USE_CAMERA_OFFSET_WHEN_PEEKING)
	{
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_RETICULE_TUNE, OFFSET_FOR_PEEK_LEFT, -0.05f, -0.5f, 0.5f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_RETICULE_TUNE, OFFSET_FOR_PEEK_RIGHT, -0.08f, -0.5f, 0.5f, 0.01f);
		Vector3 vDesiredOffset(0.0f, 0.0f, 0.0f);
		
		float fOffset = 0.0f;
		if (!attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsInLowCover) && attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsPeekingFromCover))
		{
			fOffset = attachPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft) ? OFFSET_FOR_PEEK_LEFT : OFFSET_FOR_PEEK_RIGHT;
			vDesiredOffset = VEC3V_TO_VECTOR3(attachPed->GetTransform().GetB());
			vDesiredOffset.Scale(fOffset);
		}
		
		const float fTimeStep = fwTimer::GetTimeStep();
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_RETICULE_TUNE, OFFSET_APPROACH_RATE, 0.3f, 0.0f, 10.0f, 0.01f);
		m_CoverPeekOffset.ApproachStraight(vDesiredOffset, OFFSET_APPROACH_RATE, fTimeStep);
		m_AttachPosition += m_CoverPeekOffset;
		m_ObjectSpacePosition.y += fOffset;
	}

	// Aiming from low cover uses the pelvis ik to alter the height of the character
	// the animations also move the player up and down, the pelvis ik should be timed
	// to coincide with the animated movement, this is mismatched which causes the camera to 
	// bobble up and down, the code below prevents this
	if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsInLowCover) && !CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*attachPed))
	{
		if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimIntro))
		{
			TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, PREVENT_LOWERING_Z_HEIGHT_DURING_AIM_INTRO, true);
			if (PREVENT_LOWERING_Z_HEIGHT_DURING_AIM_INTRO && m_ObjectSpacePosition.z < fPreviousObjectHeight)
			{
				m_AttachPosition.z = targetMatrix.d.z + fPreviousObjectHeight;
				m_ObjectSpacePosition.z = fPreviousObjectHeight;
			}
		}
		else if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro))
		{
			TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, PREVENT_INCREASING_Z_HEIGHT_DURING_AIM_OUTRO, false);
			if (PREVENT_INCREASING_Z_HEIGHT_DURING_AIM_OUTRO && m_ObjectSpacePosition.z > fPreviousObjectHeight)
			{
				m_AttachPosition.z = targetMatrix.d.z + fPreviousObjectHeight;
				m_ObjectSpacePosition.z = fPreviousObjectHeight;
			}
		}
	}

	m_Frame.SetPosition(m_AttachPosition);

	if(m_NumUpdatesPerformed > 0 && !m_ExitingVehicle && m_ExitInterpRate < 1.0f)
	{
		m_ExitInterpRate = Lerp(Min(0.20f * 30.0f * fwTimer::GetCamTimeStep(), 1.0f), m_ExitInterpRate, 1.0f);
		if(m_ExitInterpRate < (1.0f - 0.001f))
		{
			Vector3 cameraPosition = m_AttachPosition;

			float fInterpRateToUse = SlowIn(m_ExitInterpRate);
			Mat34V pedMatrix = attachPed->GetMatrix();
			Vec3V relativeCameraPosition = UnTransformFull(pedMatrix, RCC_VEC3V(cameraPosition));
			Vec3V blendedRelativePosition = Lerp( ScalarV(fInterpRateToUse), m_PreviousRelativeCameraPosition, relativeCameraPosition );
			m_ObjectSpacePosition = VEC3V_TO_VECTOR3(blendedRelativePosition);
			RC_VEC3V(cameraPosition) = Transform(pedMatrix, blendedRelativePosition);

			m_Frame.SetPosition( cameraPosition );
		}
		else
		{
			m_ExitInterpRate = 1.0f;
		}
	}

	if( m_NumUpdatesPerformed == 0 && !m_PovClonedDataValid)
	{
		// For blending, save the current camera position relative to player's mover.
		m_PreviousRelativeCameraPosition = VECTOR3_TO_VEC3V(m_ObjectSpacePosition);
	}
}

bool camFirstPersonShooterCamera::ComputeShouldAdjustForRollCage(const CVehicle* attachVehicle) const
{
	const CVehicleVariationInstance& vehicleVariation	= attachVehicle->GetVariationInstance();
	const u8 rollCageModIndex							= vehicleVariation.GetModIndex(VMT_CHASSIS);

	if (rollCageModIndex != INVALID_MOD)
	{
		if (const CVehicleKit* vehicleKit = vehicleVariation.GetKit())
		{
			const CVehicleModVisible& vehicleModVisible = vehicleKit->GetVisibleMods()[rollCageModIndex];
			if (vehicleModVisible.GetType() == VMT_CHASSIS)
			{
				return true;
			}
		}
	}

	return false;
}

bool camFirstPersonShooterCamera::GetSeatSpecificCameraOffset(const CVehicle* attachVehicle, Vector3& cameraOffset) const
{
	cameraOffset = VEC3_ZERO;

	const CVehicleModelInfo* vehicleModelInfo			= attachVehicle->GetVehicleModelInfo();
	const u32 vehicleModelHash							= vehicleModelInfo ? vehicleModelInfo->GetModelNameHash() : 0;
	const camVehicleCustomSettingsMetadata* settings	= vehicleModelHash > 0 ? camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash) : NULL;

	int seatIndex = camInterface::FindFollowPed()->GetAttachCarSeatIndex();
	if (seatIndex < 0)
		return false;

	if (settings)
	{
		if (settings->m_SeatSpecificCamerasSettings.m_ShouldConsiderData)
		{
			for (int i = 0; i < settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera.GetCount(); ++i)
			{
				if ((u32)seatIndex == settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_SeatIndex )
				{
					cameraOffset = settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_PovCameraOffset;
					return true;
				}
			}
		}
	}

	const CPed* followPed			= camInterface::FindFollowPed();
	const bool isFollowPedDriver	= followPed && followPed->GetIsDrivingVehicle();
	if (!isFollowPedDriver)
	{
		const CVehicleSeatInfo* seatInfo = attachVehicle->GetSeatInfo(seatIndex);
		if (seatInfo && !seatInfo->GetIsFrontSeat())
		{
			cameraOffset = vehicleModelInfo->GetPovRearPassengerCameraOffset();
		}
		else
		{
			cameraOffset = vehicleModelInfo->GetPovPassengerCameraOffset();
		}
	}

	return true;
}

bool camFirstPersonShooterCamera::IsFollowPedSittingInTheLeftOfTheVehicle(const CPed* attachPed, const CVehicle* attachVehicle, const CVehicleModelInfo* vehicleModelInfo, s32 attachSeatIndex) const
{
	if(attachVehicle && attachPed && vehicleModelInfo)
	{
		const CModelSeatInfo* seatInfo = vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;
		if(seatInfo)
		{
			if(attachSeatIndex >= 0)
			{
				s32 iBoneIndex = seatInfo->GetBoneIndexFromSeat(attachSeatIndex);

				if(iBoneIndex > -1) 
				{ 
					const crBoneData* pBoneData = attachVehicle->GetSkeletonData().GetBoneData(iBoneIndex);  

					if(pBoneData)
					{
						return pBoneData->GetDefaultTranslation().GetXf() < 0.0f;
					}
				}
			}
		}
	}
	return false; 
}

float camFirstPersonShooterCamera::GetRelativeHeading(bool bForceRead) const
{
	// If looking behind, ignore spring input disabled for mouse as we need to
	// pass the relative heading so ped is rotated to face look behind direction.
	const bool c_IsSpringInputDisabledForMouse = m_ActualAnimatedYawSpringInput && !m_AnimatedYawSpringInput && !m_bLookingBehind && !m_bWasLookingBehind;
	if((!m_MeleeAttack && !m_WasDoingMelee && !c_IsSpringInputDisabledForMouse) || bForceRead)
	{
		return m_RelativeHeading;
	}

	return 0.0f;
}

float camFirstPersonShooterCamera::GetRelativePitch(bool bForceRead) const
{
	if((!m_MeleeAttack && !m_WasDoingMelee)  || bForceRead)
	{
		return m_RelativePitch;
	}

	return 0.0f;
}

void camFirstPersonShooterCamera::GetFirstPersonNonScopeOffsets(const CPed* pPed, const CWeaponInfo* pWeaponInfo, Vector3& nonScopeOffset, Vector3& nonScopeRotOffset) const
{
	const CPedMotionData* pMotionData = pPed->GetMotionData();

	if (pMotionData->GetIsFPSRNG())
	{
		nonScopeOffset = pWeaponInfo->GetFirstPersonRNGOffset();
		nonScopeRotOffset = pWeaponInfo->GetFirstPersonRNGRotationOffset();
	}
	else if (pMotionData->GetIsFPSLT())
	{
		nonScopeOffset = pWeaponInfo->GetFirstPersonLTOffset();
		nonScopeRotOffset = pWeaponInfo->GetFirstPersonLTRotationOffset();
	}
}

void camFirstPersonShooterCamera::GetFirstPersonAsThirdPersonOffset(const CPed* pPed, const CWeaponInfo* pWeaponInfo, Vector3& offset)
{
	const CPedMotionData* pMotionData = pPed->GetMotionData();

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode))
	{
		offset = pWeaponInfo->GetFirstPersonAsThirdPersonWeaponBlockedOffset();
	}
	else if (pMotionData->GetIsFPSIdle())
	{
		offset = pWeaponInfo->GetFirstPersonAsThirdPersonIdleOffset();
	}
	else if (pMotionData->GetIsFPSRNG())
	{
		offset = pWeaponInfo->GetFirstPersonAsThirdPersonRNGOffset();
	}
	else if (pMotionData->GetIsFPSLT())
	{
		offset = pWeaponInfo->GetFirstPersonAsThirdPersonLTOffset();
	}
	else if (pMotionData->GetIsFPSScope())
	{
		offset = pWeaponInfo->GetFirstPersonAsThirdPersonScopeOffset();
	}
}

void camFirstPersonShooterCamera::ComputeGroundNormal(const CPed* pPed)
{
	Vector3 vDesiredGroundNormal(g_UnitUp);

	if (pPed && (pPed->GetGroundPos().z != PED_GROUNDPOS_RESET_Z))
	{
		vDesiredGroundNormal = pPed->GetGroundNormal();
	}

	TUNE_GROUP_BOOL(CAM_FPS, bForceGroundNormal, false);
	TUNE_GROUP_BOOL(CAM_FPS, bDrawGroundNormal, false);
	TUNE_GROUP_FLOAT(CAM_FPS, fGroundNormalInterpRate, 5.0f, 0.0f, 20.0f, 0.01f);

	if (vDesiredGroundNormal.IsZero() || bForceGroundNormal)
	{
		m_GroundNormal = vDesiredGroundNormal;
	}
	else
	{
		const float fTimeStep = fwTimer::GetTimeStep();

		float fAngleBetween = m_GroundNormal.Dot(vDesiredGroundNormal);
		fAngleBetween = rage::Clamp(fAngleBetween, 0.0f, 1.0f);
		fAngleBetween = rage::Acosf(fAngleBetween);

		float fAngleDelta = (fGroundNormalInterpRate * fAngleBetween) * fTimeStep;

		if (rage::Abs(fAngleBetween) <= fAngleDelta)
		{
			m_GroundNormal = vDesiredGroundNormal;
		}
		else
		{
			// Create rotation about cross product
			Vector3 vRotationAxis(m_GroundNormal);
			vRotationAxis.Cross(vDesiredGroundNormal);
			vRotationAxis.Normalize();

			Quaternion qRotation;
			qRotation.FromRotation(vRotationAxis, fAngleDelta);
			qRotation.Transform(m_GroundNormal);
			m_GroundNormal.NormalizeSafe(g_UnitUp);
		}
	}

#if __BANK 
	if (bDrawGroundNormal && pPed)
	{
		Vector3 vStart(pPed->GetGroundPos());
		Vector3 vEnd(vStart + (m_GroundNormal * 0.35f));
		grcDebugDraw::Line(vStart, vEnd, Color_yellow);
	}
#endif // __BANK
}

bool camFirstPersonShooterCamera::WhenSprintingUseRightStick(const CPed* attachPed, const CControl& rControl)
{
#if KEYBOARD_MOUSE_SUPPORT
	// B*2259601: Always use "right stick input" (mouse) for turning in FPS mode when using keyboard/mouse input on PC.
	if (rControl.UseRelativeLook())
	{
		return true;
	}
#endif	//KEYBOARD_MOUSE_SUPPORT

	eControlLayout layout = rControl.GetActiveLayout();
	switch(layout)
	{
	case STANDARD_FPS_LAYOUT:
	case STANDARD_FPS_ALTERNATE_LAYOUT:
	case TRIGGER_SWAP_FPS_LAYOUT:
	case TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
	case SOUTHPAW_FPS_LAYOUT:
	case SOUTHPAW_FPS_ALTERNATE_LAYOUT:
	case SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT:
	case SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT:
		if(!attachPed->GetIsFPSSwimming())
		{
			return true;
		}
		break;
	default:
		break;
	}	
	return false;
}

bool camFirstPersonShooterCamera::ComputeVehicleAttachMatrix(const CPed* attachPed, const CVehicle* attachVehicle, const camCinematicMountedCameraMetadata& vehicleCameraMetadata,
															 s32 attachSeatIndex, const Matrix34& seatMatrix, Matrix34& attachMatrix) const
{
	//Attach the camera the vehicle offset position that corresponds to the requested shot.
	attachMatrix = MAT34V_TO_MATRIX34(attachVehicle->GetMatrix());
	Vector3 vBoneRelativeAttachPosition = vehicleCameraMetadata.m_BoneRelativeAttachOffset;
	if(vehicleCameraMetadata.m_ShouldAttachToFollowPedHead)
	{
		Matrix34 boneMatrix;
		const bool isBoneValid = vehicleCameraMetadata.m_ShouldUseCachedHeadBoneMatrix ?
									attachPed->GetBoneMatrixCached(boneMatrix, BONETAG_HEAD) :
									attachPed->GetBoneMatrix(boneMatrix, BONETAG_HEAD);

		if(isBoneValid)
		{
			Vector3 desiredAttachPosition;
			boneMatrix.Transform(vBoneRelativeAttachPosition, desiredAttachPosition);

			//HACK using seat matrix instead of ped matrix as an approximation of ped matrix when seated.

			Vector3 desiredRelativeAttachPosition;
			seatMatrix.UnTransform(desiredAttachPosition, desiredRelativeAttachPosition);

			desiredRelativeAttachPosition.Min(desiredRelativeAttachPosition, vehicleCameraMetadata.m_MaxRelativeAttachOffsets);

			attachMatrix = seatMatrix;
			seatMatrix.Transform(desiredRelativeAttachPosition, attachMatrix.d);
			return true;
		}

		// TODO: not supported as depends on ped position.
		return false;
	}

	const CVehicleModelInfo* vehicleModelInfo = attachVehicle->GetVehicleModelInfo();
	if(vehicleCameraMetadata.m_ShouldAttachToVehicleTurret && camInterface::GetGameplayDirector().IsPedInOrEnteringTurretSeat(attachVehicle, *attachPed, false))
	{
		Vector3 VehiclePos(VEC3_ZERO); 
		const CTurret* pVehicleTurret = camInterface::GetGameplayDirector().GetTurretForPed(attachVehicle, *attachPed);
		if(attachVehicle && vehicleModelInfo && pVehicleTurret && pVehicleTurret->GetBarrelBoneId() > -1)
		{
			crSkeleton* pSkeleton = attachVehicle->GetSkeleton();
			if(pSkeleton)
			{
				Mat34V boneMat;
				int boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBarrelBoneId());
				pSkeleton->GetGlobalMtx(boneIndex, boneMat);

				MAT34V_TO_MATRIX34(boneMat).Transform(vBoneRelativeAttachPosition, attachMatrix.d);

				boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBaseBoneId());
				if(boneIndex > -1)
				{
					pSkeleton->GetGlobalMtx(boneIndex, boneMat);
				}

				attachMatrix.Set3x3(MAT34V_TO_MATRIX34(boneMat));
				return true;
			}
		}

		return false;
	}

	if(vehicleCameraMetadata.m_ShouldAttachToFollowPed)
	{
		attachMatrix = MAT34V_TO_MATRIX34(attachPed->GetMatrix());

		// HACK: use seat position as an approximation of ped position when sitting in vehicle.
		attachMatrix.d = seatMatrix.d;
		return true; 
	}

	if(vehicleCameraMetadata.m_ShouldAttachToFollowPedSeat)
	{
		const CModelSeatInfo* seatInfo = vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;
		if(seatInfo && attachSeatIndex >= 0)
		{
			if(vehicleCameraMetadata.m_ShouldRestictToFrontSeat)
			{	
				if(attachSeatIndex > 0)
				{
					//NOTE: We do not attempt to attach to the front passenger seats of buses, prison buses or coaches (rental/tour buses are handled normally),
					//as this seat is set back, which causes undesirable camera positioning and clipping in some cases.

					static const u32 LAYOUT_BUS_HASH		= ATSTRINGHASH("LAYOUT_BUS", 0xa1cdf1da);
					static const u32 LAYOUT_PRISON_BUS_HASH	= ATSTRINGHASH("LAYOUT_PRISON_BUS", 0x1b9367f);
					static const u32 LAYOUT_COACH_HASH		= ATSTRINGHASH("LAYOUT_COACH", 0xb050699b);

					const CVehicleLayoutInfo* layoutInfo	= attachVehicle->GetLayoutInfo();
					const u32 vehicleLayoutNameHash			= layoutInfo ? layoutInfo->GetName().GetHash() : 0;
					const bool isAttachedToBusOrCoach		= ((vehicleLayoutNameHash == LAYOUT_BUS_HASH) || (vehicleLayoutNameHash == LAYOUT_PRISON_BUS_HASH) ||
																(vehicleLayoutNameHash == LAYOUT_COACH_HASH));

					bool bIsSeatPositionOnLeftHandSide = IsFollowPedSittingInTheLeftOfTheVehicle(attachPed, attachVehicle, vehicleModelInfo, attachSeatIndex);
					if(!isAttachedToBusOrCoach && !bIsSeatPositionOnLeftHandSide && seatInfo->GetLayoutInfo() && seatInfo->GetLayoutInfo()->GetSeatInfo(Seat_frontPassenger))
					{
						attachSeatIndex = Seat_frontPassenger; 
					}
					else
					{
						attachSeatIndex = seatInfo->GetDriverSeat(); 
					}
				}
			}

			const s32 attachSeatBoneIndex = seatInfo->GetBoneIndexFromSeat(attachSeatIndex);
			if(cameraVerifyf(attachSeatBoneIndex != -1, "A cinematic mounted camera could not obtain a valid bone index for a seat (index %d)",
				attachSeatIndex))
			{
				Matrix34 boneMatrix;
				attachVehicle->GetSkeleton()->GetGlobalMtx(attachSeatBoneIndex, RC_MAT34V(boneMatrix));

				boneMatrix.Transform(vBoneRelativeAttachPosition, attachMatrix.d);
				return true;
			}
		}
	}
	// Not needed...
	////else if(vehicleCameraMetadata.m_ShouldAttachToVehicleExitEntryPoint)
	////{
	////	////const fwTransform& attachVehicleTransform = attachVehicle->GetTransform();

	////	Quaternion entryExitOrientation;
	////	bool hasClimbDown;
	////	ASSERT_ONLY(const bool hasValidEntryExitPoint =) camInterface::ComputeVehicleEntryExitPointPositionAndOrientation(*attachPed, *attachVehicle,
	////																							attachMatrix.d, entryExitOrientation, hasClimbDown);
	////	Assertf(hasValidEntryExitPoint == true, "Uh-oh");
	////}
	// Not needed...
	////else if(vehicleCameraMetadata.m_ShouldAttachToVehicleBone)
	////{
	////	eHierarchyId  boneId = camCinematicMountedPartCamera::ComputeVehicleHieracahyId(vehicleCameraMetadata.m_VehicleAttachPart); 
	////	
	////	Vector3 VehiclePos(VEC3_ZERO); 
	////	
	////	camCinematicVehicleTrackingCamera::ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(attachVehicle, boneId, vBoneRelativeAttachPosition, VehiclePos);
	////	
	////	attachMatrix.d = VehiclePos; 
	////}
	else
	{
		//Attach relative to the turret bone for tanks.
		// - We would ideally data-drive this, but we want to avoid changing the camera metadata schema in DLC.
		const bool isAttachedToTank = attachVehicle->IsTank();
		if(isAttachedToTank)
		{
			const s32 turretBoneIndex = vehicleModelInfo->GetBoneIndex(VEH_TURRET_1_BASE);
			if(turretBoneIndex != -1)
			{
				Matrix34 boneMatrix;
				attachVehicle->GetSkeleton()->GetGlobalMtx(turretBoneIndex, RC_MAT34V(boneMatrix));

				boneMatrix.Transform(vBoneRelativeAttachPosition, attachMatrix.d);
				return true;
			}
		}
	}

	return false;
}

void camFirstPersonShooterCamera::ComputeInitialVehicleCameraFov(float& vehicleFov) const
{
	if( m_PovVehicleCameraMetadata )
	{
		vehicleFov = m_PovVehicleCameraMetadata->m_BaseFov;
		if( m_PovVehicleCameraMetadata->m_AccelerationMovementSettings.m_ShouldMoveOnVehicleAcceleration )
		{
			const float accelerationZoomFactor	= m_PovVehicleCameraMetadata->m_AccelerationMovementSettings.m_MaxZoomFactor;
			if (accelerationZoomFactor >= SMALL_FLOAT)
			{
				vehicleFov /= accelerationZoomFactor;
			}
		}
	}
}

float camFirstPersonShooterCamera::ComputeInitialVehicleCameraRoll(const Matrix34& seatMatrix) const
{
	float fPovCameraRoll = camFrame::ComputeRollFromMatrix(seatMatrix);

	if(m_pVehicle && m_pVehicle->InheritsFromBike())
	{
		if(m_PovVehicleCameraMetadata)
		{
			// Assume not in driveby.
			float fBlendLevel = m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_RollDampingRatio;
			if( m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_OrientateUpLimit > Abs(fPovCameraRoll) )
			{
				//blend towards world up when within limits
				fPovCameraRoll = fwAngle::LerpTowards(fPovCameraRoll, 0.0f, fBlendLevel);
			}
			else if( m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_OrientateWithVehicleLimit > Abs(fPovCameraRoll) )
			{
				//vehicle is orientated outside angle in which we blend to world up, so lerp back to vehicle roll angle
				float fMaxLeanAngle = (1.0f - fBlendLevel) * m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_OrientateUpLimit;

				float sign = Sign(fPovCameraRoll);
				fPovCameraRoll = RampValueSafe(Abs(fPovCameraRoll), m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_OrientateUpLimit,
													m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_OrientateWithVehicleLimit,
													fMaxLeanAngle,	m_PovVehicleCameraMetadata->m_FirstPersonRollSettings.m_OrientateWithVehicleLimit);
				fPovCameraRoll *= sign;
			}
		}
	}

	return (fPovCameraRoll);
}

bool camFirstPersonShooterCamera::ComputeInitialVehicleCameraPosition(const CPed* attachPed, const Matrix34& seatMatrix, Vector3& vPosition) const
{
	if( m_pVehicle == NULL )
		return false;

	if(m_ExitingVehicle && m_PovClonedDataValid)
	{
		vPosition = m_PovCameraPosition;
		return true;
	}

	const CVehicleModelInfo* vehicleModelInfo	= m_pVehicle->GetVehicleModelInfo();
	if(vehicleModelInfo && m_PovVehicleCameraMetadata)
	{
		if(m_bUsingHeadAttachOffsetInVehicle)
		{
			// We are using head relative offset, so don't need this... just do the fov calculation and return false.
			return false;
		}

		Matrix34 attachMatrix;
		if( !ComputeVehicleAttachMatrix(attachPed, m_pVehicle, *m_PovVehicleCameraMetadata, m_VehicleSeatIndex, seatMatrix, attachMatrix) )
		{
			return false;
		}
		attachMatrix.RotateLocalZ(m_PovVehicleCameraMetadata->m_BaseHeading * DtoR); 

		Vector3 AttachPosition = m_PovVehicleCameraMetadata->m_RelativeAttachPosition;
		if(m_PovVehicleCameraMetadata->m_ShouldAttachToFollowPed && !m_PovVehicleCameraMetadata->m_ShouldIgnoreVehicleSpecificAttachPositionOffset)
		{
			const Vector3 vehicleModelCameraOffset	= vehicleModelInfo->GetPovCameraOffset();
			AttachPosition							+= vehicleModelCameraOffset;

			if ( ComputeShouldAdjustForRollCage(m_pVehicle) )
			{
				const float vehicleModelAdjustment	= vehicleModelInfo->GetPovCameraVerticalAdjustmentForRollCage();
				AttachPosition.z					+= vehicleModelAdjustment;
			}
		}

		//Add offsets specified by the custom seat settings
		Vector3 vSeatSpecificOffset;
		if( GetSeatSpecificCameraOffset(m_pVehicle, vSeatSpecificOffset) )
		{
			AttachPosition += vSeatSpecificOffset;
		}

		//Transform relative offset to world space.
		attachMatrix.Transform(AttachPosition, vPosition);
		return true;
	}

	return false;
}

void camFirstPersonShooterCamera::ComputeAttachPedPelvisOffsetForShoes(CPed* attachPed)
{
	if (attachPed == NULL)
	{
		m_ShoesOffset = 0.0f;
		return;
	}

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	TUNE_GROUP_FLOAT(CAM_FPS, c_HighHeelOffsetScale, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS, c_MaxAngleForHighHeelOffset, 45.0f, 0.0f, 90.0f, 0.10f);
	const bool hasHighHeels	= attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHighHeels);

	m_ShoesOffset = 0.0f;
	if(hasHighHeels)
	{
		// Since this is run before UpdateOrientation, this pitch is actually from the previous update.
		float fPitchPreviousFrame = m_Frame.ComputePitch();
		const float fHighHeelOffset = attachPed->GetIkManager().GetHighHeelHeight();
		float fCurrentHighHeelOffset = RampValue( Abs(fPitchPreviousFrame), 0.0f, c_MaxAngleForHighHeelOffset*DtoR, fHighHeelOffset, fHighHeelOffset * c_HighHeelOffsetScale);

		// Copied from CArmIkSolver::CalculateTargetPosition().
		const float fHighHeelBlend = attachPed->GetHighHeelExpressionDOF();
		m_ShoesOffset = fCurrentHighHeelOffset * fHighHeelBlend;
	}

#if __BANK
	TUNE_GROUP_BOOL(CAM_FPS, bUseBareFeetOffset, false);
#endif

	const bool hasBareFeet = attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBareFeet) BANK_ONLY(|| bUseBareFeetOffset);
	if(hasBareFeet)
	{
		TUNE_GROUP_FLOAT(CAM_FPS, c_BareFeetOffset, -0.005f, -1.0f, 1.0f, 0.01f);
		m_ShoesOffset += c_BareFeetOffset;
	}
}

void camFirstPersonShooterCamera::UpdateOrientation()
{
	//! Lock on & sticky aim need to be caluclated post position update as they require the latest frame position when blending.
	UpdateLockOn();
	UpdateStickyAim();

	const Matrix34 previousCameraMatrix = m_Frame.GetWorldMatrix();
	if(m_NumUpdatesPerformed != 0)
	{
		// restore previous camera position as m_Frame's position been updated already by UpdatePosition()
		// (on first update, m_PreviousCameraPosition has not been set yet)
		(const_cast<Matrix34*>(&previousCameraMatrix))->d = m_PreviousCameraPosition;
	}

	ReorientCameraToPreviousAimDirection();

	float heading;
	float pitch;
	float roll;
	if(!m_StartedAimingWithInputThisUpdate || m_NumUpdatesPerformed == 0)
	{
		m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);
	}
	else
	{
		m_PostEffectFrame.ComputeHeadingPitchAndRoll(heading, pitch, roll);
	}

	// Subtract any heading/pitch offset we applied, will be added at the end.
	heading = fwAngle::LimitRadianAngle(heading - m_RelativeHeading - m_IdleRelativeHeading);
	pitch   = fwAngle::LimitRadianAngle(pitch   - m_RelativePitch   - m_IdleRelativePitch);
	m_RollSpring.Reset(roll);

	CPed* attachPed = (m_AttachParent->GetIsTypePed()) ? (CPed*)m_AttachParent.Get() : NULL;
	CPedIntelligence* pIntelligence = attachPed ? attachPed->GetPedIntelligence() : NULL;

	if (m_StickyAimHelper)
	{
		m_StickyAimHelper->UpdateOrientation(heading, pitch, m_Frame.GetPosition(), m_Frame.GetWorldMatrix());
	}

	bool bOrientationRelativeToAttachedPed = false;
	ApplyOverriddenOrientation(heading, pitch);

	// Cache task state
	CTaskParachute* pParachuteTask = pIntelligence ? (CTaskParachute*)pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE) : NULL;
	m_bParachuting = pParachuteTask != NULL;
	m_bParachutingOpenedChute = pParachuteTask && (pParachuteTask->GetState() >= CTaskParachute::State_Parachuting);

	const CTaskMobilePhone* pMobilePhoneTask = NULL;
	TUNE_GROUP_BOOL(ARM_IK, bEnableFPSPhoneCamera, true);
	if( bEnableFPSPhoneCamera && pIntelligence )
	{
		pMobilePhoneTask = (const CTaskMobilePhone*)pIntelligence->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
		if(!pMobilePhoneTask)
		{
			pMobilePhoneTask = (const CTaskMobilePhone*)pIntelligence->FindTaskActiveByTreeAndType(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_MOBILE_PHONE);
		}
	}

	if( pMobilePhoneTask && attachPed )
	{
		const u32 c_uCurrentTimeMsec = fwTimer::GetTimeInMilliseconds();
		const s32 state = pMobilePhoneTask->GetState();

		if( state == CTaskMobilePhone::State_PutUpText )
		{
			if( m_StartTakeOutPhoneTime == 0 )
				m_StartTakeOutPhoneTime = c_uCurrentTimeMsec;
		}
		else
		{
			m_StartTakeOutPhoneTime = 0;
		}

		if( state == CTaskMobilePhone::State_PutDownFromText )
		{
			if( m_StartPutAwayPhoneTime == 0 )
				m_StartPutAwayPhoneTime = c_uCurrentTimeMsec;
		}
		else
		{
			m_StartPutAwayPhoneTime = 0;
		}
				
		m_bUsingMobile = !attachPed->GetIsInCover() && 
						 ((m_StartTakeOutPhoneTime != 0 && m_StartTakeOutPhoneTime + m_Metadata.m_TakeOutPhoneDelay < c_uCurrentTimeMsec) ||
						 (m_StartPutAwayPhoneTime != 0 && m_StartPutAwayPhoneTime + m_Metadata.m_PutAwayPhoneDelay > c_uCurrentTimeMsec) ||
						 state == CTaskMobilePhone::State_TextLoop ||
						 state == CTaskMobilePhone::State_HorizontalIntro ||
						 state == CTaskMobilePhone::State_HorizontalLoop ||
						 state == CTaskMobilePhone::State_HorizontalOutro ||
						 state == CTaskMobilePhone::State_PutDownToText);

		m_bMobileAtEar = state == CTaskMobilePhone::State_PutToEar || 
						 state == CTaskMobilePhone::State_EarLoop || 
						 state == CTaskMobilePhone::State_PutDownFromEar || 
						 state == CTaskMobilePhone::State_PutToEarFromText;
	}
	else
	{
		m_bUsingMobile = false;
		m_bMobileAtEar = false;
	}

	UpdateHeadingPitchSpringLimits();

	//NOTE: m_AttachParent has already been validated.
	const Matrix34 targetMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

	float fPrevAttachParentHeading = m_AttachParentHeading;
	float fPrevAttachParentPitch   = m_AttachParentPitch;

	bool bIsDiving = attachPed && attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDiving);

	bool bCarSliding = false;
	bool bEnteringVehicle = false;
	bool bInsideVehicle = false;
	bool bMountingLadderTop = false;
	bool bMountingLadderBottom = false;
	bool bClimbingLadder = false;
	bool bClimbing = false;
	bool bEnteringCover = false;
	bool bInCover = false;
	bool bExitingCover = false;
	bool bFacingLeftInCover = false;
	bool bInHighCover = false;
	bool bAtCorner = false;
	bool bAimDirectly = false;
	if (m_Metadata.m_IsOrientationRelativeToAttached && attachPed)
	{
		bInsideVehicle   = attachPed->GetIsInVehicle();
		bEnteringVehicle = attachPed->GetIsEnteringVehicle(true) || attachPed->IsBeingJackedFromVehicle();

		const CTaskVault* pVaultTask = static_cast<const CTaskVault*>(attachPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_VAULT));

		bClimbing        = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing) && pVaultTask && !pVaultTask->IsStepUp();
		
		bEnteringCover   = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover);
		bInCover         = attachPed->GetIsInCover();
		bExitingCover	 = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingCover);
		const CTaskClimbLadder* pClimbLadderTask  = (const CTaskClimbLadder*)attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER);
		if( pClimbLadderTask != NULL )
		{
			const s32 ladderTaskState = pClimbLadderTask->GetState();
			bClimbingLadder = ladderTaskState == CTaskClimbLadder::State_ClimbingUp ||
							  ladderTaskState == CTaskClimbLadder::State_ClimbingDown ||
							  ladderTaskState == CTaskClimbLadder::State_SlidingDown ||
							  ladderTaskState == CTaskClimbLadder::State_ClimbingIdle;
			bMountingLadderTop = ladderTaskState == CTaskClimbLadder::State_MountingAtTop ||
								 ladderTaskState == CTaskClimbLadder::State_MountingAtTopRun;
			bMountingLadderBottom = ladderTaskState == CTaskClimbLadder::State_MountingAtBottom ||
									ladderTaskState == CTaskClimbLadder::State_MountingAtBottomRun;
			if(ladderTaskState != CTaskClimbLadder::State_ClimbingIdle)
			{
				m_MovingUpLadder = ladderTaskState != CTaskClimbLadder::State_ClimbingDown && !bMountingLadderTop && ladderTaskState != CTaskClimbLadder::State_SlidingDown;
			}
		}

		if( attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting) )
		{
			const CTaskVault* pTaskVault = (const CTaskVault*)( attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT) );
			if( pTaskVault )
			{
				bCarSliding = pTaskVault->GetState() == CTaskVault::State_Slide;
				m_WasCarSlide |=  bCarSliding;
				m_WasClimbing &= !bCarSliding;
			}
			else
			{
				m_WasCarSlide &= bClimbing;
			}
		}
		else
		{
			m_WasCarSlide &= bClimbing;
		}

		if( !bEnteringVehicle && !m_ExitingVehicle && !bClimbing && !bClimbingLadder )
		{
			if ( ////(!camInterface::GetGameplayDirector().IsFirstPersonModeStrafeEnabled() && !attachPed->IsStrafing()) ||
				 ////attachPed->GetIsInVehicle() ||
				 ////bInCover || 
				 bInsideVehicle ||
				 pParachuteTask || bIsDiving)
			{
				if (m_NumUpdatesPerformed == 0)
				{
					fPrevAttachParentHeading = attachPed->GetCurrentHeading();
					fPrevAttachParentPitch   = attachPed->GetCurrentPitch();
				}

				heading = fwAngle::LimitRadianAngle(heading - fPrevAttachParentHeading);
				pitch   = fwAngle::LimitRadianAngle(pitch   - fPrevAttachParentPitch);
				bOrientationRelativeToAttachedPed = true;

				if (pParachuteTask && (m_NumUpdatesPerformed == 0))
				{
					// Initialize relative heading/pitch
					Vector2 springHeadingLimitsRadians;
					Vector2 springPitchLimitsRadians;
					GetHeadingPitchSpringLimits(springHeadingLimitsRadians, springPitchLimitsRadians);

					m_RelativeHeading = Clamp(heading, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
					m_RelativePitch   = Clamp(pitch, springPitchLimitsRadians.x, springPitchLimitsRadians.y);

					heading = 0.0f;
					pitch = 0.0f;
				}
			}
		}

		if (!CTaskCover::CanUseThirdPersonCoverInFirstPerson(*attachPed))
		{
			const CTaskCover* pCoverTask = (const CTaskCover*)attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER);
			if (pCoverTask)
			{
				bFacingLeftInCover = pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft);
				bInHighCover = pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint);
				bAtCorner = CTaskInCover::CanPedFireRoundCorner(*attachPed, bFacingLeftInCover);
				bAimDirectly = pCoverTask->IsCoverFlagSet(CTaskCover::CF_AimDirectly);
			}

			if (bEnteringCover && pCoverTask)
			{
				ProcessEnterCoverOrientation(*attachPed, heading, bFacingLeftInCover, bInHighCover, bAtCorner);
			}
			else
			{
				// Reset
				m_bHaveMovedCamDuringCoverEntry = false;
			}
		}
		else
		{
			const bool bSwitchedToFirstPersonFromThirdPersonCover = attachPed->GetPlayerInfo() && attachPed->GetPlayerInfo()->GetSwitchedToFirstPersonFromThirdPersonCoverCount() > 0 ? true : false;
			if( bInCover && !m_WasInCover && !bSwitchedToFirstPersonFromThirdPersonCover)
			{
				heading = 0.0f;
				pitch   = 0.0f;
			}
		}

		CTaskPlayerOnFoot* pTaskPlayer = (attachPed->GetPedIntelligence()) ? (CTaskPlayerOnFoot*)attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT) : NULL;
		if(pTaskPlayer)
		{
			m_TaskPlayerState = pTaskPlayer->GetState();
			m_SlopeScrambling = pTaskPlayer->IsSlopeScrambling();
		}
	}

	//NOTE: We scale the look-around offsets based up the current FOV, as this allows the look-around input to be responsive at minimum zoom without
	//being too fast at maximum zoom.
	float fovOrientationScaling = m_Frame.GetFov() / g_DefaultFov;

	bool bIsFalling = false;
	if(attachPed)
	{
		m_AttachParentHeading = attachPed->GetCurrentHeading();
		m_AttachParentPitch   = attachPed->GetCurrentPitch();
		if(Abs(m_RelativeHeading) > SMALL_FLOAT)
		{
			m_AttachParentPitch = RampValueSafe( Abs(m_RelativeHeading), 0.0f*DtoR, 180.0f*DtoR, m_AttachParentPitch, -m_AttachParentPitch );
		}

		if( attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) && !attachPed->GetIsParachuting() )
		{
			const CTaskJump* pTaskJump = (const CTaskJump*)( attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP) );
			if( pTaskJump && pTaskJump->GetIsStateFall() )
			{
				bIsFalling = true;
			}
		}
		else if( !attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling) )	// don't want high fall included
		{
			bIsFalling = m_TaskFallFalling;
		}
	}

	bool bPlayerUpsidedown = false;

	bool bUseHeadSpineBoneOrientation = ComputeUseHeadSpineBoneOrientation( attachPed, bIsFalling );

	if( !bUseHeadSpineBoneOrientation && attachPed && !bIsDiving && !m_bWasDiving && !attachPed->GetIsSwimming() )
	{
		// Test if head up is close to straight down for case when Michael is hanging by his feet.
		Matrix34 headBoneMatrix;
		attachPed->GetBoneMatrix(headBoneMatrix, BONETAG_HEAD);
		headBoneMatrix.RotateLocalY(90.0f * DtoR); //head matrix is rotated -90 degrees

		TUNE_GROUP_FLOAT(CAM_FPS, fUpsidedownAngleThreshold, 16.0f, 0.0f, 60.0f, 1.0f);
		if( -headBoneMatrix.c.z >= rage::Cosf(fUpsidedownAngleThreshold*DtoR) )
		{
			bPlayerUpsidedown = true;
		}
	}

	float lookAroundHeadingOffset = 0.0f;
	float lookAroundPitchOffset   = 0.0f;
	if( /*!bInsideVehicle &&*/ m_CurrentControlHelper )
	{
		//NOTE: m_ControlHelper has already been validated.
		lookAroundHeadingOffset	 = m_CurrentControlHelper->GetLookAroundHeadingOffset();
		lookAroundHeadingOffset	*= fovOrientationScaling;
		lookAroundPitchOffset	 = m_CurrentControlHelper->GetLookAroundPitchOffset();
		lookAroundPitchOffset	*= fovOrientationScaling;

		//Lauren is really sorry about this hack. We had a bug with the blend out of the slot machine camera, there was a strange knockback
		//like it was blending between two different orientations, or one camera was getting the look around but not the other.
		//This was the only fix I could find in our timeframe (block all look around during the interpolation), but I'm really not proud of it.
		const u32 tableGamesCameraNameBlendingOut		= camInterface::GetGameplayDirector().GetBlendingOutTableGamesCameraName();
		if (tableGamesCameraNameBlendingOut && camInterface::GetGameplayDirector().IsCameraInterpolating())
		{
			static const u32 slotMachineCameraName		= ATSTRINGHASH("CASINO_SLOT_MACHINE_CAMERA", 0x1EE8CB4C);
			const bool blendingFromSlotMachineCamera	= (tableGamesCameraNameBlendingOut == slotMachineCameraName);
			if (blendingFromSlotMachineCamera)
			{
				lookAroundHeadingOffset	= 0.0f;
				lookAroundPitchOffset	= 0.0f;
			}
		}

		if(m_HasStickyTarget && m_StickyAimEnvelopeLevel > 0.0f)
		{
			Vector3 vStickyPosition = m_StickyAimTargetPosition;
			
			//! convert to local space position.
			MAT34V_TO_MATRIX34(m_StickyAimTarget->GetTransform().GetMatrix()).Transform(vStickyPosition);

			//! get direction to "target".
			Vector3 stickyDelta	= vStickyPosition - m_Frame.GetPosition();
			stickyDelta.NormalizeSafe();
			float desiredStickyHeading;
			float desiredStickyPitch;
			camFrame::ComputeHeadingAndPitchFromFront(stickyDelta, desiredStickyHeading, desiredStickyPitch);

			heading = fwAngle::Lerp(heading, desiredStickyHeading, m_StickyAimHeadingRate * 30.0f * m_StickyAimEnvelopeLevel * fwTimer::GetCamTimeStep());
		}

		ModifyLookAroundOffsetForStickyAim(heading, pitch, lookAroundHeadingOffset, lookAroundPitchOffset);

		bool bAllowLeftStickPitch = false;
		// Allow left stick pitching if swim sprinting underwater and using a standard TPS control layout
		if (attachPed && attachPed->GetIsFPSSwimmingUnderwater())
		{
			CTaskMotionDiving *pDivingTask = static_cast<CTaskMotionDiving*>(attachPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_DIVING));
			if (pDivingTask && pDivingTask->IsFullSprintSwimming())
			{
				if (attachPed->GetControlFromPlayer())
				{
					eControlLayout controlLayout = attachPed->GetControlFromPlayer()->GetActiveLayout();
					bool bValidControlLayout = (controlLayout == STANDARD_TPS_LAYOUT || controlLayout == TRIGGER_SWAP_TPS_LAYOUT || controlLayout == SOUTHPAW_TPS_LAYOUT || controlLayout == SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT);
					if (bValidControlLayout)
					{
						bAllowLeftStickPitch = true;
					}
				}
			}
		}

		if( m_bSteerCameraWithLeftStick && !bAllowLeftStickPitch)
		{
			lookAroundPitchOffset = 0.0f;
		}

		if(bPlayerUpsidedown)
		{
			// Invert stick direction when upside down.
			lookAroundHeadingOffset	*= -1.0f;
			lookAroundPitchOffset	*= -1.0f;
		}
	}

	bool bBeingBustedOnFoot = false;
	bool bBeingBustedInVehicle = false;
	if(attachPed && attachPed->GetPedIntelligence() && attachPed->GetPedIntelligence()->GetQueriableInterface())
	{
		bBeingBustedOnFoot = (attachPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_BUSTED) && attachPed->GetIsOnFoot());
		if (attachPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT))
		{
			CTaskExitVehicleSeat *pTaskExitVehSeat = static_cast<CTaskExitVehicleSeat*>(attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
			if (pTaskExitVehSeat)
			{
				bBeingBustedInVehicle = pTaskExitVehSeat->IsBeingArrested();
			}
		}
	}

	//reset the relative heading and pitch if we are rendering another camera and the controls are disabled 
	const bool isRenderingAnotherCamera = camInterface::GetDominantRenderedCamera() != this;
	const bool areControlsDisabled = attachPed && attachPed->GetPlayerInfo() && attachPed->GetPlayerInfo()->AreControlsDisabled();
	const bool isTableGamesCamera = IsTableGamesCamera();
	if((!m_ScriptPreventingOrientationResetWhenRenderingAnotherCamera && !isTableGamesCamera && isRenderingAnotherCamera && !bInCover && areControlsDisabled) || bBeingBustedOnFoot)
	{
		bOrientationRelativeToAttachedPed = true;
		heading = 0.0f;
		pitch = 0.0f;
	}
	m_ScriptPreventingOrientationResetWhenRenderingAnotherCamera = false;

	// If running an animated camera, preserve previous frame's roll angle.
	float relativeRollToApply = (!m_bUseAnimatedFirstPersonCamera && !m_RunningAnimTask && !isTableGamesCamera) ? 0.0f : roll;
	bool bShouldLimitHeadingOrientation = ProcessLimitHeadingOrientation(*attachPed, bInCover, bExitingCover);
	bool bShouldLimitPitchOrientation = ProcessLimitPitchOrientation(*attachPed, bInCover);

	if( !m_bLookingBehind && ((attachPed->IsStrafing() && !bIsDiving) || !bOrientationRelativeToAttachedPed) )
	{
		float fTempHeading = heading;
		float fTempPitch = pitch;
		ApplyOrientationLimits(targetMatrix, fTempHeading, fTempPitch);

		if (bShouldLimitHeadingOrientation)
		{
			if( !m_bIgnoreHeadingLimits || heading == fTempHeading )
			{
				if ( Abs(m_RelativeHeading) < 1.0f*DtoR || m_IsAiming )
				{
					m_bIgnoreHeadingLimits = false;
				}
				heading = fTempHeading;
			}
		}
		if (bShouldLimitPitchOrientation)
		{
			pitch = fTempPitch;
		}
	}


	float fPitchForSlope = 0.0f;
	bool bPitchForSlopeSet = false;
	if (attachPed)
	{
		ComputeGroundNormal(attachPed);

		if(!attachPed->GetIsSwimming() && !pParachuteTask && !bIsDiving && !attachPed->GetGroundPhysical())
		{
			bPitchForSlopeSet = ComputeDesiredPitchForSlope(attachPed, fPitchForSlope);
		}
	}

	if(m_EnteringVehicle || m_bEnterBlending)
	{
		relativeRollToApply = camFrame::ComputeRollFromMatrix(previousCameraMatrix);
	}

	// Save current value as m_bWasUsingHeadSpineOrientation will be reset by HandleCustomSituations()
	bool bUsedHeadSpineBoneOrientationLastUpdate = m_bWasUsingHeadSpineOrientation;
	bool bCustomSituation = false;
	if( !m_bUseAnimatedFirstPersonCamera )
	{
		CustomSituationsParams oParam;
		oParam.pParachuteTask				= pParachuteTask;
		oParam.pMobilePhoneTask				= pMobilePhoneTask;
		oParam.targetPitch					= fPitchForSlope;
		oParam.lookAroundHeadingOffset		= lookAroundHeadingOffset;
		oParam.lookAroundPitchOffset		= lookAroundPitchOffset;
		oParam.currentRoll					= roll;
		oParam.previousAttachParentHeading	= fPrevAttachParentHeading;
		oParam.previousAttachParentPitch	= fPrevAttachParentPitch;
		oParam.bEnteringVehicle				= bEnteringVehicle;
		oParam.bClimbing					= bClimbing;
		oParam.bClimbingLadder				= bClimbingLadder;
		oParam.bMountingLadderTop			= bMountingLadderTop;
		oParam.bMountingLadderBottom		= bMountingLadderBottom;
		oParam.bJumping						= attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping);
		oParam.bCarSlide					= bCarSliding;
		oParam.bUsingHeadSpineOrientation	= bUseHeadSpineBoneOrientation;
		oParam.bGettingArrestedInVehicle	= bBeingBustedInVehicle;

		bCustomSituation = HandleCustomSituations(	attachPed, heading, pitch, relativeRollToApply,
													bOrientationRelativeToAttachedPed, oParam );
	}
	m_bWasCustomSituation = bCustomSituation && !bUseHeadSpineBoneOrientation;
	bCustomSituation     &= !m_BlendOrientation;

	if( !m_bLookingBehind && m_bIgnoreHeadingLimits &&
		(bCustomSituation || bUseHeadSpineBoneOrientation || m_bUseAnimatedFirstPersonCamera || Abs(m_RelativeHeading) < 1.0f*DtoR) )
	{
		m_RelativeHeadingSpring.Reset( 0.0f );
		m_RelativeHeading = 0.0f;
	}

	const bool bAnimatedCameraChangesStickHeading = (!m_CombatRolling && !m_TaskFallLandingRoll && !m_RunningAnimTask && !m_TakingOffHelmet);
	TUNE_GROUP_BOOL(CAM_FPS, bBlendOutRelativeHeadingPitch, false);

	const bool c_IsMelee = m_MeleeAttack || m_WasDoingMelee;
	const bool c_AnimatedCameraBlendingOut = !m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera;
	// Melee attacks are handled by CustomSituation or Animated camera, kill input for other cases (which is melee attack without animated camera, but no target)
	if( (!bCustomSituation || (m_bUsingMobile && !m_bParachutingOpenedChute) || m_bWasUsingHeadSpineOrientation) &&
		(!m_bUseAnimatedFirstPersonCamera || (!c_AnimatedCameraBlendingOut && !m_ActualAnimatedYawSpringInput)) &&
		(!m_IsLockedOn || c_AnimatedCameraBlendingOut) &&
		 !bUseHeadSpineBoneOrientation )
		////&& !m_bWasCustomSituation && !m_bWasUsingAnimatedFirstPersonCamera && !m_bWasUsingHeadSpineOrientation )
	{
		heading	+= lookAroundHeadingOffset;
		if( !bAnimatedCameraChangesStickHeading && (m_bUseAnimatedFirstPersonCamera || bUseHeadSpineBoneOrientation) )
		{
			m_SavedHeading = fwAngle::LimitRadianAngle( m_SavedHeading + lookAroundHeadingOffset );
		}

		// Blend out relative heading and apply the difference to 
		if( m_PreviousRelativeHeading != 0.0f && ((!m_bLookingBehind && !m_bIgnoreHeadingLimits) || m_bLookBehindCancelledForAiming) )
		{
			if( (!m_bUseAnimatedFirstPersonCamera || m_ActualAnimatedYawSpringInput) &&
				(m_bLookBehindCancelledForAiming || /*c_AnimatedCameraBlendingOut ||*/ (!bBlendOutRelativeHeadingPitch && !c_IsMelee)) )
			{
				const bool c_IsSpringInputDisabledForMouse = m_ActualAnimatedYawSpringInput && !m_AnimatedYawSpringInput;
				// Since spring input was requested, but we disabled for mouse input, let it blend out, we are cancelling look behind for aiming.
				if(!c_IsSpringInputDisabledForMouse || (m_bWasLookingBehind && m_bLookBehindCancelledForAiming))
				{
					heading = fwAngle::LimitRadianAngleSafe(heading + m_PreviousRelativeHeading);
					m_RelativeHeadingSpring.Reset( 0.0f );
					m_RelativeHeading = 0.0f;
				}
			}
			else
			{
				// When input is re-enabled and we have input, apply the relative heading and then clear it.
				float fAngleDifference = fwAngle::LimitRadianAngle(m_PreviousRelativeHeading - m_RelativeHeading);
				heading = fwAngle::LimitRadianAngle(heading + fAngleDifference);

				if(Abs(m_RelativeHeading) < 0.01f*DtoR)
				{
					m_RelativeHeadingSpring.Reset( 0.0f );
					m_RelativeHeading = 0.0f;
				}
			}
		}
	}

	if( (!bCustomSituation || m_bWasUsingHeadSpineOrientation) &&
		(!m_bUseAnimatedFirstPersonCamera || (!c_AnimatedCameraBlendingOut && !m_ActualAnimatedPitchSpringInput)) &&
		(!m_IsLockedOn || c_AnimatedCameraBlendingOut) &&
		 !bUseHeadSpineBoneOrientation )
		////&& !m_bWasCustomSituation && !m_bWasUsingAnimatedFirstPersonCamera && !m_bWasUsingHeadSpineOrientation )
	{
		pitch	+= lookAroundPitchOffset;
		if( !bAnimatedCameraChangesStickHeading && (m_bUseAnimatedFirstPersonCamera || bUseHeadSpineBoneOrientation) )
		{
			m_SavedPitch = fwAngle::LimitRadianAngle( m_SavedPitch + lookAroundPitchOffset );
		}

		// Blend out relative pitch and apply the difference to 
		if( m_PreviousRelativePitch != 0.0f )
		{
			if( (!m_bUseAnimatedFirstPersonCamera || m_ActualAnimatedPitchSpringInput) &&
				(/*c_AnimatedCameraBlendingOut ||*/ (!bBlendOutRelativeHeadingPitch && !c_IsMelee)) )
			{
				pitch = fwAngle::LimitRadianAngleSafe(pitch + m_PreviousRelativePitch);
				m_RelativePitchSpring.Reset( 0.0f );
				m_RelativePitch = 0.0f;
			}
			else
			{
				float fAngleDifference = fwAngle::LimitRadianAngle(m_PreviousRelativePitch - m_RelativePitch);
				pitch = fwAngle::LimitRadianAngle(pitch + fAngleDifference);

				if(Abs(m_RelativePitch) < 0.01f*DtoR)
				{
					m_RelativePitchSpring.Reset( 0.0f );
					m_RelativePitch = 0.0f;
				}
			}
		}
	}

	const CPedMotionData* attachPedMotionData = attachPed->GetMotionData();
	const bool c_IsSprinting = attachPedMotionData ? attachPedMotionData->GetIsSprinting() : false;
	if(c_IsSprinting)
	{
		const float c_MaxInputMagnitude = Max(Abs(lookAroundHeadingOffset), Abs(lookAroundPitchOffset));
		if(c_MaxInputMagnitude >= 0.001f)
		{
			const float timeScale = RampValue(c_MaxInputMagnitude, 0.15f, 1.0f, 1.0f, 5.0f) * RampValue(Abs(pitch), 0.10f, 0.40f, 1.0f, 2.0f);
			m_AccumulatedInputTimeWhileSprinting += fwTimer::GetTimeStep() * timeScale;
		}
	}
	else
	{
		m_AccumulatedInputTimeWhileSprinting = 0.0f;
	}

	if(bPlayerUpsidedown)
	{
		if(!bOrientationRelativeToAttachedPed)
		{
			heading = fwAngle::LimitRadianAngle(heading - fPrevAttachParentHeading);
			pitch   = fwAngle::LimitRadianAngle(pitch   - fPrevAttachParentPitch);
			bOrientationRelativeToAttachedPed = true;
		}

		// TODO: setup separate pitch limits for updsidedown?
		pitch   = Clamp(pitch,   m_Metadata.m_ParachutePitchLimit.x   * DtoR, m_Metadata.m_ParachutePitchLimit.y   * DtoR);
		heading = Clamp(heading, m_Metadata.m_ParachuteHeadingLimit.x * DtoR, m_Metadata.m_ParachuteHeadingLimit.y * DtoR);
	}

	bool bOrientPitchRelativeToAttachedPed = true;
	if( pMobilePhoneTask && pMobilePhoneTask->GetState() == CTaskMobilePhone::State_EarLoop && !IsLookingBehind() && !IsDoingLookBehindTransition())
	{
		if( !bOrientationRelativeToAttachedPed )
		{
			heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
			bOrientationRelativeToAttachedPed = true;
			bOrientPitchRelativeToAttachedPed = false;	// Only orient heading relative to attached ped
		}

		heading = Clamp(heading, m_Metadata.m_PhoneToEarHeadingLimit.x * DtoR, m_Metadata.m_PhoneToEarHeadingLimit.y * DtoR);
	}

	if(attachPed)
	{
		// B*2015573: Clamp camera heading limits when using the swimming motion tasks so we don't clip with the peds body 
		// (we don't turn instantly, we lerp our heading to give the feeling of being in water)
		if (attachPed->GetIsFPSSwimming() && !m_IsCarJacking && attachPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask))
		{
			if( !bOrientationRelativeToAttachedPed )
			{
				heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
				bOrientationRelativeToAttachedPed = true;
				bOrientPitchRelativeToAttachedPed = false;	// Only orient heading relative to attached ped
			}

			heading = Clamp(heading, m_Metadata.m_SwimmingHeadingLimit.x * DtoR, m_Metadata.m_SwimmingHeadingLimit.y * DtoR);
		}

		// When script controls the player, prevent the camera from rotating freely
		if( attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasControlOfPlayer) )
		{
			if( !bOrientationRelativeToAttachedPed )
			{
				heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
				pitch	= fwAngle::LimitRadianAngle(pitch - m_AttachParentPitch);
				bOrientationRelativeToAttachedPed = bOrientPitchRelativeToAttachedPed = true;
			}

			heading = Clamp(heading, -m_Metadata.m_HeadingLimitForScriptControl.x * DtoR, m_Metadata.m_HeadingLimitForScriptControl.x * DtoR);
			pitch	= Clamp(pitch, -m_Metadata.m_HeadingLimitForScriptControl.y * DtoR, m_Metadata.m_HeadingLimitForScriptControl.y * DtoR);
		}
	}

	if (bOrientationRelativeToAttachedPed)
	{
		heading = fwAngle::LimitRadianAngle(heading + m_AttachParentHeading);
		if (bOrientPitchRelativeToAttachedPed)
		{
			pitch = fwAngle::LimitRadianAngle(pitch + m_AttachParentPitch);
		}
	}

	if(attachPed && !m_bWasParachuting && !m_bWasDiving)
	{
		// Update MinPitch based on relative heading.  Further we move away from center, more we reduce min pitch.
		TUNE_GROUP_FLOAT(CAM_FPS, fMinPitchWhenLookBehindNonStealth, -9.0f, -45.0f, 30.0f, 1.0f);
		TUNE_GROUP_FLOAT(CAM_FPS, fMinPitchWhenLookBehindStealth,     4.0f, -45.0f, 30.0f, 1.0f);
		float fMinPitchWhenLookBehind = (!attachPed->GetMotionData() || !attachPed->GetMotionData()->GetUsingStealth()) ? fMinPitchWhenLookBehindNonStealth : fMinPitchWhenLookBehindStealth;
		if(m_bLookingBehind || m_bIgnoreHeadingLimits)
		{
			if( fPitchForSlope > 0.0f )
			{
				m_fExtraLookBehindPitchLimitDegrees = Lerp(0.30f * 30.0f * fwTimer::GetCamTimeStep(), m_fExtraLookBehindPitchLimitDegrees, fPitchForSlope * RtoD);
			}
			else
			{
				m_fExtraLookBehindPitchLimitDegrees = Lerp(0.30f * 30.0f * fwTimer::GetCamTimeStep(), m_fExtraLookBehindPitchLimitDegrees, 0.0f);
			}
			fMinPitchWhenLookBehind += m_fExtraLookBehindPitchLimitDegrees;
		}

		m_MinPitch = RampValueSafe(Abs(m_RelativeHeading), 0.0f, m_Metadata.m_LookBehindAngle*DtoR, m_MinPitch, fMinPitchWhenLookBehind * DtoR);
		pitch = Clamp(pitch, m_MinPitch, m_MaxPitch);
	}

	if(CPauseMenu::GetMenuPreference(PREF_FPS_AUTO_LEVEL))
	{
#if RSG_PC
		CControl& rControl = CControlMgr::GetMainPlayerControl(true);
		if(!rControl.UseRelativeLook())
#endif // RSG_PC
		{
			if( (m_AttachedIsUnarmed || m_AttachedIsUsingMelee || m_bSteerCameraWithLeftStick || (attachPed->GetMotionData() && attachPed->GetMotionData()->GetIsFPSIdle())) &&
				bPitchForSlopeSet && (!bCustomSituation || m_SlopeScrambling) && !m_BlendOrientation && fPitchForSlope != pitch &&
				lookAroundHeadingOffset == 0.0f && lookAroundPitchOffset == 0.0f  )
			{
				float fBlendRate = (m_bSteerCameraWithLeftStick || m_bLookingBehind) ? m_Metadata.m_SprintPitchBlend : m_Metadata.m_SlopePitchBlend;
				//if(m_SlopeScrambling)
				//	fBlendRate = m_Metadata.m_SlopePitchBlend*4.0f;

				// TODO: use spring instead of lerp
				pitch = Lerp(Min(fBlendRate * 30.0f * fwTimer::GetCamTimeStep(), 1.0f), pitch, fPitchForSlope);
			}
		}
	}

	float lockonHeading = TWO_PI;
	float lockonPitch = TWO_PI;
	if(m_IsLockedOn && m_LockOnEnvelopeLevel > 0.0f)
	{
		//Only update the lock-on orientation if there is sufficient distance between the base pivot position and the target positions,
		//otherwise the lock-on behavior can become unstable.
		if(ComputeIsProspectiveLockOnTargetPositionValid(m_LockOnTargetPosition))
		{
			Vector3 lockOnDelta	= m_LockOnTargetPosition - m_Frame.GetPosition();

			if(m_bLookingBehind)
			{
				m_RelativeHeadingSpring.Reset( 0.0f );
				m_RelativePitchSpring.Reset( 0.0f );
				m_RelativeHeading = 0.0f;
				m_RelativePitch = 0.0f;
				m_bWaitForLookBehindUp = true;
				m_bLookingBehind = false;
				m_bWasLookingBehind = false;
			}

			TUNE_GROUP_BOOL(CAM_FPS, bDoItTheOldWay, false);
			float desiredLockOnPitch = 0.0f;
			if(bDoItTheOldWay)
			{
				lockOnDelta.NormalizeSafe();
				camFrame::ComputeHeadingAndPitchFromFront(lockOnDelta, heading, desiredLockOnPitch);
				lockonHeading = heading;
				m_RelativeHeadingSpring.Reset( 0.0f );
				m_RelativeHeading = 0.0f;
			}

			if(	(!bCustomSituation || !c_IsMelee) &&
				(!m_bUseAnimatedFirstPersonCamera || !c_IsMelee) &&
				(!m_bWasUsingAnimatedFirstPersonCamera || !c_IsMelee) )
			{
				if(!bDoItTheOldWay)
				{
					lockOnDelta.NormalizeSafe();
					camFrame::ComputeHeadingAndPitchFromFront(lockOnDelta, heading, desiredLockOnPitch);
					lockonHeading = heading;
					m_RelativeHeadingSpring.Reset( 0.0f );
					m_RelativeHeading = 0.0f;
				}

				// Don't let lockon code overwrite pitch during melee, so custom behaviour can look at target bone.
				pitch = desiredLockOnPitch;
				m_RelativePitchSpring.Reset( 0.0f );
				m_RelativePitch = 0.0f;
			}
			lockonPitch = pitch;
		}
	}

	//NOTE: We don't support hinting when aiming.
	if(m_HintHelper)
	{
		float lookAroundInputEnvelopeLevel = m_CurrentControlHelper ? m_CurrentControlHelper->GetLookAroundInputEnvelopeLevel() : 0.0f;

		//Reuse the blend level used for DOF to blend out hinting when aiming. We achieve this by piggybacking on the response to look-around input.
		//TODO: Consider update order issues with the aiming blend level and potentially add a new blend level to allow for custom blend times.
		const float aimingBlendLevel = m_DofAimingBlendSpring.GetResult();
		lookAroundInputEnvelopeLevel = Lerp(aimingBlendLevel, lookAroundInputEnvelopeLevel, 1.0f);

		//Also piggyback on the look-around blend level to blend-out hinting when the XY-distance to the target is too small, to reduce the scope for instability.
		const Vector3& hintTargetPosition	= camInterface::GetGameplayDirector().UpdateHintPosition();
		Vector3 attachPositionToHintTarget	= hintTargetPosition - m_AttachPosition;
		attachPositionToHintTarget.z		= 0.0f;
		const float xyDistanceToHintTarget	= attachPositionToHintTarget.Mag();
		const float distanceBasedBlendLevel	= RampValueSafe(xyDistanceToHintTarget, m_Metadata.m_XyDistanceLimitsForHintBlendOut.x, m_Metadata.m_XyDistanceLimitsForHintBlendOut.y, 0.0f, 1.0f);
		lookAroundInputEnvelopeLevel		= Lerp(distanceBasedBlendLevel, 1.0f, lookAroundInputEnvelopeLevel);

		Vector3 hintOffset(Vector3::ZeroType);
		m_HintHelper->ComputeHintPivotPositionAdditiveOffset(hintOffset.x, hintOffset.z); 

		m_HintHelper->ComputeHintOrientation(m_AttachPosition, Vector2(m_MinPitch, m_MaxPitch), lookAroundInputEnvelopeLevel, heading, pitch, m_Frame.GetFov(), 0.0f, hintOffset);
	}

	//Update the custom cover logic in high cover to prevent player from staring at the wall when idle
	if (bInCover && attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint) && m_CurrentControlHelper && CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonLocoAnimations)
	{
		ProcessCoverAutoHeadingCorrection(*attachPed, heading, pitch, bFacingLeftInCover, bInHighCover, bAtCorner, bAimDirectly);
		ProcessCoverPitchRestriction(*attachPed, heading, pitch, bInHighCover);
		ProcessCoverRoll(*attachPed, relativeRollToApply, heading, bFacingLeftInCover, bInHighCover, bAtCorner, bAimDirectly);
	}
	else
	{
		if (attachPed && attachPed->GetPlayerInfo())
		{
			attachPed->GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
		}

		m_CoverHeadingCorrectionDetectedStickInput = false;

		INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_PEEK_ROLL, -180.0f, 180.0f, 0.05f);
		if (!IsClose(MAX_PEEK_ROLL, 0.0f, SMALL_FLOAT))
		{
			// Calculate the interpolation value before reducing the relative peek roll
			float interpValue = Clamp(Abs(m_RelativePeekRoll / (MAX_PEEK_ROLL * DtoR)), 0.0f, 1.0f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PEEK_ROLL_BLEND_OUT_TO_NO_COVER_RATE, 10.0f, 0.0f, 180.0f, 0.05f);
			Approach(m_RelativePeekRoll, 0.0f, PEEK_ROLL_BLEND_OUT_TO_NO_COVER_RATE * DtoR, fwTimer::GetTimeStep());
			relativeRollToApply = Lerp(interpValue, relativeRollToApply, m_RelativePeekRoll);
		}
	}

	const bool isPlayerInCover = (bInCover || m_WasInCover || attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover));
	if( m_SprintBreakOutTriggered &&
		(m_bUseAnimatedFirstPersonCamera || m_bWasUsingAnimatedFirstPersonCamera || m_AnimatedCameraBlockTag ||
		 bUseHeadSpineBoneOrientation || bCustomSituation || m_HintHelper != NULL || isPlayerInCover ||
		 m_TaskFallFalling || m_TaskFallLandingRoll || m_TaskControllingMovement) )
	{
		m_SprintBreakOutTriggered = false;
		m_SprintBreakOutFinished  = false;
	}

	bool bUsingKeyboardAndMouse = false;
#if KEYBOARD_MOUSE_SUPPORT
	// B*2275203: Don't rotate camera in direction we are pushing if using mouse/keyboard as this interferes with player trying to strafe. 
	bUsingKeyboardAndMouse = attachPed->GetControlFromPlayer() && attachPed->GetControlFromPlayer()->UseRelativeLook();
#endif	// KEYBOARD_MOUSE_SUPPORT

	if(m_SprintBreakOutTriggered && !m_SprintBreakOutFinished && !bUsingKeyboardAndMouse)
	{
		const camFirstPersonShooterCameraMetadataSprintBreakOutSettings& sprintBreakOutSettings = m_Metadata.m_SprintBreakOutSettingsList[m_SprintBreakOutType];
		const float fSprintBreakOutHeadingAngleMin = sprintBreakOutSettings.m_SprintBreakOutHeadingAngleMin*DtoR;

		float fHeadingDiff = Abs(SubtractAngleShorter(heading, m_SprintBreakOutHeading));
		float fNormalisedHeadingDiff = (fHeadingDiff - fSprintBreakOutHeadingAngleMin) / (PI - fSprintBreakOutHeadingAngleMin);
		float fHeadingRate = Lerp(fNormalisedHeadingDiff, sprintBreakOutSettings.m_SprintBreakOutHeadingRateMin, sprintBreakOutSettings.m_SprintBreakOutHeadingRateMax);
		heading = fwAngle::Lerp(heading, m_SprintBreakOutHeading, fHeadingRate * 30.0f * fwTimer::GetCamTimeStep());
	}

	if( !bUseHeadSpineBoneOrientation && !m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera )
	{
		// These will be updated after head/spine average or animated camera runs below.
		// Don't overwrite them so they will have the previous values in those cases.
		m_CameraHeading = heading;
		m_CameraPitch   = pitch;
	}

	UpdateRelativeIdleCamera();

	// Add any custom input we applied, so we don't pop when blending out.
	heading = fwAngle::LimitRadianAngleSafe(heading + m_RelativeHeading + m_IdleRelativeHeading);
	pitch   = fwAngle::LimitRadianAngleSafe(pitch   + m_RelativePitch   + m_IdleRelativePitch);

	if(m_bWasParachuting || m_bWasDiving)
		pitch = Clamp(pitch, -89.0f * DtoR, m_MaxPitch);

	if(!m_bWasUsingAnimatedFirstPersonCamera)
	{
		m_ForceLevelPitchOnAnimatedCamExit = false;
		m_AnimatedBlendOutPhaseStartLevelPitch = -1.0f;
	}

	if( !m_IsLockedOn &&
		((m_bWasUsingAnimatedFirstPersonCamera && !m_bUseAnimatedFirstPersonCamera) || (m_bUseAnimatedFirstPersonCamera && m_AnimatedCameraBlendLevel >= 1.0f)) && 
		(m_CombatRolling || m_WasCombatRolling) )
	{
		// HACK: force camera to be level so animated camera blends to level.
		if(lookAroundPitchOffset == 0.0f)
		{
			pitch = (bPitchForSlopeSet) ? fPitchForSlope : 0.0f;
		}
	}
	else if(m_bWasUsingAnimatedFirstPersonCamera && !m_bUseAnimatedFirstPersonCamera && !m_IsLockedOn && m_ForceLevelPitchOnAnimatedCamExit)
	{
		// HACK: force camera to be level so animated camera blends to level.
		// When blending out, m_AnimatedCameraBlendLevel goes from 1.0f to 0.0f.
		if(m_AnimatedBlendOutPhaseStartLevelPitch < (1.0f - SMALL_FLOAT))
			m_AnimatedBlendOutPhaseStartLevelPitch = m_AnimatedCameraBlendLevel;
		if(lookAroundPitchOffset == 0.0f)
		{
			float desiredPitch = (bPitchForSlopeSet) ? fPitchForSlope : 0.0f;
			pitch = RampValueSafe(m_AnimatedCameraBlendLevel, 0.0f, m_AnimatedBlendOutPhaseStartLevelPitch, desiredPitch, pitch);
		}
	}

	TUNE_GROUP_FLOAT( LADDER_DEBUG, fFPSLadderPitchDOF, 70.0f, 0.0f, 90.0f, 0.1f );
	if( attachPed && attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsClimbingLadder) )
		pitch = Clamp(pitch, -fFPSLadderPitchDOF * DtoR, fFPSLadderPitchDOF * DtoR);

	// B*2055894: Update the camera roll if we're in an aircraft.
	if (!m_RunningAnimTask && relativeRollToApply == 0.0f && attachPed->GetIsInVehicle() && attachPed->GetVehiclePedInside()->GetIsAircraft())
	{
		Matrix34 matRootBone = MAT34V_TO_MATRIX34(attachPed->GetMatrix());
		relativeRollToApply = camFrame::ComputeRollFromMatrix(matRootBone);
		if(!m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera)
		{
			// If not running an animated camera, use spring to smooth changes in roll.
			relativeRollToApply = m_RollSpring.UpdateAngular(relativeRollToApply, 100.0f, 0.90f);
		}
	}

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, relativeRollToApply);

	if(bPlayerUpsidedown)
	{
		Matrix34 worldMatrix = m_Frame.GetWorldMatrix();
		worldMatrix.RotateLocalY(PI);
		m_Frame.SetWorldMatrix(worldMatrix, true);
	}

	if( bUseHeadSpineBoneOrientation && attachPed && !m_IsLockedOn )
	{
		HeadSpineOrientationParams oHeadSpineParams;
		previousCameraMatrix.ToQuaternion( oHeadSpineParams.qPreviousCamera );
		oHeadSpineParams.lookAroundHeadingOffset		= lookAroundHeadingOffset;
		oHeadSpineParams.lookAroundPitchOffset			= lookAroundPitchOffset;
		oHeadSpineParams.bUseHeadSpineBoneOrientation	= bUseHeadSpineBoneOrientation;
		oHeadSpineParams.bUsedHeadSpineBoneLastUpdate	= bUsedHeadSpineBoneOrientationLastUpdate;
		oHeadSpineParams.bIsFalling						= bIsFalling;

		// Override camera orientation to use mix of head and spine3 orientation, blending in as required.
		UseHeadSpineBoneOrientation( attachPed, oHeadSpineParams, bAnimatedCameraChangesStickHeading );
				
		UseGroundAvoidance( *attachPed );
		m_CameraHeading = CalculateHeading(m_Frame.GetFront(), m_Frame.GetUp());
		m_CameraPitch   = m_Frame.ComputePitch();
		// Set flag to determine when to blend out of head/spine orientation override.
		m_bWasUsingHeadSpineOrientation = bUseHeadSpineBoneOrientation;
		m_RelativePitchOffset = 0.0f;
	}
	else
	{
		if(m_GroundAvoidCurrentAngle != 0.0f)
		{
			// To be safe, make sure ground avoid angle blends out.
			Approach( m_GroundAvoidCurrentAngle, 0.0f, m_Metadata.m_AvoidBlendOutRate, fwTimer::GetCamTimeStep() );
		}

		m_RelativePitchOffset = 0.0f;
		m_bWasFalling = false;
		m_bWasUsingRagdollForHeadSpine = false;
	}

	m_WasInCover = bInCover;
	
	INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS, fGroundAvoidBlendOutAngle, 0.0f, PI, 0.05f);
	if( (m_bUseAnimatedFirstPersonCamera && Abs(m_GroundAvoidCurrentAngle) < fGroundAvoidBlendOutAngle) || m_bWasUsingAnimatedFirstPersonCamera )
	{
		// Camera runs normally, even using head/spine orientation, then animate camera overrides here, if active.
		// Also, handles blend out.
		const bool c_AccumulatedHeadingAppliedToPedHeading = (attachPed && !attachPed->GetUsingRagdoll()) && (m_CombatRolling || (!m_RunningAnimTask && !m_ActualAnimatedYawSpringInput));
		float fAnimatedHeading = 0.0f;
		float fAnimatedPitch = 0.0f;
		UseAnimatedCameraData(	attachPed, previousCameraMatrix, lookAroundHeadingOffset, lookAroundPitchOffset, fAnimatedHeading, 
								fAnimatedPitch, lockonHeading, lockonPitch, c_AccumulatedHeadingAppliedToPedHeading );
		m_Frame.ComputeHeadingAndPitch(m_CameraHeading, m_CameraPitch);

		if( (!c_AccumulatedHeadingAppliedToPedHeading /*|| (m_ActualAnimatedYawSpringInputChanged && !m_ActualAnimatedYawSpringInput)*/)&&
			(m_AnimatedCameraHeadingAccumulator != 0.0f || m_AnimatedCameraPitchAccumulator != 0.0f) )
		{
			if( m_CameraRelativeAnimatedCameraInput || c_AccumulatedHeadingAppliedToPedHeading )
			{
				Matrix34 worldMatrix = m_Frame.GetWorldMatrix();
				worldMatrix.RotateLocalZ( m_AnimatedCameraHeadingAccumulator );
				worldMatrix.RotateLocalX( m_AnimatedCameraPitchAccumulator );

				m_Frame.SetWorldMatrix(worldMatrix, true);
			}
			else
			{
				float heading, pitch, roll;
				m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

				heading = fwAngle::LimitRadianAngleSafe(heading + m_AnimatedCameraHeadingAccumulator);
				pitch   = fwAngle::LimitRadianAngleSafe(pitch   + m_AnimatedCameraPitchAccumulator);

				m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
			}
		}

		if(!m_ActualAnimatedPitchSpringInput && m_bUseAnimatedFirstPersonCamera && m_AnimatedCameraBlendLevel == 1.0f)
		{
			float fAnimatedRelativePitch = m_CameraPitch - fAnimatedPitch;
			float fClampedPitch = Clamp(m_CameraPitch - fAnimatedPitch, m_AnimatedCamSpringPitchLimitsRadians.x*DtoR, m_AnimatedCamSpringPitchLimitsRadians.y*DtoR);
			if(fClampedPitch != fAnimatedRelativePitch)
			{
				float fDifference = fClampedPitch - fAnimatedRelativePitch;
				Matrix34 worldMatrix = m_Frame.GetWorldMatrix();
				worldMatrix.RotateLocalX( fDifference );
				m_Frame.SetWorldMatrix(worldMatrix, true);
				m_CameraPitch = fClampedPitch;
			}
		}

		// Still need this as ped heading is not updated or not updated quickly enough.
		if( c_AccumulatedHeadingAppliedToPedHeading )
		{
			// HACK: even though I see SetDesiredHeading called in CTaskAimGunOnFoot::PostCameraUpdate, ped is not turning so forcing it here for now.
			if( m_AnimatedCameraHeadingAccumulator != 0.0f )
			{
				attachPed->SetHeading( fwAngle::LimitRadianAngle(attachPed->GetCurrentHeading() + m_AnimatedCameraHeadingAccumulator) );
			}
			else if( m_IsLockedOn && lockonHeading <= (PI + SMALL_FLOAT) )
			{
				attachPed->SetHeading( fwAngle::LimitRadianAngle(lockonHeading) );
			}

			// Reset.  If we are rotating the character to the camera heading, then we only need to rotate by the current frame's amount.
			m_AnimatedCameraHeadingAccumulator = 0.0f;
			m_AnimatedCameraPitchAccumulator = 0.0f;
		}

		if(!m_ActualAnimatedYawSpringInput)
		{
			m_SavedHeading = fwAngle::LimitRadianAngle(m_SavedHeading + m_AnimatedCameraHeadingAccumulator);
			m_CameraHeading = m_SavedHeading;
			if(c_AccumulatedHeadingAppliedToPedHeading)
			{
				m_AnimatedCameraHeadingAccumulator = 0.0f;
			}
		}
		if(!m_ActualAnimatedPitchSpringInput)
		{
			m_SavedPitch   = fwAngle::LimitRadianAngle(m_SavedPitch   + m_AnimatedCameraPitchAccumulator);
			m_SavedPitch   = Clamp(m_SavedPitch, m_MinPitch, m_MaxPitch);
			m_CameraPitch   = m_SavedPitch;
		}
	}
	else
	{
		m_AnimatedCameraBlendLevel = 0.0f;		// Reset
	}

	if( !bAnimatedCameraChangesStickHeading && (m_bUseAnimatedFirstPersonCamera || bUseHeadSpineBoneOrientation) )
	{
		m_CameraHeading = m_SavedHeading;
		m_CameraPitch   = m_SavedPitch;
	}
	if(!m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera && (m_StopTimeOfProneMeleeAttackLook == 0 || m_StopTimeOfProneMeleeAttackLook < fwTimer::GetTimeInMilliseconds()))
	{
		// Make sure melee relative heading/pitch blends out when not running an animated camera.
		const CPed* pTemp = NULL;
		AnimatedLookAtMeleeTarget(attachPed, pTemp, 0.0f, 0.0f);
	}

	// Apply scope offset to camera position, after the orientation has been set.
	Vector3 vPositionOffset = m_CurrentScopeOffset.GetY() * m_Frame.GetFront();
	m_Frame.SetPosition( m_Frame.GetPosition() + vPositionOffset );

	// If the ground avoidance logic has kicked in then adjust the near clip plane to avoid clipping
	INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS, fGroundAvoidMaxAngle, 0.0f, PI, 0.05f);
	INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS, fGroundAvoidClipMaxAngle, 0.0f, PI, 0.05f);
	float fNearClip = RampValueSafe(Abs(m_GroundAvoidCurrentAngle), 0.0f, fGroundAvoidClipMaxAngle, m_Metadata.m_BaseNearClip, m_Metadata.m_MinNearClip);

#if CAM_MODIFY_NEAR_CLIP_WHEN_LOOKING_DOWN
	const float fPitchLimit = m_MinPitch * 0.45f;								// TODO: metadata?
	if( m_CameraPitch < fPitchLimit )
	{
		const u8 c_MaxTests = 2;
		u32 includeFlagsForLineTest = ArchetypeFlags::GTA_ALL_MAP_TYPES;

		WorldProbe::CShapeTestProbeDesc lineTest;
		WorldProbe::CShapeTestFixedResults<c_MaxTests> shapeTestResults;
		lineTest.SetResultsStructure(&shapeTestResults);
		lineTest.SetIsDirected(false);
		lineTest.SetIncludeFlags(includeFlagsForLineTest);
		lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		lineTest.SetContext(WorldProbe::LOS_Camera);
		lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		Vector3 startPosition = m_Frame.GetPosition();
		Vector3 endPosition   = startPosition + m_Frame.GetFront() * 2.00f;		// Length of probe is ped height.
		lineTest.SetStartAndEnd(startPosition, endPosition);

		bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		if( hasHit )
		{
			fNearClip = Min(fNearClip, RampValueSafe(pitch, fPitchLimit, m_MinPitch, m_Metadata.m_BaseNearClip, m_Metadata.m_MinNearClip));
		}
	}
#endif // CAM_MODIFY_NEAR_CLIP_WHEN_LOOKING_DOWN

	if( fNearClip < m_Metadata.m_BaseNearClip )
	{
		// We are looking down and ground is close.
		m_Frame.GetFlags().SetFlag(camFrame::Flag_BypassNearClipScanner);
		SetNearClipThisUpdate( fNearClip );
	}

	if(attachPed && bInCover && !attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
	{
		m_CoverVaultStartHeading = m_CameraHeading;
	}
	else if( !bInCover && (!attachPed || !attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting)) )
	{
		m_CoverVaultStartHeading = FLT_MAX;
		m_CoverVaultWasFacingLeft  = false;
		m_CoverVaultWasFacingRight = false;
	}

	m_PreviousRelativeHeading = m_RelativeHeading;
	m_PreviousRelativePitch   = m_RelativePitch;

	static const u32 luckyWheelCameraName = ATSTRINGHASH("CASINO_LUCKY_WHEEL_CAMERA", 0x59E53D);
	const u32 tableGamesCameraName = camInterface::GetGameplayDirector().GetTableGamesCameraName();
	const bool isLuckyWheelCamera = (tableGamesCameraName == luckyWheelCameraName);
	if (isTableGamesCamera && !isLuckyWheelCamera)
	{
		//special case for the table games camera, except for the lucky wheel minigame.
		float finalHeading, finalPitch, finalRoll;
		m_Frame.ComputeHeadingPitchAndRoll(finalHeading, finalPitch, finalRoll);
		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(finalHeading, finalPitch, 0.0f);
	}
}

void camFirstPersonShooterCamera::UpdateRelativeIdleCamera()
{
	bool bReset = m_bLookingBehind;
	if(m_IsIdleCameraActive)
	{
		m_IdleRelativeHeading = fwAngle::LimitRadianAngle(m_IdleHeading - m_CameraHeading);
		m_IdleRelativePitch   = fwAngle::LimitRadianAngle(m_IdlePitch   - m_CameraPitch);
		m_IdleRelativeHeadingSpring.Reset(m_IdleRelativeHeading);
		m_IdleRelativePitchSpring.Reset(m_IdleRelativePitch);

		m_bIgnoreHeadingLimits = true;
	}
	else if(Abs(m_IdleRelativeHeading) > SMALL_FLOAT || Abs(m_IdleRelativePitch) > SMALL_FLOAT)
	{
		const camFirstPersonShooterCameraMetadataOrientationSpring &orientationSpring = GetCurrentOrientationSpring();
		m_IdleRelativeHeading = m_IdleRelativeHeadingSpring.Update(0.0f, orientationSpring.m_LookBehindSpringConstant, orientationSpring.m_LookBehindSpringDampingRatio);
		m_IdleRelativePitch   = m_IdleRelativePitchSpring.Update(0.0f, orientationSpring.m_LookBehindSpringConstant, orientationSpring.m_LookBehindSpringDampingRatio);
	}
	else
	{
		bReset = true;
	}

	if(bReset)
	{
		m_IdleRelativeHeading = 0.0f;
		m_IdleRelativePitch   = 0.0f;
		m_IdleRelativeHeadingSpring.Reset(m_IdleRelativeHeading);
		m_IdleRelativePitchSpring.Reset(m_IdleRelativePitch);
	}
}

void camFirstPersonShooterCamera::ReorientCameraToPreviousAimDirection()
{
	// UpdateOrientation is called after UpdatePosition and UpdateZoom.
	// so, at this point, the camera position and m_IsAiming are valid.
	if( m_NumUpdatesPerformed == 0 &&
		m_AttachParent.Get() && m_AttachParent->GetIsTypePed() &&
		////(m_IsAiming || camInterface::GetGameplayDirector().GetJustSwitchedFromSniperScope()) &&
		!camInterface::GetGameplayDirector().GetPreviousAimCameraPosition().IsClose(g_InvalidPosition, SMALL_FLOAT) )
	{
		const u8 c_MaxIntersections = 1;
		WorldProbe::CShapeTestProbeDesc lineTest;
		WorldProbe::CShapeTestFixedResults<c_MaxIntersections> shapeTestResults;
		lineTest.SetResultsStructure(&shapeTestResults);
		lineTest.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
		lineTest.SetContext(WorldProbe::LOS_Camera);
		lineTest.SetIsDirected(true);
		const u32 collisionIncludeFlags =	(u32)ArchetypeFlags::GTA_ALL_TYPES_WEAPON |
											(u32)ArchetypeFlags::GTA_RAGDOLL_TYPE |
											(u32)ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
		lineTest.SetIncludeFlags(collisionIncludeFlags);
		lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		lineTest.SetTreatPolyhedralBoundsAsPrimitives(false);

		const Vector3 playerPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		const Vector3 previousCameraPosToPlayer = playerPosition - camInterface::GetGameplayDirector().GetPreviousAimCameraPosition();
		float fDotProjection = Max(previousCameraPosToPlayer.Dot(camInterface::GetGameplayDirector().GetPreviousAimCameraForward()), 0.0f);
		const Vector3 vProbeStartPosition = camInterface::GetGameplayDirector().GetPreviousAimCameraPosition() + camInterface::GetGameplayDirector().GetPreviousAimCameraForward()*fDotProjection;

		const float maxProbeDistance = 50.0f;
		Vector3 vTargetPoint = vProbeStartPosition+camInterface::GetGameplayDirector().GetPreviousAimCameraForward()*maxProbeDistance;
		lineTest.SetStartAndEnd(vProbeStartPosition, vTargetPoint);
		lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

		DEV_ONLY( CEntity* pHitEntity = NULL; )
		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		if( hasHit )
		{
			vTargetPoint = shapeTestResults[0].GetHitPosition();
		#if __DEV
			pHitEntity = shapeTestResults[0].GetHitEntity();
			if(pHitEntity)
			{
			}
		#endif
		}

		Vector3 vNewForward  = vTargetPoint - m_Frame.GetPosition();
		vNewForward.NormalizeSafe();

		m_Frame.SetWorldMatrixFromFront( vNewForward );
	}
}

bool camFirstPersonShooterCamera::IsTableGamesCamera(const camBaseCamera* camera /*= nullptr*/) const
{
	static const u32 fpTableGamesCameraName = ATSTRINGHASH("CASINO_TABLE_GAMES_FPS_CAMERA", 0x2DFF9A0);
	return ((camera ? camera->GetNameHash() :  GetNameHash()) == fpTableGamesCameraName);
}

const camEnvelope* camFirstPersonShooterCamera::GetTableGamesHintEnvelope() const
{
	const bool runningTableGamesHint = IsTableGamesCamera() && camInterface::GetGameplayDirector().IsHintActive();
	if (!runningTableGamesHint)
	{
		return NULL;
	}

	const camEnvelope* hintEnvelope = camInterface::GetGameplayDirector().GetHintEnvelope();
	return hintEnvelope;
}

bool camFirstPersonShooterCamera::IsTableGamesOrIsInterpolatingToTableGamesCamera() const
{
	if (IsTableGamesCamera())
	{
		return true;
	}

	const camFirstPersonShooterCamera* activeFPSCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if (activeFPSCamera != this && IsTableGamesCamera(activeFPSCamera))
	{
		return true;
	}

	return false;
}

void camFirstPersonShooterCamera::UpdateBlendedAnimatedFov()
{
	if(!IsTableGamesCamera())
	{
		//only do this on table games camera, this because during the blending we update two FPS cameras and we want this
		//to happen only on the CASINO camera.
		return;
	}

	TUNE_GROUP_INT(TABLE_GAMES_CAMERAS, iAnimatedFovBlendInTime, 500, 0, 99999, 1);
	TUNE_GROUP_INT(TABLE_GAMES_CAMERAS, iAnimatedFovBlendOutTime, 500, 0, 99999, 1);

	const bool forceAnimatingFovThisUpdate = m_NumUpdatesPerformed == 0 && m_AnimatedFov > SMALL_FLOAT;
	const bool hasStartedAnimatingFovThisFrame = m_AnimatedFovOnPreviousFrame < SMALL_FLOAT && m_AnimatedFov > SMALL_FLOAT;
	const bool hasStoppedAnimatingFovThisUpdate = m_AnimatedFovOnPreviousFrame > SMALL_FLOAT && m_AnimatedFov < SMALL_FLOAT;

	const u32 currentTime = fwTimer::GetTimeInMilliseconds();

	if (forceAnimatingFovThisUpdate)
	{
		m_AnimatedFovBlendInStartTime = currentTime - iAnimatedFovBlendInTime;
		m_AnimatedFovBlendOutStartTime = 0;
	}
	else if (hasStartedAnimatingFovThisFrame)
	{
		m_AnimatedFovBlendInStartTime = currentTime;
		m_AnimatedFovBlendOutStartTime = 0;
	}
	else if (hasStoppedAnimatingFovThisUpdate)
	{
		m_AnimatedFovForBlending = m_BlendedAnimatedFov; //previous blending value as starting point of the blendout.
		m_AnimatedFovBlendOutStartTime = currentTime;
		m_AnimatedFovBlendInStartTime = 0;
	}

	//only apply changes if we need to.
	if (m_AnimatedFovBlendInStartTime != 0 || m_AnimatedFovBlendOutStartTime != 0)
	{
		float interpolationT = 0.0f;

		if (m_AnimatedFovBlendInStartTime != 0)
		{
			m_AnimatedFovForBlending = Max(m_AnimatedFovForBlending, m_AnimatedFov);
			const u32 deltaTimeMS = currentTime - m_AnimatedFovBlendInStartTime;
			interpolationT = RampValue((float)deltaTimeMS, 0.0f, (float)iAnimatedFovBlendInTime, 0.0f, 1.0f);
		}
		else if (m_AnimatedFovBlendOutStartTime != 0)
		{
			const u32 deltaTimeMS = currentTime - m_AnimatedFovBlendOutStartTime;
			interpolationT = RampValue((float)deltaTimeMS, 0.0f, (float)iAnimatedFovBlendOutTime, 1.0f, 0.0f);

			if (deltaTimeMS > iAnimatedFovBlendOutTime)
			{
				m_AnimatedFovBlendOutStartTime = 0;
			}
		}

		const float newAnimatedFov = Lerp(interpolationT, m_BaseFov, m_AnimatedFovForBlending);

		cameraDebugf1("%d isAnimated %d animWeight %.2f previous %.2f current %.2f target %.2f result %.2f tVal %.2f", 
			fwTimer::GetFrameCount(), 
			m_bUseAnimatedFirstPersonCamera, 
			m_AnimatedWeight, 
			m_AnimatedFovOnPreviousFrame, 
			m_AnimatedFov, 
			m_AnimatedFovForBlending,
			newAnimatedFov, 
			interpolationT);

		m_BlendedAnimatedFov = newAnimatedFov;
	}
	else
	{
		cameraDebugf1("%d isAnimated %d animWeight %.2f previous %.2f current %.2f",
			fwTimer::GetFrameCount(),
			m_bUseAnimatedFirstPersonCamera,
			m_AnimatedWeight,
			m_AnimatedFovOnPreviousFrame,
			m_AnimatedFov);

		m_BlendedAnimatedFov = m_AnimatedFov; //just use the default animated fov.
	}
}

void camFirstPersonShooterCamera::UpdateBaseFov()
{
	const float fFovSlider = ComputeFovSlider();

	TUNE_BOOL(CAM_FIRST_PERSON_HFOV, false);
	if (CAM_FIRST_PERSON_HFOV)
	{
		const float baseHFov = 2.0f * atan(Tanf(0.5f * m_Metadata.m_BaseFov * DtoR) * 16.0f / 9.0f) ;
		const float hFovSetting = Lerp(fFovSlider, baseHFov, 110.0f * DtoR);

		const float sceneWidth = static_cast<float>(VideoResManager::GetSceneWidth());
		const float sceneHeight = static_cast<float>(VideoResManager::GetSceneHeight());
		const float viewportAspect = sceneWidth / sceneHeight;

		const float baseAspect = 16.0f / 9.0f;

		const float hFov2 = atan(Tanf(0.5f * hFovSetting) * (viewportAspect / baseAspect));
		const float vFov = 2.0f * atan(Tanf(hFov2) / viewportAspect) * RtoD;

		m_BaseFov = vFov;
	}
	else
	{
		m_BaseFov = Lerp(fFovSlider, m_Metadata.m_BaseFov, m_Metadata.m_MaxBaseFov);
	}
}

float camFirstPersonShooterCamera::ComputeFovSlider() const
{
	const s32 profileSettingFov = CPauseMenu::GetMenuPreference(PREF_FPS_FIELD_OF_VIEW);
	const float fFovSlider = Clamp((float)profileSettingFov / (float)DEFAULT_SLIDER_MAX, 0.0f, 1.0f);
	return fFovSlider;
}

void camFirstPersonShooterCamera::UpdateAnimCameraBlendLevel(const CPed* currentMeleeTarget)
{
	//If a table games hint has blocked the animated camera, we use its blend durations as overrides for the animated blend in/out durations
	const camEnvelope* tableGamesHintEnvelope	= GetTableGamesHintEnvelope();
	s32 tableGamesHintAnimBlendOutDuration		= -1;
	s32 tableGamesHintAnimBlendInDuration		= -1;
	if (tableGamesHintEnvelope)
	{
		if (tableGamesHintEnvelope->IsInReleasePhase())
		{
			tableGamesHintAnimBlendInDuration	= tableGamesHintEnvelope->GetReleaseDuration();
		}
		else
		{
			tableGamesHintAnimBlendOutDuration	= tableGamesHintEnvelope->GetAttackDuration();
		}
	}

	if (m_AnimBlendInStartTime == 0 && m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera)
	{
		m_AnimBlendInStartTime = fwTimer::GetTimeInMilliseconds();
		m_bWasUsingAnimatedFirstPersonCamera = true;
	}
	else if (m_AnimBlendOutStartTime == 0 && !m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera)
	{
		// This is used to track blend out interpolation.
		m_AnimBlendOutStartTime = fwTimer::GetTimeInMilliseconds();
		m_AnimBlendInStartTime = 0;						// Blendout takes priority over blendin.
	}

	float fInterp = 1.0f;
	TUNE_GROUP_BOOL(CAM_FPS, bAnimatedCameraUseWeightForBlendIn, false);
	TUNE_GROUP_BOOL(CAM_FPS, bAnimatedCameraUseWeightForBlendOut, false);
	TUNE_GROUP_INT(CAM_FPS, nAnimatedCameraBlendInDuration, 250, 0, 10000, 1000);
	TUNE_GROUP_INT(CAM_FPS, nAnimatedCameraMeleeBlendInDuration, 100, 0, 10000, 1000);
	TUNE_GROUP_INT(CAM_FPS, nAnimatedCameraBlendOutDuration, 750, 0, 10000, 1000);
	if (m_AnimBlendInStartTime != 0)
	{
		////m_AnimBlendOutStartTime = 0;					// Blendout takes priority over blendin.
		if (!bAnimatedCameraUseWeightForBlendIn)
		{
			const u32 animatedCameraBlendInDuration = currentMeleeTarget ? (u32)nAnimatedCameraMeleeBlendInDuration : (u32)nAnimatedCameraBlendInDuration;
			const u32 blendDuration = tableGamesHintAnimBlendInDuration > -1 ? tableGamesHintAnimBlendInDuration : (m_AnimBlendInDuration < 0) ? (u32)animatedCameraBlendInDuration : (u32)m_AnimBlendInDuration;

			if (!ShouldKillAnimatedCameraBlendIn() && m_AnimBlendInStartTime + blendDuration > fwTimer::GetTimeInMilliseconds())
			{
				fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_AnimBlendInStartTime) / (float)blendDuration;
				fInterp = SlowInOut(fInterp);
			}
			else
			{
				m_AnimBlendInStartTime = 0;
				m_bAnimatedCameraBlendInComplete = true;
			}
		}
		else
		{
			fInterp = Clamp(SlowInOut(m_AnimatedWeight), 0.0f, 1.0f);
			if (fInterp >= 1.0f)
			{
				m_bAnimatedCameraBlendInComplete = true;
				m_AnimBlendInStartTime = 0;
			}
		}
	}
	else if (m_AnimBlendOutStartTime != 0)
	{
		if (!bAnimatedCameraUseWeightForBlendOut)
		{
			// Blending out.
			const u32 blendDuration = tableGamesHintAnimBlendOutDuration > -1 ? tableGamesHintAnimBlendOutDuration : (m_AnimBlendOutDuration < 0) ? (u32)nAnimatedCameraBlendOutDuration : (u32)m_AnimBlendOutDuration;
			if (m_AnimBlendOutStartTime + blendDuration > fwTimer::GetTimeInMilliseconds())
			{
				fInterp = (float)(m_AnimBlendOutStartTime + blendDuration - fwTimer::GetTimeInMilliseconds()) / (float)blendDuration;
				fInterp = Clamp(SlowInOut(fInterp), 0.0f, 1.0f);
			}
			else
			{
				// Blend out complete.
				m_bWasUsingAnimatedFirstPersonCamera = false;
				m_bAnimatedCameraBlendInComplete = false;
				m_AnimBlendOutStartTime = 0;
				fInterp = 0.0f;
			}
		}
		else
		{
			m_bAnimatedCameraBlendInComplete = false;
			fInterp = Clamp(SlowInOut(m_AnimatedWeight), 0.0f, 1.0f);
			if (fInterp <= 0.0f)
			{
				// Blend out complete.
				m_bWasUsingAnimatedFirstPersonCamera = false;
				m_AnimBlendOutStartTime = 0;
				fInterp = 0.0f;
			}
		}
	}

	m_AnimatedCameraBlendLevel = fInterp;

	PF_SET(AnimInterp, m_AnimatedCameraBlendLevel);
	PF_SET(AnimEnabled, m_bUseAnimatedFirstPersonCamera);
	PF_SET(AnimWasEnabled, m_bWasUsingAnimatedFirstPersonCamera);
}

bool camFirstPersonShooterCamera::ShouldKillAnimatedCameraBlendIn() const
{
	bool shouldKillBlendin	 = m_NumUpdatesPerformed == 0;
	shouldKillBlendin		&= !IsInterpolating();
	shouldKillBlendin		|= m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);

	return shouldKillBlendin;
}

bool camFirstPersonShooterCamera::ComputeDesiredPitchForSlope(const CPed* attachedPed, float& fTargetPitch)
{
	if( attachedPed )
	{
		const float c_SlopePitchOffset = m_Metadata.m_SlopePitchOffset * DtoR;
		const float c_MinSpeed = m_Metadata.m_SlopePitchMinSpeed;
		const float c_MinSlope = Cosf( m_Metadata.m_SlopePitchMinSlope*DtoR );
		const float c_MaxSlope = Cosf( m_Metadata.m_SlopePitchMaxSlope*DtoR );

		if( m_GroundNormal.z <= c_MinSlope && m_GroundNormal.z >= c_MaxSlope)
		{
			const bool bIsLookingBehind = m_CurrentControlHelper && m_CurrentControlHelper->IsLookingBehind();
			Vector3 vPedVelocity = attachedPed->GetVelocity();
			if( vPedVelocity.Mag2() >= c_MinSpeed*c_MinSpeed || bIsLookingBehind)
			{
				// TODO: ramp pitch change rate with speed.

				// Calculate target pitch, based on ground normal.
				Vector3 vPedForward = VEC3V_TO_VECTOR3( attachedPed->GetTransform().GetForward() );
				if( bIsLookingBehind )
				{
					camFrame::ComputeFrontFromHeadingAndPitch(m_CameraHeading, 0.0f, vPedForward);
				}

				// Resolve ped forward against ground normal to get component perpendicular to the ground.
				float fDot = m_GroundNormal.Dot( vPedForward );
				Vector3 vParallel = m_GroundNormal * fDot;
				Vector3 vPerpendicular = vPedForward - vParallel;

				float fSlopeOffset = 0.0f;
				fTargetPitch = rage::AsinfSafe( vPerpendicular.z );
				if( c_SlopePitchOffset > 0.0f )//&& !m_SlopeScrambling )
				{
					const float c_MinAngle = 7.0f * DtoR;
					const float c_MaxAngle = 20.0f * DtoR;
					if(fTargetPitch > c_MinAngle)
						fSlopeOffset = -RampValueSafe(fTargetPitch,  c_MinAngle,  c_MaxAngle, 0.0f, c_SlopePitchOffset);
					else if(fTargetPitch < -c_MinAngle)
						fSlopeOffset =  RampValueSafe(fTargetPitch, -c_MaxAngle, -c_MinAngle, c_SlopePitchOffset, 0.0f);
				}
				fTargetPitch += fSlopeOffset;

				return (!m_bLookingBehind && !m_bIgnoreHeadingLimits);
			}
		}
	}

	return false;
}

bool camFirstPersonShooterCamera::ComputeUseHeadSpineBoneOrientation(const CPed* attachPed, bool bIsFalling)
{
	m_LookAroundDuringHeadSpineOrient = false;
	m_LookDownDuringHeadSpineOrient = false;

	if( attachPed )
	{
	#if __DEV
		TUNE_GROUP_BOOL(CAM_FPS, c_ForceUseHeadSpineOrientation, false);
		if(c_ForceUseHeadSpineOrientation)
			return true;
	#endif // __DFEV

		if( attachPed->GetUsingRagdoll() )
		{
			return true;
		}

		if( m_PlayerGettingUp )
		{
			return true;
		}

		if(m_IsLockedOn || (m_LockOnEnvelopeLevel > 0.0f) || m_HasStickyTarget || (m_StickyAimEnvelopeLevel > 0.0f))
		{
			return false;
		}
		
		if( attachPed->GetPedIntelligence() )
		{
			if( attachPed->IsExitingVehicle() )
			{
				if( attachPed->GetPedResetFlag(CPED_RESET_FLAG_JumpingOutOfVehicle) )
				{
					return true;
				}
				else if( m_pVehicle && m_pVehicle->IsInAir() )
				{
					// If jumping out of aircraft and wearing a parachute, CPED_RESET_FLAG_JumpingOutOfVehicle is not set.
					return true;
				}
			}

			// Are we in a high fall?
			if( m_bHighFalling )
				return true;

			////const CTaskJump* pTaskJump = (const CTaskJump*)( attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP) );
			////if( pTaskJump && pTaskJump->GetState() == CTaskJump::State_Jump )
			////if( attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) )
			////{
			////	m_LookAroundDuringHeadSpineOrient = true;
			////	return true;
			////}
			////else
			if( bIsFalling && !attachPed->GetIsParachuting() && !attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDiving))
			{
				m_LookAroundDuringHeadSpineOrient = true;
				return true;
			}

			if( (m_JackedFromVehicle || attachPed->GetPedResetFlag(CPED_RESET_FLAG_BeingJacked)) &&
				(!m_AllowLookAtVehicleJacker || !attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)) )
			{
				return true;
			}

			////if(m_IsCarJacking)
			////{
			////	return true;
			////}

			if( m_AnimTaskUseHeadSpine )
			{
				m_LookAroundDuringHeadSpineOrient = true;
				return true;
			}
		}
	}

	return false;
}

void camFirstPersonShooterCamera::UseAnimatedCameraData(CPed* attachPed, const Matrix34& previousCameraMatrix, float lookAroundHeadingOffset, float lookAroundPitchOffset,
														float& animatedHeading, float& animatedPitch, float lockonHeading, float lockonPitch,
														bool bAccumulatedHeadingAppliedToPedHeading)
{
	Matrix34 mtxTarget;
	m_LookAroundDuringHeadSpineOrient = true;
	m_LookDownDuringHeadSpineOrient = true;

	if(attachPed == NULL)
	{
		return;
	}

	m_WasRunningAnimTaskWithAnimatedCamera |= m_RunningAnimTask;

	const bool c_IsPlayerVaulting = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
	const float fCoverVaultHeadingRelativeTo = (c_IsPlayerVaulting && m_CoverVaultStartHeading < FLT_MAX) ? m_CoverVaultStartHeading : m_CameraHeading;

	// When no animated data is read, continue building the camera matrix and setting fov as the data was saved/restored in UpdateAnimatedData.
	// If the animated data is no longer coming through, then we are re-creating the last matrix.
	Mat34V mPed, mCam;
	if( !camInterface::GetGameplayDirector().IsFirstPersonAnimatedDataRelativeToMover() )
	{
		int rootIdx = 0;
		attachPed->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_ROOT, rootIdx);
		crSkeleton* pSkeleton = attachPed->GetSkeleton();
		pSkeleton->GetGlobalMtx(rootIdx, mPed);
	}
	else
	{
		mPed = attachPed->GetTransform().GetMatrix();
	}

	Mat34VFromQuatV(mCam, m_AnimatedOrientation);
	Transform(mCam, mPed, mCam);

	mtxTarget = MAT34V_TO_MATRIX34( mCam );

	// We are re-doing the position, since the animated camera has a different offset, re-apply the offset to the head bone
	Matrix34 targetMatrix = MAT34V_TO_MATRIX34(attachPed->GetMatrix());
	Vector3 relativeAttachPosition = m_PreviousRelativeAttachPosition;
	TransformAttachPositionToWorldSpace(targetMatrix, relativeAttachPosition, m_AttachPosition);
	m_AttachPosition.z += m_ShoesOffset;
	m_AttachPosition.z += m_AttachPedPelvisOffsetSpring.GetResult();

	// Add in the offset, from the animation data, relative to head bone. (already set as camera position)
	Vector3 vRelativeAnimatedOffset = VEC3V_TO_VECTOR3(m_AnimatedRelativeAttachOffset);
	Vector3 animatedOffset;
	if( camInterface::GetGameplayDirector().IsApplyRelativeOffsetInCameraSpace() )
	{
		TransformAttachPositionToCameraSpace(vRelativeAnimatedOffset, animatedOffset, true, &mtxTarget);
	}
	else
	{
		MAT34V_TO_MATRIX34(mPed).Transform3x3(vRelativeAnimatedOffset, animatedOffset);
	}
	m_AttachPosition += animatedOffset;

	mtxTarget.d = m_AttachPosition;

	const u32 c_uCurrentTimeMsec = fwTimer::GetTimeInMilliseconds();
	const CPed* currentMeleeTarget = (m_MeleeAttack || m_WasDoingMelee || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec) ? m_pMeleeTarget.Get() : NULL;
	if(m_LockOnTarget && currentMeleeTarget != m_LockOnTarget && m_LockOnTarget->GetIsTypePed())
	{
		// To prevent fighting, give priority to lock on target, rather than melee target.
		const CPed* previousTarget = currentMeleeTarget;
		currentMeleeTarget = static_cast<const CPed*>(m_LockOnTarget.Get());
		if(currentMeleeTarget->GetIsDeadOrDying())
		{
			currentMeleeTarget = previousTarget;
		}
	}

	Vector2 springHeadingLimitsRadians, springPitchLimitsRadians;
	GetHeadingPitchSpringLimits(springHeadingLimitsRadians, springPitchLimitsRadians);

	float animRoll;
	camFrame::ComputeHeadingPitchAndRollFromMatrix(mtxTarget, animatedHeading, animatedPitch, animRoll);
	AnimatedLookAtMeleeTarget(attachPed, currentMeleeTarget, animatedHeading, animatedPitch);
	// TODO: clean up later, for now want to retain the ability to compare the two methods. (in debug)
	TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, c_bMeleeRelativeToLocalOrientation, true);
	if(!c_bMeleeRelativeToLocalOrientation)
	{
		if( Abs(m_MeleeRelativeHeading) > SMALL_FLOAT ||
			Abs(m_MeleeRelativePitch)   > SMALL_FLOAT )
		{
			// Melee heading/pitch is applied relative to world.
			animatedHeading = fwAngle::LimitRadianAngleSafe(animatedHeading + m_MeleeRelativeHeading);
			animatedPitch   = fwAngle::LimitRadianAngleSafe(animatedPitch   + m_MeleeRelativePitch);

			camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(animatedHeading, animatedPitch, animRoll, mtxTarget);
		}
	}
	else
	{
		if( Abs(m_MeleeRelativeHeading) > SMALL_FLOAT )
		{
			mtxTarget.RotateLocalZ( m_MeleeRelativeHeading );
		}
		if( Abs(m_MeleeRelativePitch)   > SMALL_FLOAT )
		{
			mtxTarget.RotateLocalX( m_MeleeRelativePitch );
		}
	}

	const bool c_WasInCoverForVault = (m_CoverVaultWasFacingLeft || m_CoverVaultWasFacingRight);
	if(c_IsPlayerVaulting && c_WasInCoverForVault)
	{
		// If the animated camera is "behind" where camera was facing when vault started, we undo the rotation.
		// (Basically, we keep rotating where the animated camera is, until it is past the initial vault camera heading)
		float currentAnimatedHeading = camFrame::ComputeHeadingFromMatrix(mtxTarget);
		float relativeHeadingToVaultStart = fwAngle::LimitRadianAngle(currentAnimatedHeading - fCoverVaultHeadingRelativeTo);
		if( (m_CoverVaultWasFacingLeft && relativeHeadingToVaultStart > 0.0f) ||
			(m_CoverVaultWasFacingRight && relativeHeadingToVaultStart < 0.0f) )
		{
			mtxTarget.RotateZ(-relativeHeadingToVaultStart);
		}

		// Also, since camera was updated, have to be sure current matrix is not "behind" where the camera was facing
		// when vault started.  Needed as animated camera blends from current camera matrix.
		Matrix34 currentWorldMatrix = m_Frame.GetWorldMatrix();
		currentAnimatedHeading = camFrame::ComputeHeadingFromMatrix(currentWorldMatrix);
		relativeHeadingToVaultStart = fwAngle::LimitRadianAngle(currentAnimatedHeading - fCoverVaultHeadingRelativeTo);
		if( (m_CoverVaultWasFacingLeft && relativeHeadingToVaultStart > 0.0f) ||
			(m_CoverVaultWasFacingRight && relativeHeadingToVaultStart < 0.0f) )
		{
			currentWorldMatrix.RotateZ(-relativeHeadingToVaultStart);
			m_Frame.SetWorldMatrix(currentWorldMatrix);
		}
	}
	Matrix34 mtxFinalAnimated;
	mtxFinalAnimated = mtxTarget;

	// Reset when animated camera starts.
	if(m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera)
	{
		// Save the animated camera's starting heading/pitch without being turned for ground collision.
		camFrame::ComputeHeadingAndPitchFromMatrix(mtxTarget, m_SavedHeading, m_SavedPitch);
	}

	UpdateAnimCameraBlendLevel(currentMeleeTarget);

	if( m_AnimBlendOutStartTime != 0 && m_UpdateAnimCamDuringBlendout )
	{
		if( Abs(m_SavedRelativeHeading) > SMALL_FLOAT ||
			Abs(m_SavedRelativePitch)   > SMALL_FLOAT )
		{
			if( m_CameraRelativeAnimatedCameraInput )
			{
				mtxTarget.RotateLocalZ( m_SavedRelativeHeading );
				mtxTarget.RotateLocalX( m_SavedRelativePitch );
			}
			else
			{
				float heading, pitch, roll;
				camFrame::ComputeHeadingPitchAndRollFromMatrix(mtxTarget, heading, pitch, roll);

				heading = fwAngle::LimitRadianAngleSafe(heading + m_SavedRelativeHeading);
				pitch   = fwAngle::LimitRadianAngleSafe(pitch   + m_SavedRelativePitch);

				camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, mtxTarget);
			}
		}
	}
	// B*2055894: Vehicle POV camera will handle blend out, so just update the animated camera while blending out.
	// B*2344965: For anim tasks in first person vehicle camera, let the vehicle POV camera do the blend in/out.
	if( camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera() && attachPed->GetIsInVehicle() && attachPed->GetVehiclePedInside()->GetIsAircraft() &&
		((!m_RunningAnimTask && m_AnimBlendOutStartTime != 0) || (m_RunningAnimTask && (m_AnimBlendInStartTime != 0 || m_AnimBlendOutStartTime != 0))) )
	{
		m_Frame.SetWorldMatrix(mtxTarget, false);
	}
	else if(m_AnimatedCameraBlendLevel >= 1.0f - SMALL_FLOAT)
	{
		if( m_AnimBlendOutStartTime == 0 || m_UpdateAnimCamDuringBlendout )
		{
			// For m_UpdateAnimCamDuringBlendout, we are blending out from current animated matrix.
			m_Frame.SetWorldMatrix(mtxTarget, false);
		}
		else
		{
			// For blend out, use previous orientation as the animated orientation is using default data.
			m_Frame.SetWorldMatrix(previousCameraMatrix, false);
			if(m_WasRunningAnimTaskWithAnimatedCamera)
			{
				// Use the previous camera position, but transform the previous position relative
				// to the previous mover by the current mover.
				Vector3 previousCameraPosition;
				Mat34V pedMatrix = attachPed->GetMatrix();
				RC_VEC3V(previousCameraPosition) = Transform(pedMatrix, m_PreviousRelativeCameraPosition);
				m_Frame.SetPosition( previousCameraPosition );
				m_WasRunningAnimTaskWithAnimatedCamera = false;
			}
			else
			{
				m_Frame.SetPosition( mtxTarget.d );
			}
		}
		m_AnimBlendInStartTime = 0;
	}
	else if( m_AnimBlendOutStartTime != 0 && m_AnimatedCameraBlendLevel <= SMALL_FLOAT)
	{
		m_bWasUsingAnimatedFirstPersonCamera = false;
		m_AnimBlendOutStartTime = 0;
	}
	else if(m_AnimatedCameraBlendLevel > 0.0f)
	{
		Quaternion qOrientation;
		Quaternion qFrame;
		if( m_AnimBlendInStartTime != 0 || m_UpdateAnimCamDuringBlendout )
		{
			// Blending in from current update's camera matrix to current animated matrix.
			// For m_UpdateAnimCamDuringBlendout, we are blending out from current animated matrix to the camera matrix without the animated camera.
			mtxTarget.ToQuaternion( qOrientation );
		}
		else
		{
			// Blending out from previous update's matrix to current update's matrix..
			// mtxTarget is the matrix with the last known animated data, but this is not used for the blend out.
			previousCameraMatrix.ToQuaternion( qOrientation );
		}
		m_Frame.GetWorldMatrix().ToQuaternion( qFrame );
		qFrame.PrepareSlerp(qOrientation);
		qFrame.Slerp(m_AnimatedCameraBlendLevel, qOrientation );

		mtxTarget.FromQuaternion( qFrame );
		mtxTarget.d.Lerp(1.0f - m_AnimatedCameraBlendLevel, m_Frame.GetPosition());
		m_Frame.SetWorldMatrix( mtxTarget, false );
	}

	// Rotate for input or lockon after blending.
	{
		float currentHeading, currentPitch;
		m_Frame.ComputeHeadingAndPitch(currentHeading, currentPitch);
		if(m_AnimatedCameraHeadingAccumulator != 0.0f && m_PreviousAnimatedHeading != FLT_MAX)
		{
			float fDifference = fwAngle::LimitRadianAngle(currentHeading - m_PreviousAnimatedHeading);
			m_AnimatedCameraHeadingAccumulator = fwAngle::LimitRadianAngle(m_AnimatedCameraHeadingAccumulator - fDifference);
		}
		if(m_AnimatedCameraPitchAccumulator != 0.0f && m_PreviousAnimatedPitch != FLT_MAX)
		{
			float fDifference = fwAngle::LimitRadianAngle(currentPitch - m_PreviousAnimatedPitch);
			m_AnimatedCameraPitchAccumulator = fwAngle::LimitRadianAngle(m_AnimatedCameraPitchAccumulator - fDifference);
		}
		m_PreviousAnimatedHeading = currentHeading;
		m_PreviousAnimatedPitch = currentPitch;

		const Matrix34 pedMatrix = MAT34V_TO_MATRIX34(attachPed->GetTransform().GetMatrix());
		float fPedHeading = camFrame::ComputeHeadingFromMatrix( pedMatrix );
	#if 1
		if(m_CombatRolling)
		{
			// When combat rolling, use mover heading instead of camera heading, since camera will invert.
			currentHeading = fPedHeading;
		}
	#else
		// Calculate heading using a modified world matrix, in case camera is upside-down.
		float fTempPitch;
		Matrix34 worldFixedMatrix = m_Frame.GetWorldMatrix();
		worldFixedMatrix.b.Cross(rage::ZAXIS, worldFixedMatrix.a);			// world up CROSS camera right = forward
		worldFixedMatrix.c.Cross(worldFixedMatrix.a, worldFixedMatrix.b);	// camera right CROSS new forward = up
		camFrame::ComputeHeadingAndPitchFromMatrix(worldFixedMatrix, currentHeading, fTempPitch);
	#endif

		float fRelativeHeading = 0.0f;
		if(!m_ActualAnimatedYawSpringInput)
		{
			m_AnimatedCameraHeadingAccumulator += lookAroundHeadingOffset;
			if(bAccumulatedHeadingAppliedToPedHeading)
			{
				// Don't do anything for lock on, handled later by rotating the ped to face target which will rotate the camera.
				if( m_IsLockedOn && m_CombatRolling && lockonHeading <= (PI + SMALL_FLOAT) )
				{
					float fDifference = fwAngle::LimitRadianAngle( lockonHeading - currentHeading );
					m_AnimatedCameraHeadingAccumulator = fDifference;
					fRelativeHeading = fDifference;
				}
				if(m_AnimatedCameraHeadingAccumulator != 0.0f)
				{
					float fRelativeHeading = fwAngle::LimitRadianAngle(currentHeading - fPedHeading);
					float fFinalHeading = fwAngle::LimitRadianAngle(fRelativeHeading + m_AnimatedCameraHeadingAccumulator);
					fFinalHeading = Clamp(fFinalHeading, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
					m_AnimatedCameraHeadingAccumulator = fwAngle::LimitRadianAngle(fFinalHeading - fRelativeHeading);
				}
			}
			else
			{
				if( m_bUseAnimatedFirstPersonCamera || (m_bWasUsingAnimatedFirstPersonCamera && m_UpdateAnimCamDuringBlendout) )
				{
					if(m_AnimatedCameraHeadingAccumulator != 0.0f)
					{
						m_AnimatedCameraHeadingAccumulator = Clamp(m_AnimatedCameraHeadingAccumulator, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
						////fRelativeHeading = m_AnimatedCameraHeadingAccumulator;
					}
					if( fRelativeHeading == 0.0f && m_IsLockedOn && lockonHeading <= (PI + SMALL_FLOAT) )
					{
						float fDifference = fwAngle::LimitRadianAngle( lockonHeading - currentHeading );
						fRelativeHeading = Clamp(fDifference, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
					}
				}
				else if(!m_UpdateAnimCamDuringBlendout)
				{
					m_AnimatedCameraHeadingAccumulator = 0.0f;
				}
			}
		}
		else if(m_ActualAnimatedYawSpringInput == true && m_ActualAnimatedYawSpringInput == true && m_AnimatedCameraHeadingAccumulator != 0.0f)
		{
			// Convert to relative heading.
			m_RelativeHeading = m_AnimatedCameraHeadingAccumulator;
			m_AnimatedCameraHeadingAccumulator = 0.0f;
		}

		float fRelativePitch = 0.0f;
		if(!m_ActualAnimatedPitchSpringInput)
		{
			m_AnimatedCameraPitchAccumulator += lookAroundPitchOffset;
			if(m_AnimatedCameraPitchAccumulator != 0.0f)
			{
				m_AnimatedCameraPitchAccumulator = Clamp(m_AnimatedCameraPitchAccumulator, springPitchLimitsRadians.x, springPitchLimitsRadians.y);

				if(bAccumulatedHeadingAppliedToPedHeading)
				{
					fRelativePitch = m_AnimatedCameraPitchAccumulator;
				}
				else if(!m_bUseAnimatedFirstPersonCamera && !m_UpdateAnimCamDuringBlendout)
				{
					m_AnimatedCameraPitchAccumulator = 0.0f;
				}
			}
			if( fRelativePitch == 0.0f && m_IsLockedOn && lockonPitch <= (PI + SMALL_FLOAT) )
			{
				float fDifference = fwAngle::LimitRadianAngle( lockonPitch - currentPitch );
				fRelativePitch = Clamp(fDifference, springPitchLimitsRadians.x, springPitchLimitsRadians.y);
			}
		}
		else if(m_ActualAnimatedPitchSpringInput == true && m_ActualAnimatedPitchSpringInput == true && m_AnimatedCameraPitchAccumulator != 0.0f)
		{
			// Convert to relative pitch.
			m_RelativePitch = m_AnimatedCameraPitchAccumulator;
			m_AnimatedCameraPitchAccumulator = 0.0f;
		}

		if(fRelativeHeading != 0.0f || fRelativePitch != 0.0f)
		{
			Matrix34 worldMatrix = m_Frame.GetWorldMatrix();
			worldMatrix.RotateLocalZ( fRelativeHeading );
			worldMatrix.RotateLocalX( fRelativePitch );

			m_Frame.SetWorldMatrix(worldMatrix, true);
		}

		TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, c_bClampAnimatedCameraBlendoutInput, true);
		if(!m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera && (!m_ActualAnimatedYawSpringInput || !m_ActualAnimatedPitchSpringInput))
		{
			float animatedHeading, animatedPitch;
			camFrame::ComputeHeadingAndPitchFromMatrix(mtxFinalAnimated, animatedHeading, animatedPitch);

			// Blending out, if we were turning the camera, have to turn the previous matrix so input does not slow down.
			Matrix34 worldMatrix = m_Frame.GetWorldMatrix();
			if(!m_ActualAnimatedYawSpringInput && fRelativeHeading == 0.0f && lookAroundHeadingOffset != 0.0f)
			{
				if(c_bClampAnimatedCameraBlendoutInput)
				{
					float currentRelativeHeading = fwAngle::LimitRadianAngle(currentHeading - animatedHeading);
					float fEndRelativeHeading = Clamp(currentRelativeHeading + lookAroundHeadingOffset, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
					lookAroundHeadingOffset = fwAngle::LimitRadianAngle(fEndRelativeHeading - currentRelativeHeading);
				}
				worldMatrix.RotateLocalZ( lookAroundHeadingOffset );
			}
			if(!m_ActualAnimatedPitchSpringInput && fRelativePitch == 0.0f && lookAroundPitchOffset != 0.0f)
			{
				if(c_bClampAnimatedCameraBlendoutInput)
				{
					float currentRelativePitch = fwAngle::LimitRadianAngle(currentPitch - animatedPitch);
					float fEndRelativePitch = Clamp(currentRelativePitch + lookAroundPitchOffset, springPitchLimitsRadians.x, springPitchLimitsRadians.y);
					lookAroundPitchOffset = fwAngle::LimitRadianAngle(fEndRelativePitch - currentRelativePitch);
				}
				worldMatrix.RotateLocalX( lookAroundPitchOffset );
			}
			m_Frame.SetWorldMatrix(worldMatrix, true);
		}
	}

	if( m_LookAroundDuringHeadSpineOrient)
	{
		SpringInputParams paramSpringInput;
		paramSpringInput.bEnable = m_LookAroundDuringHeadSpineOrient && m_bUseAnimatedFirstPersonCamera;
		paramSpringInput.bAllowHorizontal = true;
		paramSpringInput.bAllowLookDown = m_LookDownDuringHeadSpineOrient;
	#if RSG_PC
		paramSpringInput.bEnableSpring = !m_CurrentControlHelper->WasUsingKeyboardMouseLastInput();
	#else
		paramSpringInput.bEnableSpring = true;
	#endif
		paramSpringInput.bReset = !m_bWasUsingAnimatedFirstPersonCamera;

		// No input on blend out as we are using actual input, but still need to run to blend out the relative
		// heading/pitch, whose difference will be added onto the actual heading/pitch as it goes to zero.
		const bool bEnableHeadingInput = (m_ActualAnimatedYawSpringInput);
		const bool bEnablePitchInput   = (m_ActualAnimatedPitchSpringInput);

		// Do not apply relative heading input during blendout.
		paramSpringInput.headingInput = (bEnableHeadingInput) ? lookAroundHeadingOffset : 0.0f;
		paramSpringInput.pitchInput   = (bEnablePitchInput)   ? lookAroundPitchOffset   : 0.0f;

		float heading, pitch, roll;
		m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

		CalculateRelativeHeadingPitch( paramSpringInput, false, heading, pitch, &mtxFinalAnimated );

		if( m_bUseAnimatedFirstPersonCamera &&
			(Abs(m_RelativeHeading) > SMALL_FLOAT ||Abs(m_RelativePitch)   > SMALL_FLOAT) )
		{
			if( m_CameraRelativeAnimatedCameraInput )
			{
				mtxTarget = m_Frame.GetWorldMatrix();
				mtxTarget.RotateLocalZ( fwAngle::LimitRadianAngle(m_RelativeHeading) );
				mtxTarget.RotateLocalX( fwAngle::LimitRadianAngle(m_RelativePitch) );
				m_Frame.SetWorldMatrix( mtxTarget, true );
			}
			else
			{
				heading = fwAngle::LimitRadianAngleSafe(heading + m_RelativeHeading);
				pitch   = fwAngle::LimitRadianAngleSafe(pitch   + m_RelativePitch);

				m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
			}

			if(m_AnimatedCameraBlendLevel < SMALL_FLOAT && !m_bUseAnimatedFirstPersonCamera)
			{
				// Animated camera done and blended out, clear relative heading/pitch.
				m_RelativeHeadingSpring.Reset(0.0f);
				m_RelativeHeading = 0.0f;
				m_RelativePitchSpring.Reset(0.0f);
				m_RelativePitch = 0.0f;
			}
		}
	}

	if( m_AnimBlendOutStartTime == 0 )
	{
		m_SavedRelativeHeading = m_RelativeHeading;
		m_SavedRelativePitch   = m_RelativePitch;
	}
}

void camFirstPersonShooterCamera::UseHeadSpineBoneOrientation(const CPed* attachPed, const HeadSpineOrientationParams& oParam, bool bAnimatedCameraChangesStickHeading)
{
	Matrix34 mtxTarget;
	Matrix34 mtxSpine;
	Matrix34 mtxHead;

	s32 spineBoneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_SPINE3);
	s32 headBoneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);

	if(spineBoneIndex != -1)
		mtxSpine = attachPed->GetObjectMtx(spineBoneIndex);
	else
		mtxSpine.Identity();

	if(headBoneIndex != -1)
		mtxHead = attachPed->GetObjectMtx(headBoneIndex);
	else
		mtxHead.Identity();

	// If we have root slope fixup IK being applied then we need to transform the head bone by the fixup amount.  The fixup amount will be a frame delayed
	// since the IK solve happens after the camera update but data that's a frame old is better than no data at all...
	const CRootSlopeFixupIkSolver* pRootSlopeFixupIkSolver = (CRootSlopeFixupIkSolver*)(attachPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pRootSlopeFixupIkSolver != NULL && pRootSlopeFixupIkSolver->GetBlend() > 0.0f)
	{
		Transform(RC_MAT34V(mtxSpine), RCC_MAT34V(mtxSpine), pRootSlopeFixupIkSolver->GetSpine3TransformMatrix());
		Transform(RC_MAT34V(mtxHead), RCC_MAT34V(mtxHead), pRootSlopeFixupIkSolver->GetHeadTransformMatrix());
	}

	const Mat34V* parentMtx = attachPed->GetSkeleton()->GetParentMtx();
	if (parentMtx != NULL)
	{
		mtxSpine.Dot(RCC_MATRIX34(*parentMtx));
		mtxHead.Dot(RCC_MATRIX34(*parentMtx));
	}
	mtxSpine.RotateLocalY(HALF_PI);
	mtxHead.RotateLocalY(HALF_PI);

	Quaternion qSpine;
	Quaternion qHead;
	mtxSpine.ToQuaternion(qSpine);
	mtxHead.ToQuaternion(qHead);

	Quaternion qTarget = qHead;
	qTarget.PrepareSlerp(qSpine);
	qTarget.Slerp(m_Metadata.m_HeadToSpineRatio, qSpine);

	mtxTarget.FromQuaternion(qTarget);
	mtxTarget.d = mtxHead.d;

	// Since we overwrote the camera position, re-apply the first person camera's offset which was relative to the head bone. (saved in m_RelativeHeadOffset)
	Vector3 attachOffset;
	if( !camInterface::GetGameplayDirector().IsApplyRelativeOffsetInCameraSpace() )
	{
		mtxHead.Transform(m_RelativeHeadOffset, attachOffset);
		mtxTarget.d = attachOffset;
	}
	else
	{
		TransformAttachPositionToCameraSpace(m_RelativeHeadOffset, attachOffset, !attachPed->GetUsingRagdoll(), &mtxTarget);
		mtxTarget.d += attachOffset;
	}


	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, c_HeadSpineCarJackedVerticalOffset, 0.10f, 0.0f, 1.0f, 0.001f);
	float localWaterHeight;
	Vector3 positionToTest = mtxTarget.d;
	const float c_ZOffsetForWaterTest		= 0.15f;		// was 0.0f
	const float c_MaxDistanceForWaterTest	= 0.50f;		// was 0.30f
	positionToTest.z += c_ZOffsetForWaterTest;
	bool isLocalWaterHeightValid			= camCollision::ComputeWaterHeightAtPosition(positionToTest, c_MaxDistanceForWaterTest, localWaterHeight, c_MaxDistanceForWaterTest);
	if( isLocalWaterHeightValid )
	{
		float fWaterHeightDifference = mtxTarget.d.z - localWaterHeight;
		if( Abs(fWaterHeightDifference) < c_MaxDistanceForWaterTest )
		{
			float fDotVertical = mtxTarget.b.Dot(rage::ZAXIS);
			if(fDotVertical > SMALL_FLOAT && fWaterHeightDifference < 0.0f)
			{
				mtxTarget.d.z = Min(mtxTarget.d.z, (localWaterHeight - c_HeadSpineCarJackedVerticalOffset));
			}
			else if(fDotVertical <= SMALL_FLOAT && fWaterHeightDifference > 0.0f)
			{
				mtxTarget.d.z = Max(mtxTarget.d.z, (localWaterHeight + c_HeadSpineCarJackedVerticalOffset));
			}
		}
	}

	if(!bAnimatedCameraChangesStickHeading && !m_bWasUsingHeadSpineOrientation)
	{
		// Save the head/spine orientation starting heading/pitch without being turned for ground collision.
		m_SavedHeading = CalculateHeading(mtxTarget.b, mtxTarget.c);
		m_SavedPitch   = camFrame::ComputePitchFromMatrix(mtxTarget);
	}

	if( oParam.bIsFalling && !attachPed->GetUsingRagdoll() && !m_PlayerGettingUp)
	{
		// Special case: head does not look down enough, so force it a bit more down.
		const float fDesiredPitch = m_Metadata.m_MinPitch * 0.90f * DtoR;
		float fCurrentPitch = camFrame::ComputePitchFromMatrix( mtxTarget );
		if (fCurrentPitch > fDesiredPitch)
		{
			mtxTarget.RotateLocalX( fDesiredPitch - fCurrentPitch );
			if(!m_bWasFalling)
			{
				m_HeadSpineRagdollBlendTime = 0;
				m_HeadSpineBlendInTime = fwTimer::GetTimeInMilliseconds();
			}
			m_bWasFalling = true;
		}
	}
	else
	{
		m_bWasFalling = false;
	}

	float fInterp = 1.0f;
	// m_bWasUsingHeadSpineOrientation may be have been reset by HandleCustomSitation() so check oParam.bUsedHeadSpineBoneLastUpdate instead.
	if (m_HeadSpineBlendInTime == 0 && !oParam.bUsedHeadSpineBoneLastUpdate && oParam.bUseHeadSpineBoneOrientation)
	{
		m_HeadSpineBlendInTime = fwTimer::GetTimeInMilliseconds();
		if(m_JackedFromVehicle)
		{
			m_bWasUsingRagdollForHeadSpine |= true;
		}
	}
	else if(m_HeadSpineRagdollBlendTime == 0 && attachPed->GetUsingRagdoll() && !m_bWasUsingRagdollForHeadSpine)
	{
		m_HeadSpineRagdollBlendTime = fwTimer::GetTimeInMilliseconds();
	}
	m_bWasUsingRagdollForHeadSpine |= attachPed->GetUsingRagdoll();

	if( m_HeadSpineBlendInTime != 0 )
	{
		m_HeadSpineRagdollBlendTime = 0;
		u32 blendDuration = m_Metadata.m_HeadSpineBlendInDuration;
		if(m_JackedFromVehicle)
		{
			blendDuration >>= 4;
		}
		else if( attachPed->GetPedResetFlag(CPED_RESET_FLAG_JumpingOutOfVehicle) ||
				 attachPed->GetUsingRagdoll() ||
				 m_IsCarJacking )
		{
			blendDuration >>= 3;
		}

		if( m_HeadSpineBlendInTime + blendDuration > fwTimer::GetTimeInMilliseconds() )
		{
			fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_HeadSpineBlendInTime) / (float)blendDuration;
		}
		else
		{
			m_HeadSpineBlendInTime = 0;
		}
	}
	else if( m_HeadSpineRagdollBlendTime != 0 )
	{
		const u32 blendDuration = m_Metadata.m_HeadSpineRagdollBlendInDuration;
		if( m_HeadSpineRagdollBlendTime + m_Metadata.m_HeadSpineRagdollBlendInDuration > fwTimer::GetTimeInMilliseconds() )
		{
			fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_HeadSpineRagdollBlendTime) / (float)blendDuration;
		}
		else
		{
			m_HeadSpineRagdollBlendTime = 0;
		}
	}

	if(fInterp >= 1.0f)
	{
		m_Frame.SetWorldMatrix(mtxTarget, false);
		m_HeadSpineRagdollBlendTime = 0;
		m_HeadSpineBlendInTime = 0;
	}
	else if(fInterp > 0.0f || m_HeadSpineRagdollBlendTime != 0)
	{
		Quaternion qFrame;
		Quaternion qHeadSpine;

		if( m_HeadSpineBlendInTime != 0 )
			m_Frame.GetWorldMatrix().ToQuaternion( qFrame );
		else
			qFrame = oParam.qPreviousCamera;

		mtxTarget.ToQuaternion( qHeadSpine );
		qFrame.PrepareSlerp( qHeadSpine );
		qFrame.Slerp( fInterp, qHeadSpine );

		mtxTarget.FromQuaternion( qFrame );
		if(m_HeadSpineRagdollBlendTime != 0)
		{
			// If we are blending to/from ragdoll, just do the orientation.
			mtxTarget.d.Lerp(1.0f - fInterp, m_Frame.GetPosition());
		}
		m_Frame.SetWorldMatrix( mtxTarget, false );
	}

	if( m_LookAroundDuringHeadSpineOrient )
	{
		float heading, pitch, roll;
		m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);
		float savedHeading = heading;
		float savedPitch   = pitch;

		SpringInputParams paramSpringInput;
		paramSpringInput.bEnable = m_LookAroundDuringHeadSpineOrient;
		paramSpringInput.bAllowHorizontal = true;
		paramSpringInput.bAllowLookDown = m_LookDownDuringHeadSpineOrient;
	#if RSG_PC
		const bool bAnimatedCameraBlendingOut = m_bWasUsingAnimatedFirstPersonCamera && !m_bUseAnimatedFirstPersonCamera;
		paramSpringInput.bEnableSpring = !m_CurrentControlHelper->WasUsingKeyboardMouseLastInput() || bAnimatedCameraBlendingOut;
	#else
		paramSpringInput.bEnableSpring = true;
	#endif
		paramSpringInput.bReset = !m_bWasUsingHeadSpineOrientation;
		paramSpringInput.headingInput = oParam.lookAroundHeadingOffset;
		paramSpringInput.pitchInput = oParam.lookAroundPitchOffset;

		CalculateRelativeHeadingPitch( paramSpringInput, false );

		heading = fwAngle::LimitRadianAngleSafe(heading + m_RelativeHeading);
		pitch   = fwAngle::LimitRadianAngleSafe(pitch   + m_RelativePitch);

		if( savedHeading != heading || savedPitch != pitch )
		{
			m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
		}
	}
}

void camFirstPersonShooterCamera::UseGroundAvoidance(const CPed& attachPed)
{
	float fTargetAngle = 0.0f;

	Matrix34 mtxTarget = m_Frame.GetWorldMatrix();

	Vector3 vGroundNormal = attachPed.GetGroundNormal();
	float fDotVertical = -mtxTarget.b.z; //mtxTarget.bDot( -rage::ZAXIS );
	float fDotNormal = mtxTarget.b.Dot( -vGroundNormal );
	float fCosVerticalLimit = rage::Cosf( m_Metadata.m_AvoidVerticalAngle * DtoR );
	s32 pelvisBoneIndex = attachPed.GetBoneIndexFromBoneTag(BONETAG_PELVIS);

	if(pelvisBoneIndex >= 0 && (fDotVertical > fCosVerticalLimit || fDotNormal > fCosVerticalLimit))
	{
		// TODO: test against vehicles as well?
		const u8 c_MaxTests = 2;
		u32 includeFlagsForLineTest = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE;

		WorldProbe::CShapeTestProbeDesc lineTest;
		WorldProbe::CShapeTestFixedResults<c_MaxTests> shapeTestResults;
		lineTest.SetResultsStructure(&shapeTestResults);
		lineTest.SetIsDirected(false);
		lineTest.SetIncludeFlags(includeFlagsForLineTest);
		lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		lineTest.SetContext(WorldProbe::LOS_Camera);
		lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		Vector3 startPosition = mtxTarget.d;
		// This should probably be using a vector perpendicular to a vector representing the spine instead of the ground normal
		Vector3 endPosition   = startPosition - vGroundNormal * m_Metadata.m_AvoidVerticalRange;
		lineTest.SetStartAndEnd(startPosition, endPosition);

		bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		////if(!hasHit)
		////{
		////	endPosition = startPosition - rage::ZAXIS * m_Metadata.m_AvoidVerticalRange;
		////	lineTest.SetStartAndEnd(startPosition, endPosition);
		////	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		////}

		if( hasHit )
		{
			if( m_GroundAvoidLookDirection != 0.0f )
			{
				// Allow the avoid ground direction to change when any current avoid ground direction is blended out
				if( IsClose(m_GroundAvoidCurrentAngle, 0.0f, VERY_SMALL_FLOAT) )
				{
					m_GroundAvoidLookDirection = 0.0f;
				}
		        else if( attachPed.GetUsingRagdoll() )
				{
			        static const float sfSlowVelSq = square(5.0f);
			        float fCOMvelSq = (attachPed.GetVelocity() - attachPed.GetReferenceFrameVelocity()).Mag2();
					// Allow the avoid ground direction to change while a ragdolled ped has sufficient velocity
					if( fCOMvelSq > sfSlowVelSq )
					{
						m_GroundAvoidLookDirection = 0.0f;
					}
				}
			}

			if( m_GroundAvoidLookDirection == 0.0f )
			{
				// Handle turning to a side based on hip direction, if close to ground.
				// Compare hip forward to mtxTarget right.
				Matrix34 mtxPelvis;
				attachPed.GetGlobalMtx(pelvisBoneIndex, mtxPelvis);
				if( mtxPelvis.a.Dot( mtxTarget.a ) >= 0.0f )
				{
					m_GroundAvoidLookDirection = -1.0f;
				}
				else
				{
					m_GroundAvoidLookDirection = 1.0f;
				}
			}

			// Rescale TValue so we 0.0 to 0.5 are mapped to zero and 0.5 to 1.0 are remapped to 0.0 to 1.0.
			float fTValue = Clamp( (shapeTestResults[0].GetHitTValue() - 0.50f) * 2.0f, 0.0f, 1.0f);
			INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS, fGroundAvoidMaxAngle, 0.0f, PI, 0.05f)
			fTargetAngle = fGroundAvoidMaxAngle - rage::Acosf( fDotNormal );
			// Invert TValue for interpolation so < 0.5 we are turned fully, 0.5 to 1.0 we interpolate the turn angle.
			float fInterp = 1.0f - fTValue;
			fTargetAngle = Lerp( fInterp, 0.0f, fTargetAngle );
			fTargetAngle *= m_GroundAvoidLookDirection;
		}
	}
	
	if( fTargetAngle != 0.0f || m_GroundAvoidCurrentAngle != 0.0f )
	{
		float fAvoidBlendRate = m_Metadata.m_AvoidBlendInRate;
		if( Abs(m_GroundAvoidCurrentAngle) > Abs(fTargetAngle) )
		{
			fAvoidBlendRate = m_Metadata.m_AvoidBlendOutRate;
		}

		Approach( m_GroundAvoidCurrentAngle, fTargetAngle, fAvoidBlendRate, fwTimer::GetCamTimeStep() );

		mtxTarget.RotateLocalZ( m_GroundAvoidCurrentAngle );
		m_Frame.SetWorldMatrix( mtxTarget );
	}
}

const camFirstPersonShooterCameraMetadataOrientationSpring& camFirstPersonShooterCamera::GetCurrentOrientationSpring() const
{
	if(m_bParachutingOpenedChute)
	{
		return m_Metadata.m_OrientationSpringParachute;
	}
	else if(m_bParachuting)
	{
		return m_Metadata.m_OrientationSpringSkyDive;
	}

	return m_Metadata.m_OrientationSpring;
}

void camFirstPersonShooterCamera::CalculateRelativeHeadingPitch(const SpringInputParams& oParam, bool bIsLookingBehind,
																float heading/*=0.0f*/, float pitch/*=0.0f*/, const Matrix34* pmtxFinalAnimated/*=NULL*/)
{
	Vector2 springHeadingLimitsRadians, springPitchLimitsRadians;
	GetHeadingPitchSpringLimits(springHeadingLimitsRadians, springPitchLimitsRadians);

	const camFirstPersonShooterCameraMetadataOrientationSpring &orientationSpring = GetCurrentOrientationSpring();

	// This runs all the time now, so no need to reset.
	////if( oParam.bReset )
	////{
	////	// Reset.
	////	m_RelativeHeadingSpring.Reset( 0.0f );
	////	m_RelativePitchSpring.Reset( 0.0f );
	////	m_RelativeHeading = 0.0f;
	////	m_RelativePitch   = 0.0f;
	////}

	bool bNoBlendOut = m_IsAiming && m_bParachutingOpenedChute;

	// Return to default heading and pitch.
	if(!bNoBlendOut)
	{
		float relativeHeadingSpringConstant = (!bIsLookingBehind && !m_bIgnoreHeadingLimits) ? orientationSpring.m_SpringConstant : orientationSpring.m_LookBehindSpringConstant;
		float relativePitchSpringConstant   = orientationSpring.m_SpringConstant;
		float relativeHeadingDampingRatio   = (!bIsLookingBehind && !m_bIgnoreHeadingLimits) ? orientationSpring.m_SpringDampingRatio : orientationSpring.m_LookBehindSpringDampingRatio;

		// Override spring with current relative input, so we always try to blend to zero.
		m_RelativeHeadingSpring.OverrideResult(m_RelativeHeading);
		m_RelativePitchSpring.OverrideResult(m_RelativePitch);

		// Use spring to blend relative orientation back desired. (relative offset, so desired is zero)
		// TODO: randomly choose a side to rotate?
		const float c_LookBehindFinalAngle = (!m_bParachuting) ? m_Metadata.m_LookBehindAngle : 179.0f;
		const float fLookBehindAngle = (m_ShouldRotateRightForLookBehind) ? -c_LookBehindFinalAngle : c_LookBehindFinalAngle;
		const float fTargetHeading = (!bIsLookingBehind) ? 0.0f : (fLookBehindAngle * DtoR);

		if(fTargetHeading == 0.0f && !bIsLookingBehind && !m_bIgnoreHeadingLimits)
		{
			float fInterp = RampValueSafe(Abs(m_RelativeHeadingSpring.GetResult()), 15.0f*DtoR, 45.0f*DtoR, 0.0f, 1.0f);
			relativeHeadingSpringConstant = Lerp(fInterp, orientationSpring.m_SpringConstant, orientationSpring.m_SpringConstant*4.0f);
			relativeHeadingDampingRatio   = Lerp(fInterp, orientationSpring.m_SpringDampingRatio, 0.95f);
		}

		{
			if(oParam.bEnableSpring)
			{
				m_RelativeHeading = m_RelativeHeadingSpring.Update(fTargetHeading, relativeHeadingSpringConstant, relativeHeadingDampingRatio);
				m_RelativePitch   = m_RelativePitchSpring.Update(0.0f, relativePitchSpringConstant,   orientationSpring.m_SpringDampingRatio);
			}

			if(IsClose(m_RelativeHeading, fTargetHeading, 0.01f))
			{
				m_bDoingLookBehindTransition = false;
				m_bWasLookingBehind = (fTargetHeading != 0.0f);
				if(m_bIgnoreHeadingLimits && fTargetHeading == 0.0f)
				{
					// Safety: if we are returning to zero and are nearly there, no need to ignore heading limits.
					m_bIgnoreHeadingLimits = false;
				}
			}
			else
			{
				if(bIsLookingBehind && !m_bDoingLookBehindTransition) 
				{
					// Start of look behind... fixup camera position as ped will rotate quickly for camera orientation, and leave camera behind.
					Matrix34 targetMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
					targetMatrix.RotateLocalZ(m_RelativeHeading);
					TransformAttachPositionToWorldSpace(targetMatrix, m_PreviousRelativeAttachPosition, m_AttachPosition);
					m_Frame.SetPosition(m_AttachPosition);
				}
				m_bDoingLookBehindTransition = true;
			}
		}
	}

	if( oParam.bEnable &&
		(Abs(m_RelativeHeading) > VERY_SMALL_FLOAT ||
		 Abs(m_RelativePitch) > VERY_SMALL_FLOAT ||
		 Abs(oParam.headingInput) > VERY_SMALL_FLOAT ||
		 oParam.pitchInput > VERY_SMALL_FLOAT ||
		 (oParam.bAllowLookDown && oParam.pitchInput < -VERY_SMALL_FLOAT)) )
	{
		float fPitchInput = oParam.pitchInput;
		if(!oParam.bAllowLookDown && oParam.pitchInput < 0.0f)
			fPitchInput = 0.0f;

		float fHeadingInput = oParam.headingInput;
		if(!oParam.bAllowHorizontal)
			fHeadingInput = 0.0f;

		if(m_bUseAnimatedFirstPersonCamera)
		{
			if(!m_ActualAnimatedYawSpringInput)
				fHeadingInput = 0.0f;
			if(!m_ActualAnimatedPitchSpringInput)
				fPitchInput = 0.0f;
		}

		// Ensure the heading/pitch limits are applied relative to the animated camera forward,
		// in case we are blending to/from the non-animated camera matrix.
		if(pmtxFinalAnimated != NULL)
		{
			float animatedHeading, animatedPitch;
			camFrame::ComputeHeadingAndPitchFromMatrix(*pmtxFinalAnimated, animatedHeading, animatedPitch);

			if(heading != animatedHeading)
			{
				// Relative heading is relative to the animated camera.  If the matrix is blending with the "game" camera,
				// have to convert the relative heading limits to be relative to the blended heading and re-clamp.
				const float fAngleDiff = fwAngle::LimitRadianAngle(animatedHeading - heading);
				springHeadingLimitsRadians += Vector2(fAngleDiff, fAngleDiff);
			}

			if(pitch != animatedPitch)
			{
				// Relative pitch is relative to the animated camera.  If the matrix is blending with the "game" camera,
				// have to convert the relative pitch limits to be relative to the blended pitch and re-clamp.
				const float fAngleDiff = fwAngle::LimitRadianAngle(animatedPitch - pitch);
				springPitchLimitsRadians += Vector2(fAngleDiff, fAngleDiff);
			}
		}

		// Add input.
		m_RelativeHeading = Clamp(m_RelativeHeading + fHeadingInput, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
		m_RelativePitch   = Clamp(m_RelativePitch   + fPitchInput,   springPitchLimitsRadians.x,   springPitchLimitsRadians.y);
	}
}

void camFirstPersonShooterCamera::UpdateLookBehind(const CPed* attachPed)
{
	m_bLookingBehind = false;
	m_bLookBehindCancelledForAiming = false;

	bool bPedBlockingLookBehind = false;
	if (attachPed)
	{
		if (attachPed->GetIsSwimming())
		{
			bPedBlockingLookBehind = true;
		}
		else if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover) || attachPed->GetIsInCover())
		{
			bPedBlockingLookBehind = true;
		}
		else if ( ////!m_bParachuting &&
				 (attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir)	|| 
				  attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling)		||
				  attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding)		||
				  attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasControlOfPlayer)) )
		{
			bPedBlockingLookBehind = true;
		}
	}

	//! no look behind when skydiving. causes camera to lock up.
	if(m_bParachuting && !m_bParachutingOpenedChute)
	{
		bPedBlockingLookBehind = true;
	}

	const bool c_IsLookBehindBlocked = bPedBlockingLookBehind || m_RunningAnimTask || m_TaskControllingMovement;
	if( !c_IsLookBehindBlocked && m_CurrentControlHelper && m_CurrentControlHelper->IsLookingBehind() )
	{
		const bool bAimingOrShooting = CPlayerInfo::IsAiming(true) || CPlayerInfo::IsFiring_s();
		const bool bCancelLookBehind = m_MeleeAttack || bAimingOrShooting;
		if(bCancelLookBehind)
		{
			m_bWaitForLookBehindUp = true;
		}

		m_bLookBehindCancelledForAiming = m_bIgnoreHeadingLimits && bCancelLookBehind;
		if(!m_bWaitForLookBehindUp)
		{
			m_bLookingBehind = !bAimingOrShooting;
			m_bWasLookingBehind |= m_bLookingBehind;
			m_bIgnoreHeadingLimits = !bAimingOrShooting;
		}
	}
	
	if(m_bWaitForLookBehindUp)
	{
		const bool bAimingOrShooting = CPlayerInfo::IsAiming(true) || CPlayerInfo::IsFiring_s();
		const bool bCancelLookBehind = m_MeleeAttack || bAimingOrShooting;
		if(!bCancelLookBehind)
		{
			// If aiming, ignore look behind, until look behind input is released and we are no longer aiming.
			CControl& rControl = CControlMgr::GetMainPlayerControl(true);
			const ioValue &lookBehindControl = rControl.GetPedLookBehind();
			if(lookBehindControl.IsUp())
				m_bWaitForLookBehindUp = false;
		}

		if( c_IsLookBehindBlocked )
			m_bWaitForLookBehindUp = false;			// Reset since look behind not allowed.
	}
}

float camFirstPersonShooterCamera::ComputeMotionBlurStrengthForLookBehind()
{
	float fReturn = 0.0f;
	TUNE_GROUP_FLOAT(CAM_FPS, MAX_MOTION_BLUR_LOOK_BEHIND, 0.80f, 0.0f, 2.0f, 0.01f);
	if(m_bLookingBehind || m_bIgnoreHeadingLimits)
	{
		const float fLookBehindAngle = (m_ShouldRotateRightForLookBehind) ? -m_Metadata.m_LookBehindAngle : m_Metadata.m_LookBehindAngle;
		const float fTargetHeading = (!m_bLookingBehind) ? 0.0f : (fLookBehindAngle * DtoR);

		float fDelta = Abs( fwAngle::LimitRadianAngle(fTargetHeading - m_RelativeHeading) );
		////fReturn = RampValueSafe(fDelta, 10.0f*DtoR, 55.0f*DtoR, 0.0f, 0.8f);
		float fInterp = fDelta / (m_Metadata.m_LookBehindAngle * DtoR);
		fInterp = BellInOut(fInterp);
		fReturn = fInterp * MAX_MOTION_BLUR_LOOK_BEHIND;
	}
	return (fReturn);
}

void camFirstPersonShooterCamera::UpdateDof()
{
	camFirstPersonAimCamera::UpdateDof();

	//Blend towards the free aim focus distance when actually aiming.

	if(!(m_AttachParent && m_AttachParent->GetIsTypePed()))
	{
		return;
	}

	const CPed* attachPed			= static_cast<const CPed*>(m_AttachParent.Get());
	const CPlayerInfo* playerInfo	= attachPed->GetPlayerInfo();
	if(!playerInfo)
	{
		return;
	}

	const Vector3& freeAimTargetPosition	= playerInfo->GetTargeting().GetClosestFreeAimTargetPos();

	float overriddenFocusDistance			= m_PostEffectFrame.GetDofOverriddenFocusDistance();
	float overriddenFocusDistanceBlendLevel	= m_PostEffectFrame.GetDofOverriddenFocusDistanceBlendLevel();

	const Vector3& cameraPosition			= m_PostEffectFrame.GetPosition();
	const float desiredFreeAimFocusDistance	= cameraPosition.Dist(freeAimTargetPosition);

	const float freeAimFocusDistanceOnPreviousUpdate = (m_NumUpdatesPerformed == 0) ? desiredFreeAimFocusDistance : m_FreeAimFocusDistanceSpring.GetResult();

	float springConstant			= 0.0f;
	float springDampingRatio		= 1.0f;
	const fwFlags16& cameraFlags	= m_PostEffectFrame.GetFlags();
	const bool shouldCutSpring		= ((m_NumUpdatesPerformed == 0) || cameraFlags.IsFlagSet(camFrame::Flag_HasCutPosition) || cameraFlags.IsFlagSet(camFrame::Flag_HasCutOrientation));
	if(!shouldCutSpring)
	{
		const bool isFreeAimFocusDistanceIncreasing			= (desiredFreeAimFocusDistance > freeAimFocusDistanceOnPreviousUpdate);
		const float focusDistanceIncreaseSpringConstant		= m_PostEffectFrame.GetDofFocusDistanceIncreaseSpringConstant();
		const float focusDistanceDecreaseSpringConstant		= m_PostEffectFrame.GetDofFocusDistanceDecreaseSpringConstant();

		springConstant		= isFreeAimFocusDistanceIncreasing ? focusDistanceIncreaseSpringConstant : focusDistanceDecreaseSpringConstant;
		springDampingRatio	= isFreeAimFocusDistanceIncreasing ? m_Metadata.m_FreeAimFocusDistanceIncreaseDampingRatio : m_Metadata.m_FreeAimFocusDistanceDecreaseDampingRatio;
	}

	const bool shouldLockAdaptiveDofFocusDistance	= cameraFlags.IsFlagSet(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);
	const float freeAimFocusDistanceToApply			= shouldLockAdaptiveDofFocusDistance ? freeAimFocusDistanceOnPreviousUpdate :
														m_FreeAimFocusDistanceSpring.Update(desiredFreeAimFocusDistance, springConstant, springDampingRatio, true);

	const float aimingBlendLevel = m_DofAimingBlendSpring.GetResult();

	if(overriddenFocusDistanceBlendLevel >= SMALL_FLOAT)
	{
		overriddenFocusDistance = Lerp(aimingBlendLevel, overriddenFocusDistance, freeAimFocusDistanceToApply);
	}
	else
	{
		overriddenFocusDistance = freeAimFocusDistanceToApply;
	}

	overriddenFocusDistanceBlendLevel = Lerp(aimingBlendLevel, overriddenFocusDistanceBlendLevel, 1.0f);

	m_PostEffectFrame.SetDofOverriddenFocusDistance(overriddenFocusDistance);
	m_PostEffectFrame.SetDofOverriddenFocusDistanceBlendLevel(overriddenFocusDistanceBlendLevel);

	// Blend the high cover DofMaxNearInFocusDistance (and BlendLevel)
	float maxNearInFocusDistance = m_PostEffectFrame.GetDofMaxNearInFocusDistance();
	float maxNearInFocusDistanceBlendLevel = m_PostEffectFrame.GetDofMaxNearInFocusDistanceBlendLevel();

	const float highCoverBlendLevel = m_DofHighCoverBlendSpring.GetResult();

	if(highCoverBlendLevel >= SMALL_FLOAT)
	{								 		
		maxNearInFocusDistance = Lerp(highCoverBlendLevel, maxNearInFocusDistance, Max(maxNearInFocusDistance, m_Metadata.m_HighCoverMaxNearInFocusDistance));
	}	

	maxNearInFocusDistanceBlendLevel = Lerp(highCoverBlendLevel, maxNearInFocusDistanceBlendLevel, 1.0f);
	
	m_PostEffectFrame.SetDofMaxNearInFocusDistance(maxNearInFocusDistance);
	m_PostEffectFrame.SetDofMaxNearInFocusDistanceBlendLevel(maxNearInFocusDistanceBlendLevel);
}

void camFirstPersonShooterCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camBaseCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);
	
	const camDepthOfFieldSettingsMetadata* tempCustomSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_DofSettingsForAiming.GetHash());
	if(tempCustomSettings)
	{			
		//Blend to custom DOF settings when actually aiming.
		camDepthOfFieldSettingsMetadata customSettings(*tempCustomSettings);

		//Apply the custom DOF settings specified in the weapon metadata.
		//TODO: If visible popping is reported, consider using a damped spring to smoothly approach our desired settings, be they the defaults or custom settings for a specific weapon.
		const CWeaponInfo* followPedWeaponInfo = camInterface::GetGameplayDirector().GetFollowPedWeaponInfo();
		if(followPedWeaponInfo)
		{
			customSettings.m_SubjectMagnificationPowerFactorNearFar.x	= followPedWeaponInfo->GetFirstPersonDofSubjectMagnificationPowerFactorNear();
			customSettings.m_MaxNearInFocusDistance						= followPedWeaponInfo->GetFirstPersonDofMaxNearInFocusDistance();
			customSettings.m_MaxNearInFocusDistanceBlendLevel			= followPedWeaponInfo->GetFirstPersonDofMaxNearInFocusDistanceBlendLevel();
		}

		const float blendLevel = m_DofAimingBlendSpring.GetResult();		

		BlendDofSettings(blendLevel, settings, customSettings, settings);
	}

	tempCustomSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_DofSettingsForMobile.GetHash());
	if(tempCustomSettings)
	{					
		const float blendLevel = m_DofMobileBlendSpring.GetResult();

		BlendDofSettings(blendLevel, settings, (camDepthOfFieldSettingsMetadata&)*tempCustomSettings, settings);
	}

	tempCustomSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_DofSettingsForParachute.GetHash());
	if(tempCustomSettings)
	{					
		const float blendLevel = m_DofParachuteBlendSpring.GetResult();

		BlendDofSettings(blendLevel, settings, (camDepthOfFieldSettingsMetadata&)*tempCustomSettings, settings);
	}	
}

void camFirstPersonShooterCamera::ProcessEnterCoverOrientation(CPed& rPed, float& heading, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner)
{
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, DO_CUSTOM_ENTER_COVER_BEHAVIOUR, true);
	if (!DO_CUSTOM_ENTER_COVER_BEHAVIOUR )
		return;

	const CTaskEnterCover* pEnterCoverTask = (const CTaskEnterCover*)rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_COVER);
	if (!pEnterCoverTask)
		return;

	Vector3 vCoverDirection(Vector3::ZeroType);
	if (CCover::FindCoverDirectionForPed(rPed, vCoverDirection, VEC3_ZERO))
	{
		INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, -1.0f, 1.0f, 0.01f);
		INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, CAMERA_PROBE_LENGTH, 0.0f, 5.0f, 0.01f);
		INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, FINAL_Z_OFFSET_FOR_CAMERA_PROBE, -1.0f, 1.0f, 0.01f);
		INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, Z_INCREMENT_BETWEEN_CAMERA_PROBES, 0.0f, 1.0f, 0.001f);
		float fCoverDistanceXY = rPed.ComputeLowCoverHeightOffsetFromMover(INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, FINAL_Z_OFFSET_FOR_CAMERA_PROBE, Z_INCREMENT_BETWEEN_CAMERA_PROBES, CAMERA_PROBE_LENGTH);

		TUNE_GROUP_FLOAT(FIST_PERSON_COVER_TUNE, APPLY_FACE_COVER_DISTANCE, 0.75f, -1.0f, 1.0f, 0.01f);
		const float fTimeSinceEnteringCameraLerpRange = pEnterCoverTask->GetTimeSinceEnteringCameraLerpRange();
		if (fTimeSinceEnteringCameraLerpRange <= 0.0f && fCoverDistanceXY > APPLY_FACE_COVER_DISTANCE)
		{
			Vector3 vCoverCoords;
			if (CCover::FindCoverCoordinatesForPed(rPed, vCoverDirection, vCoverCoords))
			{
				Vector3 vPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
				const float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vCoverCoords.x, vCoverCoords.y, vPedPos.x, vPedPos.y);
				TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, FACE_COVER_ENTRY_LERP_TIME, 1.2f, 0.0f, 10.0f, 0.01f);
				const float fLerpRatio = Clamp(pEnterCoverTask->GetTimeInState() / FACE_COVER_ENTRY_LERP_TIME, 0.0f, 1.0f);
				heading = fwAngle::LerpTowards(heading, fDesiredHeading, fLerpRatio);			
				return;
			}
		}

		const float fFacingDirection = CTaskMotionInCover::ComputeFacingHeadingForCoverDirection(vCoverDirection, bFacingLeftInCover);

		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, APPLY_PULL_AROUND_BEHAVIOUR_WHEN_ENTERING, true);
	
		if (rPed.GetControlFromPlayer() && !bInHighCover)
		{
			float fLeftRight = Abs(rPed.GetControlFromPlayer()->GetPedAimWeaponLeftRight().GetNorm());
			float fUpDown = Abs(rPed.GetControlFromPlayer()->GetPedAimWeaponUpDown().GetNorm());
			const bool bMovedCam = (fLeftRight > SMALL_FLOAT || fUpDown> SMALL_FLOAT) ? true : false;
			if (bMovedCam)
			{
				m_bHaveMovedCamDuringCoverEntry = true;
			}
		}

		if (!m_bHaveMovedCamDuringCoverEntry && APPLY_PULL_AROUND_BEHAVIOUR_WHEN_ENTERING)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, ENTRY_LERP_TIME_LOW, 1.2f, 0.0f, 10.0f, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, ENTRY_LERP_TIME_HIGH, 1.2f, 0.0f, 10.0f, 0.01f);
			const float fEntryLerpTime = bInHighCover ? ENTRY_LERP_TIME_HIGH : ENTRY_LERP_TIME_LOW;
			const float fLerpRatio = Clamp(fTimeSinceEnteringCameraLerpRange / fEntryLerpTime, 0.0f, 1.0f);

			float fAngleOffsetMag = 0.0f;
			if (bInHighCover)
			{
				fAngleOffsetMag = bAtCorner ? m_Metadata.m_CoverSettings.m_HighCoverEdgeCameraAngleOffsetDegrees * DtoR : m_Metadata.m_CoverSettings.m_HighCoverWallCameraAngleOffsetDegrees * DtoR;
			}
			else if (rPed.GetPedResetFlag(CPED_RESET_FLAG_ForceScriptedCameraLowCoverAngleWhenEnteringCover))
			{
				fAngleOffsetMag = bAtCorner ? m_Metadata.m_CoverSettings.m_ScriptedLowCoverEdgeCameraAngleOffsetDegrees * DtoR : m_Metadata.m_CoverSettings.m_ScriptedLowCoverWallCameraAngleOffsetDegrees * DtoR;
			}
			else
			{
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, -1.0f, 1.0f, 0.01f);
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, CAMERA_PROBE_LENGTH, 0.0f, 5.0f, 0.01f);
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, FINAL_Z_OFFSET_FOR_CAMERA_PROBE, -1.0f, 1.0f, 0.01f);
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, Z_INCREMENT_BETWEEN_CAMERA_PROBES, 0.0f, 1.0f, 0.001f);
				rPed.ComputeLowCoverHeightOffsetFromMover(INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, FINAL_Z_OFFSET_FOR_CAMERA_PROBE, Z_INCREMENT_BETWEEN_CAMERA_PROBES, CAMERA_PROBE_LENGTH);

				float fLowCoverHeightTValue = GetLowCoverHeightTValue(rPed);
				if (bAtCorner)
				{
					fAngleOffsetMag = Lerp(fLowCoverHeightTValue, m_Metadata.m_CoverSettings.m_MinLowCoverEdgeCameraAngleOffsetDegrees, m_Metadata.m_CoverSettings.m_MaxLowCoverEdgeCameraAngleOffsetDegrees);
				}
				else
				{
					fAngleOffsetMag = Lerp(fLowCoverHeightTValue, m_Metadata.m_CoverSettings.m_MinLowCoverWallCameraAngleOffsetDegrees, m_Metadata.m_CoverSettings.m_MaxLowCoverWallCameraAngleOffsetDegrees);
				}

				fAngleOffsetMag *= DtoR;
			}

			float fAngleOffset = bFacingLeftInCover ? fAngleOffsetMag : -fAngleOffsetMag;
			float fDesiredHeading = fwAngle::LimitRadianAngle(fFacingDirection + fAngleOffset);
			heading = fwAngle::LerpTowards(heading, fDesiredHeading, fLerpRatio);
		}
	}
}

float camFirstPersonShooterCamera::GetLowCoverHeightTValue(const CPed& rPed) const
{
	return ClampRange(rPed.GetLowCoverHeightOffsetFromMover(), m_Metadata.m_CoverSettings.m_MinLowCoverHeightOffsetFromMover, m_Metadata.m_CoverSettings.m_MaxLowCoverHeightOffsetFromMover);
}

float camFirstPersonShooterCamera::CalculateCoverAutoHeadingCorrectionAngleOffset(const CPed& rPed, float pitch, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner) const
{
	float fLowCoverHeightTValue = GetLowCoverHeightTValue(rPed);

	float fAngleOffset = 0.0f;
	if (bInHighCover)
	{
		fAngleOffset = bAtCorner ? m_Metadata.m_CoverSettings.m_HighCoverEdgeCameraAngleOffsetDegrees : m_Metadata.m_CoverSettings.m_HighCoverWallCameraAngleOffsetDegrees;
	}
	else
	{
		float fLowCoverPitchTValue = 1.0f - ClampRange(pitch, m_Metadata.m_CoverSettings.m_MinLowCoverPitch, m_Metadata.m_CoverSettings.m_MaxLowCoverPitch);
		float fFinalTValue = fLowCoverHeightTValue * fLowCoverPitchTValue;

		if (bAtCorner)
		{
			fAngleOffset = Lerp(fFinalTValue, m_Metadata.m_CoverSettings.m_MinLowCoverEdgeCameraAngleOffsetDegrees, m_Metadata.m_CoverSettings.m_MaxLowCoverEdgeCameraAngleOffsetDegrees);
		}
		else
		{
			fAngleOffset = Lerp(fFinalTValue, m_Metadata.m_CoverSettings.m_MinLowCoverWallCameraAngleOffsetDegrees, m_Metadata.m_CoverSettings.m_MaxLowCoverWallCameraAngleOffsetDegrees);
		}
	}

	fAngleOffset *= DtoR;

	if (!bFacingLeftInCover)
	{
		fAngleOffset = -fAngleOffset;
	}

	return fAngleOffset;
}

bool camFirstPersonShooterCamera::IsAimingDirectly(const CPed& rPed, float fAngleBetweenCoverAndCamera, float fPitch, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, float fThresholdModifier) const
{
	float fAngleOffset = CalculateCoverAutoHeadingCorrectionAngleOffset(rPed, fPitch, bFacingLeftInCover, bInHighCover, bAtCorner);
	float fMaxAngleOffset = bAtCorner ? m_Metadata.m_CoverSettings.m_MaxCameraAngleFromEdgeCoverForAutoHeadingCorrectionDegreesOffset : m_Metadata.m_CoverSettings.m_MaxCameraAngleFromWallCoverForAutoHeadingCorrectionDegreesOffset;
	fMaxAngleOffset *= DtoR;

	float fMaxAngleBetweenCoverAndCamera = HALF_PI + fMaxAngleOffset;
	fMaxAngleBetweenCoverAndCamera -= Abs(fAngleOffset) * fThresholdModifier;

	// Pretend we're aiming directly if not quite at the max angle threshold (adjusted with LIMIT_ANGLE_MODIFIER)
	if (Abs(fAngleBetweenCoverAndCamera) > fMaxAngleBetweenCoverAndCamera)
	{
		return true;
	}

	return false;
}

void camFirstPersonShooterCamera::ProcessCoverAutoHeadingCorrection(CPed& rPed, float& heading, float pitch, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, bool bAimDirectly)
{
	eCoverHeadingCorrectionType eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_INVALID;
	float fDesiredHeading = heading;
	float fCoverHeading = 0.0f;

	const camFirstPersonAimCameraMetadataHeadingCorrection* desiredHeadingCorrection = &m_Metadata.m_CoverSettings.m_HeadingCorrection;

	bool bDesiredFacingLeftInCover = bFacingLeftInCover;

	bool bLookAroundInput = m_CurrentControlHelper->IsLookAroundInputPresent();

	Vector3 vCoverDir(Vector3::ZeroType);
	if (CCover::FindCoverDirectionForPed(rPed, vCoverDir, VEC3_ZERO))
	{
		fCoverHeading = rage::Atan2f(-vCoverDir.x, vCoverDir.y);

		m_CoverVaultWasFacingLeft  = (fwAngle::LimitRadianAngle(m_CameraHeading - fCoverHeading) >= 0.0f);
		m_CoverVaultWasFacingRight = (fwAngle::LimitRadianAngle(m_CameraHeading - fCoverHeading) <  0.0f);

		const float fAngleBetweenCoverAndCamera = SubtractAngleShorter(heading, fCoverHeading);

		bool bIsDoingCoverAimOutro = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverOutroToPeek);
		const CTaskAimGunBlindFire* pAimGunBlindFireTask = static_cast<const CTaskAimGunBlindFire*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
		const bool bIsAiming = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) || (rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN) != NULL);
		
		bool bIsBlindFiring = false;
		bool bBlindFiringAtEdge = false;
		if(pAimGunBlindFireTask)
		{
			bIsBlindFiring = true;
			bBlindFiringAtEdge = pAimGunBlindFireTask->IsUsingAtEdgeAnims();
		}

		if (!bIsDoingCoverAimOutro)
		{
			if (bIsBlindFiring && pAimGunBlindFireTask->ShouldBeDoingOutro())
			{
				bIsDoingCoverAimOutro = true;
			}
		}

		bool bIsPeeking = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsPeekingFromCover) || rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverOutroToPeek);

		if ((bIsAiming || bIsBlindFiring || bIsPeeking) && !bIsDoingCoverAimOutro && (!bInHighCover || bAtCorner))
		{
			// If we haven't yet started applying heading correction then don't start applying it if just free aiming (or not aiming)
			desiredHeadingCorrection = &m_Metadata.m_AimHeadingCorrection;

			// If the input time hasn't yet been set or hasn't yet expired then don't check input
			if (bInHighCover && ((m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_INTRO && m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_AIM) || 
				m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_INTRO || fwTimer::GetTimeInMilliseconds() < m_CoverHeadingCorrectIdleInputTime))
			{
				bLookAroundInput = false;
			}

			float fLowCoverHeightTValue = GetLowCoverHeightTValue(rPed);

			bool bDesiredAimDirectly = bAimDirectly;
			if (!bDesiredAimDirectly)
			{
				TUNE_GROUP_FLOAT(FPS_COVER_TUNE, LIMIT_ANGLE_AIM_BLIND_FIRE_MODIFIER, 0.9f, 0.0f, 2.0f, 0.01f);
				float fThresholdMultiplier = LIMIT_ANGLE_AIM_BLIND_FIRE_MODIFIER;
				if (bIsPeeking)
				{
					TUNE_GROUP_FLOAT(FPS_COVER_TUNE, LIMIT_ANGLE_PEEK_MODIFIER, 0.25f, 0.0f, 2.0f, 0.01f);
					fThresholdMultiplier = LIMIT_ANGLE_PEEK_MODIFIER;
				}

				bDesiredAimDirectly = IsAimingDirectly(rPed, fAngleBetweenCoverAndCamera, pitch, bDesiredFacingLeftInCover, bInHighCover, bAtCorner, fThresholdMultiplier);
			}

			// Don't correct our aim if we're ignoring cover auto heading correction, have look-around input, are locked on, are behind very low cover or have just stopped using a sniper scope
			// But still keep remembering our desired heading in case we do want to start applying the correction
			if (!bLookAroundInput && !m_IsLockedOn && !bDesiredAimDirectly && (bInHighCover || fLowCoverHeightTValue > 0.0f) && !camInterface::GetGameplayDirector().GetJustSwitchedFromSniperScope())
			{
				if (m_CoverHeadingCorrectionDetectedStickInput && rPed.GetPlayerInfo() != NULL)
				{
					rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(fDesiredHeading);
					m_CoverHeadingCorrectionDetectedStickInput = false;
				}

				// If the cover heading has been reset (i.e. we haven't been aiming yet since entering cover or we switched sides) or
				// we're just starting to blind-firing at an angle past our cover heading...
				fDesiredHeading = rPed.GetPlayerInfo() != NULL ? rPed.GetPlayerInfo()->GetCoverHeadingCorrectionAimHeading() : FLT_MAX;
				if (fDesiredHeading == FLT_MAX || (bBlindFiringAtEdge && m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_INTRO && m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_AIM &&
					rPed.GetPedResetFlag(CPED_RESET_FLAG_IsBlindFiring) && ((bFacingLeftInCover && ((fDesiredHeading - fCoverHeading) < 0.0f)) || (!bFacingLeftInCover && ((fDesiredHeading - fCoverHeading) > 0.0f)))))
				{
					// Then just use the cover direction directly!
					fDesiredHeading = fCoverHeading;

					// If in high cover at a corner with an angle greater than 90 degrees we need to adjust our desired heading so that
					// we don't end up aiming at the corner we're going around
					if (bInHighCover && bAtCorner && rPed.GetPlayerInfo() != NULL)
					{
						bool bSaveDynamicCoverInsideCorner = rPed.GetPlayerInfo()->DynamicCoverInsideCorner();
						bool bSaveDynamicCoverCanMoveRight = rPed.GetPlayerInfo()->DynamicCoverCanMoveRight();
						bool bSaveDynamicCoverCanMoveLeft = rPed.GetPlayerInfo()->DynamicCoverCanMoveLeft();
						bool bSaveDynamicCoverHitPed = rPed.GetPlayerInfo()->DynamicCoverHitPed();
						bool bSaveDynamicCoverGeneratedByDynamicEntity = rPed.GetPlayerInfo()->IsCoverGeneratedByDynamicEntity();

						CCoverPoint cornerCoverPoint;
						if (CPlayerInfo::ms_DynamicCoverHelper.FindNewCoverPointAroundCorner(&cornerCoverPoint, &rPed, bFacingLeftInCover, !bInHighCover, true))
						{
							Vector3 vCornerCoverHeading = VEC3V_TO_VECTOR3(cornerCoverPoint.GetCoverDirectionVector(NULL));

							float fDotCornerCover = vCoverDir.Dot(vCornerCoverHeading);
							if (fDotCornerCover > 0.0f)
							{
								float fAngleToAdjust = HALF_PI - AcosfSafe(fDotCornerCover);
								fDesiredHeading += bFacingLeftInCover ? fAngleToAdjust : -fAngleToAdjust;
								fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
							}
						}

						rPed.GetPlayerInfo()->SetDynamicCoverInsideCorner(bSaveDynamicCoverInsideCorner);
						rPed.GetPlayerInfo()->SetDynamicCoverCanMoveRight(bSaveDynamicCoverCanMoveRight);
						rPed.GetPlayerInfo()->SetDynamicCoverCanMoveLeft(bSaveDynamicCoverCanMoveLeft);
						rPed.GetPlayerInfo()->SetDynamicCoverHitPed(bSaveDynamicCoverHitPed);
						rPed.GetPlayerInfo()->SetCoverGeneratedByDynamicEntity(bSaveDynamicCoverGeneratedByDynamicEntity);

						fDesiredHeading += bFacingLeftInCover ? m_Metadata.m_HighCoverEdgeCameraAngleAimOffsetDegrees * DtoR : -m_Metadata.m_HighCoverEdgeCameraAngleAimOffsetDegrees * DtoR;
						fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
					}
				}
			}

			if ((m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_INTRO && fwTimer::GetTimeInMilliseconds() >= m_CoverHeadingCorrectIdleInputTime) ||
				m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_AIM)
			{
				eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_AIM;
			}
			else
			{
				eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_INTRO;
			}

			if (rPed.GetPlayerInfo() != NULL)
			{
				if (bInHighCover && bDesiredAimDirectly)
				{
					rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
				}
				else
				{
					rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(fDesiredHeading);
				}
			}
		}
		else
		{
			const CTaskMotionInCover* pMotionCoverTask = static_cast<const CTaskMotionInCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOTION_IN_COVER));
			if (bIsDoingCoverAimOutro)
			{
				eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_OUTRO;
			}
			else if (pMotionCoverTask != NULL)
			{
				s32 iMotionCoverTaskState = pMotionCoverTask->GetState();
				if (iMotionCoverTaskState == CTaskMotionInCover::State_Idle || iMotionCoverTaskState == CTaskMotionInCover::State_Settle ||
					iMotionCoverTaskState == CTaskMotionInCover::State_AtEdge || iMotionCoverTaskState == CTaskMotionInCover::State_PlayingIdleVariation)
				{
					eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_IDLE;
				}
				else if (pMotionCoverTask->GetParent() && pMotionCoverTask->GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER && 
					static_cast<const CTaskInCover*>(pMotionCoverTask->GetParent())->IsInMovingMotionState(false))
				{
					eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_MOVING;
				}
				else if (iMotionCoverTaskState == CTaskMotionInCover::State_TurnEnter || iMotionCoverTaskState == CTaskMotionInCover::State_Turning ||
					iMotionCoverTaskState == CTaskMotionInCover::State_TurnEnd)
				{
					eDesiredCoverHeadingCorrectionState = COVER_HEADING_CORRECTION_TURN;
				}
			}

			// If the camera was facing the wrong direction when starting the turn we need to start correcting the aim heading during the
			// turn enter where we haven't yet switched the facing direction so we need to reverse the facing direction ourselves here
			if (eDesiredCoverHeadingCorrectionState == COVER_HEADING_CORRECTION_TURN)
			{
				bool bLeftStickTriggeredTurn = false;

				// The facing direction doesn't get set until part way through the turn so we need to force it here depending on the direction
				// we're turning so that the camera swings around
				if (pMotionCoverTask != NULL)
				{
					bDesiredFacingLeftInCover = pMotionCoverTask->IsTurningLeft();
					// Recalculate whether we're at a corner or not
					bAtCorner = CTaskInCover::CanPedFireRoundCorner(rPed, bDesiredFacingLeftInCover);
					bLeftStickTriggeredTurn = pMotionCoverTask->IsLeftStickTriggeredTurn();
				}

				// Don't reset the stored cover heading when turning if our last state was aiming or we're in low cover and this wasn't a left stick triggered turn
				if (rPed.GetPlayerInfo() != NULL && bLeftStickTriggeredTurn && m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_TURN &&
					m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_AIM && m_CoverHeadingCorrectionPreviousState != COVER_HEADING_CORRECTION_AIM)
				{
					// Reset the stored cover heading so that when we start aiming again we'll just set the cover heading to the current heading
					rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
					m_CoverHeadingCorrectionDetectedStickInput = false;
				}
			}
			else if (!bInHighCover && (eDesiredCoverHeadingCorrectionState == COVER_HEADING_CORRECTION_MOVING || m_NumUpdatesPerformed == 0))
			{
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, -1.0f, 1.0f, 0.01f);
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, CAMERA_PROBE_LENGTH, 0.0f, 5.0f, 0.01f);
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, FINAL_Z_OFFSET_FOR_CAMERA_PROBE, -1.0f, 1.0f, 0.01f);
				INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, Z_INCREMENT_BETWEEN_CAMERA_PROBES, 0.0f, 1.0f, 0.001f);
				rPed.ComputeLowCoverHeightOffsetFromMover(INITIAL_Z_OFFSET_FOR_CAMERA_PROBE, FINAL_Z_OFFSET_FOR_CAMERA_PROBE, Z_INCREMENT_BETWEEN_CAMERA_PROBES, CAMERA_PROBE_LENGTH);
			}

			float fMaxAngleOffset = bAtCorner ? m_Metadata.m_CoverSettings.m_MaxCameraAngleFromEdgeCoverForAutoHeadingCorrectionDegreesOffset : m_Metadata.m_CoverSettings.m_MaxCameraAngleFromWallCoverForAutoHeadingCorrectionDegreesOffset;
			fMaxAngleOffset *= DtoR;

			float fAngleOffset = CalculateCoverAutoHeadingCorrectionAngleOffset(rPed, pitch, bDesiredFacingLeftInCover, bInHighCover, bAtCorner);
			float fAbsAngleOffset = Abs(fAngleOffset);

			float fMaxAngleBetweenCoverAndCamera = HALF_PI - fAbsAngleOffset + fMaxAngleOffset;

			// If we're currently moving and had previously been moving or were previously moving and haven't looked around
			// and our cover correction angle offset just increased then try and force the camera to the angle offset until we stop moving or start looking around
			if (((eDesiredCoverHeadingCorrectionState == COVER_HEADING_CORRECTION_MOVING && m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_MOVING) ||
				((m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_MOVING || m_CoverHeadingCorrectionPreviousState == COVER_HEADING_CORRECTION_MOVING) && !bLookAroundInput)) &&
				(fAbsAngleOffset - m_CoverHeadingLastAngleOffset) > 0.05f)
			{
				fMaxAngleBetweenCoverAndCamera = HALF_PI;
			}
			else
			{
				m_CoverHeadingLastAngleOffset = fAbsAngleOffset;
			}

			fMaxAngleBetweenCoverAndCamera = fwAngle::LimitRadianAngle(fMaxAngleBetweenCoverAndCamera);

			float fAngleLimit = m_Metadata.m_CoverSettings.m_MaxLowCoverWallCameraAngleOffsetDegrees * DtoR;
			if (fAngleBetweenCoverAndCamera > 0.0f)
			{
				fAngleLimit = -fAngleLimit;
			}
			if (!bInHighCover && (eDesiredCoverHeadingCorrectionState == COVER_HEADING_CORRECTION_MOVING || eDesiredCoverHeadingCorrectionState == COVER_HEADING_CORRECTION_TURN))
			{
				const float fFacingDirection = CTaskMotionInCover::ComputeFacingHeadingForCoverDirection(fCoverHeading, fAngleBetweenCoverAndCamera > 0.0f);
				float fDesiredLimitHeading = fwAngle::LimitRadianAngle(fFacingDirection - fAngleLimit);

				// Check if our heading is outside the limits
				if ((fAngleBetweenCoverAndCamera > 0.0f && fAngleBetweenCoverAndCamera > (HALF_PI - fAngleLimit) && !bDesiredFacingLeftInCover) || (fAngleBetweenCoverAndCamera < 0.0f && fAngleBetweenCoverAndCamera < (-HALF_PI - fAngleLimit) && bDesiredFacingLeftInCover))
				{
					static bool bUseSpring = false;
					if (bUseSpring)
					{
						fDesiredHeading = fDesiredLimitHeading;
					}
					else
					{
						// Compute the desired facing direction
						TUNE_GROUP_FLOAT(FPS_COVER_TUNE, MOVING_TURNING_LIMIT_ANGLE_RATE, 4.0f, 0.0f, 30.0f, 0.01f);
						// Just do a linear blend in this case since we want a hard clamp
						float fDifference = 0.0f;
						Approach(fDifference, SubtractAngleShorter(fDesiredLimitHeading, heading), MOVING_TURNING_LIMIT_ANGLE_RATE, fwTimer::GetCamTimeStep());
						heading = fDesiredHeading = fwAngle::LimitRadianAngle(heading + fDifference);
					}
				}
			}

			if (!bLookAroundInput)
			{
				if (eDesiredCoverHeadingCorrectionState != COVER_HEADING_CORRECTION_INVALID)
				{
					if (Abs(fAngleBetweenCoverAndCamera) < fMaxAngleBetweenCoverAndCamera || (bDesiredFacingLeftInCover != bFacingLeftInCover && fMaxAngleBetweenCoverAndCamera > 0.0f))
					{
						// Compute the desired facing direction
						const float fFacingDirection = CTaskMotionInCover::ComputeFacingHeadingForCoverDirection(fCoverHeading, bDesiredFacingLeftInCover);
						fDesiredHeading = fwAngle::LimitRadianAngle(fFacingDirection + fAngleOffset);

						// If we're in the idle state...
						if (rPed.GetPlayerInfo() != NULL && m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_IDLE)
						{
							TUNE_GROUP_FLOAT(FPS_COVER_TUNE, RESET_THRESHOLD_MODIFIER, 1.05f, 0.0f, 2.0f, 0.01f);
							// If we've detected stick input and the aim is past a given threshold...
							if (m_CoverHeadingCorrectionDetectedStickInput && !bAimDirectly && 
								!IsAimingDirectly(rPed, fAngleBetweenCoverAndCamera, pitch, bDesiredFacingLeftInCover, bInHighCover, bAtCorner, RESET_THRESHOLD_MODIFIER))
							{
								// Reset the stored cover heading so that when we start aiming again we'll just set the cover heading to the current heading
								rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
							}

							// If the idle timer has expired (i.e. we're going to start correcting our heading) plus an extra fudge factor to avoid cases where someone starts to aim as the timer ends
							// and we detected that we passed the reset threshold at some point (the heading correction has been reset)...
							if (fwTimer::GetTimeInMilliseconds() >= (m_CoverHeadingCorrectIdleInputTime + m_Metadata.m_CoverSettings.m_ExtraTimeDeltaForAutoHeadingCorrectionResetMS) &&
								rPed.GetPlayerInfo()->GetCoverHeadingCorrectionAimHeading() == FLT_MAX)
							{
								// Reset the flag indicating there has been stick input so that the next time we aim we'll use the reset aim heading
								m_CoverHeadingCorrectionDetectedStickInput = false;
							}
						}
					}
				}
				else if (rPed.GetPlayerInfo() != NULL)
				{
					// Reset the stored cover heading so that when we start aiming again we'll just set the cover heading to the current heading
					rPed.GetPlayerInfo()->SetCoverHeadingCorrectionAimHeading(FLT_MAX);
				}
			}
			// Never correct the heading if the right stick has been adjusted
			else
			{
				m_CoverHeadingCorrectionDetectedStickInput = true;
			}
		}
	}

	// If we're changing state then reset timer and spring
	// If the desired state is idle and we're currently looking around then reset the timer and spring so that once we stop looking around we'll only correct once the timer is up
	if (eDesiredCoverHeadingCorrectionState != m_CoverHeadingCorrectionState || (eDesiredCoverHeadingCorrectionState == COVER_HEADING_CORRECTION_IDLE && bLookAroundInput))
	{
		m_CoverHeadingCorrectIdleInputTime = 0;
		m_RelativeCoverHeadingSpring.Reset(heading);
		m_CoverHeadingCorrectionPreviousState = m_CoverHeadingCorrectionState;
		m_CoverHeadingCorrectionState = eDesiredCoverHeadingCorrectionState;
	}

	if (m_CoverHeadingCorrectionState != COVER_HEADING_CORRECTION_INVALID)
	{
		if (m_CoverHeadingCorrectIdleInputTime == 0)
		{
			// When coming back from aiming the outro state often finishes before the aim has reached its desired heading. In that particular case we
			// don't want to wait for the timer to expire before finishing the aim correction...
			if (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_IDLE && m_CoverHeadingCorrectionPreviousState == COVER_HEADING_CORRECTION_OUTRO)
			{
				m_CoverHeadingCorrectIdleInputTime = fwTimer::GetTimeInMilliseconds();
			}
			// When coming back from peeking we don't go through the 'outro' state since there isn't really any information to tell us that we're leaving a peek
			// Instead we just assume that if we're in the idle state and we were in the aiming state that we should be using the outro timer tuning
			else if (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_OUTRO || (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_IDLE && m_CoverHeadingCorrectionPreviousState == COVER_HEADING_CORRECTION_AIM))
			{
				m_CoverHeadingCorrectIdleInputTime = fwTimer::GetTimeInMilliseconds() + m_Metadata.m_CoverSettings.m_MinTimeDeltaForAutoHeadingCorrectionAimOutroMS;
			}
			else if (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_INTRO)
			{
				m_CoverHeadingCorrectIdleInputTime = fwTimer::GetTimeInMilliseconds() + m_Metadata.m_CoverSettings.m_MinTimeDeltaForAutoHeadingCorrectionAimIntroMS;
			}
			else if (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_AIM)
			{
				m_CoverHeadingCorrectIdleInputTime = fwTimer::GetTimeInMilliseconds() + m_Metadata.m_CoverSettings.m_MinTimeDeltaForAutoHeadingCorrectionAimMS;
			}
			else if (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_MOVING || m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_TURN)
			{
				m_CoverHeadingCorrectIdleInputTime = fwTimer::GetTimeInMilliseconds() + m_Metadata.m_CoverSettings.m_MinTimeDeltaForAutoHeadingCorrectionMoveTurnMS;
			}
			else
			{
				m_CoverHeadingCorrectIdleInputTime = fwTimer::GetTimeInMilliseconds() + m_Metadata.m_CoverSettings.m_MinTimeDeltaForAutoHeadingCorrectionMS;
			}
		}

		// The timer is handled differently in the aim state.  We use the timer to determine when to start allowing user input rather than to determine when
		// to start applying the heading correction
		bool bInstantCorrection = m_NumUpdatesPerformed == 0 || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
		if (m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_AIM || fwTimer::GetTimeInMilliseconds() >= m_CoverHeadingCorrectIdleInputTime ||
			(m_CoverHeadingCorrectionState == COVER_HEADING_CORRECTION_IDLE && bInstantCorrection))
		{
			// This ensures we always rotate through the cover direction (even if it's the longer way around!)
			float fHeadingDifference = SubtractAngleShorter(fCoverHeading, heading) + SubtractAngleShorter(fDesiredHeading, fCoverHeading);

			if(bInstantCorrection)
			{
				m_RelativeCoverHeadingSpring.OverrideResult(fHeadingDifference);
			}
			else
			{
				m_RelativeCoverHeadingSpring.OverrideResult(0.0f);
			}

			// Update the heading value based on the spring model
			heading	+= m_RelativeCoverHeadingSpring.Update(fHeadingDifference, desiredHeadingCorrection->m_SpringConstant, desiredHeadingCorrection->m_AwayDampingRatio);

			// Translate back to between -PI and PI to compare the two angles below
			heading = fwAngle::LimitRadianAngle(heading);
		}
	}
}

void camFirstPersonShooterCamera::ProcessCoverPitchRestriction(const CPed& rPed, float heading, float& pitch, bool bInHighCover)
{
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, ENABLE_VARIABLE_PITCH_LIMIT, true);
	if (!ENABLE_VARIABLE_PITCH_LIMIT)
		return;

	float fDesiredPitch = pitch;
	if (!rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsPeekingFromCover) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsBlindFiring))
	{
		// Compute pitch limit based on heading
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MIN_LOW_COVER_PITCH_LOWER_LIMIT, -1.0f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_LOW_COVER_PITCH_LOWER_LIMIT, -0.1f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MIN_HIGH_COVER_PITCH_LOWER_LIMIT, -1.0f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_HIGH_COVER_PITCH_LOWER_LIMIT, -0.1f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PROJECTILE_AIM_DIRECTLY_MIN_LOW_COVER_PITCH_LOWER_LIMIT, -1.0f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PROJECTILE_AIM_DIRECTLY_MAX_LOW_COVER_PITCH_LOWER_LIMIT, -0.2f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PROJECTILE_AIM_DIRECTLY_MIN_HIGH_COVER_PITCH_LOWER_LIMIT, -1.0f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PROJECTILE_AIM_DIRECTLY_MAX_HIGH_COVER_PITCH_LOWER_LIMIT, -0.3f, -HALF_PI, HALF_PI, 0.01f);
		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, ENABLE_VARIABLE_PITCH_LIMIT_WHEN_AIMING_DIRECT_PROJECTILE, true);

		float fCoverHeading = CTaskMotionInCover::ComputeCoverHeading(rPed);
		Assert(FPIsFinite(fCoverHeading));

		bool bProjectileAimDirectly = false;
		if (ENABLE_VARIABLE_PITCH_LIMIT_WHEN_AIMING_DIRECT_PROJECTILE && rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THROW_PROJECTILE))
		{
			if (Abs(fwAngle::LimitRadianAngle(fCoverHeading - heading)) > HALF_PI)
			{
				bProjectileAimDirectly = true;
			}
		}

		fCoverHeading = fwAngle::LimitRadianAngleSafe(fCoverHeading + (bProjectileAimDirectly ? 0.0f : PI));		// Add pi to get max gaussian value when facing wall
		Assert(FPIsFinite(heading));
		float fDeltaHeading = fwAngle::LimitRadianAngleSafe(fCoverHeading - heading);
		fDeltaHeading += PI;
		fDeltaHeading /= TWO_PI;
		float fY = Clamp(CTaskMotionInCover::ComputeGaussianValueForRestrictedCameraPitch(fDeltaHeading), 0.0f, 1.0f);
		const float fMinHighCoverPitchLowerLimit = bProjectileAimDirectly ? PROJECTILE_AIM_DIRECTLY_MIN_HIGH_COVER_PITCH_LOWER_LIMIT : MIN_HIGH_COVER_PITCH_LOWER_LIMIT;
		const float fMaxHighCoverPitchLowerLimit = bProjectileAimDirectly ? PROJECTILE_AIM_DIRECTLY_MAX_HIGH_COVER_PITCH_LOWER_LIMIT : MAX_HIGH_COVER_PITCH_LOWER_LIMIT;
		const float fMinLowCoverPitchLowerLimit = bProjectileAimDirectly ? PROJECTILE_AIM_DIRECTLY_MIN_LOW_COVER_PITCH_LOWER_LIMIT : MIN_LOW_COVER_PITCH_LOWER_LIMIT;
		const float fMaxLowCoverPitchLowerLimit = bProjectileAimDirectly ? PROJECTILE_AIM_DIRECTLY_MAX_LOW_COVER_PITCH_LOWER_LIMIT : MAX_LOW_COVER_PITCH_LOWER_LIMIT;
		const float fMinCoverPitchLowerLimit = bInHighCover ? fMinHighCoverPitchLowerLimit : fMinLowCoverPitchLowerLimit;
		const float fMaxCoverPitchLowerLimit = bInHighCover ? fMaxHighCoverPitchLowerLimit : fMaxLowCoverPitchLowerLimit;

		const float fMinLimit = (1.0f - fY) * fMaxCoverPitchLowerLimit + fY * fMinCoverPitchLowerLimit;
		if (fDesiredPitch < fMinLimit)
		{
			fDesiredPitch = fMinLimit;
		}
	}

	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PITCH_LIMIT_APPROACH_RATE, 2.0f, 0.0f, 10.0f, 0.01f);
	Approach(pitch, fDesiredPitch, PITCH_LIMIT_APPROACH_RATE, fwTimer::GetTimeStep());
}

void camFirstPersonShooterCamera::ProcessCoverRoll(const CPed& rPed, float& roll, float heading, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, bool bAimDirectly)
{
	float fDesiredPeekRoll = 0.0f;

	bool bBlindFire = false;
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, BLIND_FIRE_ROLL, true);
	const CTaskAimGunBlindFire* pAimGunBlindFireTask = static_cast<const CTaskAimGunBlindFire*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
	if (BLIND_FIRE_ROLL && pAimGunBlindFireTask != NULL && !pAimGunBlindFireTask->ShouldBeDoingOutro() && pAimGunBlindFireTask->IsUsingAtEdgeAnims())
	{
		bBlindFire = true;
	}

	bool bAimToPeek = false;
	bool bPeekToAim = false;
	if (!bBlindFire)
	{
		if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverOutroToPeek))
		{
			bAimToPeek = true;
		}
		else if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) || rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN) != NULL)
		{
			bPeekToAim = true;
		}
	}

	Vector3 vCoverDir(Vector3::ZeroType);
	if ((rPed.GetPedResetFlag(CPED_RESET_FLAG_IsPeekingFromCover) || bBlindFire || bAimToPeek) && bInHighCover && bAtCorner && !bAimDirectly && CCover::FindCoverDirectionForPed(rPed, vCoverDir, VEC3_ZERO))
	{
		INSTANTIATE_TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_PEEK_ROLL, -180.0f, 180.0f, 0.05f);
		fDesiredPeekRoll = (bFacingLeftInCover ? -MAX_PEEK_ROLL : MAX_PEEK_ROLL) * DtoR;

		Vector3 vHeading(-rage::Sinf(heading), rage::Cosf(heading), 0.0f);
		
		float fDotProduct = vHeading.Dot(vCoverDir);
		fDesiredPeekRoll = RampValue(fDotProduct, 0.0f, 1.0f, 0.0f, fDesiredPeekRoll);
	}

	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PEEK_ROLL_BLEND_IN_RATE, 20.0f, 0.0f, 180.0f, 0.05f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PEEK_ROLL_BLEND_IN_FROM_AIMING_RATE, 20.0f, 0.0f, 180.0f, 0.05f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PEEK_ROLL_BLEND_OUT_RATE, 25.0f, 0.0f, 180.0f, 0.05f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, PEEK_ROLL_BLEND_OUT_TO_AIMING_RATE, 25.0f, 0.0f, 180.0f, 0.05f);

	float fPeekRollRate = bPeekToAim ? PEEK_ROLL_BLEND_OUT_TO_AIMING_RATE * DtoR : PEEK_ROLL_BLEND_OUT_RATE * DtoR;
	if (Abs(fDesiredPeekRoll) > Abs(m_RelativePeekRoll))
	{
		if (bAimToPeek)
		{
			fPeekRollRate = PEEK_ROLL_BLEND_IN_FROM_AIMING_RATE * DtoR;
		}
		else
		{
			fPeekRollRate = PEEK_ROLL_BLEND_IN_RATE * DtoR;
		}
	}

	Approach(m_RelativePeekRoll, fDesiredPeekRoll, fPeekRollRate, fwTimer::GetTimeStep());
	roll = m_RelativePeekRoll;
}

void camFirstPersonShooterCamera::ProcessRelativeAttachOffset(const CPed* pPed)
{
	if (!pPed)
		return;

	const Vector3 vDesiredOffset = ComputeDesiredAttachOffset(*pPed);
	const float fTimeStep = fwTimer::GetTimeStep();
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, XY_OFFSET_APPROACH_RATE, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, Z_OFFSET_APPROACH_RATE, 1.0f, 0.0f, 10.0f, 0.01f);
	Approach(m_RelativeCoverOffset.x, vDesiredOffset.x, XY_OFFSET_APPROACH_RATE, fTimeStep);
	Approach(m_RelativeCoverOffset.y, vDesiredOffset.y, XY_OFFSET_APPROACH_RATE, fTimeStep);
	Approach(m_RelativeCoverOffset.z, vDesiredOffset.z, Z_OFFSET_APPROACH_RATE, fTimeStep);
	//Displayf("m_RelativeCoverOffset = (%.2f,%.2f,%.2f", m_RelativeCoverOffset.x, m_RelativeCoverOffset.y, m_RelativeCoverOffset.z);
}

Vector3 camFirstPersonShooterCamera::ComputeDesiredAttachOffset( const CPed& rPed)
{
	Vector3 vDesiredOffset(Vector3::ZeroType);

	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, DO_CUSTOM_IN_COVER_BEHAVIOUR, true);
	if (!DO_CUSTOM_IN_COVER_BEHAVIOUR )
		return vDesiredOffset;

	const CTaskCover* pCoverTask = (const CTaskCover*)rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER);
	if (!pCoverTask)
		return vDesiredOffset;

	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, LOW_COVER_Z_OFFSET, 0.08f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, HIGH_COVER_Z_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);

	const bool bIsHighCover = pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint);

	// Aim height
	const bool bIsMovingOutToAimOrAiming = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover);
	if (!bIsMovingOutToAimOrAiming && !CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonAimingAnimations)
	{
		vDesiredOffset.z = bIsHighCover ? HIGH_COVER_Z_OFFSET : LOW_COVER_Z_OFFSET;
	}

	// Extra side offset for high cover to help prevent clipping with wall
	TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, HIGH_COVER_SIDE_OFFSET, 0.1f, 0.0f, 1.0f, 0.01f);
	Vector3 vWorldSpaceOffset(Vector3::ZeroType);
	Vector3 vTargetDirUnused;
	Vector3 vCoverDirection(Vector3::ZeroType);
	if (bIsHighCover && CCover::FindCoverDirectionForPed(rPed, vCoverDirection, vTargetDirUnused))
	{
		const float fOffset = CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonLocoAnimations ? 0.0f : HIGH_COVER_SIDE_OFFSET; 
		vCoverDirection.z = 0.0f;
		vCoverDirection.Normalize();
		vCoverDirection *= -fOffset;
		vWorldSpaceOffset += vCoverDirection;
	}

	const CTaskInCover* pInCoverInTask = (const CTaskInCover*)rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER);
	const CTaskMotionInCover* pMotionCoverInTask = (const CTaskMotionInCover*)rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOTION_IN_COVER);
	if (pMotionCoverInTask && pMotionCoverInTask->GetState() == CTaskMotionInCover::State_Peeking)
	{
		if (bIsHighCover && !CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonAimingAnimations)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, HIGH_COVER_PEEK_SIDE_OFFSET, 0.1f, 0.0f, 1.0f, 0.01f);
			Vector3 vCoverPeekSideOffset = VEC3V_TO_VECTOR3(rPed.GetTransform().GetB());
			vCoverPeekSideOffset.z = 0.0f;
			vCoverPeekSideOffset.Normalize();
			vCoverPeekSideOffset *= HIGH_COVER_PEEK_SIDE_OFFSET;
			vWorldSpaceOffset += vCoverPeekSideOffset;
		}
		else if (pInCoverInTask && !CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonAimingAnimations)
		{
			const float fPeekHeightOffsetFromMover = pInCoverInTask->GetPeekHeightOffsetFromMover();
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MIN_PEEK_HEIGHT_OFFSET, 0.25f, 0.0f, 1.0f, 0.01f);
			if (fPeekHeightOffsetFromMover > MIN_PEEK_HEIGHT_OFFSET)
			{
				// Compute the worldspace head offset from the mover offset
				s32 boneIndex = rPed.GetBoneIndexFromBoneTag(BONETAG_HEAD);
				if(boneIndex >= 0)
				{
					Matrix34 boneMatrix;
					rPed.GetGlobalMtx(boneIndex, boneMatrix);
					const float fWorldSpaceDesiredPeekHeight = rPed.GetTransform().GetPosition().GetZf() + fPeekHeightOffsetFromMover;
					float fWorldSpaceDesiredHeadOffset = fWorldSpaceDesiredPeekHeight - boneMatrix.d.z;
					//aiDisplayf("fWorldSpaceDesiredHeadOffset = %.2f", fWorldSpaceDesiredHeadOffset);
					TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, LOW_PEEK_ADDITIONAL_OFFSET, 0.0f, -1.0f, 1.0f, 0.01f);
					fWorldSpaceDesiredHeadOffset += LOW_PEEK_ADDITIONAL_OFFSET;
					//aiDisplayf("fWorldSpaceDesiredHeadOffset additional = %.2f", fWorldSpaceDesiredHeadOffset);
					vWorldSpaceOffset.z = fWorldSpaceDesiredHeadOffset;
				}
			}

		}
	}
	else if (!bIsHighCover && pInCoverInTask && pInCoverInTask->GetState() == CTaskInCover::State_BlindFiring && !CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonAimingAnimations)
	{
		TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, BLIND_FIRE_HEIGHT_OFFSET_LOW, 0.1f, 0.0f, 1.0f, 0.01f);
		vWorldSpaceOffset.z = BLIND_FIRE_HEIGHT_OFFSET_LOW;
	}

	// Make it relative to the attach matrix
	MAT34V_TO_MATRIX34(rPed.GetMatrix()).UnTransform3x3(vWorldSpaceOffset);
	vDesiredOffset += vWorldSpaceOffset;
	return vDesiredOffset;
}

bool camFirstPersonShooterCamera::HandleCustomSituations(	const CPed* attachPed,
															float& heading, float& pitch, float& desiredRoll,
															bool& bOrientationRelativeToAttachedPed, const CustomSituationsParams& oParam )
{
	TUNE_GROUP_FLOAT(CAM_FPS, fTaskControlMovementHeadingBlend, 0.25f, 0.0f, 1.0f, 0.01f);
	bool bReturn = false;
	bool bAllowLookAroundDuringCustomSituation = false;
	bool bAllowLookDownDuringCustomSituation   = false;
	m_BlendOrientation = false;
	if (m_Metadata.m_IsOrientationRelativeToAttached && attachPed)
	{
		const u32 c_uCurrentTimeMsec = fwTimer::GetTimeInMilliseconds();

		bool bUsingCustomLookEnterExitVehicle = false;
		m_bDiving = false;
		m_bPlantingBomb = false;
		bool bDropdown = false;

		const CTaskMotionInAutomobile* pMotionAutomobileTask = NULL;

		if( attachPed->GetPedIntelligence() )
		{
			pMotionAutomobileTask = (const CTaskMotionInAutomobile*)attachPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE);

			if( attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting) )
			{
				const CTaskDropDown* pTaskDropdown = (const CTaskDropDown*)( attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DROP_DOWN) );
				if( pTaskDropdown )
				{
					bDropdown = true;
				}
			}
		}

		if( m_ExitingVehicle || (pMotionAutomobileTask == NULL && !m_EnteringVehicle && !m_ExitingVehicle && !m_bEnterBlending) )
		{
			m_bWasHotwiring = false;
			m_BlendFromHotwiring = false;
		}
		
		CPed* pCurrentMeleeTarget = (m_MeleeAttack || m_WasDoingMelee || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec) ? m_pMeleeTarget.Get() : NULL;
		if(m_LockOnTarget && pCurrentMeleeTarget != m_LockOnTarget && m_LockOnTarget->GetIsTypePed())
		{
			// To prevent fighting, give priority to lock on target, rather than melee target.
			CPed* previousTarget = pCurrentMeleeTarget;
			pCurrentMeleeTarget = (CPed*)m_LockOnTarget.Get();
			if(pCurrentMeleeTarget->GetIsDeadOrDying())
			{
				pCurrentMeleeTarget = previousTarget;
			}
		}

		const bool c_IsPlayerVaulting = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
		const bool c_AnimatedCameraBlendOutFromVault = !m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera && c_IsPlayerVaulting;

		// TODO: revisit this... should be if( !oParam.bUsingHeadSpineOrientation && !m_bUseAnimatedFirstPersonCamera ) instead
		const bool bDoCustomVehicleEntryExit = (oParam.bEnteringVehicle || m_ExitingVehicle || m_bEnterBlending || m_BlendFromHotwiring || m_VehSeatInterp == 1.0f);
		if( !oParam.bUsingHeadSpineOrientation && !m_bUseAnimatedFirstPersonCamera &&
			(!m_bWasUsingAnimatedFirstPersonCamera || pCurrentMeleeTarget || m_bParachuting || bDoCustomVehicleEntryExit || oParam.bClimbingLadder || c_AnimatedCameraBlendOutFromVault))
		{
			if(m_bParachuting)
			{
				bReturn = CustomLookParachuting(attachPed, oParam.pParachuteTask, heading, pitch, bAllowLookAroundDuringCustomSituation, bAllowLookDownDuringCustomSituation, bOrientationRelativeToAttachedPed);
			
				Matrix34 matRootBone = MAT34V_TO_MATRIX34(attachPed->GetMatrix());
				desiredRoll = camFrame::ComputeRollFromMatrix(matRootBone);
			}
			else if( !m_ExitingVehicle && (pMotionAutomobileTask || m_bUsingMobile) )
			{
				bReturn = CustomLookAtHand( attachPed, pMotionAutomobileTask, heading, pitch, oParam.targetPitch,
											bOrientationRelativeToAttachedPed, bAllowLookAroundDuringCustomSituation, bAllowLookDownDuringCustomSituation );
			}

			m_bDiving = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDiving);// && !m_bHighFalling;
			if (m_bDiving)
			{
				bReturn = CustomLookDiving(heading, pitch, bAllowLookAroundDuringCustomSituation, bAllowLookDownDuringCustomSituation);
			}

			if (attachPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_BOMB))
			{
				CTaskBomb* pTaskBomb = static_cast<CTaskBomb*>(attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BOMB));
				if (pTaskBomb && pTaskBomb->GetState() == CTaskBomb::State_SlideAndPlant && !attachPed->GetPedResetFlag(CPED_RESET_FLAG_FPSPlantingBombOnFloor))
				{
					m_bPlantingBomb = true;

					static dev_float fHeadingBlendRateTune = 0.25f;
					float fHeadingBlendRate = fHeadingBlendRateTune;
					static dev_float fPitchBlendRateTune = 0.25f;
					float fPitchBlendRate  = fPitchBlendRateTune;

					bReturn = CustomLookAtPosition(pTaskBomb->GetBombTargetPosition(), heading, pitch, fHeadingBlendRate, fPitchBlendRate, bOrientationRelativeToAttachedPed);
					bAllowLookAroundDuringCustomSituation = false;
					bAllowLookDownDuringCustomSituation = false;
					m_bWasPlantingBomb = m_bPlantingBomb;
				}
			}

			// If the automobile or phone states have not been handled, check one of the other custom situations.
			if( !bReturn )
			{
				const bool c_TreatAsClimbingLadder = oParam.bClimbingLadder || oParam.bMountingLadderBottom || oParam.bMountingLadderTop;
				Assertf(!attachPed->GetUsingRagdoll(), "Ragdoll now handled by HeadSpineBoneOrientation, should not get here anymore");
				if( bDoCustomVehicleEntryExit )
				{
					bReturn = CustomLookEnterExitVehicle( attachPed, heading, pitch, desiredRoll, oParam,
														 bOrientationRelativeToAttachedPed, bAllowLookAroundDuringCustomSituation );
					bUsingCustomLookEnterExitVehicle = bReturn;
					m_WasEnteringVehicle = oParam.bEnteringVehicle || m_bEnterBlending || m_BlendFromHotwiring;
				}
				else if( oParam.bCarSlide || (bDropdown && !m_WasCarSlide) )							// must handle before climbing
				{
					// This is no longer used for CarSlide as this is now using animated camera track, but still used for dropdown.
					if(!oParam.bCarSlide)
					{
						if( !bOrientationRelativeToAttachedPed )
						{
							// Convert to non-relative heading.
							heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
							pitch   = fwAngle::LimitRadianAngle(pitch - m_AttachParentPitch);
						}

						float fFinalPitch = m_Metadata.m_MinPitch * 0.85f;
						pitch = Lerp(m_Metadata.m_VaultPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), pitch, fFinalPitch * DtoR);

						// Overriding pitch, make heading and pitch relative to ped orientation.
						bOrientationRelativeToAttachedPed = true;
					}

					// Set m_WasCarSlide when car sliding, but do not clear it here.
					m_WasCarSlide |= oParam.bCarSlide;
					m_WasDropdown = bDropdown;
					bReturn = true;
				}
				else if( (oParam.bClimbing || c_TreatAsClimbingLadder || c_AnimatedCameraBlendOutFromVault || (oParam.bJumping && !m_SprintBreakOutTriggered)) && !m_WasCarSlide )
				{
					bReturn = CustomLookClimb( attachPed, heading, pitch, bOrientationRelativeToAttachedPed, oParam,
											   bAllowLookAroundDuringCustomSituation, bAllowLookDownDuringCustomSituation );
				}
				else if( pCurrentMeleeTarget != NULL && (m_MeleeAttack || !m_IsLockedOn || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec) )
				{
					bReturn = CustomLookAtMeleeTarget(attachPed, pCurrentMeleeTarget, heading, pitch, bOrientationRelativeToAttachedPed, bAllowLookAroundDuringCustomSituation, bAllowLookDownDuringCustomSituation);
				}
				else if( m_TaskControllingMovement )
				{
					if( !bOrientationRelativeToAttachedPed )
					{
						// Convert to non-relative heading.
						heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
						pitch   = fwAngle::LimitRadianAngle(pitch - m_AttachParentPitch);
						bOrientationRelativeToAttachedPed = true;
					}

					if(m_RelativePitch == 0.0f)
					{
						m_RelativePitch = pitch;
						pitch           = oParam.targetPitch;
					}

					// Blend camera heading to ped (mover) heading.
					heading = fwAngle::LerpTowards(heading, 0.0f, Min(fTaskControlMovementHeadingBlend * 30.0f * fwTimer::GetCamTimeStep(), 1.0f));
					////pitch = fwAngle::LerpTowards(pitch, oParam.targetPitch, Min(fTaskControlMovementHeadingBlend * 30.0f * fwTimer::GetCamTimeStep(), 1.0f));
					bAllowLookAroundDuringCustomSituation = true;
					bAllowLookDownDuringCustomSituation = true;
					bReturn = true;
				}
			}
		}
		else //if(oParam.bUsingHeadSpineOrientation || m_bUseAnimatedFirstPersonCamera)
		{
			// If we are using head orientation or animated camera, disable custom behaviour and
			// reset the state tracking flags so we only blend back from the head orientation.
			m_WasClimbingLadder = false;
			m_WasClimbing = false;
			m_WasUsingMobile = false;
			//m_WasCarSlide = false;
			m_WasDropdown = false;
			m_bWasParachuting = false;
			m_bWasDiving = false;
			m_bWasPlantingBomb = false;
			if(!m_bWasUsingAnimatedFirstPersonCamera)
				m_WasDoingMelee = false;
		}

		if (oParam.bGettingArrestedInVehicle)
		{
			float fDesiredHeading = heading;
			if (attachPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
			{
				CTaskExitVehicle *pTaskExitVehicle = static_cast<CTaskExitVehicle*>(attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
				if (pTaskExitVehicle)
				{
					fDesiredHeading = camCinematicMountedCamera::LerpRelativeHeadingBetweenPeds(attachPed, pTaskExitVehicle->GetPedsJacker(), m_Frame.ComputeHeading());
				}
			}
			heading = fDesiredHeading;
		}

		// Blend back heading, if no other custom behaviour is running.
		if (!bReturn && m_WasUsingMobile && !m_bUsingMobile)
		{
			// After action is complete, blend out the pitch back to (near) zero.
			if (Abs(heading) <= 3.0f * DtoR || oParam.lookAroundHeadingOffset != 0.0f)
			{
				// Do nothing, when pitch is done blending, we clear the flags.
			}
			else
			{
				if( !bOrientationRelativeToAttachedPed )
				{
					heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
					bOrientationRelativeToAttachedPed = true;
				}

				if (Abs(heading) > 0.01f)
				{
					// Interpolate _relative_ heading back to straight ahead.
					heading = Lerp(Min(m_Metadata.m_LookOverridePhoneEndHeadingBlend * 30.0f * fwTimer::GetCamTimeStep(), 1.0f), heading, 0.0f);
					bReturn = true;
				}
			}
		}

		if(m_WasCombatRolling && !m_CombatRolling && m_IsLockedOn)
			m_WasCombatRolling = false;

		if(!bReturn) // don't blend if we are still in a custom situation.
		{
			bool bBlendOutMelee = (!m_bWasUsingAnimatedFirstPersonCamera && m_WasDoingMelee && pCurrentMeleeTarget == NULL);
			if( (!m_AttachedIsUnarmed && !m_AttachedIsUsingMelee) || m_WasStealthKill )
			{
				// Do not blend back for pistol whip or stealth kills.
				m_WasStealthKill = false;
				m_WasDoingMelee = false;
				bBlendOutMelee = false;
			}

			// Blend back pitch.
			if( (m_WasClimbingLadder && !oParam.bClimbingLadder && !oParam.bMountingLadderBottom && !oParam.bMountingLadderTop) ||
				(m_WasClimbing && !oParam.bClimbing && !oParam.bJumping) ||
				(m_WasUsingMobile && !m_bUsingMobile) ||
				(bBlendOutMelee) ||
				//(m_WasCarSlide && !oParam.bCarSlide) ||
				(m_WasEnteringVehicle && !oParam.bEnteringVehicle && !m_bEnterBlending && !m_BlendFromHotwiring) ||
				(m_WasCombatRolling && !m_CombatRolling) ||
				(m_WasDropdown && !bDropdown) ||
				(m_bWasParachuting && !m_bParachuting) ||
				(m_bWasDiving && !m_bDiving) ||
				(m_bWasPlantingBomb && !m_bPlantingBomb) ||
				(m_bWasUsingHeadSpineOrientation && !oParam.bUsingHeadSpineOrientation) )
			{
				const float c_fInputThreshold = 0.001f;
				bool bNoPitchInput   = Abs(oParam.lookAroundPitchOffset)   <= c_fInputThreshold;
				bool bNoHeadingInput = Abs(oParam.lookAroundHeadingOffset) <= c_fInputThreshold;

				float fTargetHeading = heading;
				float fTargetPitch = oParam.targetPitch;
				if(m_IsLockedOn && m_LockOnEnvelopeLevel > 0.0f)
				{
					//Only update the lock-on orientation if there is sufficient distance between the base pivot position and the target positions,
					//otherwise the lock-on behavior can become unstable.
					if(ComputeIsProspectiveLockOnTargetPositionValid(m_LockOnTargetPosition))
					{
						Vector3 lockOnDelta	= m_LockOnTargetPosition - m_Frame.GetPosition();

						lockOnDelta.NormalizeSafe();
						camFrame::ComputeHeadingAndPitchFromFront(lockOnDelta, fTargetHeading, fTargetPitch);
						if(bOrientationRelativeToAttachedPed)
						{
							fTargetHeading = fwAngle::LimitRadianAngle(fTargetHeading - m_AttachParentHeading);
							fTargetPitch   = fwAngle::LimitRadianAngle(fTargetPitch   - m_AttachParentPitch);
						}

						if(!m_WasDoingMelee)
						{
							fTargetHeading = heading;
						}
					}
				}

				// After action is complete, blend out the pitch back to (near) zero.
				if( ( Abs(pitch) <= (Abs(fTargetPitch) + 1.0f*DtoR) && 
					  Abs(oParam.currentRoll) <= (Abs(desiredRoll) + 0.1f*DtoR) &&
					  Abs(heading) <= (Abs(fTargetHeading) + 1.0f*DtoR) ) ||
					(m_fCustomPitchBlendingAccumulator > 0.99f && (bNoPitchInput || bNoHeadingInput)) )
				{
					m_WasClimbingLadder = false;
					m_WasClimbing = false;
					m_WasUsingMobile = false;
					m_WasDoingMelee = false;
					//m_WasCarSlide = false;
					m_WasEnteringVehicle = false;
					m_WasCombatRolling = false;
					m_WasDropdown = false;
					m_bWasUsingHeadSpineOrientation = false;
					m_bWasParachuting = false;
					m_bWasDiving = false;
					m_bWasPlantingBomb = false;
					m_fCustomPitchBlendingAccumulator = 0.0f;
				}
				else
				{
					// Previously, we had no input, so I would disable the blendout if user was moving the camera.
					// Then it changed to delaying the blendout while there is camera input.
					// Now that we apply a relative heading/pitch input, we can blendout the default heading/pitch and let the user still control the relative heading/pitch.
					if( !m_WasDoingMelee && !m_WasEnteringVehicle && !m_bWasUsingHeadSpineOrientation )
					{
						const bool bInstant = (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
						const float fInterp = !bInstant ? Min(m_Metadata.m_LookOverrideEndPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), 1.0f) : 1.0f;
						if(bNoHeadingInput)
						{
							Matrix34 matRootBone = MAT34V_TO_MATRIX34(attachPed->GetMatrix());
							float fRootHeading = camFrame::ComputeHeadingFromMatrix(matRootBone);
							if(bOrientationRelativeToAttachedPed)
							{
								fRootHeading = fwAngle::LimitRadianAngle(fRootHeading - m_AttachParentHeading);
							}
							heading = fwAngle::LerpTowards(heading, fRootHeading, fInterp*2.0f);
						}
						if(bNoPitchInput)
						{
							pitch = Lerp(fInterp, pitch, fTargetPitch);
						}
						desiredRoll = Lerp(fInterp, oParam.currentRoll, desiredRoll);
						m_fCustomPitchBlendingAccumulator = Lerp(fInterp, m_fCustomPitchBlendingAccumulator, 1.0f);
					}
					else
					{
						const bool bInstant = (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
						float fInterp = !bInstant ? Min(m_Metadata.m_LookOverrideMeleeEndPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), 1.0f) : 1.0f;
						if(bNoHeadingInput)
						{
							heading = fwAngle::LerpTowards(heading, fTargetHeading, fInterp*2.0f);
						}
						if(bNoPitchInput)
						{
							pitch = Lerp(fInterp, pitch, fTargetPitch);
						}

						desiredRoll = Lerp(fInterp, oParam.currentRoll, desiredRoll);
						m_fCustomPitchBlendingAccumulator = Lerp(fInterp, m_fCustomPitchBlendingAccumulator, 1.0f);
					}

					m_BlendOrientation = true;
					bReturn = true;
				}
			}
		}

		const bool bAnimatedCameraBlendingOut = m_bWasUsingAnimatedFirstPersonCamera && !m_bUseAnimatedFirstPersonCamera;
		if(!bUsingCustomLookEnterExitVehicle && !bAnimatedCameraBlendingOut)
		{
			SpringInputParams paramSpringInput;
			paramSpringInput.bEnable = !oParam.bUsingHeadSpineOrientation && bReturn && !m_bLookingBehind && bAllowLookAroundDuringCustomSituation;
			// Enable relative input during blendout as we are now blending out the underlying heading/pitch.
			////paramSpringInput.bEnable |= m_bWasCustomSituation || m_bWasUsingHeadSpineOrientation;
			paramSpringInput.bAllowHorizontal = !m_bUsingMobile || m_bParachutingOpenedChute;
			paramSpringInput.bAllowLookDown = bAllowLookDownDuringCustomSituation;
			paramSpringInput.bReset = bReturn && !m_bWasCustomSituation && !m_bWasUsingHeadSpineOrientation;
			paramSpringInput.headingInput = oParam.lookAroundHeadingOffset;
			paramSpringInput.pitchInput   = oParam.lookAroundPitchOffset;
		#if RSG_PC
			// Force spring input off, so moving camera using mouse does not spring back to center, should still be limited by camera limits.
			paramSpringInput.bEnableSpring = !bReturn ||
											 m_bLookingBehind || m_bIgnoreHeadingLimits ||
											 oParam.bClimbing || oParam.bJumping ||
											 m_CombatRolling  || m_TaskFallFalling ||
											!m_CurrentControlHelper->WasUsingKeyboardMouseLastInput();
		#else
			paramSpringInput.bEnableSpring = true;
		#endif // RSG_PC

			CalculateRelativeHeadingPitch( paramSpringInput, (m_bLookingBehind && (!bReturn /*|| m_bParachuting*/)) );
		}
	}

	return (bReturn);
}

bool camFirstPersonShooterCamera::CustomLookParachuting(const CPed* attachPed, const CTaskParachute* pParachuteTask, float& heading, float& pitch,
													  bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation, bool& UNUSED_PARAM(bOrientationRelativeToAttachedPed))
{
	//! Kill ragdoll blend.
	m_bWasUsingHeadSpineOrientation = false;

	if(!pParachuteTask->IsParachuteOut())
	{
		const float blendLevelToApply = (m_NumUpdatesPerformed > 0) ? m_Metadata.m_SkyDivePitchBlend * 30.0f * fwTimer::GetCamTimeStep() : 1.0f;
		float fDesiredPitch = m_Metadata.m_SkyDivePitch * DtoR;

		if(attachPed)
		{
			Vec3V vVelocity = VECTOR3_TO_VEC3V(attachPed->GetVelocity());
			ScalarV scMagSq = MagSquared(vVelocity);
			static dev_float s_fMinMag = 5.0f;
			ScalarV scMinMagSq = ScalarVFromF32(square(s_fMinMag));
			if(IsGreaterThanAll(scMagSq, scMinMagSq))
			{
				Vec3V vVelocityDirection = NormalizeFastSafe(vVelocity, Vec3V(V_ZERO));
				Vec3V vVelocityDirectionLocal = attachPed->GetTransform().UnTransform3x3(vVelocityDirection);
				float fCalculatedPitch = Atan2f(vVelocityDirectionLocal.GetZf(), vVelocityDirectionLocal.GetYf());

				//Don't go below the default pitch, it looks bad
				fDesiredPitch = Max(fDesiredPitch, fCalculatedPitch);
			}
		}

		pitch = Lerp(blendLevelToApply, pitch, fDesiredPitch);

		bAllowLookAroundDuringCustomSituation = true;
		bAllowLookDownDuringCustomSituation = true;
	}
	else if(pParachuteTask->GetState() <= CTaskParachute::State_Deploying)
	{
		const float blendLevelToApply = (m_NumUpdatesPerformed > 0) ? m_Metadata.m_ParachutePitchBlendDeploy * 30.0f * fwTimer::GetCamTimeStep() : 1.0f;
		const float fDesiredPitch = m_Metadata.m_SkyDiveDeployPitch * DtoR;
		pitch = Lerp(blendLevelToApply, pitch, fDesiredPitch);
		bAllowLookAroundDuringCustomSituation = false;
		bAllowLookDownDuringCustomSituation = false;
	}
	else
	{
		pitch = Lerp(m_Metadata.m_ParachutePitchBlend * 30.0f * fwTimer::GetCamTimeStep(), pitch, 0.0f);
		bAllowLookAroundDuringCustomSituation = true;
		bAllowLookDownDuringCustomSituation = true;
	}

	//! We want to blend heading to 0.0f, so that we always align with ped.

	float fT = Clamp(m_Metadata.m_ParachutePitchBlend * 30.0f * fwTimer::GetCamTimeStep(), 0.0f, 1.0f);
	heading = Lerp(fT, heading, 0.0f);

	m_bWasParachuting = m_bParachuting;

	return true;
}

bool camFirstPersonShooterCamera::CustomLookDiving(float& heading, float& pitch, bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation)
{
	m_bWasUsingHeadSpineOrientation = false;

	static dev_float fDesiredPitch = -89.0f * DtoR;
	pitch = Lerp(m_Metadata.m_DivingHeadingPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), pitch, fDesiredPitch);

	//! We want to blend heading to 0.0f, so that we always align with ped.
	heading = Lerp(m_Metadata.m_DivingHeadingPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), heading, 0.0f);

	bAllowLookAroundDuringCustomSituation = false;
	bAllowLookDownDuringCustomSituation = false;
	m_bWasDiving = m_bDiving;

	return true;
}

bool camFirstPersonShooterCamera::CustomLookAtPosition(Vector3 vTargetPosition, float& heading, float& pitch, float fHeadingBlendRate, float fPitchBlendRate, bool bOrientationRelativeToAttachedPed)
{
	if( bOrientationRelativeToAttachedPed )
	{
		// Convert to non-relative heading.
		heading = fwAngle::LimitRadianAngle(heading + m_AttachParentHeading);
		pitch   = fwAngle::LimitRadianAngle(pitch + m_AttachParentPitch);
	}
	
	Vector3 vCameraToTarget = vTargetPosition - GetFrame().GetPosition();
	vCameraToTarget.NormalizeSafe();

	float fTargetHeading = rage::Atan2f(-vCameraToTarget.x, vCameraToTarget.y);
	float fTargetPitch   = rage::AsinfSafe(vCameraToTarget.z);

	heading = fwAngle::LerpTowards(heading, fTargetHeading, fHeadingBlendRate * 30.0f * fwTimer::GetCamTimeStep());
	pitch = Lerp(fPitchBlendRate * 30.0f * fwTimer::GetCamTimeStep(), pitch, fTargetPitch);

	return true;
}

bool camFirstPersonShooterCamera::CustomLookAtHand(const CPed* attachPed, const CTaskMotionInAutomobile* pMotionAutomobileTask,
												   float& heading, float& pitch, float slopePitch, bool& bOrientationRelativeToAttachedPed,
												   bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation)
{
	bool bReturn = false;


	if( (m_bUsingMobile && !m_EnteringVehicle && !m_bEnterBlending) ||
		(pMotionAutomobileTask && pMotionAutomobileTask->GetState() == CTaskMotionInAutomobile::State_Hotwiring) )
	{
		if( bOrientationRelativeToAttachedPed )
		{
			// Convert to non-relative heading.
			heading = fwAngle::LimitRadianAngle(heading + m_AttachParentHeading);
			pitch   = fwAngle::LimitRadianAngle(pitch + m_AttachParentPitch);
		}

		float fPitchBlendRate   = (!m_bUsingMobile) ? m_Metadata.m_VehicleEntryPitchBlend   : m_Metadata.m_PhoneLookPitchBlend;
		
		//! DMKH. Turn off phone look at now we have IK on the phone. It doesn't really give us anything. 
		TUNE_GROUP_BOOL(ARM_IK, bDoPhoneLookAt, false);
		if(bDoPhoneLookAt || !m_bUsingMobile)
		{
			s32 boneIndex = attachPed->GetBoneIndexFromBoneTag(m_bUsingMobile ? BONETAG_R_FINGER11 : BONETAG_R_HAND);
			if(boneIndex >= 0)
			{
				Matrix34 matHandBone;
				attachPed->GetGlobalMtx(boneIndex, matHandBone);

				//! In 1st person, stealth is done as IK by moving torso down, so phone position is wrong here. Apply offset to correct.
				TUNE_GROUP_FLOAT(ARM_IK, fStealthPhoneHandOffset, 0.3f, -1.0f, 1.0f, 0.01f);
				if(attachPed->GetMotionData()->GetUsingStealth())
					matHandBone.d.z -= fStealthPhoneHandOffset;

				Vector3 vCameraToHand = matHandBone.d - m_Frame.GetPosition();
				vCameraToHand.NormalizeSafe();

				float fHandBoneHeading = rage::Atan2f(-vCameraToHand.x, vCameraToHand.y);
				float fHandBonePitch   = rage::AsinfSafe(vCameraToHand.z);

				float fHeadingBlendRate = (!m_bUsingMobile) ? m_Metadata.m_VehicleEntryHeadingBlend : m_Metadata.m_PhoneLookHeadingBlend;
				if (!m_bUsingMobile)
				{
					heading = fwAngle::LerpTowards(heading, fHandBoneHeading, fHeadingBlendRate * 30.0f * fwTimer::GetCamTimeStep());
				}
				pitch = Lerp(fPitchBlendRate * 30.0f * fwTimer::GetCamTimeStep(), pitch, fHandBonePitch);

				// Clamp heading/pitch to limits.
				if( pMotionAutomobileTask )
				{
					// Make heading/pitch relative to mover, clamp and restore.
					pitch   = fwAngle::LimitRadianAngle(pitch   - m_AttachParentPitch);
					pitch   = Clamp(pitch, fMinPitchForHotwiring*DtoR, -fMinPitchForHotwiring*DtoR);
					pitch   = fwAngle::LimitRadianAngle(pitch   + m_AttachParentPitch);
				}
			}
		}
		else
		{
			pitch = Lerp(fPitchBlendRate * 30.0f * fwTimer::GetCamTimeStep(), pitch, slopePitch);
		}

		float vehicleFov = m_Frame.GetFov();
		ComputeInitialVehicleCameraFov( vehicleFov );
		////vehicleFov = Lerp(m_VehSeatInterp, m_Frame.GetFov(), vehicleFov);
		m_Frame.SetFov( vehicleFov );

		if( !m_bUsingMobile && !m_BlendFromHotwiring && !m_bEnterBlendComplete )
		{
			// Force car entry code to blend to pov camera.
			m_ExtraVehicleEntryDelayStartTime = fwTimer::GetTimeInMilliseconds();
			m_VehSeatInterp  = 0.0009765625f;
			m_bEnterBlending = true;
		}

		m_WasUsingMobile = m_bUsingMobile;
		m_bWasHotwiring = !m_bUsingMobile;
		bReturn = true;
	}
	else if( m_bWasHotwiring && !m_BlendFromHotwiring )
	{
		// Force car entry code to blend to pov camera.
		m_BlendFromHotwiring = true;
	}

	if( bReturn )
	{
		// When overriding orientation, it is no longer relative to ped's orientation.
		bOrientationRelativeToAttachedPed = false;

		bAllowLookAroundDuringCustomSituation = true;
		bAllowLookDownDuringCustomSituation = m_bUsingMobile;
	}

	return (bReturn);
}

bool camFirstPersonShooterCamera::CustomLookAtMeleeTarget(const CPed* attachPed, CPed*& pCurrentMeleeTarget,
														  float& heading, float& pitch, bool& bOrientationRelativeToAttachedPed,
														  bool& bAllowLookAroundDuringCustomSituation, bool& UNUSED_PARAM(bAllowLookDownDuringCustomSituation))
{
	bool bReturn = false;
	const u32 c_uCurrentTimeMsec = fwTimer::GetTimeInMilliseconds();

	//! Don't do custom look at melee if we are blending in lock on (perhaps shouldn't do it if locked on at all).
	if( (m_IsLockedOn && m_LockOnEnvelopeLevel < 1.0f) || (m_HasStickyTarget && m_StickyAimEnvelopeLevel < 1.0f) )
	{
		m_MeleeTargetBoneRelativeOffset = g_InvalidRelativePosition;
		return bReturn;
	}

	// If melee target was alive when we acquired target, track until prone.
	// If melee target was dead when we acquired target, track until melee action is completed.
	////if( //!pCurrentMeleeTarget->IsProne() ||
	////	pCurrentMeleeTarget->GetDeathState() != DeathState_Dead ||
	////	(m_MeleeAttack &&
	////		(m_StopTimeOfProneMeleeAttackLook == 0 || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec)) )
	if( pCurrentMeleeTarget != NULL && (m_MeleeAttack || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec) )
	{
		if( !bOrientationRelativeToAttachedPed )
		{
			// Convert to relative heading.
			heading = fwAngle::LimitRadianAngle(heading - m_AttachParentHeading);
			pitch   = fwAngle::LimitRadianAngle(pitch - m_AttachParentPitch);

			// Overriding pitch and heading, both are now relative to ped orientation.
			bOrientationRelativeToAttachedPed = true;
		}

		eAnimBoneTag targetBoneTag = BONETAG_INVALID;
		CTaskMeleeActionResult*	pMeleeActionResult = attachPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 
		if( pMeleeActionResult && pMeleeActionResult->GetActionResult() )
		{
			switch( pMeleeActionResult->GetActionResult()->GetCameraTarget() )
			{
				case CActionFlags::CT_PED_HEAD:
					targetBoneTag = BONETAG_HEAD;
					break;

				case CActionFlags::CT_PED_ROOT:
					targetBoneTag = BONETAG_ROOT;
					break;

				case CActionFlags::CT_PED_NECK:
					targetBoneTag = BONETAG_NECK;
					break;

				case CActionFlags::CT_PED_CHEST:
					targetBoneTag = BONETAG_SPINE3;
					break;

				case CActionFlags::CT_PED_KNEE_LEFT:
					targetBoneTag = BONETAG_L_CALF;
					break;

				case CActionFlags::CT_PED_KNEE_RIGHT:
					targetBoneTag = BONETAG_R_CALF;
					break;

				default:
					targetBoneTag = BONETAG_NECK;
					break;
			}
		}

		Matrix34 matTargetBone;
		TUNE_GROUP_FLOAT(CAM_FPS_MELEE, fNeckOffsetBlend, 0.50f, 0.0f, 1.0f, 0.001f);
		if(targetBoneTag == BONETAG_INVALID)
		{
			// No target specified, using mover + offset to neck.
			s32 boneIndex = pCurrentMeleeTarget->GetBoneIndexFromBoneTag( BONETAG_NECK );
			if(boneIndex < 0)
				boneIndex = pCurrentMeleeTarget->GetBoneIndexFromBoneTag( BONETAG_HEAD );

			if(boneIndex >= 0)
			{
				matTargetBone = pCurrentMeleeTarget->GetObjectMtx(boneIndex);
				if(m_WasDoingMelee && m_MeleeTargetBoneRelativeOffset != g_InvalidRelativePosition)
				{
					// Blend offset with previous offset.
					Vector3 vPreviousMeleeTargetBoneRelativeOffset = m_MeleeTargetBoneRelativeOffset;

					// TODO: replace lerp with spring?
					matTargetBone.d.Lerp(fNeckOffsetBlend * 30.0f * fwTimer::GetCamTimeStep(), vPreviousMeleeTargetBoneRelativeOffset, matTargetBone.d);
				}

				// Transform to global space... only transforming the position because that is all we use.
				Matrix34 targetMoverMatrix = MAT34V_TO_MATRIX34(pCurrentMeleeTarget->GetTransform().GetMatrix());
				m_MeleeTargetBoneRelativeOffset = matTargetBone.d;
				targetMoverMatrix.Transform( m_MeleeTargetBoneRelativeOffset, matTargetBone.d );
			}
		}
		else
		{
			// Action result specified a specific bone to target.
			s32 boneIndex = pCurrentMeleeTarget->GetBoneIndexFromBoneTag( targetBoneTag );
			if(boneIndex >= 0)
				pCurrentMeleeTarget->GetGlobalMtx(boneIndex, matTargetBone);
		}

		float fTimeIndependentScale = 30.0f * fwTimer::GetCamTimeStep();
		float fPitchRate   = fTimeIndependentScale * 0.30f;
		float fHeadingRate = fTimeIndependentScale * 0.40f;

		Vector3 vCameraToTargetHead = matTargetBone.d - GetFrame().GetPosition();
		vCameraToTargetHead.NormalizeSafe();
		float fPitchToTarget = rage::AsinfSafe(vCameraToTargetHead.z);
		fPitchToTarget = fwAngle::LimitRadianAngle(fPitchToTarget - m_AttachParentPitch);
		pitch = Lerp(fPitchRate, pitch, fPitchToTarget);

		// If the target is very close to the camera in the X/Y plane then we no longer update the camera heading
		// since it could vary widely from frame to frame as the camera position moves around

		if(ComputeIsProspectiveLockOnTargetPositionValid(matTargetBone.d))
		{
			float fHeadingToTarget = rage::Atan2f(-vCameraToTargetHead.x, vCameraToTargetHead.y);

			// Make heading relative to ped heading, so we can apply limits.
			TUNE_GROUP_FLOAT(CAM_FPS, fMinRelativeHeading, -65.0f, -180.0f, 0.0f, 1.0f);
			TUNE_GROUP_FLOAT(CAM_FPS, fMaxRelativeHeading, 65.0f, 0.0f, 180.0f, 1.0f);
			TUNE_GROUP_FLOAT(CAM_FPS, fMoverRatio, 0.0f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(CAM_FPS, fSpineRatio, 0.0f, 0.0f, 1.0f, 0.01f);

			Matrix34 mtxSpine(Matrix34::IdentityType);
			Matrix34 mtxHead(Matrix34::IdentityType);

			float fForwardHeading = 0.0f;

			s32 spineBoneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_SPINE3);
			s32 headBoneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);

			if(spineBoneIndex >= 0)
			{
				attachPed->GetGlobalMtx(spineBoneIndex, mtxSpine);
				mtxSpine.RotateLocalY(HALF_PI);
			}

			if(headBoneIndex >= 0)
			{
				attachPed->GetGlobalMtx(headBoneIndex, mtxHead);
				mtxHead.RotateLocalY(HALF_PI);
			}

			Quaternion qSpine;
			Quaternion qHead;
			mtxSpine.ToQuaternion(qSpine);
			mtxHead.ToQuaternion(qHead);

			qHead.PrepareSlerp(qSpine);
			qHead.Slerp(fSpineRatio, qSpine);

			mtxHead.FromQuaternion(qHead);

			fForwardHeading = rage::Atan2f(-mtxHead.b.x, mtxHead.b.y);
			fForwardHeading = Lerp(fMoverRatio, m_AttachParentHeading, fForwardHeading);

			////heading = fwAngle::LimitRadianAngle(heading - fForwardHeading);
			fHeadingToTarget = fwAngle::LimitRadianAngle(fHeadingToTarget - fForwardHeading);
			fHeadingToTarget = Clamp(fHeadingToTarget, fMinRelativeHeading * DtoR, fMaxRelativeHeading * DtoR);
			heading = fwAngle::LerpTowards(heading, fHeadingToTarget, fHeadingRate);

			heading += fwAngle::LimitRadianAngle(fForwardHeading - m_AttachParentHeading);
			heading = fwAngle::LimitRadianAngle(heading);
		}

		bAllowLookAroundDuringCustomSituation = false;
		m_WasDoingMelee = m_MeleeAttack;
		m_WasStealthKill = m_DoingStealthKill;
		bReturn = true;

		if( pCurrentMeleeTarget->GetIsDeadOrDying() && 
			(m_StopTimeOfProneMeleeAttackLook == 0 || m_StopTimeOfProneMeleeAttackLook < c_uCurrentTimeMsec) )
		{
			if (pCurrentMeleeTarget->GetDeathState() == DeathState_Dead)
				m_StopTimeOfProneMeleeAttackLook = c_uCurrentTimeMsec + m_Metadata.m_LookAtDeadMeleeTargetDelay;
			else
				m_StopTimeOfProneMeleeAttackLook = c_uCurrentTimeMsec + m_Metadata.m_LookAtProneMeleeTargetDelay;
		}
	}
	else
	{
		pCurrentMeleeTarget = NULL;
	}

	return (bReturn);
}

bool camFirstPersonShooterCamera::AnimatedLookAtMeleeTarget(const CPed* attachPed, const CPed*& pCurrentMeleeTarget, float heading, float pitch)
{
	bool bReturn = false;
	const u32 c_uCurrentTimeMsec = fwTimer::GetTimeInMilliseconds();

	//! Don't do custom look at melee if we are blending in lock on (perhaps shouldn't do it if locked on at all).
	if( (m_IsLockedOn && m_LockOnEnvelopeLevel < 1.0f) || (m_HasStickyTarget && m_StickyAimEnvelopeLevel < 1.0f) )
	{
		return bReturn;
	}

	TUNE_GROUP_BOOL(CAM_FPS_ANIMATED, bBlendOutMeleeOffsetWhenAnimatedCameraBlendsOut, false);
	if(bBlendOutMeleeOffsetWhenAnimatedCameraBlendsOut && !m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera)
	{
		// Blending out, so just blend out the relative heading/pitch.
		m_MeleeRelativeHeading = Lerp(m_AnimatedCameraBlendLevel, 0.0f, m_MeleeRelativeHeading);
		m_MeleeRelativePitch   = Lerp(m_AnimatedCameraBlendLevel, 0.0f, m_MeleeRelativePitch);
		m_MeleeRelativeHeadingSpring.OverrideResult(m_MeleeRelativeHeading);
		m_MeleeRelativePitchSpring.OverrideResult(m_MeleeRelativePitch);
		return bReturn;
	}

	Vector2 springHeadingLimitsRadians, springPitchLimitsRadians;
	GetHeadingPitchSpringLimits(springHeadingLimitsRadians, springPitchLimitsRadians);

	float fHeadingToTarget = 0.0f;
	float fPitchToTarget = 0.0f;
	bool bForceBlendOut = false;

	// If melee target was alive when we acquired target, track until prone.
	// If melee target was dead when we acquired target, track until melee action is completed.
	////if( pCurrentMeleeTarget != NULL &&
	////	////m_bUseAnimatedFirstPersonCamera &&
	////	(////pCurrentMeleeTarget->GetDeathState() != DeathState_Dead ||
	////	//!pCurrentMeleeTarget->IsProne() ||
	////	(m_MeleeAttack || m_WasDoingMelee || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec)) )
	if( pCurrentMeleeTarget != NULL && (m_MeleeAttack || m_WasDoingMelee || m_StopTimeOfProneMeleeAttackLook >= c_uCurrentTimeMsec) )
	{
		eAnimBoneTag targetBoneTag = BONETAG_NECK;
		CTaskMeleeActionResult*	pMeleeActionResult = attachPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 
		if( pMeleeActionResult && pMeleeActionResult->GetActionResult() )
		{
			switch( pMeleeActionResult->GetActionResult()->GetCameraTarget() )
			{
				case CActionFlags::CT_PED_HEAD:
					targetBoneTag = BONETAG_HEAD;
					break;

				case CActionFlags::CT_PED_ROOT:
					targetBoneTag = BONETAG_ROOT;
					break;

				case CActionFlags::CT_PED_NECK:
					targetBoneTag = BONETAG_NECK;
					break;

				case CActionFlags::CT_PED_CHEST:
					targetBoneTag = BONETAG_SPINE3;
					break;

				case CActionFlags::CT_PED_KNEE_LEFT:
					targetBoneTag = BONETAG_L_CALF;
					break;

				case CActionFlags::CT_PED_KNEE_RIGHT:
					targetBoneTag = BONETAG_R_CALF;
					break;

				default:
					// default already assigned
					break;
			}

			if( pMeleeActionResult->IsDoingReaction() )
			{
				m_LookAroundDuringHeadSpineOrient = true;
				bForceBlendOut = true;
			}
		}

		if( !bForceBlendOut && (m_MeleeAttack || m_IsLockedOn) )
		{
			Matrix34 matTargetBone;
			s32 boneIndex = pCurrentMeleeTarget->GetBoneIndexFromBoneTag( targetBoneTag );
			if(boneIndex >= 0)
			{
				pCurrentMeleeTarget->GetGlobalMtx(boneIndex, matTargetBone);
				if(camInterface::IsSphereVisibleInGameViewport(matTargetBone.d, pCurrentMeleeTarget->GetBoundRadius()))
				{
					Vector3 vCameraToTargetHead = matTargetBone.d - GetFrame().GetPosition();
					vCameraToTargetHead.NormalizeSafe();

					fHeadingToTarget = rage::Atan2f(-vCameraToTargetHead.x, vCameraToTargetHead.y);
					fHeadingToTarget = fwAngle::LimitRadianAngle(fHeadingToTarget - heading);
					fHeadingToTarget = Clamp(fHeadingToTarget, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);

					TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fPitchToTargetThresholdToScaleHeading, -60.0f, -90.0f, -1.0f, 0.5f);

					fPitchToTarget   = rage::AsinfSafe(vCameraToTargetHead.z);
					if(fPitchToTarget < fPitchToTargetThresholdToScaleHeading * DtoR)
					{
						//if the camera to target is almost vertical avoid the heading as it will introduce
						//unwanted behaviors.
						fHeadingToTarget = 0.0f;
					}
					fPitchToTarget = fwAngle::LimitRadianAngle(fPitchToTarget - pitch);
					fPitchToTarget = Clamp(fPitchToTarget, springPitchLimitsRadians.x, springPitchLimitsRadians.y);

					bReturn = true;
				}
			}
		}

		if( m_MeleeAttack || m_WasDoingMelee )
		{
			if (!pCurrentMeleeTarget->GetIsDeadOrDying() || pCurrentMeleeTarget->GetDeathState() == DeathState_Dead)
				m_StopTimeOfProneMeleeAttackLook = c_uCurrentTimeMsec + m_Metadata.m_LookAtDeadMeleeTargetDelay;
			else
				m_StopTimeOfProneMeleeAttackLook = c_uCurrentTimeMsec + m_Metadata.m_LookAtProneMeleeTargetDelay;
		}
	}
	else
	{
		pCurrentMeleeTarget = NULL;
	}

	TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fMeleeSpringScale, 5.0f, 1.0f, 20.0f, 0.5f);
	TUNE_GROUP_FLOAT(CAM_FPS_ANIMATED, fBlendOutAnimatedLookAtSpringScale, 15.0f, 1.0f, 100.0f, 0.5f);

	const float springConstantBlendOut = !bForceBlendOut ? m_Metadata.m_OrientationSpringMelee.m_SpringConstant : m_Metadata.m_OrientationSpringMelee.m_SpringConstant * fBlendOutAnimatedLookAtSpringScale;
	const float springConstantHeading = (fHeadingToTarget != 0.0f) ? m_Metadata.m_OrientationSpringMelee.m_SpringConstant*fMeleeSpringScale : springConstantBlendOut;
	const float springConstantPitch   = (fPitchToTarget   != 0.0f) ? m_Metadata.m_OrientationSpringMelee.m_SpringConstant*fMeleeSpringScale : springConstantBlendOut;

	if(!m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera)
	{
		// When animated camera ends, need to blend out the melee relative heading/pitch as the animated camera blends out.
		m_MeleeRelativeHeading = Lerp(m_AnimatedCameraBlendLevel, 0.0f, m_MeleeRelativeHeading);
		m_MeleeRelativePitch   = Lerp(m_AnimatedCameraBlendLevel, 0.0f, m_MeleeRelativePitch);
		m_MeleeRelativeHeadingSpring.OverrideResult(m_MeleeRelativeHeading);
		m_MeleeRelativePitchSpring.OverrideResult(m_MeleeRelativePitch);
	}
	else
	{
		// Blend relative offset to target offset... if no target, we blend to zero.
		m_MeleeRelativeHeading = m_MeleeRelativeHeadingSpring.Update(fHeadingToTarget, springConstantHeading, m_Metadata.m_OrientationSpringMelee.m_SpringDampingRatio);
		m_MeleeRelativePitch   = m_MeleeRelativePitchSpring.Update(  fPitchToTarget,   springConstantPitch,   m_Metadata.m_OrientationSpringMelee.m_SpringDampingRatio);
	}

	if(m_MeleeAttack || m_WasDoingMelee)
	{
		m_LookAroundDuringHeadSpineOrient = false;

		float fFinalHeading = fwAngle::LimitRadianAngle(m_RelativeHeading + m_MeleeRelativeHeading);
		float fClampedHeading = Clamp(fFinalHeading, springHeadingLimitsRadians.x, springHeadingLimitsRadians.y);
		if(fClampedHeading != fFinalHeading)
		{
			m_MeleeRelativeHeading = fwAngle::LimitRadianAngle(fClampedHeading - m_RelativeHeading);
			m_MeleeRelativeHeadingSpring.OverrideResult(m_MeleeRelativeHeading);
		}

		// Clamp relative pitch to animated camera data limits.
		float fFinalRelativePitch = fwAngle::LimitRadianAngle(m_RelativePitch + m_MeleeRelativePitch);
		float fClampedPitch = Clamp(fFinalRelativePitch, springPitchLimitsRadians.x, springPitchLimitsRadians.y);
		if(fClampedPitch != fFinalRelativePitch)
		{
			m_MeleeRelativePitch = fwAngle::LimitRadianAngle(fClampedPitch - m_RelativePitch);
			m_MeleeRelativePitchSpring.OverrideResult(m_MeleeRelativePitch);
		}

		// For safety, clamp final pitch (relative to ped pitch) to regular camera limits.
		float fFinalPitch = (pitch-m_AttachParentPitch)+m_RelativePitch+m_MeleeRelativePitch;
		fFinalPitch = fwAngle::LimitRadianAngle(fFinalPitch);
		fClampedPitch = Clamp(fFinalPitch, m_MinPitch, m_MaxPitch);
		if(fClampedPitch != fFinalPitch)
		{
			m_MeleeRelativePitch = fwAngle::LimitRadianAngle(fClampedPitch - (fFinalPitch - m_MeleeRelativePitch));
			m_MeleeRelativePitchSpring.OverrideResult(m_MeleeRelativePitch);
		}
	}

	return (bReturn);
}

bool camFirstPersonShooterCamera::CustomLookEnterExitVehicle( const CPed* attachPed, float& heading, float& pitch, float& desiredRoll, const CustomSituationsParams& oParam,
															  bool& bOrientationRelativeToAttachedPed, bool& bAllowLookAroundDuringCustomSituation )
{
	bool bReturn = false;
	if( bOrientationRelativeToAttachedPed )
	{
		// Convert to non-relative heading.
		heading = fwAngle::LimitRadianAngle(heading + oParam.previousAttachParentHeading);
		pitch   = fwAngle::LimitRadianAngle(pitch   + oParam.previousAttachParentPitch);
	}

	// Have to set the result again, in case heading/pitch was clamped last update.
	m_VehicleHeadingSpring.OverrideResult(heading);
	m_VehiclePitchSpring.OverrideResult(pitch);
	m_VehicleSeatBlendHeadingSpring.OverrideResult(heading);
	m_VehicleSeatBlendPitchSpring.OverrideResult(pitch);
	m_VehicleCarJackHeadingSpring.OverrideResult(heading);
	m_VehicleCarJackPitchSpring.OverrideResult(pitch);

	bool bUseHeadingToSeat = false;
	Matrix34 playerHeadMatrix;
	Matrix34 playerSpineMatrix;
	float fHeadBoneHeading, fHeadBonePitch;
	float fSpineBoneHeading, fSpineBonePitch;
	float fRootHeading, fRootPitch;
	float fLeftShoulderHeading, fLeftShoulderPitch;
	float fRightShoulderHeading, fRightShoulderPitch;
	GetBoneHeadingAndPitch(attachPed, BONETAG_ROOT, playerSpineMatrix, fRootHeading, fRootPitch);
	Vector3 vRootPosition = playerSpineMatrix.d;
	GetBoneHeadingAndPitch(attachPed, BONETAG_R_CLAVICLE, playerSpineMatrix, fRightShoulderHeading, fRightShoulderPitch);
	GetBoneHeadingAndPitch(attachPed, BONETAG_L_CLAVICLE, playerSpineMatrix, fLeftShoulderHeading, fLeftShoulderPitch);
	GetBoneHeadingAndPitch(attachPed, BONETAG_SPINE3, playerSpineMatrix, fSpineBoneHeading, fSpineBonePitch);
	GetBoneHeadingAndPitch(attachPed, BONETAG_HEAD, playerHeadMatrix, fHeadBoneHeading, fHeadBonePitch, (!m_bEnteringTurretSeat) ? 0.075f : 0.125f);

	if(/*m_bUsingMobile ||*/ m_bMobileAtEar)
	{
		// If using phone and we got here, then player is not looking at phone.
		// He is holding phone to his ear which makes his head look to the right a bit.
		// If we try to face the head orientation, the camera will end up turning in a big circle.
		fHeadBoneHeading = m_AttachParentHeading;
		bUseHeadingToSeat = m_EnteringVehicle;
	}

	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehMaxHeadingBlend, 0.35f, 0.0f, 1.0f, 0.001f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehMaxPitchBlend,   0.27f, 0.0f, 1.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fVehicleEntryMinPhaseForRollBlend,   0.00f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fVehicleEntryMaxPhaseForRollBlend,   1.00f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fThresholdDistanceToEndBlend, 0.10f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehicleInitialHeadingInterp, 0.20f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehicleInitialPitchInterp, 0.10f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehicleShuffleHeadingInterp, 0.05f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehicleInitialPitchLimit, 40.0f, 0.0f, 90.0f, 0.10f);
	TUNE_GROUP_BOOL( CAM_FPS_ENTER_EXIT, bCarUseRootBoneHeading,  true);
	TUNE_GROUP_BOOL( CAM_FPS_ENTER_EXIT, bBoatEntryUseAttachParentHeading,  true);
	INSTANTIATE_TUNE_GROUP_INT( CAM_FPS_CAR_JACK, c_CarJackBlendOutLimitDuration, 0, 3000, 1);
	const float c_EnterVehicleLookOffsetSpringConstant = 10.0f;

	bool bInterpolateToVehicleCameraPosition = false;
	Vector3 vehicleCameraPosition = m_Frame.GetPosition();

	bool bCarSeatAlwaysHadDoor = true; //!m_bEnteringTurretSeat;
	const CModelSeatInfo* pModelSeatInfo = NULL;
	if( m_pVehicle && m_pVehicle->GetVehicleModelInfo())
	{
		pModelSeatInfo = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if(m_pVehicle->IsEntryIndexValid(m_VehicleSeatIndex) && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR))
		{
			const CCarDoor* pCarDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_VehicleSeatIndex);
			bCarSeatAlwaysHadDoor = (pCarDoor != NULL);
		}
	}

	float fLookDownLimit = -fEnterVehicleInitialPitchLimit * DtoR;
	float fShoulderAverageHeading = fwAngle::LimitRadianAngle(fRightShoulderHeading + 0.50f*fwAngle::LimitRadianAngle(fLeftShoulderHeading - fRightShoulderHeading));
	float fHeadingRelativeTo = fShoulderAverageHeading; //fSpineBoneHeading; //m_AttachParentHeading;
	float fPitchRelativeTo   = m_AttachParentPitch; //(fRightShoulderPitch  +fLeftShoulderPitch  )*0.50f; //fSpineBonePitch;   //m_AttachParentPitch;
	if( m_pVehicle )
	{
		if(m_bEnteringTurretSeat)
		{
			if(!m_pVehicle->GetIsRotaryAircraft())
			{
				fHeadingRelativeTo = fRootHeading; //fHeadBoneHeading;
			}
		}
		else if(m_pVehicle->InheritsFromPlane() || m_pVehicle->GetIsRotaryAircraft() || m_pVehicle->GetIsJetSki())
		{
			fHeadingRelativeTo = fRootHeading;
			if(m_IsSeatShuffling)
			{
				fHeadingRelativeTo = fHeadBoneHeading;
			}
		}
		else if(m_pVehicle->InheritsFromBoat() && !m_pVehicle->GetIsJetSki() && (bBoatEntryUseAttachParentHeading || m_IsCarJacking))
		{
			fHeadingRelativeTo = (!m_IsCarJacking) ? m_AttachParentHeading : fRootHeading;
		#if RSG_PC
			if( m_TripleHead && !m_IsCarJacking )
			{
				fHeadingRelativeTo = fRootHeading;
			}
		#endif
		}
		else if(bCarUseRootBoneHeading && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR))
		{
			fHeadingRelativeTo = fRootHeading; ////fHeadingRelativeTo = (m_IsSeatShuffling) ? fRootHeading : m_AttachParentHeading;
			if(!bCarSeatAlwaysHadDoor)
			{
				if(m_VehicleTargetSeatIndex == 0 || m_VehicleTargetSeatIndex == 1)
				{
					fHeadingRelativeTo = fSpineBoneHeading;
				}
				else
				{
					fHeadingRelativeTo = fHeadBoneHeading;
				}
			}

		#if RSG_PC
			if(m_TripleHead && m_ExitingVehicle)
			{
				fHeadingRelativeTo = fRootHeading;
			}
		#endif
		}
		else if(m_IsCarJacking && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR) && m_VehicleSeatIndex != m_VehicleTargetSeatIndex)
		{
			fHeadingRelativeTo = fRootHeading;
		}
	#if RSG_PC
		else if(m_TripleHead && m_IsABikeOrJetski && m_EnterPickUpBike)
		{
			fHeadingRelativeTo = fHeadBoneHeading;
		}
	#endif
	}

#if RSG_PC
	if( m_TripleHead && m_IsCarJacking && (!m_pVehicle || !m_pVehicle->GetIsJetSki() || !m_pVehicle->GetIsAircraft()) )
	{
		fHeadingRelativeTo = fShoulderAverageHeading;
	}
#endif

	const float fExitHeading = (m_pVehicle && !m_pVehicle->GetIsAircraft() && !m_pVehicle->InheritsFromBoat() && !m_IsABikeOrJetski) ? fRootHeading : fHeadBoneHeading;
	if(m_ExitingVehicle)
	{
		fHeadingRelativeTo = fExitHeading;
	}

	if(!m_ResetEntryExitSprings && (bCarSeatAlwaysHadDoor || m_ExitingVehicle))
	{
		const bool c_FasterBlend = ( (m_pVehicle && (m_pVehicle->GetIsJetSki() || (m_bEnteringTurretSeat && m_pVehicle->GetIsRotaryAircraft()))) || m_ExitingVehicle );
		float springConstant = (!c_FasterBlend) ? m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringConstant : m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringConstant*3.0f;
	#if RSG_PC
		if(m_TripleHead)
		{
			if(m_IsCarJacking)
				springConstant = m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringConstant * ((!c_FasterBlend) ? 5.0f : 10.0f);
		}
	#endif
		fHeadingRelativeTo = m_EntryExitRelativeHeadingToSpring.UpdateAngular(fHeadingRelativeTo, springConstant, m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringDampingRatio);
	}
	else
	{
		m_EntryExitRelativeHeadingToSpring.Reset(fHeadingRelativeTo);
	}

	const bool bIsAPlaneOrHelo = m_pVehicle && m_pVehicle->GetIsAircraft();
	if( m_pVehicle && m_pVehicle->GetVehicleModelInfo() && !m_ExitingVehicle &&
		m_pVehicle->IsEntryIndexValid(m_VehicleSeatIndex) &&
		m_pVehicle->IsEntryIndexValid(m_VehicleTargetSeatIndex) && 
		m_pVehicle->IsEntryIndexValid(m_VehicleInitialSeatIndex) )
	{
		if(pModelSeatInfo)
		{
			// m_VehicleTargetSeatIndex is the destination seat and m_VehicleSeatIndex is the entry seat.
			const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleTargetSeatIndex );
			Matrix34 seatMtx;
			m_pVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);

			if(bUseHeadingToSeat)
			{
				Vector3 vDelta = seatMtx.d - VEC3V_TO_VECTOR3(attachPed->GetTransform().GetPosition());
				if( vDelta.Mag2() > SMALL_FLOAT*SMALL_FLOAT )
				{
					vDelta.Normalize();
					fHeadBoneHeading = rage::Atan2f(-vDelta.x, vDelta.y);
				}
				else
				{
					bUseHeadingToSeat = false;
				}
			}

			if( m_EnteringVehicle || m_bEnterBlending || m_BlendFromHotwiring || m_VehSeatInterp >= 1.0f )
			{
				// If true, car jacking or being car jacked overrides car enter/exit behaviour.
				bool bHandled = HandlePlayerGettingCarJacked(attachPed, playerHeadMatrix.d, playerSpineMatrix.d, heading, pitch, fLookDownLimit);
				if(!bHandled && m_IsCarJacking && m_pJackedPed != NULL)
				{
					Matrix34 entrySeatMtx;
					const s32 entrySeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleSeatIndex );
					m_pVehicle->GetGlobalMtx(entrySeatBoneIndex, entrySeatMtx);

					Vector3 vTarget = entrySeatMtx.d;
					Vector3 victimSpinePosition;
					bHandled = AdjustTargetForCarJack(	attachPed, entrySeatMtx, playerSpineMatrix, vTarget, vRootPosition, heading, pitch, fLookDownLimit );
				}

				const float c_AlignVerticalOtherSeatOffset = 0.40f;
				const float c_AlignVerticalSeatOffset = 0.25f;
				const float c_EntryVerticalSeatOffset = 0.20f;
				float fAlignVerticalOffset = (m_VehicleTargetSeatIndex == m_VehicleSeatIndex) ? c_AlignVerticalSeatOffset : c_AlignVerticalOtherSeatOffset;
				if( !bHandled )
				{
					if( m_VehSeatInterp >= 0.0009765625f )
					{
						float fVehSeatInterpToUse = m_VehSeatInterp;
						if( m_EnteringVehicle || m_bEnterBlending )
							fVehSeatInterpToUse = SlowInOut(m_VehSeatInterp);

						float fMinHeadingSpringConstant = m_Metadata.m_VehicleSeatBlendHeadingMinSpring.m_SpringConstant;
						float fMinPitchSpringConstant   = m_Metadata.m_VehicleSeatBlendPitchMinSpring.m_SpringConstant;
						if(m_VehicleTargetSeatIndex != m_VehicleSeatIndex)
						{
							fMinHeadingSpringConstant = Max(m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringConstant*0.50f, fMinHeadingSpringConstant);
							fMinPitchSpringConstant   = Max(m_Metadata.m_VehicleSeatBlendPitchMaxSpring.m_SpringConstant*0.50f,   fMinPitchSpringConstant);
						}

						float fFinalVehCamHeading = 0.0f;
						float fFinalVehCamPitch   = 0.0f;
						{
							// m_VehicleTargetSeatIndex is the destination seat and m_VehicleSeatIndex is the entry seat.
							if( (m_VehicleTargetSeatIndex != m_VehicleSeatIndex && m_VehicleTargetSeatIndex >= 0 WIN32PC_ONLY(&& !m_TripleHead)) ||
								 m_pVehicle->GetIsJetSki() )
							{
								Matrix34 targetSeatMtx;
								const s32 targetSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleTargetSeatIndex );
								m_pVehicle->GetGlobalMtx(targetSeatBoneIndex, targetSeatMtx);

								float fLookForwardOffset = 0.75f;
								if(m_pVehicle->GetIsJetSki())
								{
									fLookForwardOffset = RampValue(m_VehSeatInterp, 0.25f, 1.0f, 0.0f, 0.75f);
								}

								Vector3 vDelta = targetSeatMtx.d + (targetSeatMtx.b * fLookForwardOffset) - vRootPosition;//playerHeadMatrix.d;
								vDelta.z += c_EntryVerticalSeatOffset;							// add offset to seat position.
								if( vDelta.Mag2() > SMALL_FLOAT*SMALL_FLOAT )
								{
									vDelta.Normalize();
									fFinalVehCamHeading = rage::Atan2f(-vDelta.x, vDelta.y);
									fFinalVehCamPitch   = AsinfSafe(vDelta.z);
								}

								if(m_pVehicle->GetIsJetSki())
								{
								#if RSG_PC
									if(m_TripleHead && m_EnterJackingPed)
									{
										fMinHeadingSpringConstant = Max(fMinHeadingSpringConstant, 3000.0f);
									}
								#endif

									float fAngleInterp = RampValue(m_VehSeatInterp, 0.40f, 0.80f, 0.0f, 1.0f);
									float fSeatVehCamHeading = rage::Atan2f(-seatMtx.b.x, seatMtx.b.y);
									float fSeatVehCamPitch   = AsinfSafe(seatMtx.b.z);
									fFinalVehCamHeading = fwAngle::LerpTowards(fFinalVehCamHeading, fSeatVehCamHeading, fAngleInterp);
									fFinalVehCamPitch   = fwAngle::LerpTowards(fFinalVehCamPitch,   fSeatVehCamPitch,   fAngleInterp);
								}
							}
							else
							{
								fFinalVehCamHeading = rage::Atan2f(-seatMtx.b.x, seatMtx.b.y);
								fFinalVehCamPitch   = AsinfSafe(seatMtx.b.z);
								if( m_pVehicle->GetIsRotaryAircraft() && !m_bEnteringTurretSeat )
								{
									float fAngleInterp = RampValue(m_VehSeatInterp, 0.45f, 0.85f, 0.0f, 1.0f);
									float fHeadingDiff = fwAngle::LimitRadianAngle(heading - fFinalVehCamHeading);
									float fHeadingRelativeToDiff = fwAngle::LimitRadianAngle(fHeadingRelativeTo - fFinalVehCamHeading);
									float headingToUse = fHeadingRelativeTo;
									if(!m_IsSeatShuffling && Abs(fHeadingDiff) < Abs(fHeadingRelativeToDiff))
									{
										// fHeadingDiff is closer to the final heading, so use that.
										headingToUse = heading;
									}
									fFinalVehCamHeading = fwAngle::LerpTowards(headingToUse, fFinalVehCamHeading, fAngleInterp);
								}
							}
						}

						const float fTargetRoll = ComputeInitialVehicleCameraRoll(seatMtx);
						float fRollInterp = RampValueSafe(m_VehSeatInterp, fVehicleEntryMinPhaseForRollBlend, fVehicleEntryMaxPhaseForRollBlend, 0.0f, 1.0f);
						desiredRoll = Lerp(fRollInterp, desiredRoll, fTargetRoll);

						bInterpolateToVehicleCameraPosition = ComputeInitialVehicleCameraPosition(attachPed, seatMtx, vehicleCameraPosition);

						float vehicleFov = m_Frame.GetFov();
						ComputeInitialVehicleCameraFov( vehicleFov );
						vehicleFov = Lerp(fVehSeatInterpToUse, m_Frame.GetFov(), vehicleFov);
						m_Frame.SetFov( vehicleFov );

						const u32 duration = ( (m_bEnteringTurretSeat) ? c_TurretEntryDelay : (m_BlendFromHotwiring) ? c_HotwireBlendDelay : c_ExtraVehicleEntryDelay );
						if(m_EnteringVehicle || m_bEnterBlending || m_BlendFromHotwiring)
						{
							float fHeadingSpringConstant = Lerp(m_VehSeatInterp, fMinHeadingSpringConstant, m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringConstant);
							float fPitchSpringConstant   = Lerp(m_VehSeatInterp, fMinPitchSpringConstant,   m_Metadata.m_VehicleSeatBlendPitchMaxSpring.m_SpringConstant);
							const float c_fEnterBlendMaxSpringConstant = 1000.0f;
							if(m_bEnterBlendComplete)
							{
								fHeadingSpringConstant = c_fEnterBlendMaxSpringConstant;
							}
							else if(m_bEnterBlending && m_ExtraVehicleEntryDelayStartTime != 0)
							{
								float fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_ExtraVehicleEntryDelayStartTime) / (float)duration;
								fInterp = Clamp(fInterp, 0.0f, 1.0f);

								fHeadingSpringConstant = Lerp(fInterp, fHeadingSpringConstant, c_fEnterBlendMaxSpringConstant);
							}
							heading = m_VehicleSeatBlendHeadingSpring.UpdateAngular(fFinalVehCamHeading, fHeadingSpringConstant, m_Metadata.m_VehicleSeatBlendHeadingMaxSpring.m_SpringDampingRatio);
							pitch   = m_VehicleSeatBlendPitchSpring.UpdateAngular(fFinalVehCamPitch, fPitchSpringConstant, m_Metadata.m_VehicleSeatBlendPitchMaxSpring.m_SpringDampingRatio);
						}
						else if( m_VehSeatInterp >= 1.0f )
						{
							heading = fFinalVehCamHeading;
							pitch = fFinalVehCamPitch;
						}

						if( bInterpolateToVehicleCameraPosition )
						{
							Vector3 cameraPosition = m_Frame.GetPosition();
							PushCameraForwardForHeadAndArms( attachPed, cameraPosition, vehicleCameraPosition, seatMtx, playerHeadMatrix, pitch, (!bHandled && fVehSeatInterpToUse >= 0.0009765625f) );
							if(m_bEnteringTurretSeat)
							{
								if(m_bEnterBlending && m_ExtraVehicleEntryDelayStartTime != 0)
								{
									float fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_ExtraVehicleEntryDelayStartTime) / (float)duration;
									fInterp = RampValueSafe(fInterp, 0.25f, 1.0f, 0.0f, 1.0f);

									////Mat34V pedMatrix = attachPed->GetMatrix();
									Vec3V relativeCameraPosition = UnTransformFull(RCC_MAT34V(seatMtx), RCC_VEC3V(cameraPosition));
									// Since vehicleCameraPosition is relative to ped when the entry ends, use seatMtx as an approximation of ped's ending matrix.
									Vec3V relativeVehicleCamPosition = UnTransformFull(RCC_MAT34V(seatMtx), RCC_VEC3V(vehicleCameraPosition));
									Vec3V blendedRelativePosition = Lerp( ScalarV(fInterp), relativeCameraPosition, relativeVehicleCamPosition );
									m_ObjectSpacePosition = VEC3V_TO_VECTOR3(blendedRelativePosition);
									RC_VEC3V(cameraPosition) = Transform(RCC_MAT34V(seatMtx), blendedRelativePosition);
								}
								else if (m_bEnterBlendComplete)
								{
									cameraPosition = vehicleCameraPosition;
								}
							}
							m_Frame.SetPosition( cameraPosition );
						}

						const bool c_bTreatAsCloseToSeat = ( fVehSeatInterpToUse < (1.0f - SMALL_FLOAT) &&
															(1.0f - fVehSeatInterpToUse) * m_VehicleSeatBlendDistance <= fThresholdDistanceToEndBlend ) ||
											( m_bEnteringTurretSeat && !m_bWasUsingAnimatedFirstPersonCamera && fVehSeatInterpToUse > (0.99f - SMALL_FLOAT) );
						if( m_bEnterBlending && !m_BlendFromHotwiring && 
							c_bTreatAsCloseToSeat &&
							m_ExtraVehicleEntryDelayStartTime == 0 && m_VehicleSeatIndex == m_VehicleTargetSeatIndex)
						{
							m_ExtraVehicleEntryDelayStartTime = fwTimer::GetTimeInMilliseconds();
						}

						// Extra delay before switching to pov camera, to ensure blend ends.
						if( m_ExtraVehicleEntryDelayStartTime != 0 && !m_bEnterBlendComplete &&
							m_ExtraVehicleEntryDelayStartTime+duration < fwTimer::GetTimeInMilliseconds() )
						{
							m_bEnterBlending = false;
							m_bEnterBlendComplete = true;
							m_ExtraVehicleEntryDelayStartTime = 0;
						}
					}
					else //if(!m_ExitingVehicle)
					{
						// m_VehicleTargetSeatIndex is the destination seat and m_VehicleSeatIndex is the entry seat.
						Vector3 vTarget = seatMtx.d;
						Vector3 vLookAt = vTarget;
						float fowardLookOffset = 0.50f;
						const bool bDoAlignmentBehavior = (!m_IsABikeOrJetski && m_EnterVehicleAlign);
						if(m_IsABikeOrJetski)
						{
							Vector3 vHandlePosition(Vector3::ZeroType);
							Matrix34 handleMatrix;
							int numValidBones = 0;
							int handleBoneIndex = m_pVehicle->GetBoneIndex( VEH_HBGRIP_L );
							if (handleBoneIndex != -1)
							{
								m_pVehicle->GetGlobalMtx(handleBoneIndex, handleMatrix);
								vHandlePosition += handleMatrix.d;
								numValidBones++;
							}

							handleBoneIndex = m_pVehicle->GetBoneIndex( VEH_HBGRIP_R );
							if (handleBoneIndex != -1)
							{
								m_pVehicle->GetGlobalMtx(handleBoneIndex, handleMatrix);
								vHandlePosition += handleMatrix.d;
								numValidBones++;
							}

							if(numValidBones == 2)
								vHandlePosition = vHandlePosition * 0.50f;

							if(numValidBones > 0)
								vTarget = vHandlePosition;

							vLookAt = vTarget;
							if(!m_EnterGoToDoor && !bDoAlignmentBehavior && !m_EnterOpenDoor && !m_EnterPickUpBike)
							{
								fowardLookOffset = 1.0f;
								vLookAt.z += c_EntryVerticalSeatOffset * 0.25f;					// add offset to seat position.
							}
							else
							{
								vLookAt.z += fAlignVerticalOffset * 0.25f;						// add offset to seat position.
								if(m_EnterPickUpBike)
								{
									Vector3 vCamFwd2d = m_Frame.GetFront();
									vCamFwd2d.z = 0.0f;
									vCamFwd2d.NormalizeSafe();
									vLookAt += vCamFwd2d * 1.50f;						// extra forward offset as camera gets close to seat.
								}
							}
						}
						else if(m_VehicleTargetSeatIndex != m_VehicleSeatIndex && m_pVehicle->IsEntryIndexValid(m_VehicleTargetSeatIndex))
						{
							////Matrix34 targetSeatMtx;
							////const s32 targetSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleTargetSeatIndex );
							////m_pVehicle->GetGlobalMtx(targetSeatBoneIndex, targetSeatMtx);
							////vLookAt = targetSeatMtx.d;

							fowardLookOffset = 0.70f;
							WIN32PC_ONLY( if(m_TripleHead) { fowardLookOffset = 1.50f; } )
							vLookAt.z += c_EntryVerticalSeatOffset;								// add offset to seat position.

							if(m_EnteringSeat)
							{
								const float fTargetRoll = ComputeInitialVehicleCameraRoll(seatMtx);
								float fRollInterp = RampValueSafe(m_VehSeatInterp, fVehicleEntryMinPhaseForRollBlend, fVehicleEntryMaxPhaseForRollBlend, 0.0f, 1.0f);
								desiredRoll = Lerp(fRollInterp, desiredRoll, fTargetRoll);
							}
						}
						else
						{
							if(!m_EnterGoToDoor && !bDoAlignmentBehavior && !m_EnterOpenDoor)
							{
								fowardLookOffset = (!bIsAPlaneOrHelo) ? 1.00f : 0.70f;
								vLookAt.z += c_EntryVerticalSeatOffset;							// add offset to seat position.
							}
							else
							{
								vLookAt.z += fAlignVerticalOffset;								// add offset to seat position.
							}

							if(m_EnteringSeat)
							{
								const float fTargetRoll = ComputeInitialVehicleCameraRoll(seatMtx);
								float fRollInterp = RampValueSafe(m_VehSeatInterp, fVehicleEntryMinPhaseForRollBlend, fVehicleEntryMaxPhaseForRollBlend, 0.0f, 1.0f);
								desiredRoll = Lerp(fRollInterp, desiredRoll, fTargetRoll);
							}
						}

						if (!m_IsABikeOrJetski && fowardLookOffset > 0.0f)
						{
							// Try scaling the forward offset depending on the angle 
							float seatHeading = safe_atan2f (-seatMtx.b.x, seatMtx.b.y);
							float difference = fwAngle::LimitRadianAngle(seatHeading - fHeadingRelativeTo);
							float forwardOffsetScale = RampValueSafe(Abs(difference), PI*0.6f, PI*0.9f, 1.0f, 0.0f); 
							fowardLookOffset *= forwardOffsetScale;
						}

						float fEnterVehicleHeadingSpringConstant = m_Metadata.m_VehicleEnterHeadingSpring.m_SpringConstant;
						fowardLookOffset = m_EnterVehicleLookOffsetSpring.Update(fowardLookOffset, c_EnterVehicleLookOffsetSpringConstant, 1.0f);
						vLookAt += (seatMtx.b * fowardLookOffset);

						// On entry, until we are close enough to start blending to pov camera,
						// look at target seat with offset.
						float fTargetHeading;
						float fTargetPitch;
						Vector3 vDelta = vLookAt - playerHeadMatrix.d;
						if(m_EnterJackingPed && !m_IsABikeOrJetski && vDelta.Mag2() > rage::square(0.01f))
						{
							vDelta.Normalize();
							fTargetHeading = rage::Atan2f(-vDelta.x, vDelta.y);
							fTargetPitch   = AsinfSafe(vDelta.z);
							if(m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
							{
								fTargetHeading = (fTargetHeading * 0.35f) + (m_AttachParentHeading * 0.65f);
								fTargetPitch   = (fTargetPitch   * 0.35f) + (m_AttachParentPitch   * 0.65f);
							}
						}
						else if(!m_EnterGoToDoor && !bDoAlignmentBehavior && vDelta.Mag2() > rage::square(0.01f))
						{
							vDelta.Normalize();
							fTargetHeading = rage::Atan2f(-vDelta.x, vDelta.y);
							fTargetPitch   = AsinfSafe(vDelta.z);
						}
						else
						{
							fTargetHeading = m_AttachParentHeading;
							fTargetPitch   = m_AttachParentPitch;
						}

						if( (bUseHeadingToSeat || !bCarSeatAlwaysHadDoor)
					#if RSG_PC
							 && (!m_TripleHead || m_VehicleSeatIndex > 1)
					#endif
							)
						{
							fTargetHeading = fHeadBoneHeading;
						}

						bool bTurnFaster = !bCarSeatAlwaysHadDoor;
					#if RSG_PC
						if(m_TripleHead && ( (m_pVehicle->InheritsFromBoat() && !m_IsABikeOrJetski) ||
											 (m_IsABikeOrJetski && m_EnterPickUpBike) ||
											 (m_IsCarJacking) ))
						{
							fTargetHeading = fHeadingRelativeTo;
							bTurnFaster = !m_IsCarJacking;
						}

						if(m_TripleHead && m_pVehicle->GetIsJetSki())
						{
							fTargetHeading = fHeadBoneHeading;
							fEnterVehicleHeadingSpringConstant = Max(fEnterVehicleHeadingSpringConstant, 200.0f);
						}
					#endif

						if(bTurnFaster)
						{
							fEnterVehicleHeadingSpringConstant = (m_VehicleTargetSeatIndex <= 1) ? m_Metadata.m_VehicleEnterHeadingSpring.m_SpringConstant*10.0f : 0.0f;
						}
						heading = m_VehicleHeadingSpring.UpdateAngular(fTargetHeading,
									fEnterVehicleHeadingSpringConstant,
									m_Metadata.m_VehicleEnterHeadingSpring.m_SpringDampingRatio);
						pitch   = m_VehiclePitchSpring.UpdateAngular(fTargetPitch,
									m_Metadata.m_VehicleEnterPitchSpring.m_SpringConstant, 
									m_Metadata.m_VehicleEnterPitchSpring.m_SpringDampingRatio);

						m_VehicleSeatBlendHeadingSpring.Reset(heading);
						m_VehicleSeatBlendPitchSpring.Reset(pitch);
					}
				}

				if( !m_EnteringVehicle && m_bEnterBlending && m_VehSeatInterp < 0.0009765625f )
				{
					// Stopped entering vehicle, attempting to blend to finish entry, but too far from seat... cancel blend and terminate entry.
					m_bEnterBlending = false;
					m_bEnterBlendComplete = false;
					m_ExtraVehicleEntryDelayStartTime = 0;
					m_VehicleSeatIndex = -1;
					m_VehicleTargetSeatIndex = -1;
				}
			}
		}
	}
	else if(m_ExitingVehicle && m_pVehicle && m_pVehicle->GetVehicleModelInfo() /*&& !m_pVehicle->InheritsFromPlane()*/ && m_pVehicle->IsEntryIndexValid(m_VehicleSeatIndex))
	{
		if( !HandlePlayerGettingCarJacked(attachPed, playerHeadMatrix.d, playerSpineMatrix.d, heading, pitch, fLookDownLimit) )
		{
			TUNE_GROUP_FLOAT(CAM_FPS, fExitVehiclePositionInterp, 0.11f, 0.0f, 1.0f, 0.001f);
			TUNE_GROUP_FLOAT(CAM_FPS, fExitVehicleBlendPitchZeroPhase, 0.70f, 0.0f, 1.0f, 0.001f);
			m_ExitInterpRate = Lerp(Min(fExitVehiclePositionInterp * 30.0f * fwTimer::GetCamTimeStep(), 1.0f), m_ExitInterpRate, 1.0f);
			if(m_ExitInterpRate < (1.0f - 0.001f))
			{
				if(pModelSeatInfo)
				{
					// m_VehicleTargetSeatIndex is the destination seat and m_VehicleSeatIndex is the entry seat.
					const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleSeatIndex );
					Matrix34 seatMtx;
					m_pVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);

					Vector3 cameraPosition = m_Frame.GetPosition();
					bool bVehicleCameraPositionValid = ComputeInitialVehicleCameraPosition(attachPed, seatMtx, vehicleCameraPosition);

					float fPercentToEndBlend = 0.25f;
					if(m_pVehicle->GetIsAircraft() || m_IsABikeOrJetski)
					{
						fPercentToEndBlend = 0.125f;
					}
					const float c_fMaxVehicleVelocityForBlend = 6.0f;
					float fVehicleVelocity = m_pVehicle->GetVelocity().Mag();
					fPercentToEndBlend = RampValueSafe(fVehicleVelocity, 0.0f, c_fMaxVehicleVelocityForBlend, fPercentToEndBlend, 0.01f);
					float fInterpRateToUse = RampValueSafe(m_ExitInterpRate, 0.0f, fPercentToEndBlend, 0.0f, 1.0f);

					if(fInterpRateToUse <= 1.0f)
					{
						if(fInterpRateToUse < (1.0f - SMALL_FLOAT))
						{
							Mat34V pedMatrix = attachPed->GetMatrix();
							Vec3V relativeCameraPosition = UnTransformFull(pedMatrix, RCC_VEC3V(cameraPosition));
							Vec3V relativeVehicleCamPosition = m_PreviousRelativeCameraPosition;
							if(bVehicleCameraPositionValid)
							{
								// Since vehicleCameraPosition is relative to ped when the exit starts, use seatMtx as an approximation of ped's starting matrix.
								relativeVehicleCamPosition = UnTransformFull(RCC_MAT34V(seatMtx), RCC_VEC3V(vehicleCameraPosition));
							}
							Vec3V blendedRelativePosition = Lerp( ScalarV(fInterpRateToUse), relativeVehicleCamPosition, relativeCameraPosition );
							m_ObjectSpacePosition = VEC3V_TO_VECTOR3(blendedRelativePosition);
							RC_VEC3V(cameraPosition) = Transform(pedMatrix, blendedRelativePosition);
						}

						PushCameraForwardForHeadAndArms( attachPed, cameraPosition, m_Frame.GetPosition(), seatMtx, playerHeadMatrix, pitch, false );

						m_Frame.SetPosition( cameraPosition );

						if(bVehicleCameraPositionValid)
						{
							m_OverriddenNearClip = Lerp(fInterpRateToUse, m_PovCameraNearClip, m_Metadata.m_BaseNearClip);
							m_ShouldOverrideNearClipThisUpdate = true;
						}
					}

					float fRollInterp = RampValueSafe(1.0f - m_ExitInterpRate, fVehicleEntryMinPhaseForRollBlend, fVehicleEntryMaxPhaseForRollBlend, 0.0f, 1.0f);
					desiredRoll = Lerp(fRollInterp, desiredRoll, m_ExitVehicleRoll);
				}
			}
			else
			{
				m_ExitInterpRate = 1.0f;
			}

			if(!bIsAPlaneOrHelo && m_ExitInterpRate >= fExitVehicleBlendPitchZeroPhase)
			{
				fHeadBonePitch = oParam.targetPitch;
			}
			fHeadBonePitch = Clamp(fHeadBonePitch, m_MinPitch, m_MaxPitch);

			const float fHeadingSpringConstant = (!m_ClosingDoorFromOutside) ? m_Metadata.m_VehicleExitHeadingSpring.m_SpringConstant : 1.0f;
			heading = m_VehicleHeadingSpring.UpdateAngular(fExitHeading,
							fHeadingSpringConstant, 
							m_Metadata.m_VehicleExitHeadingSpring.m_SpringDampingRatio);
			pitch   = m_VehiclePitchSpring.UpdateAngular(fHeadBonePitch,
							m_Metadata.m_VehicleExitPitchSpring.m_SpringConstant, 
							m_Metadata.m_VehicleExitPitchSpring.m_SpringDampingRatio);
		}
	}


	// Clamp heading/pitch to limits.
	if(m_pVehicle)
	{
		// Make heading/pitch relative to move.
		heading = fwAngle::LimitRadianAngle(heading - fHeadingRelativeTo);
		pitch   = fwAngle::LimitRadianAngle(pitch   - fPitchRelativeTo);

		// Clamp relative to mover.  Use smaller limit if car jacking or if a seat shuffle is required.
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fEnterVehicleInitialHeadingLimit, 70.0f, 0.0f, 90.0f, 0.10f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fCarJackingHeadingLimit, 50.0f, 0.0f, 90.0f, 0.10f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fHeloJackingHeadingLimit, 30.0f, 0.0f, 90.0f, 0.10f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fPlaneExitHeadingLimit, 30.0f, 0.0f, 90.0f, 0.10f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fMinPitchTurnedForVehicleEntryExit, -20.0f, -60.0f, 0.0f, 1.0f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fJetSkiHeadingLimitScale, 0.50f, 0.0f, 1.0f, 0.001f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fBoatHeadingLimitScale, 0.40f, 0.0f, 1.0f, 0.001f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fNoDoorHeadingLimitScale, 0.15f, 0.0f, 1.0f, 0.001f);
		TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fTripleHeadCarJackHeadingLimitScale, 0.06f, 0.0f, 1.0f, 0.001f);
		TUNE_GROUP_BOOL(CAM_FPS_ENTER_EXIT, bUseSpringForHeadingLimits, true);
		float fVehicleEntryHeadingLimitRadians = ((!m_IsCarJacking && m_VehicleTargetSeatIndex == m_VehicleSeatIndex) || m_ExitingVehicle) ? fEnterVehicleInitialHeadingLimit*DtoR : fCarJackingHeadingLimit*DtoR;
		float fVehicleEntryLookUpLimitRadians  = fEnterVehicleInitialPitchLimit * DtoR;
		if(m_ExitingVehicle && m_pVehicle->InheritsFromPlane())
		{
			fVehicleEntryHeadingLimitRadians = fPlaneExitHeadingLimit*DtoR;
		}

		if(m_BlendFromHotwiring)
		{
			fLookDownLimit = fMinPitchForHotwiring*DtoR;
		}
		else if(m_VehSeatInterp > 0.0f)
		{
			fLookDownLimit = RampValueSafe(Abs(heading), 0.0f, fEnterVehicleInitialHeadingLimit*DtoR, fLookDownLimit, fMinPitchTurnedForVehicleEntryExit * DtoR);
		}

		if(m_bEnteringTurretSeat)
		{
			fLookDownLimit = -70.0f*DtoR;
		}

		fVehicleEntryHeadingLimitRadians = RampValue(Abs(pitch), 20.0f*DtoR, 40.0f*DtoR, fVehicleEntryHeadingLimitRadians, fVehicleEntryHeadingLimitRadians*0.50f);
		if(m_IsCarJacking)
		{
			fLookDownLimit = RampValue(Abs(heading), fVehicleEntryHeadingLimitRadians*0.50f, fVehicleEntryHeadingLimitRadians, fLookDownLimit, fLookDownLimit*0.75f);
			if(m_pVehicle->GetIsRotaryAircraft())
			{
				fVehicleEntryHeadingLimitRadians = fHeloJackingHeadingLimit*DtoR;
				fVehicleEntryLookUpLimitRadians *= 0.50f;
			}
		}
		else if(m_ExitingVehicle)
		{
			fVehicleEntryHeadingLimitRadians = RampValueSafe(m_ExitInterpRate, 0.60f, 1.0f, fVehicleEntryHeadingLimitRadians, fVehicleEntryHeadingLimitRadians*1.25f);
		}

		if(!m_IsCarJacking && m_CarJackStartTime != 0)
		{
			m_ClampVehicleEntryHeading = true;
		}

		SpringInputParams paramSpringInput;
		paramSpringInput.bEnable = !oParam.bUsingHeadSpineOrientation && !m_bEnteringTurretSeat;
		// Enable relative input during blendout as we are now blending out the underlying heading/pitch.
		////paramSpringInput.bEnable |= m_bWasCustomSituation || m_bWasUsingHeadSpineOrientation;
		paramSpringInput.bAllowHorizontal = !m_bUsingMobile || m_bParachutingOpenedChute;
		paramSpringInput.bAllowLookDown = false;
	#if RSG_PC
		paramSpringInput.bEnableSpring = !m_CurrentControlHelper->WasUsingKeyboardMouseLastInput();
	#else
		paramSpringInput.bEnableSpring = true;
	#endif
		paramSpringInput.bReset = false;
		paramSpringInput.headingInput = oParam.lookAroundHeadingOffset;
		paramSpringInput.pitchInput = oParam.lookAroundPitchOffset;

		CalculateRelativeHeadingPitch( paramSpringInput, (m_bLookingBehind && !bReturn) );

		float fPitchLimitScale = (m_ClampVehicleEntryPitch) ? 1.0f : 0.75f;
		float fHeadingimitScale = (m_ClampVehicleEntryHeading) ? 1.0f : m_EntryHeadingLimitScale;
		if(m_pVehicle->GetIsJetSki() || m_bEnteringTurretSeat)
		{
			fHeadingimitScale = fJetSkiHeadingLimitScale;
		}
		else if(m_pVehicle->InheritsFromBoat())
		{
			fHeadingimitScale = fBoatHeadingLimitScale;
		}
		else if(!bCarSeatAlwaysHadDoor)
		{
			fHeadingimitScale = RampValue(m_VehSeatInterp, 0.50, 1.00, 0.0f, fNoDoorHeadingLimitScale);
		}

		float fLeftHeadingLimit = fVehicleEntryHeadingLimitRadians*fHeadingimitScale;
		float fRightHeadingLimit = -fVehicleEntryHeadingLimitRadians*fHeadingimitScale;

	#if RSG_PC
		      float fTripleHeadScale = (!m_TripleHead) ? 1.0f : 0.50f;
		const float fTripleHeadLimit = (!m_TripleHead) ? 0.0f : RampValueSafe(m_VehSeatInterp, 0.80f, 1.0f, 10.0f*DtoR, 0.0f);
		const float fTripleHeadExitLimit = (!m_TripleHead) ? 0.0f : RampValueSafe(m_VehSeatInterp, 0.80f, 1.0f, 16.0f*DtoR, 0.0f);
	#else
		const float fTripleHeadScale = 1.0f;
		const float fTripleHeadLimit = 0.0f;
		const float fTripleHeadExitLimit = 0.0f;
	#endif
		bool bIsRightSideEntry = false;
		if(m_pVehicle->IsEntryIndexValid(m_VehicleInitialSeatIndex) && m_pVehicle->GetEntryInfo(m_VehicleInitialSeatIndex))
		{
			bIsRightSideEntry = (m_pVehicle->GetEntryInfo(m_VehicleInitialSeatIndex)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? false : true);
			if(
		#if RSG_PC
				(m_TripleHead && m_EnteringVehicle && (!m_IsABikeOrJetski || m_pVehicle->GetIsJetSki())) ||
		#endif
				m_IsCarJacking )
			{
				float fBlendOutLimitInterp = 0.0f;
				if(m_CarJackStartTime == 0)
				{
					m_CarJackStartTime = fwTimer::GetTimeInMilliseconds();
				}
				else
				{
					fBlendOutLimitInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_CarJackStartTime) / (float)c_CarJackBlendOutLimitDuration;
					fBlendOutLimitInterp = Clamp(fBlendOutLimitInterp, 0.0f, 1.0f);
				}

			#if RSG_PC
				if(m_TripleHead && m_EnteringVehicle)
				{
					if(!m_IsCarJacking)
					{
						fBlendOutLimitInterp = Clamp(m_VehSeatInterp, 0.0f, 1.0f);
					}
					else
					{
						fTripleHeadScale = fTripleHeadCarJackHeadingLimitScale;
						if(m_pVehicle->GetIsRotaryAircraft())
						{
							if(!bIsRightSideEntry)
								fRightHeadingLimit = 0.0f;
							else
								fLeftHeadingLimit  = 0.0f;
						}
						else
						{
							if(!bIsRightSideEntry)
								fLeftHeadingLimit  = fLeftHeadingLimit*fTripleHeadScale;
							else
								fRightHeadingLimit = fRightHeadingLimit*fTripleHeadScale;
						}
					}
				}
			#endif

				if(m_pVehicle->GetIsRotaryAircraft() WIN32PC_ONLY( && (!m_TripleHead || !m_IsCarJacking) ))
				{
					// Zero heading limit on bad side.
					if(!bIsRightSideEntry)
						fRightHeadingLimit = fRightHeadingLimit*fTripleHeadScale;//Lerp(fBlendOutLimitInterp, fRightHeadingLimit*fTripleHeadScale, fTripleHeadLimit);
					else
						fLeftHeadingLimit = fLeftHeadingLimit*fTripleHeadScale; //Lerp(fBlendOutLimitInterp, fLeftHeadingLimit*fTripleHeadScale, -fTripleHeadLimit);
				}
				else if(m_pVehicle->InheritsFromBoat() && !m_IsABikeOrJetski)
				{
					bool bZeroBadSide = false;
					if( m_pVehicle && m_pVehicle->GetVehicleModelInfo() && m_VehicleTargetSeatIndex >= 0 )
					{
						if(pModelSeatInfo)
						{
							// m_VehicleTargetSeatIndex is the destination seat and m_VehicleSeatIndex is the entry seat.
							const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleTargetSeatIndex );
							Matrix34 seatMtx;
							m_pVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);

							Vector3 vDelta = seatMtx.d - vRootPosition;//playerHeadMatrix.d;
							float fDot = vDelta.Dot(seatMtx.b);
							if (fDot < -0.25f)
							{
								bZeroBadSide = true;
							}
						}
					}

					if(bZeroBadSide)
					{
						bIsRightSideEntry = false;
						if( GetEntrySide(attachPed, m_pVehicle.Get(), m_VehicleInitialSeatIndex, bIsRightSideEntry) )
						{
							// Zero heading limit on bad side.
							if(bIsRightSideEntry)
							{
								fLeftHeadingLimit = Lerp(fBlendOutLimitInterp, fLeftHeadingLimit*fTripleHeadScale, -fTripleHeadLimit);
								fRightHeadingLimit = Min(fRightHeadingLimit, fLeftHeadingLimit);
							}
							else
							{
								fRightHeadingLimit = Lerp(fBlendOutLimitInterp, fRightHeadingLimit*fTripleHeadScale, fTripleHeadLimit);
								fLeftHeadingLimit = Max(fLeftHeadingLimit, fRightHeadingLimit);
							}
						}
					}
					else
					{
						fLeftHeadingLimit  =  fVehicleEntryHeadingLimitRadians*0.50f*fTripleHeadScale;
						fRightHeadingLimit = -fVehicleEntryHeadingLimitRadians*0.50f*fTripleHeadScale;
					}
				}
				else if(m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
				{
					// Zero heading limit on bad side.
					const float fCarTripleHeadingLimit = ((!m_IsCarJacking) ? 7.5f : 15.0f) * DtoR;
					if(bIsRightSideEntry)
					{
						fLeftHeadingLimit = Lerp(fBlendOutLimitInterp, fLeftHeadingLimit*fTripleHeadScale, fCarTripleHeadingLimit);
						fRightHeadingLimit = Min(fRightHeadingLimit, fLeftHeadingLimit);
					}
					else
					{
						fRightHeadingLimit = Lerp(fBlendOutLimitInterp, fRightHeadingLimit*fTripleHeadScale, -fCarTripleHeadingLimit);
						fLeftHeadingLimit = Max(fLeftHeadingLimit, fRightHeadingLimit);
					}
				}
				else if(m_IsABikeOrJetski)
				{
					// Instead of figuring out which side we are car jacking from, just blend both sides.
					// (also means, don't have to worry about which side the ped will be thrown)
					const float fJetSkiMinHeadingLimit = 6.0f * DtoR;
					float fScaleToApply = (m_pVehicle && m_pVehicle->GetIsJetSki()) ? fTripleHeadScale : 1.0f;
					fRightHeadingLimit = Lerp(fBlendOutLimitInterp, fRightHeadingLimit*fScaleToApply, -fJetSkiMinHeadingLimit);
					fLeftHeadingLimit = Lerp(fBlendOutLimitInterp, fLeftHeadingLimit*fScaleToApply, fJetSkiMinHeadingLimit);
				}
			}
			else if(m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || m_pVehicle->GetIsRotaryAircraft() || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
			{
				if(bIsRightSideEntry)
				{
					// smaller limit for right side entry
					fLeftHeadingLimit = Min(fLeftHeadingLimit, fVehicleEntryHeadingLimitRadians*m_EntryHeadingLimitScale*0.50f);
					if(fLeftHeadingLimit == 0.0f)
						fLeftHeadingLimit = -fTripleHeadExitLimit;
					fRightHeadingLimit = Min(fRightHeadingLimit, fLeftHeadingLimit);
				}
				else
				{
					// reduce limit for left side (clipping in triple head)
					fRightHeadingLimit = Max(fRightHeadingLimit, -fVehicleEntryHeadingLimitRadians*m_EntryHeadingLimitScale*0.60f*fTripleHeadScale);
					if(fRightHeadingLimit == 0.0f)
						fRightHeadingLimit = fTripleHeadExitLimit;
					fLeftHeadingLimit = Max(fLeftHeadingLimit, fRightHeadingLimit);
				}
			}

			if( m_EnteringVehicle || m_bEnterBlending ||
				m_IsCarJacking    || (m_CarJackStartTime != 0 && m_VehSeatInterp < SMALL_FLOAT) )
			{
				if(m_VehSeatInterp < 1.0f && m_pVehicle->InheritsFromPlane())
				{
					bIsRightSideEntry = false;
					if( GetEntrySide(attachPed, m_pVehicle.Get(), m_VehicleInitialSeatIndex, bIsRightSideEntry) )
					{
						// Restrict heading limit on bad side.
						if(bIsRightSideEntry)
						{
							fRightHeadingLimit = (m_VehicleInitialSeatIndex == m_VehicleTargetSeatIndex) ? fRightHeadingLimit * 0.15f : fRightHeadingLimit * 0.50f;
							fLeftHeadingLimit *= 0.5f;
						}
						else
						{
							fLeftHeadingLimit   = (m_VehicleInitialSeatIndex == m_VehicleTargetSeatIndex) ? fLeftHeadingLimit * 0.15f : fLeftHeadingLimit * 0.50f;
							fRightHeadingLimit *= 0.5f;
						}
					}
				}
			}
		}

		float fTempPitch   = Clamp(pitch,   fLookDownLimit*fPitchLimitScale, fVehicleEntryLookUpLimitRadians*fPitchLimitScale);
		float fTempHeading = Clamp(heading, fRightHeadingLimit, fLeftHeadingLimit);
		m_ClampVehicleEntryHeading |= (heading == fTempHeading);
		m_ClampVehicleEntryPitch   |= (pitch == fTempPitch);
		float fFinalHeading = heading + m_RelativeHeading;
		fTempHeading = Clamp(fFinalHeading, fRightHeadingLimit, fLeftHeadingLimit);
		float fFinalPitch = pitch + m_RelativePitch;
		fTempPitch = Clamp(fFinalPitch, fLookDownLimit*fPitchLimitScale, fVehicleEntryLookUpLimitRadians*fPitchLimitScale);

		if(m_ResetEntryExitSprings)
		{
			// Init the springs in case we have to clamp on first update. (esp. for player being car jacked when looking to the side)
			m_VehicleEntryHeadingLimitSpring.OverrideResult( fFinalHeading );
			m_VehicleEntryPitchLimitSpring.OverrideResult( fFinalPitch );
		}

		if(m_ClampVehicleEntryHeading && fFinalHeading != fTempHeading)
		{
			float springConstantForHeadingLimit = m_Metadata.m_VehicleHeadingLimitSpring.m_SpringConstant;
			if(m_IsCarJacking && m_pVehicle->InheritsFromPlane())
			{
				springConstantForHeadingLimit *= 3.50f;
			}

			float difference = fTempHeading - fFinalHeading;
			if( Abs(m_RelativeHeading) > Abs(difference) &&
				((difference > 0.0f && m_RelativeHeading < 0.0f) ||
				 (difference < 0.0f && m_RelativeHeading > 0.0f)) )
			{
				if(bUseSpringForHeadingLimits)
				{
					////m_VehicleEntryHeadingLimitSpring.OverrideResult( m_RelativeHeading );
					//m_RelativeHeading = m_VehicleEntryHeadingLimitSpring.Update( fwAngle::LimitRadianAngle(m_RelativeHeading+difference),
					//										springConstantForHeadingLimit, 
					//										m_Metadata.m_VehicleHeadingLimitSpring.m_SpringDampingRatio );
					float newHeading = m_VehicleEntryHeadingLimitSpring.Update( fTempHeading,
											springConstantForHeadingLimit, 
											m_Metadata.m_VehicleHeadingLimitSpring.m_SpringDampingRatio );
					float amountRemoved = fwAngle::LimitRadianAngle(fFinalHeading - newHeading);
					m_RelativeHeading  -= amountRemoved;
				}
				else
				{
					m_RelativeHeading += difference;
				}
			}
			else
			{
				if(bUseSpringForHeadingLimits)
				{
					////m_VehicleEntryHeadingLimitSpring.OverrideResult( fFinalHeading );
					float fStartHeading = fFinalHeading;//m_VehicleEntryHeadingLimitSpring.GetResult();
					float newHeading = m_VehicleEntryHeadingLimitSpring.Update( fTempHeading,
											springConstantForHeadingLimit, 
											m_Metadata.m_VehicleHeadingLimitSpring.m_SpringDampingRatio );
					float amountRemoved = fwAngle::LimitRadianAngle(fStartHeading - newHeading);
					if(Abs(m_RelativeHeading) < Abs(amountRemoved))
					{
						heading = fwAngle::LimitRadianAngle(heading - (amountRemoved - m_RelativeHeading));
						m_RelativeHeadingSpring.Reset( 0.0f );
						m_RelativeHeading = 0.0f;
					}
					else
					{
						m_RelativeHeading = fwAngle::LimitRadianAngle(m_RelativeHeading - amountRemoved);
					}
				}
				else
				{
					// Relative heading is trying to pull towards limit or
					// difference is bigger than the relative heading.
					if(m_ClampVehicleEntryHeading)
					{
						heading = fTempHeading;
					}
					m_RelativeHeadingSpring.Reset( 0.0f );
					m_RelativeHeading = 0.0f;
				}
			}
		}
		else
		{
			m_VehicleEntryHeadingLimitSpring.OverrideResult( fFinalHeading );
		}

		if(m_ClampVehicleEntryPitch && fFinalPitch != fTempPitch)
		{
			float springConstantForPitchLimit = m_Metadata.m_VehiclePitchLimitSpring.m_SpringConstant;
			////if(m_pVehicle->InheritsFromPlane())
			////{
			////	springConstantForPitchLimit *= 2.0f;
			////}

			float difference = fTempPitch - fFinalPitch;
			if( Abs(m_RelativePitch) > Abs(difference) &&
				((difference > 0.0f && m_RelativePitch < 0.0f) ||
				 (difference < 0.0f && m_RelativePitch > 0.0f)) )
			{
				////m_VehicleEntryPitchLimitSpring.OverrideResult( m_RelativePitch );
				//m_RelativePitch = m_VehicleEntryPitchLimitSpring.Update( fwAngle::LimitRadianAngle(m_RelativePitch+difference),
				//										springConstantForPitchLimit, 
				//										m_Metadata.m_VehiclePitchLimitSpring.m_SpringDampingRatio );
				float newPitch = m_VehicleEntryPitchLimitSpring.Update( fTempPitch,
										springConstantForPitchLimit, 
										m_Metadata.m_VehiclePitchLimitSpring.m_SpringDampingRatio );
				float amountRemoved = fwAngle::LimitRadianAngle(fFinalPitch - newPitch);
				m_RelativePitch    -= amountRemoved;
			}
			else
			{
				////m_VehicleEntryPitchLimitSpring.OverrideResult( fFinalPitch );
				float fStartPitch = fFinalPitch; //m_VehicleEntryPitchLimitSpring.GetResult();
				float newPitch = m_VehicleEntryPitchLimitSpring.Update( fTempPitch,
										springConstantForPitchLimit, 
										m_Metadata.m_VehiclePitchLimitSpring.m_SpringDampingRatio );
				float amountRemoved = fwAngle::LimitRadianAngle(fStartPitch - newPitch);
				if(Abs(m_RelativePitch) < Abs(amountRemoved))
				{
					pitch = fwAngle::LimitRadianAngle(pitch - (amountRemoved - m_RelativePitch));
					m_RelativePitchSpring.Reset( 0.0f );
					m_RelativePitch = 0.0f;
				}
				else
				{
					m_RelativePitch = fwAngle::LimitRadianAngle(m_RelativePitch - amountRemoved);
				}
			}
		}
		else
		{
			m_VehicleEntryPitchLimitSpring.OverrideResult( fFinalPitch );
		}

		// Convert back to non-relative heading/pitch
		heading = fwAngle::LimitRadianAngle(heading + fHeadingRelativeTo);
		pitch   = fwAngle::LimitRadianAngle(pitch   + fPitchRelativeTo);
	}

	bReturn = true;

	// When overriding orientation, it is no longer relative to ped's orientation.
	bAllowLookAroundDuringCustomSituation = true;
	bOrientationRelativeToAttachedPed = false;

	m_ResetEntryExitSprings = false;
	return (bReturn);
}

bool camFirstPersonShooterCamera::GetEntrySide(const CPed* attachPed, const CVehicle* pVehicle, s32 seatIndex, bool& bIsRightSideEntry)
{
	if(pVehicle && pVehicle->GetVehicleModelInfo())
	{
		const CModelSeatInfo* pModelSeatInfo = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if(pModelSeatInfo)
		{
			const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( seatIndex );
			if(iSeatBoneIndex >= 0)
			{
				Matrix34 seatMtx;
				pVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);

				Vector3 vPedPosition = VEC3V_TO_VECTOR3(attachPed->GetMatrix().GetCol3());
				float fTempDot = seatMtx.a.Dot(vPedPosition - seatMtx.d);
				bIsRightSideEntry = fTempDot >= 0.0f;
				return true;
			}
		}
	}

	return false;
}

bool camFirstPersonShooterCamera::HandlePlayerGettingCarJacked(const CPed* attachPed, const Vector3& playerHeadPosition, const Vector3& playerSpinePosition, float& heading, float& pitch, float& lookDownPitchLimit)
{
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackedHeadingInterp, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackedPitchInterp, 0.40f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fBeingJackedLookAtJackerDistance, 0.30f, -1.0f, 1.5f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackedPitchLimit, -55.0f, -65.0f, 0.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fAngleToStopLookingAtCarJackerUp, 125.0f, 60.0f, 180.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fAngleToStopLookingAtCarJackerDown, 95.0f, 60.0f, 180.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fDistanceToStopLookingAtInsideCarJacker, 0.075f, 0.03f, 1.0f, 0.001f);

	const CTaskExitVehicle* pExitVehicleTask  = (const CTaskExitVehicle*)(attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
	if( (m_JackedFromVehicle || attachPed->GetPedResetFlag(CPED_RESET_FLAG_BeingJacked)) && attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) &&
		pExitVehicleTask && pExitVehicleTask->GetPedsJacker() )
	{
		const CModelSeatInfo* pModelSeatInfo = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if(pModelSeatInfo)
		{
			// m_VehicleTargetSeatIndex is the destination seat and m_VehicleSeatIndex is the entry seat.
			const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_VehicleSeatIndex );
			Matrix34 seatMtx;
			m_pVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);

			const CPed* pCarJacker = pExitVehicleTask->GetPedsJacker();
			Matrix34 mtxSpine;
			s32 spineBoneIndex = (pCarJacker) ? pCarJacker->GetBoneIndexFromBoneTag(BONETAG_SPINE3) : -1;
			if(spineBoneIndex >= 0)
			{
				pCarJacker->GetGlobalMtx(spineBoneIndex, mtxSpine);

				Vector3 vTarget = mtxSpine.d;
				lookDownPitchLimit = fVehicleJackedPitchLimit * DtoR;

				// Calculate heading and pitch to look at target.
				float fTargetHeading = m_AttachParentHeading;
				float fTargetPitch   = m_AttachParentPitch;
				Vector3 vDelta = vTarget - playerHeadPosition;
				if( vDelta.Mag2() > SMALL_FLOAT*SMALL_FLOAT )
				{
					vDelta.Normalize();
					fTargetHeading = rage::Atan2f(-vDelta.x, vDelta.y);
					fTargetPitch   = AsinfSafe(vDelta.z);
				}

				float fAngleToStopLookingAtCarJacker = fAngleToStopLookingAtCarJackerUp;
				if( (playerSpinePosition.z - mtxSpine.d.z) > 0.10f )
					fAngleToStopLookingAtCarJacker = fAngleToStopLookingAtCarJackerDown;

				float vHeadingDelta = SubtractAngleShorter(fTargetHeading, m_AttachParentHeading);
				Vector3 vDeltaSeatToPlayerHead  = playerSpinePosition - seatMtx.d;
				Vector3 vDeltaSeatToJackerSpine = mtxSpine.d - seatMtx.d;
				vDeltaSeatToPlayerHead.z = 0.0f;
				vDeltaSeatToJackerSpine.z = 0.0f;
				float fDotSeatToPlayerHead  = vDeltaSeatToPlayerHead.Dot( seatMtx.a );
				float fDotSeatToJackerSpine = vDeltaSeatToJackerSpine.Dot(seatMtx.a);
				const bool bIsPlayerOnRightSide = (m_pVehicle->GetEntryInfo(m_VehicleSeatIndex)) ? (m_pVehicle->GetEntryInfo(m_VehicleSeatIndex)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? false : true) : false;
				if(!pCarJacker->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
				{
					// Car jacker is NOT in vehicle, so player is being pulled out.
					if( (!bIsPlayerOnRightSide && (fDotSeatToPlayerHead - fBeingJackedLookAtJackerDistance) < fDotSeatToJackerSpine) ||
						( bIsPlayerOnRightSide && (fDotSeatToPlayerHead + fBeingJackedLookAtJackerDistance) > fDotSeatToJackerSpine) ||
						Abs(vHeadingDelta) > fAngleToStopLookingAtCarJacker*DtoR)
					{
						// If distance from player head to seat (along seat right vector) is bigger thatn distance from car jacker
						// spine to seat, (i.e. if player is further from the seat than the car jacker) then stop looking at him.
						m_AllowLookAtVehicleJacker = false;
					}
				}
				else
				{
					// Car jacker is in vehicle, so player is being pushed out.
					if(Abs(fDotSeatToPlayerHead) > fDistanceToStopLookingAtInsideCarJacker)
					{
						// Once player is far enough from the seat, stop looking at car jacker.
						m_AllowLookAtVehicleJacker = false;
					}
				}

				// TODO: need spring here?  It's fine without it.
				heading = fwAngle::LerpTowards(heading, fTargetHeading, fVehicleJackedHeadingInterp * 30.0f * fwTimer::GetTimeStep());
				pitch = Lerp(fVehicleJackedPitchInterp * 30.0f * fwTimer::GetTimeStep(), pitch, fTargetPitch);
			}
		}

		return true;
	}

	return false;
}

void camFirstPersonShooterCamera::PushCameraForwardForHeadAndArms(const CPed* attachPed, Vector3& cameraPosition, const Vector3& finalPosition,
																  const Matrix34& rootMtx,const Matrix34& mtxPlayerHead, float& pitch, bool bAffectPitch)
{
	// Do not let the blended camera be behind where the real camera would have been. (using seat matrix forward as forward direction)
	Matrix34 mtxUpperArmRight;
	s32 boneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_R_UPPERARM);
	if(boneIndex >= 0)
		attachPed->GetGlobalMtx(boneIndex, mtxUpperArmRight);
	else
		mtxUpperArmRight.Identity();

	Matrix34 mtxUpperArmLeft;
	boneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_L_UPPERARM);		// check left arm also, in case we get in from passenger side
	if(boneIndex >= 0)
		attachPed->GetGlobalMtx(boneIndex, mtxUpperArmLeft);
	else
		mtxUpperArmLeft.Identity();

	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fMaxArmOffsetRelativeToHead, 0.10f, 0.0f, 0.5f, 0.005f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fMaxPushAheadDistForPitchDownDuringVehicleEntry,  0.50f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fMinPhaseToBlendOutExtraPitchDown, 0.70f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fPitchDownDuringVehicleEntry,  -20.0f, -75.0f, 0.0f, 1.0f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fPushAheadBlendinRate,  0.70f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, fPushAheadBlendoutRate, 0.35f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, c_PushAheadDistanceBlendinSpringConstant, 35.0f, 1.0f, 100.0f, 0.05f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, c_PushAheadDistanceBlendoutSpringConstant, 35.0f, 1.0f, 100.0f, 0.05f);
	TUNE_GROUP_FLOAT(CAM_FPS_ENTER_EXIT, c_PushAheadPitchSpringConstant, 35.0f, 1.0f, 100.0f, 0.05f);

	s32 startingSeatIndex = m_VehicleSeatIndex;
	if( m_pVehicle->IsEntryIndexValid(m_VehicleInitialSeatIndex) )
	{
		startingSeatIndex = m_VehicleInitialSeatIndex;
	}

	bool bIsRightSideEntry = (m_pVehicle->GetEntryInfo(startingSeatIndex)) ? (m_pVehicle->GetEntryInfo(startingSeatIndex)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? false : true) : false;

	Vector3 vDeltaSeatToHead = mtxPlayerHead.d - rootMtx.d;
	Vector3 vDeltaSeatToRightArm = mtxUpperArmRight.d - rootMtx.d;
	Vector3 vDeltaSeatToLeftArm = mtxUpperArmLeft.d - rootMtx.d;
	Vector3 vDeltaSeatToBlendedCamera = cameraPosition - rootMtx.d;
	float fDotSeatToHead = vDeltaSeatToHead.Dot( rootMtx.b );
	float fDotSeatToRightArm = Min(vDeltaSeatToRightArm.Dot(rootMtx.b), fDotSeatToHead + fMaxArmOffsetRelativeToHead);
	float fDotSeatToLeftArm  = Min(vDeltaSeatToLeftArm.Dot(rootMtx.b),  fDotSeatToHead + fMaxArmOffsetRelativeToHead);
	float fDotSeatToBlendedCamera = vDeltaSeatToBlendedCamera.Dot( rootMtx.b );
	if( fDotSeatToHead > fDotSeatToBlendedCamera ||
		fDotSeatToRightArm > fDotSeatToBlendedCamera ||
		fDotSeatToLeftArm > fDotSeatToBlendedCamera)
	{
		// Blended camera position would have been behind the real camera (and possibly in/behind the player's head)
		// so push the blended camera forward by the difference.
		float fDifference;
		float fInstantDifference = 0.0f;
		if(!m_ExitingVehicle)
		{
			// To reduce the poppiness, force camera to be pushed by head and leading arm offset.
			// Trailing arm offset can be blended in.
			// TODO: do this for exits also?
			if(m_IsABikeOrJetski)
			{
				fInstantDifference = Max(Max(fDotSeatToHead, fDotSeatToRightArm), fDotSeatToLeftArm) - fDotSeatToBlendedCamera;
				fDifference = 0.0f;
			}
			else if(!bIsRightSideEntry)
			{
				fInstantDifference = Max(fDotSeatToHead, fDotSeatToRightArm) - fDotSeatToBlendedCamera;
				fDifference = fDotSeatToLeftArm - fDotSeatToBlendedCamera;
			}
			else
			{
				fInstantDifference = Max(fDotSeatToHead, fDotSeatToLeftArm) - fDotSeatToBlendedCamera;
				fDifference = fDotSeatToRightArm - fDotSeatToBlendedCamera;
			}

			fInstantDifference = Max(fInstantDifference, 0.0f);
			fDifference = Max(fDifference, 0.0f);
		}
		else
		{
			fDifference = Max(Max(fDotSeatToHead, fDotSeatToRightArm), fDotSeatToLeftArm) - fDotSeatToBlendedCamera;
		}

		if(fDifference >= m_VehicleEntryExitPushAheadOffset && fDifference > fInstantDifference)
		{
			const float blendRate = Min(fPushAheadBlendinRate * 30.0f * fwTimer::GetCamTimeStep(), 1.0f);
			m_VehicleEntryExitPushAheadOffset += Max(fInstantDifference - m_VehicleEntryExitPushAheadOffset, 0.0f);
			m_PushAheadDistanceSpring.OverrideResult(m_VehicleEntryExitPushAheadOffset);
			m_VehicleEntryExitPushAheadOffset = Lerp(blendRate, m_VehicleEntryExitPushAheadOffset, (fDifference-fInstantDifference));
		}
		else if(fInstantDifference >= m_VehicleEntryExitPushAheadOffset)
		{
			m_VehicleEntryExitPushAheadOffset = fInstantDifference;
			m_PushAheadDistanceSpring.OverrideResult(fInstantDifference);
		}
		else
		{
			fDifference = Max(fDifference, fInstantDifference);
			m_VehicleEntryExitPushAheadOffset = fDifference;
		}

		m_VehicleEntryExitPushAheadOffset = m_PushAheadDistanceSpring.Update(m_VehicleEntryExitPushAheadOffset, c_PushAheadDistanceBlendinSpringConstant, 1.0f);
	}
	else
	{
		m_VehicleEntryExitPushAheadOffset = m_PushAheadDistanceSpring.Update(0.0f, c_PushAheadDistanceBlendoutSpringConstant, 1.0f);
	}

	if(m_IsABikeOrJetski)
	{
		WorldProbe::CShapeTestCapsuleDesc capsuleTest;
		WorldProbe::CShapeTestFixedResults<1> capsuleTestResults;
		capsuleTest.SetResultsStructure(&capsuleTestResults);
		capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		capsuleTest.SetContext(WorldProbe::LOS_Camera);
		capsuleTest.SetIsDirected(true);

		const u32 collisionIncludeFlags = camCollision::GetCollisionIncludeFlagsForClippingTests();
		capsuleTest.SetIncludeFlags(collisionIncludeFlags);

		float radius = 0.025f;
		capsuleTest.SetCapsule(finalPosition, cameraPosition, radius);
		if(m_pVehicle)
		{
			capsuleTest.SetExcludeEntity(m_pVehicle, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}

		if( WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest) )
		{
			Vector3 vDelta = capsuleTestResults[0].GetHitPosition() - cameraPosition;
			float fDotCollision = Max(vDelta.Dot( rootMtx.b ), 0.0f);
			m_VehicleEntryExitPushAheadOffset = Max(m_VehicleEntryExitPushAheadOffset, fDotCollision);
		}
	}

	if(m_VehicleEntryExitPushAheadOffset > 0.001f)
	{
		cameraPosition += rootMtx.b * m_VehicleEntryExitPushAheadOffset;
		if(bAffectPitch && !m_IsABikeOrJetski)
		{
			float fInterp = Clamp(m_VehicleEntryExitPushAheadOffset / fMaxPushAheadDistForPitchDownDuringVehicleEntry, 0.0f, 1.0f);
			float fExtraPitchToApply = Lerp(fInterp, 0.0f, fPitchDownDuringVehicleEntry*DtoR);
			fExtraPitchToApply = RampValueSafe(m_VehSeatInterp, fMinPhaseToBlendOutExtraPitchDown, 1.0f, m_PushAheadPitchSpring.GetResult(), 0.0f);
			fExtraPitchToApply = m_PushAheadPitchSpring.Update(fExtraPitchToApply, c_PushAheadPitchSpringConstant, 1.0f);
			pitch += fExtraPitchToApply;
		}
	}
}

bool camFirstPersonShooterCamera::AdjustTargetForCarJack(const CPed* attachPed, const Matrix34& seatMtx, const Matrix34& playerSpineMtx, Vector3& vTarget,
														 const Vector3& vRootPosition, float& heading, float& pitch, float& lookDownLimit)
{
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingPitchLimit, -35.0f, -65.0f, 0.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingPitchForBikesLimit, -9.0f, -65.0f, 0.0f, 0.10f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingForwardOffset, 0.30f, 0.0f, 1.0f, 0.005f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingRightSeatDistance, 0.60f, 0.0f, 2.0f, 0.005f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingLookAtVictimDistance, -0.25f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingBoatLookAtVictimDistance, -0.80f, -3.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingPlaneLookAtVictimDistance, -2.00f, -4.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingHeloLookAtVictimDistance, -2.00f, -4.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingMinSeatDistanceToStopLookAtVictim, 0.50f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingMinVerticalDistanceBetweenPlayerAndVictim, 0.625f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fVehicleJackingMaxVerticalDistanceBetweenPlayerAndVictim, 1.50f, 0.0f, 3.0f, 0.01f);
#if RSG_PC
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fTripleHeadLookAtVicDistScale, 1.50f, 1.0f, 3.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fTripleHeadHeliJackingForwardVicOffset, -0.66f, -1.0f, 0.0f, 0.01f);
#endif
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, fPlaneJackingForwardLookOffset, 0.30f, 0.0f, 3.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, c_CarJackTargetZOffset, 0.0f, -1.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_FPS_CAR_JACK, c_RearSeatDistance, -0.25f, -2.0f, 0.0f, 0.01f);

	Vector3 vSpineForward = playerSpineMtx.b;
	const Vector3& playerSpinePosition = playerSpineMtx.d;
	vSpineForward.z = 0.0f;
	vSpineForward.NormalizeSafe();

	Matrix34 mtxSpine;
	int spineBoneIndex = m_pJackedPed->GetBoneIndexFromBoneTag(BONETAG_SPINE3);
	if(spineBoneIndex < 0)
		return false;

	m_pJackedPed->GetGlobalMtx(spineBoneIndex, mtxSpine);

	bool bJackingFromAbove = false;
	bool bIsRightSideEntry = (m_pVehicle->GetEntryInfo(m_VehicleSeatIndex)) ? (m_pVehicle->GetEntryInfo(m_VehicleSeatIndex)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? false : true) : false;
	// m_pVehicle validated before AdjustTargetForCarJack was called.
	if(m_pVehicle->InheritsFromPlane() || (m_pVehicle->InheritsFromBoat() && !m_IsABikeOrJetski))
	{
		bool bTemp;
		if(GetEntrySide(attachPed, m_pVehicle, m_VehicleTargetSeatIndex, bTemp))
		{
			bIsRightSideEntry = bTemp;
		}
	}

	if(!m_CarJackVictimCrossedThreshold)
	{
		Vector3 vDeltaSeatToPlayerHead  = playerSpinePosition - seatMtx.d;
		Vector3 vDeltaSeatToVictimSpine = mtxSpine.d - seatMtx.d;
		float fDotSeatUpToPlayerHead = vDeltaSeatToPlayerHead.Dot( seatMtx.c );
		float fDotSeatUpToVictimSpine = vDeltaSeatToVictimSpine.Dot(seatMtx.c);
		vDeltaSeatToPlayerHead.z = 0.0f;
		vDeltaSeatToVictimSpine.z = 0.0f;
		float fDotSeatRightToPlayerHead = vDeltaSeatToPlayerHead.Dot( seatMtx.a );
		float fDotSeatRightToVictimSpine = vDeltaSeatToVictimSpine.Dot(seatMtx.a);

		float fDistanceToSeatRight = -1.0f;
		float fDistanceToSeatForward = 0.0f;
		bool  bStopLookingAtVictim = false;
		int boneIndex;
		const bool hasValidBoneIndex = attachPed->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_ROOT, boneIndex);
		float fDotRight = 0.0f;
		Vector3 vDeltaRight;
		Vector3 vDeltaForward;
		if(hasValidBoneIndex)
		{
			Matrix34 worldBoneMatrix;
			attachPed->GetGlobalMtx(boneIndex, worldBoneMatrix);
			Vector3 positionPed = worldBoneMatrix.d;
			vDeltaRight = positionPed - seatMtx.d;
			vDeltaForward = vDeltaRight;
			fDotRight = vDeltaRight.Dot(seatMtx.a);
			fDistanceToSeatForward = vDeltaForward.Dot(seatMtx.b);
			vDeltaRight = seatMtx.a * fDotRight;				// Projection of vector from seat to ped bone, resolved against seat's right vector.
			fDistanceToSeatRight = Abs(fDotRight);
		}
		else
		{
			Vector3 positionPed = VEC3V_TO_VECTOR3(attachPed->GetMatrix().GetCol3());
			vDeltaRight = positionPed - seatMtx.d;
			vDeltaRight.z = 0.0f;
			fDistanceToSeatRight = vDeltaRight.Mag();
		}

		if( Abs(fDotSeatRightToVictimSpine) > Abs(fDotSeatRightToPlayerHead) && 
			fDistanceToSeatRight <= fVehicleJackingMinSeatDistanceToStopLookAtVictim && 
			fDistanceToSeatForward >= c_RearSeatDistance)
		{
			bStopLookingAtVictim = true;
		}

		float fDotForwardVectors = seatMtx.b.Dot( playerSpineMtx.b );
		bool bFacingFrontOfCar = (fDotForwardVectors >= -0.50f);

		eAnimBoneTag targetBoneTag = BONETAG_SPINE3;
		float fDistanceToStopLooking = (bFacingFrontOfCar) ? fVehicleJackingLookAtVictimDistance : 0.0f;
		if(m_pVehicle->GetIsAircraft())
		{
			fDistanceToStopLooking = (bFacingFrontOfCar && m_pVehicle->GetIsRotaryAircraft()) ? fVehicleJackingHeloLookAtVictimDistance : fVehicleJackingPlaneLookAtVictimDistance;
		}
		else if(m_pVehicle->InheritsFromBoat() && !m_IsABikeOrJetski)
		{
			fDistanceToStopLooking = fVehicleJackingBoatLookAtVictimDistance;
			targetBoneTag = BONETAG_HEAD;
		}

	#if RSG_PC
		if(m_TripleHead)
		{
			fDistanceToStopLooking *= fTripleHeadLookAtVicDistScale;
		}
	#endif

		const float fVerticalDelta = fDotSeatUpToPlayerHead - fDotSeatUpToVictimSpine;
		if( fVerticalDelta < fVehicleJackingMinVerticalDistanceBetweenPlayerAndVictim || 
			fVerticalDelta > fVehicleJackingMaxVerticalDistanceBetweenPlayerAndVictim )
		{
			if( (!bIsRightSideEntry && (fDotSeatRightToVictimSpine - fDistanceToStopLooking) < fDotSeatRightToPlayerHead) ||
				( bIsRightSideEntry && (fDotSeatRightToVictimSpine + fDistanceToStopLooking) > fDotSeatRightToPlayerHead) ||
				bStopLookingAtVictim )
			{
				// If distance from victim spine to seat (along seat right vector) is bigger than distance from player
				// head to seat, (i.e. if victim is further from the seat than the player) then stop looking at him.

				// If we are going to stop looking at victim, reset the seat blend distance so we blend from that point to the get in smoothly.
				////m_VehicleSeatBlendDistance = Max(fDistanceToSeat*0.50f, 0.40f);
				m_CarJackVictimCrossedThreshold = true;
				return false;
			}
		}
		else
		{
			bJackingFromAbove = true;
		}

		Vector3 vBaseTargetPos = mtxSpine.d;
		if(targetBoneTag != BONETAG_SPINE3)
		{
			int targetBoneIndex = m_pJackedPed->GetBoneIndexFromBoneTag(targetBoneTag);
			if(targetBoneIndex >= 0)
			{
				Matrix34 mtxTargetBone;
				m_pJackedPed->GetGlobalMtx(targetBoneIndex, mtxTargetBone);
				vBaseTargetPos = mtxTargetBone.d;
				vBaseTargetPos.z += c_CarJackTargetZOffset;
			}
		}

		float fCarJackBlendLevel = 0.0f;
		if(m_CarJackStartTime > 0)
		{
			fCarJackBlendLevel = (float)(fwTimer::GetTimeInMilliseconds() - m_CarJackStartTime) / (float)c_CarJackBlendOutLimitDuration;
			fCarJackBlendLevel = Clamp(fCarJackBlendLevel, 0.0f, 1.0f);
		}

		vTarget = vBaseTargetPos;
		 if(!m_pVehicle->InheritsFromBoat() || m_IsABikeOrJetski)
		 {
			const float fForwardOffset = (!bJackingFromAbove) ? 0.10f : 0.40f;
			vTarget += (seatMtx.b * fCarJackBlendLevel * fForwardOffset);
		 }
		lookDownLimit = (!m_IsABikeOrJetski) ? fVehicleJackingPitchLimit * DtoR : fVehicleJackingPitchForBikesLimit * DtoR;

		Vector3 vDelta = vTarget - seatMtx.d;
		const float fMinJackedDistanceFromSeat = fVehicleJackingRightSeatDistance;

		bool bAppliedTargetOffset = false;
		float fDotDelta = vDelta.Dot( seatMtx.b );
		if(!bJackingFromAbove && vDelta.Mag2() < rage::square(fMinJackedDistanceFromSeat))
		{
			float fOffset = fMinJackedDistanceFromSeat - vDelta.Mag();
			fOffset *= (bIsRightSideEntry) ? -1.0f : 1.0f;
			vTarget += seatMtx.a * fCarJackBlendLevel * fOffset;
			bAppliedTargetOffset = true;
		}
		else if(m_pVehicle && m_pVehicle->InheritsFromPlane())
		{
			vTarget += (vSpineForward * fCarJackBlendLevel * fPlaneJackingForwardLookOffset);
			bAppliedTargetOffset = true;
		}

		float fHeadingSpringConstant = m_Metadata.m_VehicleCarJackHeadingSpring.m_SpringConstant;
		if(!bAppliedTargetOffset)
		{
			Vector3 vecOffsetDirection = vSpineForward;
			float forwardOffset = fVehicleJackingForwardOffset;
		#if RSG_PC
			if(m_TripleHead)
			{
				vecOffsetDirection = vDeltaRight;
				vecOffsetDirection.NormalizeSafe();
				forwardOffset *= 1.5f;
			}
		#endif
			vDelta = vTarget - playerSpinePosition;
			fDotDelta = vDelta.Dot( vecOffsetDirection );
			if(fDotDelta < forwardOffset)
			{
				vTarget += vecOffsetDirection * (forwardOffset - fDotDelta);
			}
		#if RSG_PC
			if(m_TripleHead && m_pVehicle->GetIsRotaryAircraft())
			{
				vTarget += (seatMtx.b * fTripleHeadHeliJackingForwardVicOffset);
				fHeadingSpringConstant = Max(fHeadingSpringConstant, 1000.0f);
			}
		#endif
		}

		{
			// Calculate heading and pitch to look at target.
			float fTargetHeading = m_AttachParentHeading;
			float fTargetPitch   = m_AttachParentPitch;
			Vector3 vDeltaRoot = vTarget - vRootPosition;
			Vector3 vDeltaCamera = vTarget - m_Frame.GetPosition();
			if( vDeltaRoot.Mag2() > rage::square(SMALL_FLOAT) )
			{
				// We use root instead of camera, in case camera gets too close to victim or looks straight down.
				vDeltaRoot.Normalize();
				fTargetHeading = rage::Atan2f(-vDeltaRoot.x, vDeltaRoot.y);
				if(vDeltaCamera.Mag2() > rage::square(SMALL_FLOAT))
				{
					vDeltaCamera.Normalize();
					fTargetPitch = AsinfSafe(vDeltaCamera.z);
				}
				else
				{
					fTargetPitch = AsinfSafe(vDeltaRoot.z);
				}
			}

			////heading = fwAngle::LerpTowards(heading, fTargetHeading, fVehicleJackingHeadingInterp * 30.0f * fwTimer::GetTimeStep());
			////pitch = Lerp(fVehicleJackedPitchInterp * 30.0f * fwTimer::GetTimeStep(), pitch, fTargetPitch);
			heading = m_VehicleCarJackHeadingSpring.UpdateAngular(fTargetHeading,
							fHeadingSpringConstant, 
							m_Metadata.m_VehicleCarJackHeadingSpring.m_SpringDampingRatio);
			pitch   = m_VehicleCarJackPitchSpring.UpdateAngular(fTargetPitch,
							m_Metadata.m_VehicleCarJackPitchSpring.m_SpringConstant, 
							m_Metadata.m_VehicleCarJackPitchSpring.m_SpringDampingRatio);
			m_VehicleHeadingSpring.Reset(heading);
			m_VehiclePitchSpring.Reset(pitch);
		}

		return true;
	}

	return false;
}

bool camFirstPersonShooterCamera::CustomLookClimb( const CPed* attachPed, float& heading, float& pitch,
												   bool& bOrientationRelativeToAttachedPed, const CustomSituationsParams& oParam,
												   bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation )
{
	bool bReturn = false;
	if( bOrientationRelativeToAttachedPed )
	{
		// Convert to non-relative heading.
		heading = fwAngle::LimitRadianAngle(heading + oParam.previousAttachParentHeading);
		pitch   = fwAngle::LimitRadianAngle(pitch   + oParam.previousAttachParentPitch);
	}

	Matrix34 matRootBone = MAT34V_TO_MATRIX34(attachPed->GetMatrix());
	float fRootHeading = camFrame::ComputeHeadingFromMatrix(matRootBone);

	if(oParam.bMountingLadderTop)
	{
		// When mounting ladder at top, do not override pitch, just the heading.
		const float fMinPitch = (m_Metadata.m_MinPitch * 0.50f);
		Matrix34 mtxBone;
		float fBoneHeading, fBonePitch;
		if( GetBoneHeadingAndPitch(attachPed, (u32)BONETAG_SPINE3, mtxBone, fBoneHeading, fBonePitch, 0.10f) )
		{
			// Hardcoding blend rate as we have animated cameras for these so getting to the bone heading quickly is no longer needed.
			const float fBlendRate = Min(0.05f * 30.0f * fwTimer::GetCamTimeStep(), 1.0f);
			heading = fwAngle::LerpTowards(heading, fBoneHeading, fBlendRate);
			pitch = Lerp(fBlendRate, pitch, fBonePitch);
			pitch = Max(pitch, fMinPitch * DtoR);
		}
	}
	else
	{
		if(oParam.bClimbingLadder || oParam.bMountingLadderBottom)
		{
			const float c_DefaultLadderPitch = m_MovingUpLadder ? m_Metadata.m_LadderPitch : -m_Metadata.m_LadderPitch;
			float fFinalPitch = c_DefaultLadderPitch; //(!oParam.bMountingLadderBottom) ? c_DefaultLadderPitch : (m_Metadata.m_MaxPitch * 0.70f);
			pitch = Lerp(m_Metadata.m_ClimbPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), pitch, fFinalPitch * DtoR);	// TODO: pitch down when climbing down?

			bAllowLookAroundDuringCustomSituation = oParam.bClimbingLadder;
			bAllowLookDownDuringCustomSituation = oParam.bClimbingLadder && m_MovingUpLadder;

			if(oParam.bClimbingLadder)
			{
				const float c_ZOffsetForWaterTest		= 0.15f;		// was 0.0f
				const float c_MaxDistanceForWaterTest	= 0.50f;		// was 0.30f
				TUNE_GROUP_FLOAT(CAM_FPS, c_LadderOffsetForWater, 0.10f, 0.0f, 1.0f, 0.001f);
				float localWaterHeight;
				Vector3 positionToTest = m_Frame.GetPosition();
				Vector3 cameraPosition = positionToTest;
				positionToTest.z += c_ZOffsetForWaterTest;
				bool isLocalWaterHeightValid			= camCollision::ComputeWaterHeightAtPosition(positionToTest, c_MaxDistanceForWaterTest, localWaterHeight, c_MaxDistanceForWaterTest);
				if( isLocalWaterHeightValid )
				{
					float fWaterHeightDifference = cameraPosition.z - localWaterHeight;
					if( Abs(fWaterHeightDifference) < c_MaxDistanceForWaterTest )
					{
						if(m_MovingUpLadder && fWaterHeightDifference < 0.0f)
						{
							cameraPosition.z = Min(cameraPosition.z, (localWaterHeight - c_LadderOffsetForWater));
							m_Frame.SetPosition(cameraPosition);
						}
						else if(!m_MovingUpLadder && fWaterHeightDifference > 0.0f)
						{
							cameraPosition.z = Max(cameraPosition.z, (localWaterHeight + c_LadderOffsetForWater));
							m_Frame.SetPosition(cameraPosition);
						}
					}
				}
			}

			m_WasClimbingLadder = true;
			m_WasClimbing = false;
		}
		else
		{
			TUNE_GROUP_FLOAT(CAM_FPS, customJumpPitch, -8.0f, -90.0f, 90.0f, 1.0f);
			float fHeadBonePitch = customJumpPitch * DtoR;
			if( !oParam.bJumping )
			{
				Matrix34 matHeadBone;
				int boneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
				if(boneIndex >= 0)
				{
					attachPed->GetGlobalMtx(boneIndex, matHeadBone);
					fHeadBonePitch = camFrame::ComputePitchFromMatrix(matHeadBone);
				}
				bAllowLookAroundDuringCustomSituation = true;
			}

			// All other climbing.
			pitch = Lerp(m_Metadata.m_VaultPitchBlend * 30.0f * fwTimer::GetCamTimeStep(), pitch, fHeadBonePitch);
			m_WasClimbing = true;
		}

		heading = fwAngle::LerpTowards(heading, fRootHeading, (oParam.bJumping ? m_Metadata.m_JumpHeadingBlend : m_Metadata.m_VaultClimbHeadingBlend) * 30.0f * fwTimer::GetCamTimeStep());
	}
	bReturn = true;

	// When overriding orientation, it is no longer relative to ped's orientation.
	bOrientationRelativeToAttachedPed = false;

	return (bReturn);
}

void camFirstPersonShooterCamera::CloneCinematicCamera()
{
	if( !m_bUseAnimatedFirstPersonCamera )
	{
		const camBaseCamera* pCinematicRenderedCamera = camInterface::GetCinematicDirector().GetRenderedCamera();
		if(m_ExitingVehicle && camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera())
		{
			if( pCinematicRenderedCamera && pCinematicRenderedCamera->GetIsClassId( camCinematicMountedCamera::GetStaticClassId() ) )
			{
				// Because the cinematic camera has not been updated yet, GetFrame gives the frame before shake is applied.
				// Use the cached from the camInterface to get the final camera frame.
				const camFrame& frame = (camInterface::GetDominantRenderedCamera() == pCinematicRenderedCamera) ? camInterface::GetFrame() : pCinematicRenderedCamera->GetFrame();

				// Clone the frame, but not the DofFullScreenBlur level as the frame shakers won't update this for the new first person shooter camera and
				// we are picking this up when we are grabbing the post effect update frame.
				////float fSavedDofFullScreenBlurBlendLevel = m_Frame.GetDofFullScreenBlurBlendLevel();
				////m_Frame.CloneFrom( frame );
				////m_Frame.SetDofFullScreenBlurBlendLevel( fSavedDofFullScreenBlurBlendLevel );				// Restore

				const camCinematicMountedCamera* pMountedCamera = static_cast<const camCinematicMountedCamera*>(pCinematicRenderedCamera);
				if(pMountedCamera->IsFirstPersonCamera())
				{
					// On second thought, just copy what we need.
					m_Frame.SetPosition(	frame.GetPosition());
					m_Frame.SetWorldMatrix(	frame.GetWorldMatrix());
					m_Frame.SetFov(			frame.GetFov());
					m_Frame.SetNearClip(	frame.GetNearClip());
					m_Frame.SetFarClip(		frame.GetFarClip());

					m_Frame.ComputeHeadingPitchAndRoll(m_ExitVehicleHeading, m_ExitVehiclePitch, m_ExitVehicleRoll);
					m_PovCameraPosition = m_Frame.GetPosition();
					m_PovCameraNearClip = m_Frame.GetNearClip();
					const CPed* attachedPed = camInterface::FindFollowPed();
					if(attachedPed)
					{
						Mat34V pedMatrix = attachedPed->GetMatrix();
						m_PreviousRelativeCameraPosition = UnTransformFull( pedMatrix, RCC_VEC3V(m_PovCameraPosition) );
					}
					m_PovClonedDataValid = true;
				}
			}
		}
		else if( pCinematicRenderedCamera && pCinematicRenderedCamera->GetIsClassId( camCinematicFirstPersonIdleCamera::GetStaticClassId() ) )
		{
			// Because the cinematic camera has not been updated yet, GetFrame gives the frame before shake is applied.
			// Use the cached from the camInterface to get the final camera frame.
			const camFrame& frame = pCinematicRenderedCamera->GetFrame();
		
			// Calculate the heading and pitch from the camera frame and use that to set the relative heading and pitch.
			// Set heading, so first person camera can blend it when we come back.
			frame.ComputeHeadingAndPitch(m_IdleHeading, m_IdlePitch);
			m_Frame.SetFov( frame.GetFov() );

			// m_IsIdleCameraActive is cleared at end of Update(), so this is really isActive or wasActiveLastUpdate.
			//  - means we will stay on idle heading/pitch for 1 extra frame.
			m_IsIdleCameraActive = true;
		}
	}
}

bool camFirstPersonShooterCamera::IsStickWithinStrafeAngleThreshold() const
{
	const CPed* followPed = camInterface::FindFollowPed();

	// Check is same as in locomotion to start moving
	if(followPed && followPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio().Mag2() > square(MBR_WALK_BOUNDARY))
	{
		CControl& rControl = CControlMgr::GetMainPlayerControl(true);
		Vector2 vLeftStick(rControl.GetPedWalkLeftRight().GetNorm(), -rControl.GetPedWalkUpDown().GetNorm());
		float fStickDirection = Atan2f(-vLeftStick.x, vLeftStick.y);
		if( Abs(fStickDirection) < m_Metadata.m_AngleStrafeThreshold * DtoR )
		{
			return false;
		}
	}

	return true;
}

void camFirstPersonShooterCamera::OnBulletImpact(const CPed* UNUSED_PARAM(pAttacker), const CPed* pVictim)
{
	if( pVictim && pVictim->IsLocalPlayer() )
	{
		// Setup shake for ped hitting player.
		camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->Shake( m_Metadata.m_BulletImpactShakeRef );
	}
}

void camFirstPersonShooterCamera::OnMeleeImpact(const CPed* pAttacker, const CPed* pVictim)
{
	if( pAttacker && pVictim && pAttacker->IsLocalPlayer() )
	{
		if(pVictim->GetIsDeadOrDying())
		{
			// Setup timer for player attacking dead ped.
			const u32 c_uCurrentTimeMSec = fwTimer::GetTimeInMilliseconds();
			if (m_StopTimeOfProneMeleeAttackLook < c_uCurrentTimeMSec)
			{
				if (pVictim->GetDeathState() == DeathState_Dead)
					m_StopTimeOfProneMeleeAttackLook = c_uCurrentTimeMSec + m_Metadata.m_LookAtDeadMeleeTargetDelay;
				else
					m_StopTimeOfProneMeleeAttackLook = c_uCurrentTimeMSec + m_Metadata.m_LookAtProneMeleeTargetDelay;
			}
			else
			{
				m_StopTimeOfProneMeleeAttackLook = 0;
			}
		}

		// Setup shake for player hitting ped.
		camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->Shake( m_Metadata.m_MeleeImpactShakeRef );
	}
	else if( pAttacker && pVictim && pVictim->IsLocalPlayer() && !pAttacker->GetIsDeadOrDying() )
	{
		// Setup shake for ped hitting player.
		camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->Shake( m_Metadata.m_MeleeImpactShakeRef );
	}
}

void camFirstPersonShooterCamera::OnVehicleImpact(const CPed* pAttacker, const CPed* pVictim)
{
	if( pVictim && pVictim->IsLocalPlayer() && pVictim != pAttacker )
	{
		// Setup shake for ped hitting player.
		camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->Shake( m_Metadata.m_VehicleImpactShakeRef );
	}
}

void camFirstPersonShooterCamera::OnDeath(const CPed* pVictim)
{
	if( pVictim == m_AttachParent.Get() )
	{
		m_DeathShakeHash = m_Metadata.m_DeathShakeRef;
	}
}

void camFirstPersonShooterCamera::UpdateStickyAim()
{
	const CPed* desiredStickyTarget	= NULL;
	Vector3 desiredStickyAimPosition	= g_InvalidPosition;
	Vector3 fineAimOffsetOnPreviousUpdate(m_LockOnFineAimOffset);
	m_HasStickyTarget = ComputeStickyTarget(desiredStickyTarget, desiredStickyAimPosition);

	UpdateStickyAimEnvelopes();

	if(m_HasStickyTarget)
	{
		if (m_AttachParent && m_AttachParent->GetIsTypePed())
		{
			const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());

			camFirstPersonShooterCameraMetadataStickyAim stickySettings = m_IsScopeAiming ? m_Metadata.m_StickyAimScope : m_Metadata.m_StickyAimDefault;

			Vector3 vLocalDesiredStickyAimPosition=desiredStickyAimPosition;
			//! convert to local space position.
			MAT34V_TO_MATRIX34(desiredStickyTarget->GetTransform().GetMatrix()).UnTransform(vLocalDesiredStickyAimPosition);

			if(!m_StickyAimTarget || (m_StickyAimTarget != desiredStickyTarget) )
			{
				m_StickyAimTargetPosition = vLocalDesiredStickyAimPosition;
			}

			m_StickyAimTarget = desiredStickyTarget;

			float fVelScale = 0.0f;

			const CControl *pControl = attachPed->GetControlFromPlayer();
			if(pControl)
			{
				//! Left Stick scale.
				float fLeftStickControlScale = Clamp( 1.0f - abs(pControl->GetPedWalkLeftRight().GetNorm()), 0.0f, 1.0f);
				float fStickyAimLeftStickModifier = Lerp(fLeftStickControlScale, stickySettings.m_LeftStickAtMinStength, stickySettings.m_LeftStickAtMinStength);

				//! Distance scale.
				Vector3 vPedPosition = VEC3V_TO_VECTOR3(attachPed->GetTransform().GetPosition());
				Vector3 vStickyTargetPosition = desiredStickyAimPosition;
				Vector3 vDirection = vStickyTargetPosition - vPedPosition;
				float fDistance = vDirection.Mag();
				float fDistanceScale = Min(fDistance/m_Metadata.m_MaxStickyAimDistance, 1.0f);

				float fStickyAimDistanceModifier = Lerp(fDistanceScale, stickySettings.m_MinDistanceRateModifier, stickySettings.m_MaxDistanceRateModifier);
			
				//! Velocity scale.
				Vector3 vTargetVelocity = m_StickyAimTarget->GetVelocity();
				m_Frame.GetWorldMatrix().UnTransform3x3(vTargetVelocity);
				fVelScale = Clamp( abs(vTargetVelocity.x) / stickySettings.m_MaxRelativeVelocity, 0.0f, 1.0f);

				float fStickyTargetVelocityModifier = Lerp(fVelScale, stickySettings.m_MinVelRateModifier, stickySettings.m_MaxVelRateModifier);
			
				//! Scale based on input on right stick.
				float fRightStickControlScale = Clamp( abs(pControl->GetPedAimWeaponLeftRight().GetNorm()), 0.0f, 1.0f);
				float fStickyAimCameraModifier = 1.0f;
				if(fRightStickControlScale > stickySettings.m_RightStickAtMinStength)
				{
					fRightStickControlScale = Clamp(Lerp(fRightStickControlScale, stickySettings.m_RightStickAtMinStength, stickySettings.m_RightStickAtMaxStength), 0.0f, 1.0f);
					fStickyAimCameraModifier = Lerp(fRightStickControlScale, stickySettings.m_MinRightStickRateModifier, stickySettings.m_MaxRightStickRateModifier);
				}
							
				m_StickyAimHeadingRate = (stickySettings.m_HeadingRateAtMinLeftStickStrength + ((stickySettings.m_HeadingRateAtMaxLeftStickStrength - stickySettings.m_HeadingRateAtMinLeftStickStrength) * fStickyAimLeftStickModifier));
				m_StickyAimHeadingRate *= fStickyAimDistanceModifier;
				m_StickyAimHeadingRate *= fStickyTargetVelocityModifier;
				m_StickyAimHeadingRate *= fStickyAimCameraModifier;
			}

			float fTargetLerp = Lerp(fVelScale, stickySettings.m_TargetPositionLerpMinVelocity, stickySettings.m_TargetPositionLerpMaxVelocity);
			m_StickyAimTargetPosition.Lerp(fTargetLerp, m_StickyAimTargetPosition, vLocalDesiredStickyAimPosition );
		}
	}
	else
	{
		m_StickyAimTarget			= NULL;
		m_StickyAimTargetPosition	= g_InvalidPosition;
	}
}

void camFirstPersonShooterCamera::ModifyLookAroundOffsetForStickyAim(float heading, float pitch, float &lookAroundHeadingOffset, float &lookAroundPitchOffset)
{
	TUNE_GROUP_BOOL(STICKY_AIM, bDoMagnetismAlways, false);

	bool bDoMagnetism = false;
	Vector3 vLockOnPos;
	if(bDoMagnetismAlways && m_PotentialStickyAimTarget)
	{
		m_PotentialStickyAimTarget->GetLockOnTargetAimAtPos(vLockOnPos);
		bDoMagnetism = true;
	}
	else if(m_StickyAimTarget)
	{
		m_StickyAimTarget->GetLockOnTargetAimAtPos(vLockOnPos);
		bDoMagnetism = true;
	}

	if( bDoMagnetism )
	{
		Vector3 desiredDelta	= vLockOnPos - m_Frame.GetPosition();
		desiredDelta.NormalizeSafe();
		float desiredStickyHeading;
		float desiredStickyPitch;
		camFrame::ComputeHeadingAndPitchFromFront(desiredDelta, desiredStickyHeading, desiredStickyPitch);

		camFirstPersonShooterCameraMetadataStickyAim stickySettings = m_IsScopeAiming ? m_Metadata.m_StickyAimScope : m_Metadata.m_StickyAimDefault;

		if(abs(lookAroundHeadingOffset) > SMALL_FLOAT)
		{
			//! work out input to get to desired.
			float fDesiredInput = fwAngle::LimitRadianAngle(desiredStickyHeading - heading);

			float fLerpScale = Clamp((m_fTimePushingLookAroundHeading - stickySettings.m_HeadingMagnetismTimeForMinLerp) / (stickySettings.m_HeadingMagnetismTimeForMaxLerp - stickySettings.m_HeadingMagnetismTimeForMinLerp), 0.0f, 1.0f);
			float fLerp = stickySettings.m_HeadingMagnetismLerpMin + ((stickySettings.m_HeadingMagnetismLerpMax-stickySettings.m_HeadingMagnetismLerpMin)*fLerpScale);

			if(fLerp > 0.0f)
			{
				float headingOffset = lookAroundHeadingOffset;
				headingOffset = Lerp(fLerp, headingOffset, fDesiredInput);
				lookAroundHeadingOffset = headingOffset;
			}
		}

		if(abs(lookAroundPitchOffset) > SMALL_FLOAT)
		{
			//! work out input to get to desired.
			float fDesiredInput = fwAngle::LimitRadianAngle(desiredStickyPitch - pitch);

			camFirstPersonShooterCameraMetadataStickyAim stickySettings = m_IsScopeAiming ? m_Metadata.m_StickyAimScope : m_Metadata.m_StickyAimDefault;

			float fLerpScale = Clamp((m_fTimePushingLookAroundHeading - stickySettings.m_PitchMagnetismTimeForMinLerp) / (stickySettings.m_PitchMagnetismTimeForMaxLerp - stickySettings.m_PitchMagnetismTimeForMinLerp), 0.0f, 1.0f);
			float fLerp = stickySettings.m_PitchMagnetismLerpMin + ((stickySettings.m_PitchMagnetismLerpMax-stickySettings.m_PitchMagnetismLerpMin)*fLerpScale);

			if(fLerp > 0.0f)
			{
				float pitchOffset = lookAroundPitchOffset;
				pitchOffset = Lerp(fLerp, pitchOffset, fDesiredInput);
				lookAroundPitchOffset = pitchOffset;
			}
		}
	}

	if(abs(lookAroundHeadingOffset) > SMALL_FLOAT)
	{
		m_fTimePushingLookAroundHeading += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimePushingLookAroundHeading = 0.0f;
	}

	if(abs(lookAroundPitchOffset) > SMALL_FLOAT)
	{
		m_fTimePushingLookAroundPitch += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimePushingLookAroundPitch = 0.0f;
	}
}

void camFirstPersonShooterCamera::UpdateLockOn()
{
	const CEntity* desiredLockOnTarget	= NULL;
	Vector3 desiredLockOnTargetPosition	= g_InvalidPosition;
	Vector3 fineAimOffsetOnPreviousUpdate(m_LockOnFineAimOffset);

	m_IsLockedOn = ComputeLockOnTarget(desiredLockOnTarget, desiredLockOnTargetPosition, m_LockOnFineAimOffset);

	UpdateLockOnEnvelopes();

	if(m_IsLockedOn)
	{
		// Do not allow melee target to change if we are running an animated camera.
		// TODO: ok to switch targets during blendout???  Limit to only melee animated cameras?
		      bool hasChangedLockOnTargetThisUpdate = (desiredLockOnTarget != m_LockOnTarget);
		const bool runningAnimatedCamera = (m_bUseAnimatedFirstPersonCamera || m_bWasUsingAnimatedFirstPersonCamera);
		////const bool currentMeleeAttack    = (m_MeleeAttack || m_WasDoingMelee);
		if( hasChangedLockOnTargetThisUpdate && (!m_LockOnTarget.Get() || /*!currentMeleeAttack ||*/ !runningAnimatedCamera) )
		{
			if(desiredLockOnTarget && (m_NumUpdatesPerformed > 0))
			{
				if(!m_LockOnTarget || m_LockOnTargetPosition.IsClose(g_InvalidPosition, SMALL_FLOAT))
				{
					const float previousPivotToDesiredLockOnTargetDistance	= m_Frame.GetPosition().Dist(desiredLockOnTargetPosition);

					m_LockOnTargetPosition = m_Frame.GetPosition() + (previousPivotToDesiredLockOnTargetDistance * m_Frame.GetFront());
					m_LockOnTargetPositionLocal = desiredLockOnTarget ? VEC3V_TO_VECTOR3(desiredLockOnTarget->GetTransform().UnTransform(VECTOR3_TO_VEC3V(m_LockOnTargetPosition))) : m_LockOnTargetPosition;
				}

				m_LockOnTargetBlendStartTime				= fwTimer::GetCamTimeInMilliseconds();
				m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
				m_ShouldBlendLockOnTargetPosition			= true;

				//Determine a suitable blend duration base upon the angle between the current and desired lock-on target positions.

				Vector3 desiredLockOnTargetFront = desiredLockOnTargetPosition - m_Frame.GetPosition();
				desiredLockOnTargetFront.Normalize();

				Vector3 lockOnPosition = desiredLockOnTarget ? VEC3V_TO_VECTOR3(desiredLockOnTarget->GetTransform().Transform(VECTOR3_TO_VEC3V(m_LockOnTargetPositionLocal))) : m_LockOnTargetPosition;

				Vector3 currentLockOnTargetFront = lockOnPosition - m_Frame.GetPosition();
				currentLockOnTargetFront.NormalizeSafe(desiredLockOnTargetFront);

				const u32 minBlendDuration	= (m_LockOnTarget.Get() != NULL) ? m_Metadata.m_MinBlendDurationForLockOnSwitch :
					m_Metadata.m_MinBlendDurationForInitialLockOn;
				const u32 maxBlendDuration	= (m_LockOnTarget.Get() != NULL) ? m_Metadata.m_MaxBlendDurationForLockOnSwitch :
					m_Metadata.m_MaxBlendDurationForInitialLockOn;

				const float angleDelta		= currentLockOnTargetFront.FastAngle(desiredLockOnTargetFront);
				const float durationTValue	= (m_Metadata.m_MaxAngleDeltaForLockOnSwitchBlendScaling < SMALL_FLOAT) ? 1.0f :
					Min(angleDelta / (m_Metadata.m_MaxAngleDeltaForLockOnSwitchBlendScaling * DtoR), 1.0f);
				m_LockOnTargetBlendDuration	= Lerp(durationTValue, minBlendDuration, maxBlendDuration);
			}

			m_LockOnTarget = desiredLockOnTarget;
		}

		if(m_ShouldBlendLockOnTargetPosition)
		{
			UpdateLockOnTargetBlend(desiredLockOnTargetPosition);
		}
		else
		{
			m_LockOnTargetPosition = desiredLockOnTargetPosition;
		}

		m_LockOnTargetPositionLocal = desiredLockOnTarget ? VEC3V_TO_VECTOR3(desiredLockOnTarget->GetTransform().UnTransform(VECTOR3_TO_VEC3V(m_LockOnTargetPosition))) : m_LockOnTargetPosition;

		if(!ComputeIsProspectiveLockOnTargetPositionValid(m_LockOnTargetPosition))
		{
			const CPed* followPed = camInterface::FindFollowPed();
			if(followPed)
			{
				const CPlayerInfo* followPedPlayerInfo = followPed->GetPlayerInfo();
				if(followPedPlayerInfo)
				{
					if(followPed->GetWeaponManager() && followPed->GetWeaponManager()->GetEquippedWeaponInfo())
					{
						const CWeaponInfo* pWeaponInfo = followPed->GetWeaponManager()->GetEquippedWeaponInfo();
						if(pWeaponInfo->GetIsMelee())
						{
							//! break lock on.
							m_IsLockedOn = false;
							CPlayerPedTargeting &targeting			= const_cast<CPlayerPedTargeting &>(followPedPlayerInfo->GetTargeting());
							targeting.ClearLockOnTarget();

							m_LockOnTarget								= NULL;
							m_LockOnTargetPosition						= g_InvalidPosition;
							m_LockOnTargetPositionLocal					= g_InvalidPosition;
							m_LockOnFineAimOffset.Zero();
							m_LockOnTargetBlendStartTime				= 0;
							m_LockOnTargetBlendDuration					= 0;
							m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
							m_ShouldBlendLockOnTargetPosition			= false;
						}
					}
				}
			}
		}
	}
	else
	{
		m_LockOnTarget								= NULL;
		m_LockOnTargetPosition						= g_InvalidPosition;
		m_LockOnTargetPositionLocal					= g_InvalidPosition;
		m_LockOnFineAimOffset.Zero();
		m_LockOnTargetBlendStartTime				= 0;
		m_LockOnTargetBlendDuration					= 0;
		m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
		m_ShouldBlendLockOnTargetPosition			= false;
	}

	if (m_StickyAimHelper)
	{
		bool isAimingAndCanUseStickyAim = m_IsAiming && !m_bLookingBehind;
		m_StickyAimHelper->Update(m_Frame.GetPosition(), m_Frame.GetFront(), m_Frame.GetWorldMatrix(), m_IsLockedOn, isAimingAndCanUseStickyAim);
	}
}

bool camFirstPersonShooterCamera::ComputeStickyTarget(const CPed*& stickyTarget, Vector3& stickyPosition)
{
	bool useStickyAim = false;

	const CPed* followPed = camInterface::FindFollowPed();
	if(followPed)
	{
		const CPlayerInfo* followPedPlayerInfo = followPed->GetPlayerInfo();
		if(followPedPlayerInfo)
		{
			CPlayerPedTargeting &targeting			= const_cast<CPlayerPedTargeting &>(followPedPlayerInfo->GetTargeting());
			const CEntity* prospectiveStickyTarget	= targeting.GetFreeAimAssistTarget();
			Vector3 prospectiveStickyPosition;
			if(prospectiveStickyTarget)
			{
				//! Note: Free aim target assist position will give us ped intersection position, when ideally we want an intersection 
				//! with some kind of bounding sphere/box.
				prospectiveStickyPosition = targeting.GetClosestFreeAimAssistTargetPos();
				Vector3 vToCam = prospectiveStickyPosition - m_Frame.GetPosition();
				float fDistance = vToCam.Mag();
				prospectiveStickyPosition =  (m_Frame.GetPosition() + (m_Frame.GetFront() * fDistance));
			}
			else
			{
				prospectiveStickyTarget = targeting.GetFreeAimTarget();
				prospectiveStickyPosition = targeting.GetClosestFreeAimTargetPos();
			}

			if(prospectiveStickyTarget && prospectiveStickyTarget->GetIsTypePed())
			{
				const CPed *pTargetPed = static_cast<const CPed*>(prospectiveStickyTarget);

				if( (prospectiveStickyTarget != m_PotentialStickyAimTarget) && (prospectiveStickyTarget != m_StickyAimTarget) )
				{
					m_TimeSetStickyAimTarget = fwTimer::GetTimeInMilliseconds(); //! reset timer if we switched potential targets.
				}

				m_PotentialStickyAimTarget = pTargetPed;

				//! Don't allow immediate target switching.
				bool bSetTarget = true;
				const bool bTargetChanged = m_StickyAimTarget && m_PotentialStickyAimTarget != m_StickyAimTarget;
				if(bTargetChanged)
				{
					if(fwTimer::GetTimeInMilliseconds() > (m_TimeSetStickyAimTarget + m_Metadata.m_TimeToSwitchStickyTargets) )
					{
						bSetTarget = false;
					}
				}

				const bool isPositionValid = ComputeIsProspectiveStickyTargetAndPositionValid(followPed, pTargetPed, prospectiveStickyPosition);
				if(isPositionValid)
				{
					const bool shouldUseStickyAiming = ComputeUseStickyAiming();
					if(shouldUseStickyAiming)
					{
						stickyTarget	= pTargetPed;
						stickyPosition	= prospectiveStickyPosition;
						useStickyAim	= true;
					}
				}
				else
				{
					m_PotentialStickyAimTarget = NULL;
				}
			}
			else
			{
				m_PotentialStickyAimTarget = NULL;
			}
		}
	}

	return useStickyAim;
}


bool camFirstPersonShooterCamera::ComputeLockOnTarget(const CEntity*& lockOnTarget, Vector3& lockOnTargetPosition, Vector3& fineAimOffset) const
{
	bool isLockedOn = false;

	fineAimOffset.Zero();

	const bool shouldUseLockOnAiming = ComputeShouldUseLockOnAiming();
	if(shouldUseLockOnAiming)
	{
		const CPed* followPed = camInterface::FindFollowPed();
		if(followPed)
		{
			const CPlayerInfo* followPedPlayerInfo = followPed->GetPlayerInfo();
			if(followPedPlayerInfo)
			{
				CPlayerPedTargeting &targeting			= const_cast<CPlayerPedTargeting &>(followPedPlayerInfo->GetTargeting());
				const CEntity* prospectiveLockOnTarget	= targeting.GetLockOnTarget();

				const CWeaponInfo *weaponInfo = followPed->GetWeaponManager() ? followPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
				bool bUsingOnFootHomingWeapon = weaponInfo && weaponInfo->GetIsOnFootHoming();

				if((prospectiveLockOnTarget != NULL) && !bUsingOnFootHomingWeapon)
				{
					Vector3 prospectiveLockOnTargetPosition;
					prospectiveLockOnTarget->GetLockOnTargetAimAtPos(prospectiveLockOnTargetPosition);

					const bool isPositionValid = ComputeIsProspectiveLockOnTargetPositionValid(prospectiveLockOnTargetPosition);
					if(isPositionValid)
					{
						Vector3 tempFineAimOffset;
						if(targeting.GetFineAimingOffset(tempFineAimOffset))
						{
							//Scale down the fine-aim offset at close range to prevent instability.
							Vector3 vCamPosition = m_Frame.GetPosition();
							const float lockOnDistance		= vCamPosition.Dist(prospectiveLockOnTargetPosition);
							const float fineAimScaling		= RampValueSafe(lockOnDistance, m_Metadata.m_MinDistanceForFineAimScaling,
								m_Metadata.m_MaxDistanceForFineAimScaling, 0.0f, 1.0f);
							fineAimOffset					= tempFineAimOffset * fineAimScaling;

							// Script-requested ability to invert look controls as a MP pickup power
							if (followPed->GetPedResetFlag(CPED_RESET_FLAG_InvertLookAroundControls))
							{
								fineAimOffset *= -1.0f;
							}

							prospectiveLockOnTargetPosition	+= fineAimOffset;
						}

						lockOnTarget			= prospectiveLockOnTarget;
						lockOnTargetPosition	= prospectiveLockOnTargetPosition;
						isLockedOn				= true;
					}
					else
					{
						//We cannot safely handle this setup, so break lock-on.
						targeting.ClearLockOnTarget();
					}
				}
			}
		}
	}

	return isLockedOn;
}

bool camFirstPersonShooterCamera::ComputeIsProspectiveStickyTargetAndPositionValid(const CPed *pFollowPed, const CPed *pStickyPed, const Vector3& stickyPos) const
{
	if(pStickyPed->GetIsInVehicle())
	{
		return false;
	}

	//! Check normal targeting lockon behaviour. E.g. is dead, friendly etc to prevent sticky aim against these peds.
	u32 targetFlags = CPedTargetEvaluator::DEFAULT_TARGET_FLAGS | CPedTargetEvaluator::TS_ALLOW_LOCKON_EVEN_IF_DISABLED;
	if(!CPedTargetEvaluator::CheckBasicTargetExclusions(pFollowPed, pStickyPed, targetFlags, m_Metadata.m_MaxStickyAimDistance, m_Metadata.m_MaxStickyAimDistance))
	{
		return false;
	}

	camFirstPersonShooterCameraMetadataStickyAim stickySettings = m_IsScopeAiming ? m_Metadata.m_StickyAimScope : m_Metadata.m_StickyAimDefault;

	//! Check sticky pos is within bounds.
	Vector3 vEntityPos = VEC3V_TO_VECTOR3(pStickyPed->GetTransform().GetPosition());

	Vector3 vDiff = stickyPos - vEntityPos;

	float fXYMag = vDiff.XYMag();
	float fZNag = abs(vDiff.z);

	if( (fXYMag > stickySettings.m_StickyLimitsXY) || ( fZNag > stickySettings.m_StickyLimitsZ))
	{
		return false;
	}

	return true;
}

bool camFirstPersonShooterCamera::ComputeIsProspectiveLockOnTargetPositionValid(const Vector3& targetPosition) const
{
	if(!m_Metadata.m_ShouldValidateLockOnTargetPosition)
	{
		return true;
	}

	bool isValid = false;

	//Ensure that the lock-on distance and pitch are with safe limits.

	const Vector3 lockOnDelta	= targetPosition - m_Frame.GetPosition();
	const float lockOnDistance2	= lockOnDelta.Mag2();
	if(lockOnDistance2 >= (m_Metadata.m_MinDistanceForLockOn * m_Metadata.m_MinDistanceForLockOn))
	{
		Vector3 lockOnFront(lockOnDelta);
		lockOnFront.Normalize();

		const float lockOnPitch	= camFrame::ComputePitchFromFront(lockOnFront);
		isValid					= ((lockOnPitch >= m_MinPitch) && (lockOnPitch <= m_MaxPitch));
	}

	return isValid;
}

bool camFirstPersonShooterCamera::ComputeUseStickyAiming() const
{
	if(m_CombatRolling || (m_LockOnEnvelopeLevel > 0.0f) || m_bUseAnimatedFirstPersonCamera)
	{
		return false;
	}

	if(!CPlayerPedTargeting::ms_Tunables.GetUseFirstPersonStickyAim())
	{
		return false;
	}

	/*if(m_CurrentControlHelper)
	{
		float lookAroundHeadingOffset	 = m_CurrentControlHelper->GetLookAroundHeadingOffset();
		float lookAroundPitchOffset	 = m_CurrentControlHelper->GetLookAroundPitchOffset();

		if(abs(lookAroundHeadingOffset) > SMALL_FLOAT || abs(lookAroundPitchOffset) > SMALL_FLOAT)
		{
			return false;
		}
	}*/

	if (m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());

		if(attachPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisablePlayerLockon ) || attachPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerLockon ))
		{
			return false;
		}

		const CControl *pControl = attachPed->GetControlFromPlayer();
		if(pControl)
		{
			if( abs(pControl->GetPedWalkLeftRight().GetNorm()) < SMALL_FLOAT )
			{
				return false;
			}
		}

		if(attachPed->GetVehiclePedInside())
		{
			return false;
		}
	
		if(attachPed && attachPed->GetWeaponManager() && attachPed->GetWeaponManager()->GetEquippedWeaponInfo())
		{
			const CWeaponInfo* pWeaponInfo = attachPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(!pWeaponInfo->GetIsGun() || pWeaponInfo->GetIsMelee() || pWeaponInfo->GetIsProjectile())
			{
				// For unarmed or melee weapons, can always lock on.
				return false;
			}
		}
	}

	//! use same rules as lock on aiming (aim cam must be running, valid weapon etc).
	return ComputeShouldUseLockOnAiming() && m_Metadata.m_ShouldUseStickyAiming;
}

bool camFirstPersonShooterCamera::ComputeShouldUseLockOnAiming() const
{
	bool usingAimCamera = true;
	const CPed* followPed = static_cast<const CPed*>(m_AttachParent.Get());
	if(followPed != NULL)
	{
		if(followPed && followPed->GetWeaponManager() && followPed->GetWeaponManager()->GetEquippedWeaponInfo())
		{
			const CWeaponInfo* pWeaponInfo = followPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(pWeaponInfo->GetIsMelee())
			{
				// For unarmed or melee weapons, can always lock on.
				return true;
			}
		}

		CPedIntelligence* followPedIntelligence	= followPed->GetPedIntelligence();
		if(followPedIntelligence != NULL)
		{
			usingAimCamera = false;

			tGameplayCameraSettings tempSettings;
			followPedIntelligence->GetOverriddenGameplayCameraSettings(tempSettings);
			//Verify that we have a valid attach entity and that this camera exists before attempting to use it.
			if(tempSettings.m_CameraHash)
			{
				const camBaseCameraMetadata* cameraMetadata = camFactory::FindObjectMetadata<camBaseCameraMetadata>(tempSettings.m_CameraHash);
				if(cameraMetadata != NULL)
				{
					usingAimCamera = camFactory::IsObjectMetadataOfClass<camFirstPersonPedAimCameraMetadata>(*cameraMetadata) ||
						camFactory::IsObjectMetadataOfClass<camThirdPersonAimCameraMetadata>(*cameraMetadata);
				}
			}
		}
	}

	return usingAimCamera && m_Metadata.m_ShouldUseLockOnAiming;
}

void camFirstPersonShooterCamera::UpdateStickyAimEnvelopes()
{
	if(m_StickyAimEnvelope)
	{
		m_StickyAimEnvelope->AutoStartStop(m_HasStickyTarget);

		m_StickyAimEnvelopeLevel = m_StickyAimEnvelope->Update();
		m_StickyAimEnvelopeLevel = SlowInOut(m_StickyAimEnvelopeLevel);
	}
	else
	{
		m_StickyAimEnvelopeLevel = m_StickyAimEnvelope ? 1.0f : 0.0f;
	}
}

void camFirstPersonShooterCamera::UpdateLockOnEnvelopes()
{
	if(m_LockOnEnvelope)
	{
		m_LockOnEnvelope->AutoStartStop(m_IsLockedOn);

		m_LockOnEnvelopeLevel = m_LockOnEnvelope->Update();
		m_LockOnEnvelopeLevel = SlowInOut(m_LockOnEnvelopeLevel);
	}
	else
	{
		m_LockOnEnvelopeLevel = m_IsLockedOn ? 1.0f : 0.0f;
	}
}

void camFirstPersonShooterCamera::UpdateLockOnTargetBlend(const Vector3& desiredLockOnTargetPosition)
{
	const u32 currentTime			= fwTimer::GetCamTimeInMilliseconds();
	float lockOnTargetBlendLevel;
	if(m_LockOnTargetBlendDuration == 0)
	{
		lockOnTargetBlendLevel = 1.0f;
	}
	else
	{
		lockOnTargetBlendLevel	= static_cast<float>(currentTime - m_LockOnTargetBlendStartTime) /
			static_cast<float>(m_LockOnTargetBlendDuration);
	}

	lockOnTargetBlendLevel			= SlowInOut(lockOnTargetBlendLevel);
	lockOnTargetBlendLevel			= SlowOut(lockOnTargetBlendLevel);

	if(lockOnTargetBlendLevel >= (1.0f - SMALL_FLOAT))
	{
		//Clamp to the desired lock-on target position and clean up the blend.
		m_LockOnTargetPosition						= desiredLockOnTargetPosition;
		m_LockOnTargetBlendStartTime				= 0;
		m_LockOnTargetBlendDuration					= 0;
		m_LockOnTargetBlendLevelOnPreviousUpdate	= 0.0f;
		m_ShouldBlendLockOnTargetPosition			= false;

		return;
	}

	if(lockOnTargetBlendLevel > SMALL_FLOAT)
	{
		const float errorRatioToApplyThisUpdate	= (lockOnTargetBlendLevel < m_LockOnTargetBlendLevelOnPreviousUpdate) ? 0.0f :
			((lockOnTargetBlendLevel - m_LockOnTargetBlendLevelOnPreviousUpdate) /
			(1.0f - m_LockOnTargetBlendLevelOnPreviousUpdate));

		Vector3 desiredLockOnTargetDelta = desiredLockOnTargetPosition - m_Frame.GetPosition();
		Vector3 desiredLockOnTargetFront(desiredLockOnTargetDelta);
		desiredLockOnTargetFront.Normalize();

		Vector3 lockOnPosition = m_LockOnTarget ? VEC3V_TO_VECTOR3(m_LockOnTarget->GetTransform().Transform(VECTOR3_TO_VEC3V(m_LockOnTargetPositionLocal))) : m_LockOnTargetPosition;

		Vector3 currentLockOnTargetDelta = lockOnPosition - m_Frame.GetPosition();
		Vector3 currentLockOnTargetFront(currentLockOnTargetDelta);
		currentLockOnTargetFront.NormalizeSafe(desiredLockOnTargetFront);

		Vector3 axis;
		float angle;
		camFrame::ComputeAxisAngleRotation(currentLockOnTargetFront, desiredLockOnTargetFront, axis, angle);

		angle *= errorRatioToApplyThisUpdate;

		Quaternion orientationDeltaToApply;
		orientationDeltaToApply.FromRotation(axis, angle);

		Vector3 lockOnTargetFrontToApply;
		orientationDeltaToApply.Transform(currentLockOnTargetFront, lockOnTargetFrontToApply);

		//Lerp the distance to the lock-on target commensurate with the angular (s)lerp.
		const float currentLockOnTargetDistance	= currentLockOnTargetDelta.Mag();
		const float desiredLockOnTargetDistance	= desiredLockOnTargetDelta.Mag();

		const float lockOnTargetDistanceToApply	= Lerp(errorRatioToApplyThisUpdate, currentLockOnTargetDistance, desiredLockOnTargetDistance);

		m_LockOnTargetPosition = m_Frame.GetPosition() + (lockOnTargetDistanceToApply * lockOnTargetFrontToApply);

		m_LockOnTargetBlendLevelOnPreviousUpdate = lockOnTargetBlendLevel;
	}
	else
	{
		m_LockOnTargetBlendLevelOnPreviousUpdate = 0.0f;
	}
}

bool camFirstPersonShooterCamera::GetBoneHeadingAndPitch(const CPed* pPed, u32 boneTag, Matrix34& boneMatrix, float& heading, float& pitch, float fTolerance/*=0.075f*/)
{
	int boneIndex = pPed->GetBoneIndexFromBoneTag((eAnimBoneTag)boneTag);
	if(boneIndex >= 0)
	{
		pPed->GetGlobalMtx(boneIndex, boneMatrix);
		if( (eAnimBoneTag)boneTag == BONETAG_HEAD || (eAnimBoneTag)boneTag == BONETAG_SPINE3)
		{
			// Head matrix is rotated -90 degrees (so right is up and up is left), undo that rotation so heading calc is correct.
			boneMatrix.RotateLocalY( 90.0f * DtoR );
		}
		else if( (eAnimBoneTag)boneTag == BONETAG_L_CLAVICLE )
		{
			// Head matrix is rotated 180 degrees (so right is left and up is down), undo that rotation so heading calc is correct.
			boneMatrix.RotateLocalY( 180.0f * DtoR );
		}

		heading = CalculateHeading(boneMatrix.b, boneMatrix.c, fTolerance);
		pitch   = CalculatePitch(boneMatrix.b, boneMatrix.c, fTolerance);
		return true;
	}
	else
	{
		heading = 0.0f;
		pitch = 0.0f;
		boneMatrix.Identity();
	}
	return false;
}

float camFirstPersonShooterCamera::CalculateHeading(const Vector3& vecFront, const Vector3& vecUp, float fTolerance/*=0.075f*/) const
{
	Vector2 paramATan(-vecFront.x, vecFront.y);
	if( vecFront.z < -1.0f + fTolerance )
	{
		paramATan.Set(-vecUp.x, vecUp.y);		// Use up vector instead.
	} 
	else if( vecFront.z > 1.0f - fTolerance )
	{
		paramATan.Set(vecUp.x, -vecUp.y);		// Use down vector instead.
	}

	float heading = rage::Atan2f(paramATan.x, paramATan.y);
	return heading;
}

float camFirstPersonShooterCamera::CalculatePitch(const Vector3& vecFront, const Vector3& vecUp, float fTolerance/*=0.075f*/) const
{
	float fParam = vecFront.z;
	if( vecFront.z < -1.0f + fTolerance )
	{
		fParam = vecUp.z;		// Use up vector instead.
	} 
	else if( vecFront.z > 1.0f - fTolerance )
	{
		fParam = -vecUp.z;		// Use down vector instead.
	}

	float pitch = AsinfSafe(fParam);
	return pitch;
}

bool camFirstPersonShooterCamera::ProcessLimitHeadingOrientation(const CPed& rPed, bool bInCover, bool bExitingCover)
{
	if (!bInCover)
	{
		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, SHOULD_LIMIT_HEADING_ORIENTATION_WHEN_CORNER_EXITING_COVER, false);
		if (bExitingCover)
		{
			const CTaskExitCover* pExitCoverTask = static_cast<const CTaskExitCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_COVER));
			if (pExitCoverTask && pExitCoverTask->GetState() == CTaskExitCover::State_CornerExit)
			{
				return SHOULD_LIMIT_HEADING_ORIENTATION_WHEN_CORNER_EXITING_COVER;
			}
			else
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, SHOULD_LIMIT_HEADING_ORIENTATION, false);
	bool bShouldLimitHeadingOrientation = SHOULD_LIMIT_HEADING_ORIENTATION;
	return bShouldLimitHeadingOrientation;
}

bool camFirstPersonShooterCamera::ProcessLimitPitchOrientation(const CPed& rPed, bool bInCover)
{
	if (!bInCover || CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonLocoAnimations)
		return true;
	
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, SHOULD_LIMIT_PITCH_ORIENTATION, true);
	bool bShouldLimitPitchOrientation = SHOULD_LIMIT_PITCH_ORIENTATION;

	// This causes orientation pops when turning / coming to stops
	const CTaskMotionInCover* pMotionInCoverTask = (const CTaskMotionInCover*)rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOTION_IN_COVER);
	const bool bIsIdle = pMotionInCoverTask ? pMotionInCoverTask->IsIdleForFirstPerson() : false;

	if (bIsIdle)
	{
		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, LIMIT_PITCH_WHEN_IDLE, true);
		if (LIMIT_PITCH_WHEN_IDLE && !CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonLocoAnimations)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MIN_PITCH_LIMIT_IDLE, -0.408f, -HALF_PI, HALF_PI, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_PITCH_LIMIT_IDLE, QUARTER_PI, -HALF_PI, HALF_PI, 0.01f);
			Vector2 vPitchLimitsIdle(MIN_PITCH_LIMIT_IDLE, MAX_PITCH_LIMIT_IDLE);
			SetRelativePitchLimits(vPitchLimitsIdle);
			bShouldLimitPitchOrientation = true;
		}
	}
	else if (pMotionInCoverTask && 
		pMotionInCoverTask->GetState() != CTaskMotionInCover::State_Stopping &&
		pMotionInCoverTask->GetState() != CTaskMotionInCover::State_StoppingAtEdge &&
		pMotionInCoverTask->GetState() != CTaskMotionInCover::State_StoppingAtInsideEdge)

	{
		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, LIMIT_PITCH_WHEN_MOVING, false);
		if (LIMIT_PITCH_WHEN_MOVING)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MIN_PITCH_LIMIT_MOVING, -QUARTER_PI, -HALF_PI, HALF_PI, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, MAX_PITCH_LIMIT_MOVING, QUARTER_PI, -HALF_PI, HALF_PI, 0.01f);
			Vector2 vPitchLimitsMoving(MIN_PITCH_LIMIT_MOVING, MAX_PITCH_LIMIT_MOVING);
			SetRelativePitchLimits(vPitchLimitsMoving);
			bShouldLimitPitchOrientation = true;
		}
	}	
	return bShouldLimitPitchOrientation;
}

float camFirstPersonShooterCamera::GetRecoilShakeAmplitudeScaling() const
{
	return m_Metadata.m_RecoilShakeAmplitudeScaling;
}

void camFirstPersonShooterCamera::UpdateMotionBlur()
{
	camAimCamera::UpdateMotionBlur();

	TUNE_GROUP_FLOAT(CAM_FPS, MAX_COMBAT_ROLL_MOTIONBLUR, 1.0f, 0.0f, 2.0f, 0.01f);
	if( m_AttachParent && m_AttachParent->GetIsTypePed() )
	{
		const CPed* followPed = (const CPed*)(m_AttachParent.Get());
		if(followPed && (followPed->ShouldBeDead() || followPed->GetIsArrested()))
		{
			return;
		}

		// Start with the value calculated in camAimCamera::UpdateMotionBlur()
		float motionBlurStrength	= m_Frame.GetMotionBlurStrength();

		float moveStrength			= ComputeMotionBlurStrengthForMovement(followPed);
		float damageStrength		= ComputeMotionBlurStrengthForDamage(followPed);
		float rollStrength			= (m_CombatRolling || m_TaskFallLandingRoll) ? m_AnimatedCameraBlendLevel * MAX_COMBAT_ROLL_MOTIONBLUR : 0.0f;
		float lookBehindStrength	= ComputeMotionBlurStrengthForLookBehind();

		motionBlurStrength			= motionBlurStrength + moveStrength + damageStrength + rollStrength + lookBehindStrength;
		motionBlurStrength			= Clamp(motionBlurStrength, 0.0f, 1.0f);

		m_Frame.SetMotionBlurStrength(motionBlurStrength);
	}
	else
	{
		m_MotionBlurPeakAttachParentDamage = 0.0f;
	}
}

float camFirstPersonShooterCamera::ComputeMotionBlurStrengthForMovement(const CPed* followPed)
{
	const camMotionBlurSettingsMetadata* motionBlurSettings = camFactory::FindObjectMetadata<camMotionBlurSettingsMetadata>(m_Metadata.m_MotionBlurSettings.GetHash());
	if(!motionBlurSettings)
	{
		return 0.0f;
	}

	if(motionBlurSettings->m_MovementMotionBlurMaxStrength < SMALL_FLOAT)
	{
		return 0.0f;
	}

	float moveSpeed = 0.0f;

	const float fMinBlurSpeed = motionBlurSettings->m_MovementMotionBlurMinSpeed;
	const float fMaxBlurSpeed = motionBlurSettings->m_MovementMotionBlurMaxSpeed;
	if(followPed)
	{
		//Derive a motion blur strength for based upon fall speed for peds.
		Vector3 vVelocity = followPed->GetVelocity();
		if(followPed->GetUsingRagdoll())
		{
			moveSpeed = vVelocity.Mag();
		}
		else
		{
			moveSpeed = -Min(vVelocity.z, 0.0f);
		}
	}

	float motionBlurStrength = RampValueSafe(moveSpeed, fMinBlurSpeed, fMaxBlurSpeed, 0.0f, motionBlurSettings->m_MovementMotionBlurMaxStrength);

	return motionBlurStrength;
}

float camFirstPersonShooterCamera::ComputeMotionBlurStrengthForDamage(const CPed* followPed)
{
	const camMotionBlurSettingsMetadata* motionBlurSettings = camFactory::FindObjectMetadata<camMotionBlurSettingsMetadata>(m_Metadata.m_MotionBlurSettings.GetHash());
	if(!motionBlurSettings)
	{
		return 0.0f;
	}

	if(motionBlurSettings->m_DamageMotionBlurMaxStrength < SMALL_FLOAT)
	{
		return 0.0f;
	}	

	float motionBlurStrength = 0.0f;

	float health							= followPed->GetHealth();
	float damage							= m_MotionBlurPreviousAttachParentHealth - health;
	m_MotionBlurPreviousAttachParentHealth	= health;
	u32 time								= fwTimer::GetCamTimeInMilliseconds();

	if(followPed->GetPedIntelligence())
	{
		const CTask* pNmBrace = (const CTask*)( followPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_NM_BRACE ) );
		const CTask* pNmThroughWindscreen = (const CTask*)( followPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_NM_THROUGH_WINDSCREEN ) );
		if(pNmBrace || pNmThroughWindscreen)
		{
			if(!m_TreatNMTasksAsTakingFullDamage)
			{
				damage = motionBlurSettings->m_DamageMotionBlurMaxDamage;
			}
			m_TreatNMTasksAsTakingFullDamage = true;
		}
		else
		{
			m_TreatNMTasksAsTakingFullDamage = false;
		}
	}
	else
	{
		m_TreatNMTasksAsTakingFullDamage = false;
	}

	if(damage > m_MotionBlurPeakAttachParentDamage)
	{
		m_MotionBlurForAttachParentDamageStartTime	= time;
		m_MotionBlurPeakAttachParentDamage			= damage;
	}

	if(m_MotionBlurPeakAttachParentDamage > 0.0f)
	{
		u32 timeSinceStart = time - m_MotionBlurForAttachParentDamageStartTime;
		if((timeSinceStart < motionBlurSettings->m_DamageMotionBlurDuration))
		{
			motionBlurStrength	= RampValueSafe(m_MotionBlurPeakAttachParentDamage, motionBlurSettings->m_DamageMotionBlurMinDamage,
									motionBlurSettings->m_DamageMotionBlurMaxDamage, 0.0f, motionBlurSettings->m_DamageMotionBlurMaxStrength);
		}
		else
		{
			m_MotionBlurPeakAttachParentDamage = 0.0f;
		}
	}

	return motionBlurStrength;
}

float camFirstPersonShooterCamera::GetRelativeAttachPositionSmoothRate(const CPed& rPed) const
{
	const bool bAimingFromCover = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover);
	if (bAimingFromCover)
	{
		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, ENABLE_DELAYED_CAM_ON_AIM_INTRO_FROM_PEEK_HIGH, false);
		const CTaskAimGunFromCoverIntro* pAimIntroTask = static_cast<const CTaskAimGunFromCoverIntro*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_FROM_COVER_INTRO));
		if (pAimIntroTask && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsInLowCover) && ENABLE_DELAYED_CAM_ON_AIM_INTRO_FROM_PEEK_HIGH)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, DELAYED_SMOOTH_RATE_MIN, 0.0f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, DELAYED_SMOOTH_RATE_MAX, 1.0f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, DELAYED_SMOOTH_TIME_MIN, 0.25f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, DELAYED_SMOOTH_TIME_MAX, 1.0f, 0.0f, 5.0f, 0.01f);
			const float fLerpRatio = Clamp((pAimIntroTask->GetTimeInState() - DELAYED_SMOOTH_RATE_MIN) / (DELAYED_SMOOTH_TIME_MAX - DELAYED_SMOOTH_TIME_MIN), 0.0f, 1.0f);
			return (1.0f - fLerpRatio) * DELAYED_SMOOTH_RATE_MIN + fLerpRatio * DELAYED_SMOOTH_RATE_MAX;
		}
		else
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, CUSTOM_SMOOTH_RATE_AMING_FROM_COVER, 1.0f, 0.0f, 5.0f, 0.01f);
			return CUSTOM_SMOOTH_RATE_AMING_FROM_COVER;
		}
	}
	return m_Metadata.m_RelativeAttachPositionSmoothRate;
}

float camFirstPersonShooterCamera::ComputeRotationToApplyToPedSpineBone(const CPed& ped) const
{
	if(m_AttachParent.Get() != &ped)
	{
		return 0.0f;
	}

	INSTANTIATE_TUNE_GROUP_FLOAT(CAM_FPS, fGroundAvoidBlendOutAngle, 0.0f, PI, 0.05f);
	// If we are ragdolling or just starting to get up while our head was very close to the ground then don't apply any rotation to spine bone
	if(ped.GetUsingRagdoll() || (m_PlayerGettingUp && Abs(m_GroundAvoidCurrentAngle) >= fGroundAvoidBlendOutAngle))
	{
		return 0.0f;
	}

	float distance = m_CollisionFallBackDistanceSpring.GetResult();

	if (m_RunningAnimTask)
	{
		distance *= Clamp((1.0f - m_AnimatedCameraBlendLevel), 0.0f, 1.0f);
	}

	float length = m_ObjectSpacePosition.Mag();

	crSkeleton* pSkeleton = ped.GetSkeleton();
	if (pSkeleton)
	{
		int boneIndex = -1;
		pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_HEAD, boneIndex);

		if (boneIndex >= 0)
		{
			length = Mag(pSkeleton->GetObjectMtx(boneIndex).GetCol3()).Getf();
		}
	}

	// Approximating arc length s = theta * R => theta = s / R, where s = distance camera moved back and R is distance of camera from root (treated as a radius)
	const float rotationToApply = length > 0.0f ? -distance / length : 0.0f;

	return rotationToApply;
}

void camFirstPersonShooterCamera::CloneBaseOrientation(const camBaseCamera& sourceCamera)
{
	if(sourceCamera.ShouldPreventNextCameraCloningOrientation())
	{
		return;
	}

	//Clone the 'base' front of the source camera, which is independent of any non-propagating offsets.
	Vector3 baseFront = sourceCamera.GetBaseFront();

	bool bIsThirdPersonCamera = false;
	const camControlHelper* sourceControlHelper = NULL;
	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camThirdPersonCamera&>(sourceCamera).GetControlHelper();
		bIsThirdPersonCamera = true;
	}
	else if(sourceCamera.GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camCinematicMountedCamera&>(sourceCamera).GetControlHelper();
	}

	if(sourceControlHelper && sourceControlHelper->IsLookingBehind())
	{
		//NOTE: We need to manually flip the front for look-behind as the base front ignores this.
		const bool bAimingOrShooting = CPlayerInfo::IsAiming(true) || CPlayerInfo::IsFiring_s();
		m_bWaitForLookBehindUp = false;
		if(bAimingOrShooting)
		{
			m_bWaitForLookBehindUp = true;
		}

		if(!m_bWaitForLookBehindUp)
		{
			m_bLookingBehind = !bAimingOrShooting;
			m_bWasLookingBehind |= m_bLookingBehind;
			m_bIgnoreHeadingLimits = !bAimingOrShooting;
			if(m_bLookingBehind)
			{
				baseFront = -sourceCamera.GetBaseFront();
				float fLookBehindAngle = (m_ShouldRotateRightForLookBehind) ? -m_Metadata.m_LookBehindAngle : m_Metadata.m_LookBehindAngle;
				fLookBehindAngle *= DtoR;
				m_RelativeHeading = fLookBehindAngle;
				// The look behind input is not set in this frame, set the state manually and skip the update of this frame
				if(GetControlHelper())
				{
					GetControlHelper()->SetLookBehindState(true);
					GetControlHelper()->IgnoreLookBehindInputThisUpdate();
				}
			}
		}
	}

	TUNE_GROUP_BOOL(CAM_FPS, c_bUseHeadBoneHeadingWhenSwitchingToFirstFromThird, false);
	const CPed* attachPed = camInterface::GetGameplayDirector().GetFollowPed();
	if(c_bUseHeadBoneHeadingWhenSwitchingToFirstFromThird && attachPed && !attachPed->GetUsingRagdoll() && bIsThirdPersonCamera)
	{
		float fPedHeading  = attachPed->GetDesiredHeading();
		float fCameraPitch = camFrame::ComputePitchFromFront(baseFront);
		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(fPedHeading, fCameraPitch);
		const_cast<CPed*>(attachPed)->SetHeading( fPedHeading );
		m_bIgnoreHeadingLimits = true;
	}
	else
	{
		m_Frame.SetWorldMatrixFromFront(baseFront);
	}
	m_Frame.ComputeHeadingAndPitch(m_CameraHeading, m_CameraPitch);

	//Ignore any pending orientation override requests and respect the cloned orientation.
	m_ShouldOverrideRelativeHeading	= false;
	m_ShouldOverrideRelativePitch	= false;
}

bool camFirstPersonShooterCamera::GetObjectSpaceCameraMatrix(const CEntity* pTargetEntity, Matrix34& mat) const
{
	bool result = camBaseCamera::GetObjectSpaceCameraMatrix(pTargetEntity, mat);
	TUNE_GROUP_BOOL(CAM_FPS, bUseObjectSpacePosition, false);
	if (result && m_AttachParent && m_AttachParent==pTargetEntity)
	{
		// TODO: generate proper object space position when using animated camera or head/spine bone position.
		if(bUseObjectSpacePosition && !m_bUseAnimatedFirstPersonCamera && !m_bWasUsingAnimatedFirstPersonCamera && !m_bWasUsingHeadSpineOrientation)
		{
			mat.d = m_ObjectSpacePosition;
		}
		return true;
	}

	return result;
}

void camFirstPersonShooterCamera::UpdateFrameShakers(camFrame& frameToShake, float amplitudeScalingFactor)
{
	const CPed* attachPed = (m_AttachParent && m_AttachParent->GetIsTypePed()) ? static_cast<const CPed*>(m_AttachParent.Get()) : NULL;

	const bool c_IsPlayerVaulting = attachPed && attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting);
	const bool c_WasInCoverForVault = (m_CoverVaultWasFacingLeft || m_CoverVaultWasFacingRight);
	if(c_IsPlayerVaulting && c_WasInCoverForVault)
	{
		// Disable shake when vaulting from cover to prevent camera turning the wrong way.
		return;
	}

	camFirstPersonAimCamera::UpdateFrameShakers(frameToShake, amplitudeScalingFactor);

	TUNE_GROUP_BOOL(CAM_FPS, shouldBypassFrameShake, false)

	if (attachPed && !shouldBypassFrameShake)
	{
		const camFirstPersonShooterCameraMetadataRelativeAttachOrientationSettings& settings = m_Metadata.m_RelativeAttachOrientationSettings;

		Matrix34 headBoneMatrix;
		attachPed->GetBoneMatrix(headBoneMatrix, BONETAG_HEAD);
		headBoneMatrix.RotateLocalY(90.0f * DtoR); //head matrix is rotated -90 degrees

		Matrix34 attachPedMatrix = MAT34V_TO_MATRIX34(attachPed->GetTransform().GetMatrix());
		Matrix34 relativeHeadBoneMatrix;
		relativeHeadBoneMatrix.DotTranspose(headBoneMatrix, attachPedMatrix);

		float relativeHeadHeading, relativeHeadPitch, relativeHeadRoll;
		camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(relativeHeadBoneMatrix, relativeHeadHeading, relativeHeadPitch, relativeHeadRoll);

		const bool shouldCutSpring = ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));

		const Vector3 springConstants = shouldCutSpring ? VEC3_ZERO : settings.m_PerAxisSpringConstants;

		const float dampedRelativeHeadPitch = m_RelativeAttachPitchSpring.Update(relativeHeadPitch, springConstants.x, settings.m_PerAxisSpringDampingRatios.x);
		const float dampedRelativeHeadRoll = m_RelativeAttachRollSpring.Update(relativeHeadRoll, springConstants.y, settings.m_PerAxisSpringDampingRatios.y);
		const float dampedRelativeHeadHeading = m_RelativeAttachHeadingSpring.Update(relativeHeadHeading, springConstants.z, settings.m_PerAxisSpringDampingRatios.z);

		float desiredBlendLevel = 0.0f;

		const CPedMotionData* attachPedMotionData = attachPed->GetMotionData();
		const float moveBlendRatio = attachPedMotionData ? Abs(attachPedMotionData->GetCurrentMoveBlendRatio().Mag()) : 0.0f;

		const bool isRelaxedAiming = attachPedMotionData && attachPedMotionData->GetIsFPSIdle() && (!attachPed->GetIsInCover() || !CTaskMotionInCover::ms_Tunables.m_EnableFirstPersonAimingAnimations);	// Frame shaker messes with accurate peek/aim pos
		const bool shouldUseAimingBlendLevel = (!isRelaxedAiming) && (m_IsAiming || (!m_AttachedIsUnarmed && settings.m_ShouldConsiderArmedAsAiming));

		float fBlendLevelSpringConstant = settings.m_BlendLevelSpringConstant;
		float fBlendLevelSpringDampingRatio = settings.m_BlendLevelSpringDampingRatio;

		const bool bUseCustomBlendLevelForSituation = attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDiving);	

		if (m_IsLockedOn || m_HasStickyTarget || m_IsAiming || !attachPed->GetMotionData()->GetIsFPSIdle() || CPauseMenu::GetMenuPreference(PREF_FPS_HEADBOB) == FALSE)
		{
			desiredBlendLevel = 0.0f;
			fBlendLevelSpringConstant = settings.m_LockOnBlendLevelSpringConstant;
			fBlendLevelSpringDampingRatio = settings.m_LockOnBlendLevelSpringDampingRatio;
			if(moveBlendRatio >= MBR_SPRINT_BOUNDARY && m_StartedAimingWithInputThisUpdate)
			{
				// Since we use the PostEffectFrame, no need to blend out the offset.
				m_RelativeAttachBlendLevelSpring.Reset(desiredBlendLevel);
			}
		}
		//if sprinting, never use aiming blend level
		else if ((moveBlendRatio < MBR_SPRINT_BOUNDARY) && shouldUseAimingBlendLevel)
		{
			desiredBlendLevel = settings.m_BlendLevelWhenAiming;
		}
		else if (!bUseCustomBlendLevelForSituation)
		{
			if (moveBlendRatio >= MBR_SPRINT_BOUNDARY)
			{
				if (attachPed->GetIsSwimming())
				{
					desiredBlendLevel = settings.m_BlendLevelWhenSwimSprinting;
				}
				else
				{
					desiredBlendLevel = settings.m_BlendLevelWhenSprinting;
				}
			}
			else if (moveBlendRatio >= MBR_RUN_BOUNDARY)
			{
				desiredBlendLevel = settings.m_BlendLevelWhenRunning;
			}
			else if (moveBlendRatio >= MBR_WALK_BOUNDARY)
			{
				desiredBlendLevel = settings.m_BlendLevelWhenWalking;
			}
			else
			{
				desiredBlendLevel = settings.m_BlendLevelWhenIdle;
			}
		}
		else if (bUseCustomBlendLevelForSituation)
		{
			if (attachPed->GetPedResetFlag(CPED_RESET_FLAG_IsDiving))
			{
				desiredBlendLevel = settings.m_BlendLevelWhenDiving;	//0.1f
			}
		}

		const float blendLevel = m_RelativeAttachBlendLevelSpring.Update(desiredBlendLevel, shouldCutSpring ? 0.0f : fBlendLevelSpringConstant, fBlendLevelSpringDampingRatio);

		const float pitchOffset = (dampedRelativeHeadPitch + (settings.m_MaxPitchOffset * DtoR)) * blendLevel * settings.m_PitchBlendLevelScalar;
		const float rollOffset = dampedRelativeHeadRoll * blendLevel * settings.m_RollBlendLevelScalar;
		const float headingOffset = dampedRelativeHeadHeading * blendLevel * settings.m_HeadingBlendLevelScalar;

		float heading, pitch, roll;
		camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(frameToShake.GetWorldMatrix(), heading, pitch, roll);

		const float desiredPitch = fwAngle::LimitRadianAngle(pitch + pitchOffset);
		const float desiredRoll = fwAngle::LimitRadianAngle(roll + rollOffset);
		const float desiredHeading = fwAngle::LimitRadianAngle(heading + headingOffset);

		frameToShake.SetWorldMatrixFromHeadingPitchAndRoll(desiredHeading, desiredPitch, desiredRoll);
	}
}

#if __BANK
void camFirstPersonShooterCamera::DebugRenderInfoTextInWorld(const Vector3& vPos)
{
	const Color32 colour = CRGBA(0, 0, 0, 255);
	char debugText[64];
	s32 lineNb = 0;

	grcDebugDraw::Text(vPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), SAFE_CSTRING(m_Metadata.m_Name.GetCStr()));

	formatf(debugText, "Pelvis Offset  : %6.3f", m_AttachPedPelvisOffsetSpring.GetResult());
	grcDebugDraw::Text(vPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	const float invAnimatedCameraBlendLevel = Clamp((1.0f - m_AnimatedCameraBlendLevel), 0.0f, 1.0f);
	const float blendLevel = m_CollisionFallBackBlendSpring.GetResult() * invAnimatedCameraBlendLevel;
	const float distance = m_CollisionFallBackDistanceSpring.GetResult() * invAnimatedCameraBlendLevel;

	formatf(debugText, "Collision Blend: %5.3f", blendLevel);
	grcDebugDraw::Text(vPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Collision Dist : %5.3f", distance);
	grcDebugDraw::Text(vPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
}

void camFirstPersonShooterCamera::DebugRender()
{
#if KEYBOARD_MOUSE_SUPPORT
	TUNE_GROUP_BOOL(CAM_FPS, dbgRenderInputValues, false);
#endif
	TUNE_GROUP_BOOL(CAM_FPS_ENTER_EXIT, dbgRenderEnterExitText, false);
	TUNE_GROUP_BOOL(CAM_FPS, dbgRenderCameraHeadingModifiers, false);
	TUNE_GROUP_BOOL(CAM_FPS, dbgRenderSprintAim, false);

	camFirstPersonAimCamera::DebugRender();

	if(camManager::GetSelectedCamera() == this)
	{
		//NOTE: These elements are drawn even if this is the active/rendering camera. This is necessary in order to debug the camera when active.
		if(m_IsLockedOn)
		{
			static dev_float radius = 0.20f;
			grcDebugDraw::Sphere(m_LockOnTargetPosition, radius, Color_orange);
		}

		//NOTE: These elements are drawn even if this is the active/rendering camera. This is necessary in order to debug the camera when active.
		if(m_HasStickyTarget)
		{
			Vector3 vStickyPosition = m_StickyAimTargetPosition;
			//! convert to local space position.
			MAT34V_TO_MATRIX34(m_StickyAimTarget->GetTransform().GetMatrix()).Transform(vStickyPosition);

			static dev_float radius = 0.20f;
			grcDebugDraw::Sphere(vStickyPosition, radius, Color_orange);
		}

		Color32 colour = CRGBA(0, 0, 0, 255);
		char debugText[128];
		s32 lineNb = 0;

		const float fSideOffset = m_IsScopeAiming ? 0.5f : 1.5f;
		const float fUpOffset   = m_IsScopeAiming ? 0.3f : 0.5f;

		const Matrix34& camMatrix = m_Frame.GetWorldMatrix();
		Vector3 vTextPos = camMatrix.d + camMatrix.b * 3.0f + camMatrix.c * fUpOffset - camMatrix.a * fSideOffset;

	#if KEYBOARD_MOUSE_SUPPORT
		float headingInput = 0.0f;
		float pitchInput   = 0.0f;
		float lookAroundHeadingOffset = 0.0f;
		float lookAroundPitchOffset   = 0.0f;
		bool  bHeadingNeedsDeadzone = false;
		bool  bPitchNeedsDeadzone = false;
		CControl* pControl = camInterface::GetGameplayDirector().GetActiveControl();
		if(pControl)
		{
			// "Normalize" the raw input to bring within the same range as the other values so we can see and compare.
			PF_SET(Raw_Mouse_X_Input, -(float)rage::ioMouse::GetDX()/127.0f);
			PF_SET(Raw_Mouse_Y_Input, -(float)rage::ioMouse::GetDY()/127.0f);

			if( m_CurrentControlHelper )
			{
				const ioValue& headingControl	= pControl->GetPedAimWeaponLeftRight();
				const ioValue& pitchControl		= pControl->GetPedAimWeaponUpDown();

				ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
				headingInput				 = -headingControl.GetUnboundNorm(options);
				pitchInput					 = -pitchControl.GetUnboundNorm(options);

				////float inputMag = rage::Sqrtf((headingInput * headingInput) + (pitchInput * pitchInput));
				float fDeadZone = ioValue::DEFAULT_DEAD_ZONE_VALUE; ////m_CurrentControlHelper->GetDeadZoneScaling(inputMag);

				ioValue::ReadOptions deadZoneOptions	= ioValue::DEFAULT_OPTIONS;
				deadZoneOptions.m_DeadZone = fDeadZone;

				if( ioValue::RequiresDeadZone(headingControl.GetSource()) )
				{
					// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
					bHeadingNeedsDeadzone = true;
					float deadzonedInput = -headingControl.GetUnboundNorm(deadZoneOptions);
					if(deadzonedInput == 0.0f)
						headingInput = 0.0f;
				}

				if( ioValue::RequiresDeadZone(pitchControl.GetSource()) )
				{
					// Re-read input with deadzone and clear un-deadzoned input if deadzoned input is zero.
					bPitchNeedsDeadzone = true;
					float deadzonedInput = -pitchControl.GetUnboundNorm(deadZoneOptions);
					if(deadzonedInput == 0.0f)
						pitchInput = 0.0f;
				}

				PF_SET(Base_Heading_Input, headingInput);
				PF_SET(Base_Pitch_Input, pitchInput);

				float fovOrientationScaling = m_Frame.GetFov() / g_DefaultFov;

				lookAroundHeadingOffset	 = m_CurrentControlHelper->GetLookAroundHeadingOffset();
				lookAroundHeadingOffset	*= fovOrientationScaling;
				lookAroundPitchOffset	 = m_CurrentControlHelper->GetLookAroundPitchOffset();
				lookAroundPitchOffset	*= fovOrientationScaling;

				PF_SET(ControlHelper_heading, lookAroundHeadingOffset);
				PF_SET(ControlHelper_pitch, lookAroundPitchOffset);
			}
		}

		if(dbgRenderInputValues)
		{
			Color32 colour = CRGBA(255, 255, 0, 255);
			formatf(debugText, "Raw Mouse Input: x %06f  y %06f", rage::ioMouse::GetDX(), rage::ioMouse::GetDY() );
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		#if RSG_EXTRA_MOUSE_SUPPORT
			formatf(debugText, "Normalized Mouse Input: x %0.6f  y %0.6f", rage::ioMouse::GetNormalizedX(), rage::ioMouse::GetNormalizedY() );
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		#endif // RSG_EXTRA_MOUSE_SUPPORT

			formatf(debugText, "Camera's base input: x %0.6f  y %0.6f  %s %s", headingInput, pitchInput,
					bHeadingNeedsDeadzone ? "x  true" : "x false", bPitchNeedsDeadzone ? "y  true" : "y false");
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			if( m_CurrentControlHelper )
			{
				formatf(debugText, "ControlHelper values: x %0.6f  y %0.6f", lookAroundHeadingOffset, lookAroundPitchOffset);
				grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
		}
		else
	#endif // KEYBOARD_MOUSE_SUPPORT
		if(dbgRenderEnterExitText && ( m_EnteringVehicle || m_ExitingVehicle || m_bEnterBlending ))
		{
			formatf(debugText, "Status: %s", m_bEnterBlending ? "Enter Blending" : (m_EnteringVehicle ? "Enter Vehicle" : "Exit Vehicle"));
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			formatf(debugText, "Vehicle Seat Interp: %0.6f", m_VehSeatInterp);
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			if(m_bEnterBlending && !m_bEnterBlendComplete)
			{
				u32 uTimeRemaining = (m_ExtraVehicleEntryDelayStartTime != 0) ? (fwTimer::GetTimeInMilliseconds() - m_ExtraVehicleEntryDelayStartTime) : 0;
				formatf(debugText, "Enter Blend Time Remaining: %d", uTimeRemaining);
				grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
		}
		else if(dbgRenderCameraHeadingModifiers)
		{
			if( m_CurrentControlHelper )
			{
				//NOTE: m_ControlHelper has already been validated.
				float lookAroundHeadingOffset = m_CurrentControlHelper->GetLookAroundHeadingOffset();
				float lookAroundPitchOffset   = m_CurrentControlHelper->GetLookAroundPitchOffset();
				formatf(debugText, "Camera Yaw   Input: %0.6f", lookAroundHeadingOffset);
				grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
				formatf(debugText, "Camera Pitch Input: %0.6f", lookAroundPitchOffset);
				grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
				++lineNb;
			}

			formatf(debugText, "Relative Heading: %0.6f", m_RelativeHeading);
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			formatf(debugText, "Relative Pitch  : %0.6f", m_RelativePitch);
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			++lineNb;
			formatf(debugText, "Look Behind %s   Was Looking Behind %s", m_bLookingBehind ? "yes" : "no", m_bWasLookingBehind ? "yes" : "no");
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			++lineNb;
			formatf(debugText, "Accumulated Heading: %0.6f", m_AnimatedCameraHeadingAccumulator);
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			formatf(debugText, "Accumulated Pitch  : %0.6f", m_AnimatedCameraPitchAccumulator);
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			if(m_MeleeRelativeHeading != 0.0f || m_MeleeRelativePitch != 0.0f)
			{
				++lineNb;
				formatf(debugText, "Melee Relative Heading: %0.6f", m_MeleeRelativeHeading);
				grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
				formatf(debugText, "Melee Relative Pitch  : %0.6f", m_MeleeRelativePitch);
				grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
		}
		else if(dbgRenderSprintAim && m_AttachParent.Get() && m_AttachParent->GetIsTypePed())
		{
			const CPed* attachPed = (const CPed*)m_AttachParent.Get();
			const CPedMotionData* attachPedMotionData = attachPed->GetMotionData();
			const float moveBlendRatio = attachPedMotionData ? Abs(attachPedMotionData->GetCurrentMoveBlendRatio().Mag()) : 0.0f;
			formatf(debugText, "Sprinting: %s", (moveBlendRatio >= MBR_SPRINT_BOUNDARY) ? "Yes" : "No");
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			formatf(debugText, "Aiming Accumulated Time: %f", m_AccumulatedInputTimeWhileSprinting);
			grcDebugDraw::Text(vTextPos, colour, 0, ++lineNb * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}
}
#endif // __BANK

#endif // FPS_MODE_SUPPORTED
