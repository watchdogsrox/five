// 
// audio/radioaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audiodefines.h"

#if NA_RADIO_ENABLED

#include "audio/emitteraudioentity.h"
#include "audiohardware/driver.h"
#include "audioengine/engine.h"
#include "camera/CamInterface.h"
#include "camera/scripted/ScriptedFlyCamera.h"
#include "cutsceneaudioentity.h"
#include "frontendAudioEntity.h"
#include "music/musicplayer.h"
#include "northaudioengine.h"
#include "radioaudioentity.h"
#include "radioslot.h"
#include "radiostation.h"
#include "scriptaudioentity.h"

#include "debugaudio.h"
#include "frontend/MobilePhone.h"
#include "frontend/PauseMenu.h"
#include "game/ModelIndices.h"
#include "grcore/debugdraw.h"
#include "network/NetworkInterface.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "system/ControlMgr.h"
#include "vehicles/vehicle.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"

#if GTA_REPLAY
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"
#include "replayaudioentity.h"
#endif

AUDIO_MUSIC_OPTIMISATIONS()
	
audRadioAudioEntity g_RadioAudioEntity;

f32 g_RadioVolumeControlSmoothRate	= 1.0f;
f32 g_RadioVolumeControlChange		= 3.0f;
u32 g_RadioVolumeHoldTime			= 60000;
u32 g_RadioVolumeRampTime			= 30000;
f32 g_MobilePhoneRadioVolumeOffset	= 4.0f;

extern bool g_PositionedPlayerVehicleRadioEnabled;

const f32 g_AmountToDuckMusicForReplaySFX	= -9.0f;

const u32 g_MinRadioRetuneDelayMs					= 1000;
//const u32 g_MaxPlayerOutOfVehicleTimeForAutoTune	= 15000;
const f32 g_VehicleRadioProbability					= 0.85f;
#if !__FINAL
u32 g_RadioDebugSkipForwardTimeMs				= 60000;	//1 minute.
u32 g_RadioDebugSkipToTransitionTimeMs			= 10000;
#endif // !__FINAL

u32 audRadioAudioEntity::sm_LastRetuneTime = 0;
u8 audRadioAudioEntity::sm_PendingPlayerRadioStationRetunes = 0;
bool audRadioAudioEntity::sm_ForceVehicleExplicitRetune = false;
bool audRadioAudioEntity::sm_IsMobilePhoneRadioActive = false;
bool audRadioAudioEntity::sm_IsMobilePhoneRadioAvailable = false;
#if __BANK
bool audRadioAudioEntity::sm_ShouldPerformRadioRetuneTest = false;
u8 audRadioAudioEntity::sm_RequestedRadioStationIndex = 0;
u32 audRadioAudioEntity::sm_RetunePeriodMs = 3000;
const char *audRadioAudioEntity::sm_RadioStationNames[g_MaxRadioStations];
bool g_DrawRadioDebug = AUD_DEBUG_STREAMING;
bool g_DrawRadioSlotDebug = false;
bkCombo* g_RadioStationCombo = NULL;
char g_StationDebugNameFilter[128] = {'\0'};
bool g_DrawRadioStationDebug = false;
bool g_DrawTakeoverStation1 = false;
extern bool g_DebugDrawUSBStation;
bool g_DrawAudibleTrack = false;
f32 g_StationDebugDrawYScroll = 0.f;
bool g_DebugSimulateStreamSlotStarvation = false;
bool g_ForceUnlockSolomun = false;
bool g_ForceUnlockTaleOfUs = false;
bool g_ForceUnlockDixon = false;
bool g_ForceUnlockBlackMadonna = false;
bool g_PrioritizeSolomun = false;
bool g_PrioritizeTaleOfUs = false;
bool g_PrioritizeDixon = false;
bool g_PrioritizeBlackMadonna = false;

#if __WIN32PC
bool audRadioAudioEntity::sm_DebugUserMusic = false;
#endif // __WIN32PC

#if GTA_REPLAY
char g_PreviewTrackName[128]={"RADIO_01_CLASS_ROCK_IF_YOU_LEAVE_ME_NOW"};
#endif

#endif // __BANK

#if !__FINAL
bool audRadioAudioEntity::sm_ShouldSkipForward = false;
bool audRadioAudioEntity::sm_ShouldQuickSkipForward = false;
bool audRadioAudioEntity::sm_ShouldMuteRadio = false;
#endif // !__FINAL

PARAM(muteradio, "[Audio] Mute radio content.");

audRadioAudioEntity::audRadioAudioEntity()
{
}


void audRadioAudioEntity::InitClass(void)
{
#if HEIST3_HIDDEN_RADIO_ENABLED
	audWaveSlot::GetEncryptionKeyOverrideCallback = &GetEncryptionKeyForBank;
#endif

	sm_LastRetuneTime = 0;
	sm_PendingPlayerRadioStationRetunes = 0;

#if __BANK
	sm_ShouldPerformRadioRetuneTest = false;
	sm_RequestedRadioStationIndex = 0;
	sm_RetunePeriodMs = 3000;
#endif // __BANK

#if !__FINAL
	sm_ShouldSkipForward = false;
	sm_ShouldQuickSkipForward = false;
	sm_ShouldMuteRadio = PARAM_muteradio.Get();
#endif // !__FINAL

	//Load the radio definitions and instantiate the stations.
	audRadioStation::InitClass();
	// Set up radio slots
	audRadioSlot::InitClass();

#if __BANK
	PopulateRetuneWidgetStationNames();
#endif // __BANK
}

void audRadioAudioEntity::DLCInitClass()
{
	audRadioStation::MaintainHeist4SPStationState();

	u32 numStations = audRadioStation::GetNumTotalStations();
	for (u32 radioStationIndex = 0; radioStationIndex < numStations; radioStationIndex++)
	{
		audRadioStation *station = audRadioStation::GetStation(radioStationIndex);

		if (station)
		{
			station->DLCInit();
		}
	}

#if __BANK
	Displayf("[Telemetry] Radio Station IMMUTABLE_INDEX_TUNEABLE List:");

	for(u32 i = 0; i < audRadioStation::GetNumTotalStations(); i++)
	{
		if(audRadioStation* radioStation = audRadioStation::FindStationWithImmutableIndex(i, IMMUTABLE_INDEX_TUNEABLE))
		{
			Displayf("[Telemetry]     %u: %s", i, radioStation->GetName());
		}
	}

	Displayf("[Telemetry] Radio Station IMMUTABLE_INDEX_GLOBAL List:");

	for(u32 i = 0; i < audRadioStation::GetNumTotalStations(); i++)
	{
		if(audRadioStation* radioStation = audRadioStation::FindStationWithImmutableIndex(i, IMMUTABLE_INDEX_GLOBAL))
		{
			Displayf("[Telemetry]     %u: %s", i, radioStation->GetName());
		}
	}
#endif
}

#if __BANK
void audRadioAudioEntity::PopulateRetuneWidgetStationNames()
{
	//Populate a list of radio station names for the retune widget.
	u32 numStations = audRadioStation::GetNumTotalStations();
	for (u32 radioStationIndex = 0; radioStationIndex < numStations; radioStationIndex++)
	{
		const audRadioStation *station = audRadioStation::GetStation(radioStationIndex);
		if (station)
		{
			sm_RadioStationNames[radioStationIndex] = station->GetName();

			if (g_RadioStationCombo)
			{
				g_RadioStationCombo->SetString(radioStationIndex, sm_RadioStationNames[radioStationIndex]);
			}			
		}
		else
		{
			sm_RadioStationNames[radioStationIndex] = NULL;
		}
	}
}
#endif

void audRadioAudioEntity::ShutdownClass(void)
{
	audRadioSlot::ShutdownClass();
	audRadioStation::ShutdownClass();
}

f32 g_RadioPositionedToStereoTime = 0.5f;

void audRadioAudioEntity::Init(void)
{
	m_PlayerVehicleRadioState = PLAYER_RADIO_OFF;
	m_MobilePhoneRadioState = PLAYER_RADIO_OFF;
	m_IsPlayerInVehicleForRadio = false;
	m_IsPlayerInAVehicleWithARadio = false;
	m_WasPlayerInVehicleForRadioLastFrame = false;
	sm_IsMobilePhoneRadioActive = false;
	m_WasMobilePhoneRadioActiveLastFrame = false;

	m_IgnorePlayerVehicleRadioShutdown = false;
	m_IsPlayerRadioSwitchedOff = false;
	m_PlayerVehicle = NULL;
	m_LastPlayerVehicle = NULL;
	m_LastActiveRadioTimeMs = 0;
	m_PlayerRadioStation = NULL;
	m_CurrentStationIndex = 0;
	m_PlayerRadioStationPendingRetune = NULL;
	m_LastPlayerRadioStation = NULL;
	m_RequestedRadioStation = NULL;
	m_ForcePlayerStation = NULL;

#if GTA_REPLAY
	m_ReplayMusicSound = NULL;
	m_PreviouslyRequestedReplayTrack = g_NullSoundHash;
	m_PreviouslyRequestedReplayTrackID = -1;
	m_ShouldPrepareReplayMusic = false;
	m_ShouldStopReplayMusic = false;
	m_StopReplayMusicPreview = false;
	m_StopReplayMusicNoRelease = false;
	m_ShouldPlayReplayMusic = false;
	m_ShouldPlayReplayPreview = false;
	m_ShouldFadeReplayPreview = false;
	m_ShouldUpdateReplayPreview = false;
	m_ShouldUpdateReplayMusic = false;
	m_ReplayMusicStartTime = 0;
	m_PreviewReplayTrack = g_NullSoundHash;
	m_HasParsedMarkers = false;
	m_RockoutStartOffset = 0;
	m_RockoutEndOffset = 0;
	m_PreviewReplayTrackTime = 30000;
	m_ReplayMusicFadeOutScalar = 1.f;

	m_WasDuckingMusicForSFX = false;
	m_SFXMusicDuckEndTime = 0;
	m_SFXFullMusicDuckVolume = 0.0f;
#endif

	m_PlayerVehicleInsideFactor = 0.0f;
	m_RetuneLastReleaseTime = 0;
	m_VolumeLastReleaseTime = 0;
	m_LastPlayerModelId = 0;
	
	m_CachedAudibleBeatValid = false;
	m_CachedTimeTillNextAudibleBeat = 0.f;
	m_CachedAudibleBeatBPM = 0.f;
	m_CachedAudibleBeatNum = 0;

	m_AutoUnfreezeForPlayer = true;
	m_AutoUnfreezeForPlayerTimer = 0.f;

	m_AreRetunesMuted = false;

	m_StopEmittersForPlayerVehicle = false;
	m_ForcePlayerCharacterStations = false;

	m_VolumeOffset = 0.f;

	m_SkipOnOffSound = false;

	m_FrontendFadeTime = g_RadioPositionedToStereoTime;
	m_FrontendFadeTimeResetTimer = -1.f;
	
#if __BANK
	m_LastTestRetuneTimeMs = 0;
#endif // __BANK

	naAudioEntity::Init();
	audEntity::SetName("audRadioAudioEntity");

	const f32 increaseDecreaseRate = 0.001f / m_FrontendFadeTime;
	m_PositionedToStereoVolumeSmoother.Init(increaseDecreaseRate, increaseDecreaseRate, 0.0f, 1.0f);

	m_InFrontendMode = false;
	m_WasMobileRadioActiveOnEnteringFrontend = false;

	m_LastTimeVehicleRadioWasRetuned = 0;
	m_VehicleRadioJustRetuned = false;
	m_VehicleRadioRetuneAlreadyFlagged = true;

	m_FadeVolumeLinear = 1.f;
	m_FadeState = kNotFading;

	m_RadioSounds.Init(ATSTRINGHASH("RADIO_SOUNDSET", 0x951CE826));

	audRadioStation::ResetHistoryForNewGame();

	m_bMPDriverHasTriggeredRadioChange = false;
}

void audRadioAudioEntity::Pause()
{
	g_AudioEngine.GetSoundManager().Pause(2);
}

void audRadioAudioEntity::Unpause()
{
	g_AudioEngine.GetSoundManager().UnPause(2);
}

#if GTA_REPLAY
f32 audRadioAudioEntity::GetNextReplayBeat(u32 soundHash, f32 startOffsetMS)
{
	if(!audRadioTrack::IsScoreTrack(soundHash))
	{
		return GetReplayBeat(soundHash, 1, startOffsetMS);
	}
	else
	{
		return g_InteractiveMusicManager.GetNextReplayBeat(soundHash, startOffsetMS);
	}
}

f32 audRadioAudioEntity::GetPrevReplayBeat(u32 soundHash, f32 startOffsetMS)
{
	if(!audRadioTrack::IsScoreTrack(soundHash))
	{
		return GetReplayBeat(soundHash, -1, startOffsetMS);		
	}
	else
	{
		return g_InteractiveMusicManager.GetPrevReplayBeat(soundHash, startOffsetMS);
	}
}


bool audRadioAudioEntity::HasBeatMarkers(u32 soundHash)
{
	if(audRadioTrack::IsScoreTrack(soundHash))
	{
		// Assume that all score tracks have beat markers. Actually checking for beat markers in score tracks
		// requires loading all the appropriate wave data so it isn't trivial to query it when a track isn't actually playing.
		return true;
	}
	else
	{
		char textIdObjName[16];
		formatf(textIdObjName, "RTB_%08X", audRadioTrack::GetBeatSoundName(soundHash));
		const RadioTrackTextIds *beatMarkers = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);

		if(beatMarkers && beatMarkers->numTextIds > 0)
		{
			return true;
		}
	}

	return false;
}

bool audRadioAudioEntity::HasBeatMarkersInRange(u32 soundHash, u32 startMS, u32 endMS)
{
	if(HasBeatMarkers(soundHash))
	{
		if(audRadioTrack::IsScoreTrack(soundHash))
		{
			return g_InteractiveMusicManager.GetNextReplayBeat(soundHash, ((f32)startMS) - 1.f) < endMS;
		}
		else
		{
			char textIdObjName[16];
			formatf(textIdObjName, "RTB_%08X", audRadioTrack::GetBeatSoundName(soundHash));
			const RadioTrackTextIds *beatMarkers = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);

			if(beatMarkers)
			{						
				// search for the two bar markers around the current play head
				for(u32 i=0; i< beatMarkers->numTextIds; i++)
				{
					u32 beatMarkerPlayTimeMS = beatMarkers->TextId[i].OffsetMs;		

					if(beatMarkerPlayTimeMS >= startMS && beatMarkerPlayTimeMS < endMS)
					{
						return true;
					}
				}
			}
		}		
	}

	return false;
}

f32 audRadioAudioEntity::GetReplayBeat(u32 soundHash, s32 direction, f32 startOffsetMS)
{
	f32 beatTimeMS = startOffsetMS;
	char textIdObjName[16];
	formatf(textIdObjName, "RTB_%08X", audRadioTrack::GetBeatSoundName(soundHash));

	const RadioTrackTextIds *beatMarkers = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);

	if(beatMarkers)
	{
		// Can't search backwards past the initial beat marker
		if(direction > 0 || (u32)startOffsetMS > beatMarkers->TextId[0].OffsetMs)
		{
			const f32 currentPlaytimeInSeconds = startOffsetMS * 0.001f;	

			u32 nearestEarlierMarkerTimeMs = 0;
			u32 nearestLaterMarkerTimeMs = ~0U;
			u32 numBeats = 0;
			f32 firstBeatMS = 0;
			u32 nearestBeatMarkerIndex = 0;

			// search for the two bar markers around the current play head
			for(u32 i=0; i< beatMarkers->numTextIds; i++)
			{
				u32 beatMarkerPlayTimeMS = beatMarkers->TextId[i].OffsetMs;		

				if(beatMarkerPlayTimeMS >= nearestEarlierMarkerTimeMs && beatMarkerPlayTimeMS <= startOffsetMS)
				{
					nearestEarlierMarkerTimeMs = beatMarkerPlayTimeMS;
					numBeats = beatMarkers->TextId[i].TextId;
					nearestBeatMarkerIndex = i;
				}
				else if(beatMarkerPlayTimeMS > startOffsetMS && beatMarkerPlayTimeMS < nearestLaterMarkerTimeMs)
				{
					nearestLaterMarkerTimeMs = beatMarkerPlayTimeMS;
				}	
			}

			if(nearestEarlierMarkerTimeMs != nearestLaterMarkerTimeMs)
			{
				const f32 nearestBarTimeInSeconds = nearestEarlierMarkerTimeMs*0.001f;
				const f32 timeSinceBar = currentPlaytimeInSeconds - nearestBarTimeInSeconds;

				if(numBeats == 0)
				{		
					// If we're somewhere between the start of the track and the first beat, clamp to the first beat
					if(nearestEarlierMarkerTimeMs == 0)
					{
						firstBeatMS = (f32)nearestLaterMarkerTimeMs;
					}

					numBeats = 8;
				}			

				const f32 beatDuration = (nearestLaterMarkerTimeMs - nearestEarlierMarkerTimeMs) / (numBeats * 1000.f);
				f32 beatsPastLastBar = timeSinceBar / beatDuration;
				u32 beatsPastLastBarRounded = (u32)(beatsPastLastBar + 0.5f);
				bool isOnBeatBoundary = false;

				// The supplied time is in milliseconds so our timeline position calculation loses some accuracy. 
				// We need to round up/down to the nearest beat as appropriate if we're within the error range.
				if(Abs((s32)Floorf((timeSinceBar * 1000.f) + 0.5f) - (s32)Floorf((beatsPastLastBarRounded * beatDuration * 1000.f) + 0.5f)) <= 1)
				{				
					beatsPastLastBar = (f32)beatsPastLastBarRounded;
					isOnBeatBoundary = true;
				}

				s32 currentBeat = (s32)Floorf(beatsPastLastBar);

				if(direction > 0)
				{
					currentBeat++;
				}
				else if(direction < 0)
				{
					// If searching backwards and we're not on a beat boundary, just floor to the nearest beat, 
					// otherwise subtract a beat.
					if(isOnBeatBoundary)
					{
						currentBeat = beatsPastLastBarRounded - 1;
					}
				}

				f32 beatTimeS = nearestBarTimeInSeconds + (beatDuration * currentBeat);
				beatTimeS = Max(firstBeatMS * 0.001f, beatTimeS);
				beatTimeMS = (beatTimeS * 1000.f);	
			}		
		}
	}	

	return beatTimeMS;
}

bool audRadioAudioEntity::IsReplayMusicTrackPrepared()
{
	if(m_PreviouslyRequestedReplayTrack != g_NullSoundHash)
	{
		if(audRadioTrack::IsScoreTrack(m_PreviouslyRequestedReplayTrack))
		{
			return g_InteractiveMusicManager.IsMusicPrepared();
		}
		else
		{
			if(m_ShouldPrepareReplayMusic)
			{
				return false;
			}
			else if(m_ReplayMusicSound && m_ReplayMusicSound->GetPlayState() != AUD_SOUND_PLAYING)
			{
				return m_ReplayMusicSound->Prepare(g_InteractiveMusicManager.GetMusicWaveSlot(), true) == AUD_PREPARED;
			}
		}
	}

	return true;
}

void audRadioAudioEntity::RequestReplayMusicTrack(u32 soundHash, s32 musicTrackID, u32 startOffsetMS, u32 durationMs, float fadeOutScalar)
{
	if(m_PreviouslyRequestedReplayTrack != soundHash || m_PreviouslyRequestedReplayTrackID != musicTrackID)
	{
		StopReplayMusic();
	}

	if(soundHash != g_NullSoundHash)
	{
		const bool isScoreTrack = audRadioTrack::IsScoreTrack(soundHash);
		if(isScoreTrack)
		{
			g_InteractiveMusicManager.RequestReplayMusic(soundHash, startOffsetMS, durationMs, fadeOutScalar);
		}
		else
		{
			
			if(!m_ReplayMusicSound)
			{
				u32 duration = CVideoProjectAudioTrackProvider::GetMusicTrackDurationMs(soundHash);
				if(startOffsetMS < duration - 1000) // we won't allow less than 1 second of music
				{
					// Replay music is requested from the game-thread, so need to defer the creation of the sound until the audio update
					m_ShouldPrepareReplayMusic = true;		
					m_ShouldPlayReplayMusic = true;
					m_ReplayMusicStartOffset = startOffsetMS;
				}
			}
		}	
	}	

	m_ReplayMusicFadeOutScalar = fadeOutScalar;
	m_PreviouslyRequestedReplayTrack = soundHash;
	m_PreviouslyRequestedReplayTrackID = musicTrackID;
}

void audRadioAudioEntity::StopReplayMusic(bool isPreview, bool noRelease)
{
	const bool isScoreTrack = m_PreviouslyRequestedReplayTrack != g_NullSoundHash && audRadioTrack::IsScoreTrack(m_PreviouslyRequestedReplayTrack);
	if(isScoreTrack)
	{
		g_InteractiveMusicManager.StopAndReset(isPreview && m_ShouldFadeReplayPreview ? REPLAY_PREVIEW_FADE_LENGTH : -1);
	}

	m_ShouldStopReplayMusic = true;
	m_StopReplayMusicPreview = isPreview;
	m_StopReplayMusicNoRelease = noRelease;
	m_ShouldPrepareReplayMusic = false;
	m_ShouldPlayReplayMusic = false;
	m_ShouldPlayReplayPreview = false;
	m_PreviouslyRequestedReplayTrack = g_NullSoundHash;
	m_PreviouslyRequestedReplayTrackID = -1;
	m_HasParsedMarkers = false;
	m_ShouldUpdateReplayPreview = false;
	m_ShouldUpdateReplayMusic = false;
	m_ShouldFadeReplayPreview = false;
	m_ReplayMusicFadeOutScalar = 1.f;
}

void audRadioAudioEntity::UpdateReplayMusic()
{
	if(m_ReplayMusicSound)
	{
		if(m_ReplayMusicSound->GetPlayState() == AUD_SOUND_PLAYING && m_ReplayMusicSound->GetSoundTypeID() == StreamingSound::TYPE_ID)
		{
			const audStreamingSound *sound = reinterpret_cast<const audStreamingSound *>(m_ReplayMusicSound);
			for(s32 i = 0; i < sound->GetWavePlayerIdCount(); i++)
			{
				const s32 wavePlayer = sound->GetWavePlayerId(i);
				if(wavePlayer != -1)
				{
					audPcmSourceInterface::SetParam(wavePlayer, audPcmSource::Params::PauseGroup, 0U);
				}
			}			
		}

		UpdateReplayMusicVolume();		
	}
	else
	{
		g_InteractiveMusicManager.UpdateReplayMusic();
	}
}

void audRadioAudioEntity::UpdateReplayMusicVolume(bool instant)
{
	const f32 interpRate = 10.0f;
	static const f32 amountToDuckMusicForReplaySFX = audDriverUtil::ComputeLinearVolumeFromDb(g_AmountToDuckMusicForReplaySFX);

	f32 desiredVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(g_FrontendAudioEntity.GetMontageMusicVolume() + g_FrontendAudioEntity.GetClipMusicVolume()) * m_ReplayMusicFadeOutScalar;
	f32 currentVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(m_ReplayMusicSound->GetRequestedPostSubmixVolumeAttenuation());

	// adjust for user placed SFX
	if(g_ReplayAudioEntity.ShouldDuckMusic())
	{
		if(instant)
		{
			m_SFXMusicDuckEndTime = 0;
			m_SFXFullMusicDuckVolume = g_AmountToDuckMusicForReplaySFX;
			m_WasDuckingMusicForSFX = true;
		}
		if(!m_WasDuckingMusicForSFX)
		{
			m_WasDuckingMusicForSFX = true;
			m_SFXMusicDuckEndTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + 1000;
		}

		f32 fade = 1.0f;
		u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		if(m_SFXMusicDuckEndTime > currentTime)
		{
			fade = Clamp(1.0f - ((f32)(m_SFXMusicDuckEndTime - currentTime)) / 1000.0f, 0.0f, 1.0f);
			//naDisplayf("Fade %f, %u", fade, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		}
		f32 volumeDb = audDriverUtil::ComputeDbVolumeFromLinear(desiredVolumeLinear);
		f32 sfxVolume = g_ReplayAudioEntity.GetSFXVolumeAffectingMusic();

		if(volumeDb > g_AmountToDuckMusicForReplaySFX + sfxVolume)
		{
			f32 workingVolume = 1.0f - (fade *(1.0f - amountToDuckMusicForReplaySFX));
			workingVolume = audDriverUtil::ComputeDbVolumeFromLinear(workingVolume);
			m_SFXFullMusicDuckVolume = workingVolume + sfxVolume;
		}
		else
		{
			f32 workingVolume = 1.0f - (fade *(1.0f - amountToDuckMusicForReplaySFX));
			workingVolume = audDriverUtil::ComputeDbVolumeFromLinear(workingVolume);
			m_SFXFullMusicDuckVolume = workingVolume;
		}

		if(volumeDb > m_SFXFullMusicDuckVolume)
		{
			desiredVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(m_SFXFullMusicDuckVolume);
		}

		//naDisplayf("Desired Music Volume FadeOut %f %f", desiredVolumeLinear, audDriverUtil::ComputeDbVolumeFromLinear(desiredVolumeLinear));
	}
	else
	{
		if(instant)
		{
			m_SFXMusicDuckEndTime = 0;
			m_SFXFullMusicDuckVolume = g_AmountToDuckMusicForReplaySFX;
			m_WasDuckingMusicForSFX = false;
		}
		if(m_WasDuckingMusicForSFX && !instant)
		{
			m_WasDuckingMusicForSFX = false;
			m_SFXMusicDuckEndTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + 1000;
		}

		f32 fade = 0.0f;
		u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		if(m_SFXMusicDuckEndTime > currentTime)
		{
			fade = Clamp((f32)(m_SFXMusicDuckEndTime - currentTime) / 1000.0f, 0.0f, 1.0f);
			//naDisplayf("Fade %f, %u", fade, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		}
		f32 volumeDb = audDriverUtil::ComputeDbVolumeFromLinear(desiredVolumeLinear);

		f32 workingVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_SFXFullMusicDuckVolume - volumeDb);  // this is the difference between the full duck and where we want to be, we need to fade that range
		workingVolume = 1.0f - (fade *(1.0f - workingVolume));
		workingVolume = audDriverUtil::ComputeDbVolumeFromLinear(workingVolume);

		desiredVolumeLinear = audDriverUtil::ComputeLinearVolumeFromDb(workingVolume + volumeDb); // add the fade component to the desired component

		//naDisplayf("Desired Music Volume FadeIn %f %f", desiredVolumeLinear, audDriverUtil::ComputeDbVolumeFromLinear(desiredVolumeLinear));
	}

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
	m_ReplayMusicSound->SetRequestedPostSubmixVolumeAttenuation(audDriverUtil::ComputeDbVolumeFromLinear(currentVolumeLinear));	
}

void audRadioAudioEntity::StartReplayTrackPreview(u32 soundHash, u32 startOffSetMS, u32 trackDuration, bool jumpToRockout, bool fadeIn)
{
	if(m_ReplayMusicStartTime > 0)
	{
		StopReplayMusic(true);
		g_ReplayAudioEntity.StopAllAmbient(true);
	}
	m_HasParsedMarkers = !jumpToRockout;
	m_ReplayMusicStartTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
	m_ReplayMusicStartOffset = startOffSetMS;
	m_RockoutStartOffset = 0;
	m_RockoutEndOffset = 0;
	m_PreviewReplayTrackTime = trackDuration;
	m_PreviouslyRequestedReplayTrack = soundHash;
	m_ShouldPlayReplayPreview = true;
	m_ShouldFadeReplayPreview = fadeIn;
	m_ReplayMusicFadeOutScalar = 1.f;
}

u32 audRadioAudioEntity::GetReplayMusicPlaybackTimeMs()
{
	// Query how far through the total track the playback is
	if(m_ReplayMusicStartTime != 0 && m_PreviouslyRequestedReplayTrack != g_NullSoundHash)
	{
		u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
		return (m_ReplayMusicStartTime + m_ReplayMusicStartOffset) - currentTime;
	}

	return 0u;	
}

void audRadioAudioEntity::UpdateReplayTrackPreview()
{
	// we will only process the preview track if this is non 0, this is because we are using some of the same
	// member vars that the main replay audio track uses and this update happens every frame
	if(m_ReplayMusicStartTime != 0 && m_PreviouslyRequestedReplayTrack != g_NullSoundHash)
	{
		u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
		u32 soundHash = m_PreviouslyRequestedReplayTrack;
		const bool isScoreTrack = audRadioTrack::IsScoreTrack(soundHash);
		const bool isAmbientTrack = audRadioTrack::IsAmbient(soundHash);
		if(isAmbientTrack)
		{
			g_ReplayAudioEntity.Preload(0, soundHash, REPLAY_STREAM_AMBIENT, 0);
			if(g_ReplayAudioEntity.IsAmbientReadyToPlay(0, soundHash))
			{
#if __BANK
				Displayf("Preview ambinet track %d index %d", soundHash, 0);
#endif
				g_ReplayAudioEntity.PlayAmbient(0, soundHash, true);
			}
			if(g_ReplayAudioEntity.IsAmbientPlaying(0, soundHash))
			{
				if(currentTime > m_ReplayMusicStartTime + m_PreviewReplayTrackTime)
				{
					g_ReplayAudioEntity.StopAllAmbient(false);
					m_ReplayMusicStartTime = 0;
				}
			}
			else
			{
				m_ReplayMusicStartTime = currentTime;
			}
		}
		else if(isScoreTrack)
		{
			g_InteractiveMusicManager.SetReplayScoreIntensity(DEFAULT_MARKER_AUDIO_INTENSITY, DEFAULT_MARKER_AUDIO_INTENSITY, true, 0);
			g_InteractiveMusicManager.RequestReplayMusic(soundHash, m_ReplayMusicStartOffset);
			if(!g_InteractiveMusicManager.IsMusicPlaying())
			{
				m_ReplayMusicStartTime = currentTime;
			}
			if(currentTime > m_ReplayMusicStartTime + m_PreviewReplayTrackTime)
			{
				StopReplayMusic(true);
				m_ReplayMusicStartTime = 0;
			}
			g_InteractiveMusicManager.UpdateReplayMusicPreview(m_ShouldFadeReplayPreview);
		}
		else
		{
			// first we preload the wave and parse the markers
			if(!m_HasParsedMarkers && m_ShouldPlayReplayPreview)
			{
				if(m_ReplayMusicSound)
				{
					if(m_ReplayMusicSound->GetPlayState() != AUD_SOUND_PLAYING)
					{
						m_ReplayMusicStartTime = currentTime;
						if(m_ReplayMusicSound->Prepare(g_InteractiveMusicManager.GetMusicWaveSlot(), true) == AUD_PREPARED)
						{
							ParseMarkers();
							m_ReplayMusicStartOffset = m_RockoutStartOffset;
							if(((m_RockoutEndOffset - m_RockoutStartOffset) > REPLAY_PREVIEW_MIN_LENGTH) && ((m_RockoutEndOffset - m_RockoutStartOffset) < REPLAY_PREVIEW_MAX_LENGTH))
							{
								//m_PreviewReplayTrackTime = m_RockoutEndOffset - m_RockoutStartOffset;
								m_PreviewReplayTrackTime = REPLAY_PREVIEW_DEFAULT_LENGTH; // force to 30s preview
								Displayf("Found ROCKOUT section time : %dms", m_PreviewReplayTrackTime);
								u32 duration = CVideoProjectAudioTrackProvider::GetMusicTrackDurationMs(m_PreviouslyRequestedReplayTrack);
								if(m_ReplayMusicStartOffset + m_PreviewReplayTrackTime > duration - 1000)
								{
									m_PreviewReplayTrackTime = (duration - REPLAY_PREVIEW_FADE_LENGTH) - m_ReplayMusicStartOffset;
									if(m_PreviewReplayTrackTime < REPLAY_PREVIEW_MIN_LENGTH)
									{
										m_ReplayMusicStartOffset = 0;
										m_PreviewReplayTrackTime = REPLAY_PREVIEW_DEFAULT_LENGTH;
									}
								}
							}
							else
							{
								m_PreviewReplayTrackTime = REPLAY_PREVIEW_DEFAULT_LENGTH;
							}
							// kill the sound and preload to the new start offset based on ROCKOUT marker
							m_ReplayMusicSound->StopAndForget();
							m_ShouldPrepareReplayMusic = true;
						}
					}
				}
				else
				{
					m_ShouldPrepareReplayMusic = true;
				}
			} 
			else if(m_ReplayMusicSound)
			{
				if(m_ReplayMusicSound->GetPlayState() != AUD_SOUND_PLAYING && m_ShouldPlayReplayPreview)
				{
					m_ReplayMusicStartTime = currentTime;
					if(m_ReplayMusicSound->Prepare(g_InteractiveMusicManager.GetMusicWaveSlot(), true) == AUD_PREPARED)
					{
						m_ReplayMusicSound->SetRequestedPostSubmixVolumeAttenuation(-100.0f);
						m_ReplayMusicSound->GetRequestedSettings()->SetShouldPersistOverClears(true);
						m_ReplayMusicSound->Play();			
						m_ShouldPlayReplayPreview = false;

						if(audRadioTrack::IsCommercial(m_PreviouslyRequestedReplayTrack))
						{
							m_PreviewReplayTrackTime = m_ReplayMusicSound->ComputeDurationMsIncludingStartOffsetAndPredelay(NULL);
							m_ShouldFadeReplayPreview = false;
						}
					}
				}
				if(m_ReplayMusicSound->GetPlayState() == AUD_SOUND_PLAYING && m_ReplayMusicSound->GetSoundTypeID() == StreamingSound::TYPE_ID)
				{
					f32 currentFade = m_ShouldFadeReplayPreview? (float)(currentTime - m_ReplayMusicStartTime) / (f32)REPLAY_PREVIEW_FADE_LENGTH : 1.0f;
					f32 currentVolumeLinear = Clamp(currentFade, 0.0f, 1.0f);
					m_ReplayMusicSound->SetRequestedPostSubmixVolumeAttenuation(audDriverUtil::ComputeDbVolumeFromLinear(currentVolumeLinear*REPLAY_PREVIEW_MUSIC_VOLUME));

					const audStreamingSound *sound = reinterpret_cast<const audStreamingSound *>(m_ReplayMusicSound);
					for(s32 i = 0; i < sound->GetWavePlayerIdCount(); i++)
					{
						const s32 wavePlayer = sound->GetWavePlayerId(i);
						if(wavePlayer != -1)
						{
							audPcmSourceInterface::SetParam(wavePlayer, audPcmSource::Params::PauseGroup, 0U);
						}
					}			
				}
				if(currentTime > m_ReplayMusicStartTime + m_PreviewReplayTrackTime)
				{
					StopReplayMusic(true);
					m_ReplayMusicStartTime = 0;
				}
			}
			else
			{
				m_ShouldPrepareReplayMusic = true;
			}
		}
	}
}

void audRadioAudioEntity::ParseMarkers()
{
	if(audVerifyf(g_InteractiveMusicManager.GetMusicWaveSlot(), "Radio track preview playing physically with no wave slot"))
	{
		// find the wavename hash
		const u32 metadataSoundHash = m_PreviouslyRequestedReplayTrack;
		u32 wavehash[2] = {~0U,~0U};

		const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(metadataSoundHash);
		if(streamingSound)
		{
			for(u32 waveIndex = 0; waveIndex < Min<u32>(2, streamingSound->numSoundRefs); waveIndex++)
			{
				const SimpleSound *simpleSound = SOUNDFACTORY.DecompressMetadata<SimpleSound>(streamingSound->SoundRef[waveIndex].SoundId);
				if(simpleSound)
				{								
					wavehash[waveIndex] = simpleSound->WaveRef.WaveName;
				}
			}
		}

		if(wavehash[0] != ~0U || wavehash[1] != ~0U)
		{
			audWaveRef waveRef;
			audWaveMarkerList markers;

			u32 sampleRate = 0;
			if(waveRef.Init(g_InteractiveMusicManager.GetMusicWaveSlot(), wavehash[0]))
			{
				waveRef.FindMarkerData(markers);
				const audWaveFormat *format = waveRef.FindFormat();
				if(format)
				{
					sampleRate = format->SampleRate;
				}
			}
			if(markers.GetCount() == 0)
			{
				if(waveRef.Init(g_InteractiveMusicManager.GetMusicWaveSlot(), wavehash[1]))
				{
					waveRef.FindMarkerData(markers);
					const audWaveFormat *format = waveRef.FindFormat();
					if(format)
					{
						sampleRate = format->SampleRate;
					}
				}
			}

			audRadioTrack::audDjSpeechRegion currentRegion;
			currentRegion.type = audRadioTrack::audDjSpeechRegion::OUTRO;

			audRadioTrack::audDjSpeechRegion currentRockOutRegion;
			currentRockOutRegion.type = audRadioTrack::audDjSpeechRegion::ROCKOUT;

			m_RockoutStartOffset = 0;
			m_RockoutEndOffset = 0;
			u32 startRockOut = 0;
			u32 endRockOut = 0;
			for(u32 i=0; i<markers.GetCount(); i++)
			{
				if(markers[i].categoryNameHash == ATSTRINGHASH("ROCKOUT", 0xF31B4F6A))
				{
					if(markers[i].nameHash == ATSTRINGHASH("START", 0x84DC271F))
					{
						startRockOut = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);

						// always grab first ROCKOUT section and use it unless we get a better one in our magic length
						if(m_RockoutStartOffset == 0)
						{
							m_RockoutStartOffset = startRockOut;
						}
					}
					else if(markers[i].nameHash == ATSTRINGHASH("END", 0xB0C485D7))
					{
						endRockOut = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						u32 rockLength = endRockOut - startRockOut;
						if(rockLength > REPLAY_PREVIEW_MIN_LENGTH && rockLength < REPLAY_PREVIEW_MAX_LENGTH )
						{
							if(endRockOut > startRockOut)
							{
								m_RockoutStartOffset = startRockOut;
								m_RockoutEndOffset = endRockOut;
								m_HasParsedMarkers = true;
								return;
							}
						}
						// always grab first ROCKOUT section and use it unless we get a better one in our magic length
						if(m_RockoutEndOffset == 0 && endRockOut > m_RockoutStartOffset)
						{
							m_RockoutEndOffset = endRockOut;
						}
					}
				}
			}
		}
	}
	m_HasParsedMarkers = true;
}

void audRadioAudioEntity::StopReplayTrackPreview()
{
	StopReplayMusic(true);
	g_ReplayAudioEntity.StopAllAmbient(true);
	m_ReplayMusicStartTime = 0;
}


#endif // GTA_REPLAY

void audRadioAudioEntity::FrontendUpdate(const bool previewRadio)
{
	// only go into frontend mode when the game is paused
	if(CPauseMenu::IsActive() && ((previewRadio && CNetwork::IsGameInProgress() && !g_InteractiveMusicManager.IsMusicPlaying()) || fwTimer::IsGamePaused()))
	{
		if(!m_InFrontendMode)
		{
			m_WasMobileRadioActiveOnEnteringFrontend = sm_IsMobilePhoneRadioActive;
			m_InFrontendMode = true;
		}
		sm_IsMobilePhoneRadioActive  = true;
	}
	else
	{
		if(m_InFrontendMode)
		{
			sm_IsMobilePhoneRadioActive = m_WasMobileRadioActiveOnEnteringFrontend;
						
#if HEIST3_HIDDEN_RADIO_ENABLED
			// If the user tunes away from the special radio in the pause menu, set it back when we exit
			if (m_PlayerVehicleRadioState != PLAYER_RADIO_OFF && m_PlayerVehicle && m_PlayerRadioStation && m_PlayerVehicle->GetModelNameHash() == HEIST3_HIDDEN_RADIO_VEHICLE && m_PlayerRadioStation->GetNameHash() != HEIST3_HIDDEN_RADIO_STATION_NAME)
			{
				if (GetEncryptionKeyForBank(audRadioStation::sm_Heist3HiddenRadioBankId))
				{
					RetuneToStation(HEIST3_HIDDEN_RADIO_STATION_NAME);
				}
			}
#endif
		}
		m_InFrontendMode = false;
	}
	
	// Allow frontend to override pausing
	// Don't pause when in the replay editor, to ensure radio slots are cleaned up
	const bool frontendPreview = previewRadio && m_InFrontendMode;
	if(!frontendPreview && fwTimer::IsGamePaused() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive() && !CReplayMgr::IsPlaying()))
	{
		Pause();
	}
	else
	{
		Unpause();
	}
}

void audRadioAudioEntity::Shutdown(void)
{
	audRadioStation::Reset();
	audEntity::Shutdown();
}

void audRadioAudioEntity::SetFrontendFadeTime(const float fadeTimeSeconds)
{
	m_FrontendFadeTime = fadeTimeSeconds;
	m_FrontendFadeTimeResetTimer = 5.f;
}

void audRadioAudioEntity::GameUpdate()
{
    audRadioStation::GameUpdate();

	if(m_FrontendFadeTimeResetTimer > 0.f)
	{
		m_FrontendFadeTimeResetTimer -= fwTimer::GetSystemTimeStep();
		if(m_FrontendFadeTimeResetTimer <= 0.f)
		{
			m_FrontendFadeTimeResetTimer = -1.f;
			m_FrontendFadeTime = g_RadioPositionedToStereoTime;
		}
	}
	else
	{
		BANK_ONLY(m_FrontendFadeTime = g_RadioPositionedToStereoTime);
	}
	const float smootherRate = 0.001f / m_FrontendFadeTime;
	m_PositionedToStereoVolumeSmoother.SetRates(smootherRate, smootherRate);

	//Update our details about the player vehicle.
	CPed *player = CGameWorld::FindLocalPlayer();
	CVehicle * playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnSCTVTeam())
	{
		CPed *followPlayer = CGameWorld::FindFollowPlayer();

		if (followPlayer && followPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			playerVehicle = followPlayer->GetMyVehicle();
			player = followPlayer;
		}
	}

	if(m_PlayerVehicle != playerVehicle)
	{
		m_PlayerVehicle = playerVehicle;
	}
	if(player->GetModelIndex() != m_LastPlayerModelId)
	{
		m_LastPlayerModelId = player->GetModelIndex();

		// Player has switched - reset autotuning timer
		m_LastActiveRadioTimeMs = 0;
	}


	// Player is getting out if TaskExitVehicle has reached a certain state
	const bool hasPlayerExitedVeh =	(player->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE)
								&& player->GetAttachState() == ATTACH_STATE_PED_EXIT_CAR);

	//NOTE: Make sure the car has been started and allocated a radio station (off in the case of the player vehicle.)
	//Also ensure that the player is *really* in the vehicle and not getting in or out.
	bool isPlayerInVehicle = (player && m_PlayerVehicle &&
		!player->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
		!hasPlayerExitedVeh);

	if(player && m_PlayerVehicle)
	{
		if(m_PlayerVehicle->GetVehicleModelInfo() && m_PlayerVehicle->GetVehicleModelInfo()->m_data != NULL)
		{
			// Don't activate frontend radio if we're just hanging off the side of a vehicle (firetrucks etc.) or are sat in a trailer outside the cabin
			if(m_PlayerVehicle->GetVehicleAudioEntity() && m_PlayerVehicle->GetVehicleAudioEntity()->IsPedSatInExternalSeat(player))
			{
				isPlayerInVehicle = false;
			}
		}
	}	

	// IsRadioEnabled will return false until the engine is on
	bool isPlayerInAVehicleWithARadio = isPlayerInVehicle && m_PlayerVehicle->GetVehicleAudioEntity()->GetHasNormalRadio();
	bool isPlayerInVehicleForRadio = (isPlayerInVehicle && m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioEnabled() && (m_PlayerVehicle->GetVehicleAudioEntity()->GetRadioStation() != NULL || (m_IsPlayerInAVehicleWithARadio && (m_PlayerVehicleRadioState != PLAYER_RADIO_OFF || m_ForcePlayerStation != NULL))));
		
	if(NetworkInterface::IsGameInProgress() && camInterface::IsFadedOut())
	{
		// Do nothing - script often fiddle with the vehicles etc. during respawn so hold off updating until we are fading back into the game
	}
	else
	{
		// IsRadioEnabled will return false until the engine is on
		m_IsPlayerInAVehicleWithARadio = isPlayerInAVehicleWithARadio;
		m_IsPlayerInVehicleForRadio = isPlayerInVehicleForRadio;
	}	

#if GTA_REPLAY
	// No radio in replays
	if(CReplayMgr::IsEditModeActive())
	{
		m_IsPlayerInAVehicleWithARadio = false;
	}
#endif

#if !__NO_OUTPUT
	if(m_IsPlayerInAVehicleWithARadio && m_PlayerVehicle && m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioDisabledByScript())
	{
		static f32 s_NextWarnTime = 0.f;
		if(s_NextWarnTime <= 0.f)
		{
			s_NextWarnTime = 1.f;
			audWarningf("Player is in a vehicle with radio disabled by script");
		}
		else
		{
			s_NextWarnTime -= fwTimer::GetTimeStep();
		}
	}
#endif

	if(m_IsPlayerInVehicleForRadio)
	{
		// Deal with warping between vehicles (including character switch); force the station in the new vehicle to
		// match the old one.
		if(m_LastPlayerVehicle != NULL && m_PlayerVehicle && m_LastPlayerVehicle != m_PlayerVehicle && m_PlayerRadioStation)
		{			
			// Ensure we go via a STARTING state if we're not retuning
			if(m_PlayerVehicle->GetVehicleAudioEntity()->GetRadioStation() == m_PlayerRadioStation 
				|| g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ForceSeamlessRadioSwitch))
			{
				audDisplayf("Player has switched vehicle, updating radio station");
				m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioStation(m_PlayerRadioStation);
				if(m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING)
				{
					// Need to enter the 'starting' state to register this vehicle as a new emitter
					SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
					m_TimeEnteringStartingState = GetCurrentTimeMs();
				}
			}
		}
		m_LastPlayerVehicle = m_PlayerVehicle;
	}

	FrontendUpdate(CPauseMenu::ShouldPlayMusic() || CPauseMenu::ShouldPlayRadioStation());
}


#if RSG_BANK
void audRadioAudioEntity::DebugDrawStations()
{
	const f32 yStepRate = 0.015f;
	f32 xCoord = 0.05f;
	f32 yCoord = 0.05f - g_StationDebugDrawYScroll;

	if (g_DrawAudibleTrack)
	{		
		const audRadioStation* radioStation = FindAudibleStation(25.f);

		if (radioStation)
		{
			if (radioStation->IsMixStation())
			{
				radioStation->DebugDrawMixBeatMarkers();
			}
		}
	}

	if (g_DrawTakeoverStation1)
	{
		if (audRadioStation* station = audRadioStation::FindStation("RADIO_23_DLC_XM19_RADIO"))
		{
			station->DrawTakeoverStation();
		}
	}	

	if (g_DebugDrawUSBStation)
	{
		audRadioStation* station = audRadioStation::GetUSBMixStation();

		if (station)
		{
			station->DrawUSBStation();
		}
	}

	if(g_DrawRadioStationDebug)
	{
		u32 numStations = audRadioStation::GetNumTotalStations();
		for(u32 radioStationIndex=0; radioStationIndex<numStations; radioStationIndex++)
		{
			const audRadioStation *station = audRadioStation::GetStation(radioStationIndex);
			if(station)
			{
				if(g_StationDebugNameFilter[0] != 0 && !audEngineUtil::MatchName(station->GetName(), g_StationDebugNameFilter))
				{
					continue;
				}

				char tempString[256];	
				formatf(tempString, "Station %s", station->GetName());
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += yStepRate;

				formatf(tempString, "Random Seed: %u", station->GetRandomSeed());
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += yStepRate;

				xCoord += 0.02f;
				const audRadioTrack& currentTrack = station->GetCurrentTrack();

				if(currentTrack.IsInitialised())
				{
					formatf(tempString, "Current Track: %s (%s track %u)", SOUNDFACTORY.GetMetadataManager().GetObjectName(currentTrack.GetSoundRef()), TrackCats_ToString((TrackCats)currentTrack.GetCategory()), currentTrack.GetTrackIndex());
					grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
					yCoord += yStepRate;

					formatf(tempString, "Progress: %u/%u (%.02f%%)", currentTrack.GetPlayTime(), currentTrack.GetDuration(), (currentTrack.GetPlayTime()/(f32)currentTrack.GetDuration()) * 100.f);
					grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
					yCoord += yStepRate;
				}
				else
				{
					formatf(tempString, "Current Track: Not Initialised");
					grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
					yCoord += yStepRate;					
				}

				const audRadioTrack& nextTrack = station->GetNextTrack();

				if(nextTrack.IsInitialised())
				{
					formatf(tempString, "Next Track: %s (%s track %u)", SOUNDFACTORY.GetMetadataManager().GetObjectName(nextTrack.GetSoundRef()), TrackCats_ToString((TrackCats)nextTrack.GetCategory()), nextTrack.GetTrackIndex());
					grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
					yCoord += yStepRate;
				}
				else
				{
					formatf(tempString, "Next Track: Not Initialised");
					grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
					yCoord += yStepRate;					
				}

				xCoord += 0.02f;
				for(u32 category = 0; category < TrackCats::NUM_RADIO_TRACK_CATS; category++)
				{
					if(audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(station->FindTrackListsForCategory(category)))
					{
						u32 unlockedTrackListIndex = 0;

						for(u32 listIndex = 0; listIndex < trackListCollection->GetListCount(); listIndex++)
						{
							if(const RadioStationTrackList* trackList = trackListCollection->GetList(listIndex))
							{
								const char* trackListName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset);
								bool isLocked = trackListCollection->IsListLocked(listIndex);

								if(!isLocked && station->m_UseRandomizedStrideSelection && audRadioStation::IsRandomizedStrideCategory(category))
								{
									// Special case - for stations with old/new data, we store the new data stride in the takeover slot
									if(!station->m_HasTakeoverContent && trackListCollection->GetUnlockedListCount() == 2)
									{										
										if(unlockedTrackListIndex == 1)
										{
											u32 adjustedCategory = category;
											if(category == RADIO_TRACK_CAT_MUSIC) { adjustedCategory = RADIO_TRACK_CAT_TAKEOVER_MUSIC; }
											if(category == RADIO_TRACK_CAT_IDENTS) { adjustedCategory = RADIO_TRACK_CAT_TAKEOVER_IDENTS; }
											if(category == RADIO_TRACK_CAT_DJSOLO) { adjustedCategory = RADIO_TRACK_CAT_TAKEOVER_DJSOLO; }
											audRadioStationHistory* history = const_cast<audRadioStationHistory*>(station->FindHistoryForCategory(adjustedCategory));											
											formatf(tempString, "    %s: %s, Stride %d, Prev: %d/%d", trackListName, isLocked ? "Locked" : "Unlocked", station->m_CategoryStride[adjustedCategory], (*history)[0], trackList->numTracks);
										}
										else
										{
											audRadioStationHistory* history = const_cast<audRadioStationHistory*>(station->FindHistoryForCategory(category));
											formatf(tempString, "    %s: %s, Stride %d, Prev %d/%d", trackListName, isLocked ? "Locked" : "Unlocked", station->m_CategoryStride[category], (*history)[0], trackList->numTracks);
										}
									}									
									else
									{
										audRadioStationHistory* history = const_cast<audRadioStationHistory*>(station->FindHistoryForCategory(category));
										formatf(tempString, "    %s: %s, Stride %d, Prev %d/%d", trackListName, isLocked ? "Locked" : "Unlocked", station->m_CategoryStride[category], (*history)[0], trackList->numTracks);
									}
								}
								else
								{
									formatf(tempString, "    %s: %s", trackListName, isLocked ? "Locked" : "Unlocked");
								}

								if(!isLocked)
								{
									unlockedTrackListIndex++;
								}

								grcDebugDraw::Text(Vector2(xCoord, yCoord), isLocked ? Color_LightGray : Color_white, tempString);
								yCoord += yStepRate;
							}
						}
					}
				}
				xCoord -= 0.02f;
				
				xCoord -= 0.02f;
				yCoord += yStepRate;
			}
		}
	}
}

void audRadioAudioEntity::DrawDebug()
{
	if(g_DrawRadioDebug)
	{
		PUSH_DEFAULT_SCREEN();

		audDriver::SetDebugDrawRenderStates();

		audSoundDebugDrawManager drawMgr(100.f, 50.f, 20.f, 720.f);
		drawMgr.SetTextScale(1.5f);
		grcColor(Color32(255,255,255,255));

		drawMgr.DrawLinef("Max positioned levels: L: %f, M: %f, S: %f, V: %f"
			, audDriverUtil::ComputeDbVolumeFromLinear(audRadioTrack::GetLargeReverbMax())
			, audDriverUtil::ComputeDbVolumeFromLinear(audRadioTrack::GetMediumReverbMax())
			, audDriverUtil::ComputeDbVolumeFromLinear(audRadioTrack::GetSmallReverbMax())
			, audRadioTrack::GetVolumeMax()
			);
		
		const char *states[] = {"Off","Starting","Playing", "Frozen", "Stopping","Retuning"};
		drawMgr.DrawLinef("PlayerVehicleRadioState: %s, PlayerMobilePhoneRadioState: %s", states[m_PlayerVehicleRadioState],states[m_MobilePhoneRadioState]);

		const audRadioTrack *audibleTrack = FindAudibleTrack(25.f);
		if(audibleTrack)
		{
			drawMgr.DrawLinef("Currently audible track: %s (ZiT: %u) ", audibleTrack->GetBankName(), audibleTrack->GetTextId());
			drawMgr.DrawLinef("(Play time: %d/%u, %.02f%%, %d remaining)", audibleTrack->GetPlayTime(), audibleTrack->GetDuration(), (audibleTrack->GetPlayTime()/(f32)audibleTrack->GetDuration()) * 100.f, audibleTrack->GetDuration() - audibleTrack->GetPlayTime());
			float bpm = 0.f;
			s32 beatNumber = 1;
			float beatTimeS = 0.;
			if(audibleTrack->GetNextBeat(beatTimeS, bpm, beatNumber))
			{		
				drawMgr.DrawLinef("Audible beat: %.2f (%u/4 %.1f BPM)", beatTimeS, beatNumber, bpm);
			}
			
			if (audRadioStation* radioStation = audRadioStation::FindStation(audibleTrack->StationNameHash()))
			{
				radioStation->DrawDebug(drawMgr);
			}
		}

		audRadioSlot::DrawDebug(drawMgr);
		audStreamSlot::DrawDebug(drawMgr);

		POP_DEFAULT_SCREEN();
			
	}
	else if(g_DrawRadioSlotDebug)
	{
		PUSH_DEFAULT_SCREEN();

		audDriver::SetDebugDrawRenderStates();

		audSoundDebugDrawManager drawMgr(100.f, 50.f, 20.f, 720.f);
		drawMgr.SetTextScale(1.5f);
		grcColor(Color32(255,255,255,255));
		audRadioSlot::DrawDebug(drawMgr);
		audStreamSlot::DrawDebug(drawMgr);
		POP_DEFAULT_SCREEN();
	}

	if(IsPlayerRadioActive() && m_PlayerRadioStation)
	{
		static u32 s_LastPlayerTrack = 0;
		const u32 soundRef = m_PlayerRadioStation->GetCurrentTrack().GetSoundRef();
		if(s_LastPlayerTrack != soundRef && m_PlayerRadioStation->GetCurrentTrack().IsInitialised())
		{
			s_LastPlayerTrack = soundRef;

			audDebugf1("PlayerRadioTrack: %u,%s,%s,%u,%u,%s", GetCurrentTimeMs(), m_PlayerRadioStation->GetName(), g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(soundRef), 
				m_PlayerRadioStation->GetCurrentTrack().GetPlayTime(), 
				m_PlayerRadioStation->GetCurrentTrack().GetDuration(),
				m_PlayerRadioStation->IsCurrentTrackNew() ? "NEW" : "OLD");
		}
	}
}
#endif

void audRadioAudioEntity::UpdateFade()
{
	switch(m_FadeState)
	{
	case kFadingIn:
		m_FadeVolumeLinear += m_InvFadeTime * fwTimer::GetTimeStep();
		m_FadeVolumeLinear = Min(m_FadeVolumeLinear, 1.f);
		if(m_FadeVolumeLinear >= 1.f)
		{
			m_FadeState = kNotFading;
		}
		break;
	case kFadingOut:
		m_FadeVolumeLinear -= m_InvFadeTime * fwTimer::GetTimeStep();
		m_FadeVolumeLinear = Max(m_FadeVolumeLinear, 0.f);
		if(m_FadeVolumeLinear <= 0.f)
		{
			m_FadeState = kNotFading;
		}
		break;
	case kWaitingToFadeIn:
		m_FadeWaitTime -= fwTimer::GetTimeStep();
		if(m_FadeWaitTime <= 0.f)
		{
			m_FadeState = kFadingIn;
		}
		break;
	case kWaitingToFadeOut:
		m_FadeWaitTime -= fwTimer::GetTimeStep();
		if(m_FadeWaitTime <= 0.f)
		{
			m_FadeState = kFadingOut;
		}
		break;
	case kNotFading:
		// Don't reset the fade value here, as we want to hold the fade level
		break;
	}
}

void audRadioAudioEntity::CancelFade()
{
	if(m_FadeVolumeLinear < 1.f)
	{
		m_FadeState = kFadingIn;
		m_InvFadeTime = 2.f;
	}
	else
	{
		m_FadeState = kNotFading;
	}
}

void audRadioAudioEntity::StartFadeIn(const float delayTime, const float fadeTime)
{
	if(m_FadeState != kNotFading)
	{
		audWarningf("Radio was previously fading %s - overriding with new fade in time of %f", m_FadeState == kFadingIn ? "in" : "out", fadeTime);
	}
	if(fadeTime == 0.f)
	{
		m_FadeState = kNotFading;
		m_FadeVolumeLinear = 1.f;
	}
	else
	{
		m_InvFadeTime =  fadeTime > 0.f ? 1.f / fadeTime : 1.f;
		m_FadeWaitTime = delayTime;
		if(delayTime == 0.0f)
		{
			m_FadeState = kFadingIn;
		}
		else
		{
			m_FadeState = kWaitingToFadeIn;
		}

		// Note; it is assumed that the radio will not be audible when the fade is started
		m_FadeVolumeLinear = 0.f;
	}
	
}

void audRadioAudioEntity::StartFadeOut(const float delayTime, const float fadeTime)
{
	if(m_FadeState != kNotFading)
	{
		audWarningf("Radio was previously fading - overriding with new fade in time of %f (delay time %f)", fadeTime, delayTime);
	}
	m_InvFadeTime =  fadeTime > 0.f ? 1.f / fadeTime : 1.f;
	m_FadeWaitTime = delayTime;
	if(delayTime == 0.f)
	{
		m_FadeState = kFadingOut;
	}
	else
	{
		m_FadeState = kWaitingToFadeOut;
	}
}

void audRadioAudioEntity::PreUpdateService(u32)
{
	UpdateFade();

	if(m_AutoUnfreezeForPlayerTimer > 0.f)
	{
		m_AutoUnfreezeForPlayerTimer -= fwTimer::GetTimeStep();
		if(m_AutoUnfreezeForPlayerTimer <= 0.f)
		{
			m_AutoUnfreezeForPlayer = true;
		}
	}

	const u32 timeInMs = GetCurrentTimeMs();

	// always allow frontend radio while in frontend mode
	if(IsInFrontendMode())
	{		
		// Force the positioned smoother to 0
		m_PositionedToStereoVolumeSmoother.Reset();
		if(g_AudioEngine.GetSoundManager().IsPaused(2))
		{			
			// If paused, force positioned for the transition out
			m_PositionedToStereoVolumeSmoother.CalculateValue(0.0f, GetCurrentTimeMs());
			m_PlayerVehicleInsideFactor = 0.f;
		}
		else
		{
			// If not paused, force frontend for the transition out
			m_PositionedToStereoVolumeSmoother.CalculateValue(1.0f, GetCurrentTimeMs());
			m_PlayerVehicleInsideFactor = 1.f;
		}

	}

	// use the special radio timer
	if(!g_AudioEngine.GetSoundManager().IsPaused(2))
	{
		if(!g_AudioEngine.IsAudioEnabled() || (audRadioStation::GetNumTotalStations() == 0))
		{
			return;
		}

		// If we're transitioning back from the pause menu and we're on a vehicle with no ambient radio, we need to bypass the normal delay
		// since the radio will be silent for the transition, rather than muffled like it is on a vehicle with radio.
		const bool quickTransitionBackOnBike = (audNorthAudioEngine::IsTransitioningOutOfPauseMenu() && m_PlayerVehicle && m_PlayerVehicle->GetAudioEntity() && m_PlayerVehicle->GetVehicleAudioEntity()->IsAmbientRadioDisabled());
		const bool isSubmarine = m_PlayerVehicle && (m_PlayerVehicle->InheritsFromSubmarineCar() || m_PlayerVehicle->InheritsFromSubmarine());
		const bool isSpectatingPedOnFoot = m_PlayerVehicle == NULL && NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnSCTVTeam();

		const camBaseCamera* theActiveRenderedCamera = camInterface::GetDominantRenderedCamera();
		const bool isCreatorModeFlyCamActive = theActiveRenderedCamera && (camScriptedFlyCamera::GetStaticClassId() == theActiveRenderedCamera->GetClassId());

		// NOTE: use the radio timer for smoothing so that it doesn't slow down during slow mo sequences
		const bool shouldPlayFrontendRadioInGame = (
			!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::FrontendRadioDisabled) &&
			(sm_IsMobilePhoneRadioActive || (m_IsPlayerInVehicleForRadio && !FindPlayerPed()->IsDead()) || audRadioStation::IsPlayingEndCredits()) &&
			(quickTransitionBackOnBike || audNorthAudioEngine::GetActiveSlowMoMode() == AUD_SLOWMO_RADIOWHEEL || g_AudioEngine.GetEnvironment().GetSpecialEffectMode() == kSpecialEffectModeNormal || ((isSubmarine || isSpectatingPedOnFoot || isCreatorModeFlyCamActive) && g_AudioEngine.GetEnvironment().GetSpecialEffectMode() == kSpecialEffectModeUnderwater)) &&
			(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowRadioDuringSwitch) || !g_PlayerSwitch.ShouldDisableRadio())
			); 
		
#if !__NO_OUTPUT
		if((!shouldPlayFrontendRadioInGame || m_VolumeOffset <= -36.f) && m_IsPlayerInAVehicleWithARadio)
		{
			static u32 s_LastWarnTime = 0;
			const u32 warnTimeNow = GetCurrentTimeMs();
			if(warnTimeNow > s_LastWarnTime + 1000)
			{
				if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::FrontendRadioDisabled))
				{
					audWarningf("Player vehicle frontend radio is disabled due to script flag FrontendRadioDisabled");
				}
				if(g_PlayerSwitch.ShouldDisableRadio())
				{
					audWarningf("Player vehicle frontend radio is disabled due to PlayerSwitch");
				}
				if(audNorthAudioEngine::GetActiveSlowMoMode() != AUD_SLOWMO_RADIOWHEEL && !quickTransitionBackOnBike && g_AudioEngine.GetEnvironment().GetSpecialEffectMode() != kSpecialEffectModeNormal)
				{
					audWarningf("Player vehicle frontend radio is disabled due to special effect flags. SlowMoMode: %s (%d) special effect mode: %d", 
												SlowMoType_ToString(audNorthAudioEngine::GetActiveSlowMoMode()), audNorthAudioEngine::GetActiveSlowMoMode(),
												g_AudioEngine.GetEnvironment().GetSpecialEffectMode());
				}
				if(m_VolumeOffset <= -36.f)
				{
					audWarningf("Player vehicle frontend radio has been faded out via music event");
				}
				s_LastWarnTime = warnTimeNow;
			}
		}
#endif
		// Save streaming bandwidth by turning off ambient / vehicle emitters when player is moving at speed
		// or when score is playing
		if(m_PlayerVehicle && (g_InteractiveMusicManager.IsMusicPlaying() || 
			(!m_PlayerVehicle->InheritsFromBicycle() && m_PlayerVehicle->GetVehicleAudioEntity() && m_PlayerVehicle->GetVehicleAudioEntity()->GetCachedVelocity().Mag2() > (10.f*10.f))))
		{
			m_StopEmittersForPlayerVehicle = true;
		}
		else
		{
			m_StopEmittersForPlayerVehicle = false;
		}
		
		// We can't use the smoother in frontend mode
		
		if(!IsInFrontendMode())
		{
			 m_PlayerVehicleInsideFactor = m_PositionedToStereoVolumeSmoother.CalculateValue(shouldPlayFrontendRadioInGame ? 1.0f : 0.0f, 
				GetCurrentTimeMs());
		}
		
		//Don't update the user controls when we're starting up player radio, as we don't want to mess with that.
		if((m_PlayerVehicleRadioState != PLAYER_RADIO_STARTING) && (m_MobilePhoneRadioState != PLAYER_RADIO_STARTING))
		{
			UpdateUserControl(timeInMs);
		}

		UpdatePlayerVehicleRadio(timeInMs);
		UpdateMobilePhoneRadio(timeInMs);

		const audRadioStation* stationBeingListenedTo = nullptr;
		
		if (m_MobilePhoneRadioState == PLAYER_RADIO_PLAYING || m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING)
		{
			stationBeingListenedTo = m_PlayerRadioStation;
		}

		// We also count the closest audible interior station as being listened to (to pick up apartment radios etc.)
		if (!stationBeingListenedTo && audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
		{
			s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
			CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
			stationBeingListenedTo = FindAudibleStation(pIntInst, roomIdx);
		}

		if (stationBeingListenedTo)
		{
			// Accumulate system timestep to count time spent listening in the pause menu
			const_cast<audRadioStation*>(stationBeingListenedTo)->SetListenTimer(stationBeingListenedTo->GetListenTimer() + fwTimer::GetSystemTimeStep());
		}
		
		ComputeVolumeOffset();

		m_WasPlayerInVehicleForRadioLastFrame = m_IsPlayerInVehicleForRadio;
		m_WasMobilePhoneRadioActiveLastFrame = sm_IsMobilePhoneRadioActive;
	}
	// need to keep updating slots while paused so that FE music etc gets a look in
	audRadioSlot::UpdateSlots(timeInMs);
	audRadioStation::UpdateStations(timeInMs);
	
	// Store when we last retuned, and make sure we're on music
	u32 gameTimeInMs = fwTimer::GetTimeInMilliseconds();
	if (IsVehicleRadioOn() && IsRetuningVehicleRadio())
	{
		m_LastTimeVehicleRadioWasRetuned = gameTimeInMs;
		m_VehicleRadioRetuneAlreadyFlagged = false;
	}
	if ((m_LastTimeVehicleRadioWasRetuned + 3000)< gameTimeInMs && !m_VehicleRadioRetuneAlreadyFlagged)
	{
		m_VehicleRadioRetuneAlreadyFlagged = true;
		// only flag the retune if it's not talk radio, and it's on music
		const audRadioStation* station = g_RadioAudioEntity.GetPlayerRadioStation();
		if (station && !station->IsTalkStation())
		{
			const audRadioTrack &track = station->GetCurrentTrack();
			if (track.IsInitialised())
			{
				u32 trackCategory = track.GetCategory();
				if (trackCategory == RADIO_TRACK_CAT_MUSIC || trackCategory == RADIO_TRACK_CAT_TAKEOVER_MUSIC)
				{
					m_VehicleRadioJustRetuned = true;
				}
			}
		}
	}
	else
	{
		m_VehicleRadioJustRetuned = false;
	}

	m_CachedAudibleBeatValid = GetNextAudibleBeat(m_CachedTimeTillNextAudibleBeat, m_CachedAudibleBeatBPM, m_CachedAudibleBeatNum);

#if GTA_REPLAY
	if(!audDriver::GetMixer()->IsCapturing())
	{
		PreUpdateReplayMusic();
	}
#endif
}

#if GTA_REPLAY
void audRadioAudioEntity::PreUpdateReplayMusic(bool play)
{
	// Replay music is requested from the game-thread, so need to defer the creation of the sound until the audio update
	static s32 delayCount = 0;

	if(m_ShouldStopReplayMusic)
	{
		if(m_ReplayMusicSound)
		{
			if(!m_StopReplayMusicNoRelease && m_StopReplayMusicPreview && m_ShouldFadeReplayPreview)
			{
				m_ReplayMusicSound->SetReleaseTime(REPLAY_PREVIEW_FADE_LENGTH);
			}
			else if(!m_StopReplayMusicNoRelease)
			{
				m_ReplayMusicSound->SetReleaseTime(250);
			}
			m_ReplayMusicSound->StopAndForget();
		}

		m_ShouldStopReplayMusic = false;
		m_StopReplayMusicNoRelease = false;
		m_StopReplayMusicPreview = false;
	}
	
	if(m_ShouldPrepareReplayMusic)
	{
		audSoundInitParams initParams;	

		if(m_ReplayMusicStartOffset > 100)
		{
			initParams.StartOffset = m_ReplayMusicStartOffset;			
		}
		initParams.EffectRoute = EFFECT_ROUTE_MUSIC;
		initParams.TimerId = 1;				
		initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("MUSIC_SLIDER", 0xA4D158B0));
		Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
		CreateSound_PersistentReference(m_PreviouslyRequestedReplayTrack, &m_ReplayMusicSound, &initParams);

		if(m_ReplayMusicSound)
		{
			m_ReplayMusicSound->Prepare(g_InteractiveMusicManager.GetMusicWaveSlot(), true);	
			UpdateReplayMusicVolume(true);
			delayCount = 1;
		}

		m_ShouldPrepareReplayMusic = false;
	}

	if(m_ShouldPlayReplayMusic)
	{
		if(m_ReplayMusicSound)
		{
			if(audDriver::GetMixer()->IsCapturing())
			{
				audPrepareState prepare = AUD_PREPARING;

				if(delayCount > 0)
				{
					sysIpcSleep(1000);
					prepare = m_ReplayMusicSound->Prepare(g_InteractiveMusicManager.GetMusicWaveSlot(), true);
				}
				delayCount++;
			}

			if(m_ReplayMusicSound->Prepare(g_InteractiveMusicManager.GetMusicWaveSlot(), true) == AUD_PREPARED)
			{
				//Displayf("prepared music : %d", delayCount);
				delayCount = 0;

				bool waitForPlayButton = false;
				if(!play) // play overrides any waiting, play now!
				{
					bool loadingScreenActive = CVideoEditorPlayback::IsLoading();
					waitForPlayButton = CReplayMgr::IsPlaybackPaused() || loadingScreenActive;
				}
				if(m_ReplayMusicSound->GetPlayState() != AUD_SOUND_PLAYING && !audDriver::GetMixer()->IsPaused(0) && (play || !waitForPlayButton ))
				{
					if(m_ReplayMusicStartOffset > 100)
					{
						m_ReplayMusicSound->SetRequestedPostSubmixVolumeAttenuation(g_SilenceVolume);
					}
					else
					{
						m_ReplayMusicSound->SetRequestedPostSubmixVolumeAttenuation(g_FrontendAudioEntity.GetMontageMusicVolume() + g_FrontendAudioEntity.GetClipMusicVolume() + audDriverUtil::ComputeDbVolumeFromLinear(m_ReplayMusicFadeOutScalar));
					}
					m_ReplayMusicSound->GetRequestedSettings()->SetShouldPersistOverClears(true);
					m_ReplayMusicSound->Play();
					UpdateReplayMusicVolume(true);
					m_ShouldPlayReplayMusic = false;
				}
			}
		}
	}	

	if(m_ShouldUpdateReplayPreview)
	{
		UpdateReplayTrackPreview();
		m_ShouldUpdateReplayPreview = false;
	}

	if(m_ShouldUpdateReplayMusic)
	{
		UpdateReplayMusic();
		m_ShouldUpdateReplayMusic = false;
	}
}
#endif // GTA_REPLAY

void audRadioAudioEntity::UpdatePlayerVehicleRadio(u32 timeInMs)
{
	audRadioEmitter *radioEmitter = NULL;

	if(m_LastPlayerVehicle)
	{
		radioEmitter = m_LastPlayerVehicle->GetVehicleAudioEntity()->GetRadioEmitter();
	}
	else
	{
		// Do nothing - script often fiddle with the vehicles etc. during respawn so hold off updating until we are fading back into the game
		if (!NetworkInterface::IsGameInProgress() || !camInterface::IsFadedOut() || m_PlayerVehicleRadioState != PLAYER_RADIO_PLAYING)
		{
			SetPlayerVehicleRadioState(PLAYER_RADIO_OFF);
		}
	}

	switch(m_PlayerVehicleRadioState)
	{
		case PLAYER_RADIO_STARTING:
			{
				if(!m_PlayerRadioStation)
				{
					m_PlayerRadioStation = m_RequestedRadioStation? m_RequestedRadioStation : audRadioStation::GetStation(ChooseRandomRadioStationForGenre(RADIO_GENRE_UNSPECIFIED));
				}
				if(!audRadioSlot::RequestRadioStationForEmitter(m_PlayerRadioStation, radioEmitter,
													audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO))
				{
					audWarningf("Failed to request radio station %s for the player", m_PlayerRadioStation->GetName());
					// We need to wait until we grab a slot so stay in the STARTING state 
					break;
				}
				
				m_LastPlayerVehicle->GetVehicleAudioEntity()->SetRadioStation(m_PlayerRadioStation);
				if(m_PlayerRadioStation)
				{
					m_CurrentStationIndex = m_PlayerRadioStation->GetStationIndex();
				}
				if(m_PlayerRadioStation && m_PlayerRadioStation->IsFrozen())
				{
					if(m_AutoUnfreezeForPlayer)
					{
						Displayf("Automatically unfreezing player station (%s) (PLAYER_RADIO_STARTING)", m_PlayerRadioStation->GetName());
						const_cast<audRadioStation*>(m_PlayerRadioStation)->Unfreeze();
					}
					else
					{
						SetPlayerVehicleRadioState(PLAYER_RADIO_FROZEN);
					}
				}
				if(m_PlayerRadioStation && !m_PlayerRadioStation->IsFrozen() && m_PlayerRadioStation->IsStreamingPhysically())
				{
					SetPlayerVehicleRadioState(PLAYER_RADIO_PLAYING);
				}
				else
				{				
					UpdateRetuneSounds();
				}
				m_PlayerRadioStationPendingRetune = m_PlayerRadioStation;

				// Ensure we bail out of the STARTING state if we get stuck for some reason, since we disable retunes during that
				if(GetCurrentTimeMs() > m_TimeEnteringStartingState + 30000)
				{
					audErrorf("Radio stuck in STARTING state for more than 30 seconds, on station %s. Switching off to recover", 
														m_PlayerRadioStation ? m_PlayerRadioStation->GetName() : "NULL");
					SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
				}
			}
			break;

		case PLAYER_RADIO_FROZEN:
			{
				audRadioStation::StopRetuneSounds();
				if(m_PlayerRadioStation && m_PlayerRadioStation->IsFrozen() && m_AutoUnfreezeForPlayer)
				{
					Displayf("Automatically unfreezing player station (%s) (PLAYER_RADIO_FROZEN)", m_PlayerRadioStation->GetName());
					const_cast<audRadioStation*>(m_PlayerRadioStation)->Unfreeze();
				}
				if(m_PlayerRadioStation && !m_PlayerRadioStation->IsFrozen() && m_PlayerRadioStation->IsStreamingPhysically())
				{
					SetPlayerVehicleRadioState(PLAYER_RADIO_PLAYING);
				}
				/*else if(m_RequestedRadioStation != NULL)
				{
					audWarningf("Retuning radio from a frozen state");

					u32 numStations = audRadioStation::GetNumUnlockedStations();
					Assign(sm_PendingPlayerRadioStationRetunes,(m_RequestedRadioStation->GetStationIndex() + numStations - m_CurrentStationIndex) % numStations);
					m_PlayerRadioStationPendingRetune = m_RequestedRadioStation;
					SetPlayerVehicleRadioState(PLAYER_RADIO_RETUNING);
					m_AutoUnfreezeForPlayer = true; // ensure user can't get stuck on a frozen station; if they choose one we'll unfreeze automatically.
					m_RequestedRadioStation = NULL;
				}*/
			}
			break;

		case PLAYER_RADIO_PLAYING:
			{
				audRadioStation::StopRetuneSounds();

				if (m_LastPlayerVehicle)
				{
					m_PlayerRadioStation = m_LastPlayerVehicle->GetVehicleAudioEntity()->GetRadioStation();

					bool isStationLockedOrHidden = false;

					if (m_PlayerRadioStation)
					{
						bool isHidden = m_PlayerRadioStation->IsHidden();

#if HEIST3_HIDDEN_RADIO_ENABLED
						if (isHidden && m_LastPlayerVehicle->GetModelNameHash() == HEIST3_HIDDEN_RADIO_VEHICLE && m_PlayerRadioStation->GetNameHash() == HEIST3_HIDDEN_RADIO_STATION_NAME)
						{
							isHidden = false;
						}
#endif

						// We want to turn off the radio if we end up listening to a non-favourited station, but only for the player vehicle (as its valid for ambient vehicles 
						// to still listen to a non-favourited station). We also ignore this when the radio control is disabled as we assume that script are doing something 
						// custom (eg. forcing the user to listen to a particular station)
						if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::UserRadioControlDisabled))
						{
							if(!m_PlayerRadioStation->IsFavourited())
							{
								audDisplayf("Player is listening to a non favourite station, force stopping");
								m_LastPlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
								SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
								break;
							}
						}

						isStationLockedOrHidden = m_PlayerRadioStation->IsLocked() || (isHidden && !m_LastPlayerVehicle->GetVehicleAudioEntity()->IsInCarModShop());
					}

					// B*2518841 - Allow retuning to hidden radio whilst a vehicle is in the mod shop
					if (!m_PlayerRadioStation || isStationLockedOrHidden)
					{
						if (m_LastPlayerVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOff())
						{
							// Can happen if script switch instantly from one player controlled vehicle to another
							audDisplayf("Player vehicle radio is playing but vehicle has it switched off, force stopping");
							m_SkipOnOffSound = true;
							SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
							break;
						}
						else
						{
							// If we've simply 'lost' the station while in a network game with the screen faded out, its highly likely that script
							// are messing with vehicles etc. behind the scenes, so just retune back to the previous station rather than advancing
							// round the radio wheel (the station isn't locked or hiddens so there shouldn't be any reason why we can't simply retune 
							// back to it)
							if (!isStationLockedOrHidden && NetworkInterface::IsGameInProgress() && camInterface::IsFadedOut())
							{
								// Force a retune to the same station
								audDisplayf("Player vehicle radio station has been lost, force retuning to previous station");
								sm_PendingPlayerRadioStationRetunes = 1;
								sm_ForceVehicleExplicitRetune = true;
							}
							else
							{
								// Force a retune to a valid station
								audDisplayf("Player station is now locked/hidden, force retuning");
								sm_PendingPlayerRadioStationRetunes = 1;
								sm_ForceVehicleExplicitRetune = false;
							}

							TriggerRetuneBurst();
							SetPlayerVehicleRadioState(PLAYER_RADIO_RETUNING);
							break;
						}
					}
					m_PlayerRadioStationPendingRetune = m_PlayerRadioStation;
					m_LastPlayerRadioStation = m_PlayerRadioStation;

					if (m_PlayerRadioStation)
					{
						m_CurrentStationIndex = m_PlayerRadioStation->GetStationIndex();
					}

					if (m_PlayerRadioStation && m_PlayerRadioStation->IsFrozen() && m_AutoUnfreezeForPlayer)
					{
						Displayf("Automatically unfreezing player station (%s) (PLAYER_RADIO_PLAYING)", m_PlayerRadioStation->GetName());
						const_cast<audRadioStation*>(m_PlayerRadioStation)->Unfreeze();
					}

					// Switch off the radio if the vehicle radio has been switched off
					if (!m_PlayerVehicle || m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOff())
					{
						audDisplayf("Player vehicle radio has been switched off - stopping");
						SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
					}

					if (m_IsPlayerInVehicleForRadio)
					{
						m_LastActiveRadioTimeMs = timeInMs;

						if (sm_PendingPlayerRadioStationRetunes > 0)
						{
							audDisplayf("Pending retunes = %d - retuning", sm_PendingPlayerRadioStationRetunes);
							SetPlayerVehicleRadioState(PLAYER_RADIO_RETUNING);
						}

#if !__FINAL
						//Handle debug skip through radio tracks.
						else if (sm_ShouldSkipForward)
						{
							audRadioSlot::FindAndSkipForwardEmitter(radioEmitter, g_RadioDebugSkipForwardTimeMs);
							sm_ShouldSkipForward = false;
						}
						else if (sm_ShouldQuickSkipForward)
						{
							audRadioSlot::FindAndSkipForwardEmitter(radioEmitter, 10000);
							sm_ShouldQuickSkipForward = false;
						}						
#endif // !__FINAL
						else if (m_RequestedRadioStation != NULL)
						{
							audDisplayf("Requested radio station %s (current %s) - retuning", m_RequestedRadioStation->GetName(), m_PlayerRadioStation ? m_PlayerRadioStation->GetName() : "NULL");
							u32 numStations = audRadioStation::GetNumUnlockedStations();

							bool isHidden = m_RequestedRadioStation->IsHidden();

#if HEIST3_HIDDEN_RADIO_ENABLED
							if (isHidden && m_PlayerVehicle && m_PlayerVehicle->GetModelNameHash() == HEIST3_HIDDEN_RADIO_VEHICLE && m_RequestedRadioStation->GetNameHash() == HEIST3_HIDDEN_RADIO_STATION_NAME)
							{
								isHidden = false;
							}
#endif

							// Special case logic if we're in the mod-shop and retuning to a hidden radio - just do a single retune and we jump stright to the desired station
							if (isHidden && m_PlayerVehicle && m_PlayerVehicle->GetVehicleAudioEntity()->IsInCarModShop())
							{
								sm_PendingPlayerRadioStationRetunes = 1;
							}
							else
							{
								Assign(sm_PendingPlayerRadioStationRetunes, (m_RequestedRadioStation->GetStationIndex() + numStations - m_CurrentStationIndex) % numStations);
							}

							if (sm_PendingPlayerRadioStationRetunes > 0)
							{
								m_PlayerRadioStationPendingRetune = m_RequestedRadioStation;
								SetPlayerVehicleRadioState(PLAYER_RADIO_RETUNING);
							}

							m_RequestedRadioStation = NULL;
						}
					}
					else
					{
						//We are in a static playing state when the player is no longer in a vehicle, so force the state to off.
						audDisplayf("Player not in vehicle for radio - switching off");
						SetPlayerVehicleRadioState(PLAYER_RADIO_OFF);
					}

					// If we've not changed state, ensure the station is still playing physically
					if (m_PlayerRadioStation->IsStreamingVirtually() && m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING)
					{
						audErrorf("Player vehicle playing station virtually; restarting on %s", m_PlayerRadioStation->GetName());
						SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
						m_TimeEnteringStartingState = GetCurrentTimeMs();
					}
				}
			}
			break;

		case PLAYER_RADIO_STOPPING:
			if(m_IsPlayerInVehicleForRadio)
			{
				if(m_PlayerRadioStation)
				{
					PlayRadioOnOffSound(false);
				}
				
				audRadioSlot::FindAndStopEmitter(radioEmitter);
				m_PlayerRadioStationPendingRetune = m_PlayerRadioStation = NULL;
				naCErrorf(m_LastPlayerVehicle, "During update of player vehicle radio, in PLAYER_RADIO_STOPPING state but there is no last player vehicle set");
				//m_LastPlayerRadioStation = m_PlayerRadioStation;
			}
			SetPlayerVehicleRadioState(PLAYER_RADIO_OFF);
			break;

		case PLAYER_RADIO_RETUNING:
			UpdateRetuneSounds();
			if(sm_PendingPlayerRadioStationRetunes > 0)
			{
				bool isHidden = m_PlayerRadioStationPendingRetune && m_PlayerRadioStationPendingRetune->IsHidden();

#if HEIST3_HIDDEN_RADIO_ENABLED
				if (isHidden && m_PlayerVehicle && m_PlayerVehicle->GetModelNameHash() == HEIST3_HIDDEN_RADIO_VEHICLE && m_PlayerRadioStationPendingRetune->GetNameHash() == HEIST3_HIDDEN_RADIO_STATION_NAME)
				{	
					// Do nothing, keep the retune station to the hidden station
				}
				else 
#endif

				// B*2518841 - If we're in the car mod shop and want to retune to a hidden station, just retune to the explicit requested station. 
				// sm_PendingPlayerRadioStationRetunes assumes that the radio is one of the unlocked stations (ie. not hidden) and therefore isn't valid
				if(!(m_PlayerRadioStationPendingRetune && m_PlayerVehicle && isHidden && m_PlayerVehicle->GetVehicleAudioEntity()->IsInCarModShop()))
				{
					if(sm_ForceVehicleExplicitRetune)
					{
						m_PlayerRadioStationPendingRetune = audRadioStation::GetStation(m_CurrentStationIndex % audRadioStation::GetNumUnlockedStations());
						audDisplayf("Player radio retuning - target station is %s", m_PlayerRadioStationPendingRetune? m_PlayerRadioStationPendingRetune->GetName() : "null");						
					}
					else
					{
						m_PlayerRadioStationPendingRetune = audRadioStation::GetStation((m_CurrentStationIndex + sm_PendingPlayerRadioStationRetunes) % audRadioStation::GetNumUnlockedStations());
						audDisplayf("Player radio pending %d retunes - target station is %s", sm_PendingPlayerRadioStationRetunes, m_PlayerRadioStationPendingRetune? m_PlayerRadioStationPendingRetune->GetName() : "null");
					}
				}
				else
				{
					audDisplayf("Player retuning to a hidden modshop radio station %s", m_PlayerRadioStationPendingRetune? m_PlayerRadioStationPendingRetune->GetName() : "null");
				}
				
				const audRadioStation *station = m_PlayerRadioStationPendingRetune;
				if(station && station->IsFrozen())
				{
					Displayf("Automatically unfreezing player station (%s) (RETUNING)", station->GetName());
					const_cast<audRadioStation*>(station)->Unfreeze();
				}
				audRadioSlot::FindAndRetuneEmitter(radioEmitter);
			}
			else if(!audRadioSlot::IsEmitterRetuning(radioEmitter))
			{
				//We are done tuning.
				SetPlayerVehicleRadioState(PLAYER_RADIO_PLAYING);
			}
			break;

		case PLAYER_RADIO_OFF:
		default: //Intentional fall-through.
			if(!sm_IsMobilePhoneRadioActive)
			{
					audRadioStation::StopRetuneSounds();
			}
			if(m_PlayerVehicle && ((m_IsPlayerInAVehicleWithARadio && m_ForcePlayerStation) || m_IsPlayerInVehicleForRadio) && !m_WasPlayerInVehicleForRadioLastFrame)
			{
				const bool allowScoreAndRadio = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowScoreAndRadio);
				const bool isScorePlaying = g_InteractiveMusicManager.IsMusicPlaying() && !allowScoreAndRadio && !g_InteractiveMusicManager.IsScoreMutedForRadio();
				const bool isForcingStation = (m_ForcePlayerStation != NULL);

				m_PlayerRadioStation = m_PlayerVehicle->GetVehicleAudioEntity()->GetRadioStation();

				//Only automatically turn on the radio if the player has not previous turned this radio off, or if mobile phone
				//radio is active, as this should transfer to the vehicle radio.
				if(isForcingStation && !m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOn())
				{
					audDisplayf("Forcing radio station, but radio is currently off - switching ON");
					m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(false);
				}
				if(m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOn() || sm_IsMobilePhoneRadioActive)
				{
					if(isForcingStation || m_PlayerRadioStation == NULL)
					{
						if(isForcingStation)
						{
							m_PlayerRadioStation = m_ForcePlayerStation;
							m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioStation(m_ForcePlayerStation, true, true);
							audAssertf(m_PlayerVehicle->GetVehicleAudioEntity()->GetRadioStation() == m_ForcePlayerStation, "Failed to set radio station on the player vehicle!");

							naDisplayf("Forced retune to station %s", m_ForcePlayerStation->GetName());
							m_ForcePlayerStation = NULL;
							SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
							m_TimeEnteringStartingState = GetCurrentTimeMs();
						}
						else if(m_PlayerVehicle->GetVehicleAudioEntity()->GetLastRadioStation() != NULL)
						{
							m_PlayerRadioStation = m_PlayerVehicle->GetVehicleAudioEntity()->GetLastRadioStation();
							audDisplayf("Re-starting player radio with previous radio station (%s)", m_PlayerRadioStation->GetName());
							SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
							m_TimeEnteringStartingState = GetCurrentTimeMs();
						}
						/*else if(!isScorePlaying &&
							(m_LastPlayerRadioStation) &&
							(timeInMs < (m_LastActiveRadioTimeMs + g_MaxPlayerOutOfVehicleTimeForAutoTune)))
						{
							//Auto-tune to the last player station.
							m_PlayerRadioStation = m_LastPlayerRadioStation;
							TriggerRetuneBurst();
							naDisplayf("Autotuning radio");
							SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
							m_TimeEnteringStartingState = GetCurrentTimeMs();
						}*/
						else
						{
							//Choose a random station to start with.
							m_PlayerRadioStation = audRadioStation::GetStation(ChooseRandomRadioStationForGenre(m_PlayerVehicle->GetVehicleAudioEntity()->GetRadioGenre()));
							// NOTE: this will leave the radio off if the car was set up as OFF; required for the hearse
							if(m_PlayerRadioStation == NULL)
							{
								audDisplayf("Vehicle has OFF as the default genre - leaving radio OFF");
								SetPlayerVehicleRadioState(PLAYER_RADIO_OFF);
								break;
							}

							audDisplayf("Picked %s as a random radio station based on genre", m_PlayerRadioStation->GetName());
							SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
							m_TimeEnteringStartingState = GetCurrentTimeMs();
						}
											
						if(isScorePlaying && !(m_PlayerRadioStation && m_PlayerRadioStation->IsFrozen()))
						{
							// switch the vehicle radio off if score is active and the player station isn't frozen
							audDisplayf("1 - Switching OFF player radio. isScorePlaying: %s, playerRadioFrozen: %s", isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
							SetPlayerVehicleRadioState(PLAYER_RADIO_OFF);
							m_PlayerRadioStation = NULL;
							m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
						}
						else
						{
							audDisplayf("2 - STARTING player radio station %s. isScorePlaying: %s, playerRadioFrozen: %s", m_PlayerRadioStation->GetName(), isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
							SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
							m_TimeEnteringStartingState = GetCurrentTimeMs();
						}
					}
					else
					{
						if(isScorePlaying && !(m_PlayerRadioStation && m_PlayerRadioStation->IsFrozen()))
						{
							if(m_PlayerRadioStation && NetworkInterface::IsGameInProgress() && m_PlayerVehicle->GetDriver() && m_PlayerVehicle->GetDriver()->IsNetworkPlayer())
							{
								//We want to play the active vehicle station.
								audDisplayf("8 - Score playing, but entering network player vehicle which has the radio on. STARTING player radio station %s to keep in sync. isScorePlaying: %s, playerRadioFrozen: %s", m_PlayerRadioStation->GetName(), isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
								SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING); // was playing
								m_TimeEnteringStartingState = GetCurrentTimeMs();
							}
							else
							{
								// If music is playing and we enter a vehicle with a radio, switch it off
								audDisplayf("3 - STOPPING player radio. isScorePlaying: %s, playerRadioFrozen: %s", isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
								m_SkipOnOffSound = true;

								if (m_PlayerVehicleRadioState != PLAYER_RADIO_OFF)
								{
									SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
								}
								
								m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
							}
						}
						else
						{
							//We want to play the active vehicle station.
							audDisplayf("4 - STARTING player radio station %s. isScorePlaying: %s, playerRadioFrozen: %s", m_PlayerRadioStation->GetName(), isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
							SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING); // was playing
							m_TimeEnteringStartingState = GetCurrentTimeMs();
						}
					}
				}
				else if(m_PlayerRadioStation != NULL)
				{
					if(isScorePlaying && !(m_PlayerRadioStation && m_PlayerRadioStation->IsFrozen()))
					{
						// If music is playing and we enter a vehicle with a radio, switch it off
						audDisplayf("5- STOPPING player radio. isScorePlaying: %s, playerRadioFrozen: %s", isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
						m_SkipOnOffSound = true;
						SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
						m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
					}
					else
					{
						//We want to play the active vehicle station.
						audDisplayf("6 - PLAYING player radio. isScorePlaying: %s, playerRadioFrozen: %s", isScorePlaying? "true" : "false", m_PlayerRadioStation ? (m_PlayerRadioStation->IsFrozen()? "true" : "false") : "NULL");
						SetPlayerVehicleRadioState(PLAYER_RADIO_PLAYING);
					}
				}
				else if(isScorePlaying)
				{
					// Leave the radio off
					audDisplayf("7 - Score is playing, swtiching radio off.");
					SetPlayerVehicleRadioState(PLAYER_RADIO_OFF);
				}
								
			}
			break;
	}

	// Clear network flag whenever radio is off/retuned
	m_bMPDriverHasTriggeredRadioChange &= IsRetuningVehicleRadio();
}

void audRadioAudioEntity::SetPlayerVehicleRadioState(u8 newState)
{
	if (newState != m_PlayerVehicleRadioState)
	{
		m_PlayerVehicleRadioState = newState;
	}
}

void audRadioAudioEntity::SetMobileRadioState(u8 newState)
{
	if (newState != m_MobilePhoneRadioState)
	{
		m_MobilePhoneRadioState = newState;
	}
}

bool audRadioAudioEntity::IsMobileRadioNormallyPermitted() const
{
	return !CNetwork::IsGameInProgress() && !CTheScripts::GetPlayerIsOnAMission() && !audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior() && sm_IsMobilePhoneRadioAvailable;
}

void audRadioAudioEntity::UpdateMobilePhoneRadio(u32 timeInMs)
{	
	audRadioEmitter *radioEmitter = &m_MobilePhoneRadioEmitter;

	// ensure mobile radio is never active in game play unless explicitly requested by script
	if(!IsInFrontendMode() && !IsMobileRadioNormallyPermitted() && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::MobileRadioInGame) && sm_IsMobilePhoneRadioActive && !fwTimer::IsGamePaused() && !audNorthAudioEngine::IsInNetworkLobby() && !audRadioStation::IsPlayingEndCredits() && DYNAMICMIXER.GetMobileRadioCount() == 0)
	{		
		SetMobilePhoneRadioState(false);
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		SetMobilePhoneRadioState(false);
	}
#endif	

	if((!sm_IsMobilePhoneRadioActive && m_WasMobilePhoneRadioActiveLastFrame) || (m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING))
	{
		//The player just turned off mobile phone radio, or has active vehicle radio, so force the state to off.
		SetMobileRadioState(PLAYER_RADIO_OFF);
	}

	switch(m_MobilePhoneRadioState)
	{
		case PLAYER_RADIO_STARTING:
			StartMobileRadio();			
			break;

		case PLAYER_RADIO_PLAYING:

			// Wait until the track is playing before we stop the retune sound
			if(m_PlayerRadioStation && m_PlayerRadioStation->GetCurrentTrack().IsPlayingPhysicallyOrVirtually())
			{
				audRadioStation::StopRetuneSounds();
			}
			if(!m_PlayerRadioStation || m_PlayerRadioStation->IsLocked() || m_PlayerRadioStation->IsHidden())
			{
				audDisplayf("Player mobile radio station is now locked, force retuning");
				// Force a retune to a valid station
				sm_PendingPlayerRadioStationRetunes = 1;		
				SetMobileRadioState(PLAYER_RADIO_RETUNING);
				break;
			}

			if(m_PlayerRadioStation)
			{
				m_CurrentStationIndex = m_PlayerRadioStation->GetStationIndex();
			}
			m_PlayerRadioStationPendingRetune = m_PlayerRadioStation;
			m_LastPlayerRadioStation = m_PlayerRadioStation;
			m_LastActiveRadioTimeMs = timeInMs;

			if(sm_PendingPlayerRadioStationRetunes > 0)
			{
				m_PlayerRadioStationPendingRetune = audRadioStation::GetStation((m_CurrentStationIndex + sm_PendingPlayerRadioStationRetunes) %
																					audRadioStation::GetNumUnlockedStations());
				SetMobileRadioState(PLAYER_RADIO_RETUNING);
			}
#if !__FINAL
			//Handle debug skip through radio tracks.
			else if(sm_ShouldSkipForward)
			{
				audRadioSlot::FindAndSkipForwardEmitter(radioEmitter, g_RadioDebugSkipForwardTimeMs);
				sm_ShouldSkipForward = false;
			}
			else if(sm_ShouldQuickSkipForward)
			{
				audRadioSlot::FindAndSkipForwardEmitter(radioEmitter, 10000);
				sm_ShouldQuickSkipForward = false;
			}
#endif // !__FINAL
			else if(m_RequestedRadioStation)
			{
				u32 numStations = audRadioStation::GetNumUnlockedStations();			
				Assign(sm_PendingPlayerRadioStationRetunes, (m_RequestedRadioStation->GetStationIndex() + numStations - m_CurrentStationIndex) % numStations);

				if (sm_PendingPlayerRadioStationRetunes > 0)
				{
					m_PlayerRadioStationPendingRetune = m_RequestedRadioStation;
					SetMobileRadioState(PLAYER_RADIO_RETUNING);
				}

				m_RequestedRadioStation = NULL;
			}
			break;

		case PLAYER_RADIO_STOPPING:
			{
				PlayRadioOnOffSound(false);
				audRadioSlot::FindAndStopEmitter(radioEmitter);
				m_PlayerRadioStationPendingRetune = m_PlayerRadioStation = NULL;		
				SetMobileRadioState(PLAYER_RADIO_OFF);
			}
			break;

		case PLAYER_RADIO_RETUNING:
			UpdateRetuneSounds();
			
			if(sm_PendingPlayerRadioStationRetunes > 0)
			{
				m_PlayerRadioStationPendingRetune = audRadioStation::GetStation((m_CurrentStationIndex + sm_PendingPlayerRadioStationRetunes) %
																							audRadioStation::GetNumUnlockedStations());
				audRadioSlot::FindAndRetuneEmitter(radioEmitter);
			}
			else if(!audRadioSlot::IsEmitterRetuning(radioEmitter))
			{
				//We are done tuning.
				SetMobileRadioState(PLAYER_RADIO_PLAYING);
			}
			break;

		case PLAYER_RADIO_OFF:
		default: //Intentional fall-through.
			//audRadioStation::StopRetuneSounds();
			if(sm_IsMobilePhoneRadioActive && (m_PlayerVehicleRadioState == PLAYER_RADIO_OFF))
			{
				//Auto-tune to the last player station.				
				if (!CPauseMenu::IsActive() || !m_PlayerRadioStation) // Possibly should always do this? Intentionally limiting scope to pause menu only as a targetted fix for url:bugstar:7441459 Fixer: 3A - When cycling through the radio stations in the Front End Menu, the station selected does not match the audio being played.
				{
					m_PlayerRadioStation = m_LastPlayerRadioStation;					
				}				
								
				if(m_ForcePlayerStation != NULL || m_PlayerRadioStation == NULL)
				{
					//Choose a random station to start with.
					//TriggerRetuneBurst();

					 if(m_ForcePlayerStation != NULL)
					 {
						 m_PlayerRadioStation = m_ForcePlayerStation;
						 /*if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::StickyForcedRadioStation))
						 {
							 m_ForcePlayerStation = NULL;
						 }*/
					 }
					 else
					 {
						 // Keep in sync with the pause menu
						 u32 stationIndex = (u32)CPauseMenu::GetMenuPreference(PREF_RADIO_STATION);
						 if(audVerifyf(stationIndex < audRadioStation::GetNumUnlockedStations(), "Invalid station ID in PREF_RADIO_STATION: %u (only have %u stations)", stationIndex, audRadioStation::GetNumUnlockedStations() ))
						 {
							 m_PlayerRadioStation = audRadioStation::GetStation(stationIndex);
						 }
						 else
						 {
							 m_PlayerRadioStation = audRadioStation::GetStation(0);
						 }
					 }					 
				}
				else if(!audRadioSlot::IsStationActive(m_PlayerRadioStation) && !IsInFrontendMode() && !audRadioStation::IsPlayingEndCredits())
				{
					//TriggerRetuneBurst();
				}

				SetMobileRadioState(PLAYER_RADIO_STARTING);
				m_TimeEnteringStartingState = GetCurrentTimeMs();
				StartMobileRadio();
			}
			else
			{
				//Ensure we remove the emitter for the player's mobile phone if it isn't active.
				audRadioSlot::FindAndStopEmitter(radioEmitter);
			}
			break;
	}
}

void audRadioAudioEntity::StartMobileRadio()
{
	if (m_RequestedRadioStation && m_PlayerRadioStation != m_RequestedRadioStation)
	{
		m_PlayerRadioStation = m_RequestedRadioStation;
	}

	if (!audRadioSlot::RequestRadioStationForEmitter(m_PlayerRadioStation, &m_MobilePhoneRadioEmitter,
		audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO))
	{
		audWarningf("Failed to request radio station %s for player mobile radio", m_PlayerRadioStation ? m_PlayerRadioStation->GetName() : "NULL");
	}
	else
	{
		if (m_PlayerRadioStation && m_PlayerRadioStation->IsStreamingPhysically())
		{
			SetMobileRadioState(PLAYER_RADIO_PLAYING);
		}

		m_PlayerRadioStationPendingRetune = m_PlayerRadioStation;
	}
}

bool audRadioAudioEntity::StopEmittersForPlayerVehicle() const
{
	return m_StopEmittersForPlayerVehicle;
}

const audRadioStation *audRadioAudioEntity::RequestVehicleRadio(CVehicle *vehicle)
{
	const audRadioStation *radioStation = NULL;
	const bool vehicleContainsPlayer = vehicle->ContainsLocalPlayer();

	if(m_MobilePhoneRadioState == PLAYER_RADIO_PLAYING && vehicleContainsPlayer && m_PlayerRadioStation)
	{
		audRadioEmitter *radioEmitter = vehicle->GetVehicleAudioEntity()->GetRadioEmitter();
		if(audRadioSlot::RequestRadioStationForEmitter(m_PlayerRadioStation, radioEmitter, radioEmitter->GetPriority()))
		{
			return m_PlayerRadioStation;
		}
		else
		{
			return NULL;
		}
	}

	if((vehicle != NULL) && (audRadioStation::GetNumUnlockedStations() > 0))
	{
		CVehicle *playerVehicle = CGameWorld::FindLocalPlayerVehicle();
		
		bool followPlayerInCar = false;
		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnSCTVTeam())
		{
			CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
			if (pFollowPlayer && pFollowPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				CVehicle* pFollowPlayerVehicle = pFollowPlayer->GetMyVehicle();
				if (pFollowPlayerVehicle)
					followPlayerInCar = (pFollowPlayerVehicle==vehicle);
			}
		}

		//Don't request any ambient radio when the player moving quickly in a vehicle, to save streaming bandwidth.
		if(vehicleContainsPlayer || followPlayerInCar || playerVehicle == NULL || playerVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE || !StopEmittersForPlayerVehicle())
		{			
			//Don't play radio from all vehicles, and allow the vehicle to specify no radio.
			// If script have specified LOUD_RADIO then skip the random probability
			if(vehicleContainsPlayer || vehicle->GetVehicleAudioEntity()->IsPersonalVehicle() || followPlayerInCar || vehicle->GetVehicleAudioEntity()->HasLoudRadio() || audEngineUtil::ResolveProbability(g_VehicleRadioProbability))
			{
				// use the last radio station for this car if there was one
				const audRadioStation *lastRadioStation = vehicle->GetVehicleAudioEntity()->GetLastRadioStation();
				if(lastRadioStation)
				{
					radioStation = lastRadioStation;
					// Don't allow ambient, non-player vehicles to select a frozen station
					if(!vehicleContainsPlayer && !followPlayerInCar && !vehicle->PopTypeIsMission() && (radioStation->IsFrozen() || radioStation->IsLocked()))
					{
						radioStation = NULL;
					}
					else
					{
						//See if we can play it.
						audRadioEmitter *radioEmitter = vehicle->GetVehicleAudioEntity()->GetRadioEmitter();
						if(!audRadioSlot::RequestRadioStationForEmitter(radioStation, radioEmitter, radioEmitter->GetPriority()))
						{
							radioStation = NULL;
						}
						
						// Automatically unfreeze a station if the player is going to listen to it.
						if(radioStation && m_AutoUnfreezeForPlayer && (vehicleContainsPlayer || followPlayerInCar) && radioStation->IsFrozen())
						{							
							Displayf("Auto-Unfreezing radio station %s for player vehicle", radioStation->GetName());
							const_cast<audRadioStation*>(radioStation)->Unfreeze();
						}
					}
				}

#if HEIST3_HIDDEN_RADIO_ENABLED
				if (vehicle->GetModelNameHash() == HEIST3_HIDDEN_RADIO_VEHICLE)
				{
					if (GetEncryptionKeyForBank(audRadioStation::sm_Heist3HiddenRadioBankId))
					{
						radioStation = audRadioStation::FindStation(HEIST3_HIDDEN_RADIO_STATION_NAME);
					}
				}
#endif

				// Only switch a radio on automatically if the vehicle is not tagged with the 'off' genre
				if(!radioStation && vehicle->GetVehicleAudioEntity()->GetRadioGenre() != RADIO_GENRE_OFF)
				{
					// Player character overrides
					if(m_ForcePlayerCharacterStations && radioStation == NULL && vehicle->GetDriver())
					{
						switch(vehicle->GetDriver()->GetPedType())
						{
						case PEDTYPE_PLAYER_0: //MICHAEL
							{
								radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_01_CLASS_ROCK", 0x9E34587));
							}
							break;
						case PEDTYPE_PLAYER_1: //FRANKLIN
							{
								radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_03_HIPHOP_NEW", 0xFA17DE37));
							}
							break;
						case PEDTYPE_PLAYER_2: //TREVOR
							{
								radioStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_04_PUNK", 0x719B0AFB));
							}
							break;
						default:
							break;
						}
					}

					// If the vehicle hasn't already picked a station, choose based on ped/vehicle genre
					if(radioStation == NULL)
					{	
						const bool isPlayerDriver = vehicle->GetDriver() && vehicle->GetDriver()->IsPlayer();

						//Choose a station at random based on genre prefs
						RadioGenre genre1 = RADIO_GENRE_UNSPECIFIED, genre2 = RADIO_GENRE_UNSPECIFIED;
						if(vehicle->GetDriver() && !vehicle->GetDriver()->IsPlayer())
						{
							s32 g1, g2;
							vehicle->GetDriver()->GetPedRadioCategory(g1, g2);
							if(g1 != -1)
							{
								genre1 = (RadioGenre)g1;
							}
							if(g2 != -1)
							{
								genre2 = (RadioGenre)g2;
							}
						}
						else
						{
							genre1 = vehicle->GetVehicleAudioEntity()->GetRadioGenre();
						}
						
						radioStation = audRadioStation::GetStation(ChooseRandomRadioStationForGenre(genre1));

						bool pickFavouriteAsFallback = false;

						if(radioStation && isPlayerDriver && !radioStation->IsFavourited())
						{
							pickFavouriteAsFallback = true;
							radioStation = nullptr;
						}

						if(radioStation)
						{
							//See if we can play it.
							audRadioEmitter *radioEmitter = vehicle->GetVehicleAudioEntity()->GetRadioEmitter();
							if(!audRadioSlot::RequestRadioStationForEmitter(radioStation, radioEmitter, radioEmitter->GetPriority()))
							{
								radioStation = NULL;
							}
						}
						

						if(radioStation == NULL && genre2 != RADIO_GENRE_OFF)
						{
							// fall back to the second choice
							radioStation = audRadioStation::GetStation(ChooseRandomRadioStationForGenre(genre2));

							if(radioStation && isPlayerDriver && !radioStation->IsFavourited())
							{
								pickFavouriteAsFallback = true;
								radioStation = nullptr;
							}

							if(radioStation)
							{
								//See if we can play it.
								audRadioEmitter *radioEmitter = vehicle->GetVehicleAudioEntity()->GetRadioEmitter();
								if(!audRadioSlot::RequestRadioStationForEmitter(radioStation, radioEmitter, radioEmitter->GetPriority()))
								{
									radioStation = NULL;
								}
							}
						}

						if(!radioStation && pickFavouriteAsFallback)
						{
							if(audRadioStation::GetNumUnlockedFavouritedStations() > 0)
							{
								u32 stationIndex = audEngineUtil::GetRandomNumberInRange(0, audRadioStation::GetNumUnlockedFavouritedStations()-1);
								radioStation = audRadioStation::GetStation(stationIndex);

								if(radioStation)
								{
									//See if we can play it.
									audRadioEmitter *radioEmitter = vehicle->GetVehicleAudioEntity()->GetRadioEmitter();
									if(!audRadioSlot::RequestRadioStationForEmitter(radioStation, radioEmitter, radioEmitter->GetPriority()))
									{
										radioStation = NULL;
									}
								}
							}
							
						}
					}
				}// genre != off
			}
		}
	}

	return radioStation;
}

u32 audRadioAudioEntity::ChooseRandomRadioStationForGenre(const RadioGenre genre) const
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::LimitAmbientRadioStations))
	{
		// MAGDEMO 2012 - limit radio station selection
		atFixedArray<u32,4> options;
		for(u32 i = 0; i < audRadioStation::GetNumUnlockedStations(); i++)
		{
			audRadioStation *station = audRadioStation::GetStation(i);
			if(station->IsTalkStation() || station->IsInGenre(RADIO_GENRE_POP) || station->IsInGenre(RADIO_GENRE_CLASSIC_HIPHOP))
			{
				options.Append() = i;
			}
		}
		if(options.GetCount() == 0)
		{
			return g_OffRadioStation;
		}
		return options[audEngineUtil::GetRandomNumberInRange(0, options.GetCount()-1)];
	}
	else
	{
		if(genre == RADIO_GENRE_UNSPECIFIED)
		{
			// pick at random
			return audEngineUtil::GetRandomNumberInRange(0, audRadioStation::GetNumUnlockedStations()-1);
		}
		else if(genre == RADIO_GENRE_OFF)
		{
			return g_OffRadioStation;
		}
		else
		{
			const u32 maxOptions = 3;
			u32 options[maxOptions];
			u32 numOptions = 0;
			for(u32 i = 0; i < audRadioStation::GetNumUnlockedStations(); i++)
			{
				const audRadioStation *station = audRadioStation::GetStation(i);

				if(station->IsInGenre(genre))
				{
					// if we find a match that's streaming physically then use that so we maximise disk bandwidth
					if(station->IsStreamingPhysically())
					{
						return i;
					}
					else
					{
						if(numOptions < maxOptions)
						{
							options[numOptions++] = i;
						}
					}
				}
			}
			if(numOptions == 0)
			{
				//Assert(0);
				return g_OffRadioStation;
			}
			else
			{
				return options[audEngineUtil::GetRandomNumberInRange(0, numOptions-1)];
			}
		}
	}
}

void audRadioAudioEntity::StopVehicleRadio(CVehicle *vehicle, bool clearStation)
{
	if(naVerifyf(vehicle, "Null CVehicle * passed into StopVehicleRadio"))
	{
		if(vehicle == FindPlayerVehicle() && m_IgnorePlayerVehicleRadioShutdown)
		{
			return;
		}

		audRadioEmitter *radioEmitter = vehicle->GetVehicleAudioEntity()->GetRadioEmitter();
		audRadioSlot::FindAndStopEmitter(radioEmitter, clearStation);
	}

	if(vehicle == m_LastPlayerVehicle)
	{
		if(!sm_IsMobilePhoneRadioActive)
		{
			//Ensure we stop the retune sounds for the player's last vehicle to catch the case of the vehicle
			//being destroyed while its radio is retuning.
			audRadioStation::StopRetuneSounds();
			sm_PendingPlayerRadioStationRetunes = 0;
		}
		m_LastPlayerVehicle = NULL;
	}
}

bool audRadioAudioEntity::RequestStaticRadio(audStaticRadioEmitter *emitter, u32 stationNameHash, s32 requestedPCMChannel)
{
	naCErrorf(emitter, "Null audStaticRadioEmitter passed into RequestStaticRadio");

	if(emitter && (audRadioStation::GetNumUnlockedStations() > 0) REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{		
		if(!StopEmittersForPlayerVehicle())
		{
			const audRadioStation *station = NULL;
			if(stationNameHash == ATSTRINGHASH("random", 0xEBE23568))
			{
				station = audRadioStation::GetStation(audEngineUtil::GetRandomNumberInRange(0, audRadioStation::GetNumUnlockedStations() - 1));
			}
			else
			{
				station = audRadioStation::FindStation(stationNameHash);				
			}

			if(station)
			{
				//See if we can play it.
				if(audRadioSlot::RequestRadioStationForEmitter(station, emitter, emitter->GetPriority(), requestedPCMChannel))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void audRadioAudioEntity::StopStaticRadio(audStaticRadioEmitter *emitter)
{
	naAssertf(emitter, "Null audStaticRadioEmitter passed into StopStaticRadio");
	audRadioSlot::FindAndStopEmitter(emitter);
}

bool audRadioAudioEntity::IsStaticRadioEmitterActive(audStaticRadioEmitter *emitter)
{
	return audRadioSlot::IsEmitterActive(emitter);
}

void audRadioAudioEntity::SkipStationsForward(void)
{
#if !__FINAL
	audRadioStation::SkipForwardStations(g_RadioDebugSkipForwardTimeMs);	//Skip all virtually streaming stations.
	audRadioSlot::SkipForwardSlots(g_RadioDebugSkipForwardTimeMs);		//Skip all physically streaming stations.
#endif
}

const char *audRadioAudioEntity::GetPlayerRadioStationNamePendingRetune() const
{
	if(m_PlayerRadioStationPendingRetune)
	{
		return m_PlayerRadioStationPendingRetune->GetName();
	}
	else
	{
		return NULL;
	}
}

const audRadioStation *audRadioAudioEntity::GetPlayerRadioStationPendingRetune() const
{
	return m_PlayerRadioStationPendingRetune;
}

void audRadioAudioEntity::UpdateUserControl(u32 BANK_ONLY(timeInMs))
{
#if __BANK
	if(sm_ShouldPerformRadioRetuneTest && (timeInMs > m_LastTestRetuneTimeMs + sm_RetunePeriodMs))
	{
		if(audEngineUtil::ResolveProbability(0.5f))
		{
			RetuneRadioUp();
		}
		else
		{
			RetuneRadioDown();
		}

		m_LastTestRetuneTimeMs = timeInMs;

		return; //Don't take any other user input if we are testing retunes.
	}
#endif // __BANK

#if !__FINAL
	if(ioMapper::DebugKeyPressed(KEY_PERIOD))
	{
		if(ioMapper::DebugKeyDown(KEY_CONTROL))
		{
			sm_ShouldQuickSkipForward = true;
		}
		else
		{
			sm_ShouldSkipForward = true;
		}
	}
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_COMMA, KEYBOARD_MODE_DEBUG, "Toggle mobile phone radio on/off"))
	{
		ToggleMobilePhoneRadioState();
	}
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_8, KEYBOARD_MODE_DEBUG, "Toggle radio on/off"))
	{
		sm_ShouldMuteRadio = !sm_ShouldMuteRadio;
	}
#endif // !__FINAL
}

void audRadioAudioEntity::ComputeVolumeOffset(void)
{
	m_VolumeOffset = audDriverUtil::ComputeDbVolumeFromLinear(m_FadeVolumeLinear);
}

bool audRadioAudioEntity::IsTimeToRetune(u32 timeInMs)
{
	return (timeInMs >= (sm_LastRetuneTime + g_MinRadioRetuneDelayMs));
}

void audRadioAudioEntity::RetuneRadioUp(void)
{
	CVehicle *playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if((sm_IsMobilePhoneRadioActive || (playerVehicle && playerVehicle->GetVehicleAudioEntity()->GetHasNormalRadio())) &&
		(g_RadioAudioEntity.GetPlayerRadioStation()))
	{
		sm_PendingPlayerRadioStationRetunes = (sm_PendingPlayerRadioStationRetunes + 1) %
			audRadioStation::GetNumUnlockedStations();
		sm_LastRetuneTime = GetCurrentTimeMs();
	}
}

void audRadioAudioEntity::RetuneRadioDown(void)
{
	CVehicle *playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if((sm_IsMobilePhoneRadioActive || (playerVehicle && playerVehicle->GetVehicleAudioEntity()->GetHasNormalRadio())) &&
		(g_RadioAudioEntity.GetPlayerRadioStation()))
	{
		Assign(sm_PendingPlayerRadioStationRetunes, (sm_PendingPlayerRadioStationRetunes +
			audRadioStation::GetNumUnlockedStations() - 1) % audRadioStation::GetNumUnlockedStations());
		sm_LastRetuneTime = GetCurrentTimeMs();		
	}
}

#if __BANK
void audRadioAudioEntity::DebugHideStation(void)
{
	audRadioStation* station = audRadioStation::FindStation(g_RadioStationCombo->GetString(sm_RequestedRadioStationIndex));

	if (station)
	{
		station->SetHidden(true);
	}
}

void audRadioAudioEntity::DebugUnHideStation(void)
{
	audRadioStation* station = audRadioStation::FindStation(g_RadioStationCombo->GetString(sm_RequestedRadioStationIndex));

	if (station)
	{
		station->SetHidden(false);
	}
}

void audRadioAudioEntity::DebugLockStation(void)
{
	audRadioStation* station = audRadioStation::FindStation(g_RadioStationCombo->GetString(sm_RequestedRadioStationIndex));

	if (station)
	{
		station->SetLocked(true);
	}
}

void audRadioAudioEntity::DebugUnlockStation(void)
{
	audRadioStation* station = audRadioStation::FindStation(g_RadioStationCombo->GetString(sm_RequestedRadioStationIndex));

	if (station)
	{
		station->SetLocked(false);
	}
}
#endif

void audRadioAudioEntity::RetuneToStation(const char *stationName)
{
	if(!strcmp(stationName, "OFF"))
	{
		if(m_PlayerRadioStation || sm_IsMobilePhoneRadioActive)
		{
			//Switch off the player radio (vehicle and mobile.)
			if(m_PlayerVehicle)
			{
				SetPlayerVehicleRadioState(PLAYER_RADIO_STOPPING);
				m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
			}

			m_PlayerRadioStation = NULL;
			m_PlayerRadioStationPendingRetune = NULL;
			m_RequestedRadioStation = NULL;
			sm_IsMobilePhoneRadioActive = false;			
		}
	}
	else
	{
		RetuneToStation(atStringHash(stationName));
	}
}

void audRadioAudioEntity::RetuneToStation(const u32 stationNameHash)
{
	const audRadioStation *station = audRadioStation::FindStation(stationNameHash);
	if(station)
	{
		RetuneToStation(station);
	}
}

void audRadioAudioEntity::SwitchOffRadio()
{
	RetuneToStation("OFF");
}

#if __BANK
void audRadioAudioEntity::PrintRadioDebug()
{
	audDisplayf("Player radio state: %u", m_PlayerVehicleRadioState);
	audDisplayf("Mobile phone radio state: %u", m_MobilePhoneRadioState);
	audDisplayf("Player radio station: %s", m_PlayerRadioStation? m_PlayerRadioStation->GetName() : "NULL");
	audDisplayf("Player vehicle: %s", m_PlayerVehicle? m_PlayerVehicle->GetBaseModelInfo()->GetModelName() : "NULL");
}
#endif

bool audRadioAudioEntity::ShouldBlockPauseMenuRetunes() const
{
	// url:bugstar:7806555 - Summer 2022 - Radio News Broadcast - Players can bypass the Radio News Broadcast by changing the Radio Station in the Pause Menu Settings tab. 
	if(IsInFrontendMode() && CPauseMenu::IsMP() && m_PlayerRadioStation && m_PlayerRadioStation->IsLocalPlayerNewsStation() && !m_PlayerRadioStation->IsLocked())
	{
		return true;
	}

	return false;
}

void audRadioAudioEntity::RetuneToStation(const audRadioStation *station)
{
	audDisplayf("Tuning player radio station to %s", station? station->GetName() : "NULL");
	BANK_ONLY(PrintRadioDebug();)

	if(!m_PlayerRadioStation || (m_PlayerVehicleRadioState == PLAYER_RADIO_OFF && m_MobilePhoneRadioState == PLAYER_RADIO_OFF))
	{		
		if(station && !station->IsFrozen())
		{
			PlayStationSelectSound();
		}
		if(m_PlayerVehicle && m_PlayerVehicle->GetVehicleAudioEntity()->GetHasNormalRadio() && m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioEnabled())
		{
			SetPlayerVehicleRadioState(PLAYER_RADIO_STARTING);
			m_TimeEnteringStartingState = GetCurrentTimeMs();
			m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioOffState(false);
			m_LastPlayerVehicle = m_PlayerVehicle;
		}
		else if(IsMobileRadioNormallyPermitted() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::MobileRadioInGame))
		{	
			SetMobileRadioState(PLAYER_RADIO_STARTING);
			m_TimeEnteringStartingState = GetCurrentTimeMs();
			sm_IsMobilePhoneRadioActive = true;
		}

		if(m_PlayerRadioStation)
		{
			m_RequestedRadioStation = station;
			m_PlayerRadioStation = NULL;
		}
		else
		{
			m_PlayerRadioStation = station;
		}		
	}
	else
	{
		if(m_PlayerVehicleRadioState == PLAYER_RADIO_OFF)
		{
			// Apply mobile phone radio station to the player vehicle, so that you can switch the vehicle radio on/retune in the pause menu
			if(m_PlayerVehicle && m_PlayerVehicle->GetVehicleAudioEntity()->GetHasNormalRadio() && m_PlayerVehicle->GetVehicleAudioEntity()->IsRadioEnabled())
			{
				m_PlayerVehicle->GetVehicleAudioEntity()->SetRadioStation(station, false);
			}
		}
		m_RequestedRadioStation = station;
	}

	CancelFade();
}

void audRadioAudioEntity::ToggleMobilePhoneRadioState(void)
{
	g_RadioAudioEntity.SetMobilePhoneRadioState(!sm_IsMobilePhoneRadioActive);
}

void audRadioAudioEntity::SetMobilePhoneRadioState(bool isActive)
{
	if(sm_IsMobilePhoneRadioActive != isActive)
	{
		if(sm_PendingPlayerRadioStationRetunes == 0)
		{
			//Only allow the state of the mobile phone radio to be altered outside of retuning.
			sm_IsMobilePhoneRadioActive = isActive;
			naDisplayf("Switching %s mobile radio", isActive? "on" : "off");
		}
		else
		{
			naDisplayf("Attempting to switch %s mobile radio but we have %d retunes pending", isActive? "on" : "off", sm_PendingPlayerRadioStationRetunes);
		}
	}	
}

void audRadioAudioEntity::PlayRadioOnOffSound(bool bModulateVolume)
{
	if(!m_SkipOnOffSound)
	{
		audSoundInitParams initParams;
		initParams.TimerId = 2;
		if( bModulateVolume )
		{
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(m_PlayerVehicleInsideFactor);
		}
		initParams.Volume += GetVolumeOffset();
		CreateAndPlaySound(m_RadioSounds.Find(ATSTRINGHASH("OnOff", 0x73C5AA9)), &initParams);
	}
	m_SkipOnOffSound = false;
}

void audRadioAudioEntity::PlayWheelShowSound()
{
	audSoundInitParams initParams;
	initParams.TimerId = 2;	
	CreateAndPlaySound(m_RadioSounds.Find(ATSTRINGHASH("Show_Wheel", 0xBE07C731)), &initParams);
}

void audRadioAudioEntity::PlayWheelHideSound()
{
	audSoundInitParams initParams;
	initParams.TimerId = 2;
	CreateAndPlaySound(m_RadioSounds.Find(ATSTRINGHASH("Hide_Wheel", 0x1EDF5668)), &initParams);
}

void audRadioAudioEntity::PlayStationSelectSound(bool bModulateVolume)
{
	if (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::SuppressRadioSwitchBeep) && !camInterface::IsFadedOut())
	{
		audSoundInitParams initParams;
		initParams.TimerId = 2;
		if (bModulateVolume)
		{
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(m_PlayerVehicleInsideFactor);
		}

		audDisplayf("Triggering change radio station sound");
		CreateAndPlaySound(m_RadioSounds.Find(ATSTRINGHASH("Change_Station", 0x37AD47AB)), &initParams);
	}
}

extern u32 g_IceVanTuneTextId;
u32 audRadioAudioEntity::GetAudibleTrackTextId() const
{
    // Please implement through modelinfo or vehicle flags ... MI currently not in metadata
	//if(FindPlayerVehicle() && FindPlayerVehicle()->GetModelIndex() == MI_CAR_ICEVAN)
	//{
	//	if(FindPlayerVehicle()->GetVehicleAudioEntity()->IsSirenOn())
	//	{
	//		naDisplayf("Zit! icecream tune: %u", g_IceVanTuneTextId);
	//		return g_IceVanTuneTextId;
	//	}
	//}

	const audRadioTrack *track = FindAudibleTrack(10.f);
	if(track && (track->GetCategory() == RADIO_TRACK_CAT_MUSIC || track->GetCategory() == RADIO_TRACK_CAT_TAKEOVER_MUSIC))
	{
		return track->GetTextId();
	}
	enum {kUnknownTrackTextId = 1};
	return kUnknownTrackTextId;
}

const audRadioStation* audRadioAudioEntity::FindAudibleStation(const float searchRadius) const
{
	const audRadioStation* audibleStation = NULL;

	if(m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING || m_MobilePhoneRadioState == PLAYER_RADIO_PLAYING)
	{
		audibleStation = m_PlayerRadioStation;
	}
	else
	{
		// search for nearby emitters
		u32 nearestStation = ~0U;
		f32 nearestDist2 = LARGE_FLOAT;
		
		// Fix for B*4731228 - GET_NEXT_AUDIBLE_BEAT not working on hidden nightclub stations. This should probably be the same regardless of MP/SP but 
		// just being overly cautious and restricting the change to MP only
		const u32 numStationsToCheck = CNetwork::IsGameInProgress() ? audRadioStation::GetNumTotalStations() : audRadioStation::GetNumUnlockedStations();

		for(u32 i = 0; i < numStationsToCheck; i++)
		{
			audRadioSlot *slot = audRadioSlot::FindSlotByStation(audRadioStation::GetStation(i));
			if(slot)
			{				
				f32 thisDist2 = slot->ComputeDistanceSqToNearestEmitter();
				if(thisDist2 < nearestDist2)
				{
					nearestDist2 = thisDist2;
					nearestStation = i;
				}
			}
		}
		if(nearestDist2 < (searchRadius*searchRadius))
		{
			audibleStation = audRadioStation::GetStation(nearestStation);
		}
	}

	return audibleStation;
}

const audRadioStation* audRadioAudioEntity::FindAudibleStation(CInteriorInst* pIntInst, s32 roomIdx) const
{
	// search for nearby emitters
	u32 nearestStation = ~0U;
	f32 nearestDist2 = LARGE_FLOAT;

	// Fix for B*4731228 - GET_NEXT_AUDIBLE_BEAT not working on hidden nightclub stations. This should probably be the same regardless of MP/SP but 
	// just being overly cautious and restricting the change to MP only
	const u32 numStationsToCheck = CNetwork::IsGameInProgress() ? audRadioStation::GetNumTotalStations() : audRadioStation::GetNumUnlockedStations();

	for (u32 i = 0; i < numStationsToCheck; i++)
	{
		audRadioSlot *slot = audRadioSlot::FindSlotByStation(audRadioStation::GetStation(i));
		if (slot)
		{
			f32 thisDist2 = slot->ComputeDistanceSqToNearestEmitter(pIntInst, roomIdx);
			if (thisDist2 < nearestDist2)
			{
				nearestDist2 = thisDist2;
				nearestStation = i;
			}
		}
	}
	
	return audRadioStation::GetStation(nearestStation);
}

const audRadioTrack *audRadioAudioEntity::FindAudibleTrack(const float searchRadius) const
{
	const audRadioStation* audibleStation = FindAudibleStation(searchRadius);

	if (audibleStation)
	{
		const audRadioTrack* audibleTrack = &(audibleStation->GetCurrentTrack());

		if (audibleTrack->GetPlayTime() == -1 && audibleStation->GetNextTrack().GetPlayTime() >= 0)
		{
			audibleTrack = &(audibleStation->GetNextTrack());
		}
		
		return audibleTrack;
	}

	return NULL;
}

bool audRadioAudioEntity::GetNextAudibleBeat(float &timeS, float &bpm, s32 &beatNum) const
{
	const audRadioTrack *track = FindAudibleTrack(25.f);
	if(track)
	{
		return track->GetNextBeat(timeS, bpm, beatNum);
	}
	return false;
}

u32 audRadioAudioEntity::GetPlayingTrackTextIdByStationIndex(const u32 index, bool &WIN32PC_ONLY(isUserTrack)) const
{
	const audRadioStation *playerStation = audRadioStation::GetStation(index);
	if(playerStation)
	{
		const audRadioTrack &track = playerStation->GetCurrentTrack();
		if(track.GetCategory() == RADIO_TRACK_CAT_MUSIC || track.GetCategory() == RADIO_TRACK_CAT_TAKEOVER_MUSIC)
		{
			WIN32PC_ONLY(isUserTrack = track.IsUserTrack());
			return track.GetTextId();
		}
	}
	return 1;
}

u32 audRadioAudioEntity::GetPlayingTrackPlayTimeByStationIndex(const u32 index, bool &WIN32PC_ONLY(isUserTrack)) const
{
	const audRadioStation *playerStation = audRadioStation::GetStation(index);
	if(playerStation)
	{
		const audRadioTrack &track = playerStation->GetCurrentTrack();
		if(track.GetCategory() == RADIO_TRACK_CAT_MUSIC || track.GetCategory() == RADIO_TRACK_CAT_TAKEOVER_MUSIC)
		{
			WIN32PC_ONLY(isUserTrack = track.IsUserTrack());
			return track.GetPlayTime();
		}
	}
	return ~0u;
}

u32 audRadioAudioEntity::GetCurrentlyPlayingTrackTextId() const
{
	const audRadioStation *playerStation = GetPlayerRadioStationPendingRetune();
	if(playerStation)
	{
		const audRadioTrack &track = playerStation->GetCurrentTrack();
		if(track.GetCategory() == RADIO_TRACK_CAT_MUSIC || track.GetCategory() == RADIO_TRACK_CAT_MUSIC)
		{
			return track.GetTextId();
		}
	}
	return 1;
}

#if !__FINAL
void audRadioAudioEntity::SkipForward(void)
{
	sm_ShouldSkipForward = true;
}

void audRadioAudioEntity::SkipForwardToTransition(void)
{
	if (const audRadioStation* radioStation = g_RadioAudioEntity.FindAudibleStation(25.f))
	{
		audRadioSlot* slot = audRadioSlot::FindSlotByStation(radioStation);

		if (slot)
		{
			slot->SkipForwardToTransition(g_RadioDebugSkipToTransitionTimeMs);
		}
	}
}
#endif // !__FINAL

bool audRadioAudioEntity::IsInControlOfRadio(void) const
{
	return true; // local player is always in control of radio for time being - radio syncing will be added later
}

bool audRadioAudioEntity::GetCachedNextAudibleBeat(float &timeS, float &bpm, s32 &beatNum) const 
{ 
	timeS = m_CachedTimeTillNextAudibleBeat; 
	bpm = m_CachedAudibleBeatBPM; 
	beatNum = m_CachedAudibleBeatNum; 
	return m_CachedAudibleBeatValid;
}

void audRadioAudioEntity::StartEndCredits()
{
	audRadioStation::StartEndCredits();	
	m_AreRetunesMuted = true;
}

void audRadioAudioEntity::StopEndCredits()
{
	audRadioStation::StopEndCredits();
	m_AreRetunesMuted = false;
}

void audRadioAudioEntity::TriggerRetuneBurst()
{
	if(!IsInFrontendMode() && !m_AreRetunesMuted)
	{
		audSoundInitParams initParams;
		initParams.TimerId = 2;

		g_RadioAudioEntity.CreateAndPlaySound(m_RadioSounds.Find(ATSTRINGHASH("RADIO_RETUNE_MONO_ONE_SHOT", 0xC2F6E164)), &initParams);
	}
}

void audRadioAudioEntity::UpdateRetuneSounds()
{
	if(!m_AreRetunesMuted)
	{
		audRadioStation::UpdateRetuneSounds();
	}
	else
	{
		audRadioStation::StopRetuneSounds();
	}
}

void audRadioAudioEntity::FindFavouriteStations(u32 &mostFavourite, u32 &leastFavourite) const
{
	mostFavourite = leastFavourite = 0;
	float longestTime = 60.f; // User must have listened to the station for at least a minute before it can be their favourite
	float shortestTime = 0.f;

	for(u32 i = 0; i < audRadioStation::GetNumUnlockedStations(); i++)
	{
		const audRadioStation *station = audRadioStation::GetStation(i);
		if(station->GetListenTimer() >= longestTime)
		{
			longestTime = station->GetListenTimer();
			mostFavourite = station->GetNameHash();
		}
		else if(station->GetListenTimer() <= shortestTime)
		{
			shortestTime = station->GetListenTimer();
			leastFavourite = station->GetNameHash();
		}
	}

	// Don't return a valid least favourite station if the user hasn't listened to one station for the minimum time
	if(mostFavourite == 0)
	{
		leastFavourite = 0;
	}
}

u32 audRadioAudioEntity::GetCurrentTimeMs()
{
	return g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(2);
}

#if __BANK
void audRadioAudioEntity::RAGUpdateDLCBattleUnlockableTracks()
{
	UpdateDLCBattleUnlockableTracks(true);
}
#endif

#if HEIST3_HIDDEN_RADIO_ENABLED
u32* audRadioAudioEntity::GetEncryptionKeyForBank(u32 bankId)
{
	if (bankId == audRadioStation::sm_Heist3HiddenRadioBankId)
	{
		bool tuneableAvailable = true;
		static u32 tuneableEncryptionKey[4];

#if HEIST3_HIDDEN_RADIO_OVERRIDE_TUNEABLE
		tuneableEncryptionKey[0] = HEIST3_HIDDEN_RADIO_KEY_A;
		tuneableEncryptionKey[1] = HEIST3_HIDDEN_RADIO_KEY_B;
		tuneableEncryptionKey[2] = HEIST3_HIDDEN_RADIO_KEY_C;
		tuneableEncryptionKey[3] = HEIST3_HIDDEN_RADIO_KEY_D;
#else
		tuneableAvailable &= Tunables::GetInstance().Access(BASE_GLOBALS_HASH, HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_A, tuneableEncryptionKey[0]);
		tuneableAvailable &= Tunables::GetInstance().Access(BASE_GLOBALS_HASH, HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_B, tuneableEncryptionKey[1]);
		tuneableAvailable &= Tunables::GetInstance().Access(BASE_GLOBALS_HASH, HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_C, tuneableEncryptionKey[2]);
		tuneableAvailable &= Tunables::GetInstance().Access(BASE_GLOBALS_HASH, HEIST3_HIDDEN_RADIO_KEY_TUNEABLE_HASH_D, tuneableEncryptionKey[3]);
#endif

		if (tuneableAvailable)
		{
			audWaveSlot::TeaStirEncryptionKey(&(tuneableEncryptionKey[0]));
			return &(tuneableEncryptionKey[0]);
		}
	}

	return NULL;
}
#endif

void audRadioAudioEntity::UpdateDLCBattleUnlockableTracks(bool allowReprioritization)
{
	audRadioStation* dlcBattleStation = audRadioStation::FindStation(ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_RADIO", 0xF8BEAA16));

	if (dlcBattleStation)
	{
		audDisplayf("--- DLC_Battle Unlockable Music ---");

		if (allowReprioritization && audRadioStation::HasSyncedUnlockableDJStation())
		{
			audDisplayf("Ignoring reprioritization request as we have already synced the station state from a remote machine");
			allowReprioritization = false;
		}

		bool solomunEnabled = false;
		bool taleOfUsEnabled = false;
		bool dixonEnabled = false;
		bool blackMadonnaEnabled = false;
		bool priorizeMix1 = false;
		bool priorizeMix2 = false;
		bool priorizeMix3 = false;
		bool priorizeMix4 = false;

		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_SOL", 0x6268383A), solomunEnabled))
			audDisplayf("Found ENABLE_LSUR_SOL tuneable - state %s", solomunEnabled ? "enabled" : "disabled");
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_TOS", 0x1F91894), taleOfUsEnabled))
			audDisplayf("Found ENABLE_LSUR_TOS tuneable - state %s", taleOfUsEnabled ? "enabled" : "disabled");
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_DIX", 0x138F159C), dixonEnabled))
			audDisplayf("Found ENABLE_LSUR_DIX tuneable - state %s", dixonEnabled ? "enabled" : "disabled");
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("ENABLE_LSUR_BM", 0x2E637DC9), blackMadonnaEnabled))
			audDisplayf("Found ENABLE_LSUR_BM tuneable - state %s", blackMadonnaEnabled ? "enabled" : "disabled");

		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("PRIORITISE_MIX_1", 0x17BF3CB6), priorizeMix1))
			audDisplayf("Found PRIORITISE_MIX_1 tuneable - state %s", priorizeMix1 ? "enabled" : "disabled");
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("PRIORITISE_MIX_2", 0x5809839), priorizeMix2))
			audDisplayf("Found PRIORITISE_MIX_2 tuneable - state %s", priorizeMix2 ? "enabled" : "disabled");
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("PRIORITISE_MIX_3", 0x18743E24), priorizeMix3))
			audDisplayf("Found PRIORITISE_MIX_3 tuneable - state %s", priorizeMix3 ? "enabled" : "disabled");
		if (Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("PRIORITISE_MIX_4", 0x7199B6F), priorizeMix4))
			audDisplayf("Found PRIORITISE_MIX_4 tuneable - state %s", priorizeMix4 ? "enabled" : "disabled");

#if __BANK
		solomunEnabled |= g_ForceUnlockSolomun;
		taleOfUsEnabled |= g_ForceUnlockTaleOfUs;
		dixonEnabled |= g_ForceUnlockDixon;
		blackMadonnaEnabled |= g_ForceUnlockBlackMadonna;

		priorizeMix1 |= g_PrioritizeSolomun;
		priorizeMix2 |= g_PrioritizeTaleOfUs;
		priorizeMix3 |= g_PrioritizeDixon;
		priorizeMix4 |= g_PrioritizeBlackMadonna;
#endif

		s32 trackToPrioritize = -1;

		// Assuming that if multiple priorities are set, we probably want to use the latest
		if (blackMadonnaEnabled && priorizeMix4)
			trackToPrioritize = 4;
		else if (dixonEnabled && priorizeMix3)
			trackToPrioritize = 3;
		else if (taleOfUsEnabled && priorizeMix2)
			trackToPrioritize = 2;
		else if (solomunEnabled && priorizeMix1)
			trackToPrioritize = 1;
		
		audDisplayf("Solomun (ENABLE_LSUR_SOL) %s", solomunEnabled ? "enabled" : "disabled");
		audDisplayf("Tale of Us (ENABLE_LSUR_TOS) %s", taleOfUsEnabled ? "enabled" : "disabled");
		audDisplayf("Dixon (ENABLE_LSUR_DIX) %s", dixonEnabled ? "enabled" : "disabled");
		audDisplayf("The Black Madonna (ENABLE_LSUR_BM) %s", blackMadonnaEnabled ? "enabled" : "disabled");	

		audDisplayf("PRIORITISE_MIX_1 %s", priorizeMix1 ? "enabled" : "disabled");
		audDisplayf("PRIORITISE_MIX_2 %s", priorizeMix2 ? "enabled" : "disabled");
		audDisplayf("PRIORITISE_MIX_3 %s", priorizeMix3 ? "enabled" : "disabled");
		audDisplayf("PRIORITISE_MIX_4 %s", priorizeMix4 ? "enabled" : "disabled");
		
		// Always unlocking at least one track list to prevent asserts (station will still be hidden in this situation)
		/*if (solomunEnabled)*/ { dlcBattleStation->UnlockTrackList("BATTLE_MIX1_RADIO"); }
		//else { dlcBattleStation->LockTrackList("BATTLE_MIX1_RADIO"); }

		if (taleOfUsEnabled) { dlcBattleStation->UnlockTrackList("BATTLE_MIX2_RADIO"); }
		else { dlcBattleStation->LockTrackList("BATTLE_MIX2_RADIO"); }

		if (dixonEnabled) { dlcBattleStation->UnlockTrackList("BATTLE_MIX3_RADIO"); }
		else { dlcBattleStation->LockTrackList("BATTLE_MIX3_RADIO"); }

		if (blackMadonnaEnabled) { dlcBattleStation->UnlockTrackList("BATTLE_MIX4_RADIO"); }
		else { dlcBattleStation->LockTrackList("BATTLE_MIX4_RADIO"); }

		if (solomunEnabled || taleOfUsEnabled || dixonEnabled || blackMadonnaEnabled)
		{
			dlcBattleStation->SetLocked(false);
			audDisplayf("Tracks available - unlocking station");

			if (allowReprioritization)
			{
				audDisplayf("Prioritizing track %d", trackToPrioritize);
				RadioStationTrackList* forcedTrackList = NULL;

				if (trackToPrioritize == 4)
					forcedTrackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(ATSTRINGHASH("BATTLE_MIX4_RADIO", 0x25FD177D));
				else if (trackToPrioritize == 3)
					forcedTrackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(ATSTRINGHASH("BATTLE_MIX3_RADIO", 0xEC028F91));
				else if (trackToPrioritize == 2)
					forcedTrackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(ATSTRINGHASH("BATTLE_MIX2_RADIO", 0x2E8F5458));
				else if (trackToPrioritize == 1)
					forcedTrackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(ATSTRINGHASH("BATTLE_MIX1_RADIO", 0xDA1AA248));

				const s32 numTracks = dlcBattleStation->ComputeNumTracksAvailable(RADIO_TRACK_CAT_MUSIC);
				const audRadioStationTrackListCollection *trackLists = dlcBattleStation->FindTrackListsForCategory(RADIO_TRACK_CAT_MUSIC);
				dlcBattleStation->Freeze();

				if (forcedTrackList)
				{
					dlcBattleStation->ForceTrack(forcedTrackList->Track[0].SoundRef, 0);

					for (u32 trackIndex = 0; trackIndex < numTracks; trackIndex++)
					{
						if (trackLists->GetTrack(trackIndex)->SoundRef == forcedTrackList->Track[0].SoundRef)
						{
							audRadioStationHistory *history = const_cast<audRadioStationHistory*>(dlcBattleStation->FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
							(*history)[0] = (trackIndex + 1) % numTracks;
						}
					}
				}
				else // No forced track list, just do a pure randomization
				{
					u32 randomTrackIndex = audEngineUtil::GetRandomNumberInRange(0, numTracks - 1);
					const u32 soundRef = trackLists->GetTrack(randomTrackIndex)->SoundRef;
					dlcBattleStation->ForceTrack(soundRef, audEngineUtil::GetRandomNumberInRange(0, 1000000));
					audRadioStationHistory *history = const_cast<audRadioStationHistory*>(dlcBattleStation->FindHistoryForCategory(RADIO_TRACK_CAT_MUSIC));
					(*history)[0] = (randomTrackIndex + 1) % numTracks;
				}

				dlcBattleStation->Unfreeze();
			}
			else
			{
				audDisplayf("Track prioritization disabled");
			}
		}
		else
		{
			dlcBattleStation->SetLocked(true);
			audDisplayf("No tracks unlocked - locking station");
		}

		audDisplayf("--- DLC_Battle Unlockable Music ---");
	}
}

#if __BANK

char g_ScriptStream[128]={0};
char g_ForcePlayerStationName[128]={0};
u32 g_ScriptStreamStartOffset = 0;

atArray<audStreamSlot*> g_DebugStreamSlots;

void audRadioAudioEntity::DebugAllocateAllStreamSlots()
{
	audStreamClientSettings settings;
	u32 priority = audStreamSlot::STREAM_PRIORITY_CUTSCENE;
	Assign(settings.priority, priority);
	
	for(u32 loop = 0; loop < g_NumStreamSlots; loop++)
	{
		audStreamSlot* streamSlot = audStreamSlot::AllocateSlot(&settings); 

		if(streamSlot)
		{
			g_DebugStreamSlots.PushAndGrow(streamSlot);
		}
	}
}

void audRadioAudioEntity::DebugFreeStreamSlots()
{
	for(u32 loop = 0; loop < g_DebugStreamSlots.GetCount(); loop++)
	{
		g_DebugStreamSlots[loop]->StopAndFree();
	}

	g_DebugStreamSlots.clear();
}

void audRadioAudioEntity::RetuneToRequestedStation(void)
{
	g_RadioAudioEntity.RetuneToStation(audRadioStation::FindStation(sm_RadioStationNames[sm_RequestedRadioStationIndex]));
}

void audRadioAudioEntity::DebugPrintTrackListInfo()
{
	if (audRadioStation* station = audRadioStation::FindStation(g_RadioStationCombo->GetString(sm_RequestedRadioStationIndex)))
	{
		for(u32 category = 0; category < TrackCats::NUM_RADIO_TRACK_CATS; category++)
		{
			if(audRadioStationTrackListCollection *trackListCollection = const_cast<audRadioStationTrackListCollection *>(station->FindTrackListsForCategory(category)))
			{
				for(u32 listIndex = 0; listIndex < trackListCollection->GetListCount(); listIndex++)
				{
					if(const RadioStationTrackList* trackList = trackListCollection->GetList(listIndex))
					{
						const char* trackListName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trackList->NameTableOffset);
						bool isLocked = trackListCollection->IsListLocked(listIndex);
						audDisplayf("%s: %s", trackListName, isLocked ? "Locked" : "Unlocked");
					}
				}
			}
		}
	}
}

void audRadioAudioEntity::DebugShutdownTracks()
{
	if (audRadioStation* station = audRadioStation::FindStation(g_RadioStationCombo->GetString(sm_RequestedRadioStationIndex)))
	{
		station->GetCurrentTrack().Shutdown();
		station->GetNextTrack().Shutdown();
	}
}

void audRadioAudioEntity::RetuneClubEmittersToRequestedStation(void)
{
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_Bogs", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_Entry_Stairs", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_garage", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_main_area_2", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_main_area", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_office", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_rear_L_corridor", true);
	g_EmitterAudioEntity.SetEmitterEnabled("SE_ba_dlc_int_01_Bogs", true);

	const char* stationName = sm_RadioStationNames[sm_RequestedRadioStationIndex];
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_Bogs", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_Entry_Stairs", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_garage", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_main_area_2", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_main_area", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_office", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_rear_L_corridor", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_ba_dlc_int_01_Bogs", stationName);

	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_Bogs", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_Entrance_Doorway", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_lobby", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_main_bar", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_main_front_01", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_main_front_02", stationName);
	g_EmitterAudioEntity.SetEmitterRadioStation("SE_h4_dlc_int_02_h4_main_room_cutscenes", stationName);
}

void PreloadStreamCB()
{
	g_ScriptAudioEntity.PreloadStreamInternal(g_ScriptStream, g_ScriptStreamStartOffset, NULL);
}

void PlayStreamCB()
{
	g_ScriptAudioEntity.PlayStreamFrontendInternal();
}

void PlayStreamFromPlayerCB()
{
	g_ScriptAudioEntity.PlayStreamFromEntityInternal(CGameWorld::FindLocalPlayer());
}

void StopStreamCB()
{
	g_ScriptAudioEntity.StopStream();
}
void KapowCB()
{
	if(g_RadioAudioEntity.GetAudibleTrackTextId()==1)
	{
		naDisplayf("Kapow: Unknown");
	}
}
void UnpauseCB()
{
	g_RadioAudioEntity.Unpause();
}

void StartEndCreditsCB()
{
	g_RadioAudioEntity.StartEndCredits();
}

void StopEndCreditsCB()
{
	g_RadioAudioEntity.StopEndCredits();
}

void FreezeStationCB()
{
	audRadioStation::FindStation(g_ForcePlayerStationName)->Freeze();
	// Match the script API: reset to auto-unfreeze on Freeze()
	g_RadioAudioEntity.SetAutoUnfreezeForPlayer(true);
}

void UnfreezeStationCB()
{
	audRadioStation::FindStation(g_ForcePlayerStationName)->Unfreeze();
}

void ForceNextPlayerStation()
{
	audRadioStation *station = audRadioStation::FindStation(g_ForcePlayerStationName);
	if(station)
	{
		g_RadioAudioEntity.ForcePlayerRadioStation(station->GetStationIndex());
	}
	else
	{
		audErrorf("Failed to find station \'%s\'", g_ForcePlayerStationName);
	}	
}

#if __WIN32PC // user music
void NextTrackCB()
{
	audRadioStation::FindStation("RADIO_19_USER")->NextTrack();
}

void PrevTrackCB()
{
	audRadioStation::FindStation("RADIO_19_USER")->PrevTrack();
}
#endif

#if GTA_REPLAY
void PreviewTrackCB()
{
	g_RadioAudioEntity.StartReplayTrackPreview(atStringHash(g_PreviewTrackName));
}

void StopPreviewTrackCB()
{
	g_RadioAudioEntity.StopReplayTrackPreview();
}

#endif

extern s32 g_TrackReleaseTime;
extern bank_float g_CutsceneRadioLeakage;
void audRadioAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Radio", false);
		bank.PushGroup("DLC Battle Unlockable Tracks", false);
			bank.AddButton("Update Unlockable Track Status", datCallback(CFA(RAGUpdateDLCBattleUnlockableTracks)));
			bank.AddToggle("Force Unlock - Solomun", &g_ForceUnlockSolomun);
			bank.AddToggle("Force Unlock - Tale Of Us", &g_ForceUnlockTaleOfUs);
			bank.AddToggle("Force Unlock - Dixon", &g_ForceUnlockDixon);
			bank.AddToggle("Force Unlock - The Black Madonna", &g_ForceUnlockBlackMadonna);
			bank.AddToggle("Prioritize - Solomun", &g_PrioritizeSolomun);
			bank.AddToggle("Prioritize - Tale Of Us", &g_PrioritizeTaleOfUs);
			bank.AddToggle("Prioritize - Dixon", &g_PrioritizeDixon);
			bank.AddToggle("Prioritize - The Black Madonna", &g_PrioritizeBlackMadonna);
		bank.PopGroup();	

		bank.AddToggle("MobilePhoneRadioAvailable?", &sm_IsMobilePhoneRadioAvailable);
		bank.AddText("ForcePlayerStationName", &g_ForcePlayerStationName[0], sizeof(g_ForcePlayerStationName));
		bank.AddButton("ForceNextPlayerStation", datCallback(CFA(ForceNextPlayerStation)));		
		bank.AddToggle("Auto-Unfreeze", &g_RadioAudioEntity.m_AutoUnfreezeForPlayer);
		bank.AddButton("Freeze", CFA(FreezeStationCB));
		bank.AddButton("Unfreeze", CFA(UnfreezeStationCB));
		bank.AddSlider("RadioPositionedToStereoTime", &g_RadioPositionedToStereoTime, 0.f, 10000.f, 0.01f);
		bank.AddToggle("Enable Positioned Player Radio", &g_PositionedPlayerVehicleRadioEnabled);		

#if __WIN32PC // user music
		bank.PushGroup("User Music", false);
			bank.AddToggle("Show User Music Data", &g_RadioAudioEntity.sm_DebugUserMusic);
			bank.AddButton("NextTrack", CFA(NextTrackCB));
			bank.AddButton("PrevTrack", CFA(PrevTrackCB));
		bank.PopGroup();
#endif
#if GTA_REPLAY
		bank.PushGroup("Replay", false);
		bank.AddText("Preview Track", &g_PreviewTrackName[0], sizeof(g_PreviewTrackName));
		bank.AddButton("Preview Track", CFA(PreviewTrackCB));
		bank.AddButton("Stop Track", CFA(StopPreviewTrackCB));
		bank.PopGroup();
#endif
		bank.AddToggle("Perform retune test", &sm_ShouldPerformRadioRetuneTest);
		bank.AddSlider("Retune period", &sm_RetunePeriodMs, 0, 300000, 1000);
		bank.AddSlider("SkipForwardTime", &g_RadioDebugSkipForwardTimeMs, 0, 999999, 1);
		bank.AddButton("Skip player station forward", datCallback(CFA(SkipForward)));		
		bank.AddButton("Skip all stations forward", datCallback(CFA(SkipStationsForward)));
		bank.AddSlider("TransitionTime", &g_RadioDebugSkipToTransitionTimeMs, 0, 999999, 1);
		bank.AddButton("Skip audible station to transition", datCallback(CFA(SkipForwardToTransition)));
		bank.AddButton("Tune Up", datCallback(CFA(RetuneRadioUp)));
		bank.AddButton("Tune Down", datCallback(CFA(RetuneRadioDown)));
		g_RadioStationCombo = bank.AddCombo("Station", &sm_RequestedRadioStationIndex, g_MaxRadioStations, sm_RadioStationNames);
		bank.AddButton("Lock Station", datCallback(CFA(DebugLockStation)));
		bank.AddButton("Unlock Station", datCallback(CFA(DebugUnlockStation)));
		bank.AddButton("Hide Station", datCallback(CFA(DebugHideStation)));
		bank.AddButton("UnHide Station", datCallback(CFA(DebugUnHideStation)));
		bank.AddButton("Retune to station", datCallback(CFA1(RetuneToRequestedStation)));
		bank.AddButton("Print Station Track List Info", datCallback(CFA1(DebugPrintTrackListInfo)));
		bank.AddButton("Shutdown Tracks", datCallback(CFA1(DebugShutdownTracks)));
		bank.AddButton("Retune Club Emitters to station", datCallback(CFA1(RetuneClubEmittersToRequestedStation)));
		bank.AddSlider("VolumeControlSmoothRate", &g_RadioVolumeControlSmoothRate, 0.f,100.f,0.1f);
		bank.AddSlider("VolumeControlChange", &g_RadioVolumeControlChange, 0.0f, 12.0f, 0.5f);
		bank.AddSlider("VolumeHoldTime", &g_RadioVolumeHoldTime, 0, 300000, 1000);
		bank.AddSlider("VolumeHoldTime", &g_RadioVolumeRampTime, 0, 300000, 1000);
		bank.AddButton("Toggle mobile phone radio on/off", datCallback(CFA(ToggleMobilePhoneRadioState)));
		bank.AddSlider("Mobile phone radio volume offset (dB)", &g_MobilePhoneRadioVolumeOffset, 0.0f, 12.0f, 0.5f);

		bank.AddToggle("DrawDebug", &g_DrawRadioDebug);
		bank.AddToggle("DrawSlotDebug", &g_DrawRadioSlotDebug);
		bank.AddToggle("Draw Station Debug", &g_DrawRadioStationDebug);
		bank.AddText("Draw Station Name Filter", &g_StationDebugNameFilter[0], sizeof(g_StationDebugNameFilter));		
		bank.AddToggle("Draw Takeover Station 1", &g_DrawTakeoverStation1);		
		bank.AddToggle("Debug Audible Beat", &g_DrawAudibleTrack);
		bank.AddSlider("Debug Draw Scroll", &g_StationDebugDrawYScroll, 0.0f, 15.0f, 0.1f);
		bank.AddToggle("Simulate Stream Slot Starvation", &g_DebugSimulateStreamSlotStarvation);
		bank.AddButton("Allocate All Stream Slots", datCallback(CFA(DebugAllocateAllStreamSlots)));
		bank.AddButton("Release Allocated Stream Slots", datCallback(CFA(DebugFreeStreamSlots)));
		audRadioStation::AddWidgets(bank);

		bank.AddText("ScriptStream", &g_ScriptStream[0], sizeof(g_ScriptStream));
		bank.AddSlider("StartOffset", &g_ScriptStreamStartOffset, 0, 1000000, 1);
		bank.AddButton("PreloadStream", CFA(PreloadStreamCB));
		bank.AddButton("PlayStream", CFA(PlayStreamCB));
		bank.AddButton("PlayStreamFromPlayer", CFA(PlayStreamFromPlayerCB));
		bank.AddButton("StopStream", CFA(StopStreamCB));
		bank.AddSlider("TrackReleaseTime", &g_TrackReleaseTime, -1, 5000, 1);

		bank.AddButton("Kapow!", CFA(KapowCB));
		bank.AddButton("Unpause", CFA(UnpauseCB));
		bank.AddButton("RollEndCreditsMusic", CFA(StartEndCreditsCB));
		bank.AddButton("StopEndCreditsMusic", CFA(StopEndCreditsCB));
		bank.AddSlider("CutsceneRadioLeakage", &g_CutsceneRadioLeakage, 0.f, 1.f, 0.01f);

	bank.PopGroup();
}
#endif
#endif // NA_RADIO_ENABLED
