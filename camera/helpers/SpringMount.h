//
// camera/helpers/SpringMount.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAM_SPRING_MOUNT_H
#define CAM_SPRING_MOUNT_H

#include "vector/vector3.h"

#include "camera/base/BaseObject.h"

class camFrame;
class camSpringMountMetadata;
class CPhysical;

//A basic simulation of a sprung camera mount.
class camSpringMount : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camSpringMount, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camSpringMount);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camSpringMount(const camBaseObjectMetadata& metadata);

public:
	void Reset();
	void Update(Matrix34& worldMatrix, const CPhysical& host);
	void Update(camFrame& frame, const CPhysical& host);

private:
	const camSpringMountMetadata& m_Metadata;

	Vector3	m_PreviousFrameVelocity;
	Vector3	m_SpringAngularVelocity;
	Vector3	m_SpringEulers;

private:
	//Forbid copy construction and assignment.
	camSpringMount(const camSpringMount& other);
	camSpringMount& operator=(const camSpringMount& other);
};

#endif // CAM_SPRING_MOUNT_H
