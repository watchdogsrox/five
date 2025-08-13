/////////////////////////////////////////////////////////////////////////////////
// FILE :		vehiclepopulation.cpp
// PURPOSE :	Deals with creation and removal of vehicles in the world.
//				It manages the vehicle population by removing what vehicles we
//				can and adding what vehicles we should.
//				It creates ambient vehicles and also vehicles at attractors when
//				appropriate.
// AUTHOR :		Obbe.
//				Adam Croston.
// CREATED :	10/05/06
/////////////////////////////////////////////////////////////////////////////////
#include "vehicles/vehiclepopulation.h" // matching header
 
// rage includes
#include "math/angmath.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "parser/manager.h"
#include "profile/timebars.h"
#include "system/param.h"
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/world/WorldLimits.h"
#include "fragment/cacheheap.h"

// game includes
#include "ai/stats.h"
#include "animation/debug/AnimViewer.h"
#include "audio/boataudioentity.h"
#include "audio/caraudioentity.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "control/trafficlights.h"
#include "core/game.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "vehicles/Metadata/VehicleLayoutInfo.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/driverpersonality.h"
#include "vehicleAi/task/TaskVehicleGoTo.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehiclePark.h"
#include "control/garages.h"

#include "control/replay/replay.h"
#include "control/trafficlights.h"
#include "control/SlownessZones.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "debug/vectormap.h"
#include "game/Clock.h"
#include "game/Dispatch/DispatchManager.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Dispatch/OrderManager.h"
#include "game/modelindices.h"
#include "game/PopMultiplierAreas.h"
#include "Network/Live/NetworkTelemetry.h"
#include "game/zones.h"
#include "game/weather.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "network/NetworkInterface.h"
#include "network/players/NetGamePlayer.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "pedgroup/formation.h"
#include "pedgroup/pedgroup.h"
#include "peds/pedfactory.h"
#include "peds/pedintelligence.h"
#include "peds/pedpopulation.h"
#include "peds/ped.h"
#include "peds/popcycle.h"
#include "peds/popzones.h"
#include "peds/VehicleCombatAvoidanceArea.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "profile/cputrace.h"
#include "renderer/occlusion.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "renderer/water.h"
#include "renderer/zonecull.h"
#include "script/Handlers/GameScriptEntity.h"
#include "peds/PlayerInfo.h"
#include "profile/profiler.h"
#include "scene/lod/LodScale.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/portals/portal.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "script/script_hud.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "fwsys/timer.h"
#include "task/default/taskwander.h"
#include "task/general/taskbasic.h"
#include "task/movement/tasknavmesh.h"
#include "Task/Scenario/ScenarioChaining.h"
#include "task/scenario/scenariomanager.h"
#include "task/scenario/ScenarioPointManager.h"
#include "task/scenario/taskscenario.h"
#include "task/vehicle/taskcar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "vehicles/Metadata/VehicleLayoutInfo.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/train.h"
#include "vehicles/bike.h"
#include "vehicles/bmx.h"
#include "vehicles/boat.h"
#include "vehicles/buses.h"
#include "vehicles/cargen.h"
#include "vehicles/DraftVehicle.h"
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/submarine.h"
#include "vehicles/train.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/vehpopulation_channel.h"
#include "vehicles/VehiclePopulationAsync.h"
#include "vehicles/Trailer.h"
#include "vfx/misc/fire.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"

#if __DEV
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/NetworkObjectTypes.h"
#endif // __DEV

#if __BANK
#include "Network/Debug/NetworkDebug.h"
#endif // __BANK


#if __STATS
PF_PAGE(WorldPopulationPage, "World Population");

PF_GROUP(Population);
PF_LINK(WorldPopulationPage, Population);
PF_COUNTER(TotalEntitiesCreatedByPopulation, Population);
PF_COUNTER(TotalEntitiesDestroyedByPopulation, Population);

PF_GROUP(VehiclePopulation);
PF_LINK(WorldPopulationPage, VehiclePopulation);
PF_TIMER(VehiclePopulationProcess, VehiclePopulation);
PF_TIMER(ProcessCarGenerators, VehiclePopulation);
PF_COUNTER(AmbientVehiclesCreated, VehiclePopulation);
PF_COUNTER(CargenVehiclesCreated, VehiclePopulation);
PF_COUNTER(VehiclesCreatedByNetwork, VehiclePopulation);
PF_COUNTER(TotalVehiclesCreated, VehiclePopulation);
PF_COUNTER(EntitiesCreatedByVehiclePopulation, VehiclePopulation);
PF_COUNTER(EntitiesDestroyedByVehiclePopulation, VehiclePopulation);
PF_VALUE_FLOAT(AmbientVehiclePopulationRatio, VehiclePopulation);
PF_COUNTER(NormalAmbientVehicleFailures, VehiclePopulation);
PF_COUNTER(VehiclesReused, VehiclePopulation);
PF_COUNTER(VehiclesInReusePool, VehiclePopulation);
#endif // __STATS

// Parser files
#include "metadata/VehiclePopulationTuning_parser.h"

VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
RAGE_DEFINE_CHANNEL(vehicle_population)
RAGE_DEFINE_CHANNEL(vehicle_reuse)

using namespace AIStats;

bool gbGenerateAmbulance = false;
bool gbGenerateFiretruck = false;
bool gbGenerateGarbagetruck = false;

//static dev_bool g_bRandomizeScoreSlightly = false;

PARAM(nocarsexceptcops, "[vehiclepopulation] Don't create any random vehs except cops");
PARAM(nocars, "[vehiclepopulation] Don't create any random vehs");
PARAM(novehs, "[vehiclepopulation] Don't create any random vehs");
PARAM(novehsexcepttrains, "[vehiclepopulation] Don't create any random vehs except trains");
PARAM(maxVehicles, "[vehiclepopulation] Maximum vehicle population");
PARAM(minVehicles, "[vehiclepopulation] Minimum vehicle population");
PARAM(vehicleoverride, "[vehiclepopulation] Override all vehicle archetypes with the one specified");
PARAM(vehiclesUpperLimit, "[vehiclepopulation] Set the upper limit used by vehicle generation algorithm");
PARAM(vehiclesUpperLimitMP, "[vehiclepopulation] Set the MP upper limit used by vehicle generation algorithm");
PARAM(parkedVehiclesUpperLimit, "[vehiclepopulation] Set the upper limit used by parked vehicle generation algorithm");
PARAM(vehiclesMemoryMultiplier, "[vehiclepopulation] The scale factor to multiple the vehicle memory pool by");
PARAM(vehiclesAmbientDensityMultiplier, "[vehiclepopulation] Set the ambient cars spawning density");
PARAM(vehiclesParkedDensityMultiplier, "[vehiclepopulation] Set the parked cars spawning density");
PARAM(vehiclesLowPrioParkedDensityMultiplier, "[vehiclepopulation] Set the low priority parked cars spawning density");

PARAM(noReuseInMP, "[worldpopulation] Disable Ped/Vehicle Reuse in MP");
PARAM(extraReuse, "[worldpopulation] Skip HasBeenSeen Checks for reusing peds and vehicles");

PARAM(catchvehiclesremovedinview, "[vehiclepopulation] Catch Vehicles Removed In View");
PARAM(logcreatedvehicles, "[vehiclepopulation] Log created vehicles");
PARAM(logdestroyedvehicles, "[vehiclepopulation] Log destroyed vehicles");

#if __ASSERT
PARAM(noFatalAssertOnVehicleScopeDistance, "[vehiclepopulation] Disable the fatal assert on netobjvehicle scope for debug purposes");
#endif // __ASSERT

float CVehiclePopulation::ms_allZoneScaler								= 1.0f;
float CVehiclePopulation::ms_scriptedRangeMultiplier					= 1.0f;
float CVehiclePopulation::ms_scriptedRangeMultiplierThisFrame			= 1.0f;
bool  CVehiclePopulation::ms_scriptedRangeMultiplierThisFrameModified   = false;
float CVehiclePopulation::ms_rangeScaleShallowInterior					= 0.75f;
float CVehiclePopulation::ms_rangeScaleDeepInterior						= 0.5f;
float CVehiclePopulation::ms_creationDistance							= 225.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_creationDistanceOffScreen					= 50.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_creationDistMultExtendedRange				= 1.5f;		// Should be bigger than DIST_MULT_FOR_WATER_NODES
bank_float CVehiclePopulation::ms_spawnFrustumNearDistMultiplier		= 0.5f;
float CVehiclePopulation::ms_NetworkVisibleCreationDistMax				= 210.0f;	// visible rejection range for clones max.
float CVehiclePopulation::ms_NetworkVisibleCreationDistMin				= 180.0f;	// visible rejection range for clones min.
float CVehiclePopulation::ms_NetworkOffscreenCreationDistMax			= 60.0f;	// non visible rejection range for clones.
float CVehiclePopulation::ms_NetworkOffscreenCreationDistMin			= 60.0f;	// non visible rejection range for clones.
float CVehiclePopulation::ms_FallbackNetworkVisibleCreationDistMax		= 180.0f;	// fallback visible rejection distance max.
float CVehiclePopulation::ms_FallbackNetworkVisibleCreationDistMin		= 100.0f;	// fallback visible rejection distance min.
float CVehiclePopulation::ms_NetworkCreationDistMinOverride				= 45.0f;
bool CVehiclePopulation::ms_NetworkUsePlayerPrediction					= false;
bool CVehiclePopulation::ms_NetworkShowTemporaryRealVehicles			= false;
float CVehiclePopulation::ms_NetworkPlayerPredictionTime				= 1.0f;
u32 CVehiclePopulation::ms_NetworkMaxTimeVisibilityFail					= 1000 * 5; // 5 seconds
u32 CVehiclePopulation::ms_NetworkCloneRejectionCooldownDelay			= 250;
u32 CVehiclePopulation::ms_NetworkTimeVisibilityFail					= 0;
u32 CVehiclePopulation::ms_NetworkTimeExtremeVisibilityFail				= 0;
u32 CVehiclePopulation::ms_NetworkVisibilityFailCount					= 0;
u32 CVehiclePopulation::ms_NetworkMinVehiclesToConsiderOverrideMin		= 20;
u32 CVehiclePopulation::ms_NetworkMinVehiclesToConsiderOverrideMax		= 25;
// This needs to be greater than the outer spawn band radius + thickness
float CVehiclePopulation::ms_cullRange									= 235.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_cullRangeOnScreenScale						= 1.5f;
// This needs to be greater than the inner spawn band radius + thickness
float CVehiclePopulation::ms_cullRangeOffScreen							= 100.0f; // Gameconfig.xml
float CVehiclePopulation::ms_cullRangeOffScreenPedsRemoved				= 100.0f;
float CVehiclePopulation::ms_cullRangeBehindPlayerWhenZoomed			= 20.0f;
float CVehiclePopulation::ms_cullRangeOffScreenWrecked					= 10.0f;
float CVehiclePopulation::ms_maxSpeedBasedRemovalRateScale				= 22.0f;
float CVehiclePopulation::ms_maxSpeedExpectedMax						= 50.0f;
float CVehiclePopulation::ms_densityBasedRemovalRateScale				= 36.0f;
u32 CVehiclePopulation::ms_densityBasedRemovalTargetHeadroom			= 10;
s32	CVehiclePopulation::ms_vehicleRemovalDelayMsRefreshMin				= 2250; //3500;		// Cars right next to the player can only be removed if they have been off-screen for the following length of time
s32	CVehiclePopulation::ms_vehicleRemovalDelayMsRefreshMax				= 2750; //5500;		// Cars right next to the player can only be removed if they have been off-screen for the following length of time
s32	CVehiclePopulation::ms_maxNumberOfCarsInUse							= 0;		// We only ever want as many cars as this (can be multiplied by CIniFile value). Used to be VEHICLE_POOL_SIZE, but with the new configuration file, we don't know the value of that yet.
float CVehiclePopulation::ms_maxGenerationHeightDiff					= 13.0f;
bool CVehiclePopulation::ms_allowSpawningFromDeadEndRoads				= true;
s32	CVehiclePopulation::ms_maxNumMissionVehs							= 128;
s32	CVehiclePopulation::ms_maxRandomVehs								= 180;
bool CVehiclePopulation::ms_noVehSpawn									= false;
u32	CVehiclePopulation::ms_countDownToCarsAtStart						= 0;
float CVehiclePopulation::ms_randomVehDensityMultiplier					= 1.0f; // Gameconfig.xml now
float CVehiclePopulation::ms_parkedCarDensityMultiplier					= 1.0f; // Gameconfig.xml now
float CVehiclePopulation::ms_lowPrioParkedCarDensityMultiplier			= 1.0f; // Gameconfig.xml now
float CVehiclePopulation::ms_debugOverrideVehDensityMultiplier			= 1.0f; // override, because scripters sometimes dick about with the actual multiplier in-game
float CVehiclePopulation::ms_scriptRandomVehDensityMultiplierThisFrame	= 1.0f;
float CVehiclePopulation::ms_scriptParkedCarDensityMultiplierThisFrame	= 1.0f;
float CVehiclePopulation::ms_fRendererRangeMultiplierOverrideThisFrame	= 0.0f;

bool CVehiclePopulation::ms_scriptDisableRandomTrains					= false;
bool CVehiclePopulation::ms_scriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame = false;
UInt32 CVehiclePopulation::ms_scriptCapEmptyCrowdingPlayersInNetGamesThisFrame = 1000;
float CVehiclePopulation::ms_vehicleMemoryBudgetMultiplier				= 1.0f;
s32 CVehiclePopulation::ms_desiredNumberAmbientVehicles					= 0;
float CVehiclePopulation::ms_CloneSpawnValidationMultiplier				= 0.0f;
float CVehiclePopulation::ms_CloneSpawnSuccessCooldownTimerScale = 0.2f;
float CVehiclePopulation::ms_CloneSpawnMultiplierFailScore = 0.2f;
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
float CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG = 1.0f;
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
bank_bool CVehiclePopulation::ms_bGenerateCarsWaitingAtLights			= true; //false;
bank_bool CVehiclePopulation::ms_bGenerateCarsWaitingAtLightsOnlyAtRed	= false;
bank_s32 CVehiclePopulation::ms_iMaxCarsForLights						= 4;
s32 CVehiclePopulation::ms_numRandomCars								= 0;
s32 CVehiclePopulation::ms_numCarsInReusePool							= 0;
s32 CVehiclePopulation::ms_numStationaryAmbientCars						= 0;
s32 CVehiclePopulation::ms_numAmbientCarsOnHighway						= 0;
s32 CVehiclePopulation::ms_previousGenLinkDisparity[VEHGEN_NUM_AVERAGE_DISPARITY_FRAMES];
float CVehiclePopulation::ms_averageGenLinkDisparity						= 0.0f;
s32 CVehiclePopulation::ms_genLinkDisparityIndex						= 0;
s32 CVehiclePopulation::ms_numMissionCars								= 0;
s32 CVehiclePopulation::ms_numParkedCars								= 0;
s32 CVehiclePopulation::ms_numLowPrioParkedCars							= 0;
s32 CVehiclePopulation::ms_numParkedMissionCars							= 0;
s32 CVehiclePopulation::ms_numPermanentVehicles							= 0;
u32 CVehiclePopulation::ms_lastTimeLawEnforcerCreated					= 0;
s32 CVehiclePopulation::ms_overrideNumberOfParkedCars					= -1;

bool CVehiclePopulation::ms_bVehIsBeingCreated							= false;
bool CVehiclePopulation::ms_bUseExistingCarNodes						= false;
bool CVehiclePopulation::ms_bMadDrivers									= true;
float CVehiclePopulation::ms_passengerInASeatProbability				= 0.13f;

float CVehiclePopulation::ms_popGenKeyholeInnerShapeThickness			= 50.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_popGenKeyholeOuterShapeThickness			= 60.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_popGenKeyholeShapeInnerBandRadius			= 50.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_popGenKeyholeShapeOuterBandRadius			= 165.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_popGenKeyholeSideWallThickness				= 60.0f;	// Gameconfig.xml
float CVehiclePopulation::ms_popGenKeyholeShapeInnerBandRadiusHighPriority = 30.0f;
float CVehiclePopulation::ms_popGenKeyholeShapeOuterBandRadiusHighPriority = 50.0f;

float CVehiclePopulation::ms_popGenDensitySamplingVerticalFalloffStart	= 4.0f;
float CVehiclePopulation::ms_popGenDensitySamplingVerticalFalloffEnd	= 10.0f;
float CVehiclePopulation::ms_popGenDensitySamplingMinSummedDesiredNumVehs	= 1.0f;
float CVehiclePopulation::ms_popGenDensitySamplingMaxExtendedDist			= 100.0f;
float CVehiclePopulation::ms_popGenStandardVehsPerMeterPerLane			= 0.1f;
float CVehiclePopulation::ms_popGenRandomizeLinkDensity					= 2.0f;
float CVehiclePopulation::ms_popGenOuterBandLinkScoreBoost				= 0.0f; //4.0f;
u32 CVehiclePopulation::ms_lastGenLinkAttemptCountUpdateMs				= 0;
u32 CVehiclePopulation::ms_GenerateVehCreationPosFromPathsUpdateTime	= 0;

bank_bool CVehiclePopulation::ms_bEnableUseGatherLinksKeyhole			= true;
bank_float CVehiclePopulation::ms_fZoomThicknessScale					= 1.0f;
bank_float CVehiclePopulation::ms_fMaxRangeScale						= 4.0f;
bank_float CVehiclePopulation::ms_fGatherLinksUseKeyHoleShapeRangeThreshold = 2.5f;

float CVehiclePopulation::ms_fAircraftSpawnRangeMult					= 1.5f;
float CVehiclePopulation::ms_fAircraftSpawnRangeMultMP					= 1.3f;
bank_bool CVehiclePopulation::ms_bIncreaseSpawnRangeBasedOnHeight		= true;
bank_bool CVehiclePopulation::ms_bZoneBasedHeightRange					= true;
bank_float CVehiclePopulation::ms_fHeightBandStartZ						= 0.0f;
bank_float CVehiclePopulation::ms_fHeightBandEndZ						= 66.667f;
bank_float CVehiclePopulation::ms_fHeightBandMultiplier					= 2.2f;

s32 CVehiclePopulation::ms_iAmbientVehiclesUpperLimit					= 60; // Gameconfig.xml now
s32 CVehiclePopulation::ms_iAmbientVehiclesUpperLimitMP					= 60; // Gameconfig.xml now
s32 CVehiclePopulation::ms_iParkedVehiclesUpperLimit					= 80; // Gameconfig.xml now

float CVehiclePopulation::ms_fLocalAmbientVehicleBufferMult				= 1.0f;
float CVehiclePopulation::ms_fMaxLocalParkedCarsMult					= 1.0f;
bool CVehiclePopulation::ms_bNetworkPopCheckParkedCarsToggle			= true;
bool CVehiclePopulation::ms_bNetworkPopMaxIfCanUseLocalBufferToggle		= true; 

bool CVehiclePopulation::ms_useTempPopControlPointThisFrame				= false;
CPopCtrl CVehiclePopulation::ms_tmpPopOrigin;
CPopCtrl CVehiclePopulation::ms_lastPopCenter;
s8 CVehiclePopulation::ms_bInstantFillPopulation						= 0;
bank_float CVehiclePopulation::ms_fInstantFillExtraPopulationCycles		= 2.0f;
float CVehiclePopulation::ms_TempPopControlSphereRadius					= 0.0f;

u32 CVehiclePopulation::ms_lastPopCycleChangeTime						= 0;
u32 CVehiclePopulation::ms_popCycleChangeBufferTime						= 15000;
u32 CVehiclePopulation::ms_lastChosenVehicleModel						= (u32)-1;
u32 CVehiclePopulation::ms_sameVehicleSpawnLimit						= 3;
u32 CVehiclePopulation::ms_sameVehicleSpawnCount						= 0;
u32 CVehiclePopulation::ms_lastSpawnedNeonIdx							= 0;
RegdVeh CVehiclePopulation::ms_pLastSpawnedNeonVeh;
bool CVehiclePopulation::ms_ambientVehicleNeonsEnabled					= true;
float CVehiclePopulation::ms_spawnLastChosenVehicleChance				= 1.f;
Vec3V CVehiclePopulation::ms_stationaryVehicleCenter					= Vec3V(V_ZERO);

#if __BANK
float CVehiclePopulation::ms_popGenDensityMaxExpectedVehsNearLink	= 4.0f;
bool CVehiclePopulation::ms_bDisplayAbandonedRemovalDebug			= false;
CVehiclePopulation::CMarkUpEditor CVehiclePopulation::ms_MarkUpEditor;
#endif // __DEV


CPopGenShape CVehiclePopulation::ms_popGenKeyholeShape;
bool CVehiclePopulation::ms_popGenKeyholeShapeInitialized;

static u32 s_numGenerationLinks[4] ;

u32 &CVehiclePopulation::ms_numGenerationLinks							= s_numGenerationLinks[0];
u32	CVehiclePopulation::ms_numActiveVehGenLinks							= 0;
float CVehiclePopulation::ms_averageLinkHeight							= 0.0f;
s32	CVehiclePopulation::ms_iActiveLinkUpdateFreqMs						= 1000;
u32	CVehiclePopulation::ms_iNumMarkUpSpheres							= 0;

atArray<CVehiclePopulation::CAmbientPopulationVehData> CVehiclePopulation::ms_aAmbientVehicleData;

CVehiclePopulation::CVehGenLink	CVehiclePopulation::ms_aGenerationLinks[MAX_GENERATION_LINKS];	// ALIGNED(128);

CVehGenMarkUpSpheres CVehiclePopulation::ms_VehGenMarkUpSpheres;

s32 CVehiclePopulation::ms_iNumActiveLinks = 0;
s32 CVehiclePopulation::ms_iNumActiveLinksUsingLowerScore = 0;
s32 CVehiclePopulation::ms_aActiveLinks[MAX_GENERATION_LINKS]; // ALIGNED(128)
CNodeAddress CVehiclePopulation::ms_aNodesOnPlayersRoad[MAX_PLAYERS_ROAD_NODES];
RegdVeh CVehiclePopulation::ms_PlayersJunctionNodeVehicle;
CNodeAddress CVehiclePopulation::ms_PlayersJunctionNode;
CNodeAddress CVehiclePopulation::ms_PlayersJunctionEntranceNode;
s32 CVehiclePopulation::ms_iNumNodesOnPlayersRoad = 0;
float CVehiclePopulation::ms_fPlayersRoadChangeInDirectionPenalties[NUM_PLAYERS_ROAD_CID_PENALTIES][NUM_PLAYERS_ROAD_CID_VALUES];
s32	CVehiclePopulation::ms_iPlayerRoadDensityInc[16];	// gameconfig.xml
s32	CVehiclePopulation::ms_iNonPlayerRoadDensityDec[16];	// gameconfig.xml

bank_bool CVehiclePopulation::ms_bSearchLinksMustMatchVehHeading		= true;
bank_float CVehiclePopulation::ms_fLinkAttemptPenaltyMutliplier			= 6.0f; //4.0f;	/// 3.0f
bank_bool CVehiclePopulation::ms_spawnDoRelativeMovementCheck			= false; // true; // JB: off again, for now
bank_bool CVehiclePopulation::ms_spawnCareAboutLanes					= true;
bank_bool CVehiclePopulation::ms_spawnCareAboutRepeatingSameModel		= true;
bank_bool CVehiclePopulation::ms_spawnEnableSingleTrackRejection		= true;
bank_bool CVehiclePopulation::ms_spawnCareAboutDeadEnds					= true;
bank_bool CVehiclePopulation::ms_spawnCareAboutPlacingOnRoadProperly	= true;
bank_bool CVehiclePopulation::ms_spawnCareAboutBoxCollisionTest			= true;
bank_bool CVehiclePopulation::ms_spawnCareAboutAppropriateVehicleModels = true;
bank_bool CVehiclePopulation::ms_spawnCareAboutFrustumCheck				= true;
bank_float CVehiclePopulation::ms_fSkipFrustumCheckHeight				= 180.0f;

bank_float CVehiclePopulation::ms_desertedStreetsMinimumMultiplier		= 0.25f;
float CVehiclePopulation::ms_desertedStreetsMultiplier					= 1.0f;
bool CVehiclePopulation::ms_bAllowRandomBoats							= false;
int	CVehiclePopulation::ms_iNumVehiclesInRangeForAggressiveRemovalCarjackMission = 4;
float CVehiclePopulation::ms_iRadiusForAggressiveRemoalCarjackMission	= 10.0f;
bool CVehiclePopulation::ms_bAllowRandomBoatsMP							= false;
bool CVehiclePopulation::ms_bDefaultValueAllowRandomBoats				= true;
bool CVehiclePopulation::ms_bDefaultValueAllowRandomBoatsMP				= true;
bool CVehiclePopulation::ms_bAllowGarbageTrucks							= true;
bool CVehiclePopulation::ms_bDesertedStreetMultiplierLocked				= false;

RegdVeh CVehiclePopulation::ms_pInterestingVehicle						(NULL);
u32 CVehiclePopulation::ms_timeToClearInterestingVehicle				= 0;
u32 CVehiclePopulation::ms_clearInterestingVehicleDelaySP				= 1000 * 60 * 10; // 10 minutes
u32 CVehiclePopulation::ms_clearInterestingVehicleDelayMP				= 1000 * 60 * 10; // 10 minutes
float CVehiclePopulation::ms_clearInterestingVehicleProximitySP			= 100.f;
float CVehiclePopulation::ms_clearInterestingVehicleProximityMP			= 100.f;

bool CVehiclePopulation::ms_extendDelayWithinOuterRadius				= true;
u32	CVehiclePopulation::ms_extendDelayWithinOuterRadiusTime				= 3000;
float CVehiclePopulation::ms_extendDelayWithinOuterRadiusExtraDist		= -60.0f;

float CVehiclePopulation::ms_fPlayersRoadScanDistance					= 300.0f;	// gameconfig.xml
s32 CVehiclePopulation::ms_iCurrentActiveLink							= VEHGEN_NUM_PRESELECTED_LINKS;

#if __BANK
bool CVehiclePopulation::ms_displayVehicleDirAndPath					= false;
bool CVehiclePopulation::ms_displayAddCullZonesOnVectorMap				= false;
bool CVehiclePopulation::ms_displayInnerKeyholeDisparities				= false;
u32 CVehiclePopulation::ms_innerKeyholeDisparities						= 0;
s32 CVehiclePopulation::ms_iNumExcessDisparateLinks						= 0;

Vector3 CVehiclePopulation::ms_vClearAreaCenterPos						= Vector3(Vector3::ZeroType);
float CVehiclePopulation::ms_fClearAreaRadius							= 50.f;
bool CVehiclePopulation::ms_bClearAreaCheckInteresting					= false;
bool CVehiclePopulation::ms_bClearAreaLeaveCarGenCars					= true;
bool CVehiclePopulation::ms_bClearAreaCheckFrustum						= false;
bool CVehiclePopulation::ms_bClearAreaWrecked							= false;
bool CVehiclePopulation::ms_bClearAreaAbandoned							= false;
bool CVehiclePopulation::ms_bClearAreaEngineOnFire						= false;
bool CVehiclePopulation::ms_bClearAreaAllowCopRemovealWithWantedLevel	= true;
#endif // __BANK

bool CVehiclePopulation::ms_bUsePlayersRoadDensityModifier				= true;
float CVehiclePopulation::ms_fVehiclePerMeterScaleForNonPlayersRoad		= 1.0f;
float CVehiclePopulation::ms_fOnPlayersRoadPrioritizationThreshold		= 0.5f;

bool CVehiclePopulation::ms_bUseTrafficDensityModifier					= true;
bool CVehiclePopulation::ms_bUseTrafficJamManagement					= true;
u32	CVehiclePopulation::ms_iMinAmbientVehiclesForThrottle				= 50;
float	CVehiclePopulation::ms_fMinRatioForTrafficThrottle				= 0.4f;
float	CVehiclePopulation::ms_fMaxRatioForTrafficThrottle				= 0.8f;
float	CVehiclePopulation::ms_fMinDistanceForTrafficThrottle			= 100.0f;
float	CVehiclePopulation::ms_fMaxDistanceForTrafficThrottle			= 200.0f;
#if	__BANK
bool	CVehiclePopulation::ms_displayTrafficCenterInWorld				= false;
bool	CVehiclePopulation::ms_displayTrafficCenterOnVM					= false;
#endif

float CVehiclePopulation::ms_fCalculatedPopulationRangeScale			= 1.0f;
float CVehiclePopulation::ms_fCalculatedViewRangeScale					= 1.0f;
float CVehiclePopulation::ms_fCalculatedSpawnFrustumNearDist			= 0.0f;
bool CVehiclePopulation::ms_bZoomed										= false;
bool CVehiclePopulation::ms_bLockPopuationRangeScale					= false;
bank_float CVehiclePopulation::ms_fNearDistLodScaleThreshold				= 1.5f;

bool CVehiclePopulation::ms_AllowPoliceDeletion					= false;


CVehiclePopulationTuning CVehiclePopulation::ms_TuningData;

bool CVehiclePopulation::ms_bGenerationSkippedDueToNoLinksWithDisparity = false;

#define VEHGEN_CLEAR_DISABLED_NODES_FREQUENCY 2000
CNodeAddress CVehiclePopulation::ms_DisabledNodes[VEHGEN_MAX_DISABLED_NODES];
u32 CVehiclePopulation::ms_iNextDisabledNodeIndex						= 0;
u32 CVehiclePopulation::ms_iClearDisabledNodesTime						= 0;

s32 CVehiclePopulation::ms_iNumVehiclesCreatedThisCycle					= 0;
s32 CVehiclePopulation::ms_iNumPedsCreatedThisCycle						= 0;
s32 CVehiclePopulation::ms_iNumExcessEntitiesCreatedThisFrame				= 0;
s32 CVehiclePopulation::ms_iNumVehiclesDestroyedThisCycle				= 0;
s32 CVehiclePopulation::ms_iNumNetworkVehiclesCreatedThisFrame			= 0;
s32 CVehiclePopulation::ms_iNumWreckedEmptyCars							= 0;
s32 CVehiclePopulation::ms_iNumNormalVehiclesCreatedSinceDensityUpdate		= 0;

atRangeArray<CVehiclePopulation::CStreetModelAssociation, CVehiclePopulation::ms_iMaxStreetModelAssociations> CVehiclePopulation::m_StreetModelAssociations;

#if ENABLE_NETWORK_LOGGING
atFixedBitSet<MAX_NUM_ACTIVE_PLAYERS, u32> CVehiclePopulation::ms_PlayersTakingTurn;
#endif // ENABLE_NETWORK_LOGGING

int CVehiclePopulation::ms_iNumStreetModelAssociations = 0;

atFixedArray<CVehiclePopulation::CVehicleReuseEntry, VEHICLE_REUSE_POOL_SIZE> CVehiclePopulation::ms_vehicleReusePool;

bool CVehiclePopulation::ms_bVehicleReusePool							= true;
u8	CVehiclePopulation::ms_ReusedVehicleTimeout							= 15;
bool	CVehiclePopulation::ms_bVehicleReuseInMP							= true;
bool	CVehiclePopulation::ms_bVehicleReuseChecksHasBeenSeen				= true;

#if GTA_REPLAY
bool CVehiclePopulation::ms_bReplayActive								= false;
#endif // GTA_REPLAY

#if __BANK
bool CVehiclePopulation::ms_noPoliceSpawn								= false;
char CVehiclePopulation::ms_VehGroupOverride[64];
int CVehiclePopulation::ms_VehGroupOverridePercentage					= 100;
char CVehiclePopulation::ms_overrideArchetypeName[16];
bool CVehiclePopulation::ms_displayAddCullZonesInWorld					= false;
float CVehiclePopulation::ms_addCullZoneHeightOffset					= 0.5f;
bool CVehiclePopulation::ms_freezeKeyholeShape							= false;
bool CVehiclePopulation::ms_displaySpawnCategoryGrid					= false;
s32 CVehiclePopulation::ms_overrideNumberOfCars							= -1;
bool CVehiclePopulation::ms_bDisplayVehicleSpacings						= false;
bool CVehiclePopulation::ms_bRenderMarkupSpheres						= false;
bool CVehiclePopulation::ms_bHeavyDutyVehGenDebugging					= false;
bool CVehiclePopulation::ms_bDrawBoxesForVehicleSpawnAttempts			= false;
u32	CVehiclePopulation::ms_iDrawBoxesForVehicleSpawnAttemptsDuration	= 30;
bool CVehiclePopulation::ms_bEverybodyCruiseInReverse					= false;
bool CVehiclePopulation::ms_bDontStopForPeds							= false;
bool CVehiclePopulation::ms_bDontStopForCars							= false;

bool CVehiclePopulation::ms_bDebugVehicleReusePool						= false;
bool CVehiclePopulation::ms_bDisplayVehicleReusePool					= false;
bool CVehiclePopulation::ms_bDisplayVehicleCreationMethod				= false;
bool CVehiclePopulation::ms_bDisplayVehicleCreationCost					= false;
u32 CVehiclePopulation::ms_iVehicleCreationCostHighlightThreshold		= 1500;
bool CVehiclePopulation::ms_bDrawMPExcessiveDebug						= false;
bool CVehiclePopulation::ms_bDrawMPExcessivePedDebug					= false;

#undef vehPopDebugf2
#define vehPopDebugf2(fmt,...)	\
	if(CPopCycle::sm_bEnableVehPopDebugOutput) \
	{ \
		ms_debugMessageHistory[ms_nextDebugMessage].time = fwTimer::GetTimeInMilliseconds(); \
		formatf(ms_debugMessageHistory[ms_nextDebugMessage].msg, fmt, ##__VA_ARGS__); \
		formatf(ms_debugMessageHistory[ms_nextDebugMessage].timeStr, "%02d:%02d:%02d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond()); \
		ms_nextDebugMessage = (ms_nextDebugMessage + 1) % 64; \
		RAGE_DEBUGF2(vehicle_population,fmt,##__VA_ARGS__); \
	}

bool CVehiclePopulation::ms_showDebugLog = false;
u32 CVehiclePopulation::ms_nextDebugMessage = 0;
CVehiclePopulation::sDebugMessage CVehiclePopulation::ms_debugMessageHistory[64];
CVehiclePopulation::VehiclePopulationFailureData CVehiclePopulation::m_populationFailureData;

CVehiclePopulation::sSpawnEntry CVehiclePopulation::ms_spawnHistory[VEHICLE_POPULATION_SPAWN_HISTORY_SIZE];
bool CVehiclePopulation::ms_bDisplaySpawnHistory = false;
bool CVehiclePopulation::ms_bSpawnHistoryDisplayArrows = false;

bool CVehiclePopulation::ms_ignoreOnScreenRemovalDelay = false;
CVehiclePopulation::EVehLastRemovalReason   CVehiclePopulation::ms_lastRemovalReason = CVehiclePopulation::Removal_None;

bool CVehiclePopulation::ms_bCatchVehiclesRemovedInView = false;
float CVehiclePopulation::ms_fInViewDistanceThreshold = 0.0f;

float CVehiclePopulation::ms_RangeScale_fBaseRangeScale = 0.0f;
float CVehiclePopulation::ms_RangeScale_fScalingFromPosInfo = 0.0f;
float CVehiclePopulation::ms_RangeScale_fscalingFromCameraOrientation = 0.0f;
float CVehiclePopulation::ms_RangeScale_fScalingFromVehicleType = 0.0f;
float CVehiclePopulation::ms_RangeScale_fScriptedRangeMultiplier = 0.0f;
float CVehiclePopulation::ms_RangeScale_fHeightRangeScale = 0.0f;
float CVehiclePopulation::ms_RangeScale_fRendererRangeMultiplier = 0.0f;
float CVehiclePopulation::ms_RangeScale_fScopeModifier = 0.0f;

#endif // __BANK

#if __BANK
bool			CVehiclePopulation::ms_movePopulationControlWithDebugCam = false;
#endif // __DEV

bool			CVehiclePopulation::ms_vehiclePopGenLookAhead = false;

float CVehiclePopulation::ms_fMinGenerationScore						= 0.0f;
bank_float CVehiclePopulation::ms_fMinBaseGenerationScore				= 0.0f;

u32 CVehiclePopulation::ms_iVehiclePopulationFrameRate = 25; // gameconfig
u32 CVehiclePopulation::ms_iVehiclePopulationCyclesPerFrame = 1; // gameconfig
u32 CVehiclePopulation::ms_iMaxVehiclePopulationCyclesPerFrame = 0;
u32 CVehiclePopulation::ms_iVehiclePopulationFrameRateMP = 20; // gameconfig
u32 CVehiclePopulation::ms_iVehiclePopulationCyclesPerFrameMP = 1; // gameconfig
u32 CVehiclePopulation::ms_iMaxVehiclePopulationCyclesPerFrameMP = 0;
bool CVehiclePopulation::ms_bFrameCompensatedThisFrame = false;

atArray<CVehiclePopulation::FlightZone> CVehiclePopulation::sm_FlightZones;

atArray<CNodeAddress> CVehiclePopulation::sm_OpenNodes;
atArray<CNodeAddress> CVehiclePopulation::sm_ClosedNodes;
atArray<float> CVehiclePopulation::sm_NodesTotalDistance;
atArray<float> CVehiclePopulation::sm_NodesTotalDesiredNumVehs;

float CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup[16];
float CVehiclePopulation::ms_fDefaultVehiclesSpacing[16];
float CVehiclePopulation::ms_fCurrentVehiclesSpacing[16];
float CVehiclePopulation::ms_fCurrentVehicleSpacingMultiplier = 1.0f;
u32	CVehiclePopulation::ms_lastTimeAmbulanceCreated = 0;
u32	CVehiclePopulation::ms_lastTimeFireTruckCreated = 0;
bool CVehiclePopulation::ms_bAllowEmergencyServicesToBeCreated = true;

#define BOAT_EXTENDED_RANGE (1.5f)
#define MAX_NETWORK_GAME_CARS_PER_MACHINE	(23)
#define DELAY_TO_NEXT_AMBULANCE (180000)
#define DELAY_TO_NEXT_FIRETRUCK (180000)

static CVehiclePopulationAsyncUpdateData s_VehiclePopulationUpdateJobData ALIGNED(128);

static sysDependency s_VehiclePopulationDependency;
static u32 s_VehiclePopulationDependencyRunning;

static sysDependency s_UpdateDensitiesDependency;
static u32 s_UpdateDensitiesDependencyRunning;
static bool s_UpdateDensitiesDependencyLaunched;

static inline CPortalTracker::eProbeType GetVehicleProbeType(CVehicle *pVehicle);

/////////////////////////////////////////////////////////////
// FUNCTION: LoadFlightZones
// PURPOSE:  Loads the flight zone boxes.
// together into trains.
/////////////////////////////////////////////////////////////
static unsigned int FlightZonesFileVersionNumber = 1;
void CVehiclePopulation::LoadFlightZones(const char* pFileName)
{
	// Attempt to parse the note nodes XML data file and put our data into an
	// XML DOM (Document Object Model) tree.
	parTree* pTree = PARSER.LoadTree(pFileName, "");
	if(!pTree)
	{
		Errorf("Failed to flight zones file '%s'.", pFileName);
	}

	// Try to get the root DOM node.
	// It should be of the form: <flight_zones version="1"> ... </flight_zones>.

	const parTreeNode* pRootNode = pTree->GetRoot();
	Assert(pRootNode);
	Assert(stricmp(pRootNode->GetElement().GetName(), "flight_zones") == 0);

	// Make sure we have the correct version.
	unsigned int version = pRootNode->GetElement().FindAttributeIntValue("version", 0);
	Assert(version == FlightZonesFileVersionNumber);
	if(version != FlightZonesFileVersionNumber)
	{
		// Free all the memory created for the DOM tree (and its children).
		delete pTree;
		return;
	}

	// Go over any and all the DOM child trees and DOM nodes off of the root
	// DOM node.
	parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
	for(; i != pRootNode->EndChildren(); ++i)
	{
		// Check if this DOM child is the flight zone..
		// It should be of the form: <flight_zone ... />.
		Assertf((stricmp((*i)->GetElement().GetName(), "flight_zone") == 0), "An unknown and unsupported DOM child of the flight_zones has been encountered");

		// Add the flight_zone.
		FlightZone& flightZone = sm_FlightZones.Grow();

		flightZone.m_min.x	= (*i)->GetElement().FindAttributeFloatValue("minX", 0.0f);
		flightZone.m_min.y	= (*i)->GetElement().FindAttributeFloatValue("minY", 0.0f);
		flightZone.m_min.z	= (*i)->GetElement().FindAttributeFloatValue("minZ", 0.0f);
		flightZone.m_max.x	= (*i)->GetElement().FindAttributeFloatValue("maxX", 0.0f);
		flightZone.m_max.y	= (*i)->GetElement().FindAttributeFloatValue("maxY", 0.0f);
		flightZone.m_max.z	= (*i)->GetElement().FindAttributeFloatValue("maxZ", 0.0f);
	}

	// Free all the memory created for the DOM tree (and its children).
	delete pTree;
}
bool CVehiclePopulation::GetFlightHeightsFromFlightZones(const Vector3& pos, Vector2& minMaxHeight_out)
{
	u32 flightZoneCount = sm_FlightZones.GetCount();
	for(u32 i = 0; i < flightZoneCount; ++i)
	{
		if(	 (pos.x > sm_FlightZones[i].m_min.x) &&  (pos.x < sm_FlightZones[i].m_max.x) &&
			 (pos.y > sm_FlightZones[i].m_min.y) &&  (pos.y < sm_FlightZones[i].m_max.y))
		{
			minMaxHeight_out.x = sm_FlightZones[i].m_min.z;
			minMaxHeight_out.y = sm_FlightZones[i].m_max.z;

			return true;
		}
	}

	return false;
}

void CVehiclePopulation::LoadMarkupSpheres()
{
	ms_VehGenMarkUpSpheres.m_Spheres.SetCount(0);

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VEHGEN_MARKUP_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		parSettings settings = parSettings::sm_StandardSettings;
		settings.SetFlag(parSettings::PRELOAD_FILE, false);

		ASSERT_ONLY(bool bLoadedOk =) PARSER.LoadObject(pData->m_filename, "xml", ms_VehGenMarkUpSpheres, &settings);
		Assertf(bLoadedOk, "Error loading \"%s\"", pData->m_filename);
	}

#if __BANK
	ms_MarkUpEditor.Init();
#endif
}

void CVehiclePopulation::LoadStreetVehicleAssociations(const char * pFilename)
{
	ms_iNumStreetModelAssociations = 0;

	char tmpBuffer[256];

	parTree * pTree = PARSER.LoadTree(pFilename, "");
	if(AssertVerify(pTree))
	{
		const parTreeNode* pRootNode = pTree->GetRoot();
		if(AssertVerify(pRootNode))
		{
			Assert(stricmp(pRootNode->GetElement().GetName(), "StreetVehicleAssociations") == 0);
			ASSERT_ONLY(int iItems = pRootNode->GetElement().FindAttributeIntValue("NumItems", 0));

			parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
			for(; i != pRootNode->EndChildren(); ++i)
			{
				if(stricmp((*i)->GetElement().GetName(), "Associations") == 0)
				{
					ASSERT_ONLY(int iItemIndex = 0;)
					parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
					for(; j != (*i)->EndChildren(); ++j)
					{
						Assert(stricmp((*j)->GetElement().GetName(), "Item") == 0);
						Assertf(iItemIndex < iItems, "More items than expected.");

						const char * streetName = (*j)->GetElement().FindAttributeStringValue("StreetName", "none", tmpBuffer, 256, false, true);
						const char * modelName = (*j)->GetElement().FindAttributeStringValue("ModelName", "none", tmpBuffer, 256, false, true);

						Vector3 vMin,vMax;
						vMin.x = (*j)->GetElement().FindAttributeFloatValue("MinX", 0.0f);
						vMin.y = (*j)->GetElement().FindAttributeFloatValue("MinY", 0.0f);
						vMin.z = (*j)->GetElement().FindAttributeFloatValue("MinZ", 0.0f);
						vMax.x = (*j)->GetElement().FindAttributeFloatValue("MaxX", 0.0f);
						vMax.y = (*j)->GetElement().FindAttributeFloatValue("MaxY", 0.0f);
						vMax.z = (*j)->GetElement().FindAttributeFloatValue("MaxZ", 0.0f);

						if(AssertVerify(streetName && modelName))
						{
							AddStreetModelAssociation(streetName, modelName, vMin, vMax);
						}
						ASSERT_ONLY(iItemIndex++;)
					}
				}
			}
		}

		delete pTree;
	}
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitLevel
// PURPOSE :	Inits the vehicle population system for a game, initing the
//				vehicle population max size and parameters controlling the
//				populations management.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::InitValuesFromConfig()
{
	ms_iVehiclePopulationFrameRate = GET_POPULATION_VALUE(VehiclePopulationFrameRate);
	ms_iVehiclePopulationCyclesPerFrame = GET_POPULATION_VALUE(VehiclePopulationCyclesPerFrame);
	ms_iVehiclePopulationFrameRateMP = GET_POPULATION_VALUE(VehiclePopulationFrameRateMP);
	ms_iVehiclePopulationCyclesPerFrameMP = GET_POPULATION_VALUE(VehiclePopulationCyclesPerFrameMP);
	UpdateMaxVehiclePopulationCyclesPerFrame();

	/////////////////////////////////////////////////////////////////////////////
	// Ambient Vehicle Density
	ms_randomVehDensityMultiplier = GET_POPULATION_VALUE(VehicleAmbientDensityMultiplier) / 100.0f;
	PARAM_vehiclesAmbientDensityMultiplier.Get(ms_randomVehDensityMultiplier);
	/////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////
	// Parked Vehicle Density
	ms_parkedCarDensityMultiplier = GET_POPULATION_VALUE(VehicleParkedDensityMultiplier) / 100.0f;
	PARAM_vehiclesParkedDensityMultiplier.Get(ms_parkedCarDensityMultiplier);

	ms_lowPrioParkedCarDensityMultiplier = GET_POPULATION_VALUE(VehicleLowPrioParkedDensityMultiplier) / 100.0f;
	PARAM_vehiclesLowPrioParkedDensityMultiplier.Get(ms_lowPrioParkedCarDensityMultiplier);
	/////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////
	// Vehicles Upper Limit
	ms_iAmbientVehiclesUpperLimit = GET_POPULATION_VALUE(VehicleUpperLimit);
	PARAM_vehiclesUpperLimit.Get(ms_iAmbientVehiclesUpperLimit);	
	/////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////
	// Parked Vehicles Upper Limit
	ms_iParkedVehiclesUpperLimit = GET_POPULATION_VALUE(VehicleParkedUpperLimit);
	PARAM_parkedVehiclesUpperLimit.Get(ms_iParkedVehiclesUpperLimit);
	/////////////////////////////////////////////////////////////////////////////

#if GTA_VERSION
	// Maximum numbers were configured for the 140 vehicle baseline from PS3/360
	const float fVehicleModelInfoMaxNumberMultiplier = Max((ms_iAmbientVehiclesUpperLimit + ms_iParkedVehiclesUpperLimit) / 140.0f, 1.0f);
	CVehicleModelInfo::SetMaxNumberMultiplier(fVehicleModelInfoMaxNumberMultiplier);
#endif // GTA_VERSION

#if ((RSG_PC || RSG_DURANGO || RSG_ORBIS) && 0)
	ms_vehicleMemoryBudgetMultiplierNG = CGameConfig::Get().GetConfigPopulation().m_VehicleMemoryMultiplier / 100.0f;
	PARAM_vehiclesMemoryMultiplier.Get(CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG);
	CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG = Max(0.05f, Min(10.0f, CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG));
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

	/////////////////////////////////////////////////////////////////////////////
	// Vehicle Keyhole Culling
	ms_popGenKeyholeOuterShapeThickness = static_cast<float>(GET_POPULATION_VALUE(VehicleKeyholeShapeOuterThickness));
	ms_popGenKeyholeInnerShapeThickness = static_cast<float>(GET_POPULATION_VALUE(VehicleKeyholeShapeInnerThickness));
	ms_popGenKeyholeShapeInnerBandRadius = static_cast<float>(GET_POPULATION_VALUE(VehicleKeyholeShapeInnerRadius));
	ms_popGenKeyholeShapeOuterBandRadius = static_cast<float>(GET_POPULATION_VALUE(VehicleKeyholeShapeOuterRadius));
	ms_popGenKeyholeSideWallThickness = static_cast<float>(GET_POPULATION_VALUE(VehicleKeyholeSideWallThickness));

	/////////////////////////////////////////////////////////////////////////////
	// Vehicle Creation Distance
	ms_creationDistance = static_cast<float>(GET_POPULATION_VALUE(VehicleMaxCreationDistance));
	ms_creationDistanceOffScreen = static_cast<float>(GET_POPULATION_VALUE(VehicleMaxCreationDistanceOffscreen));

	/////////////////////////////////////////////////////////////////////////////
	// Vehicle Cull Range
	ms_cullRange = static_cast<float>(GET_POPULATION_VALUE(VehicleCullRange));
	ms_cullRangeOnScreenScale = static_cast<float>(GET_POPULATION_VALUE(VehicleCullRangeOnScreenScale)) / 100.0f;
	ms_cullRangeOffScreen = static_cast<float>(GET_POPULATION_VALUE(VehicleCullRangeOffScreen));

#if __BANK
	ms_fInViewDistanceThreshold = ms_cullRange - 10.0f;
#endif

	ms_densityBasedRemovalRateScale = static_cast<float>(GET_POPULATION_VALUE(DensityBasedRemovalRateScale));
	ms_densityBasedRemovalTargetHeadroom = GET_POPULATION_VALUE(DensityBasedRemovalTargetHeadroom);

	// Set up the lookup table which maps road node densities to desired vehicles-per-meter
	// This is a straight multiplier now, but we might want to tweak individual density bands later.
}

void CVehiclePopulation::InitValuesFromTunables()
{
	ms_iAmbientVehiclesUpperLimitMP = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MP_VEHICLE_UPPER_LIMIT", 0x99afe469), CGameConfig::Get().GetConfigPopulation().m_VehicleUpperLimitMP);
	PARAM_vehiclesUpperLimitMP.Get(ms_iAmbientVehiclesUpperLimitMP);	

	ms_fLocalAmbientVehicleBufferMult = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LOCAL_AMBIENT_VEHICLE_BUFFER_MULT", 0x7a5122cb), ms_fLocalAmbientVehicleBufferMult);
	ms_fLocalAmbientVehicleBufferMult = Clamp(ms_fLocalAmbientVehicleBufferMult, 0.0f, 1.0f);
	
	ms_fMaxLocalParkedCarsMult = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_LOCAL_PARKED_CARS_MULT", 0x9f9ffd8d), ms_fMaxLocalParkedCarsMult);
	ms_bNetworkPopCheckParkedCarsToggle = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORK_POP_CHECK_PARKED_CARS", 0xaee8ef91), ms_bNetworkPopCheckParkedCarsToggle);
	ms_bNetworkPopMaxIfCanUseLocalBufferToggle = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NETWORK_POP_MAX_IF_CAN_USE_LOCAL_BUFFER", 0xfa904d6f), ms_bNetworkPopMaxIfCanUseLocalBufferToggle);
}

void CVehiclePopulation::InitTunables()
{
	InitValuesFromTunables();
	InitVehicleSpacings();
	InitPlayersRoadModifiers();
}

void CVehiclePopulation::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		InitValuesFromConfig();
		InitValuesFromTunables();
	
		InitVehicleSpacings();
		InitPlayersRoadModifiers();

		ms_aAmbientVehicleData.Reserve(VEHICLE_POOL_SIZE);

		s_VehiclePopulationDependency.Init( VehiclePopulationAsyncUpdate::RunFromDependency, 0, 0 );
		s_VehiclePopulationDependency.m_Priority = sysDependency::kPriorityLow;

		s_UpdateDensitiesDependency.Init( VehiclePopAsyncUpdateDensities::RunFromDependency, 0, 0 );
		s_UpdateDensitiesDependency.m_Priority = sysDependency::kPriorityLow;
	}
	else if (initMode == INIT_SESSION)
	{
		CTheCarGenerators::InitLevelWithMapUnloaded();

#if __BANK
		if (PARAM_catchvehiclesremovedinview.Get())
			ms_bCatchVehiclesRemovedInView = true;

		if (PARAM_logcreatedvehicles.Get())
			CVehicleFactory::ms_bLogCreatedVehicles = true;

		if (PARAM_logdestroyedvehicles.Get())
			CVehicleFactory::ms_bLogDestroyedVehicles = true;			
#endif

		ms_lastPopCycleChangeTime = 0;
		ms_lastTimeLawEnforcerCreated = 0;
		ms_timeToClearInterestingVehicle = 0;
		ms_overrideNumberOfParkedCars = -1;
		ms_desertedStreetsMultiplier = 1.0f;
		ms_pInterestingVehicle = NULL;
		ms_bUseExistingCarNodes = false;
		ms_bAllowGarbageTrucks = true;
		ms_bDesertedStreetMultiplierLocked = false;
		ms_lastGenLinkAttemptCountUpdateMs = 0;
#if !__FINAL
		if(PARAM_nocars.Get() || PARAM_nocarsexceptcops.Get() || PARAM_novehs.Get() || PARAM_novehsexcepttrains.Get())
		{
			gbAllowVehGenerationOrRemoval = false;
			gbAllowCopGenerationOrRemoval = PARAM_nocarsexceptcops.Get();
			gbAllowTrainGenerationOrRemoval = PARAM_novehsexcepttrains.Get();
		}
#endif // !__FINAL
		ms_bAllowRandomBoats = ms_bDefaultValueAllowRandomBoats;
		ms_bAllowRandomBoatsMP = ms_bDefaultValueAllowRandomBoatsMP;

		ms_debugOverrideVehDensityMultiplier = 1.0f;
		ms_bMadDrivers = true;

#if !__FINAL
		if( PARAM_maxVehicles.Get() )
		{
			ms_debugOverrideVehDensityMultiplier = 6.0f;
		}
		else if( PARAM_minVehicles.Get() )
		{
			ms_debugOverrideVehDensityMultiplier = 0.1f;
		}
#endif // !__FINAL

		ms_scriptedRangeMultiplier = 1.0f;

		CTheCarGenerators::InitLevelWithMapLoaded();

		// Load 'ms_TuningData'
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VEHICLE_POPULATION_FILE);
		if(pData && DATAFILEMGR.IsValid(pData))
		{
			parSettings settings = parSettings::sm_StandardSettings;
			settings.SetFlag(parSettings::PRELOAD_FILE, false);

			PARSER.LoadObject(pData->m_filename, "xml", ms_TuningData, &settings);
		}

		if(PARAM_noReuseInMP.Get())
		{
			ms_bVehicleReuseInMP = false;
		}
		if(PARAM_extraReuse.Get())
		{
			ms_bVehicleReuseChecksHasBeenSeen = false;
		}
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
		// Init & load flight-zones
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::FLIGHTZONES_FILE);
		while(DATAFILEMGR.IsValid(pData))
		{
			LoadFlightZones(pData->m_filename);

			pData = DATAFILEMGR.GetNextFile(pData);
		}

		// Init & load street/vehicle associations
		ms_iNumStreetModelAssociations = 0;
		for(int i=0; i<ms_iMaxStreetModelAssociations; i++)
			m_StreetModelAssociations[i].Clear();

		const CDataFileMgr::DataFile* pAssocData = DATAFILEMGR.GetFirstFile(CDataFileMgr::STREET_VEHICLE_ASSOCIATION_FILE);
		if(DATAFILEMGR.IsValid(pAssocData))
		{
			LoadStreetVehicleAssociations(pAssocData->m_filename);
		}

#if __BANK
		const char* vehicleOverride = NULL;
		PARAM_vehicleoverride.Get(vehicleOverride);
		u32 mi = CModelInfo::GetModelIdFromName(vehicleOverride).GetModelIndex();
		if (mi != fwModelId::MI_INVALID)
		{
			gPopStreaming.SetVehicleOverrideIndex(mi);
			formatf(ms_overrideArchetypeName, "%s", vehicleOverride);
		}
#endif

		LoadMarkupSpheres();
	}

	ms_iNumNodesOnPlayersRoad = 0;
}

void CVehiclePopulation::UpdateMaxVehiclePopulationCyclesPerFrame()
{
	ms_iMaxVehiclePopulationCyclesPerFrame = static_cast<u32>(ceilf(fwTimer::GetMaximumFrameTime() / (1.0f / (ms_iVehiclePopulationFrameRate * ms_iVehiclePopulationCyclesPerFrame))));
	ms_iMaxVehiclePopulationCyclesPerFrameMP = static_cast<u32>(ceilf(fwTimer::GetMaximumFrameTime() / (1.0f / (ms_iVehiclePopulationFrameRateMP * ms_iVehiclePopulationCyclesPerFrameMP))));
}

void CVehiclePopulation::InitVehicleSpacings()
{
	for (int i = 0; i < NUM_LINK_DENSITIES; i++)
	{
		ms_fDefaultVehiclesSpacing[i] = static_cast<float>(GET_INDEXED_POPULATION_VALUE(VehicleSpacing, i));
	}
	
	ms_fCurrentVehiclesSpacing[0] = 1.0f;
	ms_fVehiclesPerMeterDensityLookup[0] = 1.0f;

	for(int d=1; d<NUM_LINK_DENSITIES; d++)
	{
		ms_fCurrentVehiclesSpacing[d] = ms_fDefaultVehiclesSpacing[d];
		ms_fVehiclesPerMeterDensityLookup[d] = 1.0f / ms_fCurrentVehiclesSpacing[d];
	}

#if __BANK
	UpdateCurrentVehicleSpacings();
#endif
}

void CVehiclePopulation::ModifyPlayersRoadModifiers()
{
	for(u32 i = 0; i < NUM_PLAYERS_ROAD_CID_PENALTIES; ++i)
	{
		ms_fPlayersRoadChangeInDirectionPenalties[i][PLAYERS_ROAD_CID_ANGLE] = Cosf(ms_fPlayersRoadChangeInDirectionPenalties[i][PLAYERS_ROAD_CID_ANGLE_DEGREES] * DtoR);
	}

	ScanForNodesOnPlayersRoad(true);
}

void CVehiclePopulation::InitPlayersRoadModifiers()
{
	ms_fPlayersRoadScanDistance = static_cast<float>(GET_POPULATION_VALUE(PlayersRoadScanDistance));

	for(u32 i = 0; i < NUM_LINK_DENSITIES; ++i)
	{
		ms_iPlayerRoadDensityInc[i] = GET_INDEXED_POPULATION_VALUE(PlayerRoadDensityInc, i);
		ms_iNonPlayerRoadDensityDec[i] = GET_INDEXED_POPULATION_VALUE(NonPlayerRoadDensityDec, i);
	}

	ms_fPlayersRoadChangeInDirectionPenalties[0][PLAYERS_ROAD_CID_ANGLE_DEGREES] = 0.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[0][PLAYERS_ROAD_CID_PENALTY] = 0.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[1][PLAYERS_ROAD_CID_ANGLE_DEGREES] = 40.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[1][PLAYERS_ROAD_CID_PENALTY] = 50.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[2][PLAYERS_ROAD_CID_ANGLE_DEGREES] = 90.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[2][PLAYERS_ROAD_CID_PENALTY] = 100.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[3][PLAYERS_ROAD_CID_ANGLE_DEGREES] = 105.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[3][PLAYERS_ROAD_CID_PENALTY] = 150.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[4][PLAYERS_ROAD_CID_ANGLE_DEGREES] = 120.0f;
	ms_fPlayersRoadChangeInDirectionPenalties[4][PLAYERS_ROAD_CID_PENALTY] = 200.0f;

	ModifyPlayersRoadModifiers();
}

float CVehiclePopulation::GetVehicleSpacingsMultiplier(const float UNUSED_PARAM(fRangeScale), float * fPopCycleMult, float * fTotalRandomVehicleDensityMult, float * fRangeScaleMult, float * fWantedLevelMult)
{
	const float fPopScheduleScale = ((float)CPopCycle::GetCurrentPercentageOfMaxCars()) / 100.0f;

	// The higher the population range scale gets, we must apply a proportional scaling to the vehicle spacings
	// otherwise we will quickly hit the max num vehicles limit w/o having an even vehicle distribution
//	static dev_float fRangeProportion = 0.5f;
//	const float fRangeBasedMultiplier = 1.0f / Max(fRangeScale * fRangeProportion, 1.0f);

	const float fRangeBasedMultiplier = 1.0f;

	if(fPopCycleMult)
		*fPopCycleMult = fPopScheduleScale;
	if(fTotalRandomVehicleDensityMult)
		*fTotalRandomVehicleDensityMult = GetTotalRandomVehDensityMultiplier();
	if(fRangeScaleMult)
		*fRangeScaleMult = fRangeBasedMultiplier;
	if(fWantedLevelMult)
		*fWantedLevelMult = CPopCycle::GetWantedCarMultiplier();

	float fMultiplier = Min(fPopScheduleScale, CPopCycle::GetWantedCarMultiplier()) * GetTotalRandomVehDensityMultiplier() * fRangeBasedMultiplier;
	return fMultiplier;
}

void CVehiclePopulation::UpdateCurrentVehicleSpacings(float fRangeScale)
{
	// If no rangeScale was passed in (-1.0f) then we have to calculate it now
	if(fRangeScale < 0.0f)
	{
		fRangeScale = GetPopulationRangeScale();
	}

	const float fFurthestSpacing = ms_fDefaultVehiclesSpacing[1];
	const float fClosestSpacing = ms_fDefaultVehiclesSpacing[15] * 0.25f;

	ms_fCurrentVehicleSpacingMultiplier = GetVehicleSpacingsMultiplier(fRangeScale);

	for(int d=0; d<NUM_LINK_DENSITIES; d++)
	{
		if( ms_fCurrentVehicleSpacingMultiplier < SMALL_FLOAT )
		{
			ms_fCurrentVehiclesSpacing[d] = 1000.0f;
		}
		else
		{
			ms_fCurrentVehiclesSpacing[d] = ms_fDefaultVehiclesSpacing[d] / ms_fCurrentVehicleSpacingMultiplier;
		}

		if(ms_fCurrentVehiclesSpacing[d] > fFurthestSpacing)
		{
			static dev_float fMaxDelta = 30.0f;
			static dev_float fDivisor = 8.0f; //4.0f;

			const float fDelta = (ms_fCurrentVehiclesSpacing[d] - fFurthestSpacing) / fDivisor;
			ms_fCurrentVehiclesSpacing[d] = fFurthestSpacing + (fMaxDelta * (fDelta/fMaxDelta));

			ms_fCurrentVehiclesSpacing[d] = Clamp(ms_fCurrentVehiclesSpacing[d], fClosestSpacing, fFurthestSpacing + fMaxDelta);
		}
		else
		{
			ms_fCurrentVehiclesSpacing[d] = Clamp(ms_fCurrentVehiclesSpacing[d], fClosestSpacing, fFurthestSpacing);
		}

		if(ms_fCurrentVehiclesSpacing[d] > SMALL_FLOAT)
		{
			ms_fVehiclesPerMeterDensityLookup[d] = 1.0f / ms_fCurrentVehiclesSpacing[d];
		}
		else
		{
			ms_fVehiclesPerMeterDensityLookup[d] = 1.0f;
		}
	}
}

#if __BANK
void CVehiclePopulation::ModifyVehicleSpacings()
{
	UpdateCurrentVehicleSpacings();
}
void CVehiclePopulation::OnScaleDownDefaultVehicleSpacings()
{
	for(int d=0; d<NUM_LINK_DENSITIES; d++)
	{
		ms_fDefaultVehiclesSpacing[d] *= 0.9f;
	}
	ModifyVehicleSpacings();
}
void CVehiclePopulation::OnScaleUpDefaultVehicleSpacings()
{
	for(int d=0; d<NUM_LINK_DENSITIES; d++)
	{
		ms_fDefaultVehiclesSpacing[d] *= 1.1f;
	}
	ModifyVehicleSpacings();
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShutdownLevel
// PURPOSE :	Shuts down the vehicle population system for a game.  No special
//				resources are currently allocated or need freeing here so it
//				does nothing.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        sm_FlightZones.clear();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    ClearInterestingVehicles();
    }
	else if(shutdownMode == SHUTDOWN_CORE)
	{
		ms_aAmbientVehicleData.Reset();
	}

	CVehicleFactory::GetFactory()->ClearDestroyedVehCache();
	ms_iNumNodesOnPlayersRoad = 0;
}

void CVehiclePopulation::UpdateLinksAfterGather()
{
	PF_PUSH_TIMEBAR_IDLE("Wait for VehiclePopulationAsyncUpdate");
	while(true)
	{
		volatile u32 *pDependenciesRunning = &s_VehiclePopulationDependencyRunning;
		if(*pDependenciesRunning == 0)
		{
			break;
		}

		sysIpcYield(PRIO_NORMAL);
	}
	PF_POP_TIMEBAR();


	PF_FUNC(UpdateLinksAfterGather);
	
	// Update the generation links.
	// The makes and updates a list of all path links where
	// vehicles might potentially be created.
	PF_PUSH_TIMEBAR("Update Generation links");
#if !__FINAL
	if(gbAllowVehGenerationOrRemoval)
#endif // !__FINAL
	{
		UpdateAmbientPopulationData();

		vehPopDebugf3("Update current (%d) gen links.", ms_numGenerationLinks);
		UpdateCurrentVehGenLinks();
	}
	PF_POP_TIMEBAR();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Process
// PURPOSE :	Manages the vehicle population by removing what vehicles we can
//				and adding what vehicles we should.
//				It creates ambient vehicles and also vehicles at attractors when
//				appropriate.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
extern void FireCarForcePos();
#endif // __BANK

void CVehiclePopulation::Process()
{
#if GTA_REPLAY
	ProcessReplay();

	if (ms_bReplayActive)
	{
		return;
	}
#endif // GTA_REPLAY

	PF_FUNC(VehiclePopulationProcess);
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::VEHICLEPOP);

	if (!CStreaming::IsPlayerPositioned())
		return;

	// This is needed in case we just loaded some scenario region that had been loaded at
	// an earlier time, to make sure we re-attach all scenario chain users before we consider
	// spawning new ones.
	CScenarioChainingGraph::UpdateNewlyLoadedScenarioRegions();

#if __BANK
	g_vehicleGlassMan.UpdateDebug(); // this needs to be called in a specific place .. right here actually
#endif // __BANK

	PF_AUTO_PUSH_TIMEBAR("Veh Pop Process");

	// Recompute ms_fCalculatedPopulationRangeScale for this frame's update.
	CalculatePopulationRangeScale();

	//cache AI velocities here now, instead of during the scene update, since we'll
	//need them for the extra ai update in the first frame
	if (!fwTimer::IsGamePaused())
	{
		CVehicle::StartAiUpdate();
	}

#if !__FINAL
	CTheCarGenerators::DebugRender();
#endif // !__FINAL

	if(fwTimer::IsGamePaused())
	{
		return;
	}

	PF_START_TIMEBAR_DETAIL("IncidentManager");
	CIncidentManager::GetInstance().Update();

	PF_START_TIMEBAR_DETAIL("CDispatchManager");
	CDispatchManager::GetInstance().Update();

	PF_START_TIMEBAR_DETAIL("COrderManager");
	COrderManager::GetInstance().Update();

	Assert(!s_VehiclePopulationDependencyRunning);

	if(s_UpdateDensitiesDependencyLaunched)
	{
		PF_PUSH_TIMEBAR_IDLE("Wait for VehiclePopulationAsyncUpdate");
		while(true)
		{
			volatile u32 *pDependenciesRunning = &s_UpdateDensitiesDependencyRunning;
			if(*pDependenciesRunning == 0)
			{
				break;
			}

			sysIpcYield(PRIO_NORMAL);
		}	
		PF_POP_TIMEBAR();

		ms_iCurrentActiveLink = 0;
		ms_iNumNormalVehiclesCreatedSinceDensityUpdate = 0;
		s_UpdateDensitiesDependencyLaunched = false;
	}

	PF_START_TIMEBAR_DETAIL("Misc");

#if __BANK
	// fire car debug widget has option to force car position every frame,
	// needs to get called from somewhere that get processed every frame - i.e. here
	if(CVehicleFactory::ms_bFireCarForcePos)
	{
		FireCarForcePos();
	}
#endif

	UpdateDesertedStreets();


	// Get the interest point about which to base the population.
	// Make sure to pick a position and forward vector designed to
	// get the best end visual results out of the population
	// management system.
	// NOTE: We might need to change this for multi-player so that
	// an equal population counts are created around each player.
#if __BANK
	if(CPedPopulation::ms_forceScriptedPopulationOriginOverride)
	{
		SetScriptCamPopulationOriginOverride();
	}
#endif

	const u32 time = fwTimer::GetTimeInMilliseconds();

	if (time >= ms_iClearDisabledNodesTime)
	{
		ClearDisabledNodes();
		ms_iClearDisabledNodesTime = time + VEHGEN_CLEAR_DISABLED_NODES_FREQUENCY;
	}

	const float rangeScale = GetPopulationRangeScale();

	UpdateCurrentVehicleSpacings(rangeScale);

	ms_lastPopCenter = CPedPopulation::GetPopulationControlPointInfo(ms_useTempPopControlPointThisFrame ? &ms_tmpPopOrigin : NULL);

	const float removalTimeScale = GetRemovalTimeScale();

	////////////////////////////////////////////////////////////////////////////
	ms_iNumVehiclesCreatedThisCycle = ms_iNumNetworkVehiclesCreatedThisFrame;

	if(CDispatchManager::ms_SpawnedResourcesThisFrame)
	{
		IncrementNumVehiclesCreatedThisCycle();
	}

	ms_iNumPedsCreatedThisCycle = 0;
	ms_iNumVehiclesDestroyedThisCycle = 0;
	ms_iNumNetworkVehiclesCreatedThisFrame = 0;
	////////////////////////////////////////////////////////////////////////////

	PF_INCREMENTBY(EntitiesCreatedByVehiclePopulation, -ms_iNumVehiclesCreatedThisCycle);

	UpdateVehicleReusePool();

	// Cull the vehicle population be removing cars that
	// are too far away to be important.
	PF_PUSH_TIMEBAR("RemoveDistantVehsSpecial");
#if !__FINAL
	if(gbAllowVehGenerationOrRemoval||gbAllowCopGenerationOrRemoval)
#endif // !__FINAL
	{
		TUNE_GROUP_BOOL(VEHICLE_POPULATION, forceNewMPExcessCopAndWreckedVehs, false);
		if(NetworkInterface::IsGameInProgress() || forceNewMPExcessCopAndWreckedVehs)
		{
			RemoveExcessEmptyCopAndWreckedVehs_MP();
		}
		else
		{
			RemoveExcessEmptyCopAndWreckedVehs();
		}

		RemoveEmptyOrPatrollingLawVehs(ms_lastPopCenter.m_centre);
		RemoveCongestionInNetGames();
		RemoveEmptyCrowdingPlayersInNetGames();
	}
	PF_POP_TIMEBAR();

	if(!ms_noVehSpawn)
	{
#if !__FINAL
		if(gbAllowTrainGenerationOrRemoval)
#endif // !__FINAL
		{
			// Create/remove trains
			PF_PUSH_TIMEBAR("DoTrainGenerationAndRemoval");
			CTrain::DoTrainGenerationAndRemoval();		// Trains have their own code for creation and removal
			PF_POP_TIMEBAR();
		}

		// By cycling calls to GenerateRandomVehs(), GenerateRandomCarsOnPaths(), and CTheCarGenerators::Process(),
		// we're able to limit vehicle generation to one per population cycle
		enum
		{
			VEHGEN_STATE_GENERATE_RANDOM_VEHICLES = 0,
			VEHGEN_STATE_GENERATE_SPECIAL_VEHICLES,
			VEHGEN_STATE_CAR_GENERATORS,
			NUM_VEHGEN_STATES
		};

		static u32 s_State = 0;

#if !__FINAL
		if(gbAllowVehGenerationOrRemoval)
#endif // !__FINAL
		{
			PF_PUSH_TIMEBAR("DoHeliGenerationAndRemoval");
			CHeli::DoHeliGenerationAndRemoval();		// Not as in depth as IV was, but still used to sent helis away and give them to ambient pop
			PF_POP_TIMEBAR();

			static float s_TimeSteps = 0.0f;
			s_TimeSteps += Min(fwTimer::GetTimeStep(), fwTimer::GetMaximumFrameTime()) / (1.0f / (GetPopulationFrameRate() * GetPopulationCyclesPerFrame()));

			ms_bFrameCompensatedThisFrame = s_TimeSteps >= 2.0f;

			if(ms_bInstantFillPopulation)
			{
				// Instant fill requires additional population cycles
				s_TimeSteps += ms_fInstantFillExtraPopulationCycles;
			}

			while(s_TimeSteps >= 1.0f)
			{
				if(ms_iNumVehiclesDestroyedThisCycle == 0)
				{
					PF_PUSH_TIMEBAR("RemoveDistantVehs");
					RemoveDistantVehs(ms_lastPopCenter.m_centre, ms_lastPopCenter.m_baseHeading, rangeScale, removalTimeScale);
					PF_POP_TIMEBAR();
				}

				const u32 initialState = s_State;

				while(1)
				{
					// Break if we've created an entity, or spillover hasn't been depleted
					if(ms_iNumVehiclesCreatedThisCycle + ms_iNumPedsCreatedThisCycle > 0)
					{
						break;
					}

					switch(s_State)
					{
					case VEHGEN_STATE_GENERATE_RANDOM_VEHICLES:
						{
							// ms_bInstantFillPopulation handling moved to GenerateRandomVehs()
							PF_PUSH_TIMEBAR("GenerateRandomVehs");
							GenerateRandomVehs(ms_lastPopCenter.m_centre, ms_lastPopCenter.m_centreVelocity, rangeScale);
							PF_POP_TIMEBAR();
							break;
						}
					case VEHGEN_STATE_GENERATE_SPECIAL_VEHICLES:
						{
							PF_PUSH_TIMEBAR("GenerateRandomCarsOnPaths");
							GenerateRandomCarsOnPaths(ms_lastPopCenter.m_centre, ms_lastPopCenter.m_centreVelocity, rangeScale); 
							PF_POP_TIMEBAR();
							break;
						}
					case VEHGEN_STATE_CAR_GENERATORS:
						{
							PF_PUSH_TIMEBAR("GenerateScheduledVehiclesIfReady");
							CTheCarGenerators::GenerateScheduledVehiclesIfReady();
							PF_POP_TIMEBAR();
							break;
						}
					}
					
					// If we've executed all code paths, break. Otherwise, advance state and execute the next code path immediately.
					// This guarantees that population cycles are not wasted while code paths are ready to generate vehicles.
					s_State = (s_State + 1) % NUM_VEHGEN_STATES;

					if(s_State == initialState)
					{
						break;
					}
				}

				PF_PUSH_TIMEBAR("Process Car Generators");
				PF_START(ProcessCarGenerators);
				CTheCarGenerators::Process();
				PF_STOP(ProcessCarGenerators);
				PF_POP_TIMEBAR();

				// Decrement creations by one -- should we encounter multiple creations in a single cycle,
				// allow them to spill over to the next cycle, effectively blocking subsequent, excess, creations.
				if(ms_iNumVehiclesCreatedThisCycle > 0)
				{
					--ms_iNumVehiclesCreatedThisCycle;
					PF_INCREMENT(EntitiesCreatedByVehiclePopulation);
				}
				else if(ms_iNumPedsCreatedThisCycle > 0)
				{
					--ms_iNumPedsCreatedThisCycle;
					PF_INCREMENT(EntitiesCreatedByVehiclePopulation);
				}

				if(ms_iNumVehiclesDestroyedThisCycle > 0)
				{
					--ms_iNumVehiclesDestroyedThisCycle;
					PF_INCREMENT(EntitiesDestroyedByVehiclePopulation);
				}

				s_TimeSteps -= 1.0f;

				if(NetworkInterface::IsGameInProgress())
				{
					ms_previousGenLinkDisparity[ms_genLinkDisparityIndex++] = GetLinkDisparity();
					if(ms_genLinkDisparityIndex == VEHGEN_NUM_AVERAGE_DISPARITY_FRAMES)
						ms_genLinkDisparityIndex = 0;
					float fAverageDisparity = 0.0f;
					for(int i = 0; i < VEHGEN_NUM_AVERAGE_DISPARITY_FRAMES; ++i)
					{
						fAverageDisparity += ms_previousGenLinkDisparity[i];
					}
					fAverageDisparity /= VEHGEN_NUM_AVERAGE_DISPARITY_FRAMES;
					ms_averageGenLinkDisparity = fAverageDisparity;
				}
			}

			// Persist
			PF_PUSH_TIMEBAR("RemoveDistantVehs");
			PF_POP_TIMEBAR();
			PF_PUSH_TIMEBAR("GenerateRandomVehs");
			PF_POP_TIMEBAR();
			PF_PUSH_TIMEBAR("GenerateRandomCarsOnPaths");
			PF_POP_TIMEBAR();
			PF_PUSH_TIMEBAR("Process Car Generators");
				PF_PUSH_TIMEBAR("ScheduleVehsForGenerationIfInRange");
				PF_POP_TIMEBAR();
			PF_POP_TIMEBAR();
			PF_PUSH_TIMEBAR("GenerateScheduledVehiclesIfReady");
				PF_PUSH_TIMEBAR("GenerateScheduledVehicle");
				PF_POP_TIMEBAR();
				PF_PUSH_TIMEBAR("FinishScheduledVehicleCreation");
				PF_POP_TIMEBAR();
			PF_POP_TIMEBAR();

			ms_iNumExcessEntitiesCreatedThisFrame = ms_iNumVehiclesCreatedThisCycle + ms_iNumPedsCreatedThisCycle;
		}
	}

	PF_INCREMENTBY(EntitiesCreatedByVehiclePopulation, ms_iNumVehiclesCreatedThisCycle + ms_iNumPedsCreatedThisCycle);
	PF_INCREMENTBY(EntitiesDestroyedByVehiclePopulation, ms_iNumVehiclesDestroyedThisCycle);

	PF_INCREMENTBY(TotalEntitiesCreatedByPopulation, PF_COUNTER_VAR(EntitiesCreatedByVehiclePopulation).GetValue());
	PF_INCREMENTBY(TotalEntitiesDestroyedByPopulation, PF_COUNTER_VAR(EntitiesDestroyedByVehiclePopulation).GetValue());

	// Reset some per-frame variables
	ms_bInstantFillPopulation = (s8)Max(0, ms_bInstantFillPopulation - 1) ;

	ClearInterestingVehicles(false);

	//--------------------------------------------------
	// Scan for which nodes are on the player's road

	PF_PUSH_TIMEBAR("Scan nodes on players road");

	ScanForNodesOnPlayersRoad();

	PF_POP_TIMEBAR();

#if !__FINAL
	if(gbAllowVehGenerationOrRemoval)
#endif // !__FINAL
	{
		//Update the keyhole shape so scenario cargen can use it for validity testing.
		UpdateKeyHoleShape(ms_lastPopCenter.m_centre, ms_lastPopCenter.m_centreVelocity, ms_lastPopCenter.m_baseHeading, ms_lastPopCenter.m_FOV, ms_lastPopCenter.m_tanHFov, rangeScale);

		CalculateSpawnFrustumNearDist();

		StartAsyncUpdateJob();
	}

	// we cache the value so we can access it from the outside;
	ms_desiredNumberAmbientVehicles = static_cast<s32>(CVehiclePopulation::GetDesiredNumberAmbientVehicles());

	if(NetworkInterface::IsGameInProgress())
	{
		if(fwTimer::GetSystemTimeInMilliseconds() > (ms_NetworkTimeVisibilityFail + ms_NetworkCloneRejectionCooldownDelay))
		{
			ms_CloneSpawnValidationMultiplier -= (fwTimer::GetTimeStep() * ms_CloneSpawnSuccessCooldownTimerScale);
			ms_CloneSpawnValidationMultiplier = Max(ms_CloneSpawnValidationMultiplier, 0.0f);

			if((ms_CloneSpawnValidationMultiplier < (1.0f - SMALL_FLOAT)))
			{
				ms_NetworkTimeExtremeVisibilityFail = 0;
			}
		}
	}

#if __BANK
	ValidateVehicleCounts();
#endif
	
#if __STATS
	u32 ambientVehs = PF_COUNTER_VAR(AmbientVehiclesCreated).GetValue();
	u32 cargenVehs = PF_COUNTER_VAR(CargenVehiclesCreated).GetValue();
	u32 networkVehs = PF_COUNTER_VAR(VehiclesCreatedByNetwork).GetValue();
	PF_INCREMENTBY(TotalVehiclesCreated,  ambientVehs + cargenVehs + networkVehs );

	int iVehiclesUpperLimit;
	float fDesertedStreetsMult, fInteriorMult, fWantedMult, fCombatMult, fHighwayMult, fBudgetMult;
	float fMaxAmbientVehicles = CVehiclePopulation::GetDesiredNumberAmbientVehicles(&iVehiclesUpperLimit, &fDesertedStreetsMult, &fInteriorMult, &fWantedMult, &fCombatMult, &fHighwayMult, &fBudgetMult);
	s32	TotalAmbientCarsOnMap = CVehiclePopulation::ms_numRandomCars;

	// The number of ambient vehicles were we to address all active disparities
	float maxPossibleAmbientVehicles = static_cast<float>(TotalAmbientCarsOnMap + ms_iNumActiveLinks - ms_iNumNormalVehiclesCreatedSinceDensityUpdate);

	float ambientVehiclePopulationRatio = 1.0f;
	
	if(maxPossibleAmbientVehicles > 0.0f && fMaxAmbientVehicles > 0.0f)
	{
		// Ratio of vehicles to max possible ambient vehicles
		ambientVehiclePopulationRatio = MIN(static_cast<float>(TotalAmbientCarsOnMap) / MIN(maxPossibleAmbientVehicles, fMaxAmbientVehicles), 1.0f);
	}

	PF_SET(AmbientVehiclePopulationRatio, ambientVehiclePopulationRatio);
#endif
}

#if GTA_REPLAY
void CVehiclePopulation::ProcessReplay()
{
	if (CReplayMgr::IsReplayInControlOfWorld())
	{
		// Update the population shape, and applicable inputs, while replay is active --
		// DistantCars uses it to determine where cars are drawn
		CalculatePopulationRangeScale();

		CPopCtrl populationControlPointInfo = CPedPopulation::GetPopulationControlPointInfo();
		UpdateKeyHoleShape(populationControlPointInfo.m_centre, populationControlPointInfo.m_centreVelocity, populationControlPointInfo.m_baseHeading, populationControlPointInfo.m_FOV, populationControlPointInfo.m_tanHFov, GetPopulationRangeScale());

		if (!s_VehiclePopulationDependencyRunning)
		{
			// Calculate the average link height (used by CalculatePopulationRangeScale())
			StartAsyncUpdateJob();
		}

		ms_bReplayActive = true;
	}
	else if (ms_bReplayActive)
	{
		// Clear the task handle if replay was active last frame --
		// UpdateLinksAfterGather() is not called while replay is active
		ms_bReplayActive = false;
	}
}
#endif // GTA_REPLAY

bool CountsTowardsAmbientPopulation(CVehicle * pVehicle)
{
	const bool bValidType =
		(pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBike() || pVehicle->InheritsFromBoat()) && !pVehicle->GetIsAircraft();

	if(!bValidType ||
		pVehicle->m_nVehicleFlags.bIsThisAParkedCar ||
		pVehicle->PopTypeIsMission() ||
		pVehicle->GetStatus()==STATUS_WRECKED ||
		pVehicle->GetStatus()==STATUS_PLAYER)
		return false;

	if(pVehicle->GetIsInReusePool())
	{
		return false;
	}

	return true;
}

void CVehiclePopulation::UpdateAmbientPopulationData()
{
	ms_numCarsInReusePool = 0;
	ms_stationaryVehicleCenter.ZeroComponents();
	ms_numStationaryAmbientCars = 0;
	ms_numAmbientCarsOnHighway = 0;
	ms_aAmbientVehicleData.ResetCount();
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();
	while (i--)
	{
		CVehicle* pVeh= pool->GetSlot(i);
		if(pVeh)
		{
			if(pVeh->GetIsInReusePool())
			{
				++ms_numCarsInReusePool;
			}
			else
			{
				bool bCountsTowardsAmbientPopulation = CountsTowardsAmbientPopulation(pVeh);
				pVeh->m_nVehicleFlags.bCountsTowardsAmbientPopulation = bCountsTowardsAmbientPopulation;
				if(bCountsTowardsAmbientPopulation)
				{
					CAmbientPopulationVehData &data = ms_aAmbientVehicleData.Append();
					data.m_fHeading = pVeh->GetTransform().GetHeading();
					data.m_Pos = pVeh->GetTransform().GetPosition();
					data.m_SpawnPos = pVeh->m_vecInitialSpawnPosition;
					data.m_modelIndex = pVeh->GetModelIndex();
					data.m_fSpeed = pVeh->GetVelocity().XYMag2();
					if(data.m_fSpeed < 4.0f)
					{
						++ms_numStationaryAmbientCars;
						ms_stationaryVehicleCenter += data.m_Pos;
					}
				}

				if(pVeh->GetIntelligence()->IsOnHighway())
				{
					++ms_numAmbientCarsOnHighway;
				}
			}
		}
	}

	if(ms_numStationaryAmbientCars > 0)
	{
		ms_stationaryVehicleCenter /= ScalarV((float)ms_numStationaryAmbientCars);
	}	

#if 0
	// if we have room in the array (which we usually do, not often do we fill the vehicle pool)
	// add a fake entry for each streamed in model. this is to take non-spawned vehicles into account
	// when trying to figure out the prefered model to spawn
	const CLoadedModelGroup& loadedCars = gPopStreaming.GetAppropriateLoadedCars();
	u32 numLoadedCars = loadedCars.CountMembers();
	while (numLoadedCars > 0 && ms_aAmbientVehicleData.GetCount() < ms_aAmbientVehicleData.GetCapacity())
	{
		u32 mi = loadedCars.GetMember(numLoadedCars - 1);
		if (CModelInfo::IsValidModelInfo(mi))
		{
			CAmbientPopulationVehData &data = ms_aAmbientVehicleData.Append();
			data.m_fHeading = -360.f;
			data.m_Pos.SetXf(-99999.f);
			data.m_Pos.SetYf(-99999.f);
			data.m_Pos.SetZf(-99999.f);
			data.m_SpawnPos.SetXf(-99999.f);
			data.m_SpawnPos.SetYf(-99999.f);
			data.m_SpawnPos.SetZf(-99999.f);
			data.m_modelIndex = mi;
			data.m_fSpeed = 0.0f; 
		}

		numLoadedCars--;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemoveExcessEmptyCopAndWreckedVehs
// PURPOSE :	If there are too many police cars or wrecked cars kicking about we will remove some.
//				This should fix the big pile ups we get during shootouts.
// PARAMETERS :	
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
static const u32	excessiveWreckedOrEmptyCopCars_CheckFrameInterval			= 63;// Make sure this is of the form (2^N - 1).
static const u32	excessiveWreckedOrEmptyCopCars_CheckFrameOfffset			= 40;// Make sure this is less than excessiveUselessCarsCheckFrameInterval.
static const s32	excessiveWreckedOrEmptyCopCars_BaseMaxAllowed				= 10;
static const s32	excessiveWreckedOrEmptyCopCars_MaxAggressiveDespawn			= 7;
static const s32	excessiveWreckedOrEmptyCopCars_MaxReductionPerPlayerInMp	= 0;
static const s32	excessiveWreckedOrEmptyCopCars_SmallestMaxInMp				= 7;
static const s32	excessiveWreckedOrEmptyCopCars_SmallestMaxInMpWithClone		= 7;
static const s32	excessiveWreckedOrEmptyCopCars_MaxIncludingCloneInMp		= 32;
static const u32	excessiveWreckedOrEmptyCopCars_MinRemovalWreckedTimeMs		= 3000;
static const u32	excessiveWreckedOrEmptyCopCars_MinRemovalAbandonedTimeMs	= 3000;

static const float	excessiveWreckedOrEmptyCopCars_RemoteRemovalRange_Wrecked		= 100.0f;
static const float	excessiveWreckedOrEmptyCopCars_RemoteRemovalRange_BigOrObvious	= 175.0f;
static const float	excessiveWreckedOrEmptyCopCars_RemoteRemovalRange_Default		= 140.0f;

static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers	= 20.0f;
static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSq	= square(excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers);
static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_Overloaded	= 0.0f;
static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_OverloadedSq = square(excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_Overloaded);

static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg	= 30.0f;
static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSqAgg	= square(excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg);
static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_Overloaded	= 0.0f;
static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_OverloadedSq = square(excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_Overloaded);

static const float	excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersInLOS	= square(50.0f);

static const float	excessiveWreckedOrEmptyCopCars_AmbientAndCopCarLastDriverTime	= 5000;

void CVehiclePopulation::RemoveExcessEmptyCopAndWreckedVehs()
{
	// Doesn't need to be done frequently at all
	if ((fwTimer::GetSystemFrameCount() & excessiveWreckedOrEmptyCopCars_CheckFrameInterval) != excessiveWreckedOrEmptyCopCars_CheckFrameOfffset)
	{
		return;
	}

	// Go through the pool and count the empty police cars we have.
#define MAX_CARS_TO_STORE	(100)
	s32		numCarsProcessed = 0;
	s32		numCarsToProcessWeCanRemove = 0;
	s32		numClonesToConsider = 0;
	RegdVeh apCars[MAX_CARS_TO_STORE];
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i	= (s32) pool->GetSize();

	Vector3	popCentre = CGameWorld::FindLocalPlayerCoors();

	while (i--)
	{
		CVehicle* pVeh= pool->GetSlot(i);
		if(pVeh && !pVeh->GetIsInReusePool())
		{
			bool bValidForDeletion = false;
			if(!NetworkInterface::IsGameInProgress())
			{
				bValidForDeletion = (!pVeh->PopTypeIsMission() &&
					pVeh->PopTypeGet() != POPTYPE_RANDOM_PARKED &&
					(pVeh->IsLawEnforcementVehicle() || (pVeh->GetStatus() == STATUS_WRECKED) || pVeh->m_nVehicleFlags.bIsDrowning || pVeh->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs) &&
					(!pVeh->HasAlivePedsInIt() || pVeh->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt) &&
					!pVeh->HasMissionCharsInIt());
			}
			else
			{
				const bool bCopOrEmptyAmbient = pVeh->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs || pVeh->IsLawEnforcementVehicle() || (!pVeh->GetLastDriver() || !pVeh->GetLastDriver()->IsAPlayerPed());

				bool bEmptyPopOrCopCar = false;
				if( !pVeh->IsUsingPretendOccupants() && 
					((NetworkInterface::GetSyncedTimeInMilliseconds() - pVeh->m_LastTimeWeHadADriver) > excessiveWreckedOrEmptyCopCars_AmbientAndCopCarLastDriverTime ) && bCopOrEmptyAmbient )
				{
					bEmptyPopOrCopCar = true;
				}

				const bool bWrecked = pVeh->GetStatus() == STATUS_WRECKED || pVeh->m_nVehicleFlags.bIsDrowning;

				bValidForDeletion = !pVeh->PopTypeIsMission() &&
					( pVeh->PopTypeGet() != POPTYPE_RANDOM_PARKED || bWrecked ) &&
					(bEmptyPopOrCopCar || bWrecked) &&
					(!pVeh->HasAlivePedsInIt() || pVeh->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt) &&
					!pVeh->HasMissionCharsInIt();
			}

			if(bValidForDeletion)
			{			
				if(pVeh->CanBeDeleted())
				{
					if (numCarsToProcessWeCanRemove < MAX_CARS_TO_STORE)
					{
						apCars[numCarsToProcessWeCanRemove++] = pVeh;
#if __BANK
						if(ms_bDrawMPExcessiveDebug)
						{
							grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()) + ZAXIS, 0.5f, Color_yellow, true, 60);
						}
#endif
					}
				}
				else if(NetworkInterface::IsGameInProgress() && pVeh->IsNetworkClone() && pVeh->GetNetworkObject()) // Consider clone vehicles to be deleted too
				{
					// We must consider the whole map as the max defined in MP is MAX_NUM_NETOBJVEHICLES
					/*Vector3 diff = CGameWorld::FindLocalPlayerCoors() - VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());*/
					if(/*diff.Mag2() < excessiveWreckedOrEmptyCopCars_MaxDistForConsiderCloneVehicles && */pVeh->CanBeDeletedSpecial(true, false))
					{
						numClonesToConsider++;

#if __BANK
						if(ms_bDrawMPExcessiveDebug)
						{
							grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()) + ZAXIS, 0.5f, Color_red, true, 60);
						}
#endif
					}
				}
				numCarsProcessed++;
			}
		}
	}

	s32 iMaxEmptyCars = excessiveWreckedOrEmptyCopCars_BaseMaxAllowed;
	if( NetworkInterface::IsGameInProgress() )
	{
		const s32 iNumNetworkPlayers = NetworkInterface::GetNumActivePlayers();
		iMaxEmptyCars -= iNumNetworkPlayers * excessiveWreckedOrEmptyCopCars_MaxReductionPerPlayerInMp;
		if(numClonesToConsider + numCarsToProcessWeCanRemove > excessiveWreckedOrEmptyCopCars_MaxIncludingCloneInMp)
		{
			if(iMaxEmptyCars < excessiveWreckedOrEmptyCopCars_SmallestMaxInMpWithClone)
			{
				iMaxEmptyCars = excessiveWreckedOrEmptyCopCars_SmallestMaxInMpWithClone;
			}
		}
		else if(iMaxEmptyCars < excessiveWreckedOrEmptyCopCars_SmallestMaxInMp)
		{
			iMaxEmptyCars = excessiveWreckedOrEmptyCopCars_SmallestMaxInMp;
		}
	}

	ms_iNumWreckedEmptyCars = numCarsToProcessWeCanRemove;

#if __BANK

	if(ms_bDrawMPExcessiveDebug)
	{
		char Buff[128];
		sprintf(Buff, "Cars: %d, Clones: %d, MaxEmptyCars: %d", numCarsToProcessWeCanRemove, numClonesToConsider, iMaxEmptyCars);
		grcDebugDraw::Text(popCentre + ZAXIS, Color_yellow, 0, 0, Buff, true, 60);
	}

#endif

	//Ensure we have cars we can possibly remove.
	s32 iMaxCarsToRemoveForPassiveDespawn = (numCarsProcessed - iMaxEmptyCars);
	s32 iMaxCarsToRemoveForAggressiveDespawn = (numCarsProcessed - excessiveWreckedOrEmptyCopCars_MaxAggressiveDespawn);
	if((iMaxCarsToRemoveForPassiveDespawn <= 0) && (iMaxCarsToRemoveForAggressiveDespawn <= 0))
	{
		return;
	}

	//first move all visible cars to back of list
	s32 firstVisibleIndex = numCarsToProcessWeCanRemove - 1;
	for (s32 h = numCarsToProcessWeCanRemove - 1; h >= 0; --h)
	{			
		RegdVeh	pTempVeh(apCars[h]);
		bool bIsVisible = pTempVeh->GetIsVisibleInSomeViewportThisFrame();

		//Ensure the vehicle is not visible to any remote players.
		if(!bIsVisible && NetworkInterface::IsGameInProgress())
		{
			float fRadius = pTempVeh->GetBoundRadius();
			bIsVisible = NetworkInterface::IsVisibleToAnyRemotePlayer(VEC3V_TO_VECTOR3(pTempVeh->GetTransform().GetPosition()), fRadius, 100.0f);
		}

		if(bIsVisible)
		{
			if(h != firstVisibleIndex)
			{
				RegdVeh	pTempVeh(apCars[h]);
				apCars[h] = apCars[firstVisibleIndex];
				apCars[firstVisibleIndex] = pTempVeh;
			}

			--firstVisibleIndex;
		}
	}

	s32 numVisibleCars = numCarsToProcessWeCanRemove - (firstVisibleIndex+1);

	float	aDistSq[MAX_CARS_TO_STORE];
	for (s32 t = 0; t < numCarsToProcessWeCanRemove; t++)
	{
		aDistSq[t] = (popCentre - VEC3V_TO_VECTOR3(apCars[t]->GetTransform().GetPosition())).Mag2();
		//add 10 meters to big vehicles as they're more noticable when removed
		if (apCars[t]->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) )
		{
			aDistSq[t] -= 100.0f;
		}
	}

	// Sort the hidden cars, furthest first
	bool bChange = true;
	while (bChange)
	{
		bChange = false;

		for (s32 h = 0; h <= firstVisibleIndex - 1; h++)
		{
			bool bSort;
			if(NetworkInterface::IsGameInProgress())
			{
				bSort = (aDistSq[h] < aDistSq[h+1]);
			}
			else
			{
				// prioritize wrecked vehicles over abandoned police cars, and also sort cars with the same status by distance
				s32 status1 = apCars[h]->GetStatus();
				s32 status2 = apCars[h+1]->GetStatus();
				bSort = ((aDistSq[h] < aDistSq[h+1]) && (status1 == status2)) || (status1 != STATUS_WRECKED && status2 == STATUS_WRECKED);
			}

			if (bSort)
			{
				float		tempDist = aDistSq[h];
				RegdVeh		pTempVeh(apCars[h]);
	
				aDistSq[h] = aDistSq[h+1];
				apCars[h] = apCars[h+1];
				aDistSq[h+1] = tempDist;
				apCars[h+1] = pTempVeh;
				bChange = true;
			}
		}
	}

	//Calculate the number of cars to remove without visibility checks.
	static dev_u32 s_uMaxEmptyCarsToUseVisibilityChecksInSP = 16;
	static dev_u32 s_uMaxEmptyCarsToUseVisibilityChecksInMP = 16;

	static dev_u32 s_uMinConsideredCarsBeforeAdjustRangeInMP = 6;
	static dev_u32 s_uMaxConsideredCarsBeforeAdjustRangeInMP = 16;
	static dev_u32 s_uMinConsideredCarsBeforeAdjustRangeInMP_Visible = 16;
	static dev_u32 s_uMaxConsideredCarsBeforeAdjustRangeInMP_Visible = 20;

	u32 iMaxEmptyCarsToUseVisibilityChecks = !NetworkInterface::IsGameInProgress() ? s_uMaxEmptyCarsToUseVisibilityChecksInSP : s_uMaxEmptyCarsToUseVisibilityChecksInMP;
	s32 iMaxCarsToRemoveWithoutVisibilityChecks = (numCarsProcessed - iMaxEmptyCarsToUseVisibilityChecks);

	float fRangeReductionScaler;
	float fRangeReductionScaler_Visible;

	if(NetworkInterface::IsGameInProgress())
	{
		u32 iMinNumCarsConsideredBeforeReduceRange = s_uMinConsideredCarsBeforeAdjustRangeInMP;
		u32 iMaxNumCarsConsideredBeforeReduceRange = s_uMaxConsideredCarsBeforeAdjustRangeInMP;
		u32 iMinNumCarsConsideredBeforeReduceRange_Visible = s_uMinConsideredCarsBeforeAdjustRangeInMP_Visible;
		u32 iMaxNumCarsConsideredBeforeReduceRange_Visible = s_uMaxConsideredCarsBeforeAdjustRangeInMP_Visible;

		fRangeReductionScaler = ((float)( numCarsToProcessWeCanRemove - iMinNumCarsConsideredBeforeReduceRange )) /  ((float) (iMaxNumCarsConsideredBeforeReduceRange - iMinNumCarsConsideredBeforeReduceRange));
		fRangeReductionScaler = Clamp(fRangeReductionScaler, 0.0f, 1.0f);

		fRangeReductionScaler_Visible = ((float)( numCarsToProcessWeCanRemove - iMinNumCarsConsideredBeforeReduceRange_Visible )) /  ((float) (iMaxNumCarsConsideredBeforeReduceRange_Visible - iMinNumCarsConsideredBeforeReduceRange_Visible));
		fRangeReductionScaler_Visible = Clamp(fRangeReductionScaler_Visible, 0.0f, 1.0f);
	}
	else
	{
		fRangeReductionScaler = 0.0f;
		fRangeReductionScaler_Visible = 0.0f;
	}

	const float fAggressiveMinRemovalDist = Lerp(fRangeReductionScaler, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSqAgg, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_OverloadedSq);
	const float fPassiveMinRemovalDist = Lerp(fRangeReductionScaler, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSq, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_OverloadedSq);

	const float fAggressiveMinRemovalDist_Visible = Lerp(fRangeReductionScaler_Visible, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSqAgg, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_OverloadedSq);
	const float fPassiveMinRemovalDist_Visible = Lerp(fRangeReductionScaler_Visible, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSq, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_OverloadedSq);

	if( (NetworkInterface::IsGameInProgress() && numVisibleCars > 1) || (iMaxCarsToRemoveWithoutVisibilityChecks > 0) )
	{
		//! Re-evaluate dist based on all players.
		if (NetworkInterface::IsGameInProgress())
		{
			for (s32 h = firstVisibleIndex + 1; h < numCarsToProcessWeCanRemove; h++)
			{
				float fBestDistSq = FLT_MAX;
				bool bFoundBestDist = false;

				RegdVeh		pTempVeh(apCars[h]);

				unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
				const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

				for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
				{
					const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
					if(remotePlayer && remotePlayer->GetPlayerPed())
					{
						float fRadius = pTempVeh->GetBoundRadius();
						bool bIsVisible = NetworkInterface::IsVisibleToPlayer(remotePlayer, VEC3V_TO_VECTOR3(pTempVeh->GetTransform().GetPosition()), fRadius, 100.0f);
						if(bIsVisible)
						{
							Vector3 diff = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - pTempVeh->GetTransform().GetPosition());
							float fDistSq = diff.Mag2();
							if(fDistSq < fBestDistSq)
							{
								fBestDistSq = fDistSq;

								//add 10 meters to big vehicles as they're more noticable when removed
								if (apCars[h]->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) )
								{
									fBestDistSq -= 100.0f;
								}

								bFoundBestDist = true;
							}
						}
					}
				}

				//check local.
				bool bIsVisible = pTempVeh->GetIsVisibleInSomeViewportThisFrame();
				if(bIsVisible)
				{
					if(aDistSq[h] < fBestDistSq)
					{
						fBestDistSq = aDistSq[h];
						bFoundBestDist = true;
					}
				}

				if(bFoundBestDist)
				{
					aDistSq[h] = fBestDistSq;
				}
			}
		}

		//it's got to the stage that we're happy removing visible vehicles, so sort them furthest first
		bool bChange = true;
		while (bChange)
		{
			bChange = false;

			for (s32 h = firstVisibleIndex + 1; h < numCarsToProcessWeCanRemove - 1; h++)
			{
				bool bSort;
				if(NetworkInterface::IsGameInProgress())
				{
					bSort = (aDistSq[h] < aDistSq[h+1]);
				}
				else
				{
					// prioritize wrecked vehicles over abandoned police cars, and also sort cars with the same status by distance
					s32 status1 = apCars[h]->GetStatus();
					s32 status2 = apCars[h+1]->GetStatus();
					bSort = ((aDistSq[h] < aDistSq[h+1]) && (status1 == status2)) || (status1 != STATUS_WRECKED && status2 == STATUS_WRECKED);
				}

				if (bSort)
				{
					float		tempDist = aDistSq[h];
					RegdVeh		pTempVeh(apCars[h]);

					aDistSq[h] = aDistSq[h+1];
					apCars[h] = apCars[h+1];
					aDistSq[h+1] = tempDist;
					apCars[h+1] = pTempVeh;
					bChange = true;
				}
			}
		}
	}

#if __BANK    
	if(ms_bDrawMPExcessiveDebug)
	{
		for( s32 iRender = 0; iRender < numCarsToProcessWeCanRemove; iRender++ )
		{
			CVehicle *pRemovalVeh = apCars[iRender];
			if (pRemovalVeh)
			{
				const bool bVisible = iRender > firstVisibleIndex;
				char Buff[128];
				sprintf(Buff, "%d[%.2f]%s", iRender, rage::Sqrtf(aDistSq[iRender]) , bVisible ? "(V)" : "" );
				grcDebugDraw::Text(pRemovalVeh->GetTransform().GetPosition() + VECTOR3_TO_VEC3V(ZAXIS), Color_blue, 0, 0, Buff, true, -60);
			}
		}
	}
#endif


	//we now have a sorted list of cars. All visible cars are at the end, and visible and non-visible groups are both sorted by distance, furthest first.

	s32 iCarsRemoved = 0;
	s32 carToConsider = 0;
	while (carToConsider < numCarsToProcessWeCanRemove)
	{
		CVehicle *pRemovalVeh = apCars[carToConsider];
		if (!pRemovalVeh)
		{
			carToConsider++;
			continue;
		}

		bool bCanUsePassiveDespawn = (iCarsRemoved < iMaxCarsToRemoveForPassiveDespawn);
		bool bCanUseAggressiveDespawn = (iCarsRemoved < iMaxCarsToRemoveForAggressiveDespawn);
		if(!bCanUsePassiveDespawn && !bCanUseAggressiveDespawn)
		{
			break;
		}

		//if carToConsider > firstVisibleIndex we only have visible vehicles left
		//if we're still happy not removing visible vehicles then we can break out now
		bool bPreventVisibleCarRemoval = (iCarsRemoved >= iMaxCarsToRemoveWithoutVisibilityChecks);
		if(bPreventVisibleCarRemoval && carToConsider > firstVisibleIndex)
		{			
			break;
		}

		float fDistSq = aDistSq[carToConsider];
		carToConsider++;

		bool bVisible = carToConsider > firstVisibleIndex;

		float fPassiveRejectionDist = bVisible ? fPassiveMinRemovalDist_Visible : fPassiveMinRemovalDist;
		float fAggressiveRejectionDist = bVisible ? fAggressiveMinRemovalDist_Visible : fAggressiveMinRemovalDist;

		bool bRemove = false;
		bool bRemoveAsPassive = false;
		if(!NetworkInterface::IsGameInProgress() || (carToConsider > firstVisibleIndex) )
		{
			if(bCanUsePassiveDespawn && ShouldEmptyCopOrWreckedVehBeRemovedPassively(*pRemovalVeh, fDistSq, fPassiveRejectionDist))
			{
				bRemove = true;
				bRemoveAsPassive = true;
			}
			else if(bCanUseAggressiveDespawn && ShouldEmptyCopOrWreckedVehBeRemovedAggressively(*pRemovalVeh, fDistSq, fAggressiveRejectionDist))
			{
				bRemove = true;
			}
		}
		else
		{
			if(bCanUsePassiveDespawn && fDistSq > fPassiveRejectionDist)
			{
				bRemove = true;
				bRemoveAsPassive = true;
			}

			if(!bCanUseAggressiveDespawn && fDistSq > fAggressiveRejectionDist)
			{
				bRemove = true;
			}
		}

		//Need to evaluate distances from other players in MP game - otherwise the distance checks will pass on one player and not consider another player and
		//remove the car in front of them. lavalley.
		if (NetworkInterface::IsGameInProgress() && bRemove)
		{
			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
			const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

			for(unsigned index = 0; (bRemove && (index < numRemotePhysicalPlayers)); index++)
			{
				const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
				if(remotePlayer && remotePlayer->GetPlayerPed())
				{
					Vector3 diff = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - pRemovalVeh->GetTransform().GetPosition());
					fDistSq = diff.Mag2();

					// We should use the same criteria / slot as for the player (only check distance, not whole function as only distance will have changed)
					if(bRemoveAsPassive && fDistSq < fPassiveRejectionDist)
					{
						bRemove = false;
					}

					if(!bRemoveAsPassive && fDistSq < fAggressiveRejectionDist)
					{
						bRemove = false;
					}
				}
			}
		}
		
		if(bRemove)
		{
			BANK_ONLY(ms_lastRemovalReason = Removal_EmptyCopOrWrecked);
			vehPopDebugf2("0x8%p - Remove vehicle, empty cop and wrecked car. Not close to any player.", pRemovalVeh);
			RemoveVeh(pRemovalVeh, true, Removal_EmptyCopOrWrecked);
			++iCarsRemoved;
		}
	}
}

bool CVehiclePopulation::IsEntityVisibleToAnyPlayer(const CDynamicEntity *pEntity, const CPed *pIgnorePed, float fDistance)
{
	if((CGameWorld::FindLocalPlayer() != pIgnorePed) && pEntity->GetIsVisibleInSomeViewportThisFrame())
	{
		return true;
	}

	return IsEntityVisibleToRemotePlayer(pEntity, pIgnorePed, fDistance);
}

bool CVehiclePopulation::IsEntityVisibleToRemotePlayer(const CEntity *pEntity, const CPed *pIgnorePed, float fDistance)
{
	TUNE_GROUP_BOOL(VEHICLE_POPULATION, FORCE_VISIBLE_TO_REMOTE_PLAYERS, false);
	if(FORCE_VISIBLE_TO_REMOTE_PLAYERS)
	{
		return true;
	}

	if(NetworkInterface::IsGameInProgress())
	{
		unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
			if(remotePlayer && remotePlayer->GetPlayerPed() && (remotePlayer->GetPlayerPed() != pIgnorePed) )
			{
				float fRadius = pEntity->GetBoundRadius();
				bool bIsVisible = NetworkInterface::IsVisibleToPlayer(remotePlayer, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()), fRadius, fDistance);
				if(bIsVisible)
				{
					return true;
				}
			}
		}
	}

	return false;
}

float CVehiclePopulation::GetDistanceFromAllPlayers(const Vector3 &popCentre, CEntity *pEntity, CPed *pIgnorePed)
{
	bool bIgnoreLocal = pIgnorePed && (pIgnorePed == CGameWorld::FindLocalPlayer());

	Vector3 vLocaldiff = popCentre - VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	float fDistLocal = vLocaldiff.Mag();

	float fBestDist = bIgnoreLocal ? FLT_MAX : fDistLocal;

	//! Get the closest distance for all network players, not just local distance. This ensures that we don't delete visible vehicles that are close
	//! to remote players too.
	if(NetworkInterface::IsGameInProgress())
	{
		unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();

		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
			if(remotePlayer && remotePlayer->GetPlayerPed() && (remotePlayer->GetPlayerPed() != pIgnorePed) )
			{
				Vector3 vDiffRemote = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - pEntity->GetTransform().GetPosition());
				float fDistRemote = vDiffRemote.Mag();
				if(fDistRemote < fBestDist)
				{
					fBestDist = fDistRemote;
				}
			}
		}
	}

	return fBestDist;
}

bool IsIntactParkedCar(CVehicle* pVeh)
{
	const bool bWrecked = pVeh->GetStatus() == STATUS_WRECKED || pVeh->m_nVehicleFlags.bIsDrowning;
	const bool bIntactParkedCar = (pVeh->PopTypeGet() == POPTYPE_RANDOM_PARKED) && !bWrecked;
	return bIntactParkedCar;
}

bool CVehiclePopulation::IsVehicleWreckedOrEmpty(CVehicle *pVeh, float fDist, bool bRespawnDeletion)
{
	if(pVeh->PopTypeIsMission() || pVeh->HasMissionCharsInIt())
	{
		return false;
	}

	if(!pVeh->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt)
	{
		if(bRespawnDeletion)
		{
			//! on respawn, delete vehicles without a driver and all law enforcement vehicles.
			if(pVeh->GetDriver() && !pVeh->GetDriver()->IsInjured() && !pVeh->IsLawEnforcementVehicle())
			{
				return false;
			}
		}
		else
		{
			if(pVeh->HasAlivePedsInIt())
			{
				return false;
			}
		}
	}

	//don't include child vehicles or trailers - the parent will control removal if valid
	if(pVeh->m_nVehicleFlags.bHasParentVehicle || pVeh->InheritsFromTrailer())
	{
		return false;
	}

	if(pVeh->GetIsInReusePool())
	{
		return false;
	}

	const bool bIntactParkedCar = IsIntactParkedCar(pVeh);
	
	TUNE_GROUP_BOOL(VEHICLE_POPULATION, FORCE_CONSIDER_AMBIENT_VEHS, false);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, PARKED_CAR_DELETION_DISTANCE, 150.0f, 0.0f, 300.0f, 1.0f);

	const bool bConsiderEmptyAmbientVehs = (NetworkInterface::IsGameInProgress() || FORCE_CONSIDER_AMBIENT_VEHS || bRespawnDeletion);

	if(bIntactParkedCar)
	{
		if(bConsiderEmptyAmbientVehs && !bRespawnDeletion)
		{
			if( rage::square(fDist) < rage::square(PARKED_CAR_DELETION_DISTANCE))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	const bool bEmptyAmbient = !pVeh->IsUsingPretendOccupants() && 
		(pVeh->m_nVehicleFlags.bRemoveWithEmptyCopOrWreckedVehs || 
		pVeh->IsLawEnforcementVehicle() || 
		(bConsiderEmptyAmbientVehs && (!pVeh->GetLastDriver() || !pVeh->GetLastDriver()->IsAPlayerPed())));

	bool bCanDeleteEmptyAmbient = false;
	if( bEmptyAmbient && ( ((NetworkInterface::GetSyncedTimeInMilliseconds() - pVeh->m_LastTimeWeHadADriver) > excessiveWreckedOrEmptyCopCars_AmbientAndCopCarLastDriverTime ) || bRespawnDeletion))
	{
		bCanDeleteEmptyAmbient = true;
	}

	const bool bWrecked = pVeh->GetStatus() == STATUS_WRECKED || pVeh->m_nVehicleFlags.bIsDrowning;
	return bCanDeleteEmptyAmbient || bWrecked;
}

float CVehiclePopulation::GetDeletionScoreForVehicle(tDeletionEntry &entry, float fPassiveRejectionDist, float fAggressiveRejectionDist)
{
	//! Note: The higher the score, the less likely the vehicle will be deleted.

	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_VISIBLE_VEHS, 1500.0f, 0.0f, 99999.0f, 1.0f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_INTACT_PARKED_VEHS, 750.0f, 0.0f, 99999.0f, 1.0f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_INTACT_VEHS, 100.0f, 0.0f, 99999.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_INTACT_AIRCRAFT, 650.0f, 0.0f, 99999.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_AGGRESSIVE_DELETE, 400.f, 0.0f, 99999.0f, 1.01f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_PASSIVE_DELETE, 200.f, 0.0f, 99999.0f, 1.01f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_LARGE_VEHICLE, 75.0f, 0.0f, 99999.0f, 1.01f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, REMOVAL_MIN_SCORE_FOR_NEARBY_COMBAT_PEDS, 100.f, 0.0f, 99999.0f, 1.01f);
	TUNE_GROUP_BOOL(VEHICLE_POPULATION, FORCE_ALL_DELETION_SCORES_TO_ZERO, false);

	float fScore = -1.0f;

	entry.bPassive = ShouldEmptyCopOrWreckedVehBeRemovedPassively(*entry.pVehicle, rage::square(entry.fDist), fPassiveRejectionDist);
	entry.bAggressive = ShouldEmptyCopOrWreckedVehBeRemovedAggressively(*entry.pVehicle, rage::square(entry.fDist), fAggressiveRejectionDist);

	//! Only allow adding to the list if we are able to delete it. 
	//! Note: We always allow deleting hidden vehicles. If would be daft to delete a visible vehicle over a hidden one behind us.
	if(!FORCE_ALL_DELETION_SCORES_TO_ZERO && (!entry.IsVisible() || entry.bPassive || entry.bAggressive))
	{
		fScore = 0.0f;

		if(entry.pVehicle->GetStatus() != STATUS_WRECKED)
		{
			//! Try not to delete non wrecked aircraft.
			if(entry.pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
			{
				fScore += REMOVAL_MIN_SCORE_FOR_INTACT_PARKED_VEHS;
			}
			else if(entry.pVehicle->GetIsAircraft())
			{
				fScore += REMOVAL_MIN_SCORE_FOR_INTACT_AIRCRAFT;
			}
			else
			{
				fScore += REMOVAL_MIN_SCORE_FOR_INTACT_VEHS;
			}
		}

		if(!entry.bAggressive)
		{
			fScore += REMOVAL_MIN_SCORE_FOR_AGGRESSIVE_DELETE;
		}

		if(!entry.bPassive)
		{
			fScore += REMOVAL_MIN_SCORE_FOR_PASSIVE_DELETE;
		}

		if(entry.IsVisible())
		{
			fScore += REMOVAL_MIN_SCORE_FOR_VISIBLE_VEHS;
		}

		//! we want to penalize close distance, so create a score out of this by inverting.
		static dev_float s_MaxDist = 500.0f;
		static dev_float s_DistScoreMultiplier = 100.0f;
		float fDistScore = (1.0f - (entry.fDist / s_MaxDist)) * s_DistScoreMultiplier;

		//! Make big vehicles harder to delete.
		if (entry.pVehicle->GetVehicleModelInfo() && entry.pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) )
		{
			fScore += REMOVAL_MIN_SCORE_FOR_LARGE_VEHICLE;
		}

		//! See if we have any combat peds nearby. If so, don't delete as we may be using vehicle for cover.
		if(entry.pVehicle->GetStatus() != STATUS_WRECKED)
		{
			const CEntityScannerIterator entityList = entry.pVehicle->GetIntelligence()->GetPedScanner().GetIterator();
			for(const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
			{
				const CPed* pNearbyPed=(const CPed*)pEntity;
				if( DistSquared(pNearbyPed->GetTransform().GetPosition(), pEntity->GetTransform().GetPosition()).Getf() < rage::square(10.0f) )
				{	
					if( pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COMBAT ) )
					{
						fScore += REMOVAL_MIN_SCORE_FOR_NEARBY_COMBAT_PEDS;
						break;
					}
				}
			}
		}

		fScore += fDistScore;
	}

	return fScore;
}

#if __BANK
void CVehiclePopulation::DumpExcessEmptyCopAndWreckedListToLog(atFixedArray<tDeletionEntry, MAX_CARS_TO_STORE_MP_DELETE> &apCars)
{
	if(apCars.GetCount() > 0)
	{
		Displayf("--------------------------------------------------------------------------------");
		Displayf("CVehiclePopulation::DumpExcessEmptyCopAndWreckedListToLog - Count = %d", apCars.GetCount());
		for(int carToConsider = 0; carToConsider < apCars.GetCount(); ++carToConsider)
		{
			const char *pName = NULL;
			netObject *networkObject = apCars[carToConsider].pVehicle->GetNetworkObject();

			if(networkObject)
			{
				pName = networkObject->GetLogName();
			}
			else
			{
				pName = apCars[carToConsider].pVehicle->GetDebugName();
			}

			Displayf("[Index=%d]", carToConsider);
			Displayf("	Name=%s", pName);
			Displayf("	fDist=%.2f", apCars[carToConsider].fDist);
			Displayf("	fScore=%.2f", apCars[carToConsider].fScore);
			Displayf("	bLocalVisible=%s", apCars[carToConsider].bLocalVisible ? "true" : "false");
			Displayf("	bRemoteVisible=%s", apCars[carToConsider].bRemoteVisible ? "true" : "false");
			Displayf("	bPassive=%s", apCars[carToConsider].bPassive ? "true" : "false");
			Displayf("	bAggressive=%s", apCars[carToConsider].bAggressive ? "true" : "false");
			Displayf("	IsNetworkClone=%s", apCars[carToConsider].pVehicle->IsNetworkClone() ? "true" : "false");
			Displayf("	Status=%d", apCars[carToConsider].pVehicle->GetStatus());
			Displayf("	PopType=%d", apCars[carToConsider].pVehicle->PopTypeGet());

			Vector3 vPosition = VEC3V_TO_VECTOR3(apCars[carToConsider].pVehicle->GetTransform().GetPosition());
			Displayf("	Position = %f:%f:%f", vPosition.x, vPosition.y, vPosition.z);
		}
		Displayf("--------------------------------------------------------------------------------");
	}
}

//collates all vehicle counts (matches UpdateVehCount)
//and asserts on any counts that don't match
void CVehiclePopulation::ValidateVehicleCounts()
{
	s32 iNumPermanentVehicles = 0;
	s32 iNumParkedCars = 0;
	s32 iNumRandomCars = 0;
	s32 iNumParkedMissionCars = 0;
	s32 iNumMissionCars = 0;
	s32 iNumInReusePool = 0;
	s32 iTotalCount = 0;
	s32 iNumMisc = 0;

	s32 i = (s32) CVehicle::GetPool()->GetSize();
	while(i--)
	{
		CVehicle * pVeh = CVehicle::GetPool()->GetSlot(i);
		if (pVeh)
		{
			++iTotalCount;

			if(pVeh->GetIsInReusePool())
			{
				iNumInReusePool++;
				continue;
			}
			switch(pVeh->PopTypeGet())
			{
			case POPTYPE_UNKNOWN:
				Assertf(false, "The vehicle should have a valid pop type.");
				break;
			case POPTYPE_RANDOM_PERMANENT:
				++iNumPermanentVehicles;
				break;
			case POPTYPE_RANDOM_PARKED:
				++iNumParkedCars;
				break;
			case POPTYPE_RANDOM_PATROL:
			case POPTYPE_RANDOM_SCENARIO :
			case POPTYPE_RANDOM_AMBIENT :
				++iNumRandomCars;
				break;
			case POPTYPE_PERMANENT :
				++iNumPermanentVehicles;
				break;
			case POPTYPE_MISSION :
			case POPTYPE_REPLAY :
				if (pVeh->m_nVehicleFlags.bCountAsParkedCarForPopulation)
				{
					++iNumParkedMissionCars;
				}
				else
				{
					++iNumMissionCars;
				}
				break;
			case POPTYPE_TOOL:
			case POPTYPE_CACHE:
				++iNumMisc;
				break;
			case NUM_POPTYPES :
				Assert(0);
			}
		}
	}

#if __ASSERT
	Assertf(iNumPermanentVehicles + iNumParkedCars + iNumRandomCars + iNumParkedMissionCars + iNumMissionCars + iNumInReusePool + iNumMisc == iTotalCount,
		"Vehicle counts don't match %d %d %d %d %d %d %d != %d", iNumPermanentVehicles, iNumParkedCars, iNumRandomCars,
		iNumParkedMissionCars, iNumMissionCars, iNumInReusePool, iNumMisc, iTotalCount);
	Assertf(iNumParkedCars == ms_numParkedCars, "Num parked car mismatch %d %d", iNumParkedCars, ms_numParkedCars);
	Assertf(iNumRandomCars == ms_numRandomCars, "Num random car mismatch %d %d", iNumRandomCars, ms_numRandomCars);
	Assertf(iNumPermanentVehicles == ms_numPermanentVehicles, "Num permanent car mismatch %d %d", iNumPermanentVehicles, ms_numPermanentVehicles);
	Assertf(iNumParkedMissionCars == ms_numParkedMissionCars, "Num parked mission car mismatch %d %d", iNumParkedMissionCars, ms_numParkedMissionCars);
	Assertf(iNumMissionCars == ms_numMissionCars, "Num mission car mismatch %d %d", iNumMissionCars, ms_numMissionCars);
	Assertf(iNumInReusePool == ms_vehicleReusePool.GetCount(), "Num reuse car mismatch %d %d", iNumInReusePool, ms_vehicleReusePool.GetCount());
	Assertf(iTotalCount == (GetTotalVehsOnMap() + iNumInReusePool), "Num total car mismatch %d %d", iTotalCount, GetTotalVehsOnMap() + iNumInReusePool);
#else
	if(iNumPermanentVehicles + iNumParkedCars + iNumRandomCars + iNumParkedMissionCars + iNumMissionCars + iNumInReusePool + iNumMisc != iTotalCount)
		vehPopDebugf3("Vehicle counts don't match %d %d %d %d %d %d %d != %d", iNumPermanentVehicles, iNumParkedCars, iNumRandomCars,
		iNumParkedMissionCars, iNumMissionCars, iNumInReusePool, iNumMisc, iTotalCount);
	if(iNumParkedCars != ms_numParkedCars) vehPopDebugf3("Num parked car mismatch %d %d", iNumParkedCars, ms_numParkedCars);
	if(iNumRandomCars != ms_numRandomCars) vehPopDebugf3("Num random car mismatch %d %d", iNumRandomCars, ms_numRandomCars);
	if(iNumPermanentVehicles != ms_numPermanentVehicles) vehPopDebugf3("Num permanent car mismatch %d %d", iNumPermanentVehicles, ms_numPermanentVehicles);
	if(iNumParkedMissionCars != ms_numParkedMissionCars) vehPopDebugf3("Num parked mission car mismatch %d %d", iNumParkedMissionCars, ms_numParkedMissionCars);
	if(iNumMissionCars != ms_numMissionCars) vehPopDebugf3("Num mission car mismatch %d %d", iNumMissionCars, ms_numMissionCars);
	if(iNumInReusePool != ms_vehicleReusePool.GetCount()) vehPopDebugf3("Num reuse car mismatch %d %d", iNumInReusePool, ms_vehicleReusePool.GetCount());
	if(iTotalCount != (GetTotalVehsOnMap() + iNumInReusePool)) vehPopDebugf3("Num total car mismatch %d %d", iTotalCount, GetTotalVehsOnMap() + iNumInReusePool);
#endif
}

void CVehiclePopulation::DumpVehiclePoolToLog()
{
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 poolIndex	= (s32) pool->GetSize();

	Displayf("--------------------------------------------------------------------------------");
	Displayf("CVehiclePopulation::DumpVehiclePoolToLog - Used = %d", (int)pool->GetNoOfUsedSpaces());

	s32 iNumberNetRegistered = 0;
	s32 iNumberReuse = 0;
	s32 iNumberBeingDeleted = 0;
	s32 iNumberMisc = 0;
	s32 iNumberParked = 0;
	s32 iNumberScript = 0;
	s32 iNumberRandom = 0;
	s32 iNumberClones = 0;
	s32 i = (s32) CVehicle::GetPool()->GetSize();
	while(i--)
	{
		CVehicle * pVeh = CVehicle::GetPool()->GetSlot(i);
		if (pVeh)
		{
			if(pVeh->GetIsInReusePool())
			{
				iNumberReuse++;
				continue;
			}

			if(pVeh->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				iNumberBeingDeleted++;
			}

			if(pVeh->GetNetworkObject())
			{
				iNumberNetRegistered++;
			}

			if(pVeh->IsNetworkClone())
			{
				iNumberClones++;
			}

			switch(pVeh->PopTypeGet())
			{
			case POPTYPE_UNKNOWN:
				Assertf(false, "The vehicle should have a valid pop type.");
				break;
			case POPTYPE_PERMANENT:
			case POPTYPE_REPLAY:
			case POPTYPE_CACHE:
			case POPTYPE_TOOL:
				++iNumberMisc;
				break;
			case POPTYPE_RANDOM_PARKED :
				++iNumberParked;
				break;
			case POPTYPE_MISSION :
				++iNumberScript;
				break;
			case POPTYPE_RANDOM_PERMANENT:
			case POPTYPE_RANDOM_PATROL : // Purposeful fall through.
			case POPTYPE_RANDOM_SCENARIO : // Purposeful fall through.
			case POPTYPE_RANDOM_AMBIENT :
				++iNumberRandom;
				break;				
			default:
				break;
			}
		}
	}

	s32 nNumDesiredVehicles = static_cast<s32>(GetDesiredNumberAmbientVehicles());

	Displayf("nNumDesiredVehicles: %d iNumberNetRegistered: %d iNumberClones: %d iNumberReuse: %d iNumberBeingDeleted: %d iNumberMisc: %d iNumberParked: %d iNumberScript: %d iNumberRandom: %d", 
		nNumDesiredVehicles, iNumberNetRegistered, iNumberClones, iNumberReuse, iNumberBeingDeleted, iNumberMisc, iNumberParked, iNumberScript, iNumberRandom);

	while (poolIndex--)
	{
		CVehicle* pVeh= pool->GetSlot(poolIndex);
		if(pVeh)
		{
			netObject *networkObject = pVeh->GetNetworkObject();
			const char *pName = networkObject ? networkObject->GetLogName() : pVeh->GetDebugName();
			Vector3 vPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());

			Displayf("name: %s pop type: %d reuse: %s deleted: %s net: %s time created: %d pos = %f:%f:%f",
				pName, 
				pVeh->PopTypeGet(), 
				pVeh->GetIsInReusePool() ? "T" : "F",
				pVeh->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) ? "T" : "F",
				networkObject ? "T" : "F",
				pVeh->m_TimeOfCreation,
				vPosition.x, vPosition.y, vPosition.z
				);
		}
	}

	Displayf("--------------------------------------------------------------------------------");
}
#endif

int CVehiclePopulation::tDeletionEntry::Sort(const CVehiclePopulation::tDeletionEntry *a, const CVehiclePopulation::tDeletionEntry *b) 
{ 
	int iCmp;
	if(a->GetScore() < 0.0f)
	{
		iCmp = 1;
	}
	else if(b->GetScore() < 0.0f)
	{
		iCmp = -1;
	}
	else
	{
		iCmp = (a->GetScore() < b->GetScore()) ? -1 : 1;
	}

	return iCmp;
}

void CVehiclePopulation::RemoveEmptyWreckedVehsAndPedsOnRespawn_MP(CPed *pRespawningPed)
{
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 poolIndex	= (s32) pool->GetSize();

	ms_AllowPoliceDeletion = true;

	while (poolIndex--)
	{
		CVehicle* pVeh= pool->GetSlot(poolIndex);
		if(pVeh)
		{
			if(!pVeh->IsNetworkClone())
			{
				float fVehDistanceFromAllPlayers = GetDistanceFromAllPlayers(CGameWorld::FindLocalPlayerCoors(), pVeh, pRespawningPed);

				if(IsVehicleWreckedOrEmpty(pVeh, fVehDistanceFromAllPlayers, true))
				{
					bool bVehRemoteVisible = IsEntityVisibleToAnyPlayer(pVeh, pRespawningPed);
					float fVehScopeDistance = pVeh->GetNetworkObject() ? pVeh->GetNetworkObject()->GetScopeData().m_scopeDistance : 300.0f;
					bool bInsideVehScopeDistance = fVehDistanceFromAllPlayers < fVehScopeDistance;
					bool bVehValidForDelete = !bVehRemoteVisible && !bInsideVehScopeDistance && pVeh->CanBeDeleted();
					if(bVehValidForDelete)
					{
						if(pVeh->GetStatus() != STATUS_WRECKED)
						{
							//! Check if any peds are preventing delete.
							for(int i = 0; i < pVeh->GetSeatManager()->GetMaxSeats(); ++i)
							{
								CPed *pPed = pVeh->GetSeatManager()->GetLastPedInSeat(i);
								if( pPed )
								{
									if((pPed->GetMyVehicle() == pVeh))
									{
										float fPedDistance = GetDistanceFromAllPlayers(CGameWorld::FindLocalPlayerCoors(), pPed, pRespawningPed);
										bool bPedRemoteVisible = IsEntityVisibleToAnyPlayer(pPed, pRespawningPed);
										float fPedScopeDistance = pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetScopeData().m_scopeDistance : 300.0f;
										if(!bPedRemoteVisible && (fPedDistance > fPedScopeDistance) && !pPed->IsNetworkClone() && pPed->CanBeDeleted())
										{
											CPedFactory::GetFactory()->DestroyPed(pPed);
										}
										else
										{
											//! Don't delete law enforcement vehicles if someone can see this ped. could result in them chasing around on foot
											//! or commandeering cars.
											if(pPed->IsLawEnforcementPed())
											{
												bVehValidForDelete = false;
												break;
											}
										}
									}
								}
							}
						}

						//! Delete vehicle.
						if(bVehValidForDelete)
						{
							BANK_ONLY(ms_lastRemovalReason = Removal_EmptyCopOrWrecked_Respawn);
							vehPopDebugf2("0x8%p - Remove vehicle, empty cop and wrecked car (local respawn).", pVeh);
							RemoveVeh(pVeh, true, Removal_EmptyCopOrWrecked_Respawn);
						}
					}
				}
			}
		}
	}

	ms_AllowPoliceDeletion = false;
}

void CVehiclePopulation::RemoveExcessEmptyCopAndWreckedVehs_MP()
{
	// Doesn't need to be done frequently at all
	if ((fwTimer::GetSystemFrameCount() & excessiveWreckedOrEmptyCopCars_CheckFrameInterval) != excessiveWreckedOrEmptyCopCars_CheckFrameOfffset)
	{
		return;
	}

	// Go through the pool and count the potential cars we have.
	s32		numCarsProcessed = 0;

	atFixedArray<tDeletionEntry, MAX_CARS_TO_STORE_MP_DELETE> apCars;

	Vector3	popCentre = CGameWorld::FindLocalPlayerCoors();

	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 poolIndex	= (s32) pool->GetSize();

	while (poolIndex--)
	{
		CVehicle* pVeh= pool->GetSlot(poolIndex);
		if(pVeh)
		{
			float fDistanceFromAllPlayers = GetDistanceFromAllPlayers(popCentre, pVeh);

			if(IsVehicleWreckedOrEmpty(pVeh, fDistanceFromAllPlayers, false))
			{		
				bool bValidForDelete=false;
				if(pVeh->IsNetworkClone())
				{
					bValidForDelete = pVeh->CanBeDeletedSpecial(true, false);
				}
				else
				{
					bValidForDelete = pVeh->CanBeDeleted();
				}

				//! Note: we include clones in this check. This ensures we try and delete the most appropriate vehicle across all machines.
				if(bValidForDelete)
				{
					if (aiVerifyf(apCars.GetCount() < apCars.GetMaxCount(), "Increase MAX_CARS_TO_STORE_MP_DELETE!"))
					{
						tDeletionEntry& deletionEntry = apCars.Append();
						deletionEntry.pVehicle = pVeh;
						deletionEntry.bLocalVisible = pVeh->GetIsVisibleInSomeViewportThisFrame();
						float fRemoteVisibleDistance = excessiveWreckedOrEmptyCopCars_RemoteRemovalRange_Default;
						if( pVeh->GetStatus() == STATUS_WRECKED )
						{
							fRemoteVisibleDistance = excessiveWreckedOrEmptyCopCars_RemoteRemovalRange_Wrecked;
						}
						else if(pVeh->GetVehicleModelInfo() && pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG))
						{
							fRemoteVisibleDistance = excessiveWreckedOrEmptyCopCars_RemoteRemovalRange_BigOrObvious;
						}

						deletionEntry.bRemoteVisible = IsEntityVisibleToRemotePlayer(pVeh, NULL, fRemoteVisibleDistance);
						deletionEntry.fDist = fDistanceFromAllPlayers;
					}
				}

				//! Note: Don't include intact parked cars for deletion. These are a backup to prevent deleting cars in front of us.
				//! So they are still added to the list.
				if(!IsIntactParkedCar(pVeh))
					numCarsProcessed++;
			}
			else
			{
				pVeh->m_nCloneFlaggedForWreckedOrEmptyDeletionTime = 0;
			}
		}
	}

	ms_iNumWreckedEmptyCars = apCars.GetCount();

	if(apCars.GetCount() > 0 BANK_ONLY(|| ms_bDrawMPExcessiveDebug) )
	{
		s32 iMaxEmptyCars = excessiveWreckedOrEmptyCopCars_BaseMaxAllowed;
		if( NetworkInterface::IsGameInProgress() )
		{
			const s32 iNumNetworkPlayers = NetworkInterface::GetNumActivePlayers();
			iMaxEmptyCars -= iNumNetworkPlayers * excessiveWreckedOrEmptyCopCars_MaxReductionPerPlayerInMp;
			iMaxEmptyCars = Max(iMaxEmptyCars, excessiveWreckedOrEmptyCopCars_SmallestMaxInMpWithClone);
		}

		//Ensure we have cars we can possibly remove.
		s32 nNumOverLimit = numCarsProcessed - iMaxEmptyCars;
		s32 nNumCarsToDelete = Max(numCarsProcessed - excessiveWreckedOrEmptyCopCars_MaxAggressiveDespawn, nNumOverLimit);

		if(nNumCarsToDelete <= 0 BANK_ONLY(&& !ms_bDrawMPExcessiveDebug))
		{
			return;
		}

		//Calculate the number of cars to remove without visibility checks.
		static dev_u32 iMaxEmptyCarsToUseVisibilityChecks = 20;
		static dev_u32 iMaxEmptyCarsToUseLongDistanceVisibilityChecks = 16;
		static dev_u32 iMinNumCarsConsideredBeforeReduceRange = 10;
		static dev_u32 iMaxNumCarsConsideredBeforeReduceRange = 15;
		static dev_u32 iMinNumCarsConsideredBeforeReduceRange_Visible = 16;
		static dev_u32 iMaxNumCarsConsideredBeforeReduceRange_Visible = 25;
		static dev_u32 iNumCarsBufferToPreventVisibleDeletion = 10;

		s32 iMaxCarsToRemoveWithoutVisibilityChecks = (numCarsProcessed - iMaxEmptyCarsToUseVisibilityChecks);
		s32 iMaxLongDistanceCarsToRemoveWithoutVisibilityChecks = (numCarsProcessed - iMaxEmptyCarsToUseLongDistanceVisibilityChecks);

		//! Don't allow visible deletion unless we start getting close to our max count. Not much point in doing this if we are happy with our population levels.
		if(GetTotalAmbientMovingVehsOnMap() < (GetDesiredNumberAmbientVehicles_Cached() - iNumCarsBufferToPreventVisibleDeletion) )
		{
			iMaxCarsToRemoveWithoutVisibilityChecks = 0;
			iMaxLongDistanceCarsToRemoveWithoutVisibilityChecks = 0;
		}

		float fRangeReductionScaler = ((float)( apCars.GetCount() - iMinNumCarsConsideredBeforeReduceRange )) /  ((float) (iMaxNumCarsConsideredBeforeReduceRange - iMinNumCarsConsideredBeforeReduceRange));
		fRangeReductionScaler = Clamp(fRangeReductionScaler, 0.0f, 1.0f);
		float fRangeReductionScaler_Visible = ((float)( apCars.GetCount() - iMinNumCarsConsideredBeforeReduceRange_Visible )) /  ((float) (iMaxNumCarsConsideredBeforeReduceRange_Visible - iMinNumCarsConsideredBeforeReduceRange_Visible));
		fRangeReductionScaler_Visible = Clamp(fRangeReductionScaler_Visible, 0.0f, 1.0f);

		const float fAggressiveMinRemovalDistSq = Lerp(fRangeReductionScaler, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSqAgg, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_OverloadedSq);
		const float fPassiveMinRemovalDistSq = Lerp(fRangeReductionScaler, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSq, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_OverloadedSq);
		const float fAggressiveMinRemovalDistSq_Visible = Lerp(fRangeReductionScaler_Visible, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSqAgg, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersAgg_OverloadedSq);
		const float fPassiveMinRemovalDistSq_Visible = Lerp(fRangeReductionScaler_Visible, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayersSq, excessiveWreckedOrEmptyCopCars_MinRemovalDistFromPlayers_OverloadedSq);

		//! Do another score pass. Need to know how many vehicles we have processed to do this, which is why we can't do this before pushing into the list.
		for(int carToConsider = apCars.GetCount()-1; carToConsider >= 0; --carToConsider)
		{
			tDeletionEntry &entry = apCars[carToConsider];
			float fPassiveRejectionDist = entry.IsVisible() ? fPassiveMinRemovalDistSq_Visible : fPassiveMinRemovalDistSq;
			float fAggressiveRejectionDist = entry.IsVisible() ? fAggressiveMinRemovalDistSq_Visible : fAggressiveMinRemovalDistSq;
			entry.fScore = GetDeletionScoreForVehicle(entry, fPassiveRejectionDist, fAggressiveRejectionDist);
		}

		apCars.QSort(0, -1, tDeletionEntry::Sort);

#if __BANK    
		if(ms_bDrawMPExcessiveDebug)
		{
			for( s32 iRender = 0; iRender < apCars.GetCount(); iRender++ )
			{
				CVehicle *pRemovalVeh = apCars[iRender].pVehicle;
				if (pRemovalVeh)
				{
					const bool bVisible = apCars[iRender].IsVisible();
					char Buff[128];
					sprintf(Buff, "%d[%.2f]%s%s%s", iRender, apCars[iRender].fScore , bVisible ? "(V)" : "", apCars[iRender].bPassive ? "(P)" : "", apCars[iRender].bAggressive ? "(A)" : "");
					grcDebugDraw::Text(pRemovalVeh->GetTransform().GetPosition() + VECTOR3_TO_VEC3V(ZAXIS), Color_blue, 0, 0, Buff, true, -60);

					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pRemovalVeh->GetTransform().GetPosition()) + ZAXIS, 0.5f, pRemovalVeh->IsNetworkClone() ? Color_red : Color_yellow, true, -60);
				}
			}

			s32 numVisibleCars = 0;
			s32 numClonesCars = 0;
			for(int carToConsider = 0; carToConsider < apCars.GetCount(); ++carToConsider)
			{
				if(apCars[carToConsider].IsVisible())
					numVisibleCars++;
				if(apCars[carToConsider].pVehicle->IsNetworkClone())
					numClonesCars++;
			}

			char Buff[128];
			sprintf(Buff, "T: %d L: %d, C: %d, M: %d V: %d", numCarsProcessed, apCars.GetCount()-numClonesCars, numClonesCars, iMaxEmptyCars, numVisibleCars);
			grcDebugDraw::Text(popCentre + ZAXIS, Color_yellow, 0, 0, Buff, true, -60);
		}
#endif

		BANK_ONLY( if(iMaxLongDistanceCarsToRemoveWithoutVisibilityChecks > 0) { DumpExcessEmptyCopAndWreckedListToLog(apCars); } );

		//we now have a sorted list of cars to remove.
		s32 iCarsRemoved = 0;
		for(int carToConsider = 0; carToConsider < apCars.GetCount(); ++carToConsider)
		{
			CVehicle *pRemovalVeh = apCars[carToConsider].pVehicle;

			//! invalid scores are sorted to back of the list, so break out if we hit this.
			if(apCars[carToConsider].fScore < 0.0f)
			{
				break;
			}

			//! if we have deleted enough cars, just bail now.
			if(iCarsRemoved >= nNumCarsToDelete)
			{
				continue;
			}

			if (!pRemovalVeh)
			{
				continue;
			}

			bool bPreventVisibleCarRemoval = (iCarsRemoved >= iMaxCarsToRemoveWithoutVisibilityChecks);
			if(bPreventVisibleCarRemoval && apCars[carToConsider].IsVisible())
			{			
				continue;
			}

			bool bPreventShotDistanceVisibleCarRemoval = (iCarsRemoved >= iMaxLongDistanceCarsToRemoveWithoutVisibilityChecks);
			if(bPreventShotDistanceVisibleCarRemoval && apCars[carToConsider].IsVisible())
			{
				TUNE_GROUP_FLOAT(VEHICLE_POPULATION, LONG_RANGE_VISIBLE_REMOVAL_DIST_MIN, 100.f, 0.0f, 99999.0f, 1.01f);
				TUNE_GROUP_FLOAT(VEHICLE_POPULATION, LONG_RANGE_VISIBLE_REMOVAL_DIST_MAX, 0.f, 0.0f, 99999.0f, 1.01f);

				float fScale = ((float)(iCarsRemoved - iMaxEmptyCarsToUseLongDistanceVisibilityChecks)) / ((float)(iMaxEmptyCarsToUseVisibilityChecks - iMaxEmptyCarsToUseLongDistanceVisibilityChecks));
				float fLongDistance = LONG_RANGE_VISIBLE_REMOVAL_DIST_MIN + ((LONG_RANGE_VISIBLE_REMOVAL_DIST_MAX - LONG_RANGE_VISIBLE_REMOVAL_DIST_MIN) * fScale);

				if(apCars[carToConsider].fDist < fLongDistance)
				{
					continue;
				}
			}

			if(pRemovalVeh->IsNetworkClone())
			{
				//! We can't delete clones. But assume that if we locally think it deserves to deleted, that it's owner will think that too. So apply a little
				//! time to allow that to happen. If this doesn't happen quickly enough, then start to delete more aggressively from our local list.
				static dev_u32 s_fTimeToIncludeInDeletionCount = 5000;
				if( pRemovalVeh->m_nCloneFlaggedForWreckedOrEmptyDeletionTime == 0 )
				{
					pRemovalVeh->m_nCloneFlaggedForWreckedOrEmptyDeletionTime = fwTimer::GetTimeInMilliseconds() + s_fTimeToIncludeInDeletionCount;
				}

				//! Ok, we locally thought this vehicle should have been deleted by a clone. However, this hasn't happened, so lets
				//! start to delete a local vehicle further on in list.
				if(fwTimer::GetTimeInMilliseconds() < pRemovalVeh->m_nCloneFlaggedForWreckedOrEmptyDeletionTime)
				{
					++iCarsRemoved;	
				}
#if __BANK
				else
				{
					Displayf("CVehiclePopulation::RemoveExcessEmptyCopAndWreckedVehs_MP - Clone vehicle hasn't been deleted, skipping! Index = %d", carToConsider);
				}
#endif
			}
			else
			{
#if __BANK
				if(apCars[carToConsider].IsVisible())
				{
					Displayf("CVehiclePopulation::RemoveExcessEmptyCopAndWreckedVehs_MP - Deleting a visible vehicle! Index = %d", carToConsider);
				}
				BANK_ONLY(ms_lastRemovalReason = Removal_EmptyCopOrWrecked);
				vehPopDebugf2("0x8%p - Remove vehicle, empty cop and wrecked car. Not close to any player.", pRemovalVeh);
#endif
				RemoveVeh(pRemovalVeh, true, Removal_EmptyCopOrWrecked);
				++iCarsRemoved;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemovePatrollingLawVehs
// PURPOSE :	If there are vehicles that have law peds which have been dispatched or were in combat but are now just wandering 
//				we want to get rid of them as they might be stuck in odd places (like a canal)
/////////////////////////////////////////////////////////////////////////////////
static const u32	wanderingLawVehicles_CheckFrameInterval			= 63;// Make sure this is of the form (2^N - 1).
static const u32	wanderingLawVehicles_CheckFrameOffset			= 45;// Make sure this is less than wanderingLawVehicles_CheckFrameInterval.
static const float	emptyLawVehicleMinRemovalDist					= 35.0f * 35.0f;

void CVehiclePopulation::RemoveEmptyOrPatrollingLawVehs(const Vector3& popCtrlCentre)
{
	// Don't do this too often
	if((fwTimer::GetSystemFrameCount() & wanderingLawVehicles_CheckFrameInterval) != wanderingLawVehicles_CheckFrameOffset)
	{
		return;
	}

	// If we have any wanted players then we don't want to bother trying to remove law vehicles or peds
	if( NetworkInterface::IsGameInProgress() &&
		NetworkInterface::FindClosestWantedPlayer(popCtrlCentre, WANTED_LEVEL1) )
	{
		return;
	}
	else
	{
		CWanted* pLocalPlayerWanted = CGameWorld::FindLocalPlayerWanted();
		if(pLocalPlayerWanted && pLocalPlayerWanted->GetWantedLevel() > WANTED_CLEAN)
		{
			return;
		}
	}

	// Go through the pool and remove patrolling vehicles
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();
	while(i--)
	{
		CVehicle* pVeh= pool->GetSlot(i);
		if(pVeh && !pVeh->GetIsInReusePool() && pVeh->PopTypeIsRandom() && (pVeh->PopTypeGet() != POPTYPE_RANDOM_PARKED) && !pVeh->GetIsVisibleInSomeViewportThisFrame() && pVeh->CanBeDeleted())
		{
			Vector3 vToPopCentre = popCtrlCentre - VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
			if(vToPopCentre.XYMag2() < emptyLawVehicleMinRemovalDist)
			{
				continue;
			}

			//Ensure no ped is entering the vehicle or using it as cover
			static dev_u32 s_uMinTimeSinceLastPedEnteringVehicle = 500;
			static dev_u32 s_uMinTimeSinceLastPedInCoverOnvehicle = 500;

			bool bCanRemove = false;
			if(pVeh->m_nVehicleFlags.bWasCreatedByDispatch && pVeh->GetSeatManager()->CountPedsInSeats() == 0)
			{
				if( CTimeHelpers::GetTimeSince(pVeh->GetIntelligence()->GetLastTimePedEntering()) > s_uMinTimeSinceLastPedEnteringVehicle &&
					CTimeHelpers::GetTimeSince(pVeh->GetIntelligence()->GetLastTimePedInCover()) > s_uMinTimeSinceLastPedInCoverOnvehicle &&
					pVeh->DelayedRemovalTimeHasPassed() )
				{
					bCanRemove = true;
				}
			}
			else
			{
				for (s32 iSeat = 0; iSeat < pVeh->GetSeatManager()->GetMaxSeats(); iSeat++)
				{
					CPed* pPedInSeat = pVeh->GetSeatManager()->GetPedInSeat(iSeat);
					if(pPedInSeat)
					{
						if( !pPedInSeat->IsInjured() && pPedInSeat->GetPedResetFlag(CPED_RESET_FLAG_PatrollingInVehicle) &&
							(pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBeenInArmedCombat) || pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch)) &&
							pPedInSeat->DelayedRemovalTimeHasPassed())
						{
							bCanRemove = true;
							break;
						}
					}
				}
			}
			
			if(!bCanRemove)
			{
				continue;
			}

			if( NetworkInterface::IsGameInProgress() &&
				NetworkInterface::IsVisibleToAnyRemotePlayer(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()), pVeh->GetBoundRadius(), 100.0f) )
			{
				continue;
			}

			BANK_ONLY(ms_lastRemovalReason = Removal_PatrollingLawNoLongerNeeded);
			vehPopDebugf2("0x8%p - Remove vehicle, has non mission cops that were either dispatched or in combat at some point but is now patrolling", pVeh);
			RemoveVeh(pVeh, true, Removal_PatrollingLawNoLongerNeeded);
		}
	}
}

bool CVehiclePopulation::ShouldEmptyCopOrWreckedVehBeRemovedAggressively(const CVehicle& rVehicle, float fDistSq, float fRemovalDistance)
{
	//Ensure the vehicle exceeds the min distance.
	if(fDistSq < fRemovalDistance)
	{
		return false;
	}

	if(rVehicle.GetStatus() == STATUS_WRECKED)
	{
		if((fwTimer::GetTimeInMilliseconds() - rVehicle.GetTimeOfDestruction()) < excessiveWreckedOrEmptyCopCars_MinRemovalWreckedTimeMs)
		{
			return false;
		}
	}

	//Ensure no ped is entering the vehicle.
	static dev_u32 s_uMinTimeSinceLastPedEntering = 2000;
	if(CTimeHelpers::GetTimeSince(rVehicle.GetIntelligence()->GetLastTimePedEntering()) < s_uMinTimeSinceLastPedEntering)
	{
		return false;
	}

	//Ensure no ped is in cover on the vehicle.
	static dev_u32 s_uMinTimeSinceLastPedInCover = 2000;
	if(CTimeHelpers::GetTimeSince(rVehicle.GetIntelligence()->GetLastTimePedInCover()) < s_uMinTimeSinceLastPedInCover)
	{
		return false;
	}

	//! if we are trying to delete a time sliced parked car, just treat it as aggressive.
	if( NetworkInterface::IsGameInProgress() && 
		((rVehicle.PopTypeGet() == POPTYPE_RANDOM_PARKED) && 
		(rVehicle.GetStatus() != STATUS_WRECKED) &&
		rVehicle.ShouldSkipUpdatesInTimeslicedLod()) )
	{
		return true;
	}

	//Check if there are any other vehicles nearby we can hide the despawn behind
	int iNumVehiclesNearby = 0;
	const CEntityScannerIterator entityList = rVehicle.GetIntelligence()->GetVehicleScanner().GetIterator();
	for(const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Get the vehicle.
		aiAssert(pEntity->GetIsTypeVehicle());
		const CVehicle* pNearbyVehicle = static_cast<const CVehicle *>(pEntity);

		//Ensure the distance does not exceed the maximum.
		ScalarV scDistSq = DistSquared(rVehicle.GetTransform().GetPosition(), pNearbyVehicle->GetTransform().GetPosition());
		static dev_float s_fMaxDistance = 4.0f;
		float fMaxDistance = s_fMaxDistance + rVehicle.GetBoundRadius() + pNearbyVehicle->GetBoundRadius();
		ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}

		//Ensure the speed does not exceed the maximum.
		ScalarV scSpeedSq = MagSquared(VECTOR3_TO_VEC3V(pNearbyVehicle->GetVelocity()));
		static dev_float s_fMaxSpeed = 4.0f;
		ScalarV scMaxSpeedSq = ScalarVFromF32(square(s_fMaxSpeed));
		if(IsGreaterThanAll(scSpeedSq, scMaxSpeedSq))
		{
			continue;
		}

		//Check if we have reached the threshold.
		++iNumVehiclesNearby;
		static dev_s32 s_iMinVehiclesNearby = 2;
		if(iNumVehiclesNearby >= s_iMinVehiclesNearby)
		{
			return true;
		}
	}

	return false;
}

bool CVehiclePopulation::ShouldEmptyCopOrWreckedVehBeRemovedPassively(const CVehicle& rVehicle, float fDistSq, float fRemovalDistance)
{
	if (fDistSq > fRemovalDistance)
	{
		if(rVehicle.GetStatus() == STATUS_WRECKED)
		{
			if((fwTimer::GetTimeInMilliseconds() - rVehicle.GetTimeOfDestruction()) < excessiveWreckedOrEmptyCopCars_MinRemovalWreckedTimeMs)
			{
				return false;
			}
		}
		else
		{
			if((NetworkInterface::GetSyncedTimeInMilliseconds() - rVehicle.m_LastTimeWeHadADriver) < excessiveWreckedOrEmptyCopCars_MinRemovalAbandonedTimeMs &&
				!(rVehicle.GetDriver() && rVehicle.GetDriver()->IsInjured() ) )
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemoveCongestionInNetGames
// PURPOSE :	Traffic jams that sit there for ages really stop the cars flowing.
//				This code will periodically check whether cars can be removed.
// PARAMETERS :	
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
static const u32	congestionCheckFrameInterval				= 31;// Make sure this is of the form (2^N - 1).
static const u32	congestionCheckFrameOfffset					= 23;// Make sure this is less than congestionCheckFrameInterval.
static const u32	congestionStuckVehConsiderationTimeMs		= 7000;//10000
static const float	congestionDensityThesholdPerSamplingArea	= 3;//4
static const float	congestionSamplingRadius					= 10.0f;
static const float	congestionMinRemovalDist					= 60.0f;//70.0f

void CVehiclePopulation::RemoveCongestionInNetGames()
{
	if (!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	// Doesn't need to be done frequently at all
	if ((fwTimer::GetSystemFrameCount() & congestionCheckFrameInterval) != congestionCheckFrameOfffset)
	{
		return;
	}

	// Go over all the vehicles checking if they can be removed.
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i	= (s32) pool->GetSize();
	while (i--)
	{
		CVehicle* pVeh= pool->GetSlot(i);
		if(!pVeh || !pVeh->CanBeDeleted())
		{
			continue;
		}

		if(pVeh->GetIsInReusePool())
		{
			continue;
		}

		//Attempt these checks only if the vehicle isn't specifically flagged for removal
		Vector3	Centre = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
		if(!pVeh->m_nVehicleFlags.bReadyForCleanup)
		{
			// Only do remove ground vehicles.
			if (pVeh->GetVehicleType()!=VEHICLE_TYPE_BIKE && pVeh->GetVehicleType()!=VEHICLE_TYPE_BICYCLE && pVeh->GetVehicleType()!=VEHICLE_TYPE_CAR && pVeh->GetVehicleType()!=VEHICLE_TYPE_QUADBIKE && pVeh->GetVehicleType()!=VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
			{
				continue;
			}

			// Only remove vehicles that won't be noticed.
			if (pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) )
			{
				continue;
			}

			// Don't remove parked cars.
			if (pVeh->PopTypeGet() == POPTYPE_RANDOM_PARKED)
			{
				continue;
			}

			// Only remove vehicles that are random (non-mission) and don't have players or mission chars in them.
			if (!IsPopTypeRandomNonPermanent(pVeh->PopTypeGet()) || pVeh->ContainsPlayer() || pVeh->HasMissionCharsInIt())
			{			
				continue;
			}

			// Only remove when they've been stuck for a while.
			if (fwTimer::GetTimeInMilliseconds() < pVeh->GetIntelligence()->LastTimeNotStuck + congestionStuckVehConsiderationTimeMs)
			{
				continue;
			}

			// Only remove if the are really part of a congestion problem.
			// Note: This will return the car we're investigating too.
			if (CountVehsInArea(Centre, congestionSamplingRadius) < congestionDensityThesholdPerSamplingArea )
			{
				continue;
			}
		}
		// Only remove if they are far enough away from the local player.
		if (!CGameWorld::FindLocalPlayer() || ((VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition()) - Centre).Mag() <= congestionMinRemovalDist))
		{
			continue;
		}
		// position, radius, on-screen distance, off-screen distance
		if (NetworkInterface::IsVisibleOrCloseToAnyPlayer(Centre, pVeh->GetBoundRadius(), ms_popGenKeyholeShapeOuterBandRadius, congestionMinRemovalDist))
		{
			continue;
		}

		// Looks like one that is fine to remove!
		BANK_ONLY(ms_lastRemovalReason = Removal_NetGameCongestion);
		vehPopDebugf2("0x%8p - Remove vehicle through congestion in net game.", pVeh);
		RemoveVeh(pVeh, true, Removal_NetGameCongestion);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemoveEmptyCrowdingPlayersInNetGames
// PURPOSE :	Players can cause a large number of empty cars to accumulate in one area,
//              and normal removal is prevented while these are on-screen or too close to
//              one or more players. This method removes cars under these conditions
// PARAMETERS :	
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
static const u32	excessEmptyNetCheckFrameInterval				= 31;// Make sure this is of the form (2^N - 1).
static const u32	excessEmptyNetCheckFrameOffset					= 21;// Make sure this is less than the interval.
static const float	excessEmptyNetCrowdingRadius					= 25.0f; // Vehicles beyond this range should be handled normally
static const int	excessEmptyNetCountThresholdPerPlayer			= 3; // More than this many empty cars per player will trigger removal
static const u32	RemoveExcessEmptyCrowdingInNetGames_EmptyVehicleListSize = 50;
struct EmptyVehicleDistanceListItem
{
	float m_distSqToClosestPlayerPed;
	CVehicle* m_pVeh;
};
class ExcessEmptyNetListSort
{
public:
	bool operator()(const EmptyVehicleDistanceListItem left, const EmptyVehicleDistanceListItem right)
	{
		// sort largest to smallest distance, so largest will be first after sort
		return (left.m_distSqToClosestPlayerPed < right.m_distSqToClosestPlayerPed);
	}
};
void CVehiclePopulation::RemoveEmptyCrowdingPlayersInNetGames()
{
	if( !ms_scriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame )
	{
		return;
	}

	if (!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	// Doesn't need to be done frequently at all
	if ((fwTimer::GetSystemFrameCount() & excessEmptyNetCheckFrameInterval) != excessEmptyNetCheckFrameOffset)
	{
		return;
	}

	// Local player
	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(!pLocalPlayer)
	{
		return;
	}
	// Local player position
	const Vector3 vLocalPlayerPosition = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());

	// List of vehicles to consider for removal
	EmptyVehicleDistanceListItem aEmptyVehicleConsiderationList[RemoveExcessEmptyCrowdingInNetGames_EmptyVehicleListSize];
	u32 emptyVehicleListCount = 0;

	// Go over all the vehicles checking if they should be considered for removal due to excess empty crowding
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i	= (s32) pool->GetSize();
	while (i--)
	{
		if( emptyVehicleListCount >= RemoveExcessEmptyCrowdingInNetGames_EmptyVehicleListSize )
		{
			break;
		}

		CVehicle* pVeh= pool->GetSlot(i);
		if(!pVeh)
		{
			continue;
		}

		if(pVeh->GetIsInReusePool())
		{
			continue;
		}

		// Only do remove ground vehicles.
		if (pVeh->GetVehicleType()!=VEHICLE_TYPE_BIKE && pVeh->GetVehicleType()!=VEHICLE_TYPE_BICYCLE && pVeh->GetVehicleType()!=VEHICLE_TYPE_CAR && pVeh->GetVehicleType()!=VEHICLE_TYPE_QUADBIKE && pVeh->GetVehicleType()!=VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
		{
			continue;
		}

		// Don't remove parked cars.
		if (pVeh->PopTypeGet() == POPTYPE_RANDOM_PARKED)
		{
			continue;
		}

		// Don't remove occupied cars
		if( pVeh->HasAlivePedsInIt() )
		{
			continue;
		}

		// Don't consider if any ped is entering the vehicle
		if( pVeh->GetIntelligence() )
		{
			static dev_u32 s_uMinTimeSinceLastPedEntering = 2000;
			if( CTimeHelpers::GetTimeSince(pVeh->GetIntelligence()->GetLastTimePedEntering()) < s_uMinTimeSinceLastPedEntering )
			{
				continue;
			}
		}
		
		// Only remove vehicles that are random (non-mission)
		if( !IsPopTypeRandomNonPermanent(pVeh->PopTypeGet()) )
		{
			continue;
		}

		// If vehicle cannot be deleted for a special reason
		if( !pVeh->CanBeDeleted() )
		{
			continue;
		}

		// Vehicle world position
		const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());

		// Check if beyond crowding range of the local player
		const float fDistToLocalPlayerSq = vLocalPlayerPosition.Dist2(vVehPosition);
		if( fDistToLocalPlayerSq > excessEmptyNetCrowdingRadius * excessEmptyNetCrowdingRadius )
		{
			continue;
		}

		// Initialize closest distance using local player
		float fDistToClosestPlayerSq = fDistToLocalPlayerSq;
		
		// Find the closest network player to the vehicle position,
		// as they may be closer than the local player
		const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
		const bool bSortResults = true;
		u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(vVehPosition, excessEmptyNetCrowdingRadius, closestPlayers, bSortResults);
		if( numPlayersInRange > 0 )
		{
			const CNetGamePlayer* pClosestNetGamePlayer = SafeCast(const CNetGamePlayer, closestPlayers[0]);
			if( pClosestNetGamePlayer )
			{
				const CPed* pNetPlayerPed = pClosestNetGamePlayer->GetPlayerPed();
				if(pNetPlayerPed)
				{
					const Vector3 vClosestPedPosition = VEC3V_TO_VECTOR3(pNetPlayerPed->GetTransform().GetPosition());
					const float fDistToClosestRemotePlayerSq = vClosestPedPosition.Dist2(vVehPosition);
					if( fDistToClosestRemotePlayerSq < fDistToLocalPlayerSq )
					{
						fDistToClosestPlayerSq = fDistToClosestRemotePlayerSq;
					}
				}
			}
		}

		// Write vehicle info to the list
		aEmptyVehicleConsiderationList[emptyVehicleListCount].m_distSqToClosestPlayerPed = fDistToClosestPlayerSq;
		aEmptyVehicleConsiderationList[emptyVehicleListCount].m_pVeh = pVeh;
		++emptyVehicleListCount;
	}

	// Compute the tolerance for crowding vehicles based on number of net players
	const int numEmptyThreshold = (int)(NetworkInterface::GetNumPhysicalPlayers()) * excessEmptyNetCountThresholdPerPlayer;
	const int numEmptyScriptCap = ms_scriptCapEmptyCrowdingPlayersInNetGamesThisFrame;
	
	// If enough empty vehicles were added to the list
	if( emptyVehicleListCount >= MIN(numEmptyThreshold, numEmptyScriptCap) )
	{
		// Sort the list by distance to closest net player
		std::sort(aEmptyVehicleConsiderationList, aEmptyVehicleConsiderationList + emptyVehicleListCount, ExcessEmptyNetListSort());
		
		// If the vehicle that is most distant to all net players is owned locally
		CVehicle* pMostDistantVehicle = aEmptyVehicleConsiderationList[0].m_pVeh;
		if( pMostDistantVehicle && !pMostDistantVehicle->IsNetworkClone() )
		{
			// Remove it!
			BANK_ONLY(ms_lastRemovalReason = Removal_NetGameEmptyCrowdingPlayers);
			vehPopDebugf2("0x%8p - Remove vehicle through excess empty crowding in net game.", pMostDistantVehicle);
			RemoveVeh(pMostDistantVehicle, true, Removal_NetGameEmptyCrowdingPlayers);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CountVehsInArea
// PURPOSE :	Count the number of cars near the specified centre point
// PARAMETERS :	
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////

int CVehiclePopulation::CountVehsInArea(const Vector3 &Centre, float range)
{
	const float	rangeSqr = range * range;
	int		result = 0;
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i	= (s32) pool->GetSize();
	while (i--)
	{
		const CVehicle* pVeh= pool->GetSlot(i);
		if (pVeh && !pVeh->GetIsInReusePool())
		{
			if ( (VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()) - Centre).Mag2() < rangeSqr)
			{
				result++;
			}
		}
	}
	return result;
}




void CVehiclePopulation::GatherActiveLinks()
{
	ms_iNumActiveLinks = 0;
	for(s32 i=0; i<ms_numGenerationLinks; i++)
	{
		if(ms_aGenerationLinks[i].m_bIsActive && ms_aGenerationLinks[i].m_bUpToDate)
		{
			ms_aActiveLinks[ms_iNumActiveLinks++] = i;
		}
	}
}

static u32 timeBetweenGenLinkAttemptCountUpdatesMs = 1000;

#define NUM_BEST_LINKS	32

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION		: SelectAndUpdateVehGenLinkToGenerateFrom
// PURPOSE		: Picks a veh gen link to generate from and increments the
//					current active link.
// PARAMETERS	: None.
// RETURNS		: The link index to use.
/////////////////////////////////////////////////////////////////////////////////

s32 CVehiclePopulation::SelectAndUpdateVehGenLinkToGenerateFrom()
{
	// Greatly reduced in complexity -- simply return a link index from our precomputed list
	// The active link index is incremented in TryToGenerateOneRandomVeh() when vehicle creation is
	// successful, or if there's something wrong with the link
	if(ms_iCurrentActiveLink >= VEHGEN_NUM_PRESELECTED_LINKS 
		|| ms_iCurrentActiveLink >= ms_iNumActiveLinks)
	{
		ms_iCurrentActiveLink = VEHGEN_NUM_PRESELECTED_LINKS;
		return -1;
	}

	return ms_aActiveLinks[ms_iCurrentActiveLink];
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateVehCreationPosFromLinks
// PURPOSE :  To works out where to place new vehicles from generation links.
// PARAMETERS :	
// RETURNS :	Whether or not a valid placement position was found on this try.
/////////////////////////////////////////////////////////////////////////////////

static const u32 MAX_CREATION_COORD_TRIES = 40;

bool CVehiclePopulation::GenerateVehCreationPosFromLinks(Vector3* pResult, CNodeAddress* pFromNode, CNodeAddress* pToNode, float* pFraction, int* iVehGenLinkIndex, bool* bKeepTrying, const float rangeScale)
{
	Assert(!s_VehiclePopulationDependencyRunning);

	// Try to get a vehicle generation point from the generation links.
	if (ms_numGenerationLinks == 0 || ms_numActiveVehGenLinks == 0)
	{
		// Let the caller know if we could not find a position (as there
		// are no links to draw from).
		*bKeepTrying = false;
		return false;
	}

	Vector3 genDir;
	genDir.x = ms_popGenKeyholeShape.GetDir().x;
	genDir.y = ms_popGenKeyholeShape.GetDir().y;
	genDir.z = 0.f;

	Vector3 genCenter;
	genCenter.x = ms_popGenKeyholeShape.GetCenter().x;
	genCenter.y = ms_popGenKeyholeShape.GetCenter().y;
	genCenter.z = 0.f;

	bool bPlayerSwitch = g_PlayerSwitch.IsActive();

	// Get a vehicle generation point from the generation links.
	bool posFound = false;
	u32 numTries = 0;
	while(!posFound && (numTries <= MAX_CREATION_COORD_TRIES))
	{
		++numTries;

		s32 linkNum = SelectAndUpdateVehGenLinkToGenerateFrom();

		if(linkNum == -1) //JB: If this function returns -1, then calling it 39 more times will not fix that situation!!
			break;

		// Get the start node.
		const CNodeAddress startAddress = ms_aGenerationLinks[linkNum].m_nodeA;
		const CPathNode* pStartNode = ThePaths.FindNodePointerSafe(startAddress);
		if(!pStartNode)
			continue;

		// Get the end node.
		const CNodeAddress endAddress = ms_aGenerationLinks[linkNum].m_nodeB;
		const CPathNode* pEndNode = ThePaths.FindNodePointerSafe(endAddress);
		if(!pEndNode)
			continue;

		// Don't return the nodes so that we go against the flow of traffic.
		s16 iLink = -1;
		bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(pStartNode, pEndNode, iLink);
		Assert(bLinkFound);
		if(!bLinkFound)
			continue;
		const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pStartNode, iLink);

		s16 iLinkBack = -1;
		bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(pEndNode, pStartNode, iLinkBack);
		Assert(bLinkFound);
		if(!bLinkFound)
			continue;
		const CPathNodeLink *pLinkBack = &ThePaths.GetNodesLink(pEndNode, iLinkBack);

		// We don't want to place car on a dead end or heading towards the dead end.
		// This had been disabled for Jimmy--re-enabled for V -JM

		bool currentDirectionDeadEnds = false;
		bool reverseDirectionDeadEnds = false;
		if( ms_spawnCareAboutDeadEnds )
		{
			currentDirectionDeadEnds = pEndNode->m_2.m_deadEndness && pLink->LeadsToDeadEnd();
			reverseDirectionDeadEnds = pStartNode->m_2.m_deadEndness && pLinkBack->LeadsToDeadEnd();
			Assert(!(currentDirectionDeadEnds && reverseDirectionDeadEnds));
		}

		// Generate a candidate point.
		Vector3 startPos;
		pStartNode->GetCoors(startPos);
		Vector3 endPos;
		pEndNode->GetCoors(endPos);
		const float portionAlongLink = fwRandom::GetRandomNumberInRange(0.2f, 0.8f);
		const Vector3 candidatePoint = startPos + ((endPos - startPos) * portionAlongLink);

		if(bPlayerSwitch)
		{
			// we need to check to see if we're spawning in view during a player switch
			if(CPedPopulation::IsCandidateInViewFrustum(candidatePoint, 4.f, PLAYER_SWITCH_ON_SCREEN_SPAWN_DISTANCE))
			{
				continue; // can't use this point
			}
		}
		
		if(Unlikely(CPedPopulation::ms_useTempPopControlSphereThisFrame))
		{
#define POP_CONTROL_SPHERE_IN_VIEW_DISTANCE (200.f)
			// we need to check to see if we're spawning in view while using crazy pop zones
			if(CPedPopulation::IsCandidateInViewFrustum(candidatePoint, 2.f, POP_CONTROL_SPHERE_IN_VIEW_DISTANCE*rangeScale))
			{
				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				continue; // can't use this point
			}
		}

		// if the spawn point is behind the generation point, prefer spawning the vehicle so it moves towards the generation point

		bool preferSwitch = false;

		Vector3 spawnPointDir = candidatePoint - genCenter;
		spawnPointDir.z = 0.f;
		float spawnDirDot = spawnPointDir.Dot(genDir);
		if (spawnDirDot < 0.f)
		{
			Vector3 vehDir = endPos - startPos;
			vehDir.z = 0.f;
			float vehDirDot = vehDir.Dot(genDir);
			preferSwitch = (vehDirDot < 0.f);
		}

		// We found a good point!
		// Now make sure we hand back the correct data and direction along the link.
		*pResult = candidatePoint;

		if(ms_spawnCareAboutDeadEnds && currentDirectionDeadEnds)
		{
			continue;
			/*
			// Make sure to go the opposite way.
			*pFromNode	= endAddress;
			*pToNode	= startAddress;
			*pFraction	= 1.0f - portionAlongLink;
			*/
		}
		else if(ms_spawnCareAboutDeadEnds && reverseDirectionDeadEnds)
		{
			// Make sure to go the normal way.
			*pFromNode	= startAddress;
			*pToNode	= endAddress;
			*pFraction	= portionAlongLink;
		}
		else
		{
			if (pLink->m_1.m_LanesFromOtherNode == 0)
			{
				// better to save the vehicle slot than to spawn it and never see it
				if (preferSwitch && fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.3f)
					continue;

				*pFromNode	= startAddress;
				*pToNode	= endAddress;
				*pFraction	= portionAlongLink;
			}
			else if (pLink->m_1.m_LanesToOtherNode == 0)
			{
				continue;
				/*
				*pFromNode	= endAddress;
				*pToNode	= startAddress;
				*pFraction	= portionAlongLink;
				*/
			}
			else
			{
				// Just randomly pick a way to go.
				/*
				float val = 0.5f;
				if (preferSwitch)
					val = 0.3f;
				*/

				//MAGDEMO HACK
				//Don't allow switching the nodes around if pStartNode is a SlipJ
				//we don't want to spawn any nodes on the link where the end point
				//is a slipj, so we would have excluded those links here.
				//but the opposing link could have been added, so we need to prevent
				//swapping the nodes and giving us the link we originally rejected
				/*
				if (pStartNode->m_2.m_slipJunction)
				{
					val = 1.01f;
				}
				*/

				//if(fwRandom::GetRandomNumberInRange(0.f, 1.f) < val)
				{
					*pFromNode	= startAddress;
					*pToNode	= endAddress;
					*pFraction	= portionAlongLink;
				}
				/*
				else
				{
					*pFromNode	= endAddress;
					*pToNode	= startAddress;
					*pFraction	= 1.0f - portionAlongLink;
				}
				*/
			}
		}

		// Mark that we have found a good position.
		posFound = true;

		*iVehGenLinkIndex = linkNum;
	}

	// Let the caller know if we found a good position or not.
	return posFound;
}

bool CVehiclePopulation::IsLinkVisibleInGameViewport(const Vector3 & vNodePos1, const Vector3 & vNodePos2, const int iNumLanes)
{
	const Vector3 vCenter = (vNodePos1+vNodePos2) * 0.5f;
	const float fMag = (vNodePos2-vNodePos1).Mag();
	const float fLanesExtra = (iNumLanes * LANEWIDTH);

	return IsSphereVisibleInGameViewport(vCenter, fMag+fLanesExtra);
}

bool CVehiclePopulation::IsSphereVisibleInGameViewport(const Vector3 & vPos, const float fRadius)
{
#if 0 && __BANK
	// If in debug camera mode, explicitly check the active gameplay cam so that we can debug
	// because camInterface::IsSphereVisibleInGameViewport() works at the viewport level
	if(camInterface::GetDebugDirector().IsFreeCamActive())
	{
		return camInterface::GetGameplayDirector().IsSphereVisible(vPos, fRadius);
	}
#endif

	return camInterface::IsSphereVisibleInGameViewport(vPos, fRadius);
}

void CVehiclePopulation::UpdateCurrentVehGenLinks()
{
	PF_AUTO_PUSH_DETAIL("Update Current Veh Gen Links");

	// still need to update attempts
	// Check our to see if should update the counts.
	const u32 currentTimeMs = fwTimer::GetTimeInMilliseconds();
	const bool timerPassed = currentTimeMs > (ms_lastGenLinkAttemptCountUpdateMs + timeBetweenGenLinkAttemptCountUpdatesMs);
	const bool timerWrapped = currentTimeMs < ms_lastGenLinkAttemptCountUpdateMs;
	bool updateAttempts = (timerPassed || timerWrapped || ms_bInstantFillPopulation);
	if(updateAttempts)
	{
		vehPopDebugf3("Reset gen link update attempt count to 0. %s, %dms passed since last update", timerPassed ? "timer passed" : "timer wrapped", currentTimeMs - ms_lastGenLinkAttemptCountUpdateMs);
		ms_lastGenLinkAttemptCountUpdateMs = currentTimeMs;
	}

	for(u32 i = 0; i < ms_numGenerationLinks; i++)
	{
		if(updateAttempts)
		{
			ms_aGenerationLinks[i].m_iLinkAttempts = 0;
		}
	}

	PF_START_TIMEBAR_DETAIL("Update Densities");
	
	StartAsyncUpdateDensitiesJob();
}

void CVehiclePopulation::UpdateKeyHoleShape(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float popCtrlBaseHeading, const float UNUSED_PARAM(popCtrlFov), const float UNUSED_PARAM(popCtrlTanHFov), const float fRangeScale)
{
#if __BANK
	if (ms_freezeKeyholeShape)
	{
		return;
	}
#endif // __BANK

	const float	st0	= rage::Sinf(popCtrlBaseHeading);
	const float	ct0	= rage::Cosf(popCtrlBaseHeading);
	Vector2 dir(-st0, ct0);
	Assert(dir.Mag2() > 0.1f);
	dir.Normalize();

	const float tanHFov = camInterface::GetViewportTanFovH();
	const float fCosHalfFov = rage::Cosf(rage::Atan2f(tanHFov,1.0f));

	const float fThicknessScale = CalculatePopulationSpawnThicknessScale();
	float maxSpeedPortion = 1.f;
	
	if(ms_vehiclePopGenLookAhead)
	{
		maxSpeedPortion = rage::Max(rage::Clamp((popCtrlCentreVelocity.XYMag() / ms_maxSpeedExpectedMax), 0.0f, 1.0f) * 2.0f, 1.0f);
	}	

	float fInnerRadius = ms_popGenKeyholeShapeInnerBandRadius * fRangeScale * maxSpeedPortion;
	float fInnerThickness = ms_popGenKeyholeInnerShapeThickness * fThicknessScale;
	float fOuterRadius = ms_popGenKeyholeShapeOuterBandRadius * fRangeScale * ms_fCalculatedViewRangeScale;
	float fOuterThickness = ms_popGenKeyholeOuterShapeThickness * fThicknessScale * maxSpeedPortion;
	float fSideWallThickness = ms_popGenKeyholeSideWallThickness * fThicknessScale;

	//In network we can be prevented from spawning cars due to remote player placement
	//if we want more cars then bring in the keyhole inner radius scaled by our average disparity
	//this will allow more links to be considered. Also reduce the score required to spawn a vehicle on
	//so we've more options. This should give us more chances to spawn vehicles around the player	
	ms_fMinGenerationScore = ms_fMinBaseGenerationScore;
	if(NetworkInterface::IsGameInProgress() && NetworkInterface::CanRegisterObjects(0, 1, 1, 0, 0, false))
	{
		s32	TotalAmbientCarsOnMap = CVehiclePopulation::ms_numRandomCars;
		const s32 numRemainingSpaces = (s32)GetDesiredNumberAmbientVehicles() - TotalAmbientCarsOnMap;
		float fNetworkDisparityScale = 1.0f;		
		if(numRemainingSpaces > 10)
		{
			static float s_fInnerScale = 0.8f;
			fNetworkDisparityScale = RampValue(ms_averageGenLinkDisparity, 5.0f, 10.0f, 1.0f, s_fInnerScale);
			static float s_fGenerationScoreReduction = 1.0f;
			ms_fMinGenerationScore -= RampValue(ms_averageGenLinkDisparity, 5.0f, 10.0f, 0.0f, s_fGenerationScoreReduction);
		}

		if(fNetworkDisparityScale < 1.0f)
		{
			float fOriginalInnerOuterRadius = fInnerRadius + fInnerThickness;
			float fOriginalOuterOuterRadius = fOuterRadius + fOuterThickness;

			fInnerRadius *= fNetworkDisparityScale;
			fOuterRadius *= fNetworkDisparityScale;
			fInnerThickness = fOriginalInnerOuterRadius - fInnerRadius;
			fOuterThickness = fOriginalOuterOuterRadius - fOuterRadius;
		}
	}

	// during player switch we make sure the creation zone is a full circle covering the focus point extending
	// all the way to the max creation distance
	if (g_PlayerSwitch.ShouldUseFullCircleForPopStreaming() || ms_bInstantFillPopulation)
	{
		fInnerThickness = fInnerRadius;
		fInnerRadius = 0.f;
		fOuterRadius = fInnerThickness;
		fOuterThickness = rage::Max(0.f, GetCreationDistance(fRangeScale) - fOuterRadius);
	}

	// If camera is looking behind, then ensure that we don't allow spawning of vehicles up close in front of the player.
	// This is to avoid the player glancing behind, and the vehicle population taking that opportunity to spawn something
	// directly ahead of the player.
	// Also do this during cut scenes to compensate for quick cuts.
	CutSceneManager *pCutSceneManager = CutSceneManager::GetInstancePtr();

	bool bCutScene = pCutSceneManager && pCutSceneManager->IsCutscenePlayingBack();

	if((camInterface::IsDominantRenderedDirector(camInterface::GetGameplayDirector()) && camInterface::GetGameplayDirector().IsLookingBehind()) || bCutScene)
	{
		fInnerRadius = fOuterRadius;
		fInnerThickness = fOuterThickness;
	}

	if(Unlikely(CPedPopulation::ms_useTempPopControlSphereThisFrame && ms_TempPopControlSphereRadius != 0.f))
	{
		fInnerThickness = ms_TempPopControlSphereRadius;
		fInnerRadius = 0.f; // we want a complete circle
		fOuterRadius = 0.f;
		fOuterThickness = ms_TempPopControlSphereRadius;
	}

	ms_popGenKeyholeShape.Init(
		ms_vehiclePopGenLookAhead ? popCtrlCentre + popCtrlCentreVelocity : popCtrlCentre,
		dir,
		fCosHalfFov,
		fInnerRadius,
		fInnerRadius + fInnerThickness,
		fOuterRadius,
		fOuterRadius + fOuterThickness,
		fSideWallThickness);

	ms_popGenKeyholeShapeInitialized = true;
}

void CVehiclePopulation::RegisterVehiclePopulationFailure(BANK_ONLY(const u32 iLinkIndex, const Vector3& vPosition, const u16 iFailureType, u32& iFailureCount))
{
#if __BANK
	++iFailureCount;
	VecMapDisplayVehiclePopulationEvent(vPosition, VehPopFailedCreate);

	if (iLinkIndex != -1)
	{
		ms_aGenerationLinks[iLinkIndex].m_iFailType = iFailureType;
	}
#endif

	PF_INCREMENT(NormalAmbientVehicleFailures);
}

void CVehiclePopulation::DisableCreationOnNode(const CNodeAddress& nodeAddress)
{
	CPathNode* pNode = ThePaths.FindNodePointerSafe(nodeAddress);

	if (pNode && pNode->m_1.m_specialFunction == SPECIAL_USE_NONE)
	{
		pNode->m_1.m_specialFunction = SPECIAL_DISABLE_VEHICLE_CREATION;

		// If we've filled the array, re-enable the oldest disabled node, and replace it in the array
		if (!ms_DisabledNodes[ms_iNextDisabledNodeIndex].IsEmpty())
		{
			CPathNode* pOldestDisabledNode = ThePaths.FindNodePointerSafe(ms_DisabledNodes[ms_iNextDisabledNodeIndex]);

			if (pOldestDisabledNode)
			{
				pOldestDisabledNode->m_1.m_specialFunction = SPECIAL_USE_NONE;
			}
		}

		ms_DisabledNodes[ms_iNextDisabledNodeIndex] = nodeAddress;

		++ms_iNextDisabledNodeIndex;

		if (ms_iNextDisabledNodeIndex == VEHGEN_MAX_DISABLED_NODES)
		{
			ms_iNextDisabledNodeIndex = 0;
		}
	}
}

void CVehiclePopulation::ClearDisabledNodes()
{
	for (u32 i = 0; i < VEHGEN_MAX_DISABLED_NODES; ++i)
	{
		if (!ms_DisabledNodes[i].IsEmpty())
		{
			CPathNode* pNode = ThePaths.FindNodePointerSafe(ms_DisabledNodes[i]);

			if (pNode)
			{
				pNode->m_1.m_specialFunction = SPECIAL_USE_NONE;
			}

			ms_DisabledNodes[i].SetEmpty();
		}
	}

	ms_iNextDisabledNodeIndex = 0;
}

void CVehiclePopulation::TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink()
{
	DisableCreationOnNode(ms_aGenerationLinks[ms_aActiveLinks[ms_iCurrentActiveLink]].m_nodeA);
	DisableCreationOnNode(ms_aGenerationLinks[ms_aActiveLinks[ms_iCurrentActiveLink]].m_nodeB);

	++ms_iCurrentActiveLink;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateRandomVehs
// PURPOSE :  This function works out where to place new random vehs and does so.
//			  A maximum of one veh is generated each frame
/////////////////////////////////////////////////////////////////////////////////
RAGETRACE_DECL(CVehiclePopulation_GenerateRandomCars);
void CVehiclePopulation::GenerateRandomVehs(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale)
{
	RAGETRACE_SCOPE(CVehiclePopulation_GenerateRandomCars);
	MUST_FIX_THIS( Cutscene engine );

	Assert(!s_VehiclePopulationDependencyRunning);

	// Begin Relocated from Process()
	const float fPopScheduleScale = ((float)CPopCycle::GetCurrentPercentageOfMaxCars()) / 100.0f;
	const float fMultiplayerUpperLimitMultiplier =
		NetworkInterface::IsGameInProgress() ?
		((float)ms_iAmbientVehiclesUpperLimitMP) / ((float)Max(ms_iAmbientVehiclesUpperLimit,1) )
		: 1.0f;
	const float fAmbientVehicleMultiplier = fPopScheduleScale * GetTotalRandomVehDensityMultiplier() * fMultiplayerUpperLimitMultiplier;

	if(fAmbientVehicleMultiplier <= 0.0f)
	{
		return;
	}

	if(FindNumRandomVehsThatCanBeCreated() <= 0)
	{
		if(DoAnyVehGenLinksHaveADisparity())
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "GenerateRandomVehs", "Failed:", "Population is full, can't create more vehicles");
			vehPopDebugf3("Population is full, can't create more vehicles.");
			BANK_ONLY(m_populationFailureData.m_numPopulationIsFull++);
		}

		return;
	}

	// Give game a couple of frames to get everything initialized. Then we generate vehs around player.
	if(ms_countDownToCarsAtStart > 0)
	{
		ms_countDownToCarsAtStart--;

		if(ms_countDownToCarsAtStart == 0)
		{
			ms_bInstantFillPopulation = 1;
		}
	}

	const s32 iUpperLimit = NetworkInterface::IsGameInProgress() ? ms_iAmbientVehiclesUpperLimitMP : ms_iAmbientVehiclesUpperLimit;
	
	s32 maxAttempts = 1;

	if(ms_bInstantFillPopulation && !g_PlayerSwitch.IsActive())
	{
		maxAttempts = iUpperLimit;
		vehPopDebugf2("Instant fill population requested, %d attempts", maxAttempts);
		gPopStreaming.InitialVehicleRequest(1);
	}
	// End Relocated from Process()

	// Generate some car around the player
	if(DoAnyVehGenLinksHaveADisparity())
	{
		s32 iNumTries = 0;
		while( iNumTries < maxAttempts && ms_numRandomCars < iUpperLimit )
		{
			TryToGenerateOneRandomVeh(popCtrlCentre, popCtrlCentreVelocity, rangeScale);

			iNumTries++;
		}

		ms_bGenerationSkippedDueToNoLinksWithDisparity = false;
	}
	else
	{
		ms_bGenerationSkippedDueToNoLinksWithDisparity = true;
	}
}

float CVehiclePopulation::GetVehicleLodScale(bool bIgnoreSettings)
{
	if (ms_fRendererRangeMultiplierOverrideThisFrame == 0.0f)
	{
		return bIgnoreSettings ? g_LodScale.GetGlobalScaleIgnoreSettings() : g_LodScale.GetGlobalScale();
	}
	else
	{
		return ms_fRendererRangeMultiplierOverrideThisFrame;
	}
}

float CVehiclePopulation::CalculatePopulationSpawnThicknessScale()
{
	if(!ms_bZoomed || !ms_bEnableUseGatherLinksKeyhole)
	{
		return 1.0f;
	}
	else
	{
		return ms_fZoomThicknessScale;
	}
}

float CVehiclePopulation::CalculateHeightRangeScale(const float fLowZ, const float fHighZ, const float fMult)
{
	float fHeightRangeScale = 1.0f;

	if(fHighZ > fLowZ)
	{
		float fHeightS;

		if( !ms_bIncreaseSpawnRangeBasedOnHeight || ms_lastPopCenter.m_centre.z < fLowZ)
		{
			fHeightS = 0.0f;
		}
		else if ( ms_lastPopCenter.m_centre.z > fHighZ)
		{
			fHeightS = 1.0f;
		}
		else
		{
			fHeightS = (ms_lastPopCenter.m_centre.z - fLowZ) / (fHighZ - fLowZ);
		}

		fHeightRangeScale = 1.0f + (fHeightS * (fMult-1.0f));
	}

	return fHeightRangeScale;
}

float CVehiclePopulation::CalculateAircraftRangeScale()
{
	const float fLowZ = ms_averageLinkHeight + 70.0f;
	const float fHighZ = fLowZ + 50.0f;

	float fAircraftRangeScale = CalculateHeightRangeScale(fLowZ, fHighZ, ms_fAircraftSpawnRangeMult);

	if(NetworkInterface::IsGameInProgress() && fAircraftRangeScale > ms_fAircraftSpawnRangeMultMP)
	{
		fAircraftRangeScale = ms_fAircraftSpawnRangeMultMP;
	}

	return fAircraftRangeScale;
}

void CVehiclePopulation::CalculatePopulationRangeScale()
{
#if __BANK
	if (ms_freezeKeyholeShape)
	{
		return;
	}
#endif // __BANK

	if (ms_bLockPopuationRangeScale)
	{
		return;
	}

	PF_FUNC(CalculatePopulationRangeScale);

	// Scale all ranges according to depth of the pop control into interiors.
	float scalingFromPosInfo = 1.0f;
	switch (ms_lastPopCenter.m_locationInfo)
	{
	case LOCATION_EXTERIOR:
		scalingFromPosInfo = 1.0f;
		break;
	case LOCATION_SHALLOW_INTERIOR:
		scalingFromPosInfo = ms_rangeScaleShallowInterior;
		break;
	case LOCATION_DEEP_INTERIOR:
		scalingFromPosInfo = ms_rangeScaleDeepInterior;
		break;
	default:
		Assert(false);
		break;
	}


	static dev_float scalingFromCameraOrientation = 1.0f;


	//-------------------------------------------------------------------------------------------------------------------

	float fBaseRangeScale = 1.0f;

	CPopZone * pPopZone = CPopZones::FindSmallestForPosition(&ms_lastPopCenter.m_centre, ZONECAT_ANY, ZONETYPE_SP);
	if(pPopZone)
	{
		fBaseRangeScale = pPopZone->m_fPopBaseRangeScale;
	}

	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, HEIGHT_RANGE_SCALE_MIN_THRESHOLD, 1.3f, 1.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, HEIGHT_RANGE_SCALE_MAX, 2.2f, 1.0f, 10.0f, 0.1f);

	float fHeightRangeScale = 1.0f;

	if(ms_bIncreaseSpawnRangeBasedOnHeight)
	{
		fHeightRangeScale = CalculateHeightRangeScale(ms_averageLinkHeight + ms_fHeightBandStartZ, ms_averageLinkHeight + ms_fHeightBandEndZ, ms_fHeightBandMultiplier);

		// Ignore height range scales below the minimum threshold
		if(fHeightRangeScale < HEIGHT_RANGE_SCALE_MIN_THRESHOLD)
		{
			fHeightRangeScale = 1.0f;
		}

		if(ms_bZoneBasedHeightRange && pPopZone)
		{
			const float fZoneBasedHeightRangeScale = CalculateHeightRangeScale(pPopZone->m_iPopHeightScaleLowZ, pPopZone->m_iPopHeightScaleHighZ, pPopZone->m_fPopHeightScaleMultiplier);
			fHeightRangeScale = Max(fHeightRangeScale, fZoneBasedHeightRangeScale);
		}

		// Max height range scale
		fHeightRangeScale = Min(fHeightRangeScale, HEIGHT_RANGE_SCALE_MAX);
	}

	//-------------------------------------------------------------------------------------------------------------------

	float fScalingFromVehicleType = 1.0f;

	CVehicle * pPlayerVehicle = FindPlayerVehicle();

	if (pPlayerVehicle && pPlayerVehicle->GetIsAircraft())
	{
		fScalingFromVehicleType = CalculateAircraftRangeScale();
	}

	//-------------------------------------------------------------------------------------------------------------------

	const float fScriptedRangeMultiplier = GetScriptedRangeMultiplier();

	// Don't let scripted multiplier be set in addition to vehicle-type multiplier
	if(GetScriptedRangeMultiplier() != 1.0f)
		fScalingFromVehicleType = 1.0f;

	float rangeScale =
		fBaseRangeScale *
			scalingFromPosInfo *
				scalingFromCameraOrientation *
					fScalingFromVehicleType *
						fScriptedRangeMultiplier *
							ms_allZoneScaler;

#if __BANK
	ms_RangeScale_fBaseRangeScale = fBaseRangeScale;
	ms_RangeScale_fScalingFromPosInfo = scalingFromPosInfo;
	ms_RangeScale_fscalingFromCameraOrientation = scalingFromCameraOrientation;
	ms_RangeScale_fScalingFromVehicleType = fScalingFromVehicleType;
	ms_RangeScale_fScriptedRangeMultiplier = fScriptedRangeMultiplier;
#endif

	//-------------------------------------------------------------------------------------------------------------
	// Now, we have to limit the range scale so that the outer band does not ever exceed the furthest reaches
	// of our loaded pathnodes - or we will of course get no vehicles generating there.
	//
	// Since band thickness is *dependent* upon this current range-scale calcultion, we shall just use the default
	// unscaled ms_popGenKeyholeOuterShapeThickness in the below calculation.  If the radius is to eventually
	// be scaled further then we may find it extends beyond the loaded pathnode distance (too bad!)

	float fOuterDist = (ms_popGenKeyholeShapeOuterBandRadius + ms_popGenKeyholeOuterShapeThickness) * rangeScale;
	float fNodeLoadDist = STREAMING_PATH_NODES_DIST_PLAYER + PATHFINDREGIONSIZEX;
	if(fOuterDist > fNodeLoadDist)
	{
		rangeScale *= (fNodeLoadDist / fOuterDist);
	}

	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, VIEW_RANGE_SCALE_MAX, 6.0f, 1.0f, 20.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_POPULATION, VIEW_RANGE_SCALE_SCOPE_MODIFIER, 0.4f, 0.0f, 1.0f, 0.1f);

    float maxRangeScale = ms_fMaxRangeScale;
	float maxViewRangeScale = VIEW_RANGE_SCALE_MAX;

    // ensure the range scale doesn't allow creation of vehicles outside of the vehicle scope distance in network games.
    // This can lead to problems where a vehicle is created at a spawn location, migrates to another machine and is then immediately
    // removed, which can lead to a chain reaction of vehicle creations at a point
    if(NetworkInterface::IsGameInProgress())
    {
        float reducedScopeDistance = CNetObjVehicle::GetStaticScopeData().m_scopeDistance - 5.0f;
        float maxNetworkRangeScale = reducedScopeDistance / ms_creationDistance;

        // if this ever fired the network game would be hopelessly broken, but this is included to 
        // indicate CNetObjVehicle::GetStaticScopeData().m_scopeDistance must always be much larger than
        // the hysteresis value is has been reduced by above
#if __ASSERT
        if(!PARAM_noFatalAssertOnVehicleScopeDistance.Get())
#endif // __ASSERT
        {
            FatalAssertf(reducedScopeDistance > 0.0f, "Network vehicle scope distance is much too small!");
        }

		maxRangeScale = Min(maxRangeScale, maxNetworkRangeScale);
		maxViewRangeScale = Min(maxViewRangeScale, maxNetworkRangeScale);
    }

	rangeScale = Min(rangeScale, maxRangeScale);

	float fRendererRangeMultiplier = GetVehicleLodScale();

	const camBaseCamera * pRenderCam = camInterface::GetDominantRenderedCamera();

	if((pRenderCam && pRenderCam->GetClassId() == camCinematicFirstPersonIdleCamera::GetStaticClassId())
		|| g_PlayerSwitch.IsActive())
	{
		fRendererRangeMultiplier = 1.0f;
	}

	const CPed* pPlayer = FindPlayerPed();

	bool bScopeActive = false;

	if(pPlayer && pPlayer->GetPlayerInfo()->IsAiming())
	{
		bScopeActive = pPlayer->GetWeaponManager() && pPlayer->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
	}

	const float fScopeModifier = bScopeActive ? VIEW_RANGE_SCALE_SCOPE_MODIFIER : 0.0f;

	float fViewRangeScale = (fRendererRangeMultiplier * fHeightRangeScale) + fScopeModifier;
	fViewRangeScale = Min(fViewRangeScale, maxViewRangeScale);
	
	// Pairing full circle link collection with a massive view range scale grinds the game to a halt
	if(g_PlayerSwitch.ShouldUseFullCircleForPopStreaming() || ms_bInstantFillPopulation)
	{
		fViewRangeScale = Min(fViewRangeScale, HEIGHT_RANGE_SCALE_MAX);
	}

	ms_fCalculatedPopulationRangeScale = rangeScale;
	ms_fCalculatedViewRangeScale = Max(fViewRangeScale / Max(rangeScale, 1.0f), 1.0f);
	ms_bZoomed = (ms_fCalculatedPopulationRangeScale * ms_fCalculatedViewRangeScale > ms_fGatherLinksUseKeyHoleShapeRangeThreshold) && ms_bEnableUseGatherLinksKeyhole;

#if __BANK
	ms_RangeScale_fHeightRangeScale = fHeightRangeScale;
	ms_RangeScale_fRendererRangeMultiplier = fRendererRangeMultiplier;
	ms_RangeScale_fScopeModifier = fScopeModifier;
#endif
}

void CVehiclePopulation::CalculateSpawnFrustumNearDist()
{
	//---------------------------------------------------------------

	CutSceneManager *pCutSceneManager = CutSceneManager::GetInstancePtr();
	const bool bCutScene = pCutSceneManager && pCutSceneManager->IsCutscenePlayingBack();

	if(bCutScene)
	{
		ms_fCalculatedSpawnFrustumNearDist = ms_popGenKeyholeShape.GetOuterBandRadiusMin();
	}
	else
	{
		ms_fCalculatedSpawnFrustumNearDist = ms_popGenKeyholeShape.GetOuterBandRadiusMin() * ms_spawnFrustumNearDistMultiplier;
	}
}

float CVehiclePopulation::GetRandomVehDensityMultiplier(float * pBasicMult, float * pThisFrameScriptedMult, float * pMultiplayerMult)
{
	const float fMult = ms_randomVehDensityMultiplier *
		ms_scriptRandomVehDensityMultiplierThisFrame *
			(CNetwork::IsGameInProgress() ? ms_TuningData.m_fMultiplayerRandomVehicleDensityMultiplier : 1.0f);

	if(pBasicMult)
		*pBasicMult = ms_randomVehDensityMultiplier;
	if(pThisFrameScriptedMult)
		*pThisFrameScriptedMult = ms_scriptRandomVehDensityMultiplierThisFrame;
	if(pMultiplayerMult)
		*pMultiplayerMult = (CNetwork::IsGameInProgress() ? ms_TuningData.m_fMultiplayerRandomVehicleDensityMultiplier : 1.0f);

	return fMult;
}

float CVehiclePopulation::GetParkedCarDensityMultiplier()
{
	const float fMult = ms_parkedCarDensityMultiplier *
		ms_scriptParkedCarDensityMultiplierThisFrame *
		(CNetwork::IsGameInProgress() ? ms_TuningData.m_fMultiplayerParkedVehicleDensityMultiplier : 1.0f);
	return fMult;
}

float CVehiclePopulation::GetLowPrioParkedCarDensityMultiplier()
{
	const float fMult = ms_lowPrioParkedCarDensityMultiplier *
		ms_scriptParkedCarDensityMultiplierThisFrame *
		(CNetwork::IsGameInProgress() ? ms_TuningData.m_fMultiplayerParkedVehicleDensityMultiplier : 1.0f);
	return fMult;
}

void CVehiclePopulation::StartAsyncUpdateDensitiesJob()
{
	if (ms_numGenerationLinks == 0)
	{
		return;
	}

	s_UpdateDensitiesDependency.m_Params[0].m_AsPtr = &s_UpdateDensitiesDependencyRunning;

	s_UpdateDensitiesDependencyRunning = 1;
	s_UpdateDensitiesDependencyLaunched = true;
	 
	sysDependencyScheduler::Insert( &s_UpdateDensitiesDependency );
}

void CVehiclePopulation::StartAsyncUpdateJob()
{    
	// don't let the range scale get blow up during player switches. We want nodes right around the player to spawn vehicles at
	const float rangeScale =  g_PlayerSwitch.IsActive() ? 1.0f :GetPopulationRangeScale();

    Assert(!s_VehiclePopulationDependencyRunning);

	// The 20 is to compensate for the 10 meter update delay and the fact it tests only one of the 2 nodes
    float maxNodeDist = GetCreationDistance(rangeScale) + 20.0f;
	//maxNodeDist = Min(maxNodeDist, 500.f);

	const Vector2 dir = ms_popGenKeyholeShape.GetDir();

	CVehiclePopulationAsyncUpdateData& p = s_VehiclePopulationUpdateJobData;

	p.popCtrlCentre = ms_popGenKeyholeShape.GetCenter();;
	p.sidewallFrustumOffset = ms_popGenKeyholeShape.GetSidewallFrustumOffset();
	p.maxNodeDistBase = ms_creationDistance;
	p.maxNodeDist = maxNodeDist;

	p.m_bUseKeyholeShape = ms_bEnableUseGatherLinksKeyhole;
	p.m_fKeyhole_InViewInnerRadius = ms_popGenKeyholeShape.GetOuterBandRadiusMin();
	p.m_fKeyhole_InViewOuterRadius = ms_popGenKeyholeShape.GetOuterBandRadiusMax();
	p.m_fKeyhole_BehindInnerRadius = ms_popGenKeyholeShape.GetInnerBandRadiusMin();
	p.m_fKeyhole_BehindOuterRadius = ms_popGenKeyholeShape.GetInnerBandRadiusMax();
	p.popCtrlDir = Vector3(dir.x, dir.y, 0.0f);
	p.m_fKeyhole_CosHalfFOV = ms_popGenKeyholeShape.GetCosHalfAngle();

	if(ms_bEnableUseGatherLinksKeyhole)
	{		
		p.m_fKeyhole_InViewOuterRadius = ms_popGenKeyholeShape.GetOuterBandRadiusMax() + 20.f; // don't grab links further than we're going to make them active, with a little padding
		p.m_fKeyhole_BehindInnerRadius = ms_popGenKeyholeShape.GetInnerBandRadiusMin();
		p.m_fKeyhole_BehindOuterRadius = ms_popGenKeyholeShape.GetInnerBandRadiusMax() + 20.f;
	}

	p.p_aGenerationLinks = &ms_aGenerationLinks[0];
	p.p_numGenerationLinks = &ms_numGenerationLinks;
	p.m_pAverageLinkHeight = &ms_averageLinkHeight;

	const Vector3& camFront = camInterface::GetFront();
	p.m_cambz = camFront.z;
	CVehicle* pPlayerVehicle = FindPlayerVehicle();
	p.m_playerInVehicle = (pPlayerVehicle != 0);
	p.m_playerInHeli = false;
	if (pPlayerVehicle)
	{
		p.m_moveSpeedX = pPlayerVehicle->GetVelocity().x;
		p.m_moveSpeedY = pPlayerVehicle->GetVelocity().y;
		if (pPlayerVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
			p.m_playerInHeli = true;
	}

	camFrame::ComputeNormalisedXYFromFront(camFront, p.m_camFrontXNorm, p.m_camFrontYNorm);
	p.m_FrameCount = fwTimer::GetSystemFrameCount();
	p.m_spawnCareAboutDeadEnds = ms_spawnCareAboutDeadEnds;
	p.m_allowSpawningFromDeadEndRoads = ms_allowSpawningFromDeadEndRoads;

	p.m_iNumOverlappingRoadAreas = ms_VehGenMarkUpSpheres.m_Spheres.GetCount();

	p.m_iNumTrafficAreas = CThePopMultiplierAreas::GetNumTrafficAreas();

	s_VehiclePopulationDependency.m_Params[0].m_AsPtr = &s_VehiclePopulationUpdateJobData;
	s_VehiclePopulationDependency.m_Params[1].m_AsPtr = s_numGenerationLinks;
	s_VehiclePopulationDependency.m_Params[2].m_AsPtr = &s_VehiclePopulationDependencyRunning;


	s_VehiclePopulationDependencyRunning = 1;

	sysDependencyScheduler::Insert( &s_VehiclePopulationDependency );	
}

float CVehiclePopulation::GetDesiredNumberAmbientVehicles(int * outUpperLimit, float * outDesertedStreetsMult, float * outInteriorMult, float * outWantedMult, float * outCombatMult, float * outHighwayMult, float * outBudgetMult)
{
	const s32 iUpperLimit = NetworkInterface::IsGameInProgress() ? ms_iAmbientVehiclesUpperLimitMP : ms_iAmbientVehiclesUpperLimit;

	float fDesiredNumCars = (float)iUpperLimit;
	if(NetworkInterface::IsGameInProgress())
	{
		//don't use wantedCar, combatCar or desertedStreet multipliers in multiplayer as there's always fights going on somewhere nearby
		fDesiredNumCars =	((float)iUpperLimit) *
							CPopCycle::GetInteriorCarMultiplier() *
							CPopCycle::CalcHighWayCarMultiplier() *
							CPopCycle::GetWantedCarMultiplier() *
							ms_vehicleMemoryBudgetMultiplier;

		if(outUpperLimit)
			*outUpperLimit = iUpperLimit;
		if(outDesertedStreetsMult)
			*outDesertedStreetsMult = 1.0f;
		if(outInteriorMult)
			*outInteriorMult = CPopCycle::GetInteriorCarMultiplier();
		if(outWantedMult)
			*outWantedMult = CPopCycle::GetWantedCarMultiplier();
		if(outCombatMult)
			*outCombatMult = 1.0f;
		if(outHighwayMult)
			*outHighwayMult = CPopCycle::CalcHighWayCarMultiplier();
		if(outBudgetMult)
			*outBudgetMult = ms_vehicleMemoryBudgetMultiplier;
	}
	else
	{
		fDesiredNumCars =	((float)iUpperLimit) *
							Min( GetDesertedStreetsMultiplier(), CPopCycle::GetWantedCarMultiplier() ) *
							CPopCycle::GetInteriorCarMultiplier() *
							CPopCycle::GetCombatCarMultiplier() *
							CPopCycle::CalcHighWayCarMultiplier() *
							ms_vehicleMemoryBudgetMultiplier;

		if(outUpperLimit)
			*outUpperLimit = iUpperLimit;
		if(outDesertedStreetsMult)
			*outDesertedStreetsMult = GetDesertedStreetsMultiplier();
		if(outInteriorMult)
			*outInteriorMult = CPopCycle::GetInteriorCarMultiplier();
		if(outWantedMult)
			*outWantedMult = CPopCycle::GetWantedCarMultiplier();
		if(outCombatMult)
			*outCombatMult = CPopCycle::GetCombatCarMultiplier();
		if(outHighwayMult)
			*outHighwayMult = CPopCycle::CalcHighWayCarMultiplier();
		if(outBudgetMult)
			*outBudgetMult = ms_vehicleMemoryBudgetMultiplier;
	}

	fDesiredNumCars = rage::Min( fDesiredNumCars, ((float)iUpperLimit) );
	return fDesiredNumCars;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNumberOfRandomCarsThatCanBeCreated
// PURPOSE :  This can not be used for parked cars. (Only for the ones moving)
/////////////////////////////////////////////////////////////////////////////////
s32 CVehiclePopulation::FindNumRandomVehsThatCanBeCreated()
{
	TUNE_GROUP_INT(VEHICLE_POPULATION, MinNumPopulationToScaleMissionReduction, 20, 0, 1000, 1);
	TUNE_GROUP_INT(VEHICLE_POPULATION, MaxNumPopulationToScaleMissionReduction, 80, 0, 1000, 1);
	s32	result = 999;
	s32	TotalCarsOnMap = GetTotalVehsOnMap();
	s32	TotalAmbientCarsOnMap = ms_numRandomCars;

	// This value is supposed to have been set during the initialization process.
	Assertf(ms_maxNumberOfCarsInUse, "ms_maxNumberOfCarsInUse not set properly.");

	result = rage::Min(result, (ms_maxNumberOfCarsInUse - TotalCarsOnMap));

#if __BANK
	if (ms_overrideNumberOfCars >= 0)
	{
		if (TotalCarsOnMap >= ms_overrideNumberOfCars)
		{
			result = rage::Min(result, (ms_overrideNumberOfCars - TotalCarsOnMap));
		}
		return result;
	}
#endif

	float fDesiredNumCars = GetDesiredNumberAmbientVehicles();

	// Reduce the vehicle population by a proportion of the number of mission cars depending how many cars we want, if we only want a small number dont reduce them at all
	float fReduction =(fDesiredNumCars-(float)MinNumPopulationToScaleMissionReduction)/(float)(MaxNumPopulationToScaleMissionReduction-MinNumPopulationToScaleMissionReduction);
	fReduction = rage::Clamp(fReduction, 0.0f, 1.0f);
	const s32 PropMissionCars = (s32)(fReduction*((float)ms_numMissionCars));

	result = rage::Min(result, ( ((s32)fDesiredNumCars) - (TotalAmbientCarsOnMap+PropMissionCars)));

	result = rage::Min(result, CPopCycle::GetTemporaryMaxNumCars());

	//-----------------------------------------------------------------------------------------------------------

	if (NetworkInterface::IsGameInProgress())
	{
        result += (s32)(NetworkInterface::GetNumVehiclesOutsidePopulationRange() * CVehiclePopulation::GetTotalRandomVehDensityMultiplier());
	}

	vehPopDebugf3("Can create %d vehicles. desired: %.2f, density multiplier: %f, deserted multiplier: %f, interior multiplier: %f, wanted multiplier: %f, combat multiplier: %f, highway multiplier: %f, memory multiplier: %f",
		result, fDesiredNumCars, CVehiclePopulation::GetTotalRandomVehDensityMultiplier(),
		GetDesertedStreetsMultiplier(), CPopCycle::GetInteriorCarMultiplier(), CPopCycle::GetWantedCarMultiplier(), CPopCycle::GetCombatCarMultiplier(), CPopCycle::CalcHighWayCarMultiplier(), ms_vehicleMemoryBudgetMultiplier);
	
	//-----------------------------------------------------------------------------------------------------------
	
	// if the scripters have placed down and activated custom areas that alter the ped and traffic density.....
	if(CThePopMultiplierAreas::GetNumTrafficAreas() > 0)
	{
		float trafficMultiplier = 1.0f;

		if(gVpMan.GetCurrentViewport())
		{
			const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
			const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());

			// Get the traffic multiplier from the cam pos (i.e go through all areas that contain the cam pos and multiply through)...
			trafficMultiplier = CThePopMultiplierAreas::GetTrafficDensityMultiplier(camPos, true);
		}

		result = (s32)((float)result * trafficMultiplier);
	}

	//-----------------------------------------------------------------------------------------------------------

	if (NetworkInterface::IsGameInProgress())
	{
		if(GetTotalNonMissionVehsOnMap() >= MAX_NUM_NETOBJVEHICLES)
		{
			Assertf(0, "There are too many ambient vehicles on the map in MP game. Are we leaking vehicles?");
			BANK_ONLY(DumpVehiclePoolToLog());
			return 0;
		}
	}

	return result;
}

float CVehiclePopulation::GetVisibleCloneCreationDistance()
{
	float fVisibleCreationDistance = rage::Lerp(ms_CloneSpawnValidationMultiplier, ms_NetworkVisibleCreationDistMax, ms_NetworkVisibleCreationDistMin);

	if(ms_NetworkTimeExtremeVisibilityFail > 0)
	{
		float fTimeScale = ((float)((fwTimer::GetSystemTimeInMilliseconds() - ms_NetworkTimeExtremeVisibilityFail))) / ((float)(((ms_NetworkTimeExtremeVisibilityFail + ms_NetworkMaxTimeVisibilityFail) - ms_NetworkTimeExtremeVisibilityFail)));
		fTimeScale = Clamp(fTimeScale, 0.0f, 1.0f);
		fVisibleCreationDistance = rage::Lerp(fTimeScale, ms_FallbackNetworkVisibleCreationDistMax, ms_FallbackNetworkVisibleCreationDistMin);
	}

	return fVisibleCreationDistance;
}

void CVehiclePopulation::RegisterCloneSpawnRejection()
{
	ms_CloneSpawnValidationMultiplier = Min(ms_CloneSpawnValidationMultiplier + ms_CloneSpawnMultiplierFailScore, 1.0f);

	//! Cache last time we failed to spawn a vehicle due to clone rejection.
	ms_NetworkTimeVisibilityFail = fwTimer::GetSystemTimeInMilliseconds();

	ms_NetworkVisibilityFailCount++;

	//! Cache time we last started to hit extreme pop failures.
	if (!ms_NetworkTimeExtremeVisibilityFail && (ms_CloneSpawnValidationMultiplier > (1.0f - SMALL_FLOAT)) )
		ms_NetworkTimeExtremeVisibilityFail = fwTimer::GetSystemTimeInMilliseconds();
}

bool CVehiclePopulation::CanRejectNetworkSpawn(const Vector3 &vPosition, float fRadius, bool &bForceFadeIn)
{
	// don't create vehicles in view of or close to any other network players
	float fVisibleCreationDistance = GetVisibleCloneCreationDistance();
	float fOffscreenCreationDistance = rage::Lerp(ms_CloneSpawnValidationMultiplier, ms_NetworkOffscreenCreationDistMax, ms_NetworkOffscreenCreationDistMin);

	if (NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vPosition, fRadius, fVisibleCreationDistance, fOffscreenCreationDistance, NULL, ms_NetworkUsePlayerPrediction, true))
	{
		RegisterCloneSpawnRejection();

		if (AllowNetworkSpawnOverride(vPosition))
		{
			//Fade the vehicle in
			bForceFadeIn = true;
		}
		else
		{
			return true;
		}
	}
	else
	{
		ResetNetworkVisibilityFail();
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AllowNetworkSpawnOverride
// PURPOSE :  This method is used to determine if we should override the system
//   that prevents vehicles from spawning in multiplayer if they are too close or visible.
//   We need to override this under some conditions where there are multiple overlapping
//   visibility cones, otherwise we will never get vehicles spawning.  
//   But, we do want to prevent vehicles from spawning in the general case.
//   So, we have an override...
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::AllowNetworkSpawnOverride(const Vector3& vehicleposition)
{
	if(ms_NetworkVisibilityFailCount > ms_NetworkMinVehiclesToConsiderOverrideMin)
	{
		if(ms_NetworkTimeExtremeVisibilityFail > 0 && (fwTimer::GetSystemTimeInMilliseconds() > (ms_NetworkTimeExtremeVisibilityFail + ms_NetworkMaxTimeVisibilityFail) ))
		{
			//! Don't pull right down until we absolutely have to, start pulling in range bit by bit.
			float fOverrideScale = ((float)((ms_NetworkVisibilityFailCount - ms_NetworkMinVehiclesToConsiderOverrideMin))) / ((float)((ms_NetworkMinVehiclesToConsiderOverrideMax -ms_NetworkMinVehiclesToConsiderOverrideMin)));
			fOverrideScale = Clamp(fOverrideScale, 0.0f, 1.0f);
			float fOverrideDist = rage::Lerp(fOverrideScale, ms_FallbackNetworkVisibleCreationDistMin, ms_NetworkCreationDistMinOverride);
			const netPlayer* pClosest = NULL;
			if (!NetworkInterface::IsCloseToAnyPlayer(vehicleposition, fOverrideDist, pClosest))
			{
				vehPopDebugf2("CVehiclePopulation::AllowNetworkSpawnOverride ms_NetworkVisibilityFailCount[%d] ms_NetworkTimeVisibilityFail[%u] time[%u] distance[%f]",ms_NetworkVisibilityFailCount,ms_NetworkTimeVisibilityFail,fwTimer::GetSystemTimeInMilliseconds(),ms_NetworkCreationDistMinOverride);
				return true;
			}
		}
	}

	return false;
}

void CVehiclePopulation::ResetNetworkVisibilityFail()
{ 
	vehPopDebugf2("ResetNetworkVisibilityFail");
	static dev_bool bDontResetFailCount = false;
	if(!bDontResetFailCount)
	{
		ms_NetworkVisibilityFailCount = 0; 
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TryToGenerateOneRandomVeh
// PURPOSE :  This function works out where to place a new random cars and does so.
//			  A maximum of one car is generated each time it's called
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::TryToGenerateOneRandomVeh(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale)
{
#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker genRandomVeh("TryToGenerateOneRandomVehicle", 20);
#endif
	BANK_ONLY(utimer_t startTime = sysTimer::GetTicks());
	Assert(!s_VehiclePopulationDependencyRunning);

	float 				Fraction=0.0f;//, Length;
	CNodeAddress 		fromNodeAddr, toNodeAddr;
	Vector3 			Result;
	bool				bAmbientBoat = false;
	int					iVehGenLinkIndex = -1;

	if (!gPopStreaming.IsFallbackPedAvailable())
	{
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "we don't have any ped models available");
		RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, popCtrlCentre, Fail_FallbackPedNotAvailable, m_populationFailureData.m_numFallbackPedNotAvailable));
		return;			// If we don't have any peds available to drive the vehs we won't generate them.
	}

	#define NUM_LINKS_CHECKED (5)
	s32 iGenerationAttempts = 0;
	while (iGenerationAttempts < NUM_LINKS_CHECKED)
	{
		//several (network) checks below will fail due to short term reasons. To ensure we don't
		//get stuck checking the same links each time the links are updated we allow 5 links per frame to be checked
		++iGenerationAttempts;

		STRVIS_AUTO_CONTEXT(strStreamingVisualize::VEHICLEPOP);

		#define NUM_TRIES_FOR_PLACEMENT (4)
		bool bFoundOne = false;
		bool bKeepTrying = true;
		s32 Tries = 0;
		while ((!bFoundOne && bKeepTrying) && Tries < NUM_TRIES_FOR_PLACEMENT)
		{
			bFoundOne = GenerateVehCreationPosFromLinks(&Result, &fromNodeAddr, &toNodeAddr, &Fraction, &iVehGenLinkIndex, &bKeepTrying, rangeScale);
			Tries++;
		}

		if (!bFoundOne)
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Failed to find gen position for vehicle");
			vehPopDebugf3("Failed to find gen position for vehicle");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, popCtrlCentre, Fail_NoCreationPositionFound, m_populationFailureData.m_numNoCreationPosFound));
			return;
		}

		Assert(iVehGenLinkIndex!=-1);

		FatalAssertf(ms_aGenerationLinks[iVehGenLinkIndex].m_iDensity > 0, "ms_aGenerationLinks[iVehGenLinkIndex].m_iDensity > 0\n\tiVehGenLinkIndex: %i\n\tms_aGenerationLinks[iVehGenLinkIndex].m_iInitialDensity: %i\n\tms_aGenerationLinks[iVehGenLinkIndex].m_iDensity: %i\n\tms_iNumActiveLinks: %i\n\tms_iCurrentActiveLink: %i\n\tms_aActiveLinks[ms_iCurrentActiveLink]: %i", iVehGenLinkIndex, ms_aGenerationLinks[iVehGenLinkIndex].m_iInitialDensity, ms_aGenerationLinks[iVehGenLinkIndex].m_iDensity, ms_iNumActiveLinks, ms_iCurrentActiveLink, ms_aActiveLinks[ms_iCurrentActiveLink]);
		Assert(fromNodeAddr != toNodeAddr);
		Assert(ThePaths.IsRegionLoaded(fromNodeAddr));
		Assert(ThePaths.IsRegionLoaded(toNodeAddr));

#if VEHGEN_STORE_DEBUG_INFO_IN_LINKS
		ms_aGenerationLinks[iVehGenLinkIndex].m_iFromIndex = (u16)fromNodeAddr.GetIndex();
		ms_aGenerationLinks[iVehGenLinkIndex].m_iToIndex = (u16)toNodeAddr.GetIndex();
#endif

		const CPathNode* pFromNode = ThePaths.FindNodePointerSafe(fromNodeAddr);
		const CPathNode* pToNode = ThePaths.FindNodePointerSafe(toNodeAddr);

#if __BANK
		Vector3 fromPosTemp;
		pFromNode->GetCoors(fromPosTemp);
		Vector3 toPosTemp;
		pToNode->GetCoors(toPosTemp);
		const Vector3 approximatePos = (fromPosTemp + toPosTemp) * 0.5f;
#endif // __BANK

		if(pFromNode->IsSwitchedOff() || pToNode->IsSwitchedOff())
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Node(s) switched off");
			vehPopDebugf2("Chosen node(s) are switched off");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_NodesSwitchedOff, m_populationFailureData.m_numNodesSwitchedOff));

			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
			return;	
		}

		s16 iLink = -1;
		const bool bFoundLink = ThePaths.FindNodesLinkIndexBetween2Nodes(pFromNode, pToNode, iLink);

		if(!bFoundLink)
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Failed to find link between nodes");
			vehPopDebugf2("Failed to find link between nodes");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_LinkBetweenNodesNotFound, m_populationFailureData.m_numNoLinkBetweenNodes));

			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
			return;	
		}

		const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pFromNode, iLink);
		const s32 TempLanesOurWay = pLink->m_1.m_LanesToOtherNode;

#if __BANK
		if(TempLanesOurWay == 0 && ms_spawnCareAboutLanes)
		{
			ms_aGenerationLinks[iVehGenLinkIndex].m_iFailType = Fail_AgainstFlowOfTraffic;
		}
#endif

		// If we are in a network game multiple players can try to create a vehicle at the same position
		// at the same time. To avoid this we time-slice when players are allowed to create vehicles based
		// on the number of players who could currently create a vehicle in the returned position
		if(NetworkInterface::IsGameInProgress())
		{
            if (!IsInScopeOfLocalPlayer(Result))
            {
                NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "TryToGenerateOneRandomVeh", "Failed:", "spawn pos is not in scope of the local player");
                RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_OutOfLocalPlayerScope, m_populationFailureData.m_numOutOfLocalPlayerScope));
                continue;
            }

			if(!IsNetworkTurnToCreateVehicleAtPos(Result))
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "IT IS NOT NetworkTurnToCreateVehicleAtPos");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_NotNetworkTurnToCreateVehicleAtPosition, m_populationFailureData.m_numNotNetworkTurnToCreateVehicleAtPos));
				
				//don't disable this link as that'll prevent spawn for 2 seconds which is much longer than network creation turn time
				//Instead just increase ms_iCurrentActiveLink and continue so that next link update we can check it again straight away
				++ms_iCurrentActiveLink;
				continue;
			}
		}

		// If both nodes are on the same street, then take a note of the name hash
		const u32 iStreetNameHash = (pFromNode->m_streetNameHash == pToNode->m_streetNameHash) ? pFromNode->m_streetNameHash : 0;

		// Pick veh from percentages in zone (could still be a police veh)
		bool bOnHighWay = (pFromNode->IsHighway() && pToNode->IsHighway());
		bool offroad = pFromNode->IsOffroad() && pToNode->IsOffroad();
		bool bNoBigVehicles = pFromNode->BigVehiclesProhibited() && pToNode->BigVehiclesProhibited();
		bool bSmallWorkers = pFromNode->m_1.m_specialFunction == SPECIAL_SMALL_WORK_VEHICLES;
		bool bPolice = false;
		bool bSlipLane = pFromNode->IsSlipLane() || pToNode->IsSlipLane();

		if (bOnHighWay)
		{
			gPopStreaming.SpawnedVehicleOnHighway();
		}

		//don't allow creation of big vehicles in sliplanes, even if the road were otherwise good
		//vehicles will never choose to enter one of these, so don't start them there
		bNoBigVehicles |= bSlipLane;

		CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();
		CVehicleModelInfo* pModelInfo = NULL;
		bool reuseVehicle = false;
		s32 reusedVehicleIndex = -1;
		u32 vehModel = fwModelId::MI_INVALID;
		bool bUseTrailer = false;

#if 0
		u32 preferedMi = ms_aGenerationLinks[iVehGenLinkIndex].m_preferedMi;
		if (CModelInfo::IsValidModelInfo(preferedMi) && CModelInfo::HaveAssetsLoaded(fwModelId(preferedMi)) && !CVehicle::IsLawEnforcementVehicleModelId(fwModelId(preferedMi)))
		{
			CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(preferedMi));
			if (vmi)
			{
				if (gPopStreaming.GetAppropriateLoadedCars().IsMemberInGroup(preferedMi) &&
					!CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(preferedMi) &&
					(vmi->GetNumVehicleModelRefs() < vmi->GetVehicleMaxNumber()) &&
					(!bSmallWorkers || vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SMALL_WORKER)) &&
					(!bOnHighWay || vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ONLY_ON_HIGHWAYS)) &&
					(!offroad || vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_OFFROAD_VEHICLE)) &&
					(!bNoBigVehicles || !vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_AVOID_TURNS)))
				{
					vehPopDebugf2("Choosing model '%s' to spawn based on distance to all vehicles check! (closest dist: %.2f)", vmi->GetModelName(), rage::Sqrtf(vmi->GetDistanceSqrToClosestInstance(Result)));
					vehModel = preferedMi;

					Assertf(gPopStreaming.IsVehicleInAllowedNetworkArray(vehModel), "Spawning bad vehicle type in MP game (preferred model for link)");
				}
			}
		}

		if (!CModelInfo::IsValidModelInfo(vehModel))
#endif
		{
			// Choose a model
			vehModel = ChooseModel(&bPolice, Result, pWanted ? (pWanted->GetWantedLevel() >= WANTED_LEVEL1) : false, bSmallWorkers, bOnHighWay, iStreetNameHash, offroad, bNoBigVehicles, TempLanesOurWay);
		
			// if we have a model, grab the model info for our trailer checks
			if(CModelInfo::IsValidModelInfo(vehModel))
			{
				fwModelId vehModelId((strLocalIndex(vehModel)));
				pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(vehModelId);
			}

			const s32 iNumRandomVehsThatCanBeCreated = FindNumRandomVehsThatCanBeCreated();

			// we need to decide now if we're going to use a trailer for this model
			if(pModelInfo && iNumRandomVehsThatCanBeCreated > 1 )
			{
				bool narrowRoad = pLink->m_1.m_NarrowRoad;
				bool canAttachTrailer = bOnHighWay && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ATTACH_TRAILER_ON_HIGHWAY);
				canAttachTrailer |= !narrowRoad && !bNoBigVehicles && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ATTACH_TRAILER_IN_CITY);

				if(canAttachTrailer)
				{
					// non highway trailers have a 50% chance of spawning while highway trailers have 75%, too see more of them.
					// 75% for the non highway once was a bit much and you'd see too many of them
					float diceRollThreshold = 0.25f;
					if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ATTACH_TRAILER_IN_CITY))
						diceRollThreshold = 0.5f;

					if(fwRandom::GetRandomNumberInRange(0.f, 1.f) > diceRollThreshold)
					{
						bUseTrailer = true; // going to attach a trailer
					}
				}
			}

			// see if we have a valid model in the reuse cache	
			if(!bUseTrailer && ms_bVehicleReusePool) // don't use the reuse pool if we want to spawn a trailer on the vehicle
			{
				reusedVehicleIndex = FindVehicleToReuse(vehModel);
				if(reusedVehicleIndex != -1)
				{
					reuseVehicle = true;
				}
			}
		}

		if(!CModelInfo::IsValidModelInfo(vehModel))
		{
			vehPopDebugf2("No appropriate vehicle model found to spawn");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "No appropriate vehicle model found to spawn");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_NoAppropriateModel, m_populationFailureData.m_numNoVehModelFound));
			return;
		}

		if (vehModel == ms_lastChosenVehicleModel && ms_spawnCareAboutRepeatingSameModel)
		{
			if ( ms_sameVehicleSpawnCount >= ms_sameVehicleSpawnLimit && fwRandom::GetRandomNumberInRange(0.f, 1.f) > ms_spawnLastChosenVehicleChance)
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Same vehicle model as last time");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_SameVehicleModelAsLastTime, m_populationFailureData.m_sameVehicleModelAsLastTime));
				return;
			}

			ms_sameVehicleSpawnCount++;
		}
		else
		{
			ms_sameVehicleSpawnCount = 0;
			ms_lastChosenVehicleModel = vehModel;
		}

		bool canCreateVeh = true;
		if(NetworkInterface::IsGameInProgress())
		{
				  u32 numRequiredPeds    = 1;
			const u32 numRequiredVehs    = 1;
			const u32 numRequiredObjects = 0;

			if(bPolice)
			{
				numRequiredPeds = CVehiclePopulation::GetNumRequiredPoliceVehOccupants(vehModel);
			}

			CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldVehicleGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
			CNetworkObjectPopulationMgr::ePedGenerationContext eOldPedGenContext = CNetworkObjectPopulationMgr::GetPedGenerationContext();
			CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_AmbientPopulation);

			if(numRequiredPeds == 1)
			{
				CNetworkObjectPopulationMgr::SetPedGenerationContext(CNetworkObjectPopulationMgr::PGT_AmbientDriver);
			}

			canCreateVeh = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredVehs, numRequiredObjects, 0, 0, false);

			CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldVehicleGenContext);
			CNetworkObjectPopulationMgr::SetPedGenerationContext(eOldPedGenContext);

			if(numRequiredVehs > CVehicle::GetPool()->GetNoOfFreeSpaces())
			{
				Assertf(0, "Trying to create more vehicles that we have in the pool");
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Not enough free vehicles in pool!");
				BANK_ONLY(DumpVehiclePoolToLog());
				canCreateVeh = false;
			}

#if __DEV
			if(!canCreateVeh)
			{
				if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, numRequiredPeds, false))
					NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Not enough peds.");
				else if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_AUTOMOBILE, numRequiredVehs, false))
					NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Not enough vehicles.");
			}
#endif // __DEV
		}

		if(!canCreateVeh)
		{
			vehPopDebugf2("Can't create vehicle for net game");
			gnetDebug2("Failed to create vehicle due to network population limit - Reason: %s", CNetworkObjectPopulationMgr::GetLastFailTypeString());
			BANK_ONLY(m_populationFailureData.m_numNetworkPopulationFail++);
			DEV_ONLY(VecMapDisplayVehiclePopulationEvent(approximatePos, VehPopFailedCreate));
			BANK_ONLY(ms_aGenerationLinks[iVehGenLinkIndex].m_iFailType = Fail_NetworkRegister);
			return;
		}

		// If this node is a 'waternode' we replace the veh with a boat
		bool bIsBoat = false;
		if (pFromNode->m_2.m_waterNode)
		{
			bIsBoat = true;
			vehModel = fwModelId::MI_INVALID;		// Can't use the veh model we found before since this is a proper veh and we want a boat.

			bool bAllowRandomBoats = NetworkInterface::IsGameInProgress() ? ms_bAllowRandomBoatsMP : ms_bAllowRandomBoats;
			if (!bAllowRandomBoats)
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, no random boats allowed");
				vehPopDebugf2("Skip vehicle spawn, random boats not allowed");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_Boat, m_populationFailureData.m_numRandomBoatsNotAllowed));

				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				return;
			}

			// Choose a boat model.
			bAmbientBoat = true;
			s32	numBoatsOfEachType = (s32)(1.1f * CVehiclePopulation::GetTotalRandomVehDensityMultiplier());
			if (numBoatsOfEachType > 0)
			{
				CLoadedModelGroup useableBoats;
				useableBoats.Copy(&gPopStreaming.GetLoadedBoats());
				if (pFromNode->m_2.m_highwayOrLowBridge)		// Near low bridges we don't create tall ships
				{
					useableBoats.RemoveVehModelsWithFlagSet(CVehicleModelInfoFlags::FLAG_TALL_SHIP);
				}

				vehModel = useableBoats.PickLeastUsedModel(numBoatsOfEachType-1);
			}
			fwModelId vehModelId((strLocalIndex(vehModel)));
			if (!vehModelId.IsValid() || !CModelInfo::HaveAssetsLoaded(vehModelId))
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, requested boat model not in memory");
				vehPopDebugf2("Skip vehicle spawn, requested boat model not in memory");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_Boat, m_populationFailureData.m_numChosenBoatModelNotLoaded));

				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				return;
			}
		}

		// Go on, give us a car (or bike) (or boat)......
		fwModelId vehModelId((strLocalIndex(vehModel)));
		if( !aiVerifyf(CModelInfo::HaveAssetsLoaded(vehModelId), "TryToGenerateOneRandomVeh: model requested, but not loaded!" ) )
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, requested vehicle model not in memory");
			vehPopDebugf2("Skip vehicle spawn, requested vehicle model not in memory");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_NotStreamedIn, m_populationFailureData.m_numChosenModelNotLoaded));
			return;
		}

		CNodeAddress aNodeAddr[3];
		aNodeAddr[0].SetEmpty();
		aNodeAddr[1] = fromNodeAddr;
		aNodeAddr[2] = toNodeAddr;

		s16 aNodeLinks[3];
		aNodeLinks[0] = -1;
		ThePaths.FindNodesLinkIndexBetween2Nodes(fromNodeAddr, toNodeAddr, aNodeLinks[1]);
		if(aNodeLinks[1] != -1)
			aNodeLinks[1] = (s16)ThePaths.GetNodesRegionLinkIndex(pFromNode, aNodeLinks[1]); // Get link index within region
		aNodeLinks[2] = -1;

		Matrix34 vehMatrix;
		float	ExtendedRemovalRange = 0.0f;
		Vector3 oldCoors, newCoors;

		s16 regionLinkIndexVeryOldToOld, regionLinkIndexOldToNew = (pFromNode->m_startIndexOfLinks + iLink);

		// We make sure the veh is a minimum distance away from either one of the nodes.
		// This is to stop buses and stuff from being created partially on sidewalks and inside buildings
		const Vector3& vBoundBoxMin = (CModelInfo::GetBaseModelInfo(vehModelId))->GetBoundingBoxMin();
		const Vector3& vBoundBoxMax = (CModelInfo::GetBaseModelInfo(vehModelId))->GetBoundingBoxMax();
		float	MinDistAwayFromNodeBack = -vBoundBoxMin.y;
		float	MinDistAwayFromNodeForward = vBoundBoxMax.y;
		
		if (ThePaths.FindNodePointer(aNodeAddr[2])->IsJunctionNode())
		{
			MinDistAwayFromNodeForward += 5.0;
		}

		ThePaths.FindNodePointer(aNodeAddr[1])->GetCoors(oldCoors);
		ThePaths.FindNodePointer(aNodeAddr[2])->GetCoors(newCoors);

		float	NodesDist = rage::Sqrtf( (oldCoors.x - newCoors.x) * (oldCoors.x - newCoors.x) +
										  (oldCoors.y - newCoors.y) * (oldCoors.y - newCoors.y) );

		Fraction = rage::Max(Fraction, MinDistAwayFromNodeBack / NodesDist);
		Fraction = rage::Min(Fraction, 1.0f - MinDistAwayFromNodeForward / NodesDist);
		Fraction = rage::Max(0.0f, Fraction);
		Fraction = rage::Min(1.0f, Fraction);
		vehMatrix.d =	newCoors * Fraction + oldCoors * (1.0f - Fraction);

		// Work out what speed the vehicle will be going at.
		// We need to know this here so that we can pick the appropriate lane.
		float	CruiseSpeed = PickCruiseSpeedWithVehModel(NULL, vehModel);

		// If there already is a random boat within a certain range we don't create another.
		if (bAmbientBoat)
		{
			if (IsAmbientBoatNearby(vehMatrix.d, 70.0f))
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, ambient boat requested to close to other boat");
				vehPopDebugf2("Skip vehicle spawn, ambient boat requested to close to other boat");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_AmbientBoatAlreadyHere, m_populationFailureData.m_numBoatAlreadyHere));

				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				return;
			}
		}

		if (pLink->IsSingleTrack() && ms_spawnEnableSingleTrackRejection)
		{
			if (IsVehicleFacingPotentialHeadOnCollision(vehMatrix.d, newCoors - oldCoors, pLink, CruiseSpeed*2.5f))
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, IsVehicleFacingPotentialHeadOnCollision");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_SingleTrackPotentialCollision, m_populationFailureData.m_numSingleTrackRejection));

				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				return;
			}
		}

		// Also pick a node that we were coming from (just pick one at random really).
		const CPathNode* pNode1Temp = ThePaths.FindNodePointer(aNodeAddr[1]);
		Assert(pNode1Temp->NumLinks() > 0);		// don't want no isolated nodes
		if (pNode1Temp->NumLinks() == 1 && ms_spawnCareAboutDeadEnds)
		{	
			// We only have one choice (this must be a dead end)
			// We don't want to generate vehs at the end of dead end roads.
			// This would result in the oldNode and newNode being identical and this could cause a crash later on.
			vehPopDebugf2("Skip vehicle spawn, vehicle would be placed on dead end road");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, vehicle would be placed on dead end road");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_DeadEnd, m_populationFailureData.m_numDeadEndsChosen));

			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
			return;
		}
		else
		{	// Pick a node at random
			aNodeAddr[0] = aNodeAddr[2];
			while (aNodeAddr[0] == aNodeAddr[2])
			{
				const CPathNode* pNode1 = ThePaths.FindNodePointer(aNodeAddr[1]);
				Assert(pNode1);
				s32 RandomLinkIndex = fwRandom::GetRandomNumber() % pNode1->NumLinks();
				aNodeAddr[0] = ThePaths.GetNodesLinkedNodeAddr(pNode1, RandomLinkIndex);
				if (!ThePaths.IsRegionLoaded(aNodeAddr[0]))
				{
					NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, neighboring region is not loaded");
					vehPopDebugf2("Skip vehicle spawn, neighboring region is not loaded");
					RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_RegionNotLoaded, m_populationFailureData.m_numNoPathNodesAvailable));

					TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
					return;		// If the neighbouring region is not loaded we jump out.
				}
			}
			regionLinkIndexVeryOldToOld = (s16)(ThePaths.FindRegionLinkIndexBetween2Nodes(aNodeAddr[0], aNodeAddr[1]));
			if(regionLinkIndexVeryOldToOld == -1)
			{
				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				return;
			}
			aNodeLinks[0] = regionLinkIndexVeryOldToOld;
		}

		// We have to aim the veh in the direction it will be going. Otherwise
		// we get all sorts of nasty maneuver when they are created.
		// Simply use the nodes for that.
		Vector3 Dir = newCoors - oldCoors;

#if __BANK
		if (ms_bEverybodyCruiseInReverse)
		{
			Dir = -Dir;
		}
#endif //__BANK

		if (Dir.Mag2() < SMALL_FLOAT)
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "TryToGenerateOneRandomVeh", "Failed:", "Link direction invalid");
			vehPopDebugf2("Skip vehicle spawn, link direction invalid");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_LinkDirectionInvalid, m_populationFailureData.m_numLinkDirectionInvalid));
			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
			return;
		}

		Dir.NormalizeSafe(Vector3(0.0f,1.0f,0.0f));
		vehMatrix.b = Dir;

		vehMatrix.a = Dir;
		vehMatrix.a.Cross(Vector3(0.0f,0.0f,1.0f));
		vehMatrix.a.Normalize();

		vehMatrix.c = vehMatrix.a;
		vehMatrix.c.Cross(Dir);
		vehMatrix.c.Normalize();

		// Pick a lane
		u8 LaneIndex = static_cast<u8>( fwRandom::GetRandomNumberInRange(0, TempLanesOurWay) );

		//if this is a big vehicle and we have 3 or more lanes, randomly choose a different one
		if (LaneIndex == 0 && TempLanesOurWay > 2)	
		{
			CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(vehModelId);
			if (vmi && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG))
			{
				LaneIndex = static_cast<u8>( fwRandom::GetRandomNumberInRange(1, TempLanesOurWay) );
			}
		}

		//if this vehicle wants to avoid turns, and this is a 2-lane road that's not a highway,
		//favor being in the left lane. we'll get passed, sure, but within the city the chances
		//of being spawned in a right-turn only lane is high, whereas most left turns are hidden
		//away in slip lanes
		if (LaneIndex == 1 && TempLanesOurWay == 2 && !pFromNode->IsHighway() && !pToNode->IsHighway())	
		{
			CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(vehModelId);
			if (vmi && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_AVOID_TURNS))
			{
				LaneIndex = static_cast<u8>(0 );
			}
		}

		// Calculate the lane offset.
		CPathNodeLink* pLinkOldToNew = ThePaths.FindLinkPointerSafe(aNodeAddr[1].GetRegion(),regionLinkIndexOldToNew);
	
		float laneCentreOffset = ThePaths.GetLinksLaneCentreOffset(*pLinkOldToNew, LaneIndex);

		if (LaneIndex == TempLanesOurWay - 1)
		{
			laneCentreOffset -= pLinkOldToNew->GetLaneWidth() * 0.125f;
		}

#if __BANK
		if (ms_bEverybodyCruiseInReverse)
		{
			vehMatrix.d.x += -Dir.y * laneCentreOffset;
			vehMatrix.d.y -= -Dir.x * laneCentreOffset;
		}
		else
#endif //__BANK
		{
			vehMatrix.d.x += Dir.y * laneCentreOffset;
			vehMatrix.d.y -= Dir.x * laneCentreOffset;
		}				

		// Ensure that this spawn location is not in full view, and closer than the allowed 'distance' in-view spawn area
		// This can happen now that UpdateVehGenLinks is only called periodically.

		// be leniant about vehicles created near the maximum distance
		// we are only trying to stop vehicles spawning up close
		TUNE_GROUP_BOOL(VEHICLE_POPULATION, bAllowSpawnWhenInViewport, false);
		if(ms_spawnCareAboutFrustumCheck && !bAllowSpawnWhenInViewport)
		{
			const camBaseDirector * pDominantDirector = camInterface::GetDominantRenderedDirector();

#if __FINAL
			if(pDominantDirector)
#else
			if(pDominantDirector && pDominantDirector->GetClassId() != camDebugDirector::GetStaticClassId())
#endif
			{
				const bool bZoomedIn = (GetVehicleLodScale() >= ms_fNearDistLodScaleThreshold);

				const Vector3 & camPos = pDominantDirector->GetFrame().GetPosition();
				const Vector3 vToPlacementPos = vehMatrix.d - camPos;
				const float fBoundingRadius = CModelInfo::GetBaseModelInfo(vehModelId)->GetBoundingSphereRadius();

				if(bZoomedIn || Abs(vToPlacementPos.z) < ms_fSkipFrustumCheckHeight)
				{
					const float fDistToPlacementPosSq = vToPlacementPos.XYMag2();
					float fNearDistSq = 0.0f;

					if(ms_aGenerationLinks[iVehGenLinkIndex].m_iCategory == CPopGenShape::GC_KeyHoleSideWall_on 
						|| ms_aGenerationLinks[iVehGenLinkIndex].m_iCategory == CPopGenShape::GC_KeyHoleInnerBand_on)
					{
						fNearDistSq = square(ms_popGenKeyholeShape.GetOuterBandRadiusMin());
					}
					else
					{
						fNearDistSq = square(ms_fCalculatedSpawnFrustumNearDist);
					}

					// Near to the camera?
					if(fDistToPlacementPosSq < fNearDistSq)
					{
						Vec4V vSphere = VECTOR3_TO_VEC4V(vehMatrix.d);
						vSphere.SetW(fBoundingRadius);
						const bool bOnScreen = camInterface::IsSphereVisibleInGameViewport(vSphere);
						if(bOnScreen)
						{
							NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Position was near to local player and in their view frustum");
							vehPopDebugf2("Position was near to local player and in their view frustum");
							RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_InViewFrustum, m_populationFailureData.m_numSpawnLocationInViewFrustum));

							TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
							return;
						}
					}
				}
			}
		}

		// when creating vehicles, these vars are set to determine the destination in an interior
		CInteriorInst*	pDestInteriorInst = 0;		
		s32			destRoomIdx = -1;
		bool setWaitForAllCollisionsBeforeProbe = false;

		// If this is a boat we read the height from the water level
		if (bIsBoat)
		{
			float	WaterZ;
			if (!Water::GetWaterLevel(vehMatrix.d, &WaterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
			{	// no water? no boat
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, requested boat but found some kind of water level fuckup");
				vehPopDebugf2("Skip vehicle spawn, requested boat but found some kind of water level fuckup");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_SomeKindOfWaterLevelFuckup, m_populationFailureData.m_numWaterLevelFuckup));

				TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
				return;
			}
		}
		else
		{
			// when creating vehicles, these vars are set to determine the destination in an interior
			if (  ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(vehModelId))->GetIsBike() )
			{
				if (ms_spawnCareAboutPlacingOnRoadProperly && !CBike::PlaceOnRoadProperly(NULL,&vehMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, vehModel, NULL, true, aNodeAddr, aNodeLinks, 3))
				{	// Couldn't find any ground to place this bike on? Jump out.
					vehPopDebugf2("Skip vehicle spawn, bike requested but found no ground to put it on");
					NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, bike requested but found no ground to put it on");
					RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_CouldntPlaceBike, m_populationFailureData.m_numGroundPlacementFail));

					TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
					return;
				}
			}
			else
			{
				if (ms_spawnCareAboutPlacingOnRoadProperly && !CAutomobile::PlaceOnRoadProperly(NULL, &vehMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, vehModel, NULL, true, aNodeAddr, aNodeLinks, 3))
				{	// Couldn't find any ground to place this car on? Jump out.
					vehPopDebugf2("Skip vehicle spawn, car requested but found no ground to put it on");
					NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, car requested but found no ground to put it on");
					RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_CouldntPlaceCar, m_populationFailureData.m_numGroundPlacementFail));
				
					TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
					return;
				}
			}
		}
		Assert(vehMatrix.IsOrthonormal());

		if (bIsBoat)
		{
			ExtendedRemovalRange = (BOAT_EXTENDED_RANGE * GetCullRange(1.0f, 1.0f));
		}

		// don't create vehicles in view of or close to any other network players
		bool bForceFadeIn = false;
		if(NetworkInterface::IsGameInProgress())
		{
			bool bCanReject = CanRejectNetworkSpawn(vehMatrix.d, CModelInfo::GetBaseModelInfo(vehModelId)->GetBoundingSphereRadius(), bForceFadeIn);
			if(bCanReject)
			{
				vehPopDebugf2("Skip vehicle spawn, too close to network player");
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, too close to network player");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_TooCloseToNetworkPlayer, m_populationFailureData.m_numNetworkVisibilityFail));
				
				//don't disable this link as that'll prevent spawn for 2 seconds which is overkill as people will be moving around
				//instead just increase ms_iCurrentActiveLink and continue so that next link update we can check it again straight away
				++ms_iCurrentActiveLink;
				continue;
			}
			else if(bForceFadeIn)
			{
				vehPopDebugf2("Network Override -- TryToGenerateOneRandomVeh allow vehicle to spawn");
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Override:", "Network override, allow vehicle to spawn");
			}
		}

		const s8 SpeedFromNodes = ThePaths.FindNodePointer(aNodeAddr[2])->m_2.m_speed;
		const float SpeedMultiplier = CVehicleIntelligence::FindSpeedMultiplierWithSpeedFromNodes(SpeedFromNodes);
		float InitialSpeed = CruiseSpeed * SpeedMultiplier;										

		// Do a final check to make sure that the veh is moving towards
		// the player (relatively). If it isn't we'll quickly get rid of
		// the guy (It would get deleted in a couple of seconds anyway)
		// Find the velocity:
		if(ms_spawnDoRelativeMovementCheck && !ms_bInstantFillPopulation)
		{
			float fThicknessScale = CalculatePopulationSpawnThicknessScale();

			float fMaxSpawnRange;
			if(ms_aGenerationLinks[iVehGenLinkIndex].m_iCategory==CPopGenShape::GC_KeyHoleInnerBand_on)
			{
				fMaxSpawnRange = (ms_popGenKeyholeShapeInnerBandRadius * rangeScale) + (ms_popGenKeyholeInnerShapeThickness * fThicknessScale);
				fMaxSpawnRange -= 10.0f;
			}
			else if(ms_aGenerationLinks[iVehGenLinkIndex].m_iCategory==CPopGenShape::GC_KeyHoleOuterBand_on)
			{
				fMaxSpawnRange = (ms_popGenKeyholeShapeOuterBandRadius * rangeScale) + (ms_popGenKeyholeOuterShapeThickness * fThicknessScale);
				fMaxSpawnRange -= 10.0f;
			}
			else
			{
				fMaxSpawnRange = GetCullRange(rangeScale) * 0.75f;
			}

			const Vector3 vRelVelocity = (InitialSpeed * vehMatrix.b) - popCtrlCentreVelocity;
			Vector3 vRelPos = vehMatrix.d - popCtrlCentre;
			const float fRelPosMag2 = vRelPos.XYMag2();

			// Only remove if this vehicle is approaching the cull radius, at which
			// range it may well be removed.  We don't want to go preventing vehicles
			// behind created unless we're confident they will be shortlived.
			if(fRelPosMag2 > fMaxSpawnRange*fMaxSpawnRange)
			{
				vRelPos.Normalize();
				const float fProjectedVel = DotProduct(vRelPos, vRelVelocity);
				static dev_float fMinProj = 10.0f;
				if (fProjectedVel > fMinProj)
				{
					vehPopDebugf2("Skip vehicle spawn at (%.2f, %.2f, %.2f), moving towards the cull radius", vehMatrix.d.x, vehMatrix.d.y, vehMatrix.d.z);
					NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn at (%.2f, %.2f, %.2f), moving towards the cull radius", vehMatrix.d.x, vehMatrix.d.y, vehMatrix.d.z);
					RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_RelativeSpeedCheck, m_populationFailureData.m_numRelativeMovementFail));

					TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
					return;
				}
			}
		}

		if (WillVehicleCollideWithOtherCarUsingRestrictiveChecks(vehMatrix, vBoundBoxMin, vBoundBoxMax))
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, WillVehicleCollideWithOtherCarUsingRestrictiveChecks");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_CollidesWithTrajectoryOfMarkedVehicle, m_populationFailureData.m_numTrajectoryOfMarkedVehicleRejection));

			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
			return;
		}

		// DEBUG!! -AC, if the new spawn system is good enough we can remove this expensive check!
		// Make sure it is not in collision with an existing vehicle.
		Assert(pModelInfo->GetFragType());
		phBound *bound = (phBound *)pModelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds();
		int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

		WorldProbe::CShapeTestBoundingBoxDesc bboxDesc;
		bboxDesc.SetBound(bound);
		bboxDesc.SetTransform(&vehMatrix);
		bboxDesc.SetIncludeFlags(nTestTypeFlags);

		if(ms_spawnCareAboutBoxCollisionTest && WorldProbe::GetShapeTestManager()->SubmitTest(bboxDesc))
		{
			vehPopDebugf2("Skip vehicle spawn, in collision with other object");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "in collision with other object");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_CollisionBoxCheck, m_populationFailureData.m_numBoundingBoxFail));

#if __BANK
			if(ms_bDrawBoxesForVehicleSpawnAttempts)
			{
				grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(vehMatrix), Color_red, false, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
			}
#endif

			TryToPreventSubsequentCreationAttemptsOnCurrentActiveLink();
			return;
		}
		// END DEBUG!!

#if __BANK
		if(ms_bDrawBoxesForVehicleSpawnAttempts)
		{
			char nodeBuff[64];
			sprintf(nodeBuff, "A: %d, B %d", ms_aGenerationLinks[iVehGenLinkIndex].m_nodeA.GetIndex(), ms_aGenerationLinks[iVehGenLinkIndex].m_nodeB.GetIndex());
			grcDebugDraw::Text(vehMatrix.d, Color_green, 0, 0, nodeBuff, true, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
			sprintf(nodeBuff, "Frame: %i, NumNearbyVehicles: %.1f", fwTimer::GetFrameCount(), ms_aGenerationLinks[iVehGenLinkIndex].GetNearbyVehiclesCount());
			grcDebugDraw::Text(vehMatrix.d, Color_green, 0, grcDebugDraw::GetScreenSpaceTextHeight(), nodeBuff, true, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
			grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(vehMatrix), Color_green, false, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
		}
#endif



		////////////////////////////////////////////////////////////////////////////////////////////
		// At this point we have decided we will definitely create the vehicle.
		// We are not allowed to remove the vehicle after it has been created. (This would add slowness)
		////////////////////////////////////////////////////////////////////////////////////////////



		eVehicleDummyMode desiredLod = CVehicleAILodManager::CalcDesiredLodForPoint(VECTOR3_TO_VEC3V(vehMatrix.d));
		bool bCreateInactive = (desiredLod==VDM_SUPERDUMMY && CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode);

		CVehicle* pNewVehicle = NULL;

		if(reuseVehicle)
		{
			pNewVehicle = GetVehicleFromReusePool(reusedVehicleIndex);
			if(ReuseVehicleFromVehicleReusePool(pNewVehicle, vehMatrix, POPTYPE_RANDOM_AMBIENT, ENTITY_OWNEDBY_POPULATION, true, bCreateInactive))
			{
				RemoveVehicleFromReusePool(reusedVehicleIndex);
			}
			else
			{
				// reuse failed - maybe network is full?
				pNewVehicle = NULL;
			}
		}
		else
		{
			pNewVehicle = CVehicleFactory::GetFactory()->Create(vehModelId, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_AMBIENT, &vehMatrix, true, bCreateInactive);
		}

		// pNewVehicle can be NULL in a network game if there is no available network objects for the new vehicle
		if (!pNewVehicle)
		{
			vehPopDebugf2("Skip vehicle spawn, vehicle factory returned NULL");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn vehicle factory returned NULL");
			RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_CouldntAllocateVehicle, m_populationFailureData.m_numVehicleCreateFail));
			return;
		}

		// Keep track of how many ambient vehicles we created this frame
		PF_INCREMENT(AmbientVehiclesCreated);
		BANK_ONLY(pNewVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_RANDOM);

		pNewVehicle->m_ExtendedRemovalRange = (s16)ExtendedRemovalRange;

		pNewVehicle->SetStartNodes(aNodeAddr[1], aNodeAddr[2], regionLinkIndexOldToNew, LaneIndex);

		// Set up occupants.
		if(pNewVehicle->CanSetUpWithPretendOccupants())
		{
			// Setup the veh with pretend occupants.
			// This also gives the vehicle a default task.
			pNewVehicle->SetUpWithPretendOccupants();
		}
		else
		{
			bool bDriverAdded = false;

			// Setup the veh with real occupants.
			if (bPolice)
			{
				bDriverAdded = AddPoliceVehOccupants(pNewVehicle);
			}
			else
			{
				ms_bVehIsBeingCreated = true;
				bDriverAdded = AddDriverAndPassengersForVeh(pNewVehicle, 0, CVehicleAILodManager::ComputeRealDriversAndPassengersRemaining()) > 0;
				ms_bVehIsBeingCreated = false;
			}

			pNewVehicle->GiveDefaultTask();

			if (!bDriverAdded)
			{
				vehPopDebugf2("Vehicle %s destroyed right after creation, driver couldn't be added", pNewVehicle->GetModelName());

				// the driver could not be added, so we have to remove the vehicle
				BANK_ONLY(ms_lastRemovalReason = Removal_OccupantAddFailed);
				RemoveVeh(pNewVehicle, true, Removal_OccupantAddFailed);

				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn driver couldn't be added");
				RegisterVehiclePopulationFailure(BANK_ONLY(iVehGenLinkIndex, approximatePos, Fail_CouldntAllocateDriver, m_populationFailureData.m_numDriverAddFail));
				return;
			}
		}

		//always instantly lower the roof when creating a moving vehicle.
		//when they're stopped, they may decide to put it up or down
		if (pNewVehicle->ShouldLowerConvertibleRoof())
		{
			pNewVehicle->LowerConvertibleRoof(true);
			pNewVehicle->RolldownWindows();
		}

		// attach trailer if vehicle needs it, it's allowed...
		// ...and we have room for it in the pool -JM
		// We now do the trailer checks before attempting to use the vehicle reuse pool -MK
		CVehicle* trailer = NULL;
		if (bUseTrailer)
		{
			BANK_ONLY(utimer_t trailerStartTime = sysTimer::GetTicks());
			fwModelId trailerId = pModelInfo->GetRandomTrailer();
			fwModelId originalId;
			if (trailerId.IsValid())
			{
				const bool bUseBiggerBoxesForCollisionTest = true;

				if (CNetwork::IsGameInProgress())
				{
					originalId = trailerId;
					if ((trailerId.GetModelIndex() == MI_TRAILER_TR2) || (trailerId.GetModelIndex() == MI_TRAILER_TR4))
					{
						Assertf(false, "Vehicle type (%s) not permitted in MP",CModelInfo::GetBaseModelInfoName(trailerId));
					}
				}

				trailer = CVehicleFactory::GetFactory()->CreateAndAttachTrailer(trailerId, pNewVehicle, vehMatrix, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_AMBIENT, bUseBiggerBoxesForCollisionTest);
       
				if (CNetwork::IsGameInProgress() && (pNewVehicle != NULL))
				{
					Assertf(pNewVehicle->IsVehicleAllowedInMultiplayer(), "Vehicle type (%s) not permitted in MP (orginal - %s)",pNewVehicle->GetModelName(), CModelInfo::GetBaseModelInfoName(originalId));
				}
			}
			if (trailer)
			{
				BANK_ONLY(trailer->m_CreationMethod = CVehicle::VEHICLE_CREATION_TRAILER);
				BANK_ONLY(trailer->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - trailerStartTime)));
				OUTPUT_ONLY(CVehicleModelInfo* trailerModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(trailerId));
				vehPopDebugf2("Successfully created trailer 0x%8p for vehicle 0x%8p at (%.2f, %.2f, %.2f), type: %s", trailer, pNewVehicle, vehMatrix.d.x, vehMatrix.d.y, vehMatrix.d.z, trailerModelInfo->GetModelName());
			}        
		}

		// Set vehicles to alpha-fade in only if they were generated in the far onscreen band
		// This avoids errors with vehicles getting nearby before fading-in, since alpha fade
		// is only updated when an entity is onscreen (or in the vicinity of the player)
		if( bForceFadeIn || (ms_aGenerationLinks[iVehGenLinkIndex].m_iCategory == CPopGenShape::GC_KeyHoleOuterBand_on) )
		{
			pNewVehicle->GetLodData().SetResetDisabled(false);
			pNewVehicle->GetLodData().SetResetAlpha(true);

			if(trailer)
			{
				trailer->GetLodData().SetResetDisabled(false);
				trailer->GetLodData().SetResetAlpha(true);
			}
		}

		pNewVehicle->GetIntelligence()->SpeedMultiplier = SpeedMultiplier;
		pNewVehicle->GetIntelligence()->SpeedFromNodes = SpeedFromNodes;

		// Make sure we don't use the same colour we used for the previous veh
		u8 newcol1, newcol2, newcol3, newcol4, newcol5, newcol6;
		pNewVehicle->m_nVehicleFlags.bGangVeh = false;
		pModelInfo->ChooseVehicleColourFancy(pNewVehicle, newcol1, newcol2, newcol3, newcol4, newcol5, newcol6);
		pNewVehicle->SetBodyColour1(newcol1);
		pNewVehicle->SetBodyColour2(newcol2);
		pNewVehicle->SetBodyColour3(newcol3);
		pNewVehicle->SetBodyColour4(newcol4);
		pNewVehicle->SetBodyColour5(newcol5);
		pNewVehicle->SetBodyColour6(newcol6);
		pNewVehicle->UpdateBodyColourRemapping();	// let shaders know that body colour changed

		const Vector3 vNewVehiclePos = VEC3V_TO_VECTOR3(pNewVehicle->GetVehiclePosition());
		// Use a certain margin since boats are sometimes created outside the world (the nodes are allowed to be there)
		Assert(vNewVehiclePos.x > WORLDLIMITS_XMIN - 200.0f);
		Assert(vNewVehiclePos.x < WORLDLIMITS_XMAX + 200.0f);
		Assert(vNewVehiclePos.y > WORLDLIMITS_YMIN - 200.0f);
		Assert(vNewVehiclePos.y < WORLDLIMITS_YMAX + 200.0f);
					
		// Switch on the engine
		pNewVehicle->SwitchEngineOn(true);
		pNewVehicle->SetRandomEngineTemperature();

		// bug fix 172020 : boat is being spawned in interior which hasn't been loaded yet in
		// a helicopter network race - buzz the tower
		fwInteriorLocation	interiorLocation;
		if (pNewVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT || !pDestInteriorInst || destRoomIdx == INTLOC_INVALID_INDEX) 
		{
			interiorLocation.MakeInvalid();		// force boats to be created in outside world
		} 
		else
		{
			interiorLocation = CInteriorProxy::CreateLocation(pDestInteriorInst->GetProxy(), destRoomIdx);
		}

		if (setWaitForAllCollisionsBeforeProbe)
		{
			pNewVehicle->GetPortalTracker()->SetWaitForAllCollisionsBeforeProbe(true);
		}

		CGameWorld::Add(pNewVehicle, interiorLocation);
		if (trailer)
		{
			CGameWorld::Add(trailer, interiorLocation);

			if (trailer->GetVehicleModelInfo() && fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f)
			{
				// don't spawn cars on top of trailers in network games, it uses up unnecessary vehicles
				// from our reduced budget and can lead to syncing issues. We'll leave boats for now as it give additional gameplay possibilities
				if (trailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_ON_TRAILER) && !NetworkInterface::IsGameInProgress())
				{
					CVehicleFactory::GetFactory()->AddCarsOnTrailer((CTrailer*)trailer, interiorLocation, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, NULL);
				}
				else if (trailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
				{
					if (!CVehicleFactory::GetFactory()->AddBoatOnTrailer((CTrailer*)trailer, interiorLocation, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, NULL))
					{
						// We failed to create a boat. Check to see if we're still allowed to have this trailer
						CVehicleFactory::DestroyBoatTrailerIfNecessary((CTrailer**)&trailer);
					}
				}
			}
		}

		// Make sure our "bPossiblyTouchesWater" flag is up to date.
		// This should help prevent vehicles floating in tunnels, etc.
		pNewVehicle->UpdatePossiblyTouchesWaterFlag();

		if (pNewVehicle->m_nVehicleFlags.bGangVeh)
		{
			pNewVehicle->InitExtraFlags(EXTRAS_SET_GANG);
			pNewVehicle->Fix();
		}

		BANK_ONLY(AddVehicleToSpawnHistory(pNewVehicle, &ms_aGenerationLinks[iVehGenLinkIndex], "TryToGenerateOneRandomVeh"));

		if (bPolice)
		{
			ms_lastTimeLawEnforcerCreated = fwTimer::GetTimeInMilliseconds();						
		}

		//calculate cruise speed again now that we have a random seed to pass into the RNG
		CruiseSpeed = rage::Min(CVehiclePopulation::PickCruiseSpeedWithVehModel(pNewVehicle, vehModel)
			, CDriverPersonality::FindMaxCruiseSpeed(pNewVehicle->GetDriver(), pNewVehicle, pNewVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE));

		// if reusing the vehicle, the NumTimesUsed doesn't need to be incremented
		if(!reuseVehicle)
		{
			pNewVehicle->GetVehicleModelInfo()->IncreaseNumTimesUsed();
		}

		pNewVehicle->SelectAppropriateGearForSpeed();

		Assert(!pNewVehicle->GetIsInPopulationCache());

		// Maybe add a specific scenario to this veh (delivery drive, limo/taxi driver)
		CScenarioManager::GiveScenarioToDrivingCar( pNewVehicle );

		DEV_ONLY(VecMapDisplayVehiclePopulationEvent(vNewVehiclePos, VehPopCreate));
		vehPopDebugf2("Successfully created random vehicle 0x%8p (%.2f, %.2f, %.2f), type: %s, refs(parked): %d(%d), freq: %d",
			pNewVehicle, vNewVehiclePos.x, vNewVehiclePos.y, vNewVehiclePos.z, pNewVehicle->GetModelName(),
			pModelInfo->GetNumVehicleModelRefs(), pModelInfo->GetNumVehicleModelRefsParked(), pModelInfo->GetVehicleFreq());
		BANK_ONLY(ms_aGenerationLinks[iVehGenLinkIndex].m_iFailType = Fail_None);

		// Initial velocity
		float fCreationSpeed = CVehicleIntelligence::MultiplyCruiseSpeedWithMultiplierAndClip(CruiseSpeed, pNewVehicle->GetIntelligence()->SpeedMultiplier, true, false, vehMatrix.d);

#if __BANK
		if (ms_bEverybodyCruiseInReverse)
		{
			fCreationSpeed = -fCreationSpeed;
		}
#endif //__BANK

		Vector3 vCreationVelocity = newCoors - oldCoors;
		vCreationVelocity.NormalizeSafe();
		vCreationVelocity.Scale(fCreationSpeed);
		if (pNewVehicle->m_nVehicleFlags.bCreatingAsInactive)
		{
			pNewVehicle->SetSuperDummyVelocity(vCreationVelocity);
		}
		else
		{
			pNewVehicle->SetVelocity(vCreationVelocity);
		}

		if(trailer)
		{
			trailer->SetVelocity(vCreationVelocity);
		}

		//Make the vehicle into a dummy based on distance.
		bool bSuperDummyConversionFailed = false;
		MakeVehicleIntoDummyBasedOnDistance(*pNewVehicle, RCC_VEC3V(vNewVehiclePos), bSuperDummyConversionFailed);

		//fCreationSpeed = pCarTask->FindMaximumSpeedForThisCarInTraffic(pNewVehicle, RequestedSpeed, avoidanceRadius, NULL); //TODO

		Vector3 vVelocity;
		CTaskVehicleMissionBase* pActiveTask = pNewVehicle->GetIntelligence()->GetActiveTask();
		CTaskVehicleGoToPointWithAvoidanceAutomobile* pGoToTask = pActiveTask ? static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(pActiveTask->FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)) : NULL;	
		if(pGoToTask)
		{
			const Vec3V groundPos = SubtractScaled(VECTOR3_TO_VEC3V(vehMatrix.d), VECTOR3_TO_VEC3V(vehMatrix.c), ScalarV(pNewVehicle->GetHeightAboveRoad()));
			vVelocity = *pGoToTask->GetTargetPosition() - VEC3V_TO_VECTOR3(groundPos);
		}
		else
		{
			vVelocity = newCoors - oldCoors;
		}

		vVelocity.NormalizeSafe();

		//potentially limit velocity here, as pNewVehicle's first ai update,
		//called from within MakeVehicleIntoDummyBasedOnDistance may have
		//wanted to slow for something, so we should just assume he was going at that
		//speed when spawned
		float fDesiredSpeed = pNewVehicle->GetIntelligence()->m_fDesiredSpeedThisFrame;

		const CPhysical *pObstacleEntity = NULL;
		if (pNewVehicle->GetIntelligence()->GetObstructingEntity())
		{
			pObstacleEntity = pNewVehicle->GetIntelligence()->GetObstructingEntity();
		}
		else if(pNewVehicle->GetIntelligence()->GetEntitySteeringAround())
		{
			pObstacleEntity = pNewVehicle->GetIntelligence()->GetEntitySteeringAround();
		}

		if(pObstacleEntity)
		{
			fDesiredSpeed = 0.0f;

			//! If we are travelling in the same direction as the obstruction, then don't just stop, clamp the speed to slightly
			//! below our obstructions speed to keep traffic flowing (particularly on freeways).
			if(pObstacleEntity->GetIsTypeVehicle())
			{
				Vector3 vOtherVel = pObstacleEntity->GetVelocity();

				static dev_float s_MaxVelocityToConsiderObstructionVelocity = 5.0f;
				static dev_float s_MaxDotToConsiderObstructionVelocity = 0.8f;
				static dev_float s_DesiredVelocityMultiplier = 0.95f;

				if(vOtherVel.XYMag2() > rage::square(s_MaxVelocityToConsiderObstructionVelocity))
				{
					vOtherVel.NormalizeSafe();
					float fDot = DotProduct(vOtherVel, vVelocity);
					if(fDot > s_MaxDotToConsiderObstructionVelocity)
					{
						fDesiredSpeed = Min(fCreationSpeed, pObstacleEntity->GetVelocity().Mag() * s_DesiredVelocityMultiplier);
					}
				}
			}
		}

		if (fCreationSpeed > 0.0f && fDesiredSpeed >= 0.0f && fDesiredSpeed < fCreationSpeed)
		{
			fCreationSpeed = fDesiredSpeed;
		}

		vVelocity.Scale(fCreationSpeed);

		//if we originally created as inactive, don't set superdummy velocity here,
		//only dummy, otherwise we'll activate it
		if (pNewVehicle->m_nVehicleFlags.bCreatingAsInactive && !bSuperDummyConversionFailed)
		{
			pNewVehicle->SetSuperDummyVelocity(vVelocity);
		}
		else
		{
			pNewVehicle->SetVelocity(vVelocity);
		}

		if(trailer)
		{
			trailer->SetVelocity(vVelocity);
		}

		// Set flag for whether this vehicle counts towards ambient population
		pNewVehicle->m_nVehicleFlags.bCountsTowardsAmbientPopulation = CountsTowardsAmbientPopulation(pNewVehicle);

		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "TryToGenerateOneRandomVeh", "Vehicle Created:", "%s", pNewVehicle->GetNetworkObject() ? pNewVehicle->GetNetworkObject()->GetLogName() : "No Network Object");

		++ms_iCurrentActiveLink;
		++ms_iNumNormalVehiclesCreatedSinceDensityUpdate;
		BANK_ONLY(pNewVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - startTime)));
		return;
	}
}

void CVehiclePopulation::EnableVehicleNeonsIfAppropriate(CVehicle *pVehicle)
{
	// Spawn a car with neons if:
	// * Ambient cars are allowed to spawn with neons on (controlled by script)
	// * This is not a network game
	// * There isn't a vehicle in the population with neons on already
	// * This car is allowed to have neons in the population
	// * This car has neon bones
	// * The driver ped is a suitable type
	if(!ms_ambientVehicleNeonsEnabled)
		return;

	if(CNetwork::IsGameInProgress())
		return;

	if(ms_pLastSpawnedNeonVeh != NULL)
		return;

	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(pVehicle->GetModelId());
	if(!pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CAN_HAVE_NEONS))
		return;

	if(!pVehicle->HasNeons())
		return;

	CPed* pDriver = pVehicle->GetDriver();
	if(!pDriver)
		return;

	u32 nameHash = pDriver->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash();
	if(nameHash != 0xe39b459f &&	// "YOUNGRICHMAN"
		nameHash != 0x4b196c56 &&	// "YOUNGAVERAGETOUGHMAN"
		nameHash != 0xecbcdd2e)		// "YOUNGAVERAGEWEAKMAN"
	{
		return;
	}

	//
	// After this point, we've determined we're going to enable neons
	//

	// Select a colour. This must match the colours in carmod_shop_private.sch.
	const Color32 colours[] =
	{
		//      R    G    B
		Color32(255, 255, 255),
		Color32(255, 255, 0),
		Color32(0,   0,   255),
		Color32(191, 255, 0),
		Color32(255, 165, 0),
		Color32(255, 0,   0),
		Color32(255, 192, 203),
		Color32(128, 0,   128)
	};

	static const float s_ChanceOfOnlySideNeons = 0.25f; // Chance of this car having only side neons enabled

	// Only side neons enabled?
	bool onlySideNeonsOn = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_ChanceOfOnlySideNeons;

	// Select a colour randomly, and don't select the same colour as last time
	u32 colourIdx = fwRandom::GetRandomNumberInRange(0, NELEM(colours));
	if(colourIdx == ms_lastSpawnedNeonIdx)
	{
		++ms_lastSpawnedNeonIdx;
	}
	if(colourIdx >= NELEM(colours))
	{
		// Wrap around on overflow
		colourIdx = 0;
	}
 	pVehicle->SetNeonColour(colours[colourIdx]);
	ms_lastSpawnedNeonIdx = colourIdx;

	// Enable them
	pVehicle->SetNeonLOn(true);
	pVehicle->SetNeonROn(true);
	if(!onlySideNeonsOn)
	{
		pVehicle->SetNeonFOn(true);
		pVehicle->SetNeonBOn(true);
	}
	ms_pLastSpawnedNeonVeh = pVehicle;
}

// hardcoded AABB, see url:bugstar:1613258
const Vector3 vPreventSpawnOnLinksAABB[2] =
{
	Vector3( -652.0f, -700.0f, 29.0f ),
	Vector3( -621.0f, -672.0f, 36.0f )
};

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateRandomCarsOnPaths
// PURPOSE :  Goes through the path nodes in the area and the ones that are now close
//			  to the camera will get cars generated on them (if appropriate)
/////////////////////////////////////////////////////////////////////////////////
RAGETRACE_DECL(CVehiclePopulation_GenerateRandomCarsOnPaths);
void CVehiclePopulation::GenerateRandomCarsOnPaths(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale)
{
	RAGETRACE_SCOPE(CVehiclePopulation_GenerateRandomCarsOnPaths);

	if(NetworkInterface::IsGameInProgress())
	{
		return;
	}

	// Don't fill up the vehicles pool with random cars now.
	//! DMKH. TO DO - does this take into account re-used vehicles?
	if(CVehicle::GetPool()->GetNoOfFreeSpaces() <= 12)
	{
		vehPopDebugf2("Not enough room in vehicle pool (<= 12) to create vehicles on paths");
		return;
	}

	if(FindNumRandomVehsThatCanBeCreated() <= 0)
	{
		return;
	}

	if(!gPopStreaming.IsFallbackPedAvailable())
	{
		return;
	}

	static dev_float sfMinPlayerSpeed = 10.0f;

	Vector3 minplayerspeed;
	minplayerspeed.Set(sfMinPlayerSpeed * sfMinPlayerSpeed);
	bool bPlayerGoingFastEnough = (popCtrlCentreVelocity.Mag2V().IsGreaterThan(minplayerspeed));
	if(!bPlayerGoingFastEnough)
	{
		return;
	}

	Vector3 centreDir(popCtrlCentreVelocity);
	centreDir.Normalize();

	Vector4 creationRange;
	creationRange.Set(GetCreationDistance(rangeScale));
	creationRange *= creationRange;

	// Calcaulte a range which we deem too close to create vehicles on paths
	float fTooClose = GetCreationDistance(rangeScale) * 0.25f;
	// But ensure that it is at least 25m less than the distance at which we instance junctions,
	// or we'll never have any vehicles created at traffic lights
	if(fTooClose + 25.0f > CJunctions::ms_fInstanceJunctionDist)
		fTooClose = Max(CJunctions::ms_fInstanceJunctionDist - 25.0f, 0.0f);

	Vector4 tooCloseRange;
	tooCloseRange.Set(GetCreationDistance(rangeScale) * 0.25f);
	tooCloseRange *= tooCloseRange;

	// Only scan the nodes in a reasonable range around the player
	float testRange = GetCreationDistance(rangeScale) + 100.0f;
	float fRegionMinX = popCtrlCentre.x - testRange;
	float fRegionMaxX = popCtrlCentre.x + testRange;
	float fRegionMinY = popCtrlCentre.y - testRange;
	float fRegionMaxY = popCtrlCentre.y + testRange;
	s32 regionMinX = ThePaths.FindXRegionForCoors(fRegionMinX);
	s32 regionMaxX = ThePaths.FindXRegionForCoors(fRegionMaxX);
	s32 regionMinY = ThePaths.FindYRegionForCoors(fRegionMinY);
	s32 regionMaxY = ThePaths.FindYRegionForCoors(fRegionMaxY);

	for(s32 regionX = regionMinX; regionX <= regionMaxX; regionX++)
	{
		for(s32 regionY = regionMinY; regionY <= regionMaxY; regionY++)
		{
			s32 region = FIND_REGION_INDEX(regionX, regionY);
			if(!ThePaths.apRegions[region] || !ThePaths.apRegions[region]->NumNodes)
			{
				continue;
			}

			s32 startNode, endNode;
			ThePaths.FindStartAndEndNodeBasedOnYRange(region, fRegionMinY, fRegionMaxY, startNode, endNode);

			s32 numCarNodes = ThePaths.apRegions[region]->NumNodesCarNodes;
#define NUM_BATCHES	(10);
			s32 batch = fwTimer::GetSystemFrameCount() % NUM_BATCHES;
			s32 startNodeBatch = (numCarNodes * batch) / NUM_BATCHES;
			s32 endNodeBatch = (numCarNodes * (batch+1)) / NUM_BATCHES;

			startNode = rage::Max(startNode, startNodeBatch);
			endNode = rage::Min(endNode, endNodeBatch);

			if(endNode <= startNode)
			{
				continue;
			}

			CPathNode *pBaseNode	= &ThePaths.apRegions[region]->aNodes[startNode];
			CPathNode *pEndNode		= &ThePaths.apRegions[region]->aNodes[endNode];

			for(; pBaseNode != pEndNode; ++pBaseNode)
			{
				PrefetchDC(pBaseNode + 1);

				Vector3		baseNodeCoors;
				pBaseNode->GetCoors(baseNodeCoors);

				// Calculate whether the node is within range of population centre.
				Vector3 delta = baseNodeCoors - popCtrlCentre;
				Vector4 dist = delta.XYMag2V().xyzw;
				const bool bWithinRange = creationRange.IsGreaterThan(dist);
				if(!bWithinRange)
				{
					pBaseNode->m_2.m_closeToCamera = false;
					continue;
				}
				
				if(pBaseNode->m_2.m_closeToCamera)
				{
					continue;
				}

				pBaseNode->m_2.m_closeToCamera = true;

				// Don't generate too close to player
				const bool bTooClose = tooCloseRange.IsGreaterThan(dist);
				if(bTooClose)
				{
					continue;
				}

				if(pBaseNode->m_2.m_switchedOff)
				{
					continue;
				}
				
				// Check hardcoded cull AABB
				if(baseNodeCoors.IsGreaterThan(vPreventSpawnOnLinksAABB[0]) && vPreventSpawnOnLinksAABB[1].IsGreaterThan(baseNodeCoors))
				{
					continue;
				}

				//// Make sure the cars will be created where the player is going.
				//Vector3 diff = baseNodeCoors - popCtrlCentre;
				//diff.Normalize();
				//if(diff.Dot(centreDir) <= 0.85f)
				//{
				//	continue;
				//}

				const s32 iInitialNumVehiclesCreatedThisCycle = ms_iNumVehiclesCreatedThisCycle;

			    for(u32 iLink = 0; iLink < pBaseNode->NumLinks(); iLink++)
			    {
				    PossiblyGenerateRandomCarsOnLink(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pBaseNode, iLink);

					if(ms_iNumVehiclesCreatedThisCycle != iInitialNumVehiclesCreatedThisCycle)
					{
						// We've created a vehicle
						return;
					}
			    }
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PossiblyGenerateRandomCarsOnLink
// PURPOSE :  Generates random cars on this link (if appropriate).
/////////////////////////////////////////////////////////////////////////////////
enum
{
	GENERATED_AT_TRAFFIC_LIGHTS = 0,
	GENERATED_AT_GIVE_WAY,
	GENERATED_AT_PETROL_STATION,
	GENERATED_AT_DRIVE_THROUGH_WINDOW
};

void CVehiclePopulation::PossiblyGenerateRandomCarsOnLink(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale, const CPathNode *pNode, s32 iLink) // const bool bOnlyManhattanNorthSouthLights)
{
	const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pNode, iLink);
	CNodeAddress node2 = pLink->m_OtherNode;

	if (ThePaths.IsRegionLoaded(node2))
	{
		const CPathNode * pNode2 = ThePaths.FindNodePointer(node2);

		// Check hardcoded cull AABB
		Vector3 vOtherNodeCoords;
		pNode2->GetCoors(vOtherNodeCoords);
		if(vOtherNodeCoords.IsGreaterThan(vPreventSpawnOnLinksAABB[0]) && vPreventSpawnOnLinksAABB[1].IsGreaterThan(vOtherNodeCoords))
		{
			return;
		}

		if(!pNode->IsSwitchedOff() && !pNode2->IsSwitchedOff() && !pLink->IsShortCut())
		{
			if(ms_bGenerateCarsWaitingAtLights && !pLink->m_1.m_bDontUseForNavigation)
			{
				// Fo6r traffic lights we look one node ahead.
				if ( pNode2->IsTrafficLight() && !pNode->IsJunctionNode() )
				{
					// Iterate through pNode2's links until we find the junction node
					for(s32 l=0; l<pNode2->NumLinks(); l++)
					{
						const CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe(ThePaths.GetNodesLink(pNode2, l).m_OtherNode);
						if(pJunctionNode && pJunctionNode->IsJunctionNode())
						{
							// Skip this check for now -- junctions haven't been created. Bug?
							//if ( ShouldCreateCarsAtTrafficLight(pNode2, pJunctionNode) )
							{
								GenerateRandomCarsOnLink(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode, pNode2, pLink, GENERATED_AT_TRAFFIC_LIGHTS, 1.0f, ms_iMaxCarsForLights/2 );
								return;
							}
							return;
						}
					}
				}

				// Also do something similar for give-way nodes
				if( pNode2->IsGiveWay() && !pNode->IsJunctionNode() )
				{
					const float fDensity = (float) pNode->m_2.m_density;
					const float s = Min( fDensity / 15.0f, 1.0f );
					int iNumCars = (int) ((3.0f * s) + fwRandom::GetRandomNumberInRange(0.0f, 1.0f));
					GenerateRandomCarsOnLink(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode, pNode2, pLink, GENERATED_AT_GIVE_WAY, 1.0f, iNumCars );

					return;
				}
			}
			
			// Petrol stations.
			if (pNode->m_1.m_specialFunction == SPECIAL_PETROL_STATION)
			{
				GenerateRandomCarsOnLink(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode, pNode2, pLink, GENERATED_AT_PETROL_STATION, 1.0f, 4);
				return;
			}

			// Toll booths.
			if (pNode2->m_1.m_specialFunction == SPECIAL_DRIVE_THROUGH_WINDOW)
			{
				GenerateRandomCarsOnLink(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode, pNode2, pLink, GENERATED_AT_DRIVE_THROUGH_WINDOW, 0, 4);
				//GenerateRandomCarsOnLink(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode, pNode2, pLink, GENERATED_AT_DRIVE_THROUGH_WINDOW, pLink->m_1.m_TrafficLightPosition/7.0f - 0.15f, 4);
				return;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateRandomCarsOnLink
// PURPOSE :  Generates random cars on this link.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::GenerateRandomCarsOnLink(const Vector3& popCtrlCentre, const Vector3& popCtrlCentreVelocity, const float rangeScale, const CPathNode *pNode1, const CPathNode *pNode2, const CPathNodeLink *pLink, s32 generated, float maxDist, s32 maxNumCars)
{
	// We want to create cars. We only create cars on coordinates on the nodes that
	// are multiples of 8. (We create a car every 8 meters)
#define CARS_APART_IN_JAMS		(8)		// distance cars get created from eachother in traffic jams.
	s32	startHash, endHash;
	float	lengthHash;

	Assert(!pNode1->m_2.m_waterNode);
	Assert(!pNode2->m_2.m_waterNode);

	if (pNode1->m_2.m_distanceHash < pNode2->m_2.m_distanceHash)
	{
		startHash = ( (pNode1->m_2.m_distanceHash + CARS_APART_IN_JAMS - 1) / CARS_APART_IN_JAMS) * CARS_APART_IN_JAMS;
		endHash = pNode2->m_2.m_distanceHash;
		lengthHash = (float)(pNode2->m_2.m_distanceHash - pNode1->m_2.m_distanceHash);
	}
	else
	{
		startHash = ( (pNode2->m_2.m_distanceHash + CARS_APART_IN_JAMS - 1) / CARS_APART_IN_JAMS) * CARS_APART_IN_JAMS;
		endHash = pNode1->m_2.m_distanceHash;
		lengthHash = (float)(pNode1->m_2.m_distanceHash - pNode2->m_2.m_distanceHash);
	}

	const s32 iInitialNumVehiclesCreatedThisCycle = ms_iNumVehiclesCreatedThisCycle;

	if (generated == GENERATED_AT_PETROL_STATION)		// For petrol stations we just create car on first node really.
	{
		vehPopDebugf3("Try to create vehicles at petrol station on %d lanes", pLink->m_1.m_LanesToOtherNode);
		for (u32 lane = 0; lane < pLink->m_1.m_LanesToOtherNode; lane++)
		{
			GenerateRandomCarOnLinkPosition(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode1, pNode2, pLink, lane, 0.0f, generated);

			if(ms_iNumVehiclesCreatedThisCycle != iInitialNumVehiclesCreatedThisCycle)
			{
				// We've created a vehicle
				return;
			}
		}
	}
	else
	{
		// In certain cases we may want to limit the number of cars.
		endHash = rage::Min(endHash, startHash + CARS_APART_IN_JAMS * maxNumCars);

		for (s32 hashDist = startHash; hashDist < endHash; hashDist += CARS_APART_IN_JAMS)
		{

			Vector3	node1Pos, node2Pos;
			pNode1->GetCoors(node1Pos);
			pNode2->GetCoors(node2Pos);
			float	distOnRoad = (hashDist - startHash) / lengthHash;

			if (distOnRoad <= maxDist)
			{
				vehPopDebugf3("Try to create vehicles at traffic lights or tol booths on %d lanes", pLink->m_1.m_LanesToOtherNode);
				for (u32 lane = 0; lane < pLink->m_1.m_LanesToOtherNode; lane++)
				{
					GenerateRandomCarOnLinkPosition(popCtrlCentre, popCtrlCentreVelocity, rangeScale, pNode1, pNode2, pLink, lane, distOnRoad, generated);

					if(ms_iNumVehiclesCreatedThisCycle != iInitialNumVehiclesCreatedThisCycle)
					{
						// We've created a vehicle
						return;
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShouldCreateCarsAtTrafficLight
// PURPOSE :  Works out whether the state of the light is such that cars will be stopped for a while.
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::ShouldCreateCarsAtTrafficLight(CPathNode * pTrafficLightNode, CPathNode * pJunctionNode)
{
	CJunction * pJunction = CJunctions::FindJunctionFromNode(pJunctionNode->m_address);

	static dev_bool bCreateCarsWhenNoJunction = false;

	// If no CJunction exists yet for this junction node, then don't bother
	if(!pJunction)
	{
		return bCreateCarsWhenNoJunction;
	}

	// This toggle lets us create vehicles even on a green light; might be ok we'll have to experiment
	if(!ms_bGenerateCarsWaitingAtLightsOnlyAtRed)
	{
		return true;
	}

	// If this is a templated junction, examine the entrances to see if lights are red for this entrance
	if(pJunction->GetTemplateIndex()!=-1)
	{
		for(s32 e=0; e<pJunction->GetNumEntrances(); e++)
		{
			const CJunctionEntrance & ent = pJunction->GetEntrance(e);
			if(ent.m_Node == pTrafficLightNode->m_address)
			{
				if(pJunction->GetLightPhase() != ent.m_iPhase)
				{
					return true;
				}
				return false;
			}
		}
	}
	// If this is an automatic junction then
	else
	{
		const s32 iLightCycle = CTrafficLights::CalculateNodeLightCycle(pTrafficLightNode);
		u32 iLights = LIGHT_RED;
		switch(iLightCycle)
		{
			case TRAFFIC_LIGHT_CYCLE1:
				iLights = CTrafficLights::LightForCars1(2500 + pJunction->GetAutoJunctionCycleOffset(), pJunction->GetAutoJunctionCycleScale(), pJunction->GetHasPedCrossingPhase());
				break;
			case TRAFFIC_LIGHT_CYCLE2:
				iLights = CTrafficLights::LightForCars2(2500 + pJunction->GetAutoJunctionCycleOffset(), pJunction->GetAutoJunctionCycleScale(), pJunction->GetHasPedCrossingPhase());
				break;
			default:
				Assert(false);
				break;
		}
		return (iLights!=LIGHT_GREEN);
	}

	return true;
}

 
//////////////////////////////////////////numgener
///////////////////////////////////////
// FUNCTION : GenerateRandomCarOnLinkPosition
// PURPOSE :  We definitely want to generate a car on this link.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::GenerateRandomCarOnLinkPosition(const Vector3& popCtrlCentre, const Vector3& UNUSED_PARAM(popCtrlCentreVelocity), const float UNUSED_PARAM(rangeScale), const CPathNode *pNode, const CPathNode *pNode2, const CPathNodeLink *pLink, u32 lane, float pos, s32 generated)
{
#if __BANK
	utimer_t startTime = sysTimer::GetTicks();
#endif // __BANK

	Matrix34	CarMatrix;
	Vector3		node1Pos, node2Pos;
	bool		bPolice = false;

	if (CPed::GetPool()->GetNoOfFreeSpaces() < MAX_PASSENGERS)	// we may not have enough peds to put in the car
	{
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "not enough ped slots in ped pool for drivers + passengers PedPool=%d < Max_Num=%d", CPed::GetPool()->GetNoOfFreeSpaces(), MAX_PASSENGERS);
		vehPopDebugf3("Skipping veh spawn on link, not enough ped slots in ped pool for drivers + passengers");
		return;
	}

	Assert(!pNode->m_2.m_waterNode);

		// Deal with one way stuff. If we're heading in the direction against the flow of traffic
		// we will not generate the car.
		// First find the number of nodes in the direction we're headed in.

	Assert(lane < pLink->m_1.m_LanesToOtherNode);

		// Depending on the cardensity multiplier we only generate a fraction of them.
	if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > CVehiclePopulation::GetTotalRandomVehDensityMultiplier())
	{
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "dice roll failed density multiplier");
		vehPopDebugf3("Skipping veh spawn on link, dice roll failed density multiplier");
		return;
	}

	pNode->GetCoors(node1Pos);
	pNode2->GetCoors(node2Pos);

	Assert(pos>=0.0f && pos<=1.0f);
	CarMatrix.d = node1Pos*(1.0f - pos) + node2Pos*pos;


	// If both nodes are on the same street, then take a note of the name hash
	const u32 iStreetNameHash = (pNode->m_streetNameHash == pNode2->m_streetNameHash) ? pNode->m_streetNameHash : 0;

	// Pick car from percentages in zone (could still be a policecar)
	bool bHighway = (pNode->IsHighway() && pNode2->IsHighway());
	bool offroad = pNode->IsOffroad() && pNode2->IsOffroad();
	bool bNoBigVehicles = pNode->BigVehiclesProhibited() && pNode2->BigVehiclesProhibited();

	//OR-ing these because the terminal sliplane nodes are tagged as jn's but not sliplanes
	bool bSlipLane = pNode->IsSlipLane() || pNode2->IsSlipLane();	

	//don't allow creating big vehicles in sliplanes, even if the road is otherwise good
	bNoBigVehicles |= bSlipLane;

	u32 CarModel = ChooseModel(&bPolice, CarMatrix.d, (CGameWorld::FindLocalPlayerWanted() ? CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL1 : false), pNode->m_1.m_specialFunction == SPECIAL_SMALL_WORK_VEHICLES, bHighway, iStreetNameHash, offroad, bNoBigVehicles, pLink->m_1.m_LanesToOtherNode);
	
	bool bReuseVehicle = false;
	s32 reusedVehicleIndex = -1;
	// see if we have a valid model in the reuse cache		
	if(ms_bVehicleReusePool)
	{
		reusedVehicleIndex = FindVehicleToReuse(CarModel);
		if(reusedVehicleIndex != -1)
			bReuseVehicle = true;
	}
	
	if(CarModel == fwModelId::MI_INVALID)
	{
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "no valid model found");
		vehPopDebugf2("Skipping veh spawn on link, no valid model found");
        BANK_ONLY(m_populationFailureData.m_numNoVehModelFound++);
		return;
	}

	fwModelId carModelId((strLocalIndex(CarModel)));
	if (!Verifyf(CModelInfo::HaveAssetsLoaded(carModelId), "Model not loaded!"))
		return;

    const bool vehicleInFov = IsSphereVisibleInGameViewport(CarMatrix.d, CModelInfo::GetBaseModelInfo(carModelId)->GetBoundingSphereRadius());
	const float fVehicleDist = (CarMatrix.d - popCtrlCentre).Mag();
	float fNearDist;
	
	if(g_PlayerSwitch.IsActive())
	{
		fNearDist = PLAYER_SWITCH_ON_SCREEN_SPAWN_DISTANCE;
	}
	else
	{
		fNearDist = ms_fCalculatedSpawnFrustumNearDist;
	}

	if( vehicleInFov && fVehicleDist < fNearDist)
	{
		BANK_ONLY(m_populationFailureData.m_numPointWithinCreationDistSpecialFail++);
		vehPopDebugf2("Skipping veh spawn on link, link position was within view frustum and too close");
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "link position within frustum");
		return;
	}

	if(Unlikely(CPedPopulation::ms_useTempPopControlSphereThisFrame && vehicleInFov))
	{
		BANK_ONLY(m_populationFailureData.m_numPointWithinCreationDistSpecialFail++);
		vehPopDebugf2("Skipping veh spawn on link, link position was within view frustum and too close");
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "link position within frustum");
		return;
	}

	if (CarModel == ms_lastChosenVehicleModel)
	{
		if (ms_sameVehicleSpawnCount >= ms_sameVehicleSpawnLimit && fwRandom::GetRandomNumberInRange(0.f, 1.f) > ms_spawnLastChosenVehicleChance)
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "Same vehicle as last time");
			return;
		}

		ms_sameVehicleSpawnCount++;
	}
	else
	{
		ms_sameVehicleSpawnCount = 0;
		ms_lastChosenVehicleModel = CarModel;
	}

    bool canCreateCar = true;

    if(NetworkInterface::IsGameInProgress())
    {
              u32 numRequiredPeds    = 1;
        const u32 numRequiredCars    = 1;
        const u32 numRequiredObjects = 0;

        if(bPolice)
        {
            numRequiredPeds = CVehiclePopulation::GetNumRequiredPoliceVehOccupants(CarModel);
        }

		CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldVehicleGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
		CNetworkObjectPopulationMgr::ePedGenerationContext eOldPedGenContext = CNetworkObjectPopulationMgr::GetPedGenerationContext();
		CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_AmbientPopulation);

		if(numRequiredPeds == 1)
		{
			CNetworkObjectPopulationMgr::SetPedGenerationContext(CNetworkObjectPopulationMgr::PGT_AmbientDriver);
		}

		canCreateCar = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);

		if(numRequiredCars > CVehicle::GetPool()->GetNoOfFreeSpaces())
		{
			Assertf(0, "Trying to create more vehicles that we have in the pool");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "Not enough free vehicles in pool!");
			BANK_ONLY(DumpVehiclePoolToLog());
			canCreateCar = false;
		}

		CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldVehicleGenContext);
		CNetworkObjectPopulationMgr::SetPedGenerationContext(eOldPedGenContext);

#if __DEV
		if (!canCreateCar)
		{
			if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, numRequiredPeds, false))
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "Not enough peds.");
			}
			else if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_AUTOMOBILE, numRequiredCars, false))
			{
				NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "Not enough vehicles.");
			}
		}
#endif // __DEV
	}

    if(!canCreateCar)
    {
		vehPopDebugf2("Skipping veh spawn on link, can't create net game vehicle");
        BANK_ONLY(m_populationFailureData.m_numNetworkPopulationFail++);
        gnetDebug2("Failed to create vehicle due to network population limit - Reason: %s", CNetworkObjectPopulationMgr::GetLastFailTypeString());
        return;
    }

	// We have to aim the car in the direction it will be going. Otherwise
	// we get all sorts of nasty manouvres when they are created.
	// Simply use the nodes for that.
	Vector3 dir = node2Pos - node1Pos;

	dir.NormalizeSafe(Vector3(0.0f,1.0f,0.0f));
	CarMatrix.b = dir;

	CarMatrix.a = dir;
	CarMatrix.a.Cross(Vector3(0.0f,0.0f,1.0f));
	CarMatrix.a.Normalize();

	CarMatrix.c = CarMatrix.a;
	CarMatrix.c.Cross(dir);
	CarMatrix.c.Normalize();

	// Calculate the lane offset.
	float laneCentreOffset = ThePaths.GetLinksLaneCentreOffset(*pLink, lane);

	CarMatrix.d.x += dir.y * laneCentreOffset;
	CarMatrix.d.y -= dir.x * laneCentreOffset;

	// Add the usual random offset for this car.
	u16	randomSeed = (u16)fwRandom::GetRandomNumber();		// We need to pick the randomseed before the car is created so we can get offset right.
	Vector3	randomOffset;
	CVehicleIntelligence::FindNodeOffsetForCarFromRandomSeed(randomSeed, randomOffset);
	CarMatrix.d.x += randomOffset.x;
	CarMatrix.d.y += randomOffset.y;

	// when creating vehicles, these vars are set to determine the destination in an interior
	CInteriorInst*	pDestInteriorInst = 0;		
	s32			destRoomIdx = -1;
	bool setWaitForAllCollisionsBeforeProbe = false;


	CNodeAddress aNodeAddr[3];
	aNodeAddr[0].SetEmpty();
	aNodeAddr[1] = pNode->m_address;
	aNodeAddr[2] = pNode2->m_address;

	s16 aNodeLinks[3] = { -1, -1, -1 };
	aNodeLinks[0] = -1;
	ThePaths.FindNodesLinkIndexBetween2Nodes(pNode->m_address, pNode2->m_address, aNodeLinks[1]);
	if(aNodeLinks[1] != -1)
		aNodeLinks[1] = (s16)ThePaths.GetNodesRegionLinkIndex(pNode, aNodeLinks[1]); // Get link index within region
	aNodeLinks[2] = -1;

	// Pick a node that we were coming from (just pick one at random really).
	aNodeAddr[0] = aNodeAddr[2];
	while (aNodeAddr[0] == aNodeAddr[2])
	{
		const CPathNode* pNode1 = ThePaths.FindNodePointer(aNodeAddr[1]);
		Assert(pNode1);
		s32 RandomLinkIndex = fwRandom::GetRandomNumber() % pNode1->NumLinks();
		aNodeAddr[0] = ThePaths.GetNodesLinkedNodeAddr(pNode1, RandomLinkIndex);
		if (!ThePaths.IsRegionLoaded(aNodeAddr[0]))
		{
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","TryToGenerateOneRandomVeh", "Failed:", "Skip vehicle spawn, neighboring region is not loaded");
			vehPopDebugf2("Skip vehicle spawn, neighboring region is not loaded");
			BANK_ONLY(m_populationFailureData.m_numNoPathNodesAvailable++);
			return;		// If the neighbouring region is not loaded we jump out.
		}
	}
	aNodeLinks[0] = (s16)(ThePaths.FindRegionLinkIndexBetween2Nodes(aNodeAddr[0], aNodeAddr[1]));

	bool useVirtualRoads = (aNodeLinks[0] != -1 && aNodeLinks[1] != -1);

	if (  ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(carModelId))->GetIsBike() )
	{
		if (!CBike::PlaceOnRoadProperly(NULL,&CarMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, CarModel, NULL, useVirtualRoads, aNodeAddr, aNodeLinks, 3))
		{	// Couldn't find any ground to place this car on? Jump out.
            BANK_ONLY(m_populationFailureData.m_numGroundPlacementSpecialFail++);
			vehPopDebugf2("Skipping veh spawn on link, found not ground to place the bike on");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "found not ground to place the bike on");
			return;
		}
	}
	else
	{
		if (!CAutomobile::PlaceOnRoadProperly(NULL, &CarMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, CarModel, NULL, useVirtualRoads, aNodeAddr, aNodeLinks, 3))
		{	// Couldn't find any ground to place this car on? Jump out.
            BANK_ONLY(m_populationFailureData.m_numGroundPlacementSpecialFail++);
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "found not ground to place the car on");
			vehPopDebugf2("Skipping veh spawn on link, found not ground to place the car on");
			return;
		}
	}

	Assert(CarMatrix.IsOrthonormal());

	bool bForceFadeIn = false;

	// don't create vehicles in view of or close to any other network players
	if(NetworkInterface::IsGameInProgress())
	{
		bool bCanReject = CanRejectNetworkSpawn(CarMatrix.d, CModelInfo::GetBaseModelInfo(carModelId)->GetBoundingSphereRadius(), bForceFadeIn);
		if(bCanReject)
		{
			vehPopDebugf2("Skipping veh spawn on link, vehicle too close to net player");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "vehicle too close to net player");
			return;
		}
		else if(bForceFadeIn)
		{
			vehPopDebugf2("Network Override -- GenerateRandomCarOnLinkPosition allow vehicle to spawn");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Override:", "Network override, allow vehicle to spawn");
		}
	}

	s8	SpeedFromNodes = pNode->m_2.m_speed;
	float	SpeedMultiplier = CVehicleIntelligence::FindSpeedMultiplierWithSpeedFromNodes(SpeedFromNodes);
//	float	InitialSpeed = CruiseSpeed * SpeedMultiplier;
	
	CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(carModelId);
	Assert(pModel->GetFragType());
	phBound *bound = (phBound *)pModel->GetFragType()->GetPhysics(0)->GetCompositeBounds();


	if (WillVehicleCollideWithOtherCarUsingRestrictiveChecks(CarMatrix, pModel->GetBoundingBoxMin(), pModel->GetBoundingBoxMax()))
	{
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "Skip vehicle spawn, WillVehicleCollideWithOtherCarUsingRestrictiveChecks");
		BANK_ONLY(m_populationFailureData.m_numTrajectoryOfMarkedVehicleRejection++);

#if __BANK
		if(ms_bDrawBoxesForVehicleSpawnAttempts)
		{
			grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(CarMatrix), Color_red, false, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
		}
#endif
		return;
	}


	int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

	WorldProbe::CShapeTestBoundingBoxDesc bboxDesc;
	bboxDesc.SetBound(bound);
	bboxDesc.SetTransform(&CarMatrix);
	bboxDesc.SetIncludeFlags(nTestTypeFlags);
	bboxDesc.SetExtensionToBoundingBoxMin(Vector3(-0.25f, -1.75f, 0.0f));
	bboxDesc.SetExtensionToBoundingBoxMax(Vector3(0.25f, 0.25f, 0.0f));

	if(WorldProbe::GetShapeTestManager()->SubmitTest(bboxDesc))
	{
        BANK_ONLY(m_populationFailureData.m_numBoundingBoxSpecialFail++);
		vehPopDebugf2("Skipping veh spawn on link, spawned model collides with object");
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "spawned model collides with object");

#if __BANK
		if(ms_bDrawBoxesForVehicleSpawnAttempts)
		{
			grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(CarMatrix), Color_red, false, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
		}
#endif
		return;
	}

#if __BANK
	if(ms_bDrawBoxesForVehicleSpawnAttempts)
	{
		grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(CarMatrix), Color_cyan, false, ms_iDrawBoxesForVehicleSpawnAttemptsDuration);
	}
#endif

	// At this point we have decided we will definitely create the vehicle.
	// We are not allowed to remove the vehicle after it has been created. (This would add slowness)
	CVehicle *pNewVehicle = NULL;
	if(bReuseVehicle)
	{
		pNewVehicle = GetVehicleFromReusePool(reusedVehicleIndex);
		if(ReuseVehicleFromVehicleReusePool(pNewVehicle, CarMatrix, POPTYPE_RANDOM_AMBIENT, ENTITY_OWNEDBY_POPULATION, true, false))
		{
			RemoveVehicleFromReusePool(reusedVehicleIndex);
		}
		else
		{
			// reuse failed - maybe network is full?
			pNewVehicle = NULL;
		}
	}
	else
	{
		pNewVehicle = CVehicleFactory::GetFactory()->Create(carModelId, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_AMBIENT, &CarMatrix);
	}

	// pNewVehicle can be NULL in a network game if there is no available network objects for the new vehicle
	if (!pNewVehicle)
    {
        BANK_ONLY(m_populationFailureData.m_numVehicleCreateFail++);
		vehPopDebugf2("Skipping veh spawn on link, vehicle factory returned null");
		NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", "vehicle factory returned null");
		return;
	}

	// Keep track of how many ambient vehicles we created this frame
	PF_INCREMENT(AmbientVehiclesCreated);
#if __BANK
	pNewVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_PATHS;
#endif // __BANK

	// if reusing the vehicle, the NumTimesUsed doesn't need to be increased
	if(!bReuseVehicle)
		pNewVehicle->GetVehicleModelInfo()->IncreaseNumTimesUsed();

	pNewVehicle->SetRandomSeed(randomSeed);		// We had already picked a random seed. Use it now. (or the random placement offset won't be consistent)
	pNewVehicle->GetIntelligence()->SpeedMultiplier = SpeedMultiplier;
	pNewVehicle->GetIntelligence()->SpeedFromNodes = SpeedFromNodes;

	if (pNewVehicle->ShouldLowerConvertibleRoof())
		pNewVehicle->LowerConvertibleRoof((fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.2f) ? true : false);

	// Make sure we don't use the same colour we used for the previous car
	u8 newcol1, newcol2, newcol3, newcol4, newcol5, newcol6;
	pNewVehicle->GetVehicleModelInfo()->ChooseVehicleColourFancy(pNewVehicle, newcol1, newcol2, newcol3, newcol4, newcol5, newcol6);
	pNewVehicle->SetBodyColour1(newcol1);
	pNewVehicle->SetBodyColour2(newcol2);
	pNewVehicle->SetBodyColour3(newcol3);
	pNewVehicle->SetBodyColour4(newcol4);
	pNewVehicle->SetBodyColour5(newcol5);
	pNewVehicle->SetBodyColour6(newcol6);
	pNewVehicle->UpdateBodyColourRemapping();	// let shaders know that body colour changed

	const Vector3 vNewVehiclePos = VEC3V_TO_VECTOR3(pNewVehicle->GetTransform().GetPosition());
	Assert(vNewVehiclePos.x > WORLDLIMITS_XMIN);
	Assert(vNewVehiclePos.x < WORLDLIMITS_XMAX);
	Assert(vNewVehiclePos.y > WORLDLIMITS_YMIN);
	Assert(vNewVehiclePos.y < WORLDLIMITS_YMAX);

	// Switch on the engine
	pNewVehicle->SwitchEngineOn(true);
	pNewVehicle->SetRandomEngineTemperature();

	// new - all dynamic entity derived objects need to establish some tracking data before being added to world
	//pNewVehicle->GetPortalTracker()->GetInsertionData().StoreProbeResults(m_pDestInteriorInst, m_destRoomIdx);
	fwInteriorLocation	interiorLocation = CInteriorInst::CreateLocation( pDestInteriorInst, destRoomIdx );

	if (setWaitForAllCollisionsBeforeProbe)
	{
		pNewVehicle->GetPortalTracker()->SetWaitForAllCollisionsBeforeProbe(true);
	}

	CGameWorld::Add(pNewVehicle, interiorLocation);

#if __BANK
	AddVehicleToSpawnHistory(pNewVehicle, NULL, "GenerateRandomCarOnLinkPosition");
#endif

	if (generated == GENERATED_AT_TRAFFIC_LIGHTS || generated == GENERATED_AT_GIVE_WAY || generated == GENERATED_AT_DRIVE_THROUGH_WINDOW)
	{
		ms_bVehIsBeingCreated = true;

		// Set up occupants.
		if(pNewVehicle->CanSetUpWithPretendOccupants())
		{
			// Setup the veh with pretend occupants.
			// This also gives the vehicle a default task.
			pNewVehicle->SetUpWithPretendOccupants();
		}
		else
		{
			bool bDriverAdded = false;

			// Setup the veh with real occupants.
			if (bPolice)
			{
				bDriverAdded = AddPoliceVehOccupants(pNewVehicle);
			}
			else
			{
				bDriverAdded = AddDriverAndPassengersForVeh(pNewVehicle, 0, CVehicleAILodManager::ComputeRealDriversAndPassengersRemaining()) > 0;
			}

			pNewVehicle->GiveDefaultTask();

			if (!bDriverAdded)
			{
	 			ms_bVehIsBeingCreated = false;
	 
	 			// the driver could not be added, so we have to remove the vehicle
				vehPopDebugf2("Vehicle %s at traffic lights or drive through destroyed right after creation, couldn't create driver ped", pNewVehicle->GetModelName());
	             BANK_ONLY(ms_lastRemovalReason = Removal_OccupantAddFailed);
	 			RemoveVeh(pNewVehicle, true, Removal_OccupantAddFailed);
	             BANK_ONLY(m_populationFailureData.m_numDriverAddFail++);
	 			
	 			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateRandomCarOnLinkPosition", "Failed:", ", couldn't create driver ped");
	 			return;
			}
		}

		ms_bVehIsBeingCreated = false;

// 		float CruiseSpeed = PickCruiseSpeedWithVehModel(CarModel);
// 
// 		CTaskVehicleMissionBase *pTask = pNewVehicle->GetIntelligence()->GetActiveTask();
// 
// 		if(pTask)
// 		{
// 			CruiseSpeed = rage::Min(CruiseSpeed, CDriverPersonality::FindMaxCruiseSpeed(pNewVehicle->GetDriver()));
// 			pTask->SetCruiseSpeed(CruiseSpeed);
// 		}

//		if (generated == GENERATED_AT_DRIVE_THROUGH_WINDOW)		this pause caused cars to wait at toll booths before pulling up and stopping again at the barrier.
//		{
//			pMission->m_tempActionType = TEMPACT_WAIT;
//			pMission->m_TempActionFinish = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(7000, 17000);
//		}
	}

	switch(generated)
	{
		case GENERATED_AT_PETROL_STATION:
			//CScenarioManager::GiveScenarioToParkedCar( pNewVehicle, PT_ParkedAtPetrolPumps, Vehicle_LookingInBoot );
			pNewVehicle->m_nVehicleFlags.bIsParkedAtPetrolStation = true;
			break;

		case GENERATED_AT_DRIVE_THROUGH_WINDOW:
		case GENERATED_AT_TRAFFIC_LIGHTS:
		case GENERATED_AT_GIVE_WAY:
		default:
			break;
	}
	
	//Make the vehicle into a dummy based on distance.
	bool bSuperDummyConversionFailed = false;
	MakeVehicleIntoDummyBasedOnDistance(*pNewVehicle, RCC_VEC3V(vNewVehiclePos), bSuperDummyConversionFailed);

	// Set flag for whether this vehicle counts towards ambient population
	pNewVehicle->m_nVehicleFlags.bCountsTowardsAmbientPopulation = CountsTowardsAmbientPopulation(pNewVehicle);

	vehPopDebugf2("Successfully created vehicle 0x%8p on link position (%.2f, %.2f, %.2f), type: %s", pNewVehicle, vNewVehiclePos.x, vNewVehiclePos.y, vNewVehiclePos.z, pNewVehicle->GetModelName());

	NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "GenerateRandomCarOnLinkPosition", "Vehicle Created:", "%s", pNewVehicle->GetNetworkObject() ? pNewVehicle->GetNetworkObject()->GetLogName() : "No Network Object");

    if(pNewVehicle->GetNetworkObject())
    {
        //NetworkInterface::GetObjectManagerLog().WriteMessageHeader(CNetworkLog::LOG_LEVEL_MEDIUM, pNewVehicle->GetNetworkObject()->GetPhysicalPlayerIndex(), "CREATED_CAR_ON_LINK", "NODE INDEX %d", pNode->GetAddrIndex());
    }

	if(bForceFadeIn || vehicleInFov)
	{
		pNewVehicle->GetLodData().SetResetDisabled(false);
		pNewVehicle->GetLodData().SetResetAlpha(true);
	}

#if __BANK
	if(pNewVehicle)
	{
		pNewVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - startTime));
	}
#endif // __BANK
}

//-------------------------------------------------------------------------------------------
// NAME : ScanForNodesOnPlayersRoad
// PURPOSE : Scan for nodes ahead of the player on the road which he is driving
// Mark these with the 'm_onPlayersRoad' flag, which is then used by the population code
// to artificially increase the chances of spawning a vehicle on these nodes.

void CVehiclePopulation::CleanUpPlayersJunctionNode()
{
	if(!ms_PlayersJunctionNode.IsEmpty())
	{
		if(ms_PlayersJunctionNodeVehicle)
		{
			CJunction *pJunction = CJunctions::FindJunctionFromNode(ms_PlayersJunctionNode);

			if(pJunction)
			{
				pJunction->RemoveVehicle(ms_PlayersJunctionNodeVehicle);
			}

			ms_PlayersJunctionNodeVehicle = NULL;
		}

		ms_PlayersJunctionNode.SetEmpty();
		ms_PlayersJunctionEntranceNode.SetEmpty();
	}
}

void CVehiclePopulation::ScanForNodesOnPlayersRoad(bool bForce)
{
	CPathNode * pNodes[MAX_PLAYERS_ROAD_NODES];

	CPed * pPlayer = FindPlayerPed();

	if(!pPlayer)
	{
		CleanUpPlayersJunctionNode();
		return;
	}

	if(ThePaths.bStreamHeistIslandNodes)
	{
		CleanUpPlayersJunctionNode();
		ms_iNumNodesOnPlayersRoad = 0;
		return;
	}

	CVehicle * pVehicle = FindPlayerVehicle();

	if(ms_PlayersJunctionNodeVehicle && (pVehicle != ms_PlayersJunctionNodeVehicle || ms_PlayersJunctionNodeVehicle->GetStatus() == STATUS_WRECKED))
	{
		CleanUpPlayersJunctionNode();
	}

	if(ms_PlayersJunctionNodeVehicle)
	{
		aiTask * pActiveTask = ms_PlayersJunctionNodeVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY);

		if(!pActiveTask || !CTaskTypes::IsPlayerDriveTask(pActiveTask->GetTaskType()))
		{
			CleanUpPlayersJunctionNode();
		}
	}
	
	Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	CNodeAddress iPlayerNode = pPlayer->GetPlayerInfo()->GetPlayerDataNodeAddress();
	s16	iPlayerLink = pPlayer->GetPlayerInfo()->GetPlayerDataLocalLinkIndex();

	if(iPlayerNode.IsEmpty() || iPlayerLink == -1 || !ThePaths.IsRegionLoaded(iPlayerNode))
	{
		CleanUpPlayersJunctionNode();
		return;
	}

	const CPathNode * pPlayerNode = ThePaths.FindNodePointerSafe(iPlayerNode);
	const CPathNodeLink & link = ThePaths.GetNodesLink(pPlayerNode, iPlayerLink);
	const CPathNode * pNextNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);

	if(!pPlayerNode || !pNextNode)
	{
		CleanUpPlayersJunctionNode();
		return;
	}

	// url:bugstar:679794 - turn off player's road modifier when in flying vehicle
	if( pVehicle && pPlayer->GetIsInVehicle() )
	{
		if( pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli() || pVehicle->InheritsFromAutogyro() )
		{
			return;
		}
	}
	// Also turn off modifier when player is on foot
	static dev_bool bAllowOnFoot = true;
	if( !bAllowOnFoot && !pPlayer->GetIsInVehicle() )
	{
		CleanUpPlayersJunctionNode();
		return;
	}

	static const CPathNode * pCachedPlayerNode = NULL;
	static float fCachedCameraHeading = 0.0f;

	const float fCameraHeading = camInterface::GetHeading();

	if(pCachedPlayerNode != pPlayerNode || Abs(fCameraHeading - fCachedCameraHeading) > 45.0f * DtoR || bForce)
	{
		// Clear out all the flags previously set
		ClearOnPlayersRoadFlags();

		Vector3 vDir(-Sinf(fCameraHeading), Cosf(fCameraHeading), 1.0f);

		const CPathNode *pPlayerJunctionNode;
		const CPathNode *pPlayerJunctionEntranceNode;

		ms_iNumNodesOnPlayersRoad = ThePaths.MarkPlayersRoadNodes(vPos, iPlayerNode, ms_fPlayersRoadScanDistance, vDir, pNodes, MAX_PLAYERS_ROAD_NODES, &pPlayerJunctionNode, &pPlayerJunctionEntranceNode);

		for(s32 n=0; n<ms_iNumNodesOnPlayersRoad; n++)
		{
			ms_aNodesOnPlayersRoad[n] = pNodes[n]->m_address;
		}

		if(!ms_PlayersJunctionNode.IsEmpty() && (!pPlayerJunctionNode || pPlayerJunctionNode->m_address != ms_PlayersJunctionNode))
		{
			CleanUpPlayersJunctionNode();
		}

		if(pVehicle && pVehicle->GetStatus() != STATUS_WRECKED && pPlayerJunctionNode)
		{
			aiTask * pActiveTask = pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY);

			if(pActiveTask && CTaskTypes::IsPlayerDriveTask(pActiveTask->GetTaskType()))
			{
				if(pPlayerJunctionEntranceNode)
				{
					ms_PlayersJunctionEntranceNode = pPlayerJunctionEntranceNode->m_address;
				}

				if(ms_PlayersJunctionNode != pPlayerJunctionNode->m_address)
				{
					ms_PlayersJunctionNode = pPlayerJunctionNode->m_address;
					ms_PlayersJunctionNodeVehicle = pVehicle;

					CJunction *pJunction = CJunctions::FindJunctionFromNode(pPlayerJunctionNode->m_address);

					if(pJunction)
					{
						pJunction->AddVehicle(pVehicle);
					}
					else
					{
						ms_PlayersJunctionNodeVehicle = NULL;
						ms_PlayersJunctionNode.SetEmpty();
						ms_PlayersJunctionEntranceNode.SetEmpty();
					}
				}
			}
		}

		pCachedPlayerNode = pPlayerNode;
		fCachedCameraHeading = fCameraHeading;
	}
}

void CVehiclePopulation::ClearOnPlayersRoadFlags()
{
	s32 n;
	for(n=0; n<ms_iNumNodesOnPlayersRoad; n++)
	{
		if(!ms_aNodesOnPlayersRoad[n].IsEmpty())
		{
			CPathNode * pNode = ThePaths.FindNodePointerSafe(ms_aNodesOnPlayersRoad[n]);
			if(pNode)
			{
				pNode->m_1.m_onPlayersRoad = false;
				Assert(pNode->m_distanceToTarget == PF_VERYLARGENUMBER);
			}
			ms_aNodesOnPlayersRoad[n].SetEmpty();
		}
	}

	ms_iNumNodesOnPlayersRoad = 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsAmbientBoatNearby
// PURPOSE :  Returns true if there is an ambient boat within the specified range
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::IsAmbientBoatNearby(Vector3 &vCoors, float range)
{
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		CVehicle* pVeh = VehiclePool->GetSlot(i);
		if(pVeh && !pVeh->GetIsInReusePool() && pVeh->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			if ( (vCoors - VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition())).XYMag2() < range*range)
			{
				return true;
			}
		}
	}
	return false;
}

bool CVehiclePopulation::WillVehicleCollideWithOtherCarUsingRestrictiveChecks(const Matrix34& vehMatrix, const Vector3& vBoundBoxMin, const Vector3& vBoundBoxMax)
{
	const u32 iTime = fwTimer::GetTimeInMilliseconds();

	CVehicle::Pool *VehiclePool = CVehicle::GetPool();

	spdAABB ourBoxLocal(VECTOR3_TO_VEC3V(vBoundBoxMin), VECTOR3_TO_VEC3V(vBoundBoxMax));
	spdOrientedBB ourBoxWorld(ourBoxLocal, MATRIX34_TO_MAT34V(vehMatrix));

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		CVehicle* pVeh = VehiclePool->GetSlot(i);
		if (Unlikely(pVeh && !pVeh->GetIsInReusePool() && (pVeh->m_nVehicleFlags.bOtherCarsShouldCheckTrajectoryBeforeSpawning || pVeh->m_TimeOfCreation == iTime)))
		{
			//build a bounding box for the vehicle we care about
			spdAABB tempBox;
			spdAABB otherBoxLocal = pVeh->GetLocalSpaceBoundBox(tempBox);
			Vec3V vOtherBoxLocalMax = otherBoxLocal.GetMax();

			//now stretch it out to include velocity
			//this assumes we're going forward
			const Vector3& vVelocity = pVeh->GetVelocity();
			const float fSpeed = vVelocity.XYMag();
			
			Mat34V vOtherMat = pVeh->GetTransform().GetMatrix();
			static dev_float s_fNumSecsToPlanAhead = 1.5f;
			
			ScalarV otherBoxLocalMaxY = vOtherBoxLocalMax.GetY();
			otherBoxLocalMaxY += ScalarV(fSpeed * s_fNumSecsToPlanAhead);
			vOtherBoxLocalMax.SetY(otherBoxLocalMaxY);
			otherBoxLocal.SetMax(vOtherBoxLocalMax);

			spdOrientedBB otherBoxWorld(otherBoxLocal, vOtherMat);
			//grcDebugDraw::BoxOriented(otherBoxLocal.GetMin(), otherBoxLocal.GetMax(), vOtherMat, Color_orange, false, 10);
			if (otherBoxWorld.IntersectsOrientedBB(ourBoxWorld))
			{
				return true;
			}
		}
	}

	return false;
}

bool CVehiclePopulation::IsVehicleFacingPotentialHeadOnCollision(const Vector3& vCoors, const Vector3& vLinkDir, const CPathNodeLink* pLink, const float fRange)
{
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();

	Assert(pLink);

	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		CVehicle* pVeh = VehiclePool->GetSlot(i);
		if (pVeh && !pVeh->GetIsInReusePool())
		{
			//rough distance threshold
			if ( (vCoors - VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition())).XYMag2() > fRange*fRange)
			{
				continue;
			}

			//if they're within some distance, check if our nodes are in the other guy's nodelist
			const CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
			if (!pNodeList)
			{
				continue;
			}

			s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;

			//if a nodelist exists but hasn't been populated yet, we can't do anythign with it
			if (iOldNode < 0)
			{
				continue;
			}

			for (int nodeIndex = iOldNode; nodeIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; nodeIndex++)
			{
				const CNodeAddress nodeAddr = pNodeList->GetPathNodeAddr(nodeIndex);
				if (!nodeAddr.IsEmpty() && nodeAddr == pLink->m_OtherNode)
				{
					Vector3 vOldCoords, vNewCoords;
					if (nodeIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED - 1 )
					{
						ThePaths.FindNodePointer(nodeAddr)->GetCoors(vOldCoords);
						const CPathNode* pNewNode = pNodeList->GetPathNode(nodeIndex+1);
						if (!pNewNode)
						{
							return false;
						}
						pNewNode->GetCoors(vNewCoords);
					}
					else
					{
						const CPathNode* pOldNode = pNodeList->GetPathNode(nodeIndex-1);
						Assert(pOldNode);
						if (!pOldNode)
						{
							return false;
						}
						pOldNode->GetCoors(vOldCoords);
						ThePaths.FindNodePointer(nodeAddr)->GetCoors(vNewCoords);
					}
					Vector3 vOtherLinkDir = vNewCoords - vOldCoords;

					//if the two node pairs are facing opposite directions, we have a potential collision
					if (vOtherLinkDir.Dot(vLinkDir) < 0.0f)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
float CVehiclePopulation::PickCruiseSpeedWithVehModel(const CVehicle* pVehicle, u32 carModel)
{
	// Work out what speed the vehicle will be going at.
	// We need to know this here so that we can pick the appropriate lane.
	float	cruiseSpeed;
	CVehicleModelInfo *pVehModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(carModel)));

	float fLowerBound = -1.0f;
	float fUpperBound = -1.0f;

	if (pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT) && pVehModelInfo->GetIsBoat())
	{
		fLowerBound = 10.0f;
		fUpperBound = 14.0f;
	}
	else
	{
		// If this is a sports car it will go a bit faster
		if ( pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS) )
		{
			fLowerBound = 12.0f;
			fUpperBound = 18.0f;
		}
		else if (CVehicle::IsTractorModelId(fwModelId(strLocalIndex(carModel))))
		{
			fLowerBound = 7.0f;
			fUpperBound = 10.0f;
		}
		else if ( pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) )
		{
			fLowerBound = 7.0f;
			fUpperBound = 12.0f;
		}
		else
		{
			fLowerBound = 9.0f;
			fUpperBound = 16.0f;
		}
	}

	Assert(fLowerBound >= 0.0f);
	Assert(fUpperBound >= 0.0f);
	if (pVehicle)
	{
		mthRandom rnd(pVehicle->GetRandomSeed());
		cruiseSpeed = rnd.GetRanged(fLowerBound, fUpperBound);
	}
	else
	{
		cruiseSpeed = fwRandom::GetRandomNumberInRange(fLowerBound, fUpperBound);
	}

	return cruiseSpeed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateOneEmergencyServicesCar
// PURPOSE :  Possibly generates an emergency services car.
/////////////////////////////////////////////////////////////////////////////////
CVehicle* CVehiclePopulation::GenerateCopVehicleChasingCriminal(const Vector3& position, fwModelId iModelId, CNodeAddress& SpawnNode, CNodeAddress& SpawnPreviousNode)
{
	if(!SpawnNode.IsEmpty() && !SpawnPreviousNode.IsEmpty())
	{
		Vector3 CopSpawnPosition;
		ThePaths.FindNodePointer(SpawnNode)->GetCoors(CopSpawnPosition);

		Vector3 CopPreviousSpawnPosition; 
		ThePaths.FindNodePointer(SpawnPreviousNode)->GetCoors(CopPreviousSpawnPosition);

		Vector3 vDirection = CopSpawnPosition - CopPreviousSpawnPosition;

		//Generate the input arguments for the road node search.
		CPathFind::FindNodeInDirectionInput roadNodeInput(SpawnNode, vDirection, 10.0f);

		//Set the maximum distance traveled 
		roadNodeInput.m_fMaxDistanceTraveled = 25.0f;

		//Add some variance to the scoring.
		float s_fMaxRandomVariance = 1.1f;
		roadNodeInput.m_fMaxRandomVariance = s_fMaxRandomVariance;

		//Ensure there is a route from the spawn point to the target.
		roadNodeInput.m_bCanFollowOutgoingLinks = false;
		roadNodeInput.m_bCanFollowIncomingLinks = true;

		//Find a road node in the road direction.
		CPathFind::FindNodeInDirectionOutput roadNodeOutput;
		if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
		{
			return NULL;
		}

		//Save nodes
		SpawnNode = roadNodeOutput.m_Node;
		SpawnPreviousNode = roadNodeOutput.m_PreviousNode;
	}
	else
	{
		//Find a close by node
		CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(position,100.0f,CPathFind::IgnoreSwitchedOffNodes);
		if (NearestNode.IsEmpty())
		{
			return NULL;
		}

		//Generate a random direction.
		float fAngle = fwRandom::GetRandomNumberInRange(-PI, PI);
		Vector2 vDirection(cos(fAngle), sin(fAngle));

		//Generate the input arguments for the road node search.
		CPathFind::FindNodeInDirectionInput roadNodeInput(NearestNode, Vector3(vDirection, Vector2::kXY), 40.0f);

		//Set the maximum distance traveled 
		roadNodeInput.m_fMaxDistanceTraveled = 160.0f;

		//Add some variance to the scoring.
		float s_fMaxRandomVariance = 1.1f;
		roadNodeInput.m_fMaxRandomVariance = s_fMaxRandomVariance;

		//Ensure there is a route from the spawn point to the target.
		roadNodeInput.m_bCanFollowOutgoingLinks = false;
		roadNodeInput.m_bCanFollowIncomingLinks = true;

		//Find a road node in the road direction.
		CPathFind::FindNodeInDirectionOutput roadNodeOutput;
		if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
		{
			return NULL;
		}

		//Save nodes
		SpawnNode = roadNodeOutput.m_Node;
		SpawnPreviousNode = roadNodeOutput.m_PreviousNode;
	}

	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(SpawnNode);
	if(!pNode)
	{
		return NULL;
	}

	//Grab the node position.
	Vector3 vPosition;
	pNode->GetCoors(vPosition);

	//Ensure the position is (relatively) off-screen.
	//Grab the camera position.
	Vec3V vCameraPosition = VECTOR3_TO_VEC3V(camInterface::GetPos());

	//Check if the position is close enough to require viewport checks.
	float fMaxDistanceFromCameraForViewportChecks = 150.0f;
	ScalarV scDistSq = DistSquared(VECTOR3_TO_VEC3V(vPosition), vCameraPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistanceFromCameraForViewportChecks));
	if(IsLessThanAll(scDistSq, scMaxDistSq))
	{
		//Ensure the spawn position is not visible in the viewport.
		float fRadiusForViewportCheck = 5.0f;
		if(camInterface::IsSphereVisibleInGameViewport(vPosition, fRadiusForViewportCheck))
		{
			return NULL;
		}

		//Check if a network game is in progress.
		if(NetworkInterface::IsGameInProgress())
		{
			//Ensure the spawn position is not visible to any remote player.
			if(NetworkInterface::IsVisibleToAnyRemotePlayer(vPosition, fRadiusForViewportCheck, fMaxDistanceFromCameraForViewportChecks))
			{
				return NULL;
			}
		}
	}

	//Generate the input.
	CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodesInput input(SpawnNode, SpawnPreviousNode, iModelId);

	//If the vehicle cannot be placed on the road properly, it should not result in failure.
	//This is because we sometimes spawn vehicles where map collision has not been loaded in.
	//In these cases, the car cannot be placed on the road properly since the probes will not be able to
	//detect where the road is.  These cars will end up being spawned slightly inside of the road, but
	//they will be dummied, and when they reach collision, they should right themselves.
	input.m_bPlaceOnRoadProperlyCausesFailure = false;

	//Use bounding boxes instead of bounding spheres when checking for collisions.
	//We used to use bounding spheres, but this became a problem when trying to spawn on single-lane
	//roads with fire hydrants and other obstructions on the side of the road.
	input.m_bUseBoundingBoxesForCollision = true;

	//Try to clear objects out of the way.
	//No reason to block vehicles from spawning if there is ambient stuff there.
	input.m_bTryToClearAreaIfBlocked = true;

	//Generate a vehicle with the road nodes.
	CVehicle* pVehicle = CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodes(input);

	return pVehicle;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateOneEmergencyServicesCar
// PURPOSE :  Possibly generates an emergency services car.
/////////////////////////////////////////////////////////////////////////////////
CVehicle* CVehiclePopulation::GenerateOneCriminalVehicle(CNodeAddress CopSpawnNode, CNodeAddress CopSpawnPreviousNode, bool bCanDoDriveBys)
{
#if __BANK
	utimer_t startTime = sysTimer::GetTicks();
#endif // __BANK

	if (CopSpawnNode.IsEmpty() || CopSpawnPreviousNode.IsEmpty())
	{
		return NULL;
	}

	Vector3 CopSpawnPosition;
	ThePaths.FindNodePointer(CopSpawnNode)->GetCoors(CopSpawnPosition);

	Vector3 CopPreviousSpawnPosition; 
	ThePaths.FindNodePointer(CopSpawnPreviousNode)->GetCoors(CopPreviousSpawnPosition);

	Vector3 vDirection = CopPreviousSpawnPosition - CopSpawnPosition;

	//Generate the input arguments for the road node search.
	CPathFind::FindNodeInDirectionInput roadNodeInput(CopSpawnNode, vDirection, 3.0f);

	//Find a road node in the road direction.
	CPathFind::FindNodeInDirectionOutput roadNodeOutput;
	if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
	{
		return NULL;
	}

	const CPathNode* pNearestNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_Node);
	const CPathNode* pPreviousNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_PreviousNode);
	if (!pNearestNode || !pPreviousNode)
	{
		return NULL;
	}

	//Generate the coordinates of the node.
	Vector3 vNodePosition;
	pNearestNode->GetCoors(vNodePosition);

	//Generate the coordinates of the previous node.
	Vector3 vPreviousNodePosition;
	pPreviousNode->GetCoors(vPreviousNodePosition);

	//Generate the new forward vector.
	Vector3 vNewForward = vNodePosition - vPreviousNodePosition;

	//Make sure the vehicle heads away from the cop vehicle
	if(vNewForward.Dot(vDirection) < 0.0f )
	{
		return NULL;
	}

	//Normalize the forward vector.
	if(!vNewForward.NormalizeSafeRet())
	{
		return NULL;
	}

	//Generate the new side vector.
	Vector3 vNewSide;
	vNewSide.Cross(vNewForward, ZAXIS);
	if(!vNewSide.NormalizeSafeRet())
	{
		return NULL;
	}

	//Generate the new up vector.
	Vector3 vNewUp;
	vNewUp.Cross(vNewSide, vNewForward);

	//Create the matrix for the vehicle.
	Matrix34 mVehicle(Matrix34::IdentityType);
	mVehicle.a = vNewSide;
	mVehicle.b = vNewForward;
	mVehicle.c = vNewUp;
	mVehicle.d = vNodePosition;

	//Find the link from the previous node to the node.
	s16 iLink = -1;
	if(!ThePaths.FindNodesLinkIndexBetween2Nodes(pPreviousNode, pNearestNode, iLink))
	{
		return NULL;
	}

	//Get the link.
	const CPathNodeLink& rLink = ThePaths.GetNodesLink(pPreviousNode, iLink);

	//Move the car over a bit to the right side of the road.
	float fInitialLaneCenterOffset = rLink.InitialLaneCenterOffset();
	mVehicle.d.x += vNewForward.y * fInitialLaneCenterOffset;
	mVehicle.d.y -= vNewForward.x * fInitialLaneCenterOffset;

	//Ensure the position is (relatively) off-screen.
	//Grab the camera position.
	Vec3V vCameraPosition = VECTOR3_TO_VEC3V(camInterface::GetPos());

	//Check if the position is close enough to require viewport checks.
	float fMaxDistanceFromCameraForViewportChecks = 150.0f;
	ScalarV scDistSq = DistSquared(VECTOR3_TO_VEC3V(mVehicle.d), vCameraPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistanceFromCameraForViewportChecks));
	if(IsLessThanAll(scDistSq, scMaxDistSq))
	{
		//Ensure the spawn position is not visible in the viewport.
		float fRadiusForViewportCheck = 5.0f;
		if(camInterface::IsSphereVisibleInGameViewport(mVehicle.d, fRadiusForViewportCheck))
		{
			return NULL;
		}

		//Check if a network game is in progress.
		if(NetworkInterface::IsGameInProgress())
		{
			//Ensure the spawn position is not visible to any remote player.
			if(NetworkInterface::IsVisibleToAnyRemotePlayer(mVehicle.d, fRadiusForViewportCheck, fMaxDistanceFromCameraForViewportChecks))
			{
				return NULL;
			}
		}
	}

	/////////////////////////////////////////
	//Vehicle Model type
	/////////////////////////////////////////
	// If both nodes are on the same street, then take a note of the name hash
	const u32 iStreetNameHash = pNearestNode->m_streetNameHash ;

	// Pick veh from percentages in zone (could still be a police veh)
	bool bOnHighWay = pNearestNode->IsHighway();
	bool offroad = pNearestNode->IsOffroad();
	bool bNoBigVehicles = pNearestNode->BigVehiclesProhibited();
	bool bSmallWorkers = pNearestNode->m_1.m_specialFunction == SPECIAL_SMALL_WORK_VEHICLES;
	bool bPolice = false;
	bool bSlipLane = pNearestNode->IsSlipLane();

	//don't allow creation of big vehicles in sliplanes, even if the road were otherwise good
	//vehicles will never choose to enter one of these, so don't start them there
	bNoBigVehicles |= bSlipLane;

	CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();

	strLocalIndex vehModel = strLocalIndex(fwModelId::MI_INVALID);
	vehModel = CVehiclePopulation::ChooseModel(&bPolice,mVehicle.d, pWanted ? (pWanted->GetWantedLevel() >= WANTED_LEVEL1) : false, bSmallWorkers, bOnHighWay, iStreetNameHash, offroad, bNoBigVehicles, 1);

	if(!CModelInfo::IsValidModelInfo(vehModel.Get()))
	{
		return NULL;
	}

	fwModelId vehModelId(vehModel);
	if( !aiVerifyf(CModelInfo::HaveAssetsLoaded(vehModelId), "GenerateOneCriminalVehicle model requested, but not loaded!" ) )
	{
		return NULL;
	}

	/////////////////////////////////////////
	//Spawn vehicle
	/////////////////////////////////////////

	eVehicleDummyMode desiredLod = CVehicleAILodManager::CalcDesiredLodForPoint(VECTOR3_TO_VEC3V(mVehicle.d));
	bool bCreateInactive = (desiredLod==VDM_SUPERDUMMY && CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode);

	if (!gPopStreaming.IsFallbackPedAvailable())
		return NULL;

	CVehicle* pVehicle = CVehicleFactory::GetFactory()->Create(fwModelId(vehModel), ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_AMBIENT, &mVehicle, true, bCreateInactive);

	if(!pVehicle)
	{ 
		return NULL;
	}
	BANK_ONLY(pVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_CRIMINAL);

	CPedPopulation::ChooseCivilianPedModelIndexArgs args;
	args.m_RequiredGender = GENDER_MALE;
	args.m_IgnoreMaxUsage = true;
	args.m_GroupGangMembers = true;
	args.m_MayRequireBikeHelmet = pVehicle->InheritsFromBike() ? true : false;

	u32 NewPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);

	if ( !CModelInfo::IsValidModelInfo(NewPedModelIndex) )
	{
		RemoveVeh(pVehicle, true, Removal_OccupantAddFailed);
		return NULL;
	}

	CPed* pDriver = pVehicle->SetUpDriver(false, true, NewPedModelIndex);
	if(!pDriver)
	{
		RemoveVeh(pVehicle, true, Removal_OccupantAddFailed);
		return NULL;
	}

	pDriver->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, CWeapon::LOTS_OF_AMMO);

	//Randomly spawn peds in the seats.
	s32 iMaxPeds = pVehicle->GetSeatManager()->GetMaxSeats();
	iMaxPeds = rage::Clamp(iMaxPeds, 1, 4);
	for(int i = 1; i < iMaxPeds; ++i)
	{
		static float s_fChances = 0.5f;
		if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChances)
		{
			NewPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);

			if ( CModelInfo::IsValidModelInfo(NewPedModelIndex) )
			{
				CPed* pPassenger = pVehicle->SetupPassenger(i,true,NewPedModelIndex);
				if(pPassenger)
				{
					pPassenger->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, CWeapon::LOTS_OF_AMMO);
				}
			}
		}
	}

	//Set the ped attributes.
	for(int i = 0; i < iMaxPeds; ++i)
	{
		CPed* pPedInSeat = pVehicle->GetPedInSeat(i);
		if(pPedInSeat)
		{
			bool bIsDriver = (i == 0);

			//The driver should flee.
			if(bIsDriver)
			{
				pPedInSeat->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_FleeWhilstInVehicle);
				pPedInSeat->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatSpeedToFleeInVehicle, 22.0f);
			}

			pPedInSeat->GetPedIntelligence()->GetCombatBehaviour().ChangeFlag(CCombatData::BF_CanDoDrivebys, bCanDoDriveBys);
			pPedInSeat->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_AlwaysEquipBestWeapon);
		}
	}

	//Add each ped to a group.
	s32 iPedGroupIndex = CPedGroups::AddGroup(POPTYPE_RANDOM_AMBIENT);
	if(aiVerifyf(iPedGroupIndex >= 0, "Invalid ped group index, group array is full?"))
	{
		//Grab the group.
		CPedGroup& rGroup = CPedGroups::ms_groups[iPedGroupIndex];

		//The leader is the driver.
		s32 iLeaderSeat = 0;

		//Add the leader.
		CPed* pLeader = pVehicle->GetSeatManager()->GetPedInSeat(iLeaderSeat);
		if(aiVerifyf(pLeader, "The leader is invalid -- no ped in seat: %d.", iLeaderSeat))
		{
			rGroup.GetGroupMembership()->SetLeader(pLeader);	
			rGroup.Process();
		}

		//Add the followers.
		for(s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); ++iSeat)
		{
			//Ensure the seat is not the leader.
			if(iSeat == iLeaderSeat)
			{
				continue;
			}

			//Ensure the ped is valid.
			CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if(!pPed)
			{
				continue;
			}

			//Add the follower.
			rGroup.GetGroupMembership()->AddFollower(pPed);
			rGroup.Process();
		}
	}

	/////////////////////////////////////////
	//SetUp stuff
	/////////////////////////////////////////

	// Make sure we don't use the same colour we used for the previous veh
	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(vehModelId);

	u8 newcol1, newcol2, newcol3, newcol4, newcol5, newcol6;
	pVehicle->m_nVehicleFlags.bGangVeh = false;
	pModelInfo->ChooseVehicleColourFancy(pVehicle, newcol1, newcol2, newcol3, newcol4, newcol5, newcol6);
	pVehicle->SetBodyColour1(newcol1);
	pVehicle->SetBodyColour2(newcol2);
	pVehicle->SetBodyColour3(newcol3);
	pVehicle->SetBodyColour4(newcol4);
	pVehicle->SetBodyColour5(newcol5);
	pVehicle->SetBodyColour6(newcol6);
	pVehicle->UpdateBodyColourRemapping();	// let shaders know that body colour changed

	// Switch on the engine
	pVehicle->SwitchEngineOn(true);
	pVehicle->SetRandomEngineTemperature();

	pVehicle->m_nVehicleFlags.bCountsTowardsAmbientPopulation = true;

	CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);

	CPortalTracker *pPortalTracker = pVehicle->GetPortalTracker();
	CPortalTracker::eProbeType probeType = GetVehicleProbeType(pVehicle);

	pPortalTracker->SetProbeType(probeType);
	pPortalTracker->RequestRescanNextUpdate();
	pPortalTracker->Update(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));

	pVehicle->ActivatePhysics();

	//Speed 
	float	CruiseSpeed = CVehiclePopulation::PickCruiseSpeedWithVehModel(pVehicle, vehModel.Get());

	CruiseSpeed = rage::Min(CruiseSpeed, CDriverPersonality::FindMaxCruiseSpeed(pVehicle->GetDriver(), pVehicle, pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE));

	pVehicle->GetVehicleModelInfo()->IncreaseNumTimesUsed();

	pVehicle->SelectAppropriateGearForSpeed();

	float RequestedSpeed = CVehicleIntelligence::MultiplyCruiseSpeedWithMultiplierAndClip( CruiseSpeed, pVehicle->GetIntelligence()->SpeedMultiplier, true, false, mVehicle.d);

	const s8 SpeedFromNodes = pPreviousNode->m_2.m_speed;
	const float SpeedMultiplier = CVehicleIntelligence::FindSpeedMultiplierWithSpeedFromNodes(SpeedFromNodes);
	float InitialSpeed = CruiseSpeed * SpeedMultiplier;	

	InitialSpeed = RequestedSpeed;
	float SpeedMultiplierBend = 1.0f;
	InitialSpeed = rage::Min(InitialSpeed, (SpeedMultiplierBend*RequestedSpeed));

	Vector3 vInitialVelocity;
	vInitialVelocity.SetScaled(mVehicle.b,InitialSpeed);
	if(!pVehicle->m_nVehicleFlags.bCreatingAsInactive)
	{
		pVehicle->SetVelocity(vInitialVelocity);
	}

	NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "CreateCarForScript", "Vehicle Created:", "%s", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "No Network Object");

#if __BANK
	if(pVehicle)
	{
		pVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - startTime));
	}
#endif // __BANK

	return pVehicle;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateOneEmergencyServicesCar
// PURPOSE :  Possibly generates an emergency services car.
/////////////////////////////////////////////////////////////////////////////////
CVehicle* CVehiclePopulation::GenerateOneEmergencyServicesCar(const Vector3& popCtrlCentre, const float rangeScale, fwModelId modelId, const Vector3& targetPos, const float minCreationDist, const bool bIgnoreDensity, Vector2 vCreationDir, float fCreationDot, bool bIncludeSwitchedOff, bool bAllowedAgainstFlowOfTraffic)
{
	//Generate the input.
	GenerateOneEmergencyServicesCarInput input(popCtrlCentre, rangeScale, modelId, targetPos, minCreationDist, bIgnoreDensity);
	input.m_vCreationDir = vCreationDir;
	input.m_fCreationDot = fCreationDot;
	input.m_bIncludeSwitchedOff = bIncludeSwitchedOff;
	input.m_bAllowedAgainstFlowOfTraffic = bAllowedAgainstFlowOfTraffic;
	
	//Generate the vehicle.
	return GenerateOneEmergencyServicesCar(input);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateOneEmergencyServicesCar
// PURPOSE :  Possibly generates an emergency services car.
/////////////////////////////////////////////////////////////////////////////////
CVehicle* CVehiclePopulation::GenerateOneEmergencyServicesCar(const GenerateOneEmergencyServicesCarInput& rInput)
{
	float			Length;
	float			Fraction=0.0f;
	Vector3 		Dir;
	Vector3			toCoors, fromCoors;
	CVehicle		*pNewCar;
	CNodeAddress	FromNode, ToNode;

	s32			Tries;
	Matrix34		tempMat;

	const CPathNode *pNodeTo = NULL;
	const CPathNode *pNodeFrom = NULL;

	if( !aiVerifyf(CModelInfo::HaveAssetsLoaded(rInput.m_iModelId), "GenerateOneEmergencyServicesCar: Called but model not loaded!" ) )
	{
		return NULL;
	}

	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(rInput.m_iModelId);
	if( !aiVerifyf(pModelInfo, "GenerateOneEmergencyServicesCar: No model info found for vehicle model!") )
	{
		return NULL;
	}
	if( !aiVerifyf(pModelInfo->GetFragType(), "GenerateOneEmergencyServicesCar: Model marked as loaded but no fragtype available!") )
	{
		return NULL;
	}
	if( !aiVerifyf(pModelInfo->GetFragType()->GetCommonDrawable(), "GenerateOneEmergencyServicesCar: No model info found for vehicle model!") )
	{
		return NULL;
	}

	vehPopDebugf2("Trying to create one emergency services car");
	
	// Try a couple of times finding a good place to generate the ambulance
	Tries = 0;
#define NUM_TRIES_HERE (5)

	pNewCar = NULL;
	while ((!pNewCar) && Tries < NUM_TRIES_HERE)
	{
		bool maybeFoundOne = false;

		const bool bAllowGoAgainstTraffic = true;
		maybeFoundOne = GenerateVehCreationPosFromPaths(
			rInput.m_vPopCtrlCentre, rInput.m_fRangeScale, rInput.m_vCreationDir.x, rInput.m_vCreationDir.y, rInput.m_fCreationDot, true,
			rage::Max(rInput.m_fMinCreationDist, GetCreationDistance(rInput.m_fRangeScale)), rage::Max(rInput.m_fMinCreationDist, GetCreationDistanceOffScreen(rInput.m_fRangeScale)),
			rInput.m_bIncludeSwitchedOff, true,
			&tempMat.d, &FromNode, &ToNode, &Fraction, rInput.m_bIgnoreDensity, bAllowGoAgainstTraffic);

		if (maybeFoundOne)
		{
			{
				pNodeTo = ThePaths.FindNodePointer(ToNode);
				pNodeTo->GetCoors(toCoors);
				pNodeFrom = ThePaths.FindNodePointer(FromNode);
				pNodeFrom->GetCoors(fromCoors);

				bool bFlipNodes = false;

				// We can only consider switching the nodes around if we're not on a dead-end node. If this is the case the nodes have already been ordered so that the vehicle moves away from the dead end.
				if (pNodeTo->m_2.m_deadEndness==0 && pNodeFrom->m_2.m_deadEndness == 0)
				{
					// We can only consider switching the nodes if this would not place the cars traveling against the flow of traffic.
					if (rInput.m_bAllowedAgainstFlowOfTraffic)
					{
						// Make sure the nodes point in the right general direction of the target.
						if ( (toCoors - rInput.m_vTargetPos).XYMag2() > (fromCoors - rInput.m_vTargetPos).XYMag2() )
						{
							bFlipNodes = true;
						}
					}
				}
				// Flip the nodes anyway, they should be facing away from the player because of the search
				else
				{
					bFlipNodes = true;
				}

				if( bFlipNodes )
				{
					Vector3			tmpCoors;
					CNodeAddress	tmpNode;
					const CPathNode*	pTmpNode;

					tmpCoors = toCoors;
					toCoors = fromCoors;
					fromCoors = tmpCoors;

					tmpNode = ToNode;
					ToNode = FromNode;
					FromNode = tmpNode;

					pTmpNode = pNodeTo;
					pNodeTo = pNodeFrom;
					pNodeFrom = pTmpNode;
				}
			}

			// Move the car up a bit to the right side of the road.
			s16 iLink = -1;
			const bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(pNodeFrom, pNodeTo, iLink);
			if(bLinkFound)
			{
#if __DEV
				float laneCentreOffset = ThePaths.bMakeAllRoadsSingleTrack ? 
					0.0f : ThePaths.GetNodesLink(pNodeFrom, iLink).InitialLaneCenterOffset();
#else
				float laneCentreOffset = ThePaths.GetNodesLink(pNodeFrom, iLink).InitialLaneCenterOffset();
#endif // __DEV

				Dir = toCoors - fromCoors;
				Dir.NormalizeSafe(Vector3(0.0f,1.0f,0.0f));
				tempMat.d.x += Dir.y * laneCentreOffset;
				tempMat.d.y -= Dir.x * laneCentreOffset;

				if (!ThePaths.FindNodePointer(FromNode)->m_2.m_waterNode)
				{
					// We have to aim the car in the direction it will be going. Otherwise
					// we get all sorts of nasty maneuvers when they are created.
					// Simply use the nodes for that.
					Dir = toCoors - fromCoors;
					Length = rage::Sqrtf(Dir.x*Dir.x + Dir.y*Dir.y);
					if (Length == 0.0f)
					{
						Dir.x = 1.0f;
					}
					else
					{
						Dir.x /= Length;
						Dir.y /= Length;
					}
					tempMat.b.x = Dir.x;	// Forward
					tempMat.b.y = Dir.y;
					tempMat.b.z = 0.0f;
					tempMat.a.x = Dir.y;	// Side
					tempMat.a.y = -Dir.x;
					tempMat.a.z = 0.0f;
					tempMat.c.x = 0.0f;		// Up
					tempMat.c.y = 0.0f;
					tempMat.c.z = 1.0f;

					// Find the z coordinate as interpolation of the z coords of the two nodes we are created between
					tempMat.d.z = fromCoors.z * (1.0f - Fraction) + toCoors.z * Fraction;

					//Create the car.
					GenerateOneEmergencyServicesVehicleWithMatrixInput input(tempMat, rInput.m_iModelId);
					pNewCar = GenerateOneEmergencyServicesVehicleWithMatrix(input);
				}
			}
		}
		Tries++;
	}

	if (pNewCar)
	{
#if __BANK
		AddVehicleToSpawnHistory(pNewCar, NULL, "GenerateOneEmergencyServicesCar");
#endif

		if (rInput.m_iModelId == MI_CAR_AMBULANCE)
        {
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_AMBULANCE.GetModelIndex(), rInput.m_vTargetPos);
		}
		else if (rInput.m_iModelId == MI_CAR_FIRETRUCK)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_FIRETRUCK.GetModelIndex(), rInput.m_vTargetPos);
		}
		else if(rInput.m_iModelId == MI_CAR_POLICE)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_POLICE.GetModelIndex(), rInput.m_vTargetPos);
		}
		else if(rInput.m_iModelId == MI_CAR_POLICE_2)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_POLICE_2.GetModelIndex(), rInput.m_vTargetPos);
		}
		else if(rInput.m_iModelId == MI_CAR_SHERIFF)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_SHERIFF.GetModelIndex(), rInput.m_vTargetPos);
		}
		else if(rInput.m_iModelId == MI_CAR_NOOSE)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_NOOSE.GetModelIndex(), rInput.m_vTargetPos);
		}
		else if(rInput.m_iModelId == MI_CAR_FBI)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_CAR_FBI.GetModelIndex(), rInput.m_vTargetPos);
        }
		else if (rInput.m_iModelId == MI_CAR_GARBAGE_COLLECTION)
		{
			pNewCar->SetVelocity(tempMat.b * 2.0f);
		}

		return pNewCar;
	}

	vehPopDebugf2("Failed to create emergency vehicle, found no place to spawn it");
	return NULL;
}

CVehicle* CVehiclePopulation::GenerateOneEmergencyServicesCarWithRoadNodes(const GenerateOneEmergencyServicesCarWithRoadNodesInput& rInput)
{
	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(rInput.m_Node);
	if(!pNode)
	{
		return NULL;
	}

	//Ensure the previous node is valid.
	const CPathNode* pPreviousNode = ThePaths.FindNodePointerSafe(rInput.m_PreviousNode);
	if(!pPreviousNode)
	{
		return NULL;
	}
	
	//Ensure the vehicle model info is valid.
	CVehicleModelInfo* pVehicleModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(rInput.m_iModelId);
	if(!pVehicleModelInfo)
	{
		return NULL;
	}

	//Find the link from the previous node to the node.
	s16 iLink = -1;
	if(!ThePaths.FindNodesLinkIndexBetween2Nodes(pPreviousNode->m_address, pNode->m_address, iLink))
	{
		return NULL;
	}

	//Get the link.
	const CPathNodeLink& rLink = ThePaths.GetNodesLink(pPreviousNode, iLink);

	//Generate the coordinates of the node.
	Vector3 vNodePosition;
	pNode->GetCoors(vNodePosition);

	//Generate the coordinates of the previous node.
	Vector3 vPreviousNodePosition;
	pPreviousNode->GetCoors(vPreviousNodePosition);

	//Generate the new forward vector.
	Vector3 vNewForward;
	if(rInput.m_bFacePreviousNode)
	{
		vNewForward = vPreviousNodePosition - vNodePosition;
	}
	else
	{
		vNewForward = vNodePosition - vPreviousNodePosition;
	}
	 
	 //Normalize the forward vector.
	if(!vNewForward.NormalizeSafeRet())
	{
		return NULL;
	}

	//Generate the new side vector.
	Vector3 vNewSide;
	vNewSide.Cross(vNewForward, ZAXIS);
	if(!vNewSide.NormalizeSafeRet())
	{
		return NULL;
	}

	//Generate the new up vector.
	Vector3 vNewUp;
	vNewUp.Cross(vNewSide, vNewForward);

	//Create the matrix for the vehicle.
	Matrix34 mVehicle(Matrix34::IdentityType);
	mVehicle.a = vNewSide;
	mVehicle.b = vNewForward;
	mVehicle.c = vNewUp;
	mVehicle.d = vNodePosition;

	//Check if the vehicle should be pulled over.
	if(rInput.m_bPulledOver)
	{
		//Find the road boundaries.
		bool bAllLanesThoughCentre = (rLink.m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
		float fRoadWidthOnLeft;
		float fRoadWidthOnRight;
		ThePaths.FindRoadBoundaries(rLink.m_1.m_LanesToOtherNode, rLink.m_1.m_LanesFromOtherNode, static_cast<float>(rLink.m_1.m_Width), rLink.m_1.m_NarrowRoad, bAllLanesThoughCentre, fRoadWidthOnLeft, fRoadWidthOnRight);

		//Grab the max bounding box for the vehicle.
		const Vector3& vVehicleBoundingBoxMax = pVehicleModelInfo->GetBoundingBoxMax();

		//Calculate the offset.
		float fOffset = fRoadWidthOnRight - vVehicleBoundingBoxMax.x;

		//Calculate the new position.
		mVehicle.d.x += vNewForward.y * fOffset;
		mVehicle.d.y -= vNewForward.x * fOffset;
	}
	else
	{
		//Move the car over a bit to the right side of the road.
		float fInitialLaneCenterOffset = rLink.InitialLaneCenterOffset();
		mVehicle.d.x += vNewForward.y * fInitialLaneCenterOffset;
		mVehicle.d.y -= vNewForward.x * fInitialLaneCenterOffset;
	}

	CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrixInput input(mVehicle, rInput.m_iModelId);
	input.m_bUseBoundingBoxesForCollision = rInput.m_bUseBoundingBoxesForCollision;
	input.m_bTryToClearAreaIfBlocked = rInput.m_bTryToClearAreaIfBlocked;
	input.m_bSwitchEngineOn = rInput.m_bSwitchEngineOn;
	input.m_bKickStartVelocity = rInput.m_bKickStartVelocity;
	input.m_bPlaceOnRoadProperlyCausesFailure = rInput.m_bPlaceOnRoadProperlyCausesFailure;
	return CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrix(input);
}

CVehicle* CVehiclePopulation::GenerateOneEmergencyServicesVehicleWithMatrix(const GenerateOneEmergencyServicesVehicleWithMatrixInput& rInput)
{
#if __BANK
	utimer_t startTime = sysTimer::GetTicks();
#endif // __BANK

	//Grab the model id.
	fwModelId modelId = rInput.m_ModelId;
	
	//Ensure the base model info is valid.
	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(!aiVerifyf(pBaseModelInfo, "The base model info is invalid: %d.", modelId.GetModelIndex()))
	{
		return NULL;
	}
	
	//Ensure the base model info is for a vehicle.
	if(!aiVerifyf(pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE, "The model type is invalid: %d.", pBaseModelInfo->GetModelType()))
	{
		return NULL;
	}
	
	//Grab the vehicle model info.
	CVehicleModelInfo* pVehicleModelInfo = static_cast<CVehicleModelInfo *>(pBaseModelInfo);
	
	//Ensure the assets have loaded.
	if(!aiVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "The model has not loaded: %d.", modelId.GetModelIndex()))
	{
		return NULL;
	}
	
	//Copy the transform so it can be modified.
	Matrix34 mTransform(rInput.m_mTransform);

	CInteriorInst*	pDestInteriorInst = NULL;		
	s32				destRoomIdx = -1;
	bool setWaitForAllCollisionsBeforeProbe = false;
	
	//Check if the vehicle is an automobile.
	if(pVehicleModelInfo->GetIsAutomobile())
	{
		//Place the automobile on the road properly.
		if(!CAutomobile::PlaceOnRoadProperly(NULL, &mTransform, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, modelId.GetModelIndex(), NULL, false, NULL, NULL, 0))
		{
			//Check if not being able to place on the road properly should result in failure.
			if(rInput.m_bPlaceOnRoadProperlyCausesFailure)
			{
				return NULL;
			}
		}
	}
	//Check if the vehicle is a bike.
	else if(pVehicleModelInfo->GetIsBike())
	{
		//Place the bike on the road properly.
		if(!CBike::PlaceOnRoadProperly(NULL, &mTransform, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, modelId.GetModelIndex(), NULL, false, NULL, NULL, 0))
		{
			//Check if not being able to place on the road properly should result in failure.
			if(rInput.m_bPlaceOnRoadProperlyCausesFailure)
			{
				return NULL;
			}
		}
	}
	//The other vehicle types are on their own, when it comes to placement.
	else
	{
		//Nothing to do.
	}
	
	bool bForceFadeIn = false;
	
	// don't create vehicles in view of or close to any other network players
	if(NetworkInterface::IsGameInProgress())
	{
		bool bCanReject = CanRejectNetworkSpawn(mTransform.d, pBaseModelInfo->GetBoundingSphereRadius(), bForceFadeIn);
		if(bCanReject)
		{
			vehPopDebugf2("Skipping veh spawn emergency services vehicle, vehicle too close to net player");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateOneEmergencyServicesVehicleWithMatrix", "Failed:", "vehicle too close to net player");
			return NULL;
		}
		else if(bForceFadeIn)
		{
			vehPopDebugf2("Network Override -- GenerateOneEmergencyServicesVehicleWithMatrix allow vehicle to spawn");
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION","GenerateOneEmergencyServicesVehicleWithMatrix", "Override:", "Network override, allow vehicle to spawn");
		}
	}
	
	//Check if the position is clear to spawn the emergency services car.
	Vector3 vPosition = mTransform.d;
	float fRadius = pBaseModelInfo->GetBoundingSphereRadius();
	bool bUseBoundBoxes = rInput.m_bUseBoundingBoxesForCollision;
	const CEntity* pException = rInput.m_pExceptionForCollision;
	if(!IsPositionClearToSpawnEmergencyServicesVehicle(mTransform.d, fRadius, bUseBoundBoxes, pException))
	{
		//Check if we should try to clear the area.
		if(rInput.m_bTryToClearAreaIfBlocked)
		{
			//Try to clear the area.
			CGameWorld::ClearPedsFromArea(vPosition, fRadius);
			CGameWorld::ClearCarsFromArea(vPosition, fRadius, false, false, false, false, false, false);
			CGameWorld::ClearObjectsFromArea(vPosition, fRadius);

			//Check if the area was cleared.
			if(!IsPositionClearToSpawnEmergencyServicesVehicle(mTransform.d, fRadius, bUseBoundBoxes, pException))
			{
				return NULL;
			}
		}
		else
		{
			return NULL;
		}
	}
	
	//Create the vehicle.
	CVehicle* pVehicle = CVehicleFactory::GetFactory()->Create(modelId, rInput.m_nEntityOwnedBy, rInput.m_nPopType, &mTransform);
	if(!pVehicle)
	{
		return NULL;
	}
	BANK_ONLY(pVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_EMERGENCY);
	
	//Check if the vehicle should be given a default task.
	if(rInput.m_bGiveDefaultTask)
	{
		//Give the vehicle a default task.
		pVehicle->GiveDefaultTask();
	}
	
	//Make the vehicle a dummy based on distance.
	bool bSuperDummyConversionFailed = false;
	MakeVehicleIntoDummyBasedOnDistance(*pVehicle, RCC_VEC3V(vPosition), bSuperDummyConversionFailed);

	if(pDestInteriorInst && destRoomIdx != INTLOC_INVALID_INDEX)
	{
		fwInteriorLocation interiorLocation = CInteriorInst::CreateLocation(pDestInteriorInst, destRoomIdx);

		if(setWaitForAllCollisionsBeforeProbe)
		{
			pVehicle->GetPortalTracker()->SetWaitForAllCollisionsBeforeProbe(true);
		}

		CGameWorld::Add(pVehicle, interiorLocation);
	}
	else
	{
		extern bool g_debugAmbulancePortalTracker;
		g_debugAmbulancePortalTracker = true;
		CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);

		CPortalTracker *pPortalTracker = pVehicle->GetPortalTracker();
		CPortalTracker::eProbeType probeType = GetVehicleProbeType(pVehicle);

		pPortalTracker->SetProbeType(probeType);
		pPortalTracker->RequestRescanNextUpdate();

		const Vector3 vNewCarPosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		pVehicle->GetPortalTracker()->Update(vNewCarPosition);
		g_debugAmbulancePortalTracker = false;
	}

	//Check if we should switch the engine on.
	if(rInput.m_bSwitchEngineOn)
	{
		//Switch the engine on.
		pVehicle->SwitchEngineOn(true);
		pVehicle->SetRandomEngineTemperature();
	}
	
	//Check if we should kick start the velocity.
	if(rInput.m_bKickStartVelocity)
	{
		//Kick start the velocity.
		if (!pVehicle->IsSuperDummy() || !CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode)
		{
			pVehicle->SetVelocity(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()) * 10.0f);
		}
		else
		{
			pVehicle->SetSuperDummyVelocity(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()) * 10.0f);
		}
	}

	// Set flag for whether this vehicle counts towards ambient population
	pVehicle->m_nVehicleFlags.bCountsTowardsAmbientPopulation = CountsTowardsAmbientPopulation(pVehicle);

	if(bForceFadeIn)
	{
		pVehicle->GetLodData().SetResetDisabled(false);
		pVehicle->GetLodData().SetResetAlpha(true);
	}

#if __DEV
	VecMapDisplayVehiclePopulationEvent(vPosition, VehPopCreateEmegencyVeh);
#endif

	vehPopDebugf2("Successfully created emergency vehicle 0x%8p, type: %s", pVehicle, pVehicle->GetModelName());

	NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "CreateCarForScript", "Vehicle Created:", "%s", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "No Network Object");

#if __BANK
	if(pVehicle)
	{
		pVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - startTime));
	}
#endif // __BANK

	return pVehicle;
}

bool CVehiclePopulation::IsPositionClearToSpawnEmergencyServicesVehicle(const Vector3& vPosition, float fRadius, bool bUseBoundBoxes, const CEntity* pException)
{
	//Check if any objects are kinda colliding.
	bool bDist2D = true;
	s32 iNum = 0;
	static const int sMaxNum = 2;
	CEntity* pResults[sMaxNum];
	bool bCheckBuildings = false;
	bool bCheckVehicles = true;
	bool bCheckPeds = true;
	bool bCheckObjects = true;
	bool bCheckDummies = false;
	CGameWorld::FindObjectsKindaColliding(vPosition, fRadius, bUseBoundBoxes, bDist2D, &iNum, sMaxNum, pResults, bCheckBuildings, bCheckVehicles, bCheckPeds, bCheckObjects, bCheckDummies);
	if(iNum <= 0)
	{
		return true;
	}
	
	//Check if the only collision was the exception.
	if((iNum == 1) && pException && (pException == pResults[0]))
	{
		return true;
	}
	
	return false;
}

CHeli* CVehiclePopulation::GenerateOneHeli(u32 modelIndex, u32 driverPedIndex, bool bFadeIn)
{
	CHeli* pNewHeli = CHeli::GenerateHeli(FindPlayerPed(), modelIndex, driverPedIndex, bFadeIn);
	if(pNewHeli)
	{
		if (pNewHeli->GetModelIndex() == MI_HELI_POLICE_1)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_HELI_POLICE_1.GetModelIndex(), CGameWorld::FindLocalPlayerCoors());
		}
		else if (pNewHeli->GetModelIndex() == MI_HELI_POLICE_2)
		{
			CNetworkTelemetry::EmergencySvcsCalled(MI_HELI_POLICE_2.GetModelIndex(), CGameWorld::FindLocalPlayerCoors());
		}
	}

	return pNewHeli;
}

// NAME : IsModelReservedForSpecificStreet
// PURPOSE : Determines whether for the specified spawn position, this particular vehicle model will only be
//			 spawned on a specific names street.  If this is the case then it will not be eligble as an ambient
//			 vehicle as chosen from CLoadedModelGroup::CopyEligibleCars().  (it will be selected separately)
bool CVehiclePopulation::IsModelReservedForSpecificStreet(const u32 iModelIdx, const Vector3 & vPos)
{
	for(int s=0; s<ms_iNumStreetModelAssociations; s++)
	{
		for(int m=0; m<m_StreetModelAssociations[s].m_iNumModels; m++)
		{
			if((u32)m_StreetModelAssociations[s].m_VehicleModelId[m].GetModelIndex()==iModelIdx &&
				m_StreetModelAssociations[s].m_vMin[m].IsLessThanAll(vPos) &&
				vPos.IsLessThanAll(m_StreetModelAssociations[s].m_vMax[m]))
				return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ChooseModel
// PURPOSE :  Decides from the Zone Info which type of car to create then 
//	          calls another function to decide which model of this type 
//            should be created
// INPUT :    Current Zone and Position of the player
// RETURNS :  Model and Type of new car
/////////////////////////////////////////////////////////////////////////////////
u32 CVehiclePopulation::ChooseModel(bool* pbPolice, const Vector3& vPos, bool BANK_ONLY(bNoCops), bool bSmallWorkers, bool bOnHighway, const u32 iStreetNameHash, bool offroad, const bool bNoBigVehicles, const int /*iNumLanesOurWay*/)
{
#if __BANK
	bNoCops = ms_noPoliceSpawn || bNoCops;
#endif

	CPopCycle::VerifyHasValidCurrentPopAllocation();

	if(!ms_spawnCareAboutAppropriateVehicleModels)
	{
		CLoadedModelGroup & loadedCars = gPopStreaming.GetAppropriateLoadedCars();
		u32 iCar = loadedCars.PickRandomCarModel(false).Get();
		return iCar;
	}

	// If this street is set up to spawn only particular vehicle types, then return the appropriate model index
	// if the spawn location is inside the bounding area.
	// It is up to the popzones to ensure that the models are specified for streaming in these locations.
	if(iStreetNameHash)
	{
		for(int s=0; s<ms_iNumStreetModelAssociations; s++)
		{
			if(m_StreetModelAssociations[s].m_iStreetNameHash == iStreetNameHash)
			{
				if(AssertVerify(m_StreetModelAssociations[s].m_iNumModels > 0))
				{
					// Randomly pick a vehicle model associated with this street name
					const int i = fwRandom::GetRandomNumberInRange(0, m_StreetModelAssociations[s].m_iNumModels);
					if(AssertVerify(m_StreetModelAssociations[s].m_VehicleModelId[i].GetModelIndex() != fwModelId::MI_INVALID))
					{
						if(m_StreetModelAssociations[s].m_vMin[i].IsLessOrEqualThanAll(vPos) &&
							vPos.IsLessThanAll(m_StreetModelAssociations[s].m_vMax[i]))
						{
							if(CModelInfo::HaveAssetsLoaded(m_StreetModelAssociations[s].m_VehicleModelId[i]))
							{
								Assertf(gPopStreaming.IsVehicleInAllowedNetworkArray(m_StreetModelAssociations[s].m_VehicleModelId[i].GetModelIndex()), "Spawning bad vehicle type in MP game (chosen based on street)");
								return m_StreetModelAssociations[s].m_VehicleModelId[i].GetModelIndex();
							}
							else
							{
								return fwModelId::MI_INVALID;
							}
						}
					}
				}
			}
		}
	}

	s32 iNumDesired = (s32)GetDesiredNumberAmbientVehicles();
	s32 Total = iNumDesired - ms_numRandomCars;
	
	if(Total <= 0)
	{
		vehPopDebugf3("Population is full, can't create more vehicles. ChooseModel will return MI_INVALID. Desired Ambient: %d, Random Cars %d.", iNumDesired, ms_numRandomCars);		
		return fwModelId::MI_INVALID;
	}

	Assert(Total >= 0 && Total <= iNumDesired);

#if __BANK
	if (gPopStreaming.GetVehicleOverrideIndex() != fwModelId::MI_INVALID)
		return gPopStreaming.GetVehicleOverrideIndex();
#endif
			
	{	// We want a random car
		//  Use the percentages to work out which car group we want.
		*pbPolice = false;
		
		{
            // don't remove cars that don't belong to the current population zone in MP games,
            // as we don't use zones in MP currently
			bool removeNonNative = false;
			if(!NetworkInterface::IsGameInProgress() && (ms_lastPopCycleChangeTime + ms_popCycleChangeBufferTime) < fwTimer::GetTimeInMilliseconds())
				removeNonNative = true;

			// Remove cop cars if flagged to not spawn any
			// temp fix to stop cop cars being added to the popgroups
			bool removeCopCars = false;
			if( BANK_ONLY( bNoCops || ) !CPedPopulation::GetAllowCreateRandomCops() || bSmallWorkers)
				removeCopCars = true;

			bool removeOnlyOnHighway = !bOnHighway;
			bool removeNonOffroad = offroad;

			bool removeBigVehicles = bNoBigVehicles;

			CLoadedModelGroup SelectableCars2;
			SelectableCars2.CopyEligibleCars(vPos, &gPopStreaming.GetAppropriateLoadedCars(), bSmallWorkers, removeNonNative, removeCopCars, removeOnlyOnHighway, removeNonOffroad, removeBigVehicles);

			SelectableCars2.RemoveModelsThatHaveHitMaximumNumber();


			u32 randomModel = fwModelId::MI_INVALID;
			CPopCycle::FindNewCarModelIndex(&randomModel, SelectableCars2, &vPos, false);

			if (randomModel != fwModelId::MI_INVALID)
			{
				*pbPolice = CVehicle::IsLawEnforcementVehicleModelId(fwModelId(strLocalIndex(randomModel)));
#if __BANK
				if(CPopCycle::sm_bEnableVehPopDebugOutput)
				{
					char buf[512];
					char buf2[64];
					formatf(buf, "Selected %s from: ", CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(randomModel)))->GetModelName());

					for (s32 i = 0; i < MAX_NUM_IN_LOADED_MODEL_GROUP; ++i)
					{
						strLocalIndex selected = strLocalIndex(SelectableCars2.GetMember(i));
						fwModelId selectedModelId(selected);
						if (!selectedModelId.IsValid())
							continue;

						CVehicleModelInfo* mi = (CVehicleModelInfo*)(CModelInfo::GetBaseModelInfo(selectedModelId));
						formatf(buf2, "%s%s%s(ref(parked):%d(%d),freq:%d), ", 
							CModelInfo::GetAssetRequiredFlag(selectedModelId) ? "" : "*",
							CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(selected.Get()) ? "^" : "",
							mi->GetModelName(), mi->GetNumVehicleModelRefs(), mi->GetNumVehicleModelRefsParked(), mi->GetVehicleFreq());
						safecat(buf, buf2, sizeof(buf));
					}
					vehPopDebugf2("%s", buf);
				}
#endif // __BANK
			}

			Assertf(gPopStreaming.IsVehicleInAllowedNetworkArray(randomModel), "Spawning bad vehicle type in MP game (random model): %s", randomModel != fwModelId::MI_INVALID ? CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(randomModel)))->GetModelName() : "MI_INVALID");
            Assertf(!CNetwork::IsGameInProgress() || randomModel != MI_CAR_TAXI_CURRENT_1.GetModelIndex(), "Trying to spawn an ambient taxi in MP!");
			return randomModel;
		}
	}
}

void CVehiclePopulation::MakeVehicleIntoDummyBasedOnDistance(CVehicle& rVehicle, Vec3V_In vPosition, bool& bSuperDummyConversionFailedOut)
{
#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker makeDummy("MakeVehicleIntoDummyBasedOnDistance", 21);
#endif
	//Make sure the vehicle has a follow route.
	//rVehicle.GetIntelligence()->GetTaskManager()->Process(fwTimer::GetTimeStep());
	//rVehicle.CacheAiData();	
	//PF_PUSH_TIMEBAR("Process Vehicle Intelligence");

	rVehicle.PostCreationAiUpdate();

	//PF_POP_TIMEBAR();
	//rVehicle.InvalidateCachedAiData();

	//rVehicle.GetIntelligence()->CacheFollowRoute();
	
	//Make the vehicle into a dummy, based on distance from the position.
	eVehicleDummyMode dummyModeBasedOnDistance = CVehicleAILodManager::CalcDesiredLodForPoint(vPosition);
	bSuperDummyConversionFailedOut = false;

	if (dummyModeBasedOnDistance==VDM_SUPERDUMMY)
	{
		if (!rVehicle.TryToMakeIntoDummy(dummyModeBasedOnDistance,true))
		{
			bSuperDummyConversionFailedOut = true;
		}
	}

	if(dummyModeBasedOnDistance==VDM_DUMMY || bSuperDummyConversionFailedOut)
	{
		rVehicle.TryToMakeIntoDummy(VDM_DUMMY,true);
	}

#if ENABLE_FRAG_OPTIMIZATION
	if(dummyModeBasedOnDistance != VDM_SUPERDUMMY || bSuperDummyConversionFailedOut)
	{
		rVehicle.GiveFragCacheEntry();
	}
#endif
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddDriverAndPassengersForVeh
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
u32 CVehiclePopulation::AddDriverAndPassengersForVeh(CVehicle *pNewVehicle, s32 MinPassengers, s32 MaxPassengers, bool bUseExistingNodes)
{
	Assert(pNewVehicle);
	Assert(!pNewVehicle->InheritsFromTrain()); // Should use AddDriverAndPassengersForTrain instead.

	if (!pNewVehicle->SetUpDriver(bUseExistingNodes)) // once the car has been added to the world we can add its driver
	{
		return 0;
	}

	Assert(MaxPassengers >= 0);

	// If the vehicle is a taxi or limo we don't create anyone on passenger seat.
	bool bTaxi = false;
	if ( CVehicle::IsTaxiModelId(pNewVehicle->GetModelId()))
	{
		bTaxi = true;
	}

	bool bBus = false;
	if ( CVehicle::IsBusModelId(pNewVehicle->GetModelId()))
	{
		bBus = true;
		MinPassengers = 1; //buses should have at least 1 passenger, where possible.
	}
						    
	//Now add a random number of passengers to the car.
	s32 carMaxPassengers = pNewVehicle->GetMaxPassengers();
	if (bTaxi)
	{
		if( !((CAutomobile *)pNewVehicle)->m_nAutomobileFlags.bTaxiLight )
		{
			carMaxPassengers--;
			MinPassengers = 1;
		}
		else
			carMaxPassengers = 0;
		Assert(carMaxPassengers >= 0);
	}

	//Get the maximum number of passengers that we can add to the vehicle.
	int nMaxPassengers=rage::Min(carMaxPassengers, MaxPassengers);
	nMaxPassengers = rage::Max(nMaxPassengers, MinPassengers);
						
	//Generate a number in the range 0<=n<=nMaxPassengers.
	int iRandom;
	int nPassengers=MinPassengers;
	mthRandom rnd(pNewVehicle->GetRandomSeed());
	for(iRandom=MinPassengers;iRandom<nMaxPassengers;iRandom++)
	{
	    //Add 0 or 1 randomly (weighted in favor of adding 0)
		static dev_float MAX_PROB_FOR_PASSENGER_BUS = 0.18f;	// More fullz for buses

		const float f=rnd.GetRanged(0.0f,bBus ? MAX_PROB_FOR_PASSENGER_BUS : 1.0f);
		nPassengers += (f<ms_passengerInASeatProbability) ? 1 : 0;
	}
	nPassengers = rage::Min(nPassengers, nMaxPassengers);
	Assert(nPassengers<=nMaxPassengers);
	Assert(nPassengers>=0);
								
	//Don't allow vans to have more than one passenger.  Basically, don't
	//put peds in the back of vans.
	if(pNewVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_VAN))
	{
		nPassengers=rage::Min(nPassengers,1);
	}

	u32 desiredModel = fwModelId::MI_INVALID;
	s32 overrideMaxPassengers = 0;
	if (pNewVehicle->GetDriver())
	{
		CPedModelInfo* pmi = pNewVehicle->GetDriver()->GetPedModelInfo();
		if (pmi && pmi->GetMaxPassengersInCar() > 0)
		{
			overrideMaxPassengers = (s32)pmi->GetMaxPassengersInCar();	
			if (nPassengers < overrideMaxPassengers)
				overrideMaxPassengers = rnd.GetRanged(nPassengers, overrideMaxPassengers + 1);
			else
				overrideMaxPassengers = rnd.GetRanged(0, overrideMaxPassengers + 1);
			overrideMaxPassengers = rage::Min(overrideMaxPassengers, nMaxPassengers);

			desiredModel = pNewVehicle->GetDriver()->GetModelIndex();
		}
	}
	nPassengers = rage::Max(nPassengers, overrideMaxPassengers);

	s32 iNumBusPassengers = 0;

	//Add the passengers to the car.
	int k;
	int iNumPassengersScheduled = 0;
	for(k=0;k<nPassengers;k++)
	{
		s32 iPassenger = (bTaxi)?k+1:k;

        // only add passengers if we have room
		if (!bBus || rnd.GetBool())
		{
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
			{	
				if (CPedPopulation::SchedulePedInVehicle(pNewVehicle, iPassenger+1, true	, desiredModel))
				{
					if(bBus)
					{
						++iNumBusPassengers;
					}

					++iNumPassengersScheduled;
				}
			}
		}
	}

	//add 1 for the driver
	return iNumPassengersScheduled + 1;
}

u32	CVehiclePopulation::AddDriverAndPassengersForTrain(CTrain* pTrain, s32 iMinPassengers, s32 iMaxPassengers, bool bUseExistingNodes)
{
	Assert(pTrain);
	int iNumPassengersScheduled = 0;

	if (pTrain == pTrain->GetEngine())
	{
		if (!pTrain->SetUpDriverOnEngine(bUseExistingNodes))
		{
			return 0;
		}
		++iNumPassengersScheduled;
	}
	

	Assert(iMaxPassengers >= 0);

	//Now add a random number of passengers to the car.
	s32 trainMaxPassengers = pTrain->GetEmptyPassengerSlots();

	//Get the maximum number of passengers that we can add to the vehicle.
	int nMaxPassengers=rage::Min(trainMaxPassengers, iMaxPassengers);

	//Generate a number in the range 0<=n<=nMaxPassengers.
	int iRandom;
	int nPassengers = iMinPassengers;
	mthRandom rnd(pTrain->GetRandomSeed());
	for(iRandom = iMinPassengers; iRandom < nMaxPassengers; iRandom++)
	{
		//Add 0 or 1 randomly (weighted in favor of adding 0)
		static dev_float MAX_PROB_FOR_PASSENGER_TRAIN = 0.18f;	// More fullz for trains

		const float f=rnd.GetRanged(0.0f, MAX_PROB_FOR_PASSENGER_TRAIN);
		nPassengers += (f < ms_passengerInASeatProbability) ? 1 : 0;
	}
	nPassengers = rage::Min(nPassengers, nMaxPassengers);
	Assert(nPassengers<=nMaxPassengers);
	Assert(nPassengers>=0);
	
	//Add the passengers to the car.
	CScenarioPoint* paPointsOnTrain[CTrain::sMaxPassengers];
	u32 uNumFound = SCENARIOPOINTMGR.FindUnusedPointsWithAttachedEntity(pTrain, paPointsOnTrain, NELEM(paPointsOnTrain));
	u32 uNumUsed  = 0;
	for(int k = 0; k < nPassengers && uNumUsed < uNumFound; k++)
	{
		{
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
			{	
				const float maxFrustumDistToCheck = 0.0f;
				const bool allowDeepInteriors = true;

				CScenarioPoint* pScenarioPoint = paPointsOnTrain[uNumUsed++];
				float fHeading = 0;
				Vector3 vScenarioPosition = VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition());

				int indexWithinTypeGroup;
				int spawnOptions = CPedPopulation::SF_ignoreSpawnTimes; // This allows us to still find the type of the point even if it wouldn't spawn due to time/probability.
				const int scenarioTypeReal = CPedPopulation::SelectPointsRealType(*pScenarioPoint, spawnOptions, indexWithinTypeGroup);
				if (scenarioTypeReal == -1)
				{
					continue;
				}

				CTask* pScenarioTask = CScenarioManager::SetupScenarioAndTask( scenarioTypeReal, vScenarioPosition, fHeading, *pScenarioPoint);

				const u32  iSpawnOptions = CPedPopulation::SF_forceSpawn | CPedPopulation::SF_ignoreScenarioHistory | CPedPopulation::SF_addingTrainPassengers;
				const u32  iPedIndexWithinScenario = 0;
				const bool bFindZCoordForPed = false;
				const CScenarioManager::CollisionCheckParams* pCollisionCheckParams = NULL;
				const CScenarioManager::PedTypeForScenario* pPredeterminedPedType = NULL;
				const bool bAllowFallbackPed = true;

				CPed* pPed = CScenarioManager::CreateScenarioPed(scenarioTypeReal, *pScenarioPoint, pScenarioTask, vScenarioPosition, maxFrustumDistToCheck, allowDeepInteriors, fHeading, iSpawnOptions, iPedIndexWithinScenario, bFindZCoordForPed, pCollisionCheckParams, pPredeterminedPedType, bAllowFallbackPed );

				if (pPed)
				{
					++iNumPassengersScheduled;
				}
			}
		}
	}

	return iNumPassengersScheduled;
}


void CVehiclePopulation::AddStreetModelAssociation(const char * pStreetName, const char * pVehicleName, const Vector3 & vMin, const Vector3 & vMax)
{
	if(AssertVerify(pStreetName && pVehicleName))
	{
		const u32 iStreetHash = atStringHash(pStreetName);
		// See if we already have this street registered
		int iSlot;
		for(iSlot=0; iSlot<ms_iNumStreetModelAssociations; iSlot++)
		{
			if(m_StreetModelAssociations[iSlot].m_iStreetNameHash == iStreetHash)
				break;
		}

		if(AssertVerify(iSlot < ms_iMaxStreetModelAssociations))
		{
			// Make sure we have space for a model
			CStreetModelAssociation & assoc = m_StreetModelAssociations[iSlot];
			if(AssertVerify(assoc.m_iNumModels < CStreetModelAssociation::ms_iMaxModels))
			{
				assoc.m_iStreetNameHash = iStreetHash;
				assoc.m_vMin[assoc.m_iNumModels] = vMin;
				assoc.m_vMax[assoc.m_iNumModels] = vMax;

				// Find the modelId
				CBaseModelInfo * pModelInfo = CModelInfo::GetBaseModelInfoFromName(pVehicleName, &assoc.m_VehicleModelId[assoc.m_iNumModels]);
				if(pModelInfo && AssertVerify(pModelInfo->GetModelType() == MI_TYPE_VEHICLE))
				{
					assoc.m_iNumModels++;
				}
			}
			if(iSlot == ms_iNumStreetModelAssociations)
				ms_iNumStreetModelAssociations++;
		}
	}
}

float CVehiclePopulation::GetRemovalTimeScale()
{
	// Get a value for our speed scale between zero and one.
	// This is used to change the population generation and removal
	// so it is best suited for how we are moving through the world.
	const float maxSpeedPortion = rage::Clamp((ms_lastPopCenter.m_centreVelocity.XYMag() / ms_maxSpeedExpectedMax), 0.0f, 1.0f);

	// Scale ranges according to how fast we are moving as we don't want
	// too many vehs trailing behind us.
	// 1.0f at normal speeds and up to 10.0f at high speeds.
	const float maxSpeedCubed = maxSpeedPortion * maxSpeedPortion * maxSpeedPortion;
	const float speedBasedRemovalRateScale = rage::Max(1.0f, (maxSpeedCubed * ms_maxSpeedBasedRemovalRateScale));


	static float densityBasedRemovalRateScale = 1.0f;

	const s32 numRandomVehsThatCanBeCreated = FindNumRandomVehsThatCanBeCreated();

	if(numRandomVehsThatCanBeCreated >= ms_densityBasedRemovalTargetHeadroom)
	{
		densityBasedRemovalRateScale = 1.0f;
	}
	else
	{
		float newRemovalRateScale = ms_densityBasedRemovalRateScale * Max(0.5f, (float)(ms_densityBasedRemovalTargetHeadroom - numRandomVehsThatCanBeCreated) / (float)ms_densityBasedRemovalTargetHeadroom);

		if(newRemovalRateScale > densityBasedRemovalRateScale)
		{
			densityBasedRemovalRateScale = newRemovalRateScale;
		}
	}

	float maxRemovalRateScale = rage::Max(speedBasedRemovalRateScale, densityBasedRemovalRateScale);
	
	return 1.0f / rage::Max(1.0f, maxRemovalRateScale / 4.0f);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ForcePopulationInit
// PURPOSE :	Give game some time to fill in the world (without regard for
//				visibility or distance from the player) before processing the
//				population normally.
// PARAMETERS :	None.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::ForcePopulationInit()
{
	// This is needed if we have streamed in a scenario region after the last
	// call to UpdateNewlyLoadedScenarioRegions(), which normally happens before
	// ped and vehicle population updates. Should be very cheap if nothing new has
	// streamed in, and will prevent some assert.
	CScenarioChainingGraph::UpdateNewlyLoadedScenarioRegions();

	ms_countDownToCarsAtStart = 10;
	CTheCarGenerators::InstantFill();
	InstantFillPopulation();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HasForcePopulationInitFinished
// PURPOSE :	Checks if force population init has finished its countdown
// PARAMETERS :	None.
// RETURNS :	True when counted has reached 0
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::HasForcePopulationInitFinished()
{
	return ms_countDownToCarsAtStart <= 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveDistantVehs
// PURPOSE :  Goes through the cars on the map and removes the ones that are
//			  far away from the player and not mission specific.
// PARAMETERS :	The centre point about which to base our distance.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////

void CVehiclePopulation::RemoveDistantVehs(const Vector3& popCtrlCentre, const float popCtrlHeading, const float rangeScale, const float removalTimeScale)
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;

	const float	st0	= rage::Sinf(popCtrlHeading);
	const float	ct0	= rage::Cosf(popCtrlHeading);
	Vector4 vViewPlane(-st0, ct0, 0.0f, 0.0f);
	vViewPlane.w = - vViewPlane.Dot3(popCtrlCentre);

	CVehicle *pPlayerVehicle = FindPlayerVehicle();
	bool bAircraftPopCentre = pPlayerVehicle && pPlayerVehicle->GetIsAircraft();

	static u32 i = 0;
	u32 poolSize = (u32) pool->GetSize();

	if(i >= poolSize)
	{
		i = 0;
	}

	u32 initialIndex = i;

#define NUM_BATCHES_RDC (6)
	u32 vehiclesToCheck = (u32)ceil((float)ms_iAmbientVehiclesUpperLimit / NUM_BATCHES_RDC);

	while(vehiclesToCheck > 0 && ms_iNumVehiclesDestroyedThisCycle == 0)
	{
		pVehicle = pool->GetSlot(i);

		i = (i + 1) % poolSize;

		// Skip clustered vehicles. The cluster is in charge of removing them. [5/3/2013 mdawe]
		if(pVehicle && !pVehicle->m_nVehicleFlags.bIsInCluster)
		{
			if(!pVehicle->DelayedRemovalTimeHasPassed(removalTimeScale) BANK_ONLY(&& !ms_ignoreOnScreenRemovalDelay))
			{
				vehPopDebugf3("0x%8p - Skip vehicle removal, delayed removal time has not passed yet. scale: %f", pVehicle, removalTimeScale);
				BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Delayed Removal Timer"));
			}
			else
			{
				--vehiclesToCheck;

				DEV_ONLY(Vector3 v = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
				EVehLastRemovalReason removalReason = Removal_None;

				s32 newDelayedRemovalTime = 0;

	 			if(ShouldRemoveVehicle(pVehicle, popCtrlCentre, vViewPlane, rangeScale, removalTimeScale, bAircraftPopCentre, removalReason, newDelayedRemovalTime))
				{
					BANK_ONLY(ms_lastRemovalReason = removalReason);
					if(CanVehicleBeReused(pVehicle))
					{					
						AddVehicleToReusePool(pVehicle);
					}
					else
					{
						RemoveVeh(pVehicle, true, removalReason);
					}

#if __DEV
					VecMapDisplayVehiclePopulationEvent(v, VehPopDestroy);
#endif // _DEV
				}
				else
				{
					if(newDelayedRemovalTime <= 0)
					{
						pVehicle->DelayedRemovalTimeReset();
					}
					else
					{
						pVehicle->SetDelayedRemovalTime(newDelayedRemovalTime);
					}
				}
			}
		}

		if(i == initialIndex)
		{
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShouldRemoveVehicle
// PURPOSE :  Works out whether this car can be removed. Does not remove it, but returns true and the reason why if it is to be removed.
// PARAMETERS :	pVehicle- the vehicle in question
// RETURNS :	Whether or not the vehicle should be removed
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::ShouldRemoveVehicleInternal(CVehicle *pVehicle, const Vector3& popCtrlCentre, const Vector4 & vViewPlane, float rangeScale, const float removalTimeScale, const bool bAircraftPopCentre, EVehLastRemovalReason& removalReason_Out, s32& newDelayedRemovalTime_Out)
{
	/////////////////////////////////////////////////////////////////////////////
	// Special cases

	// Trains may only be removed via CTrain::DoTrainGenerationAndRemoval()
	if(pVehicle->InheritsFromTrain())
	{
		BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Train"));
		return false;
	}

	// Vehicles in the reuse pool are removed via UpdateVehicleReusePool()
	if(pVehicle->GetIsInReusePool())
	{
		BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("In reuse pool"));
		return false;
	}

	if(pVehicle->m_nVehicleFlags.bCannotRemoveFromWorld)
	{
		BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("CannotRemoveFromWorld"));
		return false;
	}

	if (pVehicle->InheritsFromBoat() && pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Boat visible on screen"));
		return false;
	}

    if(pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
    {
        BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Flagged for removal"));
        return false;
    }

	// don't delete vehicles that are following another as part of a convoy
	if(Unlikely(pVehicle->m_nVehicleFlags.bPartOfConvoy))
	{
		// Only call GetActiveTask() when we really need it (i.e., once we know it is part of a convoy), 
		// since it is actually quite expensive.
		const CTaskVehicleMissionBase* activeTask = pVehicle->GetIntelligence()->GetActiveTask();
		if(activeTask && activeTask->GetTargetEntity())
		{
			vehPopDebugf2("0x%8p - Skip vehicle removal, is part of convoy", pVehicle);
			BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("In convoy"));
			return false;
		}
	}

	/////////////////////////////////////////////////////////////////////////////


	Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

	Vector3 vVehicleToPopCtrlCenter = vVehiclePosition - popCtrlCentre;

	//Use some of the Z distance after a certain point, to account for very low or very high vehicles -- unless we're in an aircraft.
	if(!bAircraftPopCentre)
	{
		vVehicleToPopCtrlCenter.z = Max(0.0f, Abs(vVehicleToPopCtrlCenter.z) - 50.0f);
	}
	else
	{
		vVehicleToPopCtrlCenter.z = 0.0f;
	}

	float fDistanceSquared = vVehicleToPopCtrlCenter.Mag2();

	float fOuterBandRadiusMax = ms_popGenKeyholeShape.GetOuterBandRadiusMax();
	float fInnerBandRadiusMax = Max(ms_popGenKeyholeShape.GetInnerBandRadiusMax(), GetCullRangeOffScreen(rangeScale));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// If we're zoomed in (sniper scope, telescope) then we always keep vehicles which are in the sidewall/far bands

	if( ms_bZoomed )
	{
		s32 iCategory = ms_popGenKeyholeShape.CategorisePoint(Vector2(vVehiclePosition, Vector2::kXY));
		if( iCategory==CPopGenShape::GC_KeyHoleSideWall_on || iCategory==CPopGenShape::GC_KeyHoleOuterBand_on )
			return false;
	}
	/////////////////////////////////////////////////////////////////////////////
	// Extended removal range compatibility
	bool bSkipShapeTest = false;

	if(pVehicle->m_ExtendedRemovalRange != 0 && pVehicle->m_ExtendedRemovalRange > ms_cullRange)
	{
		float extendedRemovalRangeScale = rage::Max((float)(pVehicle->m_ExtendedRemovalRange), ms_cullRange) / ms_cullRange;
		rangeScale *= extendedRemovalRangeScale;

		float extendedCullRange = GetCullRange(rangeScale);

		if(extendedCullRange > fOuterBandRadiusMax)
		{
			fInnerBandRadiusMax = extendedCullRange * (fInnerBandRadiusMax / fOuterBandRadiusMax);
			fOuterBandRadiusMax = extendedCullRange;
			bSkipShapeTest = true;
		}
	}
	/////////////////////////////////////////////////////////////////////////////

	bool bVisible = pVehicle->GetIsVisibleInSomeViewportThisFrame();

	if(bVisible)
	{
		float fOnScreenCullRange = GetCullRangeOnScreen(rangeScale);

		if(fDistanceSquared > fOnScreenCullRange * fOnScreenCullRange)
		{
			removalReason_Out = Removal_OutOfRange_OnScreen;
			return true;
		}
		else
		{
			vehPopDebugf2("0x%8p - Skip vehicle removal, in range and visible", pVehicle);
			return false;
		}
	}
	else if(CanTreatAsVisibleForRemoval(pVehicle))
	{
		//! use same cull range as visible vehicles.
		float fCullRange = GetCullRangeOnScreen(rangeScale);

		if(fDistanceSquared < (fCullRange * fCullRange) )
		{
			vehPopDebugf2("0x%8p - Skip vehicle removal, treating as visible!", pVehicle);
			return false;
		}
	}
	else if(!pVehicle->m_nVehicleFlags.bIsInCluster && !pVehicle->OffScreenRemovalTimeHasPassed(newDelayedRemovalTime_Out, removalTimeScale))
	{
		// Has it REALLY been long enough? It may have been ~2.25s since we last checked, but, that doesn't mean we've been off screen since then.
		vehPopDebugf2("0x%8p - Not visible + not enough time since last visible", pVehicle);
		return false;
	}
	else if(pVehicle->GetIntelligence()->m_bRemoveAggressivelyForCarjackingMission)
	{
		if(ShouldAggressiveRemovalVehicleForCarjackingMission(pVehicle))
		{
			removalReason_Out = Removal_OutOfRange_Parked;
			return true;
		}
		else if(fDistanceSquared > fOuterBandRadiusMax * fOuterBandRadiusMax)
		{
			removalReason_Out = Removal_OutOfRange_OuterBand;
			return true;
		}
	}
	else if(pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
	{
		float fRandomParkedCullRange = GetCullRange(rangeScale);

		if(fDistanceSquared > fRandomParkedCullRange * fRandomParkedCullRange)
		{
			removalReason_Out = Removal_OutOfRange_Parked;
			return true;
		}
	}
	else if(fDistanceSquared > fOuterBandRadiusMax * fOuterBandRadiusMax)
	{
		removalReason_Out = Removal_OutOfRange_OuterBand;
		return true;
	}
	
	/////////////////////////////////////////////////////////////////////////////
	// Keep all vehicles within the max spawn distance during cut scenes and cinematic shooting moments, preventing deletion during cuts
	CutSceneManager *pCutSceneManager = CutSceneManager::GetInstancePtr();
	
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	
	const bool bCinematicShootingCamera = pCamera && pCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId());

	bool bCinematicShootingMomentRunning = false;

	if(bCinematicShootingCamera)
	{
		const camThirdPersonPedAssistedAimCamera* pCinematicShootingCamera = static_cast<const camThirdPersonPedAssistedAimCamera*>(pCamera);

		bCinematicShootingMomentRunning = pCinematicShootingCamera->GetIsCinematicMomentRunning();
	}

	if((pCutSceneManager && pCutSceneManager->IsCutscenePlayingBack()) || bCinematicShootingMomentRunning)
	{
		return false;
	}
	/////////////////////////////////////////////////////////////////////////////


	if(pVehicle->m_nVehicleFlags.bNeverUseSmallerRemovalRange)
	{
		fInnerBandRadiusMax = fOuterBandRadiusMax;
		bSkipShapeTest = true;
	}


	/////////////////////////////////////////////////////////////////////////////
	// Special case off-screen removal

	// If vehicle is offscreen, has no live peds, and was not parked intentionally to be left behind
	// NOTE: To see this in debug builds see CVehicleIntelligence::RenderDebug and CVehiclePopulation::ms_bDisplayAbandonedRemovalDebug
	const float fCullRangeOffScreenPedsRemoved = GetCullRangeOffScreenPedsRemoved(rangeScale);

	if(fDistanceSquared > fCullRangeOffScreenPedsRemoved * fCullRangeOffScreenPedsRemoved && !pVehicle->m_nVehicleFlags.bIsThisAParkedCar && !pVehicle->HasPedsInIt())
	{
		// Get a handle to the vehicle seat manager
		const CSeatManager* pSeatManager = pVehicle->GetSeatManager();

		// if there have ever been any peds in the seats
		if( pSeatManager && pSeatManager->HasEverHadPedInAnySeat() )
		{
			// check if any former occupant is still alive
			bool bPedFound = false;
			for(int iSeat = 0; iSeat < pSeatManager->GetMaxSeats() && !bPedFound; iSeat++)
			{
				CPed* pLastPedInSeat = pSeatManager->GetLastPedInSeat(iSeat);
				if( pLastPedInSeat != NULL && !pLastPedInSeat->IsDead() )
				{
					bPedFound = true;
				}
			}
			// no former occupant was found
			if( !bPedFound )
			{
				removalReason_Out = Removal_OutOfRange_NoPeds;
				return true;
			}
		}
	}

	#define MIN_DIST_FROM_PLAYER (20.0f)
	#define MIN_DIST_FROM_PLAYER_AIRCRAFT (250.0f)

	if( (fDistanceSquared > square(MIN_DIST_FROM_PLAYER)) REPLAY_ONLY(&& !CReplayMgr::IsRecording()))
	{
		if(pVehicle->m_nVehicleFlags.bTryToRemoveAggressively)
		{
			bool remove = true;

			// For airborne aircraft, the regular limit of 20 m for aggressive removal is probably
			// too small, would be quite noticeable at that distance, so do a second distance
			// check before letting it be removed.
			if(pVehicle->GetIsAircraft() && pVehicle->IsInAir())
			{
				remove = (fDistanceSquared > square(MIN_DIST_FROM_PLAYER_AIRCRAFT));
			}

			if(remove)
			{
				vehPopDebugf2("%p - Remove vehicle that had the TryToRemoveAggressively flag set.", (void*)pVehicle);
				removalReason_Out = Removal_RemoveAggressivelyFlag;
				return true;
			}
		}

		const s32 vehicleType = pVehicle->GetVehicleType();

		// Remove any random car that ended up at an unnaturally steep angle and isn't moving.
		// This may have been a parked car that got knocked over and froze, or a low LOD
		// collision in an intersection resulting in misaligned cars.
		if(vehicleType == VEHICLE_TYPE_CAR)
		{
			// Leave non-random cars alone, and allow active vehicles to settle before they can be removed.
			if(pVehicle->PopTypeIsRandomNonPermanent() && pVehicle->GetIsStatic() && pVehicle->GetStatus() != STATUS_WRECKED)
			{
				// Check the Z component of the local Z vector to find the cosine of the angle to
				// the world Z axis, and compare against the threshold.
				const float slopeCosThreshold = 0.866f;	// cosf(30.0f*DtoR)
				if(pVehicle->GetMatrixRef().GetCol2ConstRef().GetZf() < slopeCosThreshold)
				{
					// If the vehicle isn't superdummy, or if it's superdummy but not moving, remove it.
					if(!pVehicle->IsSuperDummy() || pVehicle->GetVelocity().Mag2() <= 0.0001f)
					{
						vehPopDebugf2("%p - Remove vehicle that was stationary at a too steep angle.", (void*)pVehicle);
						removalReason_Out = Removal_TooSteepAngle;
						return true;
					}
				}
			}
		}

		const bool cantTrafficJam = !ms_bUseTrafficJamManagement ||
			vehicleType == VEHICLE_TYPE_BOAT ||
			vehicleType == VEHICLE_TYPE_PLANE ||
			vehicleType == VEHICLE_TYPE_HELI ||
			vehicleType == VEHICLE_TYPE_BLIMP ||
			vehicleType == VEHICLE_TYPE_SUBMARINE ||
			pVehicle->m_nVehicleFlags.bCantTrafficJam ||
			((pVehicle->m_nVehicleFlags.bCreatedByCarGen || pVehicle->GetOwnedBy()==ENTITY_OWNEDBY_SCRIPT) && !pVehicle->m_nVehicleFlags.bHasBeenDriven);

		if(!cantTrafficJam)
		{
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_LastTimeNotStuck_Best, 10000, 0, 60000, 100);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_LastTimeNotStuck_Worst, 5000, 0, 60000, 100);

			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_NumCarsStuckBlocking_Best, 4, 1, 32, 1);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_NumCarsStuckBlocking_Worst, 2, 1, 32, 1);

			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_RemoveStuckPlayerVehicleTime_Best, 60000, 0, 1200000, 1);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_RemoveStuckPlayerVehicleTime_Worst, 20000, 0, 1200000, 1);

			TUNE_GROUP_FLOAT(VEHICLE_POPULATION, TrafficRemoval_RemoveStuckPlayerVehicleDist_Best, 100.0f, 0.0f, 1000.0f, 0.1f);
			TUNE_GROUP_FLOAT(VEHICLE_POPULATION, TrafficRemoval_RemoveStuckPlayerVehicleDist_Worst, 50.0f, 0.0f, 1000.0f, 0.1f);

			//don't include cars on highways as they won't be contributing to traffic issues
			s32 iNumAmbientVehicles = CVehiclePopulation::GetTotalAmbientMovingVehsOnMap() - CVehiclePopulation::ms_numAmbientCarsOnHighway;
			float fProportionAmbientVehStopped = 1.0f;
			if(iNumAmbientVehicles > ms_iMinAmbientVehiclesForThrottle)
			{
				fProportionAmbientVehStopped = Clamp(CVehiclePopulation::ms_numStationaryAmbientCars / (float)iNumAmbientVehicles, 0.0f, 1.0f);
			}

			u32 nTimeSinceLastStuckLimit = Lerp(fProportionAmbientVehStopped, TrafficRemoval_LastTimeNotStuck_Best, TrafficRemoval_LastTimeNotStuck_Worst);
			u32 nNumCarsStuckBlockingLimit = Lerp(fProportionAmbientVehStopped, TrafficRemoval_NumCarsStuckBlocking_Best, TrafficRemoval_NumCarsStuckBlocking_Worst);

			if (fwTimer::GetTimeInMilliseconds() - pVehicle->GetIntelligence()->LastTimeNotStuck > nTimeSinceLastStuckLimit)
			{
				//! Don't be too aggressive with abandoned player vehicles.
				bool bValidForDeletion = true;
				if(pVehicle->GetLastDriver() && pVehicle->GetLastDriver()->IsPlayer())
				{
					float fPlayerRemovalDist = Lerp(fProportionAmbientVehStopped, TrafficRemoval_RemoveStuckPlayerVehicleDist_Best, TrafficRemoval_RemoveStuckPlayerVehicleDist_Worst);
					Vector3 vDistance = VEC3V_TO_VECTOR3(pVehicle->GetLastDriver()->GetTransform().GetPosition() - pVehicle->GetTransform().GetPosition());
					const float fDistXYSq = vDistance.XYMag2();
					if(fDistXYSq < (fPlayerRemovalDist * fPlayerRemovalDist))
					{
						bValidForDeletion = false;
					}

					u32 nPlayerRemovalTime = Lerp(fProportionAmbientVehStopped, TrafficRemoval_RemoveStuckPlayerVehicleTime_Best, TrafficRemoval_RemoveStuckPlayerVehicleTime_Worst);
					if( (NetworkInterface::GetSyncedTimeInMilliseconds() - pVehicle->m_LastTimeWeHadADriver) < nPlayerRemovalTime)
					{
						bValidForDeletion = false;
					}
				}

				if(bValidForDeletion)
				{
					// Deal with the case of cars stuck head on, or blocking more than 1 car, or 
					// a cycle of 3 cars blocking each other. These need to be removed aggressively.
					if (pVehicle->GetIntelligence()->m_iNumVehsWeAreBlockingThisFrame >= 2 ||
						pVehicle->m_nVehicleFlags.bShouldBeRemovedByAmbientPed ||
						(pVehicle->GetIntelligence()->m_pCarThatIsBlockingUs && pVehicle->GetIntelligence()->m_pCarThatIsBlockingUs->GetIntelligence()->m_pCarThatIsBlockingUs 
						&&
						(pVehicle->GetIntelligence()->m_pCarThatIsBlockingUs->GetIntelligence()->m_pCarThatIsBlockingUs == pVehicle
						|| (pVehicle->GetIntelligence()->m_pCarThatIsBlockingUs->GetIntelligence()->m_pCarThatIsBlockingUs->GetIntelligence()->m_pCarThatIsBlockingUs
						&& pVehicle->GetIntelligence()->m_pCarThatIsBlockingUs->GetIntelligence()->m_pCarThatIsBlockingUs->GetIntelligence()->m_pCarThatIsBlockingUs == pVehicle))))
					{								
						vehPopDebugf2("0x%8p - Remove vehicle not on screen and stuck for at least 10s.", pVehicle);
						removalReason_Out = Removal_StuckOffScreen;
						return true;
					}

					// Deal with the case of a car that is stuck that is blocking x or more cars.
					if(pVehicle->GetIntelligence()->m_NumCarsBehindUs >= nNumCarsStuckBlockingLimit && !pVehicle->GetHasBeenSeen())
					{
						vehPopDebugf2("0x%8p - Remove vehicle that's been stuck, and has never been seen", pVehicle);
						removalReason_Out = Removal_StuckOffScreen;
						return true;
					}
				}
			}

			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_CarsBehindUs_Worst, 4, 1, 32, 1);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_CarsBehindUs_Best, 8, 1, 32, 1);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_TimeSinceLastSeen_Worst, 3000, 0, 60000, 100);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_TimeSinceLastSeen_Best, 30000, 0, 60000, 100);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_NotMovingSecsForRemoval_Worst, 10000, 0, 60000, 100);
			TUNE_GROUP_INT(VEHICLE_POPULATION, TrafficRemoval_NotMovingSecsForRemoval_Best, 45000, 0, 60000, 100);

			u32 nNumCarsBehindUsLimit = Lerp(fProportionAmbientVehStopped, TrafficRemoval_CarsBehindUs_Best, TrafficRemoval_CarsBehindUs_Worst);
			u32 nTimeSinceLastVisibleLimit = Lerp(fProportionAmbientVehStopped, TrafficRemoval_TimeSinceLastSeen_Best, TrafficRemoval_TimeSinceLastSeen_Worst);
			u32 nNotMovingSecsForRemovalLimit = Lerp(fProportionAmbientVehStopped, TrafficRemoval_NotMovingSecsForRemoval_Best, TrafficRemoval_NotMovingSecsForRemoval_Worst);

			const u32 timeSinceLastVisible = fwTimer::GetTimeInMilliseconds() - pVehicle->GetTimeLastVisible();
			if(!pVehicle->GetHasBeenSeen() || (timeSinceLastVisible > nTimeSinceLastVisibleLimit) )
			{
				if(pVehicle->GetIntelligence()->m_NumCarsBehindUs >= nNumCarsBehindUsLimit) 
				{
					vehPopDebugf2("0x%8p - Remove vehicle that's in a long queue, and has never been seen OR hasn't been seen for 30s", pVehicle);
					removalReason_Out = Removal_LongQueue;
					return true;
				}

				// If the car hasn't moved for a long time and we haven't looked at it for a long time remove it regardless of the reason
				if (pVehicle->GetIntelligence()->MillisecondsNotMovingForRemoval > nNotMovingSecsForRemovalLimit && pVehicle->GetStatus() != STATUS_WRECKED)
				{
					vehPopDebugf2("0x%8p - Remove vehicle that's been stuck for over 45s. removal rate scale: %f", pVehicle, removalTimeScale);
					removalReason_Out = Removal_StuckOnScreen;
					return true;
				}
			}
		}
	}

#define WRECKED_REMOVAL_DISTANCE (100.0f)

	if(pVehicle->GetStatus()==STATUS_WRECKED && fDistanceSquared > WRECKED_REMOVAL_DISTANCE * WRECKED_REMOVAL_DISTANCE && pVehicle->GetTimeOfDestruction()!=0 && (fwTimer::GetTimeInMilliseconds() > pVehicle->GetTimeOfDestruction() + CAR_REMOVED_AFTER_WRECKED))
	{
		const float cullRangeOffScreenWrecked = GetCullRangeOffScreenWrecked(rangeScale);

		if(fDistanceSquared > cullRangeOffScreenWrecked * cullRangeOffScreenWrecked)
		{
			vehPopDebugf2("0x%8p - Remove wrecked vehicle off screen, died %d ms ago. distance squared: %.2f, scaled removal range: %.2f", pVehicle, fwTimer::GetTimeInMilliseconds() - pVehicle->GetTimeOfDestruction(), fDistanceSquared, cullRangeOffScreenWrecked);
			removalReason_Out = Removal_WreckedOffScreen;
			return true;
		}
	}

	if(!pVehicle->m_nVehicleFlags.bNeverUseSmallerRemovalRange)
	{
		// If we are zoomed in, then more aggressively removed ambient vehicles
		if(ms_bZoomed && pVehicle->PopTypeGet() == POPTYPE_RANDOM_AMBIENT)
		{
			const float fPlanarDist = vViewPlane.Dot3(vVehiclePosition) + vViewPlane.w;

			// If any ambient vehicle is behind the view plane, then aggressively remove it
			if(fPlanarDist < 0.0f)
			{
				const float fRemovalRange = GetCullRangeOffScreenBehindPlayerWhenZoomed(rangeScale);

				if(fDistanceSquared > fRemovalRange * fRemovalRange)
				{
					removalReason_Out = Removal_OutOfRange_BehindZoomedPlayer;
					return true;
				}
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////


	if(fDistanceSquared <= fInnerBandRadiusMax * fInnerBandRadiusMax)
	{
		BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Too close to local player"));
		return false;
	}

	if(pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED && !CModelInfo::GetAssetsAreDeletable(pVehicle->GetModelId()) && !gPopStreaming.IsCarDiscarded(pVehicle->GetModelIndex()))
	{
		BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Too close to local player (random parked)"));
		return false;
	}

	if(!bSkipShapeTest)
	{
		Vector2 vVehiclePosition2D;
		vVehiclePosition.GetVector2XY(vVehiclePosition2D);

		CPopGenShape::GenCategory category = ms_popGenKeyholeShape.CategorisePoint(vVehiclePosition2D);

		if(category != CPopGenShape::GC_Off && category != CPopGenShape::GC_KeyHoleOuterBand_off)
		{
			BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Too close to local player"));
			return false;
		}
	}

	removalReason_Out = Removal_OutOfRange;
	return true;
}

int CVehiclePopulation::ShouldAggressiveRemovalVehicleForCarjackingMission(CVehicle *pThisVehicle)
{
	int iCount = 0;

	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) VehiclePool->GetSize();

	Vector3 vOurPos = VEC3V_TO_VECTOR3(pThisVehicle->GetTransform().GetPosition());
	const float fRemovalDistSqr = ms_iRadiusForAggressiveRemoalCarjackMission*ms_iRadiusForAggressiveRemoalCarjackMission;

	while(i--)
	{
		pVehicle = VehiclePool->GetSlot(i);
		if( pVehicle && pVehicle->GetIntelligence()->m_bRemoveAggressivelyForCarjackingMission)
		{
			float fDistSqr = vOurPos.Dist2(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
			if(fDistSqr <= fRemovalDistSqr)
			{
				++iCount;
				//don't check pThisVehicle != pVehicle and just do + 1 check here for performance
				if(iCount >= ms_iNumVehiclesInRangeForAggressiveRemovalCarjackMission + 1)
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

bool CVehiclePopulation::CanTreatAsVisibleForRemoval(CVehicle *pVehicle)
{
	if( (pVehicle->GetLastTimeHomedAt() > 0) && (fwTimer::GetTimeInMilliseconds() < (pVehicle->GetLastTimeHomedAt() + 5000)))
	{
		vehPopDebugf2("0x%8p - Skip vehicle removal, treating as visible!", pVehicle);
		return true;
	}

#if GTA_REPLAY
	if(pVehicle->GetIsAircraft() && pVehicle->IsInAir() && CReplayMgr::IsRecording())
	{
		vehPopDebugf2("0x%8p - Skip vehicle removal, this is an airborned plane during a replay!", pVehicle);
		return true;
	}
#endif

	return false;
}

bool CVehiclePopulation::ShouldRemoveVehicle(CVehicle *pVehicle, const Vector3& popCtrlCentre, const Vector4 & vViewPlane, const float rangeScale, const float removalTimeScale, const bool bAircraftPopCentre, EVehLastRemovalReason& removalReason_Out, s32& newDelayedRemovalTime_Out)
{
	bool bRemove = ShouldRemoveVehicleInternal(pVehicle, popCtrlCentre, vViewPlane, rangeScale, removalTimeScale, bAircraftPopCentre, removalReason_Out, newDelayedRemovalTime_Out);

	if(bRemove)
	{
		Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

		if(!pVehicle->CanBeDeleted())
		{
			bRemove = false;
		}
		else if(CGarages::IsVehicleWithinHideOutGarageThatCanSaveIt(pVehicle))
		{
			BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("In hide out garage"));
			bRemove = false;
		}
		else if(pVehicle->GetNetworkObject())
		{
			// see if vehicle is visible to another player, can't remove it if so
			dev_float NETWORK_OFFSCREEN_REMOVAL_RANGE = 150.0f;
			dev_float NETWORK_ONSCREEN_REMOVAL_RANGE  = pVehicle->GetNetworkObject()->GetScopeData().m_scopeDistance;
			const netPlayer* pNearestVisiblePlayer = NULL; 

			bool bIsCloseToAnotherPlayer = NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vVehiclePosition, pVehicle->GetVehicleModelInfo()->GetBoundingSphereRadius(),
				NETWORK_ONSCREEN_REMOVAL_RANGE, NETWORK_OFFSCREEN_REMOVAL_RANGE, &pNearestVisiblePlayer, true, true);

			if(bIsCloseToAnotherPlayer && !((CNetObjVehicle*)pVehicle->GetNetworkObject())->TryToPassControlOutOfScope())
			{
				if(removalReason_Out==Removal_StuckOnScreen)
				{
					pVehicle->GetIntelligence()->NetworkResetMillisecondsNotMovingForRemoval();
				}
				vehPopDebugf2("0x%8p - Skip vehicle removal, still visible to net player", pVehicle);
				BANK_ONLY(pVehicle->SetLastVehPopRemovalFailReason("Visible to another player"));
                
                // if we've failed to pass this to a closer player we need to try again fairly quickly
                // otherwise if temporary failure reasons stack up it leads to "juggernaut cars"
                if(!pVehicle->GetNetworkObject()->IsPendingOwnerChange())
                {
                    const unsigned NEXT_TIME_TO_CHECK = 100;
                    newDelayedRemovalTime_Out = NEXT_TIME_TO_CHECK;
                }

				bRemove = false;
			}
		}
	}

	return bRemove;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetOffscreenRemovalDelayMin
// PURPOSE :  Get how much time before an off screen vehicle is removed.
// PARAMETERS :	None
// RETURNS :	The delay in milliseconds.
/////////////////////////////////////////////////////////////////////////////////
s32 CVehiclePopulation::GetOffscreenRemovalDelayMin()
{
	return ms_vehicleRemovalDelayMsRefreshMin;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetOffscreenRemovalDelayMax
// PURPOSE :  Get how much time before an off screen vehicle is removed.
// PARAMETERS :	None
// RETURNS :	The delay in milliseconds.
/////////////////////////////////////////////////////////////////////////////////
s32 CVehiclePopulation::GetOffscreenRemovalDelayMax()
{
	return ms_vehicleRemovalDelayMsRefreshMax;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetBoxDeleteSize
// PURPOSE :  
// PARAMETERS :	None
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
void CVehiclePopulation::CatchVehicleRemovedInView(CVehicle* pVehicle, s32 iRemovalReason)
{
	if(ms_bCatchVehiclesRemovedInView)
	{
		if(camInterface::IsRenderingDirector( camInterface::GetGameplayDirector() ) )
		{
			Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
			const float fRadius = pVehicle->GetBoundRadius();
			if( camInterface::GetGameplayDirector().IsSphereVisible(vVehiclePos, fRadius) )
			{
				const float fDist = (camInterface::GetGameplayDirector().GetFrame().GetPosition() - vVehiclePos).Mag();
				if(fDist < ms_cullRange)
				{
					Color32 col;
					const char * pRemovalString = NULL;
					switch(iRemovalReason)
					{
						case Removal_None:
							col = Color_black;
							pRemovalString = "Removal_None";
							break;
						case Removal_OutOfRange:
							col = Color_red;
							pRemovalString = "Removal_OutOfRange";
							break;
						case Removal_OutOfRange_OnScreen:
							col = Color_red;
							pRemovalString = "Removal_OutOfRange_OnScreen";
							break;
						case Removal_OutOfRange_Parked:
							col = Color_red;
							pRemovalString = "Removal_OutOfRange_Parked";
							break;
						case Removal_OutOfRange_NoPeds:
							col = Color_red;
							pRemovalString = "Removal_OutOfRange_NoPeds";
							break;
						case Removal_OutOfRange_BehindZoomedPlayer:
							col = Color_red;
							pRemovalString = "Removal_OutOfRange_BehindZoomedPlayer";
							break;
						case Removal_OutOfRange_OuterBand:
							col = Color_red;
							pRemovalString = "Removal_OutOfRange_OuterBand";
							break;
						case Removal_StuckOffScreen:
							col = Color_orange4;
							pRemovalString = "Removal_StuckOffScreen";
							break;
						case Removal_StuckOnScreen:
							col = Color_orange;
							pRemovalString = "Removal_StuckOnScreen";
							break;
						case Removal_LongQueue:
							col = Color_magenta;
							pRemovalString = "Removal_LongQueue";
							break;
						case Removal_WreckedOffScreen:
							col = Color_red4;
							pRemovalString = "Removal_WreckedOffScreen";
							break;
						case Removal_EmptyCopOrWrecked:
							col = Color_red4;
							pRemovalString = "Removal_EmptyCopOrWrecked";
							break;
						case Removal_EmptyCopOrWrecked_Respawn:
							col = Color_red4;
							pRemovalString = "Removal_EmptyCopOrWrecked_Respawn";
							break;
						case Removal_NetGameCongestion:
							col = Color_yellow;
							pRemovalString = "Removal_NetGameCongestion";
							break;
						case Removal_NetGameEmptyCrowdingPlayers:
							col = Color_yellow2;
							pRemovalString = "Removal_NetGameEmptyCrowdingPlayers";
							break;
						case Removal_OccupantAddFailed:
							col = Color_yellow4;
							pRemovalString = "Removal_OccupantAddFailed";
							break;
						case Removal_BadTrailerType:
							col = Color_violet;
							pRemovalString = "Removal_BadTrailerType";
							break;
						case Removal_Debug:
							col = Color_cyan;
							pRemovalString = "Removal_Debug";
							break;
						case Removal_RemoveAllVehicles:
							col = Color_white;
							pRemovalString = "Removal_RemoveAllVehicles";
							break;
						case Removal_ExcessNetworkVehicle:
							col = Color_NavyBlue;
							pRemovalString = "Removal_ExcessNetworkVehicle";
							break;
						case Removal_RemoveAggressivelyFlag:
							col = Color_green;
							pRemovalString = "Removal_RemoveAggressivelyFlag";
							break;
						case Removal_KickedFromVehicleReusePool:
							col = Color_purple;
							pRemovalString = "Removal_KickedFromVehicleReusePool";
							break;
						case Removal_TooSteepAngle:
							col = Color_black;
							pRemovalString = "Removal_TooSteepAngle";
							break;
						case Removal_RemoveAllVehsWithExcepton:
							col = Color_black;
							pRemovalString = "Removal_RemoveAllVehsWithExcepton";
							break;
						default:
							Assert(false);
							col = Color_black;
							pRemovalString = "UNKNOWN_ENUM_VALUE";
							break;
					}

					static dev_s32 iFrames = 640;
					grcDebugDraw::Sphere(vVehiclePos, fRadius, Color_red3, false, iFrames/4);
					grcDebugDraw::Sphere(vVehiclePos, fRadius / 4.0f, Color_red3, false, iFrames, 4);

					char text[128];

					sprintf(text, "%s (%.1fm)", pRemovalString, fDist);
					grcDebugDraw::Text(vVehiclePos, col, 0, 0, text, true, iFrames);

					sprintf(text, "%.1f, %.1f, %.1f", vVehiclePos.x, vVehiclePos.y, vVehiclePos.z);
					grcDebugDraw::Text(vVehiclePos, col, 0, grcDebugDraw::GetScreenSpaceTextHeight(), text, true, iFrames);


					Displayf("**************************************************************************************************");
					Displayf("VEHICLE \"%s\" IS BEING REMOVED IN FULL VIEW", pVehicle->GetModelName());
					Displayf("REASON : %s", pRemovalString);
					Displayf("DISTANCE : %.1f, POSITION : %.1f, %.1f, %.1f", fDist, vVehiclePos.x, vVehiclePos.y, vVehiclePos.z);
					Displayf("**************************************************************************************************");

					sysStack::PrintStackTrace();


				}
			}
		}
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveVeh
// PURPOSE :  Removes a single vehicle
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::RemoveVeh(CVehicle* pVehicle, bool cached, s32 BANK_ONLY(iRemovalReason) )
{
	BANK_ONLY( CatchVehicleRemovedInView(pVehicle, iRemovalReason); )

	CVehicleFactory::GetFactory()->Destroy(pVehicle, cached);

	BANK_ONLY(ms_lastRemovalReason = Removal_None);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddVehicleToReusePool
// PURPOSE :  Adds a vehicle to the reuse pool, kicking out an old vehicle if needed. Places the vehicle below the world and handles removing it from the world.
// PARAMETERS :	the vehicle to be added to the reuse pool
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::AddVehicleToReusePool(CVehicle* pVehicle)
{
#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker storeVeh("Store Vehicle For Reuse", 22);
#endif

	if(ms_vehicleReusePool.IsFull())
	{
		DropOneVehicleFromReusePool();
	}

	CVehicleReuseEntry& reuseVeh = ms_vehicleReusePool.Append();
	reuseVeh.pVehicle = pVehicle;
	reuseVeh.uFrameCreated = fwTimer::GetFrameCount();


	// inform network object manager about this vehicle being deleted
	if (pVehicle->GetNetworkObject())
	{
		vehPopDebugf3("Unregistering Vehicle from Network for reuse");
		if (pVehicle->IsNetworkClone())
		{
			if (pVehicle->GetNetworkObject()->IsScriptObject() && !pVehicle->PopTypeIsMission())
				Assertf(0, "Warning: clone %s is being destroyed (it is not a mission vehicle anymore)!", pVehicle->GetNetworkObject()->GetLogName());
			else
				Assertf(0, "Warning: clone %s is being destroyed!", pVehicle->GetNetworkObject()->GetLogName());

			// this is bad
			Errorf("Trying to reuse network clone - this shouldn't be happening");
		}

		NetworkInterface::UnregisterObject(pVehicle);
	}

	// Ensure that any vehicle being removed to the cache is removed from the pathserver also
	CPathServerGta::MaybeRemoveDynamicObject(pVehicle);

	// Remove refs on this vehicle's dynamic navmesh if it has one
	CPathServerGta::MaybeRemoveDynamicObjectNavMesh(pVehicle);

	// Clean dummy transition surface
	CWheel * const * ppWheels = pVehicle->GetWheels();
	for(int i=0; i<pVehicle->GetNumWheels(); i++)
	{
		ppWheels[i]->EndDummyTransition();
	}

	// freeze the vehicle
	pVehicle->m_nDEflags.bFrozen = true;

	// Must occur before call to CVehicle::SetIsInReusePool(true)
	UpdateVehCount(pVehicle, SUB);

	// Must occur before call to CVehicle::Teleport()
	pVehicle->SetIsInReusePool(true);

	CSpatialArrayNode& node = pVehicle->GetSpatialArrayNode();
	if(node.IsInserted())
	{
		CVehicle::GetSpatialArray().Remove(node);
	}

	if(pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->Shutdown();
	}	

	CGameWorld::Remove(pVehicle);

	CTheScripts::UnregisterEntity(pVehicle, false);

	pVehicle->RemovePhysics();

	// teleport it to a safe spot
	Vector3 newPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	newPosition.z	 -= 100.f; // teleport it under the world
	pVehicle->Teleport(newPosition, 0.f, false, false, false);	

	// flush tasks
	pVehicle->GetIntelligence()->GetTaskManager()->AbortTasks();

	pVehicle->GetFrameCollisionHistory()->Reset();
	pVehicle->GetIntelligence()->ClearScanners();
	pVehicle->ClearInterpolationTargetAndTime();

	// Properly remove from any junction we might be in.
	CVehicleIntelligence* pIntel = pVehicle->GetIntelligence();
	if(!pIntel->GetJunctionNode().IsEmpty())
	{
		CJunctions::RemoveVehicleFromJunction(pVehicle, pIntel->GetJunctionNode());
	}
	pIntel->ResetCachedJunctionInfo();

	pVehicle->GetIntelligence()->InvalidateCachedNodeList();
	pVehicle->GetIntelligence()->InvalidateCachedFollowRoute();
	pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();

	pVehicle->GetIntelligence()->SetCarWeAreBehind(NULL);
	
	pVehicle->GetPortalTracker()->DontRequestScaneNextUpdate();
	pVehicle->GetPortalTracker()->StopScanUntilProbeTrue();

	pVehicle->ResetExclusiveDriverForReuse();
	pVehicle->SetVelocity(VEC3_ZERO);
	pVehicle->SetAngVelocity(VEC3_ZERO);
	pVehicle->SetSuperDummyVelocity(VEC3_ZERO);
	pVehicle->SetEasyToLand(false);
	BANK_ONLY(pVehicle->ResetDebugObjectID());

	if (pVehicle->InheritsFromAutomobile())
		((CAutomobile*)pVehicle)->RemoveConstraints();
	else if (pVehicle->InheritsFromBike())
		((CBike*)pVehicle)->RemoveConstraint();
	else if (pVehicle->InheritsFromBoat())
	{
		Assertf(false, "Reusing a boat - this shouldn't be happening");
		CBoat* boat = (CBoat*)pVehicle;
		boat->DetachFromParentAndChildren(DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS);
	}

	if (pVehicle->GetDummyInst())
	{
		pVehicle->GetDummyInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

		if (pVehicle->GetDummyInst()->IsInLevel())
		{
			CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(pVehicle->GetDummyInst()->GetLevelIndex(), 0, 0);
			Vec3V vPos = pVehicle->GetDummyInst()->GetPosition();
			vPos.SetZf(newPosition.z);
			pVehicle->GetDummyInst()->SetPosition(vPos);
			if (CPhysics::GetLevel()->IsActive(pVehicle->GetDummyInst()->GetLevelIndex()))
				CPhysics::GetSimulator()->DeactivateObject(pVehicle->GetDummyInst()->GetLevelIndex());

			pVehicle->RemoveConstraints(pVehicle->GetDummyInst());
		}
	}

	if (pVehicle->GetVehicleFragInst())
	{
		pVehicle->GetVehicleFragInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

		if (pVehicle->GetVehicleFragInst()->IsInLevel())
		{
			CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(pVehicle->GetVehicleFragInst()->GetLevelIndex(), 0, 0);
			Vec3V vPos = pVehicle->GetVehicleFragInst()->GetPosition();
			vPos.SetZf(newPosition.z);
			pVehicle->GetVehicleFragInst()->SetPosition(vPos);
			if (CPhysics::GetLevel()->IsActive(pVehicle->GetVehicleFragInst()->GetLevelIndex()))
				CPhysics::GetSimulator()->DeactivateObject(pVehicle->GetVehicleFragInst()->GetLevelIndex());

			pVehicle->RemoveConstraints(pVehicle->GetFragInst());
		}
	}

	fwScriptGuid::DeleteGuid(*pVehicle);

	// update vehicle model ref
	pVehicle->GetVehicleModelInfo()->AddVehicleModelRefReusePool();

#if __ASSERT
	if(pVehicle->InheritsFromAutomobile())
	{
		CAutomobile * pAuto = static_cast<CAutomobile*>(pVehicle);
		Assert(!pAuto->m_dummyStayOnRoadConstraintHandle.IsValid());
		Assert(!pAuto->m_dummyRotationalConstraint.IsValid());
	}
#endif

	Assert(!pVehicle->GetIsRetainedByInteriorProxy() && pVehicle->GetOwnerEntityContainer() == NULL);

	vehicleReuseDebugf1("Storing vehicle %s (%p) for reuse, teleported to <%.0f, %.0f, %.0f>, frame %d", pVehicle->GetModelName(), pVehicle, newPosition.x, newPosition.y, newPosition.z, TIME.GetFrameCount());	

	IncrementNumVehiclesDestroyedThisCycle();

	REPLAY_ONLY(CReplayMgr::OnDelete(pVehicle));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindVehicleToReuse
// PURPOSE :  Finds a valid vehicle in the reuse pool by matching the model id
// PARAMETERS :	the vehicle model id we want
// RETURNS :	the index of the vehicle in the pool we want, or -1
/////////////////////////////////////////////////////////////////////////////////
u32 CVehiclePopulation::FindVehicleToReuse(u32 vehModel)
{
	int reusedVehicleIndex = -1;
	for(int i = 0; i < ms_vehicleReusePool.GetCount(); i++)
	{
		if(ms_vehicleReusePool[i].pVehicle)
		{
			// test for the vehicle we want
			if(ms_vehicleReusePool[i].pVehicle->GetModelId() == fwModelId(strLocalIndex(vehModel)))
			{
				// found a vehicle we can use				
				reusedVehicleIndex = i;
				break;
			}
		}
	}

	return reusedVehicleIndex;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetVehicleFromReusePool
// PURPOSE :  Returns the vehicle at the given index in the reuse pool
// PARAMETERS :	the index of the vehicle we want (from FindVehicleToReuse)
// RETURNS :	the vehicle we want, or NULL
/////////////////////////////////////////////////////////////////////////////////
CVehicle*	 CVehiclePopulation::GetVehicleFromReusePool(int vehReuseIndex)
{
	if(vehReuseIndex < ms_vehicleReusePool.GetCount())
	{
		return ms_vehicleReusePool[vehReuseIndex].pVehicle;
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveVehicleFromReusePool
// PURPOSE :  Remove the vehicle at the given index from the reuse pool
// PARAMETERS :	the index of the vehicle (from FindVehicleToReuse)
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::RemoveVehicleFromReusePool(int vehReuseIndex)
{
	if(vehReuseIndex < ms_vehicleReusePool.GetCount())
	{
		ms_vehicleReusePool[vehReuseIndex].pVehicle = NULL;
		ms_vehicleReusePool.Delete(vehReuseIndex);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ReuseVehicleFromVehicleReusePool
// PURPOSE :  Reuses a vehicle from the reuse pool. Sets it up like a newly created vehicle
// PARAMETERS :	A pointer to the vehicle we want to reuse (it has to be in the reuse pool), the new vehicle matrix, and the vehicles new ePopType
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::ReuseVehicleFromVehicleReusePool(CVehicle* pVehicle, Matrix34& newMatrix, ePopType popType, eEntityOwnedBy ownedBy, bool bClone, bool bCreateAsInactive)
{
#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker reuseVeh("Reuse Vehicle", 23);
#endif

	if(bClone && NetworkInterface::IsGameInProgress() && !NetworkInterface::CanRegisterObject(pVehicle->GetNetworkObjectType(), popType == POPTYPE_MISSION ? true : false))
	{
		vehPopErrorf("Can't reuse vehicle - Network can't register type");
        gnetDebug2("Failed to create vehicle due to network population limit - Reason: %s", CNetworkObjectPopulationMgr::GetLastFailTypeString());
		return false;
	}

	PF_INCREMENT(VehiclesReused);
	Assert(pVehicle->GetIsInReusePool());
	vehicleReuseDebugf1("Reusing vehicle %s (%p), teleported to <%.0f, %.0f, %.0f>, frame %d", pVehicle->GetModelName(), pVehicle, newMatrix.d.GetX(), newMatrix.d.GetY(), newMatrix.d.GetZ(), TIME.GetFrameCount());
	Assert(!pVehicle->GetIsRetainedByInteriorProxy() && pVehicle->GetOwnerEntityContainer() == NULL);

	pVehicle->SetOwnedBy(ownedBy);

	if (pVehicle->GetDummyInst() && pVehicle->GetDummyInst()->IsInLevel())
	{
		CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(pVehicle->GetDummyInst()->GetLevelIndex(), ArchetypeFlags::GTA_BOX_VEHICLE_TYPE, ArchetypeFlags::GTA_BOX_VEHICLE_INCLUDE_TYPES);
		pVehicle->SwitchCurrentPhysicsInst(pVehicle->GetDummyInst());		
	}
	if (pVehicle->GetVehicleFragInst() && pVehicle->GetVehicleFragInst()->IsInLevel())
	{
		CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(pVehicle->GetVehicleFragInst()->GetLevelIndex(), ArchetypeFlags::GTA_VEHICLE_TYPE, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
		pVehicle->SwitchCurrentPhysicsInst(pVehicle->GetVehicleFragInst());		
	}
	Assert(pVehicle->GetCurrentPhysicsInst()->IsInLevel());

	pVehicle->Teleport(newMatrix.d, -10.f, false, false);
	pVehicle->SetMatrix(newMatrix, true, true, true);

	pVehicle->SetVelocity(VEC3_ZERO);
	pVehicle->SetAngVelocity(VEC3_ZERO);
	pVehicle->SetSuperDummyVelocity(VEC3_ZERO);

	pVehicle->m_nPhysicalFlags.bNotToBeNetworked = !bClone;
	pVehicle->SetAllWindowComponentsDirty();

	// Network object init
	if (bClone && NetworkInterface::IsGameInProgress())
	{
		vehPopDebugf3("Registering reused vehicle to network");
		if(!CVehicleFactory::GetFactory()->RegisterVehicleToNetwork(pVehicle, popType))
		{			
			// vehicle could not be registered as there is no available network objects for it, so it has to be removed
			// unless it is a mission object, which is queued to be registered later
			// Remove the vehicle, as it's in a weird state now

			pVehicle->SetIsInReusePool(false);

			// find the vehicle in the reuse pool
			int foundIndex = -1;
			for(int i = 0; i < ms_vehicleReusePool.GetCount(); i++)
			{
				if(ms_vehicleReusePool[i].pVehicle == pVehicle)
				{
					foundIndex = i;
					break; // found it
				}
			}
			Assert(foundIndex != -1);

			vehicleReuseDebugf3("Failed to register vehicle - Removing vehicle");
			BANK_ONLY(ms_lastRemovalReason = Removal_KickedFromVehicleReusePool);
			RemoveVeh(ms_vehicleReusePool[foundIndex].pVehicle, true, Removal_KickedFromVehicleReusePool);
			ms_vehicleReusePool[foundIndex].pVehicle = NULL;
			ms_vehicleReusePool.Delete(foundIndex);
			return false;
		}
	}


	pVehicle->m_TimeOfCreation = fwTimer::GetTimeInMilliseconds();
	pVehicle->m_CarGenThatCreatedUs = -1;

	pVehicle->CalculateInitialLightStateBasedOnClock();

#if __BANK
	pVehicle->m_vCreatedPos = VECTOR3_TO_VEC3V(newMatrix.d);
#endif
	pVehicle->m_vecInitialSpawnPosition = VECTOR3_TO_VEC3V(newMatrix.d);

	pVehicle->m_nVehicleFlags.bCreatingAsInactive = bCreateAsInactive;
	pVehicle->DelayedRemovalTimeReset();

	pVehicle->GetIntelligence()->ResetVariables(fwTimer::GetTimeStep());
	pVehicle->GetIntelligence()->m_pCarThatIsBlockingUs = NULL;
	pVehicle->GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();
	pVehicle->GetIntelligence()->LastTimeHonkedAt = 0;
	pVehicle->GetIntelligence()->MillisecondsNotMoving = 0;
	pVehicle->GetIntelligence()->m_lastTimeTriedToPushPlayerPed = 0;
	pVehicle->GetIntelligence()->DeleteScenarioPointHistory();
	pVehicle->GetIntelligence()->m_fSmoothedSpeedSq = 0.0f;
	pVehicle->GetIntelligence()->m_nTimeLastTriedToShakePedFromRoof = 0;
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = false;	
	pVehicle->m_nVehicleFlags.SetSirenOn(false, false);
	pVehicle->m_nVehicleFlags.SetSirenMutedByScript(false);
	pVehicle->m_nVehicleFlags.bMadDriver = false;
	pVehicle->m_nVehicleFlags.bSlowChillinDriver = false;
	pVehicle->m_nVehicleFlags.bInMotionFromExplosion = false;
	pVehicle->m_nVehicleFlags.bIsAmbulanceOnDuty = false;
	pVehicle->m_nVehicleFlags.bIsFireTruckOnDuty = false;
	pVehicle->m_nVehicleFlags.bIsRacing = false;
	pVehicle->m_nVehicleFlags.bTryToRemoveAggressively = false;		// Don't remove us again!
	pVehicle->m_nVehicleFlags.bDisablePretendOccupants = false;
	pVehicle->m_nVehicleFlags.bLodForceUpdateUntilNextAiUpdate = false;
	pVehicle->m_nVehicleFlags.bMoreAccurateVirtualRoadProbes = false;
	pVehicle->GetIntelligence()->SetPretendOccupantAggressivness(-1.0f);
	pVehicle->GetIntelligence()->SetPretendOccupantAbility(-1.0f);
	pVehicle->GetIntelligence()->SetCustomPathNodeStreamingRadius(-1.0f);
	pVehicle->GetIntelligence()->m_uTimeLastTouchedRealMissionVehicleWhileDummy = 0;

	// Forget about any previous dummy/real conversion attempts.
	pVehicle->m_dummyConvertAttemptTimeMs = 0;
	pVehicle->m_realConvertAttemptTimeMs = 0;

	CPortalTracker *pPortalTracker = pVehicle->GetPortalTracker();
	pPortalTracker->Teleport();

	// David Ely: Get Klaas to buddy this
	CPortalTracker::eProbeType probeType = GetVehicleProbeType(pVehicle);
	pPortalTracker->SetProbeType(probeType);
	pPortalTracker->RequestRescanNextUpdate();

	CSpatialArrayNode& spatialArrayNode = pVehicle->GetSpatialArrayNode();
	Assert(!spatialArrayNode.IsInserted());
	CVehicle::GetSpatialArray().Insert(spatialArrayNode);
	if(spatialArrayNode.IsInserted())
	{
		pVehicle->UpdateInSpatialArray();
	}

	pVehicle->RemovePhysics();

	pVehicle->GiveDefaultTask();

	// Update vehicle model refs
	pVehicle->GetVehicleModelInfo()->RemoveVehicleModelRefReusePool();

	// make sure visibility is set correctly
	pVehicle->SetIsVisibleForAllModules();

	// Make sure the vehicle LOD is set up in a reasonable way.
	CVehicleAILodManager::ResetOnReuseVehicle(*pVehicle);

	pVehicle->m_nDEflags.bFrozen = false;

	if(pVehicle->GetVehicleAudioEntity())
	{
		pVehicle->GetVehicleAudioEntity()->Init(pVehicle);
	}

	Assert(!pVehicle->GetIsRetainedByInteriorProxy() && pVehicle->GetOwnerEntityContainer() == NULL);

	IncrementNumVehiclesCreatedThisCycle();

	pVehicle->SetIsInReusePool(false);
	BANK_ONLY(pVehicle->SetWasReused(true);)

	UpdateVehCount(pVehicle, ADD); // add this vehicle back to the population count
	pVehicle->PopTypeSet(popType); // change it to the correct pop type

	BANK_ONLY(pVehicle->AssignDebugObjectID());

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(pVehicle));

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateVehicleReusePool
// PURPOSE :  Updates the vehicle reuse pool, removing entries for vehicles that have been deleted or have timed out
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::UpdateVehicleReusePool()
{
	PF_PUSH_TIMEBAR("Update Vehicle Reuse Pool");

	for(int i = ms_vehicleReusePool.GetCount() - 1; i >= 0; i--)
	{
		if(!ms_vehicleReusePool[i].pVehicle)
		{
			vehicleReuseDebugf3("Removing vehicle that has been deleted from Reuse Pool");
			ms_vehicleReusePool.Delete(i);
			continue;
		}

		Assert(!ms_vehicleReusePool[i].pVehicle->GetIsRetainedByInteriorProxy() && ms_vehicleReusePool[i].pVehicle->GetOwnerEntityContainer() == NULL);

		if(ms_iNumVehiclesDestroyedThisCycle == 0 && (fwTimer::GetFrameCount() - ms_vehicleReusePool[i].uFrameCreated) >= ms_ReusedVehicleTimeout)
		{
			// check to see if we still want this vehicle
			bool removeVehicle = false;
			
			if(NetworkInterface::IsGameInProgress() && !ms_bVehicleReuseInMP)
				removeVehicle = true; // remove vehicles if we're in a network game

			if(gPopStreaming.GetDiscardedCars().IsMemberInGroup(ms_vehicleReusePool[i].pVehicle->GetModelIndex())) // check for discardable
			{
				vehicleReuseDebugf3("Vehicle (%s) in reuse pool is discardable, discarding", ms_vehicleReusePool[i].pVehicle->GetModelName());
				removeVehicle = true;
			}
			
			if(!gPopStreaming.GetAppropriateLoadedCars().IsMemberInGroup(ms_vehicleReusePool[i].pVehicle->GetModelIndex()))
			{
				// didn't find the model in a valid pop group, drop it as we don't want it anymore
				vehicleReuseDebugf3("Vehicle (%s) %p isn't in a valid pop group, removing", ms_vehicleReusePool[i].pVehicle->GetModelName(), ms_vehicleReusePool[i].pVehicle.Get());
				removeVehicle = true;
			}
			
			if(removeVehicle)
			{
				// Remove the vehicle
				vehicleReuseDebugf3("Removing vehicle from Reuse Pool");
				BANK_ONLY(ms_lastRemovalReason = Removal_KickedFromVehicleReusePool);
				RemoveVeh(ms_vehicleReusePool[i].pVehicle, true, Removal_KickedFromVehicleReusePool);
				ms_vehicleReusePool[i].pVehicle = NULL;
				ms_vehicleReusePool.Delete(i);
			}
			else
			{
				ms_vehicleReusePool[i].uFrameCreated = fwTimer::GetFrameCount();
			}
		}
	}

	PF_INCREMENTBY(VehiclesInReusePool, ms_vehicleReusePool.GetCount());
	PF_POP_TIMEBAR();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FlushVehicleReusePool
// PURPOSE :  Clears out the vehicle reuse pool
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::FlushVehicleReusePool()
{
	vehicleReuseDebugf1("Flushing Vehicle Reuse Pool");
	for(int i = ms_vehicleReusePool.GetCount() - 1; i >= 0; i--)
	{
		// Remove the vehicle
		if(ms_vehicleReusePool[i].pVehicle)
		{			
			BANK_ONLY(ms_lastRemovalReason = Removal_KickedFromVehicleReusePool);
			RemoveVeh(ms_vehicleReusePool[i].pVehicle, true, Removal_KickedFromVehicleReusePool);
			ms_vehicleReusePool[i].pVehicle = NULL;			
		}
		ms_vehicleReusePool.Delete(i);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DropOneVehicleFromReusePool
// PURPOSE :	Deletes the oldest vehicle in the reuse pool
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::DropOneVehicleFromReusePool()
{
	if(ms_vehicleReusePool.IsEmpty())
		return;

	int oldestEntryIndex = 0;
	// eject the oldest vehicle
	for(int i = 0; i < ms_vehicleReusePool.GetCount(); i++)
	{
		if(!ms_vehicleReusePool[i].pVehicle) // if a vehicle is already gone, just use that index
		{
			oldestEntryIndex = i;
			break;
		}
		if(ms_vehicleReusePool[i].uFrameCreated < ms_vehicleReusePool[oldestEntryIndex].uFrameCreated)
		{
			oldestEntryIndex = i;
		}
	}

	if(ms_vehicleReusePool[oldestEntryIndex].pVehicle)
	{
		vehicleReuseDebugf3("Removing oldest vehicle from reuse pool");
		// delete the extra vehicle
		BANK_ONLY(ms_lastRemovalReason = Removal_KickedFromVehicleReusePool);
		RemoveVeh(ms_vehicleReusePool[oldestEntryIndex].pVehicle, true, Removal_KickedFromVehicleReusePool);
	}

	ms_vehicleReusePool.Delete(oldestEntryIndex);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CanVehicleBeReused
// PURPOSE :  Decides if a vehicle is fit for reuse
// PARAMETERS :	A pointer to the vehicle being examined
// RETURNS :	True if the vehicle can be added to the reuse pool
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::CanVehicleBeReused(CVehicle* pVehicle)
{
	if(!ms_bVehicleReusePool)
		return false;
	
	if(NetworkInterface::IsGameInProgress() && !ms_bVehicleReuseInMP)
		return false; // disable population cache in network games (for now)

	if(fragManager::GetFragCacheAllocator()->GetMemoryUsed() >= FRAG_CACHE_POP_REUSE_MAX)
		return false; // no reuse - frag cache pool is full

	if(pVehicle)
	{
		if(ms_bVehicleReuseChecksHasBeenSeen && pVehicle->GetHasBeenSeen())
			return false; // has been seen by the player

		if(pVehicle->GetIsInReusePool())
			return false; // already in the reuse pool

		if(pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)		
			return false;// only cars, please				

		if(CVehicle::IsBusModelId(pVehicle->GetModelId()))
			return false; // don't use buses

		if(CVehicle::IsLawEnforcementVehicleModelId(pVehicle->GetModelId()))
			return false; // or cops

		if(pVehicle->GetAttachedTrailer() || pVehicle->InheritsFromTrailer())
			return false; // no trailers

		if(pVehicle->HasDummyAttachmentChildren() || pVehicle->GetDummyAttachmentParent())
			return false; // no dummy trailers

		if(pVehicle->GetVehicleDamage()->GetOverallHealth() < pVehicle->GetMaxHealth()) // no damaged vehicles	
			return false;		

		if(gPopStreaming.GetDiscardedCars().IsMemberInGroup(pVehicle->GetModelIndex())) // or discarded car models
			return false;

        if(pVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
            return false;

		// see if we have this model already
#define MAX_IDENTICAL_VEHICLES_IN_REUSE_POOL (2)
		u32 identicalFound = 0;
		for(int i = 0; i < ms_vehicleReusePool.GetCount(); i++)
		{
			if(ms_vehicleReusePool[i].pVehicle && ms_vehicleReusePool[i].pVehicle->GetModelId() == pVehicle->GetModelId())
			{
				// found identical vehicle
				identicalFound++;
			}

			if(identicalFound >= MAX_IDENTICAL_VEHICLES_IN_REUSE_POOL)
				return false; // don't want this vehicle, already have it
		}

		const eVehicleDummyMode currentMode = pVehicle->GetVehicleAiLod().GetDummyMode();

		if(currentMode != VDM_SUPERDUMMY)
			return false;
				
		if(!pVehicle->IsUsingPretendOccupants()) // no real ped occupants, only pretend
			return false;

		// check the seat manager as well
		if(const CSeatManager* seatManager = pVehicle->GetSeatManager())
		{
			if(seatManager->GetNumPlayers())
			{
				return false; // no players!
			}

			if(seatManager->CountPedsInSeats())
			{
				return false; // no peds at all!
			}

			if(seatManager->GetNumScheduledOccupants())
			{
				return false; // no sceduled occupants!
			}
		}

		// last, check if this is a car we actually might want
		if(!gPopStreaming.GetAppropriateLoadedCars().IsMemberInGroup(pVehicle->GetModelIndex()))
			return false;

		// we made it. Good to reuse
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CountVehsOfType
// PURPOSE :  Goes through the cars and counts the ones that are a specific type.
// PARAMETERS :	ModelIndex- the car type we're interested in
// RETURNS :	Number of cars of the given type
/////////////////////////////////////////////////////////////////////////////////
s32 CVehiclePopulation::CountVehsOfType(u32 ModelIndex, bool bIgnoreParkedVehicles, bool bOnlyCountRandomPopType, bool bIgnoreWrecked)
{
	s32	Total = 0;

	// Instead of checking all the sectors in a certain radius. Use the Automobile Pool
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) VehiclePool->GetSize();

	while(i--)
	{
		pVehicle = VehiclePool->GetSlot(i);
		if( pVehicle && pVehicle->GetModelIndex() == ModelIndex &&
			(!bIgnoreParkedVehicles || !pVehicle->m_nVehicleFlags.bIsThisAParkedCar) &&
			(!bOnlyCountRandomPopType || pVehicle->PopTypeIsRandom()) &&
			(!bIgnoreWrecked || (pVehicle->GetStatus() != STATUS_WRECKED)))
		{
			Total++;
		}
	}
	return (Total);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetTotalVehsOnMap
// PURPOSE :  Counts up how many cars are on the map currently.
// PARAMETERS :	None.
// RETURNS :	Number of vehs on the map.
/////////////////////////////////////////////////////////////////////////////////
s32 CVehiclePopulation::GetTotalVehsOnMap()
{
	return ms_numRandomCars + ms_numMissionCars + ms_numParkedCars + ms_numParkedMissionCars + ms_numPermanentVehicles;
}

s32 CVehiclePopulation::GetTotalNonMissionVehsOnMap()
{
	return ms_numRandomCars + ms_numParkedCars + ms_numPermanentVehicles;
}

s32 CVehiclePopulation::GetTotalAmbientMovingVehsOnMap()
{
	return ms_numRandomCars;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateVehCount
// PURPOSE :  Has to be called when vehicle get removed/created to update the
//			  count of the group it belongs to.
// PARAMETERS :	pVeh- the car we're interested in
//				AddOrSub- whether we want to add or subtracted the vehicle to
//				the population
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::UpdateVehCount(CVehicle* pVeh, VehPopOp AddOrSub)
{
	if (pVeh->GetIsInReusePool())
	{
#if __BANK
		vehPopDebugf1("0x%8p - Skipping vehicle count. In re-use pool. Creation Type (%d) Pop Type (%d) for %s", pVeh, pVeh->m_CreationPopType, pVeh->PopTypeGet(), pVeh->GetNetworkObject() ? pVeh->GetNetworkObject()->GetLogName() : pVeh->GetDebugName());
#endif
		return;
	}

	if (AddOrSub == CVehiclePopulation::ADD)
	{
		Assert(pVeh->m_CreationPopType == POPTYPE_UNKNOWN);

		switch(pVeh->PopTypeGet())
		{
			case POPTYPE_UNKNOWN:
				Assertf(false, "The vehicle should have a valid pop type.");
				break;
			case POPTYPE_RANDOM_PERMANENT :
				// Planes, trains and helicopters
				++ms_numPermanentVehicles;
				break;
			case POPTYPE_RANDOM_PARKED :
				if (pVeh->m_nVehicleFlags.bCreatedByLowPrioCargen)
					++ms_numLowPrioParkedCars;
				++ms_numParkedCars;
				break;
			case POPTYPE_RANDOM_PATROL : // Purposeful fall through.
			case POPTYPE_RANDOM_SCENARIO : // Purposeful fall through.
			case POPTYPE_RANDOM_AMBIENT :
				++ms_numRandomCars;
				break;
			case POPTYPE_PERMANENT :
				++ms_numPermanentVehicles;
				break;
			case POPTYPE_MISSION :// Purposeful fall through.
			case POPTYPE_REPLAY :
				// Doesn't really matter if MISSION car is Law Enforcer - just check if it ever happens
				if (pVeh->m_nVehicleFlags.bCountAsParkedCarForPopulation)
				{
					++ms_numParkedMissionCars;
				}
				else
				{
					++ms_numMissionCars;
				}
				break;
			case POPTYPE_TOOL :
				break;
			case POPTYPE_CACHE:
				break;
			case NUM_POPTYPES :
				Assert(0);
		}

#if __BANK
		pVeh->m_CreationPopType = pVeh->PopTypeGet();
		vehPopDebugf1("0x%8p - ADD vehicle count. Pop Type (%d) for %s", pVeh, pVeh->m_CreationPopType, pVeh->GetNetworkObject() ? pVeh->GetNetworkObject()->GetLogName() : pVeh->GetDebugName());
#endif
	}
	else	//	VehPopOp::SUB
	{
#if __BANK
		vehPopDebugf1("0x%8p - SUB Vehicle count. Creation Type (%d) Pop Type (%d) for %s", pVeh, pVeh->m_CreationPopType, pVeh->PopTypeGet(), pVeh->GetNetworkObject() ? pVeh->GetNetworkObject()->GetLogName() : pVeh->GetDebugName());
		Assertf(pVeh->m_CreationPopType == pVeh->PopTypeGet(), "Pop type has changed. This will remove vehicle (%s) from wrong slot! (%d:%d)", pVeh->GetNetworkObject() ? pVeh->GetNetworkObject()->GetLogName() : pVeh->GetDebugName(), pVeh->m_CreationPopType, pVeh->PopTypeGet());
#endif

		switch(pVeh->PopTypeGet())
		{
			case POPTYPE_UNKNOWN:
				Assertf(false, "The vehicle should have a valid pop type.");
				break;
			case POPTYPE_RANDOM_PERMANENT :
				--ms_numPermanentVehicles;
				if (ms_numPermanentVehicles < 0)
				{

#if (__XENON || __PS3)
					Warningf("CCarCtrl::UpdateCarCount - ms_NumPermanentVehicles is less than 0");
#else //(__XENON || __PS3)
					Assertf(0, "CCarCtrl::UpdateCarCount - ms_NumPermanentVehicles is less than 0");
#endif // (__XENON || __PS3)

					ms_numPermanentVehicles = 0;
				}
				break;
			case POPTYPE_RANDOM_PARKED :
				if (pVeh->m_nVehicleFlags.bCreatedByLowPrioCargen)
				{
					--ms_numLowPrioParkedCars;
					if (ms_numLowPrioParkedCars < 0)
						ms_numLowPrioParkedCars = 0; // this might reach zero as regular parked cars could use the lowprio budget
				}
				--ms_numParkedCars;
				if (ms_numParkedCars < 0)
				{
					Assertf(0, "CCarCtrl::UpdateCarCount - ms_NumParkedCars is less than 0");
					ms_numParkedCars = 0;
				}
				break;
			case POPTYPE_RANDOM_PATROL : // Purposeful fall through.
			case POPTYPE_RANDOM_SCENARIO : // Purposeful fall through.
			case POPTYPE_RANDOM_AMBIENT :
				--ms_numRandomCars;
				if (ms_numRandomCars < 0)
				{
					Assertf(0, "CCarCtrl::UpdateCarCount - ms_NumRandomCars is less than 0");
					ms_numRandomCars = 0;
				}
				break;
			case POPTYPE_PERMANENT :
				--ms_numPermanentVehicles;
				break;
			case POPTYPE_MISSION :// Purposeful fall through.
			case POPTYPE_REPLAY :
				if (pVeh->m_nVehicleFlags.bCountAsParkedCarForPopulation)
				{
					--ms_numParkedMissionCars;
				}
				else
				{
					--ms_numMissionCars;
				}

				if (ms_numParkedMissionCars < 0)
				{
					Assertf(0, "CCarCtrl::UpdateCarCount - ms_NumParkedMissionCars is less than 0");
					ms_numParkedMissionCars = 0;
				}
				if (ms_numMissionCars < 0)
				{
					Assertf(0, "CCarCtrl::UpdateCarCount - ms_NumMissionCars is less than 0");
					ms_numMissionCars = 0;
				}
				break;
			case POPTYPE_TOOL :
				break;
			case POPTYPE_CACHE :
				break;
			case NUM_POPTYPES :
				Assert(0);
		}

		BANK_ONLY(pVeh->m_CreationPopType = POPTYPE_UNKNOWN);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CanGenerateRandomCar
// PURPOSE :  Goes through the cars and counts the ones that are a specific type.
// PARAMETERS :	ModelIndex- the car type we're interested in
// RETURNS :	Number of cars of the given type
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::CanGenerateRandomCar()
{
	return (ms_numRandomCars < ms_maxRandomVehs);
}


/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::CanGenerateMissionCar()
{
	return (ms_numMissionCars < ms_maxNumMissionVehs);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CreateCarForScript
// PURPOSE :  
// PARAMETERS :	ModelIndex- the car type we want.
//				vecPosition- where to create it.
// RETURNS :	The car that was created.
/////////////////////////////////////////////////////////////////////////////////
CVehicle *CVehiclePopulation::CreateCarForScript(u32 ModelIndex, const Vector3& vecPosition, float fHeading, bool bRegisterAsNetworkObject, scrThreadId iMissionScriptId, bool bIgnoreGroundCheck)
{
#if __BANK
	utimer_t startTime = sysTimer::GetTicks();
#endif // __BANK

	fwModelId modelId((strLocalIndex(ModelIndex)));
	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
	CBoat *pNewBoat;
	CVehicle *pNewVehicle;
	float fSetHeading;

	// Bail out if the car is not loaded
	if(!scriptVerifyf(CModelInfo::HaveAssetsLoaded(modelId),"Vehicle model %s is not loaded!",pModelInfo->GetModelName()))
	{
		vehPopDebugf2("Can't create car for script, not yet loaded. model index: %d", ModelIndex);
		return NULL;
	}

	Vector3 carPosition = vecPosition;

	if (pModelInfo->GetIsBoat())
	{
		if (!bIgnoreGroundCheck && carPosition.z <= -100.0f)
		{
			carPosition.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, carPosition.x, carPosition.y);
			//				carPosition.z += 1.0f;	// Safety margin
		}

		carPosition.z -= pModelInfo->GetBoundingBoxMin().z;

		Matrix34 tempMat;
		tempMat.Identity();
		
		fSetHeading = ( DtoR * fHeading);
		tempMat.RotateFullZ(fSetHeading);
		
		tempMat.d = carPosition;

		pNewBoat = (CBoat *)CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_SCRIPT, POPTYPE_MISSION, &tempMat, bRegisterAsNetworkObject);
		//pNewBoat->TryToMakeIntoDummy(VDM_DUMMY,true);	// Should fail instantly, but don't want people copy+pasting without this call

		Assertf(pNewBoat, "CreateCarForScript - Couldn't create a new Boat");

		if (pNewBoat)
		{
			BANK_ONLY(pNewBoat->m_CreationMethod = CVehicle::VEHICLE_CREATION_SCRIPT);
			Assert(pNewBoat->GetVehicleType()==VEHICLE_TYPE_BOAT);
			
			CScriptEntities::ClearSpaceForMissionEntity(pNewBoat);	//	should this only be done for MISSION_VEHICLEs?

			CGameWorld::Add(pNewBoat, CGameWorld::OUTSIDE);
			CPortalTracker::eProbeType probeType = GetVehicleProbeType(pNewBoat);
			pNewBoat->GetPortalTracker()->SetProbeType(probeType);
			pNewBoat->GetPortalTracker()->RequestRescanNextUpdate();
			pNewBoat->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pNewBoat->GetTransform().GetPosition()));

			pNewBoat->ActivatePhysics();

			// Set flag for whether this vehicle counts towards ambient population
			pNewBoat->m_nVehicleFlags.bCountsTowardsAmbientPopulation = CountsTowardsAmbientPopulation(pNewBoat);
		}
		vehPopDebugf2("0x%8p - Created new boat for script", pNewBoat);

#if __BANK
		if(pNewBoat)
		{
			pNewBoat->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - startTime));
		}
#endif // __BANK

		return pNewBoat;
	}
	else
	{
		Matrix34 matrix;
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(ModelIndex)));
		const float distFromCOMtoBase = -pModelInfo->GetBoundingBoxMin().z;

		matrix.Identity3x3();

		if (!bIgnoreGroundCheck && carPosition.z <= -100.0f)
		{
			carPosition.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, carPosition.x, carPosition.y);
		}

		carPosition.z += distFromCOMtoBase;
		
		fSetHeading = ( DtoR * fHeading);
		matrix.RotateFullZ(fSetHeading);
		
		matrix.d = carPosition;

		pNewVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_SCRIPT, POPTYPE_MISSION, &matrix, bRegisterAsNetworkObject);

		Assertf(pNewVehicle, "CreateCarForScript - Couldn't create a new Car");

		if (pNewVehicle)
		{
			BANK_ONLY(pNewVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_SCRIPT);
			CScriptEntities::ClearSpaceForMissionEntity(pNewVehicle);	//	should this only be done for MISSION_VEHICLEs?

			pNewVehicle->PlaceOnRoadAdjust();

			CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE);
			
			CPortalTracker *pPortalTracker = pNewVehicle->GetPortalTracker();
			if (pPortalTracker)
			{
				CPortalTracker::eProbeType probeType = GetVehicleProbeType(pNewVehicle);
				pPortalTracker->SetProbeType(probeType);
				pPortalTracker->RequestRescanNextUpdate();
				pPortalTracker->Update(VEC3V_TO_VECTOR3(pNewVehicle->GetTransform().GetPosition()));
			}

			pNewVehicle->ActivatePhysics();

			if(((CVehicleModelInfo*)pModelInfo)->GetIsTrain())
			{
				// Stop this train from expecting to be running on rails, etc
				CTrain * pTrain = (CTrain*)pNewVehicle;
				pTrain->m_nTrainFlags.bCreatedAsScriptVehicle = true;
				//pTrain->m_nTrainFlags.bMissionTrain = true;
				pTrain->m_iMissionScriptId = iMissionScriptId;

				// Trains are fixed by default; make sure this one isnt
				pTrain->SetFixedPhysics(false);

				// Disable externally controlled velocity, etc
				PHLEVEL->SetInactiveCollidesAgainstInactive(pTrain->GetCurrentPhysicsInst()->GetLevelIndex(), false);
				if(pTrain->GetCurrentPhysicsInst())
				{
					pTrain->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL, false);
				}
			}

			// Set flag for whether this vehicle counts towards ambient population
			pNewVehicle->m_nVehicleFlags.bCountsTowardsAmbientPopulation = CountsTowardsAmbientPopulation(pNewVehicle);
			
			NetworkInterface::WriteDataPopulationLog("VEHICLE_POPULATION", "CreateCarForScript", "Vehicle Created:", "%s", pNewVehicle->GetNetworkObject() ? pNewVehicle->GetNetworkObject()->GetLogName() : "No Network Object");
		}

		vehPopDebugf2("Successfully created new vehicle 0x%8p for script, type: %s", pNewVehicle, pModelInfo->GetModelName());

#if __BANK
		if(pNewVehicle)
		{
			pNewVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (u32)(sysTimer::GetTicks() - startTime));
		}
#endif // __BANK

		return pNewVehicle;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetCoordsOfScriptCar
// PURPOSE :  
// PARAMETERS :	pVehicle- the vehicle we're interested in.
//				NewX, NewY, NewZ- the coords to use.
//				bClearCarOrientation- give it a new orientation
//				bAddOffset- add the centre of mass offset
// RETURNS :	The car that was created.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::SetCoordsOfScriptCar(CVehicle *pVehicle, float NewX, float NewY, float NewZ, bool bClearCarOrientation, bool bAddOffset, bool bWarp)
{
	s32 VehicleIndex = CTheScripts::GetGUIDFromEntity(*pVehicle);

	Vector3 oldPosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

	// Used so we can make boats float
	float fOffsetFraction = 1.0f;

	if (NewZ <= INVALID_MINIMUM_WORLD_Z)
	{
		NewZ = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, NewX, NewY);
		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			float fWaterLevel;
			if(Water::GetWaterLevel(Vector3(NewX,NewY,0.0f),&fWaterLevel,false, POOL_DEPTH, REJECTIONABOVEWATER, NULL) && fWaterLevel > NewZ)
			{
				static dev_float fSubmergeFraction = 0.3f;
				fOffsetFraction = 1.0f - fSubmergeFraction;
				NewZ = fWaterLevel; 
			}
		}
	}

	if (bAddOffset)
	{
		NewZ += fOffsetFraction*pVehicle->GetDistanceFromCentreOfMassToBaseOfModel();
	}

	CScriptCars::GetStuckCars().ClearStuckFlagForCar(VehicleIndex);

	//also clear the vehicle intelligence's internal stuck state
	pVehicle->GetIntelligence()->UpdateJustBeenGivenNewCommand();

	// When a car is teleported, its suspension must be reset so that the force of the springs does not
	//	create a large Z speed

	float fSetHeading = bClearCarOrientation ? 0.0f : -10.0f;

	CPortalTracker::eProbeType probeType = CPortalTracker::PROBE_TYPE_NEAR;
	CPortalTracker* pTracker = pVehicle->GetPortalTracker();
	if (pTracker)
	{
		probeType = GetVehicleProbeType(pVehicle);
		pTracker->SetProbeType(probeType);
	}

#if !__NO_OUTPUT
	const Vec3V oldPosV = pVehicle->GetVehiclePosition();
	scriptDebugf1("%d: Script teleporting vehicle %p, of model %s, from %.1f, %.1f, %.1f to %.1f, %.1f, %.1f. %s %s%s%s",
			fwTimer::GetFrameCount(), pVehicle, pVehicle->GetModelName(),
			oldPosV.GetXf(), oldPosV.GetYf(), oldPosV.GetZf(), NewX, NewY, NewZ,
			pVehicle->IsSuperDummy() ? "SuperDummy" : (pVehicle->IsDummy() ? "Dummy" : "Real"),
			pVehicle->GetIsStatic() ? "Inactive" : "Active",
			pVehicle->HasDummyConstraint() ? " Constrained" : "",
			pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing) ? " Timesliced" : ""
			);
#endif	// !__NO_OUTPUT

	if (pVehicle->InheritsFromBoat())
	{
		pVehicle->Teleport(Vector3(NewX, NewY, NewZ), fSetHeading, false, true, true, bWarp);
		CScriptEntities::ClearSpaceForMissionEntity(pVehicle);
	}
	else
	{
		pVehicle->Teleport(Vector3(NewX, NewY, NewZ), fSetHeading, false, true, true, bWarp);

		if (bAddOffset)
		{
			pVehicle->PlaceOnRoadAdjust();
		}

		CScriptEntities::ClearSpaceForMissionEntity(pVehicle);
	}

	//clearing the nodelist wasn't enough here--needed to do a more thorough reset
	CTaskVehicleMissionBase *pCarTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(pCarTask)
	{
		pCarTask->RequestSoftReset();
	}

	CVehicleFollowRouteHelper* pFollowRoute = pVehicle->GetIntelligence()->GetFollowRouteHelperMutable();
	if(pFollowRoute)
	{
		pFollowRoute->SetAllowUseForPhysicalConstraint(false);
	}
	pVehicle->GetIntelligence()->m_BackupFollowRoute.SetAllowUseForPhysicalConstraint(false);

	pVehicle->UpdatePortalTracker();

	// If we had a dummy constraint, we probably need to remove it, since we don't
	// know if it's going to be valid at the new position. We also won't have an
	// up to date path, so we can't easily just update the constraint, but it should
	// get added back soon enough.
	if(pVehicle->HasDummyConstraint() && pVehicle->InheritsFromAutomobile() && !pVehicle->InheritsFromTrailer())
	{
		static_cast<CAutomobile*>(pVehicle)->RemoveConstraints();
	}

	// if teleporting the vehicle, then teleport the people inside too
	bool bBigMove = (VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()).Dist2(oldPosition) > 25.0f);

	for(s32 i=0;i<pVehicle->GetSeatManager()->GetMaxSeats();++i)
	{
		CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
		if (pPed) 
		{
			if (bBigMove)
			{
				pPed->SetMatrix(MAT34V_TO_MATRIX34(pVehicle->GetMatrix()), true, true, true);
			}

			if (pPed->GetPortalTracker())
			{
				pPed->GetPortalTracker()->SetProbeType(probeType);
				pPed->GetPortalTracker()->Teleport(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
				pPed->GetPortalTracker()->RequestRescanNextUpdate();
			}
		}
	}

#if 1
	fragInst* pFragInst = pVehicle->GetFragInst();
	if( pFragInst )
	{
		fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
		if( pCacheEntry )
		{
			fragHierarchyInst* pFragHierInst = pCacheEntry->GetHierInst();
			environmentCloth* pCloth = pFragHierInst->envCloth;
			if( pFragHierInst && pCloth )
			{
				pCloth->SetFlag( environmentCloth::CLOTH_IsSkipSimulation, true );
				pCloth->UpdateCloth( 0.1666667f, 1.0f );
			}
		}
	}
#endif

}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddPoliceCarOccupants
// PURPOSE :  Creates the driver and passengers for a police car.
// PARAMETERS :	pVehicle- the vehicle we're interested in.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::AddPoliceVehOccupants(CVehicle *pNewVehicle, bool bAlwaysCreatePassenger, bool bUseExistingNodes, u32 desiredModel, CIncident* pDispatchIncident/*=NULL*/, DispatchType iDispatchType)
{
#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker addPolice("AddPoliceVehicleOccupants", 24);
#endif

	Assert(pNewVehicle);
    CVehicleModelInfo* pVehModelInfo = pNewVehicle->GetVehicleModelInfo();
    Assert(pVehModelInfo);
    
    if (!pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT))
    {
        return false;
    }

    //These objects no longer exist in meta
	//if (pNewVehicle->GetModelIndex() == MI_CAR_PSTOCKADE || pNewVehicle->GetModelIndex() == MI_CAR_NSTOCKADE)
	//{
	//	if (!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
	//	{
	//		return false;
	//	}
    //
    //    for(s32 i = 1; i < 4; i++)
    //    {
    //        // only create passengers if we can register another ped
    //        if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
    //        {
    //            pNewVehicle->SetupPassenger(i, true, desiredModel);
    //        }
    //    }
	//}
	//else
    if (pNewVehicle->GetModelIndex() == MI_CAR_FBI)
	{
		if (!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
		{
			return false;
		}
		for(s32 i = 1; i < 4; i++)
		{
			// only create passengers if we can register another ped
			if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED,i, false)) // since we're not spawning them this frame, try to check for spawning more than one
			{
				CPedPopulation::SchedulePedInVehicle(pNewVehicle, i, true, desiredModel, NULL, pDispatchIncident, iDispatchType);
			}
		}
	}
	else if(pNewVehicle->GetModelIndex() == MI_CAR_CRUSADER)
	{
		if(!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
		{
			return false;
		}

		// only create passengers if we can register another? ped
		if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
		{
			CPedPopulation::SchedulePedInVehicle(pNewVehicle, 1, true, desiredModel, NULL, pDispatchIncident, iDispatchType);
		}
	}
	else if (pVehModelInfo->GetIsBoat())
	{
		//Set up a driver.
		if(!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
		{
			return false;
		}
		
		//Set up two passengers in the rear seats.
		if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
		{
			CPedPopulation::SchedulePedInVehicle(pNewVehicle, 2, true, desiredModel, NULL, pDispatchIncident, iDispatchType);
		}
		if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 2, false)) // counting the above ped
		{
			CPedPopulation::SchedulePedInVehicle(pNewVehicle, 3, true, desiredModel, NULL, pDispatchIncident, iDispatchType);
		}
	}
	else if (pVehModelInfo->GetVehicleType() == VEHICLE_TYPE_HELI)
	{
		if (!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
		{
			return false;
		}

        // only create passengers if we can register another ped
        if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
        {
			CPedPopulation::SchedulePedInVehicle(pNewVehicle, 1, true, desiredModel, NULL, pDispatchIncident, iDispatchType);
        }
	}
	else if (pVehModelInfo->GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		if (!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
		{
			return false;
		}
	}
	else
	{
		if (!pNewVehicle->SetUpDriver(bUseExistingNodes, true, desiredModel))
		{
			return false;
		}

		// only create passengers if we can register another ped
		if(bAlwaysCreatePassenger && (!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false)))
		{
			CPedPopulation::SchedulePedInVehicle(pNewVehicle, 1, true, desiredModel, NULL, pDispatchIncident, iDispatchType);
		}
	}
	 
	Assert(pNewVehicle->GetDriver());
	if(!pNewVehicle->GetDriver())
	{
		vehPopDebugf2("0x%8p - Failed to add driver to police vehicle", pNewVehicle);
		return false;
	}

	Assert(pNewVehicle->GetDriver());

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddSwatAutomobileOccupants
// PURPOSE :  Creates the driver and passengers for a swat automobile.
// PARAMETERS :	pVehicle- the vehicle we're interested in.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::AddSwatAutomobileOccupants(CVehicle* pNewVehicle, bool UNUSED_PARAM(bAlwaysCreatePassenger), bool bUseExistingNodes, u32 desiredModel, CIncident* pDispatchIncident/*=NULL*/, DispatchType iDispatchType/* = DT_Invalid */, s32 pedGroupIndex/* = -1*/)
{
	aiFatalAssertf(pNewVehicle, "NULL vehicle pointer");

	if (pNewVehicle->GetModelIndex() == MI_CAR_NOOSE || pNewVehicle->GetModelIndex() == MI_CAR_NOOSE_JEEP || pNewVehicle->GetModelIndex() == MI_CAR_NOOSE_JEEP_2 || pNewVehicle->GetModelIndex() == MI_CAR_CRUSADER)
	{
		if (!pNewVehicle->SetUpSwatDriver(bUseExistingNodes, true, desiredModel))
		{
			vehPopDebugf2("0x%8p - Failed to add driver to swat vehicle. desired model index: %d", pNewVehicle, desiredModel);
			return false;
		}

		// only create passengers if we can register another ped
		if(!NetworkInterface::IsGameInProgress() || CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, 1, false))
		{
			if(pNewVehicle->GetModelIndex() == MI_CAR_NOOSE || pNewVehicle->GetModelIndex() == MI_CAR_CRUSADER)
			{
				for( s32 iSeat = 2; iSeat <= 3; iSeat++ )
				{
					CPedPopulation::SchedulePedInVehicle(pNewVehicle, iSeat, true, desiredModel, NULL, pDispatchIncident, iDispatchType, pedGroupIndex);
				}
			}
			else if(pNewVehicle->GetModelIndex() == MI_CAR_NOOSE_JEEP || pNewVehicle->GetModelIndex() == MI_CAR_NOOSE_JEEP_2)
			{
				//Hang two passengers off the back sides of the jeep.
				CPedPopulation::SchedulePedInVehicle(pNewVehicle, 6, true, desiredModel, NULL, pDispatchIncident, iDispatchType, pedGroupIndex);
				CPedPopulation::SchedulePedInVehicle(pNewVehicle, 7, true, desiredModel, NULL, pDispatchIncident, iDispatchType, pedGroupIndex);
			}
			else
			{
				aiAssertf(0, "Unhandled vehicle model index");
				return false;
			}
		}
	}
	else
	{
		aiAssertf(0, "Unhandled vehicle model index");
		return false;
	}
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetNumRequiredPoliceCarOccupants
// PURPOSE :  returns the number of drivers and passengers that will be used to populate a vehicle of the specified model index
// PARAMETERS :	pVehicle- the vehicle we're interested in.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
u32 CVehiclePopulation::GetNumRequiredPoliceVehOccupants(u32 modelIndex)
{
    u32 numRequiredOccupants = 1;

    CVehicleModelInfo* modelinfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
    Assert(modelinfo);
    Assert(modelinfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT));

    if (modelinfo->GetVehicleType() != VEHICLE_TYPE_BIKE) // police bikes only have 1 occupant
    {
        numRequiredOccupants++;
    }

    return numRequiredOccupants;
}


/////////////////////////////////////////////////////////////////////////////////
// name:		UpdateDesertedStreets
// description:	Load the cargrp.dat file into the m_pCarGroups array
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::UpdateDesertedStreets()
{
	if(NetworkInterface::IsGameInProgress())	// url:bugstar:978444
	{
		ms_desertedStreetsMultiplier = 1.0f;
		return;
	}

	if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{		// The streets are back to normal in 30 seconds when in vehicle
		ms_desertedStreetsMultiplier += fwTimer::GetTimeStep() * (1.0f / 30.0f);
	}
	else
	{		// The streets are back to normal in 100 seconds when on foot
		ms_desertedStreetsMultiplier += fwTimer::GetTimeStep() * (1.0f / 100.0f);
	}

	float	maxVal = 1.0f;

	s32	wanted = CGameWorld::FindLocalPlayerWanted() ? CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() : WANTED_CLEAN;
	if (wanted == WANTED_LEVEL2)
	{
		maxVal = 0.9f;
	}
	else if (wanted == WANTED_LEVEL3)
	{
		maxVal = 0.8f;
	}
	else if (wanted >= WANTED_LEVEL4)
	{
		maxVal = 0.7f;
	}

	if(!ms_bDesertedStreetMultiplierLocked)
	{
		ms_desertedStreetsMultiplier = Clamp(ms_desertedStreetsMultiplier, ms_desertedStreetsMinimumMultiplier, maxVal);
	}
	else
	{
		ms_desertedStreetsMultiplier = maxVal;
	}
}

void CVehiclePopulation::ResetDesertedStreetsMultiplier()
{	
	if(!ms_bDesertedStreetMultiplierLocked)
	{	
		ms_desertedStreetsMultiplier = 1.0f;
	}
}

bool CVehiclePopulation::IsInScopeOfLocalPlayer(const Vector3 &position)
{
    if (NetworkInterface::IsGameInProgress() && Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_VEH_POP_CHECK_LOCAL_PLAYER_SCOPE", 0xbdab1399), true))
    {
        const CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
        if (pLocalPlayer)
        {
            static const float VEHICLE_SCOPE_DISTANCE_SQ = CNetObjVehicle::GetStaticScopeData().m_scopeDistance * CNetObjVehicle::GetStaticScopeData().m_scopeDistance;
            Vector3 playerPos = NetworkInterface::GetPlayerFocusPosition(*pLocalPlayer);
            Vector3 diff = position - playerPos;
            if (diff.XYMag2() < VEHICLE_SCOPE_DISTANCE_SQ)
            {
                return true;
            }

            const grcViewport* pViewport = NetworkUtils::GetNetworkPlayerViewPort(*pLocalPlayer);
            if (pViewport)
            {
                diff = position - VEC3V_TO_VECTOR3(pViewport->GetCameraPosition());
                if (diff.XYMag2() < VEHICLE_SCOPE_DISTANCE_SQ)
                {
                    return true;
                }
            }
        }
        return false;
    }
    else
    {
        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////////
// name:		IsNetworkTurnToCreateVehicles
// description:	Returns whether it is the local players turn to create some ambient
//              cars at the given position
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::IsNetworkTurnToCreateVehicleAtPos(const Vector3 &position)
{
    static const float VEHICLE_CREATION_PRIORITY_DISTANCE = 350.0f;

    bool isPlayerTurn = false;

    const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
    u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(position, VEHICLE_CREATION_PRIORITY_DISTANCE, closestPlayers, false);

#if ENABLE_NETWORK_LOGGING 
    LogPlayersSharingTurn(closestPlayers, numPlayersInRange, position);
#endif

    if(numPlayersInRange == 0)
    {
        isPlayerTurn = true;
    }
    else
    {
        unsigned localPlayerTurn = 0;

        for(u32 playerIndex = 0; playerIndex < numPlayersInRange; playerIndex++)
        {
            const netPlayer *player = closestPlayers[playerIndex];

            if(AssertVerify(player))
            {
                if (NetworkInterface::GetLocalPlayer()->GetRlGamerId() < player->GetRlGamerId())
                {
                    localPlayerTurn++;
                }
            }
        }

        // players within range are given turns to create vehicles, highest peer ID first.
        // the game waits for a short period between changing turns, to help prevent
        // cars being created on top of one another
        const unsigned numTurns = numPlayersInRange + 1;
        
        const unsigned DURATION_OF_TURN       = 400;
        const unsigned DURATION_BETWEEN_TURNS = 200;

        unsigned currentTime    = NetworkInterface::GetNetworkTime();
        unsigned timeWithinTurn = currentTime % (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);
        currentTime /= (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);

        unsigned currentTurn = currentTime % (numTurns);

        if((currentTurn == localPlayerTurn) && (timeWithinTurn <= DURATION_OF_TURN))
        {
            isPlayerTurn = true;
        }
        else
        {
            isPlayerTurn = false;
        }
    }

    return isPlayerTurn;
}

#if ENABLE_NETWORK_LOGGING

void CVehiclePopulation::LogPlayersSharingTurn(const netPlayer** players, int playerCount, const Vector3 &position)
{
    atFixedBitSet<MAX_NUM_ACTIVE_PLAYERS, u32> playersTakingTurns;
    for (int i = 0; i < playerCount && playerCount < MAX_NUM_ACTIVE_PLAYERS; ++i)
    {   
        if (players[i])
        {
            playersTakingTurns.Set(players[i]->GetActivePlayerIndex(), true);
        }
    }

    if (!ms_PlayersTakingTurn.IsEqual(playersTakingTurns))
    {
        ms_PlayersTakingTurn = playersTakingTurns;
        netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
        NetworkLogUtils::WriteLogEvent(log, "PLAYERS_TAKING_TURNS", "Num participants: %d", playerCount);
        NetworkLogUtils::WriteLogEvent(log, "Last turn position", "%.2f, %.2f, %.2f", position.x, position.y, position.z);
        for (int i = 0; i < playerCount; ++i)
        {
            if (players[i])
            {
                log.WriteDataValue("Turn participant", "%s", players[i]->GetLogName());
            }
        }
    }
}

#endif // ENABLE_NETWORK_LOGGING

//returns a value that reflects our current local/total vehicle populations
//calculates ratio of desired/total vehicles for total and local population
//if only one of these is < fLimit we calculate a multiplier
// <1.0f means we don't have enough vehicles and are limited locally in the number we can create
// >1.0f means we have enough vehicles and have excess local vehicles that we aren't using
float CVehiclePopulation::CalculateVehicleShareMultiplier()
{
	float fMultiplier = 1.0f;
	if(NetworkInterface::IsGameInProgress())
	{
		s32 fDesiredNumVehicles = GetDesiredNumberAmbientVehicles_Cached();
		u32 fDesiredNumLocalVehicles = CNetworkObjectPopulationMgr::GetLocalTargetNumVehicles();
		if(fDesiredNumLocalVehicles > 0 && fDesiredNumVehicles > 0 && fDesiredNumLocalVehicles < fDesiredNumVehicles)
		{
			float fTotalVehicleRatio = (float)GetTotalAmbientMovingVehsOnMap() / fDesiredNumVehicles;
			float fLocalVehicleRatio = (float)CNetworkObjectPopulationMgr::GetNumCachedLocalVehicles() / fDesiredNumLocalVehicles;
			float fLimit = 0.95f;
			//only do this if we're above the limit on only one ratio
			if((fTotalVehicleRatio > fLimit) ^ (fLocalVehicleRatio > fLimit))
			{
				//this gives us a value that represents how much we're limited either locally (< 1.0f)
				//or how much we've got space in our local pool (> 1.0f)
				if(fTotalVehicleRatio > fLimit)
				{
					fMultiplier = fTotalVehicleRatio / fLocalVehicleRatio;
				}
				
			}
		}
	}

	return fMultiplier;
}

/////////////////////////////////////////////////////////////////////////////////
// name:		PlayerDischargedWeapon
// description:	Player has fires a gun and the population should be reduced.
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::MakeStreetsALittleMoreDeserted()
{
	if(!ms_bDesertedStreetMultiplierLocked)
	{
		ms_desertedStreetsMultiplier -= 0.1f;
		ms_desertedStreetsMultiplier = Clamp(ms_desertedStreetsMultiplier, ms_desertedStreetsMinimumMultiplier, 1.0f);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RegisterInterestingVehicle()
// PURPOSE :	Marks this vehicle as being interesting. Shouldn't immediately be removed.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::RegisterInterestingVehicle(CVehicle* pVeh)
{
	Assert(pVeh);

	// we renew the timer even if we set the same vehicle
	if (NetworkInterface::IsGameInProgress())
		ms_timeToClearInterestingVehicle = fwTimer::GetTimeInMilliseconds() + ms_clearInterestingVehicleDelayMP;
	else
		ms_timeToClearInterestingVehicle = fwTimer::GetTimeInMilliseconds() + ms_clearInterestingVehicleDelaySP;

	if (ms_pInterestingVehicle == pVeh)
	{
		return;
	}
	ms_pInterestingVehicle = pVeh;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsVehicleInteresting()
// PURPOSE :	Returns true if this vehicle is interesting (and should not be removed)
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::IsVehicleInteresting(const CVehicle *const pVeh)
{
	return (ms_pInterestingVehicle == pVeh);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ClearInterestingVehicles()
// PURPOSE :
// PARAMETERS : force will skip any timer or proximity checks
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::ClearInterestingVehicles(bool force)
{
	if (ms_pInterestingVehicle)
	{
		if (!force && ms_pInterestingVehicle->GetStatus() != STATUS_WRECKED)
		{
			if (fwTimer::GetTimeInMilliseconds() <= ms_timeToClearInterestingVehicle)
				return;

			CPed* ped = CGameWorld::FindFollowPlayer();
			if (ped)
			{
				Vec3V pedPos = ped->GetMatrixRef().GetCol3();
				Vec3V vehPos = ms_pInterestingVehicle->GetVehiclePosition();

				float fProximity = NetworkInterface::IsGameInProgress() ? ms_clearInterestingVehicleProximityMP : ms_clearInterestingVehicleProximitySP;
				if (DistSquared(pedPos, vehPos).Getf() <= (fProximity * fProximity))
					return;
			}
		}

		ms_pInterestingVehicle = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HasInterestingVehicle()
// PURPOSE :
// PARAMETERS :	None
// RETURNS :	true if we track an interesting vehicle, false otherwise
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::HasInterestingVehicle()
{
	return ms_pInterestingVehicle != NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HasInterestingVehicleStreamedIn()
// PURPOSE :
// PARAMETERS :	None
// RETURNS :	true if there's an interesting vehicle and all its mods are streamed in
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::HasInterestingVehicleStreamedIn()
{
    if (!ms_pInterestingVehicle)
        return true;

	CVehicleVariationInstance& variation = ms_pInterestingVehicle->GetVariationInstance();
	return variation.HaveModsStreamedIn(ms_pInterestingVehicle);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetInterestingVehicleClearTime()
// PURPOSE :
// PARAMETERS :	None
// RETURNS :	time in ms when the interesting vehicle will be cleared
/////////////////////////////////////////////////////////////////////////////////
u32 CVehiclePopulation::GetInterestingVehicleClearTime()
{
	return ms_timeToClearInterestingVehicle;
}

////////////////////////////////////////////////////////////////
// FUNCTION :	SetInterestingVehicle
// PURPOSE :
// PARAMETERS :	new vehicle to set as the interesting one
// RETURNS :	old interesting vehicle
/////////////////////////////////////////////////////////////////////////////////
CVehicle* CVehiclePopulation::SetInterestingVehicle(CVehicle* newVeh)
{
	CVehicle* oldVeh = ms_pInterestingVehicle;
	ms_pInterestingVehicle = newVeh;
	return oldVeh;
}

////////////////////////////////////////////////////////////////
// FUNCTION :	GetInterestingVehicle
// PURPOSE :
// PARAMETERS :	none
// RETURNS :	interesting vehicle
/////////////////////////////////////////////////////////////////////////////////
CVehicle* CVehiclePopulation::GetInterestingVehicle()
{
	return ms_pInterestingVehicle.Get();
}

#if __BANK

// FUNCTION : GetFailStats
// PURPOSE : Returns total number of vehicle gen failures, and formats a string containing them
// PARAMETERS : filsString must point to a text buffer large enough to accommodate failure string

s32 CVehiclePopulation::GetFailString(char * failString)
{
	failString[0] = 0;

	s32 failCounts[] =
	{
		m_populationFailureData.m_numNoCreationPosFound,
		m_populationFailureData.m_numNoLinkBetweenNodes,
		m_populationFailureData.m_numChosenModelNotLoaded,
		m_populationFailureData.m_numSpawnLocationInViewFrustum,
		m_populationFailureData.m_numSingleTrackRejection,
		m_populationFailureData.m_numTrajectoryOfMarkedVehicleRejection,
		m_populationFailureData.m_numNoVehModelFound,
		m_populationFailureData.m_numNetworkPopulationFail,
		m_populationFailureData.m_numVehsGoingAgainstTrafficFlow,
		m_populationFailureData.m_numDeadEndsChosen,
		m_populationFailureData.m_numNoPathNodesAvailable,
		m_populationFailureData.m_numGroundPlacementFail,
		m_populationFailureData.m_numGroundPlacementSpecialFail,
		m_populationFailureData.m_numNetworkVisibilityFail,
		m_populationFailureData.m_numPointWithinCreationDistFail,
		m_populationFailureData.m_numPointWithinCreationDistSpecialFail,
		m_populationFailureData.m_numRelativeMovementFail,
		m_populationFailureData.m_numBoundingBoxFail,
		m_populationFailureData.m_numBoundingBoxSpecialFail,
		m_populationFailureData.m_numVehicleCreateFail,
		m_populationFailureData.m_numDriverAddFail,
		m_populationFailureData.m_numFallbackPedNotAvailable,
		m_populationFailureData.m_numPopulationIsFull,
		m_populationFailureData.m_numNodesSwitchedOff,
		m_populationFailureData.m_numIdenticalModelTooClose,
		m_populationFailureData.m_numNotNetworkTurnToCreateVehicleAtPos,
		m_populationFailureData.m_sameVehicleModelAsLastTime,
		m_populationFailureData.m_numChosenBoatModelNotLoaded,
		m_populationFailureData.m_numRandomBoatsNotAllowed,
		m_populationFailureData.m_numBoatAlreadyHere,
		m_populationFailureData.m_numWaterLevelFuckup,
		m_populationFailureData.m_numLinkDirectionInvalid,
        m_populationFailureData.m_numOutOfLocalPlayerScope,
		-9999
	};

	static const char * failDescs[] =
	{
		"CreationPos",
		"NoLinks",
		"ModelNotLoaded",
		"InFrustum",
		"SingleTrack",
		"Trajectory",
		"ModelNotFound",
		"NetworkPop",
		"AgainstTraffic",
		"DeadEndsChosen",
		"NoPathNodes",
		"GroundPlacement",
		"GroundPlacementSpecial",
		"NetworkVis",
		"PointWithinDist",
		"PointWithinDistSpecial",
		"RelativeVel",
		"BBox",
		"BBoxSpecial",
		"CreateVeh",
		"AddDriver",
		"FallbackPed",
		"PopFull",
		"NodesOff",
		"ModelTooClose",
		"NotNetworkTurn",
		"SameVehAsLast",
		"BoatModelNotLoaded",
		"RandomBoatsNotAllowed",
		"BoatAlreadyHere",
		"WaterLevelFuckup",
		"LinkDirectionInvalid",
		"LocalPlayerOutOfScope"
	};

	char text[128];

	s32 iTotalFails = 0;
	s32 i=0;
	while(failCounts[i] != -9999)
	{
		if(failCounts[i] > 0)
		{
			iTotalFails += failCounts[i];

			sprintf(text, "%s(%i) ", failDescs[i], failCounts[i]);
			strcat(failString, text);
		}
		i++;
	}

	return iTotalFails;
}

void CVehiclePopulation::ClearCarsFromArea()
{
	Vector3 vAreaPos = ms_vClearAreaCenterPos;
	if (vAreaPos.IsZero())
	{
		CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		if (!pLocalPlayer)
			return;

		vAreaPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());
	}

	CGameWorld::ClearCarsFromArea(vAreaPos, ms_fClearAreaRadius, ms_bClearAreaCheckInteresting, ms_bClearAreaLeaveCarGenCars, ms_bClearAreaCheckFrustum,
		ms_bClearAreaWrecked, ms_bClearAreaAbandoned, ms_bClearAreaAllowCopRemovealWithWantedLevel, ms_bClearAreaEngineOnFire);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateDebugDrawing()
// PURPOSE :	Does most of the vehicle population debug drawing calls.  To try
//				to keep the vehicle population process function cleaner.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::UpdateDebugDrawing()
{
	bool bDisplayPopGenShapeVM = false;
#if __BANK
	if(ms_displayAddCullZonesOnVectorMap)
		bDisplayPopGenShapeVM = true;
#endif

	Vector3 vPopGenCenterStart = ms_popGenKeyholeShape.GetCenter();

	if(ms_displayAddCullZonesInWorld)
	{
		Vector3 vEndPos(vPopGenCenterStart.GetX(), vPopGenCenterStart.GetY(), 0.0f);

		WorldProbe::CShapeTestHitPoint probeResult;
		WorldProbe::CShapeTestResults probeResults(probeResult);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vPopGenCenterStart, vEndPos);
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);

		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST) && probeResults.GetNumHits() > 0)
		{
			vPopGenCenterStart.SetZ(probeResults[0].GetHitPosition().GetZ() + ms_addCullZoneHeightOffset);
		}
	}

	if(ms_bUseTrafficDensityModifier)
	{
		s32 iNumAmbientVehicles = CVehiclePopulation::GetTotalAmbientMovingVehsOnMap() - ms_numAmbientCarsOnHighway;
		if(iNumAmbientVehicles > ms_iMinAmbientVehiclesForThrottle)
		{
			if( CVehiclePopulation::ms_numStationaryAmbientCars / (float)iNumAmbientVehicles > ms_fMinRatioForTrafficThrottle)
			{
				CAmbientPopulationVehData *pVehData = ms_aAmbientVehicleData.GetElements();
				u32 vehDataCount = ms_aAmbientVehicleData.GetCount();

				if(ms_displayTrafficCenterInWorld)
				{
					Vec3V vDebugLoc = ms_stationaryVehicleCenter;
					vDebugLoc.SetZf(vDebugLoc.GetZf() + 15.0f);
					grcDebugDraw::Circle(RCC_VECTOR3(vDebugLoc), 2.0f, Color_purple, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), false, true);
					grcDebugDraw::Circle(RCC_VECTOR3(vDebugLoc), ms_fMinDistanceForTrafficThrottle, Color_yellow, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
					grcDebugDraw::Circle(RCC_VECTOR3(vDebugLoc), ms_fMaxDistanceForTrafficThrottle, Color_red, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));

					//don't include cars on highways as they won't be contributing to traffic issues
					s32 iNumAmbientVehicles = CVehiclePopulation::GetTotalAmbientMovingVehsOnMap() - CVehiclePopulation::ms_numAmbientCarsOnHighway;
					float fProportionAmbientVehStopped = 1.0f;
					if(iNumAmbientVehicles > ms_iMinAmbientVehiclesForThrottle)
					{
						fProportionAmbientVehStopped = CVehiclePopulation::ms_numStationaryAmbientCars / (float)iNumAmbientVehicles;
					}
					char buf[64];
					formatf(buf, "Ratio:%f", fProportionAmbientVehStopped);
					grcDebugDraw::Text(VEC3V_TO_VECTOR3(vDebugLoc), Color_red, buf);
				}

				if(ms_displayTrafficCenterOnVM)
				{
					Vec3V vDebugLoc = ms_stationaryVehicleCenter;
					vDebugLoc.SetZf(vDebugLoc.GetZf() + 15.0f);
					CVectorMap::DrawCircle(RCC_VECTOR3(vDebugLoc), 2.0f, Color_purple, true);
					CVectorMap::DrawCircle(RCC_VECTOR3(vDebugLoc), ms_fMinDistanceForTrafficThrottle, Color_yellow, false);
					CVectorMap::DrawCircle(RCC_VECTOR3(vDebugLoc), ms_fMaxDistanceForTrafficThrottle, Color_red, false);

					for(int i = 0; i < vehDataCount; ++i)
					{
						CAmbientPopulationVehData &pVeh = pVehData[i];
						CVectorMap::DrawCircle(RCC_VECTOR3(pVeh.m_Pos), 2.0f, pVeh.m_fSpeed < 4.0f ? Color_red : Color_green, true);				
					}

					for(int i = 0; i < ms_numGenerationLinks; ++i)
					{
						CVehGenLink & vehGenLink = ms_aGenerationLinks[i];
						if(vehGenLink.m_bIsActive && vehGenLink.m_iDensity != vehGenLink.m_iInitialDensity && vehGenLink.m_iInitialDensity <= 10)
						{
							const Vector3 vMid((float) ((vehGenLink.m_iMinX + vehGenLink.m_iMaxX) >> 1),(float) ((vehGenLink.m_iMinY + vehGenLink.m_iMaxY) >> 1),0.0f );
							CVectorMap::DrawCircle(vMid, 2.0f, Color_blue, true);	
						}
					}
				}
			}
		}
	}

	if(CDebugScene::bDisplayVehGenLinksOnVM)
	{
		//render all physical players
		unsigned                 numPhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *allPhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
		float fVisibleCreationDist = GetVisibleCloneCreationDistance();
		float fOffscreenCreationDistance = rage::Lerp(ms_CloneSpawnValidationMultiplier, ms_NetworkOffscreenCreationDistMax, ms_NetworkOffscreenCreationDistMin);

		grcViewport *pPlayerViewport = gVpMan.GetGameViewport() ? &gVpMan.GetGameViewport()->GetNonConstGrcViewport() : NULL;
		if(pPlayerViewport)
		{
			const Vector3 vForward = RCC_MATRIX34(pPlayerViewport->GetCameraMtx()).c;
			float fLookAtAngle = rage::Atan2f(vForward.x, vForward.y) - DtoR*(90.0f);

			for(unsigned index = 0; index < numPhysicalPlayers; index++)
			{
				const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
				CPed* playerPed = player->GetPlayerPed();
				if(playerPed)
				{
					CNetObjPlayer* pNetObj = ((CNetObjPlayer*)playerPed->GetNetworkObject());
					grcViewport *pViewport = pNetObj->GetViewport();
					if(pViewport)
					{
						fVisibleCreationDist *= pNetObj->GetExtendedScopeFOVMultiplier(pViewport->GetFOVY());
						NetworkColours::NetworkColour colour = NetworkColours::GetDefaultPlayerColour(player->GetPhysicalPlayerIndex());
						Color32 Colour = NetworkColours::GetNetworkColour(colour);
						CVectorMap::DrawRemotePlayerCameraCloseVisibleBounds(RCC_MATRIX34(pViewport->GetCameraMtx()), 
							fOffscreenCreationDistance, fVisibleCreationDist, pViewport->GetTanHFOV(), Colour, -fLookAtAngle);
					}
				}			
			}
		}

		//these are the links we can actually create vehicles on
		for(int i = 0; i < ms_iNumActiveLinks; ++i)
		{
			CVehGenLink & vehGenLink = ms_aGenerationLinks[ms_aActiveLinks[i]];
			const Vector3 vMid((float) ((vehGenLink.m_iMinX + vehGenLink.m_iMaxX) >> 1),
							   (float) ((vehGenLink.m_iMinY + vehGenLink.m_iMaxY) >> 1),
							   (float) ((vehGenLink.m_iMinZ + vehGenLink.m_iMaxZ) >> 1));
			Color32 col = NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vMid, 1.0f, fVisibleCreationDist, 
								fOffscreenCreationDistance, NULL, ms_NetworkUsePlayerPrediction, true) ? Color_red : Color_green;
			CVectorMap::DrawCircle(vMid, 2.0f, col, true);	
		}

		//these are the links that are active for consideration
		for(int i = 0; i < ms_numGenerationLinks; ++i)
		{
			CVehGenLink & vehGenLink = ms_aGenerationLinks[i];
			if(vehGenLink.m_bIsActive)
			{
				bool found = false;
				for(int j = 0; j < ms_iNumActiveLinks;++j)
				{
					if(ms_aActiveLinks[j] == i)
					{
						found = true;
						break;
					}
				}
				if(!found)
				{
					const Vector3 vMid((float) ((vehGenLink.m_iMinX + vehGenLink.m_iMaxX) >> 1),
									   (float) ((vehGenLink.m_iMinY + vehGenLink.m_iMaxY) >> 1),
									   (float) ((vehGenLink.m_iMinZ + vehGenLink.m_iMaxZ) >> 1));
					//oh man, I bet this is efficient
					Color32 col = NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vMid, 1.0f, fVisibleCreationDist, 
										fOffscreenCreationDistance, NULL, ms_NetworkUsePlayerPrediction, true) ? Color_orange : Color_blue;
					CVectorMap::DrawCircle(vMid, 2.0f, col, true);	
				}
			}
		}
	}

	const Vector3 vPopGenCenterEnd = vPopGenCenterStart + Vector3(0.0f, 0.0f, 10.0f);

	const float rangeScale = GetPopulationRangeScale();

	if(ms_displayAddCullZonesInWorld)
	{
		grcDebugDraw::Cylinder(RCC_VEC3V(vPopGenCenterStart), RCC_VEC3V(vPopGenCenterEnd), GetCullRange(rangeScale), Color_red, false, 1, 32);
		grcDebugDraw::Cylinder(RCC_VEC3V(vPopGenCenterStart), RCC_VEC3V(vPopGenCenterEnd), GetCullRangeOffScreen(rangeScale), Color_orange, false, 1, 32);
		grcDebugDraw::Cylinder(RCC_VEC3V(vPopGenCenterStart), RCC_VEC3V(vPopGenCenterEnd), ms_fCalculatedSpawnFrustumNearDist, Color_green, false, 1, 28);
	}

	if(bDisplayPopGenShapeVM || ms_displayAddCullZonesInWorld)
	{
		ms_popGenKeyholeShape.Draw(ms_displayAddCullZonesInWorld, bDisplayPopGenShapeVM, vPopGenCenterEnd, 1.0f);

		float scanNodeDist = GetCreationDistance() + 20.0f;
		static const float maximumNodeDist = 500.0f;
		scanNodeDist = Min(scanNodeDist, maximumNodeDist);
		Vector3 vGatherMin = ms_popGenKeyholeShape.GetCenter() - Vector3(scanNodeDist, scanNodeDist, scanNodeDist);
		Vector3 vGatherMax = ms_popGenKeyholeShape.GetCenter() + Vector3(scanNodeDist, scanNodeDist, scanNodeDist);

		if(ms_displayAddCullZonesInWorld)
		{
			grcDebugDraw::BoxAxisAligned(RCC_VEC3V(vGatherMin), RCC_VEC3V(vGatherMax), Color_salmon, false);
		}
	}

	if (ms_displaySpawnCategoryGrid)
	{
		float scanNodeDist = GetCreationDistance() + 20.0f;
		float gridSpacing = scanNodeDist * 0.1f;
		ms_popGenKeyholeShape.DrawTestCategories(scanNodeDist, gridSpacing);
	}

	if(bDisplayPopGenShapeVM && ms_vehiclePopGenLookAhead)
	{
		CVectorMap::DrawLine(ms_lastPopCenter.m_centre, ms_lastPopCenter.m_centre + ms_lastPopCenter.m_centreVelocity, Color_red, Color_red);
	}

	if(bDisplayPopGenShapeVM)
	{
		Vector3 vPos(ms_popGenKeyholeShape.GetCenter().x, ms_popGenKeyholeShape.GetCenter().y, FindPlayerPed()->GetTransform().GetPosition().GetZf());
		CVectorMap::DrawCircle(vPos, GetCullRange(rangeScale), Color32(1.0f, 0.0f, 0.0f, 0.5f), false);
		CVectorMap::DrawCircle(vPos, GetCullRangeOffScreen(rangeScale), Color32(1.0f, 0.65f, 0.0f, 0.5f), false);
	}

	if(ms_bHeavyDutyVehGenDebugging)
	{
		HeavyDutyVehGenDebugging();
	}
	if(ms_bDisplaySpawnHistory)
	{
		RenderSpawnHistory();
	}
	if(ms_bRenderMarkupSpheres)
	{
		RenderMarkupSpheres();
	}

#if __BANK
	CPopCtrl popCtrl = CPedPopulation::GetPopulationControlPointInfo( ms_useTempPopControlPointThisFrame ? &ms_tmpPopOrigin : NULL);

	if(CDebugScene::bDisplayVehicleCreationPathsOnVMap || CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap || CDebugScene::bDisplayVehicleCreationPathsInWorld)
	{
		float fThicknessScale = CalculatePopulationSpawnThicknessScale();

		// Draw the new generation zones.
		if(CDebugScene::bDisplayVehicleCreationPathsOnVMap || CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap)
		{
			// Draw the inner band (in green).
			const Color32 innerBandCol(0,128,0,128);
			CVectorMap::DrawCircle(popCtrl.m_centre, ms_popGenKeyholeShapeInnerBandRadius * rangeScale, innerBandCol, false);
			CVectorMap::DrawCircle(popCtrl.m_centre, (ms_popGenKeyholeShapeInnerBandRadius * rangeScale) + (ms_popGenKeyholeInnerShapeThickness * fThicknessScale), innerBandCol, false);

			// Draw the outer band (in red).
			const Color32 outerBandCol(128,0,0,128);
			CVectorMap::DrawCircle(popCtrl.m_centre, ms_popGenKeyholeShapeOuterBandRadius * rangeScale, outerBandCol, false);
			CVectorMap::DrawCircle(popCtrl.m_centre, (ms_popGenKeyholeShapeOuterBandRadius * rangeScale) + (ms_popGenKeyholeOuterShapeThickness * fThicknessScale), outerBandCol, false);

			// Draw the sidewalls (in blue).
			const Color32 sidewallCol(0,0,128,128);

			Vector2 centre;
			popCtrl.m_centre.GetVector2XY(centre);
			const float	st0	= rage::Sinf(popCtrl.m_baseHeading);
			const float	ct0	= rage::Cosf(popCtrl.m_baseHeading);
			Vector2 dir(-st0, ct0);
			Assert(dir.Mag2() > 0.1f);
			dir.Normalize();

			const float tanHFov				= popCtrl.m_tanHFov;
			const float HalfHFovAngle		= rage::Atan2f(tanHFov, 1.0f);
			const float	sinHalfHFovAngle	= rage::Sinf(HalfHFovAngle);
			const float offsetDist			= ms_popGenKeyholeSideWallThickness / sinHalfHFovAngle;
			

			// Draw the FOV edges using (centre, dir, and tanHFov).
			{
				Vector3	centreToUse(popCtrl.m_centre);
				centreToUse.z = 0;

				const Vector3	vPos	(centreToUse.x, centreToUse.y, 0.0f);
				const Vector3	vRight	(dir.y, -dir.x, 0.0f);
				const Vector3	vForward(dir.x, dir.y, 0.0f);
				const float		radius	= (ms_popGenKeyholeShapeOuterBandRadius * rangeScale) + (ms_popGenKeyholeOuterShapeThickness * fThicknessScale);
				const Vector3	vRightCorner	= vPos + (vForward * radius) + (vRight * tanHFov * radius);
				const Vector3	vleftCorner		= vPos + (vForward * radius) - (vRight * tanHFov * radius);
				CVectorMap::DrawLine(vPos, vRightCorner, sidewallCol, sidewallCol);
				CVectorMap::DrawLine(vPos, vleftCorner, sidewallCol, sidewallCol);
			}

			// Draw the FOV edges using (centre - (dir * offsetDist), dir, and tanHFov).
			{
				Vector3	centreToUse(popCtrl.m_centre);
				centreToUse.x -= (dir.x * offsetDist);
				centreToUse.y -= (dir.y * offsetDist);
				centreToUse.z = 0;

				const Vector3	vPos	(centreToUse.x, centreToUse.y, 0.0f);
				const Vector3	vRight	(dir.y, -dir.x, 0.0f);
				const Vector3	vForward(dir.x, dir.y, 0.0f);
				const float		radius	= (ms_popGenKeyholeShapeOuterBandRadius * rangeScale) + (ms_popGenKeyholeOuterShapeThickness * fThicknessScale);
				const Vector3	vRightCorner	= vPos + (vForward * radius) + (vRight * tanHFov * radius);
				const Vector3	vleftCorner		= vPos + (vForward * radius) - (vRight * tanHFov * radius);
				CVectorMap::DrawLine(vPos, vRightCorner, sidewallCol, sidewallCol);
				CVectorMap::DrawLine(vPos, vleftCorner, sidewallCol, sidewallCol);
			}
		}

		// Go through all the new generation links and draw them on the vector map.
		char buf[64] = {0};
		for(u32 i =0; i < ms_numGenerationLinks; ++i)
		{
			Vector3 node0Pos;
			ThePaths.FindNodePointer(ms_aGenerationLinks[i].m_nodeA)->GetCoors(node0Pos);
			Vector3 node1Pos;
			ThePaths.FindNodePointer(ms_aGenerationLinks[i].m_nodeB)->GetCoors(node1Pos);

			bool isActive = ms_aGenerationLinks[i].m_bUpToDate;
			Color32 col(0,0,0,128);

			if(isActive)
			{
				switch(ms_aGenerationLinks[i].m_iCategory)
				{
				case CPopGenShape::GC_KeyHoleInnerBand_off:
					isActive = false;
					col.Set(32,128,32,128);
					break;
				case CPopGenShape::GC_KeyHoleOuterBand_off:
					isActive = false;
					col.Set(128,32,32,128);
					break;
				case CPopGenShape::GC_KeyHoleInnerBand_on:
					isActive = true;
					col.Set(0,255,0,255);
					break;
				case CPopGenShape::GC_KeyHoleOuterBand_on:
					isActive = true;
					col.Set(255,0,0,255);
					break;
				case CPopGenShape::GC_KeyHoleSideWall_on:
					isActive = true;
					col.Set(0,0,255,255);
					break;
				case CPopGenShape::GC_InFOV_usableIfOccluded:
					isActive = false;
					col.Set(128,32,128,128);
					break;
				case CPopGenShape::GC_InFOV_on:
					isActive = true;
					col.Set(255,128,128,255);
					break;
				default:
					Assert(ms_aGenerationLinks[i].m_iCategory == CPopGenShape::GC_Off);
					isActive = false;
					break;
				}
			}

			if(isActive)
			{
				if(CDebugScene::bDisplayVehicleCreationPathsOnVMap)
				{
					CVectorMap::DrawLineThick(node0Pos, node1Pos, col, col);
				}

				if(CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap)
				{
					int colourBase = Clamp( static_cast<int>(255.0f * (ms_aGenerationLinks[i].GetNearbyVehiclesCount() / ms_popGenDensityMaxExpectedVehsNearLink)), 0, 255);
					Color32	densityColour(colourBase, colourBase, colourBase, 255);
					CVectorMap::DrawLine(node0Pos, node1Pos, densityColour, densityColour);
				}
			}
			else
			{
				if(CDebugScene::bDisplayVehicleCreationPathsOnVMap)
				{
					CVectorMap::DrawLine(node0Pos, node1Pos, col, col);
				}
			}

			// render links in world
			if (CDebugScene::bDisplayVehicleCreationPathsInWorld)
			{
				//grcDebugDraw::SetDoDebugZTest(false);
				node0Pos.z += 2.f;
				formatf(buf, "%s - %d - %d", isActive ? "active" : "inactive", ms_aGenerationLinks[i].m_nodeA.GetRegion(), ms_aGenerationLinks[i].m_nodeA.GetIndex());
				grcDebugDraw::Text(node0Pos, col, buf);
				node0Pos.z -= 1.f;
				node1Pos.z += 1.f;
				grcDebugDraw::Sphere(node0Pos, 1.f, col, false);
				grcDebugDraw::Line(node0Pos, node1Pos, col, col);
			}
		}
	}
// END DEBUG!!

#if __BANK
	if(CVehicleDeformation::ms_bDisplayTexCacheStats)	
	{
		CVehicleDeformation::DisplayTexCacheStats();
	}

	DisplayMessageHistory();
#endif

#if VEHICLE_DEFORMATION_TIMING
	VD_DisplayTiming();
#endif // VEHICLE_DEFORMATION_TIMING

#endif // __DEV

#if __BANK
	if((fwTimer::GetTimeInMilliseconds() - m_populationFailureData.m_counterTimer) >= 4000)
	{
		m_populationFailureData.Reset(fwTimer::GetTimeInMilliseconds());
	}

	if(ms_bDisplayVehicleSpacings)
	{
		float fPopScheduleMult, fTotalRandomVehDensityMult, fRangeScaleMult, fWantedLevelMult;
		grcDebugDraw::AddDebugOutput("Overall spacing multiplier = %.1f", GetVehicleSpacingsMultiplier(rangeScale, &fPopScheduleMult, &fTotalRandomVehDensityMult, &fRangeScaleMult, &fWantedLevelMult));
		grcDebugDraw::AddDebugOutput("Multipliers: (Pop Schedule = %.1f, Total Vehicle-Density = %.1f, )", fPopScheduleMult, fTotalRandomVehDensityMult);

		for(s32 d=0; d<NUM_LINK_DENSITIES; d++)
		{
			grcDebugDraw::AddDebugOutput("Density %i) initial = %.1f, current = %.1f", d, ms_fDefaultVehiclesSpacing[d], ms_fCurrentVehiclesSpacing[d]);
		}
	}

	if(ms_bDisplayVehicleReusePool)
	{
		grcDebugDraw::AddDebugOutput("Vehicle Reuse Pool:");
		for(int i = 0; i < ms_vehicleReusePool.GetCount(); i++)
		{
			if(ms_vehicleReusePool[i].pVehicle)
			{
				grcDebugDraw::AddDebugOutput("%u: %s - Age %u", i, ms_vehicleReusePool[i].pVehicle->GetModelName(), fwTimer::GetFrameCount() - ms_vehicleReusePool[i].uFrameCreated);
			}			
		}
		
	}
#endif
}

// NAME : GetPopulationCounts
// PURPOSE : Returns division of vehicles in various categories, central place for this
void CVehiclePopulation::GetPopulationCounts(
	int & iVehiclePoolSize,
	int & iTotalNumVehicles,
	int & iNumMission,
	int & iNumParked,
	int & iNumAmbient,
	int & iNumDesiredAmbient,
	int & iNumReal,
	int & iNumRealRandomTraffic,
	int & iNumDummy,
	int & iNumSuperDummy,
	int & iNumRandomGenerated,
	int & iNumPathGenerated,
	int & iNumCargenGenerated,
	int & iNumReused,
	int & iNumInReusePool)
{
	iTotalNumVehicles = 0;
	iNumMission = 0;
	iNumParked = 0;
	iNumAmbient = 0;
	iNumDesiredAmbient = 0;
	iNumReal = 0;
	iNumRealRandomTraffic = 0;
	iNumDummy = 0;
	iNumSuperDummy = 0;
	iNumRandomGenerated = 0;
	iNumPathGenerated = 0;
	iNumCargenGenerated = 0;
	iNumReused = 0;
	iNumInReusePool = 0;

	if (!CVehicle::GetPool())
		return;

	s32 i = (s32) CVehicle::GetPool()->GetSize();
	while(i--)
	{
		CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(i);
		if (pVehicle)
		{
			iTotalNumVehicles++;
			if(pVehicle->GetIsInReusePool()) 
			{
				// in reuse pool
				iNumInReusePool++;
				continue; // don't count vehicles in the reuse pool towards the population counts
			}

			if (pVehicle->PopTypeIsMission())
			{
				if (pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation)
					iNumParked++;
				else
					iNumMission++;
			}
			else
			{
				if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
					iNumParked++;
				else
					iNumAmbient++;
			}

			switch(pVehicle->GetVehicleAiLod().GetDummyMode())
			{
				case VDM_REAL:
				{
					iNumReal++;
					if(pVehicle->PopTypeGet() == POPTYPE_RANDOM_AMBIENT || pVehicle->PopTypeGet()== POPTYPE_RANDOM_PATROL)
					{
						iNumRealRandomTraffic++;
					}
					break;
				}
				case VDM_DUMMY: { iNumDummy++; break; }
				case VDM_SUPERDUMMY: { iNumSuperDummy++; break; }
				default: { Assert(false); }
			}

			switch(pVehicle->m_CreationMethod)
			{
				case CVehicle::VEHICLE_CREATION_RANDOM:
					iNumRandomGenerated++;
					break;
				case CVehicle::VEHICLE_CREATION_PATHS:
					iNumPathGenerated++;
					break;
				case CVehicle::VEHICLE_CREATION_CARGEN:
					iNumCargenGenerated++;
					break;
				default:
					break;
			}

			if(pVehicle->GetWasReused())
				iNumReused++;
		}
	}

	iVehiclePoolSize = (int) CVehicle::GetPool()->GetSize();

	const float fDesired = CVehiclePopulation::ms_overrideNumberOfCars == -1 ? CVehiclePopulation::GetDesiredNumberAmbientVehicles() : CVehiclePopulation::ms_overrideNumberOfCars;
	iNumDesiredAmbient = (int) fDesired;
}

#if DEBUG_DRAW

void CVehiclePopulation::GetVehicleLODCounts(bool bParked, int &iCount, int &iReal, int &iDummy, int &iSuperDummy, int &iActive, int &iInactive, int &iTimesliced )
{
	iCount = 0;
	iReal = 0;
	iDummy = 0;
	iSuperDummy = 0;
	iActive = 0;
	iInactive = 0;
	iTimesliced = 0;

	if (!CVehicle::GetPool())
		return;

	s32 i = (s32) CVehicle::GetPool()->GetSize();
	while(i--)
	{
		CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(i);
		if (pVehicle)
		{
			bool isParked = false;

			if (pVehicle->PopTypeIsMission())
			{
				if (pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation)
					isParked = true;
			}
			else
			{
				if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
					isParked = true;
			}

			if( bParked == isParked )
			{
				iCount++;
				switch(pVehicle->GetVehicleAiLod().GetDummyMode())
				{
					case VDM_REAL: { iReal++; break; }
					case VDM_DUMMY: { iDummy++; break; }
					case VDM_SUPERDUMMY: { iSuperDummy++; break; }
					default: { Assert(false); }
				}

				// Now get active/inactive
				phInst *pInst = pVehicle->GetCurrentPhysicsInst();
				if( pInst && pInst->IsInLevel() && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex()) )
				{
					iActive++;
				}
				else
				{
					iInactive++;
				}

				if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
				{
					iTimesliced++;
				}
			}
		}
	}
}

void CVehiclePopulation::GetVehicleLODCounts(int &iCount, int &iReal, int &iDummy, int &iSuperDummy, int &iActive, int &iInactive, int &iTimesliced )
{
	iCount = 0;
	iReal = 0;
	iDummy = 0;
	iSuperDummy = 0;
	iActive = 0;
	iInactive = 0;
	iTimesliced = 0;

	s32 i = (s32) CVehicle::GetPool()->GetSize();
	while(i--)
	{
		CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(i);
		if (pVehicle)
		{
			iCount++;
			switch(pVehicle->GetVehicleAiLod().GetDummyMode())
			{
			case VDM_REAL: { iReal++; break; }
			case VDM_DUMMY: { iDummy++; break; }
			case VDM_SUPERDUMMY: { iSuperDummy++; break; }
			default: { Assert(false); }
			}

			// Now get active/inactive
			phInst *pInst = pVehicle->GetCurrentPhysicsInst();
			if( pInst && pInst->IsInLevel() && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex()) )
			{
				iActive++;
			}
			else
			{
				iInactive++;
			}

			//if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
			//{
			//	iTimesliced++;
			//}
		}
	}

}



#endif //DEBUG_DRAW


void CVehiclePopulation::HeavyDutyVehGenDebugging()
{
	static bool bOnlyActiveLinks = true;

	static float fMaxTextDist = 500.0f;
	const Vector3 vNodeTop(0.0f, 0.0f, 1.0f);
	const int iTextHeight = grcDebugDraw::GetScreenSpaceTextHeight();
	char tmp[256];

	const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	Vector3 vNodeA, vNodeB, vLinkMid;

	int i;
	for(i=0; i<ms_numGenerationLinks; i++)
	{
		CVehGenLink & vehGenLink = ms_aGenerationLinks[i];

		if(vehGenLink.m_nodeA.IsEmpty() || vehGenLink.m_nodeB.IsEmpty())
			continue;
		if(!ThePaths.IsRegionLoaded(vehGenLink.m_nodeA) || !ThePaths.IsRegionLoaded(vehGenLink.m_nodeB))
			continue;

		const CPathNode * pNodeA = ThePaths.FindNodePointer(vehGenLink.m_nodeA);
		const CPathNode * pNodeB = ThePaths.FindNodePointer(vehGenLink.m_nodeB);
		const CPathNodeLink * pLink = ThePaths.FindLinkBetween2Nodes(pNodeA, pNodeB);
		if(!pLink)
			continue;

		Assert(pNodeA && pNodeB);

		pNodeA->GetCoors(vNodeA);
		pNodeB->GetCoors(vNodeB);

		grcDebugDraw::Line(vNodeA, vNodeA + vNodeTop, Color_orange1, Color_orange1);
		grcDebugDraw::Line(vNodeB, vNodeB + vNodeTop, Color_orange1, Color_orange1);

		//------------------------------------------------------------------

		Vector3 vLink = pNodeB->GetPos() - pNodeA->GetPos();
		vLink.z = 0.0f;
		vLink.Normalize();
		vLink *= 2.0f;
		
		vLinkMid = ((pNodeA->GetPos() + pNodeB->GetPos()) * 0.5f) + vNodeTop;

		// If lanes exist both directions, offset debug info to the appropriate side
		if(pLink->m_1.m_LanesFromOtherNode)
		{
			Vector2 vOffset( vLink.y, -vLink.x );
			vLinkMid.x += vOffset.x;
			vLinkMid.y += vOffset.y;
		}
		//vLinkMid += (pNodeA > pNodeB) ? VEC3_ZERO : (ZAXIS * 2.0f);
		Color32 iTextCol = (pNodeA > pNodeB) ? Color_cyan : Color_red;

		const float fDist = (vOrigin - vLinkMid).Mag();
		if(fDist > fMaxTextDist)
			continue;

		const float fAlpha = 1.0f - (fDist/fMaxTextDist);
		iTextCol.SetAlpha((int)(fAlpha*255.0f));

		int iY = 0;

		if(pNodeA->m_2.m_switchedOff || pNodeB->m_2.m_switchedOff)
		{
			grcDebugDraw::Text(vLinkMid, Color_black, 0, iY, "SWITCHED OFF");
			iY += iTextHeight;
		}

		sprintf(tmp, "Density: %i (initial: %i)", vehGenLink.m_iDensity, vehGenLink.m_iInitialDensity);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

	
		// TODO: Scale colour by link disparity?
		if(vehGenLink.m_bIsActive)
		{
			grcDebugDraw::Line(vNodeA + vNodeTop, vNodeB + vNodeTop, Color_white, Color_white);
		}

		if(!vehGenLink.m_bIsActive)
		{
			grcDebugDraw::Line(vNodeA + vNodeTop, vNodeB + vNodeTop, Color_black, Color_black);
		}

		if(!vehGenLink.m_bIsActive && bOnlyActiveLinks)
		{
			continue;
		}

#if VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
		if(!vehGenLink.m_junctionNode.IsEmpty())
		{
			Vector3 vJunPos;
			const CPathNode * pJunNode = ThePaths.FindNodePointer(vehGenLink.m_junctionNode);
			pJunNode->GetCoors(vJunPos);

			grcDebugDraw::Line(vLinkMid, vJunPos + vNodeTop, Color_orange1);
		}
#endif

		const char * pCatTxt;
		switch(vehGenLink.m_iCategory)
		{
			case CPopGenShape::GC_KeyHoleInnerBand_off: { pCatTxt = "GC_KeyHoleInnerBand_off"; break; }
			case CPopGenShape::GC_KeyHoleOuterBand_off: { pCatTxt = "GC_KeyHoleOuterBand_off"; break; }
			case CPopGenShape::GC_KeyHoleInnerBand_on: { pCatTxt = "GC_KeyHoleInnerBand_on"; break; }
			case CPopGenShape::GC_KeyHoleOuterBand_on: { pCatTxt = "GC_KeyHoleOuterBand_on"; break; }
			case CPopGenShape::GC_KeyHoleSideWall_on: { pCatTxt = "GC_KeyHoleSideWall_on"; break; }
			case CPopGenShape::GC_InFOV_usableIfOccluded: { pCatTxt = "GC_InFOV_usableIfOccluded"; break; }
			case CPopGenShape::GC_InFOV_on: { pCatTxt = "GC_InFOV_on"; break; }
			default: { pCatTxt = "Category Unknown!"; break; }
		}
		sprintf(tmp, "index: %i, Cat: %s", i, pCatTxt);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "Score: %.1f UpToDate: %i", vehGenLink.m_fScore, vehGenLink.m_bUpToDate);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "A (%i:%i), B (%i:%i)", vehGenLink.m_nodeA.GetRegion(), vehGenLink.m_nodeA.GetIndex(), vehGenLink.m_nodeB.GetRegion(), vehGenLink.m_nodeB.GetIndex());
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_iLinkLanes: %i, m_fLinkHeading: %.2f", vehGenLink.m_iLinkLanes, vehGenLink.GetLinkHeading());
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_fLinkGrade: %.2f", vehGenLink.GetLinkGrade());
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_fNearbyVehiclesCount: %.2f", vehGenLink.GetNearbyVehiclesCount());
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "Vehicles included by grade: %.2f", vehGenLink.m_fNumNearbyVehiclesIncludedByGrade);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_iLinkAttempts: %i", vehGenLink.m_iLinkAttempts);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "Player's road: %s", vehGenLink.m_bOnPlayersRoad ? "true" : "false");
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_bOverlappingRoads: %s", vehGenLink.m_bOverlappingRoads ? "true" : "false");
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;	

		sprintf(tmp, "m_fDesNearVehCntDelta: %.3f", vehGenLink.m_fDesiredNearbyVehicleCountDelta);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		int numVehicles = Max(vehGenLink.m_iNumTrafficVehicles - vehGenLink.m_iLinkLanes * 4, 0);
		sprintf(tmp, "m_iNumTrafficVehicles: %i (%i)", numVehicles, vehGenLink.m_iNumTrafficVehicles);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

#if DR_VEHGEN_GRADE_DEBUGGING
		sprintf(tmp, "m_fDrDistance: %.3f", vehGenLink.m_fDrDistance);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_fDrPlanarDistance: %.3f", vehGenLink.m_fDrPlanarDistance);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_fDrZDist: %.3f", vehGenLink.m_fDrZDist);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "m_fDrGradeOffset: %.3f", vehGenLink.m_fDrGradeOffset);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;
#endif // DR_VEHGEN_GRADE_DEBUGGING

		const char * pFailType = "Success";
		switch(vehGenLink.m_iFailType)
		{
			case Fail_None: break;
			case Fail_InViewFrustum : { pFailType = "Fail_InViewFrustum"; break; }
			case Fail_NoSpawnAttemptedYet : { pFailType = "Fail_NoSpawnAttemptedYet"; break; }
			case Fail_NoCreationPositionFound : { pFailType = "Fail_NoCreationPositionFound"; break; }
			case Fail_LinkBetweenNodesNotFound: { pFailType = "Fail_LinkBetweenNodesNotFound"; break; }
			case Fail_NoAppropriateModel: { pFailType = "Fail_NoAppropriateModel"; break; }
			case Fail_SameVehicleModelAsLastTime: { pFailType = "Fail_SameVehicleModelAsLastTime"; break; }
			case Fail_NetworkRegister: { pFailType = "Fail_NetworkRegister"; break; }
			case Fail_PoliceBoat: { pFailType = "Fail_PoliceBoat"; break; }
			case Fail_Boat: { pFailType = "Fail_Boat"; break; }
			case Fail_AgainstFlowOfTraffic: { pFailType = "Fail_AgainstFlowOfTraffic"; break; }
			case Fail_MatrixNotAlignedWithTraffic: { pFailType = "Fail_MatrixNotAlignedWithTraffic"; break; }
			case Fail_AmbientBoatAlreadyHere: { pFailType = "Fail_AmbientBoatAlreadyHere"; break; }
			case Fail_DeadEnd: { pFailType = "Fail_DeadEnd"; break; }
			case Fail_RegionNotLoaded : { pFailType = "Fail_RegionNotLoaded"; break; }
			case Fail_SomeKindOfWaterLevelFuckup : { pFailType = "Fail_SomeKindOfWaterLevelFuckup"; break; }
			case Fail_CouldntPlaceBike : { pFailType = "Fail_CouldntPlaceBike"; break; }
			case Fail_CouldntPlaceCar : { pFailType = "Fail_CouldntPlaceCar"; break; }
			case Fail_TooCloseToNetworkPlayer : { pFailType = "Fail_TooCloseToNetworkPlayer"; break; }
			case Fail_NotNetworkTurnToCreateVehicleAtPosition : { pFailType = "Fail_NotNetworkTurnToCreateVehicleAtPosition"; break; }
			case Fail_OutsideCreationDistance : { pFailType = "Fail_OutsideCreationDistance"; break; }
			case Fail_RelativeSpeedCheck : { pFailType = "Fail_RelativeSpeedCheck"; break; }
			case Fail_CollisionBoxCheck : { pFailType = "Fail_CollisionBoxCheck"; break; }
			case Fail_CouldntAllocateVehicle : { pFailType = "Fail_CouldntAllocateVehicle"; break; }
			case Fail_CouldntAllocateDriver : { pFailType = "Fail_CouldntAllocateDriver"; break; }
			case Fail_NotStreamedIn : { pFailType = "Fail_NotStreamedIn"; break; }
			case Fail_SingleTrackPotentialCollision : { pFailType = "Fail_SingleTrackPotentialCollision"; break; }
			case Fail_CollidesWithTrajectoryOfMarkedVehicle : { pFailType = "Fail_CollidesWithTrajectoryOfMarkedVehicle"; break; }
            case Fail_OutOfLocalPlayerScope: { pFailType = "Fail_OutOfLocalPlayerScope"; break; }
			default : { Assert(false); pFailType = "UNKNOWN!!"; break; }
		}

		sprintf(tmp, "m_iFailType: %s", pFailType);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

#if VEHGEN_STORE_DEBUG_INFO_IN_LINKS
		sprintf(tmp, "m_iNumLinksInRange: %i", vehGenLink.m_iNumLinksInRange);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "%i : %i (vg %i)", vehGenLink.m_iLinkRegion, vehGenLink.m_iLinkIndex, vehGenLink.m_iVehGenLink);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;

		sprintf(tmp, "From: %i To: %i", vehGenLink.m_iFromIndex, vehGenLink.m_iToIndex);
		grcDebugDraw::Text(vLinkMid, iTextCol, 0, iY, tmp);
		iY += iTextHeight;		
#endif
	}
}

void CVehiclePopulation::DumpVehicleMemoryInfoCB()
{
	gPopStreaming.DumpVehicleMemoryInfo();
}

#endif // __BANK


void CVehiclePopulation::ResetPerFrameModifiers()
{
	ms_scriptedRangeMultiplierThisFrame = 1.0f;
	ms_scriptedRangeMultiplierThisFrameModified = false;

	ms_fRendererRangeMultiplierOverrideThisFrame = 0.0f;

	ms_scriptRandomVehDensityMultiplierThisFrame = 1.0f;
	ms_scriptParkedCarDensityMultiplierThisFrame = 1.0f;

	ms_scriptDisableRandomTrains = false;

	ms_useTempPopControlPointThisFrame = false;

	ms_scriptRemoveEmptyCrowdingPlayersInNetGamesThisFrame = false;
	ms_scriptCapEmptyCrowdingPlayersInNetGamesThisFrame = 1000;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : NetworkRegisterAllVehicles
// PURPOSE :  Registers all vehicles with the network object manager when a new
//				network game starts
// PARAMETERS :	None.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::NetworkRegisterAllVehicles()
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;


	//Ensure all train engines are registered before train subsequent cars are registered
	for (s32 i=0; i<pool->GetSize(); i++)
	{
		pVehicle = pool->GetSlot(i);

		if (pVehicle && !pVehicle->GetNetworkObject() && !pVehicle->m_nPhysicalFlags.bNotToBeNetworked)
		{
			bool registeredVehicle = false;

			// script vehicles that are flagged not to be networked are not registered
			CScriptEntityExtension* pExtension = pVehicle->GetExtension<CScriptEntityExtension>();

			if (!pExtension || pExtension->GetIsNetworked())
			{
				if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					CTrain* pTrain = static_cast<CTrain*>(pVehicle);
					if (pTrain && pTrain->IsEngine())
					{
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CTrain*>(pVehicle), 0, 0);

						if(!registeredVehicle)
						{
#if __DEV
							NetworkInterface::GetObjectManagerLog().Log("Failed to register a vehicle with the network at the start of a network session! Reason is %s\r\n", NetworkInterface::GetLastRegistrationFailureReason());
							Assertf(0, "Failed to register a vehicle with the network at the start of a network session!");
#endif
						}
					}
				}
			}
		}
	}
	
	//Process all vehicles - now that train engines are established above
	for (s32 i=0; i<pool->GetSize(); i++)
	{
		pVehicle = pool->GetSlot(i);

		if (pVehicle && !pVehicle->GetNetworkObject())
		{
			if (pVehicle->m_nPhysicalFlags.bNotToBeNetworked)
			{
#if ENABLE_NETWORK_LOGGING
				NetworkInterface::GetObjectManagerLog().Log("Failed to register a vehicle 0x%p with the network at the start of a network session! Reason is m_nPhysicalFlags.bNotToBeNetworked\r\n", pVehicle);
#endif
			}
			else
			{
				bool registeredVehicle = false;

				// script vehicles that are flagged not to be networked are not registered
				CScriptEntityExtension* pExtension = pVehicle->GetExtension<CScriptEntityExtension>();

				if (!pExtension || pExtension->GetIsNetworked())
				{
					switch (pVehicle->GetVehicleType())
					{
					case VEHICLE_TYPE_CAR:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CAutomobile*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_QUADBIKE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CQuadBike*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_BIKE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CBike*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_BICYCLE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CBmx*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_TRAIN:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CTrain*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_BOAT:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CBoat*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_HELI:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CHeli*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_PLANE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CPlane*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_AUTOGYRO:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CAutogyro*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_SUBMARINE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CSubmarine*>(pVehicle), 0, 0);
						break;
					case VEHICLE_TYPE_TRAILER:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CTrailer*>(pVehicle), 0, 0);
						break;
#if ENABLE_DRAFT_VEHICLE
					case VEHICLE_TYPE_DRAFT:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CDraftVeh*>(pVehicle), 0, 0);
						break;
#endif
					case VEHICLE_TYPE_SUBMARINECAR:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CSubmarineCar*>(pVehicle), 0, 0);
						break;

					case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CAmphibiousAutomobile*>(pVehicle), 0, 0);
						break;

					case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CAmphibiousQuadBike*>(pVehicle), 0, 0);
						break;
				
					case VEHICLE_TYPE_BLIMP:
						registeredVehicle = NetworkInterface::RegisterObject(static_cast<CBlimp*>(pVehicle), 0, 0);
						break;
					default:
						Assertf(0, "Unrecognised vehicle type %d", pVehicle->GetVehicleType());
						break;
					}

					if(!registeredVehicle)
					{
#if ENABLE_NETWORK_LOGGING
						NetworkInterface::GetObjectManagerLog().Log("Failed to register a vehicle with the network at the start of a network session! Reason is %s\r\n", NetworkInterface::GetLastRegistrationFailureReason());
						Assertf(0, "Failed to register a vehicle with the network at the start of a network session!");
#endif
					}
				}
				else 
				{
#if ENABLE_NETWORK_LOGGING
					NetworkInterface::GetObjectManagerLog().Log("Failed to register a vehicle 0x%p with the network at the start of a network session! Reason is !pExtension->GetIsNetworked()\r\n", pVehicle);
#endif
				}
			}
		}
	}
}

void CVehiclePopulation::RemoveExcessNetworkVehicle(CVehicle *pVehicle)
{
    if(pVehicle)
    {
        BANK_ONLY(ms_lastRemovalReason = Removal_ExcessNetworkVehicle);
    
        // make sure it is on the scene update so it actually gets removed
        if(!pVehicle->GetIsOnSceneUpdate() || !aiVerifyf(pVehicle->GetUpdatingThroughAnimQueue() || pVehicle->GetIsMainSceneUpdateFlagSet(),
            "Flagging a vehicle for removal (%s) that is on neither the main scene update or update through anim queue list! This will likely cause a vehicle leak!", pVehicle->GetDebugName()))
        {
            pVehicle->AddToSceneUpdate();
        }

        // have to force the pop type here, otherwise FlagToDestroyWhenNextProcessed will fail. The mission state is not cleaned up in CTheScripts::UnregisterEntity, because the vehicle is about 
        // to be deleted
        pVehicle->PopTypeSet(POPTYPE_RANDOM_AMBIENT);

        pVehicle->FlagToDestroyWhenNextProcessed();
    }
}

void CVehiclePopulation::HandlePopCycleInfoChange()
{
	ms_lastPopCycleChangeTime = fwTimer::GetTimeInMilliseconds();
}

void CVehiclePopulation::IncrementNumVehiclesCreatedThisCycle()
{
	++ms_iNumVehiclesCreatedThisCycle;
}

void CVehiclePopulation::IncrementNumPedsCreatedThisCycle()
{
	++ms_iNumPedsCreatedThisCycle;
}

void CVehiclePopulation::IncrementNumVehiclesDestroyedThisCycle()
{
	++ms_iNumVehiclesDestroyedThisCycle;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IncrementNetworkVehiclesSpawnedThisFrame
// PURPOSE :  Tells the vehicle population system that network has spawned a vehicle this frame
// PARAMETERS :	None.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::IncrementNumNetworkVehiclesCreatedThisFrame()
{
	ms_iNumNetworkVehiclesCreatedThisFrame++;
	PF_INCREMENT(VehiclesCreatedByNetwork);
	// we might be starving the vehicle population system by calling this, maybe put some asserts in?
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StopSpawningVehs
// PURPOSE :  Stops random vehs being created for a specified time in milliseconds
// PARAMETERS :	noSpawnTime- amount of time in milliseconds to block spawning.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::StopSpawningVehs()
{
	Displayf("\nCVehiclePopulation::StopSpawningVehs()\n");

	ms_noVehSpawn = true;
	CTheCarGenerators::StopGenerating();
}

void CVehiclePopulation::StartSpawningVehs()
{
	Displayf("\nCVehiclePopulation::StartSpawningVehs()\n");

	ms_noVehSpawn = false;
	CTheCarGenerators::StartGenerating();
}

void CVehiclePopulation::InstantFillPopulation()
{
	ms_bInstantFillPopulation = 2;
}

bool CVehiclePopulation::IsInstantFillPopulationActive()
{
	return ms_bInstantFillPopulation > 0;
}

bool CVehiclePopulation::HasInstantFillPopulationFinished()
{
	return ms_bInstantFillPopulation <= 0;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveAllVehs
// PURPOSE :  Tries to remove all cars
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::RemoveAllVehsHard()
{
	RemoveAllVehs(false, false);
}
void CVehiclePopulation::RemoveAllVehsSoft()
{
	RemoveAllVehs(true, false);
}
void CVehiclePopulation::RemoveAllVehsSoftExceptFocus()
{
	RemoveAllVehs(true, true);
}
void CVehiclePopulation::RemoveAllVehsHardExceptNearFocus()
{
#if __BANK
	CVehicle::Pool *pool = CVehicle::GetPool();
	for(int i=0; i<pool->GetSize(); i++)
	{
		if(CVehicle * pVehicle = pool->GetSlot(i))
		{
			const float fNearThreshold = 3.0f;
			if(!CDebugScene::FocusEntities_IsNearGroup(pVehicle->GetVehiclePosition(),fNearThreshold))
			{
#if __DEV
				VecMapDisplayVehiclePopulationEvent(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), VehPopDestroy);
#endif // _DEV
				BANK_ONLY(ms_lastRemovalReason = Removal_RemoveAllVehicles);
				RemoveVeh(pVehicle, false, Removal_RemoveAllVehicles);
			}
		}
	}
#endif
}
void CVehiclePopulation::RemoveFocusVehicleSoft()
{
#if !__FINAL
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	CVehicle* pVehicle = NULL;
	if (pEntity && pEntity->GetIsTypeVehicle())
	{
		pVehicle = static_cast<CVehicle*>(pEntity);
	}
	if (pVehicle && pVehicle->CanBeDeleted())
	{
        BANK_ONLY(ms_lastRemovalReason = Removal_Debug);
		RemoveVeh(pVehicle, false, Removal_Debug);
	}
#endif //!__FINAL
}
void CVehiclePopulation::RemoveAllVehs(bool respectCanBeDeleted, bool NOTFINAL_ONLY(bAllExpectFocus))
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();

	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle)
		{
#if !__FINAL
			if(bAllExpectFocus)
			{
				if(CEntity * pFocusEntity = CDebugScene::FocusEntities_Get(0))
				{
					if(pFocusEntity==pVehicle)
						continue;
					if(pFocusEntity==pVehicle->GetAttachParentVehicle())
						continue;
					if(pFocusEntity==pVehicle->GetDummyAttachmentParent())
						continue;
				}
			}
#endif
			if(!respectCanBeDeleted || pVehicle->CanBeDeleted())
			{
#if __DEV
				VecMapDisplayVehiclePopulationEvent(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), VehPopDestroy);
#endif // _DEV
                BANK_ONLY(ms_lastRemovalReason = Removal_RemoveAllVehicles);
				RemoveVeh(pVehicle, false, Removal_RemoveAllVehicles);
			}
		}
	}

	// Clear out the vehicle reuse pool as well.
	FlushVehicleReusePool();
}
void CVehiclePopulation::RemoveAllVehsWithException(CVehicle* exception)
{
	CVehicle::Pool*	pool = CVehicle::GetPool();
	s32 i = (s32)pool->GetSize();
	while (i--)
	{
		CVehicle* pVeh = pool->GetSlot(i);

		if (pVeh && !pVeh->ContainsLocalPlayer() && pVeh != exception)
		{
			RemoveVeh(pVeh, false, Removal_RemoveAllVehsWithExcepton);
		}
	}
}


#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::SwitchVehToDummyCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pEnt);
			pVeh->TryToMakeIntoDummy(VDM_DUMMY);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::SwitchVehToRealCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pEnt);
			pVeh->TryToMakeFromDummy();
		}
	}
}

void CVehiclePopulation::SwitchVehSuperDummyCB()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pEnt);
			pVeh->TryToMakeIntoDummy(VDM_SUPERDUMMY);
		}
	}
}

void WreckFocusVehicleCB()
{
	CEntity* pEnt = CVehicleDebug::GetFocusVehicle();
	if(pEnt && pEnt->GetIsTypeVehicle())
	{
		CVehicle * pVehicle = (CVehicle*)pEnt;
		//pVehicle->SetIsWrecked();
		for(s32 i=0; i<10; i++)
			pVehicle->GetVehicleDamage()->BlowUpCarParts(NULL);
		pVehicle->GetVehicleDamage()->PopOutWindScreen();
		pVehicle->GetVehicleDamage()->SetEngineHealth(0.0f);
		pVehicle->ExtinguishCarFire();

		s32 iNumSeats = pVehicle->GetLayoutInfo()->GetNumSeats();
		for(s32 s=0; s<iNumSeats; s++)
		{
			CPed * pPed = pVehicle->GetPedInSeat(s);
			if(pPed)
			{
				pPed->SetPedOutOfVehicle();
			}
		}
	}
}

void CVehiclePopulation::WakeUpAllParkedVehiclesCB()
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();

	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle && pVehicle->ConsiderParkedForLodPurposes())
		{
			pVehicle->ApplyImpulseCg(ZAXIS * (pVehicle->GetMass() * -1.0f));
		}
	}
}

#if __BANK
int CVehiclePopulation::CMarkUpEditor::m_iCurrentSphere = -1;
int CVehiclePopulation::CMarkUpEditor::m_iCurrentSphereX = 0;
int CVehiclePopulation::CMarkUpEditor::m_iCurrentSphereY = 0;
int CVehiclePopulation::CMarkUpEditor::m_iCurrentSphereZ = 0;
int CVehiclePopulation::CMarkUpEditor::m_iCurrentSphereRadius = CVehiclePopulation::CMarkUpEditor::ms_iDefaultSphereRadius;
bool CVehiclePopulation::CMarkUpEditor::m_bCurrentSphere_FlagRoadsOverlap = false;
int CVehiclePopulation::CMarkUpEditor::m_iNumSpheres = 0;

bkText * CVehiclePopulation::CMarkUpEditor::m_pNumSpheresText = NULL;
bkSlider * CVehiclePopulation::CMarkUpEditor::m_pCurrentSphereSlider = NULL;
bkSlider * CVehiclePopulation::CMarkUpEditor::m_pXPosSlider = NULL;
bkSlider * CVehiclePopulation::CMarkUpEditor::m_pYPosSlider = NULL;
bkSlider * CVehiclePopulation::CMarkUpEditor::m_pZPosSlider = NULL;
bkSlider * CVehiclePopulation::CMarkUpEditor::m_pRadiusSlider = NULL;
bkToggle * CVehiclePopulation::CMarkUpEditor::m_pRoadsOverlapToggle = NULL;

void CVehiclePopulation::CMarkUpEditor::Init()
{
	if(m_pCurrentSphereSlider)
		m_pCurrentSphereSlider->SetRange((float)-1, (float)CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount());
	m_iNumSpheres = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount();
	m_iCurrentSphere = -1;
	OnChangeCurrentSphere();
}
void CVehiclePopulation::CMarkUpEditor::OnChangeCurrentSphere()
{
	if(m_iCurrentSphere >= CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount())
		m_iCurrentSphere = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount()-1;

	if(m_iCurrentSphere < 0)
	{
		m_iCurrentSphereX = 0;
		m_iCurrentSphereY = 0;
		m_iCurrentSphereZ = 0;
		m_iCurrentSphereRadius = ms_iDefaultSphereRadius;
		m_bCurrentSphere_FlagRoadsOverlap = false;
	}
	else
	{
		CVehGenSphere & sphere = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres[m_iCurrentSphere];

		m_iCurrentSphereX = sphere.m_iPosX;
		m_iCurrentSphereY = sphere.m_iPosY;
		m_iCurrentSphereZ = sphere.m_iPosZ;
		m_iCurrentSphereRadius = sphere.m_iRadius;
		m_bCurrentSphere_FlagRoadsOverlap = ((sphere.m_iFlags & CVehGenSphere::Flag_RoadsOverlap)!=0);
	}
}
void CVehiclePopulation::CMarkUpEditor::OnModifyCurrentSphereData()
{
	if(m_iCurrentSphere >= CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount())
		m_iCurrentSphere = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount()-1;

	if(m_iCurrentSphere < 0)
	{
		m_iCurrentSphereX = 0;
		m_iCurrentSphereY = 0;
		m_iCurrentSphereZ = 0;
		m_iCurrentSphereRadius = ms_iDefaultSphereRadius;
		m_bCurrentSphere_FlagRoadsOverlap = false;
	}
	else
	{
		CVehGenSphere & sphere = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres[m_iCurrentSphere];
		sphere.m_iPosX = (s16)m_iCurrentSphereX;
		sphere.m_iPosY = (s16)m_iCurrentSphereY;
		sphere.m_iPosZ = (s16)m_iCurrentSphereZ;
		sphere.m_iRadius = (u8)m_iCurrentSphereRadius;
		sphere.m_iFlags = 0;
		if(m_bCurrentSphere_FlagRoadsOverlap) sphere.m_iFlags |= CVehGenSphere::Flag_RoadsOverlap;
	}
}
void CVehiclePopulation::CMarkUpEditor::OnAddNewSphere()
{
	const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	
	if(CVehiclePopulation::ms_iNumMarkUpSpheres < MAX_VEHGEN_MARKUP_SPHERES)
	{
		CVehGenSphere & sphere = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.Append();
		sphere.m_iPosX = (s16)vOrigin.x;
		sphere.m_iPosY = (s16)vOrigin.y;
		sphere.m_iPosZ = (s16)vOrigin.z;
		sphere.m_iRadius = (s16)ms_iDefaultSphereRadius;
		sphere.m_iFlags = (u8)ms_iDefaultSphereFlags;

		m_iCurrentSphere = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount()-1;

		if(m_pCurrentSphereSlider)
			m_pCurrentSphereSlider->SetRange((float)-1, (float)CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount());
		m_iNumSpheres = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount();

		OnChangeCurrentSphere();
	}
}
void CVehiclePopulation::CMarkUpEditor::OnDeleteCurrentSphere()
{
	if(m_iCurrentSphere >= 0 && m_iCurrentSphere < ms_VehGenMarkUpSpheres.m_Spheres.GetCount())
	{
		ms_VehGenMarkUpSpheres.m_Spheres.Delete(m_iCurrentSphere);
		if(m_iCurrentSphere >= ms_VehGenMarkUpSpheres.m_Spheres.GetCount())
			m_iCurrentSphere--;

		if(m_pCurrentSphereSlider)
			m_pCurrentSphereSlider->SetRange((float)-1, (float)CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount());
		m_iNumSpheres = CVehiclePopulation::ms_VehGenMarkUpSpheres.m_Spheres.GetCount();

		OnChangeCurrentSphere();
	}
}
void CVehiclePopulation::CMarkUpEditor::OnSelectClosestToCamera()
{
	const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	float fClosestDist = FLT_MAX;
	int iClosest = -1;
	for(int i=0; i<ms_VehGenMarkUpSpheres.m_Spheres.GetCount(); i++)
	{
		CVehGenSphere & sphere = ms_VehGenMarkUpSpheres.m_Spheres[i];
		Vector3 vPos(sphere.m_iPosX, sphere.m_iPosY, sphere.m_iPosZ);
		if((vPos - vOrigin).Mag2() < fClosestDist)
		{
			iClosest = i;
			fClosestDist = (vPos - vOrigin).Mag2();
		}
	}
	if(iClosest != -1)
	{
		m_iCurrentSphere = iClosest;
		OnChangeCurrentSphere();
	}
}
void CVehiclePopulation::RenderMarkupSpheres()
{
	const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	for(int i=0; i<ms_VehGenMarkUpSpheres.m_Spheres.GetCount(); i++)
	{
		CVehGenSphere & sphere = ms_VehGenMarkUpSpheres.m_Spheres[i];
		Vector3 vPos(sphere.m_iPosX, sphere.m_iPosY, sphere.m_iPosZ);
		int ypos = 0;

		if((vPos - vOrigin).Mag2() < 1000.0f*1000.0f)
		{
			Color32 col = (ms_MarkUpEditor.m_iCurrentSphere == i) ? 
				((fwTimer::GetFrameCount()&2) ? Color_red : Color_AntiqueWhite2) : Color_AntiqueWhite2;

			Color32 textCol = (ms_MarkUpEditor.m_iCurrentSphere == i) ? 
				((sphere.m_iFlags) ? Color_white : Color_red) : ((sphere.m_iFlags) ? Color_grey7 : Color32(0.1f, 0.0f, 0.0f));

			// Draw sphere
			grcDebugDraw::Sphere(
				vPos,
				sphere.m_iRadius,
				col,
				false);

			// Draw flags associated with each sphere
			if(!sphere.m_iFlags)
			{
				grcDebugDraw::Text(vPos, textCol, 0, ypos++ * grcDebugDraw::GetScreenSpaceTextHeight(), "No Flags Set!");
			}
			else
			{
				if(sphere.m_iFlags & CVehGenSphere::Flag_RoadsOverlap)
					grcDebugDraw::Text(vPos, textCol, 0, ypos++ * grcDebugDraw::GetScreenSpaceTextHeight(), "Flag_RoadsOverlap");
			}
		}
	}
}
// Save the spheres out as PSO
void CVehiclePopulation::SaveMarkupSpheres()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VEHGEN_MARKUP_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		// Check that file is writable?
		ASSERT_ONLY(bool bSavedOk =) PARSER.SaveObject(pData->m_filename, "xml", &ms_VehGenMarkUpSpheres);
		Assertf( bSavedOk, "\n\"%s\" did not save properly.\nThis is most likely because it is not writable\nPlease ensure you have it checked out in perforce, and try again\n", pData->m_filename );
	}
}

#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::VecMapDisplayVehiclePopulationEvent(const Vector3& pos, VehPopEventType eventType)
{
	if( CDebugScene::bDisplayVehPopFailedCreateEventsOnVMap ||
		CDebugScene::bDisplayVehPopCreateEventsOnVMap ||
		CDebugScene::bDisplayVehPopDestroyEventsOnVMap ||
		CDebugScene::bDisplayVehPopConversionEventsOnVMap)
	{
		Color32 baseColour;
		switch(eventType)
		{
		case VehPopFailedCreate:
			if(!CDebugScene::bDisplayVehPopFailedCreateEventsOnVMap)
			{
				return;
			}
			baseColour.Set(255,0,0,255);
			break;
		case VehPopCreate:
			if(!CDebugScene::bDisplayVehPopCreateEventsOnVMap)
			{
				return;
			}
			baseColour.Set(128,128,255,255);
			break;
		case VehPopCreateEmegencyVeh:
			if(!CDebugScene::bDisplayVehPopCreateEventsOnVMap)
			{
				return;
			}
			baseColour.Set(255,255,128,255);
			break;
		case VehPopConvertToDummy:
			if(!CDebugScene::bDisplayVehPopConversionEventsOnVMap)
			{
				return;
			}
			baseColour.Set(255,0,255,255);
			break;
		case VehPopConvertToReal:
			if(!CDebugScene::bDisplayVehPopConversionEventsOnVMap)
			{
				return;
			}
			baseColour.Set(0,255,0,255);
			break;
		case VehPopDestroy:
			if(!CDebugScene::bDisplayVehPopDestroyEventsOnVMap)
			{
				return;
			}
			baseColour.Set(10,10,10,255);
			break;
		default:
			Assertf(false, "Unkown veh pop event type.");
			break;
		}

		CVectorMap::MakeEventRipple(pos, 20.0f, 1000, baseColour);
	}
};
#endif // __DEV



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ScriptGenerateOneEmergencyServicesCar
// PURPOSE :  For the script to try and create an ambulance or firetruck.
/////////////////////////////////////////////////////////////////////////////////
CVehicle *CVehiclePopulation::ScriptGenerateOneEmergencyServicesCar(u32 ModelIndex, const Vector3& TargetCoors, const bool bPedsWalkToLocation)
{
	fwModelId modelId((strLocalIndex(ModelIndex)));
	Assertf(CModelInfo::HaveAssetsLoaded(modelId), "%s:ScriptGenerateOneEmergencyServicesCar: Model not streamed in", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		return NULL;
	}

	bool canCreateCar = true;

	CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
	CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_EmergencyVehicle);

	if(NetworkInterface::IsGameInProgress())
	{
		if(ModelIndex == MI_CAR_AMBULANCE)
		{
			const u32 numRequiredPeds    = 2;
			const u32 numRequiredCars    = 1;
			const u32 numRequiredObjects = 0;
			canCreateCar = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);
		}
		else if(ModelIndex == MI_CAR_FIRETRUCK)
		{
			const u32 numRequiredPeds    = 2;
			const u32 numRequiredCars    = 1;
			const u32 numRequiredObjects = 0;
			canCreateCar = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);
		}
		else if(ModelIndex == MI_CAR_POLICE || ModelIndex == MI_CAR_POLICE_2 || ModelIndex == MI_CAR_SHERIFF)
		{
			const u32 numRequiredPeds    = 2;
			const u32 numRequiredCars    = 1;
			const u32 numRequiredObjects = 0;
			canCreateCar = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);
		}
	}

	CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldGenContext);

	CVehicle *pNewCar = canCreateCar ? CVehiclePopulation::GenerateOneEmergencyServicesCar(CGameWorld::FindLocalPlayerCoors(), 1.0f, modelId, TargetCoors, 0.0f, false) : 0;

	bool bOccupantsAdded = false;

	if (pNewCar)
	{
		if (ModelIndex == MI_CAR_AMBULANCE )
		{
			bOccupantsAdded = CVehiclePopulation::AddAmbulanceOccupants(pNewCar);
		}
		else if (ModelIndex == MI_CAR_FIRETRUCK )
		{
			bOccupantsAdded = CVehiclePopulation::AddFiretruckOccupants(pNewCar);
		}
		else if (ModelIndex == MI_CAR_POLICE || ModelIndex == MI_CAR_POLICE_2 || ModelIndex == MI_CAR_SHERIFF)
		{
			bOccupantsAdded = CVehiclePopulation::AddPoliceVehOccupants(pNewCar);
		}

		if (!bOccupantsAdded || !pNewCar->GetDriver())
		{
            BANK_ONLY(ms_lastRemovalReason = Removal_OccupantAddFailed);
			RemoveVeh(pNewCar, true, Removal_OccupantAddFailed);
			pNewCar = NULL;
		}
		else
		{
			CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(TargetCoors, 99999.9f);//, fwRandom::GetRandomNumberInRange(0, 5)); //vDestination);
			if ( bPedsWalkToLocation && !NearestNode.IsEmpty() )
			{
				Vector3 nearestCoors;
				ThePaths.FindNodePointer(NearestNode)->GetCoors(nearestCoors);
				CTaskSequenceList* pSequence = rage_new CTaskSequenceList();

				sVehicleMissionParams params;
				params.m_iDrivingFlags = DMode_AvoidCars;
				params.m_fCruiseSpeed = 18.0f;
				params.SetTargetPosition(nearestCoors);
				params.m_fTargetArriveDist = 10.0f;
				aiTask* pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
				pSequence->AddTask(rage_new CTaskControlVehicle(pNewCar, pCarTask));
			
				pCarTask = rage_new CTaskVehiclePullOver();
				pSequence->AddTask(rage_new CTaskControlVehicle(pNewCar, pCarTask));

				pSequence->AddTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, TargetCoors), rage_new CTaskPause(15000), CTaskComplexControlMovement::TerminateOnEither));

				// Return to the vehicle
				pSequence->AddTask(rage_new CTaskEnterVehicle(pNewCar, SR_Specific,0));
				pNewCar->GetDriver()->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskUseSequence(*pSequence), PED_TASK_PRIORITY_PRIMARY, true);

				for( s32 i = 0; i < pNewCar->GetSeatManager()->GetMaxSeats(); i++ )
				{
					if( pNewCar->GetSeatManager()->GetPedInSeat(i) && i!=pNewCar->GetDriverSeat())
					{
						CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
						pSequence->AddTask(rage_new CTaskWaitForMyCarToStop() );
						pSequence->AddTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, TargetCoors + Vector3(1.0f, 1.0f, 0.0f)), rage_new CTaskPause(15000), CTaskComplexControlMovement::TerminateOnEither));
						pSequence->AddTask(rage_new CTaskWander(MOVEBLENDRATIO_WALK, fwRandom::GetRandomNumberInRange(0.0f, TWO_PI)));
						pNewCar->GetSeatManager()->GetPedInSeat(i)->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskUseSequence(*pSequence), PED_TASK_PRIORITY_PRIMARY, true);
					}
				}
			}
			else
			{
				sVehicleMissionParams params;
				params.m_iDrivingFlags = DMode_AvoidCars;
				params.m_fCruiseSpeed = 18.0f;
				params.SetTargetPosition(TargetCoors);
				params.m_fTargetArriveDist = 20.0f;

				CTask* pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
				pNewCar->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
			}

			pNewCar->TurnSirenOn(true);

			return pNewCar;
		}
	}

	return NULL;
}

bool CVehiclePopulation::ScriptGenerateVehCreationPosFromPaths(const Vector3& targetPos, float DesiredHeadingDegrees, float DesiredHeadingToleranceDegrees, float MinCreationDistance, bool bIncludeSwitchedOffNodes, bool bNoWater, bool bAllowAgainstTraffic, Vector3& pResult, Vector3& pResultLinkDir)
{
	CNodeAddress	FromNode, ToNode;
	float rangeScale = 1.0f, pFraction = 0.0f;
	float CreationDistOnScreen = rage::Max(MinCreationDistance, GetCreationDistance(rangeScale));
	float CreationDistOffScreen = rage::Max(MinCreationDistance, GetCreationDistanceOffScreen(rangeScale));

	const float DirectionX = ANGLE_TO_VECTOR_X(DesiredHeadingDegrees);
	const float DirectionY = ANGLE_TO_VECTOR_Y(DesiredHeadingDegrees);
	const float fDesiredHeadingToleranceDegreesClamped = Clamp(DesiredHeadingToleranceDegrees, 0.0f, 180.0f);
	const float fDesiredHeadingToleranceRadians = DtoR * fDesiredHeadingToleranceDegreesClamped;
	const float RequiredDotProduct = cosf(fDesiredHeadingToleranceRadians);

	const bool bRetVal = GenerateVehCreationPosFromPaths(targetPos, rangeScale, DirectionX, DirectionY, RequiredDotProduct, true, CreationDistOnScreen, CreationDistOffScreen, bIncludeSwitchedOffNodes, bNoWater, &pResult, &FromNode, &ToNode, &pFraction, true, bAllowAgainstTraffic);
	
	if (bRetVal)
	{
		const CPathNode* pFromNode = ThePaths.FindNodePointerSafe(FromNode);
		const CPathNode* pToNode = ThePaths.FindNodePointerSafe(ToNode);
		if (pFromNode && pToNode)
		{
			Vector3 vDir = pToNode->GetPos() - pFromNode->GetPos();
			vDir.NormalizeSafe(XAXIS);
			pResultLinkDir = vDir;
		}
	}

	return bRetVal;
}


//////////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetScriptCamPopulationOriginOverride
// PURPOSE :	Allows designers to force the scripted camera to become the origin
//				for spawning of vehicles; default behaviour is for this to remain
//				centred round the player regardless of script camera.
//				Must be called every frame (for concerns of it not being deactivated)
// PARAMETERS :	None.
// RETURNS :	Nothing.
//////////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::SetScriptCamPopulationOriginOverride()
{
#if __BANK
	const bool bDbgCam = camInterface::GetDebugDirector().IsFreeCamActive();
#else
	const bool bDbgCam = false;
#endif

	const camBaseCamera * pCam = camInterface::GetDominantRenderedCamera();
	if(bDbgCam || (pCam && pCam->GetIsClassId(camScriptedCamera::GetStaticClassId())))
	{
		Vector3 vOrigin = camInterface::GetScriptDirector().GetFrame().GetPosition();
		Vector3 vDir = camInterface::GetScriptDirector().GetFrame().GetFront();
		Vector3 vVel = camInterface::GetVelocity();
		float fov = camInterface::GetScriptDirector().GetFrame().GetFov();

		LocInfo locationInfo = LOCATION_EXTERIOR;
		CViewport * pViewport = gVpMan.GetCurrentViewport();
		if(pViewport)
		{
			CInteriorInst* pIntInst = pViewport->GetInteriorProxy() ? pViewport->GetInteriorProxy()->GetInteriorInst() : NULL;
			locationInfo = CPedPopulation::ClassifyInteriorLocationInfo(false, pIntInst, pViewport->GetRoomIdx());
		}

		UseTempPopControlPointThisFrame( vOrigin, vDir, vVel, fov, locationInfo );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UseTempPopControlPointThisFrame
// PURPOSE :	Update the population centers based on where the supplied info,
//				as opposed to where the player is for this frame.
//				This must be called for every frame where we want to base the
//				population on the info.
// PARAMETERS :	None.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CVehiclePopulation::UseTempPopControlPointThisFrame (const Vector3& pos, const Vector3& dir, const Vector3& vel, float fov, LocInfo locationInfo)
{
	ms_useTempPopControlPointThisFrame	= true;

	ms_tmpPopOrigin.m_centre			= pos;
	ms_tmpPopOrigin.m_baseHeading		= fwAngle::LimitRadianAngle(rage::Atan2f(-dir.x, dir.y));
	ms_tmpPopOrigin.m_centreVelocity	= vel;
	ms_tmpPopOrigin.m_locationInfo		= locationInfo;

	const CViewport* viewport = gVpMan.GetGameViewport();
	const float aspectRatio = viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	ms_tmpPopOrigin.m_FOV = fov;
	ms_tmpPopOrigin.m_tanHFov = tanf(ms_tmpPopOrigin.m_FOV * 0.5f * DtoR) * aspectRatio;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GenerateCarCreationPos
// PURPOSE :  Finds coordinates to create a new car at. This version of the function
//			  will start at the node nearest to the player and move out from there.
/////////////////////////////////////////////////////////////////////////////////
#define MAX_DEPTH_FOR_SEARCH (80)
bool CVehiclePopulation::GenerateVehCreationPosFromPaths( const Vector3& popCtrlCentre, const float UNUSED_PARAM(rangeScale), float DirectionX, float DirectionY, float RequiredDotProduct, bool bRequiredInside, float CreationDistOnScreen, float CreationDistOffScreen, bool bIncludeSwitchedOffNodes, bool bNoWater, Vector3 *pResult, CNodeAddress *pFromNode, CNodeAddress *pToNode, float *pFraction, bool bIgnoreDensity, bool bAllowGoAgainstTraffic)
{

	CNodeAddress	NewNode, CurrentNode;
	const CPathNode 		*pNewNode = NULL;
	const CPathNode 		*pCurrentNode;
	const CPathNodeLink	*pLink = NULL;
	float			Diff1, Diff2, Dist1, Dist2, DistanceAlongRoad = 0;
	float			VecToPointX, VecToPointY, Length, DotPr;
	s32			LinkToTest;

	// We do this thing whereby we remember the nearest nodes until the player
	// has traveled a certain distance. Only then will we update the nearest nodes.
	// (Finding them is pretty expensive)
	static	CNodeAddress	StoredNearestNode, StoredSecondNearestNode;
	static	CNodeAddress	StoredNearestNode_IncludingSwitchedOff, StoredSecondNearestNode_IncludingSwitchedOff;
	static	Vector3			UpdateCoors;

	if (	((Vector3)(popCtrlCentre - UpdateCoors)).XYMag2() > (10.0f*10.0f) ||
		fwTimer::GetTimeInMilliseconds() > ms_GenerateVehCreationPosFromPathsUpdateTime)
	{
		ThePaths.Find2NodesForCarCreation(popCtrlCentre, &StoredNearestNode, &StoredSecondNearestNode, true);
		ThePaths.Find2NodesForCarCreation(popCtrlCentre, &StoredNearestNode_IncludingSwitchedOff, &StoredSecondNearestNode_IncludingSwitchedOff, false);

		UpdateCoors = popCtrlCentre;
		ms_GenerateVehCreationPosFromPathsUpdateTime = fwTimer::GetTimeInMilliseconds() + 5000;
	}

	if (((fwRandom::GetRandomNumber() & 15) == 4) && !bNoWater)
	{		// Try a boat once in a while
		CurrentNode = ThePaths.FindNodeClosestToCoors(popCtrlCentre, 50.0f, CPathFind::IncludeSwitchedOffNodes, false, true);
		CreationDistOnScreen *= ms_creationDistMultExtendedRange;	// Boats get created&deleted further away.
	}
	else
	{		// Mostly cars though
		if (fwRandom::GetRandomNumber() & 3)
		{
			if (bIncludeSwitchedOffNodes)
			{
				CurrentNode = StoredNearestNode_IncludingSwitchedOff;
			}
			else
			{
				CurrentNode = StoredNearestNode;
			}
		}
		else
		{		// Sometimes load the 2nd node and not the closest. (Otherwise we don't get
			// any traffic in the opposite direction of a freeway)
			if (bIncludeSwitchedOffNodes)
			{
				CurrentNode = StoredSecondNearestNode_IncludingSwitchedOff;
			}
			else
			{
				CurrentNode = StoredSecondNearestNode;
			}
		}
	}

	if ( (!CurrentNode.IsEmpty()) && ThePaths.IsRegionLoaded(CurrentNode) && ThePaths.FindNodePointer(CurrentNode)->m_1.m_specialFunction != SPECIAL_SMALL_WORK_VEHICLES)
	{	// We found a good node to start with.
		// At this point we start opening new nodes randomly to move away from the player.
		// We stop when we have either found a usable node pair or we've gone too far.
		CNodeAddress	aNodesFoundSoFar[MAX_DEPTH_FOR_SEARCH];
		aNodesFoundSoFar[0] = CurrentNode;
		s32	NodesFoundSoFar = 1;

		while(NodesFoundSoFar < MAX_DEPTH_FOR_SEARCH && DistanceAlongRoad < (CreationDistOnScreen * 2.0f))
		{	// Keep opening new nodes until we find an appropriate node pair or get too far away.
			if (!ThePaths.IsRegionLoaded(CurrentNode)) return false;

			pCurrentNode = ThePaths.FindNodePointer(CurrentNode);
			s32		NumLinks = pCurrentNode->NumLinks();

			if (!NumLinks)
			{
#if __DEV
				Vector3 crs;
				pCurrentNode->GetCoors(crs);
				Assertf(false, "No neighbours in GenerateVehCreationPosFromPaths %f %f %f", crs.x, crs.y, crs.z);
#endif
				return false;	// We can't find a node. Jump out.
			}

			SemiShuffledSequence shuff(NumLinks);
			bool foundANode = false;
			for (s32 linkI = 0; linkI < NumLinks; linkI++)
			{
				LinkToTest = shuff.GetElement(linkI);

				pLink = &ThePaths.GetNodesLink(pCurrentNode, LinkToTest);
				NewNode = pLink->m_OtherNode;

				if (ThePaths.IsRegionLoaded(NewNode))
				{
					pNewNode = ThePaths.FindNodePointer(NewNode);

					// Check whether this neighbour is in the list yet.
					bool bInList = false;
					for (s32 C = 0; C < NodesFoundSoFar; C++)
					{
						if (aNodesFoundSoFar[C] == NewNode)
						{
							bInList = true;
						}
						else
						{
#if __DEV
							if(CDebugScene::bDisplayVehicleCreationPathsOnVMap)
							{
								Vector3 currentNodePos(0.0f, 0.0f, 0.0f);
								pCurrentNode->GetCoors(currentNodePos);
								currentNodePos.z = 0.0f;

								Vector3 newNodePos(0.0f, 0.0f, 0.0f);
								pNewNode->GetCoors(newNodePos);
								newNodePos.z = 0.0f;

								CVectorMap::DrawLine(
									currentNodePos,
									newNodePos,
									Color32(255,0,0,255),
									Color32(255,0,0,255));
							}
#endif // __DEV
						}
					}

					if (!bInList)
					{		// We have a new node.
						foundANode = true;
						break;
					}
				}
			}

			if(!foundANode)
			{
				return false;	// We didn't find a node. Jump out.
			}

			// Test whether this node pair will do.
			Vector3 currentCoors, newCoors;
			pCurrentNode->GetCoors(currentCoors);
			pNewNode->GetCoors(newCoors);
			Dist1 = (popCtrlCentre - currentCoors).XYMag();
			Dist2 = (popCtrlCentre - newCoors).XYMag();

			bool	bThisPointWillDo = false;

			// Nodes with lower densities are sometimes rejected
			if (!bIgnoreDensity)
			{
				s32	Density = rage::Min(pCurrentNode->m_2.m_density, pNewNode->m_2.m_density);
				if ((fwRandom::GetRandomNumber() & 15) > Density) return false; // Lower density areas (back alleys) get fewer cars generated on them.
			}

			// We're not always interested in switched off nodes
			if ( (!pNewNode->m_2.m_switchedOff && !pCurrentNode->m_2.m_switchedOff) || bIncludeSwitchedOffNodes)
			{
				if ((Dist1 - CreationDistOnScreen) * (Dist2 - CreationDistOnScreen) < 0.0f)
				{		// This would work if the point is on screen.
					Diff1 = ABS(Dist1 - CreationDistOnScreen);
					Diff2 = ABS(Dist2 - CreationDistOnScreen);

					*pResult = (Diff1 * newCoors + Diff2 * currentCoors) / (Diff1 + Diff2);

					if (ms_bInstantFillPopulation || IsSphereVisibleInGameViewport(*pResult, 5.0f))
					{		// This point will do, thanks.
						bThisPointWillDo = true;
						*pFraction = Diff1 / (Diff1 + Diff2);
					}
				}

				if ((!bThisPointWillDo) && ((Dist1 - CreationDistOffScreen) * (Dist2 - CreationDistOffScreen) < 0.0f))
				{		// // This would work if the point is off screen.
					Diff1 = ABS(Dist1 - CreationDistOffScreen);
					Diff2 = ABS(Dist2 - CreationDistOffScreen);

					*pResult = (Diff1 * newCoors + Diff2 * currentCoors) / (Diff1 + Diff2);

					if (ms_bInstantFillPopulation || !IsSphereVisibleInGameViewport(*pResult, 5.0f))
					{		// This point will do, thanks.
						bThisPointWillDo = true;
						*pFraction = Diff1 / (Diff1 + Diff2);
					}
				}
			}

			// Don't create cars on extreme slopes. Asking for trouble.
			if (bThisPointWillDo)
			{
				float	Dist2 = ((Vector3)(currentCoors - newCoors)).XYMag2();
				float	HeightDiff = rage::Abs(currentCoors.z - newCoors.z);
				if (HeightDiff > Dist2 * (0.5f* 0.5f))
				{
					bThisPointWillDo = false;
				}
			}

			//if the user cares about the direction of travel, only search if there are inbound lanes
			//from this link
			if (bThisPointWillDo && !bAllowGoAgainstTraffic && pLink->m_1.m_LanesFromOtherNode == 0)
			{
				bThisPointWillDo = false;
			}


			if (bThisPointWillDo)
			{
				bool bCurrentDirectionIsValidOption = true;
				bool bOppositeDirectionIsValidOption = true;

				if (pLink->m_1.m_LanesToOtherNode == 0 || (pNewNode->m_2.m_deadEndness && pLink->m_1.m_LeadsToDeadEnd))
				{
					bCurrentDirectionIsValidOption = false;
				}

				if (pLink->m_1.m_LanesFromOtherNode == 0)
				{
					bOppositeDirectionIsValidOption = false;
				}
				else if(pLink->m_1.m_LeadsFromDeadEnd)
				{
					bOppositeDirectionIsValidOption = false;
				}

				if (bCurrentDirectionIsValidOption || bOppositeDirectionIsValidOption)
				{
					bool bSwapPoints = false;

					if (!bCurrentDirectionIsValidOption)		// We are forced to swap if our direction isn't valid.
					{
						bSwapPoints = true;
					}
					else if (bOppositeDirectionIsValidOption)	// If both are valid we have option to swap
					{
						bSwapPoints = fwRandom::GetRandomTrueFalse();
					}

					if (bSwapPoints)
					{
						*pFromNode = NewNode;
						*pToNode = CurrentNode;
						*pFraction = 1.0f - *pFraction;
					}
					else
					{
						*pFromNode = CurrentNode;
						*pToNode = NewNode;
					}

					// Make sure the angle is in the right range
					VecToPointX = pResult->x - popCtrlCentre.x;
					VecToPointY = pResult->y - popCtrlCentre.y;
					Length = rage::Sqrtf(VecToPointX * VecToPointX + VecToPointY * VecToPointY);
					VecToPointX /= Length;
					VecToPointY /= Length;
					DotPr = VecToPointX * DirectionX + VecToPointY * DirectionY;
					if ( (bRequiredInside && DotPr > RequiredDotProduct) ||
						((!bRequiredInside) && DotPr <= RequiredDotProduct))
					{
						return true;
					}
				}
				return false;
			}

			// Maintain the distance along the road.
			DistanceAlongRoad += ((Vector3)(newCoors - currentCoors)).XYMag();
			// Add the node to the list of nodes already found.
			aNodesFoundSoFar[NodesFoundSoFar++] = NewNode;
			CurrentNode = NewNode;
		}
	}
	return false;	// No luck.
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddAmbulanceOccupants
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::AddAmbulanceOccupants(CVehicle *pVehicle)
{
	// creation of the driver or passenger can fail in a network game if there is not an available network object for them
	CPed* pDriver = pVehicle->SetUpDriver();
	if(!pDriver)
	{
		vehPopDebugf2("0x%8p - Failed to add driver to ambulance", pVehicle);
		return false;
	}
	CPed* pPassenger = pVehicle->SetupPassenger(1); //Put back in when ambulance is fixed.
	if (!pPassenger)
	{
		vehPopDebugf2("0x%8p - Failed to setup ambulance passenger", pVehicle);
		CPedFactory::GetFactory()->DestroyPed(pDriver);
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddFiretruckOccupants
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::AddFiretruckOccupants(CVehicle *pVehicle)
{
	// creation of the driver or passenger can fail in a network game if there is not an available network object for them
	CPed* pDriver = pVehicle->SetUpDriver();
	if(!pDriver)
	{
		vehPopDebugf2("0x%8p - Failed to add driver to fire truck", pVehicle);
		return false;
	}
	CPed* pPassenger = pVehicle->SetupPassenger(1);
	if (!pPassenger)
	{
		vehPopDebugf2("0x%8p - Failed to setup fire truck passenger", pVehicle);
		CPedFactory::GetFactory()->DestroyPed(pDriver);
		return false;
	}

	pDriver->GetPedIntelligence()->AddTaskDefault(pDriver->ComputeDefaultDrivingTask(*pDriver, pVehicle, false));
	pPassenger->GetPedIntelligence()->AddTaskDefault(pPassenger->ComputeDefaultDrivingTask(*pPassenger, pVehicle, false));
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddGarbageTruckOccupants
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::AddGarbageTruckOccupants(CVehicle *pVehicle)
{
	// creation of the driver can fail in a network game if there is not an available network object for him
	CPed* pDriver = pVehicle->SetUpDriver(true);
	if (!pDriver)
	{
		vehPopDebugf2("0x%8p - Failed to add driver to garbage truck", pVehicle);
		return false;
	}

	CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pDriver, CScriptsForBrains::PED_STREAMED);
	pDriver->GetPedIntelligence()->AddTaskDefault(pDriver->ComputeDefaultDrivingTask(*pDriver, pVehicle, false));

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : VehCanBeRemoved
// PURPOSE :  Returns true if this vehicle would like to be removed.
/////////////////////////////////////////////////////////////////////////////////
bool CVehiclePopulation::VehCanBeRemoved(const CVehicle *const pVeh)
{
	if(!ms_AllowPoliceDeletion)
	{
		// Vehicle would like to be removed if it is a cop car that was told to return to base (and is there now)
		CPed* pDriver = pVeh->GetDriver();
		if ( pDriver && pDriver->GetPedIntelligence()->GetOrder() )
		{
			return false;
		}
	}

	return true;
}

void CVehiclePopulation::AddDrivingFlagsForAmbientVehicle(s32& iDrivingFlags, const bool bIsBike)
{
	//Avoid adverse conditions.
	iDrivingFlags |= DF_AvoidAdverseConditions;

	//bikes will by default avoid cars but obey traffic lights.
	if (bIsBike)
	{
		iDrivingFlags |= DF_SwerveAroundAllCars;
	}

#if __BANK
	if (ms_bEverybodyCruiseInReverse)
	{
		iDrivingFlags |= DF_DriveInReverse;
	}

	if (ms_bDontStopForPeds)
	{
		iDrivingFlags &= ~DF_StopForPeds;
	}

	if (ms_bDontStopForCars)
	{
		iDrivingFlags &= ~DF_StopForCars;
	}
#endif //__BANK
}


CPortalTracker::eProbeType CVehiclePopulation::GetVehicleProbeType(CVehicle *pVehicle)
{
	CPortalTracker *pPortalTracker = pVehicle->GetPortalTracker();

	CPortalTracker::eProbeType probeType = CPortalTracker::PROBE_TYPE_NEAR;
	
	if (pPortalTracker)
	{
		spdAABB box;
		box = pVehicle->GetBoundBox(box);
		const float fNearProbeDist = float(CPortalTracker::NEAR_PROBE_DIST);
		if (box.GetExtent().GetZf() > fNearProbeDist)
		{
			probeType = CPortalTracker::PROBE_TYPE_FAR;
		}
	}

	return probeType;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitWidgets
// PURPOSE :	
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CVehiclePopulation::VehicleMemoryBudgetMultiplierChanged()
{
	if (ms_vehicleMemoryBudgetMultiplier <= 0.25f)
		gPopStreaming.SetVehMemoryBudgetLevel(0);
	else if (ms_vehicleMemoryBudgetMultiplier <= 0.5f)
		gPopStreaming.SetVehMemoryBudgetLevel(1);
	else if (ms_vehicleMemoryBudgetMultiplier <= 0.75)
		gPopStreaming.SetVehMemoryBudgetLevel(2);
	else
		gPopStreaming.SetVehMemoryBudgetLevel(3);
}

void CVehiclePopulation::DisplayMessageHistory()
{
	if (!ms_showDebugLog)
		return;
	
	const u32 padX = 5;
	const u32 padY = 10;

	char str[256];

	for (u32 i = 0; i < 64; ++i)
	{
		u32 idx = (ms_nextDebugMessage - i - 1) % 64;

		if (ms_debugMessageHistory[idx].time + 8000 < fwTimer::GetTimeInMilliseconds())
		{
			break;
		}

		formatf(str, "%s - %s", ms_debugMessageHistory[idx].timeStr, ms_debugMessageHistory[idx].msg);
		grcDebugDraw::PrintToScreenCoors(str, padX, padY + i, Color32(0xFFFFFFFF));
	}
}

void CVehiclePopulation::DebugEventCreateVehicle(CVehicle* veh, CVehicleModelInfo* UNUSED_PARAM(modelInfo))
{
	vehPopDebugf2("Successfully created cargen vehicle 0x%8p (%.2f, %.2f, %.2f), distance: %.2f, type: %s",
		veh, veh->GetTransform().GetPosition().GetXf(), veh->GetTransform().GetPosition().GetYf(), veh->GetTransform().GetPosition().GetZf(),
		(VEC3V_TO_VECTOR3(veh->GetTransform().GetPosition()) - CGameWorld::FindLocalPlayerCoors()).Mag(), veh->GetModelName());
}

void OverrideArchetypeNameChange()
{
	gPopStreaming.SetVehicleOverrideIndex(CModelInfo::GetModelIdFromName(CVehiclePopulation::ms_overrideArchetypeName).GetModelIndex());
}

void CVehiclePopulation::SetVehGroupOverride()
{
	if(CPopCycle::HasValidCurrentPopAllocation())
	{
		u32 vehGroup;
		if(CPopCycle::GetPopGroups().FindVehGroupFromNameHash(atHashValue(ms_VehGroupOverride), vehGroup))
			(const_cast<CPopSchedule*>(&CPopCycle::GetCurrentPopSchedule()))->SetOverrideVehGroup(vehGroup, ms_VehGroupOverridePercentage);
	}
}

/////////////////////////////////////////////////////////////////////////////////

void CVehiclePopulation::InitWidgets()
{
	InitVehicleSpacings();

	// We should have a value for this at this point.
	Assertf(ms_maxNumberOfCarsInUse, "ms_maxNumberOfCarsInUse not set early enough.");

	// Set up some bank stuff.
	bkBank &bank = BANKMGR.CreateBank("Veh Population");

	bank.AddButton("Remove All Vehs Hard",										datCallback(RemoveAllVehsHard));
	bank.AddButton("Remove All Vehs Soft",										datCallback(RemoveAllVehsSoft));
	bank.AddButton("Remove All Vehs Soft (Except Focus)",						datCallback(RemoveAllVehsSoftExceptFocus));
	bank.AddButton("remove All Vehs Hard (Except Near Focus)",					datCallback(RemoveAllVehsHardExceptNearFocus));
	bank.AddButton("Remove Focus Vehicle Soft",									datCallback(RemoveFocusVehicleSoft));
	bank.AddButton("Remove all veh models hard",								CPopulationStreaming::FlushAllVehicleModelsHard);
	bank.AddButton("Dump Vehicle Pool To Log",									DumpVehiclePoolToLog);

	bank.AddToggle("Toggle Allow Veh Pop to Add and Remove",					&gbAllowVehGenerationOrRemoval);
	bank.AddToggle("Toggle Allow Cops to Add and Remove",						&gbAllowCopGenerationOrRemoval);
	bank.AddToggle("Toggle Allow Trains to Add and Remove",						&gbAllowTrainGenerationOrRemoval);
	bank.AddSlider("Vehicle Population Frame Rate", &ms_iVehiclePopulationFrameRate, 1, 60, 1, UpdateMaxVehiclePopulationCyclesPerFrame);
	bank.AddSlider("Vehicle Population Cycles Per Frame", &ms_iVehiclePopulationCyclesPerFrame, 1, 10, 1, UpdateMaxVehiclePopulationCyclesPerFrame);
	bank.AddSlider("Vehicle Population Frame Rate MP", &ms_iVehiclePopulationFrameRateMP, 1, 60, 1, UpdateMaxVehiclePopulationCyclesPerFrame);
	bank.AddSlider("Vehicle Population Cycles Per Frame MP", &ms_iVehiclePopulationCyclesPerFrameMP, 1, 10, 1, UpdateMaxVehiclePopulationCyclesPerFrame);
	//-------------------------------------------------------------------------------------------------------------------

	bank.PushGroup("Debug Tools", true);

	
		bank.AddToggle("Log created vehicles",									&CVehicleFactory::ms_bLogCreatedVehicles);
		bank.AddToggle("Log destroyed vehicles",								&CVehicleFactory::ms_bLogDestroyedVehicles);
		bank.AddToggle("Catch vehicles removed in view",						&ms_bCatchVehiclesRemovedInView);
		bank.AddSlider("In view distance threshold",							&ms_fInViewDistanceThreshold, 0.0f, 1000.0f, 1.0f);
		bank.AddButton("Dump vehicle memory info",								datCallback(DumpVehicleMemoryInfoCB));
		bank.AddToggle("Print car population debug",							&CPopCycle::sm_bDisplayCarDebugInfo);
		bank.AddToggle("Enable vehPopDebug output",								&CPopCycle::sm_bEnableVehPopDebugOutput);
		bank.AddToggle("Print Simple car population debug",					    &CPopCycle::sm_bDisplayCarSimpleDebugInfo);
		bank.AddToggle("Enable vehPopDebug output",								&CPopCycle::sm_bEnableVehPopDebugOutput);
		bank.AddToggle("Log vehicle creation debug",                            NetworkDebug::GetLogExtraDebugNetworkPopulation());
		bank.AddToggle("Display car population failure counts",					&CPopCycle::sm_bDisplayCarPopulationFailureCounts);
		bank.AddToggle("Display vehicle spacings",								&ms_bDisplayVehicleSpacings);
		bank.AddToggle("Ignore on-screen removal delay",						&ms_ignoreOnScreenRemovalDelay);
		bank.AddToggle("Display inner keyhole disparities",						&ms_displayInnerKeyholeDisparities);

		bank.AddToggle("Enable destroyed vehicle cache",						&CVehicleFactory::ms_reuseDestroyedVehs);
		bank.AddToggle("Print destroyed vehicle cache",							&CVehicleFactory::ms_reuseDestroyedVehsDebugOutput);
		bank.AddToggle("Reset cache stats",										&CVehicleFactory::ms_resetCacheStats);

		bank.AddToggle("Move Population Control With Debug Cam",				&ms_movePopulationControlWithDebugCam);

		bank.AddToggle("Display vehicles (on VectorMap)",						&CDebugScene::bDisplayVehiclesOnVMap);
		bank.AddToggle("Display vehicles lod (on VectorMap)",					&CVehicleAILodManager::ms_bVMLODDebug, datCallback(CVehicleAILodManager::UpdateDebugLogDisplay));
		bank.AddToggle("Display vehicles active/inactive ",			&CDebugScene::bDisplayVehiclesOnVMapAsActiveInactive);
		bank.AddToggle("Display vehicles uses fade (on VectorMap)",				&CDebugScene::bDisplayVehiclesUsesFadeOnVMap);
		bank.AddToggle("Display Add/Cull zones (on VM)",						&ms_displayAddCullZonesOnVectorMap);
		bank.AddToggle("Display zone category grid (VM)",						&ms_displaySpawnCategoryGrid);
		bank.AddToggle("Display veh pop failed create events (on VM)",			&CDebugScene::bDisplayVehPopFailedCreateEventsOnVMap);
		bank.AddToggle("Display veh pop create events (on VM)",					&CDebugScene::bDisplayVehPopCreateEventsOnVMap);
		bank.AddToggle("Display veh pop destroy events (on VM)",				&CDebugScene::bDisplayVehPopDestroyEventsOnVMap);
		bank.AddToggle("Display veh pop conversion events (on VM)", 			&CDebugScene::bDisplayVehPopConversionEventsOnVMap);
		bank.AddToggle("Display veh creation links (on VM)",					&CDebugScene::bDisplayVehGenLinksOnVM);
		bank.AddToggle("Display veh creation paths (on VM)",					&CDebugScene::bDisplayVehicleCreationPathsOnVMap);
		bank.AddToggle("Display veh creation paths (in world)",					&CDebugScene::bDisplayVehicleCreationPathsInWorld);
		bank.AddToggle("Display veh creation paths curr density (on VM)",		&CDebugScene::bDisplayVehicleCreationPathsCurrDensityOnVMap);
		bank.AddToggle("Display vehs to stream out (on VM)",					&CDebugScene::bDisplayVehiclesToBeStreamedOutOnVMap);
		bank.AddButton("Focus Veh Force to Dummy",								datCallback(SwitchVehToDummyCB));
		bank.AddButton("Focus Veh Force to Real",								datCallback(SwitchVehToRealCB));
		bank.AddButton("Focus Veh Force to SuperDummy",							datCallback(SwitchVehSuperDummyCB));
		bank.AddButton("Wreck Focus Vehicle",									datCallback(WreckFocusVehicleCB));
		bank.AddButton("Wake up all parked cars",								datCallback(WakeUpAllParkedVehiclesCB));
		bank.AddToggle("Display Vehicle Dir and Path",							&ms_displayVehicleDirAndPath);

		bank.PushGroup("VehGroup override", true);
			bank.AddText("VehGroup override", ms_VehGroupOverride, sizeof(ms_VehGroupOverride), false);
			bank.AddSlider("Percentage override", &ms_VehGroupOverridePercentage, 0, 100, 1);
			bank.AddButton("Override", datCallback(SetVehGroupOverride));
		bank.PopGroup();

		bank.AddText("Override archetype", ms_overrideArchetypeName, sizeof(ms_overrideArchetypeName), false, &OverrideArchetypeNameChange);

		bank.AddToggle("Display Add/Cull zones (in World)",						&ms_displayAddCullZonesInWorld);
		bank.AddSlider("Add/Cull zone height offset (in World)",				&ms_addCullZoneHeightOffset, -500.0f, 500.0f, 1.0f);
		bank.AddToggle("Freeze keyhole shape",									&ms_freezeKeyholeShape);
		bank.AddToggle("Display heavy-duty vehgen debugging!",					&ms_bHeavyDutyVehGenDebugging);
		bank.AddToggle("Display boxes in world for vehicle spawn attempts",		&ms_bDrawBoxesForVehicleSpawnAttempts);
		bank.AddSlider("Display boxes duration (frames)",						&ms_iDrawBoxesForVehicleSpawnAttemptsDuration, 0, 60000, 1);
		bank.AddToggle("Everybody cruise in reverse",							&ms_bEverybodyCruiseInReverse);
		bank.AddToggle("Don't stop for peds",									&ms_bDontStopForPeds);
		bank.AddToggle("Don't stop for cars",									&ms_bDontStopForCars);

		bank.AddButton("Instant fill population",								datCallback(InstantFillPopulation));
		bank.AddSlider("Instant fill extra population cycles",					&ms_fInstantFillExtraPopulationCycles, 0.0f, 20.0f, 1.0f);

		bank.AddSlider("Chance to spawn same vehicle in a row",					&ms_spawnLastChosenVehicleChance, 0.f, 1.f, 0.01f);
		bank.AddToggle("Draw excessive removal MP",								&ms_bDrawMPExcessiveDebug);

		bank.PushGroup("Clear From Area", true);
			bank.AddButton("Clear Cars From Area",								datCallback(ClearCarsFromArea));
			bank.AddVector("Area center position",								&ms_vClearAreaCenterPos, 0.0f, 0.0f, 0.0f);
			bank.AddSlider("Radius",											&ms_fClearAreaRadius, 0.f, 1000.f, 1.0f);
			bank.AddToggle("Check Interesting",									&ms_bClearAreaCheckInteresting);
			bank.AddToggle("Leave Car Gen Cars",								&ms_bClearAreaLeaveCarGenCars);
			bank.AddToggle("Check View Frustum",								&ms_bClearAreaCheckFrustum);
			bank.AddToggle("Clear If Wrecked",									&ms_bClearAreaWrecked);
			bank.AddToggle("Clear If Abandoned",								&ms_bClearAreaAbandoned);
			bank.AddToggle("Clear If Engine On Fire",							&ms_bClearAreaEngineOnFire);
			bank.AddToggle("Allow Cop Removal With Wanted Level",				&ms_bClearAreaAllowCopRemovealWithWantedLevel);
		bank.PopGroup();

	bank.PopGroup();


	bank.PushGroup("Car Generators", false);
		bank.AddToggle("Render Car Generators",						&CTheCarGenerators::gs_bDisplayCarGenerators, NullCB);
		bank.AddToggle("Render Car Generators AoI",					&CTheCarGenerators::gs_bDisplayAreaOfInterest, NullCB);
		bank.AddToggle("Render Scenario Car Generators Only",		&CTheCarGenerators::gs_bDisplayCarGeneratorsScenariosOnly);
		bank.AddToggle("Render Preassigned Car Generators Only",	&CTheCarGenerators::gs_bDisplayCarGeneratorsPreassignedOnly);
		bank.AddSlider("Render Car Generators Range",				&CTheCarGenerators::gs_fDisplayCarGeneratorsDebugRange, 10.0f, 1000.0f, 1.0f);
		bank.AddButton("Dump Car Generator Info",					datCallback(CFA(CTheCarGenerators::DumpCargenInfo)));
		bank.AddSlider("Force Spawn Car Radius",					&CTheCarGenerators::gs_fForceSpawnCarRadius, 10.0f, 1000.0f, 1.0f);
		bank.AddToggle("Check Probes When Force Spawn",				&CTheCarGenerators::gs_bCheckPhysicProbeWhenForceSpawn);
		bank.AddButton("Force Car Gen",								datCallback(CFA(CTheCarGenerators::ForceSpawnCarsWithinRadius)));

		CTheCarGenerators::InitWidgets(&bank);
	bank.PopGroup();


	bank.PushGroup("Spawn history", false);
		bank.AddToggle("Display spawn history",									&ms_bDisplaySpawnHistory);
		bank.AddButton("Clear spawn history",									ClearSpawnHistory);
		bank.AddToggle("Draw arrows to vehicles",								&ms_bSpawnHistoryDisplayArrows);
		bank.AddToggle("Show vehicle creation methods",							&ms_bDisplayVehicleCreationMethod);
		bank.AddToggle("Show vehicle creation costs",							&ms_bDisplayVehicleCreationCost);
		bank.AddSlider("Vehicle creation cost highlight threshold",				&ms_iVehicleCreationCostHighlightThreshold, 1, 10000, 1);
	bank.PopGroup();

	bank.PushGroup("Vehicle Reuse");		
		bank.AddToggle("Use Vehicle Reuse Pool",					&ms_bVehicleReusePool);
		bank.AddToggle("Debug Vehicle Reuse Pool",				&ms_bDebugVehicleReusePool);
		bank.AddToggle("Display Vehicle Reuse Pool",				&ms_bDisplayVehicleReusePool);		
		bank.AddSlider("Vehicle Reuse Timeout",					&ms_ReusedVehicleTimeout, 1, 240, 1);
		bank.AddButton("Flush Reuse Pool",						datCallback(CFA(FlushVehicleReusePool)));
		bank.AddToggle("Vehicle Reuse in MP",					&ms_bVehicleReuseInMP);
		bank.AddToggle("Check HasBeenSeen",						&ms_bVehicleReuseChecksHasBeenSeen);		
	bank.PopGroup();

	bank.PushGroup("Vehicle Creation Control", false);

		bank.PushGroup("Spawn constraints");
		bank.AddToggle("Do relative movement check",				&ms_spawnDoRelativeMovementCheck);
		bank.AddToggle("Care about flow of traffic",				&ms_spawnCareAboutLanes);
		bank.AddToggle("Care about repeating same model",			&ms_spawnCareAboutRepeatingSameModel);
		bank.AddToggle("Enable single-track rejection",				&ms_spawnEnableSingleTrackRejection);
		bank.AddToggle("Don't spawn on dead-ends",					&ms_spawnCareAboutDeadEnds);
		bank.AddToggle("Care about placing on road properly",		&ms_spawnCareAboutPlacingOnRoadProperly);
		bank.AddToggle("Do box collision test",						&ms_spawnCareAboutBoxCollisionTest);
		bank.AddToggle("Care about appropriate vehicle models",		&ms_spawnCareAboutAppropriateVehicleModels);
		bank.AddToggle("Care about frustum check",					&ms_spawnCareAboutFrustumCheck);
		bank.AddSlider("Skip frustum check height",					&ms_fSkipFrustumCheckHeight, 0.0f, 300.0f, 1.0f);

	bank.PopGroup();

		bank.AddSlider("Max number of cars in use",					&ms_maxNumberOfCarsInUse, 0, ms_maxNumberOfCarsInUse, 1);
		bank.AddSlider("Max generation height diff",				&ms_maxGenerationHeightDiff, 0.0f, 100.0f, 1.0f);
		bank.AddToggle("Allow spawning from dead end roads",		&ms_allowSpawningFromDeadEndRoads);
		
		bank.AddSlider("Vehicle memory budget multiplier",			&ms_vehicleMemoryBudgetMultiplier, 0.f, 1.f, 0.25f, VehicleMemoryBudgetMultiplierChanged);
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)		
		bank.AddSlider("Vehicle memory budget multiplier NG", &ms_vehicleMemoryBudgetMultiplierNG, 0.f, 10.f, 0.05f, VehicleMemoryBudgetMultiplierChanged);
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
		
		bank.AddSlider("Override Number of Cars",					&ms_overrideNumberOfCars, -1, 1000, 1);
		bank.AddSlider("Random Car Density Multiplier",				&ms_randomVehDensityMultiplier, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("Parked Car Density Multiplier",				&ms_parkedCarDensityMultiplier, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("Low Priority Parked Car Density Multiplier",	&ms_lowPrioParkedCarDensityMultiplier, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("Override Number of Parked Cars",			&ms_overrideNumberOfParkedCars, -1, 1000, 1);
		bank.AddToggle("Generate cars waiting at lights",			&ms_bGenerateCarsWaitingAtLights);
		bank.AddToggle("Only when lights are red",					&ms_bGenerateCarsWaitingAtLightsOnlyAtRed);
		bank.AddSlider("Number of cars created at lights",			&ms_iMaxCarsForLights, 0, 5, 1);

		bank.PushGroup("Algorithm params, etc");

			bank.AddSlider("Ambient vehicles upper limit",			&ms_iAmbientVehiclesUpperLimit, 0, 200, 1);
			bank.AddSlider("Ambient vehicles upper limit (MP)",			&ms_iAmbientVehiclesUpperLimitMP, 0, 200, 1);
			bank.AddSlider("Parked vehicles upper limit",				&ms_iParkedVehiclesUpperLimit, 0, 300, 1);
			bank.AddSlider("Density Sampling Vertical Falloff Start",		&ms_popGenDensitySamplingVerticalFalloffStart,		0.0f, 200.0f, 5.0f);
			bank.AddSlider("Density Sampling Vertical Falloff End",		&ms_popGenDensitySamplingVerticalFalloffEnd,		0.0f, 200.0f, 5.0f);
			bank.AddSlider("DensitySamplingMinSummedDesiredNumVehs",&ms_popGenDensitySamplingMinSummedDesiredNumVehs ,	0.0f, 20.0f, 0.1f);
			bank.AddSlider("Density Max Extended Sampling Dist",	&ms_popGenDensitySamplingMaxExtendedDist,			0.0f, 200.0f, 5.0f);
			bank.AddSlider("Standard Vehs Per Meter Per LaneIndex",		&ms_popGenStandardVehsPerMeterPerLane,				0.0001f, 100.0f, 0.0001f);
			bank.AddToggle("Traffic jam management",			&ms_bUseTrafficJamManagement);
		#if __DEV
			bank.AddSlider("Density Max Expected Vehs Near Link",	&ms_popGenDensityMaxExpectedVehsNearLink,			1.0f, 100.0f, 1.0f);
		#endif // __DEV
			bank.AddSlider("Min generation score",					&ms_fMinBaseGenerationScore, -5.0f, 5.0f, 0.1f);
			bank.AddSlider("Active link update freq",				&ms_iActiveLinkUpdateFreqMs, 0, 10000, 1);
			bank.AddToggle("Area Search links must match vehicle heading",  &ms_bSearchLinksMustMatchVehHeading);
			bank.AddSlider("Link attempt penalty multiplier",	&ms_fLinkAttemptPenaltyMutliplier, 0.0f, 10.0f, 1);
			bank.AddSlider("Add random link density",			&ms_popGenRandomizeLinkDensity, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Outer band link score boost",		&ms_popGenOuterBandLinkScoreBoost, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Scripted Range Multiplier",		&ms_scriptedRangeMultiplier, 0.0f, 32.0f, 0.1f);
			bank.AddSlider("Near Dist LOD Scale Threshold",		&ms_fNearDistLodScaleThreshold, 0.0f, 4.0f, 0.1f);
		bank.PopGroup();

		bank.PushGroup("Player's road tweaks");
			bank.AddSlider("Scale down vehicle-per-meter for non player's road", &ms_fVehiclePerMeterScaleForNonPlayersRoad, 0.0f, 1.0f, 0.05f);
			bank.AddSlider("Player's road density prioritization threshold", &ms_fOnPlayersRoadPrioritizationThreshold, 0.0f, 1.0f, 0.05f);
			bank.AddToggle("Use player's road density modifier", &ms_bUsePlayersRoadDensityModifier);
			bank.PushGroup("Increments for *on* player's road:", false);
				bank.AddSlider("Density 1 (+)", &ms_iPlayerRoadDensityInc[1], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 2 (+)", &ms_iPlayerRoadDensityInc[2], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 3 (+)", &ms_iPlayerRoadDensityInc[3], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 4 (+)", &ms_iPlayerRoadDensityInc[4], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 5 (+)", &ms_iPlayerRoadDensityInc[5], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 6 (+)", &ms_iPlayerRoadDensityInc[6], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 7 (+)", &ms_iPlayerRoadDensityInc[7], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 8 (+)", &ms_iPlayerRoadDensityInc[8], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 9 (+)", &ms_iPlayerRoadDensityInc[9], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 10 (+)", &ms_iPlayerRoadDensityInc[10], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 11 (+)", &ms_iPlayerRoadDensityInc[11], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 12 (+)", &ms_iPlayerRoadDensityInc[12], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 13 (+)", &ms_iPlayerRoadDensityInc[13], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 14 (+)", &ms_iPlayerRoadDensityInc[14], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 15 (+)", &ms_iPlayerRoadDensityInc[15], 0, 15, 1, ModifyPlayersRoadModifiers);
			bank.PopGroup();

			bank.PushGroup("Decrements for *not* on player's road:", false);
				bank.AddSlider("Density 1 (-)", &ms_iNonPlayerRoadDensityDec[1], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 2 (-)", &ms_iNonPlayerRoadDensityDec[2], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 3 (-)", &ms_iNonPlayerRoadDensityDec[3], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 4 (-)", &ms_iNonPlayerRoadDensityDec[4], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 5 (-)", &ms_iNonPlayerRoadDensityDec[5], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 6 (-)", &ms_iNonPlayerRoadDensityDec[6], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 7 (-)", &ms_iNonPlayerRoadDensityDec[7], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 8 (-)", &ms_iNonPlayerRoadDensityDec[8], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 9 (-)", &ms_iNonPlayerRoadDensityDec[9], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 10 (-)", &ms_iNonPlayerRoadDensityDec[10], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 11 (-)", &ms_iNonPlayerRoadDensityDec[11], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 12 (-)", &ms_iNonPlayerRoadDensityDec[12], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 13 (-)", &ms_iNonPlayerRoadDensityDec[13], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 14 (-)", &ms_iNonPlayerRoadDensityDec[14], 0, 15, 1, ModifyPlayersRoadModifiers);
				bank.AddSlider("Density 15 (-)", &ms_iNonPlayerRoadDensityDec[15], 0, 15, 1, ModifyPlayersRoadModifiers);
			bank.PopGroup();

			bank.AddToggle("Use traffic density modifier", &ms_bUseTrafficDensityModifier);		
			bank.AddToggle("Display traffic density modifier in world",  &ms_displayTrafficCenterInWorld);
			bank.AddToggle("Display traffic density modifier on VM",  &ms_displayTrafficCenterOnVM);
			bank.PushGroup("Traffic density modifiers", false);
				bank.AddSlider("Min ambient vehicles", &ms_iMinAmbientVehiclesForThrottle, 0, 100, 1);
				bank.AddSlider("Min ambient stationary ratio", &ms_fMinRatioForTrafficThrottle, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Max ambient stationary ratio", &ms_fMaxRatioForTrafficThrottle, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Min distance from traffic center", &ms_fMinDistanceForTrafficThrottle, 0.0f, 1000.0f, 1.0f);
				bank.AddSlider("Max distance from traffic center", &ms_fMaxDistanceForTrafficThrottle,0.0f, 1000.0f, 1.0f);
			bank.PopGroup();

			bank.AddSlider("Flood-fill scan distance", &ms_fPlayersRoadScanDistance, 0.0f, 2000.0f, 1.0f);

			bank.PushGroup("Player's road change-in-direction penalties (junctions)");

				char buffer[16];

				for(u32 i = 0; i < NUM_PLAYERS_ROAD_CID_PENALTIES; ++i)
				{
					sprintf(buffer, "Angle %u", i);
					bank.AddSlider(buffer, &ms_fPlayersRoadChangeInDirectionPenalties[i][PLAYERS_ROAD_CID_ANGLE_DEGREES], 0.0f, 180.0f, 0.5f, ModifyPlayersRoadModifiers);
					sprintf(buffer, "Penalty %u", i);
					bank.AddSlider(buffer, &ms_fPlayersRoadChangeInDirectionPenalties[i][PLAYERS_ROAD_CID_PENALTY], 0.0f, 300.0f, 0.5f, ModifyPlayersRoadModifiers);
				}

			bank.PopGroup();

		bank.PopGroup();

		bank.PushGroup("Default desired spacing between vehicles");

			bank.AddSlider("Density 1", &ms_fDefaultVehiclesSpacing[1], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 2", &ms_fDefaultVehiclesSpacing[2], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 3", &ms_fDefaultVehiclesSpacing[3], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 4", &ms_fDefaultVehiclesSpacing[4], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 5", &ms_fDefaultVehiclesSpacing[5], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 6", &ms_fDefaultVehiclesSpacing[6], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 7", &ms_fDefaultVehiclesSpacing[7], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 8", &ms_fDefaultVehiclesSpacing[8], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 9", &ms_fDefaultVehiclesSpacing[9], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 10", &ms_fDefaultVehiclesSpacing[10], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 11", &ms_fDefaultVehiclesSpacing[11], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 12", &ms_fDefaultVehiclesSpacing[12], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 13", &ms_fDefaultVehiclesSpacing[13], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 14", &ms_fDefaultVehiclesSpacing[14], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSlider("Density 15", &ms_fDefaultVehiclesSpacing[15], 1.0f, 1000.0f, 0.1f, ModifyVehicleSpacings);
			bank.AddSeparator();
			bank.AddButton("Scale all down 10%", OnScaleDownDefaultVehicleSpacings);
			bank.AddButton("Scale all up 10%", OnScaleUpDefaultVehicleSpacings);
			bank.AddToggle("Display vehicle spacings", &ms_bDisplayVehicleSpacings);
			bank.AddButton("Reset to defaults", InitVehicleSpacings);
		bank.PopGroup();

	bank.PopGroup();

	bank.PushGroup("Vehicle Add/Cull Ranges/Sizes", false);
		bank.AddSlider("All Zone Scaler",							&ms_allZoneScaler,	0.0f, 10.0f, 0.05f);
		bank.AddSlider("Range Scale In Shallow Interior",			&ms_rangeScaleShallowInterior,	0.0f, 10.0f, 0.05f);
		bank.AddSlider("Range Scale In Deep Interior",				&ms_rangeScaleDeepInterior,	0.0f, 10.0f, 0.05f);
		bank.AddSlider("Keyhole Shape Inner Thickness",				&ms_popGenKeyholeInnerShapeThickness,					1.0f, 1000.0f, 5.0f);
		bank.AddSlider("Keyhole Shape Outer Thickness",				&ms_popGenKeyholeOuterShapeThickness,					1.0f, 1000.0f, 5.0f);
		bank.AddSlider("Keyhole Shape Inner Radius",				&ms_popGenKeyholeShapeInnerBandRadius,					1.0f, 1000.0f, 5.0f);
		bank.AddSlider("Keyhole Shape Outer Radius",				&ms_popGenKeyholeShapeOuterBandRadius,					1.0f, 1000.0f, 5.0f);
		bank.AddSlider("Keyhole Shape Side Wall Thickness",			&ms_popGenKeyholeSideWallThickness,						1.0f, 1000.0f, 5.0f);
		bank.AddSlider("Max creation distance",						&ms_creationDistance, 0.0f, 500.0f, 5.0f);
		bank.AddSlider("Max creation distance (off-screen)",		&ms_creationDistanceOffScreen, 0.0f, 500.0f, 5.0f);
		bank.AddSlider("Creation Dist Mult Extended Range",			&ms_creationDistMultExtendedRange,	0.0f, 10.0f, 0.1f);
		bank.AddSlider("Spawn frustum near distance multiplier",	&ms_spawnFrustumNearDistMultiplier,	0.0f, 1.0f, 0.05f);
		bank.AddSlider("Cull Range",								&ms_cullRange,	0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Cull Range OnScreen Scale",					&ms_cullRangeOnScreenScale,	1.0f, 2.0f, 0.01f);
		bank.AddSlider("Cull Range OffScreen",						&ms_cullRangeOffScreen,	0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Call Range Behind Player When Zoomed",		&ms_cullRangeBehindPlayerWhenZoomed, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Cull Range OffScreen Wrecked",				&ms_cullRangeOffScreenWrecked, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Aircraft spawn range mult (SP)",			&ms_fAircraftSpawnRangeMult, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Aircraft spawn range mult (MP)",			&ms_fAircraftSpawnRangeMultMP, 0.0f, 10.0f, 0.01f);
		bank.AddToggle("Increase Spawn Range Based On Height",		&ms_bIncreaseSpawnRangeBasedOnHeight);
//		bank.AddSlider("Height Band Start Z",						&ms_fHeightBandStartZ, -1000.0f, 1000.0f, 1.0f);
//		bank.AddSlider("Height Band End Z",							&ms_fHeightBandEndZ, -1000.0f, 1000.0f, 1.0f);
//		bank.AddSlider("Height Based Range Multipler",				&ms_fHeightBandMultiplier, 0.0f, 10.0f, 1.0f);
		bank.AddToggle("Rnablr use gather links keyhole",			&ms_bEnableUseGatherLinksKeyhole);
		bank.AddSlider("Gather Links using keyhole range threshold",&ms_fGatherLinksUseKeyHoleShapeRangeThreshold, 1.0f, 10.0f, 0.1f);
		bank.AddSlider("Maximum range scale",						&ms_fMaxRangeScale,	1.0f, 20.0f, 0.1f);		
		
		bank.AddSlider("Population Sphere Radius", &ms_TempPopControlSphereRadius, 0.f,400.f,1.f);		
	bank.PopGroup();

	bank.PushGroup("Vehicle Pop Look Ahead", false);
		bank.AddToggle("Enable Look Ahead",					&ms_vehiclePopGenLookAhead);
	bank.PopGroup();

	bank.PushGroup("Veh Removal Delays", false);
		bank.AddSlider("Speed Based Removal Rate Scale",		&ms_maxSpeedBasedRemovalRateScale,			0.0f, 40.0f, 0.05f);
		bank.AddSlider("Speed Max Expected",					&ms_maxSpeedExpectedMax,					0.0f, 500.0f, 5.0f);
		bank.AddSlider("Density Based Removal Rate Scale",		&ms_densityBasedRemovalRateScale,			0.0f, 40.0f, 0.05f);
		bank.AddSlider("Density Based Removal Target Headroom", &ms_densityBasedRemovalTargetHeadroom,		0, 30, 1);
		bank.AddSlider("Removal Delay Ms Refresh Min",			&ms_vehicleRemovalDelayMsRefreshMin,		0, 30000, 100);
		bank.AddSlider("Removal Delay Ms Refresh Max",			&ms_vehicleRemovalDelayMsRefreshMax,		0, 30000, 100);
		bank.AddSlider("Clear interesting vehicle delay SP",	&ms_clearInterestingVehicleDelaySP,			0, (1000 * 60 * 15), 1000);
		bank.AddSlider("Clear interesting vehicle delay MP",	&ms_clearInterestingVehicleDelayMP,			0, (1000 * 60 * 15), 1000);
		bank.AddSeparator();
		bank.AddToggle("Extend delay within outer radius",		&ms_extendDelayWithinOuterRadius);
		bank.AddSlider("Extend delay time (ms)",				&ms_extendDelayWithinOuterRadiusTime, 0, 60000, 1000);
		bank.AddSlider("Extend delay extra dist",				&ms_extendDelayWithinOuterRadiusExtraDist, -100.0f, 100.0f, 1.0f);
	bank.PopGroup();

	bank.PushGroup("Zones", false);
		bank.AddToggle("Render Zones",								&CCullZones::bRenderCullzones, NullCB);
		bank.AddSlider("Num Cam Zones",								&CCullZones::m_numCamZones, 0, 9000000, 0);
	bank.PopGroup();

	CSlownessZoneManager::InitWidgets(bank);

	bank.PushGroup("Markup", false);
		bank.AddToggle("Display markup spheres", &ms_bRenderMarkupSpheres);
		ms_MarkUpEditor.m_pNumSpheresText = bank.AddText("Num Spheres:", &ms_MarkUpEditor.m_iNumSpheres);
		ms_MarkUpEditor.m_pCurrentSphereSlider = bank.AddSlider("Current sphere", &ms_MarkUpEditor.m_iCurrentSphere, -1, 0, 1, ms_MarkUpEditor.OnChangeCurrentSphere);
		ms_MarkUpEditor.m_pXPosSlider = bank.AddSlider("X pos", &ms_MarkUpEditor.m_iCurrentSphereX, -8192, 8192, 1, ms_MarkUpEditor.OnModifyCurrentSphereData);
		ms_MarkUpEditor.m_pYPosSlider = bank.AddSlider("Y pos", &ms_MarkUpEditor.m_iCurrentSphereY, -8192, 8192, 1, ms_MarkUpEditor.OnModifyCurrentSphereData);
		ms_MarkUpEditor.m_pZPosSlider = bank.AddSlider("Z pos", &ms_MarkUpEditor.m_iCurrentSphereZ, -8192, 8192, 1, ms_MarkUpEditor.OnModifyCurrentSphereData);
		ms_MarkUpEditor.m_pRadiusSlider = bank.AddSlider("Radius", &ms_MarkUpEditor.m_iCurrentSphereRadius, 1, 255, 1, ms_MarkUpEditor.OnModifyCurrentSphereData);
		ms_MarkUpEditor.m_pRoadsOverlapToggle = bank.AddToggle("Flag_RoadsOverlap", &ms_MarkUpEditor.m_bCurrentSphere_FlagRoadsOverlap, ms_MarkUpEditor.OnModifyCurrentSphereData);

		bank.AddSeparator();
		bank.AddButton("Select closest to camera", ms_MarkUpEditor.OnSelectClosestToCamera);
		
		bank.PushGroup("New/Delete", false);
			bank.AddButton("New", ms_MarkUpEditor.OnAddNewSphere);
			bank.AddButton("Delete", ms_MarkUpEditor.OnDeleteCurrentSphere);
		bank.PopGroup();

		bank.PushGroup("Save..", false);
			bank.AddButton("Save markup xml", SaveMarkupSpheres);
		bank.PopGroup();
	bank.PopGroup();

	bkBank* pBank = BANKMGR.FindBank("Optimization");
	if(pBank)
	{
		pBank->AddSlider("Debug Override Car Density Multiplier",	&ms_debugOverrideVehDensityMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Random Car Density Multiplier",			&ms_randomVehDensityMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Parked Car Density Multiplier",			&ms_parkedCarDensityMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Low Priority Parked Car Density Multiplier",	&ms_lowPrioParkedCarDensityMultiplier, 0.0f, 10.0f, 0.1f);
	}

	bank.AddToggle("Show Debug Log", &ms_showDebugLog, NullCB);
}

const char *CVehiclePopulation::GetLastRemovalReason()
{
    switch(ms_lastRemovalReason)
    {
    case Removal_None:
        return "Not removed by vehicle population";
    case Removal_OutOfRange:
        return "Out of range of any player";
	case Removal_OutOfRange_OnScreen:
		return "Out of range of any player (OnScreen)";
	case Removal_OutOfRange_Parked:
		return "Out of range of any player (Parked)";
	case Removal_OutOfRange_NoPeds:
		return "Out of range of any player (NoPeds)";
	case Removal_OutOfRange_BehindZoomedPlayer:
		return "Out of range of any player (BehindZoomedPlayer)";
	case Removal_OutOfRange_OuterBand:
		return "Out of range of any player (OuterBand)";
    case Removal_StuckOffScreen:
        return "Stuck for too long on screen";
    case Removal_StuckOnScreen:
        return "Stuck for too long off screen";
    case Removal_LongQueue:
        return "In long queue, never seen/not seen 30s";
    case Removal_WreckedOffScreen:
        return "Wrecked and off screen";
	case Removal_EmptyCopOrWrecked:
		return "Empty cop car or wrecked";
	case Removal_EmptyCopOrWrecked_Respawn:
		return "Empty cop car or wrecked (Respawn)";
    case Removal_NetGameCongestion:
        return "Congestion in network game";
	case Removal_NetGameEmptyCrowdingPlayers:
		return "Empty crowding players in network game";
    case Removal_OccupantAddFailed:
        return "Failed to add occupants";
    case Removal_BadTrailerType:
        return "Bad trailer type created";
    case Removal_Debug:
        return "Debug";
    case Removal_RemoveAllVehicles:
        return "Removing all vehicles";
    case Removal_ExcessNetworkVehicle:
        return "Excess network vehicle";
	case Removal_RemoveAggressivelyFlag:
		return "Remove aggressively flag";
	case Removal_KickedFromVehicleReusePool:
		return "Kicked from vehicle reuse pool";
    case Removal_PatrollingLawNoLongerNeeded:
        return "Removal_PatrollingLawNoLongerNeeded";
	case Removal_TooSteepAngle:
		return "Removal_TooSteepAngle";
	case Removal_RemoveAllVehsWithExcepton:
		return "Removal_RemoveAllVehsWithExcepton";
    default:
        Assertf(0, "Unexpected last removal reason");
        return "Unknown";
    };
}

void CVehiclePopulation::AddVehicleToSpawnHistory(CVehicle * pVehicle, CVehiclePopulation::CVehGenLink * pSpawnLink, const char * pStaticText)
{
	s32 iBestSlot = -1;
	s32 iBestFrameDelta = 0;

	for(s32 i=0; i<VEHICLE_POPULATION_SPAWN_HISTORY_SIZE; i++)
	{
		s32 iDelta = fwTimer::GetFrameCount() - ms_spawnHistory[i].m_iFrame;
		if(iDelta > iBestFrameDelta)
		{
			iBestFrameDelta = iDelta;
			iBestSlot = i;
		}
	}

	if(iBestSlot != -1)
	{
		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)pVehicle->GetBaseModelInfo();
		Assert(pModelInfo->GetFragType());
		phBound *bound = (phBound *)pModelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds();

		ms_spawnHistory[iBestSlot].m_pVehicle = pVehicle;
		ms_spawnHistory[iBestSlot].m_Matrix = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix());
		ms_spawnHistory[iBestSlot].m_AABB[0] = VEC3V_TO_VECTOR3(bound->GetBoundingBoxMin());
		ms_spawnHistory[iBestSlot].m_AABB[1] = VEC3V_TO_VECTOR3(bound->GetBoundingBoxMax());
		ms_spawnHistory[iBestSlot].m_iFrame = fwTimer::GetFrameCount();
		ms_spawnHistory[iBestSlot].m_pStaticText = pStaticText;

		if(pSpawnLink)
		{
			ms_spawnHistory[iBestSlot].m_fNumNearbyVehicles = pSpawnLink->GetNearbyVehiclesCount();
			ms_spawnHistory[iBestSlot].m_Nodes[0] = pSpawnLink->m_nodeA;
			ms_spawnHistory[iBestSlot].m_Nodes[1] = pSpawnLink->m_nodeB;
		}
		else
		{
			ms_spawnHistory[iBestSlot].m_fNumNearbyVehicles = -1.0f;
			ms_spawnHistory[iBestSlot].m_Nodes[0].SetEmpty();
			ms_spawnHistory[iBestSlot].m_Nodes[1].SetEmpty();
		}
	}
}

void CVehiclePopulation::RenderSpawnHistory()
{
	const Color32 textCol = Color_cyan;
	char txt[256];

	for(s32 i=0; i<VEHICLE_POPULATION_SPAWN_HISTORY_SIZE; i++)
	{
		if(ms_spawnHistory[i].m_iFrame != 0)
		{
			s32 iY = 0;
			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(ms_spawnHistory[i].m_AABB[0]), VECTOR3_TO_VEC3V(ms_spawnHistory[i].m_AABB[1]), MATRIX34_TO_MAT34V(ms_spawnHistory[i].m_Matrix), Color_green, false, 1);
			grcDebugDraw::Axis(ms_spawnHistory[i].m_Matrix, 1.0f, true);

			Vector3 vPos = ms_spawnHistory[i].m_Matrix.d;

			sprintf(txt, "frame: %i", ms_spawnHistory[i].m_iFrame);
			grcDebugDraw::Text(vPos, textCol, 0, iY, txt);
			iY += grcDebugDraw::GetScreenSpaceTextHeight();

			if(ms_spawnHistory[i].m_pVehicle)
			{
				sprintf(txt, "\"%s\" (0x%p)", ms_spawnHistory[i].m_pVehicle->GetBaseModelInfo()->GetModelName(), ms_spawnHistory[i].m_pVehicle.Get());
				grcDebugDraw::Text(vPos, textCol, 0, iY, txt);
				iY += grcDebugDraw::GetScreenSpaceTextHeight();
			}

			if(!ms_spawnHistory[i].m_Nodes[0].IsEmpty() || !ms_spawnHistory[i].m_Nodes[1].IsEmpty())
			{
				sprintf(txt, "link: (%i:%i) -> (%i:%i)", ms_spawnHistory[i].m_Nodes[0].GetRegion(), ms_spawnHistory[i].m_Nodes[0].GetIndex(), ms_spawnHistory[i].m_Nodes[1].GetRegion(), ms_spawnHistory[i].m_Nodes[1].GetIndex());
				grcDebugDraw::Text(vPos, textCol, 0, iY, txt);
				iY += grcDebugDraw::GetScreenSpaceTextHeight();
			}

			if(ms_spawnHistory[i].m_fNumNearbyVehicles >= 0.0f)
			{
				sprintf(txt, "num nearby: %.1f", ms_spawnHistory[i].m_fNumNearbyVehicles);
				grcDebugDraw::Text(vPos, textCol, 0, iY, txt);
				iY += grcDebugDraw::GetScreenSpaceTextHeight();
			}

			if(ms_spawnHistory[i].m_pStaticText)
			{
				sprintf(txt, "%s", ms_spawnHistory[i].m_pStaticText);
				grcDebugDraw::Text(vPos, textCol, 0, iY, txt);
				iY += grcDebugDraw::GetScreenSpaceTextHeight();
			}

			if(ms_bSpawnHistoryDisplayArrows && ms_spawnHistory[i].m_pVehicle)
			{
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vPos), ms_spawnHistory[i].m_pVehicle->GetTransform().GetPosition(), 0.3f, Color_orange);
			}
		}
	}
}

void CVehiclePopulation::ClearSpawnHistory()
{
	for(s32 i=0; i<VEHICLE_POPULATION_SPAWN_HISTORY_SIZE; i++)
	{
		ms_spawnHistory[i].m_pVehicle = NULL;
		ms_spawnHistory[i].m_iFrame = 0;
	}
}

#endif // __BANK
