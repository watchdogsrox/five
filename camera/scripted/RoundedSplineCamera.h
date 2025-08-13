//
// camera/scripted/RoundedSplineCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef ROUNDED_SPLINE_CAMERA_H
#define ROUNDED_SPLINE_CAMERA_H

#include "camera/scripted/BaseSplineCamera.h"

class camRoundedSplineCameraMetadata;

class camRoundedSplineCamera : public camBaseSplineCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camRoundedSplineCamera, camBaseSplineCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camRoundedSplineCamera(const camBaseObjectMetadata& metadata);
	const camRoundedSplineCameraMetadata& m_Metadata;

protected:
	virtual void		BuildTranslationSpline();
	virtual void		ValidateSplineDuration(); 

private:
	//Forbid copy construction and assignment.
	camRoundedSplineCamera(const camRoundedSplineCamera& other);
	camRoundedSplineCamera& operator=(const camRoundedSplineCamera& other);
};

#endif