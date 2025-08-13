//
// camera/gameplay/aim/FirstPersonShooterCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FIRST_PERSON_SHOOTER_CAMERA_H
#define FIRST_PERSON_SHOOTER_CAMERA_H

#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/helpers/DampedSpring.h"
#include "peds/pedDefines.h"								// include for FPS_MODE_SUPPORTED define
#include "system/control.h"									// include for KEYBOARD_MOUSE_SUPPORT define

#if FPS_MODE_SUPPORTED

class CControl;
class CPed;
class camEnvelope;
class camHintHelper;
class CTaskMobilePhone;
class CTaskMotionInAutomobile;
class CTaskParachute;
class CVehicle;
class CVehicleModelInfo;
class camCinematicMountedCameraMetadata;
class camFirstPersonShooterCameraMetadataOrientationSpring;


class camFirstPersonShooterCameraMetadata;

class camFirstPersonShooterCamera : public camFirstPersonAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFirstPersonShooterCamera, camFirstPersonAimCamera);

struct CustomSituationsParams
{
	const CTaskParachute* pParachuteTask;
	const CTaskMobilePhone* pMobilePhoneTask;
	float targetPitch;
	float lookAroundHeadingOffset;
	float lookAroundPitchOffset;
	float currentRoll;
	float previousAttachParentHeading;
	float previousAttachParentPitch;
	bool  bEnteringVehicle;
	bool  bClimbing;
	bool  bClimbingLadder;
	bool  bMountingLadderTop;
	bool  bMountingLadderBottom;
	bool  bJumping;
	bool  bCarSlide;
	bool  bUsingHeadSpineOrientation;
	bool  bGettingArrestedInVehicle;
};

struct HeadSpineOrientationParams
{
	Quaternion qPreviousCamera;
	float lookAroundHeadingOffset;
	float lookAroundPitchOffset;
	bool  bUseHeadSpineBoneOrientation;
	bool  bUsedHeadSpineBoneLastUpdate;
	bool  bIsFalling;
};

struct SpringInputParams
{
	float headingInput;
	float pitchInput;
	bool bEnable;
	bool bAllowHorizontal;
	bool bAllowLookDown;
	bool bEnableSpring;
	bool bReset;
};

enum eCoverHeadingCorrectionType
{
	COVER_HEADING_CORRECTION_INVALID,
	COVER_HEADING_CORRECTION_IDLE,
	COVER_HEADING_CORRECTION_INTRO,
	COVER_HEADING_CORRECTION_AIM,
	COVER_HEADING_CORRECTION_OUTRO,
	COVER_HEADING_CORRECTION_TURN,
	COVER_HEADING_CORRECTION_MOVING
};

private:
	camFirstPersonShooterCamera(const camBaseObjectMetadata& metadata);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	~camFirstPersonShooterCamera();

	virtual const camControlHelper*	GetControlHelper() const	{ return m_CurrentControlHelper; }
	virtual camControlHelper*		GetControlHelper()			{ return m_CurrentControlHelper; }

	bool IsBlendingOrientation()			{ return m_BlendOrientation; }
	bool IsStrafeEnabled() const			{ return m_AllowStrafe; }
	bool IsStickWithinStrafeAngleThreshold() const;

	void OnBulletImpact(const CPed* pAttacker, const CPed* pVictim);
	void OnMeleeImpact(const CPed* pAttacker, const CPed* pVictim);
	void OnVehicleImpact(const CPed* pAttacker, const CPed* pVictim);
	void OnDeath(const CPed* pVictim);

	bool IsAiming() const { return m_IsAiming; }
	bool IsScopeAiming() const { return m_IsScopeAiming; }
	bool ShowReticle() const { return m_ShowReticle; }
	float GetRecoilShakeAmplitudeScaling() const;
	bool IsLookingBehind() const { return m_bLookingBehind; }
	bool IsDoingLookBehindTransition() const { return m_bDoingLookBehindTransition; }
	bool IsUsingAnimatedData() const { return m_bUseAnimatedFirstPersonCamera; }
	bool IsAnimatedCameraDataBlendingOut() const { return (!m_bUseAnimatedFirstPersonCamera && m_bWasUsingAnimatedFirstPersonCamera); }
	bool IsAnimatedCameraUpdatingInBlendOut() const { return (!m_bUseAnimatedFirstPersonCamera && m_UpdateAnimCamDuringBlendout); }
	bool IsAnimatedCameraFovSet() const { return (m_AnimatedFov > SMALL_FLOAT); }
	s32  GetAnimatedCameraBlendInDuration()  const { return m_AnimBlendInDuration;  }
	s32  GetAnimatedCameraBlendOutDuration() const { return m_AnimBlendOutDuration; }

    void SetScriptPreventingOrientationResetWhenRenderingAnotherCamera() { m_ScriptPreventingOrientationResetWhenRenderingAnotherCamera = true; }

	float GetLowCoverHeightTValue(const CPed& rPed) const;
	bool IsAimingDirectly(const CPed& rPed, float fAngleBetweenCoverAndCamera, float fPitch, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, float fThresholdModifier = 1.0f) const;

	const Vector3& GetObjectSpacePosition() const				{ return m_ObjectSpacePosition; }
	float GetHeading() const { return m_CameraHeading; }
	float GetPitch() const { return m_CameraPitch; }
	float GetAnimatedWeight() const { return m_AnimatedWeight; }

	const CVehicle* GetVehicle() const { return m_pVehicle.Get(); }
	bool IsEnteringVehicle() const    { return m_EnteringVehicle; }
	bool IsBlendingForVehicle() const { return m_bEnterBlending; }
	bool IsBlendingForVehicleComplete() const { return m_bEnterBlendComplete; }
	bool IsEnteringTurretSeat() const { return m_bEnteringTurretSeat; }

	bool ShouldGoIntoFallback() const { return m_bShouldGoIntoFallbackMode; }

	float GetScopeFOV() const { return m_ScopeFov; }
	const Vector3& GetScopeOffset() const { return m_CurrentScopeOffset; }
	const Vector3& GetScopeRotationOffset() const { return m_CurrentScopeRotationOffset; }
	const Vector3& GetNonScopeOffset() const { return m_CurrentNonScopeOffset; }
	const Vector3& GetNonScopeRotationOffset() const { return m_CurrentNonScopeRotationOffset; }
	const Vector3& GetThirdPersonOffset() const { return m_CurrentThirdPersonOffset; }
	float GetHighHeelOffset() const { return m_ShoesOffset; }

	u32 GetDeathShakeHash() const { return m_DeathShakeHash; }

	float ComputeRotationToApplyToPedSpineBone(const CPed& ped) const;

	virtual void	CloneBaseOrientation(const camBaseCamera& sourceCamera);
	virtual void	UpdateFrameShakers(camFrame& frameToShake, float amplitudeScalingFactor = 1.0f);

	float	GetRelativeHeading(bool bForceRead = false) const;
	float	GetRelativePitch(  bool bForceRead = false) const;
	float	GetMeleeRelativeHeading() const { return m_MeleeRelativeHeading; }
	float	GetMeleeRelativePitch() const   { return m_MeleeRelativePitch; }

	// If in scope mode AND combat rolling, do not allow secondary motion as the rifles clip the camera.
	float	IsIkSecondaryMotionAllowed() const		{ return (!m_CombatRolling || !m_IsScopeAiming); }
	float	IsIkSecondaryMotionAttenuated() const	{ return (m_CombatRolling || m_WasCombatRolling); }

	bool	IsSprintBreakOutActive() const { return m_SprintBreakOutTriggered && !m_SprintBreakOutFinished; }

	virtual bool	GetObjectSpaceCameraMatrix(const CEntity* pTargetEntity, Matrix34& mat) const;


	const Vector2& GetAnimatedSpringHeadingLimits() const	{ return m_AnimatedCamSpringHeadingLimitsRadians; }
	const Vector2& GetAnimatedSpringPitchLimits() const		{ return m_AnimatedCamSpringPitchLimitsRadians; }

	bool IsLockedOn() const						{ return m_IsLockedOn; }
	float GetLockOnEnvelopeLevel() const		{ return m_LockOnEnvelopeLevel; }

	void	StartCustomFovBlend(float fovDelta, u32 duration);

	bool	IsTableGamesCamera(const camBaseCamera* camera = nullptr) const;
	const camEnvelope* GetTableGamesHintEnvelope() const;

	static void	GetFirstPersonAsThirdPersonOffset(const CPed* pPed, const CWeaponInfo* pWeaponInfo, Vector3& offset);

	static bool	WhenSprintingUseRightStick(const CPed* attachPed, const CControl& rControl);

#if __BANK
	virtual void	DebugRenderInfoTextInWorld(const Vector3& vPos); 
	virtual void	DebugRender();
#endif // __BANK

protected:
	virtual bool	Update();
	virtual void	UpdateZoom();
	virtual void	UpdatePosition();
	virtual void	UpdateOrientation();
	virtual void	UpdateControlHelper();
	virtual void	UpdateMotionBlur();
	virtual void	UpdateDof();
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	// Prevent base camFirstPersonAimCamera class from updating the sticky aim helper (needs to be done within here).
	virtual bool	CanUpdateStickyAimHelper() override { return false; }

private:

	// We are overriding UpdatePosition, so we can have our own version of ComputeRelativeAttachPosition since we are calling it. (this one calls base class version of ComputeRelativeAttachPosition)
	void			ComputeRelativeAttachPositionInternal(const Matrix34& targetMatrix, Vector3& relativeAttachPosition, Vector3& relativeFallbackPosition);
	void			CreateOrDestroyHintHelper();
	void			UpdateDofAimingBehaviour();
	void			UpdateDofMobileBehaviour();
	void			UpdateDofHighCoverBehaviour();
	void			UpdateDofParachuteBehaviour();
	void			UpdateTaskState(const CPed* attachPed);
	void			UpdateWeaponState(const CPed* attachedPed);
	void			UpdateVehicleState(const CPed* attachedPed);
	void			UpdateStrafeState();
	void			UpdateLookBehind(const CPed* attachPed);
	void			UpdateAnimatedCameraData(const CPed* attachPed);
	void			UpdateAnimatedCameraLimits(const CPed* attachPed);
	void			UpdateConstrainToFallBackPosition(const CPed& attachPed);
	void			UpdateAttachPedPelvisOffsetSpring();
	void			UseHeadSpineBoneOrientation(const CPed* attachPed, const HeadSpineOrientationParams& oParam, bool bAnimatedCameraChangesStickHeading);
	void			UseAnimatedCameraData(CPed* attachPed, const Matrix34& previousCameraMatrix, float lookAroundHeadingOffset, float lookAroundPitchOffset,
										  float& animatedHeading, float& animatedPitch, float lockonHeading, float lockonPitch, bool bAccumulatedHeadingAppliedToPedHeading);
	void			UseGroundAvoidance(const CPed& attachPed);
	void			UpdateRelativeIdleCamera();
	void			CloneCinematicCamera();
	bool			ComputeDesiredPitchForSlope(const CPed* attachedPed, float& fTargetPitch);
	void			ComputeAttachPedPelvisOffsetForShoes(CPed* attachPed);

	float			CalculateCoverAutoHeadingCorrectionAngleOffset(const CPed& rPed, float pitch, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner) const;
	void			ProcessEnterCoverOrientation(CPed& rPed, float& heading, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner);
	void			ProcessCoverAutoHeadingCorrection(CPed& rPed, float& heading, float pitch, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, bool bAimDirectly);
	void			ProcessCoverPitchRestriction(const CPed& rPed, float heading, float& pitch, bool bInHighCover);
	void			ProcessCoverRoll(const CPed& rPed, float& roll, float heading, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, bool bAimDirectly);
	void			ProcessRelativeAttachOffset(const CPed* pPed);
	Vector3			ComputeDesiredAttachOffset(const CPed& rPed);
	bool			HandleCustomSituations(	const CPed* attachPed,
											float& heading, float& pitch, float& desiredRoll,
											bool& bOrientationRelativeToAttachedPed, const CustomSituationsParams& oParam );

	void			UpdateCustomFovBlend(float fCurrentFov);

	void			UpdateHeadingPitchSpringLimits();
	void			GetHeadingPitchSpringLimits(Vector2 &springHeadingLimitsRadians, Vector2 &springPitchLimitsRadians);
	void			CalculateRelativeHeadingPitch(const SpringInputParams& oParam, bool bIsLookingBehind, float heading=0.0f, float pitch=0.0f, const Matrix34* pmtxFinalAnimated=NULL);
	const camFirstPersonShooterCameraMetadataOrientationSpring& GetCurrentOrientationSpring() const;

	bool			CustomLookParachuting(const CPed* attachPed, const CTaskParachute* pParachuteTask, float& heading, float& pitch,
											bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation, bool& bOrientationRelativeToAttachedPed);

	bool			CustomLookDiving(float& heading, float& pitch, bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation);

	bool			CustomLookAtPosition(Vector3 vTargetPosition, float& heading, float& pitch, float fHeadingBlendRate, float fPitchBlendRate, bool bOrientationRelativeToAttachedPed);

	bool			CustomLookAtHand(const CPed* attachPed, const CTaskMotionInAutomobile* pMotionAutomobileTask,
									 float& heading, float& pitch, float slopePitch, bool& bOrientationRelativeToAttachedPed, 
									 bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation);
	bool			CustomLookAtMeleeTarget(const CPed* attachPed, CPed*& pCurrentMeleeTarget,
											float& heading, float& pitch, bool& bOrientationRelativeToAttachedPed,
											bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation);
	bool			AnimatedLookAtMeleeTarget(const CPed* attachPed, const CPed*& pCurrentMeleeTarget, float heading, float pitch);
	bool			CustomLookEnterExitVehicle( const CPed* attachPed, float& heading, float& pitch, float& desiredRoll, const CustomSituationsParams& oParam,
											    bool& bOrientationRelativeToAttachedPed, bool& bAllowLookAroundDuringCustomSituation );
	bool			HandlePlayerGettingCarJacked(const CPed* attachPed, const Vector3& playerHeadPosition, const Vector3& playerSpinePosition, float& heading, float& pitch, float& lookDownPitchLimit);
	bool			CustomLookClimb( const CPed* attachPed, float& heading, float& pitch,
									 bool& bOrientationRelativeToAttachedPed, const CustomSituationsParams& oParam,
									 bool& bAllowLookAroundDuringCustomSituation, bool& bAllowLookDownDuringCustomSituation );
	bool			AdjustTargetForCarJack(	const CPed* attachPed, const Matrix34& seatMtx, const Matrix34& playerSpineMtx,
											Vector3& vTarget, const Vector3& vRootPosition, float& heading, float& pitch, float& lookDownLimit );
	void			PushCameraForwardForHeadAndArms(const CPed* attachPed, Vector3& cameraPosition, const Vector3& finalPosition,
													const Matrix34& rootMtx, const Matrix34& mtxPlayerHead, float& pitch, bool bAffectPitch);

	bool			ComputeUseHeadSpineBoneOrientation(const CPed* attachPed, bool bIsFalling);

	void			ComputeInitialVehicleCameraFov(float& vehicleFov) const;
	float			ComputeInitialVehicleCameraRoll(const Matrix34& seatMatrix) const;
	bool			ComputeInitialVehicleCameraPosition(const CPed* attachPed, const Matrix34& seatMatrix, Vector3& vPosition) const;
	bool			ComputeVehicleAttachMatrix(const CPed* attachPed, const CVehicle* attachVehicle, const camCinematicMountedCameraMetadata& vehicleCameraMetadata,
												s32 attachSeatIndex, const Matrix34& seatMatrix, Matrix34& attachMatrix) const;
	bool			ComputeShouldAdjustForRollCage(const CVehicle* attachVehicle) const;
	bool			GetSeatSpecificCameraOffset(const CVehicle* attachVehicle, Vector3& cameraOffset) const;

	float			ComputeMotionBlurStrengthForMovement(const CPed* followPed);
	float			ComputeMotionBlurStrengthForDamage(const CPed* followPed);
	float			ComputeMotionBlurStrengthForLookBehind();

	void			TransformAttachPositionToCameraSpace(const Vector3& relativeAttachPosition, Vector3& attachPosition,
														 bool bMakeCameraMatrixUpright, const Matrix34* pCameraMatrix=NULL);
	void	UpdateLockOn();
	
	void	UpdateStickyAim();
	void	ModifyLookAroundOffsetForStickyAim(float heading, float pitch, float &lookAroundHeadingOffset, float &lookAroundPitchOffset);
	bool	ComputeStickyTarget(const CPed*& stickyTarget, Vector3& stickyPosition);
	bool	ComputeUseStickyAiming() const;
	void	UpdateStickyAimEnvelopes();
	bool	ComputeIsProspectiveStickyTargetAndPositionValid(const CPed *pFollowPed, const CPed *pStickyPed, const Vector3& stickyPos) const;

	bool	ComputeLockOnTarget(const CEntity*& lockOnTarget, Vector3& lockOnTargetPosition, Vector3& fineAimOffset) const;
	bool	ComputeIsProspectiveLockOnTargetPositionValid(const Vector3& targetPosition) const;
	bool	ComputeShouldUseLockOnAiming() const;
	void	UpdateLockOnEnvelopes();
	void	UpdateLockOnTargetBlend(const Vector3& desiredLockOnTargetPosition);

	// Helper functions
	bool	GetBoneHeadingAndPitch(const CPed* pPed, u32 boneTag, Matrix34& boneMatrix, float& heading, float& pitch, float fTolerance=0.075f);
	float	CalculateHeading(const Vector3& vecFront, const Vector3& vecUp, float fTolerance=0.075f) const;
	float	CalculatePitch(const Vector3& vecFront, const Vector3& vecUp, float fTolerance=0.075f) const;
	bool	ProcessLimitHeadingOrientation(const CPed& rPed, bool bInCover, bool bExitingCover);
	bool	ProcessLimitPitchOrientation(const CPed& rPed, bool bInCover);
	float	GetRelativeAttachPositionSmoothRate(const CPed& rPed) const;
	bool	IsFollowPedSittingInTheLeftOfTheVehicle(const CPed* attachPed, const CVehicle* attachVehicle, const CVehicleModelInfo* vehicleModelInfo, s32 attachSeatIndex) const;
	void	GetFirstPersonNonScopeOffsets(const CPed* pPed, const CWeaponInfo* pWeaponInfo, Vector3& nonScopeOffset, Vector3& nonScopeRotOffset) const;
	void	ComputeGroundNormal(const CPed* pPed);
	bool	GetEntrySide(const CPed* attachPed, const CVehicle* pVehicle, s32 seatIndex, bool& bIsRightSideEntry);

	void	ReorientCameraToPreviousAimDirection();

	bool	IsTableGamesOrIsInterpolatingToTableGamesCamera() const;
	void	UpdateBlendedAnimatedFov();
	void	UpdateBaseFov();
	float	ComputeFovSlider() const;

	void	UpdateAnimCameraBlendLevel(const CPed* currentMeleeTarget);

	bool	ShouldKillAnimatedCameraBlendIn() const;

	const camFirstPersonShooterCameraMetadata& m_Metadata;

#if KEYBOARD_MOUSE_SUPPORT
	Matrix34 m_matPredictedOrientation;
#endif
	Vec3V m_PreviousRelativeCameraPosition;
	Vec3V m_AnimatedRelativeAttachOffset;
	QuatV m_AnimatedOrientation;
	Vector3 m_PreviousCameraPosition;
	Vector3 m_PreviousRelativeAttachPosition;
	Vector3 m_PovCameraPosition;
	Vector3 m_RelativeHeadOffset;
	Vector3 m_RelativeCoverOffset;
	Vector3 m_CoverPeekOffset;
	Vector3 m_ObjectSpacePosition;
	Vector3 m_RelativeFallbackPosition;
	Vector3 m_GroundNormal;
	Vector3 m_CurrentScopeOffset;
	Vector3 m_CurrentScopeRotationOffset;
	Vector3 m_CurrentNonScopeOffset;
	Vector3 m_CurrentNonScopeRotationOffset;
	Vector3 m_CurrentThirdPersonOffset;
	Vector3 m_MeleeTargetBoneRelativeOffset;
	Vector3 m_LookDownRelativeOffset;
	Vector2 m_AnimatedCamSpringHeadingLimitsRadians;
	Vector2 m_AnimatedCamSpringPitchLimitsRadians;
	Vector2 m_CurrentCamSpringHeadingLimitsRadians;
	Vector2 m_CurrentCamSpringPitchLimitsRadians;
	Vector2 m_DesiredAnimatedCamSpringHeadingLimitsRadians;
	Vector2 m_DesiredAnimatedCamSpringPitchLimitsRadians;
	camControlHelper* m_CurrentControlHelper;
	camControlHelper* m_AimControlHelper;
	camEnvelope* m_LookBehindEnvelope;
	camHintHelper* m_HintHelper;
	RegdPed	m_pMeleeTarget;
	RegdConstPed m_pJackedPed;
	RegdConstVeh m_pVehicle;
	const camCinematicMountedCameraMetadata* m_PovVehicleCameraMetadata;
	camDampedSpring	m_RelativeHeadingSpring;
	camDampedSpring	m_RelativePitchSpring;
	camDampedSpring	m_RollSpring;
	camDampedSpring	m_MeleeRelativeHeadingSpring;
	camDampedSpring	m_MeleeRelativePitchSpring;
	camDampedSpring	m_DofAimingBlendSpring;
	camDampedSpring	m_DofMobileBlendSpring;
	camDampedSpring	m_DofHighCoverBlendSpring;
	camDampedSpring	m_DofParachuteBlendSpring;
	camDampedSpring	m_AttachPedPelvisOffsetSpring;
	camDampedSpring m_RelativeAttachHeadingSpring;
	camDampedSpring m_RelativeAttachPitchSpring;
	camDampedSpring m_RelativeAttachRollSpring;
	camDampedSpring m_RelativeAttachBlendLevelSpring;
	camDampedSpring m_FreeAimFocusDistanceSpring;
	camDampedSpring m_CollisionFallBackBlendSpring;
	camDampedSpring m_CollisionFallBackDistanceSpring;
	camDampedSpring m_VehicleEntryHeadingLimitSpring;
	camDampedSpring m_VehicleEntryPitchLimitSpring;
	camDampedSpring m_PushAheadDistanceSpring;
	camDampedSpring m_PushAheadPitchSpring;
	camDampedSpring m_EnterVehicleLookOffsetSpring;
	camDampedSpring m_VehicleSeatBlendHeadingSpring;
	camDampedSpring m_VehicleSeatBlendPitchSpring;
	camDampedSpring m_VehicleHeadingSpring;
	camDampedSpring m_VehiclePitchSpring;
	camDampedSpring m_VehicleCarJackHeadingSpring;
	camDampedSpring m_VehicleCarJackPitchSpring;
	camDampedSpring m_IdleRelativeHeadingSpring;
	camDampedSpring m_IdleRelativePitchSpring;
	camDampedSpring m_EntryExitRelativeHeadingToSpring;
	float m_CameraHeading;
	float m_CameraPitch;
	float m_SavedHeading;
	float m_SavedPitch;
	float m_PreviousAnimatedHeading;
	float m_PreviousAnimatedPitch;
	float m_ExitVehicleHeading;
	float m_ExitVehiclePitch;
	float m_ExitVehicleRoll;
	float m_AttachParentHeading;
	float m_AttachParentPitch;
	float m_DesiredFov;
	float m_AnimatedFov;
	float m_AnimatedFovOnPreviousFrame;
	float m_BlendedAnimatedFov;
	float m_AnimatedFovForBlending;
	float m_AnimatedWeight;
	float m_GroundAvoidLookDirection;
	float m_GroundAvoidCurrentAngle;
	float m_ScopeFov;
	float m_AimFovMin;
	float m_AimFovMax;
	float m_RelativeHeading;
	float m_RelativePitch;
	float m_PreviousRelativeHeading;
	float m_PreviousRelativePitch;
	float m_MeleeRelativeHeading;
	float m_MeleeRelativePitch;
	float m_SavedRelativeHeading;
	float m_SavedRelativePitch;
	float m_IdleHeading;
	float m_IdlePitch;
	float m_IdleRelativeHeading;
	float m_IdleRelativePitch;
	float m_MotionBlurPreviousAttachParentHealth;
	float m_MotionBlurPeakAttachParentDamage;
	float m_RelativePitchOffset;
	float m_ShoesOffset;
	float m_PreviousCameraFov;
	float m_VehSeatInterp;
	float m_VehicleSeatBlendDistance;
	float m_ExitInterpRate;
	float m_fExtraLookBehindPitchLimitDegrees;
	float m_ScopeOffsetSquareMagnitude;
	float m_AnimatedCameraBlendLevel;
	float m_AnimatedCameraHeadingAccumulator;
	float m_AnimatedCameraPitchAccumulator;
	float m_VehicleEntryExitPushAheadOffset;
	float m_fCustomPitchBlendingAccumulator;
	float m_BlendFovDelta;
	float m_PovCameraNearClip;
	float m_VehicleEntrySpringBlend;
	float m_StealthOffsetBlendingAccumulator;
	float m_CoverHeadingLastAngleOffset;
	float m_AnimatedBlendOutPhaseStartLevelPitch;
	float m_RelativePeekRoll;
	float m_EntryHeadingLimitScale;
	float m_CoverVaultStartHeading;
	float m_AccumulatedInputTimeWhileSprinting;
	float m_BaseFov;
	s32   m_TaskPlayerState;
	s32   m_VehicleSeatIndex;
	s32   m_VehicleTargetSeatIndex;
	s32   m_VehicleInitialSeatIndex;
	s32   m_AnimBlendInDuration;
	s32   m_AnimBlendOutDuration;
	u32   m_StopTimeOfProneMeleeAttackLook;
	u32   m_StartTakeOutPhoneTime;
	u32   m_StartPutAwayPhoneTime;
	u32   m_HeadSpineBlendInTime;
	u32   m_HeadSpineRagdollBlendTime;
	u32   m_AnimBlendInStartTime;
	u32   m_AnimBlendOutStartTime;
	u32   m_MotionBlurForAttachParentDamageStartTime;
	u32   m_DeathShakeHash;
	u32   m_CoverHeadingCorrectIdleInputTime;
	u32	  m_ExtraVehicleEntryDelayStartTime;
	u32   m_BlendoutHeadOffsetForCarJackStart;
	u32   m_BlendFovDeltaStartTime;
	u32   m_BlendFovDeltaDuration;
	u32   m_CarJackStartTime;
	u32   m_AnimatedFovBlendInStartTime;
	u32   m_AnimatedFovBlendOutStartTime;
#if KEYBOARD_MOUSE_SUPPORT
	u32   m_LastMouseInputTime;
#endif
	eCoverHeadingCorrectionType m_CoverHeadingCorrectionState;
	eCoverHeadingCorrectionType m_CoverHeadingCorrectionPreviousState;

	RegdConstPed	m_StickyAimTarget;
	RegdConstPed	m_PotentialStickyAimTarget;
	Vector3			m_StickyAimTargetPosition;
	camEnvelope*	m_StickyAimEnvelope;
	float			m_StickyAimEnvelopeLevel;
	float			m_StickyAimHeadingRate;
	u32				m_TimeSetStickyAimTarget;
	float			m_fTimePushingLookAroundHeading;
	float			m_fTimePushingLookAroundPitch;

	RegdConstEnt	m_LockOnTarget;
	Vector3			m_LockOnTargetPosition;
	Vector3			m_LockOnTargetPositionLocal;
	Vector3			m_LockOnFineAimOffset;
	camEnvelope*	m_LockOnEnvelope;
	u32				m_LockOnTargetBlendStartTime;
	u32				m_LockOnTargetBlendDuration;
	float			m_LockOnEnvelopeLevel;
	float			m_LockOnTargetBlendLevelOnPreviousUpdate;
	float			m_CachedLockOnFineAimLookAroundSpeed;
	float			m_SprintBreakOutHeading;
	float			m_SprintBreakOutHoldAtEndTime;
	float			m_SprintBreakOutTotalTime;
	u32				m_SprintBreakOutType;

	bool  m_AllowStrafe							: 1;
	bool  m_AttachedIsUnarmed					: 1;
	bool  m_AttachedIsUsingMelee				: 1;
	bool  m_AttachedIsRNGOnlyWeapon				: 1;
	bool  m_AttachedIsThrownSpecialWeapon		: 1;
	bool  m_bSteerCameraWithLeftStick			: 1;
	bool  m_WasClimbingLadder					: 1;
	bool  m_WasClimbing							: 1;

	bool  m_bUsingMobile						: 1;
	bool  m_WasUsingMobile						: 1;
	bool  m_bMobileAtEar						: 1;
	bool  m_WasDoingMelee						: 1;
	bool  m_bWasFalling							: 1;
	bool  m_bWasUsingHeadSpineOrientation		: 1;
	bool  m_WasInCover							: 1;
	bool  m_CoverVaultWasFacingLeft				: 1;
	bool  m_CoverVaultWasFacingRight			: 1;
	bool  m_bHaveMovedCamDuringCoverEntry		: 1;

	bool  m_IsAiming							: 1;
	bool  m_ShowReticle							: 1;
	bool  m_IsLockedOn							: 1;
	bool  m_HasStickyTarget						: 1;
	bool  m_ShouldBlendLockOnTargetPosition		: 1;
	bool  m_WeaponBlocked						: 1;
	bool  m_WeaponReloading						: 1;
	bool  m_bUseAnimatedFirstPersonCamera		: 1;
	bool  m_bWasUsingAnimatedFirstPersonCamera	: 1;

	bool  m_BlendOrientation					: 1;
	bool  m_WasCarSlide							: 1;
	bool  m_WasDropdown							: 1;
	bool  m_bWasCustomSituation					: 1;
	bool  m_LookAroundDuringHeadSpineOrient		: 1;
	bool  m_LookDownDuringHeadSpineOrient		: 1;
	bool  m_bWasUsingRagdollForHeadSpine		: 1;
	bool  m_bIgnoreHeadingLimits				: 1;

	bool  m_bLookingBehind						: 1;
	bool  m_bWasLookingBehind					: 1;
	bool  m_bLookBehindCancelledForAiming		: 1;
	bool  m_ShouldRotateRightForLookBehind		: 1;
	bool  m_bWaitForLookBehindUp				: 1;
	bool  m_bDoingLookBehindTransition			: 1;

	bool  m_EnteringVehicle						: 1;
	bool  m_ExitingVehicle						: 1;
	bool  m_JackedFromVehicle					: 1;
	bool  m_AllowLookAtVehicleJacker			: 1;
	bool  m_IsCarJacking						: 1;
	bool  m_IsCarJackingIgnoreThreshold			: 1;
	bool  m_DidSeatShuffle						: 1;
	bool  m_IsSeatShuffling						: 1;
	bool  m_EnterGoToDoor						: 1;
	bool  m_EnteringSeat						: 1;
	bool  m_EnterVehicleAlign					: 1;
	bool  m_EnterOpenDoor						: 1;
	bool  m_EnterPickUpBike						: 1;
	bool  m_EnterJackingPed						: 1;
	bool  m_WasEnteringVehicle					: 1;
	bool  m_ClosingDoorFromOutside				: 1;
	bool  m_PovClonedDataValid					: 1;
	bool  m_CarJackVictimCrossedThreshold		: 1;
	bool  m_VehicleHangingOn					: 1;
	bool  m_bEnteringTurretSeat					: 1;
	bool  m_bWasHotwiring						: 1;
	bool  m_BlendFromHotwiring					: 1;
	bool  m_VehicleDistanceReset				: 1;
	bool  m_IsABikeOrJetski						: 1;
	bool  m_ClampVehicleEntryHeading			: 1;
	bool  m_ClampVehicleEntryPitch				: 1;
	bool  m_ResetEntryExitSprings				: 1;

	bool  m_IsScopeAiming						: 1;
	bool  m_StartedAimingWithInputThisUpdate	: 1;
	bool  m_bParachuting						: 1;
	bool  m_bParachutingOpenedChute				: 1;
	bool  m_bWasParachuting						: 1;
	bool  m_bDiving								: 1;
	bool  m_bWasDiving							: 1;
	bool  m_bPlantingBomb						: 1;
	bool  m_bWasPlantingBomb					: 1;
	bool  m_bAnimatedCameraBlendInComplete		: 1;

	bool  m_bUsingHeadAttachOffsetInVehicle		: 1;
	bool  m_bEnterBlending						: 1;
	bool  m_bEnterBlendComplete					: 1;
	bool  m_MovingUpLadder						: 1;
	bool  m_RunningAnimTask						: 1;
	bool  m_WasRunningAnimTaskWithAnimatedCamera: 1;
	bool  m_AnimTaskUseHeadSpine				: 1;
	bool  m_TaskControllingMovement				: 1;
	bool  m_PlayerGettingUp						: 1;

	bool  m_UpdateAnimCamDuringBlendout			: 1;
	bool  m_AnimatedCameraBlockTag				: 1;
	bool  m_ActualAnimatedYawSpringInput		: 1;
	bool  m_ActualAnimatedPitchSpringInput		: 1;
	bool  m_ActualAnimatedYawSpringInputChanged	: 1;
	bool  m_AnimatedYawSpringInput				: 1;
	bool  m_AnimatedPitchSpringInput			: 1;
	bool  m_AnimatedCameraStayBlocked			: 1;
	bool  m_TreatNMTasksAsTakingFullDamage		: 1;
	bool  m_TakingOffHelmet						: 1;

	bool  m_MeleeAttack							: 1;
	bool  m_DoingStealthKill					: 1;
	bool  m_WasStealthKill						: 1;
	bool  m_SlopeScrambling						: 1;
	bool  m_CombatRolling						: 1;
	bool  m_WasCombatRolling					: 1;
	bool  m_bHighFalling						: 1;
	bool  m_TaskFallFalling						: 1;
	bool  m_TaskFallLandingRoll					: 1;
	bool  m_TaskFallParachute					: 1;
	bool  m_ForceLevelPitchOnAnimatedCamExit	: 1;

	bool  m_SprintBreakOutTriggered				: 1;
	bool  m_SprintBreakOutFinished				: 1;

	bool  m_IsIdleCameraActive					: 1;

    bool  m_ScriptPreventingOrientationResetWhenRenderingAnotherCamera : 1;

#if RSG_PC
	bool  m_TripleHead							: 1;
#endif
#if KEYBOARD_MOUSE_SUPPORT
	bool  m_PredictedOrientationValid			: 1;
#endif

	bool  m_CoverHeadingCorrectionDetectedStickInput : 1;

	bool  m_SwitchedWeaponWithWeaponWheelUp : 1;

	bool  m_CameraRelativeAnimatedCameraInput	: 1;

	// True when the camera should go into fallback mode
	// because there is a chance we might clip through an
	// object. It's just really the result of the last 
	// probe
	bool  m_bShouldGoIntoFallbackMode			: 1;

	bool  m_bStealthTransition					: 1;

	// Forbid copy construction and assignment.
	camFirstPersonShooterCamera(const camFirstPersonShooterCamera& other);
	camFirstPersonShooterCamera& operator=(const camFirstPersonShooterCamera& other);
};

#endif // FPS_MODE_SUPPORTED
#endif // FIRST_PERSON_SHOOTER_CAMERA_H
