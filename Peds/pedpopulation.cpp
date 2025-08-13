/////////////////////////////////////////////////////////////////////////////////
// FILE :		pedpopulation.cpp
// PURPOSE :	Deals with creation and removal of peds in the world.
//				It manages the ped population by removing what peds we can and
//				adding what peds we should.
//				It creates ambient peds and also peds at attractors when
//				appropriate.
//				It also revives persistent peds and cars that are close enough
//				to the player.
// AUTHOR :		Obbe.
//				Adam Croston.
// CREATED :	10/05/06
/////////////////////////////////////////////////////////////////////////////////
#include "peds/pedpopulation.h"

// rage includes
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "fragment/cacheheap.h"
#include "grcore/debugdraw.h"
#include "math/amath.h"
#include "profile/timebars.h"
#include "system/param.h"
#include "vector/vector3.h"

// game includes
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/BlockingBounds.h"
#include "animation/debug/AnimViewer.h"
#include "audio/speechmanager.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "core/game.h"
#include "control/replay/replay.h"
#include "control/population.h"
#include "debug/vectormap.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "event/events.h"
#include "game/cheat.h"
#include "game/Dispatch/DispatchManager.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Dispatch/OrderManager.h"
#include "game/modelindices.h"
#include "game/Performance.h"
#include "game/PopMultiplierAreas.h"
#include "game/clock.h"
#include "game/weather.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/NetworkInterface.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "network/players/NetworkPlayerMgr.h"
#include "pathserver/pathserver.h"
#include "pathserver/exportcollision.h"
#include "physics/gtainst.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Physics/Physics.h"
#include "peds/popcycle.h"
#include "peds/ped.h"
#include "peds/PedCapsule.h"
#include "peds/peddebugvisualiser.h"
#include "peds/ped.h"
#include "peds/pedplacement.h"
#include "peds/rendering/pedVariationPack.h"
#include "peds/pedfactory.h"
#include "peds/pedgeometryanalyser.h"
#include "peds/pedintelligence.h"
#include "peds/population_channel.h"
#include "peds/popzones.h"
#include "peds/rendering/pedvariationds.h"
#include "peds/gangs.h"
#include "peds/WildlifeManager.h"
#include "pedgroup/pedgroup.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/stores/staticboundsstore.h"
#include "renderer/Water.h"
#include "scene/DynamicEntity.h"
#include "scene/FocusEntity.h"
#include "scene/lod/LodScale.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/worldPoints.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"				// For CStreaming::HasObjectLoaded(), etc.
#include "streaming/streamingvisualize.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/script_cars_and_peds.h"
#include "fwsys/timer.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "task/vehicle/taskcarutils.h"
#include "task/movement/taskseekentity.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioChaining.h"
#include "Task/Scenario/ScenarioDebug.h"
#include "Task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/train.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/Weapon.h"
#include "Task/Response/TaskFlee.h"

#include  "game/PopMultiplierAreas.h"
#include "network/objects/entities/NetObjPlayer.h"



AI_OPTIMISATIONS()
RAGE_DEFINE_CHANNEL(population)
RAGE_DEFINE_CHANNEL(pedReuse)

s32	CPedPopulation::ms_nNumOnFootAmbient						= 0;
s32	CPedPopulation::ms_nNumOnFootScenario						= 0;
s32	CPedPopulation::ms_nNumOnFootCop							= 0;
s32	CPedPopulation::ms_nNumOnFootOther							= 0;
s32	CPedPopulation::ms_nNumOnFootSwat							= 0;
s32	CPedPopulation::ms_nNumOnFootEmergency						= 0;
s32	CPedPopulation::ms_nNumInVehAmbient							= 0;
s32	CPedPopulation::ms_nNumInVehScenario						= 0;
s32 CPedPopulation::ms_nNumInVehAmbientDead						= 0;
s32 CPedPopulation::ms_nNumInVehAmbientNoPretendModel			= 0; // vehicle model info prevents pretend occupants
s32	CPedPopulation::ms_nNumInVehCop								= 0;
s32	CPedPopulation::ms_nNumInVehOther							= 0;
s32	CPedPopulation::ms_nNumInVehSwat							= 0;
s32	CPedPopulation::ms_nNumInVehEmergency						= 0;

CPopGenShape CPedPopulation::ms_popGenKeyholeShape;

u32 CPedPopulation::ms_uNextScheduledInVehAmbExceptionsScanTimeMS = 0;

u32		CPedPopulation::ms_timeWhenForcedPopInitStarted				= 0;
u32		CPedPopulation::ms_timeWhenForcedPopInitEnds				= 0;
bool	CPedPopulation::ms_bFirstForcePopFillIter					= false;

bool	CPedPopulation::ms_allowRevivingPersistentPeds				= true;
bool	CPedPopulation::ms_allowScenarioPeds						= true;
bool	CPedPopulation::ms_allowAmbientPeds							= true;
bool	CPedPopulation::ms_allowCreateRandomCops					= true;
bool	CPedPopulation::ms_allowCreateRandomCopsOnScenarios			= true;

float	CPedPopulation::ms_debugOverridePedDensityMultiplier		= 1.0f;
float	CPedPopulation::ms_pedDensityMultiplier						= 1.0f;
float	CPedPopulation::ms_scenarioPedInteriorDensityMultiplier		= 1.0f;
float	CPedPopulation::ms_scenarioPedExteriorDensityMultiplier		= 1.0f;

#if __BANK
s32		CPedPopulation::m_iDebugAdjustSpawnDensity					= 0;
float	CPedPopulation::m_fDebugScriptedRangeMultiplier				= 1.0f;
#endif
float	CPedPopulation::ms_scriptPedDensityMultiplierThisFrame					= 1.0f;
float	CPedPopulation::ms_scriptScenarioPedInteriorDensityMultiplierThisFrame	= 1.0f;
float	CPedPopulation::ms_scriptScenarioPedExteriorDensityMultiplierThisFrame	= 1.0f;

bool	CPedPopulation::ms_suppressAmbientPedAggressiveCleanupThisFrame = false;

float	CPedPopulation::ms_fScriptedRangeMultiplierThisFrame		= 1.0f;

s32		CPedPopulation::ms_iAdjustPedSpawnDensityThisFrame			= 0;
s32		CPedPopulation::ms_iAdjustPedSpawnDensityLastFrame			= 0;
Vector3		CPedPopulation::ms_vAdjustPedSpawnDensityMin			= VEC3_ZERO;
Vector3		CPedPopulation::ms_vAdjustPedSpawnDensityMax			= VEC3_ZERO;

s32	CPedPopulation::ms_forceCreateAllAmbientPedsThisType			= -1;
bool	CPedPopulation::ms_noPedSpawn								= false;
bool	CPedPopulation::ms_bVisualiseSpawnPoints					= false;
bank_bool CPedPopulation::ms_bPerformSpawnCollisionTest				= true;
bool	CPedPopulation::ms_bCreatingPedsForAmbientPopulation		= false;
bool	CPedPopulation::ms_bCreatingPedsForScenarios				= false;

bool	CPedPopulation::ms_useStartupModeDueToWarping				= false;
bool	CPedPopulation::ms_useTempPopControlPointThisFrame			= false;
bool	CPedPopulation::ms_useTempPopControlSphereThisFrame			= false;
CPopCtrl CPedPopulation::ms_tmpPopOrigin;
float	CPedPopulation::ms_TempPopControlSphereRadius				= 0.f;
s32		CPedPopulation::ms_bInstantFillPopulation					= 0;

// The level designers can specify a the conversion center for dummy<->real
// conversions.
// Level designers use COMMAND_SET_SCRIPTED_CONVERSION_CENTRE and
// COMMAND_CLEAR_SCRITPED_CONVERSION_CENTRE.
bool	CPedPopulation::ms_scriptedConversionCentreActive			= false;
Vector3	CPedPopulation::ms_scriptedConversionCenter;

// The level designers can specify a small box within which random peds
// are not created.
// Level designers use SET_PED_NON_CREATION_AREA
bool	CPedPopulation::ms_nonCreationAreaActive					= false;
Vector3	CPedPopulation::ms_nonCreationAreaMin;
Vector3	CPedPopulation::ms_nonCreationAreaMax;

// The level designers can specify a small box within which random peds
// are not removed. The idea is that they can populate an area before the
// player is nearby and the peds would still be there when they get there.
// Level designers use SET_PED_NON_REMOVAL_AREA
bool	CPedPopulation::ms_nonRemovalAreaActive						= false;
Vector3	CPedPopulation::ms_nonRemovalAreaMin;
Vector3	CPedPopulation::ms_nonRemovalAreaMax;

#if !__FINAL
	bool	CPedPopulation::ms_bAllowRagdolls						= true;
#endif // !__FINAL

#if __BANK
s32	CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds		= -1;
s32	CPedPopulation::ms_popCycleOverrideNumberOfScenarioPeds		= -1;
// DEBUG!! -AC, this is really only a DEV variable...
bool	CPedPopulation::ms_createPedsAnywhere						= false;
// END DEBUG!!
bool	CPedPopulation::ms_movePopulationControlWithDebugCam		= false;

bool	CPedPopulation::ms_focusPedBreakOnPopulationRemovalTest		= false;
bool	CPedPopulation::ms_focusPedBreakOnPopulationRemoval			= false;
bool	CPedPopulation::ms_focusPedBreakOnAttemptedPopulationConvert= false;
bool	CPedPopulation::ms_focusPedBreakOnPopulationConvert			= false;
bool    CPedPopulation::ms_allowNetworkPedPopulationGroupCycling    = true;

bool	CPedPopulation::ms_displayMiscDegbugTests					= false;
bool	CPedPopulation::ms_displayAddCullZonesOnVectorMap			= false;
bool	CPedPopulation::ms_displayAddCullZonesInWorld				= false;
float	CPedPopulation::ms_addCullZoneHeightOffset					= 0.5f;
bool	CPedPopulation::ms_bDrawNonCreationArea						= false;
bool	CPedPopulation::ms_displayCreationPosAttempts3D				= false;
bool	CPedPopulation::ms_displayConvertedPedLines					= false;
bool	CPedPopulation::ms_displayPedDummyStateContentness			= false;
bool	CPedPopulation::ms_forceScriptedPopulationOriginOverride	= false;
bool CPedPopulation::ms_forcePopControlSphere						= false;
bool CPedPopulation::ms_displayPopControlSphereDebugThisFrame		= false;
Vector3 CPedPopulation::ms_vForcePopCenter 							= Vector3(VEC3_ZERO);
bool CPedPopulation::ms_bLoadCollisionAroundForcedPopCenter			= false;
bool CPedPopulation::ms_bDrawSpawnCollisionFailures					= false;
bool CPedPopulation::m_bLogAlphaFade								= false;

#undef popDebugf2
#define popDebugf2(fmt,...)	\
	ms_debugMessageHistory[ms_nextDebugMessage].time = fwTimer::GetTimeInMilliseconds(); \
	formatf(ms_debugMessageHistory[ms_nextDebugMessage].msg, fmt, ##__VA_ARGS__); \
	formatf(ms_debugMessageHistory[ms_nextDebugMessage].timeStr, "%02d:%02d:%02d", CClock::GetHour(), CClock::GetMinute(), CClock::GetSecond()); \
	ms_nextDebugMessage = (ms_nextDebugMessage + 1) % 64; \
	RAGE_DEBUGF2(population,fmt,##__VA_ARGS__)

bool CPedPopulation::ms_showDebugLog = false;
bool CPedPopulation::ms_bDisplayMultipliers = false;
u32 CPedPopulation::ms_nextDebugMessage = 0;
CPedPopulation::sDebugMessage CPedPopulation::ms_debugMessageHistory[64];
#endif // __BANK

#if __STATS
EXT_PF_PAGE(WorldPopulationPage);

EXT_PF_COUNTER(TotalEntitiesCreatedByPopulation);
EXT_PF_COUNTER(TotalEntitiesDestroyedByPopulation);

PF_GROUP(PedPopulation);
PF_LINK(WorldPopulationPage, PedPopulation);
PF_TIMER(PedPopulationProcess, PedPopulation);
PF_COUNTER(AmbientPedsCreated, PedPopulation);
PF_COUNTER(AmbientPedsReused, PedPopulation);
PF_COUNTER(NetworkPedsCreated, PedPopulation);
PF_COUNTER(EntitiesCreatedByPedPopulation, PedPopulation);
PF_COUNTER(EntitiesDestroyedByPedPopulation, PedPopulation);
PF_COUNTER(PedsInReusePool, PedPopulation);
PF_COUNTER(PedsDeletedByQueue, PedPopulation);
PF_COUNTER(PedsInDeletionQueue, PedPopulation);
PF_TIMER(GeneratePedsFromScenarioPoints, PedPopulation);
PF_COUNTER(ScheduledScenarioPointPed, PedPopulation);
#endif // __STATS

float	CPedPopulation::ms_allZoneScaler							= 0.8f;
float	CPedPopulation::ms_rangeScaleShallowInterior				= 0.75f;
float	CPedPopulation::ms_rangeScaleDeepInterior					= 0.5f;
float	CPedPopulation::ms_popCycleMaxPopulationScaler				= 1.0f;

u32	CPedPopulation::ms_maxPedsForEachPedTypeAvailableOnFoot		= 40;		// How many instances of each ped type can appear at one time.
u32	CPedPopulation::ms_maxPedsForEachPedTypeAvailableInVeh		= 50;		// How many instances of each ped type can appear at one time.
s32	CPedPopulation::ms_maxPedCreationAttemptsPerFrameOnInit		= 100;		// How many time can we attempt to create a ped each frame on init.
s32	CPedPopulation::ms_maxPedCreationAttemptsPerFrameNormal		= 2;		// How many time can we attempt to create a ped each frame.
s32 CPedPopulation::ms_maxScenarioPedCreatedPerFrame			= 1;		// Max number of actual peds that we are allowed to create on a single frame, on scenarios.
u32 CPedPopulation::ms_minTimeBetweenPriRemovalOfAmbientPedFromScenario = 3000;	// If scenario peds have wandered off and became excessive ambient peds, how frequently can we remove one? (in ms)
s32 CPedPopulation::ms_maxScenarioPedCreationAttempsPerFrameOnInit = 100;	// Max number of serious attempts (involving probes or other expensive checks) to spawn peds at scenarios, per frame, during startup.
s32 CPedPopulation::ms_maxScenarioPedCreationAttempsPerFrameNormal = 5;		// Max number of serious attempts (involving probes or other expensive checks) to spawn peds at scenarios, per frame.
float	CPedPopulation::ms_initAddRangeBaseMin						= 1.0f;		// Min distance from the camera a ped can be added when starting up the game
float	CPedPopulation::ms_initAddRangeBaseMax						= 30.0f;	// Max distance from the camera a ped can be added when starting up the game
float	CPedPopulation::ms_initAddRangeBaseFrustumExtra				= 10.0f;	// Max distance from the side of the camera frustum a ped can be added when starting up the game
float	CPedPopulation::ms_addRangeBaseOutOfViewMin					= 50.0f;	// Min distance from the camera a ped can be added when not visible to the player
float	CPedPopulation::ms_addRangeBaseOutOfViewMax					= 75.0f;	// Max distance from the camera a ped can be added when not visible to the player
float	CPedPopulation::ms_addRangeBaseInViewMin					= 90.0f;	// Min distance from the camera a ped can be added when possibly in view of the player
float	CPedPopulation::ms_addRangeBaseInViewMax					= 130.0f;	// Min distance from the camera a ped can be added when possibly in view of the player
float	CPedPopulation::ms_addRangeExtendedScenarioEffective		= 120.0f;	// The current effective distance we try to find extended range scenario points at.
float	CPedPopulation::ms_addRangeExtendedScenarioExtended			= 250.0f;	// In certain situations, the range which we may extend the extended scenario point range to.
float	CPedPopulation::ms_addRangeExtendedScenarioNormal			= 120.0f;	// Distance we try to find extended range scenario points at.
float	CPedPopulation::ms_maxRangeShortRangeScenario				= 50.0f;	// Distance we start spawning at for the short range scenarios
float	CPedPopulation::ms_maxRangeShortRangeAndHighPriorityScenario = 60.0f;	// Distance we start spawning at for the short range scenarios that are also high priority
float	CPedPopulation::ms_minRangeShortRangeScenarioInView			= 20.0f;	// Distance we stop spawning at on screen for the short range scenarios (on screen)
float	CPedPopulation::ms_minRangeShortRangeScenarioOutOfView		= 10.0f;	// Distance we stop spawning at on screen for the short range scenarios (off screen)
float	CPedPopulation::ms_addRangeHighPriScenarioOutOfViewMin		= 10.0f;	// Min distance a high priority scenario ped can be added when not visible to the player
float	CPedPopulation::ms_addRangeHighPriScenarioOutOfViewMax		= 75.0f;	// Max distance a high priority scenario ped can be added when not visible to the player
float	CPedPopulation::ms_addRangeHighPriScenarioInViewMin			= 20.0f;	// Min distance a high priority scenario ped can be added when possibly in view of the player
float	CPedPopulation::ms_addRangeHighPriScenarioInViewMax			= 130.0f;	// Min distance a high priority scenario ped can be added when possibly in view of the player
bool	CPedPopulation::ms_bAlwaysAllowRemoveScenarioOutOfView				= false;
s32	CPedPopulation::ms_iMinNumSlotsLeftToAllowRemoveScenarioOutOfView	= 5;
float	CPedPopulation::ms_addRangeBaseFrustumExtra					= 10.0f;	// Max distance from the side of the camera frustum a ped can be added when starting up the game
float	CPedPopulation::ms_addRangeBaseGroupExtra					= 0.0f;		// How much further away should groups be created to not be noticeable
float	CPedPopulation::ms_addRangeScaleInViewSpeedBasedMax			= 2.0f;
bool	CPedPopulation::ms_usePrefdNumMethodForCopRemoval			= true;
float	CPedPopulation::ms_cullRangeBaseOutOfView					= 90.0f;		// Peds not visible to the player outside this range will be removed
float	CPedPopulation::ms_cullRangeBaseInView						= 140.0f;		// Peds possibly visible to the player outside this range will be removed
float	CPedPopulation::ms_cullRangeScaleSpecialPeds				= 1.6f; 	// For special peds like the ones generated at roadblocks
float	CPedPopulation::ms_cullRangeScaleCops						= 1.6f; 	// For cops
float	CPedPopulation::ms_cullRangeExtendedInViewMin				= 140.0f;

bool	CPedPopulation::ms_allowDummyCoversions						= true;
bool	CPedPopulation::ms_useNumPedsOverConvertRanges				= true;
bool	CPedPopulation::ms_useTaskBasedConvertRanges				= false;
bool	CPedPopulation::ms_useHardDummyLimitOnLowFrameRate			= true;
bool	CPedPopulation::ms_useHardDummyLimitAllTheTime				= false;
bool	CPedPopulation::ms_aboveAmbientPedLimit						= false;	// True if we have more ambient peds than we want right now.
bool	CPedPopulation::ms_atScenarioPedLimit						= false;
u32 CPedPopulation::ms_nextAllowedPriRemovalOfAmbientPedFromScenario = 0;		// Time stamp for the next time when we may be allowed to remove a scenario ped that wandered off.
u32	CPedPopulation::ms_preferredNumRealPedsAroundPlayer			= 20;
u32	CPedPopulation::ms_lowFrameRateNumRealPedsAroundPlayer		= 16;
float	CPedPopulation::ms_pedMemoryBudgetMultiplier				= 1.0f;
float	CPedPopulation::ms_convertRangeRealPedHardRadius			= 3.0f;
float	CPedPopulation::ms_convertRangeBaseDummyToReal				= 22.0f;	// Distance from the camera for dummy ped to real ped conversions.
float	CPedPopulation::ms_convertRangeBaseRealToDummy				= 33.0f;	// Distance from the camera for real ped to dummy ped conversions.
s32	CPedPopulation::ms_conversionDelayMsCivRefreshMin			= 300;		// Civilian peds can only be converted if they have been a ped or dummy ped for this length of time.
s32	CPedPopulation::ms_conversionDelayMsCivRefreshMax			= 500;		// Civilian peds can only be converted if they have been a ped or dummy ped for this length of time.
s32	CPedPopulation::ms_conversionDelayMsEmergencyRefreshMin		= 600;		// Emergency peds (cops, medics, etc.) can only be converted if they have been a ped or dummy ped for this length of time.
s32	CPedPopulation::ms_conversionDelayMsEmergencyRefreshMax		= 800;		// Emergency peds (cops, medics, etc.) can only be converted if they have been a ped or dummy ped for this length of time.

float	CPedPopulation::ms_maxSpeedBasedRemovalRateScale			= 22.0f;
float	CPedPopulation::ms_maxSpeedExpectedMax						= 50.0f;
// the two values below were changed for url:bugstar:1088433
s32	CPedPopulation::ms_removalDelayMsCivRefreshMin				= 1500; //2500;		// Civilian peds can only be removed if they have been off-screen for this length of time.
s32	CPedPopulation::ms_removalDelayMsCivRefreshMax				= 2500; //3500;		// Civilian peds can only be removed if they have been off-screen for this length of time.
s32	CPedPopulation::ms_removalDelayMsEmergencyRefreshMin		= 4000;		// Emergency peds (cops, medics, etc.) can only be removed if they have been off-screen for this length of time.
s32	CPedPopulation::ms_removalDelayMsEmergencyRefreshMax		= 6000;		// Emergency peds (cops, medics, etc.) can only be removed if they have been off-screen for this length of time.

s32 CPedPopulation::ms_removalMaxPedsInRadius = 2;
float CPedPopulation::ms_removalClusterMaxRange = 1.25f;

bool	CPedPopulation::ms_bPopulationDisabled						= false;
Vector3* CPedPopulation::ms_vPedPositionsArray = NULL;
u32* CPedPopulation:: ms_iPedFlagsArray = NULL;

float	CPedPopulation::ms_pedRemovalTimeSliceFactor			= 3.0f;
s32		CPedPopulation::ms_pedRemovalIndex						= 0;

bool CPedPopulation::ms_extendDelayWithinOuterRadius				= false;
u32	CPedPopulation::ms_extendDelayWithinOuterRadiusTime				= 3000;
float CPedPopulation::ms_extendDelayWithinOuterRadiusExtraDist		= -50.0f;

u32 CPedPopulation::ms_disableMPVisibilityTestTimer  = 0;
u32 CPedPopulation::ms_mpVisibilityFailureCount      = 0;
u32 CPedPopulation::ms_lastMPVisibilityFailStartTime = 0;
Vector3 CPedPopulation::ms_vCachedScenarioPointCenter;
atFixedArray<CScenarioPoint*, NUM_CACHED_SCENARIO_POINTS> CPedPopulation::ms_cachedScenarioPoints;
u32 CPedPopulation::ms_cachedScenarioPointFrame = 0;
u32 CPedPopulation::ms_numHighPriorityScenarioPoints = 0;
u32 CPedPopulation::ms_lowPriorityScenarioPointsAlreadyChecked = 0;
bool CPedPopulation::ms_bInvalidateCachedScenarioPoints = false;

bank_float CPedPopulation::ms_fCloseSpawnPointDistControlSq			= 100.0f;
bank_float CPedPopulation::ms_fCloseSpawnPointDistControlOffsetSq	= 400.0f;
bank_s32   CPedPopulation::ms_minPedSlotsFreeForScenarios = 10;

bank_float   CPedPopulation::ms_minPatrollingLawRemovalDistanceSq = 15.0f * 15.0f;
bank_u32	 CPedPopulation::ms_DefaultMaxMsAheadForForcedPopInit = 700;
bank_float	 CPedPopulation::ms_fMinFadeDistance = 20.0f;
bank_bool	 CPedPopulation::ms_bDontFadeDuringStartupMode = true;

s32 CPedPopulation::ms_iNumPedsCreatedThisCycle = 0;
s32 CPedPopulation::ms_iNumPedsDestroyedThisCycle = 0;
u32 CPedPopulation::ms_uNumberOfFramesFaded = 0;

u32 CPedPopulation::ms_iPedPopulationFrameRate = 25; // gameconfig
u32 CPedPopulation::ms_iPedPopulationCyclesPerFrame = 1; // gameconfig
u32 CPedPopulation::ms_iMaxPedPopulationCyclesPerFrame = 0;
u32 CPedPopulation::ms_iPedPopulationFrameRateMP = 20; // gameconfig
u32 CPedPopulation::ms_iPedPopulationCyclesPerFrameMP = 1; // gameconfig
u32 CPedPopulation::ms_iMaxPedPopulationCyclesPerFrameMP = 0;
bank_float CPedPopulation::ms_fInstantFillExtraPopulationCycles = 2.0f;

CPedPopulation::SP_SpawnValidityCheck_PositionParams CPedPopulation::ms_CurrentCalculatedParams;
CPopCtrl CPedPopulation::ms_PopCtrl;

const ScalarV CPedPopulation::ms_clusteredPedVehicleRemovalRangeSq(30.0f * 30.0f);

const s32 MAX_INSTANT_FILL						= 10;

BANK_ONLY(CPedPopulation::PedPopulationFailureData CPedPopulation::ms_populationFailureData;)

PARAM(pedMemoryMultiplier, "Ped memory multiplier");
PARAM(pedInVehicles, "Peds in vehicles");
PARAM(pedOnFoot, "Peds on foot");
PARAM(pedAmbient, "Ambient Peds");

PARAM(logcreatedpeds, "[pedpopulation] Log created peds");
PARAM(logdestroyedpeds, "[pedpopulation] Log destroyed peds");

// Structs to store ped spawn information for ped spawn scheduling
// Placed here to avoid having extra includes in pedpopulation.h
struct ScheduledScenarioPed
{
	ScheduledScenarioPed():pScenarioPoint(NULL), pPropInEnvironment(NULL), chainUser(NULL) {};
	~ScheduledScenarioPed()
	{ 
		//If we have a chain user setting here we need to release it cause teh scheduled ped never got created
		if (chainUser)
		{
			CScenarioChainingGraph::UnregisterPointChainUser(*chainUser);
			chainUser = NULL;
		}
	};

	RegdScenarioPnt pScenarioPoint;
	RegdObj pPropInEnvironment;
	CScenarioManager::CollisionCheckParams collisionCheckParamsForLater;
	CScenarioManager::PedTypeForScenario predeterminedPedType;
	float fAddRangeInViewMin;				
	s32 iMaxPeds;
	s32 iSpawnOptions;
	s32 indexWithinType;
	u32 uFrameScheduled;
	bool bAllowDeepInteriors;
	CScenarioPointChainUseInfoPtr chainUser;
};

struct ScheduledAmbientPed
{
	ScheduledAmbientPed(){};
	~ScheduledAmbientPed(){};

	u32 uNewPedModelIndex;
	Vector3 vNewPedRootPos;
	float fHeading;		
	s32 iRoomIdx;
	CInteriorInst* pIntInst;
	DefaultScenarioInfo defaultScenarioInfo;
	const CAmbientModelVariations* pVariations;
	CPopCycleConditions conditions;
	u32 uFrameScheduled;
	bool bGroundPosIsInInterior;
	bool bNetDoInFrustumTest; //added so a recheck can be made when finally placing ped possibly after a delay when scheduled
};

struct ScheduledPedInVehicle
{
	ScheduledPedInVehicle(): pIncident(NULL), dispatchType(DT_Invalid){};
	~ScheduledPedInVehicle(){};

	RegdVeh pVehicle;
	s32 seat_num;
	u32 desiredModel;
	u32 uFrameScheduled;
	RegdIncident pIncident;
	DispatchType dispatchType;
	s32 pedGroupId;
	const CAmbientModelVariations* pVariations;
	COrder* pOrder;
	bool canLeaveVehicle;
};

struct PedPendingDeletion
{
	RegdPed pPed;
	float fCullDistanceSq;
#if __ASSERT
	u32 uFrameQueued;
	float fDistanceSqWhenQueued;
#endif
};

struct PedReuseEntry
{
	RegdPed pPed;
	u32 uFrameCreated;
	bool bNeedsAnimDirectorReInit;
};
// End ped spawn scheduling structs

// time (in frames) that a ped can stay in the scheduled queue
#define SCHEDULED_PED_TIMEOUT (10)
#define SCHEDULED_DISPATCH_PED_TIMEOUT (30)
#define MAX_SCHEDULED_AMBIENT_PEDS (5)
static atQueue<ScheduledAmbientPed, MAX_SCHEDULED_AMBIENT_PEDS> ms_scheduledAmbientPeds; // Queue of ambient peds
#define MAX_SCHEDULED_SCENARIO_PEDS (3)
static atQueue<ScheduledScenarioPed, MAX_SCHEDULED_SCENARIO_PEDS> ms_scheduledScenarioPeds; // queue of scenario peds
CompileTimeAssert(MAX_SCHEDULED_SCENARIO_PEDS > 1); // we need this to be more than one because of how we handle scenario peds that need to stream resources
#define MAX_SCHEDULED_PEDS_IN_VEHICLES (8)
static atQueue<ScheduledPedInVehicle, MAX_SCHEDULED_PEDS_IN_VEHICLES> ms_scheduledPedsInVehicles;
#define MAX_PEDS_TO_DELETE (32)
static atQueue<PedPendingDeletion, MAX_PEDS_TO_DELETE> ms_pedDeletionQueue; // queue of peds pending deletion
static u32 ms_numPedsToDeletePerFrame = 1;
#define MAX_PEDS_TO_REUSE (15)
static atFixedArray<PedReuseEntry, MAX_PEDS_TO_REUSE> ms_pedReusePool; // collection of peds to reuse
static bool ms_usePedReusePool = true;
static bool ms_bPedReuseInMP = true;
static bool ms_bPedReuseChecksHasBeenSeen = true;
static u8	ms_ReusedPedTimeout = 15;
static u8	ms_numNetworkPedsCreatedThisFrame = 0;
static u8	ms_maxDeadPedsAroundPlayerInMP = 7;
//static u8	ms_maxGlobalDeadPedsAroundPlayerInMP = 2;
static bool ms_bDoBackgroundPedReInitThisFrame = false;	// For ped anim director shutdown and init.  Expensive, so only do it if an init() hasn't already been done.

#if __BANK
bool CPedPopulation::ms_bDebugPedReusePool = false;
bool CPedPopulation::ms_bDisplayPedReusePool =  false;
#endif

#if ENABLE_NETWORK_LOGGING
Vector3 CPedPopulation::m_vLastSetPopPosition = VEC3_ZERO;
#endif // ENABLE_NETWORK_LOGGING

// cache the removal rate for our ped on screen assert
ASSERT_ONLY(static float ms_cachedRemovalRangeScale = 1.f;)

#define PRIORITY_REMOVAL_DIST (15.0f)
#define PRIORITY_REMOVAL_DIST_LAW (80.0f)
#define PRIORITY_REMOVAL_DIST_EXTRA_FAR_AWAY (60.0f)
#define PRIORITY_REMOVAL_DIST_SCENARIO (80.0f)
#define PRIORITY_REMOVAL_DIST_INFOV (30.f)

// Cached ped-generation coordinates, copied from those found by the CPathServer's thread
// This is a global here to avoid including "Pathserver.h" in the file's header.
CPedGenCoords g_CachedPedGenCoords;

PARAM(nopeds, "[population] Dont create any random peds");
PARAM(noambient, "[population] Disable ambient events by code and script");
PARAM(pedsanywhere, "[population] Create random regardless of navmesh ped densities");
PARAM(noragdolls, "[physics] do not allow any ragdolls");
PARAM(lockragdolls, "[physics] Lock all peds to disallow ragdolls");
PARAM(maxPeds, "[pedpopulation] Maximum ped population");
PARAM(minPeds, "[pedpopulation] Minimum ped population");
PARAM(pedreuse, "[pedpopulation] Turn on ped reuse");
PARAM(pedreusedebug, "[pedpopulation] Turn on ped reuse debug spheres");

// defined in vehiclepopulation.cpp
XPARAM(noReuseInMP);
XPARAM(extraReuse);


// DEBUG!! -AC, Defined at the bottom of the file, but declared here since it is used in CPedPopulation::Process.
#if __DEV
void VecMapDisplayPedPopulationEvent(const Vector3& pos, const Color32 baseColour);
void MiscDebugTests(const u32 timeMS, const u32 timeDeltaMs);
#endif // __DEV
// END DEBUG!!

/////////////////////////////////////////////////////////////////////////////////
// A helper struct for several of the algorithms below.
/////////////////////////////////////////////////////////////////////////////////
struct PedOrDumyPedHolder
{
	CPed*		m_pPed;
};

/////////////////////////////////////////////////////////////////////////////////

CPedPopulation::ChooseCivilianPedModelIndexArgs::ChooseCivilianPedModelIndexArgs()
		: m_RequiredGender(GENDER_DONTCARE)
		, m_RequiredGangMembership(MEMBERSHIP_DONT_CARE)
		, m_MustHaveClipSet(CLIP_SET_ID_INVALID)
		, m_BlockedModelSetsArray(NULL)
		, m_NumBlockedModelSets(0)
		, m_RequiredModels(NULL)
		, m_CreationCoors(NULL)
		, m_RequiredPopGroupFlags(0)
		, m_SelectionPopGroup(~0U)
		, m_SpeechContextRequirement(0)
		, m_AvoidNearbyModels(false)
		, m_IgnoreMaxUsage(false)
		, m_PedModelVariationsOut(NULL)
		, m_AllowFlyingPeds(false)
		, m_AllowSwimmingPeds(false)
		, m_MayRequireBikeHelmet(false)
		, m_GroupGangMembers(false)
		, m_ConditionalAnims(NULL)
		, m_ProhibitBulkyItems(false)
		, m_SortByPosition(false)
{
}

//-----------------------------------------------------------------------------------------------

#if __BANK
void CPedGenCoords::Visualise() const
{
	for(int i=0; i<m_iNumItems; i++)
	{
		const Color32 iBaseCol = (i < m_iCurrentItem) ? Color_red4 : Color_red;
		const Color32 iTopCol = (i < m_iCurrentItem) ? Color_grey4 : Color_white;

#if DEBUG_DRAW
		Vector3 vOffset = Vector3(0.0f,0.0f,8.0f) * m_Coords[i].m_fPedDeficit / m_fMaxDeficit;
		grcDebugDraw::Line(m_Coords[i].m_vPosition, m_Coords[i].m_vPosition + vOffset, iBaseCol, iTopCol);
#endif

#if __BANK
		if(CVectorMap::m_bAllowDisplayOfMap)
			CVectorMap::DrawMarker(m_Coords[i].m_vPosition, Color_white, 1.0f);
#endif
	}
}
#endif

void CPedPopulation::UpdateMaxPedPopulationCyclesPerFrame()
{
	ms_iMaxPedPopulationCyclesPerFrame = static_cast<u32>(ceilf(fwTimer::GetMaximumFrameTime() / (1.0f / (ms_iPedPopulationFrameRate * ms_iPedPopulationCyclesPerFrame))));
	ms_iMaxPedPopulationCyclesPerFrameMP = static_cast<u32>(ceilf(fwTimer::GetMaximumFrameTime() / (1.0f / (ms_iPedPopulationFrameRateMP * ms_iPedPopulationCyclesPerFrameMP))));
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitLevel
// PURPOSE :	Inits the population system for a game, initing the population
//				max size and parameters controlling the populations management.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::InitValuesFromConfig()
{
	ms_iPedPopulationFrameRate = GET_POPULATION_VALUE(PedPopulationFrameRate);
	ms_iPedPopulationCyclesPerFrame = GET_POPULATION_VALUE(PedPopulationCyclesPerFrame);
	ms_iPedPopulationFrameRateMP = GET_POPULATION_VALUE(PedPopulationFrameRateMP);
	ms_iPedPopulationCyclesPerFrameMP = GET_POPULATION_VALUE(PedPopulationCyclesPerFrameMP);
	UpdateMaxPedPopulationCyclesPerFrame();

#if ((RSG_PC || RSG_DURANGO || RSG_ORBIS) && 0)
	CPedPopulation::ms_pedMemoryBudgetMultiplierNG = CGameConfig::Get().GetConfigPopulation().m_PedMemoryMultiplier / 100.0f;
	PARAM_pedMemoryMultiplier.Get(CPedPopulation::ms_pedMemoryBudgetMultiplierNG );
	CPedPopulation::ms_pedMemoryBudgetMultiplierNG  = Max(0.05f, Min(10.0f, CPedPopulation::ms_pedMemoryBudgetMultiplierNG ));
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	int iPedForVehicles = GET_POPULATION_VALUE(PedsForVehicles);
	CPedPopulation::ms_maxPedsForEachPedTypeAvailableInVeh = iPedForVehicles;
	PARAM_pedInVehicles.Get(CPedPopulation::ms_maxPedsForEachPedTypeAvailableInVeh );
	CPedPopulation::ms_maxPedsForEachPedTypeAvailableInVeh  = Max(10U, 100U, CPedPopulation::ms_maxPedsForEachPedTypeAvailableInVeh);
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
}

void CPedPopulation::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		Displayf("Initialising CPedPopulation...\n");

#if !__FINAL
		if(PARAM_noragdolls.Get())
		{
			ms_bAllowRagdolls = false;
		}
#endif // !__FINAL

		// Reset the ped counts.
		ms_nNumOnFootAmbient				= 0;
		ms_nNumOnFootScenario				= 0;
		ms_nNumOnFootCop					= 0;
		ms_nNumOnFootEmergency				= 0;

		ms_pedDensityMultiplier				= 1.0f;
		ms_debugOverridePedDensityMultiplier = 1.0f;
		ms_forceCreateAllAmbientPedsThisType	= -1;

		ms_uNextScheduledInVehAmbExceptionsScanTimeMS = 0;

		ms_nextAllowedPriRemovalOfAmbientPedFromScenario = 0;

		Assert(!ms_vPedPositionsArray);
		ms_vPedPositionsArray = rage_new Vector3[MAXNOOFPEDS];
		Assert(!ms_iPedFlagsArray);
		ms_iPedFlagsArray = rage_new u32[MAXNOOFPEDS];

#if !__FINAL
		if(PARAM_nopeds.Get())
		{
			gbAllowAmbientPedGeneration = false;
			ms_noPedSpawn = true;
		}
		if(PARAM_noambient.Get())
		{
			gbAllowScriptBrains = false;
			ms_noPedSpawn = true;
		}
		if(PARAM_pedsanywhere.Get())
		{
			BANK_ONLY( ms_createPedsAnywhere = true; )
		}
		if( PARAM_maxPeds.Get() )
		{
			ms_debugOverridePedDensityMultiplier = 6.0f;
		}
		else if( PARAM_minPeds.Get() )
		{
			ms_debugOverridePedDensityMultiplier = 0.1f;
		}
		if(PARAM_pedreuse.Get())
		{
			ms_usePedReusePool = true;
		}
#endif // !__FINAL

#if __BANK
		if(PARAM_pedreusedebug.Get())
		{
			ms_bDebugPedReusePool = true;
		}

		if (PARAM_logcreatedpeds.Get())
			CPedFactory::ms_bLogCreatedPeds = true;

		if (PARAM_logdestroyedpeds.Get())
			CPedFactory::ms_bLogDestroyedPeds = true;
#endif // __BANK

		if(PARAM_noReuseInMP.Get())
		{
			ms_bPedReuseInMP = false;
		}
		if(PARAM_extraReuse.Get())
		{
			ms_bPedReuseChecksHasBeenSeen = false;
		}

		SetScenarioPedDensityMultipliers(1.0f,1.0f);
		SetForcedNewAmbientPedType(-1);
		PedNonRemovalAreaClear();
		PedNonCreationAreaClear();
		SetAllowCreateRandomCops(true);
		SetAllowCreateRandomCopsOnScenarios(true);
		SetAllowDummyConversions(true);
		ClearTempPopControlSphere();

		CWildlifeManager::Init();

		InitValuesFromConfig();

#if __BANK
		s32 iAmbientPeds = 0;
		if (PARAM_pedAmbient.Get(iAmbientPeds))
		{
			CPedPopulation::ms_popCycleOverrideNumberOfAmbientPeds = Max(1, Min(1000, iAmbientPeds));
		}

		u32 iPedsOnFoot = 0;
		if (PARAM_pedOnFoot.Get(iPedsOnFoot))
		{
			CPedPopulation::ms_maxPedsForEachPedTypeAvailableOnFoot = Max(1U, Min(1000U, iPedsOnFoot));
		}
#endif

		Displayf("CPedPopulation ready\n");
	}
}

#if __BANK
void CPedPopulation::OnToggleAllowPedGeneration()
{
	if(gbAllowPedGeneration || gbAllowAmbientPedGeneration)
	{
		CPedPopulation::ms_noPedSpawn = false;
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShutdownLevel
// PURPOSE :	Shuts down the population system for a game.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_SESSION)
	{
		Assert(ms_vPedPositionsArray);
		delete []ms_vPedPositionsArray;
		ms_vPedPositionsArray = NULL;
		Assert(ms_iPedFlagsArray);
		delete []ms_iPedFlagsArray;
		ms_iPedFlagsArray = NULL;
		
		FlushPedQueues();
	}

	CWildlifeManager::Shutdown();

	CPedFactory::GetFactory()->ClearDestroyedPedCache();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Process
// PURPOSE :	Manages the ped population by removing what peds we can and
//				adding what peds we should.
//				It creates ambient peds and also peds at attractors when
//				appropriate.
//				It also revives persistent peds and cars that are close enough
//				to the player.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::Process()
{
	PF_FUNC(PedPopulationProcess);
	PF_AUTO_PUSH_TIMEBAR("Ped Pop Process");

	// Keep track of how long the screen has been faded to better inform ShouldUseStartupMode()
	if (camInterface::IsFadedOut())
	{
		ms_uNumberOfFramesFaded++;
	}
	else
	{
		ms_uNumberOfFramesFaded = 0;
	}

	if (!CStreaming::IsPlayerPositioned())
		return;

	// This is needed in case we just loaded some scenario region that had been loaded at
	// an earlier time, to make sure we re-attach all scenario chain users before we consider
	// spawning new ones.
	CScenarioChainingGraph::UpdateNewlyLoadedScenarioRegions();

	if(ms_bInstantFillPopulation && CPathServer::ms_iTimeToNextRequestAndEvict <= 0)
		return;

#if __BANK

	DisplayMessageHistory();

	if( (CPopCycle::sm_bDisplayDebugInfo || ms_bDrawNonCreationArea) && ms_nonCreationAreaActive )
	{
		grcDebugDraw::BoxAxisAligned( RCC_VEC3V(ms_nonCreationAreaMin), RCC_VEC3V(ms_nonCreationAreaMax), Color_red4, false );
	}

#endif

#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		UpdatePedReusePool();
		return;
	}
#endif

	// Make sure to only process the population when not in replay or
	// cut scene mode or when game is paused.
	if(fwTimer::IsGamePaused() || ms_bPopulationDisabled)
	{
		return;
	}	

	// Don't want to create any ambient peds when the anim viewer is active, unless
	// the debug param for creating peds anywhere is set- then allow it as someone
	// purposely wants the ped population stuff active in the anim viewer.
	bool allowPopulationAddAndRemoval = true;

	// Get the interest point about which to base the population.
	// Make sure to pick a position and forward vector designed to
	// get the best end visual results out of the population
	// management system.
	// NOTE: We might need to change this for multi-player so that
	// an equal population counts are created around each player.

#if __BANK
	if(ms_forceScriptedPopulationOriginOverride)
	{
		SetScriptCamPopulationOriginOverride();
	}
	if(ms_forcePopControlSphere)
	{
		DebugSetPopControlSphere();
	}	
#endif

	PF_START_TIMEBAR_DETAIL("Calc Population Control Info");
	ms_PopCtrl = GetPopulationControlPointInfo(ms_useTempPopControlPointThisFrame ? &ms_tmpPopOrigin : NULL);

	// Check if the game has settled down, if so process normally, otherwise
	// keep trying to populate the area around the player.
	// Disable 'atStart' mode if camera is faded up or we'll see peds spawning nearby
	const bool atStart = (ShouldUseStartupMode() || ms_bInstantFillPopulation) && (camInterface::GetFadeLevel() > 0.9f);
	if(HasForcePopulationInitFinished() && !ms_bInstantFillPopulation)
	{
		ms_timeWhenForcedPopInitEnds = 0; // reset may not be needed but is safer
		ms_timeWhenForcedPopInitStarted = 0;
	}

	if(CScenarioManager::ShouldExtendExtendedRangeFurther())
	{
		ms_addRangeExtendedScenarioEffective = ms_addRangeExtendedScenarioExtended;
	}
	else
	{
		ms_addRangeExtendedScenarioEffective = ms_addRangeExtendedScenarioNormal;
	}

	// Get a value for our speed scale between zero and one.
	// This is used to change the population generation and removal
	// so it is best suited for how we are moving through the world.
	const float maxSpeedPortion = rage::Clamp((ms_PopCtrl.m_centreVelocity.XYMag() / ms_maxSpeedExpectedMax), 0.0f, 1.0f);

	// Get the lod distance scale and make sure it is reasonable.
	float lodDistanceScale = (g_LodScale.GetGlobalScale()>0.0f)?g_LodScale.GetGlobalScale():1.0f;

	// In cutscenes, this g_LodScale thing seems to vary wildly from shot to shot, and it can get
	// as low as 0.4. This can easily cause peds to get destroyed or created on screen closer than we'd
	// want, so we now clamp it at 1 in that case for spawn distance scale purposes.
	// Note: it might make sense to do this in non-cutscene cases too, but haven't seen it cause a problem
	// there, and it seems to be >= 1 there anyway. 
	const bool bCutsceneRunning = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
	if(bCutsceneRunning)
	{
		lodDistanceScale = Max(lodDistanceScale, 1.0f);
	}

	// Scale all ranges according to depth of the pop control into interiors.
	float scalingFromPosInfo = 1.0f;
	if(ms_PopCtrl.m_locationInfo == LOCATION_SHALLOW_INTERIOR)
	{
		scalingFromPosInfo = ms_rangeScaleShallowInterior;
	}
	else if(ms_PopCtrl.m_locationInfo == LOCATION_DEEP_INTERIOR)
	{
		scalingFromPosInfo = ms_rangeScaleDeepInterior;
	}


	// When a new game is started the streets seem a bit empty so we must
	// then generate peds around player, however when a games starts we need
	// to wait a couple of frames for everything else to initialized though (or
	// the peds end up getting created at 0.0f, 0.0f, 0.0f).

	// Take into account renderer and tweak parameter scaling.
	float vehicleTypeRangeScale = 1.0f;

	// Scripted range multipliers override the vehicle type multiplier
	if(ms_fScriptedRangeMultiplierThisFrame == 1.0f && CGameWorld::GetMainPlayerInfo())
	{
		CVehicle* pPlayerVehicle = FindPlayerVehicle();

		if(pPlayerVehicle && pPlayerVehicle->GetIsAircraft())
		{
			vehicleTypeRangeScale = NetworkInterface::IsGameInProgress() ? CVehiclePopulation::ms_fAircraftSpawnRangeMultMP : CVehiclePopulation::ms_fAircraftSpawnRangeMult;
		}
	}

	const float baseRangeScale = scalingFromPosInfo * ms_allZoneScaler * lodDistanceScale * vehicleTypeRangeScale;

	const float addRangeScale = baseRangeScale * ms_fScriptedRangeMultiplierThisFrame;
	float addRangeInViewMinScale = baseRangeScale * rage::Max(ms_fScriptedRangeMultiplierThisFrame, 1.f); // don't let script reduce this scale
	float addRangeInViewMaxScale = baseRangeScale * ms_fScriptedRangeMultiplierThisFrame;

	// Possibly scale the in view add ranges according to how fast we are moving as
	// we probably want to create more peds ahead of us than to the sides or back
	// when moving quickly.
	// We scale by 1.0f at normal speeds and up to ms_addRangeScaleInViewSpeedBasedMax at
	// high speeds.
	addRangeInViewMinScale *= rage::Max(1.0f, (maxSpeedPortion * ms_addRangeScaleInViewSpeedBasedMax *.6f)); // don't let the min range get too far away - now range is 1.0-1.2 at speed
	addRangeInViewMaxScale *= rage::Max(1.0f, (maxSpeedPortion * ms_addRangeScaleInViewSpeedBasedMax));

	//Scale ranges according to how fast we are moving as we don't want
	// too many peds trailing behind us.
	// 1.0f at normal speeds and up to 10.0f at high speeds.
	const float removalRateScale = rage::Max(1.0f, (maxSpeedPortion * ms_maxSpeedBasedRemovalRateScale));
	const float removalRangeScale = baseRangeScale * ms_fScriptedRangeMultiplierThisFrame * rage::Max(1.0f, (maxSpeedPortion * ms_addRangeScaleInViewSpeedBasedMax));

#if __ASSERT
	// cache removal rate for an assert when deleting peds
	ms_cachedRemovalRangeScale = removalRangeScale;
#endif


	float	addRangeInViewMin				= addRangeInViewMinScale	* ((atStart)	? (ms_initAddRangeBaseMin)			: (ms_addRangeBaseInViewMin));
	float	addRangeInViewMax				= addRangeInViewMaxScale	* ((atStart)	? (ms_initAddRangeBaseMax)			: (ms_addRangeBaseInViewMax));
	float	addRangeOutOfViewMin			= addRangeScale			* ((atStart)	? (ms_initAddRangeBaseMin)			: (ms_addRangeBaseOutOfViewMin));
	float	addRangeOutOfViewMax			= addRangeScale			* ((atStart)	? (ms_initAddRangeBaseMax)			: (ms_addRangeBaseOutOfViewMax));
	float	addRangeFrustumExtra			= addRangeScale			* ((atStart)	? (ms_initAddRangeBaseFrustumExtra)	: (ms_addRangeBaseFrustumExtra));
	bool	doInFrustumTests				= (atStart)	? (false) : (true);
	bool	inAnInterior					= (ms_PopCtrl.m_locationInfo!=LOCATION_EXTERIOR);

	// If camera is looking behind, then ensure that we don't allow spawning of peds up close in front of the player.
	// This is to avoid the player glancing behind, and the ped population taking that opportunity to spawn something
	// directly ahead of the player.
	if(camInterface::IsDominantRenderedDirector(camInterface::GetGameplayDirector()) && camInterface::GetGameplayDirector().IsLookingBehind())
	{
		addRangeOutOfViewMin = addRangeInViewMin;
		addRangeOutOfViewMax = addRangeInViewMax;
	}

	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

	// mess with the range scales if we're using a sphere to set the pop control point
	if(Unlikely(ms_useTempPopControlSphereThisFrame && ms_TempPopControlSphereRadius != 0.f))
	{
		const bool bUsingDrone = pLocalPlayer ? pLocalPlayer->GetPedResetFlag(CPED_RESET_FLAG_UsingDrone) : false;
		if (!bUsingDrone)
		{
			addRangeInViewMin = 0.f;
			addRangeInViewMax = ms_TempPopControlSphereRadius;
			addRangeOutOfViewMin = 0.f;
			addRangeOutOfViewMax = ms_TempPopControlSphereRadius;
		}
	}

	//Store this off so others can use the calculated values.
	ms_CurrentCalculatedParams.popCtrlCentre = ms_PopCtrl.m_centre;
	ms_CurrentCalculatedParams.fAddRangeInViewMin					= addRangeInViewMin;
	ms_CurrentCalculatedParams.fAddRangeInViewMax					= addRangeInViewMax;
	ms_CurrentCalculatedParams.fAddRangeOutOfViewMin			= addRangeOutOfViewMin;
	ms_CurrentCalculatedParams.fAddRangeOutOfViewMax			= addRangeOutOfViewMax;
	ms_CurrentCalculatedParams.fAddRangeFrustumExtra			= addRangeFrustumExtra;
	ms_CurrentCalculatedParams.fRemovalRangeScale					= removalRangeScale;
	ms_CurrentCalculatedParams.fRemovalRateScale					= removalRateScale;
	ms_CurrentCalculatedParams.doInFrustumTest						= doInFrustumTests;
	ms_CurrentCalculatedParams.bInAnInterior							= inAnInterior;
	ms_CurrentCalculatedParams.faddRangeExtendedScenario	= ms_addRangeExtendedScenarioEffective;

	const float	st0	= rage::Sinf(ms_PopCtrl.m_baseHeading);
	const float	ct0	= rage::Cosf(ms_PopCtrl.m_baseHeading);
	Vector2 dir(-st0, ct0);
	Assert(dir.Mag2() > 0.1f);
	dir.Normalize();

	const float tanHFov = ms_PopCtrl.m_tanHFov;		// camInterface::GetViewportTanFovH();
	const float	cosHalfAngle = rage::Cosf(rage::Atan2f(tanHFov, 1.0f));

	ms_popGenKeyholeShape.Init(
		ms_PopCtrl.m_centre,
		dir,
		cosHalfAngle,
		addRangeOutOfViewMin,// These are the close generation band.
		addRangeOutOfViewMax,
		addRangeInViewMin,// These are the far away generation band.
		addRangeInViewMax,
		0.0f);

	// Determine how many cops should be around the player at any given time.
	u32 numPrefdCopsAroundPlayer = 0;
	// Finally, make sure all cops around the player are active
	if(	pLocalPlayer &&
		pLocalPlayer->GetPlayerWanted() )
	{
		const s32 NumPedsToKeepAround[WANTED_LEVEL_LAST] = { 0, 2, 4, 6, 8, 12 };
		s32 iWantedLevel = pLocalPlayer->GetPlayerWanted()->GetWantedLevel();

		// If player has recently lost their wanted level, then allow cop/military corpses to persist for a while (url:bugstar:1500093, url:bugstar:1498537)
		const u32 iTimeSinceWantedLevelCleared = fwTimer::GetTimeInMilliseconds() - pLocalPlayer->GetPlayerWanted()->GetTimeWantedLevelCleared();
		if(iWantedLevel == WANTED_CLEAN && iTimeSinceWantedLevelCleared < 60000)
			iWantedLevel = WANTED_LEVEL1;

		numPrefdCopsAroundPlayer = NumPedsToKeepAround[iWantedLevel];
	}

	// Check if we can remove any peds from the world.
	// Note: cops are handled slightly differently a few lines below.
	PF_START_TIMEBAR_DETAIL("Ped Removal");
	if(allowPopulationAddAndRemoval)
	{
		PedsRemoveOrMarkForRemovalOnVisImportance(ms_PopCtrl.m_centre, removalRangeScale, removalRateScale, inAnInterior);
		PedsRemoveIfRemovalIsPriority(ms_PopCtrl.m_centre, removalRangeScale);
		//PedsRemoveIfPoolTooFull(popCtrl.m_centre); // FA: comment out for now, when dummy peds were removed this function stopped removing peds

		// Handle cops differently.
		if(ms_usePrefdNumMethodForCopRemoval)
		{
			CopsRemoveOrMarkForRemovalOnPrefdNum(ms_PopCtrl.m_centre, numPrefdCopsAroundPlayer, removalRangeScale, removalRateScale, inAnInterior);
		}

		// Remove dead peds when there are still so many of them
		// (after the other removals above) that it is starting to
		// affect the rest of the population.
		u32 maxNumMaxDeadAroundPlayer = 15;
		u32 maxNumMaxDeadGlobally = 30;		// A sane value related to MAX_NUM_NETOBJPEDS
		if (NetworkInterface::IsGameInProgress())
		{
			maxNumMaxDeadAroundPlayer = ms_maxDeadPedsAroundPlayerInMP;
		}
		DeadPedsRemoveExcessCounts(ms_PopCtrl.m_centre, maxNumMaxDeadAroundPlayer, maxNumMaxDeadGlobally);
	}
	
	PF_INCREMENTBY(PedsInDeletionQueue, ms_pedDeletionQueue.GetCount()); // keep track of how many peds we have in the deletion queue

	UpdatePedReusePool();

	// Add peds to the world below.

	// Make sure that adding to the population isn't currently blocked.
	const bool generatePeds = allowPopulationAddAndRemoval && (!ms_noPedSpawn || ms_bInstantFillPopulation);

	if(generatePeds)
	{
		SetUpParamsForPathServerPedGenBackgroundTask(ms_popGenKeyholeShape);

		if(CPathServer::GetAmbientPedGen().AreCachedPedGenCoordsReady())
		{
			CPathServer::GetAmbientPedGen().GetCachedPedGenCoords(g_CachedPedGenCoords);
		}

#if __BANK
		if(ms_bVisualiseSpawnPoints)
		{
			g_CachedPedGenCoords.Visualise();
		}
#endif

		// 8/7/12 - cthomas - We know only the main thread would possibly be updating physics at this 
		// point in time, so set the flag in the physics system to indicate that. This is an optimization 
		// that will allow us to skip some locking overhead in the physics system as we add peds.
		PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);

		////////////////////////////////////////////////////////////////////////////
		ms_iNumPedsCreatedThisCycle = ms_numNetworkPedsCreatedThisFrame;

		if(CDispatchManager::ms_SpawnedResourcesThisFrame)
		{
			IncrementNumPedsCreatedThisCycle();
		}

		if(CVehiclePopulation::GetNumExcessEntitiesCreatedThisFrame() > 0)
		{
			IncrementNumPedsCreatedThisCycle();
		}

		if(CVehicleAILodManager::GetNumVehiclesConvertedToRealPedsThisFrame() > 0)
		{
			IncrementNumPedsCreatedThisCycle();
		}

		ms_iNumPedsDestroyedThisCycle = 0;
		ms_numNetworkPedsCreatedThisFrame = 0;
		////////////////////////////////////////////////////////////////////////////

		PF_INCREMENTBY(EntitiesCreatedByPedPopulation, -ms_iNumPedsCreatedThisCycle);

		if(ms_bInstantFillPopulation)
		{
			ProcessInstantFill(atStart);
		}

		PF_PUSH_TIMEBAR("CWildlifeManager::Process");
		CWildlifeManager::GetInstance().Process(g_CachedPedGenCoords.GetNumRemaining() > 0);
		PF_POP_TIMEBAR();

		// By cycling calls to SpawnScheduledPedInVehicle(), SpawnScheduledScenarioPed(), and SpawnScheduledAmbientPed(),
		// we're able to limit ped generation to one per population cycle
		enum
		{
			PEDGEN_STATE_SPAWN_PED_IN_VEHICLE = 0,
			PEDGEN_STATE_SPAWN_SCENARIO_PED,
			PEDGEN_STATE_SPAWN_AMBIENT_PED,
			NUM_PEDGEN_STATES
		};

		static u32 s_State = 0;
	
		static float s_TimeSteps = 0.0f;
		s_TimeSteps += Min(fwTimer::GetTimeStep(), fwTimer::GetMaximumFrameTime()) / (1.0f / (GetPopulationFrameRate() * GetPopulationCyclesPerFrame()));

		if(ms_bInstantFillPopulation)
		{
			// Instant fill requires additional population cycles
			s_TimeSteps += ms_fInstantFillExtraPopulationCycles;
		}

		// Stagger frame compensation by deferring when vehicle population has already compensated, but, not on consecutive frames
		static bool s_DeferFrameCompensation = false;
		s_DeferFrameCompensation = !s_DeferFrameCompensation && CVehiclePopulation::GetFrameCompensatedThisFrame();

		float truncatedTimeSteps = s_DeferFrameCompensation ? Min(1.0f, s_TimeSteps) : s_TimeSteps;

		truncatedTimeSteps = Min(truncatedTimeSteps, static_cast<float>(GetMaxPopulationCyclesPerFrame()));

		while(truncatedTimeSteps >= 1.0f)
		{
			if(ms_iNumPedsDestroyedThisCycle == 0)
			{
				PF_PUSH_TIMEBAR("ProcessPedDeletionQueue");
				ProcessPedDeletionQueue();
				PF_POP_TIMEBAR();
			}

			if(ms_allowScenarioPeds)
			{
				PF_PUSH_TIMEBAR("GeneratePedsAtScenarioPoints");
				GeneratePedsAtScenarioPoints(ms_PopCtrl.m_centre, inAnInterior, addRangeOutOfViewMin, addRangeOutOfViewMax, addRangeInViewMin, addRangeInViewMax, addRangeFrustumExtra, doInFrustumTests, HowManyMoreScenarioPedsShouldBeGenerated(), HowManyMoreScenarioPedsShouldBeGenerated(true));
				PF_POP_TIMEBAR();
			}

			if(ms_allowAmbientPeds && HowManyMoreAmbientPedsShouldBeGenerated() > 0)
			{
				PF_PUSH_TIMEBAR("AddAmbientPedToPopulation");
				AddAmbientPedToPopulation(inAnInterior, addRangeInViewMin, addRangeOutOfViewMin, doInFrustumTests, g_CachedPedGenCoords.GetNumRemaining() > 0, false);
				PF_POP_TIMEBAR();
			}

			const u32 initialState = s_State;

			while(1)
			{
				// Break if we've created a ped, or spillover hasn't been depleted
				if(ms_iNumPedsCreatedThisCycle > 0)
				{
					break;
				}

				switch(s_State)
				{
				case PEDGEN_STATE_SPAWN_PED_IN_VEHICLE:
					{
						PF_PUSH_TIMEBAR("SpawnScheduledPedInVehicle");
						SpawnScheduledPedInVehicle();
						PF_POP_TIMEBAR();
						break;
					}
				case PEDGEN_STATE_SPAWN_SCENARIO_PED:
					{
						PF_PUSH_TIMEBAR("SpawnScheduledScenarioPed");
						SpawnScheduledScenarioPed();
						PF_POP_TIMEBAR();
						break;
					}
				case PEDGEN_STATE_SPAWN_AMBIENT_PED:
					{
						PF_PUSH_TIMEBAR("SpawnScheduledAmbientPed");
						SpawnScheduledAmbientPed(addRangeInViewMin);
						PF_POP_TIMEBAR();
						break;
					}
				}

				// If we've executed all code paths, break. Otherwise, advance state and execute the next code path immediately.
				// This guarantees that population cycles are not wasted while code paths are ready to generate peds.
				s_State = (s_State + 1) % NUM_PEDGEN_STATES;

				if(s_State == initialState)
				{
					break;
				}
			}

			// Decrement creations by one -- should we encounter multiple creations in a single cycle,
			// allow them to spill over to the next cycle, effectively blocking subsequent, excess, creations.
			if(ms_iNumPedsCreatedThisCycle > 0)
			{
				--ms_iNumPedsCreatedThisCycle;
				PF_INCREMENT(EntitiesCreatedByPedPopulation);
			}

			if(ms_iNumPedsDestroyedThisCycle > 0)
			{
				--ms_iNumPedsDestroyedThisCycle;
				PF_INCREMENT(EntitiesDestroyedByPedPopulation);
			}

			truncatedTimeSteps -= 1.0f;
			s_TimeSteps -= 1.0f;
		}

		// Persist
		PF_PUSH_TIMEBAR("CWildlifeManager::Process");
		PF_POP_TIMEBAR();
		PF_PUSH_TIMEBAR("ProcessPedDeletionQueue");
		PF_POP_TIMEBAR();
		PF_PUSH_TIMEBAR("GeneratePedsAtScenarioPoints");
		PF_POP_TIMEBAR();
		PF_PUSH_TIMEBAR("AddAmbientPedToPopulation");
		PF_POP_TIMEBAR();
		PF_PUSH_TIMEBAR("SpawnScheduledPedInVehicle");
		PF_POP_TIMEBAR();
		PF_PUSH_TIMEBAR("SpawnScheduledScenarioPed");
		PF_POP_TIMEBAR();
		PF_PUSH_TIMEBAR("SpawnScheduledAmbientPed");
		PF_POP_TIMEBAR();

		PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
	}

	PF_INCREMENTBY(EntitiesCreatedByPedPopulation, ms_iNumPedsCreatedThisCycle);
	PF_INCREMENTBY(EntitiesDestroyedByPedPopulation, ms_iNumPedsDestroyedThisCycle);

	PF_INCREMENTBY(TotalEntitiesCreatedByPopulation, PF_COUNTER_VAR(EntitiesCreatedByPedPopulation).GetValue());
	PF_INCREMENTBY(TotalEntitiesDestroyedByPopulation, PF_COUNTER_VAR(EntitiesDestroyedByPedPopulation).GetValue());

	// Check if we have more peds now than what we want. If so, set ms_aboveAmbientPedLimit
	// to perhaps remove some peds that came from scenarios, originally.
	ms_aboveAmbientPedLimit = (HowManyMoreAmbientPedsShouldBeGenerated() < 0);

	UpdateInVehAmbientExceptionsCount();
#if __BANK
	PF_START_TIMEBAR_DETAIL("Debug Functions");
// DEBUG!! -AC, DisplaySpawnPointsRawDensity
	//if(CDebugScene::bDisplaySpawnPointsRawDensityOnVMap)
	//{
	//	//if(moved or time of day has changed)
	//	//{
	//		SpawnPointsData.Clear();
	//		CPathServer::CurrentSpawnPointsCallbackData tempData;
	//		CPathServer::IterateOverCurrentSpawnPoints(CurrentSpawnPointsCallback, &tempData);
	//	//}

	//	// Draw all the current spawn points.
	//	int spawnPointCount = SpawnPointsData.GetCount();
	//	for(int i = 0; i < spawnPointCount; ++i)
	//	{
	//		float densityPortion  = static_cast<float>(SpawnPointsData.density) / static_cast<float>(MAX_DENSITY);
	//		Color32 col(densityPortion,densityPortion,densityPortion,1.0);

	//		CVectorMap::DrawMarker(SpawnPointsData.pos, col);
	//	}
	//}
// END DEBUG!!
#endif // __BANK

#if __DEV
	if(ms_displayMiscDegbugTests)
	{
		const u32 timeMs			= fwTimer::GetTimeInMilliseconds();
		const u32 timeDeltaMs	= fwTimer::GetTimeStepInMilliseconds();

		MiscDebugTests(timeMs, timeDeltaMs);
	}
#endif	// __DEV

	// Update the spawning of patrolling henchmen
	CPatrolRoutes::UpdateHenchmanSpawning(ms_PopCtrl.m_centre);

	// Reset the the cam based population control flag.
	ms_useTempPopControlPointThisFrame = false;

	ms_bInstantFillPopulation = Max(0, ms_bInstantFillPopulation-1);

    // update the network visibility checks data
    if(ms_disableMPVisibilityTestTimer > 0)
    {
        ms_disableMPVisibilityTestTimer -= rage::Min(fwTimer::GetTimeStepInMilliseconds(), ms_disableMPVisibilityTestTimer);
    }

    static const u32 DISABLE_MP_VISIBILITY_CHECK_UPDATE_INTERVAL = 10000;

    if((fwTimer::GetSystemTimeInMilliseconds() - ms_lastMPVisibilityFailStartTime) >= DISABLE_MP_VISIBILITY_CHECK_UPDATE_INTERVAL)
    {
        ms_lastMPVisibilityFailStartTime = fwTimer::GetSystemTimeInMilliseconds();
        ms_mpVisibilityFailureCount      = 0;
    }

	// Background re-initialisation of ped anim directors
	if (ms_bDoBackgroundPedReInitThisFrame)
	{
		ReInitAPedAnimDirector();
	}
	ms_bDoBackgroundPedReInitThisFrame = true; // Reset flag.

	// Make sure that objects in the pop control sphere are real, so that we can have scenario peds use them.
	if(ms_useTempPopControlSphereThisFrame)
	{
		spdSphere sp = spdSphere(VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPopPos()), ScalarV(ms_TempPopControlSphereRadius));
		CObjectPopulation::SetScriptForceConvertVolumeThisFrame(sp);

#if __BANK		
		if(ms_bLoadCollisionAroundForcedPopCenter)
		{
			g_StaticBoundsStore.GetBoxStreamer().OverrideRequiredAssets(fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
			g_StaticBoundsStore.GetBoxStreamer().Deprecated_AddSecondarySearch(VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPopPos()));
		}
#endif
	}
	BANK_ONLY(ms_displayPopControlSphereDebugThisFrame = ms_useTempPopControlSphereThisFrame); // keep track of whether we used the pop control sphere this frame, for debug output
	
#if __BANK
	if((fwTimer::GetSystemTimeInMilliseconds() - ms_populationFailureData.m_counterTimer) >= 1000)
	{
		ms_populationFailureData.Reset(fwTimer::GetSystemTimeInMilliseconds());
	}
#endif
}

// Fill the area around the player with peds
void CPedPopulation::ProcessInstantFill(const bool atStart)
{
	const s32 maxPedCreationAttemptsPerFrame = (atStart) ? (ms_maxPedCreationAttemptsPerFrameOnInit) : (ms_maxPedCreationAttemptsPerFrameNormal);

	int iNumOuterIterations = 25;
	bool bFirstIter = true;

	while(HowManyMoreAmbientPedsShouldBeGenerated() > 0 && iNumOuterIterations--)
	{
		// Update pedgen params in CPathServer if we are getting new coords as background task
		// Copy new set of coords into g_CachedPedGenCoords if they are available.
		PF_PUSH_TIMEBAR("InstantFill PathServerRequests");
		if(bFirstIter && (ms_bFirstForcePopFillIter || ms_bInstantFillPopulation == MAX_INSTANT_FILL))
		{
			// Abort any path-requests which might otherwise occupy the pathserver thread
			CPathServer::ForceAbortCurrentPathRequest(false);
			// Ensure that the pathserver performs pedgen, etc - and doesn't service any further path requests
			CPathServer::ForcePerformThreadHousekeeping(true);
			// Force ped spawnpoints search to start at the beginning
			CPathServer::GetAmbientPedGen().NotifyNavmeshesHaveChanged();
			// Wake up the pathserver, if its asleep
			CPathServer::SignalRequest();

			if(ms_bFirstForcePopFillIter)
			{
				ms_bFirstForcePopFillIter = false;
			}
		}

		// Ensure that the pathserver performs pedgen, etc - and doesn't service any further path requests
		CPathServer::ForcePerformThreadHousekeeping(true);

		// Set up pedgen params to pathserver
		SetUpParamsForPathServerPedGenBackgroundTask(ms_popGenKeyholeShape);

		if(CPathServer::GetAmbientPedGen().AreCachedPedGenCoordsReady())
		{
			CPathServer::GetAmbientPedGen().GetCachedPedGenCoords(g_CachedPedGenCoords);
		}
		PF_POP_TIMEBAR();

		// Update the wildlife spawning system.
		PF_PUSH_TIMEBAR("InstantFill CWildlifeManager::Process");
		CWildlifeManager::GetInstance().Process(g_CachedPedGenCoords.GetNumRemaining() > 0);
		PF_POP_TIMEBAR();

		if(ms_allowAmbientPeds)
		{
			PF_PUSH_TIMEBAR("InstantFill AddAmbientPedToPopulation");
			s32 tries = 0;
			while(HowManyMoreAmbientPedsShouldBeGenerated() > 0 && tries < maxPedCreationAttemptsPerFrame)
			{
				// There are spawning systems that don't rely on cached points now.
				AddAmbientPedToPopulation(ms_CurrentCalculatedParams.bInAnInterior, ms_CurrentCalculatedParams.fAddRangeInViewMin, ms_CurrentCalculatedParams.fAddRangeOutOfViewMin, ms_CurrentCalculatedParams.doInFrustumTest, g_CachedPedGenCoords.GetNumRemaining() > 0, atStart);
				++tries;
			}
			PF_POP_TIMEBAR();
		}

		bFirstIter = false;
	}
}

void CPedPopulation::IncrementNumPedsCreatedThisCycle()
{
	++ms_iNumPedsCreatedThisCycle;
}

void CPedPopulation::IncrementNumPedsDestroyedThisCycle()
{
	++ms_iNumPedsDestroyedThisCycle;
}

LocInfo CPedPopulation::ClassifyInteriorLocationInfo( const bool bPlayerIsFocus, const CInteriorInst * pInterior, const int iRoom )
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();

	if(pInterior && iRoom > 0)
	{
		// When in an interior always act as if we are only in a
		// one when in or attached to a vehicle (since the can
		// generally move faster than on foot and may leave the
		// interior quickly).
		if( bPlayerIsFocus && pPlayer && pPlayer->IsInOrAttachedToCar() )
		{
			return LOCATION_SHALLOW_INTERIOR;
		}
		else if(pInterior->GetNumExitPortalsInRoom(iRoom) == 0)
		{
			return LOCATION_DEEP_INTERIOR;
		}
		else
		{
			return LOCATION_SHALLOW_INTERIOR;
		}
	}

	return LOCATION_EXTERIOR;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPopulationControlPointInfo
// PURPOSE :	Get the interest point about which to base the population.
//				Make sure to pick a position and forward vector designed to
//				get the best end visual results out of the population
//				management system.
//				NOTE: We might need to change this for multi-player so that
//				an equal population counts are created around each player.
// PARAMETERS :	None
// RETURNS :	The control point info...  Which has:
//				m_centre - The center about which to base creation and culling.
//				m_baseHeading - The base heading to guide creation and culling
//				m_locationInfo - Whether or not the camera is in an interior or not.
//				m_centreVelocity - The speed and direction of the center.
//				m_conversionCentre - The center about which to base dummy conversions.
/////////////////////////////////////////////////////////////////////////////////
CPopCtrl CPedPopulation::GetPopulationControlPointInfo(CPopCtrl * pTempOverride)
{
	CPopCtrl popCtrl;

	popCtrl.m_inCar = false;
#if __DEV
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	const bool isUsingDebugCam = debugDirector.IsFreeCamActive();
	if(isUsingDebugCam && ms_movePopulationControlWithDebugCam)
	{
		// Base the population control off the location and heading of the debug cam.
		const camFrame& freeCamFrame = debugDirector.GetFreeCamFrame();
		popCtrl.m_centre = freeCamFrame.GetPosition();
		popCtrl.m_baseHeading = freeCamFrame.ComputeHeading();
		popCtrl.m_locationInfo = LOCATION_EXTERIOR;
		popCtrl.m_centreVelocity = Vector3(0.0f, 0.0f, 0.0f);
		popCtrl.m_conversionCentre = popCtrl.m_centre;
	}
	else
#endif // __DEV
	{
		if(pTempOverride)
		{
			popCtrl.m_baseHeading		= pTempOverride->m_baseHeading;
			popCtrl.m_centre			= pTempOverride->m_centre;
			popCtrl.m_locationInfo		= pTempOverride->m_locationInfo;
			popCtrl.m_centreVelocity	= pTempOverride->m_centreVelocity;
			popCtrl.m_conversionCentre	= pTempOverride->m_conversionCentre;
			popCtrl.m_FOV				= pTempOverride->m_FOV;
			popCtrl.m_tanHFov			= pTempOverride->m_tanHFov;
		}
		else
		{
			// Obtain the dominant camera director, excluding the debug director
			// Use the dominant director's forward heading to drive the population code

			const camBaseDirector * pDominantDirector = NULL;
			const atArray<tRenderedCameraObjectSettings> & cameras = camInterface::GetRenderedDirectors();

			if(cameras.GetCount())
			{
				float fMaxBlend = 0.0f;
				for(s32 i=0; i<cameras.GetCount(); i++)
				{
					if(cameras[i].m_Object != &camInterface::GetDebugDirector() && cameras[i].m_BlendLevel > fMaxBlend + SMALL_FLOAT)
					{
						pDominantDirector = static_cast<const camBaseDirector*>(cameras[i].m_Object.Get());
					}
				}
			}

			if(pDominantDirector)
			{
				popCtrl.m_baseHeading = pDominantDirector->GetFrame().ComputeHeading();
				popCtrl.m_FOV = pDominantDirector->GetFrame().GetFov();
				popCtrl.m_tanHFov = gVpMan.GetViewportStackDepth() ? camInterface::GetViewportTanFovH() : 1.0f;
			}
			else
			{
				popCtrl.m_baseHeading = camInterface::GetGameplayDirector().GetFrame().ComputeHeading();
				popCtrl.m_FOV = camInterface::GetGameplayDirector().GetFrame().GetFov();
				popCtrl.m_tanHFov = gVpMan.GetViewportStackDepth() ? camInterface::GetViewportTanFovH() : 1.0f;
			}

			// Init FOV

			/*
			const CViewport * viewport = gVpMan.GetGameViewport(); 
			const float aspectRatio = viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);  
			popCtrl.m_tanHFov = tanf(popCtrl.m_FOV * 0.5f * DtoR) * aspectRatio;
			*/

			popCtrl.m_centre = CFocusEntityMgr::GetMgr().GetPopPos();

			// Check if the focus entity or location is inside an interior
			// This is used to make the population outside LOD more strongly.
			const CEntity* pFocusEntity = CFocusEntityMgr::GetMgr().GetEntity();
			if(!pFocusEntity || !pFocusEntity->GetIsInInterior())
			{
				popCtrl.m_locationInfo = LOCATION_EXTERIOR;
			}
			else if (pFocusEntity && pFocusEntity->GetIsDynamic())
			{
				const CDynamicEntity* pDynamicFocusEntity = static_cast<const CDynamicEntity*> (pFocusEntity);
				CPortalTracker* pPortalTracker = const_cast<CPortalTracker*>(pDynamicFocusEntity->GetPortalTracker());
				Assert(pPortalTracker);
				const s32				roomIdx		= pPortalTracker->m_roomIdx;
				CInteriorInst*	pIntInst	= pPortalTracker->GetInteriorInst();
				popCtrl.m_locationInfo = ClassifyInteriorLocationInfo(true, pIntInst, roomIdx);
			}

			// Use the focus entity's speed to control the population
			popCtrl.m_centreVelocity = CFocusEntityMgr::GetMgr().GetPopVel();
			popCtrl.m_conversionCentre = popCtrl.m_centre;
			popCtrl.m_inCar = ( CFocusEntityMgr::GetMgr().IsPlayerPed() && CGameWorld::FindLocalPlayerVehicle()!=NULL );
		}
	}

	// During scripted cut-scene cameras, do dummy conversions about the
	// camera so we have good looking non dummies close to the camera, but
	// don't modify the creation and culling because the scripted movement
	// would cause lots of population flux and it is up to the designers to
	// make sure the current population looks good.

	if(ms_scriptedConversionCentreActive)
	{
		popCtrl.m_conversionCentre = ms_scriptedConversionCenter;
	}
	else if(camInterface::GetScriptDirector().IsRendering())
	{
		popCtrl.m_conversionCentre = camInterface::GetScriptDirector().GetFrame().GetPosition();
	}

	return popCtrl;
}


/////////////////////////////////////////////////////////////////////////////////
// Population control functions.
/////////////////////////////////////////////////////////////////////////////////

// Give game some time to fill in the world (without regard for visibility or
// distance from the player) before processing the population normally.
void CPedPopulation::ForcePopulationInit(const u32 timeAheadOfCurrentToStop)
{
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	u32 nextTime = curTime + timeAheadOfCurrentToStop;
	u32 startTime = ms_timeWhenForcedPopInitStarted;
	if (nextTime > ms_timeWhenForcedPopInitEnds)
	{
		ms_timeWhenForcedPopInitStarted = (startTime == 0) ? curTime : startTime;
		ms_timeWhenForcedPopInitEnds = nextTime;
		ms_bFirstForcePopFillIter = true;
	}
}

bool CPedPopulation::HasForcePopulationInitFinished()
{
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	u32 startTime = ms_timeWhenForcedPopInitStarted;
	u32 timeDiff = Max(curTime, startTime) - Min(curTime, startTime);
	const u32 kMinTimeOfForcedPopInit = 100;
	return (ms_timeWhenForcedPopInitEnds < curTime || (timeDiff > kMinTimeOfForcedPopInit && ms_scheduledScenarioPeds.GetCount() <= 0 && ms_scheduledAmbientPeds.GetCount() <= 0));
}
void CPedPopulation::SetAdjustPedSpawnPointDensityThisFrame(const s32 iModifier, const Vector3 & vMin, const Vector3 & vMax)
{
	Assertf(ms_iAdjustPedSpawnDensityThisFrame==0, "SetAdjustPedSpawnPointDensityThisFrame() has already been called this frame.");

	ms_iAdjustPedSpawnDensityThisFrame = iModifier;
	ms_vAdjustPedSpawnDensityMin = vMin;
	ms_vAdjustPedSpawnDensityMax = vMax;
}
void CPedPopulation::SetDebugOverrideMultiplier(const float value)
{
	ms_debugOverridePedDensityMultiplier = value;
}
float CPedPopulation::GetDebugOverrideMultiplier()
{
	return ms_debugOverridePedDensityMultiplier;
}
void CPedPopulation::SetAmbientPedDensityMultiplier(float multiplier)
{
	ms_pedDensityMultiplier = multiplier;
}
void CPedPopulation::SetScriptAmbientPedDensityMultiplierThisFrame(float multiplier)
{
	ms_scriptPedDensityMultiplierThisFrame = Min(ms_scriptPedDensityMultiplierThisFrame, multiplier);
}
void CPedPopulation::SetScriptedRangeMultiplierThisFrame(float multiplier)
{
	ms_fScriptedRangeMultiplierThisFrame = multiplier;
}
float CPedPopulation::GetAmbientPedDensityMultiplier()
{
	return ms_pedDensityMultiplier * ms_scriptPedDensityMultiplierThisFrame;
}
float CPedPopulation::GetTotalAmbientPedDensityMultiplier()
{
	return ms_pedDensityMultiplier * ms_scriptPedDensityMultiplierThisFrame * ms_debugOverridePedDensityMultiplier;
}
void CPedPopulation::SetScenarioPedDensityMultipliers( float fInteriorMult, float fExteriorMult )
{
	ms_scenarioPedInteriorDensityMultiplier = fInteriorMult;
	ms_scenarioPedExteriorDensityMultiplier = fExteriorMult;
}
void CPedPopulation::SetScriptScenarioPedDensityMultipliersThisFrame( float fInteriorMult, float fExteriorMult )
{
	ms_scriptScenarioPedInteriorDensityMultiplierThisFrame = Min(ms_scriptScenarioPedInteriorDensityMultiplierThisFrame, fInteriorMult);
	ms_scriptScenarioPedExteriorDensityMultiplierThisFrame = Min(ms_scriptScenarioPedExteriorDensityMultiplierThisFrame, fExteriorMult);
}
void CPedPopulation::GetScenarioPedDensityMultipliers( float& fInteriorMult_out, float& fExteriorMult_out )
{
	fInteriorMult_out = ms_scenarioPedInteriorDensityMultiplier * ms_scriptScenarioPedInteriorDensityMultiplierThisFrame;
	fExteriorMult_out = ms_scenarioPedExteriorDensityMultiplier * ms_scriptScenarioPedExteriorDensityMultiplierThisFrame;
}
void CPedPopulation::GetTotalScenarioPedDensityMultipliers( float& fInteriorMult_out, float& fExteriorMult_out )
{
	fInteriorMult_out = ms_scenarioPedInteriorDensityMultiplier * ms_scriptScenarioPedInteriorDensityMultiplierThisFrame * ms_debugOverridePedDensityMultiplier;
	fExteriorMult_out = ms_scenarioPedExteriorDensityMultiplier * ms_scriptScenarioPedExteriorDensityMultiplierThisFrame * ms_debugOverridePedDensityMultiplier;
}
void CPedPopulation::SetSuppressAmbientPedAggressiveCleanupThisFrame()
{
	ms_suppressAmbientPedAggressiveCleanupThisFrame = true;
}
void CPedPopulation::SetAllowDummyConversions(bool allow)
{
	ms_allowDummyCoversions = allow;
}
void CPedPopulation::SetForcedNewAmbientPedType(s32 forceAllNewAmbientPedsType)
{
	ms_forceCreateAllAmbientPedsThisType = forceAllNewAmbientPedsType;
}
void CPedPopulation::SetAllowCreateRandomCops(bool allowCreateRandomCops)
{
	ms_allowCreateRandomCops = allowCreateRandomCops;
}
void CPedPopulation::SetAllowCreateRandomCopsOnScenarios(bool allowCreateRandomCops)
{
	ms_allowCreateRandomCopsOnScenarios = allowCreateRandomCops;
}
bool CPedPopulation::GetAllowCreateRandomCops()
{
	return ms_allowCreateRandomCops;
}
bool CPedPopulation::GetAllowCreateRandomCopsOnScenarios()
{
	return ms_allowCreateRandomCopsOnScenarios;
}
void CPedPopulation::SetPopCycleMaxPopulationScaler(float scaler)
{
	ms_popCycleMaxPopulationScaler = scaler;
}
float CPedPopulation::GetPopCycleMaxPopulationScaler()
{
	return ms_popCycleMaxPopulationScaler * ms_pedMemoryBudgetMultiplier;
}

int CPedPopulation::GetNumScheduledPedsInVehicles()
{
	return ms_scheduledPedsInVehicles.GetCount();
}

void CPedPopulation::ResetPerFrameScriptedMultipiers()
{
	ms_suppressAmbientPedAggressiveCleanupThisFrame = false;
	ms_scriptPedDensityMultiplierThisFrame = 1.0f;
	ms_scriptScenarioPedInteriorDensityMultiplierThisFrame = 1.0f;
	ms_scriptScenarioPedExteriorDensityMultiplierThisFrame = 1.0f;
	ms_fScriptedRangeMultiplierThisFrame = 1.0f;

#if __BANK
	if(m_iDebugAdjustSpawnDensity != 0)
	{
		ms_iAdjustPedSpawnDensityThisFrame = m_iDebugAdjustSpawnDensity;
		ms_vAdjustPedSpawnDensityMin = CPhysics::g_vClickedPos[0];
		ms_vAdjustPedSpawnDensityMax = CPhysics::g_vClickedPos[1];
	}
	if(m_fDebugScriptedRangeMultiplier != 1.0f)
	{
		ms_fScriptedRangeMultiplierThisFrame = m_fDebugScriptedRangeMultiplier;
	}
#endif

	ms_iAdjustPedSpawnDensityLastFrame = ms_iAdjustPedSpawnDensityThisFrame;
	ms_iAdjustPedSpawnDensityThisFrame = 0;

	// reset that we're using a temp pop control sphere, so it doesn't get stuck
	if(ms_useTempPopControlSphereThisFrame BANK_ONLY(&& !ms_forcePopControlSphere))
	{
		ClearTempPopControlSphere();
	}
	
	ms_scriptedConversionCentreActive = false;
}

#if __BANK
s32 CPedPopulation::GetPopCycleOverrideNumAmbientPeds()
{
	return ms_popCycleOverrideNumberOfAmbientPeds;
}
s32 CPedPopulation::GetPopCycleOverrideNumScenarioPeds()
{
	return ms_popCycleOverrideNumberOfScenarioPeds;
}
#endif //  __BANK


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AddPed
// PURPOSE :	Adds a single ped to the world.
// PARAMETERS :	pedModelIndex - which model to display for the ped
//				newPedGroundPos - position to add the ped
// RETURNS :	The ped that was created.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedPopulation::AddPed(	u32 pedModelIndex,
								const Vector3& newPedRootPos,
								const float fHeading,
								CEntity* pEntityToGetInteriorDataFrom,
								bool hasInteriorData,
								s32 roomIdx,
								CInteriorInst* pInteriorInst,
								bool giveProps,
								bool giveAmbientProps,
								bool bGiveDefaultTask,
								bool checkForScriptBrains,
								const DefaultScenarioInfo& defaultScenarioInfo,
								bool bCreatedForScenario,
								bool createdByScript,
								u32 modelSetFlags, /*= 0*/
								u32 modelClearFlags, /*= 0*/
								const CAmbientModelVariations* variations,
								CPopCycleConditions* pExtraConditions,
								bool inCar,
								u32 npcHash,
								bool bScenarioPedCreatedByConcealeadPlayer)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::PEDPOPULATION);

	RAGE_TIMEBARS_ONLY(pfAutoMarker addPedMarker("AddPed", 30));

	// Set up the new ped matrix.
	Matrix34 tempMat;
	tempMat.Identity();
	tempMat.MakeRotateZ(fHeading);
	tempMat.d = newPedRootPos;

	// Setup the control type.
	const CControlledByInfo localAiControl(false, false);

	// try to salvage a ped from the cache before creating a new one
	fwModelId pedModelId((strLocalIndex(pedModelIndex)));

	bool reusePed = false;
	int pedReuseIndex = FindPedToReuse(pedModelIndex);

	if(pedReuseIndex != -1)
		reusePed = true;

	const float fMinFadeDist = ms_fMinFadeDistance;
	const bool  bDontFadeAtStartup = ms_bDontFadeDuringStartupMode;
	const bool  bInStartupMode = ShouldUseStartupMode();

	// FA: done in ped factory instead
	//// Make sure there are enough spaces free in the pool to create the ped.
	//if(CPed::GetPool()->GetNoOfFreeSpaces() <= 0)
	//{
	//	popDebugf3("Failed to created ped, no room in ped pool");
	//	BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(bCreatedForScenario?PedPopulationFailureData::ST_Scenario:PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_poolOutOfSpace, newPedRootPos));
	//	return NULL;
	//}

	// Use the ped factory to create the ped.
	// Try to create the ped, note that the pPed can be NULL if the pool is
	// full or in a network game if there is no available network object for
	// the new ped.
	CPed* pPed = NULL;
	if(reusePed)
	{
		pPed = GetPedFromReusePool(pedReuseIndex);

#if __BANK
		if(pPed->GetCurrentPhysicsInst() && pPed->GetCurrentPhysicsInst()->IsInLevel())
		{
			pedReuseDebugf1("[B*1972222] Ped from re-use pool (%s) is already in the level. Inst level index: %i", pPed->GetDebugName(), pPed->GetCurrentPhysicsInst()->GetLevelIndex());
			CEntity *pEnt = CPhysics::GetEntityFromInst(pPed->GetCurrentPhysicsInst());
			if(pEnt && pEnt->GetIsTypePed())
			{
				pedReuseDebugf1("[B*1972222] Ped inst entity is (%s) | %i", static_cast<CPed *>(pEnt)->GetDebugName(), pEnt == pPed);
			}			
		}
#endif

		bool shouldBeCloned = true; // we want to clone this ped in network games
		if(ReusePedFromPedReusePool(pPed, tempMat, localAiControl, createdByScript, shouldBeCloned, pedReuseIndex, bScenarioPedCreatedByConcealeadPlayer))
		{
			RemovePedFromReusePool(pedReuseIndex);
		}
		else
		{
			// failed to reuse the ped (network registration issue?)
			pPed = NULL;
		}
#if __BANK
		CPedFactory::LogCreatedPed(pPed, "CPedPopulation::AddPed()", grcDebugDraw::GetScreenSpaceTextHeight(), Color_violet);
#endif
	}
	else
	{
		pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &tempMat, false, true, createdByScript, true, bScenarioPedCreatedByConcealeadPlayer);
	}

	if (!pPed)
	{
		popDebugf3("Failed to created ped, ped factory returned null");
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(bCreatedForScenario?PedPopulationFailureData::ST_Scenario:PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_factoryFailedToCreatePed, newPedRootPos));
		return NULL;
	}

#if __ASSERT
	if (pPed->GetPedModelInfo()->ShouldSpawnInWaterByDefault() && !createdByScript && !bCreatedForScenario)
	{
		float fWaterZ;
		if (Verifyf(Water::GetWaterLevelNoWaves(newPedRootPos, &fWaterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL), "Aquatic ped spawned somewhere without water.  Please pause the game, look for nearby fish/sharks, and attach a video showing all AI debug information."))
		{
			Assertf(fWaterZ > newPedRootPos.z, "Swimming ped spawned above the water line.  Please pause the game, look for nearby fish/sharks, and attach a video showing all AI debug information.");
		}
	}
#endif

	pPed->SetPedResetFlag(CPED_RESET_FLAG_SpawnedThisFrameByAmbientPopulation, true);

	if(bCreatedForScenario && pPed->GetNetworkObject())
	{
		CNetObjPed *netObjPed = static_cast<CNetObjPed *>(pPed->GetNetworkObject());
		netObjPed->SetOverrideScopeBiggerWorldGridSquare(true); //set this now before CGameWorld::Add so scope checks will know this is a scenario ped immediately
	}

#if __STATS
	PF_INCREMENT(AmbientPedsCreated);
	if(reusePed)
		PF_INCREMENT(AmbientPedsReused);
#endif


	atFixedBitSet32 incs;
	atFixedBitSet32 excs;
	if (npcHash != 0)
	{
		pPed->ApplySelectionSetByHash(npcHash);
		giveProps = false; // the npc selection set can set exact props needed
	}
	else
	{
		// Setup the ped's variation information.
		// Randomize this ped with attributes and different clothes and skin colour, etc.
		if (CPedType::IsCopType(pPed->GetPedType()))
		{
			if (g_weather.GetRain() > 0.1f && !inCar) // cops in cars should not wear their rain gear
			{
				modelClearFlags |= PV_FLAG_SUNNY;
			}
			else
			{
				modelClearFlags |= PV_FLAG_WET;
			}
		}
		modelClearFlags |= PV_FLAG_BIKE_ONLY;									// prohibit "bike only" items for regular peds

		if (CClock::GetHour() > 18 || CClock::GetHour() < 8)
			modelClearFlags |= PV_FLAG_SUNNY;

		pPed->SetVarRandom(modelSetFlags, modelClearFlags, &incs, &excs, variations, PV_RACE_UNIVERSAL);
	}

	// Set ped position and add the ped into the world (possibly in an interior).
	pPed->SetPosition(newPedRootPos);
	pPed->SetHeading(fHeading);
	pPed->SetDesiredHeading(fHeading);

	fwInteriorLocation	interiorLocation;
	if(pEntityToGetInteriorDataFrom)
	{
		interiorLocation = pEntityToGetInteriorDataFrom->GetInteriorLocation();
	}
	else if(hasInteriorData)
	{
		interiorLocation = CInteriorProxy::CreateLocation(pInteriorInst->GetProxy(), roomIdx);
	}
	else
	{
		interiorLocation.MakeInvalid();
	}

	// Determine whether this ped should alpha-fade in.
	// Only if created in the view frustum, in the far distance
	// And we are not in startup mode already
	
	if((!bDontFadeAtStartup || !bInStartupMode) && IsCandidateInViewFrustum( newPedRootPos, pPed->GetCapsuleInfo()->GetHalfWidth(), 9999.0f, fMinFadeDist ) )
	{
		if(interiorLocation.IsValid())
		{
			// creating a ped in an interior
			if(FindPlayerPed() && FindPlayerPed()->GetInteriorLocation().IsValid())
			{
				// player is in an interior
				pPed->GetLodData().SetResetDisabled(false);
				pPed->GetLodData().SetResetAlpha(true);
			}
			else
			{
				// player is in an exterior, don't reset alpha
			}
		}
		else
		{
			// exterior
			pPed->GetLodData().SetResetDisabled(false);
			pPed->GetLodData().SetResetAlpha(true);
		}
	}

#if __BANK
	if(m_bLogAlphaFade)
	{
		static char text[256];

		CViewport* gameViewport = gVpMan.GetGameViewport();

		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vCameraPos = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());
		const float fDist = (vCameraPos - vPedPos).Mag();

		Color32 iTextCol = (pPed->GetLodData().IsResetDisabled()==false && pPed->GetLodData().GetResetAlpha()==true) ? Color_green: Color_red;

		grcDebugDraw::Sphere(vPedPos, 0.25f, Color_orange, false, (s32)(10.0f / fwTimer::GetTimeStep()));

		sprintf(text, "FRAME: %i, MODEL: %s", fwTimer::GetFrameCount(), pPed->GetModelName());
		grcDebugDraw::Text(vPedPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight() * 0, text, true, (s32)(10.0f / fwTimer::GetTimeStep()));
		sprintf(text, "fwLodData::m_bDisableReset : %s", pPed->GetLodData().IsResetDisabled() ? "true":"false");
		grcDebugDraw::Text(vPedPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight() * 1, text, false, (s32)(10.0f / fwTimer::GetTimeStep()));
		sprintf(text, "fwLodData::m_bResetAlphaNextUpdate : %s", pPed->GetLodData().GetResetAlpha() ? "true":"false");
		grcDebugDraw::Text(vPedPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight() * 2, text, false, (s32)(10.0f / fwTimer::GetTimeStep()));

		bool bInFrustum = IsCandidateInViewFrustum( vPedPos, pPed->GetCapsuleInfo()->GetHalfWidth(), 9999.0f, fMinFadeDist );

		Displayf("**************************************************************************************************");
		Displayf("FRAME: %i, NAME : %s, DISTANCE : %.1f, POSITION : %.1f, %.1f, %.1f, PED : 0x%p",
			fwTimer::GetFrameCount(), pPed->GetModelName(), fDist, vPedPos.x, vPedPos.y, vPedPos.z, pPed
			);
		Displayf("bAtStartup : %s, bInstantFill : %s, bInFrustum : %s, fwLodData::m_bDisableReset : %s, fwLodData::m_bResetAlphaNextUpdate : %s",
			CPedPopulation::ShouldUseStartupMode() ? "true":"false",
			CPedPopulation::GetInstantFillPopulation() ? "true":"false",
			bInFrustum ? "true":"false",
			pPed->GetLodData().IsResetDisabled() ? "true":"false",
			pPed->GetLodData().GetResetAlpha() ? "true":"false"
			);
		sysStack::PrintStackTrace();
		Displayf("**************************************************************************************************");

	}
#endif

	// check whether the interior is in a state that can accept entities
	CInteriorProxy* pIntProxy = CInteriorProxy::GetFromLocation(interiorLocation);
	if ( interiorLocation.IsValid() && (!(pIntProxy->GetInteriorInst() && pIntProxy->GetInteriorInst()->CanReceiveObjects())) )
	{
		Assertf( false, "Trying to add an entity to an interior that is not populated. Adding to the world instead." );
		interiorLocation.MakeInvalid();
	}

	CGameWorld::Add(pPed, interiorLocation);

	// We should set their removal delay timer to make sure that even if they are
	// out of range or out of view that they don't get removed for a little while.
	// This prevents nasty population hysteresis issues.
	pPed->DelayedRemovalTimeReset();
	pPed->DelayedConversionTimeReset();

	// Update the population counts.
	Assert(!pPed->IsInOnFootPedCount());
	Assert(!pPed->IsInInVehPedCount());
	pPed->PopTypeSet(bCreatedForScenario?POPTYPE_RANDOM_SCENARIO:POPTYPE_RANDOM_AMBIENT);
	((CPedModelInfo*)pPed->GetBaseModelInfo())->IncreaseNumTimesUsed();

	// Check if we should give them a default task.
	if(bGiveDefaultTask)
	{
		// Create and add a default task for this wandering ped.
		// This picks a random direction to wander in.
		CTask* pDefaultTask = pPed->ComputeWanderTask(*pPed, pExtraConditions);
		if(pDefaultTask)
		{
			const int taskType = pDefaultTask->GetTaskType();
			if(taskType == CTaskTypes::TASK_UNALERTED)
			{
				// Attempt to create a wander prop on creation
				static_cast<CTaskUnalerted*>(pDefaultTask)->CreateProp();
			}
			else if(taskType == CTaskTypes::TASK_WANDER)
			{
				// Attempt to create a wander prop on creation
				static_cast<CTaskWander*>(pDefaultTask)->CreateProp();
			}
		}
		pPed->GetPedIntelligence()->AddTaskDefault(pDefaultTask);

		// Possibly assign a scenario to this ped instead
		if(defaultScenarioInfo.GetType() != DS_Invalid)
		{
			CScenarioManager::ApplyDefaultScenarioInfoForRandomPed(defaultScenarioInfo, pPed);
		}

		if (pExtraConditions && pExtraConditions->m_bSpawnInAir)
		{
			// Tell the motion task to start this bird off in the air.
			pPed->GetMotionData()->SetIsFlyingForwards();
		}
	}

	// Check if they need some script brains given to them.
	if(checkForScriptBrains)
	{
		CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pPed, CScriptsForBrains::PED_STREAMED);
	}

	const CPedPropRestriction* pPassedRestriction = NULL;
	if (bCreatedForScenario)
	{
		static const CPedPropRestriction earRestriction(ANCHOR_EARS, -1, CPedPropRestriction::MustUse); //i.e., must use an invalid Ear prop, meaning you can't use any ear prop
		pPassedRestriction = &earRestriction;
	}


	// Make sure the ped is properly equipped (this must be done after tasks have been assigned).
	EquipPed(pPed, true, giveProps, giveAmbientProps, &incs, &excs, modelSetFlags, modelClearFlags, variations, pPassedRestriction);

	// This is necessary to make sure the rest of the variation stuff is setup correctly...
	// Note this doesn't need a ped, just the member data...
	if (!pPed->GetPedModelInfo()->GetIsStreamedGfx())
		CPedVariationPack::RenderShaderSetup(pPed->GetPedModelInfo(), pPed->GetPedDrawHandler().GetVarData(), (CCustomShaderEffectPed*)(pPed->GetPedDrawHandler().GetShaderEffect()));

#if __DEV
	// Do some debug display.
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(ms_bCreatingPedsForAmbientPopulation)
	{
		VecMapDisplayPedPopulationEvent(vPedPosition, Color32(128,128,255,255));
	}
	else if(ms_bCreatingPedsForScenarios)
	{
		VecMapDisplayPedPopulationEvent(vPedPosition, Color32(255,128,128,255));
	}
	else
	{
		VecMapDisplayPedPopulationEvent(vPedPosition, Color32(255,255,128,255));
	}
#endif // _DEV

	// Let everyone know the default unarmed weapon is not in the Ped's loadout.
	Assertf(!pPed->GetInventory() || pPed->GetInventory()->GetWeapon(pPed->GetDefaultUnarmedWeaponHash()) != NULL, "Ped's default unarmed weapon: %s is not in its inventory (ped model %s)",
			atHashWithStringNotFinal(pPed->GetDefaultUnarmedWeaponHash()).GetCStr(),
			pPed->GetModelName());

	// Hand back the new ped.
	return pPed;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	EquipPed
// NOTE THIS MUST BE CALLED ONLY AFTER DEFAULT TASKS HAVE BEEN ASSIGNED.
// PURPOSE :	Provides a ped with weapons, addition clothes (like hats and bags),
//				ambient props (like hamburgers and books), and money.
// PARAMETERS :	pPed - the ped to equip.
//				giveWeapon - whether or not to give them a weapon (may not give them
//					one anyway if not appropriate)
//				giveProps - whether or not to give them things like hats and bags.
//				giveAmbientProps - whether or not to give them things like hamburgers
//					and books.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::EquipPed(	CPed* pPed,
								bool giveWeapon,
								bool giveProps,
								bool UNUSED_PARAM(giveAmbientProps),
								atFixedBitSet32* incs,
								atFixedBitSet32* excs,
								u32 setFlags,
								u32 clearFlags,
								const CAmbientModelVariations* variations,
								const CPedPropRestriction* pPassedPropRestriction)
{
	Assert(pPed);
	const ePedType pedType = pPed->GetPedType();

	// Determine what weapon type to give them if any.
	if(giveWeapon)
	{
		u32 weaponHash = pPed->GetDefaultUnarmedWeaponHash();

		if(CPedType::IsGangType(pedType))
		{
			const CGangInfo& gangInfo = CGangs::GetGangInfo(pedType);
			weaponHash = gangInfo.PickRandomWeapon(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));
		}

		// Give them a weapon if appropriate.
		if(weaponHash != pPed->GetDefaultUnarmedWeaponHash() && pPed->GetInventory())
		{
			pPed->GetInventory()->AddWeaponAndAmmo(weaponHash, CWeapon::LOTS_OF_AMMO);
		}
	}

	// Check if we should give props to this ped.
	// We may not want to do this for say, peds already in cars, etc.
	if(giveProps)
	{
		float freq1 = 0.4f;
		float freq2 = 0.25f;
		if (CPedType::IsCopType(pedType))
		{
			freq1 = 1.0f;
			freq2 = 0.4f;
		}

		CPedPropsMgr::ClearAllPedProps(pPed);

		const static int iMaxRestrictions = 16;  // Currently there are no more than 3 prop restrictions per character type. This could increase if needed. [5/24/2013 mdawe]
		atFixedArray<CPedPropRestriction, iMaxRestrictions> allRestrictions;

		int numRestrictions = 0;
		if(variations)
		{
			if(taskVerifyf(variations->GetIsClass<CAmbientPedModelVariations>(), "Got unexpected variations type for ped, expected CAmbientPedModelVariations."))
			{
				const CAmbientPedModelVariations& pedVar = *static_cast<const CAmbientPedModelVariations*>(variations);
			
				numRestrictions = pedVar.m_PropRestrictions.GetCount();
				aiAssert(numRestrictions <= (iMaxRestrictions - 1)); // -1 because we may add another from pPassedPropRestriction 

				const CPedPropRestriction* pRestrictions = pedVar.m_PropRestrictions.GetElements();
				for (int iIndex = 0; iIndex < numRestrictions; ++iIndex)
				{
					allRestrictions.Push(pRestrictions[iIndex]);
				}
			}
		}

		if (pPassedPropRestriction)
		{
			allRestrictions.Push(*pPassedPropRestriction);
			++numRestrictions;
		}

		CPedPropsMgr::SetPedPropsRandomly(pPed, freq1, freq2, setFlags, clearFlags, incs, excs, allRestrictions.GetElements(), numRestrictions);
	}

	// if we have ped model variations
	if (variations && variations->GetIsClass<CAmbientPedModelVariations>())
	{
		const CAmbientPedModelVariations* pModelVariations = static_cast<const CAmbientPedModelVariations*>(variations);
		// if the variation has a loadout specified
		if (pModelVariations->m_LoadOut.IsNotNull())
		{
			// set the loadout
			CPedInventoryLoadOutManager::SetLoadOut(pPed, pModelVariations->m_LoadOut.GetHash());
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_EquippedAmbientLoadOutWeapon, true);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AddPedInCar
// PURPOSE :	Adds a single ped to the world (in a car).
// PARAMETERS :	pVehicle - type of vehicle to put the ped in.
//				bDriver - is this the driver of the car.
//				seat_num - seat position in the car (if not the driver)
// RETURNS :	The ped that was created.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedPopulation::AddPedInCar(CVehicle* pVehicle, bool bDriver, s32 seat_num, bool UNUSED_PARAM(bStrictAboutRightModel),
		bool bUseExistingNodes, bool bCreatedByScript, bool bIgnoreDriverRestriction, u32 desiredModel, const CAmbientModelVariations* variations)
{
	Assert(pVehicle);
	Assert(pVehicle->IsArchetypeSet());

	// Don't try to add peds to vehicles that don't have any seats
	if (pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumSeats() <= 0)
	{
		return NULL;
	}

	// If the vehicle is a taxi or limo we don't create anyone on passenger seat.
	bool bTaxi = false;
	if (CVehicle::IsTaxiModelId(pVehicle->GetModelId()))
		bTaxi = true;

	// If the vehicle is a bus, fill it with civilians
	bool bBus = false;
	if(CVehicle::IsBusModelId(pVehicle->GetModelId()) )
	{
		bBus = true;
	}

	// If the seat passed in is mp warp only, don't spawn peds in the seat
	const s32 iDirectEntryPointIndex = pVehicle->GetDirectEntryPointIndexForSeat(seat_num);
	if (pVehicle->IsEntryIndexValid(iDirectEntryPointIndex))
	{
		// Don't allow peds to spawn in these seats in sp otherwise, unless marked to also be allowed in in sp
		if (pVehicle->GetEntryInfo(iDirectEntryPointIndex)->GetMPWarpInOut() && !pVehicle->GetEntryInfo(iDirectEntryPointIndex)->GetSPEntryAllowedAlso())
		{
			return NULL;
		}
	}

	RAGE_TIMEBARS_ONLY(pfAutoMarker addPedInCar(bBus ? "AddPedInCar - Bus" : "AddPedInCar - Car", 26));	

	// Determine the ped type and model index to try to use based on the car type.
	// Some vehicle types have specific drivers (like garbage men in a garbage truck).
	fwModelId driverPedModelId((strLocalIndex(desiredModel)));
	if(!driverPedModelId.IsValid())
	{
		driverPedModelId = (FindSpecificDriverModelForCarToUse(pVehicle->GetModelId()));
	}

	bool createCivilian = true;
	fwModelId newPedModelId;
	if( ((!bTaxi && !bBus) || bDriver) && driverPedModelId.IsValid())
	{
		createCivilian = false;
		if (!CModelInfo::HaveAssetsLoaded(driverPedModelId))
		{
			if (!bIgnoreDriverRestriction)
			{
				popDebugf2("Failed to add specific driver '%s' in car '%s', ped assets weren't streamed in", CModelInfo::GetBaseModelInfoName(driverPedModelId), CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()));
				// The driver may not have been requested, try again now as a safety
				gPopStreaming.RequestVehicleDriver(pVehicle->GetModelIndex());
				return NULL;
			}
			createCivilian = true;
		}

		newPedModelId = driverPedModelId;
	}

	if (createCivilian)
	{
#if __DEV
		Assertf(!bDriver || gPopStreaming.IsFallbackPedAvailable(), "AddPedInCar no fallback ped. Vehicle:%s", pVehicle->GetModelName());
#endif

		//	Create a normal civilian.
		CPopCycle::VerifyHasValidCurrentPopAllocation();
		newPedModelId = fwModelId(strLocalIndex(ChooseCivilianPedModelIndexForVehicle(pVehicle, bDriver)));
	}

    // we can't drop to fallback peds for bikes or they might spawn with no helmet
	if ((!bDriver || pVehicle->InheritsFromBike()) && !newPedModelId.IsValid()) 
		return NULL;

	// Make sure the ped model is loaded, and if not use a fall back model.
	if(!newPedModelId.IsValid() && gPopStreaming.IsFallbackPedAvailable())
	{
		newPedModelId = fwModelId(strLocalIndex(gPopStreaming.GetFallbackPedModelIndex()));
	}

	if (!Verifyf(newPedModelId.IsValid(), "Failed to find fallback ped as driver for  vehicle '%s'!", pVehicle->GetModelName()))
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_noDriverFallbackPedFound, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())));
		return NULL;
	}
	Assertf(CModelInfo::HaveAssetsLoaded(newPedModelId), "AddPedInCar: Model not streamed in");

	// Add the ped to the world (just at the position of the car for now).
	// Don't give them props when in car, otherwise they might interfere
	// with driving.
	// Don't give them a default task since we will want them doing an
	// in car task(that we assign below).
	DefaultScenarioInfo defaultScenarioInfo(DS_Invalid, NULL);

	// Determine the placement position and heading.
	const s32 seat = seat_num;
	Assert(seat < MAX_VEHICLE_SEATS);
	Assert(seat > -1 );
	const int boneIdx = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(seat);
	Assertf( boneIdx != -1, "Vehicle \'%s\' doesn't have a bone for seat index : %i", CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()), seat );

	if(boneIdx == -1)
	{
		popDebugf2("Failed to add ped in car, vehicle 0x%p (%s) doesn't have a bone for seat %d", pVehicle, CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()), seat);
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_vehicleHasNoSeatBone, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())));
		return NULL;
	}

	// make sure our selected ped has a helmet component
	// Note: if you change the conditions here, you may also need to change the helmet-related
	// conditions in CScenarioManager::SelectPedTypeForScenario() and in ChooseCivilianPedModelIndex().
	if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(newPedModelId);
		if (!Verifyf(pmi->HasHelmetProp() || pmi->GetCanRideBikeWithNoHelmet(), "Tried to created ped '%s' as driver for '%s' but has no bike helmet prop!", pmi->GetModelName(), pVehicle->GetModelName()))
			return NULL;
	}

	Matrix34 seatGlobalBoneMatrix;

	// We used to get this matrix like this:
	//	pVehicle->GetGlobalMtx(boneIdx, seatGlobalBoneMatrix);
	// but there is risk of stalling when accessing a global matrix like that, we might not have
	// finished the animation update this frame yet. Instead, we can compute the position from the skeleton
	// data and the global matrix of the vehicle. This probably doesn't even matter much, the ped will get
	// attached to the proper bone anyway, even before the first frame of existence (through the SetPedInVehicle()
	// call further down).
	const crSkeletonData& skelData = pVehicle->GetSkeletonData();
	skelData.ComputeGlobalTransform(boneIdx, pVehicle->GetMatrixRef(), RC_MAT34V(seatGlobalBoneMatrix));

	const Vector3 placementPos = seatGlobalBoneMatrix.d;
	const float placementHeading = pVehicle->GetTransform().GetHeading();

	u32 setFlags = 0;		// flags which must be set in the randomly selected items
	u32 clearFlags = 0;		// flags which must be clear in the randomly selected items

	if (!pVehicle->InheritsFromBike())
	{
		clearFlags |= PV_FLAG_BULKY;						// prohibit bulky items in cars (are ok for bikes)
		clearFlags |= PV_FLAG_BIKE_ONLY;					// prohibit "bike only" items if not on a bike
		clearFlags |= PV_FLAG_NOT_IN_CAR;					// prohibit "not-in-car" items

		ASSERT_ONLY(CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(newPedModelId));
		popAssertf(!pmi->GetOnlyBulkyItemVariations(),
				"About to spawn ped model '%s' with only bulky items, in vehicle '%s'.",
				pmi->GetModelName(), pVehicle->GetModelName());
	}

	u32 npcHash = 0;
	if (bDriver)
		npcHash = pVehicle->GetNpcHashForDriver(newPedModelId.GetModelIndex());

	// Try to create and add the ped into the world (possibly in an interior).
	// check if the ped needs moved out of the world and into an interior (ie. if the car is in an interior)
	CPed* pPed = NULL;
	if (pVehicle->m_nFlags.bInMloRoom)
	{
		pPed = AddPed(newPedModelId.GetModelIndex(), placementPos, placementHeading, pVehicle, false, -1, NULL, true, false, false, false, defaultScenarioInfo, false, bCreatedByScript, setFlags, clearFlags, variations, NULL, true, npcHash);
	}
	else
	{
		pPed = AddPed(newPedModelId.GetModelIndex(), placementPos, placementHeading, NULL, false, -1, NULL, true, false, false, false, defaultScenarioInfo, false, bCreatedByScript, setFlags, clearFlags, variations, NULL, true, npcHash);
	}

	// CPedFactory::GetFactory()->CreatePed called in AddPed fails silently, so don't assert if the ped is null
	if (!pPed)
	{
		return NULL;
	}

	CVehiclePopulation::IncrementNumPedsCreatedThisCycle();

	//Note that a ped was spawned.
	OnPedSpawned(*pPed, OPSF_InVehicle);

	// Place and pose the ped in car properly.
	u32 iFlags = CPed::PVF_IfForcedTestVehConversionCollision|CPed::PVF_DontApplyEnterVehicleForce;
	if(bUseExistingNodes)
		iFlags |=  CPed::PVF_UseExistingNodes;

	pPed->SetPedInVehicle(pVehicle,seat,iFlags);

	if (!pPed->IsNetworkClone())
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
	}

	popDebugf2("Created ped 0x%p (%s) in vehicle 0x%p (%s)", pPed, CModelInfo::GetBaseModelInfoName(newPedModelId), pVehicle, CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()));
	// Hand back the new ped.
	return pPed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AddPedToPopulationCount
// PURPOSE :	Adds a ped to the population
// PARAMETERS :	pPed - the ped or dummy ped to remove.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::AddPedToPopulationCount(CPed* pPed)
{
	CPedPopulation::UpdatePedCountIfNotAlready(pPed, CPedPopulation::ADD, pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ), pPed->GetModelIndex());
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemovePedFromPopulationCount
// PURPOSE :	Removes a ped from the population
// PARAMETERS :	pPed - the ped or dummy ped to remove.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::RemovePedFromPopulationCount(CPed* pPed)
{
	Assertf((pPed->GetIsTypePed()), "RemovePedFromPopulationCount - passed non ped or dummy ped entity");

	DEV_BREAK_IF_FOCUS( ms_focusPedBreakOnPopulationRemoval, pPed );

	// Remove from both the onFoot and inVeh lists.
	UpdatePedCountIfNotAlready(pPed, SUB, true, pPed->GetModelIndex());
	UpdatePedCountIfNotAlready(pPed, SUB, false, pPed->GetModelIndex());

	DEV_ONLY(VecMapDisplayPedPopulationEvent(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color32(0,0,0,255)));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdatePedCountIfNotAlready
// PURPOSE :	Helps keep track of the number of types of peds in the world.
// PARAMETERS :	pPed- the ped we are interested in.
//				addOrSub - whether this ped is being added or removed from
//				the population.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::UpdatePedCountIfNotAlready(CPed* pPed, PedPopOp addOrSub, bool inVehicle, u32 mi)
{
	Assert(pPed);

	fwModelId modelId = fwModelId(strLocalIndex(mi));
	Assert(modelId.IsValid());

	// Players aren't part of the ped population counts.
	if(pPed->GetIsTypePed() && pPed->IsPlayer())
	{
		return;
	}

	// Determine if the ped has been accounted for already
	// and use it to make sure we actually need to do anything.
	// Just return if we shouldn't to do anything.
	const bool hasBeenAccountedForAlready = (inVehicle)?(pPed->IsInInVehPedCount()):(pPed->IsInOnFootPedCount());
	if((addOrSub == ADD) && (hasBeenAccountedForAlready))
	{
		return;
	}
	if((addOrSub == SUB) && (!hasBeenAccountedForAlready))
	{
		return;
	}

	// Determine how much to modify the ped count by.
	const s32 deltaVal = (addOrSub == ADD)? 1 : -1;

	// Get the ped type of the ped or dummy ped.
	const s32 pedType = pPed->GetPedType();
	const ePopType popType = pPed->PopTypeGet();

	//Change the ped counts
	if (popType == POPTYPE_RANDOM_SCENARIO)
	{
		if(inVehicle)
		{
			ms_nNumInVehScenario += deltaVal;
			Assertf(ms_nNumInVehScenario >= 0, "UpdatePedOrDummyPedCount - ms_nNumInVehScenario is less than 0");
		}
		else
		{
			ms_nNumOnFootScenario += deltaVal;
			Assertf(ms_nNumOnFootScenario >= 0, "UpdatePedOrDummyPedCount - ms_nNumOnFootScenario is less than 0");
		}
	}
	else if (popType == POPTYPE_RANDOM_AMBIENT)
	{
		if(inVehicle)
		{
			if(!pPed->IsNetworkClone())
			{
				ms_nNumInVehAmbient += deltaVal;
				Assertf(ms_nNumInVehAmbient >= 0, "UpdatePedOrDummyPedCount - ms_nNumInVehAmbient is less than 0");
			}
		}
		else
		{
			ms_nNumOnFootAmbient += deltaVal;
			Assertf(ms_nNumOnFootAmbient >= 0, "UpdatePedOrDummyPedCount - ms_nNumOnFootAmbient is less than 0");
		}
	}
	else
	{
		if(inVehicle)
		{
			ms_nNumInVehOther += deltaVal;
			Assertf(ms_nNumInVehOther >= 0, "UpdatePedOrDummyPedCount - ms_nNumInVehOther is less than 0");
		}
		else
		{
			ms_nNumOnFootOther += deltaVal;
			Assertf(ms_nNumOnFootOther >= 0, "UpdatePedOrDummyPedCount - ms_nNumOnFootOther is less than 0");
		}
	}

	// Actually change the ped counts.
	if(	CPedType::IsSinglePlayerType(pedType) ||
		CPedType::IsMPPlayerType(pedType) ||
		CPedType::IsCivilianType(pedType) ||
		CPedType::IsGangType(pedType) ||
		CPedType::IsCriminalType(pedType) ||
		CPedType::IsAnimalType(pedType) )
	{
	}
	else if(CPedType::IsCopType(pedType))
	{
		if(inVehicle)
		{
			ms_nNumInVehCop += deltaVal;
			Assertf(ms_nNumInVehCop >= 0, "UpdatePedOrDummyPedCount - ms_nNumCop is less than 0");
		}
		else
		{
			ms_nNumOnFootCop += deltaVal;
			Assertf(ms_nNumOnFootCop >= 0, "UpdatePedOrDummyPedCount - ms_nNumCop is less than 0");
		}
	}
	else if(CPedType::IsSwatType(pedType))
	{
		if(inVehicle)
		{
			ms_nNumInVehSwat += deltaVal;
			Assertf(ms_nNumInVehSwat >= 0, "UpdatePedOrDummyPedCount - ms_nNumInVehSwat is less than 0");
		}
		else
		{
			ms_nNumOnFootSwat += deltaVal;
			Assertf(ms_nNumOnFootSwat >= 0, "UpdatePedOrDummyPedCount - ms_nNumOnFootSwat is less than 0");
		}
	}
	else if(CPedType::IsEmergencyType(pedType))
	{
		if(inVehicle)
		{
			ms_nNumInVehEmergency += deltaVal;
			Assertf(ms_nNumInVehEmergency >= 0, "UpdatePedOrDummyPedCount - ms_nNumEmergency is less than 0");
		}
		else
		{
			ms_nNumOnFootEmergency += deltaVal;
			Assertf(ms_nNumOnFootEmergency >= 0, "UpdatePedOrDummyPedCount - ms_nNumEmergency is less than 0");
		}
	}
	else
	{
		Errorf("Unknown ped type, UpdatePedOrDummyPedCount, Population.cpp");
	}

	// Make sure to mark the ped or dummy ped, so we don't add them when
	// they've already been added or subtract them before they've been added.
	CPedModelInfo *pModelInfo = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
	if(inVehicle)
	{
		pModelInfo->SetNumTimesInCar(pModelInfo->GetNumTimesInCar()+deltaVal);
		Assert(pModelInfo->GetNumTimesOnFoot() >= 0);

		pPed->SetIsInInVehPedCount(addOrSub == ADD);
	}
	else
	{
		pModelInfo->SetNumTimesOnFoot(pModelInfo->GetNumTimesOnFoot()+deltaVal);
		Assert(pModelInfo->GetNumTimesOnFoot() >= 0);

		pPed->SetIsInOnFootPedCount(addOrSub == ADD);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	LoadSpecificDriverModelsForCar
// PURPOSE:		Makes a streaming request if a certain car types has a certain.
//				specific ped type that drives it.
// PARAMETERS :	carModelIndex - the car type we are interested in.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::LoadSpecificDriverModelsForCar(fwModelId carModelId)
{
	// Get the driver type for this car type (if it has a special one).
	fwModelId driverPedModelId = FindRandomDriverModelForCarToUse(carModelId);
	if(!driverPedModelId.IsValid())
	{
		// The vehicle doesn't have a specific driver ped type.
		return;
	}

	CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(carModelId);
	if (mi && !mi->HasRequestedDrivers())
	{
		for (u32 i = 0; i < mi->GetDriverCount(); ++i)
		{
			fwModelId driverId = fwModelId(strLocalIndex(mi->GetDriver(i)));
			if (!Verifyf(driverId.IsValid(), "Invalid driver '%s' specified on vehicle '%s'", mi->TryGetDriverName(i), mi->GetModelName()))
				continue;
			CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(driverId);
			if (pmi)
				pmi->m_isRequestedAsDriver++;
		}

		mi->SetHasRequestedDrivers(true);
	}

	// Try to load the model if it isn't already.
	CModelInfo::RequestAssets(driverPedModelId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);

#if !__NO_OUTPUT
	CPedModelInfo* pedMi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(driverPedModelId);
	Assert(pedMi);
	popDebugf2("Load driver ped type %s for vehicle type %s requested", pedMi->GetModelName(), CModelInfo::GetBaseModelInfo(carModelId)->GetModelName());
#endif // !__NO_OUTPUT
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemoveSpecificDriverModelsForCar
// PURPOSE :	Tries to remove the driver ped model if this car had a specific
//				driver type (like garbage men for garbage trucks). NOTE: when we load
//				a model we request a random one from the drivers list. When we remove
//				we need to make sure we remove all loaded drivers.
// PARAMETERS :	carModelIndex - the car type we are interested in.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::RemoveSpecificDriverModelsForCar(fwModelId carModelId)
{
	// unload all drivers for this car
	CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(carModelId);
	Assert(mi);
	if (mi && mi->HasRequestedDrivers())
	{
		popDebugf2("Decrementing driver refCount for vehicle type %s", mi->GetModelName());

		for (s32 i = 0; i < mi->GetDriverCount(); ++i)
		{
			if (!CModelInfo::IsValidModelInfo(mi->GetDriver(i)))
				continue;

			// Let it be deleted.
			fwModelId driverId((strLocalIndex(mi->GetDriver(i))));
			CPedModelInfo* pedMi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(driverId);
			Assert(pedMi);
			pedMi->m_isRequestedAsDriver--;
			if (!Verifyf(pedMi->m_isRequestedAsDriver >= 0, "Decrementing too many vehicle driver refs!"))
				pedMi->m_isRequestedAsDriver = 0;

			if (pedMi->m_isRequestedAsDriver == 0)
			{
				CModelInfo::SetAssetsAreDeletable(driverId);
				popDebugf2("Unload driver ped type %s for vehicle type %s requested", pedMi->GetModelName(), CModelInfo::GetBaseModelInfo(carModelId)->GetModelName());
			}
			else
			{
				popDebugf2("Can't unload driver ped type %s for vehicle type %s, %d driver references left", pedMi->GetModelName(), CModelInfo::GetBaseModelInfo(carModelId)->GetModelName(), pedMi->m_isRequestedAsDriver);
			}
		}

		mi->SetHasRequestedDrivers(false);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	NetworkRegisterAllPedsAndDummies
// PURPOSE :	Registers all peds with the network object manager when a new
//				network game starts.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::NetworkRegisterAllPedsAndDummies()
{
	CPed::Pool* pedPool = CPed::GetPool();
	for(s32 i=0; i<pedPool->GetSize(); ++i)
	{
		CPed* pPed = pedPool->GetSlot(i);
		if(	!pPed || pPed->IsPlayer() || pPed->GetNetworkObject())
		{
			continue;
		}

		if (pPed->m_nPhysicalFlags.bNotToBeNetworked)
		{
			continue;
		}

		// script peds that are flagged not to be networked are not registered
		CScriptEntityExtension* pExtension = pPed->GetExtension<CScriptEntityExtension>();

		if (pExtension && !pExtension->GetIsNetworked())
		{
			continue;
		}

		const bool registeredPed = NetworkInterface::RegisterObject(pPed);
		if(!registeredPed)
		{
#if __DEV
			NetworkInterface::GetObjectManagerLog().Log("Failed to register a ped with the network at the start of a network session! Call DestroyPed. Reason is %s\r\n", NetworkInterface::GetLastRegistrationFailureReason());
			Warningf("Failed to register a ped with the network at the start of a network session! Call DestroyPed. Reason is %s",NetworkInterface::GetLastRegistrationFailureReason());
			CPedFactory::GetFactory()->DestroyPed(pPed);
#endif // __DEV
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	StopSpawningPeds
// PURPOSE :	Makes all spawning of new ambient peds and attractor peds stop
//				until StartSpawningPeds is called
// PARAMETERS :	None.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::StopSpawningPeds()
{
	ms_noPedSpawn = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	StartSpawningPeds
// PURPOSE :	Restarts the spawning of new ambient peds and attractor peds.
// PARAMETERS :	None.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::StartSpawningPeds()
{
	ms_noPedSpawn = false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowedToCreateMoreLocalScenarioPeds
// PURPOSE :	Check if the network population system allows creation of more local scenario peds.
// PARAMETERS :	None.
// RETURNS :	False if we've filled up the local ped budget in a MP game.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::AllowedToCreateMoreLocalScenarioPeds(bool *bAllowedToCreateHighPriorityOnly)
{
	if(NetworkInterface::IsGameInProgress())
	{
		const u32 numPeds = 1;
		bool areMissionObjects = false;
		if(!NetworkInterface::CanRegisterLocalAmbientPeds(numPeds) || !CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, numPeds, areMissionObjects))
		{
			// the network code will limit the local population based on factors such as the number of
			// cars using pretend occupants. We don't want this code to stop high priority scenario peds from spawning
			CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);

			if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, numPeds, areMissionObjects))
			{
                CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);
				return false;
            }

			CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);

			if(bAllowedToCreateHighPriorityOnly)
			{
                *bAllowedToCreateHighPriorityOnly = true;
			}
		}
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IncrementNetworkPedsSpawnedThisFrame
// PURPOSE :	Notify the ped population system that network has spawned a ped this frame
// PARAMETERS :	None.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::IncrementNetworkPedsSpawnedThisFrame()
{
	// note that this could starve the ped pop system if it gets called a lot. Maybe put some asserts in here?
	PF_INCREMENT(NetworkPedsCreated);
	ms_numNetworkPedsCreatedThisFrame++;
	Assert(ms_numNetworkPedsCreatedThisFrame < 255);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemoveAll...
// PURPOSE :	Various ped removal functions as passthrough to RemoveAllPedsWithException
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////

void CPedPopulation::RemoveAllPeds( ePedRemovalMode mode )
{
	RemoveAllPedsWithException(NULL, mode);
}

void CPedPopulation::RemoveAllRandomPeds()
{
	RemoveAllPedsWithException(NULL, PMR_CanBeDeleted);
}

void CPedPopulation::RemoveAllRandomPedsWithException(CPed *pException)
{
	RemoveAllPedsWithException(pException, PMR_CanBeDeleted);
}

void CPedPopulation::RemoveAllPedsHardNotMission()
{
	RemoveAllPedsWithException(NULL, PMR_ForceAllRandom);
}

void CPedPopulation::RemoveAllPedsHardNotPlayer()
{
	RemoveAllPedsWithException(NULL, PMR_ForceAllRandomAndMission);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemoveAllPedsWithException
// PURPOSE :	Removes all the peds and dummy peds we can (based on the rules
//				determined by removal mode). Used for entering interiors, cleanup etc.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::RemoveAllPedsWithException(CPed * pException, ePedRemovalMode mode)
{
	// Remove all the peds we can.
	{
		CPed::Pool*	pool	= CPed::GetPool();
		s32		i			= pool->GetSize();
		while(i--)
		{
			CPed* pPed = pool->GetSlot(i);
			// Easy cheap checks firsts
			if ( pPed && !pPed->IsPlayer() && pPed != pException )
			{
				bool bDelete;
				switch (mode)
				{
				case PMR_CanBeDeleted:
					bDelete = pPed->CanBeDeleted();
					break;
				case PMR_ForceAllRandom:
					bDelete = !(pPed->PopTypeIsMission() || pPed->IsPlayer());
					break;
				case PMR_ForceAllRandomAndMission:
					bDelete = !pPed->IsPlayer();
					break;
				default:
					Assert(0);
					bDelete = false;
					break;
				}
				if (bDelete)
				{
					CPedFactory::GetFactory()->DestroyPed(pPed);
				}
			}
		}
	}

	// Clear out the ped reuse pool as well.
	FlushPedReusePool();
}

#if __BANK
void RemoveAllRandomPedsExceptFocus()
{
	CPedPopulation::RemoveAllRandomPedsWithException( CPedDebugVisualiserMenu::GetFocusPed() );
}
void RemoveAllPedsExceptFocus()
{
	CPedPopulation::RemoveAllPedsWithException( CPedDebugVisualiserMenu::GetFocusPed(), CPedPopulation::PMR_ForceAllRandomAndMission );
}
void RemoveOnlyFocusPed()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();
	if(pPed && pPed->CanBeDeleted(true))
	{
		CPedFactory::GetFactory()->DestroyPed(pPed);
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PointFallsWithinNonCreationRange
// PURPOSE :	Finds out whether the level designer has set up a zone to exclude from
//				the population stuff and if so whether the specified point is in
//				that area.
// PARAMETERS:	vec - the query location.
// RETURNS:		Whether or not it is in the non-creation area.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::PointFallsWithinNonCreationArea(const Vector3& vec)
{
	if(CPathServer::GetAmbientPedGen().IsInPedGenBlockedArea(vec))
	{
		return true;
	}

	if(!ms_nonCreationAreaActive)
	{
		return false;
	}

	if( vec.x < ms_nonCreationAreaMin.x ||
		vec.x > ms_nonCreationAreaMax.x ||
		vec.y < ms_nonCreationAreaMin.y ||
		vec.y > ms_nonCreationAreaMax.y ||
		vec.z < ms_nonCreationAreaMin.z ||
		vec.z > ms_nonCreationAreaMax.z)
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PointFallsWithinNonRemovalArea
// PURPOSE :	Finds out whether the level designer has set up a zone to exclude from
//				the population stuff and if so whether the specified point is in
//				that area.
// PARAMETERS:	vec - the query location.
// RETURNS:		Whether or not it is in the non-removal area.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::PointFallsWithinNonRemovalArea(const Vector3& vec)
{
	if(!ms_nonRemovalAreaActive)
	{
		return false;
	}

	if( vec.x < ms_nonRemovalAreaMin.x ||
		vec.x > ms_nonRemovalAreaMax.x ||
		vec.y < ms_nonRemovalAreaMin.y ||
		vec.y > ms_nonRemovalAreaMax.y ||
		vec.z < ms_nonRemovalAreaMin.z ||
		vec.z > ms_nonRemovalAreaMax.z)
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldUseStartupMode
// PURPOSE :	Check if we should be bypassing visibility checks and other
//				limits, because the game is starting up, faded out, etc.
// RETURNS :	True if startup mode should be used.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::ShouldUseStartupMode()
{
	return (!HasForcePopulationInitFinished() || (camInterface::IsFadedOut() && ms_uNumberOfFramesFaded > 1) || ms_useStartupModeDueToWarping || GetInstantFillPopulation());
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedsRemoveIfPoolTooFull
// PURPOSE :	To make sure to keep some free space in the ped pool. If the ped
//				pool fills up this function will remove some peds.
// PARAMETERS :	popCtrlCentre - The centre point about which to base our ranges
//				on.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
static const u32 PedPopulationPedPoolCleanup_preferredPedBufferSize	= 8;
static const u32 PedPopulationPedPoolCleanup_maxNumToRemovePerFrame = 30;
void CPedPopulation::PedsRemoveIfPoolTooFull(const Vector3& popCtrlCentre)
{
	// Only remove peds if there are less than PedPopulationPedPoolCleanup_preferredPedBufferSize number
	// of free spaces.
	CPed::Pool* pool = CPed::GetPool();
	if((unsigned int)pool->GetNoOfFreeSpaces() >= PedPopulationPedPoolCleanup_preferredPedBufferSize)
	{
		return;
	}

// DEBUG!! -AC, This gets the job done, but it is certainly not as efficient as it could be...
	// Determine how many we need to remove.
	u32 numToRemove = PedPopulationPedPoolCleanup_preferredPedBufferSize - pool->GetNoOfFreeSpaces();

	// Go over all the peds in the pool, making a
	// list (no longer than the number we need to remove) of
	// the ones that are farthest away.
	typedef std::pair<float,CPed*>	RemovalListItem;
	u32							pedRemovalListCount = 0;
	RemovalListItem					pedRemovalList[PedPopulationPedPoolCleanup_maxNumToRemovePerFrame];
	float							farthestDistSqrInList = 0.0f;
	s32 i = pool->GetSize();
	while(i--)
	{
		// Get the ped of interest.
		CPed* pPed = pool->GetSlot(i);
		if(!pPed)
		{
			continue;
		}

		// not allowed to remove network clones, or peds about to migrate
		if(pPed->IsNetworkClone() || (pPed->GetNetworkObject() && pPed->GetNetworkObject()->IsPendingOwnerChange()))
		{
				continue;
		}

		if(pPed->PopTypeIsMission())
		{
			continue;
		}

		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			continue;
		}

		if (pPed->IsLawEnforcementPed() || pPed->GetPedType() == PEDTYPE_FIRE || pPed->GetPedType() == PEDTYPE_MEDIC)
		{
			continue;
		}

		// Get the distance to the ped.
		const float distSqr = (popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag2();

		// Determine if more items are still needed.  We use this
		// to determine if we should simply add to the list, or if
		// we should start trying to replace items in the list.
		if(pedRemovalListCount < numToRemove)
		{
			// Simply make the list longer by adding this item.
			pedRemovalList[pedRemovalListCount].first = distSqr;
			pedRemovalList[pedRemovalListCount].second = pPed;
			++pedRemovalListCount;

			// Update our farthest distance if necessary.
			if(distSqr > farthestDistSqrInList)
			{
				farthestDistSqrInList = distSqr;
			}
		}
		else
		{
			// Check if it should be put into the list.
			if(distSqr > farthestDistSqrInList)
			{
				// To put in the remove list we need to
				// overwrite an old value already in the list.

				// Get the index of the nearest item and replace it
				// with the current one.
				s32 nearestIndex = 0;
				float nearestSoFar = 100000.0f;
				for(u32 j = 0; j < pedRemovalListCount; ++j)
				{
					if(pedRemovalList[j].first < nearestSoFar)
					{
						nearestIndex = j;
						nearestSoFar = pedRemovalList[j].first;
					}
				}

				// Replace the item at the index with our current one.
				pedRemovalList[nearestIndex].first = distSqr;
				pedRemovalList[nearestIndex].second = pPed;

				// Make sure our farthest distance is updated.
				farthestDistSqrInList = distSqr;
			}
		}
	}
	Assert(pedRemovalListCount<=numToRemove);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedsRemoveOrMarkForRemovalOnVisImportance
// PURPOSE :	Scans through all the non cop peds removing or marking for
//				removal those that are out of range.
//				Cops are handled differently since the game play wanted level
//				mechanism determine how many cops should be chasing after the
//				player.
// PARAMETERS :	popCtrlCentre - The centrepoint about which to base our ranges
//				on.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
static const u32 PedPopulationPedCulling_maxNumToRemovePerFrame = 20;
void CPedPopulation::PedsRemoveOrMarkForRemovalOnVisImportance(const Vector3& popCtrlCentre, float removalRangeScale, float removalRateScale, bool popCtrlIsInInterior)
{
	// Go over all the dummy peds in the pool, making a
	// list of the ones to cull.
	// Note: we possibly just mark their removal timers here instead
	// of adding them to the removal list.
	
	CPed::Pool*	pool	= CPed::GetPool();
	Assert(pool);
	s32		i;
	// make sure that pedsToProcess is not more than the total "actual" peds in the pool
	s32 pedsToProcess = Min((s32)ceil(pool->GetNoOfUsedSpaces() / (ms_pedRemovalTimeSliceFactor)),pool->GetNoOfUsedSpaces());
	i = ms_pedRemovalIndex;

	while(pedsToProcess > 0)
	{
		i--;
		if(i < 0)
		{
			// wrapped the pool
			i = pool->GetSize()-1;
		}		

		CPed* pPed = pool->GetSlot(i);
		if(!pPed)
		{
			continue;
		}
	
		// This is an actual ped, so count it towards our total, even if we are going to exit out for the cop case below
		--pedsToProcess;

		// Skip peds in clusters. The cluster handles their removal. [5/3/2013 mdawe]
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster))
		{
			continue;
		}

		float cullRangeInView = 0.f;
		bool bRemovePed = ShouldRemovePed(pPed, popCtrlCentre, removalRangeScale, popCtrlIsInInterior, removalRateScale, cullRangeInView);

		if(bRemovePed && CanPedBeReused(pPed))
		{
			AddPedToReusePool(pPed);			
		}
		else if(	bRemovePed &&
			(!ms_pedDeletionQueue.IsFull()))
		{
#if __BANK
			CPedFactory::LogDestroyedPed(pPed, "CPedPopulation::PedsRemoveOrMarkForRemovalOnVisImportance()", grcDebugDraw::GetScreenSpaceTextHeight(), Color_orange);
#endif
			aiAssert(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster));
			PedPendingDeletion& pedDeleteStruct = ms_pedDeletionQueue.Append();
			pedDeleteStruct.pPed = pPed;
			pedDeleteStruct.fCullDistanceSq = cullRangeInView * cullRangeInView;
#if __ASSERT
			pedDeleteStruct.uFrameQueued = fwTimer::GetFrameCount();
			pedDeleteStruct.fDistanceSqWhenQueued = (popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag2();
#endif
		}

		// check for going through the entire pool once already
		// this can happen when there are small numbers of peds
		// or when the number of used slots does not equal the number of legit peds
		if(i == ms_pedRemovalIndex)
		{			
			break;
		}
	}

	// store where we are in the array for the next frame
	ms_pedRemovalIndex = i;
}


bool CPedPopulation::ShouldRemovePed( CPed* pPed, const Vector3& popCtrlCentre, float removalRangeScale, bool popCtrlIsInInterior, float removalRateScale, float& fOutCullRangeInView )
{
	DEV_BREAK_IF_FOCUS( ms_focusPedBreakOnPopulationRemovalTest, pPed );

	// Make sure the ped is not a cop.
	if(ms_usePrefdNumMethodForCopRemoval)
	{
		const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
		Assert(pModelInfo);
		if(CPedType::IsLawEnforcementType(pModelInfo->GetDefaultPedType()))
		{
			return false;
		}
	}

    if(pPed->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
        return false;

	bool bRemovePed = false;
	// Determine if we should remove the ped or not.
	// Note: Instead of removing them this possibly sets a
	// delay timer on their removal and respects that timer.
	if(IsPedARemovalCandidate(pPed, popCtrlCentre, removalRangeScale, popCtrlIsInInterior, fOutCullRangeInView))
	{
		const float Dist = (popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag();
		const float fKeyholeOuterDist = (ms_addRangeBaseInViewMax + ms_extendDelayWithinOuterRadiusExtraDist) * removalRangeScale;
		const u32 iExtraRemovalTime = (ms_extendDelayWithinOuterRadius && Dist < fKeyholeOuterDist) ? ms_extendDelayWithinOuterRadiusTime : 0;

		// Make sure they weren't still good recently,  otherwise we will instead
		// want to still wait a little while before trying to remove them.
		if(pPed->DelayedRemovalTimeHasPassed(removalRateScale, iExtraRemovalTime))
		{
			bRemovePed = true;
		}
		else
		{
			popDebugf3("Tried to remove ped 0x%p at (%.2f, %.2f, %.2f) too early, delayed removal time has not passed yet. removal rate scale: %.2f", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf(), removalRateScale);
		}
	}
	else
	{
		// We should reset their removal delay timer to make sure that even if they go
		// out of range or out of view that they don't get removed for a little while.
		pPed->DelayedRemovalTimeReset();
	}
	return bRemovePed;
}

s32 CPedPopulation::ProcessPedDeletionQueue(bool deleteAll)
{
	// Try to remove all the culled peds.
	s32 removedPeds = 0;
	OUTPUT_ONLY(u32 listCount = ms_pedDeletionQueue.GetCount();)
	ASSERT_ONLY(bool bUsingFreeCam = camInterface::GetDebugDirector().IsFreeCamActive();) // check once to see if we're using the debug cam - disables the on-screen deletion assert

	while(!ms_pedDeletionQueue.IsEmpty() && (removedPeds < ms_numPedsToDeletePerFrame || deleteAll))
	{
		PedPendingDeletion& pedDeletionStruct = ms_pedDeletionQueue.Pop();
		if (!pedDeletionStruct.pPed)
			continue;

		if(pedDeletionStruct.pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
			continue; // don't delete peds in the reuse pool, that's handled in UpdatePedReusePool

		// Try and pass control of this dummy onto another machine if the dummy
		// is still within the cull range on that machine.
		bool passedControl = false;
        if(pedDeletionStruct.pPed->GetNetworkObject() && !NetworkUtils::IsNetworkCloneOrMigrating(pedDeletionStruct.pPed))
		{
			passedControl = !(((CNetObjPed*)pedDeletionStruct.pPed->GetNetworkObject())->TryToPassControlOutOfScope());
		}
		if(!passedControl && pedDeletionStruct.pPed->CanBeDeleted())
		{
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pedDeletionStruct.pPed->GetTransform().GetPosition());
			const float fCullDist = rage::Sqrtf(pedDeletionStruct.fCullDistanceSq);
			const bool bOnScreen = IsCandidateInViewFrustum(vPedPosition, 1.0f, fCullDist);

			if(!deleteAll && bOnScreen ASSERT_ONLY(&& !bUsingFreeCam)) // if we're deleting everything in the queue, we don't mind if it's onscreen	
			{			
				float dist2 = GetPopulationControlPointInfo().m_centre.Dist2(vPedPosition);
				
				// see if the ped is closer than the cull range we stored when we decided to delete it
				if(dist2 < pedDeletionStruct.fCullDistanceSq )
				{
#if __ASSERT
					popDebugf1("Trying to delete an on-screen ped (%s) at <%.0f, %.0f, %.0f> - distance %.2f - Queued %u frames ago with distance %.2f - Cull Distance %.2f - skipping it", 
						pedDeletionStruct.pPed->GetPedModelInfo()->GetModelName(), vPedPosition.GetX(), vPedPosition.GetY(), vPedPosition.GetZ(), rage::Sqrtf(dist2),
						fwTimer::GetFrameCount() - pedDeletionStruct.uFrameQueued, rage::Sqrtf(pedDeletionStruct.fDistanceSqWhenQueued), rage::Sqrtf(pedDeletionStruct.fCullDistanceSq));
#else
					popDebugf1("Trying to delete an on-screen ped inside its on-screen cull range, skipping it");
#endif
					// don't delete this ped
					continue;
				}				
			}

			popDebugf2("Removed ped 0x%p (%s)", pedDeletionStruct.pPed.Get(), pedDeletionStruct.pPed->GetModelName());
			PF_PUSH_TIMEBAR("Destroy Ped");
			PF_INCREMENT(PedsDeletedByQueue);
			CPedFactory::GetFactory()->DestroyPed(pedDeletionStruct.pPed.Get(), true);
			PF_POP_TIMEBAR();
			removedPeds++;
		}
		else
		{
			popDebugf2("Could not remove ped 0x%p, was in range for net ped or could not be deleted. Passed control over to client.", pedDeletionStruct.pPed.Get());
		}
	}

	OUTPUT_ONLY(if (removedPeds > 0))
		popDebugf3("%d non cop, out of range peds were removed, %d were passed on to a closer net player", removedPeds, listCount - removedPeds);
	return removedPeds;

}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FlushPedQueues
// PURPOSE :	Clear out the scheduled ped queues. Keeps us from spawning peds that aren't loaded after a flush
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::FlushPedQueues()
{
	// clear out our ped queues
	while(!ms_scheduledScenarioPeds.IsEmpty())
		DropScheduledScenarioPed();

	while(!ms_scheduledAmbientPeds.IsEmpty())
		DropScheduledAmbientPed();

	while(!ms_scheduledPedsInVehicles.IsEmpty())
	{
		if(ms_scheduledPedsInVehicles.Top().pVehicle)
		{
			ms_scheduledPedsInVehicles.Top().pVehicle->RemoveScheduledOccupant();
		}
		DropScheduledPedInVehicle();
	}

	ProcessPedDeletionQueue(true); // delete all of the peds in the deletion queue

	FlushPedReusePool(); // clear this out as well
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPedsPendingDeletion
// PURPOSE :	Return the number of peds in the deletion queue
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
u32 CPedPopulation::GetPedsPendingDeletion()
{
	return ms_pedDeletionQueue.GetCount();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdatePedReusePool
// PURPOSE :	Update the pool of peds pending reuse. Periodically removes those that are no longer wanted.
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::UpdatePedReusePool()
{
	PF_PUSH_TIMEBAR("Update Ped Reuse Pool");
	for(int i = ms_pedReusePool.GetCount() - 1; i >= 0; i--)
	{
		if(!ms_pedReusePool[i].pPed)
		{
			pedReuseDebugf3("Removing Ped from reuse pool because it has been deleted");
			ms_pedReusePool.Delete(i);
			continue;
		}
		
		if(ms_iNumPedsDestroyedThisCycle == 0 && (fwTimer::GetFrameCount() - ms_pedReusePool[i].uFrameCreated) >= ms_ReusedPedTimeout)
		{
			bool removePed = false;

			if(!ms_usePedReusePool || (NetworkInterface::IsGameInProgress() && !ms_bPedReuseInMP) REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
				removePed = true; // remove peds if we're in a network game, or we're not reusing peds

			if(gPopStreaming.GetDiscardedPeds().IsMemberInGroup(ms_pedReusePool[i].pPed->GetModelIndex())) // check to see if the ped model is discardable
			{
				pedReuseDebugf3("Ped (%s) %p has become discardable, removing", ms_pedReusePool[i].pPed->GetModelName(), ms_pedReusePool[i].pPed.Get());
				removePed = true;
			}
			
			if(!removePed)
			{
				if(!gPopStreaming.GetAppropriateLoadedPeds().IsMemberInGroup(ms_pedReusePool[i].pPed->GetModelIndex()))
				{
					// didn't find the model in a valid pop group, drop it as we don't want it anymore
					pedReuseDebugf3("Ped (%s) %p isn't in a valid pop group, removing", ms_pedReusePool[i].pPed->GetModelName(), ms_pedReusePool[i].pPed.Get());
					removePed = true;
				}
			}
			
			if(removePed)
			{
				CPedFactory::GetFactory()->DestroyPed(ms_pedReusePool[i].pPed.Get(), true);				

				ms_pedReusePool[i].pPed = NULL;
				ms_pedReusePool[i].bNeedsAnimDirectorReInit = false;
				ms_pedReusePool.Delete(i);
			}
			else
			{
				ms_pedReusePool[i].uFrameCreated = fwTimer::GetFrameCount();
			}
		}		
	}

	PF_INCREMENTBY(PedsInReusePool, ms_pedReusePool.GetCount());
	PF_POP_TIMEBAR();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FlushPedReusePool
// PURPOSE :	Clears out the ped reuse pool
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::FlushPedReusePool()
{
	for(int i = ms_pedReusePool.GetCount() - 1; i >= 0; i--)
	{
		if(ms_pedReusePool[i].pPed)
		{
			CPedFactory::GetFactory()->DestroyPed(ms_pedReusePool[i].pPed.Get(), true);
			ms_pedReusePool[i].pPed = NULL;
			ms_pedReusePool[i].bNeedsAnimDirectorReInit = false;
		}

		ms_pedReusePool.Delete(i);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateInVehAmbientExceptionsCount
// PURPOSE :	Periodically counts the number of ambient peds in vehicles exceptions
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::UpdateInVehAmbientExceptionsCount()
{
	// Only run this check periodically
	u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();
	if( currentTimeMS < ms_uNextScheduledInVehAmbExceptionsScanTimeMS )
	{
		return;
	}
	else
	{
		const u32 s_PedInVehAmbientExceptionsScanPeriodMS = 2000;
		ms_uNextScheduledInVehAmbExceptionsScanTimeMS = currentTimeMS + s_PedInVehAmbientExceptionsScanPeriodMS;
	}

	// Reset counters
	ms_nNumInVehAmbientDead = 0;
	ms_nNumInVehAmbientNoPretendModel = 0;

	// Traverse all ambient peds and count the exceptions
	CPed::Pool* pPedPool = CPed::GetPool();
	s32 numPedsTotal = pPedPool->GetSize();
	for(int i=0; i < numPedsTotal; i++)
	{
		CPed* pPed = pPedPool->GetSlot(i);
		if( pPed && pPed->PopTypeGet() == POPTYPE_RANDOM_AMBIENT )
		{
			if( ShouldPedBeCountedAsInVehDead(pPed) )
			{
				ms_nNumInVehAmbientDead++;
			}

			if( ShouldPedBeCountedAsInVehNoPretendModel(pPed) )
			{
				ms_nNumInVehAmbientNoPretendModel++;
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldPedBeCountedAsInVehDead
// PURPOSE :	Determine whether a ped should be counted as dead in vehicle
// PARAMETERS :	pPed- the ped to examine
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::ShouldPedBeCountedAsInVehDead(CPed* pPed)
{
	// Ped must be in a vehicle and dead
	// NOTE: GetIsDeadOrDying switches to false while a dead ped is jacked out of their seat
	if( pPed->GetIsInVehicle() && (pPed->GetIsDeadOrDying() || (pPed->GetPedResetFlag(CPED_RESET_FLAG_BeingJacked) && pPed->ShouldBeDead())) )
	{
		return true;
	}

	// Should not be counted by default
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldPedBeCountedAsInVehNoPretendModel
// PURPOSE :	Determine whether a ped should be counted as in a vehicle whose model disallows pretend occupants
// PARAMETERS :	pPed- the ped to examine
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::ShouldPedBeCountedAsInVehNoPretendModel(CPed* pPed)
{
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if( pVehicle && !pVehicle->GetVehicleModelInfo()->GetAllowPretendOccupants() )
	{
		return true;
	}

	// Should not be counted by default
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DropOnePedFromReusePool
// PURPOSE :	Deletes the oldest ped in the reuse pool
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::DropOnePedFromReusePool()
{
	if(ms_pedReusePool.IsEmpty())
		return;

	u32 oldestCreationTime = ms_pedReusePool[0].uFrameCreated;
	u32 oldestIndex = 0;

	for(int i = 0; i < ms_pedReusePool.GetCount(); i++)
	{
		if(ms_pedReusePool[i].uFrameCreated < oldestCreationTime)
		{
			oldestCreationTime = ms_pedReusePool[i].uFrameCreated;
			oldestIndex = i;
		}
	}

	pedReuseDisplayf("Kicking oldest ped %s (%p) out of the reuse pool", ms_pedReusePool[oldestIndex].pPed->GetModelName(), ms_pedReusePool[oldestIndex].pPed.Get());		
	CPedFactory::GetFactory()->DestroyPed(ms_pedReusePool[oldestIndex].pPed.Get(), true);		

	ms_pedReusePool[oldestIndex].pPed = NULL;
	ms_pedReusePool[oldestIndex].bNeedsAnimDirectorReInit = false;
	ms_pedReusePool.Delete(oldestIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ReInitAPedAnimDirector
// PURPOSE :	Re-Initialises a single ped's anim director.
// PARAMETERS : None
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::ReInitAPedAnimDirector()
{
	for(int i = 0; i < ms_pedReusePool.GetCount(); i++)
	{
		if(ms_pedReusePool[i].pPed && ms_pedReusePool[i].bNeedsAnimDirectorReInit)
		{
			// Reinit this ped
			if (ms_pedReusePool[i].pPed->GetAnimDirector())
			{
				ms_pedReusePool[i].pPed->GetAnimDirector()->Shutdown();
				ms_pedReusePool[i].pPed->GetAnimDirector()->Init(true, true);

				// Re-initialised.
				ms_pedReusePool[i].bNeedsAnimDirectorReInit = false;

				return;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AddPedToReusePool
// PURPOSE :	Adds a ped to the reuse pool. Sets variables on the ped so it will be freezed in the world until it is reused.
// PARAMETERS : Pointer to the ped to be added to the reuse pool
// RETURNS :	None
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::AddPedToReusePool(CPed* pPed)
{
	if (!pPed)
		return;
	// treat adding to reuse like destroying the entity
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), pPed );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) );

#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker addPedToReuse("Add Ped to Reuse Pool", 27);	
#endif

	Assert(!(pPed->GetBaseFlags() & fwEntity::REMOVE_FROM_WORLD));	

	if(ms_pedReusePool.IsFull())
	{
		// make room by kicking out the oldest ped
		DropOnePedFromReusePool();
	}
	
	pedReuseDebugf1("Adding ped (%p) %s at <%0.f, %0.f, %0.f> to reuse pool", pPed, pPed->GetModelName(), pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());

#if __BANK
	CPedFactory::LogDestroyedPed(pPed, "CPedPopulation::AddPedToReusePool()", grcDebugDraw::GetScreenSpaceTextHeight()*2, Color_violet);
#endif

	// detach ped
	if(pPed->GetIsAttached())
	{
		u16 detachFlags = 0;
		detachFlags |= DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK;
		pPed->DetachFromParent(detachFlags);
	}

	// inform network object manager about this ped being deleted
	if(pPed->GetNetworkObject())
	{
		pedReuseDebugf1("Unregistering Network Ped for Reuse");
		if (pPed->IsNetworkClone() && pPed->GetNetworkObject()->IsScriptObject())
		{
			if (!pPed->PopTypeIsMission())
				Assertf(0, "Warning: clone %s is being destroyed (it is not a mission ped anymore)!", pPed->GetNetworkObject()->GetLogName());
			else
				Assertf(0, "Warning: clone %s is being destroyed!", pPed->GetNetworkObject()->GetLogName());
		}

		Assert(!pPed->IsNetworkClone());
		NetworkInterface::UnregisterObject(pPed);
	}

	Assert(!pPed->GetNetworkObject());


	CMiniMap::RemoveBlipAttachedToEntity(pPed, BLIP_TYPE_CHAR);
	
	pPed->GetFrameCollisionHistory()->Reset();
	pPed->GetPedIntelligence()->FlushImmediately(true, true, true, false);
	pPed->GetPedIntelligence()->ClearScanners();
	pPed->GetPedIntelligence()->SetOrder(NULL);
	pPed->GetPedIntelligence()->SetPedMotivation(pPed);

	pPed->ClearDamage();
	pPed->ClearDecorations();	

	pPed->ClearPedBrainWhenDeletingPed();

	BANK_ONLY(pPed->ResetDebugObjectID());

	// remove the ped from a ped group he may be in
	CPedGroup* pPedGroup = pPed->GetPedsGroup();
	if (pPedGroup && pPedGroup->IsLocallyControlled())
	{
		pPedGroup->GetGroupMembership()->RemoveMember(pPed);
	}

	// remove script guid so script can't grab this ped pointer while it's in the cache
	fwScriptGuid::DeleteGuid(*pPed);

	pPed->GetIkManager().RemoveAllSolvers();

#if __BANK
	if(pPed->GetCurrentPhysicsInst())
		pedReuseDebugf1("[B*1972222] Ped being added to re-use pool. Is ragdoll? : %i Animated level index: %i | ragdoll level index: %i ",
		pPed->GetUsingRagdoll(), pPed->GetAnimatedInst() ? pPed->GetAnimatedInst()->GetLevelIndex() : -1, pPed->GetRagdollInst() ? pPed->GetRagdollInst()->GetLevelIndex() : -1);
#endif

	pPed->RemovePhysics();

#if __BANK
	if(pPed->GetCurrentPhysicsInst())
		pedReuseDebugf1("[B*1972222] Ped being added to re-use pool physics should be removed. Animated level index: %i | ragdoll level index: %i ", 
		pPed->GetAnimatedInst() ? pPed->GetAnimatedInst()->GetLevelIndex() : -1, pPed->GetRagdollInst() ? pPed->GetRagdollInst()->GetLevelIndex() : -1);
#endif

	CPhysics::GetLevel()->SetInstanceIncludeFlags(pPed->GetRagdollInst()->GetLevelIndex(), 0);
	Vec3V vPos = pPed->GetRagdollInst()->GetPosition();
	vPos.SetZf(DELETED_ENITITY_ZPOS);
	// Call WarpInstance to properly warp an instance. This performs some necessary cleanup.
	CPhysics::WarpInstance(pPed->GetRagdollInst(),vPos);
	
	Matrix34 matrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
	matrix.d = VEC3V_TO_VECTOR3(vPos);
	pPed->SetMatrix(matrix, false, false, true);
	
	RemovePedFromPopulationCount(pPed);
	CGameWorld::Remove(pPed);

	pPed->m_nDEflags.bFrozen = true;
	
	Assert(CGameWorld::IsEntityFrozen(pPed));
	
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool, true);
	Assert(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool));

	// This has to be after the 'reusepool' flag is set on the ped
	REPLAY_ONLY(CReplayMgr::OnDelete(pPed));

	if(pPed->GetSpatialArrayNode().IsInserted())
	{
		pPed->GetSpatialArray().Remove(pPed->GetSpatialArrayNode());
	}

	pPed->DelayedRemovalTimeReset();

	CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
	pModelInfo->SetNumTimesInReusePool(pModelInfo->GetNumTimesInReusePool() + 1);

	PedReuseEntry& reusePed = ms_pedReusePool.Append();
	reusePed.pPed = pPed;
	reusePed.uFrameCreated = fwTimer::GetFrameCount();
	reusePed.bNeedsAnimDirectorReInit = true;

	ms_bDoBackgroundPedReInitThisFrame = false;

	IncrementNumPedsDestroyedThisCycle();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CanPedBeReused
// PURPOSE :	Checks to see if a ped is valid for reuse
// PARAMETERS : Pointer to the ped to be checked
// RETURNS :	True if the ped can be reused
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::CanPedBeReused(CPed* pPed, bool bSkipSeenCheck/*=false*/)
{
	if(!ms_usePedReusePool)
		return false;

	if(NetworkInterface::IsGameInProgress() && !ms_bPedReuseInMP)
		return false; // disable population cache in network games (for now)

	if(fragManager::GetFragCacheAllocator()->GetMemoryUsed() >= FRAG_CACHE_POP_REUSE_MAX)
		return false; // no reuse - frag cache pool is full

	if(!(bSkipSeenCheck || !ms_bPedReuseChecksHasBeenSeen) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedHasBeenSeen))
	{
		return false;
	}

	if(pPed->GetBaseFlags() & fwEntity::REMOVE_FROM_WORLD)
		return false; // going to be removed

	if(pPed->GetIsDeadOrDying())
		return false; // no dead or dying peds

	ePedType pedType = pPed->GetPedModelInfo()->GetDefaultPedType();
	switch(pedType)
	{	
	case PEDTYPE_PLAYER_0:
	case PEDTYPE_PLAYER_1:
	case PEDTYPE_NETWORK_PLAYER:
	case PEDTYPE_PLAYER_2:
	case PEDTYPE_COP:
		return false; // no cops, players
	default:
		break;
	}

	if(!pPed->PopTypeIsRandom()) // only random peds
		return false;

	if(gPopStreaming.GetDiscardedPeds().IsMemberInGroup(pPed->GetModelIndex())) // or discarded ped models
		return false;

#define MAX_IDENTICAL_MODELS_IN_PED_POOL (3)
	int sameModels = 0;
	// only have two of each model in the pool
	for(int i = 0; i < ms_pedReusePool.GetCount(); i++)
	{
		if(ms_pedReusePool[i].pPed && ms_pedReusePool[i].pPed->GetModelId() == pPed->GetModelId())
		{
			sameModels++;
			if(sameModels >= MAX_IDENTICAL_MODELS_IN_PED_POOL)
			{
				pedReuseDebugf3("Rejecting Ped Because there are already %d of this model in the reuse pool", sameModels);
				return false; // already have this model
			}			
		}
	}

	if(!gPopStreaming.GetAppropriateLoadedPeds().IsMemberInGroup(pPed->GetModelIndex()))
		return false; // didn't find the model in a valid pop group, drop it as we don't want it anymore

	// check ped tasks
	if(pPed->GetPedIntelligence()->GetTaskActive() != pPed->GetPedIntelligence()->GetTaskDefault())
	{
		pedReuseDebugf3("Can't reuse ped because of active task %s != default task %s", pPed->GetPedIntelligence()->GetTaskActive()->GetName().c_str(), pPed->GetPedIntelligence()->GetTaskDefault()->GetName().c_str());
		return false;
	}

	if(pPed->GetPedIntelligence()->GetCurrentEvent())
	{
		pedReuseDebugf3("Can't reuse ped because of event %s", pPed->GetPedIntelligence()->GetCurrentEvent()->GetName().c_str());
		return false;
	}
	// Made it, reuse the ped
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindPedToReuse
// PURPOSE :	Finds a matching entry in the ped reuse pool, based on ModelID
// PARAMETERS : ModelId to match
// RETURNS :	Index of the entry in the reuse pool, or -1 if not found
/////////////////////////////////////////////////////////////////////////////////
int CPedPopulation::FindPedToReuse(u32 pedModel)
{
	fwModelId pedModelId((strLocalIndex(pedModel)));
	for(int i = 0; i < ms_pedReusePool.GetCount(); i++)
	{
		if(ms_pedReusePool[i].pPed && ms_pedReusePool[i].pPed->GetModelId() == pedModelId)
		{
			pedReuseDebugf2("Found ped model %s to reuse", ms_pedReusePool[i].pPed->GetModelName());
			return i;
		}
	}
	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPedFromReusePool
// PURPOSE :	Return the ped at the given index in the reuse pool
// PARAMETERS : Index of the ped we want (from FindPedToReuse)
// RETURNS :	The Ped we want, or NULL
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedPopulation::GetPedFromReusePool(int reusedPedIndex)
{
	if(reusedPedIndex < ms_pedReusePool.GetCount())
	{
		return ms_pedReusePool[reusedPedIndex].pPed;
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemovePedFromReusePool
// PURPOSE :	Remove the entry at the given index from the reuse pool
// PARAMETERS : Index of the entry we want to remove
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::RemovePedFromReusePool(int reusedPedIndex)
{
	if(reusedPedIndex < ms_pedReusePool.GetCount())
	{
		ms_pedReusePool[reusedPedIndex].pPed = NULL;
		ms_pedReusePool[reusedPedIndex].bNeedsAnimDirectorReInit = false;
		ms_pedReusePool.Delete(reusedPedIndex);
	}	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ReusePedFromPedReusePool
// PURPOSE :	Sets up a ped from the reuse pool to be ready for reuse by the calling function
// PARAMETERS : Ped pointer, creation matrix, CControlledByInfo
// RETURNS :	NONE
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::ReusePedFromPedReusePool(CPed* pPed, Matrix34& newMatrix, const CControlledByInfo& localControlInfo, bool bCreatedByScript, bool shouldBeCloned, int pedReuseIndex, bool bScenarioPedCreatedByConcealeadPlayer)
{
#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
	pfAutoMarker reusePed("Reuse Ped From Reuse Pool", 28);
#endif
	pedReuseDebugf1("Reusing Ped %s (%p) at <%.0f, %.0f, %.0f>", pPed->GetModelName(), pPed, newMatrix.d.x, newMatrix.d.y, newMatrix.d.z);

	if(shouldBeCloned && NetworkInterface::IsGameInProgress() && !NetworkInterface::CanRegisterObject(pPed->GetNetworkObjectType(), bCreatedByScript))
	{
		popDebugf3("Can't reuse ped - Network can't register type");
		return false;
	}

	pPed->m_nDEflags.bFrozen = false;

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool, false);
	Assert(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool));

	// Animation
	if (ms_pedReusePool[pedReuseIndex].bNeedsAnimDirectorReInit && pPed->GetAnimDirector())
	{
		pPed->GetAnimDirector()->Shutdown();
		pPed->GetAnimDirector()->Init(true, true);

		// Re-initialised.
		ms_pedReusePool[pedReuseIndex].bNeedsAnimDirectorReInit = false;
	}

	// We've re-used a ped, so don't do a background anim director re-init this frame.
	ms_bDoBackgroundPedReInitThisFrame = false;

	CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
	pModelInfo->SetNumTimesInReusePool(pModelInfo->GetNumTimesInReusePool() - 1);

	// use friend class CPedFactory to continue re-initing the ped
	CPedFactory::GetFactory()->InitCachedPed(pPed);

	// set default values, as if ped was newly created by the factory	
	pPed->SetControlledByInfo(localControlInfo);
	pPed->SetOwnedBy(ENTITY_OWNEDBY_POPULATION);
	pPed->DelayedRemovalTimeReset();
	pPed->DelayedConversionTimeReset();
	pPed->SetArrestState(ArrestState_None);
	pPed->SetDeathState(DeathState_Alive);
	pPed->InitHealth();
	pPed->GetPedAiLod().ResetAllLodFlags();

	pPed->GetPedIntelligence()->FlushImmediately(true, true, true, false);

	pPed->SetBehaviorFromTaskData();

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnPedCollisionWhenDead, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnVehicleCollisionWhenDead, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated, true);

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedWasReused, true);	
	
	pPed->SetMatrix(newMatrix);
	pPed->InitPhys();
	pPed->SwitchToAnimated(false, false, false);

	pPed->m_nPhysicalFlags.bNotToBeNetworked = !shouldBeCloned;

	if (bScenarioPedCreatedByConcealeadPlayer)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByConcealedPlayer, true);
	}

	// network checking here
	if(shouldBeCloned && NetworkInterface::IsGameInProgress())
	{
		pedReuseDebugf1("Registering Reused Ped for Networking Ped for Reuse");
		bool bRegistered = NetworkInterface::RegisterObject(pPed, 0, bCreatedByScript ? CNetObjGame::GLOBALFLAG_SCRIPTOBJECT : 0);

		// ped could not be registered as a network object (due to maximum number of ped objects being exceeded
		// scripted peds are queued to be created
		if (!bRegistered && !bCreatedByScript)
		{
			// find the ped in the reuse pool
			int foundIndex = -1;
			for(int i = 0; i < ms_pedReusePool.GetCount(); i++)
			{
				if(ms_pedReusePool[i].pPed == pPed)
				{
					foundIndex = i;
					break; // found it
				}
			}
			Assert(foundIndex != -1);
			// destroy the ped, as it's in a weird state now
			CPedFactory::GetFactory()->DestroyPed(ms_pedReusePool[foundIndex].pPed.Get(), true);				

			ms_pedReusePool[foundIndex].pPed = NULL;
			ms_pedReusePool[foundIndex].bNeedsAnimDirectorReInit = false;
			ms_pedReusePool.Delete(foundIndex);
			
			return false;
		}
	}

#if __TRACK_PEDS_IN_NAVMESH
	pPed->GetNavMeshTracker().Teleport(newMatrix.d);
#endif

	// make sure visibility is set correctly
	pPed->SetIsVisibleForAllModules();

	// pretend the factory created this ped
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByFactory, true);

	// set lod visibility state like the ped was just created
	pPed->GetLodData().SetResetDisabled(true);
	pPed->SetAlpha(LODTYPES_ALPHA_VISIBLE);
	
	CPedPropsMgr::ClearAllPedProps(pPed);
	
	IncrementNumPedsCreatedThisCycle();

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(pPed));

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CanPedBeRemoved
// PURPOSE :	To determine if this ped should be culled.
// PARAMETERS :	pPed- the dummy ped we are interested in.
//				popCtrlCentre - The centre point about which to base our ranges
//				on.
// RETURNS :	Whether or not we should attempt to remove or mark for removal
//				the ped.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::CanPedBeRemoved(CPed* pPed)
{
	Assert(pPed);

	// Don't try to remove the ped if it is a player, or it can't be deleted, or
	// it's in a vehicle, or it's attached to a vehicle.
	if(	pPed->IsPlayer() ||
		!pPed->CanBeDeleted() ||
		pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) ||
		(0!=pPed->GetAttachParent() && pPed->GetAttachParent()->GetType()==ENTITY_TYPE_VEHICLE))
	{
		return false;
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
		return false;

	// Dont delete this ped if its under the population control of the patrol system
	if( pPed->PopTypeGet() == POPTYPE_RANDOM_PATROL )
	{
		return false;
	}

	// If a ped has a valid intelligence
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	if( pPedIntelligence )
	{
		// Check if the ped has any Order and is not responding to a threat
		if( pPedIntelligence->GetOrder() )
		{
			if(pPedIntelligence->GetOrder()->GetIsValid())
			{
				if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontRemoveWithValidOrder))
				{
					return false;
				}
			}

			if( !pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE) )
			{
				// If the ped's former vehicle is still around
				// NOTE: we check if this ped is in a vehicle above
				if( pPed->GetMyVehicle() )
				{
					// Don't remove the ped
					return false;
				}
			}
		}
	}

	// If the ped has a vehicle that's still around and came from a cluster, don't delete it
	if ( pPed->GetMyVehicle() && pPed->GetMyVehicle()->m_nVehicleFlags.bIsInCluster )
	{
		// If the ped is very far from the vehicle, allow it to be removed.
		ScalarV pedVehicleDist2 = DistSquared(pPed->GetTransform().GetPosition(), pPed->GetMyVehicle()->GetTransform().GetPosition());
		if (IsLessThanAll(pedVehicleDist2, CPedPopulation::ms_clusteredPedVehicleRemovalRangeSq))
		{
			return false;
		}
	}

	// Don't try to remove peds controlled remotely by another machine
	if(pPed->IsNetworkClone())
	{
		return false;
	}

	// If the ped is in a group, only the leader can be removed
	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	if( pPedsGroup )
	{
		if (pPedsGroup->PopTypeGet() == POPTYPE_RANDOM_PATROL)
		{
			return false;
		}

		if (!pPedsGroup->GetGroupMembership()->IsLeader(pPed) )
		{
			return false;
		}
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsPedARemovalCandidate
// PURPOSE :	To determine if this ped should be culled.
// PARAMETERS :	pPed- the dummy ped we are interested in.
//				popCtrlCentre - The centre point about which to base our ranges
//				on.
// RETURNS :	Whether or not we should attempt to remove or mark for removal
//				the ped.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::IsPedARemovalCandidate(CPed* pPed, const Vector3& popCtrlCentre, float removalRangeScale, bool popCtrlIsInInterior, float& cullRangeInView_Out)
{
	Assert(pPed);
	if (!CanPedBeRemoved(pPed))
	{
		return false;
	}

	return CanRemovePedAtPedsPosition(pPed, removalRangeScale, popCtrlCentre, popCtrlIsInInterior, cullRangeInView_Out);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsLawPedARemovalCandidate
// PURPOSE :	To determine if this law ped should be culled.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::IsLawPedARemovalCandidate(CPed* pPed, const Vector3& popCtrlCentre)
{
	if(pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return false;
	}

	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBeenInArmedCombat) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch))
	{
		return false;
	}

	Vector3 vDistToPopCentre = popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(vDistToPopCentre.XYMag2() < CPedPopulation::ms_minPatrollingLawRemovalDistanceSq)
	{
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedsRemoveIfRemovalIsPriority
// PURPOSE :	If the streaming is trying to get rid of this model we remove any
//				peds that we possibly can.
// PARAMETERS :	popCtrlCentre - The centre point about which to base our ranges
//				on.
//				popCtrlIsInInterior - is the centerpoint in an interior or not.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
#define DPRIRIP_BATCH (16)
void CPedPopulation::PedsRemoveIfRemovalIsPriority(const Vector3& popCtrlCentre, float UNUSED_PARAM(removalRangeScale))
{
	CPed::Pool* pool = CPed::GetPool();
	s32 s	= pool->GetSize();
	s32 batch = fwTimer::GetSystemFrameCount() % DPRIRIP_BATCH;
	s32 start = (s * batch) / DPRIRIP_BATCH;
	s32 end = (s * (batch+1)) / DPRIRIP_BATCH;

	OUTPUT_ONLY(u32 removedPeds = 0;)
	// 8/7/12 - cthomas - We know only the main thread would possibly be updating physics at this 
	// point in time, so set the flag in the physics system to indicate that. This is an optimization 
	// that will allow us to skip some locking overhead in the physics system as we remove peds.
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
	for (s32 i = start; i < end; ++i)
	{
		CPed* pPed = pool->GetSlot(i);
		if (!pPed || !pPed->RemovalIsPriority(pPed->GetModelId()))
		{
			continue;
		}
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster))
		{
			continue;
		}

		if (pPed->GetIsOnScreen(true))
		{
			pPed->TouchInFovTime(5000);
			continue;
		}

		if (!CanPedBeRemoved(pPed))
		{
			continue;
		}

		// Check the distance against our cull ranges.
		const Vector3 diff	(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - popCtrlCentre);
		const float	distSqr = diff.Mag2();
		float removalDistToUseSqr = rage::square(PRIORITY_REMOVAL_DIST);
		if (pPed->PopTypeGet() == POPTYPE_RANDOM_SCENARIO && !pPed->GetRemoveAsSoonAsPossible() && !pPed->m_nDEflags.bFrozenByInterior)
		{
			//Scenarios have to be much further or the models get removed and the scenario re-spawns them straight away.
			removalDistToUseSqr = rage::square(PRIORITY_REMOVAL_DIST_SCENARIO);
		}
		else if((pPed->GetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway ) ||
				pPed->GetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway )) &&
				CPedType::IsLawEnforcementType(pPed->GetPedType()))
		{
			removalDistToUseSqr = (rage::square(PRIORITY_REMOVAL_DIST_LAW)
				* ms_cullRangeScaleCops * ms_cullRangeScaleCops);
		}
		else if(pPed->GetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway ) ||
				pPed->GetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway ))
		{
			removalDistToUseSqr = (rage::square(PRIORITY_REMOVAL_DIST_EXTRA_FAR_AWAY)
				* ms_cullRangeScaleSpecialPeds * ms_cullRangeScaleSpecialPeds);
		}
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_RemoveDeadExtraFarAway ))
		{
			removalDistToUseSqr = Max(removalDistToUseSqr,  rage::square(PRIORITY_REMOVAL_DIST_EXTRA_FAR_AWAY));
		}
		if (distSqr <= removalDistToUseSqr)
		{
			continue;
		}
		// Todo: We might want to add a higher logic that consider how many peds are dead and so on - Traffe
		if (!pPed->HasInFovTimePassed() && (distSqr < rage::square(PRIORITY_REMOVAL_DIST_INFOV)))
		{
			continue;
		}
		
		if(!ms_pedDeletionQueue.IsFull())
		{
#if __BANK
			CPedFactory::LogDestroyedPed(pPed, "CPedPopulation::PedsRemoveIfRemovalIsPriority()", grcDebugDraw::GetScreenSpaceTextHeight(), Color_orange);
#endif
			aiAssert(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster));
			PedPendingDeletion& pedDeleteStruct = ms_pedDeletionQueue.Append();
			pedDeleteStruct.pPed = pPed;
			pedDeleteStruct.fCullDistanceSq = removalDistToUseSqr;
#if __ASSERT
			pedDeleteStruct.uFrameQueued = fwTimer::GetFrameCount();
			pedDeleteStruct.fDistanceSqWhenQueued = distSqr;
#endif
			continue;
		}


		if (pPed->GetNetworkObject() && !NetworkUtils::IsNetworkCloneOrMigrating(pPed) && !(((CNetObjPed*)pPed->GetNetworkObject())->TryToPassControlOutOfScope()))
		{
			continue;
		}

		// If the ped that we are removing is counted as an ambient, but came from a scenario, set the timer to the
		// next time when we can allow a ped to removed for this reason. This is done so we don't lose a whole crowd
		// of people after looking away for a few seconds.
		if(pPed->PopTypeGet() == POPTYPE_RANDOM_AMBIENT && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SpawnedAtScenario))
		{
			ms_nextAllowedPriRemovalOfAmbientPedFromScenario = Max(ms_nextAllowedPriRemovalOfAmbientPedFromScenario,
					fwTimer::GetTimeInMilliseconds() + ms_minTimeBetweenPriRemovalOfAmbientPedFromScenario);
		}

		popDebugf2("Removed ped 0x%p (%s), streaming removal priority. distance: %.2f, cull range: %.2f", pPed, pPed->GetModelName(), diff.Mag(), Sqrtf(removalDistToUseSqr));
		CPedFactory::GetFactory()->DestroyPed(pPed);
		OUTPUT_ONLY(removedPeds++;)
	}
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);

	OUTPUT_ONLY(if (removedPeds > 0))
		popDebugf3("%d non cop peds were removed due to streaming system not liking them", removedPeds);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CopsRemoveOrMarkForRemovalOnPrefdNum
// PURPOSE :	Scans through all the cops removing or marking for
//				removal that are greater than the pref'd num.
//				Cops are handled differently since the game play wanted level
//				mechanism determine how many cops should be chasing after the
//				player.
// PARAMETERS :	popCtrlCentre - The centre point about which to base our ranges
//				on.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
struct CopDistanceListItem
{
	float				m_distance;
	PedOrDumyPedHolder	m_pedOrDumyPedHolder;
	bool				m_preserve;
};
static const u32 PedPopulationRemoval_maxAllCopsListSize = 100;
class CopDistanceListSort
{
public:
	bool operator()(const CopDistanceListItem left, const CopDistanceListItem right)
	{
		return (left.m_distance < right.m_distance);
	}
};
void CPedPopulation::CopsRemoveOrMarkForRemovalOnPrefdNum(const Vector3& popCtrlCentre, u32 numPrefdCopsAroundPlayer, float removalRangeScale, float removalRateScale, bool popCtrlIsInInterior)
{
	PF_AUTO_PUSH_DETAIL("Remove Or Mark Cops");

	// Make a list of all the cop peds or cop dummy peds and some useful info about them.
	PF_START_TIMEBAR_DETAIL("Make all cop peds list");
	u32				allCopPedsAndCopDummiesListCount	= 0;
	CopDistanceListItem	allCopPedsAndCopDummiesList[PedPopulationRemoval_maxAllCopsListSize];
	{
		// Add peds to the list.
		CPed::Pool* pedPool = CPed::GetPool();
		s32 i = pedPool->GetSize();
		while(i--)
		{
			if(allCopPedsAndCopDummiesListCount >= PedPopulationRemoval_maxAllCopsListSize)
			{
				break;
			}

			// Get the ped of interest.
			CPed* pPed = pedPool->GetSlot(i);
			if(!pPed || pPed->IsPlayer())
			{
				continue;
			}
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster))
			{
				continue;
			}

			// Make sure the ped is a cop.
			const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
			Assert(pModelInfo);
			if(!CPedType::IsLawEnforcementType(pModelInfo->GetDefaultPedType()))
			{
				continue;
			}

			// Fill in the the ped or dummy ped holder.
			PedOrDumyPedHolder pedHolder;
			pedHolder.m_pPed = pPed;

			// Get the XY delta to this cop.
			Vector3 deltaXY = popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			deltaXY.z = 0.0f;

			// Prioritize removing offscreen corpses
			if (pPed->GetIsVisibleInSomeViewportThisFrame())
				deltaXY.Scale(0.01f);

			// Add the info to the list.
			allCopPedsAndCopDummiesList[allCopPedsAndCopDummiesListCount].m_distance			= deltaXY.Mag2();
			allCopPedsAndCopDummiesList[allCopPedsAndCopDummiesListCount].m_pedOrDumyPedHolder	= pedHolder;
			allCopPedsAndCopDummiesList[allCopPedsAndCopDummiesListCount].m_preserve			= false;
			++allCopPedsAndCopDummiesListCount;
		}
	}

	// Create a list of cops to preserve.
	PF_START_TIMEBAR_DETAIL("Determine which cops to preserve.");

	// Sort the list according to distance.
	std::sort(allCopPedsAndCopDummiesList, allCopPedsAndCopDummiesList + allCopPedsAndCopDummiesListCount, CopDistanceListSort());
	const u32 preserveCountToUse = (numPrefdCopsAroundPlayer < allCopPedsAndCopDummiesListCount)?numPrefdCopsAroundPlayer:allCopPedsAndCopDummiesListCount;

	// Normally the preserved ones would just be the first preserveCountToUse ones in the list, but
	// we need to make sure that when cops are getting injured and killed and they player has a wanted
	// level that more cops keep coming.  The code immediately below is to handle this case.
	CPed * pPlayerPed = FindPlayerPed();
	const bool playerHasWantedLevel = (pPlayerPed ? pPlayerPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN : false);
	u32 preservedCount = 0;
	for(u32 t = 0; t < allCopPedsAndCopDummiesListCount; ++t)
	{
		if( preservedCount >= preserveCountToUse)
		{
			break;
		}

		if(playerHasWantedLevel)
		{
			// Make sure this cop is not a injured or dead real ped.
			CPed* pPed = allCopPedsAndCopDummiesList[t].m_pedOrDumyPedHolder.m_pPed;
			if(pPed && !pPed->IsInjured() && !pPed->GetIsDeadOrDying())
			{
				allCopPedsAndCopDummiesList[t].m_preserve = true;
				++preservedCount;
			}
		}
		else
		{
			allCopPedsAndCopDummiesList[t].m_preserve = true;
			++preservedCount;
		}
	}

	PF_START_TIMEBAR_DETAIL("Remove Cops");
	OUTPUT_ONLY(u32 removedPeds = 0;)
	OUTPUT_ONLY(u32 passedPeds = 0;)
	for(u32 t = 0; t < allCopPedsAndCopDummiesListCount; ++t)
	{
		if(allCopPedsAndCopDummiesList[t].m_preserve)
		{
			continue;
		}

		// Determine if we should remove the dummy ped or not.
		// Note: Instead of removing them this possibly sets a
		// delay timer on their removal and respects that timer.
		if(allCopPedsAndCopDummiesList[t].m_pedOrDumyPedHolder.m_pPed)
		{
			CPed* pPed = allCopPedsAndCopDummiesList[t].m_pedOrDumyPedHolder.m_pPed;

			bool bTryToRemove = IsLawPedARemovalCandidate(pPed, popCtrlCentre);
			bool bCanTryToRemove = (pPed->GetPedResetFlag(CPED_RESET_FLAG_Wandering) ||
				pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true) ||
				pPed->GetRemoveAsSoonAsPossible());
			bool bCanResetRemovalTime = !bTryToRemove || !pPed->GetPedResetFlag(CPED_RESET_FLAG_PatrollingInVehicle);

			bool bRemovePed = false;
			float cullRangeInView = 0.f;
			if( (bTryToRemove && bCanTryToRemove) ||
				IsPedARemovalCandidate(pPed, popCtrlCentre, removalRangeScale, popCtrlIsInInterior, cullRangeInView) )
			{
				// Make sure they weren't still good recently,  otherwise we will instead
				// want to still wait a little while before trying to remove them.
				if(pPed->DelayedRemovalTimeHasPassed(removalRateScale))
				{
					bRemovePed = true;
				}
				else
				{
					popDebugf3("Tried to remove cop ped 0x%p at (%.2f, %.2f, %.2f) too early, delayed removal time has not passed yet. removal rate scale: %.2f", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf(), removalRateScale);
				}
			}
			else if(bCanResetRemovalTime)
			{
				// We should reset their removal delay timer to make sure that even if they go
				// out of range or out of view that they don't get removed for a little while.
				pPed->DelayedRemovalTimeReset();
			}

			if(bRemovePed)
			{
				if(!ms_pedDeletionQueue.IsFull())
				{
#if __BANK
					CPedFactory::LogDestroyedPed(pPed, "CPedPopulation::CopsRemoveOrMarkForRemovalOnPrefdNum()", grcDebugDraw::GetScreenSpaceTextHeight(), Color_orange);
#endif
					aiAssert(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster));
					PedPendingDeletion& pedDeleteStruct = ms_pedDeletionQueue.Append();
					pedDeleteStruct.pPed = pPed;
					pedDeleteStruct.fCullDistanceSq = cullRangeInView * cullRangeInView;
#if __ASSERT
					pedDeleteStruct.uFrameQueued = fwTimer::GetFrameCount();
					pedDeleteStruct.fDistanceSqWhenQueued = (popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag2();
#endif
					continue;
				}
				// Try and pass control of this dummy onto another machine if the dummy
				// is still within the cull range on that machine.
				bool passedControl = false;
				if(pPed->GetNetworkObject() && !NetworkUtils::IsNetworkCloneOrMigrating(pPed))
				{
					Assert(dynamic_cast<CNetObjPed*>(pPed->GetNetworkObject()));
					passedControl = !(((CNetObjPed*)pPed->GetNetworkObject())->TryToPassControlOutOfScope());
				}
				if(!passedControl)
				{
					popDebugf2("Removed cop ped 0x%p at (%.2f, %.2f, %.2f)", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());
					Assert(pPed->DelayedRemovalTimeHasPassed(removalRateScale));
					CPedFactory::GetFactory()->DestroyPed(pPed, true);
					OUTPUT_ONLY(removedPeds++;)
				}
				else
				{
					popDebugf2("Could not remove cop ped 0x%p at (%.2f, %.2f, %.2f), was in range for net ped. Passed control over to client.", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());
					OUTPUT_ONLY(passedPeds++;)
				}
			}
		}
	}

	OUTPUT_ONLY(if (removedPeds > 0))
		popDebugf3("%d cop peds were removed, %d were passed on to closer net players", removedPeds, passedPeds);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RemovePedsOnRespawn_MP
// PURPOSE :	Scans through all the peds and deletes appropriate peds
//				on a MP respawn (e.g. fleeing and dead peds).
// PARAMETERS :	pRespawningPed - The ped that is respawning.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::RemovePedsOnRespawn_MP(CPed *pRespawningPed)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	// Add peds to the list.
	CPed::Pool* pedPool = CPed::GetPool();
	s32 i = pedPool->GetSize();
	while(i--)
	{
		CPed* pPed = pedPool->GetSlot(i);
		if(!pPed || pPed->IsPlayer())
		{
			continue;
		}

		if(pPed->IsNetworkClone())
		{
			continue;
		}

		const CTaskSmartFlee *pFleeTask = static_cast<CTaskSmartFlee*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
		if(pPed->IsDead() || (pFleeTask && pFleeTask->GetFleeTarget().GetEntity() == pRespawningPed))
		{
			if(pPed->CanBeDeleted())
			{
				float fPedDistance = CVehiclePopulation::GetDistanceFromAllPlayers(CGameWorld::FindLocalPlayerCoors(), pPed, pRespawningPed);
				bool bPedRemoteVisible = CVehiclePopulation::IsEntityVisibleToAnyPlayer(pPed, pRespawningPed);
				float fPedScopeDistance = pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetScopeData().m_scopeDistance : 300.0f;
				if(!bPedRemoteVisible && (fPedDistance > fPedScopeDistance))
				{
					if(CanPedBeReused(pPed))
					{
						AddPedToReusePool(pPed);			
					}
					else
					{
						CPedFactory::GetFactory()->DestroyPed(pPed);
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DeadPedsAndDummyPedsRemoveExcessCounts
// PURPOSE :	Remove dead peds when there are still so many of them (after
//				other removals above) that it is starting to affect the rest
//				of the population.
// PARAMETERS :	popCtrlCentre - the centre about which to base our population.
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
struct DeadPedDistanceListItem
{
	float				m_distance;
	float				m_timeSinceUnderAnotherPed;
	u32					m_timeDeadFor;
	PedOrDumyPedHolder	m_deadPedOrDeadDumyPedHolder;
};
static const u32 PedPopulationRemoval_maxDeadPedsListSize = 100;
class DeadsDistanceListSort
{
public:
	bool operator()(const DeadPedDistanceListItem left, const DeadPedDistanceListItem right)
	{
		return (left.m_distance < right.m_distance);
	}
};

void CPedPopulation::DeadPedsRemoveExcessCounts(const Vector3& popCtrlCentre, u32 numMaxDeadAroundPlayer, u32 numMaxDeadGlobally)
{
	PF_AUTO_PUSH_DETAIL("Remove Or Mark Dead Peds");

	// Make a list of all the dead peds or dead dummy peds and some useful info about them.
	PF_START_TIMEBAR_DETAIL("Make all dead peds list");
	u32 allDeadPedsAndDeadDummiesListCount	= 0;
	u32 allDeadClonesToConsider = 0;
	DeadPedDistanceListItem	allDeadPedsAndDeadDummiesList[PedPopulationRemoval_maxDeadPedsListSize];
	{
		// Add peds to the list.
		CPed::Pool* pedPool = CPed::GetPool();
		s32 i = pedPool->GetSize();
		while(i--)
		{
			if(allDeadPedsAndDeadDummiesListCount >= PedPopulationRemoval_maxDeadPedsListSize)
			{
				break;
			}

			// Get the ped of interest.
			CPed* pPed = pedPool->GetSlot(i);
			if(!pPed || pPed->IsPlayer())
			{
				continue;
			}

			// Make sure the ped is dead.
			if(!pPed->IsDead())
			{
				continue;
			}

			// make sure the time of death has been set
			if(pPed->GetTimeOfDeath() == 0)
			{
				continue;
			}

			// can't delete network clones or dummy peds migrating
			if (pPed->GetNetworkObject() && (pPed->IsNetworkClone() || pPed->GetNetworkObject()->IsPendingOwnerChange() || !pPed->GetNetworkObject()->CanDelete()))
			{
				if (pPed->IsNetworkClone())
				{
					++allDeadClonesToConsider;
				}

				continue;
			}

			// Also can't delete peds inside vehicles that are network clones or migrating
			const CVehicle* pVehicle = pPed->GetMyVehicle();
			if (pVehicle && NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
			{
				continue;
			}

			// Don't remove dead mission peds, as they may actually be required.
			if(pPed->PopTypeIsMission())
			{
				continue;
			}

			static const u32 minTimeDeadFor = 3000;
			static const u32 ScaleTimeDeadFor = 5000;

			// Check time dead for rather than is using ragdoll, the ragdoll might be used for a very long time
			u32 timeDeadFor = (fwTimer::GetTimeInMilliseconds() > pPed->GetTimeOfDeath())  ? (fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfDeath()) : 0;
			if (timeDeadFor < minTimeDeadFor)
			{
				continue;
			}

			// Fill in the the ped or dummy ped holder.
			PedOrDumyPedHolder pedHolder;
			pedHolder.m_pPed = pPed;

			float deathTimeDistScale = Min(float((timeDeadFor - minTimeDeadFor) / ScaleTimeDeadFor), 1.0f);	// 

			// Get the XY delta to this dead ped.
			Vector3 deltaXY = popCtrlCentre - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			deltaXY.z = 0.0f;

			//! Get the closest distance for all network players, not just local distance. This ensures that we score peds for deletion that are furthest away from all
			//! peds 1st.
			if(NetworkInterface::IsGameInProgress())
			{
				float fBestDistSq = deltaXY.XYMag2();

				unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
				const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();

				for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
				{
					const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
					if(remotePlayer && remotePlayer->GetPlayerPed())
					{
						Vector3 deltaXYTemp = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
						deltaXY.z = 0.0f;

						float fDistRemoteSq = deltaXYTemp.XYMag2();
						if(fDistRemoteSq < fBestDistSq)
						{
							deltaXY = deltaXYTemp;
							fBestDistSq = fDistRemoteSq;
						}
					}
				}
			}

			//! Is visible?
			//! Note: Must also check if clone machines can see this ped too.
			bool bVisible = pPed->GetIsVisibleInSomeViewportThisFrame();
			if(NetworkInterface::IsGameInProgress() && !bVisible)
			{
				float fRadius = pPed->GetBoundRadius();
				bVisible = NetworkInterface::IsVisibleToAnyRemotePlayer(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), fRadius, 100.0f);
			}

			// De-prioritize removing on-screen corpses
			if (bVisible)
				deltaXY.Scale(0.01f);

			// Prioritize removing underwater corpses
			if (pPed->m_Buoyancy.GetStatus() == FULLY_IN_WATER)
				deltaXY.Scale(10.0f);

			// De-prioritize peds that might be underneath another ped
			if (pPed->GetTimeOfFirstBeingUnderAnotherRagdoll() > 0)
				deltaXY.Scale(0.8f);

			// And scale based on deathtimer, prevent peds just shot to be removed
			deltaXY.Scale(deathTimeDistScale);

			allDeadPedsAndDeadDummiesList[allDeadPedsAndDeadDummiesListCount].m_distance					= deltaXY.Mag2();
			allDeadPedsAndDeadDummiesList[allDeadPedsAndDeadDummiesListCount].m_timeSinceUnderAnotherPed	= pPed->GetTimeOfFirstBeingUnderAnotherRagdoll() == 0 ? 0.0f : ((float)(fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfFirstBeingUnderAnotherRagdoll())) / 1000.0f;
			allDeadPedsAndDeadDummiesList[allDeadPedsAndDeadDummiesListCount].m_timeDeadFor					= timeDeadFor;
			allDeadPedsAndDeadDummiesList[allDeadPedsAndDeadDummiesListCount].m_deadPedOrDeadDumyPedHolder	= pedHolder;
			++allDeadPedsAndDeadDummiesListCount;
		}// End for each ped.
	}// End making allDeadPedsAndDeadDummiesList.


	
	{	// We basicly test one dead ped against all other dead peds every frame
		// to see if we should remove some of them that are in a cluster
		// If we can spare the memory it might be more efficient to check using a
		// grid of some sort
		int iStart = fwTimer::GetSystemFrameCount() % PedPopulationRemoval_maxDeadPedsListSize;
		if (iStart < allDeadPedsAndDeadDummiesListCount)
		{
			Vector3 PedPos = VEC3V_TO_VECTOR3(allDeadPedsAndDeadDummiesList[iStart].m_deadPedOrDeadDumyPedHolder.m_pPed->GetTransform().GetPosition());
			const float MinDistXYSqr = ms_removalClusterMaxRange * ms_removalClusterMaxRange;
			const int nMaxPedsNearby = ms_removalMaxPedsInRadius;
			int iLeastBurriedPed = iStart;
			float leastTimeUnderneath = allDeadPedsAndDeadDummiesList[iStart].m_timeSinceUnderAnotherPed;
			int nPedsNearby = 0;

			for (int i = 0; i < allDeadPedsAndDeadDummiesListCount; ++i)
			{
				CPed* pPed = allDeadPedsAndDeadDummiesList[i].m_deadPedOrDeadDumyPedHolder.m_pPed;
				if (!pPed)	// Since the ped might be removed below and we continue looping
					break;

				Vector3 TestDistXY = PedPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				float DistXYSqr = TestDistXY.XYMag2();
				if (DistXYSqr < MinDistXYSqr && iStart != i && allDeadPedsAndDeadDummiesList[iLeastBurriedPed].m_timeDeadFor > 2000)  // Don't remove a ped that has just died
				{
					// We want to know which ped is least likely to be underneath another ped (or in the event of a tie, prioritize the one that's been dead longest)
					if ((allDeadPedsAndDeadDummiesList[i].m_timeSinceUnderAnotherPed < leastTimeUnderneath) ||
						(allDeadPedsAndDeadDummiesList[i].m_timeSinceUnderAnotherPed == leastTimeUnderneath && 
						allDeadPedsAndDeadDummiesList[i].m_timeDeadFor > allDeadPedsAndDeadDummiesList[iLeastBurriedPed].m_timeDeadFor))
					{
						leastTimeUnderneath = allDeadPedsAndDeadDummiesList[i].m_timeSinceUnderAnotherPed;
						iLeastBurriedPed = i;
					}

					// Remove the ped that is least likely to be underneath another ped and hasn't just died
					if (nPedsNearby >= nMaxPedsNearby && allDeadPedsAndDeadDummiesList[iLeastBurriedPed].m_timeDeadFor > 2000)
					{
						CPed* pRemovePed = allDeadPedsAndDeadDummiesList[iLeastBurriedPed].m_deadPedOrDeadDumyPedHolder.m_pPed;
						popDebugf2("Removed dead ped 0x%p at (%.2f, %.2f, %.2f), too many bodies close to each other", pRemovePed, pRemovePed->GetTransform().GetPosition().GetXf(), pRemovePed->GetTransform().GetPosition().GetYf(), pRemovePed->GetTransform().GetPosition().GetZf());

						CPedFactory::GetFactory()->DestroyPed(pRemovePed, true);
						allDeadPedsAndDeadDummiesList[iLeastBurriedPed].m_deadPedOrDeadDumyPedHolder.m_pPed = 0;
						leastTimeUnderneath = FLT_MAX;
					}
					else
					{
						++nPedsNearby;
					}
				}
			}
		}
	}	// End cluster checks


	// Create a list of dead peds to remove.
	PF_START_TIMEBAR_DETAIL("Remove dead peds");
	OUTPUT_ONLY(u32 deadPedsRemoved = 0;)

	// We only allow a certain amount globally, make sure we do our part to clean up the dead bodies that we own.
	if(NetworkInterface::IsGameInProgress())
	{
		static const u32 numRemoveExcess = 1;
		static const u32 numMinKeptExcess = 2;
		if(allDeadPedsAndDeadDummiesListCount + allDeadClonesToConsider > numMaxDeadGlobally &&
			allDeadPedsAndDeadDummiesListCount > numMinKeptExcess)
		{
			numMaxDeadAroundPlayer = Min(allDeadPedsAndDeadDummiesListCount - numRemoveExcess, numMaxDeadAroundPlayer);
			numMaxDeadAroundPlayer = Max(numMaxDeadAroundPlayer, numMinKeptExcess);
		}
	}

#if __BANK

	if(CVehiclePopulation::ms_bDrawMPExcessivePedDebug)
	{
		char Buff[128];
		sprintf(Buff, "Dead Peds: %u, Clones: %u, Max Local: %u Max Global: %u", allDeadPedsAndDeadDummiesListCount, allDeadClonesToConsider, numMaxDeadAroundPlayer, numMaxDeadGlobally);
		grcDebugDraw::Text(popCtrlCentre + ZAXIS * 1.25f, Color_yellow, 0, 0, Buff, true, 2);
	}

#endif

	if(numMaxDeadAroundPlayer < allDeadPedsAndDeadDummiesListCount)
	{
		// Sort the list according to distance.
		std::sort(allDeadPedsAndDeadDummiesList, allDeadPedsAndDeadDummiesList + allDeadPedsAndDeadDummiesListCount, DeadsDistanceListSort());

		// The preserved ones should just be the first numMaxDeadAroundPlayer ones in the list.
		for(u32 t = 0; t < allDeadPedsAndDeadDummiesListCount; ++t)
		{
			if(t < numMaxDeadAroundPlayer)
			{
				// Reset the delayed removal timer or not based on the normal population stuff...
				continue;
			}

			// Do not reset the delayed removal timer!

			// If the delayed removal timer has passed...

			// Determine if we should remove the dummy ped or not.
			// Note: Instead of removing them this possibly sets a
			// delay timer on their removal and respects that timer.
			if(allDeadPedsAndDeadDummiesList[t].m_deadPedOrDeadDumyPedHolder.m_pPed)
			{
				CPed* pPed = allDeadPedsAndDeadDummiesList[t].m_deadPedOrDeadDumyPedHolder.m_pPed;
// 				// Try and pass control of this dummy onto another machine if the dummy
// 				// is still within the cull range on that machine.
// 				bool passedControl = false;
// 				if(pPed->GetNetworkObject())
// 				{
// 					passedControl = !(((CNetObjDummyPed*)pPed->GetNetworkObject())->TryToPassControlOutOfScope());
// 				}
// 				if(!passedControl)

				{
					popDebugf2("Removed dead ped 0x%p at (%.2f, %.2f, %.2f), too many bodies lying around", pPed, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());
					CPedFactory::GetFactory()->DestroyPed(pPed, true);
					OUTPUT_ONLY(deadPedsRemoved++;)
				}

				allDeadPedsAndDeadDummiesList[t].m_deadPedOrDeadDumyPedHolder.m_pPed = 0;
			}
		}
	}
	/*
	MK - Disabling this code as the above code should be enough to keep the number of dead bodies in MP down
	static const unsigned int DEAD_PED_NETWORK_REMOVAL_TIMER = 10000;
	static const unsigned int NUM_DEAD_PEDS_ALLOWED = 2;

	// remove all dead peds that have dead for too long in the network game
	PF_START_TIMEBAR_DETAIL("Remove Network Dead Peds");
	OUTPUT_ONLY(u32 netDeadPedsRemoved = 0;)
	if(NetworkInterface::IsGameInProgress() && allDeadPedsAndDeadDummiesListCount > NUM_DEAD_PEDS_ALLOWED)
	{
		CPed      *pDeadestPed      = NULL;
		u32 longestDead = DEAD_PED_NETWORK_REMOVAL_TIMER;
		for(u32 t = 0; t < allDeadPedsAndDeadDummiesListCount; ++t)
		{
			if(allDeadPedsAndDeadDummiesList[t].m_timeDeadFor > longestDead)
			{
				pDeadestPed = allDeadPedsAndDeadDummiesList[t].m_deadPedOrDeadDumyPedHolder.m_pPed;
				longestDead = allDeadPedsAndDeadDummiesList[t].m_timeDeadFor;
			}
		}

		//We dont want to remove dead ped's that are inside vehicles:
		//  - In the network game respawn will take the place of the player
		if (pDeadestPed && !(pDeadestPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)))
		{
			if(!pDeadestPed->IsNetworkClone() && !(pDeadestPed->GetNetworkObject() && pDeadestPed->GetNetworkObject()->IsPendingOwnerChange()))
			{
				popDebugf2("Removed dead ped 0x%p at (%.2f, %.2f, %.2f) in net game, was lying around for too long", pDeadestPed, pDeadestPed->GetTransform().GetPosition().GetXf(), pDeadestPed->GetTransform().GetPosition().GetYf(), pDeadestPed->GetTransform().GetPosition().GetZf());
				CPedFactory::GetFactory()->DestroyPed(pDeadestPed, true);
				OUTPUT_ONLY(netDeadPedsRemoved++;)
			}
		}
	}

	OUTPUT_ONLY(if (deadPedsRemoved > 0))
	popDebugf3("%d bodies and additional %d net bodies were removed for lying around for too long", deadPedsRemoved, netDeadPedsRemoved);
	*/
}


s32 CPedPopulation::GetDesiredMaxNumScenarioPeds(bool bForInteriors, s32 * iPopCycleTempMaxScenarioPeds)
{
	s32 maxNumPedsHereAndNow = (s32) (CPopCycle::GetCurrentMaxNumScenarioPeds());

	// Allow 5 more in interiors!
	if( bForInteriors )
	{
		maxNumPedsHereAndNow += 5;
	}

	float fInteriorMult = 1.0f;
	float fExteriorMult = 1.0f;
	CPedPopulation::GetTotalScenarioPedDensityMultipliers( fInteriorMult, fExteriorMult );
	if( bForInteriors )
	{
		maxNumPedsHereAndNow = static_cast<s32>(maxNumPedsHereAndNow*fInteriorMult);
	}
	else
	{
		maxNumPedsHereAndNow = static_cast<s32>(maxNumPedsHereAndNow*fExteriorMult);
	}

#if __DEV
	if(ms_popCycleOverrideNumberOfScenarioPeds >= 0)
	{
		maxNumPedsHereAndNow = ms_popCycleOverrideNumberOfScenarioPeds;
		// Allow 5 more in interiors!
		if( bForInteriors )
		{
			maxNumPedsHereAndNow += 5;
		}
	}
#endif

	if(iPopCycleTempMaxScenarioPeds)
		*iPopCycleTempMaxScenarioPeds = CPopCycle::GetTemporaryMaxNumScenarioPeds();

	maxNumPedsHereAndNow = MIN(maxNumPedsHereAndNow, CPopCycle::GetTemporaryMaxNumScenarioPeds());
	maxNumPedsHereAndNow = (s32)((float)maxNumPedsHereAndNow * ms_pedMemoryBudgetMultiplier);

	return maxNumPedsHereAndNow;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HowManyMoreScenarioPedsShouldBeGenerated
// PURPOSE :	To query how many scenario peds should be generated
// PARAMETERS :	None.
// RETURNS :	How many peds are needed in the population.
/////////////////////////////////////////////////////////////////////////////////
s32 CPedPopulation::HowManyMoreScenarioPedsShouldBeGenerated(bool bForInteriors)
{
#if !__FINAL
	// Make sure that adding to the population isn't currently blocked.
	if(!gbAllowAmbientPedGeneration)
	{
		return 0;
	}
#endif // __DEV

	s32 maxNumPedsHereAndNow = GetDesiredMaxNumScenarioPeds(bForInteriors);

	// Make sure more peds are actually needed.
	// If no more peds (of any type) are necessary let the caller know.
	if(ms_nNumOnFootScenario >= maxNumPedsHereAndNow)
	{
		return 0;
	}
	return maxNumPedsHereAndNow-ms_nNumOnFootScenario;
}


s32 CPedPopulation::GetDesiredMaxNumAmbientPeds(s32 * iPopCycleMaxPeds, s32 * iPopCycleMaxCopPeds, s32 * iPopCycleTempMaxAmbientPeds)
{
	s32 maxNumPedsHereAndNow = (s32) ((CPopCycle::GetCurrentMaxNumAmbientPeds() + CPopCycle::GetCurrentMaxNumCopPeds()));

#if __DEV
	if(ms_popCycleOverrideNumberOfAmbientPeds >= 0)
	{
		maxNumPedsHereAndNow = ms_popCycleOverrideNumberOfAmbientPeds;
	}
#endif

	maxNumPedsHereAndNow = MIN(maxNumPedsHereAndNow, CPopCycle::GetTemporaryMaxNumAmbientPeds());

	if(iPopCycleMaxPeds)
		*iPopCycleMaxPeds = CPopCycle::GetCurrentMaxNumAmbientPeds();
	if(iPopCycleMaxCopPeds)
		*iPopCycleMaxCopPeds = CPopCycle::GetCurrentMaxNumCopPeds();
	if(iPopCycleTempMaxAmbientPeds)
		*iPopCycleTempMaxAmbientPeds = CPopCycle::GetTemporaryMaxNumAmbientPeds();

	return maxNumPedsHereAndNow;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HowManyMoreAmbientPedsShouldBeGenerated
// PURPOSE :	To query how many wandering peds should be generated
// PARAMETERS :	None.
// RETURNS :	How many peds are needed in the population.
/////////////////////////////////////////////////////////////////////////////////
s32 CPedPopulation::HowManyMoreAmbientPedsShouldBeGenerated(float * fScriptPopAreaMultiplier, float * fPedMemoryBudgetMultiplier)
{
#if !__FINAL
	// Make sure that adding to the population isn't currently blocked.
	if(!gbAllowAmbientPedGeneration)
	{
		return 0;
	}
#endif // __DEV

	s32 maxNumPedsHereAndNow = GetDesiredMaxNumAmbientPeds();
	s32 numPeds = maxNumPedsHereAndNow - ms_nNumOnFootAmbient;

	// if the scripters have placed down and activated custom areas that alter the ped and traffic density.....
	float pedMultiplier = 1.0f;
	if(CThePopMultiplierAreas::GetNumPedAreas() > 0)
	{
		if(gVpMan.GetViewportStackDepth() > 0 && gVpMan.GetCurrentViewport())
		{
			const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
			const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());

			// Get the traffic multiplier from the cam pos (i.e go through all areas that contain the cam pos and multiply through)...
			pedMultiplier = CThePopMultiplierAreas::GetPedDensityMultiplier(camPos, true);
		}

		numPeds = (s32)((float)numPeds * pedMultiplier);
	}

	if(fScriptPopAreaMultiplier)
		*fScriptPopAreaMultiplier = pedMultiplier;
	if(fPedMemoryBudgetMultiplier)
		*fPedMemoryBudgetMultiplier = ms_pedMemoryBudgetMultiplier;

	numPeds = (s32)((float)numPeds * ms_pedMemoryBudgetMultiplier);
	return numPeds;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AddRandomPedOrPedGroupToPopulation
// PURPOSE :	Adds a random ped or group of peds to the current population
// PARAMETERS :	popCtrlCentre - The centre point about which to base our ranges
//				on.
//				fAddRangeOutOfViewMin, fAddRangeOutOfViewMax,
//				fAddRangeInViewMin, fAddRangeInViewMax - min and max radii,
//				the ped will be added somewhere within these two boundaries.
//				Note: Try to keep at least 10 meters between the two otherwise
//				it may not work very well.
// RETURNS :	If more peds are needed in the population.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::AddAmbientPedToPopulation(bool popCtrlIsInInterior, const float fAddRangeInViewMin, const float fAddRangeOutOfViewMin, const bool doInFrustumTest, const bool bCachedPointsRemaining, const bool atStart)
{
	PF_AUTO_PUSH_TIMEBAR("AddAmbientPedToPopulation");
    // Check the network population allows a new ambient ped to created in network games
    // This is a common point of failure in network games so is checked first
    if(NetworkInterface::IsGameInProgress())
    {
        const u32 numPeds = 1;
        if(!NetworkInterface::CanRegisterLocalAmbientPeds(numPeds))
        {
            // No space for a new ped
		    BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_notAllowdByNetwork, VEC3_ZERO));
		    popDebugf3("Network population doesn't allow more ambient peds to be created!");
		    return false;
        }
    }

	// check to see if we're going to be able to schedule a new ped at all
	// if we're atStart or instant filling, then we're going to synchronously spawn anyway
	if(ms_scheduledAmbientPeds.IsFull() && !(atStart || GetInstantFillPopulation()))
	{
		return false; // we can't spawn a ped now, so exit early
	}
	
	// Try to get a ped type to create, this tries to find the least common appropriate
	// type to use.
	u32 newPedModelIndex = fwModelId::MI_INVALID;

	// Determine where to create our new ped.
	const float pedRadius = 1.21f;
	Vector3 newPedGroundPos(0.0f, 0.0f, 0.0f);
	bool bGroundPosIsInInterior	= false;
	s32 roomIdx = -1;
	CInteriorInst* pIntInst = NULL;

	bool goodPositionFound = false;
	if (bCachedPointsRemaining)
	{
        goodPositionFound = GetPedGenerationGroundCoord(
			newPedGroundPos,
			bGroundPosIsInInterior,
			popCtrlIsInInterior,
			fAddRangeInViewMin,
			fAddRangeOutOfViewMin,
			pedRadius,
			doInFrustumTest,
			roomIdx,
			pIntInst);

		if (!goodPositionFound)
		{
			// We couldn't find a spawn position, but we still need more peds.
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_noSpawnPosFound, VEC3_ZERO));
			popDebugf3("Couldn't find valid spawn position for ped but we still need to spawn more!");
			return true;
		}

		// Test for intersection with dummy or real objects
		if(ms_bPerformSpawnCollisionTest)
		{
			int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			static dev_float fCapsuleHeight = 1.8f;
			static dev_float fCapsuleRadius = 0.25f;
			capsuleDesc.SetCapsule(newPedGroundPos, Vector3(0.0f,0.0f,fCapsuleHeight), fCapsuleHeight,fCapsuleRadius);
			capsuleDesc.SetIncludeFlags(nTestTypeFlags);

			if( WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc) )
			{
#if __BANK
				if(ms_bDrawSpawnCollisionFailures)
				{
					grcDebugDraw::Capsule(RCC_VEC3V(newPedGroundPos), VECTOR3_TO_VEC3V(newPedGroundPos + Vector3(0.0f,0.0f,fCapsuleHeight)), fCapsuleRadius, Color_red, false, 1000);
				}
#endif
				return true;
			}
		}
	}

	CPopCycleConditions conditions;
	const CAmbientModelVariations* pVariations = NULL;
	if(!CPopCycle::FindNewPedModelIndex(&newPedModelIndex, false, false, POPGROUP_AMBIENT, conditions, pVariations, bCachedPointsRemaining ? &newPedGroundPos : NULL))
	{
		// We couldn't find a new ped type, but we still need more peds.
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_noPedModelFound, VEC3_ZERO));
		popDebugf3("Couldn't find ped types to create but we still need to spawn more!");
		return true;
	}

	const float wildLifePedRadius = 0.3f;
	if (conditions.m_bSpawnInAir)
	{
		goodPositionFound = CWildlifeManager::GetInstance().GetFlyingPedCoord(
			newPedGroundPos,
			bGroundPosIsInInterior,
			popCtrlIsInInterior,
			fAddRangeInViewMin,
			fAddRangeOutOfViewMin,
			wildLifePedRadius,
			doInFrustumTest,
			newPedModelIndex);
	}
	else if (conditions.m_bSpawnAsWildlife)
	{
		goodPositionFound = CWildlifeManager::GetInstance().GetGroundWildlifeCoord(
			newPedGroundPos,
			bGroundPosIsInInterior,
			popCtrlIsInInterior,
			fAddRangeInViewMin,
			wildLifePedRadius,
			doInFrustumTest,
			newPedModelIndex);
	}
	else if (conditions.m_bSpawnInWater)
	{
		goodPositionFound = CWildlifeManager::GetInstance().GetWaterSpawnCoord(
			newPedGroundPos,
			bGroundPosIsInInterior,
			popCtrlIsInInterior,
			fAddRangeInViewMin,
			wildLifePedRadius,
			doInFrustumTest,
			newPedModelIndex);
	}

	if(!goodPositionFound)
	{
		// We couldn't find a spawn position, but we still need more peds.
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Wandering, PedPopulationFailureData::FR_noSpawnPosFound, VEC3_ZERO));
		popDebugf3("Couldn't find valid spawn position for ped but we still need to spawn more!");
		return true;
	}

	// The root pos of a ped should be about 1 meter above the ground.
	const Vector3 newPedRootPos(newPedGroundPos.x, newPedGroundPos.y, newPedGroundPos.z + 1.0f);

	// Get a random heading for our new ped.
	const float fHeading = fwAngle::LimitRadianAngle(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));

	const DefaultScenarioInfo defaultScenarioInfo = CScenarioManager::GetDefaultScenarioInfoForRandomPed(newPedRootPos, newPedModelIndex);
	Assertf(!(ms_scheduledAmbientPeds.IsFull() && !(atStart || GetInstantFillPopulation())), "Scheduled Ambient Ped Queue is Full");
	// Try to create a new ped	
	CPed* pPed = NULL;
	if(atStart || GetInstantFillPopulation()) // spawn the ped right away
	{
		ms_bCreatingPedsForAmbientPopulation = true;
		pPed = AddPed(newPedModelIndex, newPedRootPos, fHeading, NULL, bGroundPosIsInInterior, 
			roomIdx, pIntInst, true, true, true, true, defaultScenarioInfo, false, false, 0, 0, pVariations, &conditions);
		ms_bCreatingPedsForAmbientPopulation = false;
	}
	else if(!ms_scheduledAmbientPeds.IsFull())
	{		
		// add the ped to the ambient ped queue
		ScheduledAmbientPed& ambientPed = ms_scheduledAmbientPeds.Append();
		
		CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(newPedModelIndex)));
		pPedModelInfo->AddPedModelRef();
		pPedModelInfo->AddRef();
		
		Assertf(pPedModelInfo->GetDrawable() || pPedModelInfo->GetFragType(), "Scheduling Ambient Ped %s:Ped model is not loaded", pPedModelInfo->GetModelName());

		ambientPed.uNewPedModelIndex = newPedModelIndex;
		ambientPed.vNewPedRootPos = newPedRootPos;
		ambientPed.fHeading = fHeading;
		ambientPed.bGroundPosIsInInterior = bGroundPosIsInInterior;
		ambientPed.pIntInst = pIntInst;
		ambientPed.iRoomIdx = roomIdx;
		ambientPed.defaultScenarioInfo = defaultScenarioInfo;
		ambientPed.pVariations = pVariations;
		ambientPed.conditions = conditions;
		ambientPed.uFrameScheduled = fwTimer::GetFrameCount();
		ambientPed.bNetDoInFrustumTest = doInFrustumTest;
		popDebugf2("Scheduling Ambient Ped (%s) at (%.2f, %.2f, %.2f)", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(newPedModelIndex))), newPedRootPos.x, newPedRootPos.y, newPedRootPos.z);
	}
	
	if(!pPed)
	{
		// We couldn't create the ped, but we still need more peds.
		return true;
	}

	//Note that a ped was spawned.
	OnPedSpawned(*pPed, OPSF_InAmbient);

	popDebugf2("Created ambient ped 0x%p (%s) at (%.2f, %.2f, %.2f)", pPed, CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(newPedModelIndex))), newPedRootPos.x, newPedRootPos.y, newPedRootPos.z);

	pPed->SetDefaultDecisionMaker();
	pPed->SetCharParamsBasedOnManagerType();
	pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
	pPed->SetDefaultRemoveRangeMultiplier();

	ManageSpecialStreamingConditions(pPed->GetModelIndex());

	// This stuff may help to prevent physics activation if we spawn at a distance.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding, true);
	CPedAILod& lod = pPed->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	lod.SetForcedLodFlag(CPedAILod::AL_LodPhysics);

	return true;
}


// Try to spawn a ped from the scheduled ped queue
// Returns how many peds were spawned
s32 CPedPopulation::SpawnScheduledAmbientPed(const float fAddRangeInViewMin)
{
	int numNew = 0;	
	while(!ms_scheduledAmbientPeds.IsEmpty())
	{
		ScheduledAmbientPed& scheduledPed = ms_scheduledAmbientPeds.Top();
		
#if __DEV // check to see if the ped is loaded
		CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.uNewPedModelIndex)));
		Assertf(pPedModelInfo->GetDrawable() || pPedModelInfo->GetFragType(), "Spawning Ambient Ped %s:Ped model is not loaded", pPedModelInfo->GetModelName());
#endif

		// Check to see if ped has expired
		if(scheduledPed.uFrameScheduled < (fwTimer::GetFrameCount() - SCHEDULED_PED_TIMEOUT))
		{
			DropScheduledAmbientPed();
			continue;
		}

		// We have to check that this spawn coord is still valid.
		// Script might have set up a blocking area in the intervening frames.
		// This code should probably call "IsPedGenerationCoordCurrentlyValid()" to handle
		// all the other cases.
		if(PointFallsWithinNonCreationArea(scheduledPed.vNewPedRootPos))
		{
			DropScheduledAmbientPed();
			continue;
		}

		// url:bugstar:1457112
		// As alluded to above, we may need to perform some other tests (eg. for position being in frustum & too close to view position)
		bool bOnScreen = IsCandidateInViewFrustum(scheduledPed.vNewPedRootPos, 1.0f, fAddRangeInViewMin);
		if(bOnScreen)
		{
			DropScheduledAmbientPed();
			continue;
		}

		// The level designers can suppress specific models
		if(	scheduledPed.uNewPedModelIndex !=fwModelId::MI_INVALID && 
			CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(scheduledPed.uNewPedModelIndex)
			)
		{
			DropScheduledAmbientPed();
			continue;
		}

		// Prevent unallowed peds from spawning in MP -- Used by the spawning system to prevent invalid (birds, etc) peds from spawning (in MP).
		if (!CWildlifeManager::GetInstance().CheckWildlifeMultiplayerSpawnConditions(scheduledPed.uNewPedModelIndex))
		{
			DropScheduledAmbientPed();
			continue;
		}

		const float pedRadius = 1.21f;
		if(	NetworkInterface::IsGameInProgress() &&
			scheduledPed.bNetDoInFrustumTest && 
			NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(scheduledPed.vNewPedRootPos, pedRadius, ms_addRangeBaseInViewMin * g_LodScale.GetGlobalScale(), ms_addRangeBaseOutOfViewMin))
		{
			popDebugf2("Network dropped schedule ambient ped visible to other player (%s) at (%.2f, %.2f, %.2f)", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(scheduledPed.uNewPedModelIndex))), scheduledPed.vNewPedRootPos.x, scheduledPed.vNewPedRootPos.y, scheduledPed.vNewPedRootPos.z);
			// Don't add peds if they are going to appear in front of other network players.
			DropScheduledAmbientPed();
			continue;
		}

		CPed* pPed = NULL;
		
		pPed = AddPed(scheduledPed.uNewPedModelIndex, scheduledPed.vNewPedRootPos, scheduledPed.fHeading, NULL, scheduledPed.bGroundPosIsInInterior, 
			scheduledPed.iRoomIdx, scheduledPed.pIntInst, true, true, true, true, scheduledPed.defaultScenarioInfo, false, false, 0, 0, scheduledPed.pVariations, &scheduledPed.conditions);
		
		DropScheduledAmbientPed(); // for either success or failure, we want this ped out of the queue

		if(!pPed)
		{
			// We couldn't create the ped
			continue;
		}

		//Note that a ped was spawned.
		OnPedSpawned(*pPed, OPSF_InAmbient);

		popDebugf2("Spawned scheduled ambient ped 0x%p (%s) at (%.2f, %.2f, %.2f)", pPed, CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(scheduledPed.uNewPedModelIndex))), scheduledPed.vNewPedRootPos.x, scheduledPed.vNewPedRootPos.y, scheduledPed.vNewPedRootPos.z);

		pPed->SetDefaultDecisionMaker();
		pPed->SetCharParamsBasedOnManagerType();
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
		pPed->SetDefaultRemoveRangeMultiplier();

		ManageSpecialStreamingConditions(pPed->GetModelIndex());

		// This stuff may help to prevent physics activation if we spawn at a distance.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding, true);
		CPedAILod& lod = pPed->GetPedAiLod();
		lod.ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
		lod.SetForcedLodFlag(CPedAILod::AL_LodPhysics);

		numNew++;
		break;
	}
	return numNew;
}

// Remove the top ambient ped from the spawn queue, and remove the refs we added to keep it from streaming out
void CPedPopulation::DropScheduledAmbientPed()
{
	if(!ms_scheduledAmbientPeds.IsEmpty())
	{
		ScheduledAmbientPed& scheduledPed = ms_scheduledAmbientPeds.Pop();

		if(scheduledPed.uNewPedModelIndex!=fwModelId::MI_INVALID)
		{
			// Remove our artificial refs
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.uNewPedModelIndex)));
			pPedModelInfo->RemovePedModelRef();
			pPedModelInfo->RemoveRef();
		}
	}	
}

int	CPedPopulation::CountScheduledPedsForIncident(CIncident* pDispatchIncident, DispatchType iDispatchType)
{
	int iCount = 0;

	if(pDispatchIncident)
	{
		int iNumScheduledPeds = ms_scheduledPedsInVehicles.GetCount();
		for(unsigned int i = 0; i < iNumScheduledPeds; i++)
		{
			ScheduledPedInVehicle& scheduledPed = ms_scheduledPedsInVehicles[i];
			if(pDispatchIncident == scheduledPed.pIncident)
			{
				if(iDispatchType == 0 || iDispatchType == scheduledPed.dispatchType)
				{
					iCount++;
				}
			}
		}
	}

	return iCount;
}

float CPedPopulation::GetDistanceSqrToClosestScheduledPedWithModelId(const Vector3& pos, u32 modelIndex)
{	
	float closestDist2 = FLT_MAX;	
	for(int i = 0; i < ms_scheduledAmbientPeds.GetCount(); i++)
	{
		if(ms_scheduledAmbientPeds[i].uNewPedModelIndex == modelIndex)
		{
			float dist2 = ms_scheduledAmbientPeds[i].vNewPedRootPos.Dist2(pos);
			if(dist2 < closestDist2)
			{
				closestDist2 = dist2;
			}
		}
	}
	for(int i = 0; i < ms_scheduledPedsInVehicles.GetCount(); i++)
	{
		if(ms_scheduledPedsInVehicles[i].desiredModel == modelIndex && ms_scheduledPedsInVehicles[i].pVehicle)
		{
			// use position of the vehicle
			float dist2 = VEC3V_TO_VECTOR3(ms_scheduledPedsInVehicles[i].pVehicle->GetTransform().GetPosition()).Dist2(pos);
			if(dist2 < closestDist2)
			{
				closestDist2 = dist2;
			}
		}
	}
	for(int i = 0; i < ms_scheduledScenarioPeds.GetCount(); i++)
	{
		if(ms_scheduledScenarioPeds[i].predeterminedPedType.m_PedModelIndex == modelIndex && ms_scheduledScenarioPeds[i].pScenarioPoint)
		{
			// use scenario point position
			float dist2 = VEC3V_TO_VECTOR3(ms_scheduledScenarioPeds[i].pScenarioPoint->GetPosition()).Dist2(pos);
			if(dist2 < closestDist2)
			{
				closestDist2 = dist2;
			}
		}
	}	

	return closestDist2;
}

bool CPedPopulation::SchedulePedInVehicle(CVehicle* pVehicle, s32 seat_num, bool canLeaveVehicle, u32 desiredModel, const CAmbientModelVariations* pVariations/*=NULL*/, CIncident* pDispatchIncident/*=NULL*/, DispatchType iDispatchType /*= DT_INVALID*/, s32 pedGroupId/* = -1*/, COrder* pOrder /* = NULL */)
{
	if(ms_scheduledPedsInVehicles.IsFull()) // if we're full
	{
		if(pVehicle->IsLawEnforcementVehicle() || pDispatchIncident) // we don't want to drop law enforcement peds or peds with incidents to respond to
		{
			popDebugf1("Spawning a scheduled ped in vehicle to make room for a law enforcement ped");		
			SpawnScheduledPedInVehicle(true); // spawn the first one in line, even if we just scheduled it, in order to make room
		}
		else
		{
			popDebugf1("Failed to schedule non lawn-enforcement ped in vehicle - queue is full.");		
			return false;
		}
	}

	// In a non-empty law enforcement or dispatched vehicle
	if( !ms_scheduledPedsInVehicles.IsFull() && !pVehicle->IsLawEnforcementVehicle() && !pDispatchIncident )
	{
		if( pVehicle->IsSeatIndexValid(seat_num) )
		{
			// If the seat specified keeps collision on
			const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(seat_num);
			if( pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle() )
			{
				// do not schedule a civilian ped hanging on externally
				return false;
			}
		}
	}
	
	if(!ms_scheduledPedsInVehicles.IsFull())
	{
		ScheduledPedInVehicle& scheduledPed = ms_scheduledPedsInVehicles.Append();

		scheduledPed.pVehicle = pVehicle;
		scheduledPed.seat_num = seat_num;
		scheduledPed.canLeaveVehicle = canLeaveVehicle;
		scheduledPed.desiredModel = desiredModel;
		scheduledPed.pVariations = pVariations;
		scheduledPed.pIncident = (CIncident*)pDispatchIncident;
		scheduledPed.dispatchType = iDispatchType;
		scheduledPed.pedGroupId = pedGroupId;
		scheduledPed.pOrder = pOrder;
		scheduledPed.uFrameScheduled = fwTimer::GetFrameCount();

		// Add a model ref to our desired model if, to keep it from streaming out
		if(scheduledPed.desiredModel!=fwModelId::MI_INVALID)
		{
			// add our artificial refs
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.desiredModel)));
			pPedModelInfo->AddPedModelRef();
			pPedModelInfo->AddRef();
		}

		// tell the vehicle that it has a ped pending
		pVehicle->AddScheduledOccupant();

		return true; // scheduled the ped
	}
	return false;
}

s32 CPedPopulation::SpawnScheduledPedInVehicle(bool bSkipFrameCheck /*= false*/)
{
	int numNew = 0;
	// Try to spawn one ped in a vehicle
	int maxTries = ms_scheduledPedsInVehicles.GetCount();
	while((maxTries > 0) && !numNew)
	{
		maxTries--;	
		ScheduledPedInVehicle& scheduledPed = ms_scheduledPedsInVehicles.Top();

		// Don't spawn a ped the same frame it was scheduled - that's what we're trying to avoid!
		if(scheduledPed.uFrameScheduled == fwTimer::GetFrameCount() && !bSkipFrameCheck )
		{
			break; // handle this ped next frame, as we just scheduled it
		}

		// Check if the ped can be rescheduled
		bool bCanPedBeRescheduled = (scheduledPed.dispatchType != DT_Invalid) &&
			(scheduledPed.pVehicle && !scheduledPed.pVehicle->GetIsVisibleInSomeViewportThisFrame());

		// at this point we're going to spawn or drop the ped, so clear it from the vehicle
		if(scheduledPed.pVehicle)
		{
			scheduledPed.pVehicle->RemoveScheduledOccupant();
		}

		const u32 scheduledPedTimeout = scheduledPed.dispatchType == DT_Invalid ? SCHEDULED_PED_TIMEOUT : SCHEDULED_DISPATCH_PED_TIMEOUT;

		// Check to see if ped has expired
		if(scheduledPed.uFrameScheduled < (fwTimer::GetFrameCount() - scheduledPedTimeout))
		{
			Assertf(scheduledPed.dispatchType == DT_Invalid || bCanPedBeRescheduled, "Scheduled passenger dispatch type %s timed out", CDispatchManager::GetInstance().GetDispatchTypeName(scheduledPed.dispatchType));
			if(!bCanPedBeRescheduled)
			{
				DropScheduledPedInVehicle();
			}
			else
			{
				ReschedulePedInVehicle();
			}
			continue;
		}

		// make sure the vehicle is still valid
		if(scheduledPed.pVehicle)
		{
			// The level designers can suppress specific models
			if(	scheduledPed.desiredModel!=fwModelId::MI_INVALID && 
				CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(scheduledPed.desiredModel)
				)
			{
				Assertf(scheduledPed.dispatchType == DT_Invalid, "Scheduled passenger dispatch type %s blocked by script", CDispatchManager::GetInstance().GetDispatchTypeName(scheduledPed.dispatchType));
				DropScheduledPedInVehicle();
				continue;
			}

			if(NetworkInterface::IsGameInProgress() && scheduledPed.pedGroupId >= 0)
			{
				Assert(scheduledPed.pedGroupId < CPedGroups::MAX_NUM_GROUPS);
				// don't try to spawn in ped groups we don't control
				if(!CPedGroups::ms_groups[scheduledPed.pedGroupId].IsLocallyControlled())
				{
					DropScheduledPedInVehicle();
					continue;
				}
			}

			// in network games, dispatch can have it's own network object reservation pool
			bool bHasReservedNetEntities = false;
			if (NetworkInterface::IsGameInProgress() && scheduledPed.dispatchType != DT_Invalid)
			{
				bHasReservedNetEntities = CDispatchManager::GetNetworkPopulationReservationId() != CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID;

				// inform the network population code that any entities created by the dispatch services will be linked to the dispatch manager reservation 
				if (bHasReservedNetEntities)
				{
					CNetworkObjectPopulationMgr::SetCurrentExternalReservationIndex(CDispatchManager::GetNetworkPopulationReservationId());
				}
			}

			// try to spawn the ped
			CPed* newPed = scheduledPed.pVehicle->SetupPassenger(scheduledPed.seat_num, scheduledPed.canLeaveVehicle, scheduledPed.desiredModel, scheduledPed.pVariations);
			if(newPed)
			{
				scheduledPed.pVehicle->TryToGiveEventReponseTaskToOccupant(newPed);

				if(scheduledPed.dispatchType != DT_Invalid)
				{
					CDispatchService::DispatchDelayedSpawnStaticCallback(static_cast<CEntity*>(newPed), scheduledPed.pIncident, scheduledPed.dispatchType, scheduledPed.pedGroupId, scheduledPed.pOrder);
				}
				numNew++;
			}

			// clean up our nework object reservation state
			if (bHasReservedNetEntities)
			{
				CNetworkObjectPopulationMgr::ClearCurrentReservationIndex();
			}

			if(!newPed && bCanPedBeRescheduled) // failed to create the ped, but can reschedule it
			{
				ReschedulePedInVehicle();
				continue;
			}
		}

		// We're done with this ped now
		DropScheduledPedInVehicle();
	}
	return numNew;
}


// Remove the top vehicle ped from the spawn queue, and remove the refs we added to keep it from streaming out
void CPedPopulation::DropScheduledPedInVehicle()
{
	if(!ms_scheduledPedsInVehicles.IsEmpty())
	{
		ScheduledPedInVehicle& scheduledPed = ms_scheduledPedsInVehicles.Pop();

		//we might need to clean up some pretend occupant event handling data
		if (scheduledPed.pVehicle && scheduledPed.pVehicle->GetIntelligence()->GetNumPedsThatNeedTaskFromPretendOccupant() > 0)
		{
			scheduledPed.pVehicle->GetIntelligence()->DecrementNumPedsThatNeedTaskFromPretendOccupant();

			if (scheduledPed.pVehicle->GetIntelligence()->GetNumPedsThatNeedTaskFromPretendOccupant() < 1)
			{
				scheduledPed.pVehicle->GetIntelligence()->ResetPretendOccupantEventData();
				scheduledPed.pVehicle->GetIntelligence()->FlushPretendOccupantEventGroup();
			}	
		}

		// set the RegdVeh pointer to NULL
		scheduledPed.pVehicle = NULL;

		if(scheduledPed.desiredModel!=fwModelId::MI_INVALID)
		{
			// Remove our artificial refs
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.desiredModel)));
			pPedModelInfo->RemovePedModelRef();
			pPedModelInfo->RemoveRef();
		}
	}	
}

// Remove the top vehicle ped from the spawn queue, and re-add to the end
void CPedPopulation::ReschedulePedInVehicle()
{
	// Really only makes sense if 2 or more peds are scheduled
	int iCount = ms_scheduledPedsInVehicles.GetCount();
	if(iCount == 1)
	{
		ms_scheduledPedsInVehicles[0].uFrameScheduled = fwTimer::GetFrameCount();

		// we removed the scheduled ped, so add it back
		if(ms_scheduledPedsInVehicles[0].pVehicle)
			ms_scheduledPedsInVehicles[0].pVehicle->AddScheduledOccupant();
	}
	else if(iCount > 1)
	{
		ScheduledPedInVehicle& scheduledPed = ms_scheduledPedsInVehicles.Pop();
		scheduledPed.uFrameScheduled = fwTimer::GetFrameCount();
		ms_scheduledPedsInVehicles.Push(scheduledPed);

		// we removed the scheduled ped, so add it back
		if(scheduledPed.pVehicle)
			scheduledPed.pVehicle->AddScheduledOccupant();
	}	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetUpParamsForPathServerPedGenBackgroundTask
// PURPOSE :	Sets up a ped generation point system which is being run in
//				another thread.
// PARAMETERS :	popGenShape- the shape about which to generate the points.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::SetUpParamsForPathServerPedGenBackgroundTask(const CPopGenShape & popGenShape)
{
	if(HowManyMoreAmbientPedsShouldBeGenerated() <= 0)
	{
		// Turn off pedgen stuff whenever possible.
		CPathServer::GetAmbientPedGen().UnsetPedGenParams();
	}
	else
	{
		CPathServerAmbientPedGen::TPedGenParams params;
		s32 iPedCount = 0;

		// Get and store all the current peds' positions.
		s32 iPedPoolSize = CPed::GetPool()->GetSize();
		while(iPedPoolSize--)
		{
			CPed * pPed = CPed::GetPool()->GetSlot(iPedPoolSize);
			if( pPed && !pPed->IsPlayer() && !pPed->GetVehiclePedInside() && !pPed->IsDead() )
			{
				ms_vPedPositionsArray[iPedCount] = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				ms_iPedFlagsArray[iPedCount] = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsInStationaryScenario) ? 
					CPathServerAmbientPedGen::TPedGenParams::FLAG_SCENARIO_PED : 0;
				iPedCount++;
			}
		}

		params.m_pPedPositionsArray = &ms_vPedPositionsArray[0];
		params.m_iPedFlagsArray = &ms_iPedFlagsArray[0];
		params.m_iNumPedPositions = iPedCount;

		CPathServer::GetAmbientPedGen().SetPedGenParams(params, popGenShape);
	}

}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPedGenerationGroundCoordFromPathserver
// PURPOSE :	Get up a ped generation point either synchronously or from the
//				system which is being run in another thread.
// PARAMETERS :	vOutPos- ped generation point.
//				vLocationCentre- the centre about which to generate the points.
//				fMinDistOutOfView- The out of view zone min dist
//				fMaxDistOutOfView- The out of view zone max dist
//				fMinDistInView- The in of view zone min dist
//				fMaxDistInView- The in of view zone max dist
// RETURNS :	whether a point is available or not.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::GetPedGenerationGroundCoordFromPathserver(Vector3& newPedGroundPosOut, bool& bIsInInteriorOut, bool& bUsableIfOccludedOut)
{
	CPedGenCoords::TItem coord;
	if(!g_CachedPedGenCoords.GetNext(coord))
		return false;

	// HACK: Avoid creating peds within this AABB (url:bugstar:1629900)
	static const Vector3 vHardcodedAABB[2] =
	{
		Vector3(-104.0f, 264.0f, 95.0f),
		Vector3(-52.0f, 295.0f, 115.0f)
	};

	while(coord.m_vPosition.IsGreaterThan(vHardcodedAABB[0]) && vHardcodedAABB[1].IsGreaterThan(coord.m_vPosition))
	{
		if(!g_CachedPedGenCoords.GetNext(coord))
			return false;
	}

	newPedGroundPosOut = coord.m_vPosition;
	bIsInInteriorOut = coord.IsInInterior();
	bUsableIfOccludedOut = coord.IsUsableIfOccluded();

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPedGenerationGroundCoord
// PURPOSE :	Finds a new random location to create a ped at.
// PARAMETERS :	newPedGroundPosOut - the output generation coord if found.
//				popCtrlCentre - The centre point about which to base our ranges
//				on.
//				fAddRangeOutOfViewMin, fAddRangeOutOfViewMax,
//				fAddRangeInViewMin, fAddRangeInViewMax - min and max radii,
//				the ped will be added somewhere within these two boundaries.
//				Note: Try to keep at least 10 meters between the two otherwise
//				it may not work very well.
//				pedRadius - the radius to use to avoid ped to ped collisions.
//				doInFrustumTest -	Whether or not to check if the new location can
//					possibly be seen or not.
// RETURNS :	Whether or not we could get a valid generation coord.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::GetPedGenerationGroundCoord(
	Vector3& newPedGroundPosOut,
	bool& bNewPedGroundPosOutIsInInterior,
	bool popCtrlIsInInterior,
	float fAddRangeInViewMin,
	float UNUSED_PARAM(fAddRangeOutOfViewMin),
	float pedRadius,
	bool doInFrustumTest,
	s32& roomIdxOut,
												CInteriorInst*& pIntInstOut)
{
	bool goodPositionFound = false;
	Vector3 candidatePedGroundPos;
	bool bIsInInterior = false;
	bool bIsUsableIfOccluded = false;
	static const s32 maxTries = 1;
	s32 tries = 0;
	while(!goodPositionFound && (tries < maxTries))
	{
		++tries;

		// Try to get a candidate coordinate from the path server.
		if(!GetPedGenerationGroundCoordFromPathserver(candidatePedGroundPos, bIsInInterior, bIsUsableIfOccluded))
		{
			// Try with a new candidate coord since we couldn't get one from the
			// path server.
			continue;
		}

		// Make sure that a ped is actually placeable right now at the
		// candidate coord.
		const Vector3 candidatePedRootPos(candidatePedGroundPos.x, candidatePedGroundPos.y, candidatePedGroundPos.z + 1.0f );
		const bool	doInFrustumTestForNewPoint	= true;
		const bool	doVisibleToAnyPlayerTest	= doInFrustumTest && (ms_disableMPVisibilityTestTimer == 0);
		const bool	doObjectCollisionTest		= true;
		const bool	allowDeepInteriors			= popCtrlIsInInterior;
		const CEntity* pExcludedEntity			= NULL;

		if(!IsPedGenerationCoordCurrentlyValid(
			candidatePedRootPos,
			bIsInInterior,
			pedRadius,
			allowDeepInteriors,
			doInFrustumTestForNewPoint,
			fAddRangeInViewMin,
			doVisibleToAnyPlayerTest,
			doObjectCollisionTest,
			pExcludedEntity,
			NULL,
			roomIdxOut,
			pIntInstOut,
			true))
		{
			// Try with a new candidate coord since the current one can't
			// actually have a ped placed there right now.
			continue;
		}

		// We found a good position, so we can stop looking.
		goodPositionFound = true;
		break;
	}

	// Check if we found a good position.
	if(!goodPositionFound)
	{
		// Let the caller know that we could no find any valid
		// generation coords.
		return false;
	}
	else
	{
		// Set the location found.
		newPedGroundPosOut = candidatePedGroundPos;
		bNewPedGroundPosOutIsInInterior = bIsInInterior;

#if __DEV
		if(ms_displayCreationPosAttempts3D)
		{
			grcDebugDraw::Line(newPedGroundPosOut, newPedGroundPosOut + Vector3(0.0f,0.0f,20.0f), Color_blue);
		}
#endif

		// Let the caller know that we found a good generation coord.
		return true;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsPedGenerationCoordCurrentlyValid
// PURPOSE :	Checks if a coord is valid for generating a ped at right now.
//				It checks against other peds, cars, and objects.  It also
//				optionally checks if the max density per meter isn't exceeded.
// PARAMETERS :	candidatePedRootPos - the point to test
//				pedRadius - the radius to use to avoid ped to ped collisions.
// RETURNS :	Whether or not it is a valid generation coord.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::IsPedGenerationCoordCurrentlyValid(
	const Vector3& candidatePedRootPos,
	bool bIsInInterior,
	float pedRadius,
	bool allowDeepInteriors,
	bool doInFrustumTest,
	float fAddRangeInViewMin,
	bool doVisibleToAnyPlayerTest,
	bool doObjectCollisionTest,
	const CEntity* pExcludedEntity1,
	const CEntity* pExcludedEntity2,
	s32& roomIdxOut,
	CInteriorInst*& pIntInstOut,
	bool doMpTest)
{
	if(PointFallsWithinNonCreationArea(candidatePedRootPos))
	{
		return false;
	}

	if(CPathServer::IsCoordInPedSwitchedOffArea(candidatePedRootPos))
	{
		return false;
	}

	// Check if we should filter out items that are on screen.
	if(doInFrustumTest)
	{
		bool bOnScreen = IsCandidateInViewFrustum(candidatePedRootPos, pedRadius, fAddRangeInViewMin);
		if(bOnScreen)
		{
			return false;
		}
	}

	if(bIsInInterior)
	{
		// See if the interior around this point is loaded, if not
		// then don't use it.  Note, if collision isn't yet loaded
		// here then the probe will fail to find any interior data.
		s32	roomIdx = -1;
		CInteriorInst* pIntInst = NULL;
		CPortalTracker::ProbeForInterior(candidatePedRootPos, pIntInst, roomIdx, NULL, CPortalTracker::SHORT_PROBE_DIST);
		if (!pIntInst || roomIdx <= 0)
		{
			return false;
		}

		roomIdxOut = roomIdx;
		pIntInstOut = pIntInst;

		// Check if they are in a potentially unsee-able interior (such as a subway
		// station when above ground).
		// Only consider subways as deep interiors - some interiors have rooms with no portals close to the entrance
		// and this would stop peds spawning
		const bool bConsideredDeepInterior = pIntInst->IsSubwayMLO();
		if(!allowDeepInteriors && bConsideredDeepInterior)
		{
			if(pIntInstOut->GetNumExitPortalsInRoom(roomIdxOut) == 0)
			{
				return false;
			}
		}
	}

		// If this is a network game - see if there are any closer players with a higher peer ID, if
		// so, they take on responsibility for generating peds in this area
		if(NetworkInterface::IsGameInProgress() && doMpTest)
		{
			static const float PED_CREATION_PRIORITY_DISTANCE = 250.0f;

			const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
			u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(candidatePedRootPos, PED_CREATION_PRIORITY_DISTANCE, closestPlayers, false);

			for(u32 playerIndex = 0; playerIndex < numPlayersInRange; playerIndex++)
			{
				const netPlayer *player = closestPlayers[playerIndex];

				if(AssertVerify(player))
				{
					if (NetworkInterface::GetLocalPlayer()->GetRlGamerId() < player->GetRlGamerId())
					{
						return false;
					}
				}
			}
		}

	// Check if this position collides with any other objects or cars.
	static const int maxObjects = 10;
	CEntity* pKindaCollidingObjects[maxObjects];
	for(s32 i =0; i < maxObjects; ++i)
	{
		pKindaCollidingObjects[i] = NULL;
	}
	if(!CPedPlacement::IsPositionClearForPed(candidatePedRootPos, pedRadius, true, 2, NULL, true, true, doObjectCollisionTest))
	{
		// NOTE : Although this scope may be entered, the pKindaCollidingObjects array will always be empty
		// because it is not passed into CPedPlacement::IsPositionClearForPed() above.

		// Determine how many of the "collisions" were real.
		int numRealCollisions = 0;
		for(s32 i =0; i < maxObjects; ++i)
		{
			// Make sure the collision exists.
			if(!pKindaCollidingObjects[i])
			{
				continue;
			}

			// Make sure the collisions is with something other than
			// the than the excluded entities.
			if(pKindaCollidingObjects[i] == pExcludedEntity1 || pKindaCollidingObjects[i] == pExcludedEntity2)
			{
				continue;
			}

			// Make sure the collision isn't just with the bounding
			// box of an overhanging object.
			const CEntityBoundAI boundAI(*pKindaCollidingObjects[i], candidatePedRootPos.z, pedRadius, true);
			if(boundAI.LiesInside(candidatePedRootPos))
			{
				continue;
			}

			// Seems to be a real collision.
			++numRealCollisions;
		}

		if(numRealCollisions > 0)
		{
#if __DEV
			if(ms_displayCreationPosAttempts3D)
			{
				grcDebugDraw::Line(candidatePedRootPos, candidatePedRootPos + Vector3(0.0f,0.0f,20.0f), Color_purple);
			}
#endif
			// The candidate coord collides with
			// something (a car or object).
			return false;
		}
	}

	// Don't add peds if they are going to appear in front of other network players.
	if(	doVisibleToAnyPlayerTest &&
		NetworkInterface::IsGameInProgress() &&
		NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(candidatePedRootPos, pedRadius, ms_addRangeBaseInViewMin * g_LodScale.GetGlobalScale(), ms_addRangeBaseOutOfViewMin))
	{
#if __DEV
		if(ms_displayCreationPosAttempts3D)
		{
			grcDebugDraw::Line(candidatePedRootPos, candidatePedRootPos + Vector3(0.0f,0.0f,20.0f), Color_purple);
		}
#endif

        ms_mpVisibilityFailureCount++;

        static const u32 DISABLE_MP_VISIBILITY_CHECK_THRESHOLD = 200;

        if(ms_mpVisibilityFailureCount > DISABLE_MP_VISIBILITY_CHECK_THRESHOLD)
        {
            static const u32 DISABLE_MP_VISIBILITY_CHECK_PERIOD = 30000;

            ms_disableMPVisibilityTestTimer = DISABLE_MP_VISIBILITY_CHECK_PERIOD;
            ms_mpVisibilityFailureCount     = 0;
        }

		// The candidate coord would appear in front
		// of another player in the game.
		return false;
	}

	// We seem to have found a good candidate coord.
	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ChooseCivilianPedModelIndex
// PURPOSE :	To find a ped that is appropriate and has the smallest number
//				of occurrences at the moment.
// PARAMETERS :	bMustBeMale - Must the ped be a male civilian.
//				bMustBeFemale - Must the ped be a female civilian.
//				mustHaveClipSet - Must they have the given anim group (-1 means use any anim group).
//				AvoidThisPedModelIndex - Don't allow this ped model index to be returned.
//				bOnlyOnFoots - Makes sure they can't drive a car.
//				pAttractorScriptName - Make sure they can be created at the attractor if given (NULL means don't check this).
// RETURNS :	The pedModelIndex to use (-1 means we couldn't find one).
/////////////////////////////////////////////////////////////////////////////////
u32 CPedPopulation::ChooseCivilianPedModelIndex(ChooseCivilianPedModelIndexArgs& args)
{
	// Pick the one ped that is appropriate and has the smallest number of occurrences at the moment.

	if(args.m_PedModelVariationsOut)
	{
		*args.m_PedModelVariationsOut = NULL;
	}

	CLoadedModelGroup candidates = gPopStreaming.GetAppropriateLoadedPeds();
	if(args.m_AvoidNearbyModels)
	{
		candidates.RemoveModelsThatAreUsedNearby(args.m_CreationCoors);
	}

	if(args.m_SortByPosition)
	{
		Assertf(args.m_CreationCoors, "Trying to sort peds models by distance, but passed in a NULL distance pointer");
		candidates.SortPedsByDistance(*args.m_CreationCoors);
	}

	// Go over all the members starting and find the least used appropriate one.
	const s32 numCandidates = candidates.CountMembers();
	u32	leastUsedAppropriatePedModelIndex	= fwModelId::MI_INVALID;
	s32	leastUsedAppropriatePedUsageAmount	= args.m_IgnoreMaxUsage ? 999 : ms_maxPedsForEachPedTypeAvailableOnFoot;		// Don't use peds more than this number(on foot)
	for(s32 i = 0; i < numCandidates; ++i)
	{
		// Get the members model index(and assure it is loaded).
		const u32 pedModelIndex = candidates.GetMember(i);
		fwModelId pedModelId((strLocalIndex(pedModelIndex)));
		Assert(pedModelId.IsValid());
		Assert(CModelInfo::HaveAssetsLoaded(pedModelId));

		const CPedModelInfo* pPedModelInfo =(CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
		Assert(pPedModelInfo);

		if (!DoesModelMeetConditions(args, pPedModelInfo, pedModelIndex))
		{
			continue;
		}

		const u32 pedModelHash = pPedModelInfo->GetHashKey();

		// Make sure the AvoidThisPedModelIndex requirement parameter is met.
		// loop through the m_BlockedModelSetsArray and check if any of the model has been blocked
		bool bFoundBlockedModel = CScenarioManager::BlockedModelSetsArrayContainsModelHash(args.m_BlockedModelSetsArray, args.m_NumBlockedModelSets, pedModelHash);
		// if the model was blocked
		if (bFoundBlockedModel)
		{
			continue;
		}

		// If we have a required model set, reject this model if it's not in it.
		const CAmbientModelVariations* pVariations = NULL;
		if(args.m_RequiredModels)
		{
			const CAmbientModelVariations** pVariationsPtr = args.m_PedModelVariationsOut ? &pVariations : NULL;
			if(!args.m_RequiredModels->GetContainsModel(pedModelHash, pVariationsPtr))
			{
				continue;
			}
		}

		// Make sure it is not a model index that is to be streamed out.
		if(CModelInfo::GetAssetsAreDeletable(pedModelId))
		{
			continue;
		}

		// The level designers can suppress specific models
		if(CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(pedModelIndex))
		{
			continue;
		}

		const s32 usageAmount = pPedModelInfo->GetNumTimesOnFoot();
		if(usageAmount >= leastUsedAppropriatePedUsageAmount)
		{
			// This wouldn't be the least used one we've seen so far, so there is no need
			// to do the flag checks (which are potentially more expensive).
			continue;
		}

		if(args.m_MayRequireBikeHelmet)
		{
			// Note: this should match the helmet conditions in AddPedInCar().
			if(!(pPedModelInfo->HasHelmetProp() || pPedModelInfo->GetCanRideBikeWithNoHelmet()))
			{
				continue;
			}
		}

		if(args.m_RequiredPopGroupFlags)
		{
			bool found = false;
			const CPopGroupList& popGroups = CPopCycle::GetPopGroups();
			const int popGroupCount = popGroups.GetPedCount();

			if (args.m_SelectionPopGroup < popGroupCount)
			{
				// Now, check if this model is in this group.
				if(popGroups.IsPedIndexMember(args.m_SelectionPopGroup, pedModelIndex))
				{
					if (args.m_PedModelVariationsOut)
					{
						*args.m_PedModelVariationsOut = CPopCycle::GetPopGroups().GetPedGroup(args.m_SelectionPopGroup).FindVariations(pedModelIndex);
					}
					found = true;
				}
			}
			else
			{
				// At this point, we don't know from which population group this model index
				// came from, so in order to determine if the flags are matched, we have to
				// look through the population groups.
				for(int popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
				{
					if(CPopCycle::GetCurrentPedGroupPercentage(popGroupIndex) == 0)
					{
						// If this group shouldn't be part of the current population, don't check
						// the flags and look for membership. This is both an optimization (avoiding
						// the linear search in IsPedIndexMember()) but is also necessary in case the
						// same model is used in groups with the proper flags set, but which aren't in
						// the current population.
						continue;
					}

					if((popGroups.GetPedGroup(popGroupIndex).GetFlags() & args.m_RequiredPopGroupFlags) != args.m_RequiredPopGroupFlags)
					{
						// The flags don't match for this population group. Thus, we don't care whether
						// this model is present in the group or not. We do this flag check first,
						// as it's likely to be more expensive to search for the model than to check the flags.
						continue;
					}

					// Now, check if this model is in this group.
					if(popGroups.IsPedIndexMember(popGroupIndex, pedModelIndex))
					{
						if (args.m_PedModelVariationsOut)
						{
							*args.m_PedModelVariationsOut = CPopCycle::GetPopGroups().GetPedGroup(popGroupIndex).FindVariations(pedModelIndex);
						}

						found = true;
						break;			// No need to keep looking.
					}
				}
			}
			
			if(!found)
			{
				// We didn't find any group with the required flags, in which this model was a member,
				// so we can't use it.
				continue;
			}
		}

		if (args.m_ConditionalAnims)
		{
			//Create the condition data.
			CScenarioCondition::sScenarioConditionData conditionData;
			conditionData.m_ModelId = pedModelId;

			// Check conditions that rely only on the ped's model index.
			if (!args.m_ConditionalAnims->CheckForMatchingPopulationConditionalAnims(conditionData))
			{
				continue;
			}
		}

		// Store least frequent one so far.
		leastUsedAppropriatePedUsageAmount	= usageAmount;
		leastUsedAppropriatePedModelIndex	= pedModelIndex;

		if(args.m_PedModelVariationsOut && !*args.m_PedModelVariationsOut)
		{
			*args.m_PedModelVariationsOut = pVariations;
		}

		if(args.m_SortByPosition)
		{			
			// we're sorting by position, so use the first ped we find - it'll be the furthest away
			break;			
		}
	}

	// if GroupGangMembers is set we force the gang membership if we happen to randomly choose a gang member.
	// if the same args will then be used again the next time we'll pick another gang member
	// maybe we should enforce the same gang too? we don't tend to have several ambient gangs in the same place though
	if (args.m_GroupGangMembers && leastUsedAppropriatePedModelIndex != fwModelId::MI_INVALID)
	{
		const CPedModelInfo* pmi = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(leastUsedAppropriatePedModelIndex)));
		Assert(pmi);
		if (pmi->GetIsGang() || pmi->GetPersonalitySettings().GetIsGang())
			args.m_RequiredGangMembership = GANG_MEMBER;
        else
            args.m_RequiredGangMembership = NOT_GANG_MEMBER;
	}

	// Return the ped model index.
	return leastUsedAppropriatePedModelIndex;
}

bool CPedPopulation::DoesModelMeetConditions( const ChooseCivilianPedModelIndexArgs &args, const CPedModelInfo* pPedModelInfo, const u32 pedModelIndex )
{
	// Make sure the maleness requirement parameter is met.
	if(args.m_RequiredGender == GENDER_MALE && !pPedModelInfo->IsMale())
	{
		return false;
	}

	// Make sure the femaleness requirement parameter is met.
	if(args.m_RequiredGender == GENDER_FEMALE && pPedModelInfo->IsMale())
	{
		return false;
	}

	if(args.m_ProhibitBulkyItems && pPedModelInfo->GetOnlyBulkyItemVariations())
	{
		return false;
	}

	// Make sure the gang requirement is met
	if(args.m_RequiredGangMembership == NOT_GANG_MEMBER && (pPedModelInfo->GetIsGang() || pPedModelInfo->GetPersonalitySettings().GetIsGang()))
	{
		return false;
	}

	// Make sure the gang requirement is met
	if(args.m_RequiredGangMembership == GANG_MEMBER && !pPedModelInfo->GetIsGang() && !pPedModelInfo->GetPersonalitySettings().GetIsGang())
	{
		return false;
	}

	if(args.m_SpeechContextRequirement != 0)
	{
		const u32 possibleTopics = static_cast<u32>(g_SpeechManager.GetPossibleConversationTopicsForPvg(pPedModelInfo->GetPedVoiceGroup()));
		if((args.m_SpeechContextRequirement & possibleTopics) == 0)
		{
			return false;
		}
	}

	// Make sure the mustHaveClipSet requirement parameter is met.
	if(args.m_MustHaveClipSet != CLIP_SET_ID_INVALID &&
		args.m_MustHaveClipSet != pPedModelInfo->GetMovementClipSet())
	{
		return false;
	}

	// Check if the ped can fly
	if (!args.m_AllowFlyingPeds)
	{
		if (CWildlifeManager::GetInstance().IsFlyingModel(pedModelIndex))
		{
			return false;
		}
	}

	// Check if the ped can swim.
	if (!args.m_AllowSwimmingPeds)
	{
		if (CWildlifeManager::GetInstance().IsAquaticModel(pedModelIndex))
		{
			return false;
		}
	}

	return true;
}


bool CPedPopulation::CheckPedModelGenderAndBulkyItems(fwModelId pedModelId, Gender requiredGender, bool bProhibitBulkyItems)
{
	if(!pedModelId.IsValid())
	{
		return requiredGender == GENDER_DONTCARE;	// Not sure
	}

	const CPedModelInfo* pPedModelInfo =(CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	Assert(pPedModelInfo);

	// Make sure the maleness requirement is met.
	if(requiredGender == GENDER_MALE && !pPedModelInfo->IsMale())
	{
		return false;
	}

	// Make sure the femaleness requirement is met.
	if(requiredGender == GENDER_FEMALE && pPedModelInfo->IsMale())
	{
		return false;
	}

	if (bProhibitBulkyItems && pPedModelInfo->GetOnlyBulkyItemVariations())
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PointIsWithinCreationZones
// PURPOSE :	To check if adding a ped at the given position is valid.
// PARAMETERS :	candidatePedRootPos -  the position to check.
//				popCtrlCentre - The centre point about which to base our ranges
//				on.
//				fAddRangeOutOfViewMin, fAddRangeOutOfViewMax,
//				fAddRangeInViewMin, fAddRangeInViewMax - min and max radii,
//				the ped will be added somewhere within these two boundaries.
//				Note: Try to keep at least 10 meters between the two otherwise
//				it may not work very well.
//				doInFrustumTest -	Whether or not to check if the new location can
//					possibly be seen or not.
//				use2dDistCheck -    Use a 2d distance XY check instead of a full 3d check
// RETURNS :	Whether or not it is.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::PointIsWithinCreationZones(	const Vector3& candidatePedRootPos,
			const Vector3& popCtrlCentre,
			const float fAddRangeOutOfViewMin,
			const float fAddRangeOutOfViewMax,
			const float fAddRangeInViewMin,
			const float fAddRangeInViewMax,
			const float fAddRangeFrustumExtra,
			const bool doInFrustumTest,
			const bool use2dDistCheck)
{
	if(PointFallsWithinNonCreationArea(candidatePedRootPos))
	{
		return false;
	}

	// Find the distance of the candidate point from the
	// ped generation origin.
	const Vector3	vDiff	= candidatePedRootPos - popCtrlCentre;
	float tempdist = 0.0f;
	if (use2dDistCheck)
	{
		// eg. peds on street below will be created when we're on top of a tall building or
		//  peds below flying plane/heli will be created as you fly over.
		tempdist = vDiff.XYMag2();
	}
	else
	{
		tempdist = vDiff.Mag2();
	}
	const float		dist2	= tempdist;

	// Make sure the candidate point isn't further than our
	// maximum generation distance.
	if(dist2 > (fAddRangeInViewMax * fAddRangeInViewMax))
	{
		return false;
	}

	// Make sure the candidate point is on at least one of two the generation bands.
	// Or in between the two bands (as the m_fKeyHoleSideThickness might make it valid).
	const bool onOutOfViewBand		= (	(dist2 >= (fAddRangeOutOfViewMin * fAddRangeOutOfViewMin))	&&
										(dist2 <= (fAddRangeOutOfViewMax * fAddRangeOutOfViewMax)));
	const bool onInViewBand			= (	(dist2 >= (fAddRangeInViewMin * fAddRangeInViewMin))	&&
										(dist2 <= (fAddRangeInViewMax * fAddRangeInViewMax)));
	const bool inBetweenViewBands	= (	(dist2 >= (fAddRangeOutOfViewMax * fAddRangeOutOfViewMax))	&&
										(dist2 <= (fAddRangeInViewMin * fAddRangeInViewMin)));
	if(!onOutOfViewBand && !onInViewBand && !((fAddRangeFrustumExtra > 0.0f) && inBetweenViewBands))
	{
		return false;
	}

	// Determine if the candidate point is visible or not.
	// This is used by both the outOfViewBand and the
	// inViewBand for filtering.
	if(!doInFrustumTest)
	{
		// It was on one or between the bands, so allow it's creation.
		return true;
	}
	else
	{
		// Check if they are visible or not.
		const float fTestRadius = 2.0f;
		const float fTestRadiusExtra = 2.0f + fAddRangeFrustumExtra;

		// Filter out of view band items that are on screen.
		bool bOnScreen = IsCandidateInViewFrustum(candidatePedRootPos, fTestRadius, fAddRangeInViewMax);
		if(onOutOfViewBand && bOnScreen && !onInViewBand)
		{
			return false;
		}

		// Make sure the candidate point is in a valid region.
		bool bOnScreenExtra = IsCandidateInViewFrustum(candidatePedRootPos, fTestRadiusExtra, fAddRangeInViewMax);
		if( (onInViewBand && !bOnScreenExtra && !onOutOfViewBand) ||
			(inBetweenViewBands && !(bOnScreenExtra && !bOnScreen)))
		{
			return false;
		}

		// It appears to be fine.
		return true;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::IsCandidateInViewFrustum( const Vector3& candidatePedRootPos, float fTestRadius, float maxDistToCheck, float minDistToCheck )
{
	bool bOnScreen = false;

#if __DEV
	// In DEV builds, if desired, make this use the game cam for the visibility
	// test even when using the debug cam.  That will make debugging the behavior
	// of the system much easier.
	const bool isUsingDebugCam = camInterface::GetDebugDirector().IsFreeCamActive();
	if(isUsingDebugCam && !ms_movePopulationControlWithDebugCam)
	{
		// Base the population control off the game cam (which isn't currently the one for the main view port).
		const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
		const Vector3& camPos = gameplayDirector.GetFrame().GetPosition();
		const float fDistSqr = (camPos - candidatePedRootPos).Mag2();
		if(fDistSqr > (maxDistToCheck * maxDistToCheck))
		{
			return false;
		}
		if(fDistSqr < (minDistToCheck * minDistToCheck))
		{
			return false;
		}

		bOnScreen = gameplayDirector.IsSphereVisible(candidatePedRootPos, fTestRadius);
	}
	else
#endif // __DEV
	{
		// Base the population control off the current (game/scripted/cutscene/etc) cam.
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(gameViewport)
		{
			const grcViewport& grcVp = gameViewport->GetGrcViewport();
			const Vector3 camPos = VEC3V_TO_VECTOR3(grcVp.GetCameraPosition());
			const float fDistSqr = (camPos - candidatePedRootPos).Mag2();
			if(fDistSqr  > (maxDistToCheck * maxDistToCheck))
			{
				return false;
			}
			if(fDistSqr < (minDistToCheck * minDistToCheck))
			{
				return false;
			}

			Vec4V sphere = VECTOR3_TO_VEC4V(candidatePedRootPos);
			sphere.SetWf(fTestRadius);
			bOnScreen = camInterface::IsSphereVisibleInGameViewport(sphere);
		}
	}

	return bOnScreen;
}



/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
PF_PAGE(GTAPopulation, "GTA Population");
PF_GROUP(ScenarioSpawning);
PF_LINK(GTAPopulation, ScenarioSpawning);

PF_TIMER(ScenarioTotal, ScenarioSpawning);
PF_TIMER(CollisionChecks, ScenarioSpawning);
PF_TIMER(PedSpawning, ScenarioSpawning);
PF_TIMER(CreateScenarioPedTotal, ScenarioSpawning);
PF_TIMER(CreateScenarioPedPreSpawnSetup, ScenarioSpawning);
PF_TIMER(CreateScenarioPedPostSpawnSetup, ScenarioSpawning);
PF_TIMER(CreateScenarioPedZCoord, ScenarioSpawning);
PF_TIMER(CreateScenarioPedAddPed, ScenarioSpawning);

static float CLOSEST_ALLOWED_SCENARIO_SPAWN_DIST = 1.0f;
static float MIN_DIST_FROM_SCENARIO_SPAWN_TO_DEAD_PED = 1.75f;

#define MAX_COLLIDED_OBJECTS 6
static float ENTITY_SPAWN_UP_LIMIT = 0.9f;

float CPedPopulation::GetClosestAllowedScenarioSpawnDist()
{
	return CLOSEST_ALLOWED_SCENARIO_SPAWN_DIST;
}

float CPedPopulation::GetMinDistFromScenarioSpawnToDeadPed()
{
	return MIN_DIST_FROM_SCENARIO_SPAWN_TO_DEAD_PED;
}

int CPedPopulation::SelectPointsRealType( const CScenarioPoint& spoint, const s32 iSpawnOptions, int& indexWithinType_Out) 
{
	// For virtual scenarios, we need to decide which one of the possible real scenario types
	// we should use for this point. To do this, we build arrays with information about possible
	// choices, and then we pick one.
	const int virtualType = spoint.GetScenarioTypeVirtualOrReal();
	const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(virtualType);
	taskAssert(numRealTypes <= CScenarioInfoManager::kMaxRealScenarioTypesPerPoint);
	atRangeArray<int, CScenarioInfoManager::kMaxRealScenarioTypesPerPoint> realTypesToPickFrom;
	atRangeArray<int, CScenarioInfoManager::kMaxRealScenarioTypesPerPoint> indexWithinTypeGroupArray;
	atRangeArray<float, CScenarioInfoManager::kMaxRealScenarioTypesPerPoint> probToPick;
	int numRealTypesToPickFrom = 0;
	float probSum = 0.0f;

	// Loop over all the real scenario types this point's virtual type can correspond to.
	for(int j = 0; j < numRealTypes; j++)
	{
		if(!CScenarioManager::CheckScenarioPointEnabledByType(spoint, j))
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPointDisabled, VEC3V_TO_VECTOR3(spoint.GetPosition()), &spoint));
			continue;
		}

		// Make sure that if this has specific spawn times that they are respected.
		if( !(iSpawnOptions & SF_ignoreSpawnTimes) && !CScenarioManager::CheckScenarioPointTimesAndProbability(spoint, true, true, j BANK_ONLY(, true)))
		{
			continue;
		}

		const float prob = SCENARIOINFOMGR.GetRealScenarioTypeProbability(virtualType, j);
		realTypesToPickFrom[numRealTypesToPickFrom] = SCENARIOINFOMGR.GetRealScenarioType(virtualType, j);
		probToPick[numRealTypesToPickFrom] = prob;
		indexWithinTypeGroupArray[numRealTypesToPickFrom] = j;
		probSum += prob;
		numRealTypesToPickFrom++;
	}

	// TODO: Think about using/merging with sPickRealScenarioTypeFromGroup()
	int chosenWithinTypeGroupIndex = -1;
	if(numRealTypesToPickFrom)
	{
		if(numRealTypesToPickFrom == 1)
		{
			chosenWithinTypeGroupIndex = 0;
		}
		else
		{
			float p = g_ReplayRand.GetFloat()*probSum;
			chosenWithinTypeGroupIndex = numRealTypesToPickFrom - 1;
			for(int j = 0; j < numRealTypesToPickFrom - 1; j++)
			{
				if(p < probToPick[j])
				{
					chosenWithinTypeGroupIndex = j;
					break;
				}
				p -= probToPick[j];
			}
		}
	}

	if(chosenWithinTypeGroupIndex < 0)
	{
		indexWithinType_Out = -1;
		return -1;
	}

	indexWithinType_Out = indexWithinTypeGroupArray[chosenWithinTypeGroupIndex];
	return realTypesToPickFrom[chosenWithinTypeGroupIndex];
}

bool CPedPopulation::SP_SpawnValidityCheck_General(const CScenarioPoint& scenarioPt, bool allowClusters)
{
	if(scenarioPt.IsFlagSet(CScenarioPointFlags::NoSpawn))
	{
		return false;
	}

	// Clustered points are ignored by this ... they will be handled by another update.
	if(!allowClusters && scenarioPt.IsInCluster())
	{
		return false;
	}

	// Check if this point has spawn history. This used to prevent spawning of another ped
	// with the original one still within a specific range.
	if(scenarioPt.HasHistory())
	{
		return false;
	}

	// Already in use?
	if(CPedPopulation::IsEffectInUse(scenarioPt))
	{
		BANK_ONLY(CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPointInUse, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
		return false;
	}

	if(!CScenarioManager::CheckScenarioPointEnabledByGroupAndAvailability(scenarioPt))
	{
		BANK_ONLY(CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPointDisabled, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
		return false;
	}

	if(!CScenarioManager::CheckScenarioPointMapData(scenarioPt))
	{
		BANK_ONLY(CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPointRequiresImap, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
		return false;
	}

	//find out if my chain is in use
	if (scenarioPt.IsChained() && scenarioPt.IsUsedChainFull())
	{
		BANK_ONLY(CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPointInUse, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
		return false;
	}

	return true;
}

bool CPedPopulation::SP_SpawnValidityCheck_ScenarioType(const CScenarioPoint& scenarioPt, const s32 UNUSED_PARAM(scenarioTypeReal))
{
	const int virtualType = scenarioPt.GetScenarioTypeVirtualOrReal();

	// If this is a vehicle scenario, bail out right now, since we're trying to generate peds.
	// This might save some unnecessary processing (we would otherwise decide to bail in
	// CScenarioManager::CheckScenarioPopulationCount()), but it also avoids registering
	// them as spawn failures.
	if(CScenarioManager::IsVehicleScenarioType(virtualType))
	{
		return false;
	}

	// Never spawn peds at a LookAt scenario.
	if (CScenarioManager::IsLookAtScenarioType(virtualType))
	{
		return false;
	}

	return true;
}

bool CPedPopulation::SP_SpawnValidityCheck_EntityAndTrainAttachementFind(const CScenarioPoint& scenarioPt, CTrain*& pAttachedTrain_OUT)
{
	CEntity* pAttachedEntity = scenarioPt.GetEntity();
	if ( pAttachedEntity )
	{
		if ( (!scenarioPt.IsEntityOverridePoint() && !pAttachedEntity->m_nFlags.bWillSpawnPeds))
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioObjectDoesntSpawnPeds, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
			return false;
		}

		// If the object is broken, don't spawn people on it.
		if( pAttachedEntity->GetFragInst() && pAttachedEntity->GetFragInst()->GetPartBroken())
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioObjectBroken, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
			return false;
		}

		// Ignore objects that have been broken up
		if( pAttachedEntity->GetIsTypeObject() && (((CObject*)pAttachedEntity)->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE))
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioObjectBroken, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
			return false;
		}

		// Ignore objects that have been uprooted
		if ( pAttachedEntity->GetIsTypeObject() && (static_cast<CObject *>(pAttachedEntity)->m_nObjectFlags.bHasBeenUprooted) )
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioObjectUprooted, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
			return false;				
		}

		//Grab the attached train.
		if(pAttachedEntity->GetIsTypeVehicle())
		{
			//Grab the attached vehicle.
			CVehicle* pAttachedVehicle = static_cast<CVehicle *>(pAttachedEntity);
			if(pAttachedVehicle->InheritsFromTrain())
			{
				pAttachedTrain_OUT = static_cast<CTrain *>(pAttachedVehicle);

				// We probably never want peds to spawn by themselves on the train - should
				// be handled by the pretend occupant management code.
				//
				//	//Ensure the train has empty passenger slots.
				//	if(pAttachedTrain_OUT && pAttachedTrain_OUT->GetEmptyPassengerSlots() == 0)
				//	{
				//		return false;
				//	}
				//

				return false;
			}
		}
	}

	return true;
}

bool CPedPopulation::SP_SpawnValidityCheck_Interior(const CScenarioPoint& scenarioPt, const s32 iSpawnOptions)
{
	// If OnlySpawnInSameInterior is set, reject this point if it's not in the same interior as the player.
	const unsigned int interiorId = scenarioPt.GetInterior();
	if(scenarioPt.IsFlagSet(CScenarioPointFlags::OnlySpawnInSameInterior) && CScenarioManager::GetRespectSpawnInSameInteriorFlag())
	{
		// Not entirely sure, should we use the player or the camera interior here? Maybe both?
		CPed *pLocalPlayer = CGameWorld::FindLocalPlayer();
		if(pLocalPlayer)
		{
			u32 playerInteriorNameHash = pLocalPlayer->GetPortalTracker()->GetInteriorNameHash();
			u32 scenarioInteriorNameHash = 0;
			if(interiorId != CScenarioPointManager::kNoInterior)
			{
				scenarioInteriorNameHash = SCENARIOPOINTMGR.GetInteriorNameHash(interiorId - 1);
			}					
			if(playerInteriorNameHash != scenarioInteriorNameHash)
			{
				BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioNotInSameInterior, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
				return false;
			}
		}
	}

	CEntity* pAttachedEntity = scenarioPt.GetEntity();

	// Only points that are not high priority can get skipped due to being outdoors when the SF_interiorAndHighPriOnly flag is set.
	bool allowSpawnInExterior = (scenarioPt.IsHighPriority() || !(iSpawnOptions & SF_interiorAndHighPriOnly));

	// If SF_dontSpawnExterior is set, we don't allow spawning in exteriors even if we are a high priority point.
	if(iSpawnOptions & SF_dontSpawnExterior)
	{
		allowSpawnInExterior = false;
	}

	if(!allowSpawnInExterior)
	{
		// Make sure that if this interior only spawn point that it is respected.
		if(pAttachedEntity && !pAttachedEntity->GetIsInInterior())
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioInsideFail, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
			return false;
		}

		// Make sure that if we are only supposed to spawn in interiors that it is respected.
		if(!pAttachedEntity)
		{
			if(scenarioPt.IsInteriorState(CScenarioPoint::IS_Outside))
			{
				BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioOutsideFail, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
				return false;
			}

			// The code above based on IsInteriorState() seems unreliable, we are often IS_Unknown
			// at this point. Would really like to get rid of that interior state thing completely.
			// Anyway, without this, I was still seeing peds spawn outside with the exterior density
			// set to 0.
			if(interiorId == CScenarioPointManager::kNoInterior)
			{
				BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioOutsideFail, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
				return false;
			}
		}
	}

	// Make sure that if we are only supposed to spawn in exteriors that it is respected.
	if(pAttachedEntity && pAttachedEntity->GetIsInInterior() && (iSpawnOptions & SF_dontSpawnInterior) )
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioOutsideFail, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
		return false;
	}

	if(!pAttachedEntity && !scenarioPt.IsInteriorState(CScenarioPoint::IS_Outside) && (iSpawnOptions & SF_dontSpawnInterior) )
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioOutsideFail, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
		return false;
	}

	// Check if the point is in an interior, and if so, don't let it spawn if that interior
	// isn't streamed in yet.
	if(interiorId != CScenarioPointManager::kNoInterior)
	{
		if(!SCENARIOPOINTMGR.IsInteriorStreamedIn(interiorId - 1))
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioInteriorNotStreamedIn, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt));
			return false;
		}
	}
	return true;
}

bool CPedPopulation::SP_SpawnValidityCheck_ScenarioInfo(const CScenarioPoint& SCENARIO_DEBUG_ONLY(scenarioPt), const s32 SCENARIO_DEBUG_ONLY(scenarioTypeReal))
{
#if SCENARIO_DEBUG
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioTypeReal);

	// Debug functionality for disabling certain scenarios, and ignoring that setting globally from within Rag
	if ( CScenarioDebug::ms_bDisableScenariosWithNoClipData && !pScenarioInfo->HasAnimationData())
	{
		ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioHasNoClipData, VEC3V_TO_VECTOR3(scenarioPt.GetPosition()), &scenarioPt);
		return false;
	}
#endif // SCENARIO_DEBUG
	return true;
}

CPedPopulation::SP_SpawnValidityCheck_PositionParams::SP_SpawnValidityCheck_PositionParams()
: fAddRangeOutOfViewMin(ms_addRangeBaseOutOfViewMin)
, fAddRangeOutOfViewMax(ms_addRangeBaseOutOfViewMax)
, fAddRangeInViewMin(ms_addRangeBaseInViewMin)
, fAddRangeInViewMax(ms_addRangeBaseInViewMax)
, fAddRangeFrustumExtra(ms_addRangeBaseFrustumExtra)
, faddRangeExtendedScenario(ms_addRangeExtendedScenarioEffective)
, fMaxRangeShortScenario(ms_maxRangeShortRangeScenario)
, fMaxRangeShortAndHighPriorityScenario(ms_maxRangeShortRangeAndHighPriorityScenario)
, fMinRangeShortScenarioInView(ms_minRangeShortRangeScenarioInView)
, fMinRangeShortScenarioOutOfView(ms_minRangeShortRangeScenarioOutOfView)
, fRemovalRangeScale(1.0f)
, fRemovalRateScale(1.0f)
, bInAnInterior(false)
, doInFrustumTest(false)
{
	popCtrlCentre.Zero();	
}

bool CPedPopulation::SP_SpawnValidityCheck_Position(const CScenarioPoint& scenarioPt, const s32 scenarioTypeReal, const SP_SpawnValidityCheck_PositionParams& params)
{
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioTypeReal);

	if (NetworkInterface::IsGameInProgress())
	{
		const Vector3 scenarioPos = VEC3V_TO_VECTOR3(scenarioPt.GetPosition());

		bool bControllingScenarioPosOverNetwork = false;
		if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnOnConcealedPlayerMachines) && NetworkInterface::AreThereAnyConcealedPlayers())
		{
			bControllingScenarioPosOverNetwork = CNetwork::IsConcealedPlayerTurn();
		}
		else
		{
			bControllingScenarioPosOverNetwork = NetworkInterface::IsPosLocallyControlled(scenarioPos);
		}

		if (!bControllingScenarioPosOverNetwork)
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPosNotLocallyControlled, scenarioPos, &scenarioPt));
			// Disabled, too much spam (B* 1322338):
			//	popDebugf2("Skipping scenario ped spawn, scenario position (%.2f, %.2f, %.2f) is not locally controlled", scenarioPos.x, scenarioPos.y, scenarioPos.z);
			return false;
		}
	}

	bool use2dDistCheck = false;
	// Get the distance from the player to the entity.
	const Vector3 scenarioPos = VEC3V_TO_VECTOR3(scenarioPt.GetPosition());
	CPed* player = FindPlayerPed();
	if (player)
	{
		use2dDistCheck = ShouldUse2dDistCheck(player);

		ScalarV distSqrd;
		if (use2dDistCheck)
		{
			// eg. peds on street below will be created when we're on top of a tall building or
			//  peds below flying plane/heli will be created as you fly over. 
			Vec2V sp_pos = scenarioPt.GetPosition().GetXY();
			if(Likely(!ms_useTempPopControlSphereThisFrame))
			{
				Vec2V pl_pos = player->GetTransform().GetPosition().GetXY();
				distSqrd = DistSquared(sp_pos, pl_pos);
			}
			else
			{					
				distSqrd = DistSquared(sp_pos, VECTOR3_TO_VEC3V(params.popCtrlCentre).GetXY());
			}
			
		}
		else
		{
			if(Likely(!ms_useTempPopControlSphereThisFrame))
			{					
				distSqrd = DistSquared(scenarioPt.GetPosition(), player->GetTransform().GetPosition());
			}
			else
			{					
				distSqrd = DistSquared(scenarioPt.GetPosition(), VECTOR3_TO_VEC3V(params.popCtrlCentre));
			}			
		}

		ScalarV thresholdDistSqV(params.fAddRangeInViewMax * params.fAddRangeInViewMax);

		// Allow extended range points to spawn beyond fAddRangeInViewMax.
		if(scenarioPt.HasExtendedRange())
		{
			const ScalarV extRange = LoadScalar32IntoScalarV(params.faddRangeExtendedScenario);
			const ScalarV extRangeSq = Scale(extRange, extRange);
			thresholdDistSqV = Max(thresholdDistSqV, extRangeSq);
		}

		if (IsGreaterThanAll(distSqrd, thresholdDistSqV))
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioPointTooFarAway, scenarioPos, &scenarioPt));
			return false;
		}
	}

	CEntity* pAttachedEntity = scenarioPt.GetEntity();
	const bool	bInterior		= (pAttachedEntity && pAttachedEntity->GetIsInInterior()) || scenarioPt.IsInteriorState(CScenarioPoint::IS_Inside) || (NetworkInterface::IsGameInProgress() && scenarioPt.GetInterior()!=CScenarioPointManager::kNoInterior);
	float		fInViewMin		= params.fAddRangeInViewMin;
	float		fInViewMax		= params.fAddRangeInViewMax;
	float		fOutViewMin		= params.fAddRangeOutOfViewMin;
	float		fOutViewMax		= params.fAddRangeOutOfViewMax;
	
	if(scenarioPt.HasExtendedRange())
	{
		fInViewMax = Max(params.faddRangeExtendedScenario, fInViewMax);
	}
	else if (scenarioPt.HasShortRange())
	{
		float fMaxRange = (scenarioPt.IsHighPriority()) ? params.fMaxRangeShortAndHighPriorityScenario : params.fMaxRangeShortScenario;
		fInViewMax = Min(fMaxRange, fInViewMax);
		fInViewMin = Min(params.fMinRangeShortScenarioInView, fInViewMin);
		fOutViewMin = Min(params.fMinRangeShortScenarioOutOfView, fOutViewMin);
		fOutViewMax = Min(fMaxRange, fOutViewMax);
	}

	if(params.doInFrustumTest)
	{
		static float INTERIOR_IN_VIEW_MIN = 20.0f;
		static float INTERIOR_OUT_VIEW_MIN = 20.0f;
		if(bInterior)
		{
			fInViewMin = INTERIOR_IN_VIEW_MIN;
			fOutViewMin = INTERIOR_OUT_VIEW_MIN;
		}
	}

	bool bShouldIgnoreSpawnRestrictions = player->GetPedResetFlag(CPED_RESET_FLAG_PlayerIgnoresScenarioSpawnRestrictions) && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::AllowPlayerIgnoreSpawnRestrictions);
	if (pScenarioInfo && !pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreCreationZones) && !bShouldIgnoreSpawnRestrictions &&
		!CPedPopulation::PointIsWithinCreationZones(scenarioPos, params.popCtrlCentre, fOutViewMin, fOutViewMax, fInViewMin, fInViewMax, params.fAddRangeFrustumExtra, params.doInFrustumTest, use2dDistCheck))
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_spawnPointOutsideCreationZone, scenarioPos, &scenarioPt));
		return false;
	}

	if(CScenarioBlockingAreas::IsPointInsideBlockingAreas(scenarioPos, true, false) || CPathServer::GetAmbientPedGen().IsInPedGenBlockedArea(scenarioPos))
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_spawnPointInBlockedArea, scenarioPos, &scenarioPt));
		return false;
	}

	if( NetworkInterface::IsGameInProgress() && NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(scenarioPos, PED_HUMAN_RADIUS, params.fAddRangeInViewMin, params.fAddRangeOutOfViewMin) )
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_visibletoMpPlayer, scenarioPos, &scenarioPt));
		return false;
	}

	return true;
}

bool CPedPopulation::ShouldUse2dDistCheck( const CPed* ped )
{
	//If the ped is flying we should use the 2d checks for populating the world under the ped.
	const CVehicle* veh = ped->GetVehiclePedInside();
	if ( ped->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) ||
		(veh && (veh->GetIsAircraft()))
		)
	{
		return true;	
	}	

	return false;
}

CPedPopulation::GetScenarioPointInAreaArgs::GetScenarioPointInAreaArgs(Vec3V_In areaCentre, float maxRange)
		: m_AreaCentre(areaCentre)
		, m_PedOrDummyToUsePoint(NULL)
		, m_Types(NULL)
		, m_MaxRange(maxRange)
		, m_NumTypes(0)
		, m_CheckPopulation(true)
		, m_EntitySPsOnly(false)
		, m_MustBeFree(true)
		, m_Random(false)
		, m_MustBeAttachedToTrain(false)
{
}


/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
// Generates peds at the spawnpoints 2deffects scattered about the map using the
// scenario manager
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::GetScenarioPointInArea(	const Vector3& areaCentre,
												float fMaxRange,
												s32 iNumTypes,
												s32* aTypes,
												CScenarioPoint** ppScenarioPoint,
												bool bMustBeFree,
												CDynamicEntity* pPedOrDummyToUsePoint,
												const bool bRandom,
												const bool bEntitySPsOnly)
{
	GetScenarioPointInAreaArgs args(RCC_VEC3V(areaCentre), fMaxRange);
	args.m_NumTypes = iNumTypes;
	args.m_Types = aTypes;
	args.m_MustBeFree = bMustBeFree;
	args.m_PedOrDummyToUsePoint = pPedOrDummyToUsePoint;
	args.m_Random = bRandom;
	args.m_EntitySPsOnly = bEntitySPsOnly;
	return GetScenarioPointInArea(args, ppScenarioPoint);
}


bool CPedPopulation::GetScenarioPointInArea( const GetScenarioPointInAreaArgs& args, CScenarioPoint** ppScenarioPoint)
{
	*ppScenarioPoint = NULL;
	float fClosestDistanceSq = 0.0f;
	CScenarioPoint* areaPoints[CScenarioPointManager::kMaxPtsInArea];
	const s32 effectCount = SCENARIOPOINTMGR.FindPointsInArea(args.m_AreaCentre, ScalarV(args.m_MaxRange), areaPoints, NELEM(areaPoints));

	CScenarioPoint** apValidScenarioPoints = Alloca(CScenarioPoint*, effectCount);
	s32 iNumStoredPnts = 0;

	bool bUsedByClonePed = false;

	const CPed*	pPed = NULL;
	if(NetworkInterface::IsGameInProgress())
	{
		if(args.m_PedOrDummyToUsePoint && args.m_PedOrDummyToUsePoint->GetIsTypePed())
		{ 
			pPed = static_cast<const CPed*>(args.m_PedOrDummyToUsePoint);
			bUsedByClonePed = pPed->IsNetworkClone();		
		}
	}

	for(s32 i = 0; i < effectCount; ++i)
	{
		CEntity *pAttachedEntity = areaPoints[i]->GetEntity();

		//filter out points that are not attached to entities.
		if (args.m_EntitySPsOnly && !pAttachedEntity)
		{
				continue;
		}

		if(!CScenarioManager::CheckScenarioPointEnabledByGroupAndAvailability(*areaPoints[i]))
		{
			continue;
		}

		if(!CScenarioManager::CheckScenarioPointMapData(*areaPoints[i]))
		{
			continue;
		}

		// get the distance from the player to the attractor
		const Vector3 effectPos = VEC3V_TO_VECTOR3(areaPoints[i]->GetPosition());
		const float fDistanceSq = RCC_VECTOR3(args.m_AreaCentre).Dist2(effectPos);
		if( fDistanceSq > rage::square(args.m_MaxRange) )
		{
			continue;
		}

		if( args.m_MustBeFree )
		{
			if(CPedPopulation::IsEffectInUse(*areaPoints[i]))
			{
				continue;
			}
		}

		if (CScenarioManager::IsScenarioPointAttachedEntityUprootedOrBroken(*areaPoints[i]) && !bUsedByClonePed)
		{
			continue;
		}

		// Since this point can be of a virtual type, we need to find out the real types
		// it can manifest itself as. We will loop over the real types.
		const int virtualType = areaPoints[i]->GetScenarioTypeVirtualOrReal();

		// Never consider LookAt scenario types for this function.
		if (CScenarioManager::IsLookAtScenarioType(virtualType))
		{
			continue;
		}

		// Check for trains.
		if (args.m_MustBeAttachedToTrain)
		{
			if(!pAttachedEntity || !pAttachedEntity->GetIsTypeVehicle() || !static_cast<CVehicle*>(pAttachedEntity)->InheritsFromTrain())
			{
				continue;
			}
		}

		const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(virtualType);
		bool found = false;
		for(int j = 0; j < numRealTypes; j++)
		{
			if(!CScenarioManager::CheckScenarioPointEnabledByType(*areaPoints[i], j))
			{
				continue;
			}

			const int scenarioTypeReal = SCENARIOINFOMGR.GetRealScenarioType(virtualType, j);

			// Check the type is correct
			if( args.m_NumTypes > 0 )
			{
				const int iNumTypes = args.m_NumTypes;
				const s32* aTypes = args.m_Types;

				// All types
				bool bValidType = false;
				for( s32 j = 0; j < iNumTypes; j++ )
				{
					if( (s32)scenarioTypeReal == (s32)aTypes[j]
							|| virtualType == aTypes[j]
							)
					{
						bValidType = true;
						break;
					}
				}
				if( !bValidType )
				{
					continue;
				}
			}

			if(args.m_CheckPopulation)
			{
				PF_START(CollisionChecks);
				bool checkMaxInRange = !areaPoints[i]->IsFlagSet(CScenarioPointFlags::IgnoreMaxInRange);
				if(!CScenarioManager::CheckScenarioPopulationCount(scenarioTypeReal, effectPos, CLOSEST_ALLOWED_SCENARIO_SPAWN_DIST, args.m_PedOrDummyToUsePoint, checkMaxInRange))
				{
					PF_STOP(CollisionChecks);
					continue;
				}

// Disabled for now, it didn't actually have any effect!
// TODO: If needed, maybe repair and re-enable this, looking at how GeneratePedsFromScenarioPointList()
// does the same thing.
#if 0
				// Check this position is free of objects (ignoring non-uprooted and this object)
				CEntity* pBlockingEntities[MAX_COLLIDED_OBJECTS] = {NULL, NULL, NULL, NULL, NULL, NULL};
				if(!CPedPlacement::IsPositionClearForPed(effectPos, PED_HUMAN_RADIUS, true, MAX_COLLIDED_OBJECTS, pBlockingEntities, true, false, true))
				{
					for(s32 i = 0; pBlockingEntities[i] != NULL && i < MAX_COLLIDED_OBJECTS; ++i)
					{
						// Ignore this entity
						if(pBlockingEntities[i] == pAttachedEntity)
						{
							continue;
						}
						//can only ignore objects
						if(!pBlockingEntities[i]->GetIsTypeObject())
						{
							break;
						}
						// Ignore doors, for qnan matrix problem
						if(((CObject*)pBlockingEntities[i])->IsADoor())
						{
							continue;
						}
						// Ignore non-uprooted objects
						if(((CObject*)pBlockingEntities[i])->m_nObjectFlags.bHasBeenUprooted)
						{
							break;
						}
					}
				}
#endif	// 0

				PF_STOP(CollisionChecks);
			}

			// One of the real types for this scenario point seems to be eligible. Currently
			// we don't care which one, so just set 'found' to true and break out.
			found = true;
			break;
		}

		if(!found)
		{
			continue;
		}

		if(args.m_Random)
		{
			apValidScenarioPoints[iNumStoredPnts] = areaPoints[i];
			++iNumStoredPnts;
			if( iNumStoredPnts == effectCount )
			{
				break;
			}
		}
		else
		{
			if((*ppScenarioPoint == NULL) || (fDistanceSq < fClosestDistanceSq))
			{
				*ppScenarioPoint = areaPoints[i];
				fClosestDistanceSq = fDistanceSq;
			}
		}
	}
	if(args.m_Random)
	{
		if(iNumStoredPnts == 0)
		{
			return false;
		}
		const s32 iRand = fwRandom::GetRandomNumberInRange(0, iNumStoredPnts);
		*ppScenarioPoint = apValidScenarioPoints[iRand];
		return true;
	}
	else
	{
		return (*ppScenarioPoint != NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsEffectInUse
// PURPOSE:		To do the general checks that determine if a C2dEffect is currently in use
// PARAMS:		The effect and the attached entity
// RETURN:		TRUE if effect is in use, FALSE if not
bool CPedPopulation::IsEffectInUse(const CScenarioPoint& scenarioPoint)
{
	return (scenarioPoint.GetUses() > 0);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ShouldUsePriRemovalOfAmbientPedsFromScenario
// PURPOSE:		Check if we should currently try to remove peds that were spawned at scenarios but are now counted as ambients.
// RETURN:		TRUE if we should prioritize removal of these peds.
bool CPedPopulation::ShouldUsePriRemovalOfAmbientPedsFromScenario()
{
	return ms_aboveAmbientPedLimit && fwTimer::GetTimeInMilliseconds() > ms_nextAllowedPriRemovalOfAmbientPedFromScenario;
}

//////////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetScriptCamPopulationOriginOverride
// PURPOSE :	Allows designers to force the scripted camera to become the origin
//				for spawning of peds AND vehicles; default behaviour is for this to
//				remain centred round the player regardless of script camera.
//				Must be called every frame (for concerns of it not being deactivated)
// PARAMETERS :	None.
// RETURNS :	Nothing.
//////////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::SetScriptCamPopulationOriginOverride()
{
	const camBaseCamera * pCam = camInterface::GetDominantRenderedCamera();
	if(pCam && pCam->GetIsClassId(camScriptedCamera::GetStaticClassId()))
	{
		Vector3 vOrigin = camInterface::GetScriptDirector().GetFrame().GetPosition();
		Vector3 vDir = camInterface::GetScriptDirector().GetFrame().GetFront();
		Vector3 vVel = camInterface::GetVelocity();

		// The velocity we got from the camera tends to be huge if we have done a
		// camera cut (presumably extrapolated from the position delta). That's actually
		// no good for ped population purposes, amongst other things it can cause an excessively
		// large cull sphere for scenario points, which can result in asserts.
		if(camInterface::GetFrame().GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition))
		{
			if(vVel.Mag2() > square(50.0f))		// Allow small velocities, in case the script is controlling the camera pos every frame.
			{
				vVel.Zero();
			}
		}

		float fov = camInterface::GetScriptDirector().GetFrame().GetFov();

		LocInfo locationInfo = LOCATION_EXTERIOR;
		CViewport * pViewport = gVpMan.GetCurrentViewport();
		if(pViewport)
		{
			CInteriorInst* pIntInst = pViewport->GetInteriorProxy() ? pViewport->GetInteriorProxy()->GetInteriorInst() : NULL;
			locationInfo = ClassifyInteriorLocationInfo(false, pIntInst, pViewport->GetRoomIdx());
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
void CPedPopulation::UseTempPopControlPointThisFrame(const Vector3& pos, const Vector3& dir, const Vector3& vel, float fov, LocInfo locationInfo)
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

void CPedPopulation::UseTempPopControlSphereThisFrame(const Vector3& pos, float radius, float vehicleRadius)
{	
	if(NetworkInterface::IsASpectatingPlayer(CGameWorld::FindLocalPlayer()))
	{
		return;
	}

#if ENABLE_NETWORK_LOGGING
	if(!pos.IsClose(m_vLastSetPopPosition, 1.0f))
	{
		AI_LOG_WITH_ARGS("[POP_CONTROL_SPHERE] - old Position: X:%.2f, Y:%.2f, Z:%.2f new Position: X:%.2f, Y:%.2f, Z:%.2f. Radius: %.2f VehicleRadius: %.2f\n",
		m_vLastSetPopPosition.GetX(), m_vLastSetPopPosition.GetY(), m_vLastSetPopPosition.GetZ(),
		pos.GetX(), pos.GetY(), pos.GetZ(),
		radius, vehicleRadius);

		m_vLastSetPopPosition = pos;
	}
#endif // ENABLE_NETWORK_LOGGING

	ms_useTempPopControlSphereThisFrame = true;
	CFocusEntityMgr::GetMgr().SetPopPosAndVel(pos, Vector3(VEC3_ZERO));

	ms_TempPopControlSphereRadius = radius;

	// set vehicle radius as well
	CVehiclePopulation::ms_TempPopControlSphereRadius = vehicleRadius;

	if (NetworkInterface::IsGameInProgress())
	{
		NetworkInterface::SetUsingExtendedPopulationRange(pos);
	}
}

void CPedPopulation::ClearTempPopControlSphere()
{
	ms_useTempPopControlSphereThisFrame = false;
	CFocusEntityMgr::GetMgr().ClearPopPosAndVel();

	if (NetworkInterface::IsGameInProgress())
	{
		NetworkInterface::ClearUsingExtendedPopulationRange();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ScriptedConversionCentreActiveSetThisFrame
// PURPOSE :	Use the scripted conversion center this frame.
// PARAMETERS :	centre - the centre to use.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::ScriptedConversionCentreSetThisFrame(const Vector3& centre)
{
	ms_scriptedConversionCenter			= centre;
	ms_scriptedConversionCentreActive	= true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedNonCreationAreaSet
// PURPOSE :	Start using the non creation area (block peds from being created
//				in this area)
// PARAMETERS :	min - the minimum world values for the non creation box.
//				max - the minimum world values for the non creation box.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::PedNonCreationAreaSet(const Vector3& min, const Vector3& max)
{
	ms_nonCreationAreaMin		= min;
	ms_nonCreationAreaMax		= max;
	ms_nonCreationAreaActive	= true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedNonCreationAreaClear
// PURPOSE :	Stop using the non creation area (allow peds to be created normally)
// PARAMETERS :	None.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::PedNonCreationAreaClear()
{
	ms_nonCreationAreaActive = false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedNonRemovalAreaSet
// PURPOSE :	Start using the non removal area (block peds from being removed
//				in this area)
// PARAMETERS :	min - the minimum world values for the non removal box.
//				max - the minimum world values for the non removal box.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::PedNonRemovalAreaSet(const Vector3& min, const Vector3& max)
{
	ms_nonRemovalAreaMin	= min;
	ms_nonRemovalAreaMax	= max;
	ms_nonRemovalAreaActive	= true;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PedNonRemovalAreaClear
// PURPOSE :	Stop using the non removal area (allow peds to be removed normally)
// PARAMETERS :	None.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::PedNonRemovalAreaClear()
{
	ms_nonRemovalAreaActive = false;
}


/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
// Generates peds at the spawnpoints 2deffects scattered about the map using the
// scenario manager
/////////////////////////////////////////////////////////////////////////////////
s32 CPedPopulation::GenerateScenarioPedsInSpecificArea(	const Vector3& areaCentre,
															bool allowDeepInteriors,
															float fRange,
															s32 iMaxPeds,
															s32 iSpawnOptions)
{
	return GeneratePedsFromScenarioPointList(areaCentre, allowDeepInteriors, 0.0f, fRange, 0.0f, fRange, 0.0f, false, iMaxPeds, iSpawnOptions);
}


/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
// Generates peds at the spawnpoints 2deffects scattered about the map using the
// scenario manager
/////////////////////////////////////////////////////////////////////////////////
s32 CPedPopulation::GeneratePedsAtScenarioPoints(const Vector3& popCtrlCentre, const bool allowDeepInteriors, const float fAddRangeOutOfViewMin, const float fAddRangeOutOfViewMax, const float fAddRangeInViewMin, const float fAddRangeInViewMax, const float fAddRangeFrustumExtra, const bool doInFrustumTest, const s32 iMaxPeds, const s32 iMaxInteriorPeds)
{
	PF_AUTO_PUSH_DETAIL("GeneratePedsAtScenarioPoints");
	PF_FUNC(GeneratePedsFromScenarioPoints);
	PF_FUNC(ScenarioTotal);

	ms_atScenarioPedLimit = (iMaxPeds <= 0);

	// In networking games, we may have stricter limits to how many locally controlled peds we
	// are allowed to have. In that case, the peds will be destroyed again before getting added
	// to the world, but that turns out to be a pretty expensive operation when done every frame
	// for a number of scenario points. Thus, since we have no chance of succeeding in creating
	// scenario peds in that case, we just return early and save a bunch of work. See
	// B* 370256: "[LB] This area of the map is running slow, this was in a CnC Crook Tutorial".
	// Note: this also skips the call to AttractPedsToScenarioPointList() further down, but that
	// function shouldn't be doing anything useful these days.
	bool bAllowedToCreateHighPriorityOnly = false;
	if(!AllowedToCreateMoreLocalScenarioPeds(&bAllowedToCreateHighPriorityOnly))
	{
		BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_factoryFailedToCreatePed, VEC3_ZERO));
		popDebugf2("Not allowed to create more local peds, so will not try to spawn more scenario peds.");
		return 0;
	}

	ms_bCreatingPedsForScenarios = true;

	PF_START_TIMEBAR_DETAIL("Collect points");

	s32 iNumPeds = 0;
	s32 iSpawnOptions = 0;
	s32 iNumPedsToSpawn = iMaxPeds;

	PF_START_TIMEBAR_DETAIL("Gen Peds");
	if (CPed::GetPool()->GetNoOfFreeSpaces() >= ms_minPedSlotsFreeForScenarios)
	{
		if( iMaxPeds == 0 )
		{
			iNumPedsToSpawn = iMaxInteriorPeds;
			iSpawnOptions = SF_interiorAndHighPriOnly;
		}
		if(!iNumPedsToSpawn)
		{
			// Check if the density multipliers have been set low. In that case, we shouldn't
			// even allow high priority points (at least not beyond the normal allowance).
			float fInteriorMult = 1.0f;
			float fExteriorMult = 1.0f;
			CPedPopulation::GetTotalScenarioPedDensityMultipliers(fInteriorMult, fExteriorMult);
	
			// These thresholds are pretty arbitrary:
			const bool allowHighPriInside = (fInteriorMult >= 0.5f);
			const bool allowHighPriOutside = (fExteriorMult >= 0.5f);
	
			// Check if we allow either interior or exterior special high priority peds.
			if(allowHighPriInside || allowHighPriOutside)
			{
				// If we're still not allowed to spawn anything, set the allowed number to 1 but reserve it
				// for high priority scenarios. If we exceed the ped budget due to this, hopefully we will despawn
				// regular priority peds to get back down.
				iNumPedsToSpawn = 1;
				iSpawnOptions = SF_highPriOnly;
	
				// Set the spawn options to specify if we are looking for an interior or exterior high priority ped.
				if(!allowHighPriInside)
				{
					iSpawnOptions |= SF_dontSpawnInterior;
				}
				if(!allowHighPriOutside)
				{
					iSpawnOptions |= SF_dontSpawnExterior;
				}
			}
		}
	}

	// If peds are going to be frozen in the exterior, we probably shouldn't try to spawn anybody there,
	// we are now removing them fairly aggressively in that case.
	if(CGameWorld::GetFreezeExteriorPeds())
	{
		iSpawnOptions |= SF_dontSpawnExterior;
	}

	if(iNumPedsToSpawn > 0)
	{
		if(bAllowedToCreateHighPriorityOnly)
		{
			iSpawnOptions |= SF_highPriOnly;
		}

		iNumPeds = GeneratePedsFromScenarioPointList(popCtrlCentre, allowDeepInteriors, fAddRangeOutOfViewMin, fAddRangeOutOfViewMax, fAddRangeInViewMin, fAddRangeInViewMax, fAddRangeFrustumExtra, doInFrustumTest, iNumPedsToSpawn, iSpawnOptions );
	}

	//PF_START_TIMEBAR_DETAIL("Attract Peds");
	//AttractPedsToScenarioPointList();

	ms_bCreatingPedsForScenarios = false;
	if (iNumPeds > 0)
	{
		popDebugf2("Created %d scenario peds, %d max requested (%d max for indoors)", iNumPeds, iMaxPeds, iMaxInteriorPeds);
	}
	return iNumPeds;
}

static const Vector3* sortPopCenter = NULL;
static bool ScenarioPointSortByDistance(const CScenarioPoint* const & a, const CScenarioPoint* const & b)
{
	Assert(sortPopCenter);

	ScalarV dist_a = DistSquared(a->GetPosition(), VECTOR3_TO_VEC3V((*sortPopCenter)));
	ScalarV dist_b = DistSquared(b->GetPosition(), VECTOR3_TO_VEC3V((*sortPopCenter)));

	return (IsLessThanAll(dist_a, dist_b) != 0) ? true : false;
}

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
s32 CPedPopulation::GeneratePedsFromScenarioPointList(	const Vector3& popCtrlCentre,
														const bool allowDeepInteriors,
														const float fAddRangeOutOfViewMin,
														const float fAddRangeOutOfViewMax,
														const float fAddRangeInViewMin,
														const float fAddRangeInViewMax,
														const float fAddRangeFrustumExtra,
														const bool doInFrustumTest,
														s32 iMaxPeds,
														const s32 iSpawnOptions)
{
	// It appears that in some cases, the range passed in here can be huge. While playing
	// the sniper section of Solomon 1, a range of >1400 m was observed, possibly due to
	// the sniper scope or movement speed. This may be a potential performance issue and it's
	// unlikely that we ever would want to spawn peds that far away, so we clamp it at least
	// a bit here now. See B* 336855: "Ignorable Assert - [ai_task] Error: Assertf(totalFound
	// <= maxInDestArray) FAILED: CScenarioPointManager::FindPointsInArea: - fired
	// while aiming remote sniper at plane"
	float scenarioFindRange = Min(Max(fAddRangeOutOfViewMax, fAddRangeInViewMax), 400.0f);

	Vec3V scenarioFindCenterV = RCC_VEC3V(popCtrlCentre);

	// In SP_SpawnValidityCheck_Position(), we actually reject any point outside of
	// fAddRangeInViewMax from the player ped, so there shouldn't be any reason to even
	// look for scenario points any further than that. Below, we recompute scenarioFindRange
	// to account for this.	
	if(Likely(!ms_useTempPopControlPointThisFrame && !ms_useTempPopControlSphereThisFrame)) // if we're changing the pop control point, we can't have the scenario points limited by the position of the player
	{
		const CPed* player = FindPlayerPed();
		if(player)
		{
			// Get the position of the population center, and the player.
			const Vec3V popPosV = RCC_VEC3V(popCtrlCentre);
			const Vec3V playerPosV = player->GetTransform().GetPosition();

			// In most cases, these positions would be more or less the same, but we may not want to
			// rely on that. Here, we compute the distance between them.
			const ScalarV centerToPlayerDistV = Dist(playerPosV, popPosV);

			// Get the limit. Stored as a square right now so we compute the root - cost of this
			// should be fairly negligible.
			const ScalarV rangeLimitV(fAddRangeInViewMax);

			// The furthest point from the population center where we could possibly find
			// a valid scenario is the sum of the distance between the positions, and the radius
			// around the player.
			const ScalarV maxDistFromPopCtrlCenterV = Add(rangeLimitV, centerToPlayerDistV);

			const ScalarV minDistFromPopCtrlCenterV = Subtract(centerToPlayerDistV, rangeLimitV);	// Note: may be <0

			// Check if the player's sphere is actually completely outside of the sphere we want to find
			// points within. If so, there is no overlap and there is no need to even try to find any points.
			if(minDistFromPopCtrlCenterV.Getf() > scenarioFindRange)
			{
				return 0;
			}

			const float maxDistFromPopCtrlCenter = maxDistFromPopCtrlCenterV.Getf();

			// If this condition holds up, it means that the sphere around the player is completely contained
			// within the original cull sphere. Thus, we can just switch both the radius and the center to
			// math the sphere around the player.
			if(maxDistFromPopCtrlCenter < scenarioFindRange)
			{
				scenarioFindRange = rangeLimitV.Getf();
				scenarioFindCenterV = playerPosV;
			}
		}
	}

	// Except for when spawning at the start, limit the max number of peds to create each frame.
	if(!ShouldUseStartupMode())
	{
		iMaxPeds = Min(iMaxPeds, ms_maxScenarioPedCreatedPerFrame);
	}

#define SCENARIO_POINT_BATCH_FRAMES (4)
#define SCENARIO_POINT_RECALCULATE_DISTANCE (20.f)
	static u32 batchFrames = SCENARIO_POINT_BATCH_FRAMES;

	Vector3 scenarioFindCenter = VEC3V_TO_VECTOR3(scenarioFindCenterV);

	bool recalculateForcedByMovement = false;
	if(ms_vCachedScenarioPointCenter.Dist2(scenarioFindCenter) > rage::square(SCENARIO_POINT_RECALCULATE_DISTANCE))
	{
		recalculateForcedByMovement = true;
		// change our batch frames, as we're moving fast enough to not get through all of the points in the batch we're using
		batchFrames = MIN(MAX(fwTimer::GetFrameCount() - ms_cachedScenarioPointFrame, 1), batchFrames);
	}

	if((ms_cachedScenarioPointFrame+batchFrames) < fwTimer::GetFrameCount() || recalculateForcedByMovement || ms_bInvalidateCachedScenarioPoints)
	{
		ms_bInvalidateCachedScenarioPoints = false;

		if(!recalculateForcedByMovement) // we didn't force a recalculate
		{
			batchFrames = SCENARIO_POINT_BATCH_FRAMES; // reset to the normal frame batch count

			if(CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
			{
				batchFrames *= 3; // batch less frequently with a wanted level
			}
		}
		

		ms_vCachedScenarioPointCenter = scenarioFindCenter;
		ms_cachedScenarioPointFrame = fwTimer::GetFrameCount();		
		ms_cachedScenarioPoints.Reset();
		
		ms_lowPriorityScenarioPointsAlreadyChecked = 0;
		// Note: one thing that's a bit odd about scenarioFindRange value above is that fAddRangeInViewMax may
		// actually be a larger value, so we might not be finding scenario points as far enough as we'd like.
		// We may also not be finding high priority scenario points at a far enough distance.

		CScenarioPoint* areaPoints[CScenarioPointManager::kMaxPtsInArea];
		bool includeClusteredPoints = false;	// These would get rejected anyway.
		scenarioFindRange = scenarioFindRange + (recalculateForcedByMovement ? SCENARIO_POINT_RECALCULATE_DISTANCE : 0.f); // increase the find distance if we're moving fast
		float extendedFindRange = ms_addRangeExtendedScenarioEffective + (recalculateForcedByMovement ? SCENARIO_POINT_RECALCULATE_DISTANCE : 0.f); // increase the find distance if we're moving fast
		const s32 pointcount = (s32)SCENARIOPOINTMGR.FindPointsInArea(scenarioFindCenterV, ScalarV(scenarioFindRange), areaPoints, NELEM(areaPoints), ScalarV(extendedFindRange), includeClusteredPoints);		

		// Go through the array of points we found, and rearrange them so that all
		// the high priority ones end up first in the array.
		u32 highPriorityPoints = 0;
		for(u32 i = 0; i < pointcount; i++)
		{
			CScenarioPoint* pt = areaPoints[i];
			if(pt->IsHighPriority())
			{
				if(highPriorityPoints != i)
				{
					CScenarioPoint* other = areaPoints[highPriorityPoints];
					areaPoints[highPriorityPoints] = pt;
					areaPoints[i] = other;
				}
				highPriorityPoints++;
			}
		}

		// usually we want to randomize the indices but if we've just spawned into the game sort the lists
		// by distance to populate scenario points close to the player first. Though we use only the world spawns
		// to avoid doing too much work
		if (!ShouldUseStartupMode())
		{
			// randomize hight priority points
			for (s32 i = 0; i < highPriorityPoints; ++i)
			{
				const s32 rndIndex = fwRandom::GetRandomNumberInRange(0, highPriorityPoints);
				Assert(rndIndex >= 0 && rndIndex < highPriorityPoints);
				CScenarioPoint* tmp = areaPoints[rndIndex];
				areaPoints[rndIndex] = areaPoints[i];
				areaPoints[i] = tmp;
			}
			// normal priority
			for (s32 i = highPriorityPoints; i < pointcount; ++i)
			{
				const s32 rndIndex = fwRandom::GetRandomNumberInRange(highPriorityPoints, pointcount);
				Assert(rndIndex >= highPriorityPoints && rndIndex < pointcount);
				CScenarioPoint* tmp = areaPoints[rndIndex];
				areaPoints[rndIndex] = areaPoints[i];
				areaPoints[i] = tmp;
			}
		}
		else
		{
			if(pointcount) // don't try to sort if we have zero points
			{
				// high priority
				sortPopCenter = &ms_vCachedScenarioPointCenter;
				std::sort(areaPoints, areaPoints + highPriorityPoints, ScenarioPointSortByDistance);

				// normal priority	
				std::sort(areaPoints + highPriorityPoints, areaPoints + pointcount, ScenarioPointSortByDistance);		
			}
		}

		// put the points into the cached array
		// Run them through the General check to reduce them a little
		ms_numHighPriorityScenarioPoints = 0;
		for(int i = 0; i < pointcount; i++)
		{
			if(i < (pointcount-1))
				PrefetchDC(areaPoints[i+1]);
			if(!ms_cachedScenarioPoints.IsFull()) 
			{
				if(SP_SpawnValidityCheck_General(*areaPoints[i], includeClusteredPoints)) // don't use cluser points (set as false above for the search)
				{
					ms_cachedScenarioPoints.Append() = areaPoints[i];
					if(areaPoints[i]->IsHighPriority())
						ms_numHighPriorityScenarioPoints++;
				}
			}
			else
			{
				popDebugf1("Ran out of space in our cached scenario point array (%sduring a player switch) - we found %d points with range %.0f", g_PlayerSwitch.IsActive() ? "" :"not ", pointcount, scenarioFindRange);
				break; // don't spam the output
			}
		}
	}
	else
	{
		// didn't recalculate, but randomize the high priority points
		if (!ShouldUseStartupMode()) // only if we're not in startup mode
		{
			// randomize hight priority points
			for (s32 i = 0; i < ms_numHighPriorityScenarioPoints; ++i)
			{
				const s32 rndIndex = fwRandom::GetRandomNumberInRange(0, ms_numHighPriorityScenarioPoints);
				Assert(rndIndex >= 0 && rndIndex < ms_numHighPriorityScenarioPoints);
				CScenarioPoint* tmp = ms_cachedScenarioPoints[rndIndex];
				ms_cachedScenarioPoints[rndIndex] = ms_cachedScenarioPoints[i];
				ms_cachedScenarioPoints[i] = tmp;
			}
		}
	}


	#if __DEV
	if(CDebugScene::bDisplayCandidateScenarioPointsOnVMap)
	{
		Vec3V playerPos(V_ZERO);
		CPed* player = FindPlayerPed();
		if (player)
		{
			playerPos = player->GetTransform().GetPosition();
		}

		for(s32 iSpawnPoint = 0; iSpawnPoint < ms_cachedScenarioPoints.GetCount(); ++iSpawnPoint)
		{
			Vec3V scenarioPos(V_ZERO);
			CEntity* pAttachedEntity = NULL;

			pAttachedEntity = ms_cachedScenarioPoints[iSpawnPoint]->GetEntity();
			scenarioPos = ms_cachedScenarioPoints[iSpawnPoint]->GetPosition();

			//if we only want points attached to objects and this point is not attached then 
			// we should do no work... 
			if((iSpawnOptions & SF_objectsOnly) && !pAttachedEntity)
			{
				continue;
			}

			// Make sure the object is within the creation zones.
			ScalarV distSqrd = DistSquared(scenarioPos, playerPos);
			ScalarV maxDistSqrd(fAddRangeInViewMax * fAddRangeInViewMax);
			if (IsGreaterThanAll(distSqrd, maxDistSqrd))
			{
				CVectorMap::DrawCircle(VEC3V_TO_VECTOR3(scenarioPos), 2.50f, Color32(200,0,0,128), false);
			}
			else
			{
				CVectorMap::DrawCircle(VEC3V_TO_VECTOR3(scenarioPos), 2.50f, Color32(128,128,128,128), false);
			}
		}
	}
	#endif // __DEV

	int numHighPriCreated = 0;

	int iTries = 0;

	if(ms_numHighPriorityScenarioPoints)
	{
		// the network code will limit the local population based on factors such as the number of
		// cars using pretend occupants. We don't want this code to stop high priority scenario peds from spawning
		CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);

		// Recompute distance thresholds to make peds spawning at high priority scenarios
		// spawn potentially further away (so they can generally be in place early on),
		// or let them spawn much closer behind you or even in front of you than normal,
		// as it's important that they exist as missions may depend on them.
		const float fModifiedRangeOutOfViewMin = Min(fAddRangeOutOfViewMin, ms_addRangeHighPriScenarioOutOfViewMin);
		const float fModifiedRangeOutOfViewMax = Max(fAddRangeOutOfViewMax, ms_addRangeHighPriScenarioOutOfViewMax);
		const float fModifiedRangeInViewMin = Min(fAddRangeInViewMin, ms_addRangeHighPriScenarioInViewMin);
		const float fModifiedRangeInViewMax = Max(fAddRangeInViewMax, ms_addRangeHighPriScenarioInViewMax);

		numHighPriCreated = GeneratePedsFromScenarioPointList(
				ms_cachedScenarioPoints.GetElements(), 
				ms_numHighPriorityScenarioPoints,
				popCtrlCentre,
				allowDeepInteriors,
				fModifiedRangeOutOfViewMin,
				fModifiedRangeOutOfViewMax,
				fModifiedRangeInViewMin,
				fModifiedRangeInViewMax,
				fAddRangeFrustumExtra,
				doInFrustumTest,
				iMaxPeds,
				iSpawnOptions | SF_highPriOnly,
				iTries);

		// Decrease the number of peds that we are still trying to spawn, based on what we just spawned.
		iMaxPeds -= numHighPriCreated;

		// If we just created some high priority scenario peds and filled up the local ped budget,
		// don't try to spawn more peds this frame. Otherwise, we may get a CPU time spike due to failed
		// ped creation attempts.
		if(numHighPriCreated)
		{
			if(!AllowedToCreateMoreLocalScenarioPeds())
			{
				popDebugf2("Not allowed to create more local peds after high priority scenario spawned.");
				iMaxPeds = 0;
			}
		}

		CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);
	}

	int numRegularPriCreated = 0;

	if(iMaxPeds > 0 && !(iSpawnOptions & SF_highPriOnly))
	{
		u32 lowPriorityPoints = ms_cachedScenarioPoints.GetCount() - ms_numHighPriorityScenarioPoints;
		u32 numToCheck = (lowPriorityPoints / batchFrames) + (lowPriorityPoints % batchFrames);
		numToCheck = MIN(numToCheck,MAX(lowPriorityPoints - ms_lowPriorityScenarioPointsAlreadyChecked, 0));
		numRegularPriCreated = GeneratePedsFromScenarioPointList(
				ms_cachedScenarioPoints.GetElements() + ms_numHighPriorityScenarioPoints + ms_lowPriorityScenarioPointsAlreadyChecked, 
				numToCheck,
				popCtrlCentre,
				allowDeepInteriors,
				fAddRangeOutOfViewMin,
				fAddRangeOutOfViewMax,
				fAddRangeInViewMin,
				fAddRangeInViewMax,
				fAddRangeFrustumExtra,
				doInFrustumTest,
				iMaxPeds,
				iSpawnOptions,
				iTries);

		ms_lowPriorityScenarioPointsAlreadyChecked += numToCheck;
	}

	return numHighPriCreated + numRegularPriCreated;
}

const s32 g_iNumExcludeScenarioPoints = 2;
const Vector4 g_vExcludeScenarioPoints[g_iNumExcludeScenarioPoints] =
{
	// url:bugstar:1646529 - Why is there so much screaming around this apartment?
	// Add a couple of points to stop scenario peds spawning in mid-air in this location
	Vector4( -766.0f, -766.6f, 27.3f,			2.0f ),
	Vector4( -766.2f, -771.4f, 27.4f,		2.0f )
};

s32 CPedPopulation::GeneratePedsFromScenarioPointList(
		CScenarioPoint** areaPoints,
		const int pointcount,
		const Vector3& popCtrlCentre,
		const bool allowDeepInteriors,
		const float fAddRangeOutOfViewMin,
		const float fAddRangeOutOfViewMax,
		const float fAddRangeInViewMin,
		const float fAddRangeInViewMax,
		const float fAddRangeFrustumExtra,
		const bool doInFrustumTest,
		const s32 iMaxPeds,
		const s32 iSpawnOptions,
		int& iTriesInOut)
{
	s32 iPedsCreated	= 0;
	s32 iTries			= iTriesInOut;

	popAssert(areaPoints);

	//No points no generate...
	if (!pointcount)
		return iPedsCreated;

	bool atStart = ShouldUseStartupMode();	

	// Determine how many expensive attempts at spawning peds we can do this frame.
	// Traditionally this was iMaxPeds*3, but now we limit it to not go beyond a certain value.
	int iMaxTries = iMaxPeds*3;
	if(atStart)
	{
		iMaxTries = Min(iMaxTries, ms_maxScenarioPedCreationAttempsPerFrameOnInit);
	}
	else
	{
		iMaxTries = Min(iMaxTries, ms_maxScenarioPedCreationAttempsPerFrameNormal);
	}

	int iNumFailedCollisionBefore = 0;

	// Go over the spawn points looking for valid ones to spawn from.
	// Note: the spawn point list should be shuffled so going from start
	// to finish shouldn't cause any distributions problems.
	for(s32 iSpawnPoint = 0; iSpawnPoint < pointcount; ++iSpawnPoint)
	{
		// Make sure we haven't already created enough peds or tried too many times.
		if((iPedsCreated >= iMaxPeds) || (iTries >= iMaxTries) || ms_scheduledScenarioPeds.IsFull())
		{
			// Break, not return, so we still write back to iTriesInOut.
			break;
		}
		if(iSpawnPoint < (pointcount-1))
			PrefetchDC(areaPoints[iSpawnPoint+1]); // try to prefetch the next scenario point
		CScenarioPoint* pScenarioPoint = areaPoints[iSpawnPoint];

		popAssert(pScenarioPoint);

		// If the SF_highPriOnly flag is set, we are currently not expected to even get here
		// except for the high priority points.
		popAssert(!(iSpawnOptions & SF_highPriOnly) || pScenarioPoint->IsHighPriority());

		// Check if this point failed its collision checks too recently (possibly because of being
		// placed in an interior that's streamed out). If so, skip it (it will time out eventually).
		if(pScenarioPoint->HasFailedCollisionRecently())
		{
			continue;
		}

		// HACK
		// Test this scenario position against a list of excluded points
		// to fix any errors noticed after shipping.
		// Add these to the "g_vExcludeScenarioPoints" list, along with bug number.

		bool bExclude = false;
		for(s32 x=0; x<g_iNumExcludeScenarioPoints && !bExclude; x++)
		{
			const ScalarV fDistSqr = MagSquared( pScenarioPoint->GetPosition() - VECTOR4_TO_VEC3V(g_vExcludeScenarioPoints[x]) );
			if( (fDistSqr < ScalarV(g_vExcludeScenarioPoints[x].w)).Getb() )
			{
				bExclude = true;
			}
		}
		if(bExclude)
			continue;

		// Mostly because we quantize time (to seconds or so, see GetFailedCollisionTicks()),
		// it's likely that many points will have their timers expired on the same frame.
		// To deal with this, we limit the number that can "wake up" on each update.
		// This should work OK, since we don't need the time to be very accurate or anything.
		if(pScenarioPoint->WasFailingCollision())
		{
			iNumFailedCollisionBefore++;

			if(iNumFailedCollisionBefore > 1)	// Just one per frame, at this point.
			{
				continue;
			}
		}

		pScenarioPoint->ClearFailedCollision();

		if (!SP_SpawnValidityCheck_General(*pScenarioPoint, false /* dont allow clustered points */))
		{
			continue;
		}

		CTrain* pAttachedTrain = NULL;
		if (!SP_SpawnValidityCheck_EntityAndTrainAttachementFind(*pScenarioPoint, pAttachedTrain))
		{
			continue;
		}

		if (!SP_SpawnValidityCheck_Interior(*pScenarioPoint, iSpawnOptions))
		{
			continue;
		}

		int foundIndexWithinType = -1;
		const int scenarioTypeReal = SelectPointsRealType(*pScenarioPoint, iSpawnOptions, foundIndexWithinType);
		const int indexWithinType = foundIndexWithinType;
		if (scenarioTypeReal == -1)
		{
			continue;
		}

		if (!SP_SpawnValidityCheck_ScenarioType(*pScenarioPoint, scenarioTypeReal))
		{
			continue;
		}

		if (!SP_SpawnValidityCheck_ScenarioInfo(*pScenarioPoint, scenarioTypeReal))
		{
			continue;
		}

		SP_SpawnValidityCheck_PositionParams params;
		params.doInFrustumTest = doInFrustumTest;
		params.fAddRangeOutOfViewMin = fAddRangeOutOfViewMin;
		params.fAddRangeOutOfViewMax = fAddRangeOutOfViewMax;
		params.fAddRangeInViewMin = fAddRangeInViewMin;
		params.fAddRangeInViewMax = fAddRangeInViewMax;
		params.fAddRangeFrustumExtra = fAddRangeFrustumExtra;
		params.popCtrlCentre = popCtrlCentre;

		if (!SP_SpawnValidityCheck_Position(*pScenarioPoint, scenarioTypeReal, params))
		{
			continue;
		}

		// At this point, we will soon do some expensive things:
		// - CheckScenarioPopulationCount(), looking for other peds in the world.
		// - IsPositionClearForPed(), also looking for other objects in the world.
		// - CPedPlacement::FindZCoorForPed() (through GiveScenarioToScenarioPoint()/CreateScenarioPed()), finding the ground position.
		// These are all fairly expensive, so even if we don't do all of them, we still consider this a "try" counting towards
		// the per-frame limit.
		++iTries;

		PF_START(CollisionChecks);

		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioTypeReal);
		CObject* pPropInEnvironment = NULL;
		if(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
		{
			if(!CScenarioManager::FindPropsInEnvironmentForScenario(pPropInEnvironment, *pScenarioPoint, scenarioTypeReal, true))
			{
				// I think this makes sense: if we couldn't find the appropriate props for the scenario,
				// we mark it as having failed collision, which will prevent us from looking again for a few seconds.
				// This should help to reduce CPU time spent looking for props, if the right props aren't there.
				pScenarioPoint->ReportFailedCollision();
				BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioNoPropFound, VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition()), pScenarioPoint));
				PF_STOP(CollisionChecks);
				continue;
			}

			// Note: there may still be a need to add some additional caching, because if we succeeded in finding props,
			// we may still not spawn somebody this frame (commonly, because we are waiting for them to stream in),
			// and will have to repeat the test again on the next attempt.
		}

		// First, look for an existing entry in the spawn check cache, or create a new one.
		CScenarioSpawnCheckCache::Entry* spawnCheckCacheEntry = &CScenarioManager::GetSpawnCheckCache().FindOrCreateEntry(*pScenarioPoint, scenarioTypeReal);

		// Check if the entry says that we failed in the past. If so, we don't bother doing any more checks right now,
		// as they are likely to fail anyway.  The cache entry should expire in a moment, and then this point will
		// start to get checked again.
		if(spawnCheckCacheEntry->m_FailedCheck)
		{
			BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_scenarioCachedFailure, VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition()), pScenarioPoint));
			PF_STOP(CollisionChecks);
			continue;
		}

		// Next, we check if we have already passed the "early model check". The purpose of this is to make sure we don't
		// waste time doing collision checks if we won't have a ped model to use anyway. This check is only done once for
		// as long as the cache entry exists.
		CScenarioManager::PedTypeForScenario* predeterminedPedType = NULL;
		CScenarioManager::PedTypeForScenario pedType;
		if(!spawnCheckCacheEntry->m_PassedEarlyModelCheck)
		{
			// Attempt to select a ped type that we would use. This call shouldn't actually stream anything in, but may select
			// a model that's not resident that we may add a streaming request for later.
			if(!CScenarioManager::SelectPedTypeForScenario(pedType, scenarioTypeReal, *pScenarioPoint, VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition()), 0, 0, fwModelId(), true))
			{
				// Couldn't find any ped at all, neither already resident nor something we are allowed to add a streaming request for.
				// Store this as a failure in the cache.
				spawnCheckCacheEntry->m_FailedCheck = true;
				spawnCheckCacheEntry->UpdateTime(500);		// Check back in 0.5 s, by then perhaps the conditions have changed (a new model got streamed in, etc).

				BANK_ONLY(ms_populationFailureData.RegisterSpawnFailure(PedPopulationFailureData::ST_Scenario, PedPopulationFailureData::FR_noPedModelFound, VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition()), pScenarioPoint));
				PF_STOP(CollisionChecks);
				continue;
			}

			// Since we have called SelectPedTypeForScenario(), set the predeterminedPedType pointer. This should
			// prevent the need for another call to SelectPedTypeForScenario() on this same frame, if we pass all
			// the other tests and actually want to spawn something.
			predeterminedPedType = &pedType;

			// Also, remember that we have passed this early model check. This just means that on the next attempt,
			// we will start with the collision check and not waste time on ped selection. If the collision check
			// succeeds, we will check again to make sure it's still possible to pick a model.
			spawnCheckCacheEntry->m_PassedEarlyModelCheck = true;
			spawnCheckCacheEntry->UpdateTime(2000);			// Probably not too critical, but consider this check passed for the next 2 s.
		}

		// Set up the parameters we would need in order to do a collision check.
		CScenarioManager::CollisionCheckParams collisionCheckParams;
		collisionCheckParams.m_pScenarioPoint = pScenarioPoint;
		collisionCheckParams.m_ScenarioTypeReal = scenarioTypeReal;
		collisionCheckParams.m_ScenarioPos = pScenarioPoint->GetWorldPosition();
		collisionCheckParams.m_pAttachedEntity = pScenarioPoint->GetEntity();
		collisionCheckParams.m_pAttachedTrain = pAttachedTrain;
		collisionCheckParams.m_pPropInEnvironment = pPropInEnvironment;

		// Check if we have already passed a preliminary collision check.
		CScenarioManager::CollisionCheckParams* pCollisionCheckParamsForLater = NULL;
		if(spawnCheckCacheEntry->m_PassedEarlyCollisionCheck)
		{
			// Yes, we passed the test already, on an earlier frame. It's likely that the
			// collision test would pass again, so we will focus on getting a ped model to use.
			// However, before we actually spawn, we should still do the test again, so we
			// set this pointer:
			pCollisionCheckParamsForLater = &collisionCheckParams;
		}
		else
		{
			// Do the preliminary collision check.
			if(CScenarioManager::PerformCollisionChecksForScenarioPoint(collisionCheckParams))
			{
				// Collision doesn't seem to be a problem, remember this for up to 2 s.
				spawnCheckCacheEntry->m_PassedEarlyCollisionCheck = true;
				spawnCheckCacheEntry->UpdateTime(2000);
			}
			else
			{
				// The collision check failed. This condition is probably not changing too
				// rapidly, so wait 2 s before trying again.
				spawnCheckCacheEntry->m_FailedCheck = true;
				spawnCheckCacheEntry->UpdateTime(2000);

				PF_STOP(CollisionChecks);
				continue;
			}
		}
		PF_STOP(CollisionChecks);

		// This point really shouldn't be in the history since we checked that already further up,
		// but this assert was added to try to narrow down what could possibly cause a similar
		// assert to fail after spawning the ped in CScenarioManager::CreateScenarioPed().
		Assert(((iSpawnOptions & CPedPopulation::SF_ignoreScenarioHistory) != 0) || !pScenarioPoint->HasHistory());

			
		// Add a scheduled ped
		ScheduledScenarioPed& scheduledPed = ms_scheduledScenarioPeds.Append();
		scheduledPed.pScenarioPoint = pScenarioPoint;
		scheduledPed.fAddRangeInViewMin = fAddRangeInViewMin;
		scheduledPed.bAllowDeepInteriors = allowDeepInteriors;
		scheduledPed.iMaxPeds = iMaxPeds - iPedsCreated;
		scheduledPed.iSpawnOptions = iSpawnOptions;
		scheduledPed.indexWithinType = indexWithinType;
		scheduledPed.pPropInEnvironment = pPropInEnvironment;
		scheduledPed.collisionCheckParamsForLater = collisionCheckParams;
		scheduledPed.predeterminedPedType = pedType;			
		scheduledPed.uFrameScheduled = fwTimer::GetFrameCount();
		scheduledPed.chainUser = NULL;

		//If this point is chained we need to get some chain user info ... 
		if (pScenarioPoint->IsChained())
		{
			scheduledPed.chainUser = CScenarioChainingGraph::RegisterChainUserDummy(*pScenarioPoint);
		}

		// Add some dummy refs to the ped model to keep it from streaming out
		if(scheduledPed.predeterminedPedType.m_PedModelIndex!=fwModelId::MI_INVALID)
		{				
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.predeterminedPedType.m_PedModelIndex)));
			pPedModelInfo->AddRef();
		}

		if(atStart) // if we're "at start" spawn this ped right now to get some peds in the world
		{
			// spawn a ped right now
			iPedsCreated += SpawnScheduledScenarioPed();
		}
	}

	iTriesInOut = iTries;

	return iPedsCreated;
}

// Try to spawn a ped from the scheduled ped queue
// Returns how many peds were spawned
s32 CPedPopulation::SpawnScheduledScenarioPed()
{
	int numNew = 0;
	int maxTries = ms_scheduledScenarioPeds.GetCount();
	while(maxTries > 0)
	{
		maxTries--;
		ScheduledScenarioPed& scheduledPed = ms_scheduledScenarioPeds.Top();

		// Check to see if ped has expired
		if(scheduledPed.uFrameScheduled < (fwTimer::GetFrameCount() - SCHEDULED_PED_TIMEOUT))
		{
			DropScheduledScenarioPed();
			continue;
		}

		if(!scheduledPed.pScenarioPoint) // scenario point might be invalid now
		{
			DropScheduledScenarioPed();
			continue;
		}
		else
		{
			// Point is valid, check for history
			if(scheduledPed.pScenarioPoint->HasHistory() && ((scheduledPed.iSpawnOptions & CPedPopulation::SF_ignoreScenarioHistory) == 0))
			{
				DropScheduledScenarioPed();
				continue; // point has history, but spawn options don't ignore it - skip this point
			}
				
#if __DEV	// testing for a possible crash later on
			if(scheduledPed.predeterminedPedType.m_PedModelIndex!=fwModelId::MI_INVALID && !scheduledPed.predeterminedPedType.m_NeedsToBeStreamed)
			{
				CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.predeterminedPedType.m_PedModelIndex)));
				Assertf(pPedModelInfo->GetDrawable() || pPedModelInfo->GetFragType(), "Spawning Scheduled Ped %s:Ped model is not loaded", pPedModelInfo->GetModelName());
				popDebugf2("Spawning Scheduled Scenario Ped %s", pPedModelInfo->GetModelName());
			}
#endif
			
			if(	scheduledPed.predeterminedPedType.m_PedModelIndex!=fwModelId::MI_INVALID )
			{
				// The level designers can suppress specific models
				if (CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(scheduledPed.predeterminedPedType.m_PedModelIndex))
			  	{
					DropScheduledScenarioPed();
					continue;
				}

				// Prevent unallowed peds from spawning in MP -- Used by the spawning system to prevent invalid (birds, etc) peds from spawning (in MP).
				if (!CWildlifeManager::GetInstance().CheckWildlifeMultiplayerSpawnConditions(scheduledPed.predeterminedPedType.m_PedModelIndex))
				{
					DropScheduledScenarioPed();
					continue;
				}
			}
			
			// see if we can spawn more peds at all!
			bool highPriorityOnly = false;
			if(!AllowedToCreateMoreLocalScenarioPeds(&highPriorityOnly))
			{				
				DropScheduledScenarioPed();
				continue;
			}

			bool isHighPriority = false;			
			if(scheduledPed.pScenarioPoint->IsHighPriority())
			{				
				isHighPriority = true; // this is a high priority point
			}

			if(highPriorityOnly)
			{
				// we can only create high-priority scenario peds
				if(!isHighPriority)
				{
					DropScheduledScenarioPed();
					continue;
				}
			}

			// Store off the chainUser pointer and clear out the smart pointer. If we fail to spawn,
			// it will have to be restored again. The reason for this is so we can run with more strict
			// asserts on the reference counts of CScenarioPointChainUseInfo.
			CScenarioPointChainUseInfo* pUser = scheduledPed.chainUser;
			scheduledPed.chainUser = NULL;

			if(isHighPriority)
			{
				NetworkInterface::SetProcessLocalPopulationLimits(false); // creating a high prio ped, we need to loosen these restraints
			}

			numNew = CScenarioManager::GiveScenarioToScenarioPoint(*scheduledPed.pScenarioPoint, scheduledPed.fAddRangeInViewMin, scheduledPed.bAllowDeepInteriors, 
				scheduledPed.iMaxPeds, scheduledPed.iSpawnOptions, scheduledPed.indexWithinType, scheduledPed.pPropInEnvironment, 
				scheduledPed.collisionCheckParamsForLater.m_pScenarioPoint ? &scheduledPed.collisionCheckParamsForLater : NULL,
				scheduledPed.predeterminedPedType.m_PedModelIndex!=fwModelId::MI_INVALID ? &scheduledPed.predeterminedPedType : NULL, pUser);
			
			if(isHighPriority) // turn this back
			{
				NetworkInterface::SetProcessLocalPopulationLimits(true);
			}

			if(numNew)
			{
				// If we spawned something, make sure to clear out the cache entries for this point,
				// so we can use them for other points.		
				CScenarioManager::GetSpawnCheckCache().RemoveEntries(*scheduledPed.pScenarioPoint);
				DropScheduledScenarioPed(); // we succeeded in creating this ped, drop it from the queue				
			}
			else
			{
				scheduledPed.chainUser = pUser;

				// check to see if model needed to be streamed in - keep it in the queue in this case
				if(scheduledPed.predeterminedPedType.m_PedModelIndex!=fwModelId::MI_INVALID && scheduledPed.predeterminedPedType.m_NeedsToBeStreamed)
				{
					// Check if the model has streamed in
					if (CModelInfo::HaveAssetsLoaded(fwModelId(strLocalIndex(scheduledPed.predeterminedPedType.m_PedModelIndex))))
					{
						// set that it not longer needs to be streamed in
						scheduledPed.predeterminedPedType.m_NeedsToBeStreamed = false;						
						
					}

					// Don't drop the ped, just put it to the back of the queue
					ScheduledScenarioPed* popped = &ms_scheduledScenarioPeds.Pop(); // remove it from the queue
					ScheduledScenarioPed* pushLoc = &ms_scheduledScenarioPeds.Append(); // add it back to the end

					//Only need to do this if we are copying to a different slot than the one we are already in.
					if (popped != pushLoc)
					{
						(*pushLoc) = (*popped);
						popped->chainUser = NULL;//clear out this pointer from the popped item...
					}
				}
				else
				{
					// failed but doesn't need to be streamed in - failed for some other reason
					DropScheduledScenarioPed();
				}
			}

			

			if(numNew)
				break; // we created a ped, we're done here, break out
		}		
	}
	return numNew;
}

// Remove the top scheduled scenario ped from the queue and remove the artifical refs we put on it
void CPedPopulation::DropScheduledScenarioPed()
{
	if(!ms_scheduledScenarioPeds.IsEmpty())
	{
		ScheduledScenarioPed& scheduledPed = ms_scheduledScenarioPeds.Pop();

		if(scheduledPed.predeterminedPedType.m_PedModelIndex!=fwModelId::MI_INVALID)
		{
			// Remove our artificial refs
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(scheduledPed.predeterminedPedType.m_PedModelIndex)));
			pPedModelInfo->RemoveRef();
		}

		if (scheduledPed.chainUser)
		{
			CScenarioPointChainUseInfo* pUser = scheduledPed.chainUser;
			scheduledPed.chainUser = NULL; //dont need this any longer but dont delete the user info ... the chaining system will handle that ... 
			CScenarioChainingGraph::UnregisterPointChainUser(*pUser);
		}
	}	
}

#if 0

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
static s32 NEXT_PED_INDEX = 0;
static float MAX_DIST_TO_ATTRACT_PED = 5.0f;
static float MAX_DOT_TO_ATTRACT_PED = 0.707f;
bool CPedPopulation::AttractPedsToScenarioPointList(void)
{
	// First find the next wandering ped to try and attract to a scenario point
	CPed::Pool* PedPool = CPed::GetPool();
	s32 iPoolSize = PedPool->GetSize();
	CPed* pWanderingPed = NULL;

	for(s32 i = 0; i < iPoolSize; ++i)
	{
		s32 iIndex = i+NEXT_PED_INDEX;
		if(iIndex >= iPoolSize)
		{
			iIndex -= iPoolSize;
		}
		CPed* pPed = PedPool->GetSlot(iIndex);
		if(pPed && !pPed->PopTypeIsMission() &&
			pPed->GetPedIntelligence()->GetTaskDefault() &&
			pPed->GetPedIntelligence()->GetTaskDefault()->GetTaskType() == CTaskTypes::TASK_WANDER &&
			pPed->GetPedIntelligence()->GetTaskDefault() == pPed->GetPedIntelligence()->GetTaskActive() &&
			pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL)
		{
			NEXT_PED_INDEX = iIndex + 1;
			if(NEXT_PED_INDEX >= iPoolSize)
			{
				NEXT_PED_INDEX -= iPoolSize;
			}
			pWanderingPed = pPed;
			break;
		}
	}

	if(!pWanderingPed)
	{
		return 0;
	}

	const float fPedRadius = pWanderingPed->GetCapsuleInfo()->GetHalfWidth();
	const u32 uPedModelHash = pWanderingPed->GetBaseModelInfo()->GetHashKey();
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pWanderingPed->GetTransform().GetPosition());
	const Vector3 vPedDir = VEC3V_TO_VECTOR3(pWanderingPed->GetTransform().GetB());

	s32 count = SCENARIOPOINTMGR.GetNumScenarioPoints();
	for(s32 i = 0; i < count; ++i)
	{
		CEntity* pAttachedEntity = NULL;

		CScenarioPoint& temp = SCENARIOPOINTMGR.GetScenarioPoint(i);
		pAttachedEntity = temp.GetEntity();
		
		// Check if the effect is in use
		if( CPedPopulation::IsEffectInUse(temp) )
		{
			continue;
		}

		// IF the object is broken, don't spawn people on it.
		if(pAttachedEntity && pAttachedEntity->GetFragInst())
		{
			if(pAttachedEntity->GetFragInst()->GetPartBroken())
				continue;
		}

		// Ignore objects that have been broken up
		if(pAttachedEntity &&
			pAttachedEntity->GetIsTypeObject() &&
			((CObject*)pAttachedEntity)->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
		{
			continue;
		}

		if(!CScenarioManager::CheckScenarioPointEnabled(temp))
		{
			continue;
		}

		if(!CScenarioManager::CheckScenarioPointTimesAndProbability(temp))
		{
			continue;
		}

		const s32 scenarioType =(s32)temp.GetScenarioType();

		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

#if !__FINAL
		// Debug functionality for disabling certain scenarios, and ignoring that setting globally from within Rag
		if ( CScenarioManager::ms_bDisableScenariosWithNoClipData && !pScenarioInfo->HasAnimationData())
		{
			continue;
		}
#endif

		// Don't be attracted to specific scenario points with model overloads
		if(temp.GetModelIndex() != CScenarioPointManager::kNoCustomModelSet)
		{
			continue;
		}

		if(pScenarioInfo->GetBlockedModelSet() && pScenarioInfo->GetBlockedModelSet()->GetContainsModel(uPedModelHash))
		{
			continue;
		}

		if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::NoBulkyItems) && pWanderingPed->HasComponentWithFlagsSet(PV_FLAG_BULKY))
		{
			continue;
		}

		// Don't attract peds to scenarios that wont work in the rain when its raining
		if((pAttachedEntity == NULL && !temp.IsInteriorState(CScenarioPoint::IS_Inside)) ||
			(pAttachedEntity && !pAttachedEntity->m_nFlags.bInMloRoom))
		{
			bool bDontAttractInRain = g_weather.IsRaining() && pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnInRain);
			if (bDontAttractInRain && !temp.IsFlagSet(CScenarioPointFlags::IgnoreWeatherRestrictions))
			{
				continue;
			}
		}

		Assert(pScenarioInfo);

		if(!pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::WillAttractPeds))
		{
			continue;
		}

		if(pScenarioInfo->GetModelSet())
		{
			if (!pScenarioInfo->GetModelSet()->GetContainsModel(uPedModelHash))
			{
				continue;
			}
		}

		// get the distance from the player to the attractor
		Vector3 effectPos = VEC3V_TO_VECTOR3(temp.GetPosition());

		if(CScenarioBlockingAreas::IsPointInsideBlockingAreas(effectPos))
		{
			continue;
		}

		// ignore if too far away
		CPed* player = FindPlayerPed();
		if (player)
		{
			ScalarV distSqrd = DistSquared(VECTOR3_TO_VEC3V(effectPos), player->GetTransform().GetPosition());
			ScalarV maxDistSqrd(fAddRangeInViewMax * fAddRangeInViewMax);
			if (IsGreaterThanAll(distSqrd, maxDistSqrd))
			{
				continue;
			}
		}

		Vector3 vFromPedToPoint = effectPos - vPedPos;

		// Note: there used to be some code here for using a different distance and angle
		// threshold for scenario type 18 ("Ski left get on"). This might be an indication
		// of a need to specify the thresholds in the scenario data instead of using the same
		// values for all scenarios.

		if(vFromPedToPoint.Mag2() > rage::square(MAX_DIST_TO_ATTRACT_PED))
		{
			continue;
		}
		vFromPedToPoint.Normalize();

		if(vPedDir.Dot(vFromPedToPoint) < MAX_DOT_TO_ATTRACT_PED)
		{
			continue;
		}

		PF_START(CollisionChecks);
		if(!CScenarioManager::CheckScenarioPopulationCount(scenarioType, effectPos, CLOSEST_ALLOWED_SCENARIO_SPAWN_DIST))
		{
			PF_STOP(CollisionChecks);
			continue;
		}

		bool bValidPosition = true;
		// Check this position is free of objects(ignoring non-uprooted and this object)
		CEntity* pBlockingEntities[MAX_COLLIDED_OBJECTS] = {NULL, NULL, NULL, NULL, NULL, NULL};
		if(!CPedPlacement::IsPositionClearForPed(effectPos, fPedRadius, true, MAX_COLLIDED_OBJECTS, pBlockingEntities, true, false, true))
		{
			for(s32 i = 0;(pBlockingEntities[i] != NULL) &&(i < MAX_COLLIDED_OBJECTS); ++i)
			{
				// Ignore this entity
				if(pBlockingEntities[i] == pAttachedEntity)
				{
					continue;
				}
				//can only ignore objects
				if(!pBlockingEntities[i]->GetIsTypeObject())
				{
					bValidPosition = false;
					break;
				}
				// Ignore doors, for qnan matrix problem
				if(((CObject*)pBlockingEntities[i])->IsADoor())
				{
					continue;
				}
				// Ignore non-uprooted objects
				if(((CObject*)pBlockingEntities[i])->m_nObjectFlags.bHasBeenUprooted)
				{
					bValidPosition = false;
					break;
				}
			}
		}
		PF_STOP(CollisionChecks);
		if(!bValidPosition)
		{
			continue;
		}

		// Get rid of any held object
		if(pWanderingPed->GetWeaponManager() && pWanderingPed->GetWeaponManager()->GetEquippedWeaponObject())
		{
			pWanderingPed->GetWeaponManager()->EquipWeapon(pWanderingPed->GetDefaultUnarmedWeaponHash(), -1, true);
		}

		CTask* pScenarioTask = NULL;

		if (pScenarioInfo->GetIsClass<CScenarioSkiLiftInfo>())
		{
			Assertf(0, "Using Unsupported Ski Lift");
// 			CTaskSkiLiftScenario* pTask = rage_new CTaskSkiLiftScenario(scenarioType, &temp);
// 			pScenarioTask = pTask;
		}
		else if (pScenarioInfo->GetIsClass<CScenarioSniperInfo>() )
		{
			//Vector3 vDir = pEff->GetSpawnPoint()->GetSpawnPointDirection(pAttachedEntity);
			//float fHeading = rage::Atan2f(-vDir.x, vDir.y);
			CTaskUseScenario* pTask = rage_new CTaskUseScenario(scenarioType, &temp);
			//pTask->SetPositionAndHeading(effectPos, fHeading);
			pScenarioTask = pTask;
		}
		else if (pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
		{
			pScenarioTask = rage_new CTaskUseScenario( scenarioType, &temp);
		}

		if(pScenarioTask)
		{
			pWanderingPed->GetPedIntelligence()->AddTaskDefault(pScenarioTask);
		}

		return true;
	}

	return false;
}

#endif	// 0

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
u32 CPedPopulation::ChooseCivilianPedModelIndexForVehicle(CVehicle* pVeh, bool bDriver)
{
	Assert(pVeh);

	const CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)pVeh->GetBaseModelInfo();
//	s32	VehicleList = pModelInfo->GetVehicleList();

	// If the car is a gang car we will create appropriate gang members.
	if (pVeh->m_nVehicleFlags.bGangVeh)
	{
		const u32 vehGroup = (u32)pModelInfo->GetGangPopGroup();
		u32 pedGroup;

		// Find equivalent ped group with the same name as the vehicle ped group
		if(CPopCycle::GetPopGroups().FindPedGroupFromNameHash(CPopCycle::GetPopGroups().GetVehGroup(vehGroup).GetName(), pedGroup))
		{
			u32	gangPedMi = fwModelId::MI_INVALID;
			for (u32 i = 0; i < CPopCycle::GetPopGroups().GetNumPeds(pedGroup); ++i)
			{
				const u32 mi = CPopCycle::GetPopGroups().GetPedIndex(pedGroup, i);

				fwModelId modelId((strLocalIndex(mi)));
				if (CModelInfo::HaveAssetsLoaded(modelId) &&
					!CModelInfo::GetAssetsAreDeletable(modelId))
				{
					if (gangPedMi == fwModelId::MI_INVALID || fwRandom::GetRandomTrueFalse())		// If we already have a suitable model pick random one of the two
					{
						gangPedMi = mi;
					}
				}
			}
			if (gangPedMi != fwModelId::MI_INVALID)
			{
				return gangPedMi;
			}
		}
	}


	// For bikes we initially go for peds that have the right tickbox set.
	if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		CLoadedModelGroup BikeGroup;
		gPopStreaming.GetFallbackPedGroup(BikeGroup, false);

		BikeGroup.RemoveNonMotorcyclePeds();

// 		if (pVeh->GetModelIndex() == MI_BIKE_FAGGIO)
// 		{
// 			BikeGroup.RemovePedModelsThatAreGangMembers();						// Remove all gang members
// 		}

        bool forceMale = false;
        bool forceFemale = false;
        if (!bDriver)
        {
            if (pVeh->GetDriver())
            {
                if (pVeh->GetDriver()->IsMale())
                    forceFemale = true;
                else
                    forceMale = true;
            }
        }
        if (forceMale || forceFemale)
            BikeGroup.RemoveMaleOrFemale(forceMale);

        const u32 miForBike = PickModelFromGroupForInCar(BikeGroup, pVeh);
        return miForBike;
	}


	// Use a new strategy to create them.
	// Start with the ones that aren't used yet.
	// Then the ones that are only used once
	// Twice and then stop.

	// On the first pass we check for identical peds in the same car.
	// On the seconds pass we don't.


	CLoadedModelGroup Group;
	gPopStreaming.GetFallbackPedGroup(Group, true);

// 	if (pVeh->GetModelIndex() == MI_BIKE_FAGGIO)
// 	{
// 		Group.RemovePedModelsThatAreGangMembers();						// Remove all gang members
// 	}

	if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_RICH_CAR))
	{
		Group.RemovePedsThatCantDriveCarType(PED_DRIVES_RICH_CAR);
	}
	else if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_AVERAGE_CAR))
	{
		Group.RemovePedsThatCantDriveCarType(PED_DRIVES_AVERAGE_CAR);
	}
	else if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_POOR_CAR))
	{
		Group.RemovePedsThatCantDriveCarType(PED_DRIVES_POOR_CAR);
	}

	if (!bDriver)
	{
		Group.RemoveNonPassengerPeds();
	}

	Group.RemoveBulkyItemPeds();

	const u32 mi = PickModelFromGroupForInCar(Group, pVeh);

	if (CModelInfo::IsValidModelInfo(mi))
	{
		return mi;
	}

#if !__NO_OUTPUT
	if (!CModelInfo::IsValidModelInfo(Group.GetMember(0)))
	{
		popDebugf1("No civilian ped available to spawn in vehicle '%s'", pVeh->GetModelName());
	}
#endif

	return Group.GetMember(0);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PickModelFromGroupForInCar
// PURPOSE :	Determines if a certain car types has a certain specific ped
//				type that drives it and let's us know what it is..
// PARAMETERS :	carModelIndex - the car type we are interested in.
// RETURNS :	The driver peds model index.
/////////////////////////////////////////////////////////////////////////////////
u32 CPedPopulation::PickModelFromGroupForInCar(const CLoadedModelGroup& group, CVehicle* pVeh)
{
	const s32 NumMembers = group.CountMembers();

	bool lawVeh = pVeh->IsLawEnforcementCar();

	for(s32 Pass = 0; Pass < 2; ++Pass)
	{
		for(s32 Usage = 0; ((u32)Usage) <= ms_maxPedsForEachPedTypeAvailableInVeh; ++Usage)
		{
			for(s32 i = 0; i < NumMembers; ++i)
			{
				const u32 ModelIndex = group.GetMember(i);
				fwModelId modelId((strLocalIndex(ModelIndex)));
				Assert(CModelInfo::HaveAssetsLoaded(modelId));

				// Don't take into account the ones that are to be streamed out.
				if(	!CModelInfo::IsValidModelInfo(ModelIndex) ||
					CModelInfo::GetAssetsAreDeletable(modelId) ||
					//					(Usage != MAXUSAGE && ((CPedModelInfo*)CModelInfo::GetModelInfo (ModelIndex))->GetNumTimesOnFootAndInCar() != Usage))	// With this line we still get peds even when max usage has been reached. Don't want this
					(((CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId))->GetNumTimesOnFootAndInCar() != Usage))
				{
					continue;
				}

				CPedModelInfo* pmi = (CPedModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
				if (lawVeh && !CPedType::IsLawEnforcementType(pmi->GetDefaultPedType()))
					continue;

				// if this ped is already in the car we only pick it if this is the last iteration
				if(Pass == 1)
				{
					return ModelIndex;
				}

				bool bIdenticalPedInCar = false;
				for(s32 iSeat = 0; iSeat < pVeh->GetSeatManager()->GetMaxSeats(); iSeat++)
				{
					if(pVeh->GetSeatManager()->GetPedInSeat(iSeat) && pVeh->GetSeatManager()->GetPedInSeat(iSeat)->GetModelIndex() == ModelIndex)
					{
						bIdenticalPedInCar = true;
					}
				}

				if(!bIdenticalPedInCar)
				{
					return ModelIndex;
				}
			}
		}
	}
	return fwModelId::MI_INVALID;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindSpecificDriverModelForCarToUse
// PURPOSE :	Determines if a certain car types has a certain specific ped
//				type that drives it and let's us know what it is...
//				Also makes sure it picks the driver that's currently in memory if there are several
// PARAMETERS :	carModelIndex - the car type we are interested in.
// RETURNS :	The driver peds model index.
/////////////////////////////////////////////////////////////////////////////////
fwModelId CPedPopulation::FindSpecificDriverModelForCarToUse(fwModelId carModelId)
{
	fwModelId retVal;

	CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(carModelId);
	Assert(mi);
	for (u32 i = 0; i < mi->GetDriverCount(); ++i)
	{
		fwModelId driver((strLocalIndex(mi->GetDriver(i))));
		if (CModelInfo::HaveAssetsLoaded(driver))
			return driver;
	}

	if (mi->GetDriverCount() > 0)
	{
		retVal.SetModelIndex(mi->GetRandomDriver());
		return retVal;
	}

	Assertf(!CVehicle::IsLawEnforcementVehicleModelId(carModelId) && !CVehicle::IsTaxiModelId(carModelId), "Law enforcement or taxi vehicle %s has no driver set up!", mi->GetModelName());

	return retVal;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FindRandomDriverModelForCarToUse
// PURPOSE :	Returns a random driver model for the specified car, if it has any
// PARAMETERS :	carModelIndex - the car type we are interested in.
// RETURNS :	The driver peds model index.
/////////////////////////////////////////////////////////////////////////////////
fwModelId CPedPopulation::FindRandomDriverModelForCarToUse(fwModelId carModelId)
{
	fwModelId retVal;

	CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(carModelId);
	Assert(mi);
	if (mi && mi->GetDriverCount() > 0)
	{
		retVal.SetModelIndex(mi->GetRandomDriver());
		return retVal;
	}

	Assertf(!CVehicle::IsLawEnforcementVehicleModelId(carModelId) && !CVehicle::IsTaxiModelId(carModelId), "Law enforcement or taxi vehicle %s has no driver set up!", mi->GetModelName());

	return retVal;
}

bool CPedPopulation::CanSuppressAggressiveCleanupForPed(const CPed *REPLAY_ONLY(pPed))
{
	//! Note: Can possibly data drive this through peds.meta, but for now, bird deletion is too obvious during a replay.
#if GTA_REPLAY
	if(CReplayMgr::IsRecording() && pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsBird())
	{
		return true;
	}
#endif

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CanRemovePedAtPedsPosition
// PURPOSE :	TODO!
// PARAMETERS :	TODO!
// RETURNS :	If they can be removed safely (hopefully without being noticed
//				by the player).
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::CanRemovePedAtPedsPosition( CDynamicEntity* pPedOrDummyPed, float removalRangeScale, const Vector3& popCtrlCentre, bool popCtrlIsInInterior, float& cullRangeInView_Out)
{
	Assertf((pPedOrDummyPed->GetIsTypePed()), "RemovePedOrDummyPed - passed non ped or dummy ped entity");

	// Make sure removal at the location isn't blocked.
	if(PointFallsWithinNonRemovalArea(VEC3V_TO_VECTOR3(pPedOrDummyPed->GetTransform().GetPosition())))
	{
		return false;
	}

	// Check if they are in a potentially unsee-able interior (such as a subway
	// station when above ground).
	if(!popCtrlIsInInterior)
	{
		CPortalTracker* pPortalTracker = const_cast<CPortalTracker*>(pPedOrDummyPed->GetPortalTracker());
		Assert(pPortalTracker);
		const s32				roomIdx		= pPortalTracker->m_roomIdx;
		CInteriorInst*	pIntInst	= pPortalTracker->GetInteriorInst();
		if(pIntInst && (roomIdx > 0))
		{
			// Only consider subways as deep interiors - some interiors have rooms with no portals close to the entrance
			// and this would stop peds spawning
			const bool bConsideredDeepInterior = pIntInst->IsSubwayMLO();
			if(bConsideredDeepInterior && pIntInst->GetNumExitPortalsInRoom(roomIdx) == 0)
			{
				return true;
			}
		}
	}

	// Determine the cull ranges and cull types allowed.
	// Scheduled and persistent peds have bigger ranges (as they are more important).
	bool	allowCullingWhenOutOfView	= !ms_suppressAmbientPedAggressiveCleanupThisFrame;
	cullRangeInView_Out					= ms_cullRangeBaseInView * removalRangeScale;
	float	cullRangeOutOfView			= ms_cullRangeBaseOutOfView * removalRangeScale;
	if(pPedOrDummyPed->GetIsTypePed())
	{
		// Ped version.
		CPed* pPed = static_cast<CPed*>(pPedOrDummyPed);

		//! Check if ped isn't allowing aggressive out of view deletion.
		if(CanSuppressAggressiveCleanupForPed(pPed))
		{
			allowCullingWhenOutOfView = false;
		}
		
		// Caclulate the cull ranges for this ped
		CalculatePedCullRanges(pPed, popCtrlIsInInterior, allowCullingWhenOutOfView, cullRangeInView_Out, cullRangeOutOfView);
	}


	// Determine the distance to use for our cull calculations.
	const Vector3 vPedOrDummyPosition = VEC3V_TO_VECTOR3(pPedOrDummyPed->GetTransform().GetPosition());
	const Vector3 diff	(vPedOrDummyPosition - popCtrlCentre);
	// Spawning is based on XY distance, thus so also should removal be
	const float distSqr = diff.XYMag2();

	// Check the distance against our cull ranges.
	if(distSqr > (cullRangeInView_Out * cullRangeInView_Out))
	{
		// The ped is far enough away that we don't
		// care if they are in view or not, so just
		// try to remove it.
		return true;
	}
	else if((distSqr > (cullRangeOutOfView * cullRangeOutOfView)) && allowCullingWhenOutOfView)
	{
		// Check if they are visible or not.
		bool bOnScreen = false;

		// In DEV builds make this use the game cam for the visibility test even
		// when using the debug cam.  That will make debugging the behavior of
		// the system much easier.
#if __DEV
		static const float fTestRadius = 2.0f;
		const bool isUsingDebugCam = camInterface::GetDebugDirector().IsFreeCamActive();
		if(isUsingDebugCam && !ms_movePopulationControlWithDebugCam)
		{
			// Base the population control off the game cam (which ins't the one
			// attached to the current main view port).
			bOnScreen = camInterface::GetGameplayDirector().IsSphereVisible(vPedOrDummyPosition, fTestRadius);
		}
		else
#endif // __DEV
		{
			if(ms_fScriptedRangeMultiplierThisFrame > 1.0f)
			{
				// If script is artificially increasing the spawn range, then we can't use 'GetIsOnScreen()' which
				// will return false if an entity was not rendered due perhaps to LOD distance.
				bOnScreen = camInterface::IsSphereVisibleInGameViewport(vPedOrDummyPosition, pPedOrDummyPed->GetBoundRadius());
			}
			else
			{
				// Base the population control off the current (game or debug) cam.
				bOnScreen = pPedOrDummyPed->GetIsOnScreen(true);
			}
		}

		// Try to remove all out of view peds.
		if(!bOnScreen)
		{
			return true;
		}
	}

	// It did not fall into the removal ranges.
	return false;
}

void CPedPopulation::CalculatePedCullRanges(CPed* pPed, const bool popCtrlIsInInterior, bool& allowCullingWhenOutOfView_out, float& cullRangeinView_out, float& cullRangeOutOfView_out)
{
	Assert(pPed);
	if(pPed->PopTypeGet() == POPTYPE_RANDOM_SCENARIO) 		// || pPed->m_Persistence.m_ReasonForP ersistence == PED_PERSISTENT_HAS_SCHEDULE
	{
		//allowCullingWhenOutOfView_out	= false;

		// url:bugstar:1229651 - only prevent scenario peds from being removed closer by off-screen,
		// IFF we have enough room to create new ones in the budget.
		if(ms_bAlwaysAllowRemoveScenarioOutOfView)
		{
			allowCullingWhenOutOfView_out = true;
		}
		else
		{
			const s32 iScenarioDelta = GetDesiredMaxNumScenarioPeds(popCtrlIsInInterior) - (ms_nNumOnFootScenario+ms_nNumInVehScenario);
			allowCullingWhenOutOfView_out = (iScenarioDelta < ms_iMinNumSlotsLeftToAllowRemoveScenarioOutOfView);
		}

		float cullRangeInView = cullRangeinView_out;
		float cullRangeOutOfView = cullRangeOutOfView_out;
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SpawnedAtExtendedRangeScenario))
		{
			// In view spawn range is up to 120, so we need to be at least ~20 more than that.		
			cullRangeInView = Max(cullRangeInView, ms_cullRangeExtendedInViewMin);

			if(CScenarioManager::ShouldExtendExtendedRangeFurther())
			{
				// Boost up the in-view spawn distance, and allow us to remove beyond the regular
				// extended cull range if off screen.
				cullRangeOutOfView = Max(cullRangeOutOfView, cullRangeinView_out);
				allowCullingWhenOutOfView_out = true;
				cullRangeInView = Max(cullRangeInView, ms_addRangeExtendedScenarioExtended);
			}
		}

		cullRangeinView_out				= cullRangeInView;
		cullRangeOutOfView_out			= cullRangeOutOfView;
	}
	else if((pPed->GetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway ) || pPed->GetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway )) && CPedType::IsLawEnforcementType(pPed->GetPedType()))
	{
		cullRangeinView_out				*= ms_cullRangeScaleCops;
		cullRangeOutOfView_out			*= ms_cullRangeScaleCops;
	}
	else if(pPed->GetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway ) || pPed->GetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway ) || ms_suppressAmbientPedAggressiveCleanupThisFrame)
	{
		cullRangeinView_out				*= ms_cullRangeScaleSpecialPeds;
		cullRangeOutOfView_out			*= ms_cullRangeScaleSpecialPeds;
	}

	cullRangeinView_out		*= pPed->GetRemoveRangeMultiplier();
	cullRangeOutOfView_out	*= pPed->GetRemoveRangeMultiplier();
}

//				the number collected.  They will early out and return true if the 
//				array is full - hence to get the potential first one of each, you'd 
//				pass in 1.
//				If a size >0 is passed in, but no array, then this is a special case - meaning it will early out as
//				soon as anything matching - just returning the bool
// PARAMETERS :	minBounds - the min bounds of the area
//				maxBounds - the max bounds of the area
//				ppPeds - address of an array of ped pointers [&((ped*)*) = ped***] in (can be NULL but will
//						 just early out
//				maxPeds - maximum size of the array/max number to collect
//				pedCount - number of peds collected
//				[above same for vehicles]
// RETURNS :	true if it finds a cop from the options specified.  The arrays can
//				then be queried up to item count.
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::GetCopsInArea( const Vector3 &minBounds,					const Vector3 &maxBounds, 
									CPed*** ppPeds,			int maxPeds,		int &pedCount,
									CVehicle*** ppVehicles, int maxVehicles,	int &vehicleCount )
{
	bool bFound = false;

	vehicleCount = 0;
	s32 CarPoolSize = (s32) CVehicle::GetPool()->GetSize();

	for(s32 i=0; i<CarPoolSize && vehicleCount < maxVehicles ; i++)
	{
		CVehicle *pVehicle = CVehicle::GetPool()->GetSlot(i);
		if (pVehicle)
		{
			// Type of law enforcement vehicle
			if ( pVehicle->IsLawEnforcementVehicle() ) //&& (pVehicle->GetModelIndex() != MODELID_BOAT_PREDATOR) )
			{
				if (pVehicle->IsWithinArea(minBounds.x, minBounds.y, minBounds.z, maxBounds.x, maxBounds.y, maxBounds.z))
				{
					if ( !ppVehicles )
					{
						++vehicleCount;
						return true;
					}
					(*ppVehicles)[vehicleCount] = pVehicle;
					++vehicleCount;
					bFound = true;
				}
			}
		}
	}

	pedCount = 0;
	s32 pedPoolSize = CPed::GetPool()->GetSize();

	for(int p=0; p< pedPoolSize && pedCount < maxPeds; p++)
	{
		CPed * pPed = CPed::GetPool()->GetSlot(p);
		if(pPed)
		{
			if ( pPed->IsLawEnforcementPed() )
			{
				if (pPed->IsWithinArea(minBounds.x, minBounds.y, minBounds.z, maxBounds.x, maxBounds.y, maxBounds.z))
				{
					if ( !ppPeds )
					{
						++pedCount;
						return true;
					}
					(*ppPeds)[pedCount] = pPed;
					++pedCount;
					bFound = true;
				}
			}
		}
	}
	return bFound;
}

void CPedPopulation::InstantFillPopulation()
{
	if(!ms_bInstantFillPopulation)
		ms_bInstantFillPopulation = MAX_INSTANT_FILL;
}

void InstantRefillPopulation()
{
	CPedPopulation::RemoveAllRandomPeds();
	CPedPopulation::InstantFillPopulation();
}


void CPedPopulation::ManageSpecialStreamingConditions(u32 modelIndex)
{
	fwModelId modelId((strLocalIndex(modelIndex)));

	if (Verifyf(modelId.IsValid(), "Invalid ped model id!"))
	{
		CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

		if (Verifyf(pPedModelInfo, "Inavlid ped model info!"))
		{
			// Manage special aquatic ped conditions.
			CWildlifeManager& wlManager = CWildlifeManager::GetInstance();
			if (wlManager.IsAquaticModel(modelIndex) && wlManager.ShouldSpawnMoreAquaticPeds())
			{
				wlManager.ManageAquaticModelStreamingConditions(modelIndex);
			}
		}
	}
}

void CPedPopulation::OnPedSpawned(CPed& rPed, fwFlags8 uFlags)
{
	//Check if the ped is a random gang ped.
	if(rPed.PopTypeIsRandom() && rPed.IsGangPed())
	{
		//Check if the ped was spawned in the ambient population, or in a scenario using non-specific models.
		if(uFlags.IsFlagSet(OPSF_InAmbient) ||
			(uFlags.IsFlagSet(OPSF_InScenario) && !uFlags.IsFlagSet(OPSF_ScenarioUsedSpecificModels)) || 
			(uFlags.IsFlagSet(OPSF_InVehicle) && fwRandom::GetRandomTrueFalse()))
		{
			//Check if the inventory is valid.
			if(rPed.GetInventory())
			{
				//Remove all weapons except unarmed.
				rPed.GetInventory()->RemoveAll();
				rPed.GetInventory()->AddWeaponAndAmmo(rPed.GetDefaultUnarmedWeaponHash(), 0);
			}
		}
	}
}

bool CPedPopulation::ExistingPlayersWithSimilarPedGenRange(const Vector3 &spawnPosition)
{
	static const float EXISTING_NEARBY_PLAYER_DISTANCE = 30.0f; 
	const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];

	u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(spawnPosition, EXISTING_NEARBY_PLAYER_DISTANCE, closestPlayers, false);

	return numPlayersInRange > 0;
}

/////////////////////////////////////////////////////////////////////////////////
// name:		IsNetworkTurnToCreateScenarioPedAtPos
// description:	Returns whether it is the local players turn to create scenario ped
//              at the given position
/////////////////////////////////////////////////////////////////////////////////
bool CPedPopulation::IsNetworkTurnToCreateScenarioPedAtPos(const Vector3 &position)
{
	static const float SCENARIO_CREATION_PRIORITY_DISTANCE = 185.0f; //Less than CNetObjPed::GetScopeDistance MINIMUM_SCOPE_TO_EXCEED_EFFECTIVE_NETWORK_WORLD_GRIDSQUARE_RADIUS

	bool isPlayerTurn = false;
	CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();
	CNetObjPlayer *localNetObjPlayer = NULL;
	if(localPlayer && localPlayer->GetPlayerPed() && localPlayer->GetPlayerPed()->GetNetworkObject())
	{
		localNetObjPlayer  = SafeCast(CNetObjPlayer, localPlayer->GetPlayerPed()->GetNetworkObject());
	}

	if (localNetObjPlayer) 
	{
		if(localNetObjPlayer->IsInPostTutorialSessionChangeHoldOffPedGenTime())
		{
			return false;;
		}

		// Check if local player is already beyond SCENARIO_CREATION_PRIORITY_DISTANCE for this position
		Vector3 localPlayerPos = localNetObjPlayer->GetPosition();
		const float fDistToLocalPlayerSq = localPlayerPos.Dist2(position);
		if( fDistToLocalPlayerSq > SCENARIO_CREATION_PRIORITY_DISTANCE * SCENARIO_CREATION_PRIORITY_DISTANCE )
		{
			return false;
		}
	}

	const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
	u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(position, SCENARIO_CREATION_PRIORITY_DISTANCE, closestPlayers, false);

	if(numPlayersInRange == 0)
	{
		if(localNetObjPlayer)
		{
			localNetObjPlayer->SetLatestTimeCanSpawnScenarioPed(CNetObjPlayer::ALLOW_PLAYER_GENERATE_SCENARIO_PED);
		}
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

		const unsigned DURATION_OF_TURN       = 1000;
		const unsigned DURATION_BETWEEN_TURNS = 500;

		unsigned networkTime    = NetworkInterface::GetNetworkTime();
		unsigned currentTime    = networkTime;
		unsigned timeWithinTurn = currentTime % (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);
		currentTime /= (DURATION_OF_TURN + DURATION_BETWEEN_TURNS);

		unsigned currentTurn = currentTime % (numTurns);

		if((currentTurn == localPlayerTurn) && (timeWithinTurn <= DURATION_OF_TURN))
		{
			if(localNetObjPlayer)
			{
				unsigned latestTimeCanSpawnScenarioPed = networkTime + (DURATION_OF_TURN - timeWithinTurn);
				localNetObjPlayer->SetLatestTimeCanSpawnScenarioPed(latestTimeCanSpawnScenarioPed);
			}
			isPlayerTurn = true;
		}
		else
		{
			isPlayerTurn = false;
		}
	}

	return isPlayerTurn;
}


#if __DEV

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void VecMapDisplayPedPopulationEvent(const Vector3& pos, const Color32 baseColour)
{
	if(CDebugScene::bDisplayPedsOnVMap && CDebugScene::bDisplayPedPopulationEventsOnVMap)
	{
		CVectorMap::MakeEventRipple(	pos,
			20.0f,
			1000,
			baseColour);
	}
};
#endif // __DEV


#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CPedPopulation::DrawAddAndCullZones()
{
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

	const Vector3 vPopGenCenterEnd = vPopGenCenterStart + Vector3(0.0f, 0.0f, 10.0f);
	
	ms_popGenKeyholeShape.Draw(ms_displayAddCullZonesInWorld, ms_displayAddCullZonesOnVectorMap, vPopGenCenterStart, 1.0f);

	const float cullRangeOutOfView = ms_cullRangeBaseOutOfView * ms_CurrentCalculatedParams.fRemovalRangeScale;
	const float cullRangeInView = ms_cullRangeBaseInView * ms_CurrentCalculatedParams.fRemovalRangeScale;

	// Draw the cull ranges.
	if(ms_displayAddCullZonesInWorld)
	{
		grcDebugDraw::Cylinder(RCC_VEC3V(vPopGenCenterStart), RCC_VEC3V(vPopGenCenterEnd), cullRangeOutOfView, Color_orange, false, 1, 32);
		grcDebugDraw::Cylinder(RCC_VEC3V(vPopGenCenterStart), RCC_VEC3V(vPopGenCenterEnd), cullRangeInView, Color_red, false, 1, 32);
	}

	if(ms_displayAddCullZonesOnVectorMap)
	{
		CVectorMap::DrawCircle(vPopGenCenterStart, cullRangeOutOfView, Color32(1.0f, 0.65f, 0.0f, 0.5f), false);
		CVectorMap::DrawCircle(vPopGenCenterStart, cullRangeInView, Color32(1.0f, 0.0f, 0.0f, 0.5f), false);
	}

	// Draw the ped/dummy ped convert zone.
	if(!ms_useNumPedsOverConvertRanges)
	{
		const float		angleSwept			= TWO_PI;

		const float		wedgeRadiusInner	= ms_convertRangeBaseDummyToReal * ms_allZoneScaler;
		const float		wedgeRadiusOuter	= ms_convertRangeBaseRealToDummy * ms_allZoneScaler;
		const float		wedgeThetaStart		= ms_PopCtrl.m_baseHeading - (angleSwept / 2.0f);
		const float		wedgeThetaEnd		= wedgeThetaStart + angleSwept;
		const s32		wedgeNumSegments	= 15;
		const Color32	wedgeColour			(0x00,0x00,0xff,0x30);

		// Draw it.
		CPopGenShape::DrawDebugWedge(vPopGenCenterStart,
			wedgeRadiusInner,
			wedgeRadiusOuter,
			wedgeThetaStart,
			wedgeThetaEnd,
			wedgeNumSegments,
			wedgeColour,
			CPedPopulation::ms_displayAddCullZonesOnVectorMap,
			CPedPopulation::ms_displayAddCullZonesInWorld);
	}

	// Draw the non creation and non removal zones.
	{
		if(ms_nonRemovalAreaActive)
		{
			const Color32 nonRemovalAreaColour(0x00,0x00,0xff,0x30);
			CVectorMap::DrawRectAxisAligned(ms_nonRemovalAreaMin,
				ms_nonRemovalAreaMax,
				nonRemovalAreaColour,
				true);
		}

		if(ms_nonCreationAreaActive)
		{
			const Color32	nonCreationAreaColour(0xff,0xff,0x00,0x30);
			CVectorMap::DrawRectAxisAligned(ms_nonCreationAreaMin,
				ms_nonCreationAreaMax,
				nonCreationAreaColour,
				true);
		}
	}
}
void MiscDebugTests(const u32 UNUSED_PARAM(timeMS), const u32 UNUSED_PARAM(timeDeltaMs))
{
}
#endif // __BANK

#if __BANK
void CPedPopulation::PedMemoryBudgetMultiplierChanged()
{
	if (ms_pedMemoryBudgetMultiplier <= 0.25f)
		gPopStreaming.SetPedMemoryBudgetLevel(0);
	else if (ms_pedMemoryBudgetMultiplier <= 0.5f)
		gPopStreaming.SetPedMemoryBudgetLevel(1);
	else if (ms_pedMemoryBudgetMultiplier <= 0.75)
		gPopStreaming.SetPedMemoryBudgetLevel(2);
	else
		gPopStreaming.SetPedMemoryBudgetLevel(3);
}
void CPedPopulation::DebugSetForcedPopCenterFromCameraCB()
{
	ms_vForcePopCenter = camInterface::GetPos();
}

void CPedPopulation::DebugSetPopControlSphere()
{	
	ms_forcePopControlSphere = true;
	UseTempPopControlSphereThisFrame(ms_vForcePopCenter, ms_TempPopControlSphereRadius, CVehiclePopulation::ms_TempPopControlSphereRadius);		
}
void CPedPopulation::DebugClearPopControlSphere()
{
	ms_forcePopControlSphere = false;
	ClearTempPopControlSphere();
}

#endif // __BANK

void CPedPopulation::AddScenarioPointToEntityFromEffect(C2dEffect* effect, CEntity* entity)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
		return;
#endif

	if (!effect && !entity)
	{
		return;
	}

	// Ignore entities not the right way up-ish
	if(entity->GetTransform().GetC().GetZf() < ENTITY_SPAWN_UP_LIMIT)
	{
		return;
	}

//	if(taskVerifyf(SCENARIOPOINTMGR.GetNumScenarioPointsWorld() < SCENARIOPOINTMGR.GetMaxScenarioPointsWorld(),
//			"Too many entity points in CScenarioPoint pool, may need to increase ScenarioPointWorld in 'common/data/gameconfig.xml' (currently %d).",
//			SCENARIOPOINTMGR.GetMaxScenarioPointsWorld()))
	{
		const CSpawnPoint* spawnPoint = effect->GetSpawnPoint();
		Assert(spawnPoint);

		bool create = true;
		bool makeProbabilityOne = false;
		if(!SCENARIOINFOMGR.IsVirtualIndex(spawnPoint->iType))
		{
			const CScenarioInfo* scenInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(spawnPoint->iType);
			if (Verifyf(scenInfo, "Unable to find scenario info for index %u", spawnPoint->iType))
			{
				if (SCENARIOINFOMGR.IsScenarioTypeEnabled(spawnPoint->iType))
				{
					if (scenInfo->GetIsFlagSet(CScenarioInfoFlags::InstanceCreationTiedToSpawnProbability))
					{
						create =  (fwRandom::GetRandomNumberInRange(0.0f,1.0f) < scenInfo->GetSpawnProbability()) ? true : false;
						makeProbabilityOne = true;
					}
				}
				else
				{
					create = false;
				}
			}
		}

		if (create)
		{
			// Probably enough to fail an assert or spew errors here if the pool fills up. That's what
			// we do when loading scenario regions, so it doesn't really make sense that we would make
			// that tolerant if we are going to crash when the next entity tries to add something.
			if(!CScenarioPoint::_ms_pPool->GetNoOfFreeSpaces())
			{
#if __ASSERT
				popAssertf(0,	"CScenarioPoint pool is full (size %d).", CScenarioPoint::_ms_pPool->GetSize());
#else
				popErrorf(		"CScenarioPoint pool is full (size %d).", CScenarioPoint::_ms_pPool->GetSize());
#endif
				return;
			}

			CScenarioPoint* newPoint = CScenarioPoint::CreateFromPool(effect->GetSpawnPoint(), entity);
			CSpawnPointOverrideExtension* extOverride = entity->GetExtension<CSpawnPointOverrideExtension>();
			if (extOverride)
			{
				extOverride->ApplyToPoint(*newPoint);
			}

			if (makeProbabilityOne)
			{
				//if we have this flag and we got here we should be at 100 probability that we will spawn
				// stuff as the implementation of this flag is what is limiting usage in the world
				newPoint->SetProbabilityOverride(1.0f);
			}

			newPoint->MarkAsFromRsc(); //points created in this way are considered from resourced data
			SCENARIOPOINTMGR.AddEntityPoint(*newPoint);
		}
	}
}

void CPedPopulation::RemoveScenarioPointsFromEntity(CEntity* entity)
{
	if (!entity)
	{
		return;
	}

	SCENARIOPOINTMGR.DeletePointsWithAttachedEntity(entity);
}
void CPedPopulation::RemoveScenarioPointsFromEntities(const CEntity* const* entities, u32 numEntities)
{
	SCENARIOPOINTMGR.DeletePointsWithAttachedEntities(entities, numEntities);
}
// END DEBUG!!


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitWidgets
// PURPOSE :
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
#if __BANK

static int g_PedPercentageOverride = 0;
static char g_PedGroupOverride[64];

void SetPedGroupOverride()
{
	if(CPopCycle::HasValidCurrentPopAllocation())
	{
		u32 pedGroup;
		if(CPopCycle::GetPopGroups().FindPedGroupFromNameHash(atHashValue(g_PedGroupOverride), pedGroup))
			(const_cast<CPopSchedule*>(&CPopCycle::GetCurrentPopSchedule()))->SetOverridePedGroup(pedGroup, g_PedPercentageOverride);
	}
}

void FlushDestroyedPedCacheCB()
{
	CPedFactory::GetFactory()->ClearDestroyedPedCache();
}

void CPedPopulation::InitWidgets()
{
	bkBank* pExistingBankPointer = BANKMGR.FindBank("Ped Population");
	bkBank &bank = pExistingBankPointer ? *pExistingBankPointer : BANKMGR.CreateBank("Ped Population");

	bank.AddButton("Remove All Random Peds",									datCallback(RemoveAllRandomPeds));
	bank.AddButton("Remove All Random Peds (Except Focus)",					datCallback(RemoveAllRandomPedsExceptFocus));
	bank.AddButton("Remove All Peds (Except Focus)",					datCallback(RemoveAllPedsExceptFocus));
	bank.AddButton("Remove Only Focus Ped",								datCallback(RemoveOnlyFocusPed));
	bank.AddButton("Force Remove Peds !{Mission,Player}",				datCallback(RemoveAllPedsHardNotMission));
	bank.AddButton("Force Remove Peds !{Player}",						datCallback(RemoveAllPedsHardNotPlayer));
	bank.AddToggle("Toggle allow peds to be added",							&gbAllowPedGeneration, OnToggleAllowPedGeneration);
	bank.AddToggle("Toggle allow ambient peds to be added",					&gbAllowAmbientPedGeneration, OnToggleAllowPedGeneration);
	bank.AddSlider("Ped Population Frame Rate", &ms_iPedPopulationFrameRate, 1, 60, 1, UpdateMaxPedPopulationCyclesPerFrame);
	bank.AddSlider("Ped Population Cycles Per Frame", &ms_iPedPopulationCyclesPerFrame, 1, 10, 1, UpdateMaxPedPopulationCyclesPerFrame);
	bank.AddSlider("Ped Population Frame Rate MP", &ms_iPedPopulationFrameRateMP, 1, 60, 1, UpdateMaxPedPopulationCyclesPerFrame);
	bank.AddSlider("Ped Population Cycles Per Frame MP", &ms_iPedPopulationCyclesPerFrameMP, 1, 10, 1, UpdateMaxPedPopulationCyclesPerFrame);
	bank.AddSlider("Instant fill extra population cycles", &ms_fInstantFillExtraPopulationCycles, 0.0f, 20.0f, 1.0f);

	CPopZones::InitWidgets(bank);

	bank.PushGroup("Debug Tools", true);

	bank.AddToggle("Log created peds",							&CPedFactory::ms_bLogCreatedPeds);
	bank.AddToggle("Log destroyed peds",						&CPedFactory::ms_bLogDestroyedPeds);
	bank.AddToggle("Log alpha fade",							&m_bLogAlphaFade);

	bank.AddToggle("Print ped population debug",				&CPopCycle::sm_bDisplayDebugInfo);
	bank.AddToggle("Print simple ped population debug",			&CPopCycle::sm_bDisplaySimpleDebugInfo);
	bank.AddToggle("Print in vehicle dead debug",				&CPopCycle::sm_bDisplayInVehDeadDebugInfo);
	bank.AddToggle("Print in vehicle no pretend model",			&CPopCycle::sm_bDisplayInVehNoPretendModelDebugInfo);
	bank.AddToggle("Print scenario ped streaming debug",		&CPopCycle::sm_bDisplayScenarioDebugInfo);
	bank.AddButton("Instant fill population",					datCallback(InstantFillPopulation));
	bank.AddButton("Remove all & instant refill population",	datCallback(InstantRefillPopulation));
	bank.AddToggle("Display ped population failure counts",		&CPopCycle::sm_bDisplayPedPopulationFailureCounts);
	bank.AddToggle("Display wandering ped population failure counts vector map", &CPopCycle::sm_bDisplayWanderingPedPopulationFailureCountsOnVectorMap);
	bank.AddToggle("Display scenario ped population failure counts vector map", &CPopCycle::sm_bDisplayScenarioPedPopulationFailureCountsOnVectorMap);
	bank.AddToggle("Display scenario ped population failure counts in the world", &CPopCycle::sm_bDisplayScenarioPedPopulationFailuresInTheWorld);
	bank.AddToggle("Display scenario vehicle population failure counts vector map", &CPopCycle::sm_bDisplayScenarioVehiclePopulationFailureCountsOnVectorMap);
#if SCENARIO_DEBUG
	bank.AddToggle("Display scenario model sets vector map", &CScenarioDebug::ms_bDisplayScenarioModelSetsOnVectorMap);
#endif // SCENARIO_DEBUG
	bank.AddToggle("Display ped population failure counts debug spew",		&CPopCycle::sm_bDisplayPedPopulationFailureCountsDebugSpew);
	bank.PushGroup("Spawn failure toggles");
		for( int i = 0; i < CPedPopulation::PedPopulationFailureData::FR_Max; i++ )
		{
			CPedPopulation::PedPopulationFailureData::FailureInfo* pFailureInfo = CPedPopulation::ms_populationFailureData.GetFailureEnumDebugInfo((CPedPopulation::PedPopulationFailureData::FailureType)i);
			if( pFailureInfo )
			{
				bank.AddToggle(pFailureInfo->m_szName, &pFailureInfo->m_bActive);
			}
		}
	bank.PopGroup();
	
	bank.AddToggle("Enable destroyed ped cache",				&CPedFactory::ms_reuseDestroyedPeds);
	bank.AddToggle("Print destroyed ped cache",					&CPedFactory::ms_reuseDestroyedPedsDebugOutput);
	bank.AddButton("Flush destroyed ped cache",					FlushDestroyedPedCacheCB);
#if __DEV
	bank.AddToggle("Display peds (on VectorMap)",				&CDebugScene::bDisplayPedsOnVMap);
	bank.AddToggle("Rotate VM with Player",						&CVectorMap::m_bRotateWithPlayer);
	bank.AddToggle("Rotate VM with Camera",						&CVectorMap::m_bRotateWithCamera);
	bank.AddToggle("Display spawn point raw density (on VM)",	&CDebugScene::bDisplaySpawnPointsRawDensityOnVMap);
	bank.AddToggle("Display ped population events (on VM)",		&CDebugScene::bDisplayPedPopulationEventsOnVMap);
	bank.AddToggle("Display peds to stream out (on VM)",		&CDebugScene::bDisplayPedsToBeStreamedOutOnVMap);
	bank.AddToggle("Display candidate scenario points (on VM)",	&CDebugScene::bDisplayCandidateScenarioPointsOnVMap);
#endif // __DEV

	bank.AddToggle("Move Population Control With Debug Cam",	&ms_movePopulationControlWithDebugCam);
	bank.AddToggle("Focus Ped Break On Removal Test",			&ms_focusPedBreakOnPopulationRemovalTest);
	bank.AddToggle("Focus Ped Break On Removal",				&ms_focusPedBreakOnPopulationRemoval);
	bank.AddToggle("Focus Ped Break On Attempted Conversion",	&ms_focusPedBreakOnAttemptedPopulationConvert);
	bank.AddToggle("Focus Ped Break On Conversion",				&ms_focusPedBreakOnPopulationConvert);

	bank.AddToggle("Display Misc Debug Tests",					&ms_displayMiscDegbugTests);
	bank.AddToggle("Display Add and Cull Zones (on VM)",		&ms_displayAddCullZonesOnVectorMap);
	bank.AddToggle("Display Add and Cull Zones (in World)",		&ms_displayAddCullZonesInWorld);
	bank.AddSlider("Add/Cull zone height offset (in World)",	&ms_addCullZoneHeightOffset, -500.0f, 500.0f, 1.0f);
	bank.AddToggle("Display Creation Pos Attempts 3D",			&ms_displayCreationPosAttempts3D);
	bank.AddToggle("Display Converted Ped Lines (on VM)",		&ms_displayConvertedPedLines);
	bank.AddToggle("Display Conversion Desire (on VM)",			&ms_displayPedDummyStateContentness);
	bank.AddToggle("Force scripted population origin override", &ms_forceScriptedPopulationOriginOverride);
	bank.AddToggle("Allow Network Ped Population Group Cycling",&ms_allowNetworkPedPopulationGroupCycling);
	bank.PushGroup("PedGroup override", true);
	bank.AddText("PedGroup override", g_PedGroupOverride, sizeof(g_PedGroupOverride), false);
	bank.AddSlider("Percentage override", &g_PedPercentageOverride, 0, 100, 1);
	bank.AddButton("Override", datCallback(SetPedGroupOverride));
	bank.PopGroup();

	bank.PushGroup("Ped Debug Tests", false);
	bank.AddButton("Remove all random peds",					RemoveAllRandomPeds);
	bank.PopGroup();
	bank.PopGroup();

	bank.PushGroup("Ped Creation Control", false);
#if __BANK
	bank.AddSlider("Pop Cycle Override Num Ambient Peds",		&ms_popCycleOverrideNumberOfAmbientPeds,	-1, 5000, 10);
	bank.AddSlider("Pop Cycle Override Num Scenario Peds",		&ms_popCycleOverrideNumberOfScenarioPeds,	-1, 5000, 10);
	bank.AddToggle("Create Peds Anywhere",						&ms_createPedsAnywhere);
	bank.AddToggle("Promote Density 1 -> Density 2",			&CPathServerAmbientPedGen::ms_bPromoteDensityOneToDensityTwo);
	
	bank.AddToggle("Visualise Spawn Points",					&ms_bVisualiseSpawnPoints);

	bank.AddToggle("ms_bPerformSpawnCollisionTest",				&ms_bPerformSpawnCollisionTest);
	bank.AddToggle("ms_bDrawSpawnCollisionFailures",			&ms_bDrawSpawnCollisionFailures);

#endif // __DEV
	bank.AddSlider("Ped memory budget multiplier",				&ms_pedMemoryBudgetMultiplier,				0.f, 1.f, 0.25f, PedMemoryBudgetMultiplierChanged);
#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	bank.AddSlider("Ped memory budget multiplier NG",			&ms_pedMemoryBudgetMultiplierNG,			0.f, 10.f, 0.05f, PedMemoryBudgetMultiplierChanged);
#endif // (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	bank.AddSlider("Pop Cycle Max Population Scaler",			&ms_popCycleMaxPopulationScaler,			0.0f, 50.0f, 0.1f);
	bank.AddToggle("Allow Reviving Persistent Peds",			&ms_allowRevivingPersistentPeds);
	bank.AddToggle("Allow Spawn Point Peds",					&ms_allowScenarioPeds);
	bank.AddToggle("Allow Ambient Peds",						&ms_allowAmbientPeds);
	bank.AddToggle("Allow Random Cops (Ambient)",				&ms_allowCreateRandomCops);
	bank.AddToggle("Allow Random Cops (Scenarios)",				&ms_allowCreateRandomCopsOnScenarios);
	bank.AddSlider("Max Peds For Each PedType On Foot",			&ms_maxPedsForEachPedTypeAvailableOnFoot,	0, 100, 1);
	bank.AddSlider("Max Peds For Each PedType In Veh",			&ms_maxPedsForEachPedTypeAvailableInVeh,	0, 100, 1);

	bank.AddSlider("Max Create Attempts Per Frame On Init",		&ms_maxPedCreationAttemptsPerFrameOnInit,	0, 1000, 10);
	bank.AddSlider("Max Create Attempts Per Frame Normal",		&ms_maxPedCreationAttemptsPerFrameNormal,	0, 1000, 10);

	bank.AddSlider("Max Scenario Create Attempts Per Frame On Init", &ms_maxScenarioPedCreationAttempsPerFrameOnInit, 0, 1000, 1);
	bank.AddSlider("Max Scenario Create Attempts Per Frame Normal", &ms_maxScenarioPedCreationAttempsPerFrameNormal, 0, 1000, 1);
	bank.AddSlider("Max Scenario Peds Created Per Frame", &ms_maxScenarioPedCreatedPerFrame, 0, 1000, 1);
	bank.AddSlider("Min Ped Slots Free To Spawn Scenario Ped", &ms_minPedSlotsFreeForScenarios, 0, 40, 1);
	bank.AddSlider("Default Ms Ahead To End Forced Population Init", &ms_DefaultMaxMsAheadForForcedPopInit, 0, 5000, 1);
	bank.AddToggle("Don\'t Fade During Startup Mode", &ms_bDontFadeDuringStartupMode);
	bank.AddSlider("Min Fade Distance", &ms_fMinFadeDistance, 0.001f, 200.0f, 0.0001f);
	// Somewhat misplaced, arguably:
	bank.AddSlider("Time After Collision Check Failure",		&CScenarioPoint::sm_FailedCollisionDelay, 0, 127, 1);
	bank.AddSlider("Min Time Between Priority Removal Of Ambient Ped From Scenario", &ms_minTimeBetweenPriRemovalOfAmbientPedFromScenario, 0, 100000, 500);
	bank.PopGroup();


	bank.PushGroup("Blocking Areas", false);
	bank.AddToggle("Wander Blocking Areas", &CPathServer::ms_bDrawPedGenBlockingAreas);
	bank.AddToggle("Spawn Blocking Areas", &CPathServer::ms_bDrawPedSwitchAreas);
	bank.AddToggle("Scenario Blocking Areas", &CScenarioBlockingAreas::ms_bRenderScenarioBlockingAreas);
	bank.AddToggle("Non-Creation Area", &ms_bDrawNonCreationArea);
	bank.AddButton("Dump Scenario Blocking Area Debug Info", datCallback(CFA(CScenarioBlockingAreas::DumpDebugInfo)));
	bank.AddButton("Add Blocking Area Around Player", datCallback(CFA(CScenarioBlockingAreas::AddAroundPlayer)));
	bank.AddButton("Clear All Blocking Areas", datCallback(CFA(CScenarioBlockingAreas::ClearAll)));
	bank.PopGroup();

	/*
	bank.PushGroup("Peds-per-meter density lookup");
	bank.AddSlider("Density 0", &m_fPedsPerMeterDensityLookup[0], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 1", &m_fPedsPerMeterDensityLookup[1], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 2", &m_fPedsPerMeterDensityLookup[2], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 3", &m_fPedsPerMeterDensityLookup[3], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 4", &m_fPedsPerMeterDensityLookup[4], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 5", &m_fPedsPerMeterDensityLookup[5], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 6", &m_fPedsPerMeterDensityLookup[6], 0.0f, 1.0f, 0.001f);
	bank.AddSlider("Density 7", &m_fPedsPerMeterDensityLookup[7], 0.0f, 1.0f, 0.001f);
	bank.PopGroup();
	*/

	bank.PushGroup("Ped Add/Cull Ranges", false);
	bank.AddSlider("All Zone Scaler",							&ms_allZoneScaler,							0.0f, 10.0f, 0.05f);
	bank.AddSlider("Debug scripted range multiplier",			&m_fDebugScriptedRangeMultiplier,			0.0f, 10.0f, 0.1f);
	bank.AddSlider("Range Scale In Shallow Interior",			&ms_rangeScaleShallowInterior,				0.0f, 10.0f, 0.1f);
	bank.AddSlider("Range Scale In Deep Interior",				&ms_rangeScaleDeepInterior,					0.0f, 10.0f, 0.1f);

	bank.AddSlider("Add Range Out Of View Min",					&ms_addRangeBaseOutOfViewMin,				0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range Out Of View Max",					&ms_addRangeBaseOutOfViewMax,				0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range In View Min",						&ms_addRangeBaseInViewMin,					0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range In View Max",						&ms_addRangeBaseInViewMax,					0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range Frustum Extra",					&ms_addRangeBaseFrustumExtra,				0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range Group Offset",					&ms_addRangeBaseGroupExtra,					0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range Extended Scenario (Normal)",		&ms_addRangeExtendedScenarioNormal,			0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Add Range Extended Scenario (Extended)",	&ms_addRangeExtendedScenarioExtended,		0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Short Range Max Spawn dist",				&ms_maxRangeShortRangeScenario,				0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Short Range And High Priority Max Spawn dist", &ms_maxRangeShortRangeAndHighPriorityScenario, 0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Short Range Min Spawn Dist Onscreen",		&ms_minRangeShortRangeScenarioInView,		0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Short Range Min Spawn Dist Offscreen",		&ms_minRangeShortRangeScenarioOutOfView,	0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Speed Based Add Range Scale",				&ms_addRangeScaleInViewSpeedBasedMax,		0.0f, 10.0f, 0.05f);

	bank.AddSlider("High Pri Scenario: Add Range Out Of View Min",	&ms_addRangeHighPriScenarioOutOfViewMin,	0.0f, 1000.0f, 1.0f);
	bank.AddSlider("High Pri Scenario: Add Range Out Of View Max",	&ms_addRangeHighPriScenarioOutOfViewMax,	0.0f, 1000.0f, 1.0f);
	bank.AddSlider("High Pri Scenario: Add Range In View Min",		&ms_addRangeHighPriScenarioInViewMin,		0.0f, 1000.0f, 1.0f);
	bank.AddSlider("High Pri Scenario: Add Range In View Max",		&ms_addRangeHighPriScenarioInViewMax,		0.0f, 1000.0f, 1.0f);
	bank.AddToggle("Always allow to remove scenario peds out of view", &ms_bAlwaysAllowRemoveScenarioOutOfView);
	bank.AddSlider("Allow scenario removal out of view when less than num slots available ", &ms_iMinNumSlotsLeftToAllowRemoveScenarioOutOfView, 0, 20, 1);

	bank.AddToggle("Use Prefd Num Method For Cop Removal",		&ms_usePrefdNumMethodForCopRemoval);
	bank.AddSlider("Cull Range Out Of View",					&ms_cullRangeBaseOutOfView,					0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Cull Range In View",						&ms_cullRangeBaseInView,					0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Cull Range Special Peds",					&ms_cullRangeScaleSpecialPeds,				0.0f, 10.0f, 0.1f);
	bank.AddSlider("Cull Range Cops",							&ms_cullRangeScaleCops,						0.0f, 10.0f, 0.1f);
	bank.PopGroup();

	bank.PushGroup("Ped Dummy Convert Method/Ranges", false);
	bank.AddToggle("Allow Dummy COnversions",					&ms_allowDummyCoversions);
	bank.AddToggle("Use Num Peds Over Convert Ranges",			&ms_useNumPedsOverConvertRanges);
	bank.AddToggle("Use Task Based Convert Ranges",				&ms_useTaskBasedConvertRanges);
	bank.AddToggle("Use Hard dummy conversion on low framerate",&ms_useHardDummyLimitOnLowFrameRate);
	bank.AddToggle("Use Hard dummy conversion all the time",	&ms_useHardDummyLimitAllTheTime);

	bank.AddSlider("Preferred Num Real Peds Around Player",		&ms_preferredNumRealPedsAroundPlayer,		0, 100, 1);
	bank.AddSlider("Low frame rate Num Real Peds Around Player",&ms_lowFrameRateNumRealPedsAroundPlayer,	0, 100, 1);
	bank.AddSlider("Convert Range Real Ped Hard Radius",		&ms_convertRangeRealPedHardRadius,			0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Convert Range Dummy To Real",				&ms_convertRangeBaseDummyToReal,			0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Convert Range Real To Dummy",				&ms_convertRangeBaseRealToDummy,			0.0f, 1000.0f, 1.0f);
	bank.AddSlider("Conversion Delay Ms Civ Refresh Min",		&ms_conversionDelayMsCivRefreshMin,			0, 30000, 100);
	bank.AddSlider("Conversion Delay Ms Civ Refresh Max",		&ms_conversionDelayMsCivRefreshMax,			0, 30000, 100);
	bank.AddSlider("Conversion Delay Ms Emergency Refresh Min",	&ms_conversionDelayMsEmergencyRefreshMin,	0, 30000, 100);
	bank.AddSlider("Conversion Delay Ms Emergency Refresh Max",	&ms_conversionDelayMsEmergencyRefreshMax,	0, 30000, 100);
	bank.PopGroup();

	bank.PushGroup("Ped Removal", false);
	bank.AddSlider("Speed Based Removal Rate Scale",			&ms_maxSpeedBasedRemovalRateScale,			0.0f, 40.0f, 0.05f);
	bank.AddSlider("Speed Max Expected",						&ms_maxSpeedExpectedMax,					0.0f, 500.0f, 5.0f);
	bank.AddSlider("Removal Delay Ms Civ Refresh Min",			&ms_removalDelayMsCivRefreshMin,			0, 30000, 100);
	bank.AddSlider("Removal Delay Ms Civ Refresh Max",			&ms_removalDelayMsCivRefreshMax,			0, 30000, 100);
	bank.AddSlider("Removal Delay Ms Emergency Refresh Min",	&ms_removalDelayMsEmergencyRefreshMin,		0, 30000, 100);
	bank.AddSlider("Removal Delay Ms Emergency Refresh Max",	&ms_removalDelayMsEmergencyRefreshMax,		0, 30000, 100);
	bank.AddSlider("Removal Cluster Max Peds In Radius",		&ms_removalMaxPedsInRadius,					0, 100, 1);
	bank.AddSlider("Removal Cluster Max Range",					&ms_removalClusterMaxRange,					0.f, 500.f, 0.25f);
	bank.AddSlider("Ped Removal Timeslicing",					&ms_pedRemovalTimeSliceFactor,			1.0f, 5.0f, 0.5f);
	bank.AddSlider("Peds Removed Per Frame",					&ms_numPedsToDeletePerFrame	,			1, 20, 1);
	bank.AddSlider("Max Dead Peds Around Player (MP)",			&ms_maxDeadPedsAroundPlayerInMP,			1, 15, 1);
	bank.AddSeparator();
	bank.AddToggle("Extend delay within outer radius",			&ms_extendDelayWithinOuterRadius);
	bank.AddSlider("Extend delay time (ms)",					&ms_extendDelayWithinOuterRadiusTime, 0, 60000, 1000);
	bank.AddSlider("Extend delay extra dist",					&ms_extendDelayWithinOuterRadiusExtraDist, -100.0f, 100.0f, 1.0f);
	bank.PopGroup();

	bank.AddToggle("Show Debug Log", &ms_showDebugLog, NullCB);
	bank.AddToggle("Draw excessive removal MP",					&CVehiclePopulation::ms_bDrawMPExcessivePedDebug);

	bank.PushGroup("Ped Reuse");
	{
		bank.AddToggle("Allow Ped Reuse",								&ms_usePedReusePool);
		bank.AddToggle("Debug Ped Reuse",								&ms_bDebugPedReusePool);
		bank.AddToggle("Display Ped Reuse Pool",							&ms_bDisplayPedReusePool);
		bank.AddSlider("Reused Ped Timeout",							&ms_ReusedPedTimeout, 1, 240, 1);
		bank.AddButton("Flush Ped Reuse Pool",							datCallback(FlushPedReusePool));
		bank.AddToggle("Ped Reuse in MP",								&ms_bPedReuseInMP);
		bank.AddToggle("Check HasBeenSeen",								&ms_bPedReuseChecksHasBeenSeen);
	}
	bank.PopGroup();

	bank.PushGroup("Pop Control Sphere");
	{
		bank.AddButton("Set Pop Control Sphere Center from Camera", datCallback(DebugSetForcedPopCenterFromCameraCB));
		bank.AddButton("Set Pop Control Sphere", datCallback(DebugSetPopControlSphere));
		bank.AddButton("Clear Pop Control Sphere", datCallback(DebugClearPopControlSphere));
		bank.AddSlider("Population Sphere Center", &ms_vForcePopCenter, -16000.f,16000.f,1.f); 
		bank.AddSlider("Population Sphere Radius", &ms_TempPopControlSphereRadius, 0.f,250.f,1.f);
		bank.AddSlider("Veh Pop Sphere Radius", &CVehiclePopulation::ms_TempPopControlSphereRadius, 0.f,400.f,1.f);
		bank.AddToggle("Load Collision Around Pop Center", &ms_bLoadCollisionAroundForcedPopCenter); 
	}
	bank.PopGroup();

	bkBank* pBank = BANKMGR.FindBank("Optimization");
	if(pBank)
	{
		pBank->AddSlider("Debug Override Ped Density Multiplier",	&ms_debugOverridePedDensityMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Ped Density Multiplier",					&ms_pedDensityMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Scenario Ped Interior Density Multiplier",&ms_scenarioPedInteriorDensityMultiplier, 0.0f, 10.0f, 0.1f);
		pBank->AddSlider("Scenario Ped Exterior Density Multiplier",&ms_scenarioPedExteriorDensityMultiplier, 0.0f, 10.0f, 0.1f);
	}
}

// FUNCTION : GetFailString
// PURPOSE : Returns total number of vehicle gen failures, and formats a string containing them
// PARAMETERS : failString must point to a text buffer large enough to accommodate failure string

s32 CPedPopulation::GetFailString(char * failString, bool bScenario, bool bNonScenario)
{
	char text[128];

	s32 iTotalFails = 0;
	s32 i=0;
	while(i < PedPopulationFailureData::FR_Max)
	{
		if( ((ms_populationFailureData.m_aFailureInfos[i].m_bScenarioSpecific && bScenario) ||
			(!ms_populationFailureData.m_aFailureInfos[i].m_bScenarioSpecific && bNonScenario)) &&
			ms_populationFailureData.m_aFailureInfos[i].m_iCount > 0)
		{
			iTotalFails += ms_populationFailureData.m_aFailureInfos[i].m_iCount;

			sprintf(text, "%s(%i) ", ms_populationFailureData.m_aFailureInfos[i].m_szShortName, ms_populationFailureData.m_aFailureInfos[i].m_iCount);
			strcat(failString, text);
		}
		i++;
	}

	return iTotalFails;
}

void CPedPopulation::RenderDebug()
{
	if(ms_displayAddCullZonesOnVectorMap || ms_displayAddCullZonesInWorld)
	{
		DrawAddAndCullZones();
	}

	if(ms_bDisplayMultipliers)
	{
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		Vector3 camPos = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

		char debugText[256];

		//-------------------------------------------------------------------------------
		// Display multipliers
		// Also lets display what spacing values that equates to for density 1 and 15

		Vector2 vPixelSize( 1.0f / ((float)grcDevice::GetWidth()), 1.0f / ((float)grcDevice::GetHeight()) );	
		Vector2 vTextPos( vPixelSize.x * 56.0f, 1.0f );
		vTextPos.y = (vPixelSize.y * ((float)grcDebugDraw::GetScreenSpaceTextHeight())) * 4.0f;

		s32 iPopCycleMaxAmbientPeds, iPopCycleMaxAmbientCopPeds, iPopCycleTempMaxAmbientPeds;
		s32 iDesiredMaxNumAmbientPeds = GetDesiredMaxNumAmbientPeds(&iPopCycleMaxAmbientPeds, &iPopCycleMaxAmbientCopPeds, &iPopCycleTempMaxAmbientPeds);

		float fScriptPopAreaAmbientMultiplier, fPedMemoryBudgetMultiplier;
		HowManyMoreAmbientPedsShouldBeGenerated(&fScriptPopAreaAmbientMultiplier, &fPedMemoryBudgetMultiplier);

		float fWeatherMultiplier, fHighwayMultiplier, fInteriorMultiplier, fWantedMultiplier, fCombatMultiplier, fPedDensityMultiplier;
		CPopCycle::GetAmbientPedMultipliers(&fWeatherMultiplier, &fHighwayMultiplier, &fInteriorMultiplier, &fWantedMultiplier, &fCombatMultiplier, &fPedDensityMultiplier);
		float fTotalAmbientPedMultiplier = fWeatherMultiplier * fHighwayMultiplier * fInteriorMultiplier * fWantedMultiplier * fCombatMultiplier * fPedDensityMultiplier;

		sprintf(debugText, "NUM DESIRED AMBIENT PEDS = %i/%i (current num ambient on foot: %i) (pop cycle max scalar: %.1f)", iDesiredMaxNumAmbientPeds, CPopCycle::GetCurrentPopAllocation().GetMaxNumAmbientPeds(), ms_nNumOnFootAmbient, CPedPopulation::GetPopCycleMaxPopulationScaler());
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "  MULTIPLIERS Total = %.1f (Weather: %.1f, Highway: %.1f, Interior: %.1f, Wanted: %.1f, Combat: %.1f, PedDensity: %.1f, ScriptPop: %.1f, PedBudget: %.1f)",
			fTotalAmbientPedMultiplier, fWeatherMultiplier, fHighwayMultiplier, fInteriorMultiplier, fWantedMultiplier, fCombatMultiplier, fPedDensityMultiplier, fScriptPopAreaAmbientMultiplier, fPedMemoryBudgetMultiplier);
		grcDebugDraw::Text( vTextPos, (fTotalAmbientPedMultiplier == 1.0f) ? Color_NavyBlue : Color_turquoise, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		float fPopAreaMult = CThePopMultiplierAreas::GetPedDensityMultiplier(camPos, true);
		char popMultTxt[64] = { ' ', 0 };
		if(fPopAreaMult != 1.0f)
		{
			sprintf(popMultTxt, "INSIDE POP-MULT AREA (x%.1f)", fPopAreaMult);
		}

		sprintf(debugText, "  SCRIPTED RANGE MULTIPLIER : %.1f %s", GetScriptedRangeMultiplierThisFrame(), popMultTxt);
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);

		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		s32 iPopCycleTempMaxScenarioPeds;
		s32 iDesiredMaxNumScenarioPedsForInteriors = GetDesiredMaxNumScenarioPeds(true, &iPopCycleTempMaxScenarioPeds);
		s32 iDesiredMaxNumScenarioPedsForOutside = GetDesiredMaxNumScenarioPeds(false);

		sprintf(debugText, "NUM DESIRED SCENARIO PEDS = (Interior: %i, Exterior: %i) / %i (current num: %i)", iDesiredMaxNumScenarioPedsForInteriors, iDesiredMaxNumScenarioPedsForOutside, CPopCycle::GetCurrentPopAllocation().GetMaxNumScenarioPeds(), ms_nNumOnFootScenario);
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		float fScenarioInteriorTotalMult = ms_scenarioPedInteriorDensityMultiplier * ms_scriptScenarioPedInteriorDensityMultiplierThisFrame * fHighwayMultiplier * fInteriorMultiplier * fWantedMultiplier * fCombatMultiplier * fPedDensityMultiplier * fScriptPopAreaAmbientMultiplier * fPedMemoryBudgetMultiplier;
		float fScenarioExteriorTotalMult = ms_scenarioPedExteriorDensityMultiplier * ms_scriptScenarioPedExteriorDensityMultiplierThisFrame * fHighwayMultiplier * fInteriorMultiplier * fWantedMultiplier * fCombatMultiplier * fPedDensityMultiplier * fScriptPopAreaAmbientMultiplier * fPedMemoryBudgetMultiplier;

		sprintf(debugText, "  MULTIPLIERS Totals = Interior: %.1f, Exterior: %.1f (InteriorDensity: %.1f, Scripted: %.1f), (ExteriorDensity: %.1f, Scripted: %.1f)",
			fScenarioInteriorTotalMult, fScenarioExteriorTotalMult, ms_scenarioPedInteriorDensityMultiplier, ms_scriptScenarioPedInteriorDensityMultiplierThisFrame, ms_scenarioPedExteriorDensityMultiplier, ms_scriptScenarioPedExteriorDensityMultiplierThisFrame);
		grcDebugDraw::Text( vTextPos, (fScenarioInteriorTotalMult == 1.0f && fScenarioExteriorTotalMult == 1.0f) ? Color_NavyBlue : Color_turquoise, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "                       (Highway: %.1f, Wanted: %.1f, Combat: %.1f, PedDensity: %.1f, ScriptPop: %.1f, PedBudget: %.1f)",
			fHighwayMultiplier, fWantedMultiplier, fCombatMultiplier, fPedDensityMultiplier, fScriptPopAreaAmbientMultiplier, fPedMemoryBudgetMultiplier);
		grcDebugDraw::Text( vTextPos, (fScenarioInteriorTotalMult == 1.0f && fScenarioExteriorTotalMult == 1.0f) ? Color_NavyBlue : Color_turquoise, debugText, true);

		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "PED COUNTS (ON FOOT) = (ambient: %i, scenario: %i, cop: %i, swat: %i, emergency: %i, other: %i)", ms_nNumOnFootAmbient, ms_nNumOnFootScenario, ms_nNumOnFootCop, ms_nNumOnFootSwat, ms_nNumOnFootEmergency, ms_nNumOnFootOther);
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(debugText, "PED COUNTS (IN VEHICLE) = (ambient: %i, scenario: %i, cop: %i, swat: %i, emergency: %i, other: %i)", ms_nNumInVehAmbient, ms_nNumInVehScenario, ms_nNumInVehCop, ms_nNumInVehSwat, ms_nNumInVehEmergency, ms_nNumInVehOther);
		grcDebugDraw::Text( vTextPos, Color_NavyBlue, debugText, true);
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();
		vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();


		// Display ped population failures
		char failString[1024] = { 0 };
		s32 iNumFails = GetFailString(failString, false, true);
		char scenarioFailString[1024] = { 0 };
		s32 iNumScenarioFails = GetFailString(scenarioFailString, true, false);

		float fLeftX = vTextPos.x;
		if(iNumFails)
		{
			sprintf(debugText, "POPULATION FAILURES Total = %i", iNumFails);
			grcDebugDraw::Text( vTextPos, Color_red3, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			vTextPos.x += vPixelSize.x * 16.0f;
			grcDebugDraw::Text( vTextPos, Color_red3, failString, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();
		}

		vTextPos.x = fLeftX;

		if(iNumScenarioFails)
		{
			sprintf(debugText, "POPULATION FAILURES (Scenario Specific) Total = %i", iNumScenarioFails);
			grcDebugDraw::Text( vTextPos, Color_red3, debugText, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();

			vTextPos.x += vPixelSize.x * 16.0f;
			grcDebugDraw::Text( vTextPos, Color_red3, scenarioFailString, true);
			vTextPos.y += vPixelSize.y * grcDebugDraw::GetScreenSpaceTextHeight();
		}
	}

	if(ms_bDebugPedReusePool)
	{
		CPed::Pool* pedPool = CPed::GetPool();
		for(int i = 0; i < pedPool->GetSize(); i++)
		{
			CPed* pPed = pedPool->GetSlot(i);
			if(!pPed)
				continue;

			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedWasReused))
			{
				grcDebugDraw::Sphere(pPed->GetTransform().GetPosition(), 1.f, Color_blue, false);
			}
		}
	}

	if(ms_bDisplayPedReusePool)
	{
		grcDebugDraw::AddDebugOutput("Ped Reuse Pool:");
		for(int i = 0; i < ms_pedReusePool.GetCount(); i++)
		{
			if(ms_pedReusePool[i].pPed)
				grcDebugDraw::AddDebugOutput("%u: %s", i+1, ms_pedReusePool[i].pPed->GetModelName());
		}

	}
}

void CPedPopulation::DisplayMessageHistory()
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

void CPedPopulation::DebugEventCreatePed(CPed* ped)
{
	popDebugf2("Created scenario ped 0x%p (%s) at (%.2f, %.2f, %.2f)", ped, ped->GetModelName(), ped->GetTransform().GetPosition().GetXf(), ped->GetTransform().GetPosition().GetYf(), ped->GetTransform().GetPosition().GetZf());
}

#if DEBUG_DRAW

void CPedPopulation::GetPedLODCounts(int &iCount, int &iEventScanHi, int &iEventScanLo, int &iMotionTaskHi, int &iMotionTaskLo, int &iPhysicsHi, int &iPhysicsLo, int &iEntityScanHi, int &iEntityScanLo, int &iActive, int &iInactive, int &iTimeslicingHi, int &iTimeslicingLo)
{
	iCount = 0;
	iEventScanHi = 0;
	iEventScanLo = 0;
	iMotionTaskHi = 0;
	iMotionTaskLo = 0;
	iPhysicsHi = 0;
	iPhysicsLo = 0;
	iEntityScanHi = 0;
	iEntityScanLo = 0;
	iActive = 0;
	iInactive = 0;
	iTimeslicingHi = 0;
	iTimeslicingLo = 0;

	if (!CPed::GetPool())
		return;

	s32 i = CPed::GetPool()->GetSize();
	while(i--)
	{
		CPed * pPed = CPed::GetPool()->GetSlot(i);
		if (pPed)
		{
			iCount++;

			CPedAILod &pedAILod = pPed->GetPedAiLod();
			
			if(pedAILod.IsLodFlagSet( CPedAILod::AL_LodEventScanning ))
				iEventScanLo++;
			else
				iEventScanHi++;

			if(pedAILod.IsLodFlagSet( CPedAILod::AL_LodMotionTask ))
				iMotionTaskLo++;
			else
				iMotionTaskHi++;

			if(pedAILod.IsLodFlagSet( CPedAILod::AL_LodPhysics ))
				iPhysicsLo++;
			else
				iPhysicsHi++;

			if(pedAILod.IsLodFlagSet( CPedAILod::AL_LodEntityScanning ))
				iEntityScanLo++;
			else
				iEntityScanHi++;

			if(pedAILod.IsLodFlagSet( CPedAILod::AL_LodTimesliceIntelligenceUpdate ))
				iTimeslicingLo++;
			else
				iTimeslicingHi++;

			// Now get active/inactive
			phInst *pInst = pPed->GetCurrentPhysicsInst();
			if( pInst && pInst->IsInLevel() && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex()) )
			{
				iActive++;
			}
			else
			{
				iInactive++;
			}
		}
	}
}

#endif //DEBUG_DRAW




CPedPopulation::PedPopulationFailureData::PedPopulationFailureData()
{
	m_aFailureInfos[FR_notAllowdByNetwork].Set(FR_notAllowdByNetwork, "FR_notAllowdByNetwork", "netForbig", Color_green, true, false);
	m_aFailureInfos[FR_factoryFailedToCreatePed].Set(FR_factoryFailedToCreatePed, "FR_factoryFailedToCreatePed", "factory", Color_blue, true, false);
	m_aFailureInfos[FR_factoryFailedToCreateVehicle].Set(FR_factoryFailedToCreateVehicle, "FR_factoryFailedToCreateVehicle", "vehicle", Color_blue, true, false);
	m_aFailureInfos[FR_poolOutOfSpace].Set(FR_poolOutOfSpace, "FR_poolOutOfSpace", "poolOut", Color_yellow, true, false);
	m_aFailureInfos[FR_noSpawnPosFound].Set(FR_noSpawnPosFound, "FR_noSpawnPosFound", "noSpawnPos", Color_red, true, false);
	m_aFailureInfos[FR_noPedModelFound].Set(FR_noPedModelFound, "FR_noPedModelFound", "noModel", Color_orange, true, false);
	m_aFailureInfos[FR_vehicleHasNoSeatBone].Set(FR_vehicleHasNoSeatBone, "FR_vehicleHasNoSeatBone", "seatBone", Color_salmon, true, false);
	m_aFailureInfos[FR_noDriverFallbackPedFound].Set(FR_noDriverFallbackPedFound, "FR_noDriverFallbackPedFound", "driverFallback", Color_sienna, true, false);
	m_aFailureInfos[FR_spawnPointOutsideCreationZone].Set(FR_spawnPointOutsideCreationZone, "FR_spawnPointOutsideCreationZone", "outsideZone", Color_DeepPink, true, false);
	m_aFailureInfos[FR_scenarioHasNoClipData].Set(FR_scenarioHasNoClipData, "FR_scenarioHasNoClipData", "sc_noClipData", Color_MistyRose, true, true);
	m_aFailureInfos[FR_spawnPointInBlockedArea].Set(FR_spawnPointInBlockedArea, "FR_spawnPointInBlockedArea", "InBlockedArea", Color_PaleVioletRed, true, false);
	m_aFailureInfos[FR_visibletoMpPlayer].Set(FR_visibletoMpPlayer, "FR_visibletoMpPlayer", "visibleMP", Color_LavenderBlush, true, false);
	m_aFailureInfos[FR_spawnCollisionFail].Set(FR_spawnCollisionFail, "FR_spawnCollisionFail", "inCollision", Color_magenta, true, false);
	m_aFailureInfos[FR_closeToMpPlayer].Set(FR_closeToMpPlayer, "FR_closeToMpPlayer", "tooCloseMP", Color_plum, true, false);
	m_aFailureInfos[FR_scenarioPointTooFarAway].Set(FR_scenarioPointTooFarAway, "FR_scenarioPointTooFarAway", "sc_tooFar", Color_purple, true, true);
	m_aFailureInfos[FR_scenarioOutsideFail].Set(FR_scenarioOutsideFail, "FR_scenarioOutsideFail", "sc_outside", Color_thistle, true, true);
	m_aFailureInfos[FR_scenarioInsideFail].Set(FR_scenarioInsideFail, "FR_scenarioInsideFail", "sc_inside", Color_AntiqueWhite, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailCarGen].Set(FR_scenarioTimeAndProbabilityFailCarGen, "FR_scenarioTimeAndProbabilityFailCarGen", "sc_timeProbCarGen", Color_gold, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailVarious].Set(FR_scenarioTimeAndProbabilityFailVarious, "FR_scenarioTimeAndProbabilityFailVarious", "sc_Various", Color_BlanchedAlmond, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailSpawnInterval].Set(FR_scenarioTimeAndProbabilityFailSpawnInterval, "FR_scenarioTimeAndProbabilityFailSpawnInterval", "sc_spawnInterval", Color_magenta2, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailTimeOverride].Set(FR_scenarioTimeAndProbabilityFailTimeOverride, "FR_scenarioTimeAndProbabilityFailTimeOverride", "sc_timeOverride", Color_magenta4, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailStreamingPressure].Set(FR_scenarioTimeAndProbabilityFailStreamingPressure, "FR_scenarioTimeAndProbabilityFailStreamingPressure", "sc_streamPressure", Color_yellow2, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailConditions].Set(FR_scenarioTimeAndProbabilityFailConditions, "FR_scenarioTimeAndProbabilityFailConditions", "sc_scenarioCond", Color_yellow4, true, true);
	m_aFailureInfos[FR_scenarioTimeAndProbabilityFailProb].Set(FR_scenarioTimeAndProbabilityFailProb, "FR_scenarioTimeAndProbabilityFailProb", "sc_scenarioProb", Color_VioletRed4, true, true);
	m_aFailureInfos[FR_scenarioPointDisabled].Set(FR_scenarioPointDisabled, "FR_scenarioPointDisabled", "sc_disabled", Color_copper, true, true);
	m_aFailureInfos[FR_scenarioObjectBroken].Set(FR_scenarioObjectBroken, "FR_scenarioObjectBroken", "sc_objBroken", Color_maroon, true, true);
	m_aFailureInfos[FR_scenarioObjectUprooted].Set(FR_scenarioObjectUprooted, "FR_scenarioObjectUprooted", "sc_objUprooted", Color_SeaGreen, true, true);
	m_aFailureInfos[FR_scenarioObjectDoesntSpawnPeds].Set(FR_scenarioObjectDoesntSpawnPeds, "FR_scenarioObjectDoesntSpawnPeds", "sc_objNoSpawn", Color_orchid, true, true);
	m_aFailureInfos[FR_scenarioPointInUse].Set(FR_scenarioPointInUse, "FR_scenarioPointInUse", "sc_inUse", Color_plum, true, true);
	m_aFailureInfos[FR_collisionChecks].Set(FR_collisionChecks, "FR_collisionChecks", "collisionChecks", Color_plum4, true, false);
	m_aFailureInfos[FR_scenarioCarGenNotReady].Set(FR_scenarioCarGenNotReady, "FR_scenarioCarGenNotReady", "sc_carGenNotReady", Color_blue3, true, true);
	m_aFailureInfos[FR_scenarioCarGenScriptDisabled].Set(FR_scenarioCarGenScriptDisabled, "FR_scenarioCarGenScriptDisabled", "sc_carGenOffScript", Color_brown, true, true);
	m_aFailureInfos[FR_scenarioPosNotLocallyControlled].Set(FR_scenarioPosNotLocallyControlled, "FR_scenarioPosNotLocallyControlled", "sc_posNotLocalMP", Color_DarkOrange, true, true);
	m_aFailureInfos[FR_scenarioPosNotLocallyControlledCarGen].Set(FR_scenarioPosNotLocallyControlledCarGen, "FR_scenarioPosNotLocallyControlledCarGen", "sc_carGenPosNotLocalMP", Color_red4, true, true);
	m_aFailureInfos[FR_scenarioVehPopLimits].Set(FR_scenarioVehPopLimits, "FR_scenarioVehPopLimits", "sc_vehPopLimit", Color_aquamarine, true, true);
	m_aFailureInfos[FR_scenarioVehModel].Set(FR_scenarioVehModel, "FR_scenarioVehModel", "sc_vehModel", Color_beige, true, true);
	m_aFailureInfos[FR_scenarioCollisionNotLoaded].Set(FR_scenarioCollisionNotLoaded, "FR_scenarioCollisionNotLoaded", "sc_collisionLoad", Color_grey, true, true);
	m_aFailureInfos[FR_scenarioPopCarGenFailed].Set(FR_scenarioPopCarGenFailed, "FR_scenarioPopCarGenFailed", "sc_carGenFail", Color_violet, true, true);
	m_aFailureInfos[FR_scenarioNoPropFound].Set(FR_scenarioNoPropFound, "FR_scenarioNoPropFound", "sc_noProp", Color_VioletRed2, true, true);
	m_aFailureInfos[FR_scenarioInteriorNotStreamedIn].Set(FR_scenarioInteriorNotStreamedIn, "FR_scenarioInteriorNotStreamedIn", "sc_interiorLoad", Color_azure, true, true);
	m_aFailureInfos[FR_scenarioNotInSameInterior].Set(FR_scenarioNotInSameInterior, "FR_scenarioNotInSameInterior", "sc_interiorsDiffer", Color_MediumPurple, true, true);
	m_aFailureInfos[FR_scenarioCachedFailure].Set(FR_scenarioCachedFailure, "FR_scenarioCachedFailure", "sc_cached", Color_LightPink, true, true);
	m_aFailureInfos[FR_scenarioPointRequiresImap].Set(FR_scenarioPointRequiresImap, "FR_scenarioPointRequiresImap", "sc_requiresImap", Color_LimeGreen, true, true);
	m_aFailureInfos[FR_dontSpawnCops].Set(FR_dontSpawnCops, "FR_dontSpawnCops", "sc_dontSpawnCops", Color_SlateGray, true, false);
	m_aFailureInfos[FR_scenarioCarGenBlockedChain].Set(FR_scenarioCarGenBlockedChain, "FR_scenarioCarGenBlockedChain", "sc_blockedChain", Color_grey88, true, true);
	Reset(0);
}

void CPedPopulation::PedPopulationFailureData::RegisterSpawnFailure( SpawnType type, FailureType iFailure, Vector3::Param vLocation, const CScenarioPoint* pScenarioPt /*= nullptr*/)
{
	if( !m_aFailureInfos[iFailure].m_bActive )
	{
		return;
	}

	++m_aFailureInfos[iFailure].m_iCount;
	
	if( type == ST_Wandering && CPopCycle::sm_bDisplayWanderingPedPopulationFailureCountsOnVectorMap && Vector3(vLocation).IsNonZero() )
	{
		CVectorMap::DrawCircle(vLocation, 0.75f, m_aFailureInfos[iFailure].m_colour, false, true);
	}
	if( type == ST_VehicleScenario && CPopCycle::sm_bDisplayScenarioVehiclePopulationFailureCountsOnVectorMap && Vector3(vLocation).IsNonZero() )
	{
		CVectorMap::DrawCircle(vLocation, 0.75f, m_aFailureInfos[iFailure].m_colour, false, true);
	}
	if( type == ST_Scenario && CPopCycle::sm_bDisplayScenarioPedPopulationFailureCountsOnVectorMap && Vector3(vLocation).IsNonZero() )
	{
		CVectorMap::DrawCircle(vLocation, 0.75f, m_aFailureInfos[iFailure].m_colour, false, true);
	}
	if (CPopCycle::sm_bDisplayScenarioPedPopulationFailuresInTheWorld)
	{
		char str[512];
		formatf(str, "%s", m_aFailureInfos[iFailure].m_szName);

		static int s_RegisterSpawnFailureScenarioDebugTTL = 10;
		grcDebugDraw::Text(vLocation, Color_red, str, true, s_RegisterSpawnFailureScenarioDebugTTL);
	}
	if( CPopCycle::sm_bDisplayPedPopulationFailureCountsDebugSpew )
	{
		const char* typeName = "?";
		switch(type)
		{
			case ST_Wandering:
				typeName = "Wandering";
				break;
			case ST_VehicleScenario:
				typeName = "VehicleScenario";
				break;
			case ST_Scenario:
				typeName = "Scenario";
				break;
			default:
				break;
		}

		char str[256];
		formatf(str, ", scenario point [%p] ", pScenarioPt);

		Displayf("Spawn failure: %s: %s (%.2f, %.2f, %.2f)%s", typeName, m_aFailureInfos[iFailure].m_szName, Vector3(vLocation).x, Vector3(vLocation).y, Vector3(vLocation).z, pScenarioPt ? str : "");
	}
}

#endif // __BANK
