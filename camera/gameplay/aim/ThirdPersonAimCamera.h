//
// camera/gameplay/aim/ThirdPersonAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_AIM_CAMERA_H
#define THIRD_PERSON_AIM_CAMERA_H

#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/helpers/DampedSpring.h"

class camThirdPersonAimCameraMetadata;
class camStickyAimHelper;

class camThirdPersonAimCamera : public camThirdPersonCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonAimCamera, camThirdPersonCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camThirdPersonAimCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camThirdPersonAimCamera();

	virtual bool	IsInAccurateMode() const		{ return false; }
	const CEntity*	GetLockOnTarget() const			{ return m_LockOnTarget; }
	bool			ShouldDisplayReticule() const	{ return m_ShouldDisplayReticule; }
	virtual float	GetRecoilShakeAmplitudeScaling() const;

	virtual void	ComputeBaseLookAtFront(const Vector3& collisionRootPosition, const Vector3& basePivotPosition, const Vector3& baseFront,
						const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition, const Vector3& postCollisionCameraPosition,
						float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const;

	virtual float	ComputeExtraZoomFactorForAccurateMode(const CWeaponInfo& UNUSED_PARAM(weaponInfo)) const {return 1.0f;}
	
	void			SetNearClipThisUpdate(float nearClip)			{ m_OverriddenNearClip = nearClip;  m_ShouldOverrideNearClipThisUpdate = true;}

	camStickyAimHelper* GetStickyAimHelper() { return m_StickyAimHelper; }

#if __BANK
	virtual void	DebugRender();
#endif // __BANK

protected:
	virtual bool	Update();
	virtual void	FlagCutsForOrbitOverrides();
	virtual void	UpdateLockOn();
	virtual bool	ComputeLockOnTarget(const CEntity*& lockOnTarget, Vector3& lockOnTargetPosition, Vector3& fineAimOffset) const;
	bool			ComputeIsProspectiveLockOnTargetPositionValid(const Vector3& targetPosition) const;
	virtual bool	ComputeShouldUseLockOnAiming() const;
	virtual void	UpdateLockOnEnvelopes(const CEntity* desiredLockOnTarget);
	void			UpdateLockOnTargetBlend(const Vector3& desiredLockOnTargetPosition);
	virtual void	UpdateLockOnTargetDamping(bool UNUSED_PARAM(hasChangedLockOnTargetThisUpdate), Vector3& UNUSED_PARAM(desiredLockOnTargetPosition)) {}
	virtual float	ComputeBaseFov() const;
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch);
	virtual void	ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const;
	virtual void	ApplyOverriddenOrbitOrientation(float& orbitHeading, float& orbitPitch);
	virtual float	ComputeDesiredFov();
	virtual float	ComputeDesiredZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const;
	virtual void	UpdateReticuleSettings();
	virtual bool	ComputeShouldDisplayReticuleDuringInterpolation(const camFrameInterpolator& frameInterpolator) const;
	virtual bool	ComputeShouldDisplayReticuleAsInterpolationSource() const { return false; }
	virtual void	ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	const camThirdPersonAimCameraMetadata& m_Metadata;

	camDampedSpring	m_FineAimBlendSpring;
	camDampedSpring	m_WeaponZoomFactorSpring;
	RegdConstEnt	m_LockOnTarget;
	RegdConstWeaponInfo	m_WeaponInfo;
	Vector3			m_LockOnTargetPosition;
	Vector3			m_CachedRelativeLockOnTargetPosition;
	Vector3			m_LockOnFineAimOffset;
	camEnvelope*	m_LockOnEnvelope;
	u32				m_LockOnTargetBlendStartTime;
	u32				m_LockOnTargetBlendDuration;
	float			m_LockOnEnvelopeLevel;
	float			m_LockOnTargetBlendLevelOnPreviousUpdate;
	float			m_CachedLockOnFineAimLookAroundSpeed;
	float			m_OverriddenNearClip; 
	bool			m_IsLockedOn								: 1;
	bool			m_ShouldBlendLockOnTargetPosition			: 1;
	bool			m_ShouldDisplayReticule						: 1;
	bool			m_ShouldOverrideNearClipThisUpdate			: 1;

	camStickyAimHelper* m_StickyAimHelper;

private:
	//Forbid copy construction and assignment.
	camThirdPersonAimCamera(const camThirdPersonAimCamera& other);
	camThirdPersonAimCamera& operator=(const camThirdPersonAimCamera& other);
};

#endif // THIRD_PERSON_AIM_CAMERA_H
