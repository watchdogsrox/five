//
// camera/helpers/HandShaker.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/HandShaker.h"

// Rage headers
#include "bank/bank.h"
#include "math/simplemath.h"

// Framework headers
#include "fwmaths/random.h"
#include "fwsys/timer.h"

CAMERA_OPTIMISATIONS()

//TODO: Look to combine this functionality with the generic frame shaker.


camHandShaker::camHandShaker()
: m_RotationToApply(Quaternion::sm_I)
, m_Eulers(VEC3_ZERO)
, m_MaxRotationEulers(Vector3(0.02f, 0.001f, 0.01f))
, m_MaxVelocityDeltaEulers(Vector3(0.006f, 0.001f, 0.003f))
, m_VelocityEulers(VEC3_ZERO)
, m_DecelerationScalingEulers(Vector3(1.3f, 1.3f, 1.4f))
, m_MinReactionScaling(0.3f)
, m_MaxReactionScaling(1.0f)
, m_TwitchFrequency(1.5f)
, m_MaxTwitchSpeed(0.03f)
{
	Reset();
}

void camHandShaker::Reset()
{
	m_RotationToApply = Quaternion::sm_I;
	m_Eulers.Zero();
	m_VelocityEulers = Vector3(fwRandom::GetRandomNumberInRange(0.0f, m_MaxVelocityDeltaEulers.x),
		fwRandom::GetRandomNumberInRange(0.0f, m_MaxVelocityDeltaEulers.y),
		fwRandom::GetRandomNumberInRange(0.0f, m_MaxVelocityDeltaEulers.z));
}

void camHandShaker::Update(float level)
{
	//Derive a linear scale of reaction that is stronger the further the shake gets to the limit.
	Vector3 t;
	t.Abs(m_Eulers / m_MaxRotationEulers);
	Vector3 reactionScaling = Vector3(m_MinReactionScaling, m_MinReactionScaling, m_MinReactionScaling) +
		(t * (m_MaxReactionScaling - m_MinReactionScaling)); //Lerp.

	//If the rotation has the same sign as the velocity we have passed the centre and should slow down.
	if(SameSign(m_Eulers.x, m_VelocityEulers.x))
	{
		reactionScaling.x *= m_DecelerationScalingEulers.x;
	}
	if(SameSign(m_Eulers.y, m_VelocityEulers.y))
	{
		reactionScaling.y *= m_DecelerationScalingEulers.y;
	}
	if(SameSign(m_Eulers.z, m_VelocityEulers.z))
	{
		reactionScaling.z *= m_DecelerationScalingEulers.z;
	}

	//Now we know how much strength we want to apply we now apply some motion.
	Vector3 rotationDelta(fwRandom::GetRandomNumberInRange(0.0f, m_MaxVelocityDeltaEulers.x),
		fwRandom::GetRandomNumberInRange(0.0f, m_MaxVelocityDeltaEulers.y),
		fwRandom::GetRandomNumberInRange(0.0f, m_MaxVelocityDeltaEulers.z));
	rotationDelta *= reactionScaling;

	//Make sure the motion is in the right direction!
	if(m_Eulers.x > 0.0f)
	{
		rotationDelta.x *= -1.0f;
	}
	if(m_Eulers.y > 0.0f)
	{
		rotationDelta.y *= -1.0f;
	}
	if(m_Eulers.z > 0.0f)
	{
		rotationDelta.z *= -1.0f;
	}

	m_VelocityEulers += rotationDelta;

	//Apply a new twitch?
	float timeStep = fwTimer::GetCamTimeStep();
	float probabiltyOfTwitchThisFrame = m_TwitchFrequency * timeStep;
	if((probabiltyOfTwitchThisFrame >= 1.0f) || (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < probabiltyOfTwitchThisFrame))
	{
		m_VelocityEulers += Vector3(fwRandom::GetRandomNumberInRange(-m_MaxTwitchSpeed, m_MaxTwitchSpeed),
			fwRandom::GetRandomNumberInRange(-m_MaxTwitchSpeed, m_MaxTwitchSpeed),
			fwRandom::GetRandomNumberInRange(-m_MaxTwitchSpeed, m_MaxTwitchSpeed));
	}

	m_Eulers += m_VelocityEulers * timeStep;

	m_Eulers.Max(m_Eulers, -m_MaxRotationEulers);
	m_Eulers.Min(m_Eulers, m_MaxRotationEulers);

	m_RotationToApply.FromEulers(m_Eulers * level, "yxz"); //NOTE: YXZ rotation ordering is used to avoid aliasing of the roll.
}

//Make this shaker blend towards another, to avoid glitches.
void camHandShaker::BlendTo(const camHandShaker& target, float rateOfChange)
{
	m_MaxRotationEulers			+= (target.m_MaxRotationEulers - m_MaxRotationEulers)					* rateOfChange;
	m_MaxVelocityDeltaEulers	+= (target.m_MaxVelocityDeltaEulers - m_MaxVelocityDeltaEulers)			* rateOfChange;
	m_DecelerationScalingEulers	+= (target.m_DecelerationScalingEulers - m_DecelerationScalingEulers)	* rateOfChange;
	m_MinReactionScaling		+= (target.m_MinReactionScaling - m_MinReactionScaling)					* rateOfChange;
	m_MaxReactionScaling		+= (target.m_MaxReactionScaling - m_MaxReactionScaling)					* rateOfChange;
	m_TwitchFrequency			+= (target.m_TwitchFrequency - m_TwitchFrequency)						* rateOfChange;
	m_MaxTwitchSpeed			+= (target.m_MaxTwitchSpeed - m_MaxTwitchSpeed)							* rateOfChange;
}

void camHandShaker::ApplyShake(Vector3& front)
{
	m_RotationToApply.Transform(front);
}

void camHandShaker::ApplyShake(Matrix34& worldMatrix)
{
	//TODO: Derive a method to apply the quaternion directly to the world matrix.
	Matrix34 transformationMatrix;
	transformationMatrix.FromQuaternion(m_RotationToApply);
	worldMatrix.Dot3x3FromLeft(transformationMatrix);
}

#if __BANK
void camHandShaker::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Hand shaker", false);
	{
		bank.AddSlider("Max rotation (rad Eulers)", &m_MaxRotationEulers, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("Max velocity delta (rad/s Eulers)", &m_MaxVelocityDeltaEulers, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("Deceleration scaling (Eulers)", &m_DecelerationScalingEulers, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Min reaction scaling", &m_MinReactionScaling, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Max reaction scaling", &m_MaxReactionScaling, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Twitch frequency (Hz)", &m_TwitchFrequency, 0.0f, 100.0f, 0.01f);
		bank.AddSlider("Max twitch speed (rad/s)", &m_MaxTwitchSpeed, 0.0f, 1.0f, 0.001f);
	}
	bank.PopGroup();
}
#endif // __BANK
