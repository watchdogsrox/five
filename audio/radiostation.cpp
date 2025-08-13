// 
// audio/radiostation.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#include "audiodefines.h"

#if NA_RADIO_ENABLED

#include "radiostation.h"
#include "music/musicplayer.h"
#include "northaudioengine.h"
#include "radioaudioentity.h"
#include "radioslot.h"

#include "audioengine/engine.h"
#include "audiosoundtypes/speechsound.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverutil.h"
#include "audiohardware/submix.h"
#include "audiosynth/synthcore.h"
#include "grcore/debugdraw.h"
#include "game/clock.h"
#include "game/weather.h"
#include "parser/manager.h"
#include "parsercore/attribute.h"
#include "string/stringhash.h"
#include "spatialdata/sphere.h"
#include "streaming/streamingengine.h"
#include "system/bootmgr.h"
#include "network/NetworkInterface.h"
#include "network/events/NetworkEventTypes.h"
#include "network/live/NetworkTelemetry.h"

#include "system/filemgr.h"

#include "debugaudio.h"

AUDIO_MUSIC_OPTIMISATIONS()

PARAM(disableradiohistory, "[Audio] Disable radio track history");
PARAM(disableuserradio, "[Audio] Disable user radio");

audWaveSlot *g_RadioDjSpeechWaveSlot = NULL;

bool audRadioStation::sm_PlayingEndCredits = false;

f32 g_RadioDjDuckingLevelDb				= 8.0f;
f32 g_RadioDjDuckingLevelLin			= 0.0f;// computed from duckingleveldb in initclass
u32 g_RadioDjDuckTimeMs					= 150;

enum { kMinRadioTracksOutsideHistory = 2};

atRangeArray<u8, kMaxNewsStories> audRadioStation::sm_NewsStoryState;

audRadioStationHistory audRadioStation::sm_NewsHistory;
audRadioStationHistory audRadioStation::sm_WeatherHistory;
audRadioStationHistory audRadioStation::sm_AdvertHistory;
audRadioStationTrackListCollection audRadioStation::sm_NewsTrackLists;
audRadioStationTrackListCollection audRadioStation::sm_WeatherTrackLists;
u32 audRadioStation::sm_PositionedPlayerVehicleRadioLPFCutoff = kVoiceFilterLPFMaxCutoff;
u32 audRadioStation::sm_PositionedPlayerVehicleRadioHPFCutoff = kVoiceFilterHPFMinCutoff;
f32 audRadioStation::sm_PositionedPlayerVehicleRadioEnvironmentalLoudness = 0.f;
f32 audRadioStation::sm_PositionedPlayerVehicleRadioVolume = 0.f;
f32 audRadioStation::sm_PositionedPlayerVehicleRadioRollOff = 1.f;
bool audRadioStation::sm_HasSyncedUnlockableDJStation = false;

float audRadioStation::sm_CountryTalkRadioSignal = 1.f;
float audRadioStation::sm_ScriptGlobalRadioSignalLevel = 1.f;
bool audRadioStation::sm_UpdateLockedStations = false;
#if RSG_PC
bool audRadioStation::sm_IsInRecoveryMode = false;
#endif

bool audRadioStation::sm_HasInitialisedStations = false;

#define AUD_CLEAR_TRISTATE_VALUE(flagvar, flagid) (flagvar &= ~(0 | ((AUD_GET_TRISTATE_VALUE(flagvar, flagid)&0x03) << (flagid<<1))))

#if __BANK && 0
extern bool g_DrawRadioDebug;
extern audRadioStation *g_DebugRadioStation;
#define RADIODEBUG(arg) if(g_DrawRadioDebug && g_DebugRadioStation == this){Printf("[%s] ", GetName());Displayf(arg);}
#define RADIODEBUG2(arg1,arg2) if(g_DrawRadioDebug && g_DebugRadioStation == this){Printf("[%s] ", GetName());Displayf(arg1, arg2);}
#define RADIODEBUG3(arg1, arg2, arg3) if(g_DrawRadioDebug && g_DebugRadioStation == this){Printf("[%s] ", GetName());Displayf(arg1, arg2, arg3);}
#define RADIODEBUG4(arg1, arg2, arg3, arg4) if(g_DrawRadioDebug && g_DebugRadioStation == this){Printf("[%s] ", GetName());Displayf(arg1,arg2,arg3,arg4);}
#else
#define RADIODEBUG(arg)
#define RADIODEBUG2(arg1,arg2)
#define RADIODEBUG3(arg1, arg2, arg3)
#define RADIODEBUG4(arg1, arg2, arg3, arg4)
#endif

BANK_ONLY(bool audRadioStation::sm_DebugHistory = false);

enum audRadioTrackWeatherContexts
{
	RADIO_TRACK_WEATHER_CONTEXT_FOG,
	RADIO_TRACK_WEATHER_CONTEXT_RAIN,
	RADIO_TRACK_WEATHER_CONTEXT_WIND,
	RADIO_TRACK_WEATHER_CONTEXT_SUN,
	RADIO_TRACK_WEATHER_CONTEXT_CLOUD,
	NUM_RADIO_TRACK_WEATHER_CONTEXTS
};

static const u32 g_RadioTrackWeatherContextNames[NUM_RADIO_TRACK_WEATHER_CONTEXTS] =
{
	ATSTRINGHASH("FOG", 0xD61BDE01),//RADIO_TRACK_WEATHER_CONTEXT_FOG,
	ATSTRINGHASH("RAIN", 0x54A69840),//RADIO_TRACK_WEATHER_CONTEXT_RAIN,
	ATSTRINGHASH("WIND", 0x35369828),//RADIO_TRACK_WEATHER_CONTEXT_WIND,
	ATSTRINGHASH("SUN", 0x47AE791A),//RADIO_TRACK_WEATHER_CONTEXT_SUN,
	ATSTRINGHASH("CLOUD", 0x27F49EC9)//RADIO_TRACK_WEATHER_CONTEXT_CLOUD,
};

bank_s32 g_RadioTrackOverlapPlayTimeMs	= 1000;
bank_s32 g_RadioTrackMixtransitionTimeMs = 2000;
bank_s32 g_RadioTrackMixtransitionOffsetTimeMs = 0;
bank_s32 g_RadioTrackOverlapTimeMs		= 1000;
bank_s32 g_UserRadioTrackOverlapTimeMs	= 500;
bank_s32 g_RadioTrackPrepareTimeMs		= 7000;
bank_s32 g_RadioTrackSkipEndTimeMs		= 7000;

bool g_UsePreciseTrackCrossfades = true;

bank_u32 g_RadioWheelWindStart = 11000;
bank_u32 g_RadioWheelWindOffset = 50000;
bank_u32 g_RadioDjSpeechMinDelayMsMotomami = 10000;

bank_float g_RadioDjSpeechProbability	= 0.8f;		//Prob of DJ speech playing within a valid window.
bank_float g_RadioDjSpeechProbabilityKult = 0.7f;	//Prob of DJ speech playing within a valid window.
bank_float g_RadioDjOutroSpeechProbabilityMotomami = 0.275f;  //Prob of DJ speech playing within a valid window.
bank_float g_RadioDjTimeProbability		= 0.5f;		//Prob of time banter playing instead of an intro.

const s32 g_RadioDjMinPrepareTimeMs		= 3000;
const s32 g_RadioDjPlayTimeTresholdMs	= 500;
const u8  g_RadioDjMaxVariations		= 100;

const char *g_RadioDjSpeechCategoryNames[NUM_RADIO_DJ_SPEECH_CATS] =
{
	"INTRO",	//RADIO_DJ_SPEECH_CAT_INTRO
	"OUTRO",	//RADIO_DJ_SPEECH_CAT_OUTRO
	"GENERAL",	//RADIO_DJ_SPEECH_CAT_GENERAL
	"TIME",		//RADIO_DJ_SPEECH_CAT_TIME
	"TO"		//RADIO_DJ_SPEECH_CAT_TO
};

//The minimum amount of time that must pass (in milliseconds) between repeated playback of a DJ speech category.
const u32 g_RadioDjSpeechMinTimeBetweenRepeatedCategories[NUM_RADIO_DJ_SPEECH_CATS] =
{
	0,			//RADIO_DJ_SPEECH_CAT_INTRO
	0,			//RADIO_DJ_SPEECH_CAT_OUTRO
	45000,		//RADIO_DJ_SPEECH_CAT_GENERAL - 45s, to prevent back to back generals when driven by score
	1800000,	//RADIO_DJ_SPEECH_CAT_TIME		- 30mins
	0			//RADIO_DJ_SPEECH_CAT_TO
};

const u32 g_RadioDjSpeechMinTimeBetweenRepeatedCategoriesKult[NUM_RADIO_DJ_SPEECH_CATS] =
{
	540000,		//RADIO_DJ_SPEECH_CAT_INTRO		- 9 mins
	0,			//RADIO_DJ_SPEECH_CAT_OUTRO
	540000,		//RADIO_DJ_SPEECH_CAT_GENERAL   - 9 mins
	1800000,	//RADIO_DJ_SPEECH_CAT_TIME		- 30mins
	0			//RADIO_DJ_SPEECH_CAT_TO
};

const u8 g_RadioDjTimeMorningStart		= 5;
const u8 g_RadioDjTimeMorningEnd		= 9;
const u8 g_RadioDjTimeAfternoonStart	= 12;
const u8 g_RadioDjTimeAfternoonEnd		= 16;
const u8 g_RadioDjTimeEveningStart		= 17;
const u8 g_RadioDjTimeEveningEnd		= 20;
const u8 g_RadioDjTimeNightStart		= 21;
const u8 g_RadioDjTimeNightEnd			= 3;

bool g_DisablePlayTimeCompensation = false;

const char *g_RadioDjMorningContext		= "MORNING";
const char *g_RadioDjAfternoonContext	= "AFTERNOON";
const char *g_RadioDjEveningContext		= "EVENING";
const char *g_RadioDjNightContext		= "NIGHT";

const char *g_RadioDjToAdvertContext	= "TO_AD";
const char *g_RadioDjToNewsContext		= "TO_NEWS";
const char *g_RadioDjToWeatherContext	= "TO_WEATHER";

static char g_RadioDjVoiceName[g_MaxRadioNameLength];
static char g_RadioDjContextName[g_MaxRadioNameLength];

bool audRadioStation::sm_IsNetworkModeHistoryActive = false;


bool audRadioStation::sm_IMDrivenDjSpeech = false;
bool audRadioStation::sm_IMDrivenDjSpeechScheduled = false;
u8 audRadioStation::sm_IMDjSpeechCategory = 0;
audRadioStation *audRadioStation::sm_IMDjSpeechStation = NULL;

const audRadioStation *audRadioStation::sm_DjSpeechStation = NULL;
u32 audRadioStation::sm_DjSpeechNextTrackCategory = 0;

audCurve audRadioStation::sm_VehicleRadioRiseVolumeCurve;
audSimpleSmoother audRadioStation::sm_DjDuckerVolumeSmoother;
audSound *audRadioStation::sm_RetuneMonoSound = NULL;
audSound *audRadioStation::sm_RetunePositionedSound = NULL;
audSound *audRadioStation::sm_DjSpeechMonoSound = NULL;
audSound *audRadioStation::sm_DjSpeechPositionedSound = NULL;
atArray<audRadioStation *> audRadioStation::sm_OrderedRadioStations;
const RadioTrackCategoryData *audRadioStation::sm_MinTimeBetweenRepeatedCategories = NULL;
const RadioTrackCategoryData *audRadioStation::sm_RadioTrackCategoryWeightsFromMusic = NULL;
const RadioTrackCategoryData *audRadioStation::sm_RadioTrackCategoryWeightsFromBackToBackMusic = NULL;
const RadioTrackCategoryData *audRadioStation::sm_RadioTrackCategoryWeightsNoBackToBackMusic = NULL;
const RadioTrackCategoryData *audRadioStation::sm_RadioTrackCategoryWeightsFromBackToBackMusicKult = NULL;
const RadioTrackCategoryData *audRadioStation::sm_RadioTrackCategoryWeightsNoBackToBackMusicKult = NULL;
#if RSG_PC // user music
const RadioTrackCategoryData *audRadioStation::sm_MinTimeBetweenRepeatedCategoriesUser = NULL;
const RadioTrackCategoryData *audRadioStation::sm_UserMusicWeightsFromMusic = NULL;
#endif

#if HEIST3_HIDDEN_RADIO_ENABLED
u32 audRadioStation::sm_Heist3HiddenRadioBankId = ~0u;
#endif

s32 audRadioStation::sm_ActiveDjSpeechStartTimeMs = 0;
u32 audRadioStation::sm_ActiveDjSpeechId = 0;
u8 audRadioStation::sm_ActiveDjSpeechCategory = g_NullDjSpeechCategory;
u32 audRadioStation::sm_NumRadioStations = 0;
u32 audRadioStation::sm_NumUnlockedRadioStations = 0;
u32 audRadioStation::sm_NumUnlockedFavouritedStations = 0;
u8 audRadioStation::sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
bool audRadioStation::sm_DisableOverlap = false;
f32 audRadioStation::sm_DjSpeechFrontendVolume = 0.f;
f32 audRadioStation::sm_DjSpeechFrontendLPF = kVoiceFilterLPFMaxCutoff;

// user music additions to pc version
#if RSG_PC
bool audRadioStation::sm_ForceUserNextTrack = false;
bool audRadioStation::sm_ForceUserPrevTrack = false;
audUserRadioTrackManager audRadioStation::sm_UserRadioTrackManager;
#endif

#if __BANK
bool g_LogRadio = false;
bool g_ForceDjSpeech = false;
bool g_DebugRadioRandom = false;
u32 g_TestSyncStationIndex = 0;
extern f32 g_StationDebugDrawYScroll;

bool g_DebugDrawUSBStation = false;
bool g_HasAddedUSBStationWidgets = false;
u32 g_NumUSBStationTrackLists = 0;
s32 g_RequestedUSBTrackListComboIndex = -1;
u32 g_USBStationTrackListOffsetMs = 0;
atRangeArray<const char*, 20> g_USBTrackListNames;
bkCombo* g_USBStationTrackListCombo = nullptr;
bkGroup* g_USBStationGroup = nullptr;
bkBank* g_USBStationBank = nullptr;
#endif

s32 QSortRadioStationCompareFunc(audRadioStation* const* stationA, audRadioStation* const* stationB)
{
	return ((*stationA)->GetOrder() - (*stationB)->GetOrder());
}

void audRadioStation::SortRadioStationList()
{
	sm_OrderedRadioStations.QSort(0, sm_NumRadioStations, QSortRadioStationCompareFunc);
}

s32 audRadioStation::GetOrder() const
{
	s32 order = 0;
	if(m_StationSettings)
	{
		order = m_StationSettings->Order;

		// url:bugstar:7186452 - The ordering of the stations on the radio wheel has changed compared to previous builds
		// Slightly hacky fix to restore station ordering now that the off station has changed index. We just take all 
		// stations past a set order value and bump them to the back of the list, effectively rotating the station wheel
		if(order < g_RadioWheelWindStart)
		{
			order += g_RadioWheelWindOffset;
		}

		// url:bugstar:7186089 - On the Radio Wheel, is it possible to keep the Media Player next to the Radio Off option,
		// even when players have begun to thin the radio wheel down using the favourite station feature?
		if(IsUSBMixStation())
		{
			order = 1;
		}

		if(!m_IsFavourited)
		{
			order += 100000;
		}

		if(AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_LOCKED) == AUD_TRISTATE_TRUE)
		{
			order += 2000000;
		}
		else if(AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_HIDDEN) == AUD_TRISTATE_TRUE)
		{
			order += 4000000;
		}
	}

	return order;
}

enum { kMaxRadioStationLists = 16};
void audRadioStation::InitClass(void)
{
	//NOTE: This function must be called AFTER all downloadable audio content has been mounted and all game objects have been combined.
	// Otherwise downloadable radio content will be ignored.
	// HL: This comment was clearly never adhered to! Additional track lists can still be merged via after ::Init has been called. Risky
	// to change so have added ::DLCInit fn to do any deferred processing
	char radioStationListName[64];

	sm_ActiveDjSpeechStartTimeMs = 0;
	sm_ActiveDjSpeechId = 0;
	sm_ActiveDjSpeechCategory = g_NullDjSpeechCategory;
	sm_NumUnlockedRadioStations = 0;
	sm_NumUnlockedFavouritedStations = 0;
	sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;

	sm_OrderedRadioStations.Resize(g_MaxRadioStations);	

	sm_MinTimeBetweenRepeatedCategories = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_MIN_TIME_BETWEEN_REPEATS", 0x4BFB4679));
	WIN32PC_ONLY(sm_MinTimeBetweenRepeatedCategoriesUser = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_TIMES_SELF", 0x26EE6A9A)));
	sm_RadioTrackCategoryWeightsFromMusic = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_WEIGHTS_FROM_MUSIC", 0xA3D15F60));
	WIN32PC_ONLY(sm_UserMusicWeightsFromMusic = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_WEIGHTS_SELF", 0x45F45451)));
	sm_RadioTrackCategoryWeightsFromBackToBackMusic = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_WEIGHTS_FROM_BACK_TO_BACK_MUSIC", 0xDBA8201E));
	sm_RadioTrackCategoryWeightsNoBackToBackMusic = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_WEIGHTS_NO_BACK_TO_BACK_MUSIC", 0xB0EA87F1));

	sm_RadioTrackCategoryWeightsNoBackToBackMusic = sm_RadioTrackCategoryWeightsFromBackToBackMusic;

	sm_RadioTrackCategoryWeightsFromBackToBackMusicKult = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_WEIGHTS_FROM_BACK_TO_BACK_MUSIC_KULTFM", 0xAA636584));
	sm_RadioTrackCategoryWeightsNoBackToBackMusicKult = audNorthAudioEngine::GetObject<RadioTrackCategoryData>(ATSTRINGHASH("RADIO_TRACK_CATEGORY_WEIGHTS_FROM_MUSIC_KULTFM", 0x8FC9E246));

	if (!sm_RadioTrackCategoryWeightsFromBackToBackMusicKult)
	{
		sm_RadioTrackCategoryWeightsFromBackToBackMusicKult = sm_RadioTrackCategoryWeightsFromBackToBackMusic;
	}

	if (!sm_RadioTrackCategoryWeightsNoBackToBackMusicKult)
	{
		sm_RadioTrackCategoryWeightsNoBackToBackMusicKult = sm_RadioTrackCategoryWeightsNoBackToBackMusic;
	}

	// Radio initialisation overwrites metadata on init, so prevent RAVE edits from screwing with that
	BANK_ONLY(const_cast<audMetadataManager&>(audNorthAudioEngine::GetMetadataManager()).DisableEditsForObjectType(RadioStationTrackList::TYPE_ID));

	for(u32 listIndex=0; listIndex<kMaxRadioStationLists; listIndex++)
	{
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
		formatf(radioStationListName, "RADIO_STATIONS_%02u", listIndex + 1);

		RadioStationList *radioStationList = audNorthAudioEngine::GetObject<RadioStationList>(radioStationListName);
		if(radioStationList == NULL)
		{
			continue;
		}

		if(listIndex == 0)
		{
			//This is the list of shipped radio stations.
			InitRadioStationList(radioStationList);
		}
		else
		{
			//This a list of downloaded radio stations that needs to be merged into the master list.
			MergeRadioStationList(radioStationList);
		}
	}

	sm_VehicleRadioRiseVolumeCurve.Init(ATSTRINGHASH("VEHICLE_RADIO_RISE", 0x5B9052BA));

	f32 increaseDecreaseRate = 1.0f / (f32)g_RadioDjDuckTimeMs;
	sm_DjDuckerVolumeSmoother.Init(increaseDecreaseRate);
	g_RadioDjDuckingLevelLin = audDriverUtil::ComputeLinearVolumeFromDb(g_RadioDjDuckingLevelDb);
	g_RadioDjSpeechWaveSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("DJ_SPEECH", 0x06b9d8c6a));

	const RadioStationTrackList *trackList = NULL;

	// Locked news track lists are dealt with differently than others
	sm_NewsTrackLists.SetLockingEnabled(false);

	for(u32 index = 1; index < 64; index++)
	{
		char buf[16];
		formatf(buf, "RADIO_NEWS_%02d", index);
		trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(buf);
		if(trackList)
		{
			Displayf("Adding news track list: %s (%u tracks)", buf, trackList->numTracks);
			Assert(trackList->Category == RADIO_TRACK_CAT_NEWS);
			sm_NewsTrackLists.AddTrackList(trackList);
		}
	}

	trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(ATSTRINGHASH("RADIO_WEATHER_01", 0x09a5d886f));
	Assert(trackList && trackList->Category == RADIO_TRACK_CAT_WEATHER);

	sm_WeatherTrackLists.AddTrackList(trackList);

	for(u32 index = 2; index < 64; index++)
	{
		char buf[32];
		formatf(buf, "RADIO_WEATHER_%02d", index);
		trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(buf);
		if(trackList)
		{
			Displayf("Adding weather track list: %s (%u tracks)", buf, trackList->numTracks);
			Assert(trackList->Category == RADIO_TRACK_CAT_WEATHER);
			sm_WeatherTrackLists.AddTrackList(trackList);
		}
	}

	Assert(sm_NewsTrackLists.ComputeNumTracks() < kMaxNewsStories);

#if RSG_PC
	sm_ForceUserNextTrack = false;
	sm_ForceUserPrevTrack = false;
	
	sm_UserRadioTrackManager.Initialise();
	if(	CPauseMenu::GetMenuPreference(PREF_UR_AUTOSCAN) )
	{
		// Quick scan only on boot
		sm_UserRadioTrackManager.StartMediaScan(false);
		
		while(sm_UserRadioTrackManager.IsScanning())
		{
			sysIpcSleep(10);
		}

		sm_UserRadioTrackManager.UpdateScanningUI(false);
	}
	sm_UserRadioTrackManager.LoadFileLookup();

	// Create a user music station
	RadioStationSettings *stationSettings = audNorthAudioEngine::GetObject<RadioStationSettings>("RADIO_19_USER");
	if(stationSettings)
	{
		sm_OrderedRadioStations[sm_NumRadioStations] = rage_new audRadioStation(stationSettings);
		sm_OrderedRadioStations[sm_NumRadioStations]->SetLocked(!HasUserTracks() || PARAM_disableuserradio.Get());
		sm_NumRadioStations++;
	}
		
#endif // RSG_PC

	//Order radio stations.
	sm_OrderedRadioStations.QSort(0, sm_NumRadioStations, QSortRadioStationCompareFunc);

	for(u32 stationIndex=0; stationIndex < sm_NumRadioStations; stationIndex++)
	{
		if(sm_OrderedRadioStations[stationIndex])
		{
			sm_OrderedRadioStations[stationIndex]->Init();

			if(!sm_OrderedRadioStations[stationIndex]->IsLocked() && !sm_OrderedRadioStations[stationIndex]->IsHidden())
			{
				sm_NumUnlockedRadioStations++;

				if(sm_OrderedRadioStations[stationIndex]->IsFavourited())
				{
					sm_NumUnlockedFavouritedStations++;
				}
			}
		}
	}

	// Flag that any stations added after this point (ie via DLC rather than patch packs/core) require explicit init
	sm_HasInitialisedStations = true;

#if __BANK
	if (!sysBootManager::IsPackagedBuild())
	{
		FileHandle fi = CFileMgr::OpenFileForWriting("common:/RadioLog.csv");
		if(fi != INVALID_FILE_HANDLE)
		{
			const char *header = "Station,TimeInMs,Category,Index,Context,Sound\r\n";
			CFileMgr::Write(fi, header, istrlen(header));
			CFileMgr::CloseFile(fi);
		}
	}
#endif

}

void audRadioStation::PostLoad()
{
#if RSG_PC
	if(m_IsUserStation)
	{
		sm_UserRadioTrackManager.PostLoad();
	}
#endif
}

#if RSG_PC
bool audRadioStation::HasUserTracks()
{
	return sm_UserRadioTrackManager.HasTracks();
}
#endif

s32 QSortBeatMarkerCompareFunc(audRadioStation::MixBeatMarker const* beatMarkerA, audRadioStation::MixBeatMarker const* beatMarkerB)
{
	s32 playTimeA = (*beatMarkerA).playtimeMs;
	s32 playTimeB = (*beatMarkerB).playtimeMs;

	s32 trackA = (s32)((*beatMarkerA).trackIndex);
	s32 trackB = (s32)((*beatMarkerB).trackIndex);

	if (playTimeA != playTimeB)
	{
		return playTimeA - playTimeB;
	}
	else
	{
		return trackA - trackB;
	}
}

void audRadioStation::ConstructMixStationBeatMarkerList()
{
	const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);

	if (trackLists)
	{		
		const u32 numTracks = trackLists->ComputeNumTracks(0u, true);

		for (u32 trackIndex = 0; trackIndex < numTracks; trackIndex++)
		{
			const u32 soundRef = trackLists->GetTrack(trackIndex, 0u, true)->SoundRef;
			u32 totalStationDuration = 0;
			u32 trackStartOffsetMs = 0;
			u32 thisTrackDuration = 0;			

			// Iterate over all the tracks and grab the total station duration, duration of the given track, and offset of the given track
			for (u32 i = 0; i < numTracks; i++)
			{
				const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(trackLists->GetTrack(i, 0u, true)->SoundRef);

				if (streamingSound)
				{
					if (i < trackIndex)
					{
						trackStartOffsetMs += streamingSound->Duration;
					}

					if (i == trackIndex)
					{
						thisTrackDuration = streamingSound->Duration;
					}

					totalStationDuration += streamingSound->Duration;
					m_MixStationBeatMarkerTrackDurations.PushAndGrow(streamingSound->Duration);
				}
			}

			char textIdObjName[16];
			formatf(textIdObjName, "RTB_%08X", audRadioTrack::GetBeatSoundName(soundRef));
			const RadioTrackTextIds *beatMarkers = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);

			if (beatMarkers)
			{
				u32 numBeatMarkers = beatMarkers->numTextIds;

				// Now add all the beat markers for the current track, offset by the tracks position into the mix
				for (u32 i = 0; i < numBeatMarkers; i++)
				{
					MixBeatMarker mixBeatMarker;
					mixBeatMarker.numBeats = beatMarkers->TextId[i].TextId;
					mixBeatMarker.playtimeMs = beatMarkers->TextId[i].OffsetMs + trackStartOffsetMs;
					mixBeatMarker.trackIndex = trackIndex;
					m_MixStationBeatMarkers.PushAndGrow(mixBeatMarker);
				}

				// Duplicate all the first track's beat markers at the end of the beat marker list
				if (trackIndex == 0)
				{
					for (u32 i = 0; i < numBeatMarkers; i++)
					{
						MixBeatMarker mixBeatMarker;
						mixBeatMarker.numBeats = beatMarkers->TextId[i].TextId;
						mixBeatMarker.playtimeMs = beatMarkers->TextId[i].OffsetMs + totalStationDuration;
						mixBeatMarker.trackIndex = trackIndex;
						m_MixStationBeatMarkers.PushAndGrow(mixBeatMarker);
					}
				}

				// Duplicate all the last track's beat markers at the beginning of the beat marker list
				if (trackIndex == numTracks - 1)
				{
					for (u32 i = 0; i < numBeatMarkers; i++)
					{
						MixBeatMarker mixBeatMarker;
						mixBeatMarker.numBeats = beatMarkers->TextId[i].TextId;
						mixBeatMarker.playtimeMs = beatMarkers->TextId[i].OffsetMs - thisTrackDuration;
						mixBeatMarker.trackIndex = trackIndex;
						m_MixStationBeatMarkers.PushAndGrow(mixBeatMarker);
					}
				}
			}
		}
	}
	
	m_MixStationBeatMarkers.QSort(0, m_MixStationBeatMarkers.GetCount(), QSortBeatMarkerCompareFunc);
}

#if __BANK
void audRadioStation::DebugDrawMixBeatMarkers() const
{
	char tempString[256];

	f32 yStepRate = 0.015f;
	f32 xCoord = 0.05f;
	f32 yCoord = 0.05f - g_StationDebugDrawYScroll;
	f32 yCoordUnscrolled = 0.05f;
	
	const audRadioTrack& beatTrack = GetMixStationBeatTrack();

	formatf(tempString, "Currently audible track: %s (ZiT: %u)", beatTrack.GetBankName(), beatTrack.GetTextId());
	grcDebugDraw::Text(Vector2(0.2f, yCoordUnscrolled), Color32(255, 255, 255), tempString);
	yCoordUnscrolled += yStepRate;

	formatf(tempString, "(Play time: %d/%u, %.02f%%, %d remaining)", beatTrack.GetPlayTime(), beatTrack.GetDuration(), (beatTrack.GetPlayTime() / (f32)beatTrack.GetDuration()) * 100.f, beatTrack.GetDuration() - beatTrack.GetPlayTime());
	grcDebugDraw::Text(Vector2(0.2f, yCoordUnscrolled), Color32(255, 255, 255), tempString);
	yCoordUnscrolled += yStepRate;

	float bpm;
	s32 beatNumber;
	float beatTimeS;
	if (beatTrack.GetNextBeat(beatTimeS, bpm, beatNumber))
	{
		formatf(tempString, "Audible beat: %.2f (%u/4 %.1f BPM)", beatTimeS, beatNumber, bpm);
		grcDebugDraw::Text(Vector2(0.2f, yCoordUnscrolled), Color32(255, 255, 255), tempString);		
	}
	else
	{
		formatf(tempString, "No audible beat found!");
		grcDebugDraw::Text(Vector2(0.2f, yCoordUnscrolled), Color32(255, 255, 255), tempString);
	}

	yCoordUnscrolled += yStepRate;
	s32 currentPlaytimeInMs = beatTrack.GetPlayTime();

	for (u32 i = 0; i < beatTrack.GetTrackIndex(); i++)
	{
		currentPlaytimeInMs += m_MixStationBeatMarkerTrackDurations[i];
	}

	s32 nearestEarlierMarkerTimeMs = -INT_MAX;
	s32 nearestLaterMarkerTimeMs = INT_MAX;
	s32 nearestEarlierMarkerIndex = -1;
	s32 nearestLaterMarkerIndex = -1;

	for (u32 i = 0; i < m_MixStationBeatMarkers.GetCount(); i++)
	{
		if (m_MixStationBeatMarkers[i].playtimeMs >= nearestEarlierMarkerTimeMs && m_MixStationBeatMarkers[i].playtimeMs <= currentPlaytimeInMs)
		{
			nearestEarlierMarkerTimeMs = m_MixStationBeatMarkers[i].playtimeMs;
			nearestEarlierMarkerIndex = i;
		}
		else if (m_MixStationBeatMarkers[i].playtimeMs > currentPlaytimeInMs && m_MixStationBeatMarkers[i].playtimeMs < nearestLaterMarkerTimeMs)
		{
			nearestLaterMarkerTimeMs = m_MixStationBeatMarkers[i].playtimeMs;
			nearestLaterMarkerIndex = i;
		}
	}	

	formatf(tempString, "Track Relative Play Time: %d", currentPlaytimeInMs);
	grcDebugDraw::Text(Vector2(0.2f, yCoordUnscrolled), Color32(255, 255, 255), tempString);
	yCoordUnscrolled += yStepRate;

	for (s32 i = 0; i < m_MixStationBeatMarkers.GetCount(); i++)
	{
		Color32 color = Color32(255, 255, 255);

		if (i == nearestEarlierMarkerIndex) { color = Color32(255, 128, 255); }
		else if (i == nearestLaterMarkerIndex) { color = Color32(128, 255, 255); }

		formatf(tempString, "%d : %d (Track %d)", m_MixStationBeatMarkers[i].playtimeMs, m_MixStationBeatMarkers[i].numBeats, m_MixStationBeatMarkers[i].trackIndex);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), color, tempString);
		yCoord += yStepRate;
	}
}
#endif

const audRadioTrack& audRadioStation::GetMixStationBeatTrack() const
{	
	// Detect when we've transitioned to the next track already
	if (!GetCurrentTrack().IsStreamingSoundValid() && GetNextTrack().IsStreamingSoundValid())
	{
		return GetNextTrack();
	}

	return GetCurrentTrack();
}

bool audRadioStation::GetNextMixStationBeat(f32& beatTimeS, f32& bpm, s32& beatNumber)
{
	const audRadioTrack& beatTrack = GetMixStationBeatTrack();
	u32 trackIndex = beatTrack.GetTrackIndex();
	s32 currentPlaytimeInMs = Max(0, beatTrack.GetPlayTime());

	// Convert our local play time into mix play time	
	if (trackIndex < m_MixStationBeatMarkerTrackDurations.GetCount())
	{
		for (u32 i = 0; i < trackIndex; i++)
		{
			currentPlaytimeInMs += m_MixStationBeatMarkerTrackDurations[i];
		}

		const f32 currentPlaytimeInSeconds = currentPlaytimeInMs*0.001f;
		s32 nearestEarlierMarkerTimeMs = -INT_MAX;
		s32 nearestLaterMarkerTimeMs = INT_MAX;
		u32 numBeats = 0;

		for (u32 i = 0; i < m_MixStationBeatMarkers.GetCount(); i++)
		{
			s32 markerPlayTimeMs = m_MixStationBeatMarkers[i].playtimeMs;

			// If this is the next track in the sequence, treat it as though it started 1000ms earlier to cope with the overlap period
			if (m_MixStationBeatMarkers[i].trackIndex == trackIndex + 1 % m_MixStationBeatMarkerTrackDurations.GetCount())
			{
				markerPlayTimeMs -= g_RadioTrackOverlapTimeMs;
			}
			// If this is the previous track in the sequence, treat it as though it started 1000ms later to cope with the overlap period
			else if(m_MixStationBeatMarkers[i].trackIndex == (trackIndex + m_MixStationBeatMarkerTrackDurations.GetCount() - 1) % m_MixStationBeatMarkerTrackDurations.GetCount())
			{
				markerPlayTimeMs += g_RadioTrackOverlapTimeMs;
			}

			if (markerPlayTimeMs >= nearestEarlierMarkerTimeMs && markerPlayTimeMs <= currentPlaytimeInMs)
			{
				nearestEarlierMarkerTimeMs = markerPlayTimeMs;
				numBeats = m_MixStationBeatMarkers[i].numBeats;
			}
			else if (markerPlayTimeMs > currentPlaytimeInMs && markerPlayTimeMs < nearestLaterMarkerTimeMs)
			{
				nearestLaterMarkerTimeMs = markerPlayTimeMs;
			}
		}
		
		const f32 nearestBarTimeInSeconds = nearestEarlierMarkerTimeMs*0.001f;
		const f32 timeSinceBar = currentPlaytimeInSeconds - nearestBarTimeInSeconds;

		if (numBeats == 0)
		{
			bpm = 0.f;
			beatNumber = 0;
			beatTimeS = 0.f;
			return false;
		}

		// markers occur n beats
		const f32 beatDuration = (nearestLaterMarkerTimeMs - nearestEarlierMarkerTimeMs) / (numBeats * 1000.f);
		bpm = 60.f / beatDuration;
		const f32 beatsPastLastBar = timeSinceBar / beatDuration;
		const f32 currentBeat = Floorf(beatsPastLastBar);
		beatNumber = 1 + ((u32)currentBeat) % 4;
		const f32 timeSinceLastBeat = (beatsPastLastBar - currentBeat) * beatDuration;
		beatTimeS = beatDuration - timeSinceLastBeat;
	}

	return true;
}

void audRadioStation::InitRadioStationList(RadioStationList *radioStationList)
{
	for(u32 stationIndex=0; stationIndex < radioStationList->numStations; stationIndex++)
	{
		RadioStationSettings *stationSettings = 
			audNorthAudioEngine::GetObject<RadioStationSettings>(radioStationList->Station[stationIndex].StationSettings);

		if(stationSettings)
		{
			//Create a new radio station using these settings.
			sm_OrderedRadioStations[sm_NumRadioStations] = rage_new audRadioStation(stationSettings);
			sm_NumRadioStations++;
		}
	}
}

void audRadioStation::MergeRadioStationList(const RadioStationList *radioStationList)
{
	bool needSort = false;
	for(u32 stationIndex = 0; stationIndex < radioStationList->numStations; stationIndex++)
	{
		RadioStationSettings *stationSettings = 
			audNorthAudioEngine::GetObject<RadioStationSettings>(radioStationList->Station[stationIndex].StationSettings);

		audRadioStation *station = FindStation(stationSettings->Name);
		if(station)
		{
			//These are additional tracks for an existing station.

			//Append the tracklists.
			for(u32 trackListIndex = 0; trackListIndex < stationSettings->numTrackLists; trackListIndex++)
			{
				const RadioStationTrackList *trackList = 
					audNorthAudioEngine::GetObject<RadioStationTrackList>(stationSettings->TrackList[trackListIndex].TrackListRef);

				if (trackList && trackList->Category == RADIO_TRACK_CAT_MUSIC)
				{
					station->AddMusicTrackList(trackList, stationSettings->TrackList[trackListIndex].TrackListRef);			
				}
				else
				{
					station->AddTrackList(trackList);
				}

				if (trackList->Category != RADIO_TRACK_CAT_MUSIC)
				{
					if(sm_IsNetworkModeHistoryActive)
					{
						station->m_HasTakeoverContent |= (trackList->Category == RADIO_TRACK_CAT_TAKEOVER_MUSIC);
					}
				}
			}

			if(station->m_HasTakeoverContent)
			{
				station->m_UseRandomizedStrideSelection = true;
			}

			if(station->m_UseRandomizedStrideSelection)
			{
				station->RandomizeCategoryStrides(true, false);
			}
		}
		else
		{
			//This is a new station.

			//Create a new radio station using these settings.
			audRadioStation *station = rage_new audRadioStation(stationSettings);

			if(sm_OrderedRadioStations.GetCount() == 0)
			{
				sm_OrderedRadioStations.Resize(g_MaxRadioStations);	
			}
				
			sm_OrderedRadioStations[sm_NumRadioStations] = station;
			sm_NumRadioStations++;

			if(sm_HasInitialisedStations)
			{
				station->Init();
				needSort = true;
			}
		}
	}
	if(needSort)
	{
		sm_OrderedRadioStations.QSort(0, sm_NumRadioStations, QSortRadioStationCompareFunc);
	}

#if HEIST3_HIDDEN_RADIO_ENABLED
	sm_Heist3HiddenRadioBankId = audWaveSlot::FindBankId(HEIST3_HIDDEN_RADIO_BANK_HASH);
#endif

	BANK_ONLY(audRadioAudioEntity::PopulateRetuneWidgetStationNames();)
}

void audRadioStation::RemoveRadioStationList(const RadioStationList *radioStationList)
{
	for(u32 stationIndex = 0; stationIndex < radioStationList->numStations; stationIndex++)
	{
		const RadioStationSettings *stationSettings = 
			audNorthAudioEngine::GetObject<RadioStationSettings>(radioStationList->Station[stationIndex].StationSettings);

		audRadioStation *station = FindStation(stationSettings->Name);

		if(station)
		{
			audMetadataObjectInfo objectInfo;
			bool isCoreStation = audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(station->GetNameHash(), 0, objectInfo);

			audRadioSlot *slot = audRadioSlot::FindSlotByStation(station);
			if(slot)
			{
				audWarningf("Stopping associated slot");
				slot->InvalidateStation();
			}

			if (isCoreStation)
			{
				// url:bugstar:6846455
				// If the station existed in core and DLC data, we want to fall back to the core version 
				// of the station rather than deleting it completely (as the core data will never be re-mounted)
				audWarningf("Reverting DLC station %s to core version", stationSettings->Name);
				const s32 index = sm_OrderedRadioStations.Find(station);

				station->Shutdown();
				delete station;

				if (audVerifyf(index >= 0, "Failed to find deleted station in station settings list!"))
				{
					RadioStationSettings* coreStationSettings = const_cast<RadioStationSettings*>(objectInfo.GetObject<RadioStationSettings>());
					sm_OrderedRadioStations[index] = rage_new audRadioStation(coreStationSettings);
					sm_OrderedRadioStations[index]->Init();
				}
			}
			else
			{
				audWarningf("Removing DLC station %s", stationSettings->Name);

				// This was a new station, so just remove it
				sm_OrderedRadioStations.DeleteMatches(station);
				sm_OrderedRadioStations.Resize(g_MaxRadioStations);
				station->Shutdown();
				delete station;
				sm_NumRadioStations--;
			}
		}
	}
}

void audRadioStation::AddMusicTrackList(const RadioStationTrackList *trackList, audMetadataRef metadataRef)
{	
	audDisplayf("Adding music track list %s on station %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset), GetName());

	u32 trackListHash = audNorthAudioEngine::GetMetadataManager().GetObjectHashFromMetadataRef(metadataRef);
	audAssert(trackListHash == atStringHash(audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset)));

	f32 trackListProbability = 1.f;

	if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, trackListHash, trackListProbability))
	{
		audDisplayf("Custom first track probability tuneable (%.02f) found for track list %s on station %s",
			trackListProbability,
			audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset),
			GetName());
	}	
	
	AddTrackList(trackList, trackListProbability);
}

void audRadioStation::AddTrackList(const RadioStationTrackList *trackList, f32 firstTrackProbability)
{	
	if(!audVerifyf(trackList, "NULL tracklist passed to %s", GetName()))
	{
		return;
	}

	audDisplayf("Adding %s track list %s on station %s", TrackCats_ToString((TrackCats)trackList->Category), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset), GetName());

	switch(trackList->Category)
	{
	case RADIO_TRACK_CAT_DJSOLO:
	case RADIO_TRACK_CAT_IDENTS:
	case RADIO_TRACK_CAT_MUSIC:
	case RADIO_TRACK_CAT_ADVERTS:
	case RADIO_TRACK_CAT_TO_AD:
	case RADIO_TRACK_CAT_TO_NEWS:
	case RADIO_TRACK_CAT_USER_INTRO:
	case RADIO_TRACK_CAT_INTRO_MORNING:
	case RADIO_TRACK_CAT_INTRO_EVENING:
	case RADIO_TRACK_CAT_USER_OUTRO:
	case RADIO_TRACK_CAT_TAKEOVER_MUSIC:
	case RADIO_TRACK_CAT_TAKEOVER_DJSOLO:
	case RADIO_TRACK_CAT_TAKEOVER_IDENTS:
		{
			audRadioStationTrackListCollection *trackLists = const_cast<audRadioStationTrackListCollection*>(FindTrackListsForCategory(static_cast<s32>(trackList->Category)));
			if(trackLists)
			{				
				audVerifyf(trackLists->AddTrackList(trackList, firstTrackProbability), "Failed to add tracklist %s to station %s (duplicate?)", 
					audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset), 
					GetName());			
			}
		}

		if(trackList->Category != RADIO_TRACK_CAT_ADVERTS)
		{
			m_OnlyPlayAds = false;
		}
		break;
	
	// Shared track lists are handled separately
	case RADIO_TRACK_CAT_WEATHER:
	case RADIO_TRACK_CAT_NEWS:
		break;
	}

	
}

void audRadioStation::ResetHistoryForNewGame()
{
	audDebugf1("Radio: Resetting history for new game");
	// Reset news state based on track lists
	u32 curIndex = 0;
	for(s32 listIndex = 0; listIndex < sm_NewsTrackLists.GetListCount(); listIndex++)
	{
		const RadioStationTrackList *trackList = sm_NewsTrackLists.GetList(listIndex);

		for(u32 i = 0; i < trackList->numTracks; i++)
		{
			Assign(sm_NewsStoryState[i + curIndex], (AUD_GET_TRISTATE_VALUE(trackList->Flags, FLAG_ID_RADIOSTATIONTRACKLIST_LOCKED) == AUD_TRISTATE_TRUE ? RADIO_NEWS_ONCE_ONLY : RADIO_NEWS_UNLOCKED));
		}
		curIndex += trackList->numTracks;
	}

	for(s32 stationIndex = 0; stationIndex < sm_OrderedRadioStations.GetCount(); stationIndex++)
	{
		audRadioStation *station = GetStation(stationIndex);
		if(station)
		{
			for(s32 category = RADIO_TRACK_CAT_ADVERTS; category < NUM_RADIO_TRACK_CATS; category++)
			{
				audRadioStationTrackListCollection *trackLists = const_cast<audRadioStationTrackListCollection*>(station->FindTrackListsForCategory(category));
				if(trackLists)
				{
					trackLists->ResetLockState();
				}
			}
		}
	}
	
	sm_PlayingEndCredits = false;	
	Reset();
}

void audRadioStation::ResetHistoryForNetworkGame()
{
	// Only reset when transitioning into MP from SP
	if(!sm_IsNetworkModeHistoryActive)
	{
		audDebugf1("Radio: Resetting history for network game");
		sm_IsNetworkModeHistoryActive = true;
		for(u32 index=0; index<sm_NumRadioStations; index++)
		{
			audRadioStation *station = GetStation(index);
			if(station WIN32PC_ONLY(&& !station->IsUserStation()) && station->GetNameHash() != ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_RADIO", 0xF8BEAA16))
			{
				if(!station->m_PlaySequentialMusicTracks && !station->m_UseRandomizedStrideSelection)
				{
					station->m_MusicTrackHistory.Reset();
				}

				s32 activeTrackIndex = station->GetMusicTrackListActiveTrackIndex();
				f32 trackOffset = station->GetCurrentPlayFractionOfActiveTrack();

				// Need to shutdown active tracks to ensure we generate network compatible track IDs for history
				// This also resets the FirstTrack flag ensuring we randomise the start position on the first track (important for mix stations)
				station->Shutdown();

				if (station->m_PlaySequentialMusicTracks)
				{
					bool trackListStateChanged = false;

					// Probably safe to do this for all tracks, but I'm paranoid about accidentally unlocking something that was intentionally locked!
					if (station->GetNameHash() == ATSTRINGHASH("RADIO_14_DANCE_02", 0xC7A2843A) || station->GetNameHash() == ATSTRINGHASH("RADIO_13_JAZZ", 0x5A254955))
					{						
						const audRadioStationTrackListCollection* trackLists = station->FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);

						for (s32 trackListIndex = 0; trackListIndex < trackLists->GetListCount(); trackListIndex++)
						{
							if (trackLists->GetListFirstTrackProbability(trackListIndex) == -1)
							{
								if (!trackLists->IsListLocked(trackListIndex))
								{
									audDisplayf("%s is locking unlocked track list %d due to tuneable", station->GetName(), trackListIndex);
									station->LockTrackList(trackLists->GetList(trackListIndex));
									trackListStateChanged = true;
								}
							}
							else
							{
								if (trackLists->IsListLocked(trackListIndex))
								{
									audDisplayf("%s is unlocking locked track list %d due to tuneable", station->GetName(), trackListIndex);
									station->UnlockTrackList(trackLists->GetList(trackListIndex));
									trackListStateChanged = true;
								}
							}
						}
					}
					
					if (trackListStateChanged)
					{
						audDisplayf("Track list state changed on %s, re-randomizing", station->GetName());
						station->RandomizeSequentialStationPlayPosition();
					}
					else
					{
						audRadioStationHistory *history = const_cast<audRadioStationHistory*>(station->FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
						(*history)[0] = activeTrackIndex;
						station->m_FirstTrackStartOffset = trackOffset;
					}
				}
			}
		}

		// GTA V nextgen B*2118827 fix
		audRadioStation *radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_16_SILVERLAKE", 0x5E61F5CF));
		if(audVerifyf(radioStation, "Invalid radio station name - RADIO_16_SILVERLAKE"))
		{
			if(!radioStation->UnlockTrackList("MIRRORPARK_LOCKED"))
			{
				audWarningf("Failed to unlock tracklist MIRRORPARK_LOCKED on RADIO_16_SILVERLAKE");
			}
		}
	}
}

void audRadioStation::ResetHistoryForSPGame()
{
	audDebugf1("Radio: Resetting history for SP game");
	if(sm_IsNetworkModeHistoryActive)
	{
		sm_IsNetworkModeHistoryActive = false;
		Reset();
	}	

	// GTA V nextgen B*2118827 fix
	audRadioStation *radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_16_SILVERLAKE", 0x5E61F5CF));
	if(audVerifyf(radioStation, "Invalid radio station name - RADIO_16_SILVERLAKE"))
	{
		if(!radioStation->LockTrackList("MIRRORPARK_LOCKED"))
		{
			audWarningf("Failed to lock tracklist MIRRORPARK_LOCKED on RADIO_16_SILVERLAKE");
		}
	}

	MaintainHeist4SPStationState();

	for (u32 i = 0; i < sm_NumRadioStations; i++)
	{
		if (GetStation(i)->m_PlaySequentialMusicTracks)
		{
			GetStation(i)->RandomizeSequentialStationPlayPosition();
		}
	}
}

void audRadioStation::MaintainHeist4SPStationState()
{
	audDisplayf("Maintaining WWFM/Flylo Radio state for SP");

	// TEMP - Force SP stations to remain in their original configuration regardless of what the tunables say
	// url:bugstar:6819975 - Block FlyLo and WWFM additional content in SP
	audRadioStation* radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_14_DANCE_02", 0xC7A2843A));

	if (radioStation)
	{
		radioStation->UnlockTrackList(ATSTRINGHASH("RADIO_14_DANCE_02_MUSIC", 0xA53FB7D7));
		radioStation->LockTrackList(ATSTRINGHASH("FLYLO_ISLAND_UPDATE_MUSIC_MUSIC", 0x5DD0DB99));
	}

	radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_13_JAZZ", 0x5A254955));

	if (radioStation)
	{
		radioStation->UnlockTrackList(ATSTRINGHASH("RADIO_13_JAZZ_MUSIC_EDITED", 0x3E56DC28));
		radioStation->LockTrackList(ATSTRINGHASH("WWFM_ISLAND_UPDATE_MUSIC_MUSIC", 0x81757B19));
	}
}

void audRadioStation::ShutdownClass(void)
{
	Reset();
	sm_OrderedRadioStations.Reset();
	sm_NumRadioStations = 0;
	sm_NumUnlockedRadioStations = 0;
	sm_NumUnlockedFavouritedStations = 0;
}

bool audRadioStation::IsRandomizedStrideCategory(const u32 category)
{
	// Takeover station stores the last chosen track for each valid category into the sync packet history slot array. Because
	// we have more categories than we have history slots (we also need to steal some slots for storing next takeover time, .etc), 
	// we need to selectively sync only those categories that we actually need, otherwise we'll run out of space
	if (category == RADIO_TRACK_CAT_MUSIC ||
		category == RADIO_TRACK_CAT_IDENTS ||
		category == RADIO_TRACK_CAT_TAKEOVER_DJSOLO ||
		category == RADIO_TRACK_CAT_TAKEOVER_IDENTS ||
		category == RADIO_TRACK_CAT_TAKEOVER_MUSIC ||
		category == RADIO_TRACK_CAT_DJSOLO)
	{
		return true;
	}

	return false;
}

audRadioStation::audRadioStationSyncData g_TestSyncData;

void audRadioStation::SyncStation(const audRadioStationSyncData &data)
{
	if(!sm_IsNetworkModeHistoryActive)
	{		
		audDisplayf("Radio station attempting to sync, but not in network history mode. Resetting history for network game");
		ResetHistoryForNetworkGame();
	}

	if(m_OnlyPlayAds || IsUSBMixStation() || IsLocalPlayerNewsStation() WIN32PC_ONLY(|| m_IsUserStation))
		return;

	if(m_IsFrozen)
	{
		audDisplayf("%s - skipping sync as station is currently frozen", GetName());
		return;
	}

	if(!audVerifyf(sm_IsNetworkModeHistoryActive, "Attempting to sync radio station %s, but we're not in network radio mode! Aborting.", GetName()))
	{
		return;
	}

	audDisplayf("Received radio station sync data for station %s (Local Player: %s, Host: %s, Synced Time: %u):", GetName(), NetworkInterface::GetLocalGamerName(), NetworkInterface::GetHostName(), NetworkInterface::GetSyncedTimeInMilliseconds());

	// Play a burst of retune noise to cover the transition
	if(g_RadioAudioEntity.IsPlayerRadioActive() && g_RadioAudioEntity.GetPlayerRadioStation() == this)
	{
		UpdateRetuneSounds();
	}
	
	m_Random.Reset(data.randomSeed);

	m_IsFirstTrack = false;

	s32 category;
	u32 index;

	if (m_UseRandomizedStrideSelection)
	{
		u32 historyIndex = 0;

		// Current track is stored in history index 0
		category = 0xFFFF & UnpackCategoryFromNetwork(data.trackHistory[historyIndex].category);
		index = data.trackHistory[historyIndex++].index;

		audRadioStationPackedInt packedAccumulatedPlayTime;
		audRadioStationPackedInt packedLastTakeoverTime;

		// History index 1-8 store some additional packed data required for takeover stations
		packedAccumulatedPlayTime.PackedData.m_Byte0 = data.trackHistory[historyIndex].index;
		m_MusicRunningCount = data.trackHistory[historyIndex++].category;

		packedAccumulatedPlayTime.PackedData.m_Byte1 = data.trackHistory[historyIndex].index;
		m_TakeoverMusicTrackCounter = data.trackHistory[historyIndex++].category;

		packedAccumulatedPlayTime.PackedData.m_Byte2 = data.trackHistory[historyIndex].index;
		m_CategoryStride[RADIO_TRACK_CAT_MUSIC] = ((s8)data.trackHistory[historyIndex++].category - 8);

		packedAccumulatedPlayTime.PackedData.m_Byte3 = data.trackHistory[historyIndex].index;
		m_CategoryStride[RADIO_TRACK_CAT_IDENTS] = ((s8)data.trackHistory[historyIndex++].category - 8);

		packedLastTakeoverTime.PackedData.m_Byte0 = data.trackHistory[historyIndex].index;
		m_CategoryStride[RADIO_TRACK_CAT_TAKEOVER_DJSOLO] = ((s8)data.trackHistory[historyIndex++].category - 8);

		packedLastTakeoverTime.PackedData.m_Byte1 = data.trackHistory[historyIndex].index;
		m_CategoryStride[RADIO_TRACK_CAT_TAKEOVER_IDENTS] = ((s8)data.trackHistory[historyIndex++].category - 8);

		packedLastTakeoverTime.PackedData.m_Byte2 = data.trackHistory[historyIndex].index;
		m_CategoryStride[RADIO_TRACK_CAT_TAKEOVER_MUSIC] = ((s8)data.trackHistory[historyIndex++].category - 8);

		packedLastTakeoverTime.PackedData.m_Byte3 = data.trackHistory[historyIndex].index;
		m_CategoryStride[RADIO_TRACK_CAT_DJSOLO] = ((s8)data.trackHistory[historyIndex++].category - 8);

		m_StationAccumulatedPlayTimeMs = packedAccumulatedPlayTime.m_Value;
		m_TakeoverLastTimeMs = packedLastTakeoverTime.m_Value;

		audDisplayf("Accumulated Play Time: %u", m_StationAccumulatedPlayTimeMs);
		audDisplayf("Last Takeover Time: %u", m_TakeoverLastTimeMs);
		audDisplayf("Takeover Music Track Count: %u", m_TakeoverMusicTrackCounter);
		audDisplayf("Music Run Count: %u", m_MusicRunningCount);

		// The rest of the history stores our actual per-category data
		for (; historyIndex < data.trackHistory.GetMaxCount(); historyIndex++)
		{
			const u32 unpackedCategory = 0xFFFF & UnpackCategoryFromNetwork(data.trackHistory[historyIndex].category);

			if(unpackedCategory == 0xffff)
			{
				continue;
			}

			audRadioStationHistory* history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(unpackedCategory));

			if (history)
			{
				history->Reset();
				(*history)[0] = data.trackHistory[historyIndex].index != 0xff ? data.trackHistory[historyIndex].index : ~0u;
				history->SetWriteIndex(1);

#if __BANK
				u32 soundHash = g_NullSoundHash;
				const audRadioStationTrackListCollection* trackList = unpackedCategory < NUM_RADIO_TRACK_CATS ? FindTrackListsForCategory(unpackedCategory) : NULL;

				if (trackList && data.trackHistory[historyIndex].index < trackList->ComputeNumTracks())
				{
					soundHash = trackList->GetTrack(data.trackHistory[historyIndex].index)->SoundRef;
				}

				audDisplayf("History %d: %s (%s track %u)", historyIndex, SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash), unpackedCategory >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)unpackedCategory), data.trackHistory[historyIndex].index);
#endif
			}
		}
	}
	else
	{
		// Also reset history	
		audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
		if (history)
		{
			history->Reset();

			// We only receive one history record (representing the currently playing track) for sequential music stations
			for (s32 i = 0; i < (m_PlaySequentialMusicTracks ? 1 : data.trackHistory.GetMaxCount()); i++)
			{
				// category in low 16bits, index in high 16 bits
				const u32 category = 0xFFFF & UnpackCategoryFromNetwork(data.trackHistory[i].category);
				const u32 trackIndex = (data.trackHistory[i].index << 16);
				(*history)[i] = (category) | trackIndex;

#if __BANK
				u32 soundHash = g_NullSoundHash;
				const u32 unpackedCategory = UnpackCategoryFromNetwork(data.trackHistory[i].category);
				const audRadioStationTrackListCollection* trackList = unpackedCategory < NUM_RADIO_TRACK_CATS ? FindTrackListsForCategory(unpackedCategory) : NULL;

				if (trackList && data.trackHistory[i].index < trackList->ComputeNumTracks())
				{
					soundHash = trackList->GetTrack(data.trackHistory[i].index)->SoundRef;
				}

				audDisplayf("History %d: %s (%s track %u)", i, SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash), unpackedCategory >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)unpackedCategory), data.trackHistory[i].index);
#endif
			}
		}

		history->SetWriteIndex(1);

		// set up the current track
		category = (*history)[0] & 0xffff;
		index = (*history)[0] >> 16;
	}

	if (GetNameHash() == ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_RADIO", 0xF8BEAA16))
	{
		const audRadioStationTrackListCollection* trackList = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);

		if (trackList)
		{
			u32 maxTrackIndex = Max(index, (u32)data.nextTrack.index);

			u32 numTracks = trackList->ComputeNumTracks();

			// Its possible that we get a sync request before the tuneables are loaded to unlock the station, so we need to unlock the appropriate tracks if we currently have them locked
			if (maxTrackIndex >= numTracks)
			{
				audDisplayf("Syncing RADIO_22_DLC_BATTLE_MIX1_RADIO, track list %u is beyond the number of available tracks (%u), unlocking BATTLE_MIX2_RADIO", maxTrackIndex, numTracks);
				UnlockTrackList("BATTLE_MIX2_RADIO");

				numTracks = trackList->ComputeNumTracks();

				if (maxTrackIndex >= numTracks)
				{
					audDisplayf("Syncing RADIO_22_DLC_BATTLE_MIX1_RADIO, track list %u is beyond the number of available tracks (%u), unlocking BATTLE_MIX3_RADIO", maxTrackIndex, numTracks);
					UnlockTrackList("BATTLE_MIX3_RADIO");

					numTracks = trackList->ComputeNumTracks();

					if (maxTrackIndex >= numTracks)
					{
						audDisplayf("Syncing RADIO_22_DLC_BATTLE_MIX1_RADIO, track list %u is beyond the number of available tracks (%u), unlocking BATTLE_MIX4_RADIO", maxTrackIndex, numTracks);
						UnlockTrackList("BATTLE_MIX4_RADIO");
					}
				}
			}
		}		

		sm_HasSyncedUnlockableDJStation = true;
	}

	// Sequential music stations should just store track index directly
	if(m_PlaySequentialMusicTracks)
	{
		(*(const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC))))[0] = index;
	}

	if(category < NUM_RADIO_TRACK_CATS)
	{
		const RadioStationTrackList::tTrack *trackMetadata = GetTrack(category, kNullContext, index);
		if(trackMetadata)
		{
			s32 desiredTracKPlaytimeMS = (s32)(data.currentTrackPlaytime * m_Tracks[m_ActiveTrackIndex].GetDuration());

			// If we're already playing the desired track and we're within a certain error range of the desired playback point,
			// then just keep it playing rather than shutting down and restarting (as this will cause an audible drop out)
			if(m_Tracks[m_ActiveTrackIndex].GetSoundRef() == trackMetadata->SoundRef && Abs(m_Tracks[m_ActiveTrackIndex].GetPlayTime() - desiredTracKPlaytimeMS) < 5000)
			{				
				u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
				m_Tracks[nextTrackIndex].Shutdown();
				audDisplayf("Station %s is already synced to the active track - ignoring sync data", GetName());
			}
			else
			{
				m_ActiveTrackIndex = 0;
				m_Tracks[0].Shutdown();
				m_Tracks[1].Shutdown();
				m_Tracks[1].Init(category, index, trackMetadata->SoundRef, data.currentTrackPlaytime, m_IsMixStation && !IsUSBMixStation(), m_IsReverbStation, m_NameHash);
				audAssertf(m_Tracks[1].GetDuration() > 0, "Invalid track/sound metadata: %s (%s : %u)", GetName(), GetTrackCategoryName(category), index);
			}
		}
		else
		{
			audWarningf("%s - Failed to sync current network radio track %u / %u", GetName(), category, index);
		}
	}
	else
	{
		audWarningf("%s - Failed to sync current network radio track; invalid category %u / %u", GetName(), category, index);
	}

	PrepareNextTrack();
	
	m_NetworkNextTrack = data.nextTrack;
	m_NetworkNextTrack.category = static_cast<u8>(UnpackCategoryFromNetwork(data.nextTrack.category));

#if __BANK
	u32 soundHash = g_NullSoundHash;
	const audRadioStationTrackListCollection* trackList = UnpackCategoryFromNetwork(data.nextTrack.category) < NUM_RADIO_TRACK_CATS ? FindTrackListsForCategory(m_NetworkNextTrack.category) : NULL;

	if(trackList && m_NetworkNextTrack.index < trackList->ComputeNumTracks())
	{
		soundHash = trackList->GetTrack(m_NetworkNextTrack.index)->SoundRef;
	}

	audDisplayf("Next Track: %s (%s track %u)", SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash), category >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)m_NetworkNextTrack.category), m_NetworkNextTrack.index);
#endif

	audDisplayf("Random Seed: %u", data.randomSeed);
	audDisplayf("Current Track Playtime: %f", data.currentTrackPlaytime);

	if(m_NetworkNextTrack.category >= NUM_RADIO_TRACK_CATS)
	{
		// remote host hadn't picked a track when we grabbed the state
		m_NetworkNextTrack.category = 0xff;
	}
	else if(!IsUSBMixStation())
	{
		ASSERT_ONLY(const RadioStationTrackList::tTrack *nextTrackPtr = GetTrack(m_NetworkNextTrack.category, kNullContext, m_NetworkNextTrack.index));
		audAssertf(nextTrackPtr, "%s - Failed to find next track (%u / %s, %u)", GetName(), m_NetworkNextTrack.category, TrackCats_ToString((TrackCats)m_NetworkNextTrack.category), m_NetworkNextTrack.index);
	}
	
	// TODO: track valid selection times?

	// TODO: deal with sequential music stations if we have any
}

void audRadioStation::PopulateSyncData(audRadioStationSyncData &syncData) const
{
	audDisplayf("Populating radio station sync data for station %s (Local Player: %s, Host: %s, Synced Time: %u):", GetName(), NetworkInterface::GetLocalGamerName(), NetworkInterface::GetHostName(), NetworkInterface::GetSyncedTimeInMilliseconds());

	syncData.nameHash = GetNameHash();
	
	if(m_PlaySequentialMusicTracks)
	{		
		// we want the sync history index 0 to be the currently playing track		

		// Sequential stations only ever use history slot 0, and it'll actually contain the index of the *next* track due 
		// to play at this point, which is of no use - instead, just query the track index directly from the track object itself
		syncData.trackHistory[0].category = static_cast<u8>(PackCategoryForNetwork(m_Tracks[m_ActiveTrackIndex].GetCategory()));		
		syncData.trackHistory[0].index = static_cast<u8>(m_Tracks[m_ActiveTrackIndex].GetTrackIndex());

#if __BANK
		u32 soundHash = g_NullSoundHash;
		const audRadioStationTrackListCollection* trackList = m_Tracks[m_ActiveTrackIndex].GetCategory() < NUM_RADIO_TRACK_CATS ? FindTrackListsForCategory(m_Tracks[m_ActiveTrackIndex].GetCategory()) : NULL;

		if(trackList && m_Tracks[m_ActiveTrackIndex].GetTrackIndex() < trackList->ComputeNumTracks())
		{
			soundHash = trackList->GetTrack(m_Tracks[m_ActiveTrackIndex].GetTrackIndex())->SoundRef;
		}

		audDisplayf("History %d: %s (%s track %u)", 0, SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash), m_Tracks[m_ActiveTrackIndex].GetCategory() >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)m_Tracks[m_ActiveTrackIndex].GetCategory()), m_Tracks[m_ActiveTrackIndex].GetTrackIndex());
#endif
		
		// Just populate the rest of the history with 'invalid'
		u32 trackId = 0xffffffff;
		const u32 category = static_cast<u32>(trackId & 0xffff);	
		trackId = trackId >> 16;

		for(s32 index = 1; index < audRadioStationHistory::kRadioHistoryLength; index++)
		{			
			syncData.trackHistory[index].category = static_cast<u8>(PackCategoryForNetwork(category));
			syncData.trackHistory[index].index = static_cast<u8>(trackId);
		}
	}
	else if (m_UseRandomizedStrideSelection)
	{
		u32 historyIndex = 0;
		syncData.trackHistory[historyIndex].category = static_cast<u8>(PackCategoryForNetwork(m_Tracks[m_ActiveTrackIndex].GetCategory()));
		syncData.trackHistory[historyIndex++].index = static_cast<u8>(m_Tracks[m_ActiveTrackIndex].GetTrackIndex());

		audRadioStationPackedInt packedAccumulatedPlayTime;
		audRadioStationPackedInt packedLastTakeoverTime;

		packedAccumulatedPlayTime.m_Value = m_StationAccumulatedPlayTimeMs;
		packedLastTakeoverTime.m_Value = m_TakeoverLastTimeMs;

		// Takeover station requires syncing some extra data, but we don't need to sync every category so we can re-purpose some history slots
		// We've got 8 bits per index, so pack our u32s across a set of history slots
		// We've got 4 bits per category, so store our u8s in these
		syncData.trackHistory[historyIndex].index = packedAccumulatedPlayTime.PackedData.m_Byte0;
		Assign(syncData.trackHistory[historyIndex++].category, m_MusicRunningCount);

		syncData.trackHistory[historyIndex].index = packedAccumulatedPlayTime.PackedData.m_Byte1;		
		syncData.trackHistory[historyIndex++].category = m_TakeoverMusicTrackCounter;
		
		syncData.trackHistory[historyIndex].index = packedAccumulatedPlayTime.PackedData.m_Byte2;
		syncData.trackHistory[historyIndex++].category = (u8)m_CategoryStride[RADIO_TRACK_CAT_MUSIC] + 8;

		syncData.trackHistory[historyIndex].index = packedAccumulatedPlayTime.PackedData.m_Byte3;
		syncData.trackHistory[historyIndex++].category = (u8)m_CategoryStride[RADIO_TRACK_CAT_IDENTS] + 8;

		syncData.trackHistory[historyIndex].index = packedLastTakeoverTime.PackedData.m_Byte0;
		syncData.trackHistory[historyIndex++].category = (u8)m_CategoryStride[RADIO_TRACK_CAT_TAKEOVER_DJSOLO] + 8;

		syncData.trackHistory[historyIndex].index = packedLastTakeoverTime.PackedData.m_Byte1;
		syncData.trackHistory[historyIndex++].category = (u8)m_CategoryStride[RADIO_TRACK_CAT_TAKEOVER_IDENTS] + 8;

		syncData.trackHistory[historyIndex].index = packedLastTakeoverTime.PackedData.m_Byte2;
		syncData.trackHistory[historyIndex++].category = (u8)m_CategoryStride[RADIO_TRACK_CAT_TAKEOVER_MUSIC] + 8;

		syncData.trackHistory[historyIndex].index = packedLastTakeoverTime.PackedData.m_Byte3;
		syncData.trackHistory[historyIndex++].category = (u8)m_CategoryStride[RADIO_TRACK_CAT_DJSOLO] + 8;

		audDisplayf("Accumulated Play Time: %u", m_StationAccumulatedPlayTimeMs);
		audDisplayf("Last Takeover Time Time: %u", m_TakeoverLastTimeMs);
		audDisplayf("Takeover Music Track Count: %u", m_TakeoverMusicTrackCounter);
		audDisplayf("Music Run Count: %u", m_MusicRunningCount);
		
		for (u32 category = 0; category < TRACKCATS_MAX; category++)
		{
			if (IsRandomizedStrideCategory(category))
			{
				audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(category));
				syncData.trackHistory[historyIndex].category = static_cast<u8>(PackCategoryForNetwork(category));
				syncData.trackHistory[historyIndex].index = static_cast<u8>(history ? history->GetPreviousEntry() : 0xff);
				historyIndex++;
			}
		}

		// Just populate the rest of the history with 'invalid'
		u32 trackId = 0xffffffff;
		const u32 category = static_cast<u32>(trackId & 0xffff);
		trackId = trackId >> 16;

		for (; historyIndex < audRadioStationHistory::kRadioHistoryLength; historyIndex++)
		{
			syncData.trackHistory[historyIndex].category = static_cast<u8>(PackCategoryForNetwork(category));
			syncData.trackHistory[historyIndex].index = static_cast<u8>(trackId);
		}
	}
	else
	{
		audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
		if(history)
		{
			for(s32 index = 0; index < audRadioStationHistory::kRadioHistoryLength; index++)
			{
				// we want the sync history index 0 to be the currently playing track, ie the previous write index
				const u32 i = (index + history->GetWriteIndex() + audRadioStationHistory::kRadioHistoryLength - 1) % audRadioStationHistory::kRadioHistoryLength;
				u32 trackId = (*history)[i];

				const u32 category = static_cast<u32>(trackId & 0xffff);	
				trackId = trackId >> 16;
						
				syncData.trackHistory[index].category = static_cast<u8>(PackCategoryForNetwork(category));
				syncData.trackHistory[index].index = static_cast<u8>(trackId);

#if __BANK				
				u32 soundHash = g_NullSoundHash;
				const audRadioStationTrackListCollection* trackList = category < NUM_RADIO_TRACK_CATS? FindTrackListsForCategory(category) : NULL;

				if(trackList && trackId < trackList->ComputeNumTracks())
				{
					soundHash = trackList->GetTrack(trackId)->SoundRef;
				}

				audDisplayf("History %d: %s (%s track %u)", index, SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash), category >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)category), trackId);
#endif				

				// we will generate sync asserts if category or index are invalid
				// if these errors are generated, we may need to increase the number of bits
				if(static_cast<unsigned>(syncData.trackHistory[index].category) > audRadioStationSyncData::TrackId::CATEGORY_MASK)
				{
					audErrorf("%d has invalid category value of %u. Clamping to %u", index, syncData.trackHistory[index].category, audRadioStationSyncData::TrackId::CATEGORY_MASK);
					syncData.trackHistory[index].category = audRadioStationSyncData::TrackId::CATEGORY_MASK;
				}

				if(static_cast<unsigned>(syncData.trackHistory[index].index) > audRadioStationSyncData::TrackId::INDEX_MASK)
				{
					audErrorf("%d has invalid index value of %u. Clamping to %u", index, syncData.trackHistory[index].index, audRadioStationSyncData::TrackId::INDEX_MASK);
					syncData.trackHistory[index].index = audRadioStationSyncData::TrackId::INDEX_MASK;
				}
			}
		}
		else
		{
			audDisplayf("No history");
		}
	}

	u32 nextTrackIndex = (m_ActiveTrackIndex+1)&1;
	
	syncData.nextTrack.category = static_cast<u8>(PackCategoryForNetwork(m_Tracks[nextTrackIndex].GetCategory()));
	syncData.nextTrack.index = static_cast<u8>(m_Tracks[nextTrackIndex].GetTrackIndex());

#if __BANK
	u32 soundHash = g_NullSoundHash;
	const audRadioStationTrackListCollection* trackList = m_Tracks[nextTrackIndex].GetCategory() < NUM_RADIO_TRACK_CATS ? FindTrackListsForCategory(m_Tracks[nextTrackIndex].GetCategory()) : NULL;

	if(trackList && m_Tracks[nextTrackIndex].GetTrackIndex() < trackList->ComputeNumTracks())
	{
		soundHash = trackList->GetTrack(m_Tracks[nextTrackIndex].GetTrackIndex())->SoundRef;
	}
	
	audDisplayf("Next Track: %s (%s track %u)", SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash), m_Tracks[nextTrackIndex].GetCategory() >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)m_Tracks[nextTrackIndex].GetCategory()), m_Tracks[nextTrackIndex].GetTrackIndex());
#endif

	syncData.randomSeed = m_Random.GetSeed();

	audDisplayf("Random Seed: %u", syncData.randomSeed);

	const u32 trackDuration = m_Tracks[m_ActiveTrackIndex].GetDuration();
	syncData.currentTrackPlaytime = trackDuration > 0 ? Clamp(m_Tracks[m_ActiveTrackIndex].GetPlayTime() / (float)(trackDuration), 0.f, 1.f) : 0.f;

	audDisplayf("Current Track Playtime: %f", syncData.currentTrackPlaytime);
}

void audRadioStation::Reset(void)
{
	StopRetuneSounds();

	if(sm_DjSpeechMonoSound)
	{
		sm_DjSpeechMonoSound->StopAndForget();
		sm_DjSpeechMonoSound = NULL;
	}
	if(sm_DjSpeechPositionedSound)
	{
		sm_DjSpeechPositionedSound->StopAndForget();
		sm_DjSpeechPositionedSound = NULL;
	}

	for(u32 i = 0; i < sm_NumRadioStations; i++)
	{
		GetStation(i)->Shutdown();
	}
}

audRadioStation *audRadioStation::FindStation(const char *name)
{
	return FindStation(atStringHash(name));
}

audRadioStation *audRadioStation::FindStation(u32 nameHash)
{
	s32 stationIndex = FindIndexOfStationWithHash(nameHash);
	if (stationIndex >= 0)
	{
		return sm_OrderedRadioStations[stationIndex];
	}

	return NULL;
}

u32 audRadioStation::GetStationIndex() const
{
	for(s32 stationIndex=0; stationIndex<sm_NumRadioStations; stationIndex++)
	{
		if(sm_OrderedRadioStations[stationIndex] == this)
		{
			return stationIndex;
		}
	}
	return g_OffRadioStation;
}

s32 audRadioStation::GetStationImmutableIndex(audRadioStationImmutableIndexType indexType) const
{
	return m_StationImmutableIndex[indexType];
}

s32 audRadioStation::FindIndexOfStationWithHash(u32 nameHash)
{
	for(s32 stationIndex=0; stationIndex<sm_NumRadioStations; stationIndex++)
	{
		if(sm_OrderedRadioStations[stationIndex] && (sm_OrderedRadioStations[stationIndex]->GetNameHash() == nameHash))
		{
			return stationIndex;
		}
	}

	return -1;
}

audRadioStation *audRadioStation::GetStation(u32 index)
{
	return ((index < sm_NumRadioStations) ? sm_OrderedRadioStations[index] : NULL);
}

audRadioStation *audRadioStation::FindStationWithImmutableIndex(u32 index, audRadioStationImmutableIndexType indexType)
{
	for(u32 stationIndex = 0; stationIndex < sm_NumRadioStations; stationIndex++)
	{
		audRadioStation* radioStation = sm_OrderedRadioStations[stationIndex];

		if(radioStation && radioStation->m_StationImmutableIndex[indexType] == (s32)index)
		{
			return radioStation;
		}
	}

	return nullptr;
}

void audRadioStation::StartEndCredits()
{
	sm_PlayingEndCredits = true;
}

void audRadioStation::StopEndCredits()
{
	sm_PlayingEndCredits = false;
}

void audRadioStation::SetHidden(bool isHidden)
{ 
	AUD_CLEAR_TRISTATE_VALUE(const_cast<RadioStationSettings*>(m_StationSettings)->Flags, FLAG_ID_RADIOSTATIONSETTINGS_HIDDEN); 
	AUD_SET_TRISTATE_VALUE(const_cast<RadioStationSettings*>(m_StationSettings)->Flags, FLAG_ID_RADIOSTATIONSETTINGS_HIDDEN, isHidden ? AUD_TRISTATE_TRUE : AUD_TRISTATE_FALSE); 
}

bool audRadioStation::IsFavourited() const
{ 
	return m_IsFavourited || !CNetwork::IsGameInProgress();
}	

void audRadioStation::SetLocked(const bool isLocked)
{
	if(!isLocked && AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_LOCKED) == AUD_TRISTATE_TRUE)
	{
		audDisplayf("Unlocking radio station %s", GetName());
		const_cast<RadioStationSettings*>(m_StationSettings)->Flags &= ~(0x03 << (FLAG_ID_RADIOSTATIONSETTINGS_LOCKED * 2));
		AUD_SET_TRISTATE_VALUE(const_cast<RadioStationSettings*>(m_StationSettings)->Flags, FLAG_ID_RADIOSTATIONSETTINGS_LOCKED, AUD_TRISTATE_FALSE);

		FlagLockedStationChange();
	}
	else if(isLocked && AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_LOCKED) != AUD_TRISTATE_TRUE)
	{
		audDisplayf("Locking radio station %s", GetName());
		const_cast<RadioStationSettings*>(m_StationSettings)->Flags &= ~(0x03 << (FLAG_ID_RADIOSTATIONSETTINGS_LOCKED * 2));
		AUD_SET_TRISTATE_VALUE(const_cast<RadioStationSettings*>(m_StationSettings)->Flags, FLAG_ID_RADIOSTATIONSETTINGS_LOCKED, AUD_TRISTATE_TRUE);

		FlagLockedStationChange();
	}	
}

void audRadioStation::SetFavourited(bool isFavourited)
{
	if(isFavourited != m_IsFavourited)
	{
		// Any station that is able to be be set as a favorite station should be entered into the FIXED_ORDER_RADIO_STATION_HASH_LIST_TUNEABLE
		// list, which is used as the index of a bitset by telemetry code to track favorite-ed stations. NB. if adding a station to the above
		// list, it must be done at the end to ensure existing station indices are not altered.
		audAssertf(!audNorthAudioEngine::GetObject<GameObjectHashList>(ATSTRINGHASH("FIXED_ORDER_RADIO_STATION_HASH_LIST_TUNEABLE", 0x616CE649)) || GetStationImmutableIndex(IMMUTABLE_INDEX_TUNEABLE) >= 0, "Non-tuneable station %s has no immutable index set up", GetName());
		m_IsFavourited = isFavourited;
		FlagLockedStationChange();
	}
}

void audRadioStation::GetFavouritedStations(u32 &favourites1, u32 &favourites2)
{
	favourites1 = 0;
	favourites2 = 0;
	for(u32 stationIndex = 0; stationIndex < sm_NumRadioStations; stationIndex++)
	{
		audRadioStation* radioStation = sm_OrderedRadioStations[stationIndex];
		if(radioStation && radioStation->m_IsFavourited)
		{
			s32 immutableIndex = radioStation->m_StationImmutableIndex[IMMUTABLE_INDEX_TUNEABLE];
			if(immutableIndex < 0) continue;

			if(audVerifyf(immutableIndex < 64, "IMMUTABLE_INDEX_TUNEABLE index exceeds bitset size"))
			{
				if(immutableIndex < 32)
				{
					favourites1 |= (u32)(1u << immutableIndex);
				}
				else
				{
					favourites2 |= (u32)(1u << (immutableIndex - 32));
				}
			}
		}
	}
}

void audRadioStation::GameUpdate()
{
    const Vec3V s_CountryTalkRadioEmitter(752.0f, 4900.f, 0.f);
    const float s_CountryTalkRadioRadius = 4000.f;
    const float s_CountryTalkRadioHysterisis = 65.f;
    audRadioStation *talkStation1 = FindStation(ATSTRINGHASH("RADIO_05_TALK_01", 0x406F56EC));
    audRadioStation *talkStation2 = FindStation(ATSTRINGHASH("RADIO_11_TALK_02", 0xE6598737));
    bool isSpectating = false;

    if(talkStation1 && talkStation2)
    {
        CPed *player = CGameWorld::FindLocalPlayer();

        if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnSCTVTeam())
        {
            CPed* followplayer = CGameWorld::FindFollowPlayer();
            if (followplayer)
            {
                player = followplayer;
                isSpectating = true;
            }
        }

        if(player)
        {
            const Vec3V playerPosition = player->GetTransform().GetPosition();
            const float distToEmitter = Dist(playerPosition, s_CountryTalkRadioEmitter).Getf();
            if(distToEmitter < s_CountryTalkRadioRadius)
            {
                if(distToEmitter < s_CountryTalkRadioRadius - s_CountryTalkRadioHysterisis)
                {
                    if(talkStation2->IsLocked())
                    {
                        talkStation2->SetLocked(false);
                        audDisplayf("Unlocking %s due to player position (Player:%.02f, %.02f, %.02f) - distance %.02f. Spectating: %s", talkStation2->GetName(), playerPosition.GetXf(), playerPosition.GetYf(), playerPosition.GetZf(), distToEmitter, isSpectating? "true" : "false");
                    }
                    if(!talkStation1->IsLocked())
                    {
                        talkStation1->SetLocked(true);
                        audDisplayf("Locking %s due to player position (Player:%.02f, %.02f, %.02f) - distance %.02f. Spectating: %s", talkStation1->GetName(), playerPosition.GetXf(), playerPosition.GetYf(), playerPosition.GetZf(), distToEmitter, isSpectating? "true" : "false");
                    }
                    sm_CountryTalkRadioSignal = 1.f;
                }
                else
                {
                    sm_CountryTalkRadioSignal = (s_CountryTalkRadioRadius - distToEmitter) / s_CountryTalkRadioHysterisis;
                }
            }
            else if(distToEmitter > s_CountryTalkRadioRadius)
            {
                if(!talkStation2->IsLocked())
                {
                    talkStation2->SetLocked(true);
                    audDisplayf("Locking %s due to player position (Player:%.02f, %.02f, %.02f) - distance %.02f. Spectating: %s", talkStation2->GetName(), playerPosition.GetXf(), playerPosition.GetYf(), playerPosition.GetZf(), distToEmitter, isSpectating? "true" : "false");
                    sm_CountryTalkRadioSignal = 0.f;	
                }
                if(talkStation1->IsLocked())
                {
                    talkStation1->SetLocked(false);
                    audDisplayf("Unlocking %s due to player position (Player:%.02f, %.02f, %.02f) - distance %.02f. Spectating: %s", talkStation1->GetName(), playerPosition.GetXf(), playerPosition.GetYf(), playerPosition.GetZf(), distToEmitter, isSpectating? "true" : "false");
                }
            }
        }

		f32 signalLevel = 1.f;

		if(!g_AudioEngine.GetSoundManager().IsPaused(2) && !g_RadioAudioEntity.IsRetuningVehicleRadio() && !g_RadioAudioEntity.IsRadioFadedOut() && g_RadioAudioEntity.IsPlayerRadioActive())
		{
			if(g_RadioAudioEntity.GetPlayerRadioStationPendingRetune() == talkStation2)
			{
				signalLevel = sm_CountryTalkRadioSignal;
			}
			else if(g_RadioAudioEntity.GetPlayerRadioStationPendingRetune() == talkStation1)
			{
				signalLevel = 1.f - sm_CountryTalkRadioSignal;
			}

			signalLevel = Min(signalLevel, sm_ScriptGlobalRadioSignalLevel);
		}

		g_AudioEngine.GetEnvironment().GetMusicSubmix()->SetEffectParam(0, ATSTRINGHASH("signalLevel", 0xFAF72E9), signalLevel);
		g_AudioEngine.GetEnvironment().GetMusicSubmix()->SetEffectParam(0, ATSTRINGHASH("Bypass", 0x10F3D95C), signalLevel < 1.f? false : true);
    }
}

void audRadioStation::UpdateStations(u32 timeInMs)
{	
#if RSG_PC
#if __BANK
	if(g_RadioAudioEntity.sm_DebugUserMusic)
		DebugDrawUserMusic();
#endif
	audRadioStation *userStation = FindStation(ATSTRINGHASH("RADIO_19_USER", 0xF74D5272));
	if(userStation)
	{
		userStation->SetLocked(!HasUserTracks() || PARAM_disableuserradio.Get());
	}
	sm_UserRadioTrackManager.UpdateScanningUI();
#endif // _WIN32PC

	u32 numUnlocked = 0;
	u32 numUnlockedFavourited = 0;

	for(u32 index=0; index<sm_NumRadioStations; index++)
	{
		audRadioStation *station = GetStation(index);
		if(station)
		{
			station->Update(timeInMs);
			if(!station->IsLocked() && !station->IsHidden())
			{
				numUnlocked++;

				if(station->IsFavourited())
				{
					numUnlockedFavourited++;
				}
			}
		}
	}

	UpdateDjSpeech(timeInMs);

	if(numUnlocked != sm_NumUnlockedRadioStations || numUnlockedFavourited != sm_NumUnlockedFavouritedStations || sm_UpdateLockedStations)
	{
		// Re-sort the list to ensure unlocked stations are first in the list.
		audDisplayf("Number of unlocked radio stations changed; was %u (%u favourited) now %u (%u favourited)", sm_NumUnlockedRadioStations, sm_NumUnlockedFavouritedStations, numUnlocked, numUnlockedFavourited);

		sm_OrderedRadioStations.QSort(0, sm_NumRadioStations, QSortRadioStationCompareFunc);
		sm_NumUnlockedRadioStations = numUnlocked;
		sm_NumUnlockedFavouritedStations = numUnlockedFavourited;
		sm_UpdateLockedStations = false;
	}
}

void audRadioStation::UpdateRetuneSounds(void)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		StopRetuneSounds();
		return;
	}
#endif	//GTA_REPLAY

	//Ensure the retune sounds are active.
	audSoundInitParams initParams;
	initParams.TimerId = 2;
	if(sm_RetuneMonoSound == NULL)
	{
		g_RadioAudioEntity.CreateAndPlaySound_Persistent(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("RetuneFrontend", 0xC46D20A)),
															&sm_RetuneMonoSound, &initParams);
	}
	if(sm_RetunePositionedSound == NULL)
	{
		g_RadioAudioEntity.CreateAndPlaySound_Persistent(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("RetunePositioned", 0x1B5588EC)), 
															&sm_RetunePositionedSound, &initParams);
	}

	f32 playerVehicleInsideFactor = g_RadioAudioEntity.GetPlayerVehicleInsideFactor();

	if(sm_RetuneMonoSound)
	{
		f32 volumeDbMono = 0.f;
		// use full volume retune loop when in frontend/lobby radio
		if(!g_RadioAudioEntity.IsMobilePhoneRadioActive())
		{
			f32 volumeLinearMono = Max<f32>(sm_VehicleRadioRiseVolumeCurve.CalculateValue(playerVehicleInsideFactor),
			g_SilenceVolumeLin);
			volumeDbMono = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinearMono);
			volumeDbMono += g_RadioAudioEntity.GetVolumeOffset();
		}
		sm_RetuneMonoSound->SetRequestedVolume(volumeDbMono);
	}

	if(sm_RetunePositionedSound)
	{
		f32 volumeLinearPos = Max<f32>(sm_VehicleRadioRiseVolumeCurve.CalculateValue(1.0f - playerVehicleInsideFactor),
			g_SilenceVolumeLin);
		f32 volumeDbPos = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinearPos);
		volumeDbPos += g_RadioAudioEntity.GetVolumeOffset();
		sm_RetunePositionedSound->SetRequestedVolume(volumeDbPos);

		CVehicle *lastPlayerVehicle = g_RadioAudioEntity.GetLastPlayerVehicle();
		if(lastPlayerVehicle != NULL)
		{
			//Attach positioned retune sound to the current or last player vehicle.
			Vector3 vehiclePosition = VEC3V_TO_VECTOR3(lastPlayerVehicle->GetTransform().GetPosition());
			sm_RetunePositionedSound->SetRequestedPosition(vehiclePosition);
		}
	}
}

void audRadioStation::StopRetuneSounds(void)
{
	if(sm_RetuneMonoSound)
	{
		sm_RetuneMonoSound->StopAndForget();
		sm_RetuneMonoSound = NULL;
	}
	if(sm_RetunePositionedSound)
	{
		sm_RetunePositionedSound->StopAndForget();
		sm_RetunePositionedSound = NULL;
	}
}

void audRadioStation::SkipForwardStations(u32 timeToSkipMs)
{
	for(u32 index=0; index<sm_NumRadioStations; index++)
	{
		audRadioStation *station = GetStation(index);
		if(station && station->IsStreamingVirtually())
		{
			station->SkipForward(timeToSkipMs);
		}
	}
}

void audRadioStation::SkipForward(u32 timeToSkip)
{
	if(IsFrozen())
	{
		audWarningf("%s ignoring skip request (%ums) due to being frozen", GetName(), timeToSkip);
	}
	else
	{
		audDisplayf("Radio station %s is skipping forward by %ums (current track: %u)", GetName(), timeToSkip, m_Tracks[m_ActiveTrackIndex].GetSoundRef());

		u32 timeLeft = m_Tracks[m_ActiveTrackIndex].GetDuration() - (u32)m_Tracks[m_ActiveTrackIndex].GetPlayTime();
		if(timeToSkip > timeLeft && !PARAM_disableradiohistory.Get())
		{
			// skip a random amount into the next track too
			u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
			if(m_Tracks[nextTrackIndex].IsInitialised())
			{
				u32 nextDuration = m_Tracks[nextTrackIndex].GetDuration();
				u32 min = Min(10000U, nextDuration);
				u32 max = Min(nextDuration, nextDuration - 20000U);
				m_Tracks[nextTrackIndex].SkipForward((u32)GetRandomNumberInRange((s32)min, (s32)max));
			}
		}
		m_Tracks[m_ActiveTrackIndex].SkipForward(timeToSkip);
	}
}

#if RSG_PC	// user music
void audRadioStation::NextTrack()
{
	if(m_IsUserStation && !sm_IsInRecoveryMode && sm_UserRadioTrackManager.GetRadioMode() != USERRADIO_PLAY_MODE_RADIO)
	{
		if(m_Tracks[m_ActiveTrackIndex].IsInitialised() && m_Tracks[m_ActiveTrackIndex].GetPlayTime() > 2000)
		{
			sm_ForceUserNextTrack = true;
		}
	}
}

void audRadioStation::PrevTrack()
{	
	if(m_Tracks[m_ActiveTrackIndex].IsInitialised() && m_Tracks[m_ActiveTrackIndex].GetPlayTime() > 2000 && sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_SEQUENTIAL)
	{
		sm_ForceUserPrevTrack = true;
	}
}
#endif


void audRadioStation::UpdateDjSpeech(u32 timeInMs)
{
	const audRadioStation *playerStation = g_RadioAudioEntity.GetPlayerRadioStation();
	if(sm_IMDrivenDjSpeech)
	{
		playerStation = sm_IMDjSpeechStation;
	}

	if(playerStation && (sm_IMDrivenDjSpeech || (playerStation->IsStreamingPhysically() && !g_RadioAudioEntity.IsRetuning() && !IsPlayingEndCredits() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))))
	{
		//Update DJ speech for the player's radio station.
		s32 trackPlayTimeMs = sm_IMDrivenDjSpeech ? g_InteractiveMusicManager.GetCurPlayTimeMs() :
														playerStation->GetCurrentPlayTimeOfActiveTrack();

		bool isPlayingSpeech = false;
		bool isDummySpeech = false;

		u32 clientVar = 0;

		if(sm_DjSpeechMonoSound)
		{
			sm_DjSpeechMonoSound->GetClientVariable(clientVar);
			isDummySpeech |= (clientVar == 0xdeadbeef);
		}

		if(sm_DjSpeechPositionedSound)
		{
			sm_DjSpeechPositionedSound->GetClientVariable(clientVar);
			isDummySpeech |= (clientVar == 0xdeadbeef);
		}
		
		f32 playerVehicleInsideFactor;
		switch(sm_DjSpeechState)
		{
			case RADIO_DJ_SPEECH_STATE_PREPARING:
				if(sm_DjSpeechMonoSound && (sm_IMDrivenDjSpeech || trackPlayTimeMs <= (sm_ActiveDjSpeechStartTimeMs + g_RadioDjPlayTimeTresholdMs)))
				{
					bool isPrepared = false;
					switch(sm_DjSpeechMonoSound->Prepare(g_RadioDjSpeechWaveSlot, true))
					{
						case AUD_PREPARE_FAILED:
							Warningf("Failed to prepare DJ speech for station %s", playerStation->GetName());

							if(sm_DjSpeechMonoSound)
							{
								sm_DjSpeechMonoSound->StopAndForget();
							}

							if(sm_DjSpeechPositionedSound)
							{
								sm_DjSpeechPositionedSound->StopAndForget();
							}

							sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
							break;

						case AUD_PREPARED:
							isPrepared = true;
							break;

						case AUD_PREPARING:
						default:
							break;
					}

					if(isPrepared)
					{
						//Ensure that the positioned speech sound (with the same wave reference) is also prepared.
						switch(sm_DjSpeechPositionedSound->Prepare(g_RadioDjSpeechWaveSlot, true))
						{
							case AUD_PREPARE_FAILED:
								naWarningf("Failed to prepare DJ speech for station %s", playerStation->GetName());

								if(sm_DjSpeechMonoSound)
								{
									sm_DjSpeechMonoSound->StopAndForget();
								}
								
								if(sm_DjSpeechPositionedSound)
								{
									sm_DjSpeechPositionedSound->StopAndForget();
								}
								
								sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
								break;

							case AUD_PREPARED:
								sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_PREPARED;
								break;

							case AUD_PREPARING:
							default:
								break;
						}
					}
				}
				else
				{
					//We are out of time to prepare and play our DJ speech, so stop trying and cleanup.
					naDisplayf("Failed to prepare Dj speech in time");
					if(sm_DjSpeechMonoSound)
						sm_DjSpeechMonoSound->StopAndForget();
					if(sm_DjSpeechPositionedSound)
						sm_DjSpeechPositionedSound->StopAndForget();
					sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
				}

				break;

			case RADIO_DJ_SPEECH_STATE_PREPARED:
				if(!sm_IMDrivenDjSpeech || sm_IMDrivenDjSpeechScheduled)
				{
					if(trackPlayTimeMs >= (sm_ActiveDjSpeechStartTimeMs - g_RadioDjPlayTimeTresholdMs))
					{
						bool isDJSpeechRetriggerTimeValid = true;

						if(playerStation->GetNameHash() == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC) && !isDummySpeech)
						{
							if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2) - playerStation->m_LastTimeDJSpeechPlaying < g_RadioDjSpeechMinDelayMsMotomami)
							{
								isDJSpeechRetriggerTimeValid = false;
							}
						} 

						const u32 nextTrackCategory = playerStation->GetNextTrack().GetCategory();
						const u32 currentTrackCategory = playerStation->GetCurrentTrack().GetCategory();
						if(isDJSpeechRetriggerTimeValid && trackPlayTimeMs <= (sm_ActiveDjSpeechStartTimeMs + g_RadioDjPlayTimeTresholdMs) && (sm_IMDrivenDjSpeech || (sm_DjSpeechStation == playerStation && sm_DjSpeechNextTrackCategory == nextTrackCategory && (currentTrackCategory == RADIO_TRACK_CAT_MUSIC || currentTrackCategory == RADIO_TRACK_CAT_TAKEOVER_MUSIC))))
						{
							if(sm_DjSpeechMonoSound)
							{
								sm_DjSpeechMonoSound->Play();
							}

							if(sm_DjSpeechPositionedSound)
							{
								sm_DjSpeechPositionedSound->Play();
							}

							const_cast<audRadioStation*>(playerStation)->AddActiveDjSpeechToHistory(timeInMs);
							sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_PLAYING;
							sm_DjSpeechStation = NULL;
						}
						else
						{
							//We are out of time to play our DJ speech, so cleanup.
							if(!sm_IMDrivenDjSpeech && sm_DjSpeechStation != playerStation)
							{
								audWarningf("Cancelled DJ speech due to station change; scheduled on %s now %s", sm_DjSpeechStation ? sm_DjSpeechStation->GetName() : "null", playerStation ? playerStation->GetName() : "null");
							}
							else if(!sm_IMDrivenDjSpeech && sm_DjSpeechNextTrackCategory != nextTrackCategory)
							{
								audWarningf("Cancelled DJ speech due to next track category change; scheduled for %s now %s", TrackCats_ToString((TrackCats)sm_DjSpeechNextTrackCategory), TrackCats_ToString((TrackCats)nextTrackCategory));
							}
							else if(!isDJSpeechRetriggerTimeValid)
							{
								audWarningf("Cancelled DJ speech due to triggering within min retrigger time");
							}
							else
							{
								audWarningf("Failed to play Dj speech in time");
							}
							
							if(sm_DjSpeechMonoSound)
							{
								sm_DjSpeechMonoSound->StopAndForget();
							}
							if(sm_DjSpeechPositionedSound)
							{
								sm_DjSpeechPositionedSound->StopAndForget();
							}
							sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
							break;					
						}
					}
				}
				// intentional fall through if the speech is now playing so it doesn't go a frame without valid volume/filter

			case RADIO_DJ_SPEECH_STATE_PLAYING:
				//Update volumes (and position) of DJ speech sounds.
				playerVehicleInsideFactor = g_RadioAudioEntity.GetPlayerVehicleInsideFactor();
				if(sm_DjSpeechMonoSound)
				{
					isPlayingSpeech = true;
					sm_DjSpeechMonoSound->SetRequestedVolume(sm_DjSpeechFrontendVolume);
					sm_DjSpeechMonoSound->SetRequestedLPFCutoff(sm_DjSpeechFrontendLPF);
				}

				if(sm_DjSpeechPositionedSound)
				{
					isPlayingSpeech = true;
					f32 volumeLinearPos = Max<f32>(sm_VehicleRadioRiseVolumeCurve.CalculateValue(1.0f - playerVehicleInsideFactor),
						g_SilenceVolumeLin);
					f32 volumeDbPos = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinearPos);
					volumeDbPos += g_RadioAudioEntity.GetVolumeOffset();
					
					CVehicle *lastPlayerVehicle = g_RadioAudioEntity.GetLastPlayerVehicle();
					if(lastPlayerVehicle != NULL)
					{
						//Attach positioned retune sound to the current or last player vehicle.
						Vector3 vehiclePosition = VEC3V_TO_VECTOR3(lastPlayerVehicle->GetTransform().GetPosition());
						sm_DjSpeechPositionedSound->SetRequestedPosition(vehiclePosition);
						volumeDbPos += lastPlayerVehicle->GetVehicleAudioEntity()->GetAmbientRadioVolume();
					}
					else
					{
						// silence the dj if we dont have a vehicle to attach them to
						volumeDbPos = -100.f;
					}
					// Prevent a harmless assert
					volumeDbPos = Clamp(volumeDbPos + sm_PositionedPlayerVehicleRadioVolume, -100.f, 20.f);

					sm_DjSpeechPositionedSound->SetRequestedVolume(0.f);
					sm_DjSpeechPositionedSound->SetRequestedPostSubmixVolumeAttenuation(volumeDbPos);
					sm_DjSpeechPositionedSound->SetRequestedHPFCutoff(sm_PositionedPlayerVehicleRadioHPFCutoff);
					sm_DjSpeechPositionedSound->SetRequestedLPFCutoff(sm_PositionedPlayerVehicleRadioLPFCutoff);
					sm_DjSpeechPositionedSound->SetRequestedEnvironmentalLoudnessFloat(sm_PositionedPlayerVehicleRadioEnvironmentalLoudness);
					sm_DjSpeechPositionedSound->SetRequestedVolumeCurveScale(sm_PositionedPlayerVehicleRadioRollOff);
				}

				if(!isPlayingSpeech)
				{
					//The DJ speech has finished.
					sm_IMDrivenDjSpeech = false;
					sm_IMDrivenDjSpeechScheduled = false;
					sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
				}
				else if(!isDummySpeech && sm_DjSpeechState == RADIO_DJ_SPEECH_STATE_PLAYING)
				{
					const_cast<audRadioStation*>(playerStation)->m_LastTimeDJSpeechPlaying = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2);
				}
				break;

			case RADIO_DJ_SPEECH_STATE_IDLE: //Intentional fall-through.
			default:
				// Don't play DJ speech if we're playing a custom tracklist, rather than standard radio content
				if(!sm_IMDrivenDjSpeech && trackPlayTimeMs >= 0 && !playerStation->HasQueuedTrackList() && playerStation->GetNextTrack().IsInitialised())
				{
					s32 introStartTimeMs = 0;
					s32 introEndTimeMs = 0;
					s32 outroStartTimeMs = 0;
					s32 outroEndTimeMs = 0;
					if(playerStation->ExtractDjMarkers(introStartTimeMs, introEndTimeMs, outroStartTimeMs, outroEndTimeMs))
					{
						//We found some active DJ markers, so check if they are within our time window.

						u8 preferredDjSpeechCategory = RADIO_DJ_SPEECH_CAT_GENERAL;
						s32 windowStartTimeMs = 0;
						s32 windowLengthMs = -1;

						s32 timeToWindowMs = introStartTimeMs - trackPlayTimeMs;
						const u32 minPrepareTimeMs = playerStation->GetNameHash() == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC) ? 500 : g_RadioDjMinPrepareTimeMs;
						if(timeToWindowMs > minPrepareTimeMs)
						{
							windowStartTimeMs = introStartTimeMs;
							windowLengthMs = introEndTimeMs - introStartTimeMs;
							if(windowLengthMs > 0)
							{
								preferredDjSpeechCategory = RADIO_DJ_SPEECH_CAT_INTRO;
							}
						}
						else
						{
							audDebugf1("DJSPEECH: Intro time to window too short: %d", timeToWindowMs);
						}

						// We want to block Outro speech before a DJSOLO on MOTOMAMI as we're pushing the outro regions to the end of the tracks, and the energy levels can be very different between general and 
						// DJSOLO
						const bool allowOutroVO = !(playerStation->GetNameHash() == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC) && playerStation->GetNextTrack().GetCategory() == RADIO_TRACK_CAT_DJSOLO);

						timeToWindowMs = outroStartTimeMs - trackPlayTimeMs;
						if(allowOutroVO && (windowLengthMs <= 0) && (timeToWindowMs > g_RadioDjMinPrepareTimeMs))
						{
							windowStartTimeMs = outroStartTimeMs;
							windowLengthMs = outroEndTimeMs - outroStartTimeMs;
							if(windowLengthMs > 0)
							{
								preferredDjSpeechCategory = RADIO_DJ_SPEECH_CAT_OUTRO;
							}
						}

						if(windowLengthMs > 0)
						{
							//Decide if we want to attempt to play DJ speech at all.						
							const u32 nextTrackCategory = playerStation->GetNextTrack().GetCategory();

							const bool isValidCategoryToSelect = timeInMs >= playerStation->m_DjSpeechCategoryNextValidSelectionTime[preferredDjSpeechCategory];

							f32 djSpeechProbability = g_RadioDjSpeechProbability;
							
							if (playerStation->m_NameHash == ATSTRINGHASH("RADIO_34_DLC_HEI4_KULT", 0xE3442163))
							{
								djSpeechProbability = g_RadioDjSpeechProbabilityKult;
							}
							else if (playerStation->GetNameHash() == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC) && (preferredDjSpeechCategory == RADIO_DJ_SPEECH_CAT_OUTRO || preferredDjSpeechCategory == RADIO_DJ_SPEECH_CAT_GENERAL))
							{								
								djSpeechProbability = g_RadioDjOutroSpeechProbabilityMotomami;								
							}							

							// always speak when going to news or weather
							// always speak intros for takeover music
							// NOTE: using global random stream for dj speech
							if(BANK_ONLY(g_ForceDjSpeech || ) (playerStation->m_HasTakeoverContent && preferredDjSpeechCategory==RADIO_DJ_SPEECH_CAT_INTRO) || nextTrackCategory == RADIO_TRACK_CAT_WEATHER || nextTrackCategory == RADIO_TRACK_CAT_NEWS || (isValidCategoryToSelect && audEngineUtil::ResolveProbability(djSpeechProbability)))
							{
								u32 speechLengthMs = playerStation->CreateCompatibleDjSpeech(timeInMs, preferredDjSpeechCategory, windowLengthMs, windowStartTimeMs);
								if((speechLengthMs > 0) && sm_DjSpeechMonoSound && sm_DjSpeechPositionedSound)
								{
									//Log the game time that this DJ speech is intended to be played at.
									//NOTE: Align intro and outro speech to the end of the window.
									sm_ActiveDjSpeechStartTimeMs = windowStartTimeMs + (windowLengthMs - (s32)speechLengthMs);
									sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_PREPARING;
									sm_DjSpeechStation = playerStation;
									sm_DjSpeechNextTrackCategory = nextTrackCategory;

									audDebugf1("DJ Speech start time: %u (window %u, length %u, speechLength %u", sm_ActiveDjSpeechStartTimeMs, windowStartTimeMs, windowLengthMs, speechLengthMs);
								}
							}
							else
							{
								//Play a dummy (wrapper) speech sound to prevent valid DJ speech from being selected.
								audSoundInitParams initParams;
								initParams.TimerId = 2;
								initParams.u32ClientVar = 0xdeadbeef;
								g_RadioAudioEntity.CreateSound_PersistentReference(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("DjSpeechDummy", 0x94D36E43)), 
																						&sm_DjSpeechMonoSound, &initParams);
								if(sm_DjSpeechMonoSound)
								{
									g_RadioAudioEntity.CreateSound_PersistentReference(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("DjSpeechDummy", 0x94D36E43)), 
																							&sm_DjSpeechPositionedSound, &initParams);
									if(sm_DjSpeechPositionedSound)
									{
										sm_ActiveDjSpeechStartTimeMs = windowStartTimeMs + windowLengthMs;
										sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_PREPARING;
										sm_DjSpeechStation = playerStation;
										sm_DjSpeechNextTrackCategory = nextTrackCategory;
									}
									else
									{
										sm_DjSpeechMonoSound->StopAndForget();
									}
								}
							}
						}
					}
				}
				break;
		}
	}
	else
	{
		//The player is not listening to a radio station, so clean up DJ speech.
		if(sm_DjSpeechMonoSound)
		{
			sm_DjSpeechMonoSound->StopAndForget();
		}
		if(sm_DjSpeechPositionedSound)
		{
			sm_DjSpeechPositionedSound->StopAndForget();
		}
		sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;
	}
}

audRadioStation::audRadioStation(RadioStationSettings *stationSettings) :
m_StationSettings(stationSettings),
m_ForceNextTrackIndex(0xffff),
m_ForceNextTrackCategory(0xff),
m_ForceNextTrackContext(kNullContext),
m_ActiveTrackIndex(0),
m_DjSpeechHistoryWriteIndex(0),
m_SoundBucketId(0xff),
m_ShouldStreamPhysically(false),
m_IsPlayingOverlappedTrack(false),
m_IsPlayingMixTransition(false),
m_IsRequestingOverlappedTrackPrepare(false),
m_HasJustPlayedBackToBackMusic(false),
m_ShouldPlayFullRadio(false),
m_ScriptSetMusicOnly(false),
m_IsFirstMusicTrackSinceBoot(true),
m_IsMixStation(false),
m_IsFavourited(true),
m_IsReverbStation(false),
m_HasConstructedMixStationBeatMap(false)
{
	for(u32 trackIndex = 0; trackIndex < 2; trackIndex++)
	{
		m_WaveSlots[trackIndex] = NULL;
	}

	m_NameHash = atStringHash(m_StationSettings->Name);
	audAssertf(m_NameHash == atStringHash(audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StationSettings->NameTableOffset)), "RadioStationSettings %s's name field (%s) does not match the gameobject name", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StationSettings->NameTableOffset), m_StationSettings->Name);

	for(u32 i = 0; i < IMMUTABLE_INDEX_TYPE_MAX; i++)
	{
		m_StationImmutableIndex[i] = -1;
	}
	
	if(!IsHidden())
	{
		if(GameObjectHashList* tuneableStationHashList = audNorthAudioEngine::GetObject<GameObjectHashList>(ATSTRINGHASH("FIXED_ORDER_RADIO_STATION_HASH_LIST_TUNEABLE", 0x616CE649)))
		{
			for(u32 i = 0; i < tuneableStationHashList->numGameObjectHashes; i++)
			{
				if(tuneableStationHashList->GameObjectHashes[i].GameObjectHash == m_NameHash)
				{
					m_StationImmutableIndex[IMMUTABLE_INDEX_TUNEABLE] = i;
				}
			}

			audAssertf(m_StationImmutableIndex[IMMUTABLE_INDEX_TUNEABLE] >= 0, "Non-hidden station %s has not been assigned an immutable index - please add it *at the end*<-(IMPORTANT!) of FIXED_ORDER_RADIO_STATION_HASH_LIST_TUNEABLE", GetName());			
		}
	}

	if(GameObjectHashList* stationHashList = audNorthAudioEngine::GetObject<GameObjectHashList>(ATSTRINGHASH("FIXED_ORDER_RADIO_STATION_HASH_LIST_GLOBAL", 0xEE10457C)))
	{
		for(u32 i = 0; i < stationHashList->numGameObjectHashes; i++)
		{
			if(stationHashList->GameObjectHashes[i].GameObjectHash == m_NameHash)
			{
				m_StationImmutableIndex[IMMUTABLE_INDEX_GLOBAL] = i;
			}
		}

		audAssertf(m_StationImmutableIndex[IMMUTABLE_INDEX_GLOBAL] >= 0, "Station %s has not been assigned a global immutable index - please add it *at the end*<-(IMPORTANT!) of FIXED_ORDER_RADIO_STATION_HASH_LIST_GLOBAL", GetName());		
	}	

	for(u32 djHistoryIndex=0; djHistoryIndex<g_MaxDjSpeechInHistory; djHistoryIndex++)
	{
		m_DjSpeechHistory[djHistoryIndex] = 0;
	}

	//Randomly distribute the valid start times for each category across the stations.
	for(u32 djCatIndex=0; djCatIndex<NUM_RADIO_DJ_SPEECH_CATS; djCatIndex++)
	{
		// NOTE: using global random stream for DJ Speech
		const u32 repeatTime = (m_NameHash == ATSTRINGHASH("RADIO_34_DLC_HEI4_KULT", 0xE3442163)) ? g_RadioDjSpeechMinTimeBetweenRepeatedCategoriesKult[djCatIndex] : g_RadioDjSpeechMinTimeBetweenRepeatedCategories[djCatIndex];
		m_DjSpeechCategoryNextValidSelectionTime[djCatIndex] = static_cast<u32>(audEngineUtil::GetRandomNumberInRange(static_cast<s32>(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2)),
																														static_cast<s32>(repeatTime)));
	}

	m_NoBackToBackMusic = (AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_NOBACK2BACKMUSIC) == AUD_TRISTATE_TRUE);
	m_PlaysBackToBackAds = (AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_BACK2BACKADS) == AUD_TRISTATE_TRUE);
	m_PlayWeather = (AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_PLAYWEATHER) == AUD_TRISTATE_TRUE);
	m_PlayNews = (AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_PLAYNEWS) == AUD_TRISTATE_TRUE);
	m_PlaySequentialMusicTracks = (AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_SEQUENTIALMUSIC) == AUD_TRISTATE_TRUE);
	m_PlayIdentsInsteadOfAds = (AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_IDENTSINSTEADOFADS) == AUD_TRISTATE_TRUE);
	m_IsFrozen = false;
	m_IsFirstTrack = true;
	m_HasTakeoverContent = false;
	m_UseRandomizedStrideSelection = false;
	m_LastTimeDJSpeechPlaying = 0u;
	m_AmbientVolume = 0.0f;

	WIN32PC_ONLY(m_IsUserStation = AUD_GET_TRISTATE_VALUE(stationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_PLAYSUSERSMUSIC) == AUD_TRISTATE_TRUE);
}

audRadioStation::~audRadioStation()
{
	for(u32 trackIndex = 0; trackIndex < 2; trackIndex++)
	{
		m_Tracks[trackIndex].Shutdown();
	}
}

void audRadioStation::Init()
{
	const RadioStationSettings *stationSettings = m_StationSettings;

	m_Random.Reset(audEngineUtil::GetRandomInteger());	

	// Start by assuming we don't have a full set of content
	m_OnlyPlayAds = true;

	for(s32 i = 0; i < NUM_RADIO_TRACK_CATS; i++)
	{
		//Randomly distribute the valid start times for each category across the stations.
		const s32 nextValidSelectionTime = GetRandomNumberInRange(static_cast<s32>(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2)),
																						static_cast<s32>(sm_MinTimeBetweenRepeatedCategories->Category[i].Value));

		m_NextValidSelectionTime[i] = static_cast<u32>(nextValidSelectionTime);
		m_CategoryStride[i] = 0;
	}

	m_DLCInitialized = false;
	m_LastForcedMusicTrackList = 0u;

	for(u32 trackListIndex = 0; trackListIndex < stationSettings->numTrackLists; trackListIndex++)
	{
		RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(stationSettings->TrackList[trackListIndex].TrackListRef);
		if(trackList)
		{
			if (trackList->Category == RADIO_TRACK_CAT_MUSIC)
			{
				AddMusicTrackList(trackList, stationSettings->TrackList[trackListIndex].TrackListRef);
			}	
			else
			{
				AddTrackList(trackList);
			}

			if (trackList->Category != RADIO_TRACK_CAT_MUSIC)
			{
				if(sm_IsNetworkModeHistoryActive)
				{
					m_HasTakeoverContent |= (trackList->Category == RADIO_TRACK_CAT_TAKEOVER_MUSIC);
				}
				
			}
		}
	}
	
	if (m_HasTakeoverContent)
	{
		m_UseRandomizedStrideSelection = true;
	}

	if(m_NameHash == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC) ||
	   m_NameHash == ATSTRINGHASH("RADIO_03_HIPHOP_NEW", 0xFA17DE37) ||
	   m_NameHash == ATSTRINGHASH("RADIO_09_HIPHOP_OLD", 0x572C04))
	{
		m_UseRandomizedStrideSelection = true;
	}

	if(m_UseRandomizedStrideSelection)
	{
		RandomizeCategoryStrides(true, false);		
	}

	if (m_NameHash == ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_CLUB", 0x7E78FD4E) ||
		m_NameHash == ATSTRINGHASH("RADIO_23_DLC_BATTLE_MIX2_CLUB", 0x1FDBE6BE) ||
		m_NameHash == ATSTRINGHASH("RADIO_24_DLC_BATTLE_MIX3_CLUB", 0xB48221E3) ||
		m_NameHash == ATSTRINGHASH("RADIO_25_DLC_BATTLE_MIX4_CLUB", 0x7E95A6DD) ||
		m_NameHash == ATSTRINGHASH("DLC_BATTLE_MIX1_CLUB_PRIV", 0x34BF5EC8) ||
		m_NameHash == ATSTRINGHASH("DLC_BATTLE_MIX2_CLUB_PRIV", 0x6703639B) ||
		m_NameHash == ATSTRINGHASH("DLC_BATTLE_MIX3_CLUB_PRIV", 0x841DDC99) ||
		m_NameHash == ATSTRINGHASH("DLC_BATTLE_MIX4_CLUB_PRIV", 0xEC2974F9))
	{
		m_IsMixStation = true;
	}

	if(m_NameHash == ATSTRINGHASH("RADIO_03_HIPHOP_NEW", 0xFA17DE37))
	{
		m_NewTrackBias = 1.2f;
		m_FirstBootNewTrackBias = 2.f;
	}
	else
	{
		m_NewTrackBias = 1.f;
		m_FirstBootNewTrackBias = 1.f;
	}

	m_IsMixStation |= AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_ISMIXSTATION) == AUD_TRISTATE_TRUE;

	if (m_NameHash == ATSTRINGHASH("RADIO_30_DLC_HEI4_MIX1_REVERB", 0xCEE7651B))
	{
		m_IsReverbStation = true;
	}

	m_TakeoverMusicTrackCounter = 0;

	// Load tuning for iFruit station from Cloud Tunables to allow late tweaking
	if(m_NameHash == ATSTRINGHASH("RADIO_23_DLC_XM19_RADIO", 0x6D1C6C45))
	{
		m_TakeoverProbability = 0.4f;
		m_TakeoverMinTimeMs = 9 * 60000;
		m_TakeoverLastTimeMs = 0;	
		m_TakeoverMinTracks = 3;
		m_TakeoverMaxTracks = 4;
		m_TakeoverDjSoloProbability = 0.55f;

		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("RADIO_23_TAKEOVER_PROB", 0xD0D6E614), m_TakeoverProbability))
		{
			audDisplayf("RADIO_23_DLC_XM19_RADIO: Tunable defined TakeOver Probability: %f", m_TakeoverProbability);
		}
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("RADIO_23_TAKEOVER_MINTIME", 0xF374152), m_TakeoverMinTimeMs))
		{
			audDisplayf("RADIO_23_DLC_XM19_RADIO: Tunable defined TakeOver MinTime: %u", m_TakeoverMinTimeMs);
		}
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("RADIO_23_TAKEOVER_MINTRACKS", 0x5273ED4E), m_TakeoverMinTracks))
		{
			audDisplayf("RADIO_23_DLC_XM19_RADIO: Tunable defined TakeOver MinTracks: %u", m_TakeoverMinTracks);
		}
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("RADIO_23_TAKEOVER_MAXTRACKS", 0x5F217590), m_TakeoverMaxTracks))
		{
			audDisplayf("RADIO_23_DLC_XM19_RADIO: Tunable defined TakeOver MaxTracks: %u", m_TakeoverMaxTracks);
		}
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("RADIO_23_TAKEOVER_DJSOLO_PROB", 0x6A820BD6), m_TakeoverDjSoloProbability))
		{
			audDisplayf("RADIO_23_DLC_XM19_RADIO: Tunable defined TakeOver DjSolo Probability: %f", m_TakeoverDjSoloProbability);
		}
	}

	// Initialise as if we had a takeover in recent history, so we don't rule out hearing one in the first 10 minutes
	m_StationAccumulatedPlayTimeMs = GetRandomNumberInRange(0, m_TakeoverMinTimeMs);

	m_ListenTimer = 0.f;
	m_Genre = (RadioGenre)m_StationSettings->Genre;
	m_AmbientVolume = (f32)m_StationSettings->AmbientRadioVol;
	
	m_QueuedTrackList = NULL;
	m_QueuedTrackListIndex = 0;
	m_NetworkNextTrack.category = 0xff;

#if RSG_PC   // user music
	m_LastPlayedTrackIndex = ~0U;
	m_CurrentlyPlayingTrackIndex = ~0U;
	m_BadTrackCount = 0;
	sm_IsInRecoveryMode = false;
#endif

	m_FirstTrackStartOffset = GetRandomNumberInRange(0.1f, 0.8f);

	/// Mix Station now unlocked by default, unlock all tracks immediately
	if (m_NameHash == ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_RADIO", 0xF8BEAA16))
	{		
		UnlockTrackList("BATTLE_MIX1_RADIO");
		UnlockTrackList("BATTLE_MIX2_RADIO");
		UnlockTrackList("BATTLE_MIX3_RADIO");
		UnlockTrackList("BATTLE_MIX4_RADIO");
	}

#if __BANK
	u32 totalNumTracks = 0;
	for(s32 i = 0; i < NUM_RADIO_TRACK_CATS; i++)
	{
		const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(i);
		if(trackLists)
		{
			const u32 numTracks = trackLists->ComputeNumTracks();
			audDisplayf("%s %s: %u tracks", GetName(), TrackCats_ToString((TrackCats)i), numTracks);
			totalNumTracks += numTracks;
		}
	}

	audDebugf1("%s has %u total tracks", GetName(), totalNumTracks);
#endif

	m_MusicRunningCount = 0;
	
	/* Mix Station now unlocked by default, no longer required
	// Lock DJ music station by default, relies on script mechanism to unlock
	if (m_NameHash == ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_RADIO", 0xF8BEAA16))
	{
		SetLocked(true);
	}
	*/
}

void audRadioStation::RandomizeCategoryStrides(bool randomizeStartTrack, bool maintainDirection)
{
	// Assign a random start index and stride for all required track categories
	for (u32 category = 0; category < NUM_RADIO_TRACK_CATS; category++)
	{
		RandomizeCategoryStride((TrackCats)category, randomizeStartTrack, maintainDirection);		
	}
}

void audRadioStation::RandomizeCategoryStride(TrackCats category, bool randomizeStartTrack, bool maintainDirection)
{
	if (IsRandomizedStrideCategory(category))
	{			
		u32 numTracksAvailable = ComputeNumTracksAvailable(category);

		if(numTracksAvailable > 0)
		{
			const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(category);

			// If this station has old and new music we want to generate separate stride/index values for both the
			// track lists so that we can step through them independently (as if we just treat it as one big list
			// we're going to hear all the old then all the new music if we have a stride value if +1/-1
			if(trackLists && trackLists->GetUnlockedListCount() == 2)
			{
				audDisplayf("Station %s is in old/new stride mode for category %s", GetName(), TrackCats_ToString(category));

				u32 unlockedListIndex = 0;

				for(u32 i = 0; i < trackLists->GetListCount(); i++)
				{
					if(trackLists->IsTrackListUnlocked(trackLists->GetList(i)))
					{
						if(unlockedListIndex == 0)
						{
							RandomizeCategoryStride(category, trackLists->GetList(i)->numTracks, randomizeStartTrack, maintainDirection);
						}
						else
						{
							TrackCats takeoverCategory= TRACKCATS_MAX;
							if(category == RADIO_TRACK_CAT_MUSIC) { takeoverCategory = RADIO_TRACK_CAT_TAKEOVER_MUSIC; }
							if(category == RADIO_TRACK_CAT_IDENTS) { takeoverCategory = RADIO_TRACK_CAT_TAKEOVER_IDENTS; }
							if(category == RADIO_TRACK_CAT_DJSOLO) { takeoverCategory = RADIO_TRACK_CAT_TAKEOVER_DJSOLO; }
							RandomizeCategoryStride(takeoverCategory, trackLists->GetList(i)->numTracks, randomizeStartTrack, maintainDirection);
						}

						unlockedListIndex++;
					}
				}
			}
			else
			{
				RandomizeCategoryStride(category, numTracksAvailable, randomizeStartTrack, maintainDirection);
			}
		}
	}
}

void audRadioStation::RandomizeCategoryStride(TrackCats category, u32 numTracksAvailable, bool randomizeStartTrack, bool maintainDirection)
{
	if (numTracksAvailable > 0)
	{
		// Pick a random stride per-category (current either just steps forwards or backwards through the tracks)
		// Nb. Don't go +/- 8! This gets packed into a 4 bit value for network syncing
		if(!maintainDirection)
		{
			m_CategoryStride[category] = GetRandomNumberInRange(0.0f, 1.0f) >= 0.5f ? -1 : 1;
		}
		// Normalize back to -1 or 1 depending on previously picked direction
		else
		{
			m_CategoryStride[category] = m_CategoryStride[category] > 0 ? 1 : -1;
		}

		// If we've got an odd number of tracks, we can step through two at a time for extra variety without repeating
		if (numTracksAvailable % 2 == 1 && GetRandomNumberInRange(0.0f, 1.0f) >= 0.5f)
		{
			m_CategoryStride[category] *= 2;
		}
		
		if (audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(category)))
		{
			if(randomizeStartTrack || (*history)[0] == -1)
			{
				// Pick a random start track per-category
				(*history)[0] = GetRandomNumberInRange(0, numTracksAvailable - 1);
				history->SetWriteIndex(1);
			}
		}
	}
}

// Any logic that needs to be applied after DLC tracks have been merged should go here
void audRadioStation::DLCInit()
{
	if (!m_DLCInitialized)
	{
		// USB station track lists are all locked by default
		if (IsUSBMixStation())
		{
#if __BANK
			g_NumUSBStationTrackLists = 0;
			audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

			if (trackListCollection)
			{				
				for (u32 i = 0; i < trackListCollection->GetListCount(); i++)
				{
					g_USBTrackListNames[g_NumUSBStationTrackLists++] = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackListCollection->GetList(i)->NameTableOffset);
				}
			}

			if(g_USBStationTrackListCombo)
			{
				g_USBStationTrackListCombo->UpdateCombo("Track List", &g_RequestedUSBTrackListComboIndex, g_NumUSBStationTrackLists, g_USBTrackListNames.GetElements());
			}
#endif
			
			ForceMusicTrackList(ATSTRINGHASH("TUNER_AP_SILENCE_MUSIC", 0x68865F9), 0u);
		}

		// USB tracks are all locked by default so no point randomizing
		if (m_PlaySequentialMusicTracks && !IsUSBMixStation())
		{
			RandomizeSequentialStationPlayPosition();
		}

		m_DLCInitialized = true;
	}	
}

void audRadioStation::RandomizeSequentialStationPlayPosition()
{
	audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
	const s32 numTracks = ComputeNumTracksAvailable(RADIO_TRACK_CAT_MUSIC);

	bool hasValidTracks = history && numTracks >= 1;
	audAssertf(hasValidTracks || IsUSBMixStation(), "Attempting to play sequential music tracks but track list is invalid or the list has less than one track");

	if (hasValidTracks)
	{
		u32 firstTrackIndex = 0;
		u32 lastTrackIndex = 0;
		s32 tuneablePrioritizedTrackListIndex = IsNetworkModeHistoryActive() ? GetTuneablePrioritizedMusicTrackListIndex(firstTrackIndex, lastTrackIndex) : -1;
		m_FirstTrackStartOffset = GetRandomNumberInRange(0.1f, 0.8f);

		if (tuneablePrioritizedTrackListIndex >= 0)
		{
			(*history)[0] = (u32)GetRandomNumberInRange((s32)firstTrackIndex, (s32)lastTrackIndex);
			audDisplayf("Radio station %s has randomised to %.02f%% through track %u/%u (prioritized track list %d)", GetName(), m_FirstTrackStartOffset * 100.f, (*history)[0], numTracks, tuneablePrioritizedTrackListIndex);
		}
		else
		{
			(*history)[0] = (u32)GetRandomNumberInRange(0, numTracks - 1);
			audDisplayf("Radio station %s has randomised to %.02f%% through track %u/%u", GetName(), m_FirstTrackStartOffset * 100.f, (*history)[0], numTracks);
		}

		history->SetWriteIndex(1);
	}
}

s32 audRadioStation::GetTuneablePrioritizedMusicTrackListIndex(u32& firstTrackIndex, u32& lastTrackIndex)
{
	const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);
	f32* trackListProbabilities = Alloca(f32, trackLists->GetListCount());
	bool foundCustomProbabilityValue = false;
	f32 probabilitySum = 0.f;

	for (u32 trackListIndex = 0; trackListIndex < trackLists->GetListCount(); trackListIndex++)
	{
		f32 trackListProbability = 0.0f;

		if (!trackLists->IsListLocked(trackListIndex))
		{
			trackListProbability = Max(0.0f, trackLists->GetListFirstTrackProbability(trackListIndex));
			foundCustomProbabilityValue |= trackListProbability != 1.f;
		}

		trackListProbabilities[trackListIndex] = trackListProbability;
		probabilitySum += trackListProbability;
	}

	if (foundCustomProbabilityValue)
	{
		audDisplayf("Found first music track probability modifier on station %s (%u tracks total)", GetName(), ComputeNumTracksAvailable(RADIO_TRACK_CAT_MUSIC));
		f32 randomTrackIndex = audEngineUtil::GetRandomNumberInRange(0.f, probabilitySum);

#if __BANK
		for (u32 trackListIndex = 0; trackListIndex < trackLists->GetListCount(); trackListIndex++)
		{
			const bool trackListLocked = trackLists->IsListLocked(trackListIndex);

			audDisplayf("Track List %d (%s) probability: %.02f, %u tracks %s",
				trackListIndex,
				audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackLists->GetList(trackListIndex)->NameTableOffset),
				trackLists->GetListFirstTrackProbability(trackListIndex),
				trackListLocked ? 0 : trackLists->GetList(trackListIndex)->numTracks,
				trackListLocked ? "(locked)" : "");
		}

		audDisplayf("Random track list index: %.02f", randomTrackIndex);
#endif

		for (u32 trackListIndex = 0; trackListIndex < trackLists->GetListCount(); trackListIndex++)
		{
			const RadioStationTrackList* trackList = trackLists->GetList(trackListIndex);
			const bool trackListLocked = trackLists->IsListLocked(trackListIndex);
			const u32 numTracks = trackListLocked ? 0 : trackList->numTracks;

			if (trackListProbabilities[trackListIndex] > 0.f)
			{
				randomTrackIndex -= trackListProbabilities[trackListIndex];

				if (randomTrackIndex <= 0.f)
				{
					lastTrackIndex = firstTrackIndex + numTracks - 1;
					return trackListIndex;
				}
			}
			
			firstTrackIndex += numTracks;
		}
	}

	return -1;
}

void audRadioStation::Shutdown(void)
{
	for(u32 trackIndex = 0; trackIndex < 2; trackIndex++)
	{
		m_Tracks[trackIndex].Shutdown();
	}
	m_IsFirstTrack = true;
	m_LastTimeDJSpeechPlaying = 0u;
}

void audRadioStation::LogTrack(const u32 BANK_ONLY(category), const u32 BANK_ONLY(context), const u32 BANK_ONLY(index), const u32 BANK_ONLY(soundRef))
{
#if __BANK	
	char buf[256];
	formatf(buf, "%s,%u,%s,%u,%u,%s\r\n", 
		GetName(), 
		g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2), 
		TrackCats_ToString((TrackCats)category),
		index,
		context,
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(soundRef));

	if(g_LogRadio && m_ShouldStreamPhysically)
	{
		FileHandle fi = CFileMgr::OpenFileForAppending("common:/RadioLog.csv");
		if(fi != INVALID_FILE_HANDLE)
		{	
			CFileMgr::Write(fi, buf, istrlen(buf));
			CFileMgr::CloseFile(fi);
		}
	}
	if(IsNetworkModeHistoryActive())
	{
		audDebugf1("NETRADIO,%s", buf);
	}	
#endif
}

u32 audRadioStation::PackCategoryForNetwork(const u32 category)
{
	if(category >= NUM_RADIO_TRACK_CATS)
	{
		return 0;
	}
	return category+1;
}

u32 audRadioStation::UnpackCategoryFromNetwork(const u32 category)
{
	if(category == 0)
	{
		return 0xFFFF;
	}
	return category - 1;
}

bool audRadioStation::ShouldAddCategoryToNetworkHistory(const u32 category)
{
	return (category == RADIO_TRACK_CAT_MUSIC || category == RADIO_TRACK_CAT_DJSOLO || category == RADIO_TRACK_CAT_ADVERTS
		|| category == RADIO_TRACK_CAT_TAKEOVER_MUSIC || category == RADIO_TRACK_CAT_TAKEOVER_DJSOLO);
}

void audRadioStation::Update(u32 timeInMs)
{
	// Need to defer doing this until the sound data has been loaded for a given pack
	if (m_IsMixStation && !IsUSBMixStation() && !m_HasConstructedMixStationBeatMap)
	{
		ConstructMixStationBeatMarkerList();
		m_HasConstructedMixStationBeatMap = true;
	}

	bool disableNetworkModeForStation = m_UseRandomizedStrideSelection;
	WIN32PC_ONLY(disableNetworkModeForStation |= m_IsUserStation);

	//Reset the TrackChanged flag
	m_bHasTrackChanged = false;

	if(!m_ShouldStreamPhysically)
	{
		//We can always select full radio content when this station is streaming virtually.
		m_ShouldPlayFullRadio = true;
	}

	if(!m_Tracks[m_ActiveTrackIndex].IsInitialised())
	{
		if(m_IsPlayingOverlappedTrack)
		{
			//Our active track has finished, but we're already overlapping playback of the next track.

			//Make the overlapped track our active track.
			m_bHasTrackChanged = true;
			m_ActiveTrackIndex = (m_ActiveTrackIndex + 1) % 2;
			m_IsPlayingOverlappedTrack = false;
			m_IsRequestingOverlappedTrackPrepare = false;

			if(!disableNetworkModeForStation && (IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get()))
			{
				audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
				if(history)
				{
					if(m_Tracks[m_ActiveTrackIndex].GetSoundRef() != ATSTRINGHASH("dlc_ch_finale_radio_djsolo", 0x14F5707))
					{
						if(ShouldAddCategoryToNetworkHistory(m_Tracks[m_ActiveTrackIndex].GetCategory()))
						{
							history->AddToHistory(m_Tracks[m_ActiveTrackIndex].GetRefForHistory());
						}
					}
				}
			}

			//Give the management code a frame to catch the track transition before attempting to select
			//and prepare the next track.
			return;
		}
		else
		{
			u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;

			//We need to prepare a track to get us started.
			if(!m_Tracks[nextTrackIndex].IsInitialised())
			{
				InitialiseNextTrack(timeInMs);
			}
			audPrepareState state = PrepareNextTrack();
			
			switch(state)
			{
				case AUD_PREPARED:
					m_bHasTrackChanged = true;
					m_ActiveTrackIndex = (m_ActiveTrackIndex + 1) % 2;
					m_Tracks[m_ActiveTrackIndex].Play();

					if(!disableNetworkModeForStation && (IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get()))
					{
						audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
						if(history)
						{							
							if(ShouldAddCategoryToNetworkHistory(m_Tracks[m_ActiveTrackIndex].GetCategory()))
							{
								history->AddToHistory(m_Tracks[m_ActiveTrackIndex].GetRefForHistory());
							}							
						}
					}

#if RSG_PC
					m_LastPlayedTrackIndex = m_CurrentlyPlayingTrackIndex;
					m_CurrentlyPlayingTrackIndex = m_Tracks[m_ActiveTrackIndex].GetTrackIndex();
					m_BadTrackCount = 0; // Reset bad track count. We got a good one.
#endif
					break;

				case AUD_PREPARE_FAILED:
					{
						OUTPUT_ONLY(char* nextTrackName = (char *)m_Tracks[nextTrackIndex].GetBankName();)

						naErrorf("Failed to prepare a track (%s) for a radio station (%s)", nextTrackName ? nextTrackName : "Unknown", GetName());

						//Kill the next track and try to prepare another next frame.
						m_Tracks[nextTrackIndex].Shutdown();
#if RSG_PC
						if(m_IsUserStation)
						{
							m_BadTrackCount++;
							Warningf("BadTrackCount: %u", m_BadTrackCount);
						}
#endif
					}					

				//Intentional fall-through.
				case AUD_PREPARING:
				default:
					return;
			}
		}
	}
	else if(IsNetworkModeHistoryActive() && m_Tracks[m_ActiveTrackIndex].IsInitialised() && m_Tracks[m_ActiveTrackIndex].IsDormant())
	{
		// B*1504812
		audWarningf("%s - Should be playing but track state is dormant, requesting play", GetName());
		m_Tracks[m_ActiveTrackIndex].PlayWhenReady();
	}

	m_Tracks[m_ActiveTrackIndex].Update(timeInMs, IsFrozen());	

	//
	//We're playing an active track, so prepare the next track.
	//

#if RSG_PC // user music
	if(m_IsUserStation)
	{
		USERRADIO_PLAYMODE playMode = sm_UserRadioTrackManager.GetRadioMode();
		if(playMode == USERRADIO_PLAY_MODE_SEQUENTIAL && (sm_ForceUserPrevTrack || sm_ForceUserNextTrack))
		{
			const s32 numTracks = sm_UserRadioTrackManager.GetNumTracks();
			if(numTracks > 0)
			{
				if(sm_ForceUserPrevTrack)
				{
					sm_UserRadioTrackManager.SetNextTrack((m_Tracks[m_ActiveTrackIndex].GetTrackIndex() + numTracks - 1) % numTracks);
				}
				else
				{
					sm_UserRadioTrackManager.SetNextTrack((m_Tracks[m_ActiveTrackIndex].GetTrackIndex() + 1) % numTracks);
				}
			}
		}

		m_ShouldPlayFullRadio = (sm_IsInRecoveryMode || playMode == USERRADIO_PLAY_MODE_RADIO);

		// ditch current track
		if(sm_ForceUserNextTrack || sm_ForceUserPrevTrack)
		{			
			m_Tracks[m_ActiveTrackIndex].Shutdown();
		}

		// if we're going backwards, ditch the next track too
		if(sm_ForceUserPrevTrack || (playMode == USERRADIO_PLAY_MODE_SEQUENTIAL && sm_ForceUserNextTrack))
		{
			u32 nextTrackIndex = (m_ActiveTrackIndex+1)%2;
			m_Tracks[nextTrackIndex].Shutdown();
		}

		if(sm_ForceUserNextTrack || sm_ForceUserPrevTrack)
		{
			sm_ForceUserNextTrack = false;
			sm_ForceUserPrevTrack = false;
			return;
		}

		sm_ForceUserNextTrack = false;
		sm_ForceUserPrevTrack = false;

		if(m_Tracks[m_ActiveTrackIndex].IsInitialised() && m_Tracks[m_ActiveTrackIndex].IsStreamingPhysically() && m_Tracks[m_ActiveTrackIndex].GetPlayTime() > 1000 && m_Tracks[m_ActiveTrackIndex].GetCategory() == RADIO_TRACK_CAT_MUSIC)
		{
			// we've succesfully played a user track; recovery mode is no longer required
			sm_IsInRecoveryMode = false;
		}		
	}
#endif

	u8 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
	if(m_IsPlayingOverlappedTrack)
	{
		//Sanity-check that we are playing a track of the correct category.
		if(ShouldOnlyPlayMusic() && m_Tracks[nextTrackIndex].GetCategory() != RADIO_TRACK_CAT_MUSIC)
		{
			//We should only choose music.
			Assert(!IsFrozen());
			//Stop playing this track and allow a music track to be selected next frame.
			audDisplayf("Station %s should only play music, but overlapped track (%u) is not music - shutting down", GetName(), m_Tracks[nextTrackIndex].GetSoundRef());
			m_Tracks[nextTrackIndex].Shutdown();
			m_IsPlayingOverlappedTrack = false;
			m_IsRequestingOverlappedTrackPrepare = false;
			return;
		}

		m_Tracks[nextTrackIndex].Update(timeInMs, IsFrozen());
	}
	else
	{
		//Sanity-check that we are playing a track of the correct category.
		if(ShouldOnlyPlayMusic() && m_Tracks[m_ActiveTrackIndex].GetCategory() != RADIO_TRACK_CAT_MUSIC)
		{
			//We should only choose music.
			Assert(!IsFrozen());
			//Stop playing this track and allow a music track to be selected next frame.
			audDisplayf("Station %s should only play music, but active track (%u) is not music - shutting down", GetName(), m_Tracks[m_ActiveTrackIndex].GetSoundRef());
			m_Tracks[m_ActiveTrackIndex].Shutdown();
			return;
		}
		
		if(!m_Tracks[nextTrackIndex].IsInitialised())
		{
			InitialiseNextTrack(timeInMs);
		}

		//Prepare the next track.
		audRadioTrack &activeTrack = m_Tracks[m_ActiveTrackIndex];
		s32 playTime = activeTrack.GetPlayTime();
		s32 duration = activeTrack.GetDuration();

		// Wait until the active track is in a playing state before attempting to prepare the next track for the overlap, otherwise
		// retuning to a track within the overlap window can cause problems.
		if(activeTrack.IsPlayingPhysicallyOrVirtually() && playTime >= (duration - g_RadioTrackPrepareTimeMs) && !sm_DisableOverlap)
		{
			if(m_IsRequestingOverlappedTrackPrepare)
			{
				audPrepareState state = PrepareNextTrack();
				if(state == AUD_PREPARED)
				{				
					s32 compensatedPlayTime = playTime;

					if(!g_DisablePlayTimeCompensation && !m_IsMixStation && !m_PlaySequentialMusicTracks && !disableNetworkModeForStation && (IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get()))
					{
						// compensate for drift by sliding the overlap
						compensatedPlayTime = Clamp(static_cast<s32>(audDriver::GetMixer()->GetMixerTimeMs() - activeTrack.GetTimePlayed()),
															playTime - 500,
															playTime + 500);

						// TODO: accumulate error when >500ms
					}
					
					m_IsPlayingMixTransition = m_Tracks[nextTrackIndex].IsFlyloPart1() != m_Tracks[m_ActiveTrackIndex].IsFlyloPart1();
					u32 overlapTimeMs = WIN32PC_ONLY(m_IsUserStation ? g_UserRadioTrackOverlapTimeMs : )g_RadioTrackOverlapTimeMs;
					u32 overlapTimePlayMs = overlapTimeMs;

					if (g_UsePreciseTrackCrossfades && m_PlaySequentialMusicTracks && m_Tracks[nextTrackIndex].GetStreamingSound())
					{
						overlapTimePlayMs += g_RadioTrackOverlapPlayTimeMs;
					}

					if(compensatedPlayTime >= (duration - overlapTimePlayMs))
					{
						if (g_UsePreciseTrackCrossfades && m_PlaySequentialMusicTracks && m_Tracks[nextTrackIndex].GetStreamingSound())
						{
							u32 trackTimeRemaining = duration - playTime;
							u32 trackTimeCalculationFrame = activeTrack.GetPlayTimeCalculationMixerFrame();
							f32 trackStartMixerFrames = trackTimeCalculationFrame + ((trackTimeRemaining - overlapTimeMs) / (g_SecondsPerMixBuffer * 1000));

#if RSG_ORBIS
							trackStartMixerFrames += 1168 / (f32)kMixBufNumSamples;
#endif

							u32 startOffsetFrames = (u32)floor(trackStartMixerFrames);
							u16 subFrameOffset = (u16)((trackStartMixerFrames - startOffsetFrames) * kMixBufNumSamples);
							m_Tracks[nextTrackIndex].GetStreamingSound()->SetRequestedMixerPlayTimeAbsolute(startOffsetFrames, subFrameOffset);
						}
						
						m_Tracks[nextTrackIndex].Play();
						m_IsPlayingOverlappedTrack = true;
	#if RSG_PC
						m_LastPlayedTrackIndex = m_CurrentlyPlayingTrackIndex;
						m_CurrentlyPlayingTrackIndex =  m_Tracks[nextTrackIndex].GetTrackIndex();
						m_BadTrackCount = 0;
	#endif
					}
				}
	#if RSG_PC // this block came from GTA IV user music, is it still needed?  SORR
				else if(state == AUD_PREPARE_FAILED)
				{
					//Kill the next track and try to prepare another next frame.
					m_Tracks[nextTrackIndex].Shutdown();

					m_BadTrackCount++;
					Warningf("BadTrackCount: %u", m_BadTrackCount);
				}
#endif
			}
			
			m_IsRequestingOverlappedTrackPrepare = true;
		}
		else
		{
			m_IsRequestingOverlappedTrackPrepare = false;
		}
	}

	if(m_ShouldStreamPhysically)
	{
		if(audRadioSlot::FindSlotByStation(this) == NULL)
		{
			naWarningf("Station %s playing physically with no valid slot; forcing virtual streaming", GetName());
			SetPhysicalStreamingState(false);
		}
	}

#if __BANK
	if(m_bHasTrackChanged)
	{
		audDisplayf("Track changed on radio station %s - new track %s (%s track %d) - offset %.02f, network time %u", GetName(), SOUNDFACTORY.GetMetadataManager().GetObjectName(m_Tracks[m_ActiveTrackIndex].GetSoundRef()), m_Tracks[m_ActiveTrackIndex].GetCategory() >= NUM_RADIO_TRACK_CATS ? "Invalid" : TrackCats_ToString((TrackCats)m_Tracks[m_ActiveTrackIndex].GetCategory()), m_Tracks[m_ActiveTrackIndex].GetTrackIndex(), m_Tracks[m_ActiveTrackIndex].GetPlayFraction(), NetworkInterface::GetSyncedTimeInMilliseconds());
	}
#endif
}

bool audRadioStation::IsStreamingPhysically() const
{
	for(u32 trackIndex = 0; trackIndex < 2; trackIndex++)
	{
		if(m_Tracks[trackIndex].IsInitialised())
		{
			if(m_Tracks[trackIndex].IsStreamingPhysically())
			{
				return true;
			}
		}
	}

	return false;
}

bool audRadioStation::IsStreamingVirtually() const
{
	bool isStreamingVirtually = false;

	if(m_Tracks[m_ActiveTrackIndex].IsInitialised())
	{
		isStreamingVirtually = m_Tracks[m_ActiveTrackIndex].IsStreamingVirtually();
	}

	//Also check the next track if need be.
	u8 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
	if(isStreamingVirtually && m_Tracks[nextTrackIndex].IsInitialised())
	{
		isStreamingVirtually = m_Tracks[nextTrackIndex].IsStreamingVirtually();
	}

	return isStreamingVirtually;
}

void audRadioStation::SetStreamSlots(audStreamSlot **streamSlots)
{
	m_WaveSlots[0] = NULL;
	m_WaveSlots[1] = NULL;

	if(streamSlots)
	{
		if(streamSlots[0])
		{
			m_WaveSlots[0] = streamSlots[0]->GetWaveSlot();
		}

		if(streamSlots[1])
		{
			m_WaveSlots[1] = streamSlots[1]->GetWaveSlot();
		}
	}
}

void audRadioStation::SetPhysicalStreamingState(bool shouldStreamPhysically, audStreamSlot **streamSlots, u8 soundBucketId)
{
	m_ShouldStreamPhysically = shouldStreamPhysically;
	m_SoundBucketId = soundBucketId;

	m_WaveSlots[0] = NULL;
	m_WaveSlots[1] = NULL;

	if(streamSlots)
	{
		if(streamSlots[0])
		{
			m_WaveSlots[0] = streamSlots[0]->GetWaveSlot();
		}

		if(streamSlots[1])
		{
			m_WaveSlots[1] = streamSlots[1]->GetWaveSlot();
		}
	}
	bool disableNetworkModeForStation = false;
	WIN32PC_ONLY(disableNetworkModeForStation = m_IsUserStation);

	audRadioTrack &activeTrack = m_Tracks[m_ActiveTrackIndex];
	if(activeTrack.IsInitialised())
	{
		bool shouldSkipEnd = false;
		s32 playTime = activeTrack.GetPlayTime();
		s32 duration = (s32)activeTrack.GetDuration();
		if(!disableNetworkModeForStation && (IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get()))
		{
			shouldSkipEnd = playTime >= (duration - 2000U);
		}
		else if(playTime >= (duration - g_RadioTrackSkipEndTimeMs))
		{
			shouldSkipEnd = true;
		}

		if(!IsFrozen() && m_ShouldStreamPhysically && (shouldSkipEnd || m_IsPlayingOverlappedTrack))
		{		
			//We are about to physically stream the end of a track or an overlap,
			//so stop the finishing track to prevent startup problems.
			activeTrack.Shutdown();
		}
		else
		{
			activeTrack.SetPhysicalStreamingState(shouldStreamPhysically, m_WaveSlots[m_ActiveTrackIndex], soundBucketId);
		}
	}

	const u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
	audRadioTrack &nextTrack = m_Tracks[nextTrackIndex];
	if(nextTrack.IsInitialised())
	{
		nextTrack.SetPhysicalStreamingState(shouldStreamPhysically, m_WaveSlots[nextTrackIndex], soundBucketId);
	}
}

void audRadioStation::AddActiveDjSpeechToHistory(u32 timeInMs)
{
	//Make sure we didn't already add this track.
	u8 previousHistoryWriteIndex = (m_DjSpeechHistoryWriteIndex + g_MaxDjSpeechInHistory - 1) %
		g_MaxDjSpeechInHistory;
	if(m_DjSpeechHistory[previousHistoryWriteIndex] != sm_ActiveDjSpeechId)
	{
		// Speech category could potentially be invalid if we've chosen to trigger some dummy DJ speech
		if(sm_ActiveDjSpeechCategory < NUM_RADIO_DJ_SPEECH_CATS)
		{
			RADIODEBUG3("DjSpeech %s:%u", g_RadioDjSpeechCategoryNames[sm_ActiveDjSpeechCategory], sm_ActiveDjSpeechId);

			m_DjSpeechHistory[m_DjSpeechHistoryWriteIndex] = sm_ActiveDjSpeechId;
			m_DjSpeechHistoryWriteIndex = (m_DjSpeechHistoryWriteIndex + 1) % g_MaxDjSpeechInHistory;

			const u32 repeatTime = (m_NameHash == ATSTRINGHASH("RADIO_34_DLC_HEI4_KULT", 0xE3442163)) ? g_RadioDjSpeechMinTimeBetweenRepeatedCategoriesKult[sm_ActiveDjSpeechCategory] : g_RadioDjSpeechMinTimeBetweenRepeatedCategories[sm_ActiveDjSpeechCategory];

			//Log the next time that this category can be selected.		
			m_DjSpeechCategoryNextValidSelectionTime[sm_ActiveDjSpeechCategory] = timeInMs + repeatTime;
		}
	}
}

u32 audRadioStation::CreateCompatibleDjSpeech(u32 timeInMs, u8 preferredCategory, s32 windowLengthMs, const s32 windowStartTime) const
{
	naCErrorf(preferredCategory < NUM_RADIO_DJ_SPEECH_CATS, "Invalid preferred category passed into CreateCompatibleDjSpeech");

	audDebugf1("DJSPEECH CreateCompatibleDjSpeech: %s, %d, %d", g_RadioDjSpeechCategoryNames[preferredCategory], windowLengthMs, windowStartTime);

	u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
	u32 nextTrackCategory = m_Tracks[nextTrackIndex].GetCategory();
	const bool inTakeoverState = m_Tracks[m_ActiveTrackIndex].GetCategory() == RADIO_TRACK_CAT_TAKEOVER_MUSIC;
	
	// no TIME DJ speech for iFruit in takeover mode or MOTOMAMI
	const bool disableTimeCategory = inTakeoverState || m_NameHash == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC);

	if(preferredCategory == RADIO_DJ_SPEECH_CAT_INTRO)
	{
		//Check if we should try and play time of day banter, ensuring that we don't hear this too often.
		// NOTE: using global random stream for dj speech
		if(!disableTimeCategory && audEngineUtil::ResolveProbability(g_RadioDjTimeProbability) &&
			(timeInMs >= m_DjSpeechCategoryNextValidSelectionTime[RADIO_DJ_SPEECH_CAT_TIME]))
		{
			preferredCategory = RADIO_DJ_SPEECH_CAT_TIME;
		}
	}
	else if(preferredCategory == RADIO_DJ_SPEECH_CAT_OUTRO) //NOTE: We don't currently have track-specific outros.
	{
		//Override outro speech with to... speech if we are playing a relevant track category next.
		if((nextTrackCategory == RADIO_TRACK_CAT_ADVERTS) || (nextTrackCategory == RADIO_TRACK_CAT_NEWS) ||
			(nextTrackCategory == RADIO_TRACK_CAT_WEATHER))
		{
			preferredCategory = RADIO_DJ_SPEECH_CAT_TO;
		}
		//Check if we should try and play time of day banter, ensuring that we don't hear this too often.
		// NOTE: using global random stream for dj speech
		else if(!inTakeoverState && audEngineUtil::ResolveProbability(g_RadioDjTimeProbability) &&
			(timeInMs >= m_DjSpeechCategoryNextValidSelectionTime[RADIO_DJ_SPEECH_CAT_TIME]))
		{
			preferredCategory = RADIO_DJ_SPEECH_CAT_TIME;
		}
		else
		{
			preferredCategory = RADIO_DJ_SPEECH_CAT_GENERAL;
		}
	}

	const char *categoryName = g_RadioDjSpeechCategoryNames[preferredCategory];

	// Override GENERAL with TAKEOVER_GENERAL if we're in a takeover state, or DD_GENERAL for WCC launch event
	if(preferredCategory == RADIO_DJ_SPEECH_CAT_GENERAL)
	{
		if(inTakeoverState)
		{
			categoryName = "TAKEOVER_GENERAL";
		}
		else if((m_NameHash == ATSTRINGHASH("RADIO_09_HIPHOP_OLD", 0x572C04) && IsTrackListUnlocked(ATSTRINGHASH("RADIO_09_HIPHOP_OLD_DD_MUSIC_LAUNCH", 0x2018C9AF))) || 
				(m_NameHash == ATSTRINGHASH("RADIO_03_HIPHOP_NEW", 0xFA17DE37) && IsTrackListUnlocked(ATSTRINGHASH("RADIO_03_HIPHOP_NEW_DD_MUSIC_LAUNCH", 0x773C7204))))
		{
			categoryName = "DD_GENERAL";
		}
		else if((m_NameHash == ATSTRINGHASH("RADIO_09_HIPHOP_OLD", 0x572C04) && IsTrackListUnlocked(ATSTRINGHASH("RADIO_09_HIPHOP_OLD_DD_MUSIC_POST_LAUNCH", 0x9ACEDFBE))) || 
		     	(m_NameHash == ATSTRINGHASH("RADIO_03_HIPHOP_NEW", 0xFA17DE37) && IsTrackListUnlocked(ATSTRINGHASH("RADIO_03_HIPHOP_NEW_DD_MUSIC_POST_LAUNCH", 0x3ED8B9F2))))
		{
			categoryName = "PL_GENERAL";
		}
	}
	
	formatf(g_RadioDjVoiceName, "DJ_%s_%s", GetName(), categoryName);

	//Determine the correct context name.
	s32 gameHours = CClock::GetHour();
	g_RadioDjContextName[0] = '\0';
	char *bankName;

	switch(preferredCategory)
	{
		case RADIO_DJ_SPEECH_CAT_INTRO:
		case RADIO_DJ_SPEECH_CAT_OUTRO: //Intentional fall-through.
			//The context name is the name of the streaming wave bank for specific intros and outros.
			// If the current track has multiple text Ids then we fall back to using the text id, rather than bank name (ie mix stations)
			if(m_Tracks[m_ActiveTrackIndex].GetNumTextIds() > 1)
			{
				char trackName[64];
				formatf(trackName, "TRACK%u", m_Tracks[m_ActiveTrackIndex].GetTextId(static_cast<u32>(windowStartTime)));
				strncpy(g_RadioDjContextName, trackName, g_MaxRadioNameLength);
			}
			else
			{
				bankName = (char *)m_Tracks[m_ActiveTrackIndex].GetBankName();
				if(bankName)
				{
					const char *trackName = (const char *)strrchr(bankName, '\\');
					if(trackName)
					{
						strncpy(g_RadioDjContextName, trackName + 1, g_MaxRadioNameLength);
					}
				}
			}
			break;

		case RADIO_DJ_SPEECH_CAT_TIME:
			if((gameHours >= (s32)g_RadioDjTimeMorningStart) &&
				(gameHours < (s32)g_RadioDjTimeMorningEnd))
			{
				strncpy(g_RadioDjContextName, g_RadioDjMorningContext, g_MaxRadioNameLength);
			}
			else if((gameHours >= (s32)g_RadioDjTimeAfternoonStart) &&
				(gameHours < (s32)g_RadioDjTimeAfternoonEnd))
			{
				strncpy(g_RadioDjContextName, g_RadioDjAfternoonContext, g_MaxRadioNameLength);
			}
			else if((gameHours >= (s32)g_RadioDjTimeEveningStart) &&
				(gameHours < (s32)g_RadioDjTimeEveningEnd))
			{
				strncpy(g_RadioDjContextName, g_RadioDjEveningContext, g_MaxRadioNameLength);
			}
			else if((gameHours >= (s32)g_RadioDjTimeNightStart) ||
				(gameHours < (s32)g_RadioDjTimeNightEnd))
			{
				strncpy(g_RadioDjContextName, g_RadioDjNightContext, g_MaxRadioNameLength);
			}
			break;

		case RADIO_DJ_SPEECH_CAT_TO:
			switch(nextTrackCategory)
			{
				case RADIO_TRACK_CAT_ADVERTS:
					strncpy(g_RadioDjContextName, g_RadioDjToAdvertContext, g_MaxRadioNameLength);
					break;

				case RADIO_TRACK_CAT_NEWS:
					strncpy(g_RadioDjContextName, g_RadioDjToNewsContext, g_MaxRadioNameLength);
					break;

				case RADIO_TRACK_CAT_WEATHER:
					strncpy(g_RadioDjContextName, g_RadioDjToWeatherContext, g_MaxRadioNameLength);
					break;

				default:
					break;
			}
			break;

		//case RADIO_DJ_SPEECH_CAT_GENERAL:
		default:
			strncpy(g_RadioDjContextName, categoryName, g_MaxRadioNameLength);
	}


	u32 variationNum;	
	u32 speechLengthMs = FindCompatibleDjSpeechVariation(g_RadioDjVoiceName, g_RadioDjContextName, -1, windowLengthMs, variationNum,
		preferredCategory, false);

	if(speechLengthMs > 0)
	{
		CreateDjSpeechSounds(variationNum);
	}
	else
	{
		switch(preferredCategory)
		{
			case RADIO_DJ_SPEECH_CAT_GENERAL:
				//No additional fall-back categories.
				break;

			//case RADIO_DJ_SPEECH_CAT_INTRO:
			//case RADIO_DJ_SPEECH_CAT_OUTRO:
			//case RADIO_DJ_SPEECH_CAT_TIME:
			//case RADIO_DJ_SPEECH_CAT_TO:
			default:
				if(!inTakeoverState)
				{
					//See if we have any general banter that is compatible as a fall-back.
					// Don't want to do this if in a takeover segment as the takeover generals haven't been written to
					// work as intros following a handover.
					speechLengthMs = CreateCompatibleDjSpeech(timeInMs, RADIO_DJ_SPEECH_CAT_GENERAL, windowLengthMs, windowStartTime);
				}
		}
	}

	return speechLengthMs;
}

void audRadioStation::PrepareIMDrivenDjSpeech(const u32 category, const u32 variationNum)
{
	// TODO: deal with playing?

	if(sm_DjSpeechMonoSound)
	{
		sm_DjSpeechMonoSound->StopAndForget();
	}
	if(sm_DjSpeechPositionedSound)
	{
		sm_DjSpeechPositionedSound->StopAndForget();
	}

	formatf(g_RadioDjVoiceName, "DJ_%s_%s", GetName(), g_RadioDjSpeechCategoryNames[category]);
	formatf(g_RadioDjContextName, "%s", g_RadioDjSpeechCategoryNames[category]);
	sm_IMDrivenDjSpeech = true;
	sm_IMDrivenDjSpeechScheduled = false;
	sm_IMDjSpeechStation = this;
	
	audDisplayf("IM Dj Speech: Preparing %s / %s variation %u", g_RadioDjVoiceName, g_RadioDjContextName, variationNum);

	CreateDjSpeechSounds(variationNum);

	sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_PREPARING;
}

void audRadioStation::TriggerIMDrivenDjSpeech(const u32 startTimeMs)
{
	sm_ActiveDjSpeechStartTimeMs = startTimeMs;
	sm_IMDrivenDjSpeechScheduled = true;
}

void audRadioStation::CancelIMDrivenDjSpeech()
{
	if(sm_DjSpeechMonoSound)
	{
		sm_DjSpeechMonoSound->StopAndForget();
	}
	if(sm_DjSpeechPositionedSound)
	{
		sm_DjSpeechPositionedSound->StopAndForget();
	}

	sm_DjSpeechState = RADIO_DJ_SPEECH_STATE_IDLE;

	sm_IMDrivenDjSpeechScheduled = false;
	sm_IMDrivenDjSpeech = false;	
}

bool audRadioStation::ChooseDjSpeechVariation(const u32 category, u32 &variationNum, u32 &lengthMs, const bool ignoreHistory)
{	
	char voiceName[g_MaxRadioNameLength];
	char contextName[g_MaxRadioNameLength];

	formatf(voiceName, "DJ_%s_%s", GetName(), g_RadioDjSpeechCategoryNames[category]);
	formatf(contextName, "%s", g_RadioDjSpeechCategoryNames[category]);

	lengthMs = FindCompatibleDjSpeechVariation(voiceName, contextName, 5000, 20000, variationNum, (u8)category, ignoreHistory);
	if(lengthMs)
	{		
		return true;
	}
	else
	{
		return false;
	}
}

void audRadioStation::CreateDjSpeechSounds(const u32 variationNum)
{
	audSoundInitParams initParams;
	initParams.TimerId = 2;
	g_RadioAudioEntity.CreateSound_PersistentReference(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("DjSpeechFrontend", 0x5924EF1)), 
															&sm_DjSpeechMonoSound, &initParams);
	if(sm_DjSpeechMonoSound)
	{
		//sm_DjSpeechMonoSound->SetPredelay(g_RadioDjDuckTimeMs / 2);
		if(((audSpeechSound *)sm_DjSpeechMonoSound)->InitSpeech(g_RadioDjVoiceName, g_RadioDjContextName, variationNum))
		{
			//Initiate the preparation of this speech.
			sm_DjSpeechMonoSound->Prepare(g_RadioDjSpeechWaveSlot, true);

			g_RadioAudioEntity.CreateSound_PersistentReference(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("DjSpeechPositioned", 0xD5115C83)),
															&sm_DjSpeechPositionedSound, &initParams);
			if(sm_DjSpeechPositionedSound)
			{
				//sm_DjSpeechPositionedSound->SetPredelay(g_RadioDjDuckTimeMs / 2);
				if(!((audSpeechSound *)sm_DjSpeechPositionedSound)->InitSpeech(g_RadioDjVoiceName, g_RadioDjContextName, variationNum))
				{
					sm_DjSpeechPositionedSound->StopAndForget();
					sm_DjSpeechMonoSound->StopAndForget();
				}
			}
			else
			{
				sm_DjSpeechMonoSound->StopAndForget();
			}
		}
		else
		{
			sm_DjSpeechMonoSound->StopAndForget();
		}
	}
}

u32 audRadioStation::FindCompatibleDjSpeechVariation(const char *voiceName, const char *contextName, s32 minLengthMs, s32 maxLengthMs, u32 &variationNum,
	u8 djSpeechCategory, const bool ignoreHistory) const
{
	u32 selectedLengthMs = 0;


	audDebugf1("DJSPEECH: FindCompatibleDjSpeechVariation voice name: %s, contextName %s, minLength %d, maxLength %d", voiceName, contextName, minLengthMs, maxLengthMs);

	u32 numVariations;
	u8 variationData[g_RadioDjMaxVariations];
	u32 variationUids[g_RadioDjMaxVariations];
	audSpeechSound::FindVariationInfoForVoiceAndContext(voiceName, contextName, numVariations, variationData, variationUids);

	if(numVariations > 0)
	{
		//See if any of the variations fit within our window.
		s32 numCompatibleVariations = 0;
		u32 compatibleVariationIndices[g_RadioDjMaxVariations];
		for(u32 i=0; i<numVariations; i++)
		{
			u32 speechLengthMs = (u32)(variationData[i]) * 100;
			if((s32)speechLengthMs < maxLengthMs && minLengthMs <= (s32)speechLengthMs)
			{
				//Check if this DJ speech is in the history.
				// - Make sure we don't look over too much history???
				u8 historyLength = g_MaxDjSpeechInHistory;
				bool isSpeechInHistory = false;

				if(!ignoreHistory)
				{
					for(u8 historyOffset=0; historyOffset<historyLength; historyOffset++)
					{
						u8 historyIndex = (m_DjSpeechHistoryWriteIndex + g_MaxDjSpeechInHistory - 1 - historyOffset) %
							g_MaxDjSpeechInHistory;
						if(m_DjSpeechHistory[historyIndex] == variationUids[i])
						{
							isSpeechInHistory = true;
							break;
						}
					}
				}

				if(!isSpeechInHistory)
				{
					// url:bugstar:7459269 - MOTOMAMI Los Santos - incorrect DJ line[Arca - KLK]
					// Prevent variation _01 from being selected
					if (i == 0 && m_NameHash == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC) && djSpeechCategory == RADIO_DJ_SPEECH_CAT_INTRO && atStringHash(contextName) == ATSTRINGHASH("KLK", 0xC2A78E3B))
					{
						audDebugf1("DJSPEECH: FindCompatibleDjSpeechVariation ignoring variation 1 for RADIO_DJ_SPEECH_CAT_INTRO context KLK");
						continue;
					}
					
					compatibleVariationIndices[numCompatibleVariations] = i;
					numCompatibleVariations++;
				}
			}
		}

		audDebugf1("DJSPEECH found %u variations, %u compatible", numVariations, numCompatibleVariations);

		if(numCompatibleVariations > 0)
		{
			//Choose a random variation from those that are compatible.
			// NOTE: currently using global random stream for DJ speech, rather than per-station stream
			s32 randomVariationIndex = audEngineUtil::GetRandomNumberInRange(0, numCompatibleVariations - 1);
			u32 variationIndex = compatibleVariationIndices[randomVariationIndex];
			//Variations are not zero-based, so add one.
			variationNum = variationIndex + 1;
			selectedLengthMs = (u32)(variationData[variationIndex]) * 100;

			sm_ActiveDjSpeechId = variationUids[variationIndex];
			sm_ActiveDjSpeechCategory = djSpeechCategory;

			audDebugf1("Chose variation %u for context %s (voice %s).  Length: %u (varData: %u)", variationNum, contextName, voiceName, selectedLengthMs, variationData[variationIndex]);
		}
	}

	return selectedLengthMs;
}

void audRadioStation::UpdateStereoEmitter(f32 volumeFactor, f32 cutoff)
{
	//Calculate the volume to be applied.
	f32 volumeLinear = Max<f32>(sm_VehicleRadioRiseVolumeCurve.CalculateValue(volumeFactor), g_SilenceVolumeLin);

	//Apply a smoothed duck when DJ speech is active.

#if __BANK
	//Recompute the linear ducking level and smoothing rate, as they might have changed via widgets.
	g_RadioDjDuckingLevelLin = audDriverUtil::ComputeLinearVolumeFromDb(g_RadioDjDuckingLevelDb);
	f32 increaseDecreaseRate = 1.0f / (f32)g_RadioDjDuckTimeMs;
	sm_DjDuckerVolumeSmoother.SetRate(increaseDecreaseRate);
#endif // __BANK

	f32 djDuckingOffsetLin = sm_DjDuckerVolumeSmoother.CalculateValue(isDjSpeaking() ? g_RadioDjDuckingLevelLin : 1.0f,
		g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2));

	// Store stereo emitter volume for player station
	if(this == g_RadioAudioEntity.GetPlayerRadioStation())
	{
		f32 djVolumeDb = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
		djVolumeDb += g_RadioAudioEntity.GetVolumeOffset();

		sm_DjSpeechFrontendVolume = djVolumeDb;
		sm_DjSpeechFrontendLPF = cutoff;
	}

	// don't apply ducking to Dj
	volumeLinear /= djDuckingOffsetLin;

	//Apply the volume.
	for(u8 trackIndex=0; trackIndex<2; trackIndex++)
	{
		f32 volumeDb = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear * GetMixTransitionVolumeLinear(trackIndex));
		volumeDb += g_RadioAudioEntity.GetVolumeOffset();
		m_Tracks[trackIndex].UpdateStereoEmitter(volumeDb,cutoff);
	}
}

f32 audRadioStation::GetMixTransitionVolumeLinear(u32 trackIndex) const
{
	if (m_IsPlayingMixTransition)
	{
		s32 playTime = m_Tracks[trackIndex].GetPlayTime();

		if (playTime < g_RadioTrackMixtransitionTimeMs)
		{
			return Max(0, (playTime - g_RadioTrackMixtransitionOffsetTimeMs)) / (f32)g_RadioTrackMixtransitionTimeMs;
		}
		else if (playTime >= m_Tracks[trackIndex].GetDuration() - g_RadioTrackMixtransitionTimeMs)
		{
			s32 timeRemaining = Max(0, (s32)m_Tracks[trackIndex].GetDuration() - g_RadioTrackMixtransitionOffsetTimeMs - playTime);
			return timeRemaining / (f32)g_RadioTrackMixtransitionTimeMs;
		}
	}

	return 1.f;
}

#if RSG_ASSERT
void audRadioStation::ValidateEnvironmentMetric(const audEnvironmentGameMetric *occlusionMetric) const
{
	char extraInfo[128];
	formatf(extraInfo, "metric: %p", occlusionMetric);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetReverbLarge(), "ReverbLarge", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetReverbMedium(), "ReverbMedium", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetReverbSmall(), "ReverbSmall", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetDryLevel(), "DryLevel", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetLeakageFactor(), "LeakageFactor", extraInfo);
	naAssertf(FPIsFinite(occlusionMetric->GetRolloffFactor()), "Non-finite RolloffFactor, %s", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetDistanceAttenuationDamping(), "DistanceAttenuationDamping", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetOcclusionAttenuationLin(), "OcclusionAttenuationLin", extraInfo);
	ValidateEnvironmentMetricInRange(occlusionMetric->GetOcclusionFilterCutoffLin(), "OcclusionFilterCutoffLin", extraInfo);
	if(occlusionMetric->GetUseOcclusionPath())
	{
		naAssertf(FPIsFinite(occlusionMetric->GetOcclusionPathDistance()), "Non-finite occlusion path distance, %s", extraInfo);
		naAssertf(FPIsFinite(MagSquared(occlusionMetric->GetOcclusionPathPosition()).Getf()), "Non-finite occlusion path position, %s [%f,%f,%f]", extraInfo, occlusionMetric->GetOcclusionPathPosition().GetX().Getf(), occlusionMetric->GetOcclusionPathPosition().GetY().Getf(), occlusionMetric->GetOcclusionPathPosition().GetZ().Getf());
	}
	
}

void audRadioStation::ValidateEnvironmentMetricInRange(const float val, const char *valName, const char *extraInfo) const
{
	naAssertf(val >= 0.f && val <= 1.f, "Invalid %s value %f.  %s", valName, val, extraInfo);
}

#endif // RSG_ASSERT

void audRadioStation::UpdatePositionedEmitter(const u32 emitterIndex, const f32 emittedVolume, const f32 volumeFactor, 
											  const Vector3 &position, const u32 LPFCutoff, const u32 HPFCutoff, const audEnvironmentGameMetric *occlusionMetric, const u32 emitterType, const f32 environmentalLoudness, bool isPlayerVehicleRadioEmitter)
{
	ASSERT_ONLY(ValidateEnvironmentMetric(occlusionMetric));

	//Calculate the volume to be applied.
	f32 volumeLinear = Max<f32>(sm_VehicleRadioRiseVolumeCurve.CalculateValue(volumeFactor), g_SilenceVolumeLin);

	//Apply a smoothed duck when DJ speech is active.

#if __BANK
	//Recompute the linear ducking level and smoothing rate, as they might have changed via widgets.
	g_RadioDjDuckingLevelLin = audDriverUtil::ComputeLinearVolumeFromDb(g_RadioDjDuckingLevelDb);
	f32 increaseDecreaseRate = 1.0f / (f32)g_RadioDjDuckTimeMs;
	sm_DjDuckerVolumeSmoother.SetRate(increaseDecreaseRate);
#endif // __BANK

	if(this == g_RadioAudioEntity.GetPlayerRadioStation() && g_RadioAudioEntity.GetLastPlayerVehicle())
	{
		// only duck for player station emitters
		f32 djDuckingOffsetLin = sm_DjDuckerVolumeSmoother.CalculateValue(isDjSpeaking() ? g_RadioDjDuckingLevelLin : 1.0f,
		g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2));
		volumeLinear /= djDuckingOffsetLin;
	}

	if(isPlayerVehicleRadioEmitter)
	{
		sm_PositionedPlayerVehicleRadioLPFCutoff = LPFCutoff;
		sm_PositionedPlayerVehicleRadioHPFCutoff = HPFCutoff;
		sm_PositionedPlayerVehicleRadioEnvironmentalLoudness = environmentalLoudness;
		sm_PositionedPlayerVehicleRadioVolume = emittedVolume;
		sm_PositionedPlayerVehicleRadioRollOff = occlusionMetric->GetRolloffFactor();
	}	

	//Apply the volume, cutoff and position to the emitter.
	for(u8 trackIndex=0; trackIndex<2; trackIndex++)
	{
		f32 volumeOffsetDb = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear * GetMixTransitionVolumeLinear(trackIndex));
		volumeOffsetDb += g_RadioAudioEntity.GetVolumeOffset();
		m_Tracks[trackIndex].UpdatePositionedEmitter(emitterIndex, emittedVolume, volumeOffsetDb, LPFCutoff, HPFCutoff, position, occlusionMetric, emitterType, environmentalLoudness);
	}
}

void audRadioStation::MuteEmitters()
{
	for(u8 trackIndex=0; trackIndex<2; trackIndex++)
	{
		m_Tracks[trackIndex].MuteEmitters();
	}
}

const RadioStationTrackList::tTrack *audRadioStation::GetTrack(const s32 category, const u32 context, const s32 trackIndex, bool ignoreLocking) const
{
	const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(category);
#if RSG_PC
	if(category == RADIO_TRACK_CAT_MUSIC && m_IsUserStation)
	{
		return trackLists->GetTrack(0);
	}
#endif
	if(!trackLists)
	{
		return NULL;
	}
	return trackLists->GetTrack(trackIndex, context, ignoreLocking);
}

void audRadioStation::AddActiveTrackToHistory()
{
	bool disableNetworkModeForStation = m_UseRandomizedStrideSelection;
	WIN32PC_ONLY(disableNetworkModeForStation = m_IsUserStation);

	if(!disableNetworkModeForStation && (IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get()))
	{
		return;
	}

	if(m_Tracks[m_ActiveTrackIndex].IsInitialised())
	{	
		const s32 category = m_Tracks[m_ActiveTrackIndex].GetCategory();
		
		// only on player station...
		if(category == RADIO_TRACK_CAT_NEWS)
		{
			if(g_RadioAudioEntity.IsPlayerRadioActive() && 
				g_RadioAudioEntity.GetPlayerRadioStation() == this)
			{
				const u32 newsStoryId = m_Tracks[m_ActiveTrackIndex].GetTrackIndex();
				naAssertf(newsStoryId < sm_NewsTrackLists.ComputeNumTracks(), "Track index is out of bounds");	
				if(sm_NewsStoryState[newsStoryId] == (RADIO_NEWS_ONCE_ONLY|RADIO_NEWS_UNLOCKED))
				{
					naDisplayf("Locking news story %u", newsStoryId+1);
					sm_NewsStoryState[newsStoryId] = RADIO_NEWS_ONCE_ONLY;
				}	

				// Generic news plays sequentially, so update the last played index
				if((sm_NewsStoryState[newsStoryId] & RADIO_NEWS_ONCE_ONLY) == 0 && sm_NewsHistory[0] != static_cast<u32>(m_Tracks[m_ActiveTrackIndex].GetTrackIndex()))
				{				
					audDisplayf("Generic news story - history index was %d, now %d", sm_NewsHistory[0] + 1, newsStoryId + 1);
					sm_NewsHistory[0] = m_Tracks[m_ActiveTrackIndex].GetTrackIndex();
				}
			}
			// News doesn't rely on normal history tracking - if the player isn't explicitly listening to this track then we don't want to lock it
			return;
		}
		
		if(m_UseRandomizedStrideSelection || (category == RADIO_TRACK_CAT_MUSIC && m_PlaySequentialMusicTracks))
		{
			// we update the sequential index when we pick a track
			return;
		}
		else
		{		
			audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(category));
			if(history)
			{
				u32 trackId = m_Tracks[m_ActiveTrackIndex].GetRefForHistory();
#if RSG_PC // user music - this was tracked differently in GTA IV this may need fixed SORR
				if(m_IsUserStation && category == RADIO_TRACK_CAT_MUSIC)
				{
					trackId = m_Tracks[m_ActiveTrackIndex].GetTrackIndex();
				}
#endif
				//Make sure we didn't already add this track.
				if(history->GetPreviousEntry() != trackId)
				{
					history->AddToHistory(trackId);
				}
			}
		}
	}
}

const audRadioStationHistory *audRadioStation::FindHistoryForCategory(const s32 category) const
{
#if RSG_PC
	if(m_IsUserStation && category == RADIO_TRACK_CAT_MUSIC)
	{
		return sm_UserRadioTrackManager.GetHistory();
	}
#endif
	// We use a single history per station for network synched radio
	if(!m_UseRandomizedStrideSelection && (PARAM_disableradiohistory.Get() || IsNetworkModeHistoryActive()))
	{
		return &m_MusicTrackHistory;
	}

	switch(category)
	{
	case RADIO_TRACK_CAT_WEATHER:
		return &sm_WeatherHistory;
	case RADIO_TRACK_CAT_ADVERTS:
		return &sm_AdvertHistory;
	case RADIO_TRACK_CAT_MUSIC:
		return &m_MusicTrackHistory;
	case RADIO_TRACK_CAT_TAKEOVER_MUSIC:
		// NOTE: We dont support network sync for Takeover music in Sequential mode; this is purely to allow us to capture
		// the station in sequential mode, and isnt expected to be used in final.
		if(m_PlaySequentialMusicTracks)
		{
			return &m_TakeoverMusicTrackHistory;
		}
		else if (m_HasTakeoverContent || m_UseRandomizedStrideSelection)
		{
			return &m_TakeoverMusicTrackHistory;
		}
		else
		{
			return &m_MusicTrackHistory;
		}
	case RADIO_TRACK_CAT_IDENTS:
		return &m_IdentTrackHistory;
	case RADIO_TRACK_CAT_TAKEOVER_IDENTS:
		return &m_TakeoverIdentTrackHistory;
	case RADIO_TRACK_CAT_DJSOLO:
		return &m_DjSoloTrackHistory;
	case RADIO_TRACK_CAT_TAKEOVER_DJSOLO:
		return &m_TakeoverDjSoloTrackHistory;
	case RADIO_TRACK_CAT_TO_AD:
		return &m_ToAdTrackHistory;
	case RADIO_TRACK_CAT_TO_NEWS:
		return &m_ToNewsTrackHistory;
	case RADIO_TRACK_CAT_USER_INTRO:
		return &m_UserIntroTrackHistory;
	case RADIO_TRACK_CAT_USER_OUTRO:
		return &m_UserOutroTrackHistory;
	case RADIO_TRACK_CAT_INTRO_MORNING:
		return &m_UserIntroMorningTrackHistory;
	case RADIO_TRACK_CAT_INTRO_EVENING:
		return &m_UserIntroEveningTrackHistory;
	case RADIO_TRACK_CAT_NEWS:
	default:
		audAssertf(false, "Invalid track category %u", category);
		return NULL;
	}
}

bool audRadioStation::IsTrackInHistory(const u32 trackId, const s32 category, const s32 historyLength) const
{
	const audRadioStationHistory *history = FindHistoryForCategory(category);
	if(history)
	{
		const bool ret = history->IsInHistory(trackId, historyLength);
#if __BANK
		if(g_DebugRadioRandom && ret)
		{
			naDebugf1("%s - skipping track id %08X (%s) due to history", GetName(), trackId, TrackCats_ToString((TrackCats)category));
		}
#endif
		return ret;
	}
	return false;
}

bool audRadioStation::IsLocked() const 
{
	bool stationLocked = AUD_GET_TRISTATE_VALUE(m_StationSettings->Flags, FLAG_ID_RADIOSTATIONSETTINGS_LOCKED) == AUD_TRISTATE_TRUE;	
	return stationLocked;
}

s32 audRadioStation::GetRandomNumberInRange(const s32 min, const s32 max)
{
	BANK_ONLY(u32 seed = m_Random.GetSeed());
	
	s32 ret = m_Random.GetRanged(min, max);
	
#if __BANK 
	if(g_DebugRadioRandom)
		naDebugf1("%s - GetRandomNumberInRange(%d, %d) - %d (seed was %u)", GetName(), min, max, ret, seed);
#endif
	return ret;
}

float audRadioStation::GetRandomNumberInRange(const float min, const float max)
{
	BANK_ONLY(u32 seed = m_Random.GetSeed());
	float ret = m_Random.GetRanged(min, max);
#if __BANK 
	if(g_DebugRadioRandom)
		naDebugf1("%s - GetRandomNumberInRange(%f, %f) - %f (seed was %u)", GetName(), min, max, ret, seed);
#endif
	return ret;
}

bool audRadioStation::ResolveProbability(const float Probability)
{
	if (Probability >= 1.0f)
	{
#if __BANK
		if(g_DebugRadioRandom)
			naDebugf1("%s - ResolveProbability(%f) - true", GetName(), Probability);
#endif
		return true;
	}

	float r = GetRandomNumberInRange(0.0f, 1.0f);
#if __BANK
	if(g_DebugRadioRandom)
		naDebugf1("%s - ResolveProbability(%f) - %f (%s)", GetName(), Probability, r, r < Probability ? "true" : "false");
#endif
	return (r < Probability);
}

bool audRadioStation::IsValidCategoryForStation(const TrackCats category, const bool playNews) const
{
	switch(category)
	{
	case RADIO_TRACK_CAT_NEWS:
		return playNews;
	case RADIO_TRACK_CAT_WEATHER:
		return m_PlayWeather;
		
	case RADIO_TRACK_CAT_INTRO_MORNING:
	case RADIO_TRACK_CAT_INTRO_EVENING:
#if RSG_PC
		if(m_IsUserStation && m_MusicRunningCount > 1 && sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_RADIO)
		{			
			const s32 gameHours = CClock::GetHour();
			if(category == RADIO_TRACK_CAT_INTRO_MORNING)
			{
				return (gameHours >= (s32)g_RadioDjTimeMorningStart && gameHours < (s32)g_RadioDjTimeMorningEnd);	
			}
			else
			{
				// Evening
				return (gameHours >= (s32)g_RadioDjTimeEveningStart && gameHours < (s32)g_RadioDjTimeEveningEnd);
			}				
		}
#endif
		return false;

	case RADIO_TRACK_CAT_USER_INTRO:
#if RSG_PC
		return m_IsUserStation && m_MusicRunningCount > 1 && sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_RADIO;
#else
		return false;
#endif // RSG_PC
	case RADIO_TRACK_CAT_TO_AD:
	case RADIO_TRACK_CAT_TO_NEWS:
#if RSG_PC
		return m_IsUserStation && m_MusicRunningCount > 1 && sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_RADIO;
#else
		return false;
#endif // RSG_PC

		case RADIO_TRACK_CAT_USER_OUTRO:
#if RSG_PC
			// avoid outro after only one song.
			return m_IsUserStation && m_MusicRunningCount > 1 && sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_RADIO;
#else
			return false;
#endif
	case RADIO_TRACK_CAT_MUSIC:
#if RSG_PC
		// prevent runs longer than 3 songs
		// TODO: factor in time
		if(m_IsUserStation && sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_RADIO && m_MusicRunningCount >= 3)
		{
			return false;
		}
#endif
		return true;
	case RADIO_TRACK_CAT_DJSOLO:
	case RADIO_TRACK_CAT_IDENTS:
#if RSG_PC
		if(m_IsUserStation)
		{
			return sm_UserRadioTrackManager.GetRadioMode() == USERRADIO_PLAY_MODE_RADIO && m_MusicRunningCount > 1;
		}
#endif
		return true;

	case RADIO_TRACK_CAT_TAKEOVER_DJSOLO:
	case RADIO_TRACK_CAT_TAKEOVER_IDENTS:
	case RADIO_TRACK_CAT_TAKEOVER_MUSIC:
		return m_HasTakeoverContent;
	default:
		return true;
	}	
}

u8 audRadioStation::ComputeNextTrackCategoryForTakeoverStation(u32 timeInMs, u32 activeTrackCategory)
{
	audDisplayf("Takeover station calculating next category at %ums, active track category %u, seed %u, takeover time %u, music count %u", timeInMs, activeTrackCategory, m_Random.GetSeed(), m_TakeoverLastTimeMs, m_MusicRunningCount);

	u8 nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
	switch(activeTrackCategory)
	{
	case RADIO_TRACK_CAT_MUSIC:
		// Should we go into a takeover state?
		if(timeInMs >= m_TakeoverLastTimeMs + m_TakeoverMinTimeMs && ResolveProbability(m_TakeoverProbability))
		{
			nextTrackCategory = RADIO_TRACK_CAT_TAKEOVER_IDENTS;
		}
		else
		{
			// check  music running counter
			if(m_MusicRunningCount > 2 || (m_MusicRunningCount>1&&ResolveProbability(0.35f)))
			{
				nextTrackCategory = RADIO_TRACK_CAT_IDENTS;
			}
			else
			{
				nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
			}
		}
		break;

	case RADIO_TRACK_CAT_IDENTS:
		// DJSOLOs are only going to follow IDENTs on this station
		nextTrackCategory = (u8)(ResolveProbability(0.75f) ? RADIO_TRACK_CAT_DJSOLO : RADIO_TRACK_CAT_MUSIC);
		break;

	case RADIO_TRACK_CAT_TAKEOVER_IDENTS:
		if(ResolveProbability(m_TakeoverDjSoloProbability))
		{
			nextTrackCategory = RADIO_TRACK_CAT_TAKEOVER_DJSOLO;
		}
		else
		{
			nextTrackCategory = RADIO_TRACK_CAT_TAKEOVER_MUSIC;
		}
		break;

	case RADIO_TRACK_CAT_TAKEOVER_DJSOLO:
		nextTrackCategory = RADIO_TRACK_CAT_TAKEOVER_MUSIC;
		break;

	case RADIO_TRACK_CAT_TAKEOVER_MUSIC:
		if(m_TakeoverMusicTrackCounter == 0)
		{
			// End the takeover				
			nextTrackCategory = RADIO_TRACK_CAT_IDENTS;
		}
		else
		{
			nextTrackCategory = RADIO_TRACK_CAT_TAKEOVER_MUSIC;				
		}
		break;

		//case g_NullRadioTrackCategory: - Start on music.
	default:
		//Transition to music only.
		nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
		break;

	}

	audDisplayf("Takeover station calculated next category as %s", TrackCats_ToString((TrackCats)nextTrackCategory));
	return nextTrackCategory;
}

bool audRadioStation::PlayIdentsInsteadOfAds() const
{
	// Special case, we want to play idents instead of adverts on West Coast Classics during the launch event. Safer to link this to the actual track list collection 
    // being unlocked rather than the tuneable so that we can ensure we stay in sync with whatever script is doing in terms of maintaining the station's SP/MP state
	if(m_NameHash == ATSTRINGHASH("RADIO_09_HIPHOP_OLD", 0x572C04) && IsTrackListUnlocked(ATSTRINGHASH("RADIO_09_HIPHOP_OLD_DD_MUSIC_LAUNCH", 0x2018C9AF)))
	{
		return true;
	}

	return m_PlayIdentsInsteadOfAds;
}

u8 audRadioStation::ComputeNextTrackCategory(u32 timeInMs)
{
	u8 nextTrackCategory = 0;
	const RadioTrackCategoryData *weights;
	f32 weightSum = 0.0f;
	f32 r = 0.0f;

	if(m_ForceNextTrackCategory != 0xff)
	{
		const u8 ret = m_ForceNextTrackCategory;
		m_ForceNextTrackCategory = 0xff;
		return ret;
	}

#if RSG_PC
	if(m_BadTrackCount > 4)
	{
		Warningf("Forcing an advert as bad track count is > 4");
		m_ShouldPlayFullRadio = true;
		sm_IsInRecoveryMode = true;
		return RADIO_TRACK_CAT_ADVERTS;
	}
	if(sm_IsInRecoveryMode)
	{
		// keep trying to play music until we are either successful or bad track count hits
		// the limit
		return RADIO_TRACK_CAT_MUSIC;
	}
#endif

	u32 activeTrackCategory = m_Tracks[m_ActiveTrackIndex].GetCategory();

	if(m_OnlyPlayAds)
	{
		//@@: location AUDRADIOSTATION_COMPUTENEXTTRACKCATEGORY_RETURN_ADVERTISEMENTS
		return RADIO_TRACK_CAT_ADVERTS;
	}

	if(ShouldOnlyPlayMusic())
	{
		return RADIO_TRACK_CAT_MUSIC;
	}

	// see if there are any unlocked mission stories; no back to back news however
	// only for player station
	bool hasUnlockedNews = false;
	if(!IsNetworkModeHistoryActive())
	{
		for(u32 i = 0; i < kMaxNewsStories; i++)
		{
			if(sm_NewsStoryState[i] == (RADIO_NEWS_UNLOCKED|RADIO_NEWS_ONCE_ONLY))
			{
				hasUnlockedNews = true;
				break;
			}
		}
	}

	// only play news on the player station
	const bool playNews = m_PlayNews && !IsNetworkModeHistoryActive() && !PARAM_disableradiohistory.Get() &&
						g_RadioAudioEntity.IsPlayerRadioActive() && g_RadioAudioEntity.GetPlayerRadioStation() == this;
	
	if(playNews && activeTrackCategory != RADIO_TRACK_CAT_NEWS && 
		hasUnlockedNews &&
		timeInMs >= GetNextValidSelectionTime(RADIO_TRACK_CAT_NEWS))
	{
		m_HasJustPlayedBackToBackMusic = false;
		m_HasJustPlayedBackToBackAds = false;
#if RSG_PC
		if(m_IsUserStation && activeTrackCategory != RADIO_TRACK_CAT_TO_NEWS)
		{
			return RADIO_TRACK_CAT_TO_NEWS;
		}
#endif
		return RADIO_TRACK_CAT_NEWS;
	}

#if RSG_PC
	if(m_IsUserStation && !HasUserTracks() && activeTrackCategory == RADIO_TRACK_CAT_IDENTS)
	{
		// act like we have just played music so we can transition to anything
		activeTrackCategory = RADIO_TRACK_CAT_MUSIC;
	}
#endif

	if(m_HasTakeoverContent)
	{
		nextTrackCategory = ComputeNextTrackCategoryForTakeoverStation(m_StationAccumulatedPlayTimeMs, activeTrackCategory);
	}
	else
	{	
		switch(activeTrackCategory)
		{
			case RADIO_TRACK_CAT_MUSIC:

				
				//Transition to any category.
				if(m_NoBackToBackMusic && activeTrackCategory == RADIO_TRACK_CAT_MUSIC)
				{
					// we just played a music track so don't want to allow another music track
					weights = sm_RadioTrackCategoryWeightsNoBackToBackMusic;
				}
				else if(m_HasJustPlayedBackToBackMusic)
				{
					//We already played back-to-back music, so prevent another music track (or ident->music) being chosen.
					weights = m_NameHash == ATSTRINGHASH("RADIO_34_DLC_HEI4_KULT", 0xE3442163) ? sm_RadioTrackCategoryWeightsFromBackToBackMusicKult : sm_RadioTrackCategoryWeightsFromBackToBackMusic;
				}
				else
				{
					weights = m_NameHash == ATSTRINGHASH("RADIO_34_DLC_HEI4_KULT", 0xE3442163) ? sm_RadioTrackCategoryWeightsNoBackToBackMusicKult : sm_RadioTrackCategoryWeightsFromMusic;
				}

#if RSG_PC
				if(m_IsUserStation)
				{
					weights = sm_UserMusicWeightsFromMusic;
					// Default to music if we've temporarily exhausted all other valid categories.
					nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
				}
#endif
				//Compute sum of weights; only for valid track cats.
				{
#if RSG_PC
					const u32 lastCat = m_IsUserStation ? RADIO_TRACK_CAT_INTRO_EVENING : RADIO_TRACK_CAT_DJSOLO;
#else
					const u32 lastCat = RADIO_TRACK_CAT_DJSOLO;
#endif
					bool validTimeForCategory = true;
					for(u32 i = 0; i <= lastCat; i++) // TODO: re-enable new categories after audio sync
					{
						if(weights)
						{
#if RSG_PC
							if(m_IsUserStation)
							{
								validTimeForCategory = timeInMs >= GetNextValidSelectionTime(weights->Category[i].Category);
							}
#endif
							if(IsValidCategoryForStation((TrackCats)weights->Category[i].Category, playNews) && validTimeForCategory)
							{
								f32 modifiedWeight = (float)weights->Category[i].Value;
								weightSum += modifiedWeight;
							}
						}
					}

					//Compute random r in range [0.0f, weightSum].
					r = GetRandomNumberInRange(0.0f, weightSum);

					//Find where r falls in weight range to pick category.
					for(u32 i = 0; i <= lastCat; i++) // TODO: re-enable new categories after audio sync
					{
						if(weights)
						{
#if RSG_PC
							if(m_IsUserStation)
							{
								validTimeForCategory = timeInMs >= GetNextValidSelectionTime(weights->Category[i].Category);
							}
#endif
							if(IsValidCategoryForStation((TrackCats)weights->Category[i].Category, playNews) && validTimeForCategory)
							{

								f32 modifiedWeight = (float)weights->Category[i].Value;
								r -= modifiedWeight;

								if((weights->Category[i].Value > 0.0f) && (r <= 0.0f))
								{
									nextTrackCategory = weights->Category[i].Category;
									break;
								}
							}
						}
					}
				}
				

			
				// only choose news if we don't have any unlocked mission stories waiting to go (since we only want to play them on
				// the player station; picking news here will mean we don't play another news for a few mins even if this plays virtually)
				if(nextTrackCategory == RADIO_TRACK_CAT_NEWS && hasUnlockedNews)
				{
					RADIODEBUG("Playing an advert instead of news since not player station and there is a pending mission news story");
					nextTrackCategory = RADIO_TRACK_CAT_ADVERTS;
				}
				else
				{
					//Ensure that we don't hear news and weather too often on this station.
					if((nextTrackCategory == RADIO_TRACK_CAT_NEWS) || 
						(nextTrackCategory == RADIO_TRACK_CAT_WEATHER))
					{
						if(timeInMs < GetNextValidSelectionTime(nextTrackCategory))
						{
							//We heard this category too recently, so play something else instead.
	#if RSG_PC
							if(m_IsUserStation)
							{
								BANK_ONLY(audDisplayf("%s - falling back to music rather than %s due to selection time", GetName(), GetTrackCategoryName(nextTrackCategory)));
								nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
							}
							else
	#endif
								nextTrackCategory = RADIO_TRACK_CAT_ADVERTS;
						}
					}
				}

				break;
			case RADIO_TRACK_CAT_INTRO_MORNING:
			case RADIO_TRACK_CAT_INTRO_EVENING:
			case RADIO_TRACK_CAT_USER_INTRO:
				nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
				break;
			case RADIO_TRACK_CAT_TO_AD:
				nextTrackCategory = RADIO_TRACK_CAT_ADVERTS;
				break;
			case RADIO_TRACK_CAT_TO_NEWS:
				nextTrackCategory = RADIO_TRACK_CAT_NEWS;
				break;
			case RADIO_TRACK_CAT_USER_OUTRO:
				nextTrackCategory = RADIO_TRACK_CAT_IDENTS;			
				break;
			case RADIO_TRACK_CAT_IDENTS:
				//Transition to music only.
				nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
				break;

			case RADIO_TRACK_CAT_ADVERTS:
				// No back-to-back adverts in the network game to maximise history usage for songs
				if(!IsNetworkModeHistoryActive() && m_PlaysBackToBackAds && !m_HasJustPlayedBackToBackAds && ResolveProbability(0.25f))
				{
					nextTrackCategory = RADIO_TRACK_CAT_ADVERTS;
				}
				else
				{
					nextTrackCategory = RADIO_TRACK_CAT_IDENTS;
				}
				break;
			case RADIO_TRACK_CAT_NEWS:
			case RADIO_TRACK_CAT_WEATHER:
				nextTrackCategory = RADIO_TRACK_CAT_IDENTS;
				break;

			//case g_NullRadioTrackCategory: - Start on music.
			default:
				//Transition to music only.
				nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
	#if RSG_PC // user music
				if(m_IsUserStation && activeTrackCategory != RADIO_TRACK_CAT_DJSOLO && ResolveProbability(0.5f))
				{
					nextTrackCategory = RADIO_TRACK_CAT_USER_INTRO;
				}
	#endif
		}
	}

	if(nextTrackCategory == RADIO_TRACK_CAT_ADVERTS && PlayIdentsInsteadOfAds())
	{
		nextTrackCategory = RADIO_TRACK_CAT_IDENTS;
	}

	if(m_NameHash == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC))
	{
		if(m_MusicRunningCount < 2)
		{
			nextTrackCategory = RADIO_TRACK_CAT_MUSIC;
		}
		else if(nextTrackCategory == RADIO_TRACK_CAT_DJSOLO)
		{
			if(ResolveProbability(0.2f))
			{
				nextTrackCategory = RADIO_TRACK_CAT_IDENTS;
			}
		}		
	}

	m_HasJustPlayedBackToBackMusic = (activeTrackCategory == RADIO_TRACK_CAT_MUSIC) && (nextTrackCategory == RADIO_TRACK_CAT_MUSIC);
	m_HasJustPlayedBackToBackAds = (activeTrackCategory == RADIO_TRACK_CAT_ADVERTS && nextTrackCategory == RADIO_TRACK_CAT_ADVERTS);

	// Update this along with m_TakeoverMusicTrackCounter for takeover stations
	if(!m_HasTakeoverContent)
	{
		if(nextTrackCategory == RADIO_TRACK_CAT_MUSIC)
		{
			m_MusicRunningCount++;
		}
		else
		{
			m_MusicRunningCount = 0;
		}
	}

	return nextTrackCategory;
}

const RadioStationTrackList::tTrack *audRadioStation::ComputeRandomTrack(s32 &category, s32 &index)
{
	naCErrorf(category < NUM_RADIO_TRACK_CATS, "Category for random track (%d) is invalid", category);

	if(/* m_ForceNextTrackContext != kNullContext && */ m_ForceNextTrackIndex != 0xffff)
	{
		naDisplayf("Forcing track index %u context %u category %d", m_ForceNextTrackIndex, m_ForceNextTrackContext, category);
		const RadioStationTrackList::tTrack *ret = GetTrack(category, m_ForceNextTrackContext, m_ForceNextTrackIndex, true);
		Assign(index, m_ForceNextTrackIndex);
		m_ForceNextTrackIndex = 0xffff;
		m_ForceNextTrackContext = kNullContext;
		
		return ret;
	}

	u32 requiredContext = kNullContext;
	if(m_ForceNextTrackContext != kNullContext)
	{
		requiredContext = m_ForceNextTrackContext;
		m_ForceNextTrackContext = kNullContext;
	}
	else
	{
		//Deal with custom News and Weather contexts.
		const bool isNightTime = (CClock::GetHour() > 18 || CClock::GetHour() < 6);
		const CWeatherType& weatherType = (g_weather.GetInterp() > 0.5f ? g_weather.GetNextType() : g_weather.GetPrevType());
		if(category == RADIO_TRACK_CAT_WEATHER)
		{
            //@@: location AUDRADIOSTATION_COMPUTERANDOMTRACK_ON_WEATHER_TRACK
			if(weatherType.m_fog>0.5f)
			{
				requiredContext = g_RadioTrackWeatherContextNames[RADIO_TRACK_WEATHER_CONTEXT_FOG];
			}
			else if(weatherType.m_rain>0.0f)
			{
				requiredContext = g_RadioTrackWeatherContextNames[RADIO_TRACK_WEATHER_CONTEXT_RAIN];
			}
			else if(!isNightTime && (weatherType.m_sun>0.6f))
			{
				requiredContext = g_RadioTrackWeatherContextNames[RADIO_TRACK_WEATHER_CONTEXT_SUN];
			}
			else if(weatherType.m_lightning)
			{
				requiredContext = g_RadioTrackWeatherContextNames[RADIO_TRACK_WEATHER_CONTEXT_WIND];
			}
			// inhibit cloudy reports at night
			else if(!isNightTime && weatherType.m_cloud>0.7f)
			{
				requiredContext = g_RadioTrackWeatherContextNames[RADIO_TRACK_WEATHER_CONTEXT_CLOUD];
			}
			else
			{
				//We don't support the current weather type, so play an advert instead.
				RADIODEBUG("Current weather type unsupported; playing advert");
				category = RADIO_TRACK_CAT_ADVERTS;
			}
		}
	}
	
	u32 numTracks = 0;
	const bool isNetworkGameInProgress = IsNetworkModeHistoryActive();
	// Don't play mission news in the network game
	if(category == RADIO_TRACK_CAT_NEWS)
	{
		numTracks = 0;

		if(!isNetworkGameInProgress && !PARAM_disableradiohistory.Get())
		{			
			// see if there are any unlocked mission stories
			u32 firstGenericStory = ~0U;
			u32 lastPlayedGenericNewsStory = sm_NewsHistory[0];
			u32 nextGenericNewsStory = ~0U;
			for(s32 i = 0; i < kMaxNewsStories; i++)
			{
				if(sm_NewsStoryState[i] & RADIO_NEWS_UNLOCKED && (sm_NewsStoryState[i]&RADIO_NEWS_ONCE_ONLY) == 0)
				{
					// Find the earliest unlocked generic story
					u32 storyId = (u32)i;
					if(storyId < firstGenericStory)
					{
						firstGenericStory = storyId;
					}
					// Find the first unlocked generic story after the last one played
					if(storyId > lastPlayedGenericNewsStory && nextGenericNewsStory == ~0U)
					{
						nextGenericNewsStory = storyId;
					}
				}
				if(sm_NewsStoryState[i] == (RADIO_NEWS_UNLOCKED|RADIO_NEWS_ONCE_ONLY))
				{
					// relock this story only when played phys on the player station
					index = i;
					return GetTrack(category, kNullContext, i);
				}
			}

			const u32 numNewsStoriesAvailable = sm_NewsTrackLists.ComputeNumTracks();
			if(nextGenericNewsStory >= numNewsStoriesAvailable)
			{
				nextGenericNewsStory = firstGenericStory;
			}
			if(nextGenericNewsStory < numNewsStoriesAvailable)
			{
				index = (s32)nextGenericNewsStory;
				return GetTrack(category, kNullContext, index);
			}
		}// !isNetworkGameInProgress

		// else - fall back to something else below
	}
	else
	{
		numTracks = ComputeNumTracksAvailable(category, requiredContext);
	}
	

	//audRadioStationTrackInfo &trackInfo = m_TrackInfoForCategory[*category];
	if(numTracks == 0)
	{
		//We don't have any tracks for this category/context...
		switch(category)
		{
			case RADIO_TRACK_CAT_IDENTS:
				RADIODEBUG("Failed to get ident; falling back to music");
				category = RADIO_TRACK_CAT_MUSIC;
				numTracks = ComputeNumTracksAvailable(category);
				break;
			case RADIO_TRACK_CAT_NEWS:
			case RADIO_TRACK_CAT_WEATHER:	//			"
			case RADIO_TRACK_CAT_DJSOLO:
				//Try and play an advert instead.
				RADIODEBUG2("Wanted %s, falling back to advert", TrackCats_ToString((TrackCats)category));

				category = RADIO_TRACK_CAT_ADVERTS;
				numTracks = ComputeNumTracksAvailable(category);
				
				if(numTracks == 0)
				{
					//We should always have music as a fallback.
					RADIODEBUG("Failed to get advert; falling back to music");
					category = RADIO_TRACK_CAT_MUSIC;
					numTracks = ComputeNumTracksAvailable(category);
				}
				break;

			case RADIO_TRACK_CAT_MUSIC:
			#if RSG_PC // user music
				if(m_IsUserStation)
				{
					// Fall back to advert
					category = RADIO_TRACK_CAT_ADVERTS;
					numTracks = ComputeNumTracksAvailable(category);
				}
				else if(!IsUSBMixStation())
				{
						naAssertf(numTracks > 0, "No music tracks for radio station %s", GetName());
				}
			#endif
				return NULL;

			//case RADIO_TRACK_CAT_ADVERTS:
			default:
				//We should always have music as a fallback.
				category = RADIO_TRACK_CAT_MUSIC;
				numTracks = ComputeNumTracksAvailable(category);
		}

		requiredContext = kNullContext;
	}

	naCErrorf(IsUSBMixStation() || numTracks > 0, "No available tracks for category %d in ComputeRandomTrack", category);
	const RadioStationTrackList::tTrack *track = NULL;
	if((category == RADIO_TRACK_CAT_MUSIC || category == RADIO_TRACK_CAT_TAKEOVER_MUSIC) && m_PlaySequentialMusicTracks)
	{
		audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(category));
		const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(category);
		if(naVerifyf(history && trackLists, "Failed to get history or track lists for category RADIO_TRACK_CAT_MUSIC"))
		{
			// need this mod here in case we've loaded a save game that had a different number of tracks		
			const s32 numTracks = trackLists->ComputeNumTracks();

			if (numTracks > 0)
			{
				u32 trackIndexToPlay = (*history)[0] % numTracks;
				(*history)[0] = (trackIndexToPlay + 1) % numTracks;
				history->SetWriteIndex(1);
				track = trackLists->GetTrack(trackIndexToPlay);
				naCErrorf(track, "Failed to get track for category RADIO_TRACK_CAT_MUSIC at index %d", trackIndexToPlay);
				index = trackIndexToPlay;
			}			
		}
	}
	else if(m_UseRandomizedStrideSelection && IsRandomizedStrideCategory(category))
	{
		audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(category));
		const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(category);

		// If we're using takeover mode or we only have one set of data (rather than old/new data) then we can just do a simple
		// selection from any of the tracks
		if(m_HasTakeoverContent || trackLists->GetUnlockedListCount() == 1)
		{
			if(naVerifyf(history && trackLists, "Failed to get history or track lists for category %s", TrackCats_ToString((TrackCats)category)))
			{	
				const s32 numTracks = trackLists->ComputeNumTracks();
				const u32 previousTrack = history->GetPreviousEntry();

				u32 trackIndexToPlay = (previousTrack + numTracks + m_CategoryStride[category]) % numTracks;
				(*history)[0] = trackIndexToPlay;
				history->SetWriteIndex(1);
				track = trackLists->GetTrack(trackIndexToPlay);
				naCErrorf(track, "Failed to get track for category %s at index %d", TrackCats_ToString((TrackCats)category), trackIndexToPlay);
				audDisplayf("Setting category %s to %u (num tracks: %u)", TrackCats_ToString((TrackCats)category), trackIndexToPlay, numTracks);
				index = trackIndexToPlay;
			}
		}
		// If we have old/new data then we need to first select which track list to play from and then pick which track from that track list
		// to play, using the per-track list stride/history (stored in the takeover state slot for the new data)
		else if(audVerifyf(trackLists->GetUnlockedListCount() == 2, "Station %s is in stride mode with %d unlocked track lists", GetName(), trackLists->GetUnlockedListCount()))
		{
			u32 numTracks = 0;
			u32 numNewTracks = 0;
			u32 unlockedListIndex = 0;

			for(u32 i = 0; i < trackLists->GetListCount(); i++)
			{
				if(trackLists->IsTrackListUnlocked(trackLists->GetList(i)))
				{
					const u32 thisTrackListNumTracks = trackLists->GetList(i)->numTracks;
					numTracks += thisTrackListNumTracks;

					if(unlockedListIndex == 1)
					{
						numNewTracks = thisTrackListNumTracks;
					}

					unlockedListIndex++;
				}				
			}

			// work out probability of picking new track based on normal distribution
			const float normalProbability = numNewTracks / static_cast<float>(numTracks);
			const float scaledProbability = normalProbability * (m_IsFirstMusicTrackSinceBoot ? m_FirstBootNewTrackBias : m_NewTrackBias);
			m_IsFirstMusicTrackSinceBoot = false;

			u32 trackListIndex = ResolveProbability(scaledProbability) ? 1 : 0;			
			s32 adjustedCategory = category;

			if(trackListIndex == 1)
			{
				if(category == RADIO_TRACK_CAT_MUSIC) { adjustedCategory = RADIO_TRACK_CAT_TAKEOVER_MUSIC; }
				if(category == RADIO_TRACK_CAT_IDENTS) { adjustedCategory = RADIO_TRACK_CAT_TAKEOVER_IDENTS; }
				if(category == RADIO_TRACK_CAT_DJSOLO) { adjustedCategory = RADIO_TRACK_CAT_TAKEOVER_DJSOLO; }
				history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(adjustedCategory));
			}

			unlockedListIndex = 0;

			for(u32 i = 0; i < trackLists->GetListCount(); i++)
			{
				if(trackLists->IsTrackListUnlocked(trackLists->GetList(i)))
				{
					if(unlockedListIndex == trackListIndex)
					{
						const RadioStationTrackList* trackList = trackLists->GetList(i);
						const s32 numTracks = trackList->numTracks;
						const u32 previousTrack = history->GetPreviousEntry();

						u32 localTrackIndexToPlay = (previousTrack + numTracks + m_CategoryStride[adjustedCategory]) % numTracks;
						(*history)[0] = localTrackIndexToPlay;
						history->SetWriteIndex(1);						

						// Map our track list specific index back to a global index
						u32 trackIndexToPlay = FindTrackId(category, trackList->Track[localTrackIndexToPlay].SoundRef);
						track = trackLists->GetTrack(trackIndexToPlay);
						naCErrorf(track, "Failed to get track for category %s at index %d", TrackCats_ToString((TrackCats)category), trackIndexToPlay);
						audDisplayf("Setting category %s to %u (num tracks: %u)", TrackCats_ToString((TrackCats)category), trackIndexToPlay, numTracks);
						index = trackIndexToPlay;
						break;
					}					

					unlockedListIndex++;
				}
			}

		}
	}
	else
	{
		s32 firstTrackIndex = 0;
		s32 endTrackIndex = (s32)numTracks - 1;

		// favor new tracks slightly (those not in the first playlist)
		if(WIN32PC_ONLY(!m_IsUserStation && )
			category == RADIO_TRACK_CAT_MUSIC &&
			numTracks > 0)
		{
			const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);
			if(m_IsFirstMusicTrackSinceBoot && trackLists->GetUnlockedListCount() > 1)
			{
				u32 numNewTracks = 0;

				// see how many new tracks we have
				bool haveMultipleTrackListsAvailable = false;
				for(s32 trackListIndex = 1; trackListIndex < trackLists->GetListCount(); trackListIndex++)
				{
					if(!trackLists->IsListLocked(trackListIndex))
					{
						haveMultipleTrackListsAvailable = true;
						numNewTracks += trackLists->GetList(trackListIndex)->numTracks;
						break;
					}
				}

				if(haveMultipleTrackListsAvailable)
				{
					// work out probability of picking new track based on normal distribution
					const float normalProbability = numNewTracks / static_cast<float>(numTracks);
					const float scaledProbability = normalProbability * (m_IsFirstMusicTrackSinceBoot ? m_FirstBootNewTrackBias : m_NewTrackBias);
					m_IsFirstMusicTrackSinceBoot = false;
					if(ResolveProbability(scaledProbability))
					{
						audDisplayf("%s Forcing new track (first music track since boot: %s", GetName(), m_IsFirstMusicTrackSinceBoot? "TRUE" : "FALSE");
						firstTrackIndex = static_cast<s32>(trackLists->GetList(0)->numTracks);
					}
					else
					{
						endTrackIndex = static_cast<s32>(trackLists->GetList(0)->numTracks - 1);
					}

					numTracks = (endTrackIndex - firstTrackIndex) + 1;
				}
			}
		}

#if RSG_PC
		if(m_IsUserStation && category == RADIO_TRACK_CAT_MUSIC)
		{
			index = sm_UserRadioTrackManager.GetNextTrack();
			return GetTrack(category, requiredContext, index);
		}
#endif // RSG_PC
		//Make sure we don't look over too much history.
		const s32 historyLength = Max<s32>((1 + endTrackIndex - firstTrackIndex) - kMinRadioTracksOutsideHistory, 0);

		bool isTrackInHistory = false;
		do
		{
			const s32 trackIndex = GetRandomNumberInRange(firstTrackIndex, endTrackIndex);
			track = GetTrack(category, requiredContext, trackIndex);
			index = trackIndex;
			//Check that this track hasn't just been played/selected (to cover virtually streaming stations that are not adding
			//selected tracks to the history.
#if RSG_PC
			if(m_IsUserStation && category == RADIO_TRACK_CAT_MUSIC)
			{
				// Track index is used as history identifier for user music
				// Should instead use hash of file path?
				isTrackInHistory = IsTrackInHistory(trackIndex, category, historyLength);
			}
			else 
#endif // RSG_PC

			if(audVerifyf(track, "NULL track!"))
			{
				if(numTracks > 1 && audRadioTrack::MakeTrackId(category, trackIndex, track->SoundRef) == m_Tracks[m_ActiveTrackIndex].GetRefForHistory())
				{
					isTrackInHistory = true;
				}
				else
				{
					isTrackInHistory = IsTrackInHistory(audRadioTrack::MakeTrackId(category, trackIndex, track->SoundRef), category, historyLength);
				}
			}			
		} while(isTrackInHistory);
	}

	return track;
}

const audRadioStationTrackListCollection *audRadioStation::FindTrackListsForCategory(const s32 category) const
{
	switch(category)
	{
	case RADIO_TRACK_CAT_MUSIC:
		return &m_MusicTrackLists;
	case RADIO_TRACK_CAT_DJSOLO:
		return &m_DjSoloTrackLists;
	case RADIO_TRACK_CAT_IDENTS:
		return &m_IdentTrackLists;
	case RADIO_TRACK_CAT_ADVERTS:
		return &m_AdvertTrackLists;
	case RADIO_TRACK_CAT_NEWS:
		return &sm_NewsTrackLists;
	case RADIO_TRACK_CAT_WEATHER:
		return &sm_WeatherTrackLists;
	case RADIO_TRACK_CAT_TO_AD:
		return &m_ToAdTrackLists;
	case RADIO_TRACK_CAT_TO_NEWS:
		return &m_ToNewsTrackLists;
	case RADIO_TRACK_CAT_USER_INTRO:
		return &m_UserIntroTrackLists;
	case RADIO_TRACK_CAT_USER_OUTRO:
		return &m_UserOutroTrackLists;
	case RADIO_TRACK_CAT_INTRO_MORNING:
		return &m_UserIntroMorningTrackLists;
	case RADIO_TRACK_CAT_INTRO_EVENING:
		return &m_UserIntroEveningTrackLists;		
	case RADIO_TRACK_CAT_TAKEOVER_DJSOLO:
		return &m_TakeoverDjSoloTrackLists;
	case RADIO_TRACK_CAT_TAKEOVER_IDENTS:
		return &m_TakeoverIdentTrackLists;
	case RADIO_TRACK_CAT_TAKEOVER_MUSIC:
		return &m_TakeoverMusicTrackLists;
	default:
		audAssertf(false, "Invalid category: %d", category);
		return NULL;
	}

}

s32 audRadioStation::ComputeNumTracksAvailable(const s32 category, u32 context) const
{
	const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(category);
	if(!trackLists)
	{
		return 0;
	}
#if RSG_PC
	if(m_IsUserStation && category == RADIO_TRACK_CAT_MUSIC)
	{
		return sm_UserRadioTrackManager.GetNumTracks();
	}
#endif
	return trackLists->ComputeNumTracks(context);
}

void audRadioStation::ForceTrack(const char *trackSettingsName, s32 startOffsetMs)
{	
	naDisplayf("Forcing track %s on %s with start offset %u", trackSettingsName, GetName(), startOffsetMs);	
	RadioTrackSettings *trackSettings = audNorthAudioEngine::GetObject<RadioTrackSettings>(trackSettingsName);

	if(trackSettings)
	{
		ForceTrack(trackSettings, startOffsetMs);
	}		
	else
	{
		ForceTrack(atStringHash(trackSettingsName), startOffsetMs);
	}
}

void audRadioStation::ForceTrack(u32 soundHash, s32 startOffsetMs)
{	
	u32 forcedTrackIndex = 0;

	// For the battle mix stations, if we set a start offset past the end of the given track,
	// we want to wrap around to the next track in the sequence
	if (m_IsMixStation)
	{
		const RadioStationTrackList* trackList = m_MusicTrackLists.GetList(0);

		if (trackList)
		{
			bool allTracksValid = true;
			u32 selectedTrackOffset = 0;
			u32 totalDuration = 0;
			const StreamingSound** streamingSounds = Alloca(const StreamingSound*, trackList->numTracks);

			for (u32 trackIndex = 0; trackIndex < trackList->numTracks; trackIndex++)
			{
				streamingSounds[trackIndex] = SOUNDFACTORY.DecompressMetadata<StreamingSound>(trackList->Track[trackIndex].SoundRef);

				if (!streamingSounds[trackIndex])
				{
					allTracksValid = false;
					break;
				}
				else
				{
					if (soundHash == trackList->Track[trackIndex].SoundRef)
					{
						selectedTrackOffset = totalDuration;
					}

					totalDuration += streamingSounds[trackIndex]->Duration;
				}
			}

			if (allTracksValid)
			{
				u32 adjustedOffset = (selectedTrackOffset + startOffsetMs) % totalDuration;
				u32 trackEnd = 0;
				u32 trackStart = 0;

				for (u32 trackIndex = 0; trackIndex < trackList->numTracks; trackIndex++)
				{
					trackStart = trackEnd;
					trackEnd += streamingSounds[trackIndex]->Duration;

					if (adjustedOffset < trackEnd)
					{
						startOffsetMs = adjustedOffset - trackStart;

						if (soundHash != trackList->Track[trackIndex].SoundRef)
						{
							naDisplayf("Requested track offset is beyond requested track length - wrapping around to track index %u, offset %u", trackIndex, startOffsetMs);
						}

						soundHash = trackList->Track[trackIndex].SoundRef;						
						forcedTrackIndex = trackIndex;

						audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
						(*history)[0] = (trackIndex + 1) % trackList->numTracks;
						break;
					}
				}
			}
		}
	}

	audMetadataObjectInfo objectInfo;

	if (SOUNDFACTORY.GetMetadataManager().GetObjectInfo(soundHash, objectInfo) && objectInfo.GetType() == StreamingSound::TYPE_ID)
	{
		RadioTrackSettings tempTrackSettings;
		tempTrackSettings.Sound = soundHash;
		tempTrackSettings.History.Category = RADIO_TRACK_CAT_MUSIC;
		tempTrackSettings.History.Sound = soundHash;
		ForceTrack(&tempTrackSettings, startOffsetMs, forcedTrackIndex);
	}
	else
	{
		audAssertf(false, "Failed to get streaming sound from hash: %d", soundHash);
	}
}

void audRadioStation::ForceTrack(const RadioTrackSettings *trackSettings, s32 startOffsetMs, u32 trackIndex)
{
	if(naVerifyf(IsFrozen(), "Attempting to force track on %s but the station is not frozen", GetName()))
	{
		if(m_Tracks[m_ActiveTrackIndex].IsInitialised())
		{
			m_Tracks[m_ActiveTrackIndex].Shutdown();

			f32 startOffset = trackSettings->StartOffset;

			if (startOffsetMs >= 0)
			{
				const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(trackSettings->Sound);				

				if (streamingSound && streamingSound->Duration > 0)
				{
					startOffset = Min(1.f, startOffsetMs / (f32)streamingSound->Duration);
				}								
			}

			audAssertf(trackSettings->History.Category != RADIO_TRACK_CAT_NEWS, "Forced radio track set to NEWS - this is not supported");
			m_Tracks[m_ActiveTrackIndex].Init(	trackSettings->History.Category, 
				trackIndex,
				trackSettings->Sound, 
				startOffset,
				m_IsMixStation && !IsUSBMixStation(),
				m_IsReverbStation,
				m_NameHash
				);

			// Override the sound ref used for history if specified, otherwise default to the actual track sound
			const u32 soundRefForHistory = trackSettings->History.Sound != 0 ? trackSettings->History.Sound : trackSettings->Sound;
			const u32 trackHistoryId = FindTrackId(trackSettings->History.Category, soundRefForHistory);
			if(trackHistoryId != ~0U)
			{
				m_Tracks[m_ActiveTrackIndex].SetRefForHistory(audRadioTrack::MakeTrackId(trackSettings->History.Category, trackHistoryId, soundRefForHistory));
			}
			// We need to support forcing a track on a station that isn't playing physically, in which case don't forcibly prepare
			// the track.
			if(m_ShouldStreamPhysically)
			{
				Assert(m_WaveSlots[m_ActiveTrackIndex]);
				m_Tracks[m_ActiveTrackIndex].Prepare(0);
			}
			m_Tracks[m_ActiveTrackIndex].PlayWhenReady();

			if(m_UseRandomizedStrideSelection)
			{
				audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(trackSettings->History.Category));				

				if(history)
				{	
					(*history)[0] = trackHistoryId;
					history->SetWriteIndex(1);
				}
			}

#if RSG_PC
			m_LastPlayedTrackIndex = m_CurrentlyPlayingTrackIndex;
			m_CurrentlyPlayingTrackIndex = m_Tracks[m_ActiveTrackIndex].GetTrackIndex();
#endif

			// Also shutdown the next track if we've already initialised it, to ensure we haven't already queued the same track
			u32 nextIndex = (m_ActiveTrackIndex+1) & 1;
			if(m_Tracks[nextIndex].IsInitialised())
			{
				m_Tracks[nextIndex].Shutdown();
			}
		}
		else
		{
			audWarningf("Attempting to force radio track, but active track is not initialised");
		}
	}
}

u32 audRadioStation::FindTrackId(const s32 category, const u32 soundRef) const
{
	const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(category);
	u32 numTracks = 0;
	if(trackLists)
	{
		numTracks = trackLists->ComputeNumTracks();
		for(u32 i = 0 ; i < numTracks; i++)
		{
			if(soundRef == trackLists->GetTrack(i)->SoundRef)
			{
				return i;
			}
		}
	}

	return ~0U;
}

audPrepareState audRadioStation::PrepareNextTrack()
{
	audPrepareState prepareState = AUD_PREPARE_FAILED;

	u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;

	audRadioTrack &nextTrack = m_Tracks[nextTrackIndex];
	nextTrack.SetPhysicalStreamingState(m_ShouldStreamPhysically, m_WaveSlots[nextTrackIndex], m_SoundBucketId);

	if(nextTrack.IsInitialised())
	{
		//Sanity-check that we are preparing a track of the correct category.
		//@@: location AUDRADIOSTATION_PREPARENEXTTRACK_SANITY_CHECK
		if(ShouldOnlyPlayMusic() && nextTrack.GetCategory() != RADIO_TRACK_CAT_MUSIC)
		{
			//We should only choose music.
			Assert(!IsFrozen());
			//Stop preparing this track and allow a music track to be selected next frame.
			nextTrack.Shutdown();
			return AUD_PREPARING;
		}

		//We are already in the process of preparing this track, so query it directly.
		if(m_ShouldStreamPhysically)
		{
			audAssertf(nextTrack.GetPlayTime() >= 0, "Negative start offset on radio track %s: %d ", GetName(), nextTrack.GetPlayTime());
			s32 playTime = nextTrack.GetPlayTime() >= 0 ? nextTrack.GetPlayTime() : 0;
			prepareState = nextTrack.Prepare(playTime);
		}
		else
		{
			prepareState = AUD_PREPARED;
		}
	}
	else
	{		
		prepareState = AUD_PREPARING;
	}

	return prepareState;
}

bool audRadioStation::InitialiseNextTrack(const u32 timeInMs)
{
	if(g_RadioAudioEntity.IsInControlOfRadio())
	{
		u32 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
		audRadioTrack &nextTrack = m_Tracks[nextTrackIndex];

		s32 category = RADIO_TRACK_CAT_MUSIC;
		s32 index = 0;
		const RadioStationTrackList::tTrack *trackMetadata = NULL;
		bool wasNetworkSelectedTrack = false;

		if(m_QueuedTrackList)
		{
			trackMetadata = &m_QueuedTrackList->Track[m_QueuedTrackListIndex++];
			if(m_QueuedTrackListIndex >= m_QueuedTrackList->numTracks)
			{
				m_QueuedTrackList = NULL;
				m_QueuedTrackListIndex = 0;
			}
		}
		else if(m_NetworkNextTrack.category != 0xff)
		{
			trackMetadata = GetTrack(m_NetworkNextTrack.category, kNullContext, m_NetworkNextTrack.index);			
			index = m_NetworkNextTrack.index;
			category = m_NetworkNextTrack.category;

			m_NetworkNextTrack.category = 0xff;

			// For sequential music stations, ensure consistency by configuring the history state to be the same as if this 
			// track was chosen next by the single player track selection logic
			if(m_PlaySequentialMusicTracks)
			{
				audRadioStationHistory *history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
				const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);
				if(naVerifyf(history && trackLists, "Failed to get history or track lists for category RADIO_TRACK_CAT_MUSIC"))
				{
					const s32 numTracks = trackLists->ComputeNumTracks();
					(*history)[0] = numTracks > 0 ? ((index + 1) % numTracks) : 0;
					history->SetWriteIndex(1);
				}
			}

			wasNetworkSelectedTrack = true;
		}
		else
		{
			category = ComputeNextTrackCategory(timeInMs);
			trackMetadata = ComputeRandomTrack(category, index);

//#if RSG_PC
//			// this would be used for sequential non random user music
//			if(m_IsUserStation && /*isSequentialMode*/)
//			{
//				index = audRadioStation::GetUserRadioTrackManagaer()->GetNextTrack();
//			}
//#endif
		}

		if(!trackMetadata)
		{
			// this can happen if all of the tracks in the chosen category are already in history, which is more likely for weather/news etc
			// fall back to music in this case
#if RSG_PC
			if(m_IsUserStation)
			{
				RADIODEBUG2("No valid tracks for %s, falling back to adverts", TrackCats_ToString((TrackCats)category));
				category = RADIO_TRACK_CAT_ADVERTS;			
				trackMetadata = ComputeRandomTrack(category, index);
			}
			else
#endif // RSG_PC
			{
				RADIODEBUG2("No valid tracks for %s, falling back to music", TrackCats_ToString((TrackCats)category));
				category = RADIO_TRACK_CAT_MUSIC;			
				trackMetadata = ComputeRandomTrack(category, index);
			}

		}

		// Valid to have no tracks on the USB station until something is unlocked
		if (!IsUSBMixStation())
		{			
			audAssertf(trackMetadata, "%s failed to compute random track from RADIO_TRACK_CAT_MUSIC category", GetName());
		}

		if(trackMetadata)
		{
			// randomise start offset if this is the first track we're playing
			nextTrack.Init(category, index, trackMetadata->SoundRef, m_IsFirstTrack ? m_FirstTrackStartOffset : 0.f, m_IsMixStation && !IsUSBMixStation(), m_IsReverbStation, m_NameHash);

			audAssertf(nextTrack.GetDuration() > 0, "Invalid track/sound metadata: %s (%s : %u)", GetName(), TrackCats_ToString((TrackCats)category), index);
			m_IsFirstTrack = false;

			LogTrack(category, 0, index, trackMetadata->SoundRef);

			// We don't want to do the takeover track selection if this was a remote selected track as all of our random
			// seed etc. info is being extracted after this process has already been done
			if (!wasNetworkSelectedTrack)
			{
				if (category == RADIO_TRACK_CAT_TAKEOVER_IDENTS)
				{
					m_TakeoverLastTimeMs = m_StationAccumulatedPlayTimeMs;

					Assign(m_TakeoverMusicTrackCounter, GetRandomNumberInRange(m_TakeoverMinTracks, m_TakeoverMaxTracks));
					RADIODEBUG2("Started Takeover Segment - %d tracks", m_TakeoverMusicTrackCounter);
				}
				if (category == RADIO_TRACK_CAT_TAKEOVER_MUSIC)
				{
					// continue to update the LastTakeoverTimer while we're in the takeover segment to avoid coming
					// back too quickly
					m_TakeoverLastTimeMs = m_StationAccumulatedPlayTimeMs;

					if (m_TakeoverMusicTrackCounter > 0)
					{
						m_TakeoverMusicTrackCounter--;
					}
				}

				if(m_HasTakeoverContent)
				{
					if(category == RADIO_TRACK_CAT_MUSIC)
					{
						m_MusicRunningCount++;
					}
					else
					{
						m_MusicRunningCount = 0;
					}
				}

				m_StationAccumulatedPlayTimeMs += nextTrack.GetDuration();
			}

			//Log the next time that this track category can be selected.
			//NOTE: We log a track category when it is selected, rather than heard, to ensure that we maintain a reasonable
			//distribution of valid selection times across virtualised stations.
			const RadioTrackCategoryData *categoryData = sm_MinTimeBetweenRepeatedCategories;
#if RSG_PC
			if(m_IsUserStation && sm_MinTimeBetweenRepeatedCategoriesUser)
			{
				categoryData = sm_MinTimeBetweenRepeatedCategoriesUser;
			}
#endif
			SetNextValidSelectionTime(category, timeInMs + categoryData->Category[category].Value);	

			return true;
		}
	}
	return false;
}

void audRadioStation::SetNextValidSelectionTime(const s32 category, const u32 timeMs)
{ 
	m_NextValidSelectionTime[category] = timeMs; 
}

void audRadioStation::UnlockMissionNewsStory(const u32 newsStory)
{
	const u32 storyId = newsStory - 1;
	if(naVerifyf(storyId < kMaxNewsStories, "Story id %d is out of bounds", storyId))
	{
		sm_NewsStoryState[storyId] = (RADIO_NEWS_UNLOCKED | RADIO_NEWS_ONCE_ONLY);
		naDisplayf("Unlocked mission news story %u", newsStory);
	}
	// check that we don't have too many news stories unlocked; lock old stories if we do
	u32 numUnlocked = 0;
	for(u32 i = 0; i < kMaxNewsStories; i++)
	{
		if(sm_NewsStoryState[i] == (RADIO_NEWS_ONCE_ONLY|RADIO_NEWS_UNLOCKED))
		{
			numUnlocked++;
		}
	}
	// only allow two unlocked stories
	if(numUnlocked > 2)
	{
		numUnlocked -= 2;
		for(u32 i = 0; i < kMaxNewsStories && numUnlocked > 0; i++)
		{
			if(sm_NewsStoryState[i] == (RADIO_NEWS_ONCE_ONLY|RADIO_NEWS_UNLOCKED))
			{
				sm_NewsStoryState[i] &= ~(RADIO_NEWS_UNLOCKED);
				naDisplayf("Too many news stories unlocked; locking %u", i+1);
				numUnlocked--;
			}
		}
	}
}

void audRadioStation::ForceNextTrack(const char* categoryName, const u32 contextHash, const u32 trackNameHash, bool ignoreLocking)
{
	TrackCats category = TrackCats_Parse(categoryName, TRACKCATS_MAX);

	if (category != TRACKCATS_MAX)
	{
		const audRadioStationTrackListCollection* trackListCollection = FindTrackListsForCategory(category);

		if (trackListCollection)
		{
			s32 numTracks = trackListCollection->ComputeNumTracks(kNullContext, ignoreLocking);

			for (u32 i = 0; i < numTracks; i++)
			{
				if (trackListCollection->GetTrack(i, kNullContext, ignoreLocking)->SoundRef == trackNameHash)
				{
					ForceNextTrack((u8)category, contextHash, (u16)i);
					return;
				}
			}
		}
	}
}

void audRadioStation::ForceNextTrack(const u8 category, const u32 context, const u16 index)
{
	m_ForceNextTrackCategory = category;
	m_ForceNextTrackContext = context;
	m_ForceNextTrackIndex = index;
	const u8 nextTrackIndex = (m_ActiveTrackIndex + 1) % 2;
	audDisplayf("%s is forcing next track to category %u context %u track %u", GetName(), category, context, index);

	if(m_Tracks[nextTrackIndex].IsInitialised())
	{
		m_Tracks[nextTrackIndex].Shutdown();
		m_IsPlayingOverlappedTrack = false;
		m_IsRequestingOverlappedTrackPrepare = false;
	}
}

struct trackEntry
{
	trackEntry(){}
	trackEntry(u8 i, u8 o) : index(i), order(o) { }
	u8 index;
	u8 order;
};

int TrackEntryCompare(const trackEntry *l, const trackEntry *r)
{
	return (int)l->order - (int)r->order;
}

void audRadioStation::QueueTrackList(const RadioStationTrackList *trackList, const bool forceNow)
{
	if(trackList->numTracks == 0)
	{
		NOTFINAL_ONLY(audWarningf("%s - Ignoring empty track list %s", GetName(), audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(trackList->NameTableOffset));)
		return;
	}
	if(m_QueuedTrackList)
	{
		NOTFINAL_ONLY(audWarningf("%s - Replacing queued list %s (at track index %u) with %s", GetName(), audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_QueuedTrackList->NameTableOffset), m_QueuedTrackListIndex, audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(trackList->NameTableOffset));)
	}
	m_QueuedTrackListIndex = 0;
	m_QueuedTrackList = trackList;

	if(AUD_GET_TRISTATE_VALUE(trackList->Flags, FLAG_ID_RADIOSTATIONTRACKLIST_SHUFFLE) == AUD_TRISTATE_TRUE)
	{		
		atFixedArray<trackEntry, RadioStationTrackList::MAX_TRACKS> shuffler;
		for(u32 i = 0; i < trackList->numTracks; i++)
		{
			if(m_MusicTrackHistory.IsInHistory(trackList->Track[i].SoundRef, 4))
			{
				// push recently heard tracks to the back of the queue
				shuffler.Push(trackEntry((u8)i, 255));
			}
			else
			{
				shuffler.Push(trackEntry((u8)i, (u8)audEngineUtil::GetRandomInteger()));
			}

		}
		shuffler.QSort(0, -1, &TrackEntryCompare);

		atRangeArray<RadioStationTrackList::tTrack, RadioStationTrackList::MAX_TRACKS> tracks;
		sysMemCpy(&tracks, &trackList->Track[0], sizeof(RadioStationTrackList::tTrack) * trackList->numTracks);

		for(u32 i = 0; i < trackList->numTracks; i++)
		{
			const_cast<RadioStationTrackList*>(trackList)->Track[i] = tracks[shuffler[i].index];
		}
	}

	audRadioTrack &activeTrack = m_Tracks[m_ActiveTrackIndex];
	const u32 nextTrackIndex = (m_ActiveTrackIndex + 1) &1;
	audRadioTrack &nextTrack = m_Tracks[nextTrackIndex];
	
	if(forceNow)
	{
		if(nextTrack.IsInitialised())
		{
			nextTrack.Shutdown();
		}
		if(activeTrack.IsInitialised())
		{
			activeTrack.Shutdown();
		}		
	}
	else
	{

		// If there's time, cancel the next track and choose again
		if(nextTrack.IsInitialised() && activeTrack.IsInitialised() && activeTrack.GetDuration() - (u32)activeTrack.GetPlayTime() > 45000U)
		{
			nextTrack.Shutdown();
		}	
	}

	NOTFINAL_ONLY(audDisplayf("%s - Queued track list %s", GetName(), audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_QueuedTrackList->NameTableOffset));)
}

void audRadioStation::ClearQueuedTracks()
{
	if(m_QueuedTrackList)
	{
		NOTFINAL_ONLY(audDisplayf("%s - Cleared queued track list %s", GetName(), audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_QueuedTrackList->NameTableOffset));)

		m_QueuedTrackList = NULL;
		m_QueuedTrackListIndex = 0;

		audRadioTrack &activeTrack = m_Tracks[m_ActiveTrackIndex];
		const u32 nextTrackIndex = (m_ActiveTrackIndex + 1) &1;
		audRadioTrack &nextTrack = m_Tracks[nextTrackIndex];

		// If there's time, cancel the next track and choose again
		if(nextTrack.IsInitialised() && activeTrack.IsInitialised() && activeTrack.GetDuration() - (u32)activeTrack.GetPlayTime() > 45000U)
		{
			nextTrack.Shutdown();
		}		
	}
}

bool audRadioStation::ShouldOnlyPlayMusic() const
{
#if RSG_PC
	if(m_IsUserStation)
	{
		return (sm_UserRadioTrackManager.GetRadioMode() != USERRADIO_PLAY_MODE_RADIO);
	}
#endif
	if(IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get())
		return false; // don't allow switch to music-only as it will break the determinism that we rely on for network sync
	return (!m_ShouldPlayFullRadio || m_ScriptSetMusicOnly);
}

void audRadioStation::DumpNewsState()
{
	u32 numNewsStoriesUnlocked = 0;
	u32 genericNewsStories = 0;

	for(u32 i = 0; i < kMaxNewsStories; i++)
	{
		if(sm_NewsStoryState[i] & RADIO_NEWS_UNLOCKED)
		{
			numNewsStoriesUnlocked++;
			if(sm_NewsStoryState[i] & RADIO_NEWS_ONCE_ONLY)
			{
				naDisplayf("Mission news %u unlocked", i);
			}
			else
			{
				genericNewsStories++;
			}
		}
	}

	naDisplayf("%u unlocked news stories (%u generic)", numNewsStoriesUnlocked, genericNewsStories);
}

u32 audRadioStation::GetNextValidSelectionTime(const s32 category) const
{ 
	bool disableNetworkModeForStation = false;
	WIN32PC_ONLY(disableNetworkModeForStation = m_IsUserStation);

	if(!disableNetworkModeForStation && (IsNetworkModeHistoryActive() || PARAM_disableradiohistory.Get()))
		return 0U; // always valid

	return m_NextValidSelectionTime[category]; 
}

bool audRadioStation::LockTrackList(const char *trackListName)
{
	const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
	return LockTrackList(trackList);	
}

bool audRadioStation::LockTrackList(u32 trackListName)
{
	const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
	return LockTrackList(trackList);
}

bool audRadioStation::LockTrackList(const RadioStationTrackList *trackList)
{	
	if (trackList)
	{
		audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(trackList->Category));
		if (audVerifyf(trackListCollection, "%s - Failed to find track lists for %s", GetName(), TrackCats_ToString((TrackCats)trackList->Category)))
		{
			const bool ret = trackListCollection->LockTrackList(trackList);
			if (ret)
			{
				audDisplayf("Locked track list %s on %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset), GetName());

				if(m_UseRandomizedStrideSelection)
				{
					RandomizeCategoryStride((TrackCats)trackList->Category, false, true);
				}
			}
			return ret;
		}
	}
	return false;
}


bool audRadioStation::IsTrackListUnlocked(const char *trackListName) const
{
	const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
	return IsTrackListUnlocked(trackList);
}

bool audRadioStation::IsTrackListUnlocked(u32 trackListName) const
{
	const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
	return IsTrackListUnlocked(trackList);
}

bool audRadioStation::IsTrackListUnlocked(const RadioStationTrackList *trackList) const
{
	if(trackList)
	{
		audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(trackList->Category));
		if(audVerifyf(trackListCollection, "%s - Failed to find track lists for %s", GetName(), TrackCats_ToString((TrackCats)trackList->Category)))
		{
			return trackListCollection->IsTrackListUnlocked(trackList);			
		}
	}

	return false;
}

bool audRadioStation::UnlockTrackList(const char *trackListName)
{
	const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
	return UnlockTrackList(trackList);
}

bool audRadioStation::UnlockTrackList(u32 trackListName)
{
	const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
	return UnlockTrackList(trackList);
}

bool audRadioStation::UnlockTrackList(const RadioStationTrackList *trackList)
{
	if(trackList)
	{
		audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(trackList->Category));
		if(audVerifyf(trackListCollection, "%s - Failed to find track lists for %s", GetName(), TrackCats_ToString((TrackCats)trackList->Category)))
		{
			const bool ret = trackListCollection->UnlockTrackList(trackList);
			if(ret)
			{
				audDisplayf("Unlocked track list %s on %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset), GetName());

				if(m_UseRandomizedStrideSelection)
				{
					RandomizeCategoryStride((TrackCats)trackList->Category, false, true);
				}
			}
			return ret;
		}
	}
	return false;
}

// Force the given music track list to be the only available track list (used for USB station behavior)
bool audRadioStation::ForceMusicTrackList(const u32 trackListName, u32 timeOffsetMs)
{
	Freeze();	
	RadioStationTrackList* trackList = audNorthAudioEngine::GetMetadataManager().GetObject<RadioStationTrackList>(trackListName);

	if (trackList)
	{
		audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

		if (trackListCollection)
		{
			for (u32 i = 0; i < trackListCollection->GetListCount(); i++)
			{
				if (trackListCollection->GetList(i) == trackList)
				{
					trackListCollection->UnlockTrackList(trackListCollection->GetList(i));
				}
				else
				{
					trackListCollection->LockTrackList(trackListCollection->GetList(i));
				}
			}

			SkipToOffsetInMusicTrackList(timeOffsetMs, true);
		}

		m_LastForcedMusicTrackList = trackListName;
	}

	Unfreeze();	
	return true;
}

// Skip to the given offset relative to the start of the currently active tracklist (used for USB station behavior)
void audRadioStation::SkipToOffsetInMusicTrackList(u32 timeOffsetMs, bool allowWrap)
{
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		const RadioStationTrackList::tTrack* selectedTrack = GetMusicTracklistTrackForTimeOffset(timeOffsetMs, allowWrap);

		if (selectedTrack)
		{
			u32 totalTracksIncludingLocked = trackListCollection->ComputeNumTracks(0u, true);

			for (u32 i = 0; i < totalTracksIncludingLocked; i++)
			{
				if (trackListCollection->GetTrack(i, 0u, true) == selectedTrack)
				{
					audRadioStationHistory* history = const_cast<audRadioStationHistory*>(FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));

					if (history)
					{
						(*history)[0] = i;

						const s32 trackIndex = GetMusicTrackIndexInTrackList(selectedTrack);
						const s32 trackStartTime = GetMusicTrackListTrackStartTimeMs(trackIndex);
						const s32 trackDuration = GetMusicTrackListTrackDurationMs(trackIndex);

						m_FirstTrackStartOffset = Clamp((timeOffsetMs - trackStartTime) / (f32)trackDuration, 0.f, 1.f);
						m_IsFirstTrack = true;
						m_ActiveTrackIndex = 0;

						m_Tracks[0].Shutdown();
						m_Tracks[1].Shutdown();
						PrepareNextTrack();
					}

					break;
				}
			}
		}
	}
}

// Query the current time offset into the current music track list (used for USB station behavior)
u32 audRadioStation::GetMusicTrackListCurrentPlayTimeMs() const
{	
	u32 startTime = 0u;
	s32 playingTrackIndex = GetMusicTrackListActiveTrackIndex();

	if (playingTrackIndex >= 0)
	{
		startTime = GetMusicTrackListTrackStartTimeMs((u32)playingTrackIndex);
	}
	
	return startTime + m_Tracks[m_ActiveTrackIndex].GetPlayTime();
}

// Get the track that will be playing at the given time offset
const RadioStationTrackList::tTrack* audRadioStation::GetMusicTracklistTrackForTimeOffset(u32 timeOffsetMs, bool allowWrap) const
{
	audRadioStationTrackListCollection* trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		u32 totalDuration = 0u;
		const u32 numUnlockedTracks = trackListCollection->ComputeNumTracks();

		if(allowWrap)
		{
			for (u32 i = 0; i < numUnlockedTracks; i++)
			{
				if (const RadioStationTrackList::tTrack* track = trackListCollection->GetTrack(i))
				{
					const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(track->SoundRef);
					totalDuration += streamingSound ? streamingSound->Duration : 0u;
				}			
			}

			timeOffsetMs %= totalDuration;
		}

		u32 cumulativeDuration = 0;

		for (u32 i = 0; i < numUnlockedTracks; i++)
		{
			const RadioStationTrackList::tTrack* track = trackListCollection->GetTrack(i);

			if (track)
			{
				const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(track->SoundRef);
				cumulativeDuration += streamingSound ? streamingSound->Duration : 0u;

				if (timeOffsetMs < cumulativeDuration)
				{
					return track;
				}
			}			
		}
	}	

	return nullptr;
}

// Get the track that will be playing at the given index offset
const RadioStationTrackList::tTrack* audRadioStation::GetMusicTrackListTrackForIndex(u32 trackIndex) const
{
	audRadioStationTrackListCollection* trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		if (audVerifyf(trackIndex < trackListCollection->ComputeNumTracks(), "Track %u exceeds total number of tracks (%u)", trackIndex, trackListCollection->ComputeNumTracks()))
		{
			return trackListCollection->GetTrack(trackIndex);
		}
	}

	return nullptr;
}

// Get the track ID for the track playing at the givne offset in the currently active music playlist (used for USB station behavior)
u32 audRadioStation::GetMusicTrackListTrackIDForTimeOffset(u32 timeOffsetMs) const
{
	const RadioStationTrackList::tTrack* track = GetMusicTracklistTrackForTimeOffset(timeOffsetMs);
	return track ? audRadioTrack::FindTextIdForSoundHash(track->SoundRef) : 0u;	
}

// Get the track ID for the given track in the currently active music playlist (used for USB station behavior)
u32 audRadioStation::GetMusicTrackListTrackIDForIndex(u32 trackIndex) const
{
	const RadioStationTrackList::tTrack* track = GetMusicTrackListTrackForIndex(trackIndex);
	return track ? audRadioTrack::FindTextIdForSoundHash(track->SoundRef) : 0u;
}

// Query the total number of tracks in the current track list (used for USB station behavior)
u32 audRadioStation::GetMusicTrackListNumTracks() const
{
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));
	return trackListCollection ? trackListCollection->ComputeNumTracks() : 0;
}

// Query the total duration of the current music track list (used for USB station behavior)
u32 audRadioStation::GetMusicTrackListDurationMs() const
{	
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));
	return trackListCollection ? GetMusicTrackListTrackStartTimeMs(trackListCollection->ComputeNumTracks()) : 0;	
}

// Query the duration of the music track at the given index in the active music track list (used for USB station behavior)
u32 audRadioStation::GetMusicTrackListTrackDurationMs(u32 trackIndex) const
{
	const RadioStationTrackList::tTrack* track = GetMusicTrackListTrackForIndex(trackIndex);	
	const StreamingSound *streamingSound = track ? SOUNDFACTORY.DecompressMetadata<StreamingSound>(track->SoundRef) : nullptr;
	return streamingSound ? streamingSound->Duration : 0u;
}

// Get the start time of the given track in the music track list (used for USB station behavior)
s32 audRadioStation::GetMusicTrackListTrackStartTimeMs(u32 trackIndex) const
{
	u32 cumulativeDuration = 0;
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));
	
	if (trackListCollection) 
	{
		const u32 numUnlockedTracks = trackListCollection->ComputeNumTracks();

		for (u32 i = 0; i < trackIndex && i < numUnlockedTracks; i++)
		{
			const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(trackListCollection->GetTrack(i)->SoundRef);
			cumulativeDuration += streamingSound ? streamingSound->Duration : 0u;
		}		
	}

	return cumulativeDuration;
}

// Get the index of the given track in the playing tracklist (used for USB station behavior)
s32 audRadioStation::GetMusicTrackIndexInTrackList(const RadioStationTrackList::tTrack* track) const
{
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		const u32 numUnlockedTracks = trackListCollection->ComputeNumTracks();

		for (u32 i = 0; i < numUnlockedTracks; i++)
		{
			if (trackListCollection->GetTrack(i) == track)
			{
				return i;
			}
		}
	}	

	return -1;
}

// Get the track index of the currently playing track relative to the music track list (used for USB station behavior)
s32 audRadioStation::GetMusicTrackListActiveTrackIndex() const
{
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		const s32 numUnlockedTracks = trackListCollection->ComputeNumTracks();

		for (s32 i = 0; i < numUnlockedTracks; i++)
		{
			const RadioStationTrackList::tTrack* track = trackListCollection->GetTrack(i);

			if (track && track->SoundRef == m_Tracks[m_ActiveTrackIndex].GetSoundRef())
			{
				return i;
			}
		}
	}	

	return -1;
}

u32 audRadioStation::GetPlayingTrackListNextTrackTimeOffset()
{
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		const s32 nextTrackIndex = GetMusicTrackListActiveTrackIndex() + 1;
		const u32 numUnlockedTracks = trackListCollection->ComputeNumTracks();

		if (nextTrackIndex < numUnlockedTracks)
		{
			return GetMusicTrackListTrackStartTimeMs(nextTrackIndex);
		}
	}	

	// Wrap around (needs confirming if this is desired behavior)
	return 0;
}


u32 audRadioStation::GetPlayingTrackListPrevTrackTimeOffset()
{
	audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC));

	if (trackListCollection)
	{
		const s32 prevTrackIndex = GetMusicTrackListActiveTrackIndex() - 1;
		const u32 numUnlockedTracks = trackListCollection->ComputeNumTracks();

		if (prevTrackIndex >= 0)
		{
			return GetMusicTrackListTrackStartTimeMs((u32)prevTrackIndex);
		}

		// Wrap around (needs confirming if this is desired behavior)
		return GetMusicTrackListTrackStartTimeMs(numUnlockedTracks - 1);
	}

	return 0u;
}

bool audRadioStation::IsCurrentTrackNew() const
{
	const audRadioTrack &track = GetCurrentTrack();
	const audRadioStationTrackListCollection * trackLists = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);

	if(trackLists && 
		track.GetCategory() == RADIO_TRACK_CAT_MUSIC &&
		track.GetTrackIndex() >= trackLists->GetList(0)->numTracks)
	{
		return true;
	}
	return false;
}

#if __BANK
u16 g_ForceNextTrackIndex = 0xffff;
u8 g_ForceNextTrackCategory = 0;
char g_ForceStationName[128]={0};
char g_ForceTrackContextName[128]={0};
char g_ForceTrackSettings[128]={0};
char g_TrackListName[128]={0};
u32 g_DebugNewsStory = 0;

void UnlockNewsCB()
{
	audRadioStation::UnlockMissionNewsStory(g_DebugNewsStory);
}

void Debug_ForceNextTrackCB()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForceStationName);
	if(station)
	{
		station->ForceNextTrack(g_ForceNextTrackCategory, atStringHash(g_ForceTrackContextName), g_ForceNextTrackIndex);
	}
}

void ForceNextTrackCB()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForceStationName);
	if(station)
	{
		station->ForceTrack(g_ForceTrackSettings);
	}
}

void DumpNewsStateCB()
{
	audRadioStation::DumpNewsState();
}

bool g_ForceQueueTrack= true;

void QueueTrackListOnStationCB()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForceStationName);
	if(station)
	{
		const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(g_TrackListName);
		if(trackList)
		{
			station->QueueTrackList(trackList, g_ForceQueueTrack);
		}
		else
		{
			audWarningf("Failed to find track list %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset));
		}
	}
}

void ClearQueuedTracksOnStationCB()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForceStationName);
	if(station)
	{
		station->ClearQueuedTracks();
	}
}

void SnapshotStationCB()
{
	audRadioStation::GetStation(g_TestSyncStationIndex)->PopulateSyncData(g_TestSyncData);
}

void ApplyStationSnapshotCB()
{
	audRadioStation::GetStation(g_TestSyncStationIndex)->SyncStation(g_TestSyncData);
}

void UnlockTrackListCB()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForceStationName);
	if(station)
	{
		station->UnlockTrackList(g_ForceTrackSettings);
	}
}

void LockTrackListCB()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForceStationName);
	if(station)
	{
		station->LockTrackList(g_ForceTrackSettings);
	}
}

void SwitchUSBStationMixCB()
{
	audRadioStation* station = audRadioStation::GetUSBMixStation();

	if (station)
	{
		const char* trackListName = g_RequestedUSBTrackListComboIndex >= 0 ? g_USBTrackListNames[g_RequestedUSBTrackListComboIndex] : nullptr;
		station->ForceMusicTrackList(atStringHash(trackListName), g_USBStationTrackListOffsetMs);		
		station->SetLocked(false);
	}	
}

void USBStationSkipToOffsetCB()
{
	audRadioStation* station = audRadioStation::GetUSBMixStation();

	if (station)
	{
		station->SkipToOffsetInMusicTrackList(g_USBStationTrackListOffsetMs);
	}	
}

void USBStationSkipPrevTrackCB()
{
	audRadioStation* station = audRadioStation::GetUSBMixStation();

	if (station)
	{
		s32 activeTrack = station->GetMusicTrackListActiveTrackIndex();

		if (activeTrack >= 0)
		{
			s32 prevTrackStartTime = station->GetMusicTrackListTrackStartTimeMs(activeTrack - 1);
			station->SkipToOffsetInMusicTrackList(prevTrackStartTime);
		}
	}	
}

void USBStationSkipNextTrackCB()
{	
	audRadioStation* station = audRadioStation::GetUSBMixStation();

	if (station)
	{
		s32 activeTrack = station->GetMusicTrackListActiveTrackIndex();

		if (activeTrack >= 0)
		{
			s32 nextTrackStartTime = station->GetMusicTrackListTrackStartTimeMs(activeTrack + 1);
			station->SkipToOffsetInMusicTrackList(nextTrackStartTime);
		}
	}
}

void USBStationScriptLockTrackListCB()
{
	if (audRadioStation* station = audRadioStation::GetUSBMixStation())
	{
		const char* trackListName = g_RequestedUSBTrackListComboIndex >= 0 ? g_USBTrackListNames[g_RequestedUSBTrackListComboIndex] : nullptr;
		u32 trackListNameHash = atStringHash(trackListName);

		// This duplicates what the script command does
		if(trackListNameHash != ATSTRINGHASH("TUNER_AP_SILENCE_MUSIC", 0x68865F9) && trackListNameHash == station->GetLastForcedMusicTrackList())
		{
			station->ForceMusicTrackList(ATSTRINGHASH("TUNER_AP_SILENCE_MUSIC", 0x68865F9), 0u);
		}
	}
}

void DumpplayerStationtracksCB()
{
	if(const audRadioStation *station=g_RadioAudioEntity.GetPlayerRadioStation())
	{
		station->DumpTrackList();
	}
}

void audRadioStation::DumpTrackList() const
{
	

		
		char textIdName[32];
		const audRadioStationTrackListCollection *trackLists = FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);
		for(s32 listIdx = 0; listIdx < trackLists->GetListCount(); listIdx++)
		{
			const RadioStationTrackList *trackList = trackLists->GetList(listIdx);
			const char *trackListName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset);
			
			for(s32 i = 0; i < trackList->numTracks; i++)
			{
				const u32 soundNameHash = trackList->Track[i].SoundRef;
				formatf(textIdName, "RTT_%08X", soundNameHash);
				const RadioTrackTextIds *rtt = audNorthAudioEngine::GetMetadataManager().GetObject<RadioTrackTextIds>(textIdName);
				const char *soundName = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(soundNameHash);
				if(rtt)
				{					

					if(rtt->numTextIds != 1)
					{
						audErrorf("%s - %s - %s - %d - Radio track with %u text Ids", GetName(), trackListName, soundName, i, rtt->numTextIds);
					}

					const u32 textId = rtt->TextId[0].TextId;
					char artistID[16];
					char trackID[16];

					formatf(artistID,"%dA", textId);
					formatf(trackID,"%dS", textId);
					const char *artistName = TheText.Get(artistID);
					const char *trackTitle = TheText.Get(trackID);
					
					audDisplayf("%s,%s,%d,%u,%s,\"%s\",\"%s\",%u", GetName(), trackListName, i, soundNameHash,soundName, artistName, trackTitle, textId);
				}
				else
				{
					audDisplayf("%s,%s,%d,%u,%s,%s,%s,%u", GetName(), trackListName, i, soundNameHash, soundName, "MISSING", "MISSING", 0);
				}
			}
		}
	}


void audRadioStation::AddWidgets(bkBank &bank)
{
	bank.AddButton("DumpPlayerStationTracks", CFA(DumpplayerStationtracksCB));
	bank.AddToggle("DisableOverlappedTracks", &sm_DisableOverlap);
	bank.AddToggle("Disable Play Time Compensation", &g_DisablePlayTimeCompensation);	
	bank.AddToggle("DebugHistory", &sm_DebugHistory);
	bank.AddButton("Snapshot", CFA(SnapshotStationCB));
	bank.AddButton("Apply", CFA(ApplyStationSnapshotCB));
	bank.AddSlider("Snapshot/Apply Station Index", &g_TestSyncStationIndex, 0, 30, 1);	
	bank.AddToggle("DebugRadioRandom", &g_DebugRadioRandom);

	bank.PushGroup("Radio Wheel Winding");	
	bank.AddSlider("Station Wind Start", &g_RadioWheelWindStart, 0, 100000, 100);
	bank.AddSlider("Station Wind Offset", &g_RadioWheelWindOffset, 0, 100000, 100);
	bank.AddButton("Re-Sort Stations", CFA(SortRadioStationList));
	bank.PopGroup();

	bank.PushGroup("Transitions");	
	bank.AddSlider("Mix Transition Time", &g_RadioTrackMixtransitionTimeMs, 0, 60000, 100);
	bank.AddSlider("Mix Transition Offset Time", &g_RadioTrackMixtransitionOffsetTimeMs, 0, 60000, 100);	
	bank.AddSlider("Track Overlap Time", &g_RadioTrackOverlapTimeMs, 0, 60000, 100);
	bank.AddSlider("User Track Overlap Time", &g_UserRadioTrackOverlapTimeMs, 0, 60000, 100);
	bank.AddSlider("Track Prepare Time", &g_RadioTrackPrepareTimeMs, 0, 60000, 100);
	bank.AddSlider("Track Skip End Time", &g_RadioTrackSkipEndTimeMs, 0, 60000, 100);
	bank.AddToggle("Use Precise Crossfades", &g_UsePreciseTrackCrossfades);
	bank.AddSlider("Precise Crossfade Play Time", &g_RadioTrackOverlapPlayTimeMs, 0, 60000, 100);	
	bank.PopGroup();

	bank.PushGroup("ForceNextTrack");
		bank.AddText("Station", &g_ForceStationName[0], sizeof(g_ForceStationName));
		bank.AddText("Track", &g_ForceTrackSettings[0], sizeof(g_ForceTrackSettings));
		bank.AddButton("ForceTrack", CFA(ForceNextTrackCB));
		bank.AddButton("UnlockTrackList", CFA(UnlockTrackListCB));
		bank.AddButton("LockTrackList", CFA(LockTrackListCB));
		bank.AddSlider("Debug_ForceIndex", &g_ForceNextTrackIndex, 0, 0xffff, 1);
		bank.AddSlider("Debug_ForceCategory", &g_ForceNextTrackCategory, 0, 255, 1);
		bank.AddText("Debug_ForceContext", &g_ForceTrackContextName[0], sizeof(g_ForceTrackContextName));
		bank.AddButton("Debug_ForceNextTrack", CFA(Debug_ForceNextTrackCB));
		bank.AddToggle("ForceDjSpeech", &g_ForceDjSpeech);
		bank.AddText("TrackListName", &g_TrackListName[0], sizeof(g_TrackListName));
		bank.AddButton("QueueListOnStation", CFA(QueueTrackListOnStationCB));
		bank.AddToggle("ForceQueuedTrack", &g_ForceQueueTrack);
		bank.AddButton("ClearQueuedTracksOnStation", CFA(ClearQueuedTracksOnStationCB));
		bank.AddSlider("NewsStory", &g_DebugNewsStory, 0, kMaxNewsStories, 1);
		bank.AddButton("UnlockNews", CFA(UnlockNewsCB));
	bank.PopGroup();

	g_USBStationBank = &bank;
	g_USBStationGroup = bank.PushGroup("USB Station");	
	bank.AddButton("Add USB Station Widgets", CFA(AddUSBStationWidgets));
	bank.PopGroup();

	bank.PushGroup("Radio DJ", false);
		bank.AddSlider("DJ ducking level (dB)", &g_RadioDjDuckingLevelDb, 0.f, 24.f, 0.5f);
		bank.AddSlider("DJ ducking smooth time (ms)", &g_RadioDjDuckTimeMs, 0, 1000, 100);
		bank.AddSlider("DJ Speech Probability", &g_RadioDjSpeechProbability, 0.f, 1.f, 0.1f);
		bank.AddSlider("DJ Speech Probability (Kult)", &g_RadioDjSpeechProbabilityKult, 0.f, 1.f, 0.1f);
		bank.AddSlider("DJ Speech Probability (Motomami Outro)", &g_RadioDjOutroSpeechProbabilityMotomami, 0.f, 1.f, 0.1f);
		bank.AddSlider("DJ Speech Min Retrigger Time Ms (Motomami)", &g_RadioDjSpeechMinDelayMsMotomami, 0u, 100000u, 100u);		
		bank.AddSlider("DJ Time Probability", &g_RadioDjTimeProbability, 0.f, 1.f, 0.1f);
	bank.PopGroup();

	bank.AddButton("DumpNewsState", CFA(DumpNewsStateCB));
	bank.AddToggle("LogRadio", &g_LogRadio);
}

void audRadioStation::AddUSBStationWidgets()
{
	if (g_USBStationBank && g_USBStationGroup && g_NumUSBStationTrackLists > 0 && !g_HasAddedUSBStationWidgets)
	{
		bkBank& bank = *g_USBStationBank;
		bank.SetCurrentGroup(*g_USBStationGroup);
		bank.AddToggle("Debug Draw USB Station", &g_DebugDrawUSBStation);
		g_USBStationTrackListCombo = bank.AddCombo("Track List", &g_RequestedUSBTrackListComboIndex, g_NumUSBStationTrackLists, g_USBTrackListNames.GetElements());
		bank.AddSlider("Start Offset (ms)", &g_USBStationTrackListOffsetMs, 0, 3600000, 1000);
		bank.AddButton("Set Track List", CFA(SwitchUSBStationMixCB));
		bank.AddButton("Skip To Offset", CFA(USBStationSkipToOffsetCB));
		bank.AddButton("<< Skip Track", CFA(USBStationSkipPrevTrackCB));
		bank.AddButton("Skip Track >>", CFA(USBStationSkipNextTrackCB));
		bank.AddButton("Lock Track List", CFA(USBStationScriptLockTrackListCB));
		g_HasAddedUSBStationWidgets = true;
	}
}

void audRadioStation::FormatTimeString(const u32 milliseconds, char *destBuffer, const u32 destLength)
{
	float seconds = milliseconds * 0.001f;
	u32 minutes = (u32)(seconds / 60.f);
	f32 remainderSeconds = seconds - (minutes*60.f);

	formatf(destBuffer, destLength, "%02u:%02.02f", minutes, remainderSeconds);
}

void audRadioStation::DrawUSBStation() const
{
	const f32 yStepRate = 0.015f;
	f32 yCoord = 0.05f;
	f32 xCoord = 0.05f;
	u32 numTracks = GetMusicTrackListNumTracks();
	s32 playingTrack = Max(0, GetMusicTrackListActiveTrackIndex());

	char tempString[256];
	formatf(tempString, "Station %s", GetName());
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;
	
	formatf(tempString, "Last Forced Track List: %s", audNorthAudioEngine::GetMetadataManager().GetObjectName(m_LastForcedMusicTrackList));
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;

	formatf(tempString, "Playing Track %u of %u", playingTrack, numTracks);
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;

	formatf(tempString, "Tracklist Play Time: %u/%u", GetMusicTrackListCurrentPlayTimeMs(), GetMusicTrackListDurationMs());
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;
	
	for (u32 i = 0; i < numTracks; i++)
	{
		s32 startTime = GetMusicTrackListTrackStartTimeMs(i);

		formatf(tempString, "Track %u", i);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), (s32)i == playingTrack ? Color32(100, 255, 100) : Color32(255, 255, 255), tempString);
		yCoord += yStepRate;
		xCoord += 0.02f;

		const RadioStationTrackList::tTrack* track = GetMusicTracklistTrackForTimeOffset(startTime);

		if (track)
		{
			formatf(tempString, "Sound: %s", SOUNDFACTORY.GetMetadataManager().GetObjectName(track->SoundRef));
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += yStepRate;

			formatf(tempString, "Start Time: %d", startTime);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += yStepRate;
		}

		xCoord -= 0.02f;
	}

	yCoord += yStepRate;
}

void audRadioStation::DrawTakeoverStation() const
{
	const f32 yStepRate = 0.015f;
	f32 yCoord = 0.05f;
	f32 xCoord = 0.05f;

	char tempString[256];
	formatf(tempString, "Station %s", GetName());
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;

	formatf(tempString, "Random Seed: %u", GetRandomSeed());
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;

	formatf(tempString, "Last Takeover Time: %u", m_TakeoverLastTimeMs);
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;

	formatf(tempString, "Station Accumulated Playtime: %u", m_StationAccumulatedPlayTimeMs);
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;	

	formatf(tempString, "Takeover Music Track Count: %u", m_TakeoverMusicTrackCounter);
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;

	formatf(tempString, "Music Run Count: %u", m_MusicRunningCount);
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
	yCoord += yStepRate;
	
	const audRadioTrack& currentTrack = GetCurrentTrack();

	if (currentTrack.IsInitialised())
	{
		formatf(tempString, "Current Track: %s (%s track %u)", SOUNDFACTORY.GetMetadataManager().GetObjectName(currentTrack.GetSoundRef()), TrackCats_ToString((TrackCats)currentTrack.GetCategory()), currentTrack.GetTrackIndex());
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
		yCoord += yStepRate;

		formatf(tempString, "Progress: %u/%u (%.02f%%)", currentTrack.GetPlayTime(), currentTrack.GetDuration(), (currentTrack.GetPlayTime() / (f32)currentTrack.GetDuration()) * 100.f);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
		yCoord += yStepRate;
	}
	else
	{
		formatf(tempString, "Current Track: Not Initialised");
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
		yCoord += yStepRate;
	}

	const audRadioTrack& nextTrack = GetNextTrack();

	if (nextTrack.IsInitialised())
	{
		formatf(tempString, "Next Track: %s (%s track %u)", SOUNDFACTORY.GetMetadataManager().GetObjectName(nextTrack.GetSoundRef()), TrackCats_ToString((TrackCats)nextTrack.GetCategory()), nextTrack.GetTrackIndex());
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
		yCoord += yStepRate;
	}
	else
	{
		formatf(tempString, "Next Track: Not Initialised");
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
		yCoord += yStepRate;
	}

	yCoord += yStepRate;

	for (u32 category = 0; category < TRACKCATS_MAX; category++)
	{		
		if (category == RADIO_TRACK_CAT_MUSIC ||
			category == RADIO_TRACK_CAT_IDENTS ||
			category == RADIO_TRACK_CAT_TAKEOVER_DJSOLO ||
			category == RADIO_TRACK_CAT_TAKEOVER_IDENTS ||
			category == RADIO_TRACK_CAT_TAKEOVER_MUSIC ||
			category == RADIO_TRACK_CAT_DJSOLO)
		{
			s32 trackIndex = ~0u;
			
			if (const audRadioStationHistory* history = FindHistoryForCategory(category))
			{
				trackIndex = history->GetPreviousEntry();
			}
			
			if (trackIndex < 0xff)
			{
				formatf(tempString, "Category %d: %s (%d tracks available)", category, TrackCats_ToString((TrackCats)category), ComputeNumTracksAvailable(category));
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
				yCoord += yStepRate;

				formatf(tempString, "Next Valid Selection Time: %d", GetNextValidSelectionTime(category));
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
				yCoord += yStepRate;				

				formatf(tempString, "Category Stride: %d", m_CategoryStride[category]);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
				yCoord += yStepRate;				

				const RadioStationTrackList::tTrack* trackMetadata = GetTrack(category, kNullContext, trackIndex);
				formatf(tempString, "Previous Track: %s (Index: %u)", trackMetadata ? SOUNDFACTORY.GetMetadataManager().GetObjectName(trackMetadata->SoundRef) : "NULL", trackIndex);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
				yCoord += yStepRate;
			}
		
			yCoord += yStepRate;
		}
	}
}

void audRadioStation::DrawDebug(audDebugDrawManager &drawMgr) const
{
	if(ShouldOnlyPlayMusic())
	{
		drawMgr.DrawLine("MUSIC ONLY");
	}

	if(GetCurrentTrack().IsInitialised())
	{
		drawMgr.PushSection("ActiveTrack");
		GetCurrentTrack().DrawDebug(drawMgr);
		drawMgr.PopSection();
	}
	else
	{
		drawMgr.DrawLine("ActiveTrack NOT INITIALISED");
	}
	if(GetNextTrack().IsInitialised())
	{
		drawMgr.PushSection("NextTrack");
		GetNextTrack().DrawDebug(drawMgr);
		drawMgr.PopSection();
	}
	else
	{
		drawMgr.DrawLine("NextTrack NOT INITIALISED");
	}

	if(sm_DebugHistory && this == g_RadioAudioEntity.GetPlayerRadioStation())
	{
		const audRadioStationHistory *musicHistory = FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC);
		const s32 historyLength = Max<s32>(ComputeNumTracksAvailable(RADIO_TRACK_CAT_MUSIC) - kMinRadioTracksOutsideHistory, 0);
		const s32 searchLength = Clamp<s32>(historyLength, 0, audRadioStationHistory::kRadioHistoryLength);
		for(s32 i = 0; i < searchLength; i++)
		{
			s32 historyIndex = (musicHistory->GetWriteIndex() + audRadioStationHistory::kRadioHistoryLength - 1 - i) % audRadioStationHistory::kRadioHistoryLength;
			const u32 trackId = (*musicHistory)[historyIndex];

			const char *objName = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(trackId);
#if RSG_PC
			if(m_IsUserStation)
			{
				if(trackId < sm_UserRadioTrackManager.GetNumTracks())
					objName = sm_UserRadioTrackManager.GetTrackTitle(static_cast<s32>(trackId));
				else
					objName = "(invalid)";
			}
#endif
			
			drawMgr.DrawLinef("%d / %d: %08X - %s", i, historyIndex, trackId, objName ? objName : "unknown");
		}
	}

	bool isDummy = false;
	if(sm_DjSpeechMonoSound != NULL)
	{
		u32 clientVar = 0;
		sm_DjSpeechMonoSound->GetClientVariable(clientVar);
		if(clientVar == 0xdeadbeef) // using special value in client variable to denote dummy DJ speech
		{
			isDummy = true;
		}
	}

	const char *djStates[] = {"Idle","Preparing","Prepared","Playing"};
	char djSpeechWaitTimeString[32];
	FormatTimeString(sm_ActiveDjSpeechStartTimeMs >  m_Tracks[m_ActiveTrackIndex].GetPlayTime() ? sm_ActiveDjSpeechStartTimeMs - m_Tracks[m_ActiveTrackIndex].GetPlayTime() : 0, djSpeechWaitTimeString);
	drawMgr.DrawLinef("Dj Speech state: %s starting in %s%s", djStates[sm_DjSpeechState], djSpeechWaitTimeString, isDummy ? " DUMMY" : "");
}

const char *audRadioStation::GetTrackCategoryName(const s32 trackCategory)
{
	const char *ret = TrackCats_ToString((TrackCats)trackCategory);
	if(ret)
	{
		return ret+16;
	}
	return NULL;
}

#if RSG_PC
void audRadioStation::DebugDrawUserMusic()
{
	//for(int i=0; i<MAX_TRACK_HISTORY; i++)
	//{
	//	s32 trackId = sm_UserRadioTrackManager.GetTrackIdFromHistory(i);
	//	if(trackId != -1)
	//	{
	//		char trackName[256];
	//		snprintf(trackName, 128, "%s", sm_UserRadioTrackManager.GetFileName(trackId));
	//		//Displayf("%d : %s : %d", i, trackName, trackId);
	//		grcDebugDraw::PrintToScreenCoors(trackName, 10, 10+i);
	//	}
	//}
}
#endif //WIN32PC
#endif
#endif // NA_RADIO_ENABLED

