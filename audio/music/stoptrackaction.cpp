// 
// audio/music/stoptrackaction.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicplayer.h"
#include "stoptrackaction.h"

AUDIO_MUSIC_OPTIMISATIONS()

audStopTrackAction::audStopTrackAction(const StopTrackAction *metadata)
: m_Metadata(metadata)
{

}

void audStopTrackAction::Trigger()
{
	if(audVerifyf(m_Metadata, "Trying to trigger a StopTrackAction with no metadata"))
	{
		float triggerTime = m_Metadata->DelayTime <= 0.f ? -1.f : (g_InteractiveMusicManager.GetCurPlayTimeMs() * 0.001f) + m_Metadata->DelayTime;
		if(m_Metadata->numTimingConstraints > 0)
		{
			const float preemptTime = 0.06f;
			triggerTime = g_InteractiveMusicManager.ComputeNextPlayTimeForConstraints(m_Metadata) - preemptTime;
		}

		g_InteractiveMusicManager.SetUnfreezeRadioOnStop(ShouldUnfreezeRadio());
		g_InteractiveMusicManager.StopAndReset(m_Metadata->FadeTime, triggerTime);		
	}
}