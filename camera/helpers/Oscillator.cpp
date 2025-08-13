//
// camera/helpers/Oscillator.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/Oscillator.h"

#include "fwmaths/angle.h"
#include "fwmaths/random.h"
#include "fwsys/timer.h"

#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camOscillator,0xFE8E27D9)


camOscillator::camOscillator(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camOscillatorMetadata&>(metadata))
, m_Phase((m_Metadata.m_Phase < 0.0f) ? fwRandom::GetRandomNumberInRange(0.0f, 1.0f) : m_Metadata.m_Phase)
, m_Decay(0.0f)
, m_Level(0.0f)
, m_UseGameTime(false)
, m_UseNonPausedCameraTime(false)
{
	m_Angle = 2.0f * PI * m_Phase;
	m_Angle = fwAngle::LimitRadianAngle(m_Angle);
}

float camOscillator::Update(float fFrequencyScale REPLAY_ONLY(, float timeScaleFactor, s32 timeOffset))
{
	float timestep			= (m_UseNonPausedCameraTime) ? fwTimer::GetNonPausableCamTimeStep() : (m_UseGameTime) ? fwTimer::GetTimeStep() : fwTimer::GetCamTimeStep();
#if GTA_REPLAY
	timestep				*= timeScaleFactor;
	timestep				*= (CReplayMgr::IsEditorActive() && CReplayMgr::GetCurrentPlayBackState().GetState(REPLAY_DIRECTION_BACK) != 0) ? -1.0f : 1.0f; // Reverse the timestep when we're rewinding in the replay editor, keeps shakes consistent

	float offsetDelta		= 2.0f * PI * (m_Metadata.m_Frequency * fFrequencyScale * ((float)timeOffset / 1000.0f));	
#endif
	float phaseDelta		= 2.0f * PI * (m_Metadata.m_Frequency * fFrequencyScale * timestep);
	m_Angle					+= phaseDelta;
	m_Angle					= fwAngle::LimitRadianAngleSafe(m_Angle);

	//TODO: Add more waveforms here.
	m_Level = 0.0f;
	float fDecay = 0.0f;
	switch (m_Metadata.m_Waveform)
	{
		case OSCILLATOR_WAVEFORM_SINEWAVE:
#if GTA_REPLAY			
			m_Level = m_Metadata.m_Amplitude * rage::Sinf(fwAngle::LimitRadianAngleSafe(m_Angle + offsetDelta));
#else
			m_Level = m_Metadata.m_Amplitude * rage::Sinf(m_Angle);
#endif
			break;

		case OSCILLATOR_WAVEFORM_DAMPED_SINEWAVE:
			if (m_Metadata.m_Frequency > SMALL_FLOAT)
			{
				float decayDelta		= 2.0f * PI * (timestep * fFrequencyScale / m_Metadata.m_Frequency);					// TODO: metadata? separate value
				m_Decay					+= decayDelta;
				m_Decay					= fwAngle::LimitRadianAngleSafe(m_Decay);

#if GTA_REPLAY
				fDecay = rage::Sinf(fwAngle::LimitRadianAngleSafe(m_Decay + offsetDelta)) * 0.50f + 0.50f;	// [0, 1]
#else
				fDecay = rage::Sinf(m_Decay) * 0.50f + 0.50f;	// [0, 1]
#endif				
				fDecay = exp(-m_Metadata.m_Decay * fDecay);

#if GTA_REPLAY
				m_Level = m_Metadata.m_Amplitude * fDecay * rage::Sinf(fwAngle::LimitRadianAngleSafe(m_Angle + offsetDelta));
#else
				m_Level = m_Metadata.m_Amplitude * fDecay * rage::Sinf(m_Angle);
#endif
			}
			break;

		default:
			break;
	}

	//Apply DC offset.
	m_Level += m_Metadata.m_DcOffset;

	return m_Level;
}
