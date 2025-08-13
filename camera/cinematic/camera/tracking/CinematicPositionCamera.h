//
// camera/cinematic/CinematicPositionCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_POSITION_CAMERA_H
#define CINEMATIC_POSITION_CAMERA_H

#include "Camera/Base/BaseCamera.h"
#include "camera/system/CameraMetadata.h"

class camCinematicPositionCameraMetadata;
class camLookAheadHelper; 
class camLookAtDampingHelper;

class camCinematicPositionCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicPositionCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	~camCinematicPositionCamera();

	bool IsValid();

	bool Update();
	
	void OverrideCamOrbitBasePosition(const Vector3& position, float heading); 

	void OverrideCamPosition(const Vector3& position) { m_CamPosition = position;  m_shouldUseAttachParent = false; }

	void ComputeCamPositionRelativeToBaseOrbitPosition(); 
	
	void SetShouldZoom(bool zoom) { m_ShouldZoom  = zoom; }

	void SetShouldStartZoomed(bool startZoomed) { m_ShouldStartZoomedIn = startZoomed; }
	
	u32 GetNumOfModes() const { return m_Metadata.m_NumofModes; }

	void SetCanUseLookAtDampingHelpers(bool UseDampingHelpers) { m_CanUseDampedLookAtHelpers = UseDampingHelpers; }
	void SetCanUseLookAheadHelper(bool UseLookAheadHelpers) { m_CanUseLookAheadHelper = UseLookAheadHelpers; }
	
	virtual void SetMode(s32 mode) { m_Mode = mode; }
protected:
	camCinematicPositionCamera(const camBaseObjectMetadata& metadata);

private:
	bool UpdateShot(); 
	bool ShouldUpdate(); 

	bool UpdateCollision(Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool ComputeCollision(Vector3& cameraPosition, const Vector3& lookAtPosition, const u32 flags);
	
	bool UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool IsTargetTooFarAway(const Vector3& cameraPosition, const Vector3& lookAtPosition);

	bool IsClipping(const Vector3& position, const u32 flags); 
	
	Vector3 ComputePositionRelativeToAttachParent(); 
	float ComputeOrbitDistance(); 
	float ComputeIntialDesiredHeading(); 
	void RandomiseInitialHeading(); 

	void UpdateZoom(); 
private:
	const camCinematicPositionCameraMetadata& m_Metadata;

	Vector3 m_intialPosition; 
	Vector3 m_CamPosition; 
	Vector3 m_LookAtPosition;

	float m_intialHeading; 
	float m_fov; 
	u32 m_ShotTimeSpentOccluded; 
	u32 m_StartTime;
	s32 m_Mode; 

	camLookAtDampingHelper* m_InVehicleLookAtDamper;
	camLookAtDampingHelper* m_OnFootLookAtDamper;
	camLookAheadHelper* m_InVehicleLookAheadHelper;

	bool m_canUpdate : 1;
	bool m_shouldUseAttachParent : 1; 
	bool m_ShouldStartZoomedIn : 1; 
	bool m_ShouldZoom : 1; 
	bool m_HaveComputedBaseOrbitPosition : 1; 
	bool m_CanUseDampedLookAtHelpers : 1;
	bool m_CanUseLookAheadHelper : 1;
};

#endif
