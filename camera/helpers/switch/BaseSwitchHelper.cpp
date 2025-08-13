//
// camera/helpers/switch/BaseSwitchHelper.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/switch/BaseSwitchHelper.h"

#include "fwsys/timer.h"
#include "profile/group.h"
#include "profile/page.h"

#include "camera/helpers/ControlHelper.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseSwitchHelper,0x7FB0C46B)

PF_PAGE(camBaseSwitchHelperPage, "Camera: Base Switch Helper");

PF_GROUP(camBaseSwitchHelperMetrics);
PF_LINK(camBaseSwitchHelperPage, camBaseSwitchHelperMetrics);

PF_VALUE_FLOAT(switchPhase, camBaseSwitchHelperMetrics);
PF_VALUE_FLOAT(switchBlendLevel, camBaseSwitchHelperMetrics);


camBaseSwitchHelper::camBaseSwitchHelper(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camBaseSwitchHelperMetadata&>(metadata))
, m_DirectionSign(1.0f)
, m_PhaseNonClipped(0.0f)
, m_Phase(0.0f)
, m_BlendLevel(0.0f)
, m_IsPaused(false)
, m_IsFinished(false)
, m_HasTriggeredPostEffects(false)
, m_HasTriggeredFpsTransitionFlash(false)
, m_IsFirstPersonSwitch(false)
, m_FirstPersonCameraModeEnabled(false)

{
}

bool camBaseSwitchHelper::IsFinished() const
{
	const bool isFinished = m_Metadata.m_ShouldReportFinishedImmediately ? true : m_IsFinished;

	return isFinished;
}

void camBaseSwitchHelper::Update()
{
	UpdatePhase();

	const bool shouldBlendIn = ComputeShouldBlendIn();

	m_BlendLevel	= ComputeBlendLevel(m_Phase, m_Metadata.m_BlendCurveType, shouldBlendIn);
	m_IsFinished	= (m_Phase > (1.0f - SMALL_FLOAT));

	UpdatePostEffects();

	UpdateFpsTransitionEffect();	

	PF_SET(switchPhase, m_Phase);
	PF_SET(switchBlendLevel, m_BlendLevel);
}

void camBaseSwitchHelper::UpdatePhase()
{
	if(m_IsPaused)
	{
		return;
	}

	if(m_Metadata.m_Duration == 0)
	{
		m_PhaseNonClipped = m_Phase = 1.0f;

		return;
	}

	const float timeStep	= fwTimer::GetCamTimeStep();
	const float phaseDelta	= timeStep * 1000.0f / static_cast<float>(m_Metadata.m_Duration);
	m_PhaseNonClipped		+= phaseDelta;
	m_Phase					= Clamp(m_PhaseNonClipped, 0.0f, 1.0f);
}

float camBaseSwitchHelper::ComputeBlendLevel(float phase, s32 curveType, bool shouldBlendIn) const
{
	float blendLevel;

	if(!shouldBlendIn)
	{
		phase = 1.0f - phase;
	}

	phase = Clamp(phase, 0.0f, 1.0f);

	switch(curveType)
	{
	case CURVE_TYPE_EXPONENTIAL:
		{
			//Make the release an exponential (rather than linear) decay, as this looks better.
			//NOTE: exp(-7x) gives a result of less than 0.001 at x=1.0.
			blendLevel = exp(-7.0f * (1.0f - phase));
		}
		break;

	case CURVE_TYPE_SLOW_IN:
		{
			blendLevel = SlowIn(phase);
		}
		break;

	case CURVE_TYPE_SLOW_OUT:
		{
			blendLevel = SlowOut(phase);
		}
		break;

	case CURVE_TYPE_SLOW_IN_OUT:
		{
			blendLevel = SlowInOut(phase);
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN:
		{
			blendLevel = SlowIn(SlowIn(phase));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_OUT:
		{
			blendLevel = SlowOut(SlowOut(phase));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_SLOW_OUT:
		{
			blendLevel = SlowIn(SlowInOut(phase));
		}
		break;

	case CURVE_TYPE_SLOW_IN_VERY_SLOW_OUT:
		{
			blendLevel = SlowOut(SlowInOut(phase));
		}
		break;

	case CURVE_TYPE_VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			blendLevel = SlowInOut(SlowInOut(phase));
		}
		break;

	case CURVE_TYPE_EASE_IN:
		{
			blendLevel = camBaseSplineCamera::EaseOut(phase);
		}
		break;

	case CURVE_TYPE_EASE_OUT:
		{
			blendLevel = camBaseSplineCamera::EaseIn(phase);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_OUT:
		{
			blendLevel = QuadraticEaseOut(phase);
		}
		break;

	case CURVE_TYPE_QUADRATIC_EASE_IN_OUT:
		{		
			blendLevel = QuadraticEaseInOut(phase);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN:
		{
			blendLevel = CubicEaseIn(phase);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_OUT:
		{
			blendLevel = CubicEaseOut(phase);
		}
		break;

	case CURVE_TYPE_CUBIC_EASE_IN_OUT:
		{		
			blendLevel = CubicEaseInOut(phase);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN:
		{
			blendLevel = QuarticEaseIn(phase);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_OUT:
		{
			blendLevel = QuarticEaseOut(phase);
		}
		break;

	case CURVE_TYPE_QUARTIC_EASE_IN_OUT:
		{
			blendLevel = QuarticEaseInOut(phase);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN:
		{
			blendLevel = QuinticEaseIn(phase);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_OUT:
		{
			blendLevel = QuinticEaseOut(phase);
		}
		break;

	case CURVE_TYPE_QUINTIC_EASE_IN_OUT:
		{
			blendLevel = QuinticEaseInOut(phase);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN:
		{
			blendLevel = CircularEaseIn(phase);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_OUT:
		{
			blendLevel = CircularEaseOut(phase);
		}
		break;

	case CURVE_TYPE_CIRCULAR_EASE_IN_OUT:
		{
			blendLevel = CircularEaseInOut(phase);
		}
		break;

	case CURVE_TYPE_LINEAR:
	default:
		{
			blendLevel = phase;
		}
	}

	return blendLevel;
}

void camBaseSwitchHelper::UpdatePostEffects()
{
	if(m_HasTriggeredPostEffects || (m_Metadata.m_TimeToTriggerMidEffect < 0) || !g_PlayerSwitch.GetShortRangeMgr().IsActive())
	{
		return;
	}

	const u32 timeSinceStart = static_cast<u32>(Floorf(m_Phase * static_cast<float>(m_Metadata.m_Duration)));
	if(timeSinceStart < static_cast<u32>(m_Metadata.m_TimeToTriggerMidEffect))
	{
		return;
	}

	g_PlayerSwitch.GetShortRangeMgr().StartSwitchMidEffect();

	m_HasTriggeredPostEffects = true;
}

void camBaseSwitchHelper::UpdateFpsTransitionEffect()
{
	if(m_HasTriggeredFpsTransitionFlash)
	{
		return;
	}

	if (!m_FirstPersonCameraModeEnabled)
	{
		return;
	}

	bool isMpSwitch = NetworkInterface::IsGameInProgress();

	if ((isMpSwitch && !m_Metadata.m_ShouldTriggerFpsTransitionFlashMp) || (!isMpSwitch && !m_Metadata.m_ShouldTriggerFpsTransitionFlash))
	{
		return;
	}
	
	const u32 timeTillEnd = m_Metadata.m_Duration - static_cast<u32>(Floorf(m_Phase * static_cast<float>(m_Metadata.m_Duration)));
	if(timeTillEnd > 300u)
	{
		return;
	}

	camManager::TriggerFirstPersonFlashEffect(camManager::CAM_PUSH_IN_FX_NEUTRAL, 0u, "First person switch camera transition");

	m_HasTriggeredFpsTransitionFlash = true;
}

void camBaseSwitchHelper::ComputeDesiredMotionBlurStrength(float& desiredMotionBlurStrength) const
{
	float blendLevelToApply;
	if(m_Metadata.m_ShouldAttainDesiredMotionBlurStrengthMidDuration)
	{
		//Remap the phase from 0.0->1.0 to 0.0->1.0->0.0.
		float phaseToConsider = 2.0f * m_Phase;
		if(phaseToConsider >= 1.0f)
		{
			phaseToConsider = 2.0f - phaseToConsider;
		}

		blendLevelToApply = ComputeBlendLevel(phaseToConsider, m_Metadata.m_BlendCurveType);
	}
	else
	{
		blendLevelToApply = m_BlendLevel;
	}

	const float blendedMotionBlurStrength	= Lerp(blendLevelToApply, desiredMotionBlurStrength, m_Metadata.m_DesiredMotionBlurStrength);
	desiredMotionBlurStrength				= blendedMotionBlurStrength;
}

bool camBaseSwitchHelper::IsFirstPersonSwitch() const
{
	return m_IsFirstPersonSwitch;
}
