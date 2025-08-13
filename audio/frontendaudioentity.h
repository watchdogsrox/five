// 
// audio/frontendaudio.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_FRONTENDAUDIOENTITY_H
#define AUD_FRONTENDAUDIOENTITY_H

#include "audiosoundtypes/sound.h"
#include "audioengine/soundset.h"
#include "audioengine/tempomap.h"
#include "pickups/Data/PickupIds.h"
#include "audiosoundtypes/streamingsound.h"
#include "streamslot.h"
#include "atl/array.h"
#include "peds/ped.h"
#include "peds/PlayerArcadeInformation.h"
#include "weapons/Explosion.h"

#include "audioentity.h"

#include "control/replay/ReplaySettings.h"

#define AUD_MUSIC_STUDIO_PLAYER 0
#if AUD_MUSIC_STUDIO_PLAYER
#define AUD_MUSIC_STUDIO_PLAYER_ONLY(x) x
#else
#define AUD_MUSIC_STUDIO_PLAYER_ONLY(x)
#endif // AUDIO_MUSIC_STUDIO_PLAYER

const u32 g_NumChannelsInFrontendMusic = 7;

enum audFEMusicState
{
	AUD_FEMUSIC_IDLE = 0,
	AUD_FEMUSIC_ACQUIRING_SLOT,
	AUD_FEMUSIC_PREPARING,
	AUD_FEMUSIC_PLAYING,
	AUD_FEMUSIC_CLEANINGUP,
};

#if GTA_REPLAY
enum audMontageMusicState
{
	AUD_MUSIC_IDLE = 0,
	AUD_MUSIC_ACQUIRING_SLOT,
	AUD_MUSIC_PREPARING,
	AUD_MUSIC_PREPARED,
	AUD_MUSIC_PLAYING,
	AUD_MUSIC_FINISHING,
	AUD_MUSIC_PAUSED,
};
#endif

enum audFEChannelState
{
	AUD_FEMUSIC_CHANNEL_MUTED = 0,
	AUD_FEMUSIC_CHANNEL_WAITING,
	AUD_FEMUSIC_CHANNEL_PLAYING,
};

class audScene;

struct audModelPickupSound
{
	s32 modelIndex;
	u32 soundHash;
};

#if AUD_MUSIC_STUDIO_PLAYER
class audMusicStudioSessionPlayer
{

public:

	audMusicStudioSessionPlayer(){}
	~audMusicStudioSessionPlayer(){}

	bool Init(audEntity *audioEntity);
	void Shutdown();

	void Update(const u32 timeInMs);
	
	bool LoadSession(const char *sessionName);
	void StartPlayback() { m_RequestedToPlay = true; }
	void StopPlayback() { m_RequestedToPlay = false; }
	void SetPartVariationIndex(const s32 sectionIndex, const s32 partIndex, const s32 variationIndex);
	void SetPartVolume(const s32 sectionIndex, const s32 partIndex, const float volumeDb);
	void SetPartLPF(const s32 sectionIndex, const s32 partIndex, const bool lpfCutoffFreq);
	void SetPartHPF(const s32 sectionIndex, const s32 partIndex, const bool hpfCutoffFreq);
	void GetTimingInfo(float &nextBeatTimeS, float &bpm, s32 &beatNum, float &timeUntilNextSectionTransitionS);
	void RequestSectionTransition(const s32 sectionIndex);
	void UnloadSession();
	void RequestDelayedSettings();
	bool IsActive() const { return m_State != AUD_AMS_IDLE && m_State != AUD_AMS_CLEANINGUP; }
	bool IsPlaying() const { return m_State == AUD_AMS_PLAYING; }
	s32 GetActiveSection() const { return m_ActiveSectionIndex; }
	u32 GetLoadedSessionHash() const { return m_SessionSet.GetNameHash(); }
	void SetSoloState(const bool solo1, const bool solo2, const bool solo3, const bool solo4);
	void SetMuteState(const bool mute) { m_Muted = mute; }
	void SetOcclusionEnabled(const bool enabled) { m_EnableOcclusion = enabled; }

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

private:

	static bool OnMusicStopCallback(u32 userData);
	static bool HasMusicStoppedCallback(u32 userData);

	bool CleanUp();

	audSoundSet m_SessionSet;
	audEntity *m_Parent;
	enum {kMaxMusicStudioSessionSections = 2};

	struct audMusicStudioPartInfo
	{
		audMusicStudioPartInfo() :
			Volume(-100.f),
			AppliedVolume(-100.f),
			VariationIndex(0),
			Hpf(0),
			Lpf(kMixerNativeSampleRate>>1)
		{

		}
		float Volume;
		float AppliedVolume;
		s32 VariationIndex;
		s32 Hpf;
		s32 Lpf;
	};
	enum {kMusicStudioSessionParts = 4};
	struct audMusicStudioSessionSectionInfo
	{
		audSound *Sound;
		audStreamSlot *StreamSlot;

		atRangeArray<audMusicStudioPartInfo, kMusicStudioSessionParts> Parts;
	};
	atRangeArray<audMusicStudioSessionSectionInfo, kMaxMusicStudioSessionSections> m_Sections;
	enum audMusicStudioSessionPlayerState
	{
		AUD_AMS_IDLE = 0,
		AUD_AMS_VIRTUALIZING_SLOT,
		AUD_AMS_ACQUIRING_SLOT,
		AUD_AMS_PREPARING,
		AUD_AMS_PREPARED,
		AUD_AMS_PLAYING,
		AUD_AMS_CLEANINGUP,
	};
	audScene *m_SessionLoadedScene;
	audScene *m_SessionPlayingScene;
	audTempoMap m_TempoMap;
	audTempoMap::TimelinePosition m_CurrentPosition;
	audMusicStudioSessionPlayerState m_State;
	s32 m_ActiveSectionIndex;
	s32 m_RequestedSectionIndex;
	u32 m_SectionTransitionBar;
	atFixedBitSet<4> m_SoloState;
	audSmoother m_MuteVolumeSmoother;
	float m_SessionTempo;
	float m_TimeUntilNextTransition;
	float m_PlayTimeOfNextTransition;
	bool m_HavePlayedTransition;
	bool m_RequestedToPlay;
	bool m_RequestedDelayedSettings;
	bool m_Muted;
	bool m_EnableOcclusion;
};
#endif // AUD_MUSIC_STUDIO_PLAYER

class audFrontendAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audFrontendAudioEntity);

	audFrontendAudioEntity();
	~audFrontendAudioEntity();

	void Init();
	void InitCurves();
	virtual void PreUpdateService(u32 timeInMs);
	void Shutdown();
	void ShutdownLevel();
	
	void PlaySoundMapZoom();
	void PlaySoundMapMovement(f32 moveRate = 1.f);
	void StopSoundMapZoom();
	void StopSoundMapMovement();
	void PlaySound(const atHashWithStringBank soundName,const char *soundSetName);
	void StartPauseMenuFirstHalf();
	void StartPauseMenuSecondHalf();
	void StopPauseMenuFirstHalf();

#if GTA_REPLAY	
	void PlaySoundReplayScrubbing();
	bool IsScrubSoundPlaying() const { return m_ShouldBePlayingReplayScrub; }
	void StopSoundReplayScrubbing();
#endif

	void TriggerPlayerTinnitusEffect(CExplosionManager::eFlashGrenadeStrength effectStrength) { m_RequestedTinnitusEffect = effectStrength; }
	void PlaySoundNavLeftRightContinuous();
	void StartSpecialAbility(u32 playerHash);
	void StopSpecialAbility();
	void StartSwitch();
	void StopSwitch();
	void StartWhiteWarningSwitch();
	void StopWhiteWarningSwitch();
	void StartRedWarningSwitch();
	void StopRedWarningSwitch();
	void StartWeaponWheel();
	void StopWeaponWheel();
	void StartStunt();
	void StopStunt();
	void StartCinematic();
	void StopCinematic();
	void StartGeneralSlowMo();
	void StopGeneralSlowMo();

	
	void StopLoadingTune(bool longRelease = false);
	void StartLoadingTune();
	bool IsLoadingTunePlaying() const;
	void StartInstall();
	void StopInstall();

	bool IsLoadingSceneActive() const { return m_LoadingScreenScene != NULL; }

	void Pickup(atHashWithStringNotFinal pickUpHash,const u32 modelIndex,const u32 modelNameHash,const CPed * pPed );
	audMetadataRef GetAudioPickUp(atHashWithStringNotFinal pickUpHash);
	void PickupWeapon(const u32 weaponHash, const u32 pickupType, const s32 modelIndex);
	bool SetModelSpecificPickupSound(const s32 modelIndex, const u32 soundNameHash);

	void SetMPSpecialAbilityBankId();
	void SetSPSpecialAbilityBankId();

	void NotifyFrontendInput();

	void TriggerLongSwitchSkyLoop(eArcadeTeam arcadeTeam);
	void TriggerLongSwitchSound(const atHashWithStringBank soundNameHash, eArcadeTeam arcadeTeam = eArcadeTeam::AT_NONE);
	void StopLongSwitchSkyLoop();

	bool GetSwitchIsActive() { return m_SwitchIsActive; }
#if __BANK
	static void AddWidgets(bkBank &bank);
#endif


	virtual bool IsUnpausable() const
	{
		return true;
	}


	void EnableFEMusic()
	{
		m_IsFEMusicCodeDisabled = false;
	}
	void DisableFEMusic()
	{
		m_IsFEMusicCodeDisabled = true;
	}

	void UpdateMusic();

	bool IsMenuMusicPlaying() const { return m_MusicStreamingSound != NULL; }
	
	enum audSpecialAbilityMode
	{
		kSpecialAbilityModeNone = -1,
		kSpecialAbilityModeMichael = 0,
		kSpecialAbilityModeFranklin,
		kSpecialAbilityModeTrevor,
		kSpecialAbilityModeMultiplayer
	};
	audSpecialAbilityMode GetSpecialAbilityMode() const { 
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
			return kSpecialAbilityModeNone;
#endif
		return m_SpecialAbilityMode; 
	}
	const audSoundSet &GetSpecialAbilitySounds() const { return m_SpecialAbilitySounds; }
	
	const audSoundSet &GetPulseHeadsetSounds() const { return m_PulseHeadsetSounds; }

#if GTA_REPLAY		
    static void InterpolateReplayVolume(f32& currentVolumeDB, f32 targetVolumeDB, f32 interpRate);
    void JumpToTargetCustomAmbienceVolume() { m_ReplayCustomAmbienceVolume = m_ClipCustomAmbienceVolume; }
    void JumpToTargetSFXOneShotVolume() { m_ReplaySFXOneShotVolume = m_ClipSFXOneshotVolume; }

	void SetClipMusicVolumeIndex(s32 index);
	void SetClipSFXVolumeIndex(s32 index);
	void SetClipDialogVolumeIndex(s32 index);
    void SetClipCustomAmbienceVolumeIndex(s32 index);
    void SetClipSFXOneshotVolumeIndex(s32 index);
	f32 VolumeIndexToDb(s32 index);

	void SetClipFadeVolLinear(f32 volumeLinear);
	void SetReplayVolume(f32 volume)				{ m_ReplayVolume = volume; }
	void SetReplayDialogVolume(f32 volume)			{ m_ReplayDialogVolume = volume; }
	void SetSpeechOn(bool on)						{ m_SpeechOn = on; }
	void SetClipSpeechOn(bool on)					{ m_ClipSpeechOn = on; }
	s32 GetClipSFXVolumeIndex()						{ return m_ClipSFXVolumeIndex; }	
	f32 GetClipSFXVolume()							{ return m_ClipSFXVolume; }
	s32 GetClipMusicVolumeIndex()					{ return m_ClipMusicVolumeIndex; }
	f32 GetClipMusicVolume()						{ return m_ClipMusicVolume; }
	s32 GetClipDialogVolumeIndex()					{ return m_ClipDialogVolumeIndex; } 
	f32 GetClipDialogVolume()						{ return m_ClipDialogVolume; }
	f32 GetMontageMusicVolume()						{ return m_MontageMusicFadeVolume; }
	f32 GetReplayVolume()							{ return m_ReplayVolume + Min(m_MontageSFXFadeVolume, m_ClipSFXFadeVolume); }
	f32 GetReplayDialogVolume()						{ return m_ReplayDialogVolume + Min(m_MontageSFXFadeVolume, m_ClipSFXFadeVolume); }
    f32 GetReplaySFXOneshotVolume()					{ return m_ReplaySFXOneShotVolume + Min(m_MontageSFXFadeVolume, m_ClipSFXFadeVolume); }
    f32 GetReplayCustomAmbienceVolume()				{ return m_ReplayCustomAmbienceVolume + Min(m_MontageSFXFadeVolume, m_ClipSFXFadeVolume); }
	bool GetSpeechOn()								{ return m_SpeechOn; }
	bool GetClipSpeechOn()							{ return m_ClipSpeechOn; }	

	f32 GetReplayMuteVolume()						{ return m_ReplayMuteVolume; }
	void SetReplayMuteVolume(f32 volDb)				{ m_ReplayMuteVolume = volDb; }

	bool ShouldMuteSpeech();	
#endif
	
#if AUD_MUSIC_STUDIO_PLAYER
	audMusicStudioSessionPlayer &GetMusicStudioSessionPlayer() { return m_MusicStudioSessionPlayer; }
#endif // AUD_MUSIC_STUDIO_PLAYER

private:
	void HandleMapZoomSoundEvent();
	void HandleMapMoveSoundEvent();
	void HandleNavLeftRightContinousEvent();
#if GTA_REPLAY
	void HandleReplayScrubSoundEvent();
#endif
	void ProcessDeferredSoundEvents();

	void StartSpecialAbilitySounds();
	void StartPauseMenuFirstHalfPrivate();
	void StartPauseMenuSecondHalfPrivate();
	bool ShouldFEMusicBePlaying() const;	
	
	static u32 GetCnCSwitchSoundsetName(eArcadeTeam arcadeTeam);
	static bool OnMusicStopCallback(u32 userData);
	static bool HasMusicStoppedCallback(u32 userData);

	void UpdateRoomTone(u32 timeInMs);
	
	void UpdateTinny();
	void UpdateTinnitusEffect();

	static const int k_NumDeferredSoundEvents = 32;

	audSoundSet m_SoundSet;
	audSoundSet m_LongSwitchSounds;

	audSound* m_MapZoomSound;
	audSound* m_MapMovementSound;
	audSound* m_NavLeftRightContinuousSound;
#if GTA_REPLAY
	audSound* m_replayScrubbingSound;
#endif
	atRangeArray<audSound*, (u32)eArcadeTeam::AT_NUM_TYPES> m_CnCSkyLoops;	
	audSound* m_TinnyLeft;
	audSound* m_TinnyRight;
	audSound *m_LoadingTune;
	audScene *m_LoadingScreenScene;
	audSound *m_PauseMenuFirstHalf;
	audSound *m_SpecialAbilitySound, *m_WeaponWheelSound, *m_SwitchSound,*m_WhiteWarningSwitchSound,*m_RedWarningSwitchSound, *m_StuntSound, *m_CinematicSound, *m_SlowMoGeneralSound;
	audCurve m_DeafeningTinnyVolumeCurve;

	audWaveSlot *m_SpecialAbilitySlot;
	atRangeArray<u32, 4> m_SpecialAbilityBankIds;
	audSoundSet m_SpecialAbilitySounds;
	audSpecialAbilityMode m_SpecialAbilityMode;
	
	audSound *m_MusicStreamingSound;

	audSoundSet m_PulseHeadsetSounds;
	audSound *m_PlayerPulseSound;

	audFEChannelState m_ChannelState[g_NumChannelsInFrontendMusic];
	f32 m_ChannelInterp[g_NumChannelsInFrontendMusic];
	f32 m_ChannelInterpSpeed[g_NumChannelsInFrontendMusic];
	f32 m_ChannelTargetPan[g_NumChannelsInFrontendMusic];
	f32 m_ChannelStartPan[g_NumChannelsInFrontendMusic];
	f32 m_ChannelWaitTime[g_NumChannelsInFrontendMusic];

	audCategoryController *m_LoadSceneCategoryController;

	AUD_MUSIC_STUDIO_PLAYER_ONLY(audMusicStudioSessionPlayer m_MusicStudioSessionPlayer);

	audFEMusicState m_MusicState;
	
	f32 m_MusicOverallIntensity;
	f32 m_MusicTargetIntensity;

	f32 m_MapCursorMoveRate;

	u32 m_LastUpdateTime;
	audStreamSlot *m_MusicStreamSlot;

	atFixedArray<audModelPickupSound,32> m_ModelSpecificPickupSounds;

	bool m_IsFEMusicCodeDisabled;

	audSound* m_RoomToneSound;
	u32 m_CurrentRoomToneHash;
	u32 m_TimeLastPlayedRoomTone;

	audSound *m_AmpFixLoop;

	audSound * m_PickUpSound;
	audSoundSet m_PickUpStdSoundSet;
	audSoundSet m_PickUpWeaponsSoundSet;
	audSoundSet m_PickUpVehicleSoundSet;
	audSoundSet m_SpecialAbilitySoundSet;

	u32 m_LastGrenadeFlashTime;

	bool m_PlayingLoadingMusic;
	bool m_IsFrontendMusicEnabled;
	bool m_HasInitialisedCurves;

	bool m_WasGrenadeFlashing;
	bool m_SwitchIsActive;

	bool m_FirstFEInput;
	bool m_StartMusicQuickly;

	bool m_HasPlayedWeaponWheelSound;
	bool m_HasToPlayPauseMenu;
	bool m_HasToPlayPauseMenuSecondHalf;
	bool m_PlayedPauseMenuFirstHalf;
	bool m_HasMutedForLoadingScreens;

	bool m_ShouldBePlayingMapMove;
	bool m_ShouldBePlayingMapZoom;
	bool m_ShouldBePlayingNavLeftRightContinuous;
	CExplosionManager::eFlashGrenadeStrength m_RequestedTinnitusEffect;

	bool m_HasPlayedLoadingTune;

	BANK_ONLY(static bool sm_ShowScaleformSoundsPlaying;);
#if GTA_REPLAY
	//bool m_WasLastSlowMoTransitionIn;
	u32 m_LastSlowMoTransitionTime;

	audSound *m_ReplaySlowMoDrone;
	audSimpleSmoother m_ReplaySlowMoDroneVolSmoother;

	audCategoryController *m_GameWorldCategoryCOntroller;
	audCategoryController *m_CutsceneCategoryController;
	audCategoryController *m_SpeechCategoryController;
	audCategoryController *m_AnimalVocalsCategoryController;
	f32 m_ClipMusicVolume;
	f32 m_ClipSFXVolume;
	f32 m_ClipDialogVolume;
    f32 m_ClipCustomAmbienceVolume;
    f32 m_ClipSFXOneshotVolume;
	f32 m_ReplayVolume;
	f32 m_MontageMusicFadeVolume;
	f32 m_MontageSFXFadeVolume;
	f32 m_ClipSFXFadeVolume;
	f32 m_ReplayDialogVolume;
    f32 m_ReplaySFXOneShotVolume;
    f32 m_ReplayCustomAmbienceVolume;
	s32 m_ClipMusicVolumeIndex;
	s32 m_ClipSFXVolumeIndex;
	s32 m_ClipDialogVolumeIndex;
    s32 m_ClipCustomAmbienceVolumeIndex;
    s32 m_ClipSFXOneshotVolumeIndex;
	bool m_SpeechOn;
	bool m_ClipSpeechOn;
	bool m_ShouldBePlayingReplayScrub;	
	f32 m_ReplayMuteVolume;
#endif

};

extern audFrontendAudioEntity g_FrontendAudioEntity;
extern audCategory *g_FrontendGameWeaponPickupsCategory;

#endif


