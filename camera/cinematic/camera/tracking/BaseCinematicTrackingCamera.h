//
// camera/cinematic/BaseCinematicTrackingCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_CINEMATIC_TRACKING_CAMERA_H
#define BASE_CINEMATIC_TRACKING_CAMERA_H

#include "camera/base/BaseCamera.h"

class camBaseCinematicTrackingCameraMetadata;

class camBaseCinematicTrackingCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseCinematicTrackingCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camBaseCinematicTrackingCamera(const camBaseObjectMetadata& metadata);

	virtual bool Update(); 

	virtual bool ComputeDesiredLookAtPosition(); 

	virtual bool UpdateOrientation(); 

protected:
	Vector3 m_DesiredLookAtPosition;
	u32 m_ShotStartTime; 
	
private:
	const camBaseCinematicTrackingCameraMetadata& m_Metadata;

private:
	//Forbid copy construction and assignment.
	camBaseCinematicTrackingCamera(const camBaseCinematicTrackingCamera& other);
	camBaseCinematicTrackingCamera& operator=(const camBaseCinematicTrackingCamera& other);
};

#endif // BASE_CINEMATIC_TRACKING_CAMERA_H