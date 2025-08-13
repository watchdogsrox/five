// 
// audio/vehicleaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "debugaudio.h"

#include "audio/ambience/ambientaudioentity.h"
#include "audio/environment/environment.h"
#include "audio/environment/environmentgroup.h"
#include "caraudioentity.h"
#include "heliaudioentity.h"
#include "trainaudioentity.h"
#include "boataudioentity.h"
#include "planeaudioentity.h"
#include "traileraudioentity.h"
#include "dynamicmixer.h"
#include "frontendaudioentity.h"
#include "northaudioengine.h"
#include "policescanner.h"
#include "radioaudioentity.h"
#include "scriptaudioentity.h"
#include "vehicleaudioentity.h"
#include "vehiclereflectionsaudioentity.h"
#include "camera/helpers/switch/BaseSwitchHelper.h"
#include "vfx/systems/VfxWater.h"

#include "vehicleengine/vehicleengine.h"
#include "vehicles/Trailer.h"
#include "audioengine/engine.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "control/replay/audio/SoundPacket.h"
#include "control/replay/Audio/CollisionAudioPacket.h"
#include "modelinfo/ModelSeatInfo.h"
#include "debug/DebugScene.h"
#include "game/weather.h"
#include "weatheraudioentity.h"
#include "animation/MoveVehicle.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "game/ModelIndices.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/VehicleGadgets.h"
#include "glassaudioentity.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedHelmetComponent.h"
#include "renderer/Water.h"
#include "radiostation.h"
#include "vehicles/train.h"
#include "Vehicles/planes.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "vehicleai/task/taskvehicleanimation.h"
#include "Vehicles/VehicleDefines.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "audiosynth/synthcore.h" 
#include "renderer/River.h"
#include "system/nelem.h"
#include "vfx/systems/VfxWheel.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "script/script_hud.h"
#include "network/Events/NetworkEventTypes.h"
#include "control/replay/Vehicle/VehiclePacket.h"


AUDIO_VEHICLES_OPTIMISATIONS()

EXT_PF_GROUP(VehicleAudioTimings);
PF_TIMER(UpdateRoadNoise, VehicleAudioTimings);
EXT_PF_GROUP(VehicleAudioEntity);
PF_COUNTER(NumNPCRoadNoise, VehicleAudioEntity);
PF_COUNTER(NumRealVehicles, VehicleAudioEntity);
PF_COUNTER(NumDisabledVehicles, VehicleAudioEntity);
PF_COUNTER(NumSuperDummyVehicles, VehicleAudioEntity);
PF_COUNTER(NumDummyVehicles, VehicleAudioEntity);
PF_VALUE_INT(NumGranularEnginesLastFrame, VehicleAudioEntity);
PF_VALUE_INT(NumEnginesUsingSubmixesLastFrame, VehicleAudioEntity);
PF_VALUE_INT(NumVehiclesInActivationRange, VehicleAudioEntity);
PF_VALUE_INT(NumVehiclesInGranularRange, VehicleAudioEntity);
PF_VALUE_FLOAT(RealRadiusScalar, VehicleAudioEntity);
PF_VALUE_FLOAT(GranularRadiusScalar, VehicleAudioEntity);
PF_VALUE_FLOAT(RainRadiusScalarQ1, VehicleAudioEntity);
PF_VALUE_FLOAT(RainRadiusScalarQ2, VehicleAudioEntity);
PF_VALUE_FLOAT(RainRadiusScalarQ3, VehicleAudioEntity);
PF_VALUE_FLOAT(RainRadiusScalarQ4, VehicleAudioEntity);
PF_VALUE_FLOAT(WaterSlapRadiusScalarQ1, VehicleAudioEntity);
PF_VALUE_FLOAT(WaterSlapRadiusScalarQ2, VehicleAudioEntity);
PF_VALUE_FLOAT(WaterSlapRadiusScalarQ3, VehicleAudioEntity);
PF_VALUE_FLOAT(WaterSlapRadiusScalarQ4, VehicleAudioEntity);
PF_VALUE_FLOAT(DummyRadiusScalarQ1, VehicleAudioEntity);
PF_VALUE_FLOAT(DummyRadiusScalarQ2, VehicleAudioEntity);
PF_VALUE_FLOAT(DummyRadiusScalarQ3, VehicleAudioEntity);
PF_VALUE_FLOAT(DummyRadiusScalarQ4, VehicleAudioEntity);

PF_VALUE_FLOAT(FrontWheelSteeringAngle, VehicleAudioEntity);
PF_VALUE_FLOAT(FrontWheelSlipAngle, VehicleAudioEntity);
PF_VALUE_FLOAT(DriftAngle, VehicleAudioEntity);
PF_VALUE_FLOAT(UndersteerFactor, VehicleAudioEntity);
PF_VALUE_FLOAT(ClatterRatioValue, VehicleAudioEntity);
PF_VALUE_FLOAT(ClatterAcceleration, VehicleAudioEntity);

audWaveSlotManager audVehicleAudioEntity::sm_StandardWaveSlotManager;
audWaveSlotManager audVehicleAudioEntity::sm_HighQualityWaveSlotManager;
audWaveSlotManager audVehicleAudioEntity::sm_LowLatencyWaveSlotManager;
audVehicleAudioEntity::audVehicleSubmix audVehicleAudioEntity::sm_VehicleEngineSubmixes[EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1];
audVehicleAudioEntity::audVehicleSubmix audVehicleAudioEntity::sm_VehicleExhaustSubmixes[EFFECT_ROUTE_VEHICLE_EXHAUST_MAX - EFFECT_ROUTE_VEHICLE_EXHAUST_MIN + 1];
audSmoother audVehicleAudioEntity::sm_HighSpeedSceneApplySmoother;
audSmoother audVehicleAudioEntity::sm_OpennessSmoother;

bool audVehicleAudioEntity::sm_IsSpectatorModeActive = false;
bool audVehicleAudioEntity::sm_TriggerStuntRaceSpeedBoostScene = false;
bool audVehicleAudioEntity::sm_TriggerStuntRaceSlowDownScene = false;
bool audVehicleAudioEntity::sm_IsInCarMeet = false;
audSoundSet audVehicleAudioEntity::sm_VehiclesInWaterSoundset;
f32 audVehicleAudioEntity::sm_DownSpeedThreshold = 4.f;
f32 audVehicleAudioEntity::sm_FwdSpeedThreshold = 1.f;
f32 audVehicleAudioEntity::sm_SizeThreshold = 7.f;
f32 audVehicleAudioEntity::sm_BigSizeThreshold = 100.f;
f32 audVehicleAudioEntity::sm_CombineSpeedThreshold = 300.f;
f32 audVehicleAudioEntity::sm_BicycleCombineSpeedMediumThreshold = 150.f;
f32 audVehicleAudioEntity::sm_BicycleCombineSpeedBigThreshold = 300.f;

u32 audVehicleAudioEntity::sm_TimeTargetLockStarted = 0;
u32 audVehicleAudioEntity::sm_TimeMissleFiredStarted = 0;
u32 audVehicleAudioEntity::sm_TimeAllClearStarted = 0;
u32 audVehicleAudioEntity::sm_TimeAcquiringTargetStarted = 0;
u32 audVehicleAudioEntity::sm_TimeATargetAcquiredStarted = 0;

f32 audVehicleAudioEntity::sm_HighSpeedReqVehicleVelocity = 50.0f;
u32 audVehicleAudioEntity::sm_HighSpeedMaxGearOffset = 1;
f32 audVehicleAudioEntity::sm_HighSpeedApplyStart = 0.85f;
f32 audVehicleAudioEntity::sm_HighSpeedApplyEnd = 0.95f;
u32 audVehicleAudioEntity::sm_InvHighSpeedSmootherIncreaseRate = 10000;
u32 audVehicleAudioEntity::sm_InvHighSpeedSmootherDecreaseRate = 100;
u32 audVehicleAudioEntity::sm_AlarmIntervalInMs = 400;
u32 audVehicleAudioEntity::sm_ScriptVehicleRequest = 0;
s32 audVehicleAudioEntity::sm_ScriptVehicleRequestScriptID = -1;
audDynamicWaveSlot* audVehicleAudioEntity::sm_ScriptVehicleWaveSlot = NULL;

CVehicle* audVehicleAudioEntity::sm_PlayerPedTargetVehicle = NULL;
audScene* audVehicleAudioEntity::sm_HighSpeedScene = NULL;
audScene* audVehicleAudioEntity::sm_ExhaustProximityScene = NULL;
audScene* audVehicleAudioEntity::sm_VehicleInteriorScene = NULL;
audScene* audVehicleAudioEntity::sm_VehicleGeneralScene = NULL;
audScene* audVehicleAudioEntity::sm_VehicleBonnetCamScene = NULL;
audScene* audVehicleAudioEntity::sm_VehicleFirstPersonTurretScene = NULL;
audScene* audVehicleAudioEntity::sm_OnMovingTrainScene = NULL;
audScene* audVehicleAudioEntity::sm_StuntTunnelScene = NULL;
audScene* audVehicleAudioEntity::sm_StuntRaceSpeedUpScene = NULL;
audScene* audVehicleAudioEntity::sm_StuntRaceSlowDownScene = NULL;
audVehicleType audVehicleAudioEntity::sm_StuntRaceVehicleType = AUD_VEHICLE_CAR;

f32 audVehicleAudioEntity::sm_PlayerVehicleOpenness = 1.0f;
f32 audVehicleAudioEntity::sm_PlayerVehicleProximityRatio = 0.0f;
f32 audVehicleAudioEntity::sm_CheapDistance = 30.0f;
f32 audVehicleAudioEntity::sm_DoorConeInnerAngleScaling = 0.666f;
f32 audVehicleAudioEntity::sm_DoorConeOuterAngle = 110.0f;
audCurve audVehicleAudioEntity::sm_InteriorOpenToCutoffCurve;
audCurve audVehicleAudioEntity::sm_RoofToOpennessCurve;
audCurve audVehicleAudioEntity::sm_InteriorOpenToVolumeCurve;
audSoundSet audVehicleAudioEntity::sm_ExtrasSoundSet;
audSoundSet audVehicleAudioEntity::sm_LightsSoundSet;
audSoundSet audVehicleAudioEntity::sm_MissileLockSoundSet;
audCurve audVehicleAudioEntity::sm_RoadNoiseDetailVolCurve, audVehicleAudioEntity::sm_RoadNoiseFastVolCurve, audVehicleAudioEntity::sm_RoadNoiseSteeringAngleToAttenuation, audVehicleAudioEntity::sm_RoadNoiseSteeringAngleCRToVolume;
audCurve audVehicleAudioEntity::sm_RoadNoisePitchCurve, audVehicleAudioEntity::sm_RoadNoiseSingleVolCurve, audVehicleAudioEntity::sm_RoadNoiseBicycleVolCurve;
audCurve audVehicleAudioEntity::sm_SkidSpeedRatioToPitch, audVehicleAudioEntity::sm_SkidDriftAngleToVolume, audVehicleAudioEntity::sm_RecentSpeedToSettleVolume, audVehicleAudioEntity::sm_RecentSpeedToSettleReleaseTime;
audCurve audVehicleAudioEntity::sm_SkidFwdSlipToMainVol;
audCurve audVehicleAudioEntity::sm_SlipToUndersteerVol;
audSoundSet audVehicleAudioEntity::sm_TarmacSkidSounds;
audSoundSet audVehicleAudioEntity::sm_ClatterSoundSet;
audSoundSet audVehicleAudioEntity::sm_ChassisStressSoundSet;
audCurve audVehicleAudioEntity::sm_NPCRoadNoiseRelativeSpeedToVol;
f32 audVehicleAudioEntity::sm_FrequencySmoothingRate = 18.f;
u32 g_SuperDummyRadiusSq = 50 * 50;

audSound* audVehicleAudioEntity::sm_InCarViewWind = NULL;

// time in seconds - for when the vehicle isn't going to start this time
f32 audVehicleAudioEntity::sm_IgnitionMinHold = 0.8f;
f32 audVehicleAudioEntity::sm_IgnitionMaxHold = 1.3f;
// and these ones for when it is
f32 audVehicleAudioEntity::sm_StartingIgnitionMinHold = 0.3f;
f32 audVehicleAudioEntity::sm_StartingIgnitionMaxHold = 0.6f;

u32 audVehicleAudioEntity::sm_LodSwitchFadeTime = 2000;
u32 audVehicleAudioEntity::sm_ScheduledVehicleSpawnFadeTime = 5000;
u32 audVehicleAudioEntity::sm_NumVehiclesInGranularActivationRange = 0;
u32 audVehicleAudioEntity::sm_NumVehiclesInGranularActivationRangeLastFrame = 0;
f32 audVehicleAudioEntity::sm_GranularActivationRangeScale = 1.0f;
u32 audVehicleAudioEntity::sm_NumVehiclesInActivationRange = 0;
u32 audVehicleAudioEntity::sm_NumVehiclesInActivationRangeLastFrame = 0;
u32 audVehicleAudioEntity::sm_NumDummyVehiclesLastFrame = 0;
u32 audVehicleAudioEntity::sm_NumDummyVehiclesCounter = 0;
f32 audVehicleAudioEntity::sm_ActivationRangeScale = 1.0f;
u32 audVehicleAudioEntity::sm_NumVehiclesInDummyRange[NumQuadrants];
f32 audVehicleAudioEntity::sm_DummyRangeScale[NumQuadrants];
s32 audVehicleAudioEntity::sm_FramesSincePlayerVehicleStarvation = 0;
u32 audVehicleAudioEntity::sm_NumVehiclesInRainRadius[NumQuadrants];
f32 audVehicleAudioEntity::sm_RainRadiusScalar[NumQuadrants];
u32 audVehicleAudioEntity::sm_NumVehiclesInWaterSlapRadius[NumQuadrants];
f32 audVehicleAudioEntity::sm_WaterSlapRadiusScalar[NumQuadrants];

f32 audVehicleAudioEntity::sm_VehDeltaSpeedForCollision = 90.f;

// Can't have more real vehicles than submixes
u32 g_MaxVehicleSubmixes = EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1;
u32 g_MaxDummyVehicles = 8;
u32 g_MaxDumyVehiclesPerQuadrant = g_MaxDummyVehicles/4;
f32 g_MaxDummyRangeScale = 30.0f;
f32 g_MaxGranularRangeScale = 6.0f;
f32 g_MaxRealRangeScale = 3.0f;
u32 g_NPCRoadNoiseAttackTime = 1000;
u32 g_NPCRoadNoiseReleaseTime = 500;
f32 g_GranularActivationScaleIncreaseRate = 2.0f;
f32 g_GranularActivationScaleDecreaseRate = 2.0f;
f32 g_ActivationScaleIncreaseRate = 2.0f;
f32 g_ActivationScaleDecreaseRate = 2.0f;
f32 g_DummyActivationScaleIncreaseRate = 2.0f;
f32 g_DummyActivationScaleDecreaseRate = 2.0f;
f32 g_MinTimeAtDesiredLOD = 0.2f;
f32 g_MinTimeAtCurrentLOD = 0.5f;
f32 g_SqdDistThresholdForWaveImpact = 900.f;
f32 g_DistThresholdToTriggerVehSplashes = 10000.f; 
u32 g_MaxWaterSlapVehiclesPerQuadrant = 1;
f32 g_HydraulicsChassisStressSensitivityMultiplier = 2.0f;
u8 g_NumHydraulicActivationDelayFrames = 1;

f32 g_WheelDirtinessIncreaseSmoothRate = 0.0015f;
f32 g_WheelDirtinessDecreaseSmoothRate = 0.00075f;
f32 g_WheelGlassinessIncreaseSmoothRate = 1.0f;
f32 g_WheelGlassinessDecreaseSmoothRate = 0.001f;
f32 g_GlassAreaDepletionRate = 0.25f;
f32 g_ModdedRadioVehicleOpenness = 0.3f;
u32 g_HydraulicsModifiedTimeoutMs = 1250;
u32 g_ModShopDoorRecentlyUsedTime = 500;
u32 g_MinSubRudderRepeatTime = 250;

extern bool g_NoHeadlightSmashAudio;
extern bool g_BonnetCamStereoEffectEnabled;
extern u32 g_FakeWaveHitDistanceSq;
extern bool g_AllowSynthsOnNetworkVehicles;
extern bool g_PositionedPlayerVehicleRadioEnabled;

bool g_DebugDrawFocusVehicleRadioOcclusion = false;
bool g_DisablePlayerVehicleDSP = false;
bool g_DisableNPCVehicleDSP = false;
bool g_DisableBurnoutSkidsInReverse = false;
bool g_ForceAmphibiousBoatMode = false;
bool g_EnableStereoSirens = true;
f32 g_CopsAndCrooksForceSirenSmashDamage = 0.75f;
f32 g_CopsAndCrooksForceSirenSmashProbability = 0.2f;

extern bool g_ForceUpgradeRadio;
extern float g_SubmersibleUnderwaterCutoff;
extern AircraftWarningSettings* g_AircraftWarningSettings;

BANK_ONLY(char g_CRFilter[128]={0};)
BANK_ONLY(f32 g_WaveSlotStreamingDelayTime = 0.0f;)
BANK_ONLY(bool g_AuditionVehExplosions = false;);
f32 g_VehHealthThreshold = 100.f;
#if __BANK
PARAM(disablegranular, "Disable the granular engine system");
#endif

XPARAM(audiodesigner);

#if __BANK
extern bool g_ForceUpgradeEngine;
extern f32 g_ForcedEngineUpgradeLevel;
extern bool g_ForceUpgradeExhaust;
extern f32 g_ForcedExhaustUpgradeLevel;
extern bool g_ForceUpgradeEngineBay[3];
extern f32 g_ForcedEngineBayUpgradeLevel[3];
extern bool g_ForceUpgradeTransmission;
extern f32 g_ForcedTransmissionUpgradeLevel;
extern bool g_ForceUpgradeTurbo;
extern f32 g_ForcedTurboUpgradeLevel;
extern bool g_ForceUpgradeRadio;
extern f32 g_ForcedRadioUpgradeLevel;
bool g_ForceGlassySurface;
bool g_ForceDirtySurface;
bool g_ShowCarsPlayingRainLoops = false;
bool g_OverrideMissileStatus = false;
bool g_DebugMissileLockStatus = false;
bool g_UndersteerAffectsMainSkidVol = true;
bool g_ShowDoorRatios = false;
extern bool g_ForceNPCHydraulicSounds;
extern bool g_DebugDrawVehicleWater;
extern bool g_ForceDamage;
extern bool g_ForceRealLodOnFocusEntity;
extern f32 g_ForcedDamageFactor;
extern bool g_ForceSynthsOnAllEngines;
extern bool g_SimulateDrowning;
extern bool g_ShowEngineStates;
extern bool g_DrawSuperDummyRadius;
extern bool g_ShowFocusVehicle;
extern bool g_DrawLinesToRealCars;
extern bool g_DrawLinesToVehiclesWithWaveSlots;
extern bool g_DrawVehiclesInModShop;
extern bool g_DrawLinesToVehiclesWithSubmixVoices;
extern bool g_OverrideHydraulicSuspensionDamage;
extern f32 g_OverridenHydraulicSuspensionDamage;

s32 g_SimulatedMissileState = 0;
f32 g_SimulatedMissileDistance = 0.5f;
extern bool g_ForceShallowWater;
extern bool g_DisableClatter;
extern bool g_DisplayRidgedSurfaceInfo;
extern bool g_UseDebugPlaneTracker;
extern bool g_UseDebugHeliTracker;
extern bool g_DisableHorns;
extern CPlayerSwitchInterface g_PlayerSwitch;

bool g_SmashPlayerSiren = false;
bool g_FixPlayerSiren = false;

bool g_OverrideFocusVehicleOpenness = false;
f32 g_OverriddenVehicleOpenness = 0.f;

bool g_OverrideFocusVehicleLeakage;
AmbientRadioLeakage g_OverriddenVehicleRadioLeakage = LEAKAGE_BASSY_LOUD;

audioCarRecordingInfo audVehicleAudioEntity::sm_DebugCarRecording;

f32 audVehicleAudioEntity::sm_CRCurrentTime = 0.f;
f32 audVehicleAudioEntity::sm_CRProgress = 0.f;
f32 audVehicleAudioEntity::sm_CRJumpTimeInMs = 1000.f;
bool audVehicleAudioEntity::sm_UpdateCRTime = false;
bool audVehicleAudioEntity::sm_PauseGame = false;
bool audVehicleAudioEntity::sm_EnableCREditor = false;
bool audVehicleAudioEntity::sm_ManualTimeSet = false;
bool audVehicleAudioEntity::sm_DrawCREvents = false;
bool audVehicleAudioEntity::sm_DrawCRName = false;
bool audVehicleAudioEntity::sm_LoadExistingCR = false;
bool audVehicleAudioEntity::sm_VehCreated = false;
char audVehicleAudioEntity::sm_CRAudioSettingsCreator[128];
char audVehicleAudioEntity::sm_RecordingName[64];
u32 audVehicleAudioEntity::sm_RecordingIndex = 0;
rage::bkGroup*	audVehicleAudioEntity::sm_CRWidgetGroup(NULL);
#endif

#if NA_RADIO_ENABLED
bank_float g_RadioLPFCutoffs[NUM_AMBIENTRADIOLEAKAGE][2] = 
{
	{50.f, 5000.f}, // Bassy/Loud
	{300.f, 5000.f}, // Bassy/Medium
	{1000.f, 5000.f}, // Mids/Loud
	{1000.f, 5000.f}, // Mids/Medium
	{1000.f, 5000.f}, // Mids/Quiet
	{50.f, 250.f}, // CrazyLoud
	{550.f, 4000.f}, // Modded
	{550.f, 1200.f}, // PartyBus
};

float g_RadioLPFLinear[NUM_AMBIENTRADIOLEAKAGE][2];

bank_u32 g_RadioHPFCutoffs[NUM_AMBIENTRADIOLEAKAGE] = 
{
	0, // Bassy/Loud
	30, // Bassy/Medium
	500, // Mids/Loud
	500, // Mids/Medium
	1000, // Mids/Quiet
	0, // CrazyLoud
	0, // Modded
	0, // PartyBus
};

bank_float g_RadioOpennessVols[NUM_AMBIENTRADIOLEAKAGE][2] =
{
	{5.0f, 1.25f}, // Bassy/Loud
	{2.5f, 1.f}, // Bassy/Medium
	{2.82f, 1.41f}, // Mids/Loud
	{1.41f, 1.41f}, // Mids/Medium
	{1.27f, 1.41f}, // Mids/Quiet
	{7.0f, 8.5f}, // Crazy/Loud
	{5.0f, 4.0f}, // Modded
	{5.0f, 13.0f}, // PartyBus
};

bank_float g_RadioRolloffs[NUM_AMBIENTRADIOLEAKAGE][2] =
{
	{2.0f, 1.f}, // Bassy/Loud
	{1.5f, 1.f}, // Bassy/Medium
	{1.0f, 1.f}, // Mids/Loud
	{1.0f, 1.f}, // Mids/Medium
	{1.1f, 1.f}, // Mids/Quiet
	{5.f, 5.f}, // CrazyLoud
	{3.f, 3.f}, // Modded
	{3.f, 4.f}, // PartyBus
};

bank_float g_MaxRadiusForAmbientRadio2[NUM_AMBIENTRADIOLEAKAGE] =
{	
	(50.f * 50.f), // Bassy/Loud
	(40.f * 40.f), // Bassy/Medium
	(25.f * 25.f), // Mids/Loud
	(25.f * 25.f), // Mids/Medium
	(25.f * 25.f), // Mids/Quiet
	(65.f * 65.f), // CrazyLoud
	(65.f * 65.f), // Modded
	(350.f * 350.f), // PartyBus
};

#if __BANK
bool g_OverrideFocusVehicleLeakageParams = false;
bool g_CopyOverridenLeakageFromCurrentSettings = false;
f32 g_OverridenLeakageVolMin = 0.f;
f32 g_OverridenLeakageVolMax = 0.f;
f32 g_OverridenLeakageLpfLinearMin = 0.f;
f32 g_OverridenLeakageLpfLinearMax = 0.f;
f32 g_OverridenLeakageRolloffMin = 0.f;
f32 g_OverridenLeakageRolloffMax = 0.f;
u32 g_OverridenLeakageHPFCutoff = 0;
f32 g_OverridenLeakageMaxRadius = 0;
#endif

bank_float g_LoudVehicleRadioOffset = 4.f;
bank_float g_AmbientVehicleRadioOffset = 5.f;
#endif //NA_RADIO_ENABLED

const char * g_MissileLockOnStates[5] = { 
	"No Lock",
	"Acquired/Acquiring",
	"Missile Fired",
	"Acquiring Target",
	"Target Acquired",
};

extern s32 g_ForcePlayerVehicleLOD;

const eHierarchyId g_DoorHierarchyIds[] =
{
	VEH_INVALID_ID,// FRONT 
	VEH_INVALID_ID,// BACK
	VEH_DOOR_DSIDE_F,
	VEH_DOOR_PSIDE_F,
	VEH_DOOR_DSIDE_R,
	VEH_DOOR_PSIDE_R,
	VEH_INVALID_ID,
	VEH_INVALID_ID
};

const eHierarchyId audVehicleAudioEntity::sm_WindowHierarchyIds[NumWindowHierarchyIds] =
{
	VEH_WINDSCREEN,
	VEH_WINDSCREEN_R,
	VEH_WINDOW_LF,
	VEH_WINDOW_RF,
	VEH_WINDOW_LR,
	VEH_WINDOW_RR,
	VEH_WINDOW_LM,
	VEH_WINDOW_RM,
};

const f32 g_WindowOpenContribs[] = 
{
	0.5f,	// VEH_WINDSCREEN
	0.45f,	// VEH_WINDSCREEN_R
	0.3f,	// VEH_WINDOW_LF
	0.3f,	// VEH_WINDOW_RF
	0.2f,	// VEH_WINDOW_LR
	0.2f,	// VEH_WINDOW_RR
	0.2f,	// VEH_WINDOW_LM
	0.2f,	// VEH_WINDOW_RM
}; 

const f32 g_WindowOutSign[] =
{
	1.0f,   // VEH_WINDSCREEN
	-1.0f,  // VEH_WINDSCREEN_R
	-1.0f,	// VEH_WINDOW_LF
	1.0f,	// VEH_WINDOW_RF
	-1.0f,	// VEH_WINDOW_LR
	1.0f,	// VEH_WINDOW_RR
	-1.0f,	// VEH_WINDOW_LM
	1.0f,	// VEH_WINDOW_RM
};

const f32 g_DoorOutSign[] =
{
	1.0f,	// VEH_INVALID_ID
	1.0f,	// VEH_INVALID_ID
	-1.0f,	// VEH_DOOR_DSIDE_F
	1.0f,	// VEH_DOOR_PSIDE_F
	-1.0f,	// VEH_DOOR_DSIDE_R
	1.0f,	// VEH_DOOR_PSIDE_R
	1.0f,	//VEH_INVALID_ID
	1.0f	//VEH_INVALID_ID
};

bool g_RenderSlotManager = false;
bool g_ShowRadioGenres = false;
bool g_BreakOnVehicleUpdate = false;
bool g_DebugVehicleOpenness = false;
bool g_DebugVehicleSkids = false;
bool g_DebugDSPParams = false;
bool g_DebugHighSpeedEffect = false;
bool g_DisableEngineExhaustDSP = false;
s32 g_OverridenSkidPitchOffset = 0;

char g_DrivingMaterialName[64] = "";
audCategory *g_SkidCategory, *g_RoadNoiseCategory;
extern const u32 g_VariationVariableHash = ATSTRINGHASH("variation", 0x0cff74a31);
audCurve g_FreewayBumpSpeedToVol, g_FreewayBumpSpeedToPitch;
audCurve g_PlayerRoadNoiseFastVolCurve;
audCurve g_RoadNoiseWetRoadVolCurve;
audCurve g_VehicleMassToRumbleVol;
audCurve g_IceVanTempoScalingCurve;
bank_u32 g_TimeToPlaySiren = 350;
u32 g_StuntBoostIntensityTimeout = 3000;
u32 g_RechargeIntensityTimeout = 3000;
u32 g_IceVanTune = 0;
u32 g_IceVanTuneTextId = 1;
f32 g_ExhaustProximityEffectRange = 1.5f;
f32 g_PlayerHornOffset = 0.f;
f32 g_UpperVehDynamicImpactThreshold = -5.f;
f32 g_LowerVehDynamicImpactThreshold = -10.f;
u32 g_audVehicleLoopReleaseTime = 1500;
f32 g_NPCRoadNoiseInnerAngle = 0.0f;
f32 g_NPCRoadNoiseOuterAngle = 160.0f;
f32 g_SpeedForSpinDebris = 0.2f;
f32 g_ClatterMaxAcceleration = 80.0f;
f32 g_ChassisStressMaxAcceleration = 10.0f;
f32 g_SpinRatioForDebris = 3.5f;
u32 g_MaxRainVehiclesPerQuadrant = 1;
float g_BodyDamageFactorForDebris = 0.4f;
float g_BodyDamageFactorForHydraulicDebris = 0.15f;
bool g_GranularEnabled = true;
bool g_InteriorEngineExhaustAtListener = true;
bool g_InteriorEngineExhaustPanned = true;

f32 g_LateralSkidSmootherIncrease = 0.6f;
f32 g_LateralSkidSmootherDecrease = 6.f;
f32 g_MainSkidSmootherIncrease = 3.5f;
f32 g_MainSkidSmootherDecrease = 3.3f;

bool g_DebugSubmarineDiveHorn = false;
bool g_ForceTriggerSubmarineDiveHorn = false;
f32 g_MinSubDiveHoldTime = 0.25f;
f32 g_SubDiveHornVelocity = 1.f;
f32 g_SubDiveHornSubmergeThreshold = 0.5f;
f32 g_SubDiveHornSubmergeRetriggerThreshold = 0.3f;
f32 g_SubDiveHornSubmergeRetriggerThresholdNoDriver = 0.55f;

const f32 g_MaxDoorOpenAngle = 70.0f;
const u32 g_MaxDoors = NELEM(g_DoorHierarchyIds);
const f32 g_NumDoorsScalar = 1.f / (f32)g_MaxDoors;

CompileTimeAssert(NELEM(g_WindowOpenContribs) == audVehicleAudioEntity::NumWindowHierarchyIds);
CompileTimeAssert(NELEM(g_DoorOutSign) == g_MaxDoors);
CompileTimeAssert(NELEM(g_WindowOpenContribs) == audVehicleAudioEntity::NumWindowHierarchyIds);

extern audCategory* g_EngineDamageCategory;
extern CWeather g_weather;
extern phMaterialMgr::Id g_PedMaterialId;
extern bool g_DisplayGroundMaterial;
extern bool g_DisplayDirtGlassInfo;
extern bool g_ShowEngineExhaust;
extern bool g_AllowDummyVehicleRoadNoise;
extern f32 g_EngineVolumeTrim;
extern f32 g_ExhaustVolumeTrim;
extern f32 g_GearboxVolumeTrim;
extern f32 g_TurboVolumeTrim;
extern f32 g_StereoEffectEngineVolumeTrim;
extern f32 g_StereoEffectExhaustVolumeTrim;
extern Vector3 g_HydrantSprayPos;
extern bool g_IsHydrantSpraying;
extern u32 g_MaxGranularEngines;
extern u32 g_StuntBoostIntensityIncreaseDelay;
extern u32 g_RechargeIntensityIncreaseDelay;

BANK_ONLY(extern bool g_TreatAsPedCar);
BANK_ONLY(extern bool g_DisableRoadNoise);
BANK_ONLY(extern bool g_ShowNPCRoadNoiseState);
BANK_ONLY(extern bool g_DisableNPCRoadNoise);
BANK_ONLY(extern bool g_AutoReapplyDSPPresets);
BANK_ONLY(extern bool g_ShowVehiclesWithWaveSlots);
BANK_ONLY(extern bool g_EngineVolumeCaptureActive);

const u32 g_MainTarmacSkid = ATSTRINGHASH("VEHICLES_WHEEL_LOOPS_MAIN_TARMAC_SKID", 0x0ef01c293);
const u32 g_MainTarmacSkidBicycle = ATSTRINGHASH("BICYCLE_SKIDS", 0x00503a04a);
const u32 g_SideTarmacSkid = ATSTRINGHASH("VEHICLES_WHEEL_LOOPS_SIDE_TARMAC_SKID", 0x04346c124);
const u32 g_ScrapeTarmacSkid = ATSTRINGHASH("VEHICLES_WHEEL_LOOPS_TARMAC_SCRAPE", 0x01491e0de);
const u32 g_FastConcreteTyreRollHash = ATSTRINGHASH("VEHICLES_WHEEL_LOOPS_FAST_CONCRETE", 0x06ae284f4);

#if __WIN32
#pragma warning(push)
#pragma warning(disable: 4355) // 'this' used in base member initializer list
#endif

// ----------------------------------------------------------------
// audVehicleAudioEntity constructor
// ----------------------------------------------------------------
audVehicleAudioEntity::audVehicleAudioEntity() : m_CollisionAudio(this)
{
	m_VehicleLOD = AUD_VEHICLE_LOD_DISABLED;
	m_VehicleType = AUD_VEHICLE_NONE;
	m_EngineWaveSlot = NULL;
	m_LowLatencyWaveSlot = NULL;
	m_SFXWaveSlot = NULL;
	m_Vehicle = NULL;
	m_VehicleClatter = NULL;
	m_SpecialFlightModeTransformSound = NULL;
	m_LandingGearSound = NULL;
	m_JumpRecharge = NULL;
	m_ChassisStressLoop = NULL;
	m_EnvironmentGroup = NULL;
	m_HornSound = NULL;
	m_AlarmSound = NULL;
	m_MissileApproachingSound = NULL;
	m_MissileApproachingSoundImminent = NULL;
	m_AcquiringTargetSound = NULL;
	m_AllClearSound = NULL;
	m_BeingTargetedSound = NULL;
	m_DrowningRadioSound = NULL;
	m_WeaponChargeSound = NULL;
	m_SirenLoop = NULL;
	m_SirenBlip = NULL;
	m_ParachuteLoop = NULL;
	m_IdleHullSlapSound = NULL;
	m_ReverseWarning = NULL;
	m_DoorOpenWarning = NULL;
	m_RoadNoiseFast = NULL;
	m_RoadNoiseRidged = NULL;
	m_RoadNoiseRidgedPulse = NULL;
	m_ExhastProximityPulseSound = NULL;
	m_DirtyWheelLoop = NULL;
	m_GlassyWheelLoop = NULL;
	m_RoadNoiseWet = NULL;
	m_RoadNoiseHeavy = NULL;
	m_WheelDetailLoop = NULL;
	m_RoadNoiseNPC = NULL;
	m_RainLoop = NULL;
	m_CaterpillarTrackLoop = NULL;
	m_DetachedBonnetSound = NULL;
	m_WaterLappingSound = NULL;
	m_VehicleCabinTone = NULL;
	m_SubTurningSweetener = NULL;
	m_SubPropSound = NULL;
	m_SubExtrasSound = NULL;
	m_SubmersibleCreakSound = NULL;
	m_WaterDiveSound = NULL;
	m_MainSkidLoop = m_SideSkidLoop = m_UndersteerSkidLoop = m_WheelSpinLoop = m_WetWheelSpinLoop = NULL;
	m_SurfaceSettleSound = NULL;
	m_WetSkidLoop = NULL;
	m_DriveWheelSlipLoop = NULL;
	m_ClothSound = NULL;
	m_CinematicTireSweetener = NULL;
	m_StuntTunnelSound = NULL;
	m_WaterTurbulenceSound = NULL;
	m_RadioStation = NULL;
	m_LastRadioStation = NULL;
	m_ReapplyDSPEffects = false;

	m_FranklinSpecialPassby = m_FranklinSpecialTraction = NULL;
	m_DamageWarningSound = NULL;
	m_SkidPitchOffset = 0;

	m_CachedMaterialSettings = NULL;
	m_SubAngularVelocityMag = 0.0f;
	m_ForcedGameObject = 0u;
	m_EngineSubmixIndex = -1;
	m_ExhaustSubmixIndex = -1;
	m_PlayerHornOn = false;
	m_PlaySirenWithNoNetworkPlayerDriver = false;
	m_WasAlarmActive = false;
	m_WasScheduledCreation = false;
	m_OverrideHorn = false;
	m_OverridenHornHash = g_NullSoundHash;
	m_BulletHitCount = 0;
	m_TimeSinceLastBulletHit = 0;
	m_LastSurfaceChangeClatterTime = 0u;

	for(u32 i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		m_ShallowWaterSounds[i] = NULL;
	}	

	for (u32 i = 0; i < CVehicle::MAX_NUM_WEAPON_BLADES; i++)
	{
		m_BladeImpactSound[i] = NULL;
	}

	m_ShallowWaterEnterSound = NULL;
	m_LastDriftAngleDelta = m_LastDriftAngle = 0.f;
	m_LastDriftChirpTime = 0;	

	m_NetworkCachedRadioStationIndex = g_NullRadioStation;
}

#if __WIN32
#pragma warning(pop)
#endif

// ----------------------------------------------------------------
// audVehicleAudioEntity reset
// ----------------------------------------------------------------
void audVehicleAudioEntity::Reset()
{
	m_EngineWaterDepth = 0.0f;
	m_RidgedSurfaceDirectionMatch = 0.0f;
	m_LastHydraulicSuspensionStateChangeTime = 0u;	
	m_LastCaterpillarTrackValidTime = 0u;
	m_LastHydraulicEngageDisengageTime = 0u;
	m_FakeAudioHealth = 1000.0f;
	m_FwdSpeedLastFrame = 0.0f;
	m_PrevDirtinessVolume = 0.0f;
	m_PrevGlassinessVolume = 0.0f;
	m_MissileApproachTimer = 0.0f;
	m_InAirRotationalForceX = 0.0f;
	m_CachedNonCameraRelativeOpenness = 0.0f;
	m_CachedCameraRelativeOpenness = 0.0f;
	m_InAirRotationalForceXLastFrame = 0.0f;
	m_InAirRotationalForceY = 0.0f;
	m_InAirRotationalForceYLastFrame = 0.0f;
	m_SteeringAngleLastFrame = 0.0f;
	m_OutOfWaterTimer = 0.f;
	m_WheelRetractRate = 0.f;
	m_InWaterTimer = 0.f;
	m_LastSubCarRudderTurnSoundTime = 0u;
	m_SubmarineTransformSoundHash = 0u;
	m_ShouldStopSubTransformSound = false;
	m_IsLandingGearRetracting = false;
	m_ShouldPlayLandingGearSound = false;
	m_ShouldStopLandingGearSound = false;
	m_WasSuspensionDropped = false;
	m_WasReducedSuspensionForceSet = false;
	m_SpecialFlightModeWingsDeployed = false;
	m_SuppressHorn = false;
	m_RequiresSFXBank = false;
	m_IsWaitingOnActivatedJumpLand = false;
	m_HasTriggeredSubmarineDiveHorn = false;
	m_HasTriggeredSubmarineSurface = true;
	m_TriggerDeferredSubmarineDiveHorn = false;
	m_TriggerDeferredSubmarineSurface = false;
	m_SubmarineDiveHoldTime = 0.f;
	m_TriggerFastSubCarTransformSound = false;
	m_ForceReverseWarning = false;
	m_ResetDSPEffects = false;
	m_ReapplyDSPPresets = false;
	m_IsScriptTriggeredHorn = false;
	m_IsBeingNetworkSpectated = false;
	m_IsVehicleBeingStopped = false;
	m_WasInWaterLastFrame = true;
	m_IsPlayerPedInFullyOpenSeat = false;
    m_ShouldPlayBonnetDetachSound = false;
	m_ShouldTriggerHydraulicBounce = false;
	m_ShouldTriggerHydraulicActivate = false;
	m_ShouldTriggerHydraulicDeactivate = false;
	m_IsPersonalVehicle = false;
	m_HydraulicActivationDelayFrames = 0u;
	m_ScriptPriority = AUD_VEHICLE_SCRIPT_PRIORITY_NONE;
	m_ScriptPriortyVehicleThreadID = THREAD_INVALID;
	m_DummyLODValid = false;
	m_VehicleModelSoundHash = m_VehicleMakeSoundHash = m_VehicleCategorySoundHash = m_ScannerVehicleSettingsHash = 0;
	m_HasMissileLockWarningSystem = false;
	m_GpsType = GPS_TYPE_NONE;
	m_GpsVoice = GPS_VOICE_FEMALE;
	m_RadioType = RADIO_TYPE_NONE;
	m_RadioLeakage = LEAKAGE_MIDS_MEDIUM;
	m_RadioOpennessVol  = 0.f;
#if NA_RADIO_ENABLED
	m_RadioLPFCutoff = (u32)g_RadioLPFCutoffs[m_RadioLeakage][0];
	m_RadioHPFCutoff = g_RadioHPFCutoffs[m_RadioLeakage];
	m_RadioRolloffFactor = g_RadioRolloffs[m_RadioLeakage][0];
	m_AmbientRadioDisabled = false;
	m_RadioDisabled = false;
	m_HasLoudRadio = false;
	m_ScriptForcedRadio = false;
	m_ScriptRequestedRadioStation = false;
#endif

	m_VehicleLOD = AUD_VEHICLE_LOD_DISABLED;
	m_PrevDesiredLOD = AUD_VEHICLE_LOD_DISABLED;
	m_TimeAtDesiredLOD = 10.0f;
	m_NextClatterTime = 0;
	m_ScriptEngineDamageFactor = 0.0f;
	m_ScriptBodyDamageFactor = 0.0f;
	m_SpecialFlightModeReplayAngularVelocity = 0.0f;
	m_DistanceFromListener2LastFrame = 0u;
	m_DistanceFromListener2 = 0u;
	m_ForcedGameObjectResetRequired = false;
	m_PreserveEnvironmentGroupOnShutdown = false;
	REPLAY_ONLY(m_ReplayRevs = 0.0f;)
	m_RadioStation = NULL;
	m_IsRadioOff = false;
	m_PlayedDiveSound = false;
	m_TimeAtCurrentLOD = 10.0f;
	m_TimeStationary = 0.0f;
	m_LastRadioStation = NULL;
	m_CachedMaterialSettings = NULL;
	m_BurnoutPitch = 0.0f;
	m_LastRevLimiterTime = 0;
	m_LastScriptDoorModifyTime = 0;
	m_LastHitByRocketTime = 0;
	m_LastWaveHitTime = 0;
	m_LastBigWaveHitTime = 0;
	m_LastWaterLeaveTime = 0;
	m_WeaponChargeBone = -1;
	m_WeaponChargeLevel = 0.f;
	m_PrevFastRoadNoiseHash = g_NullSoundHash;
	m_PrevDetailRoadNoiseHash = g_NullSoundHash;
	m_SpecialFlightModeTransformSoundToTrigger = 0u;
	m_HasScannerDescribedModel = false;
	m_HasScannerDescribedManufacturer = false;
	m_HasScannerDescribedColour = false;
	m_HasScannerDescribedCategory = false;
	m_IsFastRoadNoiseDisabled = false;
	m_InCarModShop = false;
	m_WasInCarModShop = false;
	m_HornType = 0;
	m_HornHeldDownTime = 0;
	m_LastSirenChangeTime = 0;
	m_SirenState = AUD_SIREN_OFF;
	m_IsSirenFucked = false;
	m_IsSirenForcedOn = false;
	m_SirenBypassMPDriverCheck = false;
	m_UseSirenForHorn = true;
	m_IsIndicatorRequestedOn = false;
	m_ParkingBeepEnabled = false;
	m_KeepSirenOn = false;
	m_IsReversing = false;
	m_HasIndicatorRequest = false;
	m_LastSplashTime = 0;
	m_LastHeliRotorSplashTime = 0;
	m_LastTimeOnGround = 0;
	m_LastTimeTwoWheeling = 0;
	m_LastTimeInAir = 0;
	m_LastTimeExploded = 0;
	m_LastTimeExplodedVehicle = 0;
	m_HasSmashedWindow = false;
	m_IsHornPermanentlyOn = false;
	m_WasHornPermanentlyOn = false;
	m_IsHornEnabled = true;
	m_HeadlightsSwitched = false;
	m_WasMissileApproaching = false;
	m_WasBeingTargeted = false;
	m_WasAcquiringTarget = false;
	m_HadAcquiredTarget = false;
	m_SirenControlledByConductor = true;
	m_MustPlaySiren = true;
	m_LastPlayerVehicle = false; 
	m_ShouldPlayPlayerVehSiren = true;
	m_WantsToPlaySiren = false;
	m_IsSirenOn = false;
	m_IsWaitingToInit = true;
	m_IsPlayerSeatedInVehicle = false;
	m_IsNetworkPlayerSeatedInVehicle = false;
	m_PlayerHornOn = false;
	m_IsInAirCached = false;
	m_IsPlayerVehicle = false;
	m_IsFocusVehicle = false;
	m_IsPlayerDrivingVehicle = false;
	m_OverrideHorn = false;
	m_OverridenHornHash = g_NullSoundHash;
	m_WasWrecked = false;
	m_DrowningFactor = 0.f;
	m_IgnitionHoldTime = 1.0f;
	m_TimeToPlaySiren = 0;
	m_LastCREventProcessed = 0;
	m_PrevAverageDriveWheelSpeed = 1.0f;
	m_TyreChirpAccelPlayed = false;
	m_TyreChirpBrakePlayed = false;
	m_OnStairs = false;
	m_WasOnStairs = false;
	m_ReapplyDSPEffects = false;

	m_DriftFactorSmoother.Init(1.0f, 0.05f, true);
	m_WheelDirtinessSmoother.Init(0.001f, 0.001f, true);
	m_WheelGlassinessSmoother.Init(0.001f, 0.001f, true);
	m_ChassisStressSmoother.Init(10.f, 2.41f/1000.f, true);
    m_HydraulicSuspensionInputSmoother.Init(0.1f, 0.1f, true);
	m_HydraulicSuspensionAngleSmoother.Init(0.0075f, 0.1f, true);
	m_DrowningFactorSmoother.Init(GetDrowningSpeed()/1000.0f, 0.2f/1000.0f, true);
	m_SirensVolumeSmoother.Init(0.001f,true);
	m_RainLoopVolSmoother.Init(0.1f,true);
	m_NPCRoadNoiseSmoother.Init(0.01f);
	m_WheelSpinSmoother.Init(1.0f, 0.001f);
	m_VisibilitySmoother.Init(0.001f, 0.0001f);
	m_SubAngularVelocitySmoother.Init(0.01f);
	m_ShouldStopSiren = false;
	m_SirensDesireLinVol = 1.f;
	m_NearestNodeAddress.SetEmpty();

	m_EngineEffectWetSmoother.Init(0.0006f);
	m_EngineEffectWetSmoother.CalculateValue(0.0f, fwTimer::GetTimeInMilliseconds());

	for(u32 i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		m_LastMaterialIds[i] = ~0U;
		m_LastTyreBumpTimes[i] = 0;
		m_LastSlipAngle[i] = 0.f;
	}

	for (u32 i = 0; i < CVehicle::MAX_NUM_WEAPON_BLADES; i++)
	{
		m_IsWeaponBladeColliding[i] = false;
		m_LastWeaponBladeCollisionTime[i] = 0u;
		m_LastWeaponBladeCollisionIsPed[i] = false;
	}

	m_MainSkidSmoother.Init(g_MainSkidSmootherIncrease * 0.001f,g_MainSkidSmootherDecrease * 0.001f,0.0f,2.5f);
	m_LateralSkidSmoother.Init(g_LateralSkidSmootherIncrease * 0.001f,g_LateralSkidSmootherDecrease * 0.001f,0.f, 1.f);
	m_BurnoutSmoother.Init(0.001f, true);
	m_SurfaceSettleSmoother.Init(0.1f);

	m_RoadNoiseLeftCone.Init(-Vec3V(V_X_AXIS_WZERO), 0.5f, g_NPCRoadNoiseInnerAngle, g_NPCRoadNoiseOuterAngle);
	m_RoadNoiseRightCone.Init(Vec3V(V_X_AXIS_WZERO), 0.5f, g_NPCRoadNoiseInnerAngle, g_NPCRoadNoiseOuterAngle);

	m_CachedVehicleVelocity.Zero();
	m_CachedVehicleVelocityLastFrame.Zero();
	m_CachedAngularVelocity.Zero();
    m_PrevHydraulicsInputAxis.Zero();
    m_HydraulicsInputAxis.Zero();
	m_PrevHydraulicsSuspensionAngle.Zero();
	m_HydraulicsSuspensionAngle.Zero();

	for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
	{
		m_VehicleGadgets[loop]->Reset();
	}

	m_BulletHitCount = 0;
	m_TimeSinceLastBulletHit = 0;

	m_PlayerVehicleMixGroupIndex = -1;

	m_PlayerVehicleAlarmBehaviour = false;

	m_NetworkCachedRadioStationIndex = g_NullRadioStation;

	for(s32 i = 0; i < NumWindowHierarchyIds; i++)
	{
		m_HasWindowToSmashCache.Set(i);
	}

	if(m_RainLoop)
	{
		m_RainLoop->StopAndForget();
	}
}

#if __DEV
void audVehicleAudioEntity::PoolFullCallback(void* pItem)
{
	static const char *pPopTypeNames[] =
	{
		"POPTYPE_UNKNOWN",
		"POPTYPE_RANDOM_PERMANENT",	
		"POPTYPE_RANDOM_PARKED",		
		"POPTYPE_RANDOM_PATROL",		
		"POPTYPE_RANDOM_SCENARIO",	
		"POPTYPE_RANDOM_AMBIENT",		
		"POPTYPE_PERMANENT",			
		"POPTYPE_MISSION",			
		"POPTYPE_REPLAY",				
		"POPTYPE_CACHE",				
		"POPTYPE_TOOL"
	};

	static const char *pVehicleTypeNames[] =
	{
		"AUD_VEHICLE_TRAIN",
		"AUD_VEHICLE_PLANE",
		"AUD_VEHICLE_HELI",
		"AUD_VEHICLE_CAR",
		"AUD_VEHICLE_BOAT",
		"AUD_VEHICLE_BICYCLE",
		"AUD_VEHICLE_TRAILER",
		"AUD_VEHICLE_ANY", 
	};

	if (!pItem)
	{
		Printf("ERROR - audVehicleAudioEntity Pool FULL!\n");
	}
	else
	{
		audVehicleAudioEntity* vehicleAudioEntity =  static_cast<audVehicleAudioEntity*>(pItem);
		const char *pName = vehicleAudioEntity->GetVehicleModelName();
		float fTimeSinceCreation = (fwTimer::GetTimeInMilliseconds() - vehicleAudioEntity->GetVehicle()->m_TimeOfCreation) * 0.001f;

		Displayf("	\"%s\", memory address %p, type %s, pop type %s, time since creation %3.2f",
			pName, vehicleAudioEntity, pVehicleTypeNames[vehicleAudioEntity->GetAudioVehicleType()], pPopTypeNames[vehicleAudioEntity->GetVehicle()->m_nDEflags.nPopType], fTimeSinceCreation);
	}
}
#endif

// ----------------------------------------------------------------
// audVehicleAudioEntity FixSiren
// ----------------------------------------------------------------
void audVehicleAudioEntity::FixSiren()
{
	m_IsSirenFucked = false; 
	if(m_SirenLoop)
	{
		m_SirenLoop->StopAndForget();
	}
}
// ----------------------------------------------------------------
// audVehicleAudioEntity destructor
// ----------------------------------------------------------------
audVehicleAudioEntity::~audVehicleAudioEntity()
{
	for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
	{
		delete m_VehicleGadgets[loop];
	}

	m_VehicleGadgets.clear();
	ShutdownPrivate();
}

// ----------------------------------------------------------------
// audVehicleAudioEntity Shutdown
// ----------------------------------------------------------------
void audVehicleAudioEntity::Shutdown()
{
	ShutdownPrivate();
}

// ----------------------------------------------------------------
// audVehicleAudioEntity ShutdownPrivate
// ----------------------------------------------------------------
void audVehicleAudioEntity::ShutdownPrivate()
{
	// If this is a priority vehicle, make sure we unregister it from the script that originally modified the status
	if(m_ScriptPriority > AUD_VEHICLE_SCRIPT_PRIORITY_NONE)
	{
		g_ScriptAudioEntity.SetVehicleScriptPriority(this, AUD_VEHICLE_SCRIPT_PRIORITY_NONE, m_ScriptPriortyVehicleThreadID);
	}

	g_ReflectionsAudioEntity.DetachFromVehicle(this);

	for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
	{
		m_VehicleGadgets[loop]->StopAllSounds();
	}

	if(m_Vehicle)
	{
		DYNAMICMIXER.RemoveEntityFromMixGroup(m_Vehicle);
	}	

	if(!m_PreserveEnvironmentGroupOnShutdown)
	{
		RemoveEnvironmentGroup(naAudioEntity::SHUTDOWN);
	}

#if NA_RADIO_ENABLED
	if(m_Vehicle != NULL)
	{
		g_RadioAudioEntity.StopVehicleRadio(m_Vehicle);
	}
	m_RadioStation = NULL;
#endif

	if(GetShockwaveID() != kInvalidShockwaveId)
	{
		audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(GetShockwaveID());
		SetShockwaveID(kInvalidShockwaveId);
	}

	if(camInterface::IsFadedOut())
	{
		StopAllSounds(false);
	}

	NA_POLICESCANNER_ENABLED_ONLY(g_AudioScannerManager.CancelAnySentencesFromEntity(this));
	SetLOD(AUD_VEHICLE_LOD_DISABLED);
	m_VehicleFireAudio.StopAllSounds();
	naAudioEntity::Shutdown();
}

// ----------------------------------------------------------------
// Initialise a gadgets associated with this vehicle
// ----------------------------------------------------------------
audVehicleGadget* audVehicleAudioEntity::CreateVehicleGadget(audVehicleGadget::audVehicleGadgetType type)
{	
	for(u32 i = 0; i < m_VehicleGadgets.GetCount(); i++)
	{
		if(m_VehicleGadgets[i]->GetType() == type)
		{
			return m_VehicleGadgets[i];
		}
	}

	audVehicleGadget* newGadget = NULL;

	switch(type)
	{
	case audVehicleGadget::AUD_VEHICLE_GADGET_FORKS:
		newGadget = rage_new audVehicleForks;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_ROOF:
		newGadget = rage_new audVehicleConvertibleRoof;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_TURRET:
		newGadget = rage_new audVehicleTurret;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER:
		newGadget = rage_new audVehicleDigger;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_TOWTRUCK_ARM:
		newGadget = rage_new audVehicleTowTruckArm;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_HANDLER_FRAME:
		newGadget = rage_new audVehicleHandlerFrame;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_GRAPPLING_HOOK:
		newGadget = rage_new audVehicleGrapplingHook;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_MAGNET:
		newGadget = rage_new audVehicleGadgetMagnet;
		break;
	case audVehicleGadget::AUD_VEHICLE_GADGET_DYNAMIC_SPOILER:
		newGadget = rage_new audVehicleGadgetDynamicSpoiler;
		break;
	default:
		audAssertf(false, "Unsupported vehicle gadget type!");
		break;
	}

	if(newGadget)
	{
		newGadget->Init(this);
		m_VehicleGadgets.PushAndGrow(newGadget);
	}

	return newGadget;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity update rain
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateRain(audVehicleVariables& params)
{
	f32 actualRainVol = -100.f;
	if(g_weather.IsRaining() && 
	   !m_Vehicle->IsBikeOrQuad() && 
	   !m_Vehicle->InheritsFromBicycle() && 
	   !m_Vehicle->m_nFlags.bInMloRoom && 
	   m_EnvironmentGroup && 
	   !m_EnvironmentGroup->IsUnderCover()
	   && m_Vehicle->GetIsVisible())
	{
		actualRainVol = g_WeatherAudioEntity.GetRainVolume();
	}

	f32 hydrantRainVol = -100.f;
	if(g_IsHydrantSpraying)
	{
		const f32 dist2 = (g_HydrantSprayPos - VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition())).Mag2();
		if(dist2 < (12.f*12.f))
		{
			const f32 dist = Sqrtf(dist2);
			const f32 linearVol = 1.f - (dist * (1.f/12.f));
			bank_float LinearVolMult = 3.f;
			hydrantRainVol = audDriverUtil::ComputeDbVolumeFromLinear(linearVol * LinearVolMult);
		}
	}

	if(CScriptHud::bUsingMissionCreator && !m_Vehicle->IsCollisionEnabled())
	{
		actualRainVol = -100.0f;
		hydrantRainVol = -100.0f;
	}

	f32 rainVol = Max(hydrantRainVol, actualRainVol);
	rainVol = audDriverUtil::ComputeDbVolumeFromLinear(m_RainLoopVolSmoother.CalculateValue(Min(m_VehRainInWaterVol, audDriverUtil::ComputeLinearVolumeFromDb(rainVol)),audNorthAudioEngine::GetTimeStepInMs()));
	f32 distanceFromListenerSq = 0.0f;

	if(rainVol > -100.f)
	{
		Vector3 vehPos = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
		Vector3 listenerPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		Vector3 dirToVeh = vehPos - listenerPos; 
		distanceFromListenerSq = dirToVeh.Mag2();
		u32 vehicleQuadrant = GetVehicleQuadrant();

		if(m_RainLoop && m_WasInInteriorViewLastFrame != params.isInFirstPersonCam)
		{
			m_RainLoop->StopAndForget();
		}

		if(m_IsPlayerVehicle || distanceFromListenerSq < (g_SuperDummyRadiusSq * sm_RainRadiusScalar[vehicleQuadrant]) * 0.5f)
		{
			if(!m_IsPlayerVehicle)
			{
				sm_NumVehiclesInRainRadius[vehicleQuadrant]++;
			}

			if(!m_RainLoop)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.StartOffset = audEngineUtil::GetRandomNumberInRange(0,99);
				initParams.IsStartOffsetPercentage = true;
				u32 vehicleRainSound = GetVehicleRainSound(params.isInFirstPersonCam);

				if(vehicleRainSound == g_NullSoundHash)
				{
					audMetadataRef rainSoundRef = params.isInFirstPersonCam? sm_ExtrasSoundSet.Find(ATSTRINGHASH("Vehicle_RainSoundInterior", 0x2C1D8B68)) : sm_ExtrasSoundSet.Find(ATSTRINGHASH("Vehicle_RainSound", 0x69294653));

					// No interior specific sound? Fallback to exterior
					if(params.isInFirstPersonCam && rainSoundRef == g_NullSoundRef)
					{
						rainSoundRef = sm_ExtrasSoundSet.Find(ATSTRINGHASH("Vehicle_RainSound", 0x69294653));
					}
					
					CreateAndPlaySound_Persistent(rainSoundRef, &m_RainLoop, &initParams);
				}
				else
				{
					CreateAndPlaySound_Persistent(vehicleRainSound, &m_RainLoop, &initParams);
				}
				
			}
		}
		else if(m_RainLoop && m_RainLoop->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0)) > 500)
		{
			m_RainLoop->StopAndForget();
		}

		if(m_RainLoop)
		{
			m_RainLoop->SetRequestedVolume(rainVol);
		}
	}
	else if(m_RainLoop)
	{
		m_RainLoop->StopAndForget();
	}

#if __BANK
	if(g_ShowCarsPlayingRainLoops)
	{
		if(m_RainLoop)
		{
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 2.f, Color32(0,0,255,128));	
		}

		if(rainVol > -100.0f && distanceFromListenerSq < g_SuperDummyRadiusSq)
		{
			char buf[32];
			formatf(buf, "Q%d", (u32)GetVehicleQuadrant());
			grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), Color32(255,255,255), buf);
		}
	}
#endif
}

// ----------------------------------------------------------------
// audVehicleAudioEntity init
// ----------------------------------------------------------------
void audVehicleAudioEntity::StopAllRoofLoops()
{
	for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
	{
		if(m_VehicleGadgets[loop]->GetType() == audVehicleGadget::AUD_VEHICLE_GADGET_ROOF)
		{
			m_VehicleGadgets[loop]->StopAllSounds();
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity init
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::CalculateNumVehiclesUsingSubmixes(audVehicleType type, bool engineSubmix, bool granularOnly)
{
	u32 count = 0;

	if(engineSubmix)
	{
		for(u32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1; loop++)
		{
			if(sm_VehicleEngineSubmixes[loop].vehicleType == type || (sm_VehicleEngineSubmixes[loop].vehicleType != AUD_VEHICLE_NONE && type == AUD_VEHICLE_ANY))
			{
				if(!granularOnly || sm_VehicleEngineSubmixes[loop].isGranular)
				{
					count++;
				}
			}
		}
	}
	else
	{
		for(u32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_EXHAUST_MAX - EFFECT_ROUTE_VEHICLE_EXHAUST_MIN + 1; loop++)
		{
			if(sm_VehicleExhaustSubmixes[loop].vehicleType == type || (sm_VehicleExhaustSubmixes[loop].vehicleType != AUD_VEHICLE_NONE && type == AUD_VEHICLE_ANY))
			{
				if(!granularOnly || sm_VehicleExhaustSubmixes[loop].isGranular)
				{
					count++;
				}
			}
		}
	}

	return count;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity init
// ----------------------------------------------------------------
void audVehicleAudioEntity::Init(CVehicle *vehicle)
{
	Reset();
	naAudioEntity::Init();
	audEntity::SetName("VehicleAudioEntity");
	m_Vehicle = vehicle;
	m_WasEngineOnLastFrame = m_Vehicle->m_nVehicleFlags.bEngineOn;
	m_LastVehicleDriver = m_Vehicle->GetDriver();
	m_VehicleFireAudio.Init(this);

#if NA_RADIO_ENABLED
	m_RadioEmitter.SetEmitter(vehicle);
#endif
	m_FreewayBumpPitch = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(-400,400));
	m_EnginePitchOffset = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(-250,100));

	// 60% chance of normal horn
	if(ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), 0.6f))
	{
		m_AlarmType = AUD_CAR_ALARM_TYPE_HORN;
	}
	else
	{
		m_AlarmType = (audCarAlarmType)(1 + (m_Vehicle->GetRandomSeed() % AUD_NUM_CAR_ALARM_TYPES-2));
	}

	m_CRSettings = NULL;
	for (u32 i = 0; i < CarRecordingAudioSettings::MAX_PERSISTENTMIXERSCENES; i ++)
	{
		m_CarRecordingPersistentScenes[i] = NULL;
	}
	m_TimeCreated = fwTimer::GetTimeInMilliseconds();
	m_LastTimeAlarmPlayed  = 0;
	m_RandomAlarmInterval = 0;
	m_LastClatterUpVel = 0.f;
	m_LastClatterAcceleration = 0.f;
	m_HasToTriggerAlarm = false;
	m_WasInInteriorViewLastFrame = false;
	m_OverrideHorn = false;
	m_OverridenHornHash = g_NullSoundHash;
	m_JustCameOutOfWater= false;

#if GTA_REPLAY
	m_ReplayIsInWater = false;
	m_ReplayVehiclePhysicsInWater = false;
#endif

	m_HornSoundIndex = -1;
	m_FakeEngineHealth = -1;
	m_FakeBodyHealth = 0;
	for(u32 i = 0 ; i < AUD_MAX_WHEEL_WATER_SOUNDS; i ++)
	{
		m_WaveImpactSounds[i] = NULL;
	}
	m_WasOnFreeWayLastFrame = false;

	m_VehRainInWaterVol = 1.f;
	m_VehicleCaterpillarTrackSpeed = 0.f;
	m_LastTimeTerrainProbeHit = 0;
	m_LastTimeTerrainProbeNotHit = 0;
	m_LastTimeGroundProbeNotHit = 0;
	m_TimeOfLastAircraftOverspeedWarning = 0;
	m_LastTimeAircraftDamageReported = 0;
	m_TimeOfLastAircraftLowFuelWarning = 0;
	m_TimeOfLastAircraftLowDamageWarning = 0;
	m_TimeOfLastAircraftHighDamageWarning = 0;
	m_TimeOfLastAircraftEngineDamageWarning = 0;
	m_TimeEnteredFreeway = 0;
	m_TimeExitedFreeway = 0;
	m_LastStuntRaceSpeedBoostTime = 0;
	m_LastStuntRaceSpeedBoostIntensityIncreaseTime = 0;
	m_LastRechargeIntensityIncreaseTime = 0;
	m_SpeedBoostIntensity = 0;
	m_RechargeIntensity = 0;

	m_HasPlayedEngineOnFire1 = false;
	m_HasPlayedEngineOnFire2 = false;
	m_HasPlayedEngineOnFire3 = false;
	m_HasPlayedEngineOnFire4 = false;
	m_TriggeredDispatchEnteredFreeway = true;
	m_TriggeredDispatchExitedFreeway = true;
	REPLAY_ONLY(m_ReplayRevLimiter = false;)

	m_VehicleUnderCover = false;

	if(!GetEnvironmentGroup())
	{
		InitOcclusion();
	}	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity InitClass
// ----------------------------------------------------------------
void audVehicleAudioEntity::InitClass()
{
	CompileTimeAssert(kMaxAudioWeaponBlades == CVehicle::MAX_NUM_WEAPON_BLADES);

	char slotName[32];
	for(u32 i = 0; i < g_audMaxEngineSlots; i++)
	{
		formatf(slotName, sizeof(slotName), "STREAM_ENGINE_%d", i+1);

		if(!sm_StandardWaveSlotManager.AddWaveSlot(slotName, "ENGINE"))
		{
			break;
		}
	}

	for(u32 i = 0; i < g_audMaxEngineSlots; i++)
	{
		formatf(slotName, sizeof(slotName), "STREAM_ENGINE_GRANULAR_HI_%d", i+1);

		if(!sm_HighQualityWaveSlotManager.AddWaveSlot(slotName, "ENGINE"))
		{
			break;
		}
	}

	for(u32 i = 0; i < g_audMaxEngineSlots; i++)
	{
		formatf(slotName, sizeof(slotName), "STREAM_ENGINE_LOW_LATENCY_%d", i+1);

		if(!sm_LowLatencyWaveSlotManager.AddWaveSlot(slotName, "ENGINE"))
		{
			break;
		}
	}

	for(u32 i = 0; i < NumQuadrants; i++)
	{
		sm_NumVehiclesInDummyRange[i] = 0;
		sm_DummyRangeScale[i] = 2.0f;
		sm_NumVehiclesInRainRadius[i] = 0;
		sm_RainRadiusScalar[i] = 1.0f;
		sm_NumVehiclesInWaterSlapRadius[i] = 0;
		sm_WaterSlapRadiusScalar[i] = 1.0f;
	}

	for(u32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1; loop++)
	{
		sm_VehicleEngineSubmixes[loop].submixSound = NULL;
		sm_VehicleEngineSubmixes[loop].vehicleType = AUD_VEHICLE_NONE;
		sm_VehicleEngineSubmixes[loop].isGranular = false;
	}

	for(u32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_EXHAUST_MAX - EFFECT_ROUTE_VEHICLE_EXHAUST_MIN + 1; loop++)
	{
		sm_VehicleExhaustSubmixes[loop].submixSound = NULL;
		sm_VehicleExhaustSubmixes[loop].vehicleType = AUD_VEHICLE_NONE;
		sm_VehicleExhaustSubmixes[loop].isGranular = false;
	}

#if __BANK
	if(PARAM_disablegranular.Get())
	{
		g_GranularEnabled = false;
	}
	sm_DebugCarRecording.nameHash =  atHashString();
	sm_DebugCarRecording.eventList.Reset();
	sm_DebugCarRecording.settings = NULL;
	sm_DebugCarRecording.modelId = fwModelId::MI_INVALID;

	audDisplayf("Size of naAudioEntity: %" SIZETFMT "d", sizeof(naAudioEntity));
	audDisplayf("Size of audVehicleAudioEntity: %" SIZETFMT "d", sizeof(audVehicleAudioEntity));
	audDisplayf("Size of audCarAudioEntity: %" SIZETFMT "d", sizeof(audCarAudioEntity));
	audDisplayf("Size of audHeliAudioEntity: %" SIZETFMT "d", sizeof(audHeliAudioEntity));
	audDisplayf("Size of audTrainAudioEntity: %" SIZETFMT "d", sizeof(audTrainAudioEntity));
	audDisplayf("Size of audBoatAudioEntity: %" SIZETFMT "d", sizeof(audBoatAudioEntity));
	audDisplayf("Size of audPlaneAudioEntity: %" SIZETFMT "d", sizeof(audPlaneAudioEntity));
	audDisplayf("Size of audTrailerAudioEntity: %" SIZETFMT "d", sizeof(audTrailerAudioEntity));
	audDisplayf("Size of audVehicleEngine: %" SIZETFMT "d", sizeof(audVehicleEngine));
#endif

	g_SkidCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("VEHICLES_WHEELS_SKIDS", 0x3C43E428));
	g_RoadNoiseCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("VEHICLES_WHEELS_ROAD_NOISE", 0x2EED1D0A));

	StaticConditionalWarning(g_PlayerRoadNoiseFastVolCurve.Init(ATSTRINGHASH("ROAD_NOISE_FAST_VOL_PLAYER", 0xB3F915D9)), "Invalid road noise player curve");
	StaticConditionalWarning(g_FreewayBumpSpeedToVol.Init(ATSTRINGHASH("FREEWAY_BUMPS_SPEED_TO_VOL", 0x75D3133D)), "Invalid freeway bump speed to vol curve");
	StaticConditionalWarning(g_FreewayBumpSpeedToPitch.Init(ATSTRINGHASH("FREEWAY_BUMPS_SPEED_TO_PITCH", 0x2E6AE968)), "Invalid freeway bump speed to pitch curve");
	StaticConditionalWarning(g_IceVanTempoScalingCurve.Init(ATSTRINGHASH("ICEVAN_TEMPO_SCALING", 0x5167AC58)), "Invalid ICEVAN_TEMPO_SCALING");
	StaticConditionalWarning(sm_ExtrasSoundSet.Init(ATSTRINGHASH("VehicleExtras", 0xF68247A0)), "Failed to find VehicleExtras sound set");
	StaticConditionalWarning(sm_LightsSoundSet.Init(ATSTRINGHASH("VehicleLights", 0x9A9F9DD0)), "Failed to find VehicleLights sound set");	
	StaticConditionalWarning(sm_MissileLockSoundSet .Init(ATSTRINGHASH("CNC_PLANE_DROP_SOUNDSET", 0xCE1684EE)), "Failed to find CNC_PLANE_DROP_SOUNDSET sound set");
	StaticConditionalWarning(sm_InteriorOpenToCutoffCurve.Init(ATSTRINGHASH("VEHICLE_INTERIOR_OPEN_TO_CUTOFF", 0xC162E536)), "Failed to initialise VEHICLE_INTERIOR_OPEN_TO_CUTOFF");
	StaticConditionalWarning(sm_InteriorOpenToVolumeCurve.Init(ATSTRINGHASH("VEHICLE_INTERIOR_OPEN_TO_VOL", 0x281883D8)), "Failed to initialise VEHICLE_INTERIOR_OPEN_TO_VOL");
	StaticConditionalWarning(sm_RoadNoiseDetailVolCurve.Init(ATSTRINGHASH("ROAD_NOISE_DETAIL_VOL", 0x9679D859)), "Invalid RoadNoiseDetailVolCurve");
	StaticConditionalWarning(sm_RoadNoiseFastVolCurve.Init(ATSTRINGHASH("ROAD_NOISE_FAST_VOL", 0xCDC720A4)), "Invalid RoadNoiseFastVol");
	StaticConditionalWarning(sm_RoadNoiseSteeringAngleToAttenuation.Init(ATSTRINGHASH("ROAD_NOISE_STEERING_ANGLE_TO_ATTENUATION", 0x3A94E0F5)), "Invalid ROAD_NOISE_STEERING_ANGLE_TO_ATTENUATION curve");
	StaticConditionalWarning(sm_RoadNoiseSteeringAngleCRToVolume.Init(ATSTRINGHASH("ROAD_NOISE_STEERING_ANGLE_CR_TO_VOL", 0x81A22045)), "Invalid ROAD_NOISE_STEERING_ANGLE_CR_TO_VOL curve");
	StaticConditionalWarning(sm_RoadNoisePitchCurve.Init(ATSTRINGHASH("ROAD_NOISE_PITCH", 0xF450D2B)), "Invalid RoadNoisePitch");
	StaticConditionalWarning(sm_RoadNoiseSingleVolCurve.Init(ATSTRINGHASH("ROAD_NOISE_SINGLE_VOL", 0xFB08DFAB)), "Invalid RoadNoise single vol");
	StaticConditionalWarning(sm_RoadNoiseBicycleVolCurve.Init(ATSTRINGHASH("ROAD_NOISE_BICYCLE_VOL", 0x3C5A0803)), "Invalid RoadNoise bicycle vol");
	StaticConditionalWarning(sm_SkidSpeedRatioToPitch.Init(ATSTRINGHASH("SKID_CURVES_SPEED_RATIO_TO_PITCH", 0x73EF5C32)), "Failed to initialise SKID_CURVES_SPEED_RATIO_TO_PITCH");
	StaticConditionalWarning(sm_SkidDriftAngleToVolume.Init(ATSTRINGHASH("SKID_CURVES_DRIFT_ANGLE_TO_VOL", 0x44BEB5ED)), "Failed to initialise SKID_CURVES_DRIFT_ANGLE_TO_VOL");
	StaticConditionalWarning(sm_RecentSpeedToSettleVolume.Init(ATSTRINGHASH("RECENT_SPEED_TO_SETTLE_VOL", 0x7439F09E)), "Failed to initialise RECENT_SPEED_TO_SETTLE_VOL");
	StaticConditionalWarning(sm_RecentSpeedToSettleReleaseTime.Init(ATSTRINGHASH("RECENT_SPEED_TO_SETTLE_RELEASE_TIME", 0xE116BA10)), "Failed to initialise RECENT_SPEED_TO_SETTLE_RELEASE_TIME");
	StaticConditionalWarning(sm_SkidFwdSlipToMainVol.Init(ATSTRINGHASH("SKID_CURVES_FWD_SLIP_TO_MAIN_VOL", 0x82AEB645)), "invalid fwd slip to main");
	StaticConditionalWarning(sm_NPCRoadNoiseRelativeSpeedToVol.Init(ATSTRINGHASH("NPC_ROAD_NOISE_RELATIVE_SPEED_TO_VOL", 0x97D711AE)), "invalid sm_NPCRoadNoiseRelativeSpeedToVol");
	StaticConditionalWarning(g_RoadNoiseWetRoadVolCurve.Init(ATSTRINGHASH("ROAD_NOISE_WET_VOL", 0xCF73CEB7)), "invalid road noise wet vol curve");
	StaticConditionalWarning(g_VehicleMassToRumbleVol.Init(ATSTRINGHASH("VEHICLE_MASS_TO_HEAVY_VOL", 0xA2E7D5A)), "invalid veh mass to heavy vol");	
	StaticConditionalWarning(sm_RoofToOpennessCurve.Init(ATSTRINGHASH("VEHICLE_ROOF_OPENNESS", 0x57254724)), "invalid vehicle roof openness curve");
	StaticConditionalWarning(sm_SlipToUndersteerVol.Init(ATSTRINGHASH("SKID_CURVES_FWD_SLIP_TO_UNDERSTEER_VOL", 0x3EFA4444)), "invalid understeer curve");

	StaticConditionalWarning(sm_TarmacSkidSounds.Init(ATSTRINGHASH("TARMAC_TRACTION_SOUNDS", 0xBA04BFF)), "Couldn't find tarmac skid soundset");
	StaticConditionalWarning(sm_ClatterSoundSet.Init(ATSTRINGHASH("ClatterTypes", 0x3E19B4D8)), "Failed to find ClatterTypes sound set");
	StaticConditionalWarning(sm_ChassisStressSoundSet.Init(ATSTRINGHASH("ChassStressTypes", 0x8D1C6834)), "Failed to find ChassStressTypes sound set");
	StaticConditionalWarning(sm_VehiclesInWaterSoundset.Init(ATSTRINGHASH("VEHICLE_WATER_SPLASH_SOUNDSET", 0xCE24C102)), "Failed to find VEHICLE_WATER_SPLASH_SOUNDSET sound set");

	sm_HighSpeedSceneApplySmoother.Init(1.0f/sm_InvHighSpeedSmootherIncreaseRate, 1.0f/sm_InvHighSpeedSmootherDecreaseRate, 0.0f, 1.0f);
	sm_OpennessSmoother.Init(0.01f, 0.0002f, 0.0f, 1.0f);

	// precompute linear filter values to interpolate
	for(s32 i = 0 ; i < NUM_AMBIENTRADIOLEAKAGE; i++)
	{
		g_RadioLPFLinear[i][0] = audDriverUtil::ComputeLinearFromHzFrequency(g_RadioLPFCutoffs[i][0]);
		g_RadioLPFLinear[i][1] = audDriverUtil::ComputeLinearFromHzFrequency(g_RadioLPFCutoffs[i][1]);
	}

#if __BANK
	g_OverridenLeakageVolMin = g_RadioOpennessVols[LEAKAGE_MODDED][0];
	g_OverridenLeakageVolMax = g_RadioOpennessVols[LEAKAGE_MODDED][1];
	g_OverridenLeakageLpfLinearMin = g_RadioLPFLinear[LEAKAGE_MODDED][0];
	g_OverridenLeakageLpfLinearMax = g_RadioLPFLinear[LEAKAGE_MODDED][1];
	g_OverridenLeakageRolloffMin = g_RadioRolloffs[LEAKAGE_MODDED][0];
	g_OverridenLeakageRolloffMax = g_RadioRolloffs[LEAKAGE_MODDED][1];
	g_OverridenLeakageHPFCutoff = g_RadioHPFCutoffs[LEAKAGE_MODDED];
	g_OverridenLeakageMaxRadius = Sqrtf(g_MaxRadiusForAmbientRadio2[LEAKAGE_MODDED]);
#endif
}

// ----------------------------------------------------------------
// audVehicleAudioEntity ShutdownClass
// ----------------------------------------------------------------
void audVehicleAudioEntity::ShutdownClass()
{    
}

// ----------------------------------------------------------------
// audVehicleAudioEntity CalculateVehicleQuadrant
// ----------------------------------------------------------------
audQuadrants audVehicleAudioEntity::GetVehicleQuadrant()
{
	// Quadrant 0 Fdot + Rdot -
	if(m_QuadrantDirty)
	{
		const Vec3V vehPos = m_Vehicle->GetTransform().GetPosition();
		const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition();
		const Vec3V dirToVehicle = Normalize(vehPos - listenerPos); 

		const ScalarV fDot = Dot(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().b(), dirToVehicle);
		const ScalarV rDot = Dot(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().a(), dirToVehicle);

		const BoolV fDotGreaterThanEqualZero = IsGreaterThanOrEqual(fDot, ScalarV(V_ZERO));
		const BoolV rDotGreaterThanEqualZero = IsGreaterThanOrEqual(rDot, ScalarV(V_ZERO));

		const ScalarV quadrantV = SelectFT(fDotGreaterThanEqualZero, SelectFT(rDotGreaterThanEqualZero, ScalarV(V_ONE), ScalarV(V_TWO)), SelectFT(rDotGreaterThanEqualZero, ScalarV(V_ZERO), ScalarV(V_THREE)));
		const audQuadrants quadrant = (audQuadrants)(FloatToIntRaw<0>(quadrantV).Geti());
		m_VehicleQuadrant = quadrant;
		return quadrant;
	}
	else
	{
		return m_VehicleQuadrant;
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity GetPosition
// ----------------------------------------------------------------
Vec3V_Out audVehicleAudioEntity::GetPosition() const
{
	Assert(m_Vehicle);
	return m_Vehicle->GetVehiclePosition();
}

// ----------------------------------------------------------------
// audVehicleAudioEntity GetOrientation
// ----------------------------------------------------------------
audCompressedQuat audVehicleAudioEntity::GetOrientation() const
{
	Assert(m_Vehicle);
	Assert(m_Vehicle->GetPlaceableTracker());
	return m_Vehicle->GetPlaceableTracker()->GetOrientation();
}

// ----------------------------------------------------------------
// audVehicleAudioEntity AllocateVehicleVariableBlock
// ----------------------------------------------------------------
void audVehicleAudioEntity::AllocateVehicleVariableBlock()
{
	if(!HasEntityVariableBlock())
	{
		AllocateEntityVariableBlock(ATSTRINGHASH("VEHICLE_VARIABLES", 0xBE95634E));
		audAssert(HasEntityVariableBlock());
	}
}

// ----------------------------------------------------------------
// Set LOD
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetLOD(audVehicleLOD lod)
{
	if(lod != m_VehicleLOD)
	{
		if(m_VehicleLOD == AUD_VEHICLE_LOD_DUMMY)
		{
			sm_NumDummyVehiclesCounter--;
		}

		m_VehicleLOD = lod;

		if(lod <= AUD_VEHICLE_LOD_DUMMY)
		{
			AllocateVehicleVariableBlock();
			m_LastClatterWorldPos = m_Vehicle->TransformIntoWorldSpace(m_ClatterOffsetPos);
			m_LastTimeInAir = 0;
			m_LastTimeTwoWheeling = 0;
		}

		if(m_VehicleLOD == AUD_VEHICLE_LOD_DUMMY)
		{
			sm_NumDummyVehiclesCounter++;
			sm_NumDummyVehiclesLastFrame++;
		}

		if(m_VehicleLOD >= AUD_VEHICLE_LOD_DUMMY)
		{
			StopAndForgetSounds(m_WheelDetailLoop, m_DirtyWheelLoop, m_GlassyWheelLoop, m_MainSkidLoop, m_SideSkidLoop, m_UndersteerSkidLoop, m_WheelSpinLoop, m_WetWheelSpinLoop, m_WetSkidLoop, m_DriveWheelSlipLoop, m_RoadNoiseRidged);
			StopAndForgetSounds(m_RoadNoiseRidgedPulse, m_ExhastProximityPulseSound, m_VehicleCabinTone, m_CaterpillarTrackLoop);

			for(u32 loop = 0; loop < NUM_VEH_CWHEELS_MAX; loop++)
			{
				StopAndForgetSounds(m_ShallowWaterSounds[loop]);
			}
		}

		if(m_VehicleLOD >= AUD_VEHICLE_LOD_SUPER_DUMMY)
		{
			// Disabled cars keep their sirens and alarms, but nothing else. Super dummy can play rain and warning sounds too.
			StopAndForgetSounds(m_ReverseWarning, m_SurfaceSettleSound, m_VehicleClatter, m_ChassisStressLoop, m_WaterTurbulenceSound, m_ParachuteLoop, m_JumpRecharge);
			StopAndForgetSounds(m_BeingTargetedSound, m_MissileApproachingSound, m_AcquiringTargetSound, m_AllClearSound, m_MissileApproachingSoundImminent, m_DamageWarningSound);
			StopAndForgetSounds(m_DrowningRadioSound, m_RoadNoiseNPC, m_RoadNoiseFast, m_CinematicTireSweetener, m_RoadNoiseHeavy, m_RoadNoiseWet);

			for (u32 i = 0; i < CVehicle::MAX_NUM_WEAPON_BLADES; i++)
			{
				StopAndForgetSounds(m_BladeImpactSound[i]);
			}

			m_SubmarineTransformSoundHash = 0u;

			if(m_VehicleLOD >= AUD_VEHICLE_LOD_DISABLED)
			{
				for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
				{
					m_VehicleGadgets[loop]->StopAllSounds();
				}

				for(u32 i = 0; i < AUD_MAX_WHEEL_WATER_SOUNDS; i++)
				{
					if(m_WaveImpactSounds[i])
					{
						m_WaveImpactSounds[i]->StopAndForget();
					}
				}

                m_ShouldPlayBonnetDetachSound = false;
				m_HasIndicatorRequest = false;
				m_VehicleFireAudio.StopAllSounds();
				StopAndForgetSounds(m_RainLoop, m_DoorOpenWarning, m_WaterLappingSound, m_ClothSound, m_DetachedBonnetSound, m_IdleHullSlapSound, m_LandingGearSound, m_WeaponChargeSound);
				StopSubmersibleSounds();
			}
		}

		switch(m_VehicleLOD)
		{
		case AUD_VEHICLE_LOD_REAL:
			ConvertFromDummy();
			break;
		case AUD_VEHICLE_LOD_DUMMY:
			ConvertToDummy();
			break;
		case AUD_VEHICLE_LOD_SUPER_DUMMY:
			ConvertToSuperDummy();
			break;
		case AUD_VEHICLE_LOD_DISABLED:
			ConvertToDisabled();
			break;
		default:
			break;
		};

		if(m_VehicleLOD != AUD_VEHICLE_LOD_REAL)
		{
			FreeWaveSlot();
		}

		m_TimeAtCurrentLOD = 0.0f;
	}
}

// ----------------------------------------------------------------
// Stop all submersible soudns
// ----------------------------------------------------------------
void audVehicleAudioEntity::StopSubmersibleSounds()
{
	StopAndForgetSounds(m_SubTurningSweetener, m_SubExtrasSound, m_SubmersibleCreakSound, m_SubPropSound);
}

// ----------------------------------------------------------------
// Update cloth sounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateClothSounds(audVehicleVariables& vehicleVariables)
{
	// Wind cloth sounds for vehicles
	if(!m_ClothSound && vehicleVariables.distFromListenerSq < 900)
	{
		fragInstGta* pFragInst = (fragInstGta*)m_Vehicle->GetCurrentPhysicsInst();
		Assertf(pFragInst, "Failed to allocate new fragInstGta.");
		if(pFragInst)
		{
			fragCacheEntry *cacheEntry = pFragInst->GetCacheEntry();
			if( cacheEntry )
			{
				fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
				Assert( hierInst );
				if( hierInst->envCloth )
				{
					audSoundInitParams initParams;
					initParams.Tracker = m_Vehicle->GetPlaceableTracker();
					initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
					CreateAndPlaySound_Persistent(GetWindClothSound(),&m_ClothSound,&initParams);
				}
			}
		}
	}
	else if(vehicleVariables.distFromListenerSq >= 900)
	{
		if(m_ClothSound)
		{
			m_ClothSound->StopAndForget();
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::UpdateWaveImpactSounds(audVehicleVariables& vehicleVariables)
{
	if (vehicleVariables.distFromListenerSq < g_SqdDistThresholdForWaveImpact)
	{
		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			CWheel* wheel = m_Vehicle->GetWheel(i);
			if(wheel)
			{
				u32 hierarchyID = wheel->GetHierarchyId();
				if(hierarchyID >= VEH_WHEEL_LF && hierarchyID <= VEH_WHEEL_RR)
				{
					u32 soundIdx = GetAudioWheelIndex(hierarchyID);

					// We have a valid wheel to play the wave impact sounds.
					if(wheel->GetDynamicFlags().IsFlagSet(WF_INSHALLOWWATER) || wheel->GetDynamicFlags().IsFlagSet(WF_INDEEPWATER))
					{
						Vector3 pos;
						GetCachedWheelPosition(i, pos, vehicleVariables);
						// get the river flow at this position
						Vector2 riverFlow(0.0f, 0.0f);
						River::GetRiverFlowFromPosition(VECTOR3_TO_VEC3V(pos), riverFlow);
						Vec3V vRiverVel = Vec3V(100*riverFlow.x, 100*riverFlow.y, 0.0f);
						vRiverVel.SetZ(ScalarV(V_ZERO));
						float speed = Mag(vRiverVel).Getf();

						if( speed > 0.f)
						{
							if(!m_WaveImpactSounds[soundIdx])
							{
								// Trigger a wave wet impact.
								audSoundInitParams initParams;
								initParams.UpdateEntity = true;
								initParams.Position = pos;
								initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
								CreateAndPlaySound_Persistent(GetVehicleCollisionSettings()->WaveImpactLoop,&m_WaveImpactSounds[soundIdx],&initParams);
							}
						}
						else if(m_WaveImpactSounds[soundIdx])
						{
							m_WaveImpactSounds[soundIdx]->StopAndForget();
						}
					}
					else if(m_WaveImpactSounds[soundIdx])
					{
						m_WaveImpactSounds[soundIdx]->StopAndForget();
					}
				}
			}
		}
	}
	else
	{
		for ( u32 i = 0; i < AUD_MAX_WHEEL_WATER_SOUNDS; i ++)
		{
			if(m_WaveImpactSounds[i])
			{
				m_WaveImpactSounds[i]->StopAndForget();
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
const u32 audVehicleAudioEntity::GetAudioWheelIndex(const u32 wheelId) const 
{
	switch(wheelId)
	{
	case VEH_WHEEL_LF:
		return AUD_VEHICLE_SOUND_WHEEL0 -1;
	case VEH_WHEEL_RF:
		return AUD_VEHICLE_SOUND_WHEEL1 -1;
	case VEH_WHEEL_LR:
		return AUD_VEHICLE_SOUND_WHEEL2 -1;
	case VEH_WHEEL_RR:
		return AUD_VEHICLE_SOUND_WHEEL3 -1;
	default:
		naAssertf(false,"Wrong wheel id when update the wave impacts");
		break;
	}
	return AUD_VEHICLE_SOUND_WHEEL0;
}
// ----------------------------------------------------------------
// SetScriptPriorityVehicle
// ----------------------------------------------------------------
bool audVehicleAudioEntity::SetScriptPriority(audVehicleScriptPriority priority, scrThreadId scriptThreadID)
{
	// Only the controlling thread can modify the priority vehicle status
	if(m_ScriptPriority > AUD_VEHICLE_SCRIPT_PRIORITY_NONE)
	{
		if(audVerifyf(m_ScriptPriortyVehicleThreadID == scriptThreadID, "Attempting to set a script as a priority vehicle when another script (%d) already has control!", m_ScriptPriortyVehicleThreadID))
		{
			m_ScriptPriority = priority;
		}
		else
		{
			return false;
		}
	}
	else
	{
		m_ScriptPriority = priority;
	}

	m_ScriptPriortyVehicleThreadID = scriptThreadID;
	return true;
}

// ----------------------------------------------------------------
// Calculate engine and exhaust positions
// ----------------------------------------------------------------
void audVehicleAudioEntity::CalculateEngineExhaustPositions()
{
#if __BANK
	// in this case the engine and exhuast position will be set in planeaudioentity
	if(g_UseDebugPlaneTracker && GetAudioVehicleType() == AUD_VEHICLE_PLANE)
		return;
	if(g_UseDebugHeliTracker && GetAudioVehicleType() == AUD_VEHICLE_HELI)
		return;
#endif

	m_EngineOffsetPos = Vec3V(V_ZERO);
	m_ExhaustOffsetPos = Vec3V(V_ZERO);
	m_HornOffsetPos = Vec3V(V_ZERO);

	bool isSubmarine = m_Vehicle->InheritsFromSubmarine();
	s32 exhaust1BoneId = isSubmarine? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(SUB_PROPELLER_1) : m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_EXHAUST);
	s32 exhaust2BoneId = isSubmarine? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(SUB_PROPELLER_2) : m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_EXHAUST_2);
	s32 exhaust3BoneId = isSubmarine? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(SUB_PROPELLER_3) : m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_EXHAUST_3);
	s32 exhaust4BoneId = isSubmarine? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(SUB_PROPELLER_4) : m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_EXHAUST_4);
	s32 exhaust5BoneId = isSubmarine? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(SUB_PROPELLER_LEFT) : m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_EXHAUST_5);
	s32 exhaust6BoneId = isSubmarine? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(SUB_PROPELLER_RIGHT) : m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_EXHAUST_6);
	s32 afterburner1BoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(PLANE_AFTERBURNER);
	s32 afterburner2BoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(PLANE_AFTERBURNER+1);
	s32 afterburner3BoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(PLANE_AFTERBURNER+2);
	s32 afterburner4BoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(PLANE_AFTERBURNER+3);
	CompileTimeAssert(PLANE_NUM_AFTERBURNERS == 4);
	s32 bonnetBoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_BUMPER_F);
	Vec3V ex1pos = Vec3V(V_ZERO);
	Vec3V ex2pos = Vec3V(V_ZERO);
	Vec3V ex3pos = Vec3V(V_ZERO);
	Vec3V ex4pos = Vec3V(V_ZERO);
	Vec3V ex5pos = Vec3V(V_ZERO);
	Vec3V ex6pos = Vec3V(V_ZERO);
	
	u32 numExhausts = 0;

	if(m_Vehicle->InheritsFromPlane())
	{	
		if(afterburner1BoneId>-1)
		{
			ex1pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner1BoneId).d();
			numExhausts++;
		}
		if(afterburner2BoneId>-1)
		{
			ex2pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner2BoneId).d();
			numExhausts++;
		}
		if(afterburner3BoneId>-1)
		{
			ex3pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner3BoneId).d();
			numExhausts++;
		}
		if(afterburner4BoneId>-1)
		{
			ex4pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner4BoneId).d();
			numExhausts++;
		}
	}

	if(numExhausts == 0)
	{
		if (exhaust1BoneId>-1)
		{
			ex1pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(exhaust1BoneId).d();
			numExhausts++;
		}
		if(exhaust2BoneId>-1)
		{
			ex2pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(exhaust2BoneId).d();
			numExhausts++;
		}
		if(exhaust3BoneId>-1)
		{
			ex3pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(exhaust3BoneId).d();
			numExhausts++;
		}
		if(exhaust4BoneId>-1)
		{
			ex4pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(exhaust4BoneId).d();
			numExhausts++;
		}
		if(exhaust5BoneId>-1)
		{
			ex5pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(exhaust5BoneId).d();
			numExhausts++;
		}
		if(exhaust6BoneId>-1)
		{
			ex6pos = m_Vehicle->GetSkeletonData().GetDefaultTransform(exhaust6BoneId).d();
			numExhausts++;
		}
	}	

	// use an exhaust position mid way between all exhausts, in local space
	if(numExhausts > 0)
	{
		m_ExhaustOffsetPos = (ex1pos+ex2pos+ex3pos+ex4pos)/ScalarV((f32)numExhausts);
	}

	s32 engineBoneId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_ENGINE);
	s32 engineBoneLId = m_Vehicle->InheritsFromPlane()? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(PLANE_ENGINE_L) : VEH_INVALID_ID;
	s32 engineBoneRId = m_Vehicle->InheritsFromPlane()? m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(PLANE_ENGINE_R) : VEH_INVALID_ID;	

	Vec3V engPos = Vec3V(V_ZERO);
	Vec3V engLPos = Vec3V(V_ZERO);
	Vec3V engRPos = Vec3V(V_ZERO);
	Vec3V engExtraPos = Vec3V(V_ZERO);

	u32 numEngines = 0;

	if(!m_Vehicle->InheritsFromSubmarine())
	{
		if(m_Vehicle->InheritsFromPlane())
		{
			if(afterburner1BoneId>-1)
			{
				engPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner1BoneId).d();
				numEngines++;
			}
			if(afterburner2BoneId>-1)
			{
				engLPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner2BoneId).d();
				numEngines++;
			}
			if(afterburner3BoneId>-1)
			{
				engRPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner3BoneId).d();
				numEngines++;
			}
			if(afterburner4BoneId>-1)
			{
				engExtraPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(afterburner4BoneId).d();
				numEngines++;
			}
		}
		
		if(numEngines == 0)
		{
			if (engineBoneId>-1)
			{
				engPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(engineBoneId).d();
				numEngines++;
			}
			if(engineBoneLId>-1)
			{
				engLPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(engineBoneLId).d();
				numEngines++;
			}
			if(engineBoneRId>-1)
			{
				engRPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(engineBoneRId).d();
				numEngines++;
			}
		}

		// use an engine position mid way between all engine, in local space
		if(numEngines > 0)
		{
			m_EngineOffsetPos = (engPos+engLPos+engRPos+engExtraPos)/ScalarV((f32)numEngines);

			// B*3837724 Fix - Plane engines bones are sometimes centered at the left hand propeller, which causes engine sounds to be panned left.
			// Should be safe to assume the engine sound should always central to the plane body unless we come up with some seriously whacky setups!
			if(m_Vehicle->InheritsFromPlane())
			{
				m_EngineOffsetPos.SetXf(0.f);
			}
		}
	}

	if(bonnetBoneId>-1)
	{
		m_HornOffsetPos = m_Vehicle->GetSkeletonData().GetDefaultTransform(bonnetBoneId).d();
 	}

	// If we've got an engine but no exhaust, just set both to the same position
	// in case anything tries to use the exhaust position
	if(numEngines > 0 &&
	   numExhausts == 0)
	{
		m_ExhaustOffsetPos = m_EngineOffsetPos;
	}

	// Likewise for the opposite scenario
	if(numEngines == 0 &&
		numExhausts > 0)
	{
		m_EngineOffsetPos = m_ExhaustOffsetPos;
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity InitDSPEffects
// ----------------------------------------------------------------
void audVehicleAudioEntity::InitDSPEffects()
{
	if (!g_AudioEngine.ShouldUseCheapAudioEffects() && m_EngineSubmixIndex >= 0 && m_ExhaustSubmixIndex >= 0)
	{						
		ReapplyDSPEffects();		

		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup();
		initParams.UpdateEntity = true;
		initParams.u32ClientVar = AUD_VEHICLE_SOUND_UNKNOWN;
		
		if(!GetEngineEffectSubmixSound())
		{
			const s32 engineSubmixId = audDriver::GetMixer()->GetSubmixIndex(g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex));
			initParams.SourceEffectSubmixId = (s16) engineSubmixId;
			g_FrontendAudioEntity.CreateAndPlaySound_Persistent(GetEngineSubmixVoice(), &sm_VehicleEngineSubmixes[m_EngineSubmixIndex].submixSound, &initParams);
			sm_VehicleEngineSubmixes[m_EngineSubmixIndex].vehicleType = m_VehicleType;
			sm_VehicleEngineSubmixes[m_EngineSubmixIndex].isGranular = IsUsingGranularEngine();
		}

		if(!GetExhaustEffectSubmixSound())
		{
			const s32 exhaustSubmixId = audDriver::GetMixer()->GetSubmixIndex(g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex));
			initParams.SourceEffectSubmixId = (s16) exhaustSubmixId;
			g_FrontendAudioEntity.CreateAndPlaySound_Persistent(GetExhaustSubmixVoice(), &sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].submixSound, &initParams);
			sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].vehicleType = m_VehicleType;
			sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].isGranular = IsUsingGranularEngine();
		}
	}
}

// ----------------------------------------------------------------
// Request a wave slot
// ----------------------------------------------------------------
bool audVehicleAudioEntity::RequestWaveSlot(audWaveSlotManager* waveSlotManager, u32 soundID, bool dspEffectsRequired)
{
	if(waveSlotManager)
	{
		if(dspEffectsRequired)
		{
			if(m_EngineSubmixIndex == -1)
			{
				m_EngineSubmixIndex = static_cast<s8>(g_AudioEngine.GetEnvironment().AssignVehicleEngineSubmix());

				if(m_EngineSubmixIndex >= 0)
				{
					if(sm_VehicleEngineSubmixes[m_EngineSubmixIndex].submixSound)
					{
						g_FrontendAudioEntity.StopAndForgetSounds(sm_VehicleEngineSubmixes[m_EngineSubmixIndex].submixSound);
						sm_VehicleEngineSubmixes[m_EngineSubmixIndex].submixSound = NULL;
						sm_VehicleEngineSubmixes[m_EngineSubmixIndex].vehicleType = AUD_VEHICLE_NONE;
						sm_VehicleEngineSubmixes[m_EngineSubmixIndex].isGranular = false;
					}
				}
			}

			if(m_ExhaustSubmixIndex == -1)
			{
				m_ExhaustSubmixIndex = static_cast<s8>(g_AudioEngine.GetEnvironment().AssignVehicleExhaustSubmix());

				if(m_ExhaustSubmixIndex >= 0)
				{
					if(sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].submixSound)
					{
						g_FrontendAudioEntity.StopAndForgetSounds(sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].submixSound);
						sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].submixSound = NULL;
						sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].vehicleType = AUD_VEHICLE_NONE;
						sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].isGranular = false;
					}
				}
			}
		}

		if(((m_EngineSubmixIndex >= 0 && m_ExhaustSubmixIndex >= 0) || !dspEffectsRequired) && !m_EngineWaveSlot)
		{
			m_EngineWaveSlot = waveSlotManager->LoadBankForSound(soundID, AUD_WAVE_SLOT_ANY, false, this, m_IsFocusVehicle? naWaveLoadPriority::PlayerInteractive : naWaveLoadPriority::General BANK_ONLY(, g_WaveSlotStreamingDelayTime));
		}
	}

	if(dspEffectsRequired)
	{
		if(m_EngineWaveSlot)
		{
			InitDSPEffects();
		}
		else if(m_VehicleLOD != AUD_VEHICLE_LOD_REAL)
		{
			FreeDSPEffects();
		}
	}

	return m_EngineWaveSlot != NULL;
}

// ----------------------------------------------------------------
// Request a low latency wave slot
// ----------------------------------------------------------------
bool audVehicleAudioEntity::RequestLowLatencyWaveSlot(u32 soundID)
{
	if(!m_LowLatencyWaveSlot)
	{
		m_LowLatencyWaveSlot = sm_LowLatencyWaveSlotManager.LoadBankForSound(soundID, AUD_WAVE_SLOT_ANY, false, this, m_IsFocusVehicle? naWaveLoadPriority::PlayerInteractive : naWaveLoadPriority::General BANK_ONLY(, g_WaveSlotStreamingDelayTime));
	}

	return m_LowLatencyWaveSlot != NULL;
}

// ----------------------------------------------------------------
// Request an sfx wave slot
// ----------------------------------------------------------------
bool audVehicleAudioEntity::RequestSFXWaveSlot(u32 soundID, bool useHighQualitySlot)
{
	if(!m_SFXWaveSlot)
	{
		audWaveSlotManager* waveSlotManager = useHighQualitySlot? &sm_HighQualityWaveSlotManager : &sm_StandardWaveSlotManager;
		m_SFXWaveSlot = waveSlotManager->LoadBankForSound(soundID, AUD_WAVE_SLOT_ANY, false, this, m_IsFocusVehicle? naWaveLoadPriority::PlayerInteractive : naWaveLoadPriority::General BANK_ONLY(, g_WaveSlotStreamingDelayTime));		
	}

	return m_SFXWaveSlot != NULL;
}

// ----------------------------------------------------------------
// Request a wave slot
// ----------------------------------------------------------------
void audVehicleAudioEntity::FreeWaveSlot(bool freeDSPEffects)
{
	if(m_EngineWaveSlot)
	{
		audWaveSlotManager::FreeWaveSlot(m_EngineWaveSlot, this);
	}

	if(m_LowLatencyWaveSlot)
	{
		audWaveSlotManager::FreeWaveSlot(m_LowLatencyWaveSlot, this);
	}

	if(m_SFXWaveSlot)
	{
		audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
	}

	if(freeDSPEffects)
	{
		FreeDSPEffects();
	}
}

// ----------------------------------------------------------------
// Update the vehicle filters
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateDSPEffects(audVehicleDSPSettings& dspSettings , audVehicleVariables& vehicleVariables)
{
	audSound* engineEffectSubmixSound = GetEngineEffectSubmixSound();
	audSound* exhaustEffectSubmixSound = GetExhaustEffectSubmixSound();

	if(engineEffectSubmixSound && exhaustEffectSubmixSound && engineEffectSubmixSound->GetPlayState() == AUD_SOUND_PLAYING && exhaustEffectSubmixSound->GetPlayState() == AUD_SOUND_PLAYING)
	{
		bool disableDSP = (m_IsFocusVehicle && g_DisablePlayerVehicleDSP) || (!m_IsFocusVehicle && g_DisableNPCVehicleDSP);

#if __BANK
		disableDSP |= g_DisableEngineExhaustDSP;		
#endif

		g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::Bypass, disableDSP? 1 : 0);
		g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::Bypass, disableDSP? 1 : 0);
		
		if(IsSafeToApplyDSPChanges())
		{
			if(m_ResetDSPEffects)
			{
				g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, GetEngineSubmixSynth());
				g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, GetExhaustSubmixSynth());
				m_ResetDSPEffects = false;
			}

			if(m_ReapplyDSPPresets BANK_ONLY(|| g_AutoReapplyDSPPresets))
			{
				g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::PresetNameHash, GetEngineSubmixSynthPreset());
				g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::PresetNameHash, GetExhaustSubmixSynthPreset());
				m_ReapplyDSPPresets = false;
			}
		}		

		dspSettings.proximityRatio = ComputeProximityEffectRatio();
		dspSettings.AddDSPParameter(atHashString("Proximity", 0x1C158A61), dspSettings.proximityRatio);

		f32 proximityVolumeBoost = 0.0f;
		
		if(!vehicleVariables.isInFirstPersonCam && !vehicleVariables.isInHoodMountedCam)
		{
			proximityVolumeBoost = dspSettings.proximityRatio * GetExhaustProximityVolumeBoost();
			GranularEngineAudioSettings* granularSettings = GetGranularEngineAudioSettings();

			bool isExhaustUpgraded = false;

			if(m_VehicleType == AUD_VEHICLE_CAR)
			{
				isExhaustUpgraded = ((audCarAudioEntity*)this)->IsExhaustUpgraded();
			}

			if(granularSettings && (AUD_GET_TRISTATE_VALUE(granularSettings->Flags, FLAG_ID_GRANULARENGINEAUDIOSETTINGS_EXHAUSTPROXIMITYMIXERSCENEENABLED) == AUD_TRISTATE_TRUE || isExhaustUpgraded))
			{
				sm_PlayerVehicleProximityRatio = dspSettings.proximityRatio;
			}
		}

		if(m_IsPlayerVehicle && audNorthAudioEngine::ShouldTriggerPulseHeadset() && sm_PlayerVehicleProximityRatio > 0.0f)
		{
			if(!m_ExhastProximityPulseSound)
			{
				CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("ExhaustProximity", 0x102BBD7)), &m_ExhastProximityPulseSound);
			}

			if(m_ExhastProximityPulseSound)
			{
				const Vec3V camPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(0);
				const Vec3V exhaustPos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
				const Vec3V camToCar = (exhaustPos-camPos);
				m_ExhastProximityPulseSound->FindAndSetVariableValue(ATSTRINGHASH("DistanceToExhaust", 0xBB757FBF), Mag(camToCar).Getf());
				m_ExhastProximityPulseSound->FindAndSetVariableValue(ATSTRINGHASH("Revs", 0x44D3B231), vehicleVariables.revs);
				m_ExhastProximityPulseSound->FindAndSetVariableValue(ATSTRINGHASH("Throttle", 0xEA0151DC), vehicleVariables.throttle);
			}
		}
		else
		{
			StopAndForgetSounds(m_ExhastProximityPulseSound);
		}

		for(u32 loop = 0; loop < dspSettings.dspParameters.GetCount(); loop++)
		{
			bool validForEngine = (dspSettings.dspParameters[loop].submixType == audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_BOTH || dspSettings.dspParameters[loop].submixType == audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
			bool validForExhaust = (dspSettings.dspParameters[loop].submixType == audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_BOTH || dspSettings.dspParameters[loop].submixType == audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);

			if(validForEngine)
			{
				g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, dspSettings.dspParameters[loop].parameterName.GetHash(), dspSettings.dspParameters[loop].parameterValue);
			}

			if(validForExhaust)
			{
				g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, dspSettings.dspParameters[loop].parameterName.GetHash(), dspSettings.dspParameters[loop].parameterValue);
			}
		}

		audRequestedSettings *engineReqSets = engineEffectSubmixSound->GetRequestedSettings();
		audRequestedSettings *exhaustReqSets = exhaustEffectSubmixSound->GetRequestedSettings();
		
		if(exhaustReqSets && engineReqSets)
		{
		if(m_Vehicle)
		{
#if __BANK
			if(m_IsPlayerVehicle && ((g_UseDebugPlaneTracker && GetAudioVehicleType() == AUD_VEHICLE_PLANE) || (g_UseDebugHeliTracker && GetAudioVehicleType() == AUD_VEHICLE_HELI)))
			{
				// the absolute positions are set by the debug tracker, both sounds are at the same location but that shouldn't matter for tuning flybys
				engineReqSets->SetPosition(m_EngineOffsetPos);
				exhaustReqSets->SetPosition(m_ExhaustOffsetPos);
				engineReqSets->SetPan(-1);
				exhaustReqSets->SetPan(-1);
			}
			else
#endif
			{
				if(vehicleVariables.isInFirstPersonCam)
				{
					if(g_InteriorEngineExhaustAtListener)
					{
						const Vec3V listenerPosition = g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0);
						engineReqSets->SetPosition(listenerPosition);
						exhaustReqSets->SetPosition(listenerPosition);
					}
					else
					{
						engineReqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos));
						exhaustReqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos));
					}
					
					if(g_InteriorEngineExhaustPanned)
					{
						f32 engineDegrees = atan2(m_EngineOffsetPos.GetXf(), m_EngineOffsetPos.GetYf()) * RtoD;
						f32 exhaustDegrees = atan2(m_ExhaustOffsetPos.GetXf(), m_ExhaustOffsetPos.GetYf()) * RtoD;

						Vec3V vehicleForward = Vec3V(V_Y_AXIS_WZERO);						
						Vec3V listenerForward = m_Vehicle->GetTransform().UnTransform(m_Vehicle->GetTransform().GetPosition() + g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1());
						listenerForward.SetZ(ScalarV(V_ZERO));
						listenerForward = Normalize(listenerForward);

						Vec3V forwardCross = Cross(vehicleForward, listenerForward);
						f32 forwardDot = Dot(vehicleForward, listenerForward).Getf();
						f32 headAngleDegrees = AcosfSafe(forwardDot) * RtoD;

						if(Dot(Vec3V(V_UP_AXIS_WZERO), forwardCross).Getf() < 0.0f)
						{
							headAngleDegrees *= -1.0f;
						}

						s32 engineAngle = (s32)(engineDegrees + headAngleDegrees) % 360;
						s32 exhaustAngle = (s32)(exhaustDegrees + headAngleDegrees) % 360;

						if(engineAngle < 0)
						{
							engineAngle += 360;
						}

						if(exhaustAngle < 0)
						{
							exhaustAngle += 360;
						}

						engineReqSets->SetPan(engineAngle);
						exhaustReqSets->SetPan(exhaustAngle);
					}
					else
					{
						engineReqSets->SetPan(-1);
						exhaustReqSets->SetPan(-1);
					}
				}
				else
				{
					engineReqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos));
					exhaustReqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos));
					engineReqSets->SetPan(-1);
					exhaustReqSets->SetPan(-1);
				}
			}
		}

		static f32 engineExhaustSubmixVolumeTrim = 5.0f;

		if(g_BonnetCamStereoEffectEnabled && g_ReflectionsAudioEntity.GetActiveVehicle() == this && vehicleVariables.isInFirstPersonCam)
		{
			dspSettings.enginePostSubmixAttenuation += g_StereoEffectEngineVolumeTrim;
			dspSettings.exhaustPostSubmixAttenuation += g_StereoEffectExhaustVolumeTrim;
		}

		engineReqSets->SetSourceEffectMix(1.0f, 1.0f);
		engineReqSets->SetPostSubmixVolumeAttenuation(dspSettings.enginePostSubmixAttenuation + engineExhaustSubmixVolumeTrim);
		engineReqSets->SetVolumeCurveScale(dspSettings.rolloffScale);
		engineReqSets->SetFrequencySmoothingRate(sm_FrequencySmoothingRate);
		engineReqSets->SetEnvironmentalLoudnessFloat(dspSettings.environmentalLoudness);

		exhaustReqSets->SetSourceEffectMix(1.0f, 1.0f);
		exhaustReqSets->SetPostSubmixVolumeAttenuation(dspSettings.exhaustPostSubmixAttenuation + engineExhaustSubmixVolumeTrim + proximityVolumeBoost);
		exhaustReqSets->SetVolumeCurveScale(dspSettings.rolloffScale);
		exhaustReqSets->SetFrequencySmoothingRate(sm_FrequencySmoothingRate);
		exhaustReqSets->SetEnvironmentalLoudnessFloat(dspSettings.environmentalLoudness);

		if(!m_Vehicle->m_nVehicleFlags.bEngineStarting && !m_Vehicle->m_nVehicleFlags.bEngineOn)
		{
			// Mute the submix voices if we want to keep hold of them but aren't actually playing anything through them. Can occur when eg. the player is sitting inside a car that has been switched off by script,
			// such as in the car mod shop
			engineReqSets->SetVolume(audDriver::GetMixer()->GetActiveVoiceCount(g_AudioEngine.GetEnvironment().GetVehicleEngineSubmixId(m_EngineSubmixIndex)) > 0? 0.0f : g_SilenceVolume);
			exhaustReqSets->SetVolume(audDriver::GetMixer()->GetActiveVoiceCount(g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmixId(m_ExhaustSubmixIndex)) > 0? 0.0f : g_SilenceVolume);
		}
		else
		{			
			engineReqSets->SetVolume(0.0f);
			exhaustReqSets->SetVolume(0.0f);
		} 
		}

		// This is needed to blend between player/NPC volumes
		m_EngineEffectWetSmoother.CalculateValue(m_IsPlayerSeatedInVehicle?1.0f:0.0f,dspSettings.timeInMs); 
	}

#if __BANK
	if(g_DebugDSPParams && m_IsPlayerVehicle)
	{
		char tempString[128];
		f32 xCoord = 0.1f;
		f32 yCoord = 0.1f;
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), "DSP Parameters");
		yCoord += 0.02f;
		xCoord += 0.05f;

		for(u32 loop = 0; loop < dspSettings.dspParameters.GetCount(); loop++)
		{
			sprintf(tempString, "%s: %.02f", dspSettings.dspParameters[loop].parameterName.GetCStr(), dspSettings.dspParameters[loop].parameterValue);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
			yCoord += 0.02f;
		}
	}
#endif
}

// ----------------------------------------------------------------
// Check if this vehicle should use DSP effects
// ----------------------------------------------------------------
bool audVehicleAudioEntity::ShouldUseDSPEffects() const
{	
	return BANK_ONLY(g_ForceSynthsOnAllEngines ||) m_IsFocusVehicle || sm_IsInCarMeet || (NetworkInterface::IsGameInProgress() && (m_Vehicle->IsPersonalVehicle() || m_LastPlayerVehicle)) || (g_AllowSynthsOnNetworkVehicles && m_IsNetworkPlayerSeatedInVehicle);
}

// ----------------------------------------------------------------
// Free any DSP effects so they can be reused
// ----------------------------------------------------------------
void audVehicleAudioEntity::FreeDSPEffects()
{
	audAssert(m_VehicleLOD != AUD_VEHICLE_LOD_REAL);

	if(m_EngineSubmixIndex >= 0)
	{
		g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, 0);
		g_AudioEngine.GetEnvironment().FreeVehicleEngineSubmix(m_EngineSubmixIndex);
		m_EngineSubmixIndex = -1;
	}

	if(m_ExhaustSubmixIndex >= 0)
	{
		g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, 0);
		g_AudioEngine.GetEnvironment().FreeVehicleExhaustSubmix(m_ExhaustSubmixIndex);
		m_ExhaustSubmixIndex = -1;
	}	
}

// ----------------------------------------------------------------
// Reapply the dsp presets
// ----------------------------------------------------------------
void audVehicleAudioEntity::ReapplyDSPEffects()
{
	if(m_EngineSubmixIndex >= 0)
	{
		u32 engineSubmixSynth = GetEngineSubmixSynth();

		if(engineSubmixSynth == g_NullSoundHash || IsSafeToApplyDSPChanges())
		{
			g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, engineSubmixSynth);
			g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::PresetNameHash, GetEngineSubmixSynthPreset());
		}
		else
		{
			m_ResetDSPEffects = true;
			m_ReapplyDSPPresets = true;
		}
	}

	if(m_ExhaustSubmixIndex >= 0)
	{
		u32 exhaustSubmixSynth = GetExhaustSubmixSynth();

		if(exhaustSubmixSynth == g_NullSoundHash || IsSafeToApplyDSPChanges())
		{
			g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, exhaustSubmixSynth);
			g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::PresetNameHash, GetExhaustSubmixSynthPreset());
		}
		else
		{
			m_ResetDSPEffects = true;
			m_ReapplyDSPPresets = true;
		}
	}
}

// ----------------------------------------------------------------
// Update damage warning
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateDamageWarning()
{
	f32 engineHealth = ComputeEffectiveEngineHealth();
	f32 damageRatio = Clamp(1.0f - (engineHealth / CTransmission::GetEngineHealthMax()), 0.0f, 1.0f);

	if(NetworkInterface::IsGameInProgress() &&
		m_Vehicle->IsTank() && 
		m_IsPlayerVehicle &&
		m_Vehicle->m_nVehicleFlags.bEngineOn &&
		damageRatio > 0.0f &&
		AreWarningSoundsValid())
	{
		if(!m_DamageWarningSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			
			if (GetVehicleModelNameHash() == ATSTRINGHASH("MINITANK", 0xB53C6C52))
			{
				audSoundSet soundSet;

				if (soundSet.Init(m_IsFocusVehicle ? ATSTRINGHASH("ch_vehicle_minitank_player_sounds", 0xCF2B0226) : ATSTRINGHASH("ch_vehicle_minitank_remote_sounds", 0x90AF5388)))
				{
					CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("damage_warning", 0xE58B27A1)), &m_DamageWarningSound, &initParams);
				}
			}
			else
			{
				CreateAndPlaySound_Persistent(ATSTRINGHASH("TANK_DAMAGE_WARNING_MASTER", 0x1121601B), &m_DamageWarningSound, &initParams);
			}
		}

		if(m_DamageWarningSound)
		{
			m_DamageWarningSound->FindAndSetVariableValue(ATSTRINGHASH("EngineDamage", 0xC51EB951), damageRatio);
		}
	}
	else
	{
		StopAndForgetSounds(m_DamageWarningSound);
	}
}

// ----------------------------------------------------------------
// Update the missile lock on effects
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateMissileLock()
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	if(CGameWorld::GetMainPlayerInfo()->AreControlsDisabled())
	{
		return;
	}
	if(m_Vehicle)
	{
		bool isMissileApproaching = false;
		bool isBeingTargeted = false;
		bool isAcquiringTarget = false;
		bool hasTarget = false;

		if(m_HasMissileLockWarningSystem || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::EnableMissileLockWarningForAllVehicles))
		{
			bool missileApproachingThisFrame = m_Vehicle->GetHomingProjectileDistance() > 0.0f;

			if(missileApproachingThisFrame)
			{
				m_MissileApproachTimer += fwTimer::GetTimeStep();
				isMissileApproaching = m_MissileApproachTimer > 0.5f;
			}
			else
			{
				m_MissileApproachTimer = 0.0f;
			}

			isBeingTargeted = !isMissileApproaching && (m_Vehicle->GetHomingLockedOntoState() == CEntity::HLOnS_ACQUIRED || m_Vehicle->GetHomingLockedOntoState() == CEntity::HLOnS_ACQUIRING);
			isAcquiringTarget = m_Vehicle->GetHomingLockOnState() == CEntity::HLOnS_ACQUIRING;
			hasTarget = m_Vehicle->GetHomingLockOnState() == CEntity::HLOnS_ACQUIRED;
		}

		if(m_IsPlayerVehicle && m_Vehicle->GetStatus() != STATUS_WRECKED)
		{
#if __BANK
			if(g_OverrideMissileStatus)
			{
				switch(g_SimulatedMissileState)
				{
				case 0:
					isBeingTargeted = false;
					isMissileApproaching = false;
					isAcquiringTarget = false;
					hasTarget = false;
					break;
				case 1:
					isBeingTargeted = true;
					isMissileApproaching = false;
					isAcquiringTarget = false;
					hasTarget = false;
					break;
				case 2:
					isBeingTargeted = false;
					isMissileApproaching = true;
					isAcquiringTarget = false;
					hasTarget = false;
					break;
				case 3:
					isBeingTargeted = false;
					isMissileApproaching = false;
					isAcquiringTarget = true;
					hasTarget = false;
					break;
				case 4:
					isBeingTargeted = false;
					isMissileApproaching = false;
					isAcquiringTarget = false;
					hasTarget = true;
					break;
				}
			}

			if(g_DebugMissileLockStatus)
			{
				char tempString[1024];
				const char* lockOnStateNames[] = { "None", "Acquiring", "Acquired" };
				formatf(tempString, "Lock On State: %s", lockOnStateNames[m_Vehicle->GetHomingLockOnState()]);
				safecatf(tempString, "\nLocked Onto State: %s", lockOnStateNames[m_Vehicle->GetHomingLockedOntoState()]);

				if (m_HasMissileLockWarningSystem)
				{
					safecatf(tempString, "\nBeing Targeted Sound: %s", m_BeingTargetedSound ? "Active" : "Inactive");
					safecatf(tempString, "\nMissile Approaching Sound: %s", m_MissileApproachingSound ? "Active" : "Inactive");
					safecatf(tempString, "\nMissile Approaching Imminent Sound: %s", m_MissileApproachingSoundImminent ? "Active" : "Inactive");
					safecatf(tempString, "\nAll Clear Sound: %s", m_AllClearSound ? "Active" : "Inactive");
					safecatf(tempString, "\nAcquiring Target: %s", m_AcquiringTargetSound && m_Vehicle->GetHomingLockOnState() != CEntity::HLOnS_ACQUIRED ? "Active" : "Inactive");
					safecatf(tempString, "\nTarget Acquired Sound: %s", m_AcquiringTargetSound && m_Vehicle->GetHomingLockOnState() == CEntity::HLOnS_ACQUIRED ? "Active" : "Inactive");
				}
				else
				{
					safecatf(tempString, "\nAudio Missile Lock System Not Enabled");
				}

				grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition(), Color_white, tempString, true, -1);
			}
#endif

			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			initParams.Pan = 0;

			if(isBeingTargeted && !isMissileApproaching)
			{
				if(m_IsPlayerVehicle)
				{
					sm_TimeMissleFiredStarted = 0;
					sm_TimeAllClearStarted = 0;

					/*if(sm_TimeTargetLockStarted == 0)
						sm_TimeTargetLockStarted = currentTime;
					else if(currentTime - sm_TimeTargetLockStarted> (g_AircraftWarningSettings ? g_AircraftWarningSettings->TargetedLock.MinTimeInStateToTrigger : 1500) )
						g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_TARGETED_LOCK);*/
				}

				if(fwTimer::GetTimeInMilliseconds() - m_LastHitByRocketTime > 1000)
				{
					StopAndForgetSounds(m_MissileApproachingSound);

					StopAndForgetSounds(m_MissileApproachingSoundImminent);

					if(!m_BeingTargetedSound && !m_AllClearSound)
					{
						CreateAndPlaySound_Persistent(sm_MissileLockSoundSet.Find(ATSTRINGHASH("TARGETED_LOCK", 3810899439)), &m_BeingTargetedSound, &initParams);
					}
				}
			}
			else if(isMissileApproaching)
			{
				static dev_float IMMINENT_MISSILE_DISTANCE = 35.0f;

				if(m_IsPlayerVehicle)
				{
					sm_TimeTargetLockStarted = 0;
					sm_TimeAllClearStarted = 0;

					if(sm_TimeMissleFiredStarted == 0)
						sm_TimeMissleFiredStarted = currentTime;
					else if(currentTime - sm_TimeMissleFiredStarted> (g_AircraftWarningSettings ? g_AircraftWarningSettings->MissleFired.MinTimeInStateToTrigger : 1500) )
						g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_MISSLE_FIRED);
				}

				StopAndForgetSounds(m_BeingTargetedSound);

				if(m_MissileApproachingSoundImminent)
				{
					if(m_Vehicle->GetHomingProjectileDistance() >= IMMINENT_MISSILE_DISTANCE)
					{
						StopAndForgetSounds(m_MissileApproachingSoundImminent);
					}
				}

				if(!m_MissileApproachingSound && !m_AllClearSound && !m_MissileApproachingSoundImminent)
				{
					CreateAndPlaySound_Persistent(sm_MissileLockSoundSet.Find(ATSTRINGHASH("MISSILE_FIRED", 484541956)), &m_MissileApproachingSound, &initParams);
				}

				if(m_MissileApproachingSound)
				{
					f32 distance = Clamp(m_Vehicle->GetHomingProjectileDistance() / 200.0f, 0.0f, 1.0f);

#if __BANK
					if(g_OverrideMissileStatus)
					{
						distance = g_SimulatedMissileDistance;
					}
#endif
					m_MissileApproachingSound->FindAndSetVariableValue(ATSTRINGHASH("MISSILE_DISTANCE_TO_PLANE", 3642087817), 1.0f - distance);
					
					if(m_Vehicle->GetHomingProjectileDistance() < IMMINENT_MISSILE_DISTANCE)
					{
						if(!m_MissileApproachingSoundImminent && !m_AllClearSound)
						{
							CreateAndPlaySound_Persistent(sm_MissileLockSoundSet.Find(ATSTRINGHASH("MISSILE_FIRED_SOLID", 1352055251)), &m_MissileApproachingSoundImminent, &initParams);						
							
							//Stop the beeping when we have a solid tone
							StopAndForgetSounds(m_MissileApproachingSound);
						}
					}					
				}				
			}
			else
			{
				if(m_WasMissileApproaching || m_WasBeingTargeted)
				{
					if(m_IsPlayerVehicle)
					{
						sm_TimeTargetLockStarted = 0;
						sm_TimeMissleFiredStarted = 0;

						if(sm_TimeAllClearStarted == 0)
							sm_TimeAllClearStarted = currentTime;
						else if(currentTime - sm_TimeAllClearStarted> (g_AircraftWarningSettings ? g_AircraftWarningSettings->AllClear.MinTimeInStateToTrigger : 1500) )
							g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ALL_CLEAR);
					}

					if(!m_AllClearSound && fwTimer::GetTimeInMilliseconds() - m_LastHitByRocketTime > 250)
					{
						CreateAndPlaySound_Persistent(sm_MissileLockSoundSet.Find(ATSTRINGHASH("ALL_CLEAR", 4004323938)), &m_AllClearSound, &initParams);
						m_MissileApproachTimer = 0.0f;
					}
				}
				else
				{
					if(m_IsPlayerVehicle)
					{
						sm_TimeAllClearStarted = 0;
					}
				}

				StopAndForgetSounds(m_MissileApproachingSound, m_BeingTargetedSound, m_MissileApproachingSoundImminent);
			}

			if(isAcquiringTarget && !hasTarget)
			{
				if(m_IsPlayerVehicle)
				{
					sm_TimeATargetAcquiredStarted = 0;

					/*if(sm_TimeAcquiringTargetStarted == 0)
						sm_TimeAcquiringTargetStarted = currentTime;
					else if(currentTime - sm_TimeAcquiringTargetStarted > (g_AircraftWarningSettings ? g_AircraftWarningSettings->AcquiringTarget.MinTimeInStateToTrigger : 1500) )
						g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ACQUIRING_TARGET);*/
				}

				if(!m_WasAcquiringTarget)
				{
					StopAndForgetSounds(m_AcquiringTargetSound);
				}

				if(!m_AcquiringTargetSound)
				{
					CreateAndPlaySound_Persistent(sm_MissileLockSoundSet.Find(ATSTRINGHASH("ACQUIRING_TARGET", 0xCCF0B21D)), &m_AcquiringTargetSound, &initParams);
				}
			}
			else if(hasTarget)
			{
				if(m_IsPlayerVehicle)
				{
					sm_TimeAcquiringTargetStarted = 0;

					if(sm_TimeATargetAcquiredStarted == 0)
						sm_TimeATargetAcquiredStarted = currentTime;
					else if(currentTime - sm_TimeATargetAcquiredStarted > (g_AircraftWarningSettings ? g_AircraftWarningSettings->TargetAcquired.MinTimeInStateToTrigger : 1500) )
						g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_TARGET_ACQUIRED);
				}

				if(!m_HadAcquiredTarget && m_AcquiringTargetSound)
				{
					StopAndForgetSounds(m_AcquiringTargetSound);
				}

				bool isTimeValid = true;

				// Stop the locked-on sound after a set period of time
				if(m_IsPlayerVehicle && currentTime - sm_TimeATargetAcquiredStarted > 1000)
				{
					isTimeValid = false;
				}

				if(isTimeValid)
				{
					if(!m_AcquiringTargetSound)
					{
						CreateAndPlaySound_Persistent(sm_MissileLockSoundSet.Find(ATSTRINGHASH("TARGET_ACQUIRED", 0x96CCCE93)), &m_AcquiringTargetSound, &initParams);
					}
				}
				else
				{
					StopAndForgetSounds(m_AcquiringTargetSound);
				}
			}
			else
			{
				if(m_IsPlayerVehicle)
				{
					sm_TimeAcquiringTargetStarted = 0;
					sm_TimeATargetAcquiredStarted = 0;
				}

				StopAndForgetSounds(m_AcquiringTargetSound);
			}
		}
		else
		{
			StopAndForgetSounds(m_MissileApproachingSound, m_BeingTargetedSound, m_AcquiringTargetSound, m_MissileApproachingSoundImminent);
		}

		m_WasMissileApproaching = isMissileApproaching;
		m_WasBeingTargeted = isBeingTargeted;
		m_WasAcquiringTarget = isAcquiringTarget;
		m_HadAcquiredTarget = hasTarget;
	}
	else
	{
		m_WasMissileApproaching = false;
		m_WasBeingTargeted = false;
		m_WasAcquiringTarget = false;
		m_HadAcquiredTarget = false;
	}
}

void audVehicleAudioEntity::UpdateCabinTone(audVehicleVariables& params)
{
	bool cabinToneActive = params.isInFirstPersonCam && m_Vehicle->IsEngineOn();

	if(cabinToneActive)
	{		
		if(!m_VehicleCabinTone)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetEnvironmentGroup();
			initParams.TrackEntityPosition = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_INTERIOR_OCCLUSION;
			initParams.UpdateEntity = true;
			CreateAndPlaySound_Persistent(GetCabinToneSound(), &m_VehicleCabinTone, &initParams);			
		}
		
		if(m_VehicleCabinTone)
		{
			m_VehicleCabinTone->FindAndSetVariableValue(ATSTRINGHASH("openness", 0x2721BC39), GetOpenness(false));
		}
	}
	else if(m_VehicleCabinTone)
	{
		StopAndForgetSounds(m_VehicleCabinTone);
	}
}

void audVehicleAudioEntity::UpdateAircraftWarningSpeech()
{
	//TODO: AUD_AW_OVERSPEED
	if(!g_AircraftWarningSettings || !m_Vehicle || !AreWarningSoundsValid() ||
		!(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_HELI) )
		return;

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	if(FindPlayerPed() && FindPlayerPed()->GetPlayerInfo() && FindPlayerPed()->GetPlayerInfo()->HasPlayerLeftTheWorld() &&
		currentTime - m_TimeOfLastAircraftLowFuelWarning > g_AircraftWarningSettings->LowFuel.MinTimeBetweenPlay)
	{
		g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_LOW_FUEL);
		m_TimeOfLastAircraftLowFuelWarning = currentTime;
	}

	if(currentTime - m_TimeOfLastAircraftOverspeedWarning > g_AircraftWarningSettings->Overspeed.MinTimeBetweenPlay)
	{
		f32 fwdSpeed = DotProduct(m_CachedVehicleVelocity, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB()));
		if(fwdSpeed > GetOverspeedValue())
		{
			g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_OVERSPEED);
			m_TimeOfLastAircraftOverspeedWarning = currentTime;
		}
	}

	if(fwTimer::GetTimeInMilliseconds() - m_LastTimeOnGround > 15000)
	{
		//Update terrain probe
		u32 frameCount = fwTimer::GetFrameCount();

		if(frameCount % 30 == 0)
		{
			bool largeVehicle = m_Vehicle->GetSeatManager()->GetMaxSeats() > 8;
			const Vector3& probeForward = VEC3V_TO_VECTOR3(m_Vehicle->GetVehicleForwardDirectionRef()) * g_AircraftWarningSettings->Terrain.ForwardProbeLength;

			const Vector3& pos = VEC3V_TO_VECTOR3(m_Vehicle->GetVehiclePosition());
			WorldProbe::CShapeTestHitPoint hitPoint;
			WorldProbe::CShapeTestResults probeResults(hitPoint);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetStartAndEnd(pos, pos + probeForward);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			probeDesc.SetExcludeEntity(m_Vehicle, largeVehicle ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
			bool terrainHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			if(terrainHit )
			{
				if(currentTime - m_LastTimeTerrainProbeHit > g_AircraftWarningSettings->PullUp.MaxTimeSinceTerrainTriggerToPlay )
				{
					if(currentTime - m_LastTimeTerrainProbeNotHit > g_AircraftWarningSettings->Terrain.MinTimeInStateToTrigger)
					{
						g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_TERRAIN);
					}
					m_LastTimeTerrainProbeHit = currentTime;
				}
				else
					if(currentTime - m_LastTimeTerrainProbeNotHit > g_AircraftWarningSettings->PullUp.MinTimeInStateToTrigger)
					{
						g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_PULL_UP);
					}
			}
			else
				m_LastTimeTerrainProbeNotHit = currentTime;
		}
		else if(frameCount % 30 == 15 && m_Vehicle->GetVehicleType() != VEHICLE_TYPE_HELI)
		{
			//Update ground probe
			const Vector3& pos = VEC3V_TO_VECTOR3(m_Vehicle->GetVehiclePosition());
			WorldProbe::CShapeTestHitPoint hitPoint;
			WorldProbe::CShapeTestResults probeResults(hitPoint);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetStartAndEnd(pos, Vector3(pos.x, pos.y, pos.z - g_AircraftWarningSettings->AltitudeWarningLow.DownProbeLength));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			probeDesc.SetExcludeEntity(m_Vehicle);
			bool groundHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			if(groundHit && currentTime - m_LastTimeGroundProbeNotHit > g_AircraftWarningSettings->AltitudeWarningLow.MinTimeInStateToTrigger)
			{
				if(currentTime - m_LastTimeTerrainProbeNotHit > g_AircraftWarningSettings->Terrain.MinTimeInStateToTrigger)
				{
					g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ALTITUDE_WARNING_LOW);
				}
			}
			else
			{
				m_LastTimeGroundProbeNotHit = currentTime;
			}
		}
	}

	if(m_Vehicle->GetVehicleDamage() && currentTime - m_LastTimeAircraftDamageReported > g_AircraftWarningSettings->MinTimeBetweenDamageReports)
	{
		f32 overallHealth = m_Vehicle->GetVehicleDamage()->GetOverallHealth();
		if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE && ((CPlane*)m_Vehicle)->AnyPlaneEngineOnFire())
		{
			if(!m_HasPlayedEngineOnFire1)
			{
				g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ENGINE_1_FIRE);
				m_HasPlayedEngineOnFire1 = true;
			}
			else if(!m_HasPlayedEngineOnFire2)
			{
				g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ENGINE_2_FIRE);
				m_HasPlayedEngineOnFire2 = true;
			}
			else if(!m_HasPlayedEngineOnFire3)
			{
				g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ENGINE_3_FIRE);
				m_HasPlayedEngineOnFire3 = true;
			}
			else if(!m_HasPlayedEngineOnFire4)
			{
				g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_ENGINE_4_FIRE);
				m_HasPlayedEngineOnFire4 = true;
			}

			m_LastTimeAircraftDamageReported = currentTime;
		}
		if(overallHealth < GetCriticalDamageThreshold() && 
				currentTime - m_TimeOfLastAircraftLowDamageWarning > g_AircraftWarningSettings->DamagedSerious.MinTimeBetweenPlay)
		{
			g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_DAMAGED_CRITICAL);
			m_TimeOfLastAircraftLowDamageWarning = currentTime;
			m_LastTimeAircraftDamageReported = currentTime;
		}
		else if(overallHealth < GetSeriousDamageThreshold() && 
				currentTime - m_TimeOfLastAircraftHighDamageWarning > g_AircraftWarningSettings->DamagedSerious.MinTimeBetweenPlay)
		{
			g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_DAMAGED_SERIOUS);
			m_TimeOfLastAircraftHighDamageWarning = currentTime;
			m_LastTimeAircraftDamageReported = currentTime;
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity ProcessAnimEvents
// ----------------------------------------------------------------
void audVehicleAudioEntity::ProcessAnimEvents()
{
	for(int i=0; i< AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		if(m_AnimEvents[i])
		{
			if(!IsDisabled())
			{
				for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
				{
					if(m_VehicleGadgets[loop]->HandleAnimationTrigger(m_AnimEvents[i]))
					{
						m_AnimEvents[i] = 0;
						break;
					}
				}
			}
			else
			{
				m_AnimEvents[i] = 0;
			}
		}
	}

	naAudioEntity::ProcessAnimEvents();
}

// ----------------------------------------------------------------
// Populate any variables we need for the LOD calculations
// ----------------------------------------------------------------
void audVehicleAudioEntity::PopulateVehicleLODVariables(audVehicleVariables& vehicleVariables)
{
	f32 fwdSpeed = DotProduct(m_CachedVehicleVelocity, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB()));
	vehicleVariables.fwdSpeed = fwdSpeed;
	vehicleVariables.fwdSpeedAbs = Abs(fwdSpeed);
	f32 invDriveMaxVelocity = m_Vehicle->m_Transmission.GetDriveMaxVelocity() > 0.0f ? 1.0f/m_Vehicle->m_Transmission.GetDriveMaxVelocity() : 0.0f;

	vehicleVariables.fwdSpeedRatio = Min(1.f, vehicleVariables.fwdSpeedAbs * invDriveMaxVelocity);
	vehicleVariables.invDriveMaxVelocity = invDriveMaxVelocity;
	
	const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(audNorthAudioEngine::GetMicrophones().IsTinyRacersMicrophoneActive() ? 1 : 0);
	Vec3V vehPos = m_Vehicle->GetTransform().GetPosition();	

	if (GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
	{
		vehPos = ComputeClosestPositionOnVehicleYAxis();
	}	

	const Vec3V dirToVeh = vehPos - listenerPos; 
	vehicleVariables.distFromListenerSq = static_cast<u32>(MagSquared(dirToVeh).Getf());
	vehicleVariables.visibleBySniper = audNorthAudioEngine::GetMicrophones().IsVisibleBySniper(m_Vehicle);
}

// ----------------------------------------------------------------
// audVehicleAudioEntity::ComputeClosestPositionOnVehicleYAxis
// ----------------------------------------------------------------
Vec3V_Out audVehicleAudioEntity::ComputeClosestPositionOnVehicleYAxis()
{
	const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(audNorthAudioEngine::GetMicrophones().IsTinyRacersMicrophoneActive() ? 1 : 0);
	const Vec3V vehPos = m_Vehicle->GetTransform().GetPosition();	

	Vector3 closestPoint;
	const Vector3 forward = VEC3V_TO_VECTOR3(m_Vehicle->GetMatrix().b());
	fwGeom::fwLine positioningLine = fwGeom::fwLine(VEC3V_TO_VECTOR3(vehPos) + (forward * m_Vehicle->GetBoundingBoxMin().GetY()), VEC3V_TO_VECTOR3(vehPos) + (forward * m_Vehicle->GetBoundingBoxMax().GetY()));
	positioningLine.FindClosestPointOnLine(VEC3V_TO_VECTOR3(listenerPos), closestPoint);
	return VECTOR3_TO_VEC3V(closestPoint);
}

// ----------------------------------------------------------------
// Populate the rest of the vehicle variables once we've worked out what LOD we are
// ----------------------------------------------------------------
void audVehicleAudioEntity::PopulateVehicleVariables(audVehicleVariables& vehicleVariables)
{
	vehicleVariables.hasMaterialChanged = false;
	vehicleVariables.forcedThrottle = -1.0f;
	vehicleVariables.granularEnginePitchOffset = 0;
	vehicleVariables.wetRoadSkidModifier = 0.f;
	vehicleVariables.movementSpeed = 0.0f;
	vehicleVariables.timeInMs = fwTimer::GetTimeInMilliseconds();

	if(m_IsFocusVehicle)
	{
		if(audNorthAudioEngine::IsRenderingHoodMountedVehicleCam())
		{
			vehicleVariables.isInHoodMountedCam = true;
		}
		if(audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
		{
			vehicleVariables.isInFirstPersonCam = true;
		}
	}

	// LOD is low enough that we're not going to be playing road noise, so just set some sensible defaults
	if(m_VehicleLOD >= AUD_VEHICLE_LOD_SUPER_DUMMY)
	{
		vehicleVariables.wheelSpeed = 0.0f;
		vehicleVariables.wheelSpeedAbs = 0.0f;
		vehicleVariables.wheelSpeedRatio = 0.0f;
		vehicleVariables.roadNoisePitch = 0;
		vehicleVariables.numWheelsTouchingFactor = 1.0f;
		vehicleVariables.numWheelsTouching = m_Vehicle->GetNumWheels();
		vehicleVariables.numDriveWheelsInWaterFactor = 0.0f;
		vehicleVariables.numDriveWheelsInShallowWaterFactor = 0.0f;
		m_LastTimeOnGround = vehicleVariables.timeInMs;
	}
	else
	{
		//Calculate speed variable for all vehicles
		u32 numValidWheels = 0;
		u32 numValidDriveWheels = 0;
		f32 sum = 0.0f;
		u32 numTouching = 0;
		u32 numNonBurstTouching = 0;
		u32 numFrontWheelsTouching = 0;
		u32 numDriveWheelsTouchingWater = 0;
		u32 numDriveWheelsTouchingShallowWater = 0;
		u32 numWheels = m_Vehicle->GetNumWheels();

		for(s32 i = 0; i < numWheels; i++)
		{
			CWheel* wheel = m_Vehicle->GetWheel(i);

			if(wheel)
			{
				s32 wheelId = wheel->GetHierarchyId();

				if(wheelId >= VEH_WHEEL_FIRST_WHEEL && wheelId < VEH_WHEEL_FIRST_WHEEL + 4)
				{
					numValidWheels++;
					f32 groundSpeed = wheel->GetGroundSpeed();
					sum += groundSpeed;

					if(wheel->GetIsTouching())
					{
						numTouching++;

						if(wheel->GetTyreHealth() > 0.0f)
						{
							numNonBurstTouching++;
						}

						if(wheelId == VEH_WHEEL_LF || wheelId == VEH_WHEEL_RF)
						{
							numFrontWheelsTouching++;
						}
					}

					if(wheel->GetIsDriveWheel())
					{
						numValidDriveWheels++;

						if(wheel->GetDynamicFlags().IsFlagSet(WF_INSHALLOWWATER))
						{
							numDriveWheelsTouchingShallowWater++;

							if(wheel->GetDynamicFlags().IsFlagSet(WF_INDEEPWATER))
							{
								numDriveWheelsTouchingWater++;
							}
						}
					}
				}
			}
		}

		f32 invValidWheels = numValidWheels > 0? 1.0f/numValidWheels : 0;
		f32 invValidDriveWheels = numValidDriveWheels > 0? 1.0f/numValidDriveWheels : 0;
		vehicleVariables.wheelSpeed = sum * invValidWheels;
		vehicleVariables.wheelSpeedAbs = Abs(vehicleVariables.wheelSpeed);
		vehicleVariables.wheelSpeedRatio = Min(1.f, vehicleVariables.wheelSpeedAbs * vehicleVariables.invDriveMaxVelocity);
		vehicleVariables.roadNoisePitch = (s32)sm_RoadNoisePitchCurve.CalculateValue(vehicleVariables.wheelSpeedAbs);
		vehicleVariables.movementSpeed = m_CachedVehicleVelocity.Mag();
		vehicleVariables.numNonBurstWheelsTouching = numNonBurstTouching;

		// Prevent underwater skids
		if(!m_Vehicle->m_nVehicleFlags.bIsDrowning)
		{
			vehicleVariables.numWheelsTouchingFactor = numTouching * invValidWheels;
			vehicleVariables.numWheelsTouching = numTouching;
		}
		else
		{
			vehicleVariables.numWheelsTouchingFactor = 0.0f;
			vehicleVariables.numWheelsTouching = 0;
		}

		vehicleVariables.numDriveWheelsInShallowWaterFactor = numDriveWheelsTouchingShallowWater * invValidDriveWheels;
		vehicleVariables.numDriveWheelsInWaterFactor = numDriveWheelsTouchingWater * invValidDriveWheels;

		if(vehicleVariables.numWheelsTouching > 0)
		{
			m_LastTimeOnGround = vehicleVariables.timeInMs;
		}
		else
		{
			m_LastTimeInAir = vehicleVariables.timeInMs;
		}

		if(m_Vehicle->GetNumWheels() == 4 && vehicleVariables.numWheelsTouching == 2 && numFrontWheelsTouching == 1)
		{
			m_LastTimeTwoWheeling = vehicleVariables.timeInMs;
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity InitVehicle
// ----------------------------------------------------------------
bool audVehicleAudioEntity::InitVehicle()
{
	// This needs to be done before InitVehicleSpecific in case any vehicle types rely on engine/exhaust positions for initializing volume cones
	CalculateEngineExhaustPositions();

	if(InitVehicleSpecific())
	{
		if(m_Vehicle->GetNumWheels() > 0)
		{
			Vector3 frontPos, backPos;
			GetWheelPosition(0,frontPos);
			GetWheelPosition(m_Vehicle->GetNumWheels()-1,backPos);
			m_DistanceBetweenWheels = frontPos.Dist(backPos);
			m_LastRoadPosFront.x = frontPos.x;
			m_LastRoadPosFront.y = frontPos.y;
			m_LastRoadPosBack.x = backPos.x;
			m_LastRoadPosBack.y = backPos.y;
		}

		m_ClatterOffsetPos = Vector3(0.0f, -m_Vehicle->GetBoundingBoxMax().GetY() * 0.5f, 0.0f);
		m_LastClatterWorldPos = m_Vehicle->TransformIntoWorldSpace(m_ClatterOffsetPos);
		m_LastFreewayBumpBack = m_LastFreewayBumpFront = fwTimer::GetTimeInMilliseconds();

		if (IsGoKart())
		{
			m_SkidPitchOffset = 800;
		}

		if(m_FakeEngineHealth == -1)
		{
			// Default to full health
			m_FakeEngineHealth = 1000;
			m_FakeBodyHealth = 1000;

			switch(GetRandomDamageClass())
			{
			case RANDOM_DAMAGE_ALWAYS:
				m_FakeEngineHealth = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(200.0f, 400.0f));
				m_FakeBodyHealth = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(200.0f, 400.0f));;
				break;
			case RANDOM_DAMAGE_WORKHORSE:
				if(audEngineUtil::ResolveProbability(0.5f))
				{
					m_FakeEngineHealth = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(400.0f, 1000.0f));
					m_FakeBodyHealth = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(400.0f, 1000.0f));;
				}
				break;
			case RANDOM_DAMAGE_OCCASIONAL:
				{
					if(audEngineUtil::ResolveProbability(0.1f))
					{
						m_FakeEngineHealth = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(700.0f, 1000.0f));
						m_FakeBodyHealth = static_cast<s16>(audEngineUtil::GetRandomNumberInRange(700.0f, 1000.0f));;
					}
				}
				break;
			case RANDOM_DAMAGE_NONE:
			default:
				break;
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity InitSubmersible
// ----------------------------------------------------------------
void audVehicleAudioEntity::InitSubmersible(BoatAudioSettings* boatSettings)
{
	m_SubRevsToSweetenerVolume.Init(boatSettings->TurningToSweetenerVolume);
	m_SubTurningToSweetenerVolume.Init(boatSettings->TurningToSweetenerVolume);
	m_SubTurningToSweetenerPitch.Init(boatSettings->TurningToSweetenerPitch);
	m_SubTurningEnginePitchModifier.Init(boatSettings->SubTurningEnginePitchModifier);
	m_SubRevsToSweetenerVolume.Init(boatSettings->RevsToSweetenerVolume);
	m_SubDiveVelocitySmoother.Init(0.02f, 0.01f, true);
	m_SubAngularVelocityVolumeSmoother.Init(0.1f, 0.05f, true);
}

#if __BANK
// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateDebug
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateDebug()
{
#if NA_RADIO_ENABLED
	if(g_ShowRadioGenres)
	{
		RadioGenre g1 = RADIO_GENRE_UNSPECIFIED, g2 = RADIO_GENRE_UNSPECIFIED;
		s32 ig1 = 0;
		s32 ig2 = 0;

		if(m_Vehicle->GetDriver())
		{
			m_Vehicle->GetDriver()->GetPedRadioCategory(ig1, ig2);
			if(ig1 != -1)
			{
				g1 = (RadioGenre)ig1;
			}
			if(ig2 != -1)
			{
				g2 = (RadioGenre)ig2;
			}
		}
		

		char buf[256];

		const char *stationName = m_RadioStation? m_RadioStation->GetName() : "NULL";
		formatf(buf, "%s\nped1: %s\nped2: %s\nveh: %s\nactual: %s", stationName, RadioGenre_ToString(g1), RadioGenre_ToString(g2), RadioGenre_ToString(m_RadioGenre), m_RadioStation? RadioGenre_ToString(m_RadioStation->GetGenre()) : "NULL");
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), m_RadioStation? Color32(255,255,255,255) : Color32(255,255,255,96), buf);
	}
#endif

	if(g_AudioDebugEntity == m_Vehicle)
	{
		if (m_EnvironmentGroup)
		{
			m_EnvironmentGroup->SetAsDebugEnvironmentGroup(true);
		}

#if __DEV
		SetAsDebugEntity(true);
#endif

		if(g_BreakOnVehicleUpdate)
		{
			__debugbreak();
		}
	}
	else
	{
		if (m_EnvironmentGroup)
		{
			m_EnvironmentGroup->SetAsDebugEnvironmentGroup(false);
		}

#if __DEV
		SetAsDebugEntity(false);
#endif
	}

	if(g_ShowFocusVehicle)
	{
		if(m_IsFocusVehicle)
		{
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, Color32(0,255,0,128));
		}
	}

	if(g_ShowEngineStates)
	{
		Color32 colour;

		switch(m_VehicleLOD)
		{
		case AUD_VEHICLE_LOD_DUMMY:
			colour = Color32(0,0,255,128);
			break;
		case AUD_VEHICLE_LOD_DISABLED:
			colour = Color32(255,0,0,128);
			break;
		case AUD_VEHICLE_LOD_SUPER_DUMMY:
			colour = Color32(255,255,255,128);
			break;
		default:
			colour = Color32(0,255,0,128);
			break;
		}

		Vector3 vehiclePos = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
		grcDebugDraw::Sphere(vehiclePos, 3.f, colour);

		switch(m_PrevDesiredLOD)
		{
		case AUD_VEHICLE_LOD_DUMMY:
			colour = Color32(0,0,255,128);
			break;
		case AUD_VEHICLE_LOD_DISABLED:
			colour = Color32(255,0,0,128);
			break;
		case AUD_VEHICLE_LOD_SUPER_DUMMY:
			colour = Color32(255,255,255,128);
			break;
		default:
			colour = Color32(0,255,0,128);
			break;
		}

		vehiclePos.SetZ(vehiclePos.GetZ() + 3.5f);
		grcDebugDraw::Sphere(vehiclePos, 0.5f, colour);
	}

	if(g_DrawLinesToVehiclesWithWaveSlots)
	{
		if(GetWaveSlot())
		{
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), Color32(255,0,0), Color32(255,0,0));
		}
	}

	if(g_DrawLinesToVehiclesWithSubmixVoices)
	{
		audSound* engineEffectSubmixSound = GetEngineEffectSubmixSound();
		audSound* exhaustEffectSubmixSound = GetExhaustEffectSubmixSound();

		if(engineEffectSubmixSound && exhaustEffectSubmixSound && engineEffectSubmixSound->GetPlayState() == AUD_SOUND_PLAYING && exhaustEffectSubmixSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), Color32(0,0,255), Color32(0,0,255));
		}
	}

	if(g_DrawLinesToRealCars)
	{
		if(m_VehicleType == AUD_VEHICLE_CAR && m_VehicleLOD == AUD_VEHICLE_LOD_REAL)
		{
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), Color32(0,255,0), Color32(0,255,0));
		}
	}

	if(g_DrawVehiclesInModShop)
	{		
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 2.f, m_InCarModShop? Color32(0,255,0,128) : Color32(255,0,0,128));
	}

	if(g_ShowVehiclesWithWaveSlots)
	{
		Color32 colour;

		if(GetWaveSlot())
		{
			colour = Color32(0,255,0,128);
		}
		else
		{
			colour = Color32(255,0,0,128);
		}

		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
	}

	if(g_ShowEngineExhaust)
	{
		Vec3V pos;
		pos = m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos);
		grcDebugDraw::Sphere(pos, 0.5f, Color32(255,0,0));
		pos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
		grcDebugDraw::Sphere(pos, 0.5f, Color32(0,0,255));
	}

	if(g_ShowNPCRoadNoiseState)
	{
		Color32 colour = Color32(255,0,0,(s32)(255.0f * (1.0f - m_PrevNPCRoadNoiseAttenuation)));
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), 3.f, colour);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), colour, colour);
	}
}
#endif

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateDrowning
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateDrowning(audVehicleVariables& params)
{
	bool canDrown = true;
	f32 maxDepthFactor = 0.4f;
	bool isSubmersible = IsSubmersible();

	if(m_VehicleType == AUD_VEHICLE_BOAT || m_Vehicle->InheritsFromAmphibiousAutomobile() || isSubmersible)
	{
		if(isSubmersible)
		{
			maxDepthFactor = 1.25f;
		}
		else 
		{
			canDrown = false;
		}
	}

	if(canDrown || isSubmersible)
	{
		f32 engineDepthFactor = 0.0f;

#if __BANK
		if(g_SimulateDrowning)
		{
			engineDepthFactor = 1.0f;
			m_EngineWaterDepth = 0.0f;
		}
		else 
#endif
			if(m_Vehicle->m_nFlags.bPossiblyTouchesWater && !m_Vehicle->m_nFlags.bInMloRoom)
			{
				f32 waterZ = 0.f;
				Vector3 vRiverHitPos, vRiverHitNormal;
				Vector3 buoyancyProbePos = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
				bool inRiver = m_Vehicle->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal) && (vRiverHitPos.z+REJECTIONABOVEWATER) > buoyancyProbePos.z;
				Vector3 enginePos;

				// If we're in a river, then we have to calculate the height at the buoyancy probe position, otherwise we can get inaccurate water height results
				// when sitting on a riverbed at a steep angle
				if(inRiver)
				{
					enginePos.z = buoyancyProbePos.z + m_EngineOffsetPos.GetZf();
					waterZ = vRiverHitPos.z;
				}
				// If we're in standard water then we can query the water level directly
				else
				{
					enginePos = VEC3V_TO_VECTOR3(m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos));
					Water::GetWaterLevel(enginePos, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
				}

				m_EngineWaterDepth = enginePos.z - waterZ;

				if(waterZ > enginePos.z)
				{
					engineDepthFactor = Clamp((waterZ - enginePos.z)/maxDepthFactor, 0.0f, 1.0f);

					if(engineDepthFactor >= 0.9f)
					{
						if(!m_PlayedDiveSound)
						{
							if (GetVehicleModelNameHash() != ATSTRINGHASH("Kosatka", 0x4FAF0D70))
							{
								TriggerWaterDiveSound();
							}
							
							m_PlayedDiveSound = true;
						}
					}
				}
			}
			else
			{
				m_EngineWaterDepth = 0.0f;
			}

			if(isSubmersible && Water::IsCameraUnderwater())
			{
				engineDepthFactor = 1.0f;
			}
			
			// Don't allow drowning factor to increase while the screen is faded
			if(!camInterface::IsFadedOut() || engineDepthFactor < m_DrowningFactor)
			{
				m_DrowningFactor = m_DrowningFactorSmoother.CalculateValue(engineDepthFactor, fwTimer::GetTimeInMilliseconds());
			}
			
			if(m_PlayedDiveSound)
			{
				if(engineDepthFactor <= 0.4f && !IsWaterDiveSoundPlaying())
				{
					m_PlayedDiveSound = false;
				}
			}
	}	

	if(m_VehicleType != AUD_VEHICLE_BOAT && !IsInAmphibiousBoatMode())
	{
		if(m_DrowningFactor > 0.0f && m_EngineWaterDepth > -0.7f) 
		{
			if(!m_WaterLappingSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				CreateAndPlaySound_Persistent(sm_ExtrasSoundSet.Find(ATSTRINGHASH("CarWaterLappingLoop", 0xF60AB708)), &m_WaterLappingSound, &initParams);
			}

			if(m_WaterLappingSound)
			{
				f32 waterLappingVolLin = 1.0f - Min(params.fwdSpeedAbs/5.0f, 1.0f);
				m_WaterLappingSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(waterLappingVolLin));
			}
		}
		else if(m_WaterLappingSound)
		{
			m_WaterLappingSound->StopAndForget();
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdatePedStatus
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdatePedStatus(const u32 distFromListenerSquared)
{
	const CPed* localPlayerPed = NULL;
	
	if(g_PlayerSwitch.IsActive() && camInterface::GetGameplayDirector().GetSwitchHelper())
	{
		localPlayerPed = camInterface::GetGameplayDirector().GetSwitchHelper()->GetNewPed();
	}

	if(localPlayerPed == NULL)
	{
		localPlayerPed = FindPlayerPed();
	}

	CVehicle* localPlayerVeh = NULL;
	
	if(localPlayerPed && localPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		localPlayerVeh = localPlayerPed->GetMyVehicle();
	}

	CPed *vehicleDriver = m_Vehicle->GetDriver();

	if(vehicleDriver)
	{
		m_LastVehicleDriver = vehicleDriver;
	}

	bool wasPlayerVehicle = m_IsPlayerVehicle;
	bool wasFocusVehicle = m_IsFocusVehicle;

	m_IsPlayerSeatedInVehicle = false;
	m_IsPlayerVehicle = false;
	m_IsFocusVehicle = false;
	m_IsPlayerDrivingVehicle = false;

	// The player isn't yet in a vehicle, or they are and its this one, then update the status. Only bother doing this on nearby vehicles
	u32 distance = 900;
	if(m_Vehicle->GetIsAircraft()) // you can get much further away from the players aircrsft, especially large planes
	{
		distance = 50000;
	}

	if((!localPlayerVeh && distFromListenerSquared < distance) || localPlayerVeh == m_Vehicle)
	{
		for(u32 seatIndex = 0; seatIndex < m_Vehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
		{
			CPed* pedInSeat = m_Vehicle->GetSeatManager()->GetPedInSeat(seatIndex);

			if(pedInSeat)
			{
				if(pedInSeat == localPlayerPed)
				{
					bool isPedInCar = true;
					m_IsPlayerVehicle = true;
					m_IsFocusVehicle = true;

					// want to start fading early if the player is getting in a car, so treat the car as a player car immediately
					if(pedInSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
					{
						CTaskEnterVehicle *task = (CTaskEnterVehicle*)pedInSeat->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
						if(task && task->GetState() < CTaskEnterVehicle::State_EnterSeat)
						{
							isPedInCar = false;	
						}
					}
					else if(pedInSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
					{
						CTaskExitVehicle *task = (CTaskExitVehicle*)pedInSeat->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
						if(task && task->GetState() >= CTaskExitVehicle::State_ExitSeat)
						{
							isPedInCar = false;
						}
					}

					if(pedInSeat == m_Vehicle->GetSeatManager()->GetDriver())
					{
						m_IsPlayerDrivingVehicle = true;
					}

					m_IsPlayerSeatedInVehicle = isPedInCar;
					break;
				}
			}
		}
	}
	
	m_IsFocusVehicle |= (m_Vehicle == sm_PlayerPedTargetVehicle);
	m_IsFocusVehicle |= (m_Vehicle == localPlayerVeh);

#if __BANK
	if(g_ShowEngineStates && m_IsFocusVehicle)
	{
		char buf[256];

		formatf(buf, " -- Focus Vehicle -- \nTarget Vehicle: %s\nLocal Player Vehicle: %s\nPlayer Vehicle: %s\nSpectated: %s", 
			m_Vehicle == sm_PlayerPedTargetVehicle? "true" : "false",
			m_Vehicle == localPlayerVeh? "true" : "false",
			m_IsPlayerVehicle ? "true" : "false",
			sm_IsSpectatorModeActive && m_IsBeingNetworkSpectated? "true" : "false");

		grcDebugDraw::Text(GetPosition(), Color_white, buf);
	}

	bool isHighQualitySlotOwner = (m_VehicleType == AUD_VEHICLE_CAR && static_cast<audCarAudioEntity*>(this)->m_EngineWaveSlot == sm_HighQualityWaveSlotManager.GetWaveSlot(0));

	if(g_ShowEngineStates)
	{
		char buf[256];		

		if (m_IsFocusVehicle)
		{			
			f32 xCoord = 0.5f;
			f32 yCoord = 0.05f;
			formatf(buf, "Focus Vehicle: %s (%llu)", m_Vehicle->GetModelName(), this); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Engine State: %d", (u32)static_cast<audCarAudioEntity*>(this)->GetVehicleEngine()->GetState()); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Target Vehicle: %s", m_Vehicle == sm_PlayerPedTargetVehicle ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Local Player Vehicle: %s", m_Vehicle == localPlayerVeh ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Player Vehicle: %s", m_IsPlayerVehicle ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Spectated: %s", sm_IsSpectatorModeActive && m_IsBeingNetworkSpectated ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
		}

		if (isHighQualitySlotOwner)
		{
			f32 xCoord = 0.7f;
			f32 yCoord = 0.05f;
			formatf(buf, "High Quality Slot Owner: %s (%llu)", m_Vehicle->GetModelName(), this); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;			
			formatf(buf, "Engine State: %d", (u32)static_cast<audCarAudioEntity*>(this)->GetVehicleEngine()->GetState()); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;		
			formatf(buf, "Is Target Vehicle: %s", m_Vehicle == sm_PlayerPedTargetVehicle ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Local Player Vehicle: %s", m_Vehicle == localPlayerVeh ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Player Vehicle: %s", m_IsPlayerVehicle ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
			formatf(buf, "Is Spectated: %s", sm_IsSpectatorModeActive && m_IsBeingNetworkSpectated ? "true" : "false"); grcDebugDraw::Text(Vector2(xCoord, yCoord), Color_white, buf); yCoord += 0.02f;
		}
	}
#endif

	if(sm_IsSpectatorModeActive)
	{
		if(!m_IsBeingNetworkSpectated)
		{
			m_IsPlayerVehicle = false;
			m_IsFocusVehicle = false;
			m_IsPlayerSeatedInVehicle = false;
		}
		else
		{
			m_IsPlayerVehicle = true;
			m_IsFocusVehicle = true;
			m_IsPlayerSeatedInVehicle = true;
		}
	}
	
	// Track remote players getting into and out of vehicles so that we can 
	// attach synths to them as necessary
	if(NetworkInterface::IsGameInProgress() && m_Vehicle->ContainsPlayer())
	{
		if(!m_IsNetworkPlayerSeatedInVehicle)
		{
			m_IsNetworkPlayerSeatedInVehicle = true;

			if(!m_IsFocusVehicle && g_AllowSynthsOnNetworkVehicles)
			{
				m_ResetDSPEffects = true;
				m_ReapplyDSPPresets = true;
			}	
		}
	}
	else if(m_IsNetworkPlayerSeatedInVehicle)
	{
		m_IsNetworkPlayerSeatedInVehicle = false;

		if(!m_IsFocusVehicle && g_AllowSynthsOnNetworkVehicles)
		{
			m_ResetDSPEffects = true;
			m_ReapplyDSPPresets = true;
		}		
	}
	
#if __BANK
	if(g_TreatAsPedCar)
	{
		m_IsPlayerSeatedInVehicle = false;
		m_IsPlayerVehicle = false;
		m_IsFocusVehicle = false;
	}
#endif

	if(m_IsFocusVehicle != wasFocusVehicle)
	{
		OnFocusVehicleChanged();

		if(m_IsFocusVehicle && (m_IsPlayerVehicle || m_Vehicle == sm_PlayerPedTargetVehicle))
		{
			audDisplayf("Focus vehicle changed - vehicle engine is currently %s and playing radio station %s"
				, m_Vehicle->m_nVehicleFlags.bEngineOn? "ON" : "OFF"
				, m_RadioStation ? m_RadioStation->GetName() : "NULL"
				);
		}

		// Minitank has an NPC and player version of the track loop so we need to reset this when swapping
		if (GetVehicleModelNameHash() == ATSTRINGHASH("MINITANK", 0xB53C6C52) && m_CaterpillarTrackLoop)
		{
			m_CaterpillarTrackLoop->StopAndForget(true);
		}
	}
	
	// GTA V bug fix - check for vehicles being stuck at the wrong quality setting and reset them if necessary
	audVehicleEngine* vehicleEngine = GetVehicleEngine();

	if(vehicleEngine)
	{
		if(vehicleEngine->HasBeenInitialised() && (vehicleEngine->GetState() == audVehicleEngine::AUD_ENGINE_ON || vehicleEngine->GetState() == audVehicleEngine::AUD_ENGINE_STARTING))
		{
			const audGranularMix::audGrainPlayerQuality currentEngineQuality = vehicleEngine->GetGranularEngineQuality();

			if(currentEngineQuality != vehicleEngine->GetDesiredEngineQuality())
			{
				vehicleEngine->QualitySettingChanged();
			}
		}
	}
	// End GTA V bug fix

	if(m_IsPlayerVehicle)
	{
		g_ReflectionsAudioEntity.AttachToVehicle(this);
	}
	else if(m_IsPlayerVehicle != wasPlayerVehicle)
	{	
		g_ReflectionsAudioEntity.DetachFromVehicle(this);
	}

	if(localPlayerVeh && localPlayerVeh == m_Vehicle)
	{
		m_LastPlayerVehicle = true;
	}
	else if(localPlayerVeh)
	{
		m_LastPlayerVehicle = false;
	} 

	if (!localPlayerPed || localPlayerPed->IsDead())
	{
		m_IsSirenOn = false;
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity PreUpdateService
// ----------------------------------------------------------------
void audVehicleAudioEntity::PreUpdateService(u32 timeInMs)
{
	if (!g_AudioEngine.IsAudioEnabled() || m_ForcedGameObjectResetRequired)
	{
		return;
	}
	if (!m_Vehicle)
	{
		naErrorf("a vehicle audio entity has to have an owning vehicle");
		return;
	}

	if(m_IsWaitingToInit)
	{
		if(InitVehicle())
		{
			m_IsWaitingToInit = false;
		}
		else
		{
			return;
		}
	}

	m_CachedNonCameraRelativeOpenness = -1.0f;
	m_CachedCameraRelativeOpenness = -1.0f;

	m_DistanceFromListener2LastFrame = m_DistanceFromListener2;
	m_DistanceFromListener2 = static_cast<u32>(MagSquared(Subtract(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(), m_Vehicle->GetVehiclePosition())).Getf());
	m_Vehicle->GetPlaceableTracker()->CalculateOrientation();
	UpdatePedStatus(m_DistanceFromListener2);

	m_DspSettings.ClearDSPParameters();
	m_DspSettings.timeInMs = timeInMs;
	m_QuadrantDirty = true;

	audVehicleVariables vehicleVariables = {0};
	PopulateVehicleLODVariables(vehicleVariables);

	// Trains manage their own LOD system, since they don't take up any granular/submix/wave slots like the other vehicles
	if(m_VehicleType != AUD_VEHICLE_TRAIN && m_VehicleType != AUD_VEHICLE_TRAILER)
	{
		UpdateVehicleLOD(vehicleVariables.fwdSpeedRatio, vehicleVariables.distFromListenerSq, vehicleVariables.visibleBySniper);
	}

	PopulateVehicleVariables(vehicleVariables);

	if(IsSubmersible() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive())) //For replay CPacketSubUpdate updates this.
	{
		m_SubAngularVelocityMag = m_Vehicle->IsEngineOn()? m_SubAngularVelocitySmoother.CalculateValue(GetCachedAngularVelocity().Mag()) : 0.0f;
	}

	UpdateVehicleSpecific(vehicleVariables, m_DspSettings);

	if(IsReal())
	{
		UpdateDSPEffects(m_DspSettings, vehicleVariables);
		UpdateCabinTone(vehicleVariables);
	}

	if(IsReal() || IsDummy())
	{
		audVehicleWheelPositionCache wheelPosCache;
		vehicleVariables.wheelPosCache = &wheelPosCache;

		if(HasEntityVariableBlock())
		{
			if(m_Vehicle->pHandling)
			{
				SetEntityVariable(ATSTRINGHASH("mass", 0x4BC594CB), m_Vehicle->pHandling->m_fMass);
			}

			SetEntityVariable(ATSTRINGHASH("roofopenness", 0xB146F028), GetConvertibleRoofOpenness());
			SetEntityVariable(ATSTRINGHASH("Vehicle.HealthFactor", 0xD1F00E32), 1.f - ComputeEffectiveBodyDamageFactor());
		}

		UpdateClatter(vehicleVariables);
		UpdateRoadNoiseSounds(vehicleVariables);
		UpdateMissileLock();
		UpdateDamageWarning();
		UpdateHeadLights();
		UpdateWeaponBlades();

		if(m_Vehicle->HasParachute() || m_Vehicle->HasJump())
		{
			UpdateParachuteSound();
		}		

		if(m_IsPlayerVehicle)
		{
			UpdateAircraftWarningSpeech();
		}
	}
	else if(!IsDisabled())
	{
		if(m_VehicleType == AUD_VEHICLE_TRAILER && GetVehicleModelNameHash() == ATSTRINGHASH("TRAILERSMALL2", 0x8FD54EBB))
		{
			UpdateMissileLock();
		}

		 if(m_Vehicle->m_nVehicleFlags.bIsBeingTowed)
		 {
			UpdateClatter(vehicleVariables);
		 }
		 else if(m_VehicleClatter)
		 {
			 m_VehicleClatter->StopAndForget();
		 }
	}
#if GTA_REPLAY
	else
	{
		if(CReplayMgr::IsEditModeActive() && !CReplayMgr::IsJustPlaying() && m_Vehicle->HasJump())
		{
			if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
			{
				CAutomobile* automobile = static_cast<CAutomobile*>(m_Vehicle);
				m_Vehicle->SetJumpRechargeTimer(CVehicle::ms_fJumpRechargeTime);
				automobile->GetVehicleAudioEntity()->SetRechargeSoundStartOffset(0);
			}
		}
	}
#endif

	if(CGameWorld::GetMainPlayerInfo()->AreControlsDisabled())
	{
		StopAndForgetSounds(m_MissileApproachingSound, m_BeingTargetedSound, m_AcquiringTargetSound, m_MissileApproachingSoundImminent);
	}

	if(!IsDisabled())
	{
		for(s32 loop = 0; loop < m_VehicleGadgets.GetCount(); loop++)
		{
			m_VehicleGadgets[loop]->Update();
		}

		UpdateRadio();
		UpdateRain(vehicleVariables);
		UpdateDoors();
		UpdateSurfaceSettleSounds(vehicleVariables);
		UpdateDrowning(vehicleVariables);
		UpdateDetachedBonnetSound();
		UpdateWarningSounds(vehicleVariables);
		UpdateWaveImpactSounds(vehicleVariables);
		UpdateClothSounds(vehicleVariables);		
		m_VehicleFireAudio.Update();
		UpdateWater(vehicleVariables);
		UpdateLandingGear();
		UpdateWeaponCharge();

		if (m_Vehicle->HasJump())
		{
			UpdateVehicleJump();
		}
	}
	else
	{
		m_ShouldPlayLandingGearSound = false;
		m_ShouldStopLandingGearSound = false;
		m_InWaterTimer = 0.0f;
		m_OutOfWaterTimer = 0.0f;
		m_WasInWaterLastFrame = true;
	}

#if __BANK
	UpdateDebug();
#endif

	if (m_Vehicle->UsesSiren() && NetworkInterface::IsInCopsAndCrooks() && !m_IsSirenFucked)
	{
		const f32 damageFactor = ComputeEffectiveBodyDamageFactor();

		if (damageFactor > g_CopsAndCrooksForceSirenSmashDamage && ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), g_CopsAndCrooksForceSirenSmashProbability))
		{
			SmashSiren(true);
		}
	}
	
	UpdateSpecialFlightModeTransformSounds();
	UpdateCarRecordings();
	ProcessAnimEvents();
	UpdateEnvironment();

	if(m_HasToTriggerAlarm)
	{
		m_Vehicle->TriggerCarAlarm();
		m_HasToTriggerAlarm = false;
	}

	// Updating these except on disabled vehicles,  
	if(GetVehicleLOD() < AUD_VEHICLE_LOD_DISABLED || m_IsScriptTriggeredHorn)
	{
		UpdateHorn();
	}
	else
	{
		if(m_HornSound)
		{
			m_HornSound->StopAndForget();
		}

		m_IsScriptTriggeredHorn = false;
		m_IsHornPermanentlyOn = false;
		m_WasHornPermanentlyOn = false;
	}

	UpdateSiren(vehicleVariables);
	m_TimeSinceLastBulletHit += fwTimer::GetTimeStepInMilliseconds();
	if(m_TimeSinceLastBulletHit >= SUPERCONDUCTOR.GetGunFightConductor().GetFakeScenesConductor().GetTimeToResetBulletCount())
	{
		m_BulletHitCount = 0;
	}

	// B*3051438 - Erroneous collisions triggering when rotating the vehicle at low framerates - now scaled by timestep when running below 30fps
	const f32 speedDelta = (m_FwdSpeedLastFrame - vehicleVariables.fwdSpeed) * Max(30.0f, fwTimer::GetInvTimeStep());

	if(speedDelta >= sm_VehDeltaSpeedForCollision && !m_IsVehicleBeingStopped)
	{
		m_CollisionAudio.Fakeimpact(speedDelta, m_CollisionAudio.GetVehicleCollisionSettings()->FakeImpactScale);
	}

	if(!GetEnvironmentGroup())
	{
		InitOcclusion();
	}

	if(m_Vehicle->ContainsLocalPlayer() || (sm_IsSpectatorModeActive && m_IsBeingNetworkSpectated))
	{
		// Send message to the superconductor to stop the QUITE_SCENE
		if(audSuperConductor::sm_StopQSOnPlayerInVehicle && m_Vehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
		{
			ConductorMessageData message;
			message.conductorName = SuperConductor;
			message.message = StopQuietScene;
			message.bExtraInfo = false;
			message.vExtraInfo = Vector3((f32)audSuperConductor::sm_PlayerInVehicleQSFadeOutTime
										,(f32)audSuperConductor::sm_PlayerInVehicleQSDelayTime
										,(f32)audSuperConductor::sm_PlayerInVehicleQSFadeInTime);
			SUPERCONDUCTOR.SendConductorMessage(message);
		}

		if(GetEnvironmentGroup())
		{
			CreateEnvironmentGroup("VehicleMixGroup",PLAYER_MIXGROUP);
			if(naVerifyf(GetEnvironmentGroup(), "Failed to create environment group for local player vehicle"))
			{
				int mixGroupIdx = GetEnvironmentGroup()->GetMixGroupIndex();
				if(m_PlayerVehicleMixGroupIndex < 0)
				{
					if(mixGroupIdx < 0)
					{
						DYNAMICMIXER.AddEntityToMixGroup(m_Vehicle, ATSTRINGHASH("PLAYER_GROUP", 0xC5FA40AC));
						m_PlayerVehicleMixGroupIndex = GetEnvironmentGroup()->GetMixGroupIndex();
					}

				}
				else if(mixGroupIdx != m_PlayerVehicleMixGroupIndex)
				{
					m_PlayerVehicleMixGroupIndex = -1;
					RemoveEnvironmentGroup(PLAYER_MIXGROUP);
				}
			}
		}
	}
	else if(m_PlayerVehicleMixGroupIndex >= 0 && GetEnvironmentGroup())
	{
		int mixGroupIdx = GetEnvironmentGroup()->GetMixGroupIndex();
		if(m_PlayerVehicleMixGroupIndex == mixGroupIdx)
		{
			DYNAMICMIXER.RemoveEntityFromMixGroup(m_Vehicle);
		}
		m_PlayerVehicleMixGroupIndex = -1;
		RemoveEnvironmentGroup(PLAYER_MIXGROUP);
	}

	if( GetEnvironmentGroup() )
	{
		m_VehicleUnderCover = ((naEnvironmentGroup*)GetEnvironmentGroup())->IsUnderCover();
	}
	else
	{
		m_VehicleUnderCover = false;
	}

	// Extra safety check - ensure we kill stunt race scenes once we leave stunt race mode (they should be one-shots anyway)
	if(!SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() || !NetworkInterface::IsGameInProgress())
	{
		if(sm_StuntRaceSpeedUpScene)
		{
			sm_StuntRaceSpeedUpScene->Stop();
			sm_StuntRaceSpeedUpScene = NULL;
		}

		if(sm_StuntRaceSlowDownScene)
		{
			sm_StuntRaceSlowDownScene->Stop();
			sm_StuntRaceSlowDownScene = NULL;
		}
	}

	if(g_ReflectionsAudioEntity.IsFocusVehicleInStuntTunnel() && g_ReflectionsAudioEntity.GetActiveVehicle() == this)
	{
		if(!m_StuntTunnelSound)
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			CreateAndPlaySound_Persistent(ATSTRINGHASH("DLC_Stunt_Tunnel_Interior_Sound", 0x9A98EAAA), &m_StuntTunnelSound, &initParams);
		}
	}
	else if(m_StuntTunnelSound)
	{
		m_StuntTunnelSound->StopAndForget();
	}

	m_WasEngineOnLastFrame = m_Vehicle->m_nVehicleFlags.bEngineOn;
	m_FwdSpeedLastFrame = vehicleVariables.fwdSpeed;
	m_WasInInteriorViewLastFrame = vehicleVariables.isInFirstPersonCam;
	m_WasOnStairs = m_OnStairs;
	m_OnStairs = false;
}


// ----------------------------------------------------------------
// audVehicleAudioEntity::RequiresWaveSlot
// ----------------------------------------------------------------
bool audVehicleAudioEntity::RequiresWaveSlot() const
{ 
	return m_Vehicle->m_nVehicleFlags.bEngineOn; 
}

// ----------------------------------------------------------------
// Get the desired lod for the vehicle
// ----------------------------------------------------------------
audVehicleLOD audVehicleAudioEntity::GetDesiredLOD(f32 UNUSED_PARAM(fwdSpeedRatio), u32 UNUSED_PARAM(distFromListenerSq), bool UNUSED_PARAM(visibleBySniper))
{
	if(m_VehicleType != AUD_VEHICLE_NONE)
	{
		if(m_IsFocusVehicle || RequiresWaveSlot())
		{
			return AUD_VEHICLE_LOD_REAL;
		}
	}
	
	return AUD_VEHICLE_LOD_DISABLED;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity GetDistance2FromSniperAimVector
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::GetDistance2FromSniperAimVector() const
{
	const Vector3 vehiclePosition = VEC3V_TO_VECTOR3(GetPosition());
	const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();

	Vector3 vStart, vShot, vEnd;
	vStart = aimFrame.GetPosition();
	vShot  = aimFrame.GetFront();
	vEnd = vStart + vShot * 3000.0f;

	fwGeom::fwLine positioningLine;
	positioningLine.m_start = vStart;
	positioningLine.m_end = vEnd;

	Vector3 closestPoint;
	positioningLine.FindClosestPointOnLine(vehiclePosition, closestPoint);

	// Scale the contribution up as the view zooms in
	const f32 currentFOV = camInterface::GetFov();
	const f32 sniperContribution = audCurveRepository::GetLinearInterpolatedValue(2.0f, 0.5f, 7.5f, 45.0f, currentFOV);

	// Distance divided by constribution (so sniper view vehicles appear to be closer as you zoom in)
	return (u32)(closestPoint.Dist2(vehiclePosition)/sniperContribution);
}

// ----------------------------------------------------------------
// audVehicleAudioEntity GetVehicleHydraulicsSoundSetName
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::GetVehicleHydraulicsSoundSetName() const
{
	if(m_Vehicle && m_Vehicle->GetVehicleModelInfo() && m_Vehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS))
	{
		return (m_IsFocusVehicle BANK_ONLY(&& !g_ForceNPCHydraulicSounds)) ? ATSTRINGHASH("HYDRAULIC_SUSPENSION_DONK_SS", 0x34143DB1) : ATSTRINGHASH("HYDRAULIC_SUSPENSION_DONK_NPC_SS", 0x370AA4FE);
	}
	else
	{
		return (m_IsFocusVehicle BANK_ONLY(&& !g_ForceNPCHydraulicSounds)) ? ATSTRINGHASH("HYDRAULIC_SUSPENSION_SS", 0x228F3C64) : ATSTRINGHASH("HYDRAULIC_SUSPENSION_NPC_SS", 0x6B83A0FE);
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity ShouldForceEngineOff
// ----------------------------------------------------------------
bool audVehicleAudioEntity::ShouldForceEngineOff() const
{	
	if(IsSubmarineCar())
	{
		CSubmarineCar* submarineCar = (CSubmarineCar*)m_Vehicle;

		if(submarineCar->IsInSubmarineMode() ||
		   submarineCar->GetAnimationState() == CSubmarineCar::State_StartToSubmarine ||
		   submarineCar->GetAnimationState() == CSubmarineCar::State_MoveWheelCoversOut ||
		   submarineCar->GetAnimationState() == CSubmarineCar::State_FinishToSub)
		{
			return true;
		}
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB))
	{
		if(m_Vehicle->GetSpecialFlightModeRatio() > 0.f)
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity IsHiddenByNetwork
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsHiddenByNetwork() const
{
	if(m_Vehicle && NetworkInterface::IsGameInProgress())
	{
		if(NetworkInterface::IsEntityConcealed(*m_Vehicle))
		{
			return true;
		}
		else
		{
			if(NetworkInterface::IsInMPCutscene() && m_Vehicle->IsNetworkClone())
			{
				if(m_Vehicle->GetNetworkObject() && !m_Vehicle->GetNetworkObject()->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE))
				{
					return true;
				}
			}
		}
	}

	return false;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateVehicleLOD
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateVehicleLOD(f32 fwdSpeedRatio, u32 distanceFromListenerSq, bool visibleBySniper)
{

	if(visibleBySniper)
	{
		distanceFromListenerSq = Min(distanceFromListenerSq, GetDistance2FromSniperAimVector());
	}

	// Remember whether we're within the standard dummy radius scale. This is used in several places to determine whether to play one-shot sounds than need to play
	// on nearby vehicles regardless of the actual dummy/active/disabled state of the car
	bool isSuperDummyValid = distanceFromListenerSq < g_SuperDummyRadiusSq;

	if(IsDummy())
	{
		PF_INCREMENT(NumDummyVehicles);
		sm_NumDummyVehiclesCounter++;
		audAssertf(m_VehicleType == AUD_VEHICLE_CAR, "Only cars should be using dummy LOD");
	}
	else if(IsReal())
	{
		PF_INCREMENT(NumRealVehicles);
	}
	else if(IsDisabled())
	{
		PF_INCREMENT(NumDisabledVehicles);
	}
	else if(IsSuperDummy())
	{
		PF_INCREMENT(NumSuperDummyVehicles);
	}

	audVehicleLOD desiredLOD;

	if(IsHiddenByNetwork())
	{	
		desiredLOD = AUD_VEHICLE_LOD_DISABLED;
		isSuperDummyValid = false;
	}
	else if(m_ScriptPriority == AUD_VEHICLE_SCRIPT_PRIORITY_MAX && (fwdSpeedRatio > 0.0f || m_Vehicle->GetDriver()))
	{
		desiredLOD = AUD_VEHICLE_LOD_REAL;
	}
	else if(CScriptHud::bUsingMissionCreator && !m_Vehicle->IsCollisionEnabled())
	{
		desiredLOD = AUD_VEHICLE_LOD_DISABLED;
	}
	else
	{
		desiredLOD = GetDesiredLOD(fwdSpeedRatio, distanceFromListenerSq, visibleBySniper);

		if(visibleBySniper && desiredLOD <= AUD_VEHICLE_LOD_DUMMY)
		{
			m_Vehicle->m_nVehicleFlags.bRefreshVisibility = true;
			
			if(m_Vehicle->GetNumVisiblePixels() <= 10)
			{
				desiredLOD = GetDesiredLOD(fwdSpeedRatio, distanceFromListenerSq, false);
			}
		}
	}

	if(desiredLOD == AUD_VEHICLE_LOD_DISABLED && isSuperDummyValid)
	{
		desiredLOD = AUD_VEHICLE_LOD_SUPER_DUMMY;
		m_ReapplyDSPEffects = false;
	}
	else if(!m_Vehicle->m_nVehicleFlags.bEngineOn)
	{
		// This was added to fix a case where the player can exit a car and re-enter without the car becoming a super dummy(not super dummy long enough) which is what would trigger the engine wave slot to be released.
		// When this happened the DSP wasn't reapplied so no DSP. This flag basically says we tried to transition to super dummy, so lets check if we need to reapply dsp.
		m_ReapplyDSPEffects = true;
		m_ResetDSPEffects = true;
		m_ReapplyDSPPresets = true;
	}

#if __BANK
	if(g_ForcePlayerVehicleLOD != AUD_VEHICLE_LOD_UNKNOWN && m_IsPlayerVehicle)
	{
		desiredLOD = (audVehicleLOD) g_ForcePlayerVehicleLOD;
	}
	else if(g_ForceRealLodOnFocusEntity && GetVehicle() == CDebugScene::FocusEntities_Get(0))
	{
		desiredLOD = AUD_VEHICLE_LOD_REAL;
	}
#endif

	bool priorityVehicle = m_IsFocusVehicle;

	if(desiredLOD == AUD_VEHICLE_LOD_REAL)
	{
		if(IsUsingGranularEngine())
		{
			sm_NumVehiclesInGranularActivationRange++;
		}
		
		sm_NumVehiclesInActivationRange++;

		// Mark this vehicle as using up two slots if we need one for an sfx bank. LOD activation range should then automatically shrink to accommodate this.
		if(m_RequiresSFXBank)
		{
			sm_NumVehiclesInActivationRange++;
		}
	}
	
	if(m_DummyLODValid)
	{
		audQuadrants vehicleQuadrant = GetVehicleQuadrant();
		sm_NumVehiclesInDummyRange[vehicleQuadrant]++;
	}

	if(desiredLOD == m_PrevDesiredLOD)
	{
		m_TimeAtDesiredLOD += fwTimer::GetTimeStep();
	}
	else
	{
		m_PrevDesiredLOD = desiredLOD;
		m_TimeAtDesiredLOD = 0.0f;
	}

	// This occurs when switching from high to low LOD engines - we free the wave slot then must re-grab it
	if(m_VehicleLOD == desiredLOD)
	{
		if(m_VehicleLOD == AUD_VEHICLE_LOD_REAL)
		{
			if(!m_EngineWaveSlot)
			{
				// url:bugstar:6110277 fix - Fix for personal vehicles (which remain at real lod even with the engine off) from re-grabbing a waveslot after the engine waveslot is released
				if (!(m_IsPersonalVehicle && !m_IsFocusVehicle && !m_Vehicle->m_nVehicleFlags.bEngineOn))
				{
					audAssertf(m_VehicleType == AUD_VEHICLE_NONE || (m_EngineSubmixIndex >= 0 && m_ExhaustSubmixIndex >= 0), "Vehicle does not yet have a submix, type %d", (u32)m_VehicleType);

					if (AcquireWaveSlot())
					{
						ConvertFromDummy();
					}
					else if (priorityVehicle)
					{
						SetPlayerVehicleStarving();
					}
				}
			}
		}
	}

	bool lodSwitchValid = false;	

	lodSwitchValid = (m_VehicleLOD != desiredLOD) && 
					 (m_TimeAtDesiredLOD > g_MinTimeAtDesiredLOD) && 
					 (m_TimeAtCurrentLOD >= g_MinTimeAtCurrentLOD || desiredLOD > m_VehicleLOD || priorityVehicle || IsPlayerVehicleStarving());

#if GTA_REPLAY
	// Allow for instant switching to a higher LOD on the focus vehicle in replays
	if(CReplayMgr::IsJustPlaying() && priorityVehicle && desiredLOD < m_VehicleLOD)
	{
		lodSwitchValid = true;
	}
#endif

	if(lodSwitchValid)
	{
		switch(m_VehicleLOD)
		{
		case AUD_VEHICLE_LOD_REAL:
			{
				if(desiredLOD == AUD_VEHICLE_LOD_DUMMY)
				{
					// Not enough dummy slots - disabled instead
					if(sm_NumDummyVehiclesLastFrame >= g_MaxDummyVehicles)
					{
						desiredLOD = AUD_VEHICLE_LOD_DISABLED;
					}
				}
			}
			break;

		case AUD_VEHICLE_LOD_DUMMY:
			{
				if(desiredLOD == AUD_VEHICLE_LOD_REAL)
				{
					// Dummy wants to be real, but player is starving so we need to handle that first
					if(IsPlayerVehicleStarving() && !priorityVehicle)
					{
						desiredLOD = m_VehicleLOD;
					}
				}
			}
			break;

		case AUD_VEHICLE_LOD_DISABLED:
		case AUD_VEHICLE_LOD_SUPER_DUMMY:
			{
				if(desiredLOD == AUD_VEHICLE_LOD_REAL)
				{
					// Disabled wants to be real, but player is starving so we need to handle that first
					if(IsPlayerVehicleStarving() && !priorityVehicle)
					{
						desiredLOD = m_VehicleLOD;
					}
				}
				else if(desiredLOD == AUD_VEHICLE_LOD_DUMMY)
				{
					// Disabled wants to be dummy, but not enough slots so remain where we are
					if(sm_NumDummyVehiclesLastFrame >= g_MaxDummyVehicles)
					{
						desiredLOD = m_VehicleLOD;
					}
				}
			}
			break;

		default:
			break;
		}

		if(desiredLOD == AUD_VEHICLE_LOD_REAL)
		{			
			audAssertf(!m_EngineWaveSlot, "Vehicle already has a wave slot, type %d", (u32)m_VehicleType);
			audAssertf(m_EngineSubmixIndex == -1 && m_ExhaustSubmixIndex == -1, "Vehicle already has a submix, type %d", (u32)m_VehicleType);

			bool success = false;

			if(!IsUsingGranularEngine() || CalculateNumVehiclesUsingSubmixes(AUD_VEHICLE_ANY, true, true) < g_MaxGranularEngines)
			{
				if(AcquireWaveSlot())
				{
					success = true;
				}
			}

			if(!success)
			{
				if(priorityVehicle)
				{
					SetPlayerVehicleStarving();
				}

				// Failed to become real - promote to dummy as second best best
				if(m_DummyLODValid && m_VehicleLOD > AUD_VEHICLE_LOD_DUMMY)
				{
					if(sm_NumDummyVehiclesLastFrame < g_MaxDummyVehicles)
					{
						desiredLOD = AUD_VEHICLE_LOD_DUMMY;
					}
					else
					{
						desiredLOD = AUD_VEHICLE_LOD_DISABLED;
					}
				}
				else
				{
					desiredLOD = m_VehicleLOD;
				}
			}
		}

		// Any disabled vehicles within the super dummy radius get promoted
		if(desiredLOD == AUD_VEHICLE_LOD_DISABLED && isSuperDummyValid)
		{
			desiredLOD = AUD_VEHICLE_LOD_SUPER_DUMMY;
		}

		SetLOD(desiredLOD);
	}

	m_TimeAtCurrentLOD += fwTimer::GetTimeStep();
}

// ----------------------------------------------------------------
// Check if the hydraulics system is active
// ----------------------------------------------------------------
bool audVehicleAudioEntity::AreHydraulicsActive() const
{
	bool hydraulicsActive = m_Vehicle->m_nVehicleFlags.bPlayerModifiedHydraulics || 
							m_Vehicle->m_nVehicleFlags.bAreHydraulicsAllRaised || 
							HydraulicsModifiedRecently();

	if(!hydraulicsActive)
	{
		if(m_Vehicle->InheritsFromAutomobile())
		{
			for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
			{
				CWheel* wheel = m_Vehicle->GetWheel(i);

				if(wheel)
				{
					if(wheel->GetSuspensionHydraulicState() == WHS_LOCK_UP_ALL || wheel->GetSuspensionHydraulicState() == WHS_LOCK_UP)
					{
						hydraulicsActive = true;
						break;
					}
				}
			}
		}
	}	
	
    return hydraulicsActive;
}

// ----------------------------------------------------------------
// Check if the hydraulics have been modified recently
// ----------------------------------------------------------------
bool audVehicleAudioEntity::HydraulicsModifiedRecently() const
{
    if(m_Vehicle->InheritsFromAutomobile())
    {
        // Allow a bit of leeway after the hydraulics disengage
        if(fwTimer::GetTimeInMilliseconds() - ((CAutomobile*)m_Vehicle)->GetTimeHydraulicsModified() < g_HydraulicsModifiedTimeoutMs || 
		   fwTimer::GetTimeInMilliseconds() - m_LastHydraulicSuspensionStateChangeTime < g_HydraulicsModifiedTimeoutMs)			
        {
            return true;
        }
    }

    return false;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateVehicleJump
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateVehicleJump()
{
	//if(m_Vehicle->InheritsFromAutomobile())
	{
		if(!m_Vehicle->IsJumpFullyCharged() && m_Vehicle->m_nVehicleFlags.bEngineOn)
		{
			if(!m_JumpRecharge)
			{
				const u32 vehicleModelNameHash = GetVehicleModelNameHash();
				u32 soundsetName = 0u;

				if (vehicleModelNameHash == ATSTRINGHASH("MINITANK", 0xB53C6C52))
				{
					soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("ch_vehicle_minitank_player_sounds", 0xCF2B0226) : ATSTRINGHASH("ch_vehicle_minitank_remote_sounds", 0x90AF5388);
				}
				else if (vehicleModelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD) || vehicleModelNameHash == ATSTRINGHASH("SCRAMJET", 0xD9F0503D))
				{
					soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
				}
				else
				{
					soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("DLC_AW_Mod_Vehicle_Player_Sounds", 0xD5676608) : ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_NPC_Sounds", 0x96866E58);
				}

				audSoundSet soundset;
				const u32 soundName = IsToyCar() ? ATSTRINGHASH("jump_rc_recharge", 0x70468D4A) : ATSTRINGHASH("recharge", 0xFB025813);
				
				if (soundset.Init(soundsetName))
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
#if GTA_REPLAY
					if (CReplayMgr::IsJustPlaying())
					{
						initParams.StartOffset = m_RechargeStartOffset;
					}
#endif
					CreateAndPlaySound_Persistent(soundset.Find(soundName), &m_JumpRecharge, &initParams);
				}
			}

			if(m_JumpRecharge)
			{
				f32 recharge = m_Vehicle->GetJumpRechargeFraction();
				m_JumpRecharge->FindAndSetVariableValue(ATSTRINGHASH("chargeRemaining", 0x44E9F02C), recharge);
			}
		}
		else if(m_JumpRecharge)
		{
			m_JumpRecharge->StopAndForget();
		}	
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateParachuteSound
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateParachuteSound()
{
	if(m_Vehicle->InheritsFromAutomobile())
	{
		REPLAY_ONLY(bool replayRecordStop = false;)

		if (m_Vehicle->HasParachute())
		{
			if (((CAutomobile*)m_Vehicle)->IsParachuting())
			{
				if (!m_ParachuteLoop)
				{
					const u32 vehicleModelNameHash = GetVehicleModelNameHash();

					if (vehicleModelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD))
					{
						audSoundSet soundset;
						const u32 soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
						const u32 soundName = ATSTRINGHASH("Descend", 0x3A770027);

						if (soundset.Init(soundsetName))
						{
							audSoundInitParams initParams;
							initParams.TrackEntityPosition = true;
							initParams.EnvironmentGroup = m_EnvironmentGroup;
							CreateAndPlaySound_Persistent(soundset.Find(soundName), &m_ParachuteLoop, &initParams);
						}
					}
				}
			}
			else if (m_ParachuteLoop)
			{
				m_ParachuteLoop->StopAndForget(true);
				// record a packet that will clear parachute state
				REPLAY_ONLY(replayRecordStop = true;)
			}
		}

#if GTA_REPLAY
		// we're doing this here because we can be parachuting, but not recharging, but we want to use the same packet
		if(CReplayMgr::ShouldRecord() && (m_JumpRecharge || m_ParachuteLoop || replayRecordStop))
		{
			u32 rechargePlayPosition = 0;
			if(m_JumpRecharge)
			{
				rechargePlayPosition = m_JumpRecharge->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			}
			f32 rechargeTimer = m_JumpRecharge ? (m_Vehicle)->GetJumpRechargeTimer() : CVehicle::ms_fJumpRechargeTime;
			CReplayMgr::RecordFx<CPacketVehicleJumpRechargeTimer>(CPacketVehicleJumpRechargeTimer(rechargeTimer, rechargePlayPosition, ((CAutomobile*)m_Vehicle)->GetParachuteState()), m_Vehicle);
		}
#endif
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity ToggleHeliWings
// ----------------------------------------------------------------
void audVehicleAudioEntity::ToggleHeliWings(bool deploy)
{
	if(GetVehicleModelNameHash() == ATSTRINGHASH("AKULA", 0x46699F47))
	{
		audSoundInitParams initParams;
		audSoundSet soundSet;

		if(m_Vehicle && soundSet.Init(ATSTRINGHASH("akula_sounds", 0x6CC7612D)))
		{
			const u32 soundHash = deploy? ATSTRINGHASH("gun_flaps_open", 0xFF8D5D1B) : ATSTRINGHASH("gun_flaps_close", 0xD1A0023);
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			CreateDeferredSound(soundSet.Find(soundHash), m_Vehicle, &initParams, true, true);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSet.GetNameHash(), soundHash, &initParams, m_Vehicle));
		}
	}	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerSubmarineDiveSound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSubmarineDiveSound(u32 soundHash)
{
	//Trigger offline or owner only
	if (!NetworkInterface::IsGameInProgress() || !m_Vehicle->IsNetworkClone())
	{
		audSoundSet soundSet;

		if (soundSet.Init(ATSTRINGHASH("DLC_Hei4_Kosatka_Sounds", 0xBDDCF962)))
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(soundSet.Find(soundHash), &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("DLC_Hei4_Kosatka_Sounds", 0xBDDCF962), soundHash, &initParams, m_Vehicle));

			if (NetworkInterface::IsGameInProgress() && m_Vehicle->GetNetworkObject() && !m_Vehicle->IsNetworkClone())
			{
				CPlaySoundEvent::Trigger(m_Vehicle, ATSTRINGHASH("DLC_Hei4_Kosatka_Sounds", 0xBDDCF962), soundHash, 250);
			}
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity SetBombBayDoorsOpen
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetBombBayDoorsOpen(bool open)
{
	audSoundInitParams initParams;
	audSoundSet bombBaySoundSet;

	if(m_Vehicle && bombBaySoundSet.Init(ATSTRINGHASH("DLC_SM_Bomb_Bay_Doors_Sounds", 0xC7A83A5C)))
	{
		const u32 soundHash = open? ATSTRINGHASH("open", 0x9C783318) : ATSTRINGHASH("close", 0x47C8D769);
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		CreateDeferredSound(bombBaySoundSet.Find(soundHash), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(bombBaySoundSet.GetNameHash(), soundHash, &initParams, m_Vehicle));
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateWeaponBlades
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWeaponBlades()
{	
	if (m_Vehicle->GetNumWeaponBlades())
	{
		const u32 timeMs = fwTimer::GetTimeInMilliseconds();

		for (u32 i = 0; i < CVehicle::MAX_NUM_WEAPON_BLADES; i++)
		{
			const s32 lastCollisionTime = (s32)m_LastWeaponBladeCollisionTime[i];

			if (m_IsWeaponBladeColliding[i])
			{
				const u32 soundSetName = m_IsPlayerVehicle ? ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_Player_Sounds", 0x1D430286) : ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_NPC_Sounds", 0x96866E58);

				audSoundSet soundset;

				if (soundset.Init(soundSetName))
				{
					audSoundInitParams initParams;
					initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_BLADE_0 + i;
					initParams.UpdateEntity = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.SetVariableValue(ATSTRINGHASH("bladerpm", 0x643CFF51), m_Vehicle->GetWeaponBlade(i).GetCurrentRPM());

					if (timeMs - lastCollisionTime > 250)
					{
						u32 soundName = m_LastWeaponBladeCollisionIsPed[i] ? ATSTRINGHASH("blade_impact_ped", 0xCC098265) : ATSTRINGHASH("blade_impact", 0xCC444D1B);
						CreateAndPlaySound(soundset.Find(soundName), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSetName, soundName, &initParams, m_Vehicle));
					}

					if (!m_BladeImpactSound[i])
					{
						const u32 soundName = m_LastWeaponBladeCollisionIsPed[i] ? ATSTRINGHASH("blade_sustained_ped", 0x82DA4A2B) : ATSTRINGHASH("blade_sustained", 0x4E799F0C);
						CreateAndPlaySound_Persistent(soundset.Find(soundName), &m_BladeImpactSound[i], &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundSetName, soundName, &initParams, m_BladeImpactSound[i], m_Vehicle));
					}
				}

				m_IsWeaponBladeColliding[i] = false;
				m_LastWeaponBladeCollisionIsPed[i] = false;
				m_LastWeaponBladeCollisionTime[i] = timeMs;
			}
			else if (m_BladeImpactSound[i] && timeMs - lastCollisionTime > 250)
			{
				m_BladeImpactSound[i]->StopAndForget(true);
			}
		}
	}	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateHeadLights
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateHeadLights()
{
	if(m_HeadlightsCoverSwitched)
	{
		u32 headlightSoundSetName = 0u;
		const u32 vehicleModelNameHash = GetVehicleModelNameHash();

		if(vehicleModelNameHash == ATSTRINGHASH("TROPOS", 0x707E63A4))
		{
			headlightSoundSetName = ATSTRINGHASH("tropos_headlights_ss", 0x475A5417);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD))
		{
			headlightSoundSetName = ATSTRINGHASH("ruiner2_headlights_ss", 0xA3340CED);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("INFERNUS2", 0xAC33179C))
		{
			headlightSoundSetName = ATSTRINGHASH("infernus2_headlights_ss", 0x1C00E9AD);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("TURISMO2", 0xC575DF11))
		{
			headlightSoundSetName = ATSTRINGHASH("turismo2_headlights_ss", 0x80EF8AB8);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("CHEETAH2", 0xD4E5F4D))
		{
			headlightSoundSetName = ATSTRINGHASH("cheetah2_headlights_ss", 0xA4A804DA);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("TORERO", 0x59A9E570))
		{
			headlightSoundSetName = ATSTRINGHASH("torero_headlights_ss", 0xE6A80549);
		}
		else if (vehicleModelNameHash == ATSTRINGHASH("TORERO2", 0xF62446BA))
		{
			headlightSoundSetName = ATSTRINGHASH("torero2_headlights_ss", 0x1066C5FE);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("ARDENT", 0x97E5533))
		{
			headlightSoundSetName = ATSTRINGHASH("ardent_headlights_ss", 0x61D0BD14);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("VIGILANTE", 0xB5EF4C33))
		{
			headlightSoundSetName = ATSTRINGHASH("vigilante_headlights_ss", 0x5EF539CC);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("VISERIS", 0xE8A8BA94))
		{
			headlightSoundSetName = ATSTRINGHASH("viseris_headlights_ss", 0x65527488);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("ZR350", 0x91373058))
		{
			headlightSoundSetName = ATSTRINGHASH("zr350_headlights_ss", 0x29A9ED20);
		}
		else if(vehicleModelNameHash == ATSTRINGHASH("FUTO2", 0xA6297CC8))
		{
			headlightSoundSetName = ATSTRINGHASH("futo2_headlights_ss", 0x5120B2EE);
		}

		if(headlightSoundSetName != 0u)
		{
			audSoundSet lightCoverSoundset;

			if(lightCoverSoundset.Init(headlightSoundSetName))
			{
				u32 lightCoverSoundHash = 0;

				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_LIGHTCOVER;

				switch(m_Vehicle->GetHeadlightCoverState())
				{
				case CVehicle::LIGHTCOVER_OPENING:
					lightCoverSoundHash = ATSTRINGHASH("LightCover_PopUp", 0x6B9F809D);
					break;

				case CVehicle::LIGHTCOVER_CLOSING:
					lightCoverSoundHash = ATSTRINGHASH("LightCover_PopDown", 0x4DA77B7B);
					break;

				default:
					break;
				}

				if(lightCoverSoundHash != 0)
				{
					audMetadataRef lightCoverSoundRef = lightCoverSoundset.Find(lightCoverSoundHash);

					if(lightCoverSoundRef != g_NullSoundRef)
					{
						CreateAndPlaySound(lightCoverSoundRef, &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(lightCoverSoundset.GetNameHash(), lightCoverSoundHash, &initParams, m_Vehicle));
					}
				}
			}
		}		

		m_HeadlightsCoverSwitched = false;
	}

	if(m_HeadlightsSwitched)
	{
		audMetadataRef soundRef = g_NullSoundRef;
		u32 soundHash = 0;
		bool switchedOn = m_Vehicle->m_nVehicleFlags.bHeadlightsFullBeamOn;

		switch(m_VehicleType)
		{
		case AUD_VEHICLE_CAR:
			if(IsSubmersible())
			{
				soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn_Submersible", 0x8895D4EA) : ATSTRINGHASH("Headlamp_SwitchOff_Submersible", 0x2051CFDD);
			}
			else
			{
				soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn", 0xA1D32831) : ATSTRINGHASH("Headlamp_SwitchOff", 0xB9F6F7F);
			}			
			break;
		case AUD_VEHICLE_PLANE:
			soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn_Plane", 0x2FB63D2A) : ATSTRINGHASH("Headlamp_SwitchOff_Plane", 0x8978EF66);
			break;
		case AUD_VEHICLE_BOAT:
			if(m_Vehicle && m_Vehicle->InheritsFromSubmarine())
			{
				soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn_Submersible", 0x8895D4EA) : ATSTRINGHASH("Headlamp_SwitchOff_Submersible", 0x2051CFDD);
			}
			else 
			{
				soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn_Boat", 0xDFB28208) : ATSTRINGHASH("Headlamp_SwitchOff_Boat", 0x2CD0CA60);
			}
			break;
		case AUD_VEHICLE_BICYCLE:
			soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn_Bicycle", 0xAB457F9E) : ATSTRINGHASH("Headlamp_SwitchOff_Bicycle", 0x4A6FFF8B);
			break;
		case AUD_VEHICLE_HELI:
			soundHash = switchedOn? ATSTRINGHASH("Headlamp_SwitchOn_Heli", 0x5B2256A6) : ATSTRINGHASH("Headlamp_SwitchOff_Heli", 0x55529031);
			break;
		default:
			break;
		}

		if(soundHash != 0)
		{
			soundRef = sm_LightsSoundSet.Find(soundHash);

			if(soundRef != g_NullSoundRef)
			{
				CreateAndPlaySound(soundRef);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_LightsSoundSet.GetNameHash(), soundHash, NULL, m_Vehicle));
			}
		}

		m_HeadlightsSwitched = false;
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity SetHydraulicSuspensionInputAxis
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetHydraulicSuspensionInputAxis(f32 hydraulicSuspensionInputAxisX, f32 hydraulicSuspensionInputAxisY)
{   
    m_PrevHydraulicsInputAxis = m_HydraulicsInputAxis;
    m_HydraulicsInputAxis = Vector2(hydraulicSuspensionInputAxisX, hydraulicSuspensionInputAxisY);

	if(m_Vehicle && !m_ShouldTriggerHydraulicBounce)
	{
		if(Sign(m_PrevHydraulicsInputAxis.x) != Sign(hydraulicSuspensionInputAxisX) || Sign(m_PrevHydraulicsInputAxis.y) != Sign(hydraulicSuspensionInputAxisY))
		{
			bool allWheelsInFreeMode = true;
			 
			for( int i = 0; i < m_Vehicle->GetNumWheels(); i++ )
			{
				CWheel *pWheel = m_Vehicle->GetWheel(i);

				if(!pWheel || pWheel->GetSuspensionHydraulicState() != eWheelHydraulicState::WHS_FREE)
				{
					allWheelsInFreeMode = false;
					break;
				}
			}

			if(allWheelsInFreeMode)
			{
				TriggerHydraulicBounce(m_IsPlayerVehicle);
			}
		}
	}	

    m_HydraulicSuspensionInputSmoother.CalculateValue((m_HydraulicsInputAxis - m_PrevHydraulicsInputAxis).Mag() * fwTimer::GetInvTimeStep() * 0.1f);
}

// ----------------------------------------------------------------
// audVehicleAudioEntity OnJumpingVehicleJumpActivate
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnJumpingVehicleJumpActivate()
{
	const u32 modelNameHash = GetVehicleModelNameHash();
	u32 soundsetName = 0u;

	if (modelNameHash == ATSTRINGHASH("MINITANK", 0xB53C6C52))
	{
		soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("ch_vehicle_minitank_player_sounds", 0xCF2B0226) : ATSTRINGHASH("ch_vehicle_minitank_remote_sounds", 0x90AF5388);
	}
	else if (modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD) || modelNameHash == ATSTRINGHASH("SCRAMJET", 0xD9F0503D))
	{
		soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
	}
	else
	{
		soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("DLC_AW_Mod_Vehicle_Player_Sounds", 0xD5676608) : ATSTRINGHASH("DLC_AW_Mod_Vehicle_NPC_Sounds", 0x29A11603);
	}
		
	audSoundSet soundset;
	const u32 soundName = IsToyCar() ? ATSTRINGHASH("jump_rc_activate", 0xCB05A19) : ATSTRINGHASH("jump_activate", 0xF1AFC397);

	if(soundset.Init(soundsetName))
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;	
		CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));
	}

	m_IsWaitingOnActivatedJumpLand = true;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity OnJumpingVehicleJumpPrimed
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnJumpingVehicleJumpPrimed()
{
	const u32 modelNameHash = GetVehicleModelNameHash();
	u32 soundsetName = 0u;

	if (modelNameHash == ATSTRINGHASH("MINITANK", 0xB53C6C52))
	{
		soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("ch_vehicle_minitank_player_sounds", 0xCF2B0226) : ATSTRINGHASH("ch_vehicle_minitank_remote_sounds", 0x90AF5388);
	}
	else if (modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD) || modelNameHash == ATSTRINGHASH("SCRAMJET", 0xD9F0503D))
	{
		soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
	}
	else
	{
		soundsetName = m_IsFocusVehicle ? ATSTRINGHASH("DLC_AW_Mod_Vehicle_Player_Sounds", 0xD5676608) : ATSTRINGHASH("DLC_AW_Mod_Vehicle_NPC_Sounds", 0x29A11603);
	}

	audSoundSet soundset;		
	const u32 soundName = IsToyCar() ? ATSTRINGHASH("jump_rc_armed", 0xFA26825D) : ATSTRINGHASH("jump_armed", 0xA9B32005);

	if(soundset.Init(soundsetName))
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;	
		CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity OnVehicleParachutePrepare
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnVehicleParachuteStabilize()
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD))
	{
		audSoundSet soundset;
		const u32 soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
		const u32 soundName = ATSTRINGHASH("Stabilize", 0x13306A9B);

		if(soundset.Init(soundsetName))
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;	
			CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));

			if (NetworkInterface::IsGameInProgress() && m_Vehicle->GetNetworkObject() && !m_Vehicle->IsNetworkClone())
			{
				CPlaySoundEvent::Trigger(m_Vehicle, soundsetName, soundName, 100);
			}
		}		
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerEMPShutdownSound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerEMPShutdownSound()
{
	if (m_IsFocusVehicle)
	{
		TriggerSimpleSoundFromSoundset(ATSTRINGHASH("DLC_AW_EMP_Sounds", 0xBBB256C9), ATSTRINGHASH("EMP_vehicle_affected", 0x3F586A15) REPLAY_ONLY(, true));
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerBoostActivateFail
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerBoostActivateFail()
{
	TriggerSimpleSoundFromSoundset(m_IsPlayerVehicle ? ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_Player_Sounds", 0x1D430286) : ATSTRINGHASH("DLC_AWXM2018_Mod_Vehicle_NPC_Sounds", 0x96866E58), ATSTRINGHASH("boost_fail", 0x22A21643) REPLAY_ONLY(, true));
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerSideShunt
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSideShunt()
{
	TriggerSimpleSoundFromSoundset(m_IsPlayerVehicle ? ATSTRINGHASH("DLC_AW_Mod_Vehicle_Player_Sounds", 0xD5676608) : ATSTRINGHASH("DLC_AW_Mod_Vehicle_NPC_Sounds", 0x29A11603), ATSTRINGHASH("sideswipe", 0x59CD6870) REPLAY_ONLY(, true));
}

// ----------------------------------------------------------------
// audVehicleAudioEntity IsLockedToRadioStation
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsLockedToRadioStation()
{
#if HEIST3_HIDDEN_RADIO_ENABLED 
	if (GetVehicleModelNameHash() == HEIST3_HIDDEN_RADIO_VEHICLE && m_RadioStation && m_RadioStation->GetNameHash() == HEIST3_HIDDEN_RADIO_STATION_NAME)
	{
		return true;
	}
#endif

	return false;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerStationarySuspensionDrop
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerStationarySuspensionDrop()
{
	if(!m_WasSuspensionDropped)
	{
		if (GetVehicleModelNameHash() != ATSTRINGHASH("DYNASTY", 0x127E90D5))
		{
			audSoundSet soundset;
			const u32 soundsetName = ATSTRINGHASH("DLC_XM_Air_Suspension_Sounds", 0x6638FEF);
			const u32 soundName = ATSTRINGHASH("air_suspension_down", 0xF7F05AB7);

			if(soundset.Init(soundsetName))
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;
				initParams.UpdateEntity = true;
				CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));
			}
		}

		m_WasSuspensionDropped = true;
	}	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity CancelStationarySuspensionDrop
// ----------------------------------------------------------------
void audVehicleAudioEntity::CancelStationarySuspensionDrop()
{
	m_WasSuspensionDropped = false;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerSpecialFlightModeTransform
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSpecialFlightModeTransform(bool flightModeEnabled)
{
	m_SpecialFlightModeTransformSoundToTrigger = flightModeEnabled? ATSTRINGHASH("transform_to_flight", 0x906FAB11) : ATSTRINGHASH("transform_to_car", 0xD8B90941);	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateSpecialFlightModeTransformSounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateSpecialFlightModeTransformSounds()
{
	if(m_SpecialFlightModeTransformSoundToTrigger != 0u)
	{		
		if(GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB))
		{
			if(m_SpecialFlightModeTransformSound)
			{
				m_SpecialFlightModeTransformSound->StopAndForget(true);
			}

			audSoundSet soundset;
			const u32 soundsetName = ATSTRINGHASH("deluxo_transform_ss", 0x33CF0FA6);

			if(soundset.Init(soundsetName))
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;	
				initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				CreateAndPlaySound_Persistent(soundset.Find(m_SpecialFlightModeTransformSoundToTrigger), &m_SpecialFlightModeTransformSound, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundsetName, m_SpecialFlightModeTransformSoundToTrigger, &initParams, m_SpecialFlightModeTransformSound, m_Vehicle));
			}
		}		

		m_SpecialFlightModeTransformSoundToTrigger = 0u;
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity CancelSpecialFlightModeTransform
// ----------------------------------------------------------------
void audVehicleAudioEntity::CancelSpecialFlightModeTransform()
{
	if(m_SpecialFlightModeTransformSound)
	{
		m_SpecialFlightModeTransformSound->StopAndForget(true);		
	}

	m_SpecialFlightModeTransformSoundToTrigger = 0u;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity TriggerSpecialFlightModeWingDeploy
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSpecialFlightModeWingDeploy(bool deployWings)
{
	if(GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB))
	{
		if(deployWings != m_SpecialFlightModeWingsDeployed)
		{
			audSoundSet soundset;
			const u32 soundsetName = ATSTRINGHASH("deluxo_transform_ss", 0x33CF0FA6);
			const u32 soundName = deployWings? ATSTRINGHASH("wings_out", 0xA92ED867) : ATSTRINGHASH("wings_in", 0x862E16BE);

			if(soundset.Init(soundsetName))
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;	
				initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;
				initParams.UpdateEntity = true;
				CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));
			}

			m_SpecialFlightModeWingsDeployed = deployWings;
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity OnVehicleParachuteDeploy
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnVehicleParachuteDeploy()
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD))
	{
		audSoundSet soundset;
		const u32 soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
		const u32 soundName = ATSTRINGHASH("Deploy", 0xC9200261);

		if(soundset.Init(soundsetName))
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;	
			CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));

			if (NetworkInterface::IsGameInProgress() && m_Vehicle->GetNetworkObject() && !m_Vehicle->GetNetworkObject()->IsClone())
			{
				CPlaySoundEvent::Trigger(m_Vehicle, soundsetName, soundName, 100);
			}
		}		
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity OnReducedSuspensionForceSet
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnReducedSuspensionForceSet(bool isReduced)
{
	if(isReduced == m_WasReducedSuspensionForceSet)
	{
		return;
	}

	audSoundSet suspensionForceSoundSet;
	u32 soundSetName = GetVehicleHydraulicsSoundSetName();
	u32 soundName = isReduced ? ATSTRINGHASH("Stance_Lowered", 0x90AA859B) : ATSTRINGHASH("Stance_Raised", 0xC5B6339F);

	if(suspensionForceSoundSet.Init(soundSetName))
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;	
		CreateDeferredSound(suspensionForceSoundSet.Find(soundName), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSetName, soundName, &initParams, m_Vehicle));

		/*
		if (NetworkInterface::IsGameInProgress() && m_Vehicle->GetNetworkObject() && !m_Vehicle->GetNetworkObject()->IsClone())
		{
			CPlaySoundEvent::Trigger(m_Vehicle, soundsetName, soundName, 100);
		}
		*/
	}

	m_WasReducedSuspensionForceSet = isReduced;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity OnVehicleParachuteDetach
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnVehicleParachuteDetach()
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("RUINER2", 0x381E10BD))
	{
		audSoundSet soundset;
		const u32 soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_ImportExport_Ruiner2_Sounds", 0xB2AD811) : ATSTRINGHASH("DLC_ImportExport_Ruiner2_NPC_Sounds", 0xF09C7816);
		const u32 soundName = ATSTRINGHASH("Detach", 0xFFC528E4);

		if(soundset.Init(soundsetName))
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;	
			CreateDeferredSound(soundset.Find(soundName), m_Vehicle, &initParams, true, true);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));

			if (NetworkInterface::IsGameInProgress() && m_Vehicle->GetNetworkObject() && !m_Vehicle->GetNetworkObject()->IsClone())
			{
				CPlaySoundEvent::Trigger(m_Vehicle, soundsetName, soundName, 100);
			}
		}		
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity CalculateHydraulicSuspensionAngle
// ----------------------------------------------------------------
void audVehicleAudioEntity::CalculateHydraulicSuspensionAngle()
{   
	if(m_Vehicle && m_Vehicle->InheritsFromAutomobile())
	{
		CAutomobile* automobile = static_cast<CAutomobile*>(m_Vehicle);
		f32 suspensionAngleUnitCircleX = 0.f;
		f32 suspensionAngleUnitCircleY = 0.f;

		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			f32 xAxis = 0.0;
			f32 yAxis = 0.0;

			CWheel *pWheel = m_Vehicle->GetWheel(i);

			if(pWheel)
			{
				switch(pWheel->GetHierarchyId())
				{
				case VEH_WHEEL_LF:
					xAxis = -0.707f;
					yAxis = 0.707f;
					break;
				case VEH_WHEEL_LR:
					xAxis = -0.707f;
					yAxis = -0.707f;
					break;
				case VEH_WHEEL_RR:
					xAxis = 0.707f;
					yAxis = -0.707f;
					break;
				case VEH_WHEEL_RF:
					xAxis = 0.707f;
					yAxis = 0.707f;
					break;
				default:
					break;
				}

				suspensionAngleUnitCircleX += (xAxis * pWheel->GetSuspensionRaiseAmount() / automobile->m_fHydraulicsUpperBound);
				suspensionAngleUnitCircleY += (yAxis * pWheel->GetSuspensionRaiseAmount() / automobile->m_fHydraulicsUpperBound);
			}
		}

		m_PrevHydraulicsSuspensionAngle = m_HydraulicsSuspensionAngle;
		m_HydraulicsSuspensionAngle = Vector2(suspensionAngleUnitCircleX, suspensionAngleUnitCircleY);
		m_HydraulicSuspensionAngleSmoother.CalculateValue((m_HydraulicsSuspensionAngle - m_PrevHydraulicsSuspensionAngle).Mag() * fwTimer::GetInvTimeStep() * 0.1f);
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateRadio
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateRadio()
{
#if NA_RADIO_ENABLED
	if(m_RadioStation && GetAudioVehicleType() != AUD_VEHICLE_BOAT && !IsInAmphibiousBoatMode() && !GetVehicle()->InheritsFromSubmarineCar() && m_DrowningFactor > 0.f)
	{
		if(!m_DrowningRadioSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			initParams.UpdateEntity = true;
			CreateAndPlaySound_Persistent(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("RadioDrown", 0x37CE14E5)), &m_DrowningRadioSound, &initParams);
		}
		if(m_DrowningRadioSound)
		{
			m_DrowningRadioSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(m_DrowningFactor));
		}
	}
	if(m_DrowningFactor == 0.f && m_DrowningRadioSound)
	{
		if(!m_Vehicle->m_nVehicleFlags.bIsDrowning)
		{
			// ensure the sound is muted so that the final spark sound is inaudible
			m_DrowningRadioSound->SetRequestedVolume(-100.f);
		}
		m_DrowningRadioSound->StopAndForget();
	}
#endif
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateEnvironment
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateEnvironment()
{
	if(m_EnvironmentGroup)
	{
		if(m_Vehicle)
		{
			m_EnvironmentGroup->SetSource(m_Vehicle->GetTransform().GetPosition());
		}

		if(IsReal())
		{
			m_EnvironmentGroup->SetSourceEnvironmentUpdates(200);
		}
		else if(!IsDisabled())
		{
			m_EnvironmentGroup->SetSourceEnvironmentUpdates(1000);
		}
		else
		{
			m_EnvironmentGroup->SetSourceEnvironmentUpdates(1); // enough to do updates, but only in interiors
		}

		// don't occlude the player (TODO case this for camera modes)
		CPed* localPlayer = CGameWorld::FindLocalPlayer();
		if(localPlayer && localPlayer->GetVehiclePedInside() == m_Vehicle )
		{
			m_EnvironmentGroup->SetNotOccluded(true);
		}
		else
		{
			m_EnvironmentGroup->SetNotOccluded(false);
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity SmoothSirenVolume
// ----------------------------------------------------------------
void audVehicleAudioEntity::SmoothSirenVolume(f32 rate,f32 desireLinVol,bool stopSiren)
{
	// Flag the entity to stop the siren after muting it if we want to.
	m_ShouldStopSiren = stopSiren;
	// Set rates and desire volume
	m_SirensDesireLinVol = desireLinVol;
	m_SirensVolumeSmoother.SetRate(rate);
	// If the smoother hasn't been used yet, initialize it with the current volume (1 - desire)
	if(!m_SirensVolumeSmoother.IsInitialized())
	{
		m_SirensVolumeSmoother.CalculateValue(1.f-desireLinVol,fwTimer::GetTimeInMilliseconds());
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateCarRecordings
// Check if the veh is playing a car recording and tells the audio entity to play the audio for it.
// ----------------------------------------------------------------
void audVehicleAudioEntity::CheckForRecordingChange()
{
	s32 index = CVehicleRecordingMgr::GetPlaybackIdForCar(m_Vehicle);
#if __BANK
	if(CVehicleRecordingMgr::GetRecordingHash(index) == sm_DebugCarRecording.nameHash)
	{
		return;
	}
#endif
	CarRecordingAudioSettings * newSettings = audNorthAudioEngine::GetObject<CarRecordingAudioSettings>(CVehicleRecordingMgr::GetRecordingHash(index));
	if( newSettings && newSettings != m_CRSettings)
	{
		u8* numPersistentMixerScenes = (u8*)(&m_CRSettings->Event[m_CRSettings->numEvents]);
		for(u8 i=0; i < *numPersistentMixerScenes; i++)
		{				
			if (m_CarRecordingPersistentScenes[i])
			{
				m_CarRecordingPersistentScenes[i]->Stop();
			}
		}
		if(m_CRSettings && m_CRSettings->Mixgroup)
		{
			DYNAMICMIXER.RemoveEntityFromMixGroup(m_Vehicle);
		}
		m_CRSettings = newSettings;
		m_LastCREventProcessed = 0;
	}
	if(m_CRSettings && m_CRSettings->Mixgroup)
	{
		DYNAMICMIXER.AddEntityToMixGroup(m_Vehicle, m_CRSettings->Mixgroup);
	}
}

void audVehicleAudioEntity::UpdateCarRecordings()
{
#if __BANK 
	if( sm_PauseGame)
	{
		fwTimer::SetTimeWarp(0.f);
	}
#endif
	//Check if there is a car recording going on.
	if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle))
	{
		if(!m_CRSettings)
		{
			s32 index = CVehicleRecordingMgr::GetPlaybackIdForCar(m_Vehicle);
#if __BANK
			if(CVehicleRecordingMgr::GetRecordingHash(index) == sm_DebugCarRecording.nameHash)
			{
				return;
			}
#endif
			m_CRSettings = audNorthAudioEngine::GetObject<CarRecordingAudioSettings>(CVehicleRecordingMgr::GetRecordingHash(index));
		}
		if (m_CRSettings)
		{
			if(m_CRSettings->Mixgroup)
			{
				DYNAMICMIXER.AddEntityToMixGroup(m_Vehicle, m_CRSettings->Mixgroup);
			}
			AllocateVehicleVariableBlock();
			if(m_LastCREventProcessed >= m_CRSettings->numEvents)
			{
				CheckForRecordingChange();
			}
			s32 playBackSlot = CVehicleRecordingMgr::GetPlaybackSlot(m_Vehicle);
			f32 curretPlaybackTime =  CVehicleRecordingMgr::GetPlaybackRunningTime(playBackSlot);
			for(u8 i=m_LastCREventProcessed; i < m_CRSettings->numEvents; i++)
			{
				if(curretPlaybackTime >= m_CRSettings->Event[i].Time)
				{
					// Play the sound.
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
					initParams.TrackEntityPosition = true;
					initParams.u32ClientVar = AUD_VEHICLE_SOUND_CAR_RECORDING;
					initParams.UpdateEntity = true;
					CreateAndPlaySound(m_CRSettings->Event[i].Sound, &initParams);
					// Trigger the scene.
					if(m_CRSettings->Event[i].OneShotScene != 0)
					{
						DYNAMICMIXER.StartScene(m_CRSettings->Event[i].OneShotScene);
					}
					m_LastCREventProcessed = i + 1u;
				}
				else 
				{
					break;
				}
			}

			// Need to do a bit of funky pointer manipulation here due to having two variable length lists one after another
			u8* numPersistentMixerScenes = (u8*)(&m_CRSettings->Event[m_CRSettings->numEvents]);
			CarRecordingAudioSettings::tPersistentMixerScenes* persistentMixerScenes = (CarRecordingAudioSettings::tPersistentMixerScenes*)(numPersistentMixerScenes + sizeof(u32));

			for(u8 i=0; i < *numPersistentMixerScenes; i++)
			{
				if(curretPlaybackTime >= persistentMixerScenes[i].StartTime
					&& curretPlaybackTime < persistentMixerScenes[i].EndTime
					&& !m_CarRecordingPersistentScenes[i])
				{
					DYNAMICMIXER.StartScene(persistentMixerScenes[i].Scene,&m_CarRecordingPersistentScenes[i]);
				}
				else if (m_CarRecordingPersistentScenes[i] && curretPlaybackTime > persistentMixerScenes[i].EndTime)
				{
					m_CarRecordingPersistentScenes[i]->Stop();
				}
			}
#if __BANK
			if(sm_DrawCREvents)
			{
				if(g_CRFilter[0] != 0 && !audAmbientAudioEntity::MatchName(audNorthAudioEngine::GetMetadataManager().GetObjectName(m_CRSettings->NameTableOffset,0),g_CRFilter))
				{
					return;
				}
				for(int i=0; i < m_CRSettings->numEvents; i++)
				{
					Vector3 pos;
					if (CVehicleRecordingMgr::HasRecordingFileBeenLoaded(playBackSlot)) 
					{
						CVehicleRecordingMgr::GetPositionOfCarRecordingAtTime(playBackSlot,m_CRSettings->Event[i].Time,pos);
					}
					else 
					{ 
						Matrix34 mat;
						s32	IndexInRecording = 0;
						CVehicleStateEachFrame *pVehState = reinterpret_cast<CVehicleStateEachFrame *>(&((CVehicleRecordingMgr::GetPlaybackBuffer(playBackSlot))[0]));
						CVehicleRecordingMgr::RestoreInfoForMatrix(mat, pVehState);

						while ( (IndexInRecording < CVehicleRecordingMgr::GetPlaybackBufferSize(playBackSlot)) && ((CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(playBackSlot))[IndexInRecording]))->TimeInRecording < m_CRSettings->Event[i].Time)
						{
							CVehicleRecordingMgr::RestoreInfoForMatrix( mat, reinterpret_cast<CVehicleStateEachFrame *>(&((CVehicleRecordingMgr::GetPlaybackBuffer(playBackSlot))[IndexInRecording])));
							IndexInRecording += sizeof(CVehicleStateEachFrame);
						}
						pos = mat.d;
					} 
					Color32 color = Color_red;
					if(curretPlaybackTime > m_CRSettings->Event[i].Time)
					{
						color = Color_green;
					}
					grcDebugDraw::Sphere(pos,0.3f,color);
					char txt [128]; 
					formatf(txt,"%f",m_CRSettings->Event[i].Time);
					grcDebugDraw::Text(pos + Vector3(0.f,0.f,1.f),color,txt);
					formatf(txt,"%s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_CRSettings->Event[i].Sound));
					grcDebugDraw::Text(pos + Vector3(0.f,0.f,0.75f),color,txt);
					formatf(txt,"%s",g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectName(m_CRSettings->Event[i].OneShotScene));
					grcDebugDraw::Text(pos + Vector3(0.f,0.f,0.5f),color,txt);
					if(sm_DrawCRName)
					{
						formatf(txt,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(m_CRSettings->NameTableOffset,0));
						grcDebugDraw::Text(pos + Vector3(0.f,0.f,1.25f),color,txt);
					}

				}
			}
#endif
		}
	}
	else
	{
		if(m_CRSettings)
		{
			u8* numPersistentMixerScenes = (u8*)(&m_CRSettings->Event[m_CRSettings->numEvents]);
			for(u8 i=0; i < *numPersistentMixerScenes; i++)
			{				
				if (m_CarRecordingPersistentScenes[i])
				{
					m_CarRecordingPersistentScenes[i]->Stop();
				}
			}
			if(m_CRSettings->Mixgroup)
			{
				DYNAMICMIXER.RemoveEntityFromMixGroup(m_Vehicle);
			}
			m_CRSettings = NULL;
		}
		m_LastCREventProcessed = 0;
		m_CRSettings = NULL;
	}
}
#if __BANK
void audVehicleAudioEntity::UpdateCarRecordingTool()
{
	if( sm_PauseGame)
	{
		fwTimer::SetTimeWarp(0.f);
	}
	if(sm_EnableCREditor)
	{
		if ((g_pFocusEntity && g_pFocusEntity->GetIsTypeVehicle()) || sm_LoadExistingCR)
		{
			CVehicle* pVeh = NULL;
			if(sm_LoadExistingCR && sm_DebugCarRecording.settings)
			{	
				if ( !sm_VehCreated)
				{
					if(naVerifyf(sm_DebugCarRecording.settings->VehicleModelId != fwModelId::MI_INVALID, "No veh id. stored for this CR, either do it from the mission or contact audio team."))
					{
						sm_DebugCarRecording.modelId = sm_DebugCarRecording.settings->VehicleModelId;
					}
				}
				pVeh = CGameWorld::FindLocalPlayerVehicle();
				if (pVeh)
				{
					sm_VehCreated = true;
				}
			}
			else
			{
				pVeh = static_cast<CVehicle*>(g_pFocusEntity);
			}
			if(pVeh)
			{
				if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh) || sm_LoadExistingCR)
				{
					s32 index = CVehicleRecordingMgr::GetPlaybackIdForCar(pVeh);
					if(index >= 0 || sm_LoadExistingCR)
					{
						if(sm_DebugCarRecording.nameHash.IsNull())
						{
							// Get the car recording and add the audio prefix
							char recording[128];
							if( !sm_LoadExistingCR)
							{
								formatf(sm_CRAudioSettingsCreator,sizeof(sm_CRAudioSettingsCreator),"%s", CVehicleRecordingMgr::GetRecordingName(index));
								formatf(recording,sizeof(recording),"%s", CVehicleRecordingMgr::GetRecordingName(index));
								sm_DebugCarRecording.modelId = pVeh->GetModelIndex();
							}
							else
							{
								u32 nameIdx = 3;// TO AVOID CR_
								u32 strLength = ustrlen(sm_CRAudioSettingsCreator);
								for(s32 i = nameIdx; i < strLength + 1; i++)
								{
									recording[i - nameIdx] = sm_CRAudioSettingsCreator[i];
								}
							}
							//Store it in our debug object.
							sm_DebugCarRecording.nameHash.SetFromString(sm_CRAudioSettingsCreator);
							//Get the recording name and index for the manager
							char recordingName[128];
							char recordNum[64];
							u32 length = ustrlen(recording);
							u32 nameLength = length - 3;
							formatf(recordingName,nameLength + 1,"%s", recording);
							u32 numIdx = 0;
							for(s32 i = nameLength; i < length; i++)
							{
								recordNum[numIdx++] = recording[i];
							}
							CVehicleRecordingMgr::SetCurrentRecording(recordingName,(u32)atoi(recordNum));
							sm_LoadExistingCR = false;
						}
						else
						{
							char newRecording[128];
							formatf(newRecording,sizeof(newRecording),"%s", CVehicleRecordingMgr::GetRecordingName(index));
							if( atStringHash(sm_CRAudioSettingsCreator) != sm_DebugCarRecording.nameHash.GetHash())
							{
								char errorMsg[128];
								formatf(errorMsg,sizeof(errorMsg),"%s", CVehicleRecordingMgr::GetRecordingName(index));
								naAssertf(false,"Currently working on %s, in order to work with a new recording, please finish your work.",errorMsg);
							}
						}
					}
				}
			}
		}
		if(sm_DebugCarRecording.nameHash.IsNotNull())
		{
			sm_DebugCarRecording.settings = NULL;
			sm_DebugCarRecording.settings = audNorthAudioEngine::GetObject<CarRecordingAudioSettings>(sm_DebugCarRecording.nameHash);
			
		}
		if(sm_DebugCarRecording.settings)
		{
			//sm_DebugCarRecording.eventList.Reset();
			for(int j=0; j < sm_DebugCarRecording.settings->numEvents; j++)
			{
				if(j < sm_DebugCarRecording.eventList.GetCount())
				{
					sm_DebugCarRecording.eventList[j].time = sm_DebugCarRecording.settings->Event[j].Time;
					sm_DebugCarRecording.eventList[j].hashEvent = sm_DebugCarRecording.settings->Event[j].Sound;
				}
				else
				{
					audioRecordingEvent event;
					event.time = sm_DebugCarRecording.settings->Event[j].Time;
					event.hashEvent = sm_DebugCarRecording.settings->Event[j].Sound;
					sm_DebugCarRecording.eventList.PushAndGrow(event);
				}
			}
		}
		if(sm_UpdateCRTime && !sm_ManualTimeSet)
		{
			CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
			if(pVeh)
			{
				if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
				{
					s32 playBackSlot = CVehicleRecordingMgr::GetPlaybackSlot(pVeh);
					sm_CRCurrentTime = CVehicleRecordingMgr::GetPlaybackRunningTime(playBackSlot);
					sm_CRProgress =  (f32)(sm_CRCurrentTime / (f32) CVehicleRecordingMgr::GetEndTimeOfRecording(playBackSlot)) * 100.f;
					if(sm_DebugCarRecording.settings)
					{
						for(int i=0; i < sm_DebugCarRecording.settings->numEvents; i++)
						{
							if(!sm_DebugCarRecording.eventList[i].processed && sm_CRCurrentTime >= sm_DebugCarRecording.settings->Event[i].Time)
							{
								audSoundInitParams initParams;
								initParams.EnvironmentGroup = pVeh->GetAudioEnvironmentGroup();
								initParams.TrackEntityPosition = true;
								sm_DebugCarRecording.eventList[i].processed  = true;
								pVeh->GetVehicleAudioEntity()->CreateAndPlaySound(sm_DebugCarRecording.settings->Event[i].Sound, &initParams);
							}
							else 
							{
								continue;
							}
						}
						if(sm_DrawCREvents)
						{
							if(g_CRFilter[0] != 0 && sm_DebugCarRecording.nameHash != atStringHash(g_CRFilter))//!audAmbientAudioEntity::MatchName(GetName(), g_CRFilter))
							{
								return;
							}
							for(int i=0; i < sm_DebugCarRecording.settings->numEvents; i++)
							{
								Vector3 pos;
								if (CVehicleRecordingMgr::HasRecordingFileBeenLoaded(playBackSlot)) 
								{
									CVehicleRecordingMgr::GetPositionOfCarRecordingAtTime(playBackSlot,sm_DebugCarRecording.settings->Event[i].Time,pos);
								}
								else 
								{ 
									Matrix34 mat;
									s32	IndexInRecording = 0;
									CVehicleStateEachFrame *pVehState = reinterpret_cast<CVehicleStateEachFrame *>(&((CVehicleRecordingMgr::GetPlaybackBuffer(playBackSlot))[0]));
									CVehicleRecordingMgr::RestoreInfoForMatrix(mat, pVehState);

									while ( (IndexInRecording < CVehicleRecordingMgr::GetPlaybackBufferSize(playBackSlot)) && ((CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(playBackSlot))[IndexInRecording]))->TimeInRecording < sm_DebugCarRecording.settings->Event[i].Time)
									{
										CVehicleRecordingMgr::RestoreInfoForMatrix( mat, reinterpret_cast<CVehicleStateEachFrame *>(&((CVehicleRecordingMgr::GetPlaybackBuffer(playBackSlot))[IndexInRecording])));
										IndexInRecording += sizeof(CVehicleStateEachFrame);
									}
									pos = mat.d;
								} 
								Color32 color = Color_red;
								if(sm_CRCurrentTime > sm_DebugCarRecording.settings->Event[i].Time)
								{
									color = Color_green;
								}
								grcDebugDraw::Sphere(pos,0.3f,color);
								char txt [128]; 
								formatf(txt,"%f",sm_DebugCarRecording.settings->Event[i].Time);
								grcDebugDraw::Text(pos + Vector3(0.f,0.f,1.f),color,txt);
								formatf(txt,"%s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(sm_DebugCarRecording.settings->Event[i].Sound));
								grcDebugDraw::Text(pos + Vector3(0.f,0.f,0.75f),color,txt);
								formatf(txt,"%s",g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectName(sm_DebugCarRecording.settings->Event[i].OneShotScene));
								grcDebugDraw::Text(pos + Vector3(0.f,0.f,0.5f),color,txt);
								if(sm_DrawCRName && sm_DebugCarRecording.settings)
								{
									formatf(txt,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_DebugCarRecording.settings->NameTableOffset,0));
									grcDebugDraw::Text(pos + Vector3(0.f,0.f,1.25f),color,txt);
								}
							}
						}
					}
				}
			}
		}
	
	}
}
#endif

// ----------------------------------------------------------------
// Find the priority of a given skid sound
// ----------------------------------------------------------------
u8 audVehicleAudioEntity::FindSkidSurfacePriority(const u8 priorityType)
{
	// Priorities from low->high
	static const u8 surfacePriorityOrder[NUM_SURFACEPRIORITY] = 
	{
		PRIORITY_OTHER,
		GRASS,
		TARMAC,
		SAND,
		GRAVEL,
	};	


	for(u8 k = 0; k < NUM_SURFACEPRIORITY; k ++)
	{
		if(priorityType == surfacePriorityOrder[k])
		{
			return k;
		}
	}

	return 0;
}

// ----------------------------------------------------------------
// IsPedOnMovingTrain
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsPedOnMovingTrain(const CPed* pPed)
{
	if(!pPed)
	{
		return false;
	}

	if(!audVerifyf(sysThreadType::IsUpdateThread(), "Not safe to query IsPedOnMovingTrain on audio thread"))
	{
		return false;
	}

	Vec3V vVelocity;
	CEntity* pTargetAttachedEntity = static_cast<CEntity*>(pPed->GetAttachParent());
	CPhysical* pTargetGroundPhysical = pPed->GetGroundPhysical();

	if( pTargetGroundPhysical && pTargetGroundPhysical->GetIsTypeVehicle() &&
		static_cast<CVehicle*>(pTargetGroundPhysical)->InheritsFromTrain() )
	{
		vVelocity = VECTOR3_TO_VEC3V(pTargetGroundPhysical->GetVelocity());
	}
	else if( pTargetAttachedEntity && pTargetAttachedEntity->GetIsTypeVehicle() &&
		static_cast<CVehicle*>(pTargetAttachedEntity)->InheritsFromTrain() )
	{
		CTrain* pTrain = static_cast<CTrain*>(pTargetAttachedEntity);
		vVelocity = VECTOR3_TO_VEC3V(pTrain->GetVelocity());
	}
	else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain))
	{
		vVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
	}
	else
	{
		return false;
	}

	ScalarV scMinTrainSpeedSq = LoadScalar32IntoScalarV(V_ONE);
	scMinTrainSpeedSq = (scMinTrainSpeedSq * scMinTrainSpeedSq);
	return IsGreaterThanAll(MagSquared(vVelocity), scMinTrainSpeedSq) != 0;
}

// ----------------------------------------------------------------
// Load any banks as requested by script
// ----------------------------------------------------------------
void audVehicleAudioEntity::ProcessScriptVehicleBankLoad()
{
	if(sm_ScriptVehicleRequest != 0u)
	{
		if(sm_ScriptVehicleWaveSlot && IsPlayerVehicleStarving())
		{
			audWaveSlotManager::FreeWaveSlot(sm_ScriptVehicleWaveSlot);
		}
		else if(!sm_ScriptVehicleWaveSlot && !IsPlayerVehicleStarving())
		{
			u32 bankLoadSound = 0;
			audMetadataObjectInfo objectInfo;
			bool useHighQualitySlot = false;

			if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(sm_ScriptVehicleRequest, objectInfo))
			{
				switch(objectInfo.GetType())
				{
				case CarAudioSettings::TYPE_ID:
					{
						const CarAudioSettings* carSettings = objectInfo.GetObject<CarAudioSettings>();

						if(carSettings->EngineType == ELECTRIC)
						{
							const ElectricEngineAudioSettings* engineSettings = audNorthAudioEngine::GetMetadataManager().GetObject<ElectricEngineAudioSettings>(carSettings->ElectricEngine);
							bankLoadSound = engineSettings ? engineSettings->BankLoadSound : 0u;
						}
						else
						{
							const GranularEngineAudioSettings* engineSettings = audNorthAudioEngine::GetMetadataManager().GetObject<GranularEngineAudioSettings>(carSettings->GranularEngine);
							bankLoadSound = engineSettings ? engineSettings->EngineAccel : 0u;
							useHighQualitySlot = true;
						}					
					}
					break;

				case PlaneAudioSettings::TYPE_ID:
					{
						const PlaneAudioSettings* planeSettings = objectInfo.GetObject<PlaneAudioSettings>();

						if(planeSettings)
						{
							if(planeSettings->SimpleSoundForLoading != g_NullSoundHash)
							{
								bankLoadSound = planeSettings->SimpleSoundForLoading;
							}
							else
							{
								bankLoadSound = planeSettings->BankingLoop;
							}
						}
					}
					break;

				case HeliAudioSettings::TYPE_ID:
					{
						const HeliAudioSettings* heliSettings = objectInfo.GetObject<HeliAudioSettings>();

						if(heliSettings)
						{
							if(heliSettings->SimpleSoundForLoading != g_NullSoundHash)
							{
								bankLoadSound = heliSettings->SimpleSoundForLoading;
							}
							else
							{
								bankLoadSound = heliSettings->StartUpOneShot;
							}
						}
					}
					break;

				case BoatAudioSettings::TYPE_ID:
					{
						const GranularEngineAudioSettings* engineSettings = audNorthAudioEngine::GetMetadataManager().GetObject<GranularEngineAudioSettings>(objectInfo.GetObject<BoatAudioSettings>()->GranularEngine);
						bankLoadSound = engineSettings ? engineSettings->EngineAccel : 0u;
						useHighQualitySlot = true;
					}
					break;
				}
			}

			if(bankLoadSound != 0u)
			{
				if(useHighQualitySlot)
				{
					sm_ScriptVehicleWaveSlot = sm_HighQualityWaveSlotManager.LoadBankForSound(bankLoadSound, AUD_WAVE_SLOT_ANY, false, NULL, naWaveLoadPriority::General);
				}
				else
				{
					sm_ScriptVehicleWaveSlot = sm_StandardWaveSlotManager.LoadBankForSound(bankLoadSound, AUD_WAVE_SLOT_ANY, false, NULL, naWaveLoadPriority::General);
				}
			}
			else
			{
				sm_ScriptVehicleRequest = 0u;
			}
		}
	}
}

// ----------------------------------------------------------------
// ClassFinishUpdate
// ----------------------------------------------------------------
void audVehicleAudioEntity::ClassFinishUpdate()
{
	ProcessScriptVehicleBankLoad();

	CPed* followPlayer = FindFollowPed();
	if(IsPedOnMovingTrain(followPlayer))
	{
		if(!sm_OnMovingTrainScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("ON_MOVING_TRAIN_SCENE", 0x6A8E30DA), &sm_OnMovingTrainScene);
		}
	}
	else if(sm_OnMovingTrainScene)
	{
		sm_OnMovingTrainScene->Stop();
		sm_OnMovingTrainScene = NULL;
	}

	if(g_ReflectionsAudioEntity.IsFocusVehicleInStuntTunnel())
	{
		if(!sm_StuntTunnelScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("STUNT_TUNNEL_SCENE", 0x58F4A9AB), &sm_StuntTunnelScene);
		}		
	}
	else if(sm_StuntTunnelScene)
	{
		sm_StuntTunnelScene->Stop();
		sm_StuntTunnelScene = NULL;
	}

	sm_IsSpectatorModeActive = NetworkInterface::IsInSpectatorMode();

	if(!sm_ExhaustProximityScene)
	{
		DYNAMICMIXER.StartScene(ATSTRINGHASH("EXHAUST_PROXIMITY_SCENE", 0xB1A84119), &sm_ExhaustProximityScene);
	}
	
	if(sm_ExhaustProximityScene)
	{
		sm_ExhaustProximityScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), sm_PlayerVehicleProximityRatio);
	}

	sm_PlayerVehicleOpenness = 1.0f;
	f32 playerVehicleEngineOpenness = 1.0f;

	CVehicle* followPlayerVehicle = CGameWorld::FindFollowPlayerVehicle();
	if(!followPlayerVehicle)
	{
		followPlayerVehicle = audPedAudioEntity::GetBJVehicle();
	}
	
	if(followPlayerVehicle)
	{
		VehicleType vehicleType = followPlayerVehicle->GetVehicleType();
		u32 vehicleNameHash = followPlayerVehicle->GetVehicleAudioEntity()->GetVehicleModelNameHash();
		bool isBike = vehicleType == VEHICLE_TYPE_BIKE || vehicleType == VEHICLE_TYPE_QUADBIKE || vehicleType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE;
		bool isBicycle = vehicleType == VEHICLE_TYPE_BICYCLE;
		bool isAircraft = vehicleType == VEHICLE_TYPE_PLANE || vehicleType == VEHICLE_TYPE_HELI;

		if (vehicleNameHash == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
		{
			if (!sm_VehicleGeneralScene)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("dlc_hei4_kosatka_driving_scene", 0x19ACDDEF), &sm_VehicleGeneralScene);
			}
		}
		else if(followPlayerVehicle->GetVehicleAudioEntity()->IsGoKart())
		{
			if (!sm_VehicleGeneralScene)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("Karts_Driving_Scene", 0xDF1D0DF0), &sm_VehicleGeneralScene);
			}
		}
		else if (sm_VehicleGeneralScene)
		{
			sm_VehicleGeneralScene->Stop();
			sm_VehicleGeneralScene = NULL;
		}

		if(audNorthAudioEngine::IsRenderingFirstPersonTurretCam())
		{
			if(!sm_VehicleFirstPersonTurretScene)
			{
				const camBaseCamera* dominantRenderedCamera = camInterface::GetDominantRenderedCamera();
				u32 turretSceneName = ATSTRINGHASH("VEHICLE_FIRSTPERSON_TURRET_SCENE", 0xDB9C390B);

				if(dominantRenderedCamera)
				{
					if(dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_DUNE3", 0x8501FD84) ||
						dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_APC_CANNON", 0xD3FE954B) ||
						dominantRenderedCamera->GetNameHash() == ATSTRINGHASH("POV_TURRET_CAMERA_APC_MISSILE", 0xD4FCAB99))
					{
						turretSceneName = ATSTRINGHASH("VEHICLE_FIRSTPERSON_TURRET_INTERIOR_SCENE", 0x67F22C56);						
					}
				}

				const MixerScene* mixerScene = DYNAMICMIXMGR.GetObject<MixerScene>(turretSceneName);

				if(mixerScene != NULL)
				{
					DYNAMICMIXER.StartScene(mixerScene, &sm_VehicleFirstPersonTurretScene);
				}
			}
		}
		else if(sm_VehicleFirstPersonTurretScene)
		{
			sm_VehicleFirstPersonTurretScene->Stop();
			sm_VehicleFirstPersonTurretScene = NULL;
		}

		if(audNorthAudioEngine::IsRenderingHoodMountedVehicleCam() || followPlayerVehicle->GetVehicleAudioEntity()->IsToyCar())
		{
			if(sm_VehicleInteriorScene)
			{
				sm_VehicleInteriorScene->Stop();
				sm_VehicleInteriorScene = NULL;
			}

			if(!sm_VehicleBonnetCamScene)
			{
				u32 bonnetCamSceneName = 0u;

				if (followPlayerVehicle->GetVehicleAudioEntity()->IsToyCar())
				{
					bonnetCamSceneName = ATSTRINGHASH("rcbandito_general_scene", 0x563AF803);
				}
				else if (followPlayerVehicle->GetVehicleAudioEntity()->GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
				{
					bonnetCamSceneName = ATSTRINGHASH("KOSATKA_BONNET_CAM_SCENE", 0x81E0D389);
				}
				else if(isAircraft)
				{
					bonnetCamSceneName = ATSTRINGHASH("BONNET_CAM_AIRCRAFT_SCENE", 0x664CF87B);
				}
				else if(isBike)
				{
					bonnetCamSceneName = ATSTRINGHASH("BONNET_CAM_BIKE_SCENE", 0x3236F4E7);
				}
				else if(isBicycle)
				{
					bonnetCamSceneName = ATSTRINGHASH("BONNET_CAM_BICYCLE_SCENE", 0xA212AE03);
				}
				else
				{
					bonnetCamSceneName = ATSTRINGHASH("BONNET_CAM_SCENE", 0x420AE17A);
				}

				if(DYNAMICMIXMGR.GetObject<MixerScene>(bonnetCamSceneName) != NULL)
				{
					DYNAMICMIXER.StartScene(bonnetCamSceneName, &sm_VehicleBonnetCamScene);
				}
			}
		}	
		else if(audNorthAudioEngine::IsRenderingFirstPersonVehicleCam() || (isAircraft && naMicrophones::IsSniping()))
		{
			if(sm_VehicleBonnetCamScene)
			{
				sm_VehicleBonnetCamScene->Stop();
				sm_VehicleBonnetCamScene = NULL;
			}

			if(!sm_VehicleInteriorScene)
			{			
				if(isAircraft)
				{	
					bool isPassengerSeat = true;
					s32 seatIndex = followPlayerVehicle->GetSeatManager()->GetPedsSeatIndex(followPlayer);					
					u32 numCabinSeats = 2;

					switch(vehicleNameHash)
					{
						case 0x3E2E4F8A: // TULA
							numCabinSeats = 1;
							break;

						case 0xD35698EF: // MOGUL
							numCabinSeats = 1;
							break;
					}

					if(seatIndex != -1)
					{
						if(seatIndex < numCabinSeats)
						{
							isPassengerSeat = false;
						}
					}
					
					switch(vehicleNameHash)
					{
					case 0xA52F6866: // ALPHAZ1
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("ALPHAZ1_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0xAD7B5E21) : ATSTRINGHASH("ALPHAZ1_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x875D1F93), &sm_VehicleInteriorScene);
						break;

					case 0xFE0A508C: // BOMBUSHKA
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("BOMBUSHKA_INTERIOR_VIEW_AIRCRAFT_PASSENGER_Scene", 0x95007263) : ATSTRINGHASH("BOMBUSHKA_INTERIOR_VIEW_AIRCRAFT_Scene", 0x2EB257A6), &sm_VehicleInteriorScene);
						break;

					case 0x89BA59F5: // HAVOK
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("HAVOK_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x134AB13D) : ATSTRINGHASH("HAVOK_INTERIOR_VIEW_AIRCRAFT_SCENE", 0xE75BFD1A), &sm_VehicleInteriorScene);
						break;

					case 0xC3F25753: // HOWARD
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("HOWARD_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x73CA819F) : ATSTRINGHASH("HOWARD_INTERIOR_VIEW_AIRCRAFT_SCENE", 0xC035FEC5), &sm_VehicleInteriorScene);
						break;

					case 0x11962E49: // Annihilator2
						DYNAMICMIXER.StartScene(ATSTRINGHASH("ANNIHILATOR2_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x432D4669), &sm_VehicleInteriorScene);
						break;

					case 0x96E24857: // MICROLIGHT
						DYNAMICMIXER.StartScene(ATSTRINGHASH("MICROLIGHT_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x1BE19149), &sm_VehicleInteriorScene);
						break;

					case 0x5D56F01B: // MOLOTOK
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("MOLOTOK_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x7593FD98) : ATSTRINGHASH("MOLOTOK_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x15D962DC), &sm_VehicleInteriorScene);
						break;

					case 0xD35698EF: // MOGUL
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("MOGUL_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0xD0EB500) : ATSTRINGHASH("MOGUL_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x77ED8E0C), &sm_VehicleInteriorScene);
						break;

					case 0x3DC92356: // NOKOTA
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("NOKOTA_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0xAA580D13) : ATSTRINGHASH("NOKOTA_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x36B2F355), &sm_VehicleInteriorScene);
						break;

					case 0xAD6065C0: // PYRO
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("PYRO_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0xE184AC63) : ATSTRINGHASH("PYRO_INTERIOR_VIEW_AIRCRAFT_SCENE", 0xA1C58A3A), &sm_VehicleInteriorScene);
						break;

					case 0x9A9EB7DE: // STARLING
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("STARLING_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0xF1AA0734) : ATSTRINGHASH("STARLING_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x45A2D655), &sm_VehicleInteriorScene);
						break;

					case 0xE8983F9F: // SEABREEZE
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("SEABREEZE_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x793AC0EB) : ATSTRINGHASH("SEABREEZE_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x6CC6F902), &sm_VehicleInteriorScene);
						break;

					case 0x3E2E4F8A: // TULA
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("TULA_INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x4EE16B4E) : ATSTRINGHASH("TULA_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x9EECE232), &sm_VehicleInteriorScene);
						break;

					case 0xFCFCB68B: // CARGOBOB
					case 0x60A7EA10: // CARGOBOB2
					case 0x53174EEF: // CARGOBOB3
					case 0x78BC1A3C: // CARGOBOB4
						DYNAMICMIXER.StartScene(ATSTRINGHASH("CARGOBOB_INTERIOR_SCENE", 0x9535FB29), &sm_VehicleInteriorScene);
						break;

					case 0xB79C1BF5: // SHAMAL
					case 0xA4B7B1D4: // SHAMAL3
						DYNAMICMIXER.StartScene(ATSTRINGHASH("SHAMAL_INTERIOR_SCENE", 0xCB8FA3F5), &sm_VehicleInteriorScene);			
						break;

					case 0xFD707EDE: // HUNTER
						DYNAMICMIXER.StartScene(ATSTRINGHASH("HUNTER_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x65A71D9E), &sm_VehicleInteriorScene);	
						break;

					case 0xC5DD6967: // ROGUE
						DYNAMICMIXER.StartScene(ATSTRINGHASH("ROGUE_INTERIOR_VIEW_AIRCRAFT_SCENE", 0x2EC6F888), &sm_VehicleInteriorScene); 
						break;
					
					case 0x250B0C5E: // LUXOR
					case 0xB79F589E: // LUXOR2
					case 0x2A54C47D: // SUPERVOLITO
					case 0x9C5E5644: // SUPERVOLITO2
					case 0xEBC24DF2: // SWIFT
					case 0x4019CB4C: // SWIFT2											
						DYNAMICMIXER.StartScene(isPassengerSeat? ATSTRINGHASH("INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x7EBA8171) : ATSTRINGHASH("INTERIOR_VIEW_AIRCRAFT_SCENE", 0x96E81BB6), &sm_VehicleInteriorScene);
						break;

					default:
						break;	
					}

					if(!sm_VehicleInteriorScene)
					{
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_AIRCRAFT_SCENE", 0x96E81BB6), &sm_VehicleInteriorScene);
					}
				}
				else if(isBike)
				{					
					DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_BIKE_SCENE", 0xF9DFBAF5), &sm_VehicleInteriorScene);
				}
				else if(isBicycle)
				{
					DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_BICYCLE_SCENE", 0xE2000C3E), &sm_VehicleInteriorScene);
				}
				else
				{		
					switch (vehicleNameHash)
					{
					case 0x360A438E: // COG55
					case 0x29FCD3E4: // COG552
					case 0x86FE0B60: // COGNOSCENTI
					case 0xDBF2D57A: // COGNOSCENTI2
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_COG55", 0xF27417DD), &sm_VehicleInteriorScene);
						break;

					case 0xEDC6F847: // BRICKADE
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_BRICKADE", 0x7622EBE), &sm_VehicleInteriorScene);
						break;

					case 0xB472D2B5: // ELLIE
						DYNAMICMIXER.StartScene(ATSTRINGHASH("ELLIE_INTERIOR_VIEW_SCENE", 0xE0698E9A), &sm_VehicleInteriorScene);
						break;

					case 0xB4F32118: // FLASHGT
						DYNAMICMIXER.StartScene(ATSTRINGHASH("FLASHGT_INTERIOR_VIEW_SCENE", 0xA1C16D4C), &sm_VehicleInteriorScene);
						break;

					case 0x71CBEA98: // GB200
						DYNAMICMIXER.StartScene(ATSTRINGHASH("GB200_INTERIOR_VIEW_SCENE", 0x7408304C), &sm_VehicleInteriorScene);
						break;

					case 0x42836BE5: // HOTRING
						DYNAMICMIXER.StartScene(ATSTRINGHASH("HOTRING_INTERIOR_VIEW_SCENE", 0x39DCD571), &sm_VehicleInteriorScene);
						break;

					case 0x3E5BD8D9: // MICHELLI
						DYNAMICMIXER.StartScene(ATSTRINGHASH("MICHELLI_INTERIOR_VIEW_SCENE", 0x20532D68), &sm_VehicleInteriorScene);
						break;

					case 0xE78CC3D9: // REVOLTOR
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_REVOLTER", 0x87EA6740), &sm_VehicleInteriorScene);
						break;

					case 0xF9E67C05: // SQUADDIE
						DYNAMICMIXER.StartScene(ATSTRINGHASH("SQUADDIE_INTERIOR_VIEW_SCENE", 0x91814DD8), &sm_VehicleInteriorScene);
						break;

					case 0x5E4327C8: // WINDSOR
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_WINDSOR", 0xCBD47406), &sm_VehicleInteriorScene);
						break;

					case 0x3201DD49: // Z190
						DYNAMICMIXER.StartScene(ATSTRINGHASH("Z190_INTERIOR_VIEW_SCENE", 0xA8BCDEAB), &sm_VehicleInteriorScene);
						break;

					case 0x8CF5CAE1: // WINDSOR2
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_WINDSOR2", 0xC424529B), &sm_VehicleInteriorScene);
						break;

					case 0x866BCE26: // FACTION3
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_FACTION3", 0xF38CD46E), &sm_VehicleInteriorScene);
						break;

					case 0x7E8F677F: // PROTOTIPO
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_PROTOTIPO", 0xDA144B16), &sm_VehicleInteriorScene);
						break;

					case 0x61E3E6C4: // 770
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_770", 0x7D8C1136), &sm_VehicleInteriorScene);
						break;

					case 0xBEDC8A50: // 811
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_811", 0xE31B09DF), &sm_VehicleInteriorScene);
						break;

					case 0x1324E960: // STAFFORD
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_STAFFORD", 0x1425CE20), &sm_VehicleInteriorScene);
						break;

					case 0x82CAC433: // TUG
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_TUG", 0xBA1842BD), &sm_VehicleInteriorScene);
						break;

					case 0x19DD9ED1: // NIGHTSHARK
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE_NIGHTSHARK", 0xCB8B65C7), &sm_VehicleInteriorScene);
						break;

					default:
						break;
					}

					if(!sm_VehicleInteriorScene)
					{
						DYNAMICMIXER.StartScene(ATSTRINGHASH("INTERIOR_VIEW_SCENE", 0xA3467F8D), &sm_VehicleInteriorScene);
					}
				}	
			}
		}
		else
		{
			if(sm_VehicleInteriorScene)
			{
				bool safeToShutdownInteriorScene = true;

				if(followPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
				{
					// Delay switching off the interior scene until the ped starts opening the appropriate door
					const s32 pedSeatIndex = followPlayerVehicle->GetSeatManager()->GetPedsSeatIndex(followPlayer);

					if(followPlayerVehicle->IsSeatIndexValid(pedSeatIndex))
					{
						const s32 entryPointIndex = followPlayerVehicle->GetDirectEntryPointIndexForSeat(pedSeatIndex);

						if(followPlayerVehicle->IsEntryIndexValid(entryPointIndex))
						{
							const CCarDoor* exitDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(followPlayerVehicle, entryPointIndex);

							if(exitDoor && exitDoor->GetIsClosed(0.1f))
							{
								safeToShutdownInteriorScene = false;
							}
						}
					}
				}

				if(safeToShutdownInteriorScene)
				{
					sm_VehicleInteriorScene->Stop();
					sm_VehicleInteriorScene = NULL;
				}
			}

			if(sm_VehicleBonnetCamScene)
			{
				sm_VehicleBonnetCamScene->Stop();
				sm_VehicleBonnetCamScene = NULL;
			}
		}
					
		if(audNorthAudioEngine::IsRenderingHoodMountedVehicleCam())
		{
			sm_PlayerVehicleOpenness = 1.0f;
		}
		else if(isBike || isBicycle)
		{
			CPed* pPed = CGameWorld::FindLocalPlayer();
			if(pPed && pPed->GetHelmetComponent() &&  pPed->GetHelmetComponent()->IsHelmetEnabled())
			{
				sm_PlayerVehicleOpenness = 0.7f;
			}
			else
			{
				sm_PlayerVehicleOpenness = 1.0f;
			}
		}
		else
		{
			sm_PlayerVehicleOpenness = sm_OpennessSmoother.CalculateValue(followPlayerVehicle->GetVehicleAudioEntity()->GetOpenness(false), fwTimer::GetTimeStep());
		}

		// Vehicles may have engines inside the cabin (eg. rear engine sports cars) - these should stay unoccluded when in first person mode
		playerVehicleEngineOpenness = Max(sm_PlayerVehicleOpenness, followPlayerVehicle->GetVehicleAudioEntity()->GetInteriorViewEngineOpenness());

		if(!isBike && !isBicycle && !sm_InCarViewWind)
		{
			audSoundInitParams initParams; 
			initParams.Tracker = followPlayerVehicle->GetPlaceableTracker();
			initParams.EnvironmentGroup = followPlayerVehicle->GetVehicleAudioEntity()->GetEnvironmentGroup();
			initParams.SetVariableValue(ATSTRINGHASH("openness", 0x2721BC39), sm_PlayerVehicleOpenness);

			if(vehicleNameHash == ATSTRINGHASH("Thruster", 0x58CDAF30))
			{
				audSoundSet thrusterSounds;

				if(thrusterSounds.Init(ATSTRINGHASH("thruster_turbine_sounds", 0x9C4F5C78)))
				{
					g_FrontendAudioEntity.CreateAndPlaySound_Persistent(thrusterSounds.Find(ATSTRINGHASH("wind", 0x35369828)),&sm_InCarViewWind,&initParams);
				}
			}
			else
			{
				g_FrontendAudioEntity.CreateAndPlaySound_Persistent(ATSTRINGHASH("INTERIOR_VIEW_WIND_OPENNESS", 0x5DEBE1D0),&sm_InCarViewWind,&initParams);
			}
		}
		if( sm_InCarViewWind )
		{
			// Duster is fully open but has wind shields in front of the cockpit seat. Full interior wind sounds a bit full-on.
			f32 interiorViewWindOpennessScalar = (followPlayerVehicle->GetVehicleAudioEntity()->GetVehicleModelNameHash() == ATSTRINGHASH("DUSTER", 0x39D6779E) ? 0.5f : 1.0f);
			sm_InCarViewWind->FindAndSetVariableValue(ATSTRINGHASH("openness", 0x2721BC39), sm_PlayerVehicleOpenness * interiorViewWindOpennessScalar);
            REPLAY_ONLY(sm_InCarViewWind->SetRequestedVolume(audNorthAudioEngine::IsRenderingReplayFreeCamera()? g_SilenceVolume : 0.f);)
		}
	}
	else 
	{
		sm_PlayerVehicleOpenness = sm_OpennessSmoother.CalculateValue(1.f, fwTimer::GetTimeStep());

		if(sm_VehicleInteriorScene)
		{
			sm_VehicleInteriorScene->Stop();
			sm_VehicleInteriorScene = NULL;
		}
		if(sm_InCarViewWind)
		{
			sm_InCarViewWind->StopAndForget();
			sm_InCarViewWind = NULL;
		}
		if(sm_VehicleFirstPersonTurretScene)
		{
			sm_VehicleFirstPersonTurretScene->Stop();
			sm_VehicleFirstPersonTurretScene = NULL;
		}
		if (sm_VehicleBonnetCamScene)
		{
			sm_VehicleBonnetCamScene->Stop();
			sm_VehicleBonnetCamScene = NULL;
		}
		if (sm_VehicleGeneralScene)
		{
			sm_VehicleGeneralScene->Stop();
			sm_VehicleGeneralScene = NULL;
		}
	}

	if(sm_VehicleInteriorScene)
	{
		sm_VehicleInteriorScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), 1.0f - sm_PlayerVehicleOpenness);
		sm_VehicleInteriorScene->SetVariableValue(ATSTRINGHASH("engineapply", 0xB378F6DF), 1.0f - playerVehicleEngineOpenness);
	}

	f32 highSpeedApplyFactor = 0.0f;
	f32 highSpeedApplyFactorSmoothed = 0.0f;
	f32 fwdSpeed = 0.0f;
	f32 fwdSpeedRatio = 0.0f;

	if(followPlayerVehicle)
	{
		if(!sm_HighSpeedScene)
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("HIGH_SPEED_SCENE", 0xE371E209), &sm_HighSpeedScene);
		}

		if(followPlayerVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		{
			fwdSpeed = DotProduct(followPlayerVehicle->GetVehicleAudioEntity()->GetCachedVelocity(), VEC3V_TO_VECTOR3(followPlayerVehicle->GetTransform().GetB()));

			if(followPlayerVehicle->m_Transmission.GetDriveMaxVelocity() > 0.0f)
			{
				fwdSpeedRatio = Abs(fwdSpeed)/followPlayerVehicle->m_Transmission.GetDriveMaxVelocity();
			}

			 if(followPlayerVehicle->m_Transmission.GetDriveMaxVelocity() >= sm_HighSpeedReqVehicleVelocity && 
				followPlayerVehicle->m_Transmission.GetGear() >= followPlayerVehicle->m_Transmission.GetNumGears() - sm_HighSpeedMaxGearOffset)
			 {
				 // Apply factor ranges from 90%-98% of top speed
				 highSpeedApplyFactor = Clamp((fwdSpeedRatio - sm_HighSpeedApplyStart)/(sm_HighSpeedApplyEnd - sm_HighSpeedApplyStart), 0.0f, 1.0f);
			 }
		}
	}
	else if(sm_HighSpeedScene)
	{
		sm_HighSpeedScene->Stop();
		sm_HighSpeedScene = NULL;
	}

	if(sm_HighSpeedScene)
	{
		highSpeedApplyFactorSmoothed = sm_HighSpeedSceneApplySmoother.CalculateValue(highSpeedApplyFactor, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		sm_HighSpeedScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), highSpeedApplyFactorSmoothed);
	}

	if(sm_TriggerStuntRaceSpeedBoostScene)
	{
		if(!sm_StuntRaceSpeedUpScene)
		{
			if(sm_StuntRaceVehicleType == AUD_VEHICLE_HELI || sm_StuntRaceVehicleType == AUD_VEHICLE_PLANE)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("DLC_AIRRACE_Powerup_Speed_Up_Scene", 0x68133539), &sm_StuntRaceSpeedUpScene);
			}
			else
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("DLC_STUNT_Powerup_Speed_Up_Scene", 0x7835F632), &sm_StuntRaceSpeedUpScene);
			}
		}
		
		sm_TriggerStuntRaceSpeedBoostScene = false;
	}

	if(sm_TriggerStuntRaceSlowDownScene)
	{
		if(!sm_StuntRaceSlowDownScene)
		{
			if(sm_StuntRaceVehicleType == AUD_VEHICLE_HELI || sm_StuntRaceVehicleType == AUD_VEHICLE_PLANE)
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("DLC_AIRRACE_Powerup_Speed_Down_Scene", 0x6D62B92), &sm_StuntRaceSlowDownScene);
			}
			else
			{
				DYNAMICMIXER.StartScene(ATSTRINGHASH("DLC_STUNT_Powerup_Speed_Down_Scene", 0xCE7A2CB), &sm_StuntRaceSlowDownScene);
			}
		}
		
		sm_TriggerStuntRaceSlowDownScene = false;
	}

#if __BANK
	sm_HighSpeedSceneApplySmoother.SetRates(1.0f/sm_InvHighSpeedSmootherIncreaseRate, 1.0f/sm_InvHighSpeedSmootherDecreaseRate);

	if(g_DebugHighSpeedEffect && followPlayerVehicle)
	{
		char tempString[128];
		f32 xCoord = 0.1f;
		f32 yCoord = 0.1f;
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), "High Speed Effect");
		yCoord += 0.02f;
		xCoord += 0.05f;

		sprintf(tempString, "Apply Factor: %.02f", highSpeedApplyFactor);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		sprintf(tempString, "Apply Factor Smoothed: %.02f", highSpeedApplyFactorSmoothed);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		sprintf(tempString, "Fwd Speed: %.02f", fwdSpeed);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		sprintf(tempString, "Fwd Speed Ratio: %.02f", fwdSpeedRatio);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;

		sprintf(tempString, "Gear: %d/%d", followPlayerVehicle->m_Transmission.GetGear(), followPlayerVehicle->m_Transmission.GetNumGears());
		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;
	}
#endif
	
	sm_PlayerVehicleProximityRatio = 0.0f;
	sm_LowLatencyWaveSlotManager.Update();
	sm_HighQualityWaveSlotManager.Update();
	sm_StandardWaveSlotManager.Update();

	// Stop any DSP voices that we're not using any more
	for(u32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1; loop++)
	{
		if(g_AudioEngine.GetEnvironment().IsEngineSubmixFree(loop))
		{
			if(sm_VehicleEngineSubmixes[loop].submixSound || sm_VehicleEngineSubmixes[loop].vehicleType != AUD_VEHICLE_NONE)
			{
				g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(loop)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, g_NullSoundHash);
				g_FrontendAudioEntity.StopAndForgetSounds(sm_VehicleEngineSubmixes[loop].submixSound);
				sm_VehicleEngineSubmixes[loop].submixSound = NULL;
				sm_VehicleEngineSubmixes[loop].vehicleType = AUD_VEHICLE_NONE;
				sm_VehicleEngineSubmixes[loop].isGranular = false;
			}
		}

		if(g_AudioEngine.GetEnvironment().IsExhaustSubmixFree(loop))
		{
			if(sm_VehicleExhaustSubmixes[loop].submixSound || sm_VehicleExhaustSubmixes[loop].vehicleType != AUD_VEHICLE_NONE)
			{
				g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(loop)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, g_NullSoundHash);
				g_FrontendAudioEntity.StopAndForgetSounds(sm_VehicleExhaustSubmixes[loop].submixSound);
				sm_VehicleExhaustSubmixes[loop].submixSound = NULL;
				sm_VehicleExhaustSubmixes[loop].vehicleType = AUD_VEHICLE_NONE;
				sm_VehicleEngineSubmixes[loop].isGranular = false;
			}
		}
	}

	// Safety mechanism incase the player car is starving but then cancels the load request before it finishes
	sm_FramesSincePlayerVehicleStarvation = Max(0, sm_FramesSincePlayerVehicleStarvation - 1);

	// As soon as the player starts entering a vehicle, we want to start streaming in bank data
	sm_PlayerPedTargetVehicle = NULL;
	const CPed* localPlayer = g_PlayerSwitch.IsActive() && camInterface::GetGameplayDirector().GetSwitchHelper()? camInterface::GetGameplayDirector().GetSwitchHelper()->GetNewPed() : CGameWorld::FindLocalPlayer();

	if(localPlayer)
	{
		if(localPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskEnterVehicle *task = (CTaskEnterVehicle*)localPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);

			if(task)
			{
				sm_PlayerPedTargetVehicle = task->GetVehicle();
			}
		}
	}

#if __BANK
	if(g_DebugDSPParams)
	{
		char tempString[128];
		f32 xCoord = 0.4f;
		f32 yCoord = 0.1f;

		for(u32 loop = 0; loop < EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1; loop++)
		{
			u32 numVoices = audDriver::GetMixer()->GetActiveVoiceCount(audDriver::GetMixer()->GetSubmixIndex(g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(loop)));
			sprintf(tempString, "Engine %d:%s - %d (%s)", loop, sm_VehicleEngineSubmixes[loop].submixSound && sm_VehicleEngineSubmixes[loop].submixSound->GetPlayState() == AUD_SOUND_PLAYING? "ON":"OFF", numVoices, GetVehicleTypeName(sm_VehicleEngineSubmixes[loop].vehicleType));
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);

			numVoices = audDriver::GetMixer()->GetActiveVoiceCount(audDriver::GetMixer()->GetSubmixIndex(g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(loop)));
			sprintf(tempString, "Exhaust %d:%s - %d (%s)", loop, sm_VehicleExhaustSubmixes[loop].submixSound && sm_VehicleExhaustSubmixes[loop].submixSound->GetPlayState() == AUD_SOUND_PLAYING? "ON":"OFF", numVoices, GetVehicleTypeName(sm_VehicleExhaustSubmixes[loop].vehicleType));
			grcDebugDraw::Text(Vector2(xCoord + 0.22f, yCoord), Color32(255,255,255), tempString);

			yCoord += 0.02f;
		}
	}

	if(g_RenderSlotManager)
	{
		sm_StandardWaveSlotManager.DebugDraw(0.1f, 0.1f);
		sm_HighQualityWaveSlotManager.DebugDraw(0.1f, 0.5f);
		sm_LowLatencyWaveSlotManager.DebugDraw(0.1f, 0.7f);
	}

	if(g_DrawSuperDummyRadius)
	{
		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		grcDebugDraw::Sphere(listenerPosition, sqrtf((f32)g_SuperDummyRadiusSq), Color32(255,255,255), false, 1, 128, true);
	}
#endif
}

// ----------------------------------------------------------------
// UpdateClass
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateClass()
{
#if RSG_BANK
	if (g_CopyOverridenLeakageFromCurrentSettings)
	{
		g_OverridenLeakageVolMin = g_RadioOpennessVols[g_OverriddenVehicleRadioLeakage][0];
		g_OverridenLeakageVolMax = g_RadioOpennessVols[g_OverriddenVehicleRadioLeakage][1];

		g_OverridenLeakageLpfLinearMin = g_RadioLPFLinear[g_OverriddenVehicleRadioLeakage][0];
		g_OverridenLeakageLpfLinearMax = g_RadioLPFLinear[g_OverriddenVehicleRadioLeakage][1];

		g_OverridenLeakageRolloffMin = g_RadioRolloffs[g_OverriddenVehicleRadioLeakage][0];
		g_OverridenLeakageRolloffMax = g_RadioRolloffs[g_OverriddenVehicleRadioLeakage][1];

		g_OverridenLeakageHPFCutoff = g_RadioHPFCutoffs[g_OverriddenVehicleRadioLeakage];
		g_OverridenLeakageMaxRadius = Sqrtf(g_MaxRadiusForAmbientRadio2[g_OverriddenVehicleRadioLeakage]);

		g_CopyOverridenLeakageFromCurrentSettings = false;
	}

	if (CPed* playerPed = FindPlayerPed())
	{
		if (CVehicle* playerVehicle = playerPed->GetMyVehicle())
		{
			if (g_SmashPlayerSiren)
			{
				playerVehicle->GetVehicleAudioEntity()->SmashSiren(true);
			}

			if (g_FixPlayerSiren)
			{
				playerVehicle->GetVehicleAudioEntity()->FixSiren();
			}
		}
	}

	g_SmashPlayerSiren = false;
	g_FixPlayerSiren = false;
#endif

	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	sm_IsInCarMeet = pIntInst && pIntInst->GetModelNameHash() == ATSTRINGHASH("tr_tuner_car_meet", 0xD813540);		
}

const char* audVehicleAudioEntity::GetVehicleTypeName(audVehicleType type)
{
	switch(type)
	{
		case AUD_VEHICLE_TRAIN:
			return "Train";
		case AUD_VEHICLE_PLANE:
			return "Plane";
		case AUD_VEHICLE_HELI:
			return "Heli";
		case AUD_VEHICLE_CAR:
			return "Car";
		case AUD_VEHICLE_BOAT:
			return "Boat";
		case AUD_VEHICLE_BICYCLE:
			return "Bicycle";
		case AUD_VEHICLE_TRAILER:
			return "Trailer";
		case AUD_VEHICLE_ANY:
			return "Any";
		case AUD_VEHICLE_NONE:
			return "None";
		default:
			return "Unknown";
	}
}

bool audVehicleAudioEntity::IsPlayerOnFootOrOnBicycle()
{
	bool isOnFootOrOnBicycle = true;
	const CPed* playerPed = FindPlayerPed();

	if(playerPed && playerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		const CVehicle* playerVehicle = playerPed->GetMyVehicle();

		if(playerVehicle && !playerVehicle->InheritsFromBicycle())
		{
			isOnFootOrOnBicycle = false;
		}
	}

	return isOnFootOrOnBicycle;
}

void audVehicleAudioEntity::UpdateActivationRanges()
{
#if __DEV
	PF_SET(NumVehiclesInActivationRange, sm_NumVehiclesInActivationRange);
	PF_SET(NumVehiclesInGranularRange, sm_NumVehiclesInGranularActivationRange);
#endif

	sm_NumVehiclesInGranularActivationRangeLastFrame = sm_NumVehiclesInGranularActivationRange;
	sm_NumVehiclesInActivationRangeLastFrame = sm_NumVehiclesInActivationRange;

	if(sm_NumVehiclesInGranularActivationRange > g_MaxGranularEngines)
	{
		sm_GranularActivationRangeScale -= (fwTimer::GetTimeStep() * g_GranularActivationScaleDecreaseRate);
	}
	else if(sm_NumVehiclesInGranularActivationRange < g_MaxGranularEngines)
	{
		sm_GranularActivationRangeScale += (fwTimer::GetTimeStep() * g_GranularActivationScaleIncreaseRate);
	}

	sm_NumVehiclesInGranularActivationRange = 0;
	sm_GranularActivationRangeScale = Clamp(sm_GranularActivationRangeScale, 0.02f, g_MaxGranularRangeScale);
	//audAssert(CalculateNumVehiclesUsingSubmixes(AUD_VEHICLE_ANY, true, true) <= g_MaxGranularEngines);

	if(sm_NumVehiclesInActivationRange > g_MaxVehicleSubmixes)
	{
		sm_ActivationRangeScale -= (fwTimer::GetTimeStep() * g_ActivationScaleDecreaseRate);
	}
	else if(sm_NumVehiclesInActivationRange < g_MaxVehicleSubmixes)
	{
		sm_ActivationRangeScale += (fwTimer::GetTimeStep() * g_ActivationScaleIncreaseRate);
	}

	sm_NumVehiclesInActivationRange = 0;
	sm_ActivationRangeScale = Clamp(sm_ActivationRangeScale, 0.1f, g_MaxRealRangeScale);

	for(u32 quadrant = 0; quadrant < NumQuadrants; quadrant++)
	{
		if(sm_NumVehiclesInDummyRange[quadrant] > g_MaxDumyVehiclesPerQuadrant)
		{
			sm_DummyRangeScale[quadrant] -= fwTimer::GetTimeStep() * g_DummyActivationScaleDecreaseRate;
		}
		else if(sm_NumVehiclesInDummyRange[quadrant] < g_MaxDumyVehiclesPerQuadrant)
		{
			sm_DummyRangeScale[quadrant] += fwTimer::GetTimeStep() * g_DummyActivationScaleIncreaseRate;
		}

		sm_DummyRangeScale[quadrant] = Clamp(sm_DummyRangeScale[quadrant], 0.1f, g_MaxDummyRangeScale);
	}

	sysMemSet(sm_NumVehiclesInDummyRange, 0, sizeof(u32) * NumQuadrants);
	sm_NumDummyVehiclesLastFrame = sm_NumDummyVehiclesCounter;
	sm_NumDummyVehiclesCounter = 0;

	for(u32 quadrant = 0; quadrant < NumQuadrants; quadrant++)
	{
		if(sm_NumVehiclesInWaterSlapRadius[quadrant] > g_MaxWaterSlapVehiclesPerQuadrant)
		{
			sm_WaterSlapRadiusScalar[quadrant] -= fwTimer::GetTimeStep();
		}
		else if(sm_NumVehiclesInWaterSlapRadius[quadrant] < g_MaxWaterSlapVehiclesPerQuadrant)
		{
			sm_WaterSlapRadiusScalar[quadrant] += fwTimer::GetTimeStep();
		}

		sm_WaterSlapRadiusScalar[quadrant] = Clamp(sm_WaterSlapRadiusScalar[quadrant], 0.0f, 1.0f);
	}

	sysMemSet(sm_NumVehiclesInWaterSlapRadius, 0, sizeof(u32) * NumQuadrants);

	for(u32 quadrant = 0; quadrant < NumQuadrants; quadrant++)
	{
		if(sm_NumVehiclesInRainRadius[quadrant] > g_MaxRainVehiclesPerQuadrant)
		{
			sm_RainRadiusScalar[quadrant] -= fwTimer::GetTimeStep();
		}
		else if(sm_NumVehiclesInRainRadius[quadrant] < g_MaxRainVehiclesPerQuadrant)
		{
			sm_RainRadiusScalar[quadrant] += fwTimer::GetTimeStep();
		}

		sm_RainRadiusScalar[quadrant] = Clamp(sm_RainRadiusScalar[quadrant], 0.0f, 1.0f);
	}
	
	sysMemSet(sm_NumVehiclesInRainRadius, 0, sizeof(u32) * NumQuadrants);

#if __DEV
	PF_SET(GranularRadiusScalar, sm_GranularActivationRangeScale);
	PF_SET(RealRadiusScalar, sm_ActivationRangeScale);
	PF_SET(DummyRadiusScalarQ1, sm_DummyRangeScale[0]);
	PF_SET(DummyRadiusScalarQ2, sm_DummyRangeScale[1]);
	PF_SET(DummyRadiusScalarQ3, sm_DummyRangeScale[2]);
	PF_SET(DummyRadiusScalarQ4, sm_DummyRangeScale[3]);
	PF_SET(RainRadiusScalarQ1, sm_RainRadiusScalar[0]);
	PF_SET(RainRadiusScalarQ2, sm_RainRadiusScalar[1]);
	PF_SET(RainRadiusScalarQ3, sm_RainRadiusScalar[2]);
	PF_SET(RainRadiusScalarQ4, sm_RainRadiusScalar[3]);
	PF_SET(WaterSlapRadiusScalarQ1, sm_WaterSlapRadiusScalar[0]);
	PF_SET(WaterSlapRadiusScalarQ2, sm_WaterSlapRadiusScalar[1]);
	PF_SET(WaterSlapRadiusScalarQ3, sm_WaterSlapRadiusScalar[2]);
	PF_SET(WaterSlapRadiusScalarQ4, sm_WaterSlapRadiusScalar[3]);
#endif
}

// ----------------------------------------------------------------
// Stop road noise sounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::StopRoadNoiseSounds()
{
	StopAndForgetSounds(m_RoadNoiseFast, m_RoadNoiseHeavy, m_RoadNoiseWet, m_RoadNoiseRidged, m_RoadNoiseNPC, m_WheelDetailLoop);
	StopAndForgetSounds(m_RoadNoiseRidgedPulse, m_DirtyWheelLoop, m_GlassyWheelLoop, m_CaterpillarTrackLoop);
	StopAndForgetSounds(m_MainSkidLoop, m_SideSkidLoop, m_UndersteerSkidLoop, m_WheelSpinLoop, m_WetWheelSpinLoop, m_DriveWheelSlipLoop, m_WetSkidLoop);

	for(s32 i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		if(m_ShallowWaterSounds[i])
		{
			m_ShallowWaterSounds[i]->StopAndForget();
		}
	}
}

// ----------------------------------------------------------------
// Update any road noise sounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateRoadNoiseSounds(audVehicleVariables& vehicleVariables)
{	
	PF_FUNC(UpdateRoadNoise);

	if(HasEntityVariableBlock())
	{
		SetEntityVariable(ATSTRINGHASH("speed", 0xf997622b), Abs(vehicleVariables.fwdSpeed));
	}
	
	// Helicopters apparently have wheels...
	if(m_Vehicle->GetNumWheels() == 0)
	{
		return; 
	}

	if(m_VehicleType == AUD_VEHICLE_HELI)
	{
		CalculateWheelMaterials(vehicleVariables);
		return;
	}

#if __BANK
	if(g_DisableRoadNoise)
	{
		StopRoadNoiseSounds();
		return;
	}
#endif

	if(m_Vehicle->InheritsFromAmphibiousQuadBike() && ((CAmphibiousQuadBike*)m_Vehicle)->IsWheelsFullyRaised())
	{
		StopRoadNoiseSounds();
		return;
	}

	if(m_Vehicle->InheritsFromSubmarineCar() && !((CSubmarineCar*)m_Vehicle)->AreWheelsActive())
	{
		StopRoadNoiseSounds();
		return;
	}

	if(IsSeaPlane() && CalculateIsInWater())
	{
		StopRoadNoiseSounds();
		return;
	}

	if(GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB) && m_Vehicle->GetSpecialFlightModeRatio() > 0.f)
	{
		StopRoadNoiseSounds();
		return;
	}
	
	if(IsReal())
	{
		// no smoothing in slow mo
		if(audNorthAudioEngine::IsInSlowMo())
		{
			m_MainSkidSmoother.SetRates(1.f, 1.f);
			m_LateralSkidSmoother.SetRates(1.f, 1.f);
		}
		else
		{
			m_MainSkidSmoother.SetRates(g_MainSkidSmootherIncrease * 0.001f, g_MainSkidSmootherDecrease * 0.001f);
			m_LateralSkidSmoother.SetRates(g_LateralSkidSmootherIncrease * 0.001f, g_LateralSkidSmootherDecrease * 0.001f);
		}

		CalculateWheelMaterials(vehicleVariables);
		UpdateWheelDetail(vehicleVariables);
		UpdateWheelDirtyness(vehicleVariables);
		UpdateCaterpillarTracks(vehicleVariables);

		if(!(m_Vehicle->pHandling->mFlags & MF_HAS_TRACKS))
		{
			UpdateSkids(vehicleVariables);

			if (!IsToyCar())
			{
				UpdateWheelWater(vehicleVariables);
			}
			
			UpdateRidgedSurfaces(vehicleVariables);
			UpdateWheelSpin(vehicleVariables);
		}
	}
	else
	{
		//invalidate the stored material settings, we'll always play concrete for dummy cars
		m_CachedMaterialSettings = NULL;
	}

	// Special pass-by loops for Franklin's special ability
	if(!m_IsPlayerVehicle && g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeFranklin)
	{
		const u32 relativeSpeedVar = ATSTRINGHASH("relativeSpeed", 0x842B383);
		const u32 fwdSpeedRatioVar = ATSTRINGHASH("fwdSpeedRatio", 0xB10DFAB9);

		if(!m_FranklinSpecialPassby)
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.SetVariableValue(relativeSpeedVar, 0.f);
			initParams.SetVariableValue(fwdSpeedRatioVar, 0.f);
			CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetSpecialAbilitySounds().Find("Franklin_Passby"), &m_FranklinSpecialPassby, &initParams);
		}
		else
		{
			const f32 relativeSpeed = (m_CachedVehicleVelocity - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetListenerVelocity())).Mag();
			m_FranklinSpecialPassby->FindAndSetVariableValue(relativeSpeedVar, relativeSpeed);
			m_FranklinSpecialPassby->FindAndSetVariableValue(fwdSpeedRatioVar, vehicleVariables.fwdSpeedRatio);
		}
	}
	else
	{
		StopAndForgetSounds(m_FranklinSpecialPassby);
	}

	UpdateFreewayBumps(vehicleVariables);
	UpdateFastRoadNoise(vehicleVariables);
	UpdateNPCRoadNoise(vehicleVariables);
}

// ----------------------------------------------------------------
// Calculate wheel materials
// ----------------------------------------------------------------
void audVehicleAudioEntity::CalculateWheelMaterials(audVehicleVariables& params)
{
	phMaterialMgr::Id mainWheelMaterial = 0;
	const f32 fwdSpeedVolDb = audDriverUtil::ComputeDbVolumeFromLinear(Min(0.5f, params.fwdSpeedRatio));
	const f32 massVolDb = Clamp(audDriverUtil::ComputeDbVolumeFromLinear(m_Vehicle->pHandling->m_fMass / 1500.f), -5.f, 12.f);

	for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
	{
		phMaterialMgr::Id matId = PGTAMATERIALMGR->UnpackMtlId(m_Vehicle->GetWheel(i)->GetMaterialId());

		// Did this wheel hit another vehicle? If so, check if the vehicle wants to overwrite the specified material
		CPhysical* contactEntity = m_Vehicle->GetWheel(i)->GetHitPhysical();

		if(contactEntity)
		{
			CollisionMaterialSettings* collisionSettings = g_audCollisionMaterials[static_cast<s32>(matId)];

			if(collisionSettings)
			{
				collisionSettings = audCollisionAudioEntity::GetMaterialOverride(contactEntity, collisionSettings, m_Vehicle->GetWheel(i)->GetHitComponent());

				if(collisionSettings)
				{
					if(collisionSettings->MaterialID >= 0 && collisionSettings->MaterialID < (phMaterialMgr::Id)g_audCollisionMaterials.GetCount())
					{
						matId = collisionSettings->MaterialID;
					}
				}
			}
		}
		else if(ShouldForceSnow())
		{
			CollisionMaterialSettings* collisionSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_SNOW_COMPACT", 0x10AFB64F));

			if(collisionSettings && collisionSettings->MaterialID >= 0 && collisionSettings->MaterialID < (phMaterialMgr::Id)g_audCollisionMaterials.GetCount())
			{
				matId = collisionSettings->MaterialID;
			}
		}

		// preserve the actual material so that we can do component specific audio (head/torso etc) but
		// for history purposes treat any ped material as one
		phMaterialMgr::Id historyMatId = matId;

		if(PGTAMATERIALMGR->GetIsPed(matId))
		{
			historyMatId = g_PedMaterialId;

			// ensure we play a ped tyre bump
			naAssertf(matId<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Out of bounds matid");
			const s32 matId32=static_cast<s32>(matId);
			g_audCollisionMaterials[matId32]->TyreBump = ATSTRINGHASH("TYRE_BUMP_PED", 0x3421B9D8);
		}

		// only for four wheels
		if((i == 0 || i == 1 || i == m_Vehicle->GetNumWheels() - 2 || i == m_Vehicle->GetNumWheels() -1) 
			&& m_Vehicle->GetWheel(i)->GetIsTouching())
		{
			// only play tyrebumps for cars the right way up and moving a little
			if(m_VehicleType != AUD_VEHICLE_HELI && m_VehicleType != AUD_VEHICLE_BICYCLE && !m_Vehicle->IsUpsideDown() && Abs(params.fwdSpeed) > 0.1f)
			{
				if(m_LastMaterialIds[i] != ~0U && m_LastTyreBumpTimes[i/2] + 250 < fwTimer::GetTimeInMilliseconds())
				{
					naAssertf(m_LastMaterialIds[i]<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "out of bounds");

					if(m_LastMaterialIds[i] != historyMatId && matId < (u32)g_audCollisionMaterials.GetCount() && m_LastMaterialIds[i] < (u32)g_audCollisionMaterials.GetCount() && g_audCollisionMaterials[static_cast<u32>(matId)] && g_audCollisionMaterials[static_cast<u32>(m_LastMaterialIds[i])])
					{
						if(g_audCollisionMaterials[static_cast<u32>(matId)]->TyreBump == ATSTRINGHASH("TYRE_BUMP_PED", 0x3421B9D8) || (g_audCollisionMaterials[static_cast<u32>(matId)]->TyreBump != g_NullSoundHash && g_audCollisionMaterials[static_cast<u32>(m_LastMaterialIds[i])]->TyreBump != g_NullSoundHash))
						{
							// material has changed - play a tyre bump
							audSoundInitParams initParams;
							initParams.EnvironmentGroup = m_EnvironmentGroup;
							u32 soundHash = g_audCollisionMaterials[static_cast<u32>(matId)]->TyreBump;
							if(IsToyCar())
							{
								soundHash = ATSTRINGHASH("RC_BANDITTO_TYRE_BUMPS_MULTI", 0xAA32FAE1);
							}
							// only boost volume based on mass for interesting tyre bumps
							else if(soundHash == ATSTRINGHASH("TYRE_BUMPS_CONCRETE", 0xE6F65DED))
							{
								initParams.Volume = fwdSpeedVolDb;
								if(m_Vehicle->GetWheel(i)->GetTyreHealth()<=0.f)
								{
									soundHash = ATSTRINGHASH("STREET_PROPS_CAR_WHEEL", 0x2940F3DC);
								}
							}
							else if(g_audCollisionMaterials[static_cast<u32>(matId)]->TyreBump == ATSTRINGHASH("TYRE_BUMP_PED", 0x3421B9D8))
							{
								// ped crunches only affected by vehicle mass
								initParams.Volume = Clamp(massVolDb, -3.f, 6.f);
								if(m_Vehicle->GetWheel(i)->GetHitPhysical() && m_Vehicle->GetWheel(i)->GetHitPhysical()->GetType() == ENTITY_TYPE_PED)
								{
									// break a bone?
									if(audEngineUtil::ResolveProbability(0.4f))
									{
										((CPed*)m_Vehicle->GetWheel(i)->GetHitPhysical())->GetPedAudioEntity()->TriggerBoneBreak((RagdollComponent)m_Vehicle->GetWheel(i)->GetHitComponent());
									}
								}
							}
							else
							{
								initParams.Volume = fwdSpeedVolDb + massVolDb;
							}

							Vector3 pos;
							GetCachedWheelPosition(i, pos, params);
							initParams.Position = pos;
							CreateAndPlaySound(soundHash, &initParams);
							m_LastTyreBumpTimes[i/2] = fwTimer::GetTimeInMilliseconds();
						}
					}
				}
			}
		}

		m_LastMaterialIds[i] = historyMatId;
	}

	u32 maxWheelsUsingMaterial = 0;
	atFixedArray<WheelMaterialUsage, NUM_VEH_CWHEELS_MAX> wheelMaterialUsage;

	// We're only playing one material per car, so we want to select the one that is being used by the majority of wheels. In the event
	// that we have multiple materials being used by the same number of wheels, we fall back to the priority system. First of all, count up
	// what materials are being used, and by how many wheels.
	for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
	{
		bool alreadyExists = false;
		CollisionMaterialSettings* materialSettings = g_audCollisionMaterials[static_cast<u32>(m_LastMaterialIds[i])];

		if(materialSettings)
		{
			for(u32 loop = 0; loop < wheelMaterialUsage.GetCount(); loop++)
			{
				if(wheelMaterialUsage[loop].materialSettings == materialSettings)
				{
					alreadyExists = true;
					wheelMaterialUsage[loop].numWheels++;
					maxWheelsUsingMaterial = Max(wheelMaterialUsage[loop].numWheels, maxWheelsUsingMaterial);
				}
			}

			if(!alreadyExists)
			{
				WheelMaterialUsage materialUsageEntry;
				materialUsageEntry.materialSettings = materialSettings;
				materialUsageEntry.numWheels = 1;
				materialUsageEntry.wheelIndex = i;
				wheelMaterialUsage.Push(materialUsageEntry);
				maxWheelsUsingMaterial = Max(materialUsageEntry.numWheels, maxWheelsUsingMaterial);
			}
		}
	}
	
	// Now get rid of any materials that aren't being used by the max number of wheels
	for(s32 i = 0; i < wheelMaterialUsage.GetCount(); )
	{
		if(wheelMaterialUsage[i].numWheels < maxWheelsUsingMaterial)
		{
			wheelMaterialUsage.Delete(i);
		}
		else
		{
			i++;
		}
	}

	s32 loudestSurface = -1;
	s32 wheelIndexToUse = -1;

	// Check for burnout materials, as they take priority
	for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
	{
		if(m_Vehicle->GetWheel(i)->GetDynamicFlags().IsFlagSet(WF_BURNOUT))
		{
			CollisionMaterialSettings* materialSettings = g_audCollisionMaterials[static_cast<u32>(m_LastMaterialIds[i])];

			if(materialSettings)
			{
				u8 prio = FindSkidSurfacePriority(materialSettings->SurfacePriority);

				if(prio > loudestSurface)
				{
					loudestSurface = prio;
					wheelIndexToUse = i;
				}	
			}
		}
	}

	// No burnout materials, so select the highest priority material from the ones being used
	if(wheelIndexToUse == -1)
	{
		for(s32 i = 0; i < wheelMaterialUsage.GetCount(); i++)
		{
			u8 prio = FindSkidSurfacePriority(wheelMaterialUsage[i].materialSettings->SurfacePriority);

			if(prio > loudestSurface)
			{
				loudestSurface = prio;
				wheelIndexToUse = wheelMaterialUsage[i].wheelIndex;
			}				
		}
	}

#if __BANK
	if(g_DisplayGroundMaterial && GetVehicle()->ContainsLocalPlayer())
	{
		formatf(g_DrivingMaterialName, "%s", PGTAMATERIALMGR->GetMaterial(m_LastMaterialIds[wheelIndexToUse]).GetName());
	}
#endif

	mainWheelMaterial = m_LastMaterialIds[wheelIndexToUse];
	params.hasMaterialChanged = (mainWheelMaterial != m_MainWheelMaterial);

	if(params.hasMaterialChanged || !m_CachedMaterialSettings)
	{
		CollisionMaterialSettings* oldMaterialSettings = m_CachedMaterialSettings;
		
		// material has changed, need to regrab cached settings
		naAssertf(mainWheelMaterial<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Out of bounds");
		m_CachedMaterialSettings = g_audCollisionMaterials[static_cast<s32>(mainWheelMaterial)];
		m_MainWheelMaterial = mainWheelMaterial;

		if(m_CachedMaterialSettings && oldMaterialSettings)
		{
			// Play a bit of clatter if we transition to/from a dirty material
			if((m_CachedMaterialSettings->Dirtiness >= 0.75f && oldMaterialSettings->Dirtiness <= 0.3f) ||
			   (m_CachedMaterialSettings->Dirtiness <= 0.3f && oldMaterialSettings->Dirtiness >= 0.75f))
			{
				audMetadataRef clatterSound = g_NullSoundRef;

				if(!m_Vehicle->m_nVehicleFlags.bIsBeingTowed && m_VehicleType != AUD_VEHICLE_TRAILER)
				{
					clatterSound = GetClatterSound();		

					const f32 rescaledSpeedRatio = Min(1.f, params.movementSpeed * params.invDriveMaxVelocity * 0.8f);

					if(clatterSound != g_NullSoundRef && 
						params.numWheelsTouching > 0 &&
						rescaledSpeedRatio > 0.0f					
						BANK_ONLY(&& !g_DisableClatter))
					{
						if(params.timeInMs - m_LastSurfaceChangeClatterTime > 1000)
						{
							audSoundInitParams initParams;
							initParams.EnvironmentGroup = m_EnvironmentGroup;
							initParams.TrackEntityPosition = true;
							initParams.u32ClientVar = AUD_VEHICLE_SOUND_HEAVY_CLATTER;
							initParams.UpdateEntity = true;

							// Heavy vehicle clatter is a bit much here, so scale the volume down
							initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(rescaledSpeedRatio * (HasHeavyRoadNoise()? 0.5f : 1.0f)) + GetClatterVolumeBoost();
							CreateAndPlaySound(clatterSound, &initParams);						
						}						

						// Set this even if we haven't played a sound - means the player needs to be travelling on a clean/dirty surface for a minimum period of time
						m_LastSurfaceChangeClatterTime = params.timeInMs;
					}					
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Update surface settle sounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateSurfaceSettleSounds(audVehicleVariables& params)
{
	if(m_VehicleType == AUD_VEHICLE_CAR || m_VehicleType == AUD_VEHICLE_BICYCLE)
	{
		// Force settling to happen when we go back onto tarmac
		const float levelToUseForSettling = m_CachedMaterialSettings && m_CachedMaterialSettings->SurfaceSettle == g_NullSoundHash ? 0.f : params.skidLevel;

		m_SurfaceSettleSmoother.CalculateValue(levelToUseForSettling);

		// If we've stopped skidding abruptly, play a surface settle sound
		if(!m_SurfaceSettleSound)
		{
			static float s_SettleRateThreshold = 4.f;
			const float changeRate = (m_SurfaceSettleSmoother.GetLastValue() - levelToUseForSettling) * fwTimer::GetInvTimeStep();
			if(IsReal() && changeRate > s_SettleRateThreshold)
			{
				if(m_CachedMaterialSettings && m_CachedMaterialSettings->SurfaceSettle != g_NullSoundHash)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.Position = VEC3V_TO_VECTOR3(m_Vehicle->GetVehiclePosition());
					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_RecentSpeedToSettleVolume.CalculateValue(m_SurfaceSettleSmoother.GetLastValue()));
					CreateAndPlaySound_Persistent(m_CachedMaterialSettings->SurfaceSettle, &m_SurfaceSettleSound, &initParams);

					if(m_SurfaceSettleSound)
					{
						if(AUD_GET_TRISTATE_VALUE(m_CachedMaterialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_SURFACESETTLEISLOOP) == AUD_TRISTATE_TRUE)
						{
							m_SurfaceSettleSound->SetReleaseTime((s32)(1000.0f * sm_RecentSpeedToSettleReleaseTime.CalculateValue(m_SurfaceSettleSmoother.GetLastValue())));
						}
					}					
				}

				m_SurfaceSettleSmoother.Reset();
				m_SurfaceSettleSmoother.CalculateValue(0.0f);
			}
		}
		else
		{
			// For surface settle sounds that are loops - we want to turn it on, set the release time, then turn it off again
			if(!m_CachedMaterialSettings || AUD_GET_TRISTATE_VALUE(m_CachedMaterialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_SURFACESETTLEISLOOP) == AUD_TRISTATE_TRUE)
			{
				StopAndForgetSounds(m_SurfaceSettleSound);
			}
		}
	}
}

// ----------------------------------------------------------------
// Update wheel detail loop 
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWheelDetail(audVehicleVariables& params)
{
	f32 detailVolFactor;
	f32 vehicleSteerAngle = m_Vehicle->GetSteerAngle();

	if(m_VehicleType == AUD_VEHICLE_BICYCLE)
	{
		detailVolFactor = sm_RoadNoiseBicycleVolCurve.CalculateValue(params.fwdSpeedAbs);
	}
	else if(m_IsFastRoadNoiseDisabled || m_VehicleType == AUD_VEHICLE_PLANE)
	{
		detailVolFactor = sm_RoadNoiseSingleVolCurve.CalculateValue(params.fwdSpeedAbs);
	}
	else
	{
		detailVolFactor = sm_RoadNoiseDetailVolCurve.CalculateValue(params.fwdSpeedAbs);
	}

	f32 stationaryVolumeDB = g_SilenceVolume;

	// If we're stationary, also play the detail sound if we start adjusting the wheels (volume scaled by angle change rate). Only doing 
	// this on the player vehicle as any jerky NPC wheel movements have the potential to sound rubbish
	if(m_IsPlayerVehicle)
	{
		u32 numSteeringWheels = 0;
		u32 numSteeringWheelsTouching = 0;

		for(u32 wheelIndex = 0; wheelIndex < m_Vehicle->GetNumWheels(); wheelIndex++)
		{
			CWheel* wheel = m_Vehicle->GetWheel(wheelIndex);

			if(wheel && wheel->GetConfigFlags().IsFlagSet(WCF_STEER))
			{
				numSteeringWheels++;

				if(wheel->GetIsTouching())
				{
					numSteeringWheelsTouching++;
				}
			}
		}

		if(numSteeringWheels > 0 && numSteeringWheelsTouching > 0 && GetVehicleModelNameHash() != ATSTRINGHASH("ZHABA", 0x4C8DBA51))
		{
			bool hasDonkSuspension = (m_Vehicle->GetVehicleModelInfo() && m_Vehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS));
			f32 steeringAngleCR = Abs(vehicleSteerAngle - m_SteeringAngleLastFrame) * fwTimer::GetInvTimeStep();

			if(hasDonkSuspension)
			{
				steeringAngleCR = Max(steeringAngleCR, m_HydraulicSuspensionAngleSmoother.GetLastValue() * 25.0f);
			}

			if(m_VehicleType == AUD_VEHICLE_BICYCLE)
			{
				steeringAngleCR *= 2.0f;
			}

			const f32 stationaryVolumeScale = numSteeringWheelsTouching/(f32)numSteeringWheels;
			f32 stationaryVolumeLin = sm_RoadNoiseSteeringAngleCRToVolume.CalculateValue(steeringAngleCR * stationaryVolumeScale * GetRoadNoiseVolumeScale());						

			if(IsMonsterTruck())
			{
				// Monster truck has super big tyres, so exaggerate this effect
				stationaryVolumeLin *= 2.0f;
			}
			else if(hasDonkSuspension)
			{
				stationaryVolumeLin *= 1.5f;
			}

			stationaryVolumeLin *= (1.0f - Min(params.fwdSpeedRatio/0.05f, 1.0f));
			stationaryVolumeDB = audDriverUtil::ComputeDbVolumeFromLinear(stationaryVolumeLin);

			static const float steeringAngleSqueakLimit = 20.0f * DtoR;

			if((vehicleSteerAngle < -steeringAngleSqueakLimit && m_SteeringAngleLastFrame > -steeringAngleSqueakLimit) ||
				(vehicleSteerAngle > steeringAngleSqueakLimit && m_SteeringAngleLastFrame < steeringAngleSqueakLimit))
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.UpdateEntity = true;
				initParams.TrackEntityPosition = true;
				initParams.Volume = stationaryVolumeDB;
				CreateAndPlaySound(ATSTRINGHASH("STEERING_ANGLE_SQUEAL", 0x797A3325), &initParams);
			}
		}
	}

	f32 volumeLin = detailVolFactor * GetRoadNoiseVolumeScale() * audDriverUtil::ComputeLinearVolumeFromDb(m_IsPlayerVehicle?0.f:5.f);
	
	// On a bike, lets treat having only one wheel as putting additional pressure on it (stoppie/endo) and therefore scale up the
	// road noise volume rather than decreasing it
	if(m_VehicleType == AUD_VEHICLE_BICYCLE && params.numWheelsTouchingFactor > 0.0f)
	{				
		f32 endoStoppieVolumeScalar = (1.0f - params.numWheelsTouchingFactor);		
		CWheel* rearWheel = m_Vehicle->GetWheelFromId(BIKE_WHEEL_R);

		// Reduce volume for wheelies vs. stoppies, as we can sustain them for longer so having a super loud increase sounds weird
		if(rearWheel && rearWheel->GetIsTouching())
		{
			endoStoppieVolumeScalar *= 0.5f;
		}
		
		volumeLin *= (0.66f + (0.5f * endoStoppieVolumeScalar));
	}
	else
	{
		volumeLin *= params.numWheelsTouchingFactor;
	}

	f32 volumeDB = audDriverUtil::ComputeDbVolumeFromLinear(volumeLin);

	// Steering angle on aircraft is controlled by adjusting the ailerons rather than actually rotating the wheels
	// so doesn't make sense to play a wheel sound in this situation
	if(!m_Vehicle->InheritsFromPlane() && !m_Vehicle->InheritsFromHeli())
	{
		volumeDB += sm_RoadNoiseSteeringAngleToAttenuation.CalculateValue(Abs(vehicleSteerAngle) * RtoD);
	}
	
	volumeDB = Max(volumeDB, stationaryVolumeDB);

	if(volumeDB > g_SilenceVolume)
	{
		// detail tyre noise
		if(!m_WheelDetailLoop || params.hasMaterialChanged || (m_VehicleType == AUD_VEHICLE_BICYCLE && Abs(m_FwdSpeedLastFrame) < 0.05f))
		{
			if(m_CachedMaterialSettings)
			{
				u32 detailRollHash = g_NullSoundHash;

				if(m_VehicleType == AUD_VEHICLE_BICYCLE && Abs(params.fwdSpeed) >= 0.05f)
				{
					detailRollHash = m_CachedMaterialSettings->BicycleTyreRoll;
					m_IsFastRoadNoiseDisabled = true;
				}
				else if(m_CachedMaterialSettings->DetailTyreRoll != g_NullSoundHash)
				{
					detailRollHash = m_CachedMaterialSettings->DetailTyreRoll;
					m_IsFastRoadNoiseDisabled = false;
				}
				else
				{
					detailRollHash = m_CachedMaterialSettings->FastTyreRoll;
					m_IsFastRoadNoiseDisabled = true;
				}

				if(m_WheelDetailLoop)
				{
					if(detailRollHash != m_PrevDetailRoadNoiseHash)
					{
						m_WheelDetailLoop->SetReleaseTime(300);
						m_WheelDetailLoop->StopAndForget();
					}
				}

				if(!m_WheelDetailLoop)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.UpdateEntity = true;
					initParams.TrackEntityPosition = true;
					CreateAndPlaySound_Persistent(detailRollHash, &m_WheelDetailLoop, &initParams);
					m_PrevDetailRoadNoiseHash = detailRollHash;
				}
			}
			else if(m_WheelDetailLoop)
			{
				m_WheelDetailLoop->SetReleaseTime(300);
				m_WheelDetailLoop->StopAndForget();
			}
		}

		if(m_WheelDetailLoop)
		{
			m_WheelDetailLoop->SetRequestedVolume(volumeDB);
			m_WheelDetailLoop->SetRequestedPitch(params.roadNoisePitch);
		}
	}
	else if(m_WheelDetailLoop)
	{
		m_WheelDetailLoop->SetReleaseTime(300);
		m_WheelDetailLoop->StopAndForget();
	}

	if(m_IsPlayerVehicle && m_VehicleType == AUD_VEHICLE_CAR && g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeFranklin)
	{
		const u32 wheelSpeedVar = ATSTRINGHASH("wheelSpeed", 0xAF5C7495);
		const u32 steeringAngleVar = ATSTRINGHASH("steeringAngle", 0x926D6EC9);

		if(!m_FranklinSpecialTraction)
		{
			audSoundInitParams initParams;
			initParams.SetVariableValue(wheelSpeedVar, 0.f);
			initParams.SetVariableValue(steeringAngleVar, 0.f);
			CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetSpecialAbilitySounds().Find("Franklin_Traction"), &m_FranklinSpecialTraction, &initParams);
		}
		else
		{
			m_FranklinSpecialTraction->FindAndSetVariableValue(wheelSpeedVar, params.wheelSpeedRatio);
			m_FranklinSpecialTraction->FindAndSetVariableValue(steeringAngleVar, vehicleSteerAngle);
		}
	}
	else
	{
		StopAndForgetSounds(m_FranklinSpecialTraction);
	}

	m_SteeringAngleLastFrame = vehicleSteerAngle;
}

// ----------------------------------------------------------------
// Check if this is a monster truck
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsMonsterTruck() const
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("MONSTER", 0xCD93A7DB) ||
		modelNameHash == ATSTRINGHASH("MARSHALL", 0x49863E9C))
	{
		return true;
	}

	return false;
}

// ----------------------------------------------------------------
// Update NPC road noise
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateNPCRoadNoise(audVehicleVariables& UNUSED_PARAM(params))
{
#if __BANK
	if(g_DisableNPCRoadNoise)
	{
		if(m_RoadNoiseNPC)
		{
			m_RoadNoiseNPC->StopAndForget();
		}

		return;
	}
#endif

	if(!m_IsPlayerVehicle && GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		if(!m_RoadNoiseNPC)
		{
			if(GetEntityVariableValue(ATSTRINGHASH("mass", 0x4BC594CB)) > 0.0f)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				initParams.AttackTime = g_NPCRoadNoiseAttackTime;
				CreateSound_PersistentReference(GetNPCRoadNoiseSound(), &m_RoadNoiseNPC, &initParams);

				if(m_RoadNoiseNPC)
				{
					m_RoadNoiseNPC->SetReleaseTime(g_NPCRoadNoiseReleaseTime);
					m_RoadNoiseNPC->PrepareAndPlay();
				}
			}
		}

		if(m_RoadNoiseNPC)
		{
			PF_INCREMENT(NumNPCRoadNoise);

			Mat34V m = m_Vehicle->GetMatrix();
			f32 leftAttenuation = m_RoadNoiseLeftCone.ComputeAttenuation(m);
			f32 rightAttenuation = m_RoadNoiseRightCone.ComputeAttenuation(m);
			f32 minAttenuation = Min(leftAttenuation, rightAttenuation);
			BANK_ONLY(m_PrevNPCRoadNoiseAttenuation = minAttenuation * 2.0f);

			Vec3V vehicleToListener = Subtract(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(), m_Vehicle->GetVehiclePosition());
			Vec3V vehicleToListenerDirection = NormalizeFastSafe(vehicleToListener, Vec3V(V_ZERO));

			f32 relativeVelocity = (m_CachedVehicleVelocity - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetListenerVelocity())).Mag();
			f32 volLin = sm_NPCRoadNoiseRelativeSpeedToVol.CalculateValue(relativeVelocity);

			Vec3V vehicleVelocity = RCC_VEC3V(m_CachedVehicleVelocity);
			Vec3V vehicleForward = m_Vehicle->GetTransform().GetForward();
			Vec3V vehicleDirection = NormalizeFastSafe(vehicleVelocity, vehicleForward);
			f32 forwardDot = Dot(vehicleDirection, vehicleToListenerDirection).Getf();

			// Adjust the smoothing based on z height above us (so overhead roads are smooth, but passbys are still nice and quick)
			static f32 maxSmoothing = 0.0001f;
			f32 zOffsetFactor = Clamp(abs(vehicleToListener.GetZf())/5.0f, 0.0f, 1.0f);
			f32 smoothingRequired = 1.0f;

			// Choppy remote vehicle updates in network games means that need heavier smoothing
			if(NetworkInterface::IsGameInProgress())
			{
				smoothingRequired = maxSmoothing;
			}
			// In single player, blend between 1.0 and maxSmoothing depending on the zOffset
			else
			{
				smoothingRequired = maxSmoothing + ((1.0f - maxSmoothing) * (1.0f - zOffsetFactor));
			}

			m_NPCRoadNoiseSmoother.SetRate(smoothingRequired);

			// Sweep the synth param forwards or backwards from the centre as the car moves toward/away from us
			f32 finalParam = m_NPCRoadNoiseSmoother.CalculateValue(0.5f + ((forwardDot > 0.0f)? -minAttenuation : minAttenuation), fwTimer::GetTimeInMilliseconds());
			m_RoadNoiseNPC->FindAndSetVariableValue(ATSTRINGHASH("CTRL", 0xAF169697), finalParam);
			m_RoadNoiseNPC->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volLin));
		}
	}
	else
	{
		if(m_RoadNoiseNPC)
		{
			m_RoadNoiseNPC->StopAndForget();
		}
	}
}

// ----------------------------------------------------------------
// Check if any water VFX are playing
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsAnyWaterVFXActive() const
{
	for(u32 i = 0; i < BOAT_WATER_SAMPLES; i++)
	{
		if(m_WaterSamples.IsSet(i))
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Request a weapon charge sfx
// ----------------------------------------------------------------
void audVehicleAudioEntity::RequestWeaponChargeSfx(s32 boneIndex, f32 chargeLevel)
{
	m_WeaponChargeBone = boneIndex;
	m_WeaponChargeLevel = chargeLevel;
}

// ----------------------------------------------------------------
// Get the transformation soundset associated with a submarine car vehicle 
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::GetSubmarineCarTransformSoundSet() const
{
	if (GetVehicleModelNameHash() == ATSTRINGHASH("STROMBERG", 0x34DBA661))
	{
		return m_IsFocusVehicle ? ATSTRINGHASH("stromberg_transform_ss", 0xDE029774) : ATSTRINGHASH("stromberg_transform_npc_ss", 0x48112622);
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("TOREADOR", 0x56C8A5EF))
	{
		return m_IsFocusVehicle ? ATSTRINGHASH("toreador_transform_ss", 0xA29C53CF) : ATSTRINGHASH("toreador_transform_npc_ss", 0xEFE26236);
	}

	return 0u;
}

// ----------------------------------------------------------------
// Get the soundset associated with a submarine car vehicle 
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::GetSubmarineCarSoundset() const
{
	if (GetVehicleModelNameHash() == ATSTRINGHASH("STROMBERG", 0x34DBA661))
	{
		return ATSTRINGHASH("stromberg_submersible_sounds", 0x1DF32727);
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("TOREADOR", 0x56C8A5EF))
	{
		return ATSTRINGHASH("toreador_submersible_sounds", 0x9919A4F4);
	}

	return 0u;
}

// ----------------------------------------------------------------
// Query if this is a submarine car type vehicle
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsSubmarineCar() const
{
	return GetVehicleModelNameHash() == ATSTRINGHASH("STROMBERG", 0x34DBA661) || GetVehicleModelNameHash() == ATSTRINGHASH("TOREADOR", 0x56C8A5EF);
}

// ----------------------------------------------------------------
// Called when submarine car wings tilt fully up/down
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSubmarineCarWingFlapSound()
{
	TriggerSimpleSoundFromSoundset(GetSubmarineCarSoundset(), ATSTRINGHASH("wing_flap", 0x531673B5) REPLAY_ONLY(, true));
}

// ----------------------------------------------------------------
// Called when submarine car rudders start turning
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSubmarineCarRudderTurnStart()
{
	if(fwTimer::GetTimeInMilliseconds() - m_LastSubCarRudderTurnSoundTime > g_MinSubRudderRepeatTime)
	{
		TriggerSimpleSoundFromSoundset(GetSubmarineCarSoundset(), ATSTRINGHASH("rotor_turn", 0xC24998A7) REPLAY_ONLY(, true));
		m_LastSubCarRudderTurnSoundTime = fwTimer::GetTimeInMilliseconds();
	}	
}

// ----------------------------------------------------------------
// Called when a submarine car starts transforming
// ----------------------------------------------------------------
void audVehicleAudioEntity::OnSubmarineCarAnimationStateChanged(u32 newState, f32 UNUSED_PARAM(phase))
{
	if(!IsDisabled())
	{
		switch((CSubmarineCar::AnimationState)newState)
		{
		case CSubmarineCar::State_StartToSubmarine: // State_MoveWheelsUp
			m_SubmarineTransformSoundHash = ATSTRINGHASH("transform_to_sub", 0xF585770F);
			break;

		case CSubmarineCar::State_MoveWheelCoversOut:
			m_SubmarineTransformSoundHash = ATSTRINGHASH("move_wheel_covers_out", 0xC56A6456);		
			break;

		case CSubmarineCar::State_FinishToSub:
			m_SubmarineTransformSoundHash = ATSTRINGHASH("finish_to_sub", 0xE9EC6B7);		
			break;

		case CSubmarineCar::State_StartToCar: // Same as State_MoveWheelCoversIn
			m_SubmarineTransformSoundHash = m_TriggerFastSubCarTransformSound? ATSTRINGHASH("transform_to_car_short", 0x99F3DA7) : ATSTRINGHASH("transform_to_car", 0xD8B90941);

			if(m_ShouldStopSubTransformSound && m_TriggerFastSubCarTransformSound)
			{
				m_ShouldStopSubTransformSound = false;
			}

			m_TriggerFastSubCarTransformSound = false;
			break;

		case CSubmarineCar::State_MoveWheelsDown:
			m_SubmarineTransformSoundHash = ATSTRINGHASH("move_wheel_covers_down", 0x23DE340B);
			m_TriggerFastSubCarTransformSound = false;
			break;

		case CSubmarineCar::State_FinishToCar:
			m_SubmarineTransformSoundHash = ATSTRINGHASH("finish_to_car", 0xF3722B5A);
			m_TriggerFastSubCarTransformSound = false;
			break;

		default:
			break;
		}
	}	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateWeaponCharge
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWeaponCharge()
{
	if(GetVehicleModelNameHash() == ATSTRINGHASH("KHANJALI", 0xAA6F980A))
	{
		if(m_WeaponChargeLevel > 0.f)
		{
			if(!m_WeaponChargeSound)
			{
				const u32 soundNameHash = m_IsFocusVehicle? ATSTRINGHASH("xm_vehicle_khanjali_railgun_charge_player_master", 0x842C8EC3) : ATSTRINGHASH("xm_vehicle_khanjali_railgun_charge_npc_master", 0xD294CA0D);
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;

				if(m_WeaponChargeBone != -1)
				{
					initParams.UpdateEntity = true;
					initParams.u32ClientVar = (u32(AUD_VEHICLE_SOUND_CUSTOM_BONE)&0xffff) | ((u32)m_WeaponChargeBone << 16);
				}
				else
				{
					initParams.TrackEntityPosition = true;
				}
				
				CreateAndPlaySound_Persistent(soundNameHash, &m_WeaponChargeSound, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundNameHash, &initParams, m_WeaponChargeSound, GetOwningEntity()));
			}

			if(m_WeaponChargeSound)
			{
				m_WeaponChargeSound->FindAndSetVariableValue(ATSTRINGHASH("chargeLevel", 0x6C265400), m_WeaponChargeLevel);
			}
		}
		else if(m_WeaponChargeSound)
		{
			m_WeaponChargeSound->StopAndForget(true);
		}

		m_WeaponChargeLevel = 0.f;
	}	
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateWaterState
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateLandingGear()
{
	if(m_ShouldStopLandingGearSound && GetVehicleModelNameHash() == ATSTRINGHASH("Thruster", 0x58CDAF30))
	{		
		if(m_LandingGearSound)
		{
			m_LandingGearSound->StopAndForget(true);
		}

		m_ShouldStopLandingGearSound = false;
	}
	else if(m_ShouldPlayLandingGearSound)
	{
		if(GetVehicleModelNameHash() == ATSTRINGHASH("BLAZER5", 0xA1355F67))
		{
			audSoundSet soundSet;

			if(soundSet.Init(ATSTRINGHASH("DLC_ImportExport_Blazer5_Wheel_Sounds", 0x50782A26)))
			{
				TriggerLandingGearSound(soundSet, m_IsLandingGearRetracting? ATSTRINGHASH("retract", 0x8E1F66E6) : ATSTRINGHASH("deploy", 0xC9200261));
			}
		}
		
		m_ShouldPlayLandingGearSound = false;
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity StopLandingGearSound
// ----------------------------------------------------------------
void audVehicleAudioEntity::StopLandingGearSound()
{
	m_ShouldPlayLandingGearSound = false;
	m_ShouldStopLandingGearSound = true;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateWaterState
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWaterState(const VfxWaterSampleData_s* pFxSampleData)
{	
	for(u32 i = 0; i < BOAT_WATER_SAMPLES; i++)
	{
		m_WaterSamples.Set(i, pFxSampleData[i].isInWater);		
	}	
}

// ----------------------------------------------------------------
// Update fast road noise
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateFastRoadNoise(audVehicleVariables& params)
{
	if(!m_IsFastRoadNoiseDisabled)
	{
		// dummy vehicles don't seem to have a valid ground speed
		const f32 fastRoadNoiseVolLin = (m_IsPlayerVehicle?g_PlayerRoadNoiseFastVolCurve.CalculateValue(Abs(params.fwdSpeed)):sm_RoadNoiseFastVolCurve.CalculateValue(Abs(params.fwdSpeed))) * params.numWheelsTouchingFactor * GetRoadNoiseVolumeScale() * audDriverUtil::ComputeLinearVolumeFromDb(m_IsPlayerVehicle?0.f:5.f);

		if(fastRoadNoiseVolLin > g_SilenceVolumeLin)
		{
			if(!m_RoadNoiseFast || params.hasMaterialChanged)
			{
				u32 soundHash = (m_CachedMaterialSettings?m_CachedMaterialSettings->FastTyreRoll:g_FastConcreteTyreRollHash);

				if(m_RoadNoiseFast)
				{
					// Only stop the sound if we've actually changed over
					if(m_PrevFastRoadNoiseHash != soundHash)
					{
						m_RoadNoiseFast->SetReleaseTime(300);
						StopAndForgetSounds(m_RoadNoiseFast);
					}
				}

				if(!m_RoadNoiseFast)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.TrackEntityPosition = true;
					initParams.UpdateEntity = true;
					initParams.AttackTime = 300;
					CreateSound_PersistentReference(soundHash, &m_RoadNoiseFast, &initParams);
					if(m_RoadNoiseFast)
					{
						m_RoadNoiseFast->PrepareAndPlay();
						m_PrevFastRoadNoiseHash = soundHash;
					}
				}
			}

			if(m_RoadNoiseFast)
			{
				// 10dB louder for AI cars
				m_RoadNoiseFast->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(fastRoadNoiseVolLin));
				m_RoadNoiseFast->SetRequestedPitch(params.roadNoisePitch);	
			}
		}
		else if(m_RoadNoiseFast)
		{
			m_RoadNoiseFast->SetReleaseTime(300);
			StopAndForgetSounds(m_RoadNoiseFast);
		}

		f32 wetRollVolumeLin = 0.0f;

		if(!m_Vehicle->m_nFlags.bInMloRoom && 
		   m_EnvironmentGroup && 
		  !m_EnvironmentGroup->IsUnderCover())
		{
			wetRollVolumeLin = g_weather.GetWetness() * params.numWheelsTouchingFactor * GetRoadNoiseVolumeScale() * (g_RoadNoiseWetRoadVolCurve.CalculateValue(params.fwdSpeedAbs) + params.wetRoadSkidModifier);
		}

		UpdateLoopWithLinearVolumeAndPitch(&m_RoadNoiseWet, sm_TarmacSkidSounds.Find(ATSTRINGHASH("WetRoll", 0x7B8E2BAA)), wetRollVolumeLin, Max(params.roadNoisePitch, (s32)sm_RoadNoisePitchCurve.CalculateValue(params.fwdSpeedAbs)), m_EnvironmentGroup, g_RoadNoiseCategory, true, false);

		if(HasHeavyRoadNoise())
		{
			// heavy vehicle rumble
			UpdateLoopWithLinearVolumeAndPitch(&m_RoadNoiseHeavy, ATSTRINGHASH("TRUCK_LOW_ROAD_NOISE", 0x314B4DBE), fastRoadNoiseVolLin * g_VehicleMassToRumbleVol.CalculateValue(m_Vehicle->pHandling->m_fMass), 0, m_EnvironmentGroup, NULL, true, false, 0.f);
		}

		u32 tireSweetenerWheelVar = AUD_VEHICLE_SOUND_UNKNOWN;

		if(m_IsPlayerVehicle && m_Vehicle->GetOwnedBy()!=ENTITY_OWNEDBY_CUTSCENE)
		{
			const camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector();

			if(cinematicDirector.IsRenderingAnyInVehicleCinematicCamera())
			{
				const camBaseCamera* renderedCinematicCamera = cinematicDirector.GetRenderedCamera();

				if(renderedCinematicCamera)
				{
					CWheel* tireSweetenerWheel = NULL;

					if(renderedCinematicCamera->GetNameHash() == ATSTRINGHASH("WHEEL_FRONT_LEFT_CAMERA", 0x4472093A))
					{
						tireSweetenerWheel = m_Vehicle->GetWheelFromId(VEH_WHEEL_LF);
					}
					else if(renderedCinematicCamera->GetNameHash() == ATSTRINGHASH("WHEEL_FRONT_RIGHT_CAMERA", 0x9843718D))
					{
						tireSweetenerWheel = m_Vehicle->GetWheelFromId(VEH_WHEEL_RF);
					}
					else if(renderedCinematicCamera->GetNameHash() == ATSTRINGHASH("WHEEL_REAR_LEFT_CAMERA", 0xA4EAD485))
					{
						tireSweetenerWheel = m_Vehicle->GetWheelFromId(VEH_WHEEL_LR);
					}
					else if(renderedCinematicCamera->GetNameHash() == ATSTRINGHASH("WHEEL_REAR_RIGHT_CAMERA", 0x560D1CF6))
					{
						tireSweetenerWheel = m_Vehicle->GetWheelFromId(VEH_WHEEL_RR);
					}

					if(!tireSweetenerWheel || !tireSweetenerWheel->GetIsTouching())
					{
						tireSweetenerWheelVar = AUD_VEHICLE_SOUND_UNKNOWN;
					}
					else
					{
						// Left front isn't always wheel 0 as things like trailers only have two rear wheels, so go through and grab the appropriate
						// index for the chosen wheelID
						for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
						{
							if(m_Vehicle->GetWheel(loop) == tireSweetenerWheel)
							{
								tireSweetenerWheelVar = GetWheelSoundUpdateClientVar(loop);
								break;
							}
						}
					}
				}
			}
		}

		if(tireSweetenerWheelVar != AUD_VEHICLE_SOUND_UNKNOWN)
		{
			if(!m_CinematicTireSweetener)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.u32ClientVar = tireSweetenerWheelVar;
				initParams.UpdateEntity = true;
				CreateAndPlaySound_Persistent(sm_TarmacSkidSounds.Find(ATSTRINGHASH("CinematicTireSweetener", 0x383949C7)), &m_CinematicTireSweetener, &initParams);
			}

			if(m_CinematicTireSweetener)
			{
				m_CinematicTireSweetener->SetRequestedDopplerFactor(0.0f);
				m_CinematicTireSweetener->SetClientVariable(tireSweetenerWheelVar);
				m_CinematicTireSweetener->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(fastRoadNoiseVolLin));
				m_CinematicTireSweetener->SetRequestedPitch(params.roadNoisePitch);	
			}
		}
		else if(m_CinematicTireSweetener)
		{
			m_CinematicTireSweetener->StopAndForget();
		}
	}
	else
	{
		if(m_RoadNoiseFast)
		{
			m_RoadNoiseFast->StopAndForget();
		}

		if(m_RoadNoiseWet)
		{
			m_RoadNoiseWet->StopAndForget();
		}
	}
}

// ----------------------------------------------------------------
// Update ridged surfaces
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateRidgedSurfaces(audVehicleVariables& params)
{
	u32 ridgedSoundHash = g_NullSoundHash;

	if(params.numWheelsTouchingFactor > 0.0f)
	{
		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			if(m_Vehicle->GetWheel(i)->GetIsTouching() && Abs(m_Vehicle->GetWheel(i)->GetGroundSpeed()) > 0.2f)
			{
				CollisionMaterialSettings* materialSettings = g_audCollisionMaterials[static_cast<s32>(m_LastMaterialIds[i])];

				if(materialSettings && materialSettings->RidgedSurfaceLoop != g_NullSoundHash)
				{
					ridgedSoundHash = materialSettings->RidgedSurfaceLoop;
				}
			}
		}
	}

	if(ridgedSoundHash != g_NullSoundHash)
	{
		if(ridgedSoundHash != m_LastRidgedRoadNoise)
		{
			StopAndForgetSounds(m_RoadNoiseRidged);
		}

		m_LastRidgedRoadNoise = ridgedSoundHash;

		if(!m_RoadNoiseRidged)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			CreateSound_PersistentReference(ridgedSoundHash, &m_RoadNoiseRidged, &initParams);
			if(m_RoadNoiseRidged)
			{
				m_RoadNoiseRidged->PrepareAndPlay();
			}
		}

		if(m_RoadNoiseRidged)
		{			
			f32 vehicleSpeed = Abs(Dot(m_Vehicle->GetTransform().GetB(), VECTOR3_TO_VEC3V(m_CachedVehicleVelocity)).Getf()) * m_RidgedSurfaceDirectionMatch;
			m_RoadNoiseRidged->FindAndSetVariableValue(ATSTRINGHASH("SPEED", 0xf997622b), vehicleSpeed);

			f32 ridgedHPFCutoff = 0;

			if(!m_IsPlayerVehicle)
			{
				ridgedHPFCutoff = 500.0f;
				if(m_RoadNoiseRidgedPulse)
				{
					m_RoadNoiseRidgedPulse->StopAndForget();
				}
			}
			else
			{

				if(audNorthAudioEngine::ShouldTriggerPulseHeadset())
				{
					if(!m_RoadNoiseRidgedPulse)
					{
						CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("RumbleStrip", 0xE1802336)), &m_RoadNoiseRidgedPulse);
					}

					if(m_RoadNoiseRidgedPulse)
					{
						m_RoadNoiseRidgedPulse->FindAndSetVariableValue(ATSTRINGHASH("SPEED", 0xf997622b), vehicleSpeed);
					}
				}
				else
				{
					StopAndForgetSounds(m_RoadNoiseRidgedPulse);
				}

				static const f32 minDistanceSq = 15.0f * 15.0f;
				static const f32 maxDistanceSq = 50.0f * 50.0f;
				ridgedHPFCutoff = 500.0f * Clamp((m_DistanceFromListener2LastFrame - minDistanceSq)/(maxDistanceSq - minDistanceSq), 0.0f, 1.0f);
			}

			m_RoadNoiseRidged->SetRequestedHPFCutoff((u32)ridgedHPFCutoff);
		}
	}
	else
	{
		StopAndForgetSounds(m_RoadNoiseRidged);
		StopAndForgetSounds(m_RoadNoiseRidgedPulse);
	}
}

// ----------------------------------------------------------------
// Update skids
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateFreewayBumps(audVehicleVariables& params)
{
	const bool isOnFreeway = (m_IsPlayerVehicle?FindPlayerPed()->GetPlayerInfo()->GetPlayerDataPlayerOnHighway()!=0
		:m_Vehicle->GetIntelligence()->IsOnHighway());

	if(m_Vehicle && m_Vehicle->GetDriver() && m_Vehicle->GetDriver() == FindPlayerPed())
	{
		u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
		if(!m_WasOnFreeWayLastFrame && isOnFreeway)
		{
			m_TimeEnteredFreeway = currentTime;
			m_TriggeredDispatchEnteredFreeway = false;
		}
		else if(!m_WasOnFreeWayLastFrame && isOnFreeway)
		{
			m_TimeExitedFreeway = currentTime;
			m_TriggeredDispatchExitedFreeway = false;
		}
			
		if(isOnFreeway && !m_TriggeredDispatchEnteredFreeway && currentTime - m_TimeEnteredFreeway > 4000)
		{
			g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY);
			m_TriggeredDispatchEnteredFreeway = true;
		}
		if(!isOnFreeway && !m_TriggeredDispatchExitedFreeway && currentTime - m_TimeExitedFreeway > 4000)
		{
			g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY);
			m_TriggeredDispatchExitedFreeway = true;
		}
	}
	m_WasOnFreeWayLastFrame = isOnFreeway;

	if((m_VehicleType == AUD_VEHICLE_CAR || m_VehicleType == AUD_VEHICLE_TRAILER) && isOnFreeway)
	{
		// fake freeway bumps every 12m axis aligned
		Vector3 vehiclePosition = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
		Vector3 positionToWheel = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetForward()) * (m_DistanceBetweenWheels * 0.5f);
		Vector3 frontPos = vehiclePosition + positionToWheel;
		Vector3 backPos = vehiclePosition - positionToWheel;
			
		u32 lastFront, curFront;
		u32 lastBack, curBack;
		bank_float fDistScalar = (1.f/30.f);
		if(Abs(m_CachedVehicleVelocity.x) > Abs(m_CachedVehicleVelocity.y))
		{
			// use x axis
			lastFront = (u32)Abs(m_LastRoadPosFront.x * fDistScalar);
			curFront = (u32)Abs(frontPos.x * fDistScalar);
			lastBack = (u32)Abs(m_LastRoadPosBack.x * fDistScalar);
			curBack = (u32)Abs(backPos.x * fDistScalar);
		}
		else
		{
			// use y axis
			lastFront = (u32)Abs(m_LastRoadPosFront.y * fDistScalar);
			curFront = (u32)Abs(frontPos.y * fDistScalar);
			lastBack = (u32)Abs(m_LastRoadPosBack.y * fDistScalar);
			curBack = (u32)Abs(backPos.y * fDistScalar);
		}

		const f32 bumpVolOffset = (m_IsPlayerVehicle?0.f:2.f);
		
		if(lastFront != curFront)
		{
			if(m_LastFreewayBumpFront + 300 < fwTimer::GetTimeInMilliseconds() && m_Vehicle->GetWheel(0)->GetIsTouching())
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.Position = frontPos;
				initParams.Volume = bumpVolOffset + audDriverUtil::ComputeDbVolumeFromLinear(g_FreewayBumpSpeedToVol.CalculateValue(params.wheelSpeedAbs));
				initParams.Pitch = (s16) (m_FreewayBumpPitch + (s32)g_FreewayBumpSpeedToPitch.CalculateValue(params.wheelSpeedAbs));
				CreateAndPlaySound(ATSTRINGHASH("TYRE_BUMPS_FREEWAY", 0x48022590), &initParams);
			}
			m_LastFreewayBumpFront = fwTimer::GetTimeInMilliseconds();
		}

		if(lastBack != curBack)
		{
			if(m_LastFreewayBumpBack + 300 < fwTimer::GetTimeInMilliseconds() && m_Vehicle->GetWheel(m_Vehicle->GetNumWheels()-1)->GetIsTouching())
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.Position = backPos;
				initParams.Volume = bumpVolOffset + audDriverUtil::ComputeDbVolumeFromLinear(g_FreewayBumpSpeedToVol.CalculateValue(params.wheelSpeedAbs));
				initParams.Pitch = (s16) (m_FreewayBumpPitch + (s32)g_FreewayBumpSpeedToPitch.CalculateValue(params.wheelSpeedAbs));
				CreateAndPlaySound(ATSTRINGHASH("TYRE_BUMPS_FREEWAY", 0x48022590), &initParams);
			}
			m_LastFreewayBumpBack = fwTimer::GetTimeInMilliseconds();
		}

		m_LastRoadPosFront.x = frontPos.x;
		m_LastRoadPosFront.y = frontPos.y;
		m_LastRoadPosBack.x = backPos.x;
		m_LastRoadPosBack.y = backPos.y;
	}
}

// ----------------------------------------------------------------
// Update wheel spin
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWheelSpin(audVehicleVariables& params)
{
	if(params.hasMaterialChanged || !m_CachedMaterialSettings)
	{
		StopAndForgetSounds(m_WheelSpinLoop);
	}

	if(!m_CachedMaterialSettings)
	{
		StopAndForgetSounds(m_WetWheelSpinLoop);
	}

	if(m_CachedMaterialSettings)
	{
		f32 vehicleSpeed2 = m_CachedVehicleVelocity.Mag2();
		f32 maxWheelSpin = 0.0f;

		for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
		{
			CWheel* wheel = m_Vehicle->GetWheel(loop);

			if (wheel && wheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				if (vehicleSpeed2 < 4.0f)
				{	
					f32 wheelGroundSpeed = Abs(wheel->GetGroundSpeed());

					if (wheelGroundSpeed > 4.0f)
					{
						maxWheelSpin = Max(maxWheelSpin, CVfxHelper::GetInterpValue(wheelGroundSpeed, 4.0f, 12.0f));
					}
				}
			}
		}

		f32 wheelSpinRatio = m_WheelSpinSmoother.CalculateValue(maxWheelSpin, fwTimer::GetTimeInMilliseconds());

		if(wheelSpinRatio > 0.0f)
		{
			if(!m_WheelSpinLoop)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				CreateAndPlaySound_Persistent(m_CachedMaterialSettings->WheelSpinLoop, &m_WheelSpinLoop, &initParams);
			}

			f32 wetnessFactor = 0.0f;

			if(!m_Vehicle->m_nFlags.bInMloRoom && 
				m_EnvironmentGroup && 
				!m_EnvironmentGroup->IsUnderCover())
			{
				wetnessFactor = Max(g_weather.GetWetness(), params.numDriveWheelsInShallowWaterFactor);
			}

			if(!m_WetWheelSpinLoop && wetnessFactor > 0.0f)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				CreateAndPlaySound_Persistent(sm_TarmacSkidSounds.Find(ATSTRINGHASH("WetWheelSpin", 0x9A716CA8)), &m_WetWheelSpinLoop, &initParams);
			}

			if(m_WheelSpinLoop)
			{
				m_WheelSpinLoop->FindAndSetVariableValue(ATSTRINGHASH("wheelSpinRatio", 0x73422810), wheelSpinRatio);
			}

			if(m_WetWheelSpinLoop)
			{
				m_WetWheelSpinLoop->FindAndSetVariableValue(ATSTRINGHASH("wheelSpinRatio", 0x73422810), wheelSpinRatio);
				m_WetWheelSpinLoop->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(wetnessFactor * wheelSpinRatio));
			}
		}
		else
		{
			StopAndForgetSounds(m_WheelSpinLoop, m_WetWheelSpinLoop);
		}
	}
}

// ----------------------------------------------------------------
// GetShallowWaterWheelSound
// ----------------------------------------------------------------
audMetadataRef audVehicleAudioEntity::GetShallowWaterWheelSound() const
{
	return sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleWheelWaterLoop", 0x1182FD91));
}

// ----------------------------------------------------------------
// GetShallowWaterEnterSound
// ----------------------------------------------------------------
audMetadataRef audVehicleAudioEntity::GetShallowWaterEnterSound() const
{
	return sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleWheelWaterEntry", 0x86ABA8E5));
}

// ----------------------------------------------------------------
// Update wheel water sounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWheelWater(audVehicleVariables& params)
{
	audMetadataRef shallowWaterSound = GetShallowWaterWheelSound();

	if(shallowWaterSound != g_NullSoundRef)
	{
		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			CWheel *wheel = m_Vehicle->GetWheel(i);

			if(wheel->GetHierarchyId() >= VEH_WHEEL_LF && wheel->GetHierarchyId() <= VEH_WHEEL_RR)
			{
				f32 speedRatio = (wheel->GetIsTouching()?Clamp(Abs(wheel->GetGroundSpeed()) / m_Vehicle->m_Transmission.GetDriveMaxVelocity(), 0.0f,1.0f):0.0f);
				bool inWater = wheel->GetDynamicFlags().IsFlagSet(WF_INSHALLOWWATER) || (wheel->GetDynamicFlags().IsFlagSet(WF_INDEEPWATER) && m_DrowningFactor <= 0.0f) BANK_ONLY(|| g_ForceShallowWater);
				const f32 shallowWaterVol = inWater? Clamp(speedRatio/0.3f, 0.0f, 1.0f) : 0.0f;
				bool wasShallowWaterPlaying = (m_ShallowWaterSounds[i] != NULL);
				UpdateLoopWithLinearVolumeAndPitch(&m_ShallowWaterSounds[i], shallowWaterSound, shallowWaterVol, 0, m_EnvironmentGroup, NULL, true, false, 0.f, true, true);

				if(!wasShallowWaterPlaying && m_ShallowWaterSounds[i] && !m_ShallowWaterEnterSound)
				{
					if(m_Vehicle->InheritsFromAutomobile())
					{
						audSoundInitParams initParams;
						initParams.TrackEntityPosition = true;
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(Min(params.fwdSpeedAbs/20.0f, 1.0f));
						CreateAndPlaySound_Persistent(GetShallowWaterEnterSound(), &m_ShallowWaterEnterSound, &initParams);
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Calculate water speed factor
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::CalculateWaterSpeedFactor() const
{
	return Min(1.f, DotProduct(m_CachedVehicleVelocity, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB()) ) / 20.f);
}

// ----------------------------------------------------------------
// Update water sounds - common across all vehicles as we can have water
// based helis/planes in addition to boats
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWater(audVehicleVariables& vehicleVariables)
{
	bool isInWater = CalculateIsInWater();
	u32 idleSlapSound = GetIdleHullSlapSound();
	const u32 timeInMs = fwTimer::GetTimeInMilliseconds();
	f32 minWaveHitDelay = (GetAudioVehicleType() == AUD_VEHICLE_HELI && IsSeaPlane()) ? 1000.f : 250.f;

	if(idleSlapSound != g_NullSoundHash)
	{
		f32 idleSlapLinVolume = GetIdleHullSlapVolumeLinear(vehicleVariables.fwdSpeedAbs);

		if(CScriptHud::bUsingMissionCreator && !m_Vehicle->IsCollisionEnabled())
		{
			idleSlapLinVolume = 0.0f;
		}		

		if((isInWater || (IsSubmersible() && isInWater && m_EngineWaterDepth > -1.5f)) && idleSlapLinVolume > g_SilenceVolumeLin)
		{
			u32 vehicleQuadrant = GetVehicleQuadrant();

			const u32 modelNameHash = GetVehicleModelNameHash();
			const f32 hullSlapDistanceScalar = modelNameHash == ATSTRINGHASH("Kosatka", 0x4FAF0D70) ? 3.f : 1.f;			

			if((m_IsFocusVehicle && modelNameHash == ATSTRINGHASH("Kosatka", 0x4FAF0D70)) || // Kosatka is so big that we don't want to cull hull slaps when panning around the camera on the focus vehicle
			   (vehicleVariables.distFromListenerSq < (g_SuperDummyRadiusSq * hullSlapDistanceScalar * sm_WaterSlapRadiusScalar[vehicleQuadrant]) * 0.5f))
			{
				// Kosatka requires data from the sfx bank for the hull slaps, which isn't loaded unless real
				if (modelNameHash != ATSTRINGHASH("Kosatka", 0x4FAF0D70) || IsReal())
				{
					sm_NumVehiclesInWaterSlapRadius[vehicleQuadrant]++;

					if (!m_IdleHullSlapSound)
					{
						audSoundInitParams initParams;
						initParams.StartOffset = audEngineUtil::GetRandomNumberInRange(0, 99);
						initParams.IsStartOffsetPercentage = true;

						if (modelNameHash != ATSTRINGHASH("Kosatka", 0x4FAF0D70))
						{
							initParams.Tracker = m_Vehicle->GetPlaceableTracker();
						}
						
						initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
						CreateAndPlaySound_Persistent(idleSlapSound, &m_IdleHullSlapSound, &initParams);
					}

					if (m_IdleHullSlapSound && modelNameHash == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
					{
						m_IdleHullSlapSound->SetRequestedPosition(ComputeClosestPositionOnVehicleYAxis());
					}
				}				
			}
			else if(m_IdleHullSlapSound && m_IdleHullSlapSound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0)) > 500)
			{
				m_IdleHullSlapSound->StopAndForget();
			}

			if(m_IdleHullSlapSound)
			{
				m_IdleHullSlapSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(idleSlapLinVolume)); 
			}
		}
		else if(m_IdleHullSlapSound)
		{
			m_IdleHullSlapSound->StopAndForget();
		}
	}

	// wave hit
	if(isInWater && !m_WasInWaterLastFrame && m_LastWaveHitTime + minWaveHitDelay < timeInMs)
	{
		u32 soundHash = GetWaveHitSound();

		if(soundHash != g_NullSoundHash)
		{
			m_LastWaveHitTime = timeInMs;
			audSoundInitParams initParams;
			const f32 timerVol = Min(1.f, m_OutOfWaterTimer);
			const f32 minVelocityRatio = (GetAudioVehicleType() == AUD_VEHICLE_HELI && IsSeaPlane()) ? 0.4f : 0.0f;
			const f32 velocityRatio = Clamp(m_CachedVehicleVelocity.Mag2()/50.0f, minVelocityRatio, 1.0f);
			u32 bigSoundHash = GetWaveHitBigSound();

			initParams.Tracker = m_Vehicle->GetPlaceableTracker();
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(velocityRatio);

			if(bigSoundHash != g_NullSoundHash)
			{
				if((m_IsPlayerVehicle || m_DistanceFromListener2LastFrame < g_FakeWaveHitDistanceSq) && timeInMs - m_LastBigWaveHitTime > 500 && m_OutOfWaterTimer >= GetWaveHitBigAirTime() && velocityRatio >= 0.75f)
				{
					soundHash = GetWaveHitBigSound();
					m_LastBigWaveHitTime = timeInMs;
				}
			}		

			audSound* waveHitSound = NULL;
			if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				CreateSound_LocalReference(soundHash, &waveHitSound, &initParams);
			}

			f32 speedFactor = CalculateWaterSpeedFactor();
			f32 impactForce = timerVol * (0.5f + (speedFactor * 0.5f));

			if(waveHitSound)
			{
				waveHitSound->FindAndSetVariableValue(ATSTRINGHASH("impactForce", 0x165EF795), impactForce);
				waveHitSound->PrepareAndPlay();

#if __BANK
				if(m_IsPlayerVehicle && g_DebugDrawVehicleWater)
				{
					char tempString[128];
					formatf(tempString, soundHash == bigSoundHash? "BIG Splash!" : "Splash!");
					grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), Color32(0,0,255), tempString, true, 5);
				}
#endif
			}
		}
	}
	else if(m_WasInWaterLastFrame && !isInWater && m_LastWaterLeaveTime + minWaveHitDelay < timeInMs)
	{
		u32 leftWaterSound = GetLeftWaterSound();

		if(leftWaterSound != g_NullSoundHash)
		{
			m_LastWaterLeaveTime = timeInMs;
			audSoundInitParams initParams;
			initParams.Tracker = m_Vehicle->GetPlaceableTracker();
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(Clamp(m_CachedVehicleVelocity.Mag2()/50.0f, 0.0f, 1.0f));
			CreateAndPlaySound(leftWaterSound, &initParams);	

#if __BANK
			if(m_IsPlayerVehicle && g_DebugDrawVehicleWater)
			{
				char tempString[128];
				formatf(tempString, "Left!");
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), Color32(0,0,255), tempString, true, 5);
			}
#endif
		}		
	}	

	if(isInWater)
	{
		m_InWaterTimer += fwTimer::GetTimeStep();
		m_OutOfWaterTimer = 0.f;
	}
	else
	{
		m_OutOfWaterTimer += fwTimer::GetTimeStep();
		m_InWaterTimer = 0.f;
	}

	m_WasInWaterLastFrame = isInWater;
}

// ----------------------------------------------------------------
// Check if warning sounds are valid
// ----------------------------------------------------------------
bool audVehicleAudioEntity::AreWarningSoundsValid()
{
	// Avoid playing warning sounds on the player vehicle in multiplayer when the player can't actually control the vehicle (eg. on leaderboards)
	if(m_IsPlayerVehicle && NetworkInterface::IsGameInProgress() && CGameWorld::GetMainPlayerInfo() && CGameWorld::GetMainPlayerInfo()->AreControlsDisabled())
	{
		return false;
	}

	return true;
}

// ----------------------------------------------------------------
// AddCommonDSPParameters
// ----------------------------------------------------------------
void audVehicleAudioEntity::AddCommonDSPParameters(audVehicleDSPSettings& dspSettings)
{
	const f32 engineBayUpgradeLevel0 = GetEngineBayUpgradeLevel(0);
	const f32 engineBayUpgradeLevel1 = GetEngineBayUpgradeLevel(1);
	const f32 engineBayUpgradeLevel2 = GetEngineBayUpgradeLevel(2);
	const f32 turboUpgradeLevel = GetTurboUpgradeLevel();
	const f32 firstPerson = *g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Microphone.FirstPerson", 0x378A1D92));
	dspSettings.AddDSPParameter(atHashString("Mod_Upgrade_Level", 0x893FF500), GetEngineUpgradeLevel(), audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
	dspSettings.AddDSPParameter(atHashString("Mod_Upgrade_Level", 0x893FF500), GetExhaustUpgradeLevel(), audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);
	dspSettings.AddDSPParameter(atHashString("Mod_Turbo_Upgrade_Level", 0x282B095B), turboUpgradeLevel, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
	dspSettings.AddDSPParameter(atHashString("Mod_Turbo_Upgrade_Level", 0x282B095B), turboUpgradeLevel, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);
	dspSettings.AddDSPParameter(atHashString("Mod_Slot_0_Upgrade_Level", 0x2E672FAF), engineBayUpgradeLevel0, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
	dspSettings.AddDSPParameter(atHashString("Mod_Slot_0_Upgrade_Level", 0x2E672FAF), engineBayUpgradeLevel0, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);
	dspSettings.AddDSPParameter(atHashString("Mod_Slot_1_Upgrade_Level", 0x4951A638), engineBayUpgradeLevel1, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
	dspSettings.AddDSPParameter(atHashString("Mod_Slot_1_Upgrade_Level", 0x4951A638), engineBayUpgradeLevel1, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);
	dspSettings.AddDSPParameter(atHashString("Mod_Slot_2_Upgrade_Level", 0xE26554E6), engineBayUpgradeLevel2, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
	dspSettings.AddDSPParameter(atHashString("Mod_Slot_2_Upgrade_Level", 0xE26554E6), engineBayUpgradeLevel2, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);
	dspSettings.AddDSPParameter(atHashString("FirstPerson", 0xEE38E8E0), firstPerson, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_ENGINE);
	dspSettings.AddDSPParameter(atHashString("FirstPerson", 0xEE38E8E0), firstPerson, audVehicleDSPSettings::AUD_VEHICLE_DSP_PARAM_EXHAUST);
}

// -------------------------------------------------------------------------------
// Check how much the engine is upgraded
// -------------------------------------------------------------------------------
f32 audVehicleAudioEntity::GetEngineUpgradeLevel() const
{
#if __BANK
	if(g_ForceUpgradeEngine || (g_EngineVolumeCaptureActive && audCarAudioEntity::sm_VolumeCaptureState == audCarAudioEntity::State_CapturingAccelModded))
	{
		return g_ForcedEngineUpgradeLevel;
	}
#endif

	if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_ENGINE) != INVALID_MOD)
	{
		if(VMT_ENGINE >= VMT_TOGGLE_MODS)
		{
			return 1.0f;
		}
		else
		{
			return m_Vehicle->GetVariationInstance().GetAudioApply(VMT_ENGINE);
		}
	}
	else
	{
		return 0.0f;
	}
}

// -------------------------------------------------------------------------------
// Check how much the engine bay is upgraded
// -------------------------------------------------------------------------------
f32 audVehicleAudioEntity::GetEngineBayUpgradeLevel(u32 slot) const
{
	audAssert(slot < 3);
	const u32 modIndex = VMT_ENGINEBAY1 + slot;

#if __BANK
	if(g_ForceUpgradeEngineBay[slot])
	{
		return g_ForcedEngineBayUpgradeLevel[slot];
	}
#endif

	if(m_Vehicle->GetVariationInstance().GetModIndex((eVehicleModType)modIndex) != INVALID_MOD)
	{
		if(modIndex >= VMT_TOGGLE_MODS)
		{
			return 1.0f;
		}
		else
		{
			return m_Vehicle->GetVariationInstance().GetAudioApply((eVehicleModType)modIndex);
		}
	}

	return 0.0f;
}

// -------------------------------------------------------------------------------
// Check how much the radio is upgraded
// -------------------------------------------------------------------------------
f32 audVehicleAudioEntity::GetRadioUpgradeLevel() const
{
#if __BANK
	if(g_ForceUpgradeRadio)
	{
		return g_ForcedRadioUpgradeLevel;
	}
#endif

	f32 upgradeLevel = 0.f;

	// Take the biggest upgrade provided by either the ICE or the Trunk speaker mods
	if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_ICE) != INVALID_MOD && !m_Vehicle->HasExpandedMods())
	{
		if(VMT_ICE >= VMT_TOGGLE_MODS)
		{
			upgradeLevel = 1.0f;
		}
		else
		{
			upgradeLevel = m_Vehicle->GetVariationInstance().GetAudioApply(VMT_ICE);
		}
	}

	if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_TRUNK) != INVALID_MOD)
	{
		if(VMT_TRUNK >= VMT_TOGGLE_MODS)
		{
			upgradeLevel = 1.0f;
		}
		else
		{
			upgradeLevel = Max(upgradeLevel, m_Vehicle->GetVariationInstance().GetAudioApply(VMT_TRUNK));
		}
	}

	return upgradeLevel;
}

// -------------------------------------------------------------------------------
// Check how much the transmission is upgraded
// -------------------------------------------------------------------------------
f32 audVehicleAudioEntity::GetTransmissionUpgradeLevel() const
{
#if __BANK
	if(g_ForceUpgradeTransmission)
	{
		return g_ForcedTransmissionUpgradeLevel;
	}
#endif

	if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_GEARBOX) != INVALID_MOD)
	{
		if(VMT_GEARBOX >= VMT_TOGGLE_MODS)
		{
			return 1.0f;
		}
		else
		{
			return m_Vehicle->GetVariationInstance().GetAudioApply(VMT_GEARBOX);
		}
	}
	else
	{
		return 0.0f;
	}
}

// -------------------------------------------------------------------------------
// Check how much the exhaust is upgraded
// -------------------------------------------------------------------------------
f32 audVehicleAudioEntity::GetExhaustUpgradeLevel() const
{
#if __BANK
	if(g_ForceUpgradeExhaust || (g_EngineVolumeCaptureActive && audCarAudioEntity::sm_VolumeCaptureState == audCarAudioEntity::State_CapturingAccelModded))
	{
		return g_ForcedExhaustUpgradeLevel;
	}
#endif

	if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_EXHAUST) != INVALID_MOD)
	{
		if(VMT_EXHAUST >= VMT_TOGGLE_MODS)
		{
			return 1.0f;
		}
		else
		{
			return m_Vehicle->GetVariationInstance().GetAudioApply(VMT_EXHAUST);
		}
	}
	else
	{
		return 0.0f;
	}
}

// -------------------------------------------------------------------------------
// Check how much the turbo is upgraded
// -------------------------------------------------------------------------------
f32 audVehicleAudioEntity::GetTurboUpgradeLevel() const
{
#if __BANK
	if(g_ForceUpgradeTurbo)
	{
		return g_ForcedTurboUpgradeLevel;
	}
#endif

	if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_TURBO) != INVALID_MOD)
	{
		if(VMT_TURBO >= VMT_TOGGLE_MODS)
		{
			return 1.0f;
		}
		else
		{
			return m_Vehicle->GetVariationInstance().GetAudioApply(VMT_TURBO);
		}
	}
	else
	{
		return 0.0f;
	}
}

// ----------------------------------------------------------------
// Check if we're forcing snow sounds (eg. Xmas DLC)
// ----------------------------------------------------------------
bool audVehicleAudioEntity::ShouldForceSnow()
{
	if(g_vfxWheel.GetUseSnowWheelVfxWhenUnsheltered())
	{
		if (m_EnvironmentGroup)
		{
			// If we're inside or under cover, don't use the snow settings
			if(m_EnvironmentGroup->IsUnderCover() || m_EnvironmentGroup->GetInteriorInst() != NULL)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

// ----------------------------------------------------------------
// Check if it is safe to apply DSP changes
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsSafeToApplyDSPChanges()
{
	// In network games, it's common for the focus vehicle to swap to another vehicle whilst the vehicle is actively revving
	// (eg. the player gets into a remote player's car as a passenger). This triggers a swap to the high quality/DSP'd version
	// of the asset, which can sound quite jarring mid-revs, especially on vehicles with more aggressive DSP settings. For this
	// reason, we wait until the car has returned to idle before switching on the DSP. In single player it is rarely an issue 
	// as you're generally pulling an NPC out of the car in order to get into it, so the car is idle anyway when it becomes the
	// focus vehicle. In future, we ideally want to build an 'apply' value into all of the synth effects so that we can smoothly
	// fade them in rather than just having a binary on/off.
#if __BANK
	if(g_AutoReapplyDSPPresets)
	{
		return true;
	}
#endif

	if(NetworkInterface::IsGameInProgress() && m_IsFocusVehicle)
	{
		if(IsUsingGranularEngine())
		{
			audVehicleEngine* vehicleEngine = GetVehicleEngine();

			if(vehicleEngine)
			{
				const audGranularEngineComponent* granularEngine = vehicleEngine->GetGranularEngine();
				
				if(granularEngine)
				{
					if(!m_Vehicle->m_nVehicleFlags.bEngineOn || m_ReapplyDSPEffects || 
					   granularEngine->GetIdleVolumeScale() >= 1.0f ||
					   vehicleEngine->GetTransmission()->GetLastGearChangeTimeMs() == fwTimer::GetTimeInMilliseconds())
					{
						m_ReapplyDSPEffects = false;
						return true;
					}
					else
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

// ----------------------------------------------------------------
// Update vehicle caterpillar tracks
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateCaterpillarTracks(audVehicleVariables& params)
{
	const u32 vehicleModelNameHash = GetVehicleModelNameHash();
	if (vehicleModelNameHash == ATSTRINGHASH("ZHABA", 0x4C8DBA51))
	{
		m_VehicleCaterpillarTrackSpeed = params.wheelSpeedAbs;
	}

	bool shouldPlayTrackSound = m_VehicleCaterpillarTrackSpeed > 0.01f;

	if (vehicleModelNameHash == ATSTRINGHASH("MINITANK", 0xB53C6C52))
	{
		shouldPlayTrackSound = m_Vehicle->IsEngineOn();
	}

	if(shouldPlayTrackSound)
	{
		if(!m_CaterpillarTrackLoop)
		{			
			u32 trackSoundSetName = 0u;
			u32 trackSoundName = ATSTRINGHASH("track_move_loop", 0xDC74446F);

			if(vehicleModelNameHash == ATSTRINGHASH("HALFTRACK", 0xFE141DA6))
			{
				trackSoundSetName = ATSTRINGHASH("halftrack_wheels_ss", 0x91D832CB);
			}
			else if (vehicleModelNameHash == ATSTRINGHASH("MINITANK", 0xB53C6C52))
			{
				if (m_IsFocusVehicle)
				{
					trackSoundSetName = ATSTRINGHASH("ch_vehicle_minitank_player_sounds", 0xCF2B0226);
				}
				else
				{
					trackSoundSetName = ATSTRINGHASH("ch_vehicle_minitank_remote_sounds", 0x90AF5388);
				}

				trackSoundName = ATSTRINGHASH("move_loop", 0xADFCA51D);
			}
			else if (vehicleModelNameHash == ATSTRINGHASH("ZHABA", 0x4C8DBA51))
			{
				trackSoundSetName = ATSTRINGHASH("zhaba_wheels_ss", 0x554AD4A9);
				trackSoundName = ATSTRINGHASH("move_loop", 0xADFCA51D);
			}
			else if(vehicleModelNameHash == ATSTRINGHASH("KHANJALI", 0xAA6F980A))
			{
				trackSoundSetName = ATSTRINGHASH("khanjali_wheels_ss", 0xFEB271E4);
			}
			else if (vehicleModelNameHash == ATSTRINGHASH("SCARAB", 0xBBA2A2F7) ||
					 vehicleModelNameHash == ATSTRINGHASH("SCARAB2", 0x5BEB3CE0) ||
					 vehicleModelNameHash == ATSTRINGHASH("SCARAB3", 0xDD71BFEB))
			{
				trackSoundSetName = ATSTRINGHASH("scarab_wheels_ss", 0x50C9A4F3);
			}			

			if(trackSoundSetName != 0)
			{
				audSoundSet soundSet;

				if(soundSet.Init(trackSoundSetName))
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.TrackEntityPosition = true;
					initParams.UpdateEntity = true;
					CreateAndPlaySound_Persistent(soundSet.Find(trackSoundName), &m_CaterpillarTrackLoop, &initParams);
				}
			}
		}

		if(m_CaterpillarTrackLoop)
		{
			m_CaterpillarTrackLoop->FindAndSetVariableValue(ATSTRINGHASH("trackspeed", 0xBDAAE8C), m_VehicleCaterpillarTrackSpeed);
			m_CaterpillarTrackLoop->FindAndSetVariableValue(ATSTRINGHASH("wheelTouchingFactor", 0x870DDA90), params.numWheelsTouchingFactor);
			m_CaterpillarTrackLoop->FindAndSetVariableValue(ATSTRINGHASH("throttleInput", 0x918028C4), params.throttleInput);
		}
		
		m_LastCaterpillarTrackValidTime = fwTimer::GetTimeInMilliseconds();
	}
	else if(m_CaterpillarTrackLoop)
	{
		if(fwTimer::GetTimeInMilliseconds() - m_LastCaterpillarTrackValidTime > 500)
		{
			m_CaterpillarTrackLoop->StopAndForget(true);
		}
	}	
}

// ----------------------------------------------------------------
// Calculate the wheel dirtiness
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWheelDirtyness(audVehicleVariables& variables)
{
	f32 dirtiness = 0.0f;
	f32 desiredDirtiness = m_CachedMaterialSettings? m_CachedMaterialSettings->Dirtiness : 0.0f;

#if __BANK
	if(g_ForceDirtySurface)
	{
		desiredDirtiness = 1.0f;
	}
#endif

	f32 dirtinessVolume = 0.0f;

	if(variables.numWheelsTouchingFactor > 0.0f)
	{
		dirtinessVolume = Max(0.0f, m_PrevDirtinessVolume - fwTimer::GetTimeStep() * 2.0f);

		// We want to interpolate to the material dirtiness - pick up/shed more dirt the faster we are traveling
		f32 speedRatio = Clamp(variables.fwdSpeedAbs/20.0f, 0.0f, 1.0f);
		f32 smoothRateIncrease = g_WheelDirtinessIncreaseSmoothRate * speedRatio;
		f32 smoothRateDecrease = g_WheelDirtinessDecreaseSmoothRate * speedRatio;
		m_WheelDirtinessSmoother.SetRates(smoothRateIncrease, smoothRateDecrease);

		f32 oldDirtiness = m_WheelDirtinessSmoother.GetLastValue();
		dirtiness = m_WheelDirtinessSmoother.CalculateValue(desiredDirtiness, variables.timeInMs);

		// If we're losing dirtyness by transitioning to a clean(ish) surface, play the sound
		if(oldDirtiness > dirtiness && desiredDirtiness <= 0.3f)
		{
			dirtinessVolume = Clamp(variables.fwdSpeedAbs/5.0f, 0.0f, 1.0f);

			if(dirtiness < desiredDirtiness + 0.3f)
			{
				dirtinessVolume *= (dirtiness/(desiredDirtiness + 0.3f));
			}
		}
	}

	const u32 dirtyWheelSound = m_VehicleType == AUD_VEHICLE_BICYCLE? ATSTRINGHASH("BicycleDirtyWheelLoop", 0x123EA33) : ATSTRINGHASH("CarDirtyWheelLoop", 0xC2701795);
	UpdateSoundsetLoopWithLinearVolumeAndPitch(&m_DirtyWheelLoop, dirtyWheelSound, &sm_ExtrasSoundSet, dirtinessVolume, variables.roadNoisePitch, m_EnvironmentGroup, NULL, true, true);
	m_PrevDirtinessVolume = dirtinessVolume;

	f32 glassiness = 0.0f;
	f32 desiredGlassiness = g_GlassAudioEntity.GetGlassiness(GetPosition(), true, fwTimer::GetTimeStep() * g_GlassAreaDepletionRate * variables.fwdSpeedRatio);

#if __BANK
	if(g_ForceGlassySurface)
	{
		desiredGlassiness = 1.0f;
	}
#endif

	f32 glassinessVolume = 0.0f;

	if(variables.numWheelsTouchingFactor > 0.0f)
	{
		glassinessVolume = Max(0.0f, m_PrevGlassinessVolume - fwTimer::GetTimeStep() * 2.0f);
		f32 smoothRateIncrease = g_WheelGlassinessIncreaseSmoothRate * Min(variables.fwdSpeedAbs, 1.0f);
		f32 smoothRateDecrease = g_WheelGlassinessDecreaseSmoothRate * Min(variables.fwdSpeedAbs/10.0f, 1.0f);
		m_WheelGlassinessSmoother.SetRates(smoothRateIncrease, smoothRateDecrease);
		glassiness = m_WheelGlassinessSmoother.CalculateValue(desiredGlassiness, variables.timeInMs);

		if(glassiness < 0.3f)
		{
			glassinessVolume *= glassiness/0.3f;
		}

		glassinessVolume = glassiness;
	}

	const u32 glassyWheelSound = m_VehicleType == AUD_VEHICLE_BICYCLE? ATSTRINGHASH("BicycleGlassyWheelLoop", 0x776F9315) : ATSTRINGHASH("CarGlassyWheelLoop", 0xF392D925);
	UpdateSoundsetLoopWithLinearVolumeAndPitch(&m_GlassyWheelLoop, glassyWheelSound, &sm_ExtrasSoundSet, glassinessVolume, variables.roadNoisePitch, m_EnvironmentGroup, NULL, true, true);
	m_PrevGlassinessVolume = glassinessVolume;

#if __BANK
	if(g_DisplayDirtGlassInfo && m_IsPlayerVehicle)
	{
		char tempString[128];
		sprintf(tempString, "Material Dirtiness: %.02f", desiredDirtiness);
		grcDebugDraw::Text(Vector2(0.08f, 0.1f), Color32(255,255,255), tempString);

		sprintf(tempString, "Wheel Dirtiness: %.02f", m_WheelDirtinessSmoother.GetLastValue());
		grcDebugDraw::Text(Vector2(0.08f, 0.12f), Color32(255,255,255), tempString);

		sprintf(tempString, "Ground Glassiness: %.02f", desiredGlassiness);
		grcDebugDraw::Text(Vector2(0.08f, 0.14f), Color32(255,255,255), tempString);

		sprintf(tempString, "Wheel Glassiness: %.02f", m_WheelGlassinessSmoother.GetLastValue());
		grcDebugDraw::Text(Vector2(0.08f, 0.16f), Color32(255,255,255), tempString);
	}
#endif
}

// ----------------------------------------------------------------
// Update skids
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateSkids(audVehicleVariables& params)
{
#if __BANK
	if (g_OverridenSkidPitchOffset != 0)
	{
		m_SkidPitchOffset = g_OverridenSkidPitchOffset;
	}
#endif

	// skids
	f32 sideVol;

	// take the largest fwd/lateral slip angle from all wheels
	f32 fwdSlipAngle = 0.f;
	
	const f32 numTyresScalar = 1.f / (f32)m_Vehicle->GetNumWheels();
	bool burnOut = false;

	float numDriveWheelsTouching = 0.f;
	u32 numDriveWheels = 0;
	f32 averageDriveWheelSpeed = 0.0f;

	s32 numWheelsTouching = 0;
	const f32 moveMag = params.movementSpeed;
	Vector3 normVel = m_CachedVehicleVelocity;
	normVel.NormalizeSafe();
	Vec3V normForward = m_Vehicle->GetTransform().GetB();
	normForward = NormalizeSafe(normForward,Vec3V(V_X_AXIS_WZERO));
	
	float slipAngleDeltas[NUM_VEH_CWHEELS_MAX] = {0.f};

	float meanDriveWheelSlip = 0.f;

	naCErrorf(m_Vehicle->GetNumWheels()<=NUM_VEH_CWHEELS_MAX, "Vehicle has too many wheels");
	for(s32 i = 0 ; i < m_Vehicle->GetNumWheels(); i++)
	{
		CWheel* thisWheel = m_Vehicle->GetWheel(i);

		if(thisWheel->GetIsDriveWheel())
		{
			numDriveWheels++;
		}

		if(thisWheel->GetIsTouching() && !m_Vehicle->m_nVehicleFlags.bIsDrowning && !thisWheel->GetDynamicFlags().IsFlagSet(WF_INSHALLOWWATER) && !thisWheel->GetDynamicFlags().IsFlagSet(WF_INDEEPWATER))
		{
			numWheelsTouching++;

			const float effectiveSlipAngle = thisWheel->GetEffectiveSlipAngle();
			const f32 healthScalar = m_Vehicle->GetWheel(i)->GetTyreHealth() * TYRE_HEALTH_DEFAULT_INV;
			fwdSlipAngle += effectiveSlipAngle * healthScalar * numTyresScalar;
			if(thisWheel->GetDynamicFlags().IsFlagSet(WF_BURNOUT))
			{
				burnOut = true;
			}

			if(thisWheel->GetIsDriveWheel())
			{
				averageDriveWheelSpeed += thisWheel->GetGroundSpeed();
				meanDriveWheelSlip += thisWheel->GetFwdSlipAngle() * healthScalar;
				numDriveWheelsTouching += 1.f;
			}

			const float slipAngle = thisWheel->GetSideSlipAngle();
			slipAngleDeltas[i] = (slipAngle - m_LastSlipAngle[i]) * fwTimer::GetInvTimeStep();
			m_LastSlipAngle[i] = slipAngle;
		}
		else
		{
			m_LastSlipAngle[i] = 0.f;
		}
	}

	f32 driftAngle = 0.f;

	f32 burnOutPitch = 0.f;

	const bool hasRearWheelSteering = m_Vehicle->InheritsFromAutomobile() && (((CAutomobile*)m_Vehicle)->pHandling->hFlags & HF_STEER_REARWHEELS);
	if(burnOut)
	{
		driftAngle = 1.f;
		burnOutPitch = Min(200.f, m_BurnoutPitch + fwTimer::GetTimeStep() * 150.f);

		// Don't play a squeal at the start of a burnout
		m_LastDriftAngle = driftAngle;
	}
	else
	{
		driftAngle = (moveMag>0.5f?(1.f - Abs(DotProduct(VEC3V_TO_VECTOR3(normForward), normVel))):0.f);
		burnOutPitch = Max(-200.f, m_BurnoutPitch - fwTimer::GetTimeStep() * 150.f);
	}

	if(hasRearWheelSteering)
	{
		driftAngle *= 0.3f;
	}
	
	u32 mainSkidHash = (m_CachedMaterialSettings?m_CachedMaterialSettings->MainSkid:g_MainTarmacSkid);

	const bool tarmacSkids = mainSkidHash == g_MainTarmacSkid;

	if(m_VehicleType == AUD_VEHICLE_BICYCLE && tarmacSkids)
	{
		mainSkidHash = g_MainTarmacSkidBicycle;
	}
	
	const float driftAngleDeltaS = (driftAngle - m_LastDriftAngle) * fwTimer::GetInvTimeStep();
	m_LastDriftAngle = driftAngle;

	if(numDriveWheelsTouching > 0)
	{
		meanDriveWheelSlip /= numDriveWheelsTouching;
	}
	
	// AI time-slicing is leading to jittery NPC skids under braking.  Ignore +ve slip if both throttle and brake are applied, and scale
	// down by brake
	if(!m_IsPlayerVehicle && meanDriveWheelSlip > 0.f)
	{
		meanDriveWheelSlip *= (1.f - Min(1.f, 3.f * params.throttleInput)) * (params.brakePedal * params.brakePedal);
	}
	meanDriveWheelSlip *= Min(1.f, 2.f * Max(params.wheelSpeedRatio, params.throttle));
	
	m_LastDriftAngleDelta = driftAngleDeltaS;

	f32 entityVariableLongSlipAngle = 0.0f;
	f32 entityVariableLatSlipAngle = 0.0f;

	if(HasEntityVariableBlock())
	{
		entityVariableLongSlipAngle = GetEntityVariableValue(ATSTRINGHASH("fakelongslip", 0x6E7FF3FC));
		entityVariableLatSlipAngle = GetEntityVariableValue(ATSTRINGHASH("fakelatslip", 0x34DA17A0));
	}

	m_BurnoutPitch = burnOutPitch;
	
	f32 driftScalar = 1.f;
	if(m_VehicleType != AUD_VEHICLE_BICYCLE)
	{
		driftScalar = m_LateralSkidSmoother.CalculateValue(sm_SkidDriftAngleToVolume.CalculateValue(driftAngle), fwTimer::GetTimeInMilliseconds());
	}
	
	fwdSlipAngle = m_MainSkidSmoother.CalculateValue(fwdSlipAngle, fwTimer::GetTimeInMilliseconds());

	s32 burnoutPitchCents = (s32)burnOutPitch;

	if(params.hasMaterialChanged)
	{
		if(m_MainSkidLoop)
		{
			m_MainSkidLoop->SetReleaseTime(300);
			m_MainSkidLoop->StopAndForget(true);
		}
		if(m_UndersteerSkidLoop)
		{
			m_UndersteerSkidLoop->SetReleaseTime(100);
			m_UndersteerSkidLoop->StopAndForget(true);
		}
		if(m_SideSkidLoop)
		{
			m_SideSkidLoop->SetReleaseTime(300);
			m_SideSkidLoop->StopAndForget(true);
		}
	}

#if __DEV
	if(g_BreakOnVehicleUpdate && g_AudioDebugEntity == m_Vehicle)
	{
		__debugbreak();
	}
#endif

	const s32 skidPitch = Max(burnoutPitchCents, (s32)(sm_SkidSpeedRatioToPitch.CalculateValue(params.fwdSpeedRatio)) + m_EnginePitchOffset);
	
	// compute amount of skidding 'on the water'
	params.wetRoadSkidModifier = Min(0.5f, sm_SkidFwdSlipToMainVol.CalculateValue(fwdSlipAngle) * 0.5f) * params.numWheelsTouchingFactor;

	// increase threshold and decrease volume with road wetness, only on tarmac
	float mainVol = UpdateSkidsVolume(params, tarmacSkids, numWheelsTouching, fwdSlipAngle, slipAngleDeltas);

	const float burnoutOverride = m_BurnoutSmoother.CalculateValue(burnOut ? 1.f : 0.f, params.timeInMs);

	float angVelMag = m_CachedAngularVelocity.Mag() / (2.f * PI);
	const float mainSpeedFactor = Max(burnoutOverride, Min(1.f, params.fwdSpeedRatio * 2.f));

	float finalMainVol = mainSpeedFactor * mainVol * Min(1.f,(2.f*driftScalar)) * params.numWheelsTouchingFactor * (1.0f - params.numDriveWheelsInWaterFactor);

	// Allow entity variable to drive volume directly
	finalMainVol = Max(finalMainVol, entityVariableLongSlipAngle);
	
	static float s_DriveWheelSlipScalar = 2.f * PI;
	float burnoutScalar = Lerp(params.throttle, params.fwdSpeedRatio, 1.f);

	bool isQuietVehicle = false;

	if(m_VehicleType == AUD_VEHICLE_CAR && ((audCarAudioEntity*)this)->IsElectricOrHybrid())
	{
		isQuietVehicle = true;
	}

	if(tarmacSkids && g_DisableBurnoutSkidsInReverse && !isQuietVehicle)
	{
		// lots of wheel spin sounds wrong on tarmac, but necessary for off-road materials
		burnoutScalar *= (m_Vehicle->m_Transmission.GetGear() <= 0 ? 0.f : 1.f);
	}
	
	if(hasRearWheelSteering)
	{
		driftScalar *= 0.5f;
		fwdSlipAngle *= 0.5f;
		angVelMag *= 0.5f;
	}
	
	float driveWheelSlipVol = Min(1.f, Abs(meanDriveWheelSlip * burnoutScalar) / s_DriveWheelSlipScalar);

	driveWheelSlipVol *= driveWheelSlipVol;
	driveWheelSlipVol *= GetSkidVolumeScale();

	// Special case speculative bug fix for NPC vehicles getting wheel slip loop stuck on. Don't allow it to 
	// play if the vehicle isn't moving and no throttle is being applied
	if(!m_IsPlayerVehicle && params.fwdSpeedAbs < 0.1f && params.throttle == 0.0f && !burnOut)
	{
		driveWheelSlipVol = 0.0f;
	}
	
	// side skid volume
	const float sideSpeedFactor = Max(burnoutOverride, Min(1.f, Max(params.wheelSpeedRatio * 2.f, 2.f*angVelMag)));
	sideVol = sideSpeedFactor * Min(1.f, 1.2f*mainVol) * driftScalar * params.numWheelsTouchingFactor * (1.0f - params.numDriveWheelsInWaterFactor) * GetSkidVolumeScale();
	sideVol *= GetSkidVolumeScale();

	float driftChangeVol = 0.f;

	// High rate of drift angle change triggers also side skid
	static float s_DriftAngleDeltaScalar = 3.f;
	float driftChangeFactor = Min(1.f, Abs(driftAngleDeltaS) * s_DriftAngleDeltaScalar);

	// Recorded vehicles don't have correct slip angles (the wheels are asleep so don't get physics updates) so increase the drift contribution
	// to ensure that we still get squeals when cornering
	if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle) && params.numWheelsTouchingFactor == 1.0f)
	{
		driftChangeFactor *= 5.0f;
		driftChangeFactor = m_DriftFactorSmoother.CalculateValue(driftChangeFactor);
	}

	driftChangeVol = params.numWheelsTouchingFactor * (1.0f - params.numDriveWheelsInWaterFactor) * Min(1.f, 2.f*params.wheelSpeedRatio) * driftChangeFactor * 1.2f;
	driftChangeVol *= GetSkidVolumeScale();

	// Muting bicycle drifts during car recordings - bike velocity vs wheel angle is generally incorrect, giving us skids when we don't want them
	if(m_VehicleType == AUD_VEHICLE_BICYCLE && CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle))
	{
		sideVol = 0.0f;
	}
	else
	{
		sideVol = Max(sideVol, Min(0.7f, driftChangeVol * driftChangeVol));
		sideVol = Max(sideVol, entityVariableLatSlipAngle);
	}
	
	const u32 sideSkidHash = (m_CachedMaterialSettings?m_CachedMaterialSettings->SideSkid:g_SideTarmacSkid);
	
	float mainSkidVolume = 0.f;
	if(m_VehicleType == AUD_VEHICLE_BICYCLE)
	{
		mainSkidVolume = Max(finalMainVol, sideVol);
	}
	else if((m_Vehicle->InheritsFromBike() || hasRearWheelSteering) && tarmacSkids)
	{
		// main skid is too much for bikes and the forklift
		sideVol = Max(finalMainVol, sideVol);
		mainVol = 0.f;
	}
	else
	{
		mainSkidVolume = tarmacSkids ? finalMainVol : Max(finalMainVol, driveWheelSlipVol);
	}

	float understeerVolume = 0.f;
	float understeerThrottleScalar = Lerp(params.throttle, 0.85f, 1.f);

	TUNE_FLOAT(bikeSlipScalar, 1.7f, 0.f, 10.f, 0.1f)
	float fwdSlipAngleForUndersteer = fwdSlipAngle;
	if (m_Vehicle->InheritsFromBike())
		fwdSlipAngleForUndersteer *= bikeSlipScalar;
	understeerVolume = Min(1.f, GetSkidVolumeScale() * Lerp(mainVol, 1.f, 0.5f) * understeerThrottleScalar * Min(1.5f, params.fwdSpeedRatio * 2.4f) * sm_SlipToUndersteerVol.CalculateValue(fwdSlipAngleForUndersteer));

	// Non-tarmac skids don't have a bespoke understeer loop, so allow understeer to contribute to main skid volume
	if(GetAudioVehicleType() == AUD_VEHICLE_CAR && !tarmacSkids BANK_ONLY(&& g_UndersteerAffectsMainSkidVol))
	{
		mainSkidVolume = Max(mainSkidVolume, understeerVolume);
	}

	// Drop out all tarmac skids if all the wheels have totally burst (as these are very obviously rubber-on-tarmac sounds)
	if(tarmacSkids && GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		if(params.numNonBurstWheelsTouching == 0)
		{
			mainSkidVolume = 0.0f;
			sideVol = 0.0f;
			driveWheelSlipVol = 0.0f;
			understeerVolume = 0.0f;
		}
	}

	if (IsToyCar())
	{
		f32 maxVolume = Max(Max(mainSkidVolume, sideVol), understeerVolume);

		if (!m_MainSkidLoop && m_IsFocusVehicle && maxVolume > 0)
		{
			audSoundSet soundSet;

			if (soundSet.Init(ATSTRINGHASH("rcbandito_sounds", 0x7B9D9FE0)))
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;
				CreateAndPlaySound_Persistent(soundSet.Find(ATSTRINGHASH("skid_loop", 0x63D72B3C)), &m_MainSkidLoop, &initParams);
			}
		}

		if (m_MainSkidLoop)
		{
			if (!m_IsFocusVehicle || maxVolume <= 0)
			{
				m_MainSkidLoop->StopAndForget(true);
			}
			else
			{
				f32 throttleInput = Abs(params.throttle);

				if (!CReplayMgr::IsEditModeActive())
				{
					throttleInput = Max(throttleInput, Abs(params.throttleInput));
				}

				const f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;				

				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("maxSkidVolume", 0x2628A402), maxVolume);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("mainSkidVolume", 0x6A01D3CB), mainSkidVolume);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("understeerVolume", 0x3EF3A4EA), understeerVolume);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("sideSkidVolume", 0x8C3D7FDC), sideVol);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("surfaceHardness", 0x55D4D89C), m_CachedMaterialSettings ? m_CachedMaterialSettings->FootstepMaterialHardness : 0.f);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), throttleInput);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("rollAngle", 0x94FC4D71), rollAngle);
				m_MainSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("steerAngle", 0x6D2AC1CE), Abs(m_Vehicle->GetSteerAngle()) * RtoD);
			}
		}
	}
	else
	{
		UpdateLoopWithLinearVolumeAndPitch(&m_MainSkidLoop, mainSkidHash, mainSkidVolume, skidPitch + m_SkidPitchOffset, m_EnvironmentGroup, g_SkidCategory, true, false);

		if(sideSkidHash != g_NullSoundHash && m_VehicleType != AUD_VEHICLE_BICYCLE)
		{
			UpdateLoopWithLinearVolumeAndPitch(&m_SideSkidLoop, sideSkidHash, sideVol, skidPitch + m_SkidPitchOffset, m_EnvironmentGroup, g_SkidCategory, true, false);
		}

		if(tarmacSkids)
		{
			const f32 wetSkidVol = 0.55f * g_weather.GetWetness() * GetSkidVolumeScale() * sm_SkidFwdSlipToMainVol.CalculateValue(fwdSlipAngle) * sideSpeedFactor * params.numWheelsTouchingFactor;
			UpdateLoopWithLinearVolumeAndPitch(&m_WetSkidLoop, sm_TarmacSkidSounds.Find(ATSTRINGHASH("WetSkid", 0xCFD760FC)), wetSkidVol, skidPitch + m_SkidPitchOffset, m_EnvironmentGroup, g_SkidCategory, true, false);

			// Only play drivewheelslip and understeer for cars
			if(GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				/*if(driveWheelSlipVol > 0.1f)
				{
				char message[128];
				formatf(message, "DWS: %f, meanDWS: %f, speed: %f throttle: %f brake: %f", driveWheelSlipVol, meanDriveWheelSlip, params.fwdSpeed, params.throttleInput, params.brakePedal);
				grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition() + Vec3V(0.f,0.f,2.f), Color32(255,0,0), 0,0, message, true, 1000);
				}*/
				UpdateLoopWithLinearVolumeAndPitch(&m_DriveWheelSlipLoop, sm_TarmacSkidSounds.Find(ATSTRINGHASH("DriveWheelSlip", 0x95EDB1AB)), driveWheelSlipVol, m_SkidPitchOffset, m_EnvironmentGroup, g_SkidCategory, true, false);
				UpdateLoopWithLinearVolumeAndPitch(&m_UndersteerSkidLoop, sm_TarmacSkidSounds.Find(ATSTRINGHASH("Understeer", 0x6605017D)), understeerVolume, m_SkidPitchOffset + (m_Vehicle->InheritsFromBike() ? 200 : 0), m_EnvironmentGroup, g_SkidCategory, true, false);

				if(m_UndersteerSkidLoop)
				{		
					const float scrapeSkidRatio = Min(1.f, Max(params.fwdSpeedRatio, angVelMag));
					m_UndersteerSkidLoop->FindAndSetVariableValue(ATSTRINGHASH("scrapeSkidRatio", 0xB3882CB), scrapeSkidRatio);	
				}
			}
		}
		else
		{
			if(m_UndersteerSkidLoop)
			{
				m_UndersteerSkidLoop->StopAndForget(true);
			}
			if(m_DriveWheelSlipLoop)
			{
				m_DriveWheelSlipLoop->StopAndForget(true);
			}
			if(m_WetSkidLoop)
			{
				m_WetSkidLoop->StopAndForget(true);
			}
		}
	}	

	params.skidLevel = Max(Max(mainSkidVolume, sideVol), driveWheelSlipVol);

	if (!IsToyCar())
	{
		const float envLoudness = Lerp(params.skidLevel*params.skidLevel, 0.5f, 1.f);
		const float skidRolloff = Lerp(params.skidLevel, 1.f, 2.5f);
		const u8 envLoudnessU8 = (u8)(255.f * envLoudness);
		if (m_MainSkidLoop)
		{
			m_MainSkidLoop->SetRequestedEnvironmentalLoudness(envLoudnessU8);
			m_MainSkidLoop->SetRequestedVolumeCurveScale(skidRolloff);
		}
		if (m_SideSkidLoop)
		{
			m_SideSkidLoop->SetRequestedEnvironmentalLoudness(envLoudnessU8);
			m_SideSkidLoop->SetRequestedVolumeCurveScale(skidRolloff);
		}
		if (m_DriveWheelSlipLoop)
		{
			m_DriveWheelSlipLoop->SetRequestedEnvironmentalLoudness(envLoudnessU8);
			m_DriveWheelSlipLoop->SetRequestedVolumeCurveScale(skidRolloff);
		}
	}	
	
#if __BANK
	if(m_IsPlayerVehicle && g_DebugVehicleSkids)
	{	
		static const char * wheelMeterNames[] = {"Fwd", "Effective", "Side"};
		static audMeterList wheelMeterList[4];
		static u32 numWheelMeters = 3;
		static f32 wheelMeterValues[12];		
		
		for (s32 wheelIndex = 0; wheelIndex < m_Vehicle->GetNumWheels() && wheelIndex < 4; wheelIndex++)
		{
			CWheel* thisWheel = m_Vehicle->GetWheel(wheelIndex);

			if(thisWheel)
			{
				s32 i = wheelIndex * 3;
				wheelMeterValues[i++] = thisWheel->GetFwdSlipAngle();
				wheelMeterValues[i++] = thisWheel->GetEffectiveSlipAngle();
				wheelMeterValues[i++] = thisWheel->GetSideSlipAngle();

				audMeterList& meterList = wheelMeterList[wheelIndex];
				meterList.left = 100.f;
				meterList.bottom = 250.f + (220 * wheelIndex);
				meterList.width = 220.f;
				meterList.height = 180.f;
				meterList.rangeMax = 15.f;
				meterList.values = &wheelMeterValues[wheelIndex * 3];
				meterList.names = wheelMeterNames;
				meterList.numValues = numWheelMeters;
				meterList.drawValues = true;

				audNorthAudioEngine::DrawLevelMeters(&meterList);
			}
		}

		static const char *meterNames[] = {"Skid Level", "Main Vol", "Side Vol", "Understeer Vol", "Drift Ang", "Angular Vel", "Fwd Speed Ratio", "Drift Angle Delta"};
		const u32 numMeters = sizeof(meterNames)/sizeof(meterNames[0]);
		static audMeterList meterList;
		static f32 meterValues[numMeters];

		s32 i = 0;
		meterValues[i++] = params.skidLevel;
		meterValues[i++] = finalMainVol;
		meterValues[i++] = sideVol;
		meterValues[i++] = understeerVolume;
		meterValues[i++] = driftAngle;
		meterValues[i++] = angVelMag;
		meterValues[i++] = params.fwdSpeedRatio;
		
		meterValues[i++] = Abs(driftAngleDeltaS);
		meterList.left = 430.f;
		meterList.bottom = 320.f;
		meterList.width = 1100.f;
		meterList.height = 300.f;
		meterList.values = &meterValues[0];
		meterList.names = meterNames;
		meterList.numValues = numMeters;
		audNorthAudioEngine::DrawLevelMeters(&meterList);
	}
#endif // __BANK
}

float audVehicleAudioEntity::UpdateSkidsVolume(audVehicleVariables& params, bool tarmacSkids, s32 numWheelsTouching, float fwdSlipAngle, const float* slipAngleDeltas)
{
	const f32 wetModifier = Max(0.25f, 1.f - g_weather.GetWetness());

	float mainVol;
	if(tarmacSkids)
	{	
		mainVol = sm_SkidFwdSlipToMainVol.CalculateValue(fwdSlipAngle * wetModifier) * (1.f - g_weather.GetWetness()*0.5f);

		// Disable chirps for bikes until the ABS bug is fixed
		if(AreTyreChirpsEnabled() && m_IsPlayerVehicle && GetAudioVehicleType() == AUD_VEHICLE_CAR && !IsToyCar())
		{
			if(params.fwdSpeedRatio > 0.1f && m_Vehicle->GetBrake() >= 1.0f && numWheelsTouching == m_Vehicle->GetNumWheels())
			{
				if(!m_TyreChirpBrakePlayed)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.TrackEntityPosition = true;
					initParams.Pitch = (s16)m_SkidPitchOffset;
					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(Min(1.f, 2.5f * params.fwdSpeedRatio * m_Vehicle->GetBrake()));
					CreateAndPlaySound(sm_TarmacSkidSounds.Find(ATSTRINGHASH("HeavyBrakingChirp", 0xE79FD624)), &initParams);
					m_TyreChirpBrakePlayed = true;
				}
			}

			{

				for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
				{
					static float s_ChirpDeltaThreshold = 32.f;

					if(Abs(slipAngleDeltas[i]) > s_ChirpDeltaThreshold)
					{
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						initParams.u32ClientVar = GetWheelSoundUpdateClientVar(i);
						initParams.UpdateEntity = true;
						initParams.Pitch = (s16)m_SkidPitchOffset;
						initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(Min(1.f, GetSkidVolumeScale() * params.fwdSpeedRatio*2.f));
						GetCachedWheelPosition(i, initParams.Position, params);

						CreateAndPlaySound(sm_TarmacSkidSounds.Find(ATSTRINGHASH("TyreChirps", 0xEF29A2D8)), &initParams);
					}
				}
			}
		}
	}
	else
	{
		mainVol = sm_SkidFwdSlipToMainVol.CalculateValue(fwdSlipAngle);
	}
	mainVol *= GetSkidVolumeScale();

	if(m_TyreChirpBrakePlayed && fwdSlipAngle * wetModifier < 0.25f && m_Vehicle->GetBrake() <= 0.25f)
	{
		m_TyreChirpBrakePlayed = false;
	}

	return mainVol;
}

// ----------------------------------------------------------------
audMetadataRef audVehicleAudioEntity::GetHornLoopRef(const u32 hornPattern)
{
	audMetadataRef hornLoop = g_NullSoundRef;
	hornLoop = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(hornPattern);
	bool useSirenForHorn = false;
	const bool alarmActive = m_Vehicle->IsAlarmActivated();
	if(!alarmActive)
	{
		if(m_Vehicle->UsesSiren() && m_UseSirenForHorn)
		{
			useSirenForHorn = audEngineUtil::ResolveProbability(0.2f);
		}
		if(useSirenForHorn && m_VehSirenSounds.IsInitialised())
		{
			hornLoop = m_VehSirenSounds.Find(audEngineUtil::ResolveProbability(0.5f) ?  ATSTRINGHASH("HORN_SLOW", 0xD91C6C0F): ATSTRINGHASH("HORN_FAST", 0xCB4A8157));
		}
		if(m_SirenLoop && m_VehSirenSounds.IsInitialised())
		{
			hornLoop = m_VehSirenSounds.Find(ATSTRINGHASH("BLIP", 0x26D96F23));
		}
	}
	if(m_OverrideHorn)
	{
		hornLoop = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(m_OverridenHornHash);
	}
	return hornLoop;
}
// ----------------------------------------------------------------
// Update the horn and siren
// ----------------------------------------------------------------
void audVehicleAudioEntity::HornOffHandler(u32 /*hash*/, void *context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		CVehicle* vehicle = ped->GetVehiclePedInside();
		if(vehicle && vehicle->GetVehicleAudioEntity())
		{
			vehicle->GetVehicleAudioEntity()->SetHornPermanentlyOn(false);
			vehicle->GetVehicleAudioEntity()->StopHorn();
		}
	}
}

void audVehicleAudioEntity::TriggerAlarm()
{
	if(HasAlarm() && !m_Vehicle->IsAlarmActivated())
	{
		m_HasToTriggerAlarm = true;
	}
}
void audVehicleAudioEntity::PlayVehicleAlarm()
{
	if (m_AlarmType == AUD_CAR_ALARM_TYPE_HORN)
	{
		naAssertf(m_Vehicle->IsAlarmActivated(),"Trying to play the alarm but it is not active");
		m_LastTimeAlarmPlayed = fwTimer::GetTimeInMilliseconds();
		m_RandomAlarmInterval = audEngineUtil::GetRandomNumberInRange(0,150);
		if(m_LastTimeAlarmPlayed > (2u * (sm_AlarmIntervalInMs + m_RandomAlarmInterval)))
			m_LastTimeAlarmPlayed -= (2u * (sm_AlarmIntervalInMs + m_RandomAlarmInterval));
		m_LastTimeAlarmPlayed += audEngineUtil::GetRandomNumberInRange(0,1000);
		//PlayHeldDownHorn(sm_AlarmIntervalInMs/1000.f);
	}
}
void audVehicleAudioEntity::UpdateHorn()
{
#if __BANK
	if(g_DisableHorns)
	{
		StopAndForgetSounds(m_HornSound);
		return;
	}
#endif
	if(!m_IsHornEnabled)
	{
		StopHorn();
		return;
	}
	if(!m_WasInCarModShop && m_InCarModShop)
	{
		StopHorn();
	}
	m_WasInCarModShop = m_InCarModShop;
	naAssertf(m_Vehicle, "m_Vehicle must be valid, about to access a null ptr...");
	if(m_Vehicle->GetStatus() != STATUS_WRECKED)
	{
		if(m_IsHornPermanentlyOn)
		{
			m_WasHornPermanentlyOn = m_IsHornPermanentlyOn;
			m_IsHornPermanentlyOn = false;
			if(!m_HornSound)
			{
				PlayHeldDownHorn(-1.f);
			}
		}
		else if (m_WasHornPermanentlyOn)
		{
			m_WasHornPermanentlyOn = false;
			if(m_HornSound)
			{
				m_HornSound->StopAndForget();
			}
		}
	}
	else 
	{
		if (m_HornSound)
		{
			m_HornSound->StopAndForget();
		}
	}

	if(m_HornSound)
	{
		// Disable doppler on the player's horn if we're just in a regular vehicle camera (prevents pitching when spinning the camera around)
		if(m_IsPlayerVehicle && !camInterface::GetCinematicDirector().IsAnyCinematicContextActive())
		{
			m_HornSound->SetRequestedDopplerFactor(0.0f);			
		}
		else
		{
			m_HornSound->SetRequestedDopplerFactor(1.0f);
		}
	}

	if(m_AlarmType > AUD_CAR_ALARM_TYPE_HORN && m_AlarmType <= AUD_CAR_ALARM_TYPE_4)
	{
		const bool alarmActive = m_Vehicle->IsAlarmActivated();
		if(m_AlarmSound && !alarmActive)
		{
			m_AlarmSound->StopAndForget();
		}
		else
		{
			if(alarmActive && !m_AlarmSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				initParams.Predelay = audEngineUtil::GetRandomNumberInRange(0,1100);
				initParams.StartOffset = audEngineUtil::GetRandomNumberInRange(0,99);
				initParams.IsStartOffsetPercentage = true;
				char soundName[32];
				formatf(soundName, sizeof(soundName), "VEHICLES_EXTRAS_CAR_ALARM_%u", m_AlarmType);
				CreateAndPlaySound_Persistent(soundName, &m_AlarmSound, &initParams);
			}
		}
	}
	else if (m_AlarmType == AUD_CAR_ALARM_TYPE_HORN)
	{
		const bool alarmActive = m_Vehicle->IsAlarmActivated();
		if(alarmActive)
		{
			if(!m_HornSound)
			{
				if (m_LastTimeAlarmPlayed + (2u * (sm_AlarmIntervalInMs + m_RandomAlarmInterval)) < fwTimer::GetTimeInMilliseconds())
				{
					// already had sm_AlarmIntervalInMs milliseconds gap, play again 
					PlayHeldDownHorn((sm_AlarmIntervalInMs + m_RandomAlarmInterval)/1000.f);
					m_LastTimeAlarmPlayed = fwTimer::GetTimeInMilliseconds();
				}
			}
			m_WasAlarmActive = true;
		}
		else if(m_HornSound && m_WasAlarmActive)
		{
			m_HornSound->StopAndForget();
			m_WasAlarmActive = false;
		}
	}
}

void audVehicleAudioEntity::OnScriptSetHydraulicWheelState()
{
	CAutomobile* automobile = static_cast<CAutomobile*>(m_Vehicle);
	f32 suspensionAngleUnitCircleX = 0.f;
	f32 suspensionAngleUnitCircleY = 0.f;

	for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
	{
		f32 xAxis = 0.0;
		f32 yAxis = 0.0;

		CWheel *pWheel = m_Vehicle->GetWheel(i);

		if(pWheel)
		{
			switch(pWheel->GetHierarchyId())
			{
			case VEH_WHEEL_LF:
				xAxis = -0.707f;
				yAxis = 0.707f;
				break;
			case VEH_WHEEL_LR:
				xAxis = -0.707f;
				yAxis = -0.707f;
				break;
			case VEH_WHEEL_RR:
				xAxis = 0.707f;
				yAxis = -0.707f;
				break;
			case VEH_WHEEL_RF:
				xAxis = 0.707f;
				yAxis = 0.707f;
				break;
			default:
				break;
			}

			suspensionAngleUnitCircleX += (xAxis * pWheel->GetSuspensionTargetRaiseAmount() / automobile->m_fHydraulicsUpperBound);
			suspensionAngleUnitCircleY += (yAxis * pWheel->GetSuspensionTargetRaiseAmount() / automobile->m_fHydraulicsUpperBound);
		}
	}

	SetHydraulicSuspensionInputAxis(suspensionAngleUnitCircleX, suspensionAngleUnitCircleY);
}

void audVehicleAudioEntity::RequestHydraulicActivation()
{ 
	m_ShouldTriggerHydraulicActivate = true; 
	m_HydraulicActivationDelayFrames = g_NumHydraulicActivationDelayFrames; 
}

void audVehicleAudioEntity::UpdateSiren(audVehicleVariables& vehicleVariables)
{
	naAssertf(m_Vehicle, "m_Vehicle must be valid, about to access a null ptr...");
	if(!m_Vehicle->UsesSiren())
	{
		return;
	}
	if(m_WantsToPlaySiren)
	{
		m_TimeToPlaySiren += fwTimer::GetTimeStepInMilliseconds();
		if(m_TimeToPlaySiren >= g_TimeToPlaySiren)
		{
			m_ShouldPlayPlayerVehSiren = true;
		}
	}
	bool enteringExitingVehicle = false;
	if(m_Vehicle->GetDriver())
	{
		enteringExitingVehicle = m_Vehicle->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle);
	}

	bool hasDriver = m_IsSirenForcedOn || !enteringExitingVehicle;

	const bool isEngineOn = m_IsSirenForcedOn || m_Vehicle->m_nVehicleFlags.bEngineOn;

	bool isPlayer = false;
	if(m_Vehicle->GetDriver() && m_Vehicle->GetDriver()->IsPlayer())
	{
		isPlayer = true;
	}
	u32 sirenSlow = ((m_IsSirenFucked && isPlayer) ? ATSTRINGHASH("FUCKED", 0x4DD32855) : ATSTRINGHASH("SLOW", 0xC4EE147F));

	bool isIceVan = false;
    // Please implement icecream truck through modelinfo or vehicle flags ... MI currently not in metadata
	//else if(m_Vehicle->GetModelIndex() == MI_CAR_ICEVAN)
	//{
	//	isIceVan = true;
	//	sirenSlow = iceVanSiren;
	//}

	if(m_Vehicle->GetStatus() == STATUS_WRECKED && !m_WasWrecked)
	{
		m_WasWrecked = true;

		if(isIceVan)
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(ATSTRINGHASH("ICECREAM_MELTING", 0xAC534109), &initParams);
		}
	}

	bool mustPlaySiren = m_IsSirenForcedOn || m_MustPlaySiren;
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	mustPlaySiren = mustPlaySiren || (playerVeh == m_Vehicle);
	bool isSirenOn = m_Vehicle->m_nVehicleFlags.GetIsSirenAudioOn();
	if( !m_SirenLoop && (!mustPlaySiren || !isSirenOn))
	{
		return;
	}
	if(m_Vehicle->GetDriver())
	{
		m_IsSirenOn = isSirenOn && !(m_Vehicle->GetDriver()->GetSpeechAudioEntity() && m_Vehicle->GetDriver()->GetSpeechAudioEntity()->IsMegaphoneSpeechPlaying()) && !m_Vehicle->GetDriver()->IsDead();
	}
	else
	{
		if(NetworkInterface::IsGameInProgress())
		{
			CPed* player = CGameWorld::FindLocalPlayer();
			if (player)
			{
				m_IsSirenOn = m_IsSirenOn && isSirenOn && (m_SirenBypassMPDriverCheck || m_PlaySirenWithNoNetworkPlayerDriver || SUPERCONDUCTOR.GetVehicleConductor().PlaySirenWithNoNetworkPlayerDriver());
			}
		}
		else
		{
			m_IsSirenOn = isSirenOn && SUPERCONDUCTOR.GetVehicleConductor().PlaySirenWithNoDriver();
		}
	}
	isSirenOn = mustPlaySiren && m_IsSirenOn && m_ShouldPlayPlayerVehSiren && !m_WasWrecked;
	
	switch(m_SirenState)
	{
	case AUD_SIREN_OFF:
		if(m_SirenLoop && !IsHornOn())
		{
			m_SirenLoop->StopAndForget(true);
		}

		if((!m_IsSirenFucked || isPlayer) && isSirenOn) 
		{
			if(IsHornOn() && !m_IsSirenFucked && !isIceVan)
			{
				// fast siren when horn is pressed
				m_SirenState = AUD_SIREN_FAST;
			}
			else
			{
				m_SirenState = AUD_SIREN_SLOW;
			}
		}
		break;
	case AUD_SIREN_SLOW:
		if(!isSirenOn)
		{
			if(m_SirenLoop)
			{
				if(m_Vehicle->GetStatus() == STATUS_WRECKED)
				{
					m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("isFucked", 0x82DA7FEC), 1.f);
				}
				m_SirenLoop->StopAndForget(true);
			}
			m_PlaySirenWithNoNetworkPlayerDriver = false;
			m_SirenState = AUD_SIREN_OFF;
		}
		else if(IsHornOn() && !isIceVan && !m_IsSirenFucked )
		{
			// switch to fast siren
			if(m_SirenLoop)
			{
				m_SirenLoop->StopAndForget(true);
			}
			if(m_Vehicle->GetDriver() && !m_Vehicle->GetDriver()->IsDead())
			{
				m_SirenState = AUD_SIREN_FAST;
				m_LastSirenChangeTime = fwTimer::GetTimeInMilliseconds();
			}
		}
		else if(!isIceVan && !isPlayer && m_Vehicle->m_nVehicleFlags.bApproachingJunctionWithSiren && m_Vehicle->GetDriver() && vehicleVariables.fwdSpeed > 5.f)
		{
			if(fwTimer::GetTimeInMilliseconds() > m_LastSirenChangeTime + 4000)
			{
				if(m_SirenLoop)
				{
					m_SirenLoop->StopAndForget(true);
				}

				m_SirenState = AUD_SIREN_WARNING;
				m_LastSirenChangeTime = fwTimer::GetTimeInMilliseconds();
			}
		}
		else if(!m_SirenLoop && !m_SirenBlip)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.UpdateEntity = true;
			initParams.TrackEntityPosition = true;
			initParams.RemoveHierarchy = false;
			if((m_Vehicle->GetDriver() && !m_Vehicle->GetDriver()->IsLocalPlayer()))
			{
				// random predelay so that lots of sirens starting on the same frame don't sound ridiculous
				initParams.Predelay = audEngineUtil::GetRandomNumberInRange(0, 350);
			}
			if(m_VehSirenSounds.IsInitialised())
			{
				CreateSound_PersistentReference(m_VehSirenSounds.Find(sirenSlow), &m_SirenLoop, &initParams);
			}
			if(m_SirenLoop)
			{
				if(isIceVan)
				{
					// hack hack hack :) use the smoothing to stop the pitch bend at the end of the note, when the retriggered overlap
					// kicks off the mathop sound to choose the next random pitch.
					m_SirenLoop->SetRequestedFrequencySmoothingRate(0.0f);

					// step through all ice van tunes since playing the same one more than once at a time causes bad things to happen
					f32 var;
					bool hasVar = m_SirenLoop->FindVariableValue(g_VariationVariableHash, var);
					const u32 numTunes = (u32)(hasVar?var+1:1);
					m_SirenLoop->FindAndSetVariableValue(g_VariationVariableHash, static_cast<f32>(g_IceVanTune));
					// lookup this variation tempo scaling
					m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("tempoScaling", 0xB4873DA8), g_IceVanTempoScalingCurve.CalculateValue(static_cast<f32>(g_IceVanTune)));
					g_IceVanTuneTextId = 400 + g_IceVanTune;
					g_IceVanTune = (g_IceVanTune + 1) % numTunes;

					// turn off the player radio so that they can enjoy the icevan tunes that I've worked so hard on :)
					if(m_Vehicle->GetDriver() && m_Vehicle->GetDriver()->IsLocalPlayer())
					{
						NA_RADIO_ENABLED_ONLY(g_RadioAudioEntity.RetuneToStation(ATSTRINGHASH("OFF", 0x77E9145)));
					}
				}
				m_SirenLoop->PrepareAndPlay();
			}

		}
		break;
	case AUD_SIREN_FAST:

		if(!IsHornOn() || isIceVan)
		{
			//switch to slow siren
			if(m_SirenLoop)
			{
				m_SirenLoop->StopAndForget(true);
			}
			m_SirenState = AUD_SIREN_SLOW;
			m_LastSirenChangeTime = fwTimer::GetTimeInMilliseconds();
		}
		else if(!m_SirenLoop && !m_SirenBlip)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			initParams.UpdateEntity = true;
			u32 soundHash = ATSTRINGHASH("WARNING", 0xE91F183C);
			// sometimes trigger the warning for player vehicles
			if((m_Vehicle->GetDriver() && isPlayer) && m_Vehicle->GetNumSequentialSirenPresses() == 1)
			{
				soundHash = ATSTRINGHASH("FAST", 0x64B8047E);
			}
			m_Vehicle->ResetNumSequentialSirenPresses();

			if(m_VehSirenSounds.IsInitialised())
			{
				CreateAndPlaySound_Persistent(m_VehSirenSounds.Find(soundHash), &m_SirenLoop, &initParams);
			}
		}
		break;
	case AUD_SIREN_WARNING:
		if(!m_IsSirenOn || !isEngineOn || !hasDriver || isIceVan)
		{
			if(m_SirenLoop)
			{
				m_SirenLoop->StopAndForget(true);
			}
			m_PlaySirenWithNoNetworkPlayerDriver = false;
			m_SirenState = AUD_SIREN_OFF;
		}
		else if(fwTimer::GetTimeInMilliseconds() > m_LastSirenChangeTime + 1000 && (!m_Vehicle->m_nVehicleFlags.bApproachingJunctionWithSiren || vehicleVariables.fwdSpeed <= 5.f || fwTimer::GetTimeInMilliseconds() > m_LastSirenChangeTime + 3000))
		{
			if(m_SirenLoop)
			{
				m_SirenLoop->StopAndForget(true);
			}
			if(IsHornOn())
			{
				m_SirenState = AUD_SIREN_FAST;
			}
			else
			{
				m_SirenState = AUD_SIREN_SLOW;
			}

			m_LastSirenChangeTime = fwTimer::GetTimeInMilliseconds();
		}
		else if(!m_SirenLoop  && !m_SirenBlip)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.UpdateEntity = true;
			initParams.TrackEntityPosition = true;
			if(m_VehSirenSounds.IsInitialised())
			{
				CreateAndPlaySound_Persistent(m_VehSirenSounds.Find(ATSTRINGHASH("WARNING", 0xE91F183C)), &m_SirenLoop, &initParams);
			}
		}
		break;
	}

	if(m_SirenLoop)
	{
		if(isIceVan)
		{
			const f32 damageFactor = 1.f - (m_Vehicle->GetHealth() * 0.001f);
			const f32 pitchRange = 0.17f + (damageFactor*0.8f);
			const f32 delay = (0.250f * (1.f - vehicleVariables.fwdSpeedRatio));

			m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("minPitch", 0x60D1797E), -pitchRange);
			m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("maxPitch", 0x3FA79219), pitchRange);
			m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("delayTime", 0xDA06E848), delay);
			m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("playDirection", 0x4A65EC8F), (vehicleVariables.fwdSpeed < -0.5f ? -1.f : 1.f));
		}
		else
		{
			// dont cone the ice cream van.
			m_SirenLoop->SetRequestedOrientation(m_Vehicle->GetMatrix());
		}
		// Conductors siren volume.
		f32 newVolume = m_SirensVolumeSmoother.CalculateValue(m_SirensDesireLinVol, vehicleVariables.timeInMs);
		m_SirenLoop->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(newVolume));
		// if the siren is muted and we want to stop it
		if(newVolume == 0.f && m_ShouldStopSiren)
		{
			// set Must play siren (it will stop the siren )
			m_MustPlaySiren = false;
		}
	}

	const bool enableStereoSirens = g_EnableStereoSirens && m_IsFocusVehicle && NetworkInterface::IsInCopsAndCrooks();

	if (m_SirenLoop)
	{		
		m_SirenLoop->SetRequestedPan(enableStereoSirens ? 0 : -1);
	}
	
	if (m_SirenBlip)
	{
		m_SirenBlip->SetRequestedPan(enableStereoSirens ? 0 : -1);
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity UpdateWarningSounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateWarningSounds(audVehicleVariables& vehicleVariables)
{
	if(vehicleVariables.distFromListenerSq < 900)
	{
		// default to true for door open warning
		CCarDoor* pDoor = m_Vehicle->GetDoorFromId(VEH_DOOR_DSIDE_F);
		if(pDoor && HasDoorOpenWarning())
		{
			const bool shouldPlayDoorWarning = m_Vehicle->m_nVehicleFlags.bLightsOn && !pDoor->GetIsClosed() && m_Vehicle->IsEngineOn() && !m_InCarModShop;

			if(m_DoorOpenWarning)
			{
				if(!shouldPlayDoorWarning)
				{
					m_DoorOpenWarning->StopAndForget();
				}
			}
			else
			{
				if(shouldPlayDoorWarning)
				{
					audSoundInitParams initParams;
					f32 atten;
					u32 cutoff;
					GetInteriorSoundOcclusion(atten,cutoff);
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.LPFCutoff = cutoff;
					initParams.Volume = atten;
					initParams.TrackEntityPosition = true;
					initParams.UpdateEntity = true;
					CreateSound_PersistentReference(sm_ExtrasSoundSet.Find(ATSTRINGHASH("Warning_DoorOpen", 0xB8D9B657)), &m_DoorOpenWarning, &initParams);
					if(m_DoorOpenWarning)
					{
						m_DoorOpenWarning->SetClientVariable((u32)AUD_VEHICLE_SOUND_INTERIOR_OCCLUSION);
						m_DoorOpenWarning->PrepareAndPlay();
					}
				}
			}
		}

		if(HasReverseWarning())
		{
			bool inReverseGear = m_Vehicle->m_Transmission.GetGear() <= 0;

			// We can't trust car recordings to be in the correct gear - but if they're going backwards then it should be
			// safe to asssume that they're reversing intentionally
			if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle))
			{
				inReverseGear = true;
			}

			inReverseGear |= m_ForceReverseWarning;

			if(m_ReverseWarning)
			{
				if(!inReverseGear || vehicleVariables.fwdSpeed >= 0.f || !m_Vehicle->m_nVehicleFlags.bEngineOn)
				{
					m_ReverseWarning->StopAndForget();
				}
			}
			else
			{
				if(inReverseGear && vehicleVariables.fwdSpeed < -0.5f && m_Vehicle->m_nVehicleFlags.bEngineOn)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.TrackEntityPosition = true;
					initParams.UpdateEntity = true;

					u32 customReverseWarningSoundHash = GetReverseWarningSound();

					if(customReverseWarningSoundHash != g_NullSoundHash)
					{
						CreateAndPlaySound_Persistent(customReverseWarningSoundHash, &m_ReverseWarning, &initParams);
					}
					else
					{
						CreateAndPlaySound_Persistent(sm_ExtrasSoundSet.Find(ATSTRINGHASH("Warning_Reversing", 0x1AE37740)), &m_ReverseWarning, &initParams);
					}
				}
			}
		}	
	}
	else
	{
		StopAndForgetSounds(m_DoorOpenWarning, m_ReverseWarning);
	}
}

// ----------------------------------------------------------------
// Get the default vehicle collision settings
// ----------------------------------------------------------------
VehicleCollisionSettings* audVehicleAudioEntity::GetVehicleCollisionSettings() const
{
	static u32 propTrailerHash = ATSTRINGHASH("proptrailer", 0x153e1b0a);
	if(propTrailerHash == m_Vehicle->GetBaseModelInfo()->GetModelNameHash())
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(ATSTRINGHASH("VEHICLE_COLLISION_PROP_TRAILER", 0x49b15570));
	}
	return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(ATSTRINGHASH("VEHICLE_COLLISION_CAR", 0xC2FB47));
}

// ----------------------------------------------------------------
// Compute the engine health
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::ComputeEffectiveEngineHealth() const
{
	const f32 maxHealth = CTransmission::GetEngineHealthMax();
	const f32 knackeredCarHealth = Min((f32)m_FakeEngineHealth, maxHealth);
	const f32 drowningEngineHealth = GetAudioVehicleType() == AUD_VEHICLE_BOAT? maxHealth : Lerp(m_DrowningFactor, maxHealth, 0.f);
	f32 engineHealth = Min((1.0f - m_ScriptEngineDamageFactor) * maxHealth, Min(knackeredCarHealth, m_Vehicle->GetVehicleDamage()->GetEngineHealth(), drowningEngineHealth));

#if __BANK
	if(g_ForceDamage && m_IsPlayerVehicle)
	{
		engineHealth = maxHealth * (1.0f - g_ForcedDamageFactor);
	}
#endif

	return engineHealth;
}

// ----------------------------------------------------------------
// Fix the vehicle
// ----------------------------------------------------------------
void audVehicleAudioEntity::Fix()
{
	FixSiren(); 
	m_ScriptEngineDamageFactor = 0.0f; 
	m_ScriptBodyDamageFactor = 0.0f; 
	m_FakeBodyHealth = static_cast<s16>(CVehicleDamage::GetBodyHealthMax());
	m_FakeEngineHealth = static_cast<s16>(CTransmission::GetEngineHealthMax());
};

// ----------------------------------------------------------------
// Compute the vehicle's body damage factor
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::ComputeEffectiveBodyDamageFactor() const
{
	const f32 maxHealth = CVehicleDamage::GetBodyHealthMax();
	f32 bodyDamageFactor = Max(m_ScriptBodyDamageFactor, 1.0f - (m_Vehicle->GetVehicleDamage()->GetBodyHealth() / maxHealth));
	bodyDamageFactor = Max(bodyDamageFactor, 1.0f - (m_FakeBodyHealth/1000.0f));

#if __BANK
	if(g_ForceDamage)
	{
		bodyDamageFactor = g_ForcedDamageFactor;
	}
#endif

	return bodyDamageFactor;
}

// ----------------------------------------------------------------
// Compute the vehicle suspension health
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::ComputeEffectiveSuspensionHealth() const
{
	f32 averageSuspensionHealth = 0.f;
	
	for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
	{
		CWheel* wheel = m_Vehicle->GetWheel(loop);

		if(wheel && wheel->GetTyreHealth() > 0.0f)
		{
			averageSuspensionHealth += wheel->GetSuspensionHealth();
		}			
	}	

	// Suspension health as 0-1 range
	averageSuspensionHealth /= m_Vehicle->GetNumWheels();
	averageSuspensionHealth = Clamp(averageSuspensionHealth * 0.001f, 0.0f, 1.0f);

#if __BANK
	if(g_OverrideHydraulicSuspensionDamage)
	{
		averageSuspensionHealth = g_OverridenHydraulicSuspensionDamage;
	}
#endif

	return averageSuspensionHealth;
}

// ----------------------------------------------------------------
// Update chassis stress sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateClatter(audVehicleVariables& state)
{
	const Vector3 clatterWorldPos = m_Vehicle->TransformIntoWorldSpace(m_ClatterOffsetPos) - VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
    Vector3 delta = (clatterWorldPos - m_LastClatterWorldPos) * fwTimer::GetInvTimeStep();
    const f32 spinVelocity = Abs(DotProduct(delta, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetA())));
	const f32 forwardVelocity = Abs(DotProduct(delta, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB())));
	const f32 velocityRatio =  spinVelocity/forwardVelocity;		   
	f32 bodyDamageFactor = ComputeEffectiveBodyDamageFactor();	

	const f32 clatterUpVelocity = DotProduct(delta, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetC()));
	const f32 clatterAcceleration = clatterUpVelocity - m_LastClatterUpVel;

	if(state.timeInMs - m_TimeCreated > 500 && (!m_Vehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing) || m_VisibilitySmoother.GetLastValue() >= 0.5f))
	{
		// acceleration changed direction
		// NOTE: minimum of 0.4f contribution from speed, all the way up to 1.4 (so small bumps at high speeds are pretty loud)
		// NOTE: 0.4 now scaled by wheel speed to prevent sounds triggering when stationary (eg. when physics pop slightly when transitioning between LODs)
		const f32 rescaledSpeedRatio = (0.4f * Min(1.0f, state.wheelSpeed)) + Min(1.f,state.wheelSpeedRatio);
		f32 clatterAccelerationCR = Abs(clatterAcceleration - m_LastClatterAcceleration) * fwTimer::GetInvTimeStep();
		f32 heavyVehicleScaler = HasHeavyRoadNoise()? 2.5f : 1.0f;
		f32 trailerScaler = 1.0f;

		// Scale up sensitivity on big trailers Small trailers have mass ~1000, massive ones are ~3500
		if(m_Vehicle->InheritsFromTrailer())
		{
			trailerScaler = 8.0f * ClampRange(m_Vehicle->pHandling->m_fMass - 1000.0f, 0.0f, 2500.0f);
		}

        // Ignore chassis stress if hydraulics are fully raised (prevents squeaks when initially raising)
        if(m_Vehicle->m_nVehicleFlags.bAreHydraulicsAllRaised)
        {
            clatterAccelerationCR = 0.f;
        }

		// Bump up chassis stress when stationary at low speed
		f32 hydraulicsChassisStressScalar =  AreHydraulicsActive()? Lerp(1.0f - Min(state.fwdSpeedRatio/0.5f, 1.0f), 1.0f, g_HydraulicsChassisStressSensitivityMultiplier) : 1.0f;
		f32 clatterRatio = rescaledSpeedRatio * trailerScaler * heavyVehicleScaler * Min(1.f, (clatterAccelerationCR * GetClatterSensitivityScalar()) / g_ClatterMaxAcceleration);
		f32 chassisStressRatio = m_ChassisStressSmoother.CalculateValue((AreHydraulicsActive() ? 1.0f : rescaledSpeedRatio) * Min(1.f, (clatterAccelerationCR * GetChassisStressSensitivityScalar() * hydraulicsChassisStressScalar) / g_ChassisStressMaxAcceleration), state.timeInMs);        

		if(m_Vehicle->m_nVehicleFlags.bIsDrowning || g_ReflectionsAudioEntity.IsStuntTunnelMaterial(GetMainWheelMaterial()))
		{
			clatterRatio = 0.f;  
			chassisStressRatio = 0.f;
		}

		if(GetVehicleModelNameHash() == ATSTRINGHASH("DELUXO", 0x586765FB) && m_Vehicle->GetSpecialFlightModeRatio() > 0.f)
		{
			chassisStressRatio = 0.f;
		}

		audMetadataRef clatterSoundRef = g_NullSoundRef;		

		if(m_Vehicle->m_nVehicleFlags.bIsBeingTowed)
		{
			audSoundSet soundSet;
			soundSet.Init(GetTowTruckSoundSet());
			clatterSoundRef = soundSet.Find(ATSTRINGHASH("TowedVehicleClatter", 0xA1510B56));
		}
		else
		{
			clatterSoundRef = GetClatterSound();		
		}

		if(m_CachedMaterialSettings)
		{
			// add some random clatter when driving on cobbles
			clatterRatio = Min(1.f, clatterRatio + (state.wheelSpeedRatio * audEngineUtil::GetRandomNumberInRange(m_CachedMaterialSettings->Roughness/2.0f,m_CachedMaterialSettings->Roughness)));
			chassisStressRatio = Min(1.f, chassisStressRatio + (state.wheelSpeedRatio * audEngineUtil::GetRandomNumberInRange(m_CachedMaterialSettings->Roughness/2.0f,m_CachedMaterialSettings->Roughness)));
		}

		f32 clatterVolumeLin = clatterRatio;
		clatterVolumeLin *= audDriverUtil::ComputeLinearVolumeFromDb(3.0f * state.engineDamageFactor);

		f32 chassisStressVolumeLin = chassisStressRatio;
		chassisStressVolumeLin *= audDriverUtil::ComputeLinearVolumeFromDb(3.0f * state.engineDamageFactor);

		// Chassis stress comes in with damage on non-truck vehicles
		if(!HasHeavyRoadNoise() && !AreHydraulicsActive())
		{
			chassisStressVolumeLin *= 0.3f + (0.7f * Min(bodyDamageFactor/0.6f, 1.0f));
		}

		if(!m_VehicleClatter)
		{
			if(state.numWheelsTouching > 0 &&
				clatterRatio > 0.3f && 
				!HydraulicsModifiedRecently())
			{
				TriggerClatter(clatterSoundRef, clatterVolumeLin);

				if(forwardVelocity > 0.f && state.wheelSpeedAbs > g_SpeedForSpinDebris && state.movementSpeed > g_SpeedForSpinDebris
					&& velocityRatio > g_SpinRatioForDebris && bodyDamageFactor > g_BodyDamageFactorForDebris)
				{
					GetCollisionAudio().TriggerDebrisSounds(velocityRatio); 
				}
			}
		}
		else
		{
			f32 currentVolumeLin = audDriverUtil::ComputeLinearVolumeFromDb(m_VehicleClatter->GetRequestedVolume());

			if(clatterVolumeLin > currentVolumeLin)
			{
				// Smooth out the volume increase if we get a sudden spike
				clatterVolumeLin = Min(currentVolumeLin + (10.0f * fwTimer::GetTimeStep()), clatterVolumeLin);
				m_VehicleClatter->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(clatterVolumeLin) + GetClatterVolumeBoost());
			}
		}

		if(chassisStressVolumeLin > g_SilenceVolumeLin)
		{
			if(!m_ChassisStressLoop)
			{
				const audMetadataRef chassisStressSoundRef = GetChassisStressSound();
				if(chassisStressSoundRef != g_NullSoundRef)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.TrackEntityPosition = true;
					initParams.u32ClientVar = AUD_VEHICLE_SOUND_HEAVY_CLATTER;
					initParams.UpdateEntity = true;
					CreateAndPlaySound_Persistent(chassisStressSoundRef, &m_ChassisStressLoop, &initParams);
				}
			}
		}
		else if(m_ChassisStressLoop)
		{
			m_ChassisStressLoop->StopAndForget();
		}

		if(m_ChassisStressLoop)
		{
			m_ChassisStressLoop->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(chassisStressVolumeLin) + GetChassisStressVolumeBoost());
		}

		if(m_IsPlayerVehicle)
		{
			PF_SET(ClatterAcceleration, clatterAccelerationCR);
			PF_SET(ClatterRatioValue, clatterRatio);
		}
	}
	else if(m_ChassisStressLoop)
	{
		m_ChassisStressLoop->StopAndForget();
	}

	m_LastClatterAcceleration = clatterAcceleration;
	m_LastClatterUpVel = clatterUpVelocity;				
	m_LastClatterWorldPos = clatterWorldPos;
}

// ----------------------------------------------------------------
// Trigger some clatter
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerClatter(audMetadataRef clatterSoundRef, f32 volumeLin)
{
	if(!m_VehicleClatter)
	{
		if(clatterSoundRef != g_NullSoundRef && 
			fwTimer::GetTimeInMilliseconds() > m_NextClatterTime && 
			volumeLin > g_SilenceVolumeLin 
			BANK_ONLY(&& !g_DisableClatter))
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_HEAVY_CLATTER;
			initParams.UpdateEntity = true;
			CreateAndPlaySound_Persistent(clatterSoundRef, &m_VehicleClatter, &initParams);
			m_NextClatterTime = fwTimer::GetTimeInMilliseconds() + (u32)audEngineUtil::GetRandomNumberInRange(300,500);

			if(m_VehicleClatter)
			{
				m_VehicleClatter->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volumeLin) + GetClatterVolumeBoost());

				if(m_IsPlayerVehicle)
				{
					if(audNorthAudioEngine::ShouldTriggerPulseHeadset())
					{
						audSound* clatterPulseSound = NULL;

						CreateSound_LocalReference(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("VehicleClatter", 0x75805775)), &clatterPulseSound);

						if(clatterPulseSound)
						{	
							clatterPulseSound->FindAndSetVariableValue(ATSTRINGHASH("CLATTERVOL", 0x9E3FAEBB), volumeLin);
							clatterPulseSound->PrepareAndPlay();
						}
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Get the clatter sound for the given type
// ----------------------------------------------------------------
audMetadataRef audVehicleAudioEntity::GetClatterSoundFromType(ClatterType clatterType)
{
	audMetadataRef clatterRef = g_NullSoundRef;

	switch(clatterType)
	{
	case AUD_CLATTER_TRUCK_CAB:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("TruckCab", 0x755FCFFE));
		break;
	case AUD_CLATTER_TRUCK_CAGES:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("TruckCages", 0x736A7A81));
		break;
	case AUD_CLATTER_DETAIL:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("Detailed", 0xDCE667E1));
		break;
	case AUD_CLATTER_TAXI:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("Taxi", 0xC703DB5F));
		break;
	case AUD_CLATTER_BUS:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("Bus", 0xD577C962));
		break;
	case AUD_CLATTER_TRANSIT:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("TransitVan", 0x72B297F4));
		break;
	case AUD_CLATTER_TRANSIT_CLOWN:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("TransitVanClown", 0x7E67381B));
		break;
	case AUD_CLATTER_TRUCK_TRAILER_CHAINS:
		clatterRef = sm_ClatterSoundSet.Find(ATSTRINGHASH("TruckTrailerChains", 0xCCD2EB7C));
		break;
	default:
		break;
	}

	return clatterRef;
}

// ----------------------------------------------------------------
// Check if a stunt race is active
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsStuntRaceActive()
{
	return SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() && NetworkInterface::IsGameInProgress();
}

// ----------------------------------------------------------------
// Preload the vehicle audio for a particular vehicle
// ----------------------------------------------------------------
bool audVehicleAudioEntity::PreloadScriptVehicleAudio(const u32 vehicleModelNameHash, s32 scriptID)
{	
	if(sm_ScriptVehicleRequestScriptID == -1 || scriptID == sm_ScriptVehicleRequestScriptID)
	{
		if(sm_ScriptVehicleRequest != vehicleModelNameHash)
		{		
			if(sm_ScriptVehicleWaveSlot)
			{
				audWaveSlotManager::FreeWaveSlot(sm_ScriptVehicleWaveSlot);
			}

			audDisplayf("PRELOAD_VEHICLE_AUDIO_BANK - script requesting preload for vehicle %u", vehicleModelNameHash);
			sm_ScriptVehicleRequest = vehicleModelNameHash;

			if(vehicleModelNameHash != 0u)
			{
				sm_ScriptVehicleRequestScriptID = scriptID;
			}
			else
			{
				sm_ScriptVehicleRequestScriptID = -1;
			}
		}

		return true;
	}

	return true;
}

// ----------------------------------------------------------------
// Get the chassis stress sound for the given type
// ----------------------------------------------------------------
audMetadataRef audVehicleAudioEntity::GetChassisStressSoundFromType(ClatterType clatterType)
{
	audMetadataRef chassisStressRef = g_NullSoundRef;

	switch(clatterType)
	{
	case AUD_CLATTER_TRUCK_CAB:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("TruckCab", 0x755FCFFE));
		break;
	case AUD_CLATTER_TRUCK_CAGES:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("TruckCages", 0x736A7A81));
		break;
	case AUD_CLATTER_DETAIL:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("Detailed", 0xDCE667E1));
		break;
	case AUD_CLATTER_TAXI:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("Taxi", 0xC703DB5F));
		break;
	case AUD_CLATTER_BUS:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("Bus", 0xD577C962));
		break;
	case AUD_CLATTER_TRANSIT:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("TransitVan", 0x72B297F4));
		break;
	case AUD_CLATTER_TRANSIT_CLOWN:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("TransitVanClown", 0x7E67381B));
		break;
	case AUD_CLATTER_TRUCK_TRAILER_CHAINS:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("TruckTrailerChains", 0xCCD2EB7C));
		break;
	default:
		chassisStressRef = sm_ChassisStressSoundSet.Find(ATSTRINGHASH("Detail", 0x677DF1B8));
		break;
	}

	return chassisStressRef;
}

// ----------------------------------------------------------------
// Update entity sounds
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs)
{
	Vector3 pos;

	u32 clientVariable;
	reqSets->GetClientVariable(clientVariable);
	audVehicleSounds soundId = (audVehicleSounds)clientVariable;
	u32 cutoff = kVoiceFilterLPFMaxCutoff;

	if((soundId & 0xffff) == AUD_VEHICLE_SOUND_CUSTOM_BONE)
	{
		u32 boneIndex = soundId >> 16;
		reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(boneIndex).d));
	}
	else
	{
		switch(soundId)
		{
		case AUD_VEHICLE_SOUND_WHEEL0:
		case AUD_VEHICLE_SOUND_WHEEL1:
		case AUD_VEHICLE_SOUND_WHEEL2:
		case AUD_VEHICLE_SOUND_WHEEL3:
		case AUD_VEHICLE_SOUND_WHEEL4:
		case AUD_VEHICLE_SOUND_WHEEL5:
			GetWheelPosition(clientVariable - (u32)AUD_VEHICLE_SOUND_WHEEL0, pos);
			reqSets->SetPosition(pos);
			break;
		case AUD_VEHICLE_SOUND_WHEEL6:
		case AUD_VEHICLE_SOUND_WHEEL7:
			GetWheelPosition((clientVariable-(u32)AUD_VEHICLE_SOUND_WHEEL6) + 6, pos);
			reqSets->SetPosition(pos);
			break;
		case AUD_VEHICLE_SOUND_INTERIOR_OCCLUSION:
			f32 atten;
			GetInteriorSoundOcclusion(atten,cutoff);
			reqSets->SetVolume(atten);
			break;
		case AUD_VEHICLE_SOUND_ENGINE:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_EngineOffsetPos));
			break;
		case AUD_VEHICLE_SOUND_HORN:
			pos.Zero();
			GetHornOffsetPos(pos);
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(pos));
			if(m_Vehicle->ContainsLocalPlayer() && !m_Vehicle->IsAlarmActivated())
			{
				reqSets->SetVolume(g_PlayerHornOffset);
			}
			else
			{
				reqSets->SetVolume(0.f);
			}
			reqSets->SetOrientation(m_Vehicle->GetPlaceableTracker()->GetOrientation());
			break;
		case AUD_VEHICLE_SOUND_SCRAPE:
		case AUD_VEHICLE_SOUND_FOLIAGE:
			GetCollisionAudio().UpdateSound(sound, reqSets, timeInMs);
			break;
		case AUD_VEHICLE_SOUND_LIGHTCOVER:
			{
				const s32 lightCoverBone = m_Vehicle->GetBoneIndex(VEH_LIGHTCOVER);

				if(lightCoverBone > -1)
				{
					Matrix34 boneMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(lightCoverBone));
					reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(boneMatrix.d));				
				}
			}
			break;
		case AUD_VEHICLE_SOUND_SPOILER_STRUTS:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(VEH_STRUTS)).d));
			break;
		case AUD_VEHICLE_SOUND_SPOILER:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(VEH_SPOILER)).d));		
			break;
		case AUD_VEHICLE_SOUND_AIRBRAKE_L:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(HELI_AIRBRAKE_L)).d));		
			break;
		case AUD_VEHICLE_SOUND_AIRBRAKE_R:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(HELI_AIRBRAKE_R)).d));		
			break;
		case AUD_VEHICLE_SOUND_SUBMARINECAR_PROPELLOR:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace((m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(SUBMARINECAR_PROPELLER_1)).d + m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(SUBMARINECAR_PROPELLER_2)).d) * 0.5f));
			break;		
		case AUD_VEHICLE_SOUND_SUBMARINE_PROPELLOR:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace((m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(SUB_PROPELLER_1)).d + m_Vehicle->GetObjectMtx(m_Vehicle->GetBoneIndex(SUB_PROPELLER_2)).d) * 0.5f));
			break;
		case AUD_VEHICLE_SOUND_EXHAUST:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos));
			break;
		case AUD_VEHICLE_SOUND_HEAVY_CLATTER:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_ClatterOffsetPos));
			break;
		case AUD_VEHICLE_SOUND_BLADE_0:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetWeaponBlade(0).GetFastBone() != -1 ? m_Vehicle->GetWeaponBlade(0).GetFastBone() : m_Vehicle->GetWeaponBlade(0).GetModBone()).d));
			break;
		case AUD_VEHICLE_SOUND_BLADE_1:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetWeaponBlade(1).GetFastBone() != -1 ? m_Vehicle->GetWeaponBlade(1).GetFastBone() : m_Vehicle->GetWeaponBlade(1).GetModBone()).d));			
			break;
		case AUD_VEHICLE_SOUND_BLADE_2:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetWeaponBlade(2).GetFastBone() != -1 ? m_Vehicle->GetWeaponBlade(2).GetFastBone() : m_Vehicle->GetWeaponBlade(2).GetModBone()).d));
			break;
		case AUD_VEHICLE_SOUND_BLADE_3:
			reqSets->SetPosition(m_Vehicle->TransformIntoWorldSpace(m_Vehicle->GetObjectMtx(m_Vehicle->GetWeaponBlade(3).GetFastBone() != -1 ? m_Vehicle->GetWeaponBlade(3).GetFastBone() : m_Vehicle->GetWeaponBlade(3).GetModBone()).d));
			break;
		case AUD_VEHICLE_SOUND_CAR_RECORDING:
			if(!CVehicleRecordingMgr::IsPlaybackGoingOnForCar(m_Vehicle))
			{
				sound->StopAndForget();
			}
			break;
		default:
			break;
		}
	}	

	cutoff = Min(cutoff, CalculateVehicleSoundLPFCutoff());
	reqSets->SetLowPassFilterCutoff(cutoff);
}

u32 audVehicleAudioEntity::GetWheelSoundUpdateClientVar(u32 wheelIndex)
{
	// These are no longer consecutive in the enum so we can't simply do (AUD_VEHICLE_SOUND_WHEEL0 + i)
	switch (wheelIndex)
	{
	case 0:
		return AUD_VEHICLE_SOUND_WHEEL0;
	case 1:
		return AUD_VEHICLE_SOUND_WHEEL1;
	case 2:
		return AUD_VEHICLE_SOUND_WHEEL2;
	case 3:
		return AUD_VEHICLE_SOUND_WHEEL3;
	case 4:
		return AUD_VEHICLE_SOUND_WHEEL4;
	case 5:
		return AUD_VEHICLE_SOUND_WHEEL5;
	case 6:
		return AUD_VEHICLE_SOUND_WHEEL6;
	case 7:
		return AUD_VEHICLE_SOUND_WHEEL7;
	case 8:
		return AUD_VEHICLE_SOUND_WHEEL8;
	case 9:
		return AUD_VEHICLE_SOUND_WHEEL9;
	case 10:
		return AUD_VEHICLE_SOUND_WHEEL10;
	default:
		audAssertf(false, "Wheel index %d has no matching sound update enum", wheelIndex);
		break;
	}

	return AUD_VEHICLE_SOUND_UNKNOWN;
}

// ----------------------------------------------------------------
// CalculateVehicleSoundCutoff
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::CalculateVehicleSoundLPFCutoff()
{
	f32 cutoff = kVoiceFilterLPFMaxCutoff;

	// If the player is in bonnet cam, low pass filter sounds from all other vehicles
	if(audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
	{
		if(!m_IsFocusVehicle)
		{
			static const float minInteriorViewCutoff = 1000.0f;
			const f32 interiorViewCutoff =  minInteriorViewCutoff + ((kVoiceFilterLPFMaxCutoff - minInteriorViewCutoff) * sm_PlayerVehicleOpenness);
			cutoff = Min(cutoff, interiorViewCutoff);
		}
	}

	f32 minUnderwaterCutoff = 1000.0f;
	
	// all sounds get lowpassed when underwater
	if(IsSubmersible())
	{
		minUnderwaterCutoff = g_SubmersibleUnderwaterCutoff;
	}
	
	const f32 underwaterCutoff =  minUnderwaterCutoff + ((kVoiceFilterLPFMaxCutoff - minUnderwaterCutoff) * (1.0f - m_DrowningFactor));
	return (u32)Min(cutoff, underwaterCutoff);
}

// ----------------------------------------------------------------
// audVehicleAudioEntity::DeployOutriggers
// ----------------------------------------------------------------
void audVehicleAudioEntity::DeployOutriggers(bool deploy)
{
	if(!IsDisabled() && GetVehicleModelNameHash() == ATSTRINGHASH("CHERNOBOG", 0xD6BC7523) REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		TriggerSimpleSoundFromSoundset(ATSTRINGHASH("chernobog_barrage_sounds", 0x9EB5B64D), deploy? ATSTRINGHASH("activate", 0xE64E0807) : ATSTRINGHASH("deactivate", 0x51AFCCE6));
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity::StartGliding
// ----------------------------------------------------------------
void audVehicleAudioEntity::StartGliding()
{	
	if(!IsDisabled() && GetVehicleModelNameHash() == ATSTRINGHASH("OPPRESSOR", 0x34B82784) REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		TriggerSimpleSoundFromSoundset(m_IsFocusVehicle? ATSTRINGHASH("DLC_Gunrunning_Oppressor_Sounds", 0xC98DA953) : ATSTRINGHASH("DLC_Gunrunning_Oppressor_NPC_Sounds", 0x23FDA269), ATSTRINGHASH("wings_out", 0xA92ED867));
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity::StopGliding
// ----------------------------------------------------------------
void audVehicleAudioEntity::StopGliding()
{	
	if(!IsDisabled() && GetVehicleModelNameHash() == ATSTRINGHASH("OPPRESSOR", 0x34B82784) REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		TriggerSimpleSoundFromSoundset(m_IsFocusVehicle? ATSTRINGHASH("DLC_Gunrunning_Oppressor_Sounds", 0xC98DA953) : ATSTRINGHASH("DLC_Gunrunning_Oppressor_NPC_Sounds", 0x23FDA269), ATSTRINGHASH("wings_in", 0x862E16BE));
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity::TriggerSimpleSoundFromSoundset
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSimpleSoundFromSoundset(u32 soundSetHash, u32 soundFieldHash REPLAY_ONLY(, bool recordSound))
{
	if(soundSetHash != 0u)
	{
		audSoundSet soundSet;

		if(soundSet.Init(soundSetHash))
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.UpdateEntity = true;
			CreateDeferredSound(soundSet.Find(soundFieldHash), m_Vehicle, &initParams, true);

#if GTA_REPLAY
			if(recordSound)
			{
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSetHash, soundFieldHash, &initParams, m_Vehicle));
			}
#endif
		}
	}
}

// ----------------------------------------------------------------
// Notify vehicle that it is in a mod shop
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetIsInCarModShop(bool inCarMod) 
{
	if(inCarMod != m_InCarModShop)
	{
		audDisplayf("SET_VEHICLE_IN_CAR_MOD_SHOP - Vehicle %s in mod shop = %s", GetVehicleModelName(), inCarMod? "TRUE" : "FALSE");
		m_InCarModShop = inCarMod;
	}
}

// ----------------------------------------------------------------
// isHornOn
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsHornOn()
{
	return (m_HornSound != NULL || m_PlayerHornOn);
}

// ----------------------------------------------------------------
// Play veh horn
// ----------------------------------------------------------------
void audVehicleAudioEntity::PlayVehicleHorn(f32 holdTimeInSeconds,bool updateTime,bool isScriptTriggered)
{
	// If wrecked or siren or alarm activated, then horn disabled
	// also stop the horn when the car is frozen by an interior, since its super annoying.
	// Please implement icecream truck through modelinfo or vehicle flags ... MI currently not in metadata
	if(m_Vehicle->m_nVehicleFlags.bIsDrowning || !m_IsHornEnabled || IsDisabled() || m_Vehicle->m_nDEflags.bFrozenByInterior || IsHiddenByNetwork())
	{
		return;
	}

	// url:bugstar:6756505 - DLC Vehicle Audio - Toreador - Is it possible to disable the horn under water?
	if (IsSubmarineCar() && m_Vehicle->IsInSubmarineMode())
	{
		return;
	}

	if(m_Vehicle->m_nVehicleFlags.GetIsSirenAudioOn())
	{
		if(m_Vehicle->GetDriver() && m_Vehicle->GetDriver()->IsPlayer())
		{
			m_PlayerHornOn = true;
		}
		if(m_SirenLoop || (!m_SirenLoop && m_SirenState == AUD_SIREN_FAST))
		{
			return;
		}
	}
	if (m_Vehicle->IsAlarmActivated())
	{
		return;
	}


	bool bWasHornOn = IsHornOn();

	CVehicleVariationInstance& variation = m_Vehicle->GetVariationInstance();
	if(!m_Vehicle->GetDriver() || (m_Vehicle->GetDriver() && !m_Vehicle->GetDriver()->IsPlayer()) || (m_InCarModShop && !m_Vehicle->UsesSiren()))
	{
		switch(m_HornType)
		{
		case 0x839504CB: //HeldDown
			PlayHeldDownHorn(holdTimeInSeconds,updateTime);
			break;
		case 0xC91D8B07: //Aggressive
			PlayAggressiveHorn();
			break;
		case 0x4F485502: //Normal
		default:
			// Avoid rest if not need to be computed
			if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AggressiveHorns))
			{
				PlayAggressiveHorn();
			}
			else
			{
				PlayNormalHorn();
			}
			break;
		}
	}
	else
	{
		naAssertf(m_Vehicle->GetDriver()->IsPlayer(),"playing the player horn on a NPC vehicle");
		if(m_HornSound && m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			m_HornSound->StopAndForget();
		}

		audSound** hornSoundPtr = &m_HornSound;

		if (m_Vehicle->UsesSiren() && m_Vehicle->IsLawEnforcementVehicle() /*&& !m_IsSirenFucked*/)
		{
			hornSoundPtr = &m_SirenLoop;
		}

		if(!(*hornSoundPtr))
		{
			audMetadataRef hornLoop = g_NullSoundRef;
			hornLoop = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(GetVehicleHornSoundHash());
			if(m_Vehicle->UsesSiren() && m_Vehicle->IsLawEnforcementVehicle() /*&& !m_IsSirenFucked*/)
			{
				if(m_VehSirenSounds.IsInitialised())
				{
					hornLoop = m_VehSirenSounds.Find(ATSTRINGHASH("HORN", 0x3E34681F));
				}
			}
			if(hornLoop != g_NullSoundRef)
			{
				if(variation.GetMods()[VMT_HORN] != 255)
				{
					if(variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier() != 0)
					{
						hornLoop = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier());
					}
				}
				if(m_OverrideHorn)
				{
					hornLoop = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(m_OverridenHornHash);
				}
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_HORN;
				if(m_Vehicle->ContainsLocalPlayer() && !m_Vehicle->IsAlarmActivated())
				{
					initParams.Volume = g_PlayerHornOffset;
				}

				Vector3 pos;
				pos.Zero();
				GetHornOffsetPos(pos);
				initParams.Position = m_Vehicle->TransformIntoWorldSpace(pos);
				CreateSound_PersistentReference(hornLoop, hornSoundPtr, &initParams);
				if(*hornSoundPtr)
				{
					if(!DoesHornHaveTail())
					{
						(*hornSoundPtr)->SetReleaseTime(60);
					}

					(*hornSoundPtr)->PrepareAndPlay();
					m_PlayerHornOn = true;
				}
			}
		}
	}

	// tell other cars that we're honking
	if (IsHornOn() && !bWasHornOn)
	{
		m_IsScriptTriggeredHorn = isScriptTriggered;
		m_Vehicle->GetIntelligence()->TellOthersAboutHonk();
	}

#if __BANK
	if (IsHornOn())
	{
		audDisplayf("Frame : %i, Horn is on for vehicle %s : 0x%p, m_HornSound 0x%p, m_PlayerHornOn = %s, m_HornType = %u, m_IsHornPermanentlyOn = %s, holdTimeInSeconds = %.2f", 
			fwTimer::GetFrameCount(), m_Vehicle->GetDebugName(), m_Vehicle, m_HornSound, m_PlayerHornOn ? "TRUE" : "FALSE", m_HornType, m_IsHornPermanentlyOn ? "TRUE" : "FALSE", holdTimeInSeconds);
	}
#endif // __BANK

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketVehicleHorn>(
			CPacketVehicleHorn(m_HornType,m_HornSoundIndex, holdTimeInSeconds),
			m_Vehicle);
	}
#endif // GTA_REPLAY
}
// ----------------------------------------------------------------
void audVehicleAudioEntity::PlayNormalHorn()
{
	if(naVerifyf(m_Vehicle, "Trying to play the horn on a NULL vehicle"))
	{	
		if(!m_HornSound)
		{
			audMetadataRef hornLoop = GetHornLoopRef(GetNormalPattern());
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_HORN;

			Vector3 pos;
			pos.Zero();
			GetHornOffsetPos(pos);
			initParams.Position = m_Vehicle->TransformIntoWorldSpace(pos);
			CreateSound_PersistentReference(hornLoop, &m_HornSound, &initParams);
			if(m_HornSound)
			{
				if(!DoesHornHaveTail())
				{
					m_HornSound->SetReleaseTime(60);
				}

				m_HornSound->PrepareAndPlay();
			}
		}
	}
}
// ----------------------------------------------------------------
void audVehicleAudioEntity::PlayAggressiveHorn()
{
	if(naVerifyf(m_Vehicle, "Trying to play the horn on a NULL vehicle"))
	{
		if(!m_HornSound)
		{
			audMetadataRef hornLoop = GetHornLoopRef(GetAggressivePattern());
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_HORN;

			Vector3 pos;
			pos.Zero();
			GetHornOffsetPos(pos);
			initParams.Position = m_Vehicle->TransformIntoWorldSpace(pos);
			CreateSound_PersistentReference(hornLoop, &m_HornSound, &initParams);
			if(m_HornSound)
			{
				if(!DoesHornHaveTail())
				{
					m_HornSound->SetReleaseTime(60);
				}

				m_HornSound->PrepareAndPlay();
			}
		}
	}
}
// ----------------------------------------------------------------
void audVehicleAudioEntity::PlayHeldDownHorn(f32 holdTimeInSeconds,bool updateTime)
{
	//naAssertf(holdTimeInSeconds > 0.f, "Wrong time to play the horn. ");
	if(naVerifyf(m_Vehicle, "Trying to play the horn on a NULL vehicle"))
	{	
		if(!m_HornSound)
		{
			audMetadataRef hornLoop = GetHornLoopRef(GetHeldDownPattern());
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_HORN;

			Vector3 pos;
			pos.Zero();
			GetHornOffsetPos(pos);
			initParams.Position = m_Vehicle->TransformIntoWorldSpace(pos);
			initParams.SetVariableValue(ATSTRINGHASH("HoldTime", 0x96499EA2),holdTimeInSeconds);
			CreateSound_PersistentReference(hornLoop, &m_HornSound, &initParams);
			if(m_HornSound)
			{
				if(!DoesHornHaveTail())
				{
					m_HornSound->SetReleaseTime(60);
				}

				m_HornSound->PrepareAndPlay();
			}
		}
		else if(updateTime)
		{
			naAssertf(holdTimeInSeconds != -1.f, "Trying to play the horn sound forever with the wrong command");
			m_IsHornPermanentlyOn = false;
			m_WasHornPermanentlyOn = false;
			m_HornSound->FindAndSetVariableValue(ATSTRINGHASH("HoldTime", 0x96499EA2),holdTimeInSeconds);
		}
	}
}
// ----------------------------------------------------------------
// audVehicleAudioEntity PostUpdate
// ----------------------------------------------------------------
void audVehicleAudioEntity::PostUpdate()
{
	m_InAirRotationalForceXLastFrame = m_InAirRotationalForceX;
	m_InAirRotationalForceYLastFrame = m_InAirRotationalForceY;

	m_CachedVehicleVelocityLastFrame = m_CachedVehicleVelocity;
	m_CachedVehicleVelocity = m_Vehicle->GetVelocity();
	m_CachedAngularVelocity = m_Vehicle->GetAngVelocity();
	m_CollisionAudio.PostUpdate();
	m_IsBeingNetworkSpectated = false;
	m_IsVehicleBeingStopped = false;
	m_IsPersonalVehicle = m_Vehicle->IsPersonalVehicle();

	if(NetworkInterface::IsInSpectatorMode())
	{
		m_IsBeingNetworkSpectated = (m_Vehicle == CGameWorld::FindFollowPlayerVehicle());
	}

	if (m_TriggerDeferredSubmarineDiveHorn BANK_ONLY(|| (m_Vehicle->InheritsFromSubmarine() && g_ForceTriggerSubmarineDiveHorn)))
	{
		TriggerSubmarineDiveSound(ATSTRINGHASH("dive_horn", 0xD4CA7D22));
		TriggerSubmarineDiveSound(ATSTRINGHASH("dive", 0xE7E43AD4));
		m_TriggerDeferredSubmarineDiveHorn = false;
		BANK_ONLY(g_ForceTriggerSubmarineDiveHorn = false;)
	}

	if (m_TriggerDeferredSubmarineSurface)
	{
		TriggerSubmarineDiveSound(ATSTRINGHASH("surface_horn", 0xF949F920));
		TriggerSubmarineDiveSound(ATSTRINGHASH("surface", 0xF3C674C0));
		m_TriggerDeferredSubmarineSurface = false;
	}
	
	m_IsPlayerPedInFullyOpenSeat = IsPedSatInExternalSeat(FindFollowPed()) || GetVehicleModelNameHash() == ATSTRINGHASH("DUSTER", 0x39D6779E);	

#if NA_RADIO_ENABLED
	
	naAssertf(GetControllerId() != AUD_INVALID_CONTROLLER_ENTITY_ID, "Invalid controller entity id");
	bool wasRadioValid = m_RadioStation != NULL;

	AmbientRadioLeakage leakage = GetAmbientRadioLeakage();
	
	if(m_RadioStation && (m_RadioStation->IsLocked() || m_RadioStation->IsHidden()) && !m_IsPlayerVehicle)		
	{
		// if we are playing an invalid station (ie a station that has since been locked) then dump it
		// Don't do this for the player, since we handle that case explicitly in radio audio entity
		audDisplayf("Stopping locked/hidden station (%s) on non-player vehicle %s", m_RadioStation->GetStationSettings()->Name, GetVehicleModelName());
		g_RadioAudioEntity.StopVehicleRadio(m_Vehicle);
		m_RadioStation = NULL;
		m_LastRadioStation = NULL;
	}
	else if(m_RadioStation && (!IsRadioEnabled() || IsRadioSwitchedOff()))
	{		
		g_RadioAudioEntity.StopVehicleRadio(m_Vehicle);			
		m_RadioStation = NULL;
	}
	else if(IsRadioEnabled() && GetHasNormalRadio() && !IsRadioSwitchedOff())
	{
		bool shouldStopRadio = false;
		if(ShouldRequestRadio(leakage, shouldStopRadio))
		{
			// Only automatically turn on the radio if there is a driver (prevents radio magically coming on when getting
			// in a car as a passenger)
			if((m_RadioStation == NULL && m_Vehicle->GetDriver()) || m_IsPersonalVehicle || (m_Vehicle->IsEngineOn() && (GetVehicleModelNameHash() == ATSTRINGHASH("PBUS2", 0x149BD32A) || GetVehicleModelNameHash() == ATSTRINGHASH("BLIMP3", 0xEDA4ED97))))
			{
				m_RadioStation = g_RadioAudioEntity.RequestVehicleRadio(m_Vehicle);
				if(!HasLoudRadio() && !m_RadioStation)
				{
					// Don't keep requesting
					//SetRadioOffState(true);
				}
			}
		}
		else if(shouldStopRadio)
		{
			if(m_RadioStation)
			{
				g_RadioAudioEntity.StopVehicleRadio(m_Vehicle, false);			
				//m_RadioStation = NULL;
			}
		}
	}


	// Reset flag automatically; script can call every frame they want to force it on
	m_ScriptForcedRadio = false;
	if(m_RadioStation)
	{
		f32 openness = GetOpenness();
		bool isAircraftPassengerSceneEnabled = DYNAMICMIXER.GetActiveSceneFromHash(ATSTRINGHASH("INTERIOR_VIEW_AIRCRAFT_PASSENGER_SCENE", 0x7EBA8171)) != NULL;

		if(m_IsPlayerVehicle && (g_PositionedPlayerVehicleRadioEnabled || isAircraftPassengerSceneEnabled))
		{						
			leakage = GetPositionalPlayerRadioLeakage();

			// If aircraft passenger scene is enabled then we're inside and want this as loud as possible
			if(isAircraftPassengerSceneEnabled)
			{
				openness = 1.0f;
			}
		}

		// If the trunk is upgraded with a speaker mod, then base the radio openness on the openness of the trunk
		if(m_Vehicle->GetVariationInstance().GetModIndex(VMT_TRUNK) != INVALID_MOD && m_Vehicle->GetVariationInstance().GetAudioApply(VMT_TRUNK) > 0.f)
		{
			CCarDoor* trunkDoor = m_Vehicle->GetDoorFromId(VEH_BOOT);

			if(trunkDoor)
			{
				if(trunkDoor->GetIsIntact(m_Vehicle))
				{
					openness = trunkDoor->GetDoorRatio();
				}
				else
				{
					openness = 1.f;
				}
			}
		}

		// Party bus has external speakers so is always at full openness
		if (GetVehicleModelNameHash() == ATSTRINGHASH("PBUS2", 0x149BD32A) || GetVehicleModelNameHash() == ATSTRINGHASH("BLIMP3", 0xEDA4ED97) || (GetVehicleModelNameHash() == ATSTRINGHASH("CLUB", 0x82E47E85) && IsRadioUpgraded()))
		{
			openness = 1.f;
		}

		if(leakage == LEAKAGE_MODDED || leakage == LEAKAGE_PARTYBUS)
		{
			openness = Max(openness, g_ModdedRadioVehicleOpenness);
		}

#if RSG_BANK
		if(m_Vehicle && g_OverrideFocusVehicleLeakage && m_Vehicle == CDebugScene::FocusEntities_Get(0))
		{
			leakage = g_OverriddenVehicleRadioLeakage;
		}		
#endif // RSG_BANK

		//Add a volume offset and modify the source filter cutoff frequency based upon how 'open' this
		//positioned emitter is.
		f32 opennessVolumeOffsetLinear = g_RadioOpennessVols[leakage][0] + (openness * (g_RadioOpennessVols[leakage][1] - g_RadioOpennessVols[leakage][0]));		
		float lpfLinear = Lerp(openness, g_RadioLPFLinear[leakage][0], g_RadioLPFLinear[leakage][1]);
		m_RadioLPFCutoff = static_cast<u32>(Clamp<f32>(audDriverUtil::ComputeHzFrequencyFromLinear(lpfLinear), kVoiceFilterLPFMinCutoff, kVoiceFilterLPFMaxCutoff));
		m_RadioRolloffFactor = Lerp(openness, g_RadioRolloffs[leakage][0], g_RadioRolloffs[leakage][1]);
		m_RadioHPFCutoff = g_RadioHPFCutoffs[leakage];

#if __BANK
		if(g_OverrideFocusVehicleLeakageParams && m_Vehicle == CDebugScene::FocusEntities_Get(0))
		{
			opennessVolumeOffsetLinear = g_OverridenLeakageVolMin + (openness * (g_OverridenLeakageVolMax - g_OverridenLeakageVolMin));
			lpfLinear = Lerp(openness, g_OverridenLeakageLpfLinearMin, g_OverridenLeakageLpfLinearMax);			
			m_RadioRolloffFactor = Lerp(openness, g_OverridenLeakageRolloffMin, g_OverridenLeakageRolloffMax);
			m_RadioHPFCutoff = g_OverridenLeakageHPFCutoff;
		}
#endif

		m_RadioLPFCutoff = static_cast<u32>(Clamp<f32>(audDriverUtil::ComputeHzFrequencyFromLinear(lpfLinear), kVoiceFilterLPFMinCutoff, kVoiceFilterLPFMaxCutoff));
		m_RadioOpennessVol = audDriverUtil::ComputeDbVolumeFromLinear(opennessVolumeOffsetLinear);

		// Keep track of the last valid radio station
		m_LastRadioStation = m_RadioStation;


#if __BANK
		if(g_DebugDrawFocusVehicleRadioOcclusion && m_Vehicle)
		{
			char tempString[512];
			formatf(tempString, "Openness: %.02f", openness);
			safecatf(tempString, "\nLeakage: %s", g_OverrideFocusVehicleLeakageParams? "CUSTOM" : AmbientRadioLeakage_ToString((AmbientRadioLeakage)leakage));
			safecatf(tempString, "\nVolume: %.02f (Min %.02f, Max %.02f)", opennessVolumeOffsetLinear, g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageVolMin : g_RadioOpennessVols[leakage][0], g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageVolMax : g_RadioOpennessVols[leakage][1]);
			safecatf(tempString, "\nLpf Cutoff Linear: %.02f (Min %.02f, Max %.02f)", lpfLinear, g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageLpfLinearMin : g_RadioLPFLinear[leakage][0], g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageLpfLinearMax : g_RadioLPFLinear[leakage][1]);
			safecatf(tempString, "\nHpf Cutoff: %u", g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageHPFCutoff : g_RadioHPFCutoffs[leakage]);
			safecatf(tempString, "\nRoll Off: %.02f (Min %.02f, Max %.02f)", m_RadioRolloffFactor, g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageRolloffMin : g_RadioRolloffs[leakage][0], g_OverrideFocusVehicleLeakageParams? g_OverridenLeakageRolloffMax : g_RadioRolloffs[leakage][1]);
			safecatf(tempString, "\nMax Radius: %.02f (Current distance %.02f)", Sqrtf(g_OverrideFocusVehicleLeakageParams? (g_OverridenLeakageMaxRadius * g_OverridenLeakageMaxRadius) : g_MaxRadiusForAmbientRadio2[leakage]), (VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag());			
			grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition(), CDebugScene::FocusEntities_Get(0) == m_Vehicle? Color32(255,128,128) : Color32(255,255,255), tempString);
		}
#endif

	}
	else if(wasRadioValid && m_DrowningRadioSound)
	{
		m_DrowningRadioSound->StopAndForget();
	}

#endif // NA_RADIO_ENABLED

	if(m_RoadNoiseRidged)
	{
		f32 directionMatch = 1.0f;
		f32 previousNodeMatch = 1.0f;
		f32 nextNodeMatch = 1.0f;

		const Vector3 vVehPosition = VEC3V_TO_VECTOR3(m_Vehicle->GetVehiclePosition());
		const Vector3 vVehForwardDir = VEC3V_TO_VECTOR3(m_Vehicle->GetVehicleForwardDirection());

		// If we previously had a valid node but have now moved some distance away, clear it. Searching for nodes is expensive so we want to avoid
		// doing it too frequently
		if(!m_NearestNodeAddress.IsEmpty())
		{
			if(m_NearestNodeAddressPos.Dist2(vVehPosition) > 20.0f * 20.0f)
			{
				m_NearestNodeAddress.SetEmpty();
			}
		}

		// If we don't have a valid node, request one
		if(m_NearestNodeAddress.IsEmpty())
		{
			m_NearestNodeAddress = ThePaths.FindNodeClosestToCoors(vVehPosition, 40.0f);
			m_NearestNodeAddressPos = vVehPosition;
		}

		// By now we should have a node, so calculate how closely we match its direction
		if(!m_NearestNodeAddress.IsEmpty())
		{
			CPathNode* pNode = ThePaths.FindNodePointerSafe(m_NearestNodeAddress);

			if(pNode)
			{
				directionMatch = ThePaths.CalcDirectionMatchValue(pNode, vVehForwardDir.x, vVehForwardDir.y, 1.0f);

				// Occasionally we might get road nodes with very weird orientations compared to the ones infront and behind
				// (eg. at a T junction), so just averaging the match value to make sure that this doesn't totally kill the 
				// tyre bumps
				if(pNode->NumLinks() == 2)
				{
					CPathNodeLink& nextLink = ThePaths.GetNodesLink(pNode, 0);
					CPathNodeLink& prevLink = ThePaths.GetNodesLink(pNode, 1);

					CPathNode* nextNode = ThePaths.FindNodePointerSafe(nextLink.m_OtherNode);
					CPathNode* prevNode = ThePaths.FindNodePointerSafe(prevLink.m_OtherNode);

					if(nextNode)
					{
						previousNodeMatch = ThePaths.CalcDirectionMatchValue(nextNode, vVehForwardDir.x, vVehForwardDir.y, 1.0f);
					}

					if(prevNode)
					{
						nextNodeMatch = ThePaths.CalcDirectionMatchValue(prevNode, vVehForwardDir.x, vVehForwardDir.y, 1.0f);
					}
				}

				directionMatch += previousNodeMatch;
				directionMatch += nextNodeMatch;
				directionMatch *= 0.33f;
			}
			else
			{
				m_NearestNodeAddress.SetEmpty();
			}
		}

		m_RidgedSurfaceDirectionMatch = directionMatch;
		
#if __BANK
		if(m_IsPlayerVehicle && g_DisplayRidgedSurfaceInfo)
		{
			char tempString[128];
			sprintf(tempString, "Direction Match: %.02f", directionMatch);
			grcDebugDraw::Text(m_NearestNodeAddressPos, Color32(255,255,255), tempString);
			grcDebugDraw::Sphere(m_NearestNodeAddressPos, 2.f, Color32(0,0,255,128));	
			grcDebugDraw::Line(m_NearestNodeAddressPos, VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()), Color32(255, 255, 255, 255));
		}
#endif
	}
	else
	{
		m_RidgedSurfaceDirectionMatch = 0.0f;
	}

	m_VehRainInWaterVol = 1.f;

	if(m_ForcedGameObjectResetRequired)
	{
		m_PreserveEnvironmentGroupOnShutdown = true;
		Shutdown();
		m_PreserveEnvironmentGroupOnShutdown = false;
		m_IsWaitingToInit = true;
		Init(m_Vehicle);
		m_ForcedGameObjectResetRequired = false;
	}

	if(m_Vehicle->InheritsFromSubmarineCar())
	{
		((CSubmarineCar*)m_Vehicle)->CheckForAudioModeSwitch(m_IsFocusVehicle BANK_ONLY(, g_ForceAmphibiousBoatMode));
	}

	if(m_Vehicle->InheritsFromAmphibiousAutomobile())
	{
		((CAmphibiousAutomobile*)m_Vehicle)->CheckForAudioModeSwitch(m_IsFocusVehicle BANK_ONLY(, g_ForceAmphibiousBoatMode));
	}
}

bool audVehicleAudioEntity::ShouldRequestRadio(const AmbientRadioLeakage leakage, bool &shouldStopRadio) const
{
	const f32 dist2 = (VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
	bool isLocalPlayerInVehicle = m_Vehicle && m_Vehicle->ContainsLocalPlayer();

	bool followPlayerInVehicle = false;
	if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnSCTVTeam())
	{
		CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
		if (pFollowPlayer && pFollowPlayer->GetMyVehicle())
			followPlayerInVehicle = true;
	}

	f32 maxDistance = g_MaxRadiusForAmbientRadio2[leakage];

#if RSG_BANK
	if (m_Vehicle && g_OverrideFocusVehicleLeakageParams && m_Vehicle == CDebugScene::FocusEntities_Get(0))
	{
		maxDistance = (g_OverridenLeakageMaxRadius * g_OverridenLeakageMaxRadius);
	}
#endif

	shouldStopRadio = !followPlayerInVehicle && (dist2 > maxDistance + 100);
	// Script forced radio means play even with no driver and regardless of distance
	return (m_ScriptForcedRadio || (isLocalPlayerInVehicle || ((m_Vehicle->GetDriver() != NULL || m_IsPersonalVehicle || (m_Vehicle->IsEngineOn() && (GetVehicleModelNameHash() == ATSTRINGHASH("PBUS2", 0x149BD32A) || GetVehicleModelNameHash() == ATSTRINGHASH("BLIMP3", 0xEDA4ED97)))) && dist2 < maxDistance)));
}

AmbientRadioLeakage audVehicleAudioEntity::GetAmbientRadioLeakage() const
{ 
	if (IsRadioUpgraded())
	{
		if (GetVehicleModelNameHash() == ATSTRINGHASH("PBUS2", 0x149BD32A) || GetVehicleModelNameHash() == ATSTRINGHASH("BLIMP3", 0xEDA4ED97))
		{
			return LEAKAGE_PARTYBUS;
		}
		else
		{
			return LEAKAGE_MODDED;
		}
	}
	else
	{
		return m_HasLoudRadio || m_Vehicle->m_nVehicleFlags.bSlowChillinDriver ? LEAKAGE_CRAZY_LOUD : m_RadioLeakage;
	}	
}

// -------------------------------------------------------------------------------
// Update any submersible sounds
// -------------------------------------------------------------------------------
void audVehicleAudioEntity::UpdateSubmersible(audVehicleVariables& vehicleVariables, BoatAudioSettings* boatSettings)
{
	f32 subDiveVelocity = 0.0f;
	CSubmarineHandling* subHandling = m_Vehicle->GetSubHandling();

	if(m_IsFocusVehicle)
	{
		if(subHandling && subHandling->GetDiveControl() != 0.0f && m_Vehicle->IsEngineOn())
		{
			if(SameSign(subHandling->GetDiveControl(), m_CachedVehicleVelocity.GetZ()))
			{
				subDiveVelocity = abs(m_CachedVehicleVelocity.GetZ() * 0.5f);
			}
		}
	}

	if (subHandling && GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
	{
		const f32 submergeLevel = m_Vehicle->m_Buoyancy.GetSubmergedLevel();					
		const f32 submarineDiveControl = subHandling->GetDiveControl();

		if (submarineDiveControl == -1.f)
		{
			m_SubmarineDiveHoldTime += fwTimer::GetTimeStep();
		}
		else
		{
			m_SubmarineDiveHoldTime = 0.f;
		}

		if (submergeLevel < 0.9f)
		{
			if (!m_HasTriggeredSubmarineSurface)
			{
				m_TriggerDeferredSubmarineSurface = true;
				m_HasTriggeredSubmarineSurface = true;
			}
		}
		else if (submarineDiveControl < 0.f || m_EngineWaterDepth < -10.0f)
		{
			m_HasTriggeredSubmarineSurface = false;
		}

		if (m_SubmarineDiveHoldTime > g_MinSubDiveHoldTime && 
			submergeLevel >= g_SubDiveHornSubmergeThreshold && 
			subDiveVelocity >= g_SubDiveHornVelocity && 
			Sign(m_CachedVehicleVelocity.GetZ()) < 0)
		{
			if (!m_HasTriggeredSubmarineDiveHorn)
			{
				m_TriggerDeferredSubmarineDiveHorn = true;
				m_HasTriggeredSubmarineDiveHorn = true;
			}
		}
		else
		{
			f32 diveHornRetriggerThreshold = m_Vehicle->GetDriver() ? g_SubDiveHornSubmergeRetriggerThreshold : g_SubDiveHornSubmergeRetriggerThresholdNoDriver;

			if (submarineDiveControl >= 0.f && submergeLevel < diveHornRetriggerThreshold)
			{
				m_HasTriggeredSubmarineDiveHorn = false;
			}
		}

#if __BANK
		if (g_DebugSubmarineDiveHorn)
		{
			char tempString[128];
			f32 xCoord = 0.1f;
			f32 yCoord = 0.1f;
			
			formatf(tempString, "Has Triggered Dive: %s", m_HasTriggeredSubmarineDiveHorn ? "true" : "false");
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Has Triggered Surface: %s", m_HasTriggeredSubmarineSurface ? "true" : "false");
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += 0.02f;			

			formatf(tempString, "Dive Hold Time: %.02f", m_SubmarineDiveHoldTime);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Submerge Level: %.02f", submergeLevel);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += 0.02f;

			formatf(tempString, "Dive Velocity: %.02f", subDiveVelocity);
			grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255, 255, 255), tempString);
			yCoord += 0.02f;
		}
#endif
	}	

	f32 smoothedDiveVelocity = m_SubDiveVelocitySmoother.CalculateValue(subDiveVelocity);
	f32 volumeLin = m_SubAngularVelocityVolumeSmoother.CalculateValue(m_SubTurningToSweetenerVolume.CalculateValue(Max(m_SubAngularVelocityMag, smoothedDiveVelocity)) * m_SubRevsToSweetenerVolume.CalculateValue(Max(smoothedDiveVelocity, vehicleVariables.rawRevs)));
	s32 pitch = static_cast<s32>(m_SubTurningToSweetenerPitch.CalculateValue(m_SubAngularVelocityMag));
	UpdateLoopWithLinearVolumeAndPitch(&m_SubTurningSweetener, boatSettings->SubTurningSweetenerSound, volumeLin, pitch, m_EnvironmentGroup, NULL, true, false);

	const bool isKosatka = GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70);

	// Sub extras sound lives in the granular bank, so we need to make sure it is stopped once the waveslot is released or else the bank can't be unloaded
	bool subExtrasValid = (m_IsPlayerVehicle || GetVehicleModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70)) && (m_Vehicle->GetIsInWater() REPLAY_ONLY(|| m_ReplayVehiclePhysicsInWater)) && m_Vehicle->m_nVehicleFlags.bEngineOn && m_EngineWaveSlot;

	if(subExtrasValid)
	{
		if(!m_SubExtrasSound)
		{
			// Sub extras sound just plays while the engine is on - control is all via PTS. Purposefully not updating entity as we want this that same
			// regardless of above/below water status
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();

			if (!isKosatka)
			{
				initParams.Tracker = m_Vehicle->GetPlaceableTracker();
			}
			
			CreateAndPlaySound_Persistent(boatSettings->SubExtrasSound, &m_SubExtrasSound, &initParams);
		}

		if (isKosatka && m_SubExtrasSound)
		{
			m_SubExtrasSound->SetRequestedPosition(ComputeClosestPositionOnVehicleYAxis());
		}
	}
	else if(m_SubExtrasSound)
	{
		m_SubExtrasSound->StopAndForget();
	}

	// Sub creaks live in the sfx bank, so we need to make sure it is stopped once the waveslot is released or else the bank can't be unloaded
	bool subCreakValid = m_IsPlayerVehicle && (m_Vehicle->GetIsInWater() REPLAY_ONLY(|| m_ReplayVehiclePhysicsInWater)) && Water::IsCameraUnderwater() && m_SFXWaveSlot;

	if(!m_SubmersibleCreakSound)
	{
		if(subCreakValid)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
			initParams.Tracker = m_Vehicle->GetPlaceableTracker();
			CreateAndPlaySound_Persistent(boatSettings->SubmersibleCreaksSound, &m_SubmersibleCreakSound, &initParams);
		}
	}
	else if(!subCreakValid)
	{
		m_SubmersibleCreakSound->StopAndForget();
	}

	if(m_WaterDiveSound)
	{
		m_WaterDiveSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(m_DrowningFactor));
	}

	bool isSubmarineCar = false;
	u32 submarineSoundSetHash = GetSubmarineCarSoundset();
	
	if (submarineSoundSetHash != 0u)
	{
		isSubmarineCar = true;
	}
	else if (isKosatka)
	{
		submarineSoundSetHash = ATSTRINGHASH("DLC_Hei4_Kosatka_Sounds", 0xBDDCF962);	
	}

	if(submarineSoundSetHash != 0u)
	{
		f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
		f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
		f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;
		f32 angularVelocity = GetCachedAngularVelocity().Mag();

		if(m_SubExtrasSound)
		{			
			m_SubExtrasSound->FindAndSetVariableValue(ATSTRINGHASH("throttleInput", 0x918028C4), m_Vehicle->GetThrottle());
			m_SubExtrasSound->FindAndSetVariableValue(ATSTRINGHASH("rollAngle", 0x94FC4D71), rollAngle);
			m_SubExtrasSound->FindAndSetVariableValue(ATSTRINGHASH("pitchAngle", 0x809A226D), pitchAngle);
			m_SubExtrasSound->FindAndSetVariableValue(ATSTRINGHASH("yawAngle", 0x15DE02B1), yawAngle);
			m_SubExtrasSound->FindAndSetVariableValue(ATSTRINGHASH("angularVelocity", 0xC90EACC2), angularVelocity);
		}

		if(m_SubTurningSweetener)
		{
			m_SubTurningSweetener->FindAndSetVariableValue(ATSTRINGHASH("throttleInput", 0x918028C4), m_Vehicle->GetThrottle());
			m_SubTurningSweetener->FindAndSetVariableValue(ATSTRINGHASH("rollAngle", 0x94FC4D71), rollAngle);
			m_SubTurningSweetener->FindAndSetVariableValue(ATSTRINGHASH("pitchAngle", 0x809A226D), pitchAngle);
			m_SubTurningSweetener->FindAndSetVariableValue(ATSTRINGHASH("yawAngle", 0x15DE02B1), yawAngle);
			m_SubTurningSweetener->FindAndSetVariableValue(ATSTRINGHASH("angularVelocity", 0xC90EACC2), angularVelocity);
		}

		f32 maxPropSpeed = 0.f;

		CSubmarineHandling* subHandling = nullptr;

		if (isSubmarineCar)
		{
			CSubmarineCar* submarineCar = (CSubmarineCar*)m_Vehicle;
			subHandling = submarineCar->GetSubHandling();
		}
		else
		{
			CSubmarine* submarine = (CSubmarine*)m_Vehicle;
			subHandling = submarine->GetSubHandling();
		}		

		if(subHandling)
		{			
			for(u32 i = 0; i < subHandling->GetNumPropellors(); i++)
			{
				maxPropSpeed = Max(maxPropSpeed, Abs(subHandling->GetPropellorSpeed(i)));
			}
		}
				
		if(maxPropSpeed > 0.f || (isKosatka && m_Vehicle->GetHealth() > 0.f))
		{
			if(!m_SubPropSound)
			{
				audSoundSet submarineSoundset;
				submarineSoundset.Init(submarineSoundSetHash);

				if(submarineSoundset.IsInitialised())
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();

					// We want to do some custom underwater filtering on the Kosatka engine sound, so don't update it as normal
					if (!isKosatka)
					{
						initParams.UpdateEntity = true;
						initParams.u32ClientVar = isSubmarineCar ? AUD_VEHICLE_SOUND_SUBMARINECAR_PROPELLOR : AUD_VEHICLE_SOUND_SUBMARINE_PROPELLOR;
					}
					
					CreateAndPlaySound_Persistent(submarineSoundset.Find(ATSTRINGHASH("rotor_loop", 0x3E09EE0)), &m_SubPropSound, &initParams);
				}				
			}

			if(m_SubPropSound)
			{
				if (isKosatka)
				{
					m_SubPropSound->SetRequestedPosition(ComputeClosestPositionOnVehicleYAxis());
					m_SubPropSound->FindAndSetVariableValue(ATSTRINGHASH("enginewaterdepth", 0x620614B8), Max(0.f, m_EngineWaterDepth * -1.f));
				}

				m_SubPropSound->FindAndSetVariableValue(ATSTRINGHASH("rotorspeed", 0x4F4CEFD4), maxPropSpeed);
			}
		}
		else if(m_SubPropSound)
		{
			m_SubPropSound->StopAndForget(true);
		}

		if(m_SubExtrasSound)
		{
			m_SubExtrasSound->FindAndSetVariableValue(ATSTRINGHASH("rotorspeed", 0x4F4CEFD4), maxPropSpeed);
		}
	}	
}

// ----------------------------------------------------------------
// Trigger a dive sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerWaterDiveSound()
{
	if(IsSubmersible() && !m_WaterDiveSound)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
		initParams.TrackEntityPosition = true;
		CreateAndPlaySound_Persistent(ATSTRINGHASH("SUBMERSIBLE_WATER_ENTER_MASTER", 0xC9598E64), &m_WaterDiveSound, &initParams);
	}
}

// ----------------------------------------------------------------
// Trigger the submarine crush sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSubmarineCrushSound()
{
	// Kosatka has bespoke script triggered crush sounds
	if(IsSubmersible() && GetVehicleModelNameHash() != ATSTRINGHASH("Kosatka", 0x4FAF0D70))
	{	
		if(sysThreadType::IsUpdateThread())
		{
			CreateDeferredSound(ATSTRINGHASH("SUB_CREAKS_ONESHOT_MASTER", 0xD67211AE), m_Vehicle, NULL, true, true);
		}
		else
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(ATSTRINGHASH("SUB_CREAKS_ONESHOT_MASTER", 0xD67211AE), &initParams);
		}
	}
}

// ----------------------------------------------------------------
// Trigger the implode warning sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSubmarineImplodeWarning(bool fullyImplode)
{
	// Kosatka has bespoke script triggered crush sounds
	if(IsSubmersible() && GetVehicleModelNameHash() != ATSTRINGHASH("Kosatka", 0x4FAF0D70))
	{
		u32 soundHash = fullyImplode? ATSTRINGHASH("SUB_PRESSURE_DESTROY_MASTER", 0xFF3C6194) : ATSTRINGHASH("SUB_PRESSURE_WARNING_MASTER", 0xE77825EA);

		if(sysThreadType::IsUpdateThread())
		{
			CreateDeferredSound(soundHash, m_Vehicle, NULL, true, true);
		}
		else
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(soundHash, &initParams);
		}
	}
}

// ----------------------------------------------------------------
// audVehicleAudioEntity ComputeOutsideWorldOcclusion
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::ComputeOutsideWorldOcclusion()
{
	f32 occlusion = 0.f;
	// bikes and boats are open
	if((m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR && m_Vehicle->CarHasRoof()) || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_HELI || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AUTOGYRO)
	{
		// factor all doors equally
		f32 doorOpenRatio = 0.f;
		for(u32 i=0; i<g_MaxDoors; i++)
		{
			CCarDoor* pDoor = m_Vehicle->GetDoorFromId(g_DoorHierarchyIds[i]);
			f32 openRatio = 1.0f;

			if(pDoor)
			{
				if(pDoor->GetIsIntact(NULL))
				{
					openRatio = pDoor->GetDoorRatio();
				}
			}
			else
			{
				// this door doesn't exist; it's not open
				openRatio = 0.f;
			}

			doorOpenRatio += openRatio * g_NumDoorsScalar;
		}
		// windows are weighted
		f32 windowOpenRatio = 0.f;
		for(u32 i = 0; i < NumWindowHierarchyIds; i++)
		{
			if(m_Vehicle->HasWindow(audVehicleAudioEntity::sm_WindowHierarchyIds[i]) && !m_HasWindowToSmashCache.IsSet(i))
			{
				windowOpenRatio += g_WindowOpenContribs[i];
			}
		}

		// overall occlusion - if half of the doors are fully open then so is the car
		occlusion = Min(1.f, 1.f - (doorOpenRatio*2.f + windowOpenRatio));
	}

	return occlusion;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity GetInteriorSoundOcclusion
// ----------------------------------------------------------------
void audVehicleAudioEntity::GetInteriorSoundOcclusion(f32 &volAtten, u32 &cutoffFreq)
{
	f32 vehicleOpenness = GetOpenness();
	f32 opennessVolumeOffsetLinear = sm_InteriorOpenToVolumeCurve.CalculateValue(vehicleOpenness);
	volAtten = audDriverUtil::ComputeDbVolumeFromLinear(opennessVolumeOffsetLinear);
	cutoffFreq = (u32)sm_InteriorOpenToCutoffCurve.CalculateValue(vehicleOpenness);
}

// ----------------------------------------------------------------
// Check if the ped is sat in an external seat
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsPedSatInExternalSeat(CPed* ped) const
{
	s32 numEnclosedSeats = -1;

	// GTAV specific fix
	const u32 modelNameHash = GetVehicleModelNameHash();
	switch(modelNameHash)
	{
	case 0xCEEA3F4B: // "BARRACKS"
		numEnclosedSeats = 2;
		break;
	case 0xB6410173: // "DUBSTA3"
		numEnclosedSeats = 4;
		break;
	case 0x9114EADA: // "INSURGENT"
	case 0x8D4B7A8A: // "INSURGENT3"
		numEnclosedSeats = 4;
		break;
	case 0x83051506: // "TECHNICAL"
	case 0x50D4D19F: // "TECHNICAL3"
		numEnclosedSeats = 2;
		break;
	case 0xF34DFB25: // "BARRAGE"
		numEnclosedSeats = 2;
		break;
	default:
		break;
	}	
	// End fix

	// If the player is sat in a vehicle with enclosed front cab but an open rear trailer w/seats, 
	// we should to ignore any closed windows/doors when sat in the rear. We also want to ignore them
	// if the player is just hanging onto the vehicle rather than being sat in it (eg. firetrucks and the like)
	if(ped && m_Vehicle->ContainsPed(ped))
	{		
		s32 pedSeatIndex = m_Vehicle->GetSeatManager()->GetPedsSeatIndex(ped);

		if(m_Vehicle->IsSeatIndexValid(pedSeatIndex))
		{
			if(numEnclosedSeats >= 0 && pedSeatIndex >= numEnclosedSeats)
			{
				return true;
			}
			else
			{
				const CVehicleSeatAnimInfo* seatAnimInfo = m_Vehicle->GetSeatAnimationInfo(pedSeatIndex);

				if(seatAnimInfo && seatAnimInfo->GetKeepCollisionOnWhenInVehicle())
				{
					return true;
				}
			}			
		}
	}

	return false;
}

// ----------------------------------------------------------------
// audVehicleAudioEntity GetOpenness
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::GetOpenness(bool cameraRelative)
{
	f32 openness = 1.0f;

#if RSG_BANK
	if(m_Vehicle && m_Vehicle == CDebugScene::FocusEntities_Get(0) && g_OverrideFocusVehicleOpenness)
	{
		return g_OverriddenVehicleOpenness;
	}
#endif // RSG_BANK

	// Save having to recalculate openness if we've done it already this frame
	if(!cameraRelative && m_CachedNonCameraRelativeOpenness >= 0.0f)
	{
		return m_CachedNonCameraRelativeOpenness;
	}
	else if(cameraRelative && m_CachedCameraRelativeOpenness >= 0.0f)
	{
		return m_CachedCameraRelativeOpenness;
	}

	const bool hasRoof = (m_Vehicle->GetVehicleType() == VEHICLE_TYPE_CAR && m_Vehicle->CarHasRoof()) 
							|| m_Vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE
							|| m_Vehicle->GetVehicleType() == VEHICLE_TYPE_DRAFT
							|| m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR
							|| (m_Vehicle->GetVehicleType() == VEHICLE_TYPE_HELI && GetVehicleModelNameHash() != ATSTRINGHASH("Thruster", 0x58CDAF30))
							|| m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BLIMP
							|| m_Vehicle->GetVehicleType() == VEHICLE_TYPE_AUTOGYRO
							|| m_Vehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN;
	
	if(m_Vehicle && hasRoof && m_Vehicle->GetSkeleton() && !m_IsPlayerPedInFullyOpenSeat && m_Vehicle->GetFragInst())
	{
		openness = 0.0f;

		CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)m_Vehicle->GetBaseModelInfo();

		for(u8 i=0; i<g_MaxDoors; i++)
		{					
			CCarDoor* pDoor = m_Vehicle->GetDoorFromId(g_DoorHierarchyIds[i]);
			f32 openAngle = 90.0f;

			if(pDoor && pDoor->GetIsIntact(NULL))
			{
				openAngle = g_MaxDoorOpenAngle * pDoor->GetDoorRatio();
			}

			if(openAngle > 0.0f)
			{
				//Generate a vector starting from the centre of the door cavity and pointing outwards along the angle
				//of the open door.


				s32 doorBoneIndex = modelInfo->GetBoneIndex(g_DoorHierarchyIds[i]);

				//Assert(doorBoneIndex > -1);
				//Assert(m_Vehicle->GetSkeleton());

				if((doorBoneIndex > -1))
				{
					if(cameraRelative)
					{
						Matrix34 doorMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(doorBoneIndex));

						//Get the centre of bounding box of (closed) door component.
						Vector3 halfWidthVector, centreVector;
						s32 doorComponent = m_Vehicle->GetFragInst()->GetComponentFromBoneIndex(doorBoneIndex);

						if(doorComponent != -1 && m_Vehicle->GetFragInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)
						{
							phBound *doorBound = ((phBoundComposite *)m_Vehicle->GetFragInst()->GetArchetype()->GetBound())->
								GetBound(doorComponent);
							if(doorBound)
							{
								doorBound->GetBoundingBoxHalfWidthAndCenter(RC_VEC3V(halfWidthVector), RC_VEC3V(centreVector));
							}
							else
							{
								// When the door breaks off the bound is no longer available.
								// I don't think we need the centre of the door cavity in this case, as its fully open?
								centreVector.Zero();
							}

							doorMatrix.d += centreVector;
							Vector3 doorBackVector = -doorMatrix.b;
							Vector3 doorOutVector = doorMatrix.a * g_DoorOutSign[i];

							Matrix34 doorMatrixWorld = doorMatrix;
							doorMatrixWorld.Dot(MAT34V_TO_MATRIX34(m_Vehicle->GetMatrix()));

							//Use the full open angle, as this results in a better inner cone than bisecting this angle.
							f32 openAngleRadians = openAngle * DtoR;
							Vector3 doorOpenVector = (doorBackVector * rage::Cosf(openAngleRadians)) +
								(doorOutVector * rage::Sinf(openAngleRadians));
							Vector3 doorOpenVectorWorld = doorOpenVector;
							doorMatrixWorld.Transform(doorOpenVectorWorld);


							audVolumeCone openDoorVolumeCone;
							openDoorVolumeCone.Init(ATSTRINGHASH("LINEAR_RISE", 0xD0E6F19), doorOpenVector, 1.0f, openAngle *
								sm_DoorConeInnerAngleScaling, sm_DoorConeOuterAngle);


							const f32 doorOpenness = 1.0f - openDoorVolumeCone.ComputeAttenuation(doorMatrixWorld);
							openness += doorOpenness;
#if __BANK
							if(g_DebugVehicleOpenness)
							{
								grcDebugDraw::Line(doorMatrixWorld.d, doorOpenVectorWorld, Color32(0, 255, 0, 255));
								char buf[64];
								formatf(buf, sizeof(buf)-1,"%.2f", doorOpenness);
								grcDebugDraw::Text(doorMatrixWorld.d,Color32(0,255,0), buf);
							}
#endif

						}
					}
					else
					{
						openness += 0.2f;
					}
				}
			}
		}

		// windows - only for door-attached windows at the moment
		for(u32 i = 0; i < NumWindowHierarchyIds; i++)
		{
			const s32 windowBoneIndex = modelInfo->GetBoneIndex(audVehicleAudioEntity::sm_WindowHierarchyIds[i]);
			if(windowBoneIndex > -1)
			{
				if(m_Vehicle->HasWindow(audVehicleAudioEntity::sm_WindowHierarchyIds[i]) && !m_HasWindowToSmashCache.IsSet(i))
				{					
					if(cameraRelative)
					{
						// first 4 windows are attached to doors; need to dot with door mat
						Matrix34 localWindowMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(windowBoneIndex));
						Vector3 rightVector;
						rightVector = localWindowMatrix.b;
						if(naVerifyf(i < g_MaxDoors,"Wrong window idx"))
						{
							eHierarchyId doorId = g_DoorHierarchyIds[i];
							if(doorId != VEH_INVALID_ID)
							{
								s32 doorBoneIndex = modelInfo->GetBoneIndex(doorId);
								if(doorBoneIndex > 0)
								{
									Matrix34 doorMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(doorBoneIndex));
									localWindowMatrix.Dot(doorMatrix);
									rightVector = localWindowMatrix.a;
								}
								else if (audVehicleAudioEntity::sm_WindowHierarchyIds[i] >=VEH_WINDOW_LF)
								{
									rightVector = localWindowMatrix.a;
								}
							}
							else if (audVehicleAudioEntity::sm_WindowHierarchyIds[i] >=VEH_WINDOW_LF)
							{
								rightVector = localWindowMatrix.a;
							}
							
						}

						Matrix34 worldWindowMatrix = localWindowMatrix;

						worldWindowMatrix.Dot(MAT34V_TO_MATRIX34(m_Vehicle->GetMatrix()));

						Vector3 windowOpenVector = rightVector * g_WindowOutSign[i];	

						audVolumeCone openWindowVolumeCone;
						openWindowVolumeCone.Init(ATSTRINGHASH("LINEAR_RISE", 0xD0E6F19), windowOpenVector, 1.0f, sm_DoorConeInnerAngleScaling * 90.f, sm_DoorConeOuterAngle);
						const f32 windowOpenness = (1.f - openWindowVolumeCone.ComputeAttenuation(worldWindowMatrix)) * g_WindowOpenContribs[i];
						openness += windowOpenness;
#if __BANK
						if(g_DebugVehicleOpenness)
						{

							Vector3 windowOpenVectorWorld  = windowOpenVector;
							worldWindowMatrix.Transform(windowOpenVectorWorld);

							grcDebugDraw::Line(worldWindowMatrix.d, windowOpenVectorWorld, Color32(0, 0, 255),
								Color32(0, 255, 0, 255));
							char buf[64];
							formatf(buf, sizeof(buf)-1,"%.2f", windowOpenness);
							grcDebugDraw::Text(worldWindowMatrix.d,Color32(0,0,255), buf);
						}
#endif
					}
					else
					{
						// scale non-directional openness by 50%
						openness += g_WindowOpenContribs[i] * 0.5f;
					}

				}
			}
		}

		openness = Max(GetConvertibleRoofOpenness(), openness);
	}
	Assert(openness >= 0.f);
	openness = Clamp(openness, GetMinOpenness(), 1.f);

#if __BANK
	if(g_DebugVehicleOpenness)
	{
		char buf[64];
		formatf(buf, "%.2f", openness);
		if(cameraRelative)
		{
			grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()),Color32(0,255,0), buf);
		}
		else
		{
			grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition() + Vec3V(0.f,0.f,1.f),Color32(255,0,255), buf);
		}
	}
	
#endif

	if(cameraRelative)
	{
		m_CachedCameraRelativeOpenness = openness;
	}
	else
	{
		m_CachedNonCameraRelativeOpenness = openness;
	}

	return openness;
}

// ----------------------------------------------------------------
// GetConvertibleRoofOpenness
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::GetConvertibleRoofOpenness() const
{
	if(m_Vehicle)
	{
		switch(m_Vehicle->GetConvertibleRoofState())
		{	
		case CTaskVehicleConvertibleRoof::STATE_RAISING:
		case CTaskVehicleConvertibleRoof::STATE_LOWERING:		
		case CTaskVehicleConvertibleRoof::STATE_CLOSING_BOOT:
			{
				return sm_RoofToOpennessCurve.CalculateValue(m_Vehicle->GetMoveVehicle().GetMechanismPhase());
			}
			break;
		case CTaskVehicleConvertibleRoof::STATE_LOWERED:
		case CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_LOWERED:
			// fully open
			return 1.0f;
			break;
		default:
			return 0.0f;
			break;
		}
	}	

	return 0.0f;
}

// ----------------------------------------------------------------
// InitOcclusion
// ----------------------------------------------------------------
void audVehicleAudioEntity::InitOcclusion()
{
	CreateEnvironmentGroup("Vehicle");

	if (m_EnvironmentGroup)
	{
		m_EnvironmentGroup->Init(this, sm_CheapDistance, 1000);
		m_EnvironmentGroup->SetIsVehicle(true);
		if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
		{
			m_EnvironmentGroup->SetUsePortalOcclusion(true);
		}
	}
}

// ----------------------------------------------------------------
// Trigger a speed boost sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerStuntRaceSpeedBoost()
{
	u32 time = fwTimer::GetTimeInMilliseconds();

	if(time - m_LastStuntRaceSpeedBoostTime < g_StuntBoostIntensityTimeout)
	{
		if(time - m_LastStuntRaceSpeedBoostIntensityIncreaseTime > g_StuntBoostIntensityIncreaseDelay)
		{
			m_SpeedBoostIntensity++;
			m_LastStuntRaceSpeedBoostIntensityIncreaseTime = time;
		}
	}
	else
	{
		m_SpeedBoostIntensity = 0;
	}
	
	audSoundSet stuntBoostSoundset;

	if(m_VehicleType == AUD_VEHICLE_PLANE || m_VehicleType == AUD_VEHICLE_HELI)
	{
		stuntBoostSoundset.Init(ATSTRINGHASH("DLC_Air_Race_Sounds", 0xD2A9FD4E));
	}
	else
	{
		stuntBoostSoundset.Init(ATSTRINGHASH("DLC_Stunt_Race_Sounds", 0x85CD8A73));
	}

	if(stuntBoostSoundset.IsInitialised())
	{
		// Needed for fake revs behavior
		AllocateVehicleVariableBlock();

		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.SetVariableValue(ATSTRINGHASH("intensity", 0xEFCD993D), (f32)(m_SpeedBoostIntensity - 1));
		u32 boostSound = m_IsFocusVehicle? ATSTRINGHASH("Speed_Up_Player", 0x9096D9FB) : ATSTRINGHASH("Speed_Up_Remote", 0xE1774624);
		CreateDeferredSound(stuntBoostSoundset.Find(boostSound), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(stuntBoostSoundset.GetNameHash(), ATSTRINGHASH("Speed_Up_Remote", 0xE1774624), &initParams, m_Vehicle));
	}

	if(m_IsFocusVehicle && !sm_StuntRaceSpeedUpScene && SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() && NetworkInterface::IsGameInProgress())
	{				
		sm_TriggerStuntRaceSpeedBoostScene = true;
		sm_StuntRaceVehicleType = m_VehicleType;
	}

	audDisplayf("Triggering stunt race boost with intensity %u", m_SpeedBoostIntensity);
	m_LastStuntRaceSpeedBoostTime = time;
}

// ----------------------------------------------------------------
// Trigger a slow down sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerStuntRaceSlowDown()
{	
	m_SpeedBoostIntensity = 0;
	audSoundSet stuntBoostSoundset;

	if(m_VehicleType == AUD_VEHICLE_PLANE || m_VehicleType == AUD_VEHICLE_HELI)
	{
		stuntBoostSoundset.Init(ATSTRINGHASH("DLC_Air_Race_Sounds", 0xD2A9FD4E));
	}
	else
	{
		stuntBoostSoundset.Init(ATSTRINGHASH("DLC_Stunt_Race_Sounds", 0x85CD8A73));
	}

	if(stuntBoostSoundset.IsInitialised())
	{
		// Needed for fake revs behavior
		AllocateVehicleVariableBlock();

		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		u32 boostSound = m_IsFocusVehicle? ATSTRINGHASH("Slow_Down_Player", 0xC12528C6) : ATSTRINGHASH("Slow_Down_Remote", 0x2E6079D2);
		CreateDeferredSound(stuntBoostSoundset.Find(boostSound), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(stuntBoostSoundset.GetNameHash(), ATSTRINGHASH("Slow_Down_Remote", 0x2E6079D2), &initParams, m_Vehicle));
	}
	 
	if(m_IsFocusVehicle && !sm_StuntRaceSlowDownScene && SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() && NetworkInterface::IsGameInProgress())
	{				
		sm_TriggerStuntRaceSlowDownScene = true;
		sm_StuntRaceVehicleType = m_VehicleType;
	}
}

// ----------------------------------------------------------------
// Trigger missile battery reload sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerMissileBatteryReloadSound()
{
	audSoundSet missileSoundSet;

	if(missileSoundSet.Init(ATSTRINGHASH("gr_vehicle_weapon_trailer_missile_sounds", 0x1B9A6F4D)))
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;		
		CreateDeferredSound(missileSoundSet.Find(ATSTRINGHASH("Flaps_Close", 0x4EAD5EE9)), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(missileSoundSet.GetNameHash(), ATSTRINGHASH("Flaps_Close", 0x4EAD5EE9), &initParams, m_Vehicle));
	}
}

// ----------------------------------------------------------------
// Trigger an instant refill sound on cars with a boost mechanism
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerBoostInstantRecharge()
{
	u32 time = fwTimer::GetTimeInMilliseconds();

	if(time - m_LastStuntRaceRechargeTime < g_RechargeIntensityTimeout)
	{
		if(time - m_LastRechargeIntensityIncreaseTime > g_RechargeIntensityIncreaseDelay)
		{
			m_RechargeIntensity++;
			m_LastRechargeIntensityIncreaseTime = time;
		}
	}
	else
	{
		m_RechargeIntensity = 0;
	}

	audSoundSet rechargeSoundSet;

	if(rechargeSoundSet.Init(ATSTRINGHASH("DLC_Special_Race_Sounds", 0x99BFF6E)))
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.SetVariableValue(ATSTRINGHASH("intensity", 0xEFCD993D), (f32)(m_RechargeIntensity - 1));
		u32 rechargeSound = m_IsFocusVehicle? ATSTRINGHASH("instant_refill_player", 0x24C5D426) : ATSTRINGHASH("instant_refill_remote", 0xD923B0F8);
		CreateDeferredSound(rechargeSoundSet.Find(rechargeSound), m_Vehicle, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(rechargeSoundSet.GetNameHash(), ATSTRINGHASH("instant_refill_remote", 0xD923B0F8), &initParams, m_Vehicle));
	}

	audDisplayf("Triggering recharge with intensity %u", m_RechargeIntensity);
	m_LastStuntRaceRechargeTime = time;
}

// ----------------------------------------------------------------
// SetEnvironmentGroupSettings
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetEnvironmentGroupSettings(const bool useInteriorDistanceMetrics, const u8 maxPathDepth)
{
	if (m_EnvironmentGroup)
	{
		m_EnvironmentGroup->SetUseInteriorDistanceMetrics(useInteriorDistanceMetrics);
		m_EnvironmentGroup->SetMaxPathDepth(maxPathDepth);
	}
}

// ----------------------------------------------------------------
// Get the vehicle model name
// ----------------------------------------------------------------
#if !__NO_OUTPUT
const char *audVehicleAudioEntity::GetVehicleModelName() const
{
	CVehicleModelInfo *model = (CVehicleModelInfo*)(m_Vehicle->GetBaseModelInfo());
	naAssertf(model, "Couldn't get CVehicleModelInfo for m_vehicle, about to access a null ptr...");
	return model->GetModelName();
}
#endif // !__NO_OUTPUT

#if NA_RADIO_ENABLED
// ----------------------------------------------------------------
// Set radio station from network
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetRadioStationFromNetwork(u8 radioStationIndex, bool bTriggeredByDriver)
{	
	u8 currentStationIndex = GetRadioStation() ? (u8) GetRadioStation()->GetStationIndex() : -1;
	m_NetworkCachedRadioStationIndex = radioStationIndex;

	if (radioStationIndex != currentStationIndex)
	audDisplayf("Network triggered radio station switch on vehicle %s: station %u, current %u, driver: %s, focus %s", GetVehicleModelName(), radioStationIndex, currentStationIndex, bTriggeredByDriver? "true" : "false", m_IsFocusVehicle? "true" : "false");

	const bool localPlayerInCar = (CGameWorld::FindLocalPlayerVehicle()==m_Vehicle);

	bool followPlayerInCar = false;
	if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnSCTVTeam())
	{
		CPed* pFollowPlayer = CGameWorld::FindFollowPlayer();
		if (pFollowPlayer)
		{
			CVehicle* pFollowPlayerVehicle = pFollowPlayer->GetMyVehicle();
			if (pFollowPlayerVehicle)
			{
				followPlayerInCar = (pFollowPlayerVehicle==m_Vehicle);

				if (followPlayerInCar && NetworkInterface::IsOverrideSpectatedVehicleRadioSet())
					return;
			}
		}
	}

	if(radioStationIndex == g_NullRadioStation || radioStationIndex == g_OffRadioStation)
	{
		if((radioStationIndex != currentStationIndex) || !m_IsRadioOff)
		{
			audDisplayf("NULL/Off station requested - switching off radio (current: %u new: %u, current status: %s)", currentStationIndex, radioStationIndex, m_IsRadioOff? "OFF" : "ON");

			if(m_RadioStation)
			{
				g_RadioAudioEntity.StopVehicleRadio(m_Vehicle);
			}

			if (radioStationIndex == g_OffRadioStation)
			{
				if (localPlayerInCar || followPlayerInCar)
					g_RadioAudioEntity.SwitchOffRadio();			
			}

			SetRadioOffState(true); //off
			m_RadioStation = NULL;
		}
	}
	else
	{
		//Note: bail if radio station is already set as this can cause problems if we keep setting to the same channel
		if (radioStationIndex != currentStationIndex)
		{
			SetRadioOffState(false); //on

			const audRadioStation *station = audRadioStation::GetStation(radioStationIndex);
			audDisplayf("Valid station selected (%s) - switching on radio. localPlayerInCar: %s, followPlayerInCar: %s", station->GetStationSettings()->Name, localPlayerInCar? "true" : "false", followPlayerInCar? "true" : "false");

			SetLastRadioStation(station);
			// Only request a radio emitter if we already had one, otherwise let the normal distance/priority logic decide
			if(m_RadioStation || localPlayerInCar || followPlayerInCar || m_IsPersonalVehicle)
			{
				g_RadioAudioEntity.StopVehicleRadio(m_Vehicle);
				m_RadioStation = g_RadioAudioEntity.RequestVehicleRadio(m_Vehicle);			
			}
		
			// also need to update frontend radio
			if(localPlayerInCar || followPlayerInCar)
			{
				g_RadioAudioEntity.PlayStationSelectSound(true);
				g_RadioAudioEntity.RetuneToStation(station);

				if(bTriggeredByDriver)
					g_RadioAudioEntity.SetMPDriverTriggeredRadioChange();
			}
		}
	}
}

void audVehicleAudioEntity::ResetRadioStationFromNetwork()
{
	audDisplayf("ResetRadioStationFromNetwork");
	SetRadioStationFromNetwork(m_NetworkCachedRadioStationIndex, false);
}

// ----------------------------------------------------------------
// Get Ambient Radio Volume
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::GetAmbientRadioVolume() const
{
	if(IsAmbientRadioDisabled())
	{
		return -100.f;
	}
	else
	{
		f32 carVol = GetVehicleModelRadioVolume();
		
		if(m_HasLoudRadio)
		{
			carVol += g_LoudVehicleRadioOffset;
		}	

		f32 radioStationVol = (m_RadioStation ? m_RadioStation->GetAmbientVolume():0.f);
		f32 finalVolume = g_AmbientVehicleRadioOffset + carVol + m_RadioOpennessVol + radioStationVol;

		if(finalVolume > 32.f)
		{
			audWarningf("Vehicle ambient radio volume for station %s is %.02f, seems excessive! AmbientVehicleRadioOffset %.02f, carVol %.02f, radioOpennessVol %.02f, radioStationVol %.02f"
				, m_RadioStation? m_RadioStation->GetName() : "NULL"
				, finalVolume
				, g_AmbientVehicleRadioOffset
				, carVol
				, m_RadioOpennessVol
				, radioStationVol			
				);
		}		

		return finalVolume;
	}
}

// ----------------------------------------------------------------
// Get Frontend Radio Volume Offset
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::GetFrontendRadioVolumeOffset()
{
	static f32 g_LoudVehicleRadioVolume = 0.0f;
	static f32 g_VeryLoudVehicleRadioVolume = 1.0f;
	f32 volume = 0.0f;

	if(GetVehicleVolumeCategory() == VEHICLE_VOLUME_LOUD)
	{
		volume = g_LoudVehicleRadioVolume;
	}
	else if(GetVehicleVolumeCategory() == VEHICLE_VOLUME_VERY_LOUD)
	{
		volume = g_VeryLoudVehicleRadioVolume;
	}

	return volume;
}

// ----------------------------------------------------------------
// GetHasNormalRadio
// ----------------------------------------------------------------
bool audVehicleAudioEntity::GetHasNormalRadio() const
{
	if(m_RadioType == RADIO_TYPE_NORMAL_OFF_MISSION_AND_MP)
	{
		return NetworkInterface::IsGameInProgress() || !CTheScripts::GetPlayerIsOnAMission();
	}
	return (m_RadioType == RADIO_TYPE_NORMAL || (m_RadioType == RADIO_TYPE_EMERGENCY_SERVICES && NetworkInterface::IsGameInProgress()));
}

// ----------------------------------------------------------------
// GetHasEmergencyServicesRadio
// ----------------------------------------------------------------
bool audVehicleAudioEntity::GetHasEmergencyServicesRadio() const
{
	return (m_RadioType == RADIO_TYPE_EMERGENCY_SERVICES);
}

// ----------------------------------------------------------------
// IsAmbientRadioDisabled
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsAmbientRadioDisabled() const
{
	return m_AmbientRadioDisabled;
}

// ----------------------------------------------------------------
// Check if the radio is enabled for this vehicle
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsRadioEnabled() const
{	
	if (g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::PlayerOnDLCHeist4Island))
	{
		return false;
	}

    bool radioOnEngineOff = false;	
	if(m_VehicleType == AUD_VEHICLE_HELI || m_VehicleType == AUD_VEHICLE_PLANE || GetVehicleModelNameHash() == ATSTRINGHASH("TORO", 0x3FD5AA2F) || GetVehicleModelNameHash() == ATSTRINGHASH("TORO2",0x362CAC6D))
	{
		if(m_IsPlayerVehicle && m_IsPlayerSeatedInVehicle && m_Vehicle->GetStatus() != STATUS_WRECKED)
		{			
			radioOnEngineOff = true;
		}
	}	

	return ((m_Vehicle->m_nVehicleFlags.bEngineOn || radioOnEngineOff) && !m_RadioDisabled && (m_RadioStation || !IsPlayingStartupSequence()) REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()));
}

// ----------------------------------------------------------------
// SetLoudRadio
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetLoudRadio(const bool loud)
{
	m_HasLoudRadio = loud;	
}

bool audVehicleAudioEntity::HasLoudRadio() const
{
	if(m_HasLoudRadio)
		return true;
	return m_Vehicle->m_nVehicleFlags.bSlowChillinDriver;
}

// ----------------------------------------------------------------
// SetRadioStation
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetRadioStation(u8 radioStationIndex)
{
	SetRadioStation(audRadioStation::GetStation(radioStationIndex));
}

// ----------------------------------------------------------------
// SetRadioStation
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetRadioStation(const audRadioStation *station, const bool alsoRetuneFE /*= true*/, const bool isForcedStation)
{
	if (m_RadioStation == station)
	{
		return;
	}

	// Don't allow script to set a station on a vehicle with no radio, since the user won't be able to retune
	if(!GetHasNormalRadio())
	{
		return;
	}

#if RSG_PC
	// url:bugstar:5172897 - Don't allow retuning to the user radio station on NPC vehicles if we don't have it available on the local machine
	if (station && (station->IsLocked() || station->IsHidden()) && !m_IsPlayerVehicle && NetworkInterface::IsGameInProgress())
	{
		return;
	}
#endif

	// Also retune the player station if this is the player vehicle
	if(alsoRetuneFE && g_RadioAudioEntity.GetLastPlayerVehicle() == m_Vehicle && g_RadioAudioEntity.IsPlayerRadioActive() && g_RadioAudioEntity.GetPlayerRadioStation() != station)
	{
		g_RadioAudioEntity.RetuneToStation(station);
	}
	else
	{
		m_RadioStation = station;
		if(m_RadioStation)
		{
			m_IsRadioOff = false;
			SetLastRadioStation(station);

			if(g_RadioAudioEntity.GetPlayerVehicle() != m_Vehicle && !isForcedStation)
			{
				bool shouldStopRadio = false;
				if(ShouldRequestRadio(GetAmbientRadioLeakage(), shouldStopRadio))
				{
					m_RadioStation = g_RadioAudioEntity.RequestVehicleRadio(m_Vehicle);
				}
			}
		}
	}
		
	// If we changed the station, check to see if any passengers playing an ambient clip need to reevaluate their base clip. [11/13/2012 mdawe]
	// This is to stop a "dance to music" animation from continuing to play if we've switched to talk radio (for example).
	const int iNumSeats = m_Vehicle->GetSeatManager()->GetMaxSeats();
	for (int iSeat = 0; iSeat < iNumSeats; ++iSeat)
	{
		CPed* pPed = m_Vehicle->GetPedInSeat(iSeat);
		if (pPed)
		{
			//Check base if we're running CTaskAmbientClips.
			CTaskAmbientClips* pAmbientClips = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pAmbientClips)
			{
				// Force CTaskAmbientClips to update the base clip, which will stop playing it if the conditions on it are no longer met.
				pAmbientClips->TerminateIfBaseConditionsAreNotMet();
			}
		}
	}
}
#endif

// ----------------------------------------------------------------
// GetVehicleModelNameHash
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::GetVehicleModelNameHash() const
{
	if(m_Vehicle)
	{
		CVehicleModelInfo *model = (CVehicleModelInfo*)(m_Vehicle->GetBaseModelInfo());
		naAssertf(model, "Couldn't get CVehicleModelInfo for m_vehicle, about to access a null ptr...");
		return model->GetModelNameHash();
	}
	
	return 0u;
}

// ----------------------------------------------------------------
// Trigger a trailer detach sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerTrailerDetach()
{
	if(!IsDisabled())
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.UpdateEntity = true;

		if(sysThreadType::IsUpdateThread())
		{
			CreateDeferredSound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("TrailerUnhitch", 0x53130271)), m_Vehicle, &initParams, true);
		}
		else
		{
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("TrailerUnhitch", 0x53130271)), &initParams);
		}
	}
}

// ----------------------------------------------------------------
// Trigger a windscreen smash sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerWindscreenSmashSound(Vec3V_In position)
{	
	if(m_IsPlayerVehicle && audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(position);
		initParams.UpdateEntity = true;
		CreateDeferredSound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("WindscreenSmash_Interior", 0x4C0BD045)), m_Vehicle, &initParams, true);
	}
}

// ----------------------------------------------------------------
// Trigger a window smash sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerWindowSmashSound(Vec3V_In position)
{	
	if(m_IsPlayerVehicle && audNorthAudioEngine::IsRenderingFirstPersonVehicleCam())
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(position);
		initParams.UpdateEntity = true;
		CreateDeferredSound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("WindowSmash_Interior", 0x4948B2BA)), m_Vehicle, &initParams, true);
	}
}

// ----------------------------------------------------------------
// Trigger a headlight smash
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerHeadlightSmash(const u32 headlightIndex, const Vector3 &localPos)
{
	if(!IsDisabled() && GetAudioVehicleType() != AUD_VEHICLE_BOAT && !g_NoHeadlightSmashAudio)
	{
		Vector3 pos;
		m_Vehicle->TransformIntoWorldSpace(pos, localPos);

		u32 soundHash;
		switch(headlightIndex)
		{
		case VEH_HEADLIGHT_L:			// 00
		case VEH_HEADLIGHT_R:			// 01
			soundHash = ATSTRINGHASH("FRONT_HEADLIGHT_SMASH", 0x4EC863C3);
			break;
		default:
			soundHash = ATSTRINGHASH("REAR_HEADLIGHT_SMASH", 0xEC3A719F);
			break;
		}

		audSoundInitParams initParams;
		initParams.UpdateEntity = true;	
		initParams.Position = pos;
		CreateDeferredSound(soundHash, m_Vehicle, &initParams, true);
	}
}

// ----------------------------------------------------------------
// Trigger tyre puncture
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerTyrePuncture(const eHierarchyId wheelId, bool slowPuncture)
{
	if(!IsDisabled() && wheelId >= VEH_WHEEL_LF && wheelId <= VEH_WHEEL_RR)
	{
		audSoundInitParams initParams;
		s32 wheel = 0;

		naAssertf(m_Vehicle, "Sanity check failed! Vehicle audio entity has no vehicle!");
		if(!m_Vehicle)
		{
			return;
		}

		for(;wheel<m_Vehicle->GetNumWheels(); wheel++)
		{
			if(m_Vehicle->GetWheel(wheel) && m_Vehicle->GetWheel(wheel)->GetHierarchyId() == wheelId)
			{
				break;
			}
		}

		Vector3 pos;
		GetWheelPosition(wheel, pos);
		initParams.Position = pos;
		initParams.UpdateEntity = true;
		initParams.u32ClientVar = GetWheelSoundUpdateClientVar(wheel);
		CreateDeferredSound(slowPuncture?ATSTRINGHASH("FLAT_TYRE_BLOWOUT", 0xAA624CD4):ATSTRINGHASH("VEH_TYRE_BURST", 0xA9D7073), m_Vehicle, &initParams, true);
	}
}
// ----------------------------------------------------------------
// Get a wheel position - just use the cached value if we've already calculated the position this frame
// ----------------------------------------------------------------
void audVehicleAudioEntity::GetCachedWheelPosition(u32 wheelIndex, Vector3 &pos, audVehicleVariables& vehicleVariables)
{
	naAssertf(wheelIndex < NUM_VEH_CWHEELS_MAX, "Invalid wheel index, will result in null ptr access");

	audVehicleWheelPositionCache* wheelCache = vehicleVariables.wheelPosCache;

	if(!wheelCache)
	{
		GetWheelPosition(wheelIndex, pos);
	}
	else if(wheelCache->cachedWheelPositionValid[wheelIndex])
	{
		pos = wheelCache->cachedWheelPositions[wheelIndex];
	}
	else
	{
		GetWheelPosition(wheelIndex, pos);
		wheelCache->cachedWheelPositionValid[wheelIndex] = true;
		wheelCache->cachedWheelPositions[wheelIndex] = pos;
	}
}

// ----------------------------------------------------------------
// Get a wheel position
// ----------------------------------------------------------------
void audVehicleAudioEntity::GetWheelPosition(u32 wheelIndex, Vector3 &pos)
{
	naAssertf(wheelIndex < m_Vehicle->GetNumWheels(), "Invalid wheel index");
	CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)(m_Vehicle->GetBaseModelInfo());

	if(modelInfo)
	{
		CWheel* wheel = m_Vehicle->GetWheel(wheelIndex);

		if(wheel)
		{
			s32 boneIndex = modelInfo->GetBoneIndex(m_Vehicle->GetWheel(wheelIndex)->GetHierarchyId());
			Matrix34 matrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(boneIndex));
			matrix.d = m_Vehicle->TransformIntoWorldSpace(matrix.d);
			pos = matrix.d;
		}
	}
}

// ----------------------------------------------------------------
// Trigger an indicator
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerIndicator(const bool isOn)
{
	if(m_VehicleLOD < AUD_VEHICLE_LOD_DISABLED)
	{
		if(m_DistanceFromListener2LastFrame < 625)
		{
			m_IsIndicatorRequestedOn = isOn;
			m_HasIndicatorRequest = true;
		}
	}
}

// ----------------------------------------------------------------
// Called when the rev limiter is hit
// ----------------------------------------------------------------
void audVehicleAudioEntity::RevLimiterHit()
{
	m_LastRevLimiterTime = fwTimer::GetTimeInMilliseconds();
}

// ----------------------------------------------------------------
// Called when the vehicle is hit by a rocket
// ----------------------------------------------------------------
void audVehicleAudioEntity::SetHitByRocket()
{
	m_LastHitByRocketTime = fwTimer::GetTimeInMilliseconds();
}

// ----------------------------------------------------------------
// Trigger a door sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerDoorSound(u32 hash, eHierarchyId doorId, f32 volumeOffset)
{
	if(!IsDisabled() && hash != g_NullSoundHash REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)(m_Vehicle->GetBaseModelInfo());
		s32 doorBoneIndex = modelInfo->GetBoneIndex(doorId);
		Matrix34 doorMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(doorBoneIndex));
		doorMatrix.d = m_Vehicle->TransformIntoWorldSpace(doorMatrix.d);

		audSoundInitParams initParams;
		initParams.UpdateEntity = true;	
		initParams.Volume = volumeOffset;
		initParams.Position = doorMatrix.d;

		if(sysThreadType::IsUpdateThread())
		{
			CreateDeferredSound(hash, m_Vehicle, &initParams, true);
		}
		else
		{
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(hash, &initParams);
		}

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hash, &initParams, m_Vehicle));

		CCarDoor *door = m_Vehicle->GetDoorFromId(doorId);

		if(door)
		{
			door->TriggeredAudio(fwTimer::GetTimeInMilliseconds());
		}
	}
}

// ----------------------------------------------------------------
// Trigger a landing gear sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerLandingGearSound(const audSoundSet& soundset, const u32 soundFieldHash)
{
	audMetadataRef soundRef = soundset.Find(soundFieldHash);

	if(soundRef != g_NullSoundRef)
	{
		// Stop the existing sound if we're triggering a new one
		if(m_LandingGearSound)
		{
			m_LandingGearSound->StopAndForget(true);
		}

		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;

		CreateSound_PersistentReference(soundRef, &m_LandingGearSound, &initParams);

		if(m_LandingGearSound)
		{
			m_LandingGearSound->SetUpdateEntity(true);
			m_LandingGearSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_UNKNOWN);
			m_LandingGearSound->PrepareAndPlay();
			REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundset.GetNameHash(), soundFieldHash, &initParams, m_LandingGearSound, GetOwningEntity()));
		}
	}
}

// ----------------------------------------------------------------
// Update Doors
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateDoors()
{
	for(s32 i = 0; i < m_Vehicle->GetNumDoors(); i++)
	{		
		CCarDoor *door = m_Vehicle->GetDoor(i);

		// Need a way to detect sliding cockpit doors? Adding vehicle type check seems to work ok for all planes
		if(door && (door->GetFlag(CCarDoor::IS_ARTICULATED) || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)) 
		{
			// If we're in the mod shop, suppress door sounds when script open/shut doors as part of the mod shop menu system, as they trigger their own bespoke sounds
			// This allows remote players (possible in the office mod shop) to open/close doors and everyone to hear the regular open/close sounds		
			if(fwTimer::GetTimeInMilliseconds() > door->GetLastAudioTime()+250 &&
			  (!m_InCarModShop || (m_InCarModShop && NetworkInterface::IsGameInProgress() && fwTimer::GetTimeInMilliseconds() - m_LastScriptDoorModifyTime > g_ModShopDoorRecentlyUsedTime)))
			{
				f32 oldDoorRatio = door->GetOldAudioRatio();
				f32 doorRatio = door->GetDoorRatio();

#if __BANK
				if(g_ShowDoorRatios)
				{
					CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)(m_Vehicle->GetBaseModelInfo());
					s32 doorBoneIndex = modelInfo->GetBoneIndex(door->GetHierarchyId());
					Matrix34 doorMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(doorBoneIndex));

					char tempString[128];
					sprintf(tempString, "Door Ratio %4.2f  Old %4.2f", doorRatio, oldDoorRatio);
					grcDebugDraw::Text(m_Vehicle->TransformIntoWorldSpace(doorMatrix.d), Color_white, tempString);
				}
#endif

				// No work to do if the ratios are the same
				if(oldDoorRatio != doorRatio)
				{
					f32 speed = Abs((door->GetOldAudioRatio()-door->GetDoorRatio()) * fwTimer::GetInvTimeStep());

					static bank_float cargobobRearDoorOpenRatio = 0.76f;
					if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || m_Vehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
					{
						static bank_float startCloseRatio = 0.96f;
						if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_HELI && door->GetHierarchyId() == VEH_DOOR_DSIDE_R &&
							doorRatio <= 0.7f && doorRatio > 0.6f && oldDoorRatio > doorRatio && speed > 1.0f)
						{
							TriggerDoorStartCloseSound(door->GetHierarchyId());
						}
						else if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_HELI && door->GetHierarchyId() == VEH_DOOR_DSIDE_R &&
							doorRatio >= cargobobRearDoorOpenRatio && oldDoorRatio < doorRatio && speed > 0.5f)
						{
							TriggerDoorFullyOpenSound(door->GetHierarchyId());
						}
						else if(doorRatio <= startCloseRatio && oldDoorRatio > startCloseRatio)
						{
							//Start to close
							TriggerDoorStartCloseSound(door->GetHierarchyId());
						}
						else if(doorRatio <= 0.1f && oldDoorRatio > 0.1f)
						{	
							TriggerDoorCloseSound(door->GetHierarchyId(), speed < 1.f);
						}
						else if(oldDoorRatio <= 0.01f && doorRatio > 0.01f)
						{
							TriggerDoorOpenSound(door->GetHierarchyId());
							TriggerDoorStartOpenSound(door->GetHierarchyId());
						}
						else if(doorRatio >= 0.99f && oldDoorRatio < 0.98f)
						{
							TriggerDoorFullyOpenSound(door->GetHierarchyId());
						}
					}
					else
					{
						f32 doorOpenLimit = 0.01f;
						f32 doorCloseLimit = 0.1f;

						// HL - Somewhat hacky fix for GTAV B*2187239, where trash compactor rear door isn't triggering consistently.
						// Ideal fix would be to:
						// a) Support doorStartOpen/DoorStartClose sounds on all vehicle types
						// b) Define trigger ratios for each sound type in the vehicle gameobject
						if(GetVehicleModelNameHash() == ATSTRINGHASH("TRASH2", 0xB527915C) && door->GetHierarchyId() == VEH_BOOT)
						{
							doorOpenLimit = 0.2f;
							doorCloseLimit = 0.8f;
						}

						// url:bugstar:6185262 - Vagrant - The rear hood door audio occasionally does not fire when it is opened/closed in the Interaction Menu.
						if (GetVehicleModelNameHash() == ATSTRINGHASH("VAGRANT", 0x2C1FEA99) && door->GetHierarchyId() == VEH_BONNET)
						{
							doorOpenLimit = 0.2f;
							doorCloseLimit = 0.8f;
						}
						// End hacky fix.

						if(m_Vehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE && doorRatio < 0.98f && oldDoorRatio > 0.98f)
						{
							TriggerDoorStartCloseSound(door->GetHierarchyId());
						}
						else if(doorRatio < doorCloseLimit && oldDoorRatio >= doorCloseLimit)
						{
							TriggerDoorCloseSound(door->GetHierarchyId(), speed < 1.f);
						}
						else if(oldDoorRatio < doorOpenLimit && doorRatio >= doorOpenLimit)
						{
							TriggerDoorOpenSound(door->GetHierarchyId());

							//Need to trigger sliding door open sound here, can't tag up get_out animation because can play when door's already open
							if (door->GetFlag(CCarDoor::AXIS_SLIDE_X) || door->GetFlag(CCarDoor::AXIS_SLIDE_Y) || door->GetFlag(CCarDoor::AXIS_SLIDE_Z))
							{
								TriggerDoorSound(ATSTRINGHASH("DLC_LOWRIDER_DOOR_SLIDE_MOVE_OPEN", 0xb8d0c956), door->GetHierarchyId());
							}
						}
						else if(doorRatio >= 0.99f && oldDoorRatio < 0.98f)
						{
							TriggerDoorFullyOpenSound(door->GetHierarchyId());
						}
					}
				}
			}

			door->AudioUpdate();
		}
	}
}

// ----------------------------------------------------------------
// Stop the siren if broken
// ----------------------------------------------------------------
void audVehicleAudioEntity::StopSirenIfBroken()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	CVehicle *playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(m_IsSirenFucked && (!playerVeh && player == m_LastVehicleDriver))
	{
		if (m_SirenLoop)
		{
			m_SirenLoop->StopAndForget();
			m_PlaySirenWithNoNetworkPlayerDriver = false;
			m_SirenState = AUD_SIREN_OFF;
		}
	}
}
// ----------------------------------------------------------------
// Smash the siren
// ----------------------------------------------------------------
void audVehicleAudioEntity::SmashSiren(bool force)
{
	if(!m_IsSirenFucked)
	{
		if(m_Vehicle->GetVehicleDamage()->GetSirenHealth() >= 1 && ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), 0.1f))
		{
			m_IsSirenFucked = true;
		}
		else if(m_Vehicle->GetVehicleDamage()->GetSirenHealth() >= 2 &&  ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), 0.25f))
		{
			m_IsSirenFucked = true;
		}
		else if (m_Vehicle->GetVehicleDamage()->GetSirenHealth() >= 3 && ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), 0.65f))
		{
			m_IsSirenFucked = true;
		}
		
		if (force)
		{
			m_IsSirenFucked = force;
		}

		if(m_IsSirenFucked)
		{
			if(m_SirenLoop && m_SirenState == AUD_SIREN_SLOW  && (force || m_Vehicle->GetVehicleDamage()->GetEngineHealth() < g_VehHealthThreshold))
			{
				m_SirenLoop->FindAndSetVariableValue(ATSTRINGHASH("isFucked", 0x82DA7FEC), 1.f);
				m_SirenLoop->StopAndForget();
			}
			else
			{
				if(force || ENTITY_SEED_PROB(m_Vehicle->GetRandomSeed(), 0.5f))
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.RemoveHierarchy = false;
					initParams.SetVariableValue(ATSTRINGHASH("isFucked", 0x82DA7FEC), 1.f);

					if(sysThreadType::IsUpdateThread())
					{
						CreateDeferredSound(m_VehSirenSounds.Find(ATSTRINGHASH("FUCKED_ONE_SHOT", 0x5417639D)), m_Vehicle, &initParams, true);
					}
					else
					{
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						CreateAndPlaySound(m_VehSirenSounds.Find(ATSTRINGHASH("FUCKED_ONE_SHOT", 0x5417639D)), &initParams);
					}
				}
			}

			m_PlaySirenWithNoNetworkPlayerDriver = false;
			m_SirenState = AUD_SIREN_OFF;
		}
	}
}

// ----------------------------------------------------------------
// Blip siren
// ----------------------------------------------------------------
void audVehicleAudioEntity::BlipSiren()
{
	if(m_Vehicle)
	{
		if(m_Vehicle->UsesSiren() && !m_SirenLoop && !m_SirenBlip && !m_IsSirenFucked && m_VehSirenSounds.IsInitialised())
		{ 
			audSoundInitParams initParams; 
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateSound_PersistentReference(m_VehSirenSounds.Find(ATSTRINGHASH("BLIP", 0x26D96F23)),&m_SirenBlip,&initParams);
			if(m_SirenBlip)
			{
				m_SirenBlip->PrepareAndPlay();
			}
		}
	}
}
// ----------------------------------------------------------------------------------------------------------------
// Tells the audio entity that a change has been made to the siren state.  That helps us decide which sound to play
// ----------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::SirenStateChanged(bool on)
{
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh && pVeh == m_Vehicle)
	{
		if(on)
		{
			if(!m_IsSirenOn)
			{
				// siren just turn on, start checking the time. 
				m_TimeToPlaySiren = 0;
				m_WantsToPlaySiren = true;
				m_ShouldPlayPlayerVehSiren = false;
			}
		}
		else 
		{
			m_IsSirenOn = false;
			m_WantsToPlaySiren = false;
			if(!m_ShouldPlayPlayerVehSiren)
			{
				//We stop the siren before it plays, play the blip instead
				BlipSiren();
			}
		}
	}
}
u32 audVehicleAudioEntity::GetVehicleHornSoundHash(bool UNUSED_PARAM(ignoreMods))
{
	return  0;
}

// ----------------------------------------------------------------
// Supports audDynamicEntitySound queries
// ----------------------------------------------------------------
u32 audVehicleAudioEntity::QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs)
{
	naAssertf(objectRefs, "A null object ref array ptr has been passed into audVehicleAudioEntity::QuerySoundNameFromObjectAndField");
	naAssertf(numRefs >= 2,"The object ref list should always have at least two entries when entering QuerySoundNameFromObjectAndField");
	naAssertf(m_Vehicle, "Tried to QuerySoundNameFromObject but m_Vehicle is invalid which could result in null ptr access");

	if(numRefs ==2 && m_Vehicle->GetVehicleAudioEntity())
	{

		if(objectRefs[1] == ATSTRINGHASH("HornSounds", 0xAD9FE71E) || objectRefs[1] == ATSTRINGHASH("HornSound", 0x4DD0098F) )
		{ 
			u32 hornSound = GetVehicleHornSoundHash();
			CVehicleVariationInstance& variation = m_Vehicle->GetVariationInstance();
			if(variation.GetMods()[VMT_HORN] != 255)
			{
				if(variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier() != 0)
				{
					hornSound = variation.GetKit()->GetStatMods()[variation.GetMods()[VMT_HORN]].GetModifier();
				}
			}

			//naDisplayf("HORN : %u", hornSound);
			return hornSound;
		} 
		//We're at the end of the object ref chain, see if we have a valid object to query
		else if(*objectRefs == ATSTRINGHASH("CAR", 0x69697274))
		{
			if(m_Vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				++objectRefs;
				return m_Vehicle->GetVehicleAudioEntity()->GetSoundFromObjectData(*objectRefs);
			}
		}
		else if(*objectRefs == ATSTRINGHASH("HELI", 0xD3F50E77))
		{
			if(m_Vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
			{
				++objectRefs;
				return m_Vehicle->GetVehicleAudioEntity()->GetSoundFromObjectData(*objectRefs); 
			}
		}
		else if(*objectRefs == ATSTRINGHASH("BOAT", 0x6910F770))
		{
			if(m_Vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BOAT)
			{
				++objectRefs;
				return m_Vehicle->GetVehicleAudioEntity()->GetSoundFromObjectData(*objectRefs); 
			}
		}
		else if(*objectRefs == ATSTRINGHASH("BICYCLE", 0x9B943939))
		{
			if(m_Vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BICYCLE)
			{
				++objectRefs;
				return m_Vehicle->GetVehicleAudioEntity()->GetSoundFromObjectData(*objectRefs); 
			}
		}
		else if(*objectRefs == ATSTRINGHASH("PLANE", 0x198530F1))
		{
			if(m_Vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
			{
				++objectRefs;
				return m_Vehicle->GetVehicleAudioEntity()->GetSoundFromObjectData(*objectRefs); 
			}
		}
		else if(*objectRefs == ATSTRINGHASH("TRAILER", 0x5C27AA11))
		{
			if(m_Vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAILER)
			{
				++objectRefs;
				return m_Vehicle->GetVehicleAudioEntity()->GetSoundFromObjectData(*objectRefs); 
			}
		}
		else
		{
			naWarningf("Attempting to query an object field in vehicleaudioentity but object is not a supported type");
		}
	}
	else
	{
		--numRefs;
		if(*objectRefs == ATSTRINGHASH("DRIVER", 0x7CF5B63F))
		{
			++objectRefs;
			CPed *pDriver = m_Vehicle->GetDriver();
			if(pDriver)
			{
				return pDriver->GetPedAudioEntity()->QuerySoundNameFromObjectAndField(objectRefs, numRefs);
			}
		}
		else
		{
			naWarningf("Unsupported object type (%d) in reference chain passed into audCarAudioEntity::QuerySoundNameFromObjectAndField", *objectRefs);	
		}

	}

	return 0; 
}

// ----------------------------------------------------------------
// ScriptAppliedForce
// ----------------------------------------------------------------
void audVehicleAudioEntity::ScriptAppliedForce(const Vector3 &vecForce)
{
	const f32 forceMag = vecForce.Mag();
	audSoundInitParams initParams;

	dev_float magScalar = 1.f/5.f;

	initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(Min(1.f, magScalar * forceMag));
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	CreateAndPlaySound(ATSTRINGHASH("SUSPENSION_SCRIPT_FORCE", 0x742C529), &initParams);
}

// ----------------------------------------------------------------
// Trigger a door breaking off
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerDoorBreakOff(const Vector3 &pos, bool isBonnet, fragInst* doorFragInst)
{
	if(!IsDisabled())
	{
		audSoundInitParams initParams;

        if(!sysThreadType::IsUpdateThread())
        {
		    initParams.EnvironmentGroup = m_EnvironmentGroup;
        }

		initParams.Position = pos;

		if(isBonnet)
		{
            if(sysThreadType::IsUpdateThread())
            {
                CreateDeferredSound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleBonnetBreakOff", 0x37EF7329)), m_Vehicle, &initParams, true);
            }
            else
            {
                CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleBonnetBreakOff", 0x37EF7329)), &initParams);
            }			

			if(m_IsPlayerVehicle)
			{
				if(doorFragInst)
				{
					m_DetachedBonnetLevelIndex = doorFragInst->GetLevelIndex();
				}                
                
                m_ShouldPlayBonnetDetachSound = true;                
			}
		}
		else
		{
            if(sysThreadType::IsUpdateThread())
            {
                CreateDeferredSound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleDoorBreakOff", 0xA432CD83)), m_Vehicle, &initParams, true);
            }
            else
            {
			    CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleDoorBreakOff", 0xA432CD83)), &initParams);
            }
		}
	}
}

// ----------------------------------------------------------------
// Update the bonnet loop
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateDetachedBonnetSound(bool firstFrame)
{
    if(m_ShouldPlayBonnetDetachSound)
    {
        if(!m_DetachedBonnetSound)
        {
            audSoundInitParams initParams;
            initParams.EnvironmentGroup = m_EnvironmentGroup;
            CreateAndPlaySound_Persistent(sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleBonnetFlyPast", 0xC334DF01)), &m_DetachedBonnetSound, &initParams);
        }

        m_ShouldPlayBonnetDetachSound = false;
    }

	if(m_DetachedBonnetSound)
	{
		phCollider* pBonnetCollider = CPhysics::GetSimulator()->GetCollider(m_DetachedBonnetLevelIndex);

		if(pBonnetCollider && (firstFrame || MagSquared(pBonnetCollider->GetVelocity()).Getf() > 1.0f))
		{
			m_DetachedBonnetSound->SetRequestedPosition(pBonnetCollider->GetPosition());
		}
		else
		{
			m_DetachedBonnetSound->StopAndForget();
		}
	}
}

// ----------------------------------------------------------------
// Check if this is a sea plane or not
// ----------------------------------------------------------------
bool audVehicleAudioEntity::IsSeaPlane() const
{
	if(m_VehicleType == AUD_VEHICLE_PLANE || m_VehicleType == AUD_VEHICLE_HELI)
	{
		if(m_Vehicle)
		{
			if(m_Vehicle->pHandling && m_Vehicle->pHandling->GetSeaPlaneHandlingData())
			{
				return true;
			}
		}
	}
	
	return false;
}

// ----------------------------------------------------------------
// Trigger a splash
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerBoatEntrySplash(bool goneIntoWater, bool comeOutOfWater)
{
	// Boats don't normally trigger entry splashes, but the APC is an exception
	if(GetVehicleModelNameHash() == ATSTRINGHASH("APC", 0x2189D250))
	{
		float downwardSpeed = Max(0.0f, -m_Vehicle->GetVelocity().z);				

		// get the vfx vehicle info
		CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(m_Vehicle);
		if (pVfxVehicleInfo)
		{
			// play all other water vfx (in/out/wade) if the camera is above or close to the water surface
			if (goneIntoWater)
			{
				TriggerSplash(downwardSpeed);
			}
			else if (comeOutOfWater)
			{
				TriggerSplash(downwardSpeed,true);
			}
		}
	}
}

// ----------------------------------------------------------------
// Trigger a splash
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerSplash(const f32 downSpeed,bool outOfWater)
{	
	if(!IsDisabled() && !IsSeaPlane())
	{
		if(fwTimer::GetTimeInMilliseconds()>m_LastSplashTime+1000 REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()) )
		{
			f32 sqdDistance = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Mag2();
			if (sqdDistance <= g_DistThresholdToTriggerVehSplashes)
			{
				audMetadataRef soundRef = g_NullSoundRef;
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.Tracker = m_Vehicle->GetPlaceableTracker();
				if (!outOfWater)
				{
					f32 velocity = fabs(m_CachedVehicleVelocity.Mag());
					if ( downSpeed >= sm_DownSpeedThreshold || m_JustCameOutOfWater)
					{
						/*f32 mass = m_Vehicle->pHandling->m_fMass;
						f32 volume = size * (m_Vehicle->GetBoundingBoxMax().z - m_Vehicle->GetBoundingBoxMin().z);*/
						//initParams.SetVariableValue(ATSTRINGHASH("DownSpeed", 0x4756096),downSpeed);
						//initParams.SetVariableValue(ATSTRINGHASH("Size", 0xC052DEA7),size);
						//initParams.SetVariableValue(ATSTRINGHASH("Mass", 0x4BC594CB),mass);
						//initParams.SetVariableValue(ATSTRINGHASH("Volume", 0x691362F2),volume);
						if((m_Vehicle && m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)  || m_JustCameOutOfWater)
						{
							if ( velocity * downSpeed < sm_BicycleCombineSpeedMediumThreshold)
							{
								soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("BICYCLE_SMALL", 0x2DB76DC1) : ATSTRINGHASH("BICYCLE_SMALL_UNDERWATER", 0xA1C9F0DE) );
							}
							else if (velocity * downSpeed < sm_BicycleCombineSpeedBigThreshold)
							{
								soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("BICYCLE_MEDIUM", 0xD05884B2) : ATSTRINGHASH("BICYCLE_MEDIUM_UNDERWATER", 0xE7997D8B) );
							}
							else 
							{
								soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("BICYCLE_HEAVY", 0x344D5A48) : ATSTRINGHASH("BICYCLE_HEAVY_UNDERWATER", 0x1152764D) );
							}
						}
						else
						{
							f32 size = (m_Vehicle->GetBoundingBoxMax().x - m_Vehicle->GetBoundingBoxMin().x)
								* (m_Vehicle->GetBoundingBoxMax().y - m_Vehicle->GetBoundingBoxMin().y);
							if ( (size < sm_SizeThreshold) || m_JustCameOutOfWater)
							{
								// trigger small splash.
								soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("SMALL", 0xB7F05D20) : ATSTRINGHASH("UNDERWATER_SMALL", 0xCB87A7DC) );
							}
							else
							{
								if ((velocity * downSpeed < sm_CombineSpeedThreshold) && size < sm_BigSizeThreshold)
								{
									soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("MEDIUM", 0x74E06FAC) : ATSTRINGHASH("UNDERWATER_MEDIUM", 0x9B393786) );
								}
								else 
								{
									soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("HEAVY", 0xB1AEBE06) : ATSTRINGHASH("UNDERWATER_HEAVY", 0xC6055D14) );
								}
							}

							//naDisplayf("[Velocity %f] [downSpeed %f] [combined %f] [size %f] [mass %f] [volume %f] ",velocity,downSpeed,velocity*downSpeed,size,mass,volume);
						}
						m_JustCameOutOfWater = false;
					}
					else if( velocity > sm_FwdSpeedThreshold  )
					{
						if(m_Vehicle && m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
						{
							soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("BICYCLE_SMALL", 0x2DB76DC1) : ATSTRINGHASH("BICYCLE_SMALL_UNDERWATER", 0xA1C9F0DE) );
						}
						else
						{
							soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("SHALLOW", 0x8448CEF5) : ATSTRINGHASH("UNDERWATER_SHALLOW", 0x34F89C97) );
						}
					}
				}
				else
				{
					if(m_Vehicle && m_Vehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
					{
						soundRef= sm_VehiclesInWaterSoundset.Find(!Water::IsCameraUnderwater() ? ATSTRINGHASH("BICYCLE_SMALL", 0x2DB76DC1) : ATSTRINGHASH("BICYCLE_SMALL_UNDERWATER", 0xA1C9F0DE) );
					}
					else
					{
						soundRef= sm_ExtrasSoundSet.Find(ATSTRINGHASH("VehicleComingOutOfWater", 0xC618DCC4));
					}
					m_JustCameOutOfWater = true;
				}
				m_LastSplashTime = fwTimer::GetTimeInMilliseconds();
				CreateAndPlaySound(soundRef, &initParams);
#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketVehicleSplashPacket>(
						CPacketVehicleSplashPacket(downSpeed, outOfWater), GetOwningEntity());
				}
#endif
			}
		}
	}
}
void audVehicleAudioEntity::TriggerHeliRotorSplash()
{
	if(!IsDisabled())
	{

		if(fwTimer::GetTimeInMilliseconds()>m_LastHeliRotorSplashTime+500)
		{
			audSoundInitParams initParams;

			m_LastHeliRotorSplashTime = fwTimer::GetTimeInMilliseconds();
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.Position = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
			CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("HeliRotorHittingWater", 0xC7147B73)), &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_ExtrasSoundSet.GetNameHash(), ATSTRINGHASH("HeliRotorHittingWater", 0xC7147B73), &initParams, GetOwningEntity()));
		}
	}
}
// ----------------------------------------------------------------
// This function supports dynamic sound creation from collisions
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerDynamicImpactSounds(f32 volume)
{
	if(!IsDisabled())
	{
		u32 soundhash = 0;
		audSoundInitParams initParams;
		if(volume <= g_LowerVehDynamicImpactThreshold)
		{
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			soundhash = ATSTRINGHASH("VEH_DYNAMIC_IMPACTS_LOW", 0x2AA0D9E0);
		}
		else if(volume <= g_UpperVehDynamicImpactThreshold)
		{
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			soundhash = ATSTRINGHASH("VEH_DYNAMIC_IMPACTS_MEDIUM", 0x5BC85E0D);
		}
		else
		{
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			soundhash = ATSTRINGHASH("VEH_DYNAMIC_IMPACTS_HIGH", 0xC6E79F96);
		}

		CreateAndPlaySound(soundhash, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundhash, &initParams, GetOwningEntity()));
	}
}

// ----------------------------------------------------------------
// Trigger a towing hook attach sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerTowingHookAttachSound()
{
	if(!IsDisabled())
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;	
		CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("TowTruck_HookAttach", 0xA8A8FF30)), &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_ExtrasSoundSet.GetNameHash(), ATSTRINGHASH("TowTruck_HookAttach", 0xA8A8FF30), &initParams, GetOwningEntity()));
	}
}

// ----------------------------------------------------------------
// Trigger a towing hook detach sound
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerTowingHookDetachSound()
{
	if(!IsDisabled())
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;	
		CreateAndPlaySound(sm_ExtrasSoundSet.Find(ATSTRINGHASH("TowTruck_HookRelease", 0xA10E2691)), &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_ExtrasSoundSet.GetNameHash(), ATSTRINGHASH("TowTruck_HookRelease", 0xA10E2691), &initParams, GetOwningEntity()));
	}
}

// ----------------------------------------------------------------
// Get any trailer attached to this vehicle
// ----------------------------------------------------------------
audTrailerAudioEntity* audVehicleAudioEntity::GetTrailer()
{
	for(int i = 0; i < m_Vehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget *pVehicleGadget = m_Vehicle->GetVehicleGadget(i);

		if(pVehicleGadget->GetType() == VGT_TRAILER_ATTACH_POINT)
		{
			CVehicleTrailerAttachPoint *pTrailerAttachPoint = static_cast<CVehicleTrailerAttachPoint*>(pVehicleGadget);
			CTrailer *pTrailer = pTrailerAttachPoint->GetAttachedTrailer(m_Vehicle);

			if(pTrailer && pTrailer->GetVehicleAudioEntity() && 
				pTrailer->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAILER)
			{
				return ((audTrailerAudioEntity*)pTrailer->GetVehicleAudioEntity());
			}
		}
	}

	return NULL;
}

// ----------------------------------------------------------------
// This function supports dynamic sound creation from explosions
// ----------------------------------------------------------------
void audVehicleAudioEntity::TriggerDynamicExplosionSounds(bool isUnderwater)
{
	VehicleExploded();

	m_LastTimeExploded = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	audSoundInitParams initParams;
	BANK_ONLY(	initParams.IsAuditioning = g_AuditionVehExplosions; );
	if(m_VehicleType == AUD_VEHICLE_BICYCLE)
	{
		return;
	}
	if(!isUnderwater)
	{
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
		CreateAndPlaySound(ATSTRINGHASH("VEH_DYNAMIC_EXPLOSION", 0xED136850), &initParams);
	}
	else 
	{
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_Vehicle->GetAudioEnvironmentGroup();
		CreateAndPlaySound(ATSTRINGHASH("VEH_DYNAMIC_EXPLOSION_UNDERWATER", 0xDB101148), &initParams);
	}
}

// ----------------------------------------------------------------
// Get the wave slot
// ----------------------------------------------------------------
audWaveSlot* audVehicleAudioEntity::GetWaveSlot()
{
	if(m_EngineWaveSlot)
	{
		return m_EngineWaveSlot->waveSlot;
	}
	else
	{
		return NULL;
	}
}

// ----------------------------------------------------------------
// Compute proximity ratio to exhaust
// ----------------------------------------------------------------
f32 audVehicleAudioEntity::ComputeProximityEffectRatio()
{
	const Vec3V camPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(0);
	const Vec3V exhaustPos = m_Vehicle->TransformIntoWorldSpace(m_ExhaustOffsetPos);
	const Vec3V camToCar = (exhaustPos-camPos);
	f32 factor = Min((MagSquared(camToCar)/ScalarV(g_ExhaustProximityEffectRange*g_ExhaustProximityEffectRange)).Getf(), 1.0f);
	if(fwTimer::GetTimeWarp() < 0.9f)
	{
		// disable proximity effect when in slowmo as it causes distortion
		factor = 1.f;
	}

	return 1.0f-factor;
}

// ----------------------------------------------------------------
// Update loop with linear volume/pitch
// ----------------------------------------------------------------
void audVehicleAudioEntity::UpdateLoopWithLinearVolumeAndPitchRatio(audSound **sound, const u32 soundHash, f32 linVolume, f32 pitchRatio, audEnvironmentGroupInterface *occlusionGroup, audCategory *category, bool stopWhenSilent, bool smoothPitch, f32 environmentalLoudness, bool trackPosition)
{
	s32 pitch = audDriverUtil::ConvertRatioToPitch(pitchRatio);
	UpdateLoopWithLinearVolumeAndPitch(sound, soundHash, linVolume, pitch, occlusionGroup, category, stopWhenSilent, smoothPitch, environmentalLoudness, trackPosition);
}

// ----------------------------------------------------------------
// Force this vehicle to use a particular game object
// ----------------------------------------------------------------
void audVehicleAudioEntity::ForceUseGameObject(u32 gameObject)
{
	m_ForcedGameObject = gameObject;
	m_ForcedGameObjectResetRequired = true;
}

//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::StopHorn(bool checkForCarModShop, bool onlyStopIfPlayer)
{
	if(!checkForCarModShop || (checkForCarModShop && (!m_InCarModShop || m_Vehicle->UsesSiren())))
	{
		if(!onlyStopIfPlayer || (onlyStopIfPlayer && m_PlayerHornOn))
		{
			if(m_HornSound)
			{
				m_HornSound->StopAndForget(true);
#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketVehicleHornStop>(
						CPacketVehicleHornStop(),
						m_Vehicle);
				}
#endif // GTA_REPLAY
			}
			m_PlayerHornOn = false;
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::UpdateBulletHits()
{
	if(!m_Vehicle->GetDriver() || (m_Vehicle->GetDriver() && !m_Vehicle->GetDriver()->IsPlayer()))
	{
		m_BulletHitCount ++;
		m_TimeSinceLastBulletHit = 0;
	}
}
#if __BANK
bool audVehicleAudioEntity::ShouldRenderRecording(const s32 index)
{
	bool shouldRenderRecording = true;
	if(sm_EnableCREditor)
	{
		CarRecordingAudioSettings *CRSettings = NULL;
		CRSettings = audNorthAudioEngine::GetObject<CarRecordingAudioSettings>(CVehicleRecordingMgr::GetRecordingHash(index));

		if(CRSettings)
		{
			if(g_CRFilter[0] != 0 && !audAmbientAudioEntity::MatchName(audNorthAudioEngine::GetMetadataManager().GetObjectName(CRSettings->NameTableOffset,0),g_CRFilter))
			{
				shouldRenderRecording = false;
			}
		}
	}
	return shouldRenderRecording;
}

//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::SkipTo()
{
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh)
	{
		if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
		{
			s32 playBackSlot = CVehicleRecordingMgr::GetPlaybackSlot(pVeh);
			sm_ManualTimeSet = false;	
			f32 desireTime = (sm_CRCurrentTime - CVehicleRecordingMgr::GetPlaybackRunningTime(playBackSlot));
			CVehicleRecordingMgr::SkipTimeForwardInRecording(pVeh,desireTime);
			for(u32 i = 0; i<sm_DebugCarRecording.eventList.GetCount();i++)
			{
				if(sm_DebugCarRecording.eventList[i].time > sm_CRCurrentTime)
				{
					sm_DebugCarRecording.eventList[i].processed = false;
				}
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::FwdCR()
{
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh)
	{
		if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
		{
			sm_CRCurrentTime += sm_CRJumpTimeInMs;
			audVehicleAudioEntity::SkipTo();
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::RewindCR()
{
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh)
	{
		if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
		{
			sm_CRCurrentTime -= sm_CRJumpTimeInMs;
			audVehicleAudioEntity::SkipTo();
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::UpdateProgressBar() 
{	
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh)
	{
		if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
		{
			s32 index = CVehicleRecordingMgr::GetPlaybackSlot(pVeh);
			if(sm_EnableCREditor)
			{
				if(sm_ManualTimeSet)
				{	
					sm_CRProgress =  (f32)(sm_CRCurrentTime / (f32) CVehicleRecordingMgr::GetEndTimeOfRecording(index)) * 100.f;
				}
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::UpdateTimeBar() 
{	
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh)
	{
		if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
		{
			s32 index = CVehicleRecordingMgr::GetPlaybackSlot(pVeh);
			if(sm_EnableCREditor)
			{
				if(sm_ManualTimeSet)
				{	
					sm_CRCurrentTime = (f32) CVehicleRecordingMgr::GetEndTimeOfRecording(index) * 0.01f * sm_CRProgress;
				}
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::LoadCRAudioSettings()
{
	if(sm_EnableCREditor && !sm_UpdateCRTime)
	{
		sm_UpdateCRTime = true;
		if(naVerifyf(strlen(sm_CRAudioSettingsCreator) != 0 ,"Please in order to enable the Car Recording audio editor, select one."))
		{		
			CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
			if(pVeh)
			{
				if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
				{

					bkBank* bank = BANKMGR.FindBank("Audio");
					if( sm_CRWidgetGroup )
					{
						bank->DeleteGroup(*sm_CRWidgetGroup);
						sm_CRWidgetGroup = NULL;
					}
					bank->SetCurrentGroup(*CVehicleRecordingMgr::sm_WidgetGroup);
					sm_CRWidgetGroup = bank->PushGroup("Current recording editor",true);
						s32 index = CVehicleRecordingMgr::GetPlaybackSlot(pVeh);
						bank->AddToggle("Manual time/progress set up ",&sm_ManualTimeSet);
						bank->AddSlider("Current time",&sm_CRCurrentTime,0.f,(f32) CVehicleRecordingMgr::GetEndTimeOfRecording(index),0.1f,CFA(UpdateProgressBar));
						bank->AddSlider("Progress bar",&sm_CRProgress,0.f,100.f,0.1f,CFA(UpdateTimeBar));
						bank->AddButton("Skip to selected time/progress", CFA(SkipTo));
						bank->AddSlider("Jump time in ms",&sm_CRJumpTimeInMs,0.f,10000.f,0.1f);
						bank->AddButton("Forward", CFA(FwdCR));
						bank->AddButton("Rewind", CFA(RewindCR));
					bank->PopGroup();
					bank->UnSetCurrentGroup(*CVehicleRecordingMgr::sm_WidgetGroup);
				}
			}
		}
	}
	else if (sm_EnableCREditor)
	{
		for(u32 i = 0; i<sm_DebugCarRecording.eventList.GetCount();i++)
		{
			sm_DebugCarRecording.eventList[i].processed = false;
		}
	}
}
void audVehicleAudioEntity::CancelCRAudioSettings()
{
	formatf(sm_CRAudioSettingsCreator,sizeof(sm_CRAudioSettingsCreator),"");
	formatf(CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith,sizeof(CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith),"");
	CVehicleRecordingMgr::ms_debug_RecordingNumToPlayWith  = 0;					
	sm_DebugCarRecording.nameHash.Clear();
	sm_DebugCarRecording.eventList.Reset(); 
	sm_DebugCarRecording.settings = NULL;
	sm_DebugCarRecording.modelId = 0;
	sm_CRCurrentTime = 0.f;
	sm_CRProgress = 0.f;
	sm_ManualTimeSet = false;
	sm_DrawCREvents = false;
	sm_DrawCRName = false;
	sm_LoadExistingCR = false;
	sm_UpdateCRTime = false;
	sm_VehCreated = false;
	g_pFocusEntity = NULL;
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if(pVeh)
	{
		if(CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pVeh))
		{
			CVehicleRecordingMgr::StopPlaybackRecordedCar(pVeh);
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::CreateAudioSettings() 
{
	if(sm_DebugCarRecording.nameHash.IsNotNull())
	{
		char xmlMsg[8192];
		FormatCRAudioSettingsXml(xmlMsg,sizeof(xmlMsg), sm_DebugCarRecording.nameHash.GetCStr());
		naAssertf(g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg)), "Failed to send xml message to rave");
		naAssertf(g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg)), "Failed to send xml message to rave");
	}
}
void audVehicleAudioEntity::CreateCREvent() 
{
	if(sm_DebugCarRecording.nameHash.IsNotNull())
	{
		audioRecordingEvent event;
		event.time = sm_CRCurrentTime;
		event.hashEvent = g_NullSoundHash;
		event.processed = false;
		//Add the event in order.
		atArray<audioRecordingEvent> eventListCached;
		eventListCached.Reset();
		u32 i = 0; 
		if(sm_DebugCarRecording.eventList.GetCount())
		{
			bool added = false;
			while (i <  sm_DebugCarRecording.eventList.GetCount())
			{
				if(added || sm_DebugCarRecording.eventList[i].time <= event.time)
				{
					eventListCached.PushAndGrow(sm_DebugCarRecording.eventList[i]);
					i++;
				}
				else if(!added)
				{
					eventListCached.PushAndGrow(event);
					added = true;
				}
			}
			if(!added)
			{
				eventListCached.PushAndGrow(event);
			}
		}
		else
		{
			eventListCached.PushAndGrow(event);
		}
		sm_DebugCarRecording.eventList.Reset();
		for (u32 i = 0; i < eventListCached.GetCount();i++)
		{
			sm_DebugCarRecording.eventList.PushAndGrow(eventListCached[i]);
		}
		char xmlMsg[8192];
		FormatCRAudioSettingsXml(xmlMsg,sizeof(xmlMsg), sm_DebugCarRecording.nameHash.GetCStr());
		naAssertf(g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg)), "Failed to send xml message to rave");
		naAssertf(g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg)), "Failed to send xml message to rave");
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audVehicleAudioEntity::FormatCRAudioSettingsXml(char * xmlMsg, const size_t bufferSize,const char * settingsName)
{
	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	formatf(xmlMsg,bufferSize, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};
	//CarRecordingAudioSettings
	formatf(tmpBuf, "<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", ATSTRINGHASH("BASE", 0x44E21C90));
	safecat(xmlMsg, tmpBuf,bufferSize);
	formatf(tmpBuf, "<CarRecordingAudioSettings name=\"%s\">\n", &upperSettings[0]);
	safecat(xmlMsg, tmpBuf,bufferSize);
	// Keep current events
	for (s32 i = 0; i< sm_DebugCarRecording.eventList.GetCount(); i++)
	{
		formatf(tmpBuf, "<Event>\n");
		safecat(xmlMsg, tmpBuf,bufferSize);
		formatf(tmpBuf, "<Time>%f</Time>\n", sm_DebugCarRecording.eventList[i].time);
		safecat(xmlMsg, tmpBuf,bufferSize);
		formatf(tmpBuf, "<Sound>%s</Sound>\n", SOUNDFACTORY.GetMetadataManager().GetObjectName(sm_DebugCarRecording.eventList[i].hashEvent));
		safecat(xmlMsg, tmpBuf,bufferSize);
		formatf(tmpBuf, "</Event>\n");
		safecat(xmlMsg, tmpBuf,bufferSize);
	}
	formatf(tmpBuf, "<VehicleModelId>%u</VehicleModelId>\n", sm_DebugCarRecording.modelId);
	safecat(xmlMsg, tmpBuf,bufferSize);
	formatf(tmpBuf, "<Description/>\n");
	safecat(xmlMsg, tmpBuf,bufferSize);
	formatf(tmpBuf, "<VehicleModel/>\n");
	safecat(xmlMsg, tmpBuf,bufferSize);
	formatf(tmpBuf, "</CarRecordingAudioSettings>\n");
	safecat(xmlMsg, tmpBuf,bufferSize);
	formatf(tmpBuf, "</EditObjects>\n");
	safecat(xmlMsg, tmpBuf,bufferSize);
	formatf(tmpBuf, "</RAVEMessage>\n");
	safecat(xmlMsg, tmpBuf,bufferSize);
}

#if __BANK
// ----------------------------------------------------------------
// Draw Bounding Box
// ----------------------------------------------------------------
void audVehicleAudioEntity::DrawBoundingBox(const Color32 color)
{
	const Vector3 & vMin = m_Vehicle->GetBoundingBoxMin();
	const Vector3 & vMax = m_Vehicle->GetBoundingBoxMax();
	CDebugScene::Draw3DBoundingBox(vMin, vMax, MAT34V_TO_MATRIX34(m_Vehicle->GetMatrix()), color);
}
#endif

// ----------------------------------------------------------------
// Add widgets
// ----------------------------------------------------------------
void LoadExistingCR()
{
	if(naVerifyf(strlen(audVehicleAudioEntity::sm_CRAudioSettingsCreator) != 0 ,"Please in order to load a Car Recording , write the name of the object in the 'Current Car Recording' field"))
	{
		audVehicleAudioEntity::sm_DebugCarRecording.settings = audNorthAudioEngine::GetObject<CarRecordingAudioSettings>(audVehicleAudioEntity::sm_CRAudioSettingsCreator);
		if(naVerifyf(audVehicleAudioEntity::sm_DebugCarRecording.settings, "there is no existing settings for %s",audVehicleAudioEntity::sm_CRAudioSettingsCreator))
		{
			audVehicleAudioEntity::sm_LoadExistingCR = true;
			audVehicleAudioEntity::sm_VehCreated = false;
		}
	}
}
void CreatePlaybackVehicleCB()
{
	CVehicleFactory::CreateCar(audVehicleAudioEntity::GetPlayBackVehModelIndex());
}
void CreateDefaultPlaybackVehicleCB()
{
	CVehicleFactory::CreateCar(audVehicleAudioEntity::GetDefaultPlayBackVehModelIndex());
}
void WarpPlayerToPlaybackVehicleCB()
{
	CVehicleFactory::WarpPlayerOutOfCar();
	CVehicleFactory::WarpPlayerIntoRecentCar();
}

bool audVehicleAudioEntity::AddCarRecordingWidgets(bkBank *bank)
{
	bank->AddSeparator();
	sm_EnableCREditor = true;
	bank->SetCurrentGroup(*CVehicleRecordingMgr::sm_WidgetGroup);
	bank->AddText("Current Car Recording Audio GO", sm_CRAudioSettingsCreator, sizeof(sm_CRAudioSettingsCreator), false);
	bank->AddButton("Load existing CR", CFA(LoadExistingCR));
	bank->AddButton("Create playback veh", CFA(CreatePlaybackVehicleCB));
	bank->AddButton("Create default playback veh (tailgater) ", CFA(CreateDefaultPlaybackVehicleCB));
	bank->AddButton("Warp player to  playback veh", CFA(WarpPlayerToPlaybackVehicleCB));
	bank->AddToggle("Draw events",&sm_DrawCREvents);
	bank->AddToggle("Draw recording name",&sm_DrawCRName);
	bank->AddText("Filter by Name", g_CRFilter, sizeof(g_CRFilter));
	bank->AddToggle("Pause Game",&sm_PauseGame);
	bank->AddButton("Create audio settings", CFA(CreateAudioSettings));
	bank->AddButton("Create current event", CFA(CreateCREvent));
	bank->AddButton("Finish current work ", CFA(CancelCRAudioSettings));
	bank->AddButton("Cancel ", CFA(CancelCRAudioSettings));
	bank->UnSetCurrentGroup(*CVehicleRecordingMgr::sm_WidgetGroup);
	return true;
}
void audVehicleAudioEntity::AddWidgets(bkBank &bank)
{
	bank.AddToggle("DebugVehicleSkids", &g_DebugVehicleSkids);
	bank.AddToggle("Understeer Affects Skid Vol", &g_UndersteerAffectsMainSkidVol);
	bank.AddToggle("Show cars playing rain loops", &g_ShowCarsPlayingRainLoops);
	bank.AddSlider("Override Skid Pitch", &g_OverridenSkidPitchOffset, -24000, 24000, 100);
	bank.AddToggle("g_AuditionVehExplosions",&g_AuditionVehExplosions);
	bank.AddSlider("Delta fwd speed for collision", &audVehicleAudioEntity::sm_VehDeltaSpeedForCollision, 0.f, 100.f, 0.1f);
	bank.AddSlider("Time to play player's veh siren",&g_TimeToPlaySiren,0,5000,10 );
	bank.AddSlider("sm_AlarmIntervalInMs",&sm_AlarmIntervalInMs,0,5000,10 );	
	bank.AddSlider("Mod Shop Door Recently Used Time",&g_ModShopDoorRecentlyUsedTime,0,5000,10 );	
	
	bank.PushGroup("Submarine Dive Horn", false);	
		bank.AddToggle("Debug Draw", &g_DebugSubmarineDiveHorn);
		bank.AddToggle("Trigger", &g_ForceTriggerSubmarineDiveHorn);		
		bank.AddSlider("Min Dive Button Hold Time", &g_MinSubDiveHoldTime, 0.f, 10.0f, 0.01f);
		bank.AddSlider("Submerge Threshold", &g_SubDiveHornSubmergeThreshold, 0.f, 1.0f, 0.01f);
		bank.AddSlider("Submerge Retrigger Threshold", &g_SubDiveHornSubmergeRetriggerThreshold, 0.f, 1.0f, 0.01f);
		bank.AddSlider("Submerge Retrigger (No Driver)", &g_SubDiveHornSubmergeRetriggerThresholdNoDriver, 0.f, 1.0f, 0.01f);		
		bank.AddSlider("Sub Dive Velocity", &g_SubDiveHornVelocity, 0.f, 100.0f, 0.01f);
	bank.PopGroup();
		
	bank.PushGroup("Sirens", false);
		bank.AddToggle("Enable Player Stereo Sirens", &g_EnableStereoSirens);
		bank.AddToggle("Smash Player Siren", &g_SmashPlayerSiren);
		bank.AddToggle("Fix Player Siren", &g_FixPlayerSiren);
		bank.AddSlider("C&C Siren Smash Damage", &g_CopsAndCrooksForceSirenSmashDamage, 0.f, 1.0f, 0.01f);		
		bank.AddSlider("C&C Siren Smash Probability", &g_CopsAndCrooksForceSirenSmashProbability, 0.f, 1.0f, 0.01f);				
	bank.PopGroup();
	bank.PushGroup("Missile Lock", false);
		bank.AddToggle("Debug Missile Status", &g_DebugMissileLockStatus);
		bank.AddToggle("Override Missile Status", &g_OverrideMissileStatus);
		bank.AddCombo("Missile Status", &g_SimulatedMissileState, 5, &g_MissileLockOnStates[0], 0, NullCB, "" );
		bank.AddSlider("Simulated Missile Distance", &g_SimulatedMissileDistance, 0.0f, 1.0f, 0.01f);
	bank.PopGroup();
	bank.PushGroup("Vehicle High Speed Effect", false);
		bank.AddToggle("Debug High Speed Effect", &g_DebugHighSpeedEffect);
		bank.AddSlider("Minimum Vehicle Top Speed", &audVehicleAudioEntity::sm_HighSpeedReqVehicleVelocity, 0.0f, 100.f, 1.0f);
		bank.AddSlider("Num Valid Gears (below top gear)", &audVehicleAudioEntity::sm_HighSpeedMaxGearOffset, 0, 10, 1);
		bank.AddSlider("Min Forward Speed Ratio", &audVehicleAudioEntity::sm_HighSpeedApplyStart, 0.0f, 1.0, 0.01f);
		bank.AddSlider("Max Forward Speed Ratio", &audVehicleAudioEntity::sm_HighSpeedApplyEnd, 0.0f, 1.0, 0.01f);
		bank.AddSlider("1/Smoother Increase Rate", &audVehicleAudioEntity::sm_InvHighSpeedSmootherIncreaseRate, 0, 100000000, 10);
		bank.AddSlider("1/Smoother Decrease Rate", &audVehicleAudioEntity::sm_InvHighSpeedSmootherDecreaseRate, 0, 100000000, 10);
	bank.PopGroup();
	bank.PushGroup("Vehicle water splashes");
	bank.AddSlider("sm_DownSpeedThreshold", &sm_DownSpeedThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("sm_FwdSpeedThreshold", &sm_FwdSpeedThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("sm_SizeThreshold", &sm_SizeThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("sm_BigSizeThreshold", &sm_BigSizeThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("sm_CombineSpeedThreshold", &sm_CombineSpeedThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("sm_BicycleCombineSpeedMediumThreshold", &sm_BicycleCombineSpeedMediumThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("sm_BicycleCombineSpeedBigThreshold", &sm_BicycleCombineSpeedBigThreshold, 0.f, 1000.f,0.1f);
	bank.AddSlider("g_DistThresholdToTriggerVehSplashes", &g_DistThresholdToTriggerVehSplashes,0.f, 10000.f,0.1f);
	bank.AddSlider("g_SqdDistThresholdForWaveImpact", &g_SqdDistThresholdForWaveImpact,0.f, 10000.f,0.1f);
	bank.PopGroup();

	bank.PushGroup("Amphibious Vehicles");
		bank.AddToggle("Force Amphibious Boat Mode", &g_ForceAmphibiousBoatMode);	
	bank.PopGroup();

	bank.PushGroup("Radio occlusion");
		bank.AddToggle("Debug Draw Radio Occlusion", &g_DebugDrawFocusVehicleRadioOcclusion);
		bank.AddToggle("Force Upgrade Radio", &g_ForceUpgradeRadio);		
		bank.AddToggle("Positioned Player Vehicle Radio Enabled", &g_PositionedPlayerVehicleRadioEnabled);
		bank.AddToggle("Override Focus Vehicle Openness", &g_OverrideFocusVehicleOpenness);
		bank.AddSlider("Overridden Openness", &g_OverriddenVehicleOpenness, 0.f, 1.f, 0.001f);
		bank.AddSlider("Modded Radio Min Openness", &g_ModdedRadioVehicleOpenness, 0.f, 1.f, 0.001f);		
		const char *leakageNames[NUM_AMBIENTRADIOLEAKAGE];
		for(s32 l = LEAKAGE_BASSY_LOUD; l < NUM_AMBIENTRADIOLEAKAGE; l++)
		{
			leakageNames[l] = AmbientRadioLeakage_ToString((AmbientRadioLeakage)l);
		}
		bank.AddToggle("Override Focus Vehicle Leakage", &g_OverrideFocusVehicleLeakage);
		bank.AddCombo("Overridden Vehicle Leakage", (s32*)&g_OverriddenVehicleRadioLeakage, NUM_AMBIENTRADIOLEAKAGE, leakageNames);
		bank.AddToggle("Override Focus Vehicle Leakage Params", &g_OverrideFocusVehicleLeakageParams);
		bank.AddToggle("Copy Overriden Leakage Params from Selected Settings", &g_CopyOverridenLeakageFromCurrentSettings);
		bank.AddSlider("Volume Min", &g_OverridenLeakageVolMin, 0.f, 20.f, 0.001f);
		bank.AddSlider("Volume Max", &g_OverridenLeakageVolMax, 0.f, 20.f, 0.001f);
		bank.AddSlider("Lpf Cutoff Linear Min", &g_OverridenLeakageLpfLinearMin, 0.f, 1.f, 0.001f);
		bank.AddSlider("Lpf Cutoff Linear Max", &g_OverridenLeakageLpfLinearMax, 0.f, 1.f, 0.001f);
		bank.AddSlider("Rolloff Min", &g_OverridenLeakageRolloffMin, 0.f, 200.f, 0.1f);
		bank.AddSlider("Rolloff Max", &g_OverridenLeakageRolloffMax, 0.f, 200.f, 0.1f);
		bank.AddSlider("Hpf Cutoff", &g_OverridenLeakageHPFCutoff, 0, kVoiceFilterHPFMaxCutoff, 1);
		bank.AddSlider("Max Radius", &g_OverridenLeakageMaxRadius, 0, 10000, 100);
	bank.PopGroup();
}
#endif
