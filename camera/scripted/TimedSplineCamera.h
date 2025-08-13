//
// camera/scripted/TimedSplineCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TIMED_SPLINE_CAMERA_H
#define TIMED_SPLINE_CAMERA_H

#include "camera/scripted/SmoothedSplineCamera.h"

class camTimedSplineCameraMetadata;

class camTimedSplineCamera : public camSmoothedSplineCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camTimedSplineCamera, camSmoothedSplineCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camTimedSplineCamera(const camBaseObjectMetadata& metadata);
	const camTimedSplineCameraMetadata& m_Metadata;

public:

protected:
	virtual void AddNodeInternal(camSplineNode* node, u32 transitionTime); 
	virtual void ValidateSplineDuration(); 
	//NOTE: The transition deltas are fixed times, so we need not update this.
	virtual void UpdateTransitionDeltas() {}
	virtual void BuildTranslationSpline();
	virtual void ConstrainTranslationVelocities();

private:
	//Forbid copy construction and assignment.
	camTimedSplineCamera(const camTimedSplineCamera& other);
	camTimedSplineCamera& operator=(const camTimedSplineCamera& other);
};

#endif