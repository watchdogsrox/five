//
// camera/gameplay/aim/ThirdPersonPedMeleeAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef THIRD_PERSON_PED_MELEE_AIM_CAMERA_H
#define THIRD_PERSON_PED_MELEE_AIM_CAMERA_H

#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"

class camThirdPersonPedMeleeAimCameraMetadata;

class camThirdPersonPedMeleeAimCamera : public camThirdPersonPedAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camThirdPersonPedMeleeAimCamera, camThirdPersonPedAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camThirdPersonPedMeleeAimCamera(const camBaseObjectMetadata& metadata);

public:

protected:
	virtual bool	Update();
	virtual void	UpdateLockOn();
	virtual void	UpdateLockOnEnvelopes(const CEntity* desiredLockOnTarget);
	virtual float	UpdateOrbitDistance();
	virtual void	ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch);

	const camThirdPersonPedMeleeAimCameraMetadata& m_Metadata;

	camDampedSpring	m_LockOnDistanceSpring;
	camDampedSpring	m_LockOnHeadingSpring;
	camDampedSpring	m_LockOnPitchSpring;

	//Forbid copy construction and assignment.
	camThirdPersonPedMeleeAimCamera(const camThirdPersonPedMeleeAimCamera& other);
	camThirdPersonPedMeleeAimCamera& operator=(const camThirdPersonPedMeleeAimCamera& other);
};

#endif // THIRD_PERSON_PED_MELEE_AIM_CAMERA_H
