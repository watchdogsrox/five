//
// camera/gameplay/follow/FollowParachuteCamera.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef FOLLOW_PARACHUTE_CAMERA_H
#define FOLLOW_PARACHUTE_CAMERA_H

#include "camera/gameplay/follow/FollowObjectCamera.h"

class camEnvelope;
class camFollowParachuteCameraMetadata;

class camFollowParachuteCamera : public camFollowObjectCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFollowParachuteCamera, camFollowObjectCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual ~camFollowParachuteCamera();

protected:
	camFollowParachuteCamera(const camBaseObjectMetadata& metadata);

	virtual bool	Update();
	virtual float	ComputeVerticalMoveSpeedScaling(const Vector3& attachParentVelocity) const;

	const camFollowParachuteCameraMetadata& m_Metadata;
	camEnvelope*	m_VerticalMoveSpeedEnvelope;

	//Forbid copy construction and assignment.
	camFollowParachuteCamera(const camFollowParachuteCamera& other);
	camFollowParachuteCamera& operator=(const camFollowParachuteCamera& other);
};

#endif // FOLLOW_PARACHUTE_CAMERA_H
