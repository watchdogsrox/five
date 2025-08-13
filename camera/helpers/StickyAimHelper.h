//
// camera/helpers/StickyAimHelper.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef STICKY_AIM_HELPER_H
#define STICKY_AIM_HELPER_H

#include "camera/base/BaseObject.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/helpers/Frame.h"
#include "camera/system/CameraMetadata.h"

class camEnvelope;
class camStickyAimHelperMetadata; 
class CEntity;
class CPed;

class camStickyAimHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camStickyAimHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camStickyAimHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camStickyAimHelper(const camBaseObjectMetadata& metadata);

public:
	~camStickyAimHelper();

	bool Update(const Vector3& basePivotPosition, const Vector3& baseFront, const Matrix34& worldMatrix, const bool isLockedOn, const bool isAiming = true);

	void UpdateOrientation(float& orbitHeading, float& orbitPitch, const Vector3& basePivotPosition, const Matrix34& worldMatrix);

	void PostCameraUpdate(const Vector3& basePivotPosition, const Vector3& baseFront);

	void FixupPositionOffsetsFromRagdoll(const CPed* targetPed, const Vector3& vPrevPosition, const Vector3& vNewPosition);

private:

	bool UpdateInternal(const CPed* followPed, const Vector3& basePivotPosition, const Vector3& baseFront, const Matrix34& worldMatrix, const bool isLockedOn);

	bool ShouldProcessStickyAim(const CPed* attachPed, const u32 lastStickyTargetTime);

	bool ComputeStickyAimTarget(const CPed* attachPed, const Vector3 cameraPosition, const Vector3 cameraFront, const Matrix34& mCameraWorldMat, const u32 timeSetStickyTarget);
	bool IsProspectiveStickyTargetAndPositionValid(const CPed* attachPed, const CEntity* prospectiveTarget);
	void SetStickyAimTargetInternal(const CEntity* newStickyAimTarget, const CEntity* previousStickyAimTarget, const Vector3 stickyReticlePosition, const bool selectedNewTarget);
	
	void UpdateStickyAimEnvelopes(const CPed* attachPed, const Vector3& cameraPosition, const Vector3& cameraFront);

	const CEntity*	GetCurrentStickyAimTarget() const;
	const CEntity*	GetStickyAimTarget() const { return m_StickyAimTarget; }
	const CEntity*	GetBlendOutStickyAimTarget() const { return m_BlendingOutStickyAimTarget; }

	Vector2 ProcessStickyAimRotation(
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
		, camDampedSpring& rRotationPitchModifierSpring);

	Vector2 CalculateStickyAimCameraRotationLerpModifier(
		const CPed* attachPed
		, const CEntity* targetEntity
		, const Matrix34& mCameraWorldMat
		, const bool bIsPlayerLookingOrMoving
		, camDampedSpring& rRotationHeadingModifierSpring
		, camDampedSpring& rRotationPitchModifierSpring);

	void ResetStickyAimParams(bool bResetEnvelopeAndCamera = true, bool bResetPreviousPosition = true);
	void ResetStickyAimTargetDueToFailedFindTarget();
	void ResetStickyAimRotationParams(bool bResetPreviousPosition = true);

	Vector3 TransformPositionRelativeToUprightTarget(const CEntity* stickyTarget, const Vector3 position);
	Vector3 UnTransformPositionRelativeToUprightTarget(const CEntity* stickyTarget, const Vector3 position);
	bool ShouldUsePelvisPositionForLocalPositionCaching(const CEntity* stickyTarget);

	bool CanApplyCameraRotation(const CPed* attachPed, const u32 rotationNoStickTime, const float lookAroundInput);
	bool IsPlayerLooking(const float lookAroundInput, const u32 rotationNoStickTime);
	bool IsPlayerMoving(const CPed* attachPed);

	static float NormaliseInRange(const float fIn, const float fMin, const float fMax);
	static Vector2 TransformWorldToScreen(Vector3 worldPosition);

private:
	bool m_CanUseStickyAim;

	RegdConstEnt m_StickyAimTarget;
	RegdConstEnt m_BlendingOutStickyAimTarget;
	RegdConstEnt m_StickyAimGroundEntity;

	Vector3 m_StickyReticlePosition;
	Vector3 m_PreviousReticlePosition;

	Vector2 m_StickyAimRotation;

	Vector3 m_PreviousStickyTargetTransformPosition;	// Used for warp detection.

	bool m_HasStickyTarget;
	bool m_ChangedStickyTargetThisUpdate;
	bool m_IsLockedOn;

	u32 m_TimeSetStickyTarget;
	u32 m_LastStickyTargetTime;
	u32 m_CameraRotationNoStickTime;

	float m_LookAroundInput;

	camEnvelope*	m_StickyAimEnvelope;
	float			m_StickyAimEnvelopeLevel;

	camDampedSpring	m_StickyAimCameraRotationHeadingModifierSpring;
	camDampedSpring m_StickyAimCameraRotationPitchModifierSpring;

	const camStickyAimHelperMetadata& m_Metadata;

	//Forbid copy construction and assignment.
	camStickyAimHelper(const camStickyAimHelper& other);
	camStickyAimHelper& operator=(const camStickyAimHelper& other);
};

#endif // STICKY_AIM_HELPER_H
