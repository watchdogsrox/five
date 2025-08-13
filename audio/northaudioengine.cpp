//
// audio/northaudioengine.cpp
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//


#include "northaudioengine.h"

//#include "audiogeometry.h"
#include "pedaudioentity.h"
#include "pedscenariomanager.h"
#include "glassaudioentity.h"
#include "ambience/ambientaudioentity.h"
#include "ambience/audambientzone.h"
#include "ambience/water/audshorelinePool.h"
#include "ambience/water/audshorelineRiver.h"
#include "ambience/water/audshorelineOcean.h"
#include "ambience/water/audshorelineLake.h"
#include "environment/audwarpmanager.h"
#include "vehiclereflectionsaudioentity.h"
#include "explosionaudioentity.h"
#include "emitteraudioentity.h"
#include "fireaudioentity.h"
#include "frontendaudioentity.h"
#include "collisionaudioentity.h"
#include "cutsceneaudioentity.h"
#include "dynamicmixer.h"
#include "radioaudioentity.h"
#include "scriptaudioentity.h"
#include "speechaudioentity.h"
#include "speechmanager.h"
#include "weaponaudioentity.h"
#include "vehicleaudioentity.h"
#include "heliaudioentity.h"
#include "trainaudioentity.h"
#include "caraudioentity.h"
#include "planeaudioentity.h"
#include "bicycleaudioentity.h"
#include "traileraudioentity.h"
#include "boataudioentity.h"
#include "vehiclefireaudio.h"
#include "frontend/NewHud.h"
#include "vehicleengine/vehicleengine.h"
#include "vehicleengine/electricengine.h"
#include "vehicleengine/turbo.h"
#include "vehicleengine/transmission.h"
#include "weatheraudioentity.h" 
#include "audio/environment/environment.h"
#include "policescanner.h"
#include "music/musicplayer.h"
#include "replayaudioentity.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"
#include "control/stuntjump.h"
#include "core/game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debug.h"
#include "debug/debugscene.h"
#include "grcore/debugdraw.h"
#include "game/clock.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "input/mouse.h"
#include "scene/DataFileMgr.h"
#include "scene/FileLoader.h"
#include "scene/RefMacros.h"
#include "system/system.h"
#include "audiodata/simpletypes.h"
#include "audioeffecttypes/delayeffect.h"
#include "audioeffecttypes/filterdefs.h"
#include "audioengine/categorymanager.h"
#include "audioengine/categorycontrollermgr.h"
#include "audioengine/curverepository.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/environment.h"
#include "audioengine/soundfactory.h"
#include "audioengine/soundmanager.h"
#include "audiohardware/channel.h"
#include "audiohardware/config.h"
#include "audiohardware/grainplayer.h"
#include "audiohardware/device_360.h"
#include "audioengine/dynamicmixmgr.h"
#include "audiohardware/device.h"
#include "audiohardware/device_ps3.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverutil.h"
#include "audiohardware/device_xaudio_pc.h"
#include "audiosoundtypes/speechsound.h"
#include "audiosoundtypes/environmentsound.h"
#include "audiosynth/synthesizer.h"
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "modelinfo/MloModelInfo.h"
#include "parser/manager.h"
#include "profile/profiler.h"
#include "renderer/PostProcessFX.h"
#include "renderer/water.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/scene.h"
#include "system/ControlMgr.h"
#include "system/memory.h"
#include "system/param.h"
#include "system/performancetimer.h"
#include "system/simpleallocator.h"
#include "peds/ped.h"
#include "vfx/VisualEffects.h"
#include "net/nethardware.h"
#include "frontend/MobilePhone.h"
#include "system/ThreadPriorities.h"
#include "system/memvisualize.h"
#include "streaming/streamingvisualize.h"
#include "video/VideoPlayback.h"

#include "debugaudio.h"

#include "grprofile/pix.h"

#include "Network/Commerce/CommerceManager.h"
#include "Network/Live/livemanager.h"
#include "Network/Network.h"
#include "Network/Voice/NetworkVoice.h"

#if RSG_ORBIS
#include "audiohardware/device_orbis.h"
#include "rline/rlnp.h"
#endif

#if GTA_REPLAY
#include "replaycoordinator/ReplayCoordinator.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "replayaudioentity.h"
#endif

#if RSG_PC
#include "audiohardware/device_xaudio_pc.h"

#if __BANK && RSG_PC
bool g_ShowAudioFrameDebugInfo;
u32 g_NumberOfMixBuffers = rage::kXAudioNumberOfBuffers;
bool g_OverrideCPULimitedAudioSetting = false;
bool g_ForceMinSpecMode = false;

#endif

#endif

AUDIO_OPTIMISATIONS()

extern bool g_UseInteriorCarFilter;

bool g_HasInitialisedStaticClasses = false;
static bool s_HasScanStarted;

static const char *g_AudioConfigPath = "audio:/config/";
// PURPOSE
//	Specifies the distance relative to camera that RegisterLoundSound() responds to
static const f32 g_audLoudSoundDistanceMag = 40.f;

char * audNorthAudioEngine::sm_ExtraContentPath = NULL;
naAnimHandler audNorthAudioEngine::sm_AudAnimHandler;
#if __BANK

static bool g_ShouldMuteWind = false;
static audCategoryController * g_WindMuteController = NULL;
bool g_ForceDefaultCutsceneSettings = false;
bool g_ForceAllowRadioOverScreenFade = false;
extern bool g_DebugPlayingScriptSounds;
extern bool g_GameTimerCutscenes;
bool g_WarpToStaticEmitter = false;
bool g_WarpToAmbientZone = false;
bool g_WarpToInterior = false;
const StaticEmitter *g_RequestedWarpEmitter = NULL;
const AmbientZone *g_RequestedWarpAmbZone = NULL;
const CInteriorProxy *g_RequestedWarpInterior = NULL;
bool g_IgnoreFrontendVolumeSliders = false;

bool g_DisplaySlowMo = false;

#endif

namespace rage
{
	extern float g_SFXHPFCutoff;
	extern bool g_SFXHPFBypass;
}

#if !__FINAL
bool audNorthAudioEngine::sm_ShouldAddStrVisMarkers = false;
class naStreamingDebugInterface : public audStreamingDebugInterface
{
public:
	void SetMarker(const char *text) const
	{
		if(audNorthAudioEngine::ShouldAddStrVisMarkers())
		{
			STRVIS_SET_MARKER_TEXT(text);
		}
	}
};
#endif

volatile bool audNorthAudioEngine::sm_IsAudioUpdateCurrentlyRunning = false;

audMetadataDataFileMounter *audNorthAudioEngine::sm_SoundDlcMounter;
audMetadataDataFileMounter *audNorthAudioEngine::sm_DynamicMixDlcMounter;
audMetadataDataFileMounter  *audNorthAudioEngine::sm_GameObjectDlcMounter;
audMetadataDataFileMounter  *audNorthAudioEngine::sm_CurveDlcMounter;
audMetadataDataFileMounter  *audNorthAudioEngine::sm_SynthDlcMounter;

#if NA_RADIO_ENABLED
static f32 g_RadioDuckingVolume		= -7.0f;
static f32 g_RadioDuckingVolumeStereo		= -7.0f;
static f32 g_RadioDuckingVolumeVoiceChat		= -18.0f;
static f32 g_TalkRadioDuckingVolume = -12.0f;
static f32 g_RadioNonMusicAdditionalDucking = -3.f;
BANK_ONLY(static f32 g_RadioGPSDuckingVolume	= -3.0f);
static bool sm_ShouldDuckRadio		= false;
#endif

static f32 g_RadioDuckingSmoothRate	= 0.001f;
static f32 g_RadioDuckingSmoothRateUp	= 0.0003f;

bool audNorthAudioEngine::sm_IsPlayerSpecialAbilityFadingOut = false;

audNorthAudioEngine::DataSetState audNorthAudioEngine::sm_DataSetState = audNorthAudioEngine::Loaded_SP;
bool audNorthAudioEngine::sm_IsMPDataRequested = false;
u32 audNorthAudioEngine::sm_DataSetStateCount = 0;
u8 *audNorthAudioEngine::sm_SPSoundDataPtr = NULL;
u32 audNorthAudioEngine::sm_SPSoundDataSize = 0;

//u8 *audNorthAudioEngine::sm_ReplayedSoundDataPtr = NULL;
//u32 audNorthAudioEngine::sm_ReplayedSoundDataSize = 0;


#if GTA_REPLAY
bool audNorthAudioEngine::sm_EnableHPF = true;
bool audNorthAudioEngine::sm_EnableLPF = true;
bool audNorthAudioEngine::sm_UseMonoReverbs = false;
float audNorthAudioEngine::sm_AudioFrameTime = 15.0f;
bool audNorthAudioEngine::sm_VehicleEffectsBypass = false;
bool audNorthAudioEngine::sm_UseSingleListener = false;
#endif

u32 audNorthAudioEngine::sm_StreamingSoundBucketId = ~0U;

bank_float g_SlowMoTrainVol = -9.f;
bank_float g_ConversationTrainVol = -0.f;
bank_float g_ConversationHeliVol = -6.f;

f32 g_FadeValueForSilence = 1.f;
bool g_DebugFadeLevels = false;

bool audNorthAudioEngine::sm_ApplyMusicSlider = false;

audSimpleSmoother audNorthAudioEngine::sm_CutsceneLeakageSmoother;
f32 g_CutsceneLeakageSmootherRate = 1.0f;
f32 g_CutscenePositionedLeakage = 0.75f;
bool g_LeakForRPICutscenes = true;
BANK_ONLY(bool g_DebugCutsceneLeakage = false;)

bool g_MuteGameWorldAudio = false;
bool g_MutePositionedRadio = false;
bool g_MuteGameWorldAndPositionedRadioForTV = false;

bool g_AudioInitialised = false;

f32 g_AttachedToTrainVolume = -9.0f;
f32 g_TrainRolloffInSubways = 2.5f;

bool audNorthAudioEngine::sm_IsInNetworkLobby = false;
bool audNorthAudioEngine::sm_IsRenderingHoodMountedVehicleCam = false;
bool audNorthAudioEngine::sm_IsRenderingFirstPersonVehicleCam = false;
bool audNorthAudioEngine::sm_IsRenderingFirstPersonVehicleTurretCam = false;
REPLAY_ONLY(bool audNorthAudioEngine::sm_IsRenderingReplayFreeCamera = false;)
bool audNorthAudioEngine::sm_OverrideLobbyMute = false;
bool audNorthAudioEngine::sm_StartingNewGame = false;
bool audNorthAudioEngine::sm_CinematicThirdPersonAimCameraActive = false;
bool audNorthAudioEngine::sm_IsFirstPersonActiveForPlayer = false;
bool audNorthAudioEngine::sm_IsCutsceneActive = false;

audMetadataManager audNorthAudioEngine::sm_MetadataMgr;
audGameObjectManager audNorthAudioEngine::sm_GameObjectMgr;

float g_PIVMusicVolumeFadeInS = 1.0f;
float g_PIVMusicVolumeFadeOutS = 2.0f;
float g_PIVOpennessFadeInS = 0.1f;
float g_PIVOpennessFadeOutS = 5.0f;
audSmoother audNorthAudioEngine::sm_PIVMusicVolumeSmoother;
audSmoother audNorthAudioEngine::sm_PIVRadioVolumeSmoother;
audSmoother audNorthAudioEngine::sm_PIVOpennessSmoother;

#if __BANK
char audNorthAudioEngine::sm_SoundName[64];
char g_AuditionWaveSlotName[64];
char g_SlowMoModeName[64] = {0};
bool audNorthAudioEngine::sm_ShouldRemoveSoundHierarchy = true;

bool audNorthAudioEngine::sm_ShouldAuditionThroughFocusEntity = false;
bool audNorthAudioEngine::sm_ShouldAuditionThroughPlayer = false;
bool audNorthAudioEngine::sm_ShouldAuditionPannedFrontend = false;
bool audNorthAudioEngine::sm_ShouldAuditionSoundOverNetwork = false;

float audNorthAudioEngine::sm_DebugDrawYOffset = 0.f;
bool audNorthAudioEngine::sm_AuditionSoundUnpausable = true;
bool audNorthAudioEngine::sm_AllowAuditionSoundToLoad = true;
bool audNorthAudioEngine::sm_AuditionSoundsOnPPU = false;
bool audNorthAudioEngine::sm_ForceBaseCategory = false;
audRemoteControl::AuditionSoundDrawMode audNorthAudioEngine::sm_AuditionSoundDrawMode = audRemoteControl::kVariablesOnly;
char audNorthAudioEngine::sm_AuditionSoundSetName[128] = {0};
char audNorthAudioEngine::sm_AuditionSoundName[128] = {0};
#endif

audController audNorthAudioEngine::sm_AudioController;
naEnvironment audNorthAudioEngine::sm_Environment;
naEnvironmentGroupManager audNorthAudioEngine::sm_EnvironmentGroupManager;
naOcclusionManager audNorthAudioEngine::sm_OcclusionManager;
audGunFireAudioEntity audNorthAudioEngine::sm_GunFireAudioEntity;
audSuperConductor audNorthAudioEngine::sm_SuperConductor;
audDynamicMixer audNorthAudioEngine::sm_DynamicMixer;
u32 audNorthAudioEngine::sm_LastLoudSoundTime = 0;
bool audNorthAudioEngine::sm_ShouldMuteAudio = false;
f32 audNorthAudioEngine::sm_EngineVolume = 0.0f;
f32 audNorthAudioEngine::sm_PlayerVehicleOpenness = 1.0f;

//Matrix34 audNorthAudioEngine::sm_ListenerMatrix;
bool audNorthAudioEngine::sm_Paused = false;
bool audNorthAudioEngine::sm_PausedLastFrame = false;
u32 audNorthAudioEngine::sm_TimeInMs = 0;
u32 audNorthAudioEngine::sm_LastTimeInMs = 0;
audSmoother audNorthAudioEngine::sm_RadioDuckingSmoother;
audSmoother audNorthAudioEngine::sm_GPSDuckingSmoother;
audSmoother audNorthAudioEngine::sm_TrainRolloffInSubwaySmoother;
f32 audNorthAudioEngine::sm_FrontendRadioVolumeLin;
u32 audNorthAudioEngine::sm_LastTimeSpecialAbilityActive = 0;
Vec3V audNorthAudioEngine::sm_PedPosLastFrame;

float audNorthAudioEngine::sm_GameTimeHours = 0.f;

audSmoother audNorthAudioEngine::sm_TimeWarpSmoother;
#if GTA_REPLAY
audCurve audNorthAudioEngine::sm_ReplayTimeWarpToPitch;
audCurve audNorthAudioEngine::sm_ReplayTimeWarpToTimeScale;
bool audNorthAudioEngine::sm_AreReplayBanksLoaded = false;
#endif
audCurve audNorthAudioEngine::sm_TimeWarpToPitch;
audCurve audNorthAudioEngine::sm_TimeWarpToTimeScale;
audCurve g_TimeWarpToDelayFeedback;
sysIpcSema audNorthAudioEngine::sm_RunUpdateSema;
sysIpcSema audNorthAudioEngine::sm_UpdateFinishedSema;
sysIpcThreadId audNorthAudioEngine::sm_UpdateThread;
bool audNorthAudioEngine::sm_RunUpdateInSeperateThread = true;
bool audNorthAudioEngine::sm_IsInSlowMo = false;
bool audNorthAudioEngine::sm_ForceSlowMoVideoEditor = false;
bool audNorthAudioEngine::sm_ForceSuperSlowMoVideoEditor = false;
bool audNorthAudioEngine::sm_WaitingForPauseMenuSlowMoToEnd = false;
SlowMoType audNorthAudioEngine::sm_SlowMoMode = AUD_SLOWMO_GENERAL;
u32 audNorthAudioEngine::sm_ActiveSlowMoModes[NUM_SLOWMOTYPE] = {0};

audScene * audNorthAudioEngine::sm_AmbienceMuted = NULL;
audScene * audNorthAudioEngine::sm_CutsceneMuted = NULL;
audScene * audNorthAudioEngine::sm_FrontendMuted = NULL;
audScene * audNorthAudioEngine::sm_MusicMuted = NULL;
audScene * audNorthAudioEngine::sm_RadioMuted = NULL;
audScene * audNorthAudioEngine::sm_SfxMuted = NULL;
audScene * audNorthAudioEngine::sm_SpeechMuted = NULL;
audScene * audNorthAudioEngine::sm_GunsMuted = NULL;
audScene * audNorthAudioEngine::sm_VehiclesMuted = NULL;

audScene * audNorthAudioEngine::sm_NorthAudioMixScene = NULL;
audScene * audNorthAudioEngine::sm_MuteAllScene = NULL;
audScene * audNorthAudioEngine::sm_DeathScene = NULL;
audScene * audNorthAudioEngine::sm_MpLobbyScene = NULL;
audScene * audNorthAudioEngine::sm_ReplayEditorScene = NULL;
audScene * audNorthAudioEngine::sm_TvScene = NULL;
audScene * audNorthAudioEngine::sm_FrontendScene = NULL;
audScene * audNorthAudioEngine::sm_GameWorldScene = NULL;
audScene * audNorthAudioEngine::sm_PosRadioMuteScene = NULL;
audScene * audNorthAudioEngine::sm_SlowMoScene = NULL;
audScene * audNorthAudioEngine::sm_SlowMoVideoEditorScene = NULL;
audScene * audNorthAudioEngine::sm_SuperSlowMoVideoEditorScene = NULL;
audScene * audNorthAudioEngine::sm_PlayerInVehScene = NULL;
audScene * audNorthAudioEngine::sm_SpeechScene = NULL;
audScene * audNorthAudioEngine::sm_GameSuspendedScene = NULL;
audSound * audNorthAudioEngine::sm_SlowMoSound = NULL;
audScene * audNorthAudioEngine::sm_FirstPersonModeScene = NULL;

u32 audNorthAudioEngine::sm_TimeSpeechSceneLastStarted = 0;
u32 audNorthAudioEngine::sm_TimeSpeechSceneLastStopped = 0;
f32 audNorthAudioEngine::sm_SpeechGunfirePatchApplyAtStart = 0.0f;
f32 audNorthAudioEngine::sm_SpeechScorePatchApplyAtStart = 0.0f;
f32 audNorthAudioEngine::sm_SpeechVehiclesPatchApplyAtStart = 0.0f;
f32 audNorthAudioEngine::sm_SpeechVehiclesFirstPersonPatchApplyAtStart = 0.0f;
f32 audNorthAudioEngine::sm_SpeechWeatherPatchApplyAtStart = 0.0f;
f32 audNorthAudioEngine::sm_SpeechGunfirePatchApplyAtStop = 0.0f;
f32 audNorthAudioEngine::sm_SpeechScorePatchApplyAtStop = 0.0f;
f32 audNorthAudioEngine::sm_SpeechVehiclesPatchApplyAtStop = 0.0f;
f32 audNorthAudioEngine::sm_SpeechVehiclesFirstPersonPatchApplyAtStop = 0.0f;
f32 audNorthAudioEngine::sm_SpeechWeatherPatchApplyAtStop = 0.0f;
bool audNorthAudioEngine::sm_SpeechSceeneApplied = false;

audDynMixPatch * audNorthAudioEngine::sm_SpeechGunfirePatch=NULL;
audDynMixPatch * audNorthAudioEngine::sm_SpeechScorePatch=NULL;
audDynMixPatch * audNorthAudioEngine::sm_SpeechVehiclesPatch=NULL;
audDynMixPatch * audNorthAudioEngine::sm_SpeechVehiclesFirstPersonPatch=NULL;
audDynMixPatch * audNorthAudioEngine::sm_SpeechWeatherPatch=NULL;

const audCategory *audNorthAudioEngine::sm_ScoreProxyCat = NULL;
const audCategory *audNorthAudioEngine::sm_RadioProxyCat = NULL;
audCategoryController *audNorthAudioEngine::sm_ScoreVolCategoryController = NULL;
audCategoryController *audNorthAudioEngine::sm_OneShotVolCategoryController = NULL;
audCategoryController *audNorthAudioEngine::sm_MusicSliderController = NULL;
audCategoryController *audNorthAudioEngine::sm_SFXSliderController = NULL;
audCategoryController *audNorthAudioEngine::sm_NoFadeVolCategoryController = NULL;
audCategoryController *audNorthAudioEngine::sm_ScriptedOverrideFadeVolCategoryController = NULL;

#if RSG_PS3 || RSG_ORBIS
float g_PulseHPFCutoff = 50.f;
bool audNorthAudioEngine::sm_ShouldTriggerPulseHeadset = false;
audScene *audNorthAudioEngine::sm_PulseHeadsetScene = NULL;
bool audNorthAudioEngine::sm_IsWirelessHeadsetConnected = false;
s32 audNorthAudioEngine::sm_PreviousOutputPref = 0;
#endif // RSG_PS3 || RSG_ORBIS

audScene *audNorthAudioEngine::sm_StonedScene = NULL;

naMicrophones audNorthAudioEngine::sm_Microphones;

f32 audNorthAudioEngine::sm_SfxVolume = 1.f;
f32 audNorthAudioEngine::sm_MusicVolume = 1.f;

f32 audNorthAudioEngine::sm_ThirdPersonCameraBlendThreshold = 1.0f;
u32 audNorthAudioEngine::sm_LoadingScreenSFXSliderDelay = 0;

volatile bool audNorthAudioEngine::sm_IsShuttingDown = false;

bool audNorthAudioEngine::sm_HasAppliedARC1BankRemappings = false;
bool audNorthAudioEngine::sm_IsFadedToBlack = false;
bool audNorthAudioEngine::sm_ScreenFadedOutThisFrame = false;

audCurve audNorthAudioEngine::sm_BuiltUpToWeaponTails;

u32 audNorthAudioEngine::sm_PauseMenuSlowMoCount = 0;

extern CPlayerSwitchInterface g_PlayerSwitch;

#if GTA_REPLAY
bool g_UseSingleListener = false;
bool g_UseMonoReverbs = false;
bool g_EnableHPF = true;
bool g_EnableLPF = true;
f32 g_AudioFrameTime = 15.f;
extern bool g_VehicleEffectsBypass;
#endif

#if __BANK
u32 audNorthAudioEngine::sm_CurrentMeterIndex = 0;
audMeterList *audNorthAudioEngine::sm_MeterList[g_MaxBufferedMeters];

RegdEnt g_AudioDebugEntity(NULL);
audSound *audNorthAudioEngine::sm_DebugSound = NULL;
#endif

sysMemAllocator *g_AudioPhysicalAllocator;
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
void *g_AudioPhysicalMem;
#endif

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	#define MAX_WAVE_DATA (24 * 1024 * 1024 * 10)
#elif RSG_XENON
	#define MAX_WAVE_DATA (26 * 1024 * 1024)
#else
	#define MAX_WAVE_DATA (24 * 1024 * 1024)
#endif

const s32 g_AudioUpdateThreadCPU = 5;

// Audio dlc pack order - later packs override previous packs.
#define FIXED_DLC_PACK_ORDER_COUNT 20
const char * g_AudioDLCPackOrder[FIXED_DLC_PACK_ORDER_COUNT] = 
{ 
	"SinglePlayer",
	"GTAOnline",
	"dlcbeach",
	"dlcvalentines",
	"dlcbusiness",
	"dlcbusi2",
	"dlchipster",
	"dlcindependence",
	"dlcpilotschool",
	"dlcmpLTS",
	"dlcxmas2",
	"dlcmpheist",
	"dlcpd03",
	"dlcluxe",
	"dlcluxe2",
	"dlcsfx1",
	"dlclowrider",
	"dlchalloween",
	"dlcapartment",
	"dlcxmas3"
};

u32 GetSizeOfLargestVehicleClass()
{
	size_t maxSize = 0;

	maxSize = Max(sizeof(audCarAudioEntity), maxSize);
	maxSize = Max(sizeof(audHeliAudioEntity), maxSize);
	maxSize = Max(sizeof(audTrainAudioEntity), maxSize);
	maxSize = Max(sizeof(audBoatAudioEntity), maxSize);
	maxSize = Max(sizeof(audPlaneAudioEntity), maxSize);
	maxSize = Max(sizeof(audTrailerAudioEntity), maxSize); 
	maxSize = Max(sizeof(audBicycleAudioEntity), maxSize); 

	return (u32) maxSize;
}

static const int kSizeOfLargestVehicleClassAudio = GetSizeOfLargestVehicleClass();

INSTANTIATE_POOL_ALLOCATOR(audVehicleAudioEntity);

audCategoryController *g_BaseCategory;
audCategoryController *g_GameWorldCategory;

PF_PAGE(AudioEntitiesPage, "GTA Audio Entities");
PF_GROUP(AudioEntities);
PF_LINK(AudioEntitiesPage, AudioEntities);

PF_VALUE_FLOAT(UpdateLength, AudioEntities);
PF_VALUE_FLOAT(SlotUpdateLength, AudioEntities);
PF_VALUE_FLOAT(ControllerUpdateLength, AudioEntities);

PF_PAGE(NorthAudioPage, "North Audio Engine");
PF_GROUP(NorthAudio);
PF_LINK(NorthAudioPage, NorthAudio);
PF_VALUE_INT(MaxVoicesPerBucket, NorthAudio);
PF_VALUE_INT(MaxSoundsPerBucket, NorthAudio);
PF_VALUE_INT(MaxRequestedSettingsPerBucket, NorthAudio);

PF_PAGE(NorthAudioUpdatePage, "North Audio Update");
PF_GROUP(NorthAudioUpdate);
PF_LINK(NorthAudioUpdatePage, NorthAudioUpdate);
PF_TIMER(NorthAudioUpdateTimer, NorthAudioUpdate);
PF_TIMER(NorthAudioUpdateTimer_Update, NorthAudioUpdate);
PF_TIMER(NorthAudioUpdateTimer_CommandBuffer, NorthAudioUpdate);
PF_TIMER(NorthAudioUpdateTimer_FinishUpdate, NorthAudioUpdate);

PF_PAGE(TriggerSoundsPage, "TriggeredSounds");
PF_GROUP(TriggerSoundsGroup);
PF_LINK(TriggerSoundsPage, TriggerSoundsGroup);

PARAM(mute, "[RAGE Audio] Mutes audio.");
PARAM(nogameaudio, "[Audio] Disable game audio engine init for offline tools (may not work in-game)");

XPARAM(audiodesigner);
XPARAM(audiotag);
PARAM(radiohudwidgets, "Activates radio hud widgets");

NOSTRIP_PC_PARAM(minspecaudio, "Force minspec audio processing");
NOSTRIP_PC_PARAM(nominspecaudio, "Force normal audio processing regardless of CPU");

namespace rage
{
	NOSTRIP_XPARAM(audiofolder);
}

#if !__FINAL
XPARAM(allcutsceneaudio);
#endif

#if __BANK
// offline compute audio world sectors
PARAM(calculateAudioWorldSectors, "[Audio] Enabled audio world sectors offline computation.");
PARAM(inGameAudioWorldSectors, "[Audio] Enabled audio world sectors online info.");
namespace rage
{
XPARAM(rave);
XPARAM(raveskipchunks);
PARAM(raveSkipReleasedDlcChunks, "[RAGE Audio] Automatically skips a hard coded list of metadata chunks");
XPARAM(fullsoundhierarchy);
XPARAM(noaudio);
XPARAM(audiowidgets);
XPARAM(printlargesounds);
XPARAM(loadtestsounds);
extern bool g_WarnOnMissingSounds;
extern bool g_WarnOnMissingCurves;
extern bool g_FullCurveTweakAbility;
}
XPARAM(Audio_tty);
XPARAM(usefatcuts);

extern bool g_UseEditedCutscenes;
extern bool g_UseMasterCutscenes;
#endif

#if __DEV
namespace rage
{
extern u32 g_MaxRequestedSettingsSlotsUsed;
extern u32 g_MaxSoundSlotsUsed;
extern u32 g_MaxVoicesUsed;
extern u32 g_MaxSoundsAllocated;
extern u32 g_MaxActiveEnvironmentGroups;
extern u32 g_MaxActiveAudioEntities;
extern u32 g_MaxActiveVirtualVoices;
extern u32 g_MaxActiveVoiceGroups;
extern bool g_AssertOnSoundPoolFull;
extern bool g_PrintSoundPoolWhenFull;
}
#endif

#if !__FINAL
PARAM(audiomem, "[Audio] write audio memory report");

class audMemoryReporter
{
public:
	audMemoryReporter(const char *destFileName)
	{
		m_Stream = ASSET.Create(destFileName, "");
		m_Total = 0;
	}

	bool HasStream() const
	{
		return m_Stream != NULL;
	}

	void AddLine(const char *str)
	{
		char lineBuf[128];
		formatf(lineBuf, "%s\r\n", str);
		m_Stream->Write(lineBuf, istrlen(lineBuf));
	}

	void AddLine(const char *entry, const u32 sizeBytes)
	{
		char lineBuf[128];
		u32 sizeKb = sizeBytes >> 10;
		formatf(lineBuf, "%s,%uKB\r\n", entry, sizeKb);
		m_Stream->Write(lineBuf, istrlen(lineBuf));
		m_Values.PushAndGrow(sizeBytes);
		m_Total += sizeBytes;
	}


	void AddMetadata(const char *entry, const audMetadataManager &metadataMgr)
	{
		char lineBuf[128];
		u32 size = metadataMgr.GetLoadedDataSize();
		m_Total += size;
		m_Values.PushAndGrow(size);
		formatf(lineBuf, "%s_Data: %uKB (Source CL %u)\r\n", entry, size >> 10, metadataMgr.GetAssetChangelist());
		m_Stream->Write(lineBuf, istrlen(lineBuf));
		size = metadataMgr.GetLoadedNameTableSize();
		m_Total += size;
		m_Values.PushAndGrow(size);
		formatf(lineBuf, "%s_Names: %uKB\r\n", entry, size >> 10);
		m_Stream->Write(lineBuf, istrlen(lineBuf));
	}

	void Close()
	{
		PrintTotal();
		PrintCSV();
		m_Stream->Close();
	}

private:

	void PrintCSV()
	{
		char lineBuf[64];
		bool needComma = false;
		for(s32 i = 0 ; i < m_Values.GetCount(); i++)
		{
			formatf(lineBuf, "%s%u", needComma ? "," : "", m_Values[i]);
			m_Stream->Write(lineBuf, istrlen(lineBuf));
			needComma = true;
		}

		formatf(lineBuf, ",%u\r\n", m_Total);
		m_Stream->Write(lineBuf, istrlen(lineBuf));
	}

	void PrintTotal()
	{
		char lineBuf[128];
		formatf(lineBuf, "\r\nTotal: %.2fMB\r\n", m_Total / (1024.f * 1024.f));
		m_Stream->Write(lineBuf, istrlen(lineBuf));
	}

	atArray<u32> m_Values;
	u32 m_Total;
	fiStream *m_Stream;
};

void audNorthAudioEngine::GenerateMemoryReport(const char *destFileName)
{
	char lineBuf[128];

	audMemoryReporter report(destFileName);
	if(report.HasStream())
	{
		const char *platform = "None";
#if RSG_XENON
		platform = "Xbox360";
#elif RSG_PS3
		platform = "PS3";
#elif RSG_ORBIS
		platform = "Orbis";
#elif RSG_DURANGO
		platform = "Durango";
#elif RSG_PC
		platform = "PC";
#else 
	#error "Not Implemented"
#endif
		
		report.AddLine(formatf(lineBuf, "%s Audio memory usage report\r\n", platform));

#if __BANK
		if(PARAM_rave.Get())
		{
			report.AddLine(formatf(lineBuf, "Warning: running with RAVE - memory usage will be inflated\r\n"));
		}
#endif // __BANK
		report.AddLine("WaveSlots_Main", audWaveSlot::GetTotalSlotSizeMain());
#if __PS3
		report.AddLine("WaveSlots_VRAM", audWaveSlot::GetTotalSlotSizeVRAM());
#endif

		report.AddMetadata("Category", g_AudioEngine.GetCategoryManager().GetMetadataManager());
		report.AddMetadata("Curve", g_AudioEngine.GetCurveRepository().GetMetadataManager());
		report.AddMetadata("DynamicMixer", g_AudioEngine.GetDynamicMixManager().GetMetadataManager());
		report.AddMetadata("GameObject", audNorthAudioEngine::GetMetadataManager());
		report.AddMetadata("Sound", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager());
		report.AddMetadata("ModularSynth", synthSynthesizer::GetMetadataManager());
		report.AddMetadata("Speech", audSpeechSound::GetMetadataManager());

		report.Close();
	}

}

#endif

RAGE_DEFINE_SUBCHANNEL(Audio,NorthAudio)

#if __ASSERT
class audNorthAudioEngineThreadVerifier : public audControllerThreadVerifier
{
public:
	void OnCreateSound(const audMetadataRef &soundRef) const
	{
		if(sysThreadType::IsUpdateThread() && audNorthAudioEngine::IsAudioUpdateCurrentlyRunning())
		{
			const char *soundName = "invalid";
			if(soundRef.IsValid())
			{
				const Sound *s = reinterpret_cast<const Sound*>(g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataPtr(soundRef));
				if(s)
				{
					soundName = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(s->NameTableOffset);
				}
			}
			naAssertf(false, "Creating a sound (%s) from the main thread while audNorthAudioEngine Update is running.", soundName);
		}
	}
};
audNorthAudioEngineThreadVerifier g_NorthAudioThreadVerifier;
#endif

void InitialiseAudioForInstall()
{
	audNorthAudioEngine::InitClass();
}

bool audNorthAudioEngine::InitClass()
{
	if(g_AudioInitialised == true)
		return true;

#if !__FINAL
	if(PARAM_nogameaudio.Get())
		return true;
#endif

#if !RSG_FINAL
	PARAM_allcutsceneaudio.Set("yes");	
#endif 

#if __BANK
	if(PARAM_allcutsceneaudio.Get())
	{
		PARAM_loadtestsounds.Set("yes"); 
	}
	if(PARAM_audiodesigner.Get())
	{
		audEnvironmentSound::SetRecordInvalidCategorySounds(true);

		naDisplayf("Enabling audio designer support");
		if(!PARAM_rave.Get())
		{
			PARAM_rave.Set("Sounds,GameObjects,Curves,Categories,ModularSynth,DynamicMixer,AudioConfig"); 
		}
#if RSG_PS3 || RSG_XENON
		if(!PARAM_raveskipchunks.Get())
		{
			PARAM_raveskipchunks.Set("base,singleplayer,optimised");
		}
#endif

		if (PARAM_raveSkipReleasedDlcChunks.Get() && !PARAM_raveskipchunks.Get())
		{
			PARAM_raveskipchunks.Set("dlcbeach,dlcbusiness,dlcbusi2,dlclowrider,dlclow2,dlcImportExport,dlcSpecialRaces,dlcGunrunning,dlcAirraces,dlcSmuggler,dlcChristmas2017,dlcAssault,dlcBattle,dlcAWXM2018,dlchalloween,dlcvalentines,dlchipster,dlcApartment,dlcJanuary2016,mpValentines2,dlcExec1,dlcStunt,dlcBiker,dlcmpheist,dlcpd03,dlcpd02,dlcpilotschool,dlcindependence,dlcmplts,dlcthelab,dlcxmas2,DLC_RADIOSURF,dlcluxe,dlcXmas3,dlcluxe2,dlcsfx1,dlcVinewood,dlcHeist3");
		}

		PARAM_fullsoundhierarchy.Set("yes");
		PARAM_printlargesounds.Set("yes");

		Channel_Audio_NorthAudio.TtyLevel = DIAG_SEVERITY_DEBUG3; // PARAM_Audio_NorthAudio_tty.Set("debug3");
		Channel_Audio.TtyLevel = DIAG_SEVERITY_DEBUG3; // PARAM_Audio_tty.Set("debug3");
		PARAM_audiowidgets.Set("yes");
		PARAM_radiohudwidgets.Set("yes");
	

		sm_AuditionSoundsOnPPU = true;
		
		g_WarnOnMissingCurves = true;
		// No point warning on missing sounds if -noaudio is specified.
		g_WarnOnMissingSounds = !PARAM_noaudio.Get();

		g_FullCurveTweakAbility = true;
		
		// default to ignoring frontend sliders
		g_IgnoreFrontendVolumeSliders = true;
	}
#endif

	USE_MEMBUCKET(MEMBUCKET_AUDIO);
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	u32 memSize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("AudioHeap", 0x5293FA5), CONFIGURED_FROM_FILE) * 1024 * 1024;
#if RSG_DURANGO || RSG_ORBIS	
	g_AudioPhysicalMem = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->Allocate(memSize, 16);	
#else
	const u32 memForUserMusic = 13 * 1024 * 1024;
	memSize += memForUserMusic;

	// url:bugstar:6554866 - Heist 4 : Please increase audio heap for PC dev_ng so that we can have larger Streamed Ambience waveslots
	const u32 ambientBankCompensation = 4 * 1024 * 1024;
	memSize += ambientBankCompensation;

	g_AudioPhysicalMem = sysMemPhysicalAllocate(memSize);
#endif
	Assert(g_AudioPhysicalMem);

	g_AudioPhysicalAllocator = rage_new sysMemSimpleAllocator(g_AudioPhysicalMem, memSize, MEMTYPE_COUNT+1);

#if RAGE_TRACKING
	diagTracker* t = diagTracker::GetCurrent();
	if (t && sysMemVisualize::GetInstance().HasAudio())
	{
		t->InitHeap("Audio Physical", g_AudioPhysicalMem, memSize);
	}
#endif // RAGE_TRACKING

#else // RSG_PC || RSG_DURANGO || RSG_ORBIS

	g_AudioPhysicalAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

#endif //RSG_PC || RSG_DURANGO || RSG_ORBIS

	if(!InitializeAudio(sm_AudioController, g_AudioPhysicalAllocator, sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL),
		g_AudioConfigPath))
	{
		return false;
	}

	LoadSPData();
	// Now keep MP speech data loaded in SP for reindeer assets
	audSpeechSound::LoadSpeechMetadata("GTAOnline", "audio:/config/dlc_gtao_speech.dat");

#if !__FINAL
	static naStreamingDebugInterface s_DebugStreamingInterface;
	audStreamingWaveSlot::SetDebugInterface(&s_DebugStreamingInterface);
#endif

	PS3_ONLY(CheckForWirelessHeadset(true));

#if RSG_BANK && 0
	if(!CFileMgr::ShouldForceReleaseAudioPacklist())
	{
		audDisplayf("Loading placeholder speech lookup");
		audSpeechSound::LoadSpeechMetadata("PlaceholderSpeech", "audio:/config/p_speech.dat");
	}
#endif

	ASSERT_ONLY(sm_AudioController.SetThreadVerifier(&g_NorthAudioThreadVerifier));

#if __DEV
	g_AssertOnSoundPoolFull = false;
#endif

	// Assert if we've allocated too much memory for wave banks
	if(audWaveSlot::GetTotalSlotSize() > MAX_WAVE_DATA)
	{
		naWarningf("Total wave data size: %d", audWaveSlot::GetTotalSlotSize());
#if !__PPU
		// This error triggers every single time on PS3, so nerf it for now.
		naErrorf("Audio: Exceeded memory budget - please inform audio team.");
#endif
	}

	sm_MetadataMgr.Init("GameObjects", "audio:/config/game.dat", audMetadataManager::External_NameTable_BankOnly, GAMEOBJECTS_SCHEMA_VERSION, true);

	// Reserve the chunk order for DLC packs. Audio data for some packs will get unmounted on session shutdown then remounted for the next session, whilst 
	// for others it will be persistent, meaning that the order of loaded packs can change. However, we need the audio data order to be consistent so that 
	// we can guarantee that certain packs will override others, regardless of when they were loaded.
	for(u32 dlcPackIndex = 0; dlcPackIndex < FIXED_DLC_PACK_ORDER_COUNT; dlcPackIndex++)
	{		
		audDisplayf("Reserving metadata chunks for pack %s at index %d", g_AudioDLCPackOrder[dlcPackIndex], dlcPackIndex);
		sm_MetadataMgr.ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
		g_AudioEngine.GetDynamicMixManager().GetMetadataManager().ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
		g_AudioEngine.GetCurveRepository().GetMetadataManager().ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
		g_AudioEngine.GetCategoryManager().GetMetadataManager().ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
		synthSynthesizer::GetMetadataManager().ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
		audSpeechSound::GetMetadataManager().ReserveChunk(g_AudioDLCPackOrder[dlcPackIndex]);
	}

	// assert if we've not loaded valid game.dat
	naAssertf(sm_MetadataMgr.ComputeNumberOfObjects()>0, "Failed to load a valid game.dat in InitClass"); 

	sm_MetadataMgr.SetObjectModifiedCallback(&sm_GameObjectMgr);

	if(PARAM_mute.Get())
	{
		sm_ShouldMuteAudio = true;
	}

	g_AudioEngine.EnableAudio(true);

	sm_StreamingSoundBucketId = audSound::ReserveBucket();
	naDisplayf("Bucket reserved for streaming: %u", sm_StreamingSoundBucketId);
	VIDEO_PLAYBACK_ONLY( VideoPlayback::SetAudioBucketId( sm_StreamingSoundBucketId ) );

	sm_TimeWarpSmoother.Init(5.f/1000.f, 1.f/1000.f, 0.f, 1.f);

	// need to init emitter audio entity early to catch initial object creation
	g_EmitterAudioEntity.Init();

	sm_AudioController.SetEnvironmentGroupManager(&sm_EnvironmentGroupManager);

	sm_RadioDuckingSmoother.Init(g_RadioDuckingSmoothRateUp, g_RadioDuckingSmoothRate, 0.0f, 1.5f);
	sm_GPSDuckingSmoother.Init(g_RadioDuckingSmoothRate, g_RadioDuckingSmoothRate, 0.0f, 1.0f);
	sm_TrainRolloffInSubwaySmoother.Init(0.0003f, 0.0003f, 0.0f, 10.0f);
	
	audCategory *firstSpeechCat;
	firstSpeechCat = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("GLOBAL_SPEECH", 0xAFDC8D3));
	
	const u32 firstSpeechCatIndex = (u32)g_AudioEngine.GetCategoryManager().GetCategoryIndex(firstSpeechCat);
	g_AudioEngine.GetEnvironment().SetSpeechCategoryRange(firstSpeechCatIndex, firstSpeechCatIndex + 12);

	// need a listener
	g_AudioEngine.GetEnvironment().SetListenerContribution(1.0f,0);
	g_AudioEngine.GetEnvironment().SetRollOff(1.0f,0);
	g_AudioEngine.CommitGameSettings(0);
	// need to force an update so that the controller activates
	sm_AudioController.PreUpdate(0);
	sm_AudioController.Update(0);

	audCategoryControllerManager::Instantiate();
	audInteractiveMusicManager::Instantiate();

	// need a listener
	g_AudioEngine.GetEnvironment().SetListenerContribution(1.0f,0);
	g_AudioEngine.CommitGameSettings(0);
	// need to force an update so that the controller activates
	sm_AudioController.PreUpdate(0);
	sm_AudioController.Update(0);
	
	StartUserUpdateFunction();

	g_AudioInitialised = true;
	g_HasInitialisedStaticClasses = false;

	sm_IsShuttingDown = false;
	sm_RunUpdateSema = sysIpcCreateSema(0);
	sm_UpdateFinishedSema = sysIpcCreateSema(0);

	sm_UpdateThread = sysIpcCreateThread(AudioUpdateThread, NULL, 256*1024, PRIO_AUDIO_THREAD, "North Audio Update", g_AudioUpdateThreadCPU, "NorthAudioUpdate");

	audAmbisonics::SetDrawFunction(AmbisonicDraw);

	sm_Microphones.Init();

#if USE_GUN_TAILS
	sm_GunFireAudioEntity.Init(); 
#endif
#if USE_CONDUCTORS
	sm_SuperConductor.Init(); 
#endif
	audGlassAudioEntity::InitClass();
	audSmashableGlassEvent::InitClass();
	audCollisionAudioEntity::InitClass();

	sm_DynamicMixer.Init();

	g_FrontendAudioEntity.Init();

#if GTA_REPLAY
	g_ReplayAudioEntity.Init();
#endif

#if __BANK 
	if(PARAM_calculateAudioWorldSectors.Get())
	{
		sm_Environment.DrawAudioWorldSectors(true); 
		sm_Environment.ProcessSectors(true); 
	}
	else if (PARAM_inGameAudioWorldSectors.Get())
	{
		sm_Environment.ProcessSectors(true); 
	}
#endif
#if !__FINAL
	const char *reportFileName = "audiomem.txt";
	if(PARAM_audiomem.Get(reportFileName))
	{
		GenerateMemoryReport(reportFileName);
	}

#if !__FINAL_LOGGING
	// Display source asset changelists for all metadata types
	audDisplayf("Source asset CLs - Sounds: %u, GameObjects: %u, Categories: %u, DynamicMixer: %u, AudioConfig: %u, Curves: %u, Synth: %u Speech %u",
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetAssetChangelist(),
		sm_MetadataMgr.GetAssetChangelist(),
		g_AudioEngine.GetCategoryManager().GetMetadataManager().GetAssetChangelist(),
		g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetAssetChangelist(),
		audConfig::GetMetadataManager().GetAssetChangelist(),
		g_AudioEngine.GetCurveRepository().GetMetadataManager().GetAssetChangelist(),
		synthSynthesizer::GetMetadataManager().GetAssetChangelist(),
		audSpeechSound::GetMetadataManager().GetAssetChangelist());
#endif
#endif

	sm_ActiveSlowMoModes[AUD_SLOWMO_GENERAL] = ATSTRINGHASH("SLOWMO_GENERAL", 0x995E46B3);

	naAudioEntity::SetDefaultAnimHandler((fwAudioAnimHandlerInterface *)&sm_AudAnimHandler);

	BANK_ONLY(g_AudioEngine.GetCategoryManager().SetTelemetryEnabled(true));

	// Hook into the extra content mounting system
	RegisterDataFileMounters();

	/*
	g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().LoadMetadataChunk("DLC_RADIOSURF", "audio:/config/dlc_radiosurf_sounds.dat");
	GetMetadataManager().LoadMetadataChunk("DLC_RADIOSURF", "audio:/config/dlc_radiosurf_game.dat");
	audSpeechSound::GetMetadataManager().LoadMetadataChunk("DLC_RADIOSURF", "audio:/config/dlc_radiosurf_speech.dat");
	*/

	// This will be loaded as part of a propper dlc, un comment to force load temporarily.
	//g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().LoadMetadataChunk("ReplayED", 
	//	"audio:/config/dlcsfx1_sounds.dat",
	//	sm_ReplayedSoundDataPtr,
	//	sm_ReplayedSoundDataSize);
	//GetMetadataManager().LoadMetadataChunk("ReplayEdSFX", "audio:/config/dlcsfx1_game.dat");
	//if(g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().FindChunkId("ReplayEdSFX") == -1)
	//{
	//	Displayf("Failed to load ReplayED audio data");
	//}

	sm_RadioProxyCat = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("RADIO_VOL_PROXY", 0xA6A84701));
	sm_ScoreProxyCat = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("SCORE_VOL_PROXY", 0x3C496EED));

	sm_ScoreVolCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("INTERACTIVE_MUSIC", 0x05db47ec6));
	sm_OneShotVolCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("MUSIC_ONESHOT", 0x020230050));
	sm_SFXSliderController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("SFX_SLIDER", 0xBAD598C7));
	sm_MusicSliderController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("MUSIC_SLIDER", 0xA4D158B0));
	sm_NoFadeVolCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("FRONTEND_GAME_NOFADE", 0x2C7B342));
	sm_ScriptedOverrideFadeVolCategoryController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("SCRIPTED_OVERRIDE_FADE", 0xEB0390D7));

	sm_CutsceneLeakageSmoother.Init(g_CutsceneLeakageSmootherRate * 0.001f, true);

	sm_DataSetState = Loaded_SP;
	sm_IsMPDataRequested = false;

	const audMetadataManager &soundMetadataManager = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager();
	const audMetadataChunk &spChunk = soundMetadataManager.GetChunk(soundMetadataManager.FindChunkId("SinglePlayer"));
	sm_SPSoundDataPtr = spChunk.data;
	sm_SPSoundDataSize = spChunk.dataSize;

#if RSG_BANK
	if(g_AudioEngine.GetRemoteControl().IsConnected())
	{
		audRemoteControlSerializer serializer(g_AudioEngine.GetRemoteControl().GetPayloadBuffer(), audRemoteControl::kRemoteControlPayloadSizeBytes);
		g_AudioEngine.GetRemoteControl().SendCommand(serializer, AUDIO_ENGINE_LOADED);
	}
#endif
	return true;
}

extern CutsceneSettings *g_DefaultCutsceneSettings;
void audNorthAudioEngine::Init(u32 initMode)
{
    if(initMode == INIT_CORE)
    {
        if(!InitClass())
        {
            Assertf(FALSE, "Failed to initialise Audio Engine!");
        }
		audController::UpdateEnvironmentGroups(true);
    }
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		// Load additional map specific data for Liberty / Prologue
		// Alternative maps are DLC packs, so will load/unload any extra metadata via that mechanism
		//LoadMapData();
	}
    else if(/*initMode == INIT_AFTER_MAP_LOADED || */initMode == INIT_SESSION)
    {
	    g_AudioEngine.GetEnvironment().InitCurves();


		g_BaseCategory = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("BASE", 0x044e21c90));
	    g_GameWorldCategory = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("GAME_WORLD", 0x02729f75d));

		DYNAMICMIXER.StartScene(ATSTRINGHASH("NORTHAUDIO_ENGINE_MIXING", 0xa3657c2c), &sm_NorthAudioMixScene);
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("radioVol", 0xe7e3432c), 0.f);
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("radioFrontendVol", 0xc00f6acd), 0.f);
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("djFrontendVol", 0xc06429e6), 0.f);
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("posRadioVol", 0x4be8fa9a), 0.f);
		//sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("heliVol", 0x5acd3f4a), 0.f);
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("interiorRatio", 0x13fc3a70), 0.f);
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("scriptedSpeechVol", 0x5b3a463), 0.f);

		if(!sm_SpeechScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("SPEECH_SCENE", 0x576520E1),&sm_SpeechScene);
			if(sm_SpeechScene)
			{
				sm_SpeechScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), 1.f);
				sm_SpeechGunfirePatch = sm_SpeechScene->GetPatch(ATSTRINGHASH("SPEECH_GUNFIRE_PATCH", 0x82B39AD3));
				sm_SpeechScorePatch = sm_SpeechScene->GetPatch(ATSTRINGHASH("SPEECH_SCORE", 0x216E4ABF));
				sm_SpeechVehiclesPatch = sm_SpeechScene->GetPatch(ATSTRINGHASH("SPEECH_VEHICLES_PATCH", 0x283253));
				sm_SpeechVehiclesFirstPersonPatch = sm_SpeechScene->GetPatch(ATSTRINGHASH("SPEECH_VEHICLES_FIRST_PERSON_PATCH", 0xA2DA442));
				sm_SpeechWeatherPatch = sm_SpeechScene->GetPatch(ATSTRINGHASH("SPEECH_WEATHER_PATCH", 0x33B1F2AC));
			}
		}

	    // init the static emitters - this needs to be done after all dlc has been loaded.
	    g_EmitterAudioEntity.InitStaticEmitters();
	    // tell the emitter audio entity its safe to start triggering audio
	    g_EmitterAudioEntity.SetActive(true);
	    g_EmitterAudioEntity.ResetTimers();
	    g_WeatherAudioEntity.Init();


	    // this needs to be after we've loaded all episodic content
	    if(!g_HasInitialisedStaticClasses)
	    {
			CPacketSoundCreatePos::CalculateValidMetadataOffsets();

		    naEnvironmentGroup::InitClass();
			sm_EnvironmentGroupManager.Init();
		    sm_AudioController.SetEnvironmentGroupManager(&sm_EnvironmentGroupManager);

			naAudioEntity::InitClass();

			audStreamSlot::InitClass();
		    NA_RADIO_ENABLED_ONLY(audRadioAudioEntity::InitClass());

			audDoorAudioEntity::InitClass();
		    audScriptAudioEntity::InitClass();
		    audPedAudioEntity::InitClass();
		    audSpeechAudioEntity::InitClass();
			audVehicleReflectionsEntity::InitClass();
		    audVehicleAudioEntity::InitClass();
			audVehicleEngine::InitClass();
			audVehicleElectricEngine::InitClass();
			audVehicleTurbo::InitClass();
			audVehicleTransmission::InitClass();
			audVehicleCollisionAudio::InitClass();
			audHeliAudioEntity::InitClass();
			audPlaneAudioEntity::InitClass();
			audCarAudioEntity::InitClass();
			audBicycleAudioEntity::InitClass();
			audTrailerAudioEntity::InitClass();
			audBoatAudioEntity::InitClass();
			audTrainAudioEntity::InitClass();
			audAmbientZone::InitClass();
		    audWeatherAudioEntity::InitClass();
		    audExplosionAudioEntity::InitClass();
			audShoreLine::InitClass();
			audShoreLinePool::InitClass();
			audShoreLineRiver::InitClass();
			audShoreLineOcean::InitClass();
			audShoreLineLake::InitClass();
			audVehicleFireAudio::InitClass();
			audFireAudioEntity::InitClass();
			g_FireSoundManager.InitClass();
		    g_FrontendAudioEntity.InitCurves();
			g_HasInitialisedStaticClasses = true;
	    }

		sm_OcclusionManager.Init();
	    sm_Environment.Init();

	    sm_TimeWarpToPitch.Init(ATSTRINGHASH("TIME_WARP_TO_PITCH", 0x61E2DEFA));
	    sm_TimeWarpToTimeScale.Init(ATSTRINGHASH("TIME_WARP_TO_TIME_SCALE", 0xDDE168FB));
	    g_TimeWarpToDelayFeedback.Init(ATSTRINGHASH("TIME_WARP_TO_DELAY_FEEDBACK", 0x6E4FCCB0));
#if GTA_REPLAY
		sm_ReplayTimeWarpToTimeScale.Init(ATSTRINGHASH("REPLAY_TIME_WARP_TO_TIME_SCALE", 0xA2363059));
#endif

	    sm_BuiltUpToWeaponTails.Init(ATSTRINGHASH("BUILT_UP_TO_TAIL_VOLUME", 0xC94C4CC4));

	    NA_RADIO_ENABLED_ONLY(g_RadioAudioEntity.Init());
		NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.Init());

	    g_CollisionAudioEntity.Init();
	    g_CutsceneAudioEntity.Init();
	    g_SpeechManager.Init();
	    g_ScriptAudioEntity.Init();
	    g_WeaponAudioEntity.Init();
	    g_GlassAudioEntity.Init();
	    g_AmbientAudioEntity.Init();
		g_ReflectionsAudioEntity.Init();
	    g_InteractiveMusicManager.Init();

	    g_MuteGameWorldAudio = false;
	    g_MuteGameWorldAndPositionedRadioForTV = false;

#if __BANK
	    // cutscene mix editor for Nick
	    bkBank *audioBank = BANKMGR.FindBank("CutsceneAudio");
	    if(audioBank)
	    {
		    BANKMGR.DestroyBank(*audioBank);
	    }
#endif

		sm_StartingNewGame = false;
    }

	sm_PIVMusicVolumeSmoother.Init(0.001f / g_PIVMusicVolumeFadeInS, 0.001f / g_PIVMusicVolumeFadeOutS, 0.f, 1.f);
	sm_PIVRadioVolumeSmoother.Init(0.001f / g_PIVMusicVolumeFadeInS, 0.001f / g_PIVMusicVolumeFadeOutS, 0.f, 1.f);
	sm_PIVOpennessSmoother.Init(0.001f / g_PIVOpennessFadeInS, 0.001f / g_PIVOpennessFadeOutS, 0.f, 1.f);
}

void audNorthAudioEngine::DLCInit(u32 initMode)
{
	if(initMode == INIT_SESSION)
	{
		g_EmitterAudioEntity.ReInitStaticEmitters();
		g_CollisionAudioEntity.ProcessMacsModelOverrides();
		g_AmbientAudioEntity.CheckAndAddZones();
		audVehicleConductor::InitForDLC();
		NA_RADIO_ENABLED_ONLY(audRadioAudioEntity::DLCInitClass();)
	}
}

void audNorthAudioEngine::Shutdown(u32 shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        ShutdownClass();
    }
	else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		// Alternative maps are DLC packs, so will load/unload any extra metadata via that mechanism
		//UnloadMapData();
	}
    else if(shutdownMode ==  SHUTDOWN_SESSION)
    {
		sm_Environment.PurgeInteriors();

	    g_EmitterAudioEntity.SetActive(false);
        g_FrontendAudioEntity.ShutdownLevel();
        g_WeatherAudioEntity.Shutdown();
		g_GlassAudioEntity.Shutdown();
		g_InteractiveMusicManager.Shutdown();

        // this needs to be after we've loaded all episodic content
        if (false && g_HasInitialisedStaticClasses)
        {
			sm_EnvironmentGroupManager.Shutdown();
            naEnvironmentGroup::ShutdownClass();
            sm_AudioController.SetEnvironmentGroupManager(NULL);

            audStreamSlot::ShutdownClass();
            NA_RADIO_ENABLED_ONLY(audRadioAudioEntity::ShutdownClass());

            audScriptAudioEntity::ShutdownClass();
            audPedAudioEntity::ShutdownClass();
            audVehicleAudioEntity::ShutdownClass();
            audWeatherAudioEntity::ShutdownClass();
			audShoreLineOcean::ShutDownClass();
			audShoreLineRiver::ShutDownClass();
			audShoreLineLake::ShutDownClass();
			g_FireSoundManager.ShutdownClass();
            audInteractiveMusicManager::Destroy();
			audInteractiveMusicManager::Destroy();
			g_HasInitialisedStaticClasses = false;
        }

		sm_OcclusionManager.Shutdown();
        sm_Environment.Shutdown();

        NA_RADIO_ENABLED_ONLY(g_RadioAudioEntity.Shutdown());
        NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.Shutdown());

        g_CollisionAudioEntity.Shutdown();
        g_CutsceneAudioEntity.Shutdown();
        g_SpeechManager.Shutdown();
        g_ScriptAudioEntity.Shutdown();
        g_WeaponAudioEntity.Shutdown();

        g_AmbientAudioEntity.Shutdown();
		audShoreLineOcean::ShutDownClass();
		audShoreLineRiver::ShutDownClass();
		audShoreLineLake::ShutDownClass();

		g_ReflectionsAudioEntity.Shutdown();

#if USE_GUN_TAILS
		sm_GunFireAudioEntity.Shutdown();
#endif

		CONDUCTORS_ONLY(SUPERCONDUCTOR.ShutDown());
		//This last just in case other things use categories ... 
        AUDCATEGORYCONTROLLERMANAGER.DestroyController(g_BaseCategory); g_BaseCategory = NULL;
        AUDCATEGORYCONTROLLERMANAGER.DestroyController(g_GameWorldCategory); g_GameWorldCategory = NULL;

	    // don't do this if the game is exiting
	    if(!CSystem::WantToExit())
	    {
		    // ditch any loads in progress to ensure we don't deadlock the streamer
		    audWaveSlot::Drain();
	    }
		// Force a synchronous update to ensure the Stop... calls are actioned immediately (it will be along time until the next update)
		audNorthAudioEngine::MinimalUpdate();

		if(sm_NorthAudioMixScene)
		{
			sm_NorthAudioMixScene->Stop();
		}
		if(sm_PlayerInVehScene)
		{
			sm_PlayerInVehScene->Stop();
		}
		if(sm_SpeechScene)
		{
			sm_SpeechScene->Stop();
		}
		if(sm_MuteAllScene)
		{
			sm_MuteAllScene->Stop();
		}
		if(sm_DeathScene)
		{
			sm_DeathScene->Stop();
		}
		if(sm_MpLobbyScene)
		{
			sm_MpLobbyScene->Stop();
		}
		if(sm_ReplayEditorScene)
		{
			sm_ReplayEditorScene->Stop();
		}
		if(sm_TvScene)
		{
			sm_TvScene->Stop();
		}
		if(sm_FrontendScene)
		{
			sm_FrontendScene->Stop();
		}
		if(sm_GameWorldScene)
		{
			sm_GameWorldScene->Stop();
		}
		if(sm_PosRadioMuteScene)
		{
			sm_PosRadioMuteScene->Stop();
		}
		if(sm_SlowMoScene)
		{
			sm_SlowMoScene->Stop();
		}
		if(sm_SlowMoVideoEditorScene)
		{
			sm_SlowMoVideoEditorScene->Stop();
		}
		if(sm_SuperSlowMoVideoEditorScene)
		{
			sm_SuperSlowMoVideoEditorScene->Stop();
		}
		if(sm_SlowMoSound)
		{
			sm_SlowMoSound->StopAndForget();
		}
	}
}

void audNorthAudioEngine::ShutdownClass()
{
	// if shutdown already then return
	if(g_AudioInitialised == false)
		return;

	if(sm_ExtraContentPath)
	{
		delete[] sm_ExtraContentPath;
		sm_ExtraContentPath = NULL;
	}
#if GTA_REPLAY
	g_ReplayAudioEntity.Shutdown();
#endif
	g_FrontendAudioEntity.Shutdown();
	g_EmitterAudioEntity.Shutdown();
	audScriptAudioEntity::ShutdownClass();
	audPedAudioEntity::ShutdownClass();

	audWeatherAudioEntity::ShutdownClass();
	audVehicleAudioEntity::ShutdownClass();

	sm_DynamicMixer.Shutdown();

	sm_RadioProxyCat = NULL;
	sm_ScoreProxyCat = NULL;

	AUDCATEGORYCONTROLLERMANAGER.DestroyController(sm_ScoreVolCategoryController);
	sm_ScoreVolCategoryController = NULL;
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(sm_OneShotVolCategoryController);
	sm_OneShotVolCategoryController = NULL;
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(sm_SFXSliderController);
	sm_SFXSliderController = NULL;
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(sm_MusicSliderController);
	sm_MusicSliderController = NULL;
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(sm_NoFadeVolCategoryController);
	sm_NoFadeVolCategoryController = NULL;
	AUDCATEGORYCONTROLLERMANAGER.DestroyController(sm_ScriptedOverrideFadeVolCategoryController);
	sm_ScriptedOverrideFadeVolCategoryController = NULL;

	NA_RADIO_ENABLED_ONLY(audRadioAudioEntity::ShutdownClass());

	audStreamSlot::ShutdownClass();

	sm_AudioController.Shutdown();
	g_AudioEngine.Shutdown();

	sm_MetadataMgr.Shutdown();

	StopUserUpdateFunction();

	//Destroy the category manager
	audCategoryControllerManager::Destroy();

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	delete g_AudioPhysicalAllocator;

#if RSG_DURANGO || RSG_ORBIS
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->Free(g_AudioPhysicalMem);
#else
	sysMemPhysicalFree(g_AudioPhysicalMem);
#endif

#endif

	sm_IsShuttingDown = true;
	sysIpcSignalSema(sm_RunUpdateSema);
	sysIpcWaitThreadExit(sm_UpdateThread);
	sysIpcDeleteSema(sm_RunUpdateSema);
	sysIpcDeleteSema(sm_UpdateFinishedSema);

	sm_RunUpdateSema = 0;
	sm_UpdateFinishedSema = 0;
	sm_UpdateThread = sysIpcThreadIdInvalid;

	g_AudioInitialised = false;
}


audMetadataDataFileMounter::audMetadataDataFileMounter(audMetadataManager &metadataManager) : m_MetadataManager(metadataManager)
{
	m_UnloadingChunksCount = 0;
	for(int i=0; i < k_MaxDlcPacks; i++)
	{
		m_FramesToUnload[i] = 0;
		m_UnloadingChunkNames[i][0] = '\0';
	}
}

bool audMetadataDataFileMounter::LoadDataFile(const CDataFileMgr::DataFile &file)
{	
	char chunkName[k_ChunkNameLength];
	DeriveChunkName(file.m_filename, chunkName, sizeof(chunkName));
	audDisplayf("Loading DLC audio chunk %s from %s, time %d", chunkName, file.m_filename, fwTimer::GetTimeInMilliseconds());

	bool alreadyLoaded = false;
	//If we have a defered unload of this pack, cancel it
	for(int i=0; i < k_MaxDlcPacks; i++)
	{
		if(m_UnloadingChunkNames[i][0] != '\0' && atStringHash(chunkName) == atStringHash(m_UnloadingChunkNames[i]))
		{
			audDisplayf("Was unloading %s, cancelling", chunkName);
			m_UnloadingChunkNames[i][0] = '\0';
			m_FramesToUnload[i] = 0;
			--m_UnloadingChunksCount;
			alreadyLoaded = true;
		}
	}
	bool success = false;
	
	if(alreadyLoaded)
	{
		success = true;
	}
	else
	{
		success = m_MetadataManager.LoadMetadataChunk(chunkName, file.m_filename);
	}

	if(file.m_fileType == CDataFileMgr::AUDIO_GAMEDATA)
	{
		int chunkId = audNorthAudioEngine::GetMetadataManager().FindChunkId(chunkName);
		audMetadataObjectInfo objectInfo;
		if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(ATSTRINGHASH("RADIO_STATIONS_DLC", 0x953BD40D), static_cast<u32>(chunkId), objectInfo))
		{
			audRadioStation::MergeRadioStationList(objectInfo.GetObject<RadioStationList>());
		}
	}

#if !__NO_OUTPUT
	if(success)
	{
		const s32 chunkId = m_MetadataManager.FindChunkId(chunkName);
		audDisplayf("Successfully loaded DLC audio chunk %s, changlist %u", file.m_filename, m_MetadataManager.GetAssetChangelist(chunkId));
	}	
#endif

	if(success && file.m_fileType == CDataFileMgr::AUDIO_DYNAMIXDATA)
	{
		g_AudioEngine.GetDynamicMixManager().InitMixgroupsForChunk(g_AudioEngine.GetDynamicMixManager().GetMetadataManager().FindChunkId(chunkName));
	}

	return success;
}

void audMetadataDataFileMounter::UnloadDataFile(const CDataFileMgr::DataFile &file)
{
	char chunkName[k_ChunkNameLength];
	DeriveChunkName(file.m_filename, chunkName, sizeof(chunkName));
	audDisplayf("Starting unload of DLC audio chunk %s from %s time %d", chunkName, file.m_filename, fwTimer::GetTimeInMilliseconds());
	if(file.m_fileType == CDataFileMgr::AUDIO_DYNAMIXDATA)
	{
		audNorthAudioEngine::GetDynamicMixer().StopScenesFromChunk(chunkName);
	}
	if(file.m_fileType == CDataFileMgr::AUDIO_GAMEDATA)
	{
		const s32 chunkId = audNorthAudioEngine::GetMetadataManager().FindChunkId(chunkName);
		audMetadataObjectInfo objectInfo;
		if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(ATSTRINGHASH("RADIO_STATIONS_DLC", 0x953BD40D), static_cast<u32>(chunkId), objectInfo))
		{
			audDisplayf("Unloading %s which contained DLC radio stations", chunkName);
			audRadioStation::RemoveRadioStationList(objectInfo.GetObject<RadioStationList>());
		}

		g_CollisionAudioEntity.RemoveMacsModelOverridesForChunk(chunkId, ATSTRINGHASH("MACS_MODELS_OVERRIDES", 0x53F9AB71));
		g_CollisionAudioEntity.RemoveMacsModelOverridesForChunk(chunkId, ATSTRINGHASH("MACS_MODEL_OVERRIDE_LIST_GEN9", 0x149D39FA));
	}

	u32 indexToAdd = k_MaxDlcPacks;
	for(int i=0; i < k_MaxDlcPacks; i++)
	{
		if(m_UnloadingChunkNames[i][0] == '\0')
		{
			indexToAdd = i;
			break;
		}
	}
	FatalAssertf(indexToAdd < k_MaxDlcPacks, "Increase the max number of dlc packs for audMetadataFileMounter");
	m_FramesToUnload[indexToAdd] = 1;
	formatf(m_UnloadingChunkNames[indexToAdd], k_ChunkNameLength, chunkName);
	++m_UnloadingChunksCount;
}

void audMetadataDataFileMounter::Update()
{
	if(m_UnloadingChunksCount > 0)
	{
		for(int i=0; i < k_MaxDlcPacks; i++)
		{
			if(m_FramesToUnload[i] > 0)
			{
				--m_FramesToUnload[i];
			}
			else if(m_UnloadingChunkNames[i][0] != '\0')
			{
				naDebugf1("Attempting unload (deferred) chunk %s metadata type %u, time %d, frame %d", m_UnloadingChunkNames[i], m_MetadataManager.GetMetadataTypeHash(), fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
				if(m_MetadataManager.GetMetadataTypeHash() == ATSTRINGHASH("Sounds", 0x94FA081B))
				{
					if(!g_AudioEngine.GetSoundManager().IsAnyChunkBeingUnloaded())
					{
						g_AudioEngine.RequestThreadPause(true);
						g_AudioEngine.EnterUpdateLock();
						g_AudioEngine.RequestThreadPause(false);

						g_AudioEngine.GetSoundManager().StartUnloadingChunk(m_UnloadingChunkNames[i]);
						u32 initialEngineTimeFrames = g_AudioEngine.GetEngineTimeFrames();
						g_AudioEngine.ExitUpdateLock();				

						// Wait for an update so that any sounds using unloaded metadata automatically stop themselves
						while(initialEngineTimeFrames == g_AudioEngine.GetEngineTimeFrames())
						{
							audDisplayf("Waiting for audio engine update. chunkname=%s", m_UnloadingChunkNames[i]);
							sysIpcSleep(5);
						}

						m_MetadataManager.UnloadMetadataChunk(m_UnloadingChunkNames[i]);
						g_AudioEngine.GetSoundManager().ClearDisabledMetadata();
						
						m_UnloadingChunkNames[i][0] = '\0';
						--m_UnloadingChunksCount;
					}
					else
					{
						continue;
					}
				}
				else if(m_MetadataManager.GetMetadataTypeHash() == ATSTRINGHASH("ModularSynth", 0xDFBAF5D4))
				{
					// need to ensure the mixer isn't running while we unload this data
					synthSynthesizer::UnloadMetadataChunk(m_UnloadingChunkNames[i]);

					m_UnloadingChunkNames[i][0] = '\0';
					--m_UnloadingChunksCount;
				}
				else
				{
					m_MetadataManager.UnloadMetadataChunk(m_UnloadingChunkNames[i]);
				
					m_UnloadingChunkNames[i][0] = '\0';
					--m_UnloadingChunksCount;
				}
			}
		}
	}
}

void audMetadataDataFileMounter::DeriveChunkName(const char *fileName, char *chunkNameBuffer, const size_t chunkNameBufferLen)
{
	const char *chunkNamePtr = strrchr(fileName, '/') + 1;
	const size_t chunkNameLen = strchr(chunkNamePtr, '_') - chunkNamePtr;
	audAssertf(chunkNameLen < chunkNameBufferLen, "Chunk name too long for file %s", fileName);
	safecpy(chunkNameBuffer, chunkNamePtr, Min(chunkNameBufferLen, chunkNameLen + 1));
}


void audNorthAudioEngine::RegisterDataFileMounters()
{
	class audWavePackDataFileMounter : public CDataFileMountInterface
	{
	public:

		virtual bool LoadDataFile(const CDataFileMgr::DataFile &file)
		{
			Displayf("Mounting audio pack %s", file.m_filename);

#if RSG_BANK
			// If running with audiofolder, assume all assets are available at audio:/
			if(PARAM_audiofolder.Get())
			{
				return true;
			}
#endif // RSG_BANK

			char packName[32];
			DerivePackName(file.m_filename, packName, sizeof(packName));
			char prefix[9];
			strncpy(prefix, packName, 8);
			prefix[8] = '\0';

			char searchPath[RAGE_MAX_PATH];
			DeriveSearchPath(file.m_filename, packName, searchPath, sizeof(searchPath));

			audWaveSlot::SetSearchPath(prefix, searchPath);

			if(atStringHash(prefix) == ATSTRINGHASH("dlc_Sum2", 0x7E30C9B))
			{
				audNorthAudioEngine::ApplyARC1BankRemappings();
			}

			return true;
		}

		virtual void UnloadDataFile(const CDataFileMgr::DataFile &file)
		{
			Displayf("Unmounting audio pack %s time %d", file.m_filename, fwTimer::GetTimeInMilliseconds());

			char packName[32];
			DerivePackName(file.m_filename, packName, sizeof(packName));
			char prefix[9];
			memset(prefix, 0, 9);
			strncpy(prefix, packName, 8);
			prefix[8] = '\0';

			audWaveSlot::RemoveSearchPath(prefix);			
		}

	private:
		static void DeriveSearchPath(const char *fileName, const char *packName, char *searchPathBuffer, const size_t searchPathBufferLen)
		{
			safecpy(searchPathBuffer, fileName, searchPathBufferLen);
			const size_t packStart = strlen(searchPathBuffer) - strlen(packName);
			audAssert(packStart <= strlen(searchPathBuffer));
			searchPathBuffer[packStart] = 0;
		}

		static void DerivePackName(const char *fileName, char *chunkNameBuffer, const size_t chunkNameBufferLen)
		{
			const char *chunkNamePtr = strrchr(fileName, '/') + 1;
			const size_t chunkNameLen = strlen(chunkNamePtr);
			audAssertf(chunkNameLen < chunkNameBufferLen, "Pack name too long for file %s", fileName);
			safecpy(chunkNameBuffer, chunkNamePtr, Min(chunkNameBufferLen, chunkNameLen + 1));
		}
	};

	class audSpeechDataFileMounter : public CDataFileMountInterface
	{
	public:
		audSpeechDataFileMounter()
		{

		}

		virtual bool LoadDataFile(const CDataFileMgr::DataFile &file)
		{	
			char chunkName[32];
			DeriveChunkName(file.m_filename, chunkName, sizeof(chunkName));
			audDisplayf("Loading DLC speech audio chunk %s from %s", chunkName, file.m_filename);
			return audSpeechSound::LoadSpeechMetadata(chunkName, file.m_filename);
		}

		virtual void UnloadDataFile(const CDataFileMgr::DataFile &file) 
		{
			char chunkName[32];
			DeriveChunkName(file.m_filename, chunkName, sizeof(chunkName));
			audDisplayf("Unloading DLC speech audio chunk %s from %s", chunkName, file.m_filename);
			audSpeechSound::UnloadSpeechMetadata(chunkName);
		}

	private:

		static void DeriveChunkName(const char *fileName, char *chunkNameBuffer, const size_t chunkNameBufferLen)
		{
			const char *chunkNamePtr = strrchr(fileName, '/') + 1;
			const size_t chunkNameLen = strchr(chunkNamePtr, '_') - chunkNamePtr;
			audAssertf(chunkNameLen < chunkNameBufferLen, "Chunk name too long for file %s", fileName);
			safecpy(chunkNameBuffer, chunkNamePtr, Min(chunkNameBufferLen, chunkNameLen + 1));
		}

	};

	static audMetadataDataFileMounter s_SoundDataMounter(g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager());	
	static audMetadataDataFileMounter s_CurveDataMounter(g_AudioEngine.GetCurveRepository().GetMetadataManager());	
	static audMetadataDataFileMounter s_GameDataMounter(audNorthAudioEngine::GetMetadataManager());		
	static audMetadataDataFileMounter s_DynamicMixerMounter(g_AudioEngine.GetDynamicMixManager().GetMetadataManager());	
	static audMetadataDataFileMounter s_SynthDataMounter(synthSynthesizer::GetMetadataManager());

	static audSpeechDataFileMounter s_SpeechDataMounter;
	static audWavePackDataFileMounter s_WavePackMounter;

	sm_SoundDlcMounter = &s_SoundDataMounter;
	sm_DynamicMixDlcMounter = &s_DynamicMixerMounter;
	sm_GameObjectDlcMounter = &s_GameDataMounter;
	sm_CurveDlcMounter = &s_CurveDataMounter;
	sm_SynthDlcMounter = &s_SynthDataMounter;
	
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_SOUNDDATA, &s_SoundDataMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_CURVEDATA, &s_CurveDataMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_GAMEDATA, &s_GameDataMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_SPEECHDATA, &s_SpeechDataMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_DYNAMIXDATA, &s_DynamicMixerMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_SYNTHDATA, &s_SynthDataMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AUDIO_WAVEPACK, &s_WavePackMounter, eDFMI_UnloadFirst);
}

void audNorthAudioEngine::StartUserUpdateFunction()
{
	//Set the game specific audio thread update
	g_AudioEngine.SetUserGameUpdateFunction(&audNorthAudioEngine::UpdateAudioThread);
}

void audNorthAudioEngine::StopUserUpdateFunction()
{
	//Kill the game update function
	g_AudioEngine.SetUserGameUpdateFunction(NULL);
}

void audNorthAudioEngine::UpdateAudioThread()
{
	AUDCATEGORYCONTROLLERMANAGER.Update();

#if AUD_ATOMIC_CATEGORIES
	// If we re-enable AUD_ATOMIC_CATEGORIES, this seems dodgy.
	g_AudioEngine.GetCategoryManager().CommitCategorySettings();
#endif
}

void audNorthAudioEngine::StartEnvironmentLocalObjectListUpdate()
{
	if(!fwTimer::IsGamePaused())
	{
		sm_Environment.LocalObjectsList_StartAsyncUpdate();
		s_HasScanStarted = true;
	}
}

void audNorthAudioEngine::FinishEnvironmentLocalObjectListUpdate()
{
	if(s_HasScanStarted)
	{
		sm_Environment.LocalObjectsList_EndAsyncUpdate();
		s_HasScanStarted = false;
	}
}

extern bool g_DisableNPCVehicleDSP, g_DisablePlayerVehicleDSP;

XPARAM(headphones);
void audNorthAudioEngine::StartUpdate()
{
#if GTA_REPLAY
	static bool s_HaveInitialisedReplayTracks = false;
	if(CReplayMgr::IsEditorActive() && !s_HaveInitialisedReplayTracks)
	{
		CReplayAudioTrackProvider::InitReplayRadioStations();
		s_HaveInitialisedReplayTracks = true;
	}
#endif // GTA_REPLAY
#if RSG_PC
	if(!PARAM_nominspecaudio.Get())
	{
		static bool forceMinSpec = PARAM_minspecaudio.Get();

		const u32 uMinCores = 4; 
		const float cfMinClockSpeed = 2.6f; 
	
		bool isMinSpec = (sysIpcGetProcessorCount() <= uMinCores) || (sysTimerConsts::CpuSpeed < cfMinClockSpeed) || forceMinSpec;

#if __BANK
		if(g_OverrideCPULimitedAudioSetting)
		{
			isMinSpec = g_ForceMinSpecMode;
		}
#endif

		g_DisableNPCVehicleDSP = isMinSpec;
		audDriver::GetVoiceManager().SetCPULimitedAudio(isMinSpec);
	}

#endif

	sm_Microphones.UpdatePlayerHeadMatrix();

	sm_Environment.ResetHasNaveMeshInfoBeenUpdated();

	bool isAlreadyFadedOut = sm_IsFadedToBlack;
	sm_IsFadedToBlack = camInterface::IsFadedOut();
	if(!isAlreadyFadedOut && sm_IsFadedToBlack)
	{
		sm_ScreenFadedOutThisFrame = true;
	}

	const bool isMixerRunning = audDriver::GetMixer() != NULL;

	PF_PUSH_TIMEBAR("Audio Wait on Thread");
	if(isMixerRunning && !sm_IsShuttingDown)
	{
#if RSG_ORBIS
		audMixerDeviceOrbis *device = ((audMixerDeviceOrbis*)audDriver::GetMixer());
		
		// We need to have a valid user id to associate the pad
		device->SetPadSpeakerUserId(g_rlNp.GetUserServiceId(CControlMgr::GetMainPlayerIndex()));

		// Ensure we have up to date info on the port status
		device->UpdatePortState();
#endif
#if GTA_REPLAY
		if(audDriver::GetMixer() && audDriver::GetMixer()->IsCapturing() && audDriver::GetMixer()->IsFrameRendering())
		{
			g_AudioEngine.TriggerUpdate();
			g_AudioEngine.WaitForAudioFrame();
			audDriver::GetMixer()->EnterReplaySwitchLock();
			audDriver::GetMixer()->TriggerUpdate();
			audDriver::GetMixer()->WaitOnMixBuffers();
			audDriver::GetMixer()->ExitReplaySwitchLock();
		}
#endif
		audDriver::GetMixer()->WaitOnThreadCommandBufferProcessing();
	}
	PF_POP_TIMEBAR();

	if (REPLAY_ONLY(!CReplayMgr::IsPreCachingScene() &&)
		((fwTimer::IsUserPaused() && !fwTimer::GetSingleStepThisFrame()) || 
		fwTimer::IsGamePaused()
		BANK_ONLY(|| g_CutsceneAudioPaused)))
	{
 		Pause();
	}
	else
	{
		UnPause();
	}

	UpdateDataSet();
	
#if __DEV
	// JP: This can cause asserts at startup as it can be called before the viewport is set up
	// Seems to make more sense to just use the focus entity selection stuff.
	// Could probably get rid of g_AudioDebugEntity altogether and just use the focus entity array
	//g_AudioDebugEntity = CDebugScene::GetEntityUnderMouse();
	g_AudioDebugEntity = CDebugScene::FocusEntities_Get(0);
#endif

	// Cache this as its not safe to query on the audio update thread
	sm_IsRenderingHoodMountedVehicleCam = camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera() && !camInterface::GetCinematicDirector().IsRenderingCinematicPointOfViewCamera();
	sm_IsRenderingFirstPersonVehicleCam = camInterface::IsRenderedCameraInsideVehicle();
	const camBaseCamera* dominantRenderedCamera = camInterface::GetDominantRenderedCamera();

	// These vehicles have turrets that are inside the vehicle, so ensure we keep the regular interior scene active
	if(dominantRenderedCamera)
	{
		if(dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_DUNE3", 0x8501FD84) ||
		   dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_APC_CANNON", 0xD3FE954B) ||
		   dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_APC_MISSILE", 0xD4FCAB99))
		{
			sm_IsRenderingFirstPersonVehicleCam = true;
			sm_IsRenderingFirstPersonVehicleTurretCam = true;
		}
		// Special case for vehicle mounted turret cams - player is visually outside the vehicle when manning these so first person vehicle scene occludes things weirdly
		else if(dominantRenderedCamera &&
			(dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_INSURGENT", 0x3F451CCC) || 
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_INSURGENT3", 0x9722AEFA) ||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_LIMO2", 0x6CCE4543) ||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_BOXVILLE", 0xCB2DF32E) ||	
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_AATRAILER_DUALAA", 0xE52B5A93)	||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_HALFTRACK_TWINMG", 0x546EBC04) ||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_HALFTRACK_QUADMG", 0x5030429E) ||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_AATRAILER_QUADMG", 0xE53AE034) ||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_AATRAILER_MISSILE", 0xE9FA695B) ||
			dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_TECHNICAL", 0x33DCCD41)))
		{
			sm_IsRenderingFirstPersonVehicleCam = false;
			sm_IsRenderingFirstPersonVehicleTurretCam = true;
		}
		else
		{
			sm_IsRenderingFirstPersonVehicleTurretCam = false;
		}
	}
	else
	{
		sm_IsRenderingFirstPersonVehicleTurretCam = false;
	}

#if GTA_REPLAY    
    if (CReplayMgr::IsEditorActive() && dominantRenderedCamera && dominantRenderedCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
    {
        sm_IsRenderingReplayFreeCamera = true;
    }
    else
    {
        sm_IsRenderingReplayFreeCamera = false;
    }

    if(CReplayMgr::IsEditorActive())
    {
        sm_IsRenderingFirstPersonVehicleCam |= (CReplayMgr::IsRenderedCamInVehicle() && CReplayMgr::GetCamInFirstPersonFlag());
    }
#endif

	// World probes to get the water heights, needs to run in the main thread. 
	g_AmbientAudioEntity.UpdateShoreLineWaterHeights();
	BANK_ONLY (g_AmbientAudioEntity.UpdateShoreLineEditor();)

	// Update our occlusion data based on if interiors have streamed in/out
	sm_OcclusionManager.UpdateInteriorMetadata();

	// Update our local environment metrics - this line-scans around the camera to build up a detailed local view of the world
	// Same's true of the static emitter update that needs to grab interior info for non-linked emitters
	if(!fwTimer::IsGamePaused())
	{
		sm_OcclusionManager.Update();
		sm_Environment.UpdateLocalEnvironment();
		g_EmitterAudioEntity.CheckStaticEmittersForInteriorInfo();
	}

	s32 speakerOutputOption = CPauseMenu::GetMenuPreference(PREF_SPEAKER_OUTPUT);

	bool headphones = false;
	audOutputMode outputMode = audDriver::GetHardwareOutputMode();

	static AmbisonicDecoderMode modeArray[AUD_FRONT_SPEAKER_NUM][AUD_REAR_SPEAKER_NUM] =
	{	{kAmbisonicsFwRr, kAmbisonicsFwRm, kAmbisonicsFwRs},
		{kAmbisonicsFmRr, kAmbisonicsFmRm, kAmbisonicsFmRs},
		{kAmbisonicsFnRr, kAmbisonicsFnRm, kAmbisonicsFnRs}
	};

	// On PS4 we can't automatically detect the headset, so if the user selects Headphones and then enables Pulse Headset Profile, force surround.
	
#if RSG_PS3 || RSG_ORBIS
	if(sm_ShouldTriggerPulseHeadset)
	{
		// Sony Wireless Headset
		if(outputMode == AUD_OUTPUT_5_1)
		{
			g_AudioEngine.GetAmbisonics().SetEnvironmentDecoder(kAmbisonicsFmRr);
		}
		else
		{
			g_AudioEngine.GetAmbisonics().SetEnvironmentDecoder(kAmbisonicsHeadphones);
			headphones = true;
		}
	}
	else
#endif // RSG_PS3 || RSG_ORBIS
	{		
		switch(speakerOutputOption)
		{
		case 0: // Speakers
			if(outputMode == AUD_OUTPUT_5_1)
			{
				s32 frontPos = CPauseMenu::GetMenuPreference(PREF_SS_FRONT);
				s32 rearPos = CPauseMenu::GetMenuPreference(PREF_SS_REAR);
				AmbisonicDecoderMode ambiMode = modeArray[frontPos][rearPos];
				
				g_AudioEngine.GetAmbisonics().SetEnvironmentDecoder(ambiMode);
			}
			else
			{
				g_AudioEngine.GetAmbisonics().SetEnvironmentDecoder(kAmbisonicsStereo);
			}
			break;
		case 1: // TV
		case 3: // Stereo Speakers
			outputMode = AUD_OUTPUT_STEREO;
			g_AudioEngine.GetAmbisonics().SetEnvironmentDecoder(kAmbisonicsStereo);
			break;
		case 2: // Headphones
			headphones = true;
			outputMode = AUD_OUTPUT_STEREO;
			g_AudioEngine.GetAmbisonics().SetEnvironmentDecoder(kAmbisonicsHeadphones);
			break;
		
		default:
			naErrorf("Unknown speaker output option");
			CPauseMenu::SetMenuPreference(PREF_SPEAKER_OUTPUT, 0); //Unknown speaker output so set it to speakers
			break;
		}
	}

	g_AudioEngine.GetEnvironment().SetDownmixMode(outputMode);
	g_AudioEngine.GetEnvironment().SetUsingHeadphones(headphones);

	if(!sm_RunUpdateInSeperateThread)
	{
		Update();
	}

	// move on the particle thread buffer index
	if(!fwTimer::IsGamePaused())
	{
		g_AmbientAudioEntity.IncrementAudioEffectWriteIndex();
	}

	//Game updates for audio entities
	NA_RADIO_ENABLED_ONLY(g_RadioAudioEntity.GameUpdate());
	
	g_SpeechManager.UpdateAnimalBanks();
	g_ScriptAudioEntity.UpdateConversation();
	g_GlassAudioEntity.Update();

	audPedAudioEntity::UpdateClass();
	
	audSoundManager &soundManager = g_AudioEngine.GetSoundManager();
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Lighting.CurrentExposure", 0x4FBAC7F6), PostFX::g_adaptation.GetUpdateExposure());
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Lighting.TargetExposure", 0x31FC0865), PostFX::g_adaptation.GetUpdateTargetExposure());
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Lighting.SunVisibility", 0x6E1B558D), CVisualEffects::GetSunVisibility());
	
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player && player->GetSpecialAbility() && player->GetSpecialAbility()->IsFadingOut())
	{
		sm_IsPlayerSpecialAbilityFadingOut = true;
	}
	else
	{
		sm_IsPlayerSpecialAbilityFadingOut = false;
	}

	sm_IsAudioUpdateCurrentlyRunning = true;

	sysIpcSignalSema(sm_RunUpdateSema);
}

void audNorthAudioEngine::FinishUpdate()
{
	PF_FUNC(NorthAudioUpdateTimer_FinishUpdate);
	PF_PUSH_TIMEBAR("Wait on sm_UpdateFinishedSema");
	sysIpcWaitSema(sm_UpdateFinishedSema);
	PF_POP_TIMEBAR();
	sm_IsAudioUpdateCurrentlyRunning = false;

	FinishEnvironmentLocalObjectListUpdate();
	UpdateVehicleCategories();

	audVehicleAudioEntity::ClassFinishUpdate();
	sm_AudioController.PostUpdate();

	sm_OcclusionManager.FinishUpdate();

#if __BANK
	audWaveSlot::DebugDrawSlotSizeErrors();
	naEnvironmentGroup::DebugDrawEnvironmentGroupPool();
	audEnvironmentSound::DebugDrawInvalidCategorySounds();
	g_AmbientAudioEntity.DebugDraw();
	g_CollisionAudioEntity.DrawDebug();
	g_RadioAudioEntity.DebugDrawStations();
	g_InteractiveMusicManager.DebugDraw();
	g_ScriptAudioEntity.DrawDebug();
	sm_SuperConductor.GetVehicleConductor().VehicleConductorDebugInfo();
	audVehicleAudioEntity::UpdateCarRecordingTool();
	if(!PARAM_inGameAudioWorldSectors.Get())
	{
		sm_Environment.GenerateAudioWorldSectorsInfo();
	}

	audCarAudioEntity::UpdateVolumeCaptureUpdateThread();
	audWarpManager::Update();
#endif

	if(!fwTimer::IsGamePaused())
	{
		CONDUCTORS_ONLY(sm_SuperConductor.ProcessUpdate());
		g_WeaponAudioEntity.ProcessCachedWeaponFireEvents();
#if USE_GUN_TAILS
		sm_GunFireAudioEntity.Update(fwTimer::GetTimeInMilliseconds());
#endif
#if __DEV
		g_WeaponAudioEntity.DebugDraw();
		naEnvironment::DebugDraw();  
#endif
		g_CollisionAudioEntity.TriggerDeferedImpacts();
		if( !Water::IsCameraUnderwater())
		{
			sm_Environment.UpdateInterestingObjects();
		}
	}

	naAudioEntity::ProcessDeferredSounds();
	audEntity::ProcessBatchedSoundRequests(0); // 0 timestep for the second update this frame
	audEntity::ResetDeletedObjectLists();

	sm_DynamicMixer.FinishUpdate();
	DYNAMICMIXMGR.PostUpdateProcess();
	g_InteractiveMusicManager.PostUpdate();
	audExplosionAudioEntity::Update();
	
	const bool isMixerRunning = audDriver::GetMixer() != NULL;
	if(isMixerRunning)
		audDriver::GetMixer()->FlagThreadCommandBufferReadyToProcess();

	sm_ScreenFadedOutThisFrame = false;

	if(sm_DynamicMixDlcMounter)
	{
		sm_DynamicMixDlcMounter->Update();
	}
	if(sm_SoundDlcMounter)
	{
		sm_SoundDlcMounter->Update();
	}
	if(sm_GameObjectDlcMounter)
	{
		sm_GameObjectDlcMounter->Update();
	}
	if(sm_CurveDlcMounter)
	{
		sm_CurveDlcMounter->Update();
	}
	if(sm_SynthDlcMounter)
	{
		sm_SynthDlcMounter->Update();
	}

	sm_CinematicThirdPersonAimCameraActive = false;
	const camThirdPersonAimCamera* thirdPersonAimCamera	= camInterface::GetGameplayDirector().GetThirdPersonAimCamera();	

	if (thirdPersonAimCamera && thirdPersonAimCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()))
	{
		const camThirdPersonPedAssistedAimCamera* assistedAimCamera = static_cast<const camThirdPersonPedAssistedAimCamera*>(thirdPersonAimCamera);
		sm_CinematicThirdPersonAimCameraActive = assistedAimCamera->GetCinematicMomentBlendLevel() >= sm_ThirdPersonCameraBlendThreshold;
	}

#if __BANK
	if(g_WarpToStaticEmitter && g_RequestedWarpEmitter)
	{		
		Vector3 warpPos(g_RequestedWarpEmitter->Position.x, g_RequestedWarpEmitter->Position.y, g_RequestedWarpEmitter->Position.z);
		Vector3 zero(0.f,0.f,0.f);
		CWarpManager::SetWarp(warpPos, zero, 0.f, true, true);
		g_RequestedWarpEmitter = NULL;
	}
	if(g_WarpToAmbientZone && g_RequestedWarpAmbZone)
	{		
		Vector3 warpPos(g_RequestedWarpAmbZone->ActivationZone.Centre);
		Vector3 zero(0.f,0.f,0.f);
		CWarpManager::SetWarp(warpPos, zero, 0.f, true, true);
		g_RequestedWarpAmbZone = NULL;
	}
	if(g_WarpToInterior && g_RequestedWarpInterior)
	{		
		Vec3V warpPos = Vec3V(V_ZERO);
		g_RequestedWarpInterior->GetPosition(warpPos);
		Vector3 zero(0.f,0.f,0.f);
		CWarpManager::SetWarp(VEC3V_TO_VECTOR3(warpPos), zero, 0.f, true, true);
		g_RequestedWarpInterior = NULL;
	}
#endif
}

void audNorthAudioEngine::ApplyARC1BankRemappings()
{
	if (sm_HasAppliedARC1BankRemappings)
	{
		return;
	}

	// See url:bugstar:6176865 - Replay : Can we compress the replay editor banks so that they only use one slot, instead of four?
	SOUNDFACTORY.AddBankRemapping(ATSTRINGHASH("DLC_REPLAYED/REPLAY_01", 0x4534DBDD), ATSTRINGHASH("DLC_SUM20/Replay_SFX", 0x977D1731));
	SOUNDFACTORY.AddBankRemapping(ATSTRINGHASH("DLC_REPLAYED/REPLAY_02", 0x4A57E623), ATSTRINGHASH("DLC_SUM20/Replay_SFX", 0x977D1731));
	SOUNDFACTORY.AddBankRemapping(ATSTRINGHASH("DLC_REPLAYED/REPLAY_03", 0x63801873), ATSTRINGHASH("DLC_SUM20/Replay_SFX", 0x977D1731));
	SOUNDFACTORY.AddBankRemapping(ATSTRINGHASH("DLC_REPLAYED/REPLAY_04", 0x69BD24ED), ATSTRINGHASH("DLC_SUM20/Replay_SFX", 0x977D1731));

	sm_HasAppliedARC1BankRemappings = true;
}

void audNorthAudioEngine::UpdateDataSet()
{
	const s32 framesToWait = 5;
#if GTA_REPLAY
	LoadReplayAudioBanks();
#endif
	
	if(sm_IsMPDataRequested || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::LoadMPData))
	{
		switch(sm_DataSetState)
		{
		case Loaded_SP:
			UnloadSPData();
			sm_DataSetStateCount = 0;			
			sm_DataSetState = Stopping_SP;
			BANK_ONLY(g_AudioEngine.GetRemoteControl().SetOutgoingMessagesEnabled(false);)
			break;
		case Stopping_SP:
			sm_DataSetStateCount++;
			if(sm_DataSetStateCount > framesToWait)
			{			
				LoadMPData();
				sm_DataSetState = Loaded_MP;
				BANK_ONLY(g_AudioEngine.GetRemoteControl().SetOutgoingMessagesEnabled(true);)
			}
			break;
		case Stopping_MP:
			sm_DataSetStateCount++;
			if(sm_DataSetStateCount > framesToWait)
			{
				LoadMPData();
				sm_DataSetState = Loaded_MP;
				BANK_ONLY(g_AudioEngine.GetRemoteControl().SetOutgoingMessagesEnabled(true);)
			}
			break;
		case Loaded_MP:
			break;
		}
	}
	else
	{
		switch(sm_DataSetState)
		{
		case Loaded_SP:
			break;
		case Stopping_SP:
			sm_DataSetStateCount++;
			if(sm_DataSetStateCount > framesToWait)
			{
				LoadSPData();
				sm_DataSetState = Loaded_SP;
				BANK_ONLY(g_AudioEngine.GetRemoteControl().SetOutgoingMessagesEnabled(true);)
			}
			break;
		case Stopping_MP:
			sm_DataSetStateCount++;
			if(sm_DataSetStateCount > framesToWait)
			{
				LoadSPData();
				sm_DataSetState = Loaded_SP;
				BANK_ONLY(g_AudioEngine.GetRemoteControl().SetOutgoingMessagesEnabled(true);)
			}
			break;
		case Loaded_MP:
			BANK_ONLY(g_AudioEngine.GetRemoteControl().SetOutgoingMessagesEnabled(false);)
			UnloadMPData();
			sm_DataSetStateCount = 0;
			sm_DataSetState = Stopping_MP;
			break;

		}
	}
}

void audNorthAudioEngine::LoadMPData()
{
	audDisplayf("Loading multiplayer metadata");

	// If we enter here in the Stopping_MP state then the data is already loaded
	if(sm_DataSetState != Stopping_MP)
	{
		// Unload SP data (preserve buffer)
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().UnloadMetadataChunk("SinglePlayer", false);

		
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().LoadMetadataChunk("GTAOnline", 
																							"audio:/config/dlc_gtao_sounds.dat",
																							sm_SPSoundDataPtr,
																							sm_SPSoundDataSize);

		GetMetadataManager().LoadMetadataChunk("GTAOnline", "audio:/config/dlc_gtao_game.dat");
		g_AudioEngine.GetDynamicMixManager().GetMetadataManager().LoadMetadataChunk("GTAOnline", "audio:/config/dlc_gtao_mix.dat");
	
		if(g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().FindChunkId("GTAOnline") == -1)
		{
			Quitf(ERR_AUD_GTA_ONLINE_LOAD,"Failed to load GTAOnline audio data");
		}

		audDisplayf("Loaded GTAOnline audio assets, source CLs: sounds %u, game objects: %u, dynamic mixer: %u", 
											g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetAssetChangelist(-1),
											sm_MetadataMgr.GetAssetChangelist(-1),
											g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetAssetChangelist(-1));
	

		g_AudioEngine.GetSoundManager().ClearDisabledMetadata();
		
	
		g_AudioEngine.GetDynamicMixManager().InitMixgroupsForChunk(g_AudioEngine.GetDynamicMixManager().GetMetadataManager().FindChunkId("GTAOnline"));

		g_EmitterAudioEntity.ReInitStaticEmitters();
		g_AmbientAudioEntity.CheckAndAddZones();
		audPedAudioEntity::InitMPSpecificSounds();

		// See url:bugstar:5745340 - GTAO - Consolidate 321_GO and MP_WASTED banks into GTAO Snacks
		SOUNDFACTORY.AddBankRemapping(ATSTRINGHASH("SCRIPT/HUD_321_GO", 0x1F90E64), ATSTRINGHASH("DLC_GTAO/SNACKS", 0x81CAD0C));
		SOUNDFACTORY.AddBankRemapping(ATSTRINGHASH("SCRIPT/MP_WASTED", 0x26ED9800), ATSTRINGHASH("DLC_GTAO/SNACKS", 0x81CAD0C));

#if RSG_PC
		// Need to manually point the GTA Online pack at the loose files in the titleupdate folder, otherwise the file system
		// gets confused and tries to read from the original .rpf file that we shipped with. See B*2460076 etc.
		if(!PARAM_audiofolder.Get())
		{
			audWaveSlot::SetSearchPath("DLC_GTAO", "update:/x64/audio/sfx/");
		}
#endif
		g_FrontendAudioEntity.SetMPSpecialAbilityBankId();	// this grabs the bank id for the GTAO/MP_RESIDENT bank
	}
	else
	{
		g_AudioEngine.GetSoundManager().CancelUnloadingChunk("GTAOnline");
		g_AudioEngine.GetDynamicMixManager().CancelUnloadingChunk("GTAOnline");
		g_AudioEngine.GetSoundManager().ClearDisabledMetadata();

		const s32 chunkId = sm_MetadataMgr.FindChunkId("GTAOnline");
		if(naVerifyf(chunkId != -1, "Failed to find loaded gameobject chunk GTAOnline"))
		{
			sm_MetadataMgr.SetChunkEnabled(chunkId, true);
		}

		g_EmitterAudioEntity.ReInitStaticEmitters();
		g_AmbientAudioEntity.CheckAndAddZones();
	}

	audEnvironmentSound::SetMixGroupPitchFrequencyScalingEnabled(true);
}

void audNorthAudioEngine::LoadSPData()
{	
	audDisplayf("Loading singleplayer metadata");

	// Unload multiplayer data if we have any loaded (preserve buffer for sounds)
	g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().UnloadMetadataChunk("GTAOnline",false);
	
	GetMetadataManager().UnloadMetadataChunk("GTAOnline");
	g_AudioEngine.GetDynamicMixManager().GetMetadataManager().UnloadMetadataChunk("GTAOnline");

	g_AudioEngine.GetSoundManager().ClearDisabledMetadata();

	// load SP data
	g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().LoadMetadataChunk("SinglePlayer", 
																						"audio:/config/sp_sounds.dat",
																						sm_SPSoundDataPtr,
																						sm_SPSoundDataSize);

		
	g_EmitterAudioEntity.ReInitStaticEmitters();
	g_AmbientAudioEntity.CheckAndAddZones();

	g_FrontendAudioEntity.SetSPSpecialAbilityBankId();	

	SOUNDFACTORY.RemoveBankRemapping(ATSTRINGHASH("SCRIPT/HUD_321_GO", 0x1F90E64));
	SOUNDFACTORY.RemoveBankRemapping(ATSTRINGHASH("SCRIPT/MP_WASTED", 0x26ED9800));
}

void audNorthAudioEngine::UnloadMPData()
{
	naDisplayf("Unloading MP Data time %u, frame %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
	// Ensure no score is playing (it can hold onto Mood object references)
#if GTA_REPLAY
		bool isReplayTransition = (CReplayMgr::IsEditModeActive() && CReplayCoordinator::IsExportingToVideoFile() && CVideoEditorPlayback::IsLoading());
#else
		bool isReplayTransition = false;
#endif
	if (!isReplayTransition)
		g_InteractiveMusicManager.Reset();

	g_AudioEngine.GetSoundManager().StartUnloadingChunk("GTAOnline");
	sm_DynamicMixer.StopOnlineScenes();
	g_AudioEngine.GetDynamicMixManager().StartUnloadingChunk("GTAOnline");

	const s32 chunkId = sm_MetadataMgr.FindChunkId("GTAOnline");
	if(chunkId != -1)
	{
		const audMetadataChunk &chunk = sm_MetadataMgr.GetChunk(chunkId);
		RegisterGameObjectMetadataUnloading(chunk);
		sm_MetadataMgr.SetChunkEnabled(chunkId, false);
	}

	g_EmitterAudioEntity.ReInitStaticEmitters();
	g_AmbientAudioEntity.CheckAndAddZones();

#if RSG_PC
	audWaveSlot::RemoveSearchPath("DLC_GTAO");
#endif

	audEnvironmentSound::SetMixGroupPitchFrequencyScalingEnabled(false);
}

void audNorthAudioEngine::UnloadSPData()
{
	naDisplayf("Unloading SP Data time %u, frame %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
	g_AudioEngine.GetSoundManager().StartUnloadingChunk("SinglePlayer");
}

void audNorthAudioEngine::LoadMapData()
{
#if RSG_BANK
	if(!PARAM_audiodesigner.Get())
	{
		return;
	}

	const char *pLevelName = audNorthAudioEngine::GetCurrentAudioLevelName(); 
	audDisplayf("Current audio level: %s", pLevelName);

	size_t len = strlen(pLevelName);
	
	// ignore "gta5_"
	if(len > 5)
	{
		const char *suffix = pLevelName + 5;
		char fileName[RAGE_MAX_PATH];
		char chunkName[RAGE_MAX_PATH];

		formatf(chunkName, "dlc%s", suffix);

		// Sounds
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().LoadMetadataChunk(chunkName, formatf(fileName, "audio:/config/dlc%s_sounds.dat", suffix));
		// Curves
		g_AudioEngine.GetCurveRepository().GetMetadataManager().LoadMetadataChunk(chunkName, formatf(fileName, "audio:/config/dlc%s_curves.dat", suffix));
		// DynamicMixer
		g_AudioEngine.GetDynamicMixManager().GetMetadataManager().LoadMetadataChunk(chunkName, formatf(fileName, "audio:/config/dlc%s_mix.dat", suffix));
		// Synth
		synthSynthesizer::GetMetadataManager().LoadMetadataChunk(chunkName, formatf(fileName, "audio:/config/dlc%s_amp.dat", suffix));
		// Game Objects
		sm_MetadataMgr.LoadMetadataChunk(chunkName, formatf(fileName, "audio:/config/dlc%s_game.dat", suffix));
	}
#endif // RSG_BANK
}

void audNorthAudioEngine::UnloadMapData()
{
#if RSG_BANK
	if(!PARAM_audiodesigner.Get())
	{
		return;
	}
	const char *pLevelName = audNorthAudioEngine::GetCurrentAudioLevelName(); 

	size_t len = strlen(pLevelName);

	// ignore "gta5_"
	if(len > 5)
	{
		const char *suffix = pLevelName + 5;		
		char chunkName[RAGE_MAX_PATH];
		formatf(chunkName, "dlc%s", suffix);

		// Sounds
		g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().UnloadMetadataChunk(chunkName);
		// Curves
		g_AudioEngine.GetCurveRepository().GetMetadataManager().UnloadMetadataChunk(chunkName);
		// DynamicMixer
		g_AudioEngine.GetDynamicMixManager().GetMetadataManager().UnloadMetadataChunk(chunkName);
		// Synth
		synthSynthesizer::GetMetadataManager().UnloadMetadataChunk(chunkName);
		// Game Objects
		sm_MetadataMgr.UnloadMetadataChunk(chunkName);
	}
#endif // RSG_BANK
}

void audNorthAudioEngine::RegisterGameObjectMetadataUnloading(const audMetadataChunk& chunk)
{
	// Clear out all CMloModelInfo's that are referencing any InteriorSettings/InteriorRoom GO we're about to unload
	CInteriorProxy::Pool* pool = CInteriorProxy::GetPool();
	if(pool)
	{
		s32 numSlots = pool->GetSize();
		for(s32 slot = 0; slot < numSlots; slot++)
		{
			CInteriorProxy* intProxy = pool->GetSlot(slot);
			if(intProxy)
			{
				CInteriorInst* intInst = intProxy->GetInteriorInst();
				if(intInst && intInst->IsPopulated())
				{
					CMloModelInfo *modelInfo = intInst->GetMloModelInfo();
					if(naVerifyf(modelInfo, "NULL CMLOModelInfo"))
					{
						const s32 numRooms = intInst->GetNumRooms();

						// Start at roomIdx 1 as roomdIdx 0 is always limbo(outside) which we won't have settings for.
						for(s32 roomIdx = 1; roomIdx < numRooms; roomIdx++)
						{
							const InteriorSettings* intSettings;
							const InteriorRoom* intRoom;
							modelInfo->GetAudioSettings(roomIdx, intSettings, intRoom);
							if((intSettings && chunk.IsObjectInChunk(intSettings))
								|| (intRoom && chunk.IsObjectInChunk(intRoom)))
							{
								modelInfo->SetAudioSettings(roomIdx, NULL, NULL);
							}
						}
					}
				}
			}
		}
	}

	// Also clear out any references to those InteriorSettings/InteriorRoom GO's in the environmentGroups
	audNorthAudioEngine::GetAudioController()->RegisterGameObjectMetadataUnloading(chunk);
}

#if GTA_REPLAY
void audNorthAudioEngine::LoadReplayAudioBanks()
{
	BANK_ONLY(bool forceLoad = sm_ForceSuperSlowMoVideoEditor || sm_ForceSlowMoVideoEditor);

	if((CReplayMgr::IsEditModeActive() BANK_ONLY(||forceLoad)) && !sm_AreReplayBanksLoaded)
	{
		sm_AreReplayBanksLoaded = g_ScriptAudioEntity.ReplayLoadScriptBank(ATSTRINGHASH("DLC_SUM20/Replay_SFX", 0x977D1731), 10); 
	}
}

bool audNorthAudioEngine::PumpReplayAudio()
{
	bool addedAudio = false;
	if(audDriver::GetMixer() && audDriver::GetMixer()->IsCapturing() && audDriver::GetMixer()->IsFrameRendering())
	{
		const float kfFrameStep = 1.0f / 31.0f;
		// Only trigger an extra engine update when exporting at 30fps or less to make sure enough audio is generated this frame
		if(audDriver::GetMixer()->GetTotalAudioTimeNs() < audDriver::GetMixer()->GetTotalVideoTimeNs() && CReplayMgr::GetExportFrameStep() >= kfFrameStep)
		{
			g_AudioEngine.TriggerUpdate();
			g_AudioEngine.WaitForAudioFrame();
		}

		while(audDriver::GetMixer()->GetTotalAudioTimeNs() < audDriver::GetMixer()->GetTotalVideoTimeNs())
		{
			audDriver::GetMixer()->EnterReplaySwitchLock();
			audDriver::GetMixer()->TriggerUpdate();
			audDriver::GetMixer()->WaitOnMixBuffers();
			audDriver::GetMixer()->ExitReplaySwitchLock();
			//Displayf("Catch Up Audio: %LLu chasing (%LLu)", 
			//	NANOSECONDS_TO_ONE_H_NS_UNITS(audDriver::GetMixer()->GetTotalAudioTimeNs()), 
			//	NANOSECONDS_TO_ONE_H_NS_UNITS(audDriver::GetMixer()->GetTotalVideoTimeNs()));	
			addedAudio = true;
		}

	}
	return addedAudio;
}
#endif

void audNorthAudioEngine::AudioUpdateThread(void*)
{
	CSystem::SetThisThreadId(SYS_THREAD_AUDIO);
	const bool isMixerRunning = audDriver::GetMixer() != NULL;
	if(isMixerRunning)
	{
		const u32 commandBufferSize = 8192 WIN32PC_ONLY( *COMMAND_BUFFER_SIZE_MUTIPLIER );
		audDriver::GetMixer()->InitClientThread("NorthAudioUpdateThread", commandBufferSize);
	}
	while(true)
	{
		TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"Wait on sm_RunUpdateSema");
		sysIpcWaitSema(sm_RunUpdateSema);
		TELEMETRY_END_ZONE(__FILE__,__LINE__);
		PF_START(NorthAudioUpdateTimer);

		if(sm_IsShuttingDown)
		{
			return;
		}
		if(sm_RunUpdateInSeperateThread)
		{
			u32 timestamp = 0;

			if(isMixerRunning)
			{
				PF_START(NorthAudioUpdateTimer_CommandBuffer);
				TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"WaitOnThreadCommandBufferProcessing");
#if GTA_REPLAY
				if(audDriver::GetMixer() && audDriver::GetMixer()->IsCapturing() && audDriver::GetMixer()->IsFrameRendering())
				{
					audDriver::GetMixer()->TriggerUpdate();
					audDriver::GetMixer()->WaitOnMixBuffers();
				}
#endif
				audDriver::GetMixer()->WaitOnThreadCommandBufferProcessing();
				timestamp = audDriver::GetMixer()->GetMixerTimeFrames();
				TELEMETRY_END_ZONE(__FILE__,__LINE__);
				PF_STOP(NorthAudioUpdateTimer_CommandBuffer);
			}

#if COMMERCE_CONTAINER
			if (CLiveManager::GetCommerceMgr().ContainerIsStoreMode())
			{
				MinimalUpdate();
			}
			else
#endif
			{
				TELEMETRY_START_ZONE(PZONE_NORMAL, __FILE__,__LINE__,"audNorthAudioEngine::Update()");
				PF_START(NorthAudioUpdateTimer_Update);
				Update();
				PF_STOP(NorthAudioUpdateTimer_Update);
				TELEMETRY_END_ZONE(__FILE__,__LINE__);
			}
			
			if(isMixerRunning)
			{
				audDriver::GetMixer()->FlagThreadCommandBufferReadyToProcess(timestamp);
			}
		}
		sysIpcSignalSema(sm_UpdateFinishedSema);
		PF_STOP(NorthAudioUpdateTimer);
	}
}

void audNorthAudioEngine::MinimalUpdate()
{
	const bool isMixerRunning = audDriver::GetMixer() != NULL;
	if(isMixerRunning)
	{
		audDriver::GetMixer()->WaitOnThreadCommandBufferProcessing();
	}
	naAudioEntity::ProcessDeferredSounds();
	audEntity::ProcessBatchedSoundRequests(static_cast<s32>(fwTimer::GetTimeStepInMilliseconds()));
	audEntity::ResetDeletedObjectLists();

	const bool shouldUpdateEnvironmentGroups = sm_AudioController.sm_ShouldUpdateEnvironmentGroups;
	sm_AudioController.sm_ShouldUpdateEnvironmentGroups = false;
	sm_AudioController.Update(sm_TimeInMs);
	sm_AudioController.sm_ShouldUpdateEnvironmentGroups = shouldUpdateEnvironmentGroups;

	//Update the dynamic mixer
	sm_DynamicMixer.Update();
	AUDCATEGORYCONTROLLERMANAGER.Update();
	g_AudioEngine.CommitGameSettings(sm_TimeInMs);

	if(isMixerRunning)
	{
		audDriver::GetMixer()->FlagThreadCommandBufferReadyToProcess();
	}
}

void audNorthAudioEngine::ActivateSlowMoMode(u32 settingsHash)
{
	SlowMoSettings * settings = GetObject<SlowMoSettings>(settingsHash);
	if(settings)
	{
		if(naVerifyf(!sm_ActiveSlowMoModes[settings->Priority], "Trying to set SlowMoType %d, but it is already set; deactivate it first", settings->Priority))
		{
			if(settings->Priority > sm_SlowMoMode)
			{
				if(sm_IsInSlowMo)
				{
					if(sm_SlowMoScene)
					{
						sm_SlowMoScene->Stop();
					}
					sm_DynamicMixer.StartScene(settings->Scene, &sm_SlowMoScene);

					if(sm_SlowMoSound)
					{
						sm_SlowMoSound->StopAndForget();
					}
					g_FrontendAudioEntity.CreateAndPlaySound_Persistent(settings->SlowMoSound, &sm_SlowMoSound);
				}	
				sm_SlowMoMode = (SlowMoType)settings->Priority;
			}
			sm_ActiveSlowMoModes[settings->Priority] = settingsHash;
		}
	}
}

void audNorthAudioEngine::DeactivateSlowMoMode(u32 settingsHash)
{
	SlowMoSettings * settings = GetObject<SlowMoSettings>(settingsHash);
	if(settings)
	{
		if(sm_SlowMoMode == settings->Priority)
		{
			SlowMoType priorityType = AUD_SLOWMO_GENERAL;
			for(int i=1; i<sm_SlowMoMode; i++)
			{
				if(sm_ActiveSlowMoModes[i])
				{
					priorityType = (SlowMoType)i;
				}
			}
			if(sm_SlowMoScene)
			{
				sm_SlowMoScene->Stop();
				SlowMoSettings * newSettings = GetObject<SlowMoSettings>(sm_ActiveSlowMoModes[priorityType]);
				if(newSettings)
				{
					sm_DynamicMixer.StartScene(newSettings->Scene, &sm_SlowMoScene);
				}
			}
			if(sm_SlowMoSound)
			{
				sm_SlowMoSound->StopAndForget();				
				g_FrontendAudioEntity.CreateAndPlaySound_Persistent(settings->SlowMoSound, &sm_SlowMoSound);
			}
			sm_SlowMoMode = priorityType;
		}
		sm_ActiveSlowMoModes[settings->Priority] = 0;
	}
}

void audNorthAudioEngine::DeactivateSlowMoMode(SlowMoType priority)
{
	if(priority == AUD_SLOWMO_GENERAL)
	{
		return; //can't deactivate slowmo general
	}
	if(sm_SlowMoMode == priority)
	{
		SlowMoType priorityType = AUD_SLOWMO_GENERAL;
		for(int i=1; i<sm_SlowMoMode; i++)
		{
			if(sm_ActiveSlowMoModes[i])
			{
				priorityType = (SlowMoType)i;
			}
		}

		SlowMoSettings * newSettings = GetObject<SlowMoSettings>(sm_ActiveSlowMoModes[priorityType]);
		if(sm_SlowMoScene)
		{
			sm_SlowMoScene->Stop();			
			if(newSettings)
			{
				sm_DynamicMixer.StartScene(newSettings->Scene, &sm_SlowMoScene);
			}
		}

		if(sm_SlowMoSound)
		{
			sm_SlowMoSound->StopAndForget();					
			g_FrontendAudioEntity.CreateAndPlaySound_Persistent(newSettings->SlowMoSound, &sm_SlowMoSound);
		}
		sm_SlowMoMode = priorityType;
	}
	sm_ActiveSlowMoModes[priority] = 0;
}

bool audNorthAudioEngine::IsAFirstPersonCameraActive(const CPed* ped, const bool bCheckStrafe, const bool bIncludeClone, const bool bDisableForDominantScriptedCams, const bool bDisableForDominantCutsceneCams, const bool bCheckFlags)
{
	if(!ped)
	{
		return false;
	}

	bool isFirstPerson = FPS_MODE_SUPPORTED_ONLY(ped ? ped->IsFirstPersonShooterModeEnabledForPlayer(bCheckStrafe, bIncludeClone, bDisableForDominantScriptedCams, bDisableForDominantCutsceneCams, bCheckFlags) :) false;

#if GTA_REPLAY
	if(CReplayMgr::IsPlaying())
	{
		return camInterface::IsDominantRenderedCameraAnyFirstPersonCamera();
	}
#endif

	return isFirstPerson;
}

bool audNorthAudioEngine::IsSuperSlowVideoEditor()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		if(CReplayMgr::IsUserPaused() && CReplayMgr::GetCursorSpeed() > 0.f && CReplayMgr::GetCursorSpeed() <= 0.06f)
		{
			return true;
		}

		if(!CReplayMgr::IsUserPaused() && CReplayMgr::GetMarkerSpeed() <= 0.06f && CReplayMgr::GetCursorSpeed() == 1.f)
		{
			return true;
		}
	}
#if __BANK
	if(sm_ForceSuperSlowMoVideoEditor)
	{
		return true;
	}
#endif
#endif
	return false;
}

#if GTA_REPLAY
void audNorthAudioEngine::ShutdownReplayEditor()
{
	g_ScriptAudioEntity.CleanUpScriptBankSlotsAfterReplay();
}
#endif

bool audNorthAudioEngine::IsInCinematicSlowMo()
{
	if(sm_IsInSlowMo && sm_CinematicThirdPersonAimCameraActive)
	{
		return true;
	}
	return false;
}

bool audNorthAudioEngine::IsInVideoEditor()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return true;
	}
#endif
	return false;
}

bool audNorthAudioEngine::IsInSlowMoVideoEditor()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		if(CReplayMgr::IsUserPaused() && CReplayMgr::GetCursorSpeed() > 0.f && CReplayMgr::GetCursorSpeed() < 1.f)
		{
			return true;
		}

		if(!CReplayMgr::IsUserPaused() && CReplayMgr::GetMarkerSpeed() < 1.f && CReplayMgr::GetCursorSpeed() == 1.f)
		{
			return true;
		}
	}
#endif

#if __BANK
	if(sm_ForceSlowMoVideoEditor)
	{
		return true;
	}
#endif
	
	if(sm_IsInSlowMo && sm_CinematicThirdPersonAimCameraActive)
	{
		return true;
	}

	return false;
}

SlowMoType audNorthAudioEngine::GetActiveSlowMoMode()
{
	return sm_SlowMoMode;
}

void audNorthAudioEngine::Update()
{
	USE_MEMBUCKET(MEMBUCKET_AUDIO);

#if RSG_PC && __BANK
	audMixerDeviceXAudio2::SetNumberOfAudioMixBuffers(g_NumberOfMixBuffers); 
#endif

#if __DEV
	PF_SET(MaxVoicesPerBucket, g_MaxVoicesUsed);
	PF_SET(MaxSoundsPerBucket, g_MaxSoundSlotsUsed);
	PF_SET(MaxRequestedSettingsPerBucket, g_MaxRequestedSettingsSlotsUsed);
#endif
#if __WIN32
	sysPerformanceTimer timer("audNorthAudioEngine");
	timer.Start();
#endif // __WIN32

	PIXBegin(0, "NorthAudioUpdate");

	// Do anything specific if we've just been paused.
	sm_PausedLastFrame = sm_Paused;

	// If we're paused, don't update the time we pass into camera and controller updates
	if (!sm_Paused)
	{
		sm_LastTimeInMs = sm_TimeInMs;
		sm_TimeInMs = fwTimer::GetTimeInMilliseconds();
	}

	sm_IsInSlowMo = fwTimer::GetTimeWarpActive() < 1.f REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && IsInSlowMoVideoEditor()) || sm_ForceSlowMoVideoEditor || sm_ForceSuperSlowMoVideoEditor);
	bool isInSpecialAbilitySlowmo = (fwTimer::GetTimeWarpActive() < 1.f && !sm_IsPlayerSpecialAbilityFadingOut) REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && IsInSlowMoVideoEditor()));

	if(!sm_IsInSlowMo && sm_WaitingForPauseMenuSlowMoToEnd)
	{
		sm_WaitingForPauseMenuSlowMoToEnd = false;
	}

	// Set up the microphones
	sm_Microphones.SetUpMicrophones();

#if RSG_PS3 || RSG_ORBIS
	ORBIS_ONLY(sm_IsWirelessHeadsetConnected = CPauseMenu::GetMenuPreference(PREF_SPEAKER_OUTPUT) == 2 && CPauseMenu::GetMenuPreference(PREF_PULSE_HEADSET) == TRUE);

	if(sm_IsWirelessHeadsetConnected)
	{
		if(!sm_ShouldTriggerPulseHeadset)
		{
			sm_ShouldTriggerPulseHeadset = true;
			
			g_SFXHPFCutoff = g_PulseHPFCutoff;
			g_SFXHPFBypass = false;

			if(!sm_PulseHeadsetScene)
			{
				DYNAMICMIXER.StartScene("PULSE_HEADSET_SCENE", &sm_PulseHeadsetScene);
			}
		}
	}
	else if(sm_PulseHeadsetScene)
	{
		sm_ShouldTriggerPulseHeadset = false;
		g_SFXHPFBypass = true;
		sm_PulseHeadsetScene->Stop();
		sm_PulseHeadsetScene = NULL;
	}
#endif

	if(g_AudioEngine.GetEnvironment().GetSpecialEffectMode() == kSpecialEffectModeStoned)
	{
		if(!sm_StonedScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("SPECIAL_EFFECT_MODE_STONED_SCENE", 0x8D342873), &sm_StonedScene);
		}
	}
	else
	{
		if(sm_StonedScene)
		{
			sm_StonedScene->Stop();
			sm_StonedScene = NULL;
		}
	}

	CPed * player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		if(player->GetSpecialAbility())
		{
			bool specialAbilityActive = player->GetSpecialAbility()->IsActive();
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				specialAbilityActive = false;
			}
#endif

			if(specialAbilityActive)
			{
				sm_LastTimeSpecialAbilityActive = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			}

			//Displayf("specialAbilityActive %d", specialAbilityActive);

			if(specialAbilityActive && !sm_ActiveSlowMoModes[AUD_SLOWMO_SPECIAL])
			{
				if(CNetwork::IsGameInProgress()) // currently there is no differentiation between local player slowmo and remote player slowmo
				{
					ActivateSlowMoMode(ATSTRINGHASH("GTAO_BT_ACTIVATING_PLAYER", 0x3F4DC076));
					//ActivateSlowMoMode(ATSTRINGHASH("GTAO_SLOWMO_SPECIAL_BYSTANDER", 0xEBCBB786));

				}
				else
				{
					switch(player->GetPedAudioEntity()->GetPedType())
					{
					case PEDTYPE_PLAYER_0: //MICHAEL
						ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_SPECIAL_MICHAEL", 0xAAADDCBC));
						break;
					case PEDTYPE_PLAYER_1: //FRANKLIN
						ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_SPECIAL_FRANKLIN", 0x579EE10D));
						break;
					case PEDTYPE_PLAYER_2: //TREVOR
						ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_SPECIAL_TREVOR", 0xF798196B));
						break;
					default:
						break;
					}
				}
			}
			else if(!specialAbilityActive)
			{
				g_FrontendAudioEntity.StopSpecialAbility();
				if(sm_ActiveSlowMoModes[AUD_SLOWMO_SPECIAL])
				{
					DeactivateSlowMoMode(AUD_SLOWMO_SPECIAL);
				}
			}
		}

		bool weaponWheelActive = false;

		if(CNetwork::IsGameInProgress())
		{
			const CPed *playerPed = CGameWorld::FindLocalPlayer();
			const bool isUsingVehicleWeaponControls = playerPed && (playerPed->GetIsInVehicle() || playerPed->GetIsParachuting() || playerPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack));
			weaponWheelActive = CNewHud::IsWeaponWheelVisible() && !isUsingVehicleWeaponControls;
		}
		else
		{
			 weaponWheelActive = CNewHud::IsWeaponWheelActive() && CNewHud::ShouldWheelBlockCamera();
		}

		if(weaponWheelActive)
		{
			if(!sm_ActiveSlowMoModes[AUD_SLOWMO_WEAPON])
			{
				ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_WEAPON", 0xF1D29082));
			}
		}
		else if(!weaponWheelActive && sm_ActiveSlowMoModes[AUD_SLOWMO_WEAPON])
		{
			DeactivateSlowMoMode(AUD_SLOWMO_WEAPON);
		}
		if(CStuntJumpManager::IsAStuntjumpInProgress())
		{
			if(!sm_ActiveSlowMoModes[AUD_SLOWMO_STUNT])
			{
				ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_STUNT", 0xC7CA5E0C));
			}
		}
		if(!CStuntJumpManager::IsAStuntjumpInProgress() && sm_ActiveSlowMoModes[AUD_SLOWMO_STUNT])
		{
			DeactivateSlowMoMode(AUD_SLOWMO_STUNT);
		}		

		if(sm_CinematicThirdPersonAimCameraActive && sm_IsInSlowMo)
		{
			if(!sm_ActiveSlowMoModes[AUD_SLOWMO_THIRD_PERSON_CINEMATIC_AIM])
			{
				ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_THIRD_PERSON_CINEMATIC_AIM", 0xAEDE85AD));
			}			
		}
		else
		{
			DeactivateSlowMoMode(AUD_SLOWMO_THIRD_PERSON_CINEMATIC_AIM);
		}
		
		if(camInterface::GetCinematicDirector().IsSlowMoActive())
		{
			if(!sm_ActiveSlowMoModes[AUD_SLOWMO_CINEMATIC])
			{
				ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_CINEMATIC", 0x929AEE2A));
			}			
		}
		else if(!camInterface::GetCinematicDirector().IsSlowMoActive() && sm_ActiveSlowMoModes[AUD_SLOWMO_CINEMATIC])
		{
			DeactivateSlowMoMode(AUD_SLOWMO_CINEMATIC);
		}

		if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ActivateSwitchWheelAudio) && !sm_ActiveSlowMoModes[AUD_SLOWMO_SWITCH])
		{
			ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_SWITCH", 0xC33BCB9F));
		}
		else if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ActivateSwitchWheelAudio) && sm_ActiveSlowMoModes[AUD_SLOWMO_SWITCH])
		{
			DeactivateSlowMoMode(AUD_SLOWMO_SWITCH);
		}

		if(sm_SlowMoMode == AUD_SLOWMO_SPECIAL && !sm_ActiveSlowMoModes[AUD_SLOWMO_STUNT])
		{
			sm_IsInSlowMo = isInSpecialAbilitySlowmo;
		}

		if(sm_IsInSlowMo && sm_SlowMoMode == AUD_SLOWMO_SPECIAL REPLAY_ONLY( && !CReplayMgr::IsEditorActive()))
		{
			if(NetworkInterface::IsGameInProgress()) // currently there is no differentiation between local player slowmo and remote player slowmo
			{
				g_FrontendAudioEntity.StartSpecialAbility(ATSTRINGHASH("Multiplayer", 0xDDA1F78));				
			}
			else
			{
				switch(player->GetPedAudioEntity()->GetPedType())
				{
				case PEDTYPE_PLAYER_0: //MICHAEL
					g_FrontendAudioEntity.StartSpecialAbility(ATSTRINGHASH("Michael", 0x55932F38));				
					break;
				case PEDTYPE_PLAYER_1: //FRANKLIN
					g_FrontendAudioEntity.StartSpecialAbility(ATSTRINGHASH("Franklin", 0x44C24694));				
					break;
				case PEDTYPE_PLAYER_2: //TREVOR
					g_FrontendAudioEntity.StartSpecialAbility(ATSTRINGHASH("Trevor", 0x2737D5AC));				
					break;
				default:
					break;
				}
			}
		}
		else
		{
			g_FrontendAudioEntity.StopSpecialAbility();
		}
		if(sm_IsInSlowMo && sm_SlowMoMode == AUD_SLOWMO_SWITCH)
		{
			g_FrontendAudioEntity.StartSwitch();
		}
		else
		{
			g_FrontendAudioEntity.StopSwitch();
		}
		if(weaponWheelActive)
		{
			g_FrontendAudioEntity.StartWeaponWheel();
		}
		else
		{
			g_FrontendAudioEntity.StopWeaponWheel();
		}
		if(sm_IsInSlowMo && sm_SlowMoMode == AUD_SLOWMO_STUNT)
		{
			g_FrontendAudioEntity.StartStunt();
		}
		else
		{
			g_FrontendAudioEntity.StopStunt();
		}
		if(sm_IsInSlowMo && sm_SlowMoMode == AUD_SLOWMO_CINEMATIC)
		{
			g_FrontendAudioEntity.StartCinematic();
		}
		else
		{
			g_FrontendAudioEntity.StopCinematic();
		}
		if(sm_IsInSlowMo && sm_SlowMoMode == AUD_SLOWMO_GENERAL)
		{
			g_FrontendAudioEntity.StartGeneralSlowMo();
		}
		else
		{
			g_FrontendAudioEntity.StopGeneralSlowMo();
		}
	}

	// note: using the audio timer, since fwTimer will be warped by the time warp
	const f32 timeWarp = sm_TimeWarpSmoother.CalculateValue(fwTimer::GetTimeWarpActive(), g_AudioEngine.GetTimeInMilliseconds());
	f32 timeScale = sm_TimeWarpToTimeScale.CalculateValue(timeWarp);

	f32 slowmoTimesWarp = timeWarp;
	f32 slowmoTimesScale = timeScale;
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() BANK_ONLY(|| sm_ForceSlowMoVideoEditor || sm_ForceSuperSlowMoVideoEditor) )
	{

#if __BANK
		if(sm_ForceSlowMoVideoEditor)
		{
			timeScale = 0.2f;
		}
		if(sm_ForceSuperSlowMoVideoEditor)
		{
			timeScale = 0.05f;
		}
		if(!sm_ForceSlowMoVideoEditor && !sm_ForceSuperSlowMoVideoEditor) // if we're not forcing just use the timescale from the marker otherwise use whatever was set
		{
			timeScale = CReplayMgr::GetMarkerSpeed();
		}
#else
		timeScale = CReplayMgr::GetMarkerSpeed();
#endif

		f32 replayTimeScale = timeScale;
		if(timeScale < 1.0f)
		{
			if(sm_ReplayTimeWarpToTimeScale.IsValid())
			{
				replayTimeScale = sm_ReplayTimeWarpToTimeScale.CalculateValue(timeScale);
			}
			else
			{
				static const audThreePointPiecewiseLinearCurve replayTimeScaleCurve(0.05f, 0.25f, 0.475f, 0.55f, 1.0f, 1.0f);
				replayTimeScale = replayTimeScaleCurve.CalculateValue(timeScale);
			}
		}
		g_GameWorldCategory->SetFrequencyRatio(replayTimeScale);
		slowmoTimesScale = replayTimeScale;
		slowmoTimesWarp = replayTimeScale;
	}
	else
	{
		g_GameWorldCategory->SetFrequencyRatio(1.0f);
	}
#endif

	// lets only slow down pausable stuff
	g_AudioEngine.GetSoundManager().SetTimeScale(0, sm_SlowMoMode == AUD_SLOWMO_PAUSEMENU ? 1.f : slowmoTimesScale);

	if(sm_IsInSlowMo)
	{
		SlowMoSettings * slowMo = GetObject<SlowMoSettings>(sm_ActiveSlowMoModes[sm_SlowMoMode]);

		if(slowMo)
		{
			if(!sm_SlowMoScene)
			{						
				sm_DynamicMixer.StartScene(slowMo->Scene, &sm_SlowMoScene);
			}

			if(!sm_SlowMoSound)
			{				
				g_FrontendAudioEntity.CreateAndPlaySound_Persistent(slowMo->SlowMoSound, &sm_SlowMoSound);
			}
		}

		if(sm_SlowMoScene)
		{
			sm_SlowMoScene->SetVariableValue(ATSTRINGHASH("pitchApply", 0x20E8D2B3), slowmoTimesWarp);
		}

		if(sm_SlowMoSound)
		{
			sm_SlowMoSound->FindAndSetVariableValue(ATSTRINGHASH("timeWarp", 0xBC6E3F8F), timeWarp);
		}
	}
	else 
	{
		if(sm_SlowMoScene)
		{
			sm_SlowMoScene->Stop();
		}

		if(sm_SlowMoSound)
		{
			sm_SlowMoSound->StopAndForget();
		}
	}

	if(IsInSlowMoVideoEditor() || IsSuperSlowVideoEditor())
	{
		if(IsSuperSlowVideoEditor())
		{
			if(!sm_SuperSlowMoVideoEditorScene)
			{
				sm_DynamicMixer.StartScene(ATSTRINGHASH("SLO_MO_VIDEO_EDITOR_05", 0x56A19675), &sm_SuperSlowMoVideoEditorScene);		
			}
			if(sm_SlowMoVideoEditorScene)
			{
				sm_SlowMoVideoEditorScene->Stop();
			}
		}
		else 
		{
			if(!sm_SlowMoVideoEditorScene)
			{
				sm_DynamicMixer.StartScene(ATSTRINGHASH("SLO_MO_VIDEO_EDITOR", 0xA828B595), &sm_SlowMoVideoEditorScene);		
			}
			if(sm_SuperSlowMoVideoEditorScene)
			{
				sm_SuperSlowMoVideoEditorScene->Stop();
			}
		}		
	}
	else 
	{
		if(sm_SlowMoVideoEditorScene)
		{
			sm_SlowMoVideoEditorScene->Stop();
		}
		if(sm_SuperSlowMoVideoEditorScene)
		{
			sm_SuperSlowMoVideoEditorScene->Stop();
		}
	}

	if(sm_SlowMoMode == AUD_SLOWMO_RADIOWHEEL && sm_SlowMoScene)
	{
		float radioSlowMoFade = 0.0f;
		float radioOffFade = 1.f;
		if(g_RadioAudioEntity.GetPlayerRadioStation())
		{
			radioSlowMoFade = 1.f;
			radioOffFade = 0.f;
		}
		sm_SlowMoScene->SetVariableValue(ATSTRINGHASH("onApply", 0x6BDBAC89), radioSlowMoFade);
		sm_SlowMoScene->SetVariableValue(ATSTRINGHASH("offApply", 0xA7790120), radioOffFade);	
	}

	sysPerformanceTimer controllerTimer("audController");
	controllerTimer.Start();
	sm_AudioController.PreUpdate(sm_TimeInMs);
	controllerTimer.Stop();

	audEntity::ProcessBatchedSoundRequests(static_cast<s32>(fwTimer::GetTimeStepInMilliseconds()));

	if(!sm_Paused)
	{
		audVehicleAudioEntity::UpdateActivationRanges();
		audTrainAudioEntity::UpdateActivationRanges();
	}

	audSoundManager &soundManager = g_AudioEngine.GetSoundManager();

	// supply sounds with the game time
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.DayOfWeek", 0x357A17E4), static_cast<f32>(CClock::GetDayOfWeek()));
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.Hours", 0x98263E3E), static_cast<f32>(CClock::GetHour()));
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.Minutes", 0xF1150725), static_cast<f32>(CClock::GetMinute()));
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.Seconds", 0x70D769DA), static_cast<f32>(CClock::GetSecond()));
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.Days", 0x147D8EB9), static_cast<f32>(CClock::GetDay()));
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.Months", 0x67F650AC), static_cast<f32>(CClock::GetMonth()));
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.SecsPerMin", 0x266005A7), static_cast<f32>(CClock::GetMsPerGameMinute()/1000.f));

	// whole hours, decimal mins+secs
	sm_GameTimeHours = static_cast<float>(CClock::GetHour()) + (static_cast<float>(CClock::GetMinute()) / 60.f) + (static_cast<float>(CClock::GetSecond()) / 3600.f);
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.DecimalHours", 0x59664070), sm_GameTimeHours);
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Clock.TimeWarp", 0xDFA2FAF7), sm_TimeWarpSmoother.GetLastValue());

	CPed *playerPed = FindPlayerPed();
	if(playerPed)
	{
		const Vec3V playerPos = playerPed->GetTransform().GetPosition();
		soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.Position.x", 0xA39016E6), playerPos.GetXf());
		soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.Position.y", 0x91297215), playerPos.GetYf());
		soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.Position.z", 0x7EE44D8B), playerPos.GetZf());
		soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.Speed", 0x8CE50515), VEC3V_TO_VECTOR3(sm_PedPosLastFrame).Dist(VEC3V_TO_VECTOR3(playerPos))/fwTimer::GetTimeStep());
		soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.Health", 0x5CF478F), playerPed->GetHealth());
		soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.MaxHealth", 0x23B3E65D), playerPed->GetMaxHealth());
		sm_PedPosLastFrame = playerPos;
	}

	const Vec3V panningPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Microphone.Position.x", 0xE67CF42F), panningPos.GetXf());
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Microphone.Position.y", 0xA3C06EB7), panningPos.GetYf());
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Microphone.Position.z", 0x85E73305), panningPos.GetZf());

	soundManager.SetVariableValue(ATSTRINGHASH("Game.Water.CameraDepth", 0xD38CC888), Water::GetCameraWaterDepth());

	float firstPerson = sm_Microphones.IsFirstPerson() ? 1.f : 0.f;
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Microphone.FirstPerson", 0x378A1D92),firstPerson);

	float slowMoVideoEditor = IsInSlowMoVideoEditor() ? 1.f : 0.f;
	soundManager.SetVariableValue(ATSTRINGHASH("Game.VideoEditor.Slowmo", 0x9FA46F1D), slowMoVideoEditor);

	float isVideoEditor = IsInVideoEditor() ? 1.f : 0.f;
	soundManager.SetVariableValue(ATSTRINGHASH("Game.VideoEditor.Replay", 0x5EC0D39A), isVideoEditor);

	f32 interiorRatio = 0.0f;
	sm_Environment.AreWeInAnInterior(NULL, &interiorRatio);
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Player.InteriorRatio", 0x8BE286CD), interiorRatio);

	float isCopsAndCrooks = NetworkInterface::IsInCopsAndCrooks() ? 1.f : 0.f;
	soundManager.SetVariableValue(ATSTRINGHASH("Game.Mode.IsCopsAndCrooks", 0xB315260A), isCopsAndCrooks);

	audDoorAudioEntity::UpdateClass();
	audVehicleAudioEntity::UpdateClass();
	audCarAudioEntity::UpdateClass();
	audSpeechAudioEntity::UpdateClass();

	g_FireSoundManager.Update();
	g_SpeechManager.Update();
	g_ReflectionsAudioEntity.Update();

	if(!fwTimer::IsGamePaused())
	{
		g_EmitterAudioEntity.Update();
		NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.Update());
		g_AmbientAudioEntity.Update(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	}

	g_PedScenarioManager.Update();
	audStreamSlot::UpdateSlots(sm_TimeInMs);

#if __BANK
	UpdateAuditionSound();
	UpdateRAVEVariables();
#endif

	// calculate updated environment metrics
	sm_Environment.Update();

	// The environment update relies on the state of the microphone ( frozen/unfrozen ) so just in case it was frozen, lets unfreeze it after the environment update.
	sm_Microphones.UnFreezeMicrophone();

	// Needs to be called after sm_Environment.Update() which gives us all the interior info we need
	sm_OcclusionManager.UpdateOcclusionMetrics();	

	//Update the dynamic mixer
	sm_DynamicMixer.Update();

	controllerTimer.Start();
	sm_AudioController.Update(sm_TimeInMs);
	controllerTimer.Stop();

	//Update length is the time taken to perform update.
	PF_SET(ControllerUpdateLength, (f32)controllerTimer.GetTimeMS());

	UpdateCategories();

	g_AudioEngine.CommitGameSettings(sm_TimeInMs);
	
	// This call can take ~800uS so we don't want to call it every audio frame
	// We don't have to call it every game frame either if we can think of something else to run in its place
	audDriver::GetVoiceManager().UpdateUserMusicState();

#if __WIN32
	timer.Stop();
	//Update length is the time taken to perform update.
	PF_SET(UpdateLength, (f32)timer.GetTimeMS());
#endif // __WIN32

	PIXEnd();
}

void audNorthAudioEngine::MuteGameWorldAndPositionedRadioForTv(bool mute)
{
	if (mute)
	{
		if(!sm_TvScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("TV_SCENE", 2327741164), &sm_TvScene);
		}
	}
	else
	{
		if(sm_TvScene)
		{
			sm_TvScene->Stop();
		}
	}
}

void audNorthAudioEngine::UpdateVehicleCategories()
{
	CVehicle* playerVehicle = NULL; 
	CPed* playerPed = NULL;

	if(!playerVehicle)
	{
		playerVehicle = audPedAudioEntity::GetBJVehicle();
	}

	if(NetworkInterface::IsInSpectatorMode())
	{
		playerVehicle = CGameWorld::FindFollowPlayerVehicle();
		playerPed = CGameWorld::FindFollowPlayer();
	}
	else
	{
		playerVehicle = CGameWorld::FindLocalPlayerVehicle();
		playerPed = CGameWorld::FindLocalPlayer();
	}

	bool isPlayerHangingOffVehicle = false;

	if(playerVehicle && playerVehicle->GetSeatManager())
	{
		s32 pedSeatIndex = playerVehicle->GetSeatManager()->GetPedsSeatIndex(playerPed);

		if(playerVehicle->IsSeatIndexValid(pedSeatIndex))
		{			
			const CVehicleSeatAnimInfo* seatAnimInfo = playerVehicle->GetSeatAnimationInfo(pedSeatIndex);

			if(seatAnimInfo && seatAnimInfo->GetKeepCollisionOnWhenInVehicle())
			{
				isPlayerHangingOffVehicle = true;
			}			
		}
	}

	if (playerVehicle && playerVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() != AUD_VEHICLE_BICYCLE && !isPlayerHangingOffVehicle)
	{
		const float smoothedOpenness = sm_PIVOpennessSmoother.CalculateValue(1.f - playerVehicle->GetVehicleAudioEntity()->GetOpenness(false), fwTimer::GetTimeStep());

		if(!sm_PlayerInVehScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("PLAYER_IN_VEHICLE_SCENE", 0x201E71F0),&sm_PlayerInVehScene);
		}
		if (naVerifyf(sm_PlayerInVehScene, "Failed to create player in vehicle scene, this shouldn't happen"))
		{
			sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("openness", 0x2721BC39), smoothedOpenness);
			sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("musicVolume", 0x499CB684), sm_PIVMusicVolumeSmoother.GetLastValue());
			sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("radioVolume", 0x3C12FAC8), sm_PIVRadioVolumeSmoother.GetLastValue());
			if (sm_Microphones.IsVehBonnetMic())
			{
				// engine boost for rear engine vehicles
				Vector3 enginePos;
				enginePos = VEC3V_TO_VECTOR3(playerVehicle->TransformIntoWorldSpace(playerVehicle->GetVehicleAudioEntity()->GetEngineOffsetPos()));
				// Check if the engine is behind the vehicle
				Vector3 directionToEngine = enginePos - VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
				f32 frontDot = directionToEngine.Dot(VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetB()));
				if (frontDot < 0)
				{
					sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("rearEngineApply", 0x6EB37A69), 1.f);
				}
				else
				{
					sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("rearEngineApply", 0x6EB37A69), 0.f);
				}
			}
			else
			{
				sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("rearEngineApply", 0x6EB37A69), 0.f);
			}
		}
	}
	else
	{
		if(sm_PlayerInVehScene)
		{
			sm_PlayerInVehScene->Stop();
		}

		sm_PIVOpennessSmoother.CalculateValue(0.f, fwTimer::GetTimeStep());
	}

	// offset radio volume by the current player vehicle engine volume.
	sm_EngineVolume = 0.0f;
	sm_PlayerVehicleOpenness = 1.0f;

	if (playerVehicle)
	{
		sm_EngineVolume = playerVehicle->GetVehicleAudioEntity()->GetFrontendRadioVolumeOffset();
		sm_PlayerVehicleOpenness = playerVehicle->GetVehicleAudioEntity()->GetOpenness(false);
	}
}

void audNorthAudioEngine::UpdateCategories()
{
	naAssertf(sm_NorthAudioMixScene, "No north audio mix scene present, need to create a valid scene in Init()");

	if(!sm_NorthAudioMixScene)
	{
		return;
	}

#if RSG_DURANGO && 0
	if(audDriver::GetMixer())
	{
		if(audDriver::GetMixer()->IsSuspended())
		{
			if(!sm_GameSuspendedScene)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("GAME_SUSPENDED_SCENE", 0x37EC776), &sm_GameSuspendedScene);
			}
		}
		else
		{
			if(sm_GameSuspendedScene)
			{
				sm_GameSuspendedScene->Stop();
			}
		}
	}
#endif

#if !__BANK && __WIN32PC
	bool muteForLostFocus = false;
	bool isCapturing = audDriver::GetMixer() && audDriver::GetMixer()->IsCapturing();
	if(!isCapturing && GRCDEVICE.GetLostFocusForAudio() && CPauseMenu::GetMenuPreference(PREF_AUDIO_MUTE_ON_FOCUS_LOSS))
	{
		muteForLostFocus = true;
	}
#else
	const bool muteForLostFocus = false;
#endif
	if (sm_ShouldMuteAudio || muteForLostFocus)
	{
		if(!sm_MuteAllScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("MUTE_ALL_SCENE", 1043566406U), &sm_MuteAllScene);
		}
		else
		{
			sm_MuteAllScene->SetVariableValue(ATSTRINGHASH("basevol", 0xD10CB74A), 1.0f);
		}
	}
	else
	{
		if(sm_MuteAllScene)
		{
			sm_MuteAllScene->Stop();
		}
	}

	// Update Categories
	if (g_BaseCategory)
	{
		// We scale the overall rolloff according to the environment. Makes sense to set it here, so all category changes are done in one place.
		// Similarly, we scale down occlusion in certain camera modes (cine-cam), so do that here too.
		naAssertf(audNorthAudioEngine::GetGtaEnvironment(), "In UpdateCategories, could not get the environment; will access a null ptr");
		g_BaseCategory->SetDistanceRolloffScale(1.0f);//audNorthAudioEngine::GetGtaEnvironment()->GetRollOffFactor());
		audCategory* baseCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("BASE", 0x44E21C90));
		if (baseCategory)
		{
			baseCategory->SetOcclusionDamping(audNorthAudioEngine::GetOcclusionManager()->GetGlobalOcclusionDampingFactor());
		}
	}

	// screen fade stuff only mutes the game world - not frontend or radio
	// fades don't seem to be totally fading out, so we make it silent at a small level
	f32 fadeLevel = camInterface::GetFadeLevel();

	fadeLevel = ClampRange(fadeLevel, 0.0f, g_FadeValueForSilence);
	fadeLevel /= g_FadeValueForSilence;
	
#if GTA_REPLAY
	if(CReplayMgr::IsEditorActive())
	{		
		if(!CReplayCoordinator::IsExportingToVideoFile())
		{
			if(!CReplayMgr::IsEditModeActive() || CVideoEditorPlayback::IsLoading())
			{			
				fadeLevel = 1.f;
			}
		}
	}
	else 
#endif
	if(sm_StartingNewGame || fwTimer::IsUserPaused() || fwTimer::IsGamePaused() BANK_ONLY(|| g_CutsceneAudioPaused))
	{
		BANK_ONLY(if(!fwTimer::IsDebugPause()))
			fadeLevel = 1.f;
	}

	float musicApply = 0.f;

#if GTA_REPLAY
	if(CReplayMgr::IsEditorActive())
	{
		if(!CReplayCoordinator::IsExportingToVideoFile() && !g_InteractiveMusicManager.IsReplayPreviewPlaying())
		{
			musicApply = fadeLevel;
		}		
	}
	else 
#endif
	if((sm_StartingNewGame || CPauseMenu::IsActive()) && sm_DataSetState != Loaded_MP REPLAY_ONLY(&& !g_InteractiveMusicManager.IsReplayPreviewPlaying() && !CReplayCoordinator::IsExportingToVideoFile()))
	{
		musicApply = fadeLevel;
	}

	sm_PIVMusicVolumeSmoother.SetRates(0.001f / g_PIVMusicVolumeFadeInS, 0.001f / g_PIVMusicVolumeFadeOutS);
	sm_PIVRadioVolumeSmoother.SetRates(0.001f / g_PIVMusicVolumeFadeInS, 0.001f / g_PIVMusicVolumeFadeOutS);
	sm_PIVOpennessSmoother.SetRates(0.001f / g_PIVOpennessFadeInS, 0.001f / g_PIVOpennessFadeOutS);

	float isFEMusicPlaying = 0.f;
	float isFERadioPlaying = 0.f;
	if(g_InteractiveMusicManager.IsMusicPlaying())
	{
		isFEMusicPlaying = 1.f;
	}
	if(g_RadioAudioEntity.IsVehicleRadioOn())
	{
		isFEMusicPlaying = 1.f;
		isFERadioPlaying = 1.f;
	}

	isFEMusicPlaying *= Min(1.f, sm_MusicVolume / Max(SMALL_FLOAT, sm_SfxVolume));
	sm_PIVMusicVolumeSmoother.CalculateValue(isFEMusicPlaying, fwTimer::GetTimeStep());

	isFERadioPlaying *= Min(1.f, sm_MusicVolume / Max(SMALL_FLOAT, sm_SfxVolume));
	sm_PIVRadioVolumeSmoother.CalculateValue(isFERadioPlaying, fwTimer::GetTimeStep());

	f32 radioVol = 1.f;
	// when in frontend mode and not paused we don't want to use the screen fade, otherwise we do
	// unless we're in the network lobby
	// when playing end credits fade is controlled by script
	if((!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowRadioOverScreenFade) BANK_ONLY(&& !g_ForceAllowRadioOverScreenFade)) && 
			!audRadioStation::IsPlayingEndCredits() && !(g_RadioAudioEntity.IsInFrontendMode() && !g_AudioEngine.GetSoundManager().IsPaused(2)))
	{
		radioVol = (1.f-fadeLevel) * (!NetworkInterface::IsGameInProgress()&&fwTimer::IsGamePaused()?0.0f:1.0f);
	}

	if(!audDynamicMixer::FrontendSceneIsOverridden())
	{
		if (fadeLevel > 0.f)
		{
			if(!sm_FrontendScene)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("FRONTEND_SCENE", 1182742998U), &sm_FrontendScene);
				naAssertf(sm_FrontendScene, "Couldn't create audio frontend scene");
			}
			if(sm_FrontendScene)
			{
				sm_FrontendScene->SetVariableValue(ATSTRINGHASH("fade", 1855728566U), fadeLevel);
				sm_FrontendScene->SetVariableValue(ATSTRINGHASH("musicApply", 0x72758950), musicApply);
				if(sm_DataSetState == Loaded_MP)
				{
					sm_FrontendScene->SetVariableValue(ATSTRINGHASH("posRadioVol", 0x4BE8FA9A), 1.f - radioVol);
				}
			}
		}
		else
		{
			if(sm_FrontendScene)
			{
				sm_FrontendScene->Stop();
			}
		}
	}

	if (!sm_OverrideLobbyMute && sm_IsInNetworkLobby)
	{
		if(!sm_MpLobbyScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("MP_LOBBY_SCENE", 1883148293U), &sm_MpLobbyScene);
		}
	}
	else
	{
		if(sm_MpLobbyScene)
		{
			sm_MpLobbyScene->Stop();
		}
	}

#if GTA_REPLAY
	// Keep this on permanently when exporting - don't want to risk it flicking on/off around the clip transition points
	if(CReplayMgr::IsEditModeActive() || CReplayCoordinator::IsExportingToVideoFile())
	{
		if(!sm_ReplayEditorScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("REPLAY_EDITOR_PLAYBACK_SCENE", 0x6B579349), &sm_ReplayEditorScene);
		}		
	}
	else
	{
		if(sm_ReplayEditorScene)
		{
			sm_ReplayEditorScene->Stop();
		}
	}
#endif

	bool bFirstPerson = IsAFirstPersonCameraActive(CGameWorld::FindLocalPlayer(), false, false, true, true) || audNorthAudioEngine::IsRenderingFirstPersonTurretCam();
	sm_IsFirstPersonActiveForPlayer = bFirstPerson;
	sm_IsCutsceneActive = (gVpMan.AreWidescreenBordersActive() || ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsStreamedCutScene()) && CGameWorld::GetMainPlayerInfo()->AreControlsDisabled()));
	bool isSniping = audNorthAudioEngine::GetMicrophones().IsSniping();
	bool isSwitchingPlayer = g_PlayerSwitch.IsActive();
	if(bFirstPerson && !sm_IsCutsceneActive && !isSniping && !isSwitchingPlayer && !audVehicleAudioEntity::IsVehicleInteriorSceneActive() && !audVehicleAudioEntity::IsVehicleFirstPersonTurretSceneActive() && !audVehicleAudioEntity::IsBonnetCamSceneActive())
	{
		if(!sm_FirstPersonModeScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("FIRST_PERSON_MODE_SCENE", 0xAB0410F1), &sm_FirstPersonModeScene);
		}
	}
	else
	{
		if(sm_FirstPersonModeScene)
		{
			sm_FirstPersonModeScene->Stop();
		}
	}

#if NA_RADIO_ENABLED
	sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("radioVol", 0xe7e3432c), 1.f-radioVol);
#endif

	// Update the radio/music volume - primarily to duck it for a variety of reasons
	f32 musicVolLin = 0.f;
	const f32 frontendLinearScaling = 1.f/10.f;

	// Director mode loads MP data, but we want it to use the SP music slider (unless we end up with a MP director mode at some point in the future!)
	if(CPauseMenu::IsMP() || (sm_DataSetState == Loaded_MP && (!CTheScripts::GetIsInDirectorMode() || NetworkInterface::IsGameInProgress())))
	{
		// duck the radio quicker in network games
		sm_RadioDuckingSmoother.SetRates(g_RadioDuckingSmoothRate*1.67f, g_RadioDuckingSmoothRate*1.67f);
		musicVolLin = ((f32)CPauseMenu::GetMenuPreference(PREF_MUSIC_VOLUME_IN_MP)) * frontendLinearScaling;
	}
	else
	{
		sm_RadioDuckingSmoother.SetRates(g_RadioDuckingSmoothRateUp, g_RadioDuckingSmoothRate);
		musicVolLin = ((f32)CPauseMenu::GetMenuPreference(PREF_MUSIC_VOLUME)) * frontendLinearScaling;
	}

	// Don't apply the music slider during the initial game load
	if(!sm_ApplyMusicSlider)
	{
		musicVolLin = 1.f;
	}

	float sfxSliderVolume = 0.0f;
	
	if(sm_DataSetState == Loaded_MP)
	{
#if RSG_PC
		if(NetworkInterface::GetVoice().IsAnyRemoteGamerTalking())
		{
			f32 sfxAudioSliderVolume = ((f32)CPauseMenu::GetMenuPreference(PREF_SFX_VOLUME)) * frontendLinearScaling;
			f32 voiceChatSoundSliderVolume = ((f32)CPauseMenu::GetMenuPreference(PREF_VOICE_SOUND_VOLUME)) * frontendLinearScaling;
			sfxSliderVolume = sfxAudioSliderVolume * voiceChatSoundSliderVolume;

			//f32 sfxVolDb = audDriverUtil::ComputeDbVolumeFromLinear(sfxAudioSliderVolume);
			//f32 voiceVolDb = audDriverUtil::ComputeDbVolumeFromLinear(voiceChatSoundSliderVolume);
			//f32 totalVolDb = sfxVolDb + voiceVolDb;
			//Displayf("sfx %f(%f) - voice %f(%f) - total %f(%f)(%f)", sfxAudioSliderVolume, sfxVolDb, voiceChatSoundSliderVolume, voiceVolDb, sfxSliderVolume, audDriverUtil::ComputeLinearVolumeFromDb(totalVolDb), totalVolDb);
		}
		else
#endif // RSG_PC
		{
			sfxSliderVolume = ((f32)CPauseMenu::GetMenuPreference(PREF_SFX_VOLUME)) * frontendLinearScaling;
		}
	}
	else
	{
		sfxSliderVolume = ((f32)CPauseMenu::GetMenuPreference(PREF_SFX_VOLUME)) * frontendLinearScaling;
	}

#if __BANK
	if(g_IgnoreFrontendVolumeSliders)
	{
		// Only override if not muted
		if(sfxSliderVolume != 0.f)
			sfxSliderVolume = 1.f;
		if(musicVolLin != 0.f)
			musicVolLin = 1.f;
	}
#endif

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && (CReplayCoordinator::IsExportingToVideoFile() || CReplayMgr::IsPlaying()))
	{
		// In replay, the user uses replay markers to control music/volume so ignore
		// the frontend ones. The exception is when the user has manually paused the replay and
		// is doing stuff with the menus - in this case, use the same menu sfx volume as the main game
		if(CReplayCoordinator::IsExportingToVideoFile())
		{			
			sfxSliderVolume = 1.f;
		}		
		else if(CReplayMgr::IsPlaying() && !CReplayMgr::IsScrubbing() && !CReplayMgr::IsJumping() && !CReplayMgr::IsUserPaused() && !CReplayMgr::GetNextPlayBackState().IsSet(REPLAY_STATE_PAUSE))
		{		
			sfxSliderVolume = 1.f;
		}

		musicVolLin = 1.f;
	}	
	else if(CReplayMgr::IsEditorActive() && VideoPlayback::IsPlaybackActive(false))
	{
		// Unmute audio when watching an already-recorded video
		sfxSliderVolume = 1.f;
	}
	else if(g_RadioAudioEntity.IsReplayMusicPreviewPlaying())
	{
		musicVolLin = 1.f;
	}
#endif

#if RSG_ORBIS
	// Scale pad speaker volume from 0 to 2, map bottom 50% to pad speaker volume, anything above starts sending to SFX
	float controlPadSpeakerVol = ((f32)CPauseMenu::GetMenuPreference(PREF_CTRL_SPEAKER_VOL)) * frontendLinearScaling * 2.f;
	// Don't bother scaling to silence, and tweak the curve to make the most out of the six volume steps we have available
	const float s_PadExp = 1.4f;
	const float padSpeakerRescaledVol = powf((0.2f + Clamp(controlPadSpeakerVol, 0.f, 1.f)) * 0.8333333f, s_PadExp);
	g_AudioEngine.GetEnvironment().SetPadSpeakerVolume(sfxSliderVolume * padSpeakerRescaledVol);
	g_AudioEngine.GetEnvironment().SetPadSpeakerSFXVolume(sfxSliderVolume * Clamp(controlPadSpeakerVol - 1.f, 0.f, 1.f));
	
	audMixerDeviceOrbis *device = ((audMixerDeviceOrbis*)audDriver::GetMixer());
	// Choose which pref to apply based on the Output option: ie we use a separate pref when the user is in Headphone mode
	const s32 controlPadSpeaker = CPauseMenu::AreHeadphonesEnabled() ? PREF_CTRL_SPEAKER_HEADPHONE : PREF_CTRL_SPEAKER;	
	g_AudioEngine.GetEnvironment().SetPadSpeakerEnabled(CPauseMenu::GetMenuPreference(controlPadSpeaker) != 0 && device->IsPadSpeakerEnabled());
#endif // RSG_ORBIS

	u32 sfxSliderCurrentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
	if(g_FrontendAudioEntity.IsLoadingSceneActive())
	{
		if(sm_LoadingScreenSFXSliderDelay == 0)
		{
			sm_LoadingScreenSFXSliderDelay = sfxSliderCurrentTime + 2000;
		}
		if(sm_LoadingScreenSFXSliderDelay != 0 && sfxSliderCurrentTime > sm_LoadingScreenSFXSliderDelay)
		{
			// Mute SFX during loads
			sfxSliderVolume = 0.f;
		}
	}
	else
	{
		sm_LoadingScreenSFXSliderDelay = 0;
	}

	// mute if lost focus PC only
#if __WIN32PC
	if(muteForLostFocus)
	{
		sfxSliderVolume = 0.f;
		musicVolLin = 0.f;
	}
#endif
	SetSfxVolume(sfxSliderVolume);
	SetMusicVolume(musicVolLin);

	// In the pause menu, we boost scripted speech
	const f32 audSettingsScriptBoostScale = ((f32)CPauseMenu::GetMenuPreference(PREF_DIAG_BOOST)) * frontendLinearScaling;
	sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("scriptedSpeechVol", 0x5b3a463), audSettingsScriptBoostScale);

#if NA_RADIO_ENABLED
	// the amount we duck depends on whether we're in stereo/prologic mode or not - we duck less if we're not
	f32 duckVolDb = g_RadioDuckingVolumeStereo;
	if (audDriver::GetDownmixOutputMode() >= AUD_OUTPUT_5_1)
	{
		duckVolDb = g_RadioDuckingVolume;
	}
		
	const audRadioStation* station = g_RadioAudioEntity.GetPlayerRadioStation();
	
	// duck more for talk stations
	if (station && station->IsTalkStation())
	{
		duckVolDb = g_TalkRadioDuckingVolume;
	}
	else
	{
		if (station)
		{
			// or if dj, adverts, idents, etc are playing

			const audRadioTrack &track = station->GetCurrentTrack();
			if (track.IsInitialised())
			{
				u32 trackCategory = track.GetCategory();
				if(trackCategory != RADIO_TRACK_CAT_MUSIC && trackCategory != RADIO_TRACK_CAT_TAKEOVER_MUSIC)
				{
					// duck it some more
					duckVolDb += g_RadioNonMusicAdditionalDucking;
				}
			}
		}
	}
		
	// Increase ducking with the engine volume offset
	duckVolDb -= sm_EngineVolume;
	
	float radioProxyDuckVolDb = 0.f;
	if(sm_RadioProxyCat)
	{
		radioProxyDuckVolDb += sm_RadioProxyCat->GetVolume();
	}

	float scoreDuckVolDb = 0.f;
	if(sm_ScoreProxyCat)
	{
		scoreDuckVolDb += sm_ScoreProxyCat->GetVolume();
	}

	if(GetSfxVolume() == 0.f || g_RadioAudioEntity.IsInFrontendMode())
	{
		// dont duck when Sfx are muted or we are in frontend radio mode
		duckVolDb = 0.f;
		scoreDuckVolDb = radioProxyDuckVolDb = 0.f;
	}
	else
	{
		const f32 musicVoldB = audDriverUtil::ComputeDbVolumeFromLinear(GetMusicVolume());
		const f32 sfxVoldB = audDriverUtil::ComputeDbVolumeFromLinear(GetSfxVolume());
		// preserve relative balance between sfx and music		
		// Clamp to ensure we don't apply positive gain
		duckVolDb = Min(duckVolDb - (musicVoldB - sfxVoldB), 0.0f);

		// Don't duck radio/score if sfx is down (ie proximity effect ducking radio, which sounds weird if engine sounds are quiet)
		scoreDuckVolDb = Clamp(scoreDuckVolDb + (musicVoldB - sfxVoldB), scoreDuckVolDb, 0.0f);
		radioProxyDuckVolDb = Clamp(radioProxyDuckVolDb + (musicVoldB - sfxVoldB), radioProxyDuckVolDb, 0.0f);
	}

	if(sm_ScoreVolCategoryController && sm_OneShotVolCategoryController)
	{
		sm_ScoreVolCategoryController->SetVolumeDB(scoreDuckVolDb);
		sm_OneShotVolCategoryController->SetVolumeDB(scoreDuckVolDb);
	}

	f32 desiredRadioDuckingVolume = 0.f;

	// Is there a scripted conversation ongoing?
	if (!g_ScriptAudioEntity.ShouldNotDuckRadioThisLine() &&
		(g_ScriptAudioEntity.ShouldDuckForScriptedConversation() || sm_ShouldDuckRadio || g_AudioScannerManager.ShouldDuckRadio()))
	{
		desiredRadioDuckingVolume = duckVolDb;
	}
	else if(audPedAudioEntity::GetShouldDuckRadioForPlayerRingtone())
	{
		desiredRadioDuckingVolume = duckVolDb;
	}


#if RSG_PC
	if(sm_DataSetState == Loaded_MP)
	{
		if(NetworkInterface::GetVoice().IsAnyRemoteGamerTalking())
		{
			desiredRadioDuckingVolume = audDriverUtil::ComputeDbVolumeFromLinear( ((f32)CPauseMenu::GetMenuPreference(PREF_VOICE_MUSIC_VOLUME)) * frontendLinearScaling );
			//Displayf("Radio Duck %f", desiredRadioDuckingVolume);
		}
	}
#else
	if(CPauseMenu::GetMenuPreference(PREF_VOICE_OUTPUT) == 0 &&
#if __XENON
		CProfileSettings::GetInstance().GetInt(CProfileSettings::VOICE_VOLUME)>0 &&
		CProfileSettings::GetInstance().GetInt(CProfileSettings::VOICE_THRU_SPEAKERS) &&
#endif
		(NetworkInterface::GetVoice().IsAnyRemoteGamerTalking()/* || NetworkInterface::GetVoice().IsAnyRemoteGamerAboutToTalk()*/))
	{
		desiredRadioDuckingVolume = g_RadioDuckingVolumeVoiceChat;
	}
#endif

	// use unpausable timer so that it unducks radio in pause menu
	f32 smoothedRadioDuckingVolumeLin = sm_RadioDuckingSmoother.CalculateValue(audDriverUtil::ComputeLinearVolumeFromDb(desiredRadioDuckingVolume), g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1));
	
	const f32 missionCompleteDuckingVolDb = g_ScriptAudioEntity.ComputeRadioVolumeForMissionComplete();
	const f32 missionCompleteDuckingVolLin = audDriverUtil::ComputeLinearVolumeFromDb(missionCompleteDuckingVolDb);

	sm_FrontendRadioVolumeLin = missionCompleteDuckingVolLin * smoothedRadioDuckingVolumeLin * audDriverUtil::ComputeLinearVolumeFromDb(radioProxyDuckVolDb);

	sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("radioFrontendVol", 0xc00f6acd), 1.f-sm_FrontendRadioVolumeLin);
	if(sm_PlayerInVehScene)
	{
		sm_PlayerInVehScene->SetVariableValue(ATSTRINGHASH("musicVol", 0x7F9D4065),1.f- GetMusicVolume());
	}
	
	// this is relatively nasty - if the music volume (controlled via the frontend) is fully silenced then set the radio category to silence, so that
	// emitters (which don't go via the music effect route) are also muted
	// also have to explicitly mute positioned radio in the frontend, since the radio can be unpaused in the audio menu

	// Note posRadioVol is a linear volume the wrong way round; 1 is silent, 0 is full volume
	if(GetMusicVolume() == 0.f)
	{
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("posRadioVol", 0x4be8fa9a), 1.f);
	}
	else
	{
		sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("posRadioVol", 0x4be8fa9a), 0.f);
	}

#endif // NA_RADIO_ENABLED
/*
	f32 desiredGPSDuckingVolume = 0.0f;
	// Is there a scripted conversation ongoing?
	if (g_ScriptAudioEntity.ShouldDuckForScriptedConversation())
	{
		desiredGPSDuckingVolume = g_RadioDuckingVolume;
	}
	f32 smoothedGPSDuckingVolume = sm_GPSDuckingSmoother.CalculateValue(audDriverUtil::ComputeLinearVolumeFromDb(desiredGPSDuckingVolume), sm_TimeInMs);
	g_GpsCategory->SetVolumeLinear(smoothedGPSDuckingVolume);
*/	

	// Duck all things train if we're riding on it or in it
	/*f32 trainVolume = 1.0f;
	if (camInterface::IsAttachedToTrain()) - CDE: Removed custom camera code for trains.
	{
		trainVolume = g_AttachedToTrainVolume;
	}
	// also duck the train if we're in slowmo as its rather prone to clipping
	if(fwTimer::GetTimeWarpActive() < 0.9f)
	{
		trainVolume *= audDriverUtil::ComputeLinearVolumeFromDb(g_SlowMoTrainVol);
	}
	if (g_TrainCategory)
	{
		g_TrainCategory->SetVolumeLinear(trainVolume);
	} */
	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	f32 applyValue = 1.0f;
	f32 weatherApply = sm_PlayerVehicleOpenness;

	if(g_ScriptAudioEntity.ShouldDuckForScriptedConversation())
	{
		if (sm_SpeechScene )
		{
			if(!sm_SpeechSceeneApplied)
			{
				naDisplayf("SPEECH_SCENE apply set to 1.");
				sm_TimeSpeechSceneLastStarted = currentTime;
				sm_SpeechSceeneApplied = true;
				sm_SpeechGunfirePatchApplyAtStart = (sm_SpeechGunfirePatch ? sm_SpeechGunfirePatch->GetApplyFactor() : 0.f);
				sm_SpeechScorePatchApplyAtStart = (sm_SpeechScorePatch ? sm_SpeechScorePatch->GetApplyFactor() : 0.f);
				sm_SpeechVehiclesPatchApplyAtStart = (sm_SpeechVehiclesPatch ? sm_SpeechVehiclesPatch->GetApplyFactor() : 0.f);
				sm_SpeechVehiclesFirstPersonPatchApplyAtStart = (sm_SpeechVehiclesFirstPersonPatch ? sm_SpeechVehiclesFirstPersonPatch->GetApplyFactor() : 0.f);
				sm_SpeechWeatherPatchApplyAtStart = (sm_SpeechWeatherPatch ? sm_SpeechWeatherPatch->GetApplyFactor() : 0.f);
			}

			u32 timeSinceSceneStarted = (currentTime - sm_TimeSpeechSceneLastStarted);
			if(sm_SpeechGunfirePatch && sm_SpeechGunfirePatch->GetApplyFactor() != 1.0f)
			{
				applyValue = (sm_SpeechGunfirePatch->GetFadeInTime() == 0) ? 1.0f:
					Clamp(sm_SpeechGunfirePatchApplyAtStart + ((float)timeSinceSceneStarted/(float)sm_SpeechGunfirePatch->GetFadeInTime()),
					0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("gunfireApply", applyValue);
			}
			if(sm_SpeechScorePatch && sm_SpeechScorePatch->GetApplyFactor() != 1.0f)
			{
				applyValue = (sm_SpeechScorePatch->GetFadeInTime() == 0) ? 1.0f:
					Clamp(sm_SpeechScorePatchApplyAtStart + ((float)timeSinceSceneStarted/(float)sm_SpeechScorePatch->GetFadeInTime()),
					0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("scoreApply", applyValue);
			}
			if(sm_SpeechVehiclesPatch && sm_SpeechVehiclesPatch->GetApplyFactor() != 1.0f)
			{
				applyValue = (sm_SpeechVehiclesPatch->GetFadeInTime() == 0) ? 1.0f:
					Clamp(sm_SpeechVehiclesPatchApplyAtStart + ((float)timeSinceSceneStarted/(float)sm_SpeechVehiclesPatch->GetFadeInTime()),
					0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("vehiclesApply", applyValue);
			}
			if(sm_SpeechVehiclesFirstPersonPatch)
			{
				applyValue = (sm_SpeechVehiclesFirstPersonPatch->GetFadeInTime() == 0) ? 1.0f:
					Clamp(sm_SpeechVehiclesFirstPersonPatchApplyAtStart + ((float)timeSinceSceneStarted/(float)sm_SpeechVehiclesFirstPersonPatch->GetFadeInTime()),
					0.0f, 1.0f);
				// the apply factor it's only 1 when in 1st person camera : 
				if( !sm_Microphones.IsVehBonnetMic() )
				{
					applyValue = (sm_SpeechVehiclesFirstPersonPatch->GetFadeInTime() == 0) ? 0.0f:
						Clamp(sm_SpeechVehiclesFirstPersonPatchApplyAtStop - ((float)timeSinceSceneStarted/(float)sm_SpeechVehiclesFirstPersonPatch->GetFadeOutTime()), 0.0f, 1.0f);
				}
				sm_SpeechScene->SetVariableValue("vehiclesFirstPersonApply", applyValue);
			}
			if(sm_SpeechWeatherPatch && sm_SpeechWeatherPatch->GetApplyFactor() != weatherApply)
			{
				applyValue = (sm_SpeechWeatherPatch->GetFadeInTime() == 0) ? 1.0f:
					Clamp(sm_SpeechWeatherPatchApplyAtStart + ((float)timeSinceSceneStarted/(float)sm_SpeechWeatherPatch->GetFadeInTime()),
					0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("weatherApply", applyValue * weatherApply);
			}
		}
	}
	else
	{
		if(sm_SpeechScene)
		{
			if(sm_SpeechSceeneApplied)
			{
				naDisplayf("SPEECH_SCENE apply set to 0.");
				sm_TimeSpeechSceneLastStopped = currentTime;
				sm_SpeechSceeneApplied = false;
				sm_SpeechGunfirePatchApplyAtStop = (sm_SpeechGunfirePatch ? sm_SpeechGunfirePatch->GetApplyFactor() : 0.f);
				sm_SpeechScorePatchApplyAtStop = (sm_SpeechScorePatch ? sm_SpeechScorePatch->GetApplyFactor() : 0.f);
				sm_SpeechVehiclesPatchApplyAtStop = (sm_SpeechVehiclesPatch ? sm_SpeechVehiclesPatch->GetApplyFactor() : 0.f);
				sm_SpeechVehiclesFirstPersonPatchApplyAtStop = (sm_SpeechVehiclesFirstPersonPatch ? sm_SpeechVehiclesFirstPersonPatch->GetApplyFactor() : 0.f);
				sm_SpeechWeatherPatchApplyAtStop = (sm_SpeechWeatherPatch ? sm_SpeechWeatherPatch->GetApplyFactor() : 0.f);

			}

			u32 timeSinceSceneStopped = (currentTime - sm_TimeSpeechSceneLastStopped);
			if(sm_SpeechGunfirePatch && sm_SpeechGunfirePatch->GetApplyFactor() != 0.0f)
			{
				applyValue = (sm_SpeechGunfirePatch->GetFadeOutTime() == 0) ? 0.0f :
					Clamp(sm_SpeechGunfirePatchApplyAtStop - ((float)timeSinceSceneStopped/(float)sm_SpeechGunfirePatch->GetFadeOutTime()), 0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("gunfireApply", applyValue);
			}
			if(sm_SpeechScorePatch && sm_SpeechScorePatch->GetApplyFactor() != 0.0f)
			{
				applyValue = (sm_SpeechScorePatch->GetFadeOutTime() == 0) ? 0.0f:
					Clamp(sm_SpeechScorePatchApplyAtStop - ((float)timeSinceSceneStopped/(float)sm_SpeechScorePatch->GetFadeOutTime()), 0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("scoreApply", applyValue);
			}
			if(sm_SpeechVehiclesPatch && sm_SpeechVehiclesPatch->GetApplyFactor() != 0.0f)
			{
				applyValue = (sm_SpeechVehiclesPatch->GetFadeOutTime() == 0) ? 0.0f:
					Clamp(sm_SpeechVehiclesPatchApplyAtStop - ((float)timeSinceSceneStopped/(float)sm_SpeechVehiclesPatch->GetFadeOutTime()), 0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("vehiclesApply", applyValue);
			}
			if(sm_SpeechVehiclesFirstPersonPatch && sm_SpeechVehiclesFirstPersonPatch->GetApplyFactor() != 0.0f)
			{
				applyValue = (sm_SpeechVehiclesFirstPersonPatch->GetFadeOutTime() == 0) ? 0.0f:
					Clamp(sm_SpeechVehiclesFirstPersonPatchApplyAtStop - ((float)timeSinceSceneStopped/(float)sm_SpeechVehiclesFirstPersonPatch->GetFadeOutTime()), 0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("vehiclesFirstPersonApply", applyValue);
			}
			if(sm_SpeechWeatherPatch && sm_SpeechWeatherPatch->GetApplyFactor() != weatherApply)
			{
				applyValue = (sm_SpeechWeatherPatch->GetFadeOutTime() == 0) ? 0.0f:
					Clamp(sm_SpeechWeatherPatchApplyAtStop - ((float)timeSinceSceneStopped/(float)sm_SpeechWeatherPatch->GetFadeOutTime()), 0.0f, 1.0f);
				sm_SpeechScene->SetVariableValue("weatherApply", applyValue * weatherApply);
			}
		}
	}
	// Get rid of waves in interiors. And church bells.
	f32 playerInteriorRatio = 0.0f;
	//bool isInteriorASubway = false;
	//bool inInterior = audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(&isInteriorASubway, &playerInteriorRatio);

	sm_NorthAudioMixScene->SetVariableValue(ATSTRINGHASH("interiorRatio", 0x13fc3a70), playerInteriorRatio);

	/*
	if (g_AmbienceJetsCategory)
	{
		g_AmbienceJetsCategory->SetVolumeLinear(waveVolumeDB);
	} 
	// If we're in a subway, increase the rolloff on trains - need to smooth it, over at least a second
	f32 desiredTrainRolloff = 1.0f;
	if (inInterior && isInteriorASubway)
	{
		desiredTrainRolloff = g_TrainRolloffInSubways;
	}
	f32 trainRolloffSmoothed = sm_TrainRolloffInSubwaySmoother.CalculateValue(desiredTrainRolloff, sm_TimeInMs);
	if (g_TrainCategory)
	{
		g_TrainCategory->BeginDistRolloffScaleApproach(trainRolloffSmoothed);
	} */

	// We don't want to smooth leakage when going from black as it's a hard cut, so reset the smoother
	if(IsScreenFadedOut())
	{
		sm_CutsceneLeakageSmoother.Reset();
	}

	// Also do the mocap cutscene leakage here, because it vaguely fits
	f32 leakage;
	if (sm_IsCutsceneActive)
	{
		leakage = sm_CutsceneLeakageSmoother.CalculateValue(g_CutscenePositionedLeakage, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		g_AudioEngine.GetEnvironment().SetGlobalPositionedLeakage(leakage);
	}
	else
	{
		leakage = sm_CutsceneLeakageSmoother.CalculateValue(0.0f, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		g_AudioEngine.GetEnvironment().SetGlobalPositionedLeakage(leakage);
	}

#if __BANK
	if(g_DebugCutsceneLeakage)
	{
		sm_CutsceneLeakageSmoother.SetRate(g_CutsceneLeakageSmootherRate * 0.001f);

		static audMeterList leakageMeter;
		static const char* leakageMeterName = "Cutscene Leakage";
		static f32 leakageMeterValue;

		leakageMeterValue = leakage;

		leakageMeter.left = 150.f;
		leakageMeter.width = 50.f;
		leakageMeter.bottom = 350.f;
		leakageMeter.height = 50.f;
		leakageMeter.names = &leakageMeterName;
		leakageMeter.values = &leakageMeterValue;
		leakageMeter.numValues = 1;
		DrawLevelMeters(&leakageMeter);
	}
#endif

#if __BANK
	if(g_ShouldMuteWind)
	{
		if(!g_WindMuteController)
		{
			g_WindMuteController = AUDCATEGORYCONTROLLERMANAGER.CreateController(ATSTRINGHASH("AMBIENCE_WEATHER", 0xB26931C3));
			if(g_WindMuteController)
			{
				g_WindMuteController->SetVolumeLinear(0.f);
			}
		}
	}
	else if(g_WindMuteController)
	{
		AUDCATEGORYCONTROLLERMANAGER.DestroyController(g_WindMuteController);
		g_WindMuteController = NULL;
	}
#endif
}

void audNorthAudioEngine::Pause()
{
	// Timer 0 is the normal timer, timer 5 is the slow-mo timer
	// Both should pause/unpause together
	if((sm_FrontendScene && sm_FrontendScene->IsFullyAppliedForPause()))
	{ 
		sm_Paused = true;
		g_AudioEngine.GetSoundManager().Pause(0);
		g_AudioEngine.GetSoundManager().Pause(5);
	}
	else if(fwTimer::IsUserPaused() BANK_ONLY(|| fwTimer::IsDebugPause()))
	{
		// When pausing via the debug keyboard we don't want to apply the fade, but we do want to pause
		sm_Paused = true;
		g_AudioEngine.GetSoundManager().Pause(0); 
		g_AudioEngine.GetSoundManager().Pause(5);
	}
	g_AudioEngine.SetGamePaused(true);
}

void audNorthAudioEngine::UnPause()
{
	sm_Paused = false;
	g_AudioEngine.GetSoundManager().UnPause(0);
	g_AudioEngine.GetSoundManager().UnPause(5);
	g_AudioEngine.SetGamePaused(false);
}

void audNorthAudioEngine::RegisterLoudSound(Vector3 &pos, bool playerShot)
{
	Vector3 dist = pos - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	if(dist.Mag2() <= (g_audLoudSoundDistanceMag*g_audLoudSoundDistanceMag))
	{
		audNorthAudioEngine::sm_LastLoudSoundTime = fwTimer::GetTimeInMilliseconds();
		g_EmitterAudioEntity.NotifyLoudSound();
		// Send message to the superconductor to stop the QUITE_SCENE
		if(audSuperConductor::sm_StopQSOnLoudSounds && !playerShot && !NetworkInterface::IsGameInProgress())
		{
			ConductorMessageData message;
			message.conductorName = SuperConductor;
			message.message = StopQuietScene;
			message.bExtraInfo = false;
			message.vExtraInfo = Vector3((f32)audSuperConductor::sm_LoudSoundQSFadeOutTime
										,(f32)audSuperConductor::sm_LoudSoundQSDelayTime
										,(f32)audSuperConductor::sm_LoudSoundQSFadeInTime);
			SUPERCONDUCTOR.SendConductorMessage(message);
		}
	}
}

u32 audNorthAudioEngine::GetLastLoudSoundTime()
{
	return audNorthAudioEngine::sm_LastLoudSoundTime;
}

const char* audNorthAudioEngine::GetCurrentAudioLevelName()
{			
	// Game level index value defaults to level 1 even with 0 levels loaded...
	s32 levelIndex = CGameLogic::GetCurrentLevelIndex();

	if(levelIndex < CScene::GetLevelsData().GetCount())
	{				
		const char* levelNameString = CScene::GetLevelsData().GetFilename(levelIndex);
		s32 baseFolderStart = atString(levelNameString).LastIndexOf('\\');

		if(baseFolderStart >= 0)
		{
			return &levelNameString[baseFolderStart + 1];
		}		
	}

	return "gta5";
}

bool audNorthAudioEngine::IsScreenFadedOut()
{
	return sm_IsFadedToBlack;
}

void audNorthAudioEngine::DebugDraw()
{
#if __BANK
	g_RadioAudioEntity.DrawDebug();

	DrawBufferedLevelMeters();
	g_AudioEngine.DrawDebug();
	g_AudioEngine.GetSoundManager().DebugDrawSounds();

	audSpeechAudioEntity::DebugDraw();

	if(audGrainPlayer::s_DebugDrawGrainPlayer)
	{
		audGrainPlayer::s_DebugDrawGrainPlayer->DebugDraw();
	}

	sm_DynamicMixer.DrawDebugScenes();

	g_PoliceScanner.DrawDebug();

	DebugDrawSlowMo();

	audSoundDebugDrawManager waveSlotDrawMgr(100.f, 50.f, 50.f, 720.f);
	audWaveSlot::DebugDraw(waveSlotDrawMgr);
#endif
}
void *audNorthAudioEngine::GetObject(const u32 nameHash, const u32 TYPE_ID)
{
	u8 *ptr = static_cast<u8*>(sm_MetadataMgr.GetObjectMetadataPtr(nameHash));
	if(ptr)
	{
		naCErrorf(TYPE_ID == (*ptr), "In GetObject, we've found an object for nameHash (%d) but it's type id (%d) doesn't match the one we expected (%d)", nameHash, (*ptr), TYPE_ID);
		if(TYPE_ID != (*ptr))
		{
			ptr = NULL;
		}
	}
	return ptr;
}

void audNorthAudioEngine::AmbisonicDraw(ambisonicDrawData data[90])
{
#if __BANK
	Vector2 unit = grcDebugDraw::Get2DAspect();
	Vector2 origin(0.5f, 0.5f);
	Vector2 startA(0.f, 0.f);
	Vector2 prevA(0.f, 0.f);
	Vector2 ang(0.f, 0.f);
	Vector2 angE(0.f, 0.f);
	Vector2 angV(0.f, 0.f);
	Vector2 vecE(0.f, 0.f);
	Vector2 vecV(0.f, 0.f);
	Vector2 vecEPrev(0.f, 0.f);
	Vector2 vecVPrev(0.f, 0.f);
	Vector2 energy(0.f, 0.f);
	Vector2 energyPrev(0.f, 0.f);
	Color32 colour;
	
	
	startA.Set(0.2f*Cosf(0.f), 0.2f*Sinf(DtoR*(0.f)));
	angE.Set(0.15f*Cosf(data[0].reproducedAngE), 0.15f*Sinf(data[0].reproducedAngE));
	angV.Set(0.1f*Cosf(data[0].reproducedAngV), 0.1f*Sinf(data[0].reproducedAngV));

	startA.Multiply(unit);
	angE.Multiply(unit);
	angV.Multiply(unit);
	startA.Add(origin);
	angE.Add(origin);
	angV.Add(origin);

	vecE.Set(0.2f*data[0].vecLengthE*Cosf(0.f), 0.2f*data[0].vecLengthE*Sinf(0.f));
	vecE.Multiply(unit);
	vecE.Add(origin);

	vecV.Set(0.2f*data[0].vecLengthV*Cosf(0.f), 0.2f*data[0].vecLengthV*Sinf(0.f));
	vecV.Multiply(unit);
	vecV.Add(origin);

	energy.Set(0.05f*data[0].energy*Cosf(0.f), 0.05f*data[0].energy*Sinf(0.f));
	energy.Multiply(unit);
	energy.Add(origin);

	colour.Set(255, 0, 0);
	grcDebugDraw::Line(origin, startA, colour);
	colour.Set(0, 255, 0);
	grcDebugDraw::Line(origin, angE, colour);
	colour.Set(0, 0, 255);
	grcDebugDraw::Line(origin, angV, colour);
	
	prevA.Set(startA);
	vecVPrev.Set(vecV);
	vecEPrev.Set(vecE);
	energyPrev.Set(energy);

	for(s32 i=1; i<90; i++)
	{
		f32 curAngle = DtoR*((float)(i*4));
		f32 curCos = Cosf(curAngle);
		f32 curSin = Sinf(curAngle);
		ang.Set(0.2f*curCos, 0.2f*curSin);
		ang.Multiply(unit);
		ang.Add(origin);


		vecE.Set(0.2f*data[i].vecLengthE*curCos, 0.2f*data[i].vecLengthE*curSin);
		vecE.Multiply(unit);
		vecE.Add(origin);

		vecV.Set(0.2f*data[i].vecLengthV*curCos, 0.2f*data[i].vecLengthV*curSin);
		vecV.Multiply(unit);
		vecV.Add(origin);

		energy.Set(0.05f*data[i].energy*curCos, 0.05f*data[i].energy*curSin);
		energy.Multiply(unit);
		energy.Add(origin);

		colour.Set(255, 255, 255);
		grcDebugDraw::Line(prevA, ang, colour);
		colour.Set(100, 50, 100);
		grcDebugDraw::Line(vecEPrev, vecE, colour);
		colour.Set(100, 150, 0);
		grcDebugDraw::Line(vecVPrev, vecV, colour);
		colour.Set(0, 0, 0);
		grcDebugDraw::Line(energyPrev, energy, colour);

		vecVPrev.Set(vecV);
		vecEPrev.Set(vecE);
		energyPrev.Set(energy);

		//Compare incident angles to reproduced angles
		if(i == 15 || i ==22 || i == 30 || i == 45 || i == 60 || i == 75)
		{
			angE.Set(0.15f*Cosf(data[i].reproducedAngE), 0.15f*Sinf(data[i].reproducedAngE));
			angV.Set(0.1f*Cosf(data[i].reproducedAngV), 0.1f*Sinf(data[i].reproducedAngV));
			angE.Multiply(unit);
			angV.Multiply(unit);
			angE.Add(origin);
			angV.Add(origin);

			colour.Set(255, 255, 255);
			grcDebugDraw::Line(origin, ang, colour);
			colour.Set(0, 255, 0);
			grcDebugDraw::Line(origin, angE, colour);
			colour.Set(0, 0, 255);
			grcDebugDraw::Line(origin, angV, colour);
		}

		prevA.Set(ang);
	}

	ang.Set(0.2f*Cosf(2.f*PI), 0.2f*Sinf(2*PI));
	ang.Multiply(unit);
	ang.Add(origin);

	vecE.Set(0.2f*data[0].vecLengthE*Cosf(2.f*PI), 0.2f*data[0].vecLengthE*Sinf(2.f*PI));
	vecE.Multiply(unit);
	vecE.Add(origin);

	vecV.Set(0.2f*data[0].vecLengthV*Cosf(2.f*PI), 0.2f*data[0].vecLengthV*Sinf(2.f*PI));
	vecV.Multiply(unit);
	vecV.Add(origin);

	energy.Set(0.05f*data[0].energy*Cosf(2*PI), 0.05f*data[0].energy*Sinf(2*PI));
	energy.Multiply(unit);
	energy.Add(origin);

	colour.Set(255, 255, 255);
	grcDebugDraw::Line(prevA, ang, colour);
	colour.Set(100, 50, 100);
	grcDebugDraw::Line(vecEPrev, vecE, colour);
	colour.Set(100, 150, 0);
	grcDebugDraw::Line(vecVPrev, vecV, colour);
	colour.Set(0, 0, 0);
	grcDebugDraw::Line(energyPrev, energy, colour);
#else // __BANK
	(void)data;
#endif // __BANK
}

void audNorthAudioEngine::AmbisonicSave()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();

	if(settings.AreSettingsValid())
	{
		ambisonicDecodeData decoderData;
		g_AudioEngine.GetAmbisonics().SaveDecoder(decoderData);

		s32 i = static_cast<s32>(CProfileSettings::AMBISONIC_DECODER);
		for(s32 sp = 0; sp < AMBISONICS_SPEAKER_MAX; sp++)
		{
			for(s32 a = 0; a < AMB_ORDER; a++)
			{
				settings.Set(static_cast<CProfileSettings::ProfileSettingId>(i), decoderData.decoder[sp].Co[a]);
				++i;
				settings.Set(static_cast<CProfileSettings::ProfileSettingId>(i), decoderData.decoder[sp].Si[a]);
				++i;
			}
			settings.Set(static_cast<CProfileSettings::ProfileSettingId>(i), decoderData.decoder[sp].w);
			++i;
		}

		settings.Set(CProfileSettings::AMBISONIC_DECODER_TYPE, decoderData.decoderType);

		settings.Write();
		Displayf("Ambisonic Decoder Saved");
	}
}

void audNorthAudioEngine::AmbisonicLoad()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();

	if(settings.AreSettingsValid())
	{
		ambisonicDecodeData decodeData;

		s32 i = static_cast<s32>(CProfileSettings::AMBISONIC_DECODER);
		for(s32 sp = 0; sp < AMBISONICS_SPEAKER_MAX; sp++)
		{
			for(s32 a = 0; a < AMB_ORDER; a++)
			{
				if(settings.Exists(static_cast<CProfileSettings::ProfileSettingId>(i))) 
				{
					decodeData.decoder[sp].Co[a] = settings.GetFloat(static_cast<CProfileSettings::ProfileSettingId>(i));
				}
				++i;
				if(settings.Exists(static_cast<CProfileSettings::ProfileSettingId>(i))) 
				{
					decodeData.decoder[sp].Si[a] = settings.GetFloat(static_cast<CProfileSettings::ProfileSettingId>(i));
				}
				++i;
			}
			if(settings.Exists(static_cast<CProfileSettings::ProfileSettingId>(i))) 
			{
				decodeData.decoder[sp].w = settings.GetFloat(static_cast<CProfileSettings::ProfileSettingId>(i));
			}
			++i;
		}

		if(settings.Exists(CProfileSettings::AMBISONIC_DECODER_TYPE)) 
		{
			decodeData.decoderType = static_cast<EDecoderType>(settings.GetInt(CProfileSettings::AMBISONIC_DECODER_TYPE));
		}

		g_AudioEngine.GetAmbisonics().LoadDecoder(decodeData);
		Displayf("Ambisonic Decoder Loaded");
	}
}

void audNorthAudioEngine::MuteCategory(audScene ** scene, const char * settings, bool mute)
{
	if(mute && !*scene)
	{
		naDisplayf("[NorthAudio] Muting %s", settings);
		sm_DynamicMixer.StartScene(settings, scene);
	}
	else if(!mute && *scene)
	{
		naDisplayf("[NorthAudio] Unmuting %s", settings);
		(*scene)->Stop();
	}
#if ! __FINAL
	else if(mute)
	{
		naDisplayf("[NorthAudio] Mute %s is already on", settings);
	}
	else
	{
		naDisplayf("[NorthAudio] Mute %s is already off", settings);
	}
#endif

}	

void audNorthAudioEngine::MuteAmbience(bool mute)
{
	MuteCategory(&sm_AmbienceMuted, "MUTES_AMBIENCE_SCENE", mute);
}
		
void audNorthAudioEngine::MuteCutscene(bool mute)
{
	MuteCategory(&sm_CutsceneMuted, "MUTES_CUTSCENE_SCENE", mute);
}

void audNorthAudioEngine::MuteFrontend(bool mute)
{
	MuteCategory(&sm_FrontendMuted, "MUTES_FRONTEND_SCENE", mute);
}

void audNorthAudioEngine::MuteMusic(bool mute)
{
	MuteCategory(&sm_MusicMuted, "MUTES_MUSIC_SCENE", mute);
}

void audNorthAudioEngine::MuteRadio(bool mute)
{
	MuteCategory(&sm_RadioMuted, "MUTES_RADIO_SCENE", mute);
}

void audNorthAudioEngine::MuteSfx(bool mute)
{
	MuteCategory(&sm_SfxMuted, "MUTES_SFX_SCENE", mute);
}

void audNorthAudioEngine::MuteSpeech(bool mute)
{
	MuteCategory(&sm_SpeechMuted, "MUTES_SPEECH_SCENE", mute);
}

void audNorthAudioEngine::MuteGuns(bool mute)
{
	MuteCategory(&sm_GunsMuted, "MUTES_GUNS_SCENE", mute);
}

void audNorthAudioEngine::MuteVehicles(bool mute)
{
	MuteCategory(&sm_VehiclesMuted, "MUTES_VEHICLES_SCENE", mute);
}

void audNorthAudioEngine::ToggleMuteAmbience() 
{ 
	MuteAmbience(sm_AmbienceMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteCutscene() 
{ 
	MuteCutscene(sm_CutsceneMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteFrontend() 
{ 
	MuteFrontend(sm_FrontendMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteMusic()	  
{ 
	MuteMusic(sm_MusicMuted ? false:true); 
}
void audNorthAudioEngine::ToggleMuteRadio()	  
{ 
	MuteMusic(sm_RadioMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteSfx()	  
{ 
	MuteSfx(sm_SfxMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteSpeech()	  
{ 
	MuteSpeech(sm_SpeechMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteGuns()	  
{ 
	MuteGuns(sm_GunsMuted ? false:true); 
}

void audNorthAudioEngine::ToggleMuteVehicles()	  
{ 
	MuteVehicles(sm_VehiclesMuted ? false:true); 
}

void audNorthAudioEngine::NotifyLocalPlayerArrested()
{
	NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportSuspectArrested());
	g_InteractiveMusicManager.GetWantedMusic().NotifyArrested();
}

void audNorthAudioEngine::NotifyLocalPlayerDied()
{
	/*if(!CTheScripts::GetPlayerIsOnAMission() && !g_ScriptAudioEntity.IsPlayingMissionComplete() && g_ScriptAudioEntity.CanPlayDeathJingle())
	{
		// Before Triggering it check if we have to cancel if there is one preparing ( We have seen this happening when you fail a stuntjump and die at the same time).
		if( g_ScriptAudioEntity.IsPreparingMissionComplete())
		{
			g_ScriptAudioEntity.StopMissionCompleteSound();
		}
		g_ScriptAudioEntity.TriggerMissionComplete("DEAD_OFFMISSION", 3);
	}*/
	g_InteractiveMusicManager.GetWantedMusic().NotifyDied();
}

void audNorthAudioEngine::NotifyLocalPlayerPlaneCrashed()
{
	g_InteractiveMusicManager.GetVehicleMusic().NotifyPlaneCrashed();
}

void audNorthAudioEngine::NotifyLocalPlayerBailedFromAircraft()
{
	g_InteractiveMusicManager.GetVehicleMusic().NotifyPlayerBailed();
}

void audNorthAudioEngine::NotifyOpenNetwork()
{
	sm_IsMPDataRequested = true;
}

void audNorthAudioEngine::NotifyCloseNetwork(const bool fullClose)
{
	g_InteractiveMusicManager.GetWantedMusic().NotifyCloseNetwork();
	if(fullClose)
	{
		sm_IsMPDataRequested = false;
	}
}

void audNorthAudioEngine::StartLongPlayerSwitch(const CPed &UNUSED_PARAM(oldPed), const CPed &UNUSED_PARAM(newPed))
{
	g_RadioAudioEntity.SetForcePlayerCharacterStation(true);
}

void audNorthAudioEngine::StopLongPlayerSwitch()
{
	g_RadioAudioEntity.SetForcePlayerCharacterStation(false);
	g_FrontendAudioEntity.StopLongSwitchSkyLoop();
}

void audNorthAudioEngine::StartPauseMenuSlowMo()
{
	if(sm_PauseMenuSlowMoCount == 0)
	{
		ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_PAUSEMENU", 0xFAFBB6BF));
	}

	sm_PauseMenuSlowMoCount++;
}

void audNorthAudioEngine::StopPauseMenuSlowMo()
{
	if(sm_PauseMenuSlowMoCount > 0)
	{
		sm_PauseMenuSlowMoCount--;
		if(sm_PauseMenuSlowMoCount == 0)
		{
			sm_WaitingForPauseMenuSlowMoToEnd = true;
			DeactivateSlowMoMode(AUD_SLOWMO_PAUSEMENU);
		}
	}
}

void audNorthAudioEngine::SetSfxVolume(const float volLinear)
{
	sm_SfxVolume = volLinear;
	if(sm_SFXSliderController)
	{
		sm_SFXSliderController->SetVolumeLinear(volLinear);
	}
	if(sm_NoFadeVolCategoryController)
	{
		sm_NoFadeVolCategoryController->SetVolumeLinear(volLinear);
	}
	if(sm_ScriptedOverrideFadeVolCategoryController)
	{
		sm_ScriptedOverrideFadeVolCategoryController->SetVolumeLinear(volLinear);
	}
	
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Audio.Sliders.SFXSlider", 0x2432BB37), volLinear);
}

void audNorthAudioEngine::SetMusicVolume(const float volLinear)
{
	sm_MusicVolume = volLinear;
	if(sm_MusicSliderController)
	{
		sm_MusicSliderController->SetVolumeLinear(volLinear);
	}

	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Audio.Sliders.MusicSlider", 0xFB69E838), volLinear);
}

#if RSG_PS3
void audNorthAudioEngine::CheckForWirelessHeadset(const bool loadPRX /*=false*/)
{
	sm_IsWirelessHeadsetConnected = ((audMixerDevicePs3*)audDriver::GetMixer())->IsPulseHeadsetConnected(loadPRX);
}
#endif

#if __BANK

void audNorthAudioEngine::UpdateRAVEVariables()
{
	if((fwTimer::GetFrameCount() & 7) == 7)
	{
		if(g_AudioEngine.GetRemoteControl().IsConnected())
		{
			static char messageBuf[4096];
			formatf(messageBuf, "<RAVEMessage timeStamp=\"%u\">", g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1));

			// Focus Entity variables
			CEntity *ent = CDebugScene::FocusEntities_Get(0);

			if(ent && ent->GetAudioEntity())
			{
				safecat(messageBuf, "<FocusEntityVariables>");

				audVariableBlock *vars = ent->GetAudioEntity()->GetVariableBlock();
				if(vars)
				{
					for(u32 i = 0; i < vars->GetMetadata()->numVariables; i++)
					{
						safecatf(messageBuf, "<Variable nameHash=\"%u\" value=\"%f\"/>", vars->GetMetadata()->Variable[i].Name, vars->GetValueByIndex(i));
					}			
				}

				safecat(messageBuf, "</FocusEntityVariables>");			
			}

			// Global variables
			ravesimpletypes::VariableList *list = audConfig::GetMetadataManager().GetObject<ravesimpletypes::VariableList>("GlobalVariableList");
			safecat(messageBuf, "<GlobalVariables>");
			if(list)
			{
				for(u32 i = 0; i < list->numVariables; i++)
				{		
					safecatf(messageBuf, "<Variable nameHash=\"%u\" value=\"%f\"/>", list->Variable[i].Name, g_AudioEngine.GetSoundManager().GetVariableByIndex(i));
				}
			}
			safecat(messageBuf, "</GlobalVariables>");

			safecat(messageBuf, "</RAVEMessage>");
			g_AudioEngine.GetRemoteControl().SendXmlMessage(messageBuf);
		}
	}
}

void audNorthAudioEngine::UpdateAuditionSound()
{
	g_AudioEngine.GetRemoteControl().SetAuditionSoundDrawMode(sm_AuditionSoundDrawMode);
	g_AudioEngine.GetRemoteControl().SetAuditionSoundDrawOffsetY(sm_DebugDrawYOffset);

	u32 soundNameHash;
	if(SOUNDFACTORY.GetAuditionStartSound(soundNameHash))
	{
		audMetadataObjectInfo info;
		if(SOUNDFACTORY.GetMetadataManager().GetObjectInfo(soundNameHash, info))
		{
			if(gSoundsIsOfType(info.GetType(), Sound::TYPE_ID))
			{
				AuditionSound(soundNameHash);
			}
		}
		else
		{
			audWarningf("RAVE Auditioning: Failed to find sound with hash %u", soundNameHash);
		}
	}

	u32 auditionStopNameHash;
	if(SOUNDFACTORY.GetAuditionStopSound(auditionStopNameHash))
	{
		audSound *auditionSound = *g_AudioEngine.GetRemoteControl().GetAuditionSoundPtr();
		if(auditionSound)
		{
			auditionSound->StopAndForget();
		}
	}
}

void audNorthAudioEngine::AuditionSound(const u32 soundNameHash)
{
	audSound **auditionSoundPtr = g_AudioEngine.GetRemoteControl().GetAuditionSoundPtr();

	if(*auditionSoundPtr)
	{
		(*auditionSoundPtr)->StopAndForget();
	}

	audSoundInitParams initParams;
	initParams.RemoveHierarchy = false;
	initParams.IsAuditioning = true;
	if(sm_AuditionSoundsOnPPU)
	{
		// stick it in the last bucket, which should be reserved
		Assign(initParams.BucketId, audSound::GetStaticPool().GetNumBuckets()-1);
	}

	audWaveSlot *auditionWaveSlot = audWaveSlot::FindWaveSlot(g_AuditionWaveSlotName);
	initParams.WaveSlot = auditionWaveSlot;
	initParams.AllowLoad = sm_AllowAuditionSoundToLoad;
	initParams.TimerId = sm_AuditionSoundUnpausable ? 1 : 0;
	if(sm_ForceBaseCategory)
	{
		initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("BASE", 0x44E21C90));
	}

	CEntity *auditioningEntity = NULL;
	if(sm_ShouldAuditionThroughPlayer)
	{
		auditioningEntity = FindPlayerPed();
	}
	else if(sm_ShouldAuditionThroughFocusEntity)
	{
		auditioningEntity = CDebugScene::FocusEntities_Get(0);
	}

	if(auditioningEntity && !sm_ShouldAuditionPannedFrontend)
	{
		initParams.Tracker = auditioningEntity->GetPlaceableTracker();
		initParams.EnvironmentGroup = auditioningEntity->GetAudioEnvironmentGroup();
	}
	else
	{
		initParams.Pan = 0;
	}

	if(auditioningEntity && auditioningEntity->GetAudioEntity())
	{
		auditioningEntity->GetAudioEntity()->CreateSound_PersistentReference(soundNameHash, auditionSoundPtr, &initParams);
	}
	else
	{
		g_FrontendAudioEntity.CreateSound_PersistentReference(soundNameHash, auditionSoundPtr, &initParams);
	}

	if(*auditionSoundPtr == NULL)
	{
		audWarningf("Couldn't instantiate auditioned sound with hash %u / name %s", soundNameHash, SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash));
		return;
	}
	else
	{
		audSound *auditionSound = *auditionSoundPtr;
		auditionSound->PrepareAndPlay(auditionWaveSlot, sm_AllowAuditionSoundToLoad, 3000);
	}
}

void audNorthAudioEngine::AuditionSound(const audMetadataRef soundRef)
{
	audSound **auditionSoundPtr = g_AudioEngine.GetRemoteControl().GetAuditionSoundPtr();

	if(*auditionSoundPtr)
	{
		(*auditionSoundPtr)->StopAndForget();
	}

	audSoundInitParams initParams;
	initParams.RemoveHierarchy = false;
	initParams.IsAuditioning = true;
	if(sm_AuditionSoundsOnPPU)
	{
		// stick it in the last bucket, which should be reserved
		Assign(initParams.BucketId, audSound::GetStaticPool().GetNumBuckets()-1);
	}

	audWaveSlot *auditionWaveSlot = audWaveSlot::FindWaveSlot(g_AuditionWaveSlotName);
	initParams.WaveSlot = auditionWaveSlot;
	initParams.AllowLoad = sm_AllowAuditionSoundToLoad;
	initParams.TimerId = sm_AuditionSoundUnpausable ? 1 : 0;
	if(sm_ForceBaseCategory)
	{
		initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("BASE", 0x44E21C90));
	}

	CEntity *auditioningEntity = NULL;
	if(sm_ShouldAuditionThroughPlayer)
	{
		auditioningEntity = FindPlayerPed();
	}
	else if(sm_ShouldAuditionThroughFocusEntity)
	{
		auditioningEntity = CDebugScene::FocusEntities_Get(0);
	}

	if(auditioningEntity && !sm_ShouldAuditionPannedFrontend)
	{
		initParams.Tracker = auditioningEntity->GetPlaceableTracker();
		initParams.EnvironmentGroup = auditioningEntity->GetAudioEnvironmentGroup();
	}
	else
	{
		initParams.Pan = 0;
	}

	if(auditioningEntity && auditioningEntity->GetAudioEntity())
	{
		auditioningEntity->GetAudioEntity()->CreateSound_PersistentReference(soundRef, auditionSoundPtr, &initParams);
	}
	else
	{
		g_FrontendAudioEntity.CreateSound_PersistentReference(soundRef, auditionSoundPtr, &initParams);
	}

	if(*auditionSoundPtr == NULL)
	{
		audWarningf("Couldn't instantiate auditioned sound  %u / name %s",soundRef.Get(), SOUNDFACTORY.GetMetadataManager().GetObjectName(soundRef));
		return;
	}
	else
	{
		audSound *auditionSound = *auditionSoundPtr;
		auditionSound->PrepareAndPlay(auditionWaveSlot, sm_AllowAuditionSoundToLoad, 3000);
	}
}

void audNorthAudioEngine::DrawBufferedLevelMeters()
{
	if(sm_CurrentMeterIndex > 0)
	{		
		PUSH_DEFAULT_SCREEN();

		audDriver::SetDebugDrawRenderStates();

		for(u32 ml = 0; ml < sm_CurrentMeterIndex; ml++)
		{
			const float elemWidth = sm_MeterList[ml]->width / (float)sm_MeterList[ml]->numValues;

			// draw alpha'd background
			grcBegin(drawTriStrip, 4);
				grcColor(sm_MeterList[ml]->bgColour);
				grcVertex2f(sm_MeterList[ml]->left,sm_MeterList[ml]->bottom);
				grcVertex2f(sm_MeterList[ml]->left,sm_MeterList[ml]->bottom-sm_MeterList[ml]->height);
				grcVertex2f(sm_MeterList[ml]->left+sm_MeterList[ml]->width,sm_MeterList[ml]->bottom);
				grcVertex2f(sm_MeterList[ml]->left+sm_MeterList[ml]->width,sm_MeterList[ml]->bottom-sm_MeterList[ml]->height);
			grcEnd();

			for(u32 i = 0; i < sm_MeterList[ml]->numValues; i++)
			{
				const f32 x = sm_MeterList[ml]->left + (elemWidth * (float)i);
				const f32 value = Clamp(sm_MeterList[ml]->values[i] / sm_MeterList[ml]->rangeMax, 0.f, 1.f);
				const f32 y = sm_MeterList[ml]->bottom - (value * sm_MeterList[ml]->height);
				grcBegin(drawTriStrip, 4);
					grcColor(sm_MeterList[ml]->bottomColour);
					grcVertex2f(x, sm_MeterList[ml]->bottom);
					grcColor(sm_MeterList[ml]->topColour);
					grcVertex2f(x, y);
					grcColor(sm_MeterList[ml]->bottomColour);
					grcVertex2f(x + elemWidth, sm_MeterList[ml]->bottom);
					grcColor(sm_MeterList[ml]->topColour);
					grcVertex2f(x + elemWidth, y);
				grcEnd();

				grcBegin(drawLineStrip, 2);
					grcColor(sm_MeterList[ml]->bgColour);
					grcVertex2f(x, sm_MeterList[ml]->bottom);
					grcVertex2f(x, y);
				grcEnd();

				grcColor(sm_MeterList[ml]->textColour);
				grcDraw2dText(x, sm_MeterList[ml]->bottom + 10.f, sm_MeterList[ml]->names[i], true);

				if(sm_MeterList[ml]->drawValues)
				{
					char valueString[32];
					formatf(valueString, "%.02f", sm_MeterList[ml]->values[i]);
					grcDraw2dText(x, sm_MeterList[ml]->bottom + 20.f, valueString, true);
				}
			}

			if(sm_MeterList[ml]->title)
			{
				grcDraw2dText(sm_MeterList[ml]->left, sm_MeterList[ml]->bottom - sm_MeterList[ml]->height - 20.0f, sm_MeterList[ml]->title, true);
			}
		}

		sm_CurrentMeterIndex = 0;
		POP_DEFAULT_SCREEN();
	}
}

void audNorthAudioEngine::TriggerSound()
{
	audSoundInitParams initParams;
	initParams.RemoveHierarchy = sm_ShouldRemoveSoundHierarchy;
	initParams.WaveSlot = audWaveSlot::FindWaveSlot(g_AuditionWaveSlotName);
	initParams.AllowLoad = true;
	initParams.PrepareTimeLimit = 3000;
	initParams.IsAuditioning = true;
	if(sm_ShouldAuditionThroughPlayer)
	{
		initParams.Tracker = (FindPlayerPed()->GetPlaceableTracker());
		initParams.EnvironmentGroup = FindPlayerPed()->GetPedAudioEntity()->GetEnvironmentGroup(true);
	}

	if(sm_DebugSound)
	{
		sm_DebugSound->StopAndForget();
	}

	g_FrontendAudioEntity.CreateSound_PersistentReference(sm_SoundName, &sm_DebugSound, &initParams);
	if(sm_DebugSound)
	{
		sm_DebugSound->PrepareAndPlay();
	}
	else
	{
		naDisplayf("Couldn't play sound %s", sm_SoundName);
	}
}

void audNorthAudioEngine::StopSound()
{
	if(sm_DebugSound)
	{
		sm_DebugSound->StopAndForget();
	}
}

void audNorthAudioEngine::AuditionWaveSlotChanged()
{
	/*audWaveSlot* waveSlot = NULL;
	waveSlot = audWaveSlot::FindWaveSlot(g_AuditionWaveSlotName);
	g_AudioEngine.SetAuditionWaveSlot(waveSlot);*/
}

extern bank_bool g_DisableExplosionSFX;

void audNorthAudioEngine::InitWidgets()
{
	if (PARAM_audiodesigner.Get())
	{
		BANKMGR.CreateOutputWindow("Audio", "NamedColor:Black"); 
	}

	sprintf(g_AuditionWaveSlotName, "AMBIENT_SCRIPT_SLOT_0");
	AuditionWaveSlotChanged();

	if(PARAM_audiowidgets.Get())
	{
		CreateWidgets();
	}
	else
	{
		bkBank *bankPtr = BANKMGR.FindBank("Audio");
		if(bankPtr)
		{
			bankPtr->Destroy();
		}
		bkBank& bank = BANKMGR.CreateBank("Audio");

		bank.AddButton("Create Audio Widgets", CFA(CreateWidgets));
	}

}

void audNorthAudioEngine::StopSoundFromWidget()
{
	audSound **auditionSoundPtr = g_AudioEngine.GetRemoteControl().GetAuditionSoundPtr();

	if(*auditionSoundPtr)
	{
		(*auditionSoundPtr)->StopAndForget();
	}
}

void audNorthAudioEngine::TriggerSoundFromWidget()
{
	audMetadataObjectInfo info;
	audSoundSet soundSet;

	soundSet.Init(sm_AuditionSoundSetName);
	if(soundSet.IsInitialised())
	{
		AuditionSound(soundSet.Find(sm_AuditionSoundName));
	}
	else
	{
		if(SOUNDFACTORY.GetMetadataManager().GetObjectInfo(atStringHash(sm_AuditionSoundName), info))
		{
			if(gSoundsIsOfType(info.GetType(), Sound::TYPE_ID))
			{
				AuditionSound(atStringHash(sm_AuditionSoundName));
			}
		}
	}

	if(sm_ShouldAuditionSoundOverNetwork)
	{
		Vec3V position = Vec3V(V_ZERO);
		if(CEntity *auditioningEntity = sm_ShouldAuditionThroughPlayer ? FindPlayerPed() : CDebugScene::FocusEntities_Get(0))
		{
			position = auditioningEntity->GetTransform().GetPosition();
		}
		
		CGameScriptId scriptId;
		CPlaySoundEvent::Trigger(VEC3V_TO_VECTOR3(position), atStringHash(sm_AuditionSoundSetName), atStringHash(sm_AuditionSoundName), 0xff, scriptId, 50);
	}
}

void ActivateSlowMoCB()
{
	audNorthAudioEngine::ActivateSlowMoMode(atStringHash(g_SlowMoModeName));
}

void DeactivateSlowMoCB()
{
	audNorthAudioEngine::DeactivateSlowMoMode(atStringHash(g_SlowMoModeName));
}

u32 g_CutsceneSkipTime = 0;


void SkipCutsceneCB()
{
	g_CutsceneAudioEntity.SkipCutsceneToTime(g_CutsceneSkipTime);
}

void TriggerCutsceneCB()
{
	g_CutsceneAudioEntity.TriggerCutscene();
}

void EnableMPSpeechExclusionCB()
{
	audDisplayf("Enabling NFG speech exclusion");
	audSpeechSound::SetExclusionPrefix("NFG");
}

void DisableMPSpeechExclusionCB()
{
	audDisplayf("Disabling speech exclusion");
	audSpeechSound::SetExclusionPrefix(NULL);
}

void StopCutsceneAudioCB()
{
	g_CutsceneAudioEntity.StopOverunAudio();
}

void audNorthAudioEngine::CreateWidgets()
{

	bkBank *bankPtr = BANKMGR.FindBank("Audio");
	if(bankPtr)
	{
		bankPtr->Destroy();
	}
	bkBank& bank = BANKMGR.CreateBank("Audio");

#if RSG_PS3 || RSG_ORBIS
	bank.AddToggle("PulseHeadsetSupport", &sm_ShouldTriggerPulseHeadset);
#endif

	bank.AddToggle("g_UseInteriorCarFilter", &g_UseInteriorCarFilter);

#if RSG_PC
	bank.AddToggle("Show Audio Frame Debug Info", &g_ShowAudioFrameDebugInfo);
	bank.AddSlider("Number of mix buffers", &g_NumberOfMixBuffers, 8, 16, 1);

	bank.AddToggle("Override CPU Limited Audio Setting ", &g_OverrideCPULimitedAudioSetting);
	bank.AddToggle("Force Min Spec Mode", &g_ForceMinSpecMode);
#endif	

	bank.AddButton("EnableSpeechExcl", CFA(EnableMPSpeechExclusionCB));
	bank.AddButton("DisableSpeechExcl", CFA(DisableMPSpeechExclusionCB));

	bank.AddToggle("StrVisMarkers", &sm_ShouldAddStrVisMarkers);
	bank.AddSlider("Cutscene time offset ms", &g_CutsceneTimeOffset, 0, 1000, 1);
	bank.AddSlider("Cutscene time scale", &g_CutsceneTimeScale, 0.0001f, 2.f, 0.1f);
	bank.AddButton("Stop Cutscene Audio", CFA(StopCutsceneAudioCB));
	bank.AddToggle("Use game time for cutscenes", &g_GameTimerCutscenes);

	bank.AddToggle("IgnoreFrontendVolumeSliders", &g_IgnoreFrontendVolumeSliders);
	DEV_ONLY(bank.AddToggle("AssertOnSoundPoolFull", &g_AssertOnSoundPoolFull);)
	DEV_ONLY(bank.AddToggle("PrintSoundPoolWhenFull", &g_PrintSoundPoolWhenFull);)
	bank.AddToggle("Warn on missing sounds", &g_WarnOnMissingSounds);
	bank.AddToggle("Warn on missing curves", &g_WarnOnMissingCurves);
	bank.AddToggle("Mute audio", &sm_ShouldMuteAudio);
	bank.AddToggle("Mute wind", &g_ShouldMuteWind);
	bank.AddToggle("DisableExplosionSFX", &g_DisableExplosionSFX);
	bank.AddToggle("Force SlowMo Video Editor", &sm_ForceSlowMoVideoEditor);	
	bank.AddToggle("Force Super SlowMo Video Editor", &sm_ForceSuperSlowMoVideoEditor);	
	bank.AddSlider("Slow Mo Camera Blend Threshold", &sm_ThirdPersonCameraBlendThreshold, 0.0f, 1.f, 0.01f);
	bank.AddToggle("Force Allow Radio Over Screen Fade", &g_ForceAllowRadioOverScreenFade);		

	bank.PushGroup("Sound Auditioning", true);
		bank.AddToggle("Audition through player entity", &sm_ShouldAuditionThroughPlayer);
		bank.AddToggle("Audition through focus entity", &sm_ShouldAuditionThroughFocusEntity);
		bank.AddToggle("Audition panned frontend", &sm_ShouldAuditionPannedFrontend);
		bank.AddToggle("Audition over network", &sm_ShouldAuditionSoundOverNetwork);
		bank.AddText("AuditionWaveSlot", &g_AuditionWaveSlotName[0], sizeof(g_AuditionWaveSlotName),false, &audNorthAudioEngine::AuditionWaveSlotChanged);

		bank.AddToggle("AuditionSoundsOnPPU", &sm_AuditionSoundsOnPPU);
		bank.AddToggle("AuditionSoundUnpausable", &sm_AuditionSoundUnpausable);
		bank.AddToggle("AllowAuditionSoundToLoad", &sm_AllowAuditionSoundToLoad);
		audRemoteControl::AddWidgets(bank);
		const char *nameList[] =
		{
			"Nothing",
			"Everything",
			"HierarchyOnly",
			"NoDynamicChildren",
			"VariablesOnly"
		};
		bank.AddCombo("AuditionSoundDrawMode", (s32*)&sm_AuditionSoundDrawMode, audRemoteControl::kNumDrawModes, nameList, 0);
		bank.AddSlider("VerticalScroll", &sm_DebugDrawYOffset, 0.f, 2000.f, 1.f);
	
		bank.AddText("SoundSet Name", sm_AuditionSoundSetName, sizeof(sm_AuditionSoundSetName), false);
		bank.AddText("SoundName", sm_AuditionSoundName, sizeof(sm_AuditionSoundName), false);
		bank.AddButton("Play Sound", CFA(TriggerSoundFromWidget));
		bank.AddButton("Stop Sound", CFA(StopSoundFromWidget));
	
	bank.PopGroup();

	audSoundManager::AddSoundDebuggingWidgets(bank);
	
	bank.AddToggle("Use Edited Cutscenes", &g_UseEditedCutscenes);
	bank.AddToggle("Use Master Cutscenes", &g_UseMasterCutscenes);
	bank.AddSlider("Cutscene skip time", &g_CutsceneSkipTime, 0, 90000, 10);
	bank.AddButton("Skip Cutscene", CFA(SkipCutsceneCB));
	bank.AddButton("Trigger Cutscene", CFA(TriggerCutsceneCB));
	bank.AddToggle("Debug playing script sounds", &g_DebugPlayingScriptSounds);
	bank.PushGroup("audNorthAudioEngine", false);
		bank.AddText("Sound Name", &sm_SoundName[0], sizeof(sm_SoundName),false);
		bank.AddToggle("Remove sound hierarchy", &sm_ShouldRemoveSoundHierarchy);
		bank.AddButton("Play Sound", CFA(TriggerSound));
		bank.AddButton("Stop Sound", CFA(StopSound));

		bank.AddText("SloMoModeName", g_SlowMoModeName, sizeof(g_SlowMoModeName), false);
		bank.AddButton("Activate SlowMo Mode", CFA(ActivateSlowMoCB));
		bank.AddButton("Deactivate SlowMo Mode", CFA(DeactivateSlowMoCB));
		bank.AddToggle("Display SloMo", &g_DisplaySlowMo);

#if NA_RADIO_ENABLED
		bank.AddSlider("Radio ducking volume",&g_RadioDuckingVolume, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("Radio ducking volume stereo",&g_RadioDuckingVolumeStereo, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("g_RadioDuckingVolumeVoiceChat",&g_RadioDuckingVolumeVoiceChat, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("Talk Radio ducking volume",&g_TalkRadioDuckingVolume, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("Radio gps ducking volume",&g_RadioGPSDuckingVolume, -100.0f, 0.0f, 0.1f);
		bank.AddToggle("Duck radio", &sm_ShouldDuckRadio);
#endif
		bank.AddSlider("Radio ducking smooth rate",&g_RadioDuckingSmoothRate, 0.0f, 100.0f, 0.1f);

		bank.AddToggle("ThreadedAudioUpdate", &sm_RunUpdateInSeperateThread);

		bank.AddToggle("g_MuteGameWorldAudio", &g_MuteGameWorldAudio);
		bank.AddToggle("g_MutePositionedRadio", &g_MutePositionedRadio);

		bank.AddSlider("g_FadeValueForSilence",&g_FadeValueForSilence, 0.05f, 1.0f, 0.05f);
		bank.AddToggle("g_DebugFadeLevels", &g_DebugFadeLevels);

		bank.AddSlider("g_CutsceneLeakageSmootherRate", &g_CutsceneLeakageSmootherRate, 0.0f, 1.0f, 0.0001f);
		bank.AddSlider("g_CutscenePositionedLeakage",&g_CutscenePositionedLeakage, 0.0f, 1.0f, 0.05f);
		bank.AddToggle("g_LeakForRPICutscenes", &g_LeakForRPICutscenes);
		bank.AddToggle("g_DebugCutsceneLeakage", &g_DebugCutsceneLeakage);

		bank.AddSlider("g_AttachedToTrainVolume",&g_AttachedToTrainVolume, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("SlowMoTrainVol",&g_SlowMoTrainVol, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("g_ConversationTrainVol",&g_ConversationTrainVol, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("g_ConversationHeliVol",&g_ConversationHeliVol, -100.0f, 0.0f, 0.1f);
		bank.AddSlider("g_TrainRolloffInSubways",&g_TrainRolloffInSubways, 1.0f, 10.0f, 0.1f);

		bank.PushGroup("Ambisonics");
		bank.AddButton("Save decoder", CFA(AmbisonicSave));
		bank.AddButton("Load decoder", CFA(AmbisonicLoad));
		bank.PopGroup();

		bank.PushGroup("Category Mutes");
		bank.AddButton("Mute ambience", CFA(ToggleMuteAmbience));
			bank.AddButton("Mute cutscene", datCallback(ToggleMuteCutscene));
			bank.AddButton("Mute frontend", datCallback(ToggleMuteFrontend));
			bank.AddButton("Mute music", datCallback(ToggleMuteMusic));
			bank.AddButton("Mute sfx", datCallback(ToggleMuteSfx));
			bank.AddButton("Mute speech", datCallback(ToggleMuteSpeech));
			bank.AddButton("Mute guns", datCallback(ToggleMuteGuns));
			bank.AddButton("Mute vehicles", datCallback(ToggleMuteVehicles));
			bank.PopGroup();
bank.PopGroup();

	bank.PushGroup("RAGE Audio", false);
		g_AudioEngine.AddWidgets(bank);
	bank.PopGroup();

#if USE_GUN_TAILS
	audGunFireAudioEntity::AddWidgets(bank);
#endif
#if USE_CONDUCTORS
	audSuperConductor::AddWidgets(bank);
#endif
	audObjectAudioEntity::AddWidgets(bank);
	audExplosionAudioEntity::AddWidgets(bank);
	audFireAudioEntity::AddWidgets(bank);
	audDoorAudioEntity::AddWidgets(bank);
	audVehicleAudioEntity::AddWidgets(bank);
	audCarAudioEntity::AddWidgets(bank);
	audHeliAudioEntity::AddWidgets(bank);
	audTrainAudioEntity::AddWidgets(bank);
	audPlaneAudioEntity::AddWidgets(bank);
	audBoatAudioEntity::AddWidgets(bank);
	audBicycleAudioEntity::AddWidgets(bank);
	audTrailerAudioEntity::AddWidgets(bank);
	audPedAudioEntity::AddWidgets(bank);
	audSpeechAudioEntity::AddWidgets(bank);
	audScriptAudioEntity::AddWidgets(bank);
	bank.PushGroup("Environment", false);
		bank.AddSlider("PIVMusicVolumeFadeInS", &g_PIVMusicVolumeFadeInS, 0.f, 30.f, 0.01f);
		bank.AddSlider("PIVMusicVolumeFadeOutS", &g_PIVMusicVolumeFadeOutS, 0.f, 30.f, 0.01f);
		bank.AddSlider("PIVOpennessFadeInS", &g_PIVOpennessFadeInS, 0.f, 30.f, 0.01f);
		bank.AddSlider("PIVOpennessFadeOutS", &g_PIVOpennessFadeOutS, 0.f, 30.f, 0.01f);

		naEnvironment::AddWidgets(bank);
		naEnvironmentGroupManager::AddWidgets(bank);
		naEnvironmentGroup::AddWidgets(bank);
		naOcclusionManager::AddWidgets(bank);
		naMicrophones::AddWidgets(bank);
	bank.PopGroup();
	audGlassAudioEntity::AddWidgets(bank);
	audVehicleFireAudio::AddWidgets(bank);
	NA_RADIO_ENABLED_ONLY(audRadioAudioEntity::AddWidgets(bank));
	audCollisionAudioEntity::AddWidgets(bank);
	audEmitterAudioEntity::AddWidgets(bank);
	audWeaponAudioEntity::AddWidgets(bank);
	audWeatherAudioEntity::AddWidgets(bank);
	audFrontendAudioEntity::AddWidgets(bank);
#if GTA_REPLAY
	audReplayAudioEntity::AddWidgets(bank);
#endif
	NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.AddWidgets(bank));
	g_AmbientAudioEntity.AddWidgets(bank);
	g_ReflectionsAudioEntity.AddWidgets(bank);
	g_InteractiveMusicManager.AddWidgets(bank);
	AUDCATEGORYCONTROLLERMANAGER.AddWidgets(bank);
	sm_DynamicMixer.AddWidgets(bank);
//	NA_RADIO_ENABLED_ONLY(CRadioHud::InitWidgets());
}


void audNorthAudioEngine::DebugDrawSlowMo() 
{

	if(!g_DisplaySlowMo)
	{
		return;
	}

	PUSH_DEFAULT_SCREEN();


	audNorthDebugDrawManager drawMgr(100.f, 50.f, 50.f, 720.f);

	Color32 prevColor = grcFont::GetCurrent().GetInternalColor();
	bool bOldLighting = grcLighting( false );

	grcFont::GetCurrent().SetInternalColor( Color32( 250, 250, 250 ) );

	char sectionHeader[128];
	formatf(sectionHeader, "Active SlowMo Modes");
	drawMgr.PushSection(sectionHeader);



	for(int i=0; i<NUM_SLOWMOTYPE; i++)
	{
		grcFont::GetCurrent().SetInternalColor( Color32( 250, 250, 250 ) );

		const char *slowMoName = GetMetadataManager().GetObjectName(sm_ActiveSlowMoModes[i]);

		formatf(sectionHeader, "%s: %s", SlowMoType_ToString((SlowMoType)i), slowMoName);
		drawMgr.DrawLine(sectionHeader);
	}

	drawMgr.PopSection();

	formatf(sectionHeader, "Current SlowMo Mode");
	drawMgr.PushSection(sectionHeader);
	formatf(sectionHeader, "%s: %s : %s", SlowMoType_ToString(sm_SlowMoMode), sm_SlowMoScene ? DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(sm_SlowMoScene->GetSceneSettings()->NameTableOffset): NULL, sm_SlowMoSound ? g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_SlowMoSound->GetNameTableOffset()): NULL);

	drawMgr.DrawLine(sectionHeader);

	drawMgr.PopSection();
	
	grcFont::GetCurrent().SetInternalColor( prevColor );
	grcLighting(bOldLighting);

	POP_DEFAULT_SCREEN();
}
#endif


void audGameObjectManager::OnObjectViewed(const u32 BANK_ONLY(nameHash))
{
#if __BANK
	audMetadataObjectInfo info;
	if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(nameHash,info))
	{
		if(info.GetType() == ModelAudioCollisionSettings::TYPE_ID)
		{
			ModelAudioCollisionSettings * macs = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(nameHash);
			if(macs)
			{
				g_CollisionAudioEntity.HandleRaveSelectedMaterial(macs);
			}
		}
		else if(info.GetType() == StaticEmitter::TYPE_ID)
		{
			g_RequestedWarpEmitter = info.GetObject<StaticEmitter>();			
		}
		else if(g_WarpToInterior && info.GetType() == InteriorSettings::TYPE_ID)
		{
			CInteriorProxy::Pool* proxyPool = CInteriorProxy::GetPool();
			if(proxyPool)
			{
				for(int i =0; i < proxyPool->GetSize(); i++)
				{
					CInteriorProxy* pProxy = proxyPool->GetSlot(i);
					if(pProxy && pProxy->GetNameHash() == nameHash)
					{
						g_RequestedWarpInterior = pProxy;			
					}
				}
			}
		}
		else if(info.GetType() == AmbientZone::TYPE_ID)
		{
			g_RequestedWarpAmbZone = info.GetObject<AmbientZone>();			
		}
	}
#endif
}

void audGameObjectManager::OnObjectOverridden(const u32 BANK_ONLY(nameHash))
{
#if __BANK
	audMetadataObjectInfo info;
	if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(nameHash,info))
	{
		if(info.GetType() == AmbientZone::TYPE_ID)
		{
			const AmbientZone* ambientZone = info.GetObject<AmbientZone>();
			g_AmbientAudioEntity.HandleRaveZoneCreatedNotification(const_cast<AmbientZone*>(ambientZone));
		}
		else if(info.GetType() == StaticEmitter::TYPE_ID)
		{
			g_EmitterAudioEntity.HandleRaveStaticEmitterCreatedNotification(nameHash);
		}
		else if (info.GetType() == ShoreLinePoolAudioSettings::TYPE_ID)
		{
			ShoreLinePoolAudioSettings* settings = audNorthAudioEngine::GetObject<ShoreLinePoolAudioSettings>(nameHash);
			g_AmbientAudioEntity.HandleRaveShoreLineCreatedNotification(static_cast<ShoreLineAudioSettings*>(settings));
		}
		else if (info.GetType() == ShoreLineRiverAudioSettings::TYPE_ID)
		{
			ShoreLineRiverAudioSettings* settings = audNorthAudioEngine::GetObject<ShoreLineRiverAudioSettings>(nameHash);
			g_AmbientAudioEntity.HandleRaveShoreLineCreatedNotification(static_cast<ShoreLineAudioSettings*>(settings));
		}
		else if (info.GetType() == ShoreLineOceanAudioSettings::TYPE_ID)
		{
			ShoreLineOceanAudioSettings* settings = audNorthAudioEngine::GetObject<ShoreLineOceanAudioSettings>(nameHash);
			g_AmbientAudioEntity.HandleRaveShoreLineCreatedNotification(static_cast<ShoreLineAudioSettings*>(settings));
		}
		else if (info.GetType() == ShoreLineLakeAudioSettings::TYPE_ID)
		{
			ShoreLineLakeAudioSettings* settings = audNorthAudioEngine::GetObject<ShoreLineLakeAudioSettings>(nameHash);
			g_AmbientAudioEntity.HandleRaveShoreLineCreatedNotification(static_cast<ShoreLineAudioSettings*>(settings));
		}
	}
	OnObjectEdit(nameHash);
#endif
}

void audGameObjectManager::OnObjectModified(const u32 BANK_ONLY(nameHash))
{
#if __BANK
	OnObjectEdit(nameHash);
#endif
}

void audGameObjectManager::OnObjectEdit(const u32 BANK_ONLY(nameHash))
{
#if __BANK
	audMetadataObjectInfo info;
	if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(nameHash,info))
	{
		if(info.GetType() == VehicleCollisionSettings::TYPE_ID)
		{
			audVehicleCollisionAudio::UpdateCollisionSettings();
		}
		else if(info.GetType() == CollisionMaterialSettings::TYPE_ID)
		{
			audVehicleCollisionAudio::UpdateCollisionSettings();
		}
		else if (gGameObjectsIsOfType(info.GetType(),ShoreLineAudioSettings::TYPE_ID))
		{
			g_AmbientAudioEntity.HandleRaveShoreLineEditedNotification();
		}
	}
#endif
}

void audGameObjectManager::OnObjectAuditionStart(const u32 BANK_ONLY(nameHash))
{
#if __BANK
	audMetadataObjectInfo info;
	audNorthAudioEngine::GetMetadataManager().GetObjectInfo(nameHash,info);
	if(gGameObjectsIsOfType(info.GetType(), MusicAction::TYPE_ID) || info.GetType() == MusicEvent::TYPE_ID)
	{
		g_InteractiveMusicManager.GetEventManager().TriggerEvent(info.GetName(), nameHash);
	}
#endif
}


void naAnimHandler::HandleEvent(const audAnimEvent &event,fwEntity *entity)
{
	CEntity *pEnt = (CEntity* ) entity;
	if(pEnt->GetIsTypeObject())
	{
		CObject *pObject = (CObject *)entity;
		if(pObject)
		{
			CWeapon *pWeapon = pObject->GetWeapon();
			if(pWeapon)
			{
				g_WeaponAudioEntity.AddWeaponAnimEvent(event,pObject);
			}
			else
			{
				g_EmitterAudioEntity.AddBuildingAnimEvent(event.hash, pEnt->GetTransform().GetPosition());
			}
		}
	
	}
	else if(pEnt->GetIsTypeBuilding())
	{
		g_EmitterAudioEntity.AddBuildingAnimEvent(event.hash, pEnt->GetTransform().GetPosition());
	}
}


