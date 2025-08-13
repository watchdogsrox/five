// 
// sagAudio/MusicPlayer.cpp 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#include "musicplayer.h"
#include "audio/northaudioengine.h"
#include "audio/frontendaudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/scriptaudioentity.h"
#include "audio/streamslot.h"
#include "musicaction.h"
#include "musicactionfactory.h"

#include "audioengine/categorycontroller.h"
#include "audioengine/categorycontrollermgr.h"
#include "audioengine/categorymanager.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiohardware/streamingwaveslot.h"
#include "audiosoundtypes/streamingsound.h"

#include "frontend/HudTools.h"
#include "system/param.h"
#include "system/controlMgr.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"

#include "audio/debugaudio.h"

#if GTA_REPLAY
#include "replaycoordinator/ReplayCoordinator.h"
#endif

AUDIO_MUSIC_OPTIMISATIONS()

PARAM(nomusic, "Disable Music");

#define MAX_SOURCE_MUSIC_DISTANCE (50.f)

enum {kMusicTimerId = 3};

audInteractiveMusicManager* audInteractiveMusicManager::sm_Instance = NULL;

#if __BANK
const char *audInteractiveMusicManager::sm_LastOneShotRequested = "";
#endif

void audInteractiveMusicManager::Instantiate()
{
	RAGE_TRACK(audInteractiveMusicManager_Instantiate);
	Assert(!sm_Instance);
	sm_Instance = rage_new audInteractiveMusicManager();
}

void audInteractiveMusicManager::Destroy()
{
	if (sm_Instance)
	{
		delete sm_Instance; 
		sm_Instance = NULL;
	}
}

#if __BANK
const char* audInteractiveMusicManager::sm_BankMusicMoodTypeStrings[] =
{
	"SILENT",
	"IDLE",
	"PASTORAL",
	"SUSPENSE",
	"DRAMATIC",
	"GUNFIGHT",
	"CHASE",
	"HAPPY",
	"EVERYTHING",
};
CompileTimeAssert(NELEM(audInteractiveMusicManager::sm_BankMusicMoodTypeStrings)==audInteractiveMusicManager::kNumBankMoodTypes);
const char* audInteractiveMusicManager::sm_BankMusicMoodVariationStrings[] =
{
	"",
	"_LOW",
	"_HIGH",
};
CompileTimeAssert(NELEM(audInteractiveMusicManager::sm_BankMusicMoodVariationStrings)==audInteractiveMusicManager::kNumBankMoodsVariations);
#endif

u32 g_StemSyncMetadataCategoryName = 0xc6a88fa1;//"STEMSYNC"
u32 g_StemNumMetadataCategoryName = 0x9544aec7;//"STEMNUM"

bool g_PlaySongsPhysically = true;

audInteractiveMusicManager::audInteractiveMusicManager() :
naAudioEntity(),
m_MusicSound(NULL),
m_OneShotSound(NULL),
m_pMusicWaveSlot(NULL),
m_pOneshotWaveSlot(NULL),
m_iTrackLengthMs(-1), //Uncomputed
m_iCurPlayTimeMs(-1), //Uncomputed
m_iFadeInTime(-1),
m_iTimeToTriggerMoodSnap(-1),
m_enMode(kModeStopped),
m_PlayingTrackSoundHash(0),
m_PendingTrackSoundHash(0),
m_TempoMapSoundHash(g_NullSoundHash),
m_ActiveOneshotHash(g_NullSoundHash),
m_pPlayingMood(NULL),
m_pScriptSetMood(NULL),
m_pMoodToSnapTo(NULL),
m_fScriptSetVolumeOffset(0.f),
m_StaticEmitterVolumeOffset(0.f),
m_pCatCtrlStems(NULL),
m_uTimeToReleaseScriptControl(UINT_MAX),
m_uTimeNoMusicPlaying(0),
m_iTimePlayerMovingFast(0),
m_iTimePlayerMovingFastThres(2000),
m_iTimePlayerMovingFastLimit(4000),
m_MarkerTimeForIsAudioSyncNow(-1),
m_uTimeWithMusicPlaying(0),
m_uNextMusicPlayTime(UINT_MAX),
m_uNextMusicStopTime(UINT_MAX),
m_uTimeInCurrentUnscriptedMood(0),
m_MusicCategory(NULL),
m_OneShotCategory(NULL),
m_MusicDuckingCategory(NULL),
m_OneShotDuckingCategory(NULL),
m_bDisabled(false),
m_bTrackIsLooping(true),
m_bJustReleasedSciptControl(false),
m_bWaitForTrackToFinishBeforeRelease(false),
m_bWrapAroundSnap(false),
m_bAudioSyncVariablesAreSet(false)
#if __BANK
, m_DrawLevelMeters(false)
#endif
{
#if __BANK
	m_bOverrideScript = false;
	formatf(m_szOverridenTrackName, sizeof(m_szOverridenTrackName), "FTR_SONG_01");
	formatf(m_szOverridenMoodName, sizeof(m_szOverridenMoodName), "");
	formatf(m_szOverridenOneshotName, sizeof(m_szOverridenOneshotName), "");
	m_szActiveTrackName[0] = '\0';
	m_szActiveSoundName[0] = '\0';
	m_szActiveMoodName[0] = '\0';
	m_szActiveOneshotName[0] = '\0';
	m_enWidgetMoodType = kBankMoodTypeIdle;
	m_enWidgetMoodVariation = kBankMoodVarBlank;
	m_fTrackSkipStartOffsetScalar = 0.f;
	m_fWidgetVolumeOffset = 0.f;
	m_iWidgetFadeInTime = -1;
	m_iWidgetFadeOutTime = -1;
	m_bUseFadeOutTimeWidget = false;
#endif
}
audInteractiveMusicManager::~audInteractiveMusicManager()
{
	Shutdown();
}

void audInteractiveMusicManager::Init()
{
	m_bDisabled = PARAM_nomusic.Get();

	naAudioEntity::Init();

	m_pMusicWaveSlot = (audStreamingWaveSlot*)audWaveSlot::FindWaveSlot(ATSTRINGHASH("INTERACTIVE_MUSIC_1", 0x09205c7a1));
	m_pOneshotWaveSlot = (audStreamingWaveSlot*)audWaveSlot::FindWaveSlot(ATSTRINGHASH("INTERACTIVE_MUSIC_2", 0x0d3594a47));

	m_PendingMoodChange = NULL;
	m_PendingMoodChangeTime = 0.f;
	m_PendingMoodVolumeOffset = 0.f;
	m_PendingMoodFadeInTime = 0;
	m_PendingMoodFadeOutTime = 0;

	m_PendingStopTrackTime = -1.f;
	m_PendingStopTrackRelease = -1;

	m_MoodState = WAITING_FOR_EXPIRY;
	m_CurrentStemMixExpiryTime = ~0U;
	m_NextStemMixMusicAction = NULL;

	m_UnfreezeRadioOnStop = false;
	m_UnfreezeRadioOnOneShot = false;
	m_MusicOverridesRadio = true;
	m_IsScoreMutedForRadio = false;
	m_MuteRadioOffSound = false;
	
	if(!audCategoryControllerManager::InstancePtr())
	{
		Quitf(ERR_AUD_MUSIC_INIT,"audInteractiveMusicManager::Init requires a properly initialized audio system");
	}

	m_MusicCategory = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("INTERACTIVE_MUSIC", 0x05db47ec6));
	m_OneShotCategory = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("MUSIC_ONESHOT", 0x020230050));
	m_MusicDuckingCategory = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("INTERACTIVE_MUSIC", 0x05db47ec6));
	m_OneShotDuckingCategory = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("MUSIC_ONESHOT", 0x020230050));
	naAssert(m_MusicCategory && m_OneShotCategory && m_MusicDuckingCategory && m_OneShotDuckingCategory);

	char categoryName[64];
	for (int i = 0; i < AUD_NUM_MUSIC_STEMS; ++i)
	{
		formatf(categoryName, "INTERACTIVE_MUSIC_%.1d", i+1);
		m_pCatCtrlStems[i] = AUDCATEGORYCONTROLLERMANAGER.CreateController(categoryName);
		if(!m_pCatCtrlStems[i])
		{
			Errorf("[audInteractiveMusicManager::Init] Category %s not found. Music will be disabled.", categoryName);
			m_bDisabled = true;
			break;
		}

#if GTA_REPLAY
		// Required during replay so that we pause the stem mix categories along with the wave players when
		// transitioning between clips
		m_pCatCtrlStems[i]->SetPauseGroup(0);		
		m_MusicCategory->SetPauseGroup(0);
#endif
	}

	m_EventManager.Init();
	m_WantedMusic.Init();
	m_IdleMusic.Init();
	m_VehicleMusic.Init();

	m_StartTrackTimer = -1.f;

	m_iTrackLengthMs = -1; //Uncomputed
	m_iCurPlayTimeMs = -1; //Uncomputed
	m_iFadeInTime = -1;
	m_iTimeToTriggerMoodSnap = -1;

	m_fAmbMusicDuckingVol = 0.0f;

	m_uLastTimeSuspenseAllowed = 0;
	m_uLastTimePlayerFiredWeapon = 0;
	m_uLastTimePlayerFiredUpon = 0;
	m_uTerritoryChangeTime = UINT_MAX;

	m_pDynMixState = NULL;
	m_uTimeToTriggerMixer = UINT_MAX;
	m_uTimeToDeTriggerMixer = 0;

	m_bTrackIsLooping = false;
	m_bJustReleasedSciptControl = false;
	m_bWrapAroundSnap = false;
	m_bAudioSyncVariablesAreSet = false;

	m_iOneShotReleaseTime = 0;
	m_iTimeToPlayOneShot = -1;
	m_iTimeToStopOneShot = -1;
	m_bWaitingToPlayOneshot = false;
	m_bStopOneshotOnNextMarker = false;
	m_bFadeOneshotOnDelayedStop = false;
	m_bOneShotWasPlayingLastFrame = false;
	m_bWaitingOnWrapAroundToPlayOneshot = false;
	m_bWaitingOnWrapAroundToStopOneshot = false;
	m_bMusicJustWrappedAround = false;

	m_ReplayWavePlayTimeAdjustment = 0;

#if GTA_REPLAY
	m_ReplayScoreIntensity = DEFAULT_MARKER_AUDIO_INTENSITY;
	m_ReplayScorePrevIntensity = DEFAULT_MARKER_AUDIO_INTENSITY;
	m_ReplayScoreSound = g_NullSoundHash;
	m_ReplayPreviewPlaying = false;
	m_ReplaySwitchMoodInstantly = false;
	m_ReplayMoodSwitchOffset = -1;
	m_ReplayMusicFadeVolumeLinear = 1.0f;

	m_ShouldRestoreSavedMusic = false;
	m_EditorWasActive = false;

    m_SavedCurrentStemMixId = 0u;
    m_SavedPlayingMood = NULL;
    m_SavedPlayingMoodVolumeOffset = 0.f;
    m_SavedMusicOverridesRadio = false;
    m_SavedMuteRadioOffSound = false;

	m_SavedPendingMoodChange = NULL;
	m_SavedPendingMoodChangeTime = 0.0f;
	m_SavedPendingMoodVolumeOffset = 0.0f;
	m_SavedPendingMoodFadeInTime = 0;
	m_SavedPendingMoodFadeOutTime = 0;
	m_SavedPendingStopTrackTime = 0.0f;
	m_SavedPendingStopTrackRelease = 0;

#endif

	m_MusicPlayState = IDLE;

	m_StaticRadioEmitterSmoother.Init(1.f / 4000.f, true);

#if __BANK
		
	m_ClockBarsBeatsText[0] = 0;
	m_ClockMinutesSecondsText[0] = 0;

	formatf(m_DebugEventName, "TEST_MUSIC_EVENT_START");
	bkBank *bankPtr = BANKMGR.FindBank("Audio");
	if(bankPtr)
	{
		AddWidgets(*bankPtr);		
	}
#endif
}

void audInteractiveMusicManager::Shutdown()
{
	InternalStopAndReset();

	naAudioEntity::Shutdown();

	AUDCATEGORYCONTROLLERMANAGER.DestroyMultipleControllers(&m_pCatCtrlStems[0], AUD_NUM_MUSIC_STEMS);
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_MusicCategory);
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_OneShotCategory);
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_MusicDuckingCategory);
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_OneShotDuckingCategory);

	m_WantedMusic.Shutdown();
	m_IdleMusic.Shutdown();
	m_VehicleMusic.Shutdown();
	m_EventManager.Shutdown();
	
	sysMemSet(&m_pCatCtrlStems, 0, sizeof(m_pCatCtrlStems));
	m_MusicCategory = NULL;
	m_OneShotCategory = NULL;
	m_MusicDuckingCategory = NULL;
	m_OneShotDuckingCategory = NULL;
}

void audInteractiveMusicManager::Reset()
{
	InternalStopAndReset(1000);
}

u32 audInteractiveMusicManager::GetTimeInMs() const
{
	return g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(kMusicTimerId);
}

void audInteractiveMusicManager::UpdateMusicTimerPause()
{
	// Music runs on it's own timer, and should continue playing when the game is paused unless the frontend menu is active or the game is paused by
	// the system (xbox dashboard etc)
#if GTA_REPLAY
	// *Don't* pause in replays if we've not got a valid music sound. Timer needs to be unpaused for sounds to be fully cleaned up, so calling
	// StopAndForget on a sound then pausing it will actually leave it hanging around and hogging waveslots - not so good when replay is attempting
	// to cache data prior to playback
	bool replayMusicPause = (m_MusicSound != NULL);

	if(g_RadioAudioEntity.IsReplayMusicPreviewPlaying())
	{
		replayMusicPause = false;
	}
	else if(CReplayMgr::IsEditModeActive() && CReplayMgr::IsPlaying())
	{
		replayMusicPause = false;
	}
#endif
	if((fwTimer::IsSystemPaused() || (CPauseMenu::IsActive() && fwTimer::IsGamePaused())) REPLAY_ONLY(&& replayMusicPause))
	{	
		g_AudioEngine.GetSoundManager().Pause(kMusicTimerId);
	}
	else
	{
		g_AudioEngine.GetSoundManager().UnPause(kMusicTimerId);
	}
}

void audInteractiveMusicManager::PreUpdateService(u32)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditorActive())
	{
		if(CReplayMgr::IsReplayInControlOfWorld()) // the mission would have been canceled so clear saved music
		{
			ClearSavedMusicState();
		}
		if(IsFrontendMusicPlaying() && !IsReplayPreviewPlaying())
		{
			SetRestoreMusic(true);
		}
		if(!IsReplayPreviewPlaying() && !g_RadioAudioEntity.IsStartingReplayPreview() && !CReplayMgr::IsReplayInControlOfWorld())
		{
			OnReplayEditorActivate();
		}
		m_EditorWasActive = true;
	}
	else
	{
		if(IsFrontendMusicPlaying() && !CReplayMgr::IsEditorActive())
		{
			SaveMusicState();
		}
	}
	if(!CReplayMgr::IsEditorActive() && m_ShouldRestoreSavedMusic && m_EditorWasActive && !CPauseMenu::IsActive())
	{
		if(!IsMusicPlaying())
		{
			RestoreMusicState();
			SaveMusicState();
			m_ShouldRestoreSavedMusic = false;
			m_EditorWasActive = false;
		}
	}
#endif

	if (m_bDisabled)
	{
		InternalStopAndReset();
		return;
	}

	UpdateMusicTimerPause();

	const u32 uTime = GetTimeInMs();

#if GTA_REPLAY
	// Once inside the editor, the only music comes from either user placed tracks or 
	// UI triggered music previews - we don't want wanted/idle/vehicle music attempting 
	// to trigger
	if(!CReplayMgr::IsEditorActive())
#endif
	{
		m_WantedMusic.Update(uTime);
		m_IdleMusic.Update(uTime);
		m_VehicleMusic.Update(uTime);
	}

	m_StartTrackTimer -= fwTimer::GetTimeStep();
	if(m_StartTrackTimer < 0.f)
	{
		m_StartTrackTimer = -1.f;
	}

	// Clamp to 0dB just incase
	m_StaticEmitterVolumeOffset = audDriverUtil::ComputeDbVolumeFromLinear(Min(1.f, m_StaticRadioEmitterSmoother.CalculateValue(IsMusicPlaying() && m_enMode != kModeStopped ? 0.f : 1.f, uTime)));

	if(m_OneShotSound)
	{
		UpdateOneshot(uTime);
		m_bOneShotWasPlayingLastFrame = true;
	}
	else if(m_bOneShotWasPlayingLastFrame)
	{
		m_iOneShotReleaseTime = 0;
		m_iTimeToStopOneShot = 0;
		m_iTimeToPlayOneShot = -1;

		m_bWaitingToPlayOneshot = false;
		m_bStopOneshotOnNextMarker = false;
		m_bFadeOneshotOnDelayedStop = false;
		m_bWaitingOnWrapAroundToPlayOneshot = false;
		m_bWaitingOnWrapAroundToStopOneshot = false;

		m_bOneShotWasPlayingLastFrame = false;
	}

	if (m_MusicSound)
	{
		UpdateMusic(uTime);
	}
	else
	{
		if (m_enMode == kModePlaying)
		{
			//	Check to see that maybe this track has stopped unexpectedly
			if (m_PlayingTrackSoundHash)
			{
				//	If we still have over 10 seconds left in the track we were playing or maybe we're looping, this must mean that it stopped unexpectedly
				const bool b10SecondsLeft = m_iTrackLengthMs - m_iCurPlayTimeMs > 10000;
				if (b10SecondsLeft || m_bTrackIsLooping)
				{
					BANK_ONLY(Warningf("[audInteractiveMusicManager] Playing track %s seems to have stopped unexpectedly. TrackLength: %i, PlayTime: %i Resuming where we left off.",
						g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_PlayingTrackSoundHash), m_iTrackLengthMs, m_iCurPlayTimeMs));

					//	If we're looping go back to the beginning if we're close to the end, otherwise stay put
					m_iCurPlayTimeMs = b10SecondsLeft && m_iCurPlayTimeMs > 0 ? m_iCurPlayTimeMs : 0;

					audSoundInitParams initParams;
					initParams.StartOffset = m_iCurPlayTimeMs;
					initParams.Volume = 0.f;//m_pPlayingTrack->TrackVolume*0.01f;
					initParams.WaveSlot = m_pMusicWaveSlot;
					initParams.PrepareTimeLimit = -1;
					initParams.AllowLoad = true;
					Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
					initParams.TimerId = kMusicTimerId;
					initParams.ShouldPlayPhysically = g_PlaySongsPhysically;				
					CreateSound_PersistentReference(m_PlayingTrackSoundHash, (audSound**)&m_MusicSound, &initParams);
					if(m_MusicSound)
					{
						// Now that we've got a valid sound, we might need to pause the timer
						UpdateMusicTimerPause();

						// And if we're in a replay then we need to start off on the correct clip volume
#if GTA_REPLAY
						if(CReplayMgr::IsEditModeActive())
						{
							m_MusicSound->GetRequestedSettings()->SetShouldPersistOverClears(true);
							UpdateReplayMusicVolume(true);
						}
#endif

						m_MusicSound->Prepare();
						m_MusicPlayState = PREPARING;
					}
				}
				else
				{
					m_PlayingTrackSoundHash = 0;
					
					m_pPlayingMood = NULL;
					m_pScriptSetMood = NULL;
					m_fScriptSetVolumeOffset = 0.f;
					m_iTrackLengthMs = -1; //Uncomputed
					m_iCurPlayTimeMs = -1; //Uncomputed
					m_iTimeToTriggerMoodSnap = -1;
					m_enMode = kModeStopped;
					m_bWrapAroundSnap = false;
					m_bAudioSyncVariablesAreSet = false;
				}
			}
		}
	}

	if(m_MusicSound && m_iTimeToTriggerMoodSnap>0)
	{
		bool TimeToSnap = false;
		if(!m_bWrapAroundSnap && m_iTimeToTriggerMoodSnap<m_iCurPlayTimeMs)
			TimeToSnap = true;
		//I think we can assume all tracks will be well over 5 seconds long.
		if(m_bWrapAroundSnap && m_iCurPlayTimeMs-m_iTimeToTriggerMoodSnap>0 && m_iCurPlayTimeMs-m_iTimeToTriggerMoodSnap<5000)
			TimeToSnap = true;

		if(TimeToSnap)
		{
			InternalCrossfadeCategoriesToMood(m_pMoodToSnapTo, 0.0f, -1, -1);
			m_iTimeToTriggerMoodSnap = -1;
			m_pMoodToSnapTo = NULL;
			m_bWrapAroundSnap = false;
		}
		
	}

	UpdateEventQueue();

#if __BANK
	//	Display the current track and mood names
	formatf(m_szActiveTrackName, sizeof(m_szActiveTrackName), m_PlayingTrackSoundHash ?
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_PlayingTrackSoundHash) : "");

	formatf(m_szActiveSoundName, sizeof(m_szActiveSoundName), m_MusicSound ? m_MusicSound->GetName() : "");

	safecpy(m_szActiveMoodName, m_pPlayingMood ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_pPlayingMood->NameTableOffset) : "",
					sizeof(m_szActiveMoodName));

	safecpy(m_szActiveOneshotName, m_OneShotSound ? sm_LastOneShotRequested : "" , sizeof(m_szActiveOneshotName));

	if(m_DrawLevelMeters && IsMusicPlaying())
	{
		static audMeterList meterList;
		static float linearVols[AUD_NUM_MUSIC_STEMS] = {0.f};
		static const char *names[AUD_NUM_MUSIC_STEMS] = {"  1","  2","  3","  4","  5","  6","  7","  8"};

		meterList.values = linearVols;		
		meterList.names = names;
		meterList.numValues = AUD_NUM_MUSIC_STEMS;
		meterList.title = "Music stem mix";

		float x0,x1,y0,y1;		
		CHudTools::GetMinSafeZone(x0,y0,x1,y1);
		x1 *= VideoResManager::GetUIWidth();
		y1 *= VideoResManager::GetUIHeight();

		meterList.width = 200.f;
		meterList.height = 200.f;
		meterList.left = x1 - meterList.width;
		meterList.bottom = y1 - 50.f;
		for (int i = 0; i < AUD_NUM_MUSIC_STEMS; ++i)
		{
			audCategoryController* categoryController = m_pCatCtrlStems[i];
			if(categoryController)
			{
				linearVols[i] = categoryController->GetVolumeLinear();
			}
		}

		audNorthAudioEngine::DrawLevelMeters(&meterList);

	}
#endif
}

bool audInteractiveMusicManager::IsScoreEnabled() const
{
	const bool musicEnabled = CPauseMenu::GetMenuPreference(PREF_INTERACTIVE_MUSIC) != 0;
	return musicEnabled;
}

void audInteractiveMusicManager::UpdateMusic(const u32& UNUSED_PARAM(uTime))
{
	if (m_enMode != kModePlaying)
	{
		if (m_MusicCategory->GetVolumeLinear()==0.f)
		{
			if(m_MusicSound)
			{
				m_MusicSound->StopAndForget();
				REPLAY_ONLY(m_ReplayScoreSound = g_NullSoundHash);
			}
		}
	}
	else
	{
		const int iFadeTime = m_iFadeInTime>=0 ? m_iFadeInTime : (m_pPlayingMood ? m_pPlayingMood->FadeInTime : 3000);
		
		switch (m_MusicPlayState)
		{
		case IDLE:		
			break;
		case PREPARING:
		case PREPARED:
			if(m_MusicSound->Prepare() == AUD_PREPARED)
			{
				m_MusicPlayState = PREPARED;				

#if GTA_REPLAY
				bool replayPaused = CReplayMgr::IsEditModeActive() && !CReplayMgr::IsPlaying();
				bool loadingScreenActive = CReplayCoordinator::IsExportingToVideoFile() ? CReplayCoordinator::ShouldShowLoadingScreen() : CVideoEditorPlayback::IsLoading();

				if(CReplayMgr::IsEditModeActive())
				{
					if(m_TempoMapSoundHash != m_PlayingTrackSoundHash)
					{
						ParseMarkers();
					}
				}
#endif

				if (m_StartTrackTimer <= 0.f REPLAY_ONLY(&& !replayPaused && !loadingScreenActive && !g_AudioEngine.GetSoundManager().IsPaused(kMusicTimerId)))
				{
					m_MusicPlayState = PLAYING;
					m_MusicSound->Play();					
					UpdateMusicTimerPause();
					REPLAY_ONLY(UpdateReplayMusicPreview();)
					ParseMarkers();

					// Override radio if its playing
					if(IsScoreEnabled())
					{
						if(m_MusicOverridesRadio && (g_RadioAudioEntity.IsVehicleRadioOn() || g_RadioAudioEntity.IsRetuningVehicleRadio()))
						{
							if(m_MuteRadioOffSound)
							{
								m_MuteRadioOffSound = false;
								g_RadioAudioEntity.MuteNextOnOffSound();
							}
							g_RadioAudioEntity.SwitchOffRadio();	
						}

						audDisplayf("Score starting (m_IsScoreMutedForRadio = false)");
						m_IsScoreMutedForRadio = false;
						m_MusicCategory->BeginGainFadeLinear(1.f, iFadeTime*0.001f);
						// Also reset one shot category volume since we're clearing the 'WasPlayerRadioActive' flag.
						m_OneShotCategory->SetVolumeLinear(1.f);
					}

					m_iFadeInTime = -1;

					//	Allow fall-through to PLAYING state
				}
				else
				{
					REPLAY_ONLY(UpdateReplayMusicVolume(true);)
					break;
				}
			}
		case PLAYING:

			// If the user switches on frontend radio then keep playing interactive music, but mute the category
			// this ensures the scripted events continue to process, and we can fade it back up at any time.
#if GTA_REPLAY
			if(m_ReplayPreviewPlaying)
			{
				// We definitely don't want the music category muted if we're trying to play a replay preview
				if(m_IsScoreMutedForRadio)
				{
					m_MusicCategory->BeginGainFadeLinear(1.f, 4.f);
					m_OneShotCategory->BeginGainFadeLinear(1.f, 4.f);

					audDisplayf("Replay preview playing - allowing score to play (m_IsScoreMutedForRadio = false)");
					m_IsScoreMutedForRadio = false;
				}				
			}
			else 
#endif
			if(!IsScoreEnabled() || 
				(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowScoreAndRadio) && (g_RadioAudioEntity.IsPlayerRadioActive() || g_RadioAudioEntity.IsRetuning()) && !g_RadioAudioEntity.IsRadioFadedOut()))
			{
				if(!m_IsScoreMutedForRadio)
				{
					m_MusicCategory->BeginGainFadeLinear(0.f, 0.5f);
					m_OneShotCategory->BeginGainFadeLinear(0.f, 0.5f);

					audDisplayf("Player radio activated - muting score for radio (m_IsScoreMutedForRadio = true)");
					m_IsScoreMutedForRadio = true;
				}
			}
			else if(m_IsScoreMutedForRadio)
			{
				// If we were muted for radio, wait until we are out of the car to fade up
				CPed *player = CGameWorld::FindLocalPlayer();
				if(!player || !player->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
				{
					m_MusicCategory->BeginGainFadeLinear(1.f, 4.f);
					m_OneShotCategory->BeginGainFadeLinear(1.f, 4.f);

					audDisplayf("Player radio deactivated - allowing score to play (m_IsScoreMutedForRadio = false)");
					m_IsScoreMutedForRadio = false;
				}				
			}

			// Optionally apply radio ducking to score
			if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::SpeechDucksScore) || g_ScriptAudioEntity.ShouldDuckScoreThisLine())
			{
				m_MusicDuckingCategory->SetVolumeLinear(audNorthAudioEngine::GetFrontendRadioVolumeLin());
				m_OneShotDuckingCategory->SetVolumeLinear(audNorthAudioEngine::GetFrontendRadioVolumeLin());
			}
			else
			{
				m_MusicDuckingCategory->SetVolumeLinear(1.f);
				m_OneShotDuckingCategory->SetVolumeLinear(1.f);
			}

			m_iTrackLengthMs = m_MusicSound->ComputeDurationMsExcludingStartOffsetAndPredelay(NULL);
			const s32 soundPlaytimeMs = m_ReplayWavePlayTimeAdjustment + m_MusicSound->GetCurrentPlayTimeOfWave();
			const s32 newCurPlayTimeMs = m_bTrackIsLooping ? (soundPlaytimeMs % m_iTrackLengthMs) : Max(m_iCurPlayTimeMs, soundPlaytimeMs);
			m_bMusicJustWrappedAround = newCurPlayTimeMs < m_iCurPlayTimeMs;
			
			// Adjust our pending track stop time to cope with track looping around
			const float trackLengthS = m_iTrackLengthMs * 0.001f;
			if(m_bMusicJustWrappedAround)
			{
				if(m_PendingStopTrackTime >= 0.f && m_PendingStopTrackTime >= trackLengthS)
				{
					m_PendingStopTrackTime -= trackLengthS;
					audDisplayf("Music track has wrapped around - adjusting pending stop track time by -%f (now %f)", m_PendingStopTrackTime, trackLengthS);
				}
				else
				{
					audDisplayf("Music track has wrapped around - no pending stop track time to adjust");
				}

				if(m_CurrentStemMixExpiryTime != ~0u && m_CurrentStemMixExpiryTime >= m_iTrackLengthMs)
				{
					m_CurrentStemMixExpiryTime -= m_iTrackLengthMs;
					audDisplayf("Music track has wrapped around - adjusting stem mix expiry time by -%u (now %u)", m_iTrackLengthMs, m_CurrentStemMixExpiryTime);
				}
				else
				{
					audDisplayf("Music track has wrapped around - no stem mix expiry time to adjust");
				}
			}
			
			m_iCurPlayTimeMs = newCurPlayTimeMs;

			const float playTimeS = m_iCurPlayTimeMs*0.001f;

			UpdateMoodState(playTimeS);

			if(m_PendingMoodChange != NULL && playTimeS >= m_PendingMoodChangeTime)
			{				
				InternalSetMood(m_PendingMoodChange, m_PendingMoodVolumeOffset, m_PendingMoodFadeInTime, m_PendingMoodFadeOutTime);
				m_PendingMoodChange = NULL;			
			}

			if(m_PendingStopTrackTime >= 0.f)
			{
				// Unfreeze the radio a little before starting the music fade out
				if(m_UnfreezeRadioOnStop && playTimeS >= m_PendingStopTrackTime - 0.2f)
				{	
					g_RadioAudioEntity.SetAutoUnfreezeForPlayer(true);
					m_UnfreezeRadioOnStop = false;				
				}
				if(playTimeS >= m_PendingStopTrackTime)
				{
					InternalStopAndReset(m_PendingStopTrackRelease);
					m_PendingStopTrackTime = -1.f;
				}
			}

#if __BANK
			if(m_TempoMap.HasEntries())
			{
				const float playTimeS = newCurPlayTimeMs * 0.001f;
				audTempoMap::TimelinePosition pos;
				m_TempoMap.ComputeTimelinePosition(playTimeS, pos);
				formatf(m_ClockBarsBeatsText, "%u : %u : %.2f", pos.Bar, pos.Beat, pos.BeatOffsetS);

				const float clockS = m_TempoMap.ComputeTimeForPosition(pos);

				const s32 minutes = static_cast<s32>(Floorf(clockS / 60.f));
				const s32 seconds = static_cast<s32>(Floorf(clockS - (minutes * 60.f)));
				const s32 milliseconds = static_cast<s32>(1000.f * (clockS - Floorf(clockS)));

				formatf(m_ClockMinutesSecondsText, "%02d : %02d : %03d", minutes, seconds, milliseconds);				
			}
#endif

			break;
		}
	}
}

void audInteractiveMusicManager::UpdateOneshot(const u32& UNUSED_PARAM(uTime))
{
	if(m_bMusicJustWrappedAround)
	{
		m_bWaitingOnWrapAroundToPlayOneshot = false;
		m_bWaitingOnWrapAroundToStopOneshot = false;
	}

	if(!m_bWaitingOnWrapAroundToPlayOneshot && m_iTimeToPlayOneShot!=-1)
	{
		if(m_iCurPlayTimeMs == -1 || m_iTimeToPlayOneShot < m_iCurPlayTimeMs)
		{
			m_bWaitingToPlayOneshot = true;
			m_iTimeToPlayOneShot = -1;
		}
	}

	if(m_bWaitingToPlayOneshot && IsOneShotPrepared())
	{
		PlayPreloadedOneShot();
		m_bWaitingToPlayOneshot = false;
		m_iTimeToPlayOneShot = -1;
	}

	if(m_bStopOneshotOnNextMarker && ! m_bWaitingOnWrapAroundToStopOneshot && m_iTimeToStopOneShot < m_iCurPlayTimeMs)
	{
		StopOneShot(m_bFadeOneshotOnDelayedStop);
		m_iTimeToStopOneShot = -1;
		m_bStopOneshotOnNextMarker = false;
	}
}

void audInteractiveMusicManager::UpdateEventQueue()
{
	while (!m_EventQueue.IsEmpty())
	{
		tMusicEvent& musEvent = m_EventQueue.Top();

		//	TO AVOID AN INFINITE LOOP, YOU MUST DROP() AND/OR RETURN AT THE END OF EACH SWITCH CASE!!!!!
		switch (musEvent.m_EventType)
		{
		case kEventDoNothing:
			m_EventQueue.Drop();
			break;
		case kEventPrepareTrack:
			InternalPrepareTrack(musEvent.m_arg1.u, //track
				(const rage::InteractiveMusicMood*)musEvent.m_arg2.p, //mood
				musEvent.m_arg3.f, //startOffset
				true,//looping
				musEvent.m_arg4.f); //delayTme

			m_MusicOverridesRadio = (musEvent.m_arg5.u != 0);
			m_MuteRadioOffSound = (musEvent.m_arg5.u == 2);
			m_EventQueue.Drop();
			break;
		case kEventSnapCategoriesToMood:
			InternalSnapCategoriesToMood((const rage::InteractiveMusicMood*)musEvent.m_arg1.p, musEvent.m_arg2.f);
			m_EventQueue.Drop();
			break;
		case kEventCrossFadeCategoriesToMood:
			InternalCrossfadeCategoriesToMood((const rage::InteractiveMusicMood*)musEvent.m_arg1.p, musEvent.m_arg2.f, musEvent.m_arg3.i, musEvent.m_arg4.i);
			m_EventQueue.Drop();
			break;
		case kEventWaitForWaveslotFree:
			if (!m_MusicSound && m_pMusicWaveSlot->GetReferenceCount() == 0)
			{
				m_EventQueue.Drop();
				break;
			}
			return;
		case kEventWaitForTime:
			musEvent.m_arg1.f -= TIME.GetUnwarpedSeconds();
			if (musEvent.m_arg1.f <= 0.f)
			{
				m_EventQueue.Drop();
				break;
			}
			return;
		default:
			Assertf(0, "[audInteractiveMusicManager] Unhandled event type: %i", musEvent.m_EventType);
			m_EventQueue.Drop();
			break;
		}
	}
}

void audInteractiveMusicManager::PlayLoopingTrack(const u32 soundNameHash, const InteractiveMusicMood *pMood,
									   f32 fVolumeOffset, s32 iFadeInTime, s32 iFadeOutTime, f32 fStartOffsetScalar, f32 startDelay, bool overrideRadio, bool muteRadioOffSound)
{
	if(!pMood)
		return;
	
	tMusicEvent musEvent;

	bool bQueueNewTrack = false;
	if ((soundNameHash != m_PlayingTrackSoundHash && soundNameHash != m_PendingTrackSoundHash) || m_enMode == kModeSuspended)
	{
		InternalStopAndReset( iFadeOutTime>=0 ? iFadeOutTime : (m_pPlayingMood ? m_pPlayingMood->FadeOutTime : 2000) );

		musEvent.m_EventType = kEventWaitForWaveslotFree;
		m_EventQueue.Push(musEvent);
		bQueueNewTrack = true;
	}

	SetMood(pMood, 0.f, fVolumeOffset, iFadeInTime, iFadeOutTime);

	m_pScriptSetMood = pMood;
	m_fScriptSetVolumeOffset = fVolumeOffset;
	
	if (bQueueNewTrack)
	{
		fStartOffsetScalar = Selectf(fStartOffsetScalar, Min(fStartOffsetScalar, 1.f), audEngineUtil::GetRandomNumberInRange(0.f, 0.75f)); //Not 0 - 1.0 because we don't want to start way at the end
		musEvent.m_EventType = kEventPrepareTrack;
		musEvent.m_arg1.u = soundNameHash;
		musEvent.m_arg2.p = pMood;
		musEvent.m_arg3.f = fStartOffsetScalar;
		musEvent.m_arg4.f = startDelay;
			
		musEvent.m_arg5.u = (overrideRadio ? (muteRadioOffSound ? 2 : 1) : 0);
		m_iFadeInTime = iFadeInTime;
		m_EventQueue.Push(musEvent);
		m_PendingTrackSoundHash = soundNameHash;	
	}
}

bool audInteractiveMusicManager::IsMusicPrepared()
{
	return m_MusicSound && m_MusicSound->Prepare() == AUD_PREPARED;
}

void audInteractiveMusicManager::SetMood(const InteractiveMusicMood *mood, const float triggerTime, const float volumeOffset, const s32 fadeInTime, const s32 fadeOutTime)
{
	if (mood)
	{
		if(triggerTime == 0.0f)
		{
			InternalSetMood(mood, volumeOffset, fadeInTime, fadeOutTime);
			m_PendingMoodChange = NULL;
		}
		else
		{
			m_PendingMoodChange = mood;
			m_PendingMoodChangeTime = triggerTime;
			m_PendingMoodVolumeOffset = volumeOffset;
			m_PendingMoodFadeInTime = fadeInTime;
			m_PendingMoodFadeOutTime = fadeOutTime;
		}
	}
}

void audInteractiveMusicManager::NotifyWeaponFired()
{
	m_uLastTimePlayerFiredWeapon = fwTimer::GetTimeInMilliseconds();
}
void audInteractiveMusicManager::NotifyFiredUponPlayer()
{
	m_uLastTimePlayerFiredUpon = fwTimer::GetTimeInMilliseconds();
}
void audInteractiveMusicManager::NotifyTerritoryChanged()
{
	m_uTerritoryChangeTime = fwTimer::GetTimeInMilliseconds()+10000;
}

void audInteractiveMusicManager::StopAndReset(const s32 releaseTime /* = -1 */, const float triggerTime /* = -1.f */)
{
	if(triggerTime < 0.0f)
	{
		m_PendingStopTrackTime = -1.f;
		InternalStopAndReset(releaseTime); 

		if(m_UnfreezeRadioOnStop)
		{
			g_RadioAudioEntity.SetAutoUnfreezeForPlayer(true);
			m_UnfreezeRadioOnStop = false;	
		}
	}
	else
	{
		m_PendingStopTrackTime = triggerTime;
		m_PendingStopTrackRelease = releaseTime;
	}
}

bool audInteractiveMusicManager::PreloadOneShot(const u32 soundNameHash, const s32 iPredelay, const s32 iFadeInTime, 
													const s32 iFadeOutTime, const bool playWhenPrepared /* = false */)
{
#if __BANK
	const char *soundName = SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash);
#endif

	if(m_OneShotSound)
	{
		Assertf(!m_OneShotSound, "Unable to play oneshot %s.  There is already a oneshot playing (%s).", soundName, m_OneShotSound->GetName());
		return false;
	}
	
	audSoundInitParams initParams;
	initParams.AttackTime = iFadeInTime!=-1 ? iFadeInTime : 0;
	m_iOneShotReleaseTime = iFadeOutTime;
	initParams.PrepareTimeLimit = -1;
	initParams.WaveSlot = m_pOneshotWaveSlot;
	Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	initParams.TimerId = kMusicTimerId;
	initParams.Predelay = iPredelay;	
	m_ActiveOneshotHash = soundNameHash;
	CreateSound_PersistentReference(soundNameHash, &m_OneShotSound, &initParams);
	if(!m_OneShotSound)
	{
		Assertf(0, "Failed to prepare sound for oneshot.");
		return false;
	}

	const int prepareState = m_OneShotSound->Prepare(m_pOneshotWaveSlot, true);

#if __BANK
	sm_LastOneShotRequested = soundName;
#endif	
	
	if(prepareState == AUD_PREPARE_FAILED)
	{
		Assertf(0, "Failed to prepare oneshot %s.", sm_LastOneShotRequested);
		return false;
	}

	m_bWaitingToPlayOneshot = playWhenPrepared;
	
	return true;
}

void audInteractiveMusicManager::PlayTimedOneShot(const float triggerTime)
{
	if(audVerifyf(m_OneShotSound, "Trying to play a one shot without preparing it first"))
	{
		m_iTimeToPlayOneShot = static_cast<s32>(triggerTime * 1000.f);
	}
}

bool audInteractiveMusicManager::IsOneShotPrepared()
{
	if(!m_OneShotSound)
		return false;

	const int prepareState = m_OneShotSound->Prepare(m_pOneshotWaveSlot, true);
	if(prepareState == AUD_PREPARE_FAILED)
	{
#if __BANK
		naWarningf("Failed to prepare audio track for oneshot %s.", sm_LastOneShotRequested);
#endif
	}

	return (prepareState != AUD_PREPARING);
}

void audInteractiveMusicManager::PlayPreloadedOneShot()
{
	if(m_OneShotSound)
	{
		m_OneShotSound->Play();
	}
	m_bWaitingToPlayOneshot = false;

	if(m_UnfreezeRadioOnOneShot)
	{
		g_RadioAudioEntity.SetAutoUnfreezeForPlayer(true);
		m_UnfreezeRadioOnOneShot = false;
	}
}

void audInteractiveMusicManager::StopOneShot(bool bIgnoreFadeTime
#if __BANK
	 , bool fromWidget
#endif
	 )
{
	if(!m_OneShotSound)
	{
#if __BANK
		naWarningf("Attempting to stop oneshot when no oneshot is currently playing.");
#endif
		return;
	}

	/*bool bMarkedForStop = false;
	if(bStopOnMarker)
	{
		m_bStopOneshotOnNextMarker = true;
		m_bFadeOneshotOnDelayedStop = bIgnoreFadeTime;
		
		if(Verifyf(m_pPlayingMood, "[audInteractiveMusicManager::GetTimeToStopOneshot] No mood is currently set."))
		{
			bMarkedForStop = true;
		}
	}*/
	
	//if(!bMarkedForStop)
	{
		if(m_OneShotSound)
		{
			m_OneShotSound->SetReleaseTime(bIgnoreFadeTime ? 0 : BANK_ONLY(fromWidget ? m_iWidgetFadeOutTime : ) m_iOneShotReleaseTime);
			m_OneShotSound->StopAndForget();
			m_iOneShotReleaseTime = 0;
		}
	}

	// Also reset a timed trigger
	m_iTimeToPlayOneShot = -1;
}

bool audInteractiveMusicManager::OnStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop this oneshot.
	if(m_OneShotSound)
		m_OneShotSound->StopAndForget();

	return true;
}

bool audInteractiveMusicManager::HasStoppedCallback(u32 UNUSED_PARAM(userData))
{
	return !m_OneShotSound;
}



#if __BANK

void audInteractiveMusicManager::AddWidgets(rage::bkBank& bank/*, rage::bkGroup* pGroup*/)
{
	//audGtaAudioEntity::AddWidgets(bank, pGroup);	

	if(bank.FindChild("Interactive Music"))
	{
		return;
	}

	bank.PushGroup("Interactive Music",false);
	{
		m_EventManager.AddWidgets(bank);
		bank.AddToggle("Disable", &m_bDisabled);
		bank.AddToggle("OverrideScript", &m_bOverrideScript);
		bank.AddToggle("g_PlaySongsPhysically", &g_PlaySongsPhysically);
		bank.AddText("Override Track", m_szOverridenTrackName, sizeof(m_szOverridenTrackName));
		bank.AddText("Override Mood", m_szOverridenMoodName, sizeof(m_szOverridenMoodName));
		bank.AddText("Override Track", m_szOverridenOneshotName, sizeof(m_szOverridenOneshotName));
		bank.AddCombo("MoodType", (int*)&m_enWidgetMoodType, kNumBankMoodTypes, sm_BankMusicMoodTypeStrings);
		bank.AddCombo("MoodVariation", (int*)&m_enWidgetMoodVariation, kNumBankMoodsVariations, sm_BankMusicMoodVariationStrings);		
		bank.AddText("TimeNoMusicPlaying", (int*)&m_uTimeNoMusicPlaying, true);
		bank.AddText("TimeWithMusicPlaying", (int*)&m_uTimeWithMusicPlaying, true);
		bank.AddText("NextMusicPlayTime", (int*)&m_uNextMusicPlayTime, true);
		bank.AddText("Static Emitter Vol Offset", (float*)&m_StaticEmitterVolumeOffset, true);
		bank.AddText("Music Mode", (int*)&m_enMode, true);
		bank.AddText("NextMusicStopTime", (int*)&m_uNextMusicStopTime, true);
		bank.AddText("TimePlayerMovingFast", &m_iTimePlayerMovingFast, true);
		bank.AddSlider("TimePlayerMovingFastLimit", &m_iTimePlayerMovingFastLimit, 0, 3600000, 1);
		bank.AddSlider("TimePlayerMovingFastThres", &m_iTimePlayerMovingFastThres, 0, 3600000, 1);
		bank.AddButton("StopOneShot", datCallback(MFA(audInteractiveMusicManager::WidgetStopOneshot),this) );
		bank.AddButton("StopTrackLooping", datCallback(MFA(audInteractiveMusicManager::WidgetStopAndReset),this) );
		bank.AddToggle("Use FadeOutTime Widget When Stopping", &m_bUseFadeOutTimeWidget);
		bank.AddButton("Reset", datCallback(MFA1(audInteractiveMusicManager::StopAndReset),this,0) );
		bank.AddButton("TrackSkip", datCallback(MFA(audInteractiveMusicManager::WidgetTrackSkip),this) );
		bank.AddSlider("VolumeOffset", &m_fWidgetVolumeOffset, -100.f, +12.f, 1.f);
		bank.AddSlider("FadeInTime", &m_iWidgetFadeInTime, -1, 120000, 1);
		bank.AddSlider("FadeOutTime", &m_iWidgetFadeOutTime, -1, 120000, 1);
		bank.AddSlider("SkipStartOffsetPercentage", &m_fTrackSkipStartOffsetScalar, -1.f, 1.f, 0.25f);

	}bank.PopGroup();

	bank.PushGroup("V Music Interface");
	bank.AddText("Active Track", m_szActiveTrackName, sizeof(m_szActiveTrackName), true);
	bank.AddText("Active Sound", m_szActiveSoundName, sizeof(m_szActiveSoundName), true);
	bank.AddText("Active Mood", m_szActiveMoodName, sizeof(m_szActiveMoodName), true);
	bank.AddText("Prepared/Playing Oneshot", m_szActiveOneshotName, sizeof(m_szActiveOneshotName), true);
	bank.AddText("TrackLengthMs", &m_iTrackLengthMs, true);
	bank.AddText("CurPlayTimeMs", &m_iCurPlayTimeMs, true);
	bank.AddText("CurStemMixExpiryTimeMs", (s32*)&m_CurrentStemMixExpiryTime, true);
	bank.AddText("Clock_BarsBeats", m_ClockBarsBeatsText,sizeof(m_ClockBarsBeatsText), true);
	bank.AddText("Clock_MinutesSeconds", m_ClockMinutesSecondsText,sizeof(m_ClockMinutesSecondsText), true);

	bank.AddToggle("DrawLevels", &m_DrawLevelMeters);
	bank.AddText("EventName", m_DebugEventName, sizeof(m_DebugEventName));
	bank.AddButton("PrepareEvent", datCallback(MFA(audInteractiveMusicManager::DebugPrepareEvent), this));
	bank.AddButton("TriggerEvent", datCallback(MFA(audInteractiveMusicManager::DebugTriggerEvent), this));
	bank.AddButton("CancelEvent", datCallback(MFA(audInteractiveMusicManager::DebugCancelEvent), this));
	bank.AddButton("Stop Music", datCallback(MFA(audInteractiveMusicManager::DebugStopTrack), this));
	bank.PopGroup();
}

void audInteractiveMusicManager::WidgetStopOneshot()
{
	if(m_bUseFadeOutTimeWidget)
		StopOneShot(false, true);
	else
		StopOneShot();
}

void audInteractiveMusicManager::WidgetStopAndReset()
{
	if(m_bUseFadeOutTimeWidget)
		InternalStopAndReset(m_iWidgetFadeOutTime);
	else
		InternalStopAndReset();
}
void audInteractiveMusicManager::WidgetTrackSkip()
{
	if (m_MusicSound && m_iTrackLengthMs>0)
	{
		const float fSkipScale = Selectf(m_fTrackSkipStartOffsetScalar, m_fTrackSkipStartOffsetScalar, audEngineUtil::GetRandomNumberInRange(0.f, 0.75f));
		m_iCurPlayTimeMs = s32(fSkipScale * m_iTrackLengthMs);
		m_MusicSound->StopAndForget();
	}
}

void audInteractiveMusicManager::DebugPrepareEvent()
{
	const audMusicAction::PrepareState prepareState = m_EventManager.PrepareEvent(m_DebugEventName, ~0U);
	const char *stateNames[] = {"PREPARED", "PREPARING", "PREPARE_FAILED"};
	audDisplayf("DebugPrepareEvent: %s: %s", m_DebugEventName, stateNames[prepareState]);
}

void audInteractiveMusicManager::DebugCancelEvent()
{
	const bool success = m_EventManager.CancelEvent(m_DebugEventName);
	audDisplayf("DebugCancelEvent: %s: %s", m_DebugEventName, success ? "succeeded" : "failed");
}

void audInteractiveMusicManager::DebugTriggerEvent()
{
	const bool success = m_EventManager.TriggerEvent(m_DebugEventName);
	audDisplayf("DebugTriggerEvent: %s: %s", m_DebugEventName, success ? "succeeded" : "failed");
}

void audInteractiveMusicManager::DebugStopTrack()
{
	const bool success = m_EventManager.TriggerEvent("debug_stop_music_event");
	audDisplayf("DebugStopTrack: %s", success ? "succeeded" : "failed - check debug_stop_music_event is valid in RAVE");
}

void audInteractiveMusicManager::DebugDraw()
{
	m_EventManager.DebugDraw();
}

#endif //__BANK

void audInteractiveMusicManager::InternalPrepareTrack(const u32 soundNameHash, const rage::InteractiveMusicMood* pMood, float fStartOffsetScalar, bool bLooping, f32 delayTime)
{
	if(m_MusicSound)
	{
		Assertf(0, "[audInteractiveMusicManager::InternalPlayScriptedTrack] We should not be playing anything if we get here.");
		m_MusicSound->StopAndForget();
	}

	//	Mute the master category so the music can fade up
	m_MusicCategory->SetVolumeLinear(0.f);

	audSoundInitParams initParams;

	const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(soundNameHash);
	if (streamingSound)
	{		
		//	Compute our actual start offset
		initParams.StartOffset = s32(fStartOffsetScalar * streamingSound->Duration);
	}
	
	initParams.PrepareTimeLimit = -1;
	initParams.WaveSlot = m_pMusicWaveSlot;
	initParams.AllowLoad = true;
	Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	initParams.TimerId = kMusicTimerId;
	initParams.ShouldPlayPhysically = g_PlaySongsPhysically;
	CreateSound_PersistentReference(soundNameHash, (audSound**)&m_MusicSound, &initParams);
	if (Verifyf(m_MusicSound, "[audInteractiveMusicManager::InternalPrepareTrack] Failed to create sound"))
	{
		// Now that we've got a valid sound, we might need to pause the timer
		UpdateMusicTimerPause();

		// And if we're in a replay then we need to start off on the correct clip volume
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			m_MusicSound->GetRequestedSettings()->SetShouldPersistOverClears(true);
			UpdateReplayMusicVolume(true);
		}
#endif

		m_MusicSound->Prepare();
		m_MusicPlayState = PREPARING;

		m_PlayingTrackSoundHash = soundNameHash;
		m_PendingTrackSoundHash = 0;
		
		m_pPlayingMood = pMood;
		m_bTrackIsLooping = bLooping;
		m_StartTrackTimer = delayTime;
		m_enMode = kModePlaying;

#if GTA_REPLAY
		if(!CReplayMgr::IsEditorActive())
		{
			SaveMusicState();
		}
#endif
	}
	m_iTrackLengthMs = -1; //Uncomputed
	m_iCurPlayTimeMs = -1; //Uncomputed
	m_iTimeToTriggerMoodSnap = -1;
	m_bWrapAroundSnap = false;
}

void audInteractiveMusicManager::InternalSetMood(const rage::InteractiveMusicMood* mood, f32 volumeOffset, s32 fadeInTime, s32 fadeOutTime)
{
	if(m_pPlayingMood REPLAY_ONLY(&& (!CReplayMgr::IsEditModeActive() || !m_ReplaySwitchMoodInstantly)))
	{
		InternalCrossfadeCategoriesToMood(mood, volumeOffset, fadeInTime, fadeOutTime);
	}
	else
	{
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive() && m_ReplayScorePrevIntensity != MARKER_AUDIO_INTENSITY_MAX)
		{
			const char *moodNames[MARKER_AUDIO_INTENSITY_MAX] = {
				"VID_XS_MD",
				"VID_S_MD",
				"VID_M_MD",
				"VID_L_MD",
				"VID_XL_MD"
			};

			rage::InteractiveMusicMood* previousMood = audNorthAudioEngine::GetObject<InteractiveMusicMood>(moodNames[m_ReplayScorePrevIntensity]);
			InternalSnapCategoriesToMood(previousMood, volumeOffset);

			for (int i = 0; i < AUD_NUM_MUSIC_STEMS; ++i)
			{
				audCategoryController* pCat = m_pCatCtrlStems[i];							
				pCat->Update(0.0f);
			}

			InternalCrossfadeCategoriesToMood(mood, volumeOffset, fadeInTime, fadeOutTime, m_ReplayMoodSwitchOffset);
		}
		else		
#endif		
		{
			InternalSnapCategoriesToMood(mood, volumeOffset);
		}		
	}	

#if GTA_REPLAY
	m_ReplayScorePrevIntensity = MARKER_AUDIO_INTENSITY_MAX;
	m_ReplaySwitchMoodInstantly = false;
#endif
}

void audInteractiveMusicManager::InternalSnapToMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset)
{
	tMusicEvent musEvent;
	musEvent.m_EventType = kEventSnapCategoriesToMood;
	musEvent.m_arg1.p = pMood;
	musEvent.m_arg2.f = fVolumeOffset;
	m_EventQueue.Push(musEvent);
}

void audInteractiveMusicManager::ResetMoodState()
{
	m_CurrentStemMixId = 0;
	if(m_pPlayingMood->numStemMixes > 1)
	{
		const float mixDuration = audEngineUtil::GetRandomNumberInRange(m_pPlayingMood->StemMixes[0].MinDuration, m_pPlayingMood->StemMixes[0].MaxDuration);
		m_CurrentStemMixExpiryTime = (m_MusicSound? (m_ReplayWavePlayTimeAdjustment + m_MusicSound->GetCurrentPlayTimeOfWave()) : 0) + static_cast<u32>(mixDuration*1000.f);
	}
	else
	{
		m_CurrentStemMixExpiryTime = ~0U;
	}
	if(m_NextStemMixMusicAction)
	{
		delete m_NextStemMixMusicAction;
		m_NextStemMixMusicAction = NULL;
	}
	m_MoodState = WAITING_FOR_EXPIRY;
}

void audInteractiveMusicManager::UpdateMoodState(const float currentPlayTime)
{
	const InteractiveMusicMood *mood = m_pPlayingMood;

	if(mood && mood->numStemMixes > 1)
	{	
		switch(m_MoodState)
		{
		case WAITING_FOR_EXPIRY:
			{
				const u32 nextMixId = (m_CurrentStemMixId + 1) % mood->numStemMixes;
				if(m_NextStemMixMusicAction == NULL && mood->StemMixes[nextMixId].OnTriggerAction.IsValid())
				{
					m_NextStemMixMusicAction = audMusicActionFactory::Create(mood->StemMixes[nextMixId].OnTriggerAction);
				}
				if(m_NextStemMixMusicAction && !m_OneShotSound)
				{
					m_NextStemMixMusicAction->Prepare();
				}
				if(m_MusicSound && (m_ReplayWavePlayTimeAdjustment + m_MusicSound->GetCurrentPlayTimeOfWave()) >= m_CurrentStemMixExpiryTime)
				{
					
					if(mood->StemMixes[nextMixId].numTimingConstraints > 0)
					{
						const audMetadataManager &mgr = audNorthAudioEngine::GetMetadataManager();
						atFixedArray<const MusicTimingConstraint *, MusicAction::MAX_TIMINGCONSTRAINTS> constraints;
						for(u32 i = 0; i < mood->StemMixes[nextMixId].numTimingConstraints; i++)
						{
							constraints.Append() = reinterpret_cast<const MusicTimingConstraint *>(mgr.GetObjectMetadataPtr(mood->StemMixes[nextMixId].TimingConstraints[i].Constraint));
						}
						m_TrackTimeToSwitchStemMix = ComputeNextPlayTimeForConstraints(constraints);
					}
					else
					{
						m_TrackTimeToSwitchStemMix = 0.f;
					}

					if(m_NextStemMixMusicAction)
					{
						audDisplayf("Triggered StemMix action");
						m_NextStemMixMusicAction->Trigger();
						delete m_NextStemMixMusicAction;
						m_NextStemMixMusicAction = NULL;
					}

					m_MoodState = WAITING_FOR_CONSTRAINT;

					audDisplayf("Stem mix / mood state: WAITING_FOR_CONSTRAINT: %f seconds", m_TrackTimeToSwitchStemMix);

					// intentional fall-through
				}
				else
					break;
			}
		case WAITING_FOR_CONSTRAINT:		
			if(currentPlayTime >= m_TrackTimeToSwitchStemMix)
			{
				if(audVerifyf(mood->numStemMixes >= m_CurrentStemMixId, 
												"Invalid stem mix id %u for mood %s", 
												m_CurrentStemMixId, 
												audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(mood->NameTableOffset)))
				{
					m_CurrentStemMixId = (m_CurrentStemMixId + 1) % mood->numStemMixes;
					
					const StemMix *mix = audNorthAudioEngine::GetObject<StemMix>(mood->StemMixes[m_CurrentStemMixId].StemMix);
					if(audVerifyf(mix, "Invalid stem mix %u for mood %s", m_CurrentStemMixId, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(mood->NameTableOffset)))
					{
						const float fadeTimeS = mood->StemMixes[m_CurrentStemMixId].FadeTime;
						CrossfadeToStemMix(mix, m_PlayingMoodVolumeOffset, fadeTimeS, fadeTimeS);

						audDisplayf("Switched to stem mix %u / %u", m_CurrentStemMixId, mood->numStemMixes);
					}
				}

				const u32 mixTime = static_cast<u32>(1000.f * audEngineUtil::GetRandomNumberInRange(mood->StemMixes[m_CurrentStemMixId].MinDuration, mood->StemMixes[m_CurrentStemMixId].MaxDuration));
				m_CurrentStemMixExpiryTime = (m_MusicSound? (m_ReplayWavePlayTimeAdjustment + m_MusicSound->GetCurrentPlayTimeOfWave()) : 0) + mixTime;
				m_MoodState = WAITING_FOR_EXPIRY;
			}
			break;
		}
	}
}

void audInteractiveMusicManager::InternalSnapCategoriesToMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset)
{
	const StemMix *mix = audNorthAudioEngine::GetObject<StemMix>(pMood->StemMixes[0].StemMix);
	if(audVerifyf(mix, "Invalid stem mix for mood %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pMood->NameTableOffset)))
	{
		for (int i = 0; i < AUD_NUM_MUSIC_STEMS; ++i)
		{
			audCategoryController* pCat = m_pCatCtrlStems[i];
			if (audVerifyf(pCat, "Invalid Category Controller Stem at index %d for mood %s", i, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pMood->NameTableOffset)))
			{
				float fStemVolume = *(&mix->Stem1Volume + i)*0.01f + fVolumeOffset;
				pCat->SetVolumeDB(fStemVolume);
			}			
		}
	}
	m_fAmbMusicDuckingVol = pMood->AmbMusicDuckingVol;
	m_pPlayingMood = pMood;
	m_PlayingMoodVolumeOffset = fVolumeOffset;

	ResetMoodState();
}

void audInteractiveMusicManager::CrossfadeToStemMix(const StemMix *mix, const float volumeOffsetDb, const float fadeInTimeS, const float fadeOutTimeS, const float crossfadeOffsetTimeS)
{
	for (int i = 0; i < AUD_NUM_MUSIC_STEMS; ++i)
	{
		audCategoryController* pCat = m_pCatCtrlStems[i];

		f32 fCatVol = pCat->GetVolumeDB();
		float fNewStemVolume = (*(&mix->Stem1Volume+i)*0.01f) + volumeOffsetDb;			
		float fadeTimeS = Selectf(fNewStemVolume - fCatVol, fadeInTimeS, fadeOutTimeS);

		if(crossfadeOffsetTimeS >= 0)
		{
			f32 fCurrentVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(fCatVol);
			f32 fNewVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(fNewStemVolume);
			f32 fFadeRate = (fNewVolumeLinear - fCurrentVolumeLinear)/fadeTimeS;
			fCatVol = fCurrentVolumeLinear + (fFadeRate * Min(crossfadeOffsetTimeS, fadeTimeS));
			fadeTimeS -= Min(crossfadeOffsetTimeS, fadeTimeS);
			pCat->SetVolumeLinear(fCatVol);
		}
		
		naAssertf(fNewStemVolume <= 0.f, "StemMix %s stem %u invalid volume: %f (offset %f); active mood: %s", 
						audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(mix->NameTableOffset),
						i,
						fNewStemVolume,
						volumeOffsetDb,
						m_pPlayingMood ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_pPlayingMood->NameTableOffset) : "none");
		
		pCat->BeginGainFadeDB(fNewStemVolume, fadeTimeS);
	}
}

void audInteractiveMusicManager::InternalCrossfadeCategoriesToMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset, s32 iFadeInTime, s32 iFadeOutTime, s32 iCrossfadeOffsetTime)
{
	const f32 fFadeInTime = (iFadeInTime>=0 ? iFadeInTime : pMood->FadeInTime) * 0.001f;
	const f32 fFadeOutTime = (iFadeOutTime>=0 ? iFadeOutTime : (m_pPlayingMood ? m_pPlayingMood->FadeOutTime : pMood->FadeInTime) ) * 0.001f;
	const f32 fCrossfadeOffsetTime = (iCrossfadeOffsetTime>=0 ? iCrossfadeOffsetTime : 0) * 0.001f;

	const StemMix *mix = audNorthAudioEngine::GetObject<StemMix>(pMood->StemMixes[0].StemMix);
	if(audVerifyf(mix, "Invalid stem mix for mood %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pMood->NameTableOffset)))
	{
		CrossfadeToStemMix(mix, fVolumeOffset, fFadeInTime, fFadeOutTime, fCrossfadeOffsetTime);
	}
	m_fAmbMusicDuckingVol = pMood->AmbMusicDuckingVol;
	m_pPlayingMood = pMood;
	m_PlayingMoodVolumeOffset = fVolumeOffset;

	ResetMoodState();
}

void audInteractiveMusicManager::InternalStop(s32 iReleaseTime)
{
	if (m_MusicSound)
	{
		m_MusicCategory->BeginGainFadeLinear(0.f, Max(0.f, iReleaseTime*0.001f));
	}
	m_PlayingTrackSoundHash = 0;
	m_PendingTrackSoundHash = 0;
	REPLAY_ONLY(m_ReplayScoreSound = g_NullSoundHash;)
	
	m_pPlayingMood = NULL;
	m_iTrackLengthMs = -1; //Uncomputed
	m_iCurPlayTimeMs = -1; //Uncomputed
	m_enMode = kModeStopped;
	m_iTimeToTriggerMoodSnap = -1;
	m_bWrapAroundSnap = false;
	m_fAmbMusicDuckingVol = 0.0f;

	m_PendingStopTrackTime = -1.f;
	m_PendingStopTrackRelease = -1;

	m_uTimeToTriggerMixer = UINT_MAX;
	m_uTimeToDeTriggerMixer = 0;

	if(m_NextStemMixMusicAction)
	{
		delete m_NextStemMixMusicAction;
		m_NextStemMixMusicAction = NULL;
	}
}

void audInteractiveMusicManager::InternalStopAndReset(s32 iReleaseTime)
{
	m_EventQueue.Reset();
	InternalStop(iReleaseTime);
}

void audInteractiveMusicManager::ParseMarkers()
{
	m_TempoMap.Reset();
	m_SilentRegions.Reset();
	m_TempoMapSoundHash = g_NullSoundHash;
	
	if (!m_PlayingTrackSoundHash || !m_MusicSound)
	{
		return;
	}
	
	const u32 metadataSoundHash = m_PlayingTrackSoundHash;

	// find the wavename hash
	const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(metadataSoundHash);
	if(streamingSound)
	{
		for(int i = 0; i<AUD_NUM_MUSIC_STEMS && i<streamingSound->numSoundRefs; ++i)
		{
			u32 wavehash[2] = {~0U,~0U};
			const MultitrackSound *multitrackSound = SOUNDFACTORY.DecompressMetadata<MultitrackSound>(streamingSound->SoundRef[i].SoundId);
			if(multitrackSound)
			{
				for(u32 waveIndex = 0; waveIndex < 2; waveIndex++)
				{
					const SimpleSound *simpleSound = SOUNDFACTORY.DecompressMetadata<SimpleSound>(multitrackSound->SoundRef[waveIndex].SoundId);
					if(simpleSound)
					{
						wavehash[waveIndex] = simpleSound->WaveRef.WaveName;
					}
				}
			}


			audWaveSlot *slot = m_MusicSound->GetWaveSlot();
			if(audVerifyf(slot, "Failed to find music wave slot"))
			{
				audWaveRef waveRef0, waveRef1;
				audWaveMarkerList markers0, markers1;

				const audWaveFormat *format0 = NULL, *format1 = NULL;
				if(wavehash[0] != ~0U)
				{
					if(waveRef0.Init(slot, wavehash[0]))
					{
						waveRef0.FindMarkerData(markers0);
						if(markers0.GetCount())
						{
							u32 formatSizeBytes;
							format0 = waveRef0.FindFormat(formatSizeBytes);
						}
					}
				}

				if(wavehash[1] != ~0U)
				{
					if(waveRef1.Init(slot, wavehash[1]))
					{
						waveRef1.FindMarkerData(markers1);
						if(markers1.GetCount())
						{
							u32 formatSizeBytes;
							format1 = waveRef1.FindFormat(formatSizeBytes);
						}
					}
				}

				bool foundSilence = false;

				// Construct tempo map: assumes we only have TEMPO markers in a single stem/channel
				for(u32 j = 0; j < markers0.GetCount(); j++)
				{
					if(markers0[j].categoryNameHash == ATSTRINGHASH("TEMPO", 0x7a495db3))
					{
						audAssertf(format0, "Failed to find format chunk for streaming wave %u", wavehash[0]);
						m_TempoMap.AddTempoChange(markers0[j].positionSamples / float(format0->SampleRate),
							markers0[j].value,
							markers0[j].data);
					}
					else if(markers0[j].categoryNameHash == ATSTRINGHASH("SILENCE", 0xD0D7570A))
					{
						foundSilence = true;
						SilentRegion r;
						r.durationS = markers0[j].value;
						audAssertf(format0, "Failed to find format chunk for streaming wave %u", wavehash[0]);
						r.startTimeS = markers0[j].positionSamples / float(format0->SampleRate);

						m_SilentRegions.Append() = r;
					}
				}
				for(u32 j = 0; j < markers1.GetCount(); j++)
				{
					if(markers1[j].categoryNameHash == ATSTRINGHASH("TEMPO", 0x7a495db3))
					{
						audAssertf(format1, "Failed to find format chunk for streaming wave %u", wavehash[1]);
						m_TempoMap.AddTempoChange(markers1[j].positionSamples / float(format1->SampleRate),
							markers1[j].value,
							markers1[j].data);
					}
					else if(!foundSilence && markers1[j].categoryNameHash == ATSTRINGHASH("SILENCE", 0xD0D7570A))
					{
						SilentRegion r;
						r.durationS = markers0[j].value;
						audAssertf(format1, "Failed to find format chunk for streaming wave %u", wavehash[1]);
						r.startTimeS = markers0[j].positionSamples / float(format1->SampleRate);

						m_SilentRegions.Append() = r;
					}
				}
			}			
		}
	}

	m_TempoMapSoundHash = metadataSoundHash;

	if(!m_TempoMap.HasEntries())
	{
		// Add a dummy entry so we have a valid tempo map; it won't be musically in sync but allows us to use a musical timeline for
		// constraints
		m_TempoMap.AddTempoChange(0.f, 120.f, 4);
	}
}

int ConstraintCompare(const MusicTimingConstraint* const* a, const MusicTimingConstraint* const* b)
{
	// Sort in descending order of TYPE_ID
	return (int)((*b)->ClassId - (*a)->ClassId);
}

float audInteractiveMusicManager::ComputeNextPlayTimeForConstraints(atFixedArray<const MusicTimingConstraint *, MusicAction::MAX_TIMINGCONSTRAINTS> &constraints, const float timeOffsetS) const
{
	const float trackLengthS = m_iTrackLengthMs * 0.001f;
	const float currentPlayTime = fmodf(m_iCurPlayTimeMs * 0.001f + timeOffsetS, trackLengthS);

	// We need to process bar constraints before beat constraints
	constraints.QSort(0, -1, &ConstraintCompare);

	audTempoMap::TimelinePosition currentPosition;
	m_TempoMap.ComputeTimelinePosition(currentPlayTime, currentPosition);

	for(s32 i = 0; i < constraints.GetCount(); i++)
	{
		if(naVerifyf(constraints[i] && gGameObjectsIsOfType(constraints[i], MusicTimingConstraint::TYPE_ID), "Invalid timing constraint reference in slot %d", i))
		{
			switch(constraints[i]->ClassId)
			{
			case BarConstraint::TYPE_ID:
				{
					const BarConstraint *constraint = reinterpret_cast<const BarConstraint*>(constraints[i]);

					// We always need to start from the next bar when evaluating a bar constraint, since we've
					// missed the start of the current bar, so we conceptually add one to 'bar' here.
					// We need to subtract one from the current bar for the test below since it's based on 0-indexing, so
					// that cancels out the above.
					s32 bar = static_cast<s32>(currentPosition.Bar);

					currentPosition.Beat = 1;
					currentPosition.BeatOffsetS = 0.f;

					const s32 patternLength = Max(1, constraint->PatternLength);
					const s32 validBar = constraint->ValidBar - 1;
					// Note that this works based on 0-indexing
					while(bar % patternLength != validBar)
					{
						bar++;		
					}
					// add 1 to the chosen bar to get back to 1-indexing
					currentPosition.Bar = static_cast<u32>(bar + 1);
				}
				break;
			case BeatConstraint::TYPE_ID:
				{
					const BeatConstraint *constraint = reinterpret_cast<const BeatConstraint*>(constraints[i]);					

					audAssertf(constraint->ValidSixteenths.Value != 0, "Invalid beat constraint in slot %u; no value 16ths", i);

					bool found = false;
					const float beatLength = (60.f / currentPosition.Tempo);
					const float sixteenthLength = 0.25f * beatLength;

					do
					{						
						u32 currentSixteenth = (currentPosition.Beat - 1) * 4 + u32(currentPosition.BeatOffsetS / sixteenthLength);
						u32 sixteenthMask = 1 << currentSixteenth;

						for(; currentSixteenth < 16; currentSixteenth++)
						{
							if(constraint->ValidSixteenths.Value & sixteenthMask)
							{
								currentPosition.Beat = currentSixteenth >> 2;
								currentPosition.BeatOffsetS = (currentSixteenth * sixteenthLength) - (currentPosition.Beat * beatLength);
								currentPosition.Beat++;
								found = true;
								break;
							}
							sixteenthMask <<= 1;
						}

						if(!found)
						{
							// continue searching from the start of the next bar
							currentPosition.Bar++;
							currentPosition.Beat = 1;
							currentPosition.BeatOffsetS = 0.f;
						}
					}while(!found);
					

				}
				break;
			case SilenceConstraint::TYPE_ID:
				{
					const SilenceConstraint *constraint = reinterpret_cast<const SilenceConstraint*>(constraints[i]);		
					const float trackTime = fmodf(m_TempoMap.ComputeTimeForPosition(currentPosition), m_iTrackLengthMs * 0.001f);
					const float silentTime = ComputeNextSilentTime(constraint->MinimumDuration, trackTime);
					if(silentTime >= 0.f)
					{
						m_TempoMap.ComputeTimelinePosition(silentTime, currentPosition);
					}
				}
				break;
			}
		}
	}	

	const float trackTime = m_TempoMap.ComputeTimeForPosition(currentPosition) - timeOffsetS;
	return fmodf(trackTime, m_iTrackLengthMs * 0.001f);
}

float audInteractiveMusicManager::ComputeNextPlayTimeForConstraints(const MusicAction *constrainedAction, const float timeOffsetS) const
{
	atFixedArray<const MusicTimingConstraint *, MusicAction::MAX_TIMINGCONSTRAINTS> constraints;

	const audMetadataManager &mgr = audNorthAudioEngine::GetMetadataManager();
	for(u32 i = 0; i < constrainedAction->numTimingConstraints; i++)
	{		
		constraints.Append() = reinterpret_cast<const MusicTimingConstraint *>(mgr.GetObjectMetadataPtr(constrainedAction->TimingConstraints[i].Constraint));
	}	
	
	return ComputeNextPlayTimeForConstraints(constraints, timeOffsetS) + constrainedAction->DelayTime;
}

bool audInteractiveMusicManager::IsMusicPlaying() const
{ 
	return (m_MusicSound != NULL) || (m_OneShotSound != NULL && m_OneShotSound->GetPlayState() == AUD_SOUND_PLAYING); 
}

float audInteractiveMusicManager::GetOneShotLength() const
{
	if(m_OneShotSound)
	{
		return 0.001f * m_OneShotSound->ComputeDurationMsExcludingStartOffsetAndPredelay(NULL);
	}
	return 0.f;
}

float audInteractiveMusicManager::ComputeNextSilentTime(const float minLength, const float timeOffsetS /* = -1 */) const
{
	float trackTimeOffset = (timeOffsetS < 0.f) ? m_iCurPlayTimeMs * 0.001f : timeOffsetS;
	for(s32 i = 0; i < m_SilentRegions.GetCount(); i++)
	{
		if(m_SilentRegions[i].startTimeS <= trackTimeOffset && 
			m_SilentRegions[i].startTimeS + m_SilentRegions[i].durationS - trackTimeOffset > minLength)
		{
			return trackTimeOffset;
		}
		else if(m_SilentRegions[i].startTimeS > trackTimeOffset && m_SilentRegions[i].durationS >= minLength)
		{
			return m_SilentRegions[i].startTimeS;
		}
	}

	// Failed to find a suitable region
	return -1.f;
}

#if GTA_REPLAY
void audInteractiveMusicManager::SaveMusicState()
{
	if(m_pPlayingMood)
	{
		m_SavedPlayingTrackSoundHash = m_PlayingTrackSoundHash;
		m_SavedPlayingMood = m_pPlayingMood;
		m_SavedPlayingMoodVolumeOffset = m_PlayingMoodVolumeOffset;
		m_SavedCurrentStemMixId = m_CurrentStemMixId;
		m_SavedPlayTimeOfWave = (m_MusicSound? (m_ReplayWavePlayTimeAdjustment + m_MusicSound->GetCurrentPlayTimeOfWave()) : 0);
		m_SavedMusicOverridesRadio = m_MusicOverridesRadio;
		m_SavedMuteRadioOffSound = m_MuteRadioOffSound;

	}
	else
	{
		m_SavedPlayingTrackSoundHash = g_NullSoundHash;
		m_SavedPlayingMood = NULL;
		m_SavedPlayingMoodVolumeOffset = 0.0f;
		m_SavedCurrentStemMixId = 0;
		m_SavedPlayTimeOfWave = 0;
		m_ReplayWavePlayTimeAdjustment = 0;
	}

	m_SavedPendingMoodChange = m_PendingMoodChange;
	m_SavedPendingMoodChangeTime = m_PendingMoodChangeTime;
	m_SavedPendingMoodVolumeOffset = m_PendingMoodVolumeOffset;
	m_SavedPendingMoodFadeInTime = m_PendingMoodFadeInTime;
	m_SavedPendingMoodFadeOutTime = m_PendingMoodFadeOutTime;
	m_SavedPendingStopTrackTime = m_PendingStopTrackTime;
	m_SavedPendingStopTrackRelease = m_PendingStopTrackRelease;
}

void audInteractiveMusicManager::ClearSavedMusicState()
{
	m_SavedPlayingTrackSoundHash = g_NullSoundHash;
	m_SavedPlayingMood = NULL;
	m_SavedPlayingMoodVolumeOffset = 0.0f;
	m_SavedCurrentStemMixId = 0;
	m_SavedPlayTimeOfWave = 0;
	m_ReplayWavePlayTimeAdjustment = 0;

	m_SavedPendingMoodChange = NULL;
	m_SavedPendingMoodChangeTime = 0.0f;
	m_SavedPendingMoodVolumeOffset = 0.0f;
	m_SavedPendingMoodFadeInTime = 0;
	m_SavedPendingMoodFadeOutTime = 0;
	m_SavedPendingStopTrackTime = -1.0f;
	m_SavedPendingStopTrackRelease = -1;
}

void audInteractiveMusicManager::RestoreMusicState()
{

	m_CurrentStemMixId = m_SavedCurrentStemMixId;

	PlayLoopingTrack(
		m_SavedPlayingTrackSoundHash,
		m_SavedPlayingMood,
		m_SavedPlayingMoodVolumeOffset, 
		0, 
		0, 
		0.0f,
		0.0f,
		m_SavedMusicOverridesRadio,
		m_SavedMuteRadioOffSound);

	m_ReplayWavePlayTimeAdjustment = m_SavedPlayTimeOfWave;

	m_PendingMoodChange = m_SavedPendingMoodChange;
	m_PendingMoodChangeTime = m_SavedPendingMoodChangeTime;
	m_PendingMoodVolumeOffset = m_SavedPendingMoodVolumeOffset;
	m_PendingMoodFadeInTime = m_SavedPendingMoodFadeInTime;
	m_PendingMoodFadeOutTime = m_SavedPendingMoodFadeOutTime;
	m_PendingStopTrackTime = m_SavedPendingStopTrackTime;
	m_PendingStopTrackRelease = m_SavedPendingStopTrackRelease;

}

void audInteractiveMusicManager::SetReplayScoreIntensity(const eReplayMarkerAudioIntensity intensity, const eReplayMarkerAudioIntensity prevIntensity, bool instant, u32 replaySwitchMoodOffset)
{
	if(intensity != m_ReplayScoreIntensity || instant)
	{
		m_ReplayScoreIntensity = intensity;		
		m_ReplayScorePrevIntensity = prevIntensity;		
		m_ReplaySwitchMoodInstantly = instant;
		m_ReplayMoodSwitchOffset = replaySwitchMoodOffset;

		const char *actionNames[MARKER_AUDIO_INTENSITY_MAX] = {
			"VID_XS_F",
			"VID_S_F",
			"VID_M_F",
			"VID_L_F",
			"VID_XL_F"
		};

		m_EventManager.TriggerEvent(actionNames[intensity]);
	}
}

void audInteractiveMusicManager::RequestReplayMusic(const u32 soundHash, const u32 startOffsetMs, const s32 durationMs, const f32 fadeOutScalar)
{
	if(m_ReplayScoreSound != soundHash)
	{
		const char *moodNames[MARKER_AUDIO_INTENSITY_MAX] = {
			"VID_XS_MD",
			"VID_S_MD",
			"VID_M_MD",
			"VID_L_MD",
			"VID_XL_MD"
		};
		const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(soundHash);
		if(audVerifyf(streamingSound, "Replay music hash %u not streaming sound", soundHash))
		{
			if(startOffsetMs < streamingSound->Duration)
			{
				float startOffsetScaled = 0.f;
				if(streamingSound->Duration > 0)
				{
					startOffsetScaled = (startOffsetMs % streamingSound->Duration) / float(streamingSound->Duration);
				}
				// Default to track duration
				if(durationMs >= 0)
				{
					SetReplayScoreDuration(durationMs);
				}
				else
				{
					SetReplayScoreDuration(streamingSound->Duration);
				}

				PlayLoopingTrack(soundHash,
					audNorthAudioEngine::GetObject<InteractiveMusicMood>(moodNames[m_ReplayScoreIntensity]),
					0.f,
					startOffsetMs > 100 ? 250 : 0,
					250,
					startOffsetScaled,
					0,
					true,
					true);
				m_ReplayScoreSound = soundHash;
			}			
		}
	}

	if(m_enMode != kModeStopped)
	{
		m_ReplayMusicFadeVolumeLinear = Clamp(fadeOutScalar, 0.0f, 1.0f);
	}	
}

void audInteractiveMusicManager::UpdateReplayMusic()
{
	if(m_ReplayScoreSound != g_NullSoundHash && m_MusicSound && m_MusicSound->GetPlayState() == AUD_SOUND_PLAYING)
	{
		UpdateReplayMusicVolume();
		for(s32 i = 0; i < m_MusicSound->GetWavePlayerIdCount(); i++)
		{
			audPcmSourceInterface::SetParam(m_MusicSound->GetWavePlayerId(i), audPcmSource::Params::PauseGroup, 0U);
		}
	}
}

void audInteractiveMusicManager::UpdateReplayMusicVolume(bool instant)
{
	if(m_ReplayScoreSound != g_NullSoundHash && m_MusicSound)
	{
		const f32 interpRate = 10.0f;
		f32 desiredVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(g_FrontendAudioEntity.GetMontageMusicVolume() + g_FrontendAudioEntity.GetClipMusicVolume()) * m_ReplayMusicFadeVolumeLinear;
		f32 currentVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(m_MusicSound->GetRequestedPostSubmixVolumeAttenuation());

		if(instant)
		{
			currentVolumeLinear = desiredVolumeLinear;
		}
		else 
		{
			if(currentVolumeLinear < desiredVolumeLinear)
			{
				currentVolumeLinear = Clamp(currentVolumeLinear + (fwTimer::GetTimeStep() * interpRate), currentVolumeLinear, desiredVolumeLinear);
			}
			if(currentVolumeLinear > desiredVolumeLinear)
			{
				currentVolumeLinear = Clamp(currentVolumeLinear - (fwTimer::GetTimeStep() * interpRate), desiredVolumeLinear, currentVolumeLinear);
			}
		}

		m_MusicSound->SetRequestedPostSubmixVolumeAttenuation(audDriverUtil::ComputeDbVolumeFromLinear(currentVolumeLinear));
	}
}

void audInteractiveMusicManager::UpdateReplayMusicPreview(bool allowFading)
{
	m_ReplayPreviewPlaying = (m_ReplayScoreSound != g_NullSoundHash && m_MusicSound && !CReplayMgr::IsEditModeActive()) ? true : false;

	if(m_ReplayPreviewPlaying)
	{
		f32 volume = 1.0f;

		if(allowFading)
		{
			u32 currentPlayTime = m_MusicSound->GetCurrentPlayTime();
			if(currentPlayTime < REPLAY_PREVIEW_FADE_LENGTH)
			{
				volume = Clamp((float)currentPlayTime / (float)REPLAY_PREVIEW_FADE_LENGTH, 0.0f, 1.0f);
			}
			else if(currentPlayTime > REPLAY_PREVIEW_DEFAULT_LENGTH - 2*REPLAY_PREVIEW_FADE_LENGTH)
			{
				volume = Clamp(((float)(REPLAY_PREVIEW_DEFAULT_LENGTH-REPLAY_PREVIEW_FADE_LENGTH) - (float)currentPlayTime) / (float)REPLAY_PREVIEW_FADE_LENGTH, 0.0f, 1.0f);
			}
		}
		
		m_MusicSound->SetRequestedPostSubmixVolumeAttenuation(audDriverUtil::ComputeDbVolumeFromLinear(volume*REPLAY_PREVIEW_MUSIC_VOLUME));

		if(m_MusicSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			m_MusicSound->GetRequestedSettings()->SetShouldPersistOverClears(true);
			for(s32 i = 0; i < m_MusicSound->GetWavePlayerIdCount(); i++)
			{
				audPcmSourceInterface::SetParam(m_MusicSound->GetWavePlayerId(i), audPcmSource::Params::PauseGroup, 0U);
			}

			if(m_MusicSound->GetCurrentPlayTime() > m_ReplayScoreDuration)
			{
				m_EventManager.TriggerEvent("VID_STOP");
				m_ReplayScoreDuration = ~0U;
			}
		}
	}
}


void audInteractiveMusicManager::SetReplayScoreDuration(const u32 durationMs)
{
	m_ReplayScoreDuration = durationMs;
}

void audInteractiveMusicManager::OnReplayEditorActivate()
{
	// We need to stop any wanted/vehicle/idle music as the replay score preview will nuke it. If we don't do this,
	// then it won't start up again correctly when we return to the main game	
	m_VehicleMusic.StopMusic();
	m_IdleMusic.StopMusic();
	m_WantedMusic.StopMusic();
	StopAndReset();
}

f32 audInteractiveMusicManager::GetPrevReplayBeat(u32 soundHash, f32 startOffsetMS)
{
	if(soundHash == m_TempoMapSoundHash)
	{
		if(!m_TempoMap.HasEntries())
		{
			ParseMarkers();
		}

		if(m_TempoMap.HasEntries())
		{
			f32 timeSeconds = m_TempoMap.ComputePrevBeatTime(startOffsetMS);
			return (timeSeconds * 1000.f);
		}
	}
    else
    {
        audWarningf("Trying to access a tempo map for a different sound!");
    }

	return startOffsetMS;
}

f32 audInteractiveMusicManager::GetNextReplayBeat(u32 soundHash, f32 startOffsetMS)
{
	if(soundHash == m_TempoMapSoundHash)
	{
		if(!m_TempoMap.HasEntries())
		{
			ParseMarkers();
		}

		if(m_TempoMap.HasEntries())
		{
			f32 timeSeconds = m_TempoMap.ComputeNextBeatTime(startOffsetMS);			
			return (timeSeconds * 1000.f);
		}
	}
    else
    {
        audWarningf("Trying to access a tempo map for a different sound!");
    }

	return startOffsetMS;
}

#endif	//GTA_REPLAY
