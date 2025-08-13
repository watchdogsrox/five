//
// camera/helpers/Envelope.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/Envelope.h"

#include "math/amath.h"
#include "math/simplemath.h"

#include "camera/scripted/BaseSplineCamera.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camEnvelope,0xC93E3D5)

const u32	g_MaxBinarySearchIterationsForInverseAttackResponse	= 20;
const float	g_MaxErrorForInverseAttackResponse					= 0.001f;
const u32	INVALID_DURATION = UINT_MAX;

camEnvelope::camEnvelope(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camEnvelopeMetadata&>(metadata))
, m_Phase(PREDELAY_PHASE)
, m_PhaseStartTime(0)
, m_EnvelopeStartTime(0)
, m_OverrideStartTime(0)
, m_OverrideAttackDuration(INVALID_DURATION)
, m_OverrideHoldDuration(INVALID_DURATION)
, m_Level(0.0f)
, m_MaxReleaseLevel(m_Metadata.m_SustainLevel)
, m_IsActive(false)
, m_UseGameTime(false)
, m_UseNonPausedCameraTime(false)
, m_IsReversible(false)
{
}

void camEnvelope::AutoStartStop(bool shouldStart, bool shouldWaitForHoldOnRelease)
{
	if(shouldStart)
	{
		//Only start the envelope if it's inactive or currently releasing.
		if(!m_IsActive || (m_Phase == RELEASE_PHASE))
		{
			Start(m_Level);
		}
	}
	else if(!shouldWaitForHoldOnRelease || (m_Phase == HOLD_PHASE))
	{
		Release();
	}
}

void camEnvelope::AutoStart()
{
	//Only start the envelope if it's inactive or currently releasing.
	if(!m_IsActive || (m_Phase == RELEASE_PHASE))
	{
		Start(m_Level);
	}
	else if(m_Phase == HOLD_PHASE)
	{
		//Reset the hold start time, as we don't want to deplete a finite hold when repeatedly calling auto-start. This is analogous to a
		//sustained key-press.
		m_PhaseStartTime = GetCurrentTimeInMilliseconds();
	}
}

void camEnvelope::Start(float initialLevel)
{
	m_Level				= Clamp(initialLevel, 0.0f, 1.0f);
	m_PhaseStartTime	= (m_OverrideStartTime > 0) ? m_OverrideStartTime : GetCurrentTimeInMilliseconds();
	m_MaxReleaseLevel	= m_Metadata.m_SustainLevel;
	m_IsActive			= true;

	if(m_Level >= SMALL_FLOAT)
	{
		//Jump to the attack phase and compute a start time that would produce this initial level.
		m_Phase = ATTACK_PHASE;

		const float normalisedLevel	= (m_Metadata.m_DecayDuration > 0) ? m_Level :
										RampValueSafe(m_Level, 0.0f, m_Metadata.m_SustainLevel, 0.0f, 1.0f);
		const float phaseProgress	= CalculateInverseAttackResponse(normalisedLevel);

		u32 uAttackDuration = (m_OverrideAttackDuration == INVALID_DURATION) ? m_Metadata.m_AttackDuration : m_OverrideAttackDuration;
		const u32 timeSinceStart	= (u32)Floorf(phaseProgress * (float)uAttackDuration);
		m_PhaseStartTime			-= timeSinceStart;
	}
	else
	{
		m_Phase = PREDELAY_PHASE;
	}

	m_EnvelopeStartTime = m_PhaseStartTime;
}

void camEnvelope::OverrideStartTime(u32 startTime)
{
	m_OverrideStartTime = startTime;
	m_EnvelopeStartTime = startTime;
	m_PhaseStartTime = startTime;
}

float camEnvelope::Update(REPLAY_ONLY(float timeScaleFactor, s32 timeOffset))
{
	m_Level = 0.0f;

	if(!m_IsActive)
	{
		return 0.0f;
	}

	if (m_IsReversible)
	{
		return UpdateReversible(REPLAY_ONLY(timeScaleFactor, timeOffset));
	}

#if GTA_REPLAY
	s32 time = GetCurrentTimeInMilliseconds();
	time += timeOffset;
#else
	const u32 time = GetCurrentTimeInMilliseconds();
#endif

	switch(m_Phase)
	{
	case PREDELAY_PHASE:
		if(m_Metadata.m_PreDelayDuration > 0)
		{
			float phaseProgress = ((float)(time - m_PhaseStartTime) REPLAY_ONLY( * timeScaleFactor)) / (float)m_Metadata.m_PreDelayDuration;
			phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

			if(phaseProgress < 1.0f)
			{
				break;
			}
		}

		//End of phase:
		m_Phase = ATTACK_PHASE;
		m_PhaseStartTime = time;
		//Intentional fall-through.

	case ATTACK_PHASE:
		{
			u32 uAttackDuration = (m_OverrideAttackDuration == INVALID_DURATION) ? m_Metadata.m_AttackDuration : m_OverrideAttackDuration;
			if(uAttackDuration > 0)
			{
				float phaseProgress = ((float)(time - m_PhaseStartTime) REPLAY_ONLY( * timeScaleFactor)) / (float)uAttackDuration;
				phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

				if(phaseProgress < 1.0f)
				{
					//NOTE: We only attack to a target level of 1.0 with decay, otherwise we just attack to the sustain level, avoiding any discontinuity.
					m_Level = CalculateAttackResponse(phaseProgress); 
					break;
				}
			}
		}

		//End of phase:
		m_Phase = DECAY_PHASE;
		m_PhaseStartTime = time;
		//Intentional fall-through.

	case DECAY_PHASE:
		m_OverrideAttackDuration = INVALID_DURATION;
		if(m_Metadata.m_DecayDuration > 0)
		{
			float phaseProgress = ((float)(time - m_PhaseStartTime) REPLAY_ONLY( * timeScaleFactor)) / (float)m_Metadata.m_DecayDuration;
			phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

			if(phaseProgress < 1.0f)
			{
				m_Level = Lerp(phaseProgress, 1.0f, m_Metadata.m_SustainLevel);
				break;
			}
		}

		//End of phase:
		m_Phase = HOLD_PHASE;
		m_PhaseStartTime = time;
		//Intentional fall-through.

	case HOLD_PHASE:
		m_OverrideAttackDuration = INVALID_DURATION;
		{
			s32 uHoldDuration = (m_OverrideHoldDuration == INVALID_DURATION) ? m_Metadata.m_HoldDuration : m_OverrideHoldDuration;
			if(uHoldDuration < 0)
			{
				//Infinite hold phase, allowing an endless oscillation to be requested.
				m_Level = m_Metadata.m_SustainLevel;
				break;
			}
			else if(uHoldDuration > 0)
			{
				float phaseProgress = ((float)(time - m_PhaseStartTime) REPLAY_ONLY( * timeScaleFactor)) / (float)uHoldDuration;
				phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

				if(phaseProgress < 1.0f)
				{
					m_Level = m_Metadata.m_SustainLevel;
					break;
				}
			}
		}

		//End of phase:
		m_Phase = RELEASE_PHASE;
		m_PhaseStartTime = time;
		//Intentional fall-through.

	case RELEASE_PHASE:
		m_OverrideAttackDuration = INVALID_DURATION;
		m_OverrideHoldDuration = INVALID_DURATION;
		if(m_Metadata.m_ReleaseDuration > 0)
		{
			float phaseProgress = ((float)(time - m_PhaseStartTime) REPLAY_ONLY( * timeScaleFactor)) / (float)m_Metadata.m_ReleaseDuration;
			phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

			if(phaseProgress < 1.0f)
			{
				m_Level = CalculateReleaseResponse(phaseProgress); 	
				break;
			}
		}

		//End of phase:
		m_IsActive = false; //We are done releasing, so deactivate.
	}

	return m_Level;
}

float camEnvelope::UpdateReversible(REPLAY_ONLY(float timeScaleFactor, s32 timeOffset))
{
#if GTA_REPLAY
	const s32 time = GetCurrentTimeInMilliseconds() + timeOffset;
#else
	const u32 time = GetCurrentTimeInMilliseconds();
#endif

	// NOTE: PhaseStartTime = start time of the whole envelope in reversible mode.
	const u32 envelopeStartTime = m_EnvelopeStartTime;
	if (time < envelopeStartTime)
	{
		return 0.0f;
	}
	
#if GTA_REPLAY
	const s32 envelopeTime = (u32)(Floorf((float)(time - envelopeStartTime) * timeScaleFactor));
#else
	const u32 envelopeTime = time - envelopeStartTime;
#endif

	const u32 preDelayEnd = m_Metadata.m_PreDelayDuration;
	const u32 attackDuration = (m_OverrideAttackDuration == INVALID_DURATION) ? m_Metadata.m_AttackDuration : m_OverrideAttackDuration;
	const u32 attackEnd = preDelayEnd+attackDuration;
	const u32 decayEnd = attackEnd + m_Metadata.m_DecayDuration;
	const s32 holdDuration = (m_OverrideHoldDuration == INVALID_DURATION) ? m_Metadata.m_HoldDuration : m_OverrideHoldDuration;
	const u32 holdEnd = holdDuration<0 ? MAX_UINT32 : decayEnd + holdDuration;
	const u32 releaseEnd = holdEnd + m_Metadata.m_ReleaseDuration;

	if (envelopeTime < preDelayEnd)
	{
		m_Phase = PREDELAY_PHASE;
		m_PhaseStartTime = m_EnvelopeStartTime;
		// just wait - nothing to see here
	}
	else if (envelopeTime < attackEnd)
	{
		m_Phase = ATTACK_PHASE;
		m_PhaseStartTime = preDelayEnd;
		float phaseProgress = (float)(envelopeTime - m_PhaseStartTime) / (float)attackDuration;
		phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

		if(phaseProgress < 1.0f)
		{
			//NOTE: We only attack to a target level of 1.0 with decay, otherwise we just attack to the sustain level, avoiding any discontinuity.
			m_Level = CalculateAttackResponse(phaseProgress); 
		}
	}
	else if (envelopeTime < decayEnd)
	{
		m_Phase = DECAY_PHASE;
		m_PhaseStartTime = attackEnd;
		float phaseProgress = (float)(envelopeTime - m_PhaseStartTime) / (float)m_Metadata.m_DecayDuration;
		phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

		if(phaseProgress < 1.0f)
		{
			m_Level = Lerp(phaseProgress, 1.0f, m_Metadata.m_SustainLevel);
		}

	}
	else if (envelopeTime < holdEnd)
	{
		m_Phase = HOLD_PHASE;
		m_PhaseStartTime = decayEnd;
		if(holdDuration < 0)
		{
			//Infinite hold phase, allowing an endless oscillation to be requested.
			m_Level = m_Metadata.m_SustainLevel;
		}
		else if(holdDuration > 0)
		{
			float phaseProgress = (float)(envelopeTime - m_PhaseStartTime) / (float)holdDuration;
			phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

			if(phaseProgress < 1.0f)
			{
				m_Level = m_Metadata.m_SustainLevel;
			}
		}
	}
	else if (envelopeTime < releaseEnd)
	{
		m_Phase = RELEASE_PHASE;
		m_PhaseStartTime = holdEnd;
		float phaseProgress = (float)(envelopeTime - m_PhaseStartTime) / (float)m_Metadata.m_ReleaseDuration;
		phaseProgress = Clamp(phaseProgress, 0.0f, 1.0f);

		if(phaseProgress < 1.0f)
		{
			m_Level = CalculateReleaseResponse(phaseProgress); 	
		}
	}
	else
	{
		//m_IsActive = false; //Don't end automatically in reversible mode, as we want to be able to scrub off the end of the envelope and back again.
	}

	return m_Level;
}

float camEnvelope::CalculateReleaseResponse(float phaseProgress) const
{
	float baseLevel = 0.0f; 

	switch(m_Metadata.m_ReleaseCurveType)
	{
	case CURVE_TYPE_LINEAR:
		{
			baseLevel = 1.0f - phaseProgress;
		}
		break;
	
	case CURVE_TYPE_SLOW_IN:
		{
			baseLevel = 1.0f - SlowIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_SLOW_OUT:
		{
			baseLevel = 1.0f - SlowOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_SLOW_IN_OUT:
		{
			baseLevel = 1.0f - SlowInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN:
		{
			baseLevel = 1.0f - SlowIn(SlowIn(phaseProgress));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_OUT:
		{
			baseLevel = 1.0f - SlowOut(SlowOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_SLOW_OUT:
		{
			baseLevel = 1.0f - SlowIn(SlowInOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_SLOW_IN_VERY_SLOW_OUT:
		{
			baseLevel = 1.0f - SlowOut(SlowInOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			baseLevel = 1.0f - SlowInOut(SlowInOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_EASE_IN:
		{
			baseLevel = 1.0f - camBaseSplineCamera::EaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_EASE_OUT:
		{
			baseLevel = 1.0f - camBaseSplineCamera::EaseIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_OUT:
		{
			baseLevel = 1.0f - QuadraticEaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_IN_OUT:
		{		
			baseLevel = 1.0f - QuadraticEaseInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN:
		{
			baseLevel = 1.0f - CubicEaseIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_OUT:
		{
			baseLevel = 1.0f - CubicEaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN_OUT:
		{		
			baseLevel = 1.0f - CubicEaseInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN:
		{
			baseLevel = 1.0f - QuarticEaseIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_OUT:
		{
			baseLevel = 1.0f - QuarticEaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN_OUT:
		{
			baseLevel = 1.0f - QuarticEaseInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN:
		{
			baseLevel = 1.0f - QuinticEaseIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_OUT:
		{
			baseLevel = 1.0f - QuinticEaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN_OUT:
		{
			baseLevel = 1.0f - QuinticEaseInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN:
		{
			baseLevel = 1.0f - CircularEaseIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_OUT:
		{
			baseLevel = 1.0f - CircularEaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN_OUT:
		{
			baseLevel = 1.0f - CircularEaseInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_EXPONENTIAL:
	default:
		{
			//Make the release an exponential (rather than linear) decay, as this looks better.
			//NOTE: exp(-7x) gives a result of less than 0.001 at x=1.0.
			baseLevel = exp(-7.0f * phaseProgress);
		}
		break;
	}

	const float level = baseLevel * m_MaxReleaseLevel;

	return level; 
}

float camEnvelope::CalculateAttackResponse(float phaseProgress) const
{
	float baseLevel = 0.0f; 

	switch(m_Metadata.m_AttackCurveType)
	{
	case CURVE_TYPE_SLOW_IN:
		{
			baseLevel = SlowIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_SLOW_OUT:
		{
			baseLevel = SlowOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_SLOW_IN_OUT:
		{
			baseLevel = SlowInOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN:
		{
			baseLevel = SlowIn(SlowIn(phaseProgress));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_OUT:
		{
			baseLevel = SlowOut(SlowOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_SLOW_OUT:
		{
			baseLevel = SlowIn(SlowInOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_SLOW_IN_VERY_SLOW_OUT:
		{
			baseLevel = SlowOut(SlowInOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			baseLevel = SlowInOut(SlowInOut(phaseProgress));
		}
		break;

	case CURVE_TYPE_EASE_IN:
		{
			baseLevel = camBaseSplineCamera::EaseOut(phaseProgress);
		}
		break;

	case CURVE_TYPE_EASE_OUT:
		{
			baseLevel = camBaseSplineCamera::EaseIn(phaseProgress);
		}
		break;

	case CURVE_TYPE_EXPONENTIAL:
		{
			//Make the release an exponential (rather than linear) decay, as this looks better.
			//NOTE: exp(-7x) gives a result of less than 0.001 at x=1.0.
			baseLevel = 1.0f - exp(-7.0f * phaseProgress);
		}
		break;

	case CURVE_TYPE_LINEAR:
	default:
		{
			baseLevel = phaseProgress;
		}
		break;
	}

	const float targetLevel	= (m_Metadata.m_DecayDuration > 0) ? 1.0f : m_Metadata.m_SustainLevel;
	const float level		= baseLevel * targetLevel;

	return level; 
}

float camEnvelope::CalculateInverseAttackResponse(float desiredLevel) const
{
	//Binary-search for the phase that will generate the desired level.

	float phase					= 0.5f;
	float binarySearchStepSize	= 0.25f;

	// B* 2060929 If the previous desired level was 1, initialise the phase to 1.
	// Without this the binary search below may tend to return a phase less than 1 as it finds a phase close
	// enough to the error check prior to 1, this probably is more noticeable on slow out curves who tend slowly to 1 at the top end
	if( m_Metadata.m_UseSpecialCaseEnvelopeReset && IsClose(desiredLevel, 1.0f, SMALL_FLOAT ) )
	{
		return 1.0f;
	}

	for(u32 searchIteration=0; searchIteration<g_MaxBinarySearchIterationsForInverseAttackResponse; searchIteration++)
	{
		const float level = CalculateAttackResponse(phase);

		if(IsClose(level, desiredLevel, g_MaxErrorForInverseAttackResponse))
		{
			break;
		}

		phase += (level > desiredLevel) ? -binarySearchStepSize : binarySearchStepSize;

		binarySearchStepSize *= 0.5f;
	}

	return phase;
}

void camEnvelope::Release()
{
	if(m_IsActive && (m_Phase != RELEASE_PHASE))
	{
		if (m_IsReversible)
		{
			// In reversible mode, we need to set the actual length we were in the hold section for reversing purposes.
			m_OverrideHoldDuration = GetCurrentTimeInMilliseconds() - m_PhaseStartTime;
		}

		m_Phase = RELEASE_PHASE;
		m_PhaseStartTime = GetCurrentTimeInMilliseconds();

		//Make sure the release starts from the current envelope level and does not snap to the sustain level.
		m_MaxReleaseLevel = m_Level;
	}
}

void camEnvelope::Stop()
{
	m_Phase				= RELEASE_PHASE;
	m_PhaseStartTime	= 0;
	m_Level				= 0.0f;
	m_MaxReleaseLevel	= m_Metadata.m_SustainLevel;
	m_IsActive			= false;
}

u32 camEnvelope::GetAttackDuration() const
{
	if (m_OverrideAttackDuration == INVALID_DURATION)
	{
		return m_Metadata.m_AttackDuration;
	}

	return m_OverrideAttackDuration;
}

u32 camEnvelope::GetHoldDuration() const
{
	if (m_OverrideHoldDuration == INVALID_DURATION)
	{
		return m_Metadata.m_HoldDuration;
	}

	return m_OverrideHoldDuration;
}

u32 camEnvelope::GetReleaseDuration() const
{
	return m_Metadata.m_ReleaseDuration;
}
