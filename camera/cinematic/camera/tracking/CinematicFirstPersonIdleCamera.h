//
// camera/cinematic/CinematicFirstPersonIdleCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_FIRST_PERSON_IDLE_CAMERA_H
#define CINEMATIC_FIRST_PERSON_IDLE_CAMERA_H

#include "camera/base/BaseCamera.h"

#define USE_SIMPLE_DOF				0		// Disabled, does not make much difference, if re-enabling, need to re-add metadata field.

#define FPI_MAX_TARGET_HISTORY		8

class camCinematicFirstPersonIdleCameraMetadata;
class CEntity;
class CEvent;
class CVehicle;
class CPed;

class camCinematicFirstPersonIdleCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicFirstPersonIdleCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum ShotType
	{
		WIDE_SHOT = 0,
		MEDIUM_SHOT,
		CLOSE_UP_SHOT,
		NUM_SHOT_TYPES
	};

protected:
	camCinematicFirstPersonIdleCamera(const camBaseObjectMetadata& metadata);

public:
	~camCinematicFirstPersonIdleCamera();

	bool IsValid();

	bool Update(); 

	void Init(const camFrame& frame, const CEntity* target);

	const CPed* GetPedToMakeInvisible() const;

#if __BANK
	virtual void	DebugRender();
#endif // __BANK

protected:

	const camCinematicFirstPersonIdleCameraMetadata& m_Metadata;

private:
	void			UpdateOrientation(const CPed* player, const Matrix34& matPlayer, float& fDistanceToTarget, float& fAngleToTarget);
	void			UpdateOrientationForNoTarget(const CPed* player, const Matrix34& matPlayer);
	void			InitFrame(const Matrix34& matPlayer);
	void			UpdateZoom(const CPed* player, float fDistanceToTarget, float fAngleToTarget);
	void			UpdatePlayerHeading(CPed* player);

	void			SearchForLookatTarget();
	void			SearchForLookatEvent();
	void			UpdateTargetPosition(const CPed* player, Vector3& vTargetPosition);
	bool			ValidateLookatTarget(const Vector3& playerPosition);

	bool			TestLineOfSight(const Vector3& targetPosition, const CEntity* pTargetToTest, const u32 flags) const;
	bool			TrimPointToCollision(Vector3& testPosition, const u32 flags) const;

	void			RotateDirectionAboutAxis(Vector3& vDirection, const Vector3& vAxis, float fAngleToRotate);
	const CEvent*	GetShockingEvent(const CPed* pNearbyPed);

	void			TestEventTarget(const CPed* pNearbyPed,
									const CEntity*& pBestValidEventPed, float& fBestValidEventPedDistanceSq,
									const Vector3& vCameraFront2d);
	void			TestPedTarget(const CPed* player, const CPed* pNearbyVeh,
								  const CEntity*& pBestValidPed, float& fBestValidPedDistanceSq,
								  const CEntity*& pBestValidBird, float& fBestValidBirdDistanceSq,
								  const Vector3& vCameraFront2d,
								  float fCosMaxHorizontalAngleToSearch, float fCosMaxVerticalAngleToSearch);
	void			TestVehicleTarget(const CVehicle* pNearbyVeh,
									  const CEntity*& pBestValidVehicle, float& fBestValidVehicleDistanceSq,
									  const Vector3& vCameraFront2d,
									  float fCosMaxHorizontalAngleToSearch, float fCosMaxVerticalAngleToSearch);

	void			ResetForNewTarget();
	void			ClearTarget();
	void			OnTargetChange(const CEntity* pOldTarget);
	void			AddTargetToHistory(const CEntity* pTarget);
	s32				IsInHistoryList(const CEntity* pTarget);

	bool			IsBirdTarget(const CEntity* pTarget);
	bool			IsValidBirdTarget(const CEntity* pTarget);
	bool			IsScenarioPedProne(const CPed* pTarget) const;

	Quaternion		m_SavedOrientation;
	Quaternion		m_DesiredOrientation;
	Vector3			m_LookatPosition;
	RegdConstEnt	m_LastValidTarget[FPI_MAX_TARGET_HISTORY];
	float			m_fTurnRate;
	float			m_fZoomStartFov;
	float			m_LookatZOffset;
	float			m_PrevChestZOffset;
	float			m_PrevHeadZOffset;
	float			m_StartAngleToTarget;
	float			m_MinTurnRateScale;
#if USE_SIMPLE_DOF
	float			m_DistanceToTarget;
#endif
	u32				m_TimeSinceTargetLastUpdated;
	u32				m_TimeSinceLastTargetSearch;
	u32				m_TimeSinceLastEventSearch;
	u32				m_LastTimeLosWasClear;
	u32				m_TimeSinceTargetLastMoved;
	u32				m_TimeOfLastNoTargetPosUpdate;
	u32				m_ZoomInStartTime;
	u32				m_ZoomOutStartTime;
	u32				m_CheckOutGirlStartTime;
	u32				m_LastTimeBirdWasTargetted;
	u32				m_LastTimeVehicleWasTargetted;
	u32				m_RandomTimeToSelectNewNoTargetPoint;
	u32				m_TimeSinceTargetRotationEnded;
	u16				m_PedType;
	bool			m_LookingAtEntity;
	bool			m_IsActive;
	bool			m_BlendChestZOffset;
	bool			m_BlendHeadZOffset;
	bool			m_ZoomedInTA;
	bool			m_HasCheckedOutGirl;
	bool			m_ForceTargetPointDown;
	bool			m_PerformNoTargetInterpolation;
	bool			m_LockedOnTarget;

	//Forbid copy construction and assignment.
	camCinematicFirstPersonIdleCamera(const camCinematicFirstPersonIdleCamera& other);
	camCinematicFirstPersonIdleCamera& operator=(const camCinematicFirstPersonIdleCamera& other);
};

#endif // CINEMATIC_FIRST_PERSON_IDLE_CAMERA_H