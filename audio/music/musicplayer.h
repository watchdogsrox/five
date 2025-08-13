// 
// audio/music/musicplayer.h 
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_MUSICPLAYER_H
#define AUD_MUSICPLAYER_H

#include "audio/gameobjects.h"

#include "audio/audioentity.h"

#include "atl/array.h"
#include "atl/queue.h"
#include "audioengine/tempomap.h"
#include "audioengine/widgets.h"
#include "bank/bank.h"
#include "math/amath.h"
#include "control/replay/ReplayMarkerInfo.h"

#include "musiceventmanager.h"
#include "idlemusic.h"
#include "vehiclemusic.h"
#include "wantedmusic.h"

//////////////////////////////////////////////////////////////////////////
// Forward declarations
class audDynMixState;
class audInteractiveMusicManager;
class audStreamSlot;
class audMusicAction;

namespace rage
{
	class audCategory;
	class audCategoryController;
	class audStreamingSoundSlot;
	class audStreamingWaveSlot;
	class bkBank;
	class audStreamingSound;
}

#define g_InteractiveMusicManager (audInteractiveMusicManager::InstanceRef())

#define AUD_NUM_MUSIC_STEMS 8

class audInteractiveMusicManager : public naAudioEntity
{
private:
	static audInteractiveMusicManager* sm_Instance;
	audInteractiveMusicManager();
	audInteractiveMusicManager(const audInteractiveMusicManager&);
	audInteractiveMusicManager& operator=(const audInteractiveMusicManager&);
	~audInteractiveMusicManager();

public:

	static void Instantiate();
	static void Destroy();
	static inline audInteractiveMusicManager* InstancePtr()	{ FastAssert(sm_Instance); return  sm_Instance; }
	static inline audInteractiveMusicManager& InstanceRef()	{ FastAssert(sm_Instance); return *sm_Instance; }
	static inline bool	IsInstatiated()				{ return sm_Instance != NULL; }


	virtual bool IsUnpausable() const
	{
		return true;
	}

	enum enState
	{
		kStateUndefined = -1,
		kStateSilent,
		kStateUnscripted,
		kStateScripted,

		kNumStates
	};

	enum enMode
	{
		kModePlaying,
		kModeStopped,
		kModeSuspended,

		kNumModes
	};

	enum enEventTypes
	{
		kEventDoNothing,
		kEventPrepareTrack,
		kEventSnapCategoriesToMood,
		kEventCrossFadeCategoriesToMood,
		kEventWaitForWaveslotFree,
		kEventWaitForTime,

		kNumEvents
	};

#if __BANK
	enum enBankBankMoodType
	{
		kBankMoodTypeSilent,
		kBankMoodTypeIdle,
		kBankMoodTypePastoral,
		kBankMoodTypeSuspense,
		kBankMoodTypeDramatic,
		kBankMoodTypeGunfight,
		kBankMoodTypeChase,
		KBankMoodTypeHappy,
		kBankMoodTypeEverything,

		kNumBankMoodTypes
	};
	enum enBankBankMoodVariation
	{
		kBankMoodVarBlank,
		kBankMoodVarLow,
		kBankMoodVarHigh,

		kNumBankMoodsVariations
	};
	static const char* sm_BankMusicMoodVariationStrings[];
	static const char* sm_BankMusicMoodTypeStrings[];
#endif

	struct tMusicEvent
	{
		tMusicEvent() :
	m_EventType(kEventDoNothing)
	{
		m_arg1.p=NULL;
		m_arg2.p=NULL;
		m_arg3.p=NULL;
		m_arg4.p=NULL;
		m_arg5.p=NULL;
	}

	enEventTypes m_EventType;

	union unPtrIntFlt
	{
		const void* p;
		s32 i;
		u32 u;
		f32 f;
	};

	unPtrIntFlt m_arg1;
	unPtrIntFlt m_arg2;
	unPtrIntFlt m_arg3;
	unPtrIntFlt m_arg4;
	unPtrIntFlt m_arg5;
	};

	void Init();
	void Shutdown();
	void Reset();
	void PreUpdateService(u32);
	void UpdateMusic(const u32& uTime);
	void UpdateOneshot(const u32& uTime);
	void UpdateEventQueue();

	// PURPOSE
	//	Begins playing a specific music track and sets the state machine to scripted
	// PARAMS
	//	const char * pszTrackName :	The name of the Rave game object MusicTrack
	//	const InteractiveMusicMood * mood:	Mood to apply
	//	iFadeInTime - Use -1 to have code decide, or something else to override. If we're already playing the track
	//	 passed in, then this will function as the crossfade time if the mood changes.
	//	iFadeOutTime - Use -1 to have code decide, or something else to override. If we're already playing the track
	//	 passed in, this param is not used.
	//	s32 fStartOffsetScalar:	Specify value 0 - 1.0 to start at a specific point in the track, -1.0 for random
	void PlayLoopingTrack(const u32 soundNameHash, const rage::InteractiveMusicMood *mood, f32 fVolumeOffset, s32 iFadeInTime, s32 iFadeOutTime, f32 fStartOffsetScalar, f32 startDelay, bool overrideRadio, bool muteRadioOffSound);

	// PURPOSE
	//	All automagic.  Plays a random stinger for post load depending on what region we are.
	void PlayPostLoadStinger();

	bool IsMusicPrepared();

	// PURPOSE
	//	Trigger a mood change at the specified triggerTime (specified in seconds relative to the start of the currently playing track)
	void SetMood(const InteractiveMusicMood *mood, const float triggerTime, const float volumeOffset, const s32 fadeInTime, const s32 fadeOutTime);
	
	void NotifyWeaponFired();
	void NotifyFiredUponPlayer();
	void NotifyTerritoryChanged();
	
	void StopAndReset(const s32 releaseTime = -1, const float triggerTime = -1.f);
	s32 GetCurPlayTimeMs(){ return m_iCurPlayTimeMs; }
	f32 GetAmbientMusicDuckingVolume() { return m_fAmbMusicDuckingVol; }

	int GetMillisecondsUntilAudioSync(const char* moodName, const char* szBackupMoodName, s32 minTime, s32 maxTime, bool changeMoodAutomatically=true);
	bool IsAudioSyncNow(const char* moodName, const char* szBackupMoodName, s32 minTime, s32 maxTime, bool changeMoodAutomatically);
	void ClearAudioSyncVariables();

	float ComputeNextPlayTimeForConstraints(const MusicAction *constrainedAction, const float timeOffsetS = 0.f) const;
	float ComputeNextPlayTimeForConstraints(atFixedArray<const MusicTimingConstraint *, MusicAction::MAX_TIMINGCONSTRAINTS> &constraints, const float timeOffsetS = 0.f) const;

	bool PreloadOneShot(const u32 soundNameHash, const s32 iPredelay, const s32 iFadeInTime, const s32 iFadeOutTime, const bool playWhenPrepared = false);
	void PlayTimedOneShot(const float triggerTime);

	bool IsOneShotPrepared();
	void PlayPreloadedOneShot();
	void StopOneShot(bool bIgnoreFadeTime=false 
#if __BANK
		, bool fromWidget=false
#endif
		);
	bool OnStopCallback(u32 userData);
	bool HasStoppedCallback(u32 userData);
#if __BANK
	void AddWidgets(rage::bkBank& bank/*, rage::bkGroup* pGroup*/);
	void WidgetPlayLooping();
	void WidgetStopOneshot();	
	void WidgetStopAndReset();
	void WidgetTrackSkip();

	void DebugPrepareEvent();
	void DebugTriggerEvent();
	void DebugCancelEvent();
	void DebugStopTrack();

	void DebugDraw();
#endif

#if GTA_REPLAY
	void RequestReplayMusic(const u32 soundHash, const u32 startOffsetMs, const s32 durationMs = -1, const f32 fadeOutScaler = 1.0f);
	void SetReplayScoreIntensity(const eReplayMarkerAudioIntensity intensity, const eReplayMarkerAudioIntensity prevIntensity, bool instant, u32 replaySwitchMoodOffset);
	void UpdateReplayMusic();
	void UpdateReplayMusicPreview(bool allowFading = true);
	void UpdateReplayMusicVolume(bool instant = false);
	void SetReplayScoreDuration(const u32 durationMs);
	bool IsReplayPreviewPlaying() { return m_ReplayPreviewPlaying; }
	void OnReplayEditorActivate();
	f32 GetPrevReplayBeat(u32 soundHash, f32 startOffsetMS);
	f32 GetNextReplayBeat(u32 soundHash, f32 startOffsetMS);

	void SaveMusicState();
	void RestoreMusicState();
	void ClearSavedMusicState();
	void SetRestoreMusic(bool shouldRestore)			{ m_ShouldRestoreSavedMusic = shouldRestore; }
	void SetEditorWasActive(bool wasActive)				{ m_EditorWasActive = wasActive; }
#endif	//GTA_REPLAY

	//	Accessors/modifiers
	bool IsMusicPlaying() const;
	bool IsFrontendMusicPlaying() const { return m_MusicSound != NULL; }
	bool IsOneshotActive() const { return m_OneShotSound != NULL; }

	bool ShouldMusicOverrideRadio() const { return m_MusicOverridesRadio; }
	
	u32 GetPlayingTrackSoundHash() {return m_PlayingTrackSoundHash;}
	const rage::InteractiveMusicMood* GetPlayingMood() {return m_pPlayingMood;}

	audMusicEventManager &GetEventManager() { return m_EventManager; }
	audWantedMusic &GetWantedMusic() { return m_WantedMusic; }
	audIdleMusic &GetIdleMusic() { return m_IdleMusic; }
	audVehicleMusic &GetVehicleMusic() { return m_VehicleMusic; }

	void SetUnfreezeRadioOnStop(const bool shouldUnfreeze) { m_UnfreezeRadioOnStop = shouldUnfreeze; }
	void SetUnfreezeRadioOnOneShot(const bool shouldUnfreeze) { m_UnfreezeRadioOnOneShot = shouldUnfreeze; }

	u32 GetActiveOneShotHash() const { return m_ActiveOneshotHash; }
	float GetOneShotLength() const;

	float GetStaticRadioEmitterVolumeOffset() const { return m_StaticEmitterVolumeOffset; }

	// PURPOSE
	//	Searches for the next silent region after the specified time offset in seconds
	// PARAMS
	//	minLength - minimum silent region duration to consider
	//	timeOffsetS - track time to search from, in seconds.  -1.f will use the current playtime
	// RETURNS
	//	Start of next silent region in seconds, -1.f if none were found
	float ComputeNextSilentTime(const float minLength, const float timeOffsetS = -1.f) const;

	u32 GetTimeInMs() const;

	// PURPOSE
	//	Returns true if the user has turned on the radio, resulting in the score being muted
	// NOTES
	//	Only true if score is playing, since we reset when the next track is started
	bool IsScoreMutedForRadio() const { return m_IsScoreMutedForRadio && IsMusicPlaying() && m_MusicPlayState == PLAYING; }

	bool IsMusicPrepared() const { return m_MusicPlayState == PREPARED; }	
	bool IsScoreEnabled() const;

	audStreamingWaveSlot *GetOneShotWaveSlot() { return m_pOneshotWaveSlot; }
	audStreamingWaveSlot *GetMusicWaveSlot() const { return m_pMusicWaveSlot; }

private:

	void InternalPrepareTrack(const u32 soundNameHash, const rage::InteractiveMusicMood* pMood, f32 fStartOffsetScalar, bool bLooping, f32 delayTime);
	void InternalSetMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset, s32 iFadeInTime, s32 iFadeOutTime);
	void InternalSnapToMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset=0.0f);
	void InternalSnapCategoriesToMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset);
	void InternalCrossfadeCategoriesToMood(const rage::InteractiveMusicMood* pMood, f32 fVolumeOffset, s32 iFadeInTime, s32 iFadeOutTime, s32 iCrossfadeOffsetTime = -1);
	void InternalStop(s32 iReleaseTime = -1);
	void InternalStopAndReset(s32 iReleaseTime = -1);
		
	void ParseMarkers();
	void UpdateMusicTimerPause();

	void ResetMoodState();
	void UpdateMoodState(const float trackPlayTimeS);

	void CrossfadeToStemMix(const StemMix *mix, const float volumeOffsetDb, const float fadeInTimeS, const float fadeOutTimeS, const float crossfadeOffsetTimeS = 0.0f);

	//	Members

	atQueue<tMusicEvent, 16> m_EventQueue;
	
	audStreamingSound *m_MusicSound;
	audSound *m_OneShotSound;
	audStreamingWaveSlot* m_pMusicWaveSlot;
	audStreamingWaveSlot* m_pOneshotWaveSlot;
	s32 m_iTrackLengthMs;
	s32 m_iCurPlayTimeMs;
	s32 m_iFadeInTime;
	s32 m_iTimeToTriggerMoodSnap;
	enMode m_enMode;
	
	f32 m_fAmbMusicDuckingVol;

	u32 m_PlayingTrackSoundHash;
	u32 m_PendingTrackSoundHash;
	u32 m_ActiveOneshotHash;
	const rage::InteractiveMusicMood* m_pPlayingMood;
	const rage::InteractiveMusicMood* m_pScriptSetMood;
	const rage::InteractiveMusicMood* m_pMoodToSnapTo;
	
	f32 m_fScriptSetVolumeOffset;
	float m_StaticEmitterVolumeOffset;

	audCategoryController* m_MusicCategory;
	audCategoryController* m_OneShotCategory;
	audCategoryController* m_MusicDuckingCategory;
	audCategoryController* m_OneShotDuckingCategory;
	atRangeArray<audCategoryController*,AUD_NUM_MUSIC_STEMS> m_pCatCtrlStems;
	
	audTempoMap m_TempoMap;
	u32 m_TempoMapSoundHash;

#if __BANK
	static const char *sm_LastOneShotRequested;
#endif

	audMusicEventManager m_EventManager;
	audWantedMusic m_WantedMusic;
	audIdleMusic m_IdleMusic;
	audVehicleMusic m_VehicleMusic;

	const InteractiveMusicMood *m_PendingMoodChange;
	float m_PendingMoodChangeTime;
	float m_PendingMoodVolumeOffset;
	s32 m_PendingMoodFadeInTime;
	s32 m_PendingMoodFadeOutTime;

	float m_PendingStopTrackTime;
	s32 m_PendingStopTrackRelease;

	u32 m_uTimeToReleaseScriptControl;
	u32 m_uTimeNoMusicPlaying;
	u32 m_uTimeWithMusicPlaying;
	u32 m_uNextMusicPlayTime;
	u32 m_uNextMusicStopTime;
	u32 m_uTimeInCurrentUnscriptedMood;

	s32 m_iTimePlayerMovingFast;
	s32 m_iTimePlayerMovingFastThres;
	s32 m_iTimePlayerMovingFastLimit;
	s32 m_MarkerTimeForIsAudioSyncNow;

	u32 m_uLastTimeSuspenseAllowed;
	u32 m_uLastTimePlayerFiredWeapon;
	u32 m_uLastTimePlayerFiredUpon;
	u32 m_uTerritoryChangeTime;

	audDynMixState* m_pDynMixState;
	u32 m_uTimeToTriggerMixer;
	u32 m_uTimeToDeTriggerMixer;

	s32 m_iOneShotReleaseTime;
	s32 m_iTimeToPlayOneShot;
	s32 m_iTimeToStopOneShot;

	u32 m_CurrentStemMixId;
	u32 m_CurrentStemMixExpiryTime;
	audMusicAction *m_NextStemMixMusicAction;
	float m_TrackTimeToSwitchStemMix;
	float m_PlayingMoodVolumeOffset;

	u32 m_ReplayWavePlayTimeAdjustment;

	float m_StartTrackTimer;

	struct SilentRegion
	{
		float startTimeS;
		float durationS;
	};
	// For now only support one set of silent regions, rather than one per stem
	atFixedArray<SilentRegion, 64> m_SilentRegions;

	audSimpleSmoother m_StaticRadioEmitterSmoother;

#if GTA_REPLAY
	eReplayMarkerAudioIntensity m_ReplayScoreIntensity;	
	eReplayMarkerAudioIntensity m_ReplayScorePrevIntensity;
	f32 m_ReplayMusicFadeVolumeLinear;
	u32 m_ReplayScoreSound;
	u32 m_ReplayScoreDuration;
	s32 m_ReplayMoodSwitchOffset;
	bool m_ReplaySwitchMoodInstantly;
	bool m_ReplayPreviewPlaying;

	u32 m_SavedCurrentStemMixId;
	const InteractiveMusicMood *m_SavedPlayingMood;
	float m_SavedPlayingMoodVolumeOffset;
	bool m_SavedMusicOverridesRadio;
	bool m_SavedMuteRadioOffSound;

	u32 m_SavedPlayingTrackSoundHash;
	u32 m_SavedPlayTimeOfWave;

	const InteractiveMusicMood *m_SavedPendingMoodChange;
	float m_SavedPendingMoodChangeTime;
	float m_SavedPendingMoodVolumeOffset;
	s32 m_SavedPendingMoodFadeInTime;
	s32 m_SavedPendingMoodFadeOutTime;
	float m_SavedPendingStopTrackTime;
	s32 m_SavedPendingStopTrackRelease;
	

	bool m_ShouldRestoreSavedMusic;
	bool m_EditorWasActive;
#endif	//GTA_REPLAY

	enum MoodState
	{
		WAITING_FOR_EXPIRY = 0,
		WAITING_FOR_CONSTRAINT,
	}m_MoodState;

	enum MusicPlayState
	{
		IDLE = 0,
		PREPARING,
		PREPARED,
		PLAYING,
	}m_MusicPlayState;

	bool m_bDisabled;
	bool m_bTrackIsLooping;
	bool m_bJustReleasedSciptControl;
	bool m_bWaitForTrackToFinishBeforeRelease;

	bool m_bWrapAroundSnap;
	bool m_bAudioSyncVariablesAreSet;

	bool m_bWaitingToPlayOneshot;
	bool m_bStopOneshotOnNextMarker;
	bool m_bFadeOneshotOnDelayedStop;
	bool m_bOneShotWasPlayingLastFrame;
	bool m_bWaitingOnWrapAroundToPlayOneshot;
	bool m_bWaitingOnWrapAroundToStopOneshot;
	bool m_bMusicJustWrappedAround;

	bool m_UnfreezeRadioOnStop;
	bool m_UnfreezeRadioOnOneShot;
	bool m_MusicOverridesRadio;
	bool m_IsScoreMutedForRadio;
	bool m_MuteRadioOffSound;

#if __BANK
	bool m_bOverrideScript;
	char m_szOverridenTrackName[128];
	char m_szOverridenMoodName[128];
	char m_szOverridenOneshotName[128];
	char m_szActiveTrackName[128];
	char m_szActiveSoundName[128];
	char m_szActiveMoodName[128];
	char m_szActiveOneshotName[128];
	char m_ClockBarsBeatsText[128];
	char m_ClockMinutesSecondsText[128];
	float m_ComputedMusicTime;
	f32 m_fTrackSkipStartOffsetScalar;
	f32 m_fWidgetVolumeOffset;
	s32 m_iWidgetFadeInTime;
	s32 m_iWidgetFadeOutTime;
	bool m_bUseFadeOutTimeWidget;
	enBankBankMoodType m_enWidgetMoodType;
	enBankBankMoodVariation m_enWidgetMoodVariation;

	char m_DebugEventName[64];

	bool m_DrawLevelMeters;
#endif
};






#endif // AUD_MUSICPLAYER_H

