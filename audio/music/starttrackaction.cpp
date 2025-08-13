// 
// audio/music/starttrackaction.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicplayer.h"
#include "starttrackaction.h"

AUDIO_MUSIC_OPTIMISATIONS()

bool audStartTrackAction::sm_HaveCheckedData = false;
bool audStartTrackAction::sm_AltSongsEnabled = true;

audStartTrackAction::audStartTrackAction(const StartTrackAction *metadata)
: m_Metadata(metadata)
{

}

void audStartTrackAction::Trigger()
{
	const audMetadataManager &metadataManager = audNorthAudioEngine::GetMetadataManager();
	if(audVerifyf(m_Metadata, "Trying to trigger a StartTrackAction with no metadata"))
	{		
		const u32 song = ChooseSong();
		if(audVerifyf(song, "StartTrackAction with invalid track: %s", metadataManager.GetObjectNameFromNameTableOffset(m_Metadata->NameTableOffset)))
		{			
			InteractiveMusicMood *mood = metadataManager.GetObject<InteractiveMusicMood>(m_Metadata->Mood);
			if(audVerifyf(mood, "StartTrackAction with invalid mood: %s", metadataManager.GetObjectNameFromNameTableOffset(m_Metadata->NameTableOffset)))
			{

				float startOffset = ShouldRandomizeStartOffset() ? audEngineUtil::GetRandomNumberInRange(0.f, 0.85f) 
																	: m_Metadata->StartOffsetScalar;

				g_InteractiveMusicManager.PlayLoopingTrack(song,
															mood,
															m_Metadata->VolumeOffset, 
															m_Metadata->FadeInTime, 
															m_Metadata->FadeOutTime, 
															startOffset,
															m_Metadata->DelayTime,
															ShouldOverrideRadio(),
															ShouldMuteRadioOffSound());
			}
		}
	}
}

bool audStartTrackAction::ShouldOverrideRadio() const
{
	return AUD_GET_TRISTATE_VALUE(m_Metadata->Flags, FLAG_ID_STARTTRACKACTION_OVERRIDERADIO) != AUD_TRISTATE_FALSE;
}

bool audStartTrackAction::ShouldMuteRadioOffSound() const
{
	return AUD_GET_TRISTATE_VALUE(m_Metadata->Flags, FLAG_ID_STARTTRACKACTION_MUTERADIOOFFSOUND) == AUD_TRISTATE_TRUE;
}

bool audStartTrackAction::ShouldRandomizeStartOffset() const
{
	return AUD_GET_TRISTATE_VALUE(m_Metadata->Flags, FLAG_ID_STARTTRACKACTION_RANDOMSTARTOFFSET) == AUD_TRISTATE_TRUE;
}

u32 audStartTrackAction::ChooseSong()
{
	// Check if the data has AltSongs
	if(!sm_HaveCheckedData)
	{
		sm_AltSongsEnabled = m_Metadata->LastSong == 1000;
		sm_HaveCheckedData = true;
	}

	if(!sm_AltSongsEnabled || m_Metadata->numAltSongs == 0)
	{
		return m_Metadata->Song;
	}

	atFixedArray<u32, 64> validSongs;


	const Vec3V s_CitySphereCentre(-1652.691406f, -1549.344238f, 0.f);
	const float s_CitySphereRadius = 3800.f;

	
	MusicArea currentArea = kMusicAreaCountry;
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		const float distToEmitter = Dist(player->GetTransform().GetPosition(), s_CitySphereCentre).Getf();
		if(distToEmitter < s_CitySphereRadius)
		{
			currentArea = kMusicAreaCity;
		}
	}
	
	if(m_Metadata->Song != 0)
	{
		validSongs.Append() = m_Metadata->Song;
	}

	for(u32 i = 0; i < m_Metadata->numAltSongs; i++)
	{
		if(m_Metadata->AltSongs[i].ValidArea == kMusicAreaEverywhere || m_Metadata->AltSongs[i].ValidArea == currentArea)
		{
			validSongs.Append() = m_Metadata->AltSongs[i].Song;
		}
	}

	if(!naVerifyf(validSongs.GetCount() != 0, "%s has no valid songs (current area: %s)", GetName(), MusicArea_ToString(currentArea)))
	{	
		return 0;
	}
	else if(validSongs.GetCount() == 1)
	{
		return validSongs[0];
	}
	else
	{	
		s32 index = 0;
		do 
		{
			index = audEngineUtil::GetRandomNumberInRange(0, validSongs.GetCount() - 1);
		} while (validSongs[index] == m_Metadata->LastSong);


		StartTrackAction *metadata = const_cast<StartTrackAction*>(m_Metadata);
		metadata->LastSong = validSongs[index];
	
		return validSongs[index];
	}
}

Implement_MusicActionName(audStartTrackAction);