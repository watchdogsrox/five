#include "control/replay/replayinternal.h"

#if GTA_REPLAY

// Game includes
#include "camera/base/BaseCamera.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "core/game.h"
#include "control/gamelogic.h"
#include "audioengine/engine.h"
#include "audio/northaudioengine.h"
#include "audio/dynamicmixer.h"
#include "audio/cutsceneaudioentity.h"
#include "audio/emitteraudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/frontendaudioentity.h"
#include "audio/gameobjects.h"
#include "audio/speechmanager.h"
#include "audio/scriptaudioentity.h"
#include "audio/streamslot.h"
#include "audio/policescanner.h"
#include "audio/replayaudioentity.h"
#include "text/messages.h"
#include "vfx/particles/PtFxManager.h"
#include "Vfx/Misc/WaterCannon.h"
#include "vfx/misc/Fire.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "text/TextConversion.h"
#include "event/ShockingEvents.h"
#include "animation/animManager.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwutil/keyGen.h"
#include "game/localisation.h"
#include "game/modelIndices.h"
#include "objects/DummyObject.h"
#include "objects/ProcObjects.h"
#include "physics/physics.h"
#include "renderer/color.h"
#include "renderer/Water.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "Task/Scenario/ScenarioManager.h"
#include "streaming/IslandHopper.h"
#include "streaming/streamingengine.h"
#include "streaming/streaming.h"
#include "vehicles/vehiclefactory.h"
#include "audiohardware/driver.h"
#include "renderer/PostProcessFX.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "audioengine/controller.h"
#include "scene/world/GameWorld.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/decals/DecalManager.h"
#include "vfx/visualeffects.h"
#include "cutscene/CutSceneManagerNew.h"
#include "frontend/GameStreamMgr.h"
#include "stats/StatsTypes.h"
#include "system/controlMgr.h"
#include "renderer/ScreenshotManager.h"
#include "control/replay/Audio/AmbientAudioPacket.h"
#include "control/replay/Audio/SoundPacket.h"
#include "network/Live/livemanager.h"
#include "rline/rlgamerinfo.h"
#include "audio/music/musicplayer.h"
#include "replaycoordinator/storage/Montage.h"
#include "fwpheffects/ropemanager.h"
#include "fwpheffects/ropedatamanager.h"
#include "Vfx/Misc/DistantLights.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "script/script_hud.h"
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"

// TODO: remove when B*2048265 is implemented
#include "control/videorecording/videorecording.h"

#if __BANK
#include "debug/LightProbe.h"
#endif

// Rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "system/simpleallocator.h"
#include "grcore/image.h"
#include "grcore/texturepc.h"
#include "grcore/resourcecache.h"
#include "system/fileMgr.h"
#include "vectormath/vec3f.h"
#include "video/media_common.h"
#include "breakableglass/glassmanager.h"
#undef SAFE_RELEASE

// Framework includes
#include "fwlocalisation/templateString.h"

// Replay includes
#include "ReplayInterfacePed.h"
#include "ReplayInterfaceObj.h"
#include "ReplayInterfaceVeh.h"
#include "ReplayInterfacePickup.h"
#include "Overlay/ReplayOverlay.h"
#include "Preload/ReplayPreloader.h"
#include "Preload/ReplayPreplayer.h"
#include "Game/ReplayInterfaceGame.h"
#include "ReplayEventManager.h"
#include "Camera/ReplayInterfaceCam.h"
#include "ReplayController.h"
#include "Audio/ExplosionAudioPacket.h"
#include "Audio/BulletByAudioPacket.h"
#include "Audio/CutSceneAudioPacket.h"
#include "Audio/SpeechAudioPacket.h"
#include "Audio/MeleeAudioPacket.h"
#include "Audio/ProjectileAudioPacket.h"
#include "Ped/BonedataMap.h"
#include "audio/ReplaySoundRecorder.h"
#include "File/FileStorePC.h"
#include "ReplayMovieController.h"
#include "ReplayMovieControllerNew.h"
#include "ReplayInteriorManager.h"
#include "ReplayIPLManager.h"
#include "ReplayMeshSetManager.h"
#include "ReplayExtensions.h"
#include "ReplayRayFireManager.h"
#include "ReplayRecording.h"
#include "ReplayTrackingController.h"
#include "ReplayTrackingControllerImpl.h"
#include "ReplayHideManager.h"
#include "ReplayLightingManager.h"

// Packet includes
#include "control/replay/Ped/PedPacket.h"
#include "control/replay/Ped/PlayerPacket.h"
#include "control/replay/Entity/ObjectPacket.h"
#include "control/replay/Entity/FragmentPacket.h"
#include "control/replay/Entity/SmashablePacket.h"
#include "control/replay/Audio/FootStepPacket.h"
#include "control/replay/Audio/ScriptAudioPacket.h"
#include "control/replay/Audio/WeaponAudioPacket.h"
#include "control/replay/Audio/CollisionAudioPacket.h"
#include "control/replay/Effects/LightningFxPacket.h"
#include "control/replay/Effects/ParticlePacket.h"
#include "control/replay/Effects/ParticlePedFxPacket.h"
#include "control/replay/Effects/ParticleVehicleFxPacket.h"
#include "control/replay/Effects/ParticleFireFxPacket.h"
#include "control/replay/Effects/ParticleWeaponFxPacket.h"
#include "control/replay/Effects/ParticleBloodFxPacket.h"
#include "control/replay/Effects/ParticleEntityFxPacket.h"
#include "control/replay/Effects/ParticleMarkFxPacket.h"
#include "control/replay/Effects/ParticleMaterialFxPacket.h"
#include "control/replay/Effects/ParticleMiscFxPacket.h"
#include "control/replay/Effects/ParticleWaterFxPacket.h"
#include "control/replay/Effects/ParticleRippleFxPacket.h"
#include "control/replay/Effects/ParticleScriptedFxPacket.h"
#include "control/replay/Effects/ProjectedTexturePacket.h"
#include "control/replay/Misc/CameraPacket.h"
#include "control/replay/misc/ReplayPacket.h"
#include "control/replay/Misc/GlassPackets.h"
#include "control/replay/Misc/MoviePacket.h"
#include "control/replay/Misc/MovieTargetPacket.h"
#include "control/replay/Misc/InteriorPacket.h"
#include "control/replay/Misc/IPLPacket.h"
#include "control/replay/Misc/MeshSetPacket.h"
#include "control/replay/Misc/ReplayRayFirePacket.h"
#include "control/replay/Misc/RopePacket.h"
#include "control/replay/Misc/HidePacket.h"
#include "control/replay/Misc/LightPacket.h"
#include "control/replay/Vehicle/WheelPacket.h"
#include "control/replay/ReplayRenderTargetHelper.h"
#include "control/replay/ReplayTrailController.h"

#if USE_SRLS_IN_REPLAY
#include "streaming/streamingrequestlist.h"
#endif

//includes for the replay debug channel
#include "system/param.h"
RAGE_DEFINE_CHANNEL(replay)

#include	<ctime>

#define REPLAY_ALIGN(x, y)	(((x) + (y - 1)) & ~(y - 1))

#define DEBUG_SAVE_THREAD	(1)

// Convenience macro for accessing the Save Thread State Mgr
#define STATEMGR		sm_SaveThreadParams.GetStateMgr()

#define FAILED_IMAGE_COLOUR	CHudColour::GetRGBA(HUD_COLOUR_MENU_GREY_DARK)

#define COMPRESSSPEED( X ) (X)
#define UNCOMPRESSSPEED( X ) (X)
#define UNCOMPRESSANIMTIME( X ) (X)
#define UNCOMPRESSANIMSPEED( X ) (X)
#define UNCOMPRESSANIMBLENDAMOUNT( X ) (X)

#define COMPRESSPARTICLESPEED( X ) (MAX(-1.0f, MIN(1.0f, X )) * (120.0f))
#define UNCOMPRESSPARTICLESPEED( X ) (X * (1.0f / 120.0f))
#define COMPRESSPARTICLECOORS( X ) ( X * 4.0f )
#define UNCOMPRESSPARTICLECOORS( X ) ( X / 4.0f )

#define STREAMING_STALL_LIMIT_MS	6600
#define AUDIO_STALL_LIMIT_MS		6600

const int c_sScreenshotNameLength = 96;

extern audSpeechManager	g_SpeechManager;
extern const u8			g_MaxAmbientSpeechSlots;

extern void DrawTexturedRectangle(CSprite2d* pSprite, fwRect& oRect, Color32& color);


sysIpcThreadId			CReplayMgrInternal::sm_recordingThread[MAX_REPLAY_RECORDING_THREADS] = {sysIpcThreadIdInvalid};
sysIpcThreadId			CReplayMgrInternal::sm_quantizeThread;
sysIpcCurrentThreadId	CReplayMgrInternal::sm_recordingThreadId[MAX_REPLAY_RECORDING_THREADS] = {sysIpcCurrentThreadIdInvalid};
sysIpcSema				CReplayMgrInternal::sm_entityStoringStart[MAX_REPLAY_RECORDING_THREADS];
sysIpcSema				CReplayMgrInternal::sm_entityStoringFinished[MAX_REPLAY_RECORDING_THREADS];
sysIpcSema				CReplayMgrInternal::sm_quantizeStart;
sysIpcSema				CReplayMgrInternal::sm_recordingFinished;
sysIpcSema				CReplayMgrInternal::sm_recordingThreadEnded;
sysIpcSema				CReplayMgrInternal::sm_quantizeThreadEnded;
bool					CReplayMgrInternal::sm_wantRecordingThreaded = true;
bool					CReplayMgrInternal::sm_recordingThreaded = true;
bool					CReplayMgrInternal::sm_exitRecordingThread = false;
bool					CReplayMgrInternal::sm_recordingThreadRunning = false;
u16						CReplayMgrInternal::sm_currentSessionBlockID = 0;					 
bool					CReplayMgrInternal::sm_storingEntities = false;
int						CReplayMgrInternal::sm_wantRecordingThreadCount = 4;
int						CReplayMgrInternal::sm_recordingThreadCount = 4;

FrameRef				CReplayMgrInternal::sm_frameRefBeforeConsolidation;
CAddressInReplayBuffer	CReplayMgrInternal::sm_addressBeforeConsolidation;
CAddressInReplayBuffer	CReplayMgrInternal::sm_addressAfterConsolidation;

bool					CReplayMgrInternal::sm_lockRecordingFrameRate = true;
int						CReplayMgrInternal::sm_numRecordedFramesPerSec = 30;
bool					CReplayMgrInternal::sm_lockRecordingFrameRatePrev = true;
int						CReplayMgrInternal::sm_numRecordedFramesPerSecPrev = 30;
float					CReplayMgrInternal::sm_recordingFrameCounter = 0.0f;

// Settings Values
u32						CReplayMgrInternal::BytesPerReplayBlock			= BYTES_IN_REPLAY_BLOCK;
u16						CReplayMgrInternal::NumberOfReplayBlocks		= NUMBER_OF_REPLAY_BLOCKS;
u16						CReplayMgrInternal::NumberOfReplayBlocksFromSettings = NUMBER_OF_REPLAY_BLOCKS;
u16						CReplayMgrInternal::NumberOfTempReplayBlocks	= NUMBER_OF_TEMP_REPLAY_BLOCKS;
u16						CReplayMgrInternal::TotalNumberOfReplayBlocks	= CReplayMgrInternal::NumberOfReplayBlocks + NUMBER_OF_TEMP_REPLAY_BLOCKS;

u16						CReplayMgrInternal::TempBlockIndex				= CReplayMgrInternal::NumberOfReplayBlocks;
bool					CReplayMgrInternal::SettingsChanged				= false;
s32						CReplayMgrInternal::sm_CutSceneAudioSync		= 0;

IReplayPlaybackController* CReplayMgrInternal::sm_pPlaybackController = NULL;
s16						CReplayMgrInternal::sm_iCurrentSoundBaseId	= 0;
s16						CReplayMgrInternal::sm_iSoundInstanceID		= 0;

atSNode<CPreparingSound>	CReplayMgrInternal::sm_aPreparingSounds[MAX_PREPARING_SOUNDS];
atSList<CPreparingSound>	CReplayMgrInternal::sm_lPreparingSounds;

CBufferInfo				CReplayMgrInternal::sm_ReplayBufferInfo;
u32						CReplayMgrInternal::sm_BlockSize		= 0;

atString				CReplayMgrInternal::sm_currentMissionName = atString("");
atString				CReplayMgrInternal::sm_currentMontageFilter = atString("");
s32						CReplayMgrInternal::sm_currentMissionIndex = -1;

u32						CReplayMgrInternal::sm_previousFrameHash = 0;
u32						CReplayMgrInternal::sm_currentFrameHash = 0;

#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
CReplayMgrInternal::eScriptSavePrepState	CReplayMgrInternal::sm_ScriptSavePreparationState = CReplayMgrInternal::SCRIPT_SAVE_PREP_WAITING_FOR_RESPONSE_FROM_SCRIPT;
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE

sysTimer				CReplayMgrInternal::sm_scriptWaitTimer;

bool					CReplayMgrInternal::sm_waitingForScriptCleanup = false;
bool					CReplayMgrInternal::sm_scriptsHaveCleanedUp = false;
bool					CReplayMgrInternal::sm_bScriptsHavePausedTheClearWorldTimeout = false;

bool					CReplayMgrInternal::sm_IsMemoryAllocated = false;
bool			 		CReplayMgrInternal::sm_bFileRequestMemoryAlloc = false;

CAddressInReplayBuffer	CReplayMgrInternal::sm_oRecord;
CAddressInReplayBuffer	CReplayMgrInternal::sm_oPlayback;
CAddressInReplayBuffer	CReplayMgrInternal::sm_oPlaybackStart;
CAddressInReplayBuffer	CReplayMgrInternal::sm_oHistoryProcessPlayback;

bool					CReplayMgrInternal::sm_wantToRecordEvents = true;
bool					CReplayMgrInternal::sm_recordEvents = true;

bool					CReplayMgrInternal::sm_wantToMonitorEvents = true;
bool					CReplayMgrInternal::sm_monitorEvents = true;
bool					CReplayMgrInternal::sm_forcePortalScanOnClipLoad = false;
bool					CReplayMgrInternal::sm_IsEditorActive = false;
bool					CReplayMgrInternal::sm_wantBufferSwap = false;

CPacketFrame*			CReplayMgrInternal::sm_pPreviousFramePacket = NULL;

float					CReplayMgrInternal::sm_recordingBufferProgress = 0.0f;

sysCriticalSectionToken CReplayMgrInternal::sm_PacketRecordCriticalSection;

atArray<audCachedSoundPacket> CReplayMgrInternal::sm_CachedSoundPackets;

// Interpolation stuff
u32						CReplayMgrInternal::sm_uPlaybackStartTime;
bool					CReplayMgrInternal::sm_bShouldSetStartTime;
u32						CReplayMgrInternal::sm_uPlaybackEndTime;
float					CReplayMgrInternal::sm_fInterpCurrentTime;

u32						CReplayMgrInternal::sm_uTimePlaybackBegan = 0;

float					CReplayMgrInternal::sm_fLastInterpValue;

CFrameInfo				CReplayMgrInternal::sm_currentFrameInfo;
CFrameInfo				CReplayMgrInternal::sm_nextFrameInfo;
CFrameInfo				CReplayMgrInternal::sm_prevFrameInfo;
CFrameInfo				CReplayMgrInternal::sm_firstFrameInfo;
CFrameInfo				CReplayMgrInternal::sm_lastFrameInfo;

CFrameInfo				CReplayMgrInternal::sm_eventJumpCurrentFrameInfo;
CFrameInfo				CReplayMgrInternal::sm_eventJumpNextFrameInfo;
CFrameInfo				CReplayMgrInternal::sm_eventJumpPrevFrameInfo;

// Playback times
float					CReplayMgrInternal::sm_playbackFrameStepDelta = 0.0f;
float					CReplayMgrInternal::sm_exportFrameStep = 0.0f;
u64						CReplayMgrInternal::sm_exportTotalNs = 0;
u64						CReplayMgrInternal::sm_exportFrameCount = 0;
float					CReplayMgrInternal::sm_fTimeScale = 1.0f;
float					CReplayMgrInternal::sm_playbackSpeed = 1.0f;

bool					CReplayMgrInternal::sm_shouldUpdateScripts = false;
bool					CReplayMgrInternal::sm_fixedTimeExport = false;

s32						CReplayMgrInternal::sm_playerIntProxyIndex = -1;

eReplayMode				CReplayMgrInternal::sm_uMode			= REPLAYMODE_DISABLED;
eReplayMode				CReplayMgrInternal::sm_desiredMode		= REPLAYMODE_DISABLED;

// Temp values to get going
Matrix34				CReplayMgrInternal::sm_oCamMatrix;
bool					CReplayMgrInternal::sm_bCameraInSniperMode;
bool					CReplayMgrInternal::sm_bFinishPlaybackPending;
bool					CReplayMgrInternal::sm_bUserCancel		= false;
u16						CReplayMgrInternal::sm_uSniperModelIndex;
bool					CReplayMgrInternal::sm_KickPreloader = false;
bool					CReplayMgrInternal::sm_bJumpPreloaderKicked = false;
bool					CReplayMgrInternal::sm_bPreloaderResetFwd = false;
bool					CReplayMgrInternal::sm_bPreloaderResetBack = false;
bool					CReplayMgrInternal::sm_IgnorePostLoadStateChange = false;
bool					CReplayMgrInternal::sm_KickPrecaching = false;
bool					CReplayMgrInternal::sm_WantsToKickPrecaching = false;
bool					CReplayMgrInternal::sm_doInitialPrecache = false;
bool					CReplayMgrInternal::sm_OverwatchEnabled = REPLAY_OVERWATCH;
bool					CReplayMgrInternal::sm_ShouldOverwatchBeEnabled = REPLAY_OVERWATCH;
bool					CReplayMgrInternal::sm_ReplayInControlOfWorld = false;
bool					CReplayMgrInternal::sm_ClearingWorldForReplay = false;
bool					CReplayMgrInternal::sm_PokeGameSystems = false;
bool					CReplayMgrInternal::sm_NeedEventJump = false;
u32						CReplayMgrInternal::sm_JumpToEventsTime = 0;
bool					CReplayMgrInternal::sm_DoFullProcessPlayback = false;

atString				CReplayMgrInternal::sm_fileToLoad;
atString				CReplayMgrInternal::sm_lastLoadedClip;

Matrix34				CReplayMgrInternal::sm_SecondaryListenerMatrix;
Vector3					CReplayMgrInternal::sm_LocalEnvironmentScanPosition( 0.0f, 0.0f, 0.0f );
f32						CReplayMgrInternal::sm_SecondaryListenerContribution	= 0.0f;
f32						CReplayMgrInternal::sm_AudioSceneStopTimer = 0.0f;
f32						CReplayMgrInternal::sm_AudioSceneStopDelay = 0.1f;
/*TODO4FIVE CCamTypes::eCamType	CReplayMgrInternal::sm_CameraType						= CCamTypes::CAM_TYPE_BAD;*/
bool					CReplayMgrInternal::sm_bIsLocalEnvironmentPositionFixed	= false;

bool					CReplayMgrInternal::sm_bDisableLumAdjust = false;
bool					CReplayMgrInternal::sm_bMultiplayerClip = false;

//Game timer values to save and restore between clips
u32						CReplayMgrInternal::m_CamTimerTimeInMilliseconds = 0;
float					CReplayMgrInternal::m_CamTimerTimeStepInSeconds = 0.0f;
u32						CReplayMgrInternal::m_GameTimerTimeInMilliseconds = 0;
float					CReplayMgrInternal::m_GameTimerTimeStepInSeconds = 0.0f;
u32						CReplayMgrInternal::m_ScaledNonClippedTimerTimeInMilliseconds = 0;
float					CReplayMgrInternal::m_ScaledNonClippedTimerTimeStepInSeconds = 0.0f;
u32						CReplayMgrInternal::m_NonScaledClippedTimerTimeInMilliseconds = 0;
float					CReplayMgrInternal::m_NonScaledClippedTimerTimeStepInSeconds = 0.0f;
u32						CReplayMgrInternal::m_ReplayTimerTimeInMilliseconds = 0;
float					CReplayMgrInternal::m_ReplayTimerTimeStepInSeconds = 0.0f;
bool					CReplayMgrInternal::m_ReplayTimersStored = false;

s32						CReplayMgrInternal::sm_sNumberOfRealObjectsRequested = -1;
u32						CReplayMgrInternal::sm_uStreamingStallTimer = 0;
u32						CReplayMgrInternal::sm_uAudioStallTimer = 0;
u32						CReplayMgrInternal::sm_uStreamingSettleCount = 0;
bool					CReplayMgrInternal::sm_bReplayEnabled = false;

char					CReplayMgrInternal::sm_sReplayFileName[REPLAYPATHLENGTH];

u32						CReplayMgrInternal::sm_uLastPlaybackTime;

bool					CReplayMgrInternal::sm_bSaveReplay;
eDelaySaveState			CReplayMgrInternal::sm_eDelayedSaveState = REPLAY_DELAYEDSAVE_DISABLED;
bool					CReplayMgrInternal::sm_bIsDead = false;

bool					CReplayMgrInternal::sm_bScriptWantsReplayDisabled = false;
s32						CReplayMgrInternal::sm_uCurrentFrameInBlock = -1;

bool					CReplayMgrInternal::sm_bWasPreviouslyRewindingOrFastForwarding = false;

bool					CReplayMgrInternal::sm_overrideWeatherPlayback = false;
bool					CReplayMgrInternal::sm_overrideTimePlayback = false;

naAudioEntity			CReplayMgrInternal::s_AudioEntity;
audScene*				CReplayMgrInternal::sm_ffwdAudioScene = NULL;
audScene*				CReplayMgrInternal::sm_rwdAudioScene = NULL;
bool					CReplayMgrInternal::sm_bCheckForPreviewExit = false;
bool					CReplayMgrInternal::sm_bIsLastClipFinished = false;
bool					CReplayMgrInternal::sm_bInRelease		    = false;
s32                 	CReplayMgrInternal::sm_MontageReleaseTime     = 0;
s32                 	CReplayMgrInternal::sm_SimpleReleaseStopTime  = 0;
s32                 	CReplayMgrInternal::sm_SimpleReleaseStartTime = 0;

CPed*					CReplayMgrInternal::sm_pMainPlayer = NULL;
bool					CReplayMgrInternal::sm_reInitAudioEntity = false;

bool					CReplayMgrInternal::sm_IsMarkedClip = false;
bool					CReplayMgrInternal::sm_doSave    = false;
u32						CReplayMgrInternal::sm_postSaveFrameCount = 0;

rage::atMap<s32, PTFX_ReplayStreamingRequest> CReplayMgrInternal::sm_StreamingRequests;

s32                     CReplayMgrInternal::sm_MusicDuration = 0;
s32						CReplayMgrInternal::sm_MusicOffsetTime = 0;
s32						CReplayMgrInternal::sm_sMusicStartCountDown = 0;

sysIpcSema				CReplayMgrInternal::sm_LoadOpWaitForMem = sysIpcCreateSema(0);
atString				CReplayMgrInternal::sm_Filepath = atString("");

bool					(*CReplayMgrInternal::sm_pSaveResultCB)(bool) = NULL;
MarkerBufferInfo		CReplayMgrInternal::markerBufferInfo;

float					CReplayMgrInternal::sm_fExtraBlockRemotePlayerRecordingDistance = 15.0f;

sysTimer				CReplayMgrInternal::sm_oReplayFullFrameTimer;
sysTimer				CReplayMgrInternal::sm_oReplayInterpFrameTimer;

CReplayState			CReplayMgrInternal::sm_eReplayState;
CReplayState			CReplayMgrInternal::sm_eLastReplayState;
CReplayState			CReplayMgrInternal::sm_nextReplayState;
eReplayLoadingState		CReplayMgrInternal::sm_eLoadingState = REPLAYLOADING_NONE;
bool					CReplayMgrInternal::sm_isSettingUp = false;
bool					CReplayMgrInternal::sm_isSettingUpFirstFrame = false;
bool					CReplayMgrInternal::sm_allEntitiesSetUp = false;
u32						CReplayMgrInternal::sm_JumpPrepareState = eNoPreload;
bool					CReplayMgrInternal::sm_allowBlockingLoading = false;

bool					CReplayMgrInternal::sm_wantPause = false;
bool					CReplayMgrInternal::sm_canPause = false;

bool					CReplayMgrInternal::sm_wantQuit = false;

CReplayFrameData		CReplayMgrInternal::sm_replayFrameData;
CReplayClipData			CReplayMgrInternal::sm_replayClipData;
bool					CReplayMgrInternal::sm_useRecordedCamera = true;
int						CReplayMgrInternal::sm_SuppressCameraMovementCameraFrameCount = -1;
bool					CReplayMgrInternal::sm_SuppressCameraMovementThisFrame = false;
bool					CReplayMgrInternal::sm_SuppressCameraMovementLastFrame = false;

bool					CReplayMgrInternal::sm_SuppressEventsThisFrame = false;
bool					CReplayMgrInternal::sm_SuppressEventsLastFrame = false;
u32						CReplayMgrInternal::sm_FrameTimeEventsWereSuppressed = 0;

bool					CReplayMgrInternal::sm_EnableEventsThisFrame = false;
int						CReplayMgrInternal::sm_EnableEventsCooldown = 0;
bool					CReplayMgrInternal::sm_EnableEventsLastFrame = false;
u32						CReplayMgrInternal::sm_FrameTimeScriptEventsWereDisabled = 0;	// Never enabled


eReplayFileErrorCode	CReplayMgrInternal::sm_LastErrorCode = REPLAY_ERROR_SUCCESS;
bool					CReplayMgrInternal::sm_wasModifiedContent = false;
bool					CReplayMgrInternal::sm_PreviouslySuppressedReplay = false;
u32						CReplayMgrInternal::sm_CurrentTimeInMilliseconds = 0;
s32						CReplayMgrInternal::sm_SuppressRefCount = 0;
bool					CReplayMgrInternal::sm_ClipCompressionEnabled = REPLAY_COMPRESSION_ENABLED;
atArray<CBlockProxy>	CReplayMgrInternal::sm_BlocksToSave;
s16						CReplayMgrInternal::sm_ShouldSustainRecordingLine = -1;
CSavegameQueuedOperation_LoadReplayClip CReplayMgrInternal::sm_ReplayLoadClipStructure;

atMap<u32, CBuilding*>	CReplayMgrInternal::sm_BuildingHashes;
bool					CReplayMgrInternal::sm_vinewoodIMAPSsForceLoaded = false;
bool					CReplayMgrInternal::sm_shouldLoadReplacementCasinoImaps = false;
atHashString			CReplayMgrInternal::sm_islandActivated = ATSTRINGHASH("", 0);
/*TODO4FIVE #if __WIN32PC && __SPOTCHECK
bool				CReplayMgrInternal::sm_bSpotCheck = true;
#endif*/

// IMAPs to load in pre-Vinewood update clips
static char const* vinewoodIMAPsToForceLoad[] = { "hei_dlc_windows_casino",
												"hei_vw_dlc_casino_door_replay"};


#if USE_SAVE_SYSTEM
CSavegameQueuedOperation_ReplayLoad CReplayMgrInternal::sm_ReplayLoadStructure;
CSavegameQueuedOperation_ReplaySave CReplayMgrInternal::sm_ReplaySaveStructure;

enum eReplaySaveState
{
	REPLAY_SAVE_NOT_STARTED,
#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	REPLAY_SAVE_BEGIN_WAITING_FOR_SCRIPT_TO_PREPARE,
	REPLAY_SAVE_WAITING_FOR_SCRIPT_TO_PREPARE,
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	REPLAY_SAVE_IN_PROGRESS,
	REPLAY_SAVE_SUCCEEDED,
#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE && DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS
	REPLAY_SAVE_FAILED_DISPLAY_WARNING_MESSAGE,
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE && DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS
	REPLAY_SAVE_FAILED
};

static eReplaySaveState g_ReplaySaveState = REPLAY_SAVE_NOT_STARTED;
#else	//	USE_SAVE_SYSTEM
static bool g_bWorldHasBeenCleared = false;
#endif	//	USE_SAVE_SYSTEM

#if __BANK
static float s_fFramesElapsed;
static float s_fSecondsElapsed;

static u32	s_currentReplayBlockPos = 0;
static u32	s_currentReplayBlock = 0;
static float s_currentProgressPercentage = 0;
#if DO_REPLAY_OUTPUT_XML
static char s_ExportReplayFilename[REPLAYPATHLENGTH];
#endif // REPLAY_DEBUG && RSG_PC
static char s_ReplayIDToHighlight[16];
volatile static bool	s_ExportReplayRunning = false;
static sysIpcSema s_ExportReplayWait = sysIpcCreateSema(0);
static float s_customDeltaSettingMS = 1.0f;
static s32	s_sNumberOfPedsBeforeReplay;
static s32	s_sNumberOfPedsDuringReplay;
static s32	s_sNumberOfPedsWaitingToDel;
static s32	s_sNumberOfVehiclesBeforeReplay;
static s32	s_sNumberOfVehiclesDuringReplay;
static s32	s_sNumberOfVehiclesWaitingToDel;
static s32	s_sNumberOfObjectsBeforeReplay;
static s32	s_sNumberOfObjectsDuringReplay;
static s32	s_sNumberOfObjectsWaitingToDel;
static s32	s_sNumberOfDummyObjectsBeforeReplay;
static s32	s_sNumberOfDummyObjectsDuringReplay;
static s32	s_sNumberOfNodesBeforeReplay;
static s32	s_sNumberOfNodesDuringReplay;

static s32	s_sNumberOfParticleEffectsAvailable;
static u32	s_trackablesCount;

char					CReplayMgrInternal::sm_customReplayPath[RAGE_MAX_PATH] = "X:\\Replay\\";
bool					CReplayMgrInternal::sm_bUseCustomWorkingDir = false;
atString				CReplayMgrInternal::sm_PreventCallstack;
atString				CReplayMgrInternal::sm_SuppressCameraMovementCallstack;
bool					CReplayMgrInternal::sm_SuppressCameraMovementByScript = false;
bool					CReplayMgrInternal::s_bTurnOnDebugBones = false;
u16						CReplayMgrInternal::s_uBoneId = 0xffff;

bool					CReplayMgrInternal::sm_bUpdateSkeletonsEnabledInRag = false;
bool					CReplayMgrInternal::sm_bQuantizationOptimizationEnabledInRag = true;

#endif // __BANK

#if __BANK
static char s_Filename[REPLAYFILELENGTH];

bool					CReplayMgrInternal::sm_bRecordingEnabledInRag = false;
int						CReplayMgrInternal::sm_ClipIndex = 0;

bkText*					CReplayMgrInternal::sm_ReplayClipNameText = NULL;
bkCombo*				CReplayMgrInternal::sm_ReplayClipNameCombo = NULL;
bkSlider*				CReplayMgrInternal::sm_replayBlockPosSlider = NULL;
bkSlider*				CReplayMgrInternal::sm_replayBlockSlider = NULL;
#endif // __BANK

CReplayInterfaceManager	CReplayMgrInternal::sm_interfaceManager;
CReplayEventManager		CReplayMgrInternal::sm_eventManager;
CReplayAdvanceReader*	CReplayMgrInternal::sm_pAdvanceReader	= NULL;
ReplayFileManager		CReplayMgrInternal::sm_FileManager;

bool					CReplayMgrInternal::sm_TriggerPlayback = false;

#if !__FINAL
PARAM(fixedDevPlayback, "Playback the replay in fixed time to help with Asserts throwing off time");
PARAM(disableOverwatch, "Disable Overwatch");
PARAM(setRecordingFramerate, "Set the recording framerate");
PARAM(noRecordingFrameLock, "Disable the recording frame lock");
#endif

//#define DUMP_AUDIO_TO_FILE_WITH_RENDER
#ifdef DUMP_AUDIO_TO_FILE_WITH_RENDER
HANDLE g_hFile;
#endif

//new cursor control functionality
float					CReplayMgrInternal::m_TimeToJumpToMS	= 0;
float					CReplayMgrInternal::m_jumpDelta			= 0;
float					CReplayMgrInternal::m_cursorSpeed		= 1.0f;
float					CReplayMgrInternal::m_markerSpeed		= 1.0f;
bool					CReplayMgrInternal::m_ForceUpdateRopeSim= false; 
bool					CReplayMgrInternal::m_ForceUpdateRopeSimWhenJumping= false; 
bool					CReplayMgrInternal::m_FineScrubbing = false; 
bool					CReplayMgrInternal::sm_isWithinEnvelope = false;

bool					CReplayMgrInternal::m_WantsReactiveRenderer = false;
s32						CReplayMgrInternal::m_FramesTillReactiveRenderer = 0;

bool					CReplayMgrInternal::sm_IsBeingDeleteByReplay = false;

bool					CReplayMgrInternal::sm_haveFrozenShadowSetting = false;
bool					CReplayMgrInternal::sm_shadowsFrozenInGame = false;
bool					CReplayMgrInternal::sm_padReplayToggle = false;

// Callbacks
CReplayMgrInternal::replayCallback	CReplayMgrInternal::m_onPlaybackSetup	= NULL;

replaySoundRecorder replaySndRecorder;
replayGlassManager replayGlassManager;

//enable automatic replay recording
PARAM(consolepadreplaytoggle, "Enable recording/saving via the four back buttons of a pad on console");

#include "script/script.h"			// CTheScripts

void PTFX_ReplayStreamingRequest::Load()
{
	s32 flags = STRFLAG_MISSION_REQUIRED|m_StreamingFlags;
	CTheScripts::PrioritizeRequestsFromMissionScript(flags);
	m_StreamingModule->StreamingRequest(strLocalIndex(m_Index), flags);
}

void PTFX_ReplayStreamingRequest::Destroy()
{
	CStreaming::SetMissionDoesntRequireObject(strLocalIndex(m_Index), m_StreamingModule->GetStreamingModuleId());
}

//========================================================================================================================
void CReplayMgrInternal::InitSession(unsigned initMode)
{
	Init(initMode);
	CReplayBonedataMap::Init();

	sm_monitorEvents = sm_wantToMonitorEvents = true;

#if !__FINAL
	if(PARAM_setRecordingFramerate.Get(sm_numRecordedFramesPerSec))
	{
		replayDebugf1("Recording locked to %d frames per second", sm_numRecordedFramesPerSec);
	}

	sm_lockRecordingFrameRate = !PARAM_noRecordingFrameLock.Get();
	replayDebugf1("Recording frame lock enabled = %s", sm_lockRecordingFrameRate ? "Yes" : "No");
#endif // !__FINAL

	rage::audRequestedSettings::SetRecorder(&replaySndRecorder);
	rage::glassRecordingRage::Set(&replayGlassManager);
}

//========================================================================================================================
void CReplayMgrInternal::ShutdownSession(unsigned mode)
{
	CheckAllThreadsAreClear();

	CGenericGameStorage::ms_PushThroughReplaySave = false;

	sm_OverwatchEnabled = false;

	if(sm_uMode == REPLAYMODE_RECORD)
	{
		DisableRecording();
		sm_uMode = REPLAYMODE_DISABLED;
	}

	sm_interfaceManager.DeregisterAllCurrentEntities();

	Disable();

	CReplayExtensionManager::Cleanup();

	sm_interfaceManager.ReleaseAndClear();
	sm_eventManager.ReleaseAndClear();

	sm_pAdvanceReader->Clear();

	sm_BlocksToSave.clear();

	//this may need removing for shutdown
	s_AudioEntity.Shutdown();

	sm_FileManager.Shutdown();

	FreeMemory();

	//If the last clip to run was a multiplayer clip shutdown to multiplayer audio data
	if( sm_bMultiplayerClip )
	{
		audNorthAudioEngine::NotifyCloseNetwork(true);
		sm_bMultiplayerClip = false;
	}

	// Return to cloth on mutiple threads.
	CPhysics::GetClothManager()->SetDontDoClothOnSeparateThread(false);
	CRenderer::SetDisableArtificialLights(false);
	sm_ReplayInControlOfWorld = false;
	sm_BuildingHashes.Reset();
	ASSERT_ONLY(rage::SetNoPlacementAssert(false));

	if(mode == SHUTDOWN_CORE)
	{
		// Set the exit recording thread flag and kick the recording thread
		sm_exitRecordingThread = true;
		for (unsigned int i = 0; i < MAX_REPLAY_RECORDING_THREADS; i++)
			sysIpcSignalSema(sm_entityStoringStart[i]);
		sysIpcSignalSema(sm_quantizeStart);

		// Wait for the thread ended sema and then wait for the thread to exit
		sysIpcWaitSema(sm_recordingThreadEnded, MAX_REPLAY_RECORDING_THREADS);
		sysIpcWaitSema(sm_quantizeThreadEnded);

		for (unsigned int i = 0; i < MAX_REPLAY_RECORDING_THREADS; i++)
			sysIpcWaitThreadExit(sm_recordingThread[i]);
		sysIpcWaitThreadExit(sm_quantizeThread);

		// Delete all the semas
		for (unsigned int i = 0; i < MAX_REPLAY_RECORDING_THREADS; i++)
		{
			sysIpcDeleteSema(sm_entityStoringStart[i]);
			sysIpcDeleteSema(sm_entityStoringFinished[i]);
		}
		sysIpcDeleteSema(sm_quantizeStart);
		sysIpcDeleteSema(sm_recordingFinished);
		sysIpcDeleteSema(sm_recordingThreadEnded);
		sysIpcDeleteSema(sm_quantizeThreadEnded);

		sm_pAdvanceReader->Shutdown();
		delete sm_pAdvanceReader;
		sm_pAdvanceReader = NULL;

		CReplayTrailController::GetInstance().Shutdown();

#if REPLAY_RECORD_SAFETY
		CReplayRecorder::FreeScratch();
#endif // REPLAY_RECORD_SAFETY
	}
}

//========================================================================================================================
void CReplayMgrInternal::SetTempBuffer()
{
	if( sm_ReplayBufferInfo.IsAnyTempBlockOn() || sm_oRecord.GetBlock()->IsTempBlock() )
	{
		return;
	}

	PacketRecordWait();

	replayDebugf1("SetTemp buffer...");

	// Make sure the buffers are empty before starting to dump into it
	//replayAssert(sm_ReplayBufferInfo.GetBlockStatus(TempBlockIndex) == REPLAYBLOCKSTATUS_EMPTY);

	// Copy the data from the current block into the first temp block...
	CBlockInfo* pTempBlock = sm_ReplayBufferInfo.GetFirstTempBlock();
	CBlockInfo* pCurrBlock = sm_oRecord.GetBlock();

	pTempBlock->LockForWrite();
	pCurrBlock->LockForRead();

	pTempBlock->CopyFrom(*pCurrBlock);
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	pTempBlock->SetThumbnailRef(pCurrBlock->GetThumbnailRef());
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
	pTempBlock->SetTimeSpan(pCurrBlock->GetTimeSpan());
	pTempBlock->SetStartTime(pCurrBlock->GetStartTime());
	pTempBlock->SetSizeLost(pCurrBlock->GetSizeLost());
	pTempBlock->SetSizeUsed(pCurrBlock->GetSizeUsed());
	pTempBlock->SetFrameCount(pCurrBlock->GetFrameCount());
	pTempBlock->SetStatus(pCurrBlock->GetStatus());
	pTempBlock->SetSessionBlockIndex(pCurrBlock->GetSessionBlockIndex());
	pTempBlock->SetNormalBlockEquiv(pCurrBlock);

#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	pTempBlock->GenerateThumbnail();
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

	// All the 'latest update' and 'create' packet pointers are pointing to the main block we just
	// duplicated so we have to offset them into the temporary block we just copied into.
	s64 offset = pTempBlock->GetData() - pCurrBlock->GetData();
	sm_interfaceManager.OffsetCreatePackets(offset);
	sm_interfaceManager.OffsetUpdatePackets(offset, pCurrBlock->GetData(), pCurrBlock->GetBlockSize());
	
	pCurrBlock->UnlockBlock();
	pTempBlock->UnlockBlock();

	// Set the recording address to the temp buffer
	sm_oRecord.SetBlock(pTempBlock);
	sm_oRecord.SetPacket(CPacketEnd());

	replayAssert(sm_oRecord.GetBlock()->GetStatus() == REPLAYBLOCKSTATUS_BEINGFILLED);

	replayDebugf3("Set Block %p, %u", pTempBlock, pTempBlock->GetMemoryBlockIndex());
	for (int i = 0; i < sm_BlocksToSave.GetCount(); ++i )
	{
		sm_BlocksToSave[i].SetSaving();
	}

	sm_wantBufferSwap = true;

	s64 dataOffset = pTempBlock->GetData() - pCurrBlock->GetData();
	sm_pPreviousFramePacket = (CPacketFrame*)((u8*)sm_pPreviousFramePacket + dataOffset);
	sm_pPreviousFramePacket->ValidatePacket();

	PacketRecordSignal();

	//sm_ReplayBufferInfo.SetBlockStatus(tempBlockIndex, REPLAYBLOCKSTATUS_BEINGFILLED);
	//sm_interfaceManager.ResetCreatePacketsForCurrentBlock();
}


//////////////////////////////////////////////////////////////////////////
int CReplayMgrInternal::SetupBlock(CBlockInfo& block, void*)
{
	REPLAY_CHECK(sm_BlockSize > 0, false, "Block size is 0");
	const u16 aligment = 16;

	// allocate the memory for each block out of the streaming pool
	void* blockMemory = sysMemManager::GetInstance().GetReplayAllocator()->Allocate(sm_BlockSize, aligment);
	REPLAY_CHECK(blockMemory != NULL, false, "Failed to allocated memory for replay block, size %u!", sm_BlockSize);
	
	sm_IsMemoryAllocated = true;

	block.SetData((u8*)blockMemory, sm_BlockSize);

	return 0;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::AllocateMemory(u16 maxBlocksCount, u32 blockSize)
{
	REPLAYTRACE("*CReplayMgrInternal::AllocateMemory - Block Count:%u, BlockSize:%u", maxBlocksCount, blockSize);

	replayFatalAssertf(sm_IsMemoryAllocated == false, "Trying to allocate memory when we've not deallocated");
	replayFatalAssertf(_IsPowerOfTwo((int)blockSize),
						"The block size %i,is not a power of 2, this will cause the replay to use much more memory due to how the streaming allocations work", 
						blockSize);

	sm_ReplayBufferInfo.SetNumberOfBlocksAllocated(maxBlocksCount);
	sm_BlockSize = blockSize;

	// Setup all the blocks...
	int result = sm_ReplayBufferInfo.ForeachBlockReverse(SetupBlock);
	return result == 0;
}


//////////////////////////////////////////////////////////////////////////
int CReplayMgrInternal::ShutdownBlock(CBlockInfo& block, void*)
{
	if(block.GetData() != NULL)
		sysMemManager::GetInstance().GetReplayAllocator()->Free((void*)block.GetData());

	block.SetData(NULL, 0);
	block.SetStatus(REPLAYBLOCKSTATUS_EMPTY);

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::FreeMemory()
{
	if(!sm_IsMemoryAllocated)
		return;

	REPLAYTRACE("*CReplayMgrInternal::FreeMemory");
	
	// Ensure any preloading requests are gone at this point as the memory they will
	// point to is about to be nuked.
	CheckAllThreadsAreClear();

	// Shutdown all the blocks
	sm_ReplayBufferInfo.ForeachBlock(ShutdownBlock);
	
	sm_ReplayBufferInfo.SetNumberOfBlocksAllocated(0);
	sm_oRecord.Reset();
	sm_oPlayback.Reset();
	sm_oPlaybackStart.Reset();
	sm_IsMemoryAllocated = false;

	// NULL this pointer out otherwise it'll still be pointing to old data.
	sm_pPreviousFramePacket = NULL;

	// Reset these as they all hold onto pointers into the free replay buffer
	sm_currentFrameInfo.Reset();
	sm_nextFrameInfo.Reset();
	sm_prevFrameInfo.Reset();
	sm_firstFrameInfo.Reset();
	sm_lastFrameInfo.Reset();

	sm_eventJumpCurrentFrameInfo.Reset();
	sm_eventJumpNextFrameInfo.Reset();
	sm_eventJumpPrevFrameInfo.Reset();
	sm_oHistoryProcessPlayback.Reset();
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::SetupReplayBuffer(u16 normalBlockCount, u16 tempBlockCount)
{
	REPLAYTRACE("*CReplayMgrInternal::SetupReplayBuffer - Block Count:%u, Temp Count:%u, BlockSize:%u", normalBlockCount, tempBlockCount, BYTES_IN_REPLAY_BLOCK);

	const u16 totalNumberOfBlocks = normalBlockCount + tempBlockCount;

	// If the number of blocks we require is greater than what we have allocated then
	// attempt to allocate more
	if(totalNumberOfBlocks > sm_ReplayBufferInfo.GetNumberOfBlocksAllocated())
	{
		FreeMemory();
		if(!AllocateMemory(totalNumberOfBlocks, BYTES_IN_REPLAY_BLOCK))
		{
			FreeMemory();
			return false;
		}
	}

	if(sm_ReplayBufferInfo.GetNumberOfBlocksAllocated() > 0)
	{
		sm_ReplayBufferInfo.Reset();
		sm_ReplayBufferInfo.SetBlockCount(normalBlockCount, tempBlockCount);

		sm_oRecord.SetBlock(sm_ReplayBufferInfo.GetFirstBlock());
		sm_oRecord.PrepareForRecording();
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::CheckAllThreadsAreClear()
{
#if DO_REPLAY_OUTPUT
	u32 time = fwTimer::GetSystemTimeInMilliseconds();
#endif // DO_REPLAY_OUTPUT

	sm_pAdvanceReader->WaitForAllScanners();
	
	WaitForEntityStoring();
	WaitForRecordingFinish();
#if __BANK
	WaitForReplayExportToFinish();
#endif // __BANK

	replayDebugf1("Waited in CReplayMgrInternal::CheckAllThreadsAreClear for %u ms", fwTimer::GetSystemTimeInMilliseconds() - time);

	return true;
}


//========================================================================================================================
void CReplayMgrInternal::Init(unsigned initMode, bool bSaveThread)
{
	REPLAYTRACE("*CReplayMgrInternal::Init");
	if (!bSaveThread && IsSaving())
	{
		// Do not touch the buffers while we're dumping them
		return;
	}

	if(initMode == INIT_CORE)
	{
#if REPLAY_RECORD_SAFETY
		CReplayRecorder::AllocateScratch();
#endif // REPLAY_RECORD_SAFETY

		CBlockProxy::SetBuffer(sm_ReplayBufferInfo);

		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::sm_debugMessages.Reserve(16);)

		for (unsigned int i = 0; i < MAX_REPLAY_RECORDING_THREADS; i++)
		{
			sm_entityStoringStart[i]	= sysIpcCreateSema(0);
			sm_entityStoringFinished[i]		= sysIpcCreateSema(0);
		}
		sm_quantizeStart		= sysIpcCreateSema(0);
		sm_recordingFinished	= sysIpcCreateSema(0);
		sm_recordingThreadEnded		= sysIpcCreateSema(0);
		sm_quantizeThreadEnded		= sysIpcCreateSema(0);
		sm_exitRecordingThread		= false;

#if RSG_DURANGO
		const int recordingThreadCore[] = { 1, 2, 3, 5 };
		const int quantizeThreadCore = 1;
#elif RSG_ORBIS
		const int recordingThreadCore[] = { 0, 1, 2, 3 };
		const int quantizeThreadCore = 0;
#else
		const int recordingThreadCore[] = { 0, 0, 0, 0 };
		const int quantizeThreadCore = 0;
#endif

		sm_recordingThread[0]		= sysIpcCreateThread(&RecordingThread, (void*)0, sysIpcMinThreadStackSize, PRIO_ABOVE_NORMAL, "Replay Default Recording", recordingThreadCore[0], "RecordingThreadDefault");
		sm_recordingThread[1]		= sysIpcCreateThread(&RecordingThread, (void*)1, sysIpcMinThreadStackSize, PRIO_ABOVE_NORMAL, "Replay Ped Recording", recordingThreadCore[1], "RecordingThreadPed");
		sm_recordingThread[2]		= sysIpcCreateThread(&RecordingThread, (void*)2, sysIpcMinThreadStackSize, PRIO_ABOVE_NORMAL, "Replay Vehicle Recording", recordingThreadCore[2], "RecordingThreadVehicle");
		sm_recordingThread[3]		= sysIpcCreateThread(&RecordingThread, (void*)3, sysIpcMinThreadStackSize, PRIO_ABOVE_NORMAL, "Replay Game/Object Recording", recordingThreadCore[3], "RecordingThreadGameAndObject");
		sm_quantizeThread			= sysIpcCreateThread(&QuantizeThread, NULL, sysIpcMinThreadStackSize, PRIO_BELOW_NORMAL, "Replay Quantize", quantizeThreadCore, "ReplayQuantizeThread");

		{
			sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());

			sm_pAdvanceReader = rage_new CReplayAdvanceReader(sm_firstFrameInfo, sm_lastFrameInfo, sm_ReplayBufferInfo);
			sm_pAdvanceReader->AddScanner(rage_new CReplayPreloader(sm_eventManager, sm_interfaceManager), CReplayAdvanceReader::ePreloadDirFwd);
			sm_pAdvanceReader->AddScanner(rage_new CReplayPreloader(sm_eventManager, sm_interfaceManager), CReplayAdvanceReader::ePreloadDirBack);
			sm_pAdvanceReader->AddScanner(rage_new CReplayPreplayer(sm_eventManager), CReplayAdvanceReader::ePreloadDirFwd);
		}
		sm_pAdvanceReader->Init();

		CReplayTrailController::GetInstance().Init();
		ReplayMovieController::Init();
		ReplayMovieControllerNew::Init();
	}

	sm_FileManager.Startup();

	sm_oRecord.Reset();
	sm_oPlayback.Reset();
	sm_oPlaybackStart.Reset();

	sm_oHistoryProcessPlayback.Reset();

	sm_uPlaybackStartTime = 0xffffffff;
	sm_uPlaybackEndTime = 0;
	sm_bShouldSetStartTime = false;

	SetCurrentTimeMs(0.0f);

	sm_fLastInterpValue = 0.0f;

	sm_uLastPlaybackTime = 0;

	sm_uMode = REPLAYMODE_DISABLED;
	sm_desiredMode = REPLAYMODE_DISABLED;
	CReplayControl::RemoveRecordingDisallowed(DISALLOWED1);
	CReplayControl::RemoveRecordingDisallowed(DISALLOWED2);
	SetNextPlayBackState(REPLAY_STATE_PAUSE);

	sm_pMainPlayer = NULL;

	sm_bFileRequestMemoryAlloc = false;
	sm_bSaveReplay = false;
	sm_eDelayedSaveState = REPLAY_DELAYEDSAVE_DISABLED;
	sm_bIsDead = false;
	sm_oCamMatrix.Identity();
	sm_bCameraInSniperMode = false;
	sm_sNumberOfRealObjectsRequested = -1;
	sm_uStreamingStallTimer = 0;
	sm_uAudioStallTimer = 0;
	sm_uStreamingSettleCount = 0;
	sm_bFinishPlaybackPending = false;
	sm_bUserCancel = false;

	sm_oReplayFullFrameTimer.Reset();
	sm_oReplayInterpFrameTimer.Reset();
	sm_fTimeScale = 1.0f;
	sm_uSniperModelIndex = 0;
	sm_bCheckForPreviewExit = false;
	sm_waitingForScriptCleanup = false;

	// Flag to re-initialise the audio entity on next preprocess 
	// to avoid conflicting with the audio thread
	sm_reInitAudioEntity = true;

	// (re)set Overwatch to the desired setting (this could have been disabled on a previous clip playback)
	sm_OverwatchEnabled = sm_ShouldOverwatchBeEnabled;
#if !__FINAL
	if(PARAM_disableOverwatch.Get())
		sm_OverwatchEnabled = false;
#endif

	CPacketSndLoadScriptBank::expirePrevious = -1;

#if __BANK
	s_fFramesElapsed = 0.0f;
	s_fSecondsElapsed = 0.0f;

	s_sNumberOfPedsBeforeReplay = 0;
	s_sNumberOfPedsDuringReplay = 0;
	s_sNumberOfPedsWaitingToDel = 0;
	s_sNumberOfVehiclesBeforeReplay = 0;
	s_sNumberOfVehiclesDuringReplay = 0;
	s_sNumberOfVehiclesWaitingToDel = 0;
	s_sNumberOfObjectsBeforeReplay = 0;
	s_sNumberOfObjectsDuringReplay = 0;
	s_sNumberOfObjectsWaitingToDel = 0;
	s_sNumberOfDummyObjectsBeforeReplay = 0;
	s_sNumberOfDummyObjectsDuringReplay = 0;
	s_sNumberOfNodesBeforeReplay = 0;
	s_sNumberOfNodesDuringReplay = 0;

	s_sNumberOfParticleEffectsAvailable = 0;
	s_trackablesCount = 0;	
#endif // __BANK

	CPacketRequestNamedPtfxAsset::ClearAll();

	// Do this only once at the game launch...
	if(initMode == INIT_CORE)
	{
		const u32 commonEffectFlags = REPLAY_STATE_PLAY | REPLAY_CURSOR_NORMAL | REPLAY_DIRECTION_FWD;
		const u32 decalFlags = REPLAY_STATE_PLAY | REPLAY_CURSOR_NORMAL | REPLAY_CURSOR_JUMP | REPLAY_CURSOR_SPEED | REPLAY_DIRECTION_FWD | REPLAY_DIRECTION_BACK | REPLAY_STATE_FAST | REPLAY_STATE_SLOW | REPLAY_STATE_PAUSE;
		const u32 notBackwardsEffectFlags = decalFlags & ~REPLAY_DIRECTION_BACK;
		const u32 notJumpEffectFlags = decalFlags & ~REPLAY_CURSOR_JUMP;

		sm_eventManager.SetInterfaceManager(&sm_interfaceManager);

		// Instant effects connected to an entity
		REGISTEREVENTPACKET(PACKETID_PEDBREATHFX,				CPacketPedBreathFx,			CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_PEDSMOKEFX,				CPacketPedSmokeFx,			CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_PEDFOOTFX,					CPacketPedFootFx,			CPed,			commonEffectFlags);		// Works, particle position is wrong
		REGISTEREVENTPACKET(PACKETID_PARACUTESMOKE,				CPacketParacuteSmokeFx,		CEntity,		commonEffectFlags | REPLAY_EVERYFRAME_PACKET);	// Works
		REGISTEREVENTPACKET(PACKETID_PEDVARIATIONCHANGE,		CPacketPedVariationChange,	CPed,			decalFlags);	
		REGISTEREVENTPACKET(PACKETID_PEDMICROMORPHCHANGE,		CPacketPedMicroMorphChange,	CPed,			decalFlags);	
		REGISTEREVENTPACKET(PACKETID_PEDHEADBLENDCHANGE,		CPacketPedHeadBlendChange,	CPed,			decalFlags);	
		REGISTEREVENTPACKET(PACKETID_VEHBACKFIREFX,				CPacketVehBackFireFx,		CVehicle,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_VEHHEADLIGHTSMASHFX,		CPacketVehHeadlightSmashFx, CVehicle,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_VEHRESPRAYFX,				CPacketVehResprayFx,		CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_VEHTYREPUNCTUREFX,			CPacketVehTyrePunctureFx,	CVehicle,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_VEHTYREBURSTFX,			CPacketVehTyreBurstFx,		CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_HELIPARTSDESTROYFX,		CPacketHeliPartsDestroyFx,	CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_VEHPARTFX,					CPacketVehPartFx,			CEntity,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_FIRESMOKEPEDFX,			CPacketFireSmokeFx,			CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WEAPONMUZZLEFLASHFX, 		CPacketWeaponMuzzleFlashFx, CEntity,		notBackwardsEffectFlags );	// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONMUZZLESMOKEFX, 		CPacketWeaponMuzzleSmokeFx, CEntity,		notBackwardsEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONBULLETIMPACTFX,		CPacketWeaponBulletImpactFx,CEntity,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONVOLUMETRICFX,		CPacketVolumetricFx,		CEntity,		commonEffectFlags | REPLAY_EVERYFRAME_PACKET);	// Works
		REGISTEREVENTPACKET(PACKETID_BLOODFX,					CPacketBloodFx,				CEntity,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_BLOODDECAL,				CPacketBloodDecal,			CEntity,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);	// Works
		REGISTEREVENTPACKET(PACKETID_BLOODMOUTHFX,				CPacketBloodMouthFx,		CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_BLOODFALLDEATHFX,			CPacketBloodFallDeathFx,	CPed,			commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_BLOODSHARKBITEFX,			CPacketBloodSharkBiteFx,	CPed,			commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_PTEXBLOODPOOL,				CPacketBloodPool,			void,			decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO |REPLAY_MONITORED_UPDATABLE);	
		REGISTEREVENTPACKET(PACKETID_BLOODWHEELFX,				CPacketBloodWheelSquashFx,	CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_BLOODPIGEONFX,				CPacketBloodPigeonShotFx,	CObject,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_ENTITYAMBIENTFX,			CPacketEntityAmbientFx,		CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_ENTITYFRAGBREAKFX,			CPacketEntityFragBreakFx,	CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_ENTITYSHOTFX_OLD,			CPacketEntityShotFx_Old,	CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_ENTITYSHOTFX,				CPacketEntityShotFx,		CEntity,		commonEffectFlags);

		REGISTEREVENTPACKET(PACKETID_PTFX_FRAGMENTDESTROY,		CPacketPtFxFragmentDestroy,	void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_DECAL_FRAGMENTDESTROY,		CPacketDecalFragmentDestroy,void,			commonEffectFlags);

		REGISTEREVENTPACKET(PACKETID_GLASSSMASHFX,				CPacketMiscGlassSmashFx,	CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERBOATENTRYFX,			CPacketWaterBoatEntryFx,	CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHHELIFX,			CPacketWaterSplashHeliFx,	CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHGENERICFX,		CPacketWaterSplashGenericFx,CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHVEHICLEFX,		CPacketWaterSplashVehicleFx,CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHVEHICLETRAILFX,	CPacketWaterSplashVehicleTrailFx ,CVehicle,	commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHPEDFX,			CPacketWaterSplashPedFx,	CPed,			commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHPEDOUTFX,		CPacketWaterSplashPedOutFx,	CPed,			commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_WATERSPLASHPEDINFX,		CPacketWaterSplashPedInFx,	CPed,			commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_RIPPLEWAKEFX,				CPacketRippleWakeFx,		CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_MATERIALBANGFX,			CPacketMaterialBangFx,		CEntity,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONEXPLOSIONFX_OLD,		CPacketWeaponExplosionFx_Old,CEntity,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONEXPLOSIONFX,			CPacketWeaponExplosionFx,	CEntity,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_PLANEWINGTIPFX,			CPacketPlaneWingtipFx,		CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SUBDIVEFX,					CPacketSubDiveFx,			CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_VEHICLESLIPSTREAMFX,		CPacketVehicleSlipstreamFx,	CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_PTEXWEAPONSHOT,			CPacketPTexWeaponShot,		CEntity,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);	// Works
		REGISTEREVENTPACKET(PACKETID_DECALSREMOVE,				CPacketDecalRemove,			CEntity,		decalFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDFOOTSTEP,			CPacketFootStepSound,		CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDBUSH,				CPacketPedBushSound,		CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDCLOTH,			CPacketPedClothSound,		CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDPETROLCAN,		CPacketPedPetrolCanSound,	CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDMOLOTOV,			CPacketPedMolotovSound,		CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_SCRIPTSWEETENER,		CPacketPedScriptSweetenerSound,	CPed,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDWATER,			CPacketPedWaterSound,		CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDSLOPEDEBRIS,		CPacketPedSlopeDebrisSound,	CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_PEDSTANDINMATERIAL,	CPacketPedStandingMaterial,	CPed,			commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_VEHICLE_HORN,		CPacketVehicleHorn,			CVehicle,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_VEHICLE_HORN_STOP,	CPacketVehicleHornStop,		CVehicle,		commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_TRAFFICLIGHTOVERRIDE,		CPacketTrafficLight,		CObject,		decalFlags);
		REGISTEREVENTPACKET(PACKETID_VEHVARIATIONCHANGE,		CPacketVehVariationChange,	CVehicle,		decalFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_PLAYERSWITCH,				CPacketPlayerSwitch,		CPed,			decalFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_TRAILDECAL,				CPacketTrailDecal,			void,			decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO | REPLAY_MONITORED_UPDATABLE | REPLAY_MONITORED_PLAY_BACKWARDS);		// Works

		REGISTEREVENTPACKET(PACKETID_VEHICLEDMGUPDATE,			CPacketVehDamageUpdate,		CVehicle,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);	// Works
		REGISTEREVENTPACKET(PACKETID_VEHICLEDMGUPDATE_PLAYERVEH,CPacketVehDamageUpdate_PlayerVeh, CVehicle,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);	// Works
		REGISTEREVENTPACKET(PACKETID_SMASHABLEEXPLOSION,		CPacketSmashableExplosion,	CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SMASHABLECOLLISION,		CPacketSmashableCollision,	CEntity,		commonEffectFlags);

		REGISTEREVENTPACKET(PACKETID_SCRIPTEDBUILDINGSWAP,		CPacketBuildingSwap,		void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_PEDBLOODDAMAGE,			CPacketPedBloodDamage,		CPed,			decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_PEDBLOODDAMAGESCRIPT,		CPacketPedBloodDamageScript,CPed,			decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_PTEXMTLBANG,				CPacketPTexMtlBang,				CEntity,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_PTEXMTLBANG_PLAYERVEH,		CPacketPTexMtlBang_PlayerVeh,	CEntity,		decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTEREVENTPACKET(PACKETID_PTEXMTLSCRAPE,				CPacketPTexMtlScrape,			CEntity,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_PTEXMTLSCRAPE_PLAYERVEH,	CPacketPTexMtlScrape_PlayerVeh,	CEntity,		decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTEREVENTPACKET(PACKETID_ADDSNOWBALLDECAL,			CPacketAddSnowballDecal,		void,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_DYNAMICWAVEFX,				CPacketDynamicWaveFx,		void,			commonEffectFlags);
		//REGISTEREVENTPACKET(PACKETID_SOUND_UPDATE_AND_PLAY,		CPacketSoundUpdateAndPlay,	void,			commonEffectFlags);


		REGISTEREVENTPACKET(PACKETID_SOUND_UPDATE_SETTINGS,		CPacketSoundUpdateSettings,	void,			commonEffectFlags);
		//REGISTEREVENTPACKET(PACKETID_SOUND_AUTOMATIC,			CPacketAutomaticSound,		void,			commonEffectFlags);
		//REGISTEREVENTPACKET(PACKETID_SOUND_BREATH,				CPacketBreathSound,			CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_PROJECTILE,			CPacketProjectile,			void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_PROJECTILESTOP,		CPacketProjectileStop,		void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_BULLETBY,			CPacketSoundBulletBy,		void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_EXPLOSIONBULLETBY,	CPacketSoundExplosionBulletBy,	void,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPEECH,				CPacketScriptedSpeech,		CPed,			commonEffectFlags | REPLAY_PRELOAD_PACKET);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPEECH_SCRIPTED_UPDATE,	CPacketScriptedSpeechUpdate,	CPed,	commonEffectFlags | REPLAY_PRELOAD_PACKET);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPEECH_PAIN,			CPacketSpeechPain,			CEntity,		decalFlags/*commonEffectFlags*/);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPEECH_STOP,			CPacketSpeechStop,			CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPEECH_SAY,			CPacketSpeech,				CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPEECH_ANIMAL,		CPacketAnimalVocalization,  CPed,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_CUTSCENE,			CCutSceneAudioPacket,		void,			commonEffectFlags | REPLAY_PRELOAD_PACKET);
		REGISTEREVENTPACKET(PACKETID_SOUND_CUTSCENE_STOP,		CCutSceneStopAudioPacket,   void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_SYNCHSCENE,			CSynchSceneAudioPacket,		CEntity,		commonEffectFlags | REPLAY_PRELOAD_PACKET);
		REGISTEREVENTPACKET(PACKETID_SOUND_SYNCHSCENE_STOP,		CSynchSceneStopAudioPacket, void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_WEAPON_ECHO,			CPacketSoundWeaponEcho,		void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SND_PAINWAVEBANKCHANGE,	CPacketPainWaveBankChange,	void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_SND_SETPLAYERPAINROOT,		CPacketSetPlayerPainRoot,	void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_PRESUCK,				CPacketPresuckSound,		CEntity,		decalFlags | REPLAY_PREPLAY_PACKET);
		REGISTEREVENTPACKET(PACKETID_SOUND_PLAYPRELOADEDSPEECH,	CPacketPlayPreloadedSpeech,	CPed,			commonEffectFlags);	// Not used anymore

		// Instant effects in the world																		
		REGISTEREVENTPACKET(PACKETID_WEAPONBULLETTRACEFX, 		CPacketWeaponBulletTraceFx,	void,			notBackwardsEffectFlags);			// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONBULLETTRACEFX2, 		CPacketWeaponBulletTraceFx2,	CObject,		notBackwardsEffectFlags);			// Works
		REGISTEREVENTPACKET(PACKETID_WEAPONEXPLOSIONWATERFX,	CPacketWeaponExplosionWaterFx,	void,		commonEffectFlags);			// Works
		REGISTEREVENTPACKET(PACKETID_GLASSGROUNDFX,				CPacketMiscGlassGroundFx,	void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SCRIPTEDFX,				CPacketScriptedFx,			void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SCRIPTEDUPDATEFX,			CPacketScriptedUpdateFx,	void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_PTEXFOOTPRINT,				CPacketPTexFootPrint,		void,			decalFlags);					// Works
		REGISTEREVENTPACKET(PACKETID_FIREFX,					CPacketFireFx_OLD,			CEntity,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);							// Works
		REGISTERTRACKEDPACKET(PACKETID_TRACKED_FIREFX,			CPacketFireFx,				CEntity,		ptxFireRef, decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS | REPLAY_MONITOR_PACKET_FRAME | REPLAY_EVERYFRAME_PACKET);	// Works
		REGISTERTRACKEDPACKET(PACKETID_TRACKED_STOPFIREFX,		CPacketStopFireFx,			CEntity,		ptxFireRef, decalFlags );	// Works

		REGISTEREVENTPACKET(PACKETID_FIREENTITYFX,				CPacketFireEntityFx,		CEntity,		commonEffectFlags);	
		REGISTEREVENTPACKET(PACKETID_EXPLOSIONFX,				CPacketExplosionFx,			CEntity,		commonEffectFlags | REPLAY_EVERYFRAME_PACKET);	
		REGISTEREVENTPACKET(PACKETID_DIRECTIONALLIGHTNING,		CPacketDirectionalLightningFxPacket,void,	commonEffectFlags);	// Works
		REGISTEREVENTPACKET(PACKETID_CLOUDLIGHTNING,			CPacketCloudLightningFxPacket,	void,		commonEffectFlags);			// Works
		REGISTEREVENTPACKET(PACKETID_LIGHTNINGSTRIKE,			CPacketLightningStrikeFxPacket,	void,		commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_EXPLOSION,			CPacketExplosion,			CEntity,		commonEffectFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_PROJECTILEFX,				CPacketFireProjectileFx,	CObject,		commonEffectFlags);		

		//glass packets
		REGISTEREVENTPACKET(PACKETID_GLASS_CREATEBREAKABLEGLASS,CPacketCreateBreakableGlass,CObject,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_GLASS_HIT,					CPacketHitGlass,			CObject,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_GLASS_TRANSFER,			CPacketTransferGlass,		CObject,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);

		REGISTEREVENTPACKET(PACKETID_WATERCANNONSPRAYFX,		CPacketWaterCannonSprayFx,	CVehicle,		notBackwardsEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERCANNONJETFX,			CPacketWaterCannonJetFx,	CVehicle,		notBackwardsEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WEAPONPROJTRAILFX,			CPacketWeaponProjTrailFx,	CObject,		notBackwardsEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_WEAPONGUNSHELLFX,			CPacketWeaponGunShellFx,	CEntity,		notBackwardsEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WEAPONFLASHLIGHT,			CPacketWeaponFlashLight,	CEntity,		decalFlags);
		REGISTEREVENTPACKET(PACKETID_WEAPON_THERMAL_VISION,		CPacketWeaponThermalVision,	void,			decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);

		// AC: These ScriptPtFX packets need to handle expiring properly
		REGISTERTRACKEDPACKET(PACKETID_STARTSCRIPTFX,			CPacketStartScriptPtFx,			CEntity,	ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS);
		REGISTERTRACKEDPACKET(PACKETID_STARTNETWORKSCRIPTFX,	CPacketStartNetworkScriptPtFx,	CEntity,	ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS | REPLAY_PRELOAD_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_STOPSCRIPTFX,			CPacketStopScriptPtFx,			void,	ptxEffectRef,	decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_DESTROYSCRIPTFX,			CPacketDestroyScriptPtFx,		void,	ptxEffectRef,	decalFlags);
		REGISTERTRACKEDPACKET(PACKETID_FORCEVEHINTERIORSCRIPTFX,CPacketForceVehInteriorScriptPtFx,	void,	ptxEffectRef,	decalFlags);
		REGISTERTRACKEDPACKET(PACKETID_EVOLVEPTFX,				CPacketEvolvePtFx,				void,	ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS | REPLAY_MONITOR_PACKET_JUST_ONE);
		REGISTERTRACKEDPACKET(PACKETID_UPDATEOFFSETSCRIPTFX,	CPacketUpdateOffsetScriptPtFx,	void,	ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS | REPLAY_MONITOR_PACKET_JUST_ONE);
		REGISTERTRACKEDPACKET(PACKETID_UPDATESCRIPTFXCOLOUR,	CPacketUpdateScriptPtFxColour,	void,	ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS | REPLAY_MONITOR_PACKET_JUST_ONE);
		REGISTERTRACKEDPACKET(PACKETID_UPDATESCRIPTFXFARCLIPDIST,	CPacketUpdatePtFxFarClipDist,	void,	ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS | REPLAY_MONITOR_PACKET_JUST_ONE);

		REGISTEREVENTPACKET(PACKETID_VEHICLE_BADGE_REQUEST,		CPacketVehicleBadgeRequest,		CVehicle,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS);
		REGISTEREVENTPACKET(PACKETID_VEHICLE_WEAPON_CHARGE,		CPacketVehicleWeaponCharge,		CVehicle,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_TRIGGEREDSCRIPTFX,			CPacketTriggeredScriptPtFx,		CEntity,	commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_TRIGGEREDSCRIPTFXCOLOUR,	CPacketTriggeredScriptPtFxColour,		void,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_REQUESTPTFXASSET,			CPacketRequestPtfxAsset,		void,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_REQUESTNAMEDPTFXASSET,		CPacketRequestNamedPtfxAsset,	void,		commonEffectFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_PRELOAD_PACKET);

		// Continual effects that are interpolated
		// 		REGISTERINTERPPACKET(PACKETID_VEHOVERHEATFX,			CPacketVehOverheatFx,		CVehicle,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_VEHPETROLTANKFX, 			CPacketVehFirePetrolTankFx, CVehicle,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_VEHTYREFIREFX,			CPacketVehTyreFireFx,		CVehicle,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_TRAINSPARKSFX,			CPacketVehTrainSparksFx,	CVehicle,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_HELIDOWNWASHFX,			CPacketHeliDownWashFx,		CVehicle,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_FIREVEHWRECKEDFX,			CPacketFireVehWreckedFx,	CVehicle,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_FIRESTEAMFX,				CPacketFireSteamFx,			CEntity,	ptxEffectInst,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_FIREPEDFX,				CPacketFirePedBoneFx,		CPed,		ptxEffectInst,		commonEffectFlags);

		//		REGISTERINTERPPACKET(PACKETID_ENTITYANIMUPDTFX,			CPacketEntityAnimUpdtFx,	CEntity,	ptxEffectInst,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_WATERBOATWASHFX,			CPacketWaterBoatWashFx,			CVehicle,	commonEffectFlags);	// Works
 		REGISTEREVENTPACKET(PACKETID_WATERBOATBOWFX,			CPacketWaterBoatBowFx,			CVehicle,	commonEffectFlags);	// Works
 		REGISTEREVENTPACKET(PACKETID_WATERSPLASHPEDWADEFX,		CPacketWaterSplashPedWadeFx,	CPed,		commonEffectFlags);
		// 		REGISTERINTERPPACKET(PACKETID_WATERSPLASHVEHWADEFX,		CPacketWaterSplashVehWadeFx,CVehicle,	ptxEffectInst,		commonEffectFlags);

		REGISTERTRACKEDPACKET(PACKETID_ADDROPE,					CPacketAddRope,						void,   ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_UPDATABLE | REPLAY_MONITORED_PLAY_BACKWARDS);
		REGISTERTRACKEDPACKET(PACKETID_DELETEROPE,				CPacketDeleteRope,					CEntity,ptxEffectRef,	decalFlags);
		REGISTERTRACKEDPACKET(PACKETID_ATTACHROPETOENTITY,		CPacketAttachRopeToEntity,			CEntity,ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTERTRACKEDPACKET(PACKETID_ATTACHENTITIESTOROPE,	CPacketAttachEntitiesToRope,		CEntity,ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITORED_PLAY_BACKWARDS);
		REGISTERTRACKEDPACKET(PACKETID_DETACHROPEFROMENTITY,	CPacketDetachRopeFromEntity,		CEntity,ptxEffectRef,	decalFlags );
		REGISTERTRACKEDPACKET(PACKETID_ATTACHOBJECTSTOROPEARRAY,CPacketAttachObjectsToRopeArray,	CEntity,ptxEffectRef,	decalFlags );
		REGISTERTRACKEDPACKET(PACKETID_PINROPE,					CPacketPinRope,						void,	ptxEffectRef,	decalFlags | REPLAY_EVERYFRAME_PACKET );
		REGISTERTRACKEDPACKET(PACKETID_UNPINROPE,				CPacketUnPinRope,					void,	ptxEffectRef,	decalFlags | REPLAY_EVERYFRAME_PACKET );
		REGISTERTRACKEDPACKET(PACKETID_RAPPELPINROPE,			CPacketRappelPinRope,				void,	ptxEffectRef,	decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_LOADROPEDATA,			CPacketLoadRopeData,				void,	ptxEffectRef,	commonEffectFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTERTRACKEDPACKET(PACKETID_ROPEWINDING,				CPacketRopeWinding,					void,	ptxEffectRef,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_ROPEUPDATEORDER,			CPacketRopeUpdateOrder,				void,   ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);

		//OLD ROPE VERSION TO REMOVE
		REGISTERTRACKEDPACKET(PACKETID_ADDROPE_OLD,					CPacketAddRope_OLD,						void,   ptxEffectRef,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTERTRACKEDPACKET(PACKETID_DELETEROPE_OLD,				CPacketDeleteRope_OLD,					void,	ptxEffectRef,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_DETACHROPEFROMENTITY_OLD,	CPacketDetachRopeFromEntity_OLD,		CEntity,ptxEffectRef,	decalFlags );

		// Script decals
		REGISTERTRACKEDPACKET(PACKETID_SCRIPTDECALADD,				CPacketAddScriptDecal,			void,	tTrackedDecalType,	decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTERTRACKEDPACKET(PACKETID_SCRIPTDECALREMOVE,			CPacketRemoveScriptDecal,		void,	tTrackedDecalType,	decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_DISABLE_DECAL_RENDERING,		CPacketDisableDecalRendering,	void,	decalFlags | REPLAY_EVERYFRAME_PACKET);


		// Sound effects
		REGISTEREVENTPACKET(PACKETID_SOUND_WEAPON_FIRE_EVENT_OLD,	CPacketWeaponSoundFireEventOld,	CEntity,	commonEffectFlags | REPLAY_PREPLAY_PACKET);					// Works
		REGISTEREVENTPACKET(PACKETID_SOUND_WEAPON_FIRE_EVENT,		CPacketWeaponSoundFireEvent,	CEntity,	commonEffectFlags | REPLAY_PREPLAY_PACKET);					// Works
		REGISTERTRACKEDPACKET(PACKETID_SOUND_WEAPON_PERSIST,		CPacketWeaponPersistant,		CEntity,	tTrackedSoundType,	commonEffectFlags);				// Works
		REGISTERTRACKEDPACKET(PACKETID_SOUND_WEAPON_PERSIST_STOP,	CPacketWeaponPersistantStop,	CEntity,	tTrackedSoundType,	commonEffectFlags);				// Works
		REGISTERTRACKEDPACKET(PACKETID_SOUND_WEAPON_SPINUP,			CPacketWeaponSpinUp,			CEntity,	tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_COLLISIONPLAY,			CPacketCollisionPlayPacket,		CEntity,	tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_COLLISIONUPDATE,		CPacketCollisionUpdatePacket,	void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_STOP,					CPacketSoundStop,				void,		tTrackedSoundType,	commonEffectFlags);						// Works
		REGISTERTRACKEDPACKET(PACKETID_SOUND_START, 				CPacketSoundStart,				void,		tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_UPDATE_VOLUME,			CPacketSoundUpdateVolume,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_UPDATE_PITCH,			CPacketSoundUpdatePitch,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_UPDATE_DOPPLER,		CPacketSoundUpdateDoppler,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_SET_VALUE_DH,			CPacketSoundSetValueDH,			void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_SET_CLIENT_VAR,		CPacketSoundSetClientVar,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_PHONE_RING,			CPacketSoundPhoneRing,			CEntity,	tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_STOP_PHONE_RING,		CPacketSoundStopPhoneRing,		CEntity,	tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_CREATE_AND_PLAY_PERSIST,CPacketSoundCreatePersistant,	CEntity,	tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_UPDATE_PERSIST,		CPacketSoundUpdatePersistant,	CEntity,	tTrackedSoundType,	commonEffectFlags);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_LPF_CUTOFF,			CPacketSoundUpdateLPFCutoff,	void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_PITCH_POS,				CPacketSoundUpdatePitchPos,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_PITCH_POS_VOL,			CPacketSoundUpdatePitchPosVol,	void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_PITCH_VOL,				CPacketSoundUpdatePitchVol,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_POS,					CPacketSoundUpdatePosition,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_POS_VOL,				CPacketSoundUpdatePosVol,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SOUND_POST_SUBMIX,			CPacketSoundUpdatePostSubmix,	void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SND_PLAYSTREAMFROMENTITY,	CPacketPlayStreamFromEntity,	CEntity,	tTrackedSoundType,	commonEffectFlags | REPLAY_PRELOAD_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SND_PLAYSTREAMFROMPOSITION,	CPacketPlayStreamFromPosition,	void,		tTrackedSoundType,	commonEffectFlags | REPLAY_PRELOAD_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SND_PLAYSTREAMFRONTED,		CPacketPlayStreamFrontend,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_PRELOAD_PACKET);		
		REGISTERTRACKEDPACKET(PACKETID_DYNAMICMIXER_SCENE,			CPacketDynamicMixerScene,		void,		tTrackedSceneType,	commonEffectFlags );
		REGISTERTRACKEDPACKET(PACKETID_SOUND_SET_VARIABLE_ON_SOUND,	CPacketScriptSetVarOnSound,		void,		tTrackedSoundType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTERTRACKEDPACKET(PACKETID_SET_AUDIO_SCENE_VARIABLE,	CPacketSceneSetVariable,		void,		tTrackedSceneType,	commonEffectFlags | REPLAY_EVERYFRAME_PACKET);

		//REGISTERSOUNDPACKET(PACKETID_WATERBOATBOWFX,			CPacketWaterBoatBowFx,			ptxEffectInst);

		REGISTEREVENTPACKET(PACKETID_SND_STOPSTREAM,			CPacketStopStream,					void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_CREATE_MISC_OLD,		CPacketSoundCreateMisc_Old,			CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_CREATE_MISC,			CPacketSoundCreateMisc,				CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_CREATE_MISC_W_VAR,	CPacketSoundCreateMiscWithVars,		CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_CREATE_POSITION,		CPacketSoundCreatePos,				void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_CREATE_AND_PLAY,		CPacketSoundCreate,					CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_VEHICLE_PED_COLLSION,CPacketVehiclePedCollisionPacket,	CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_PED_IMPACT_COLLSION, CPacketPedImpactPacket,				CEntity,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_VEHICLE_SPLASH,		CPacketVehicleSplashPacket,			CVehicle,		commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_BULLETIMPACT,		CPacketBulletImpact,				void,			commonEffectFlags);

		/*ADDEVENTPACKETFUNCTORS(PACKETID_WEAPON_ECHO,				CPacketSoundWeaponEcho, CEntity);*/
		//REGISTEREVENTPACKET(PACKETID_SOUND_PLAYERWEAPONREPORT,	CPacketSoundPlayerWeaponReport, CEntity,		commonEffectFlags);

		REGISTEREVENTPACKET(PACKETID_SND_LOADWEAPONDATA,		CPacketSndLoadWeaponData,		void,		decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITOR_PACKET_JUST_ONE | REPLAY_MONITORED_PLAY_BACKWARDS);

		REGISTEREVENTPACKET(PACKETID_SND_LOADSCRIPTWAVEBANK,	CPacketSndLoadScriptWaveBank,	void,		commonEffectFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_PRELOAD_PACKET);
		REGISTEREVENTPACKET(PACKETID_SND_LOADSCRIPTBANK,		CPacketSndLoadScriptBank,		void,		commonEffectFlags | REPLAY_EVERYFRAME_PACKET );

		REGISTEREVENTPACKET(PACKETID_MOVIE,						CPacketMovie,				void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_MOVIE_TARGET,				CPacketMovieTarget,			void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_MOVIE_ENTITY,				CPacketMovieEntity,			void,			decalFlags);

		REGISTEREVENTPACKET(PACKETID_IPL,						CPacketIPL,					void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_FORCE_ROOM_FOR_GAMEVIEWPORT,CPacketForceRoomForGameViewport,void,		notJumpEffectFlags | REPLAY_MONITOR_PACKET_LOW_PRIO | REPLAY_MONITOR_PACKET_FRAME);

		REGISTERTRACKEDPACKET(PACKETID_FORCE_ROOM_FOR_ENTITY,	CPacketForceRoomForEntity,			CEntity, int, decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITOR_PACKET_JUST_ONE);

		REGISTEREVENTPACKET(PACKETID_MODEL_CULL,				CPacketModelCull,			void,			decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_DISABLE_OCCLUSION,			CPacketDisableOcclusion,	void,			decalFlags | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_MESH_SET,					CPacketMeshSet,				void,			decalFlags);

		REGISTEREVENTPACKET(PACKETID_RAYFIRE_STATIC,			CPacketRayFireStatic,			void,			decalFlags);
		REGISTERINTERPPACKET(PACKETID_RAYFIRE_UPDATING,			CPacketRayFireUpdating,			void,			ptxEffectRef, decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_RAYFIRE_PRE_LOAD,			CPacketRayFirePreLoad,			void,			decalFlags | REPLAY_PRELOAD_PACKET);

		REGISTEREVENTPACKET(PACKETID_CUTSCENE_NONRP_OBJECT_HIDE,CPacketCutsceneNonRPObjectHide,void,			decalFlags);

		REGISTEREVENTPACKET(PACKETID_CUTSCENECHARACTERLIGHTPARAMS, CPacketCutsceneCharacterLightParams,			void,		decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_CUTSCENECAMERAARGS,		CPacketCutsceneCameraArgs,		void,		decalFlags | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_REQUESTCLOUDHAT,			CPacketRequestCloudHat,			void,			decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITOR_PACKET_ONE_PER_EFFECT);		// Works
		REGISTEREVENTPACKET(PACKETID_UNLOADALLCLOUDHATS,		CPacketUnloadAllCloudHats,		void,			decalFlags);		// Works
		REGISTEREVENTPACKET(PACKETID_LIGHT_BROKEN,				CPacketLightBroken,				void,		decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_PHONE_LIGHT,				CPacketPhoneLight,				CEntity,	decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_PED_LIGHT,					CPacketPedLight,				CEntity,	decalFlags | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_OBJECT_SNIPED,				CPacketObjectSniped,			CObject,	decalFlags | REPLAY_MONITOR_PACKET_LOW_PRIO);
		REGISTEREVENTPACKET(PACKETID_WATERFOAM,					CPacketWaterFoam,				void,		notBackwardsEffectFlags);
		REGISTEREVENTPACKET(PACKETID_TOWTRUCKARMROTATE,			CPacketTowTruckArmRotate,		CVehicle,		decalFlags | REPLAY_EVERYFRAME_PACKET);	// Works
		
		REGISTEREVENTPACKET(PACKETID_ENTITYATTACHDETACH,		CPacketEntityAttachDetach,		void,		decalFlags);
	
		REGISTEREVENTPACKET(PACKETID_SET_UNDERWATER_STREAM_VARIABLE,	CPacketSetUnderWaterStreamVariable,		void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SET_OVERRIDE_UNDERWATER_STREAM,	CPacketOverrideUnderWaterStream,		void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SET_SCRIPT_AUDIO_FLAGS,			CPacketScriptAudioFlags,				void,			commonEffectFlags);
		REGISTEREVENTPACKET(PACKETID_SOUND_SPECIAL_EFFECT_MODE,			CPacketScriptAudioSpecialEffectMode,	void,			commonEffectFlags | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_REGVEHICLEFIRE,			CPacketRegisterVehicleFire,		CVehicle,			commonEffectFlags | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_SETDUMMYOBJECTTINT,		CPacketSetDummyObjectTint, void,				decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_SETBUILDINGTINT,			CPacketSetBuildingTint,		void,				decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_EVERYFRAME_PACKET);
		REGISTEREVENTPACKET(PACKETID_SETOBJECTTINT,				CPacketSetObjectTint,		CObject,			decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_REGISTERANDLINKRENDERTARGET, CPacketRegisterAndLinkRenderTarget, void,		decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTEREVENTPACKET(PACKETID_SETGFXALIGN,				CPacketSetGFXAlign,	void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_SETRENDER,					CPacketSetRender,	void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_DRAWSPRITE,				CPacketDrawSprite,	void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_DRAWSPRITE2,				CPacketDrawSprite2,	void,			decalFlags);

		REGISTEREVENTPACKET(PACKETID_TRAILPOLYSFRAME,			CPacketTrailPolyFrame,	void,		notJumpEffectFlags | REPLAY_EVERYFRAME_PACKET);

		REGISTEREVENTPACKET(PACKETID_VEHICLEJUMPRECHARGE,		CPacketVehicleJumpRechargeTimer,		CVehicle,		commonEffectFlags);	
		REGISTEREVENTPACKET(PACKETID_SOUND_BULLETIMPACT_NEW,	CPacketBulletImpactNew,				void,			commonEffectFlags);

		REGISTEREVENTPACKET(PACKETID_SOUND_THERMAL_SCOPE,		CPacketThermalScopeAudio,			void,			commonEffectFlags);
				
		REGISTEREVENTPACKET(PACKETID_FOCUSENTITY,				CPacketFocusEntity,		CEntity,			decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTEREVENTPACKET(PACKETID_FOCUSPOSANDVEL,			CPacketFocusPosAndVel,	void,				decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);

		REGISTEREVENTPACKET(PACKETID_SOUND_WEAPON_AUTO_FIRE_STOP, CPacketWeaponAutoFireStop,	CEntity,	commonEffectFlags | REPLAY_PREPLAY_PACKET);					// Works

		REGISTEREVENTPACKET(PACKETID_OVERRIDEOBJECTLIGHTCOLOUR, CPacketOverrideObjectLightColour,	CObject,	decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);

		REGISTEREVENTPACKET(PACKETID_SOUND_VEHICLE_NITROUS_ACTIVE, CPacketSoundNitrousActive,	CVehicle,	commonEffectFlags);					

		REGISTEREVENTPACKET(PACKETID_REGISTER_RENDERTARGET,		CPacketRegisterRenderTarget,	void, decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTEREVENTPACKET(PACKETID_LINK_RENDERTARGET,			CPacketLinkRenderTarget,		void, decalFlags | REPLAY_MONITOR_PACKET_HIGH_PRIO);
		REGISTEREVENTPACKET(PACKETID_DRAWMOVIE,					CPacketDrawMovie,	void,			decalFlags);
		REGISTEREVENTPACKET(PACKETID_MOVIE2,					CPacketMovie2,				void,			decalFlags);
	}

	if(initMode == INIT_SESSION)
	{
		sysMemAllocator& oldAlloc = sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(*MEMMANAGER.GetReplayAllocator());
		CReplayInterfaceGame* pGameInterface	= rage_new CReplayInterfaceGame(sm_fTimeScale, sm_overrideTimePlayback, sm_overrideWeatherPlayback);
		CReplayInterfaceCamera* pCamInterface	= rage_new CReplayInterfaceCamera(sm_replayFrameData.m_cameraData, sm_interfaceManager, sm_useRecordedCamera);
		CReplayInterfaceVeh* pVehInterface		= rage_new CReplayInterfaceVeh();
		CReplayInterfacePed* pPedInterface		= rage_new CReplayInterfacePed(*pVehInterface);
		CReplayInterfaceObject* pObjInterface	= rage_new CReplayInterfaceObject();
		CReplayInterfacePickup* pPickupInterface = rage_new CReplayInterfacePickup();
		sysMemAllocator::SetCurrent(oldAlloc);

		sm_interfaceManager.AddInterface(pGameInterface);
		sm_interfaceManager.AddInterface(pCamInterface);
		sm_interfaceManager.AddInterface(pVehInterface);
		sm_interfaceManager.AddInterface(pPedInterface);
		sm_interfaceManager.AddInterface(pPickupInterface);
		sm_interfaceManager.AddInterface(pObjInterface);

		if(CObject::GetPool() && CPickup::GetPool() && CPed::GetPool() && CVehicle::GetPool())
		{
			CReplayExtensionManager::RegisterExtension<ReplayEntityExtension>(STRINGIFY(ReplayEntityExtension), (u32)(CObject::GetPool()->GetSize() + CPickup::GetPool()->GetSize() + CPed::GetPool()->GetSize() + CVehicle::GetPool()->GetSize()));
			CReplayExtensionManager::RegisterExtension<ReplayObjectExtension>(STRINGIFY(ReplayObjectExtension), (u32)CObject::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayPedExtension>(STRINGIFY(ReplayPedExtension), (u32)CPed::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayBicycleExtension>(STRINGIFY(ReplayBicycleExtension), (u32)CVehicle::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayGlassExtension>(STRINGIFY(ReplayGlassExtension), (u32)CObject::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayParachuteExtension>(STRINGIFY(ReplayParachuteExtension), (u32)CPed::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayReticuleExtension>(STRINGIFY(ReplayReticuleExtension), (u32)CPed::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayHUDOverlayExtension>(STRINGIFY(ReplayHUDOverlayExtension), (u32)CPed::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayTrafficLightExtension>(STRINGIFY(ReplayTrafficLightExtension), (u32)CObject::GetPool()->GetSize());
			CReplayExtensionManager::RegisterExtension<ReplayVehicleExtension>(STRINGIFY(ReplayVehicleExtension), (u32)CVehicle::GetPool()->GetSize());			
		}

#if GTA_REPLAY_OVERLAY
		CReplayOverlay::GeneratePacketNameList(sm_interfaceManager, sm_eventManager);
		CReplayOverlay::UpdateComboWidgets();
#endif

	}	
}

//========================================================================================================================
void CReplayMgrInternal::InitWidgets()
{
#if __BANK
	AddDebugWidgets();
#endif // __BANK
}

//========================================================================================================================
void CReplayMgrInternal::Shutdown()
{
	PauseReplayPlayback();

	{
		sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
		sm_pAdvanceReader->Shutdown();
		delete sm_pAdvanceReader;
		sm_pAdvanceReader = NULL;
	}

	Disable();

	sm_CachedSoundPackets.Reset();
}

void CReplayMgrInternal::WantQuit()
{
	sm_wantQuit = true;
}

//========================================================================================================================
void CReplayMgrInternal::QuitReplay()
{
	REPLAYTRACE("*CReplayMgrInternal::QuitReplay");

	Disable();

#if USE_SAVE_SYSTEM
	// only load savegame if it has been saved
	bool bStartNewSession = false;
	if(g_ReplaySaveState == REPLAY_SAVE_SUCCEEDED)
	{
		sm_ReplayLoadStructure.Init();
		if (!CGenericGameStorage::PushOnToSavegameQueue(&sm_ReplayLoadStructure))
		{
			bStartNewSession = true;
		}
	}
	else if (g_ReplaySaveState == REPLAY_SAVE_FAILED)
	{
		bStartNewSession = true;
	}

	if (bStartNewSession)
	{
		CGame::SetStateToIdle();

		camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
		CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
	}

	g_ReplaySaveState = REPLAY_SAVE_NOT_STARTED;
#else	//	USE_SAVE_SYSTEM
	if (g_bWorldHasBeenCleared)
	{
		if (!CGenericGameStorage::QueueLoadMostRecentSave())
		{
			CGame::SetStateToIdle();

			camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
			CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
		}
	}

	g_bWorldHasBeenCleared = false;
#endif	//	USE_SAVE_SYSTEM

	//clear any requests that have been made.
	sm_StreamingRequests.Reset();

	//Reset light list when leaving replay
	ReplayLightingManager::Reset();

	//Do some audio cleanup if we edited a clip.
	if(sm_ReplayInControlOfWorld)
	{
		audNorthAudioEngine::ShutdownReplayEditor();
	}

	if( sm_bMultiplayerClip )
	{
		audNorthAudioEngine::NotifyCloseNetwork(true);
		sm_bMultiplayerClip = false;
	}

	sm_desiredMode = REPLAYMODE_DISABLED;

	sm_wantQuit = false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::Enable()
{
	REPLAYTRACE("*CReplayMgrInternal::Enable");

	if(sm_bReplayEnabled)
	{
		return true;
	}

	if(TotalNumberOfReplayBlocks > MAX_NUMBER_OF_ALLOCATED_BLOCKS)
	{
		replayAssertf(false, "Replay allocation error, tried to allocate more memory than the maxmium, requested %i blocks, max is %i blocks", TotalNumberOfReplayBlocks, MAX_NUMBER_OF_ALLOCATED_BLOCKS);
		TotalNumberOfReplayBlocks = MAX_NUMBER_OF_ALLOCATED_BLOCKS;
	}

	sm_bReplayEnabled = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::Disable()
{
	REPLAYTRACE("*CReplayMgrInternal::Disable");
	if(sm_bReplayEnabled == false)	return false;

	sm_bReplayEnabled = false;
	sm_currentFrameInfo.Reset();
	if(!CReplayMgr::IsReplayInControlOfWorld())
	{
		g_ScriptAudioEntity.ClearPlayedList();
		g_SpeechManager.FreeAllAmbientSpeechSlots();
	}

	// If we're leaving edit mode then put the game state back to they way
	// it was.
 	if(sm_uMode == REPLAYMODE_EDIT && sm_haveFrozenShadowSetting == true)
 	{
 		extern ShadowFreezing ShadowFreezer;
 		ShadowFreezer.SetEnabled(sm_shadowsFrozenInGame);
 	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::HandleLogicForMemory()
{
	sysCriticalSection cs(sm_PacketRecordCriticalSection);

	// File loading logic
	if(sm_bFileRequestMemoryAlloc)
	{
		//make sure we disable the replay to ensure the right amount of memory is allocated when it's enable again
		Disable();
		Enable();
		SetupReplayBuffer(NumberOfReplayBlocks, NumberOfTempReplayBlocks);

		// Signal the loading thread that the memory is ready
		sysIpcSignalSema(sm_LoadOpWaitForMem);

		sm_bFileRequestMemoryAlloc = false;
	}
	// FrontEnd logic
	else
	{
		if(sm_bReplayEnabled && SettingsChanged)
		{
			Disable();
			Enable();
			SettingsChanged = false;
		}
	}
}

//========================================================================================================================
CReplayMgrInternal::ValidationResult CReplayMgrInternal::Validate(bool preload)
{
	if (IsLoading() || IsClipTransition())
	{
		return eValidationOther;
	}

	if(!sm_eReplayState.IsPrecaching() && sm_KickPrecaching)
	{
		sm_eReplayState.SetPrecaching(true);
		sm_nextReplayState.SetPrecaching(true);
		sm_KickPreloader = true;
		sm_IgnorePostLoadStateChange = true;

		// Stop the replay music and immediately kick off a sync request
		if(!sm_pPlaybackController || !sm_pPlaybackController->IsExportingToVideoFile())
		{
			g_RadioAudioEntity.StopReplayMusic();
		}	

		if(sm_desiredMode != REPLAYMODE_DISABLED)
		{
			RequestReplayMusic();
		}

		if(sm_eLoadingState == REPLAYLOADING_NONE)
		{
			sm_eLoadingState = REPLAYLOADING_PRECACHING;
		}
	}

	sm_KickPrecaching = false;

	if(sm_JumpPrepareState)
	{
		return eValidationOk;
	}

	// Reset buffers when needed
	if (!IsEnabled() ||
		CutSceneManager::GetInstancePtr()->IsRunning() ||
		(IsEditModeActive() && IsPreCachingScene()))
	{
		if (IsPreCachingScene())
		{
			
			if(preload)
			{
				sm_uStreamingStallTimer += MIN(33,fwTimer::GetNonPausedTimeStepInMilliseconds());
			}
			
			if ( !IsWaitingOnWorldStreaming() /*&& (sm_sNumberOfRealObjectsRequested == 0*/)
			{
				if(sm_islandActivated.GetHash() != 0)
				{
					CIslandHopper::GetInstance().RequestToggleIsland(sm_islandActivated, true);
					sm_islandActivated = ATSTRINGHASH("", 0);
					return eValidationOther;
				}

				const int worldStreamedConsecutiveFrames = 10;

				++sm_uStreamingSettleCount;
				if(sm_uStreamingSettleCount < worldStreamedConsecutiveFrames)
				{
					return eValidationOther;
				}

				if(!preload)
				{
					return eValidationOther;
				}

				// Music should already be syncing by this point, but safe to poll it just in-case
				if(sm_desiredMode != REPLAYMODE_DISABLED)
				{
					RequestReplayMusic();
				}

				if(sm_KickPreloader)
				{	
					replayAssert(!sm_JumpPrepareState);
					sm_pAdvanceReader->Kick(sm_currentFrameInfo, sm_bPreloaderResetFwd, sm_bPreloaderResetBack);
				}

				bool readerResult = sm_pAdvanceReader->HandleResults(CReplayPreloader::ScannerType, CReplayState(), 0, false);

				if(sm_pAdvanceReader->HasReachedExtents(CReplayPreloader::ScannerType))
					sm_KickPreloader = false;

				if(!readerResult)
				{
					return eValidationOther;
				}
				
				// Check music stream is prepared and ready to play when playback resumes
				g_RadioAudioEntity.PreUpdateReplayMusic();
				if((!g_RadioAudioEntity.IsReplayMusicTrackPrepared() || !g_ReplayAudioEntity.AreAllStreamingSoundsPrepared()) && sm_uAudioStallTimer < AUDIO_STALL_LIMIT_MS)
				{
					sm_uAudioStallTimer += MIN(33,fwTimer::GetNonPausedTimeStepInMilliseconds());
					return eValidationOther;
				}
				replayAssertf(sm_uAudioStallTimer < AUDIO_STALL_LIMIT_MS, "Timed out waiting for %s", !g_RadioAudioEntity.IsReplayMusicTrackPrepared() ? "IsReplayMusicTrackPrepared" : "AreAllStreamingSoundsPrepared");

				sm_IgnorePostLoadStateChange = false;
				sm_forcePortalScanOnClipLoad = true;
				GetCurrentPlayBackState().SetPrecaching(false);
				sm_nextReplayState.SetPrecaching(false);
				sm_uStreamingStallTimer = 0;
				sm_uAudioStallTimer = 0;
				sm_eLoadingState = REPLAYLOADING_NONE;
				sm_WantsToKickPrecaching = false;
				sm_uStreamingSettleCount = 0;

				// Unfreeze the render phase now the precaching has finished.
				m_WantsReactiveRenderer = true;
				
#if USE_SRLS_IN_REPLAY
				if(IsUsingSRL() && !gStreamingRequestList.IsPlaybackActive())
				{
					//by this point the prestreaming of the SRL should have been 
					//completed in IsWaitingOnWorldStreaming, no we start actually 
					//using it for the playback
					//only start this when we are actually leaving the precaching stage
					gStreamingRequestList.Start();
				}
#endif
			}
			else
			{
				sm_uStreamingSettleCount = 0;
				return eValidationOther;
			}
		}
		return eValidationOk;
	}
	else if (fwTimer::IsGamePaused() /*TODO4FIVE || (ReplayPlaybackGoingOn()  && gCamInterface.GetFading())*/)
	{
		// Last condition is to ensure the replay is ffwd'ed to the starting point if start point is not frame 0.
		sm_oReplayInterpFrameTimer.Reset();
		sm_oReplayFullFrameTimer.Reset();

		return eValidationPaused;
	}

	return eValidationOk;
}

void CReplayMgrInternal::FreezeAudio()
{
}

void CReplayMgrInternal::UnfreezeAudio()
{	
	if(!CReplayMgr::IsReplayInControlOfWorld())
	{
		// If replay wasn't in control, do nothing.
		return;
	}

	g_ReplayAudioEntity.StopAllSFX();

	if(!sm_pPlaybackController || !sm_pPlaybackController->IsExportingToVideoFile())
	{
		g_InteractiveMusicManager.StopAndReset();
		g_RadioAudioEntity.StopReplayMusic();
		g_ReplayAudioEntity.StopAllStreams();
	}

	// we need these because time goes backwards in the replay editor
	/*TODO4FIVE g_AmbientAudioEntity.ResetLastTimePlayed();
	g_ExplosionAudioEntity.ResetDebrisDelay();
	g_EmitterAudioEntity.ResetTimers();
	//audGtaAudioEngine::GetGtaEnvironment()->ResetTimersForReplay();*/
	g_ReplayAudioEntity.StopAllStreams();
	g_ScriptAudioEntity.ClearPlayedList();
	g_ScriptAudioEntity.GetSpeechAudioEntity().StopAmbientSpeech();
	g_ScriptAudioEntity.StopStream();
	g_ScriptAudioEntity.ClearRecentPedCollisions();
	g_FrontendAudioEntity.StopAllSounds();
	g_EmitterAudioEntity.ClearBuildingAnimEvents();

	g_EmitterAudioEntity.StopAndResetStaticEmitters();
	g_SpeechManager.FreeAllAmbientSpeechSlots();
	g_FrontendAudioEntity.SetReplayMuteVolume( -100.0f );
	g_CutsceneAudioEntity.StopCutscene(true, 0);
	g_AudioScannerManager.CancelAllReports();
	g_PoliceScanner.SetNextAmbientMs(g_AudioEngine.GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(1000, 15000));
	g_AudioScannerManager.SetTimeCheckedLastPosition(g_AudioEngine.GetTimeInMilliseconds());

	audNorthAudioEngine::GetDynamicMixer().KillReplayScenes();

	sm_MusicOffsetTime = 0;
	sm_sMusicStartCountDown = 1;

	audNorthAudioEngine::StopAllSounds();
	audNorthAudioEngine::UnPause();			// this allows the sounds to stop
	g_AudioEngine.GetSoundManager().Pause(2);

	sm_interfaceManager.StopAllEntitySounds();

	//Clean up the cut scene audio
	CutSceneManager::GetInstance()->ReplayCleanup();

	naAudioEntity::ClearDeferredSoundsForReplay();	

	// Clear all the Sounds
	audStreamSlot::CleanUpSlotsForReplay();
	//audController::GetController()->ClearAllSounds();	// I don't think we need this and it's affecting our interface sounds.
}

//========================================================================================================================
void CReplayMgrInternal::ProcessPlaybackAudio()
{
	CReplayState &state = GetCurrentPlayBackState();
	if(state.IsSet(REPLAY_STATE_PLAY) && state.IsSet(REPLAY_CURSOR_NORMAL) && state.IsSet(REPLAY_DIRECTION_FWD))
	{
		sm_AudioSceneStopTimer += fwTimer::GetTimeStep_NonPausedNonScaledClipped();

		// Delay the audio scenes stopping for a short time during edit mode to ensure that we don't get little
		// spikes of audio when scrubbing quickly back and forth
		if( VIDEO_RECORDING_ENABLED_ONLY( VideoRecording::IsRecording() || ) sm_AudioSceneStopTimer > sm_AudioSceneStopDelay)
		{
			if(sm_ffwdAudioScene)
			{
				sm_ffwdAudioScene->Stop();
				sm_ffwdAudioScene = NULL;
			}

			if(sm_rwdAudioScene)
			{
				sm_rwdAudioScene->Stop();
				sm_rwdAudioScene = NULL;
			}
		}				

#if USE_SRLS_IN_REPLAY
		if(!IsCreatingSRL())
#endif
		{
			RequestReplayMusic();
		}		

		g_RadioAudioEntity.RequestUpdateReplayMusic();
	}
	else
	{
		if(sm_desiredMode != REPLAYMODE_DISABLED)
		{
			sm_AudioSceneStopTimer = 0.0f;

			if(!sm_pPlaybackController || !sm_pPlaybackController->IsExportingToVideoFile())
			{
				if(state.IsSet(REPLAY_DIRECTION_FWD))
				{
					if(!sm_ffwdAudioScene)
					{
						MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("REPLAY_FFWD_SCENE", 0xacb90ba4)); 
						if(sceneSettings)
						{
							DYNAMICMIXER.StartScene(sceneSettings, &sm_ffwdAudioScene);
						}
					}
					else if(sm_rwdAudioScene)
					{
						sm_rwdAudioScene->Stop();
					}
				}
				else
				{
					if(!sm_rwdAudioScene)
					{
						MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("REPLAY_RWD_SCENE", 0x2D80FEDA)); 
						if(sceneSettings)
						{
							DYNAMICMIXER.StartScene(sceneSettings, &sm_rwdAudioScene);
						}
					}
					else if(sm_ffwdAudioScene)
					{
						sm_ffwdAudioScene->Stop();
					}
				}
			}			
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::RequestReplayMusic()
{
	if(sm_pPlaybackController)
	{
		s32 clipIndex = sm_pPlaybackController->GetCurrentClipIndex();

		if(clipIndex >= 0)
		{
			float const c_currentTimeMs = GetCurrentTimeMsFloat();
			float const c_playbackTimeMs = sm_pPlaybackController->ConvertNonDilatedTimeToTimeMs( clipIndex, c_currentTimeMs );

			s32 musicTrackID = sm_pPlaybackController->GetMusicIndexAtCurrentNonDilatedTimeMs( clipIndex, c_currentTimeMs );
			float startTime = (float)sm_pPlaybackController->GetMusicStartTimeMs( musicTrackID );
			float startOffset = c_playbackTimeMs - startTime;
			float trackDuration = (float)sm_pPlaybackController->GetMusicDurationMs( musicTrackID );
			const u32 musicSoundHash = sm_pPlaybackController->GetMusicSoundHash(musicTrackID);
			const bool isScore = (musicTrackID == -1 ? false : audRadioTrack::IsScoreTrack(musicSoundHash));
			const bool hasBeenTrimmed = CReplayAudioTrackProvider::GetMusicTrackDurationMs( musicSoundHash ) != trackDuration;
			const bool needToFadeEnd = hasBeenTrimmed;
			const bool needToFadeStart = isScore || hasBeenTrimmed;
			
			// The music can technically start *before* the replay starts if we have a negative start time (this just means that the music
			// starts on the first frame, already n ms into the track). The audio needs to know the track duration relative to the start of 
			// the actual asset, so count any negative start time towards the duration.
		
			float musicTrackStartOffset = float(sm_pPlaybackController->GetMusicStartOffsetMs( musicTrackID ));

			float musicStartTime =  startTime + musicTrackStartOffset;
			float musicEndTime = musicStartTime + trackDuration;

			const float timeRemaining = musicEndTime - c_playbackTimeMs;

			const float timePlayed = c_playbackTimeMs - Max(0.f, startTime + musicTrackStartOffset);

			if(musicTrackID >= 0)
			{
				const float trimmedTrackFadeInTime = isScore ? (float)REPLAY_MONTAGE_SCORE_MUSIC_FADEIN_TIME : (float)REPLAY_MONTAGE_MUSIC_FADE_TIME;
				const float trimmedTrackFadeOutTime = (float)REPLAY_MONTAGE_MUSIC_FADE_TIME;
				const float fadeOut = needToFadeEnd ? Min(1.f, timeRemaining / trimmedTrackFadeOutTime) : 1.f;
				const float fadeIn = needToFadeStart ? Min(1.f, timePlayed / trimmedTrackFadeInTime) : 1.f;
				const float fade = fadeIn * fadeOut;
				g_RadioAudioEntity.RequestReplayMusicTrack(musicSoundHash, musicTrackID, (u32)startOffset, (u32)musicEndTime, fade);
			}						
			else
			{
				g_RadioAudioEntity.StopReplayMusic();
			}

			//ambient track
			if(!audNorthAudioEngine::IsAudioUpdateCurrentlyRunning() && (CReplayMgr::IsPreCachingScene() || CReplayMgr::IsJustPlaying()))
			{
				s32 ambientTrackID = sm_pPlaybackController->GetAmbientTrackIndexAtCurrentNonDilatedTimeMs( clipIndex, c_currentTimeMs );
				const u32 ambientSoundHash = sm_pPlaybackController->GetAmbientTrackSoundHash(ambientTrackID);
				float ambientStartTime = (float)sm_pPlaybackController->GetAmbientTrackStartTimeMs(ambientTrackID);
				float ambientStartOffset = c_playbackTimeMs - ambientStartTime;
				u32 duration = CVideoProjectAudioTrackProvider::GetMusicTrackDurationMs(ambientSoundHash);
				if(duration > 0)
				{
					ambientStartOffset = fmodf(ambientStartOffset, (float)duration); 
				}
#if __BANK
				float ambientTrackDuration = (float)sm_pPlaybackController->GetAmbientTrackDurationMs(ambientTrackID);
				const bool ambientNeedToFadeEnd = true;
				const bool ambientNeedToFadeStart = true;

				float ambientTrackStartOffset = float(sm_pPlaybackController->GetAmbientTrackStartOffsetMs(ambientTrackID));

				float ambientBeginTime =  ambientStartTime + ambientTrackStartOffset;
				float ambientEndTime = ambientBeginTime + ambientTrackDuration;

				const float ambientTimeRemaining = ambientEndTime - c_playbackTimeMs;

				const float ambientTimePlayed = c_playbackTimeMs - Max(0.f, ambientStartTime + ambientTrackStartOffset);
#endif
				if(ambientTrackID >= 0)
				{
#if __BANK
					const float ambientTrimmedTrackFadeInTime = (float)REPLAY_MONTAGE_AMBINET_FADE_TIME;
					const float ambientTrimmedTrackFadeOutTime = (float)REPLAY_MONTAGE_AMBINET_FADE_TIME;
					const float ambientFadeOut = ambientNeedToFadeEnd ? Min(1.f, ambientTimeRemaining / ambientTrimmedTrackFadeOutTime) : 1.f;
					const float ambientFadeIn = ambientNeedToFadeStart ? Min(1.f, ambientTimePlayed / ambientTrimmedTrackFadeInTime) : 1.f;
					const float ambientFade = ambientFadeIn * ambientFadeOut;
#endif
					// preload and play ambient track
					g_ReplayAudioEntity.Preload(ambientTrackID, ambientSoundHash, REPLAY_STREAM_AMBIENT, (u32)ambientStartOffset);
					if(CReplayMgr::IsJustPlaying() && ambientSoundHash != g_NullSoundHash && g_ReplayAudioEntity.IsAmbientReadyToPlay(ambientTrackID, ambientSoundHash))
					{
#if __BANK
						Displayf("Play ambinet track %d index %d fade %f", ambientSoundHash, ambientTrackID, ambientFade);
#endif
						g_ReplayAudioEntity.PlayAmbient(ambientTrackID, ambientSoundHash);
					}
				}						
				g_ReplayAudioEntity.StopExpiredAmbientTracks(ambientTrackID);
			}
		}
	}
	else
	{
		g_RadioAudioEntity.StopReplayMusic();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::Process()
{
	

	PF_AUTO_PUSH_TIMEBAR("Replay-Process");

	if(IsEditModeActive() && (sm_interfaceManager.HasBatchedEntitiesToProcess()))
	{
		// Bail for a frame so we can continue adding in entities
		return;
	}

	// Lock to prevent any event packets being written in...
	//PacketRecordWait();

	sm_FileManager.Process();

	// Record the state of the world just before rendering.  Everything
	// should be processed by this point and be in its final state to be 
	// shown on screen.
	//ProcessRecord();

	// Unlock...event packets may now be written
	//PacketRecordSignal();

	// Same reason as above for the playback.  Leave it 'til the very last 
	// minute for the replay system to playback so that nothing can override
	// our entities.
	ProcessPlayback();

	// Reset our per-frame enabled events flag, if we just went high, record the time
	if( sm_EnableEventsThisFrame == false  )
	{
		if( sm_EnableEventsCooldown == 0 )
		{
			// Get the time this was first latched high
			UpdateFrameTimeScriptEventsWereDisabled();					/// UPDATE last not recording time.

			// Update any start times to the disabled time
			ReplayBufferMarkerMgr::ClipUnprocessedMarkerStartTimesToCurrentEndTime(false);

			CReplayMgr::SetMissionName(atString(""));
			CReplayMgr::SetFilterString(atString(""));
		}
		else
		{
			--sm_EnableEventsCooldown;
		}
	}

	if(sm_SuppressEventsThisFrame == false && sm_SuppressEventsLastFrame == true)
	{
#if __BANK
		replayDebugf3("STOPPED SUPPRESSING REPLAY callstack: %s", sm_PreventCallstack.c_str());
#endif
	}
	else if( sm_SuppressEventsThisFrame == true && sm_SuppressEventsLastFrame == false )
	{
#if __BANK
		replayDebugf3("STARTED SUPPRESSING REPLAY callstack: %s", sm_PreventCallstack.c_str());
#endif
		ReplayBufferMarkerMgr::ClipUnprocessedMarkerEndTimesToCurrentEndTime(true);
	}
	else if(sm_SuppressEventsThisFrame == true)
	{
		UpdateFrameTimeAllEventsWereSuppressed();						/// UPDATE last suppressed time

		// Update any start times to the disabled time
		ReplayBufferMarkerMgr::ClipUnprocessedMarkerStartTimesToCurrentEndTime(true);
	}

	//Enable Events - We've just turned off the events capture, so we need to clip any unprocessed marker end times to the current time.
	if( sm_EnableEventsThisFrame == false && sm_EnableEventsLastFrame == true )
	{
		ReplayBufferMarkerMgr::ClipUnprocessedMarkerEndTimesToCurrentEndTime(false);
	}

	ReplayRecordingScriptInterface::AutoStopRecordingIfNecessary();

	//Print out some logging if the camera movement was turned on/off
	if( sm_SuppressCameraMovementThisFrame == false && sm_SuppressCameraMovementLastFrame == true)
	{
#if __BANK
		replayDebugf1("STOPPED SUPPRESSING CAMERA MOVEMENT callstack: %s", sm_SuppressCameraMovementCallstack.c_str());
#endif // __BANK
	}
	else if( sm_SuppressCameraMovementThisFrame == true && sm_SuppressCameraMovementLastFrame == false )
	{
		//Put out a feed message if camera movement was disabled.
		CReplayControl::ShowCameraMovementDisabledWarning();

#if __BANK
		replayDebugf1("STARTED SUPPRESSING CAMERA MOVEMENT callstack: %s", sm_SuppressCameraMovementCallstack.c_str());
#endif // __BANK
	}

#if __BANK
	sm_SuppressCameraMovementByScript = false;
#endif // __BANK

	sm_SuppressCameraMovementLastFrame = sm_SuppressCameraMovementThisFrame;

	sm_EnableEventsLastFrame = sm_EnableEventsThisFrame;
	sm_EnableEventsThisFrame = false;
	sm_SuppressEventsLastFrame = sm_SuppressEventsThisFrame;
	sm_SuppressEventsThisFrame = false;

	static int tickerID = -1;
	if(sm_ReplayBufferInfo.GetTempBlocksHaveLooped())
	{
		if(tickerID == -1)
		{
			CGameStreamMgr::GetGameStream()->FreezeNextPost();
			tickerID = CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REPLAY_SAVE_CLIP_LONG"), true);
		}
	}
	else if(tickerID != -1)
	{
		CGameStreamMgr::GetGameStream()->RemoveItem(tickerID);
		tickerID = -1;
	}

	if(IsEditModeActive())
	{
		sm_interfaceManager.Process();
	}
}

void CReplayMgrInternal::UpdateFrameTimeScriptEventsWereDisabled()
{
	sm_FrameTimeScriptEventsWereDisabled = rage::fwTimer::GetReplayTimeInMilliseconds();
}

void CReplayMgrInternal::UpdateFrameTimeAllEventsWereSuppressed()
{
	sm_FrameTimeEventsWereSuppressed = rage::fwTimer::GetReplayTimeInMilliseconds();
}

void CReplayMgrInternal::SaveMarkedUpClip(MarkerBufferInfo& info)
{
	replayDebugf1("Replay Save triggered... %d markers", info.eventMarkers.GetCount());
	BANK_ONLY(sysStack::PrintStackTrace());
	REPLAY_CHECK(sm_pPreviousFramePacket != NULL, NO_RETURN_VALUE, "Save was called without having actually recorded a frame!");

	markerBufferInfo = info;
	sm_IsMarkedClip = true;

	WaitForRecordingFinish();
	// Tell replay to start recording into the temp buffer, so as not to overwrite any currently marked up blocks
	SetTempBuffer();

	sm_doSave = true;

	// Store off the current frame hash
	sm_currentFrameHash = sm_interfaceManager.GetFrameHash();
}

bool CReplayMgrInternal::ShouldSustainRecording()
{
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && pPlayer->GetSpeechAudioEntity() && 
		(pPlayer->GetSpeechAudioEntity()->IsScriptedSpeechPlaying() || pPlayer->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() 
		|| pPlayer->GetSpeechAudioEntity()->IsPainPlaying() || g_ScriptAudioEntity.IsPlayerConversationActive(pPlayer, sm_ShouldSustainRecordingLine)))
	{
		return true;
	}
	return false;
}

void CReplayMgrInternal::ProcessInputs(const ReplayInternalControl& control)
{
	if(control.m_KEY_R)
	{
		if(IsEditModeActive() == false && sm_uMode != REPLAYMODE_WAITINGFORSAVE)
		{
			if(IsRecordingEnabled())
			{
				BANK_ONLY(sm_bRecordingEnabledInRag = false;)
				sm_desiredMode = REPLAYMODE_DISABLED;
			}
			else
			{		
				BANK_ONLY(sm_bRecordingEnabledInRag = true;)
				sm_desiredMode = REPLAYMODE_RECORD;
			}
		}
	}

	if(control.m_KEY_F1)
	{
		SaveCallback();
	}

#if __BANK
	if(control.m_KEY_S)
	{
		if(IsEditModeActive() == false)
		{
			sm_bRecordingEnabledInRag = false;
			RecordCallback();
			CGameStreamMgr::GetGameStream()->PostTicker("Replay: Stop Recording", false);	
		}
	}

	if(control.m_KEY_SPACE)
	{
		if(GetLastPlayBackState().IsSet(REPLAY_STATE_PLAY) && IsEditModeActive())
		{
			PauseCallback();
			CGameStreamMgr::GetGameStream()->PostTicker("Replay: Pause", false);
		}
		else if(CGameLogic::IsGameStateInPlay() && camInterface::IsFadedIn())
		{
			PlayCallback();
			CGameStreamMgr::GetGameStream()->PostTicker("Replay: Play", false);
		}
	}

#if GTA_REPLAY_OVERLAY
	if(control.m_KEY_F3)
	{
		CReplayOverlay::IncrementOverlayMode();
	}
#endif //GTA_REPLAY_OVERLAY

	if(IsEditModeActive())
	{
		if(control.m_KEY_Q)
		{
			QuitReplayCallback();
			CGameStreamMgr::GetGameStream()->PostTicker("Replay: Quit Playback", false);
		}

		if(control.m_KEY_F5)
		{
			LoadCallback();
			CGameStreamMgr::GetGameStream()->PostTicker("Replay: Load Replay", false);
		}

		bool isRightDown = control.m_KEY_RIGHT;
		bool isLeftDown = control.m_KEY_LEFT;

		static bool usingManualDelta = false;
		static bool wasSeeking = false;

		if(isRightDown || isLeftDown)
		{
			usingManualDelta = false;
			wasSeeking = true;
			SetCursorSpeed(isRightDown ? 2.0f : -2.0f);
		}
		else if(wasSeeking)
		{	
			wasSeeking = false;
			ResumeNormalPlayback();
		}
		else
		{
			static const float deltaIncrease = 0.1f;

			if(control.m_KEY_UP)
			{
				float currentSpeed = GetCursorSpeed();
				currentSpeed += deltaIncrease;
				
				SetCursorSpeed(currentSpeed);
			}
			else if(control.m_KEY_DOWN)
			{
				float currentSpeed = GetCursorSpeed();
				currentSpeed -= deltaIncrease;

				SetCursorSpeed(currentSpeed);
			}
			else if(control.m_KEY_RETURN)
			{
				ResumeNormalPlayback();
			}
		}
	}
#endif
	if(IsEnabled() && sm_doSave)
	{
		if(!IsSaving())
		{
			SaveClip();
		}
		sm_IsMarkedClip = false;
		sm_doSave		= false;
		sm_postSaveFrameCount = 0;
	}

}

//========================================================================================================================
void CReplayMgrInternal::ProcessInputs()
{
	ReplayInternalControl control;
	control.m_KEY_R				= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_REPLAY);
	control.m_KEY_S				= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_REPLAY);
	control.m_KEY_ADD			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_ADD, KEYBOARD_MODE_REPLAY);
	control.m_KEY_NUMPADENTER	= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_NUMPADENTER, KEYBOARD_MODE_REPLAY);
	control.m_KEY_SPACE			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_SPACE, KEYBOARD_MODE_REPLAY);
	control.m_KEY_F1			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F1, KEYBOARD_MODE_REPLAY);
	control.m_KEY_F3			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F3, KEYBOARD_MODE_REPLAY);
	control.m_KEY_Q				= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_Q, KEYBOARD_MODE_REPLAY);
	control.m_KEY_F5			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F5, KEYBOARD_MODE_REPLAY);
	control.m_KEY_RIGHT			= CControlMgr::GetKeyboard().GetKeyDown(KEY_RIGHT, KEYBOARD_MODE_REPLAY);
	control.m_KEY_LEFT			= CControlMgr::GetKeyboard().GetKeyDown(KEY_LEFT, KEYBOARD_MODE_REPLAY);
	control.m_KEY_UP			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_REPLAY);
	control.m_KEY_DOWN			= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_REPLAY);
	control.m_KEY_RETURN		= CControlMgr::GetKeyboard().GetKeyDown(KEY_RETURN, KEYBOARD_MODE_REPLAY);
	
	ProcessInputs(control);
}

//========================================================================================================================
bool CReplayMgrInternal::OnSaveCompleteFunc(int retCode)
{
	if(retCode != REPLAY_ERROR_SUCCESS)
	{
		CGameStreamMgr::GetGameStream()->SetImportantParamsRGBA(255, 0, 0, 100);
		CGameStreamMgr::GetGameStream()->SetImportantParamsFlashCount(5);
		CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REPLAY_SAVE_CLIP_FAILED"), true);
		CGameStreamMgr::GetGameStream()->ResetImportantParams();

		replayDebugf1("Clip failed to save with error code: %i", retCode);
	}

	sm_BlocksToSave.clear();

	if(sm_pSaveResultCB)
	{
		sm_pSaveResultCB(retCode == REPLAY_ERROR_SUCCESS);
		sm_pSaveResultCB = NULL;
	}

	if(markerBufferInfo.callback)
	{
		if( !markerBufferInfo.callback(retCode) )
		{
			markerBufferInfo.callback = NULL;
		}
	}

#if __BANK
	atArray<const char*> clipNames;
	GetClipNames(clipNames);
	if( clipNames.GetCount() > 0 )
	{
		sm_ReplayClipNameCombo->UpdateCombo("Clip Selector", &sm_ClipIndex, clipNames.GetCount(), clipNames.GetElements(), ClipSelectorCallBack);
	}
#endif//__BANK

	sm_LastErrorCode = (eReplayFileErrorCode)retCode;

	g_ptFxManager.ResetReplayScriptRequestAsset();

	return true;
}

//========================================================================================================================
void CReplayMgrInternal::SaveClip()
{
	sm_LastErrorCode = REPLAY_ERROR_SUCCESS;

	ReplayHeader replayHeader;
	replayHeader.Reset();
	replayHeader.PhysicalBlockCount	= (u16)sm_BlocksToSave.GetCount();
	replayHeader.PhysicalBlockSize	= BytesPerReplayBlock;
	replayHeader.bCompressed		= sm_ClipCompressionEnabled;
	replayHeader.uImportance		= markerBufferInfo.importance;
	replayHeader.uMission			= markerBufferInfo.mission;
	replayHeader.MarkerCount		= markerBufferInfo.eventMarkers.GetCount();

	replayHeader.wasModifiedContent	= sm_wasModifiedContent;

	for( int i = 0; i < markerBufferInfo.eventMarkers.GetCount(); ++i)
	{
		replayHeader.eventMarkers[i] = markerBufferInfo.eventMarkers[i];
	}

	replayHeader.PlatformType = ReplayHeader::GetCurrentPlatform();

	// Store the ExtraContent hashes.
	atArray<u32> extraContentHashes;
	extraContentHashes.Reserve(MAX_EXTRACONTENTHASHES);
	EXTRACONTENT.GetMapChangeArray(extraContentHashes);

	replayHeader.extraContentHashCount = Min((u32)extraContentHashes.GetCount(), MAX_EXTRACONTENTHASHES);
	memcpy(replayHeader.extraContentHashes, extraContentHashes.GetElements(), replayHeader.extraContentHashCount * sizeof(u32));
	

	CSavingMessage::BeginDisplaying( CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING );

	atString dateTimeStr;
	u32 uid;
	ReplayHeader::GenerateDateString(dateTimeStr, uid);
	strcpy_s(replayHeader.creationDateTime, REPLAY_DATETIME_STRING_LENGTH, dateTimeStr.c_str());

	replayHeader.previousSequenceHash = sm_previousFrameHash;
	replayHeader.nextSequenceHash = sm_currentFrameHash;
	replayHeader.ActiveIslandHash = CIslandHopper::GetInstance().GetCurrentActiveIsland();

	sm_previousFrameHash = sm_currentFrameHash;

	// Lock this thumbnail so it can't be used until it's been saved out correctly.
	if(sm_BlocksToSave[0].GetThumbnailRef().IsInitialAndQueued() && sm_BlocksToSave[0].GetThumbnailRef().GetThumbnail().IsPopulated())
		sm_BlocksToSave[0].GetThumbnailRef().GetThumbnail().SetLocked(true);

	if( !sm_FileManager.StartSave(replayHeader, sm_BlocksToSave, OnSaveCompleteFunc) )
	{
		OnSaveCompleteFunc(REPLAY_ERROR_FAILURE);
	}
}

//========================================================================================================================
void CReplayMgrInternal::SaveAllBlocks()
{
	replayDebugf1("Replay Save triggered...");
	BANK_ONLY(sysStack::PrintStackTrace());
	REPLAY_CHECK(sm_pPreviousFramePacket != NULL, NO_RETURN_VALUE, "Save was called without having actually recorded a frame!");

	sm_BlocksToSave.clear();

	CBlockProxy firstBlock = sm_ReplayBufferInfo.FindFirstBlock();
	CBlockProxy block = firstBlock;
	do 
	{
		if(!SaveBlock(block))
		{
			break;
		}

		block = block.GetNextBlock();
	} while (block.IsValid() && block != firstBlock);
	
	// Tell replay to start recording into the temp buffer, so as not to overwrite any currently marked up blocks
	SetTempBuffer();

	sm_doSave = true;
}

//========================================================================================================================
bool CReplayMgrInternal::SaveBlock(CBlockProxy block)
{	
	if(block.IsValid() == false)
		return false;

	CBlockInfo* pBlock = block.GetBlock();
	if( pBlock->GetStatus() == REPLAYBLOCKSTATUS_EMPTY )
	{
		return false;
	}
	
	if(sm_BlocksToSave.size() >= NumberOfReplayBlocks)
	{
		replayAssertf(false, "Trying to save too many blocks!");
		return false;
	}

	if( pBlock != NULL )
	{
		if( sm_BlocksToSave.Find(block) == -1 )
		{
			sm_BlocksToSave.PushAndGrow(block);

			return true;
		}
	}

	return false;
}

//========================================================================================================================
void CReplayMgrInternal::ClearSaveBlocks()
{
	sm_BlocksToSave.clear();
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::IsBlockMarkedForSave(CBlockProxy block)
{
	if(block.IsValid() == false)
		return false;

	return sm_BlocksToSave.Find(block) != -1;
}

//========================================================================================================================
const atArray<CBlockProxy>& CReplayMgrInternal::GetBlocksToSave()
{
	return sm_BlocksToSave;
}

//========================================================================================================================
u32 CReplayMgrInternal::GetPacketInfoInSaveBlocks(atArray<CBlockProxy>& blocksToSave, u32& uStartTime, u32& uEndTime, u32& uFrameFlags)
{
	if( blocksToSave.GetCount() == 0 )
	{
		replayAssertf(false, "Block count is 0, Returning 0 clip length!");

		return 0;
	}

	CAddressInReplayBuffer startAddress(blocksToSave[0].GetBlock(), 0);

	CBlockProxy lastBlock = blocksToSave[blocksToSave.GetCount()-1];
	CAddressInReplayBuffer endAddress = FindEndOfBlock(CAddressInReplayBuffer(lastBlock.GetBlock(), 0));

	replayDebugf2("Saving from block %u (position %u) to block %u (position %u)", startAddress.GetMemoryBlockIndex(), startAddress.GetPosition(), endAddress.GetMemoryBlockIndex(), endAddress.GetPosition());
	
	ReplayController controller(startAddress, sm_ReplayBufferInfo);
	controller.GetPlaybackFlags().SetState( REPLAY_DIRECTION_FWD );
	controller.EnablePlayback();

	bool hasReachedEnd = false;

	uStartTime = 0xffffffff;
	uEndTime = 0;
	uFrameFlags = 0;

	while(hasReachedEnd == false && (controller.GetCurrentBlockIndex() != endAddress.GetMemoryBlockIndex() || controller.GetCurrentPosition() < endAddress.GetPosition()))
	{
		if(controller.IsEndOfBlock())
		{
			hasReachedEnd = controller.IsLastBlock();
			controller.AdvanceBlock();
			continue;
		}
		
		if (controller.GetCurrentPacketID() == PACKETID_GAMETIME)
		{
			CPacketGameTime const* pPacketGameTime = controller.ReadCurrentPacket<CPacketGameTime>();
			if(pPacketGameTime->GetGameTime() > uEndTime)
			{
				uEndTime = pPacketGameTime->GetGameTime();
			}
			if(pPacketGameTime->GetGameTime() < uStartTime)
			{
				uStartTime = pPacketGameTime->GetGameTime();
			}
		}
		// check for any restrictions
		else if (controller.GetCurrentPacketID() == PACKETID_FRAME)
		{ 
			CPacketFrame const* pPacketFrame = controller.ReadCurrentPacket<CPacketFrame>(); 
			uFrameFlags |= pPacketFrame->GetFrameFlags();
		}

		CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();
		if (!replayVerifyf(pPacket, "pPacket is NULL!"))
		{
			break;
		}
		controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
	}

	controller.DisablePlayback();

	replayDebugf3("Start Time: %u", uStartTime);
	replayDebugf3("End Time: %u",	uEndTime);
	replayDebugf3("Length: %u",		(uEndTime - uStartTime));
	
	replayDebugf3("Number Of Blocks: %u", blocksToSave.GetCount());
	replayDebugf3("last Block End Time: %u", lastBlock.GetEndTime());

	return uEndTime - uStartTime;
}

//========================================================================================================================
bool CReplayMgrInternal::LoadStartFunc(ReplayHeader& pHeader)
{
	sm_uPlaybackStartTime = 0xffffffff;
	sm_bShouldSetStartTime = true;

	//Grab the start time from the header event markers.	
	u32 markerStartTime = pHeader.eventMarkers[0].GetStartTime();

	//only set the start time if the marker is valid
	//otherwise we default to the previous behaviour	
	if( markerStartTime != 0 )
	{
		sm_uPlaybackStartTime = pHeader.eventMarkers[0].GetClipStartTime();
		sm_bShouldSetStartTime = false;
	}

	EXTRACONTENT.SetReplayState(pHeader.extraContentHashes, pHeader.extraContentHashCount);

	sm_islandActivated = ATSTRINGHASH("", 0);
	if(pHeader.uHeaderVersion >= HEADERVERSION_V5)
	{
		sm_islandActivated = pHeader.ActiveIslandHash;
	}

	// For url:bugstar:5797430 - see if we're loading a clip that references the vinewood update
	sm_shouldLoadReplacementCasinoImaps = true;

	// Assuming extracontent only in MP
	if(pHeader.extraContentHashCount == 0)
	{
		sm_shouldLoadReplacementCasinoImaps = false;
	}

	for(int i = 0; i < pHeader.extraContentHashCount; ++i)
	{
		if(pHeader.extraContentHashes[i] == 0xf067593e/*"MPVINEWOOD_MAP_UPDATE"*/)
		{
			sm_shouldLoadReplacementCasinoImaps = false;
			break;
		}
	}

	//we don't need to re alloc memory unless we have none or the sizes have changed
	if(IsEnabled() == false || pHeader.PhysicalBlockSize != BytesPerReplayBlock || pHeader.PhysicalBlockCount != NumberOfReplayBlocks || !sm_IsMemoryAllocated || NumberOfTempReplayBlocks > 0)
	{
		BytesPerReplayBlock			= pHeader.PhysicalBlockSize;
		NumberOfReplayBlocks		= pHeader.PhysicalBlockCount;
		NumberOfTempReplayBlocks	= 0;
		TotalNumberOfReplayBlocks	= NumberOfReplayBlocks + NumberOfTempReplayBlocks;
		TempBlockIndex				= NumberOfReplayBlocks;

		replayDebugf1("Loading clip with %u blocks @ %u bytes each", NumberOfReplayBlocks, BytesPerReplayBlock);

		// Tell the main thread we're loading from file now so it can 
		// allocate memory
		sm_bFileRequestMemoryAlloc = true;

		// Wait for the main thread to allocate memory
		sysIpcWaitSema(sm_LoadOpWaitForMem);

		if(sm_bReplayEnabled == false)
			return false;
	}
	else
	{
		SetupReplayBuffer(pHeader.PhysicalBlockCount, 0);
	}

	return true;
}

//========================================================================================================================
bool CReplayMgrInternal::OnLoadCompleteFunc(int retCode)
{
	sm_TriggerPlayback = retCode == REPLAY_ERROR_SUCCESS;

	sm_LastErrorCode = (eReplayFileErrorCode)retCode;

	if(DidLastFileOpFail())
	{	// The last load failed so set to disabled so we can hopefully try again?
		sm_desiredMode = REPLAYMODE_DISABLED;
		sm_eReplayState.SetState(REPLAY_STATE_PAUSE);
		sm_nextReplayState.SetState(REPLAY_STATE_PAUSE);

		replayDebugf1("Failed to load clip %s...skipping", ((sm_pPlaybackController) ? sm_pPlaybackController->GetCurrentRawClipFileName() : "<unknown - exiting?>"));
	}
	return true;
}

//========================================================================================================================
void CReplayMgrInternal::LoadClip( u64 const ownerId, const char* filename)
{	
	replayDebugf1("Loading Clip...%s", filename);
	sysStack::PrintStackTrace();
	sm_desiredMode = REPLAYMODE_LOADCLIP;

	// REPLAY_CLIPS_PATH change
	char clipPath[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectoryForUser( ownerId, clipPath );
	sm_fileToLoad = atString(clipPath);
	sm_fileToLoad += filename;

	sm_lastLoadedClip = filename;

	sm_eLoadingState = REPLAYLOADING_CLIP;
	sm_LastErrorCode = REPLAY_ERROR_SUCCESS;
}

//========================================================================================================================
s32 CReplayMgrInternal::GetMissionIDFromClips(const char* /*missionFilter*/)
{
	FileDataStorage fileDataStorage;
	/*CReplayMgr::EnumerateFiles(missionFilter, fileDataStorage);*/

	for( int i = 0; i < fileDataStorage.GetCount(); ++i )
	{
		atString filename = atString(fileDataStorage[i].getFilename());
		int startindex = filename.LastIndexOf("_");
		int endindex = filename.LastIndexOf(".");
		if(startindex != -1 && endindex != -1)
		{
			atString missionIndex;
			missionIndex.Set(filename.c_str(), filename.length(), startindex + 1, endindex - startindex - 1);
			if(missionIndex.length() > 0)
			{
				return atoi(missionIndex);
			}
		}
	}
	return -1;
}

//========================================================================================================================
s32 CReplayMgrInternal::CalculateCurrentMissionIndex()
{
	int index = 0;
	const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
	const sStatData* pActualStat = statsDataMgr.GetStat(STAT_MISSIONS_PASSED.GetHash(CHAR_MICHEAL));
	const sStatData* pActualStat1 = statsDataMgr.GetStat(STAT_MISSIONS_PASSED.GetHash(CHAR_FRANKLIN));
	const sStatData* pActualStat2 = statsDataMgr.GetStat(STAT_MISSIONS_PASSED.GetHash(CHAR_TREVOR));
	if(pActualStat)
	{
		index += pActualStat->GetInt();
	}
	if(pActualStat1)
	{
		index += pActualStat1->GetInt();
	}
	if(pActualStat2)
	{
		index += pActualStat2->GetInt();
	}

	return index;
}

void CReplayMgrInternal::SetWasModifiedContent()
{
#if RSG_PC
	sm_wasModifiedContent = true;
#endif
}

void CReplayMgrInternal::UpdateAvailableDiskSpace()
{
	ReplayFileManager::UpdateAvailableDiskSpace();
}

u64 CReplayMgrInternal::GetAvailiableDiskSpace()
{
	return ReplayFileManager::GetAvailableDiskSpace();
}

bool CReplayMgrInternal::WaitingForDataToLoad()
{
	bool waitingForLoad = false;

	bool loadedAudioDataSet = (!sm_bMultiplayerClip && audNorthAudioEngine::HasLoadedSPDataSet()) || (sm_bMultiplayerClip && audNorthAudioEngine::HasLoadedMPDataSet());	
	waitingForLoad = !loadedAudioDataSet;

	return waitingForLoad;
}

//========================================================================================================================
void CReplayMgrInternal::ProcessPlayback()
{
	if(!IsEditModeActive() && DidLastFileOpFail() == false)
		return;

	UpdateClip();
	if( IsClipTransition() || IsLoading() || WaitingForDataToLoad() || sm_desiredMode == REPLAYMODE_DISABLED)
	{
		return;
	}

	PF_AUTO_PUSH_TIMEBAR("Replay-ProcessPlayback");
	
	if (sm_uMode == REPLAYMODE_EDIT)
	{
		PostFX::SetUseNightVision(false);
		const CPacketFrame* pCurrFramePacket = sm_currentFrameInfo.GetFramePacket();
		u32 frameFlags = 0;
		if(pCurrFramePacket)
		{
			frameFlags = pCurrFramePacket->GetFrameFlags();

			if(pCurrFramePacket->GetPacketVersion() >= CPacketFrame_V1)
			{
				CRenderer::SetDisableArtificialLights(pCurrFramePacket->GetShouldDisableArtificialLights());

				if(pCurrFramePacket->GetPacketVersion() >= CPacketFrame_V2)
				{
					PostFX::SetUseNightVision(pCurrFramePacket->GetShouldUseNightVision() && IsUsingRecordedCamera());

					if(pCurrFramePacket->GetPacketVersion() >= CPacketFrame_V3)
					{
						CRenderer::SetDisableArtificialVehLights(pCurrFramePacket->GetShouldDisableArtificialVehLights());
					}
				}
			}
		}
#if GTA_REPLAY_OVERLAY
		CReplayOverlay::UpdatePlayback(GetCurrentSessionFrameRef(), sm_replayFrameData, frameFlags, sm_currentFrameInfo.GetAddress(), sm_ReplayBufferInfo, sm_interfaceManager, sm_eventManager, *sm_pAdvanceReader, GetAvailiableDiskSpace());
#endif //GTA_REPLAY_OVERLAY

		if(sm_eReplayState.IsSet(REPLAY_STATE_PLAY))
		{
			if(GetLastPlayBackState().IsSet(REPLAY_STATE_PLAY) == false)
			{
				sm_uTimePlaybackBegan = GetCurrentTimeAbsoluteMs();
			}
		}

		PlayBackThisFrame();

		if(sm_eReplayState.IsSet(REPLAY_STATE_PLAY))
		{
			g_FrontendAudioEntity.SetReplayMuteVolume( 0.0f );			
		}

		ProcessPlaybackAudio();

		{	// Replay of movies
			CReplayState &theState = GetCurrentPlayBackState();
			bool	playingForward = ( theState.IsSet(REPLAY_STATE_PLAY) && theState.IsSet(REPLAY_CURSOR_NORMAL) && theState.IsSet(REPLAY_DIRECTION_FWD) && CReplayMgr::GetMarkerSpeed() == 1.0f );
			ReplayMovieController::Process(playingForward);
			ReplayMovieControllerNew::Process(playingForward);
		}

#if __BANK
		//this signifying to the panoramic capture code that we've finished updating the replay for this time step
		if(LightProbe::IsCapturingPanorama() && IsPlaybackPlaying())
		{
			LightProbe::OnFinishedReplayTick();
		}
#endif
	}
}

//========================================================================================================================
void CReplayMgrInternal::ProcessRecord()
{
	PF_AUTO_PUSH_DETAIL("CReplayMgrInternal::ProcessRecord");

	if(IsAwareOfWorldForRecording())
	{
		ReplayMovieControllerNew::Record();

		sm_eventManager.CheckMonitoredEvents(sm_uMode != REPLAYMODE_RECORD);
		sm_eventManager.CheckAllTrackablesDuringRecording();
	}

	g_decalMan.ResetReplayDecalEvents();

	if (sm_uMode != REPLAYMODE_RECORD)
	{
#if GTA_REPLAY_OVERLAY
		if(sm_uMode == REPLAYMODE_DISABLED)
		{
			CReplayOverlay::UpdateRecord(FrameRef(), sm_oRecord, sm_ReplayBufferInfo, sm_interfaceManager, sm_eventManager, *sm_pAdvanceReader, GetAvailiableDiskSpace(), false, sm_OverwatchEnabled);
		}
#endif //GTA_REPLAY_OVERLAY

		//Make sure frame flags are reset even if we're not recording.
		ResetRecordingFrameFlags();
		return;
	}
	
#if __BANK
	if(SettingsChanged)
	{
		sm_replayBlockPosSlider->SetRange(0.0f,		(float)BytesPerReplayBlock);
		sm_replayBlockSlider->SetRange(0.0f,		(float)NumberOfReplayBlocks);
	}
#endif // __BANK

	if( !IsRecording() || CReplayControl::IsPlayerOutOfControl() )
	{
		sm_eventManager.CheckMonitoredEvents(true);
		sm_eventManager.CheckAllTrackablesDuringRecording();

		//Make sure frame flags are reset even if we're not recording.
		ResetRecordingFrameFlags();
		return;
	}

	if (sm_uMode == REPLAYMODE_RECORD)
	{
		PF_PUSH_TIMEBAR_BUDGETED("Replay-ProcessRecord", 1);
#if GTA_REPLAY_OVERLAY
		CReplayOverlay::UpdateRecord(GetCurrentSessionFrameRef(), sm_oRecord, sm_ReplayBufferInfo, sm_interfaceManager, sm_eventManager, *sm_pAdvanceReader, GetAvailiableDiskSpace(), true, sm_OverwatchEnabled);
#endif //GTA_REPLAY_OVERLAY
		
		//////////////////////////////////////////////////////////////////////////
		// Figure out whether we need to record this frame...
		bool doRecord = !sm_lockRecordingFrameRate;

		if(sm_lockRecordingFrameRate != sm_lockRecordingFrameRatePrev)
		{
			sm_lockRecordingFrameRatePrev = sm_lockRecordingFrameRate;
			replayDebugf1("*** Recording frame lock has been changed to %s ***", sm_lockRecordingFrameRate ? "On" : "Off");
		}

		if(sm_numRecordedFramesPerSec != sm_numRecordedFramesPerSecPrev)
		{
			sm_numRecordedFramesPerSecPrev = sm_numRecordedFramesPerSec;
			replayDebugf1("*** Recording frame rate has been changed to %d frames per second ***", sm_numRecordedFramesPerSec);
		}

		if(!doRecord)
		{
			const float step = (1000.0f / (float)Max(sm_numRecordedFramesPerSec, 1));
			float frameStep = fwTimer::GetTimeStep_NonScaledClipped() * 1000.0f;

			sm_recordingFrameCounter += frameStep;
			while(sm_recordingFrameCounter > step)
			{
				sm_recordingFrameCounter -= step;
				doRecord = true;
			}
		}

		if(doRecord)
		{
			if(sm_recordingThreaded)
			{
				WaitForRecordingFinish();

				CBlockInfo* pCurrBlock = sm_oRecord.GetBlock();
				sm_recordingBufferProgress = ((float)(pCurrBlock->GetSessionBlockIndex()) / (float)sm_ReplayBufferInfo.GetNumNormalBlocks());

				sm_storingEntities = true;
				sm_recordingThreadRunning = true;
				sm_currentSessionBlockID = sm_oRecord.GetSessionBlockIndex();

				for (u32 i = 0; i < MAX_REPLAY_RECORDING_THREADS; i++)
					sysIpcSignalSema(sm_entityStoringStart[i]);
			}
			else
			{
				RecordFrame(false);
			}
		}
	

	#if __BANK
		u32 uFlags =	(REPLAY_BANK_FRAMES_ELAPSED|
						REPLAY_BANK_SECONDS_ELAPSED|
						REPLAY_BANK_ENTITIES_BEFORE_REPLAY|
						REPLAY_BANK_PFX_EFFECT_LIST|
						REPLAY_BANK_ENTITIES_TO_DEL);
		UpdateBanks(uFlags);
	#endif // __BANK		
		
		PF_POP_TIMEBAR();
	}

}

//////////////////////////////////////////////////////////////////////////
float CReplayMgrInternal::GetNextAudioBeatTime( float const* overrideClipTimeMs )
{
	float nextBeat = -1.f;

	if(sm_pPlaybackController)
	{
		s32 clipIndex = sm_pPlaybackController->GetCurrentClipIndex();

		if(clipIndex >= 0)
		{
			float const c_currentTimeMs = overrideClipTimeMs ? *overrideClipTimeMs : GetCurrentTimeMsFloat();
			float const c_playbackTimeMs = sm_pPlaybackController->ConvertNonDilatedTimeToTimeMs( clipIndex, c_currentTimeMs );

			s32 musicTrackID = sm_pPlaybackController->GetNextMusicIndexFromCurrentNonDilatedTimeMs( clipIndex, c_currentTimeMs );

			if(musicTrackID >= 0)
			{
				u32 soundHash = sm_pPlaybackController->GetMusicSoundHash(musicTrackID);
				float const c_musicStartTimeMs = (float)sm_pPlaybackController->GetMusicStartTimeMs( musicTrackID ) + (float)sm_pPlaybackController->GetMusicStartOffsetMs( musicTrackID );
				float const c_soundDuration = (float)sm_pPlaybackController->GetMusicDurationMs( musicTrackID );
				float const c_musicEndTimeMs = c_musicStartTimeMs + c_soundDuration;

				if( c_playbackTimeMs < c_musicStartTimeMs )
				{
					nextBeat = c_musicStartTimeMs + REPLAY_JUMP_EPSILON;
				}
				else if( c_playbackTimeMs >= c_musicEndTimeMs )
				{
					nextBeat = c_musicEndTimeMs;
				}
				else
				{
					float const c_startOffset = c_playbackTimeMs - c_musicStartTimeMs;
					float const c_nextBeatLocal = g_RadioAudioEntity.GetNextReplayBeat( soundHash, c_startOffset );	
					nextBeat = c_musicStartTimeMs + c_nextBeatLocal;
				}			
			}			
		}
	}

	return nextBeat;
}

//////////////////////////////////////////////////////////////////////////
float CReplayMgrInternal::GetPrevAudioBeatTime( float const* overrideClipTimeMs )
{
	float prevBeat = -1;

	if(sm_pPlaybackController)
	{
		s32 clipIndex = sm_pPlaybackController->GetCurrentClipIndex();

		if(clipIndex >= 0)
		{
			float const c_currentTimeMs = overrideClipTimeMs ? *overrideClipTimeMs : GetCurrentTimeMsFloat();
			float const c_playbackTimeMs = sm_pPlaybackController->ConvertNonDilatedTimeToTimeMs( clipIndex, c_currentTimeMs );
			s32 musicTrackID = sm_pPlaybackController->GetPreviousMusicIndexFromCurrentNonDilatedTimeMs( clipIndex, c_currentTimeMs );

			if(musicTrackID >= 0)
			{
				u32 soundHash = sm_pPlaybackController->GetMusicSoundHash(musicTrackID);
				float const c_musicStartTimeMs = (float)sm_pPlaybackController->GetMusicStartTimeMs( musicTrackID ) + (float)sm_pPlaybackController->GetMusicStartOffsetMs( musicTrackID );
				float const c_soundDuration = (float)sm_pPlaybackController->GetMusicDurationMs( musicTrackID );
				float const c_musicEndTimeMs = c_musicStartTimeMs + c_soundDuration;

				if( c_playbackTimeMs <= c_musicStartTimeMs )
				{
					prevBeat = c_musicStartTimeMs;
				}
				else if( c_playbackTimeMs > c_musicEndTimeMs )
				{
					prevBeat = c_musicEndTimeMs - REPLAY_JUMP_EPSILON;
				}
				else
				{
					float const c_startOffset = c_playbackTimeMs - c_musicStartTimeMs;
					float const c_prevBeatLocal = g_RadioAudioEntity.GetPrevReplayBeat( soundHash, c_startOffset );		
					prevBeat = c_musicStartTimeMs + c_prevBeatLocal;
				}
			}
		}
	}

	return prevBeat;
}

//////////////////////////////////////////////////////////////////////////
float CReplayMgrInternal::GetClosestAudioBeatTimeMs( float const clipTimeMs, bool const nextBeat )
{
	float const result = nextBeat ? GetNextAudioBeatTime( &clipTimeMs ) : GetPrevAudioBeatTime( &clipTimeMs );
	return result;
}

//========================================================================================================================
void CReplayMgrInternal::PreProcess()
{
	PF_AUTO_PUSH_TIMEBAR("Replay-Pre Process");
	if(sm_wantQuit == true)
	{
		QuitReplay();
	}

	HandleLogicForMemory();

	sm_DoFullProcessPlayback = false;
	sm_eventManager.ResetPlaybackResults();

	// Cache this locally as it's conceivable that this could be altered on another thread.  (Thanks ThomR!)
	eReplayMode desiredMode = sm_desiredMode;

	bool updateState = false;
	if(desiredMode != sm_uMode)
	{
		updateState = true;		// State change requested

#if DO_REPLAY_OUTPUT
		const char*	modeStrings[] = {	"Disabled", "Record", "Edit", "LoadClip", "WaitingForSave" };
		REPLAYTRACE("*CReplayMgrInternal::PreProcess DesiredMode:%s, Mode:%s", modeStrings[(int)sm_desiredMode], modeStrings[(int)sm_uMode]);
#endif // DO_REPLAY_OUTPUT

		if(sm_uMode == REPLAYMODE_RECORD)
		{
			//Fixed B*2049778 - Ignore fileops that don't affect replay internal i.e. enumeration
			if(IsSaving() || IsLoading() || WillSave())
			{
				replayDebugf1("Still performin a fileop!  Don't change state until next frame");
				updateState = false;	// Cancel state change
			}
		}
	}


	if(updateState)	// Continue with the state change
	{
		CheckAllThreadsAreClear();

		if(sm_uMode == REPLAYMODE_EDIT)
		{
			OnExitClip();

			sm_eReplayState.SetPrecaching(false);
			sm_nextReplayState.SetPrecaching(false);
			sm_interfaceManager.StopAllEntitySounds();
			sm_IsBeingDeleteByReplay = true;

			OnCleanupPlaybackSession();

			sm_eventManager.Cleanup();

			sm_pAdvanceReader->Clear();
			sm_interfaceManager.FlushPreloadRequests();

			ResetParticles();

			sm_interfaceManager.ClearWorldOfEntities();
			sm_IsBeingDeleteByReplay = false;
		}
		else if(sm_uMode == REPLAYMODE_RECORD)
		{
			DisableRecording();

			sm_eventManager.ClearCurrentEvents();
			 
			// Re do not disable the replay system if the UI editor is up as it requires the
			// replay system to be enabled. This can happen if we are saving a clip as the editor
			// is opened (url:bugstar:2034809).
			if(IsEditorActive() == false)
			{
			 	Disable();
			}
		}
		else if(IsAwareOfWorldForRecording() == false)
		{
			sm_eventManager.Reset();
		}
		else
		{	// Still aware of world for recording...overwatch will be active
			// We have to remove the interp events from the tracking system as these won't work
			sm_eventManager.RemoveInterpEvents();
		}

		if(desiredMode == REPLAYMODE_DISABLED)
		{
			SetupReplayBuffer(0,0);
		}
		else if(desiredMode == REPLAYMODE_RECORD)
		{
			Enable();
			EnableRecording();
		}
		else if(desiredMode == REPLAYMODE_LOADCLIP)
		{
			if(sm_uMode == REPLAYMODE_RECORD)
			{
				DisableRecording();
			}

 			if(IsAwareOfWorldForRecording())
 			{
 				sm_interfaceManager.DeregisterAllCurrentEntities();
 			}

			
			
			sm_LastErrorCode = REPLAY_ERROR_SUCCESS;

			sm_ReplayLoadClipStructure.Init( sm_fileToLoad.c_str(), sm_ReplayBufferInfo, LoadStartFunc);

			if (!CGenericGameStorage::PushOnToSavegameQueue(&sm_ReplayLoadClipStructure))
			{
				replayWarningf("CReplayMgrInternal::REPLAYMODE_LOADCLIP - CGenericGameStorage::PushOnToSavegameQueue failed\n");
				sm_TriggerPlayback = false;
				sm_LastErrorCode = REPLAY_ERROR_FAILURE;
				g_bWorldHasBeenCleared = true;
			}

#if DO_REPLAY_OUTPUT_XML
			atString name = atString(sm_fileToLoad);
			name.Replace(REPLAY_CLIP_FILE_EXT, "");
			strcpy_s(s_ExportReplayFilename, REPLAYPATHLENGTH, name.c_str());
#endif // REPLAY_DEBUG* /
			sm_fileToLoad = "";
		}

		sm_uMode = desiredMode;
	}


	// Process toggles
	if(sm_recordEvents != sm_wantToRecordEvents)
		sm_recordEvents = sm_wantToRecordEvents;

	if(sm_monitorEvents != sm_wantToMonitorEvents)
		sm_monitorEvents = sm_wantToMonitorEvents;

	if(sm_recordingThreaded != sm_wantRecordingThreaded)
		sm_recordingThreaded = sm_wantRecordingThreaded;

	if(sm_recordingThreadCount != sm_wantRecordingThreadCount)
		sm_recordingThreadCount = Max(1, Min((int)MAX_REPLAY_RECORDING_THREADS, sm_wantRecordingThreadCount));

#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	if( sm_uMode == REPLAYMODE_RECORD )
	{
		CBlockInfo* pBlock = sm_oRecord.GetBlock();
		pBlock->RequestThumbnail();
	}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS	

	if(IsEditModeActive() == false || IsLoading())
		return;

	if(sm_interfaceManager.CreateBatchedEntities(sm_eReplayState))
	{
		// Bail for a frame so we can continue adding in entities
		return;
	}

	extern bool gbForceReScanForReplay;
	gbForceReScanForReplay = false;
	if(sm_nextReplayState.IsSet(REPLAY_CURSOR_JUMP) || sm_forcePortalScanOnClipLoad)
	{
		sm_forcePortalScanOnClipLoad = false;
		gbForceReScanForReplay = true;
	}

	if(sm_nextReplayState.IsSet(REPLAY_STATE_PLAY) && fwTimer::IsUserPaused() && !IsPreCachingScene() && !sm_eventManager.IsRetryingEvents() && !sm_NeedEventJump )
	{
		fwTimer::EndUserPause();
	}
	else if(sm_nextReplayState.IsSet(REPLAY_STATE_PAUSE))
	{
		sm_wantPause = true;
		//fwTimer::StartUserPause(); // Don't do this here...set the flag
	}
	else if(sm_nextReplayState.IsSet(REPLAY_DIRECTION_BACK) || sm_nextReplayState.IsSet(REPLAY_CURSOR_JUMP))
	{
		//Only reset the particles on the frame we start rewinding or jumping.
		bool currentStateBackOrJump = sm_eReplayState.IsSet(REPLAY_DIRECTION_BACK) || sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP);
		if( !(currentStateBackOrJump) || (currentStateBackOrJump && sm_eLastReplayState.IsSet(REPLAY_STATE_PAUSE)) )
		{
			ResetParticles();
		}
		if(!sm_nextReplayState.IsPrecaching())
		{			
			UnfreezeAudio();
		}
	}
	
	if( IsScrubbing() )
	{
		UnfreezeAudio();
	}

	//reactive the render after set number of frames to cover issue with water that rely
	//on the camera being setup correctly before we detected underwater states
	if(m_WantsReactiveRenderer)
	{
		if(m_FramesTillReactiveRenderer > 0)
		{
			--m_FramesTillReactiveRenderer;
		}
		else
		{	
			CPauseMenu::TogglePauseRenderPhases(true, OWNER_OVERRIDE, __FUNCTION__ );
			m_WantsReactiveRenderer = false;
		}
	}

	if(sm_reInitAudioEntity)
	{
		s_AudioEntity.Shutdown();
		s_AudioEntity.Init();
		sm_reInitAudioEntity = false;
	}

	// Update any changed IPL groups
	ReplayIPLManager::SetDoProcess();

	if(IsEditModeActive())
	{
		ValidationResult res = Validate(false);
		if(res != eValidationOk && res != eValidationPaused)
		{
			return;
		}
	}

	ProcessCursorMovement();

	sm_eLastReplayState = sm_eReplayState;
	sm_eReplayState = sm_nextReplayState;

	// Force the unpause if the state is playing... - url:bugstar:4199777
	if(sm_eReplayState.IsSet(REPLAY_STATE_PLAY) && fwTimer::IsUserPaused())
	{
		fwTimer::EndUserPause();
	}

	if(sm_wantPause)
	{
		sm_canPause = true;
		sm_wantPause = false;
	}

	//u32 uPlaybackFrameStepDelta = static_cast<u32>(sm_oReplayInterpFrameTimer.GetMsTime() + 0.5f);
	float playbackFrameStepDelta = fwTimer::GetSystemTimeStep() * 1000.0f;

	// Whilst setting up the delta shouldn't increment.
	if(sm_isSettingUp)
	{
		playbackFrameStepDelta = 0.0f;
	}
	//SetPlaybackFrameStepDelta(uPlaybackFrameStepDelta);

	// Set the custom frame delta if the override is set
	float playbackSpeed = 1.0f;
	if(sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP))
	{
		//It's okay for this to fire now as we bail sometimes without completing a jump
		playbackFrameStepDelta = m_jumpDelta;
		m_jumpDelta = 0;
	}
	else if(sm_eReplayState.IsSet(REPLAY_CURSOR_SPEED))
	{
		playbackFrameStepDelta *= m_cursorSpeed;
		playbackSpeed *= m_cursorSpeed;
	}

	// Set the custom frame delta if the marker override is set
	if(!sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP))
	{
		playbackFrameStepDelta *= m_markerSpeed;
		playbackSpeed *= m_markerSpeed;
	}

	sm_eReplayState.SetPlaybackSpeed(playbackSpeed);

	// url:bugstar:2059757 and url:bugstar:2097705. If we are running fast (e.g. 60fps+) and the replay clip is slowed down (e.g. 5%) then
	// we get rounding errors and the game does not update. We can't just stop 5% speed as the same issue will happen at 10% speed when 120fps.
	// To fix this, if we end up with a dt of 0ms we don't reset the frame timer (note this dt of 0 will be converted to 1ms inside UpdateTimer().
	// We do this above any code where we want the dt to be 0.
	if(playbackFrameStepDelta != 0)
	{
		sm_oReplayInterpFrameTimer.Reset();
	}
	
	// If we're paused then set the frame step delta to 0
	if(IsClipTransition() || (IsPlaybackPaused() && (sm_eReplayState.IsSet(REPLAY_CURSOR_NORMAL) || sm_eReplayState.IsSet(REPLAY_CURSOR_SPEED))))
	{
		playbackFrameStepDelta = 0;
	}

	// Before passing down delta, apply timescale from the recorded game
	playbackFrameStepDelta *= sm_playbackSpeed;

	//Cut scene audio alignment
	//sm_CutSceneAudioSync = Clamp(sm_CutSceneAudioSync, -abs((sm_playbackFrameStepDelta/2)), abs((sm_playbackFrameStepDelta/2)));
	//SetPlaybackFrameStepDelta((u32)((float)sm_playbackFrameStepDelta + sm_CutSceneAudioSync));
	//sm_CutSceneAudioSync = 0;

#if __BANK
	// Prevent long times from occuring during playback while debugging.
	if(!sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP) && abs(playbackFrameStepDelta) > 500)
	{
		playbackFrameStepDelta = playbackFrameStepDelta / abs(playbackFrameStepDelta);
	}
#endif

#if __BANK
	//if were doing a panoramic capture then we need to advance the replay
	//by a fixed timestep if we're playing back othewise make sure the time step is zero
	if(LightProbe::IsCapturingPanorama())
	{
		if(IsPlaybackPlaying())
			playbackFrameStepDelta = (float)LightProbe::GetFixedPanoramaTimestepMS();
		else
			playbackFrameStepDelta = 0.0f;
	}
#endif

	const float totalTimeMS = GetTotalTimeMSFloat();
	float clipStartTime = 0;
	float clipEndTime = totalTimeMS;

	if (replayVerify(sm_pPlaybackController))
	{
		clipStartTime = sm_pPlaybackController->GetClipStartNonDilatedTimeMs(sm_pPlaybackController->GetCurrentClipIndex());
		clipEndTime = sm_pPlaybackController->GetClipNonDilatedEndTimeMs(sm_pPlaybackController->GetCurrentClipIndex());
	}

#if VIDEO_RECORDING_ENABLED && USES_MEDIA_ENCODER
	const bool exporting = sm_pPlaybackController && sm_pPlaybackController->IsExportingToVideoFile();
	const bool exportingPaused = exporting && sm_pPlaybackController->IsExportingPaused();
	const bool exportingStart = exporting && sm_pPlaybackController->IsStartingClipNextFrame();
	const bool fixedTimeExport = IsFixedTimeExport();

	if(fixedTimeExport)
	{
		float exportFrameStep = 0.0f;

		//g_RadioAudioEntity.PreUpdateReplayMusic();

		if(exporting)
		{
			// If we are exporting then make sure everything is loaded
#if !USE_SRLS_IN_REPLAY
			CStreaming::LoadAllRequestedObjects();
#endif

			if (!exportingPaused || exportingStart)
			{
				exportFrameStep = sm_pPlaybackController->GetExportFrameDurationMs();

				const u64 exportFrameStepNs = FLOAT_MILLISECONDS_TO_NANOSECONDS(exportFrameStep); 

				const s32 clipIndex = sm_pPlaybackController->GetCurrentClipIndex();
				const u64 realTimeToClipNs = sm_pPlaybackController->GetLengthTimeToClipNs(clipIndex);
				const float clipTrimmedTimeMS = sm_pPlaybackController->GetClipTrimmedTimeMs(clipIndex);
				const float timeRemaining = Max(clipEndTime - sm_fInterpCurrentTime, 0.0f);

				// Need to generate a playback delta from the accumulated export time including this frame
				// Sampling the clip to find the total non dilated time from the beginning of the montage (to account for all trim markers and marker speeds)
				const u64 desiredExportTotalNs = sm_exportTotalNs + exportFrameStepNs;
				const u64 clipTimeToSampleNs = desiredExportTotalNs > realTimeToClipNs ? desiredExportTotalNs - realTimeToClipNs : 0;
				const float clipTimeToSample = NANOSECONDS_TO_FLOAT_MILLISECONDS(clipTimeToSampleNs);

				if (clipTimeToSample < clipTrimmedTimeMS)
				{
					const float nonDilatedTimeMs = sm_pPlaybackController->ConvertTimeToNonDilatedTimeMs(clipIndex, clipTimeToSample);
					const float nonDilatedTimeToClipMs = sm_pPlaybackController->GetLengthNonDilatedTimeToClipMs(clipIndex);
	
					playbackFrameStepDelta = Max(nonDilatedTimeMs - (nonDilatedTimeToClipMs + sm_fInterpCurrentTime), 0.0f);
				}
				else
				{
					playbackFrameStepDelta = timeRemaining;
				}

				if (timeRemaining <= playbackFrameStepDelta)
				{
					// Clamp to time remaining in order to transition to next clip
					// Remaining time will be accounted for above at the start of the next clip
					// since sm_exportTotal will be held this frame
					playbackFrameStepDelta = timeRemaining;

					if (sm_pPlaybackController->IsCurrentClipLastClip())
					{
						audDriver::GetMixer()->AddVideoTimeNs((s64)exportFrameStepNs);
					}

					replayDebugf1("<Export!>Clp: %2d Frm: %lu Mkr: %4.4f Stp: %6.4f IntpT: %8.4f ExpT: %8.4f MonT: %8.4f RealT: %10.2f (%lu) ClipT: %lu AudV: %lu  AudA: %lu", 
						sm_pPlaybackController->GetCurrentClipIndex(),
						sm_exportFrameCount, 
						sm_eReplayState.GetPlaybackSpeed(), 
						playbackFrameStepDelta, 
						sm_fInterpCurrentTime + playbackFrameStepDelta, 
						NANOSECONDS_TO_FLOAT_MILLISECONDS(sm_exportTotalNs), 
						sm_pPlaybackController->ConvertNonDilatedTimeToTimeMs(clipIndex, sm_fInterpCurrentTime + playbackFrameStepDelta),
						sm_pPlaybackController->GetLengthTimeToClipMs(clipIndex),
						realTimeToClipNs,
						clipTimeToSampleNs,
						audDriver::GetMixer()->GetTotalVideoTimeNs(),
						audDriver::GetMixer()->GetTotalAudioTimeNs());
				}
				else
				{
					// Advance export time
					sm_exportTotalNs += exportFrameStepNs;

					// Advance export time for audio to match
					g_RadioAudioEntity.PreUpdateReplayMusic(exporting); // this will play the music
					audDriver::GetMixer()->AddVideoTimeNs((s64)exportFrameStepNs);

					replayDebugf1("<Export> Clp: %2d Frm: %lu Mkr: %4.4f Stp: %6.4f IntpT: %8.4f ExpT: %8.4f MonT: %8.4f RealT: %10.2f (%lu) ClipT: %lu AudV: %lu  AudA: %lu", 
						sm_pPlaybackController->GetCurrentClipIndex(),
						sm_exportFrameCount, 
						sm_eReplayState.GetPlaybackSpeed(), 
						playbackFrameStepDelta, 
						sm_fInterpCurrentTime + playbackFrameStepDelta, 
						NANOSECONDS_TO_FLOAT_MILLISECONDS(sm_exportTotalNs), 
						sm_pPlaybackController->ConvertNonDilatedTimeToTimeMs(clipIndex, sm_fInterpCurrentTime + playbackFrameStepDelta),
						sm_pPlaybackController->GetLengthTimeToClipMs(clipIndex),
						realTimeToClipNs,
						clipTimeToSampleNs,
						audDriver::GetMixer()->GetTotalVideoTimeNs(),
						audDriver::GetMixer()->GetTotalAudioTimeNs());

					sm_exportFrameCount++;
				}
			}
			else
			{
#if USE_SRLS_IN_REPLAY
				if(!IsCreatingSRL())
#endif
				{
					// Don't advance playback time while export is paused
					playbackFrameStepDelta = 0.0f;
				}
			}
		}

		SetExportFrameStep(exportFrameStep * 0.001f);
	}
	else
	{
		SetExportFrameStep(fwTimer::GetSystemTimeStep());
	}
#endif

	SetPlaybackFrameStepDelta(playbackFrameStepDelta);

	float tempCurrentTime = sm_fInterpCurrentTime + sm_playbackFrameStepDelta;
	if(sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP))
	{	// If we're jumping then don't rely on working out the destination time based on the delta as we can lose some precision
		tempCurrentTime = m_TimeToJumpToMS;
	}
	SetCurrentTimeMs(Clamp(tempCurrentTime, 0.0f, totalTimeMS));

	bool pause = false;
	if( GetCurrentTimeMsFloat() <= clipStartTime )
	{
		SetCurrentTimeMs(clipStartTime);
		pause = GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK);
	}
	else if( GetCurrentTimeMsFloat() >= clipEndTime )
	{
		SetCurrentTimeMs(clipEndTime);
		pause = GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_FWD);
	}

	// Update timers for this frame to match current time and playbackFrameStepDelta
	UpdateTimer();

	//If we're the first frame and we've just paused whilst playing backwards
	//Force our state to forwards so monitored packets can be extracted.
	if( GetCurrentPlayBackState().IsSet(REPLAY_STATE_PAUSE) && 
		IsFirstFrame(sm_currentFrameInfo.GetFrameRef()) && 
		GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK))
	{
		SetNextPlayBackState(REPLAY_DIRECTION_FWD);
	}

	if(GetCurrentPlayBackState().IsSet(REPLAY_STATE_PAUSE) == false && sm_canPause && pause)
	{
		if( GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK) )
		{
			SetNextPlayBackState(REPLAY_STATE_PAUSE | REPLAY_DIRECTION_BACK);
		}
		else if( GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_FWD) )
		{
			SetNextPlayBackState(REPLAY_STATE_PAUSE | REPLAY_DIRECTION_FWD);
		}
	}

	UpdateInterpolationFrames(GetCurrentTimeAbsoluteMs16BitsAccurary(), true);

	static bool wasJumping = false;

	if(sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP) && IsSettingUpFirstFrame() == false)
	{
		//don't pause if we're at the start of a clip will trash any states we wanted to preserve, see UpdateClip for reference
		if(sm_isSettingUpFirstFrame == false)
		{
			SetNextPlayBackState(REPLAY_STATE_PAUSE | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_NORMAL);
		}

		sm_WantsToKickPrecaching = true; 
	}

	//UI could be want to kick the precaching at any point, only set sm_KickPrecaching here.
	if(sm_WantsToKickPrecaching)
	{
		sm_WantsToKickPrecaching = false;
		sm_KickPrecaching = true;
		
		//After we've been scrubbing around set the cursor speed back to normal to reset the UI.
		SetNextPlayBackState(REPLAY_CURSOR_NORMAL);
	}

	// Pull out the population values for this frame
	sm_replayFrameData.m_populationData = sm_currentFrameInfo.GetFramePacket()->GetPopulationValues();
	sm_replayFrameData.m_timeWarpData = sm_currentFrameInfo.GetFramePacket()->GetTimeWarpValues();

	// Pull out the instance priority for this frame
	sm_replayFrameData.m_instancePriority = sm_currentFrameInfo.GetFramePacket()->GetReplayInstancePriority();
	
	fwTimer::SetTimeWarpSpecialAbility(sm_replayFrameData.m_timeWarpData.m_timeWrapSpecialAbility);
	fwTimer::SetTimeWarpScript(sm_replayFrameData.m_timeWarpData.m_timeWrapScript);
	fwTimer::SetTimeWarpCamera(sm_replayFrameData.m_timeWarpData.m_timeWrapCamera);

	const CPacketFrame* pCurrFramePacket = sm_currentFrameInfo.GetFramePacket();
	if(pCurrFramePacket)
	{
		if(pCurrFramePacket->GetPacketVersion() >= CPacketFrame_V1 && camInterface::GetReplayDirector().IsRecordedCamera())
		{
			RenderPhaseSeeThrough::SetState(pCurrFramePacket->GetShouldUseThermalVision());
		}
		else
		{
			RenderPhaseSeeThrough::SetState(false);
		}
	}

	if(sm_interfaceManager.CreateBatchedEntities(sm_eReplayState))
	{
		// Bail for a frame so we can continue adding in entities
		return;
	}

	if( IsClipTransition() )
	{
#if USE_SRLS_IN_REPLAY
		//Update SRL if recording 
		if( IsCreatingSRL() || IsUsingSRL() )
		{
			gStreamingRequestList.SetTime( GetCurrentTimeMs() * 1.0f );		
		}
#endif
		return;
	}

	// If we were jumping then ensure we play this frame forwards.
	CReplayState state = sm_eReplayState;
	if(wasJumping)
	{
		state.SetState(REPLAY_DIRECTION_FWD);
		wasJumping = false;
	}

	if(sm_JumpPrepareState)
	{
		if(sm_JumpPrepareState >= ePreloadKickNeeded)
		{
			sm_pAdvanceReader->WaitForAllScanners();
			bool isWithinEnvelope = sm_pAdvanceReader->IsFrameWithinCurrentEnvelope(CReplayPreloader::ScannerType, sm_currentFrameInfo);

			if(sm_JumpPrepareState == ePreloadFirstKickNeeded)
			{
				if(!isWithinEnvelope)
				{
					PauseRenderPhase();
				}
				--sm_JumpPrepareState;
			}

			// Perform a Jump on the actual frame we're jumping to.  This should
			// get all the create packets organised before we kick off the preloader
			// to load all the models
			CReplayState state(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_JUMP);
			{
				CAddressInReplayBuffer address = sm_currentFrameInfo.GetModifiableAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(state);
				controller.EnablePlayback();
				controller.SetFineScrubbing(m_FineScrubbing);
				sm_IsBeingDeleteByReplay = true;

				while(!controller.IsNextFrame())
				{
					if(controller.IsEndOfPlayback())
						break;

					ePlayPktResult result = sm_interfaceManager.JumpPackets(controller);
					if(result == PACKET_UNHANDLED)
					{
						controller.AdvanceUnknownPacket(controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize());
					}
					else if(result == PACKET_ENDBLOCK)
						break;
				}

				sm_interfaceManager.ProcessRewindDeletions(controller);

				controller.DisablePlayback();
				sm_IsBeingDeleteByReplay = false;
			}

			sm_pAdvanceReader->Kick(sm_currentFrameInfo, sm_bPreloaderResetFwd, sm_bPreloaderResetBack, false, CReplayClipScanner::Entity | CReplayClipScanner::SingleFrame);
			--sm_JumpPrepareState;
		}
		else if(sm_pAdvanceReader->HandleResults(CReplayPreloader::ScannerType, CReplayState(), 0, false, CReplayClipScanner::Entity))
		{
			sm_JumpPrepareState = eNoPreload;
		}
	}

	CAddressInReplayBuffer oPrevInterpFrame = sm_currentFrameInfo.GetModifiableAddress();
	ReplayController controller(oPrevInterpFrame, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
	controller.SetPlaybackFlags( state );
	controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
	controller.EnablePlayback();

	float fInterpolation = 0.0f;
	if (sm_nextFrameInfo.GetGameTime() >= GetCurrentTimeAbsoluteMs() && sm_nextFrameInfo.GetGameTime() != sm_currentFrameInfo.GetGameTime())
	{
		float frameLength = float((float)sm_nextFrameInfo.GetGameTime() - (float)sm_currentFrameInfo.GetGameTime());
		float currentFramePos = float(GetCurrentTimeAbsoluteMs16BitsAccurary() - ((u64)sm_currentFrameInfo.GetGameTime() << 16));
		fInterpolation = currentFramePos / (frameLength * (float)(0x1 << 16));

		// Insanity check
		if(fInterpolation < 0.0f)
			fInterpolation *= -1.0f;
		replayAssertf(fInterpolation <= 1.0f && fInterpolation >= 0.0f, "%f", fInterpolation);
		fInterpolation = Max(0.0f, Min(1.0f, fInterpolation));
	}

	bool thisOrPrevFrameNotPaused = (!sm_eReplayState.IsSet(REPLAY_STATE_PAUSE) || !sm_eLastReplayState.IsSet(REPLAY_STATE_PAUSE));
	if(!sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP) && thisOrPrevFrameNotPaused && !sm_JumpPrepareState)
	{
		if(sm_eReplayState.IsSet(REPLAY_STATE_PAUSE) && !sm_eLastReplayState.IsSet(REPLAY_STATE_PAUSE))
		{   // If we're paused but we weren't last frame then reset the advance reader extents
			sm_bPreloaderResetFwd = sm_bPreloaderResetBack = true;
		}

		sm_pAdvanceReader->Kick(sm_nextFrameInfo, sm_bPreloaderResetFwd, sm_bPreloaderResetBack);
	}

 	if(sm_interfaceManager.CreateBatchedEntities(sm_eReplayState))
 	{
		// Bail for a frame so we can continue adding in entities
		controller.DisablePlayback();
		return;
 	}

	sm_fLastInterpValue = fInterpolation;

	controller.SetInterp(sm_fLastInterpValue);
	controller.SetFrameInfos(sm_firstFrameInfo, sm_currentFrameInfo, sm_nextFrameInfo, sm_lastFrameInfo);
	PlayBackThisFrameInterpolation(controller, true, sm_JumpPrepareState > eNoPreload);
	sm_fLastInterpValue = controller.GetInterp();
	controller.DisablePlayback();

	// Update Rayfire
	ReplayRayFireManager::Process();

	ReplayHideManager::Process();

	ReplayAttachmentManager::Get().Process();

	sm_pAdvanceReader->HandleResults(CReplayPreloader::ScannerType | CReplayPreplayer::ScannerType, controller.GetPlaybackFlags(), GetCurrentTimeAbsoluteMs(), false);

	if(IsCursorFastForwarding() || IsCursorRewinding() || sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP))
	{
		if(!IsPreCachingScene())
		{
			if(!sm_pPlaybackController || !sm_pPlaybackController->IsExportingToVideoFile())
			{
				g_RadioAudioEntity.StopReplayMusic();
			}			
		}
		UnfreezeAudio();
	}

	if( (!IsReplayCursorJumping() || m_ForceUpdateRopeSimWhenJumping) && fwTimer::IsUserPaused() && m_ForceUpdateRopeSim )
	{	
		m_ForceUpdateRopeSim = false;
		m_ForceUpdateRopeSimWhenJumping = false;
		CPhysics::GetRopeManager()->UpdatePhysics( 0, ROPE_UPDATE_WITHSIM );
		CPhysics::RopeSimUpdate( 0, ROPE_UPDATE_EARLY );
	}

	m_FineScrubbing = false;


#if USE_SRLS_IN_REPLAY
	//Update SRL if recording 
	if( IsCreatingSRL() || IsUsingSRL() )
	{
		gStreamingRequestList.SetTime( GetCurrentTimeMs() * 1.0f );		
	}
#endif
	if(sm_JumpPrepareState == eNoPreload)
	{
		sm_DoFullProcessPlayback = true;
	}
}

//========================================================================================================================
void CReplayMgrInternal::PostProcess()
{
	CReplayTrailController::GetInstance().Update();

	if (!IsEditModeActive() || IsLoading())
	{
		return;
	}

	if(sm_interfaceManager.HasBatchedEntitiesToProcess())
	{
		// Bail for a frame so we can continue adding in entities
		sm_interfaceManager.PostProcess();
		return;
	}

	ValidationResult res = Validate(false);
	if(res == eValidationOther)
	{
		return;
	}

	PF_AUTO_PUSH_TIMEBAR("Replay-Post Process");
	if (sm_uMode == REPLAYMODE_EDIT)
	{
		sm_interfaceManager.PostProcess();
	}

	if(res != eValidationOk)
	{
		return;
	}

	ReplayMeshSetManager::Process();
}

//========================================================================================================================
bool CReplayMgrInternal::IsWaitingOnWorldStreaming()
{
	if(sm_uStreamingStallTimer >= STREAMING_STALL_LIMIT_MS)
	{
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::PrependPrecacheString("Gave up waiting for - ");)
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Streaming Checks
	s32 const objRequestCount = CStreaming::GetNumberObjectsRequested();
	if(objRequestCount > 0)
	{
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on objects requested...", objRequestCount);)
		return true;
	}

	bool const priorityObjLoading = strStreamingEngine::GetIsLoadingPriorityObjects();
	if(priorityObjLoading)
	{
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on priority objects...");)
		return true;
	}

	s32 const realRequestCount = strStreamingEngine::GetInfo().GetNumberRealObjectsRequested();
	if(realRequestCount)
	{
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on real objects...", realRequestCount);)
		return true;
	}
	

	//////////////////////////////////////////////////////////////////////////
	// Vehicle Interface Checks
	iReplayInterface* pI = CReplayMgrInternal::GetReplayInterface("CVehicle");
	CReplayInterfaceVeh* pVehicleInterface = verify_cast<CReplayInterfaceVeh*>(pI);
	if(pVehicleInterface->WaitingOnLoading())
	{
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on vehicle loading...");)
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Ped Interface Checks
 	pI = CReplayMgrInternal::GetReplayInterface("CPed");
 	CReplayInterfacePed* pPedInterface = verify_cast<CReplayInterfacePed*>(pI);
 	if(pPedInterface->WaitingOnLoading())
	{
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on ped loading...");)
		
		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::AddDebugMessage(0x3deefd, "Waited on Peds loading in... see url:bugstar:2427183.  Did this occur?"); )
		/*return true;*/
	}

	//////////////////////////////////////////////////////////////////////////
	// These messages won't display when loading a clip (only on precaching)...
	// Just adding for consistency (and incase things change).
	if( sm_eLoadingState == REPLAYLOADING_CLIP )
	{
		if(pVehicleInterface->IsWaitingOnDamageToBeApplied())
		{
			GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on vehicle damage being applied...");)
			return true;
		}

		if( DECALMGR.GetNumAddInfos() > 0 )
		{
			GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on decals being applied...", (s32)DECALMGR.GetNumAddInfos());)
			return true;
		}

		if( !CPedPropsMgr::HasProcessedAllRequests() )
		{
			GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("Waiting on Ped props processing requests...");)
			return true;
		}
	}
#if USE_SRLS_IN_REPLAY
	if(IsUsingSRL() && !gStreamingRequestList.IsPlaybackActive())
	{
		if(IsUsingSRL() && gStreamingRequestList.GetPrestreamMode() != SRL_PRESTREAM_FORCE_ON)
		{
			gStreamingRequestList.BeginPrestream(NULL, true);
			gStreamingRequestList.SetTime(0);
			gStreamingRequestList.SetPrestreamMode(SRL_PRESTREAM_FORCE_ON);
			return true;
		}
		//if we are using a previously recorded SRL we want to do alittle prestreaming before
		//we actually kick off the clip's playback. The setup to use the SRL is done at the point 
		//we switch clips (See: CReplayMgrInternal::UpdateClip())
		//here we just want to make sure we have been able to preload the first N frames for playback
		if(!gStreamingRequestList.AreAllAssetsLoaded())
		{
			return true;
		}
	}
#endif
	GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::SetPrecacheString("");)
	return false;
}

//========================================================================================================================
void CReplayMgrInternal::StartFrame()
{
	if (sm_uMode == REPLAYMODE_RECORD)
	{
		sm_oReplayInterpFrameTimer.Reset();
	}
	else if (sm_uMode == REPLAYMODE_EDIT)
	{
		sm_oReplayFullFrameTimer.Reset();
	}
}

//========================================================================================================================

void CReplayMgrInternal::PostRender()
{
	if( sm_uMode == REPLAYMODE_RECORD )
		sm_interfaceManager.PostRender();

	gVpMan.PushViewport(gVpMan.GetGameViewport());
	PostRenderInternal();
	gVpMan.PopViewport();	
}


#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
bool CReplayMgrInternal::ShouldScriptsPrepareForSave()
{
	if(sm_uMode == REPLAYMODE_WAITINGFORSAVE)
	{
		if ( (g_ReplaySaveState == REPLAY_SAVE_BEGIN_WAITING_FOR_SCRIPT_TO_PREPARE) || (g_ReplaySaveState == REPLAY_SAVE_WAITING_FOR_SCRIPT_TO_PREPARE) )
		{
			return true;
		}
	}

	return false;
}


void CReplayMgrInternal::SetScriptsHavePreparedForSave(bool bHavePrepared)
{
	replayAssertf(sm_ScriptSavePreparationState == SCRIPT_SAVE_PREP_WAITING_FOR_RESPONSE_FROM_SCRIPT, "CReplayMgrInternal::SetScriptsHavePreparedForSave - expected sm_ScriptSavePreparationState to be WAITING_FOR_RESPONSE_FROM_SCRIPT");

	if (bHavePrepared)
	{
		sm_ScriptSavePreparationState = SCRIPT_SAVE_PREP_PREPARED;
	}
	else
	{
		sm_ScriptSavePreparationState = SCRIPT_SAVE_PREP_NOT_PREPARED;
	}
}
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE


void CReplayMgrInternal::PostRenderInternal()
{
	if(sm_uMode == REPLAYMODE_WAITINGFORSAVE)
	{

#if USE_SAVE_SYSTEM
#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE

#if DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS
		const eReplaySaveState c_FailState = REPLAY_SAVE_FAILED_DISPLAY_WARNING_MESSAGE;
#else	//	DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS
		const eReplaySaveState c_FailState = REPLAY_SAVE_FAILED;
#endif	//	DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS

		static sysTimer scriptSavePreparationTimer;

		if (g_ReplaySaveState == REPLAY_SAVE_BEGIN_WAITING_FOR_SCRIPT_TO_PREPARE)
		{
			scriptSavePreparationTimer.Reset();
			sm_shouldUpdateScripts = true;
			g_ReplaySaveState = REPLAY_SAVE_WAITING_FOR_SCRIPT_TO_PREPARE;
		}

		if (g_ReplaySaveState == REPLAY_SAVE_WAITING_FOR_SCRIPT_TO_PREPARE)
		{
			switch (sm_ScriptSavePreparationState)
			{
				case SCRIPT_SAVE_PREP_WAITING_FOR_RESPONSE_FROM_SCRIPT :
					{
						const float ScriptSavePreparationTimeout = 2000.0f;
						if (scriptSavePreparationTimer.GetMsTime() < ScriptSavePreparationTimeout)
						{
							replayDebugf2("Still waiting for scripts to prepare for saving... waited for %f ms", scriptSavePreparationTimer.GetMsTime());
							return;
						}
						else
						{
							replayDebugf1("Given up waiting for scripts to prepare for saving... waited for %f ms", scriptSavePreparationTimer.GetMsTime());
							// Give up waiting, don't attempt to save

							sm_shouldUpdateScripts = false;
							g_ReplaySaveState = c_FailState;
						}
					}
					break;

				case SCRIPT_SAVE_PREP_NOT_PREPARED :
					{
						replayDebugf1("Scripts have called SET_SCRIPTS_HAVE_PREPARED_FOR_SAVE(FALSE) so don't attempt to save");

						sm_shouldUpdateScripts = false;
						g_ReplaySaveState = c_FailState;
					}
					break;

				case SCRIPT_SAVE_PREP_PREPARED :
					{
						replayDebugf1("Scripts have called SET_SCRIPTS_HAVE_PREPARED_FOR_SAVE(TRUE)... waited for %f ms", scriptSavePreparationTimer.GetMsTime());
						sm_shouldUpdateScripts = false;

						sm_ReplaySaveStructure.Init();
						if (CGenericGameStorage::PushOnToSavegameQueue(&sm_ReplaySaveStructure))
						{
							g_ReplaySaveState = REPLAY_SAVE_IN_PROGRESS;
						}
						else
						{
							g_ReplaySaveState = c_FailState;
						}
					}
					break;
			}
		}

		//	 Do I need to set sm_ScriptSavePreparationState back to SCRIPT_SAVE_PREP_WAITING_FOR_RESPONSE_FROM_SCRIPT here?
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE

		if (g_ReplaySaveState == REPLAY_SAVE_IN_PROGRESS)
		{
			if(sm_ReplaySaveStructure.GetStatus() == MEM_CARD_BUSY)
				return;

			if (sm_ReplaySaveStructure.GetStatus() == MEM_CARD_COMPLETE)
			{
				g_ReplaySaveState = REPLAY_SAVE_SUCCEEDED;
			}
			else
			{
#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
				g_ReplaySaveState = c_FailState;
#else	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
				g_ReplaySaveState = REPLAY_SAVE_FAILED;
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
			}
		}

#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE && DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS
		if (g_ReplaySaveState == REPLAY_SAVE_FAILED_DISPLAY_WARNING_MESSAGE)
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "VEUI_HDR_ALERT", "REPL_NO_PROGRESS_SAVE", FE_WARNING_YES_NO);

			eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput();
			if( c_result == FE_WARNING_YES)
			{
				g_ReplaySaveState = REPLAY_SAVE_FAILED;
			}
			else if ( c_result == FE_WARNING_NO )
			{
				replayAssertf(0, "CReplayMgrInternal::PostRenderInternal - still need to add the code here to cancel the editing of the clip");

				//	I think the clip will have been loaded by this stage.
				//	There are functions called Disable() and CleanupPlayback()
				//	Can I call one of those here? Or do I need a new function to unload the clip and reset other variables?
				//	Or CReplayCoordinator::KillPlaybackOrBake()?
//				CReplayCoordinator::KillPlaybackOrBake();
//				CleanupPlayback();
//				sm_desiredMode = REPLAYMODE_DISABLED;

				g_ReplaySaveState = REPLAY_SAVE_NOT_STARTED;
				return;
			}
			else
			{
				//	The player hasn't made a choice yet
				return;
			}
		}
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE && DISPLAY_MESSAGE_IF_REPLAY_SAVE_FAILS
#endif	//	USE_SAVE_SYSTEM

		g_bWorldHasBeenCleared = true;

		GTA_REPLAY_OVERLAY_ONLY(CReplayOverlay::sm_debugMessages.clear();)

		//Prepare replay for playback
		if(!ClearWorld())
			return;

		sm_allowBlockingLoading = true;
		EXTRACONTENT.ExecuteReplayMapChanges();

		CFileLoader::MarkArchetypesWhichCannotBeDummy();
		sm_allowBlockingLoading = false;

		if( sm_uMode == REPLAYMODE_DISABLED || sm_desiredMode == REPLAYMODE_DISABLED )
		{
			return;
		}

		sm_ReplayInControlOfWorld = true;
		// Turn off multi-threaded cloth, it doesn't work with Replay.
		CPhysics::GetClothManager()->SetDontDoClothOnSeparateThread(true);
		ASSERT_ONLY(rage::SetNoPlacementAssert(true));
		PrepareForPlayback();

 		sm_desiredMode = REPLAYMODE_EDIT;
		return;
	}

	//File manager is loading
	if (IsLoading())
	{
		return;
	}

	//Replay internal is loading update
	if (sm_uMode == REPLAYMODE_LOADCLIP)
	{
		//Move to on update.
		MemoryCardError result = sm_ReplayLoadClipStructure.GetStatus();
		if( result != MEM_CARD_BUSY )
		{
			if( result == MEM_CARD_COMPLETE )
			{
				sm_TriggerPlayback = true;
				TriggerPlayback();
			}
			else
			{
				sm_LastErrorCode = REPLAY_ERROR_FAILURE;
				//Forces a reload of the last save when we exit, see CReplayMgrInternal::QuitReplay() LN:1447
				g_bWorldHasBeenCleared = true;
			}
		}
		return;
	}

	if (sm_uMode == REPLAYMODE_RECORD)
	{
		sm_oReplayInterpFrameTimer.Reset();
	}
	else if (sm_uMode == REPLAYMODE_EDIT)
	{
		sm_oReplayFullFrameTimer.Reset();
	}

	if (IsEditModeActive() )
	{
		if( !IsPreCachingScene() && sm_pPlaybackController)
		{
			if( (IsSinglePlaybackMode() || sm_pPlaybackController->IsCurrentClipLastClip()) && !fwTimer::IsUserPaused() 
					&& sm_eReplayState.IsSet(REPLAY_DIRECTION_BACK) == false) //don't trigger the finished playback code if we're rewinding, otherwise we can get stuck at the end
			{
				float uCurrentTime	= GetCurrentTimeRelativeMsFloat();
				float clipEndTime	= sm_pPlaybackController->GetClipNonDilatedEndTimeMs(sm_pPlaybackController->GetCurrentClipIndex());

				if (uCurrentTime >= clipEndTime || uCurrentTime >= sm_uPlaybackEndTime - sm_uPlaybackStartTime)
				{
					SetFinishPlaybackPending(true);
				}
			}
		}

		if(sm_eReplayState.IsSet(REPLAY_STATE_PAUSE) && sm_canPause && sm_eLoadingState != REPLAYLOADING_CLIP)
		{
			if(!fwTimer::IsUserPaused())
				fwTimer::StartUserPause();
			sm_canPause = false;
		}

		Validate(true);
	}



	if (IsFinishPlaybackPending() )
	{
		FinishPlayback();

		SetFinishPlaybackPending(false, false);

		if(!fwTimer::IsUserPaused())
			fwTimer::StartUserPause();
	}
}

//////////////////////////////////////////////////////////////////////////
u32 CReplayMgrInternal::CalculateSizeOfBlock(atArray<u32>& sizes, bool blockChange)
{
	PF_AUTO_PUSH_TIMEBAR("Replay-CalculateSizeOfBlock");
	u32 sizeOfBlock = 0;

	sizes.clear();
	sizes.PushAndGrow(sizeof(CPacketFrame));
	sm_interfaceManager.GetMemoryUsageForFrame(blockChange, sizes);
	sizes.PushAndGrow(sizeof(CPacketNextFrame));
	sizes.PushAndGrow(sm_eventManager.GetEventBufferReq(blockChange));
	sizes.PushAndGrow(ReplayIPLManager::GetMemoryUsageForFrame(blockChange));
	sizes.PushAndGrow(ReplayMeshSetManager::GetMemoryUsageForFrame(blockChange));
	sizes.PushAndGrow(ReplayHideManager::GetMemoryUsageForFrame(blockChange));
	sizes.PushAndGrow(ReplayAttachmentManager::Get().GetMemoryUsageForFrame(blockChange));
	sizes.PushAndGrow(sizeof(CPacketEnd));

	for(int i = 0; i < sizes.size(); ++i)
		sizeOfBlock += sizes[i];

	return sizeOfBlock;
}


//////////////////////////////////////////////////////////////////////////
s64 CReplayMgrInternal::CopyBlockToBlock(CBlockInfo* pTo, CBlockInfo* pFrom)
{
	replayDebugf3("Copying block %u to block %u", pFrom->GetMemoryBlockIndex(), pTo->GetMemoryBlockIndex());
	// Lock...
	pTo->LockForWrite();
	pFrom->LockForRead();

	// Copy across...
	pTo->CopyFrom(*pFrom);
	pTo->SetTimeSpan(pFrom->GetTimeSpan());
	pTo->SetStartTime(pFrom->GetStartTime());
	pTo->SetSizeLost(pFrom->GetSizeLost());
	pTo->SetSizeUsed(pFrom->GetSizeUsed());
	pTo->SetFrameCount(pFrom->GetFrameCount());
	pTo->SetStatus(pFrom->GetStatus());
	pTo->SetSessionBlockIndex(pFrom->GetSessionBlockIndex());
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	pTo->SetThumbnailRef(pFrom->GetThumbnailRef());
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

	// All the 'latest update' packet pointers are pointing to the temporary block so we need
	// to offset them into the main block we just copied into.
	// We don't need to worry about the create packets as they just got reset above.
	s64 offset = pTo->GetData() - pFrom->GetData();
	sm_interfaceManager.OffsetUpdatePackets(offset, pFrom->GetData(), pFrom->GetBlockSize());

	// Double check whether we're currently recording into a temp block
	if(sm_oRecord.GetMemoryBlockIndex() == pFrom->GetMemoryBlockIndex())
		sm_oRecord.SetBlock(pFrom->GetNormalBlockEquiv());

	replayAssert(sm_oRecord.GetBlock()->GetStatus() == REPLAYBLOCKSTATUS_BEINGFILLED);

	// Reset temp block...
	BUFFERDEBUG("Disable temp block %u (Equiv - %u)", pFrom->GetMemoryBlockIndex(), pFrom->GetNormalBlockEquiv()->GetMemoryBlockIndex());
	pFrom->SetNormalBlockEquiv(NULL);
	pFrom->SetStatus(REPLAYBLOCKSTATUS_EMPTY);

	// Unlock
	pFrom->UnlockBlock();
	pTo->UnlockBlock();

	return offset;
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::RecordingThread(void* pData)
{
	int threadIndex = (int)(ptrdiff_t)(pData);

	sm_recordingThreadId[threadIndex] = sysIpcGetCurrentThreadId();

#if REPLAY_RECORD_SAFETY
	if(threadIndex != 0)
		CReplayRecorder::GetScratch(threadIndex);
#endif // REPLAY_RECORD_SAFETY

	while(sm_exitRecordingThread == false)
	{
		// Wait here until a record is triggered
		sysIpcWaitSema(sm_entityStoringStart[threadIndex]);

		if(!sm_exitRecordingThread)
		{
			// Record the frame
			RecordFrame(threadIndex);
		}
	}

	sysIpcSignalSema(sm_recordingThreadEnded);
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::OnEditorActivate()		
{ 
	sm_IsEditorActive = true;
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::OnEditorDeactivate()	
{ 
	sm_IsEditorActive = false;	
}

bool CReplayMgrInternal::CanFreeUpBlocks()
{
    return sm_uMode == REPLAYMODE_DISABLED && !IsSaving() && !sm_doSave;
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::FreeUpBlocks()
{
	replayFatalAssertf(sm_uMode == REPLAYMODE_DISABLED, "Mode should be disabled for free'ing blocks");
	replayFatalAssertf(!IsSaving(),	"Can't be saving before free'ing blocks");
	replayFatalAssertf(!sm_doSave, "Can't be expecting to be saving before free'ing blocks");

	FreeMemory();
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::RecordFrame(u32 threadIndex)
{
	if (threadIndex != 0)
	{
		if (threadIndex < GetRecordingThreadCount())
		{
#if ENABLE_PIX_TAGGING
			char markerName[255];
			sprintf(markerName, "Replay-Record-Frame-%u", threadIndex);
#endif
			PF_PUSH_MARKER(markerName);
			StoreEntityInformation(threadIndex);
			PF_POP_MARKER();

			sysIpcSignalSema(sm_entityStoringFinished[threadIndex]);
		}
		return;
	}

	PF_PUSH_MARKER("Replay-Record-Frame-0");

	ReplayController controller(sm_oRecord, sm_ReplayBufferInfo, FrameRef(sm_oRecord.GetSessionBlockIndex(), (u16)sm_uCurrentFrameInBlock));

	StoreEntityInformation(threadIndex);

	PF_PUSH_MARKER("Replay-Wait-To-Consolidate");

	// Wait for the other recording thread (if any)
	for (u32 i = 1; i < GetRecordingThreadCount(); i++)
	{
		sysIpcWaitSema(sm_entityStoringFinished[i]);
	}	

	

	PF_POP_MARKER();

	PacketRecordWait();

	PF_PUSH_MARKER("Replay-Consolidate");
	
	ConsolidateFrame(controller, sm_addressBeforeConsolidation, sm_frameRefBeforeConsolidation);

	PF_POP_MARKER();

	PacketRecordSignal();

	sm_addressAfterConsolidation = controller.GetAddress();

	//sm_storingEntities = false;
	sysIpcSignalSema(sm_entityStoringFinished[0]);
	sysIpcSignalSema(sm_quantizeStart);
}

void CReplayMgrInternal::QuantizeThread(void* )
{
	while(sm_exitRecordingThread == false)
	{
		// Wait here until ready to quantize
		sysIpcWaitSema(sm_quantizeStart);

		if(!sm_exitRecordingThread)
		{
			PF_PUSH_MARKER("Replay-Quantize");

			if (replayVerifyf(sm_addressBeforeConsolidation.GetMemoryBlockIndex() == sm_addressAfterConsolidation.GetMemoryBlockIndex(), "Recorder should record data into only one block!"))
			{
				sm_addressBeforeConsolidation.SetAllowPositionChange(true);
				ReplayController postController(sm_addressBeforeConsolidation, sm_ReplayBufferInfo, sm_frameRefBeforeConsolidation);

				sm_interfaceManager.PostRecordOptimizations(postController, sm_addressAfterConsolidation);
			}

			PF_POP_MARKER();

			// Signal that we're completely finished recording...
			sysIpcSignalSema(sm_recordingFinished);

			PF_POP_MARKER();
		}
	}

	sysIpcSignalSema(sm_quantizeThreadEnded);
}

bool CReplayMgrInternal::IsStoringEntities()
{
	if (!sm_storingEntities)
		return false;

	if (!sm_recordingThreaded)
		return true;

	sysIpcCurrentThreadId id = rage::sysIpcGetCurrentThreadId();
	for (int i = 0; i < sm_recordingThreadCount; i++)
	{
		if (id == sm_recordingThreadId[i])
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::StoreEntityInformation(u32 threadIndex)
{
#if ENABLE_PIX_TAGGING
	char markerName[255];
	sprintf(markerName, "Replay-Store-Entity-Information-%u", threadIndex);
	PIX_AUTO_TAG(1, markerName);
#endif
	
	//sm_uCurrentFrameInBlock is set to -1 at the start of the replay (it also gets set to zero when the block is full, see below)
	//and is now incremented at the start of the frame, it's done this way because it was causing creation frames 
	//to being recorded a frame too late, which then caused issues, especially if the next block was a new one, which would
	//leave an entity with a creation frame that never existed causing problems during rewinding
	
	// ***Thread all this while entities are LOCKED***

	sm_interfaceManager.RecordFrame(threadIndex, sm_currentSessionBlockID);

	if (threadIndex == 0)
	{
		ReplayIPLManager::CollectData();
		ReplayMeshSetManager::CollectData();
		ReplayInteriorManager::CollectData();
	}
}

int UpdateSavedFlag(CBlockProxy block, void*)
{
	block.UpdateSavedFlag();
	return 0;
}
//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::ConsolidateFrame(ReplayController& controller, CAddressInReplayBuffer& addressBeforeConsolidation, FrameRef& frameRefBeforeConsolidation)
{
	PIX_AUTO_TAG(1,"Replay-ConsolidateFrame");

	// This will only happen on the very first frame of recording...
	if(sm_pPreviousFramePacket == NULL)
	{
		ReplayAttachmentManager::Get().OnRecordStart();
		ReplayIPLManager::OnRecordStart();
		//
		ReplayMeshSetManager::OnRecordStart();

		CPacketWheel::OnRecordStart();
	}

	// Pulls all the frame hashes from the interfaces
	sm_interfaceManager.UpdateFrameHash();

	bool blockChange = (sm_pPreviousFramePacket == NULL);
	atArray<u32> expectedRecordSizes;
	int sizesIndex = 0;

	sm_eventManager.Lock();
	s32 dataSize = CalculateSizeOfBlock(expectedRecordSizes, blockChange);

	// Update the state of any blocks that have been flagged as finishing saved
	ForeachBlock(&UpdateSavedFlag);

	replayFatalAssertf(controller.GetCurrentBlock()->GetStatus() == REPLAYBLOCKSTATUS_BEINGFILLED, "Current block is not set to 'being filled' check 1 failed");
	// Set up for recording...
	if(!controller.EnableRecording(dataSize, false))
	{
		// Failed to set the controller for recording because there was not enough
		// space in the current block.
		blockChange = true;
		sm_uCurrentFrameInBlock = 0;

		sm_interfaceManager.ResetCreatePacketsForCurrentBlock();

		//ReplayHideManager::OnBlockChange();	Needs a rethink...this causes a crash

		replayDebugf1("wantBlockSwitch = %s", sm_wantBufferSwap ? "TRUE" : "FALSE");
		if(sm_ReplayBufferInfo.IsAnyBlockSaving() == false && sm_wantBufferSwap == true)
		{
			// Loop though all the temp blocks and see if we can copy them back into 
			// the main buffer
			CBlockInfo* pCurrBlock = controller.GetCurrentBlock();
			
			replayDebugf1("Block writing to before swap: %u", pCurrBlock->GetMemoryBlockIndex());
			sm_ReplayBufferInfo.SwapTempBlocks(pCurrBlock);

			replayDebugf1("Block writing to after swap: %u", pCurrBlock->GetMemoryBlockIndex());

			// If the temp buffer looped, we need to clear out the normal block buffer before copying back to have consistent times 
			// Be aware, that by this time replay is already filling block[numOfTempBlocks+1] with new data
			if( sm_ReplayBufferInfo.GetTempBlocksHaveLooped() )
			{
				sm_ReplayBufferInfo.ResetNormalBlocksAfterTempLoop();
			}

			sm_ReplayBufferInfo.SetTempBlocksHaveLooped(false);
			sm_wantBufferSwap = false;
		}

		sm_pPreviousFramePacket->ValidatePacket();
		dataSize = CalculateSizeOfBlock(expectedRecordSizes, blockChange);
		controller.EnableRecording(dataSize, true);

#if GTA_REPLAY_OVERLAY
		if( CReplayOverlay::ShouldCalculateBlockStats() )
			CalculateBlockStats(controller.GetCurrentBlock()->GetPrevBlock());
#endif //GTA_REPLAY_OVERLAY
	}

	replayFatalAssertf(controller.GetCurrentBlock()->GetStatus() == REPLAYBLOCKSTATUS_BEINGFILLED, "Current block is not set to 'being filled' check 2 failed");

	addressBeforeConsolidation = controller.GetAddress();
	frameRefBeforeConsolidation = controller.GetCurrentFrameRef();


	// ***Thread all this after entities are UNLOCKED***

	//replayDebugf1("Recording Block: %u (mem %u) Frame: %u", controller.GetCurrentFrameRef().GetBlockIndex(), controller.GetCurrentBlockIndex(), controller.GetCurrentFrameRef().GetFrame());
	
	// Record current frame packet
	CPacketFrame* pCurrFramePacket = controller.GetAddress().GetBufferPosition<CPacketFrame>();
	u32 framePacketPosition = controller.GetCurrentPosition();
	controller.RecordPacketWithParam<CPacketFrame>(controller.GetCurrentFrameRef(), expectedRecordSizes[sizesIndex++]);

	// This will only happen on the very first frame of recording...
	if(sm_pPreviousFramePacket == NULL)
	{
		replayFatalAssertf(controller.GetCurrentPosition() == sizeof(CPacketFrame), "First Packet was not a Frame packet....this clip will break");

		// Just started recording (OnRecordStart)
		sm_pPreviousFramePacket = pCurrFramePacket;
	}
	
	// Is this frame packet in a different block to the previous frame packet?
	bool prevFrameWasDiffBlock = pCurrFramePacket->GetFrameRef().GetBlockIndex() != sm_pPreviousFramePacket->GetFrameRef().GetBlockIndex();

	// Calculate the offset from the previous packet to the current one
	u32 prevOffset = (u32)((u8*)pCurrFramePacket - (u8*)sm_pPreviousFramePacket);
	if(prevFrameWasDiffBlock)
	{	// If there has been a block change then we need to take that into account...
		CBlockInfo* pBlock = sm_ReplayBufferInfo.GetPrevBlock(controller.GetCurrentBlock());

		prevOffset = (u32)((u8*)sm_pPreviousFramePacket - (u8*)pBlock->GetData());
		replayFatalAssertf(prevOffset < BYTES_IN_REPLAY_BLOCK, "Offset to previous packet is too large! (%u)", prevOffset);
	}
	pCurrFramePacket->SetPrevFrameOffset(prevOffset, prevFrameWasDiffBlock);

	u32 nextOffset = (u32)((u8*)pCurrFramePacket - (u8*)sm_pPreviousFramePacket);
	if(prevFrameWasDiffBlock)
	{
		CBlockInfo* pBlock = controller.GetCurrentBlock();
		nextOffset = (u32)((u8*)pCurrFramePacket - (u8*)pBlock->GetData());
		replayFatalAssertf(nextOffset < BYTES_IN_REPLAY_BLOCK, "Offset to next packet is too large! (%u)", nextOffset);
	}
	sm_pPreviousFramePacket->SetNextFrameOffset(nextOffset, prevFrameWasDiffBlock);

 	const CPacketFrame* pTestPacket1 = GetNextFrame(sm_pPreviousFramePacket);
	replayFatalAssertf(pCurrFramePacket == sm_pPreviousFramePacket || pTestPacket1, "pTestPacket1 is NULL, offsets are %s, %u (Need to see this in the debugger!)", sm_pPreviousFramePacket->GetNextFrameIsDiffBlock() ? "true" : "false", sm_pPreviousFramePacket->GetNextFrameOffset());
 	if(pTestPacket1 && !pTestPacket1->ValidatePacket())
	{
		DumpReplayPackets((char*)sm_pPreviousFramePacket);
		DumpReplayPackets((char*)pTestPacket1);
		replayAssertf(false, "Failed check on 'next' packet");
	}
	const CPacketFrame* pTestPacket2 = GetPrevFrame(pCurrFramePacket);
	replayFatalAssertf(pCurrFramePacket == sm_pPreviousFramePacket || pTestPacket2, "pTestPacket2 is NULL, offsets are %s, %u (Need to see this in the debugger!)", pCurrFramePacket->GetPrevFrameIsDiffBlock() ? "true" : "false", pCurrFramePacket->GetPrevFrameOffset());
	if(pTestPacket2 && !pTestPacket2->ValidatePacket())
	{
		DumpReplayPackets((char*)pCurrFramePacket);
		DumpReplayPackets((char*)pTestPacket2);
		replayAssertf(false, "Failed check on 'previous' packet");
	}

	sm_pPreviousFramePacket = pCurrFramePacket;

	//---------------------------------------------------------------------
	// Record all Entities
	PF_PUSH_TIMEBAR("Replay-Record Entities");
	

	sm_interfaceManager.PostRecordFrame(controller, blockChange, expectedRecordSizes, sizesIndex);

	//---------------------------------------------------------------------
	// Record the next frame
	controller.RecordPacket<CPacketNextFrame>(expectedRecordSizes[sizesIndex++]);

	pCurrFramePacket->SetOffsetToEvents(controller.GetCurrentPosition() - framePacketPosition);

	PF_POP_TIMEBAR();

	//---------------------------------------------------------------------
	// Record attachment information
	ReplayAttachmentManager::Get().RecordData(controller);

	//---------------------------------------------------------------------
	// Record the events from last frame that relied on entities created this frame
	PF_PUSH_TIMEBAR("Replay-RecordEvents");
	sm_eventManager.RecordEvents(controller, blockChange, expectedRecordSizes[sizesIndex++]);

	//---------------------------------------------------------------------
	// Record Any IPL changes
	ReplayIPLManager::RecordData(controller, blockChange);

	//Reset mistmatches on block change
	if(blockChange)
	{
		ReplayInteriorManager::ResetEntitySetLastState(true);
	}

	//---------------------------------------------------------------------
	// Record Any MovieMeshSet changes
	ReplayMeshSetManager::RecordData(controller, blockChange);

	//---------------------------------------------------------------------
	// Record Any Non Replay Tracked Entity Hides (buildings, for now)
	ReplayHideManager::RecordData(controller, blockChange);

	//---------------------------------------------------------------------
	//Record any frame packet data
	RecordFramePacketFlags(pCurrFramePacket);

	//---------------------------------------------------------------------
	// Set the end frame packet
	controller.SetPacket<CPacketEnd>();

	PF_POP_TIMEBAR();

	//replayDebugf1("Recorded frame - Position: %u, Limit: %u, Difference: %d", controller.GetCurrentPosition(), controller.GetAddressLimit(), (s32)controller.GetAddressLimit() - (s32)controller.GetCurrentPosition());
	controller.DisableRecording();
	controller.GetCurrentBlock()->SetSizeUsed(controller.GetCurrentPosition());

	// Increment the frame count here
	++sm_uCurrentFrameInBlock;
	replayAssertf(sm_uCurrentFrameInBlock >= 0, "The current frame in this block is invalid!");
	controller.GetCurrentBlock()->SetFrameCount(controller.GetCurrentBlock()->GetFrameCount()+1);
}

bool CReplayMgrInternal::ShouldDisabledCameraMovement()
{
	CPlayerInfo* pPlayerInfo = NULL;
	CPed* ped = CGameWorld::FindLocalPlayer();
	if (ped)
	{
		pPlayerInfo = ped->GetPlayerInfo();
	}
	
	return (pPlayerInfo == NULL || pPlayerInfo->AreControlsDisabled() || sm_SuppressCameraMovementThisFrame || camInterface::IsDominantRenderedCameraAnyFirstPersonCamera() );
}

bool CReplayMgrInternal::ShouldSetCameraDisabledFlag()
{
	bool shouldDisableCameraMovement = ShouldDisabledCameraMovement();

	//If we don't stop recording on camera disabled we don't need to worry about delaying
	//the frame flag for a frame
	if( !CReplayControl::ShouldStopRecordingOnCameraMovementDisabled() )
	{
		return shouldDisableCameraMovement;
	}

	//Due to recording an extra frame when saving we need to delay setting the camera disabled flag
	//for a frame so the recording has a chance to move to the temp buffer before the flag is set.
	if ( shouldDisableCameraMovement && sm_SuppressCameraMovementCameraFrameCount == -1 )
	{
		sm_SuppressCameraMovementCameraFrameCount = 1;
	}
	else if( shouldDisableCameraMovement && sm_SuppressCameraMovementCameraFrameCount >= 0)
	{
		if( sm_SuppressCameraMovementCameraFrameCount > 0 )
		{
			--sm_SuppressCameraMovementCameraFrameCount;
		}
		else
		{
			return true;
		}
	}
	else
	{
		sm_SuppressCameraMovementCameraFrameCount = -1;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::RecordFramePacketFlags(CPacketFrame* pCurrFramePacket)
{
	CPed* ped = CGameWorld::FindLocalPlayer();
	if (ped)
	{
		if(camInterface::IsDominantRenderedCameraAnyFirstPersonCamera())
		{
			pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_FIRST_PERSON);
		}

		if( ShouldSetCameraDisabledFlag() )
		{
			pCurrFramePacket->SetFrameFlags(FRAME_PACKET_DISABLE_CAMERA_MOVEMENT);
		}
	}

	if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack())
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_CUTSCENE);
	}

	if(camInterface::IsRenderedCameraInsideVehicle())
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_CAM_IN_VEHICLE);
	}

	if(CVfxHelper::GetCamInVehicleFlag())
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_CAM_IN_VEHICLE_VFX);
	}

#if __BANK
	if( CReplayControl::ShowDisabledRecordingMessage() )
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDING_DISABLED);
	}
#endif // __BANK

	if(NetworkInterface::IsGameInProgress())
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_MULTIPLAYER);
	}

	if(CTheScripts::GetIsInDirectorMode())
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_DIRECTOR_MODE);
	}

	if(CRenderPhaseCascadeShadowsInterface::IsUsingAircraftShadows())
	{
		pCurrFramePacket->SetFrameFlags(FRAME_PACKET_RECORDED_USE_AIRCRAFT_SHADOWS);
	}

	ResetRecordingFrameFlags();
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::IsPlaybackFlagSet(u32 flag)
{
	const CPacketFrame* pCurrFramePacket = sm_currentFrameInfo.GetFramePacket();
	if(pCurrFramePacket)
	{
		return pCurrFramePacket->IsFrameFlagSet(flag);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::IsPlaybackFlagSetNextFrame(u32 flag)
{
	const CPacketFrame* pCurrFramePacket = sm_currentFrameInfo.GetFramePacket();
	if(pCurrFramePacket)
	{
		const CPacketFrame* pNextFramePacket = GetNextFrame(pCurrFramePacket);
		if(pNextFramePacket)
		{
			return pNextFramePacket->IsFrameFlagSet(flag);
		}
	}

	return false;
}

bool CReplayMgrInternal::IsUsingRecordedCamera()
{
	const camBaseCamera* pActiveCamera = camInterface::GetReplayDirector().GetActiveCamera();
	if(pActiveCamera && pActiveCamera->GetIsClassId(camReplayRecordedCamera::GetStaticClassId()))
	{
		return camInterface::IsRenderingCamera(*pActiveCamera);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::IsSafeToTriggerUI()
{
	return !IsPreCachingScene() && !IsClipTransition();
}

//========================================================================================================================
void CReplayMgrInternal::PlayBackThisFrame(bool bFirstTime)
{
	///////////////////////////////////
	//  - Play Back Replay Buffer -
	///////////////////////////////////

	// Just as a note for the future...
	// We could on several consecutive frames be playing the same address
	// from the replay buffer.  However this will be with different interpolation
	// values so this is valid.
	CAddressInReplayBuffer oPrevInterpFrame = sm_currentFrameInfo.GetModifiableAddress();
	ReplayController controller(oPrevInterpFrame, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
	controller.SetPlaybackFlags( sm_eReplayState );
	controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
	controller.EnablePlayback();
	controller.SetInterp(sm_fLastInterpValue);
	controller.SetFrameInfos(sm_firstFrameInfo, sm_currentFrameInfo, sm_nextFrameInfo, sm_lastFrameInfo);
	PlayBackThisFrameInterpolation(controller, bFirstTime, !sm_DoFullProcessPlayback);
	controller.DisablePlayback();

	///////////////////////////////////
	// - Play Back History Buffer -
	///////////////////////////////////
	
	static const int pokeCount = 20;
	static int pokeAgain = 0;

	if(sm_DoFullProcessPlayback && sm_currentFrameInfo.GetHistoryAddress().GetBlock() != NULL)
	{
		sm_PokeGameSystems = false;
		{
			CAddressInReplayBuffer address = sm_currentFrameInfo.GetHistoryAddress();
			ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
			controller.SetPlaybackFlags(sm_eReplayState);
			controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
			controller.EnablePlayback();
			PlayBackHistory(controller, GetCurrentTimeAbsoluteMs());
			controller.DisablePlayback();
		}

		if(!sm_isSettingUp)
		{
			// Look in the next frame's worth of event packets as some can be stored in the buffer a little late :(
			// We're still guarded by the time value so nothing should be played out of order.
			{
				CAddressInReplayBuffer address = sm_nextFrameInfo.GetHistoryAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_nextFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(sm_eReplayState);
				controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
				controller.EnablePlayback();
				PlayBackHistory(controller, GetCurrentTimeAbsoluteMs(), true);
				controller.DisablePlayback();
			}
		}

		CPacketSetBuildingTint::ProcessDeferredPackets();

		ReplayPedExtension::ResetVariationSet();

#if __BANK	// Check that we're not taking too long getting into the clip...
		static u32 readyTimeoutTimer = (u32)-1;
		const u32 readyTimeout = 15000;
		if(sm_isSettingUpFirstFrame)
		{
			if(readyTimeoutTimer == (u32)-1)
			{
				readyTimeoutTimer = fwTimer::GetSystemTimeInMilliseconds();
			}
			else if(fwTimer::GetSystemTimeInMilliseconds() - readyTimeoutTimer > readyTimeout)
			{
				replayDebugf1("Timing out waiting for clip to set up... Took over %u ms", readyTimeout);
			}
		}
		else
		{
			readyTimeoutTimer = (u32)-1;
		}
#endif // __BANK

		if(sm_isSettingUp && sm_isSettingUpFirstFrame && !IsWaitingOnEventsForExport() && sm_doInitialPrecache)
		{
 			KickPrecaching(true);
			sm_doInitialPrecache = false;

			// Reset the poke again counter
			pokeAgain = pokeCount;
		}

		if(sm_DoFullProcessPlayback && sm_WantsToKickPrecaching == false && sm_KickPrecaching == false && IsPreCachingScene() == false)
		{
			//ReplayInteriorManager::DoHacks(IsPlaybackFlagSet(FRAME_PACKET_RECORDED_MULTIPLAYER));

			if(sm_NeedEventJump)
			{
				// Made it all the way through preprocess so all the entities should be set up.
				// Now we need to process jumping through events...if we return false from here
				// then there are still events to be processed
				ProcessEventJump();

				// Reset the poke again counter
				pokeAgain = pokeCount;
			}
		}

		// If the poke again counter isn't at zero then do a poke and count down...
		if(pokeAgain > 0 && !sm_PokeGameSystems)
		{
			sm_PokeGameSystems = true;
			pokeAgain--;
		}
	
		if(sm_pPlaybackController)
		{
			if(sm_isSettingUp &&
				!IsWaitingOnEventsForExport() &&			// Not still waiting for events to extract (if we're exporting)
				(!sm_pPlaybackController->IsExportingToVideoFile() || !sm_PokeGameSystems) &&	// Not still giving the game systems a wee poke (if we're exporting)
				sm_WantsToKickPrecaching == false && sm_KickPrecaching == false && IsPreCachingScene() == false) //make sure we've finished any precaching as well
			{
				// Take off the flags that we're setting up
				sm_isSettingUp = false;	

				BANK_ONLY(readyTimeoutTimer = (u32)-1;)	// Reset timeout

				ReplayInteriorManager::ProcessInteriorStatePackets();

				replayDebugf1("Finished frame setup!");

				if(!sm_pPlaybackController->IsExportingToVideoFile())
				{
					//Probe attach viewport entity
					CViewport *pVp = gVpMan.GetCurrentViewport();
					if(pVp && pVp->GetAttachEntity() && pVp->GetAttachEntity()->GetIsDynamic())
					{
						//Ugly const cast as attached entity wasn't ever designed to be altered.
						CEntity* pEntity = const_cast<CEntity*>(pVp->GetAttachEntity());
						CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pEntity);
						if(pDynamicEntity->GetPortalTracker())
						{
							pDynamicEntity->GetPortalTracker()->RequestRescanNextUpdate();
						}
					}
				}

				CPed* pPlayer = FindPlayerPed();
				if(pPlayer && pPlayer->IsFirstPersonCameraOrFirstPersonSniper())
					pPlayer->UpdateFpsCameraRelativeMatrix();

				ReplayClothPacketManager::OnFineScrubbingEnd();

				//this should ensure that the correct initial state for a clip is set depending on the mode
				if( sm_pPlaybackController && sm_isSettingUpFirstFrame )
				{
					sm_pPlaybackController->OnClipPlaybackStart();
				}

				if(sm_isSettingUpFirstFrame)
				{
					PostFX::ResetAdaptedLuminance();
					Lights::ResetPedLight();
					CParaboloidShadow::RequestJumpCut();
				}

				sm_isSettingUpFirstFrame = false;
			}
		}
	}

    // Fire callback as we're finished processing entities, don't want for events as they can take a long time
    if(!sm_isSettingUp && sm_allEntitiesSetUp == false && sm_DoFullProcessPlayback)
    {
        sm_allEntitiesSetUp = true;
        DoOnPlaybackSetup();
    }

	sm_oHistoryProcessPlayback = oPrevInterpFrame;

	sm_ReplayBufferInfo.SetAllBlocksNotPlaying();
	sm_currentFrameInfo.GetAddress().GetBlock()->SetBlockPlaying(true);

	CReplayRenderTargetHelper::GetInstance().Process();

#if __BANK
	u32 uFlags =	(REPLAY_BANK_ENTITIES_DURING_REPLAY|
					REPLAY_BANK_PFX_EFFECT_LIST|
					REPLAY_BANK_ENTITIES_TO_DEL);
	UpdateBanks(uFlags);
#endif

	/*TODO4FIVE if (sm_bDisableLumAdjust && !gCamInterface.GetFading() && !IsPreCachingScene())
	{
		CPostFX::SetEnableLumAdapt(false);
		sm_bDisableLumAdjust = false;
	}*/
}


//////////////////////////////////////////////////////////////////////////
// This processes event packets during a jump in the timeline.  If we tried to
// extract a packet that returned 'Retry' then we bail (return false) and let
// the game go round a frame before trying again.
bool CReplayMgrInternal::ProcessEventJump()
{
	replayDebugf3("ProcessEventJump");
	sm_PokeGameSystems = true;

	while(sm_eventJumpNextFrameInfo.GetGameTime() < sm_JumpToEventsTime)
	{
		CReplayState state(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_JUMP);
		{
			CAddressInReplayBuffer address = sm_eventJumpCurrentFrameInfo.GetHistoryAddress();
			ReplayController controller(address, sm_ReplayBufferInfo, sm_eventJumpCurrentFrameInfo.GetFrameRef());
			controller.SetPlaybackFlags(state);
			controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
			controller.EnablePlayback();
			PlayBackHistory(controller, sm_eventJumpNextFrameInfo.GetGameTime());
			controller.DisablePlayback();

			if(sm_eventManager.IsRetryingEvents())
			{
				return false;
			}
		}

		MoveCurrentFrame(1, sm_eventJumpCurrentFrameInfo, sm_eventJumpPrevFrameInfo, sm_eventJumpNextFrameInfo);
	}

	replayDebugf3("Finished ProcessEventJump");
	sm_NeedEventJump = false;
	return true;
}

//========================================================================================================================
bool CReplayMgrInternal::UpdateInterpolationFrames(u64 uAccurateGameTime, bool preprocessOnly)
{
	PF_AUTO_PUSH_TIMEBAR("Replay-UpdateInterpolationFrames");
	bool bIsLastFrame = false, bIsFirstFrame = false;
	u32 accurateGameTime32 = (u32)(uAccurateGameTime >> 16);

	sm_JumpToEventsTime = accurateGameTime32;	// Update this so we know how far to jump to.
	if(sm_NeedEventJump && uAccurateGameTime < sm_eventJumpCurrentFrameInfo.GetGameTime16BitShifted())
	{
		// Set the earliest time that we found the event manager said it needs to retry
		sm_eventJumpCurrentFrameInfo= sm_currentFrameInfo;
		sm_eventJumpPrevFrameInfo	= sm_prevFrameInfo;
		sm_eventJumpNextFrameInfo	= sm_nextFrameInfo;
		sm_NeedEventJump = false;
		replayDebugf3("Kill the event jump!");
	}


	// Jumping...
	if(GetCurrentPlayBackState().IsSet(REPLAY_CURSOR_JUMP))
	{
#if DO_REPLAY_OUTPUT
		FrameRef frameJumpingTo = GetFrameAtTime(accurateGameTime32);
#endif // DO_REPLAY_OUTPUT
		FrameRef currentFrame	= sm_currentFrameInfo.GetFrameRef();
		FrameRef nextFrame		= GetNextFrame(sm_currentFrameInfo.GetFramePacket())->GetFrameRef();
		(void)currentFrame; (void)nextFrame;
		replayDebugf3("*** Jump from %u:%u to %u:%u @ %lu", currentFrame.GetBlockIndex(), currentFrame.GetFrame(), frameJumpingTo.GetBlockIndex(), frameJumpingTo.GetFrame(), uAccurateGameTime);

		// Jumping forwards...
		if(sm_nextFrameInfo.GetGameTime16BitShifted() <= uAccurateGameTime)
		{
			while(sm_nextFrameInfo.GetGameTime16BitShifted() <= uAccurateGameTime && !IsLastFrame(sm_nextFrameInfo.GetFrameRef()))
			{
				CReplayState state(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_JUMP);
				{
					if(sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP_LINEAR))
					{
						state.SetState(REPLAY_CURSOR_JUMP | REPLAY_CURSOR_JUMP_LINEAR);
					}

					CAddressInReplayBuffer address = sm_currentFrameInfo.GetModifiableAddress();
					ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
					controller.SetPlaybackFlags(state);
					controller.EnablePlayback();
					sm_IsBeingDeleteByReplay = true;

					while(!controller.IsNextFrame())
					{
						if(controller.IsEndOfPlayback())
							return true;

						ePlayPktResult result = sm_interfaceManager.JumpPackets(controller);
						if(result == PACKET_UNHANDLED)
						{
							controller.AdvanceUnknownPacket(controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize());
						}
						else if(result == PACKET_ENDBLOCK)
							return true;
					}
					controller.DisablePlayback();

					sm_IsBeingDeleteByReplay = false;
				}

				// Process events here when jumping...
				// This will result in some being not played as we weren't able to 
				// find entities or were trying to feed too much to the game system.
				{
					CAddressInReplayBuffer address = sm_currentFrameInfo.GetHistoryAddress();
					ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
					controller.SetIsFirstFrame(IsFirstFrame(sm_currentFrameInfo.GetFrameRef()));
					controller.SetPlaybackFlags(state);
					controller.EnablePlayback();
					PlayBackHistory(controller, sm_nextFrameInfo.GetGameTime(), true);
					controller.DisablePlayback();

					if(sm_eventManager.IsRetryingEvents() && !sm_NeedEventJump)
					{
						// Set the earliest time that we found the event manager said it needs to retry
						sm_eventJumpCurrentFrameInfo= sm_currentFrameInfo;
						sm_eventJumpPrevFrameInfo	= sm_prevFrameInfo;
						sm_eventJumpNextFrameInfo	= sm_nextFrameInfo;
						sm_NeedEventJump = true;					// Need to process the events after jumping over several frames...
					}
				}

				MoveCurrentFrame(1, sm_currentFrameInfo, sm_prevFrameInfo, sm_nextFrameInfo);
			}
		}
		// Jumping backwards...
		else if(uAccurateGameTime < sm_currentFrameInfo.GetGameTime16BitShifted())
		{
			// As we had to check in the 'next frame' when playing forwards for any packets that creeped over
			// we must check the next frame when jumping backwards.  This can't really go wrong as by definition
			// the next frame's packets should be unplayed and therefore not rewind accidentally...we're just here
			// to check for those wee extra packets that've gone over.
			{
				CReplayState state(REPLAY_STATE_PLAY | REPLAY_DIRECTION_BACK | REPLAY_CURSOR_JUMP);
				CAddressInReplayBuffer address = sm_nextFrameInfo.GetHistoryAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_nextFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(state);
				controller.EnablePlayback();
				PlayBackHistory(controller, sm_nextFrameInfo.GetGameTime());
				controller.DisablePlayback();
			}

			while(uAccurateGameTime < sm_currentFrameInfo.GetGameTime16BitShifted())
			{
				CReplayState state(REPLAY_STATE_PLAY | REPLAY_DIRECTION_BACK | REPLAY_CURSOR_JUMP);
				{
					if(sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP_LINEAR))
					{
						state.SetState(REPLAY_CURSOR_JUMP | REPLAY_CURSOR_JUMP_LINEAR);
					}
					CAddressInReplayBuffer address = sm_currentFrameInfo.GetModifiableAddress();
					ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
					controller.SetPlaybackFlags(state);
					controller.SetFineScrubbing(m_FineScrubbing);
					controller.EnablePlayback();
					sm_IsBeingDeleteByReplay = true;

					while(!controller.IsNextFrame())
					{
						if(controller.IsEndOfPlayback())
							return true;

						ePlayPktResult result = sm_interfaceManager.JumpPackets(controller);
						if(result == PACKET_UNHANDLED)
						{
							controller.AdvanceUnknownPacket(controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize());
						}
						else if(result == PACKET_ENDBLOCK)
							return true;
					}
					controller.DisablePlayback();

					sm_IsBeingDeleteByReplay = false;
				}

				{
					CAddressInReplayBuffer address = sm_currentFrameInfo.GetHistoryAddress();
					ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
					controller.SetPlaybackFlags(state);
					controller.EnablePlayback();
					PlayBackHistory(controller, sm_nextFrameInfo.GetGameTime());
					controller.DisablePlayback();
				}

				ReplayPedExtension::ResetVariationSet();

				MoveCurrentFrame(-1, sm_currentFrameInfo, sm_prevFrameInfo, sm_nextFrameInfo);

				//when jumping backwards we need to extract the first frame so monitored packets have a chance to reset
				bool isFirstFrame = IsFirstFrame(sm_currentFrameInfo.GetFrameRef());
				if(uAccurateGameTime == sm_currentFrameInfo.GetGameTime() && isFirstFrame)
				{
					CAddressInReplayBuffer address = sm_currentFrameInfo.GetHistoryAddress();
					ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
					controller.SetPlaybackFlags(state);
					controller.SetIsFirstFrame(isFirstFrame);
					controller.EnablePlayback();
					PlayBackHistory(controller, sm_nextFrameInfo.GetGameTime());
					controller.DisablePlayback();
				}
			}
		}

		g_SpeechManager.ReconcilePainBank();
	}
	// Moving forwards...
	else if(sm_nextFrameInfo.GetGameTime16BitShifted() <= uAccurateGameTime)
	{	
		// Check for last frame before starting the loop
		bIsLastFrame = IsLastFrame(accurateGameTime32);

		// ...this is the default action for a replay.  We process all the entity packets
		// and event packets through each frame until we reach the point we're supposed to be
		replayAssertf(bIsLastFrame || GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_FWD), "State is not forward!");

		// Advancing loop
		while (sm_nextFrameInfo.GetGameTime16BitShifted() <= uAccurateGameTime && !bIsLastFrame)
		{
			// Advance the frame
			sm_fLastInterpValue = 0.0f;
			
			if (!gRenderThreadInterface.IsActive())
			{
				g_ptFxManager.WaitForUpdateToFinish();
			}

			{
				CAddressInReplayBuffer address = sm_currentFrameInfo.GetModifiableAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(sm_eReplayState);
				controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
				controller.EnablePlayback();
				controller.SetInterp(0.0f);
				controller.SetFrameInfos(sm_firstFrameInfo, sm_currentFrameInfo, sm_nextFrameInfo, sm_lastFrameInfo);
				PlayBackThisFrameInterpolation(controller, preprocessOnly);
				controller.DisablePlayback();
			}

			{
				CAddressInReplayBuffer address = sm_currentFrameInfo.GetHistoryAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(sm_eReplayState);
				controller.SetIsFirstFrame(IsFirstFrame(controller.GetCurrentFrameRef()));
				controller.EnablePlayback();
				PlayBackHistory(controller, sm_nextFrameInfo.GetGameTime());
				controller.DisablePlayback();
			}

			// Fire off the pfx update, to update the effects in the loop
			if (!gRenderThreadInterface.IsActive())
			{
				float deltaTime = (sm_nextFrameInfo.GetGameTime() - sm_currentFrameInfo.GetGameTime()) / 1000.0f;
				g_ptFxManager.Update(deltaTime);
			}

			MoveCurrentFrame(1, sm_currentFrameInfo, sm_prevFrameInfo, sm_nextFrameInfo);

			// Check for last frame here too
			bIsLastFrame = IsLastFrame(accurateGameTime32);
		}

		if (!gRenderThreadInterface.IsActive())
		{
			g_ptFxManager.WaitForUpdateToFinish();
		}
	}
	// Moving backwards...
	else if(uAccurateGameTime < sm_currentFrameInfo.GetGameTime16BitShifted())
	{
		// ...this is pretty much the same as moving forwards...
		bIsFirstFrame = IsFirstFrame(accurateGameTime32);

		replayFatalAssertf(GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK), "State is not backwards!");
		while (uAccurateGameTime < sm_currentFrameInfo.GetGameTime16BitShifted() && !bIsFirstFrame)
		{	
			{
				CAddressInReplayBuffer address = sm_currentFrameInfo.GetModifiableAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(sm_eReplayState);
				controller.SetIsFirstFrame(IsFirstFrame(GetCurrentTimeAbsoluteMs()));
				controller.EnablePlayback();
				controller.SetInterp(0.0f);
				controller.SetFrameInfos(sm_firstFrameInfo, sm_currentFrameInfo, sm_nextFrameInfo, sm_lastFrameInfo);
				PlayBackThisFrameInterpolation(controller, preprocessOnly);
				controller.DisablePlayback();
			}

			{
				CAddressInReplayBuffer address = sm_currentFrameInfo.GetHistoryAddress();
				ReplayController controller(address, sm_ReplayBufferInfo, sm_currentFrameInfo.GetFrameRef());
				controller.SetPlaybackFlags(sm_eReplayState);
				controller.EnablePlayback();
				PlayBackHistory(controller, sm_nextFrameInfo.GetGameTime());
				controller.DisablePlayback();
			}

			ReplayPedExtension::ResetVariationSet();

			MoveCurrentFrame(-1, sm_currentFrameInfo, sm_prevFrameInfo, sm_nextFrameInfo);
			bIsFirstFrame = IsFirstFrame(accurateGameTime32);
		}

		g_SpeechManager.ReconcilePainBank();
	}

	return (bIsLastFrame || bIsFirstFrame);
}

//========================================================================================================================
bool CReplayMgrInternal::IsLastFrame(u32 uAccurateGameTime)
{
	if (sm_nextFrameInfo.GetGameTime() <= uAccurateGameTime &&
		sm_nextFrameInfo.GetGameTime() == sm_uPlaybackEndTime)
	{
		// Adjust the time in case we went over the end time (interpolation)
		SetCurrentTimeMs((float)(sm_uPlaybackEndTime - sm_uPlaybackStartTime));

		return true;
	}
	return false;
}

bool CReplayMgrInternal::IsLastFrame(FrameRef frame)
{
	return (GetLastFrame().GetFrameRef() == frame);
}

//========================================================================================================================
bool CReplayMgrInternal::IsFirstFrame(u32 uAccurateGameTime)
{
	if (uAccurateGameTime <= sm_currentFrameInfo.GetGameTime() &&
		sm_currentFrameInfo.GetGameTime() == sm_uPlaybackStartTime)
	{
		return true;
	}
	return false;
}

bool CReplayMgrInternal::IsFirstFrame(FrameRef frame)
{
	return (GetFirstFrame().GetFrameRef() == frame);
}

//========================================================================================================================
float CReplayMgrInternal::GetVfxDelta()
{
	if(sm_playbackFrameStepDelta > 0)
		return (float)sm_playbackFrameStepDelta;
	else
		return	0.033f;
}

//========================================================================================================================
bool CReplayMgrInternal::PlayBackThisFrameInterpolation(ReplayController& controller, bool preprocessOnly, bool entityMayNotExist)
{
	replayAssert(sm_IsBeingDeleteByReplay == false);
	sm_IsBeingDeleteByReplay = true;

	if(preprocessOnly)
	{
		sm_interfaceManager.ProcessRewindDeletions(controller);
	}

	//PF_AUTO_PUSH_TIMEBAR("Replay-PlayBackThisFrameInterpolation");
	while(!controller.IsNextFrame())
	{
		if(controller.IsEndOfPlayback())
			return true;

		ePlayPktResult result = sm_interfaceManager.PlayPackets(controller, preprocessOnly, entityMayNotExist);
		if(result == PACKET_UNHANDLED)
		{
			controller.AdvanceUnknownPacket(controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize());
		}
		else if(result == PACKET_ENDBLOCK)
			return true;
	}
	
	controller.AdvancePacket();

	if(preprocessOnly)
	{
		sm_interfaceManager.CreateDeferredEntities(controller);
	}

	sm_IsBeingDeleteByReplay = false;

	return false;
}


//========================================================================================================================
void CReplayMgrInternal::ClearOldSounds()
{
	if(!audNorthAudioEngine::IsAudioUpdateCurrentlyRunning() && !audDriver::GetMixer()->IsCapturing())
	{
		UnfreezeAudio();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::PlayBackHistory(ReplayController& controller, u32 LastTime, bool entityMayBeAbsent)
{
	//PF_AUTO_PUSH_TIMEBAR("Replay-PlayBackHistory");
	sm_uLastPlaybackTime	= LastTime;

	return sm_eventManager.PlayBackHistory(controller, LastTime, entityMayBeAbsent);	
}


//========================================================================================================================
// Loop through all the History packets until we come to the one immediately after the time specified
bool CReplayMgrInternal::RewindHistoryBufferAddress(CAddressInReplayBuffer& roBufferAddress, u32 uTime)
{
	ReplayController controller(roBufferAddress, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();

	while(pPacket->GetPacketID() == PACKETID_END_HISTORY || sm_eventManager.IsRelevantHistoryPacket(pPacket->GetPacketID()))
	{
		if(controller.IsEndOfPlayback())
		{
			controller.DisablePlayback();
			return true;
		}

		CPacketEvent const* pHistoryPacket = controller.ReadCurrentPacket<CPacketEvent>();
		if(pHistoryPacket->GetGameTime() >= uTime)
			break;

		pPacket = pHistoryPacket;

		controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
		pPacket = controller.ReadCurrentPacket<CPacketBase>();
	}

	controller.DisablePlayback();

	return true;
}

//========================================================================================================================
bool CReplayMgrInternal::ProcessHistoryBufferFromStart(ReplayController& controller, u32 uTime)
{
	CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();

	while(pPacket->GetPacketID() == PACKETID_END_HISTORY || sm_eventManager.IsRelevantHistoryPacket(pPacket->GetPacketID()))
	{
		if(controller.IsEndOfPlayback())
			return true;

		CPacketEvent const* pHistoryPacket = controller.ReadCurrentPacket<CPacketEvent>();
		if(pHistoryPacket->GetGameTime() >= uTime)
			break;

		pPacket = pHistoryPacket;

		switch (controller.GetCurrentPacketID())
		{
			case PACKETID_SCRIPTEDBUILDINGSWAP:
			{
				CPacketBuildingSwap const* pDamagePacket = controller.ReadCurrentPacket<CPacketBuildingSwap>();

				CEventInfo<void> eventInfo;
				pDamagePacket->Extract(eventInfo);

				controller.AdvancePacket();
			}
			break;

			default:
			{
				controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
			}
			break;
		}
		pPacket = controller.ReadCurrentPacket<CPacketBase>();
	}

	return true;
}

//========================================================================================================================
bool CReplayMgrInternal::InvalidateHistoryBufferFromFrameToPlay(CAddressInReplayBuffer& roBufferAddress, u32 uTime)
{
	CAddressInReplayBuffer oBufferAddress = roBufferAddress;

	ReplayController controller(oBufferAddress, sm_ReplayBufferInfo);
	CPacketBase* pPacket = controller.GetCurrentPacket<CPacketBase>();

	while(pPacket->GetPacketID() == PACKETID_END_HISTORY || sm_eventManager.IsRelevantHistoryPacket(pPacket->GetPacketID()))
	{
		if(controller.IsEndOfPlayback())
			return true;

		CPacketEvent* pHistoryPacket = controller.GetCurrentPacket<CPacketEvent>();
		if(pHistoryPacket->GetGameTime() >= uTime)
			break;

		pPacket = pHistoryPacket;

		sm_eventManager.InvalidatePacket(controller, pPacket->GetPacketID(), pPacket->GetPacketSize());

		pPacket = controller.GetCurrentPacket<CPacketBase>();
	}

	// Store the start of the history process address
	sm_oHistoryProcessPlayback = oBufferAddress;

	return true;
}


//========================================================================================================================
void CReplayMgrInternal::FinishPlayback()
{
	/*TODO4FIVE CStreaming::MakeSpaceFor(0, 512 * 1024 * 1024);*/
	//CPortalTracker::RemoveFromActivatingTrackerList(m_pTrackingObj->GetPortalTracker());
	//CWorld::Remove(m_pTrackingObj);

	// Tracking object deleted though DeleteAllEntities below.
	//m_pTrackingObj = NULL;

	//m_bForceRescan = false;

	if (sm_uMode == REPLAYMODE_EDIT)
	{
		bool clearEntities = true;

		//if we're at the end of the clip pause playback.
		if( sm_bIsLastClipFinished || IsSinglePlaybackMode() )
		{
			SetNextPlayBackState(REPLAY_STATE_PAUSE);
		}

		replayAssert(IsFinishPlaybackPending() && "CReplayMgrInternal::FinishPlayback: Do not call FinishPlayback directly; use SetFinishPlaybackPending() instead");

		if (gRenderThreadInterface.IsActive())
		{
			gRenderThreadInterface.Flush();
		}

		if ( IsClipTransition() )
		{
			// Reset Audio
			audNorthAudioEngine::StopAllSounds( true ); // The Dan Van Zant case!
			sm_interfaceManager.StopAllEntitySounds();
			g_FrontendAudioEntity.SetReplayMuteVolume( 0.0f );
			audStreamSlot::CleanUpSlotsForReplay();

			// If we're exporting a video then music is paused at a lower level to maintain sync
			if(!sm_pPlaybackController || !sm_pPlaybackController->IsExportingToVideoFile())
			{
				g_RadioAudioEntity.StopReplayMusic();
			}
		}
		else
		{
			// STOP ENCODING!!!
			if( sm_pPlaybackController )
			{
				sm_pPlaybackController->OnPlaybackFinished();
				clearEntities = false;
			}

			UnfreezeAudio();	// this actually stops all audio and Unpauses it so stuff can clean up

			g_RadioAudioEntity.StopReplayMusic();
		}

		/*TODO4FIVE CDummyObject::ShowAll(true);*/
		//PrintOutPackets();

		if(sm_bUserCancel) //todo4five a temp way of returning to the game after you started a replay
		{
			sm_desiredMode = REPLAYMODE_DISABLED;
			Disable();
		}

//		/*TODO4FIVE CWater::ClearDynamicWater();*/
//
//
//		g_FrontendAudioEntity.SetGameWorldDuckVolume(0.0f);
//
//		// re-add dummy objects to world 
//		////CDummyObject::RemoveAllFromWorld(false);
//
//		sm_uMode = REPLAYMODE_RECORD;
//
//		// shutdown the replay camera
//		ProcessReplayCamera(sm_uMode);
//
//		/*TODO4FIVE CVisualEffects::ShutdownSession();
//		CVisualEffects::InitSession();*/
//
//#if CHECK_REPLAY_HISTORY_HEADERS
//		delete sm_Packet;
//		sm_Packet = NULL;
//#endif
//
//		Init();
	}
}

//========================================================================================================================
void CReplayMgrInternal::UpdateClip()
{
	if( !sm_pPlaybackController )
	{
		return;
	}

	sm_pPlaybackController->OnClipPlaybackUpdate();

	if ( !IsLoadingClipTransition() && IsRequestingClipTransition() && !IsFinishPlaybackPending() )
	{
		LoadClip( sm_pPlaybackController->GetCurrentRawClipOwnerId(), sm_pPlaybackController->GetCurrentRawClipFileName() );
		sm_eReplayState.SetState( REPLAY_STATE_CLIP_TRANSITION_LOAD );

		if(IsPausingMusicOnClipEnd()) 
		{
			audNorthAudioEngine::Pause();
			g_AudioEngine.GetSoundManager().Pause(2);
		}
		//else	
		//{
		//	audGtaAudioEngine::StopAllSounds( true ); // The Dan Van Zant case!
		//}
	}
	else if( sm_uMode == REPLAYMODE_EDIT || DidLastFileOpFail())
	{
		if ( !sm_pPlaybackController->GetPlaybackSingleClipOnly() && !IsClipTransition() )
		{
			float uCurrentTime	= GetCurrentTimeRelativeMsFloat();
			s32 iCurrentClip    = sm_pPlaybackController->GetCurrentClipIndex();
			float clipStartTime	= sm_pPlaybackController->GetClipStartNonDilatedTimeMs( iCurrentClip );
			float clipEndTime	= sm_pPlaybackController->GetClipNonDilatedEndTimeMs( iCurrentClip );
			s32 iClipCount	    = sm_pPlaybackController->GetTotalClipCount();
			s32 iClip		    = iCurrentClip;
			bool bLastClipSRL	= false;

			if (DidLastFileOpFail() == false && !(replayVerifyf(clipEndTime <= sm_uPlaybackEndTime - sm_uPlaybackStartTime, "Clip end time is longer than the playback length!")))
			{
				clipEndTime = float(sm_uPlaybackEndTime - sm_uPlaybackStartTime);
			}

			CReplayState nextState;

			if( sm_pPlaybackController->IsPendingClipJump() )
			{
				iClip = sm_pPlaybackController->GetPendingJumpClip();

				if( iClip != iCurrentClip )
				{
					u32 setState = 0;

					bool const pauseOnJump = sm_pPlaybackController->GetPendingJumpClipNonDilatedTimeMs() >= 0;
					if( pauseOnJump )
					{
						PauseReplayPlayback();
						setState = REPLAY_DIRECTION_FWD | REPLAY_STATE_PAUSE;
					}
					else
					{
						bool const forwardAfterJump = iClip > iCurrentClip;
						setState = forwardAfterJump ? REPLAY_DIRECTION_FWD | REPLAY_STATE_PAUSE : REPLAY_DIRECTION_BACK | REPLAY_STATE_PAUSE;
						
						m_cursorSpeed = forwardAfterJump ? fabsf( m_cursorSpeed ) : -fabsf( m_cursorSpeed );
					}

					nextState.SetState(setState);
				}
			}
			else
			{
				bool bIsRewinding	= IsCursorRewinding() || GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK);

				if( bIsRewinding && (uCurrentTime <= clipStartTime || DidLastFileOpFail()) )
				{
					iClip = iCurrentClip - 1;

					if (iClip < 0)
					{
						iClip = 0;
					}
				}
				else if( !bIsRewinding && (uCurrentTime >= clipEndTime || DidLastFileOpFail()) )
				{
					iClip = iCurrentClip + 1;

					if (iClip == iClipCount)
					{
						sm_bCheckForPreviewExit = true;
					}

					if (iClip >= iClipCount)
					{
						iClip = iClipCount - 1;
#if USE_SRLS_IN_REPLAY
						if(IsCreatingSRL())
						{
							bLastClipSRL = true;
						}
#endif
					}

#if USE_SRLS_IN_REPLAY
					if(sm_pPlaybackController->IsCurrentClipLastClip() && !IsCreatingSRL())
#else
					if(sm_pPlaybackController->IsCurrentClipLastClip())
#endif
					{
						//tells finishplayback() that this was the last clip and we're at the end of it so we can pause.
						sm_bIsLastClipFinished = true;
					}
				}
				
				if( iClip != iCurrentClip )
				{
					bool const forwardAfterJump = iClip > iCurrentClip;
					m_cursorSpeed = forwardAfterJump ? fabsf( m_cursorSpeed ) : -fabsf( m_cursorSpeed );

					nextState = sm_eReplayState;
					nextState.SetState(REPLAY_CURSOR_NORMAL);
				}
			}
			
			//if we are done recording and SRL, playback the scene again but this time using the SRL
			if( iClip != iCurrentClip || bLastClipSRL)
			{				
#if USE_SRLS_IN_REPLAY
				if(IsCreatingSRL())
				{
					//we have created the SRL by this point, go back to the start of the clip and 
					//play it back while using the SRL
					sm_eReplayState.SetState( REPLAY_PLAYBACK_SRL );
					SetNextPlayBackState(REPLAY_PLAYBACK_SRL);

					SetLastPlayBackState( GetCurrentPlayBackState() );
					sm_eReplayState.SetState( REPLAY_STATE_CLIP_TRANSITION_REQUEST );
					sm_pPlaybackController->OnClipChanged( iCurrentClip );
					SetFinishPlaybackPending(true);

					//we also need to finish recording the scene
					gStreamingRequestList.Finish();
					//setup for prestreaming when the scene has reset properly
					return;
				}
				else if(IsUsingSRL())
				{
					//move on to the next clip and start recording the SRL again
					sm_eReplayState.SetState( REPLAY_RECORD_SRL );
					nextState.SetState(REPLAY_RECORD_SRL);

					//get rid of the old srl and prep for a new one to be recorded
					gStreamingRequestList.Finish();					
					gStreamingRequestList.DeleteRecording();

					if(!bLastClipSRL)
					{
						gStreamingRequestList.RecordForReplay(); //tell the SRLs that we ar eabout to start recording again
						gStreamingRequestList.SetTime(0);
					}				
				}
#endif

				u32 thisClipSequenceHash = 0;
				u32 nextClipSequenceHash = 0;

				thisClipSequenceHash = sm_FileManager.GetClipSequenceHash( sm_pPlaybackController->GetCurrentRawClipOwnerId(), sm_pPlaybackController->GetCurrentRawClipFileName(), false);

				SetLastPlayBackState( GetCurrentPlayBackState() );
				SetNextPlayBackState(nextState.GetState());
				sm_eReplayState.SetState( REPLAY_STATE_CLIP_TRANSITION_REQUEST );
				sm_pPlaybackController->OnClipChanged( iClip );

				nextClipSequenceHash = sm_FileManager.GetClipSequenceHash( sm_pPlaybackController->GetCurrentRawClipOwnerId(), sm_pPlaybackController->GetCurrentRawClipFileName(), true);

				replayDebugf1("Previous clip hash %u, Next clip hash %u", thisClipSequenceHash, nextClipSequenceHash);
				if(thisClipSequenceHash && (thisClipSequenceHash == nextClipSequenceHash))
				{
					replayDebugf1(" - Hashes match...clips are in sequence");
				}
				sm_pPlaybackController->OnClipPlaybackEnd();

				SetFinishPlaybackPending(true);
			}
		}
	}
}

//========================================================================================================================
bool CReplayMgrInternal::ShouldPlayBackPacket( eReplayPacketId packetId, u32 playBackFlags, s32 replayId, u32 packetTime, u32 startTimeWindow )
{
	(void)packetId;
	(void)playBackFlags;
	(void)replayId;
	(void)packetTime;
	(void)startTimeWindow;

#if 0
	if( replayId < 0 )
	{
		return false;	// Invalid Replay ID
	}

	if (!CanRender())
	{
		return false;
	}

	u32 packetGroupFlag = GetFlagForPacket( packetId );
	
	if( !( packetGroupFlag & playBackFlags ) )
	{
		return false;	// Packet Group Disabled for Play Back
	}

	// Perform Packet Group Specific Checks
	switch( packetGroupFlag )
	{
	case REPLAY_PACKET_GROUP_SOUND:
		{
			if( GetPlayBackState() != REPLAY_STATE_PLAY )
			{
				return false;	// Sounds Only Play During the "Play" State
			}

			if( packetTime < startTimeWindow )
			{
				return false;
			}
		}
		break;

	default:
		break;
	}
#endif
	// Packet Passed All Conditions
	return true;
}

//========================================================================================================================
u32 CReplayMgrInternal::GetFlagForPacket( eReplayPacketId packetId )
{
	if( packetId > PACKETID_GAME_MIN && packetId < PACKETID_GAME_MAX )
	{
		return REPLAY_PACKET_GROUP_GAME;
	}
	else if( (packetId > PACKETID_VEHICLE_MIN && packetId < PACKETID_VEHICLE_MAX) || packetId == PACKETID_AMPHAUTOUPDATE || packetId == PACKETID_AMPHQUADUPDATE || packetId == PACKETID_SUBCARUPDATE)
	{
		return REPLAY_PACKET_GROUP_VEHICLE;
	}
	else if( packetId > PACKETID_BUILDING_MIN && packetId < PACKETID_BUILDING_MAX )
	{
		return REPLAY_PACKET_GROUP_BUILDING;
	}
	else if( packetId > PACKETID_PED_MIN && packetId < PACKETID_PED_MAX )
	{
		return REPLAY_PACKET_GROUP_PED;
	}
	else if( packetId > PACKETID_OBJECT_MIN && packetId < PACKETID_OBJECT_MAX )
	{
		return REPLAY_PACKET_GROUP_OBJECT;
	}
	else if( packetId > PACKETID_PICKUP_MIN && packetId < PACKETID_PICKUP_MAX )
	{
		return REPLAY_PACKET_GROUP_PICKUP;
	}
	else if( packetId > PACKETID_FRAG_MIN && packetId < PACKETID_FRAG_MAX )
	{
		return REPLAY_PACKET_GROUP_FRAG;
	}
	else if( packetId > PACKETID_PARTICLE_MIN && packetId < PACKETID_PARTICLE_MAX )
	{
		return REPLAY_PACKET_GROUP_PARTICLE;
	}
	else if( packetId > PACKETID_PTEX_MIN && packetId < PACKETID_PTEX_MAX )
	{
		return REPLAY_PACKET_GROUP_PROJ_TEX;
	}
	else if(( packetId > PACKETID_SOUND_MIN && packetId < PACKETID_SOUND_MAX) || ( packetId > PACKETID_SOUND2_MIN && packetId < PACKETID_SOUND2_MAX))
	{
		return REPLAY_PACKET_GROUP_SOUND;
	}
	else if( packetId > PACKETID_SMASH_MIN && packetId < PACKETID_SMASH_MAX )
	{
		return REPLAY_PACKET_GROUP_SMASH;
	}
	else if( packetId == PACKETID_SCRIPTEDBUILDINGSWAP )
	{
		return REPLAY_PACKET_GROUP_SCRIPT;
	}
	else
	{
		replayAssertf( false, "CReplayMgrInternal::GetFlagForPacket: Invalid Packet ID - %d", packetId);
		return REPLAY_PACKET_GROUP_NONE;
	}
}


//========================================================================================================================
void CReplayMgrInternal::ProcessReplayCamera(eReplayMode eType)
{
	if (eType == REPLAYMODE_RECORD)
	{
		// Shutdown the replay cam as we're going back into game mode
		/*TODO4FIVE CCamReplay* pReplayCam = camInterface::GetReplayDirector().GetReplayCam();
		replayAssertf(pReplayCam, "ReplayMgr: Cannot find the Replay camera!");

		pReplayCam->FindCam(CCamTypes::CAM_TYPE_REPLAY)->SetActive(false);
		pReplayCam->SetActive(false);										// switch off the Replay camera
		pReplayCam->SetPropagateUp(false);
		pReplayCam->Reset();

		CPostFX::SetEnableLumAdapt(true);*/
	}
	else if (eType == REPLAYMODE_EDIT)
	{
		// Start up the replay cam as we're entering playback mode
		/*TODO4FIVE CCamReplay*  pReplayCam = gCamSys.GetDefaultManager()->GetReplayCam();
		replayAssertf(pReplayCam, "CReplayMgrInternal: Cannot find the Replay camera!");
		pReplayCam->SetActive(true);
		pReplayCam->FindCam(CCamTypes::CAM_TYPE_REPLAY)->SetActive(true);
		pReplayCam->SetPropagateUp(true);*/
	}
	else
	{
		replayAssertf(false, "Wrong replay mode in CReplayMgrInternal::ProcessReplayCamera");
	}
}

//========================================================================================================================
s32 CReplayMgrInternal::GetCurrentEpisodeIndex()
{
	//TODO4FIVE implement

	return 0;
}

//========================================================================================================================
void CReplayMgrInternal::Display()
{
	if (sm_uMode == REPLAYMODE_RECORD)
	{		// Print some general debug shite
		return;
	}

#if __BANK && 0
	// PC: this seems redundant now that we render a replay hud element during playback, disabling.
	TimeCount = (TimeCount + 1) & 65535;
	u32 Pulse = TimeCount & 0x3F;
	{
		float fAlpha = Max(192.0f - 192.0f * ((float)Pulse/63.0f), 32.0f);
		u32 uAlpha = (u32)fAlpha;
		CTextLayout oTextLayout;
		oTextLayout.SetScale(Vector2(0.5f, 0.5f));
		oTextLayout.SetColor(Color32((u32)255, (u32)255, (u32)200, uAlpha));
		oTextLayout.SetDropColor(Color32( (u32)0, (u32)0, (u32)0, uAlpha));
		oTextLayout.SetDropShadow(true);
		oTextLayout.SetStyle(FONT_STYLE_STANDARD);

		if (sm_uMode == REPLAYMODE_EDIT)
		{
			//			strcpy(gString, "R");
			//			AsciiToUnicode(gString, gUString);
			//oTextLayout.Render(Vector2(1.0f/20.0f, 17.0f/20.0f), "Replay Edit");
			oTextLayout.Render(Vector2(9.0f/20.0f, 2.0f/20.0f), "Replay Edit");
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::FindStartOfPlayback(CAddressInReplayBuffer& oBufferAddress)
{
	// Find the one that is 'being filled'...
	CBlockProxy block = sm_ReplayBufferInfo.FindFirstBlock();

	// We should now be at the first buffer that is full or being filled
	oBufferAddress.SetPosition(0);
	oBufferAddress.SetBlock(block.GetBlock());
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::MoveCurrentFrame(const s32 numberOfFramesToMove, CFrameInfo& currFrame, CFrameInfo& prevFrame, CFrameInfo& nextFrame)
{
	ASSERT_ONLY(FrameRef previousFrame = currFrame.GetFrameRef();)

	const CPacketFrame* pFramePacket = GetRelativeFrame(numberOfFramesToMove, currFrame.GetFramePacket());
	REPLAY_CHECK(pFramePacket != NULL, NO_RETURN_VALUE, "Failed to move current frame... %p, %u:%u, %d, ", currFrame.GetFramePacket(), previousFrame.GetBlockIndex(), previousFrame.GetFrame(), numberOfFramesToMove);
	
	CBlockInfo* pBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(pFramePacket->GetFrameRef().GetBlockIndex());

	currFrame.SetFramePacket(pFramePacket);
	currFrame.SetAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pFramePacket - pBlock->GetData())));
	currFrame.SetHistoryAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetOffsetToEvents()));
	
	FrameRef currentFrame = currFrame.GetFrameRef();

	// Previous frame
	if(IsFirstFrame(currentFrame) == false)
	{
		pFramePacket = GetPrevFrame(currFrame.GetFramePacket());
		pBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(pFramePacket->GetFrameRef().GetBlockIndex());
		prevFrame.SetFramePacket(pFramePacket);
		prevFrame.SetAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pFramePacket - pBlock->GetData())));
		prevFrame.SetHistoryAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetOffsetToEvents()));
	}
	else
	{
		prevFrame.SetFramePacket(NULL);
	}

	// Next frame
	if(IsLastFrame(currentFrame) == false)
	{
		pFramePacket = GetNextFrame(currFrame.GetFramePacket());
		pBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(pFramePacket->GetFrameRef().GetBlockIndex());
		nextFrame.SetFramePacket(pFramePacket);
		nextFrame.SetAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pFramePacket - pBlock->GetData())));
		nextFrame.SetHistoryAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetOffsetToEvents()));
	}
	else
	{
		nextFrame.SetFramePacket(NULL);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::UpdateCreatePacketsForNewBlock(CAddressInReplayBuffer newBlockStart)
{
	ReplayController controller(newBlockStart, sm_ReplayBufferInfo); 
	controller.EnablePlayback();

	sm_interfaceManager.ResetCreatePacketsForCurrentBlock();

// 	while(controller.IsEndOfBlock() == false)
// 	{
// 		const CPacketBase* pPacket = controller.ReadCurrentPacket<CPacketBase>();

		sm_interfaceManager.LinkCreatePacketsForNewBlock(controller);
// 		controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
// 	}

	controller.DisablePlayback();
}


//////////////////////////////////////////////////////////////////////////
const CPacketFrame* CReplayMgrInternal::GetRelativeFrame(s32 nodeOffset, const CPacketFrame* pCurrFramePacket)
{
	while(nodeOffset != 0)
	{
		if(nodeOffset > 0)
		{
			pCurrFramePacket = GetNextFrame(pCurrFramePacket);
			--nodeOffset;
		}
		else if(nodeOffset < 0)
		{
			pCurrFramePacket = GetPrevFrame(pCurrFramePacket);
			++nodeOffset;
		}
	}

	return pCurrFramePacket;
}


//////////////////////////////////////////////////////////////////////////
u32 CReplayMgrInternal::GetGameTimeAtRelativeFrame(s32 nodeOffset)
{
	if(sm_currentFrameInfo.GetFramePacket() == NULL)
	{
		replayAssertf(false, "Current Frame is invalid, offset was %d", nodeOffset);
		return 0;
	}

	const CPacketFrame* pCurrFramePacket = sm_currentFrameInfo.GetFramePacket();
	if(nodeOffset != 0)
	{
		pCurrFramePacket = GetRelativeFrame(nodeOffset, sm_currentFrameInfo.GetFramePacket());
		if(pCurrFramePacket == NULL)
		{
			replayAssertf(false, "Can't find frame relative to current (%u:%u), offset %d", sm_currentFrameInfo.GetFramePacket()->GetFrameRef().GetBlockIndex(), sm_currentFrameInfo.GetFramePacket()->GetFrameRef().GetFrame(), nodeOffset);
			return 0;
		}
	}
	pCurrFramePacket->ValidatePacket();

	const CPacketBase* pBasePacket = pCurrFramePacket;
	const CPacketBase* pPrevPacket = pBasePacket;
	do
	{
		pPrevPacket = pBasePacket;
		pBasePacket = (const CPacketBase*)((u8*)pBasePacket + pBasePacket->GetPacketSize());
		
		if(pBasePacket->ValidatePacket() == false || pBasePacket->GetPacketID() == PACKETID_FRAME)
		{
			replayAssertf(false, "failed finding game time packet, pCurrFramePacket=%p", pCurrFramePacket);
			return 0;
		}
	}
	while(pBasePacket->GetPacketID() != PACKETID_GAMETIME);

	const CPacketGameTime* pGameTimePacket = (const CPacketGameTime*)pBasePacket;
	if(!pGameTimePacket->ValidatePacket())
	{
		DumpReplayPackets((char*)pPrevPacket);
		DumpReplayPackets((char*)pGameTimePacket);
		replayAssertf(false, "Failed to find CPacketGameTime in GetGameTimeAtRelativeFrame");
	}
	return pGameTimePacket->GetGameTime();
}

//////////////////////////////////////////////////////////////////////////
FrameRef CReplayMgrInternal::GetFrameAtTime(u32 time)
{
	CAddressInReplayBuffer oBufferAddress;
	FindStartOfPlayback(oBufferAddress);
	ReplayController controller(oBufferAddress, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	const CPacketFrame* pCurrFramePacket		= controller.ReadCurrentPacket<CPacketFrame>();	
	const CPacketFrame* pNextFramePacket		= NULL;

	while(controller.GetCurrentPacketID() != PACKETID_GAMETIME)
	{
		controller.ReadCurrentPacket<CPacketBase>();
		controller.AdvancePacket();
	}
	const CPacketGameTime* pCurrGameTimePacket	= controller.ReadCurrentPacket<CPacketGameTime>();
	const CPacketGameTime* pNextGameTimePacket	= NULL;

	if(time <= pCurrGameTimePacket->GetGameTime())
	{
		controller.DisablePlayback();
		return pCurrFramePacket->GetFrameRef();	// First frame
	}

	do 
	{
		pNextFramePacket = GetNextFrame(pCurrFramePacket);
		if(pNextFramePacket == NULL)
			break;	// Reached the past the end packet?

		const CPacketBase* pPacket = (const CPacketBase*)((const u8*)pNextFramePacket + pNextFramePacket->GetPacketSize());
		while(pPacket->GetPacketID() != PACKETID_GAMETIME)
		{
			pPacket = (const CPacketBase*)((const u8*)pPacket + pPacket->GetPacketSize());
			pPacket->ValidatePacket();
		}
		pNextGameTimePacket = (const CPacketGameTime*)pPacket;

		if(!pNextGameTimePacket->ValidatePacket())
		{
			DumpReplayPackets((char*)pCurrFramePacket);
			DumpReplayPackets((char*)pNextGameTimePacket);
			replayAssertf(false, "Failed to find CPacketGameTime in GetFrameAtTime");
			controller.DisablePlayback();
			return FrameRef::INVALID_FRAME_REF;
		}
		
		if(time < pNextGameTimePacket->GetGameTime())
			break;

		if(pNextFramePacket == pCurrFramePacket)
			break;

		pCurrFramePacket	= pNextFramePacket;
	} while (1);

	controller.DisablePlayback();

	return pCurrFramePacket->GetFrameRef();
}


//////////////////////////////////////////////////////////////////////////
const CPacketFrame* CReplayMgrInternal::GetNextFrame(const CPacketFrame* pCurrFramePacket)
{
	if((pCurrFramePacket->GetNextFrameOffset() == 0 && pCurrFramePacket->GetNextFrameIsDiffBlock() == false) || (sm_lastFrameInfo.GetFramePacket() != NULL && pCurrFramePacket->GetFrameRef() == sm_lastFrameInfo.GetFrameRef()))
		return NULL;

	const CPacketFrame* pNextFramePacket	= NULL;
	if(pCurrFramePacket->GetNextFrameIsDiffBlock())
	{
		CBlockInfo* pBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(pCurrFramePacket->GetFrameRef().GetBlockIndex());
		CBlockInfo* pNextBlock = NULL;
		pNextBlock = sm_ReplayBufferInfo.GetNextBlock(pBlock);
		
		if(IsRecording() && pNextBlock != NULL && pNextBlock->GetStatus() != REPLAYBLOCKSTATUS_BEINGFILLED)
		{
			// Bit awkward....we were recording into a normal block but now
			// we're recording into a temporary block while we save out the
			// Clip.  Hunt down the correct temporary block...
			CBlockInfo* pFirstTempBlock = sm_ReplayBufferInfo.GetFirstTempBlock();
			CBlockInfo* pTempBlock = pFirstTempBlock;
			do 
			{
				if(pTempBlock == NULL || pTempBlock->GetNormalBlockEquiv() == pNextBlock)
					break;
				pNextBlock = pTempBlock;
				pTempBlock = sm_ReplayBufferInfo.GetNextBlock(pTempBlock);
			} while (pTempBlock != pFirstTempBlock);
		}
		replayAssertf(pNextBlock != NULL, "Failed to find the 'NextBlock' to validate the next frame packet (Curr block: %u, block count %u (+%u), Temp is on: %s)", pCurrFramePacket->GetFrameRef().GetBlockIndex(), sm_ReplayBufferInfo.GetNumNormalBlocks(), sm_ReplayBufferInfo.GetNumTempBlocks(), sm_ReplayBufferInfo.IsAnyTempBlockOn() ? "True" : "False");
		if(pNextBlock != NULL)
		{
			pNextFramePacket = (const CPacketFrame*)((const u8*)pNextBlock->GetData() + pCurrFramePacket->GetNextFrameOffset());
		}
	}
	else
	{
		pNextFramePacket = (const CPacketFrame*)((const u8*)pCurrFramePacket + pCurrFramePacket->GetNextFrameOffset());
	}

	if(pNextFramePacket != NULL && !pNextFramePacket->ValidatePacket())
	{
		DumpReplayPackets((char*)pCurrFramePacket);
		DumpReplayPackets((char*)pNextFramePacket);
		replayAssertf(false, "Failed to find next frame in GetNextFrame");
	}

	return pNextFramePacket;
}


//////////////////////////////////////////////////////////////////////////
const CPacketFrame* CReplayMgrInternal::GetPrevFrame(const CPacketFrame* pCurrFramePacket)
{
	REPLAY_CHECK(pCurrFramePacket, NULL, "pCurrFramePacket is NULL going into CReplayMgrInternal::GetPrevFrame");
	if(pCurrFramePacket->GetPrevFrameOffset() == 0 && pCurrFramePacket->GetPrevFrameIsDiffBlock() == false)
		return NULL;

	u16 currBlockIndex = pCurrFramePacket->GetFrameRef().GetBlockIndex();
	const CBlockInfo* pCurrBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(currBlockIndex);
	REPLAY_CHECK(pCurrBlock, NULL, "pCurrBlock is NULL in CReplayMgrInternal::GetPrevFrame currBlockIndex:%u", currBlockIndex);
	const u8* pData = (const u8*)pCurrFramePacket;
	const CPacketFrame* pPrevFramePacket = NULL;
	
	if(pCurrFramePacket->GetPrevFrameIsDiffBlock())
	{
		const CBlockInfo* pPrevBlock = NULL;
		if(sm_ReplayBufferInfo.IsAnyTempBlockOn())
		{	// Temporary blocks are being used...so find it...
			pCurrBlock = sm_ReplayBufferInfo.FindTempBlockFromSessionBlock(currBlockIndex);

			if(sm_ReplayBufferInfo.GetFirstTempBlock() == pCurrBlock && sm_ReplayBufferInfo.GetTempBlocksHaveLooped() == false)
			{	// Current block is the first temp block...AND the current temp block has an equivalent...
				pPrevBlock = sm_ReplayBufferInfo.GetPrevBlock(pCurrBlock->GetNormalBlockEquiv());
			}
			else
			{	// Any other temp block...
				pPrevBlock = sm_ReplayBufferInfo.GetPrevBlock(pCurrBlock);
			}
		}
		else
		{
			pPrevBlock = sm_ReplayBufferInfo.GetPrevBlock(pCurrBlock);
		}

		replayFatalAssertf(pPrevBlock != NULL, "pPrevBlock cannot be NULL here");
		pData = (const u8*)pPrevBlock->GetData();

		pPrevFramePacket = (const CPacketFrame*)(pData + pCurrFramePacket->GetPrevFrameOffset());
		REPLAY_CHECK(pPrevFramePacket->ValidatePacket(), NULL, "Failing to validate pPrevFramePacket in CReplayMgrInternal::GetPrevFrame %p, %u", pCurrFramePacket, pCurrFramePacket->GetPrevFrameOffset());
	}
	else
	{
		pPrevFramePacket = (const CPacketFrame*)(pData - pCurrFramePacket->GetPrevFrameOffset());
		REPLAY_CHECK(pPrevFramePacket->ValidatePacket(), NULL, "Failing to validate pPrevFramePacket in CReplayMgrInternal::GetPrevFrame %p, %u", pCurrFramePacket, pCurrFramePacket->GetPrevFrameOffset());
	}


	if(pPrevFramePacket != NULL && !pPrevFramePacket->ValidatePacket())
	{
		DumpReplayPackets((char*)pCurrFramePacket);
		DumpReplayPackets((char*)pPrevFramePacket);
		replayAssertf(false, "Failed to find next frame in GetPrevFrame");
	}

	return pPrevFramePacket;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::ApplyFades()
{
	// Setup starting buffer address.
	CAddressInReplayBuffer oBufferAddress;
	FindStartOfPlayback(oBufferAddress);
	ReplayController controller(oBufferAddress, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	u32 frame = 0;

	while(controller.IsEndOfBlock() == false || controller.IsLastBlock() == false)
	{
		const CPacketBase* pPacket = controller.ReadCurrentPacket<CPacketBase>();
		pPacket->ValidatePacket();
		eReplayPacketId ePacketID = pPacket->GetPacketID();

		if(controller.IsEndOfBlock())
		{
			controller.AdvanceBlock();
			continue;
		}
		else
		{
			sm_interfaceManager.ApplyFades(controller, frame);
		}

		if(ePacketID == PACKETID_NEXTFRAME)
		{
			++frame;
		}
		controller.AdvancePacket();	
	}

	controller.DisablePlayback();
	sm_interfaceManager.ApplyFadesSecondPass();
}

//========================================================================================================================
void CReplayMgrInternal::EnableRecording()		
{
	REPLAYTRACE("*CReplayMgrInternal::EnableRecording");

	SetNumberOfReplayBlocks(NumberOfReplayBlocksFromSettings);

	if(SetupReplayBuffer(NumberOfReplayBlocks, NUMBER_OF_TEMP_REPLAY_BLOCKS))
	{
		SettingsChanged = false;
	}
	else
	{
		replayFatalAssertf(sm_IsMemoryAllocated == false, "Failed to set up replay buffer but memory is still allocated");
		return;
	}

	sm_desiredMode = REPLAYMODE_RECORD;
	sm_uCurrentFrameInBlock = 0;
	sm_pPreviousFramePacket = NULL;

	sm_recordingBufferProgress = 0;

	sm_wantBufferSwap = false;

	// Reset frame hash values... 0 is invalid
	sm_previousFrameHash = sm_currentFrameHash = 0;

	sm_ReplayBufferInfo.GetFirstBlock()->SetStatus(REPLAYBLOCKSTATUS_BEINGFILLED);
	sm_ReplayBufferInfo.GetFirstBlock()->SetFrameCount(0);
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	sm_ReplayBufferInfo.GetFirstBlock()->GenerateThumbnail();
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
	sm_lastFrameInfo.SetFramePacket(NULL);

	if(IsAwareOfWorldForRecording() == false)
	{
		sm_interfaceManager.RegisterAllCurrentEntities();
		CPacketSndLoadScriptBank::expirePrevious = -1;
	}

	sm_interfaceManager.EnableRecording();
 	sm_interfaceManager.ResetEntityCreationFrames();

	ReplayHideManager::OnRecordStart();

	// Reset any that were stored in this list so that they don't get carried over to the next time we start recording
	// ...Normally this reset is only called when the packet is actually recorded :(
	CPacketDisabledThisFrame::Reset();

	sm_lockRecordingFrameRatePrev = sm_lockRecordingFrameRate;
	sm_numRecordedFramesPerSecPrev = sm_numRecordedFramesPerSec;
	if(sm_lockRecordingFrameRate)
	{
		replayDebugf1(" - Recording at %u fps", sm_numRecordedFramesPerSec);
	}
	else
	{
		replayDebugf1(" - Recording at game framerate");
	}
}

//========================================================================================================================
void CReplayMgrInternal::DisableRecording()
{
	REPLAYTRACE("*CReplayMgrInternal::DisableRecording");

	sm_desiredMode = REPLAYMODE_DISABLED;	

	if(sm_OverwatchEnabled == false)
	{
		sm_interfaceManager.DeregisterAllCurrentEntities();
	}
	else
	{
		sm_interfaceManager.ResetEntityCreationFrames();
	}
}

//========================================================================================================================
void CReplayMgrInternal::TriggerPlayback()
{
	//nothing was recorded, so don' try and start
	if(sm_ReplayBufferInfo.GetNumBlocksSet() == 0 || sm_desiredMode == REPLAYMODE_DISABLED)
	{
		g_bWorldHasBeenCleared = true;
		return;
	}
	replayDebugf1("*CReplayMgrInternal::TriggerPlayback");

#if USE_SAVE_SYSTEM
	sm_ReplaySaveStructure.Init();
	if( !IsClipTransition() && (g_ReplaySaveState == REPLAY_SAVE_NOT_STARTED) )
	{
#if ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
		sm_ScriptSavePreparationState = SCRIPT_SAVE_PREP_WAITING_FOR_RESPONSE_FROM_SCRIPT;
		g_ReplaySaveState = REPLAY_SAVE_BEGIN_WAITING_FOR_SCRIPT_TO_PREPARE;
#else	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
		if (CGenericGameStorage::PushOnToSavegameQueue(&sm_ReplaySaveStructure))
		{
			g_ReplaySaveState = REPLAY_SAVE_IN_PROGRESS;
		}
#endif	//	ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	}
#endif

	sm_desiredMode = REPLAYMODE_WAITINGFORSAVE;
	sm_KickPreloader = true;	// Kick the preloader as soon as the world streaming has finished.
}


//////////////////////////////////////////////////////////////////////////
CPed* pOldPlayerPed = NULL;
void CReplayMgrInternal::PrepareForPlayback()
{
	REPLAYTRACE("*CReplayMgrInternal::PrepareForPlayback");
	
	pOldPlayerPed = FindPlayerPed();
	//u32 bufferIdx = 0;

	SetReplayPlayerInteriorProxyIndex(-1);

	sm_eReplayState = sm_nextReplayState;
	sm_bIsLastClipFinished = false;

	sm_NeedEventJump = false;
	sm_JumpToEventsTime = 0;

	sm_eventManager.Reset();
	sm_interfaceManager.Clear();

	sm_IgnorePostLoadStateChange = false;
	sm_uLastPlaybackTime = 0;
	sm_fTimeScale = 1.0f;
	sm_isSettingUpFirstFrame = true;

	// Shadows don't appear to be moving at all during playback.  If the
	// shadow freezing is enabled then disable it while replay playback is playing
	// (we'll set it back again to what it was when we kill the replay)
 	extern ShadowFreezing ShadowFreezer;
 	sm_shadowsFrozenInGame = ShadowFreezer.IsEnabled();
 	if(sm_shadowsFrozenInGame == true)
 	{
 		ShadowFreezer.SetEnabled(false);
		sm_haveFrozenShadowSetting = true;
 	}

	if (!IsClipTransition())
	{
		g_FrontendAudioEntity.SetReplayMuteVolume( 0.0f );
		audNorthAudioEngine::StopAllSounds();
		sm_interfaceManager.StopAllEntitySounds();

		//todo4five crashes
		/*bool bHasMusicTrack = true;
		const char* szTrack = GetMontage()->GetMusicClip();
		if( szTrack == NULL || szTrack[0] == '\0' )
			bHasMusicTrack = false;

		sm_MusicDuration = 0;
		
		if(bHasMusicTrack)
		{
			Sound uncompressedMetadata;
			Sound *streamingMetadata = (Sound*)g_AudioEngine.GetSoundManager().GetFactory().GetMetadataPtr(rage::atStringHash(GetMontage()->GetMusicClip()));
			replayAssert(streamingMetadata);
			replayAssert(streamingMetadata->ClassId == StreamingSound::TYPE_ID); //AUD_OBJ_TYPE_STREAMINGSOUND);

			if(streamingMetadata) 
			{
				StreamingSound *streamingSound = (StreamingSound*)audSound::DecompressMetadata(streamingMetadata, uncompressedMetadata);
				sm_MusicDuration = (s32)streamingSound->Duration;
			}
		}*/
	}

	// Find the replay block we're currently writing into
	FindStartOfPlayback(sm_oPlayback);

	sm_oPlaybackStart = sm_oPlayback;

	replayAssert(sm_IsBeingDeleteByReplay == false);
	sm_IsBeingDeleteByReplay = true;
	
	OnEnterClip();

	sm_IsBeingDeleteByReplay = false;
	/*sm_eventManager.ResetPlaybackPfxInsts();*/

	{
		CAddressInReplayBuffer historyStartAddress = sm_oPlaybackStart;
		ReplayController controller(historyStartAddress, sm_ReplayBufferInfo); 
		controller.EnableModify();
		sm_eventManager.ResetHistoryPacketsAfterTime(controller, 0);
		controller.DisableModify();
	}

	SetupInitialPlayback();

	// Adjust fade values for entities being created/destroyed.
	ApplyFades();

	Vector3		CentreCoors;
	GetFirstCameraFocusCoords(&CentreCoors);

	sm_vinewoodIMAPSsForceLoaded = false;
	if(sm_shouldLoadReplacementCasinoImaps)
	{
		// If the clip doesn't reference the Vinewood update then we need to force load the new
		// Casino IMAPS if we're close enough  - Fixes url:bugstar:5797430
		float dist2 = CentreCoors.Dist2(Vector3(959, 40, 116));
		const float casinoDist = 180.0f; // Roughly where the LOD changes
		if(dist2 < (casinoDist*casinoDist))
		{
			for(int i = 0; i < NELEM(vinewoodIMAPsToForceLoad); ++i)
			{
				strLocalIndex slotIndex = fwMapDataStore::GetStore().FindSlot(vinewoodIMAPsToForceLoad[i]);
				if (!fwMapDataStore::GetStore().HasObjectLoaded(slotIndex))
				{
					fwMapDataStore::GetStore().StreamingRequest(slotIndex, STRFLAG_DONTDELETE);
					sm_vinewoodIMAPSsForceLoaded = true;
					replayDebugf1("Force loading %s", vinewoodIMAPsToForceLoad[i]);
				}
			}
		}
	}

	{
		sm_oReplayFullFrameTimer.Reset();
		sm_oReplayInterpFrameTimer.Reset();
		SetPlaybackFrameStepDelta(0);
		SetExportFrameStep(0.0f);

		//! Clear the transition flags if they were set, since we are loaded now
		sm_eReplayState.ClearState( REPLAY_STATE_CLIP_TRANSITION_REQUEST | REPLAY_STATE_CLIP_TRANSITION_LOAD );
		sm_nextReplayState.ClearState( REPLAY_STATE_CLIP_TRANSITION_REQUEST | REPLAY_STATE_CLIP_TRANSITION_LOAD );

		if(sm_pPlaybackController)
		{
			replayDebugf2("Whole clip start time:   %7u", sm_uPlaybackStartTime);
			replayDebugf2("Whole clip end time:     %7u", sm_uPlaybackEndTime);
			replayDebugf2("Whole clip length:       %7u", sm_uPlaybackEndTime - sm_uPlaybackStartTime);

			const s32 clipIndex = sm_pPlaybackController->GetCurrentClipIndex();

			// Grab any overriden starting time, clear it after we have it
			float const pendingJumpTime = sm_pPlaybackController->GetPendingJumpClipNonDilatedTimeMs();
			sm_pPlaybackController->ClearPendingJumpClipNonDilatedTimeMs();

			float startTime = sm_pPlaybackController->GetClipStartNonDilatedTimeMs(clipIndex);
			float endTime = sm_pPlaybackController->GetClipNonDilatedEndTimeMs(clipIndex);

			replayDebugf2("Sub-clip start time: %f", startTime);
			replayDebugf2("Sub-clip end time:   %f", endTime);
			replayDebugf2("Sub-clip length:     %f", endTime-startTime);

			if (!(replayVerifyf(startTime >= 0, "Start time must be positive!")))
			{
				startTime = 0;
			}
			replayAssertf(endTime <= sm_uPlaybackEndTime - sm_uPlaybackStartTime, "End time must be less than the whole clip length!");

			// If higher-level systems gave us a valid jump time, use that
			if( pendingJumpTime >= 0 && pendingJumpTime >= startTime && pendingJumpTime < endTime )
			{
				startTime = pendingJumpTime;
			}
			// Otherwise if we're playing backwards then set the start time to the very end of the clip
			else if(sm_eReplayState.IsSet(REPLAY_DIRECTION_BACK))
			{
				//for safety we're moving the smallest amount we can from the end to avoid trigger >= logic that
				//would reload the clip you just rewound from, also make sure we're pause to prevent the same effect
				startTime = endTime -1;
				sm_eReplayState.SetState(REPLAY_STATE_PAUSE | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_NORMAL);
				SetNextPlayBackState(REPLAY_STATE_PAUSE | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_NORMAL);
			}

			sm_doInitialPrecache = true;
			JumpTo(startTime, JO_Force | JO_FreezeRenderPhase);	
		}
	}
	
#if __BANK
	s_fSecondsElapsed = (float)(GetPlayBackEndTimeAbsoluteMs() - GetPlayBackStartTimeAbsoluteMs())/1000.f;
	s_fFramesElapsed = s_fSecondsElapsed * 30.f; // Ideal
#endif // __BANK

	//If this is a multiplayer clip then load the multiplayer audio data
	if( IsPlaybackFlagSet(FRAME_PACKET_RECORDED_MULTIPLAYER) || IsPlaybackFlagSet( FRAME_PACKET_RECORDED_DIRECTOR_MODE) )
	{
		audNorthAudioEngine::NotifyOpenNetwork();
		sm_bMultiplayerClip = true;
	}
	//If this clip isnt multiplayer but we've previously run a multiplayer clip then close the multiplayer audio
	else if( sm_bMultiplayerClip )
	{
		audNorthAudioEngine::NotifyCloseNetwork(true);
		sm_bMultiplayerClip = false;
	}

	sm_bDisableLumAdjust = true;

	if(CPhysics::GetRopeManager())
	{
		ropeDataManager::LoadRopeTexures();
	}

#if USE_SRLS_IN_REPLAY
	//if we are in the first stage of recording
	//we should be creating an SRL for use in the 
	//second stage where the actual video baking is done.
	if(IsCreatingSRL())
	{		
		gStreamingRequestList.EnableForReplay();
		gStreamingRequestList.RecordForReplay(); //tell the SRLs that we are about to start recording
		gStreamingRequestList.BeginPrestream(NULL, true);
		gStreamingRequestList.SetTime(0);
	}
#endif
}

void CReplayMgrInternal::PauseTheClearWorldTimeout(bool bPause)
{
	if (sm_bScriptsHavePausedTheClearWorldTimeout && !bPause)
	{	//	If we're clearing the paused flag then reset the timer
		sm_scriptWaitTimer.Reset();
	}

	sm_bScriptsHavePausedTheClearWorldTimeout = bPause;
}

bool CReplayMgrInternal::JumpTo(float timeToJumpToMS, u32 jumpOptions)
{ 
	float const c_currentTimeRelativeMs = GetCurrentTimeRelativeMsFloat();
	float const c_jumpTimeDiff = timeToJumpToMS - c_currentTimeRelativeMs;

	if( fabsf(c_jumpTimeDiff) < REPLAY_JUMP_EPSILON && !(jumpOptions & JO_Force))
		return false;

	replayAssertf(timeToJumpToMS <= (float)GetTotalTimeMS(), "Tried to jump past the end of the replay. "
		"Jump time %f requested, and total time available is %u", timeToJumpToMS, GetTotalTimeMS() );

	if(jumpOptions & JO_LinearJump)
	{
		SetCursorState((int)(REPLAY_CURSOR_JUMP | REPLAY_CURSOR_JUMP_LINEAR));	
	}
	else
	{
		SetCursorState(REPLAY_CURSOR_JUMP);
	}

	// Make sure the direction is correct when jumping
	if(timeToJumpToMS <  c_currentTimeRelativeMs )
	{
		SetNextPlayBackState(REPLAY_DIRECTION_BACK);
	}
	else
	{
		SetNextPlayBackState(REPLAY_DIRECTION_FWD);
	}
	m_TimeToJumpToMS = timeToJumpToMS;
	m_ForceUpdateRopeSim = true;
	m_ForceUpdateRopeSimWhenJumping = !(jumpOptions & JO_FreezeRenderPhase);
	sm_bIsLastClipFinished = false;
	fwTimer::EndUserPause();

	sm_JumpPrepareState = ePreloadKickNeeded;

	if(!sm_isSettingUp)
	{
		sm_JumpPrepareState = ePreloadFirstKickNeeded;
	}

	m_FineScrubbing = jumpOptions & JO_FineScrubbing;
	sm_isSettingUp = true;
	sm_allEntitiesSetUp = false;	

	// Only if a large jump?
	sm_bPreloaderResetBack = true;
	sm_bPreloaderResetFwd = true;

	if(!(jumpOptions & JO_FreezeRenderPhase))
	{
		UnPauseRenderPhase();
	}
	else if(!(jumpOptions & JO_FineScrubbing) && !sm_isSettingUpFirstFrame)
	{
		PauseRenderPhase();
	}

	return true;
}

void CReplayMgrInternal::PauseRenderPhase()
{
	if(!IsSettingUpFirstFrame() && !sm_pPlaybackController->IsExportingToVideoFile())
	{
		CPauseMenu::TogglePauseRenderPhases(false, OWNER_REPLAY, __FUNCTION__ );
		m_WantsReactiveRenderer = false;
		m_FramesTillReactiveRenderer = NUM_FRAMES_TO_WAIT_AFTER_JUMP;
	}
}

void CReplayMgrInternal::UnPauseRenderPhase()
{
	CPauseMenu::TogglePauseRenderPhases(true, OWNER_REPLAY, __FUNCTION__ );
}

//========================================================================================================================
bool CReplayMgrInternal::ClearWorld()
{
	g_fireMan.m_disableAllSpreading = true;
	
	const float ScriptWaitTimeout = 5000.0f;
	if(sm_waitingForScriptCleanup == true)
	{
		if(sm_scriptsHaveCleanedUp == false)
		{
			if (sm_bScriptsHavePausedTheClearWorldTimeout)
			{
				replayDebugf2("CReplayMgrInternal::ClearWorld - scripts have paused the ClearWorld timeout... waited for %f ms", sm_scriptWaitTimer.GetMsTime());
				return false;
			}
			else
			{
				if(sm_scriptWaitTimer.GetMsTime() < ScriptWaitTimeout)
				{
					replayDebugf2("Still waiting for scripts to cleanup... waited for %f ms", sm_scriptWaitTimer.GetMsTime());
					return false;
				}
				else
				{
					replayDebugf2("Sod this... given up waiting for scripts to cleanup... waited for %f ms", sm_scriptWaitTimer.GetMsTime());
					// Fkit, give up waiting, fall through to the success and cleanup elsewhere
				}
			}
		}

		// Great scripts have done, crack on!
		replayDebugf1("Scripts have finished cleaning up... waited for %f ms", sm_scriptWaitTimer.GetMsTime());
		sm_waitingForScriptCleanup = false;
		sm_scriptsHaveCleanedUp = false;
		sm_shouldUpdateScripts = false;
		sm_IsBeingDeleteByReplay = false;

		g_ScriptAudioEntity.CleanUpScriptAudioEntityForReplay();
	}
	else
	{
		REPLAYTRACE("*CReplayMgrInternal::ClearWorld");
		sm_interfaceManager.PreClearWorld();

		sm_waitingForScriptCleanup = true;
		sm_scriptsHaveCleanedUp = false;
		sm_shouldUpdateScripts = true;
		sm_IsBeingDeleteByReplay = true;
		sm_bScriptsHavePausedTheClearWorldTimeout = false;

		sm_scriptWaitTimer.Reset();
		// Need to unpause to let the scripts process...
		fwTimer::EndUserPause();
		// return and wait for scripts
		replayDebugf2("Start waiting for scripts to cleanup...");
		return false;
	}

	sm_ClearingWorldForReplay = true;

	// Clear any cutscene
	if( CutSceneManager::GetInstance()->IsActive() )
	{
		CutSceneManager::GetInstance()->ShutDownCutscene();
		camInterface::FadeIn(500);
	}

	UnfreezeAudio();

	ResetParticles();

	// Disable Overwatch here...  It won't be re-enabled again until we reload the game
	sm_OverwatchEnabled = false;

	sm_interfaceManager.DeregisterAllCurrentEntities();
	sm_interfaceManager.ClearWorldOfEntities();

	//make sure the current player is cleared when we clear the world
	sm_pMainPlayer = NULL;

	fwMapDataStore::GetStore().SafeRemoveAll(true);

	CScenarioManager::GetSpawnHistory().ClearHistory();
	CScenarioManager::GetSpawnHistory().Update();
	CScenarioChainingGraph::UpdatePointChainUserInfo();

	CProjectileManager::RemoveAllProjectiles();

	// Wipe off everything to trigger a clean load
	/*TODO4FIVE CIplStore::ShutdownSession();*/

	const Vector3 VecCoors(0.0f, 0.0f, 0.0f);
	const float Radius = 1000000.0f;
	g_decalMan.RemoveInRange(RCC_VEC3V(VecCoors), ScalarV(Radius), NULL, NULL);

	CRenderer::SetDisableArtificialLights(false);

	CStreaming::PurgeRequestList();
	CStreaming::GetCleanup().DeleteAllDrawableReferences();

	CReplayExtensionManager::CheckExtensionsAreClean();

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		pRopeManager->Flush();
	}

	sm_ClearingWorldForReplay = false;

	return true;
}

void CReplayMgrInternal::ResetParticles()
{
	g_ptFxManager.RemovePtFxInRange(Vec3V(V_ZERO), 10000000.0f);

	for( u32 i = 0; i < MAX_FIRES; i++ )
		g_fireMan.ClearReplayFireFlag(i);
	g_fireMan.ExtinguishAll(false);

	//Disable any lightning when we rewind otherwise it can set 
	//the lightning effects in part of the scene where we dont want them.
	g_vfxLightningManager.SetLightningType(LIGHTNING_TYPE_NONE);

	RMPTFXMGR.Reset(NULL);
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::CaptureUpdatedThumbnail()
{
	if (!sm_pPlaybackController)
	{
		replayAssertf(false, "CReplayMgrInternal::CaptureUpdatedThumbnail() - Invalid playback controller");
		return;
	}

	char szName[32] = {0};
	const char* pszName = sm_pPlaybackController->GetCurrentRawClipFileName();
	const char* pszPeriod = strstr(pszName, ".");

	if (pszPeriod)
	{
		strncpy(szName, pszName, pszPeriod - pszName);
	}
	else
	{
		strcpy(szName, pszName);
	}

	char szScreenshotName[c_sScreenshotNameLength];
	safecpy(szScreenshotName, szName, c_sScreenshotNameLength);

	bool bSuccess = CPhotoManager::RequestSaveScreenshotToFile(szScreenshotName, SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT);

	if (bSuccess)
	{
		// TODO: Display message
		CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);
	}
}

bool CReplayMgrInternal::StartEnumerateClipFiles( const char* /*filepath*/, FileDataStorage& fileList, const char* /*filter*/ )
{
	return sm_FileManager.StartEnumerateClipFiles( fileList, REPLAY_CLIP_FILE_EXT );
}

bool CReplayMgrInternal::CheckEnumerateClipFiles( bool& result )
{
	return sm_FileManager.CheckEnumerateClipFiles( result );
}

bool CReplayMgrInternal::StartDeleteClipFile( const char* filepath )
{
	return sm_FileManager.StartDeleteClip( filepath );
}

bool CReplayMgrInternal::CheckDeleteClipFile( bool& result )
{
	return sm_FileManager.CheckDeleteClip( result );
}

bool CReplayMgrInternal::StartEnumerateProjectFiles( const char* /*filepath*/, FileDataStorage& fileList, const char* /*filter*/ )
{
	return sm_FileManager.StartEnumerateProjectFiles( fileList, REPLAY_MONTAGE_FILE_EXT );
}

bool CReplayMgrInternal::CheckEnumerateProjectFiles( bool& result )
{
	return sm_FileManager.CheckEnumerateProjectFiles( result );
}

bool CReplayMgrInternal::StartGetHeader( const char* filePath )
{
	return sm_FileManager.StartGetHeader( filePath );
}

bool CReplayMgrInternal::StartUpdateFavourites()
{
	return sm_FileManager.StartUpdateFavourites();
}

bool CReplayMgrInternal::CheckUpdateFavourites( bool& result )
{
	return sm_FileManager.CheckUpdateFavourites( result );
}

void CReplayMgrInternal::MarkAsFavourite(const char* path)
{
	sm_FileManager.MarkAsFavourite( path );
}

void CReplayMgrInternal::UnmarkAsFavourite(const char* path)
{
	sm_FileManager.UnMarkAsFavourite( path );
}

bool CReplayMgrInternal::IsMarkedAsFavourite(const char* path)
{
	return sm_FileManager.IsMarkedAsFavourite( path );
}

bool CReplayMgrInternal::StartMultiDelete(const char* filter)
{
	return sm_FileManager.StartMultiDelete(filter);
}

bool CReplayMgrInternal::CheckMultiDelete( bool& result )
{
	return sm_FileManager.CheckMultiDelete( result );
}

void CReplayMgrInternal::MarkAsDelete(const char* path)
{
	sm_FileManager.MarkAsDelete( path );
}

void CReplayMgrInternal::UnmarkAsDelete(const char* path)
{
	sm_FileManager.UnMarkAsDelete( path );
}

bool CReplayMgrInternal::IsMarkedAsDelete(const char* path)
{
	return sm_FileManager.IsMarkedAsDelete( path );
}

u32 CReplayMgrInternal::GetCountOfFilesToDelete(const char* filter)
{
	return sm_FileManager.GetCountOfFilesToDelete(filter);
}

u32	CReplayMgrInternal::GetClipRestrictions(const ClipUID& clipUID, bool* firstPersonCam, bool* cutsceneCam, bool* blockedCam)
{
	u32 const c_restrictions = sm_FileManager.GetClipRestrictions( clipUID );
	if (c_restrictions != 0)
	{
		if (firstPersonCam)
			*firstPersonCam = ((c_restrictions & FRAME_PACKET_RECORDED_FIRST_PERSON) !=0);
		if (cutsceneCam)
			*cutsceneCam = ((c_restrictions & FRAME_PACKET_RECORDED_CUTSCENE) !=0);
		if (blockedCam)
			*blockedCam = ((c_restrictions & FRAME_PACKET_DISABLE_CAMERA_MOVEMENT) !=0);
	}
	return c_restrictions;
}

bool CReplayMgrInternal::CheckGetHeader( bool& result, ReplayHeader& header )
{
	return sm_FileManager.CheckGetHeader( result, header );
}

bool CReplayMgrInternal::StartDeleteFile( const char* filepath )
{
	return sm_FileManager.StartDeleteFile( filepath );
}

bool CReplayMgrInternal::CheckDeleteFile( bool& result )
{
	return sm_FileManager.CheckDeleteFile( result );
}

bool CReplayMgrInternal::StartLoadMontage( const char* filepath, CMontage* montage )
{
	return sm_FileManager.StartLoadMontage( filepath, montage );
}

bool CReplayMgrInternal::CheckLoadMontage( bool& result)
{
	return sm_FileManager.CheckLoadMontage( result );
}

bool CReplayMgrInternal::StartSaveMontage( const char* filepath, CMontage* montage )
{
	return sm_FileManager.StartSaveMontage( filepath, montage );
}

bool CReplayMgrInternal::CheckSaveMontage( bool& result)
{
	return sm_FileManager.CheckSaveMontage( result );
}

u64 CReplayMgrInternal::GetOpExtendedResultData()
{
	return sm_FileManager.GetLastOpExtendedResultData();
}

//////////////////////////////////////////////////////////////////////////
float CReplayMgrInternal::LoadReplayLengthFromFileFloat( u64 const ownerId, const char* szFileName)
{
	return sm_FileManager.GetClipLength( ownerId, szFileName );
}

bool CReplayMgrInternal::IsClipUsedInAnyMontage( const ClipUID& uid )
{ 
	return sm_FileManager.IsClipInAProject(uid);
}

bool CReplayMgrInternal::GetFreeSpaceAvailableForVideos( size_t& out_sizeBytes )
{
	return sm_FileManager.getFreeSpaceAvailableForVideos( out_sizeBytes );
}

u32 CReplayMgrInternal::GetClipCount() 
{ 
	return sm_FileManager.GetClipCount(); 
}

u32 CReplayMgrInternal::GetProjectCount() 
{ 
	return sm_FileManager.GetProjectCount(); 
}

u32 CReplayMgrInternal::GetMaxClips() 
{ 
	return  sm_FileManager.GetMaxClips(); 
}

u32 CReplayMgrInternal::GetMaxProjects() 
{ 
	return  sm_FileManager.GetMaxProjects(); 
}

bool notDamagedFlagStored = false;
bool playerWasNotDamagedByAnything = false;
void CReplayMgrInternal::Close()
{
	if(notDamagedFlagStored)
	{
		CPed* pPlayer = FindPlayerPed();
		pPlayer->m_nPhysicalFlags.bNotDamagedByAnything = playerWasNotDamagedByAnything;
	}
	notDamagedFlagStored = false;
	playerWasNotDamagedByAnything = false;
}


//========================================================================================================================
void CReplayMgrInternal::BeginPlayback( IReplayPlaybackController& playbackController )
{
	REPLAYTRACE("*CReplayMgrInternal::BeginPlayback");
	
	CPed* pPlayer = FindPlayerPed();
	CWeapon* pWeapon = pPlayer->GetWeaponManager()->GetEquippedWeapon();
	if (pWeapon)
	{
		if (pWeapon->GetIsCooking())
			pWeapon->CancelCookTimer();
	}

	if(!notDamagedFlagStored)
	{
		playerWasNotDamagedByAnything = pPlayer->m_nPhysicalFlags.bNotDamagedByAnything;
		notDamagedFlagStored = true;
	}
	pPlayer->m_nPhysicalFlags.bNotDamagedByAnything = true;
	CExplosionManager::RemoveExplosionsInArea(EXP_TAG_DONTCARE, pPlayer->GetPlayerInfo()->GetPos(), 4000.0f);
	CProjectileManager::RemoveAllProjectiles();

	CReplayControl::SetRecordingDisallowed(DISALLOWED2);	
	sm_pPlaybackController = NULL;
	PauseReplayPlayback();

	SetCurrentTimeMs(0.0f);
	ResetExportTotalTime();
	ResetExportFrameCount();

	fwTimer::StartUserPause();

	sm_pPlaybackController = &playbackController;
	sm_pPlaybackController->ResetStartClipFlag();

	SetNextPlayBackState(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_NORMAL);
	LoadClip( playbackController.GetCurrentRawClipOwnerId(), playbackController.GetCurrentRawClipFileName() );

	OnBeginPlaybackSession();
}

void CReplayMgrInternal::CleanupPlayback()
{
	REPLAYTRACE("*CReplayMgrInternal::CleanupPlayback");

	CheckAllThreadsAreClear();
	sm_FileManager.StopFileThread();

	OnExitClip(); // For the last clip.

	CReplayRenderTargetHelper::GetInstance().Cleanup();

	StopPreCaching();
	sm_KickPrecaching = false;
	sm_WantsToKickPrecaching = false;

	g_RadioAudioEntity.StopReplayMusic(false, true);
	UnfreezeAudio();

    // Definitely don't nuke all the sounds if we haven't unloaded the game world since entering the editor, 
    // otherwise we're going to lose any cutscenes/dialogue/music etc. that might have been playing.
    if(g_bWorldHasBeenCleared)
    {
	    audController::GetController()->ClearAllSounds();	// removed from UnfreezeAudio(), but it's needed here
    }

	ResetParticles();

	RenderPhaseSeeThrough::SetState(false);

	sm_eventManager.Cleanup();

	sm_DoFullProcessPlayback = false;
	sm_pPlaybackController = NULL;
	PauseReplayPlayback();
	g_fireMan.m_disableAllSpreading = false;

	if(sm_ffwdAudioScene)
	{
		sm_ffwdAudioScene->Stop();
	}

	if(sm_rwdAudioScene)
	{
		sm_rwdAudioScene->Stop();
	}

	sm_desiredMode = REPLAYMODE_DISABLED;
	sm_LastErrorCode = REPLAY_ERROR_SUCCESS;
	sm_FileManager.StartFileThread();

	if(CPhysics::GetRopeManager())
	{
		ropeDataManager::UnloadRopeTexures();
	}

	g_distantLights.EnableDistantCars(true);
}

void CReplayMgrInternal::OnSignOut()
{
	if( !IsRecordingEnabled() && !IsReplayDisabled() )
	{
		CReplayControl::OnSignOut();

		sm_waitingForScriptCleanup = false;
		sm_scriptsHaveCleanedUp = false;
		sm_shouldUpdateScripts = false;
		sm_IsBeingDeleteByReplay = false;

		g_ScriptAudioEntity.CleanUpScriptAudioEntityForReplay();

		CleanupPlayback();

		sm_wantQuit = true;
		//we don't want to load our last save since the sign in process does that.
		g_bWorldHasBeenCleared = false;

		//Need to force preprocess to update since the game skeleton doesn't 
		//update when we're signed out.
		PreProcess();
	}
}

//========================================================================================================================
// Called on when a clip starts and ends (per clip).
bool clipEntered = false;
void CReplayMgrInternal::OnEnterClip()
{
	StoreGameTimers();

	ReplayMovieController::OnEntry();
	ReplayMovieControllerNew::OnEntry();
	ReplayIPLManager::OnEntry();
	ReplayMeshSetManager::OnEntry();
	ReplayRayFireManager::OnEntry();
	ReplayInteriorManager::OnEntry();
	ReplayHideManager::OnEntry();
	ReplayClothPacketManager::OnEntry();

	clipEntered = true;
}


void CReplayMgrInternal::OnExitClip()
{
	if(!clipEntered)
		return;

	if(CIslandHopper::GetInstance().GetCurrentActiveIsland() != 0)
	{
		CIslandHopper::GetInstance().RequestToggleIsland(CIslandHopper::GetInstance().GetCurrentActiveIsland(), false);
	}
	sm_islandActivated = ATSTRINGHASH("", 0);

	if(sm_vinewoodIMAPSsForceLoaded)
	{
		// If we force loaded the casino IMAPS then unload it here
		for(int i = 0; i < NELEM(vinewoodIMAPsToForceLoad); ++i)
		{
			strLocalIndex slotIndex = fwMapDataStore::GetStore().FindSlot(vinewoodIMAPsToForceLoad[i]);
			if (fwMapDataStore::GetStore().HasObjectLoaded(slotIndex))
			{
				fwMapDataStore::GetStore().ClearRequiredFlag(slotIndex.Get(), STRFLAG_DONTDELETE);
				fwMapDataStore::GetStore().SafeRemove(strLocalIndex(slotIndex));
				replayDebugf1("Unload %s", vinewoodIMAPsToForceLoad[i]);
			}
		}
		sm_vinewoodIMAPSsForceLoaded = false;
	}

	// Restore the default for the Focus Entity Manager as we're clearing the world.
	CFocusEntityMgr::GetMgr().SetDefault();

	ReplayMovieController::OnExit();
	ReplayMovieControllerNew::OnExit();
	ReplayIPLManager::SetOnExit();
	ReplayMeshSetManager::OnExit();
	ReplayRayFireManager::OnExit();
	ReplayInteriorManager::OnExit();
	ReplayHideManager::OnExit();
	ReplayClothPacketManager::OnExit();

	CReplayTrailController::GetInstance().OnExitClip();

	CPacketSetBuildingTint::ClearDeferredPackets();

	RestoreGameTimers();

	clipEntered = false;
}


// Called on BeginPlayback()/CleanupPlayback() (per play back sesssion).
void CReplayMgrInternal::OnBeginPlaybackSession()
{
	CPacketMapChange::OnBeginPlaybackSession();
	//CPhysics::GetClothManager()->SetDontDoClothOnSeparateThread(true);
	CLOUDHATMGR->UnloadAllCloudHats();
}


void CReplayMgrInternal::OnCleanupPlaybackSession()
{
	//CPhysics::GetClothManager()->SetDontDoClothOnSeparateThread(false);
	CPacketMapChange::OnCleanupPlaybackSession();

	CLOUDHATMGR->UnloadAllCloudHats();
}

//========================================================================================================================
void CReplayMgrInternal::SetupInitialPlayback()
{
	REPLAYTRACE("*CReplayMgrInternal::SetupInitialPlayback");
	sm_CachedSoundPackets.Reset();

	//clear any requests that have been made.
	sm_StreamingRequests.Reset();

	SetCurrentTimeMs(0.0f);
	sm_uPlaybackEndTime = 0;
	
	CAddressInReplayBuffer startAddress = sm_oPlayback;
	CAddressInReplayBuffer endAddress;

	CAddressInReplayBuffer replayAddress = sm_oPlayback;

	ReplayController controller(replayAddress, sm_ReplayBufferInfo);
	controller.EnablePlayback();
	//work out the end address for the currently loaded replay
	while(controller.IsLastBlock() == false)
	{	
		controller.AdvanceBlock();
	}
	controller.DisablePlayback();

	endAddress = FindEndOfBlock(controller.GetAddress());

	SetupPlayback(startAddress, endAddress, LINK_BLOCKS_IN_SEQUENCE, startAddress.GetSessionBlockIndex());

#if DO_REPLAY_OUTPUT_XML
	ExportReplayPacketCallback();
#endif // REPLAY_DEBUG

	//setup the first frame in the list as the current frame
	const CPacketFrame* pFramePacket	= sm_oPlayback.GetBufferPosition<CPacketFrame>();
	sm_currentFrameInfo.SetFramePacket(pFramePacket);

	CBlockInfo* pBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(sm_currentFrameInfo.GetFrameRef().GetBlockIndex());
	REPLAY_ASSERTF(pBlock == sm_oPlayback.GetBlock(), "Incorrect Block");
	sm_currentFrameInfo.SetAddress(CAddressInReplayBuffer(pBlock, 0));
	sm_currentFrameInfo.SetHistoryAddress(CAddressInReplayBuffer(pBlock, pFramePacket->GetOffsetToEvents()));

	sm_firstFrameInfo.SetFramePacket(pFramePacket);
	sm_firstFrameInfo.SetAddress(CAddressInReplayBuffer(pBlock, 0));
	sm_firstFrameInfo.SetHistoryAddress(CAddressInReplayBuffer(pBlock, pFramePacket->GetOffsetToEvents()));

	sm_prevFrameInfo.SetFramePacket(NULL);

	const CPacketFrame* pNextFramePacket = GetNextFrame(pFramePacket);
	sm_nextFrameInfo.SetFramePacket(pNextFramePacket);
	pBlock					= sm_ReplayBufferInfo.FindBlockFromSessionIndex(sm_nextFrameInfo.GetFrameRef().GetBlockIndex());
	sm_nextFrameInfo.SetAddress(CAddressInReplayBuffer(pBlock, (u32)((u8*)pNextFramePacket - (u8*)pFramePacket)));
	sm_nextFrameInfo.SetHistoryAddress(CAddressInReplayBuffer(pBlock, sm_nextFrameInfo.GetAddress().GetPosition() + pNextFramePacket->GetOffsetToEvents()));

	// Collect the entity instance priority from the 1st frame and set it before streaming anything in (this value is picked up in CInstancePriority::Update()).
	sm_replayFrameData.m_instancePriority = pFramePacket->GetReplayInstancePriority();
}


//========================================================================================================================
CAddressInReplayBuffer CReplayMgrInternal::FindEndOfBlock(CAddressInReplayBuffer startAddressOfBlock)
{
	//find the last valid address in this block
	ReplayController controller(startAddressOfBlock, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	while(controller.IsEndOfBlock() == false)
	{
		CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();
		if (!replayVerifyf(pPacket, "pPacket is NULL!"))
		{
			break;
		}
		controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
	}

	controller.DisablePlayback();

	return controller.GetAddress();
}

//========================================================================================================================
CAddressInReplayBuffer CReplayMgrInternal::FindOverlapWithPreviousBlock(CAddressInReplayBuffer startAddressOfBlock)
{
	//this function finds the end of the first frame in a block so it can be used as the end address in
	//LinkPacketsForPlayback(), which allows a block which precedes this one to be link into the replay correctly

	ReplayController controller(startAddressOfBlock, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	while(controller.GetCurrentPacketID() != PACKETID_GAMETIME || controller.GetCurrentPosition() == 0)
	{
		controller.AdvanceUnknownPacket(controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize());
	}

	controller.DisablePlayback();

	return controller.GetAddress();
}

//========================================================================================================================
CAddressInReplayBuffer CReplayMgrInternal::FindOverlapWithNextBlock(CAddressInReplayBuffer startAddressOfBlock)
{
	//this function finds the start of the last frame in a block so it can be used as the start address in
	//LinkPacketsForPlayback(), which allows a block which follows this one to be link into the replay correctly

	ReplayController controller(startAddressOfBlock, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	u32 lastFrameIndex = 0;

	while(controller.IsEndOfBlock() == false)
	{
		u32 packetID = controller.GetCurrentPacketID();
		if(packetID == PACKETID_GAMETIME)
		{
			lastFrameIndex = controller.GetCurrentPosition();
		}

		controller.AdvanceUnknownPacket(controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize());
	}

	CAddressInReplayBuffer result = controller.GetAddress();
	result.SetPosition(lastFrameIndex);

	controller.DisablePlayback();

	return result;
}

//========================================================================================================================
bool CReplayMgrInternal::SetupPlayback(CAddressInReplayBuffer& startAddress, CAddressInReplayBuffer& endAddress, const eBlockLinkType linkType, const u16 startingMasterBlockIndex)
{
	//note the starting master block index has to correspond to the buffer being passed as the start address
	u16 currentMasterBlockIndex = startingMasterBlockIndex;

	sm_eventManager.ResetSetupInformation();
	sm_eventManager.ResetStaticBuildingMap();
	ReplayLightingManager::Reset();
	ReplayAttachmentManager::Get().Reset();

	sm_replayClipData.Reset();

	u32 uLastGameTime = 0;

	replayDebugf2("Playing back from block %u (position %u) to block %u (position %u)", startAddress.GetMemoryBlockIndex(), startAddress.GetPosition(), endAddress.GetMemoryBlockIndex(), endAddress.GetPosition());

	CAddressInReplayBuffer startOfAddressSpace = startAddress;
	ReplayController controller(startOfAddressSpace, sm_ReplayBufferInfo, FrameRef(startingMasterBlockIndex, 0));
	controller.GetPlaybackFlags().SetState( REPLAY_DIRECTION_FWD );
	controller.EnablePlayback();

	bool hasReachedEnd = false;

	u32 timePackets = 0;

	while(hasReachedEnd == false && 
			(controller.GetCurrentBlockIndex() != endAddress.GetMemoryBlockIndex() || controller.GetCurrentPosition() < endAddress.GetPosition()))
	{	
		if(controller.IsEndOfBlock())
		{
			hasReachedEnd = controller.IsLastBlock();
			currentMasterBlockIndex++;

			controller.AdvanceBlock();

			continue;
		}

		CPacketBase const* pCurrentPacket = controller.ReadCurrentPacket<CPacketBase>();
	
		switch(controller.GetCurrentPacketID())
		{
			case PACKETID_GAMETIME:
			{
				CPacketGameTime const* pGameTimePacket = controller.ReadCurrentPacket<CPacketGameTime>();
				uLastGameTime = pGameTimePacket ? pGameTimePacket->GetGameTime() : 0;
			
				if(pGameTimePacket && pGameTimePacket->GetGameTime() < sm_uPlaybackStartTime && sm_bShouldSetStartTime)
				{
					sm_uPlaybackStartTime = pGameTimePacket->GetGameTime();
				}
			}
			break;
			case PACKETID_NEXTFRAME:
			{
				if(linkType == LINK_BLOCKS_IN_SEQUENCE ||
					(linkType == LINK_BLOCKS_AFTER_ANOTHER && controller.GetCurrentBlockIndex() != startAddress.GetMemoryBlockIndex()) ||
					(linkType == LINK_BLOCKS_BEFORE_ANOTHER && controller.GetCurrentBlockIndex() != endAddress.GetMemoryBlockIndex()) )
				{
					// Store it at all frames, last store will be the last time
					if(uLastGameTime > sm_uPlaybackEndTime)
					{
						sm_uPlaybackEndTime = uLastGameTime;
						timePackets++;
					}

					controller.ReadCurrentPacket<CPacketNextFrame>();
					controller.GetCurrentBlock()->SetFrameCount(controller.GetCurrentFrameRef().GetFrame());
				}
			}
			break;
			case PACKETID_TRAFFICLIGHTOVERRIDE:
			{
				CPacketTrafficLight const* pPacket = controller.ReadCurrentPacket<CPacketTrafficLight>();
				CReplayInterface<CObject>* pObjectInterface = sm_interfaceManager.FindCorrectInterface<CObject>();

				// With the traffic lights we need to verify the initial settings with the first traffic 
				// light event packet.  The first event packet will contain the initial traffic light 
				// settings so make sure these are set.  If on the off chance there are no traffic light 
				// event packets then the light settings are verified in CReplayInterfaceObj::ResetEntity().
				CPacketObjectCreate* pCreatePacket = pPacket && pObjectInterface ? 
					pObjectInterface->GetCreatePacket(pPacket->GetReplayID()) : NULL;
				if(pCreatePacket && pCreatePacket->GetInitialTrafficLightsVerified() == false)
				{
					pCreatePacket->SetInitialTrafficLightCommands(pPacket->GetPrevTrafficLightCommands());
					pCreatePacket->SetInitialTrafficLightsVerified();
				} 
			}
			break;
			case PACKETID_PEDVARIATIONCHANGE:
			{
				CPacketPedVariationChange const* pPacket = controller.ReadCurrentPacket<CPacketPedVariationChange>();
				if(pPacket->IsCancelled() == false)
				{
					CReplayInterface<CPed>* pPedInterface = sm_interfaceManager.FindCorrectInterface<CPed>();

					CPacketPedCreate* pCreatePacket = pPedInterface->GetCreatePacket(pPacket->GetReplayID());
					replayFatalAssertf(pCreatePacket, "Ped Create packet missing");
					if(pCreatePacket && pCreatePacket->IsVarationDataVerified() == false)
					{
						pCreatePacket->SetVarationDataVerified();
					}
				}
			}
			break;
			case PACKETID_VEHVARIATIONCHANGE:
			{
				CPacketVehVariationChange const* pPacket = controller.ReadCurrentPacket<CPacketVehVariationChange>();
				if(pPacket->IsCancelled() == false)
				{
					CReplayInterface<CVehicle>* pVehInterface = sm_interfaceManager.FindCorrectInterface<CVehicle>();

					CPacketVehicleCreate* pCreatePacket = pVehInterface->GetCreatePacket(pPacket->GetReplayID());
					replayFatalAssertf(pCreatePacket, "Vehicle Create packet missing");
					if( pCreatePacket && pCreatePacket->IsVarationDataVerified() == false)
					{
						pCreatePacket->SetVarationDataVerified();
					}
				}
			}
			break;
			case PACKETID_STARTSCRIPTFX:
				{
					CPacketStartScriptPtFx const* pPacket = controller.ReadCurrentPacket<CPacketStartScriptPtFx>();
					
					PTFX_ReplayStreamingRequest* request = CReplayMgr::GetStreamingRequests().Access(pPacket->GetSlotId());
					if( !request )
					{
						request = &GetStreamingRequests().Insert(pPacket->GetSlotId()).data;
						request->SetIndex(pPacket->GetSlotId());
						request->SetStreamingModule(ptfxManager::GetAssetStore());
						request->Load();
					}

					if(!sm_interfaceManager.PlaybackSetup(controller))
						sm_eventManager.PlaybackSetup(controller);
				}
				break;
			case PACKETID_TRIGGEREDSCRIPTFX:
				{
					CPacketTriggeredScriptPtFx const* pPacket = controller.ReadCurrentPacket<CPacketTriggeredScriptPtFx>();

					PTFX_ReplayStreamingRequest* request = CReplayMgr::GetStreamingRequests().Access(pPacket->GetSlotId());
					if( !request )
					{
						request = &GetStreamingRequests().Insert(pPacket->GetSlotId()).data;
						request->SetIndex(pPacket->GetSlotId());
						request->SetStreamingModule(ptfxManager::GetAssetStore());
						request->Load();
					}

					if(!sm_interfaceManager.PlaybackSetup(controller))
						sm_eventManager.PlaybackSetup(controller);
				}
				break;
			case PACKETID_REQUESTPTFXASSET:
				{
					CPacketRequestPtfxAsset const* pPacket = controller.ReadCurrentPacket<CPacketRequestPtfxAsset>();

					PTFX_ReplayStreamingRequest* request = CReplayMgr::GetStreamingRequests().Access(pPacket->GetSlotId());
					if( !request )
					{
						request = &GetStreamingRequests().Insert(pPacket->GetSlotId()).data;
						request->SetIndex(pPacket->GetSlotId());
						request->SetStreamingModule(ptfxManager::GetAssetStore());
						request->Load();
					}

					if(!sm_interfaceManager.PlaybackSetup(controller))
						sm_eventManager.PlaybackSetup(controller);
				}
				break;
			case PACKETID_FRAME:
			{
				const CPacketFrame* pPacket = controller.ReadCurrentPacket<CPacketFrame>();
				sm_lastFrameInfo.SetFramePacket(pPacket);

				sm_replayClipData.m_clipFlags |= pPacket->GetFrameFlags();

				CBlockInfo* pBlock = sm_ReplayBufferInfo.FindBlockFromSessionIndex(sm_lastFrameInfo.GetFrameRef().GetBlockIndex());

				CPacketBase* p = (CPacketBase*)pBlock->GetData();
				p->ValidatePacket();
				sm_lastFrameInfo.SetAddress(CAddressInReplayBuffer(pBlock, 0));
			}
			break;
			case PACKETID_INTERIOR:
				{
					const CPacketInterior* pPacket = controller.ReadCurrentPacket<CPacketInterior>();
					pPacket->Setup();
				}
				break;
			default:
				{
					if(!sm_interfaceManager.PlaybackSetup(controller))
						sm_eventManager.PlaybackSetup(controller);
				}
			break;
		}
	
		controller.AdvanceUnknownPacket(pCurrentPacket->GetPacketSize());			
	}

	// Go through the building pool and see if any are in existance that will
	// be required by events in the clip.
	replayDebugf1("Number of static buildings needed %d", sm_eventManager.GetStaticBuildingRequiredCount());
	ResetStaticBuildings();
	for(int i = 0, count = 0, found = 0; i < CBuilding::GetPool()->GetSize() && count < CBuilding::GetPool()->GetNoOfUsedSpaces() && found < sm_eventManager.GetStaticBuildingRequiredCount(); ++i)
	{
		CBuilding* pBuilding = CBuilding::GetPool()->GetSlot(i);
		if(pBuilding)
		{
			if(AddBuilding(pBuilding))
			{
				++found;
			}
			++count;
		}
	}

	// Close off all dangling events
	sm_eventManager.ResetSetupInformation();
	sm_interfaceManager.RemoveRecordingInformation();

	replayDebugf2("Setting up playback");
	replayDebugf2("%u time packets", timePackets);
	replayDebugf2("Start:  %7u", sm_uPlaybackStartTime);
	replayDebugf2("End:    %7u", sm_uPlaybackEndTime);
	replayDebugf2("Length: %7u", sm_uPlaybackEndTime - sm_uPlaybackStartTime);

	controller.DisablePlayback();

	return hasReachedEnd;
}

//========================================================================================================================
s32 CReplayMgrInternal::GetNumberOfFramesInBuffer()
{
	s32	sFramesCounted = 0;
	CAddressInReplayBuffer address;
	FindStartOfPlayback(address);
	ReplayController controller(address, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	while(controller.IsEndOfPlayback() == false)
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		if(packetID == PACKETID_NEXTFRAME)
		{
			++sFramesCounted;
		}

		CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();
		controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
	}

	controller.DisablePlayback();

	return sFramesCounted;
}


u32 CReplayMgrInternal::GetTotalMsInBuffer()
{
	u32 uStartTime = 0xffffffff;
	u32 uEndTime = 0;

	CAddressInReplayBuffer address;
	FindStartOfPlayback(address);
	ReplayController controller(address, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	while(controller.IsEndOfPlayback() == false)
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		if(packetID == PACKETID_GAMETIME)
		{
			CPacketGameTime const* pPacketGameTime = controller.ReadCurrentPacket<CPacketGameTime>();
			if(pPacketGameTime->GetGameTime() > uEndTime)
			{
				uEndTime = pPacketGameTime->GetGameTime();
			}
			if(pPacketGameTime->GetGameTime() < uStartTime)
			{
				uStartTime = pPacketGameTime->GetGameTime();
			}
		}

		CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();
		if (!replayVerifyf(pPacket, "pPacket is NULL!"))
		{
			break;
		}
		controller.AdvanceUnknownPacket(pPacket->GetPacketSize());
	}

	controller.DisablePlayback();

	return uEndTime - uStartTime;
}

//========================================================================================================================
void CReplayMgrInternal::GetFirstCameraFocusCoords(Vector3 *pResult)
{
	*pResult = Vector3(0.0f, 0.0f, 0.0f);
	CBlockProxy block = sm_ReplayBufferInfo.FindFirstBlock();
	for (u16 blockIndex = 0; blockIndex < sm_ReplayBufferInfo.GetNumNormalBlocks(); blockIndex++)
	{
		REPLAY_CHECK(block.IsValid(), NO_RETURN_VALUE, "Failed to get correct block in GetFirstCameraFocusCoords");
		if (block.GetStatus() != REPLAYBLOCKSTATUS_EMPTY)
		{
			u8* pTempBuffer = &(block.GetData()[0]);
			CPacketBase* pPacket = (CPacketBase*)pTempBuffer;
			eReplayPacketId packetID = pPacket->GetPacketID();
			while (packetID != PACKETID_END && packetID != PACKETID_END_HISTORY)
			{
				switch(packetID)
				{
				case PACKETID_PEDUPDATE:
					{
						CPacketPedUpdate* pPedPacket = (CPacketPedUpdate*)pPacket;
						if (pPedPacket->IsMainPlayer())
						{
							pPedPacket->GetPosition(*pResult);
							return;
						}
					}
					break;
				default:
					break;
				}
				pTempBuffer += pPacket->GetPacketSize();

				pPacket = (CPacketBase*)pTempBuffer;
				pPacket->ValidatePacket();
				packetID = pPacket->GetPacketID();
			}
		}
		block = block.GetNextBlock();
	}
}


//========================================================================================================================
FrameRef CReplayMgrInternal::GetCurrentSessionFrameRef()
{
	if(IsEditModeActive())
	{
		return sm_currentFrameInfo.GetFrameRef();
	}
	else
	{
		replayAssertf(sm_uCurrentFrameInBlock >= 0, "The current frame in this block is invalid!");
		return FrameRef(sm_oRecord.GetSessionBlockIndex(), (u16)sm_uCurrentFrameInBlock);
 	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::SaveCallback()
{
	if( !IsSaving() )
	{
		SaveAllBlocks();
		replayDebugf3("Saving...");
	}
}


#if __BANK

f32 CReplayMgrInternal::sm_fMonatgeMusicVolume = 0.0f;
f32	CReplayMgrInternal::sm_fMonatgeSFXVolume = -6.0f;


void CReplayMgrInternal::LoadCallback()
{
	if(IsPerformingFileOp() == false && CGameLogic::IsGameStateInPlay() && camInterface::IsFadedOut() == false)
	{
		LoadClip( sm_FileManager.getCurrentUserIDToPrint(), s_Filename );
	}
}

void CReplayMgrInternal::DeleteCallback()
{
	/*atString basePath = atString(REPLAY_CLIPS_PATH);
	basePath += s_Filename;

	atString fullPath;
	
	fullPath = basePath;
	fullPath += ".clip";
	sm_FileManager.StartDelete(fullPath.c_str());

	fullPath = basePath;
	fullPath += ".jpg";
	sm_FileManager.StartDelete(fullPath.c_str());

	fullPath = basePath;
	fullPath += ".xml";
	sm_FileManager.StartDelete(fullPath.c_str());

	atArray<const char*> clipNames;
	GetClipNames(clipNames);
	sm_ReplayClipNameCombo->UpdateCombo("Clip Selector", &sm_ClipIndex, clipNames.GetCount(), clipNames.GetElements(), ClipSelectorCallBack);
	*/
}

void CReplayMgrInternal::SetAllSaved()
{
	sm_ReplayBufferInfo.SetAllSaved();
}

void CReplayMgrInternal::RecordCallback()
{
	if (sm_bRecordingEnabledInRag)
	{
		sm_desiredMode = REPLAYMODE_RECORD;
	}
	else
	{
		sm_desiredMode = REPLAYMODE_DISABLED;
	}
}

void CReplayMgrInternal::QuitReplayCallback()
{
	sm_wantQuit = true;
}

void CReplayMgrInternal::DumpMonitoredEventsToTTY()
{
	sm_eventManager.DumpMonitoredEventsToTTY();
}

void CReplayMgrInternal::PlayCallback()
{
	if(IsEditModeActive() == false)
	{
		TriggerPlayback();
	}
	else if(GetLastPlayBackState().IsSet(REPLAY_STATE_PAUSE))
	{
		SetNextPlayBackState(REPLAY_STATE_PLAY);
	}
	else if( GetCursorState() != REPLAY_CURSOR_NORMAL)
	{
		 SetCursorState(REPLAY_CURSOR_NORMAL);
	}
}

void CReplayMgrInternal::PauseCallback()
{
	if(IsEditModeActive())
	{
		SetNextPlayBackState(REPLAY_STATE_PAUSE);
	}
}

void CReplayMgrInternal::ScrubbingCallback()
{
	if(IsEditModeActive())
	{
		u32 newTime = u32(GetTotalMsInBuffer() * (s_currentProgressPercentage / 100.0f));
		JumpTo((float)newTime, JO_FreezeRenderPhase);
	}
}

void CReplayMgrInternal::CursorSpeedCallback()
{
	if(IsEditModeActive())
	{
		SetCursorSpeed(s_customDeltaSettingMS);
	}
}


bool CReplayMgrInternal::SaveReplayPacketCompleteFunc(int)
{
	sysIpcSignalSema(s_ExportReplayWait);
	return true;
}

void CReplayMgrInternal::WaitForReplayExportToFinish()
{
	if(s_ExportReplayRunning)
	{
		sysIpcWaitSema(s_ExportReplayWait);
		s_ExportReplayRunning = false;
	}
}

void CReplayMgrInternal::SaveReplayPacketsFunc(FileHandle handle)
{
	const char* xmlStart = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

	CFileMgr::Write(handle, xmlStart, istrlen(xmlStart));

	CAddressInReplayBuffer address;
	CReplayMgrInternal::FindStartOfPlayback(address);
	ReplayController controller(address, sm_ReplayBufferInfo);
	controller.EnablePlayback();

	eReplayClipTraversalStatus status = UNHANDLED_PACKET;
	while(status != END_CLIP)
	{
		u32 position = controller.GetCurrentPosition();
		u16 blockIndex = controller.GetCurrentBlockIndex();

		status = sm_interfaceManager.PrintOutReplayPackets(controller, REPLAY_OUTPUT_XML, handle);
		if(status == UNHANDLED_PACKET)
			status = sm_eventManager.PrintOutHistoryPackets(controller, REPLAY_OUTPUT_XML, handle);

		if(status == UNHANDLED_PACKET && position == controller.GetCurrentPosition() && blockIndex == controller.GetCurrentBlockIndex())
		{
			replayAssertf(false, "Unrecognised packet while printing history packets... 0x%X", controller.GetCurrentPacketID());
			status = END_CLIP; // Break out of the loop
		}
	};

	controller.DisablePlayback();
}

#if DO_REPLAY_OUTPUT_XML
void CReplayMgrInternal::ExportReplayPacketCallback()
{
	if(IsEnabled() && IsPerformingFileOp() == false)
	{		
		// REPLAY_CLIPS_PATH change
		atString path;
		path += s_ExportReplayFilename;
		path += ".xml";

		s_ExportReplayRunning = true;
		if(!sm_FileManager.StartSaveFile(path, &SaveReplayPacketsFunc, NULL, &SaveReplayPacketCompleteFunc))
		{
			// Even though we have failed we still want to clean up and signal the relevant semaphore.
			SaveReplayPacketCompleteFunc(0);
		}
	}
}
#endif // REPLAY_DEBUG && RSG_PC

void CReplayMgrInternal::IncSuppressRefCountCallBack()
{
	IncSuppressRefCount();
}

void CReplayMgrInternal::DecSuppressRefCountCallBack()
{
	DecSuppressRefCount();
}

void CReplayMgrInternal::ClipSelectorCallBack()
{
	const char* name = sm_ReplayClipNameCombo->GetString(sm_ClipIndex);

	if( name != NULL )
	{
		atString clipName = atString(name);
	 
		if( clipName.GetLength() < REPLAYPATHLENGTH )
		{
			strcpy(s_Filename, clipName.c_str());

			sm_ReplayClipNameText->SetString(s_Filename);
		}
	}
}

void CReplayMgrInternal::GetClipNames(atArray<const char*>& clipNames)
{
	FileDataStorage files;
	/*sm_FileManager.EnumerateFiles(REPLAY_CLIPS_PATH, files, ".clip");*/

	for(int i = 0; i < files.GetCount(); ++i)
	{
		CFileData& fileData = files[i];

		atString name = atString(fileData.getFilename());

		/*if( IsReplayClipFileValid(name) )*/
		{
			name.Replace(".clip", "");
			clipNames.PushAndGrow(StringDuplicate(name.c_str()));
		}
	}
	
	if( clipNames.GetCount() > 0 )
	{
		if( strlen(clipNames[0]) < REPLAYPATHLENGTH )
		{
			strcpy(s_Filename, clipNames[0]);

			sm_ReplayClipNameText->SetString(s_Filename);
		}
	}	
}

void CReplayMgrInternal::Core0Callback()
{
	sysIpcSetThreadProcessor(sm_recordingThread[0], 0);
}

void CReplayMgrInternal::Core5Callback()
{
	sysIpcSetThreadProcessor(sm_recordingThread[0], 5);
}

void CReplayMgrInternal::HighlightReplayEntity()
{
	int i = strtol(s_ReplayIDToHighlight, NULL, 0);
	if(i != 0)
	{
		CEntity* pEntity = GetEntityAsEntity(i);

		if(pEntity)
		{
			g_PickerManager.AddEntityToPickerResults(pEntity, true, true);
			return;
		}
	}

	replayDebugf1("Invalid Replay ID: %s", s_ReplayIDToHighlight);
}


void CReplayMgrInternal::HighlightMapObjectViaMapHash()
{
	int i = strtol(s_ReplayIDToHighlight, NULL, 0);
	if(i != 0)
	{
		for(int j = 0; j < CObject::GetPool()->GetSize(); ++j)
		{
			CObject* pMapObject = CObject::GetPool()->GetSlot(j);
			if(pMapObject && pMapObject->GetRelatedDummy() && pMapObject->GetRelatedDummy()->GetHash() == (u32)i)
			{
				g_PickerManager.AddEntityToPickerResults(pMapObject, true, true);
				return;
			}
		}
	}

	replayDebugf1("Invalid Map Hash: %s", s_ReplayIDToHighlight);
}


void CReplayMgrInternal::HighlightMapObject()
{
	CEntity* pReplayEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
	if(!pReplayEntity || !pReplayEntity->GetIsTypeObject())
		return;

	if(pReplayEntity->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
	{
		CObject* pReplayObject = (CObject*)pReplayEntity;
		ReplayObjectExtension* pExtension = ReplayObjectExtension::GetExtension(pReplayObject);
		if(!pExtension || pExtension->GetMapHash() == REPLAY_INVALID_OBJECT_HASH)
			return;

		for(int i = 0; i < CObject::GetPool()->GetSize(); ++i)
		{
			CObject* pMapObject = CObject::GetPool()->GetSlot(i);
			if(pMapObject && pMapObject->GetRelatedDummy() && pMapObject->GetRelatedDummy()->GetHash() == pExtension->GetMapHash())
			{
				g_PickerManager.AddEntityToPickerResults(pMapObject, true, true);
				break;
			}
		}
	}
}


void CReplayMgrInternal::HighlightParent()
{
	CEntity* pReplayEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
	if(!pReplayEntity || (!pReplayEntity->GetIsTypeObject() && !pReplayEntity->GetIsTypeVehicle() && !pReplayEntity->GetIsTypePed()))
		return;

	if(pReplayEntity->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
	{
		fwAttachmentEntityExtension *extension = ((CPhysical*)pReplayEntity)->GetAttachmentExtension();
		if(extension)
		{
			fwEntity* pParent = (fwEntity*)extension->GetAttachParent();
			if(pParent)
			{
				g_PickerManager.AddEntityToPickerResults(pParent, true, true);
			}
		}
	}
}


void CReplayMgrInternal::AddDebugWidgets()
{
	if (BANKMGR.FindBank("Replay") != NULL)
	{
		// Already exists
		return;
	}

	bkBank& bank = BANKMGR.CreateBank("Replay");

	bank.AddToggle("Record", &sm_bRecordingEnabledInRag, RecordCallback);
	sprintf(s_Filename, "default");
	sm_ReplayClipNameText = bank.AddText("Filename", s_Filename, REPLAYPATHLENGTH);

	atArray<const char*> clipNames;
	GetClipNames(clipNames);
	sm_ReplayClipNameCombo = bank.AddCombo( "Clip Selector", &sm_ClipIndex, clipNames.GetCount(), clipNames.GetElements(), ClipSelectorCallBack);

	bank.AddButton("Save", SaveCallback);
	bank.AddButton("Load", LoadCallback);
	bank.AddButton("Delete", DeleteCallback);

	bank.AddButton("SetAllSaved (Debug Only!)", SetAllSaved);

	bank.AddToggle("Use Custom Working Dir", &sm_bUseCustomWorkingDir);
	bank.AddText("Custom Replay Path", sm_customReplayPath, RAGE_MAX_PATH);
	bank.AddButton("Dump Replay Files", ReplayFileManager::DumpReplayFiles_Bank);

	bank.AddToggle("Enable Framerate Lock", &sm_lockRecordingFrameRate);
	bank.AddSlider("Num recorded frames per second", &sm_numRecordedFramesPerSec, 5, 100, 1);

	bank.PushGroup("Playback Controls", true);
	
	bank.AddButton("Play", PlayCallback);
	bank.AddButton("Pause", PauseCallback);
	bank.AddButton("(Debug) Quit replay", QuitReplayCallback);
	bank.AddToggle("Record Events", &sm_wantToRecordEvents);
	bank.AddToggle("Enable Event Monitoring", &sm_wantToMonitorEvents);
	bank.AddButton("Dump Monitored Events to Log", DumpMonitoredEventsToTTY);
	bank.AddToggle("Threaded Recording", &sm_wantRecordingThreaded);
	bank.AddSlider("Progress", &s_currentProgressPercentage, 0.0f, 100.0f,1.0f, ScrubbingCallback);

	bank.AddSlider("Custom Delta", &s_customDeltaSettingMS, -4.0f, 4.0f, 0.2f);
	bank.AddButton("Set Delta", CursorSpeedCallback);

	bank.AddSlider("Playback Speed", &sm_playbackSpeed, 0.0f, 1.0f, 0.001f);

	bank.PopGroup();

	sm_FileManager.AddDebugWidgets(bank);

	bank.PushGroup("Debug", false);

	bank.AddButton("Print History Packets", PrintHistoryPackets);
	bank.AddButton("Print Replay Packets", PrintReplayPackets);

	bank.AddButton("Increment Suppress Refcount", IncSuppressRefCountCallBack);
	bank.AddButton("Decrement Suppress Refcount", DecSuppressRefCountCallBack);

#if DO_REPLAY_OUTPUT_XML
	sprintf(s_ExportReplayFilename, "defaultReplay");
	bank.AddText("Export Replay Filename", s_ExportReplayFilename, REPLAYFILELENGTH);
	bank.AddButton("Export Replay Packets", ExportReplayPacketCallback);
#endif // REPLAY_DEBUG && RSG_PC

	sm_replayBlockPosSlider		= bank.AddSlider("Replay Block Position",	&s_currentReplayBlockPos,	0, BytesPerReplayBlock,		1);
	sm_replayBlockSlider		= bank.AddSlider("Replay Block",			&s_currentReplayBlock,		0, MAX_NUM_REPLAY_BLOCKS,	1);

	bank.AddSlider("Frames elapsed", &s_fFramesElapsed, 0.0f, 12000.0f, 1.0f);
	bank.AddSlider("Seconds elapsed", &s_fSecondsElapsed, 0.0f, 500.0f, 1.0f);

	bank.AddSlider("Pfx List Count", &s_sNumberOfParticleEffectsAvailable, 0, 10, 1);

	bank.AddSlider("Trackables Count", &s_trackablesCount, 0, 512, 1);

	bank.AddSlider("Music Volume", &sm_fMonatgeMusicVolume, -100.0f, 0.0f, 1.0f);
	bank.AddSlider("SFX Volume", &sm_fMonatgeSFXVolume, -100.0f, 0.0f, 1.0f);

	bank.AddSlider("Peds waiting for delete", &s_sNumberOfPedsWaitingToDel, 0, 2000, 1);
	bank.AddSlider("Vehs waiting for delete", &s_sNumberOfVehiclesWaitingToDel, 0, 2000, 1);
	bank.AddSlider("Objs waiting for delete", &s_sNumberOfObjectsWaitingToDel, 0, 2000, 1);

	bank.AddSlider("Streaming Num Objs requested", &sm_sNumberOfRealObjectsRequested, -1, 200, 1);
	bank.AddSlider("Streaming stall timer", &sm_uStreamingStallTimer, 0, 1000, 1);
	bank.AddSlider("Audio stall timer", &sm_uAudioStallTimer, 0, 1000, 1);

	bkBank& bankBoneData = BANKMGR.CreateBank("Replay bonedata map");
	bankBoneData.AddToggle("Debug Bones", &s_bTurnOnDebugBones);
	bankBoneData.AddSlider("Debug Bone Id", &s_uBoneId, 0, 65535, 1);
	
	bank.AddToggle("Enable Clip Compression", &sm_ClipCompressionEnabled);

	bank.AddSlider("BlockRemotePlayerRecording Add-On Distance", &sm_fExtraBlockRemotePlayerRecordingDistance, 1.0f, 100.0f, 0.5f);

	bank.PopGroup();


	bank.PushGroup("Perf", false);

	bank.AddButton("Core 0", Core0Callback);
	bank.AddButton("Core 5", Core5Callback);
	bank.AddToggle("Update ped skeletons", &sm_bUpdateSkeletonsEnabledInRag);
	bank.AddToggle("Quaternion quantization optimization", &sm_bQuantizationOptimizationEnabledInRag);
	bank.AddSlider("Recording thread count", &sm_wantRecordingThreadCount, 1, MAX_REPLAY_RECORDING_THREADS, 1);

	bank.PopGroup();

	bank.PushGroup("Debug", false);

	bank.AddText("ReplayID or Map Hash to Highlight", s_ReplayIDToHighlight, 16);
	bank.AddButton("Highlight Replay Entity", HighlightReplayEntity);
	bank.AddButton("Highlight Map Object from Map Hash", HighlightMapObjectViaMapHash);
	bank.AddButton("Highlight Map Object hidden by selected Replay Object", HighlightMapObject);
	bank.AddButton("Highlight Parent", HighlightParent);

	bank.PopGroup();

#if GTA_REPLAY_OVERLAY	
	CReplayOverlay::GeneratePacketNameList(sm_interfaceManager, sm_eventManager);
	CReplayOverlay::AddWidgets(bank);
#endif //GTA_REPLAY_OVERLAY

	sm_interfaceManager.AddDebugWidgets();

	sm_eventManager.AddDebugWidgets();
}

void CReplayMgrInternal::UpdateBanks(u32 eBankFlags)
{
	if (eBankFlags & REPLAY_BANK_FRAMES_ELAPSED)
		s_fFramesElapsed++;
	if (eBankFlags & REPLAY_BANK_SECONDS_ELAPSED)
		s_fSecondsElapsed = s_fFramesElapsed/30.0f;
	if (eBankFlags & REPLAY_BANK_PEDS_BEFORE_REPLAY)
		s_sNumberOfPedsBeforeReplay = (s32) CPed::GetPool()->GetNoOfUsedSpaces();
	if (eBankFlags & REPLAY_BANK_VEHS_BEFORE_REPLAY)
		s_sNumberOfVehiclesBeforeReplay = (s32) CVehicle::GetPool()->GetNoOfUsedSpaces();
	if (eBankFlags & REPLAY_BANK_OBJS_BEFORE_REPLAY)
	{
		s_sNumberOfObjectsBeforeReplay = (s32) CObject::GetPool()->GetNoOfUsedSpaces();
		s_sNumberOfDummyObjectsBeforeReplay = (s32) CDummyObject::GetPool()->GetNoOfUsedSpaces();
	}
	if (eBankFlags & REPLAY_BANK_PEDS_DURING_REPLAY)
		s_sNumberOfPedsDuringReplay = (s32) CPed::GetPool()->GetNoOfUsedSpaces();
	if (eBankFlags & REPLAY_BANK_VEHS_DURING_REPLAY)
		s_sNumberOfVehiclesDuringReplay = (s32) CVehicle::GetPool()->GetNoOfUsedSpaces();
	if (eBankFlags & REPLAY_BANK_OBJS_DURING_REPLAY)
	{
		s_sNumberOfObjectsDuringReplay = (s32) CObject::GetPool()->GetNoOfUsedSpaces();
		s_sNumberOfDummyObjectsDuringReplay = (s32) CDummyObject::GetPool()->GetNoOfUsedSpaces();
	}
	/*TODO4FIVE if (eBankFlags & REPLAY_BANK_NODES_DURING_REPLAY)
		s_sNumberOfNodesDuringReplay = CEntryInfoNode::GetPool()->GetNoOfUsedSpaces();
	if (eBankFlags & REPLAY_BANK_NODES_BEFORE_REPLAY)
		s_sNumberOfNodesBeforeReplay = CEntryInfoNode::GetPool()->GetNoOfUsedSpaces();*/
	if (eBankFlags & REPLAY_BANK_PEDS_TO_DEL)
	{
		s_sNumberOfPedsWaitingToDel = 0;
		for (s32 pedIndex = 0; pedIndex < (s32) CPed::GetPool()->GetSize(); ++pedIndex)
		{
			CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);

			if (pPed && pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				s_sNumberOfPedsWaitingToDel++;
			}
		}
	}
	if (eBankFlags & REPLAY_BANK_VEHS_TO_DEL)
	{
		s_sNumberOfVehiclesWaitingToDel = 0;
		for (s32 vehIndex = 0; vehIndex < (s32) CVehicle::GetPool()->GetSize(); ++vehIndex)
		{
			CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(vehIndex);

			if (pVehicle && pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				s_sNumberOfVehiclesWaitingToDel++;
			}
		}
	}
	if (eBankFlags & REPLAY_BANK_OBJS_TO_DEL)
	{
		s_sNumberOfObjectsWaitingToDel = 0;
		for (s32 objIndex = 0; objIndex < (s32) CObject::GetPool()->GetSize(); ++objIndex)
		{
			CObject* pObject = CObject::GetPool()->GetSlot(objIndex);

			if (pObject && pObject->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				s_sNumberOfObjectsWaitingToDel++;
			}
		}
	}

	if (eBankFlags & REPLAY_BANK_PFX_EFFECT_LIST)
	{
		/*TODO4FIVE s_sNumberOfParticleEffectsAvailable = RMPTFXMGR.GetNumberOfEffectInstAvailable();*/
	}


	//g_FrontendAudioEntity.SetMontageMusicVolume(sm_fMonatgeMusicVolume);
	//g_FrontendAudioEntity.SetMontageSFXVolume(sm_fMonatgeSFXVolume);

	if(sm_uMode == REPLAYMODE_RECORD)
	{
		s_currentReplayBlockPos = sm_oRecord.GetPosition();
		s_currentReplayBlock	= sm_oRecord.GetMemoryBlockIndex();
	}
	else if(sm_uMode == REPLAYMODE_EDIT)
	{
		CAddressInReplayBuffer oPrevInterpFrame = sm_currentFrameInfo.GetAddress();

		s_currentReplayBlockPos = oPrevInterpFrame.GetPosition();
		s_currentReplayBlock	= oPrevInterpFrame.GetMemoryBlockIndex();

		s_currentProgressPercentage = ((float)GetCurrentTimeRelativeMs() / (float)GetTotalTimeMS()) * 100.0f;
	}

	s_trackablesCount = sm_eventManager.GetTrackablesCount();
}
#endif // #if __BANK

#if RSG_BANK && !__NO_OUTPUT
void CReplayMgrInternal::DebugStackPrint(size_t, const char* text, size_t) 
{
	replayDebugf2("%s", text); 
}
#endif

//========================================================================================================================
void CReplayMgrInternal::SetNextPlayBackState(u32 state)
{
#if RSG_BANK && !__NO_OUTPUT
	replayDebugf2("--------------------------------------------------------");
	sysStack::PrintStackTrace(DebugStackPrint);
	replayDebugf2("SetNextPlayBackState sm_nextReplayState - oldState:%i newState:%i", sm_nextReplayState.GetState(), state);
	replayDebugf2("SetNextPlayBackState sm_eReplayState - state:%i", sm_eReplayState.GetState());
	replayDebugf2("--------------------------------------------------------");
#endif

	sm_nextReplayState.SetState(state);
}

//========================================================================================================================
void CReplayMgrInternal::ClearNextPlayBackState(u32 state)
{
#if RSG_BANK && !__NO_OUTPUT
	replayDebugf2("--------------------------------------------------------");
	sysStack::PrintStackTrace(DebugStackPrint);
	replayDebugf2("ClearNextPlayBackState sm_nextReplayState - oldState:%i newState:%i", sm_nextReplayState.GetState(), state);
	replayDebugf2("ClearNextPlayBackState sm_eReplayState - state:%i", sm_eReplayState.GetState());
	replayDebugf2("--------------------------------------------------------");
#endif

	sm_nextReplayState.ClearState(state);
}

//========================================================================================================================
float CReplayMgrInternal::GetTotalTimeSec()
{
	return ((float)(sm_uPlaybackEndTime - sm_uPlaybackStartTime))/1000.0f;
}

//========================================================================================================================
u32 CReplayMgrInternal::GetTotalTimeMS()
{
	return sm_uPlaybackEndTime - sm_uPlaybackStartTime;
}

float CReplayMgrInternal::GetTotalTimeMSFloat()
{
	return (float)(sm_uPlaybackEndTime - sm_uPlaybackStartTime);
}

//========================================================================================================================
bool CReplayMgrInternal::IsSaveReplayButtonPressed()
{
	if (CControlMgr::GetMainFrontendControl().GetSaveReplayClip().IsPressed())
	{
		return true;
	}

#if RSG_BANK
	if( (CControlMgr::GetPlayerPad()->GetLeftShoulder1() && CControlMgr::GetPlayerPad()->GetLeftShoulder2() &&
		CControlMgr::GetPlayerPad()->GetRightShoulder1() && CControlMgr::GetPlayerPad()->GetRightShoulder2()) || 
		(CControlMgr::GetDebugPad().GetLeftShoulder1() && CControlMgr::GetDebugPad().GetLeftShoulder2() &&
		CControlMgr::GetDebugPad().GetRightShoulder1() && CControlMgr::GetDebugPad().GetRightShoulder2()) )
	{
		return true;
	}
#endif // RSG_BANK

	return false;
}


//========================================================================================================================
bool CReplayMgrInternal::IsDiskSpaceAvailable( size_t const size )
{
#if RSG_PC
	char szPath[RAGE_MAX_PATH];

	// need to call this to make the user data directory is created.
	CFileMgr::SetUserDataDir("Videos");
	sprintf(szPath, "%s", CFileMgr::GetUserDataRootDirectory());

	ULARGE_INTEGER ulFreeBytesAvailable;
	ULARGE_INTEGER ulTotalNumberOfBytes;
	ULARGE_INTEGER ulTotalNumberOfFreeBytes;

	ulFreeBytesAvailable.QuadPart	  = 0;
	ulTotalNumberOfBytes.QuadPart	  = 0;
	ulTotalNumberOfFreeBytes.QuadPart = 0;

	if (GetDiskFreeSpaceEx(szPath, &ulFreeBytesAvailable, &ulTotalNumberOfBytes, &ulTotalNumberOfFreeBytes))
	{
		if (ulFreeBytesAvailable.QuadPart > size )
		{
			return true;
		}
	}
#else
	(void)size;
	replayFatalAssertf(false, "Todo for other platforms");
#endif

	return false;
}

//========================================================================================================================
bool CReplayMgrInternal::IsDiskSpaceAvailableForClip()
{
	const u64 CLIP_DISK_SPACE = 300 * 1024 * 1024;

	return IsDiskSpaceAvailable(CLIP_DISK_SPACE);
}

bool CReplayMgrInternal::IsLastFrame()
{
	if (sm_uMode == REPLAYMODE_EDIT )
	{
		if ( !IsClipTransition() )
		{
			u32 uCurrentTime   = GetCurrentTimeRelativeMs();
			u32 uEndTime = GetTotalTimeRelativeMs();
			
			if ( uCurrentTime + sm_playbackFrameStepDelta >= uEndTime )
			{
				return true;
			}
		}
	}

	return false;
}

bool CReplayMgrInternal::IsLastFrameAndLastClip()
{
	return IsLastFrame() && sm_pPlaybackController && sm_pPlaybackController->IsCurrentClipLastClip();
}

//========================================================================================================================
void CReplayMgrInternal::PauseReplayPlayback()
{
	sm_eReplayState.SetState( REPLAY_STATE_PAUSE );
	SetNextPlayBackState(REPLAY_STATE_PAUSE);

}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgrInternal::IsAwareOfWorldForRecording()
{
	return (sm_uMode == REPLAYMODE_RECORD || (sm_uMode == REPLAYMODE_DISABLED && sm_OverwatchEnabled == true)) && CScriptHud::bUsingMissionCreator == false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::OnCreateEntity(CPhysical* pEntity)
{
	if(pEntity && IsAwareOfWorldForRecording())
	{	
		replayAssert(pEntity->GetReplayID() == ReplayIDInvalid);

		sm_interfaceManager.OnCreateEntity(pEntity);

		//replayAssert(pEntity->GetReplayID() != ReplayIDInvalid);// This fires on pickups as they're init'd as objects first
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::OnDeleteEntity(CPhysical* pEntity)
{
	if(IsAwareOfWorldForRecording())
	{
		//replayAssert(pEntity->GetReplayID() != ReplayIDInvalid); // This can happen with vehicles in the reuse pool... they had their ID removed when they went into the pool but then they may be deleted from there...

		sm_interfaceManager.OnDeleteEntity(pEntity);
	}

#if __ASSERT
	if(pEntity->GetReplayID() != ReplayIDInvalid)
	{
		u32 ownedBy = pEntity->GetOwnedBy();
		int entityType = pEntity->GetType();
		replayAssertf(pEntity->GetReplayID() == ReplayIDInvalid, "Entity being deleted but has an ID...likely the game is deleting a replay entity! (ownedBy %u, type %d)", ownedBy, entityType);
	}
#endif // __ASSERT
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgrInternal::OnPlayerDeath()
{
#if __BANK
	SaveCallback();
#endif

	sm_desiredMode = REPLAYMODE_DISABLED;
}

//////////////////////////////////////////////////////////////////////////
iReplayInterface* CReplayMgrInternal::GetReplayInterface(const atHashWithStringNotFinal& type)
{
	return sm_interfaceManager.FindCorrectInterface(type);
}


//////////////////////////////////////////////////////////////////////////
iReplayInterface* CReplayMgrInternal::GetReplayInterface(const CEntity* pEntity)
{
	return sm_interfaceManager.FindCorrectInterface(pEntity);
}


//========================================================================================================================
void CReplayMgrInternal::UpdateTimer()
{
	if ( CanUpdateGameTimer() && sm_uPlaybackStartTime != 0xffffffff)
	{
		u32 uGameTime  = GetCurrentTimeAbsoluteMs();
		float fGameDelta = sm_playbackFrameStepDelta/1000.0f;

		if(!replayVerifyf(uGameTime != 0, "uGameTime should not be 0"))
		{
			return;
		}

		fwTimer::GetCamTimer().SetTimeInMilliseconds(uGameTime);
		fwTimer::GetCamTimer().SetTimeStepInSeconds(abs(fGameDelta));

		if(sm_playbackFrameStepDelta <= 0)
		{
			TUNE_GROUP_INT(REPLAY_TIMER, iInvalidReplayTimeDeltaMs, 1, 1, 100000, 1);
			fGameDelta = (float)iInvalidReplayTimeDeltaMs/1000.0f;
		}

		fwTimer::GetGameTimer().SetTimeInMilliseconds(uGameTime);
		fwTimer::GetGameTimer().SetTimeStepInSeconds(fGameDelta);

		fwTimer::GetScaledNonClippedTimer().SetTimeInMilliseconds(uGameTime);
		fwTimer::GetScaledNonClippedTimer().SetTimeStepInSeconds(fGameDelta);

		fwTimer::GetNonScaledClippedTimer().SetTimeInMilliseconds(uGameTime);
		fwTimer::GetNonScaledClippedTimer().SetTimeStepInSeconds(fGameDelta);

		fwTimer::GetReplayTime().SetTimeInMilliseconds(uGameTime);
		fwTimer::GetReplayTime().SetTimeStepInSeconds(fGameDelta);


		//CTimer::GetNonPausableCamTime().SetTimeInMilliseconds(uGameTime);
		//CTimer::GetNonPausableCamTime().SetTimeStepInSeconds(fTimeStepInSeconds);
	}
}

//========================================================================================================================
void CReplayMgrInternal::StoreGameTimers()
{
	m_CamTimerTimeInMilliseconds = fwTimer::GetCamTimer().GetTimeInMilliseconds();
	m_CamTimerTimeStepInSeconds = fwTimer::GetCamTimer().GetTimeStepInMilliseconds() / 1000.0f;  

	m_GameTimerTimeInMilliseconds = fwTimer::GetGameTimer().GetTimeInMilliseconds();
	m_GameTimerTimeStepInSeconds = fwTimer::GetGameTimer().GetTimeStepInMilliseconds() / 1000.0f;

	m_ScaledNonClippedTimerTimeInMilliseconds = fwTimer::GetScaledNonClippedTimer().GetTimeInMilliseconds();
	m_ScaledNonClippedTimerTimeStepInSeconds = fwTimer::GetScaledNonClippedTimer().GetTimeStepInMilliseconds() / 1000.0f;

	m_NonScaledClippedTimerTimeInMilliseconds = fwTimer::GetNonScaledClippedTimer().GetTimeInMilliseconds();
	m_NonScaledClippedTimerTimeStepInSeconds = fwTimer::GetNonScaledClippedTimer().GetTimeStepInMilliseconds() / 1000.0f;

	m_ReplayTimerTimeInMilliseconds = fwTimer::GetReplayTime().GetTimeInMilliseconds();
	m_ReplayTimerTimeStepInSeconds = fwTimer::GetReplayTime().GetTimeStepInMilliseconds() / 1000.0f;

	m_ReplayTimersStored = true;
}

//========================================================================================================================
void CReplayMgrInternal::RestoreGameTimers()
{
	if( m_ReplayTimersStored )
	{
		fwTimer::GetCamTimer().SetTimeInMilliseconds(m_CamTimerTimeInMilliseconds);
		fwTimer::GetCamTimer().SetTimeStepInSeconds(m_CamTimerTimeStepInSeconds);

		fwTimer::GetGameTimer().SetTimeInMilliseconds(m_GameTimerTimeInMilliseconds);
		fwTimer::GetGameTimer().SetTimeStepInSeconds(m_GameTimerTimeStepInSeconds);

		fwTimer::GetScaledNonClippedTimer().SetTimeInMilliseconds(m_ScaledNonClippedTimerTimeInMilliseconds);
		fwTimer::GetScaledNonClippedTimer().SetTimeStepInSeconds(m_ScaledNonClippedTimerTimeStepInSeconds);

		fwTimer::GetNonScaledClippedTimer().SetTimeInMilliseconds(m_NonScaledClippedTimerTimeInMilliseconds);
		fwTimer::GetNonScaledClippedTimer().SetTimeStepInSeconds(m_NonScaledClippedTimerTimeStepInSeconds);

		fwTimer::GetReplayTime().SetTimeInMilliseconds(m_ReplayTimerTimeInMilliseconds);
		fwTimer::GetReplayTime().SetTimeStepInSeconds(m_ReplayTimerTimeStepInSeconds);

		m_ReplayTimersStored = false;
	}
}

//========================================================================================================================
void CReplayMgrInternal::ValidateTitle()
{
	/*TODO4FIVE if (CFrontEnd::IsInTitleMenu())
	{
		HandleLogicForMemory();
	}*/
}


void CReplayMgrInternal::ProcessCursorMovement()
{
	float fOldInterpTime = GetCurrentTimeMsFloat();
	bool clearTrails = false;

	switch(GetNextCursorState())
	{
		case REPLAY_CURSOR_JUMP:
		{
			float const c_currentTimeRelativeMs = GetCurrentTimeRelativeMsFloat();
			float const c_jumpTimeDiff = m_TimeToJumpToMS - c_currentTimeRelativeMs;

			//don't seek if the destination is the same as the current time
			if( fabsf(c_jumpTimeDiff) < REPLAY_JUMP_EPSILON )
			{
				return;
			}

			// This should perhaps be passing through the position we're jumping to
			// and resetting only past this point?
			sm_interfaceManager.ResetAllEntities();

			CAddressInReplayBuffer historyStartAddress;
			FindStartOfPlayback(historyStartAddress);

			// Rewind the history buffer to the time requested
			sm_oPlayback = historyStartAddress;
			RewindHistoryBufferAddress(sm_oPlayback, (u32)m_TimeToJumpToMS);
			sm_oHistoryProcessPlayback = sm_oPlayback;

			m_jumpDelta = m_TimeToJumpToMS - fOldInterpTime;
			replayAssert(m_jumpDelta != 0.f);

			sm_PokeGameSystems = true;		// Nudge some game systems to update even if the game is paused.
			ResetParticles();

			//reset the light list for this frame
			ReplayLightingManager::Reset();

			sm_eventManager.SetPlaybackTrackablesStale();

			ClearOldSounds();

			// Screen bloom-ed out when jumping back to the start of a replay clip from an interior, force reset of exposure.
			PostFX::ResetAdaptedLuminance();
			CParaboloidShadow::RequestJumpCut();

			g_ptFxManager.ResetStoredLights();

			clearTrails = true;
		}
		break;
		case REPLAY_CURSOR_SPEED:
			{
				// Leave to drop through
			}
		case REPLAY_CURSOR_NORMAL:
		{
			if(sm_nextReplayState.IsSet(REPLAY_DIRECTION_BACK))
			{
				clearTrails = true;
			}
		}
		break;
		default:
			return;
		break;
	}

	if(clearTrails)
	{	// Remove any 'Trail' decals from the world
		u32 decalExceptions = 0;
		decalExceptions |= 1 << DECALTYPE_TRAIL_SCRAPE;
		decalExceptions |= 1 << DECALTYPE_TRAIL_GENERIC;
		decalExceptions = ~decalExceptions;
		g_decalMan.Remove(NULL, -1, NULL, decalExceptions);
	}

}

bool CReplayMgrInternal::GetBuildingInfo(const CPhysical* pEntity, u32& hashKey, u16& index)
{
	if(pEntity->GetIsTypeBuilding())
	{
		// Procedural buildings don't get recorded and cannot be relied upon to exist in the
		// correct location on playback....if at all.
		if(pEntity->GetOwnedBy() == ENTITY_OWNEDBY_PROCEDURAL)
			return false;

		if(pEntity->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT) && !pEntity->IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS))
		{	// Flagged as having it's own physics...
			CBuilding* pBuilding = (CBuilding*)pEntity;
			Vector3 pos = VEC3V_TO_VECTOR3(pBuilding->GetTransform().GetPosition());
			hashKey = CReplayMgr::GenerateObjectHash(pos, pBuilding->GetArchetype()->GetModelNameHash());
			index = NON_STATIC_BOUNDS_BUILDING;

			return true;
		}


		rage::phInst* pEntityPhInst = g_pDecalCallbacks->GetPhysInstFromEntity(pEntity);
		// Get the slot from the static bound store...
		hashKey = g_StaticBoundsStore.GetHash(strLocalIndex(pEntity->GetIplIndex()));
		strLocalIndex slot = g_StaticBoundsStore.FindSlotFromHashKey(hashKey);
		u16 physInstCount = 0;
		if(slot != -1)
		{
			rage::fwBoundData* pData = g_StaticBoundsStore.GetSlotData(slot.Get());
			// We should assert here, but for some reason this is a valid state in replay (see comment below)
			//Assertf(pData, "This is really bad! Unable to find fwBoundData for fwBoundDef %d", slot.Get());

			if (pData)
			{
				physInstCount = pData->m_physInstCount;
				for(u16 i = 0; i < physInstCount; ++i)
				{
					if(pData->m_physicsInstArray[i] == pEntityPhInst)
					{
						index = i;
						return true;
					}
				}
			}
		}
	}

	replayDebugf1("Unable to find building to get info...");

	return false;
}

void CReplayMgrInternal::RecordMapObject(const CEntity* pEntity)
{
	iReplayInterface* pI = GetReplayInterface("CObject");
	replayFatalAssertf(pI, "Interface is NULL");
	CReplayInterfaceObject* pObjectInterface = verify_cast<CReplayInterfaceObject*>(pI);
	replayFatalAssertf(pObjectInterface, "Incorrect interface");

	pObjectInterface->RecordMapObject((CObject*)pEntity);
}

void CReplayMgrInternal::PrintHistoryPacketsFunc(FileHandle handle)
{
	CAddressInReplayBuffer address;

	FindStartOfPlayback(address);
	ReplayController controller(address, sm_ReplayBufferInfo);

	sm_eventManager.PrintOutHistoryPackets(controller, REPLAY_OUTPUT_DISPLAY, handle);
}

void CReplayMgrInternal::PrintHistoryPackets()
{
	if(IsEditModeActive() && IsPerformingFileOp() == false)
	{
		replayDebugf1("==============================");
		replayDebugf1("History Packets");
		// Find the first block after this that is full (or being filled)
	
		// REPLAY_CLIPS_PATH change
		char clipPath[REPLAYPATHLENGTH];
		ReplayFileManager::getClipDirectory(clipPath);
		atString path = atString(clipPath);
		path += "HistoryPackets.txt";

		sm_FileManager.StartSaveFile(path, &PrintHistoryPacketsFunc, NULL);
	}
	else
	{
		replayDebugf1("==============================");
		replayDebugf1("PrintHistoryPackets : IsEnabled() || IsPerformingFileOp()");
	}
}

void CReplayMgrInternal::PrintReplayPacketsFunc(FileHandle handle)
{
	CAddressInReplayBuffer address;

	FindStartOfPlayback(address);
	ReplayController controller(address, sm_ReplayBufferInfo);

	sm_interfaceManager.PrintOutReplayPackets(controller, REPLAY_OUTPUT_DISPLAY, handle);
}

void CReplayMgrInternal::PrintReplayPackets()
{
	if(IsEditModeActive() && IsPerformingFileOp() == false)
	{
 		replayDebugf1("==============================");
 		replayDebugf1("Replay Packets");
 		// Find the first block after this that is full (or being filled)
 	
		// REPLAY_CLIPS_PATH change
		char clipPath[REPLAYPATHLENGTH];
		ReplayFileManager::getClipDirectory(clipPath);
		atString path = atString(clipPath);
		path += "ReplayPackets.txt";

		sm_FileManager.StartSaveFile(path, &PrintReplayPacketsFunc, NULL);
 	}
	else
	{
		replayDebugf1("==============================");
		replayDebugf1("PrintReplayPackets : IsEnabled() || IsPerformingFileOp()");
	}
}

#if GTA_REPLAY_OVERLAY
void CReplayMgrInternal::CalculateBlockStats(CBlockInfo* pBlock)
{
	if( pBlock && pBlock->GetStatus() == REPLAYBLOCKSTATUS_FULL )
	{
		CAddressInReplayBuffer address;
		address.SetPosition(0);
		address.SetBlock(pBlock);	

		ReplayController controller(address, sm_ReplayBufferInfo);
		controller.EnablePlayback();

		eReplayClipTraversalStatus status = UNHANDLED_PACKET;
		while(status != END_CLIP)
		{
			status = sm_interfaceManager.CalculateBlockStats(controller, pBlock);
			if(status == UNHANDLED_PACKET)
				status = sm_eventManager.CalculateBlockStats(controller, pBlock);
		};

		controller.DisablePlayback();
	}	
}

#endif //GTA_REPLAY_OVERLAY


bool CReplayMgrInternal::IsReplayCursorJumping()
{
	return (IsEditModeActive() && sm_eReplayState.IsSet(REPLAY_CURSOR_JUMP));
}

#define PRINT_BUILDINGS_FOR_REPLAY 0
bool CReplayMgrInternal::AddBuilding(CBuilding* pBuilding)
{
	static s32 HighCount = 0;
	if(IsReplayInControlOfWorld() && pBuilding->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT) && !pBuilding->IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS))
	{	
		// Add the building to the replay system
		Vector3 pos = VEC3V_TO_VECTOR3(pBuilding->GetTransform().GetPosition());
		u32 hash = CReplayMgr::GenerateObjectHash(pos, pBuilding->GetArchetype()->GetModelNameHash());

		// See if we actually need this building for an event
		if(sm_eventManager.IsStaticBuildingRequired(hash) == false)
		{
			return false;
		}

		if(sm_BuildingHashes.Access(hash) == NULL)
		{
			sm_BuildingHashes.Insert(hash, pBuilding);
			HighCount = Max(HighCount, sm_BuildingHashes.GetNumUsed());
#if PRINT_BUILDINGS_FOR_REPLAY
			replayDebugf1("Adding building... %u, %p, %d, %d", hash, pBuilding, sm_BuildingHashes.GetNumUsed(), HighCount);
#endif // PRINT_BUILDINGS_FOR_REPLAY
			return true;
		}
	}

	return false;
}


void CReplayMgrInternal::RemoveBuilding(CBuilding* pBuilding)
{
	if(CReplayMgr::IsReplayInControlOfWorld() && pBuilding->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT) && !pBuilding->IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS))
	{
		// Remove the building from the replay system
		Vector3 pos = VEC3V_TO_VECTOR3(pBuilding->GetTransform().GetPosition());
		u32 hash = CReplayMgr::GenerateObjectHash(pos, pBuilding->GetArchetype()->GetModelNameHash());

		sm_BuildingHashes.Delete(hash);
	}
}


CBuilding* CReplayMgrInternal::GetBuilding(u32 hash)
{
	CBuilding** p = sm_BuildingHashes.Access(hash);
	if(p)
	{
		return *p;
	}
	return NULL;
}

#endif // GTA_REPLAY
