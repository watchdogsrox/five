// 
// audio/gtaaudioenvironment.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//  

#include "audio/ambience/ambientaudioentity.h"
#include "audio/cutsceneaudioentity.h"
#include "audio/debugaudio.h"
#include "audio/environment/environment.h"
#include "audio/environment/environmentgroup.h"
#include "audio/frontendaudioentity.h"
#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "audio/radiostation.h"
#include "audio/scannermanager.h"
#include "audio/scriptaudioentity.h"
#include "audio/weatheraudioentity.h"
#include "audioeffecttypes/biquadfiltereffect.h"
#include "audioeffecttypes/delayeffect.h"
#include "audioeffecttypes/reverbeffect.h"
#include "audiosoundtypes/sounddefs.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/environment.h"
#include "audiohardware/driverutil.h"
#include "audiohardware/submix.h"
#include "audwarpmanager.h"
#include "camera/CamInterface.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debugglobals.h"
#include "debug/DebugScene.h"
#include "grcore/debugdraw.h"
#include "game/weather.h"
#include "game/zones.h"
#include "math/amath.h"
#include "modelinfo/MloModelInfo.h"
#include "objects/door.h"
#include "peds/PedIntelligence.h"
#include "peds/ped.h"
#include "peds/PopCycle.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalInst.h"
#include "scene/WarpManager.h"
#include "scene/world/GameWorldWaterHeight.h"
#include "fwscene/search/Search.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwsys/timer.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "vector/geometry.h"
#include "vfx/Systems/VfxWeather.h"
#include "fwscene/world/WorldLimits.h"
#include "scene/world/GameWorldHeightMap.h"
#include "vectormath/vectormath.h"
#include "vehicleAi/pathfind.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()

// game-thread, gta-specific
f32 g_NumTreesForRain = 25.f;
f32 g_NumTreesForRainBuiltUp = 200.f;
f32 g_OverrideBuiltUpFactor = 0.5f;
f32 g_DeafeningEarLeak = 0.2f;
f32 g_DeafeningRolloff = 1.5f;
f32 g_MassiveDeafeningVolume = 16.0f;
f32 g_MinHeightForTree = 3.0f;
f32 g_MinBuildingHeight = 5.0f;
f32 g_MinBuildingFootprintArea = 1000.0f;
f32 g_MaxBuildingFootprintArea = 10000.0f;
f32 g_SourceEnvironmentRadius = 20.0f;
f32 g_OpenSpaceReverbSize = 0.3f;
f32 g_OpenSpaceReverbWet = 0.08f;
f32 g_SpeakerReverbSmootherRate = 0.001f;
f32 g_SpeakerReverbDamping = 0.525f;
f32 g_FitnessReduced = 0.01f;
f32 g_VolSmootherRate = 0.001f;
u32 g_DeafeningRampUpTime = 1500;
u32 g_DeafeningHoldTime = 1500;
u32 g_DeafeningRampDownTime = 4500;
u32 g_SpecialAbilitySlowMoTimeout = 250;

bool g_DebugListenerReverb = false;
bool g_ShouldOverrideBuiltUpFactor = false;

bool g_UseInteriorCarFilter = false;

bool g_EnableLocalEnvironmentMetrics = true;

u16 naEnvironment::sm_ObjOcclusionUpdateTime = 500;

bool naEnvironment::sm_ProcessSectors = false;

audCurve naEnvironment::sm_HDotToSlowmoResoVol;
audCurve naEnvironment::sm_VDotToSlowmoResoVol;
audCurve naEnvironment::sm_SpecialAbToFrequency;
audCurve naEnvironment::sm_CityGustDotToVolSpikeForBgTrees;	
audCurve naEnvironment::sm_CountrySideGustDotToVolSpikeForBgTrees;	

f32 naEnvironment::sm_SlowMoLFOFrequencyMin = 0.5f; 
f32 naEnvironment::sm_SlowMoLFOFrequencyMax = 2.f; 

f32 naEnvironment::sm_RadiusScale = 2.5f;
f32 naEnvironment::sm_RainOnPropsVolumeOffset = 0.f; 
u32 naEnvironment::sm_RainOnPopsPreDealy = 0; 
u32 naEnvironment::sm_MaxNumRainProps = 5; 
u32 naEnvironment::sm_MinWindObjTime = 15000; 
u32 naEnvironment::sm_MaxWindObjtTime = 25000; 
u32 naEnvironment::sm_WindTimeVariance = 10000; 
u32 naEnvironment::sm_MinTimeToRetrigger = 25000; 

u32 naEnvironment::sm_MinNumberOfTreesToTriggerGustEnd = 5;

#if __BANK
bool g_DrawWeaponEchoes = false;

bool g_audDrawLocalObjects = false;
bool g_audDrawShockwaves = false;
bool g_ForceResonanceTracksListener = false;
bool g_ForceRainLoopTracksListener = false;
bool g_ForceRainLoopPlayFromTopOfBounds = false;

f32 naEnvironment::sm_NumWaterProbeSets = 10.f;
bool naEnvironment::sm_UseCoverAproximation = false;
bool naEnvironment::sm_DebugCoveredObjects = false;
bool naEnvironment::sm_DebugGustRustle = false;
bool naEnvironment::sm_DebugGustWhistle = false;
bool naEnvironment::sm_DrawAudioWorldSectors = false; 
bool naEnvironment::sm_IsWaitingToInit = true;
bool naEnvironment::sm_LogicStarted = false;
bool naEnvironment::sm_DisplayResoIntensity = false;
bool naEnvironment::sm_DrawBgTreesInfo = false;
bool naEnvironment::sm_MuteBgFL = false;
bool naEnvironment::sm_MuteBgFR = false;
bool naEnvironment::sm_MuteBgRL = false;
bool naEnvironment::sm_MuteBgRR = false;
u32 naEnvironment::sm_CurrentSectorX = 0;
u32 naEnvironment::sm_CurrentSectorY = 0;
#endif
bank_bool g_UpdateBuiltUpFactor = false;

bool naEnvironment::ms_localObjectSearchLaunched = false;
fwSearch naEnvironment::ms_localObjectSearch(1900);

f32 naEnvironment::sm_HeliShockwaveRadius = 30.f;
f32 naEnvironment::sm_TrainDistanceThreshold = 300.f;
f32 naEnvironment::sm_TrainShockwaveRadius = 30.f;

f32 naEnvironment::sm_MeleeResoImpulse = 0.3f;
f32 naEnvironment::sm_PistolResoImpulse = 0.2f;
f32 naEnvironment::sm_RifleResoImpulse = 0.35f;
f32 naEnvironment::sm_WeaponsResonanceDistance = 15.f;
f32 naEnvironment::sm_ExplosionsResonanceImpulse = 0.2f;
f32 naEnvironment::sm_ExplosionsResonanceDistance = 100.f;
f32 naEnvironment::sm_VehicleCollisionResonanceImpulse = 0.05f;
f32 naEnvironment::sm_VehicleCollisionResonanceDistance = 5.f;
f32 naEnvironment::sm_DistancePlateau = 3.f;
// Echo metrics
f32 g_MaxEchoLength = 40.0f;
f32 g_MinEchoLength = 15.0f;
// BuiltUp stuff
f32 g_BuiltUpSmootherRate = 1.0f;
bool g_DebugBuiltUpInfo = false;
bool g_DisplayBuiltUpFactors = false;
bool g_UseProperBuiltUpFactor = true;

f32 g_NSEW_Offset = 0.49f;
f32 g_BuiltUpDampingFactorSmoothUpRate = 1.0f;
f32 g_BuiltUpDampingFactorSmoothDownRate = 1.0f;
f32 g_BuiltUpDampingFactorOverride = 1.0f;

f32 g_InCarRatioSmoothRate = 1.0f;
f32 g_InteriorRatioSmoothRate = 1.0f;

f32 g_windWhistleTreshold = 3.0f;

f32 naEnvironment::sm_SqdDistanceToPlayRainSounds = 225.f;
f32 naEnvironment::sm_BushesMinRadius = 1.5f; 
f32 naEnvironment::sm_SmallTreesMinRadius = 4.5f;
f32 naEnvironment::sm_BigTreesMinRadius = 10.f; 
f32 naEnvironment::sm_TreesSoundDist = 30.f; 
f32 naEnvironment::sm_BigTreesFitness = 1.f;
f32 naEnvironment::sm_TreesFitness = 0.6f;
f32 naEnvironment::sm_BushesFitness = 0.3f;
u8 naEnvironment::sm_NumTreesForAvgDist = 5;

#if 1 //!USE_AUDMESH_SECTORS
atRangeArray<audSector, g_audNumWorldSectors> g_AudioWorldSectors;
#endif

bool g_audDrawWorldSectorDebug = false;
bool g_audResetWorldSectorRoadInfo = false;

Vector3 g_Directions[4];


// Interior stuff
bool g_UseDebugRoomSettings = false;
bool g_TryRoomsEveryTime = false;
InteriorSettings* g_DebugRoomSettings;
f32 g_DebugRoomSettingsReverbSmall = 0.0f;
f32 g_DebugRoomSettingsReverbMedium = 0.0f;
f32 g_DebugRoomSettingsReverbLarge = 0.0f;
bool g_UsePlayer = true;

audCurve g_ShockwaveDistToVol;

char g_InteriorInfoDebug[64] = "";
char g_RoomInfoDebug[64] = "";

extern CPathFind ThePaths;
extern audScannerManager g_AudioScannerManager;

// only used on audio thread
PARAM(headphones, "[Audio] Mix for headphones.");
// only used on game thread
PARAM(echoes, "[Audio] Enables local environment scanning and turns echoes on.");

enum
{
	AUD_DEAFENING_OFF,
	AUD_DEAFENING_RAMP_UP,
	AUD_DEAFENING_HOLD,
	AUD_DEAFENING_RAMP_DOWN
};

naEnvironment::naEnvironment()
{
}

naEnvironment::~naEnvironment()
{
}

void naEnvironment::Init()
{
	g_Directions[0] = Vector3(0.0f, 1.0f, 0.0f); //N
	g_Directions[1] = Vector3(1.0f, 0.0f, 0.0f); //E
	g_Directions[2] = Vector3(0.0f, -1.0f, 0.0f); //S
	g_Directions[3] = Vector3(-1.0f, 0.0f, 0.0f); //W

	if(PARAM_echoes.Get())
	{
		g_EnableLocalEnvironmentMetrics = true;
	}
	
	m_BuiltUpToRolloff.Init(ATSTRINGHASH("BUILT_UP_TO_ROLLOFF", 0x7F6DB253));

	m_DeafeningState[0] = m_DeafeningState[1] = AUD_DEAFENING_OFF;
	m_DeafeningPhaseEndTime[0] = m_DeafeningPhaseEndTime[1] = 0;
	m_DeafeningMaxStrength[0] = m_DeafeningMaxStrength[1] = 0.0f;
	m_DeafeningMinStrength[0] = m_DeafeningMinStrength[1] = 0.0f;
	m_DeafeningAttenuationCurve.Init(ATSTRINGHASH("DEAFENING_STRENGTH_TO_ATTENUATION", 0x65D9D83E));
	m_DeafeningFilterCurve.Init(ATSTRINGHASH("DEAFENING_STRENGTH_TO_FILTER", 0xA8DE551F));
	m_DeafeningReverbCurve.Init(ATSTRINGHASH("DEAFENING_STRENGTH_TO_REVERB", 0x5D6B04BC));
	m_DeafeningVolumeToStrengthCurve.Init(ATSTRINGHASH("DEAFENING_VOLUME_TO_STRENGTH_CURVE", 0xE4309D3));
	m_DeafeningDistanceRolloffCurve.Init(ATSTRINGHASH("DEFAULT_ROLLOFF", 0x3BD35057));
	g_ShockwaveDistToVol.Init(ATSTRINGHASH("SHOCKWAVE_DIST_TO_VOL", 0x26002533));

	m_IsListenerInHead = false;
	m_ForceAmbientMetricsUpdate = false;

	g_DebugRoomSettings = audNorthAudioEngine::GetObject<InteriorSettings>(ATSTRINGHASH("DEBUG_INTERIOR", 0x044a9a7e2));

	ClearLinkedRooms();
	m_NumRainPropSounds = 0;
	m_EarStrength[0] = m_EarStrength[1] = 0.0f;

	for (s32 i=0; i<g_audNumLevelProbeRadials; i++)
	{
		m_LevelReflectionNormal[i].Zero();
	}

	for (s32 i=0; i<g_audNumMidProbeRadials; i++)
	{
		m_UpperReflectionNormal[i].Zero();
		m_EchoDistance[i] = 0.0f;
		m_EchoSuitability[i] = 0.0f;
		m_EchoPosition[i].Zero();
		m_EchoDirection[i].Set(1.0f);
	}
	
	m_ListenerInteriorInstance = NULL;
	m_InteriorRoomId = 0;
	m_ListenerInteriorSettings = NULL;
	m_ListenerInteriorRoom = NULL;

	m_DistanceToEchoCurve.Init(ATSTRINGHASH("DISTANCE_TO_ECHO_GOODNESS", 0x4883D0D7));
	m_SilentWhenEchoCoLocatedCurve.Init(ATSTRINGHASH("ECHO_COLOCATED_ATTENUATION", 0xDB040C16));
	m_EchoGoodnessLinearVolumeCurve.Init(ATSTRINGHASH("ECHO_GOODNESS_TO_LINEAR_VOLUME", 0x4D2F84CA));
	m_EchoGoodnessDbVolumeCurve.Init(ATSTRINGHASH("ECHO_GOODNESS_TO_DB_VOLUME", 0x59B4D084));
	m_DistanceToDelayCurve.Init(ATSTRINGHASH("ECHO_DISTANCE_TO_PREDELAY", 0xE42B577A));
	m_AngleOfIncidenceToSuitabilityCurve.Init(ATSTRINGHASH("ECHO_ANGLE_TO_SUITABILITY", 0x9972AF32));
	m_OutdoorInteriorWeaponVolCurve.Init(ATSTRINGHASH("REV_WET_TO_OUTDOOR_INTERIOR_WEAPON_VOLUME", 0xBD8ED687));
	m_OutdoorInteriorWeaponHoldCurve.Init(ATSTRINGHASH("REV_SIZE_TO_OUTDOOR_INTERIOR_WEAPON_HOLD", 0x6D07F377));

	BANK_ONLY(m_CoveredPolys.Reset());
	m_NumValidPolys = 0;
	for (u32 i=0; i<MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES; i++)
	{
		m_SourceEnvironmentMetrics[i] = 0;
		m_SourceEnvironmentPolyAreas[i] = 0.0f;
		m_SourceEnvironmentPolyCentroids[i].Zero();
		for (u32 j=0; j<g_NumSourceReverbs; j++)
		{
			m_SourceEnvironmentPolyReverbWet[i][j] = 0.0f;
		}
	}
	m_ListenerPositionLastSourceEnvironmentUpdate.Zero();
	m_SourceEnvironmentTimeLastUpdated = 0;

	m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;

	m_SourceEnvironmentToReverbWetCurve.Init(ATSTRINGHASH("SOURCE_ENVIRONMENT_TO_REVERB_WET", 0x069607de3));
	m_SourceEnvironmentToReverbSizeCurve.Init(ATSTRINGHASH("SOURCE_ENVIRONMENT_TO_REVERB_SIZE", 0x0fd1b7244));
	m_SourceEnvironmentSizeToWetScaling.Init(ATSTRINGHASH("SOURCE_ENVIRONMENT_SIZE_TO_WET_SCALING", 0x033ef2c0b));

	m_DistanceToListenerReverbCurve.Init(ATSTRINGHASH("DISTANCE_TO_LISTENER_REVERB", 0x49BAD9E5));
	m_AreaToListenerReverbCurve.Init(ATSTRINGHASH("AREA_TO_LISTENER_REVERB", 0x2AD353B));
	for (u32 i=0; i<4; i++)
	{
		for (u32 j=0; j<3; j++)
		{
			m_SpeakerReverbWetSmoothers[i][j].Init(g_SpeakerReverbSmootherRate, true);
		}
	}
	//Prepare crossfade curves for source reverb submixes.
	for(u32 i=0; i<g_NumSourceReverbs; i++)
	{
		m_SourceReverbCrossfadeCurves[i].Init(g_SourceReverbCurveNames[i]);
	}

	m_NumBuildingAddsThisFrame = m_NumBuildingRemovesThisFrame = m_NumTreeAddsThisFrame = m_NumTreeRemovesThisFrame = 0;

	m_BuiltUpFactor = 0.0f;
	m_BuildingDensityFactor = 0.0f;
	m_TreeDensityFactor = 0.0f;
	m_WaterFactor = 0.0f;
	m_HighwayFactor = 0.0f;
	m_AveragePedZPos = 0.0f;
	m_PedFractionMale = 0.0f;

	m_WaterHeightAtListener = 0.f;

	m_HeightToBuiltUpFactor.Init(ATSTRINGHASH("HEIGHT_TO_BUILT_UP_FACTOR", 0x1ADE3D7F));
	m_BuildingNumToDensityFactor.Init(ATSTRINGHASH("BUILDING_COUNT_TO_DENSITY_FACTOR", 0x6E08E885));
	m_TreeNumToDensityFactor.Init(ATSTRINGHASH("TREE_COUNT_TO_DENSITY_FACTOR", 0x7DD816BE));
	m_WaterAmountToWaterFactor.Init(ATSTRINGHASH("WATER_AMOUNT_TO_WATER_FACTOR", 0xCD234F5D));
	m_HighwayAmountToHighwayFactor.Init(ATSTRINGHASH("HIGHWAY_AMOUNT_TO_HIGHWAY_FACTOR", 0x7446E468));
	m_VehicleAmountToVehicleDensity.Init(ATSTRINGHASH("VEHICLE_COUNT_TO_DENSITY_FACTOR", 0x4F5ECEE5));
	m_VehicleDistanceSqToVehicleAmount.Init(ATSTRINGHASH("VEHICLE_DISTANCE_SQ_TO_VEHICLE_AMOUNT", 0x81F1EBC2));
	m_VehicleSpeedToVehicleAmount.Init(ATSTRINGHASH("VEHICLE_SPEED_TO_VEHICLE_AMOUNT", 0xB181AEFB));

	m_BuiltUpSmoother.Init(g_BuiltUpSmootherRate, true);
	m_BuildingDensitySmoother.Init(g_BuiltUpSmootherRate, true);
	m_TreeDensitySmoother.Init(g_BuiltUpSmootherRate, true);
	m_WaterSmoother.Init(g_BuiltUpSmootherRate, true);
	m_HighwaySmoother.Init(g_BuiltUpSmootherRate, true);
	m_BuiltUpDampingFactorSmoother.Init(g_BuiltUpDampingFactorSmoothUpRate, g_BuiltUpDampingFactorSmoothDownRate, true);

	for (u32 i=0; i<AUD_NUM_SECTORS; i++)
	{
		m_BuildingHeights[i] = 0.0f;
		m_BuildingCount[i] = 0.0f;
		m_TreeCount[i] = 0.0f;
		m_BuiltUpFactorPerZone[i] = 0.0f;
	}
	for (u32 i=0; i<4; i++)
	{
		m_BuiltUpDirectionalSmoother[i].Init(g_BuiltUpSmootherRate, true);
		m_BuildingDensityDirectionalSmoother[i].Init(g_BuiltUpSmootherRate, true);
		m_TreeDensityDirectionalSmoother[i].Init(g_BuiltUpSmootherRate, true);
		m_WaterDirectionalSmoother[i].Init(g_BuiltUpSmootherRate, true);
		m_HighwayDirectionalSmoother[i].Init(g_BuiltUpSmootherRate, true);

		m_DirectionalBuiltUpFactor[i] = 0.0f;
		m_DirectionalBuildingDensity[i] = 0.0f;
		m_DirectionalTreeDensity[i] = 0.0f;
		m_DirectionalWaterFactor[i] = 0.0f;
		m_DirectionalHighwayFactor[i] = 0.0f;
	}
	for (u32 i=0; i<4; i++)
	{
		m_SpeakerBuiltUpSmoother[i].Init(g_BuiltUpSmootherRate, true);
		m_SpeakerBuiltUpFactors[i] = 0.0f;
		m_SpeakerBlockedFactor[i] = 0.0f;
		m_SpeakerBlockedLinearVolume[i] = 0.0f;
		m_SpeakerOWOFactor[i] = 0.0f;
	}

	m_CarInteriorFilterSmoother.Init(40.f,40.f,2000.f,24000.f);
	// On the first update it'll jump to the right spot and then start smoothing after that 
	m_CarInteriorFilterCutoff = 24000.f;

	m_PlayerInCarRatio = 0.0f;
	m_IsPlayerInVehicle = false;
	m_IsPlayerVehicleATrain = false;
	m_PlayerInCarSmoother.Init(g_InCarRatioSmoothRate/1000.0f, true);

	m_PlayerIsInASubwayStationOrSubwayTunnel = false;
	m_PlayerIsInASubwayOrTunnel = false;
	m_PlayerWasInASubwayOrTunnelLastFrame = false;
	m_PlayerIsInAnInterior = false;
	m_PlayerInteriorRatioSmoother.Init(g_InteriorRatioSmoothRate/1000.0f, true);
	m_PlayerInteriorRatio = 0.0f;
	m_RoomToneHash = g_NullSoundHash;
	m_RainType = RAIN_TYPE_NONE;

	m_NavmeshInformationUpdatedThisFrame = false;

	m_LastTimeWeWereAPassengerInATaxi = 0;

	for(u32 i = 0; i < g_MaxInterestingLocalObjects; i++)
	{
		m_InterestingLocalObjects[i].object = NULL;
		m_InterestingLocalObjects[i].envGroup = NULL;
		m_InterestingLocalObjects[i].rainSound = NULL;
		m_InterestingLocalObjects[i].windSound = NULL;
		m_InterestingLocalObjects[i].randomAmbientSound = NULL;
		m_InterestingLocalObjects[i].macsComponent = -1;
		m_InterestingLocalObjects[i].stillValid = false;
		m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound = NULL;
		m_InterestingLocalObjects[i].resoSoundInfo.resonanceIntensity = 0.f;
		m_InterestingLocalObjects[i].resoSoundInfo.attackTime = 0.f;
		m_InterestingLocalObjects[i].resoSoundInfo.releaseTime = 0.f;
		m_InterestingLocalObjects[i].resoSoundInfo.resoSoundRef = g_NullSoundRef;
		m_InterestingLocalObjects[i].resoSoundInfo.slowmoResoVolLin = 0.f;
		m_InterestingLocalObjects[i].resoSoundInfo.slowmoResoPhase = 0.f;
		m_InterestingLocalObjects[i].shockwaveSoundInfo.attackTime = -1.f;
		m_InterestingLocalObjects[i].shockwaveSoundInfo.releaseTime = -1.f;
		m_InterestingLocalObjects[i].shockwaveSoundInfo.prevVol = 0.f;
		BANK_ONLY(m_InterestingLocalObjects[i].underCover = true);
		m_InterestingLocalObjectsCached[i].object = NULL;
		m_InterestingLocalObjectsCached[i].envGroup = NULL;
		m_InterestingLocalObjectsCached[i].rainSound = NULL;
		m_InterestingLocalObjectsCached[i].windSound = NULL;
		m_InterestingLocalObjectsCached[i].randomAmbientSound = NULL;
		m_InterestingLocalObjectsCached[i].macsComponent = -1;
		m_InterestingLocalObjectsCached[i].stillValid = false;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.resonanceSound = NULL;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.resonanceIntensity = 0.f;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.attackTime = 0.f;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.releaseTime = 0.f;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.resoSoundRef = g_NullSoundRef;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.slowmoResoVolLin = 0.f;
		m_InterestingLocalObjectsCached[i].resoSoundInfo.slowmoResoPhase = 0.f;
		m_InterestingLocalObjectsCached[i].shockwaveSoundInfo.attackTime = -1.f;
		m_InterestingLocalObjectsCached[i].shockwaveSoundInfo.releaseTime = -1.f;
		m_InterestingLocalObjectsCached[i].shockwaveSoundInfo.prevVol = 0.f;
		BANK_ONLY(m_InterestingLocalObjectsCached[i].underCover = true);
		m_NextInterestingObjectRandTime[i] = audEngineUtil::GetRandomNumberInRange(5000, 30000);
	}
	for(u32 i = 0; i < g_MaxWindSoundsHistory; i++)
	{
		m_WindTriggeredSoundsHistory[i].soundRef = g_NullSoundRef;
		m_WindTriggeredSoundsHistory[i].time = 0;
	}
	m_NextWindObjectTime = 0;
	for (u8 i = 0; i < g_MaxInterestingLocalTrees; i++ )
	{
		m_LocalTrees[i].tree = NULL;
		m_LocalTrees[i].sector = AUD_SECTOR_FL;
		m_LocalTrees[i].type = AUD_BUSHES;
		m_LocalTrees[i].windSound = NULL;
	}
	for (u8 i = 0; i < MAX_FB_SECTORS; i++)
	{
		m_BackGroundTreesSounds[0][i].sound = NULL;
		m_BackGroundTreesSounds[1][i].sound = NULL;
		m_BackGroundTreesSounds[0][i].fitness = 0.f;
		m_BackGroundTreesSounds[1][i].fitness = 0.f;
		BANK_ONLY(m_BackGroundTreesSounds[0][i].linVol = 0.f;)
		BANK_ONLY(m_BackGroundTreesSounds[1][i].linVol = 0.f;)
		m_BackGroundTreesSounds[0][i].avgDistance = 0.f;
		m_BackGroundTreesSounds[1][i].avgDistance = 0.f;
		m_BackGroundTreesSounds[0][i].volSmoother.Init(0.01f);
		m_BackGroundTreesSounds[1][i].volSmoother.Init(0.01f);

		m_BackGroundTrees[i].bigTreesFitness = 0.f;
		m_BackGroundTrees[i].treesFitness = 0.f;
		m_BackGroundTrees[i].bushesFitness = 0.f;
		m_BackGroundTrees[i].numBigTrees = 0;
		m_BackGroundTrees[i].numTrees = 0;
		m_BackGroundTrees[i].numBushes = 0;
		m_CurrentSoundIdx[i] = 0;
	}
	//for (u8 i = 0; i < MAX_AUD_TREE_TYPES; i++)
	//{
	m_BackGroundTreesFitness[AUD_BIG_TREES] = 1.f;
	m_BackGroundTreesFitness[AUD_SMALL_TREES] = 0.6f;
	m_BackGroundTreesFitness[AUD_BUSHES] = 0.3f;
	//}

	m_WindBushesSoundset.Init(ATSTRINGHASH("WIND_BUSH_SOUNDSET", 0x4AF50222));
	m_WindTreesSoundset.Init(ATSTRINGHASH("WIND_TREE_SMALL_SOUNDSET", 0xDA99FD95));
	m_WindBigTreesSoundset.Init(ATSTRINGHASH("WIND_TREE_LARGE_SOUNDSET", 0x377B6059));

	// BIG TREES
	m_NumTreesToVolume[AUD_BIG_TREES].Init(ATSTRINGHASH("NUM_BIG_TREES_TO_BG_VOLUME", 0xA19220F8));
	m_TreesDistanceToVolume[AUD_BIG_TREES].Init(ATSTRINGHASH("BIG_TREES_DIST_TO_ATTENUATION", 0x30D49CAD));
	m_NumTreesToVolume[AUD_SMALL_TREES].Init(ATSTRINGHASH("NUM_SMALL_TREES_TO_BG_VOLUME", 0xB2B37BAD));
	m_TreesDistanceToVolume[AUD_SMALL_TREES].Init(ATSTRINGHASH("SMALL_TREES_DIST_TO_ATTENUATION", 0x7E5C39CE));
	m_NumTreesToVolume[AUD_BUSHES].Init(ATSTRINGHASH("NUM_BUSHES_TO_BG_VOLUME", 0x86F51D84));
	m_TreesDistanceToVolume[AUD_BUSHES].Init(ATSTRINGHASH("BUSHES_DIST_TO_ATTENUATION", 0x602A042F));

	for(u32 i = 0; i < g_MaxResonatingObjects; i++)
	{
		m_ResonatingObjects[i].Clear();
	}
	
	m_RegisteredShockwavesAllocation.Init(kMaxRegisteredShockwaves);
	
	m_UsePortalInfoForWeather = false;
	
	//Load audio world sectors info 
	// TODO: Fix file endian for PC.
	if(!sm_ProcessSectors)
	{
		fiStream *dataFile = fiStream::Open("common:/data/levels/gta5/AudioWorldSectorsInfo.dat");
		if(dataFile)
		{
			dataFile->Read(&g_AudioWorldSectors,sizeof(g_AudioWorldSectors));
			dataFile->Close();
		}
		else
		{
			naWarningf("Fail opening audio world sectors data file");
		}
	}
	else
	{
		for(u32  sector = 0; sector < g_AudioWorldSectors.GetMaxCount(); sector++)
		{
			audSector initSector;
			initSector.numHighwayNodes = 0;
			initSector.tallestBuilding = 0;
			initSector.numBuildings = 0;
			initSector.numTrees = 0;
			initSector.isWaterSector = false;
			g_AudioWorldSectors[sector] = initSector;
		}
	}
	m_InterestingObjectRainSmoother.Init(0.001f, true);

	m_ScriptedSpecialEffectMode = kSpecialEffectModeNormal;

	sm_HDotToSlowmoResoVol.Init(ATSTRINGHASH("HDOT_TO_SLOWMO_RESO_VOL", 0xEE5D047A));
	sm_VDotToSlowmoResoVol.Init(ATSTRINGHASH("VDOT_TO_SLOWMO_RESO_VOL", 0x97E459ED));
	sm_SpecialAbToFrequency.Init(ATSTRINGHASH("SPECIAL_ABILITY_TO_FREQUENCY", 0xD0974421));
	sm_CityGustDotToVolSpikeForBgTrees.Init(ATSTRINGHASH("CITY_GUST_DOT_TO_VOL", 0x5322FF8F));
	sm_CountrySideGustDotToVolSpikeForBgTrees.Init(ATSTRINGHASH("COUNTRYSIDE_GUST_DOT_TO_VOL", 0x805E6E0));
	
	m_PlayerUnderCoverFactor = 0.f;
	m_PlayerUnderCoverSmoother.Init(0.03f,0.01f,0.f,100.f);

#if RSG_BANK
	m_OverrideSpecialEffectMode = false;
	m_SpecialEffectModeOverride = kSpecialEffectModeNormal;
#endif
}

void naEnvironment::Shutdown()
{
	for(u32 i = 0; i < g_MaxInterestingLocalObjects; i++)
	{
		m_InterestingLocalObjects[i].object = NULL;
		m_InterestingLocalObjects[i].envGroup = NULL;
		m_InterestingLocalObjects[i].macsComponent = -1;
		m_InterestingLocalObjects[i].stillValid = false;
		BANK_ONLY(	m_InterestingLocalObjects[i].underCover = true);
		m_InterestingLocalObjectsCached[i].object = NULL;
		m_InterestingLocalObjectsCached[i].envGroup = NULL;
		m_InterestingLocalObjectsCached[i].macsComponent = -1;
		m_InterestingLocalObjectsCached[i].stillValid = false;
		BANK_ONLY(m_InterestingLocalObjectsCached[i].underCover = true);
	}
	for(u32 i = 0; i < g_MaxWindSoundsHistory; i++)
	{
		m_WindTriggeredSoundsHistory[i].soundRef = g_NullSoundRef;
		m_WindTriggeredSoundsHistory[i].time = 0;
	}
	for (u8 i = 0; i < g_MaxInterestingLocalTrees; i++ )
	{
		m_LocalTrees[i].tree = NULL;
		m_LocalTrees[i].sector = AUD_SECTOR_FL;
		m_LocalTrees[i].type = AUD_BUSHES;
	}
	for(u32 i = 0; i < g_MaxResonatingObjects; i++)
	{
		m_ResonatingObjects[i].Clear();
	} 
}

void naEnvironment::PurgeInteriors()
{
	m_ListenerInteriorInstance = NULL;
	m_InteriorRoomId = 0;
	m_ListenerInteriorSettings = NULL;
	m_ListenerInteriorRoom = NULL;
}

//****************************************************************************************************************************************
// Game-thread GTA-specific functions
//****************************************************************************************************************************************
bool naEnvironment::ParseNodeCallback(CPathNode * pNode, void * pData)
{
	audParseNodeCallbackData* callbackData = (audParseNodeCallbackData*)pData;
	Vector3 position;
	pNode->GetCoors(position);

	if(position.x > callbackData->minPos.x &&
		position.y > callbackData->minPos.y &&
		position.x <= callbackData->maxPos.x &&
		position.y <= callbackData->maxPos.y)
	{
		if(!pNode->m_address.IsEmpty() && ThePaths.IsRegionLoaded(pNode->m_address))
		{
			if(pNode->IsHighway())
			{
				callbackData->numHighwayNodes = (callbackData->numHighwayNodes) + 1;
			}

			callbackData->numNodes = (callbackData->numNodes) + 1;
		}
	}
	
	return true;
}

void naEnvironment::Update()
{
	// Update the water height at the listener position. 
	m_WaterHeightAtListener = CGameWorldWaterHeight::GetWaterHeightAtPos(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());

	// Update the metrics about how built-up the area is based on the world sector info
	UpdateMetrics();
	
	// Map our directional built-up metrics to speaker directions, to save an inverse mapping on every sound
	MapDirectionalMetricsToSpeakers();

	if (g_EnableLocalEnvironmentMetrics)
	{
		// Update our possible echo positions - on the basis of our local environment
		UpdateEchoPositions();

		// Update our view of the local reverb environment - based on our navmesh info, and the real-time wallscanning.
		UpdateLocalReverbEnvironment();
	}

	UpdateInsideCarMetric();

	bool passenegerInATaxi = AreWeAPassengerInATaxi();
	if (passenegerInATaxi)
	{
		m_LastTimeWeWereAPassengerInATaxi = fwTimer::GetTimeInMilliseconds();
	}

	bool isInteriorCam = camInterface::IsRenderedCameraInsideVehicle();

	if(!isInteriorCam && passenegerInATaxi && camInterface::GetScriptDirector().IsRendering())
	{
		isInteriorCam = true;
	}

	const f32 filterCutoff = (isInteriorCam?5000.f:24000.f);
	m_CarInteriorFilterCutoff = m_CarInteriorFilterSmoother.CalculateValue(filterCutoff,fwTimer::GetTimeInMilliseconds());

	// Update deafening state
	UpdateDeafeningState();
#if __BANK
	if(sm_ProcessSectors)
	{
		// world sector stuff
		ProcessCachedWorldSectorOps();
	}
#endif

	// trigger random ambients from local objects
	for(s32 i = g_MaxInterestingLocalObjects -1; i >= 0; i--)
	{
		if(m_InterestingLocalObjects[i].object && GetInterestingLocalObjectMaterial(i) && GetInterestingLocalObjectMaterial(i)->RandomAmbient != g_NullSoundHash &&
			fwTimer::GetTimeInMilliseconds() > m_NextInterestingObjectRandTime[i] && !m_InterestingLocalObjects[i].randomAmbientSound && !IsObjectDestroyed(m_InterestingLocalObjects[i].object))
		{
			audSoundInitParams initParams;
			initParams.Tracker = m_InterestingLocalObjects[i].object->GetPlaceableTracker();

			if (!initParams.Tracker)
			{
				initParams.Position = VEC3V_TO_VECTOR3(m_InterestingLocalObjects[i].object->GetMatrix().GetCol3());
			}

			initParams.EnvironmentGroup = m_InterestingLocalObjects[i].envGroup;
			const u32 interiorTrashRustle = ATSTRINGHASH("RANDOM_TRASH_RUSTLE", 0x04cfd6fb4);
			u32 soundHash = GetInterestingLocalObjectMaterial(i)->RandomAmbient;
			if(soundHash == interiorTrashRustle)
			{
				s32 roomId = m_InterestingLocalObjects[i].object->GetInteriorLocation().GetRoomIndex();
				if(!m_InterestingLocalObjects[i].object->m_nFlags.bInMloRoom || roomId < 0 || roomId == INTLOC_INVALID_INDEX)
				{
					const u32 exteriorTrashRustle = ATSTRINGHASH("TRASH_RUSTLE_EXTERIOR_RANDOM", 0x0e8c5b77a);
					soundHash = exteriorTrashRustle;
				}
			}
			g_AmbientAudioEntity.CreateAndPlaySound_Persistent(soundHash, &m_InterestingLocalObjects[i].randomAmbientSound, &initParams);

			if(audEngineUtil::ResolveProbability(0.2f))
			{
				// occasionally play two in quick succession
				m_NextInterestingObjectRandTime[i] = fwTimer::GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(3000,10000);
			}
			else
			{
				m_NextInterestingObjectRandTime[i] = fwTimer::GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(15000,45000);
			}
		}
		if(m_InterestingLocalObjects[i].object && m_InterestingLocalObjects[i].envGroup)
		{
			phInst * pInst = ((CObject*)((CEntity*)m_InterestingLocalObjects[i].object))->GetCurrentPhysicsInst();
			if(pInst && pInst->GetLevelIndex() != phInst::INVALID_INDEX && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex()))
			{
				m_InterestingLocalObjects[i].envGroup->SetSource(pInst->GetCenterOfMass()); 
				m_InterestingLocalObjects[i].envGroup->SetUpdateTime(sm_ObjOcclusionUpdateTime);	
			}
			else if(m_InterestingLocalObjects[i].envGroup->HasGotUnderCoverResults())
			{
				m_InterestingLocalObjects[i].envGroup->SetSource(m_InterestingLocalObjects[i].object->GetTransform().GetPosition()); 
				m_InterestingLocalObjects[i].envGroup->SetUpdateTime(0);
			}
		}
	}

	BANK_ONLY(DrawDebug());

#if RSG_BANK 
	if(sm_DrawAudioWorldSectors)
	{
		DrawAudioWorldSectors();
	}
	//GenerateAudioWorldSectorsInfo();
#endif

#if RSG_BANK
	if(m_OverrideSpecialEffectMode)
	{
		g_AudioEngine.GetEnvironment().SetSpecialEffectMode(m_SpecialEffectModeOverride);
		return;
	}
#endif // RSG_BANK

	// Special effect mode - underwater takes precedence over script
	if(g_FrontendAudioEntity.IsMenuMusicPlaying())
	{
		g_AudioEngine.GetEnvironment().SetSpecialEffectMode(kSpecialEffectModePauseMenu);
	}
	else if(Water::IsCameraUnderwater() && !audRadioStation::IsPlayingEndCredits())
	{
		g_AudioEngine.GetEnvironment().SetSpecialEffectMode(kSpecialEffectModeUnderwater);
	}
	else
	{
		g_AudioEngine.GetEnvironment().SetSpecialEffectMode(m_ScriptedSpecialEffectMode);
	}

	// Auto-reset scripted mode; for safety script must set every frame
	m_ScriptedSpecialEffectMode = kSpecialEffectModeNormal;
}

bool naEnvironment::IsObjectDestroyed(CEntity* pEntity)
{
	if(pEntity && pEntity->GetIsTypeObject())
	{
		if(((CObject*)pEntity)->GetHealth() == 0.f)
		{
			return true;
		}
	}

	return false;
}

#if __BANK 
void naEnvironment::DrawAudioWorldSectors(bool draw)
{
	sm_DrawAudioWorldSectors = draw;
	g_audDrawWorldSectorDebug = draw;
}
void naEnvironment::AbortAudWorldSectorsGeneration()
{
	sm_ProcessSectors = false; 
	sm_DrawAudioWorldSectors = false; 
	sm_IsWaitingToInit = true;
	audWarpManager::WaitUntilTheSceneIsLoaded(false);
	audWarpManager::FadeCamera(true);
	fiStream *rescueFile = fiStream::Open("C:/AudioSectorInfoGeneratorRescue",false);
	if(rescueFile)
	{
		sm_CurrentSectorX = 0;
		sm_CurrentSectorY = 0;
		rescueFile->WriteInt(&sm_CurrentSectorX,1);
		rescueFile->WriteInt(&sm_CurrentSectorY,1);
		rescueFile->Close();
	}
}
void naEnvironment::InitAudioWorldSectorsGenerator()
{
	sm_IsWaitingToInit = false;
	naDisplayf("Initializing process"); 
	naEnvironment::sm_DrawAudioWorldSectors = true; 
	naDisplayf("Making player invincible");
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Couldn't find local player, aborting process"))
	{
		pPlayer->m_nPhysicalFlags.bNotDamagedByAnything = true;
	}
	else
	{
		AbortAudWorldSectorsGeneration();
	}
	naDisplayf("Checking if we have to resume the process or start a new one.");
	sm_CurrentSectorX = 0;
	sm_CurrentSectorY = 0;
	CleanUpWaterTest();

	fiStream *dataFile = fiStream::Open("C:/AudioWorldSectorsInfo.dat",false);
	if(!dataFile)
	{
		dataFile = fiStream::Create("C:/AudioWorldSectorsInfo.dat");
									 
		if(naVerifyf(dataFile,"Fail opening/creating the file, aborting the process."))
		{
			//Reset the array in memory and write it in one go to the file, so we reset the file as well to the size it will have. 
			naDisplayf("%d",g_AudioWorldSectors.GetMaxCount());
			for(u32  sector = 0; sector < g_AudioWorldSectors.GetMaxCount(); sector++)
			{
				audSector initSector;
				initSector.numHighwayNodes = 0;
				initSector.tallestBuilding = 0;
				initSector.numBuildings = 0;
				initSector.numTrees = 0;
				initSector.isWaterSector = false;
				dataFile->Write(&initSector,sizeof(audSector));
			}
		}
		else
		{
			AbortAudWorldSectorsGeneration();
		}
	}
	else
	{
		//Check if we are resuming a process.
		fiStream *rescueFile = fiStream::Open("C:/AudioSectorInfoGeneratorRescue");
		if(rescueFile)
		{
			rescueFile->ReadInt(&sm_CurrentSectorX,1);
			rescueFile->ReadInt(&sm_CurrentSectorY,1);
			rescueFile->Close();
		}
	}
	dataFile->Close();
}

void naEnvironment::WriteAudioWorldSectorsData()
{
	// The players has been warpped and the scene is being completely loaded, write the file
	naDisplayf("Scene loaded : Writing rescue file");
	fiStream *rescueFile = fiStream::Open("C:/AudioSectorInfoGeneratorRescue",false);
	if(!rescueFile)
	{
		rescueFile = fiStream::Create("C:/AudioSectorInfoGeneratorRescue");
	}
	if(naVerifyf(rescueFile,"Fail opening/creating the file, aborting the process."))
	{
		rescueFile->WriteInt(&sm_CurrentSectorX,1);
		rescueFile->WriteInt(&sm_CurrentSectorY,1);
		rescueFile->Close();
	}
	else
	{
		AbortAudWorldSectorsGeneration();
	}
	naDisplayf("Scene loaded : Writing data file");
	fiStream *dataFile = fiStream::Open("C:/AudioWorldSectorsInfo.dat",false);
	if(naVerifyf(dataFile,"Fail opening/creating the file, aborting the process."))
	{
		u32 idx = (sm_CurrentSectorY * g_audNumWorldSectorsX) + sm_CurrentSectorX;
		// Seek the last position
		dataFile->Seek(idx * sizeof(audSector) );
		dataFile->Write(&g_AudioWorldSectors[idx],sizeof(audSector));
		dataFile->Close();
	}
	else
	{
		AbortAudWorldSectorsGeneration();
	}
}
void naEnvironment::GenerateAudioWorldSectorsInfo()
{
	if(sm_ProcessSectors && sm_IsWaitingToInit)
	{
		InitAudioWorldSectorsGenerator();
	}
	else if(sm_ProcessSectors)
	{
		if(audWarpManager::IsActive())
		{
			// Waiting for the scene to be loaded.
			CheckForWaterSector();
			return;
		}
		else if(sm_LogicStarted) // need to check that we have made a loop already, otherwise we have nothing to write on the file.
		{
			CheckForWaterSector();
			WriteAudioWorldSectorsData();
			sm_CurrentSectorX ++;
			if(sm_CurrentSectorX == g_audNumWorldSectorsX )
			{
				sm_CurrentSectorX = 0;
				sm_CurrentSectorY ++;
			}
			if(sm_CurrentSectorY == g_audNumWorldSectorsY )
			{
				naDisplayf("All sectors have been processed, terminating...");
				fiStream *rescueFile = fiStream::Open("C:/AudioSectorInfoGeneratorRescue");
				if(rescueFile)
				{
					sm_CurrentSectorX = 0;
					sm_CurrentSectorY = 0;
					rescueFile->WriteInt(&sm_CurrentSectorX,1);
					rescueFile->WriteInt(&sm_CurrentSectorY,1);
					rescueFile->Close();
				}
				sm_ProcessSectors =  false;
				audWarpManager::WaitUntilTheSceneIsLoaded(false);
				audWarpManager::FadeCamera(true);
				sm_IsWaitingToInit = false;
				naDisplayf("Done");
				return;
			}
		}
		Vector3 centreFirstSector; 
		centreFirstSector.SetX(g_audWorldSectorsMinX + 0.5f * g_audWidthOfSector);
		centreFirstSector.SetY(g_audWorldSectorsMinY + 0.5f * g_audDepthOfSector);
		centreFirstSector.SetZ(100.f);
		sm_LogicStarted = true;
		naDisplayf("processing sector %d", (sm_CurrentSectorY * g_audNumWorldSectorsX) + sm_CurrentSectorX);
		Vector3 sectorCentre = centreFirstSector;
		sectorCentre.SetX(sectorCentre.GetX() + sm_CurrentSectorX*g_audWidthOfSector);
		sectorCentre.SetY(sectorCentre.GetY() + sm_CurrentSectorY*g_audDepthOfSector);
		naDisplayf("Wrapping player to the center sector");
		naDisplayf("Loading scene....");
		audWarpManager::WaitUntilTheSceneIsLoaded(true);
		audWarpManager::FadeCamera(false);
		audWarpManager::SetWarp(sectorCentre, sectorCentre, 0.f, true, true, 600.f);
		CheckForWaterSector();
	}
}
//-------------------------------------------------------------------------------------------------------------------
void naEnvironment::CleanUpWaterTest()
{
	for(s32 i=0; i<NUMBER_OF_WATER_TEST; i++)
	{
		m_WaterTestLines[i] = Vector3(0.f,0.f,0.f);
	}
	m_WaterLevel = 0.f;
}
//-------------------------------------------------------------------------------------------------------------------
void naEnvironment::ProcessWaterTest(const Vector3 &startPoint, const Vector3 &deltaMovement)
{
	naAssert(sm_NumWaterProbeSets < NUMBER_OF_WATER_TEST);
	bool lineHitWater[NUMBER_OF_WATER_TEST];
	m_WaterTestLines[0] = startPoint;
	lineHitWater[0] = false;
	for (s32 i = 1; i < sm_NumWaterProbeSets; i++)
	{
		lineHitWater[i] = false;
		m_WaterTestLines[i] = m_WaterTestLines[i -1] + deltaMovement;
	}
	// Process them 
	for (s32 i = 0; i < sm_NumWaterProbeSets ; ++i)
	{
		// first look for a map collision
		WorldProbe::CShapeTestProbeDesc probeTest;
		WorldProbe::CShapeTestHitPoint result;
		WorldProbe::CShapeTestResults results(result);
		probeTest.SetResultsStructure(&results);
		probeTest.SetContext(WorldProbe::EAudioLocalEnvironment);
		probeTest.SetIsDirected(false);
		probeTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		probeTest.SetStartAndEnd(m_WaterTestLines[i], Vector3(m_WaterTestLines[i].GetX(),m_WaterTestLines[i].GetY(),-1.f));

		bool foundCollision = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
		Vec3V vWaterPos = Vec3V(V_ZERO);
		if (CVfxHelper::TestLineAgainstWater(VECTOR3_TO_VEC3V(m_WaterTestLines[i]),Vec3V(m_WaterTestLines[i].GetX(),m_WaterTestLines[i].GetY(),-1.f), vWaterPos))
		{
			if(!foundCollision || (foundCollision && vWaterPos.GetZf() > result.GetHitPosition().GetZ()))
			{
				m_WaterLevel += 100.f/(sm_NumWaterProbeSets * sm_NumWaterProbeSets);// (sm_NumWaterProbeSets)  because is the number of line sets we send ( sests of 12 linnes ( NUMBER_OF_WATER_TESTS))
				lineHitWater[i] = true;
			}
		}
	}
	for (s32 i = 0; i < NUMBER_OF_WATER_TEST ; ++i)
	{
		Color32 color = Color_red;
		if(lineHitWater[i])
		{
			color = Color_green;
		}
		grcDebugDraw::Line(m_WaterTestLines[i],Vector3(m_WaterTestLines[i].GetX(),m_WaterTestLines[i].GetY(),-1.f), color,1);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void naEnvironment::CheckForWaterSector()
{
	CleanUpWaterTest();
	// Calculate all the lines we need to process
	Vector3 centreFirstSector; 
	centreFirstSector.SetX(g_audWorldSectorsMinX + 0.5f * g_audWidthOfSector);
	centreFirstSector.SetY(g_audWorldSectorsMinY + 0.5f * g_audDepthOfSector);
	centreFirstSector.SetZ(810.f);
	Vector3 sectorCentre = centreFirstSector;
	sectorCentre.SetX(sectorCentre.GetX() + sm_CurrentSectorX*g_audWidthOfSector);
	sectorCentre.SetY(sectorCentre.GetY() + sm_CurrentSectorY*g_audDepthOfSector);

	Vector3 startPoint = sectorCentre;
	Vector3 deltaMovement = Vector3(0.f,0.f,0.f);
	if(sm_NumWaterProbeSets > 1)
	{
		deltaMovement = Vector3((g_audWidthOfSector/(sm_NumWaterProbeSets - 1)),0.f,0.f);
	}

	startPoint = sectorCentre;
	startPoint.SetX(sectorCentre.GetX() - (0.5f * g_audWidthOfSector));
	startPoint.SetY(sectorCentre.GetY() - (0.5f * g_audDepthOfSector));

	for (f32 i = 0.f; i < sm_NumWaterProbeSets; i ++)
	{
		// iterate over y
		ProcessWaterTest(startPoint,deltaMovement);
		startPoint.SetX(sectorCentre.GetX() - (0.5f * g_audWidthOfSector));
		if( sm_NumWaterProbeSets > 1)
		{
			startPoint.SetY(startPoint.GetY() + (g_audDepthOfSector/(sm_NumWaterProbeSets - 1)));
		}
	}

	m_WaterLevel = Min(m_WaterLevel, 100.f);
	naDisplayf("m_WaterLevel %f",m_WaterLevel);
	u32 idx = (sm_CurrentSectorY * g_audNumWorldSectorsX) + sm_CurrentSectorX;
	if(m_WaterLevel > 40.f)
	{
		g_AudioWorldSectors[idx].isWaterSector = true;
	}
	else
	{
		g_AudioWorldSectors[idx].isWaterSector = false;
	}
}
#endif 

void naEnvironment::UpdateInsideCarMetric()
{
	BANK_ONLY(m_PlayerInCarSmoother.SetRate(g_InCarRatioSmoothRate/1000.0f));

	bool isPlayerVehicleATrain = false;
	bool isInACar = true;
	CVehicle *veh = CGameWorld::FindLocalPlayerVehicle();
	if(veh)
	{
		CPed *player = CGameWorld::FindLocalPlayer();
		if(player->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskEnterVehicle *task = (CTaskEnterVehicle*)player->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
			if(task && task->GetState() < CTaskEnterVehicle::State_EnterSeat)
			{
				isInACar = false;	
			}
		}
		else if(player->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			CTaskExitVehicle *task = (CTaskExitVehicle*)player->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
			if(task && task->GetState() >= CTaskExitVehicle::State_ExitSeat)
			{
				isInACar = false;
			}
		}
		if (veh->GetVehicleType()==VEHICLE_TYPE_TRAIN && isInACar)
		{
			isPlayerVehicleATrain = true;
		}
	}
	else
	{
		if(audPedAudioEntity::GetBJVehicle())
		{
			isInACar = true;
		}
		else
		{
			isInACar = false;
		}
	}

	m_IsPlayerVehicleATrain = isPlayerVehicleATrain;
	m_IsPlayerInVehicle = isInACar;
	m_PlayerInCarRatio = m_PlayerInCarSmoother.CalculateValue((isInACar?1.0f:0.0f), audNorthAudioEngine::GetCurrentTimeInMs());
//	grcDebugDraw::AddDebugOutput("PlayerInCarRatio: %f", m_PlayerInCarRatio);
}

bool naEnvironment::IsPlayerInVehicle(f32 *inCarRatio, bool *isPlayerVehicleATrain) const
{
	if (inCarRatio)
	{
		*inCarRatio = m_PlayerInCarRatio;
	}
	if (isPlayerVehicleATrain)
	{
		*isPlayerVehicleATrain = m_IsPlayerVehicleATrain;
	}
	return m_IsPlayerInVehicle;
}


bool naEnvironment::AreWeInAnInterior(bool *isInteriorASubway, f32 *playerInteriorRatio)
{
	if (isInteriorASubway && m_PlayerIsInAnInterior)
	{
		*isInteriorASubway = m_PlayerIsInASubwayStationOrSubwayTunnel;
	}
	if (playerInteriorRatio)
	{
		*playerInteriorRatio = m_PlayerInteriorRatio;
	}
	return m_PlayerIsInAnInterior;
}

bool naEnvironment::AreWeAPassengerInATaxi() const
{
	CPed* player = CGameWorld::FindLocalPlayer();
	if(player && player->GetVehiclePedInside())
	{
		if(CVehicle::IsTaxiModelId(player->GetVehiclePedInside()->GetModelId()))
		{
			// see if the player has a car drive task, otherwise they are just a passenger
			if(!player->GetIsDrivingVehicle() && !player->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
			{
				return true;
			}
		}
	}

	return false;
}

void naEnvironment::RegisterDeafeningSound(Vector3 &pos, f32 volume)
{
	// Work out how loud it is at the listener.
	Vector3 positionRelativeToListener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().ComputePositionRelativeToPanningListener(VECTOR3_TO_VEC3V(pos)));
	// TODO: Need to convert the curve to a registered curve Id if its required
	f32 volumeAtListener = volume + g_AudioEngine.GetEnvironment().GetDistanceAttenuation(0,/*&m_DeafeningDistanceRolloffCurve, */g_DeafeningRolloff, g_AudioEngine.GetEnvironment().ComputeDistanceRelativeToVolumeListener(pos));
	// Map our volume into a deafening strength factor. I'm so making this shit up, with no idea where it's going...
	//	f32 linearVolume = audDriverUtil::ComputeLinearVolumeFromDb(volumeAtListener);
	f32 deafeningStrength = m_DeafeningVolumeToStrengthCurve.CalculateValue(volumeAtListener);//m_VolumeToDeafeningCurve(linearVolume);

	// Work out how much we hear in each made up ear. 
	Vector3 normalisedPosition = positionRelativeToListener;
	normalisedPosition.NormalizeSafe();
	f32 rightEarScaling = 0.5f * (1.0f + normalisedPosition.x); // 1 when full right, 0 when full left.
	f32 leftEarScaling = 1.0f - rightEarScaling;
	rightEarScaling = g_DeafeningEarLeak + rightEarScaling * (1.0f - g_DeafeningEarLeak);
	leftEarScaling = g_DeafeningEarLeak + leftEarScaling * (1.0f - g_DeafeningEarLeak);

	f32 rightEarStrength = deafeningStrength * rightEarScaling;
	f32 leftEarStrength = deafeningStrength * leftEarScaling;

	//	Warningf("RES:: %f; LES: %f; ", rightEarStrength, leftEarStrength);

	f32 earStrength[2];
	earStrength[0] = leftEarStrength;
	earStrength[1] = rightEarStrength;

	// For each independent ear, work out what state we're currently in, and transition as necessary
	u32 currentTime = fwTimer::GetTimeInMilliseconds();
	for (s32 i=0; i<2; i++)
	{
		switch (m_DeafeningState[i])
		{
		case AUD_DEAFENING_OFF:
			// Easy peasy - nothing to check
			m_DeafeningState[i] = AUD_DEAFENING_RAMP_UP;
			m_DeafeningMaxStrength[i] = earStrength[i];
			m_DeafeningMinStrength[i] = 0.0f;
			m_DeafeningPhaseEndTime[i] = currentTime + g_DeafeningRampUpTime;
			break;

		case AUD_DEAFENING_RAMP_UP:
			// If we're going to be louder, override, kicking off a new ramp-up phase, with old level as new min. If not, do nothing.
			if (earStrength[i] > m_DeafeningMaxStrength[i])
			{
				m_DeafeningState[i] = AUD_DEAFENING_RAMP_UP;
				// Get our current volume, to use as the min for the new ramp
				f32 proportionThrough = 1.0f - (((f32)m_DeafeningPhaseEndTime[i] - (f32)currentTime) / (f32)g_DeafeningRampUpTime);
				f32 min = (m_DeafeningMaxStrength[i] - m_DeafeningMinStrength[i]) * proportionThrough + m_DeafeningMinStrength[i];
				m_DeafeningMaxStrength[i] = earStrength[i];
				m_DeafeningMinStrength[i] = min;
				m_DeafeningPhaseEndTime[i] = currentTime + g_DeafeningRampUpTime;
			}
			break;

		case AUD_DEAFENING_HOLD:
			// If we're louder, override, kicking off a new ramp-up phase, with old level as new min. If not, do nothing.
			if (earStrength[i] > m_DeafeningMaxStrength[i])
			{
				m_DeafeningState[i] = AUD_DEAFENING_RAMP_UP;
				m_DeafeningMinStrength[i] = m_DeafeningMaxStrength[i];
				m_DeafeningMaxStrength[i] = earStrength[i];
				m_DeafeningPhaseEndTime[i] = currentTime + g_DeafeningRampUpTime;
			}
			break;

		case AUD_DEAFENING_RAMP_DOWN:
			// if we're louder than we are currently, kick off a new ramp up phase, otherwise ignore (which is a little cheap, but whatever)
			f32 proportionThrough = 1.0f - (((f32)m_DeafeningPhaseEndTime[i] - (f32)currentTime) / (f32)g_DeafeningRampDownTime);
			f32 currentStrength = m_DeafeningMaxStrength[i] * (1.0f - proportionThrough);
			if (earStrength[i] > currentStrength)
			{
				m_DeafeningState[i] = AUD_DEAFENING_RAMP_UP;
				m_DeafeningMinStrength[i] = currentStrength;
				m_DeafeningMaxStrength[i] = earStrength[i];
				m_DeafeningPhaseEndTime[i] = currentTime + g_DeafeningRampUpTime;
			}
		}
	}
}


void naEnvironment::UpdateDeafeningState()
{
	u32 currentTime = audNorthAudioEngine::GetCurrentTimeInMs();// This timer takes pausing into account fwTimer::GetTimeInMilliseconds();
	f32 earStrength[2];

	// Instantly stop any of this if we're in slowmo, or paused, or the screen's fully faded out
	f32 timeWarp = fwTimer::GetTimeWarp();
	f32 fadeLevel = camInterface::GetFadeLevel();
	bool playingCutscene = gVpMan.AreWidescreenBordersActive() ||
		(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsStreamedCutScene()); 
	if (timeWarp<0.9f || audNorthAudioEngine::IsAudioPaused() || fadeLevel>=1.0f || playingCutscene)
	{
		m_DeafeningState[0] = AUD_DEAFENING_OFF;
		m_DeafeningState[1] = AUD_DEAFENING_OFF;
	}

	if(fadeLevel >= 1.0f)
	{
		audNorthAudioEngine::ResetLoudSoundTime();
	}

	for (s32 i=0; i<2; i++)
	{
		switch (m_DeafeningState[i])
		{
		case AUD_DEAFENING_OFF:
			earStrength[i] = 0.0f;
			break;

		case AUD_DEAFENING_RAMP_UP:
			// Are we done with our ramping yet?
			if (currentTime >= m_DeafeningPhaseEndTime[i])
			{
				// Go into Hold phase, and set strength to max
				m_DeafeningState[i] = AUD_DEAFENING_HOLD;
				m_DeafeningPhaseEndTime[i] = currentTime + g_DeafeningHoldTime;
				earStrength[i] = m_DeafeningMaxStrength[i];
			}
			else
			{
				f32 proportionThrough = 1.0f - (((f32)m_DeafeningPhaseEndTime[i] - (f32)currentTime) / (f32)g_DeafeningRampUpTime);
				earStrength[i] = (m_DeafeningMaxStrength[i] - m_DeafeningMinStrength[i]) * proportionThrough + m_DeafeningMinStrength[i];
			}
			break;

		case AUD_DEAFENING_HOLD:
			// Are we done with the hold phase yet?
			if (currentTime >= m_DeafeningPhaseEndTime[i])
			{
				// Go into Ramp Down phase
				m_DeafeningState[i] = AUD_DEAFENING_RAMP_DOWN;
				m_DeafeningPhaseEndTime[i] = currentTime + g_DeafeningRampDownTime;
			}
			earStrength[i] = m_DeafeningMaxStrength[i];
			break;

		case AUD_DEAFENING_RAMP_DOWN:
			// Are we done with the ramp down phase yet?
			if (currentTime >= m_DeafeningPhaseEndTime[i])
			{
				// Go into nothing phase
				m_DeafeningState[i] = AUD_DEAFENING_OFF;
				earStrength[i] = 0.0f;
			}
			else
			{
				f32 proportionThrough = 1.0f - (((f32)m_DeafeningPhaseEndTime[i] - (f32)currentTime) / (f32)g_DeafeningRampDownTime);

				earStrength[i] = m_DeafeningMaxStrength[i] * (1.0f - proportionThrough);
			}
			break;
		}
	}
	g_AudioEngine.GetEnvironment().SetEarAttenuation(m_DeafeningAttenuationCurve.CalculateValue(earStrength[0]),m_DeafeningAttenuationCurve.CalculateValue(earStrength[1]));

	m_EarStrength[0] = earStrength[0];
	m_EarStrength[1] = earStrength[1];

	f32 deafeningStrength = Max(earStrength[0], earStrength[1]);

	// use the most restrictive filter cutoff from deafening computation and the car interior filter
	if(g_UseInteriorCarFilter)
	{
		g_AudioEngine.GetEnvironment().SetDeafeningFilterCutoff(Min<f32>(m_CarInteriorFilterCutoff, m_DeafeningFilterCurve.CalculateValue(deafeningStrength)));
	}
	else
	{
		g_AudioEngine.GetEnvironment().SetDeafeningFilterCutoff(m_DeafeningFilterCurve.CalculateValue(deafeningStrength));
	}
	g_AudioEngine.GetEnvironment().SetDeafeningReverbWetness(m_DeafeningReverbCurve.CalculateValue(deafeningStrength));
	if(m_DeafeningState[0] == AUD_DEAFENING_OFF && m_DeafeningState[1] == AUD_DEAFENING_OFF)
	{
		g_AudioEngine.GetEnvironment().SetDeafeningAffectsEverything(false);
	}
	else
	{
		// when we're doing real deafening we want to affect all sounds regardless of effect route
		g_AudioEngine.GetEnvironment().SetDeafeningAffectsEverything(true);
	}
}

void naEnvironment::GetDeafeningStrength(f32* leftEar, f32* rightEar)
{
	naAssertf (leftEar, "Null ptr leftEar (f32*) passed into GetDeafeningStrength");
	naAssertf (rightEar, "Null ptr rightEar (f32*) passed into GetDeafeningStrength");

	if (leftEar && rightEar)
	{
		*leftEar = m_EarStrength[0];
		*rightEar = m_EarStrength[1];
	}
}

bool naEnvironment::IsDeafeningOn()
{
	if (m_DeafeningState[0] != AUD_DEAFENING_OFF || m_DeafeningState[1] != AUD_DEAFENING_OFF)
	{
		return true;
	}
	return false;
}

bool naEnvironment::FindCloseObjectsCB(fwEntity* entity, void* )
{
	CEntity* pEntity = (CEntity*) entity;
	if ( naVerifyf(pEntity, "Null ptr pEntity (CEntity*) passed into FindClosestObjectsCB"))
	{
		CBaseModelInfo *modelInfo = pEntity->GetBaseModelInfo();
		if(modelInfo)
		{
			if(modelInfo->GetIsTree())
			{
				audNorthAudioEngine::GetGtaEnvironment()->AddLocalTree(pEntity);
			}
			else
			{
				const ModelAudioCollisionSettings * materialSettings = NULL;
				s32 component = -1;
				if(pEntity->GetType() == ENTITY_TYPE_OBJECT || pEntity->GetType() == ENTITY_TYPE_BUILDING)
				{
					if(pEntity->GetFragInst() && pEntity->GetFragInst()->GetPartBroken())
					{
						u32 numGroups = (pEntity->GetFragInst()) ? pEntity->GetFragInst()->GetTypePhysics()->GetNumChildGroups() : 0;
						bool isInteresting = false;
						component = 0;
						for(u32 j=0; j < numGroups; j++)
						{
							if(!pEntity->GetFragInst()->GetGroupBroken(j))
							{
								u32 numComponents = (pEntity->GetFragInst()) ? pEntity->GetFragInst()->GetTypePhysics()->GetGroup(j)->GetNumChildren() : 0;
								for(u32 k=0; k < numComponents; k++)
								{
									materialSettings = audCollisionAudioEntity::GetFragComponentMaterialSettings(pEntity, component,true);
									if(materialSettings && (materialSettings->RainLoop != g_NullSoundHash || materialSettings->RandomAmbient != g_NullSoundHash || materialSettings->ShockwaveSound != g_NullSoundHash || materialSettings->EntityResonance != g_NullSoundHash || materialSettings->WindSounds != 0))
									{
										isInteresting = true;
										break;
									}
									component++;							
								}
							}
							else 
							{
								component += (pEntity->GetFragInst()) ? pEntity->GetFragInst()->GetTypePhysics()->GetGroup(j)->GetNumChildren() : 0;
							}
							if(isInteresting)
							{
								break;
							}
						}
					}
					else
					{
						materialSettings = pEntity->GetBaseModelInfo()->GetAudioCollisionSettings();
					}

					if(materialSettings && (materialSettings->RainLoop != g_NullSoundHash || materialSettings->RandomAmbient != g_NullSoundHash || materialSettings->ShockwaveSound != g_NullSoundHash || materialSettings->EntityResonance != g_NullSoundHash || materialSettings->WindSounds != 0))
					{
						// add this to the list
						audNorthAudioEngine::GetGtaEnvironment()->AddInterestingObject(pEntity, materialSettings->ShockwaveSound, materialSettings->EntityResonance,component, materialSettings->RainLoop != g_NullSoundHash);
					}	
				}
			}
		}
	}
	return true;
}
void naEnvironment::UpdateSectorFitness(const u8 sector)
{
	// FIrst of all update each fitness for the current sector. 
	m_BackGroundTrees[sector].bushesFitness = (f32)(MAX_FB_SECTORS * m_BackGroundTrees[sector].numBushes)/(f32)g_MaxInterestingLocalTrees; 
	m_BackGroundTrees[sector].bushesFitness *= m_BackGroundTreesFitness[AUD_BUSHES]; 
	m_BackGroundTrees[sector].treesFitness = (f32)(MAX_FB_SECTORS * m_BackGroundTrees[sector].numTrees)/(f32)g_MaxInterestingLocalTrees; 
	m_BackGroundTrees[sector].treesFitness *= m_BackGroundTreesFitness[AUD_SMALL_TREES]; 
	m_BackGroundTrees[sector].bigTreesFitness = (f32)(MAX_FB_SECTORS * m_BackGroundTrees[sector].numBigTrees)/(f32)g_MaxInterestingLocalTrees; 
	m_BackGroundTrees[sector].bigTreesFitness *= m_BackGroundTreesFitness[AUD_BIG_TREES]; 
	// update current sector fitness
	switch((audTreeTypes)m_BackGroundTreesSounds[m_CurrentSoundIdx[sector]][sector].type)
	{
	case AUD_BUSHES:
		m_BackGroundTreesSounds[m_CurrentSoundIdx[sector]][sector].fitness = m_BackGroundTrees[sector].bushesFitness;
		break;
	case AUD_SMALL_TREES:
		m_BackGroundTreesSounds[m_CurrentSoundIdx[sector]][sector].fitness = m_BackGroundTrees[sector].treesFitness;
		break;
	case AUD_BIG_TREES:
		m_BackGroundTreesSounds[m_CurrentSoundIdx[sector]][sector].fitness = m_BackGroundTrees[sector].bigTreesFitness;
		break;
	default:
		break;
	}
}
void naEnvironment::PlayBgTreesWind(const u8 sector, const u8 soundIdx, const u8 newSoundIdx,const audMetadataRef soundRef,const u8 type,const f32 /*fitness*/,const f32 avgDistance,const u32 numTrees,Vec3V_In soundDir,bool change)
{
	if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor)
	{
		return;
	}
	u32 currentIdx = !change ? soundIdx : newSoundIdx;
	//m_BackGroundTreesFitness[type] *= g_FitnessReduced;
	//m_BackGroundTreesSounds[soundIdx][sector].fitness = fitness;
	m_BackGroundTreesSounds[currentIdx][sector].type = (audTreeTypes)type;

	f32 playerInteriorRatio = 0.0f;
	audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
	playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);
	f32 linVol = m_NumTreesToVolume[type].CalculateValue((f32)numTrees) * m_TreesDistanceToVolume[type].CalculateValue(avgDistance) * (1.f - playerInteriorRatio);
	m_BackGroundTreesSounds[currentIdx][sector].volSmoother.SetRate(g_VolSmootherRate);
	linVol = m_BackGroundTreesSounds[currentIdx][sector].volSmoother.CalculateValue(linVol,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	BANK_ONLY(m_BackGroundTreesSounds[currentIdx][sector].linVol = linVol;)
	f32 vol = audDriverUtil::ComputeDbVolumeFromLinear(linVol);
	if(vol > g_SilenceVolume)
	{
		audSoundInitParams initParams;
		initParams.Volume = vol;
		initParams.Position = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPositionLastFrame() + soundDir * ScalarV(sm_TreesSoundDist));

		if(!change)
		{
			g_WeatherAudioEntity.CreateAndPlaySound_Persistent(soundRef,&m_BackGroundTreesSounds[soundIdx][sector].sound,&initParams);
		}
		else
		{
			g_WeatherAudioEntity.CreateAndPlaySound_Persistent(soundRef,&m_BackGroundTreesSounds[newSoundIdx][sector].sound,&initParams);
			//Stop the old one 
			if( m_BackGroundTreesSounds[soundIdx][sector].sound)
			{
				m_BackGroundTreesSounds[soundIdx][sector].sound->StopAndForget();
				BANK_ONLY(m_BackGroundTreesSounds[soundIdx][sector].linVol = 0;)
			}
			m_BackGroundTreesSounds[soundIdx][sector].volSmoother.Reset();

			m_CurrentSoundIdx[sector] = newSoundIdx;
		}
	}
}
void naEnvironment::UpdateBgTreeSound(const u8 soundIdx, const u8 sector, const u32 numTrees,const u8 type, const f32 avgDistance,Vec3V_In soundDir)
{
	if (m_BackGroundTreesSounds[soundIdx][sector].sound)
	{
		f32 playerInteriorRatio = 0.0f;
		audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
		playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);
		f32 linVol = m_NumTreesToVolume[type].CalculateValue((f32)numTrees) * m_TreesDistanceToVolume[type].CalculateValue(avgDistance) * (1.f - playerInteriorRatio);
		f32 volSpike = 0.f;
		if(g_WeatherAudioEntity.IsGustInProgress())
		{
			Vec3V pos = m_BackGroundTreesSounds[soundIdx][sector].sound->GetRequestedPosition();
			Vector3 dirToGust =  g_WeatherAudioEntity.GetGustMiddlePoint() - VEC3V_TO_VECTOR3(pos);
			dirToGust.SetZ(0.f);
			Vector3 gustLine =  g_WeatherAudioEntity.GetGustDirection();
			gustLine.RotateZ(90.f*DtoR);
			gustLine.SetZ(0.f);
			dirToGust.Normalize();
			gustLine.Normalize();
			f32 dotProduct = gustLine.Dot(dirToGust);

			if(g_AmbientAudioEntity.IsPlayerInTheCity())
			{
				volSpike = sm_CityGustDotToVolSpikeForBgTrees.CalculateValue(fabs(dotProduct));
			}
			else
			{
				volSpike = sm_CountrySideGustDotToVolSpikeForBgTrees.CalculateValue(fabs(dotProduct));
			}

#if __BANK
			if(sm_DrawBgTreesInfo)
			{
				char text[256];
				formatf(text,sizeof(text),"DOT %f Vol %f", fabs(dotProduct),volSpike);
				grcDebugDraw::Text(pos + Vec3V(0.0f,0.0f,1.6f), Color_white,text);
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pos), VEC3V_TO_VECTOR3(pos) + gustLine *2.f, Color32(255, 0, 0, 255));
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pos), VEC3V_TO_VECTOR3(pos) + dirToGust *2.f, Color32(0, 255, 0, 255));
			}
#endif
		}
		linVol *= audDriverUtil::ComputeLinearVolumeFromDb(volSpike);
		m_BackGroundTreesSounds[soundIdx][sector].volSmoother.SetRate(g_VolSmootherRate);
		linVol = m_BackGroundTreesSounds[soundIdx][sector].volSmoother.CalculateValue(linVol,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		BANK_ONLY(m_BackGroundTreesSounds[soundIdx][sector].linVol = linVol);
		f32 vol = audDriverUtil::ComputeDbVolumeFromLinear(linVol);
		if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			vol = g_SilenceVolume;
		}
		if(vol > g_SilenceVolume)
		{
			m_BackGroundTreesSounds[soundIdx][sector].sound->SetRequestedVolume(vol);
#if __BANK 
			if(sm_MuteBgFL && sector == AUD_SECTOR_FL)
			{
				m_BackGroundTreesSounds[soundIdx][sector].sound->SetRequestedVolume(-100.f);
				m_BackGroundTreesSounds[soundIdx][sector].linVol = 0.f;
			}
			if(sm_MuteBgFR && sector == AUD_SECTOR_FR)
			{
				m_BackGroundTreesSounds[soundIdx][sector].sound->SetRequestedVolume(-100.f);
				m_BackGroundTreesSounds[soundIdx][sector].linVol = 0.f;
			}
			if(sm_MuteBgRL && sector == AUD_SECTOR_RL)
			{
				m_BackGroundTreesSounds[soundIdx][sector].sound->SetRequestedVolume(-100.f);
				m_BackGroundTreesSounds[soundIdx][sector].linVol = 0.f;
			}
			if(sm_MuteBgRR && sector == AUD_SECTOR_RR)
			{
				m_BackGroundTreesSounds[soundIdx][sector].sound->SetRequestedVolume(-100.f);
				m_BackGroundTreesSounds[soundIdx][sector].linVol = 0.f;
			}

#endif
			m_BackGroundTreesSounds[soundIdx][sector].sound->SetRequestedPosition(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPositionLastFrame() + soundDir * ScalarV(sm_TreesSoundDist)));
		}
		else 
		{
			m_BackGroundTreesSounds[soundIdx][sector].sound->StopAndForget();
		}
	}
}
void naEnvironment::GetWindTreeSectorInfo(audMetadataRef &soundRef, const u8 sector, audTreeTypes &type,f32 &fitness,f32 &avgDistance,Vec3V_InOut soundDir,u32 &numTrees)
{
	if( m_BackGroundTrees[sector].bushesFitness > 0.f)
	{
		type = AUD_BUSHES;
		fitness = m_BackGroundTrees[sector].bushesFitness;
		soundRef = m_WindBushesSoundset.Find(ATSTRINGHASH("Background1", 0x6FE7F076));
	}
	if ((m_BackGroundTrees[sector].treesFitness > 0.f) && (m_BackGroundTrees[sector].treesFitness > fitness))
	{
		type = AUD_SMALL_TREES;
		fitness = m_BackGroundTrees[sector].treesFitness;
		soundRef = m_WindTreesSoundset.Find(ATSTRINGHASH("Background1", 0x6FE7F076));
	}
	if(m_BackGroundTrees[sector].bigTreesFitness > 0.f && m_BackGroundTrees[sector].bigTreesFitness > fitness) 
	{
		type = AUD_BIG_TREES;
		fitness = m_BackGroundTrees[sector].bigTreesFitness;
		soundRef = m_WindBigTreesSoundset.Find(ATSTRINGHASH("Background1", 0x6FE7F076));

	}
	audSoundInitParams initParams;
	switch((audFBSectors)sector)
	{
	case AUD_SECTOR_FL : 
		//north west
		soundDir = Vec3V(-0.707f,0.707f,0.0f);
		break;
	case AUD_SECTOR_FR : 
		// north east
		soundDir = Vec3V(0.707f,0.707f,0.0f);
		break;
	case AUD_SECTOR_RL : 
		// south west
		soundDir = Vec3V(-0.707f,-0.707f,0.0f);
		break;
	case AUD_SECTOR_RR : 
		// south east
		soundDir = Vec3V(0.707f,-0.707f,0.0f);
		break;
	default:
		break;
	}
	m_BackGroundTreesFitness[type] *= g_FitnessReduced;
	switch((audTreeTypes)type)
	{
	case AUD_BUSHES:
		numTrees = m_BackGroundTrees[sector].numBushes;
		break;
	case AUD_SMALL_TREES:
		numTrees = m_BackGroundTrees[sector].numTrees;
		break;
	case AUD_BIG_TREES:
		numTrees = m_BackGroundTrees[sector].numBigTrees;
		break;
	default:
		break;
	}
	f32 clostestDist[5];//sm_NumTreesForAvgDist
	s8 clostestIdx[5];//sm_NumTreesForAvgDist
	u8 index = 0;
	for ( u8 k= 0; k < sm_NumTreesForAvgDist ; k++)
	{
		clostestDist[k] = LARGE_FLOAT;
		clostestIdx[k] = -1;
	}
	for(u8 treeIdx = 0; treeIdx < m_NumLocalTrees; treeIdx ++)
	{
		if(m_LocalTrees[treeIdx].tree)
		{
			if((m_LocalTrees[treeIdx].sector == (audFBSectors)sector) && (m_LocalTrees[treeIdx].type == type))
			{
				Vec3V treePos2D = m_LocalTrees[treeIdx].tree->GetTransform().GetPosition();
				treePos2D.SetZ(0);
				Vec3V listenerPos2D = g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().GetCol3();
				listenerPos2D.SetZ(0);
				Vec3V dirToTree = Subtract(treePos2D, listenerPos2D);
				f32 distance = fabs(((ScalarV)Mag(dirToTree)).Getf());
				if(index < sm_NumTreesForAvgDist)
				{
					clostestDist[index] = distance;
					clostestIdx[index ++] = treeIdx;
				}
				else
				{
					// replace for the bigger distance
					f32 biggerDist = 0.f;
					s8 biggerIndex = -1;
					for ( u8 k= 0; k < sm_NumTreesForAvgDist ; k++)
					{
						if(clostestDist[k] > biggerDist)
						{
							biggerIndex = k;
							biggerDist = clostestDist[k];
						}
					}
					if (biggerIndex != -1)
					{
						naAssertf(biggerIndex < sm_NumTreesForAvgDist, "wrong index when computing avg distance to trees");
						clostestDist[biggerIndex] = distance;
						clostestIdx[biggerIndex] = treeIdx;
					}
				}
			}
		}
	}
	for ( u8 k= 0; k < index ; k++)
	{
		avgDistance += clostestDist[k];
	}
	avgDistance /= sm_NumTreesForAvgDist;
	m_BackGroundTreesSounds[m_CurrentSoundIdx[sector]][sector].avgDistance = avgDistance;
}

void naEnvironment::UpdateInterestingObjects()
{
#if __BANK 
	u32 numRainObjects = 0;
#endif
	u32 numWindValidIdx = 0;
	s32 windObjectValidIdx[g_MaxInterestingLocalObjects];
	for(u8 i = 0; i < g_MaxInterestingLocalObjects; i ++)
	{
		windObjectValidIdx[i] = -1;
	}
	UpdateShockwaves();
	
	for(s32 i = g_MaxInterestingLocalObjects -1 ; i >=0; i--)
	{
		if(m_InterestingLocalObjects[i].object )
		{
			UpdateResonance(i);
			UpdateRainObject(i);
			if(!audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior() && !m_InterestingLocalObjects[i].object->GetIsInInterior())
			{
				if( audNorthAudioEngine::GetCurrentTimeInMs() > m_NextWindObjectTime )
				{
					const ModelAudioCollisionSettings* material = GetInterestingLocalObjectMaterial(i);
					if(material && material->WindSounds != 0)
					{
						Vec3V pos = m_InterestingLocalObjects[i].object->GetTransform().GetPosition();
						ScalarV distSqd = MagSquared(Subtract(pos,g_AudioEngine.GetEnvironment().GetVolumeListenerPositionLastFrame()));
						if(IsTrue(distSqd >= ScalarV(100)))
						{
							windObjectValidIdx[numWindValidIdx++] = i;			
						}
					}
				}
			}
#if __BANK
			if(m_InterestingLocalObjects[i].rainSound)
			{
				numRainObjects ++;
			}
#endif
		}
	}

	s32 indexToUse = windObjectValidIdx[(u32)audEngineUtil::GetRandomNumberInRange(0.f,(f32)numWindValidIdx)];
	if(indexToUse != -1)
	{
		const ModelAudioCollisionSettings* material = GetInterestingLocalObjectMaterial(indexToUse);
		if(material && material->WindSounds != 0)
		{
			// HL, hacky fix for url:bugstar:6743217 - Beach party : Lots of bottle rolling / rattle assets playing from the bar
			const bool beachPartyZoneActive = CNetwork::IsGameInProgress() && g_AmbientAudioEntity.IsZoneActive(ATSTRINGHASH("AZ_DLC_H4_IH_BEACH_01", 0x17265575));
			const bool suppressPropWindSound = beachPartyZoneActive && (material->WindSounds == ATSTRINGHASH("WIND_PROPS_BOTTLES_RATTLE", 0x6B1C1499) || material->WindSounds == ATSTRINGHASH("WIND_PROPS_BOTTLE_ROLL", 0x9818D730));

			audSoundSet windSounds;
			windSounds.Init(material->WindSounds);
			audMetadataRef soundRef = windSounds.Find(ATSTRINGHASH("WIND_REACTION", 0xD238F5E5));
			if(soundRef != g_NullSoundRef)
			{
				bool found = false;
				s32 index = -1;
				// look in the history : 
				for(u32 i = 0 ; i < g_MaxWindSoundsHistory; i++)
				{
					if(m_WindTriggeredSoundsHistory[i].soundRef == soundRef)
					{
						found = true;
						if(m_WindTriggeredSoundsHistory[i].time + sm_MinTimeToRetrigger > audNorthAudioEngine::GetCurrentTimeInMs() )
						{
							index = i;
						}
						break;
					}
					else if (m_WindTriggeredSoundsHistory[i].soundRef == g_NullSoundRef )
					{
						index = i;
					}
				}
				if (!found && index == -1)
				{
					u32 oldestTime = 0;
					for(u32 i = 0 ; i < g_MaxWindSoundsHistory; i++)
					{
						if (oldestTime <= m_WindTriggeredSoundsHistory[i].time)
						{
							oldestTime = m_WindTriggeredSoundsHistory[i].time;
							index = i;
						}
					}
				}
				if(index != -1)
				{
					if (!suppressPropWindSound)
					{
						audSoundInitParams initParams;
						initParams.Tracker = m_InterestingLocalObjects[indexToUse].object->GetPlaceableTracker();
						initParams.EnvironmentGroup = m_InterestingLocalObjects[indexToUse].envGroup;
						initParams.UpdateEntity = true;
						initParams.u32ClientVar = AUD_ENV_SOUND_WIND;
						g_AmbientAudioEntity.CreateAndPlaySound(soundRef, &initParams);
					}
					
					u32 windTimeVariance = (u32)(sm_WindTimeVariance * g_WeatherAudioEntity.GetWindStrength());
					m_NextWindObjectTime = audNorthAudioEngine::GetCurrentTimeInMs() + (u32)audEngineUtil::GetRandomNumberInRange((f32)(sm_MinWindObjTime - windTimeVariance),(f32)(sm_MaxWindObjtTime - windTimeVariance));
					m_WindTriggeredSoundsHistory[index].soundRef = soundRef;
					m_WindTriggeredSoundsHistory[index].time = audNorthAudioEngine::GetCurrentTimeInMs();
				}
			}
		}
	}
#if __BANK
	if(g_audDrawLocalObjects)
	{
		char txt[128];
		formatf(txt,"NUM RAIN OBJECTS : %u",numRainObjects);
		grcDebugDraw::AddDebugOutput(Color_green,txt);
	}
#endif
	UpdateInterestingTrees();
}
void naEnvironment::UpdateInterestingTrees()
{
	m_BackGroundTreesFitness[AUD_BIG_TREES] = sm_BigTreesFitness;
	m_BackGroundTreesFitness[AUD_SMALL_TREES] = sm_TreesFitness;
	m_BackGroundTreesFitness[AUD_BUSHES] = sm_BushesFitness;
	
	for (u8 i = 0; i < MAX_FB_SECTORS; i ++)
	{
		UpdateSectorFitness(i);

		audMetadataRef soundRef = g_NullSoundRef; 
		audTreeTypes type = AUD_BUSHES;
		f32 fitness = 0.f;
		f32 avgDistance = 0.f;
		u32 numTrees = 0;
		Vec3V soundDir = Vec3V(V_ZERO);
		GetWindTreeSectorInfo(soundRef,i,type,fitness,avgDistance,soundDir,numTrees);

		u8 nextIndex = (m_CurrentSoundIdx[i] + 1) & 1;
		if( !m_BackGroundTreesSounds[m_CurrentSoundIdx[i]][i].sound && !m_BackGroundTreesSounds[nextIndex][i].sound)
		{
			// if we are not playing any sound for the current sector yet, play it if we have to.
			PlayBgTreesWind(i,m_CurrentSoundIdx[i],nextIndex,soundRef,(u8)type,fitness,avgDistance,numTrees,soundDir,false);
		}
		else if (m_BackGroundTreesSounds[m_CurrentSoundIdx[i]][i].sound)
		{
			if( (type != m_BackGroundTreesSounds[m_CurrentSoundIdx[i]][i].type) && (fitness > m_BackGroundTreesSounds[m_CurrentSoundIdx[i]][i].fitness))
			{
				PlayBgTreesWind(i,m_CurrentSoundIdx[i],nextIndex,soundRef,(u8)type,fitness,avgDistance,numTrees,soundDir,true);
			}
		}
		UpdateBgTreeSound(m_CurrentSoundIdx[i], i,numTrees,(u8)type,avgDistance,soundDir);
	}
#if __BANK
	if(sm_DrawBgTreesInfo)
	{
		for (u32 i = 0; i < m_NumLocalTrees; i ++)
		{
			if(m_LocalTrees[i].tree)
			{
				grcDebugDraw::Sphere(m_LocalTrees[i].tree->GetTransform().GetPosition(),2.f,Color32(255,0,0,128));

				char text[256];
				formatf(text,sizeof(text),"Size %f", m_LocalTrees[i].tree->GetBoundRadius());
				grcDebugDraw::Text(m_LocalTrees[i].tree->GetTransform().GetPosition()+ Vec3V(0.0f,0.0f,1.6f), Color_white,text);
			}
		}
		static audMeterList meterLists[8];
		static const char *namesList[24][1]={{"FL0: B"},{"FL0: ST"},{"FL0: T"},{"FL1: B"},{"FL1: ST"},{"FL1: T"},
											{"FR0: B"},{"FR0: ST"},{"FR0: T"},{"FR1: B"},{"FR1: ST"},{"FR1: T"},
											{"RL0: B"},{"RL0: ST"},{"RL0: T"},{"RL1: B"},{"RL1: ST"},{"RL1: T"},
											{"RR0: B"},{"RR0: ST"},{"RR0: T"},{"RR1: B"},{"RR1: ST"},{"RR1: T"}};
		for (u32 i = 0; i < MAX_FB_SECTORS; i ++)
		{
			char text[256];
			formatf(text,sizeof(text),"SECTOR : %u, Dist :%f [BUSHES %u] [TREES:%u] [BIG TREES %u]", i, m_BackGroundTreesSounds[m_CurrentSoundIdx[i]][i].avgDistance, m_BackGroundTrees[i].numBushes,m_BackGroundTrees[i].numTrees,m_BackGroundTrees[i].numBigTrees);
			grcDebugDraw::AddDebugOutput(Color_green,text);
			
			if( m_BackGroundTreesSounds[0][i].sound)
			{
				grcDebugDraw::Sphere(m_BackGroundTreesSounds[0][i].sound->GetRequestedPosition(),2.f,Color32(0,0,255,128));
				char text[256];
				formatf(text,sizeof(text),"[Sector: %u] [Sound %s] [Volume %f]",i, g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_BackGroundTreesSounds[0][i].sound->GetNameTableOffset(),0),m_BackGroundTreesSounds[0][i].sound->GetRequestedVolume());
				grcDebugDraw::AddDebugOutput(Color_white,text);
			}
			
			if( m_BackGroundTreesSounds[1][i].sound)
			{
				grcDebugDraw::Sphere(m_BackGroundTreesSounds[1][i].sound->GetRequestedPosition(),2.f,Color32(0,0,255,128));
				char text[256];
				formatf(text,sizeof(text),"[Sector: %u] [Sound %s] [Volume %f]",i, g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_BackGroundTreesSounds[1][i].sound->GetNameTableOffset(),0),m_BackGroundTreesSounds[1][i].sound->GetRequestedVolume());
				grcDebugDraw::AddDebugOutput(Color_white,text);
			}
			meterLists[i*2].left = 90.f;
			meterLists[i*2].width = 50.f;
			meterLists[i*2].bottom = 350.f + 100.f * i;
			meterLists[i*2].height = 50.f;
			meterLists[i*2].names = namesList[i*6 + m_BackGroundTreesSounds[0][i].type];
			meterLists[i*2].values = &m_BackGroundTreesSounds[0][i].linVol;
			meterLists[i*2].numValues = 1;
			audNorthAudioEngine::DrawLevelMeters(&meterLists[i*2]);

			meterLists[i*2 + 1].left = 200.f;
			meterLists[i*2 + 1].width = 50.f;
			meterLists[i*2 + 1].bottom = 350.f + 100.f *i;
			meterLists[i*2 + 1].height = 50.f;
			meterLists[i*2 + 1].names = namesList[i*6 + MAX_AUD_TREE_TYPES + m_BackGroundTreesSounds[1][i].type];
			meterLists[i*2 + 1].values = &m_BackGroundTreesSounds[1][i].linVol;
			meterLists[i*2 + 1].numValues = 1;
			audNorthAudioEngine::DrawLevelMeters(&meterLists[i*2 + 1]);
		}
	}
#endif
}
bool naEnvironment::ShouldTriggerWindGustEnd()
{
	Vec3V gustPos2D = VECTOR3_TO_VEC3V(g_WeatherAudioEntity.GetGustMiddlePoint());
	gustPos2D.SetZ(0);
	Vec3V listenerPos2D = g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().GetCol3();
	listenerPos2D.SetZ(0);
	Vec3V dirToGust = Subtract(gustPos2D, listenerPos2D);
	dirToGust = Normalize(dirToGust);
	float frontDot = Dot( Vec3V(0.0f,1.f,0.0f), dirToGust).Getf();
	float rightDot = Dot( Vec3V(1.f,0.0f,0.0f), dirToGust).Getf();

	audFBSectors sector = AUD_SECTOR_FL;

	if((frontDot >= 0.f))
	{
		if(rightDot >= 0.f)
			sector = AUD_SECTOR_FR;
		else
			sector = AUD_SECTOR_FL; 
	}
	else
	{
		if(rightDot >= 0.f)
			sector = AUD_SECTOR_RR; 
		else
			sector = AUD_SECTOR_RL; 
	}
	u32 numTreesInSector = m_BackGroundTrees[sector].numBigTrees + m_BackGroundTrees[sector].numTrees + m_BackGroundTrees[sector].numBushes;
	if( numTreesInSector >= sm_MinNumberOfTreesToTriggerGustEnd )
	{
		return true;
	}
	return false;
}
s32 naEnvironment::AllocateShockwave()
{
	for(s32 i = 0; i < kMaxRegisteredShockwaves; i++)
	{
		if(m_RegisteredShockwavesAllocation.IsClear(i))
		{
			m_RegisteredShockwavesAllocation.Set(i);
			return i;
		}
	}
	return kInvalidShockwaveId;
}

void naEnvironment::FreeShockwave(const s32 shockwaveId)
{
	naAssertf(shockwaveId != kInvalidShockwaveId, "Invalid shockwave id (%d) passed into FreeShockwave", shockwaveId);
	m_RegisteredShockwavesAllocation.Clear(shockwaveId);
}

void naEnvironment::UpdateShockwave(s32 &shockwaveId, const audShockwave &shockwaveData)
{
	if(shockwaveId == kInvalidShockwaveId)
	{
		shockwaveId = AllocateShockwave();
	}
	else
	{
		audAssertf(m_RegisteredShockwavesAllocation.IsSet(shockwaveId), "Updating a shockwave but we don't have one registered!");
	}

	if(shockwaveId != kInvalidShockwaveId && shockwaveData.intensity > 0.f)
	{
		m_RegisteredShockwaves[shockwaveId] = shockwaveData;
		m_RegisteredShockwaves[shockwaveId].lastUpdateTime = fwTimer::GetTimeInMilliseconds();
	}
	else if(shockwaveId != kInvalidShockwaveId)
	{
		FreeShockwave(shockwaveId);
		shockwaveId = kInvalidShockwaveId;
	}
}

void naEnvironment::UpdateShockwaves()
{
	f32 objectShockwaveVolumes[g_MaxInterestingLocalObjects];
	sysMemSet(&objectShockwaveVolumes, 0, sizeof(objectShockwaveVolumes));

	const f32 timeStep = fwTimer::GetTimeStep();
	for(u32 shockwaveId = 0; shockwaveId < kMaxRegisteredShockwaves; shockwaveId++)
	{
		if(m_RegisteredShockwavesAllocation.IsSet(shockwaveId))
		{
			m_RegisteredShockwaves[shockwaveId].delay -= timeStep;
			if(m_RegisteredShockwaves[shockwaveId].delay <= 0.f)
			{
				// update shockwave radius and intensity
				m_RegisteredShockwaves[shockwaveId].radius += m_RegisteredShockwaves[shockwaveId].radiusChangeRate * timeStep;
				m_RegisteredShockwaves[shockwaveId].intensity += m_RegisteredShockwaves[shockwaveId].intensityChangeRate * timeStep;
			}

			// compute per object shockwave volumes
			for(s32 objId = g_MaxInterestingLocalObjects -1 ; objId >= 0; objId--)
			{
				if(m_InterestingLocalObjects[objId].object && GetInterestingLocalObjectMaterial(objId) && GetInterestingLocalObjectMaterial(objId)->ShockwaveSound != g_NullSoundHash)
				{
					Vec3V objectPos = m_InterestingLocalObjects[objId].object->GetTransform().GetPosition();
					if(m_InterestingLocalObjects[objId].object->GetCurrentPhysicsInst())
					{
						objectPos = m_InterestingLocalObjects[objId].object->GetCurrentPhysicsInst()->GetCenterOfMass();
					}
					const f32 distToShockwaveCentre = (VEC3V_TO_VECTOR3(objectPos) - m_RegisteredShockwaves[shockwaveId].centre).Mag();
					f32 distToShockwave = distToShockwaveCentre * m_RegisteredShockwaves[shockwaveId].distanceToRadiusFactor;
					f32 linVol = 1.f - Min((distToShockwave/m_RegisteredShockwaves[shockwaveId].radius), 1.f);
					linVol *= m_RegisteredShockwaves[shockwaveId].intensity;
					 
					if (!m_RegisteredShockwaves[shockwaveId].centered)
					{
						distToShockwave = Abs((distToShockwaveCentre - m_RegisteredShockwaves[shockwaveId].radius)) * m_RegisteredShockwaves[shockwaveId].distanceToRadiusFactor;
						linVol = m_RegisteredShockwaves[shockwaveId].intensity * g_ShockwaveDistToVol.CalculateValue(distToShockwave);
					}

					const f32 attackTime = m_InterestingLocalObjects[objId].shockwaveSoundInfo.attackTime;
					const f32 releaseTime = m_InterestingLocalObjects[objId].shockwaveSoundInfo.releaseTime;

					if (attackTime >= 0.f && releaseTime >= 0.f)
					{
						objectShockwaveVolumes[objId] = Max(ProcessResoVolume(linVol, attackTime, releaseTime, m_InterestingLocalObjects[objId].shockwaveSoundInfo.prevVol), objectShockwaveVolumes[objId]);
						m_InterestingLocalObjects[objId].shockwaveSoundInfo.prevVol = objectShockwaveVolumes[objId];
					}
					else
					{
						objectShockwaveVolumes[objId] = Max(linVol, objectShockwaveVolumes[objId]);
					}					
				}
				if(m_InterestingLocalObjects[objId].object && GetInterestingLocalObjectMaterial(objId) && GetInterestingLocalObjectMaterial(objId)->EntityResonance != g_NullSoundHash)
				{
					if (!m_RegisteredShockwaves[shockwaveId].centered)
					{
						Vec3V objectPos = m_InterestingLocalObjects[objId].object->GetTransform().GetPosition();
						if(m_InterestingLocalObjects[objId].object->GetCurrentPhysicsInst())
						{
							objectPos = m_InterestingLocalObjects[objId].object->GetCurrentPhysicsInst()->GetCenterOfMass();
						}
						const f32 distToShockwaveCentre = (VEC3V_TO_VECTOR3(objectPos) - m_RegisteredShockwaves[shockwaveId].centre).Mag();
						f32 distToShockwave = Abs((distToShockwaveCentre - m_RegisteredShockwaves[shockwaveId].radius)) * m_RegisteredShockwaves[shockwaveId].distanceToRadiusFactor;
						f32 linVol = m_RegisteredShockwaves[shockwaveId].intensity * g_ShockwaveDistToVol.CalculateValue(distToShockwave);
						const f32 attackTime = m_InterestingLocalObjects[objId].resoSoundInfo.attackTime;
						const f32 releaseTime = m_InterestingLocalObjects[objId].resoSoundInfo.releaseTime;
						m_InterestingLocalObjects[objId].resoSoundInfo.resonanceIntensity = ProcessResoVolume(linVol,attackTime,releaseTime,m_InterestingLocalObjects[objId].resoSoundInfo.resonanceIntensity);
					}
				}
			}
			// player shockwave

			if(audNorthAudioEngine::ShouldTriggerPulseHeadset() && !m_RegisteredShockwaves[shockwaveId].centered)
			{
				CPed* pPlayer = CGameWorld::FindLocalPlayer();
				if(pPlayer)
				{

					const f32 distToShockwaveCentre = (VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) - m_RegisteredShockwaves[shockwaveId].centre).Mag();
					f32 distToShockwave = Abs((distToShockwaveCentre - m_RegisteredShockwaves[shockwaveId].radius)) * m_RegisteredShockwaves[shockwaveId].distanceToRadiusFactor;
					f32 intensity = m_RegisteredShockwaves[shockwaveId].intensity * g_ShockwaveDistToVol.CalculateValue(distToShockwave);
					if(distToShockwave <= 5.f)
					{
						audSoundInitParams initParams;
						initParams.SetVariableValue(ATSTRINGHASH("Intensity", 0xEFCD993D),intensity);
						g_FrontendAudioEntity.CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("ExplosionShockwave", 0xAAB11A2A)),&initParams);
					}
				}
			}


			if((m_RegisteredShockwaves[shockwaveId].radiusChangeRate > 0.0f && m_RegisteredShockwaves[shockwaveId].radius > m_RegisteredShockwaves[shockwaveId].maxRadius) || 
			   (m_RegisteredShockwaves[shockwaveId].intensityChangeRate < 0.0f && m_RegisteredShockwaves[shockwaveId].intensity <= 0.1f))
			{
				m_RegisteredShockwavesAllocation.Clear(shockwaveId);
			}
		}
	}
	
	for(s32 objId = g_MaxInterestingLocalObjects -1 ; objId >= 0; objId--)
	{
		if(m_InterestingLocalObjects[objId].object && GetInterestingLocalObjectMaterial(objId)->ShockwaveSound != g_NullSoundHash)
		{
			Vec3V objectPos = m_InterestingLocalObjects[objId].object->GetTransform().GetPosition();
			if(m_InterestingLocalObjects[objId].object->GetCurrentPhysicsInst())
			{
				objectPos = m_InterestingLocalObjects[objId].object->GetCurrentPhysicsInst()->GetCenterOfMass();
			}
			const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(objectShockwaveVolumes[objId]);
			if(dbVol > g_SilenceVolume)
			{
				fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(m_InterestingLocalObjects[objId].object, (s32)objId);
				g_AmbientAudioEntity.RegisterEffectAudio(GetInterestingLocalObjectMaterial(objId)->ShockwaveSound, effectUniqueID, VEC3V_TO_VECTOR3(objectPos), m_InterestingLocalObjects[objId].object, dbVol);
			}
		}
	}
}
void naEnvironment::UpdateResonance(const u32 objectIndex)
{
	const ModelAudioCollisionSettings* material = GetInterestingLocalObjectMaterial(objectIndex);
	if(material && material->EntityResonance != g_NullSoundHash)
	{
		const f32 attackTime = m_InterestingLocalObjects[objectIndex].resoSoundInfo.attackTime;
		const f32 releaseTime = m_InterestingLocalObjects[objectIndex].resoSoundInfo.releaseTime;
		m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceIntensity = ProcessResoVolume(0.f,attackTime,releaseTime,m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceIntensity);
		f32 resoLinVol = m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceIntensity;

		UpdateMichaelSpeacialAbilityForResoObjects(objectIndex,resoLinVol);

		if(resoLinVol > 0.001f)
		{
			const bool resonanceLoopTracksListener = (AUD_GET_TRISTATE_VALUE(material->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_RESONANCELOOPTRACKSLISTENER) == AUD_TRISTATE_TRUE) BANK_ONLY(|| g_ForceResonanceTracksListener);

			Vec3V objectPos = m_InterestingLocalObjects[objectIndex].object->GetTransform().GetPosition();
			if(m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst())
			{
				objectPos = m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst()->GetCenterOfMass();
			}
			if(!m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_InterestingLocalObjects[objectIndex].envGroup;

				if (!resonanceLoopTracksListener)
				{
					initParams.Position = VEC3V_TO_VECTOR3(objectPos);
					initParams.Tracker = m_InterestingLocalObjects[objectIndex].object->GetPlaceableTracker();
				}

				const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(resoLinVol);
				initParams.Volume = dbVol;
				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(m_InterestingLocalObjects[objectIndex].resoSoundInfo.resoSoundRef, &m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound, &initParams);
			}
			if(m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound)
			{
				const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(resoLinVol);
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound->SetRequestedVolume(dbVol);

				if (resonanceLoopTracksListener)
				{
					spdAABB boundingBox = m_InterestingLocalObjects[objectIndex].object->GetBaseModelInfo()->GetBoundingBox();
					const Mat34V objectMatrix = m_InterestingLocalObjects[objectIndex].object->GetMatrix();
					const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition();
					const Vec3V objectRelativePos = UnTransformOrtho(m_InterestingLocalObjects[objectIndex].object->GetMatrix(), listenerPos);
					const Vec3V closestPosition = FindClosestPointOnAABB(boundingBox, objectRelativePos);
					const Vec3V finalPosition = Transform(objectMatrix, closestPosition);
					m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound->SetRequestedPosition(finalPosition);
				}
			}
		}
		else
		{
			if(m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound)
			{
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound->StopAndForget();
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound = NULL;
			}
		}
#if __BANK
		if(sm_DisplayResoIntensity && m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound )
		{
			Vec3V objectPos = m_InterestingLocalObjects[objectIndex].object->GetTransform().GetPosition();
			if(m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst())
			{
				objectPos = m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst()->GetCenterOfMass();
			}
			char txt[64];
			formatf(txt, "[Intensity : %f] [Vol: %f] ", m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceIntensity,m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceSound->GetRequestedVolume());
			grcDebugDraw::Text(objectPos,Color_white,txt);
		}
#endif
	}
}

void naEnvironment::UpdateMichaelSpeacialAbilityForResoObjects(const u32 objectIndex,f32 &resoLinVol)
{
	bool failed = true;
	CPed *pPlayer = CGameWorld::FindLocalPlayer();
	if( pPlayer &&  (g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMichael ||  g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMultiplayer))
	{
		const CPlayerInfo* playerInfo = pPlayer->GetPlayerInfo();
		if(playerInfo)
		{
			const CEntity* target = playerInfo->GetTargeting().GetTarget();
			if(target && (target == m_InterestingLocalObjects[objectIndex].object) )
			{
				Vec3V objectPos = target->GetTransform().GetPosition();
				if(target->GetCurrentPhysicsInst())
				{
					objectPos = target->GetCurrentPhysicsInst()->GetCenterOfMass();
				}
				failed = false;
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin = 1.f;
				// Workout the intensity based on the angular distance to the camera front.
				Vector3 front = camInterface::GetFront();
				front.SetZ(0.f);
				front.Normalize();
				Vector3 cameraPos = camInterface::GetPos();
				Vector3 dirToObject = VEC3V_TO_VECTOR3(objectPos) - cameraPos;
				dirToObject.SetZ(0);
				dirToObject.Normalize();
				f32 horizontalDot = front.Dot(dirToObject);

				m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin *= sm_HDotToSlowmoResoVol.CalculateValue(horizontalDot);
				//naDisplayf("HDot %f V %f ",horizontalDot,slomoResoIntensity);

				front = camInterface::GetUp();
				front.SetY(0.f);
				front.Normalize();
				cameraPos = camInterface::GetPos();
				dirToObject = VEC3V_TO_VECTOR3(objectPos) - cameraPos;
				dirToObject.SetY(0);
				dirToObject.Normalize();
				f32 verticalDot = front.Dot(dirToObject);
				verticalDot *= (verticalDot >=0.f ? -1.f : 1.f);
				verticalDot += 1.f;
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin *= sm_VDotToSlowmoResoVol.CalculateValue(verticalDot);
				//naDisplayf("VDot %f V %f",verticalDot,slomoResoIntensity);

				f32 frequency = sm_SpecialAbToFrequency.CalculateValue(pPlayer->GetSpecialAbility()->GetRemainingNormalized());//,sm_SlowMoLFOFrequencyMin,sm_SlowMoLFOFrequencyMax);
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoPhase += audNorthAudioEngine::GetTimeStep() * (1.f/frequency);
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoPhase = fmodf(m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoPhase,1);
				m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin *= ((rage::Sin(ScalarV(2.f * PI * m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoPhase)).Getf() + 1 ) * 0.5f);
				// Get the max 
				resoLinVol = Max(resoLinVol,m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin);
			}
		}
	}
	if(failed && m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoPhase != 0.f)  
	{
		m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoPhase = 0.f;
		m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceIntensity = Max(resoLinVol,m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin);
		resoLinVol = m_InterestingLocalObjects[objectIndex].resoSoundInfo.resonanceIntensity;
		m_InterestingLocalObjects[objectIndex].resoSoundInfo.slowmoResoVolLin = 0.f;
	}
}
f32 naEnvironment::ProcessResoVolume(const f32 inputSignal, const f32 attack, const f32 release, const f32 state)
{
	if(naVerifyf(attack * release > 0.f,"Neither attack or release times can't be 0, please check the Reso envelope sound"))
	{
		float prevEnvelope = state;
		const f32 input = Abs(inputSignal);

		if(naVerifyf(fwTimer::GetTimeStep() > 0.f,"Got a 0 time step, aborting reso volume computation."))
		{
			const f32 effectiveSampleRate = 1.f / fwTimer::GetTimeStep();
			// prevent dbz
			const f32 attackTime = Max(0.00001f, attack);
			const f32 releaseTime = Max(0.00001f, release);
			const f32 a = Powf(0.01f, 1.0f/(attackTime * effectiveSampleRate)); 
			const f32 r = Powf(0.01f, 1.0f/(releaseTime * effectiveSampleRate)); 

			// envelope >= input ? release : attack
			const f32 factor = Selectf(input - prevEnvelope, a, r);

			// don't allow output to go denormal; clamp to 0
			const f32 unclampedVal = factor * (prevEnvelope - input) + input;
			prevEnvelope = unclampedVal <= 1e-17f ? 0.f : unclampedVal;
			return prevEnvelope;
		}
	}
	return 0.f;
}
void naEnvironment::CalculateUnderCoverFactor()
{
	f32 underCoverFactor = 0.f;
	for (u32 i = 0; i < MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES; i++)
	{
		if(m_CoveredPolys.IsSet(i))
		{
			underCoverFactor += sqrt(m_SourceEnvironmentPolyAreas[i]/PI) * sm_RadiusScale;
		}
	}
	f32 factor = 0.f;
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer)
	{
		// This was added as a way to detect when in a vehicle and going undercover.
		if (audNorthAudioEngine::GetGtaEnvironment()->IsPlayerInVehicle())
		{
			CVehicle* vehicle = pPlayer->GetVehiclePedInside();
			if (vehicle && vehicle->GetAudioEnvironmentGroup())
			{
				if (((naEnvironmentGroup*)vehicle->GetAudioEnvironmentGroup())->IsUnderCover())
				{
					factor = underCoverFactor;
				}
			}
		}
		if (pPlayer->GetAudioEnvironmentGroup())
		{
			if (((naEnvironmentGroup*)pPlayer->GetAudioEnvironmentGroup())->IsUnderCover())
			{
				factor = underCoverFactor;
			}
		}
	}
	m_PlayerUnderCoverFactor = m_PlayerUnderCoverSmoother.CalculateValue(factor,audNorthAudioEngine::GetCurrentTimeInMs());
}
void naEnvironment::CreateRainObjectEnvironmentGroup(const u32 objectIndex)
{
	BANK_ONLY(if(!sm_UseCoverAproximation))		
	{
		Vec3V objectPos = m_InterestingLocalObjects[objectIndex].object->GetTransform().GetPosition();
		if(m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst())
		{
			objectPos = m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst()->GetCenterOfMass();
		}
		if(!m_InterestingLocalObjects[objectIndex].envGroup)
		{
			m_InterestingLocalObjects[objectIndex].envGroup = naEnvironmentGroup::Allocate("InterestingObj"); 
			if(m_InterestingLocalObjects[objectIndex].envGroup) 
			{ 
				m_InterestingLocalObjects[objectIndex].envGroup->Init(NULL, 20.0f,sm_ObjOcclusionUpdateTime); 
				m_InterestingLocalObjects[objectIndex].envGroup->SetSource(objectPos); 
				m_InterestingLocalObjects[objectIndex].envGroup->ForceSourceEnvironmentUpdate(m_InterestingLocalObjects[objectIndex].object); 
				m_InterestingLocalObjects[objectIndex].envGroup->SetUsePortalOcclusion(true); 
				m_InterestingLocalObjects[objectIndex].envGroup->SetUseInteriorDistanceMetrics(true); 
				m_InterestingLocalObjects[objectIndex].envGroup->SetMaxPathDepth(2); 
				m_InterestingLocalObjects[objectIndex].envGroup->AddSoundReference(); 
			} 
		}
	}
#if __BANK
	else
	{
		m_InterestingLocalObjects[objectIndex].underCover = false;
		for (u32 i = 0; i < MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES; i++)
		{
			if(m_CoveredPolys.IsSet(i))
			{
				f32 distToObj = fabs((VEC3V_TO_VECTOR3(m_InterestingLocalObjects[objectIndex].object->GetTransform().GetPosition()) - m_SourceEnvironmentPolyCentroids[i]).Mag());
				f32 approximatedRadius = sqrt(m_SourceEnvironmentPolyAreas[i]/PI) * sm_RadiusScale;
				if(distToObj < approximatedRadius)
				{
					m_InterestingLocalObjects[objectIndex].underCover = true;
					break;
				}
			}
		}
	}
#endif
}
void naEnvironment::UpdateRainObject(const u32 objectIndex)
{	
	if (g_weather.GetTimeCycleAdjustedRain() != 0 && m_InterestingLocalObjects[objectIndex].object && !audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
	{
		Vec3V objectPos = m_InterestingLocalObjects[objectIndex].object->GetTransform().GetPosition();
		if (m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst())
		{
			objectPos = m_InterestingLocalObjects[objectIndex].object->GetCurrentPhysicsInst()->GetCenterOfMass();
		}

		const ModelAudioCollisionSettings* material = GetInterestingLocalObjectMaterial(objectIndex);

		const bool rainLoopTracksListener = (AUD_GET_TRISTATE_VALUE(material->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_RAINLOOPTRACKSLISTENER) == AUD_TRISTATE_TRUE) BANK_ONLY(|| g_ForceRainLoopTracksListener);
		const bool placeRainLoopOnTopOfBounds = (AUD_GET_TRISTATE_VALUE(material->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_PLACERAINLOOPONTOPOFBOUNDS) == AUD_TRISTATE_TRUE) BANK_ONLY(|| g_ForceRainLoopPlayFromTopOfBounds);

		if (rainLoopTracksListener)
		{
			spdAABB boundingBox = m_InterestingLocalObjects[objectIndex].object->GetBaseModelInfo()->GetBoundingBox();

			if (placeRainLoopOnTopOfBounds)
			{
				Vec3V min = boundingBox.GetMin();
				min.SetZ(boundingBox.GetMax().GetZ());
				boundingBox.SetMin(min);
			}

			const Mat34V objectMatrix = m_InterestingLocalObjects[objectIndex].object->GetMatrix();
			const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition();
			const Vec3V objectRelativePos = UnTransformOrtho(m_InterestingLocalObjects[objectIndex].object->GetMatrix(), listenerPos);
			const Vec3V closestPosition = FindClosestPointOnAABB(boundingBox, objectRelativePos);
			const Vec3V finalPosition = Transform(objectMatrix, closestPosition);
			objectPos = finalPosition;
		}
		else if (placeRainLoopOnTopOfBounds)
		{
			const spdAABB boundingBox = m_InterestingLocalObjects[objectIndex].object->GetBaseModelInfo()->GetBoundingBox();
			const Mat34V objectMatrix = m_InterestingLocalObjects[objectIndex].object->GetMatrix();
			const Vec3V finalPosition = Transform(objectMatrix, Vec3V(ScalarV(V_ZERO), ScalarV(V_ZERO), boundingBox.GetMax().GetZ()));
			objectPos = finalPosition;
		}

		ScalarV sqdDistance = MagSquared(objectPos - g_AudioEngine.GetEnvironment().GetVolumeListenerPositionLastFrame());
		if (sqdDistance.Getf() <= sm_SqdDistanceToPlayRainSounds)
		{			
			if(material && material->RainLoop != g_NullSoundHash)
			{
				bool allowPlay = (m_NumRainPropSounds < sm_MaxNumRainProps);
				if(allowPlay)
				{
					CreateRainObjectEnvironmentGroup(objectIndex);
					if(m_InterestingLocalObjects[objectIndex].envGroup)
					{
						allowPlay = m_InterestingLocalObjects[objectIndex].envGroup->GetLastUpdateTime() != 0;
					}
				}
				if( allowPlay && !m_InterestingLocalObjects[objectIndex].rainSound && !IsObjectUnderCover(objectIndex))
				{
					audSoundInitParams initParams;
					initParams.AttackTime = 1000;
					initParams.EnvironmentGroup = m_InterestingLocalObjects[objectIndex].envGroup;
					initParams.Position = VEC3V_TO_VECTOR3(objectPos);

					if (!rainLoopTracksListener && !placeRainLoopOnTopOfBounds)
					{
						initParams.Tracker = m_InterestingLocalObjects[objectIndex].object->GetPlaceableTracker();
					}

					// By default don't modulate the props rain Vol through code.
					f32 vol = g_WeatherAudioEntity.GetRainVolume();
					if(audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())// || audNorthAudioEngine::GetGtaEnvironment()->IsCameraShelteredFromRain())
					{
						vol = -100.f;
					}

					f32 volLinear = audDriverUtil::ComputeLinearVolumeFromDb(vol);
					f32 smoothedVolLinear = m_InterestingObjectRainSmoother.CalculateValue(volLinear, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
					f32 smoothedVolDb = audDriverUtil::ComputeDbVolumeFromLinear(smoothedVolLinear);
					if(AUD_GET_TRISTATE_VALUE(material->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_TURNOFFRAINVOLONPROPS) != AUD_TRISTATE_TRUE)
					{
						initParams.Volume = smoothedVolDb;
						initParams.Volume += sm_RainOnPropsVolumeOffset;
						initParams.Predelay = sm_RainOnPopsPreDealy;
					}
					m_NumRainPropSounds++;
					g_WeatherAudioEntity.CreateAndPlaySound_Persistent(material->RainLoop, &m_InterestingLocalObjects[objectIndex].rainSound, &initParams);
				}
				else if(IsObjectUnderCover(objectIndex))
				{
					if( m_InterestingLocalObjects[objectIndex].rainSound)
					{
						if (m_NumRainPropSounds > 0)
						{
							m_NumRainPropSounds--;
						}
						m_InterestingLocalObjects[objectIndex].rainSound->StopAndForget();
						m_InterestingLocalObjects[objectIndex].rainSound = NULL;
					}
				}
				if( m_InterestingLocalObjects[objectIndex].rainSound)
				{
					// By default don't modulate the props rain Vol through code.
					f32 vol = g_WeatherAudioEntity.GetRainVolume();
					if(audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
					{
						vol = -100.f;
					}

					f32 volLinear = audDriverUtil::ComputeLinearVolumeFromDb(vol);
					f32 smoothedVolLinear = m_InterestingObjectRainSmoother.CalculateValue(volLinear, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
					f32 smoothedVolDb = audDriverUtil::ComputeDbVolumeFromLinear(smoothedVolLinear);
					if(AUD_GET_TRISTATE_VALUE(material->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_TURNOFFRAINVOLONPROPS) != AUD_TRISTATE_TRUE)
					{
						m_InterestingLocalObjects[objectIndex].rainSound->SetRequestedVolume(smoothedVolDb +sm_RainOnPropsVolumeOffset);
					}

					if (rainLoopTracksListener || placeRainLoopOnTopOfBounds)
					{
						m_InterestingLocalObjects[objectIndex].rainSound->SetRequestedPosition(objectPos); 
					}
				}
			}
		}
		else
		{
			 if(m_InterestingLocalObjects[objectIndex].rainSound)
			 {
				if (m_NumRainPropSounds > 0)
				{
					m_NumRainPropSounds--;
				}
				m_InterestingLocalObjects[objectIndex].rainSound->StopAndForget();
				m_InterestingLocalObjects[objectIndex].rainSound = NULL;
			 }
			 if( m_InterestingLocalObjects[objectIndex].envGroup )
				{
					m_InterestingLocalObjects[objectIndex].envGroup->RemoveSoundReference();
					m_InterestingLocalObjects[objectIndex].envGroup = NULL;
				}
		}
	}
	else 
	{
		if(m_InterestingLocalObjects[objectIndex].rainSound)
		{
			if (m_NumRainPropSounds > 0)
			{
				m_NumRainPropSounds--;
			}
			m_InterestingLocalObjects[objectIndex].rainSound->StopAndForget();
			m_InterestingLocalObjects[objectIndex].rainSound = NULL;
		}
		if( m_InterestingLocalObjects[objectIndex].envGroup )
		{
			m_InterestingLocalObjects[objectIndex].envGroup->RemoveSoundReference();
			m_InterestingLocalObjects[objectIndex].envGroup = NULL;
		}
	}
}
void naEnvironment::UpdateLocalEnvironment()
{
	// JB: I moved this stuff into a separate function so it can be shared whatever async/sync approach we use above.
	UpdateLocalEnvironmentObjects();
}

void naEnvironment::LocalObjectsList_StartAsyncUpdate()
{
	if(g_EnableLocalEnvironmentMetrics && !(fwTimer::GetSystemFrameCount() & 15))
	{
		PF_PUSH_TIMEBAR("LocalObjectsList_StartAsyncUpdate");
		spdSphere searchSphere(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(), ScalarV(30.f));
		fwIsSphereIntersecting searchVol(searchSphere);

		// start spu search
		ms_localObjectSearch.Start(
			fwWorldRepMulti::GetSceneGraphRoot(),								// root scene graph node
			&searchVol,															// search volume
			ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_BUILDING,				// entity types
			SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,				// location flags
			SEARCH_LODTYPE_HIGHDETAIL,											// lod flags
			SEARCH_OPTION_SKIP_PROCEDURAL_ENTITIES,								// option flags
			WORLDREP_SEARCHMODULE_AUDIO,										// module
			sysDependency::kPriorityCritical									// priority
			);

		ms_localObjectSearchLaunched = true;
		PF_POP_TIMEBAR();
	}
}

void naEnvironment::LocalObjectsList_EndAsyncUpdate()
{
	if(ms_localObjectSearchLaunched)
	{
		PF_PUSH_TIMEBAR("LocalObjectsList_EndAsyncUpdate");
		ms_localObjectSearch.Finalize();

		for (u32 i = 0 ; i < m_NumLocalTrees; i ++)
		{
			m_LocalTrees[i].tree = NULL;
			m_LocalTrees[i].sector = AUD_SECTOR_FL;
		}
		m_NumLocalTrees = 0;
		for (u32 i = 0 ; i < MAX_FB_SECTORS; i ++)
		{
			m_BackGroundTrees[i].numBushes = 0;
			m_BackGroundTrees[i].numTrees = 0;
			m_BackGroundTrees[i].numBigTrees = 0;
			m_BackGroundTrees[i].bushesFitness = 0.f;
			m_BackGroundTrees[i].treesFitness = 0.f;
			m_BackGroundTrees[i].bigTreesFitness = 0.f;
		}
		// Before recalculating the new objects, all the current ones are marked up to delete.
		for (u32 i = 0; i < g_MaxInterestingLocalObjects; i ++ )
		{
			m_InterestingLocalObjects[i].stillValid = false;
			if( !m_InterestingLocalObjects[i].object )
			{
				m_InterestingLocalObjects[i].macsComponent = -1;
				if( m_InterestingLocalObjects[i].envGroup )
				{
					m_InterestingLocalObjects[i].envGroup->RemoveSoundReference();
					m_InterestingLocalObjects[i].envGroup = NULL;
				}
				BANK_ONLY(m_InterestingLocalObjects[i].underCover = true);
				if(m_InterestingLocalObjects[i].rainSound)
				{
					m_InterestingLocalObjects[i].rainSound->StopAndForget();
					m_InterestingLocalObjects[i].rainSound = NULL;
					if (m_NumRainPropSounds > 0)
					{
						m_NumRainPropSounds--;
					}
				}
				if (m_InterestingLocalObjects[i].randomAmbientSound)
				{
					m_InterestingLocalObjects[i].randomAmbientSound->StopAndForget();
					m_InterestingLocalObjects[i].randomAmbientSound = NULL;					
				}
				if(m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound)
				{
					m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound->StopAndForget();
					m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound = NULL;
				}
				m_InterestingLocalObjects[i].resoSoundInfo.resonanceIntensity = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.attackTime = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.releaseTime = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.slowmoResoPhase = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.slowmoResoVolLin = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.resoSoundRef = g_NullSoundRef;
			}
			else
			{
				if (m_InterestingLocalObjects[i].randomAmbientSound && IsObjectDestroyed(m_InterestingLocalObjects[i].object))
				{
					m_InterestingLocalObjects[i].randomAmbientSound->StopAndForget();
					m_InterestingLocalObjects[i].randomAmbientSound = NULL;					
				}
			}
		}
		for(u32 i = 0; i < m_NumInterestingLocalObjectsCached; i++)
		{
			m_InterestingLocalObjectsCached[i].object = NULL;
			m_InterestingLocalObjectsCached[i].envGroup = NULL;
			m_InterestingLocalObjectsCached[i].rainSound = NULL;
			m_InterestingLocalObjectsCached[i].windSound = NULL;
			m_InterestingLocalObjectsCached[i].randomAmbientSound  = NULL;
			m_InterestingLocalObjectsCached[i].macsComponent = -1;
			BANK_ONLY(m_InterestingLocalObjectsCached[i].underCover = true);
			m_InterestingLocalObjectsCached[i].resoSoundInfo.resonanceSound = NULL;
			m_InterestingLocalObjectsCached[i].resoSoundInfo.resonanceIntensity = 0.f;
			m_InterestingLocalObjectsCached[i].resoSoundInfo.attackTime = 0.f;
			m_InterestingLocalObjectsCached[i].resoSoundInfo.releaseTime = 0.f;
			m_InterestingLocalObjectsCached[i].resoSoundInfo.slowmoResoVolLin = 0.f;
			m_InterestingLocalObjectsCached[i].resoSoundInfo.slowmoResoPhase = 0.f;
			m_InterestingLocalObjectsCached[i].resoSoundInfo.resoSoundRef = g_NullSoundRef;
		}
		m_NumInterestingLocalObjectsCached = 0;
		ms_localObjectSearch.ExecuteCallbackOnResult(FindCloseObjectsCB, NULL);
		ms_localObjectSearchLaunched = false;
		// Go through the cached objects and remove the occlusion group for those that aren't valid anymore.
		for(u32 i = 0; i < m_NumInterestingLocalObjectsCached; i++)
		{
			bool alreadyAdded = false; 
			s32 freeIdx = -1;
			for (u32 j = 0; j < g_MaxInterestingLocalObjects; j++)
			{
				if(m_InterestingLocalObjectsCached[i].object == m_InterestingLocalObjects[j].object)
				{
					m_InterestingLocalObjects[j].stillValid = true;
					alreadyAdded = true; 
					break;
				}
				else if (!m_InterestingLocalObjects[j].object)
				{
					freeIdx = j;
				}
			}
			if(!alreadyAdded && freeIdx != -1 && freeIdx < g_MaxInterestingLocalObjects)
			{
				m_InterestingLocalObjects[freeIdx] = m_InterestingLocalObjectsCached[i];
				m_InterestingLocalObjects[freeIdx].stillValid = true;
			}
		}
		// Free up the objects that aren't valid.
		for (u32 i = 0; i < g_MaxInterestingLocalObjects; i++)
		{
			if(!m_InterestingLocalObjects[i].stillValid)
			{
				m_InterestingLocalObjects[i].object = NULL;
				m_InterestingLocalObjects[i].macsComponent = -1;
				if( m_InterestingLocalObjects[i].envGroup )
				{
					m_InterestingLocalObjects[i].envGroup->RemoveSoundReference();
					m_InterestingLocalObjects[i].envGroup = NULL;
				}
				BANK_ONLY(m_InterestingLocalObjects[i].underCover = true);
				if(m_InterestingLocalObjects[i].rainSound)
				{
					m_InterestingLocalObjects[i].rainSound->StopAndForget();
					m_InterestingLocalObjects[i].rainSound = NULL;
					if (m_NumRainPropSounds > 0)
					{
						m_NumRainPropSounds--;
					}
				}
				if(m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound)
				{
					m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound->StopAndForget();
					m_InterestingLocalObjects[i].resoSoundInfo.resonanceSound = NULL;
				}
				m_InterestingLocalObjects[i].resoSoundInfo.resonanceIntensity = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.attackTime = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.releaseTime = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.slowmoResoPhase = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.slowmoResoVolLin = 0.f;
				m_InterestingLocalObjects[i].resoSoundInfo.resoSoundRef = g_NullSoundRef;
			}
		}
		PF_POP_TIMEBAR();
	}
}

void naEnvironment::UpdateLocalEnvironmentObjects()
{
	if(g_EnableLocalEnvironmentMetrics)
	{
		//Offset the resonance objects processing from the interesting objects
		if(!((fwTimer::GetSystemFrameCount() + 7) & 15))
		{
			for(u32 i=0; i< g_MaxResonatingObjects; i++)
			{
				//If the sound is no longer playing or the entity has been deleted, clear the entry out
				if(!m_ResonatingObjects[i].entity && !m_ResonatingObjects[i].resoSnd)
				{
					m_ResonatingObjects[i].Clear();
				}
			}	 
		}

		// update city ambiance metrics
		if(!(fwTimer::GetSystemFrameCount() & 31) || m_ForceAmbientMetricsUpdate)
		{
			const Vector3 north = Vector3(0.0f, 1.0f, 0.0f);
			const Vector3 east = Vector3(1.0f,0.0f,0.0f);
			const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());

			for(u32 i = 0 ; i < 4; i++)
			{
				m_VehicleSegmentCounts[i] = 0.f;
				m_HostilePedSegmentCount[i] = 0.f;
				m_ExteriorPedSegmentCounts[i] = 0.f;
				m_InteriorPedSegmentCounts[i] = 0.f;
				m_InteriorOccludedPedSegmentCounts[i] = 0.0f;
				m_ExteriorPedPanicCount[i] = 0.0f;
				m_InteriorPedPanicCount[i] = 0.0f;
			}

			u32 segment = ~0U;
			m_VehicleSegmentCounts[4] = 0.f;
			CVehicle::Pool* vehiclePool = CVehicle::GetPool();

			for(s32 i = 0; i < vehiclePool->GetSize(); i++)
			{
				CVehicle *veh = vehiclePool->GetSlot(i);
				if(veh)
				{
					f32 vehicleAmount = 1.0f;
					const Vector3 pos = VEC3V_TO_VECTOR3(veh->GetTransform().GetPosition());
					Vector3 vehToListener = pos - listenerMatrix.d;
					vehicleAmount *= m_VehicleDistanceSqToVehicleAmount.CalculateValue(vehToListener.Mag2());
					vehicleAmount *= m_VehicleSpeedToVehicleAmount.CalculateValue(veh->GetVehicleAudioEntity()->GetFwdSpeedLastFrame());

					if(vehToListener.Mag2() < (100.f*100.f))
					{
						m_VehicleSegmentCounts[4]+=vehicleAmount;
					}
					else
					{
						vehToListener.Normalize();
						const f32 dp = DotProduct(vehToListener, north);
						const f32 angle = AcosfSafe(dp);
						const f32 degrees = RtoD*angle;
						f32 actualDegrees;
						if(vehToListener.Dot(east) <= 0.0f)
						{
							actualDegrees = 360.0f - degrees;
						}
						else
						{
							actualDegrees = degrees;
						}
						if(segment != ~0U)
						{
							m_VehicleSegmentCounts[segment]+=vehicleAmount;
						}
						// defer using segment to prevent LHS
						segment = static_cast<u32>(((actualDegrees+45.f) / 90.0f)) % 4;
					}

				}
			}
			if(segment != ~0U)
			{
				m_VehicleSegmentCounts[segment]+=1.f;
			}

			fwPool<CPed> *pedPool = CPed::GetPool();
			f32 totalPedZPos = 0.0f;
			u32 totalNumPeds = 0;
			u32 totalMalePeds = 0;

			CInteriorProxy* pIntProxy = CPortalVisTracker::GetPrimaryInteriorProxy();
			s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
			
			spdAABB roomBoundingBox;
			roomBoundingBox.Invalidate();

			if (pIntProxy)
			{
				CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
				if (pIntInst && roomIdx>0 && roomIdx!=INTLOC_INVALID_INDEX)
				{
					if(roomIdx < pIntInst->GetNumRooms())
					{
						pIntInst->GetRoomBoundingBox(roomIdx, roomBoundingBox);
					}
				}
			}

			for(s32 i = 0; i < pedPool->GetSize(); i++)
			{
				CPed *ped = pedPool->GetSlot(i);

				// Don't count law enforcement peds, otherwise we'll start getting eg. loads of walla during 5* wanted level shootouts
				// Also don't count animals, for obvious reasons
				if(ped && !ped->IsLocalPlayer() && ped->GetHealth() > 0.0f && ped->GetPedModelInfo()->GetPersonalitySettings().GetIsHuman())
				{
					const Vector3 pos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
					Vector3 pedToListener = pos - listenerMatrix.d;

					// Bias against from peds at different heights to the listener when in an interior
					if(ped->GetIsInInterior())
					{
						pedToListener.SetZ(pedToListener.GetZ() * 4.0f);
					}

					if (pedToListener.Mag2() < (60.f*60.f)) // only include peds that are reasonably close
					{
						Vector3 normalisedPedToListener = pedToListener;
						normalisedPedToListener.Normalize();
						const f32 dp = DotProduct(normalisedPedToListener, north);
						const f32 angle = AcosfSafe(dp);
						const f32 degrees = RtoD*angle;
						f32 actualDegrees;
						if(normalisedPedToListener.Dot(east) <= 0.0f)
						{
							actualDegrees = 360.0f - degrees;
						}
						else
						{
							actualDegrees = degrees;
						}
						const u32 segment = static_cast<u32>(((actualDegrees+45.f) / 90.0f)) % 4;

						if(ped->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() != NULL)
						{
							m_HostilePedSegmentCount[segment]+=1.f;
						}
						else if(ped->GetIsInInterior())
						{
							if(ped->GetAudioEntity()->GetEnvironmentGroup())
							{
								f32 occludedPedFactor = 1.0f - (Clamp(ped->GetAudioEntity()->GetEnvironmentGroup()->GetOcclusionValue()/0.5f, 0.0f, 1.0f));

								// Fade off the ped contribution as we approach the edge of the range
								occludedPedFactor *= (1.0f - Clamp((pedToListener.Mag() - 30.0f)/30.0f, 0.0f, 1.0f));
								m_InteriorOccludedPedSegmentCounts[segment] += occludedPedFactor;
								m_InteriorPedSegmentCounts[segment]+=1.f;
							}

							if(ped->GetIsPanicking())
							{
								m_InteriorPedPanicCount[segment]+=1.f;
							}
						}
						else if(ped->GetIsInExterior())
						{
							m_ExteriorPedSegmentCounts[segment]+=1.f;

							if(ped->GetIsPanicking())
							{
								m_ExteriorPedPanicCount[segment]+=1.f;
							}
						}

						if(ped->IsMale())
						{
							totalMalePeds++;
						}

						totalNumPeds++;
						totalPedZPos += pos.z;
					}
				}
			}

			if(totalNumPeds > 0)
			{
				m_AveragePedZPos = totalPedZPos/totalNumPeds;
				m_PedFractionMale = totalMalePeds/(f32)totalNumPeds;
			}

			m_ForceAmbientMetricsUpdate = false;
		}
	}
}

void naEnvironment::UpdateLinkedRoomList(CInteriorInst* intInst)
{
	ClearLinkedRooms();

	if (intInst)
	{
		CMloModelInfo *modelInfo = intInst->GetMloModelInfo();
		s32 roomPortalIdx = intInst->GetNumPortalsInRoom(INTLOC_ROOMINDEX_LIMBO) - 1;

		while(roomPortalIdx >= 0)
		{
			// Need the global portal index in order to get the portal data
			s32 globalPortalIdx = intInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, roomPortalIdx);

			CPortalFlags portalFlags = modelInfo->GetPortalFlags(globalPortalIdx);

			if(portalFlags.GetIsLinkPortal())
			{
				// If this is a link portal, update our list of rooms to not occlude from
				// this is a valid linked portal to another MLO - update tracker accordingly
				// Need to use the room specific portal index in this function
				CPortalInst* portalInst = intInst->GetMatchingPortalInst(INTLOC_ROOMINDEX_LIMBO, roomPortalIdx);
				if(portalInst)
				{
					// update using info from the linked portal
					CInteriorInst* destIntInst = NULL;
					s32	destPortalIdx = -1;

					// get data about linked interior through this portal 
					portalInst->GetDestinationData(intInst, destIntInst, destPortalIdx);
					if(destIntInst)
					{
						// need to get the room index from the new interior and portal
						s32 linkedRoomIdx = destIntInst->GetDestThruPortalInRoom(INTLOC_ROOMINDEX_LIMBO, destPortalIdx);		//entry/exit portal so we know it's room INTLOC_ROOMINDEX_LIMBO
						if(linkedRoomIdx != INTLOC_INVALID_INDEX)
						{
							AddLinkedRoom(destIntInst, linkedRoomIdx);
						}
					}
				}
			}

			roomPortalIdx--;
		}
	}
}

void naEnvironment::AddLinkedRoom(CInteriorInst* intInst, s32 roomIdx)
{
	if(intInst)
	{
		for (u32 i=0; i<AUD_MAX_LINKED_ROOMS; i++)
		{
			if(CInteriorProxy::GetFromLocation(m_LinkedRoom[i]) == intInst->GetProxy() && m_LinkedRoom[i].GetRoomIndex() == roomIdx)
			{
				// already stored this frame
				return;
			}
			else if (!m_LinkedRoom[i].IsValid())
			{
				m_LinkedRoom[i] = CInteriorInst::CreateLocation(intInst, roomIdx);
				break;
			}
		}
	}
}

void naEnvironment::ClearLinkedRooms()
{
	for (u32 i=0; i<AUD_MAX_LINKED_ROOMS; i++)
	{
		m_LinkedRoom[i].MakeInvalid();
	}
}

bool naEnvironment::IsRoomInLinkedRoomList(const InteriorSettings* linkedInteriorSettings, const InteriorRoom* linkedIntRoom)
{
	if(naVerifyf(linkedInteriorSettings && linkedIntRoom, "Querying invalid room in audio check"))
	{
		for (u32 i=0; i<AUD_MAX_LINKED_ROOMS; i++)
		{
			if(m_LinkedRoom[i].IsValid())
			{
				const InteriorSettings* intSettings = NULL;
				const InteriorRoom* intRoom = NULL;
				GetInteriorSettingsForInteriorLocation(m_LinkedRoom[i], intSettings, intRoom);
				if(intSettings == linkedInteriorSettings && intRoom == linkedIntRoom)
				{
					return true;
				}
			}
		}
	}
	return false;
}
 
void naEnvironment::SpikeResonanceObjects(const f32 impulse, const Vector3 &pos, const f32 maxDist,const CEntity* entity)
{
	for(u32 i = 0; i < g_MaxResonatingObjects ; i++)
	{
		if(m_InterestingLocalObjects[i].object && m_InterestingLocalObjects[i].resoSoundInfo.resoSoundRef != g_NullSoundRef)
		{
			Vec3V objectPos = m_InterestingLocalObjects[i].object->GetTransform().GetPosition();
			if(m_InterestingLocalObjects[i].object->GetCurrentPhysicsInst())
			{
				objectPos = m_InterestingLocalObjects[i].object->GetCurrentPhysicsInst()->GetCenterOfMass();
			}
			if(entity && entity != m_InterestingLocalObjects[i].object)
			{		
				// if we have an entity is because we only want to spike that entity alone.
				continue;
			}
			const f32 dist = (VEC3V_TO_VECTOR3(objectPos) - pos).Mag();
			// Spike the resonanceIntensity based on the distance
			f32 intensitySpike = 1.f - Min((dist <= sm_DistancePlateau ? 0.f : dist)/maxDist, 1.f);
			intensitySpike *= impulse;
			intensitySpike = Min(intensitySpike, 1.f);
			const f32 attackTime = m_InterestingLocalObjects[i].resoSoundInfo.attackTime;
			const f32 releaseTime = m_InterestingLocalObjects[i].resoSoundInfo.releaseTime;
			m_InterestingLocalObjects[i].resoSoundInfo.resonanceIntensity = ProcessResoVolume(intensitySpike,attackTime,releaseTime,m_InterestingLocalObjects[i].resoSoundInfo.resonanceIntensity);
		}
	}
}

f32 naEnvironment::GetVectorToNorthXYAngle(const Vector3& v)
{
	Vector3 XYVecNormalized = v;
	XYVecNormalized.z = 0.0f;
	XYVecNormalized.NormalizeFast();
	f32 cosAngle = XYVecNormalized.Dot(g_Directions[AUD_AMB_DIR_NORTH]);
	cosAngle = Max(Min(cosAngle, 1.0f), -1.0f);
	f32 angle = AcosfSafe(cosAngle);
	angle *= (360.0f*0.5f/PI); // Switch from radians to degrees
	// But it might be 360-that if we're in the left side - do a cos with right to see
	f32 right = XYVecNormalized.Dot(g_Directions[AUD_AMB_DIR_EAST]);
	if (right<0.0f)
	{
		angle = 359.9f - angle;
	}
	return angle;
}

f32 naEnvironment::GetVectorToUpAngle(const Vector3& v)
{
	const Vector3 up = Vector3(0.0f, 0.0f, 1.0f);
	Vector3 vNormalized = v;
	vNormalized.NormalizeFast();
	f32 cosUp = vNormalized.Dot(up);
	cosUp = Max(Min(cosUp, 1.0f), -1.0f);
	f32 angleUp = AcosfSafe(cosUp);
	angleUp *= (360.0f*0.5f/PI); // convert radians to degrees
	return angleUp;
}

void naEnvironment::UpdateEchoPositions()
{
// @OCC TODO - Determine best echo metrics calculation
/*
	// Update the suitability of each direction to having echoes from it
	// Centered around each direction, look at lower and upper, and each side, with half importance.
	Vector3 centrePosition = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();

	for (s32 i=0; i<8; i++)
	{
		f32 echoGoodnessFactor = 0.0f;
		f32 bestDistance = 0.0f;
		for (s32 j=-1; j<2; j++)
		{
			u32 index = (i+j+8) % 8;
			f32 lowerContribution = m_DistanceToEchoCurve.CalculateValue(m_LevelWallDistances[index*3]); 
			f32 upperContribution = m_DistanceToEchoCurve.CalculateValue(m_UpperWallDistances[index]);
			if (j!=0)
			{
				lowerContribution *= 0.33f;
				upperContribution *= 0.33f;
			}
			else
			{
				// Average our normals
				Vector3 lowerNormal = m_LevelReflectionNormal[i];
				lowerNormal.z = 0.0f;
				lowerNormal.NormalizeSafe();
				Vector3 upperNormal = m_UpperReflectionNormal[i];
				upperNormal.z = 0.0f;
				upperNormal.NormalizeSafe();
				if ((lowerContribution+upperContribution)>0.01f)
				{
					upperNormal.Scale(upperContribution/(lowerContribution+upperContribution));
					lowerNormal.Scale(lowerContribution/(lowerContribution+upperContribution));
					m_EchoDirection[i] = lowerNormal + upperNormal;
				}
				else
				{
					m_EchoDirection[i].Set(1.0f);
					m_EchoDirection[i].Normalize();
				}
			}
			echoGoodnessFactor += (lowerContribution + upperContribution);
			bestDistance += (lowerContribution * m_LevelWallDistances[index]);
			bestDistance += (upperContribution * m_UpperWallDistances[index]);
		}
		// That's up-and-down, 1/3 from each side, so a total of (1+2/3)*2 = 3.33 zero-to-ones
		echoGoodnessFactor /= 3.33f;
		m_EchoSuitability[i] = echoGoodnessFactor;
		bestDistance /= 3.33f;
		// But we also need to scale the distance up by the total echo goodness, to normalise it :-/
		bestDistance = Selectf(echoGoodnessFactor*-1.0f, 25.0f, bestDistance/echoGoodnessFactor);

		if(bestDistance < 0)
		{
			bestDistance = 25.0;
			m_EchoSuitability[i] = 0.0f;
		}

		m_EchoDistance[i] = bestDistance;
		Vector3 offset = m_RelativeScanPositions[i*3];
		offset.Scale(bestDistance);
		m_EchoPosition[i] = centrePosition + offset;
	}
*/


	// Update the suitability of each direction to having echoes from it
	// Centered around each direction, look at lower and upper, and each side, with half importance.
	Vector3 centrePosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());

	for (u8 i=0; i<8; i++)
	{
		f32 interpolatedDistance = 0.0f;
		Vector3 vEchoDirection(0.0f, 0.0f, 0.0f);

		// We use 4 probes per N, NE, E, ...direction.  1 on 15 degrees to either side of the main level radial, and the upper probe on the main radial.
		// So for N, we use the NNW, N, NNE level probes, and the N upper probe
		s32 levelLeftProbeIdx = (i*3 + -1 + 24) % 24;
		s32 levelCenterProbeIdx = (i*3 + 24) % 24;
		s32 levelRightProbeIdx = (i*3 + 1 + 24) % 24;
		s32 upperProbeIdx = i;

		Vector3 vProbeNormals[4];
		vProbeNormals[0] = m_LevelReflectionNormal[levelLeftProbeIdx];
		vProbeNormals[1] = m_LevelReflectionNormal[levelCenterProbeIdx];
		vProbeNormals[2] = m_LevelReflectionNormal[levelRightProbeIdx];
		vProbeNormals[3] = m_UpperReflectionNormal[upperProbeIdx];

		f32 probeDistances[4];
		probeDistances[0] = audNorthAudioEngine::GetOcclusionManager()->GetWallDistance(AUD_OCC_PROBE_ELEV_W, levelLeftProbeIdx);
		probeDistances[1] = audNorthAudioEngine::GetOcclusionManager()->GetWallDistance(AUD_OCC_PROBE_ELEV_W, levelCenterProbeIdx);
		probeDistances[2] = audNorthAudioEngine::GetOcclusionManager()->GetWallDistance(AUD_OCC_PROBE_ELEV_W, levelRightProbeIdx);
		probeDistances[3] = audNorthAudioEngine::GetOcclusionManager()->GetWallDistance(AUD_OCC_PROBE_ELEV_WWN, upperProbeIdx);

		f32 totalWeight = 0.0f;
		f32 probeWeights[4];
		for(s32 probe = 0; probe < 4; probe++)
		{
			probeWeights[probe] = m_DistanceToEchoCurve.CalculateValue(probeDistances[probe]); 
			totalWeight += probeWeights[probe];
		}

		// Interpolate based on weights to get the rest of the data (distance and normal)
		if(totalWeight > 0.0f)
		{
			for(s32 probe = 0; probe < 4; probe++)
			{
				// Should we factor in the z component?
				f32 probeWeight = probeWeights[probe] / totalWeight;

				interpolatedDistance += probeDistances[probe] * probeWeight;
				if(probeWeight > 0.0f)
				{
					vProbeNormals[probe].Scale(probeWeight);
					vEchoDirection += vProbeNormals[probe];
				}
			}
		}
		else
		{
			vEchoDirection = audNorthAudioEngine::GetOcclusionManager()->GetRelativeScanPosition(i*3);
			vEchoDirection.Scale(-1.0f);
			interpolatedDistance = (probeDistances[0] + probeDistances[1] + probeDistances[2] + probeDistances[3]) * 0.25f;
		}

		Vector3 offset = audNorthAudioEngine::GetOcclusionManager()->GetRelativeScanPosition(i*3);
		offset.Scale(interpolatedDistance);
		m_EchoPosition[i] = centrePosition + offset;

		// Base the suitability on all the probes, so if only 1 is suitable it's not that suitable a location overall
		m_EchoSuitability[i] = totalWeight * 0.25f;
		m_EchoDistance[i] = interpolatedDistance;
		m_EchoDirection[i] = vEchoDirection;

		audWeaponAudioEntity::SetReportGoodness(i, m_EchoSuitability[i]);
	}

#if __BANK
	DrawWeaponEchoes();
#endif
}

void naEnvironment::GetEchoSuitablilityForSourcePosition(const Vector3& sourcePosition, f32* suitability)
{
	if (!suitability)
	{
		naErrorf("Null ptr suitability passed into GetEchoSuitabilityForSourcePosition");
		return;
	}

	Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	// Work out angle of incidence stuff, and multiply those factors by the standard suitability factors.
	for (s32 i=0; i<8; i++)
	{
		Vector3 echoPoint = m_EchoPosition[i];
		Vector3 echoPointNormal = m_EchoDirection[i];
		Vector3 echoToListener = listenerPosition - echoPoint;
		Vector3 echoToSource = sourcePosition - echoPoint;
		// Flatten it - we do all this in 2d
		echoToListener.z = 0.0f;
		echoToListener.NormalizeSafe();
		echoToSource.z = 0.0f;
		echoToSource.NormalizeSafe();
		// Reflect listener direction in echo normal
		// Reflect listener direction in echo normal
		Quaternion listenerToNormalRotation;
		bool success = listenerToNormalRotation.FromVectors(echoToListener, echoPointNormal);
		if (success)
		{
			Vector3 listenerReflection; // I think this is wrong in rage
			listenerToNormalRotation.Transform(echoPointNormal, listenerReflection);
			// Now see how similar that direction is to the source direction
			f32 sourceAndReflectedListenerDotProduct = echoToSource.Dot(listenerReflection);
			//rk ported the next 2 lines from RDR2  11/12/12
			if(sourceAndReflectedListenerDotProduct < 0)
				sourceAndReflectedListenerDotProduct *= -1;
			// Map the dot product to a suitability scaling factor
			f32 angleSuitability = m_AngleOfIncidenceToSuitabilityCurve.CalculateValue(sourceAndReflectedListenerDotProduct);
			suitability[i] = m_EchoSuitability[i] * angleSuitability;
		}
		else
		{
			suitability[i] = 0.0f;
		}
	}
}

s32 naEnvironment::GetBestEchoForSourcePosition(const Vector3& sourcePosition, s32* oppositeEcho)
{
	if (!g_EnableLocalEnvironmentMetrics)
	{
		if (oppositeEcho)
		{
			*oppositeEcho = -1;
		}
		return -1;
	}

	f32 echoSuitablility[8];
	GetEchoSuitablilityForSourcePosition(sourcePosition, &echoSuitablility[0]);
	// Pick the best one
	s32 bestEcho = -1;
	f32 bestSuitability = 0.0f;
	s32 bestOppositeEcho = -1;
	for (s32 i=0; i<8; i++)
	{
		if (echoSuitablility[i] >= bestSuitability)
		{
			bestSuitability = echoSuitablility[i];
			bestEcho = i;
		}
	}
	if (bestEcho>-1)
	{
		// Once we've found our best echo, look for the best on the 'other side' and play one from there too.
		f32 bestOppositeSuitability = 0.0f;
		for (s32 i=3; i<6; i++)
		{
			u32 echoIndex = (bestEcho + i) % 8;
			if (echoSuitablility[echoIndex] >= bestOppositeSuitability)
			{
				bestOppositeSuitability = echoSuitablility[echoIndex];
				bestOppositeEcho = echoIndex;
			}
		}
	}
	if (oppositeEcho)
	{
		*oppositeEcho = bestOppositeEcho;
	}
	return bestEcho;
}

void naEnvironment::CalculateEchoDelayAndAttenuation(u32 echoIndex, const Vector3 &sourcePosition, u32* returnPredelay, f32* returnExtraAttenuation, Vector3* projectedPosition, u32 depth)
{
	f32		LocalEchoSuitability[8];
	f32		LocalEchoDistance[8];
	Vector3 LocalEchoPositions[8];

	const Vector3 vListenerPos(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()));

	if(depth==0)
	{
		for(s32 i=0; i<8; ++i)
		{
			LocalEchoSuitability[i] = m_EchoSuitability[i];
			LocalEchoDistance[i] = m_EchoDistance[i];
			LocalEchoPositions[i].Set(m_EchoPosition[i]);
		}
	}
	else
	{
		s32 offset = audEngineUtil::GetRandomInteger()%8;
		for(s32 i=0; i<8; ++i)
		{
			s32 index = (i + offset)%8;
			LocalEchoSuitability[i] = m_EchoSuitability[index];
			LocalEchoDistance[i] = Min(20.0f, m_EchoDistance[index]);
			LocalEchoPositions[i].Set(m_EchoPosition[index]-vListenerPos);
			LocalEchoPositions[i].Scale(LocalEchoDistance[i]);
			LocalEchoPositions[i].Add(sourcePosition);
		}
	}

	f32 listenerDistanceToEcho = LocalEchoDistance[echoIndex];
	f32 sourceDistanceToEcho = sourcePosition.Dist(LocalEchoPositions[echoIndex]);
	f32 sourceDistanceToListener = sourcePosition.Dist(vListenerPos);

	f32 distanceDifference = listenerDistanceToEcho + sourceDistanceToEcho - sourceDistanceToListener;

	// Work out predelay
	u32 predelay = (u32) m_DistanceToDelayCurve.CalculateValue(distanceDifference);

	// Work out extra attenuation - want to make it silent if the source is co-located with the echo position, otherwise it'll sound odd.
	// Plus, we attenuate echoes depending on how suitable they are.
	// We project the echo back past the echo point, so that it gets distance attenuated correctly anyway - that way we get reverb and filtering for free.
	//	f32 extraDistanceAttenuation = m_DeafeningDistanceRolloffCurve.CalculateValue(sourceDistanceToEcho / rolloffScale);
	f32 coLocatedAttenuation = m_SilentWhenEchoCoLocatedCurve.CalculateValue(sourceDistanceToEcho);
	f32 suitabilityAttenuation = m_EchoGoodnessDbVolumeCurve.CalculateValue(m_EchoSuitability[echoIndex]);
	f32 extraAttenuation = coLocatedAttenuation + suitabilityAttenuation;

	Vector3 projectedEcho = LocalEchoPositions[echoIndex] - vListenerPos;
	projectedEcho.Normalize();
	projectedEcho.Scale(sourceDistanceToEcho);
	projectedEcho.Add(LocalEchoPositions[echoIndex]);

	if (returnPredelay && returnExtraAttenuation)
	{
		*returnPredelay = predelay;
		*returnExtraAttenuation = extraAttenuation;
		*projectedPosition = projectedEcho;
		//		Warningf("dd: %f; eda: %f; cla: %f; sa: %f; ex: %f; delay: %d", distanceDifference, extraDistanceAttenuation, coLocatedAttenuation, suitabilityAttenuation, extraAttenuation, predelay);
	}
}

void naEnvironment::GetInteriorGunSoundMetricsForExterior(f32& returnVolume, f32& returnHold)
{
#if __BANK
	m_OutdoorInteriorWeaponVolCurve.Init("REV_WET_TO_OUTDOOR_INTERIOR_WEAPON_VOLUME");
	m_OutdoorInteriorWeaponHoldCurve.Init("REV_SIZE_TO_OUTDOOR_INTERIOR_WEAPON_HOLD");
#endif

	f32 normalizedReverbWet = ((f32)((m_SourceEnvironmentMetrics[0] & 12) >> 2))/3.0f;
	f32 normalizedReverbSize = ((f32)((m_SourceEnvironmentMetrics[0] & 3)))/3.0f;

	if(m_SourceEnvironmentMetrics[0] == 0)
	{
		returnVolume = m_OutdoorInteriorWeaponVolCurve.CalculateValue(0.0f);
		returnHold = m_OutdoorInteriorWeaponHoldCurve.CalculateValue(0.0f);
	}
	else if(normalizedReverbWet == 0)
	{
		returnVolume = m_OutdoorInteriorWeaponVolCurve.CalculateValue(0.15f);
		returnHold = m_OutdoorInteriorWeaponHoldCurve.CalculateValue(normalizedReverbSize);
	}
	else
	{
		returnVolume = m_OutdoorInteriorWeaponVolCurve.CalculateValue(normalizedReverbWet);
		returnHold = m_OutdoorInteriorWeaponHoldCurve.CalculateValue(normalizedReverbSize);
	}
}


void naEnvironment::SetLevelReflectionNormal(const Vector3& levelReflectionNormal, s32 radialIndex)
{
	if(naVerifyf(radialIndex >= 0 && radialIndex < g_audNumLevelProbeRadials, 
		"naEnvironment::SetLevelReflectionNormal Trying to set reflectionNormal data out of bounds at index: %d", radialIndex))
	{
		m_LevelReflectionNormal[radialIndex] = levelReflectionNormal;
	}
}

void naEnvironment::SetUpperReflectionNormal(const Vector3& upperReflectionNormal, s32 radialIndex)
{
	if(naVerifyf(radialIndex >= 0 && radialIndex < g_audNumMidProbeRadials, 
		"naEnvironment::SetUpperReflectionNormal Trying to set reflectionNormal data out of bounds at index: %d", radialIndex))
	{
		m_UpperReflectionNormal[radialIndex] = upperReflectionNormal;
	}
}

void naEnvironment::UpdateLocalReverbEnvironment()
{
	// Get up-to-date navmesh numbers
	UpdateNavmeshSourceEnvironment();
	// Work out how reverby each speaker direction is.
	// Draw lines out from the camera in the direction of the speaker - then dot that vector with each of the local offsets, to see how much
	// they contribute - quite a lot like the panning algorithm
	
	f32 speakerReverbWets[4][3];
	for (u32 i=0; i<4; i++)
	{
		for (u32 j=0; j<g_NumSourceReverbs; j++)
		{
			speakerReverbWets[i][j] = 0.0f;
		}
	}

	Vector3 speakerPosition[4];
	speakerPosition[0] = Vector3(-0.7071f, 0.7071f, 0.0f);
	speakerPosition[1] = Vector3(0.7071f, 0.7071f, 0.0f);
	speakerPosition[2] = Vector3(-0.7071f, -0.7071f, 0.0f);
	speakerPosition[3] = Vector3(0.7071f, -0.7071f, 0.0f);
	Matrix34 fullListenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
//	listenerMatrix.d.Zero();
//	Matrix34 fullListenerMatrix = FindPlayerPed()->GetMatrix();
	Matrix34 listenerMatrix = fullListenerMatrix;
	listenerMatrix.d.Zero();

	f32 elevation[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
	Vector3 positionOnUnitCircle[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
	f32 contributionScaling[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
	for (u32 k=0; k < m_NumValidPolys; k++)
	{
		// Where is the poly relative to us
		Vector3 polyRelativePosition = m_SourceEnvironmentPolyCentroids[k];
		fullListenerMatrix.UnTransform(polyRelativePosition);

		f32 distanceFromCamera = polyRelativePosition.Mag();
		Vector3 positionOnUnitSphere = polyRelativePosition;
		positionOnUnitSphere.NormalizeSafe();
		positionOnUnitCircle[k] = positionOnUnitSphere;
		positionOnUnitCircle[k].z = 0.f;
		positionOnUnitCircle[k].NormalizeSafe();
		// Get the angle between the pos on unit sphere and unit circle - and try linearly interping between our headline contr.
		elevation[k] = positionOnUnitCircle[k].Dot(positionOnUnitSphere);
		if (elevation[k] < -0.01f || elevation[k] > 1.01f)
		{
			elevation[k] = 0.0f; // it's almost certainly directly above or below, which is elevation=0.0f
		}
		// Scale contribution by the distance away from the camera
		f32 distanceScale = m_DistanceToListenerReverbCurve.CalculateValue(distanceFromCamera);
		f32 areaScale     = m_AreaToListenerReverbCurve.CalculateValue(m_SourceEnvironmentPolyAreas[k]);
		contributionScaling[k] = distanceScale * areaScale;
	}

	const float scriptOverrideScalar = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ListenerReverbDisabled) ? 0.f : 1.f;

	for (u32 i=0; i<4; i++)
	{
		// For this speaker, sum all our neighbouring poly's reverb contribution, scaling the contribution up with poly area,
		// up with angle alignment, and down with distance - all done with curves.
		// Probably also need to add the one you're actually over in a special cased way - currently just include it as normal,
		// since it's centre won't be totally aligned with the listener.
		f32 contribution[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
		f32 totalContribution = 0.0f;
		for (u32 k=0; k<m_NumValidPolys; k++)
		{
			// Work out this poly's contribution for this speaker
			f32 dotProd = positionOnUnitCircle[k].Dot(speakerPosition[i]);
			// Don't care about ones more than 90degs away
			dotProd = Max(0.0f, dotProd);
			if (k==0)
			{
				// it's the poly we're actually in
				dotProd = 1.0f;
			}
			// Linearly interp between 1/num_speakers and primary contribution as elevation goes from 0 (90degs) to 1 (flat)
			contribution[k] = ((1.0f - elevation[k]) * (1.0f/4.0f)) + (elevation[k] * dotProd);

			// Scale contribution by area and distance scales
			contribution[k] *= contributionScaling[k];
			totalContribution += contribution[k];
		}
		for (u32 k=0; k<m_NumValidPolys; k++)
		{
			for (u32 j=0; j<g_NumSourceReverbs; j++)
			{
				speakerReverbWets[i][j] += (contribution[k] * m_SourceEnvironmentPolyReverbWet[k][j] / totalContribution);
			}
		}
	}

	// Factor in the ambient environment zones.
	const EnvironmentRule* avgEnvRule = g_AmbientAudioEntity.GetAmbientEnvironmentRule();
	const f32 envRuleReverbScale = g_AmbientAudioEntity.GetAmbientEnvironmentRuleReverbScale();
	if(avgEnvRule && envRuleReverbScale != 0.0f)
	{
		const f32 scaledReverbDamp = avgEnvRule->ReverbDamp * envRuleReverbScale;

		for (u32 i=0; i<4; i++)
		{
			speakerReverbWets[i][0] *= 1.0f - scaledReverbDamp;	// Small
			speakerReverbWets[i][1] *= 1.0f - scaledReverbDamp;	// Medium
			speakerReverbWets[i][2] *= 1.0f - scaledReverbDamp;	// Large

			speakerReverbWets[i][0] = Max(speakerReverbWets[i][0], avgEnvRule->ReverbSmall * envRuleReverbScale);
			speakerReverbWets[i][1] = Max(speakerReverbWets[i][1], avgEnvRule->ReverbMedium * envRuleReverbScale);
			speakerReverbWets[i][2] = Max(speakerReverbWets[i][2], avgEnvRule->ReverbLarge * envRuleReverbScale);
		}
	}

	// Smooth our final stored ones
	for (u32 i=0; i<4; i++)
	{
		for (u32 j=0; j<g_NumSourceReverbs; j++)
		{
			m_SpeakerReverbWets[i][j] = g_SpeakerReverbDamping * m_SpeakerReverbWetSmoothers[i][j].CalculateValue(scriptOverrideScalar * speakerReverbWets[i][j], audNorthAudioEngine::GetCurrentTimeInMs());
		}
	}

#if __BANK
	if (g_DebugListenerReverb)
	{
		for (u32 i=0; i<4; i++)
		{
			for (u32 j=0; j<3; j++)
			{
				m_SpeakerReverbWetSmoothers[i][j].SetRate(g_SpeakerReverbSmootherRate);
			}
		}

		static const char *meterNames[] = {"s", "m", "l"};
		static f32 x[4] = {300, 700, 300, 700};
		static f32 y[4] = {300, 300, 600, 600};
		static audMeterList meterList[4];
		for (u32 i=0; i<4; i++)
		{
			meterList[i].left = x[i];
			meterList[i].bottom = y[i];
			meterList[i].width = 100.f;
			meterList[i].height = 200.f;
			meterList[i].values = &(m_SpeakerReverbWets[i][0]);
			meterList[i].names = meterNames;
			meterList[i].numValues = 3;
			audNorthAudioEngine::DrawLevelMeters(&(meterList[i])); 
		}

		for(u32 k = 0; k < m_NumValidPolys; k++)
		{
			grcDebugDraw::Sphere(m_SourceEnvironmentPolyCentroids[k], 0.25f, Color_AliceBlue);
			char buf[128];
			formatf(buf, "%d: %f, %f", k, m_SourceEnvironmentPolyReverbWet[k], m_SourceEnvironmentPolyAreas[k]);
			grcDebugDraw::Text(m_SourceEnvironmentPolyCentroids[k], Color_AliceBlue, buf);
		}
	}
	if(sm_DebugCoveredObjects)
	{
		for (u32 i = 0; i< m_InterestingLocalObjects.GetMaxCount(); i++)
		{
			if(m_InterestingLocalObjects[i].object && m_InterestingLocalObjects[i].envGroup)// && m_InterestingLocalObjects[i].underCover)
			{
				Vec3V objectPos = m_InterestingLocalObjects[i].object->GetTransform().GetPosition();
				if(m_InterestingLocalObjects[i].object->GetCurrentPhysicsInst())
				{
					objectPos = m_InterestingLocalObjects[i].object->GetCurrentPhysicsInst()->GetCenterOfMass();
				}
				char buf[128];
				formatf(buf, "%s COVER", (m_InterestingLocalObjects[i].envGroup->IsUnderCover()) ? "UNDER " : "NOT UNDER ");
				//formatf(buf, "%s COVER", (m_InterestingLocalObjects[i].underCover) ? "UNDER " : "NOT UNDER ");
				grcDebugDraw::Text(objectPos, Color_AliceBlue, buf);
				//grcDebugDraw::Sphere(m_InterestingLocalObjects[i].object->GetTransform().GetPosition(),1.f,Color_green);
			}
		}
		for(u32 k = 0; k < m_NumValidPolys; k++)
		{
			if(m_CoveredPolys.IsSet(k))
			{
				grcDebugDraw::Sphere(m_SourceEnvironmentPolyCentroids[k],sqrt(m_SourceEnvironmentPolyAreas[k]/PI)*sm_RadiusScale, Color32(0,255,0,100));
			}
		}
	}
#endif

	g_AudioEngine.GetEnvironment().SetSpeakerReverbWets(m_SpeakerReverbWets);
}

void naEnvironment::UpdateNavmeshSourceEnvironment()
{
	u32 roomToneHash = g_NullSoundHash;
	u32 rainType = RAIN_TYPE_NONE;
	// update our default one

    if (!g_DebugRoomSettings)
    {
        return;
    }

	InteriorRoom* room = audNorthAudioEngine::GetObject<InteriorRoom>(g_DebugRoomSettings->Room[0].Ref);

	if(room)
	{
		room->ReverbSmall = g_DebugRoomSettingsReverbSmall;
		room->ReverbMedium = g_DebugRoomSettingsReverbMedium;
		room->ReverbLarge = g_DebugRoomSettingsReverbLarge;
	}

	// This is almost identical to the occlusion group source environment calculations, but we need to store off
	// a bunch more info, and it's wasteful to keep that in pooled OcGroups, just for this one thing.
	u32 timeInMs = audNorthAudioEngine::GetCurrentTimeInMs();
//	Matrix34 listenerMatrix = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix();
	bool playerIsInAnInterior = false;
	bool playerIsInASubwayStationOrSubwayTunnel = false;
	bool playerIsInASubwayOrTunnel = false;
	Vector3 listenerPosition = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	
	CInteriorInst* pIntInst = NULL;
	s32 roomIdx = -1;
#if __DEV
    bool usingPlayer = false;
#endif
	if ((m_IsListenerInHead || g_UsePlayer) && !audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
	{
#if __DEV
        usingPlayer = true;
#endif
		CPed* player = CGameWorld::FindLocalPlayer();
		if(player && player->GetIsInInterior())
		{
			pIntInst = CInteriorInst::GetInteriorForLocation(player->GetInteriorLocation());
			roomIdx = player->GetInteriorLocation().GetRoomIndex();
		}
	}
	else
	{
		pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
		roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
	}

	if (pIntInst && roomIdx>0 && roomIdx!=INTLOC_INVALID_INDEX)
	{
		playerIsInAnInterior = true;
		if (pIntInst->IsSubwayMLO())
		{
			playerIsInASubwayOrTunnel = true;
		}
	}
	/*
	int32 interiorId = -1;
	if (player)
	{
		interiorId = player->GetInteriorInstId();
	}
	CInteriorInst* pIntInst = NULL;
	if (player && player->m_nFlags.bInMloRoom)
	{
		int32 roomId = player->GetInteriorRoomId();
		if (roomId>=0 && roomId!=INTLOC_INVALID_INDEX)
		{
			playerIsInAnInterior = true;
			pIntInst = CInteriorInst::GetPool()->GetAt(interiorId);
			if (pIntInst && pIntInst->IsSubwayMLO())
			{
				playerIsInASubway = true;
			}
		}
	}
	*/
	if (pIntInst && pIntInst->IsPopulated() && roomIdx>0 && roomIdx!=INTLOC_INVALID_INDEX)// && !pIntInst->IsSubwayMLO())
	{
		g_AudioEngine.GetEnvironment().SetIsListenerInInterior(true, 0);
		if (m_SourceEnvironmentPathHandle != PATH_HANDLE_NULL)
		{
			CPathServer::CancelRequest(m_SourceEnvironmentPathHandle);
			m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
		}
		m_NumValidPolys = 1;
		m_SourceEnvironmentPolyCentroids[0] = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0));
		m_SourceEnvironmentPolyAreas[0] = 1.0f;
		// Get the room we're in
		const InteriorSettings* interiorSettings = NULL;
		const InteriorRoom* room = NULL;
		GetInteriorSettingsForInterior(pIntInst, roomIdx, interiorSettings, room);
		naCErrorf(interiorSettings, "Couldn't get InteriorSettings for the room we're in");

		if (interiorSettings && room)
		{
			m_SourceEnvironmentMetrics[0] = 0;
			f32 reverbWets[g_NumSourceReverbs];
			reverbWets[0] = room->ReverbSmall;
			reverbWets[1] = room->ReverbMedium;
			reverbWets[2] = room->ReverbLarge;
			for (u32 j=0; j<g_NumSourceReverbs; j++)
			{
				m_SourceEnvironmentPolyReverbWet[0][j] = reverbWets[j];
			}
			roomToneHash = room->RoomToneSound;
			
			rainType = room->RainType;
			if (room->InteriorType==INTERIOR_TYPE_ROAD_TUNNEL ||
				room->InteriorType==INTERIOR_TYPE_SUBWAY_TUNNEL ||
				room->InteriorType==INTERIOR_TYPE_SUBWAY_STATION)
			{
				// override the sourceEnvironment, to save setting it, and still get speech echos
				m_SourceEnvironmentMetrics[0] = 15;
			}
			else if (room->InteriorType==INTERIOR_TYPE_SUBWAY_ENTRANCE)
			{
				// override the sourceEnvironment, to save setting it, and still get speech echos, but a bit less than in the subway proper
				m_SourceEnvironmentMetrics[0] = 10;
			}
			if (room->InteriorType==INTERIOR_TYPE_SUBWAY_TUNNEL ||
				room->InteriorType==INTERIOR_TYPE_SUBWAY_STATION)
			{
				playerIsInASubwayStationOrSubwayTunnel = true;
			}
		}

		// @OCC TODO - Should we switch the CInteriorInst* and InteriorRoomIdx to instead use the fwInteriorLocation?
		// Do not update the listener interior settings if the microphone is frozen.
		if(!audNorthAudioEngine::GetMicrophones().IsMicFrozen())
		{
			m_ListenerInteriorInstance = pIntInst;
			m_InteriorRoomId = roomIdx;
			m_ListenerInteriorSettings = interiorSettings;
			m_ListenerInteriorRoom = room;
		}

		// Update Interior info
		UpdateLinkedRoomList(pIntInst);
//		UpdateLinkedRoomList(NULL);
	}
	else
	{
		// Update Interior info
		UpdateLinkedRoomList(NULL);

		m_ListenerInteriorInstance = NULL;
		m_InteriorRoomId = 0;
		m_ListenerInteriorSettings = NULL;
		m_ListenerInteriorRoom = NULL;

		if (m_SourceEnvironmentPathHandle != PATH_HANDLE_NULL)
		{
			// we're waiting on a request, so let's see if it's done yet.
			EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_SourceEnvironmentPathHandle);
			if (ret==ERequest_Ready)
			{
				u32 numValidPolys;
				u32 sourceEnvironmentMetrics[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
				f32 sourceEnvironmentPolyAreas[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
				Vector3 sourceEnvironmentPolyCentroids[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
				BANK_ONLY(m_CoveredPolys.Reset());
				u8 sourceEnvironmentFlags[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
				EPathServerErrorCode errorCode = CPathServer::GetAudioPropertiesResult(m_SourceEnvironmentPathHandle, numValidPolys, 
					sourceEnvironmentMetrics, sourceEnvironmentPolyAreas, sourceEnvironmentPolyCentroids, sourceEnvironmentFlags);
				if (errorCode == PATH_NO_ERROR && numValidPolys>0)
				{
					m_NumValidPolys = numValidPolys;
					for (u32 i= 0; i<numValidPolys; i++)
					{
						m_SourceEnvironmentMetrics[i] = sourceEnvironmentMetrics[i];
						const f32 reverbSize = GetSourceEnvironmentReverbSizeFromSEMetric(m_SourceEnvironmentMetrics[i]);
						const f32 reverbWet  = GetSourceEnvironmentReverbWetFromSEMetric(m_SourceEnvironmentMetrics[i]);
						if (sourceEnvironmentFlags[i] & CAudioRequest::FLAG_POLYGON_IS_SHELTERED)
						{
							m_CoveredPolys.Set(i);
						}
						for (u32 j=0; j<g_NumSourceReverbs; j++)
						{
							m_SourceEnvironmentPolyReverbWet[i][j] = reverbWet * m_SourceReverbCrossfadeCurves[j].CalculateValue(reverbSize);
						}
						m_SourceEnvironmentPolyAreas[i] = sourceEnvironmentPolyAreas[i];
						m_SourceEnvironmentPolyCentroids[i] = sourceEnvironmentPolyCentroids[i];
					}
				}
				m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
				// And now update our time and position, so we don't fire in another request too soon. 
				// (You could argue these should be set when the request is made, but better to update too slowly than thrash the pathserver.)
				// (Actually, I'll ALSO set them when the request goes in, to be super friendly.)
				m_SourceEnvironmentTimeLastUpdated = timeInMs;
				m_ListenerPositionLastSourceEnvironmentUpdate = listenerPosition;
			}
			else if(ret == ERequest_NotFound)
			{
				m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
			}
			else
			{
				// If we've been waiting more than 2 seconds, something's broken - we've probably lost track of our path handle somehow
				// Or you've paused the game... or things have got really busy and there's nothing we can do... or it's a full moon.
				// I'll enable this locally and keep an eye on it.
				// naAssertf(timeInMs < (m_SourceEnvironmentTimeLastUpdated + 2000) || g_TempFakedPaused || fwTimer::IsGamePaused(), "We've had to wait more than two seconds in UpdateNavmeshSourceEnvironment...could be something broken....or just a full moon");
			}
		}
		else
		{
			// We don't have a pending request, so see if it's about time we did.
			u32 minimumTimeToUpdate = 200;
			f32 distanceMovedSqrd = (listenerPosition - m_ListenerPositionLastSourceEnvironmentUpdate).Mag2();
			if ((timeInMs > (m_SourceEnvironmentTimeLastUpdated + minimumTimeToUpdate)) &&
				(distanceMovedSqrd > 0.25f))
			{
				// It's been a while, and we've moved, so fire in an update request
				m_SourceEnvironmentPathHandle = CPathServer::RequestAudioProperties(listenerPosition, NULL, g_SourceEnvironmentRadius, true);
				if (m_SourceEnvironmentPathHandle!=PATH_HANDLE_NULL)
				{
					// And make sure we don't update again for a while - we also update these number when we get the results back,
					// doing it here is mainly to ensure that a failed request doesn't start thrashing.
					m_SourceEnvironmentTimeLastUpdated = timeInMs;
					m_ListenerPositionLastSourceEnvironmentUpdate = listenerPosition;
				}
				else
				{
					// We got a KB. 
				}
			}
		}
	}

	// See if we've just left a subway/tunnel
	if (m_PlayerIsInASubwayOrTunnel && !playerIsInASubwayOrTunnel)
	{
		g_ScriptAudioEntity.PlayMobileGetSignal(0.6f);
	}
	m_PlayerIsInASubwayStationOrSubwayTunnel = playerIsInASubwayStationOrSubwayTunnel;
	m_PlayerIsInASubwayOrTunnel = playerIsInASubwayOrTunnel;
	m_PlayerIsInAnInterior = playerIsInAnInterior;
	BANK_ONLY(m_PlayerInteriorRatioSmoother.SetRate(g_InteriorRatioSmoothRate/1000.0f));
	if (m_PlayerIsInAnInterior)
	{
		m_PlayerInteriorRatio = m_PlayerInteriorRatioSmoother.CalculateValue(1.0f, timeInMs);
	}
	else
	{
		m_PlayerInteriorRatio = m_PlayerInteriorRatioSmoother.CalculateValue(0.0f, timeInMs);
	}

	// Update our room tone hash
	m_RoomToneHash = roomToneHash;
	m_RainType = rainType;

	if(m_PlayerIsInASubwayOrTunnel && !m_PlayerWasInASubwayOrTunnelLastFrame)
		g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO);

	m_PlayerWasInASubwayOrTunnelLastFrame = m_PlayerIsInASubwayOrTunnel;
	CalculateUnderCoverFactor();
	m_NavmeshInformationUpdatedThisFrame = true;
	/*
#if __DEV
	if (1)
	{
		char occlusionDebug[256] = "";
		sprintf(occlusionDebug, "t:%d", timeInMs - m_SourceEnvironmentTimeLastUpdated);
		grcDebugDraw::PrintToScreenCoors(occlusionDebug, 35,40);
	}
#endif
	*/
}

void naEnvironment::GetReverbFromSEMetric(const f32 sourceEnvironmentWet, const f32 sourceEnvironmentSize, f32* reverbSmall, f32* reverbMedium, f32* reverbLarge)
{
	static const audThreePointPiecewiseLinearCurve revCurve0(0.0f, 1.0f, 0.25f, 0.71f, 0.5f, 0.0f);
	(*reverbSmall) = sourceEnvironmentWet * revCurve0.CalculateValue(sourceEnvironmentSize);
	if(sourceEnvironmentSize <= 0.5f)
	{
		static const audThreePointPiecewiseLinearCurve revCurve1sm(0.0f, 0.0f, 0.25f, 0.71f, 0.5f, 1.0f);
		(*reverbMedium) = sourceEnvironmentWet * revCurve1sm.CalculateValue(sourceEnvironmentSize);
	}
	else
	{
		static const audThreePointPiecewiseLinearCurve revCurve1big(0.5f, 1.0f, 0.75f, 0.71f, 1.0f, 0.0f);
		(*reverbMedium) = sourceEnvironmentWet * revCurve1big.CalculateValue(sourceEnvironmentSize);
	}
	static const audThreePointPiecewiseLinearCurve revCurve2(0.5f, 0.0f, 0.75f, 0.71f, 1.0f, 1.0f);
	(*reverbLarge) = sourceEnvironmentWet * revCurve2.CalculateValue(sourceEnvironmentSize);
}

f32 naEnvironment::GetSourceEnvironmentReverbWetFromSEMetric(const u32 sourceEnvironment)
{
	if (sourceEnvironment==0)
	{
		return 0.0f;
	}
	u32 quantisedReverbWet = (sourceEnvironment & 12) >> 2;
	u32 quantisedReverbSize = (sourceEnvironment & 3);
	f32 wet = m_SourceEnvironmentToReverbWetCurve.CalculateValue((f32)quantisedReverbWet);
	// Now scale the wet value by size - we want to make small spaces 'wetter' - we could do 16 individual pairings, if this isn't good enough
	wet *= m_SourceEnvironmentSizeToWetScaling.CalculateValue((f32)quantisedReverbSize);
	return wet;
}

f32 naEnvironment::GetSourceEnvironmentReverbSizeFromSEMetric(const u32 sourceEnvironment)
{
	if (sourceEnvironment==0)
	{
		return 0.0f;
	}
	u32 quantisedReverbSize = (sourceEnvironment & 3);
	return m_SourceEnvironmentToReverbSizeCurve.CalculateValue((f32)quantisedReverbSize);
}

fwInteriorLocation naEnvironment::GetRoomIntLocFromPortalIntLoc(const fwInteriorLocation portalIntLoc) const
{
	// Initialize to invalid
	fwInteriorLocation roomIntLoc;
	const CInteriorInst* intInst = NULL;
	s32 roomIdx = INTLOC_INVALID_INDEX;

	if(naVerifyf(portalIntLoc.IsValid(), "Passed Invalid fwInteriorLocation")
		&& naVerifyf(portalIntLoc.IsAttachedToPortal(), "Passed non portal attached interior location when trying to get a room attached interior location"))
	{
		const CInteriorInst* portalIntInst = CInteriorInst::GetInteriorForLocation(portalIntLoc);
		if(portalIntInst)
		{
			const s32 portalIdx = portalIntLoc.GetPortalIndex();

			CPortalFlags portalFlags;
			u32 roomToUnsignedIdx, roomFromUnsignedIdx;
			portalIntInst->GetMloModelInfo()->GetPortalData(portalIdx, roomToUnsignedIdx, roomFromUnsignedIdx, portalFlags);
			// naAssertf(!portalFlags.GetIsMirrorPortal(), "We have an entity attached to a mirror portal, not sure why, but need to handle that case");
			const s32 roomToIdx = static_cast<s32>(roomToUnsignedIdx);
			const s32 roomFromIdx = static_cast<s32>(roomFromUnsignedIdx);

			const CInteriorInst* playerIntInst = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance();
			const s32 playerRoomIdx = audNorthAudioEngine::GetGtaEnvironment()->GetInteriorRoomId();

			if(playerIntInst && playerRoomIdx != INTLOC_ROOMINDEX_LIMBO)
			{
				if(portalIntInst == playerIntInst)
				{
					intInst = playerIntInst;

					// Set the roomIdx to be the same room as the player, so doors don't occlude themselves and get the correct rooms reverb
					if(playerRoomIdx == roomToIdx)
						roomIdx = roomToIdx;
					else if(playerRoomIdx == roomFromIdx)
						roomIdx = roomFromIdx;
					// We're in the same interior but neither of the rooms the door belongs to, so just prefer to use an interior room over outside
					else
						roomIdx = roomToIdx != INTLOC_ROOMINDEX_LIMBO ? roomToIdx : roomFromIdx;
				}
				else
				{
					// Check for a linked portal in which case we need to go through and get the destination to see if it matches the player
					if(portalFlags.GetIsLinkPortal())
					{
						// If the the portal's interior isn't loaded, then we can't see what interiors are linked, so just return what we can.
						// Worst case scenario is a door is occluded when it shouldn't be because we're not using the same room index as the player
						// but this really shouldn't be an issue in normal gameplay since doors that you see will have their interiors loaded.
						// This is to fix a crash probably due to the interior's being reset/reloaded/swapped after a checkpoint restart and
						// we end up accessing the GetRoomZeroPortalInsts() when that array is empty because it's not populated.
						if(!portalIntInst->IsPopulated())
						{
							intInst = portalIntInst;
							roomIdx = roomToIdx != INTLOC_ROOMINDEX_LIMBO ? roomToIdx : roomFromIdx;
						}
						else
						{
							CPortalInst* portalInst = portalIdx < portalIntInst->GetRoomZeroPortalInsts().GetCount() ? portalIntInst->GetRoomZeroPortalInsts()[portalIdx] : NULL;
							if(naVerifyf(portalInst, "Couldn't get the CPortalInst* when getting the linked interior destination") && portalInst->IsLinkPortal())
							{
								CInteriorInst* linkedIntInst = NULL;
								s32 linkedIntRoomLimboPortalIdx = INTLOC_INVALID_INDEX;
								portalInst->GetDestinationData(portalIntInst, linkedIntInst, linkedIntRoomLimboPortalIdx);
								// Is it linked to the interior that the player is in?
								if(linkedIntInst && linkedIntInst == playerIntInst && linkedIntRoomLimboPortalIdx != INTLOC_INVALID_INDEX)
								{
									intInst = playerIntInst;
									roomIdx = linkedIntInst->GetDestThruPortalInRoom(INTLOC_ROOMINDEX_LIMBO, linkedIntRoomLimboPortalIdx);		//entry/exit portal so we know it's room 0
								}
								// it's linked but not to us, so grab the non-limbo roomIdx, otherwise we'll be treating it as though it's outside
								else
								{
									intInst = portalIntInst;
									roomIdx = roomToIdx != INTLOC_ROOMINDEX_LIMBO ? roomToIdx : roomFromIdx;
								}
							}
						}
					}
					// That means it's in a different interior that's not connected, so prefer to use outside or the limbo room, as that's the closest "room" to the player
					else
					{
						intInst = portalIntInst;
						roomIdx = roomToIdx == INTLOC_ROOMINDEX_LIMBO ? roomToIdx : roomFromIdx;
					}
				}
			}
			// The player is outside
			else
			{
				// If it's a link portal, then don't use outside as roomIdx of 0 is actually the interior it's leading to
				// Otherwise we'll be playing doors to linked interiors as though they're outside
				if(portalFlags.GetIsLinkPortal())
				{
					intInst = portalIntInst;
					roomIdx = roomToIdx != INTLOC_ROOMINDEX_LIMBO ? roomToIdx : roomFromIdx;
				}
				// Prefer to use outside, so doors don't occlude themselves because they're inside and the player is outside
				else
				{
					intInst = portalIntInst;
					roomIdx = roomToIdx == INTLOC_ROOMINDEX_LIMBO ? roomToIdx : roomFromIdx;
				}
			}
		}
	}

	if(roomIdx != INTLOC_INVALID_INDEX)
	{
		roomIntLoc = CInteriorInst::CreateLocation(intInst, roomIdx);
	}

	return roomIntLoc;
}

void naEnvironment::FindInteriorSettingsForRoom(const u32 interiorHash, const u32 roomHash, const InteriorSettings*& interior, const InteriorRoom*& intRoom) const
{
	interior = NULL;
	intRoom = NULL;

	// Get the gameobject with the interior name, then look through for the room.
	// If the game object doesn't exist, store NULL, and we'll use defaults from higher-level code
	interior = audNorthAudioEngine::GetObject<InteriorSettings>(interiorHash);
	if(interior && interior->numRooms > 0)
	{
		naAssertf(interior->ClassId == InteriorSettings::TYPE_ID, "Attempting to use an interior settings object but it has type %u when it should be %u", interior->ClassId, InteriorSettings::TYPE_ID);
		// Look for the right room - use the first one if we don't find a match
		for (u8 i=0; i<interior->numRooms; i++)
		{
			const InteriorRoom* loopRoom = audNorthAudioEngine::GetObject<InteriorRoom>(interior->Room[i].Ref);
			if(loopRoom && loopRoom->RoomName == roomHash)
			{
				intRoom = loopRoom;
				break;
			}
		}
	}
}

void naEnvironment::GetInteriorSettingsForEntity(const CEntity* entity, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom)
{
	intSettings = NULL;
	intRoom = NULL;

	// If we're debugging, override with our default global room settings
	if (g_UseDebugRoomSettings)
	{
		GetDebugInteriorSettings(intSettings, intRoom);
		return;
	}

	if(naVerifyf(entity, "Null entity pointer in GetInteriorSettingsForEntity"))
	{
		const fwInteriorLocation intLoc = entity->GetAudioInteriorLocation();
		GetInteriorSettingsForInteriorLocation(intLoc, intSettings, intRoom);
	}
}

void naEnvironment::GetInteriorSettingsForInteriorLocation(const fwInteriorLocation intLoc, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom)
{
	intSettings = NULL;
	intRoom = NULL;

	// If we're debugging, override with our default global room settings
	if (g_UseDebugRoomSettings)
	{
		GetDebugInteriorSettings(intSettings, intRoom);
		return;
	}

	// If we're attached to a portal, then we need to figure out the best roomIdx to use based on the player location
	fwInteriorLocation roomAttachedIntLoc = intLoc;
	if(intLoc.IsAttachedToPortal())
	{
		roomAttachedIntLoc = GetRoomIntLocFromPortalIntLoc(intLoc);
	}
	
	const CInteriorInst* intInst = CInteriorInst::GetInteriorForLocation(roomAttachedIntLoc);
	if(intInst)
	{
		const s32 roomIdx = roomAttachedIntLoc.GetRoomIndex();
		if(roomIdx != INTLOC_INVALID_INDEX && roomIdx < (s32)intInst->GetNumRooms())
		{
			GetInteriorSettingsForInterior(intInst, roomIdx, intSettings, intRoom);
		}
	}
}

void naEnvironment::GetInteriorSettingsForInterior(const CInteriorInst* intInst, const s32 roomIdx, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom)
{
	intSettings = NULL;
	intRoom = NULL;

	// If we're debugging, override with our default global room settings
	if (g_UseDebugRoomSettings)
	{
		GetDebugInteriorSettings(intSettings, intRoom);
		return;
	}

	if (intInst && roomIdx!=INTLOC_INVALID_INDEX
		&& naVerifyf(roomIdx < intInst->GetNumRooms(), "Requesting InteriorSettings with an invalid roomIdx.  Int: %s roomIdx: %d", intInst->GetModelName(), roomIdx))
	{
		// Get the room we're in
		CMloModelInfo* pModelInfo = intInst->GetMloModelInfo();
		if(naVerifyf(pModelInfo, "Failed to get model info from interior instance model index %d", intInst->GetModelId().GetModelIndex()))
		{
			pModelInfo->GetAudioSettings(roomIdx, intSettings, intRoom);

			// If we don't find one, return a sensible default
			if (!intSettings)
			{
				// perhaps try again - in debug mode, we might have created the object after the room was init'd. 
				if (g_TryRoomsEveryTime)
				{
					audNorthAudioEngine::GetGtaEnvironment()->FindInteriorSettingsForRoom(pModelInfo->GetHashKey(), intInst->GetRoomHashcode(roomIdx), intSettings, intRoom);
					// And store it, so next time we find it straight away
					pModelInfo->SetAudioSettings(roomIdx, intSettings, intRoom);
				}
				// If the default doesn't exist, use the debug one
				if (!intSettings)
				{
					GetDebugInteriorSettings(intSettings, intRoom);
				}
			}
		}
	}
}

void naEnvironment::GetDebugInteriorSettings(const InteriorSettings*& intSettings, const InteriorRoom*& intRoom) const
{
	intSettings = g_DebugRoomSettings;
	intRoom = audNorthAudioEngine::GetObject<InteriorRoom>(g_DebugRoomSettings->Room[0].Ref);
}

#if __BANK
void naEnvironment::AddBuildingToWorld(const Vector3 &pos, CEntity* entity)
{
	if(entity)
	{
		const Vector3 boundingBoxMax = entity->GetBoundingBoxMax();
		const Vector3 boundingBoxMin = entity->GetBoundingBoxMin();

		f32 width = abs(boundingBoxMax.x - boundingBoxMin.x);
		f32 depth = abs(boundingBoxMax.y - boundingBoxMin.y);
		f32 baseArea = (depth * width);

		// Discount buildings with a massive or tiny footprint- they're probably actually huge chunks of terrain or
		// little electricity pylons respectively
		if(baseArea > g_MinBuildingFootprintArea &&
		   baseArea < g_MaxBuildingFootprintArea)
		{
			f32 height = abs(boundingBoxMax.z - boundingBoxMin.z);

			if(height >= g_MinBuildingHeight)
			{
				if(m_NumBuildingAddsThisFrame >= g_audMaxCachedSectorOps)
				{
					ProcessCachedWorldSectorOps();
				}

				m_WSBuildingAddHeights[m_NumBuildingAddsThisFrame] = static_cast<u32>(height);
				m_WSBuildingAddIndices[m_NumBuildingAddsThisFrame++] = static_cast<u32>(ComputeWorldSectorIndex(pos));
			}
		}
	}
}

void naEnvironment::AddTreeToWorld(const Vector3 &pos, CEntity* entity)
{
	if(entity)
	{
		f32 height = entity->GetBoundingBoxMax().z - entity->GetBoundingBoxMin().z;

		// Small trees don't count. Its a shrubbery! etc.
		if(height > g_MinHeightForTree)
		{
			// if theres space in the cached list then add it there to save a LHS
			if(m_NumTreeAddsThisFrame >= g_audMaxCachedSectorOps)
			{
				ProcessCachedWorldSectorOps();
			}
			m_WSTreeAddIndices[m_NumTreeAddsThisFrame++] = static_cast<u32>(ComputeWorldSectorIndex(pos));
		}
	}
}

void naEnvironment::RemoveTreeFromWorld(const Vector3 &pos)
{
	// if theres space in the cached list then add it there to save a LHS
	if(m_NumTreeRemovesThisFrame >= g_audMaxCachedSectorOps)
	{
		ProcessCachedWorldSectorOps();
	}
	m_WSTreeRemoveIndices[m_NumTreeRemovesThisFrame++] = static_cast<u32>(ComputeWorldSectorIndex(pos));
}

void naEnvironment::RemoveBuildingFromWorld(const Vector3 &pos)
{
	// if theres space in the cached list then add it there to save a LHS
	if(m_NumBuildingRemovesThisFrame >= g_audMaxCachedSectorOps)
	{
		ProcessCachedWorldSectorOps();
	}
	m_WSBuildingRemoveIndices[m_NumBuildingRemovesThisFrame++] = static_cast<u32>(ComputeWorldSectorIndex(pos));
}

void naEnvironment::AddWaterQuad(const Vector3 &v1,const Vector3 &v2,const Vector3 &v3,const Vector3 &v4)
{
	const u32 index1 = static_cast<u32>(ComputeWorldSectorIndex(v1));
	const u32 index2 = static_cast<u32>(ComputeWorldSectorIndex(v2));
	const u32 index3 = static_cast<u32>(ComputeWorldSectorIndex(v3));
	const u32 index4 = static_cast<u32>(ComputeWorldSectorIndex(v4));

	const u32 secX1 = index1 % g_audNumWorldSectorsX;
	const u32 secY1 = (index1-secX1)/g_audNumWorldSectorsX;
	const u32 secX2 = index2 % g_audNumWorldSectorsX;
	const u32 secY2 = (index2-secX2)/g_audNumWorldSectorsX;
	const u32 secX3 = index3 % g_audNumWorldSectorsX;
	const u32 secY3 = (index3-secX3)/g_audNumWorldSectorsX;
	const u32 secX4 = index4 % g_audNumWorldSectorsX;
	const u32 secY4 = (index4-secX4)/g_audNumWorldSectorsX;

	// find the min and max sector index
	const u32 minIndex = Min(index1, index2, index3, index4);
	const u32 maxIndex = Max(index1, index2, index3, index4);

	// and min/max X/Y
	const u32 minSecX = Max(Min(secX1, secX2, secX3, secX4), (u32)0);
	const u32 maxSecX = Min(Max(secX1, secX2, secX3, secX4), (u32)(g_audNumWorldSectorsX-1));
	const u32 minSecY = Max(Min(secY1, secY2, secY3, secY4), (u32)0);
	const u32 maxSecY = Min(Max(secY1, secY2, secY3, secY4), (u32)(g_audNumWorldSectorsY-1));

	for(u32 i = minIndex; i <= maxIndex; i++)
	{
		const u32 curX = i % g_audNumWorldSectorsX;
		const u32 curY = (i-curX)/g_audNumWorldSectorsX;
		// does this sector intersect
		if(curX >= minSecX && curX <= maxSecX && curY >= minSecY && curY <= maxSecY)
		{
			//g_AudioWorldSectors[i].isWaterSector = false;
		}
	}
}

void naEnvironment::ProcessCachedWorldSectorOps()
{
	for(u32 i =0; i < m_NumBuildingAddsThisFrame; i++)
	{
		f32 fBuildingHeight = (f32)(m_WSBuildingAddHeights[i]/284.f);
		s32 buildingHeight = (s32)(fBuildingHeight * 254.f);
		buildingHeight = (u8)Clamp(buildingHeight,0,254);
		if(g_AudioWorldSectors[m_WSBuildingAddIndices[i]].tallestBuilding < buildingHeight)
		{
			Assign(g_AudioWorldSectors[m_WSBuildingAddIndices[i]].tallestBuilding, buildingHeight);
		}

		if(g_AudioWorldSectors[m_WSBuildingAddIndices[i]].numBuildings < 255)
		{
			g_AudioWorldSectors[m_WSBuildingAddIndices[i]].numBuildings++;
		}
	}

	for(u32 i =0; i < m_NumBuildingRemovesThisFrame; i++)
	{
		if(g_AudioWorldSectors[m_WSBuildingRemoveIndices[i]].numBuildings > 0)
		{
			g_AudioWorldSectors[m_WSBuildingRemoveIndices[i]].numBuildings--;
		}
	}

	for(u32 i =0; i < m_NumTreeAddsThisFrame; i++)
	{
		if(g_AudioWorldSectors[m_WSTreeAddIndices[i]].numTrees < 127)
		{
			g_AudioWorldSectors[m_WSTreeAddIndices[i]].numTrees++;
		}
	}

	for(u32 i =0; i < m_NumTreeRemovesThisFrame; i++)
	{
		if(g_AudioWorldSectors[m_WSTreeRemoveIndices[i]].numTrees > 0)
		{
			g_AudioWorldSectors[m_WSTreeRemoveIndices[i]].numTrees--;
		}
	}

	m_NumBuildingAddsThisFrame = m_NumBuildingRemovesThisFrame = m_NumTreeAddsThisFrame = m_NumTreeRemovesThisFrame = 0;
}

#endif
f32 naEnvironment::ComputeWorldSectorIndex(const Vector3 &pos) const
{
	// NOTE: this function returns an f32 rather than u32 index, so that a LHS can be avoided in higher level code in some cases
	// NOTE: assumes world is centered at origin
	f32 x = floorf((pos.x / g_audWidthOfSector) + (g_fAudNumWorldSectorsX * 0.5f));
	f32 y = floorf((pos.y / g_audDepthOfSector) + (g_fAudNumWorldSectorsY * 0.5f));
	x = Clamp(x, 0.0f, g_fAudNumWorldSectorsX-1);
	y = Clamp(y, 0.0f, g_fAudNumWorldSectorsY-1);
	return x + (y * g_fAudNumWorldSectorsX);
}	

void naEnvironment::ComputeRainMixForSector(const u32 sectorIndex, const f32 rainVol, audRainXfade *directionMixes)
{
	audRainXfade sectorRain[9];
	const u32 sectorX = sectorIndex % g_audNumWorldSectorsX, sectorY = (sectorIndex-sectorX)/g_audNumWorldSectorsY;
	u32 i = 0;

	ComputeRainMixForSector(sectorX-1,sectorY+1,sectorRain[i++]);
	ComputeRainMixForSector(sectorX,sectorY+1,sectorRain[i++]);
	ComputeRainMixForSector(sectorX+1,sectorY+1,sectorRain[i++]);
	ComputeRainMixForSector(sectorX-1,sectorY,sectorRain[i++]);
	ComputeRainMixForSector(sectorX,sectorY,sectorRain[i++]);
	ComputeRainMixForSector(sectorX+1,sectorY,sectorRain[i++]);
	ComputeRainMixForSector(sectorX-1,sectorY-1,sectorRain[i++]);
	ComputeRainMixForSector(sectorX,sectorY-1,sectorRain[i++]);
	ComputeRainMixForSector(sectorX+1,sectorY-1,sectorRain[i++]);

	const Matrix34 listenerMatrix = audNorthAudioEngine::GetMicrophones().GetAmbienceMic();//g_AudioEngine.GetEnvironment().GetPanningListenerMatrix();

	const Vector3 north = Vector3(0.0f, 1.0f, 0.0f);
	const Vector3 east = Vector3(1.0f,0.0f,0.0f);
	Vector3 right = listenerMatrix.a;
	right.NormalizeSafe();
	const f32 cosangle = right.Dot(north);
	const f32 angle = AcosfSafe(cosangle);
	const f32 degrees = RtoD*angle;
	f32 actualDegrees;
	if(right.Dot(east) <= 0.0f)
	{
		actualDegrees = 360.0f - degrees;
	}
	else
	{
		actualDegrees = degrees;
	}

	s32 basePan = 360 - ((s32)actualDegrees - 90);

	const Vector2 listenerPos(listenerMatrix.d.x, listenerMatrix.d.y);
	const Vector2 emitterPositions[] = {Vector2(sectorRain[1].x,sectorRain[1].y), // NORTH
										Vector2(sectorRain[3].x,sectorRain[3].y), // EAST
										Vector2(sectorRain[5].x,sectorRain[5].y), // SOUTH
										Vector2(sectorRain[7].x,sectorRain[7].y)}; // WEST

	for(u32 emitterIdx = 0; emitterIdx < 4; emitterIdx++)
	{
		Vector2 list2emitter = (listenerPos - emitterPositions[emitterIdx]);
		list2emitter.NormalizeSafe();

		f32 factor[9];
		for(u32 i = 0; i < 9; i++)
		{
			const Vector2 sectorPos = Vector2(sectorRain[i].x, sectorRain[i].y);
			Vector2 list2sector = (listenerPos - sectorPos);
			list2sector.NormalizeSafe();
			// ignore angle for sector that the listener is in
			const f32 angleContrib = (i==4?1.0f:Clamp(list2emitter.Dot(list2sector), 0.0f, 1.0f));
			static f32 distScaler = 0.05f;
			const f32 distFactor = Clamp(1.0f / ((listenerPos - sectorPos) * distScaler).Mag2(), 0.0f, 1.0f);

			factor[i] = angleContrib * distFactor;
		}

		f32 sqSum = 0.0f;
		for(u32 i = 0; i < 9; i++)
		{
			sqSum += factor[i] * factor[i];
		}
		f32 sqFactor = sqrt(sqSum);

		directionMixes[emitterIdx].buildingRain = directionMixes[emitterIdx].waterRain = directionMixes[emitterIdx].treeRain = 0.0f;
		for(u32 i = 0; i < 9; i++)
		{
			const f32 sectorFactor = (factor[i]/sqFactor);
			directionMixes[emitterIdx].buildingRain += (sectorFactor * sectorRain[i].buildingRain);
			directionMixes[emitterIdx].treeRain += (sectorFactor * sectorRain[i].treeRain);
			directionMixes[emitterIdx].waterRain += (sectorFactor * sectorRain[i].waterRain);
		}

		directionMixes[emitterIdx].buildingRain *= rainVol;
		directionMixes[emitterIdx].treeRain *= rainVol;
		directionMixes[emitterIdx].waterRain *= rainVol;
		directionMixes[emitterIdx].pan = (basePan + emitterIdx*90) % 360;
	}
}

void naEnvironment::ComputeRainMixForSector(const u32 sectorX, const u32 sectorY, audRainXfade &rain) const
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sector = sectorX + (sectorY*g_audNumWorldSectorsX);
		const bool isWaterSector = g_AudioWorldSectors[sector].isWaterSector;
		rain.x = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f));
		rain.y = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f));
		if(g_AmbientAudioEntity.IsPlayerInTheCity())
		{
			rain.treeRain = (Min( g_NumTreesForRain, static_cast<f32>(g_AudioWorldSectors[sector].numTrees)) / g_NumTreesForRainBuiltUp);
		}
		else
		{
			rain.treeRain = (Min( g_NumTreesForRain, static_cast<f32>(g_AudioWorldSectors[sector].numTrees)) / g_NumTreesForRain);
		}
		rain.buildingRain = 1.0f - rain.treeRain;
		
		const Vector3 v1(rain.x,rain.y,100.0f), v2(rain.x+g_audWidthOfSector,rain.y+g_audDepthOfSector,100.f);
		// const Vector3 centre = (v1+v2) * 0.5f;
			
		if(isWaterSector)
		{
			rain.waterRain = 1.0f;
			rain.buildingRain = 0.0f;
			rain.treeRain = 0.0f;
		}
		else
		{
			rain.waterRain = 0.0f;
		}
	}
	else
	{
		// invalid sector - default to water (since the map is surrounded by water)
		rain.waterRain = 1.0f;
		rain.buildingRain = 0.0f;
		rain.treeRain = 0.0f;
		rain.x = 0.0f;
		rain.y = 0.0f;
	}

	f32 sqFactor = sqrt(rain.waterRain*rain.waterRain + rain.buildingRain*rain.buildingRain + rain.treeRain*rain.treeRain);
	rain.waterRain /= sqFactor;
	rain.buildingRain /= sqFactor;
	rain.treeRain /= sqFactor;
}
f32	 naEnvironment::GetBuiltUpFactorOfSector(u32 sector){
	return m_BuiltUpFactorPerZone[sector];
}

void naEnvironment::UpdateMetrics()
{
	const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	const u32 sectorIndex = static_cast<u32>(ComputeWorldSectorIndex(listenerMatrix.d));
	const u32 sectorX = sectorIndex % g_audNumWorldSectorsX;
	const u32 sectorY = (sectorIndex-sectorX)/g_audNumWorldSectorsY;

	if(!m_RefreshHighwayFactor)
	{
		if(m_LastHighwaySectorX != sectorX ||
			m_LastHighwaySectorY != sectorY)
		{
			m_RefreshHighwayFactor = true;
			m_HighwayZoneToRefresh = 0;

			m_LastHighwaySectorX = sectorX;
			m_LastHighwaySectorY = sectorY;
		}
	}
		for (u32 y=0; y<AUD_NUM_SECTORS_ACROSS; y++)
		{
			for (u32 x=0; x<AUD_NUM_SECTORS_ACROSS; x++)
			{
				u32 xSector = sectorX+x-AUD_HALF_NUM_SECTORS_ACROSS;
				u32 ySector = sectorY-y+AUD_HALF_NUM_SECTORS_ACROSS;

				if(xSector < g_audNumWorldSectorsX && ySector < g_audNumWorldSectorsY)
				{
					const u32 sector = xSector + (ySector*g_audNumWorldSectorsX);
					m_BuildingHeights[x+AUD_NUM_SECTORS_ACROSS*y] = GetBuildingHeightForSector(sector);
					m_BuildingCount[x+AUD_NUM_SECTORS_ACROSS*y] = GetBuildingCountForSector(sector);
					m_TreeCount[x+AUD_NUM_SECTORS_ACROSS*y] = GetTreeCountForSector(sector);
					m_WaterAmount[x+AUD_NUM_SECTORS_ACROSS*y] = GetWaterAmountForSector(sector);
					m_HighwayAmount[x+AUD_NUM_SECTORS_ACROSS*y] = GetHighwayAmountForSector(sector);

#if __BANK 
					// Just update one zone at a time, no need for uber-accuracy
					if(sm_ProcessSectors && m_RefreshHighwayFactor && 
						(y + (x * AUD_NUM_SECTORS_ACROSS)) == m_HighwayZoneToRefresh)
					{
						Vector3 zoneMin,zoneMax;
						zoneMin.x = g_audWidthOfSector * (xSector - (g_fAudNumWorldSectorsX/2.f));
						zoneMin.y = g_audDepthOfSector * (ySector - (g_fAudNumWorldSectorsY/2.f));
						zoneMax.x = zoneMin.x + g_audWidthOfSector;
						zoneMax.y = zoneMin.y + g_audDepthOfSector;

						Vector3 centre = (zoneMin + zoneMax) * 0.5f;
						centre.z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(centre.x, centre.y);
						zoneMin.z = centre.z - 50.0f;
						zoneMax.z = centre.z + 50.0f;

						audParseNodeCallbackData callbackData;
						callbackData.numHighwayNodes = 0;
						callbackData.numNodes = 0;
						callbackData.minPos = zoneMin;
						callbackData.maxPos = zoneMax;
						ThePaths.ForAllNodesInArea(zoneMin, zoneMax, ParseNodeCallback, &callbackData);

						if(callbackData.numNodes > 0)
						{
							Assign(g_AudioWorldSectors[sector].numHighwayNodes, Clamp(callbackData.numHighwayNodes, 0, 256));
						}
					}
#endif
				}

				if(g_UpdateBuiltUpFactor)
				{
					f32 builtUpFactor = m_HeightToBuiltUpFactor.CalculateValue(m_BuildingHeights[x+AUD_NUM_SECTORS_ACROSS*y]);
					m_BuiltUpFactorPerZone[x+AUD_NUM_SECTORS_ACROSS*y] = m_BuiltUpSmoother.CalculateValue(builtUpFactor, audNorthAudioEngine::GetCurrentTimeInMs());
				}
			}
		}
	// Work out where the centre of the main sector is, so we know how much of the others to factor in
	f32 mainSectorX = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f) + 0.5f);
	f32 mainSectorY = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f) + 0.5f);
	f32 ourX = listenerMatrix.d.x;
	f32 ourY = listenerMatrix.d.y;
	f32 ratioCtoR = (ourX - mainSectorX)/(g_audWidthOfSector); // this goes from -0.5 to 0.5 - we never want to be fully in the next sector
	f32 ratioCtoU = (ourY - mainSectorY)/(g_audDepthOfSector); // this goes from -0.5 to 0.5 - we never want to be fully in the next sector
	u32 diagonalCrossfadeIndex = AUD_CENTER_SECTOR;
	u32 yCrossfadeIndex = AUD_CENTER_SECTOR + AUD_NUM_SECTORS_ACROSS;
	if (ratioCtoU>0.0f)
	{
		yCrossfadeIndex = AUD_CENTER_SECTOR - AUD_NUM_SECTORS_ACROSS;
	}
	u32 xCrossfadeIndex = AUD_CENTER_SECTOR - 1;
	if (ratioCtoR>0.0f)
	{
		xCrossfadeIndex = AUD_CENTER_SECTOR + 1;
		diagonalCrossfadeIndex = yCrossfadeIndex+1;
	}
	else
	{
		diagonalCrossfadeIndex = yCrossfadeIndex-1;
	}

	UpdateBuiltUpMetrics(ratioCtoR, ratioCtoU, xCrossfadeIndex, yCrossfadeIndex, diagonalCrossfadeIndex);
	UpdateBuildingDensityMetrics(ratioCtoR, ratioCtoU, xCrossfadeIndex, yCrossfadeIndex, diagonalCrossfadeIndex);
	UpdateTreeDensityMetrics(ratioCtoR, ratioCtoU, xCrossfadeIndex, yCrossfadeIndex, diagonalCrossfadeIndex);
	UpdateWaterMetrics(ratioCtoR, ratioCtoU, xCrossfadeIndex, yCrossfadeIndex, diagonalCrossfadeIndex);
	UpdateHighwayMetrics(ratioCtoR, ratioCtoU, xCrossfadeIndex, yCrossfadeIndex, diagonalCrossfadeIndex);

	if(m_RefreshHighwayFactor)
	{
		m_HighwayZoneToRefresh++;

		if(m_HighwayZoneToRefresh >= AUD_NUM_SECTORS)
		{
			m_RefreshHighwayFactor = false;
		}
	}
}

void naEnvironment::UpdateBuiltUpMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex)
{
	m_BuiltUpSmoother.SetRate(g_BuiltUpSmootherRate);
	m_BuiltUpDampingFactorSmoother.SetRates(g_BuiltUpDampingFactorSmoothUpRate/1000.0f, g_BuiltUpDampingFactorSmoothDownRate/1000.0f);

	f32 xRatio = Abs(ratioCtoR);
	f32 yRatio = Abs(ratioCtoU);
	f32 centreHeight = GetAveragedBuildingHeightForIndex(AUD_CENTER_SECTOR);
	f32 xOffsetHeight = GetAveragedBuildingHeightForIndex(xCrossfadeIndex);
	f32 yOffsetHeight = GetAveragedBuildingHeightForIndex(yCrossfadeIndex);
	f32 diagOffsetHeight = GetAveragedBuildingHeightForIndex(diagonalCrossfadeIndex);

	f32 baseXOffsetHeight = audCurveRepository::GetLinearInterpolatedValue(centreHeight, xOffsetHeight, 0.0f, 1.0f, xRatio);
	f32 yOffsetXOffsetHeight = audCurveRepository::GetLinearInterpolatedValue(yOffsetHeight, diagOffsetHeight, 0.0f, 1.0f, xRatio);
	f32 finalHeight = audCurveRepository::GetLinearInterpolatedValue(baseXOffsetHeight, yOffsetXOffsetHeight, 0.0f, 1.0f, yRatio);

	f32 builtUpFactor = m_HeightToBuiltUpFactor.CalculateValue(finalHeight);
	f32 timeSmoothedBuiltUpFactor = m_BuiltUpSmoother.CalculateValue(builtUpFactor, audNorthAudioEngine::GetCurrentTimeInMs());

	m_BuiltUpFactor = timeSmoothedBuiltUpFactor;

	f32 directionalBuildingHeight[4];

	directionalBuildingHeight[0] = GetAveragedBuildingHeightFromXYOffset(0.0f+ratioCtoR, g_NSEW_Offset+ratioCtoU);
	directionalBuildingHeight[1] = GetAveragedBuildingHeightFromXYOffset(g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);
	directionalBuildingHeight[2] = GetAveragedBuildingHeightFromXYOffset(0.0f+ratioCtoR, -g_NSEW_Offset+ratioCtoU);
	directionalBuildingHeight[3] = GetAveragedBuildingHeightFromXYOffset(-g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);

	for (u32 i=0; i<4; i++)
	{
		m_DirectionalBuiltUpFactor[i] = m_BuiltUpDirectionalSmoother[i].CalculateValue(m_HeightToBuiltUpFactor.CalculateValue(directionalBuildingHeight[i]), audNorthAudioEngine::GetCurrentTimeInMs());
	}

#if __BANK
	if (g_DebugBuiltUpInfo)
	{
		static f32 x[AUD_NUM_SECTORS];
		static f32 y[AUD_NUM_SECTORS];
		for (u32 i=0; i<AUD_NUM_SECTORS_ACROSS; i++)
		{
			for (u32 j=0; j<AUD_NUM_SECTORS_ACROSS; j++)
			{
				x[i*AUD_NUM_SECTORS_ACROSS+j] = 100.0f + (800.0f/AUD_NUM_SECTORS_ACROSS)*j;
				y[i*AUD_NUM_SECTORS_ACROSS+j] = 100.0f + (700.0f/AUD_NUM_SECTORS_ACROSS)*i;
			}
		}
		static f32 heights[AUD_NUM_SECTORS][2];
		static audMeterList meterList[AUD_NUM_SECTORS];
		static const char* noName[2] = {"",""};
		for (u32 i=0; i<AUD_NUM_SECTORS; i++)
		{
			heights[i][0] = m_BuildingHeights[i] / 256.0f;
			heights[i][1] = m_HeightToBuiltUpFactor.CalculateValue(m_BuildingHeights[i]);
			meterList[i].left = x[i];
			meterList[i].bottom = y[i];
			meterList[i].width = 10.f;
			meterList[i].height = 40.f;
			meterList[i].values = &(heights[i][0]);
			meterList[i].names = &noName[0];
			meterList[i].numValues = 2;
			audNorthAudioEngine::DrawLevelMeters(&(meterList[i])); 
		}

		static f32 localBuiltUpValue;
		static audMeterList localBuiltUpMeter;
		static const char* localBuiltUpMeterName = "C";
		localBuiltUpValue = m_BuiltUpFactor;
		localBuiltUpMeter.left = x[AUD_NUM_SECTORS-1] + 150;
		localBuiltUpMeter.bottom = y[AUD_NUM_SECTORS/2];
		localBuiltUpMeter.width = 10.f;
		localBuiltUpMeter.height = 40.f;
		localBuiltUpMeter.values = &localBuiltUpValue;
		localBuiltUpMeter.numValues = 1;
		localBuiltUpMeter.names = &localBuiltUpMeterName;
		audNorthAudioEngine::DrawLevelMeters(&localBuiltUpMeter); 

		const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
		Vector3 right = listenerMatrix.a;

		right.NormalizeFast();
		const f32 cosangle = right.Dot(g_Directions[AUD_AMB_DIR_NORTH]);
		const f32 angle = AcosfSafe(cosangle);
		const f32 degrees = RtoD*angle;
		f32 actualDegrees;
		if(right.Dot(g_Directions[AUD_AMB_DIR_EAST]) <= 0.0f)
		{
			actualDegrees = 360.0f - degrees;
		}
		else
		{
			actualDegrees = degrees;
		}

		//Adjust the degrees so North is 0 degrees and get it between 0-360
		actualDegrees += 270.f;
		while(actualDegrees > 360.0f)
		{
			actualDegrees -= 360.0f;
		}

		static audMeterList dirMeterLists[4];
		static const char *dirNamesList[4] = {"N", "E", "S", "W"};
		static const f32 meterBasePan[4] = {0, 90, 180, 270};
		static f32 dirBuiltUpValues[4];

		for(u32 i = 0; i < 4; i++)
		{
			dirBuiltUpValues[i] = m_DirectionalBuiltUpFactor[i];

			f32 cirleDegrees = meterBasePan[i] + (360.0f - actualDegrees) + 270.0f;
			while(cirleDegrees > 360.0f)
			{
				cirleDegrees -= 360.0f;
			}
			const f32 angle = cirleDegrees * (PI/180);

			dirMeterLists[i].left = x[AUD_NUM_SECTORS-1] + 150 + (75.f * rage::Cosf(angle));
			dirMeterLists[i].width = 10.f;
			dirMeterLists[i].bottom = y[AUD_NUM_SECTORS/2] + (75.f * rage::Sinf(angle));
			dirMeterLists[i].height = 40.f;
			dirMeterLists[i].names = &dirNamesList[i];
			dirMeterLists[i].values = &dirBuiltUpValues[i];
			dirMeterLists[i].numValues = 1;
			audNorthAudioEngine::DrawLevelMeters(&dirMeterLists[i]);
		}
	}
	if (g_DisplayBuiltUpFactors)
	{
		char builtUpFactor[256] = "";
//		sprintf(builtUpFactor, "bUF: %f; ht: %f; ySec: %d, xSec: %d; diagSec: %d; yR: %f; xR: %f", m_BuiltUpFactor, finalHeight, 
//			yCrossfadeIndex, xCrossfadeIndex, diagonalCrossfadeIndex, yRatio, xRatio);
		sprintf(builtUpFactor, "Centre: %f; North: %f; East: %f; South: %f; West: %f", m_BuiltUpFactor, m_DirectionalBuiltUpFactor[0], m_DirectionalBuiltUpFactor[1], m_DirectionalBuiltUpFactor[2], m_DirectionalBuiltUpFactor[3]);
		grcDebugDraw::PrintToScreenCoors(builtUpFactor, 25,30);
	}
#endif

	// Until I can do an audio sync, override this good stuff with something shit
	if (!g_UseProperBuiltUpFactor)
	{
		if (g_ShouldOverrideBuiltUpFactor)
		{
			m_BuiltUpFactor = g_OverrideBuiltUpFactor;
			for (u32 i=0; i<4; i++)
			{
				m_DirectionalBuiltUpFactor[i] = g_OverrideBuiltUpFactor;
			}
		}
//		else if (CMapAreas::IsPositionInNamedArea(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()), atStringhash("Manhattan")))
//		{
//			m_BuiltUpFactor = 1.0f;
//		}
		else
		{
			m_BuiltUpFactor = 0.5f;
		}
	}

	// If we're in an interior, damp down all our built-up factors.
	f32 builtUpDampingFactor = 1.0f;
	bool isInteriorASubway = false;
	if (AreWeInAnInterior(&isInteriorASubway))
	{
		if (!isInteriorASubway)
		{
			builtUpDampingFactor = 0.0f;
		}
	}
	f32 builtUpDampingFactorSmoothed = m_BuiltUpDampingFactorSmoother.CalculateValue(builtUpDampingFactor, audNorthAudioEngine::GetCurrentTimeInMs());

	// Do useful stuff with the metric
	g_AudioEngine.GetEnvironment().SetBuiltUpFactor(m_BuiltUpFactor);
	g_AudioEngine.GetEnvironment().SetBuiltUpDirectionFactors(m_DirectionalBuiltUpFactor);
	const f32 builtUpRolloff = m_BuiltUpToRolloff.CalculateValue(m_BuiltUpFactor);
	f32 plateauScale = 1.0f;
	if(naVerifyf(builtUpRolloff != 0.0f, "'BUILT_UP_TO_ROLLOFF' curve returned 0.0f, needs to be non-zero"))
	{
		plateauScale = 1.0f / builtUpRolloff;
	}
	g_AudioEngine.GetEnvironment().SetGlobalRolloffPlateauScale(plateauScale);
	g_AudioEngine.GetEnvironment().SetRolloffDampingFactor(builtUpDampingFactorSmoothed*g_BuiltUpDampingFactorOverride);
}

void naEnvironment::UpdateBuildingDensityMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex)
{
	m_BuildingDensitySmoother.SetRate(g_BuiltUpSmootherRate);

	f32 xRatio = Abs(ratioCtoR);
	f32 yRatio = Abs(ratioCtoU);
	f32 centreCount = GetAveragedBuildingCountForIndex(AUD_CENTER_SECTOR);
	f32 xOffsetCount = GetAveragedBuildingCountForIndex(xCrossfadeIndex);
	f32 yOffsetCount = GetAveragedBuildingCountForIndex(yCrossfadeIndex);
	f32 diagOffsetCount = GetAveragedBuildingCountForIndex(diagonalCrossfadeIndex);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(centreCount, xOffsetCount, 0.0f, 1.0f, xRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetCount, diagOffsetCount, 0.0f, 1.0f, xRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(baseXOffsetCount, yOffsetXOffsetCount, 0.0f, 1.0f, yRatio);

	f32 buldingDensity = m_BuildingNumToDensityFactor.CalculateValue(finalCount);
	f32 timeSmoothedDensity = m_BuildingDensitySmoother.CalculateValue(buldingDensity, audNorthAudioEngine::GetCurrentTimeInMs());

	m_BuildingDensityFactor = timeSmoothedDensity;

	f32 directionalBuildingDensity[4];

	directionalBuildingDensity[0] = GetAveragedBuildingCountFromXYOffset(0.0f+ratioCtoR, g_NSEW_Offset+ratioCtoU);
	directionalBuildingDensity[1] = GetAveragedBuildingCountFromXYOffset(g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);
	directionalBuildingDensity[2] = GetAveragedBuildingCountFromXYOffset(0.0f+ratioCtoR, -g_NSEW_Offset+ratioCtoU);
	directionalBuildingDensity[3] = GetAveragedBuildingCountFromXYOffset(-g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);

	for (u32 i=0; i<4; i++)
	{
		m_DirectionalBuildingDensity[i] = m_BuildingDensityDirectionalSmoother[i].CalculateValue(m_BuildingNumToDensityFactor.CalculateValue(directionalBuildingDensity[i]), audNorthAudioEngine::GetCurrentTimeInMs());
	}
}

void naEnvironment::UpdateTreeDensityMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex)
{
	m_TreeDensitySmoother.SetRate(g_BuiltUpSmootherRate);

	f32 xRatio = Abs(ratioCtoR);
	f32 yRatio = Abs(ratioCtoU);
	f32 centreCount = GetAveragedTreeCountForIndex(AUD_CENTER_SECTOR);
	f32 xOffsetCount = GetAveragedTreeCountForIndex(xCrossfadeIndex);
	f32 yOffsetCount = GetAveragedTreeCountForIndex(yCrossfadeIndex);
	f32 diagOffsetCount = GetAveragedTreeCountForIndex(diagonalCrossfadeIndex);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(centreCount, xOffsetCount, 0.0f, 1.0f, xRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetCount, diagOffsetCount, 0.0f, 1.0f, xRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(baseXOffsetCount, yOffsetXOffsetCount, 0.0f, 1.0f, yRatio);

	f32 treeDensity = m_TreeNumToDensityFactor.CalculateValue(finalCount);
	f32 timeSmoothedDensity = m_TreeDensitySmoother.CalculateValue(treeDensity, audNorthAudioEngine::GetCurrentTimeInMs());

	m_TreeDensityFactor = timeSmoothedDensity;

	f32 directionalTreeDensity[4];

	directionalTreeDensity[0] = GetAveragedTreeCountFromXYOffset(0.0f+ratioCtoR, g_NSEW_Offset+ratioCtoU);
	directionalTreeDensity[1] = GetAveragedTreeCountFromXYOffset(g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);
	directionalTreeDensity[2] = GetAveragedTreeCountFromXYOffset(0.0f+ratioCtoR, -g_NSEW_Offset+ratioCtoU);
	directionalTreeDensity[3] = GetAveragedTreeCountFromXYOffset(-g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);

	for (u32 i=0; i<4; i++)
	{
		m_DirectionalTreeDensity[i] = m_TreeDensityDirectionalSmoother[i].CalculateValue(m_TreeNumToDensityFactor.CalculateValue(directionalTreeDensity[i]), audNorthAudioEngine::GetCurrentTimeInMs());
	}
}

void naEnvironment::UpdateWaterMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex)
{
	m_WaterSmoother.SetRate(g_BuiltUpSmootherRate);

	f32 xRatio = Abs(ratioCtoR);
	f32 yRatio = Abs(ratioCtoU);
	f32 centreCount = GetAveragedWaterAmountForIndex(AUD_CENTER_SECTOR);
	f32 xOffsetCount = GetAveragedWaterAmountForIndex(xCrossfadeIndex);
	f32 yOffsetCount = GetAveragedWaterAmountForIndex(yCrossfadeIndex);
	f32 diagOffsetCount = GetAveragedWaterAmountForIndex(diagonalCrossfadeIndex);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(centreCount, xOffsetCount, 0.0f, 1.0f, xRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetCount, diagOffsetCount, 0.0f, 1.0f, xRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(baseXOffsetCount, yOffsetXOffsetCount, 0.0f, 1.0f, yRatio);

	f32 waterFactor = m_WaterAmountToWaterFactor.CalculateValue(finalCount);
	f32 timeSmoothedFactor = m_WaterSmoother.CalculateValue(waterFactor, audNorthAudioEngine::GetCurrentTimeInMs());

	m_WaterFactor = timeSmoothedFactor;

	f32 directionalWaterAmount[4];

	directionalWaterAmount[0] = GetAveragedWaterAmountFromXYOffset(0.0f+ratioCtoR, g_NSEW_Offset+ratioCtoU);
	directionalWaterAmount[1] = GetAveragedWaterAmountFromXYOffset(g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);
	directionalWaterAmount[2] = GetAveragedWaterAmountFromXYOffset(0.0f+ratioCtoR, -g_NSEW_Offset+ratioCtoU);
	directionalWaterAmount[3] = GetAveragedWaterAmountFromXYOffset(-g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);

	for (u32 i=0; i<4; i++)
	{
		m_DirectionalWaterFactor[i] = m_WaterDirectionalSmoother[i].CalculateValue(m_WaterAmountToWaterFactor.CalculateValue(directionalWaterAmount[i]), audNorthAudioEngine::GetCurrentTimeInMs());
	}
}

void naEnvironment::UpdateHighwayMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex)
{
	m_HighwaySmoother.SetRate(g_BuiltUpSmootherRate);

	f32 xRatio = Abs(ratioCtoR);
	f32 yRatio = Abs(ratioCtoU);
	f32 centreCount = GetAveragedHighwayAmountForIndex(AUD_CENTER_SECTOR);
	f32 xOffsetCount = GetAveragedHighwayAmountForIndex(xCrossfadeIndex);
	f32 yOffsetCount = GetAveragedHighwayAmountForIndex(yCrossfadeIndex);
	f32 diagOffsetCount = GetAveragedHighwayAmountForIndex(diagonalCrossfadeIndex);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(centreCount, xOffsetCount, 0.0f, 1.0f, xRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetCount, diagOffsetCount, 0.0f, 1.0f, xRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(baseXOffsetCount, yOffsetXOffsetCount, 0.0f, 1.0f, yRatio);

	f32 highwayFactor = m_HighwayAmountToHighwayFactor.CalculateValue(finalCount);
	f32 timeSmoothedFactor = m_HighwaySmoother.CalculateValue(highwayFactor, audNorthAudioEngine::GetCurrentTimeInMs());

	m_HighwayFactor = timeSmoothedFactor;

	f32 directionalHighwayAmount[4];

	directionalHighwayAmount[0] = GetAveragedHighwayAmountFromXYOffset(0.0f+ratioCtoR, g_NSEW_Offset+ratioCtoU);
	directionalHighwayAmount[1] = GetAveragedHighwayAmountFromXYOffset(g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);
	directionalHighwayAmount[2] = GetAveragedHighwayAmountFromXYOffset(0.0f+ratioCtoR, -g_NSEW_Offset+ratioCtoU);
	directionalHighwayAmount[3] = GetAveragedHighwayAmountFromXYOffset(-g_NSEW_Offset+ratioCtoR, 0.0f+ratioCtoU);

	for (u32 i=0; i<4; i++)
	{
		m_DirectionalHighwayFactor[i] = m_HighwayDirectionalSmoother[i].CalculateValue(m_HighwayAmountToHighwayFactor.CalculateValue(directionalHighwayAmount[i]), audNorthAudioEngine::GetCurrentTimeInMs());
	}
}

f32 naEnvironment::GetBuildingHeightForSector(u32 sectorX, u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sector = sectorX + (sectorY*g_audNumWorldSectorsX);
		return GetBuildingHeightForSector(sector);
//		rain.x = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f));
//		rain.y = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f));
	}
	else
	{
		// invalid sector - default to water (since the map is surrounded by water)
		return 0.0f;
	}
}

f32 naEnvironment::GetBuildingCountForSector(u32 sectorX, u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sector = sectorX + (sectorY*g_audNumWorldSectorsX);
		return GetBuildingCountForSector(sector);
	}
	else
	{
		// invalid sector - default to water (since the map is surrounded by water)
		return 0.0f;
	}
}

f32 naEnvironment::GetTreeCountForSector(u32 sectorX, u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sector = sectorX + (sectorY*g_audNumWorldSectorsX);
		return GetTreeCountForSector(sector);
	}
	else
	{
		// invalid sector - default to water (since the map is surrounded by water)
		return 0.0f;
	}
}

f32 naEnvironment::GetWaterAmountForSector(u32 sectorX, u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sector = sectorX + (sectorY*g_audNumWorldSectorsX);
		return GetWaterAmountForSector(sector);
	}
	else
	{
		// invalid sector - default to water (since the map is surrounded by water)
		return 1.0f;
	}
}

f32 naEnvironment::GetHighwayAmountForSector(u32 sectorX, u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sector = sectorX + (sectorY*g_audNumWorldSectorsX);
		return GetHighwayAmountForSector(sector);
	}
	else
	{
		// invalid sector
		return 0.0f;
	}
}

f32 naEnvironment::GetAveragedBuildingHeightForIndex(u32 index)
{
	// Straight average the nine heights around the requested one - we can maybe do better than this, and bias to the centre
	f32 totalHeight = 0.0f;
	for (s32 i=-AUD_NUM_SECTORS_ACROSS; i<=AUD_NUM_SECTORS_ACROSS; i+=AUD_NUM_SECTORS_ACROSS)
	{
		for (s32 j=-1; j<2; j++)
		{
			s32 finalIndex = index+j+i; 
			if (finalIndex >= 0 && finalIndex < AUD_NUM_SECTORS)
			{
				totalHeight += m_BuildingHeights[index+j+i];
			}
		}
	}
	f32 averageHeight = totalHeight/9.0f;
	return averageHeight;
}

f32 naEnvironment::GetAveragedBuildingCountForIndex(u32 index)
{
	// Straight average the nine counts around the requested one - we can maybe do better than this, and bias to the centre
	f32 totalCount = 0.0f;
	for (s32 i=-AUD_NUM_SECTORS_ACROSS; i<=AUD_NUM_SECTORS_ACROSS; i+=AUD_NUM_SECTORS_ACROSS)
	{
		for (s32 j=-1; j<2; j++)
		{
			s32 finalIndex = index+j+i; 
			if (finalIndex >= 0 && finalIndex < AUD_NUM_SECTORS)
			{
				totalCount += m_BuildingCount[index+j+i];
			}
		}
	}
	f32 averageCount = totalCount/9.0f;
	return averageCount;
}

f32 naEnvironment::GetAveragedTreeCountForIndex(u32 index)
{
	// Straight average the nine counts around the requested one - we can maybe do better than this, and bias to the centre
	f32 totalCount = 0.0f;
	for (s32 i=-AUD_NUM_SECTORS_ACROSS; i<=AUD_NUM_SECTORS_ACROSS; i+=AUD_NUM_SECTORS_ACROSS)
	{
		for (s32 j=-1; j<2; j++)
		{
			s32 finalIndex = index+j+i; 
			if (finalIndex >= 0 && finalIndex < AUD_NUM_SECTORS)
			{
				totalCount += m_TreeCount[index+j+i];
			}
		}
	}
	f32 averageCount = totalCount/9.0f;
	return averageCount;
}


f32 naEnvironment::GetAveragedWaterAmountForIndex(u32 index)
{
	// Straight average the nine counts around the requested one - we can maybe do better than this, and bias to the centre
	f32 totalCount = 0.0f;
	for (s32 i=-AUD_NUM_SECTORS_ACROSS; i<=AUD_NUM_SECTORS_ACROSS; i+=AUD_NUM_SECTORS_ACROSS)
	{
		for (s32 j=-1; j<2; j++)
		{
			s32 finalIndex = index+j+i; 
			if (finalIndex >= 0 && finalIndex < AUD_NUM_SECTORS)
			{
				totalCount += m_WaterAmount[index+j+i];
			}
		}
	}
	f32 averageCount = totalCount/9.0f;
	return averageCount;
}

f32 naEnvironment::GetAveragedHighwayAmountForIndex(u32 index)
{
	// Straight average the nine counts around the requested one - we can maybe do better than this, and bias to the centre
	f32 totalCount = 0.0f;
	for (s32 i=-AUD_NUM_SECTORS_ACROSS; i<=AUD_NUM_SECTORS_ACROSS; i+=AUD_NUM_SECTORS_ACROSS)
	{
		for (s32 j=-1; j<2; j++)
		{
			s32 finalIndex = index+j+i; 
			if (finalIndex >= 0 && finalIndex < AUD_NUM_SECTORS)
			{
				totalCount += m_HighwayAmount[index+j+i];
			}
		}
	}
	f32 averageCount = totalCount/9.0f;
	return averageCount;
}

f32 naEnvironment::GetAveragedBuildingHeightFromXYOffset(f32 xOffset, f32 yOffset)
{
	// Check we're not straying out of bounds
// valid situation now that the player can drive outside the world
// it returns 0.0 (the height of the water) so everything should be fine
//	Assert(Abs(xOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));
//	Assert(Abs(yOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));

	if (Abs(xOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS) || Abs(yOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS))
	{
		return 0.0f;
	}

	// Work out which main square we're in, then which ones to combine, and by how much
	u32 primaryXIndexOffset = (u32)Abs(xOffset);
	f32 primaryToSecondaryXRatio = 1.0f - (Abs(xOffset) - (f32)primaryXIndexOffset);

	u32 primaryYIndexOffset = (u32)Abs(yOffset);
	f32 primaryToSecondaryYRatio = 1.0f - (Abs(yOffset) - (f32)primaryYIndexOffset);

	u32 primarySquare	 = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) + 
												(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryXSquare = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
												(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryYSquare = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) +
												((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 diagSquare		 = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
												((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);

	f32 primaryHeight = GetAveragedBuildingHeightForIndex(primarySquare);
	f32 xOffsetHeight = GetAveragedBuildingHeightForIndex(secondaryXSquare);
	f32 yOffsetHeight = GetAveragedBuildingHeightForIndex(secondaryYSquare);
	f32 diagOffsetHeight = GetAveragedBuildingHeightForIndex(diagSquare);

	f32 baseXOffsetHeight = audCurveRepository::GetLinearInterpolatedValue(xOffsetHeight, primaryHeight, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 yOffsetXOffsetHeight = audCurveRepository::GetLinearInterpolatedValue(diagOffsetHeight, yOffsetHeight, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 finalHeight = audCurveRepository::GetLinearInterpolatedValue(yOffsetXOffsetHeight, baseXOffsetHeight, 0.0f, 1.0f, primaryToSecondaryYRatio);

	return finalHeight;
}

f32 naEnvironment::GetAveragedBuildingCountFromXYOffset(f32 xOffset, f32 yOffset)
{
	// Check we're not straying out of bounds
	// valid situation now that the player can drive outside the world
	// it returns 0.0 (the height of the water) so everything should be fine
	//	Assert(Abs(xOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));
	//	Assert(Abs(yOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));

	if (Abs(xOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS) || Abs(yOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS))
	{
		return 0.0f;
	}

	// Work out which main square we're in, then which ones to combine, and by how much
	u32 primaryXIndexOffset = (u32)Abs(xOffset);
	f32 primaryToSecondaryXRatio = 1.0f - (Abs(xOffset) - (f32)primaryXIndexOffset);

	u32 primaryYIndexOffset = (u32)Abs(yOffset);
	f32 primaryToSecondaryYRatio = 1.0f - (Abs(yOffset) - (f32)primaryYIndexOffset);

	u32 primarySquare	 = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) + 
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryXSquare = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryYSquare = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 diagSquare		 = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);

	f32 primaryCount = GetAveragedBuildingCountForIndex(primarySquare);
	f32 xOffsetCount = GetAveragedBuildingCountForIndex(secondaryXSquare);
	f32 yOffsetCount = GetAveragedBuildingCountForIndex(secondaryYSquare);
	f32 diagOffsetCount = GetAveragedBuildingCountForIndex(diagSquare);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(xOffsetCount, primaryCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(diagOffsetCount, yOffsetCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetXOffsetCount, baseXOffsetCount, 0.0f, 1.0f, primaryToSecondaryYRatio);

	return finalCount;
}

f32 naEnvironment::GetAveragedTreeCountFromXYOffset(f32 xOffset, f32 yOffset)
{
	// Check we're not straying out of bounds
	// valid situation now that the player can drive outside the world
	// it returns 0.0 (the height of the water) so everything should be fine
	//	Assert(Abs(xOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));
	//	Assert(Abs(yOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));

	if (Abs(xOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS) || Abs(yOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS))
	{
		return 0.0f;
	}

	// Work out which main square we're in, then which ones to combine, and by how much
	u32 primaryXIndexOffset = (u32)Abs(xOffset);
	f32 primaryToSecondaryXRatio = 1.0f - (Abs(xOffset) - (f32)primaryXIndexOffset);

	u32 primaryYIndexOffset = (u32)Abs(yOffset);
	f32 primaryToSecondaryYRatio = 1.0f - (Abs(yOffset) - (f32)primaryYIndexOffset);

	u32 primarySquare	 = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) + 
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryXSquare = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryYSquare = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 diagSquare		 = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);

	f32 primaryCount = GetAveragedTreeCountForIndex(primarySquare);
	f32 xOffsetCount = GetAveragedTreeCountForIndex(secondaryXSquare);
	f32 yOffsetCount = GetAveragedTreeCountForIndex(secondaryYSquare);
	f32 diagOffsetCount = GetAveragedTreeCountForIndex(diagSquare);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(xOffsetCount, primaryCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(diagOffsetCount, yOffsetCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetXOffsetCount, baseXOffsetCount, 0.0f, 1.0f, primaryToSecondaryYRatio);

	return finalCount;
}

f32 naEnvironment::GetAveragedWaterAmountFromXYOffset(f32 xOffset, f32 yOffset)
{
	// Check we're not straying out of bounds
	// valid situation now that the player can drive outside the world
	// it returns 0.0 (the height of the water) so everything should be fine
	//	Assert(Abs(xOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));
	//	Assert(Abs(yOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));

	if (Abs(xOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS) || Abs(yOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS))
	{
		return 0.0f;
	}

	// Work out which main square we're in, then which ones to combine, and by how much
	u32 primaryXIndexOffset = (u32)Abs(xOffset);
	f32 primaryToSecondaryXRatio = 1.0f - (Abs(xOffset) - (f32)primaryXIndexOffset);

	u32 primaryYIndexOffset = (u32)Abs(yOffset);
	f32 primaryToSecondaryYRatio = 1.0f - (Abs(yOffset) - (f32)primaryYIndexOffset);

	u32 primarySquare	 = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) + 
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryXSquare = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryYSquare = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 diagSquare		 = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);

	f32 primaryCount = GetAveragedWaterAmountForIndex(primarySquare);
	f32 xOffsetCount = GetAveragedWaterAmountForIndex(secondaryXSquare);
	f32 yOffsetCount = GetAveragedWaterAmountForIndex(secondaryYSquare);
	f32 diagOffsetCount = GetAveragedWaterAmountForIndex(diagSquare);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(xOffsetCount, primaryCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(diagOffsetCount, yOffsetCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetXOffsetCount, baseXOffsetCount, 0.0f, 1.0f, primaryToSecondaryYRatio);

	return finalCount;
}

f32 naEnvironment::GetAveragedHighwayAmountFromXYOffset(f32 xOffset, f32 yOffset)
{
	// Check we're not straying out of bounds
	// valid situation now that the player can drive outside the world
	// it returns 0.0 (the height of the water) so everything should be fine
	//	Assert(Abs(xOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));
	//	Assert(Abs(yOffset)<(f32)(AUD_HALF_NUM_SECTORS_ACROSS));

	if (Abs(xOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS) || Abs(yOffset)>=(f32)(AUD_HALF_NUM_SECTORS_ACROSS))
	{
		return 0.0f;
	}

	// Work out which main square we're in, then which ones to combine, and by how much
	u32 primaryXIndexOffset = (u32)Abs(xOffset);
	f32 primaryToSecondaryXRatio = 1.0f - (Abs(xOffset) - (f32)primaryXIndexOffset);

	u32 primaryYIndexOffset = (u32)Abs(yOffset);
	f32 primaryToSecondaryYRatio = 1.0f - (Abs(yOffset) - (f32)primaryYIndexOffset);

	u32 primarySquare	 = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) + 
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryXSquare = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		(primaryYIndexOffset*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 secondaryYSquare = AUD_CENTER_SECTOR +  primaryXIndexOffset*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);
	u32 diagSquare		 = AUD_CENTER_SECTOR + (primaryXIndexOffset+1)*(xOffset>0.0f?1:-1) +
		((primaryYIndexOffset+1)*(yOffset<0.0f?1:-1)*AUD_NUM_SECTORS_ACROSS);

	f32 primaryCount = GetAveragedHighwayAmountForIndex(primarySquare);
	f32 xOffsetCount = GetAveragedHighwayAmountForIndex(secondaryXSquare);
	f32 yOffsetCount = GetAveragedHighwayAmountForIndex(secondaryYSquare);
	f32 diagOffsetCount = GetAveragedHighwayAmountForIndex(diagSquare);

	f32 baseXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(xOffsetCount, primaryCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 yOffsetXOffsetCount = audCurveRepository::GetLinearInterpolatedValue(diagOffsetCount, yOffsetCount, 0.0f, 1.0f, primaryToSecondaryXRatio);
	f32 finalCount = audCurveRepository::GetLinearInterpolatedValue(yOffsetXOffsetCount, baseXOffsetCount, 0.0f, 1.0f, primaryToSecondaryYRatio);

	return finalCount;
}

void naEnvironment::MapDirectionalMetricsToSpeakers()
{
	Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	static Vector3 directions[4];
	directions[0] = Vector3(1.0f, 0.0f, 0.0f); //N
	directions[1] = Vector3(0.0f, -1.0f, 0.0f); //E
	directions[2] = Vector3(-1.0f, 0.0f, 0.0f); //S
	directions[3] = Vector3(0.0f, 1.0f, 0.0f); //W

	Vector3 speakerPosition[4]; // these are in some different space, where 1,0,0 is forwards, and 0,-1,0 is right.
	speakerPosition[0] = Vector3(0.7071f, 0.7071f, 0.0f); // FL
	speakerPosition[1] = Vector3(0.7071f, -0.7071f, 0.0f); // FR
	speakerPosition[2] = Vector3(-0.7071f, 0.7071f, 0.0f); // RL
	speakerPosition[3] = Vector3(-0.7071f, -0.7071f, 0.0f); // RR 

	// we're always NESW from that, so ignore any positional aspect
	listenerMatrix.d.Zero();

	// This does the same clipped dot-product followed by elevation leakage that the panning algorithm and listener reverb do. 
	// quick elevation check - will be the same elevation for all directions
	f32 elevation = Abs(listenerMatrix.c.z); // 1 for not at all elevated, 0 for fully

	f32 speakerBuiltUpFactor[4];
	f32 speakerBlockedFactor[4];
	f32 speakerBlockedLinearVolume[4];
	f32 speakerOWOFactor[4];
	for (u32 i=0; i<4; i++) // 4 speakers
	{
		speakerBuiltUpFactor[i] = 0.0f;
		speakerBlockedFactor[i] = 0.0f;
		speakerBlockedLinearVolume[i] = 0.0f;
		speakerOWOFactor[i] = 0.0f;
		for (u32 j=0; j<4; j++) // 4 directional metrics
		{
			// Where is the poly relative to us
			Vector3 directionRelative = directions[j];
			listenerMatrix.UnTransform(directionRelative);

			Vector3 positionOnUnitCircle = directionRelative;
			positionOnUnitCircle.z = 0.0f;
			positionOnUnitCircle.NormalizeSafe();
			f32 dotProd = positionOnUnitCircle.Dot(speakerPosition[i]);
			// Don't care about ones more than 90degs away
			dotProd = Max(0.0f, dotProd);
			// Scale our contribution by the elevation
			f32 contribution = ((1.0f - elevation) * (1.0f/4.0f)) + (elevation * dotProd * dotProd);
			speakerBuiltUpFactor[i] += (contribution * m_DirectionalBuiltUpFactor[j]);
			f32 exteriorOcclusionInDirection = audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection((audAmbienceDirection)j);
			speakerBlockedFactor[i] += (contribution * audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)j));
			speakerBlockedLinearVolume[i] += (contribution * audNorthAudioEngine::GetOcclusionManager()->GetBlockedLinearVolumeForDirection((audAmbienceDirection)j));
			speakerOWOFactor[i] += (contribution * exteriorOcclusionInDirection);
		}
		m_SpeakerBuiltUpFactors[i] = m_SpeakerBuiltUpSmoother[i].CalculateValue(speakerBuiltUpFactor[i], audNorthAudioEngine::GetCurrentTimeInMs());
		m_SpeakerBlockedFactor[i] = speakerBlockedFactor[i];
		m_SpeakerBlockedLinearVolume[i] = speakerBlockedLinearVolume[i];
		m_SpeakerOWOFactor[i] = speakerOWOFactor[i];
	}

#if __BANK
	if (g_DebugBuiltUpInfo)
	{
		char builtUpFactor[256] = "";
		sprintf(builtUpFactor, "el: %f; FL: %f; FR: %f; RL: %f; RR: %f", elevation, speakerBuiltUpFactor[0], speakerBuiltUpFactor[1], speakerBuiltUpFactor[2], speakerBuiltUpFactor[3]);
		grcDebugDraw::PrintToScreenCoors(builtUpFactor, 32,37);
	}
#endif

	g_AudioEngine.GetEnvironment().SetBuiltUpSpeakerFactors(m_SpeakerBuiltUpFactors);
	g_AudioEngine.GetEnvironment().SetBlockedSpeakerFactors(m_SpeakerBlockedFactor);
	g_AudioEngine.GetEnvironment().SetBlockedSpeakerLinearVolumes(m_SpeakerBlockedLinearVolume);
}

f32 naEnvironment::GetBuildingHeightForSector(const u32 sector) const
{
	return (f32)(g_AudioWorldSectors[sector].tallestBuilding);
}

f32 naEnvironment::GetBuildingCountForSector(const u32 sector) const
{
	return (f32)(g_AudioWorldSectors[sector].numBuildings);
}

f32 naEnvironment::GetTreeCountForSector(const u32 sector) const
{
	return (f32)(g_AudioWorldSectors[sector].numTrees);
}

f32 naEnvironment::GetWaterAmountForSector(const u32 sector) const
{
	if(g_AudioWorldSectors[sector].isWaterSector &&
	   g_AudioWorldSectors[sector].numTrees == 0 &&
	   g_AudioWorldSectors[sector].numBuildings == 0)
	{
		return 1.0f;
	}

	return 0.0f;
}

f32 naEnvironment::GetHighwayAmountForSector(const u32 sector) const
{
	return (f32)g_AudioWorldSectors[sector].numHighwayNodes;
}

f32 naEnvironment::GetRollOffFactor()
{
	return m_BuiltUpToRolloff.CalculateValue(g_AudioEngine.GetEnvironment().GetBuiltUpFactor());
}
bool naEnvironment::IsCameraShelteredFromRain() const
{
	// only return true when all probes are colliding
	for(u32 i = 0 ; i < VFXWEATHER_NUM_WEATHER_PROBES; i++)
	{
		if(!g_vfxWeather.GetWeatherProbeResult(i))
		{
			return false;
		}
	}
	return true;
}


f32 naEnvironment::GetSpeakerReverbWet(u32 speaker, u32 reverb)
{
	if(naVerifyf(speaker < 4, "Invalid speaker passed into GetSpeakerReverbWet; out of bounds")
		&& naVerifyf(reverb < 3, "Invalid reverb passed into GetSpeakerReverbWet; out of bounds"))
	{
		return m_SpeakerReverbWets[speaker][reverb];
	}
	return 0.f;
}

CEntity* naEnvironment::GetInterestingLocalObject(u32 index) 
{
	if(naVerifyf(index < g_MaxInterestingLocalObjects, "Invalid index passed into GetInterestingLocalObject; out of bounds"))
	{
		return m_InterestingLocalObjects[index].object;
	}
	return NULL;
}
const CEntity* naEnvironment::GetInterestingLocalObject(u32 index) const
{
	if(naVerifyf(index < g_MaxInterestingLocalObjects, "Invalid index passed into GetInterestingLocalObject; out of bounds"))
	{
		return m_InterestingLocalObjects[index].object;
	}
	return NULL;
}
void naEnvironment::GetInterestingLocalObjectInfo(u32 index,audInterestingObjectInfo &objectInfo) 
{
	if(naVerifyf(index < g_MaxInterestingLocalObjects, "Invalid index passed into GetInterestingLocalObject; out of bounds"))
	{
		objectInfo = m_InterestingLocalObjects[index];
	}
}
const audInterestingObjectInfo *naEnvironment::GetInterestingLocalObjectInfo(u32 index) const
{
	if(naVerifyf(index < g_MaxInterestingLocalObjects, "Invalid index passed into GetInterestingLocalObject; out of bounds"))
	{
		return &m_InterestingLocalObjects[index];
	}
	return NULL;
}
const ModelAudioCollisionSettings * naEnvironment::GetInterestingLocalObjectMaterial(u32 index) const
{
	const audInterestingObjectInfo *objectInfo = GetInterestingLocalObjectInfo(index);
	if(objectInfo && objectInfo->object)
	{
		if(objectInfo->macsComponent == -1)
		{
			return (objectInfo->object->GetBaseModelInfo()->GetAudioCollisionSettings());
		}
		else
		{
			return audCollisionAudioEntity::GetFragComponentMaterialSettings(objectInfo->object,(u32)objectInfo->macsComponent);
		}
	}
	return NULL;
}
CEntity *naEnvironment::GetInterestingLocalObjectCached(u32 index) 
{
	if(naVerifyf(index < m_NumInterestingLocalObjectsCached, "Invalid index passed into GetInterestingLocalObjectCached; out of bounds"))
	{
		return m_InterestingLocalObjectsCached[index].object;
	}
	return NULL;
}
naEnvironmentGroup *naEnvironment::GetInterestingLocalEnvGroup(u32 index) 
{
	if(naVerifyf(index < g_MaxInterestingLocalObjects, "Invalid index passed into GetInterestingLocalEnvGroupCached; out of bounds"))
	{
		return m_InterestingLocalObjects[index].envGroup;
	}
	return NULL;
}
naEnvironmentGroup *naEnvironment::GetInterestingLocalEnvGroupCached(u32 index) 
{
	if(naVerifyf(index < m_NumInterestingLocalObjectsCached, "Invalid index passed into GetInterestingLocalEnvGroupCached; out of bounds"))
	{
		return m_InterestingLocalObjectsCached[index].envGroup;
	}
	return NULL;
}
u32 naEnvironment::GetNumInterestingLocalObjectsCached() const
{
	return m_NumInterestingLocalObjectsCached;
}

bool naEnvironment::IsObjectUnderCover(u32 index)
{
	naAssertf(index < g_MaxInterestingLocalObjects, "Invalid index passed into GetInterestingLocalObject; out of bounds");
#if __BANK
	if(sm_UseCoverAproximation)
	{
		return m_InterestingLocalObjects[index].underCover;
	}
#endif
	if(m_InterestingLocalObjects[index].envGroup)
	{
		return m_InterestingLocalObjects[index].envGroup->IsUnderCover();
	}
	return true;
}

void naEnvironment::AddInterestingObject(CEntity *obj,const u32 shockwaveSoundHash, const u32 resoSoundHash,s32 component ,bool /*checkCovered*/)
{
	if(naVerifyf(obj, "Null CEntity ptr passed into AddInter estingObject"))
	{
		naAssertf(obj->GetOwnedBy()!=ENTITY_OWNEDBY_PROCEDURAL, "Found a procedural entity tagged with audio, please bug the audio team.");
		if(m_NumInterestingLocalObjectsCached < g_MaxInterestingLocalObjects)
		{
			naAssertf(!m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].object, "Trying to add an interesting object but slot is already occupied");
			// Set the entity.
			m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].object = obj;
			// Before setting anything up, check if we already had the info in the cached list.
			s32 objectIndex = -1;
			for(s32 i = g_MaxInterestingLocalObjects - 1; i >= 0; i--)
			{
				if(audNorthAudioEngine::GetGtaEnvironment()->GetInterestingLocalObject(i) == obj)
				{
					objectIndex = i;
					break;
				}
			}
			if( objectIndex != -1 )
			{
				audNorthAudioEngine::GetGtaEnvironment()->GetInterestingLocalObjectInfo(objectIndex,m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached]);
			}
			else
			{
				m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].macsComponent = component;
				m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].shockwaveSoundInfo.prevVol = 0.0f;
				if (shockwaveSoundHash != g_NullSoundHash)
				{
					bool hasCustomAttackReleaseTime = false;
					f32 customAttackTime = 0.f;
					f32 customReleaseTime = 0.f;

					Sound uncompressedMetadata;
					const VariableBlockSound *vbSound = SOUNDFACTORY.DecompressMetadata<VariableBlockSound>(shockwaveSoundHash, uncompressedMetadata);
					if (vbSound)
					{
						for (u32 i = 0; i < vbSound->numVariables; i++)
						{
							if (vbSound->Variable[i].VariableRef == ATSTRINGHASH("AttackTime", 0xF0104A91))
							{
								customAttackTime = vbSound->Variable[i].VariableValue;
								hasCustomAttackReleaseTime = true;
							}

							if (vbSound->Variable[i].VariableRef == ATSTRINGHASH("ReleaseTime", 0x19482A71))
							{								
								customReleaseTime = vbSound->Variable[i].VariableValue;
								hasCustomAttackReleaseTime = true;
							}
						}
					}
					
					m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].shockwaveSoundInfo.attackTime = hasCustomAttackReleaseTime ? Max(0.00001f, customAttackTime) : -1.f;
					m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].shockwaveSoundInfo.releaseTime = hasCustomAttackReleaseTime ? Max(0.00001f, customReleaseTime) : -1.f;
				}
				
				if(resoSoundHash != g_NullSoundHash)
				{
					Sound uncompressedMetadata;
					const EnvelopeSound *envSound = SOUNDFACTORY.DecompressMetadata<EnvelopeSound>(resoSoundHash, uncompressedMetadata);
					if(envSound) 
					{
						m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].resoSoundInfo.attackTime = (f32)audSound::ApplyVariance(envSound->Envelope.Attack, envSound->Envelope.AttackVariance, 1<<16U);
						m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].resoSoundInfo.attackTime *= 0.001f;
						m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].resoSoundInfo.releaseTime = (f32)audSound::ApplyVariance(envSound->Envelope.Release, envSound->Envelope.ReleaseVariance, 1<<16U);
						m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].resoSoundInfo.releaseTime *= 0.001f;
						m_InterestingLocalObjectsCached[m_NumInterestingLocalObjectsCached].resoSoundInfo.resoSoundRef = envSound->SoundRef;
					}
				}

			}
			m_NumInterestingLocalObjectsCached ++;
		}
	}
}
void naEnvironment::AddLocalTree(CEntity *tree)
{
	if (tree->GetTransform().GetPosition().GetZf() > m_WaterHeightAtListener &&  (tree->GetBoundRadius() > sm_BushesMinRadius) && (m_NumLocalTrees < (g_MaxInterestingLocalTrees - 1)))
	{
		//	Add it to the list
		m_LocalTrees[m_NumLocalTrees].tree = tree;
		// update our backGround tree info. 
		// First get the sector: FL,FR,RL,RR
		Vec3V treePos2D = tree->GetTransform().GetPosition();
		treePos2D.SetZ(0);
		Vec3V listenerPos2D = g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix().GetCol3();
		listenerPos2D.SetZ(0);
		Vec3V dirToTree = Subtract(treePos2D, listenerPos2D);
		//f32 distance = fabs(((ScalarV)Mag(dirToTree)).Getf());
		dirToTree = Normalize(dirToTree);
		float frontDot = Dot( Vec3V(0.0f,1.f,0.0f), dirToTree).Getf();
		float rightDot = Dot( Vec3V(1.f,0.0f,0.0f), dirToTree).Getf();

		audFBSectors sector = AUD_SECTOR_FL;
		
		if((frontDot >= 0.f))
		{
			if(rightDot >= 0.f)
				sector = AUD_SECTOR_FR;
			else
				sector = AUD_SECTOR_FL; 
		}
		else
		{
			if(rightDot >= 0.f)
				sector = AUD_SECTOR_RR; 
			else
				sector = AUD_SECTOR_RL; 
		}
		m_LocalTrees[m_NumLocalTrees].sector = sector;

		if(tree->GetBoundRadius() >= sm_BigTreesMinRadius)
		{
			m_BackGroundTrees[sector].numBigTrees ++ ;
			m_LocalTrees[m_NumLocalTrees ++].type = AUD_BIG_TREES;
		}
		else if(tree->GetBoundRadius() >= sm_SmallTreesMinRadius) 
		{
			m_BackGroundTrees[sector].numTrees ++ ;
			m_LocalTrees[m_NumLocalTrees ++].type = AUD_SMALL_TREES;
		}
		else
		{
			m_BackGroundTrees[sector].numBushes ++ ;
			m_LocalTrees[m_NumLocalTrees ++].type = AUD_BUSHES;
		}
	}
}

#if __DEV
void naEnvironment::DebugDraw()
{
}
#endif
	
#if __BANK
void naEnvironment::CreateRAVEInteriorRoomObject(const char* instanceName, const char* roomName)
{
	if(roomName)
	{
		char xmlMsg[4096];

		sprintf(xmlMsg, "<RAVEMessage>\n");
		naDisplayf("%s", xmlMsg);
		char tmpBuf[256] = {0};
		sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", (u32)audStringHash("BASE"));
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "		<InteriorRoom name=\"%s_%s\">\n", instanceName, roomName);
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<RoomName>%s</RoomName>\n", roomName);
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<ReverbSmall>%f</ReverbSmall>\n", g_DebugRoomSettingsReverbSmall);
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<ReverbMedium>%f</ReverbMedium>\n", g_DebugRoomSettingsReverbMedium);
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<ReverbLarge>%f</ReverbLarge>\n", g_DebugRoomSettingsReverbLarge);
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "		</InteriorRoom>\n");
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	</EditObjects>\n");
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "</RAVEMessage>\n");
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);

		naDisplayf("XML message is %" SIZETFMT "d characters long \n", strlen(xmlMsg));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
}

void naEnvironment::CreateRAVEObjectsForCurrentInteriorAndRooms()
{
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

	if(pIntInst)
	{
		for(u32 instanceRoomLoop = 0; instanceRoomLoop < pIntInst->GetNumRooms(); instanceRoomLoop++)
		{
			const char* instanceName = pIntInst->GetModelName();
			const char* roomName = pIntInst->GetRoomName(instanceRoomLoop);
			CreateRAVEInteriorRoomObject(instanceName, roomName);
		}

		CreateRAVEObjectsForCurrentInterior();
	}
}

void naEnvironment::CreateRAVEObjectsForCurrentInterior()
{
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

	if(pIntInst)
	{
		char xmlMsg[4096];
		const char* instanceName = pIntInst->GetModelName();

		sprintf(xmlMsg, "<RAVEMessage>\n");
		naDisplayf("%s", xmlMsg);
		char tmpBuf[256] = {0};
		sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", (u32)audStringHash("BASE"));
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "		<InteriorSettings name=\"%s\">\n", instanceName);
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);

		for(u32 instanceRoomLoop = 0; instanceRoomLoop < pIntInst->GetNumRooms(); instanceRoomLoop++)
		{
			const char* roomName = pIntInst->GetRoomName(instanceRoomLoop);

			sprintf(tmpBuf, "			<Room>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "				<Ref>%s_%s</Ref>\n", instanceName, roomName);
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "			</Room>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
		}

		sprintf(tmpBuf, "		</InteriorSettings>\n");
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	</EditObjects>\n");
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "</RAVEMessage>\n");
		naDisplayf("%s", tmpBuf);
		strcat(xmlMsg, tmpBuf);

		naDisplayf("XML message is %" SIZETFMT "d characters long \n", strlen(xmlMsg));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
}

void naEnvironment::CreateRAVEObjectsForCurrentRoom()
{
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

	if(pIntInst)
	{
		s32 roomIndex = CPortalVisTracker::GetPrimaryRoomIdx();

		if(roomIndex < pIntInst->GetNumRooms())
		{
			const char* instanceName = pIntInst->GetModelName();
			const char* roomName = pIntInst->GetRoomName(roomIndex);
			CreateRAVEInteriorRoomObject(instanceName, roomName);
		}
	}
}

void DebugDrawSector(const u32 sectorX, const u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sectorIndex = sectorX + (sectorY * g_audNumWorldSectorsX);

		Color32 colour(255,0,0);

		const f32 x = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f));
		const f32 y = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f));

		Vector3 v0,v1,v2,v3;

		v0.z = v1.z = v2.z = v3.z = 810.0f;

		v0.x = x;
		v0.y = y;

		v1.x = x + g_audWidthOfSector;
		v1.y = y;

		v2.x = x;
		v2.y = y + g_audDepthOfSector;

		v3.x = x + g_audWidthOfSector;
		v3.y = y + g_audDepthOfSector;

		Vector3 centre = (v0 + v3) * 0.5f;

		centre.z = v0.z = v1.z = v2.z = v3.z = 70.f;
		if(g_AudioWorldSectors[sectorIndex].isWaterSector)
		{
			colour = Color32(0,0,255);
		}

		grcDebugDraw::Line(v0,v1,colour);
		grcDebugDraw::Line(v1,v3,colour);
		grcDebugDraw::Line(v3,v2,colour);
		grcDebugDraw::Line(v2,v0,colour);

		char rainDebug[256] = "";

		formatf(rainDebug, 255, "%d: numHighwayNodes %d tallestBuilding: %d numBuildings: %d numTrees: %u", sectorIndex, g_AudioWorldSectors[sectorIndex].numHighwayNodes, g_AudioWorldSectors[sectorIndex].tallestBuilding, g_AudioWorldSectors[sectorIndex].numBuildings, g_AudioWorldSectors[sectorIndex].numTrees);

		grcDebugDraw::Text(centre, colour, rainDebug);
	}
}

void DebugDrawRoadInfo(const u32 sectorX, const u32 sectorY)
{
	if(sectorX < g_audNumWorldSectorsX && sectorY < g_audNumWorldSectorsY)
	{
		const u32 sectorIndex = sectorX + (sectorY * g_audNumWorldSectorsX);

		Color32 colour(255,0,0);

		const f32 x = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f));
		const f32 y = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f));

		Vector3 v0,v1;

		v0.x = x;
		v0.y = y;

		v1.x = x + g_audWidthOfSector;
		v1.y = y + g_audDepthOfSector;

		f32 height = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(v0.x, v0.y);
		v0.z = height - 20.0f;
		v1.z = height + 20.0f;

		if(g_AudioWorldSectors[sectorIndex].numHighwayNodes > 0)
		{
			colour.SetAlpha((u32)Clamp((s32)(255.0f * (g_AudioWorldSectors[sectorIndex].numHighwayNodes/50.0f)), 0, 255));
			grcDebugDraw::BoxAxisAligned(VECTOR3_TO_VEC3V(v0), VECTOR3_TO_VEC3V(v1), colour, true);
		}
	}
}

void naEnvironment::DrawDebug() const
{
	if(g_audDrawWorldSectorDebug)
	{
		const Vector3 camPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		const u32 sector = static_cast<u32>(ComputeWorldSectorIndex(camPos));

		const u32 sectorX = sector % g_audNumWorldSectorsX;
		const u32 sectorY = (sector - sectorX)/g_audNumWorldSectorsX;

		DebugDrawSector(sectorX-1, sectorY-1);
		DebugDrawSector(sectorX-1, sectorY);
		DebugDrawSector(sectorX-1, sectorY+1);
		DebugDrawSector(sectorX, sectorY-1);
		DebugDrawSector(sectorX, sectorY);
		DebugDrawSector(sectorX, sectorY+1);
		DebugDrawSector(sectorX+1, sectorY);
		DebugDrawSector(sectorX+1, sectorY+1);	
		DebugDrawSector(sectorX+1, sectorY-1);	

	}
	if(g_audResetWorldSectorRoadInfo)
	{
		for(u32 x = 0; x < g_audNumWorldSectorsX; x++)
		{
			for(u32 y = 0; y < g_audNumWorldSectorsY; y++)
			{
				g_AudioWorldSectors[x + (y * g_audNumWorldSectorsX)].numHighwayNodes = 0;
			}
		}

		g_audResetWorldSectorRoadInfo = false;
	}
	if(g_audDrawLocalObjects)
	{
		for(u32 i = 0; i < g_MaxInterestingLocalObjects; i++)
		{
			if(m_InterestingLocalObjects[i].object && GetInterestingLocalObjectMaterial(i))
			{
				const char *matName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(GetInterestingLocalObjectMaterial(i)->NameTableOffset);

				Color32 color = Color32(255,0,0);
				Vector3 pos = VEC3V_TO_VECTOR3(m_InterestingLocalObjects[i].object->GetTransform().GetPosition());
				if(m_InterestingLocalObjects[i].object->GetCurrentPhysicsInst())
				{
					color = Color32(0,255,0);
					pos = VEC3V_TO_VECTOR3(m_InterestingLocalObjects[i].object->GetCurrentPhysicsInst()->GetCenterOfMass());
				}
				CDebugScene::DrawEntityBoundingBox(m_InterestingLocalObjects[i].object,color);
				char buf[128];

				formatf(buf, sizeof(buf), "%s %s %s %s %s", matName, (AUD_GET_TRISTATE_VALUE(GetInterestingLocalObjectMaterial(i)->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_ISRESONANT)==AUD_TRISTATE_TRUE?"reso":""), (GetInterestingLocalObjectMaterial(i)->RandomAmbient != g_NullSoundHash?"rand ambient":""), (GetInterestingLocalObjectMaterial(i)->ShockwaveSound != g_NullSoundHash?"shockwave":""),(GetInterestingLocalObjectMaterial(i)->RainLoop != g_NullSoundHash?"rain":""));
				grcDebugDraw::Text(pos + Vector3(0.f,0.f,0.5f), color, buf);
			}
		}
	}
	if(g_audDrawShockwaves)
	{
		for(u32 i = 0 ; i < kMaxRegisteredShockwaves; i++)
		{
			if(m_RegisteredShockwavesAllocation.IsSet(i))
			{
				grcDebugDraw::Sphere(RCC_VEC3V(m_RegisteredShockwaves[i].centre), 1.f, Color32(0,0,255));
				char buf[128];
				formatf(buf, sizeof(buf), "intensity: %f updated %0.2fs ago", m_RegisteredShockwaves[i].intensity, (fwTimer::GetTimeInMilliseconds() - m_RegisteredShockwaves[i].lastUpdateTime) * 0.001f);
				grcDebugDraw::Text(RCC_VEC3V(m_RegisteredShockwaves[i].centre), Color32(0,0,255), buf);
				if(m_RegisteredShockwaves[i].radius > 0.f)
				{
					grcDebugDraw::Sphere(RCC_VEC3V(m_RegisteredShockwaves[i].centre), m_RegisteredShockwaves[i].radius, Color32(255,0,0), false);
				}
			}
		}
	}
}

void naEnvironment::DrawWeaponEchoes()
{
	if(g_DrawWeaponEchoes)
	{
		Vector3 vListenerPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		Vector3 vBestEchoPlayPos, vOppositeEchoPlayPos;
		u32 bestEchoPredelay = 0;
		u32 oppositeEchoPredelay = 0;
		f32 bestEchoAtten = 0.0f;
		f32 oppositeEchoAtten = 0.0f;
		s32 oppositeEchoIdx;
		s32 bestEchoIdx = GetBestEchoForSourcePosition(vListenerPos, &oppositeEchoIdx);

		if(bestEchoIdx > -1)
		{
			CalculateEchoDelayAndAttenuation(bestEchoIdx, vListenerPos, &bestEchoPredelay, &bestEchoAtten, &vBestEchoPlayPos);
		}
		if(oppositeEchoIdx > -1)
		{
			CalculateEchoDelayAndAttenuation(oppositeEchoIdx, vListenerPos, &oppositeEchoPredelay, &oppositeEchoAtten, &vOppositeEchoPlayPos);
		}

		char buf[255];

		f32 echoSuitablility[8];
		GetEchoSuitablilityForSourcePosition(vListenerPos, &echoSuitablility[0]);
		for(s32 i = 0; i < 8; i++)
		{
			if(echoSuitablility[i] == 0)
				continue;
			f32 distance = (vListenerPos - m_EchoPosition[i]).Mag();
			if(i == bestEchoIdx)
			{
				grcDebugDraw::Sphere(m_EchoPosition[i], 1.0f, Color_green);
				formatf(buf, sizeof(buf), "Best Echo - Suitability: %f distance: %f predelay: %u volume: %f ", 
					echoSuitablility[i], distance, bestEchoPredelay, bestEchoAtten);
				grcDebugDraw::Text(m_EchoPosition[i] + Vector3(0.0f, 0.0f, 1.0f), Color_green, buf);
				grcDebugDraw::Sphere(vBestEchoPlayPos, 1.0f, Color_green);
				formatf(buf, sizeof(buf), "Best Echo Play Position");
				grcDebugDraw::Text(vBestEchoPlayPos + Vector3(0.0f, 0.0f, 1.0f), Color_green, buf);
				Vector3 vNormalArrow = m_EchoDirection[i];
				vNormalArrow.Scale(2.0f);
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(m_EchoPosition[i]), VECTOR3_TO_VEC3V(m_EchoPosition[i] + vNormalArrow), 1.0f, Color_OrangeRed);
			}
			else if(i == oppositeEchoIdx)
			{
				grcDebugDraw::Sphere(m_EchoPosition[i], 1.0f, Color_OliveDrab);
				char buf[255];
				formatf(buf, sizeof(buf), "Opposite Echo - Suitability: %f distance: %f predelay: %u volume: %f", 
					echoSuitablility[i], distance, oppositeEchoPredelay, oppositeEchoAtten);
				grcDebugDraw::Text(m_EchoPosition[i] + Vector3(0.0f, 0.0f, 1.0f), Color_OliveDrab, buf);
				grcDebugDraw::Sphere(vOppositeEchoPlayPos, 1.0f, Color_OliveDrab);
				formatf(buf, sizeof(buf), "Opposite Echo Play Position");
				grcDebugDraw::Text(vOppositeEchoPlayPos + Vector3(0.0f, 0.0f, 1.0f), Color_OliveDrab, buf);
				Vector3 vNormalArrow = m_EchoDirection[i];
				vNormalArrow.Scale(2.0f);
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(m_EchoPosition[i]), VECTOR3_TO_VEC3V(m_EchoPosition[i] + vNormalArrow), 1.0f, Color_OrangeRed);
			}
			else
			{
				grcDebugDraw::Sphere(m_EchoPosition[i], 0.5f, Color_grey);
				char buf[255];
				formatf(buf, sizeof(buf), "Suitability: %f distance: %f", echoSuitablility[i], distance);
				grcDebugDraw::Text(m_EchoPosition[i] + Vector3(0.0f, 0.0f, 0.5f), Color_grey, buf);
				Vector3 vNormalArrow = m_EchoDirection[i];
				vNormalArrow.Scale(2.0f);
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(m_EchoPosition[i]), VECTOR3_TO_VEC3V(m_EchoPosition[i] + vNormalArrow), 1.0f, Color_OrangeRed);
			}
		}

		f32 normalizedReverbWet = ((f32)((m_SourceEnvironmentMetrics[0] & 12) >> 2))/3.0f;
		f32 normalizedReverbSize = ((f32)((m_SourceEnvironmentMetrics[0] & 3)))/3.0f;

		f32 volume;
		f32 hold;

		if(m_SourceEnvironmentMetrics[0] == 0)
		{
			volume = m_OutdoorInteriorWeaponVolCurve.CalculateValue(0.0f);
			hold = m_OutdoorInteriorWeaponHoldCurve.CalculateValue(0.0f);
		}
		else if(normalizedReverbWet == 0)
		{
			volume = m_OutdoorInteriorWeaponVolCurve.CalculateValue(0.15f);
			hold = m_OutdoorInteriorWeaponHoldCurve.CalculateValue(normalizedReverbSize);
		}
		else
		{
			volume = m_OutdoorInteriorWeaponVolCurve.CalculateValue(normalizedReverbWet);
			hold = m_OutdoorInteriorWeaponHoldCurve.CalculateValue(normalizedReverbSize);
		}

		//Draw tight area stats
		f32 yPos = 0.05f;
		f32 yInc = 0.025f;
		f32 xPos = 0.05f;
		formatf(buf, sizeof(buf), "Volume: %f", volume);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_blue1, buf);
		yPos += yInc;
		formatf(buf, sizeof(buf), "Hold: %f", hold);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_blue1, buf);
	}
}

void naEnvironment::DrawAudioWorldSectors()
{
	Vector3 origin,gridControl; 
	origin.SetX((f32)g_audWorldSectorsMinX);
	origin.SetY((f32)g_audWorldSectorsMinY);
	origin.SetZ(70.f);
	gridControl = origin;
	for(u32 i = 0 ; i <= g_audNumWorldSectorsX ; ++i)
	{
		grcDebugDraw::Line(gridControl,gridControl+ Vector3(g_fAudNumWorldSectorsX * g_audWidthOfSector,0.f,0.f), Color_red,1);
		gridControl.SetY(gridControl.GetY() + g_audWidthOfSector);
	}
	gridControl = origin;
	for(u32 i = 0 ; i <= g_audNumWorldSectorsY ; ++i)
	{
		grcDebugDraw::Line(gridControl,gridControl+ Vector3(0.f,g_fAudNumWorldSectorsY * g_audDepthOfSector,0.f), Color_red,1);
		gridControl.SetX(gridControl.GetX() + g_audDepthOfSector);
	}
}
void GenerateAudioSectorsCB()
{
	naEnvironment::DrawAudioWorldSectors(true); 
	naEnvironment::ProcessSectors(true); 
}
void StopCB()
{
	naEnvironment::ProcessSectors(false); 
	naEnvironment::DrawAudioWorldSectors(false); 
}
extern bool g_WarpToInterior;
void naEnvironment::AddWidgets(bkBank &bank)
{
	bank.PushGroup("GTA Environment",false);
	
		const char *specialEffectModes[kNumSpecialEffectModes] = { "Normal", "Underwater", "Stoned", "PauseMenu", "SlowMotion"};
	
		bank.AddCombo("SpecialEffectModeOverride", (s32*)&audNorthAudioEngine::GetGtaEnvironment()->m_SpecialEffectModeOverride, kNumSpecialEffectModes, (const char**)specialEffectModes);
		bank.AddToggle("override", &audNorthAudioEngine::GetGtaEnvironment()->m_OverrideSpecialEffectMode);

		bank.AddToggle("Update built up factor for each zone.", &g_UpdateBuiltUpFactor);
		// GTA-specific, game-thread only things
		bank.PushGroup("Audio world sectors info generator",false);
		bank.AddToggle("Draw audio world sectors ", &sm_DrawAudioWorldSectors);
		bank.AddToggle("Draw AudioWorldSectors info", &g_audDrawWorldSectorDebug);
		bank.AddSlider("sm_NumWaterProbeSets", &sm_NumWaterProbeSets, 0.0f, 997.0f, 1.f);
		bank.AddSlider("sm_CurrentSectorX", &sm_CurrentSectorX, 0, 100, 1);
		bank.AddSlider("sm_CurrentSectorY", &sm_CurrentSectorY, 0, 100, 1);
		bank.AddSlider("g_NumTreesForRain", &g_NumTreesForRain, 0.0f, 997.0f, 1.f);
		bank.AddSlider("g_NumTreesForRainBuiltUp", &g_NumTreesForRainBuiltUp, 0.0f, 997.0f, 1.f);
		bank.AddButton("Generate audio sectors info",datCallback(GenerateAudioSectorsCB));
		bank.AddButton("Stop",datCallback(StopCB));
		bank.PopGroup();
		bank.AddToggle("EnableLocalEnvironmentMetrics", &g_EnableLocalEnvironmentMetrics);

		bank.PushGroup("Interiors");
		
			bank.AddToggle("WarpToInterior", &g_WarpToInterior);
			bank.AddToggle("g_UseDebugRoomSettings", &g_UseDebugRoomSettings);
			bank.AddToggle("g_TryRoomsEveryTime", &g_TryRoomsEveryTime);
			bank.AddSlider("ReverbSmall", &g_DebugRoomSettingsReverbMedium, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("ReverbMedium", &g_DebugRoomSettingsReverbSmall, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("ReverbLarge", &g_DebugRoomSettingsReverbLarge, 0.0f, 1.0f, 0.01f);
			bank.AddToggle("g_UsePlayer", &g_UsePlayer);
			bank.AddText("Interior", &g_InteriorInfoDebug[0], sizeof(g_InteriorInfoDebug));
			bank.AddText("Room", &g_RoomInfoDebug[0], sizeof(g_RoomInfoDebug));
			bank.AddButton("Create RAVE objects for current Interior and Rooms", CFA(CreateRAVEObjectsForCurrentInteriorAndRooms));
			bank.AddButton("Create RAVE object for current Interior Only", CFA(CreateRAVEObjectsForCurrentInterior));
			bank.AddButton("Create RAVE object for current Room Only", CFA(CreateRAVEObjectsForCurrentRoom));
		bank.PopGroup();

		bank.AddToggle("g_DrawWeaponEchos", &g_DrawWeaponEchoes);

		bank.AddSlider("OpenSpaceReverbSize", &g_OpenSpaceReverbSize, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("OpenSpaceReverbWet", &g_OpenSpaceReverbWet, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("SpeakerReverbSmootherRate", &g_SpeakerReverbSmootherRate, 0.0f, 1.0f, 0.0001f);
		bank.AddSlider("SpeakerReverdDamping", &g_SpeakerReverbDamping, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("SourceEnvironmentRadius", &g_SourceEnvironmentRadius, 0.0f, 20.0f, 0.1f);

		bank.AddSlider("TheManhattanFactor", &g_OverrideBuiltUpFactor, 0.0f, 1.0f, 0.01f);
		bank.AddToggle("OverrideManhattanFactor", &g_ShouldOverrideBuiltUpFactor);
		bank.AddSlider("Special Ability Slow-mo timeout", &g_SpecialAbilitySlowMoTimeout, 0, 10000, 10);

		bank.AddSlider("Deafening Up Time", &g_DeafeningRampUpTime, 0, 10000, 1);
		bank.AddSlider("Deafening Hold Time", &g_DeafeningHoldTime, 0, 10000, 1);
		bank.AddSlider("Deafening Down Time", &g_DeafeningRampDownTime, 0, 10000, 1);
		bank.AddSlider("Deafening Ear Leak", &g_DeafeningEarLeak, 0.0f, 1.0f, 0.1f);
		bank.AddSlider("g_DeafeningRolloff", &g_DeafeningRolloff, 0.0f, 10.0f, 0.1f);
		bank.PushGroup("Interesting local objects");
			bank.PushGroup("ShockWaves");
				bank.AddToggle("g_audDrawShockwaves", &g_audDrawShockwaves);
				bank.AddSlider("Heli shockwave radius", &sm_HeliShockwaveRadius, 0.0f, 1000.0f, 0.01f);
				bank.AddSlider("Train distance threshold", &sm_TrainDistanceThreshold, 0.0f, 1000.0f, 0.01f);
				bank.AddSlider("Train shockwave radius", &sm_TrainShockwaveRadius, 0.0f, 1000.0f, 0.01f);
			bank.PopGroup();
			bank.PushGroup("Resonance");
				bank.AddToggle("Force Resonance Tracks Listener", &g_ForceResonanceTracksListener);
				bank.AddToggle("Display ResoIntensity", &sm_DisplayResoIntensity);
				bank.AddSlider("Melee resonance impulse", &sm_MeleeResoImpulse, 0.0f, 1.f, 0.001f);
				bank.AddSlider("Pistol resonance impulse", &sm_PistolResoImpulse, 0.0f, 1.f, 0.001f);
				bank.AddSlider("Rifle resonance impulse", &sm_RifleResoImpulse, 0.0f, 1.f, 0.001f);
				bank.AddSlider("Weapons resonance distance", &sm_WeaponsResonanceDistance, 0.0f, 1000.0f, 0.01f);
				bank.AddSlider("Explosions resonance impulse", &sm_ExplosionsResonanceImpulse, 0.0f, 1.f, 0.001f);
				bank.AddSlider("Explosions resonance distance", &sm_ExplosionsResonanceDistance, 0.0f, 1000.f, 0.001f);
				bank.AddSlider("Vehicle collision resonance impulse", &sm_VehicleCollisionResonanceImpulse, 0.0f, 1.f, 0.001f);
				bank.AddSlider("Vehicle collision distance", &sm_VehicleCollisionResonanceDistance, 0.0f, 1000.f, 0.001f);
				bank.AddSlider("Plateau effect", &sm_DistancePlateau, 0.0f, 1000.0f, 0.01f);
				bank.PushGroup("Slow motion", false);
				bank.AddSlider("Frequency", &sm_SlowMoLFOFrequencyMin, 0.0f, 1000.0f, 0.01f);
				bank.AddSlider("Frequency", &sm_SlowMoLFOFrequencyMax, 0.0f, 1000.0f, 0.01f);
				bank.PopGroup();
			bank.PopGroup();
			bank.PushGroup("Rain");
				bank.AddToggle("Force Rain Tracks Listener", &g_ForceRainLoopTracksListener);
				bank.AddToggle("Force Rain Plays From Top Of Bounds", &g_ForceRainLoopPlayFromTopOfBounds);
				bank.AddSlider("sm_SqdDistanceToPlayRainSounds", &sm_SqdDistanceToPlayRainSounds, 0.0f, 10000.0f, 1.f);
				bank.AddSlider("sm_RainOnPropsVolumeOffset", &sm_RainOnPropsVolumeOffset, -100.0f, 24.f, 1.f);
				bank.AddSlider("sm_RainOnPopsPreDealy", &sm_RainOnPopsPreDealy, 0, 10000, 100);
				bank.AddSlider("sm_MaxNumRainProps", &sm_MaxNumRainProps, 0, 64, 1);
			bank.PopGroup();
			bank.AddToggle("g_audDrawLocalObjects",&g_audDrawLocalObjects);
			bank.AddToggle("Use aproximation to compute which objects are covered", &sm_UseCoverAproximation);
			bank.AddToggle("Debug covered objects", &sm_DebugCoveredObjects);
			bank.AddSlider("sm_RadiusScale", &sm_RadiusScale, 0.0f, 10.0f, 0.0001f);
			bank.AddSlider("sm_ObjOcclusionUpdateTime", &sm_ObjOcclusionUpdateTime, 0, 10000, 100);
		bank.PopGroup();
		bank.PushGroup("Wind");
			bank.PushGroup("Through trees");
				bank.PushGroup("Background",false);
					bank.AddToggle("sm_DrawBgTreesInfo", &sm_DrawBgTreesInfo);
					bank.AddSlider("sm_NumTreesForAvgDist", &sm_NumTreesForAvgDist, 0, 50, 1);
					bank.AddSlider("g_FitnessReduced", &g_FitnessReduced, 0.0f, 1.0f, 0.01f);
					bank.AddSlider("g_VolSmootherRate", &g_VolSmootherRate, 0.0f, 2.0f, 0.01f);
					bank.AddSlider("Bushes min radius", &sm_BushesMinRadius, 0.0f, 1000.0f, 0.01f);
					bank.AddSlider("Trees min radius", &sm_SmallTreesMinRadius, 0.0f, 1000.0f, 0.01f);
					bank.AddSlider("Big trees min radius", &sm_BigTreesMinRadius, 0.0f, 1000.0f, 0.01f);
					bank.AddSlider("sm_BigTreesFitness", &sm_BigTreesFitness, 0.0f, 1.0f, 0.01f);
					bank.AddSlider("sm_TreesFitness", &sm_TreesFitness, 0.0f, 1.0f, 0.01f);
					bank.AddSlider("sm_BushesFitness", &sm_BushesFitness, 0.0f, 1.0f, 0.01f);
					bank.AddSlider("Sounds distance from the listener", &sm_TreesSoundDist, 0.0f, 1000.0f, 0.01f);
					//bank.AddSlider("sm_GustVolSpikeForBgTrees", &sm_GustVolSpikeForBgTrees, 0.0f, 24.0f, 0.5f);
					bank.AddToggle("sm_MuteBgFL", &sm_MuteBgFL);
					bank.AddToggle("sm_MuteBgFR", &sm_MuteBgFR);
					bank.AddToggle("sm_MuteBgRL", &sm_MuteBgRL);
					bank.AddToggle("sm_MuteBgRR", &sm_MuteBgRR);
					bank.PushGroup("GustEnd sound",false);
					bank.AddSlider("sm_MinNumberOfTreesToTriggerGustEnd", &sm_MinNumberOfTreesToTriggerGustEnd, 0, 1000, 1);
					bank.PopGroup();
				bank.PopGroup();
			bank.PopGroup();
			bank.PushGroup("On props");
			bank.AddToggle("Show Info", &sm_DebugGustWhistle);
			bank.AddSlider("sm_MinTimeToRetrigger", &sm_MinTimeToRetrigger, 0,50000,100);
			bank.PopGroup();
		bank.PopGroup();
		bank.AddToggle("Reset AudioWorldSector Road Info", &g_audResetWorldSectorRoadInfo);
		bank.AddToggle("Debug Listener Reverb", &g_DebugListenerReverb);

		bank.AddSlider("g_BuiltUpSmootherRate", &g_BuiltUpSmootherRate, 0.0f, 1.0f, 0.0001f);
		bank.AddToggle("Debug BuiltUp Info", &g_DebugBuiltUpInfo);
		bank.AddToggle("g_DisplayBuiltUpFactors", &g_DisplayBuiltUpFactors);
		bank.AddToggle("g_UseProperBuiltUpFactor", &g_UseProperBuiltUpFactor);
		bank.AddSlider("g_NSEW_Offset", &g_NSEW_Offset, 0.0f, 2.49f, 0.01f);
		bank.AddSlider("g_BuiltUpDampingFactorSmoothUpRate", &g_BuiltUpDampingFactorSmoothUpRate, 0.0f, 10000.0f, 1.0f);
		bank.AddSlider("g_BuiltUpDampingFactorSmoothDownRate", &g_BuiltUpDampingFactorSmoothDownRate, 0.0f, 10000.0f, 1.0f);
		bank.AddSlider("g_BuiltUpDampingFactorOverride", &g_BuiltUpDampingFactorOverride, 0.0f, 1.0f, 0.1f);

		bank.AddSlider("g_InCarRatioSmoothRate", &g_InCarRatioSmoothRate, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("g_InteriorRatioSmoothRate", &g_InteriorRatioSmoothRate, 0.0f, 100.0f, 0.1f);
		
	bank.PopGroup();
}
#endif
