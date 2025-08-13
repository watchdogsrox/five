//
// camera/helpers/DampedSpring.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/DampedSpring.h"

#include "control/replay/Replay.h"

#include "math/amath.h"

#include "math/angmath.h"

#include "fwmaths/angle.h"

#include "fwsys/timer.h"

CAMERA_OPTIMISATIONS()


float camDampedSpring::Update(float target, float springConstant, float dampingRatio /*= 1.0f*/, bool shouldUseCameraTimeStep /*= false*/, bool REPLAY_ONLY(shouldUseReplayTimeStep) /*= false*/, bool shouldUseNonPausableCamTimeStep /*= false*/)
{
	//If spring constant is too small, snap to the target.
	if(springConstant < SMALL_FLOAT)
	{
		Reset(target);
	}
	else
	{
		float timeStep = shouldUseNonPausableCamTimeStep ? fwTimer::GetNonPausableCamTimeStep() : shouldUseCameraTimeStep ? fwTimer::GetCamTimeStep() : fwTimer::GetTimeStep();

#if GTA_REPLAY
		if (shouldUseReplayTimeStep && (CReplayMgr::GetPlaybackFrameStepDeltaMsFloat() / 1000.0f) >= SMALL_FLOAT)
		{
			timeStep = CReplayMgr::GetPlaybackFrameStepDeltaMsFloat() / 1000.0f;
		}
#endif // GTA_REPLAY

		if(timeStep >= SMALL_FLOAT)
		{
			//NOTE: The spring equations have been normalised for unit mass.

			const float intialDisplacement	= m_Result - target;
			const float halfDampingCoeff	= dampingRatio * Sqrtf(springConstant);
			const float omegaSqr			= springConstant - (halfDampingCoeff * halfDampingCoeff);

			float springDisplacementToApply	= 0.0f;
			if(omegaSqr >= SMALL_FLOAT)
			{
				//Under-damped.
				const float omega			= Sqrtf(omegaSqr);
				const float c				= (m_Velocity + (halfDampingCoeff * intialDisplacement)) / omega;
				springDisplacementToApply	= expf(-halfDampingCoeff * timeStep) * ((intialDisplacement * Cosf(omega * timeStep)) +
												(c * Sinf(omega * timeStep)));
				m_Velocity					= expf(-halfDampingCoeff * timeStep) * ((-halfDampingCoeff *
												((intialDisplacement * Cosf(omega * timeStep)) + (c * Sinf(omega * timeStep)))) +
												(omega * ((c * Cosf(omega * timeStep)) - (intialDisplacement * Sinf(omega * timeStep)))));
			}
			else
			{
				//Critically- or over-damped.
				//NOTE: We must negate (and clamp) the square omega term and use an alternate equation to avoid computing the square root of a
				//negative number.
				const float negatedOmegaSqr	= Max(-omegaSqr, 0.0f);
				const float omega			= Sqrtf(negatedOmegaSqr);
				springDisplacementToApply	= intialDisplacement * expf((-halfDampingCoeff + omega) * timeStep);
				m_Velocity					= (-halfDampingCoeff + omega) * springDisplacementToApply;
			}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 			NOTE: The equation for critical damping should actually be as below, but the solution for an over-damped spring works adequately for
//			our purposes.
//
// 			const float	alpha			= m_Velocity + (halfDampingCoeff * intialDisplacement);
// 			springDisplacementToApply	= ((alpha * timeStep) + intialDisplacement) * expf(-halfDampingCoeff * timeStep);
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			m_Result = target + springDisplacementToApply;
		}
	}

	return m_Result;
}

float camDampedSpring::UpdateAngular(float target, float springConstant, float dampingRatio, bool shouldUseCameraTimeStep)
{
	//If spring constant is too small, snap to the target.
	if(springConstant < SMALL_FLOAT)
	{
		Reset(target);
	}
	else
	{
		const float timeStep = shouldUseCameraTimeStep ? fwTimer::GetCamTimeStep() : fwTimer::GetTimeStep();
		if(timeStep >= SMALL_FLOAT)
		{
			//NOTE: The spring equations have been normalised for unit mass.

			const float intialDisplacement	= rage::SubtractAngleShorter(m_Result, target);
			const float halfDampingCoeff	= dampingRatio * Sqrtf(springConstant);
			const float omegaSqr			= springConstant - (halfDampingCoeff * halfDampingCoeff);

			float springDisplacementToApply	= 0.0f;
			if(omegaSqr >= SMALL_FLOAT)
			{
				//Under-damped.
				const float omega			= Sqrtf(omegaSqr);
				const float c				= (m_Velocity + (halfDampingCoeff * intialDisplacement)) / omega;
				springDisplacementToApply	= expf(-halfDampingCoeff * timeStep) * ((intialDisplacement * Cosf(omega * timeStep)) +
												(c * Sinf(omega * timeStep)));
				m_Velocity					= expf(-halfDampingCoeff * timeStep) * ((-halfDampingCoeff *
												((intialDisplacement * Cosf(omega * timeStep)) + (c * Sinf(omega * timeStep)))) +
												(omega * ((c * Cosf(omega * timeStep)) - (intialDisplacement * Sinf(omega * timeStep)))));
			}
			else
			{
				//Critically- or over-damped.
				//NOTE: We must negate (and clamp) the square omega term and use an alternate equation to avoid computing the square root of a
				//negative number.
				const float negatedOmegaSqr	= Max(-omegaSqr, 0.0f);
				const float omega			= Sqrtf(negatedOmegaSqr);
				springDisplacementToApply	= intialDisplacement * expf((-halfDampingCoeff + omega) * timeStep);
				m_Velocity					= (-halfDampingCoeff + omega) * springDisplacementToApply;
			}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 			NOTE: The equation for critical damping should actually be as below, but the solution for an over-damped spring
//			works adequately for our purposes.
//
// 			const float	alpha			= m_Velocity + (halfDampingCoeff * intialDisplacement);
// 			springDisplacementToApply	= ((alpha * timeStep) + intialDisplacement) * expf(-halfDampingCoeff * timeStep);
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			m_Result = fwAngle::LimitRadianAngleSafe(target + springDisplacementToApply);
		}
	}

	return m_Result;
}
