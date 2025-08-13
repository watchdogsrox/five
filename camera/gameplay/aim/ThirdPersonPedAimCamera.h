//
// camera/gameplay/aim/ThirdPersonPedAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_PED_AIM_CAMERA_H
#define THIRD_PERSON_PED_AIM_CAMERA_H

#include "camera/gameplay/aim/ThirdPersonAimCamera.h"

class camThirdPersonPedAimCameraMetadata;

class camThirdPersonPedAimCamera : public camThirdPersonAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonPedAimCamera, camThirdPersonAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camThirdPersonPedAimCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camThirdPersonPedAimCamera();

	virtual bool	IsInAccurateMode() const;
	virtual float	ComputeExtraZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const;
    virtual void	UpdateFrameShakers(camFrame& frameToShake, float amplitudeScalingFactor = 1.0f);
    virtual float   GetFrameShakerScaleFactor() const;

	virtual bool	GetIsTurretCam() const { return m_IsTurretCam; }
	inline void		SetIsTurretCam(bool isTurretCam) { m_IsTurretCam = isTurretCam; }
	float			GetOrbitHeadingLimitsRatio() const { return m_OrbitHeadingLimitsRatio; }

	void			CacheCinematicCameraOrientation(const camFrame& cinematicCamFrame);
	void			ForcePreviousViewMode(s32 mode) { m_previousViewMode = mode; }
	bool			ShouldPreventBlendToItself() const;
#if __BANK
	static void		AddWidgets(bkBank& bank);
#endif // __BANK

protected:
	virtual bool	Update();
	virtual float	ComputeIdealOrbitHeadingForLimiting() const;
	void			UpdateCustomOffsetScaling();
	void			UpdateCustomRollOffset();
	virtual bool	ComputeShouldForceAttachPedPelvisOffsetToBeConsidered() const;
	virtual void	UpdateBaseAttachPosition();
	virtual float	ComputeAttachPedPelvisOffsetForShoes() const;
	virtual void	UpdateOrbitPitchLimits();
	virtual void	UpdateRelativeOrbitHeadingLimits();
	virtual void	ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const;
	virtual float	UpdateOrbitDistance();
#if __BANK
	virtual float	ComputeBasePivotHeightScalingForFootRoom() const;
#endif // __BANK
	virtual void	ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const;
	virtual void	UpdateLockOn();
	virtual void	UpdateLockOnEnvelopes(const CEntity* desiredLockOnTarget);
	bool			ComputeIsPedStunned(const CPed& ped) const;
	virtual void	UpdateLockOnTargetDamping(bool hasChangedLockOnTargetThisUpdate, Vector3& desiredLockOnTargetPosition);
	virtual float	ComputeDesiredZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const;
	virtual bool	ComputeShouldDisplayReticuleDuringInterpolation(const camFrameInterpolator& frameInterpolator) const;
	bool			GetTurretMatrixForVehicleSeat(const CVehicle& vehicle, const s32 seatIndex, Matrix34& turretMatrixOut) const;
	virtual void	UpdateDof();
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;
	virtual bool	ReorientCameraToPreviousAimDirection();

	const camThirdPersonPedAimCameraMetadata& m_Metadata;

	Vector3			m_PreviousCinematicCameraForward;
	Vector3			m_PreviousCinematicCameraPosition;
	camDampedSpring	m_CustomSideOffsetScalingSpring;
	camDampedSpring	m_CustomVertOffsetScalingSpring;
	camDampedSpring m_CustomRollOffsetSpring;
	camDampedSpring m_CustomOrbitDistanceScalingSpring;
	camDampedSpring	m_LockOnOrbitDistanceScalingSpring;
	camDampedSpring	m_LockOnTargetStunnedHeadingSpring;
	camDampedSpring	m_LockOnTargetStunnedPitchSpring;
	camDampedSpring	m_LockOnTargetStunnedDistanceSpring;
	camEnvelope*	m_LockOnTargetStunnedEnvelope;
	float			m_LockOnTargetStunnedEnvelopeLevel;
	s32				m_CachedSeatIndexForTurret;
	float			m_OrbitHeadingLimitsRatio;
	s32				m_previousViewMode;
	bool			m_HasAttachPedFiredEarly;
	bool			m_IsTurretCam;	

#if __BANK
	static float	ms_DebugFullShotScreenRatio;
	static float	ms_DebugOverriddenOrbitDistance;
	static bool		ms_ShouldDebugForceFullShot;
	static bool		ms_ShouldDebugOverrideOrbitDistance;
#endif // __BANK

	//Forbid copy construction and assignment.
	camThirdPersonPedAimCamera(const camThirdPersonPedAimCamera& other);
	camThirdPersonPedAimCamera& operator=(const camThirdPersonPedAimCamera& other);
};

#endif // THIRD_PERSON_PED_AIM_CAMERA_H
