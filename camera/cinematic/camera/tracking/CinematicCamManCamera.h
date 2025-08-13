//
// camera/cinematic/CinematicCamManCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_CAM_MAN_CAMERA_H
#define CINEMATIC_CAM_MAN_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"

class camCinematicCameraManCameraMetadata;
class CEntity;
class camInconsistentBehaviourZoomHelper;
class camLookAheadHelper; 
class camLookAtDampingHelper; 
class camDampedSpring;

//A cinematic camera that tracks a target from the perspective of camera man at a fixed position.
class camCinematicCamManCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicCamManCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	~camCinematicCamManCamera();

protected:
	camCinematicCamManCamera(const camBaseObjectMetadata& metadata);

public:

	bool			IsValid();

	virtual void	SetMode(s32 mode) { m_Mode = mode; }
	virtual void	LookAt(const CEntity* target);
	virtual u32		GetNumOfModes() const { return 4; } 
	
	static CEntity*	GetVehiclePedIsExitingEnteringOrIn(const CPed* pPed); 

	void SetShouldScaleFovForLookAtTargetBounds(bool ScaleFov)	{ m_ShouldScaleFovForLookAtTargetBounds = ScaleFov; }
	void SetCanUseLookAtDampingHelpers(bool UseDampingHelpers)	{ m_CanUseDampedLookAtHelpers = UseDampingHelpers; }
	void SetMaxShotDuration(u32 maxShotDuration)				{ m_MaxShotDuration = maxShotDuration; }
	void SetCanUseLookAheadHelper(bool UseLookAheadHelper)		{ m_CanUseLookAheadHelper = UseLookAheadHelper; }
	void SetBoomShot(float totalHeightChange);
	void SetVehicleRelativeBoomShot(float totalHeightChange, Vector2 vehicleRelativePositionMultiplier, float minimumHeight, float scanRadius);

#if __BANK
	static void		AddWidgets(bkBank &bank);
#endif // __BANK

protected:
	virtual bool	Update();
	virtual bool	ComputeIsPositionValid();
	bool			ComputeCanUseBoomShot(const Vector3& cameraPosition) const;
	void			UpdateZoom(const Vector3& Pos);
	void			UpdateZoomForInconsistencies(const Vector3& position);
	void			UpdateShake();
	void			UpdateCameraOperatorShakeAmplitudes(float& xUncertaintyAmplitude, float& zUncertaintyAmplitude, float& turbulenceAmplitude);
	float			RandomiseInitialHeading(float InitialHeading, float AngleRange); 
	float			RandomiseInitialHeading(); 
	bool			IsClipping(const Vector3& position, const u32 flags); 
	void			ScaleFovBasedOnLookAtTargetBounds(const Vector3& CameraPos); 
	void			DampFov(float &Fov); 
	void			ExtendCameraPositionIfCloseToVehicleFront(const Matrix34& LookAtTagetMat, float Heading, float &scanRadius, float& scanDistance ); 
	float			ComputeFov(const Vector3& position, const float boundRadiusScale) const; 
	void			TriggerZoom(float InitialFov, float TargetFov, u32 ZoomDuration); 
	void			ComputeMaxFovBasedOnTargetVelocity(float &MaxFov); 

	const camCinematicCameraManCameraMetadata& m_Metadata;

	camDampedSpring m_CameraOperatorUncertaintySpring;
	camDampedSpring m_CameraOperatorHeadingRatioSpring;
	camDampedSpring m_CameraOperatorPitchRatioSpring;
	camDampedSpring m_CameraOperatorTurbulenceSpring;

	Vector3 m_LookAtPosition;
	Vector2 m_VehicleRelativeBoomShotPositionMultiplier;

	s32		m_Mode;
	u32		m_StartTime;
	u32		m_PreviousTimeAtLostStaticLosToTarget;
	u32		m_PreviousTimeAtLostDynamicLosToTarget;
	u32		m_CurrentZoomTime; 
	u32		m_ZoomDuration; 
	u32		m_MaxShotDuration;
	u32		m_BoomShotTimeAtStart;

	float	m_BoomShotCameraHeightAtStart;
	float	m_BoomShotMininumHeight;
	float	m_BoomShotTotalHeightChange;
	float	m_MaxFov; 
	float	m_MinFov; 
	float	m_CurrentFov; 
	float	m_TargetFov; 
	float	m_VehicleRelativeBoomShotScanRadius;

	float	m_HeadingOnPreviousUpdate;
	float	m_PitchOnPreviousUpdate;
	float	m_RollOnPreviousUpdate;
	float	m_FovOnPreviousUpdate;

	bool	m_HasAcquiredTarget		: 1;
	bool	m_ShouldStartZoomedIn	: 1;
	bool	m_ShouldZoom			: 1;
	bool	m_ShouldElevatePosition	: 1;
	bool	m_CanUpdate				: 1; 
	bool	m_IsActive				: 1; 
	bool	m_ShouldScaleFovForLookAtTargetBounds : 1; 
	bool	m_CanUseDampedLookAtHelpers : 1; 
	bool	m_CanUseLookAheadHelper	: 1;
	bool	m_ShouldZoomAtStart : 1; 
	bool	m_ShouldUseVehicleRelativeBoomShot : 1;
	bool	m_ReactedToInconsistentBehaviourLastUpdate : 1;
	bool	m_ZoomTriggeredFromInconsistency : 1;
	bool	m_HasLostStaticLosToTarget	: 1;
	bool	m_HasLostDynamicLosToTarget	: 1;

#if __BANK
	static float ms_DebugUncertaintyShakeAmplitude;
	static float ms_DebugHeadingRatio;
	static float ms_DebugTurbulenceVehicleScale;
	static float ms_DebugTurbulenceDistanceScale;
	static bool ms_DebugSetUncertaintyShakeAmplitude;
	static bool ms_DebugSetTurbulenceShakeAmplitude;
	static bool ms_DebugSetShakeRatio;
#endif // __BANK

	camDampedSpring m_FovSpring; 

	camLookAtDampingHelper*				m_InVehicleLookAtDamper; 
	camLookAtDampingHelper*				m_OnFootLookAtDamper; 
	camLookAheadHelper*					m_InVehicleLookAheadHelper;
	camInconsistentBehaviourZoomHelper* m_InconsistentBehaviourZoomHelper;

	//static bank_float	ms_ScanRadiusForMode[NUM_MODES];
	//static bank_float	ms_ScanDistanceForMode[NUM_MODES];
	Vector3 m_WaterPlane; 
	Vector3 m_waterFixedHeight; 
	Vector3 NavMeshHeight; 
	
private:
	//Forbid copy construction and assignment.
	camCinematicCamManCamera(const camCinematicCamManCamera& other);
	camCinematicCamManCamera& operator=(const camCinematicCamManCamera& other);
};

#endif // CINEMATIC_CAM_MAN_CAMERA_H
