#ifndef PED_MOVE_BLEND_BASE_H
#define PED_MOVE_BLEND_BASE_H

#include "Peds/PedMotionData.h"


#define DEBUG_PED_ACCELERATION_CLAMPING 0

//**********************************************************************************************
// CMoveBlendRatioSpeeds
// This class stores the speed of movement of each move-blend ratio.
// Tasks and other systems use this to evaluate how fast a ped will travel at a given MBR.
// The CPedMoveBlendBase::GetMoveSpeeds() function must be able to fill this out when required.
// If a ped cannot move faster than any given MoveBlendRatio, then the successive move-states
// will need to be filled with the highest MBR they can achieve.
// For example, if a ped cannot move any faster than RUN then SetSprintSpeed() should still
// be set, but with the same speed as RUN.

class CMoveBlendRatioSpeeds
{
public:

	inline CMoveBlendRatioSpeeds() { Zero(); }

	inline void Zero() { m_fSpeeds[0] = m_fSpeeds[1] = m_fSpeeds[2] = m_fSpeeds[3] = 0.0f; }

	inline void SetWalkSpeed(const float s) { m_fSpeeds[1] = s; }
	inline void SetRunSpeed(const float s) { m_fSpeeds[2] = s; }
	inline void SetSprintSpeed(const float s) { m_fSpeeds[3] = s; }

	inline float GetWalkSpeed() const { return m_fSpeeds[1]; }
	inline float GetRunSpeed() const { return m_fSpeeds[2]; }
	inline float GetSprintSpeed() const { return m_fSpeeds[3]; }

	inline const float * GetSpeedsArray() const { return m_fSpeeds; }

	inline float GetMoveSpeed(float fMoveBlendRatio) const
	{
		if (fMoveBlendRatio<1.0f)
		{
			return m_fSpeeds[0] + ((m_fSpeeds[1] - m_fSpeeds[0])*fMoveBlendRatio);
		}
		else if (fMoveBlendRatio<2.0f)
		{
			return m_fSpeeds[1] + ((m_fSpeeds[2] - m_fSpeeds[1])*fMoveBlendRatio-1.0f);
		}
		else if (fMoveBlendRatio<3.0f)
		{
			return m_fSpeeds[2] + ((m_fSpeeds[3] - m_fSpeeds[2])*fMoveBlendRatio-2.0f);
		}
		return m_fSpeeds[3];
	}

private:

	float m_fSpeeds[4];
};


//**********************************************************************************************
// CMoveBlendRatioSpeeds
// This class stores the rate of turning (in radians per second) at each move blend ratio.
// Tasks and other systems use this to evaluate how quickly a ped can turn.
//**********************************************************************************************
class CMoveBlendRatioTurnRates
{
public:

	CMoveBlendRatioTurnRates() { Zero(); }

	void Zero()										{ m_fTurnRates[0] = m_fTurnRates[1] = m_fTurnRates[2] = 0.0f; }

	inline void SetWalkTurnRate(const float s)		{ m_fTurnRates[0] = s; }
	inline void SetRunTurnRate(const float s)		{ m_fTurnRates[1] = s; }
	inline void SetSprintTurnRate(const float s)	{ m_fTurnRates[2] = s; }

	inline float GetWalkTurnRate() const			{ return m_fTurnRates[0]; }
	inline float GetRunTurnRate() const				{ return m_fTurnRates[1]; }
	inline float GetSprintTurnRate() const			{ return m_fTurnRates[2]; }

	inline const float * GetTurnRateArray() const	{ return m_fTurnRates; }

	inline float GetTurnRate(float fMoveBlendRatio) const
	{
		if (fMoveBlendRatio <= MOVEBLENDRATIO_WALK)
		{

			return m_fTurnRates[0];
		}
		else if (fMoveBlendRatio <= MOVEBLENDRATIO_RUN)
		{
			return m_fTurnRates[1];
		}
		else
		{
			return m_fTurnRates[2];
		}
	}

private:

	float m_fTurnRates[3];
};

#endif
