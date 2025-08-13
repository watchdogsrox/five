// 
// audio/frontendaudioentity.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
// 

#include "frontendaudioentity.h"
#include "gameobjects.h"
#include "audiodefines.h"
#include "audioengine/categorycontrollermgr.h"
#include "audio/dynamicmixer.h"
#include "audio/environment/environment.h"
#include "northaudioengine.h"
#include "audioengine/categorycontrollermgr.h"
#include "audioengine/curve.h"
#include "audioengine/environmentgroup.h"
#include "audioengine/environment_game.h"
#include "audioengine/engine.h"
#include "audioengine/widgets.h"
#include "audiohardware/config.h"
#include "audiohardware/syncsource.h"
#include "audiohardware/waveref.h"
#include "audiohardware/waveslot.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/simplesound.h"
#include "audiosoundtypes/streamingsound.h"
#include "audioengine/categorymanager.h"
#include "control/gamelogic.h"
#include "control/replay/IReplayMarkerStorage.h"
#include "control/replay/Replay.h"
#include "control/replay/ReplayMarkerContext.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/VideoEditor/Core/VideoProjectPlaybackController.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "network/network.h"
#include "network/networkinterface.h"
#include "radioaudioentity.h"
#include "pickups/Data/PickupIds.h"
#include "frontend/loadingscreens.h"
#include "scriptaudioentity.h"
#include "input/keyboard.h"
#include "fwsys/timer.h"
#include "system/param.h"
#include "debugaudio.h"
#include "video/VideoPlayback.h"

#if GTA_REPLAY
#include "frontend/VideoEditor/ui/Editor.h"
#endif // #if GTA_REPLAY

#include "profile/profiler.h"
#include "system/param.h"


AUDIO_OPTIMISATIONS()
//OPTIMISATIONS_OFF()
audFrontendAudioEntity g_FrontendAudioEntity;

audCategory *g_FrontendGameWeaponPickupsCategory;

bank_float g_FrontendMusicReverbLarge = 0.4f;
bank_float g_FrontendMusicLargeReverbHPF = 250.f;
bank_float g_FrontendMusicLargeReverbDelayFeedback = -6.f;
u32 g_FrontendMusicDrumsLPF = 7000;

BANK_ONLY(bool audFrontendAudioEntity::sm_ShowScaleformSoundsPlaying = false;);

audCurve g_FEMusicVolCurve;

PARAM(noloadingtune, "Disables playing of music during game load");
PARAM(nomenumusic, "Disable menu music");

namespace rage
{
	XPARAM(noaudio);
}

#if GTA_REPLAY
f32 g_fMontageVolumeTable[REPLAY_VOLUMES_COUNT] = { -100.0f, -20.0f, -13.0f, -9.6f, -7.1f, -5.1f, -3.6f, -2.2f, 0.0f }; // 0, 25, 50, 75, 100
#endif

const u32 g_FrontendGameTinnyLeftHash = ATSTRINGHASH("FRONTEND_GAME_TINNY_LEFT", 0x0581ffe68);
const u32 g_FrontendGameTinnyRightHash = ATSTRINGHASH("FRONTEND_GAME_TINNY_RIGHT", 0x064860dfc);

bank_float g_FEChannelInterpSpeed = 0.05f;
bank_float g_FEMusicIntensityRate = 0.01f;
bank_float g_FEChannelWaitScale[g_NumChannelsInFrontendMusic] = {1.4f, 1.0f, 1.0f, 1.5f, 1.8f, 1.8f, 2.0f};//, 2.0f, 2.2f, 2.2f, 2.1f, 2.4f, 1.1f};
bank_float g_FEChannelSpeedScale[g_NumChannelsInFrontendMusic] = {0.4f, 0.6f, 0.5f, 0.4f, 0.7f, 0.6f, 1.f};//, 1.3f, 1.4f, 1.1f, 1.2f, 1.65f, 0.8f};
bank_float g_FEChannelIntensityScale[g_NumChannelsInFrontendMusic] = {0.f, 0.1f, 0.1f, 0.2f, 0.3f, 0.3f, 0.4f};//, 0.4f, 1.f, 1.f, 1.25f, 1.25f, 2.0f};

PF_PAGE(FEMusicPage,"FrontendMusic");
PF_GROUP(FEMusic);
PF_LINK(FEMusicPage, FEMusic);
PF_VALUE_FLOAT(Intensity, FEMusic);
PF_VALUE_FLOAT(Stem1, FEMusic);
PF_VALUE_FLOAT(Stem2, FEMusic);
PF_VALUE_FLOAT(Stem3, FEMusic);
PF_VALUE_FLOAT(Stem4, FEMusic);
PF_VALUE_FLOAT(Stem5, FEMusic);
PF_VALUE_FLOAT(Stem6, FEMusic);
PF_VALUE_FLOAT(Stem7, FEMusic);

audFrontendAudioEntity::audFrontendAudioEntity()
{
	m_LoadingScreenScene = NULL;
	m_HasMutedForLoadingScreens = false;
	m_LoadSceneCategoryController = NULL;
	m_ShouldBePlayingMapMove = false;
	m_ShouldBePlayingNavLeftRightContinuous = false;
	m_ShouldBePlayingMapZoom = false;
	m_RequestedTinnitusEffect = CExplosionManager::NoEffect;
#if GTA_REPLAY
	m_GameWorldCategoryCOntroller = NULL;
	m_CutsceneCategoryController = NULL;
	m_SpeechCategoryController = NULL;
	m_AnimalVocalsCategoryController = NULL;
#endif

	m_HasPlayedLoadingTune = false;
}

audFrontendAudioEntity::~audFrontendAudioEntity()
{
}

void audFrontendAudioEntity::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audFrontendAudioEntity");
	m_TinnyLeft = NULL;
	m_TinnyRight = NULL;
	m_LoadingTune = NULL;	
	m_PauseMenuFirstHalf = NULL;
	m_SpecialAbilitySound = NULL;
	m_WeaponWheelSound = NULL;
	m_SwitchSound = NULL;
	m_WhiteWarningSwitchSound = NULL;
	m_RedWarningSwitchSound = NULL;
	m_StuntSound = NULL;
	m_CinematicSound = NULL;
	m_SlowMoGeneralSound = NULL;
	m_HasInitialisedCurves = false;
	m_HasToPlayPauseMenu = false;
	m_PlayedPauseMenuFirstHalf = false;
	m_HasToPlayPauseMenuSecondHalf = false;
	m_MapZoomSound = NULL;
	m_MapMovementSound = NULL;
	m_NavLeftRightContinuousSound = NULL;

#if GTA_REPLAY
	m_replayScrubbingSound = NULL;
	m_ShouldBePlayingReplayScrub = false;
#endif

	m_AmpFixLoop = NULL;

	m_RoomToneSound = NULL;
	m_CurrentRoomToneHash = g_NullSoundHash;
	m_TimeLastPlayedRoomTone = 0;

	m_PickUpSound = NULL;

	for (u32 i = 0; i < m_CnCSkyLoops.GetMaxCount(); i++)
	{
		m_CnCSkyLoops[i] = NULL;
	}

	m_HasPlayedWeaponWheelSound = false;

	m_PickUpStdSoundSet.Init(ATSTRINGHASH("HUD_FRONTEND_STANDARD_PICKUPS_SOUNDSET", 0xAFDDE447));
	m_PickUpVehicleSoundSet.Init(ATSTRINGHASH("HUD_FRONTEND_VEHICLE_PICKUPS_SOUNDSET", 0xC813E70F));
	m_PickUpWeaponsSoundSet.Init(ATSTRINGHASH("HUD_FRONTEND_WEAPONS_PICKUPS_SOUNDSET", 0x8C7D50B1));
	m_SpecialAbilitySoundSet.Init(ATSTRINGHASH("SPECIAL_ABILITY_SOUNDSET", 0xB4837459));

	m_MusicStreamingSound = NULL;
	for(u32 i = 0; i < g_NumChannelsInFrontendMusic; i++)
	{
		m_ChannelInterp[i] = 0.f;
		m_ChannelInterpSpeed[i] = 1.f;
		m_ChannelTargetPan[i] = 0.f;
		m_ChannelStartPan[i] = 0.f;
		m_ChannelState[i] = AUD_FEMUSIC_CHANNEL_MUTED;
	}

	m_MusicOverallIntensity = 0.f;
	m_MusicTargetIntensity = 1.f;

	m_MusicState = AUD_FEMUSIC_IDLE;
	
	m_LastUpdateTime = 0;
	m_MusicStreamSlot = NULL;
	
	g_FrontendGameWeaponPickupsCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("FRONTEND_GAME_WEAPON_PICKUPS", 0x06accab74));

	m_SpecialAbilityMode = kSpecialAbilityModeNone;

	m_FirstFEInput = true;
	m_StartMusicQuickly = false;

	m_IsFrontendMusicEnabled = false;
	m_IsFEMusicCodeDisabled = false;
	m_PlayingLoadingMusic = false;

	m_WasGrenadeFlashing = false;
	m_LastGrenadeFlashTime = 0;

	m_MapCursorMoveRate = 1.f;

	AUD_MUSIC_STUDIO_PLAYER_ONLY(m_MusicStudioSessionPlayer.Init(this));
	
#if GTA_REPLAY		
	m_GameWorldCategoryCOntroller = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("GAME_WORLD", 0x2729F75D));
	m_CutsceneCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("CUTSCENES", 0x7F01B626));
	m_SpeechCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("SPEECH", 0x9BBF7C4));
	m_AnimalVocalsCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("ANIMALS_VOCALS", 0x6C742462));
	m_MontageMusicFadeVolume = 0.0f;
	m_MontageSFXFadeVolume = 0.0f;
	m_ClipSFXFadeVolume = 0.0f;
	m_ReplayVolume = 0.f;
	m_ReplayDialogVolume = 0.f;
    m_ReplayCustomAmbienceVolume = 0.f;
    m_ReplaySFXOneShotVolume = 0.f;
	m_ClipMusicVolume = 0.f;
	m_ClipSFXVolume = 0.f;
	m_ClipDialogVolume = 0.f;
    m_ClipSFXOneshotVolume = 0.f;
    m_ClipCustomAmbienceVolume = 0.f;
	m_ClipMusicVolumeIndex = 8;
	m_ClipSFXVolumeIndex = 8;
	m_ClipDialogVolumeIndex = 8;
    m_ClipCustomAmbienceVolumeIndex = 8;
    m_ClipSFXOneshotVolumeIndex = 8;
#endif

	m_SoundSet.Init(ATSTRINGHASH("Frontend", 0x3E073C82));
	m_LongSwitchSounds.Init(ATSTRINGHASH("LONG_PLAYER_SWITCH_SOUNDS", 0x8DDBFC96));

#if GTA_REPLAY
	m_ReplaySlowMoDrone = NULL;
	m_ReplaySlowMoDroneVolSmoother.Init(1.f/500.f);
#endif

	m_SpecialAbilitySlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("AMB_STREAM_SPEC_AB", 0xDCE0989));

	m_SpecialAbilityBankIds[0] = audWaveSlot::FindBankId(ATSTRINGHASH("STREAMED_AMBIENCE/MICHAEL", 0xBB404F96));
	m_SpecialAbilityBankIds[1] = audWaveSlot::FindBankId(ATSTRINGHASH("STREAMED_AMBIENCE/FRANKLIN", 0x5156FB36));
	m_SpecialAbilityBankIds[2] = audWaveSlot::FindBankId(ATSTRINGHASH("STREAMED_AMBIENCE/TREVOR", 0x96A3420F));
	m_SpecialAbilityBankIds[3] = audWaveSlot::FindBankId(ATSTRINGHASH("STREAMED_AMBIENCE/MP_BUSTED", 0x7572DFE8));

	m_SpecialAbilitySounds.Init(ATSTRINGHASH("SPECIAL_ABILITY_SOUNDS", 0x56A69671));

	m_SwitchIsActive = false;

	if(!m_LoadSceneCategoryController)
	{
		m_LoadSceneCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController("BASE");
	}

	bool playLoadingMusic = true;
#if RSG_PC
	if(CLandingPageArbiter::ShouldShowLandingPageOnBoot())
	{
		playLoadingMusic = false;
	}
#endif
	// Start playing the loading music as early as possible
	if(playLoadingMusic)
	{
			StartLoadingTune();
	}


	m_PulseHeadsetSounds.Init(ATSTRINGHASH("PULSE_HEADSET_SOUNDS", 0x2957B6F7));
	m_PlayerPulseSound = NULL;

	VIDEO_PLAYBACK_ONLY( VideoPlayback::SetAudioEntityForPlayback( this ) );
}

void audFrontendAudioEntity::SetMPSpecialAbilityBankId()
{
	m_SpecialAbilityBankIds[3] = audWaveSlot::FindBankId(ATSTRINGHASH("DLC_GTAO/MP_RESIDENT", 0x3F1E72E));
	m_SpecialAbilitySounds.Init(ATSTRINGHASH("SPECIAL_ABILITY_SOUNDS", 0x56A69671));
	m_SpecialAbilitySoundSet.Init(ATSTRINGHASH("SPECIAL_ABILITY_SOUNDSET", 0xB4837459));
}

void audFrontendAudioEntity::SetSPSpecialAbilityBankId()
{
	m_SpecialAbilityBankIds[3] = audWaveSlot::FindBankId(ATSTRINGHASH("STREAMED_AMBIENCE/MP_BUSTED", 0x7572DFE8));
	m_SpecialAbilitySounds.Init(ATSTRINGHASH("SPECIAL_ABILITY_SOUNDS", 0x56A69671));
	m_SpecialAbilitySoundSet.Init(ATSTRINGHASH("SPECIAL_ABILITY_SOUNDSET", 0xB4837459));
}

void audFrontendAudioEntity::InitCurves()
{
	m_DeafeningTinnyVolumeCurve.Init("DEAFENING_STRENGTH_TO_TINNY_VOLUME");
	m_HasInitialisedCurves = true;
}

void audFrontendAudioEntity::Shutdown()
{
	AUD_MUSIC_STUDIO_PLAYER_ONLY(m_MusicStudioSessionPlayer.Shutdown());

	audEntity::Shutdown();
	if(m_MusicStreamSlot)
	{
		m_MusicStreamSlot->Free();
		m_MusicStreamSlot = NULL;
	}
	m_HasInitialisedCurves = false;

	

#if GTA_REPLAY	
	if(m_GameWorldCategoryCOntroller)
	{
		AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_GameWorldCategoryCOntroller);
	}
	if(m_CutsceneCategoryController)
	{
		AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_CutsceneCategoryController);
	}	
	if(m_SpeechCategoryController)
	{
		AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_SpeechCategoryController);
	}
	if(m_AnimalVocalsCategoryController)
	{
		AUDCATEGORYCONTROLLERMANAGER.DestroyController(m_AnimalVocalsCategoryController);
	}

	m_GameWorldCategoryCOntroller = NULL;
	m_CutsceneCategoryController = NULL;
	m_SpeechCategoryController = NULL;
	m_AnimalVocalsCategoryController = NULL;
#endif

}

void audFrontendAudioEntity::ShutdownLevel()
{
	Shutdown();
	// force a few controller frames to ensure all sounds are stopped
	for(u32 i = 0; i < 3; i++)
	{
		g_Controller->Update(fwTimer::GetTimeInMilliseconds());
		sysIpcSleep(33);
	}
	Init();
}

void audFrontendAudioEntity::InterpolateReplayVolume(f32& currentVolumeDB, f32 targetVolumeDB, f32 interpRate)
{
    f32 desiredVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(targetVolumeDB);
    f32 currentVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(currentVolumeDB);
    
    if(currentVolumeLinear < desiredVolumeLinear)
    {
        currentVolumeLinear = Clamp(currentVolumeLinear + (fwTimer::GetTimeStep() * interpRate), currentVolumeLinear, desiredVolumeLinear);
    }
    if(currentVolumeLinear > desiredVolumeLinear)
    {
        currentVolumeLinear = Clamp(currentVolumeLinear - (fwTimer::GetTimeStep() * interpRate), desiredVolumeLinear, currentVolumeLinear);
    }

    currentVolumeDB = audDriverUtil::ComputeDbVolumeFromLinear(currentVolumeLinear);
}

void audFrontendAudioEntity::PreUpdateService(u32 timeInMs)
{
	if(m_HasToPlayPauseMenu)
	{
		StartPauseMenuFirstHalfPrivate();
	}
	if(m_HasToPlayPauseMenuSecondHalf)
	{
		StartPauseMenuSecondHalfPrivate();
	}
	UpdateTinny();
	UpdateTinnitusEffect();
	
	UpdateRoomTone(timeInMs);

	ProcessDeferredSoundEvents();

#if !__FINAL
	bool keyPressed = false;
	for (u32 count=0; count<256; count++)
	{
		if(ioKeyboard::KeyPressed(count))
		{
			keyPressed = true;
			break;
		}
	}

	if(keyPressed)
	{
		CreateAndPlaySound("FRONTEND_KEYBOARD_PRESS", NULL);
	}
#endif //Not MASTER.

	UpdateMusic();

#if GTA_REPLAY
	SetReplayVolume(m_ClipSFXVolume);	
	SetReplayDialogVolume(m_ClipDialogVolume);	
    InterpolateReplayVolume(m_ReplaySFXOneShotVolume, m_ClipSFXOneshotVolume, 10.f);
    InterpolateReplayVolume(m_ReplayCustomAmbienceVolume, m_ClipCustomAmbienceVolume, 10.f);    
#endif

	// only trigger the amp fix loop when the game is extremely quiet (frontend menus etc), so we don't waste a physical voice during normal gameplay
	if(g_AudioEngine.GetSoundManager().GetMasterRMSSmoothed() < 0.0005f)
	{
		if(!m_AmpFixLoop)
		{
			if(!PARAM_noaudio.Get())
			{
				audDebugf1("Starting amp-fix (%f)", g_AudioEngine.GetSoundManager().GetMasterRMSSmoothed());
				audSoundInitParams initParams;
				initParams.ShouldPlayPhysically = true;
				CreateAndPlaySound_Persistent(m_SoundSet.Find("AmpFix"), &m_AmpFixLoop, &initParams);
			}
		}
	}
	else
	{
		if(m_AmpFixLoop)
		{
			audDebugf1("Stopping amp-fix (%f)", g_AudioEngine.GetSoundManager().GetMasterRMSSmoothed());
			m_AmpFixLoop->StopAndForget();
		}
	}

	if(m_SpecialAbilitySound)
	{
		CPed *localPlayer = CGameWorld::FindLocalPlayer();
		if(localPlayer && localPlayer->GetSpecialAbility())
		{
			m_SpecialAbilitySound->FindAndSetVariableValue(ATSTRINGHASH("SpecialAbilityTime", 0xBDF9C044), localPlayer->GetSpecialAbility()->GetRemainingNormalized());
		}
	}
	else if(m_SpecialAbilitySlot && !m_SpecialAbilitySlot->GetIsLoading())
	{
		CPed *localPlayer = CGameWorld::FindLocalPlayer();
		const bool isMP = CNetwork::IsGameInProgress();
		if(localPlayer || isMP)
		{
			u32 desiredBankId = m_SpecialAbilitySlot->GetLoadedBankId();
			OUTPUT_ONLY(const char *playerName = "unknown");
			if(isMP)
			{
				OUTPUT_ONLY(playerName = "Multiplayer");
				desiredBankId = m_SpecialAbilityBankIds[3];
			}
			else
			{				
				switch(localPlayer->GetPedType())
				{
				case PEDTYPE_PLAYER_0:
					OUTPUT_ONLY(playerName = "Michael");
					desiredBankId = m_SpecialAbilityBankIds[0];
					break;
				case PEDTYPE_PLAYER_1:
					OUTPUT_ONLY(playerName = "Franklin");
					desiredBankId = m_SpecialAbilityBankIds[1];
					break;
				case PEDTYPE_PLAYER_2:
					OUTPUT_ONLY(playerName = "Trevor");
					desiredBankId = m_SpecialAbilityBankIds[2];
					break;
				default:
					break;
				}
			}

			if(desiredBankId < AUD_INVALID_BANK_ID && desiredBankId != m_SpecialAbilitySlot->GetLoadedBankId())
			{
				audDisplayf("Requesting special ability bank %s (%u) for %s", audWaveSlot::GetBankName(desiredBankId), 
																				desiredBankId, 
																				playerName);
				if(m_SpecialAbilitySlot->GetBankLoadingStatus(desiredBankId) == audWaveSlot::NOT_REQUESTED)
				{
					m_SpecialAbilitySlot->LoadBank(desiredBankId, naWaveLoadPriority::PlayerInteractive);
				}
			}

		}

		if(GetSpecialAbilityMode() != kSpecialAbilityModeNone)
		{
			StartSpecialAbilitySounds();
		}

		if(m_HasPlayedWeaponWheelSound && !m_WeaponWheelSound)
		{
			StartWeaponWheel();
		}

		if(m_SwitchIsActive && !m_SwitchSound)
		{
			StartSwitch();
		}
	}

	if(audNorthAudioEngine::ShouldTriggerPulseHeadset())
	{
		if(!m_PlayerPulseSound)
		{
			CreateAndPlaySound_Persistent(m_PulseHeadsetSounds.Find(ATSTRINGHASH("PlayerHeartbeat", 0x9B5628DD)), &m_PlayerPulseSound);
		}
	}
	else if(m_PlayerPulseSound)
	{
		m_PlayerPulseSound->StopAndForget();
	}

#if GTA_REPLAY
	m_MontageMusicFadeVolume = 0.0f; 
	m_MontageSFXFadeVolume = 0.0f;

	if(CReplayMgr::IsEditorActive())
	{
		if(CReplayMgr::IsJustPlaying())	//CReplayCoordinator::IsExportingToVideoFile() || CReplayCoordinator::IsPreviewingVideo()) not sure why this was restricted to render and preview.
		{
			CVideoProjectPlaybackController& playbackController = CVideoEditorInterface::GetPlaybackController();

			if(playbackController.CanQueryStartAndEndTimes())
			{
				f32 const currentTimeMilliseconds = playbackController.GetPlaybackCurrentTimeMs();				
				f32 const endTimeMilliseconds = playbackController.GetPlaybackEndTimeMs();				
				const CMontage* montage = CReplayCoordinator::GetReplayPlaybackController().GetActiveMontage();

				if(montage)
				{
					u32 const clipCount = montage->GetClipCount();
					const CClip* finalClip = montage->GetClip(clipCount - 1);

					if(finalClip)
					{
						f32 const endTime = finalClip->GetEndNonDilatedTimeMs();
						s32 const musicIndex = CReplayCoordinator::GetReplayPlaybackController().GetMusicIndexAtCurrentNonDilatedTimeMs(clipCount - 1, endTime);

						// Only fade out music if anything is actually playing on the final frame
						if(musicIndex >= 0)
						{
							f32 const musicVolumeLinear = Min(1.0f, (endTimeMilliseconds - currentTimeMilliseconds)/(f32)REPLAY_MONTAGE_MUSIC_FADE_TIME);
							m_MontageMusicFadeVolume = audDriverUtil::ComputeDbVolumeFromLinear(musicVolumeLinear);
						}				

						f32 const sfxVolumeLinear = Min(1.0f, (endTimeMilliseconds - currentTimeMilliseconds)/(f32)REPLAY_MONTAGE_SFX_FADE_TIME);				
						m_MontageSFXFadeVolume = audDriverUtil::ComputeDbVolumeFromLinear(sfxVolumeLinear);
					}					
				}				
			}
		}
	}

	if(CReplayMgr::IsEditModeActive())
	{		
		f32 volume = GetReplayVolume();
		if(volume > 0.0f)
		{
			volume = 0.0f;
		}
		if(m_GameWorldCategoryCOntroller)
		{
			m_GameWorldCategoryCOntroller->SetVolumeDB(volume);
		}
		if(m_CutsceneCategoryController)
		{
			m_CutsceneCategoryController->SetVolumeDB(volume);
		}

		f32 dialogVolume = GetReplayDialogVolume();
		if(m_SpeechCategoryController)
		{
			m_SpeechCategoryController->SetVolumeDB(dialogVolume - volume);
		}
		if(m_AnimalVocalsCategoryController)
		{
			m_AnimalVocalsCategoryController->SetVolumeDB(dialogVolume - volume);
		}
		/*if(!m_ReplaySlowMoDrone)
		{
			CreateAndPlaySound_Persistent("SLO_MO_DRONE_MT",&m_ReplaySlowMoDrone);
		}*/

		//todo4five this needs changing now the slow mo system has changed
		//if(m_ReplaySlowMoDrone)
		//{
		//	f32 requestedVol = 0.f;
		//	switch(CReplayMgr::GetSlowMotionSetting())
		//	{
		//	case REPLAY_SLOWMOTION_X2:
		//		requestedVol = 0.0f;
		//		break;
		//	case REPLAY_SLOWMOTION_X3:
		//		requestedVol = 0.5f;
		//		break;
		//	case REPLAY_SLOWMOTION_X4:
		//	case REPLAY_SLOWMOTION_BULLET:
		//		requestedVol = 1.f;
		//		break;
		//	default:
		//		break;
		//	}

		//	if(!CReplayMgr::IsPlaybackPlaying())
		//	{
		//		requestedVol = 0.f;
		//	}

		//	/*TODO4FIVE const f32 linVol = m_ReplaySlowMoDroneVolSmoother.CalculateValue(requestedVol, (audEngineUtil::IsRendering() ? audEngineUtil::GetReplayRenderTimeInMS() : audEngineUtil::GetCurrentTimeInMilliseconds()));
		//	m_ReplaySlowMoDrone->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(linVol));*/
		//}
 
		IReplayMarkerStorage* markers = CReplayMarkerContext::GetMarkerStorage();
		if(markers)
		{
			u32 currentTime = CReplayMgr::GetCurrentTimeRelativeMs();
			if(currentTime > m_LastSlowMoTransitionTime)
			{
				m_LastSlowMoTransitionTime = 0;
			}

			for( u32 markerIndex = 0; markerIndex < markers->GetMarkerCount(); ++markerIndex)
			{
				sReplayMarkerInfo* currentMarker = markers->TryGetMarker( markerIndex );
				if( currentMarker && currentMarker->GetNonDilatedTimeMs() > currentTime)
				{
					//todo4five this needs changing now the slow mo system has changed
					//bool isSettingSlowMo = false;
					//bool isInSlowMo = false;

					//eReplaySlowMotion nextSlowMoSetting = CReplayMgr::GetSlowMotionSettingFromMarkerSpeed((*i)->Speed);
					//if(nextSlowMoSetting == REPLAY_SLOWMOTION_X4 || nextSlowMoSetting == REPLAY_SLOWMOTION_BULLET)
					//{
					//	isSettingSlowMo = true;
					//}
					//eReplaySlowMotion currentSlowMoSetting = CReplayMgr::GetSlowMotionSetting();
					//if(currentSlowMoSetting == REPLAY_SLOWMOTION_X4 || currentSlowMoSetting == REPLAY_SLOWMOTION_BULLET)
					//{
					//	isInSlowMo = true;
					//}
					//const bool transitioningIntoSlowMo = isSettingSlowMo && !isInSlowMo;
					//const bool transitioningOutOfSlowMo = isInSlowMo && nextSlowMoSetting == REPLAY_SLOWMOTION_NONE;

					//if(transitioningIntoSlowMo || transitioningOutOfSlowMo)
					//{
					//	u32 frameLength = 33;
					//	//const u32 millisecondsTilMarker = CReplayMgr::ComputeTimeMsBetweenFrames(currentFrameIndex,(*i)->Frame);
					//	//Displayf("ms til marker: %u",millisecondsTilMarker);
					//	if((currentTime >= (*i)->Time-frameLength) && m_LastSlowMoTransitionTime != (*i)->Time)
					//	{		
					//		// we're one frame away from the slow mo change event
					//		// compute a predelay based on the current frame length
					//		static u32 preemptTimeMs = 50;
					//		audSoundInitParams initParams;

					//		if(CReplayMgr::IsSlowMotionOn())
					//		{
					//			frameLength = (u32)((33.f/(f32)CReplayMgr::GetSlowMoValueInMs()) * (f32)fwTimer::GetTimeStep());
					//		}
					//		if(frameLength * 3 > preemptTimeMs)
					//		{
					//			initParams.Predelay = (frameLength*3) - preemptTimeMs;
					//		}

					//		if(transitioningIntoSlowMo)
					//		{
					//			Displayf("SLO_MO_INTRO: %u", initParams.Predelay);
					//			CreateAndPlaySound("SLO_MO_INTRO",&initParams);
					//			m_WasLastSlowMoTransitionIn = true;
					//		}
					//		else if(transitioningOutOfSlowMo)
					//		{
					//			Displayf("SLO_MO_OUTRO: %u", initParams.Predelay);
					//			CreateAndPlaySound("SLO_MO_OUTRO",&initParams);
					//			m_WasLastSlowMoTransitionIn = false;
					//		}
					//		m_LastSlowMoTransitionTime = (*i)->Time;						
					//	}


					//	// if this marker changes our state then stop looking for other markers
					//	break;
					//}
				}
			}
		}
	}
	else
	{
		if(m_ReplaySlowMoDrone)
		{
			m_ReplaySlowMoDrone->StopAndForget();
		}		

		if(m_GameWorldCategoryCOntroller)
		{
			m_GameWorldCategoryCOntroller->SetVolumeDB(0.0f);
		}
		if(m_CutsceneCategoryController)
		{
			m_CutsceneCategoryController->SetVolumeDB(0.0f);
		}
		if(m_SpeechCategoryController)
		{
			m_SpeechCategoryController->SetVolumeDB(0.0f);
		}
		if(m_AnimalVocalsCategoryController)
		{
			m_AnimalVocalsCategoryController->SetVolumeDB(0.0f);
		}
	}
#endif
}

void audFrontendAudioEntity::UpdateTinny()
{
	if(m_HasInitialisedCurves)
	{
		f32 strengthLeft = 0.0f;
		f32 strengthRight = 0.0f;
		audNorthAudioEngine::GetGtaEnvironment()->GetDeafeningStrength(&strengthLeft, &strengthRight);
		f32 volumeLeft = m_DeafeningTinnyVolumeCurve.CalculateValue(strengthLeft);
		f32 volumeRight = m_DeafeningTinnyVolumeCurve.CalculateValue(strengthRight);

		// left
		if (volumeLeft>g_SilenceVolume)
		{
			if (!m_TinnyLeft)
			{
				CreateAndPlaySound_Persistent(g_FrontendGameTinnyLeftHash, &m_TinnyLeft);
			}
			if (m_TinnyLeft)
			{
				m_TinnyLeft->SetRequestedVolume(volumeLeft);
			}
		}
		else
		{
			if (m_TinnyLeft)
			{
				m_TinnyLeft->StopAndForget();
			}
		}
		// right
		if (volumeRight>g_SilenceVolume)
		{
			if (!m_TinnyRight)
			{
				CreateAndPlaySound_Persistent(g_FrontendGameTinnyRightHash, &m_TinnyRight);
			}
			if (m_TinnyRight)
			{
				m_TinnyRight->SetRequestedVolume(volumeRight);
			}
		}
		else
		{
			if (m_TinnyRight)
			{
				m_TinnyRight->StopAndForget();
			}
		}
	}
}

void audFrontendAudioEntity::UpdateTinnitusEffect()
{
	u32 soundNameHash = 0u;
	u32 soundSetNameHash = 0u;
	u32 sceneNameHash = 0u;

	switch (m_RequestedTinnitusEffect)
	{
	case CExplosionManager::NoEffect:
		break;

	case CExplosionManager::StrongEffect:
		soundNameHash = ATSTRINGHASH("tinnitus", 0x6CE2F0F8);
		soundSetNameHash = ATSTRINGHASH("dlc_arc1_weapon_flash_grenade_sounds", 0xF038E85A);
		sceneNameHash = ATSTRINGHASH("arc1_weapons_flash_grenade_tinnitus_scene", 0x5EDD4C8E);
		break;

	case CExplosionManager::WeakEffect:
		soundNameHash = ATSTRINGHASH("tinnitus_weak", 0x2F62A64A);
		soundSetNameHash = ATSTRINGHASH("dlc_arc1_weapon_flash_grenade_sounds", 0xF038E85A);
		sceneNameHash = ATSTRINGHASH("arc1_weapons_flash_grenade_tinnitus_weak_scene", 0x436F160);
		break;
	}

	if (soundNameHash != 0u)
	{
		audSoundSet soundSet;
		if (soundSet.Init(soundSetNameHash))
		{
			audSoundInitParams initParams;
			initParams.Pan = 0;
			CreateAndPlaySound(soundSet.Find(soundNameHash), &initParams);
		}
	}

	if (sceneNameHash != 0u)
	{
		DYNAMICMIXER.StartScene(sceneNameHash);
	}

	m_RequestedTinnitusEffect = CExplosionManager::NoEffect;
}

void audFrontendAudioEntity::NotifyFrontendInput()
{
	if(m_FirstFEInput)
	{
		if(m_MusicState == AUD_FEMUSIC_PLAYING)
		{
			const u32 numChannelsToSpeedUp = 8;
			const f32 scalars[numChannelsToSpeedUp][2] = { 
				{0.8f, 1.f}, //organ
				{0.75f, 0.97f}, //organ wet
				{0.6f, 0.8f}, //cello
				{0.7f, 0.8f}, // cello wet
				{0.5f, 0.75f}, // string 1
				{0.4f, 0.7f}, // string1 wet
				{0.3f, 0.6f}, // string2 
				{0.2f, 0.5f}}; //string2 wet

			for(u32 i = 0; i < numChannelsToSpeedUp; i++)
			{
				if(m_ChannelState[i] == AUD_FEMUSIC_CHANNEL_WAITING)
				{
					const f32 lobbyScalar = (audNorthAudioEngine::IsInNetworkLobby() ? 1.5f : 1.f);
					m_ChannelWaitTime[i] -= audEngineUtil::GetRandomNumberInRange(scalars[i][0] * lobbyScalar * m_ChannelWaitTime[i], scalars[i][1] * lobbyScalar * m_ChannelWaitTime[i]);
				}
			}
			m_FirstFEInput = false;
		}
		else
		{
			m_StartMusicQuickly = true;
		}
	}
}

void audFrontendAudioEntity::PlaySound(const atHashWithStringBank soundName,const char *soundSetName)
{
#if __BANK
	if(sm_ShowScaleformSoundsPlaying)
	{
		naDisplayf("Soundset: %s Sound: %s",soundSetName,soundName.GetCStr());
	}
#endif
	audSoundSet soundSet;
	soundSet.Init(soundSetName);
	if(soundSet.IsInitialised())
	{
		audSoundInitParams initParams;	
		// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
		initParams.AllowOrphaned = true;
		initParams.TimerId = 1;
		initParams.Pan = 0;
		CreateDeferredSound(soundSet.Find(soundName.GetHash()), NULL, &initParams, false, false, false, true);
	}
	else
	{
		audWarningf("Couldn't initialize the soundset :%s",soundSetName);
	}

	// Ensure sounds during loading screens (or paused replays) get pushed through
	if((CLoadingScreens::AreActive() && !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive())))
	{
		audNorthAudioEngine::MinimalUpdate();
	}
}

void audFrontendAudioEntity::PlaySoundMapZoom()
{
	m_ShouldBePlayingMapZoom = true;	
}

void audFrontendAudioEntity::HandleMapZoomSoundEvent()
{
	if(!m_MapZoomSound)
	{
#if __BANK
		if(sm_ShowScaleformSoundsPlaying)
		{
			naDisplayf("Soundset: HUD_FRONTEND_DEFAULT_SOUNDSET Sound: MAP_ZOOM time %d frame %d", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
		}
#endif
		audSoundSet soundSet;
		soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1));
		if(soundSet.IsInitialised())
		{
			audSoundInitParams initParams;	
			// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
			initParams.AllowOrphaned = true;
			initParams.TimerId = 1;
			initParams.Pan = 0;
			CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("MAP_ZOOm", 0xF1AB59B4)),&m_MapZoomSound ,&initParams);
		}
		else
		{
			audWarningf("Couldn't initialize the soundset :HUD_FRONTEND_DEFAULT_SOUNDSET");
		}
	}
}

void audFrontendAudioEntity::HandleNavLeftRightContinousEvent()
{
	if(!m_NavLeftRightContinuousSound)
	{
#if __BANK
		if(sm_ShowScaleformSoundsPlaying)
		{
			naDisplayf("Soundset: HUD_FRONTEND_DEFAULT_SOUNDSET Sound: CONTINUOUS_SLIDER time %d frame %d", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
		}
#endif
		audSoundSet soundSet;
		soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1));
		if(soundSet.IsInitialised())
		{
			audSoundInitParams initParams;	
			// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
			initParams.AllowOrphaned = true;
			initParams.TimerId = 1;
			initParams.Pan = 0;			
			CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("CONTINUOUS_SLIDER", 0x47C7D8E0)), &m_NavLeftRightContinuousSound, &initParams);

			if(m_NavLeftRightContinuousSound)
			{
				m_NavLeftRightContinuousSound->SetShouldPersistOverClears(true);
			}
		}
		else
		{
			audWarningf("Couldn't initialize the soundset :HUD_FRONTEND_DEFAULT_SOUNDSET");
		}
	}
}

void audFrontendAudioEntity::StopSoundMapZoom()
{
	m_ShouldBePlayingMapZoom = false;
}
void audFrontendAudioEntity::PlaySoundMapMovement(f32 moveRate /* = 1.f */)
{
	m_ShouldBePlayingMapMove = true;
	m_MapCursorMoveRate = moveRate;
}

void audFrontendAudioEntity::PlaySoundNavLeftRightContinuous()
{
	m_ShouldBePlayingNavLeftRightContinuous = true;
}

void audFrontendAudioEntity::HandleMapMoveSoundEvent()
{
	if(!m_MapMovementSound)
	{
#if __BANK
		if(sm_ShowScaleformSoundsPlaying)
		{
			naDisplayf("Soundset: HUD_FRONTEND_DEFAULT_SOUNDSET Sound: MAP_MOVEMENT time %d frame %d", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
		}
#endif
		audSoundSet soundSet;
		soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1));
		if(soundSet.IsInitialised())
		{
			audSoundInitParams initParams;	
			// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
			initParams.AllowOrphaned = true;
			initParams.TimerId = 1;
			initParams.Pan = 0;
#if RSG_PC 
			if( m_MapCursorMoveRate != -1.0f )
			{
				initParams.SetVariableValue(ATSTRINGHASH("MoveRate", 0x24535F0E),m_MapCursorMoveRate);
				CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("MAP_MOVEMENT_MOUSE", 0xDFE085DB)),&m_MapMovementSound ,&initParams);
			}
			else
#endif
			{
				CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("MAP_MOVEMENT", 0xA33059CD)),&m_MapMovementSound ,&initParams);
			}

		}
		else
		{
			audWarningf("Couldn't initialize the soundset :HUD_FRONTEND_DEFAULT_SOUNDSET");
		}
	}
#if RSG_PC
	else
	{
		m_MapMovementSound->SetVariableValueDownHierarchyFromName(ATSTRINGHASH("MoveRate", 0x24535F0E),m_MapCursorMoveRate);
	}
#endif 
}

void audFrontendAudioEntity::StopSoundMapMovement()
{
	m_ShouldBePlayingMapMove = false;
}

void audFrontendAudioEntity::ProcessDeferredSoundEvents()
{
	if(m_ShouldBePlayingMapMove)
	{
		HandleMapMoveSoundEvent();
	}
	else if(m_MapMovementSound)
	{
		m_MapMovementSound->Stop();
	}
	
	if(m_ShouldBePlayingMapZoom)
	{
		HandleMapZoomSoundEvent();
	}
	else if(m_MapZoomSound)
	{
		m_MapZoomSound->Stop();
	}

	if(m_ShouldBePlayingNavLeftRightContinuous)
	{
		HandleNavLeftRightContinousEvent();

		// Automatically stop on the next frame - external systems must continually 
		// request to keep the sound alive
		m_ShouldBePlayingNavLeftRightContinuous = false;
	}
	else if(m_NavLeftRightContinuousSound)
	{
		m_NavLeftRightContinuousSound->Stop();
	}

#if GTA_REPLAY

	if( m_ShouldBePlayingReplayScrub )
	{
		HandleReplayScrubSoundEvent();
	}
	else if(m_replayScrubbingSound)
	{
		m_replayScrubbingSound->Stop();
	}
#endif
}

void audFrontendAudioEntity::StartPauseMenuFirstHalf()
{
	m_HasToPlayPauseMenu = true;
}
void audFrontendAudioEntity::StartPauseMenuFirstHalfPrivate()
{
#if __BANK
	if(sm_ShowScaleformSoundsPlaying)
	{
		naDisplayf("Soundset: HUD_FRONTEND_DEFAULT_SOUNDSET Sound: PAUSE_FIRST_HALF");
	}
#endif
	audSoundSet soundSet;
	soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1));
	if(soundSet.IsInitialised() && !m_PauseMenuFirstHalf)
	{
		audSoundInitParams initParams;	
		// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
		initParams.AllowOrphaned = true;
		initParams.TimerId = 1;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("PAUSE_FIRST_HALF", 0x52D70C15)),&m_PauseMenuFirstHalf, &initParams);
	}
	else if (!m_PauseMenuFirstHalf)
	{
		audWarningf("Couldn't initialize the soundset HUD_FRONTEND_DEFAULT_SOUNDSET");
	}
	m_HasToPlayPauseMenu = false;
	m_PlayedPauseMenuFirstHalf = true;
}
void audFrontendAudioEntity::StopPauseMenuFirstHalf()
{
	if(m_PauseMenuFirstHalf)
	{
		m_PauseMenuFirstHalf->StopAndForget();
	}
}
void audFrontendAudioEntity::StartPauseMenuSecondHalf()
{
	if (m_PlayedPauseMenuFirstHalf)
	{
		m_HasToPlayPauseMenuSecondHalf = true;
		m_PlayedPauseMenuFirstHalf = false;
	}
}

#if GTA_REPLAY

void audFrontendAudioEntity::PlaySoundReplayScrubbing()
{
	m_ShouldBePlayingReplayScrub = true;
}

void audFrontendAudioEntity::StopSoundReplayScrubbing()
{
	m_ShouldBePlayingReplayScrub = false;
}

void audFrontendAudioEntity::HandleReplayScrubSoundEvent()
{
	if(!m_replayScrubbingSound)
	{
#if __BANK
		if(sm_ShowScaleformSoundsPlaying)
		{
			naDisplayf("Soundset: HUD_FRONTEND_DEFAULT_SOUNDSET Sound: CONTINUOUS_SLIDER time %d frame %d", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
		}
#endif
		audSoundSet soundSet;
		soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1));
		if(soundSet.IsInitialised())
		{
			audSoundInitParams initParams;	
			// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
			initParams.AllowOrphaned = true;
			initParams.TimerId = 1;
			initParams.Pan = 0;
			CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("CONTINUOUS_SLIDER", 0x47C7D8E0)),&m_replayScrubbingSound ,&initParams);

			if(m_replayScrubbingSound)
			{
				m_replayScrubbingSound->SetShouldPersistOverClears(true);				
			}
		}
		else
		{
			audWarningf("Couldn't initialize the soundset :HUD_FRONTEND_DEFAULT_SOUNDSET");
		}
	}
}

#endif // GTA_REPLAY

void audFrontendAudioEntity::StartPauseMenuSecondHalfPrivate()
{
#if __BANK
	if(sm_ShowScaleformSoundsPlaying)
	{
		naDisplayf("Soundset: HUD_FRONTEND_DEFAULT_SOUNDSET Sound: PAUSE_SECOND_HALF");
	}
#endif
	audSoundSet soundSet;
	soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1));
	if(soundSet.IsInitialised())
	{
		audSoundInitParams initParams;	
		// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
		initParams.AllowOrphaned = true;
		initParams.TimerId = 1;
		initParams.Pan = 0;
		CreateAndPlaySound(soundSet.Find(ATSTRINGHASH("PAUSE_SECOND_HALF", 0xA34ED6D7)), &initParams);
	}
	else 
	{
		audWarningf("Couldn't initialize the soundset HUD_FRONTEND_DEFAULT_SOUNDSET");
	}
	m_HasToPlayPauseMenuSecondHalf = false;
}
void audFrontendAudioEntity::StartSpecialAbility(u32 playerHash)
{	
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		m_SpecialAbilityMode = kSpecialAbilityModeNone;
		return;
	}
#endif

	switch(playerHash)
	{
	case 0x55932F38: //ATSTRINGHASH("Michael", 0x55932F38)
		m_SpecialAbilityMode = kSpecialAbilityModeMichael;
		break;
	case 0x44C24694: // Franklin
		m_SpecialAbilityMode = kSpecialAbilityModeFranklin;
		break;
	case 0x2737D5AC:
		m_SpecialAbilityMode = kSpecialAbilityModeTrevor;
		break;
	case 0xDDA1F78:
		m_SpecialAbilityMode = kSpecialAbilityModeMultiplayer;
	default:
		break;
	}

	StartSpecialAbilitySounds();
}

void audFrontendAudioEntity::StartSpecialAbilitySounds()
{
	if(!m_SpecialAbilitySound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());

		u32 playerHash = ATSTRINGHASH("Michael", 0x55932F38);
		if(GetSpecialAbilityMode() == kSpecialAbilityModeFranklin)
		{
			playerHash = ATSTRINGHASH("Franklin", 0x44C24694);
		}
		else if(GetSpecialAbilityMode() == kSpecialAbilityModeTrevor)
		{
			playerHash = ATSTRINGHASH("Trevor", 0x2737D5AC);
		}
		else if(GetSpecialAbilityMode() == kSpecialAbilityModeMultiplayer)
		{
			playerHash = ATSTRINGHASH("Multiplayer", 0xDDA1F78);
		}
		
		CreateSound_PersistentReference(m_SpecialAbilitySounds.Find(playerHash), &m_SpecialAbilitySound, &initParams);

		// Don't record the special ability sound for replays - we now strip out all slow-mo effects so it makes no sense if it triggers
		//REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_SpecialAbilitySounds.GetNameHash(), playerHash, &initParams, m_SpecialAbilitySound, NULL, eFrontendSoundEntity);)

		if(m_SpecialAbilitySound)
		{
			CPed * localPlayer = CGameWorld::FindLocalPlayer();
			if(localPlayer && localPlayer->GetSpecialAbility())
			{
				m_SpecialAbilitySound->FindAndSetVariableValue(ATSTRINGHASH("SpecialAbilityTime", 0xBDF9C044), localPlayer->GetSpecialAbility()->GetRemainingNormalized());
			}
			m_SpecialAbilitySound->PrepareAndPlay();
		}
		else
		{
			audWarningf("Failed to play special ability sound %u", playerHash);
		}
	}
}

void audFrontendAudioEntity::StartSwitch()
{
	if(!m_SwitchSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(ATSTRINGHASH("Switch", 0xD60E64BB)), &m_SwitchSound, &initParams);
	}
	m_SwitchIsActive = true;
}

void audFrontendAudioEntity::StartWhiteWarningSwitch()
{
	if(!m_WhiteWarningSwitchSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(ATSTRINGHASH("SwitchWhiteWarning", 0xB2AB4308)), &m_WhiteWarningSwitchSound, &initParams);
	}
}

void audFrontendAudioEntity::StartRedWarningSwitch()
{
	if(!m_RedWarningSwitchSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(ATSTRINGHASH("SwitchRedWarning", 0x5A0DCA5E)), &m_RedWarningSwitchSound, &initParams);
	}
}

void audFrontendAudioEntity::StartWeaponWheel()
{
	if(!m_WeaponWheelSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		u32 soundNameHash = ATSTRINGHASH("WeaponWheel", 0xD20D0C07);
		if(CNetwork::IsGameInProgress())
		{
			soundNameHash = ATSTRINGHASH("WeaponWheel_MP", 0xF1ADFD1A);
		}
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(soundNameHash), &m_WeaponWheelSound, &initParams);
	}
	m_HasPlayedWeaponWheelSound = true;
}

void audFrontendAudioEntity::StartStunt()
{
	if(!m_StuntSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(ATSTRINGHASH("Stunt", 0x81794C70)), &m_StuntSound, &initParams);
	}
}

void audFrontendAudioEntity::StartCinematic()
{
	if(!m_CinematicSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(ATSTRINGHASH("Cinematic", 0x9C002B5E)), &m_CinematicSound, &initParams);
	}
}

void audFrontendAudioEntity::StartGeneralSlowMo()
{
	if(!m_SlowMoGeneralSound)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(m_SpecialAbilitySoundSet.Find(ATSTRINGHASH("General", 0x6B34FE60)), &m_SlowMoGeneralSound, &initParams);
	}
}

void audFrontendAudioEntity::StopSpecialAbility()
{
	m_SpecialAbilityMode = kSpecialAbilityModeNone;

	if(m_SpecialAbilitySound)
	{
		m_SpecialAbilitySound->StopAndForget();
	}
}

void audFrontendAudioEntity::StopGeneralSlowMo()
{
	if(m_SlowMoGeneralSound)
	{
		m_SlowMoGeneralSound->StopAndForget();
	}
}


void audFrontendAudioEntity::StopCinematic()
{
	if(m_CinematicSound)
	{
		m_CinematicSound->StopAndForget();
	}
}

void audFrontendAudioEntity::StopSwitch()
{
	if(m_SwitchSound)
	{
		m_SwitchSound->StopAndForget();
	}
	m_SwitchIsActive = false;
}
void audFrontendAudioEntity::StopWhiteWarningSwitch()
{
	if(m_WhiteWarningSwitchSound)
	{
		m_WhiteWarningSwitchSound->StopAndForget();
	}
}
void audFrontendAudioEntity::StopRedWarningSwitch()
{
	if(m_RedWarningSwitchSound)
	{
		m_RedWarningSwitchSound->StopAndForget();
	}
}
void audFrontendAudioEntity::StopStunt()
{
	if(m_StuntSound)
	{
		m_StuntSound->StopAndForget();
	}
}

void audFrontendAudioEntity::StopWeaponWheel()
{
	if(m_WeaponWheelSound)
	{
		m_WeaponWheelSound->StopAndForget();
	}
	m_HasPlayedWeaponWheelSound = false;
}

void audFrontendAudioEntity::StartLoadingTune()
{
	if(PARAM_noloadingtune.Get() || GetControllerId() == AUD_INVALID_CONTROLLER_ENTITY_ID)
	{
		return;
	}

#if 0
	if(!m_HasMutedForLoadingScreens && m_LoadSceneCategoryController)
	{
		audDisplayf("Muting BASE");
		m_LoadSceneCategoryController->SetVolumeLinear(0.f);
		m_HasMutedForLoadingScreens = true;
	}
#else
	if(!m_LoadingScreenScene)
	{
		DYNAMICMIXER.StartScene(ATSTRINGHASH("LOADING_SCREENS_SCENE", 0x9A8F7310), &m_LoadingScreenScene);
	}
#endif
	if(!m_LoadingTune)
	{
		audWaveSlot *slot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("INTERACTIVE_MUSIC_1", 0x9205C7A1));
		
		audSoundInitParams initParams;
		Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());


		if(!m_HasPlayedLoadingTune)
		{
#if RSG_PC
			initParams.Predelay = 11000;
#else
			initParams.Predelay = 8000;
#endif
		}

		CreateSound_PersistentReference(m_SoundSet.Find(ATSTRINGHASH("LoadingTune", 0x96119697)), &m_LoadingTune, &initParams);
		if(m_LoadingTune)
		{
			m_HasPlayedLoadingTune = true;
			m_LoadingTune->PrepareAndPlay(slot, true, -1);
			audNorthAudioEngine::MinimalUpdate();
		}
	}

}

void audFrontendAudioEntity::StopLoadingTune(bool longRelease)
{
	if(m_LoadingTune)
	{
		if(longRelease)
		{
			m_LoadingTune->SetReleaseTime(2700);
		}
		m_LoadingTune->StopAndForget();
	}

	// Start applying the frontend music slider value now that we're finished with the loading tune.
	audNorthAudioEngine::OnLoadingTuneStopped();

#if 0
	if(m_HasMutedForLoadingScreens)
	{
		audDisplayf("Unmuting BASE");
		m_LoadSceneCategoryController->SetVolumeLinear(1.f);
		m_HasMutedForLoadingScreens = false;
	}
#else
	if(m_LoadingScreenScene)
	{
		m_LoadingScreenScene->Stop();
		m_LoadingScreenScene = NULL;
	}
#endif
}

void audFrontendAudioEntity::StartInstall()
{
	if(m_LoadingTune)
	{
		m_LoadingTune->FindAndSetVariableValue(ATSTRINGHASH("IsInstalling", 0x689241BC), 1.f);
	}
}

void audFrontendAudioEntity::StopInstall()
{
	if(m_LoadingTune)
	{
		m_LoadingTune->FindAndSetVariableValue(ATSTRINGHASH("IsInstalling", 0x689241BC), 0.f);
	}
}

bool audFrontendAudioEntity::IsLoadingTunePlaying() const
{
	if(!g_AudioEngine.IsAudioEnabled())
	{
		return true;
	}
	return (GetControllerId() != AUD_INVALID_CONTROLLER_ENTITY_ID && m_LoadingTune && m_LoadingTune->GetPlayState() == AUD_SOUND_PLAYING);
}

void audFrontendAudioEntity::Pickup(atHashWithStringNotFinal pickUpHash,const u32 /*modelIndex*/,const u32 /*modelNameHash*/,const CPed * pPed )
{
	if(pPed->IsLocalPlayer() && !m_PickUpSound)
	{
		audSoundInitParams initParams;
		audMetadataRef soundRef = g_NullSoundRef;
		soundRef = GetAudioPickUp(pickUpHash);
		initParams.Pan = 0;
		CreateAndPlaySound_Persistent(soundRef,&m_PickUpSound, &initParams);
	}
}

audMetadataRef audFrontendAudioEntity::GetAudioPickUp(atHashWithStringNotFinal pickUpHash)
{
	audMetadataRef soundRef = g_NullSoundRef;
	soundRef = m_PickUpStdSoundSet.Find(pickUpHash);
	if (soundRef != g_NullSoundRef)
	{
		return soundRef;
	}
	soundRef = m_PickUpWeaponsSoundSet.Find(pickUpHash);
	if (soundRef != g_NullSoundRef)
	{
		return soundRef;
	}
	soundRef = m_PickUpVehicleSoundSet.Find(pickUpHash);
	return soundRef;
}
bool audFrontendAudioEntity::SetModelSpecificPickupSound(const s32 modelIndex, const u32 soundNameHash)
{
	for(s32 i = 0; i < m_ModelSpecificPickupSounds.GetCount(); i++)
	{
		if(m_ModelSpecificPickupSounds[i].modelIndex == modelIndex)
		{
			m_ModelSpecificPickupSounds[i].soundHash = soundNameHash;
			return true;
		}
	}
	if(!m_ModelSpecificPickupSounds.IsFull())
	{
		audModelPickupSound &mps = m_ModelSpecificPickupSounds.Append();
		mps.modelIndex = modelIndex;
		mps.soundHash = soundNameHash;
		return true;
	}
	else
	{
		naErrorf("SetModelSpecificPickupSound: too many model specific pickup sounds");
		return false;
	}
}
void audFrontendAudioEntity::PickupWeapon(const u32 /*weaponHash*/, const u32 /*pickupType*/, const s32 /*modelIndex*/)
{
#if 0 // JG - This is done in new pickup code now

	//Get the weapon settings game object for this weapon type.
	if(weaponHash != 0)
	{
		WeaponSettings *weaponSettings = NULL;
		const CWeaponInfo *weaponInfo = CWeaponInfoManager::GetItemInfo<CWeaponInfo>(weaponHash);
		if(naVerifyf(weaponInfo, "Couldn't get weapon info from weaponHash passed into PickupWeapon"))
		{
			u32 nameHash = weaponInfo->GetModelHash();
			weaponSettings = audNorthAudioEngine::GetObject<WeaponSettings>(nameHash);
			naCErrorf(weaponSettings, "A new weapon has been added without the requisite audio metadata, or a weapon model has been renamed: %d; %s", weaponHash, weaponInfo->GetName());		
		}
		
		if(weaponSettings)
		{
			audSoundInitParams initParams;
			initParams.Category = g_FrontendGameWeaponPickupsCategory;
			CreateAndPlaySound(weaponSettings->PickupSound, &initParams);
		}
	}
	else
	{
		if(pickupType == CPickup::PICKUP_ONCE_FOR_MISSION)
		{
			static const audStringHash genericMissionPickup("FRONTEND_GAME_GENERIC_MISSION_PICKUP");
			u32 pickupSound = genericMissionPickup;
			for(s32 i = 0; i < m_ModelSpecificPickupSounds.GetCount(); i++)
			{
				if(m_ModelSpecificPickupSounds[i].modelIndex == modelIndex)
				{
					pickupSound = m_ModelSpecificPickupSounds[i].soundHash;
					break;
				}
			}
			audSoundInitParams initParams;
			initParams.Category = g_FrontendGameWeaponPickupsCategory;
			CreateAndPlaySound(pickupSound, &initParams);
		}
	}
	// default to generic pickup sound
//	Pickup(AUD_PICKUP_INFO);

#endif // 0
}

void audFrontendAudioEntity::UpdateRoomTone(u32 timeInMs)
{
	// If we're playing something, see if it's the same as we're meant to be - if not, stop it. 
	// If we're not, and it's not too soon, kick new one off.
	u32 newRoomTone = audNorthAudioEngine::GetGtaEnvironment()->GetRoomToneHash();
	// always position the room tone sound just in front of the camera - this is often irrelevant, but lets us turn on distance attenuation,
	// and hence get environmental effects
	Vector3 position = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	position.x += 0.5f; // arbitrary, just feels better not to be right on top of the camera

	if (m_RoomToneSound)
	{
		m_RoomToneSound->SetRequestedPosition(position);
	}
	
	if (m_CurrentRoomToneHash != newRoomTone)
	{
		// stop anything we're currently playing
		StopAndForgetSounds(m_RoomToneSound);
	}

	if (!m_RoomToneSound &&
			newRoomTone!=0 && newRoomTone!=g_NullSoundHash && (m_TimeLastPlayedRoomTone+500)<timeInMs)
	{
		audSoundInitParams initParams;
		initParams.Position = position;
		m_TimeLastPlayedRoomTone = timeInMs;
		m_CurrentRoomToneHash = newRoomTone;
	
		CreateAndPlaySound_Persistent(newRoomTone, &m_RoomToneSound, &initParams);	
	}
}

bool audFrontendAudioEntity::ShouldFEMusicBePlaying() const
{
	if(PARAM_nomenumusic.Get())
	{
		return false;
	}

	// disable menu music if the music studio session player is active
#if AUD_MUSIC_STUDIO_PLAYER
	if(m_MusicStudioSessionPlayer.IsActive())
	{
		return false;
	}
#endif // AUD_MUSIC_STUDIO_PLAYER

	// no frontend music if the mobile phone radio is on and not paused, or if playing fullscreen video
	if(CPauseMenu::ShouldPlayMusic() || CPauseMenu::ShouldPlayRadioStation() || m_IsFEMusicCodeDisabled 

#if GTA_REPLAY
		|| CVideoEditorUi::IsActive()
		|| CReplayMgr::IsEditModeActive()
#endif // #if GTA_REPLAY

		VIDEO_PLAYBACK_ONLY( || VideoPlayback::IsPlaybackActive() )
		)
	{
		return false;
	}
	return g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::PlayMenuMusic) 
		|| (!CNetwork::IsGameInProgress() && CPauseMenu::IsActive() && fwTimer::IsGamePaused());
}

void audFrontendAudioEntity::UpdateMusic()
{
	static u32 stemVolVarNames[g_NumChannelsInFrontendMusic] = {
		ATSTRINGHASH("Stem1_Vol", 0x2301A214),
		ATSTRINGHASH("Stem2_Vol", 0x2EE5EEFD),
		ATSTRINGHASH("Stem3_Vol", 0x3CA0BCA),
		ATSTRINGHASH("Stem4_Vol", 0x24FF609A),
		ATSTRINGHASH("Stem5_Vol", 0x6D64EEB6),
		ATSTRINGHASH("Stem6_Vol", 0xA90D6485),
		ATSTRINGHASH("Stem7_Vol", 0xBEC6363)
	};
	static u32 stemPanVarNames[g_NumChannelsInFrontendMusic] = {
		ATSTRINGHASH("Stem1_Pan", 0x360FEEAF),
		ATSTRINGHASH("Stem2_Pan", 0x87781F5C),
		ATSTRINGHASH("Stem3_Pan", 0x9E4BF269),
		ATSTRINGHASH("Stem4_Pan", 0xFBD5CD06),
		ATSTRINGHASH("Stem5_Pan", 0xE0859332),
		ATSTRINGHASH("Stem6_Pan", 0xCD215C2E),
		ATSTRINGHASH("Stem7_Pan", 0x6DEA5B77)
	};
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
	const f32 timeElapsedS = Clamp((timeInMs - m_LastUpdateTime) * 0.001f, 0.0f, 0.1f);

	AUD_MUSIC_STUDIO_PLAYER_ONLY(m_MusicStudioSessionPlayer.Update(timeInMs));

	if(!m_IsFrontendMusicEnabled && !fwTimer::IsGamePaused() && !CPauseMenu::IsActive())
	{
		// once the game has properly booted we can start playing frontend music; we don't want to play it during loading
		m_IsFrontendMusicEnabled = true;
	}

	switch(m_MusicState)
	{
	case AUD_FEMUSIC_IDLE:
		if(ShouldFEMusicBePlaying() && m_IsFrontendMusicEnabled)
		{
			m_MusicState = AUD_FEMUSIC_ACQUIRING_SLOT;
			m_FirstFEInput = true;
		}
		break;
	case AUD_FEMUSIC_ACQUIRING_SLOT:

		if(!m_MusicStreamSlot)
		{
			audStreamClientSettings clientSettings;
			clientSettings.priority = audStreamSlot::STREAM_PRIORITY_FRONTENDMENU;
			clientSettings.hasStoppedCallback = &HasMusicStoppedCallback;
			clientSettings.stopCallback = &OnMusicStopCallback;
			clientSettings.userData = 0;
			m_MusicStreamSlot = audStreamSlot::AllocateSlot(&clientSettings);
		}
		if(m_MusicStreamSlot && (m_MusicStreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
		{
			m_MusicState = AUD_FEMUSIC_PREPARING;
		}

		break;
	case AUD_FEMUSIC_PREPARING:
		{
			naAssertf(m_MusicStreamSlot, "No slot set for streaming music");		
			if(!m_MusicStreamingSound)
			{
				audSoundInitParams initParams;
				Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
				CreateSound_PersistentReference("TD_MENU_MUSIC", &m_MusicStreamingSound, &initParams);
			}
			if(m_MusicStreamingSound)
			{
				audPrepareState prepareState = m_MusicStreamingSound->Prepare(m_MusicStreamSlot->GetWaveSlot(), true);
				if(prepareState == AUD_PREPARED)
				{					
					m_MusicStreamingSound->Play();
					g_FEMusicVolCurve.Init("FEMUSIC_INTERP_TO_VOL");
					m_MusicState = AUD_FEMUSIC_PLAYING;
					
				}
				else if(prepareState == AUD_PREPARE_FAILED)
				{
					naErrorf("Failed to prepare FE music sound");
					m_MusicState = AUD_FEMUSIC_CLEANINGUP;
				}
			}
			else
			{
			//	Errorf("Failed to create FE music sound");
				m_MusicState = AUD_FEMUSIC_CLEANINGUP;
			}
		}
		break;
	case AUD_FEMUSIC_CLEANINGUP:
		{
			for(u32 i = 0; i < g_NumChannelsInFrontendMusic; i++)
			{
				m_ChannelState[i] = AUD_FEMUSIC_CHANNEL_MUTED;
			}

			if(m_MusicStreamingSound)
			{
				m_MusicStreamingSound->StopAndForget();
			}

			if(m_MusicStreamSlot)
			{
				m_MusicStreamSlot->Free();
				m_MusicStreamSlot = NULL;
			}
			m_MusicOverallIntensity = 0.f;
			m_MusicState = AUD_FEMUSIC_IDLE;
			m_FirstFEInput = true;
		}
		break;
	case AUD_FEMUSIC_PLAYING:
		{			
			// if the stream is starved we'll lose our sound, in which case need to restart streaming
			// also check for individual channel refs being nulled...not sure why this would happen but it evidently does
			bool needCleanup = (m_MusicStreamingSound == NULL);
			
			if(needCleanup)
			{
				m_MusicState = AUD_FEMUSIC_CLEANINGUP;
			}
			else
			{
				if(m_MusicTargetIntensity > m_MusicOverallIntensity)
				{
					m_MusicOverallIntensity += g_FEMusicIntensityRate * timeElapsedS;
				}
				else
				{
					m_MusicOverallIntensity -= g_FEMusicIntensityRate * timeElapsedS;
				}
				
				if(m_MusicOverallIntensity > 1.f)
				{
					m_MusicOverallIntensity = 1.f;
					m_MusicTargetIntensity = 0.f;
				}
				if(m_MusicOverallIntensity < 0.f)
				{
					m_MusicOverallIntensity = 0.f;
					m_MusicTargetIntensity = 1.f;
				}

				const f32 oneMinusIntensity = 1.f - m_MusicOverallIntensity;

				for(u32 i = 0; i < g_NumChannelsInFrontendMusic; i++)
				{	
					switch(m_ChannelState[i])
					{
					case AUD_FEMUSIC_CHANNEL_PLAYING:
						{				
							const f32 panInterp = m_ChannelInterp[i] * 0.5f;
							const f32 pan = Lerp(panInterp, m_ChannelStartPan[i], m_ChannelTargetPan[i]);

							const f32 linVol = g_FEMusicVolCurve.CalculateValue(m_ChannelInterp[i]);
													
							m_MusicStreamingSound->FindAndSetVariableValue(stemVolVarNames[i], audDriverUtil::ComputeDbVolumeFromLinear(linVol));
							m_MusicStreamingSound->FindAndSetVariableValue(stemPanVarNames[i], pan);

							m_ChannelInterp[i] += g_FEChannelInterpSpeed * m_ChannelInterpSpeed[i] * timeElapsedS;

							if(m_ChannelInterp[i] >= 2.f)
							{
								m_ChannelState[i] = AUD_FEMUSIC_CHANNEL_MUTED;
							}
						}
						break;
					case AUD_FEMUSIC_CHANNEL_MUTED:
						m_ChannelStartPan[i] = audEngineUtil::GetRandomNumberInRange(0.f, 359.f);
						m_ChannelTargetPan[i] = audEngineUtil::GetRandomNumberInRange(0.f, 359.f);
						m_ChannelInterp[i] = 0.f;
						m_ChannelInterpSpeed[i] = audEngineUtil::GetRandomNumberInRange(0.5f, 2.f) * g_FEChannelSpeedScale[i];
						m_ChannelState[i] = AUD_FEMUSIC_CHANNEL_WAITING;

						m_ChannelWaitTime[i] = audEngineUtil::GetRandomNumberInRange(30.f, 75.f) * (g_FEChannelWaitScale[i] + oneMinusIntensity*g_FEChannelIntensityScale[i]);

						m_MusicStreamingSound->FindAndSetVariableValue(stemVolVarNames[i], g_SilenceVolume);

						break;
					case AUD_FEMUSIC_CHANNEL_WAITING:
						m_ChannelWaitTime[i] -= timeElapsedS;
						if(m_ChannelWaitTime[i] <= 0.f)
						{
							m_ChannelState[i] = AUD_FEMUSIC_CHANNEL_PLAYING;
						}
						
						m_MusicStreamingSound->FindAndSetVariableValue(stemVolVarNames[i], g_SilenceVolume);
						break;
					}
				}

				if(!ShouldFEMusicBePlaying())
				{
					m_MusicState = AUD_FEMUSIC_CLEANINGUP;
				}

				if(m_StartMusicQuickly || audNorthAudioEngine::IsInNetworkLobby() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::PlayMenuMusic))
				{
					// since the lobby isnt the actual frontend need to force it to start music quickly
					NotifyFrontendInput();
					m_StartMusicQuickly = false;
				}
			}
			break;
		}
	}

	//  In order to utilise reverb, we have to play the menu music through the positioned effect route which means that the volume is controlled by
	// the SFX slider rather than the music slider.  To compensate, reduce the volume of the sound based on the difference between the sliders.
	if(m_MusicStreamingSound)
	{
		const float sfxVolumeDB = audDriverUtil::ComputeDbVolumeFromLinear(audNorthAudioEngine::GetSfxVolume());
		const float musicVolumeDB = audDriverUtil::ComputeDbVolumeFromLinear(audNorthAudioEngine::GetMusicVolume());

		const float feMusicVolComp = Min(0.f, musicVolumeDB - sfxVolumeDB);
		m_MusicStreamingSound->SetRequestedVolume(feMusicVolComp);
	}

	m_LastUpdateTime = timeInMs;

	PF_SET(Stem1, m_ChannelInterp[0]);
	PF_SET(Stem2, m_ChannelInterp[1]);
	PF_SET(Stem3, m_ChannelInterp[2]);
	PF_SET(Stem4, m_ChannelInterp[3]);
	PF_SET(Stem5, m_ChannelInterp[4]);
	PF_SET(Stem6, m_ChannelInterp[5]);
	PF_SET(Stem7, m_ChannelInterp[6]);
	PF_SET(Intensity, m_MusicOverallIntensity);

}

bool audFrontendAudioEntity::OnMusicStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop the music
	// shouldn't actually ever happen
	g_FrontendAudioEntity.m_MusicState = AUD_FEMUSIC_CLEANINGUP;
	return true;
}

bool audFrontendAudioEntity::HasMusicStoppedCallback(u32 UNUSED_PARAM(userData))
{
	g_FrontendAudioEntity.UpdateMusic();
	return (g_FrontendAudioEntity.m_MusicState == AUD_FEMUSIC_IDLE);
}

void audFrontendAudioEntity::TriggerLongSwitchSkyLoop(eArcadeTeam arcadeTeam)
{	
	if (arcadeTeam != eArcadeTeam::AT_NONE)
	{
		if (!m_CnCSkyLoops[(u32)arcadeTeam])
		{
			audSoundSet soundSet;

			if (soundSet.Init(GetCnCSwitchSoundsetName(arcadeTeam)))
			{
				audDisplayf("Triggering sky loop sound for team %s", arcadeTeam == eArcadeTeam::AT_CNC_COP ? "Cops" : "Crooks");
				CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("Sky_Loop", 0xB89C9C52)), &m_CnCSkyLoops[(u32)arcadeTeam]);
			}
		}
	}
}

void audFrontendAudioEntity::StopLongSwitchSkyLoop()
{
	for (u32 i = 0; i < m_CnCSkyLoops.GetMaxCount(); i++)
	{
		if (m_CnCSkyLoops[i])
		{
			audDisplayf("Stopping sky loop sound for team %s", i == (u32)eArcadeTeam::AT_CNC_COP ? "Cops" : "Crooks");
			m_CnCSkyLoops[i]->StopAndForget();
		}
	}
}

u32 audFrontendAudioEntity::GetCnCSwitchSoundsetName(eArcadeTeam arcadeTeam)
{
	u32 soundsetName = 0u;

	switch (arcadeTeam)
	{	
	case eArcadeTeam::AT_CNC_COP:
		soundsetName = ATSTRINGHASH("Arc1_CnC_Switch_Cop_Sounds", 0xB74E5166);
		break;
	case eArcadeTeam::AT_CNC_CROOK:
		soundsetName = ATSTRINGHASH("Arc1_CnC_Switch_Crook_Sounds", 0x3FD64BE1);
		break;
	default:	
		break;	
	}

	return soundsetName;
}

void audFrontendAudioEntity::TriggerLongSwitchSound(const atHashWithStringBank soundNameHash, eArcadeTeam arcadeTeam)
{
	bool playedOneshotSound = false;
	
	if (arcadeTeam != eArcadeTeam::AT_NONE)
	{
		audSoundSet copsAndCrooksSoundset;

		if (copsAndCrooksSoundset.Init(GetCnCSwitchSoundsetName(arcadeTeam)))
		{
			audDisplayf("Triggering long switch sound %s for team %s", soundNameHash.TryGetCStr(), arcadeTeam == eArcadeTeam::AT_CNC_COP ? "Cops" : "Crooks");
			CreateAndPlaySound(copsAndCrooksSoundset.Find(soundNameHash));
			playedOneshotSound = true;
		}
	}
	
	// Fallback to regular switch sound in the event that we can't initialize a C&C specific soundset
	if(!playedOneshotSound)
	{
		CreateAndPlaySound(m_LongSwitchSounds.Find(soundNameHash));
	}	
}

#if GTA_REPLAY
bool audFrontendAudioEntity::ShouldMuteSpeech()
{
	if(GetSpeechOn() == false)
		return true;

	return !GetClipSpeechOn();
}

void audFrontendAudioEntity::SetClipMusicVolumeIndex(s32 index)
{
	Assert(index >= 0 && index < REPLAY_VOLUMES_COUNT);
	m_ClipMusicVolumeIndex = index;
	m_ClipMusicVolume = g_fMontageVolumeTable[index];
}

void audFrontendAudioEntity::SetClipSFXVolumeIndex(s32 index)
{
	Assert(index >= 0 && index < REPLAY_VOLUMES_COUNT);
	m_ClipSFXVolumeIndex = index;
	m_ClipSFXVolume = g_fMontageVolumeTable[index];
}

void audFrontendAudioEntity::SetClipDialogVolumeIndex(s32 index)
{
	Assert(index >= 0 && index < REPLAY_VOLUMES_COUNT);
	m_ClipDialogVolumeIndex = index;
	m_ClipDialogVolume = g_fMontageVolumeTable[index];
}

void audFrontendAudioEntity::SetClipCustomAmbienceVolumeIndex(s32 index)
{
    Assert(index >= 0 && index < REPLAY_VOLUMES_COUNT);
    m_ClipCustomAmbienceVolumeIndex = index;
    m_ClipCustomAmbienceVolume = g_fMontageVolumeTable[index];
}

void audFrontendAudioEntity::SetClipSFXOneshotVolumeIndex(s32 index)
{
    Assert(index >= 0 && index < REPLAY_VOLUMES_COUNT);
    m_ClipSFXOneshotVolumeIndex = index;
    m_ClipSFXOneshotVolume = g_fMontageVolumeTable[index];
}

void audFrontendAudioEntity::SetClipFadeVolLinear(f32 volumeLinear)	
{ 
	m_ClipSFXFadeVolume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear); 
}

f32 audFrontendAudioEntity::VolumeIndexToDb(s32 index)
{ 
	Assert(index >= 0 && index < REPLAY_VOLUMES_COUNT);
	return g_fMontageVolumeTable[index]; 
}

#endif

#if AUD_MUSIC_STUDIO_PLAYER
bool audMusicStudioSessionPlayer::Init(audEntity *audioEntity)
{
	sysMemZeroBytes<sizeof(m_Sections)>(&m_Sections[0]);
	m_State = AUD_AMS_IDLE;
	m_Parent = audioEntity;
	m_RequestedSectionIndex = m_ActiveSectionIndex = 0;
	m_HavePlayedTransition = false;
	m_SectionTransitionBar = 0;
	m_SessionLoadedScene = m_SessionPlayingScene = NULL;
	m_SoloState.Reset();
	m_Muted = false;
	m_EnableOcclusion = true;
	m_MuteVolumeSmoother.Init(1.3f / 1000.f, 0.5f / 1000.f, 0.f, 1.f);
	return true;
}

void audMusicStudioSessionPlayer::Shutdown()
{
	if(m_State != AUD_AMS_IDLE)
	{
		m_State = AUD_AMS_CLEANINGUP;
		CleanUp();
	}
}

bool audMusicStudioSessionPlayer::CleanUp()
{
	m_RequestedToPlay = false;

	bool streamSlotsFree = true;

	if(m_SessionLoadedScene)
	{

		m_SessionLoadedScene->Stop();
	}
	if(m_SessionPlayingScene)
	{
		m_SessionPlayingScene->Stop();
	}

	for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
	{
		if(m_Sections[i].Sound)
		{
			m_Sections[i].Sound->StopAndForget();
		}

		if(m_Sections[i].StreamSlot)
		{
			if(m_Sections[i].StreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE)
			{
				m_Sections[i].StreamSlot->Free();
			}

			streamSlotsFree &= m_Sections[i].StreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_IDLE;
		}		
	}

	if(streamSlotsFree)
	{
		for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
		{
			m_Sections[i].StreamSlot = nullptr;
		}

		audWaveSlot* virtualWaveSlots[2] = 
		{
			virtualWaveSlots[0] = audWaveSlot::FindWaveSlot(ATSTRINGHASH("STREAM_VIRTUAL_0", 0x3FF7870B)),
			virtualWaveSlots[1] = audWaveSlot::FindWaveSlot(ATSTRINGHASH("STREAM_VIRTUAL_1", 0x4E352386))
		};

		bool banksUnloaded = virtualWaveSlots[0]->ForceUnloadBank();
		banksUnloaded &= virtualWaveSlots[1]->ForceUnloadBank();

		if(banksUnloaded)
		{
			virtualWaveSlots[0]->ForceSetBankMemory(nullptr, 0);
			virtualWaveSlots[1]->ForceSetBankMemory(nullptr, 0);

			u32 bankID = audWaveSlot::FindBankId("RESIDENT\\EXPLOSIONS");
			audAssert(bankID != AUD_INVALID_BANK_ID);

			audWaveSlot* sourceWaveSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("STATIC_EXPLOSIONS", 0x1E64499C));

			if(sourceWaveSlot->GetLoadedBankId() != bankID) 
			{
				sourceWaveSlot->LoadBank(bankID);
			}
			
			return true;
		}
	}

	return false;
}
#if RSG_BANK
char g_SessionClockInfo[64]={0};
#endif // RSG_BANK

void audMusicStudioSessionPlayer::Update(const u32 UNUSED_PARAM(timeInMs))
{
	const u32 partVariationHashes[kMusicStudioSessionParts] = {
		ATSTRINGHASH("p1_variation", 0xDE60923C),
		ATSTRINGHASH("p2_variation", 0x2B29246D),
		ATSTRINGHASH("p3_variation", 0x55178236),
		ATSTRINGHASH("p4_variation", 0x20DC67C1)
	};

	const u32 partVolumeHashes[kMusicStudioSessionParts] = {
		ATSTRINGHASH("p1_vol", 0x79B02E56),
		ATSTRINGHASH("p2_vol", 0x99945B63),
		ATSTRINGHASH("p3_vol", 0xA03C2A60),
		ATSTRINGHASH("p4_vol", 0x902C7A9E)
	};

	const u32 partHpfHashes[kMusicStudioSessionParts] = {
		ATSTRINGHASH("p1_hpf", 0xE00119EB),
		ATSTRINGHASH("p2_hpf", 0xD6F67CEA),
		ATSTRINGHASH("p3_hpf", 0xF654354F),
		ATSTRINGHASH("p4_hpf", 0x96C2B4BD)
	};

	const u32 partLpfHashes[kMusicStudioSessionParts] = {
		ATSTRINGHASH("p1_lpf", 0xACEBDF61),
		ATSTRINGHASH("p2_lpf", 0xC3F95BBC),
		ATSTRINGHASH("p3_lpf", 0x2E67EFA6),
		ATSTRINGHASH("p4_lpf", 0x3C40A275)
	};

	switch(m_State)
	{
	case AUD_AMS_IDLE:
		break;
	case AUD_AMS_VIRTUALIZING_SLOT:
		{
			audWaveSlot* sourceWaveSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("STATIC_EXPLOSIONS", 0x1E64499C));

			if(sourceWaveSlot->ForceUnloadBank())
			{
				audWaveSlot* virtualWaveSlots[2] = 
				{
					virtualWaveSlots[0] = audWaveSlot::FindWaveSlot(ATSTRINGHASH("STREAM_VIRTUAL_0", 0x3FF7870B)),
					virtualWaveSlots[1] = audWaveSlot::FindWaveSlot(ATSTRINGHASH("STREAM_VIRTUAL_1", 0x4E352386))
				};

				const u32 sourceSlotSize = sourceWaveSlot->GetSlotSize();
				const u32 virtualSlotSize = ((sourceSlotSize >> 1) - 2047) & ~(2047); // Splitting one slot into two, align to 2k

				u8* sourceSlotData = sourceWaveSlot->GetSlotData();

				if(virtualWaveSlots[0]->ForceSetBankMemory(&sourceSlotData[0], virtualSlotSize) &&
				   virtualWaveSlots[1]->ForceSetBankMemory(&sourceSlotData[virtualSlotSize], virtualSlotSize))
				{
					m_State = AUD_AMS_ACQUIRING_SLOT; // Intentional fall through
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	case AUD_AMS_ACQUIRING_SLOT:
		{
			bool slotsReady = true;
		
			for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
			{
				if(!m_Sections[i].StreamSlot)
				{
					audStreamClientSettings clientSettings;
					clientSettings.priority = audStreamSlot::STREAM_PRIORITY_SCRIPT;
					clientSettings.hasStoppedCallback = &HasMusicStoppedCallback;
					clientSettings.stopCallback = &OnMusicStopCallback;
					clientSettings.userData = (u32)i;
					m_Sections[i].StreamSlot = audStreamSlot::AllocateVirtualSlot(&clientSettings);
				}
				if(!m_Sections[i].StreamSlot || (m_Sections[i].StreamSlot->GetState() != audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
				{
					slotsReady = false;
				}
			}

			if(slotsReady)
			{
				m_State = AUD_AMS_PREPARING;
			}
		}
		break;
	case AUD_AMS_PREPARING:
		{
			bool allPrepared = true;
		
			for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
			{
				naAssertf(m_Sections[i].StreamSlot, "No slot set for music studio");		
				if(!m_Sections[i].Sound)
				{
					audSoundInitParams initParams;
					Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());

					const char *sectionNames[] = {"A_Section", "B_Section"};
					m_Parent->CreateSound_PersistentReference(m_SessionSet.Find(sectionNames[i]), &m_Sections[i].Sound, &initParams);
				}
				if(m_Sections[i].Sound)
				{
					audPrepareState prepareState = m_Sections[i].Sound->Prepare(m_Sections[i].StreamSlot->GetWaveSlot(), true);
					if(prepareState == AUD_PREPARED)
					{					
						
					}
					else
					{
						allPrepared = false;
						if(prepareState == AUD_PREPARE_FAILED)
						{
							naErrorf("Failed to prepare music studio sound");
							m_State = AUD_AMS_CLEANINGUP;
						}
					}
					
				}
			}
			if(!allPrepared)
			{
				break;
			}
			
			else 
			{
				m_State = AUD_AMS_PREPARED;
				// intentional fall-through
			}
		}
		
	case AUD_AMS_PREPARED:
		if(!m_SessionLoadedScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("dlc_security_music_studio_session_active_scene", 0x16719848), &m_SessionLoadedScene);
		}
		if(!m_RequestedToPlay)
		{
			break;
		}
		else
		{
			// start playback
			m_RequestedDelayedSettings = false; // ensure we don't delay applying initial settings
			s32 syncId = audMixerSyncManager::Get()->Allocate();
			for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
			{
				m_Sections[i].Sound->SetRequestedSyncSlaveId(syncId);
				m_Sections[i].Sound->SetRequestedSyncMasterId(syncId);

				m_Sections[i].Sound->Play();
			}
			m_State = AUD_AMS_PLAYING;
			// intentional fall-through			
		}
	case AUD_AMS_PLAYING:
		{


			if(!m_SessionPlayingScene && !m_Muted)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("dlc_security_music_studio_session_playing_scene", 0xFB936566), &m_SessionPlayingScene);
			}
			if(m_SessionPlayingScene && m_Muted)
			{
				m_SessionPlayingScene->Stop();				
			}

			float fakeOcclusionVolume = 0.f;
			u32 fakeOcclusionLPF = 24000;

			if(m_EnableOcclusion && audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance() && audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance()->GetModelNameHash() == ATSTRINGHASH("sf_dlc_studio_sec", 0x7B87D75B))
			{
				const s32 roomId = audNorthAudioEngine::GetGtaEnvironment()->GetInteriorRoomId();
				
				if(roomId == 7) // control room where the mixer app is placed
				{
					fakeOcclusionLPF = 24000;
					fakeOcclusionVolume = 0.f;
				}
				else if(roomId == 6) // smoking room (also has a mixer app)
				{
					fakeOcclusionLPF = 24000;
					fakeOcclusionVolume = -3.f;
				}
				else
				{
					const Vec3V emitterPos(-994.f,-67.f,-99.f);
					float fakeDist2 = MagSquared((g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()-emitterPos)).Getf();
					
					if(roomId == 3 || roomId == 4) // entrance to control room
					{
						fakeOcclusionLPF = 12000;
					}
					else if(roomId == 5)
					{
						fakeOcclusionLPF = 6000;
					}
					else
					{
						fakeOcclusionLPF = 2400;
					}

					fakeOcclusionVolume += Lerp(ClampRange(fakeDist2, 16.f, 600.f), 0.f, -32.f);
				}
			}

			m_MuteVolumeSmoother.CalculateValue(m_Muted ? 0.f : 1.f, fwTimer::GetTimeStep());

			if(m_Sections[0].Sound)
			{
				for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
				{
					// volume is set further down
					m_Sections[i].Sound->SetRequestedLPFCutoff(fakeOcclusionLPF);
				}


				audTempoMap::TimelinePosition timelinePosition;
				float soundPlayTime = 0.f;

				// if we can, grab the streaming sound and use the play time of the wave for improved accuracy
				if(m_Sections[0].Sound->GetSoundTypeID() == StreamingSound::TYPE_ID)
				{
					soundPlayTime = ((audStreamingSound*)m_Sections[0].Sound)->GetCurrentPlayTimeOfWave() / 1000.f;					
				}
				else
				{
					soundPlayTime = m_Sections[0].Sound->GetCurrentPlayTime() / 1000.f;
				}

				bool isSoundLooping = false;
				const float soundDuration = m_Sections[0].Sound->ComputeDurationMsExcludingStartOffsetAndPredelay(&isSoundLooping) / 1000.f;
				m_TempoMap.ComputeTimelinePosition(soundPlayTime, timelinePosition);
				m_CurrentPosition = timelinePosition; // cache for script access
				// See if we need to transition from one section to the other
				if(m_RequestedSectionIndex != m_ActiveSectionIndex || m_RequestedDelayedSettings)
				{

					// transition on the next bar line
					if(m_SectionTransitionBar == 0)
					{
						// Always transition on an even bar so we hit the even bar line after the transition, to match the 2-bar phrasing in the track
						if(timelinePosition.Beat <= 3 && (timelinePosition.Bar & 1) == 0)
						{
							m_SectionTransitionBar = timelinePosition.Bar;
						}
						else
						{
							m_SectionTransitionBar = timelinePosition.Bar + 1;
							m_SectionTransitionBar += (m_SectionTransitionBar & 1);
						}

						audTempoMap::TimelinePosition transitionTimelinePos;
						transitionTimelinePos.Bar = m_SectionTransitionBar + 1; // plus one because the transition occurs at the end of the transition bar
						m_PlayTimeOfNextTransition = m_TempoMap.ComputeTimeForPosition(transitionTimelinePos);
						if(m_PlayTimeOfNextTransition > soundDuration + 0.1f) // adding epsilon here to ensure we don't wrap too early
						{
							audDisplayf("Transition exceeds loop length; wrapping (bar %d becoming %d, transitionTime: %f length: %f", m_SectionTransitionBar, 2, m_PlayTimeOfNextTransition, soundDuration);
							m_SectionTransitionBar = 2;

							transitionTimelinePos.Bar = m_SectionTransitionBar + 1; // plus one because the transition occurs at the end of the transition bar
							m_PlayTimeOfNextTransition = m_TempoMap.ComputeTimeForPosition(transitionTimelinePos);
						}

					}


					if(!m_HavePlayedTransition && m_SectionTransitionBar == timelinePosition.Bar)
					{
						audSoundInitParams initParams;
						//if(timelinePosition.Beat > 1)
						{
							// if we are transitioning on this bar then offset into the transition piece
							float timeThroughBar = ((timelinePosition.Beat-1) * timelinePosition.BeatLength) + timelinePosition.BeatOffsetS;
							initParams.StartOffset = (s32)(timeThroughBar * 1000.f);
							initParams.AttackTime = (s32)(timelinePosition.BeatLength*1000.f);
						}
						audDisplayf("Transition start offset %d (%u beats * %f + %f)", initParams.StartOffset, timelinePosition.Beat, timelinePosition.BeatLength, timelinePosition.BeatOffsetS);
						initParams.Volume = fakeOcclusionVolume + audDriverUtil::ComputeDbVolumeFromLinear(m_MuteVolumeSmoother.GetLastValue());
						initParams.LPFCutoff = fakeOcclusionLPF;
						m_Parent->CreateAndPlaySound(m_SessionSet.Find(ATSTRINGHASH("Transition", 0xB9933991)), &initParams);
						m_HavePlayedTransition = true;
					}
					// deal with wrap-around when checking if we have completed our transition
					if(m_HavePlayedTransition && timelinePosition.Bar != m_SectionTransitionBar)
					{
						m_ActiveSectionIndex = m_RequestedSectionIndex;
						m_HavePlayedTransition = false;
						m_SectionTransitionBar = 0;
						m_TimeUntilNextTransition = 0.f;
						m_RequestedDelayedSettings = false;
					}
					else
					{
						// Update time until next transition for script
						if(m_PlayTimeOfNextTransition < soundPlayTime)
						{
							m_TimeUntilNextTransition = (soundDuration - soundPlayTime) + m_PlayTimeOfNextTransition;
						}
						else
						{
							m_TimeUntilNextTransition = m_PlayTimeOfNextTransition - soundPlayTime;
						}					
					}
				}


#if RSG_BANK
				formatf(g_SessionClockInfo, "%u:%u:%.2f - %f", timelinePosition.Bar, timelinePosition.Beat, timelinePosition.BeatOffsetS, m_TimeUntilNextTransition);
#endif // RSG _BANK
			}
			
			// apply part volumes / variation selection
			if(m_RequestedToPlay)
			{
				const bool isSoloActive = m_SoloState.AreAnySet();

				for(s32 section = 0; section < kMaxMusicStudioSessionSections; section++)
				{
					if(!m_Sections[section].Sound)
					{
						audErrorf("Music studio sound unexpectedly stopped for section %d", section);
						m_State = AUD_AMS_CLEANINGUP;
						break;
					}
					// set an overall hpf when muting to simulate taking headphones off during the volume fade
					m_Sections[section].Sound->SetRequestedHPFCutoff(m_Muted ? 800U : 0U);

					if(!m_RequestedDelayedSettings)
					{
						m_Sections[section].Sound->SetRequestedVolume(section == m_ActiveSectionIndex ? 0.f : g_SilenceVolume);

						for(s32 part = 0; part < kMusicStudioSessionParts; part++)
						{
							m_Sections[section].Parts[part].AppliedVolume = m_Sections[section].Parts[part].Volume;

							m_Sections[section].Sound->FindAndSetVariableValue(partVariationHashes[part], static_cast<f32>(m_Sections[section].Parts[part].VariationIndex));
							m_Sections[section].Sound->FindAndSetVariableValue(partLpfHashes[part], static_cast<f32>(m_Sections[section].Parts[part].Lpf));
							m_Sections[section].Sound->FindAndSetVariableValue(partHpfHashes[part], static_cast<f32>(m_Sections[section].Parts[part].Hpf));
						}
					}

					// we apply volume separately so we can process mute, solo and fake occlusion outside of any delayed settings
					for(s32 part = 0; part < kMusicStudioSessionParts; part++)
					{
						float volToSet = m_Sections[section].Parts[part].AppliedVolume + fakeOcclusionVolume + audDriverUtil::ComputeDbVolumeFromLinear(m_MuteVolumeSmoother.GetLastValue());
						if(isSoloActive && !m_SoloState.IsSet(part))
						{
							volToSet = g_SilenceVolume;
						}
						m_Sections[section].Sound->FindAndSetVariableValue(partVolumeHashes[part], volToSet);
					}
				}
			}
			else 
			{
				bool soundValid = false;

				if(m_SessionPlayingScene)
				{
					m_SessionPlayingScene->Stop();
				}

				for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
				{
					if(m_Sections[i].Sound)
					{
						soundValid = true;
						m_Sections[i].Sound->StopAndForget();
					}
				}

				// Wait a frame after calling StopAndForget otherwise we hit bucket full asserts 
				// due to the new sounds being created before the old ones have fully cleaned up
				if(!soundValid)
				{
					m_State = AUD_AMS_PREPARING;
				}
			}
		}
		break;
	case AUD_AMS_CLEANINGUP:
		if(CleanUp())
		{
			m_State = AUD_AMS_IDLE;
		}
		break;
	}
}

void audMusicStudioSessionPlayer::RequestDelayedSettings()
{
	m_RequestedDelayedSettings = true;
}

bool audMusicStudioSessionPlayer::LoadSession(const char *sessionName)
{
	if(!m_SessionSet.IsInitialised() || m_SessionSet.GetNameHash() != atStringHash(sessionName))
	{	
		m_SessionSet.Init(sessionName);
		// look up tempo info
		char tempoFieldName[64];
		formatf(tempoFieldName, "%s_tempo", sessionName);
		float tempo = audConfig::GetValue<float>(tempoFieldName,0.f);
		audDisplayf("Session tempo for %s is %f", sessionName, tempo);
		m_TempoMap.Reset();
		m_TempoMap.AddTempoChange(0.f, tempo, 4);
		m_SessionTempo = tempo;

		if(m_State == AUD_AMS_PREPARING || m_State == AUD_AMS_PREPARED || m_State == AUD_AMS_PLAYING)
		{
			for(s32 i = 0; i < kMaxMusicStudioSessionSections; i++)
			{
				if(m_Sections[i].Sound)
				{
					m_Sections[i].Sound->StopAndForget();
				}
			}

			m_State = AUD_AMS_PREPARING;
		}
	}

	if(m_State == AUD_AMS_IDLE)
	{
		m_State = AUD_AMS_VIRTUALIZING_SLOT;
	}

	return m_State == AUD_AMS_PREPARED || m_State == AUD_AMS_PLAYING;
}

void audMusicStudioSessionPlayer::SetSoloState(const bool solo1, const bool solo2, const bool solo3, const bool solo4)
{
	m_SoloState.Set(0,solo1);
	m_SoloState.Set(1,solo2);
	m_SoloState.Set(2,solo3);
	m_SoloState.Set(3,solo4);
}

void audMusicStudioSessionPlayer::SetPartVariationIndex(const s32 sectionIndex, const s32 partIndex, const s32 variationIndex)
{
	if(sectionIndex < kMaxMusicStudioSessionSections && partIndex < kMusicStudioSessionParts && sectionIndex >= 0 && partIndex >= 0)
	{
		m_Sections[sectionIndex].Parts[partIndex].VariationIndex = variationIndex;
	}
}

void audMusicStudioSessionPlayer::SetPartVolume(const s32 sectionIndex, const s32 partIndex, const float volumeDb)
{
	if(sectionIndex < kMaxMusicStudioSessionSections && partIndex < kMusicStudioSessionParts && sectionIndex >= 0 && partIndex >= 0)
	{
		m_Sections[sectionIndex].Parts[partIndex].Volume = volumeDb;
	}
}

void audMusicStudioSessionPlayer::SetPartLPF(const s32 sectionIndex, const s32 partIndex, const bool lpfFilterOn)
{
	if(sectionIndex < kMaxMusicStudioSessionSections && partIndex < kMusicStudioSessionParts && sectionIndex >= 0 && partIndex >= 0)
	{
		m_Sections[sectionIndex].Parts[partIndex].Lpf = lpfFilterOn ? 800 : (kMixerNativeSampleRate/2);
	}
}

void audMusicStudioSessionPlayer::SetPartHPF(const s32 sectionIndex, const s32 partIndex, const bool hpfFilterOn)
{
	if(sectionIndex < kMaxMusicStudioSessionSections && partIndex < kMusicStudioSessionParts && sectionIndex >= 0 && partIndex >= 0)
	{
		m_Sections[sectionIndex].Parts[partIndex].Hpf = hpfFilterOn ? 400 : 0;
	}
}

void audMusicStudioSessionPlayer::RequestSectionTransition(const s32 sectionIndex)
{
	m_RequestedSectionIndex = sectionIndex;
}

void audMusicStudioSessionPlayer::UnloadSession()
{
	m_State = AUD_AMS_CLEANINGUP;
}

bool audMusicStudioSessionPlayer::OnMusicStopCallback(u32 UNUSED_PARAM(userData))
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().m_State = AUD_AMS_CLEANINGUP;
	return true;
}

bool audMusicStudioSessionPlayer::HasMusicStoppedCallback(u32 UNUSED_PARAM(userData))
{
	return !g_FrontendAudioEntity.GetMusicStudioSessionPlayer().IsActive();
}

void audMusicStudioSessionPlayer::GetTimingInfo(float &nextBeatTimeS, float &bpm, s32 &beatNum, float &timeUntilNextSectionTransitionS)
{
	timeUntilNextSectionTransitionS = m_TimeUntilNextTransition;
	bpm = m_SessionTempo;
	nextBeatTimeS = m_CurrentPosition.BeatLength - m_CurrentPosition.BeatOffsetS;
	beatNum = (s32)m_CurrentPosition.Beat;
}
#if __BANK
char g_RagSessionName[64] = {0};

s32 g_RagAVariation[4] = {1,1,1,1};
f32 g_RagAVolume[4] = {0.f,0.f,0.f,0.f};
bool g_RagAHPF[4] = {false,false,false};
bool g_RagALPF[4] = {false,false,false,false};
s32 g_RagBVariation[4] = {2,1,0,1};
f32 g_RagBVolume[4] = {0.f,0.f,0.f,0.f};
bool g_RagBHPF[4] = {false,false,false,false};
bool g_RagBLPF[4] = {false,false,false,false};
bool g_RagSolo[4] = {false,false,false,false};
bool g_RagMusicSessionMute = false;
s32 g_RagSection = 0;

void Rag_UpdateVariations()
{
	for(s32 i = 0; i < 4; i++)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartVariationIndex(0, i, g_RagAVariation[i]);
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartVolume(0, i, g_RagAVolume[i]);
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartHPF(0, i, g_RagAHPF[i]);
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartLPF(0, i, g_RagALPF[i]);

		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartVariationIndex(1, i, g_RagBVariation[i]);
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartVolume(1, i, g_RagBVolume[i]);
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartHPF(1, i, g_RagBHPF[i]);
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartLPF(1, i, g_RagBLPF[i]);
	}	
}

void Rag_LoadSession()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().LoadSession(g_RagSessionName);
}

void Rag_UnloadSession()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().UnloadSession();
}

void Rag_StartPlayback()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().StartPlayback();
	Rag_UpdateVariations();
}

void Rag_StopPlayback()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().StopPlayback();
}

void Rag_UpdateSection()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().RequestSectionTransition(g_RagSection);
}

void Rag_RequestDelayedSettings()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().RequestDelayedSettings();
}

void Rag_UpdateSolo()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetSoloState(g_RagSolo[0],g_RagSolo[1],g_RagSolo[2],g_RagSolo[3]);
}

void Rag_UpdateMute()
{
	g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetMuteState(g_RagMusicSessionMute);
}

void audMusicStudioSessionPlayer::AddWidgets(bkBank &bank)
{

	formatf(g_RagSessionName, "dlc_security_studio_session_01");
	bank.PushGroup("Music Studio Session Player");
		bank.AddText("Session Name", g_RagSessionName, sizeof(g_RagSessionName), false);
		bank.AddButton("Load Session", CFA(Rag_LoadSession));
		bank.AddButton("Unload Session", CFA(Rag_UnloadSession));
		bank.AddButton("Start Playback", CFA(Rag_StartPlayback));
		bank.AddButton("Stop Playback", CFA(Rag_StopPlayback));
		bank.AddButton("RequestDelayedSettings", CFA(Rag_RequestDelayedSettings));
		bank.AddToggle("Mute", &g_RagMusicSessionMute, CFA(Rag_UpdateMute));
		bank.AddToggle("Solo 1", &g_RagSolo[0], CFA(Rag_UpdateSolo));
		bank.AddToggle("Solo 2", &g_RagSolo[1],  CFA(Rag_UpdateSolo));
		bank.AddToggle("Solo 3", &g_RagSolo[2],  CFA(Rag_UpdateSolo));
		bank.AddToggle("Solo 4", &g_RagSolo[3], CFA(Rag_UpdateSolo));
		bank.AddText("Clock_MinutesSeconds", g_SessionClockInfo, sizeof(g_SessionClockInfo), true);		

		bank.AddSlider("Section", &g_RagSection, 0, 1, 1, CFA(Rag_UpdateSection));
		s32 p = 0;

		bank.PushGroup("A Section");
			
			bank.AddSlider("Part 1 Variation", &g_RagAVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
			bank.AddSlider("Part 1 Volume", &g_RagAVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 1 LPF", &g_RagALPF[p], CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 1 HPF", &g_RagAHPF[p],  CFA(Rag_UpdateVariations));

			p++;
			bank.AddSlider("Part 2 Variation", &g_RagAVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
			bank.AddSlider("Part 2 Volume", &g_RagAVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 2 LPF", &g_RagALPF[p], CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 2 HPF", &g_RagAHPF[p],  CFA(Rag_UpdateVariations));

			p++;
			bank.AddSlider("Part 3 Variation", &g_RagAVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
			bank.AddSlider("Part 3 Volume", &g_RagAVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 3 LPF", &g_RagALPF[p], CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 3 HPF", &g_RagAHPF[p],  CFA(Rag_UpdateVariations));

			p++;
			bank.AddSlider("Part 4 Variation", &g_RagAVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
			bank.AddSlider("Part 4 Volume", &g_RagAVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 4 LPF", &g_RagALPF[p], CFA(Rag_UpdateVariations));
			bank.AddToggle("Part 4 HPF", &g_RagAHPF[p],  CFA(Rag_UpdateVariations));
		bank.PopGroup();

		p = 0;

		bank.PushGroup("B Section");
		
		bank.AddSlider("Part 1 Variation", &g_RagBVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
		bank.AddSlider("Part 1 Volume", &g_RagBVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 1 LPF", &g_RagBLPF[p], CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 1 HPF", &g_RagBHPF[p],  CFA(Rag_UpdateVariations));

		p++;
		bank.AddSlider("Part 2 Variation", &g_RagBVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
		bank.AddSlider("Part 2 Volume", &g_RagBVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 2 LPF", &g_RagBLPF[p], CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 2 HPF", &g_RagBHPF[p],  CFA(Rag_UpdateVariations));

		p++;
		bank.AddSlider("Part 3 Variation", &g_RagBVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
		bank.AddSlider("Part 3 Volume", &g_RagBVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 3 LPF", &g_RagBLPF[p], CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 3 HPF", &g_RagBHPF[p],  CFA(Rag_UpdateVariations));

		p++;
		bank.AddSlider("Part 4 Variation", &g_RagBVariation[p], 0, 2, 1, CFA(Rag_UpdateVariations));
		bank.AddSlider("Part 4 Volume", &g_RagBVolume[p], -100.f, 0.f, 1.f, CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 4 LPF", &g_RagBLPF[p], CFA(Rag_UpdateVariations));
		bank.AddToggle("Part 4 HPF", &g_RagBHPF[p],  CFA(Rag_UpdateVariations));
		bank.PopGroup();

	bank.PopGroup();
}

#endif // __BANK
#endif // AUD_MUSIC_STUDIO_PLAYER

#if RSG_BANK
void audFrontendAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("FrontendAudio", false);
	AUD_MUSIC_STUDIO_PLAYER_ONLY(g_FrontendAudioEntity.GetMusicStudioSessionPlayer().AddWidgets(bank));
	bank.AddToggle("Print scaleform sounds.", &sm_ShowScaleformSoundsPlaying);
	
		bank.PushGroup("FEMusic",false);
			bank.AddSlider("ReverbSize", &g_FrontendMusicReverbLarge, 0.f, 1.f, 0.0001f);
			bank.AddSlider("ReverbHPF", &g_FrontendMusicLargeReverbHPF, 0.0f, kVoiceFilterLPFMaxCutoff, 0.1f);
			bank.AddSlider("DelayFeedback", &g_FrontendMusicLargeReverbDelayFeedback, -100.0f, 0.f, 0.1f);
			bank.AddSlider("DrumsLPF", &g_FrontendMusicDrumsLPF, 100, kVoiceFilterLPFMaxCutoff, 1);
		bank.PopGroup();
	bank.PopGroup();
}

#endif


