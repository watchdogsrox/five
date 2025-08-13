//
// camera/scripted/SmoothedSplineCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef SMOOTHED_SPLINE_CAMERA_H
#define SMOOTHED_SPLINE_CAMERA_H

#include "camera/scripted/RoundedSplineCamera.h"

class camSmoothedSplineCameraMetadata;

class camSmoothedSplineCamera : public camRoundedSplineCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camSmoothedSplineCamera, camRoundedSplineCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camSmoothedSplineCamera(const camBaseObjectMetadata& metadata);
	const camSmoothedSplineCameraMetadata& m_Metadata;

protected:
	virtual void		BuildTranslationSpline();
	void		SmoothTranslationVelocities();

private:
	//Forbid copy construction and assignment.
	camSmoothedSplineCamera(const camSmoothedSplineCamera& other);
	camSmoothedSplineCamera& operator=(const camSmoothedSplineCamera& other);

};

#endif