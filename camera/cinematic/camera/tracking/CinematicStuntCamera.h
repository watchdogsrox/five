//
// camera/cinematic/CinematicStuntCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_STUNT_CAMERA_H
#define CINEMATIC_STUNT_CAMERA_H

#include "camera/cinematic/camera/tracking/BaseCinematicTrackingCamera.h"

class camCinematicStuntCameraMetadata;

class camCinematicStuntCamera : public camBaseCinematicTrackingCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicStuntCamera, camBaseCinematicTrackingCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool IsValid(); 

	bool Update(); 

protected:
	void Init(); 
	
	void ComputeFovParams(); 

	virtual bool ComputeDesiredLookAtPosition(); 
	
	virtual void UpdateZoom(); 

	camCinematicStuntCamera(const camBaseObjectMetadata& metadata);

protected:
	float m_StartFovTime; 
	float m_StartFov; 
	float m_EndFov; 
	float m_ZoomInDuration; 
	float m_ZoomHoldDuration; 
	float m_ZoomOutDuration; 
	u32 m_ZoomStartTime;
	bool m_IsZoomInComplete;
	bool m_IsZoomHoldComplete;

private:
	const camCinematicStuntCameraMetadata& m_Metadata;

private:
	//Forbid copy construction and assignment.
	camCinematicStuntCamera(const camCinematicStuntCamera& other);
	camCinematicStuntCamera& operator=(const camCinematicStuntCamera& other);
};

#endif // CINEMATIC_STUNT_CAMERA_H