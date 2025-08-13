// 
// audio/music/setmoodaction.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicplayer.h"
#include "setmoodaction.h"

#define OBJECT_NAME(x) (x ? metadataManager.GetObjectNameFromNameTableOffset(x->NameTableOffset) : NULL)

AUDIO_MUSIC_OPTIMISATIONS()

audSetMoodAction::audSetMoodAction(const SetMoodAction *metadata)
: m_Metadata(metadata)
{

}

void audSetMoodAction::Trigger()
{
	if(audVerifyf(m_Metadata, "Trying to trigger a SetMoodAction with no metadata"))
	{
		const audMetadataManager &metadataManager = audNorthAudioEngine::GetMetadataManager();

		InteractiveMusicMood *mood = metadataManager.GetObject<InteractiveMusicMood>(m_Metadata->Mood);
			
		float triggerTime = (g_InteractiveMusicManager.GetCurPlayTimeMs() * 0.001f) + m_Metadata->DelayTime;
		if(m_Metadata->numTimingConstraints > 0)
		{
			const float preemptTime = 0.06f;
			float constrainTimeOffset = 0.f;
			if(m_Metadata->Constrain == kConstrainEnd)
			{
				constrainTimeOffset = m_Metadata->FadeInTime * 0.001f;
			}
			triggerTime = g_InteractiveMusicManager.ComputeNextPlayTimeForConstraints(m_Metadata, constrainTimeOffset) - preemptTime;
		}

		g_InteractiveMusicManager.SetMood(mood,
											triggerTime,
											m_Metadata->VolumeOffset,
											m_Metadata->FadeInTime,
											m_Metadata->FadeOutTime);
	}
}