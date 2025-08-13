// 
// audio/northaudioengine.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_NORTHAUDIOENGINE_H
#define AUD_NORTHAUDIOENGINE_H

#include "audioengine/controller.h"
#include "audioengine/metadatamanager.h"
#include "audioengine/metadataref.h"
#include "audioengine/remotecontrol.h"
#include "audio_channel.h" 
#include "dynamicmixer.h"
#include "gunfireaudioentity.h"
#include "audio/environment/environment.h"
#include "audio/environment/environmentgroup.h"
#include "audio/environment/environmentgroupmanager.h"
#include "audio/environment/microphones.h"
#include "audio/environment/occlusion/occlusionmanager.h"
#include "scene/RegdRefTypes.h"
#include "superconductor.h"
#include "system/ipc.h"

#include "audioentity.h"

#include "grcore/font.h"
#include "grcore/quads.h"

struct audMeterList
{
	audMeterList()
	{
		bottomColour = Color32(50,0,255,255);
		topColour = Color32(255,0,0,255);
		textColour = Color32(255,255,255,255);
		bgColour = Color32(25,25,25,200);
		title = NULL;
		drawValues = false;
		rangeMax = 1.f;
	}
	float left,bottom,width,height;
	u32 numValues;
	Color32 bottomColour,topColour,textColour, bgColour;
	float *values;
	float rangeMax;
	bool drawValues;
	const char **names;
	const char *title;
};

const u32 g_MaxBufferedMeters = 128;

class audScene;
class CPed;

class audGameObjectManager : public audObjectModifiedInterface
{
	virtual void OnObjectViewed(const u32 nameHash);
	virtual void OnObjectAuditionStart(const u32 nameHash);
	virtual void OnObjectOverridden(const u32 nameHash);
	virtual void OnObjectModified(const u32 nameHash);
	void OnObjectEdit(const u32 nameHash);
};

namespace rage
{
class audSound;
struct ambisonicDrawData;
}

class naAnimHandler : fwAudioAnimHandlerInterface
{
public:
	virtual void HandleEvent(const audAnimEvent & event,fwEntity *entity);
};

class audMetadataDataFileMounter : public CDataFileMountInterface
{
	static const u32 k_MaxDlcPacks = 48;
	static const u32 k_ChunkNameLength = 32;

public:
	audMetadataDataFileMounter(audMetadataManager &metadataManager);	

	virtual bool LoadDataFile(const CDataFileMgr::DataFile &file);

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file);

	virtual void Update();

private:

	static void DeriveChunkName(const char *fileName, char *chunkNameBuffer, const size_t chunkNameBufferLen);

	audMetadataManager &m_MetadataManager;
	char m_UnloadingChunkNames[k_MaxDlcPacks][k_ChunkNameLength];
	atRangeArray<u32, k_MaxDlcPacks> m_FramesToUnload;
	u32 m_UnloadingChunksCount;
};

class audNorthAudioEngine
{
public:
	static void Update();
	static void MinimalUpdate();

	static void StartEnvironmentLocalObjectListUpdate();
	static void FinishEnvironmentLocalObjectListUpdate();
	static void UpdateVehicleCategories();

	static void StartUpdate();
	static void FinishUpdate();
	static void StartUserUpdateFunction();
	static void StopUserUpdateFunction();
	static void UpdateAudioThread();

	// PURPOSE
	//	Called after the game has finished loading
	static void Init(u32 initMode);
	static void DLCInit(u32 initMode);

	static void Shutdown(u32 shutdownMode);

	// PURPOSE
	//  Pause the audio engine, and also do anything game-specific. Safe to call every frame.
	static void Pause();

	// PURPOSE
	//  Unpause the audio engine, and also do anything game-specific. Safe to call every frame.
	static void UnPause();

	static void OverrideFrontendScene(audScene* scene)
	{
		if(sm_FrontendScene)
		{
			sm_FrontendScene->Stop();
		}
		sm_FrontendScene = scene;
	}

	static void PurgeInteriors() { sm_Environment.PurgeInteriors(); }

	// PURPOSE
	//  Return whether or not we're paused.
	static bool IsAudioPaused()
	{
		return sm_Paused;
	}

	static void SetSfxVolume(const f32 volLinear);
	static f32 GetSfxVolume() { return sm_SfxVolume; }

	static void SetMusicVolume(const f32 volLinear);

	static f32 GetMusicVolume() { return sm_MusicVolume; }

	static u32 GetCurrentTimeInMs()
	{
		return sm_TimeInMs;
	}
	static u32 GetTimeStepInMs()
	{
		return sm_TimeInMs - sm_LastTimeInMs;
	}
	static f32 GetTimeStep()
	{
		return (f32)(sm_TimeInMs - sm_LastTimeInMs) / 1000.f;
	}
	static naMicrophones &GetMicrophones() {return sm_Microphones;}

	// PURPOSE
	//	Call from audio code when something loud happens - used to mute birds
	static void RegisterLoudSound(Vector3 &pos,bool playerShot = false);

	// PURPOSE
	//	Returns the last time a loud sound was registered near the listener
	static u32 GetLastLoudSoundTime();

	static void ResetLoudSoundTime()
	{
		sm_LastLoudSoundTime = 0;
	}

	// Get the current level name in a format that the audio systems expect.
	// Currently this is just the base folder name eg. 'gta5', 'gta5_liberty' etc.
	static const char* GetCurrentAudioLevelName();	

	// PURPOSE
	//	Draws a list of normalised meters to the screen
	static void DrawLevelMeters(audMeterList *BANK_ONLY(meters))
	{
#if __BANK
		if(sm_CurrentMeterIndex < g_MaxBufferedMeters-1)
		{
			sm_MeterList[sm_CurrentMeterIndex++] = meters;
		}
#endif
	}

	static void DebugDraw();

	static bool IsInSlowMo()
	{
		return sm_IsInSlowMo;
	}

	static bool IsInCinematicSlowMo();
	static bool IsInVideoEditor();
	static bool IsInSlowMoVideoEditor();	
	static bool IsSuperSlowVideoEditor();

#if __BANK
	static bool IsForcingSlowMoVideoEditor()		{ return sm_ForceSlowMoVideoEditor; }
	static bool IsForcingSuperSlowMoVideoEditor()	{ return sm_ForceSuperSlowMoVideoEditor; }
#endif

	static SlowMoType GetActiveSlowMoMode();

	static u32 GetBucketIdForStreamingSounds()
	{
		return sm_StreamingSoundBucketId;
	}

	static void NotifyStartNewGame()
	{
		sm_StartingNewGame = true;
	}

	static void CancelStartNewGame()
	{
		sm_StartingNewGame = false;
	}

	static void NotifyOpenNetwork();
	static void NotifyCloseNetwork(const bool fullClose);

#if __BANK
	static void AuditionWaveSlotChanged();
	static void InitWidgets();
	static void DebugDrawSlowMo();
#endif
	static bool sm_ShouldMuteAudio;
	
	static naEnvironment *GetGtaEnvironment()
	{
		return &sm_Environment;
	}

	static naOcclusionManager *GetOcclusionManager()
	{
		return &sm_OcclusionManager;
	}

	static audController *GetAudioController()
	{
		return &sm_AudioController;
	}

	static audGunFireAudioEntity* GetGunFireAudioEntity()
	{
		return &sm_GunFireAudioEntity;
	}
	static audDynamicMixer& GetDynamicMixer()
	{
		return sm_DynamicMixer;
	}
	static audSuperConductor& GetSuperConductor()
	{
		return sm_SuperConductor;
	}

	static u32 GetTimeInMsSinceSpecialAbilityWasActive()
	{
		return g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - sm_LastTimeSpecialAbilityActive;
	}
	
	static f32 GetFrontendRadioVolumeLin()
	{
		return sm_FrontendRadioVolumeLin;
	}

	static bool IsInNetworkLobby()
	{
		return sm_IsInNetworkLobby;
	}

	static bool IsRenderingHoodMountedVehicleCam()
	{
		return sm_IsRenderingHoodMountedVehicleCam;
	}

	static bool IsRenderingFirstPersonVehicleCam()
	{
		return sm_IsRenderingFirstPersonVehicleCam;
	}

	static bool IsRenderingFirstPersonTurretCam()
	{
		return sm_IsRenderingFirstPersonVehicleTurretCam;
	}
	
	static bool IsRenderingFirstPersonVehicleTurretCam()
	{
		return sm_IsRenderingFirstPersonVehicleTurretCam;
	}

#if GTA_REPLAY
    static bool IsRenderingReplayFreeCamera()
    {
        return sm_IsRenderingReplayFreeCamera;
    }
#endif

	static void SetInNetworkLobby(const bool isInNetworkLobby)
	{
		sm_IsInNetworkLobby = isInNetworkLobby;
	}

	static void SetLobbyMuteOverride(const bool overrideMute)
	{
		sm_OverrideLobbyMute = overrideMute;
	}

	template <class _T> static _T *GetObject(const char *name)
	{
		return sm_MetadataMgr.GetObject<_T>(name);
	}

	template <class _T> static _T *GetObject(const u32 nameHash)
	{
		return sm_MetadataMgr.GetObject<_T>(nameHash);
	}

	template <class _T> static _T *GetObject(const audMetadataRef metadataReference)
	{
		return sm_MetadataMgr.GetObject<_T>(metadataReference);
	}

	static audMetadataManager &GetMetadataManager()
	{
		return sm_MetadataMgr;
	}

	static bool IsScreenFadedOut();

	//PURPOSE:
	//Mutes specified category of sounds
	//PARAMS:
	//mute - 'true' turns mute on, 'false' turns mute off
	
	static void MuteAmbience(bool mute);
	static void MuteCutscene(bool mute);
	static void MuteFrontend(bool mute);
	static void MuteMusic(bool mute);
	static void MuteSfx(bool mute);
	static void MuteSpeech(bool mute);
	static void MuteGuns(bool mute);
	static void MuteRadio(bool mute);
	static void MuteVehicles(bool mute);
	static void MuteCategory(audScene ** scene, const char * settings, bool mute);

	static void ToggleMuteAmbience();
	static void ToggleMuteCutscene();
	static void ToggleMuteFrontend();
	static void ToggleMuteMusic();
	static void ToggleMuteRadio();
	static void ToggleMuteSfx();
	static void ToggleMuteSpeech();
	static void ToggleMuteGuns();
	static void ToggleMuteVehicles();
	static void ToggleStartCarJumpScene();

	static bool GetIsAmbienceMuted() { return (sm_AmbienceMuted!=NULL); }
	static bool GetIsCutsceneMuted() { return (sm_CutsceneMuted!=NULL); }
	static bool GetIsFrontendMuted() { return (sm_FrontendMuted!=NULL); }
	static bool GetIsMusicMuted() { return (sm_MusicMuted!=NULL); }
	static bool GetIsRadioMuted() { return (sm_RadioMuted!=NULL); }
	static bool GetIsSfxMuted() { return (sm_SfxMuted!=NULL); }
	static bool GetIsSpeechMuted() { return (sm_SpeechMuted!=NULL); }
	static bool GetIsGunsMuted() { return (sm_GunsMuted!=NULL); }
	static bool GetIsVehiclesMuted() { return (sm_VehiclesMuted!=NULL); }

	static void ActivateSlowMoMode(u32 settingsHash);
	static void DeactivateSlowMoMode(u32 settingsHash);
	static void DeactivateSlowMoMode(SlowMoType priority);

	static bool IsTransitioningOutOfPauseMenu() { return sm_WaitingForPauseMenuSlowMoToEnd; }

	//Temporary functions pending the profile saving gubbins being moved into rage
	static void AmbisonicSave();
	static void AmbisonicLoad();
	static void AmbisonicDraw(ambisonicDrawData data[90]);

	static float GetGameTimeHours() { return sm_GameTimeHours; }

	static void StartLongPlayerSwitch(const CPed &oldPed, const CPed &newPed);
	static void StopLongPlayerSwitch();

	static bool IsPlayerSpecialAbilityFadingOut() { return sm_IsPlayerSpecialAbilityFadingOut;};

	static bool ShouldTriggerPulseHeadset() 
	{ 
#if RSG_PS3 || RSG_ORBIS
			return sm_ShouldTriggerPulseHeadset; 
#else
		return false;
#endif
	}


#if __BANK
	static void UpdateAuditionSound();
	static void UpdateRAVEVariables();	
#endif

#if !__FINAL
static bool ShouldAddStrVisMarkers() { return sm_ShouldAddStrVisMarkers; }
#endif

#if 0 // __BANK
	//For debugging a sound from code: NB best not use with the rag audition, do one or the other
	static void SetDebugSound(audSound * sound)
	{
		sm_DebugSound = sound;
	}
#endif

	static void MuteGameWorldAndPositionedRadioForTv(bool mute);

	static bool IsCutsceneActive() { return sm_IsCutsceneActive; }
	static bool IsAFirstPersonCameraActive(const CPed* ped, const bool bCheckStrafe = true, const bool bIncludeClone = false, const bool bDisableForDominantScriptedCams = false, const bool bDisableForDominantCutsceneCams = true, const bool bCheckFlags = true);
	static bool IsFirstPersonActive() { return sm_IsFirstPersonActiveForPlayer; }

#if GTA_REPLAY
	static void LoadReplayAudioBanks();
	static bool PumpReplayAudio();
	static void ShutdownReplayEditor();
	static void StopAllSounds( bool saveMusic = false )
	{
		replayAssert(!audNorthAudioEngine::IsAudioUpdateCurrentlyRunning());
		
		(void)saveMusic;
		sm_AudioController.StopAllSounds();
	}

	static audCurve sm_ReplayTimeWarpToPitch;
	static audCurve sm_ReplayTimeWarpToTimeScale;
	static bool sm_AreReplayBanksLoaded;

#endif

	static void NotifyLocalPlayerArrested();
	static void NotifyLocalPlayerDied();
	static void NotifyLocalPlayerPlaneCrashed();
	static void NotifyLocalPlayerBailedFromAircraft();

	static bool IsAudioUpdateCurrentlyRunning() { return sm_IsAudioUpdateCurrentlyRunning; }

	static void StartPauseMenuSlowMo();
	static void StopPauseMenuSlowMo();

#if RSG_PS3 || RSG_ORBIS
	static bool IsWirelessHeadsetConnected() { return sm_IsWirelessHeadsetConnected; }
#endif

#if RSG_PS3
	static void CheckForWirelessHeadset(const bool loadPRX = false);
#endif

	static bool InitClass();

	static void OnLoadingTuneStopped() { sm_ApplyMusicSlider = true; }

	static bool HasLoadedMPDataSet() { return sm_DataSetState == Loaded_MP; }
	static bool HasLoadedSPDataSet() { return sm_DataSetState == Loaded_SP; }
	static bool IsMPDataRequested()  { return sm_IsMPDataRequested; }
	static void ApplyARC1BankRemappings();

private:

    static void ShutdownClass();

	static void RegisterDataFileMounters();
	
	static void UpdateDataSet();
	static void LoadSPData();
	static void LoadMPData();
	static void UnloadSPData();
	static void UnloadMPData();

	static void LoadMapData();
	static void UnloadMapData();

	static void RegisterGameObjectMetadataUnloading(const audMetadataChunk& chunk);

#if !__FINAL
	static void GenerateMemoryReport(const char *fileName);
#endif

	static void *GetObject(const u32 nameHash, const u32 TYPE_ID);

	static void AudioUpdateThread(void*);	

	static void UpdateCategories();

	static audMetadataDataFileMounter *sm_SoundDlcMounter;
	static audMetadataDataFileMounter *sm_DynamicMixDlcMounter;
	static audMetadataDataFileMounter  *sm_GameObjectDlcMounter;
	static audMetadataDataFileMounter  *sm_CurveDlcMounter;
	static audMetadataDataFileMounter  *sm_SynthDlcMounter;

	static naMicrophones sm_Microphones;

	static naAnimHandler sm_AudAnimHandler;

	static audMetadataManager sm_MetadataMgr;
	static audGameObjectManager sm_GameObjectMgr;

	static u32 sm_LastLoudSoundTime;
	static audController sm_AudioController;
	static naEnvironment sm_Environment;
	static naEnvironmentGroupManager sm_EnvironmentGroupManager;
	static naOcclusionManager sm_OcclusionManager;
	static audGunFireAudioEntity sm_GunFireAudioEntity;
	static audSuperConductor sm_SuperConductor;
	
	static audDynamicMixer sm_DynamicMixer;

	static audSmoother sm_TimeWarpSmoother;
	static audCurve sm_TimeWarpToPitch;
	static audCurve sm_TimeWarpToTimeScale;

	static u32 sm_PauseMenuSlowMoCount;

	static audCurve sm_BuiltUpToWeaponTails;

	static bool sm_Paused;
	static bool sm_PausedLastFrame;

	static u32 sm_TimeInMs; // pauses when paused
	static u32 sm_LastTimeInMs; // pauses when paused

	static u32 sm_StreamingSoundBucketId;

	static audSmoother sm_RadioDuckingSmoother;
	static audSmoother sm_OverlayedDJDuckingSmoother;
	static audSmoother sm_GPSDuckingSmoother;
	static audSmoother sm_TrainRolloffInSubwaySmoother;
	static f32 sm_FrontendRadioVolumeLin;
	static u32 sm_LastTimeSpecialAbilityActive;

	static sysIpcSema sm_RunUpdateSema, sm_UpdateFinishedSema;
	static sysIpcThreadId sm_UpdateThread;
	static bool sm_RunUpdateInSeperateThread;

	static char *sm_ExtraContentPath;

	static bool sm_IsInSlowMo;
	static bool sm_ForceSlowMoVideoEditor;
	static bool sm_ForceSuperSlowMoVideoEditor;

	static bool sm_WaitingForPauseMenuSlowMoToEnd;
	static SlowMoType sm_SlowMoMode;
	static u32 sm_ActiveSlowMoModes[NUM_SLOWMOTYPE];
	static bool sm_IsInNetworkLobby;
	static bool sm_IsRenderingHoodMountedVehicleCam;    
	static bool sm_IsRenderingFirstPersonVehicleCam;
	static bool sm_IsRenderingFirstPersonVehicleTurretCam;
    REPLAY_ONLY(static bool sm_IsRenderingReplayFreeCamera;)
	static bool sm_OverrideLobbyMute;
	static volatile bool sm_IsShuttingDown;

	static bool sm_ApplyMusicSlider;

	static audScene * sm_NorthAudioMixScene;
	static audScene * sm_MuteAllScene;
	static audScene * sm_DeathScene;
	static audScene * sm_MpLobbyScene;
	static audScene * sm_ReplayEditorScene;
	static audScene * sm_TvScene;
	static audScene * sm_FrontendScene;
	static audScene * sm_GameWorldScene;
	static audScene * sm_PosRadioMuteScene;
	static audScene * sm_SlowMoScene;
	static audScene * sm_SlowMoVideoEditorScene;
	static audScene * sm_SuperSlowMoVideoEditorScene;
	static audScene * sm_GameSuspendedScene;
	static audScene * sm_FirstPersonModeScene;


	static audScene * sm_AmbienceMuted;
	static audScene * sm_CutsceneMuted;
	static audScene * sm_FrontendMuted;
	static audScene * sm_MusicMuted;
	static audScene * sm_RadioMuted;
	static audScene * sm_SfxMuted;
	static audScene * sm_SpeechMuted;
	static audScene * sm_GunsMuted;
	static audScene * sm_VehiclesMuted;
	static audScene * sm_PlayerInVehScene;
	static audScene * sm_SpeechScene;

	static audDynMixPatch * sm_SpeechGunfirePatch;
	static audDynMixPatch * sm_SpeechScorePatch;
	static audDynMixPatch * sm_SpeechVehiclesPatch;
	static audDynMixPatch * sm_SpeechVehiclesFirstPersonPatch;
	static audDynMixPatch * sm_SpeechWeatherPatch;

	static audSound * sm_SlowMoSound;

	static audSmoother sm_PIVMusicVolumeSmoother;
	static audSmoother sm_PIVRadioVolumeSmoother;
	static audSmoother sm_PIVOpennessSmoother;
	
	static f32 sm_PlayerVehicleOpenness;
	static f32 sm_EngineVolume;
	static f32 sm_MusicVolume;
	static f32 sm_SfxVolume;
	static Vec3V sm_PedPosLastFrame;

	static u32 sm_LoadingScreenSFXSliderDelay;

	static float sm_GameTimeHours;

	static f32 sm_ThirdPersonCameraBlendThreshold;
	static u32 sm_TimeSpeechSceneLastStarted;
	static u32 sm_TimeSpeechSceneLastStopped;
	static f32 sm_SpeechGunfirePatchApplyAtStart;
	static f32 sm_SpeechScorePatchApplyAtStart;
	static f32 sm_SpeechVehiclesPatchApplyAtStart;
	static f32 sm_SpeechVehiclesFirstPersonPatchApplyAtStart;
	static f32 sm_SpeechWeatherPatchApplyAtStart;
	static f32 sm_SpeechGunfirePatchApplyAtStop;
	static f32 sm_SpeechScorePatchApplyAtStop;
	static f32 sm_SpeechVehiclesPatchApplyAtStop;
	static f32 sm_SpeechVehiclesFirstPersonPatchApplyAtStop;
	static f32 sm_SpeechWeatherPatchApplyAtStop;
	static bool sm_SpeechSceeneApplied;
	static bool sm_StartingNewGame;
	static bool sm_CinematicThirdPersonAimCameraActive;
	static bool sm_IsFirstPersonActiveForPlayer;
	static bool sm_IsCutsceneActive;

	static const audCategory *sm_ScoreProxyCat;
	static const audCategory *sm_RadioProxyCat;
	static audCategoryController *sm_ScoreVolCategoryController;
	static audCategoryController *sm_OneShotVolCategoryController;	
	static audCategoryController *sm_MusicSliderController;
	static audCategoryController *sm_SFXSliderController;
	static audCategoryController *sm_NoFadeVolCategoryController;
	static audCategoryController *sm_ScriptedOverrideFadeVolCategoryController;

#if GTA_REPLAY
	static bool sm_EnableHPF;
	static bool sm_EnableLPF;
	static bool sm_UseMonoReverbs;
	static float sm_AudioFrameTime;
	static bool sm_VehicleEffectsBypass;
	static bool sm_UseSingleListener;
#endif

	static audScene *sm_StonedScene;

#if RSG_PS3 || RSG_ORBIS
	static audScene *sm_PulseHeadsetScene;
	static bool sm_ShouldTriggerPulseHeadset;
	static bool sm_IsWirelessHeadsetConnected;
	static s32 sm_PreviousOutputPref;
#endif

	static audSimpleSmoother sm_CutsceneLeakageSmoother;
	static bool sm_IsFadedToBlack;
	static bool sm_ScreenFadedOutThisFrame;
	static bool sm_IsPlayerSpecialAbilityFadingOut;
	static bool sm_HasAppliedARC1BankRemappings;
	
	enum DataSetState
	{
		Loaded_SP = 0,
		Stopping_SP,
		Loaded_MP,
		Stopping_MP
	};
	static DataSetState sm_DataSetState;
	static bool sm_IsMPDataRequested;
	static u32 sm_DataSetStateCount;
	static u8 *sm_SPSoundDataPtr;
	static u32 sm_SPSoundDataSize;

	//static u8 *sm_ReplayedSoundDataPtr;
	//static u32 sm_ReplayedSoundDataSize;

	static volatile bool sm_IsAudioUpdateCurrentlyRunning;
#if __BANK
	static void TriggerSound();
	static void StopSound();
	static void DrawBufferedLevelMeters();
	static void CreateWidgets();
	static void TriggerSoundFromWidget();
	static void StopSoundFromWidget();
	static void AuditionSound(const u32 soundNameHash);
	static void AuditionSound(const audMetadataRef soundRef);
	static u32 sm_CurrentMeterIndex;
	static audMeterList *sm_MeterList[g_MaxBufferedMeters];
	static audSound *sm_DebugSound;
	static char sm_SoundName[64];
	static bool sm_ShouldRemoveSoundHierarchy;

	static bool sm_ShouldAuditionThroughFocusEntity;
	static bool sm_ShouldAuditionThroughPlayer;
	static bool sm_ShouldAuditionPannedFrontend;
	static bool sm_ShouldAuditionSoundOverNetwork;

	static float sm_DebugDrawYOffset;
	static bool sm_AuditionSoundUnpausable;
	static bool sm_AllowAuditionSoundToLoad;
	static bool sm_AuditionSoundsOnPPU;
	static bool sm_ForceBaseCategory;
	static audRemoteControl::AuditionSoundDrawMode sm_AuditionSoundDrawMode;
	static char sm_AuditionSoundSetName[128];
	static char sm_AuditionSoundName[128];

#endif

	NOTFINAL_ONLY(static bool sm_ShouldAddStrVisMarkers);
};

#if __BANK
class audNorthDebugDrawManager : public audDebugDrawManager
{
public:

	audNorthDebugDrawManager(const float startX, const float startY, const float clipMinY, const float clipMaxY)
		: m_X(startX)
		, m_Y(startY)
		, m_MinClipY(clipMinY)
		, m_MaxClipY(clipMaxY)
		, m_OffsetY(0.f)
	{
		ResetColour();
	}

	virtual void PushSection(const char *title)
	{
		const float offsetY = m_Y - m_OffsetY;
		if(offsetY >= m_MinClipY && offsetY < m_MaxClipY)
		{

			grcFont::GetCurrent().SetInternalColor(m_Colour);

			grcDrawSingleQuadf(m_X, offsetY, m_X + grcFont::GetCurrent().GetStringWidth(title, (int)strlen(title)), offsetY+grcFont::GetCurrent().GetHeight(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Color32(60, 20, 20, 200));

			grcFont::GetCurrent().Drawf(m_X, offsetY, title);
		}
		m_Y += lineHeight;
		m_X += tabWidth;
	}

	virtual void PopSection()
	{
		m_X -= tabWidth;
		Assert(m_X >= 0.f);
	}

	virtual void DrawLine(const char *text)
	{
		const float offsetY = m_Y - m_OffsetY;
		if(offsetY >= m_MinClipY && offsetY < m_MaxClipY)
		{ 
			grcDrawSingleQuadf(m_X, offsetY, m_X + grcFont::GetCurrent().GetStringWidth(text, (int)strlen(text)), offsetY+grcFont::GetCurrent().GetHeight(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Color32(0, 0, 0, 140));

			grcFont::GetCurrent().SetInternalColor(m_Colour);
			
			grcFont::GetCurrent().Drawf(m_X, offsetY, text);
		}
		m_Y += lineHeight;
	}

	void SetOffsetY(const float yOffset)
	{
		m_OffsetY = yOffset;
	}

	void SetColour(Color32 colour)
	{
		m_Colour = colour;
	}

	void ResetColour()
	{
		m_Colour = Color32(200, 200, 200);
	}

private:

	enum { lineHeight = 10 };
	enum { tabWidth = 20 };

	Color32 m_Colour;

	float m_X;
	float m_Y;

	float m_MinClipY;
	float m_MaxClipY;
	float m_OffsetY;
};
#endif

extern RegdEnt g_AudioDebugEntity;
#endif // AUD_NORTHAUDIOENGINE_H
