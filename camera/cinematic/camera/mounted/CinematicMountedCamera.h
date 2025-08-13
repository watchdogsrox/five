//
// camera/cinematic/CinematicMountedCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_MOUNTED_CAMERA_H
#define CINEMATIC_MOUNTED_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/system/CameraMetadata.h"
#include "physics/WorldProbe/worldprobe.h"

class camCinematicMountedCameraMetadata;
class camControlHelper;
class camEnvelope;
class camSpringMount;
class camLookAtDampingHelper; 
class camLookAheadHelper;
class CFirstPersonDriveByLookAroundData;

//A cinematic camera that can be mounted at several offset positions on a target.
class camCinematicMountedCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicMountedCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCinematicMountedCamera(const camBaseObjectMetadata& metadata);

public:
	~camCinematicMountedCamera();

	virtual const Vector3& GetBaseFront() const		{ return m_BaseFront; }

	bool			IsValid(); 
	bool			IsCameraInsideVehicle() const;
	bool			IsCameraForcingMotionBlur() const;	
	bool			ShouldDisplayReticule() const;
	const CPed*		GetPedToMakeInvisible() const { return m_PedToMakeInvisible; }
	const CPed*		GetPedToMakeHeadInvisible() const { return m_PedToMakeHeadInvisible; }
	void			ShouldDisableControlHelper(bool disable) { m_DisableControlHelper = disable; }
	void			SetLookAtBehaviour(eCinematicMountedCameraLookAtBehaviour lookAtBehaviour) 
					{ m_OverriddenLookAtBehaviour = lookAtBehaviour; m_ShouldOverrideLookAtBehaviour = true; }
	void			SetShouldLimitAndTerminateForPitchAndHeadingLimits(bool ShouldLimitHeadingAndPitch, bool ShouldTerminateForHeadingAndPitch,
																		const Vector2& InitialPitchLimits, const Vector2& InitialHeadingLimits,
																		const Vector2& RelativeParentPitchLimits, const Vector2& RelativeParentHeadingLimits); 
	void			SetUseScriptHeading(float Heading) { m_UseScriptRelativeHeading = true; m_CachedScriptHeading = Heading; }
    void			SetUseScriptPitch(float Pitch) { m_UseScriptRelativePitch = true; m_CachedScriptPitch = Pitch; }
	void			SetBoneRelativeAttachPosition(const Vector3& BoneOffset) { m_BoneRelativeAttachPosition = BoneOffset; }
	void			SetFov(float fov) { m_Fov = fov;} 
	void			OverrideVehicleAttachPart(eCinematicVehicleAttach CinematicAttachPart) { m_AttachPart = CinematicAttachPart; } 
	void			OverrideLookAtDampingHelpers(atHashString InVehicleLookAtDamper, atHashString OnFootLookAtDamper); 		
	void			SetCanUseLookAtDampingHelpers(bool UseDampingHelpers) { m_CanUseDampedLookAtHelpers = UseDampingHelpers; }
	void			SetCanUseLookAheadHelper(bool UseLookAheadHelper) { m_CanUseLookAheadHelper = UseLookAheadHelper; }

	void			SetShouldTerminateForOcclusion(bool TerminateForOcclusion) { m_shouldTerminateForOcclusion = TerminateForOcclusion; }
	void			SetShouldTerminateForDistance(bool TerminateForDistance) { m_shouldTerminateForDistanceToTarget = TerminateForDistance; }

	void			SetShouldAdjustRelativeAttachPositionForRollCage(bool shouldAdjustForRollCage) { m_ShouldAdjustRelativeAttachPositionForRollCage = shouldAdjustForRollCage; }

	void			SetSeatSpecificCameraOffset(const Vector3& cameraOffset) { m_SeatSpecificCameraOffset = cameraOffset; }

	bool			ShouldShakeOnImpact() const;
	
	bool			ShouldLookBehindInGameplayCamera() const;

	bool			ShouldAffectAiming() const;

	bool			IsFirstPersonCamera() const { return m_Metadata.m_FirstPersonCamera; }

	bool			IsVehicleTurretCamera() const { return m_AttachedToTurret && m_Metadata.m_ShouldAttachToVehicleTurret; }

	void			StartCustomFovBlend(float fovDelta, u32 duration);

#if FPS_MODE_SUPPORTED
	bool			WasUsingAnimatedData() const { return m_WasUsingFirstPersonAnimatedData; }
	void			SetWasUsingAnimatedData()	 { m_WasUsingFirstPersonAnimatedData = true; }
#else
	bool			WasUsingAnimatedData() const { return false; }
#endif

	bool			IsLookingLeft() const;

	const camControlHelper* GetControlHelper() const;

	void  ResetAllowSpringCutOnFirstUpdate()				{ m_bAllowSpringCutOnFirstUpdate = false; }
	const camDampedSpring&  GetSeatSwitchSpring() const		{ return m_SeatSwitchingLimitSpring; }
	camDampedSpring&		GetSeatSwitchSpringNonConst()	{ return m_SeatSwitchingLimitSpring; }

	static float LerpRelativeHeadingBetweenPeds(const CPed *pPlayer, const CPed *pArrestingPed, float fCamHeading);
	
	static Vector2 GetSeatSpecificScriptedAnimOffset(const CVehicle* attachVehicle);
	static const CFirstPersonDriveByLookAroundData *GetFirstPersonDrivebyData(const CVehicle* attachVehicle);

#if __BANK
	static void AddWidgets();
#endif

protected:
	virtual void	PostUpdate();
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

private:
	virtual bool	Update();
	void			UpdatePedVisibility(float relativeHeading);
	bool			ComputeCanPedBeVisibleInVehicle(const CPed& ped, float relativeHeading) const;
	void			ComputeAttachMatrix(const CPhysical& attachPhysical, Matrix34& attachMatrix);
	void			ApplyDampingToAttachMatrix(Matrix34& attachMatrix);
	float			UpdateOrientation(const CPhysical& attachPhysical, const Matrix34& attachMatrix);
	void			LimitRelativeHeading(float& relativeHeading) const;
	void			LimitParentHeadingRelativeToAttachParent(float& relativeHeading) const;
	void			UpdateFov();
	void			UpdateNearClip(bool bUsingTripleHead, float aspectRatio);
	bool			ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition, bool &HasHitAttachedParent); 
	bool			UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition);
	bool			ComputeShouldTerminateForWaterClipping(const Vector3& position);
	bool			ComputeShouldTerminateForClipping(const Vector3& position, const Vector3& attachPosition);
	bool			IsClipping(const Vector3& position, const Vector3& attachPosition, u32 includeFlags, u32 excludeFlags, bool shouldExcludeBikes = false) const;
	bool			ComputeHasHitExcludingHeistCratesProp(const WorldProbe::CShapeTestResults& shapeTestResults) const;
	bool			ComputeHasHitExcludingBikes(const WorldProbe::CShapeTestResults& shapeTestResults) const;
	void			UpdateRagdollBlendLevel();
	void			UpdateRappelBlendLevel();
	void			UpdateHeadingLimitNextToTurretSeatBlendLevel();
	void			UpdateOpenHeliSideDoorsBlendLevel();
	void			UpdateSwitchHelmetVisorBlendLevel();
	void			UpdateScriptedVehAnimBlendLevel();
	void			UpdateSmashWindowBlendLevel();
	void			UpdateUnarmedDrivebyBlendLevel();
	void			UpdateVehicleMeleeBlendLevel();
	void			UpdateDuckingBlendLevel();
	void			UpdateDrivebyBlendLevel();
	void			UpdateLowriderAltAnimsBlendLevel();
	bool			ComputeIsCreatingRopeForRappel() const;
	bool			ComputeHeadingLimitNextToTurretSeat();
	bool			ComputeIsOpeningSlidingDoorFromInside() const;
	bool			ComputeIsSwitchingHelmetVisor() const;
	bool			ComputeIsScriptedVehAnimPlaying() const;
	bool			ComputeNeedsToSmashWindow() const;
	bool			ComputeIsPedDrivebyAiming(bool bCheckForUnarmed = true) const;
	bool			ComputeIsPedDoingVehicleMelee() const;
	bool			ComputeIsPedDucking() const;
	bool			ComputeIsPedUsingLowriderAltAnims() const;
	void			UpdateSeatShufflingBlendLevel();
	void			UpdateAccelerationMovement();
	void			CreateDampedLookAtHelpers(atHashString InVehicleLookAtDamperRef, atHashString OnFootLookAtDamperRef); 
	bool			IsFollowPedSittingInTheLeftOfTheVehicle(const CVehicle* attachVehicle, const CPed* followPed) const;  
	bool			IsFollowPedSittingInVehicleFacingForward(const CVehicle* attachVehicle, const CPed* followPed) const;
	bool			ShouldAllowRearSeatLookback();
	virtual void	UpdateDof();
	void			UpdateShakeAmplitudeScaling();
	void			UpdateIdleShake();
	void			UpdateSpeedRelativeShake(const CPhysical& parentPhysical, const camSpeedRelativeShakeSettingsMetadata& settings, camDampedSpring& speedSpring);
	void			UpdateRocketSettings();

	void			UpdateCustomFovBlend(float fCurrentFov);
	void 			BlendMatrix(float fInterp, const Vector3& vFinalPosition, const Quaternion& qFinalOrientation, float fFinalFov, float fFinalNearClip, bool bUseAnimatedCameraFov);
	void			UpdateFirstPersonAnimatedData();

	bool			CanResetLook() const;
	bool			CanLookBehind() const;

	bool			ComputeIdealLookAtPosition(const Matrix34& attachMatrix, Vector3& lookAtPosition) const;
	bool			RestoreCameraOrientation(const Matrix34& attachMatrix, float& relativeHeading, float& relativePitch) const;
	bool			IsAttachVehicleOnTheGround(const CVehicle& attachVehicle) const;

	const camCinematicMountedCameraMetadata& m_Metadata;

	camDampedSpring m_AttachMatrixPitchSpring;
	camDampedSpring m_AttachMatrixRollSpring;
	camDampedSpring m_AttachMatrixHeadingSpring;
	camDampedSpring m_RelativeAttachPositionSprings[3];
	camDampedSpring	m_RelativeHeadingSpring;
	camDampedSpring	m_RelativePitchSpring;
	camDampedSpring m_LeadingLookHeadingSpring;
	camDampedSpring m_HighSpeedShakeSpring;
	camDampedSpring m_HighSpeedVibrationShakeSpring;
	camDampedSpring m_AccelerationMovementSpring;
	camDampedSpring m_SeatSwitchingLimitSpring;
	camDampedSpring m_RappellingBlendLevelSpring;
	camDampedSpring m_HeadingLimitNextToTurretSeatBlendLevelSpring;
	camDampedSpring m_OpenHeliSideDoorBlendLevelSpring;
	camDampedSpring m_VehMeleeBlendLevelSpring;
	camDampedSpring m_SwitchHelmetVisorBlendLevelSpring;
	camDampedSpring m_ScriptedVehAnimBlendLevelSpring;
	camDampedSpring m_SideOffsetSpring;
	camDampedSpring m_MiniTankJumpingSpring;
	RegdConstPed m_PedToMakeInvisible;
	RegdConstPed m_PedToMakeHeadInvisible;
	camControlHelper* m_ControlHelper;
	camControlHelper* m_MobilePhoneCameraControlHelper;
	camEnvelope* m_RelativeAttachSpringConstantEnvelope;
	camEnvelope* m_RagdollBlendEnvelope;
	camEnvelope* m_UnarmedDrivebyBlendEnvelope;
	camEnvelope* m_DrivebyBlendEnvelope;
	camEnvelope* m_SmashWindowBlendEnvelope;
	camEnvelope* m_DuckingBlendEnvelope;
	camEnvelope* m_LookBehindEnvelope;
	camEnvelope* m_ResetLookEnvelope;
	camEnvelope* m_LowriderAltAnimsBlendEnvelope;
	camLookAtDampingHelper* m_InVehicleLookAtDamper; 
	camLookAtDampingHelper* m_OnFootLookAtDamper;
	camLookAheadHelper* m_InVehicleLookAheadHelper;
	camSpringMount* m_SpringMount;
	Matrix34 m_AttachMatrix;
	Vector3 m_LocalExitEnterOffset;
	Vector3 m_BoneRelativeAttachPosition; 
	Vector3 m_SeatSpecificCameraOffset;
	Vector3 m_BaseFront;
	Vector2 m_InitialRelativePitchLimits; 
	Vector2 m_InitialRelativeHeadingLimits; 
	Vector2 m_AttachParentRelativePitchLimits; 
	Vector2 m_AttachParentRelativeHeadingLimits; 
	Vector2 m_FinalPitchLimits;
	u32 m_TimeSpentClippingIntoDynamicCollision;
	u32 m_BlendAnimatedStartTime;

	float m_LookBehindBlendLevel;
	float m_ResetLookBlendLevel;

	float m_fHeadingLimitForNextToTurretSeats;
	float m_RelativeHeading;
	float m_RelativeHeadingWithLookBehind;
	float m_CachedScriptHeading;
    float m_CachedScriptPitch;
	float m_RelativePitch;
	float m_ShotTimeSpentOccluded; 
	float m_RagdollBlendLevel; 
	float m_UnarmedDrivebyBlendLevel;
	float m_VehicleMeleeBlendLevel;
	float m_DuckingBlendLevel;
	float m_DriveByBlendLevel;
	float m_SmashWindowBlendLevel;
	float m_LowriderAltAnimsBlendLevel;
	float m_HeadBoneHeading;
	float m_HeadBonePitch;
	float m_DestSeatHeading;
	float m_DestSeatPitch;
	float m_PreviousVehicleHeading;
	float m_PreviousVehiclePitch;
	float m_DesiredSideOffset;
	Vector3 m_PitchOffset;
	Vector3 m_vPutOnHelmetOffset;
	float m_Fov; 
	float m_CamFrontRelativeAttachSpeedOnPreviousUpdate;
#if KEYBOARD_MOUSE_SUPPORT
	float m_PreviousLookAroundInputEnvelopeLevel;
#endif // KEYBOARD_MOUSE_SUPPORT
	float m_ShakeAmplitudeScalar;
	float m_BlendLookAroundTime; 
	float m_BlendFovDelta;
	u32   m_BlendFovDeltaStartTime;
	u32   m_BlendFovDeltaDuration;
	float m_BaseHeadingRadians;
	float m_DesiredRelativeHeading;
	float m_SeatShufflePhase;
	s32   m_SeatShuffleTargetSeat;
	eCinematicMountedCameraLookAtBehaviour m_OverriddenLookAtBehaviour; 
	eCinematicVehicleAttach m_AttachPart; 
	
	bool m_canUpdate : 1; 
	bool m_DisableControlHelper : 1;
	bool m_ShouldOverrideLookAtBehaviour : 1;
	bool m_ShouldLimitAttachParentRelativePitchAndHeading : 1; 
	bool m_ShouldTerminateForPitchAndHeading : 1; 
	bool m_shouldTerminateForOcclusion : 1; 
	bool m_shouldTerminateForDistanceToTarget : 1; 
	bool m_CanUseDampedLookAtHelpers : 1; 
	bool m_IsSeatPositionOnLeftHandSide : 1; 
	bool m_ShouldAdjustRelativeAttachPositionForRollCage : 1;
	bool m_CanUseLookAheadHelper : 1;
	bool m_ShouldRotateRightForLookBehind : 1;
	bool m_ShouldApplySideOffsetToLeft : 1;
	bool m_HaveCachedSideForSideOffset : 1;
	bool m_bShouldBlendRelativeAttachOffsetWhenLookingAround : 1;
	bool m_bAllowSpringCutOnFirstUpdate : 1;
	bool m_bResettingLook : 1;
	bool m_UseScriptRelativeHeading : 1;
    bool m_UseScriptRelativePitch : 1;
	bool m_bResetLookBehindForDriveby : 1;
	bool m_PreviousWasInVehicleCinematicCamera : 1;
	bool m_AttachedToTurret : 1;
	bool m_SeatShuffleChangedCameras : 1;
	bool m_IsShuffleTaskFirstUpdate : 1;

#if FPS_MODE_SUPPORTED
	bool m_UsingFirstPersonAnimatedData : 1;
	bool m_WasUsingFirstPersonAnimatedData : 1;
#endif

	//Forbid copy construction and assignment.
	camCinematicMountedCamera(const camCinematicMountedCamera& other);
	camCinematicMountedCamera& operator=(const camCinematicMountedCamera& other);
};

#endif // CINEMATIC_MOUNTED_CAMERA_H
