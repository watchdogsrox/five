//
// camera/helpers/DampedSpring.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_DAMPED_SPRING_H
#define CAMERA_DAMPED_SPRING_H

#include "camera/base/BaseObject.h"

class camDampedSpringMetadata;

//A simple damped spring simulation.
class camDampedSpring
{
public:
	camDampedSpring() { Reset(); }

	void	Reset(float target = 0.0f)				{ m_Velocity = 0.0f; m_Result = target; }
	float	GetResult() const						{ return m_Result; }
	float	GetVelocity() const						{ return m_Velocity; }
	void	OverrideResult(float overriddenValue)	{ m_Result = overriddenValue; }
	void	OverrideVelocity(float overriddenValue)	{ m_Velocity = overriddenValue; }

	//NOTE: A dampingRatio of 1.0 gives critical damping.
	float	Update(float target, float springConstant, float dampingRatio = 1.0f, bool shouldUseCameraTimeStep = false, bool shouldUseReplayTimeStep = false, bool shouldUseNonPausableCamTimeStep = false);
	float	UpdateAngular(float target, float springConstant, float dampingRatio = 1.0f, bool shouldUseCameraTimeStep = false);

private:
	float	m_Velocity;
	float	m_Result;

private:
	//Forbid copy construction and assignment.
	camDampedSpring(const camDampedSpring& other);
	camDampedSpring& operator=(const camDampedSpring& other);
};

#endif // CAMERA_DAMPED_SPRING_H
