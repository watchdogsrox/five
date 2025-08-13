// 
// audio/vehicleaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "caraudioentity.h"
#include "frontendaudioentity.h"
#include "debugaudio.h"
#include "northaudioengine.h"
#include "audio/environment/environment.h"
#include "pedaudioentity.h"
#include "peds/PedFactory.h"
#include "vehicleaudioentity.h"
#include "policescanner.h"
#include "radioaudioentity.h"
#include "radiostation.h"
#include "radioslot.h"
#include "scriptaudioentity.h"
#include "weatheraudioentity.h"
#include "modelinfo/VehicleModelInfoEnums.h"
#include "vehiclereflectionsaudioentity.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/Submarine.h"
#include "entity/archetypemanager.h"

#include "boataudioentity.h"
#include "heliaudioentity.h"
#include "animation/animbones.h"
#include "audioeffecttypes/waveshapereffect.h"
#include "audioengine/categorymanager.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/environment.h"
#include "audioengine/environment_game.h"
#include "audioengine/soundfactory.h"
#include "audioengine/widgets.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "audiosoundtypes/envelopesound.h"
#include "audiosoundtypes/simplesound.h"
#include "audiosoundtypes/granularsound.h"
#include "audiosoundtypes/sound.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/sounddefs.h"
#include "atl/staticpool.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "debug/DebugScene.h"
#include "vehicleAi/vehicleintelligence.h"
#include "control/record.h"
#include "crskeleton/skeleton.h"
#include "vfx/systems/VfxWheel.h"
#include "debug/debugglobals.h"
#include "grcore/debugdraw.h"
#include "game/ModelIndices.h"
#include "game/weather.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "network/NetworkInterface.h"
#include "phbound/boundcomposite.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "profile/element.h"
#include "fwsys/timer.h"
#include "vehicles/automobile.h"
#include "vehicles/boat.h"
#include "vehicles/bike.h"
#include "vehicles/door.h"
#include "vehicles/heli.h"
#include "vehicles/vehicle.h"
#include "vehicles/Trailer.h"
#include "vfx/Systems/VfxVehicle.h"
#include "peds/pedeventscanner.h"
#include "peds/PopCycle.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "system/controlMgr.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "renderer/water.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "control/replay/Vehicle/VehiclePacket.h"
#include "Control/Replay/Audio/WeaponAudioPacket.h"
#include "control/replay/replay.h"
#include "camera/helpers/ControlHelper.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "vehicles/AmphibiousAutomobile.h"

#include "debugaudio.h"

AUDIO_VEHICLES_OPTIMISATIONS()

XPARAM(audiodesigner);

PF_PAGE(VehicleAudioEntityPage,"audVehicleAudioEntity");
PF_GROUP(VehicleAudioEntity);
PF_LINK(VehicleAudioEntityPage, VehicleAudioEntity);
PF_VALUE_FLOAT(Revs, VehicleAudioEntity);
PF_VALUE_FLOAT(RawRevs, VehicleAudioEntity);
PF_VALUE_FLOAT(PowerFactor, VehicleAudioEntity);
PF_VALUE_FLOAT(Throttle, VehicleAudioEntity);
PF_VALUE_INT(Gear, VehicleAudioEntity);

PF_PAGE(VehicleAudioTimingPage, "audVehicleAudioEntity Timings");
PF_GROUP(VehicleAudioTimings);
PF_LINK(VehicleAudioTimingPage, VehicleAudioTimings);
PF_TIMER(UpdateInternal, VehicleAudioTimings);
PF_TIMER(UpdateEngineAndExhaust, VehicleAudioTimings);
PF_TIMER(UpdateEngineFilters, VehicleAudioTimings);
PF_TIMER(UpdateStartLoop, VehicleAudioTimings);
PF_TIMER(UpdateBrakes, VehicleAudioTimings);
PF_TIMER(PopulateVariables, VehicleAudioTimings);

extern CVfxWheel g_vfxWheel;

bool audCarAudioEntity::sm_ShouldDisplayEngineDebug = false;
bool audCarAudioEntity::sm_ShouldFlipPlayerCones = false;

extern bank_float g_FakeRevsOffsetBelow;
extern bank_float g_FakeRevsOffsetAbove;
extern bank_float g_FakeRevsMaxHoldTimeMin;
extern bank_float g_FakeRevsMaxHoldTimeMax;
extern bank_float g_FakeRevsMinHoldTimeMin;
extern bank_float g_FakeRevsMinHoldTimeMax;
extern bank_float g_FakeRevsIncreaseRateBase;
extern bank_float g_FakeRevsIncreaseRateScale;
extern bank_float g_FakeOutOfWaterMinTime;
extern bank_float g_FakeOutOfWaterMaxTime;
extern bank_float g_FakeInWaterMinTime;
extern bank_float g_FakeInWaterMaxTime;
extern bank_float g_FakeRevsDecreaseRateBase;
extern bank_float g_FakeRevsDecreaseRateScale;
extern u32 g_FakeWaveHitDistanceSq;
extern f32 g_BankSprayPanDistanceScalar;
extern f32 g_BankSprayForwardDistanceScalar;
extern f32 g_LateralSkidSmootherIncrease;
extern f32 g_LateralSkidSmootherDecrease;
extern f32 g_MainSkidSmootherIncrease;
extern f32 g_MainSkidSmootherDecrease;

f32 audCarAudioEntity::sm_InnerUpdateFrames = 3.0f;
f32 audCarAudioEntity::sm_OuterUpdateFrames = 3.0f;
f32 audCarAudioEntity::sm_InnerCheapUpdateFrames = 6.0f;
f32 audCarAudioEntity::sm_OuterCheapUpdateFrames = 6.0f;
f32 audCarAudioEntity::sm_CarFwdSpeedSum = 1.0f;
f32 audCarAudioEntity::sm_NumActiveCars = 0;
u32 audCarAudioEntity::sm_LastSportsRevsTime = 0;
u32 audCarAudioEntity::sm_SportsCarRevsHistoryIndex = 0;
atRangeArray<audCarAudioEntity::SportsCarRevHistoryEntry, audCarAudioEntity::kSportsCarRevsHistorySize> audCarAudioEntity::sm_SportsCarRevsSoundHistory;

BANK_ONLY(atArray<audCarAudioEntity::carPassHistory> audCarAudioEntity::sm_CarPassHistory;)
BANK_ONLY(Vector3 audCarAudioEntity::sm_VolumeCaptureStartPos; )
BANK_ONLY(audCarAudioEntity::VolumeCaptureState audCarAudioEntity::sm_VolumeCaptureState = audCarAudioEntity::State_NextVehicle;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCaptureTimer = 0.0f;)
BANK_ONLY(s32 audCarAudioEntity::sm_VolumeCaptureVehicleIndex = 0;)
BANK_ONLY(s32 audCarAudioEntity::sm_VolumeCaptureVehicleStartIndex = 0;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCaptureIntervalTimer = 0.0f;)
BANK_ONLY(u32 audCarAudioEntity::sm_VolumeCaptureFramesAtState = 0u;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCaptureRevs = 0.0f;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCaptureThrottle = 0.0f;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCapturePeakLoudness = g_SilenceVolume;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCapturePeakRMS = g_SilenceVolume;)
BANK_ONLY(f32 audCarAudioEntity::sm_VolumeCapturePeakAmplitude = g_SilenceVolume;)
BANK_ONLY(FileHandle audCarAudioEntity::sm_VolumeCaptureFileID = INVALID_FILE_HANDLE;)
BANK_ONLY(atArray<f32> audCarAudioEntity::sm_VolumeCaptureLoudnessHistory;)
BANK_ONLY(atArray<f32> audCarAudioEntity::sm_VolumeCaptureRMSHistory;)
BANK_ONLY(atArray<f32> audCarAudioEntity::sm_VolumeCaptureAmplitudeHistory;)
BANK_ONLY(fwModelId audCarAudioEntity::sm_VolumeCaptureVehicleRequest;)
BANK_ONLY(extern f32 g_WaterTurbulenceVolumeTrim;)
BANK_ONLY(extern f32 g_BankSprayVolumeTrim;)
extern bool g_TreatAllBoatsAsNPCs;

f32 g_RevMaxIncreaseRate = 1.f;
f32 g_RevMaxDecreaseRate = 10.f;
f32 g_RevMaxForceStopDecreaseRate = 1.f;
s32 g_DamageLayerMinPitch = -300;
s32 g_DamageLayerMaxPitch = 300;
s32 g_DamagedWheelMinPitch = -200;
s32 g_DamagedWheelMaxPitch = 600;
f32 g_HighRevsMisfireSoundProb = 0.3f;
f32 g_BaseRollOffScalePlayer = 1.0f;
f32 g_BaseRollOffScaleNPC = 2.0f; 
f32 g_UnderLoadRevsSmootherScalar = 3.0f;
f32 g_TrunkRadioPositionOffset = 1.6f;
f32 g_HydraulicSuspensionAngleLoopTriggerThreshold = 0.1f;
f32 g_HydraulicSuspensionAngleLoopTriggerThresholdRemoteVehicle = 0.01f;
f32 g_HydraulicSuspensionAngleLoopReTriggerThreshold = 0.08f;
f32 g_HydraulicSuspensionAngleLoopReTriggerThresholdRemoteVehicle = 0.75f;

extern u32 g_MaxVehicleSubmixes;
extern u32 g_MaxDummyVehicles;
extern f32 g_GranularQualityCrossfadeSpeed;
extern f32 g_VehicleReflectionsActivationDist;
extern f32 g_MaxDummyRangeScale;
extern f32 g_MaxGranularRangeScale;
extern f32 g_MaxRealRangeScale;
extern f32 g_GranularActivationScaleIncreaseRate;
extern f32 g_GranularActivationScaleDecreaseRate;
extern f32 g_ActivationScaleIncreaseRate;
extern f32 g_ActivationScaleDecreaseRate;
extern f32 g_DummyActivationScaleIncreaseRate;
extern f32 g_DummyActivationScaleDecreaseRate;
extern f32 g_MinTimeAtDesiredLOD;
extern f32 g_MinTimeAtCurrentLOD;
extern u32 g_NPCRoadNoiseAttackTime;
extern u32 g_NPCRoadNoiseReleaseTime;
extern u32 g_SuperDummyRadiusSq;
extern u32 g_SportsCarRevsEngineSwitchTime;
extern u32 g_HydraulicsModifiedTimeoutMs;
extern u8 g_NumHydraulicActivationDelayFrames;
extern bool g_InteriorEngineExhaustAtListener;
extern bool g_InteriorEngineExhaustPanned;
extern bool g_DisablePlayerVehicleDSP;
extern bool g_DisableNPCVehicleDSP;
extern u32 g_StuntBoostIntensityTimeout;
extern u32 g_RocketBoostCooldownTime;
extern f32 g_IdleMainSmootherRateIncrease;
extern f32 g_IdleMainSmootherRateDecrease;

f32 g_StuntSpeedBoostDecelSmoothing = 100.f;
u32 g_StuntSpeedBoostDecelSmoothingDuration = 2500;

u32 g_MaxGranularEngines = 5;

f32 g_ExhaustPopRateThreshold = 1.f;
f32 audCarAudioEntity::sm_MaxSuspensionStartOffset = 90.f;

u32 g_EngineStartDuration = 1000;
u32 g_EngineStopDuration = 300;

f32 g_EngineVolumeTrim = 0.0f;
f32 g_ExhaustVolumeTrim = 0.0f;
f32 g_GranularAccelVolumeTrim = 0.0f;
f32 g_GranularDecelVolumeTrim = 0.0f;
f32 g_GranularIdleVolumeTrim = 0.0f;
f32 g_GranularXValueSmoothRate = 1.0f;
f32 g_GranularEngineRevMultiplier = 1.0f;
f32 g_NPCVehicleRevsScalar = 1.2f;
f32 g_OffscreenVisiblityScalar = 0.2f;
u32 g_EngineSwitchFadeInTime = 1000;
u32 g_EngineSwitchFadeOutTime = 500;
f32 g_LODSpeedBias = 0.9f;
f32 g_LODMinSpeedRatio = 0.05f;
f32 g_LODMaxSpeedRatio = 0.4f;
f32 g_MinDamageThrottleCutDuration = 0.05f;
f32 g_MaxDamageThrottleCutDuration = 0.25f;
f32 g_ShortDamageCutProbMin = 0.4f;
f32 g_ShortDamageCutProbMax = 0.7f;
s32 g_MinTimeBetweenDamageThrottleCutsShort = 50;
s32 g_MaxTimeBetweenDamageThrottleCutsShort = 350;
s32 g_MinTimeBetweenDamageThrottleCuts = 2000;
s32 g_MaxTimeBetweenDamageThrottleCuts = 4000;
f32 g_GearboxVolumeTrim = 0.0f;
f32 g_TurboVolumeTrim = 0.0f;
f32 g_WindNoiseSmootherIncreaseRate = 700.0f;
f32 g_WindNoiseSmootherDecreaseRate = 10.0f;
f32 g_HyrdaulicInputBounceLimit = 0.75f;
f32 g_MaxHydraulicBounceVolumeReduction = 0.5f;
u32 g_HydraulicsJumpLandSuppressionTime = 300;

bool g_LinkExhaustPopsToAudioRevs = true;
bool g_RevsBoostEffectEnabled = true;
f32 g_RevsBoostPitchOffset = 150.0f;
f32	g_RevsBoostRevsScalar = 1.2f;
f32	g_RevsBoostAngularVelocityLimit = 0.01f;
f32	g_RevsBoostAngularVelocityLimitSensitivity = 39.0f;
f32 g_RevsBoostPitchOffsetFadeInTime = 10.0f;
f32 g_RevsBoostPitchOffsetFadeOutTime = 2.0f;
f32 g_RevsBoostRevsScaleFadeInTime = 15.0f;
f32 g_RevsBoostRevsScaleFadeOutTime = 1.0f;
f32 g_RevsBoostEffectFwdSpeedRatio = 0.80f;
f32 g_RevsBoostEffectFwdSpeedRatioNPC = 0.70f;
f32 g_RevsBoostEffectRevRatio = 0.75f;

bool g_SportsCarRevsEnabled = true;
bool g_RevsBoostPitchOffsetOnlyInFinalGear = false;

bool g_DisplayClusteredVehicles = false;
bool g_EnableDamageThrottleCut = true;
bool g_GranularLimiterEnabled = true;
bool g_GranularWobblesEnabled = true;
bool g_GranularDebugRenderingEnabled = false;
bool g_NonGranularDebugRenderingEnabled = false;
bool g_GranularIdleDebugRenderingEnabled = false;
bool g_GranularForceLowQuality = false;
bool g_GranularForceHighQuality = false;
bool g_GranularEnableNPCGranularEngines = true;
bool g_ForceGranularNPCEngines = false;
bool g_RenderGranularNPCEngines = false;
bool g_GranularLODEqualPowerCrossfade = false;
bool g_AllowSynthsOnNetworkVehicles = true;

#if __BANK
bool g_RenderTimeSlicedVehicles = false;
bool g_EngineVolumeCaptureActive = false;
bool g_DisableHorns = false;
bool g_DebugSpecialFlightMode = false;
bool g_DebugCarPasses = false;
bool g_ToggleNos = false;
bool g_ForceEnableBlips = false;
bool g_DisableThrottleBlips = false;
bool g_ForceMuteGranularSynths = false;
bool g_DisableConing = false;
bool g_MutePlayerVehicle = false;
bool g_ForceShallowWater = false;
bool g_DrawVehicleLODQuadrants = false;
bool g_ForceAutoShutoffEngines = false;
bool g_SimulateDrowning = false;
bool g_GranularSkipGrainTogglePressed = false;
bool g_GranularSkipEveryOtherGrain = false;
bool g_GranularToneGeneratorEnabled = false;
bool g_GranularToggleTestTonePressed = false;
bool g_VehicleAudioAllowEntityFocus = false;
bool g_ForceRealLodOnFocusEntity = false;
bool g_SimulatePlayerEnteringVehicle = false;
bool g_TestForceGameObject = false;
char g_TestForceGameObjectName[64] = { "BANSHEE" };
bool g_GranularLoopEditorShiftLeftPressed = false;
bool g_GranularLoopEditorShiftRightPressed = false;
bool g_GranularLoopEditorGrowLoopPressed = false;
bool g_GranularLoopEditorShrinkLoopPressed = false;
bool g_GranularLoopEditorCreateLoopPressed = false;
bool g_GranularLoopEditorDeleteLoopPressed = false;
bool g_GranularLoopEditorExportDataPressed = false;
bool g_GranularEditorControlRevsThrottle = false;
bool g_GranularForceRevLimiter = false;
bool g_SetGranularEngineIdleNative = false;
bool g_SetGranularExhaustIdleNative = false;
bool g_DebugDrawTurret = false;
bool g_DebugDrawTowArm = false;
bool g_DebugDrawGrappling = false;
f32 g_GrapSoundAcceleration = 0.f;
f32 g_GrapIntensity = 0.f;
s32 g_WeaponAmmoCountOverride = -1;
bool g_DisplayDamageDebug = false;
bool g_MuteDamageLayers = false;
bool g_MuteFanbeltDamage = false;
bool g_MuteGranularDamage = false;
bool g_ForceAllCarsAlwaysDamaged = false;
bool g_OverrideHydraulicSuspensionDamage = false;
f32 g_OverridenHydraulicSuspensionDamage = 1.f;
s32 g_GranularInterGrainCrossfadeStyle = (s32)audGranularMix::s_InterGrainCrossfadeStyle;
s32 g_GranularLoopGrainCrossfadeStyle = (s32)audGranularMix::s_LoopGrainCrossfadeStyle;
s32 g_GranularLoopLoopCrossfadeStyle = (s32)audGranularMix::s_LoopLoopCrossfadeStyle;
s32 g_GranularMixCrossfadeStyle = (s32)audGranularMix::s_GranularMixCrossfadeStyle;
bool g_ShowVehiclesWithWaveSlots = false;
bool g_ForceSynthsOnAllEngines = false;
bool g_TestWobble = false;
u32 g_TestWobbleLength = 50;
f32 g_TestWobbleLengthVariance = 0.0f;
f32 g_TestWobbleSpeed = 0.175f;
f32 g_TestWobbleSpeedVariance = 0.0f;
f32 g_TestWobblePitch = 0.1f;
f32 g_TestWobblePitchVariance = 0.0f;
f32 g_TestWobbleVolume = 0.35f;
f32 g_TestWobbleVolumeVariance = 0.0f;
bool g_DrawVehiclesInModShop = false;
bool g_ForceUpgradeEngine = false;
f32 g_ForcedEngineUpgradeLevel = 1.0f;
bool g_ForceUpgradeExhaust = false;
f32 g_ForcedExhaustUpgradeLevel = 1.0f;
bool g_ForceUpgradeEngineBay[3] = { false, false, false };
f32 g_ForcedEngineBayUpgradeLevel[3] = { 1.0f, 1.0f, 1.0f };
bool g_ForceUpgradeTransmission = false;
f32 g_ForcedTransmissionUpgradeLevel = 1.0f;
bool g_ForceUpgradeTurbo = false;
f32 g_ForcedTurboUpgradeLevel = 1.0f;
bool g_ForceUpgradeRadio = false;
f32 g_ForcedRadioUpgradeLevel = 1.0f;
bool g_ForceDamage = false;
bool g_ForceWheelDamage = false;
bool g_TestBackfire = false;
bool g_TestBackfireBanger = false;
bool g_DebugDrawVehicleWater = false;
f32 g_ForcedDamageFactor = 0.0f;
f32 g_GranularEditorRevs = 0.0f;
f32 g_GranularEditorThrottle = 0.0f;
s32 g_ForcePlayerVehicleLOD = AUD_VEHICLE_LOD_UNKNOWN;

const char * g_VehicleLODNames[5] = { 
	"Real",
	"Dummy",
	"Super Dummy",
	"Disabled",
	"Default",
};

const char * g_GranularPlaybackStyleNames[4] = { 
	"Loops And Grains",
	"Loops Only",
	"Grains Only",
	"Dynamic",
};

const char * g_GranularCrossfadeStyleNames[2] = { 
	"Linear",
	"Equal Power",
};

const char * g_GranularMixStyleNames[3] = { 
	"Accel and Decel",
	"Accel Only",
	"Decel Only",
};

const char * g_GranularRandomisationStyleNames[5] = {
	"Sequential",
	"Random",
	"Walk",
	"Reverse",
	"MixNMatch",
};

f32 g_GranularSlidingWindowSizeHzFraction = 0.035f;
u32 g_GranularSlidingWindowMinGrains = 5;
u32 g_GranularSlidingWindowMaxGrains = 30;
u32 g_GranularMinRepeatRate = 3;
s32 g_GranularMixStyle = 0;
s32 g_GranularRandomisationStyle = audGranularSubmix::PlaybackOrderWalk;
s32 g_GranularIdleRandomisationStyle = audGranularSubmix::PlaybackOrderMax;
s32 g_GrainPlaybackStyle = audGranularMix::PlaybackStyleMax;

bool g_ForceSleighBellsOn = false;
#endif

extern f32 g_ClatterMaxAcceleration;
extern f32 g_SpinRatioForDebris;
extern f32 g_SpeedForSpinDebris;

extern f32 g_ElectricEngineBoostVolScale;
extern f32 g_ElectricEngineSpeedVolScale;
extern f32 g_ElectricEngineRevsOffVolScale;
extern f32 g_ElectricEngineSpeedLoopSpeedApplyScalar;
extern f32 g_ElectricEngineSpeedLoopSpeedBias;

audCurve audCarAudioEntity::sm_BrakeToBrakeDiscVolumeCurve;
audCurve audCarAudioEntity::sm_SpeedToBrakeDiscVolumeCurve;
audCurve audCarAudioEntity::sm_BrakeHoldTimeToStartOffsetCurve;
audSmoother audCarAudioEntity::sm_PlayerWindNoiseSpeedSmoother;
SportsCarRevsSettings* audCarAudioEntity::sm_SportsCarRevsSettings = NULL;

BANK_ONLY(bool g_AuditionWheelFire = false;);
BANK_ONLY(extern f32 g_WaveSlotStreamingDelayTime;)

audCurve g_EngineLowVolCurve;
audCurve g_EngineHighVolCurve;
audCurve g_ExhaustLowVolCurve;
audCurve g_ExhaustHighVolCurve;
audCurve g_EngineExhaustPitchCurve;
audCurve g_IdleVolCurve;
audCurve g_IdlePitchCurve;

// engine curves
audCurve audCarAudioEntity::sm_SuspensionVolCurve;
audCurve audCarAudioEntity::sm_SkidFwdSlipToScrapeVol, audCarAudioEntity::sm_LateralSlipToSideVol;
audCurve g_PlayerThrottleCurve;
audCurve g_PlayerRevsCurve;
audCurve g_PedThrottleCurve;
audCurve g_PedRevsCurve;
audCurve audCarAudioEntity::sm_EnvironmentalLoudnessThrottleCurve, audCarAudioEntity::sm_EnvironmentalLoudnessRevsCurve;
audCurve g_RevsSmoothingCurve;
audCurve g_PlayerAngVelToCarPass;

extern bool g_PretendThePlayerIsPissedOff;
extern bool g_BreakOnVehicleUpdate;
extern bool g_RenderSlotManager;
extern bool g_ShowRadioGenres;
extern bool g_DebugDSPParams;
extern bool g_DisableEngineExhaustDSP;
extern bool g_DebugVehicleOpenness;
extern f32 g_NPCRoadNoiseOuterAngle;
extern f32 g_NPCRoadNoiseInnerAngle;
extern f32 g_TransWobbleFreq;
extern f32 g_TransWobbleFreqVariance;
extern f32 g_TransWobbleMag;
extern f32 g_TransWobbleMagVariance;
extern f32 g_TransWobbleLength;
extern f32 g_TransWobbleLengthVariance;
BANK_ONLY(extern bool g_ForceTransWobble;)
BANK_ONLY(extern bool g_ShowDoorRatios;)

#if NA_RADIO_ENABLED
extern bank_float g_LoudVehicleRadioOffset;
#endif
extern f32 g_LowerVehDynamicImpactThreshold;
extern f32 g_UpperVehDynamicImpactThreshold;
extern bool g_GranularEnabled;
extern f32 g_GranularAccelDecelSmoothRate;

f32 audCarAudioEntity::sm_TimeToApplyHandbrake = 2.0f; // time in s until the fake handbrake is applied

audSoundSet audCarAudioEntity::sm_DamageSoundSet;

// m2
f32 g_DummyRadii[NUM_VEHICLEVOLUMECATEGORY] =
{
	50.f * 50.f, //VEHICLE_VOLUME_VERY_LOUD,
	45.f * 45.f, //VEHICLE_VOLUME_LOUD,
	43.f * 43.f, //VEHICLE_VOLUME_NORMAL,
	40.f * 40.f, //VEHICLE_VOLUME_QUIET,
	37.f * 37.f, //VEHICLE_VOLUME_VERY_QUIET,
};

audCurve g_RelSpeedToCarByVol;
audCurve g_DistanceToCarByVol;

bool g_ShowEngineExhaust = false;

f32 g_BrakeLinearVolumeSmootherRate = 0.5f;

extern f32 g_BonnetCamGTOffset;
extern f32 g_TurboSpinUp;
extern f32 g_TurboSpinDown;
extern f32 g_DumpValvePressureThreshold;

// in dBs, the volume range of the throttle (vol = 0 - (1-throttle) * range))
const f32 g_audThrottleVolumeRange = 0.f;
f32 g_audExhaustThrottleVolumeRange = 1.f;

// gear boxes wont crunch unless damage is above this
f32 g_audVehicleDamageFactoGearBoxCrunch = 0.4f;
// this probability will be scaled by damage factor
f32 g_audVehicleGearBoxCrunchProb = 0.65f;

f32 g_audEngineSteamSoundVolRange = -2.5f;

extern f32 g_ExhaustProximityEffectRange;

f32 g_MinCarRollSpeed = 1.0f;
f32 g_MaxCarRollSpeed = 15.0f;
f32 g_MinCarRollImpulseMag = 0.1f;
f32 g_MaxCarRollImpulseMag = 1.0;

const f32 g_EnginePowerBand[11] = 
{
	2.f, // reverse, only ever gets to 50%
	0.95f, // full gear for 1st and 2nd
	0.9f,
	0.7f,
	0.60f,
	0.6f,
	0.6f,
	0.6f,
	0.6f,
	0.6f,
	0.6f
};

CompileTimeAssert((MAX_NUM_GEARS + 1) == sizeof(g_EnginePowerBand)/sizeof(g_EnginePowerBand[0]));

audCategory *g_PlayerVolumeCategories[5];
audCategory *g_PedVolumeCategories[5];
audCategory *g_EngineCategory, *g_EngineLoudCategory, *g_EngineDamageCategory;
audCategory *g_IgnitionCategory;

audCurve g_EnginePowerToDistortion;

#if __BANK
bool g_TreatAsPedCar = false;
bool g_DebugWindNoise = false;
bool g_TreatAsDummyCar = false;
bool g_DisableVolumeCategory = false;
audSound *g_EngineReferenceLoop = NULL;
bool g_MuteSuspension = false;
bool g_ForceNPCHydraulicSounds = false;
bool g_DebugHydraulicSounds = false;
bool g_DisableClatter = false;
bool g_DisplayRidgedSurfaceInfo = false;

f32 g_OverriddenThrottle = 0.f;
f32 g_OverriddenRevs = 0.f;
bool g_OverrideSim = false;
bool g_RenderVehicleSlots = false;
bool g_BlipPlayerThrottle = false;
bool g_DisableRoadNoise = false;
bool g_DisableNPCRoadNoise = false;
bool g_ShowNPCRoadNoiseState = false;
bool g_AutoReapplyDSPPresets = false;
bool g_DisplayDirtGlassInfo = false;
bool g_DrawLinesToRealCars = false;
bool g_DrawCarsPlayingSportsRevs = false;
bool g_DrawSportsRevsPassbyDebug = false;
bool g_ForceAllCarsPlaySportsRevs = false;
bool g_DrawLinesToVehiclesWithWaveSlots = false;
bool g_DrawLinesToVehiclesWithSubmixVoices = false;
bool g_ShowEngineStates = false;
bool g_DrawSuperDummyRadius = false;
bool g_ShowFocusVehicle = false;

extern bool g_ForceGlassySurface;
extern bool g_ForceDirtySurface;
extern f32 g_WheelDirtinessIncreaseSmoothRate;
extern f32 g_WheelDirtinessDecreaseSmoothRate;
extern f32 g_WheelGlassinessIncreaseSmoothRate;
extern f32 g_WheelGlassinessDecreaseSmoothRate;
extern f32 g_GlassAreaDepletionRate;

extern char g_DrivingMaterialName[64];
extern bool g_DisplayGroundMaterial;
extern bool g_DebugVehicleSkids;
#endif

extern bool g_DisableBurnoutSkidsInReverse;
float g_CarVelForPedImpact = 0.2f;
extern float g_BodyDamageFactorForDebris;
extern float g_BodyDamageFactorForHydraulicDebris;
float g_DamageImpactVel = 10.f;
float g_TrailerBumpVel = 5.f;

extern float g_DeformationImpactVel;
  
f32 g_MissionVehicleRevsScaler = 1.3f;
bool g_ForceRandomBlips = false;
bank_float g_RandomBlipMaxSpeed = 5.0f;
bank_float g_MinSqDistForFastBlips = 225.0f;

extern f32 g_PlayerHornOffset;

extern const u32 g_TemperatureVariableHash = ATSTRINGHASH("temperature", 0x0d649ef0f);
audSound *g_ExhaustEffectFix = NULL, *g_EngineEffectFix = NULL;

static const struct tDamageLayerDescription g_EngineDamageLayers[] = { 
	
	{ATSTRINGHASH("DAMAGE_RATTLE_AND_SQUEEK", 0xF93244FE), ATSTRINGHASH("DAMAGE_CURVES_RATTLE_AND_SQUEEK_VOL", 0xE50748DC)}, //0 heavy
	{ATSTRINGHASH("DAMAGE_EXTRA_LOOP_2", 0x89E5DB76), ATSTRINGHASH("DAMAGE_CURVES_EXTRA_LOOP_2", 0xAD448DC0)}, // 1 light
	{ATSTRINGHASH("DAMAGE_EXTRA_LOOP_3", 0x7B053DB5), ATSTRINGHASH("DAMAGE_CURVES_EXTRA_LOOP_3", 0xCE9CD070)}, // 2 heavy
	{ATSTRINGHASH("DAMAGE_EXTRA_LOOP_4", 0x642F1009), ATSTRINGHASH("DAMAGE_CURVES_EXTRA_LOOP_4", 0xB4A29C70)}, // 3 light
	{ATSTRINGHASH("DAMAGE_EXTRA_LOOP_5", 0xD7AA76FE), ATSTRINGHASH("DAMAGE_CURVES_EXTRA_LOOP_5", 0xD73AE1A0)}, // 4 heavy
	{ATSTRINGHASH("DAMAGE_EXTRA_LOOP_7", 0xB340AE2B), ATSTRINGHASH("DAMAGE_CURVES_EXTRA_LOOP_7", 0xC2B73899)}, // 5 light
};

static const u32 g_LightDamageLayerIndices[] = {1,3,5};
static const u32 g_HeavyDamageLayerIndices[] = {0,2,4};
const u32 g_NumHeavyDamageLayers = (sizeof(g_HeavyDamageLayerIndices)/sizeof(u32));
const u32 g_NumLightDamageLayers = (sizeof(g_LightDamageLayerIndices)/sizeof(u32));
const u32 g_NumDamageLayers = (sizeof(g_EngineDamageLayers)/sizeof(g_EngineDamageLayers[0]));

audCurve g_EngineDamageCurves[g_NumDamageLayers];

#if __WIN32
#pragma warning(push)
#pragma warning(disable: 4355) // 'this' used in base member initializer list
#endif

audCarAudioEntity::audCarAudioEntity() : audVehicleAudioEntity()
{
	m_VehicleLOD = AUD_VEHICLE_LOD_DISABLED;
	m_VehicleType = AUD_VEHICLE_CAR;
	m_DiscBrakeSqueel = NULL;
	m_BrakeReleaseSound = NULL;
	m_ParkingBeep = NULL;
	m_SpecialFlightModeSound = NULL;
	m_StartUpRevsSound = NULL;
	m_InAirRotationSound = NULL;
	m_RevsBlipSound = NULL;
	m_NOSBoostLoop = NULL;
	m_OffRoadRumbleSound = NULL;
	m_BankSpray = NULL;
	m_WooWooSound = NULL;
	m_GliderSound = NULL;
	m_ToyCarEngineLoop = NULL;
	m_SubmarineTransformSound = NULL;
	m_WindFastSound = NULL;
	m_DamageTyreScrapeSound = NULL;
	m_DamageTyreScrapeTurnSound = NULL;
    m_HydraulicSuspensionInputLoop = NULL;
	m_HydraulicSuspensionAngleLoop = NULL;
	m_SportsCarRevsSound = NULL;
	m_OffRoadRumblePulse = NULL;
	m_AdditionalRevsSmoothing = 0.0f;

	for(u32 i = 0 ; i < g_NumDamageLayersPerCar; i++)
	{
		m_EngineDamageLayers[i] = NULL;
	}

	for(u32 i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		m_WheelGrindLoops[i] = NULL;
		m_SuspensionSounds[i] = NULL;
		m_TyreDamageLoops[i] = NULL;
		m_WheelOffRoadSounds[i] = NULL;
		m_WheelOffRoadSoundHashes[i] = g_NullSoundHash;
	}
}

#if __WIN32
#pragma warning(pop)
#endif

audCarAudioEntity::~audCarAudioEntity()
{
	ShutdownPrivate();
}

void audCarAudioEntity::Shutdown()
{
	ShutdownPrivate();
	audVehicleAudioEntity::Shutdown();
}

void audCarAudioEntity::ShutdownPrivate()
{
	if(m_IsGeneratingReflections)
	{
		g_ReflectionsAudioEntity.RemoveVehicleReflections(this);
	}
}

// -------------------------------------------------------------------------------
// audCarAudioEntity::UpdateClass
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateClass()
{
	f32 averageCarSpeed = sm_NumActiveCars > 0? sm_CarFwdSpeedSum/sm_NumActiveCars : 0.0f;
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Ambience.AverageCarSpeed", 0xB49F8979), averageCarSpeed);
	
	sm_CarFwdSpeedSum = 0.0f;
	sm_NumActiveCars = 0;

#if __BANK
	if(g_DrawVehicleLODQuadrants)
	{
		Vector3 playerPos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition()); 
		grcDebugDraw::Line(playerPos,playerPos + 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).b,Color_red);
		grcDebugDraw::Line(playerPos,playerPos - 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).b,Color_red);
		grcDebugDraw::Line(playerPos,playerPos + 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).a,Color_red);
		grcDebugDraw::Line(playerPos,playerPos - 10.f *MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).a,Color_red);
	}

	if(g_EngineVolumeCaptureActive)
	{
		UpdateVolumeCapture();
	}
	else
	{
		if(CFileMgr::IsValidFileHandle(sm_VolumeCaptureFileID))
		{
			CFileMgr::CloseFile(sm_VolumeCaptureFileID);
			sm_VolumeCaptureFileID = INVALID_FILE_HANDLE;
			sm_VolumeCaptureState = State_NextVehicle;
			sm_VolumeCapturePeakLoudness = g_SilenceVolume;
			sm_VolumeCapturePeakRMS = g_SilenceVolume;
			sm_VolumeCapturePeakAmplitude = g_SilenceVolume;
			sm_VolumeCaptureTimer = 0.0f;
			sm_VolumeCaptureFramesAtState = 0;
			sm_VolumeCaptureRevs = 0.0f;
			sm_VolumeCaptureThrottle = 0.0f;
			sm_VolumeCaptureIntervalTimer = 0.0f;
			sm_VolumeCaptureVehicleIndex = sm_VolumeCaptureVehicleStartIndex;
			sm_VolumeCaptureLoudnessHistory.Reset();
			sm_VolumeCaptureRMSHistory.Reset();
			sm_VolumeCaptureAmplitudeHistory.Reset();
			sm_VolumeCaptureVehicleRequest.Invalidate();
		}
	}
#endif
}

#if __BANK
// -------------------------------------------------------------------------------
// Helper function for sorting vehicles by name
// -------------------------------------------------------------------------------
s32 QSortVehicleModels(CVehicleModelInfo* const* infoA, CVehicleModelInfo* const* infoB)
{
	return stricmp((*infoA)->GetModelName(), (*infoB)->GetModelName());
}

// -------------------------------------------------------------------------------
// Volume capture stuff needed on the update thread
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateVolumeCaptureUpdateThread()
{
	if(!fwSceneGraph::IsSceneGraphLocked())
	{
		if(sm_VolumeCaptureVehicleRequest.IsValid())
		{
			if(g_EngineVolumeCaptureActive)
			{
				CVehicle* localVehicle = CGameWorld::FindLocalPlayerVehicle();

				if(!localVehicle)
				{
					sm_VolumeCaptureStartPos = CGameWorld::FindLocalPlayerCoors();
				}

				CVehicleFactory::WarpPlayerOutOfCar();
				FindPlayerPed()->SetPosition(sm_VolumeCaptureStartPos, true, true, true);

				CVehicle* oldVehicle = CVehicleFactory::GetCreatedVehicle();

				if(oldVehicle)
				{
					CVehicleFactory::GetFactory()->Destroy(oldVehicle);
				}

				CVehicleFactory::CreateCar(sm_VolumeCaptureVehicleRequest.GetModelIndex());
				CVehicleFactory::WarpPlayerIntoRecentCar();
			}

			sm_VolumeCaptureVehicleRequest.Invalidate();
		}
	}
}

// -------------------------------------------------------------------------------
// Update the volume capture system
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateVolumeCapture()
{
	static const f32 captureInterval = 0.1f;
	static const f32 captureRevTime = 7.0f;
	static const f32 captureSwitchTime = 12.0f;

	if(audVerifyf(audDriver::GetMixer()->GetLoudnessMeter() != NULL, "Volume capture requires -audiodesigner"))
	{
		CVehicle* localPlayerVeh = CGameWorld::FindLocalPlayerVehicle();
		const char* vehicleName = NULL;

		VolumeCaptureState initialState = sm_VolumeCaptureState;
		char tempString[256];
		float xCoord = 0.1f;
		float yCoord = 0.2f;

		formatf(tempString, "Engine Volume Capture Active");
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;
		
		if(localPlayerVeh)
		{
			CarAudioSettings* currentCarSettings = ((audCarAudioEntity*)localPlayerVeh->GetVehicleAudioEntity())->GetCarAudioSettings();
			vehicleName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(currentCarSettings->NameTableOffset);
			formatf(tempString, "Current Vehicle: %s", vehicleName);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		}
		
		yCoord += 0.02f;

		if(localPlayerVeh)
		{
			formatf(tempString, "Type: %s", ((audCarAudioEntity*)localPlayerVeh->GetVehicleAudioEntity())->IsUsingGranularEngine()? "Granular" : "Non-Granular");
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		}
		
		yCoord += 0.02f;

		switch(sm_VolumeCaptureState)
		{
		case State_NextVehicle:
			{
				formatf(tempString, "State: Selecting Next Vehicle");
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;

				if(sm_VolumeCaptureFramesAtState == 0)
				{
					if(!CFileMgr::IsValidFileHandle(sm_VolumeCaptureFileID))
					{							
						// Try and open a file for the results
						CFileMgr::SetDir("common:/DATA");
						sm_VolumeCaptureFileID = CFileMgr::OpenFileForWriting("VehicleVolumeCapture.csv");
						CFileMgr::SetDir("");

						if(!audVerifyf(CFileMgr::IsValidFileHandle(sm_VolumeCaptureFileID), "Failed to open VehicleVolumeCapture.csv"))
						{
							g_EngineVolumeCaptureActive = false;
							return;
						}
						else
						{
							char fileOutput[255];
							formatf(fileOutput, "%s,%s,%s, ,%s\n", "Vehicle", "Metric", "Peak", "Data");
							CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));
							audDriver::GetMixer()->GetMasterSubmix()->SetFlag(audMixerSubmix::ComputePeakAmplitude, true);
							sm_VolumeCaptureVehicleIndex = sm_VolumeCaptureVehicleStartIndex;
						}
					}

					atArray<CVehicleModelInfo*> vehicleTypeArray;
					fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
					vehModelInfoStore.GatherPtrs(vehicleTypeArray);
					vehicleTypeArray.QSort(0, -1, QSortVehicleModels);
					u32 numVehicleNames = vehicleTypeArray.GetCount();
					const char* vehicleName = NULL;

					while(sm_VolumeCaptureVehicleIndex < numVehicleNames)
					{
						vehicleName = vehicleTypeArray[sm_VolumeCaptureVehicleIndex]->GetModelName();
						VehicleType vehicleType = vehicleTypeArray[sm_VolumeCaptureVehicleIndex]->GetVehicleType();
						bool isCar = (vehicleType == VEHICLE_TYPE_CAR || vehicleType == VEHICLE_TYPE_SUBMARINECAR || vehicleType == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || vehicleType == VEHICLE_TYPE_BIKE || vehicleType == VEHICLE_TYPE_QUADBIKE || vehicleType == VEHICLE_TYPE_DRAFT || vehicleType == VEHICLE_TYPE_SUBMARINECAR || vehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);

						// We've found a valid car, so use this
						if(isCar && audNorthAudioEngine::GetObject<CarAudioSettings>(vehicleName) != NULL)
						{
							fwModelId modelID = CModelInfo::GetModelIdFromName(vehicleName);

							if(audVerifyf(modelID.IsValid(), "Invalid modelID!"))
							{
								sm_VolumeCaptureVehicleRequest = modelID;
							}
							else
							{
								g_EngineVolumeCaptureActive = false;
								return;
							}
							
							break;
						}
						// Not a valid car, keep searching
						else
						{
							sm_VolumeCaptureVehicleIndex++;
						}
					}

					if(sm_VolumeCaptureVehicleIndex >= numVehicleNames)
					{
						// Fin
						g_EngineVolumeCaptureActive = false;
						return;
					}
				}

				camInterface::GetGameplayDirector().GetThirdPersonCamera()->GetControlHelper()->SetLookBehindState(true);
				camInterface::GetGameplayDirector().GetThirdPersonCamera()->GetControlHelper()->IgnoreLookBehindInputThisUpdate();

				// Give the car time to load the new bank etc. - signalled by the request becoming invalid again
				if(sm_VolumeCaptureTimer > 1.0f && localPlayerVeh && !sm_VolumeCaptureVehicleRequest.IsValid())
				{
					sm_VolumeCaptureState = State_CapturingAccel;
					sm_VolumeCapturePeakLoudness = g_SilenceVolume;
					sm_VolumeCapturePeakRMS = g_SilenceVolume;
					sm_VolumeCapturePeakAmplitude = g_SilenceVolume;
					sm_VolumeCaptureTimer = 0.0f;
					sm_VolumeCaptureFramesAtState = 0;
					sm_VolumeCaptureRevs = 0.0f;
					sm_VolumeCaptureThrottle = 0.0f;
					sm_VolumeCaptureIntervalTimer = 0.0f;
					sm_VolumeCaptureLoudnessHistory.Reset();
					sm_VolumeCaptureRMSHistory.Reset();
					sm_VolumeCaptureAmplitudeHistory.Reset();
				}
			}
			break;

		case State_CapturingAccel:
		case State_CapturingAccelModded:
			{
				formatf(tempString, "State: Capturing %s", sm_VolumeCaptureState == State_CapturingAccel? "Accel" : "Modded Accel");
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;

				f32 desiredRevs = sm_VolumeCaptureTimer/captureRevTime;

				if(desiredRevs > 1.1f)
				{
					sm_VolumeCaptureRevs = 0.0f;
					sm_VolumeCaptureThrottle = 0.0f;
				}
				else
				{
					sm_VolumeCaptureRevs = desiredRevs;
					sm_VolumeCaptureThrottle = 1.0f;
				}

				formatf(tempString, "Rev Ratio: %f", sm_VolumeCaptureRevs);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;

				formatf(tempString, "Throttle: %f", sm_VolumeCaptureThrottle);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;

				f32 peakAmplitude = audDriverUtil::ComputeDbVolumeFromLinear(Max(audDriver::GetMixer()->GetMasterSubmix()->GetAndResetPeakAmplitude(0), audDriver::GetMixer()->GetMasterSubmix()->GetAndResetPeakAmplitude(1)));
				f32 loudness = audDriver::GetMixer()->GetLoudnessMeter()->GetLoudness();
				f32 rms = audDriverUtil::ComputeDbVolumeFromLinear(audDriver::GetMixer()->GetMasterSubmix()->GetRMS());

				if(sm_VolumeCaptureIntervalTimer > captureInterval)
				{
					sm_VolumeCaptureLoudnessHistory.PushAndGrow(loudness);
					sm_VolumeCaptureRMSHistory.PushAndGrow(rms);
					sm_VolumeCaptureAmplitudeHistory.PushAndGrow(peakAmplitude);

					sm_VolumeCapturePeakLoudness = Max(loudness, sm_VolumeCapturePeakLoudness);
					sm_VolumeCapturePeakRMS = Max(rms, sm_VolumeCapturePeakRMS);
					sm_VolumeCapturePeakAmplitude = Max(peakAmplitude, sm_VolumeCapturePeakAmplitude);
					sm_VolumeCaptureIntervalTimer = 0.0f;
				}

				static const char *meterNames[] = {"Loudness", "RMS", "Amplitude", "Peak Loudness", "Peak RMS", "Peak Amplitude"};
				const u32 numMeters = sizeof(meterNames)/sizeof(meterNames[0]);
				static audMeterList meterList;
				static f32 meterValues[numMeters];

				s32 i = 0;
				meterValues[i++] = audDriverUtil::ComputeLinearVolumeFromDb(loudness);
				meterValues[i++] = rms;
				meterValues[i++] = peakAmplitude;
				meterValues[i++] = audDriverUtil::ComputeLinearVolumeFromDb(sm_VolumeCapturePeakLoudness);
				meterValues[i++] = sm_VolumeCapturePeakRMS;
				meterValues[i++] = sm_VolumeCapturePeakAmplitude;

				meterList.left = 128.0f;
				meterList.bottom = 620.f;
				meterList.width = 400.f;
				meterList.height = 260.f;
				meterList.values = &meterValues[0];
				meterList.names = meterNames;
				meterList.numValues = numMeters;
				audNorthAudioEngine::DrawLevelMeters(&meterList);

				if(sm_VolumeCaptureTimer > captureSwitchTime)
				{
					// Print our data to the csv file
					char fileOutput[255];
					u32 numOutputItems = (u32)(captureRevTime/captureInterval);

					// Print loudness
					formatf(fileOutput, "%s%s,Loudness,%.02f, ,", vehicleName, sm_VolumeCaptureState == State_CapturingAccelModded? "_MODDED" : "", sm_VolumeCapturePeakLoudness);
					CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));

					for(u32 loop = 0; loop < numOutputItems && loop < sm_VolumeCaptureLoudnessHistory.GetCount(); loop++)
					{
						formatf(fileOutput, "%.02f,", sm_VolumeCaptureLoudnessHistory[loop]);
						CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));	
					}

					formatf(fileOutput, "\n");
					CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));	

					// Print RMS
					formatf(fileOutput, ",RMS,%.02f, ,", sm_VolumeCapturePeakRMS);
					CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));

					for(u32 loop = 0; loop < numOutputItems && loop < sm_VolumeCaptureRMSHistory.GetCount(); loop++)
					{
						formatf(fileOutput, "%.02f,", sm_VolumeCaptureRMSHistory[loop]);
						CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));	
					}

					formatf(fileOutput, "\n");
					CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));	
					
					// Print Amplitude
					formatf(fileOutput, ",Amplitude,%.02f, ,", sm_VolumeCapturePeakAmplitude);
					CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));

					for(u32 loop = 0; loop < numOutputItems && loop < sm_VolumeCaptureAmplitudeHistory.GetCount(); loop++)
					{
						formatf(fileOutput, "%.02f,", sm_VolumeCaptureAmplitudeHistory[loop]);
						CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));	
					}

					formatf(fileOutput, "\n\n");
					CFileMgr::Write(sm_VolumeCaptureFileID, &fileOutput[0], istrlen(fileOutput));	

					sm_VolumeCaptureState = sm_VolumeCaptureState == State_CapturingAccel? State_CapturingAccelModded : State_NextVehicle;
					sm_VolumeCapturePeakLoudness = g_SilenceVolume;
					sm_VolumeCapturePeakRMS = g_SilenceVolume;
					sm_VolumeCapturePeakAmplitude = g_SilenceVolume;
					sm_VolumeCaptureTimer = 0.0f;
					sm_VolumeCaptureFramesAtState = 0;
					sm_VolumeCaptureRevs = 0.0f;
					sm_VolumeCaptureThrottle = 0.0f;
					sm_VolumeCaptureIntervalTimer = 0.0f;
					sm_VolumeCaptureLoudnessHistory.Reset();
					sm_VolumeCaptureRMSHistory.Reset();
					sm_VolumeCaptureAmplitudeHistory.Reset();
					
					if(sm_VolumeCaptureState == State_NextVehicle)
					{
						sm_VolumeCaptureVehicleIndex++;
					}
				}
			}
			break;

		default:
			break;
		}

		// Don't update these if we've just switched state
		if(initialState == sm_VolumeCaptureState)
		{
			sm_VolumeCaptureFramesAtState++;
			sm_VolumeCaptureTimer += fwTimer::GetTimeStep();
			sm_VolumeCaptureIntervalTimer += fwTimer::GetTimeStep();
		}
	}
	else
	{
		g_EngineVolumeCaptureActive = false;
	}
}
#endif

// -------------------------------------------------------------------------------
// audCarAudioEntity::InitClass
// -------------------------------------------------------------------------------
void audCarAudioEntity::InitClass()
{
	audVehicleConvertibleRoof::InitClass();
	audVehicleTowTruckArm::InitClass();

	g_PedVolumeCategories[0] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PED_VEHICLES_VERY_LOUD", 0xD59675EA));
	g_PedVolumeCategories[1] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PED_VEHICLES_LOUD", 0xEA185B02));
	g_PedVolumeCategories[2] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PED_VEHICLES_NORMAL", 0xAC3F546D));
	g_PedVolumeCategories[3] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PED_VEHICLES_QUIET", 0x41ACDDFF));
	g_PedVolumeCategories[4] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PED_VEHICLES_VERY_QUIET", 0xC8B2D787));

	g_PlayerVolumeCategories[0] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PLAYER_VEHICLES_VERY_LOUD", 0x3AA35DEF));
	g_PlayerVolumeCategories[1] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PLAYER_VEHICLES_LOUD", 0xDCA3CC61));
	g_PlayerVolumeCategories[2] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PLAYER_VEHICLES_NORMAL", 0x1F418253));
	g_PlayerVolumeCategories[3] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PLAYER_VEHICLES_QUIET", 0x933E72C3));
	g_PlayerVolumeCategories[4] = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PLAYER_VEHICLES_VERY_QUIET", 0xF05AAEDD));

	g_EngineCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("VEHICLES_ENGINES", 0xF7C35252));
	g_EngineLoudCategory = g_EngineCategory; //g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("CARS_ENGINES_LOUD", 0x7366FD5F));
	g_EngineDamageCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("VEHICLES_ENGINES_DAMAGE", 0x19B73388));
	g_IgnitionCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("VEHICLES_ENGINES_IGNITION", 0x1365BE95));

	if(!sm_BrakeToBrakeDiscVolumeCurve.Init(ATSTRINGHASH("BRAKE_TO_BRAKE_DISC_VOLUME", 0xF33BF170)))
	{
		naWarningf("Failed to initialise BRAKE_TO_BRAKE_DISC_VOLUME");
	}
	if(!sm_SpeedToBrakeDiscVolumeCurve.Init(ATSTRINGHASH("SPEED_TO_BRAKE_DISC_VOLUME", 0x67A7E038)))
	{
		naWarningf("Failed to initialise SPEED_TO_BRAKE_DISC_VOLUME");
	}
	if(!sm_BrakeHoldTimeToStartOffsetCurve.Init(ATSTRINGHASH("BRAKE_HOLD_TIME_TO_START_OFFSET", 0xBCBD3464)))
	{
		naWarningf("Failed to initialise BRAKE_HOLD_TIME_TO_START_OFFSET");
	}
	if(!g_EnginePowerToDistortion.Init(ATSTRINGHASH("ENGINE_POWER_TO_DISTORTION", 0x8613F6AD)))
	{
		naWarningf("Failed to initialise ENGINE_POWER_TO_DISTORTION");
	}

	StaticConditionalWarning(g_IdleVolCurve.Init(ATSTRINGHASH("IDLE_VOLUME", 0xECEC42E2)), "Invalid IdleVolumeCurve");
	StaticConditionalWarning(g_IdlePitchCurve.Init(ATSTRINGHASH("LINEAR_RISE", 0xD0E6F19)), "Invalid IdlePitchCurve");
	StaticConditionalWarning(g_EngineLowVolCurve.Init(ATSTRINGHASH("ENGINE_LOW_VOLUME", 0x26BEE93D)), "Invalid LowEngineVolCurve");
	StaticConditionalWarning(g_EngineHighVolCurve.Init(ATSTRINGHASH("ENGINE_HIGH_VOLUME", 0x5D56EA9B)), "Invalid HighEngineVolCurve");
	StaticConditionalWarning(g_ExhaustLowVolCurve.Init(ATSTRINGHASH("EXHAUST_LOW_VOLUME", 0xF4EC46B2)), "Invalid LowExhaustVolCurve");
	StaticConditionalWarning(g_ExhaustHighVolCurve.Init(ATSTRINGHASH("EXHAUST_HIGH_VOLUME", 0x17DE7D8F)), "Invalid HighExhaustVolCurve");
	StaticConditionalWarning(g_EngineExhaustPitchCurve.Init(ATSTRINGHASH("ENGINE_PITCH_CURVE", 0x8E5FECAE)), "Invalid EnginePitchCurve");
	StaticConditionalWarning(sm_EnvironmentalLoudnessThrottleCurve.Init(ATSTRINGHASH("ENVIRONMENTAL_LOUDNESS_THROTTLE_CURVE", 0x544E1E65)), "EnvironmentalLoudnessThrottleCurve");
	StaticConditionalWarning(sm_EnvironmentalLoudnessRevsCurve.Init(ATSTRINGHASH("ENVIRONMENTAL_LOUDNESS_REVS_CURVE", 0x3E65DB4A)), "Invalid EnvironmentalLoudnessRevsCurve");
	StaticConditionalWarning(sm_SuspensionVolCurve.Init(ATSTRINGHASH("SUSPENSION_COMPRESSION_TO_VOL", 0x2BEC9A45)), "Invalid SuspVolCurve");
	StaticConditionalWarning(sm_SkidFwdSlipToScrapeVol.Init(ATSTRINGHASH("SKID_CURVES_FWD_SLIP_TO_SCRAPE_VOL", 0xB44AEB94)), "Invalid fwd slip to scrape");
	StaticConditionalWarning(sm_LateralSlipToSideVol.Init(ATSTRINGHASH("SKID_CURVES_LATERAL_SLIP_TO_SIDE_VOL", 0xC2308CC3)), "invalid lateral slip to side");
	StaticConditionalWarning(g_PlayerThrottleCurve.Init(ATSTRINGHASH("PLAYER_THROTTLE_CURVE", 0x2E2C2E62)), "Invalid player_throttle_curve");
	StaticConditionalWarning(g_PlayerRevsCurve.Init(ATSTRINGHASH("PLAYER_REVS_CURVE", 0xDDAF5B01)), "Invalid player_revs_curve");
	StaticConditionalWarning(g_PedThrottleCurve.Init(ATSTRINGHASH("PED_THROTTLE_CURVE", 0xEAE4078C)), "Invalid ped throttle_curve");
	StaticConditionalWarning(g_PedRevsCurve.Init(ATSTRINGHASH("PED_REVS_CURVE", 0xEF094EBA)), "Invalid ped_revs_curve");
	StaticConditionalWarning(g_RevsSmoothingCurve.Init(ATSTRINGHASH("REVS_SMOOTHING_RATE", 0x1B4E3DB0)), "invalid revs smoothing rate curve");
	StaticConditionalWarning(g_RelSpeedToCarByVol.Init(ATSTRINGHASH("REL_SPEED_TO_CARBY_VOL", 0xE6D1B0E7)), "Invalid rel speed to carby vol");
	StaticConditionalWarning(g_DistanceToCarByVol.Init(ATSTRINGHASH("DISTANCE_TO_CARBY_VOL", 0x3A34A988)), "Invalid distance to carby vol");
	StaticConditionalWarning(g_PlayerAngVelToCarPass.Init(ATSTRINGHASH("PLAYER_ANG_VEL_TO_CAR_PASS_VOL", 0x427467E0)), "Invalid angvel to carby vol");

	for(u32 i = 0; i < g_NumDamageLayers; i++)
	{
		g_EngineDamageCurves[i].Init(g_EngineDamageLayers[i].volCurveName);
	}

	for(u32 i = 0; i < kSportsCarRevsHistorySize; i++)
	{
		sm_SportsCarRevsSoundHistory[i].soundName = 0u;
		sm_SportsCarRevsSoundHistory[i].time = 0u;
	}

	StaticConditionalWarning(sm_DamageSoundSet.Init(ATSTRINGHASH("VehicleDamage", 0x25E4D820)), "Failed to find VehicleDamage sound set");
	sm_PlayerWindNoiseSpeedSmoother.Init(1.0f/g_WindNoiseSmootherIncreaseRate, 1.0f/g_WindNoiseSmootherDecreaseRate);
	sm_SportsCarRevsSettings = audNorthAudioEngine::GetObject<SportsCarRevsSettings>(ATSTRINGHASH("SPORTS_CAR_REVS_SETTINGS_DEFAULT", 0xF4E580EC));

#if __BANK
	sm_VolumeCaptureVehicleRequest.Invalidate();
#endif
}

void audCarAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();
	m_LastHydraulicBounceTime = 0u;
	m_RevsLastFrame = 0.0f;
    m_HydraulicBounceTimer = 0.0f;
	m_SportsCarRevsRequested = false;
	m_SportsCarRevsEnabled = false;
	m_FakeEngineStartupRevs = false;
	m_HydraulicAngleLoopRetriggerValid = true;
	m_IsInAmphibiousBoatMode = false;
	m_LeanRatio = 0.0f;
	m_LastSportsCarRevsTime = 0;
	m_NextDamageThrottleCutTime = 0;
	m_ConsecutiveShortThrottleCuts = 0;
	m_DamageThrottleCutTimer = 0.0f;
	m_MaxSpeedPitchFactor = 0.0f;
	m_MaxSpeedRevsAddition = 0.0f;
	m_ScriptStartupRevsSoundRef = g_NullSoundRef;
	m_DummyLODValid = false;
	m_HasAutoShutOffEngine = false;
	m_IsGeneratingReflections = false;
	m_AutoShutOffTimer = 0.0f;
	m_SportsCarRevsApplyFactor = 0.0f;
	m_IsReversing = false;
	m_WasUpsideDownInAir = false;
	m_HasPlayedInitialJumpStress = false;
	m_HasPlayedInAirJumpStress = true;
	m_HasInitialisedVolumeCones = false;
	m_WasWaitingAtLights = false;
	m_WasDoingBurnout = false;
	m_CarAudioSettings = NULL;
	m_AmphibiousBoatSettings = NULL;
	m_VehicleEngineSettings = NULL;
	m_GranularEngineSettings = NULL;
	m_ElectricEngineSettings = NULL;
	m_DistanceToPlayerLastFrame = 0.f;
	m_DistanceToPlayerDeltaLastFrame = 0.f;
	m_WasInCarByRange = false;
	m_LastCarByTime = 0;
	m_WasSportsCarRevsCancelled = false;
	m_JumpThrottleCutTimer = 0.0f;
	m_OutOfWaterRevsMultiplier = 1.0f;
	m_FakeRevsSmoother.Init(0.0001f, 0.0001f, 0.0f, 1.0f);
	m_RevsSmoother.Init(g_RevMaxIncreaseRate / 1000.f,g_RevMaxDecreaseRate / 1000.f,true);
	m_TyreDamageGrindRatioSmoother.Init(0.1f, 0.05f);
	m_ThrottleSmoother.Init(0.005f,0.005f,-1.0f,1.0f);
	m_RollOffSmoother.Init(1.0f, 0.2f);
	m_BrakeLinearVolumeSmoother.Init(g_BrakeLinearVolumeSmootherRate / 1000.0f, 0.01f);
	m_WasBrakeHeldLastFrame = false;
	m_WasStationaryLastFrame = true;
	m_StartupRevsVolBoostSmoother.Reset();
	m_ReversingExhaustVolSmoother.Init(1.0f);
	m_StartupRevsVolBoostSmoother.Init(0.01f, 0.005f);
	m_NextDoorTime = 0;
	m_LastTimeStationary = 0;
	m_TimeAtCurrentLOD = 1.0f;
	m_IsIndicatorOn = false;
	m_AreExhaustPopsEnabled = true;
	m_HasPlayedStartupSequence = false;
	m_WasPlayingStartupSound = false;
	m_WaterTurbulenceOutOfWaterTimer = 0.0f;
	m_WaterTurbulenceInWaterTimer = 0.0f;
	m_PrevOffRoadRumbleHash = g_NullSoundHash;
	m_EngineDamageFrequencyRatioFluctuator.Init(0.001f, 0.001f, 0.88f, 0.89f, 1.0f, 1.0f, 0.05f, 0.005f, 1.f);
	m_SportsCarRevsTriggerType = REVS_TYPE_MAX;

	for(u32 i = 0 ; i < g_NumDamageLayersPerCar; i++)
	{
		m_EngineDamageLayerIndex[i] = -1;
	}

	for(u32 i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		m_LastCompressionChange[i] = 0.f;
		m_LastBottomOutTime[i] = 0;
	}	

	m_IsTyreCompletelyDead.Reset();
	m_HasBlippedSinceStopping = false;
	m_ForceThrottleBlip = false;
	m_ThrottleBlipTimer = 0;
	m_FakeOutOfWater = false;
	m_FakeOutOfWaterTimer = 0.0f;
	m_FakeRevsHoldTime = 0.0f;
	m_WasHighFakeRevs = false;
	m_TimeAlarmLastArmed = 0;
#if GTA_REPLAY
	m_ReplayIsInWater = false;
	m_ReplayVehiclePhysicsInWater = false;
#endif
}

CarAudioSettings *audCarAudioEntity::GetCarAudioSettings()
{
	bool isSupportedVehicleType = false;
	
	if(m_Vehicle)
	{
		if (m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR || 
			m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || 
			m_Vehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || 
			m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || 
			m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE || 
			m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ||
			m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)
		{
			isSupportedVehicleType = true;
		}

		audAssertf(isSupportedVehicleType, "Unsupported car vehicle type - %d", m_Vehicle->GetVehicleType());
	}

	if(!g_AudioEngine.IsAudioEnabled() || !m_Vehicle || !isSupportedVehicleType)
	{
#if __BANK
	if(!g_AuditionWheelFire)
	{
		return NULL;
	}
#else
		return NULL;
#endif
	}

	if(m_CarAudioSettings) //If we've already set this don't do it again
	{
		return m_CarAudioSettings;
	}

	CarAudioSettings *settings = NULL;

	if(m_ForcedGameObject != 0u)
	{
		settings = audNorthAudioEngine::GetObject<CarAudioSettings>(m_ForcedGameObject);
		m_ForcedGameObject = 0u;
	}

	if(!settings)
	{
		if(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
		{
			settings = audNorthAudioEngine::GetObject<CarAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
		}
		
		if(!settings)
		{
			settings = audNorthAudioEngine::GetObject<CarAudioSettings>(GetVehicleModelNameHash());
		}
	}
	
	if(CSystem::IsThisThreadId(SYS_THREAD_AUDIO))
	{
		audAssertf(settings != NULL, "Couldn't find car audio settings for %s - this vehicle will probably be silent!", GetVehicleModelName());
	}

	if( settings )
	{
		m_VehicleEngineSettings = audNorthAudioEngine::GetObject<VehicleEngineAudioSettings>(settings->Engine);
		m_ElectricEngineSettings = audNorthAudioEngine::GetObject<ElectricEngineAudioSettings>(settings->ElectricEngine);

		// Its valid for a bicycle to not have an engine, so don't force one if there isn't one set in RAVE
		if(!m_VehicleEngineSettings && !m_ElectricEngineSettings && m_Vehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
		{
			if (CSystem::IsThisThreadId(SYS_THREAD_AUDIO))
				naDebugf1("Couldn't find engine audio settings for %s", GetVehicleModelName());
		}

		GranularEngineAudioSettings* primaryEngine = audNorthAudioEngine::GetObject<GranularEngineAudioSettings>(settings->GranularEngine);
		GranularEngineSet* alternativeEngines = audNorthAudioEngine::GetObject<GranularEngineSet>(settings->AlternativeGranularEngines);
		m_GranularEngineSettings = primaryEngine;

		if(alternativeEngines && alternativeEngines->numGranularEngines > 0 && (!primaryEngine || audEngineUtil::ResolveProbability(settings->AlternativeGranularEngineProbability)))
		{
			f32 totalWeight = 0.0f;

			for(u32 loop = 0; loop < alternativeEngines->numGranularEngines; loop++)
			{
				totalWeight += alternativeEngines->GranularEngines[loop].Weight;
			}

			f32 randomWeight = audEngineUtil::GetRandomNumberInRange(0.f, totalWeight);

			for(u32 loop = 0; loop < alternativeEngines->numGranularEngines; loop++)
			{
				randomWeight -= alternativeEngines->GranularEngines[loop].Weight;

				if(randomWeight <= 0.f)
				{
					m_GranularEngineSettings = audNorthAudioEngine::GetObject<GranularEngineAudioSettings>(alternativeEngines->GranularEngines[loop].GranularEngine);
					break;
				}
			}
		}

#if __BANK
		if(g_ForceGranularNPCEngines && (!m_GranularEngineSettings || (!m_IsPlayerVehicle && m_GranularEngineSettings->NPCEngineAccel == g_NullSoundHash)))
		{
			const u32 defaultGranularEngineNames[] = {ATSTRINGHASH("BANSHEE_GRANULAR_ENGINE", 0xCC012EBD), ATSTRINGHASH("PENUMBRA_GRANULAR_ENGINE", 0xE9A310B0), ATSTRINGHASH("SULTAN_GRANULAR_ENGINE", 0xC8FA3AB8), ATSTRINGHASH("CHEETAH_GRANULAR_ENGINE", 0x952D6F23)};
			m_GranularEngineSettings = audNorthAudioEngine::GetObject<GranularEngineAudioSettings>(defaultGranularEngineNames[audEngineUtil::GetRandomNumberInRange(0, NELEM(defaultGranularEngineNames) - 1)]);
		}
#endif

		// If we're not running an audio designer build, we must obey the publish flag
		if(!PARAM_audiodesigner.Get())
		{
			if(m_GranularEngineSettings)
			{
				if(AUD_GET_TRISTATE_VALUE(m_GranularEngineSettings->Flags, FLAG_ID_GRANULARENGINEAUDIOSETTINGS_PUBLISH) != AUD_TRISTATE_TRUE)
				{
					m_GranularEngineSettings = NULL;
				}
			}
		}
	}

	if(settings)
	{
		// Turn off auto-shut-off engines for mission vehicles, just incase it messes with any vehicle recordings etc
		if(!m_Vehicle->PopTypeIsMission())
		{
			m_HasAutoShutOffEngine = audEngineUtil::ResolveProbability(settings->StopStartProb/100.0f);
		}

		BANK_ONLY(m_HasAutoShutOffEngine |= g_ForceAutoShutoffEngines;)

		m_HasRandomWheelDamage = GetRandomDamageClass() == RANDOM_DAMAGE_ALWAYS && ENTITY_SEED_PROB(GetVehicle()->GetRandomSeed(), 0.1f);
		m_VehicleModelSoundHash = settings->CarModel;
		m_VehicleMakeSoundHash = settings->CarMake;
		m_VehicleCategorySoundHash = settings->CarCategory;
		m_ScannerVehicleSettingsHash = settings->ScannerVehicleSettings;
		m_SportsCarRevsEnabled = (AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_CARAUDIOSETTINGS_SPORTSCARREVSENABLED) == AUD_TRISTATE_TRUE);
		
#if NA_RADIO_ENABLED
		m_AmbientRadioDisabled = (AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_CARAUDIOSETTINGS_DISABLEAMBIENTRADIO) == AUD_TRISTATE_TRUE);
		m_RadioType = (RadioType)settings->RadioType;
		m_RadioGenre = (RadioGenre)settings->RadioGenre;
		m_RadioLeakage = (AmbientRadioLeakage)settings->RadioLeakage;

		// only 30% of cars marked up as bassy loud get bassy loud
		if(m_RadioLeakage == LEAKAGE_BASSY_LOUD)
		{
			if(ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), 0.7f))
			{
				m_RadioLeakage = LEAKAGE_BASSY_MEDIUM;
			}
		}
#endif

		m_GpsType = settings->GpsType;
		m_GpsVoice = settings->GpsVoice;
		
		if(m_VehicleEngineSettings)
		{
			m_Vehicle->m_nVehicleFlags.bHasCoolingFan = (m_VehicleEngineSettings->CoolingFan!=g_NullSoundHash);
		}
		m_VehSirenSounds.Init(settings->SirenSounds);

		// HL - Hacky fix for new CG Heists pack vehicles, due to smoothing not
		// being configurable on a per-vehicle basis
		switch(GetVehicleModelNameHash())
		{
		case 0x825A9F4C: //GUARDIAN
		case 0x83051506: //TECHNICAL
		case 0x50D4D19F: //TECHNICAL3
			m_AdditionalRevsSmoothing = 1.0f;
			break;
		case 0x9114EADA: //INSURGENT
		case 0x7B7E56F0: //INSURGENT2
		case 0x8D4B7A8A: //INSURGENT3
			m_AdditionalRevsSmoothing = 2.0f;
			break;
		case 0x11AA0E14: //GBURRITO2
			m_AdditionalRevsSmoothing = 6.0f;
			break;
		default:
			m_AdditionalRevsSmoothing = 0.0f;
			break;
		}
		// End hacky fix
	}

	return settings;
}

bool audCarAudioEntity::AreExhaustPopsEnabled() const
{
	return m_AreExhaustPopsEnabled BANK_ONLY(&& !g_EngineVolumeCaptureActive);
}

audMetadataRef audCarAudioEntity::GetClatterSound() const
{
	if(!m_Vehicle->InheritsFromBike() && !m_Vehicle->InheritsFromQuadBike() && !m_Vehicle->InheritsFromAmphibiousQuadBike())
	{
		if(m_CarAudioSettings)
		{
			ClatterType clatterType = (ClatterType)m_CarAudioSettings->ClatterType;

			if(clatterType == AUD_CLATTER_NONE && m_IsFocusVehicle && !IsToyCar() && !IsGoKart())
			{
				clatterType = AUD_CLATTER_DETAIL;
			}

			return GetClatterSoundFromType(clatterType);
		}
	}
	
	return audVehicleAudioEntity::GetClatterSound();
}

audMetadataRef audCarAudioEntity::GetChassisStressSound() const
{
	CVehicleVariationInstance& variation = m_Vehicle->GetVariationInstance();
	if(variation.GetMods()[VMT_HORN] != INVALID_MOD)
	{
		const u32 modKitHash = variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier();

		// Certain horns enable sleigh bell 'clatter' sounds
		if(modKitHash == ATSTRINGHASH("XM15_HORN_01", 0x354642A) || modKitHash == ATSTRINGHASH("XM15_HORN_02", 0x17D78D30) || modKitHash == ATSTRINGHASH("XM15_HORN_03", 0x216DA05C) BANK_ONLY(||g_ForceSleighBellsOn))
		{
			return g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("sleighbells_chassis_stress_detail", 0x68C7D7F0));
		}
	}

	if(!m_Vehicle->InheritsFromBike() && !m_Vehicle->InheritsFromQuadBike() && !m_Vehicle->InheritsFromAmphibiousQuadBike())
	{
		if(m_CarAudioSettings)
		{
			ClatterType clatterType = (ClatterType)m_CarAudioSettings->ClatterType;

			if(clatterType == AUD_CLATTER_NONE && m_IsFocusVehicle)
			{
				clatterType = AUD_CLATTER_DETAIL;
			}

			return GetChassisStressSoundFromType(clatterType);
		}
	}

	return g_NullSoundRef;
}

f32 audCarAudioEntity::GetChassisStressSensitivityScalar() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ChassisStressSensitivityScalar;
	}

	return 1.0f;
}

f32 audCarAudioEntity::GetClatterSensitivityScalar() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ClatterSensitivityScalar;
	}

	return 1.0f;
}

f32 audCarAudioEntity::GetClatterVolumeBoost() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ClatterVolumeBoost * 0.01f;
	}

	return 0.0f;
}

f32 audCarAudioEntity::GetChassisStressVolumeBoost() const		
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ChassisStressVolumeBoost * 0.01f;
	}

	return 0.0f;
}

void audCarAudioEntity::SetupVolumeCones()
{
	Vec3V engineConeDir = Vec3V(0.f,1.0f,0.f);
	Vec3V exhaustConeDir = Vec3V(0.f,-1.0f,0.f);

	bool useExhaustCone = true;
	const Vec3V exPos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
	const Vec3V vVehiclePosition = m_Vehicle->GetTransform().GetPosition();
	Vec3V ex2centre = vVehiclePosition - exPos;
	ScalarV dp = Dot(ex2centre, m_Vehicle->GetTransform().GetB());

	if(IsTrue(IsLessThanOrEqual(dp, ScalarV(V_ZERO))))
	{
		useExhaustCone = false;
	}
	
	Vec3V temp = vVehiclePosition - m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos);
	dp = Dot(temp, m_Vehicle->GetTransform().GetB());

	if(IsTrue(IsGreaterThan(dp, ScalarV(V_ZERO))))
	{
		engineConeDir = exhaustConeDir;
	}

	if(sm_ShouldFlipPlayerCones)
	{
		engineConeDir *= Vec3V(0.0f,-1.f,0.0f);
		exhaustConeDir *= Vec3V(0.0f,-1.f,0.0f);
	}
	
	if(m_vehicleEngine.IsGranularEngineActive())
	{
		m_EngineVolumeCone.Init(engineConeDir, m_GranularEngineSettings->EngineMaxConeAttenuation/100.f, 60.f,165.f);
		m_ExhaustVolumeCone.Init(exhaustConeDir, (useExhaustCone?m_GranularEngineSettings->ExhaustMaxConeAttenuation/100.f:0.f), 45.f, 160.f);
		m_HasInitialisedVolumeCones = true;
	}
	else if(m_VehicleEngineSettings)
	{
		m_EngineVolumeCone.Init(engineConeDir, m_VehicleEngineSettings->MaxConeAttenuation/100.f, 60.f,165.f);
		m_ExhaustVolumeCone.Init(exhaustConeDir, (useExhaustCone?m_VehicleEngineSettings->MaxConeAttenuation/100.f:0.f), 45.f, 160.f);
		m_HasInitialisedVolumeCones = true;
	}
}

// ----------------------------------------------------------------
// Get the idle hull slap volume
// ----------------------------------------------------------------
f32 audCarAudioEntity::GetIdleHullSlapVolumeLinear(f32 speed) const
{
	if(m_Vehicle && (m_Vehicle->InheritsFromAmphibiousQuadBike() || m_Vehicle->InheritsFromAmphibiousAutomobile()))
	{
		return m_VehicleSpeedToHullSlapVol.CalculateValue(speed);
	}
	else
	{
		return audVehicleAudioEntity::GetIdleHullSlapVolumeLinear(speed);
	}
}

// ----------------------------------------------------------------
// audCarAudioEntity GetIdleHullSlapSound
// ----------------------------------------------------------------
u32 audCarAudioEntity::GetIdleHullSlapSound() const
{
	if(m_AmphibiousBoatSettings)
	{
		return m_AmphibiousBoatSettings->IdleHullSlapLoop;
	}

	return audVehicleAudioEntity::GetIdleHullSlapSound();
}

// ----------------------------------------------------------------
// UpdateAmphibiousBoatMode
// ----------------------------------------------------------------
void audCarAudioEntity::UpdateAmphibiousBoatMode(audVehicleVariables* vehicleVariables)
{
	if(m_Vehicle->m_nVehicleFlags.bEngineOn && m_AmphibiousBoatSettings && m_IsInAmphibiousBoatMode)
	{
		Vec3V pos;
		const s32 engineBoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_ENGINE);

		// Using separate water timers for turbulence sounds - the default timers only monitor the big bow VFX 
		// at the front of the vehicle, so can report that the boat is not in water even when water VFX is playing visually.
		bool isAnyWaterVFXActive = IsAnyWaterVFXActive();

		if(isAnyWaterVFXActive)
		{
			m_WaterTurbulenceInWaterTimer += fwTimer::GetTimeStep();
			m_WaterTurbulenceOutOfWaterTimer = 0.f;
		}
		else
		{
			m_WaterTurbulenceOutOfWaterTimer += fwTimer::GetTimeStep();
			m_WaterTurbulenceInWaterTimer = 0.f;
		}

		f32 volLin = m_WaterTurbulenceVolumeCurve.CalculateValue((vehicleVariables->revs - 0.2f) * 1.25f);
		volLin *= isAnyWaterVFXActive? Clamp(m_WaterTurbulenceInWaterTimer * 3.0f, 0.0f, 1.0f) : 1.0f - Clamp(m_WaterTurbulenceOutOfWaterTimer * 3.0f, 0.0f, 1.0f);

		// Submersibles use the centre of the sub as the turbulence sound position
		if(volLin > g_SilenceVolumeLin)
		{
			if(engineBoneId !=-1 )
			{
				pos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
			}
			else
			{
				pos = m_Vehicle->GetTransform().GetPosition();
			}

			if(!m_WaterTurbulenceSound)
			{
				audSoundInitParams initParams;
				initParams.AttackTime = m_WasEngineOnLastFrame? sm_LodSwitchFadeTime : 300;
				initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
				CreateAndPlaySound_Persistent(m_AmphibiousBoatSettings->WaterTurbulance, &m_WaterTurbulenceSound, &initParams);
			}

			if(m_WaterTurbulenceSound)
			{
				f32 vol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);
				f32 pitch = m_WaterTurbulencePitchCurve.CalculateValue(vehicleVariables->revs);

				m_WaterTurbulenceSound->SetRequestedPitch(static_cast<s32>(pitch));
				m_WaterTurbulenceSound->SetRequestedVolume(vol BANK_ONLY(+ g_WaterTurbulenceVolumeTrim));
				m_WaterTurbulenceSound->SetRequestedPosition(pos);
			}
		}
		else if(m_WaterTurbulenceSound)
		{
			m_WaterTurbulenceSound->StopAndForget();
		}
	}
	else if(m_WaterTurbulenceSound)
	{
		m_WaterTurbulenceSound->SetReleaseTime(800);
		m_WaterTurbulenceSound->StopAndForget();
	}

	f32 boatSprayAngle = 0.0f;
	f32 speedFactor = 0.0f;
	f32 inWaterFactor = 0.0f;

	speedFactor = CalculateWaterSpeedFactor();
	inWaterFactor = !m_Vehicle->GetIsInWater()?0.f:1.f;	

	if(m_IsFocusVehicle && m_AmphibiousBoatSettings && m_IsInAmphibiousBoatMode)
	{
		const f32 bankVol = m_BankSpraySmoother.CalculateValue(inWaterFactor, fwTimer::GetTimeInMilliseconds());

		if(bankVol > g_SilenceVolumeLin)
		{
			if(!m_BankSpray)
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
				CreateSound_PersistentReference(m_AmphibiousBoatSettings->BankSpraySound, &m_BankSpray, &initParams);

				if(m_BankSpray)
				{
					m_BankSpray->PrepareAndPlay();
				}
			}

			if(m_BankSpray)
			{
				m_BankSpray->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(bankVol) BANK_ONLY(+ g_BankSprayVolumeTrim));
				Vector3 boatRight = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetA());						
				Vector3 boatForward = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB());						
				boatSprayAngle = DotProduct(boatRight, g_UnitUp);
				m_BankSpray->FindAndSetVariableValue(ATSTRINGHASH("angle", 0xF3805B5), Abs(boatSprayAngle));
				boatRight.SetZ(0.0f);
				boatForward.SetZ(0.0f);

				// Pan the sound left and right as the angle changes
				m_BankSpray->SetRequestedPosition(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()) + (boatForward * g_BankSprayForwardDistanceScalar * ((vehicleVariables->isInFirstPersonCam || vehicleVariables->isInHoodMountedCam) ? 0.0f : 1.0f)) +  (boatRight * g_BankSprayPanDistanceScalar * Clamp(boatSprayAngle * 2, -1.0f, 1.0f)));
			}
		}
		else if(m_BankSpray)
		{
			m_BankSpray->StopAndForget();
		}
	}		
	else if(m_BankSpray)
	{
		m_BankSpray->StopAndForget();
	}	
}

// ----------------------------------------------------------------
// audCarAudioEntity GetLeftWaterSound
// ----------------------------------------------------------------
u32 audCarAudioEntity::GetLeftWaterSound() const
{
	if(m_AmphibiousBoatSettings)
	{
		return m_AmphibiousBoatSettings->LeftWaterSound;
	}

	return audVehicleAudioEntity::GetLeftWaterSound();
}

// ----------------------------------------------------------------
// audCarAudioEntity GetWaveHitSound
// ----------------------------------------------------------------
u32 audCarAudioEntity::GetWaveHitSound() const
{
	if(m_AmphibiousBoatSettings)
	{
		return m_AmphibiousBoatSettings->WaveHitSound;
	}

	return audVehicleAudioEntity::GetWaveHitSound();
}


// ----------------------------------------------------------------
// audCarAudioEntity GetWaveHitBigSound
// ----------------------------------------------------------------
u32 audCarAudioEntity::GetWaveHitBigSound() const
{
	if(m_AmphibiousBoatSettings)
	{
		return m_AmphibiousBoatSettings->WaveHitBigAirSound;
	}

	return audVehicleAudioEntity::GetWaveHitBigSound();
}

void audCarAudioEntity::UpdateParkingBeep(float distance)
{
	if(HasEntityVariableBlock()) 
	{
		SetEntityVariable(ATSTRINGHASH("distance", 0xd6ff9582), distance);
	}

	if(!m_ParkingBeep)
	{
		if(!GetCarAudioSettings())
		{
			return;
		}

		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.u32ClientVar = AUD_VEHICLE_SOUND_HORN;

		const Vector3 pos = VEC3V_TO_VECTOR3(m_Vehicle->TransformIntoWorldSpace(m_HornOffsetPos));
		initParams.Position = pos;

		CreateSound_PersistentReference(GetCarAudioSettings()->ParkingTone, &m_ParkingBeep, &initParams);

		if(m_ParkingBeep)
		{
			m_ParkingBeep->PrepareAndPlay();
		}

	}
}
void audCarAudioEntity::StopParkingBeepSound()
{
	if(m_ParkingBeep){
		if(!m_Vehicle->ContainsLocalPlayer())
		{
			//// if we're getting out of the car, ramp it down in volume
			//// if we're totally out of the car, stop playing it, so we don't ever ramp it up again if you get in another one.
			//f32 inCarRatio = 0.0f;
			//audNorthAudioEngine::GetGtaEnvironment()->IsPlayerInVehicle(&inCarRatio);
			//if (inCarRatio==0.0f)
			//{
			//	StopAndForgetSounds(m_ParkingBeep);
			//	return;
			//}
			//f32 dBVolume = audDriverUtil::ComputeDbVolumeFromLinear(inCarRatio);
			//m_ParkingBeep->SetRequestedVolume(dBVolume);
			m_ParkingBeep->SetReleaseTime(500);					
		}
		StopAndForgetSounds(m_ParkingBeep);
	}
}

bool audCarAudioEntity::CanBeDescribedAsACar()
{
	// Only time it can is if it's a TYPE_CAR, and doesn't have the IAMNOTACAR flag set to true
	if (m_CarAudioSettings)
	{
		if (m_Vehicle && (m_Vehicle->GetVehicleType()==VEHICLE_TYPE_CAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR) && 
			!(AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_IAMNOTACAR) == AUD_TRISTATE_TRUE))
		{
			return true;
		}
	}
	return false;
}

void audCarAudioEntity::EngineStarted()
{
	m_EngineStartTime = fwTimer::GetTimeInMilliseconds();
	m_RevsSmoother.CalculateValue(CTransmission::ms_fIdleRevs,m_EngineStartTime);// init revs to idle

	m_EngineEffectWetSmoother.Reset();
	m_EngineEffectWetSmoother.CalculateValue(0.0f,fwTimer::GetTimeStep());
}

void audCarAudioEntity::SetBoostActive(bool active)
{
	if(active)
	{
		if(!m_NOSBoostLoop)
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;
			initParams.EnvironmentGroup = m_EnvironmentGroup;

			audSoundSet newBoostSoundSet;

			if (m_Vehicle->HasNitrousBoost() && NetworkInterface::IsGameInProgress() && newBoostSoundSet.Init(m_IsPlayerVehicle ? ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_Player_Sounds", 0x1D430286) : ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_NPC_Sounds", 0x96866E58)))
			{
				CreateAndPlaySound_Persistent(newBoostSoundSet.Find(ATSTRINGHASH("boost_activate", 0xF6D2A804)), &m_NOSBoostLoop, &initParams);
			}
			else
			{
				CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("CarNOSBoostStart", 0xDD67FE00)), &initParams);
				CreateAndPlaySound_Persistent(sm_ExtrasSoundSet.Find(ATSTRINGHASH("CarNOSBoostDuration", 0x150E6407)), &m_NOSBoostLoop, &initParams);
			}
		}
	}
	else
	{
		if(m_NOSBoostLoop)
		{
			m_NOSBoostLoop->StopAndForget(true);
		}
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSoundNitrousActive>(CPacketSoundNitrousActive(active), m_Vehicle);
	}
#endif

}

void audCarAudioEntity::TriggerRoofStuckSound()
{
	if(IsDisabled())
	{
		return;
	}

	if(m_CarAudioSettings && m_Vehicle)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;
		CreateAndPlaySound(m_CarAudioSettings->RoofStuckSound, &initParams);
	}
}

bool audCarAudioEntity::InitVehicleSpecific()
{
	if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		m_EngineDamageLayerIndex[0] = 5;
		m_EngineDamageLayerIndex[1] = -1;
		m_EngineDamageLayerIndex[2] = 2;
	}
	else
	{
		// pick two light damage layers 
		m_EngineDamageLayerIndex[0] = g_LightDamageLayerIndices[audEngineUtil::GetRandomNumberInRange(0, g_NumLightDamageLayers-1)];
		do
		{
			m_EngineDamageLayerIndex[1] = g_LightDamageLayerIndices[audEngineUtil::GetRandomNumberInRange(0, g_NumLightDamageLayers-1)];
		}while(m_EngineDamageLayerIndex[0] != m_EngineDamageLayerIndex[1]);
		//and one heavy damage layer
		m_EngineDamageLayerIndex[2] = g_HeavyDamageLayerIndices[audEngineUtil::GetRandomNumberInRange(0,g_NumHeavyDamageLayers-1)];
	}

	m_CarAudioSettings = GetCarAudioSettings();

	if(m_CarAudioSettings)
	{
		m_vehicleEngine.Init(this);
		SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());
		SetupVolumeCones();

		const u32 modelNameHash = GetVehicleModelNameHash();

		if(modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD) || 
		   modelNameHash == ATSTRINGHASH("OPPRESSOR", 0x34B82784) || 
		   modelNameHash == ATSTRINGHASH("OPPRESSOR2", 0x7B54A9D3) ||
		   modelNameHash == ATSTRINGHASH("VIGILANTE", 0xB5EF4C33) ||
		   modelNameHash == ATSTRINGHASH("STROMBERG", 0x34DBA661) ||
		   modelNameHash == ATSTRINGHASH("TOREADOR", 0x56C8A5EF) ||
		   modelNameHash == ATSTRINGHASH("DELUXO", 0x586765FB) ||
		   modelNameHash == ATSTRINGHASH("TERBYTE", 0x897AFC65) ||
		   modelNameHash == ATSTRINGHASH("SCRAMJET", 0xD9F0503D) ||
		   modelNameHash == ATSTRINGHASH("CHERNOBOG", 0xD6BC7523) ||
		   modelNameHash == ATSTRINGHASH("TOREADOR", 0x56C8A5EF))
		{				
			m_HasMissileLockWarningSystem = true;
		}

		if(m_Vehicle->InheritsFromAmphibiousAutomobile() || m_Vehicle->InheritsFromAmphibiousQuadBike() || m_Vehicle->InheritsFromSubmarineCar())
		{
			m_BankSpraySmoother.Init(0.0015f, 0.002f);
			BoatAudioSettings* boatAudioSettings = NULL;			

			if(modelNameHash == ATSTRINGHASH("TECHNICAL2", 0x4662BCBB))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("TECHNICAL2_BOAT", 0x8EFD0604));
			}			
			else if(modelNameHash == ATSTRINGHASH("BLAZER5", 0xA1355F67))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("BLAZER5_BOAT", 0xFF2659BD));
			}
			else if(modelNameHash == ATSTRINGHASH("APC", 0x2189D250))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("APC_BOAT", 0xF81A98FE));
			}
			else if(modelNameHash == ATSTRINGHASH("STROMBERG", 0x34DBA661))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("STROMBERG_SUBMERSIBLE", 0x6E78C824));
			}
			else if (modelNameHash == ATSTRINGHASH("TOREADOR", 0x56C8A5EF))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("TOREADOR_SUBMERSIBLE", 0xE0C5A1B0));
			}
			else if (modelNameHash == ATSTRINGHASH("ZHABA", 0x4C8DBA51))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("ZHABA_BOAT", 0x15924D7D));
			}
			else if(modelNameHash == ATSTRINGHASH("TOREADOR", 0x56C8A5EF))
			{
				boatAudioSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(ATSTRINGHASH("STROMBERG_SUBMERSIBLE", 0x6E78C824));
			}
			else
			{
				audAssertf(false, "Unsupported amphibious vehicle %s!", m_Vehicle->GetVehicleModelInfo()->GetModelName());
			}

			if(boatAudioSettings)
			{
				m_AmphibiousBoatSettings = boatAudioSettings;
				m_WaterTurbulenceVolumeCurve.Init(boatAudioSettings->WaterTurbulanceVol);
				m_VehicleSpeedToHullSlapVol.Init(boatAudioSettings->IdleHullSlapSpeedToVol);
				m_WaterTurbulencePitchCurve.Init(boatAudioSettings->WaterTurbulancePitch);

				if(AUD_GET_TRISTATE_VALUE(boatAudioSettings->Flags, FLAG_ID_BOATAUDIOSETTINGS_ISSUBMARINE) == AUD_TRISTATE_TRUE)
				{
					InitSubmersible(boatAudioSettings);
				}
			}
		}
		
		return true;
	}

	return false;
}

#if __BANK
void audCarAudioEntity::UpdateDebug(audVehicleVariables* state)
{
	if(g_TestForceGameObject && m_IsPlayerVehicle)
	{
		g_TestForceGameObject = false;
		ForceUseGameObject(atHashString(g_TestForceGameObjectName));
	}

	if(g_SimulatePlayerEnteringVehicle && m_IsPlayerVehicle)
	{
		OnFocusVehicleChanged();
		g_SimulatePlayerEnteringVehicle = false;
	}

	if(g_RenderGranularNPCEngines)
	{
		if(IsReal())
		{
			Color32 colour;

			if(m_vehicleEngine.IsGranularEngineActive())
			{
				colour = Color32(0,255,0,128);
			}
			else
			{
				colour = Color32(255,0,0,128);
			}

			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
		}
	}

	if(g_BlipPlayerThrottle && m_IsPlayerVehicle)
	{
		BlipThrottle();
		g_BlipPlayerThrottle = false;
	}

	if(g_RenderVehicleSlots)
	{
		if(m_EngineWaveSlot)
		{
			if(m_EngineWaveSlot->waveSlot->GetLoadedBankId() < AUD_INVALID_BANK_ID)
			{
				char tempString[32];
				sprintf(tempString, "Throttle:%.02f Revs:%.02f", state->throttle, state->revs );
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), Color32(255,255,255), tempString);
			}
		}
	}

	if(g_DebugSpecialFlightMode)
	{
		f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
		f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
		f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;
		f32 angularVelocity = GetCachedAngularVelocity().Mag();

		f32 xCoord = 0.15f;
		f32 yCoord = 0.15f;

		char tempString[128];
		formatf(tempString, "throttle: %f", state->throttle);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		formatf(tempString, "throttleInput: %f", state->throttleInput);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		formatf(tempString, "rollAngle: %f", rollAngle);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		formatf(tempString, "pitchAngle: %f", pitchAngle);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		formatf(tempString, "yawAngle: %f", yawAngle);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		formatf(tempString, "angularVelocity: %f", angularVelocity);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;
	}

	if(g_RenderTimeSlicedVehicles)
	{
		if(m_Vehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
		{
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, Color32(0,255,0,128));
		}
	}

	if(g_DisplayDamageDebug)
	{
		if(m_IsPlayerVehicle)
		{
			f32 xCoord = 0.15f;
			f32 yCoord = 0.15f;

			char tempString[128];
			formatf(tempString, "Revs: %f", state->revs);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Rev Ratio: %f", (state->revs - 0.2f) * 1.25f);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Engine Damage Factor: %f", state->engineDamageFactor);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Body Damage Factor: %f", ComputeEffectiveBodyDamageFactor());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
			{
				CWheel *wheel = m_Vehicle->GetWheel(i);
				formatf(tempString, "Wheel %d friction damage: %f", i, g_ForceWheelDamage? 1.0f : wheel->GetFrictionDamage());
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;
			}

			f32 wheelScrapeTurnVolume = m_DamageTyreScrapeTurnSound != NULL? audDriverUtil::ComputeLinearVolumeFromDb(m_DamageTyreScrapeTurnSound->GetRequestedVolume()) : 0.0f;
			formatf(tempString, "Wheel Scrape Turn Volume: %f", wheelScrapeTurnVolume);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			f32 wheelScrapeVolume = m_DamageTyreScrapeSound != NULL? audDriverUtil::ComputeLinearVolumeFromDb(m_DamageTyreScrapeSound->GetRequestedVolume()) : 0.0f;
			formatf(tempString, "Wheel Scrape Volume: %f", wheelScrapeVolume);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;
		}
	}

	if(g_DebugCarPasses && m_IsPlayerVehicle)
	{
		f32 xCoord = 0.15f;
		f32 yCoord = 0.15f;
		char tempString[128];

		if(g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Player.Speed", 0x8CE50515)))
		{
			formatf(tempString, "Player Speed: %.02f", *g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Player.Speed", 0x8CE50515)));
			grcDebugDraw::Text(Vector2(0.25f, 0.075f), Color32(0,0,255), tempString );
		}
		
		for(u32 loop = 0; loop < sm_CarPassHistory.GetCount(); loop++)
		{
			formatf(tempString, "Vehicle: %s", sm_CarPassHistory[loop].vehicleName);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Bounding Volume: %.02f", sm_CarPassHistory[loop].boundingVolume);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Relative Speed: %.02f", sm_CarPassHistory[loop].relativeSpeed);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Trigger Distance: %.02f", sm_CarPassHistory[loop].triggerDistance);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.04f;
		}
	}

	if((!g_VehicleAudioAllowEntityFocus && IsPlayerVehicle()) || (g_VehicleAudioAllowEntityFocus && GetVehicle() == CDebugScene::FocusEntities_Get(0)))
	{
		if(g_NonGranularDebugRenderingEnabled || g_EngineVolumeCaptureActive)
		{
			f32 xCoord = 80.0f/1280.0f;
			f32 yCoord = 50.0f/720.0f; 
			f32 wavRenderWidth = 1100.0f/1280.0f;
			f32 wavRenderHeight = 50.0f/720.0f;

			Color32 bgColour = Color32(100,100,100,200);
			Vector2 startPos = Vector2(xCoord, yCoord);
			Vector2 endPos = Vector2(xCoord + wavRenderWidth, yCoord + wavRenderHeight);
			grcDebugDraw::RectAxisAligned(startPos, endPos, bgColour, true);

			// Draw the current X playback point
			Vector2 v0,v1;
			v0.x = v1.x = xCoord + (m_RevsSmoother.GetLastValue() * wavRenderWidth);
			v0.y = yCoord;
			v1.y = yCoord + wavRenderHeight;
			grcDebugDraw::Line(v0, v1, Color_black, Color_black);
		}

		if(g_GranularDebugRenderingEnabled || g_NonGranularDebugRenderingEnabled)
		{
			char tempString[128];
			f32 xCoord = 0.7f;
			f32 yCoord = 0.55f;

			formatf(tempString, "Gear: %d/%d", GetVehicle()->m_Transmission.GetGear(), GetVehicle()->m_Transmission.GetNumGears());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Rev Ratio: %f", GetVehicle()->m_Transmission.GetRevRatio());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Throttle: %f", GetVehicle()->m_Transmission.GetThrottle());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Speed: %f", GetCachedVelocity().Mag());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Fwd Speed Ratio: %f", state->fwdSpeedRatio);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Max Speed Pitch Factor: %f", m_MaxSpeedPitchFactor);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Max Speed Revs Add: %f", m_MaxSpeedRevsAddition);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Brake: %f", GetVehicle()->GetBrake());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Clutch: %f", GetVehicle()->m_Transmission.GetClutchRatio());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Roof Openness: %f", GetEntityVariableValue(ATSTRINGHASH("roofopenness", 0xB146F028)));
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			if(m_Vehicle->GetDriver() && m_Vehicle->GetDriver()->GetPlayerInfo())
			{
				formatf(tempString, "Air Drag Mult: %f", m_Vehicle->GetDriver()->GetPlayerInfo()->m_fForceAirDragMult);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;
			}

			if(m_Vehicle->InheritsFromBike())
			{
				formatf(tempString, "Lean Ratio: %f", m_LeanRatio);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
				yCoord += 0.02f;
			}

			formatf(tempString, "Velocity CR: %f", (GetCachedVelocity().Mag() - GetCachedVelocityLastFrame().Mag()) * fwTimer::GetInvTimeStep());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Ang Velocity Mag: %f", GetCachedAngularVelocity().Mag());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Ang Velocity X: %f", GetCachedAngularVelocity().GetX());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Ang Velocity Y: %f", GetCachedAngularVelocity().GetY());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Ang Velocity Z: %f", GetCachedAngularVelocity().GetZ());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Material Grip: %f", PGTAMATERIALMGR->GetMtlTyreGrip(m_MainWheelMaterial));
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

            formatf(tempString, "Hydraulic Input Rate: %f", m_HydraulicSuspensionInputSmoother.GetLastValue());
            grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
            yCoord += 0.02f;

			formatf(tempString, "Hydraulic Angle Rate: %f", m_HydraulicSuspensionAngleSmoother.GetLastValue());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Health Factor: %f", 1.0f - ComputeEffectiveBodyDamageFactor());
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;			

			if(m_Vehicle->pHandling->hFlags & HF_HAS_KERS)
			{
				formatf(tempString, "Remaining Kers: %f", m_Vehicle->m_Transmission.GetKERSRemaining()/CTransmission::ms_KERSDurationMax);
				grcDebugDraw::Text(Vector2(xCoord, yCoord), m_Vehicle->GetKERSActive() ? Color32(0,255,0) : Color32(255,255,255), tempString);
				yCoord += 0.02f;
			}

			if(m_Vehicle->HasRocketBoost())
			{
				formatf(tempString, "Remaining Rocket Boost: %f", m_Vehicle->GetRocketBoostRemaining()/m_Vehicle->GetRocketBoostCapacity());
				grcDebugDraw::Text(Vector2(xCoord, yCoord), m_Vehicle->IsRocketBoosting() ? Color32(0,255,0) : Color32(255,255,255), tempString);
				yCoord += 0.02f;
			}
		}
	}

	if(g_DisplayClusteredVehicles)
	{
		if(m_Vehicle->m_nVehicleFlags.bIsInCluster)
		{
			Color32 colour = Color32(255,0,0,120);
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
		}
	}

	if(m_IsPlayerVehicle)
	{
		if(g_TestBackfire)
		{
			m_Vehicle->m_nVehicleFlags.bAudioBackfired = true;
			g_TestBackfire = false;
		}

		if(g_TestBackfireBanger)
		{
			m_Vehicle->m_nVehicleFlags.bAudioBackfiredBanger = true;
			g_TestBackfireBanger = false;
		}
	}
}
#endif

bool audCarAudioEntity::IsUsingGranularEngine()
{
	return m_GranularEngineSettings != NULL;
}

void audCarAudioEntity::UpdateVehicleSpecific(audVehicleVariables& state, audVehicleDSPSettings& dspSettings)
{
	PF_FUNC(UpdateInternal);

	sm_CarFwdSpeedSum += state.fwdSpeed;
	sm_NumActiveCars++;

	if(m_HasAutoShutOffEngine)
	{
		if(!m_WasPlayingStartupSound &&
		   m_Vehicle->m_nVehicleFlags.bEngineOn && 
		   state.fwdSpeedAbs < 0.1f && 
		   !m_Vehicle->IsNetworkClone() &&
		   m_Vehicle->m_Transmission.GetRevRatio() <= CTransmission::ms_fIdleRevs &&
		   m_Vehicle->m_Transmission.GetThrottle() == 0.0f)
		{
			m_AutoShutOffTimer += fwTimer::GetTimeStep();
		}
		else
		{
			m_AutoShutOffTimer = 0.0f;
		}
	}

	CPed *localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();

	if(localPlayer->GetIsInVehicle() && !m_IsPlayerVehicle && DistSquared(localPlayer->GetTransform().GetPosition(), GetPosition()).Getf() < (g_VehicleReflectionsActivationDist * g_VehicleReflectionsActivationDist))
	{
		m_IsGeneratingReflections = g_ReflectionsAudioEntity.AddVehicleReflection(this);
	}
	else if(m_IsGeneratingReflections)
	{
		g_ReflectionsAudioEntity.RemoveVehicleReflections(this);
		m_IsGeneratingReflections = false;
	}

	if(IsDummy() || IsReal())
	{
		if(IsReal())
		{			
			PopulateCarAudioVariables(&state);
			dspSettings.rolloffScale = state.rollOffScale;
			dspSettings.enginePostSubmixAttenuation = state.enginePostSubmixVol;
			dspSettings.exhaustPostSubmixAttenuation = state.exhaustPostSubmixVol;
			dspSettings.AddDSPParameter(atHashString("RPM", 0x5B924509), state.revs);
			dspSettings.AddDSPParameter(atHashString("Throttle", 0xEA0151DC), state.throttle);
			dspSettings.AddDSPParameter(atHashString("ThrottleInput", 0x918028C4), state.throttleInput);
			dspSettings.AddDSPParameter(atHashString("RevLimiter", 0x4F40F76C), state.onRevLimiter ? 1.0f : 0.0f);
			dspSettings.AddDSPParameter(atHashString("IsInTunnel", 0xEBD5E4C3), m_IsPlayerVehicle && g_ReflectionsAudioEntity.AreInteriorReflectionsActive());
			AddCommonDSPParameters(dspSettings);

			if(!m_Vehicle->m_nVehicleFlags.bEngineOn && m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STOPPING)
			{
				// keep the wet mix at 0 so it smooths up whatever happens (even if the car is started with the player inside)
				m_EngineEffectWetSmoother.CalculateValue(0.0f, fwTimer::GetTimeStep());
			}

			UpdateRevsBoostEffect(&state);
			m_vehicleEngine.Update(&state);
			m_WasEngineOnLastFrame = (m_Vehicle->m_nVehicleFlags.bEngineOn);	
			UpdateBrakes(&state);
			UpdateWind(&state);
			UpdateBlips(&state);
			UpdateWooWooSound(&state);
		}
        
		UpdateGliding(&state);
		UpdateAmphibiousBoatMode(&state);
		UpdateDamage(&state);
		UpdateOffRoad(&state);
		UpdateSuspension(&state);
		UpdateJump(&state);
		UpdateSpecialFlightMode(&state);
		UpdateToyCar(&state);

		if(state.fwdSpeedAbs < 0.1f)
		{
			m_LastTimeStationary = state.timeInMs;
		}
	}

	if(!IsDisabled())
	{
		if(!audNorthAudioEngine::GetMicrophones().IsHeliMicActive())
		{
			UpdateCarPassSounds(&state);
			UpdatePlayerCarSpinSounds(&state);
		}
		
		UpdateIndicator();
		m_vehicleEngine.UpdateEngineCooling(&state);
		
		if(m_AmphibiousBoatSettings)
		{
			if(IsSubmersible())
			{
				UpdateSubmersible(state, m_AmphibiousBoatSettings);
			}
			else
			{
				StopSubmersibleSounds();
			}

			u32 transformSoundSet = GetSubmarineCarTransformSoundSet();			

			if(transformSoundSet != 0u)
			{
				if(m_ShouldStopSubTransformSound)
				{
					if(m_SubmarineTransformSound)
					{
						m_SubmarineTransformSound->StopAndForget();
					}

					m_ShouldStopSubTransformSound = false;
				}
				else if(m_SubmarineTransformSoundHash != 0u)
				{		
					audSoundSet soundSet;

					if(soundSet.Init(transformSoundSet))
					{
						audMetadataRef metadataRef = soundSet.Find(m_SubmarineTransformSoundHash);

						if(metadataRef != g_NullSoundRef)
						{
							if(m_SubmarineTransformSound)
							{
								m_SubmarineTransformSound->StopAndForget();
							}

							audSoundInitParams initParams;
							initParams.TrackEntityPosition = true;
							initParams.UpdateEntity = true;
							initParams.EnvironmentGroup = m_EnvironmentGroup;
							CreateAndPlaySound_Persistent(metadataRef, &m_SubmarineTransformSound, &initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundSet.GetNameHash(), m_SubmarineTransformSoundHash, &initParams, m_SubmarineTransformSound, m_Vehicle));
						}
					}
				}

				m_SubmarineTransformSoundHash = 0u;
			}
		}
	}

	UpdateSportsCarRevs(&state);
	UpdateHydraulicSuspension();

	if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle)
		&& !CVehicleRecordingMgr::IsPlaybackSwitchedToAiForCar(m_Vehicle))
	{
		if(state.fwdSpeedAbs > 1.0f)
		{
			state.forcedThrottle = 1.0f;
		}

		if(GetCachedAngularVelocity().Mag2() > (0.5f * 0.5f))
		{
			f32 velocityChange = (GetCachedVelocity().Mag2() - m_CachedVehicleVelocityLastFrame.Mag2()) * fwTimer::GetInvTimeStep();

			// If we're heavily slowing down, keep the revs on to sound like engine braking
			if(velocityChange < -10.0f && velocityChange > -150.0f)
			{
				state.forcedThrottle = 0.0f;
			}
		}
	}

	if(m_Vehicle->InheritsFromBike() || m_Vehicle->InheritsFromQuadBike() || (m_Vehicle->InheritsFromAmphibiousQuadBike() && !m_IsInAmphibiousBoatMode))
	{
		f32 desiredLeanRatio = Abs(Clamp(((CBike*)m_Vehicle)->m_fAnimLeanFwd, -1.0f, 1.0f) * Clamp(state.fwdSpeedRatio/0.3f, 0.0f, 1.0f));

		if(desiredLeanRatio < m_LeanRatio)
		{
			m_LeanRatio = Clamp(m_LeanRatio - fwTimer::GetTimeStep() * 4.0f, 0.0f, 1.0f);
		}
		else
		{
			m_LeanRatio = Clamp(Min(desiredLeanRatio, (m_LeanRatio + (fwTimer::GetTimeStep() * 2.0f))), 0.0f, 1.0f);
		}

		state.exhaustPostSubmixVol += 1.5f * m_LeanRatio;
		state.enginePostSubmixVol += 1.5f * m_LeanRatio;	
		state.granularEnginePitchOffset = Max(state.granularEnginePitchOffset, (s32)(50.0f * m_LeanRatio));
	}

	m_Vehicle->m_Transmission.ForceThrottle(state.forcedThrottle);

	if(IsReal())
	{
		if(IsSubmersible())
		{
			m_vehicleEngine.SetGranularPitch(static_cast<s32>(m_SubTurningEnginePitchModifier.CalculateValue(m_SubAngularVelocityMag)));
		}
		else
		{
			m_vehicleEngine.SetGranularPitch(state.granularEnginePitchOffset);
		}


		m_vehicleEngine.SetGranularPitch(state.granularEnginePitchOffset);
	}

	SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());

#if __BANK
	if(g_ToggleNos && m_IsPlayerVehicle)
	{
		if(m_NOSBoostLoop)
		{
			SetBoostActive(false);
		}
		else
		{
			SetBoostActive(true);
		}

		g_ToggleNos = false;
	}
#endif

	BANK_ONLY(UpdateDebug(&state);)
}

void audCarAudioEntity::UpdateSpecialFlightMode(audVehicleVariables* state)
{
	u32 soundsetName = 0u;

	if (GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB))
	{
		soundsetName = ATSTRINGHASH("deluxo_flight_sounds", 0xA6DE1C45);
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("OPPRESSOR2", 0x7B54A9D3))
	{
		soundsetName = m_IsFocusVehicle? ATSTRINGHASH("oppressor2_flight_sounds", 0x3243BC86) : ATSTRINGHASH("oppressor2_npc_flight_sounds", 0x6368F652);
	}

	if(soundsetName != 0u)
	{
		 if(m_Vehicle->GetSpecialFlightModeRatio() > 0.f)
		 {
			 if(!m_SpecialFlightModeSound)
			 {
				 audSoundSet soundset;
				 const u32 soundFieldName = ATSTRINGHASH("flight_loop", 0x42AF7BEA);

				 if(soundset.Init(soundsetName))
				 {
					 audSoundInitParams initParams;
					 initParams.TrackEntityPosition = true;
					 initParams.EnvironmentGroup = m_EnvironmentGroup;
					 CreateAndPlaySound_Persistent(soundset.Find(soundFieldName), &m_SpecialFlightModeSound, &initParams);
				 }
			 }

			 if(m_SpecialFlightModeSound)
			 {
				 f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
				 f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
				 f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;
				 f32 angularVelocity = GetCachedAngularVelocity().Mag();
				 REPLAY_ONLY(if (CReplayMgr::IsEditModeActive()) { angularVelocity = m_SpecialFlightModeReplayAngularVelocity; })

				 m_SpecialFlightModeSound->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), state->throttle);
				 m_SpecialFlightModeSound->FindAndSetVariableValue(ATSTRINGHASH("throttleInput", 0x918028C4), state->throttleInput);
				 m_SpecialFlightModeSound->FindAndSetVariableValue(ATSTRINGHASH("rollAngle", 0x94FC4D71), rollAngle);
				 m_SpecialFlightModeSound->FindAndSetVariableValue(ATSTRINGHASH("pitchAngle", 0x809A226D), pitchAngle);
				 m_SpecialFlightModeSound->FindAndSetVariableValue(ATSTRINGHASH("yawAngle", 0x15DE02B1), yawAngle);
				 m_SpecialFlightModeSound->FindAndSetVariableValue(ATSTRINGHASH("angularVelocity", 0xC90EACC2), angularVelocity);
			 }
		 }
		 else if(m_SpecialFlightModeSound)
		 {
			 m_SpecialFlightModeSound->StopAndForget(true);
		 }
	}
}

void audCarAudioEntity::UpdateToyCar(audVehicleVariables* state)
{
	if (IsToyCar())
	{
		f32 throttleInput = Abs(state->throttle);
			
		if(!CReplayMgr::IsEditModeActive())
		{
			throttleInput = Max(throttleInput, Abs(state->throttleInput));
		}

		if (m_Vehicle->IsEngineOn() && (throttleInput > 0 || state->fwdSpeedAbs > 0.1f))
		{
			if (!m_ToyCarEngineLoop)
			{
				audSoundSet soundSet;

				if (soundSet.Init(ATSTRINGHASH("rcbandito_sounds", 0x7B9D9FE0)))
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.UpdateEntity = true;
					CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("motor_loop", 0xF90A7007)), &m_ToyCarEngineLoop, &initParams);
				}
			}

			if (m_ToyCarEngineLoop)
			{
				const f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
				const f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
				const f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;

				m_ToyCarEngineLoop->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), throttleInput);
				m_ToyCarEngineLoop->FindAndSetVariableValue(ATSTRINGHASH("rollAngle", 0x94FC4D71), rollAngle);
				m_ToyCarEngineLoop->FindAndSetVariableValue(ATSTRINGHASH("pitchAngle", 0x809A226D), pitchAngle);
				m_ToyCarEngineLoop->FindAndSetVariableValue(ATSTRINGHASH("yawAngle", 0x15DE02B1), yawAngle);
				m_ToyCarEngineLoop->FindAndSetVariableValue(ATSTRINGHASH("steerAngle", 0x6D2AC1CE), Abs(m_Vehicle->GetSteerAngle()) * RtoD);
				m_ToyCarEngineLoop->FindAndSetVariableValue(ATSTRINGHASH("surfaceHardness", 0x55D4D89C), m_CachedMaterialSettings ? m_CachedMaterialSettings->FootstepMaterialHardness : 0.f);
			}
		}
		else if (m_ToyCarEngineLoop)
		{
			m_ToyCarEngineLoop->StopAndForget(true);
		}
	}
}

void audCarAudioEntity::UpdateJump(audVehicleVariables* state)
{
	bool isUpsideDownInAir = false;

	if(m_IsPlayerVehicle || m_Vehicle->IsNetworkClone())
	{
		f32 angleAdjustCreakVolumeLin = 0.0f;

		if(state->numWheelsTouching == 0 && GetVehicleModelNameHash() != ATSTRINGHASH("OPPRESSOR2", 0x7B54A9D3) && !IsToyCar() && !IsGoKart())
		{
			f32 angVelocityMag = GetCachedAngularVelocity().Mag();
			
			if((m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE) && !m_Vehicle->GetIsInWater())
			{
				if(state->fwdSpeedAbs < 1.0f || state->timeInMs - GetLastTimeOnGround() > 250)
				{
					angleAdjustCreakVolumeLin = Min(Max(Abs(m_InAirRotationalForceX), Abs(m_InAirRotationalForceY)) * Min(angVelocityMag, 1.0f), 1.0f);
				}
			}
			
			// Automobile::isInAir isn't safe to call from the audio thread due to the call to HasContactWheels(), but we already know the wheel contact status so
			// just manually check the rest of the conditions
			bool isInAir = !m_Vehicle->GetIsInWater() && !m_Vehicle->GetIsAnyFixedFlagSet() && m_Vehicle->GetVelocity().Mag2() >= 0.005f;

			if(isInAir)
			{
				if(m_IsPlayerVehicle)
				{
					isUpsideDownInAir = m_Vehicle->IsUpsideDown();

					if(isUpsideDownInAir != m_WasUpsideDownInAir)
					{
						GetCollisionAudio().TriggerDebrisSounds(1.0f);
					}

					if((m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE) && state->timeInMs - GetLastTimeOnGround() > 50 && state->timeInMs - GetLastTimeOnGround() < 100 && !m_HasPlayedInitialJumpStress)
					{						
						audSoundInitParams initParams;
						initParams.TrackEntityPosition = true;
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear((0.6f * Min((angVelocityMag - 1.0f)/4.0f, 1.0f)) + (0.4f * Min(state->fwdSpeedRatio * 2.0f, 1.0f)));
						CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("PlayerCarJumpStartWronk", 0xFB9CB980)), &initParams);
						m_HasPlayedInitialJumpStress = true;
					}
				}

				if((state->revs > 0.95f && state->throttle > 0.9f) || state->onRevLimiter)
				{
					m_JumpThrottleCutTimer += fwTimer::GetTimeStep();
				}
				else if(m_JumpThrottleCutTimer <= 0.2f)
				{
					m_JumpThrottleCutTimer = 0.0f;
				}

				if(m_JumpThrottleCutTimer > 0.2f)
				{
					state->forcedThrottle = 0.0f;
				}
			}
			else
			{
				m_JumpThrottleCutTimer = 0.0f;
			}
		}
		else
		{
			// True by default so that it doesn't play if you just jump with the analog stick pressed - you must release and re-press
			m_HasPlayedInAirJumpStress = true;
			m_HasPlayedInitialJumpStress = false;
			m_JumpThrottleCutTimer = 0.0f;
		}

		if(!m_InAirRotationSound && !m_HasPlayedInAirJumpStress && angleAdjustCreakVolumeLin > g_SilenceVolumeLin && !IsToyCar() && !IsGoKart())
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;
			initParams.UpdateEntity = true;
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(angleAdjustCreakVolumeLin);
			CreateAndPlaySound_Persistent(sm_ExtrasSoundSet.Find(ATSTRINGHASH("PlayerCarAngleAdjustWronk", 0x9A9AFCEF)), &m_InAirRotationSound, &initParams);

			if(m_InAirRotationSound)
			{
				m_HasPlayedInAirJumpStress = true;
			}
		}
		else 
		{
			if(m_InAirRotationSound)
			{
				f32 volumeDB = audDriverUtil::ComputeDbVolumeFromLinear(angleAdjustCreakVolumeLin);
				m_InAirRotationSound->SetRequestedVolume(Max(m_InAirRotationSound->GetRequestedVolume(), volumeDB));
			}
			else if(state->numWheelsTouching == 0 &&
				   ((Sign(m_InAirRotationalForceX) != Sign(m_InAirRotationalForceXLastFrame) && m_InAirRotationalForceXLastFrame != 0.0f) ||
				   (Sign(m_InAirRotationalForceY) != Sign(m_InAirRotationalForceYLastFrame) && m_InAirRotationalForceYLastFrame != 0.0f) ||
				   (m_InAirRotationalForceX == 0.0f && m_InAirRotationalForceY == 0.0f)))
			{	
				m_HasPlayedInAirJumpStress = false;
			}
		}
	}
	else
	{
		m_JumpThrottleCutTimer = 0.0f;
		m_HasPlayedInitialJumpStress = false;
		m_HasPlayedInAirJumpStress = true;
	}

	m_WasUpsideDownInAir = isUpsideDownInAir;
}

// ----------------------------------------------------------------
// audCarAudioEntity UpdateHydraulicSuspension
// ----------------------------------------------------------------
void audCarAudioEntity::UpdateHydraulicSuspension()
{
	bool hydraulicInputLoopValid = false;
	bool hydraulicAngleLoopValid = false;

	if(!IsDisabled())
	{
		if(m_ShouldTriggerHydraulicBounce)
		{
			TriggerHydraulicBounce(false);			
		}		
		else if(fwTimer::GetTimeInMilliseconds() - m_LastHydraulicBounceTime > 250)
		{
			if(m_ShouldTriggerHydraulicActivate)
			{
				if(m_HydraulicActivationDelayFrames == 0)
				{
					SetHydraulicSuspensionActive(true);
				}				
			}
			else if(m_ShouldTriggerHydraulicDeactivate)
			{		
				SetHydraulicSuspensionActive(false);
			}
		}
		
		if(m_Vehicle->InheritsFromAutomobile() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
		{
			const CAutomobile* automobile = static_cast<CAutomobile*>(m_Vehicle);

			if(automobile->HasHydraulicSuspension())
			{
				const bool useDonkSuspension = m_Vehicle->GetVehicleModelInfo() && m_Vehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS);

				if(m_IsPlayerVehicle)
				{
					hydraulicInputLoopValid = UpdateHydraulicSuspensionInputLoop();
				}
				
				CalculateHydraulicSuspensionAngle();
				hydraulicAngleLoopValid = UpdateHydraulicSuspensionAngleLoop(useDonkSuspension);
			}
		}
	}

	if(m_HydraulicActivationDelayFrames > 0)
	{
		m_HydraulicActivationDelayFrames--;
	}
	else
	{
		m_ShouldTriggerHydraulicActivate = false;
	}
	
	m_ShouldTriggerHydraulicDeactivate = false;
	m_ShouldTriggerHydraulicBounce = false;

    if(m_HydraulicSuspensionInputLoop && !hydraulicInputLoopValid)
    {
        m_HydraulicSuspensionInputLoop->StopAndForget();
    }

	if(m_HydraulicSuspensionAngleLoop && !hydraulicAngleLoopValid)
	{
		m_HydraulicSuspensionAngleLoop->StopAndForget();
	}
}

// ----------------------------------------------------------------
// audCarAudioEntity UpdateHydraulicSuspensionInputLoop
// ----------------------------------------------------------------
bool audCarAudioEntity::UpdateHydraulicSuspensionInputLoop()
{
	bool loopValid = false;
	audSoundSet hydraulicSuspensionSoundSet;
	hydraulicSuspensionSoundSet.Init(GetVehicleHydraulicsSoundSetName());

	if(hydraulicSuspensionSoundSet.IsInitialised())
	{
		if(m_HydraulicSuspensionInputSmoother.GetLastValue() > 0.f)
		{
			if(!m_HydraulicSuspensionInputLoop)
			{
				const u32 soundName = ATSTRINGHASH("Hydraulic_Suspension_Loop", 0xF27A3105);				
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.UpdateEntity = true;            
				CreateAndPlaySound_Persistent(hydraulicSuspensionSoundSet.Find(soundName), &m_HydraulicSuspensionInputLoop, &initParams);

				if(m_HydraulicSuspensionInputLoop)
				{
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(hydraulicSuspensionSoundSet.GetNameHash(), soundName, &initParams, m_HydraulicSuspensionInputLoop, m_Vehicle);)
				}							
			}

			if(m_HydraulicSuspensionInputLoop)
			{
				m_HydraulicSuspensionInputLoop->FindAndSetVariableValue(ATSTRINGHASH("MovementRate", 0xE654C106), m_HydraulicSuspensionInputSmoother.GetLastValue());
			}

			loopValid = true;
		}
	}

	return loopValid;
}

// ----------------------------------------------------------------
// audCarAudioEntity UpdateHydraulicSuspensionAngleLoop
// ----------------------------------------------------------------
bool audCarAudioEntity::UpdateHydraulicSuspensionAngleLoop(bool useDonkSuspension)
{
	bool loopValid = false;
	audSoundSet hydraulicSuspensionSoundSet;
	hydraulicSuspensionSoundSet.Init(GetVehicleHydraulicsSoundSetName());

	if(m_HydraulicSuspensionAngleSmoother.GetLastValue() > (m_Vehicle->IsNetworkClone()? g_HydraulicSuspensionAngleLoopTriggerThresholdRemoteVehicle : g_HydraulicSuspensionAngleLoopTriggerThreshold))
	{
		if(m_HydraulicAngleLoopRetriggerValid)
		{
			if(!m_HydraulicSuspensionAngleLoop)
			{				
				const u32 soundName = useDonkSuspension? ATSTRINGHASH("Hydraulic_Suspension_Donk_Loop", 0x7B9EE3F6) : ATSTRINGHASH("Hydraulic_Suspension_Angle_Loop", 0x593CF220);
				audMetadataRef soundRef = hydraulicSuspensionSoundSet.Find(soundName);

				if(soundRef.IsValid())
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.UpdateEntity = true;
					CreateAndPlaySound_Persistent(soundRef, &m_HydraulicSuspensionAngleLoop, &initParams);

					if(m_HydraulicSuspensionAngleLoop)
					{
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(hydraulicSuspensionSoundSet.GetNameHash(), soundName, &initParams, m_HydraulicSuspensionAngleLoop, m_Vehicle);)
					}
				}
			}

			m_HydraulicAngleLoopRetriggerValid = false;
		}	

		loopValid = true;
	}

	if(m_HydraulicSuspensionAngleLoop)
	{
		m_HydraulicSuspensionAngleLoop->FindAndSetVariableValue(ATSTRINGHASH("MovementRate", 0xE654C106), m_HydraulicSuspensionAngleSmoother.GetLastValue());		
	}

	if(!m_HydraulicAngleLoopRetriggerValid)
	{
		if(m_HydraulicSuspensionAngleSmoother.GetLastValue() < (m_Vehicle->IsNetworkClone() ? g_HydraulicSuspensionAngleLoopReTriggerThresholdRemoteVehicle : g_HydraulicSuspensionAngleLoopReTriggerThreshold))
		{
			m_HydraulicAngleLoopRetriggerValid = true;
		}
	}

	return loopValid;
}

// ----------------------------------------------------------------
// Called when the hydraulic suspension is modified
// ----------------------------------------------------------------
void audCarAudioEntity::SetHydraulicSuspensionActive(bool hydraulicsActive)
{
	bool anyWheelTouching = true;
	bool anyWheelLanding = false;
	const bool isUpsideDownOrOnSide = m_Vehicle->IsOnItsSide() || m_Vehicle->IsUpsideDown();
	
	// If we're upside down or sideways, just trigger stuff regardless
	if(!isUpsideDownOrOnSide)
	{
		anyWheelTouching = false;

		for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
		{
			CWheel* wheel = m_Vehicle->GetWheel(loop);

			if(wheel)
			{
				if(wheel->GetIsTouching())
				{
					anyWheelTouching = true;
				}

				if(wheel->GetSuspensionHydraulicState() == WHS_BOUNCE_LANDING || wheel->GetSuspensionHydraulicState() == WHS_BOUNCE_LANDING)
				{
					anyWheelLanding = true;
				}
			}
		}
	}

	if(anyWheelTouching)
	{
		audSoundSet hydraulicSuspensionSoundSet;
		hydraulicSuspensionSoundSet.Init(GetVehicleHydraulicsSoundSetName());

		if(!anyWheelLanding)
		{
			if(hydraulicSuspensionSoundSet.IsInitialised())
			{
				const u32 soundName = hydraulicsActive? ATSTRINGHASH("Hydraulic_Suspension_Activate", 0xC9E2ED9E) : ATSTRINGHASH("Hydraulic_Suspension_Deactivate", 0x1D682F39);
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.UpdateEntity = true;

				if(sysThreadType::IsUpdateThread())
				{
					CreateDeferredSound(hydraulicSuspensionSoundSet.Find(soundName), m_Vehicle, &initParams, true);
				}
				else
				{
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					CreateAndPlaySound(hydraulicSuspensionSoundSet.Find(soundName), &initParams);
				}

				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hydraulicSuspensionSoundSet.GetNameHash(), soundName, &initParams, m_Vehicle));
			}
		}		

		m_LastHydraulicEngageDisengageTime = m_LastHydraulicSuspensionStateChangeTime = fwTimer::GetTimeInMilliseconds();
	}
}

// ----------------------------------------------------------------
// Trigger a hydraulic bounce on the given number of wheels
// ----------------------------------------------------------------
void audCarAudioEntity::TriggerHydraulicBounce(bool smallBounce)
{		
	bool allWheelsTouching = true;
	const bool isUpsideDownOrOnSide = m_Vehicle->IsOnItsSide() || m_Vehicle->IsUpsideDown();	

	// If we're upside down or sideways, just trigger stuff regardless
	if(!isUpsideDownOrOnSide)
	{
		for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
		{
			CWheel* wheel = m_Vehicle->GetWheel(loop);

			if(wheel && !wheel->GetIsTouching())
			{
				allWheelsTouching = false;
				break;
			}			
		}	
	}	

	if(allWheelsTouching)
	{
		audSoundSet hydraulicSuspensionSoundSet;
		hydraulicSuspensionSoundSet.Init(GetVehicleHydraulicsSoundSetName());

		if(hydraulicSuspensionSoundSet.IsInitialised())
		{
			const u32 soundName = smallBounce? ATSTRINGHASH("Hydraulic_Suspension_Bounce_Small", 0x36B0F523) : ATSTRINGHASH("Hydraulic_Suspension_Bounce", 0xFA0677E7);
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.UpdateEntity = true;

			if(sysThreadType::IsUpdateThread())
			{
				CreateDeferredSound(hydraulicSuspensionSoundSet.Find(soundName), m_Vehicle, &initParams, true);
			}
			else
			{
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				CreateAndPlaySound(hydraulicSuspensionSoundSet.Find(soundName), &initParams);
			}
			
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hydraulicSuspensionSoundSet.GetNameHash(), soundName, &initParams, m_Vehicle));
			m_LastHydraulicBounceTime = fwTimer::GetTimeInMilliseconds();
			m_ShouldTriggerHydraulicActivate = false;
		}
	}		

	m_LastHydraulicSuspensionStateChangeTime = fwTimer::GetTimeInMilliseconds();
}

void audCarAudioEntity::UpdateDamage(audVehicleVariables* state)
{
	if(m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_OFF BANK_ONLY(&& !g_MuteDamageLayers))
	{
		for(u32 i = 0; i < g_NumDamageLayersPerCar; i++)
		{
			if(m_EngineDamageLayerIndex[i] != -1)
			{
				s32 pitch = (s32)(g_DamageLayerMinPitch + ((g_DamageLayerMaxPitch - g_DamageLayerMinPitch) * state->revs));
				f32 vol = g_EngineDamageCurves[m_EngineDamageLayerIndex[i]].CalculateValue(state->engineDamageFactor) * audDriverUtil::ComputeLinearVolumeFromDb(state->engineConeAtten + state->engineThrottleAtten);
				UpdateLoopWithLinearVolumeAndPitch(&m_EngineDamageLayers[i], g_EngineDamageLayers[m_EngineDamageLayerIndex[i]].soundHash, vol, pitch, m_EnvironmentGroup, NULL, true, true);
			}
		}
	}
	else
	{
		for(u32 loop = 0; loop < g_NumDamageLayersPerCar; loop++)
		{
			if(m_EngineDamageLayers[loop])
			{
				m_EngineDamageLayers[loop]->StopAndForget(true);
			}
		}
	}

	if(state->engineDamageFactor > 0.5f)
	{
		f32 damageFrequencyRatio = m_EngineDamageFrequencyRatioFluctuator.CalculateValue();
		f32 revRatio = 1.0f - Clamp((((state->revs - 0.2f) * 1.25f)/0.4f), 0.0f, 1.0f);

		if(revRatio > 0.0f && !m_IsInAmphibiousBoatMode)
		{
			state->granularEnginePitchOffset = (s32)(audDriverUtil::ConvertRatioToPitch(damageFrequencyRatio) * state->engineDamageFactor * revRatio);
		}
	}

	static const f32 minWheelDamageFactor = 0.1f;

	if(m_Vehicle->GetNumWheels() >= 4)
	{
		f32 maxWheelDamageVol = 0.0f;
		f32 maxGrindRatio = 0.0f;
		Vector3 soundPosition;
		s32 maxPitch = -1200;
		bool anyWheelsDamaged = false;

		// Quickly check that any wheels are actually damaged. In most cases they won't be, which saves us the work of
		// querying the wheel positions etc.
		for(s32 i = 0; i < 2; i++)
		{
			CWheel *wheel = m_Vehicle->GetWheel(i);
			
			if(m_HasRandomWheelDamage || wheel->GetFrictionDamage() > minWheelDamageFactor BANK_ONLY(|| g_ForceWheelDamage))
			{
				anyWheelsDamaged = true;
				break;
			}
		}

		if(anyWheelsDamaged)
		{
			Vector3 leftWheelPos, rightWheelPos;
			GetCachedWheelPosition(0, leftWheelPos, *state);
			GetCachedWheelPosition(1, rightWheelPos, *state);
			Vector3 wheelCentre = (leftWheelPos + rightWheelPos) * 0.5f;
			soundPosition = wheelCentre;

			// High levels of body damage can also cause wheel damage audio, as its pretty hard to properly damage (ie. misalign) the wheels 
			f32 bodyDamageFactor = ((GetVehicle()->GetVehicleDamage()->GetBodyHealth() / CVehicleDamage::GetBodyHealthMax()) - 0.7f)/0.5f;

			for(s32 i = 0; i < 2; i++)
			{
				CWheel *wheel = m_Vehicle->GetWheel(i);
				f32 speedRatio = (wheel->GetIsTouching()?Clamp(Abs(wheel->GetGroundSpeed()) * state->invDriveMaxVelocity, 0.0f,1.0f):0.0f);
				s32 pitch = g_DamagedWheelMinPitch + (s32)((g_DamagedWheelMaxPitch - g_DamagedWheelMinPitch) * Min(speedRatio/0.5f, 1.0f));

				f32 steeringGrindRatio = 0.0f;

				if(wheel->GetConfigFlags().IsFlagSet(WCF_STEER))
				{
					f32 steeringAngle = wheel->GetSteeringAngle();
					steeringGrindRatio = Clamp(Abs(steeringAngle*4.0f),0.0f,1.0f);
					maxGrindRatio = Max(maxGrindRatio, steeringGrindRatio);
				}

				// Wheels are generally completely dead by about 0.3 damage, so max out the volume a little before then
				f32 wheelDamage = wheel->GetFrictionDamage();

				if(m_HasRandomWheelDamage)
				{
					wheelDamage = Max(wheelDamage, 0.15f);
				}

#if __BANK
				if(g_ForceWheelDamage)
				{
					wheelDamage = 1.0f;
				}
#endif

				f32 damageRatio = 0.0f;

				if(wheelDamage > 0.1f)
				{
					damageRatio = Max(Clamp((wheelDamage - 0.1f)/0.1f, 0.f, 1.f), bodyDamageFactor);
					// Scale a bit of the volume quickly at slow speeds, and then apply a more gradual speed fade with a greater range
					f32 vol = damageRatio * ((0.3f * Clamp((speedRatio/0.075f),0.0f,1.0f)) + (0.7f * speedRatio));
					vol *= (0.6f + (0.4f * steeringGrindRatio)); // Louder when turning
					maxWheelDamageVol = Max(vol, maxWheelDamageVol);
					maxPitch = Max(pitch, maxPitch);
				}

				soundPosition += ((i == 0? leftWheelPos : rightWheelPos) - wheelCentre) * damageRatio;
			}

			maxGrindRatio = m_TyreDamageGrindRatioSmoother.CalculateValue(maxGrindRatio);
		}

		if(maxWheelDamageVol > g_SilenceVolumeLin)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetEnvironmentGroup();
			initParams.UpdateEntity = true;

			if(!m_DamageTyreScrapeTurnSound)
			{
				const audMetadataRef tyreDentTurnRef = GetDamageSoundSet()->Find("Tyre_Scrape_Dent_Turn");
				CreateAndPlaySound_Persistent(tyreDentTurnRef, &m_DamageTyreScrapeTurnSound, &initParams);
			}

			if(!m_DamageTyreScrapeSound)
			{
				const audMetadataRef tyreDentRef = GetDamageSoundSet()->Find("Tyre_Scrape_Dent");
				CreateAndPlaySound_Persistent(tyreDentRef, &m_DamageTyreScrapeSound, &initParams);
			}

			if(m_DamageTyreScrapeTurnSound)
			{
				m_DamageTyreScrapeTurnSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(maxWheelDamageVol * Sinf(maxGrindRatio * (PI * 0.5f))));
				m_DamageTyreScrapeTurnSound->SetRequestedPosition(soundPosition);
				m_DamageTyreScrapeTurnSound->SetRequestedPitch(maxPitch);
				m_DamageTyreScrapeTurnSound->SetRequestedDopplerFactor(0.0f);
			}

			if(m_DamageTyreScrapeSound)
			{
				m_DamageTyreScrapeSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(maxWheelDamageVol * Sinf((1.0f - maxGrindRatio) * (PI * 0.5f))));
				m_DamageTyreScrapeSound->SetRequestedPitch(maxPitch);
				m_DamageTyreScrapeSound->SetRequestedPosition(soundPosition);
				m_DamageTyreScrapeSound->SetRequestedDopplerFactor(0.0f);
			}
		}
		else
		{
			StopAndForgetSounds(m_DamageTyreScrapeSound, m_DamageTyreScrapeTurnSound);
		}
	}

	if(m_IsPlayerVehicle && m_Vehicle->m_nVehicleFlags.bEngineOn BANK_ONLY(&& g_EnableDamageThrottleCut))
	{
		u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

		// This should only occur on vehicles that are actually damaged (not 'fake' audio damaged)
		f32 engineDamageFactor = 1.0f - (m_Vehicle->GetVehicleDamage()->GetEngineHealth() / CTransmission::GetEngineHealthMax());

#if __BANK
		if(g_ForceDamage && m_IsPlayerVehicle)
		{
			engineDamageFactor = g_ForcedDamageFactor;
		}
#endif

		if(currentTime > m_NextDamageThrottleCutTime)
		{
			if(engineDamageFactor > 0.7f && state->gear > 1 && state->throttle == 1.0f)
			{
				f32 throttleCutFactor = (engineDamageFactor - 0.7f)/0.3f;

				if(state->rawRevs > 0.9f - (0.2f * throttleCutFactor))
				{
					if(m_DamageThrottleCutTimer <= 0.0f)
					{
						if(audEngineUtil::ResolveProbability(g_HighRevsMisfireSoundProb))
						{
							audSoundInitParams initParams;
							initParams.UpdateEntity = true;
							initParams.TrackEntityPosition = true;
							initParams.EnvironmentGroup = m_EnvironmentGroup;
							CreateAndPlaySound(ATSTRINGHASH("DAMAGE_ENGINE_MISFIRE", 0x39395D40), &initParams);
						}

						m_DamageThrottleCutTimer = audEngineUtil::GetRandomNumberInRange(g_MinDamageThrottleCutDuration, g_MaxDamageThrottleCutDuration);
						
						f32 shortCutProbability = g_ShortDamageCutProbMin + ((g_ShortDamageCutProbMax - g_ShortDamageCutProbMin) * throttleCutFactor);

						if(audEngineUtil::ResolveProbability(shortCutProbability))
						{
							m_NextDamageThrottleCutTime = currentTime + audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenDamageThrottleCutsShort, g_MaxTimeBetweenDamageThrottleCutsShort);
							m_ConsecutiveShortThrottleCuts++;
						}
						else
						{
							m_NextDamageThrottleCutTime = currentTime + audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenDamageThrottleCuts, g_MaxTimeBetweenDamageThrottleCuts);

							if(!m_vehicleEngine.IsMisfiring())
							{
								if(m_ConsecutiveShortThrottleCuts > 2)
								{
									if(audEngineUtil::ResolveProbability(0.05f * m_ConsecutiveShortThrottleCuts))
									{
										audSoundInitParams initParams;
										initParams.UpdateEntity = true;
										initParams.EnvironmentGroup = m_EnvironmentGroup;
										initParams.Predelay = (s32)(m_DamageThrottleCutTimer * 1000.0f);
										initParams.PostSubmixAttenuation = state->exhaustConeAtten;
										initParams.u32ClientVar = AUD_VEHICLE_SOUND_EXHAUST;
										if(CreateAndPlaySound(ATSTRINGHASH("EX_POPS_BACKFIRE", 0x38BEF758), &initParams))
										{
											m_Vehicle->m_nVehicleFlags.bAudioBackfiredBanger = true;
										}
									}
								}
							}

							m_ConsecutiveShortThrottleCuts = 0;
						}
					}
				}
			}
		}

		if(m_DamageThrottleCutTimer > 0.0f)
		{
			m_DamageThrottleCutTimer -= fwTimer::GetTimeStep();
			state->forcedThrottle = 0.0f;
		}
	}
	else
	{
		m_DamageThrottleCutTimer = 0.0f;
	}
}

void audCarAudioEntity::UpdateWind(audVehicleVariables* state)
{
	if(m_IsPlayerVehicle)
	{
		if(!m_WindFastSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			CreateAndPlaySound_Persistent(sm_ExtrasSoundSet.Find(ATSTRINGHASH("SpeedWindSound", 0x1209E974)), &m_WindFastSound, &initParams);
		}

		if(m_WindFastSound)
		{ 
			f32 smoothedSpeed = sm_PlayerWindNoiseSpeedSmoother.CalculateValue(Min(state->fwdSpeed, g_AudioEngine.GetEnvironment().GetActualListenerSpeed()), g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			m_WindFastSound->FindAndSetVariableValue(ATSTRINGHASH("SmoothedSpeed", 0xC5D4E354), smoothedSpeed);

			f32 cutoff = kVoiceFilterLPFMaxCutoff;	

			if(audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
			{
				static const float minInteriorViewCutoff = 1000.0f;
				const f32 interiorViewCutoff =  minInteriorViewCutoff + ((kVoiceFilterLPFMaxCutoff - minInteriorViewCutoff) * sm_PlayerVehicleOpenness);
				cutoff = Min(cutoff, interiorViewCutoff);
			}

#if __BANK
			sm_PlayerWindNoiseSpeedSmoother.SetRates(1.0f/g_WindNoiseSmootherIncreaseRate, 1.0f/g_WindNoiseSmootherDecreaseRate);

			if(g_DebugWindNoise)
			{
				char tempString[128];
				formatf(tempString, "Fwd Speed: %f", state->fwdSpeed);
				grcDebugDraw::Text(Vector2(0.15f, 0.15f), Color32(255,255,255), tempString);

				formatf(tempString, "Smoothed Speed: %f", sm_PlayerWindNoiseSpeedSmoother.GetLastValue());
				grcDebugDraw::Text(Vector2(0.15f, 0.17f), Color32(255,255,255), tempString);
			}
#endif

			m_WindFastSound->SetRequestedLPFCutoff((u32)cutoff);
		}
	}
	else if(m_WindFastSound)
	{
		m_WindFastSound->StopAndForget();
	}
}

void audCarAudioEntity::BlipThrottle()
{
	m_ForceThrottleBlip = true;
}

GranularEngineAudioSettings* audCarAudioEntity::GetGranularEngineAudioSettings() const
{
	if(m_IsPlayerVehicle || g_GranularEnableNPCGranularEngines || g_ForceGranularNPCEngines)
	{
		return m_GranularEngineSettings;
	}
	else
	{
		return NULL;
	}
}

void audCarAudioEntity::OnFocusVehicleChanged()
{
	if(g_GranularEnabled && 
	   m_vehicleEngine.HasBeenInitialised() &&
	   m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STOPPING)
	{
		m_vehicleEngine.QualitySettingChanged();
	}

	if(!m_IsFocusVehicle && m_ParachuteLoop)
	{
		m_ParachuteLoop->StopAndForget();
	}

	StopSirenIfBroken();

	const u32 vehicleModelNameHash = GetVehicleModelNameHash();

	if(vehicleModelNameHash == ATSTRINGHASH("VOLTIC2", 0x3AF76F4A) || vehicleModelNameHash == ATSTRINGHASH("rcbandito", 0xEEF345EC))
	{
		// Voltic 2 has a special high-quality sized SFX bank, so need to make sure we clear that out when the quality switches
		if(m_vehicleEngine.HasBeenInitialised() && m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STOPPING)
		{
			ReapplyDSPEffects();
			FreeWaveSlot(false);
		}
	}
}

// -------------------------------------------------------------------------------
// audCarAudioEntity::AcquireWaveSlot
// -------------------------------------------------------------------------------
bool audCarAudioEntity::AcquireWaveSlot()
{
	u32 sfxBankLoadSound = 0u;
	m_RequiresSFXBank = false;
	bool useHighQualitySfxBank = false;

	if(m_IsFocusVehicle)
	{
		u32 vehicleModelNameHash = GetVehicleModelNameHash();

		if(vehicleModelNameHash == ATSTRINGHASH("VOLTIC2", 0x3AF76F4A))
		{
			sfxBankLoadSound = ATSTRINGHASH("voltic2_bank_load_sound", 0xCA26A3F1);
			useHighQualitySfxBank = true;
		}
		else if (vehicleModelNameHash == ATSTRINGHASH("rcbandito", 0xEEF345EC))
		{
			sfxBankLoadSound = ATSTRINGHASH("rcbandito_bank_load_sound", 0x388C3ED1);
			useHighQualitySfxBank = true;
		}
		else if (vehicleModelNameHash == ATSTRINGHASH("imorgon", 0xBC7C0A00))
		{
			sfxBankLoadSound = ATSTRINGHASH("imorgon_bank_load_sound", 0x78851083);
		}
		else if(m_AmphibiousBoatSettings)
		{
			sfxBankLoadSound = m_AmphibiousBoatSettings->SFXBankSound;
		}
	}

	if(sfxBankLoadSound != 0u && sfxBankLoadSound != g_NullSoundHash)
	{
		m_RequiresSFXBank = true;
		RequestSFXWaveSlot(sfxBankLoadSound, useHighQualitySfxBank);

		if(!m_SFXWaveSlot)
		{
			return false;
		}
	}
	else
	{
		m_RequiresSFXBank = false;
	}

	if(m_vehicleEngine.IsGranularEngineActive())
	{
		if(m_vehicleEngine.IsLowQualityGranular())
		{
			RequestWaveSlot(&sm_StandardWaveSlotManager, m_GranularEngineSettings->NPCEngineAccel);
		}
		else
		{
			RequestWaveSlot(&sm_HighQualityWaveSlotManager, m_GranularEngineSettings->EngineAccel);
		}
	}
	else if(m_VehicleEngineSettings)
	{
		RequestWaveSlot(&sm_StandardWaveSlotManager, m_VehicleEngineSettings->IdleEngineSimpleLoop);
	}
	else if(m_ElectricEngineSettings)
	{
		RequestWaveSlot(&sm_StandardWaveSlotManager, m_ElectricEngineSettings->BankLoadSound);
	}

	if(m_RequiresSFXBank)
	{
		// If we didn't manage to acquire a primary wave slot, release the SFX slot so we don't hog it
		if(m_SFXWaveSlot && !m_EngineWaveSlot)
		{
			audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
		}

		return m_SFXWaveSlot != NULL && m_EngineWaveSlot != NULL;
	}
	else
	{
		return m_EngineWaveSlot != NULL;
	}
}

// -------------------------------------------------------------------------------
// audCarAudioEntity::GetDesiredLOD
// -------------------------------------------------------------------------------
audVehicleLOD audCarAudioEntity::GetDesiredLOD(f32 fwdSpeedRatio, u32 originalDistFromListenerSq, bool visibleBySniper)
{
	// Artificially increase height offset to give preference to vehicles on the same plane as the listener
	Vector3 distToListener = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(audNorthAudioEngine::GetMicrophones().IsTinyRacersMicrophoneActive() ? 1 : 0));
	distToListener.SetZ(distToListener.GetZ() * 2.0f);
	f32 mag2 = visibleBySniper? originalDistFromListenerSq : distToListener.Mag2();
	f32 speedRatioMult = Clamp((fwdSpeedRatio - g_LODMinSpeedRatio)/g_LODMaxSpeedRatio, 0.0f, 1.0f);
	bool movingQuicklyAway = !visibleBySniper && (((s32)m_DistanceFromListener2 - (s32)m_DistanceFromListener2LastFrame) * fwTimer::GetInvTimeStep() > 200.0f);
	m_DummyLODValid = false;

	if(m_ScriptPriority == AUD_VEHICLE_SCRIPT_PRIORITY_MEDIUM)
	{
		mag2 *= 0.5f;
		speedRatioMult = Clamp(speedRatioMult * 2.0f, 0.0f, 1.0f);
		movingQuicklyAway = false;
	}
	else if(m_ScriptPriority == AUD_VEHICLE_SCRIPT_PRIORITY_HIGH)
	{
		mag2 *= 0.33f;
		speedRatioMult = 1.0f;
		movingQuicklyAway = false;
	}

	// No driver, no revs, disabled? Car is probably just sat stationary so keep it disabled. Personal vehicles can be turned on remotely at any point, so are an exception.
	if(!m_Vehicle->GetDriver() && (!m_Vehicle->m_nVehicleFlags.bEngineOn && m_vehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_OFF) && !m_IsFocusVehicle && !m_IsPersonalVehicle)
	{
		return AUD_VEHICLE_LOD_DISABLED;
	}
	else if(m_IsFocusVehicle)
	{
#if __BANK
		if(g_TreatAsDummyCar)
		{
			return AUD_VEHICLE_LOD_DUMMY; 
		}
		else
#endif
		{
			return AUD_VEHICLE_LOD_REAL;
		}
	}
	else
	{
		// Dummy cars only care about speed. All we're playing is roadnoise-type sounds for them, which is inaudible when stationary, so no point in having a
		// silent car take up a dummy slot.
		if((m_VehicleLOD <= AUD_VEHICLE_LOD_DUMMY || !movingQuicklyAway) && mag2 < (g_DummyRadii[m_CarAudioSettings->VolumeCategory] * sm_DummyRangeScale[GetVehicleQuadrant()] * speedRatioMult))
		{
			m_DummyLODValid = true;
		}

		f32 desiredVisibilityScalar = 1.0f;

		if(m_ScriptPriority < AUD_VEHICLE_SCRIPT_PRIORITY_HIGH)
		{
			spdSphere boundSphere;
			m_Vehicle->GetBoundSphere(boundSphere);
			if(!camInterface::IsSphereVisibleInGameViewport(boundSphere.GetV4()))
			{
				// We're not maxing out the submixes, allow vehicles to stay audible even after going off screen
				if(sm_ActivationRangeScale == g_MaxRealRangeScale &&
				   (!m_vehicleEngine.IsGranularEngineActive() || sm_GranularActivationRangeScale == g_MaxGranularRangeScale))
				{
					desiredVisibilityScalar = Max(g_OffscreenVisiblityScalar, m_VisibilitySmoother.GetLastValue());
				}
				else
				{
					desiredVisibilityScalar = g_OffscreenVisiblityScalar;
				}
			}
		}

		f32 visibilityScalar = m_VisibilitySmoother.CalculateValue(desiredVisibilityScalar, fwTimer::GetTimeStep());
		f32 underLoadScalar = 1.0f;

		if(m_Vehicle->m_Transmission.IsEngineUnderLoad() && fwdSpeedRatio > 0.01f && m_Vehicle->m_Transmission.GetGear() == 1)
		{
			underLoadScalar *= 1.3f;
		}

		// Cars playing high-rolloff revs can stay real until they finish
		if(m_SportsCarRevsSound || m_SportsCarRevsApplyFactor > 0.0f || m_SportsCarRevsRequested)
		{
			return AUD_VEHICLE_LOD_REAL;
		}
		// Active cars primarily take speed into account, but also a bit of distance - we still want to hear the idle on very close stationary ones
		else if(mag2 < (g_DummyRadii[m_CarAudioSettings->VolumeCategory] * sm_ActivationRangeScale * sm_GranularActivationRangeScale * underLoadScalar * visibilityScalar * ((1.0f - g_LODSpeedBias) + (g_LODSpeedBias * speedRatioMult))))
		{
			if(movingQuicklyAway && m_VehicleLOD != AUD_VEHICLE_LOD_REAL)
			{
				return m_VehicleLOD;
			}
			else
			{
				return AUD_VEHICLE_LOD_REAL;
			}
		}
		else if(m_DummyLODValid)
		{
			return AUD_VEHICLE_LOD_DUMMY;
		}
		else
		{
			return AUD_VEHICLE_LOD_DISABLED;
		}
	}
}

void audCarAudioEntity::StartEngineSounds(const bool UNUSED_PARAM(fadeIn))
{
	m_EngineStartTime = fwTimer::GetTimeInMilliseconds();
	m_RevsSmoother.CalculateValue(CTransmission::ms_fIdleRevs,m_EngineStartTime);// init revs to idle
	m_EngineEffectWetSmoother.Reset();
	m_EngineEffectWetSmoother.CalculateValue(0.0f,fwTimer::GetTimeStep());
}

// -------------------------------------------------------------------------------
// audCarAudioEntity::UpdatePlayerCarSpinSounds
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdatePlayerCarSpinSounds(audVehicleVariables* state)
{
	// Reusing m_DistanceToPlayerLastFrame/m_LastCarByTime as we don't need them for player vehicle
	if(m_IsPlayerVehicle && m_Vehicle->InheritsFromAutomobile())
	{
		const f32 dp = m_Vehicle->GetTransform().GetRoll();

		if(state->numWheelsTouchingFactor == 0.0f && 
		   Sign(dp) != Sign(m_DistanceToPlayerLastFrame) && 
		   state->timeInMs - GetLastTimeOnGround() > 200 &&
		   m_CachedVehicleVelocity.Mag2() > (25.0f))
		{
			if(m_LastCarByTime + 250 < state->timeInMs)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				const f32 dpChangeRate = Abs(dp - m_DistanceToPlayerLastFrame) * fwTimer::GetInvTimeStep();
				initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(g_PlayerAngVelToCarPass.CalculateValue(dpChangeRate));
				CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("PlayerCarSpin", 0x7E61D83A)), &initParams);
			}

			m_LastCarByTime = state->timeInMs;
		}

		m_DistanceToPlayerLastFrame = dp;
	}
}

// -------------------------------------------------------------------------------
// audCarAudioEntity::TriggerSportsCarRevs
// -------------------------------------------------------------------------------
void audCarAudioEntity::TriggerSportsCarRevs(SportCarRevsTriggerType triggerType)
{
	if(m_VehicleLOD == AUD_VEHICLE_LOD_REAL)
	{
		if(!m_SportsCarRevsSound)
		{
			AllocateVehicleVariableBlock();
			CreateAndPlaySound_Persistent(triggerType == REVS_TYPE_JUNCTION? ATSTRINGHASH("SPORTS_REVS_JUNCTION", 0x7EF46601) : ATSTRINGHASH("SPORTS_REVS_PASSBY", 0xA718080A), &m_SportsCarRevsSound);
			m_LastSportsCarRevsTime = fwTimer::GetTimeInMilliseconds();
			m_SportsCarRevsRequested = false;

			if(m_GranularEngineSettings)
			{
				sm_SportsCarRevsSoundHistory[sm_SportsCarRevsHistoryIndex].soundName = m_GranularEngineSettings->EngineAccel;
				sm_SportsCarRevsSoundHistory[sm_SportsCarRevsHistoryIndex].time = fwTimer::GetTimeInMilliseconds();
				sm_SportsCarRevsHistoryIndex = (sm_SportsCarRevsHistoryIndex + 1) % kSportsCarRevsHistorySize;
			}
		}
	}
	else
	{
		m_SportsCarRevsRequested = true;
	}

	m_WasSportsCarRevsCancelled = false;
	m_SportsCarRevsTriggerType = triggerType;
}

void audCarAudioEntity::UpdateCarPassSounds(audVehicleVariables* state)
{
	// car by wind sounds
	if((m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE) && !m_IsPlayerVehicle)
	{
		const CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();

		if(playerVeh)
		{			
			static const f32 passbyAttackTime = 0.1f;
			const Vector3 vPlayerVehPosition = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetPosition()) + (playerVeh->GetVehicleAudioEntity()->GetCachedVelocity() * passbyAttackTime);
			const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()) + (GetCachedVelocity() * passbyAttackTime);
			const f32 distToPlayer2 = (vPlayerVehPosition-vVehiclePosition).Mag2();

			if(FPIsFinite(distToPlayer2) && distToPlayer2 <= (15.f * 15.f))
			{
				const f32 distToPlayer = Sqrtf(distToPlayer2);
				const f32 distToPlayerDelta = distToPlayer - m_DistanceToPlayerLastFrame;

				if(state->timeInMs > m_LastCarByTime + 2000 && 
					m_WasInCarByRange && 
					m_DistanceToPlayerDeltaLastFrame != 0.0f && 
					Abs(vPlayerVehPosition.z-vVehiclePosition.z) < 5.f && 
					Sign(m_DistanceToPlayerDeltaLastFrame) != Sign(distToPlayerDelta))
				{
					const f32 relSpeed = (playerVeh->GetVelocity()-m_Vehicle->GetVelocity()).Mag();

					// Boost the volume slightly when in high speed mode
					const f32 distanceVol = g_DistanceToCarByVol.CalculateValue(distToPlayer);
					const f32 relVol = audDriverUtil::ComputeDbVolumeFromLinear(g_RelSpeedToCarByVol.CalculateValue(relSpeed) * distanceVol) + (2.0f * sm_HighSpeedSceneApplySmoother.GetLastValue());

					if(relVol > g_SilenceVolume)
					{
						audSoundInitParams initParams;
						initParams.TrackEntityPosition = true;
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						initParams.Volume = relVol;

						audSound *sound = NULL;
						CreateSound_LocalReference(sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehiclePassBy", 0xD52094B3)), &sound, &initParams);

						if(sound)
						{
							const f32 boundingVolume = (m_Vehicle->GetBoundingBoxMax() - m_Vehicle->GetBoundingBoxMin()).Mag();
							sound->FindAndSetVariableValue(ATSTRINGHASH("RELATIVE_SPEED", 0xCA2A352A), relSpeed);
							sound->FindAndSetVariableValue(ATSTRINGHASH("BOUNDING_VOLUME", 0x49613F52), boundingVolume);

							// only want this playing from listener 1 - the car mic
							sound->SetRequestedListenerMask(1);
							sound->SetRequestedDopplerFactor(0.0f);
							sound->PrepareAndPlay();

							// enable distance attenuation if we're in a cinematic camera
							const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
							if(renderedCamera)
							{
								if(renderedCamera->GetClassId() == camCinematicStuntCamera::GetStaticClassId() ||
								   renderedCamera->GetClassId() == camCinematicHeliChaseCamera::GetStaticClassId())
								{
									sound->SetRequestedShouldAttenuateOverDistance(AUD_TRISTATE_TRUE);
								}
							}							

#if __BANK
							if(g_DebugCarPasses)
							{
								grcDebugDraw::Sphere(vVehiclePosition, 3.f, Color32(255,0,0,128), true, 10);

								carPassHistory history;
								history.vehicleName = GetVehicleModelName();
								history.boundingVolume = boundingVolume;
								history.relativeSpeed = relSpeed;
								history.triggerDistance = distToPlayer;
								sm_CarPassHistory.PushAndGrow(history);

								if(sm_CarPassHistory.GetCount() > 6)
								{
									sm_CarPassHistory.Delete(0);
								}
							}
#endif
						}
					}

					m_LastCarByTime = state->timeInMs;
					m_DistanceToPlayerDeltaLastFrame = m_DistanceToPlayerLastFrame = 0.f;
				}

				m_DistanceToPlayerLastFrame = distToPlayer;

				if(!m_WasInCarByRange)
				{
					m_DistanceToPlayerDeltaLastFrame = 0.0f;
				}
				else
				{
					m_DistanceToPlayerDeltaLastFrame = distToPlayerDelta;
				}

				m_WasInCarByRange = true;
			}
			else
			{
				m_WasInCarByRange = false;
			}
		}
	}
}

// ----------------------------------------------------------------
// Update offroad noises
// ----------------------------------------------------------------
void audCarAudioEntity::UpdateOffRoad(audVehicleVariables* state)
{
#if __BANK
	if(g_DisableRoadNoise)
	{
		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			if(m_WheelOffRoadSounds[i])
			{
				m_WheelOffRoadSounds[i]->StopAndForget();
			}
		}

		StopAndForgetSounds(m_OffRoadRumbleSound, m_OffRoadRumblePulse);
		return;
	}	
#endif

	if(m_IsPlayerVehicle && !IsToyCar() && IsReal() && m_Vehicle)
	{
		CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)(m_Vehicle->GetBaseModelInfo());

		if(modelInfo)
		{
			for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
			{
				s32 wheelId = m_Vehicle->GetWheel(i)->GetHierarchyId();

				// Only care about the main four wheels
				if(wheelId >= VEH_WHEEL_FIRST_WHEEL && wheelId < VEH_WHEEL_FIRST_WHEEL + 4 && m_LastMaterialIds[i] != ~0U)
				{
					naAssertf(m_LastMaterialIds[i]<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "out of bounds");

					if(g_audCollisionMaterials[static_cast<u32>(m_LastMaterialIds[i])])
					{
						u32 offRoadSoundHash = g_audCollisionMaterials[static_cast<u32>(m_LastMaterialIds[i])]->OffRoadSound;

						if(offRoadSoundHash != g_NullSoundHash && offRoadSoundHash == m_WheelOffRoadSoundHashes[i])
						{
							if(!m_WheelOffRoadSounds[i])
							{
								audSoundInitParams initParams;
								initParams.EnvironmentGroup = m_EnvironmentGroup;
								GetCachedWheelPosition(i, initParams.Position, *state);
								initParams.UpdateEntity = true;
								initParams.u32ClientVar = GetWheelSoundUpdateClientVar(i - VEH_WHEEL_FIRST_WHEEL);
								CreateAndPlaySound_Persistent(offRoadSoundHash, &m_WheelOffRoadSounds[i], &initParams);
							}
						}
						else if(m_WheelOffRoadSounds[i])
						{
							m_WheelOffRoadSounds[i]->StopAndForget();
						}

						m_WheelOffRoadSoundHashes[i] = offRoadSoundHash;
					}
				}
			}
		}
	}

	// Only doing this on real cars as material settings will always be tarmac on dummy or lower
	if(IsReal() && !IsToyCar())
	{
		if(m_CachedMaterialSettings && m_CachedMaterialSettings->OffRoadRumbleSound != g_NullSoundHash && state->numWheelsTouching > 0)
		{
			if(m_OffRoadRumbleSound && m_CachedMaterialSettings->OffRoadRumbleSound != m_PrevOffRoadRumbleHash)
			{
				StopAndForgetSounds(m_OffRoadRumbleSound);
			}

			if(!m_OffRoadRumbleSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_UNKNOWN;
				initParams.Volume = m_CarAudioSettings->OffRoadRumbleSoundVolume/100.0f;
				CreateAndPlaySound_Persistent(m_CachedMaterialSettings->OffRoadRumbleSound, &m_OffRoadRumbleSound, &initParams);
				m_PrevOffRoadRumbleHash = m_CachedMaterialSettings->OffRoadRumbleSound;

				if(m_OffRoadRumbleSound && m_IsPlayerVehicle && !m_OffRoadRumblePulse && audNorthAudioEngine::ShouldTriggerPulseHeadset())
				{
					CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("OffRoadRumble", 0x1ADE4D12)), &m_OffRoadRumblePulse);
				}
			}
		}
		else
		{
			StopAndForgetSounds(m_OffRoadRumbleSound);
		}


		// Only play rumble pulse while the real rumble is playing on the player vehicle
		if(!m_IsPlayerVehicle || !audNorthAudioEngine::ShouldTriggerPulseHeadset() || !m_OffRoadRumbleSound)
		{
			StopAndForgetSounds(m_OffRoadRumblePulse);
		}
	}
}

void audCarAudioEntity::UpdateSuspension(audVehicleVariables* state)
{
	f32 cinematicOffsetVol = 0.f;

	if(m_IsPlayerVehicle || AreHydraulicsActive())
	{
#if __BANK
		if(g_MuteSuspension)
		{
			for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
			{
				if(m_SuspensionSounds[i])
				{
					m_SuspensionSounds[i]->StopAndForget();
				}
			}

			return;
		}
#endif
		const u32 currentTime = fwTimer::GetTimeInMilliseconds();
		bool twoWheelLanding = false;
        bool hydraulicsActive = AreHydraulicsActive();              

		// suspension sounds
		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			f32 compressionChange = m_Vehicle->GetWheel(i)->GetCompressionChange() * fwTimer::GetInvTimeStep();
			// use the max of the previous frame and this frame, since physics is double stepped otherwise we'll
			// miss some

			bool ignoreWheel = false;

			if(m_Vehicle->m_nVehicleFlags.bIsAsleep || 
			   (m_Vehicle->InheritsFromSubmarineCar() && !((CSubmarineCar*)m_Vehicle)->AreWheelsActive()) ||
			   ((GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB) || ATSTRINGHASH("OPPRESSOR2", 0x7B54A9D3)) && m_Vehicle->GetSpecialFlightModeRatio() > 0.f))
			{
				// ignore physics if asleep or the wheel isn't touching anything
				compressionChange = 0.0f;
				ignoreWheel = true;
			}
			
			const f32 absCompressionChange = Abs(compressionChange);
			f32 damageFactor = 1.0f - m_Vehicle->GetWheel(i)->GetSuspensionHealth() * SUSPENSION_HEALTH_DEFAULT_INV;
			f32 ratio = (absCompressionChange - m_CarAudioSettings->MinSuspCompThresh) / (m_CarAudioSettings->MaxSuspCompThres - m_CarAudioSettings->MinSuspCompThresh - (damageFactor*2.f));

			// Exaggerate the suspension travel on rougher surfaces
			if(m_CachedMaterialSettings)
			{
				ratio *= (1.0f + m_CachedMaterialSettings->Roughness);
			}
			
			ratio = Clamp(ratio, 0.0f,1.0f);

			// suspension bottoming out
			CWheel* wheel = m_Vehicle->GetWheel(i);

			if(wheel)
			{
				u32 hierarchyID = wheel->GetHierarchyId();

				if(hierarchyID >= VEH_WHEEL_LF && hierarchyID <= VEH_WHEEL_RR)
				{
					if(!ignoreWheel && wheel->GetCompression() + wheel->GetWheelRadiusIncBurst() >= wheel->GetSuspensionLength())
					{
						m_LastBottomOutTime[i] = currentTime;

						s32 oppositeWheel = m_Vehicle->GetWheel(i)->GetOppositeWheelIndex();

						if(oppositeWheel >= 0)
						{
							if(currentTime - m_LastBottomOutTime[oppositeWheel] < 100)
							{
								if(currentTime - m_LastTimeInAir < 250 &&
								   m_CachedVehicleVelocity.Mag() > 5.0f) // Using velocity rather than state->fwdspeed so spinning the car doesn't affect this
								{
									GetCollisionAudio().TriggerJumpLand(5.f);

                                    if(!hydraulicsActive && !IsToyCar())
                                    {
									    GetCollisionAudio().TriggerJumpLandScrape();
                                    }
								}
							}
						}

						// On a 4-wheel vehicle, detect when the player has righted the vehicle after being stationary upside down, and play a clunk then
						if(m_Vehicle->GetNumWheels() == 4)
						{
							if(currentTime - m_LastTimeTwoWheeling < 250 && 
							   state->numWheelsTouching == 4)
							{
								s32 frontRearWheel = VEH_INVALID_ID;

								switch(hierarchyID)
								{
								case VEH_WHEEL_LF:
									frontRearWheel = VEH_WHEEL_LR;
									break;
								case VEH_WHEEL_LR:
									frontRearWheel = VEH_WHEEL_LF;
									break;
								case VEH_WHEEL_RF:
									frontRearWheel = VEH_WHEEL_RR;
									break;
								case VEH_WHEEL_RR:
									frontRearWheel = VEH_WHEEL_RF;
									break;
								default:
									break;
								}

								bool shouldTrigger = true;

								if(g_ReflectionsAudioEntity.IsStuntTunnelMaterial(GetMainWheelMaterial()))
								{
									if(state->fwdSpeedAbs > 5.f)
									{
										shouldTrigger = false;
									}
								}

								if(shouldTrigger && frontRearWheel >= 0)
								{
									if(currentTime - m_LastBottomOutTime[frontRearWheel - VEH_WHEEL_FIRST_WHEEL] < 100)
									{
										if(!m_Vehicle->InheritsFromAmphibiousQuadBike() || ((CAmphibiousQuadBike*)m_Vehicle)->IsWheelTransitionFinished())
										{
											GetCollisionAudio().TriggerJumpLand(5.f);

											if(!hydraulicsActive && !IsToyCar())
											{
												GetCollisionAudio().TriggerJumpLandScrape();
											}

											twoWheelLanding = true;
										}
									}
								}
							}
						}
					}
				}
			}

			if(m_SuspensionSounds[i])
			{
				if(compressionChange*m_LastCompressionChange[i] < 0.f)
				{
					// suspension has changed direction - cancel previous sound
					m_SuspensionSounds[i]->StopAndForget();
				}
				else
				{
					// Disabling volume update so that we hear the whole one shot rather than it being played then immediately muted
					//m_SuspensionSounds[i]->SetRequestedVolume(cinematicOffsetVol + audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio)));
					Vector3 pos;
					GetCachedWheelPosition(i, pos, *state);
					m_SuspensionSounds[i]->SetRequestedPosition(pos);
				}
			}

			if(absCompressionChange > m_CarAudioSettings->MinSuspCompThresh)
			{
				if(!m_SuspensionSounds[i])
				{
					// trigger a suspension sound
					u32 soundHash;
                    audSoundInitParams initParams;
                    initParams.UpdateEntity = true;
                    initParams.EnvironmentGroup = m_EnvironmentGroup;
                    GetCachedWheelPosition(i, initParams.Position, *state);
                    initParams.Volume = cinematicOffsetVol + audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio));

                    if(hydraulicsActive)
                    {
                        audSoundSet hydraulicSuspensionSoundSet;
                        hydraulicSuspensionSoundSet.Init(GetVehicleHydraulicsSoundSetName());

                        if(hydraulicSuspensionSoundSet.IsInitialised())
                        {
                            audMetadataRef hydraulicRef = hydraulicSuspensionSoundSet.Find(compressionChange > 0 ? ATSTRINGHASH("Hydraulic_Suspension_Down", 0xBED584C) : ATSTRINGHASH("Hydraulic_Suspension_Up", 0x95B33F3B));

                            if(hydraulicRef != g_NullSoundRef)
                            {
                                CreateAndPlaySound_Persistent(hydraulicRef, &m_SuspensionSounds[i], &initParams);
                            }
                        }
                    }
                    
                    if(!m_SuspensionSounds[i])
                    {
					    if(compressionChange > 0)
					    {
						    soundHash = m_CarAudioSettings->SuspensionDown;
					    }
					    else
					    {
						    soundHash = m_CarAudioSettings->SuspensionUp;
					    }

					
					    CreateAndPlaySound_Persistent(soundHash, &m_SuspensionSounds[i], &initParams);
                    }
				}

#if GTA_REPLAY
				if(!CReplayMgr::IsEditModeActive())
#endif
				{
					// For big suspension compression on stairs, trigger a tyre bump
					if(compressionChange > 0 && wheel->GetTyreHealth() > 0.0f && !IsMonsterTruck() && !IsToyCar() && !HydraulicsModifiedRecently())
					{
						if(m_OnStairs || m_WasOnStairs)
						{
							u32 soundName = m_Vehicle->InheritsFromBike() ? ATSTRINGHASH("TyreBumpStairsBike", 0xB0B6D726) : ATSTRINGHASH("TyreBumpStairs", 0x9050DECD);
							audSoundInitParams initParams;
							initParams.UpdateEntity = true;
							initParams.EnvironmentGroup = m_EnvironmentGroup;
							GetCachedWheelPosition(i, initParams.Position, *state);							
							CreateAndPlaySound(sm_ExtrasSoundSet.Find(soundName), &initParams);

#if GTA_REPLAY
							if(CReplayMgr::ShouldRecord())
							{
								REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_ExtrasSoundSet.GetNameHash(), soundName, &initParams, m_Vehicle));
							}
#endif
						}
					}
				}
			}
		}

		if(twoWheelLanding && !hydraulicsActive && !IsToyCar() && !IsGoKart())
		{
			if(!m_Vehicle->InheritsFromAmphibiousQuadBike() || ((CAmphibiousQuadBike*)m_Vehicle)->IsWheelTransitionFinished())
			{
				// Two wheel landings always get a fair amount of volume so that they play when flipping the car after being upside down
				f32 volumeLin = Max(0.75f, state->fwdSpeedRatio);
				audMetadataRef clatterSoundRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("Detailed", 0xDCE667E1));
				TriggerClatter(clatterSoundRef, volumeLin);
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Convert the car from dummy
// -------------------------------------------------------------------------------
void audCarAudioEntity::ConvertFromDummy()
{
	m_vehicleEngine.ConvertFromDummy();
	m_ThrottleBlipTimer = 0;
}

// -------------------------------------------------------------------------------
// Convert the car to dummy
// -------------------------------------------------------------------------------
void audCarAudioEntity::ConvertToDummy()
{
	m_vehicleEngine.ConvertToDummy();

	// These sounds are all updated in UpdateBrakes, which is for real cars only
	StopAndForgetSoundsContinueUpdating(m_DiscBrakeSqueel, m_BrakeReleaseSound, m_WindFastSound, m_OffRoadRumbleSound, m_OffRoadRumblePulse, m_WooWooSound);
	for(u32 loop = 0; loop < NUM_VEH_CWHEELS_MAX; loop++)
	{
		StopAndForgetSoundsContinueUpdating(m_WheelGrindLoops[loop], m_TyreDamageLoops[loop]);
	}

	if(m_RevsBlipSound)
	{
		m_RevsBlipSound->StopAndForget();
		SetEntityVariable(ATSTRINGHASH("fakethrottle", 0xEB27990), 0.0f);
		SetEntityVariable(ATSTRINGHASH("fakerevs", 0xCEB98BEB), 0.0f);
		SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), 0.0f);
	}
}

// -------------------------------------------------------------------------------
// Convert the car to super dummy
// -------------------------------------------------------------------------------
void audCarAudioEntity::ConvertToSuperDummy()
{
	ConvertToDummy();
	StopAndForgetSounds(m_DamageTyreScrapeSound, m_DamageTyreScrapeTurnSound, m_HydraulicSuspensionInputLoop, m_HydraulicSuspensionAngleLoop);
	StopAndForgetSounds(m_WaterTurbulenceSound, m_BankSpray, m_GliderSound, m_SpecialFlightModeSound, m_ToyCarEngineLoop);	

	for(u32 loop = 0; loop < g_NumDamageLayersPerCar; loop++)
	{
		if(m_EngineDamageLayers[loop])
		{
			m_EngineDamageLayers[loop]->StopAndForget(true);
		}
	}

	for(u32 loop = 0; loop < NUM_VEH_CWHEELS_MAX; loop++)
	{
		StopAndForgetSounds(m_WheelOffRoadSounds[loop], m_SuspensionSounds[loop]);
		m_WheelOffRoadSoundHashes[loop] = g_NullSoundHash;
	}
}

// -------------------------------------------------------------------------------
// Convert the car to disabled
// -------------------------------------------------------------------------------
void audCarAudioEntity::ConvertToDisabled()
{
	ConvertToSuperDummy();

	// Stop the engine cooling sounds
	m_vehicleEngine.ConvertToDisabled();
	StopAndForgetSounds(m_SubmarineTransformSound);
}

// -------------------------------------------------------------------------------
// Populate the car variables struct
// -------------------------------------------------------------------------------
void audCarAudioEntity::PopulateCarAudioVariables(audVehicleVariables* state)
{
	PF_FUNC(PopulateVariables);
	CTransmission* transmission = &m_Vehicle->m_Transmission;

	if(m_IsInAmphibiousBoatMode && m_Vehicle->GetSecondTransmission())
	{
		transmission = m_Vehicle->GetSecondTransmission();
	}

	naAssertf(m_Vehicle, "m_vehicle must be valid, about to access a null ptr...");
	f32 revsSmoothIncreaseScalar = 1.0f + (m_CarAudioSettings->AdditionalRevsIncreaseSmoothing * 0.01f);
	f32 revsSmoothDecreaseScalar = 1.0f;

	u32 timeSinceGearChange = 0u;
	if(m_vehicleEngine.HasBeenInitialised())
	{
		timeSinceGearChange = state->timeInMs - m_vehicleEngine.GetTransmission()->GetLastGearChangeTimeMs();
	}

#if __BANK
	if(audNorthAudioEngine::GetMetadataManager().IsRAVEConnected())
	{
		SetupVolumeCones();
	}
#endif // __BANK

	state->pitchRatio = 1.0f;
	state->gear = transmission->GetGear();
	state->gasPedal = Abs(transmission->GetThrottle());
	state->clutchRatio = transmission->GetClutchRatio();

	if(!m_IsFocusVehicle && state->fwdSpeedAbs > 1.0f)
	{
		state->gasPedal = 1.0f;
	}

	if(m_Vehicle->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE && state->fwdSpeedAbs > 0.0f)
	{
		state->gasPedal = 1.0f;
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		state->rawRevs = m_ReplayRevs;
	}
	else
#endif
	{
		if(m_IsInAmphibiousBoatMode)
		{
			if(m_Vehicle->GetDriver() && (!m_Vehicle->GetDriver()->IsPlayer() || g_TreatAllBoatsAsNPCs) && !IsSubmersible() && m_Vehicle->m_nVehicleFlags.bEngineOn)
			{
				// AI controlled boat - fake the revs based on fwd speed
				if(!m_Vehicle->GetIsJetSki() || state->fwdSpeed < 2.5f || m_Vehicle->m_nVehicleFlags.bIsRacing)
				{
					state->rawRevs = Clamp(state->fwdSpeed * 0.1f, 0.2f, 0.85f);
					m_FakeOutOfWaterTimer = 0.0f;
					m_FakeOutOfWater = false;
				}
				else
				{
					f32 fakeRevs = Clamp(state->fwdSpeed * 0.14f, 0.2f, 0.7f);
					f32 baseFakeRevs = Max(fakeRevs - audEngineUtil::GetRandomNumberInRange(g_FakeRevsOffsetBelow * 0.5f, g_FakeRevsOffsetBelow), 0.2f);
					f32 maxFakeRevs = Min(fakeRevs + audEngineUtil::GetRandomNumberInRange(g_FakeRevsOffsetAbove * 0.5f, g_FakeRevsOffsetAbove), 0.8f);

					f32 fakeRevsTarget = m_WasHighFakeRevs? maxFakeRevs : baseFakeRevs;
					m_FakeRevsSmoother.SetBounds(baseFakeRevs, maxFakeRevs);
					state->rawRevs = m_FakeRevsSmoother.CalculateValue(fakeRevsTarget, state->timeInMs);

					if(m_WasHighFakeRevs)
					{
						state->gasPedal = 1.0f;
					}
					else
					{
						state->gasPedal = 0.0f;
					}

					if(state->rawRevs == fakeRevsTarget)
					{
						m_FakeRevsHoldTime -= fwTimer::GetTimeStep();

						if(m_FakeRevsHoldTime < 0.0f)
						{
							if(m_WasHighFakeRevs)
							{
								m_WasHighFakeRevs = false;
								m_FakeRevsHoldTime = audEngineUtil::GetRandomNumberInRange(g_FakeRevsMinHoldTimeMin, g_FakeRevsMinHoldTimeMax);
							}
							else
							{
								m_WasHighFakeRevs = true;
								m_FakeRevsHoldTime = audEngineUtil::GetRandomNumberInRange(g_FakeRevsMaxHoldTimeMin, g_FakeRevsMaxHoldTimeMax);
							}

							m_FakeRevsSmoother.SetRates((1.0f/g_FakeRevsIncreaseRateBase) * audEngineUtil::GetRandomNumberInRange(1.0f, g_FakeRevsIncreaseRateScale), (1.0f/g_FakeRevsDecreaseRateBase) * audEngineUtil::GetRandomNumberInRange(1.0f, g_FakeRevsDecreaseRateScale));
						}
					}

					if(state->fwdSpeed > 5.0f && m_DistanceFromListener2LastFrame > g_FakeWaveHitDistanceSq)
					{
						m_FakeOutOfWaterTimer -= fwTimer::GetTimeStep();

						if(m_FakeOutOfWaterTimer < 0.0f)
						{
							m_FakeOutOfWater = !m_FakeOutOfWater;

							if(m_FakeOutOfWater)
							{
								m_FakeOutOfWaterTimer = audEngineUtil::GetRandomNumberInRange(g_FakeOutOfWaterMinTime, g_FakeOutOfWaterMaxTime);
							}
							else
							{
								m_FakeOutOfWaterTimer = audEngineUtil::GetRandomNumberInRange(g_FakeInWaterMinTime, g_FakeInWaterMaxTime);
							}
						}
					}
					else
					{
						m_FakeOutOfWaterTimer = 0.0f;
						m_FakeOutOfWater = false;
					}
				}
			}
			else
			{
				state->rawRevs = F32_VerifyFinite(transmission->GetRevRatio());
				m_FakeOutOfWaterTimer = 0.0f;
				m_FakeOutOfWater = false;
			}

			// Don't do this on submarine cars, mutes engine if you beach them on dry land
			if(m_Vehicle->GetDriver() && !m_Vehicle->InheritsFromSubmarineCar())
			{
				f32 filterFactor = 0.2f;
				f32 desiredRevsMultiplier = 1.0f;

				if(m_OutOfWaterTimer > 0.0f)
				{
					desiredRevsMultiplier = audBoatAudioEntity::sm_BoatAirTimeToRevsMultiplier.CalculateValue(m_OutOfWaterTimer);
					desiredRevsMultiplier *= Min(state->fwdSpeed * 0.5f, 1.0f);
				}

				m_OutOfWaterRevsMultiplier = (desiredRevsMultiplier * filterFactor) + (m_OutOfWaterRevsMultiplier * (1.0f - filterFactor));
				state->rawRevs *= m_OutOfWaterRevsMultiplier;
			}

			state->granularEnginePitchOffset = static_cast<s32>(audBoatAudioEntity::sm_BoatAirTimeToPitch.CalculateValue(m_OutOfWaterTimer));

			REPLAY_ONLY(m_ReplayRevs = state->rawRevs;)
		}
		else
		{
			f32 loadRevsMult = 1.0f;
			f32 desiredRawRevs = 0.0f;
			u32 gearChangeSmoothingTimeMS = m_CarAudioSettings->AdditionalGearChangeSmoothingTime;

			if(state->gear > 1 && timeSinceGearChange < gearChangeSmoothingTimeMS)
			{
				revsSmoothIncreaseScalar += ((m_CarAudioSettings->AdditionalGearChangeSmoothing * 0.01f) * Max(1.0f - (timeSinceGearChange/(f32)gearChangeSmoothingTimeMS), 0.0f));
			}

			// Vehicle is under load and moving
			if(transmission->IsEngineUnderLoad() && state->gasPedal > 0.0f && state->fwdSpeedAbs > 0.1f)
			{
				// Scale up the revs - more in lower gears
				f32 gearFactor = Max(1.0f - transmission->GetGear()/4.0f, 0.0f);
				loadRevsMult = 1.0f + (0.25f * Min(state->fwdSpeedRatio/0.02f, 1.0f) * gearFactor);
				desiredRawRevs = Min((F32_VerifyFinite(transmission->GetRevRatio()) * loadRevsMult), 1.0f);

				// Apply a second revs value directly linked to throttle (but reduced as the gears increase)
				if(m_IsPlayerDrivingVehicle)
				{
					f32 gasPedalGearScalar = 1.0f - ((transmission->GetGear() - 1) * 0.2f);
					gasPedalGearScalar *= transmission->GetClutchRatio();

					// Take the max of the two values and smooth out the increase a bit to prevent revs jumping too quickly
					desiredRawRevs = Max(state->gasPedal * gasPedalGearScalar, desiredRawRevs);
				}

				revsSmoothIncreaseScalar += g_UnderLoadRevsSmootherScalar;
			}
			else
			{
				desiredRawRevs = Clamp((F32_VerifyFinite(transmission->GetRevRatio()) + m_MaxSpeedRevsAddition + (0.075f * m_LeanRatio)), CTransmission::ms_fIdleRevs, 1.0f);

				// When coming out of sports car revs we might be at a totally different rev ratio, so smooth out a bit
				if(!m_IsPlayerVehicle && !m_SportsCarRevsSound && m_SportsCarRevsApplyFactor > 0.0f)
				{
					if(m_WasSportsCarRevsCancelled)
					{
						revsSmoothIncreaseScalar = 2.0f;
						revsSmoothDecreaseScalar = 2.0f;
					}
					else
					{
						revsSmoothIncreaseScalar = 3.0f;
						revsSmoothDecreaseScalar = 3.0f;
					}
				}
			}

			if(m_Vehicle->IsRocketBoosting())
			{
				state->gasPedal = 1.f;
				desiredRawRevs = 1.f;
			}

			if(ShouldForceEngineOff())
			{
				state->gasPedal = 0.f;
				desiredRawRevs = 0.f;
			}

			f32 filterFactor = 1.0f;

			if(m_Vehicle->GetOwnedBy()==ENTITY_OWNEDBY_CUTSCENE)
			{
				filterFactor = 0.1f;
			}
			else if(state->numWheelsTouchingFactor == 1.0f && 
				desiredRawRevs > m_RevsLastFrame && 
				transmission->GetGear() > 2 &&
				m_RevsLastFrame < 0.98f)
			{
				if(m_Vehicle->GetHandBrake())
				{
					filterFactor = 0.2f;
				}
				else
				{
					filterFactor = 0.1f;
				}
			}

			state->rawRevs = (m_RevsLastFrame * (1.0f - filterFactor)) + (filterFactor * desiredRawRevs);
			m_RevsLastFrame = state->rawRevs;

			REPLAY_ONLY(m_ReplayRevs = state->rawRevs;)

			if(m_CarAudioSettings->EngineType == HYBRID && transmission->GetNumGears() == 1)
			{
				state->rawRevs = 0.2f + (0.8f * state->fwdSpeedRatio);
			}

			if(!m_IsPlayerVehicle && m_Vehicle->GetDriver() && !m_Vehicle->GetDriver()->IsNetworkPlayer() && state->rawRevs > CTransmission::ms_fIdleRevs && !CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle) && m_Vehicle->GetOwnedBy()!=ENTITY_OWNEDBY_CUTSCENE)
			{
				state->rawRevs = Clamp(state->rawRevs * g_NPCVehicleRevsScalar, 0.0f, 1.0f);
			}
		}		
	}

#if __BANK
	if(g_EngineVolumeCaptureActive && (sm_VolumeCaptureState == State_CapturingAccel || sm_VolumeCaptureState == State_CapturingAccelModded))
	{
		state->gasPedal = sm_VolumeCaptureThrottle;
		state->rawRevs = sm_VolumeCaptureRevs;
		state->gear = 1;
	}
#endif

	f32 entityVariableThrottle = 0.0f;
	f32 entityVariableRevs = 0.0f;
	bool useFakeEngine = false;

	if(HasEntityVariableBlock())
	{
		entityVariableThrottle = GetEntityVariableValue(ATSTRINGHASH("fakethrottle", 0xEB27990));
		entityVariableRevs = GetEntityVariableValue(ATSTRINGHASH("fakerevs", 0xCEB98BEB));

		// Reduce this each frame so that that the behaviour gets canceled even if a sound forgets to do so
		f32 fakeEngineFactor = GetEntityVariableValue(ATSTRINGHASH("usefakeengine", 0x91DF7F97));
		SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), Clamp(fakeEngineFactor - 0.1f, 0.0f, 1.0f));
		useFakeEngine = fakeEngineFactor > 0.1f;

		if(useFakeEngine && m_WasPlayingStartupSound)
		{
			m_FakeEngineStartupRevs = true;
		}
	}

	if(useFakeEngine)
	{
		state->gasPedal = entityVariableThrottle;
		state->rawRevs = entityVariableRevs;
	}

	// Reversing flag (not the same as being in reverse gear) can be used to determine whether the vehicle is 
	// intentionally moving backwards, as opposed to going backwards because it did a big drift/spin
	if(state->gear == 0 && state->fwdSpeed < 0.0f && state->numWheelsTouching > 0 && !m_Vehicle->GetHandBrake())
	{
		m_IsReversing = true;
	}
	else if(m_IsReversing && ((state->fwdSpeed >= 0.0f && state->gear > 0) || state->numWheelsTouching == 0))
	{
		m_IsReversing = false;
	}

	if(m_IsReversing)
	{
		state->rawRevs = Clamp(state->rawRevs, 0.0f, 0.8f);
	}

	if(m_vehicleEngine.IsAutoShutOffSystemEngaged())
	{
		state->rawRevs = CTransmission::ms_fIdleRevs;
	}

	if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE && !useFakeEngine)
	{
		if(state->gear == 0)
		{
			// don't let bikes rev up in reverse
			state->rawRevs = CTransmission::ms_fIdleRevs;
			state->gasPedal = 0.f;
		}
	}
#if __BANK
	if(g_OverrideSim && ((m_Vehicle == g_AudioDebugEntity) || (!g_AudioDebugEntity && m_IsPlayerVehicle)))
	{
		state->gasPedal = g_OverriddenThrottle;
		state->rawRevs = g_OverriddenRevs;
	}
#endif

	state->engineConeAtten = m_HasInitialisedVolumeCones? m_EngineVolumeCone.ComputeAttenuation(m_Vehicle->GetMatrix()) : 0.0f;
	state->exhaustConeAtten = m_HasInitialisedVolumeCones? m_ExhaustVolumeCone.ComputeAttenuation(m_Vehicle->GetMatrix()) : 0.0f;

	if(g_RevsBoostEffectEnabled && state->gear == transmission->GetNumGears())
	{
		// Fade out the engine coning as we reach top speed
		state->engineConeAtten *= (1.0f - m_MaxSpeedPitchFactor);
	}

#if __BANK
	if(g_DisableConing)
	{
		state->engineConeAtten = 0.0f;
		state->exhaustConeAtten = 0.0f;
	}
#endif

	// if in reverse then use the wheel brake force since the brake pedal isnt active
	if(state->gear == 0)
	{
		f32 force = 0.0f;

		if(m_Vehicle->GetDriver() && !m_Vehicle->GetDriver()->IsBeingJackedFromVehicle())
		{
			for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
			{
				CWheel *wheel = m_Vehicle->GetWheel(i);
				force = Max(force, wheel->GetBrakeForce());
			}
		}
		
		state->brakePedal = Clamp(force*2.0f, 0.0f, 1.0f);
		state->speedForBrakes = -1.5f * state->fwdSpeed;
	}
	else
	{
		state->speedForBrakes = state->fwdSpeed;
		state->brakePedal = m_Vehicle->GetBrake();
	}
	if(Abs(state->fwdSpeed) < 0.05f)
	{
		state->speedForBrakes = 0.0f;
	}

	if(m_Vehicle->m_nVehicleFlags.bIsAsleep)
	{
		// override sim values if physics is sleeping
		state->speedForBrakes = 0.0f;
	}

	if(IsPlayerDrivingVehicle())
	{
		m_ThrottleSmoother.SetRates(0.005f,0.005f);
	}
	else
	{
		m_ThrottleSmoother.SetRates(0.0025f,0.0025f);
	}

	// Increase revs a bit for going up hill
	if(!m_IsPlayerVehicle || HasHeavyRoadNoise())
	{
		const f32 currentSlope = m_Vehicle->GetTransform().GetB().GetZf();// Positive = uphill.
		const f32 slopeGasMultiplierForward	= 1.0f + (0.2f * Clamp(currentSlope/0.2f, 0.0f, 1.0f)) * Clamp(state->fwdSpeedRatio * 20.0f, 0.0f, 1.0f) * state->numWheelsTouchingFactor;
		state->rawRevs *= slopeGasMultiplierForward;
	}
	
	if(m_WasPlayingStartupSound && !m_StartUpRevsSound)
	{
		if(HasEntityVariableBlock())
		{
			SetEntityVariable(ATSTRINGHASH("fakethrottle", 0xEB27990), 0.0f);
			SetEntityVariable(ATSTRINGHASH("fakerevs", 0xCEB98BEB), 0.0f);
			SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), 0.0f);
		}
		
		m_WasPlayingStartupSound = false;
	}

	if(m_vehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_STARTING)
	{
		// if the player starts acelerating then interrupt starting behaviour
		if((m_Vehicle->GetThrottle()>0.1f || state->fwdSpeed<-0.1f) && m_IsPlayerVehicle)
		{
			// force the throttle to be 0 so it'll smooth up
			m_ThrottleSmoother.Reset();
			m_ThrottleSmoother.CalculateValue(0.0f, state->timeInMs);
			StopAndForgetSounds(m_StartUpRevsSound);
		}
		else
		{
			if(m_vehicleEngine.IsGranularEngineActive())
			{
				m_RevsSmoother.SetRates(g_RevMaxIncreaseRate / 1000.f,g_RevMaxDecreaseRate / 1000.f);

				if(!m_StartUpRevsSound && !m_HasPlayedStartupSequence)
				{
					// Don't kick off the fake revs until the real ones have started
					if(m_vehicleEngine.GetGranularEngine()->AreAnySoundsPlaying())
					{
						if(HasEntityVariableBlock())
						{
							SetEntityVariable(ATSTRINGHASH("fakethrottle", 0xEB27990), 0.0f);
							SetEntityVariable(ATSTRINGHASH("fakerevs", 0xCEB98BEB), 0.0f);
						}

						audMetadataRef startupRevsRef = g_NullSoundRef;
						
						if(m_CarAudioSettings->EngineType == COMBUSTION)
						{
							startupRevsRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(m_CarAudioSettings->StartupRevs);
						}

						// If we have an explicit startup sound, use this instead. Its a one-time use thing though, so forget it immediately afterwards
						if(m_ScriptStartupRevsSoundRef != g_NullSoundRef)
						{
							startupRevsRef = m_ScriptStartupRevsSoundRef;
							m_ScriptStartupRevsSoundRef = g_NullSoundRef;
						}

						CreateAndPlaySound_Persistent(startupRevsRef, &m_StartUpRevsSound);
						m_HasPlayedStartupSequence = true;
						m_FakeEngineStartupRevs = false;

						if(m_StartUpRevsSound)
						{
							m_WasPlayingStartupSound = true;
						}
					}
				}
			}
			else if(m_CarAudioSettings && m_CarAudioSettings->EngineType != ELECTRIC)
			{
				// seriously smooth revs if we're starting the engine
				m_RevsSmoother.SetRates(1.5f / 1000.f,0.45f / 1000.f);

				u32 engineOnDuration = fwTimer::GetTimeInMilliseconds() - m_EngineStartTime;
				if(engineOnDuration < 5 || engineOnDuration > (u32)(g_EngineStartDuration/3.0f))
				{
					state->rawRevs = CTransmission::ms_fIdleRevs;
				}
				else
				{
					state->rawRevs = 0.75f;
				}
			}
		}
	}
	else if(m_vehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_STOPPING)
	{		
		if(ShouldForceEngineOff())
		{
			m_RevsSmoother.SetRates(g_RevMaxIncreaseRate / 1000.f, g_RevMaxForceStopDecreaseRate / 1000.f);
		}
		// seriously smooth revs if we're stopping the engine
		else
		{
			m_RevsSmoother.SetRates(0.3f / 1000.f, 0.3f / 1000.f);
		}
		
		state->rawRevs = 0.0f;
		m_HasPlayedStartupSequence = false;

		// Kill the startup revs immediately if we're just playing fake engine blips. For cars with 
		// bespoke startups, keep these playing as it sounds odd to cut them short
		if(m_FakeEngineStartupRevs)
		{
			StopAndForgetSounds(m_StartUpRevsSound);
		}
	}
	else
	{
		// hold the revs down until we trigger the backfire sfx/vfx
		if(m_vehicleEngine.IsMisfiring())
		{
			//m_RevsSmoother.SetRates(1.0f / 1000.f,0.7f / 1000.f);
			//state->rawRevs = CTransmission::ms_fIdleRevs;
			state->forcedThrottle = 0.0f;
		}
		else
		{
			revsSmoothDecreaseScalar += (m_CarAudioSettings->AdditionalRevsDecreaseSmoothing * 0.01f);
			revsSmoothDecreaseScalar *= g_RevsSmoothingCurve.CalculateValue(state->rawRevs);			

			// Add extra revs smoothing following a stunt boost			
			const u32 time = fwTimer::GetTimeInMilliseconds();
			const f32 stuntBoostRatio = 1.f - Clamp((time - m_LastStuntRaceSpeedBoostTime) / (f32)g_StuntSpeedBoostDecelSmoothingDuration, 0.f, 1.f);
			revsSmoothDecreaseScalar += g_StuntSpeedBoostDecelSmoothing * stuntBoostRatio;

			// Prevent divide by zero
			if(revsSmoothDecreaseScalar < 1.0f)
			{
				revsSmoothDecreaseScalar = 1.0f;
			}

			if(state->gear == 0 && m_IsReversing && !m_SportsCarRevsSound)
			{
				m_RevsSmoother.SetRates(0.0002f, g_RevMaxDecreaseRate / (1000.f * revsSmoothDecreaseScalar));
			}
			else
			{
				m_RevsSmoother.SetRates(g_RevMaxIncreaseRate / (1000.f * revsSmoothIncreaseScalar), g_RevMaxDecreaseRate / (1000.f * revsSmoothDecreaseScalar));
			}
		}
	}	

	// If the engine is off, revert to clutch pressed
	if(!m_Vehicle->IsEngineOn())
	{
		state->clutchRatio = 0.1f;
	}

	// randomly trigger gear box crunch/backfires if car is damaged
	const f32 engineHealth = ComputeEffectiveEngineHealth();
	state->engineDamageFactor = Clamp(1.0f - (engineHealth / CTransmission::GetEngineHealthMax()), 0.0f, 1.0f);

	s32 timeSinceLimiter = fwTimer::GetTimeInMilliseconds() - GetTimeSinceLastRevLimiter();

	// url:bugstar:6458027 - Manama2 - Rev limiter pops / vfx are playing as the vehicle revs when modified, before it reaches the top of the rev range
	// For cars with additional revs smoothing, we may hit the limiter quite a long while before the audio revs max out, so we want to delay the limiter 
	// so that eg. exhaust pops don't trigger too early
	const bool linkExhaustPopsToAudioRevs = g_LinkExhaustPopsToAudioRevs && m_CarAudioSettings->AdditionalRevsIncreaseSmoothing > 0;

	// Hitting the limiter starts fiddling with the throttle/pedal controls, which sounds rubbish. Just assume we're on max
	// throttle until it calms down again	
#if __BANK
	if(g_EngineVolumeCaptureActive && (sm_VolumeCaptureState == State_CapturingAccel || sm_VolumeCaptureState == State_CapturingAccelModded))
	{
		state->throttle = m_ThrottleSmoother.CalculateValue(sm_VolumeCaptureThrottle, state->timeInMs);
		state->throttleInput = sm_VolumeCaptureThrottle;
	}
	else
#endif
	// Stop the rev limiter effect as soon as we change down gears
	if((timeSinceLimiter < 100 && (!linkExhaustPopsToAudioRevs || m_RevsSmoother.GetLastValue() >= 1.f)) && timeSinceGearChange > 100 && !useFakeEngine && m_CarAudioSettings->EngineType != ELECTRIC)
	{
		// Cancel the limiter if we're hitting the brake or in reverse
		if((state->fwdSpeedRatio > 0.05f && (state->brakePedal > 0.0f || m_Vehicle->GetThrottle() == 0.0f)) || state->gear <= 0 || m_IsReversing)
		{
			if(!m_IsReversing)
			{
				state->rawRevs = 1.0f;
				state->throttle = m_ThrottleSmoother.CalculateValue(state->gasPedal, state->timeInMs);
			}
			else
			{
				state->throttle = m_ThrottleSmoother.CalculateValue(Max(state->gasPedal, Abs(m_Vehicle->GetThrottle())), state->timeInMs);
			}
						
			state->throttleInput = m_Vehicle->GetThrottle();
			state->onRevLimiter = false;
		}
		else
		{
			m_ThrottleSmoother.Reset();
			state->gasPedal = 1.0f;
			state->throttle = m_ThrottleSmoother.CalculateValue(state->gasPedal, state->timeInMs);
			state->throttleInput = 1.0f;
			state->rawRevs = 1.0f;

			if(m_DrowningFactor == 0.0f)
			{
				state->onRevLimiter = true;
			}
			else
			{
				state->onRevLimiter = false;
			}
		}
	}
	else
	{
		state->throttle = m_ThrottleSmoother.CalculateValue(state->gasPedal, state->timeInMs);
		state->throttleInput = m_Vehicle->GetThrottle();
		state->onRevLimiter = false;
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		state->onRevLimiter = m_ReplayRevLimiter;
		state->gasPedal = 1.0f;
		state->throttle = m_ThrottleSmoother.CalculateValue(state->gasPedal, state->timeInMs);
		state->throttleInput = 1.0f;
	}
#endif

	state->revs = F32_VerifyFinite(m_RevsSmoother.CalculateValue(state->rawRevs, state->timeInMs));
	if(m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STARTING && m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_STOPPING)
	{
		// clamp revs to idle minimum
		state->revs = Max(CTransmission::ms_fIdleRevs,state->revs);
	}

	if(m_vehicleEngine.IsGranularEngineActive())
	{
		state->carVolOffset = (m_GranularEngineSettings->MasterVolume/100.f);
	}
	else if(m_VehicleEngineSettings)
	{
		state->carVolOffset = (m_VehicleEngineSettings->MasterVolume/100.f);
	}

	state->exhaustThrottleAtten = (0.f - ((1.f - state->throttle) * g_audExhaustThrottleVolumeRange));
	state->engineThrottleAtten = (0.f - ((1.f - state->throttle) * g_audThrottleVolumeRange));

	if(state->revs > g_EnginePowerBand[state->gear])
	{
		state->enginePowerFactor = (g_EnginePowerBand[state->gear] - (state->revs - g_EnginePowerBand[state->gear])) / g_EnginePowerBand[state->gear];
	}
	else
	{
		state->enginePowerFactor = state->revs / g_EnginePowerBand[state->gear];
	}

	f32 outputCategoryVol = 0.0f;
	f32 environmentalLoudness = 0.0f;
	const f32 wetness = m_EngineEffectWetSmoother.GetLastValue();

	naAssertf(g_PlayerVolumeCategories[m_CarAudioSettings->VolumeCategory], "Vehicle volume category for player car %s is invalid, about to access a null ptr...", GetVehicleModelName());
	naAssertf(g_PedVolumeCategories[m_CarAudioSettings->VolumeCategory], "Vehicle volume category for ped car %s is invalid, about to access a null ptr...", GetVehicleModelName());

	// promote the player volume category to loud if its below that
	// NOTE: smaller category IDs are louder (loudest == 0)
	const u32 playerVolCategory = (g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::BoostPlayerCarVolume) ? Min((u32)m_CarAudioSettings->VolumeCategory,(u32)VEHICLE_VOLUME_LOUD):m_CarAudioSettings->VolumeCategory);
	state->playerVolCategory = playerVolCategory;

#if __BANK
	if(g_DisableVolumeCategory || g_EngineVolumeCaptureActive)
	{
		outputCategoryVol = 0.0f;
		m_EngineEffectWetSmoother.Reset();
	}
	else 
#endif
	{
		if(wetness == 0.f)
		{
			// fully dry; use ped volume
			outputCategoryVol = g_PedVolumeCategories[m_CarAudioSettings->VolumeCategory]->GetVolume();
		}
		else if(wetness == 1.f)
		{
			// fully wet; use player volume
			outputCategoryVol = g_PlayerVolumeCategories[playerVolCategory]->GetVolume();
		}
		else
		{
			// fading - interp between
			const f32 outputCategoryVolLinPlayer = audDriverUtil::ComputeLinearVolumeFromDb(g_PlayerVolumeCategories[playerVolCategory]->GetVolume());
			const f32 outputCategoryVolLinPed = audDriverUtil::ComputeLinearVolumeFromDb(g_PedVolumeCategories[m_CarAudioSettings->VolumeCategory]->GetVolume());
			outputCategoryVol = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(wetness, outputCategoryVolLinPed, outputCategoryVolLinPlayer));
		}

		if(m_IsPlayerVehicle)
		{
			environmentalLoudness = g_PlayerVolumeCategories[playerVolCategory]->GetEnvironmentalLoudness();
		}
		else
		{	
			environmentalLoudness = g_PedVolumeCategories[m_CarAudioSettings->VolumeCategory]->GetEnvironmentalLoudness();
		}
	}

	// environmental loudness gets scaled down by revs and throttle
	environmentalLoudness *= sm_EnvironmentalLoudnessThrottleCurve.CalculateValue(state->throttle);
	environmentalLoudness *= sm_EnvironmentalLoudnessRevsCurve.CalculateValue(state->revs);
	environmentalLoudness = Max(environmentalLoudness, m_SportsCarRevsApplyFactor);
	state->environmentalLoudness = environmentalLoudness;

	f32 enginePostSubmixVol = outputCategoryVol + state->carVolOffset;
	f32 exhaustPostSubmixVol = outputCategoryVol + state->carVolOffset;

	// In practice rev ratio always lies between 0.2 and 1.0, so scale up to a 0-1 range
	f32 revRatio = ((state->revs - 0.2f) * 1.25f);

	// Similarly, clutch ratio actually goes from 0.1 to 1.0, so scale up to a 0-1 range
	f32 clutchRatio = ((state->clutchRatio - 0.1f) * 1.11f);

	// B*2537551 fix - don't duck exhaust while reversing on certain vehicles. Please make
	// this configurable from metadata for future projects!
	bool duckExahustWhileReversing = true;
	const u32 vehicleModelName = GetVehicleModelNameHash();

	if(vehicleModelName == ATSTRINGHASH("BUCCANEER2", 0xC397F748) ||
	   vehicleModelName == ATSTRINGHASH("CHINO2", 0xAED64A63) ||
	   vehicleModelName == ATSTRINGHASH("SLAMVAN", 0x2B7F9DE3) ||
	   vehicleModelName == ATSTRINGHASH("BLAZER4", 0xE5BA6858) ||
	   vehicleModelName == ATSTRINGHASH("BLAZER5", 0xA1355F67) ||
	   vehicleModelName == ATSTRINGHASH("RATLOADER2", 0xDCE1D9F7) ||
	   vehicleModelName == ATSTRINGHASH("VOODOO", 0x779B4F2D))
	{
		duckExahustWhileReversing = false;
	}

	// Doesn't make sense on any bikes as the ped walks them backwards to reverse
	if(m_Vehicle->InheritsFromBike())
	{
		duckExahustWhileReversing = false;
	}

	f32 reversingExhaustVol = m_ReversingExhaustVolSmoother.CalculateValue(m_IsReversing && duckExahustWhileReversing && state->fwdSpeedAbs > 0.0f? -5.0f : 0.0f);	

	if(m_vehicleEngine.IsGranularEngineActive())
	{
		enginePostSubmixVol += (m_GranularEngineSettings->EngineVolume_PostSubmix * 0.01f);
		exhaustPostSubmixVol += (m_GranularEngineSettings->ExhaustVolume_PostSubmix * 0.01f);

		enginePostSubmixVol += revRatio * (m_GranularEngineSettings->EngineRevsVolume_PostSubmix * 0.01f);		
		exhaustPostSubmixVol += revRatio * (m_GranularEngineSettings->ExhaustRevsVolume_PostSubmix * 0.01f );

		if(!m_IsReversing)
		{
			// Quickly fade in clutch attenuation effect once player gains speed - don't want it triggering when stationary
			f32 clutchAttenuationApplyFactor = Clamp((state->fwdSpeedRatio - 0.05f) * 25.0f, 0.0f, 1.0f);
			enginePostSubmixVol += state->throttle * (m_GranularEngineSettings->EngineThrottleVolume_PostSubmix * 0.01f);
			enginePostSubmixVol += (m_GranularEngineSettings->EngineClutchAttenuation_PostSubmix * clutchAttenuationApplyFactor * 0.01f) * (1.0f - clutchRatio);
			exhaustPostSubmixVol += state->throttle * (m_GranularEngineSettings->ExhaustThrottleVolume_PostSubmix * 0.01f);
			exhaustPostSubmixVol += (m_GranularEngineSettings->ExhaustClutchAttenuation_PostSubmix * clutchAttenuationApplyFactor * 0.01f) * (1.0f - clutchRatio);
		}

		exhaustPostSubmixVol += reversingExhaustVol;
		enginePostSubmixVol += (m_GranularEngineSettings->UpgradedEngineVolumeBoost_PostSubmix/100.0f) * GetEngineUpgradeLevel();
		exhaustPostSubmixVol += (m_GranularEngineSettings->UpgradedExhaustVolumeBoost_PostSubmix/100.0f) * GetExhaustUpgradeLevel();

		f32 engineIdleBoostLin = Lerp(m_vehicleEngine.GetGranularEngine()->GetIdleVolumeScale(), 1.0f, audDriverUtil::ComputeLinearVolumeFromDb((m_GranularEngineSettings->EngineIdleVolume_PostSubmix/100.0f)));
		f32 exhaustIdleBoostLin = Lerp(m_vehicleEngine.GetGranularEngine()->GetIdleVolumeScale(), 1.0f, audDriverUtil::ComputeLinearVolumeFromDb((m_GranularEngineSettings->ExhaustIdleVolume_PostSubmix/100.0f)));

		enginePostSubmixVol += audDriverUtil::ComputeDbVolumeFromLinear(engineIdleBoostLin);
		exhaustPostSubmixVol += audDriverUtil::ComputeDbVolumeFromLinear(exhaustIdleBoostLin);
	} 
	else if(m_VehicleEngineSettings)
	{
		const f32 pedEngineFxComp = m_VehicleEngineSettings->NonPlayerFXComp * 0.01f;
		const f32 playerEngineFxComp = m_VehicleEngineSettings->FXCompensation * 0.01f;
		const f32 revsForCompensation = (m_IsPlayerVehicle?g_PlayerRevsCurve.CalculateValue(state->revs):g_PedRevsCurve.CalculateValue(state->revs));
		const f32 interpolatedFxComp = Lerp(wetness, pedEngineFxComp, playerEngineFxComp);
		const f32 engineFxCompensation = (revsForCompensation * interpolatedFxComp);
		const f32 throttleForCompensation = (m_IsPlayerVehicle?g_PlayerThrottleCurve.CalculateValue(state->throttle):g_PedThrottleCurve.CalculateValue(state->throttle));
		const f32 exhaustFxCompensation = throttleForCompensation * engineFxCompensation;

		enginePostSubmixVol += engineFxCompensation;
		exhaustPostSubmixVol += exhaustFxCompensation;

		enginePostSubmixVol += (m_VehicleEngineSettings->UpgradedEngineVolumeBoost_PostSubmix/100.0f) * GetEngineUpgradeLevel();
		exhaustPostSubmixVol += (m_VehicleEngineSettings->UpgradedExhaustVolumeBoost_PostSubmix/100.0f) * GetExhaustUpgradeLevel();		
	}

#if __BANK
	if(g_MutePlayerVehicle && m_IsPlayerVehicle)
	{
		enginePostSubmixVol = g_SilenceVolume;
		exhaustPostSubmixVol = g_SilenceVolume;
	}
#endif

	// Duck exhaust on backfires
	if(m_vehicleEngine.IsExhaustPopActive())
	{
		exhaustPostSubmixVol -= 3.0f;
	}

	if((m_StartUpRevsSound && useFakeEngine && entityVariableRevs > 0.0f) || m_vehicleEngine.GetStartupSound())
	{
		m_StartupRevsVolBoostSmoother.CalculateValue(revRatio, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	}
	else
	{
		m_StartupRevsVolBoostSmoother.CalculateValue(0.0f, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	}
	
	if(m_GranularEngineSettings)
	{
		enginePostSubmixVol += ((m_GranularEngineSettings->StartupRevsVolumeBoostEngine_PostSubmix * 0.01f) * m_StartupRevsVolBoostSmoother.GetLastValue());
		exhaustPostSubmixVol += ((m_GranularEngineSettings->StartupRevsVolumeBoostExhaust_PostSubmix * 0.01f) * m_StartupRevsVolBoostSmoother.GetLastValue());
	}
	else
	{
		enginePostSubmixVol += 3.0f * m_StartupRevsVolBoostSmoother.GetLastValue();
		exhaustPostSubmixVol += 3.0f * m_StartupRevsVolBoostSmoother.GetLastValue();
	}

	state->enginePostSubmixVol = enginePostSubmixVol + state->engineConeAtten;
	state->exhaustPostSubmixVol = exhaustPostSubmixVol + state->exhaustConeAtten;

	state->enginePostSubmixVol += g_EngineVolumeTrim;
	state->exhaustPostSubmixVol += g_ExhaustVolumeTrim;

	f32 rollOff = m_IsPlayerVehicle? m_CarAudioSettings->MaxRollOffScalePlayer : m_CarAudioSettings->MaxRollOffScaleNPC;
	f32 desiredRollOff = m_IsPlayerVehicle? g_BaseRollOffScalePlayer : g_BaseRollOffScaleNPC + (rollOff * Clamp(revRatio * revRatio * revRatio, 0.0f, 1.0f));
	f32 smoothedRollOff = m_RollOffSmoother.CalculateValue(desiredRollOff);

	if(sm_SportsCarRevsSettings)
	{
		if(m_SportsCarRevsSound)
		{
			m_SportsCarRevsApplyFactor += fwTimer::GetTimeStep() * sm_SportsCarRevsSettings->AttackTimeScalar;
		}
		else
		{
			m_SportsCarRevsApplyFactor -= fwTimer::GetTimeStep() * sm_SportsCarRevsSettings->ReleaseTimeScalar * (m_WasSportsCarRevsCancelled ? 2.0f : 1.0f);
		}

		m_SportsCarRevsApplyFactor = Clamp(m_SportsCarRevsApplyFactor, 0.0f, 1.0f);
		state->enginePostSubmixVol += (sm_SportsCarRevsSettings->EngineVolumeBoost * 0.01f * m_SportsCarRevsApplyFactor * state->revs);
		state->exhaustPostSubmixVol += (sm_SportsCarRevsSettings->ExhaustVolumeBoost * 0.01f * m_SportsCarRevsApplyFactor * state->revs);

		if(m_EnvironmentGroup)
		{
			m_EnvironmentGroup->SetReverbOverrideLarge((u8)(sm_SportsCarRevsSettings->LargeReverbSend * m_SportsCarRevsApplyFactor * state->revs));
			m_EnvironmentGroup->SetReverbOverrideMedium((u8)(sm_SportsCarRevsSettings->MediumReverbSend * m_SportsCarRevsApplyFactor * state->revs));
			m_EnvironmentGroup->SetReverbOverrideSmall((u8)(sm_SportsCarRevsSettings->SmallReverbSend * m_SportsCarRevsApplyFactor * state->revs));
		}

		state->rollOffScale = smoothedRollOff + (sm_SportsCarRevsSettings->RollOffBoost * m_SportsCarRevsApplyFactor);
	}
	else
	{
		state->rollOffScale = smoothedRollOff;
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		m_ReplayRevLimiter = state->onRevLimiter;
	}
#endif

#if __BANK
	if(m_Vehicle->GetStatus() == STATUS_PLAYER)
	{
		PF_SET(Revs, state->revs);
		PF_SET(RawRevs, state->rawRevs);
		PF_SET(Throttle, state->throttle);
		PF_SET(PowerFactor, state->enginePowerFactor);
		PF_SET(Gear, state->gear);
	}

	char vehicleDebug[256] = "";
	if(sm_ShouldDisplayEngineDebug )
	{
		if(m_Vehicle->GetStatus()==STATUS_PLAYER)
		{
			CVehicleModelInfo *model = (CVehicleModelInfo*)m_Vehicle->GetBaseModelInfo();
			const f32 engineHealth = ComputeEffectiveEngineHealth();
			sprintf(vehicleDebug, "%s h: %.1f rR:%.3f r:%.3f t:%.3f s:%d g:%d mv:%.2f nw:%d temp: %.2f", model->GetGameName(), engineHealth, state->rawRevs, state->revs, state->throttle, (u32)m_vehicleEngine.GetState(), state->gear, transmission->GetDriveMaxVelocity(), m_Vehicle->GetNumContactWheels(), m_Vehicle->m_EngineTemperature);
			grcDebugDraw::PrintToScreenCoors(vehicleDebug, 5,30);
			sprintf(vehicleDebug, "%f %f %f %f %f %f %f %f", state->engineConeAtten, state->exhaustConeAtten, state->engineThrottleAtten, state->exhaustThrottleAtten, state->carVolOffset, state->enginePowerFactor, state->pitchRatio,(state->revs - state->rawRevs) * fwTimer::GetInvTimeStep());
			grcDebugDraw::PrintToScreenCoors(vehicleDebug, 5, 10);
		}

		sprintf(vehicleDebug, "bs %f r %f rr %f t %f su %f", state->speedForBrakes, state->revs, state->rawRevs, state->throttle, m_StartupRevsVolBoostSmoother.GetLastValue());
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()) + Vector3(0.f,0.f,1.f), Color32(50,50,255), vehicleDebug);
	}

	if(g_DisableVolumeCategory && m_Vehicle->GetStatus() == STATUS_PLAYER)
	{
		if(CControlMgr::GetKeyboard().GetKeyDown(KEY_0, KEYBOARD_MODE_DEBUG, "engine test loop"))
		{
			if(!g_EngineReferenceLoop)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				CreateAndPlaySound_Persistent(ATSTRINGHASH("ENGINE_REFERENCE_LOOP", 0x68ACCBCB), &g_EngineReferenceLoop, &initParams);
			}
			if(g_EngineReferenceLoop)
			{
				u32 posIdx = (sm_ShouldFlipPlayerCones?AUD_VEHICLE_SOUND_ENGINE:AUD_VEHICLE_SOUND_EXHAUST);
				g_EngineReferenceLoop->SetUpdateEntity(true);
				g_EngineReferenceLoop->SetClientVariable(posIdx);
			}
		}
		else
		{
			if(g_EngineReferenceLoop)
			{
				g_EngineReferenceLoop->StopAndForget();
			}
		}
	}
#endif
}

void audCarAudioEntity::SetIsInCarModShop(bool inCarMod)
{
	if(inCarMod != IsInCarModShop())
	{
		audVehicleAudioEntity::SetIsInCarModShop(inCarMod);

		// Cancel any startup rev behaviour as we'll be playing revs on the way in/out of the mod shop
		if(m_vehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_STARTING)
		{
			m_vehicleEngine.ForceEngineState(audVehicleEngine::AUD_ENGINE_ON);

			if(m_StartUpRevsSound)
			{
				m_StartUpRevsSound->StopAndForget();
			}

			m_HasPlayedStartupSequence = true;
			m_WasPlayingStartupSound = false;
			m_FakeEngineStartupRevs = false;
		}
	}
}

// ----------------------------------------------------------------
// audCarAudioEntity CalculateIsInWater
// ----------------------------------------------------------------
bool audCarAudioEntity::CalculateIsInWater() const
{
#if GTA_REPLAY // For replay we set this from CPacketAmphQuadUpdate/CPacketAmphAutoUpdate
	if(CReplayMgr::IsEditModeActive())
	{	
		return m_ReplayIsInWater;
	}
#endif
	if(m_Vehicle->InheritsFromAmphibiousAutomobile())
	{
		CBuoyancyInfo* buoyancyInfo = m_Vehicle->m_Buoyancy.GetBuoyancyInfo(m_Vehicle);

		if(buoyancyInfo)
		{
			s8 numWaterSampleRows = buoyancyInfo->m_nNumBoatWaterSampleRows;
			s8 lastRowWaterSampleIndex = buoyancyInfo->m_nBoatWaterSampleRowIndices[numWaterSampleRows-1];
			s8 secondLastRowWaterSampleIndex = buoyancyInfo->m_nBoatWaterSampleRowIndices[numWaterSampleRows-2];
			s8 secondLastRowNumWaterSamples = lastRowWaterSampleIndex - secondLastRowWaterSampleIndex;
			s8 entryWaterSampleIndex = secondLastRowWaterSampleIndex + (secondLastRowNumWaterSamples)/2;
			return m_WaterSamples.IsSet(entryWaterSampleIndex);
		}	
	}

	return audVehicleAudioEntity::CalculateIsInWater();
}

// -------------------------------------------------------------------------------
// Update sports car revs
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateSportsCarRevs(audVehicleVariables* state)
{
	if(!sm_SportsCarRevsSettings)
	{
		return;
	}

	bool sportsCarRevsValid = (m_SportsCarRevsEnabled BANK_ONLY(|| g_ForceAllCarsPlaySportsRevs)) &&
							  !camInterface::GetCutsceneDirector().IsCutScenePlaying() && 
							  m_Vehicle->GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && 
							  m_Vehicle->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT &&
							  !m_Vehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer;

	if(m_SportsCarRevsSound && m_SportsCarRevsTriggerType != REVS_TYPE_DEBUG)
	{
		if(!sportsCarRevsValid ||
		  (m_SportsCarRevsTriggerType == REVS_TYPE_JUNCTION && state->fwdSpeedAbs < sm_SportsCarRevsSettings->JunctionStopSpeed) ||
		  (m_SportsCarRevsTriggerType == REVS_TYPE_PASSBY && state->fwdSpeedAbs < sm_SportsCarRevsSettings->PassbyStopSpeed))
		{
			StopAndForgetSounds(m_SportsCarRevsSound);
			m_WasSportsCarRevsCancelled = true;
		}
	}

	if(m_GranularEngineSettings && g_SportsCarRevsEnabled && sportsCarRevsValid)
	{
		f32 distanceFromListenerChangeRate = ((s32)m_DistanceFromListener2 - (s32)m_DistanceFromListener2LastFrame) * fwTimer::GetInvTimeStep();
		bool isApproachingListener = distanceFromListenerChangeRate < 0.0f;

		bool forceTrigger = false;
		SportCarRevsTriggerType triggerType = REVS_TYPE_MAX;
		m_WasWaitingAtLights |= ((m_Vehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_LIGHTS || m_Vehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GIVE_WAY) && state->fwdSpeedAbs < 0.1f);
		
		// If a car is waiting at some lights, trigger revs as it pulls away
		if(m_WasWaitingAtLights && (m_Vehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO || m_Vehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_NOT_ON_JUNCTION) && state->fwdSpeedAbs > sm_SportsCarRevsSettings->JunctionTriggerSpeed)
		{
			m_WasWaitingAtLights = false;

			if(m_DistanceFromListener2 > (sm_SportsCarRevsSettings->JunctionMinDistance * sm_SportsCarRevsSettings->JunctionMinDistance) && m_DistanceFromListener2 < (sm_SportsCarRevsSettings->JunctionMaxDistance * sm_SportsCarRevsSettings->JunctionMaxDistance))
			{
				triggerType = REVS_TYPE_JUNCTION;
			}
		}

		bool isDoingBurnout = static_cast<CAutomobile*>(m_Vehicle)->m_nAutomobileFlags.bBurnoutModeEnabled;
		m_WasDoingBurnout |= isDoingBurnout;

		if(m_WasDoingBurnout && !isDoingBurnout && state->fwdSpeedAbs > 0.75f && !m_Vehicle->InheritsFromBike())
		{
			forceTrigger = true;
			m_WasDoingBurnout = false;
			triggerType = REVS_TYPE_JUNCTION;
		}

		// Special case - clustered vehicles can trigger revs even if the player is driving a car
		if(triggerType == REVS_TYPE_MAX && !IsPlayerOnFootOrOnBicycle() && m_Vehicle->m_nVehicleFlags.bIsInCluster)
		{
			if(m_DistanceFromListener2 < sm_SportsCarRevsSettings->ClusterTriggerDistance && m_DistanceFromListener2LastFrame >= (sm_SportsCarRevsSettings->ClusterTriggerDistance * sm_SportsCarRevsSettings->ClusterTriggerDistance) && state->fwdSpeedAbs > sm_SportsCarRevsSettings->ClusterTriggerSpeed)
			{
				triggerType = REVS_TYPE_PASSBY;
			}
		}

		// If a car passes close by at speed, trigger some revs
		if(triggerType == REVS_TYPE_MAX)
		{
			if(m_Vehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_NOT_ON_JUNCTION || m_Vehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO)
			{
				// New approach - attempt to predict when the car is due to pass the player and trigger a bit earlier
				if(isApproachingListener)
				{
					if(state->fwdSpeedAbs > sm_SportsCarRevsSettings->PassbyTriggerSpeed)
					{
						if(m_DistanceFromListener2 > (sm_SportsCarRevsSettings->PassbyMinDistance * sm_SportsCarRevsSettings->PassbyMinDistance) && 
							m_DistanceFromListener2 < (sm_SportsCarRevsSettings->PassbyMaxDistance * sm_SportsCarRevsSettings->PassbyMaxDistance))
						{
							CPed* playerPed = FindPlayerPed();
							
							if(playerPed)
							{
								Vec3V playerPositionOnLine;
								Vector3 vehiclePos = VEC3V_TO_VECTOR3(m_Vehicle->GetVehiclePosition());
								fwGeom::fwLine positioningLine = fwGeom::fwLine(vehiclePos, vehiclePos + (m_CachedVehicleVelocity * 10.0f * sm_SportsCarRevsSettings->PassbyLookaheadTime));
								ScalarV distanceAlongLine = positioningLine.FindClosestPointOnLineV(playerPed->GetTransform().GetPosition(), playerPositionOnLine);

								if(IsLessThan(distanceAlongLine, ScalarV(0.1f)).Getb())
								{
									triggerType = REVS_TYPE_PASSBY;
								}
							}
						}
					}
				}
			}
		}

		if(m_SportsCarRevsRequested)
		{
			TriggerSportsCarRevs(m_SportsCarRevsTriggerType);
		}
		else if(triggerType != REVS_TYPE_MAX)
		{
			if(forceTrigger ||
			  (state->timeInMs > sm_LastSportsRevsTime + sm_SportsCarRevsSettings->MinTriggerTime && state->timeInMs - m_LastSportsCarRevsTime > 10000))
			{
				// Special case - clustered vehicles can trigger revs even if the player is driving a car
				if(!m_IsFocusVehicle && !audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior() && (IsPlayerOnFootOrOnBicycle() || m_Vehicle->m_nVehicleFlags.bIsInCluster))
				{
					bool playedRecently = false;

					if(!forceTrigger)
					{
						for(u32 loop = 0; loop < kSportsCarRevsHistorySize; loop++)
						{
							if(m_GranularEngineSettings &&
							   sm_SportsCarRevsSoundHistory[loop].soundName == m_GranularEngineSettings->EngineAccel &&
							   fwTimer::GetTimeInMilliseconds() < sm_SportsCarRevsSoundHistory[loop].time + sm_SportsCarRevsSettings->MinRepeatTime)
							{
								playedRecently = true;
								break;
							}
						}
					}

					if(!playedRecently)
					{
						// Avoid playing sports car revs when in a vehicle as we're low on granular engine and these use 'em up
						TriggerSportsCarRevs(triggerType);
						sm_LastSportsRevsTime = state->timeInMs;
					}
				}
			}
		}

#if __BANK
		if(g_DrawSportsRevsPassbyDebug)
		{
			CPed* playerPed = FindPlayerPed();

			if(playerPed)
			{
				Vec3V playerPositionOnLine;
				Vector3 vehiclePos = VEC3V_TO_VECTOR3(m_Vehicle->GetVehiclePosition());
				fwGeom::fwLine positioningLine = fwGeom::fwLine(vehiclePos, vehiclePos + (m_CachedVehicleVelocity * 10.0f * sm_SportsCarRevsSettings->PassbyLookaheadTime));
				grcDebugDraw::Line(positioningLine.m_start, positioningLine.m_end, Color32(255,0,0,255), Color32(255,0,0,255));
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(playerPositionOnLine), 1.f, Color32(0,255,0,255));
			}
		}

		if(g_DrawCarsPlayingSportsRevs)
		{
			bool drawDetails = true;
			bool playedRecently = false;

			for(u32 loop = 0; loop < kSportsCarRevsHistorySize; loop++)
			{
				if(m_GranularEngineSettings &&
				   sm_SportsCarRevsSoundHistory[loop].soundName == m_GranularEngineSettings->EngineAccel &&
				   fwTimer::GetTimeInMilliseconds() < sm_SportsCarRevsSoundHistory[loop].time + sm_SportsCarRevsSettings->MinRepeatTime)
				{
					playedRecently = true;
					break;
				}
			}

			if(m_SportsCarRevsSound)
			{
				Color32 colour = Color32(255,0,0,120);
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), Color32(0,255,0), Color32(0,255,0));
			}
			else if(m_WasDoingBurnout)
			{
				Color32 colour = Color32(255,0,255,120);
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
			}
			else if(m_WasWaitingAtLights && !playedRecently)
			{
				Color32 colour = Color32(0,255,0,120);
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
			}
			else if(m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_SPORTSCARREVSENABLED) == AUD_TRISTATE_TRUE)
			{
				Color32 colour = playedRecently? Color32(0,255,255,30) : Color32(0,0,255,60);
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
			}
			else
			{
				drawDetails = false;
			}

			char tempString[256];
			formatf(tempString, "Spd %.02f - Dst %.02f - Dst CR %.02f - JC %d", state->fwdSpeedAbs, sqrt((f32)state->distFromListenerSq), ((f32)m_DistanceFromListener2 - (f32)m_DistanceFromListener2LastFrame) * fwTimer::GetInvTimeStep(), m_Vehicle->GetIntelligence()->GetJunctionCommand());
			grcDebugDraw::Text(GetPosition(), Color32(255,255,255), tempString);
		}
#endif
	}
}

// -------------------------------------------------------------------------------
// Update synth light effect on the front of the Ruiner
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateWooWooSound(audVehicleVariables* state)
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD))
	{
		if(m_Vehicle->m_nVehicleFlags.bEngineOn && m_IsFocusVehicle && state->revs <= 0.21f && state->fwdSpeedAbs < 0.1f)
		{
			if(!m_WooWooSound)
			{
				audSoundSet soundset;
				const u32 soundsetName = ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811);
				const u32 soundName = ATSTRINGHASH("light_woo", 0x8396256B);

				if(soundset.Init(soundsetName))
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					CreateAndPlaySound_Persistent(soundset.Find(soundName), &m_WooWooSound, &initParams);
				}				
			}
		}
		else if(m_WooWooSound)
		{
			m_WooWooSound->StopAndForget(true);
		}
	}
}

// -------------------------------------------------------------------------------
// Update gliding
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateGliding(audVehicleVariables* state)
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("OPPRESSOR", 0x34B82784))
	{
		if(m_Vehicle->HasGlider() && m_Vehicle->GetGliderState() == CVehicle::GLIDING && state->numWheelsTouching == 0)
		{
			if(!m_GliderSound)
			{				
				audSoundSet soundset;
				const u32 soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_Gunrunning_Oppressor_Sounds", 0xC98DA953) : ATSTRINGHASH("DLC_Gunrunning_Oppressor_NPC_Sounds", 0x23FDA269);
				const u32 soundFieldName = ATSTRINGHASH("glide", 0xF9209D38);

				if(soundset.Init(soundsetName))
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					CreateAndPlaySound_Persistent(soundset.Find(soundFieldName), &m_GliderSound, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundsetName, soundFieldName, &initParams, m_GliderSound, GetOwningEntity()));
				}
			}
		}
		
		if(m_GliderSound && state->numWheelsTouching > 0)
		{
			m_GliderSound->StopAndForget(true);
		}
	}
}

// -------------------------------------------------------------------------------
// Update engine blips
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateBlips(audVehicleVariables* state)
{
#if __BANK
	if(g_DisableThrottleBlips)
	{
		if(m_RevsBlipSound)
		{
			m_RevsBlipSound->StopAndForget();
		}

		return;
	}
#endif

	bool areBlipsAllowed = false;

	if(!m_IsPlayerVehicle && !camInterface::GetCutsceneDirector().IsCutScenePlaying() && !m_StartUpRevsSound)
	{
		bool driverIsValid = false;
		
		CPed* driver = m_Vehicle->GetDriver();

		if(driver && !driver->ShouldBeDead() && !driver->IsNetworkPlayer())
		{
			CPedModelInfo* modelInfo = driver->GetPedModelInfo();

			if(modelInfo)
			{
				driverIsValid = modelInfo->IsMale();
			}
		}
		
		bool vehicleIsValid = m_Vehicle->m_Swankness > 3 && m_CarAudioSettings->VolumeCategory <= VEHICLE_VOLUME_NORMAL && !m_Vehicle->PopTypeIsMission();
		bool speedIsValid = state->fwdSpeedAbs < 0.1f;
		areBlipsAllowed = driverIsValid && vehicleIsValid && speedIsValid;
	}

#if __BANK
	areBlipsAllowed |= g_ForceEnableBlips;
#endif

	if(areBlipsAllowed)
	{
		if(m_Vehicle->m_nVehicleFlags.bBlipThrottleRandomly || m_ForceThrottleBlip BANK_ONLY(||g_ForceEnableBlips))
		{
			if(m_ThrottleBlipTimer > 3400 || m_ThrottleBlipTimer <= 0)
			{
				m_ThrottleBlipTimer = audEngineUtil::GetRandomNumberInRange(500,3400);
			}
		}
		else if(!m_HasBlippedSinceStopping)
		{
			if(m_Vehicle->IsHornOn())
			{
				if(m_ThrottleBlipTimer > 2000 || m_ThrottleBlipTimer <= 0)
				{
					m_ThrottleBlipTimer = audEngineUtil::GetRandomNumberInRange(1000,2000);
				}
			}
			else
			{
				if(m_ThrottleBlipTimer > 20000 || m_ThrottleBlipTimer <= 0)
				{
					m_ThrottleBlipTimer = audEngineUtil::GetRandomNumberInRange(10000,20000);
				}
			}
		}
		
		if(!m_RevsBlipSound && m_ThrottleBlipTimer > 0)
		{
			m_ThrottleBlipTimer -= fwTimer::GetTimeStepInMilliseconds();

			if(m_ThrottleBlipTimer <= 0)
			{
				CreateAndPlaySound_Persistent(ATSTRINGHASH("THROTTLE_BLIPS_RND", 0x1CF1A02A), &m_RevsBlipSound);
				m_HasBlippedSinceStopping = true;
			}
		}
	}
	else if(m_RevsBlipSound)
	{
		m_RevsBlipSound->StopAndForget();
		SetEntityVariable(ATSTRINGHASH("fakethrottle", 0xEB27990), 0.0f);
		SetEntityVariable(ATSTRINGHASH("fakerevs", 0xCEB98BEB), 0.0f);
		SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), 0.0f);
		m_ThrottleBlipTimer = 0;
	}

	if(state->fwdSpeedAbs > 5.0f)
	{
		m_HasBlippedSinceStopping = false;
	}

	m_ForceThrottleBlip = false;
}

// -------------------------------------------------------------------------------
// Check if the engine is upgraded
// -------------------------------------------------------------------------------
bool audCarAudioEntity::IsEngineUpgraded() const
{
	return m_Vehicle->GetVariationInstance().GetModIndex(VMT_ENGINE) != INVALID_MOD BANK_ONLY(|| g_ForceUpgradeEngine || (g_EngineVolumeCaptureActive && sm_VolumeCaptureState == State_CapturingAccelModded));
}

// -------------------------------------------------------------------------------
// Check if the transmission is upgraded
// -------------------------------------------------------------------------------
bool audCarAudioEntity::IsTransmissionUpgraded() const
{
	return m_Vehicle->GetVariationInstance().GetModIndex(VMT_GEARBOX) != INVALID_MOD BANK_ONLY(|| g_ForceUpgradeTransmission);
}

// -------------------------------------------------------------------------------
// Check if the exhaust is upgraded
// -------------------------------------------------------------------------------
bool audCarAudioEntity::IsExhaustUpgraded() const
{
	return m_Vehicle->GetVariationInstance().GetModIndex(VMT_EXHAUST) != INVALID_MOD BANK_ONLY(|| g_ForceUpgradeExhaust || (g_EngineVolumeCaptureActive && sm_VolumeCaptureState == State_CapturingAccelModded));
}

// -------------------------------------------------------------------------------
// Check if the turbo is upgraded
// -------------------------------------------------------------------------------
bool audCarAudioEntity::IsTurboUpgraded() const
{
	return m_Vehicle->GetVariationInstance().GetModIndex(VMT_TURBO) != INVALID_MOD BANK_ONLY(|| g_ForceUpgradeTurbo);
}

// -------------------------------------------------------------------------------
// Check if the radio is upgraded
// -------------------------------------------------------------------------------
bool audCarAudioEntity::IsRadioUpgraded() const
{
	// CHEBUREK is a special case as it has a speaker item linked to certain livery mods, we designate which using the audio apply factor
	if (GetVehicleModelNameHash() == ATSTRINGHASH("CHEBUREK", 0xC514AAE0) && m_Vehicle->GetVariationInstance().GetModIndex(VMT_LIVERY_MOD) != INVALID_MOD && m_Vehicle->GetVariationInstance().GetAudioApply(VMT_LIVERY_MOD) > 0.f)
	{
		return true;
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("PBUS2", 0x149BD32A))
	{
		return true;
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("CLUB", 0x82E47E85) && m_Vehicle->GetVariationInstance().GetModIndex(VMT_ROOF) != INVALID_MOD && m_Vehicle->GetVariationInstance().GetAudioApply(VMT_ROOF) > 0.f)
	{
		return true;
	}
	else if (m_Vehicle->GetVariationInstance().GetModIndex(VMT_ICE) != INVALID_MOD && !m_Vehicle->HasExpandedMods())
	{
		return true;
	}
	else if (m_Vehicle->GetVariationInstance().GetModIndex(VMT_TRUNK) != INVALID_MOD && m_Vehicle->GetVariationInstance().GetAudioApply(VMT_TRUNK) > 0.f)
	{
		return true;
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("ASBO", 0x42ACA95F) && m_Vehicle->GetVariationInstance().GetModIndex(VMT_CHASSIS) != INVALID_MOD && m_Vehicle->GetVariationInstance().GetAudioApply(VMT_CHASSIS) > 0.f)
	{
		return true;
	}
#if __BANK
	else if(g_ForceUpgradeRadio)
	{
		return true;
	}
#endif
	
	return false;
}

// -------------------------------------------------------------------------------
// Get the radio offset position
// -------------------------------------------------------------------------------
Vec3V_Out audCarAudioEntity::GetRadioEmitterOffset() const
{
	if(m_Vehicle && m_Vehicle->GetVariationInstance().GetModIndex(VMT_TRUNK) != INVALID_MOD && m_Vehicle->GetVariationInstance().GetAudioApply(VMT_TRUNK) > 0.f)
	{
		return Negate(Vec3V(V_Y_AXIS_WZERO) * ScalarV(g_TrunkRadioPositionOffset));
	}

	return Vec3V(V_ZERO);
}

// ----------------------------------------------------------------
// Called when a vehicle mode is altered
// ----------------------------------------------------------------
void audCarAudioEntity::OnModChanged(eVehicleModType modSlot, u8 UNUSED_PARAM(modSlotnewModIndex))
{
	if(modSlot == VMT_ENGINE || (modSlot >= VMT_ENGINEBAY1 && modSlot <= VMT_ENGINEBAY3))
	{
		audSound* engineEffectSubmixSound = GetEngineEffectSubmixSound();		

		if(engineEffectSubmixSound)
		{
			if((m_vehicleEngine.IsGranularEngineActive() && m_GranularEngineSettings && m_GranularEngineSettings->UpgradedEngineSynthDef != m_GranularEngineSettings->EngineSynthDef && m_GranularEngineSettings->UpgradedEngineSynthDef != 0u) ||
				(!m_vehicleEngine.IsGranularEngineActive() && m_VehicleEngineSettings && m_VehicleEngineSettings->UpgradedEngineSynthDef != m_VehicleEngineSettings->EngineSynthDef && m_VehicleEngineSettings->UpgradedEngineSynthDef != 0u))
			{
				m_ResetDSPEffects = true;
				m_ReapplyDSPPresets = true;
			}
			else if((m_vehicleEngine.IsGranularEngineActive() && m_GranularEngineSettings && m_GranularEngineSettings->UpgradedEngineSynthPreset != m_GranularEngineSettings->EngineSynthPreset && m_GranularEngineSettings->UpgradedEngineSynthPreset != 0u) ||
				(!m_vehicleEngine.IsGranularEngineActive() && m_VehicleEngineSettings && m_VehicleEngineSettings->UpgradedEngineSynthPreset != m_VehicleEngineSettings->EngineSynthPreset && m_VehicleEngineSettings->UpgradedEngineSynthPreset != 0u))
			{
				m_ReapplyDSPPresets = true;
			}
		}
	}
	
	if(modSlot == VMT_EXHAUST || (modSlot >= VMT_ENGINEBAY1 && modSlot <= VMT_ENGINEBAY3))
	{
		audSound* exhaustEffectSubmixSound = GetExhaustEffectSubmixSound();

		if(exhaustEffectSubmixSound)
		{
			if((m_vehicleEngine.IsGranularEngineActive() && m_GranularEngineSettings && m_GranularEngineSettings->UpgradedExhaustSynthDef != m_GranularEngineSettings->ExhaustSynthDef && m_GranularEngineSettings->UpgradedExhaustSynthDef != 0u) ||
				(!m_vehicleEngine.IsGranularEngineActive() && m_VehicleEngineSettings && m_VehicleEngineSettings->UpgradedExhaustSynthDef != m_VehicleEngineSettings->ExhaustSynthDef && m_VehicleEngineSettings->UpgradedExhaustSynthDef != 0u))
			{
				m_ResetDSPEffects = true;
				m_ReapplyDSPPresets = true;
			}
			else if((m_vehicleEngine.IsGranularEngineActive() && m_GranularEngineSettings && m_GranularEngineSettings->UpgradedExhaustSynthPreset != m_GranularEngineSettings->ExhaustSynthPreset && m_GranularEngineSettings->UpgradedExhaustSynthPreset != 0u) ||
				(!m_vehicleEngine.IsGranularEngineActive() && m_VehicleEngineSettings && m_VehicleEngineSettings->UpgradedExhaustSynthPreset != m_VehicleEngineSettings->ExhaustSynthPreset && m_VehicleEngineSettings->UpgradedExhaustSynthPreset != 0u))
			{
				m_ReapplyDSPPresets = true;
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix synth def
// -------------------------------------------------------------------------------
u32 audCarAudioEntity::GetEngineSubmixSynth() const
{
	if(ShouldUseDSPEffects())
	{
		if(g_GranularEnabled && m_GranularEngineSettings)
		{
			if(IsEngineUpgraded() && m_GranularEngineSettings->UpgradedEngineSynthDef != 0u)
			{
				return m_GranularEngineSettings->UpgradedEngineSynthDef;
			}
			else
			{
				return m_GranularEngineSettings->EngineSynthDef;
			}
		}
		else if(m_VehicleEngineSettings)
		{
			if(IsEngineUpgraded() && m_VehicleEngineSettings->UpgradedEngineSynthDef != 0u)
			{
				return m_VehicleEngineSettings->UpgradedEngineSynthDef;
			}
			else
			{
				return m_VehicleEngineSettings->EngineSynthDef;
			}
		}
	}

	return audVehicleAudioEntity::GetEngineSubmixSynth();
}

// -------------------------------------------------------------------------------
// Get the engine submix synth preset
// -------------------------------------------------------------------------------
u32 audCarAudioEntity::GetEngineSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects())
	{
		if(g_GranularEnabled && m_GranularEngineSettings)
		{
			if(IsEngineUpgraded() && m_GranularEngineSettings->UpgradedEngineSynthPreset != 0u)
			{
				return m_GranularEngineSettings->UpgradedEngineSynthPreset;
			}
			else
			{
				return m_GranularEngineSettings->EngineSynthPreset;
			}
		}
		else if(m_VehicleEngineSettings)
		{
			if(IsEngineUpgraded() && m_VehicleEngineSettings->UpgradedEngineSynthPreset != 0u)
			{
				return m_VehicleEngineSettings->UpgradedEngineSynthPreset;
			}
			else
			{
				return m_VehicleEngineSettings->EngineSynthPreset;
			}
		}
	}

	return audVehicleAudioEntity::GetEngineSubmixSynthPreset();
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth def
// -------------------------------------------------------------------------------
u32 audCarAudioEntity::GetExhaustSubmixSynth() const
{
	if(ShouldUseDSPEffects())
	{
		if(g_GranularEnabled && m_GranularEngineSettings)
		{
			if(IsExhaustUpgraded() && m_GranularEngineSettings->UpgradedExhaustSynthDef != 0u)
			{
				return m_GranularEngineSettings->UpgradedExhaustSynthDef;
			}
			else
			{
				return m_GranularEngineSettings->ExhaustSynthDef; 
			}
		}
		else if(m_VehicleEngineSettings)
		{
			if(IsExhaustUpgraded() && m_VehicleEngineSettings->UpgradedExhaustSynthDef != 0u)
			{
				return m_VehicleEngineSettings->UpgradedExhaustSynthDef;
			}
			else
			{
				return m_VehicleEngineSettings->ExhaustSynthDef;
			}
		}
	}

	return audVehicleAudioEntity::GetExhaustSubmixSynth();
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth preset
// -------------------------------------------------------------------------------
u32 audCarAudioEntity::GetExhaustSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects())
	{
		if(g_GranularEnabled && m_GranularEngineSettings)
		{
			if(IsExhaustUpgraded() && m_GranularEngineSettings->UpgradedExhaustSynthPreset != 0u)
			{
				return m_GranularEngineSettings->UpgradedExhaustSynthPreset;
			}
			else
			{
				return m_GranularEngineSettings->ExhaustSynthPreset;
			}
		}
		else if(m_VehicleEngineSettings)
		{
			if(IsExhaustUpgraded() && m_VehicleEngineSettings->UpgradedExhaustSynthPreset != 0u)
			{
				return m_VehicleEngineSettings->UpgradedExhaustSynthPreset;
			}
			else
			{
				return m_VehicleEngineSettings->ExhaustSynthPreset;
			}
		}
	}

	return audVehicleAudioEntity::GetExhaustSubmixSynthPreset();
}


// -------------------------------------------------------------------------------
// Get the engine submix voice
// -------------------------------------------------------------------------------
u32 audCarAudioEntity::GetEngineSubmixVoice() const			
{
	if(g_GranularEnabled && m_GranularEngineSettings)
	{
		return m_GranularEngineSettings->EngineSubmixVoice;
	}
	else if(m_VehicleEngineSettings)
	{
		return m_VehicleEngineSettings->EngineSubmixVoice;
	}
	
	return audVehicleAudioEntity::GetEngineSubmixVoice();
}

// -------------------------------------------------------------------------------
// Get the exhaust submix voice
// -------------------------------------------------------------------------------
u32 audCarAudioEntity::GetExhaustSubmixVoice() const			
{
	if(g_GranularEnabled && m_GranularEngineSettings)
	{
		return m_GranularEngineSettings->ExhaustSubmixVoice;
	}
	else if(m_VehicleEngineSettings)
	{
		return m_VehicleEngineSettings->ExhaustSubmixVoice;
	}
	
	return audVehicleAudioEntity::GetExhaustSubmixVoice();
}

// -------------------------------------------------------------------------------
// Get the exhaust proximity effect volume boost
// -------------------------------------------------------------------------------
f32 audCarAudioEntity::GetExhaustProximityVolumeBoost() const
{
	if(m_GranularEngineSettings)
	{
		return m_GranularEngineSettings->ExhaustProximityVolume_PostSubmix * 0.01f;
	}

	return audVehicleAudioEntity::GetExhaustProximityVolumeBoost();
}

// ----------------------------------------------------------------
// audCarAudioEntity GetCabinToneSound
// ----------------------------------------------------------------
u32 audCarAudioEntity::GetCabinToneSound() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->CabinToneLoop;
	}

	return audVehicleAudioEntity::GetCabinToneSound();
}

// -------------------------------------------------------------------------------
// The revs boost effect slowly scales up revs and pitch to give the impression that the 
// vehicle is constantly accelerating even once it is going at max speed
// -------------------------------------------------------------------------------
void audCarAudioEntity::UpdateRevsBoostEffect(audVehicleVariables* state)
{
	bool suggestChangeUp = false;

	if(g_RevsBoostEffectEnabled && !m_IsInAmphibiousBoatMode)
	{
		u32 timeSinceGearChange = state->timeInMs - m_vehicleEngine.GetTransmission()->GetLastGearChangeTimeMs();
		
		// If we're going above the speed and rev limits, start bringing in the effect
		if(state->fwdSpeedRatio > (m_IsPlayerVehicle? g_RevsBoostEffectFwdSpeedRatio : g_RevsBoostEffectFwdSpeedRatioNPC) && 
			state->rawRevs > g_RevsBoostEffectRevRatio && 
			state->numWheelsTouchingFactor == 1.0f &&
			state->throttle == 1.0f && 
			timeSinceGearChange > 2000)
		{
			// Kill the effect once the vehicle starts deviating from a set angular velocity. This means that cornering/weaving
			// will drop the revs, allowing them to start climbing again. The limit gets stricter as we approach the top of the pitch range
			f32 currentAngularVelocityLimit = g_RevsBoostAngularVelocityLimit + ((g_RevsBoostAngularVelocityLimit * g_RevsBoostAngularVelocityLimitSensitivity) * (1.0f - m_MaxSpeedPitchFactor));

			// More sensitive on NPC vehicles
			if(!m_IsPlayerVehicle)
			{
				currentAngularVelocityLimit *= 10.0f;
			}

			if(g_RevsBoostAngularVelocityLimit > 0.0f && Abs(GetCachedAngularVelocity().GetZ()) > currentAngularVelocityLimit)
			{
				// Separate decrease rates for angular velocity changes incase we want to fade the effect quicker
				m_MaxSpeedRevsAddition -= (g_RevsBoostRevsScalar - 1.0f) * (fwTimer::GetTimeStep()/g_RevsBoostRevsScaleFadeOutTime);
				m_MaxSpeedPitchFactor -= (fwTimer::GetTimeStep()/g_RevsBoostPitchOffsetFadeOutTime);
			}
			else
			{
				// Revs increase first
				if(state->rawRevs < 1.0f)
				{
					m_MaxSpeedRevsAddition += (g_RevsBoostRevsScalar - 1.0f) * (fwTimer::GetTimeStep() / (g_RevsBoostRevsScaleFadeInTime * (m_IsPlayerVehicle? 1.0f : 2.0f)));
				}
				else
				{
					m_MaxSpeedRevsAddition -= (g_RevsBoostRevsScalar - 1.0f) * (fwTimer::GetTimeStep() / (g_RevsBoostRevsScaleFadeInTime * (m_IsPlayerVehicle? 1.0f : 2.0f) * 2.0f));
				}

				// Once revs are fully increased, bring in the pitch scale
				if(state->rawRevs >= 0.98f )
				{
					suggestChangeUp = true;

					// Only in top gear
					if(!g_RevsBoostPitchOffsetOnlyInFinalGear || state->gear == m_Vehicle->m_Transmission.GetNumGears())
					{
						m_MaxSpeedPitchFactor += (fwTimer::GetTimeStep() / g_RevsBoostPitchOffsetFadeInTime) * (m_IsPlayerVehicle? 1.0f : 2.0f);
					}
				}
			}
		}
		else
		{
			m_MaxSpeedRevsAddition -= (g_RevsBoostRevsScalar - 1.0f) * (fwTimer::GetTimeStep()/g_RevsBoostRevsScaleFadeOutTime);
			m_MaxSpeedPitchFactor -= (fwTimer::GetTimeStep()/g_RevsBoostPitchOffsetFadeOutTime);
		}

		m_MaxSpeedPitchFactor = Clamp(m_MaxSpeedPitchFactor, 0.0f, 1.0f);
		m_MaxSpeedRevsAddition = Max(m_MaxSpeedRevsAddition, 0.0f);

		f32 pitchAdditionFactor = m_MaxSpeedPitchFactor;
		state->granularEnginePitchOffset = (s32)(g_RevsBoostPitchOffset * pitchAdditionFactor);
	}

	m_Vehicle->m_Transmission.AudioSuggestChangeUp(suggestChangeUp);
}

bool audCarAudioEntity::IsKickstarted() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_ISKICKSTARTED) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::HasCBRadio() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_HASCBRADIO) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::IsDamageModel() const
{
	return GetRandomDamageClass() == RANDOM_DAMAGE_ALWAYS;
}

bool audCarAudioEntity::IsToyCar() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_ISTOYCAR) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::IsGoKart() const 
{
	return GetVehicleModelNameHash() == ATSTRINGHASH("Veto", 0xCCE5C8FA) || GetVehicleModelNameHash() == ATSTRINGHASH("Veto2", 0xA703E4A9);
}

bool audCarAudioEntity::IsGolfKart() const
{
	const u32 modelNameHash = GetVehicleModelNameHash();
	return modelNameHash == ATSTRINGHASH("Caddy", 0x44623884) || modelNameHash == ATSTRINGHASH("Caddy2", 0xDFF0594C) || modelNameHash == ATSTRINGHASH("Caddy3", 0xD227BDBB);
}

bool audCarAudioEntity::AreTyreChirpsEnabled() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_TYRECHIRPSENABLED) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::HasReverseWarning() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_REVERSEWARNING) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::HasDoorOpenWarning() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_DOOROPENWARNING) != AUD_TRISTATE_FALSE;
}

bool audCarAudioEntity::HasHeavyRoadNoise() const
{
	return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_HEAVYROADNOISE) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::CanCauseControllerRumble() const
{
	// B*2136772 GTAV fix - disabling this, it got forgotten about and only set up on a solitary vehicle, making it look like a bug rather than an intentional feature
	// TODO: Reinstate for future projects and actually make use of it
	return false;
	//return m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_CAUSECONTROLLERRUMBLE) == AUD_TRISTATE_TRUE;
}

bool audCarAudioEntity::HasAlarm()  
{
	const CarAudioSettings* settings = GetCarAudioSettings();
	if(settings)
	{
		return (AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_CARAUDIOSETTINGS_HASALARM) == AUD_TRISTATE_TRUE);
	}
	return false;
}

RandomDamageClass audCarAudioEntity::GetRandomDamageClass() const
{
	if(m_CarAudioSettings)
	{
		return (RandomDamageClass) m_CarAudioSettings->RandomDamage;
	}

	return audVehicleAudioEntity::GetRandomDamageClass();
}

void audCarAudioEntity::TriggerBrakeRelease(f32 brakeHoldTime, bool isHandbrake)
{
	if(IsDisabled())
	{
		return;
	}

	if(brakeHoldTime > 0.25f && !audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
	{
		f32 startOffset = sm_BrakeHoldTimeToStartOffsetCurve.CalculateValue(brakeHoldTime);
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;
		initParams.StartOffset = (s32)startOffset;
		initParams.IsStartOffsetPercentage = true;
		CreateAndPlaySound(sm_ExtrasSoundSet.Find(isHandbrake?ATSTRINGHASH("BigRig_HandbrakeRelease", 0x2F52453A):ATSTRINGHASH("BigRig_BrakeRelease", 0xFE7CB891)), &initParams); 
	}
}

void audCarAudioEntity::UpdateBrakes(audVehicleVariables* state)
{
	PF_FUNC(UpdateBrakes);

	f32 scrapeVolLin = 0.0f;
	for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
	{
		CWheel *wheel = m_Vehicle->GetWheel(i);

		// check how damaged the wheel is and play metal grind sounds
		// for a front wheel check the angle of the wheel and compare with steering angle to grind more
		f32 speedRatio = (wheel->GetIsTouching()?Clamp(Abs(wheel->GetGroundSpeed()) * state->invDriveMaxVelocity, 0.0f,1.0f):0.0f);
		
		f32 steeringGrindRatio = 0.5f;

		if(wheel->GetConfigFlags().IsFlagSet(WCF_STEER))
		{
			// dot with the right vector
			//f32 bentAngle = offset.x;

			f32 steeringAngle = wheel->GetSteeringAngle();
			steeringGrindRatio = Clamp(Abs(steeringAngle*4.0f),0.0f,1.0f);

		}

		// Wheels are generally completely dead by about 0.3 damage, so max out the volume a little before then
		f32 damageRatio = Clamp((wheel->GetFrictionDamage() - 0.1f)/0.25f, 0.f, 1.f);
		f32 vol = damageRatio * Clamp((speedRatio*2.f),0.0f,1.0f) * audDriverUtil::ComputeLinearVolumeFromDb(Lerp(steeringGrindRatio, -15.0f, 0.0f));
		scrapeVolLin = Max(vol, scrapeVolLin);

		if(m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_OFF)
		{
			bool needToSetupSound = (m_WheelGrindLoops[i] == NULL);
			// We position wheel loops manually so disable automatic tracking
			UpdateLoopWithLinearVolumeAndPitch(&m_WheelGrindLoops[i], ATSTRINGHASH("DAMAGE_WHEEL_GRIND", 0x50040B5D), vol, i*50, m_EnvironmentGroup, NULL, true, false, 1.f, false);
			if(m_WheelGrindLoops[i] && needToSetupSound)
			{
				Vector3 pos;
				GetCachedWheelPosition(i, pos, *state);
				m_WheelGrindLoops[i]->SetRequestedPosition(pos);
				m_WheelGrindLoops[i]->SetClientVariable(GetWheelSoundUpdateClientVar(i));
				m_WheelGrindLoops[i]->SetUpdateEntity(true);
			}
		}

		// tyre damage
		const f32 tyreHealth = wheel->GetTyreHealth() * TYRE_HEALTH_DEFAULT_INV;
		u32 hashToPlay = g_NullSoundHash;
		f32 tyreDamageVol = 0.f;
		bool inWater = wheel->GetDynamicFlags().IsFlagSet(WF_INSHALLOWWATER) || wheel->GetDynamicFlags().IsFlagSet(WF_INDEEPWATER) BANK_ONLY(|| g_ForceShallowWater);

		if(!m_Vehicle->m_nVehicleFlags.bIsAsleep && wheel->GetIsFlat() && m_Vehicle->GetWheel(i)->GetIsTouching() && !inWater)
		{
			if(tyreHealth > 0.f)
			{
				tyreDamageVol = Min(1.f, speedRatio * 2.f);
				// still have some tyre
				hashToPlay = ATSTRINGHASH("FLAT_TYRE", 0x5741B3C5);
			}
			else
			{
				// no tyre left. Speed is greatly reduced, so scale back the speed ratio when calculating volume
				tyreDamageVol = Clamp(Max(state->fwdSpeedRatio, speedRatio)/0.2f, 0.0f, 1.0f);
				hashToPlay = ATSTRINGHASH("WHEEL_RIM", 0x6A8F09B3);

				// catch the case where we were playing rubber and we're now down to the rim
				if(!m_IsTyreCompletelyDead.IsSet(i))
				{
					if(m_TyreDamageLoops[i])
					{
						m_TyreDamageLoops[i]->StopAndForget();
					}

					m_IsTyreCompletelyDead.Set(i);
				}

				VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(m_Vehicle->GetWheel(i)->GetMaterialId());
				VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(VFXTYRESTATE_BURST_DRY, vfxGroup);

				if(pVfxWheelInfo)
				{
					CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(m_Vehicle);

					if(pVfxVehicleInfo)
					{
						atHashWithStringNotFinal ptFxHashName;

						if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==2)
						{
							ptFxHashName = pVfxWheelInfo->ptFxWheelFric2HashName;
						}
						else
						{
							ptFxHashName = pVfxWheelInfo->ptFxWheelFric1HashName;
						}

						if(ptFxHashName.GetHash() != ATSTRINGHASH("rim_fric_hard", 0x9F6F204C))
						{
							tyreDamageVol = 0.f;
						}
					}
				}
			}
			
			if(!m_TyreDamageLoops[i])
			{
				if(tyreDamageVol > g_SilenceVolumeLin)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					CreateSound_PersistentReference(hashToPlay, &m_TyreDamageLoops[i], &initParams);
					if(m_TyreDamageLoops[i])
					{
						Vector3 pos;
						GetCachedWheelPosition(i, pos, *state);
						m_TyreDamageLoops[i]->SetUpdateEntity(true);
						m_TyreDamageLoops[i]->SetRequestedPosition(pos);
						m_TyreDamageLoops[i]->SetClientVariable(GetWheelSoundUpdateClientVar(i));
						m_TyreDamageLoops[i]->PrepareAndPlay();
					}
				}
			}
		}
		if(m_TyreDamageLoops[i])
		{
			if(tyreDamageVol  < g_SilenceVolumeLin)
			{
				m_TyreDamageLoops[i]->StopAndForget();
			}
			else
			{
				const f32 pitch = 760.f * state->wheelSpeedRatio;
				m_TyreDamageLoops[i]->SetRequestedPitch((s32)pitch);
				m_TyreDamageLoops[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(tyreDamageVol));
			}
		}
	}

	m_BrakeLinearVolumeSmoother.SetRates(g_BrakeLinearVolumeSmootherRate / 1000.0f, 0.01f);
	f32 minSuspensionHealth = SUSPENSION_HEALTH_DEFAULT;
	for(s32 i = 0 ; i < m_Vehicle->GetNumWheels(); i++)
	{
		minSuspensionHealth = Min(m_Vehicle->GetVehicleDamage()->GetSuspensionHealth(i), minSuspensionHealth);
	}
	const f32 suspensionDamageFactor = Max(m_ScriptBodyDamageFactor, 1.0f - minSuspensionHealth / SUSPENSION_HEALTH_DEFAULT);
	f32 volLin = 0.0f;
	
	if(m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_OFF && !m_Vehicle->m_nVehicleFlags.bIsBeingTowed && !m_IsInAmphibiousBoatMode)
	{
		volLin = m_CarAudioSettings->BrakeSqueekFactor * sm_BrakeToBrakeDiscVolumeCurve.CalculateValue(state->brakePedal) * sm_SpeedToBrakeDiscVolumeCurve.CalculateValue(state->speedForBrakes) * audDriverUtil::ComputeLinearVolumeFromDb(state->carVolOffset + ((1.f - suspensionDamageFactor) * -4.f));
		// if there is a wheel knackered then also apply some brake disc squeal while the wheel is rotating
		volLin = Max(volLin, scrapeVolLin*0.4f);
	}
	
	f32 volLinSmoothed = m_BrakeLinearVolumeSmoother.CalculateValue(volLin, state->timeInMs);
	bool hasBigRigBrakes = AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_BIGRIGBRAKES) == AUD_TRISTATE_TRUE;
	UpdateLoopWithLinearVolumeAndPitch(&m_DiscBrakeSqueel,(hasBigRigBrakes?ATSTRINGHASH("BIG_RIG_DISC_BRAKE", 0xF67F84FA):ATSTRINGHASH("DISC_BRAKE_SQUEEL_LOOP", 0x8F09D5E0)), volLinSmoothed, 0, m_EnvironmentGroup, NULL, true, false);

	if(m_vehicleEngine.GetState() != audVehicleEngine::AUD_ENGINE_OFF)
	{
		if(hasBigRigBrakes)
		{
			// if vehicle is stationary then charge up brakes (pseudo-handbrake)
			if(state->fwdSpeed < 0.1f && m_Vehicle->m_nVehicleFlags.bEngineOn)
			{
				if(m_WasStationaryLastFrame)
				{
					m_TimeStationary += fwTimer::GetTimeStep();
				}
				else
				{
					m_TimeStationary = 0.0f;
				}

				m_WasStationaryLastFrame = true;
			}
			else
			{
				// dont trigger the release if the engine turns off
				if(m_WasStationaryLastFrame && m_TimeStationary >= sm_TimeToApplyHandbrake && m_Vehicle->GetThrottle() > 0.1f)
				{
					TriggerBrakeRelease(m_TimeStationary - sm_TimeToApplyHandbrake, true);
				}

				m_WasStationaryLastFrame = false;

			}

			f32 brakePedal = state->brakePedal;

			if(GetVehicleModelNameHash() == ATSTRINGHASH("CHERNOBOG", 0xD6BC7523) && m_Vehicle->GetOutriggerDeployRatio() > 0.f)
			{
				brakePedal = 0.f;
			}

			if(brakePedal > 0.1f)
			{
				if(m_WasBrakeHeldLastFrame)
				{
					m_BrakeHoldTime += fwTimer::GetTimeStep();
				}
				else
				{
					m_BrakeHoldTime = 0.f;
				}
				m_WasBrakeHeldLastFrame = true;
			}
			else
			{
				if(m_WasBrakeHeldLastFrame)
				{
					TriggerBrakeRelease(m_BrakeHoldTime, false);
				}

				m_WasBrakeHeldLastFrame = false;
			}
		}
	}
}

void audCarAudioEntity::TriggerHandbrakeSound()
{
	if(IsDisabled())
	{
		return;
	}

	if(m_vehicleEngine.WasEngineOnLastFrame() && m_CarAudioSettings)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;
		CreateAndPlaySound(m_CarAudioSettings->Handbrake, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_CarAudioSettings->Handbrake, &initParams, m_Vehicle));
	}
}

void audCarAudioEntity::UpdateIndicator()
{
	if(m_HasIndicatorRequest)
	{
		const bool isOn = m_IsIndicatorRequestedOn;
		if(m_CarAudioSettings)
		{
			u32 hash = 0;
			if(m_IsIndicatorOn && !isOn)
			{
				// trigger off
				hash = m_CarAudioSettings->IndicatorOff;
			}
			else if(!m_IsIndicatorOn && isOn)
			{
				// trigger on
				hash = m_CarAudioSettings->IndicatorOn;
			}
			if(hash!=0)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				
				f32 vol;
				u32 cutoff;
				GetInteriorSoundOcclusion(vol,cutoff);
				initParams.Volume = Max(vol, -6.f);

				CreateAndPlaySound(hash, &initParams);
			}
		}
		m_IsIndicatorOn = isOn;
		m_HasIndicatorRequest = false;
	}
}

bool audCarAudioEntity::DoesHornHaveTail() const
{
	// HL - Hacky fix here to support new horns for the BTYPE vehicle:
	// The BTYPE's default claxon-style horn uses an on-stop sound to play a tail, hence we don't want to use the 
	// standard horn release time value as it cuts it off. 
	const u32 vehicleModelNameHash = GetVehicleModelNameHash();

	if(vehicleModelNameHash == ATSTRINGHASH("BTYPE", 0x6FF6914) || vehicleModelNameHash == ATSTRINGHASH("BTYPE3", 0xDC19D101))
	{
		CVehicleVariationInstance& variation = m_Vehicle->GetVariationInstance();
		
		if(variation.GetMods()[VMT_HORN] == INVALID_MOD)
		{
			// No mod - must be using the default horn
			return true;
		}
		else
		{
			u32 modeKitHash = variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier();
			
			if(modeKitHash == 0)
			{
				// No mod - must be using the default horn
				return true;
			}
		}		
	}
	// End hacky fix

	return false;
}

u32 audCarAudioEntity::GetVehicleHornSoundHash(bool ignoreMods)
{
	u32 vehicleHornHash = 0;
	if(m_Vehicle && GetCarAudioSettings())
	{
		u32 hornSoundsHash = GetCarAudioSettings()->HornSounds;

		// HL - Hacky fix here to support new horns for the BTYPE vehicle:
		// The new horn assets are bundled in with the vehicle engine bank, so we need to duplicate them in both the NPC and 
		// player banks so that cars can still honk even if the player isn't driving them. This means that we need to play a 
		// different sound depending on whether or not this is a player vehicle.
		const u32 vehicleModelNameHash = GetVehicleModelNameHash();

		if(vehicleModelNameHash == ATSTRINGHASH("BTYPE", 0x6FF6914))
		{
			if(m_IsPlayerVehicle)
			{
				hornSoundsHash = ATSTRINGHASH("HORN_LIST_BTYPE", 0x329FE267);
			}
			else
			{
				hornSoundsHash = ATSTRINGHASH("HORN_LIST_BTYPE_NPC", 0x5A5E45C6);
			}
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("BTYPE3", 0xDC19D101))
		{
			if(m_IsPlayerVehicle)
			{
				hornSoundsHash = ATSTRINGHASH("HORN_LIST_BTYPE3", 0x28435C95);
			}
			else
			{
				hornSoundsHash = ATSTRINGHASH("HORN_LIST_BTYPE3_NPC", 0xE6E47A37);
			}
		}
		// End hacky fix

		SoundHashList* hornSounds = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObject<SoundHashList>(hornSoundsHash);
		if(naVerifyf(hornSounds, "Vehicle has no horn Sounds."))
		{			
			if(	m_HornSoundIndex == -1)
			{
				m_HornSoundIndex = static_cast<s16>(hornSounds->CurrentSoundIdx);
				naAssertf(m_HornSoundIndex != -1, "Fail getting the horn index, please contact audio team.");
				hornSounds->CurrentSoundIdx = (hornSounds->CurrentSoundIdx + 1) % hornSounds->numSoundHashes;
			}
			if(naVerifyf(m_HornSoundIndex < hornSounds->numSoundHashes,"Wrong horn index."))
			{
				vehicleHornHash = hornSounds->SoundHashes[m_HornSoundIndex].SoundHash;
			}

		}
		if(!ignoreMods)
		{
			CVehicleVariationInstance& variation = m_Vehicle->GetVariationInstance();
			if(variation.GetMods()[VMT_HORN] != INVALID_MOD)
			{
				u32 modKitHash = variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier();
				if( modKitHash != 0)
				{
					vehicleHornHash = modKitHash;
				}
			}
		}
	}
	return vehicleHornHash;
}

void audCarAudioEntity::BlipHorn()
{
	if(m_CarAudioSettings)
	{
		audEnvelopeSound *envelopeSound;
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		const Vector3 pos = VEC3V_TO_VECTOR3(m_Vehicle->TransformIntoWorldSpace(m_HornOffsetPos));
		initParams.Position = pos;
		CreateSound_LocalReference(ATSTRINGHASH("UTIL_ENVELOPE", 0xF4983BA1),(audSound**)&envelopeSound, &initParams);
		if(envelopeSound)
		{
			envelopeSound->SetRequestedEnvelope(30, 0,100,100,50);
			envelopeSound->SetChildSoundReference(GetVehicleHornSoundHash());
			envelopeSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_HORN);
			envelopeSound->SetUpdateEntity(true);
			envelopeSound->PrepareAndPlay(NULL, false, 100);
		}
	}
}

void audCarAudioEntity::TriggerDoorOpenSound(eHierarchyId doorId)
{
	if(IsDisabled())
	{
		return;
	}

	if(m_CarAudioSettings)
	{
		u32 hash = g_NullSoundHash;

		// Zhaba boot/door correspond to the small flaps on the side of the vehicle
		if (GetVehicleModelNameHash() == ATSTRINGHASH("ZHABA", 0x4C8DBA51) && (doorId == VEH_BONNET || doorId == VEH_BOOT))
		{	
			hash = m_CarAudioSettings->BootOpenSound;
		}
		else if(doorId == VEH_BONNET)
		{
			hash = ATSTRINGHASH("VEHICLES_EXTRAS_DAMAGED_HOOD_OPEN", 0x34DA9EFC);
		}
		else if(doorId == VEH_BOOT)
		{
			hash = m_CarAudioSettings->BootOpenSound;
		}
		else
		{
			hash = m_CarAudioSettings->DoorOpenSound;
		}
		TriggerDoorSound(hash, doorId);
	}
}

void audCarAudioEntity::TriggerDoorCloseSound(eHierarchyId doorId, const bool isBroken)
{
	if(IsDisabled())
	{
		return;
	}

	// if it's the player vehicle, and they're pissed off - slam the door. Note that this'll have his passengers slam the door too - oh well.
	f32 volumeOffset = 0.0f;
	if (m_Vehicle && m_Vehicle == CGameWorld::FindLocalPlayerVehicle() && 
		CGameWorld::GetMainPlayerInfo()->PlayerIsPissedOff())
	{
		if (doorId == VEH_DOOR_DSIDE_F || 
			doorId == VEH_DOOR_DSIDE_R ||
			doorId == VEH_DOOR_PSIDE_F ||
			doorId == VEH_DOOR_PSIDE_R)
		{
			volumeOffset = 6.0f;
		}
	}
	if(m_CarAudioSettings)
	{
		u32 hash = g_NullSoundHash;

		// Zhaba boot/door correspond to the small flaps on the side of the vehicle
		if (GetVehicleModelNameHash() == ATSTRINGHASH("ZHABA", 0x4C8DBA51) && (doorId == VEH_BONNET || doorId == VEH_BOOT))
		{
			hash = m_CarAudioSettings->BootCloseSound;
		}
		else if(doorId == VEH_BOOT || doorId == VEH_BONNET)
		{
			if(isBroken)
			{
				if(fwTimer::GetTimeInMilliseconds() < m_NextDoorTime)
				{
					return;
				}
				m_NextDoorTime = fwTimer::GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(100,300);
				hash = ATSTRINGHASH("VEHICLES_EXTRAS_DAMAGED_HOOD_SHUT", 0x209CA2D3);
			}
			else
			{
				hash = m_CarAudioSettings->BootCloseSound;
			}
		}
		else
		{
			hash = m_CarAudioSettings->DoorCloseSound;
		}
		
		TriggerDoorSound(hash, doorId, volumeOffset);
	}
}

void audCarAudioEntity::OnPlayerOpenDoor()
{
	const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(m_PlayerVehicleAlarmBehaviour && 
		now > m_TimeAlarmLastArmed + 5000 && 
		m_Vehicle && 
		m_Vehicle->m_Swankness >= 3 && 
		m_Vehicle->GetHealth() > 250.f &&
		!m_Vehicle->IsEngineOn() &&
		m_Vehicle->GetNumberOfPassenger() == 0 &&
		m_Vehicle->GetDriver() == NULL &&
		m_VehicleType == AUD_VEHICLE_CAR)
	{
		m_PlayerVehicleAlarmBehaviour = false;
		m_TimeAlarmLastArmed = now;
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("Alarm_Disarm", 0x95DE98C6)), &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_ExtrasSoundSet.GetNameHash(), ATSTRINGHASH("Alarm_Disarm", 0x95DE98C6), &initParams, m_Vehicle));
	}
}

void audCarAudioEntity::TriggerDoorFullyOpenSound(eHierarchyId doorId)
{
	if(IsDisabled())
	{
		return;
	}

	if(m_CarAudioSettings)
	{
		if(GetVehicleModelNameHash() != ATSTRINGHASH("Draugur", 0xD235A4A6))
		{
			TriggerDoorSound(ATSTRINGHASH("VEHICLES_EXTRAS_DOOR_LIMIT", 0xE4126EED), doorId);
		}		
	}
}

u32 audCarAudioEntity::GetJumpLandingSound(bool interiorView) const
{
	if(m_CarAudioSettings)
	{
		if(interiorView && m_CarAudioSettings->JumpLandSoundInterior != g_NullSoundHash)
		{
			return m_CarAudioSettings->JumpLandSoundInterior;
		}
		else
		{
			return m_CarAudioSettings->JumpLandSound;
		}
	}

	return audVehicleAudioEntity::GetJumpLandingSound(interiorView);
}

u32 audCarAudioEntity::GetVehicleRainSound(bool interiorView) const
{
	if(m_CarAudioSettings)
	{
		if(interiorView)
		{
			return m_CarAudioSettings->VehicleRainSoundInterior;
		}
		else
		{
			return m_CarAudioSettings->VehicleRainSound;
		}		
	}

	return audVehicleAudioEntity::GetVehicleRainSound(interiorView);
}

u32 audCarAudioEntity::GetTowTruckSoundSet() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->TowTruckSounds;
	}

	return audVehicleAudioEntity::GetTowTruckSoundSet();
}

u32 audCarAudioEntity::GetDamagedJumpLandingSound(bool interiorView) const
{
	if(m_CarAudioSettings)
	{
		if(interiorView && m_CarAudioSettings->DamagedJumpLandSoundInterior != g_NullSoundHash)
		{
			return m_CarAudioSettings->DamagedJumpLandSoundInterior;
		}
		else
		{
			return m_CarAudioSettings->DamagedJumpLandSound;
		}
	}

	return audVehicleAudioEntity::GetDamagedJumpLandingSound(interiorView);
}

u32 audCarAudioEntity::GetNPCRoadNoiseSound() const
{
	if(m_CarAudioSettings)
	{
		if(m_Vehicle->GetIntelligence()->IsOnHighway())
		{
			return m_CarAudioSettings->NPCRoadNoiseHighway;
		}
		else
		{
			return m_CarAudioSettings->NPCRoadNoise;
		}
	}

	return audVehicleAudioEntity::GetNPCRoadNoiseSound();
}

u32 audCarAudioEntity::GetReverseWarningSound() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ReverseWarningSound;
	}

	return audVehicleAudioEntity::GetReverseWarningSound();
}

u32 audCarAudioEntity::GetForkliftSoundSet() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ForkliftSounds;
	}

	return audVehicleAudioEntity::GetForkliftSoundSet();
}

u32 audCarAudioEntity::GetDiggerSoundSet() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->DiggerSounds;
	}

	return audVehicleAudioEntity::GetDiggerSoundSet();
}

u32 audCarAudioEntity::GetConvertibleRoofSoundSet() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ConvertibleRoofSoundSet;
	}

	return audVehicleAudioEntity::GetConvertibleRoofSoundSet();
}

u32 audCarAudioEntity::GetConvertibleRoofInteriorSoundSet() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->ConvertibleRoofInteriorSoundSet;
	}

	return audVehicleAudioEntity::GetConvertibleRoofInteriorSoundSet();
}

u32 audCarAudioEntity::GetTurretSoundSet() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->TurretSounds;
	}

	return audVehicleAudioEntity::GetTurretSoundSet();
}


u32 audCarAudioEntity::GetWindNoiseSound() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->WindNoise;
	}

	return audVehicleAudioEntity::GetWindNoiseSound();
}

u32 audCarAudioEntity::GetEngineIgnitionLoopSound() const			
{
	if(m_VehicleEngineSettings)
	{
		return m_VehicleEngineSettings->Ignition;
	}

	return audVehicleAudioEntity::GetEngineIgnitionLoopSound();
}

u32 audCarAudioEntity::GetEngineShutdownSound() const
{
	if(m_VehicleEngineSettings)
	{
		return m_VehicleEngineSettings->EngineShutdown;
	}

	return audVehicleAudioEntity::GetEngineShutdownSound();
}

u32 audCarAudioEntity::GetVehicleShutdownSound() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->CarSpecificShutdownSound;
	}

	return audVehicleAudioEntity::GetVehicleShutdownSound();
}

u32 audCarAudioEntity::GetEngineStartupSound() const
{
	if(m_VehicleEngineSettings)
	{
		return m_VehicleEngineSettings->EngineStartUp;
	}
	else if(m_ElectricEngineSettings)
	{
		return m_ElectricEngineSettings->EngineStartUp;
	}

	return audVehicleAudioEntity::GetEngineStartupSound();
}

bool audCarAudioEntity::IsElectricOrHybrid() const
{
	if(m_CarAudioSettings)
	{
		if(m_CarAudioSettings->EngineType == ELECTRIC || m_CarAudioSettings->EngineType == HYBRID)
		{
			return true;
		}
	}

	return false;
}

VehicleCollisionSettings* audCarAudioEntity::GetVehicleCollisionSettings() const
{
	if(m_CarAudioSettings)
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(m_CarAudioSettings->VehicleCollisions);
	}
	
	return audVehicleAudioEntity::GetVehicleCollisionSettings();
}

// ----------------------------------------------------------------
// Compute the ignition hold time
// ----------------------------------------------------------------
f32 audCarAudioEntity::ComputeIgnitionHoldTime(bool isStartingThisTime)
{
	bool isStartingFirstTime = (isStartingThisTime && m_Vehicle->m_failedEngineStartAttempts == 0);
	f32 maxHold = (isStartingFirstTime?sm_StartingIgnitionMaxHold:sm_IgnitionMaxHold);
	f32 minHold = (isStartingFirstTime?sm_StartingIgnitionMinHold:sm_IgnitionMinHold);
	f32 timeFactor = 1.f;

	// Electric engines just click on immediately
	if(m_CarAudioSettings && m_CarAudioSettings->EngineType == ELECTRIC)
	{
		m_IgnitionHoldTime = 0.0f;
	}
	else
	{
		if(!isStartingFirstTime && (m_Vehicle->GetStatus() == STATUS_WRECKED || m_Vehicle->m_Transmission.GetEngineHealth() <= ENGINE_DAMAGE_ONFIRE || !CTaskMotionInAutomobile::IsVehicleDrivable(*m_Vehicle)))
		{
			// its not going to start so try for longer
			timeFactor = 2.5f;
		}
		else if(HasHeavyRoadNoise())
		{
			timeFactor = 1.5f;
		}
		// Don't like hard coding the sound name here, but we've got non-granular trucks that aren't set to have heavy road noise so this is the simplest way to identify them
		else if(!m_vehicleEngine.IsGranularEngineActive() && m_VehicleEngineSettings && m_VehicleEngineSettings->Ignition == ATSTRINGHASH("IGNITION_TRUCK", 0xFBF1D8BA))
		{
			timeFactor = 1.5f;
		}

		if(m_Vehicle->m_nVehicleFlags.bCarNeedsToBeHotwired)
		{
			// Doing a little ignition at the end of the hotwire anim, need to keep it short to fit with the visuals
			maxHold = minHold;
		}

		m_IgnitionHoldTime = audEngineUtil::GetRandomNumberInRange(minHold * timeFactor, maxHold * timeFactor);
	}

	return m_IgnitionHoldTime;
}

VehicleVolumeCategory audCarAudioEntity::GetVehicleVolumeCategory()
{
	if (m_CarAudioSettings)
	{
		return (VehicleVolumeCategory) m_CarAudioSettings->VolumeCategory;
	}

	return VEHICLE_VOLUME_NORMAL;
}

f32 audCarAudioEntity::GetVehicleModelRadioVolume() const
{
	if(m_CarAudioSettings)
	{
		return m_CarAudioSettings->AmbientRadioVol; 
	}

	return 0.0f;
}

f32 audCarAudioEntity::GetControllerRumbleIntensity() const
{
	// For now - just full intensity while the startup sound is playing. May want to customize this with a curve or something
	if(m_IsPlayerVehicle && CanCauseControllerRumble() && m_vehicleEngine.GetStartupSound() != NULL)
	{
		return 1.0f;
	}

	return audVehicleAudioEntity::GetControllerRumbleIntensity();
}

f32 audCarAudioEntity::GetEngineSwitchFadeTimeScalar() const
{
	if(!m_IsPlayerVehicle)
	{
		// If we're low on vehicles, we can be a bit more lazy with our fade time - the lack of vehicles makes the sharper 
		// cuts in/out more obvious, so this improves the sound of the transitions
		if(m_Vehicle->GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && 
		   m_Vehicle->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
		{
			if(sm_ActivationRangeScale == g_MaxRealRangeScale &&
			  (!m_vehicleEngine.IsGranularEngineActive() || sm_GranularActivationRangeScale == g_MaxGranularRangeScale))
			{
				return 2.0f;
			}
		}
	}

	return 1.0f;
}

f32 audCarAudioEntity::GetInteriorViewEngineOpenness() const
{
	if(m_CarAudioSettings)
	{		
		return m_CarAudioSettings->InteriorViewEngineOpenness;	
	}

	return audVehicleAudioEntity::GetInteriorViewEngineOpenness();
}

f32 audCarAudioEntity::GetMinOpenness() const 
{
	f32 openness = 0.f;
	if(m_CarAudioSettings)
	{
		openness = m_CarAudioSettings->Openness;
	}
	//CVehicleVariationInstance& variation = m_Vehicle->GetVariationInstance();
	//if(variation.GetMods()[VMT_ROOF] != 255)
	//{
	//	if(variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier() != 0)
	//	{
	//		hornSound = variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier();
	//	}
	//}
	return openness;
}
void audCarAudioEntity::Fix()
{
	audVehicleAudioEntity::Fix();

	// also fix all car tyres
	m_IsTyreCompletelyDead.Reset();
}

f32 audCarAudioEntity::GetSkidVolumeScale() const
{
	if(m_CarAudioSettings && AUD_GET_TRISTATE_VALUE(m_CarAudioSettings->Flags, FLAG_ID_CARAUDIOSETTINGS_DISABLESKIDS) == AUD_TRISTATE_TRUE)
	{
		return 0.f;
	}
	return 1.f;
}

AmbientRadioLeakage audCarAudioEntity::GetPositionalPlayerRadioLeakage() const
{
	return GetAmbientRadioLeakage();	
}

bool audCarAudioEntity::IsSubmersible() const
{
	if(m_Vehicle->InheritsFromSubmarineCar() && m_IsInAmphibiousBoatMode)
	{		
		return true;
	}

	return audVehicleAudioEntity::IsSubmersible();
}

bool audCarAudioEntity::IsPlayingStartupSequence() const
{
	if(m_StartUpRevsSound)
	{
		const u32 playTimeMs = m_StartUpRevsSound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		static u32 s_StartupSequenceTime = 500;
		if(playTimeMs <= s_StartupSequenceTime)
		{
			// Only count the first 0.5s of the sequence
			return true;
		}
		return false;
	}
	else if(m_HasPlayedStartupSequence)
	{
		return false;
	}
	// We'll be in a starting state before we've played the sequence, so need to treat that as 'playing startup sequence'
	return (m_vehicleEngine.IsGranularEngineActive() && m_vehicleEngine.GetState() == audVehicleEngine::AUD_ENGINE_STARTING);
}

#if __BANK
void GranularLoopEditorShiftLeft()
{
	g_GranularLoopEditorShiftLeftPressed = true;
}

void GranularLoopEditorShiftRight()
{
	g_GranularLoopEditorShiftRightPressed = true;
}

void GranularLoopEditorGrowLoopPressed()
{
	g_GranularLoopEditorGrowLoopPressed = true;
}

void GranularLoopEditorShrinkLoopPressed()
{
	g_GranularLoopEditorShrinkLoopPressed = true;
}

void GranularLoopEditorCreateLoopPressed()
{
	g_GranularLoopEditorCreateLoopPressed = true;
}

void GranularLoopEditorDeleteLoopPressed()
{
	g_GranularLoopEditorDeleteLoopPressed = true;
}

void GranularLoopEditorExportLoopData()
{
	g_GranularLoopEditorExportDataPressed = true;
}

void TriggerMobilePreRing()
{
	g_ScriptAudioEntity.PlayMobilePreRing(ATSTRINGHASH("MOBILE_PRERING", 0x80DC9690), 1.0f);
}

void TestStuntRaceBoost()
{
	CVehicle* vehicle = FindPlayerVehicle();

	if(vehicle)
	{
		vehicle->GetVehicleAudioEntity()->TriggerStuntRaceSpeedBoost();
	}
}

void OverrideVehicleWeaponAmmoCount()
{
	CVehicle* vehicle = FindPlayerVehicle();

	if(vehicle && vehicle->GetVehicleWeaponMgr())
	{
		for(u32 i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
		{
			vehicle->SetRestrictedAmmoCount(i, g_WeaponAmmoCountOverride);
		}
	}
}

void TestStuntRaceSlowdown()
{
	CVehicle* vehicle = FindPlayerVehicle();

	if(vehicle)
	{
		vehicle->GetVehicleAudioEntity()->TriggerStuntRaceSlowDown();
	}
}

void TestSportsRevs()
{
	if(FindPlayerVehicle())
	{
		audVehicleAudioEntity* vehicleAudioEntity = FindPlayerVehicle()->GetVehicleAudioEntity();

		if(vehicleAudioEntity && vehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		{
			((audCarAudioEntity*)vehicleAudioEntity)->TriggerSportsCarRevs(audCarAudioEntity::REVS_TYPE_DEBUG);
		}
	}
}

void StartAutomatedVolumeCapture()
{
	g_EngineVolumeCaptureActive = true;
}

void StopAutomatedVolumeCapture()
{
	g_EngineVolumeCaptureActive = false;
}

void GranularToggleSkipEveryOtherGrain()
{
	g_GranularSkipGrainTogglePressed = true;
	g_GranularSkipEveryOtherGrain = !g_GranularSkipEveryOtherGrain;
}

void GranularToggleTestTone()
{
	g_GranularToggleTestTonePressed = true;
	g_GranularToneGeneratorEnabled = !g_GranularToneGeneratorEnabled;
}

void audCarAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Vehicles", false);
		bank.AddToggle("Debug Special Flight Mode", &g_DebugSpecialFlightMode);
		bank.AddToggle("Disable Horns", &g_DisableHorns);
		bank.AddToggle("Simulate Drowning", &g_SimulateDrowning);
		bank.AddToggle("Test Force Game Object", &g_TestForceGameObject);		
		bank.AddText("Test Force Game Object Name", &g_TestForceGameObjectName[0], sizeof(g_TestForceGameObjectName), false);
		bank.AddToggle("Force Auto Shutoff Engines", &g_ForceAutoShutoffEngines);
		bank.AddToggle("Audition Wheel Fire", &g_AuditionWheelFire);
		bank.AddToggle("Render vehicle slots", &g_RenderVehicleSlots);
		bank.AddToggle("Breakpoint debug vehicle update", &g_BreakOnVehicleUpdate);
		bank.AddToggle("Show Genre Info", &g_ShowRadioGenres);
		bank.AddToggle("Debug Vehicle Openness", &g_DebugVehicleOpenness);
		bank.AddToggle("Debug Draw Vehicle Water", &g_DebugDrawVehicleWater);		
#if NA_RADIO_ENABLED
		bank.AddSlider("g_LoudVehicleRadioOffset", &g_LoudVehicleRadioOffset, 0.f, 12.f, 0.1f);
#endif
		
		bank.AddSlider("Player Horn Offset", &g_PlayerHornOffset, 0.f, 12.f, 0.1f);
		bank.AddSlider("CheapDistance", &sm_CheapDistance, 0.0f, 40.0f, 1.0f);
		bank.AddButton("Trigger Mobile PreRing", CFA(TriggerMobilePreRing));

		bank.PushGroup("Collisions");
			bank.AddSlider("Clatter Max Acceleration", &g_ClatterMaxAcceleration, 0.0f, 200.f, 1.0f);
			bank.AddSlider("Car Velocity for Ped Impact", &g_CarVelForPedImpact, 0.f, 10.f, 0.1f);
			bank.AddSlider("Damage Impact Velocity", &g_DamageImpactVel, 0.f, 50.f, 1.f);
			bank.AddSlider("Deformation Impact Velocity", &g_DeformationImpactVel, 0.f, 50.f, 1.f);
			bank.AddSlider("Trailer Bump Velocity", &g_TrailerBumpVel, 0.f, 50.f, 1.f);
			bank.AddSlider("LowDynamicImpacts", &g_LowerVehDynamicImpactThreshold, -100.f, 6.f, 0.1f);
			bank.AddSlider("HighDynamicImpacts", &g_UpperVehDynamicImpactThreshold, -100.f, 6.f, 0.1f);
		bank.PopGroup();

		bank.PushGroup("Gadgets");
			bank.AddToggle("Enable Turret Debug", &g_DebugDrawTurret);
			bank.AddToggle("Enable Tow Arm Debug", &g_DebugDrawTowArm);
			bank.AddToggle("Enable Grappling Hook Debug", &g_DebugDrawGrappling);
			bank.AddSlider("Grap - soundAcceleration", &g_GrapSoundAcceleration, 0.f, 70.f, 0.1f);
			bank.AddSlider("Grap - Intensity", &g_GrapIntensity, -1.f, 1.f, 0.1f);
			bank.AddSlider("Vehicle Weapon Ammo Count", &g_WeaponAmmoCountOverride, -1, 100, 1);
			bank.AddButton("Override Vehicle Weapon Ammo Count", CFA(OverrideVehicleWeaponAmmoCount));
		bank.PopGroup();

		bank.PushGroup("Stunt Races");
			bank.AddSlider("Boost Intensity Timeout Ms", &g_StuntBoostIntensityTimeout, 0, 15000, 10);
			bank.AddButton("Trigger Stunt Race Boost", CFA(TestStuntRaceBoost));
			bank.AddButton("Trigger Stunt Race Slow Down", CFA(TestStuntRaceSlowdown));
			bank.AddSlider("Speed Boost Decel Smoothing Duration", &g_StuntSpeedBoostDecelSmoothingDuration, 0, 60000, 100);
			bank.AddSlider("Speed Boost Decel Smoothing", &g_StuntSpeedBoostDecelSmoothing, 0.f, 500.f, 1.f);
		bank.PopGroup();

		bank.PushGroup("Damage");
			bank.AddToggle("Display Damage Debug", &g_DisplayDamageDebug);
			bank.AddToggle("Force Engine Damage", &g_ForceDamage);
			bank.AddToggle("Force Wheel Damage", &g_ForceWheelDamage);
			bank.AddSlider("Forced Engine Damage Factor", &g_ForcedDamageFactor, 0.0f, 1.0f, 0.01f);
			bank.AddToggle("Set All Cars Always Damaged", &g_ForceAllCarsAlwaysDamaged);
			bank.AddToggle("Mute Damage Layers", &g_MuteDamageLayers);
			bank.AddToggle("Mute Fanbelt Damage", &g_MuteFanbeltDamage);
			bank.AddToggle("Mute Granular Damage", &g_MuteGranularDamage);
			bank.AddSlider("Engine Volume Trim", &g_EngineVolumeTrim, -100.0f, 100.f, 0.1f);
			bank.AddSlider("Exhaust Volume Trim", &g_ExhaustVolumeTrim, -100.0f, 100.f, 0.1f);
			bank.AddSlider("Damage Layer Min Pitch", &g_DamageLayerMinPitch, -2000, 2000, 10);
			bank.AddSlider("Damage Layer Max Pitch", &g_DamageLayerMaxPitch, -2000, 2000, 10);	
			bank.AddSlider("Body Damage Factor For Debris", &g_BodyDamageFactorForDebris, 0.f, 1.f, 0.01f);
			bank.AddSlider("Body Damage Factor For Hydraulic Debris", &g_BodyDamageFactorForHydraulicDebris, 0.f, 1.f, 0.01f);			
			bank.AddSlider("Damaged Wheel Min Pitch", &g_DamagedWheelMinPitch, -2000, 2000, 10);
			bank.AddSlider("Damaged Wheel Max Pitch", &g_DamagedWheelMaxPitch, -2000, 2000, 10);	
			bank.AddToggle("Enable Damage Throttle Cut", &g_EnableDamageThrottleCut);
			bank.AddSlider("High Revs Misfire Sound Prob", &g_HighRevsMisfireSoundProb, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Min Throttle Cut Duration", &g_MinDamageThrottleCutDuration, 0.0f, 10.f, 0.01f);
			bank.AddSlider("Max Throttle Cut Duration", &g_MaxDamageThrottleCutDuration, 0.0f, 10.f, 0.01f);
			bank.AddSlider("Short Throttle Cut Prob Min", &g_ShortDamageCutProbMin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Short Throttle Cut Prob Max", &g_ShortDamageCutProbMax, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Min Time Between Short Throttle Cuts", &g_MinTimeBetweenDamageThrottleCutsShort, 0, 60000, 10);	
			bank.AddSlider("Max Time Between Short Throttle Cuts", &g_MaxTimeBetweenDamageThrottleCutsShort, 0, 60000, 10);	
			bank.AddSlider("Min Time Between Long Throttle Cuts", &g_MinTimeBetweenDamageThrottleCuts, 0, 60000, 10);	
			bank.AddSlider("Max Time Between Long Throttle Cuts", &g_MaxTimeBetweenDamageThrottleCuts, 0, 60000, 10);	
		bank.PopGroup();

		bank.PushGroup("Car Mods");
			bank.AddToggle("Draw Vehicles In Mod Shop", &g_DrawVehiclesInModShop);
			bank.AddToggle("Upgrade Engine", &g_ForceUpgradeEngine);
			bank.AddSlider("Engine Bay Upgrade Level", &g_ForcedEngineUpgradeLevel, 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Exhaust", &g_ForceUpgradeExhaust);
			bank.AddSlider("Engine Bay Upgrade Level", &g_ForcedExhaustUpgradeLevel, 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Transmission", &g_ForceUpgradeTransmission);
			bank.AddSlider("Transmission Upgrade Level", &g_ForcedTransmissionUpgradeLevel, 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Turbo", &g_ForceUpgradeTurbo);
			bank.AddSlider("Turbo Upgrade Level", &g_ForcedTurboUpgradeLevel, 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Radio", &g_ForceUpgradeRadio);	
			bank.AddSlider("Radio Upgrade Level", &g_ForcedRadioUpgradeLevel, 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Engine Bay Slot 0", &g_ForceUpgradeEngineBay[0]);
			bank.AddSlider("Engine Bay Slot 0 Upgrade Level", &g_ForcedEngineBayUpgradeLevel[0], 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Engine Bay Slot 1", &g_ForceUpgradeEngineBay[1]);
			bank.AddSlider("Engine Bay Slot 1 Upgrade Level", &g_ForcedEngineBayUpgradeLevel[1], 0.0f, 1.0f, 0.1f);
			bank.AddToggle("Upgrade Engine Bay Slot 2", &g_ForceUpgradeEngineBay[2]);
			bank.AddSlider("Engine Bay Slot 2 Upgrade Level", &g_ForcedEngineBayUpgradeLevel[2], 0.0f, 1.0f, 0.1f);
			bank.AddSlider("Trunk Radio Offset Position", &g_TrunkRadioPositionOffset, 0.0f, 10.0f, 0.1f);			
		bank.PopGroup();

		bank.PushGroup("Revs Boost Effect");
			bank.AddToggle("Revs Boost Enabled", &g_RevsBoostEffectEnabled);
			bank.AddToggle("Enable Debug Rendering", &g_GranularDebugRenderingEnabled);
			bank.AddSlider("Max Pitch Offset", &g_RevsBoostPitchOffset, 0.0f, 2400.0f, 1.0f);
			bank.AddSlider("Max Revs Scalar", &g_RevsBoostRevsScalar, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Pitch Offset Fade In Time", &g_RevsBoostPitchOffsetFadeInTime, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Pitch Offset Fade Out Time", &g_RevsBoostPitchOffsetFadeOutTime, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Revs Scale Fade In Time", &g_RevsBoostRevsScaleFadeInTime, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Revs Scale Fade Out Time", &g_RevsBoostRevsScaleFadeOutTime, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Min Fwd Speed Ratio", &g_RevsBoostEffectFwdSpeedRatio, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Min Rev Ratio", &g_RevsBoostEffectRevRatio, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Angular Velocity Limit", &g_RevsBoostAngularVelocityLimit, 0.0f, 50.0f, 0.01f);	
			bank.AddSlider("Angular Velocity Limit Sensitivity", &g_RevsBoostAngularVelocityLimitSensitivity, 0.0f, 50.0f, 0.01f);	
			bank.AddToggle("Pitch Shift Only in Final Gear", &g_RevsBoostPitchOffsetOnlyInFinalGear);
		bank.PopGroup();

		bank.PushGroup("Vehicle Occlusion");
			bank.AddSlider("InnerUpdateFrames", &sm_InnerUpdateFrames, 0.0f, 10.0f, 1.0f);
			bank.AddSlider("OuterUpdateFrames", &sm_OuterUpdateFrames, 0.0f, 10.0f, 1.0f);
			bank.AddSlider("InnerCheapUpdateFrames", &sm_InnerCheapUpdateFrames, 0.0f, 10.0f, 1.0f);
			bank.AddSlider("OuterCheapUpdateFrames", &sm_OuterCheapUpdateFrames, 0.0f, 10.0f, 1.0f);
		bank.PopGroup();
		
		bank.PushGroup("Surfaces and Road Noise");
			bank.AddToggle("DebugVehicleSkids", &g_DebugVehicleSkids);
			bank.AddText("Player ground material", &g_DrivingMaterialName[0], sizeof(g_DrivingMaterialName));
			bank.AddToggle("Display ground material", &g_DisplayGroundMaterial);
			bank.AddSlider("Spin Ratio For Debris", &g_SpinRatioForDebris, 0.000001f, 10.f, 0.001f);
			bank.AddSlider("Speed For Spin Debris", &g_SpeedForSpinDebris, 0.f, 10.f, 0.1f);
			bank.AddToggle("Disable Road Noise", &g_DisableRoadNoise);
			bank.AddToggle("Disable NPC Road Noise", &g_DisableNPCRoadNoise);
			bank.AddToggle("Show NPC Road Noise State", &g_ShowNPCRoadNoiseState);
			bank.AddSlider("NPC Road Noise Inner Angle", &g_NPCRoadNoiseInnerAngle, 0.0f, 180.0f, 1.0f);
			bank.AddSlider("NPC Road Noise Outer Angle", &g_NPCRoadNoiseOuterAngle, 0.0f, 180.0f, 1.0f);
			bank.AddToggle("Force Shallow Water", &g_ForceShallowWater);
			bank.AddToggle("Display Ridged Surface Info", &g_DisplayRidgedSurfaceInfo);
			bank.AddToggle("Disable Clatter", &g_DisableClatter);
			bank.AddToggle("Display Dirt/Glass", &g_DisplayDirtGlassInfo);
			bank.AddToggle("Force Glassy Surface", &g_ForceGlassySurface);
			bank.AddToggle("Force Dirty Surface", &g_ForceDirtySurface);
			bank.AddToggle("Disable Burnout Skids in Reverse", &g_DisableBurnoutSkidsInReverse);
			bank.AddSlider("Wheel Dirtiness Increase Smooth Rate", &g_WheelDirtinessIncreaseSmoothRate, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Wheel Dirtiness Decrease Smooth Rate", &g_WheelDirtinessDecreaseSmoothRate, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Wheel Glassiness Increase Smooth Rate", &g_WheelGlassinessIncreaseSmoothRate, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Wheel Glassiness Decrease Smooth Rate", &g_WheelGlassinessDecreaseSmoothRate, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Glass Area Depletion Rate", &g_GlassAreaDepletionRate, 0.0f, 1.0f, 0.001f);			
			bank.AddSlider("Main Skid Smoother Increase", &g_MainSkidSmootherIncrease, 0.0f, 100.0f, 0.001f);			
			bank.AddSlider("Main Skid Smoother Decrease", &g_MainSkidSmootherDecrease, 0.0f, 100.0f, 0.001f);	
			bank.AddSlider("Lateral Skid Smoother Increase", &g_LateralSkidSmootherIncrease, 0.0f, 100.0f, 0.001f);
			bank.AddSlider("Lateral Skid Smoother Decrease", &g_LateralSkidSmootherDecrease, 0.0f, 100.0f, 0.001f);
		bank.PopGroup();

		bank.PushGroup("Wind and SFX");
			bank.AddToggle("Debug Passbys", &g_DebugCarPasses);
			bank.AddToggle("Debug Wind Noise", &g_DebugWindNoise);
			bank.AddSlider("Wind Noise Increase Smoothing", &g_WindNoiseSmootherIncreaseRate, 0.0f, 1000000.0f, 10.0f);
			bank.AddSlider("Wind Noise Decrease Smoothing", &g_WindNoiseSmootherDecreaseRate, 0.0f, 1000000.0f, 10.0f);
		bank.PopGroup();

		bank.PushGroup("Sports Car Revs", false);
			bank.AddToggle("Sports Car Revs Enabled", &g_SportsCarRevsEnabled);
			bank.AddToggle("Draw Cars Playing Sports Revs", &g_DrawCarsPlayingSportsRevs);
			bank.AddToggle("Draw Sports Car Passbys", &g_DrawSportsRevsPassbyDebug);
			bank.AddToggle("Force Sports Revs on All Cars", &g_ForceAllCarsPlaySportsRevs);
			bank.AddButton("Trigger Sports Revs", CFA(TestSportsRevs));
			bank.AddSlider("Engine Switch Attack Time", &g_SportsCarRevsEngineSwitchTime, 0, 10000, 10);
		bank.PopGroup();

		bank.PushGroup("Vehicle LODs", false);
			bank.AddToggle("Display Vehicle LODs", &g_ShowEngineStates);
			bank.AddToggle("Display Super Dummy Radius", &g_DrawSuperDummyRadius);
			bank.AddToggle("Display Focus Vehicle", &g_ShowFocusVehicle);
			bank.AddToggle("Show Vehicles with Wave Slots", &g_ShowVehiclesWithWaveSlots);
			bank.AddToggle("Render slot manager", &g_RenderSlotManager);
			bank.AddToggle("Debug Draw DSP Effects", &g_DebugDSPParams);
			bank.AddToggle("Draw Lines to Real Cars", &g_DrawLinesToRealCars);
			bank.AddToggle("Draw Lines to Vehicles with Wave Slots", &g_DrawLinesToVehiclesWithWaveSlots);
			bank.AddToggle("Draw Lines to Vehicles with DSP Effects", &g_DrawLinesToVehiclesWithSubmixVoices);
			bank.AddCombo("Force Player Vehicle LOD", &g_ForcePlayerVehicleLOD, 5, &g_VehicleLODNames[0], 0, NullCB, "" );
			bank.AddSlider("Max Dummy Range Scale", &g_MaxDummyRangeScale, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Max Granular Range Scale", &g_MaxGranularRangeScale, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Max Real Range Scale", &g_MaxRealRangeScale, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Real Scale Increase Rate", &g_ActivationScaleIncreaseRate, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Real Scale Decrease Rate", &g_ActivationScaleDecreaseRate, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Granular Scale Increase Rate", &g_GranularActivationScaleIncreaseRate, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Granular Scale Decrease Rate", &g_GranularActivationScaleDecreaseRate, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Dummy Scale Increase Rate", &g_DummyActivationScaleIncreaseRate, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Dummy Scale Decrease Rate", &g_DummyActivationScaleDecreaseRate, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Off Screen Visibility Scalar", &g_OffscreenVisiblityScalar, 0.0f, 10.0f, 0.01f);	
			bank.AddSlider("Engine Switch Fade In Time", &g_EngineSwitchFadeInTime, 0, 10000, 10);	
			bank.AddSlider("Engine Switch Fade Out Time", &g_EngineSwitchFadeOutTime, 0, 10000, 10);	
			bank.AddSlider("NPC Road Noise Attack Time", &g_NPCRoadNoiseAttackTime, 0, 10000, 10);	
			bank.AddSlider("NPC Road Noise Release Time", &g_NPCRoadNoiseReleaseTime, 0, 10000, 10);	
			bank.AddSlider("Min Time At Desired LOD", &g_MinTimeAtDesiredLOD, 0.0f, 10.0f, 0.01f);	
			bank.AddSlider("Min Time At Current LOD", &g_MinTimeAtCurrentLOD, 0.0f, 10.0f, 0.01f);	
			bank.AddSlider("LOD Speed Bias", &g_LODSpeedBias, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("LOD Min Speed Ratio", &g_LODMinSpeedRatio, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("LOD Max Speed Ratio", &g_LODMaxSpeedRatio, 0.0f, 1.0f, 0.01f);
			bank.AddToggle("Show Time Sliced Vehicles", &g_RenderTimeSlicedVehicles);
		bank.PopGroup();
		
		bank.PushGroup("Engines",false);
			bank.PushGroup("Mixing",false);
				bank.AddToggle("EngineMixMode", &g_DisableVolumeCategory);
				bank.AddToggle("TreatAsPedCar", &g_TreatAsPedCar);
				bank.AddToggle("TreatAsDummyCar", &g_TreatAsDummyCar);
				bank.AddToggle("Enable Debug Focusing", &g_VehicleAudioAllowEntityFocus);
				bank.AddToggle("Force Real LOD on Focus Vehicle", &g_ForceRealLodOnFocusEntity);
			bank.PopGroup();

			bank.PushGroup("Transmission",false);
				bank.AddToggle("Force Wobble", &g_ForceTransWobble);
				bank.AddSlider("Wobble Frequency", &g_TransWobbleFreq, 0.0f, 200.0f, 1.0f);
				bank.AddSlider("Wobble Frequency Variance", &g_TransWobbleFreqVariance, 0.0f, 200.0f, 1.0f);
				bank.AddSlider("Wobble Magnitude", &g_TransWobbleMag, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Wobble Magnitude Variance", &g_TransWobbleMagVariance, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Wobble Length", &g_TransWobbleLength, 0.0f, 100.0f, 0.1f);
				bank.AddSlider("Wobble Length Variance", &g_TransWobbleLengthVariance, 0.0f, 100.0f, 0.1f);
			bank.PopGroup();

			bank.PushGroup("DSP Effects",false);
				bank.AddToggle("Debug Draw", &g_DebugDSPParams);
				bank.AddToggle("Mute DSP Effects", &g_DisableEngineExhaustDSP);
				bank.AddToggle("Disable Player DSP Effects", &g_DisablePlayerVehicleDSP);
				bank.AddToggle("Disable NPC DSP Effects", &g_DisableNPCVehicleDSP);
				bank.AddToggle("Auto Re-apply DSP Presets", &g_AutoReapplyDSPPresets);				
				bank.AddToggle("Allow Synths on Network Vehicles", &g_AllowSynthsOnNetworkVehicles);
				bank.AddToggle("Force Synths On All Engines", &g_ForceSynthsOnAllEngines);
			bank.PopGroup();

			bank.PushGroup("Electric Engines",false);
				bank.AddSlider("Speed Volume Scale", &g_ElectricEngineSpeedVolScale, 0.0f, 1.0f, 0.1f);
				bank.AddSlider("Speed Loop Speed Bias", &g_ElectricEngineSpeedLoopSpeedBias, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Speed Loop Speed Scalar", &g_ElectricEngineSpeedLoopSpeedApplyScalar, 0.0f, 10.0f, 0.1f);
				bank.AddSlider("Boost Volume Scale", &g_ElectricEngineBoostVolScale, 0.0f, 1.0f, 0.1f);
				bank.AddSlider("Revs Off Volume Scale", &g_ElectricEngineRevsOffVolScale, 0.0f, 1.0f, 0.1f);
			bank.PopGroup();

			bank.PushGroup("Volume Controls",false);
				bank.AddSlider("Engine Volume Trim", &g_EngineVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Exhaust Volume Trim", &g_ExhaustVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Gearbox Volume Trim", &g_GearboxVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Turbo Volume Trim", &g_TurboVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Granular Accel Volume Trim", &g_GranularAccelVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Granular Decel Volume Trim", &g_GranularDecelVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddSlider("Granular Idle Volume Trim", &g_GranularIdleVolumeTrim, -100.0f, 100.f, 0.1f);
				bank.AddToggle("Disable Coning", &g_DisableConing);
				bank.AddSlider("Base Rolloff Scale Player", &g_BaseRollOffScalePlayer, 0.0f, 100.0f, 0.1f);
				bank.AddSlider("Base Rolloff Scale NPC", &g_BaseRollOffScaleNPC, 0.0f, 100.0f, 0.1f);
				bank.AddButton("Start Automated Volume Capture", CFA(StartAutomatedVolumeCapture));
				bank.AddButton("Stop Automated Volume Capture", CFA(StopAutomatedVolumeCapture));
				bank.AddSlider("Volume Capture Start Index", &audCarAudioEntity::sm_VolumeCaptureVehicleStartIndex, 0, 300, 1);
			bank.PopGroup();

			bank.PushGroup("Wobbles",false);
				bank.AddToggle("Test Wobble", &g_TestWobble);
				bank.AddSlider("Length", &g_TestWobbleLength, 1, 10000, 1);
				bank.AddSlider("Length Variance", &g_TestWobbleLengthVariance, 0.0f, 100.f, 0.1f);
				bank.AddSlider("Speed", &g_TestWobbleSpeed, 0.0f, 10.f, 0.1f);
				bank.AddSlider("Speed Variance", &g_TestWobbleSpeedVariance, 0.0f, 100.f, 0.1f);
				bank.AddSlider("Pitch", &g_TestWobblePitch, 0.0f, 10.f, 0.1f);
				bank.AddSlider("Pitch Variance", &g_TestWobblePitchVariance, 0.0f, 10.f, 0.1f);
				bank.AddSlider("Volume", &g_TestWobbleVolume, 0.0f, 10.f, 0.1f);
				bank.AddSlider("Volume Variance", &g_TestWobbleVolumeVariance, 0.0f, 100.f, 0.1f);
			bank.PopGroup();

			bank.PushGroup("Granular Controls ",false);
				bank.AddSlider("Granular X Value Smooth Rate", &g_GranularXValueSmoothRate, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Granular Sliding Window Size Hz Fraction", &g_GranularSlidingWindowSizeHzFraction, 0.0f, 1.0f, 0.001f);
				bank.AddSlider("Granular Sliding Window Min Grains", &g_GranularSlidingWindowMinGrains, 0, 30, 1);
				bank.AddSlider("Granular Sliding Window Max Grains", &g_GranularSlidingWindowMaxGrains, 0, 30, 1);			
				bank.AddSlider("Granular Min Repeat Rate", &g_GranularMinRepeatRate, 0, 10, 1);			
				bank.AddSlider("Default Granular Change Rate for Loops", &audGranularMix::s_DefaultGranularChangeRateForLoops, 0.0f, 100.0f, 0.1f);			
				bank.AddSlider("Granular Physics Revs Multiplier", &g_GranularEngineRevMultiplier, 0.5f, 2.0f, 0.01f);
				bank.AddCombo("Grain Playback Style", &g_GrainPlaybackStyle, 4, &g_GranularPlaybackStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Grain Mix Style", &g_GranularMixStyle, 3, &g_GranularMixStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Accel-Decel Crossfade Style", &g_GranularMixCrossfadeStyle, 2, &g_GranularCrossfadeStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Inter-Grain Crossfade Style", &g_GranularInterGrainCrossfadeStyle, 2, &g_GranularCrossfadeStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Loop-Grain Crossfade Style", &g_GranularLoopGrainCrossfadeStyle, 2, &g_GranularCrossfadeStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Loop-Loop Crossfade Style", &g_GranularLoopLoopCrossfadeStyle, 2, &g_GranularCrossfadeStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Grain Randomisation Style", &g_GranularRandomisationStyle, 5, &g_GranularRandomisationStyleNames[0], 0, NullCB, "" );
				bank.AddCombo("Idle Grain Randomisation Style", &g_GranularIdleRandomisationStyle, 5, &g_GranularRandomisationStyleNames[0], 0, NullCB, "" );				
				bank.AddSlider("Idle to Main Smooth Rate (Increase)", &g_IdleMainSmootherRateIncrease, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Idle to Main Smooth Rate (Decrease)", &g_IdleMainSmootherRateDecrease, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Granular Loop Below Bias", &audGranularMix::s_GranularLoopBelowBias, 0.0f, 5.0f, 0.1f);
				bank.AddSlider("Granular Loop To Grain Smooth Rate", &audGranularMix::s_GranularLoopToGrainChangeRate, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Granular Grain To Loop Smooth Rate", &audGranularMix::s_GranularGrainToLoopChangeRate, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Granular Accel/Decel Smooth Rate", &g_GranularAccelDecelSmoothRate, 0.0f, 1.0f, 0.01f);
				bank.AddToggle("Enable Granular Limiter", &g_GranularLimiterEnabled);
				bank.AddToggle("Force Granular Limiter", &g_GranularForceRevLimiter);
				bank.AddToggle("Enable Granular Wobbles", &g_GranularWobblesEnabled);
				bank.AddToggle("Force Granular Low Quality", &g_GranularForceLowQuality);
				bank.AddToggle("Force Granular High Quality", &g_GranularForceHighQuality);
				bank.AddToggle("Simulate Player Leaving/Entering Vehicle", &g_SimulatePlayerEnteringVehicle);		
				bank.AddSlider("Granular Quality Change Crossfade Speed", &g_GranularQualityCrossfadeSpeed, 0.0f, 10.0f, 0.01f);
				bank.AddToggle("Enable Granular NPC Engines", &g_GranularEnableNPCGranularEngines);
				bank.AddToggle("Force Granular NPC Engines", &g_ForceGranularNPCEngines);
				bank.AddToggle("Render Granular NPC Engines", &g_RenderGranularNPCEngines);
				bank.AddToggle("Granular LOD Switch Equal Power X-Fade", &g_GranularLODEqualPowerCrossfade);				
				bank.AddToggle("Mute Granular Synths", &g_ForceMuteGranularSynths);
				bank.AddSlider("Granular Loop Randomisation Granular CR", &audGranularMix::s_LoopRandomisationGranularChangeRate, 0.0f, 50.0f, 0.1f);
				bank.AddToggle("Granular Mid-Loop Randomisation Enabled", &audGranularSubmix::s_GranularMidLoopRandomisationEnabled);
				bank.AddToggle("Enable Granular Loop Peak Normalisation", &audGranularMix::s_GranularPeakNormaliseLoops);
				bank.AddButton("Toggle Granular Test Tone", CFA(GranularToggleTestTone));
				bank.AddButton("Skip Every Other Grain", CFA(GranularToggleSkipEveryOtherGrain));
				bank.AddToggle("Force Granular Starvation", &audGranularSubmix::s_ForceGranularSubmixStarvation);
				bank.AddToggle("Set Granular Engine Idle Native", &g_SetGranularEngineIdleNative);
				bank.AddToggle("Set Granular Exhaust Idle Native", &g_SetGranularExhaustIdleNative);				
			bank.PopGroup();

			bank.PushGroup("Debug Rendering",false);
				bank.AddToggle("Enable Granular Debug Rendering", &g_GranularDebugRenderingEnabled);
				bank.AddToggle("Enable Granular Idle Debug Rendering", &g_GranularIdleDebugRenderingEnabled);
				bank.AddToggle("Enable Non-Granular Debug Rendering", &g_NonGranularDebugRenderingEnabled);
#if __PS3
				bank.AddToggle("Draw Granular Vram Loaders", &audGranularMix::s_DebugDrawVramGrainLoaders);
				bank.AddToggle("Draw Loop Vram Loaders", &audGranularMix::s_DebugDrawVramLoopLoaders);
#endif
			bank.PopGroup();

			bank.PushGroup("Granular Loop Editing",false);
				bank.AddToggle("Enable Granular Debug Rendering", &g_GranularDebugRenderingEnabled);
				bank.AddToggle("Loop Editor Active", &audGrainPlayer::s_LoopEditorActive);
				bank.AddSlider("Loop Editor Mix Index", &audGrainPlayer::s_LoopEditorMixIndex, 0, 10, 1);
				bank.AddSlider("Loop Editor Loop Index", &audGrainPlayer::s_LoopEditorLoopIndex, 0, 10, 1);
				bank.AddButton("Shift Loop Left", CFA(GranularLoopEditorShiftLeft));
				bank.AddButton("Shift Loop Right", CFA(GranularLoopEditorShiftRight));
				bank.AddSlider("Shift Amount", &audGrainPlayer::s_LoopEditorShiftAmount, 1, 100, 1);
				bank.AddButton("Grow Loop", CFA(GranularLoopEditorGrowLoopPressed));
				bank.AddButton("Shrink Loop", CFA(GranularLoopEditorShrinkLoopPressed));
				bank.AddButton("Create New Loop", CFA(GranularLoopEditorCreateLoopPressed));
				bank.AddButton("Delete Selected Loop", CFA(GranularLoopEditorDeleteLoopPressed));
				bank.AddButton("Export Loop Data", CFA(GranularLoopEditorExportLoopData));
				bank.AddToggle("Force Rev/Throttle Values", &g_GranularEditorControlRevsThrottle);
				bank.AddSlider("Revs", &g_GranularEditorRevs, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Throttle", &g_GranularEditorThrottle, 0.0f, 1.0f, 0.01f);
			bank.PopGroup();

			bank.PushGroup("Misc Options",false);
				bank.AddToggle("Link Exhaust Pops to Audio Revs", &g_LinkExhaustPopsToAudioRevs);
				bank.AddSlider("Rocket Boost Cooldown Time", &g_RocketBoostCooldownTime, 0, 60000, 100);
				bank.AddToggle("Show Engine/Exhaust", &g_ShowEngineExhaust);
				bank.AddToggle("Position Interior Engine/Exhaust at Listener", &g_InteriorEngineExhaustAtListener);
				bank.AddToggle("Interior Engine/Exhaust Panned", &g_InteriorEngineExhaustPanned);
				bank.AddToggle("Mute Player Vehicle", &g_MutePlayerVehicle);
				bank.AddSlider("NPC Revs Scalar", &g_NPCVehicleRevsScalar, 0.0f, 10.0f, 0.01f);
				bank.AddSlider("Engine Start Duration", &g_EngineStartDuration, 0, 10000, 1);
				bank.AddSlider("Engine Stop Duration", &g_EngineStopDuration, 0, 10000, 1);
				bank.AddSlider("Revs Increase Rate", &g_RevMaxIncreaseRate, 0.1f, 1000.f, 1.f);
				bank.AddSlider("Revs Force Stop Decrease Rate", &g_RevMaxForceStopDecreaseRate, 0.1f, 1000.f, 1.f);
				bank.AddSlider("Revs Decrease Rate", &g_RevMaxDecreaseRate, 0.1f, 1000.f, 1.f);
				bank.AddSlider("Under Load Revs Smoothing", &g_UnderLoadRevsSmootherScalar, 0.1f, 100.f, 0.1f);
				bank.AddSlider("Frequency Smoothing Rate", &sm_FrequencySmoothingRate, -1.f, 100.f, 0.1f);
				bank.AddToggle("Display engine debug text", &sm_ShouldDisplayEngineDebug);
				bank.AddToggle("Flip player volume cones", &sm_ShouldFlipPlayerCones);
				bank.AddSlider("BrakeLinearVolumeSmoothRate", &g_BrakeLinearVolumeSmootherRate, 0.1f, 10.0f, 0.1f);
				bank.AddSlider("BonnetCamGTOffset", &g_BonnetCamGTOffset, 0.0f, 12.0f, 0.1f);
				bank.AddSlider("TurboSpinUp", &g_TurboSpinUp, 0.0f, 10.f, 0.1f);
				bank.AddSlider("TurboSpinDown", &g_TurboSpinDown, 0.0f, 10.f, 0.1f);
				bank.AddSlider("DumpValvePressureThresh", &g_DumpValvePressureThreshold, 0.0f, 1.0f, 0.1f);
				bank.AddSlider("ExhaustThrottleAtten", &g_audExhaustThrottleVolumeRange, 0.0f, 100.f, 0.1f);
				bank.AddSlider("ExhaustProximityAffectRange", &g_ExhaustProximityEffectRange, 1.0f, 50.f, 0.1f);
				bank.AddToggle("Override Sim", &g_OverrideSim);
				bank.AddSlider("OverriddenThrottle", &g_OverriddenThrottle, 0.f, 1.f, 0.001f);
				bank.AddSlider("OverriddenRevs", &g_OverriddenRevs, 0.f, 1.f, 0.001f);
				bank.AddSlider("Mission Vehicle Revs Scalar", &g_MissionVehicleRevsScaler, 0.f, 3.f, 0.1f);
				bank.AddToggle("ForceRandomBlips", &g_ForceRandomBlips);
				bank.AddSlider("g_RandomBlipMaxSpeed", &g_RandomBlipMaxSpeed, 0.0f, 20.0f, 0.01f);
				bank.AddSlider("g_MinSqDistForFastBlips", &g_MinSqDistForFastBlips, 0.0f, 10000.0f, 1.0f);
				bank.AddSlider("ExhaustPopsThresh", &g_ExhaustPopRateThreshold, 0.01f, 100.f, 0.1f);
				bank.AddSlider("Starting Ignition Max Hold", &audVehicleAudioEntity::sm_StartingIgnitionMaxHold, 0.0f, 10.f, 0.01f);
				bank.AddSlider("Starting Ignition Min Hold", &audVehicleAudioEntity::sm_StartingIgnitionMinHold, 0.0f, 10.f, 0.01f);
				bank.AddToggle("BlipPlayerThrottle", &g_BlipPlayerThrottle);
				bank.AddToggle("Force Throttle Blips", &g_ForceEnableBlips);
				bank.AddToggle("Disable Throttle Blips", &g_DisableThrottleBlips);
				bank.AddToggle("Toggle NOS Boost", &g_ToggleNos);
				bank.AddToggle("Test Backfire", &g_TestBackfire);
				bank.AddToggle("Test Backfire Banger", &g_TestBackfireBanger);
				bank.AddToggle("Show Clustered Vehicles", &g_DisplayClusteredVehicles);
				bank.AddSlider("Min Ignition Time", &sm_IgnitionMinHold, 0.01f, 2.f, 0.01f);
				bank.AddSlider("Max Ignition Time", &sm_IgnitionMaxHold, 0.01f, 5.f, 0.01f);
			bank.PopGroup();
		bank.PopGroup();

		bank.PushGroup("Suspension",false);
			bank.AddSlider("MinSuspensionStartOffset", &sm_MaxSuspensionStartOffset, 0.f,100.f,1.f);
			bank.AddToggle("Mute Suspension", &g_MuteSuspension);
			bank.AddSlider("Hydraulics Modified Timeout Ms", &g_HydraulicsModifiedTimeoutMs, 0,10000,10);			
			bank.AddSlider("Hydraulics Activation Delay Frames", &g_NumHydraulicActivationDelayFrames, 0,50,1);						
			bank.AddToggle("Force NPC Hydraulics", &g_ForceNPCHydraulicSounds);			
			bank.AddToggle("Print Hydraulic Impact Info", &g_DebugHydraulicSounds);			
			bank.AddToggle("Override Hydraulic Suspension Health", &g_OverrideHydraulicSuspensionDamage);
			bank.AddSlider("Hydraulic Suspension Health", &g_OverridenHydraulicSuspensionDamage, 0.f,1.f,0.01f);			
			bank.AddSlider("Hydraulic Jump Land Suppression Time", &g_HydraulicsJumpLandSuppressionTime, 0,5000,10);			
			bank.AddSlider("Suspension Loop Threshold", &g_HydraulicSuspensionAngleLoopTriggerThreshold, 0.f,10.f,0.01f);
			bank.AddSlider("Suspension Loop Threshold Remote", &g_HydraulicSuspensionAngleLoopTriggerThresholdRemoteVehicle, 0.f,10.f,0.01f);			
			bank.AddSlider("Suspension ReTrigger Threshold", &g_HydraulicSuspensionAngleLoopReTriggerThreshold, 0.f,100.f,0.01f);
			bank.AddSlider("Suspension ReTrigger Threshold Remote", &g_HydraulicSuspensionAngleLoopReTriggerThresholdRemoteVehicle, 0.f,100.f,0.01f);
		bank.PopGroup();

		bank.PushGroup("Openness",false);
			bank.AddSlider("Door Cone Inner Angle Scaling", &sm_DoorConeInnerAngleScaling, 0.0f, 1.0f, 0.1f);
			bank.AddSlider("Door Cone Outer Angle", &sm_DoorConeOuterAngle, 0.0f, 360.0f, 1.0f);
			bank.AddToggle("Show Door Ratios", &g_ShowDoorRatios);
		bank.PopGroup();

		bank.PushGroup("Rolling cars",false);
			bank.AddSlider("g_MinCarRollImpulseMag", &g_MinCarRollImpulseMag, 0.0f, 100.0f, 0.01f);
			bank.AddSlider("g_MaxCarRollImpulseMag", &g_MaxCarRollImpulseMag, 0.0f, 100.0f, 0.01f);
			bank.AddSlider("g_MinCarRollSpeed", &g_MinCarRollSpeed, 0.0f, 100.0f, 0.01f);
			bank.AddSlider("g_MaxCarRollSpeed", &g_MaxCarRollSpeed, 0.0f, 100.0f, 0.01f);
		bank.PopGroup();

		bank.PushGroup("Optimisation Tests", false);
			bank.AddSlider("Max Real Vehicles", &g_MaxVehicleSubmixes, 0, 100, 1);
			bank.AddSlider("Max Dummy Vehicles", &g_MaxDummyVehicles, 0, 100, 1);
			bank.AddToggle("Draw Vehicle LOD Quadrants", &g_DrawVehicleLODQuadrants);
			bank.AddSlider("Slow Streaming Test Delay Time", &g_WaveSlotStreamingDelayTime, 0.0f, 20.0f, 0.1f);
		bank.PopGroup();

		audVehicleCollisionAudio::AddWidgets(bank);


	bank.PopGroup();
}
#endif


