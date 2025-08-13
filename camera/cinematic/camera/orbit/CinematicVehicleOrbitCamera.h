//
// camera/cinematic/CinematicVehicleOrbitCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_VEHICLEORBIT_CAMERA_H
#define CINEMATIC_VEHICLEORBIT_CAMERA_H

#include "Camera/Base/BaseCamera.h"
#include "Camera/helpers/DampedSpring.h"

class camCinematicVehicleOrbitCameraMetadata;
class camCinematicVehicleLowOrbitCameraMetadata;
class camSpeedRelativeShakeSettingsMetadata;

struct sOrbitData
{
	float heading;
	float headingDelta; 
	float pitch;
	float pitchDelta; 
};

class camCinematicVehicleOrbitCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehicleOrbitCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum eVehicleOrbitCamModes
	{
		NUM_MODES = 6
	};

	bool IsValid();

	bool Update();

	void SetShotId(u32 ShotId) { m_shotId = ShotId; }

	u32 GetShotId() { return m_shotId; }
	
	void Init(); 

	virtual u32 GetNumOfModes() const { return NUM_MODES; }
	
protected:
	camCinematicVehicleOrbitCamera(const camBaseObjectMetadata& metadata);

private:
	bool UpdateShot(); 
	bool ShouldUpdate(); 

	float ComputeDesiredHeading();

	bool UpdateCollision(Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool ComputeCollision(Vector3& cameraPosition, const Vector3& lookAtPosition, const u32 flags);
	
	bool UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	
	bool IsClipping(const Vector3& position, const u32 flags); 

	bool IsPitchDeltaToHigh(const Vector3& cameraPosition, const Vector3& lookAtPosition); 

	bool IsAttachParentPitchSuitable(); 



private:
	const camCinematicVehicleOrbitCameraMetadata& m_Metadata;

	float m_orbitDistance; 
	float m_desiredOrbitAngle; 
	float m_intialOrbitAngle; 
	float m_shotTotalDuration; 
	float m_shotStartTime; 
	float m_Pitch; 
	float m_fov; 
	float m_hitLastFrame; 
	float m_hitDeltaThisFrame; 
	float m_ShotTimeSpentOccluded; 
	float m_TimeAttachParentPitchIsOutOfLimits; 


	u32 m_shotId; 
	u32 m_Direction; 

	bool m_canUpdate : 1;

};

class camCinematicVehicleLowOrbitCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehicleLowOrbitCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum eVehicleOrbitCamModes
	{
		NUM_MODES = 2
	};

	bool IsValid();

	bool Update();

	void SetShotId(u32 ShotId) { m_shotId = ShotId; }

	u32 GetShotId() { return m_shotId; }

	void Init(); 

	virtual u32 GetNumOfModes() const { return NUM_MODES; }

protected:
	camCinematicVehicleLowOrbitCamera(const camBaseObjectMetadata& metadata);

private:
	bool UpdateShot(); 
	bool ShouldUpdate(); 

	float ComputeDesiredHeading();

	bool UpdateCollision(Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool ComputeCollision(Vector3& cameraPosition, const Vector3& lookAtPosition, const u32 flags);

	bool UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 

	bool IsClipping(const Vector3& position, const u32 flags); 

	bool UpdateCameraPositionRelativeToGround(Vector3 &CamPos); 

	void UpdateLookAt(const Vector3& cameraPosition); 

	void ComputeCameraZwithRespectToVehicle( Vector3& cameraPosition);

	float ComputeWorldPitchWithRespectToVehiclePosition(const Vector3& CamPos); 

	//void ScaleSpringConstWithRespectToVelocity(float &SpringConst); 

	bool IsVehicleOrientationValid(const CVehicle& vehicle); 

	bool ShouldTerminateForChangeOfTrailerStatus(); 

	bool ShouldTerminateShotEarly(); 

	bool ShouldTerminateForReversing();

	void UpdateSpeedRelativeShake(const CPhysical& parentPhysical, const camSpeedRelativeShakeSettingsMetadata& settings, camDampedSpring& speedSpring);

private:
	const camCinematicVehicleLowOrbitCameraMetadata& m_Metadata;
	camDampedSpring m_OrbitHeadingSpring; 
	camDampedSpring	m_CameraPosZSpring;
	camDampedSpring m_CollisionPushSpring; 
	camDampedSpring m_InaccuracyShakeSpring;
	camDampedSpring m_HighSpeedShakeSpring;

	float m_orbitDistance; 
	float m_intialOrbitAngle; 
	float m_Pitch; 
	float m_fov; 
	float m_hitLastFrame; 
	float m_hitDeltaThisFrame; 
	float m_ShotTimeSpentOccluded; 
	float m_TimeToTerminateShotEarly; 
	float m_TimeAttachParentPitchIsOutOfLimits; 
	float m_MinHeightAboveGround; 
	float m_LastFramPitch; 
	float m_lastFrameTrailerHeadingDelta; 
	
	u32 m_shotId; 
	u32 m_Direction; 

	bool m_canUpdate : 1;
	bool m_shouldUpdateSpring :1; 
	bool m_HasTrailerOnInit :1;
	bool m_shouldCameraLookLeft :1; 
	bool m_ShouldTerminateForTrailerChange :1; 

};

#endif