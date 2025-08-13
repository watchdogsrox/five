// 
// audio/radiostation.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOSTATION_H
#define AUD_RADIOSTATION_H

#include "audiodefines.h"
#include "userradiotrackmanager.h"
#include "math/random.h"

enum { kMaxNewsStories = 256};

const u8 g_MaxRadioStations		= 96;
const u8 g_OffRadioStation		= 255;
const u8 g_NullRadioStation		= 254;

const u8 g_MaxDjSpeechInHistory				= 10;

const u32 g_MaxRadioNameLength	= 255;

#if NA_RADIO_ENABLED

#include "gameobjects.h"
#include "radiostationhistory.h"
#include "radiostationtracklistcollection.h"
#include "streamslot.h"
#include "radiotrack.h"
#include "atl/array.h"
#include "atl/bitset.h"
#include "audioengine/widgets.h"

namespace rage
{
	class audSpeechSound;
	struct RadioTrackSettings;
}

#define PRINT_RADIO_DEBUG_TEXT 0
#define HEIST3_HIDDEN_RADIO_ENABLED 0

#if HEIST3_HIDDEN_RADIO_ENABLED 
#define HEIST3_HIDDEN_RADIO_OVERRIDE_TUNEABLE __DEV && 0
#define HEIST3_HIDDEN_RADIO_BANK_HASH 0x1E9DEF21 // DLC_HEIST3/HEIST3_HIDDEN_RADIO_01
#define HEIST3_HIDDEN_RADIO_VEHICLE 0xC3DDFDCE // TAILGATER
#define HEIST3_HIDDEN_RADIO_STATION_NAME 0x32E1EB1 // DLC_HEIST3_HRS

#if HEIST3_HIDDEN_RADIO_OVERRIDE_TUNEABLE
#define HEIST3_HIDDEN_RADIO_KEY_A 0xB648EC6C // 3058232428
#define HEIST3_HIDDEN_RADIO_KEY_B 0x8BA44761 // 2342799201
#define HEIST3_HIDDEN_RADIO_KEY_C 0x4AE7FCEE // 1256717550
#define HEIST3_HIDDEN_RADIO_KEY_D 0x83010484 // 2197881988
#else
#define HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_A 0x39BF6078 // HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_A
#define HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_B 0xFB326363 // HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_B
#define HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_C 0xECFFC6FE // HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_C
#define HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_D 0xE5CFB89E // HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_D
#endif
#endif

enum audRadioNewsStoryState
{
	RADIO_NEWS_UNLOCKED =	1,
	RADIO_NEWS_ONCE_ONLY =	2,
};

enum audRadioStationImmutableIndexType
{
	IMMUTABLE_INDEX_GLOBAL,
	IMMUTABLE_INDEX_TUNEABLE,
	IMMUTABLE_INDEX_TYPE_MAX,
};

enum audRadioDjSpeechCategories
{
	RADIO_DJ_SPEECH_CAT_INTRO,
	RADIO_DJ_SPEECH_CAT_OUTRO,
	RADIO_DJ_SPEECH_CAT_GENERAL,
	RADIO_DJ_SPEECH_CAT_TIME,
	RADIO_DJ_SPEECH_CAT_TO,
	NUM_RADIO_DJ_SPEECH_CATS
};

enum audRadioDjSpeechStates
{
	RADIO_DJ_SPEECH_STATE_IDLE,
	RADIO_DJ_SPEECH_STATE_PREPARING,
	RADIO_DJ_SPEECH_STATE_PREPARED,
	RADIO_DJ_SPEECH_STATE_PLAYING
};

class audRadioPRNG16
{
public:
	audRadioPRNG16() {}

	void Reset(const u32 seed)
	{
		m_x = seed & 0xffff;
		m_y = seed >> 16;
	}

	u32 GetSeed() const
	{
		return (m_x | (m_y<<16));
	}

	u16 GetInt16()
	{
		const u16 t =(m_x ^ (m_x << 5)); 
		m_x = m_y; 
		m_y = (m_y ^ (m_y >> 1)) ^ (t ^ (t >> 3));

		return m_y;
	}

	u32 GetInt32()
	{
		return (GetInt16()<<16)|GetInt16();
	}

	u16 GetRanged(const u16 m, const u16 M)
	{
		return(((M-m+1)!=0?(GetInt16()%(M-m+1)):0)+m);
	}

	s32 GetRanged(const s32 m, const s32 M)
	{
		return (s32)GetRanged((u16)m, (u16)M);
	}

	float GetFloat()
	{
		const u32 theInt = GetInt32();
		// Mask lower 23 bits, multiply by 1/2**23.
		return (theInt & ((1<<23)-1)) * 0.00000011920928955078125f;
	}
	
	inline float GetRanged(float m,float M)
	{
		return(GetFloat()*(M-m)+m);
	}

private:

	u16 m_x;
	u16 m_y;
};

const u8 g_NullDjSpeechCategory	= NUM_RADIO_DJ_SPEECH_CATS;

//class audEnvironmentGameMetric;
class audRadioStation
{
	friend class CRadioStationSaveStructure;
	friend class audRadioAudioEntity;

public:
	static void InitClass(void);
	static void ShutdownClass(void);
	static void Reset(void);

	static void ResetHistoryForNewGame();
	static void ResetHistoryForNetworkGame();
	static void ResetHistoryForSPGame();

	
	static s32 FindIndexOfStationWithHash(u32 nameHash);
	static audRadioStation *FindStation(const char *name);
	static audRadioStation *FindStation(u32 nameHash);
	static audRadioStation *GetStation(u32 index);
	static audRadioStation *FindStationWithImmutableIndex(u32 index, audRadioStationImmutableIndexType indexType);

	static u32 GetNumUnlockedStations()
	{
		return sm_NumUnlockedRadioStations;
	}

	static u32 GetNumUnlockedFavouritedStations()
	{
		return sm_NumUnlockedFavouritedStations;
	}

	static u32 GetNumTotalStations()
	{
		return sm_NumRadioStations;
	}

	static void UpdateStations(u32 timeInMs);
    static void GameUpdate();
	static void UpdateRetuneSounds();
	static void StopRetuneSounds();
	static void UpdateDjSpeech(u32 timeInMs);

	static bool isDjSpeaking()
	{
		return (sm_DjSpeechMonoSound && (((audSound *)sm_DjSpeechMonoSound)->GetPlayState() == AUD_SOUND_PLAYING));
	}

	static void SkipForwardStations(u32 timeToSkipMs);

	audRadioStation(RadioStationSettings *stationSettings);
	~audRadioStation();


	void Init(void);
	void DLCInit(void);
	void Shutdown(void);

	void PostLoad();

	// Helper struct to facilitate speading a u32 across multiple history array slots
	struct audRadioStationPackedInt
	{
		union
		{
			struct
			{
				u8		m_Byte0;
				u8		m_Byte1;
				u8		m_Byte2;
				u8		m_Byte3;
			} PackedData;
			u32		m_Value;
		};
	};

	struct audRadioStationSyncData
	{
		audRadioStationSyncData()
		{
			nameHash = 0;
			randomSeed = 0;
			currentTrackPlaytime = 0.0f;
		}

		u32 nameHash; // needs full 32bits
		u32 randomSeed; // needs full 32bits

		float currentTrackPlaytime; // could ditch the sign bit and store in 15bits

		struct TrackId
		{
			TrackId() : category (0), index(0) {}

			static const unsigned CATEGORY_BITS = 4;
			static const unsigned CATEGORY_MASK = (1<<CATEGORY_BITS)-1;
			static const unsigned INDEX_BITS = 8;
			static const unsigned INDEX_MASK = (1<<INDEX_BITS)-1;

			u8 category; 
			u8 index;
		};
		TrackId nextTrack;
		atRangeArray<TrackId, audRadioStationHistory::kRadioHistoryLength> trackHistory; 
	};

	// PURPOSE
	//	Grab a snapshot of this station for network sync
	void PopulateSyncData(audRadioStationSyncData &syncData) const;
	void SyncStation(const audRadioStationSyncData &data);	

	bool GetNextMixStationBeat(f32& beatTimeS, f32& bpm, s32& beatNumber);
	void ConstructMixStationBeatMarkerList();
	u32 GetNumChannels() const { return AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_HASREVERBCHANNEL) == AUD_TRISTATE_TRUE ? 3 : 2; }

	void Update(u32 timeInMs);
	bool IsStreamingPhysically() const;
	bool IsStreamingVirtually() const;
	void SetPhysicalStreamingState(bool shouldStreamPhysically, audStreamSlot **streamSlots = NULL, u8 soundBucketId = 0xff);
	void SetStreamSlots(audStreamSlot **streamSlots);

	void UpdateStereoEmitter(f32 volumeFactor, f32 cutoff);
	void UpdatePositionedEmitter(const u32 emitterIndex, const f32 emittedVolume, const f32 volumeFactor, 
		const Vector3 &position, const u32 LPFCutoff, const u32 HPFCutoff, const audEnvironmentGameMetric *occlusionMetric, const u32 emitterType, const f32 environmentalLoudness, bool isPlayerVehicleRadioEmitter);
	void MuteEmitters();

	bool IsLocalPlayerNewsStation() const { return m_NameHash == ATSTRINGHASH("HIDDEN_RADIO_MPSUM2_NEWS", 0xEAE7EFBF); }
	bool IsUSBMixStation() const { return m_NameHash == GetUSBMixStationName(); }
	static u32 GetUSBMixStationName() { return ATSTRINGHASH("RADIO_36_AUDIOPLAYER", 0x526EAB09); }
	static audRadioStation* GetUSBMixStation() { return FindStation(GetUSBMixStationName()); }

	void AddActiveTrackToHistory(void);
	u32 FindTrackId(const s32 category, const u32 soundRef) const;

	void QueueTrackList(const RadioStationTrackList *trackList, const bool forceNow);
	void ClearQueuedTracks();

	u32 GetNameHash() const 
	{
		return m_NameHash;
	}

	s32 GetOrder() const;

	bool IsMixStation() const { return m_IsMixStation; }
	float GetListenTimer() const { return m_ListenTimer; }
	void SetListenTimer(const float listenTimer) { m_ListenTimer = listenTimer; }

	const RadioStationSettings *GetStationSettings() const
	{
		return m_StationSettings;
	}

	bool IsRequestingOverlappedTrackPrepare()
	{
		return m_IsRequestingOverlappedTrackPrepare;
	}

	const char *GetName() const
	{
		Assert(m_StationSettings);
		return m_StationSettings->Name;
	}

	s32 GetStationImmutableIndex(audRadioStationImmutableIndexType indexType) const;
	u32 GetStationIndex() const;

	f32 GetAmbientVolume() const
	{
		return m_AmbientVolume;
	}

	void SetShouldPlayFullRadio(bool shouldPlayFullRadio)
	{
		m_ShouldPlayFullRadio = shouldPlayFullRadio;
	}

	void SkipForward(u32 timeToSkip);

#if __BANK
	static void AddWidgets(bkBank &bank);
	static void AddUSBStationWidgets();

	void DebugDrawMixBeatMarkers() const;
	void DrawDebug(audDebugDrawManager &drawMgr) const;
	void DrawTakeoverStation() const;
	void DrawUSBStation() const;
	void DumpTrackList() const;
#endif // __BANK

	s32 GetCurrentPlayTimeOfActiveTrack() const
	{
		return m_Tracks[m_ActiveTrackIndex].GetPlayTime();
	}

	f32 GetCurrentPlayFractionOfActiveTrack() const
	{
		return m_Tracks[m_ActiveTrackIndex].GetPlayFraction();
	}

	audRadioTrack &GetCurrentTrack()
	{
		return m_Tracks[m_ActiveTrackIndex];
	}

	const audRadioTrack &GetCurrentTrack() const
	{
		return m_Tracks[m_ActiveTrackIndex];
	}

	const audRadioTrack& GetMixStationBeatTrack() const;

	u32 GetActiveTrackIndex()
	{
		return m_ActiveTrackIndex;
	}

	u32 GetNextTrackIndex()
	{
		return (m_ActiveTrackIndex + 1) % 2;
	}

	audRadioTrack &GetNextTrack()
	{
		u32 nextIndex = (m_ActiveTrackIndex + 1) % 2;
		return m_Tracks[nextIndex];
	}

	const audRadioTrack &GetNextTrack() const
	{
		u32 nextIndex = (m_ActiveTrackIndex + 1) % 2;
		return m_Tracks[nextIndex];
	}

	void ForceNextTrack(const u8 category, const u32 context, const u16 index);
	void ForceNextTrack(const char* category, const u32 contextHash, const u32 trackNameHash, bool ignoreLocking = false);

	bool IsInGenre(RadioGenre genre) const
	{
		return (m_Genre == genre);
	}

	RadioGenre GetGenre() const
	{
		return m_Genre;
	}

	bool IsTalkStation() const
	{
		return (GetGenre() == RADIO_GENRE_LEFT_WING_TALK || GetGenre() == RADIO_GENRE_RIGHT_WING_TALK);
	}

	bool UseRandomizedStrideSelection() const { return m_UseRandomizedStrideSelection;}

	bool UnlockTrackList(const RadioStationTrackList* trackList);
	bool UnlockTrackList(const char *trackListName);
	bool UnlockTrackList(u32 trackListName);

	bool LockTrackList(const RadioStationTrackList* trackList);
	bool LockTrackList(const char *trackListName);
	bool LockTrackList(u32 trackListName);

	bool IsTrackListUnlocked(const RadioStationTrackList* trackList) const;
	bool IsTrackListUnlocked(const char *trackListName) const;
	bool IsTrackListUnlocked(u32 trackListName) const;

	// USB Station functions
	bool ForceMusicTrackList(const u32 trackListName, u32 timeOffsetMs);
	void SkipToOffsetInMusicTrackList(u32 timeOffsetMs, bool allowWrap = false);
	u32 GetLastForcedMusicTrackList() { return m_LastForcedMusicTrackList; }
	u32 GetMusicTrackListCurrentPlayTimeMs() const;
	u32 GetMusicTrackListDurationMs() const;
	u32 GetMusicTrackListTrackDurationMs(u32 trackIndex) const;
	s32 GetMusicTrackListActiveTrackIndex() const;
	u32 GetMusicTrackListNumTracks() const;
	u32 GetMusicTrackListTrackIDForTimeOffset(u32 timeOffsetMs) const;
	u32 GetMusicTrackListTrackIDForIndex(u32 trackIndex) const;
	s32 GetMusicTrackListTrackStartTimeMs(u32 trackIndex) const;

	u32 GetPlayingTrackListNextTrackTimeOffset();
	u32 GetPlayingTrackListPrevTrackTimeOffset();

	static f32 GetCountryTalkRadioSignal() { return sm_CountryTalkRadioSignal; }
	static void SetScriptGlobalRadioSignalLevel(f32 signalLevel) { sm_ScriptGlobalRadioSignalLevel = signalLevel; }

	static void UnlockMissionNewsStory(const u32 newsStory);
	static bool IsMissionNewsStoryUnlocked(const u32 newsStory)
	{
		return (sm_NewsStoryState[newsStory - 1] & RADIO_NEWS_UNLOCKED) != 0;
	}

	// PURPOSE
	//	Unlock a news story as 'generic', so that script can unlock mission pack news stories
	static void UnlockGenericNewsStory(const u32 newsStory)
	{
		sm_NewsStoryState[newsStory-1] = RADIO_NEWS_UNLOCKED;
	}

	static void DumpNewsState();
	
	bool IsFrozen() const
	{
		return m_IsFrozen;
	}

	void Freeze()
	{
		m_IsFrozen = true;
	}

	void Unfreeze()
	{
		m_IsFrozen = false;
	}

	static bool IsPlayingEndCredits()
	{
		return sm_PlayingEndCredits;
	}
	static void StartEndCredits();
	static void StopEndCredits();

	void ForceTrack(const char *trackSettingsName, s32 startOffsetMs = -1);
	void ForceTrack(const RadioTrackSettings *trackSettings, s32 startOffsetMs = -1, u32 trackIndex = 0);
	void ForceTrack(const u32 trackSettingsName, s32 startOffsetMs = -1);

	void SetLocked(const bool isLocked);

	static bool IsNetworkModeHistoryActive()
	{
		return sm_IsNetworkModeHistoryActive;
	}

	static bool ShouldAddCategoryToNetworkHistory(const u32 category);

#if __BANK
	static void FormatTimeString(const u32 milliseconds, char *destBuffer, const u32 destLength);
	template <typename T, size_t _Size> static void FormatTimeString(const u32 milliseconds, T (&dest)[_Size])
	{
		FormatTimeString(milliseconds, dest, _Size);
	}

	static const char *GetTrackCategoryName(const s32 trackCategory);
	u32 GetRandomSeed() const { return m_Random.GetSeed(); }
#endif

	void AddMusicTrackList(const RadioStationTrackList *trackList, audMetadataRef metadataRef);
	void AddTrackList(const RadioStationTrackList *trackList, f32 firstTrackProbability = 1.f);
	bool HasQueuedTrackList() const { return m_QueuedTrackList != NULL; }

	static void DisableOverlappedTracks(const bool disable) { sm_DisableOverlap = disable; }

	static bool HasSyncedUnlockableDJStation() { return sm_HasSyncedUnlockableDJStation; }
	static void MaintainHeist4SPStationState();

	void PrepareIMDrivenDjSpeech(const u32 category, const u32 variationNum);
	static void TriggerIMDrivenDjSpeech(const u32 startTimeMs);
	bool ChooseDjSpeechVariation(const u32 category, u32 &variationNum, u32 &lengthMs, const bool ignoreHistory);
	void CancelIMDrivenDjSpeech();
	bool IsDjSpeechPrepared() const { return sm_DjSpeechState == RADIO_DJ_SPEECH_STATE_PREPARED; }

	bool IsLocked() const;
	bool IsHidden() const { return AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_HIDDEN) == AUD_TRISTATE_TRUE; }
	void SetHidden(bool isHidden);

	bool IsFavourited() const;
	void SetFavourited(bool isFavourited);

	static void GetFavouritedStations(u32 &favourites1, u32 &favourites2);

	void SetMusicOnly(const bool musicOnly)
	{
		m_ScriptSetMusicOnly = musicOnly;
	}

	bool HasTrackChanged() const { return m_bHasTrackChanged; }
	bool ShouldOnlyPlayAds() const { return m_OnlyPlayAds; }

	bool IsCurrentTrackNew() const;

#if RSG_PC // user music
	static audUserRadioTrackManager* GetUserRadioTrackManager() { return &sm_UserRadioTrackManager; }

	bool IsUserStation() const
	{
		return m_IsUserStation;
	}

	void NextTrack();
	void PrevTrack();

	static bool HasUserTracks();
	static bool IsInRecoveryMode()				{ return sm_IsInRecoveryMode; }
#endif

	static void MergeRadioStationList(const RadioStationList *radioStationList);
	static void RemoveRadioStationList(const RadioStationList *radioStationList);

	s32 ComputeNumTracksAvailable(s32 category, u32 context = kNullContext) const;
	const audRadioStationHistory *FindHistoryForCategory(const s32 category) const;
	const audRadioStationTrackListCollection *FindTrackListsForCategory(const s32 category) const;

	struct MixBeatMarker
	{
		s32 playtimeMs;
		u32 numBeats;
		u32 trackIndex;
	};

#if HEIST3_HIDDEN_RADIO_ENABLED
	static u32 sm_Heist3HiddenRadioBankId;
#endif

private:
	// USB Station functions
	s32 GetMusicTrackIndexInTrackList(const RadioStationTrackList::tTrack* track) const;
	const RadioStationTrackList::tTrack* GetMusicTracklistTrackForTimeOffset(u32 timeOffsetMs, bool allowWrap = false) const;
	const RadioStationTrackList::tTrack* GetMusicTrackListTrackForIndex(u32 index) const;	

	ASSERT_ONLY(void ValidateEnvironmentMetric(const audEnvironmentGameMetric *occlusionMetric) const);
	ASSERT_ONLY(void ValidateEnvironmentMetricInRange(const float val, const char *valName, const char *extraInfo) const);

	void RandomizeCategoryStrides(bool randomizeStartTrack, bool maintainDirection);
	void RandomizeCategoryStride(TrackCats trackCategory, bool randomizeStartTrack, bool maintainDirection);
	void RandomizeCategoryStride(TrackCats trackCategory, u32 numTracks, bool randomizeStartTrack, bool maintainDirection);

	static bool IsRandomizedStrideCategory(const u32 category);
	static u32 PackCategoryForNetwork(const u32 category);
	static u32 UnpackCategoryFromNetwork(const u32 category);
	static void CreateDjSpeechSounds(const u32 variationNum);
	void LogTrack(const u32 category, const u32 context, const u32 index, const u32 soundRef);

	static void FlagLockedStationChange() { sm_UpdateLockedStations = true; }

	static void InitRadioStationList(RadioStationList *radioStationList);

	static void SortRadioStationList();

	s32 GetTuneablePrioritizedMusicTrackListIndex(u32& firstTrackIndex, u32& lastTrackIndex);
	void AddActiveDjSpeechToHistory(u32 timeInMs);
	bool ExtractDjMarkers(s32 &introStartTimeMs, s32 &introEndTimeMs, s32 &outroStartTimeMs, s32 &outroEndTimeMs) const
	{
		return m_Tracks[m_ActiveTrackIndex].GetDjMarkers(introStartTimeMs, introEndTimeMs, outroStartTimeMs, outroEndTimeMs);
	}
	u32 CreateCompatibleDjSpeech(u32 timeInMs, u8 preferredCategory, s32 windowLengthMs, const s32 windowStartTime) const;
	u32 FindCompatibleDjSpeechVariation(const char *voiceName, const char *contextName, s32 minLengthMs, s32 maxLengthMs, u32 &variationNum,
		u8 djSpeechCategory, const bool ignoreHistory) const;

	const RadioStationTrackList::tTrack *GetTrack(const s32 category, const u32 context, const s32 index, bool ignoreLocking = false) const;

	bool IsTrackInHistory(const u32 trackId,  const s32 category, const s32 historyLength) const;

	u8 ComputeNextTrackCategory(u32 timeInMs);
	u8 ComputeNextTrackCategoryForTakeoverStation(u32 timeInMs, u32 activeTrackCategory);
	const RadioStationTrackList::tTrack *ComputeRandomTrack(s32 &category, s32 &index);
	RadioStationTrackList::tTrack *GetTrackForNetwork(u8 category, u8 index);
	
	audPrepareState PrepareNextTrack();
	bool InitialiseNextTrack(const u32 timeInMs);
	
	void RandomizeSequentialStationPlayPosition();	

	u32 GetNextValidSelectionTime(const s32 category) const;
	void SetNextValidSelectionTime(const s32 category, const u32 timeMs);
	bool ShouldOnlyPlayMusic() const;
	f32 GetMixTransitionVolumeLinear(u32 trackIndex) const;
	
	s32 GetRandomNumberInRange(const s32 min, const s32 max);
	float GetRandomNumberInRange(const float min, const float max);
	bool ResolveProbability(const float Probability);

	bool IsValidCategoryForStation(const TrackCats category, const bool playNews) const;
	bool PlayIdentsInsteadOfAds() const;

	atRangeArray<audRadioTrack,2> m_Tracks;
	atRangeArray<audWaveSlot *, 2> m_WaveSlots;

	audRadioStationTrackListCollection m_MusicTrackLists;
	audRadioStationTrackListCollection m_AdvertTrackLists;
	audRadioStationTrackListCollection m_IdentTrackLists;
	audRadioStationTrackListCollection m_DjSoloTrackLists;

	audRadioStationTrackListCollection m_TakeoverMusicTrackLists;
	audRadioStationTrackListCollection m_TakeoverIdentTrackLists;
	audRadioStationTrackListCollection m_TakeoverDjSoloTrackLists;

	audRadioStationTrackListCollection m_ToAdTrackLists;
	audRadioStationTrackListCollection m_ToNewsTrackLists;
	audRadioStationTrackListCollection m_UserIntroTrackLists;
	audRadioStationTrackListCollection m_UserOutroTrackLists;
	audRadioStationTrackListCollection m_UserIntroEveningTrackLists;
	audRadioStationTrackListCollection m_UserIntroMorningTrackLists;

	static audRadioStationTrackListCollection sm_NewsTrackLists;
	static audRadioStationTrackListCollection sm_WeatherTrackLists;

	const RadioStationTrackList *m_QueuedTrackList;
	u32 m_QueuedTrackListIndex;

	u32 m_NextValidSelectionTime[NUM_RADIO_TRACK_CATS];
	atRangeArray<s8, NUM_RADIO_TRACK_CATS> m_CategoryStride;

	static u32 sm_PositionedPlayerVehicleRadioLPFCutoff;
	static u32 sm_PositionedPlayerVehicleRadioHPFCutoff;
	static f32 sm_PositionedPlayerVehicleRadioEnvironmentalLoudness;
	static f32 sm_PositionedPlayerVehicleRadioVolume;
	static f32 sm_PositionedPlayerVehicleRadioRollOff;
	static bool sm_HasSyncedUnlockableDJStation;

	atArray<MixBeatMarker> m_MixStationBeatMarkers;
	atArray<u32> m_MixStationBeatMarkerTrackDurations;
	
	audRadioStationHistory m_MusicTrackHistory;
	audRadioStationHistory m_TakeoverMusicTrackHistory;
	audRadioStationHistory m_IdentTrackHistory;
	audRadioStationHistory  m_DjSoloTrackHistory;
	audRadioStationHistory m_TakeoverIdentTrackHistory;
	audRadioStationHistory  m_TakeoverDjSoloTrackHistory;

	audRadioStationHistory  m_ToAdTrackHistory;
	audRadioStationHistory  m_ToNewsTrackHistory;
	audRadioStationHistory  m_UserIntroTrackHistory;
	audRadioStationHistory  m_UserOutroTrackHistory;
	audRadioStationHistory  m_UserIntroMorningTrackHistory;
	audRadioStationHistory  m_UserIntroEveningTrackHistory;

	static audRadioStationHistory sm_AdvertHistory;
	static audRadioStationHistory sm_NewsHistory;
	static audRadioStationHistory sm_WeatherHistory;

	const RadioStationSettings *m_StationSettings;
	atRangeArray<u32, g_MaxDjSpeechInHistory> m_DjSpeechHistory;
	atRangeArray<u32, NUM_RADIO_DJ_SPEECH_CATS> m_DjSpeechCategoryNextValidSelectionTime;
	
	f32 m_AmbientVolume;
	f32 m_FirstTrackStartOffset;
	f32 m_NewTrackBias;
	f32 m_FirstBootNewTrackBias;

	float m_ListenTimer;

	u32 m_ForceNextTrackContext;
	u32 m_LastForcedMusicTrackList;
	
	RadioGenre m_Genre;
	u32 m_NameHash;

	u32 m_MusicRunningCount;

	//mthRandom m_Random;
	audRadioPRNG16 m_Random;
	
	u16 m_ForceNextTrackIndex;
	u8 m_ForceNextTrackCategory;

	audRadioStationSyncData::TrackId m_NetworkNextTrack;

	u8 m_TakeoverMusicTrackCounter;
	float m_TakeoverProbability;
	float m_TakeoverDjSoloProbability;
	u32 m_TakeoverMinTimeMs;
	u32 m_TakeoverLastTimeMs;
	s32 m_TakeoverMinTracks;
	s32 m_TakeoverMaxTracks;
	u32 m_StationAccumulatedPlayTimeMs;
	u32 m_LastTimeDJSpeechPlaying;
	atRangeArray<s32, IMMUTABLE_INDEX_TYPE_MAX> m_StationImmutableIndex;
	
	u8 m_ActiveTrackIndex;
	u8 m_DjSpeechHistoryWriteIndex;
	u8 m_SoundBucketId;

	bool m_ShouldStreamPhysically;
	bool m_IsPlayingOverlappedTrack;
	bool m_IsPlayingMixTransition;
	bool m_IsRequestingOverlappedTrackPrepare;
	bool m_HasJustPlayedBackToBackMusic;
	bool m_HasJustPlayedBackToBackAds;
	bool m_ShouldPlayFullRadio;
	bool m_IsFirstTrack;
	bool m_PlaySequentialMusicTracks;
	bool m_IsMixStation;
	bool m_IsReverbStation;
	bool m_HasConstructedMixStationBeatMap;
	
	bool m_NoBackToBackMusic;
	bool m_PlaysBackToBackAds;
	bool m_PlayWeather;
	bool m_PlayNews;
	bool m_IsFrozen;
	bool m_PlayIdentsInsteadOfAds;	
	bool m_OnlyPlayAds;
	bool m_IsFavourited;

	bool m_ScriptSetMusicOnly;
	bool m_bHasTrackChanged; // Has the current track changed this frame?

	bool m_IsFirstMusicTrackSinceBoot;

	bool m_HasTakeoverContent;
	bool m_UseRandomizedStrideSelection;
	bool m_DLCInitialized;

	static audCurve sm_VehicleRadioRiseVolumeCurve;
	static audSimpleSmoother sm_DjDuckerVolumeSmoother;
	static audSound *sm_RetuneMonoSound;
	static audSound *sm_RetunePositionedSound;
	static audSound *sm_DjSpeechMonoSound;
	static audSound *sm_DjSpeechPositionedSound;
	static float sm_DjSpeechFrontendVolume;
	static float sm_DjSpeechFrontendLPF;
	static atArray<audRadioStation *> sm_OrderedRadioStations;
	
	static const RadioTrackCategoryData *sm_MinTimeBetweenRepeatedCategories;
	static const RadioTrackCategoryData *sm_RadioTrackCategoryWeightsFromMusic;
	static const RadioTrackCategoryData *sm_RadioTrackCategoryWeightsFromBackToBackMusic;
	static const RadioTrackCategoryData *sm_RadioTrackCategoryWeightsNoBackToBackMusic;
	static const RadioTrackCategoryData *sm_RadioTrackCategoryWeightsFromBackToBackMusicKult;
	static const RadioTrackCategoryData *sm_RadioTrackCategoryWeightsNoBackToBackMusicKult;
#if RSG_PC
	static const RadioTrackCategoryData *sm_MinTimeBetweenRepeatedCategoriesUser;
	static const RadioTrackCategoryData *sm_UserMusicWeightsFromBackToBackMusic;
	static const RadioTrackCategoryData *sm_UserMusicWeightsFromMusic;
#endif

	static atRangeArray<u8, kMaxNewsStories> sm_NewsStoryState;
	static s32 sm_ActiveDjSpeechStartTimeMs;
	static u32 sm_ActiveDjSpeechId;
	static u8 sm_ActiveDjSpeechCategory;
	static u32 sm_NumRadioStations;
	static u32 sm_NumUnlockedRadioStations;
	static u32 sm_NumUnlockedFavouritedStations;
	static bool sm_UpdateLockedStations;
	static u8 sm_DjSpeechState;
	static const audRadioStation *sm_DjSpeechStation;
	static u32 sm_DjSpeechNextTrackCategory;
	static bool sm_IMDrivenDjSpeech;
	static bool sm_IMDrivenDjSpeechScheduled;
	static u8 sm_IMDjSpeechCategory;
	static audRadioStation *sm_IMDjSpeechStation;
	
	static bool sm_DisableOverlap;
	static bool sm_PlayingEndCredits;
	static bool sm_IsNetworkModeHistoryActive;
	static bool sm_HasInitialisedStations;
#if __BANK
	static bool sm_DebugHistory;
#endif

#if RSG_PC // user music
	static audUserRadioTrackManager sm_UserRadioTrackManager;
	bool m_IsUserStation;

	static bool sm_IsInRecoveryMode;
	u32 m_LastPlayedTrackIndex;
	u32 m_CurrentlyPlayingTrackIndex;
	u32 m_BadTrackCount;
	static bool sm_ForceUserNextTrack;
	static bool sm_ForceUserPrevTrack;

	static bool sm_IsUserStationActive;
#if __BANK
	static void DebugDrawUserMusic();
#endif //__BANK

#endif

	static float sm_CountryTalkRadioSignal;
	static float sm_ScriptGlobalRadioSignalLevel;
};

#endif // NA_RADIO_ENABLED
#endif // AUD_RADIOSTATION_H



