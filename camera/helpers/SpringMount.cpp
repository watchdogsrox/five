//
// camera/helpers/SpringMount.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//
#include "camera/helpers/SpringMount.h"

#include "math/vecmath.h"

#include "fwsys/timer.h"

#include "camera/helpers/Frame.h"
#include "camera/system/CameraMetadata.h"
#include "scene/Physical.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camSpringMount,0x3D211ECA)


camSpringMount::camSpringMount(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camSpringMountMetadata&>(metadata))
, m_PreviousFrameVelocity(VEC3_ZERO)
, m_SpringAngularVelocity(VEC3_ZERO)
, m_SpringEulers(VEC3_ZERO)
{
}

void camSpringMount::Reset()
{
	m_PreviousFrameVelocity.Zero();
	m_SpringAngularVelocity.Zero();
	m_SpringEulers.Zero();
}

void camSpringMount::Update(Matrix34& worldMatrix, const CPhysical& host)
{
	Vector3 frameOffset				= worldMatrix.d - VEC3V_TO_VECTOR3(host.GetTransform().GetPosition());
	Vector3 frameVelocity			= host.GetLocalSpeed(frameOffset);
	Vector3 frameAcceleration		= (frameVelocity - m_PreviousFrameVelocity) * fwTimer::GetInvTimeStep();
	m_PreviousFrameVelocity			= frameVelocity;

	frameAcceleration = VEC3V_TO_VECTOR3(host.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(frameAcceleration))); //Convert to host-relative acceleration.

	//Limit Acceleration.
	frameAcceleration.Max(frameAcceleration, -m_Metadata.m_AccelerationLimit);
	frameAcceleration.Min(frameAcceleration, m_Metadata.m_AccelerationLimit);

	float timeStep					= fwTimer::GetTimeStep();

	//Switch the x and y Eulers, as forward acceleration produces pitch and sideways acceleration produces roll.
	float tempEuler					= frameAcceleration.x;
	frameAcceleration.x				= frameAcceleration.y;
	frameAcceleration.y				= tempEuler;
	frameAcceleration.z				= 0.0f; //We don't displace the mount vertically.

	//Apply force of acceleration to angular velocity.
	Vector3 velocityDelta			= m_Metadata.m_AccelerationForce * frameAcceleration * timeStep;
	m_SpringAngularVelocity			+= velocityDelta;

	//Apply forces of spring.
	Vector3 springDelta				= m_Metadata.m_SpringForce * m_SpringEulers * timeStep;
	m_SpringAngularVelocity			-= springDelta;

	//Dampen overall angular velocity.
	Vector3 dampeningDelta			= Vector3(	rage::Powf(m_Metadata.m_DampeningForce.x, timeStep),
												rage::Powf(m_Metadata.m_DampeningForce.y, timeStep),
												rage::Powf(m_Metadata.m_DampeningForce.z, timeStep));
	m_SpringAngularVelocity			*= dampeningDelta;

	//Apply the angular velocity.
	m_SpringEulers					+= m_SpringAngularVelocity;

	//Apply the resulting displacement to the world matrix.
	Vector3 eulers;
	worldMatrix.ToEulersYXZ(eulers);
	eulers							+= m_SpringEulers;
	worldMatrix.FromEulersYXZ(eulers);
}

void camSpringMount::Update(camFrame& frame, const CPhysical& host)
{
	Matrix34 worldMatrix = frame.GetWorldMatrix();
	Update(worldMatrix, host);
	frame.SetWorldMatrix(worldMatrix);
}
