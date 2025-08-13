// 
// audio/music/djspeechaction.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"
#include "audio/radiostation.h"
#include "musicplayer.h"
#include "djspeechaction.h"
#include "fwsys/timer.h"

AUDIO_MUSIC_OPTIMISATIONS()

audRadioDjSpeechAction::audRadioDjSpeechAction(const RadioDjSpeechAction *metadata)
: m_Metadata(metadata)
, m_HasPrepared(false)
{

}

audMusicAction::PrepareState audRadioDjSpeechAction::Prepare()
{
	u32 variationNum = 0;
	u32 speechLengthMs = 0;

	audRadioStation *station = audRadioStation::FindStation(m_Metadata->Station);
	if(audVerifyf(station, "Failed to find station for RadioDjSpeechAction %s", GetName()))
	{
		if(!m_HasPrepared)
		{
			station->ChooseDjSpeechVariation(m_Metadata->Category, variationNum, speechLengthMs, true);
			if(speechLengthMs)
			{
				//@@: location AUDRADIODJSPEECHACTIONAUDRADIODJSPEECHACTIONPREPARE
				station->PrepareIMDrivenDjSpeech(m_Metadata->Category, variationNum);
				m_HasPrepared = true;
			}
			else
			{
				NOTFINAL_ONLY(audWarningf("RadioDjSpeechAction %s failed to choose variation", GetName());)
				return audMusicAction::PREPARE_FAILED;
			}
		}

		return station->IsDjSpeechPrepared() ? audMusicAction::PREPARED : audMusicAction::PREPARING;
	}
	return audMusicAction::PREPARE_FAILED;
}

void audRadioDjSpeechAction::Trigger()
{
	naAssertf(g_InteractiveMusicManager.IsMusicPlaying(), "Attempting to trigger DjSpeechAction %s without score playing", GetName());
	if(!g_InteractiveMusicManager.IsScoreMutedForRadio() && g_RadioAudioEntity.IsPlayerInAVehicleWithARadio() && g_InteractiveMusicManager.IsMusicPlaying())
	{
		if(!m_HasPrepared)
		{
			NOTFINAL_ONLY(audWarningf("%s - Triggering DJ speech without preparing", GetName());)
			Prepare();
		}
		audRadioStation *station = audRadioStation::FindStation(m_Metadata->Station);
		if(audVerifyf(station, "Failed to find station for RadioDjSpeechAction %s", GetName()))
		{
			u32 triggerTime = g_InteractiveMusicManager.GetCurPlayTimeMs() + u32(m_Metadata->DelayTime * 1000.f);
			if(m_Metadata->numTimingConstraints > 0)
			{
				triggerTime = (u32)(1000.f * g_InteractiveMusicManager.ComputeNextPlayTimeForConstraints(m_Metadata));
			}
			audRadioStation::TriggerIMDrivenDjSpeech(triggerTime);
		}
	}
	else if(m_HasPrepared)
	{
		Cancel();
	}
}

void audRadioDjSpeechAction::Cancel()
{
	if(m_HasPrepared)
	{	
		audRadioStation *station = audRadioStation::FindStation(m_Metadata->Station);
		if(audVerifyf(station, "Failed to find station for RadioDjSpeechAction %s", GetName()))
		{
			station->CancelIMDrivenDjSpeech();
		}

		m_HasPrepared = false;
	}
}

Implement_MusicActionName(audRadioDjSpeechAction);

