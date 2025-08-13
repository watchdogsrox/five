//
// camera/gameplay/follow/FollowObjectCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FOLLOW_OBJECT_CAMERA_H
#define FOLLOW_OBJECT_CAMERA_H

#include "camera/gameplay/follow/FollowCamera.h"

class camFollowObjectCameraMetadata;

class camFollowObjectCamera : public camFollowCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFollowObjectCamera, camFollowCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camFollowObjectCamera(const camBaseObjectMetadata& metadata);

	virtual bool	Update();

	const camFollowObjectCameraMetadata& m_Metadata;

private:
	//Forbid copy construction and assignment.
	camFollowObjectCamera(const camFollowObjectCamera& other);
	camFollowObjectCamera& operator=(const camFollowObjectCamera& other);
};

#endif // FOLLOW_OBJECT_CAMERA_H
