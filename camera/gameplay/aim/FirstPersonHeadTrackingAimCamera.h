//
// camera/gameplay/aim/FirstPersonHeadTrackingAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FIRST_PERSON_HEADTRACKING_AIM_CAMERA_H
#define FIRST_PERSON_HEADTRACKING_AIM_CAMERA_H

#include "camera/gameplay/aim/AimCamera.h"

class camFirstPersonHeadTrackingAimCameraMetadata;

class camFirstPersonHeadTrackingAimCamera : public camAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFirstPersonHeadTrackingAimCamera, camAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool			ShouldDisplayReticule() const;

protected:
	camFirstPersonHeadTrackingAimCamera(const camBaseObjectMetadata& metadata);

	virtual bool	Update();
	virtual void	UpdatePosition();
	virtual void	UpdateOrientation();
	void			UpdateShake();

	const camFirstPersonHeadTrackingAimCameraMetadata& m_Metadata;

private:
	//Forbid copy construction and assignment.
	camFirstPersonHeadTrackingAimCamera(const camFirstPersonHeadTrackingAimCamera& other);
	camFirstPersonHeadTrackingAimCamera& operator=(const camFirstPersonHeadTrackingAimCamera& other);
};

#endif // FIRST_PERSON_HEADTRACKING_AIM_CAMERA_H
