//
// camera/cinematic/CinematicHeliChaseCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_HELI_CHASE_CAMERA_H
#define CINEMATIC_HELI_CHASE_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"

class camCinematicHeliChaseCameraMetadata;
class camInconsistentBehaviourZoomHelper;
class camLookAheadHelper; 
class camLookAtDampingHelper; 
class camDampedSpring;

//A cinematic camera that tracks a target from the perspective of a chase helicopter.
class camCinematicHeliChaseCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicHeliChaseCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	~camCinematicHeliChaseCamera();

	enum eTrackingAccuracyMode
	{
		HIGH_TRACKING_ACCURACY = 0,
		MEDIUM_TRACKING_ACCURACY,
		LOW_TRACKING_ACCURACY,
		NUM_OF_ACCURACY_MODES
	};

private:
	camCinematicHeliChaseCamera(const camBaseObjectMetadata& metadata);
	
public:
	bool			IsValid();
	void			SetPathHeading(float heading) { m_PathHeading = heading; }
	virtual void	SetMode(s32 mode) { m_Mode = mode; }
	virtual u32		GetNumOfModes() const { return 4; } 
	void			SetCanUseLookAtDampingHelpers(bool UseDampingHelpers) { m_CanUseDampedLookAtHelpers = UseDampingHelpers; }
	void			SetTrackingAccuracyMode(u32 Mode);
	void			SetCanUseLookAheadHelper(bool UseLookAheadHelpers) { m_CanUseLookAheadHelper = UseLookAheadHelpers; }

#if __BANK
	virtual void	DebugRender();
	static void		AddWidgets(bkBank &bank);
#endif // __BANK

private:
	virtual bool	Update();
	bool			IsClipping(const Vector3& position, const u32 flags); 
	void			ComputeValidRoute();
	bool			ComputeIsPositionValid(const Vector3& position, Vector3& targetPosition);
	void			UpdateZoom(const Vector3& position, const Vector3& targetPosition);
	void			UpdateZoomForInconsistencies(const Vector3& position);
	void			UpdateShake();
	void			UpdateCameraOperatorShakeAmplitudes(float& xUncertaintyAmplitude, float& zUncertaintyAmplitude, float& turbulenceAmplitude);
	float			ComputeFov(const Vector3& position, const float boundRadiusScale) const;
	void			TriggerZoom(float InitialFov, float TargetFov, u32 ZoomDuration); 
	void			ComputeMaxFovBasedOnTargetVelocity(float &MaxFov); 
	void			DampFov(float &Fov); 
	float			RandomiseInitialHeading(float ForbiddenAngle, float MinAngleDelta, float MinHeadingFromMinRange, float MaxHeadingFromMaxRange); 
	void			RandomiseTrackingAccuracy(); 
	bool			HasCameraCollidedBetweenUpdates(const Vector3& PreviousPosition, const Vector3& currentPosition); 

	const CObject*  GetParachuteObject() const;

	const camCinematicHeliChaseCameraMetadata& m_Metadata;
	
	camDampedSpring m_FovSpring; 
	camDampedSpring m_CameraOperatorUncertaintySpring;
	camDampedSpring m_CameraOperatorHeadingRatioSpring;
	camDampedSpring m_CameraOperatorPitchRatioSpring;
	camDampedSpring m_CameraOperatorTurbulenceSpring;

	Vector3	m_RouteStartPosition;
	Vector3	m_RouteEndPosition;
	Vector3	m_TargetLockPosition;
	Vector3 m_LocalStart; 
	Vector3 m_LocalEnd;
	Vector3 m_InitialLookAtTargetPosition; 
	Vector3 m_LookAtPosition;
	u32		m_StartGameTime;
	u32		m_PreviousTimeHadLosToTarget;
	u32		m_TargetLockStartTime;
	u32		m_ZoomDuration;
	u32		m_CurrentZoomTime; 
	s32		m_Mode; 
	float	m_StartDistanceBehindTarget;
	float	m_EndDistanceBeyondTarget;
	float	m_HeightAboveTarget;
	float	m_HeightAboveTargetForTrain;
	float	m_HeightAboveTargetForHeli;
	float	m_HeightAboveTargetForJetpack;
	float	m_PathOffsetFromSideOfVehicle;
	float	m_HeightAboveTargetForBike; 
	float	m_ZoomOutStartFov;
	float	m_PathHeading; 
	float	m_MaxFov; 
	float	m_MinFov; 
	float	m_InitialHeading; 
	float	m_TrackingAccuracy; 

	float	m_CurrentFov; 
	float	m_TargetFov; 

	float	m_HeadingOnPreviousUpdate;
	float	m_PitchOnPreviousUpdate;
	float	m_RollOnPreviousUpdate;
	float	m_FovOnPreviousUpdate;

	camLookAtDampingHelper* m_InVehicleLookAtDamper; 
	camLookAtDampingHelper* m_OnFootLookAtDamper; 
	camLookAheadHelper* m_InVehicleLookAheadHelper;
	camInconsistentBehaviourZoomHelper* m_InconsistentBehaviourZoomHelper;

	bool	m_HasAcquiredTarget		: 1;
	bool	m_ShouldLockTarget		: 1;
	bool	m_CanUseDampedLookAtHelpers : 1; 
	bool	m_ShouldZoom			: 1; 
	bool	m_ShouldIgnoreLineOfAction : 1; 
	bool	m_ShouldTrackParachuteInMP		: 1; 
	bool	m_CanUseLookAheadHelper : 1;
	bool	m_ReactedToInconsistentBehaviourLastUpdate : 1;
	bool	m_ZoomTriggeredFromInconsistency : 1;
	bool	m_HasLosToTarget		: 1;

#if __BANK
	static float ms_DebugUncertaintyShakeAmplitude;
	static float ms_DebugHeadingRatio;
	static float ms_DebugTurbulenceVehicleScale;
	static float ms_DebugTurbulenceDistanceScale;
	static bool ms_DebugSetUncertaintyShakeAmplitude;
	static bool ms_DebugSetTurbulenceShakeAmplitude;
	static bool ms_DebugSetShakeRatio;
#endif // __BANK

	//Forbid copy construction and assignment.
	camCinematicHeliChaseCamera(const camCinematicHeliChaseCamera& other);
	camCinematicHeliChaseCamera& operator=(const camCinematicHeliChaseCamera& other);
};

#endif // CINEMATIC_HELI_CHASE_CAMERA_H
