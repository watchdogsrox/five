// 
// audio/music/fadeoutradioaction.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"

#include "fadeoutradioaction.h"
#include "musicplayer.h"

AUDIO_MUSIC_OPTIMISATIONS()

audFadeOutRadioAction::audFadeOutRadioAction(const FadeOutRadioAction *metadata)
: m_Metadata(metadata)
{

}

void audFadeOutRadioAction::Trigger()
{
	if(!g_InteractiveMusicManager.IsScoreMutedForRadio())
	{
		NOTFINAL_ONLY(audWarningf("%s - fading out radio", GetName());)
		g_RadioAudioEntity.StartFadeOut(m_Metadata->DelayTime, m_Metadata->FadeTime);
	}
	else
	{
		NOTFINAL_ONLY(audDisplayf("Ignoring FadeOutRadioAction (%s) because score is muted for radio", GetName());)
	}
}

Implement_MusicActionName(audFadeOutRadioAction);
