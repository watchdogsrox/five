// 
// audio/music/fadeinradioaction.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"

#include "fadeinradioaction.h"
#include "musicplayer.h"

AUDIO_MUSIC_OPTIMISATIONS()

audFadeInRadioAction::audFadeInRadioAction(const FadeInRadioAction *metadata)
: m_Metadata(metadata)
{

}

void audFadeInRadioAction::Trigger()
{
	if(!g_InteractiveMusicManager.IsScoreMutedForRadio())
	{
		NOTFINAL_ONLY(audDisplayf("%s - fading in radio", GetName());)
		g_RadioAudioEntity.StartFadeIn(m_Metadata->DelayTime, m_Metadata->FadeTime);
	}
	else
	{
		NOTFINAL_ONLY(audDisplayf("Ignoring FadeInRadioAction (%s) because score is muted for radio", GetName());)
	}
}

Implement_MusicActionName(audFadeInRadioAction);
