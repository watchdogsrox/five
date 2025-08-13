// 
// audio/music/forceradiotrackaction.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"
#include "audio/radiostation.h"
#include "audio/scriptaudioentity.h"
#include "forceradiotrackaction.h"
#include "musicplayer.h"

AUDIO_MUSIC_OPTIMISATIONS()

audForceRadioTrackAction::audForceRadioTrackAction(const ForceRadioTrackAction *metadata)
: m_Metadata(metadata)
, m_RequestedTrack(0u)
, m_IsPrepared(false)
, m_HasFrozen(false)
{

}

void audForceRadioTrackAction::Trigger()
{
	if(!m_IsPrepared && (!g_InteractiveMusicManager.IsScoreMutedForRadio() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowForceRadioAfterRetune)))
	{
		NOTFINAL_ONLY(audWarningf("Triggered ForceRadioTrackAction %s without preparing", GetName());)
		Prepare();
	}

	if(m_HasFrozen)
	{
		g_RadioAudioEntity.SetAutoUnfreezeForPlayer(m_Metadata->DelayTime);
		if(AUD_GET_TRISTATE_VALUE(m_Metadata->Flags, FLAG_ID_FORCERADIOTRACKACTION_UNFREEZESTATION) == AUD_TRISTATE_TRUE)
		{
			audRadioStation *station = audRadioStation::FindStation(m_Metadata->Station);
			if(audVerifyf(station, "Failed to find station for ForceRadioTrackAction %s", GetName()))
			{
				// Double check that we're actually set up to play the track that we requested. If not, something has gone wrong - can occur if we
				// get a network radio sync request sometime between the Prepare() and Trigger() calls. 
				if(station->GetCurrentTrack().GetSoundRef() != m_RequestedTrack)
				{
					NOTFINAL_ONLY(audWarningf("ForceRadioTrackAction %s requested track %u on station %s, but it is set to play %u! Re-preparing...", GetName(), m_RequestedTrack, station->GetName(), station->GetCurrentTrack().GetSoundRef());)
					m_IsPrepared = false;
					Prepare();
				}
				
				NOTFINAL_ONLY(audDisplayf("%s unfreezing %s directly, current track %s", GetName(), station->GetName(), station->GetCurrentTrack().GetBankName());)
				station->Unfreeze();
			}
		}	
	}
}

audMusicAction::PrepareState audForceRadioTrackAction::Prepare()
{
	if(!m_IsPrepared && m_Metadata->numTrackLists > 0 && (!g_InteractiveMusicManager.IsScoreMutedForRadio() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowForceRadioAfterRetune)))
	{
		u32 nextIndex = m_Metadata->NextIndex;
		const_cast<ForceRadioTrackAction*>(m_Metadata)->NextIndex = (nextIndex + 1) % m_Metadata->numTrackLists;

		audRadioStation *station = audRadioStation::FindStation(m_Metadata->Station);
		if(audVerifyf(station, "Failed to find station for ForceRadioTrackAction %s", GetName()))
		{
			g_RadioAudioEntity.SetAutoUnfreezeForPlayer(false);
			station->Freeze();
			m_HasFrozen = true;
			const RadioTrackSettings *settings = audNorthAudioEngine::GetObject<RadioTrackSettings>(m_Metadata->TrackList[nextIndex].Track);
			if(audVerifyf(settings, "Failed to find track settings for ForceRadioTrackAction %s", GetName()))
			{
				m_RequestedTrack = settings->Sound;
				station->ForceTrack(settings);
			}
			if(g_RadioAudioEntity.IsPlayerInAVehicleWithARadio())
			{
				audDisplayf("audForceRadioTrackAction retuning vehicle radio to %s", station->GetName());
				g_RadioAudioEntity.RetuneToStation(station);
			}
			else
			{
				audDisplayf("audForceRadioTrackAction forcing player radio to %s", station->GetName());
				g_RadioAudioEntity.ForcePlayerRadioStation(station);
			}			
		}
	}

	m_IsPrepared = true;

	return audMusicAction::PREPARED;
}

void audForceRadioTrackAction::Cancel()
{
	if(m_HasFrozen)
	{
		g_RadioAudioEntity.SetAutoUnfreezeForPlayer(true);
	}
	// TODO: reset next track on station?
}

Implement_MusicActionName(audForceRadioTrackAction);
