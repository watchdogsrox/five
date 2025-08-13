/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayAudioTrackProvider.h
// PURPOSE : Class for listing and validating audio tracks for use in replays
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef REPLAY_AUDIO_TRACK_PROVIDER_H_
#define REPLAY_AUDIO_TRACK_PROVIDER_H_

// rage
#include "atl/hashstring.h"
#include "audioengine/entity.h"

// framework
#include "fwlocalisation/textTypes.h"

// game
#include "audio/gameobjects.h"

class CMontage;
class CReplayRadioStation;

enum { kMaxReplayRadioStationLists = 16};

enum eReplayMusicType 
{	
	REPLAY_RADIO_INVALID = -1,
	REPLAY_RADIO_FIRST = 0,

	REPLAY_RADIO_MUSIC = REPLAY_RADIO_FIRST,
	REPLAY_SCORE_MUSIC,
	REPLAY_AMBIENT_TRACK,
	REPLAY_SFX_ONESHOT,

	NUMBER_OF_REPLAY_MUSIC_TYPES
};

#define DISABLE_COMMERCIALS 1

class CReplayRadioStationTrack
{
public:
	CReplayRadioStationTrack() 
		: m_SoundHash(g_NullSoundHash) 
		, m_useCount( 0 )
	{

	}

	void SetSoundHash(u32 soundHash)									{ m_SoundHash = soundHash; }
	u32 GetSoundHash() const											{ return m_SoundHash; }
	u32 GetTrackId() const;

	void ResetUsage() { m_useCount = 0; }
	u32 GetUseCount() const { return m_useCount; }
	u32 IncrementUsage() { return ++m_useCount; }
	u32 DecrementUsage() { return m_useCount > 0 ? --m_useCount : m_useCount; }

private:
	u32	m_SoundHash;
	u32	m_useCount;
};

class CReplayRadioStationList
{
public:
	CReplayRadioStationList() :
		m_NumRadioStations(0)
	{
	}

	atArray<CReplayRadioStation*> m_RadioStation;
	u32 m_NumRadioStations;
};

class CReplayRadioStation
{
public:
	CReplayRadioStation() : 
		m_StationSettings(NULL),
		m_NumberRadioTracks(0),
		m_NameHash(g_NullSoundHash),
		m_ListReversed(false),
		m_IsUnlockableClubStation(false)
	{
	}
	
	void Init(RadioStationSettings *stationSettings);
	void AddTrackList(const ReplayRadioStationTrackList *trackList );
	u32 GetNameHash() const											{ return m_NameHash; }
	const char* GetName() const										{ return m_StationSettings->Name; }
	s32 GetNumberOfTracks()	const									{ return m_RadioTrack.GetCount(); }
	bool IsUnlockableClubStation() const							{ return m_IsUnlockableClubStation; }
	void SetIsUnlockableClubStation(bool isClubStation)				{ m_IsUnlockableClubStation = isClubStation;  }
	s32 GetNumberOfUsableTracks() const;
	u32 GetTrackId(u32 index) const;
	u32 GetTrackUseCount(u32 index) const;
	u32 GetTrackSoundHash(u32 index) const;
	CReplayRadioStationTrack* GetTrack( u32 index );
	void SetListReversed(bool reversed);
	bool IsListReversed() { return m_ListReversed; }

private:
	const RadioStationSettings *m_StationSettings;
	u32 m_NameHash;
	atArray<CReplayRadioStationTrack*> m_RadioTrack;
	u32 m_NumberRadioTracks;
	bool m_ListReversed;
	bool m_IsUnlockableClubStation;
};

class CReplayAudioTrackProvider
{
public:
	enum eTRACK_VALIDITY_RESULT
	{
		TRACK_VALIDITY_FIRST,

		TRACK_VALIDITY_INVALID = TRACK_VALIDITY_FIRST,
		TRACK_VALIDITY_ALREADY_USED,
		TRACK_VALIDITY_VALID,

		TRACK_VALIDITY_MAX
	};

	static u32 GetCountOfCategoriesAvailable(eReplayMusicType musicType);
	static u32 GetMusicCategoryNameHash(eReplayMusicType musicType, u32 const index);
	static const char * GetMusicCategoryNameKey(eReplayMusicType musicType, u32 const index);

	static u32 GetTotalCountOfTracksForCategory(eReplayMusicType musicType, u32 const categoryIndex);
	static u32 GetCountOfUsableTracksForCategory(eReplayMusicType musicType, u32 const categoryIndex);

	static bool GetCategoryAndIndexFromMusicTrackHash(eReplayMusicType musicType, u32 soundHash, s32 &categoryIndex, s32 &trackIndex);
	static u32 GetMusicTrackSoundHash(eReplayMusicType musicType, u32 const categoryIndex, u32 const index);
	static u32 GetMusicTrackUsageCount(eReplayMusicType musicType, u32 const categoryIndex, u32 const index);
	static bool GetMusicTrackArtistNameKey(eReplayMusicType musicType, u32 const categoryIndex, u32 const index, char (&out_buffer)[ rage::LANG_KEY_SIZE ]);
	static bool GetMusicTrackArtistNameKey(eReplayMusicType musicType, u32 const soundHash, char (&out_buffer)[ rage::LANG_KEY_SIZE ]);
	static bool GetMusicTrackNameKey(eReplayMusicType musicType, u32 const categoryIndex, u32 const index, char (&out_buffer)[ rage::LANG_KEY_SIZE ]);
	static bool GetMusicTrackNameKey(eReplayMusicType musicType, u32 const soundHash, char (&out_buffer)[ rage::LANG_KEY_SIZE ]);
	
	static u32 GetMusicTrackDurationMs(const u32 soundHash);
	static eTRACK_VALIDITY_RESULT IsValidSoundHash(const u32 soundHash);
	static eReplayMusicType GetMusicType( u32 const soundHash );
    static bool IsCommercial( u32 const soundHash );
    static bool IsAmbient( u32 const soundHash );
	static bool IsSFX( u32 const soundHash );
	static bool HasBeatMarkers( u32 const soundHash );

	static void UpdateTracksUsedInProject( CMontage& activeProject );
	static void UpdateTracksUsedInProject( u32 const trackHash, bool const isUsed );
	static void ClearTracksUsedInProject();

	static bool RemoveInvalidTracksAndUpdateUsage( CMontage& validationTarget );

	static void InitReplayRadioStations();

	static u32 GetNumReplayRadioStationLists() { return sm_RadioStationList.GetMaxCount(); }
	static CReplayRadioStationList* GetReplayRadioStationList( u32 index ) { return &sm_RadioStationList[index]; }

	static void ReverseLists();

private:
	
	static void MergeRadioStationList(eReplayMusicType musicType, RadioStationList *radioStationList, bool isUnlockableClubStation = false);
	static CReplayRadioStation* FindStation(eReplayMusicType musicType, const char *name);
	static CReplayRadioStation* FindStation(eReplayMusicType musicType, u32 nameHash);
	static s32 FindIndexOfStationWithHash(eReplayMusicType musicType, u32 nameHash);

	static atRangeArray<CReplayRadioStationList, NUMBER_OF_REPLAY_MUSIC_TYPES> sm_RadioStationList;
};

#endif // REPLAY_AUDIO_TRACK_PROVIDER_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
