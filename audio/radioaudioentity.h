// 
// audio/radioaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOAUDIOENTITY_H
#define AUD_RADIOAUDIOENTITY_H

#include "audiodefines.h"

#if NA_RADIO_ENABLED


#include "radiostation.h"
#include "radioemitter.h"
#include "gameobjects.h"
#include "audioengine/tracker.h"
#include "audioengine/requestedsettings.h"
#include "audioengine/categorymanager.h"
#include "audioengine/engineutil.h"
#include "audioengine/soundset.h"
#include "audiohardware/driverutil.h"
#include "audiohardware/streamingwaveslot.h"
#include "audiosoundtypes/soundcontrol.h"

#include "audioentity.h"

#include "bank/bkmgr.h"
#include "vehicles/vehicle.h"
#include "system/param.h"

#if GTA_REPLAY
#define REPLAY_PREVIEW_MIN_LENGTH		20000
#define REPLAY_PREVIEW_MAX_LENGTH		50000
#define REPLAY_PREVIEW_DEFAULT_LENGTH	30000
#define REPLAY_PREVIEW_FADE_LENGTH		1000
#define REPLAY_PREVIEW_MUSIC_VOLUME		0.8f
#endif

class audRadioAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audRadioAudioEntity);

	enum audPlayerRadioStates
	{
		PLAYER_RADIO_OFF,
		PLAYER_RADIO_STARTING,
		PLAYER_RADIO_PLAYING,
		PLAYER_RADIO_FROZEN,
		PLAYER_RADIO_STOPPING,
		PLAYER_RADIO_RETUNING
	};

	audRadioAudioEntity();

	static void InitClass(void);
	static void DLCInitClass(void);
	static void ShutdownClass(void);

	void Init(void);
	virtual void Shutdown(void);
	virtual void PreUpdateService(u32 timeInMs);
	void GameUpdate(void);

#if GTA_REPLAY
	void RequestUpdateReplayMusic() { m_ShouldUpdateReplayMusic = true; }
	void RequestUpdateMusicPreview() { m_ShouldUpdateReplayPreview = true; }
	u32 GetReplayMusicPlaybackTimeMs();

	void RequestReplayMusicTrack(u32 soundHash, s32 musicTrackID, u32 startOffsetMS, u32 durationMs, float fadeOutScalar);
	bool IsReplayMusicTrackPrepared();

	void StartReplayTrackPreview(u32 soundHash, u32 startOffSetMS = 0, u32 trackDuration = REPLAY_PREVIEW_DEFAULT_LENGTH, bool jumpToRockout = true, bool fadeIn = true);
	void StopReplayTrackPreview();
	bool IsReplayMusicPreviewPlaying()	{ return (m_ReplayMusicStartTime > 0); }
	void ParseMarkers();

	f32 GetNextReplayBeat(u32 soundHash, f32 startOffsetMS);
	f32 GetPrevReplayBeat(u32 soundHash, f32 startOffsetMS);
	f32 GetReplayBeat(u32 soundHash, s32 direction, f32 startOffsetMS);
	bool HasBeatMarkers(u32 soundHash);
	bool HasBeatMarkersInRange(u32 soundHash, u32 startMS, u32 endMS);
#endif // GTA_REPLAY

	const audRadioStation *RequestVehicleRadio(CVehicle *vehicle);
	void StopVehicleRadio(CVehicle *vehicle, bool clearStation = true);

	bool RequestStaticRadio(audStaticRadioEmitter *emitter, u32 stationNameHash, s32 requestedPCMChannel = -1);
	void StopStaticRadio(audStaticRadioEmitter *emitter);
	bool IsStaticRadioEmitterActive(audStaticRadioEmitter *emitter);
	bool ShouldBlockPauseMenuRetunes() const;

	static void SkipStationsForward(void);

	bool IsTimeToRetune(u32 timeInMs);
	u8 GetPendingPlayerRadioStationRetunes(void)
	{
		return sm_PendingPlayerRadioStationRetunes;
	}
	void ClearPendingPlayerRadioStationRetunes(void)
	{
		sm_PendingPlayerRadioStationRetunes = 0;
		sm_ForceVehicleExplicitRetune = false;
	}

	void OnRetuneActioned(void);

	bool IsPlayerInVehicle(void)
	{
		return m_IsPlayerInVehicleForRadio;
	}
	f32 GetPlayerVehicleInsideFactor(void)
	{
		return m_PlayerVehicleInsideFactor;
	}
	CVehicle *GetLastPlayerVehicle(void)
	{
		return m_LastPlayerVehicle;
	}
	CVehicle *GetPlayerVehicle(void)
	{
		return m_PlayerVehicle;
	}
	const audRadioStation *GetPlayerRadioStation()
	{
		return m_PlayerRadioStation;
	}

	u8 IsPlayerRadioActive() const
	{
		return (m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING || m_MobilePhoneRadioState == PLAYER_RADIO_PLAYING);
	}

	void SetPlayerRadioStation(const audRadioStation *station)
	{
		m_PlayerRadioStation = station;
	}

	bool StopEmittersForPlayerVehicle() const;

	const char *GetPlayerRadioStationNamePendingRetune() const;
	const audRadioStation *GetPlayerRadioStationPendingRetune() const;

	const audRadioStation *GetLastPlayerRadioStation()
	{
		return m_LastPlayerRadioStation;
	}
	bool IsMobilePhoneRadioActive(void)
	{
		return sm_IsMobilePhoneRadioActive;
	}
	bool IsRetuning(void)
	{
		return ((m_PlayerVehicleRadioState == PLAYER_RADIO_RETUNING) ||
			    (m_MobilePhoneRadioState == PLAYER_RADIO_RETUNING));
	}

	bool IsStarting() const
	{
		return ((m_PlayerVehicleRadioState == PLAYER_RADIO_STARTING) ||
			(m_MobilePhoneRadioState == PLAYER_RADIO_STARTING));
	}

	bool IsVehicleRadioOn(void)
	{
		return ((m_PlayerVehicleRadioState == PLAYER_RADIO_PLAYING) || 
				(m_PlayerVehicleRadioState == PLAYER_RADIO_RETUNING));

	}

	bool WasPlayerInAVehicleForRadioLastFrame() const
	{
		return m_WasPlayerInVehicleForRadioLastFrame;
	}

	void SwitchOffRadio();
	void PlayRadioOnOffSound(bool bModulateVolume = true );
	void PlayStationSelectSound(bool bModulateVolume = true );
	void PlayWheelShowSound();
	void PlayWheelHideSound();

	bool IsInFrontendMode() const
	{
		return m_InFrontendMode;
	}

	bool IsRetuningVehicleRadio(void)
	{
		return (m_PlayerVehicleRadioState == PLAYER_RADIO_RETUNING);
	}

	f32 GetVolumeOffset(void) 
	{
		return m_VolumeOffset;
	}

	bool WasMobileRadioActiveOnEnteringFrontend(void)
	{
		return m_WasMobileRadioActiveOnEnteringFrontend;
	}

	void ForcePlayerRadioStation(const u32 stationId)
	{
		m_ForcePlayerStation = audRadioStation::GetStation(stationId);
	}

	void ForcePlayerRadioStation(const audRadioStation *station)
	{
		m_ForcePlayerStation = station;
	}

	u32 GetLastTimeVehicleRadioWasRetuned()
	{
		return m_LastTimeVehicleRadioWasRetuned;
	}
	bool WasVehicleRadioJustRetuned()
	{
		return m_VehicleRadioJustRetuned;
	}

	void FindFavouriteStations(u32 &mostFavourite, u32 &leastFavourite) const;

	void StartFadeIn(const float delayTime, const float fadeTime);
	void StartFadeOut(const float delayTime, const float fadeTime);

	void MuteNextOnOffSound() { m_SkipOnOffSound = true; }

	// PURPOSE
	//	Picks the most audible track and returns its text id; 0 for unknown
	u32 GetAudibleTrackTextId() const;
	// Gets the currently playing track id, as above, except only for the player's car/phone - to be used for tune-away info, not Zit!
	u32 GetCurrentlyPlayingTrackTextId() const;
	u32 GetPlayingTrackTextIdByStationIndex(const u32 index, bool &isUserTrack) const;
	u32 GetPlayingTrackPlayTimeByStationIndex(const u32 index, bool &isUserTrack) const;

	// PURPOSE
	//	Returns info on the next audible music beat, if available
	// SEE ALSO
	//	audRadioTrack::GetNextBeat
	bool GetCachedNextAudibleBeat(float &timeS, float &bpm, s32 &beatNum) const;

	bool IsInControlOfRadio(void) const;
	static void UpdateDLCBattleUnlockableTracks(bool allowReprioritization);
	BANK_ONLY(static void RAGUpdateDLCBattleUnlockableTracks();)

	void RetuneToStation(const char *stationName);
	void RetuneToStation(const u32 stationNameHash);
	void RetuneToStation(const audRadioStation *station);
	void SetMobilePhoneRadioState(bool isActive);
	bool IsMobileRadioNormallyPermitted() const;

	static void RetuneRadioUp(void);
	static void RetuneRadioDown(void);
	static void ToggleMobilePhoneRadioState(void);
	BANK_ONLY(static void DebugLockStation(void);)
	BANK_ONLY(static void DebugUnlockStation(void);)
	BANK_ONLY(static void DebugHideStation(void);)
	BANK_ONLY(static void DebugUnHideStation(void);)

#if HEIST3_HIDDEN_RADIO_ENABLED
	static u32* GetEncryptionKeyForBank(u32 bankId);
#endif

	void FrontendUpdate(const bool shouldPreviewRadio);

	void Pause();
	void Unpause();

	virtual bool IsUnpausable() const
	{
		return true;
	}

	void StartEndCredits();
	void StopEndCredits();

	void SetIgnorePlayerRadioShudown(bool ignore)
	{
		m_IgnorePlayerVehicleRadioShutdown = ignore;
	}

	void MuteRetunes()
	{
		m_AreRetunesMuted = true;
	}

	void UnmuteRetunes()
	{
		m_AreRetunesMuted = false;
	}
	
	void SetAutoUnfreezeForPlayer(const bool shouldUnfreeze)
	{
		m_AutoUnfreezeForPlayer = shouldUnfreeze;
		// Disable any active timer
		m_AutoUnfreezeForPlayerTimer = 0.f;
	}

	void SetAutoUnfreezeForPlayer(const float delayTime)
	{
		if(delayTime == 0.f)
		{
			m_AutoUnfreezeForPlayer = true;
		}
		m_AutoUnfreezeForPlayerTimer = delayTime;
	}

	void SetFrontendFadeTime(const float fadeTimeSeconds);

#if __BANK
	void DrawDebug();
	void DebugDrawStations();
	void PrintRadioDebug();

	static void PopulateRetuneWidgetStationNames();
	static void DebugAllocateAllStreamSlots();
	static void DebugFreeStreamSlots();
	static void RetuneToRequestedStation(void);
	static void RetuneClubEmittersToRequestedStation(void);
	static void DebugPrintTrackListInfo();
	static void AddWidgets(bkBank &bank);
	static void DebugShutdownTracks();
	

#if __WIN32PC
	static bool sm_DebugUserMusic;
#endif

#endif // __BANK

#if !__FINAL
	static void SkipForward(void);
	static void SkipForwardToTransition(void);

	static bool sm_ShouldMuteRadio;
#endif // !__FINAL

	bool IsRadioFadedOut() const
	{
		return m_FadeVolumeLinear <= g_SilenceVolumeLin;
	}

	const audSoundSet &GetRadioSounds() const { return m_RadioSounds; }

	bool IsPlayerInAVehicleWithARadio() const { return m_IsPlayerInAVehicleWithARadio; }
	void SetForcePlayerCharacterStation(const bool force) { m_ForcePlayerCharacterStations = force; }

	bool MPDriverHasTriggeredRadioChange() const { return m_bMPDriverHasTriggeredRadioChange; }
	void SetMPDriverTriggeredRadioChange() { m_bMPDriverHasTriggeredRadioChange =  true; }

#if GTA_REPLAY
	void PreUpdateReplayMusic(bool play = false);
	void StopReplayMusic(bool isPreview = false, bool noRelease = false);
	bool IsStartingReplayPreview()	{ return m_ShouldPlayReplayPreview; }
#endif // GTA_REPLAY

	const audRadioStation* FindAudibleStation(const float searchRadius) const;
	const audRadioStation* FindAudibleStation(CInteriorInst* pIntInst, s32 roomIdx) const;

private:
	// PURPOSE
	//	Returns info on the next audible music beat, if available
	// SEE ALSO
	//	audRadioTrack::GetNextBeat
	bool GetNextAudibleBeat(float &timeS, float &bpm, s32 &beatNum) const;

#if GTA_REPLAY
	void UpdateReplayMusic();
	void UpdateReplayMusicVolume(bool instant = false);
	void UpdateReplayTrackPreview();
#endif

	static u32 GetCurrentTimeMs();

	void UpdateFade();
	void CancelFade();

	void StartMobileRadio();
	void SetPlayerVehicleRadioState(u8 newState);	
	void SetMobileRadioState(u8 newState);	

	const audRadioTrack *FindAudibleTrack(const float searchRadius) const;

	u32 ChooseRandomRadioStationForGenre(const RadioGenre genre) const;
	void UpdatePlayerVehicleRadio(u32 timeInMs);
	void UpdateMobilePhoneRadio(u32 timeInMs);
	void ComputeVolumeOffset(void);
	void UpdateUserControl(u32 timeInMs);

	void TriggerRetuneBurst();
	void UpdateRetuneSounds();
	
	static u32 sm_LastRetuneTime;
	static u8 sm_PendingPlayerRadioStationRetunes;
	static bool sm_ForceVehicleExplicitRetune;
	static bool sm_IsMobilePhoneRadioActive;

	audEntityRadioEmitter m_MobilePhoneRadioEmitter;

	const audRadioStation *m_PlayerRadioStation;
	const audRadioStation *m_PlayerRadioStationPendingRetune;
	const audRadioStation *m_LastPlayerRadioStation;
	const audRadioStation *m_RequestedRadioStation;
	const audRadioStation *m_ForcePlayerStation;

	audSoundSet m_RadioSounds;

#if GTA_REPLAY
	audSound* m_ReplayMusicSound;
	u32 m_PreviouslyRequestedReplayTrack;
	s32 m_PreviouslyRequestedReplayTrackID;
	float m_ReplayMusicFadeOutScalar;
	bool m_ShouldStopReplayMusic;
	bool m_StopReplayMusicPreview;
	bool m_StopReplayMusicNoRelease;
	bool m_ShouldPrepareReplayMusic;
	bool m_ShouldPlayReplayMusic;
	bool m_ShouldUpdateReplayPreview;
	bool m_ShouldPlayReplayPreview;
	bool m_ShouldFadeReplayPreview;
	bool m_ShouldUpdateReplayMusic;
	u32 m_ReplayMusicStartOffset;
	u32 m_ReplayMusicStartTime;
	u32 m_PreviewReplayTrack;
	bool m_HasParsedMarkers;
	u32 m_RockoutStartOffset;
	u32 m_RockoutEndOffset;
	u32 m_PreviewReplayTrackTime;

	bool m_WasDuckingMusicForSFX;
	u32 m_SFXMusicDuckEndTime;
	f32 m_SFXFullMusicDuckVolume;
#endif

	u32 m_RetuneLastReleaseTime;
	u32 m_VolumeLastReleaseTime;
	u32 m_CurrentStationIndex;

	u32 m_LastActiveRadioTimeMs;
	f32 m_PlayerVehicleInsideFactor;
	f32 m_VolumeOffset;
	RegdVeh m_PlayerVehicle;
	RegdVeh m_LastPlayerVehicle;
	audSmoother m_PositionedToStereoVolumeSmoother;

	f32 m_CachedTimeTillNextAudibleBeat;
	f32 m_CachedAudibleBeatBPM;
	s32 m_CachedAudibleBeatNum;
	bool m_CachedAudibleBeatValid;
	
	u32 m_TimeEnteringStartingState;

	u32 m_LastPlayerModelId;

	u8 m_PlayerVehicleRadioState;
	u8 m_MobilePhoneRadioState;
	bool m_IsPlayerInAVehicleWithARadio;
	
	bool m_IsPlayerInVehicleForRadio;
	bool m_WasPlayerInVehicleForRadioLastFrame;
	bool m_WasMobilePhoneRadioActiveLastFrame;
	bool m_WasMobileRadioActiveOnEnteringFrontend;
	bool m_IsPlayerRadioSwitchedOff;	
	bool m_InFrontendMode;
	bool m_AreRetunesMuted;
	u32 m_LastTimeVehicleRadioWasRetuned;
	bool m_VehicleRadioJustRetuned;
	bool m_VehicleRadioRetuneAlreadyFlagged;
	bool m_IgnorePlayerVehicleRadioShutdown;

	float m_AutoUnfreezeForPlayerTimer;
	bool m_AutoUnfreezeForPlayer;

	bool m_ForcePlayerCharacterStations;
	bool m_SkipOnOffSound;

	bool m_bMPDriverHasTriggeredRadioChange;
	
	enum FadeState
	{
		kFadingIn = 0,
		kFadingOut,
		kWaitingToFadeIn,
		kWaitingToFadeOut,
		kNotFading
	};
	FadeState m_FadeState;
	float m_InvFadeTime;
	float m_FadeWaitTime;
	float m_FadeVolumeLinear;

	float m_FrontendFadeTime;
	float m_FrontendFadeTimeResetTimer;

	bool m_StopEmittersForPlayerVehicle;

	static bool sm_IsMobilePhoneRadioAvailable;
	
#if __BANK
	static bool sm_ShouldPerformRadioRetuneTest;
	static u8 sm_RequestedRadioStationIndex;
	static u32 sm_RetunePeriodMs;
	static const char *sm_RadioStationNames[g_MaxRadioStations];

	u32 m_LastTestRetuneTimeMs;
#endif // __BANK

	
#if !__FINAL
	static bool sm_ShouldSkipForward;
	static bool sm_ShouldQuickSkipForward;
#endif // !__FINAL

};

extern audRadioAudioEntity g_RadioAudioEntity;

#endif // NA_RADIO_ENABLED
#endif // AUD_RADIOAUDIOENTITY_H
