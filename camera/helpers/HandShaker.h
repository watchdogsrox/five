//
// camera/helpers/HandShaker.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAM_HAND_SHAKER_H
#define CAM_HAND_SHAKER_H

#include "vector/quaternion.h"

namespace rage
{
	class bkBank;
	class Matrix34;
}

//A basic simulation of the shaking of a human hand holding a camera.
class camHandShaker
{
public:
	camHandShaker();

	const Vector3& GetMaxRotationEulers()			{ return m_MaxRotationEulers; }
	const Vector3& GetMaxVelocityDeltaEulers()		{ return m_MaxVelocityDeltaEulers; }
	const Vector3& GetDecelerationScalingEulers()	{ return m_DecelerationScalingEulers; }
	float GetMinReactionScaling()					{ return m_MinReactionScaling; }
	float GetMaxReactionScaling()					{ return m_MaxReactionScaling; }
	float GetTwitchFrequency()						{ return m_TwitchFrequency; }
	float GetMaxTwitchSpeed()						{ return m_MaxTwitchSpeed; }

	void SetMaxRotationEulers(const Vector3& maxRotationEulers)					{ m_MaxRotationEulers			= maxRotationEulers; }
	void SetMaxVelocityDeltaEulers(const Vector3& maxVelocityDeltaEulers)		{ m_MaxVelocityDeltaEulers		= maxVelocityDeltaEulers; }
	void SetDecelerationScalingEulers(const Vector3& decelerationScalingEulers)	{ m_DecelerationScalingEulers	= decelerationScalingEulers; }
	void SetMinReactionScaling(float minReactionScaling)						{ m_MinReactionScaling			= minReactionScaling; }
	void SetMaxReactionScaling(float maxReactionScaling)						{ m_MaxReactionScaling			= maxReactionScaling; }
	void SetTwitchFrequency(float twitchFrequency)								{ m_TwitchFrequency				= twitchFrequency; }
	void SetMaxTwitchSpeed(float maxTwitchSpeed)								{ m_MaxTwitchSpeed				= maxTwitchSpeed; }

	void Reset();
	void Update(float level);
	void BlendTo(const camHandShaker& target, float rateOfChange);

	void ApplyShake(Vector3& front);
	void ApplyShake(Matrix34& worldMatrix);

#if __BANK
	void AddWidgets(bkBank& bank);
#endif // __BANK

private:
	Quaternion	m_RotationToApply;
	Vector3		m_Eulers;
	Vector3		m_MaxRotationEulers;
	Vector3		m_MaxVelocityDeltaEulers;
	Vector3		m_VelocityEulers;
	Vector3		m_DecelerationScalingEulers;
	float		m_MinReactionScaling;
	float		m_MaxReactionScaling;
	float		m_TwitchFrequency;
	float		m_MaxTwitchSpeed;
};

#endif // CAM_HAND_SHAKER_H
