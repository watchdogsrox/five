// 
// audio/music/startoneshotaction.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicplayer.h"
#include "startoneshotaction.h"
#include "fwsys/timer.h"

AUDIO_MUSIC_OPTIMISATIONS()

audStartOneShotAction::audStartOneShotAction(const StartOneShotAction *metadata)
: m_Metadata(metadata)
, m_PrepareTime(0)
, m_HasPrepared(false)
{

}

audMusicAction::PrepareState audStartOneShotAction::Prepare()
{
	if(!m_HasPrepared)
	{
		if(g_InteractiveMusicManager.IsOneshotActive())
		{
			audWarningf("Trying to prepare a one shot while one is active; %s.  Cancelling previously active one shot.", GetName());
			g_InteractiveMusicManager.StopOneShot(true);
		}

		bool loaded = g_InteractiveMusicManager.PreloadOneShot(m_Metadata->Sound, 
													m_Metadata->Predelay, 
													m_Metadata->FadeInTime, 
													m_Metadata->FadeOutTime);

		if(!loaded)
		{
			return audMusicAction::PREPARE_FAILED;
		}

		m_PrepareTime = fwTimer::GetSystemTimeInMilliseconds();
		m_HasPrepared = true;
	}
	return (g_InteractiveMusicManager.IsOneShotPrepared() ? audMusicAction::PREPARED : audMusicAction::PREPARING);
}

void audStartOneShotAction::Trigger()
{
	if(m_HasPrepared)
	{
		const u32 prepareDuration = fwTimer::GetSystemTimeInMilliseconds() - m_PrepareTime;
		if(prepareDuration < 5000U)
		{
			audWarningf("Triggering StartOneShotEvent: %s, only had %.2f seconds to prepare (should always be >5!)", GetName(), prepareDuration * 0.001f);
		}
		float triggerTime = (g_InteractiveMusicManager.GetCurPlayTimeMs() * 0.001f) + m_Metadata->DelayTime;
		if(m_Metadata->numTimingConstraints > 0)
		{
			float constrainTimeOffset = 0.f;
			if(m_Metadata->Constrain == kConstrainEnd)
			{
				constrainTimeOffset = g_InteractiveMusicManager.GetOneShotLength();
			}
			triggerTime = g_InteractiveMusicManager.ComputeNextPlayTimeForConstraints(m_Metadata, constrainTimeOffset);
		}
		audAssertf(m_Metadata->Sound == g_InteractiveMusicManager.GetActiveOneShotHash(), "Triggering event %s; another one shot has been prepared", GetName());
		if(triggerTime < 0.f)
		{
			g_InteractiveMusicManager.PlayPreloadedOneShot();
		}
		else
		{
			g_InteractiveMusicManager.PlayTimedOneShot(triggerTime);
		}
	}
	else
	{
		audWarningf("Triggering a one shot that wasn't preloaded: %s", GetName());

		if(g_InteractiveMusicManager.IsOneshotActive())
		{
			audWarningf("Trying to prepare a one shot while one is active; %s.  Cancelling previously active one shot.", GetName());
			g_InteractiveMusicManager.StopOneShot(true);
		}

		g_InteractiveMusicManager.PreloadOneShot(m_Metadata->Sound, 
			m_Metadata->Predelay, 
			m_Metadata->FadeInTime, 
			m_Metadata->FadeOutTime,
			true);
	}

	g_InteractiveMusicManager.SetUnfreezeRadioOnOneShot(ShouldUnfreezeRadio());
}

void audStartOneShotAction::Cancel()
{
	if(m_HasPrepared)
	{
		// This is only valid before the sound has been played, so force 0ms release time
		g_InteractiveMusicManager.StopOneShot(true);
		m_HasPrepared = false;
	}
}

#if __FINAL && !__NO_OUTPUT
const char *audStartOneShotAction::GetName() const { return ""; }
#else
Implement_MusicActionName(audStartOneShotAction);
#endif
