/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayAudioTrackProvider.cpp
// PURPOSE : Class for listing and validating audio tracks for use in replays
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReplayAudioTrackProvider.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// game
#include "storage/Montage.h"
#include "audio/northaudioengine.h"
#if NA_RADIO_ENABLED
#include "audio/radioaudioentity.h"
#include "audio/radiotrack.h"
#endif

REPLAY_COORDINATOR_OPTIMISATIONS();

#if NA_RADIO_ENABLED

#if __BANK
extern bool g_ForceUnlockSolomun;
extern bool g_ForceUnlockTaleOfUs;
extern bool g_ForceUnlockDixon;
extern bool g_ForceUnlockBlackMadonna;
#endif

atRangeArray<CReplayRadioStationList, NUMBER_OF_REPLAY_MUSIC_TYPES> CReplayAudioTrackProvider::sm_RadioStationList;

CReplayAudioTrackProvider g_ReplayAudioTrackProvider;

u32 CReplayRadioStationTrack::GetTrackId() const
{
	return audRadioTrack::FindTextIdForSoundHash(m_SoundHash);
}

void CReplayRadioStation::Init(RadioStationSettings *stationSettings)
{ 
	m_StationSettings = stationSettings;
	m_NameHash = atStringHash(m_StationSettings->Name); 
}

void CReplayRadioStation::AddTrackList(const ReplayRadioStationTrackList *trackList )
{	
	bool wasTrackListReversed = false;

	// Make sure we're not reversed when adding new tracks
	if(m_ListReversed)
	{
		SetListReversed(false);
		wasTrackListReversed = true;
	}

	if(trackList) 
	{
		for(s32 i = 0; i < trackList->numTracks; i++)
		{
#if DISABLE_COMMERCIALS
			//! Skip any tracks that are commercials
			if( CReplayAudioTrackProvider::IsCommercial( trackList->Track[i].SoundRef ) )
			{
				continue;
			}
#endif
			bool found = false;
			for(s32 j = 0; j < m_RadioTrack.GetCount(); j++)
			{
				if(m_RadioTrack[j]->GetSoundHash() == trackList->Track[i].SoundRef)
				{
					found = true;
					break;
				}
			}
			if(!found)
			{
				CReplayRadioStationTrack* track = rage_new CReplayRadioStationTrack();
				if(track)
				{
					track->SetSoundHash(trackList->Track[i].SoundRef);
					m_RadioTrack.PushAndGrow(track);
					m_NumberRadioTracks++;
				}
			}
		}
	}

	// Re-reverse the list if required
	if(wasTrackListReversed)
	{
		SetListReversed(true);
	}
}

void CReplayRadioStation::SetListReversed(bool reversed)
{
	if(m_ListReversed != reversed)
	{
		u16 numTracks = static_cast<u16>(m_RadioTrack.GetCount());
		CReplayRadioStationTrack** data = Alloca(CReplayRadioStationTrack*, numTracks);
		atUserArray<CReplayRadioStationTrack*> tempArray(data, numTracks);
		tempArray.CopyFrom(m_RadioTrack.GetElements(), numTracks);
		m_RadioTrack.Reset();

		for(s32 loop = tempArray.GetCount() - 1; loop >= 0; loop--)
		{
			m_RadioTrack.PushAndGrow(tempArray[loop]);
		}

		m_ListReversed = reversed;
	}
}

void CReplayAudioTrackProvider::InitReplayRadioStations()
{
	char radioStationListName[64];

	// these tunables have been removed from the cloud, since we don't really need to ever turn them off we are hard coding it on.
	bool m_enableHalloween = true; //Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TURN_ON_HALLOWEEN_SOUNDS", 0xF4E5EC39), false); 
	bool solomunEnabled = true; //Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_SOL", 0x6268383A), solomunEnabled);
	bool taleOfUsEnabled = true; //Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_TOS", 0x1F91894), taleOfUsEnabled);
	bool dixonEnabled = true; // Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_DIX", 0x138F159C), dixonEnabled);	
	bool blackMadonnaEnabled = true; // Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_BM", 0x2E637DC9), blackMadonnaEnabled);	

#if __BANK
	solomunEnabled |= g_ForceUnlockSolomun;
	taleOfUsEnabled |= g_ForceUnlockTaleOfUs;
	dixonEnabled |= g_ForceUnlockDixon;
	blackMadonnaEnabled |= g_ForceUnlockBlackMadonna;
#endif
	
	audDisplayf("Halloween reply sounds are %s", m_enableHalloween ? "ON" : "OFF");
	audDisplayf("Solomun replay mixes are %s", taleOfUsEnabled ? "ON" : "OFF");
	audDisplayf("Tale of Us replay mixes are %s", taleOfUsEnabled ? "ON" : "OFF");
	audDisplayf("Dixon replay mixes are %s", dixonEnabled ? "ON" : "OFF");
	audDisplayf("Black Madonna replay mixes are %s", blackMadonnaEnabled ? "ON" : "OFF");

	// Used to filter out
	// REPL_AMBIENT_STATION_LIST_02
	// REPL_SFX_STATION_LIST_02

	for(u32 listIndex=0; listIndex<kMaxReplayRadioStationLists; listIndex++)
	{
		RadioStationList *radioStationList = NULL;

		formatf(radioStationListName, "REPL_MUSIC_STATION_LIST_%02u", listIndex + 1);

		if (listIndex != 1)
		{
			radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(radioStationListName);
			if (radioStationList)
			{
				Displayf("Adding station list %s to radio tracks", radioStationListName);
				MergeRadioStationList(REPLAY_RADIO_MUSIC, radioStationList);
			}
		}

		formatf(radioStationListName, "REPL_SCORE_STATION_LIST_%02u", listIndex + 1);

		radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(radioStationListName);
		if(radioStationList)
		{
			Displayf("Adding station list %s to score tracks", radioStationListName);
			MergeRadioStationList(REPLAY_SCORE_MUSIC, radioStationList);
		}

		if(listIndex != 1 || (listIndex == 1 && m_enableHalloween))
		{
			formatf(radioStationListName, "REPL_AMBIENT_STATION_LIST_%02u", listIndex + 1);

			radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(radioStationListName);
			if(radioStationList)
			{
				Displayf("Adding station list %s to ambient tracks", radioStationListName);
				MergeRadioStationList(REPLAY_AMBIENT_TRACK, radioStationList);
			}

			formatf(radioStationListName, "REPL_SFX_STATION_LIST_%02u", listIndex + 1);

			radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(radioStationListName);
			if(radioStationList)
			{
				Displayf("Adding station list %s to sfx oneshot tracks", radioStationListName);
				MergeRadioStationList(REPLAY_SFX_ONESHOT, radioStationList);
			}
		}
	}

	if (solomunEnabled)
	{
		RadioStationList *radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(ATSTRINGHASH("REPL_MUSIC_STATION_LIST_SOL", 0x49EA44DF));

		if (radioStationList)
		{
			Displayf("Adding station list REPL_MUSIC_STATION_LIST_SOL to radio tracks");
			MergeRadioStationList(REPLAY_RADIO_MUSIC, radioStationList, true);
		}
	}

	if (dixonEnabled)
	{
		RadioStationList *radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(ATSTRINGHASH("REPL_MUSIC_STATION_LIST_DIX", 0x46B72B17));

		if (radioStationList)
		{
			Displayf("Adding station list REPL_MUSIC_STATION_LIST_DIX to radio tracks");
			MergeRadioStationList(REPLAY_RADIO_MUSIC, radioStationList, true);
		}
	}

	if (taleOfUsEnabled)
	{
		RadioStationList *radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(ATSTRINGHASH("REPL_MUSIC_STATION_LIST_TOU", 0x8D0FEA6B));

		if (radioStationList)
		{
			Displayf("Adding station list REPL_MUSIC_STATION_LIST_TOU to radio tracks");
			MergeRadioStationList(REPLAY_RADIO_MUSIC, radioStationList, true);
		}
	}

	if (blackMadonnaEnabled)
	{
		RadioStationList *radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(ATSTRINGHASH("REPL_MUSIC_STATION_LIST_BM", 0xE71C8764));

		if (radioStationList)
		{
			Displayf("Adding station list REPL_MUSIC_STATION_LIST_BM to radio tracks");
			MergeRadioStationList(REPLAY_RADIO_MUSIC, radioStationList, true);
		}
	}	
}

void CReplayAudioTrackProvider::ReverseLists()
{
	u32 const c_radioStationListCount = GetNumReplayRadioStationLists();
	for( u32 listIndex = 0; listIndex < c_radioStationListCount; ++listIndex )
	{
		CReplayRadioStationList* replayRadioStationList = GetReplayRadioStationList( listIndex );
		if(replayRadioStationList)
		{
			for(u32 stationIndex = 0; stationIndex < replayRadioStationList->m_NumRadioStations; ++stationIndex )
			{
				replayRadioStationList->m_RadioStation[stationIndex]->SetListReversed( 
					!replayRadioStationList->m_RadioStation[stationIndex]->IsListReversed() );
			}			
		}
	}
}

void CReplayAudioTrackProvider::MergeRadioStationList(eReplayMusicType musicType, RadioStationList *radioStationList, bool isUnlockableClubStation)
{
	for(u32 stationIndex = 0; stationIndex < radioStationList->numStations; stationIndex++)
	{
		RadioStationSettings *stationSettings = 
			audNorthAudioEngine::GetObject<RadioStationSettings>(radioStationList->Station[stationIndex].StationSettings);

		CReplayRadioStation *station = FindStation(musicType, stationSettings->Name);

		// Unlockable club tracks just get added to the same station, so loop through and see if we have an unlockable club track station already - if so, just use that
		if (!station && isUnlockableClubStation)
		{
			for (s32 stationIndex = 0; stationIndex < sm_RadioStationList[musicType].m_RadioStation.GetCount(); stationIndex++)
			{
				if (sm_RadioStationList[musicType].m_RadioStation[stationIndex] && sm_RadioStationList[musicType].m_RadioStation[stationIndex]->IsUnlockableClubStation())
				{
					station = sm_RadioStationList[musicType].m_RadioStation[stationIndex];
				}
			}
		}

		if(station)
		{
			Displayf("Found Station %s", stationSettings->Name);
			//These are additional tracks for an existing station. Append the tracklists.
			for(u32 trackListIndex = 0; trackListIndex < stationSettings->numTrackLists; trackListIndex++)
			{
				const ReplayRadioStationTrackList *trackList = 
					audNorthAudioEngine::GetObject<ReplayRadioStationTrackList>(stationSettings->TrackList[trackListIndex].TrackListRef);

				Displayf("Merge Station %s : Tracklist with %d tracks", stationSettings->Name, trackList->numTracks);
				station->AddTrackList( trackList );
			}
		}
		else
		{
			Displayf("New Station %s", stationSettings->Name);
			//This is a new station. Create a new radio station using these settings.
			CReplayRadioStation* station = rage_new CReplayRadioStation();
			if(station)
			{
				station->Init(stationSettings);
				station->SetIsUnlockableClubStation(isUnlockableClubStation);
				sm_RadioStationList[musicType].m_RadioStation.PushAndGrow(station);
				for(u32 trackListIndex = 0; trackListIndex < stationSettings->numTrackLists; trackListIndex++)
				{
					const ReplayRadioStationTrackList *trackList = 
						audNorthAudioEngine::GetObject<ReplayRadioStationTrackList>(stationSettings->TrackList[trackListIndex].TrackListRef);

					Displayf("Merge Station Tracklist with %d tracks", trackList->numTracks);
					station->AddTrackList( trackList );
				}
				sm_RadioStationList[musicType].m_NumRadioStations++;
			}
		}
	}
}

CReplayRadioStation *CReplayAudioTrackProvider::FindStation(eReplayMusicType musicType, const char *name)
{
	return FindStation(musicType, atStringHash(name));
}

CReplayRadioStation *CReplayAudioTrackProvider::FindStation(eReplayMusicType musicType, u32 nameHash)
{
	s32 stationIndex = FindIndexOfStationWithHash(musicType, nameHash);
	if (stationIndex >= 0)
	{
		return sm_RadioStationList[musicType].m_RadioStation[stationIndex];
	}

	return NULL;
}

s32 CReplayRadioStation::GetNumberOfUsableTracks() const
{
	s32 count = 0;

	s32 const c_trackTotal = m_RadioTrack.GetCount();
	for( s32 index = 0; index < c_trackTotal; ++index )
	{
		CReplayRadioStationTrack const* c_currentTrack = m_RadioTrack[index];
		if( c_currentTrack && c_currentTrack->GetUseCount() == 0 )
		{
			++count;
		}
	}

	return count;
}

u32 CReplayRadioStation::GetTrackId(u32 index) const
{
	if(index >= m_RadioTrack.GetCount())
		return 0;

	return m_RadioTrack[index]->GetTrackId();
}

u32 CReplayRadioStation::GetTrackUseCount( u32 index ) const
{
	if(index >= m_RadioTrack.GetCount())
		return 0;

	return m_RadioTrack[index]->GetUseCount();
}

u32 CReplayRadioStation::GetTrackSoundHash(u32 index) const
{
	if(index >= m_RadioTrack.GetCount())
		return 0;

	return m_RadioTrack[index]->GetSoundHash();
}

CReplayRadioStationTrack* CReplayRadioStation::GetTrack( u32 index )
{
	return ( index < m_RadioTrack.GetCount() ) ? m_RadioTrack[index] : NULL;
}

s32 CReplayAudioTrackProvider::FindIndexOfStationWithHash(eReplayMusicType musicType, u32 nameHash)
{
	for(s32 stationIndex=0; stationIndex<sm_RadioStationList[musicType].m_RadioStation.GetCount(); stationIndex++)
	{
		if(sm_RadioStationList[musicType].m_RadioStation[stationIndex] && (sm_RadioStationList[musicType].m_RadioStation[stationIndex]->GetNameHash() == nameHash))
		{
			return stationIndex;
		}
	}
	return -1;
}

u32 CReplayAudioTrackProvider::GetCountOfCategoriesAvailable(eReplayMusicType musicType)
{
	return sm_RadioStationList[musicType].m_RadioStation.GetCount();
}

const char* CReplayAudioTrackProvider::GetMusicCategoryNameKey(eReplayMusicType musicType, u32 const index)
{
	if(index >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return "";

	return sm_RadioStationList[musicType].m_RadioStation[index]->GetName();
}

u32 CReplayAudioTrackProvider::GetMusicCategoryNameHash(eReplayMusicType musicType, u32 const index)
{
	if(index >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return 0;

	return sm_RadioStationList[musicType].m_RadioStation[index]->GetNameHash();
}

u32 CReplayAudioTrackProvider::GetTotalCountOfTracksForCategory(eReplayMusicType musicType, u32 const categoryIndex)
{
	if(categoryIndex >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return 0;

	return sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetNumberOfTracks();
}

u32 CReplayAudioTrackProvider::GetCountOfUsableTracksForCategory( eReplayMusicType musicType, u32 const categoryIndex )
{
	if(categoryIndex >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return 0;

	return sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetNumberOfUsableTracks();
}

bool CReplayAudioTrackProvider::GetCategoryAndIndexFromMusicTrackHash(eReplayMusicType musicType, u32 soundHash, s32 &categoryIndex, s32 &trackIndex)
{
	// search loaded tracks for matching hash
	for(int i=0; i<sm_RadioStationList[musicType].m_RadioStation.GetCount(); i++)
	{
		for(int j=0; j<sm_RadioStationList[musicType].m_RadioStation[i]->GetNumberOfTracks(); j++)
		{
			if(sm_RadioStationList[musicType].m_RadioStation[i]->GetTrackSoundHash(j) == soundHash)
			{
				categoryIndex = i;
				trackIndex = j;
				return true;
			}
		}
	}
	categoryIndex = -1;
	trackIndex = -1;
	return false;
}

u32 CReplayAudioTrackProvider::GetMusicTrackSoundHash(eReplayMusicType musicType, u32 const categoryIndex, u32 const index)
{
	if(categoryIndex >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return 0;

	if(index >= sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetNumberOfTracks())
		return 0;

	return sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetTrackSoundHash(index);

}

u32 CReplayAudioTrackProvider::GetMusicTrackUsageCount( eReplayMusicType musicType, u32 const categoryIndex, u32 const index )
{
	if(categoryIndex >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return 0;

	if(index >= sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetNumberOfTracks())
		return 0;

	return sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetTrackUseCount(index);
}

bool CReplayAudioTrackProvider::GetMusicTrackArtistNameKey(eReplayMusicType musicType, u32 const categoryIndex, u32 const index, char (&out_buffer)[ rage::LANG_KEY_SIZE ])
{
	if(categoryIndex >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return false;

	if(index >= sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetNumberOfTracks())
		return false;

	formatf( out_buffer, "%dA", sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetTrackId(index) );
	return true;
}

bool CReplayAudioTrackProvider::GetMusicTrackArtistNameKey(eReplayMusicType musicType, u32 const soundHash, char (&out_buffer)[ rage::LANG_KEY_SIZE ])
{
	bool result = false;

	s32 categoryIndex;
	s32 index;

	if( GetCategoryAndIndexFromMusicTrackHash( musicType, soundHash, categoryIndex, index ) )
	{
		result = GetMusicTrackArtistNameKey( musicType, (u32)categoryIndex, (u32)index, out_buffer );
	}

	return result;
}

bool CReplayAudioTrackProvider::GetMusicTrackNameKey(eReplayMusicType musicType, u32 const categoryIndex, u32 const index, char (&out_buffer)[ rage::LANG_KEY_SIZE ])
{
	if(categoryIndex >= sm_RadioStationList[musicType].m_RadioStation.GetCount())
		return false;

	if(index >= sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetNumberOfTracks())
		return false;

	formatf( out_buffer, "%dS",  sm_RadioStationList[musicType].m_RadioStation[categoryIndex]->GetTrackId(index) );
	return true;
}

bool CReplayAudioTrackProvider::GetMusicTrackNameKey(eReplayMusicType musicType, u32 const soundHash, char (&out_buffer)[ rage::LANG_KEY_SIZE ])
{
	bool result = false;

	s32 categoryIndex;
	s32 index;

	if( GetCategoryAndIndexFromMusicTrackHash( musicType, soundHash, categoryIndex, index ) )
	{
		result = GetMusicTrackNameKey( musicType, (u32)categoryIndex, (u32)index, out_buffer );
	}

	return result;
}

u32 CReplayAudioTrackProvider::GetMusicTrackDurationMs(const u32 soundHash)
{
	const StreamingSound *soundMeta = SOUNDFACTORY.DecompressMetadata<StreamingSound>(soundHash);
	return soundMeta ? soundMeta->Duration : 0;	
}

CReplayAudioTrackProvider::eTRACK_VALIDITY_RESULT CReplayAudioTrackProvider::IsValidSoundHash(const u32 soundHash)
{
	eTRACK_VALIDITY_RESULT result = TRACK_VALIDITY_INVALID;
	bool resultFound = false;

#if DISABLE_COMMERCIALS
	// If we are a commercial skip the loop and assume invalid
	resultFound = IsCommercial( soundHash );
#endif

	// search loaded tracks for matching hash
	for(int musicType = REPLAY_RADIO_FIRST; !resultFound && musicType < NUMBER_OF_REPLAY_MUSIC_TYPES; ++musicType )
	{
		for(int i=0;  !resultFound &&  i<sm_RadioStationList[musicType].m_RadioStation.GetCount(); i++)
		{
			for(int j=0; !resultFound && j<sm_RadioStationList[musicType].m_RadioStation[i]->GetNumberOfTracks(); j++)
			{
				if( sm_RadioStationList[musicType].m_RadioStation[i]->GetTrackSoundHash(j) == soundHash )
				{
					if(sm_RadioStationList[musicType].m_RadioStation[i]->GetTrackId(j) != 0)
					{
						result = ( musicType == REPLAY_SCORE_MUSIC || musicType == REPLAY_AMBIENT_TRACK ||
							sm_RadioStationList[musicType].m_RadioStation[i]->GetTrackUseCount(j) == 0 ) ? TRACK_VALIDITY_VALID : TRACK_VALIDITY_ALREADY_USED;
					}
					else
					{
						result = TRACK_VALIDITY_INVALID;
					}

					resultFound = true;
				}
			}
		}
	}

	return result;
}

eReplayMusicType CReplayAudioTrackProvider::GetMusicType( u32 const soundHash )
{
	eReplayMusicType musicType = REPLAY_RADIO_INVALID;

	for(int musicIndex = REPLAY_RADIO_FIRST; musicType == REPLAY_RADIO_INVALID && musicIndex < NUMBER_OF_REPLAY_MUSIC_TYPES; ++musicIndex )
	{
		for(int i=0; musicType == REPLAY_RADIO_INVALID && i<sm_RadioStationList[musicIndex].m_RadioStation.GetCount(); i++)
		{
			for(int j=0; musicType == REPLAY_RADIO_INVALID && j<sm_RadioStationList[musicIndex].m_RadioStation[i]->GetNumberOfTracks(); j++)
			{
				if(sm_RadioStationList[musicIndex].m_RadioStation[i]->GetTrackSoundHash(j) == soundHash)
				{
					if(sm_RadioStationList[musicIndex].m_RadioStation[i]->GetTrackId(j) != 0)
					{
						musicType = (eReplayMusicType)musicIndex;
					}
				}
			}
		}
	}

	return musicType;
}

bool CReplayAudioTrackProvider::IsCommercial( u32 const soundHash )
{
	bool const c_isCommercial = audRadioTrack::IsCommercial( soundHash );
	return c_isCommercial;
}

bool CReplayAudioTrackProvider::IsAmbient( u32 const soundHash )
{
    eReplayMusicType const c_musicType = GetMusicType( soundHash );
    return c_musicType == REPLAY_AMBIENT_TRACK;
}

bool CReplayAudioTrackProvider::IsSFX( u32 const soundHash )
{
	bool const c_isSFX = audRadioTrack::IsSFX( soundHash );
	return c_isSFX;
}

bool CReplayAudioTrackProvider::HasBeatMarkers( u32 const soundHash )
{
	return g_RadioAudioEntity.HasBeatMarkers( soundHash );
}

// PURPOSE: Updates tracking of which tracks are already in use, so we can block them being used multiple times
// PARAMS:
//		activeProject - Montage we want to check for tracks
void CReplayAudioTrackProvider::UpdateTracksUsedInProject( CMontage& activeProject )
{
	ClearTracksUsedInProject();

	for( u32 trackIndex = 0; trackIndex < activeProject.GetMusicClipCount(); )
	{
		u32 const c_currentTrackSoundHash = activeProject.GetMusicClipSoundHash(trackIndex);
		UpdateTracksUsedInProject( c_currentTrackSoundHash, true );
	}
}

void CReplayAudioTrackProvider::UpdateTracksUsedInProject( u32 const trackHash, bool const isUsed )
{
	for( u32 musicType = REPLAY_RADIO_FIRST; musicType < NUMBER_OF_REPLAY_MUSIC_TYPES; ++musicType )
	{
		u32 const c_stationCount = (u32)sm_RadioStationList[musicType].m_RadioStation.size();
		for( u32 stationIndex = 0; stationIndex < c_stationCount; ++stationIndex )
		{
			int const c_trackCount = sm_RadioStationList[musicType].m_RadioStation[stationIndex]->GetNumberOfTracks();
			for( int trackIndex = 0; trackIndex < c_trackCount; ++trackIndex )
			{
				CReplayRadioStationTrack* track = sm_RadioStationList[musicType].m_RadioStation[stationIndex]->GetTrack( (u32)trackIndex );
				if( track && track->GetSoundHash() == trackHash )
				{
					if( isUsed )
					{
						track->IncrementUsage();
					}
					else
					{
						track->DecrementUsage();
					}
				}
			}
		}
	}
}

// PURPOSE: Clear tracking of which tracks are already in use, for when a project is unloaded/new project is created
void CReplayAudioTrackProvider::ClearTracksUsedInProject()
{
	for( u32 musicType = REPLAY_RADIO_FIRST; musicType < NUMBER_OF_REPLAY_MUSIC_TYPES; ++musicType )
	{
		u32 const c_stationCount = (u32)sm_RadioStationList[musicType].m_RadioStation.size();
		for( u32 stationIndex = 0; stationIndex < c_stationCount; ++stationIndex )
		{
			int const c_trackCount = sm_RadioStationList[musicType].m_RadioStation[stationIndex]->GetNumberOfTracks();
			for( int trackIndex = 0; trackIndex < c_trackCount; ++trackIndex )
			{
				CReplayRadioStationTrack* track = sm_RadioStationList[musicType].m_RadioStation[stationIndex]->GetTrack( (u32)trackIndex );
				if( track )
				{
					track->ResetUsage();
				}
			}
		}
	}
}

// PURPOSE: Removes any tracks from the given montage that are not supported by this provider
// PARAMS:
//		validationTarget - Montage we want to remove invalid tracks from
// RETURNS:
//		True if any tracks were removed. False otherwise
bool CReplayAudioTrackProvider::RemoveInvalidTracksAndUpdateUsage( CMontage& validationTarget )
{
	ClearTracksUsedInProject();
	bool anyTracksRemoved = false;

	for( u32 trackIndex = 0; trackIndex < validationTarget.GetMusicClipCount(); )
	{
		u32 currentTrackSoundHash = validationTarget.GetMusicClipSoundHash(trackIndex);
		if( IsValidSoundHash( currentTrackSoundHash ) == TRACK_VALIDITY_VALID )
		{
			UpdateTracksUsedInProject( currentTrackSoundHash, true );
			++trackIndex;
		}
		else
		{
			validationTarget.DeleteMusicClip(trackIndex);
			anyTracksRemoved = true;
		}
	}

	return anyTracksRemoved;
}

#else // !NA_RADIO_ENABLED

u32 CReplayAudioTrackProvider::GetCountOfCategoriesAvailable(eReplayMusicType /*musicType*/)
{
	return 0;
}

const char* CReplayAudioTrackProvider::GetMusicCategoryNameKey(eReplayMusicType /*musicType*/, u32 const /*index*/)
{
	return "";
}

u32 CReplayAudioTrackProvider::GetMusicCategoryNameHash(eReplayMusicType /*musicType*/, u32 const /*index*/)
{
	return 0;
}

u32 CReplayAudioTrackProvider::GetCountOfTracksForCategory(eReplayMusicType /*musicType*/, u32 const /*categoryIndex*/)
{
	return 0;
}


u32 CReplayAudioTrackProvider::GetMusicTrackSoundHash(eReplayMusicType /*musicType*/, u32 const /*categoryIndex*/, u32 const /*index*/)
{
	return 0;
}

bool CReplayAudioTrackProvider::GetMusicTrackArtistNameKey(eReplayMusicType /*musicType*/, u32 const /*categoryIndex*/, u32 const /*index*/, char (&/*out_buffer*/)[ 16 ])
{
	return false;
}

bool CReplayAudioTrackProvider::GetMusicTrackArtistNameKey(eReplayMusicType /*musicType*/, u32 const /*soundHash*/, char (&/*out_buffer*/)[ 16 ])
{
	return false;
}

bool CReplayAudioTrackProvider::GetMusicTrackNameKey(eReplayMusicType /*musicType*/, u32 const /*categoryIndex*/, u32 const /*index*/, char (&/*out_buffer*/)[ 16 ])
{
	return false;
}

bool CReplayAudioTrackProvider::GetMusicTrackNameKey(eReplayMusicType /*musicType*/, u32 const /*soundHash*/, char (&/*out_buffer*/)[ 16 ])
{
	return false;
}

u32 CReplayAudioTrackProvider::GetMusicTrackDurationMs(const u32 soundHash)
{
	const StreamingSound *soundMeta = SOUNDFACTORY.DecompressMetadata<StreamingSound>(soundHash);
	return soundMeta ? soundMeta->Duration : 0;	
}

bool CReplayAudioTrackProvider::RemoveInvalidTracks(CMontage&) 
{
	return false;
}

#endif	// !NA_RADIO_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
