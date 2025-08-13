// Title	:	Automobile.cpp
// Author	:	Richard Jobling/William Henderson
// Started	:	25/08/99
//
//
//

// Rage headers
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "grcore/texture.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/sleep.h"
#include "physics/constraintcylindrical.h"
#include "physics/constraintdistance.h"
#include "physics/intersection.h"
#include "profile/profiler.h"
#include "profile/timebars.h"
#include "rmptfx/ptxmanager.h"
#include "phBound/support.h"
#include "profile/cputrace.h"
#include "phbound/boundcomposite.h"
#include "math/vecmath.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwnet/netobject.h"
#include "fwmaths/angle.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/Task/TaskVehicleMissionBase.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "control/record.h"
#include "control/remote.h"
#include "control/replay/Replay.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "game/modelIndices.h"
#include "Stats/StatsMgr.h"
#include "game/zones.h"
#include "modelInfo/modelInfo.h"
#include "modelInfo/vehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoExtensions.h"
#include "network/Events/NetworkEventTypes.h"
#include "peds/pedDebugVisualiser.h"
#include "peds/pedIntelligence.h"
#include "peds/Ped.h"
#include "peds/popcycle.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/gtaMaterialManager.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/PickupManager.h"
#include "renderer/lights/AsyncLightOcclusionMgr.h"
#include "renderer/lights/lights.h"
#include "renderer/renderer.h"
#include "renderer/zoneCull.h"
#include "script/script.h"
#include "shaders/CustomShaderEffectProp.h"
#include "streaming/streaming.h"
#include "system/pad.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Movement/TaskParachuteObject.h"
#include "Task/Movement/TaskParachute.h"
#include "TimeCycle/TimeCycle.h"
#include "timecycle/TimeCycleConfig.h"
#include "weapons/explosion.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/automobile.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/bike.h"
#include "vehicles/handlingMgr.h" 
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/trailer.h"
#include "vehicles/Submarine.h"
#include "vehicles/wheel.h"
#include "vehicles/vehiclegadgets.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/virtualroad.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/Systems/VfxVehicle.h"
#include "vfx/Systems/VfxWater.h"
#include "network/NetworkInterface.h"
#include "weapons/weapondamage.h"
#include "scene/world/GameWorldHeightMap.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsInterface.h"
#include "network/Objects/Entities/NetObjPhysical.h"
#include "event/EventNetwork.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "ai/debug/system/AIDebugLogManager.h"
#include "frontend/PauseMenu.h"
#include "Frontend/NewHud.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

#define CAR_COMPONENT_REMOVE_LIFESPAN 	(20000)

//#define IGNORE_HIERARCHY

// BILL
extern CAutomobile* gpCar;
//extern tTerrain G_nTerrainType;
#define WHEEL_LOD


//#define TEST_DAMAGE
#define NEW_MODEL_HIERARCHY

float CAR_BALANCE_MULT = 0.08f;

#define JIMMY_TRANSFORM_TOTAL_TIME			(250.0f)

//#define CAR_RAILTRACK_SURFACETYPE (SURFACE_TYPE_GRAVEL)
#define CAR_RAILTRACK_SURFACETYPE (SURFACE_TYPE_RAILTRACK)

static dev_float AUTOMOBILE_SINK_TIME						= 5.0f;
static dev_float TRAILER_SINK_TIME							= 0.5f;
static dev_float AUTOMOBILE_SINK_STEP						= 0.002f;		// How much is the fForceMult decreased by each frame once car starts sinking
static dev_float AUTOMOBILE_SINK_FORCE_MULT_MULTIPLIER	= 0.5f;			// Buoyancy forceMult is decreased by the sink step until it reaches this fraction of the original
static dev_float AUTOMOBILE_SINK_CRASH_EVENT_RANGE			= 35.0f;	// Magic number to force the perception range for peds reacting to a sinking car.
static dev_s32	 AUTOMOBILE_SINK_EVENT_SUBMERGE_PERCENT		= 20;		// Magic number for the car to count as submerged for event purposes.

static dev_float PLANE_ENGINE_OFF_TIME						= 1.0f;

atFixedArray<CAutomobile::PlaceOnRoadAsyncData, MAX_ASYNC_PLACE_ON_ROAD_ENTRIES> CAutomobile::ms_placeOnRoadArray;

float CAutomobile::m_sfPassengerMassMult = 0.05f;

bank_float CQuadBike::ms_fQuadBikeSelfRightingTorqueScale = 15.0f;
bank_float CQuadBike::ms_fQuadBikeAntiRollOverTorqueScale = -35.0f;

bool CAutomobile::ms_bUseAsyncWheelProbes = true;
float CAutomobile::ms_fBumpSeverityMulti = 1.0f;
BANK_ONLY(bool CAutomobile::ms_bDrawWheelProbeResults = false;)

	
#define ASBO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.07f))
BANK_SWITCH_NT(static, static const) Vec3V Asbo_AmbientVolume_Offset = ASBO_AMBIENT_OCCLUDER_OFFSET;

#define ARDENT_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V Ardent_AmbientVolume_Offset = ARDENT_AMBIENT_OCCLUDER_OFFSET;

#define BRIOSO2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.044f))
BANK_SWITCH_NT(static, static const) Vec3V Brioso2_AmbientVolume_Offset = BRIOSO2_AMBIENT_OCCLUDER_OFFSET;

#define BRIOSO3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.060f))
BANK_SWITCH_NT(static, static const) Vec3V Brioso3_AmbientVolume_Offset = BRIOSO3_AMBIENT_OCCLUDER_OFFSET;

#define CHAMPION_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.138f))
BANK_SWITCH_NT(static, static const) Vec3V Champion_AmbientVolume_Offset = CHAMPION_AMBIENT_OCCLUDER_OFFSET;

#define CHEETAH2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.17f))
BANK_SWITCH_NT(static, static const) Vec3V Cheetah2_AmbientVolume_Offset = CHEETAH2_AMBIENT_OCCLUDER_OFFSET;

#define CLIQUE_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.075f))
BANK_SWITCH_NT(static, static const) Vec3V Clique_AmbientVolume_Offset = CLIQUE_AMBIENT_OCCLUDER_OFFSET;

#define COMET4_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.05f))
BANK_SWITCH_NT(static, static const) Vec3V Comet4_AmbientVolume_Offset = COMET4_AMBIENT_OCCLUDER_OFFSET;

#define COMET6_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.15f))
BANK_SWITCH_NT(static, static const) Vec3V Comet6_AmbientVolume_Offset = COMET6_AMBIENT_OCCLUDER_OFFSET;

#define COMET7_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.13f))
BANK_SWITCH_NT(static, static const) Vec3V Comet7_AmbientVolume_Offset = COMET7_AMBIENT_OCCLUDER_OFFSET;

#define COQUETTE4_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.185f))
BANK_SWITCH_NT(static, static const) Vec3V Coquette4_AmbientVolume_Offset = COQUETTE4_AMBIENT_OCCLUDER_OFFSET;

#define CORSITA_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.13f))
BANK_SWITCH_NT(static, static const) Vec3V Corsita_AmbientVolume_Offset = CORSITA_AMBIENT_OCCLUDER_OFFSET;

#define CYCLONE2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.19f))
BANK_SWITCH_NT(static, static const) Vec3V Cyclone2_AmbientVolume_Offset = CYCLONE2_AMBIENT_OCCLUDER_OFFSET;

#define CYPHER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V Cypher_AmbientVolume_Offset = CYPHER_AMBIENT_OCCLUDER_OFFSET;

#define DEVESTE_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.22f))
BANK_SWITCH_NT(static, static const) Vec3V Deveste_AmbientVolume_Offset = DEVESTE_AMBIENT_OCCLUDER_OFFSET;

#define DOMINATOR8_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.1165f))
BANK_SWITCH_NT(static, static const) Vec3V Dominator8_AmbientVolume_Offset = DOMINATOR8_AMBIENT_OCCLUDER_OFFSET;

#define DRAFTER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.075f))
BANK_SWITCH_NT(static, static const) Vec3V Drafter_AmbientVolume_Offset = DRAFTER_AMBIENT_OCCLUDER_OFFSET;

#define DRAUGUR_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.155f))
BANK_SWITCH_NT(static, static const) Vec3V Draugur_AmbientVolume_Offset = DRAUGUR_AMBIENT_OCCLUDER_OFFSET;

#define DUNE4_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.200f))
BANK_SWITCH_NT(static, static const) Vec3V Dune4_AmbientVolume_Offset = DUNE4_AMBIENT_OCCLUDER_OFFSET;

#define DUNE5_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.200f))
BANK_SWITCH_NT(static, static const) Vec3V Dune5_AmbientVolume_Offset = DUNE5_AMBIENT_OCCLUDER_OFFSET;

#define DUKES3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.078f))
BANK_SWITCH_NT(static, static const) Vec3V Dukes3_AmbientVolume_Offset = DUKES3_AMBIENT_OCCLUDER_OFFSET;

#define DYNASTY_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.0125f))
BANK_SWITCH_NT(static, static const) Vec3V Dynasty_AmbientVolume_Offset = DYNASTY_AMBIENT_OCCLUDER_OFFSET;

#define ELLIE_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.003f))
BANK_SWITCH_NT(static, static const) Vec3V Ellie_AmbientVolume_Offset = ELLIE_AMBIENT_OCCLUDER_OFFSET;

#define EUROS_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.095f))
BANK_SWITCH_NT(static, static const) Vec3V Euros_AmbientVolume_Offset = EUROS_AMBIENT_OCCLUDER_OFFSET;

#define FLASHGT_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.15f))
BANK_SWITCH_NT(static, static const) Vec3V FlashGT_AmbientVolume_Offset = FLASHGT_AMBIENT_OCCLUDER_OFFSET;

#define FMJ_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.175f))
BANK_SWITCH_NT(static, static const) Vec3V Fmj_AmbientVolume_Offset = FMJ_AMBIENT_OCCLUDER_OFFSET;

#define FORMULA_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.16f))
BANK_SWITCH_NT(static, static const) Vec3V Formula_AmbientVolume_Offset = FORMULA_AMBIENT_OCCLUDER_OFFSET;

#define FURIA_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.15f))
BANK_SWITCH_NT(static, static const) Vec3V Furia_AmbientVolume_Offset = FURIA_AMBIENT_OCCLUDER_OFFSET;

#define IGNUS_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.185f))
BANK_SWITCH_NT(static, static const) Vec3V Ignus_AmbientVolume_Offset = IGNUS_AMBIENT_OCCLUDER_OFFSET;

#define IGNUS2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.192f))
BANK_SWITCH_NT(static, static const) Vec3V Ignus2_AmbientVolume_Offset = IGNUS2_AMBIENT_OCCLUDER_OFFSET;

#define GB200_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.125f))
BANK_SWITCH_NT(static, static const) Vec3V GB200_AmbientVolume_Offset = GB200_AMBIENT_OCCLUDER_OFFSET;

#define GP1_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.115f))
BANK_SWITCH_NT(static, static const) Vec3V GP1_AmbientVolume_Offset = GP1_AMBIENT_OCCLUDER_OFFSET;

#define GAUNTLET4_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.186f))
BANK_SWITCH_NT(static, static const) Vec3V Gauntlet4_AmbientVolume_Offset = GAUNTLET4_AMBIENT_OCCLUDER_OFFSET;

#define GAUNTLET5_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.1f))
BANK_SWITCH_NT(static, static const) Vec3V Gauntlet5_AmbientVolume_Offset = GAUNTLET5_AMBIENT_OCCLUDER_OFFSET;

#define GROWLER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.14f))
BANK_SWITCH_NT(static, static const) Vec3V Growler_AmbientVolume_Offset = GROWLER_AMBIENT_OCCLUDER_OFFSET;

#define HOTRING_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.17f))
BANK_SWITCH_NT(static, static const) Vec3V Hotring_AmbientVolume_Offset = HOTRING_AMBIENT_OCCLUDER_OFFSET;

#define HUSTLER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.30f))
BANK_SWITCH_NT(static, static const) Vec3V Hustler_AmbientVolume_Offset = HUSTLER_AMBIENT_OCCLUDER_OFFSET;

#define IMORGON_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.07f))
BANK_SWITCH_NT(static, static const) Vec3V Imorgon_AmbientVolume_Offset = IMORGON_AMBIENT_OCCLUDER_OFFSET;

#define INFERNUS2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.075f))
BANK_SWITCH_NT(static, static const) Vec3V Infernus2_AmbientVolume_Offset = INFERNUS2_AMBIENT_OCCLUDER_OFFSET;

#define ISSI3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.003f))
BANK_SWITCH_NT(static, static const) Vec3V Issi3_AmbientVolume_Offset = ISSI3_AMBIENT_OCCLUDER_OFFSET;

#define ISSI7_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.18f))
BANK_SWITCH_NT(static, static const) Vec3V Issi7_AmbientVolume_Offset = ISSI7_AMBIENT_OCCLUDER_OFFSET;

#define ITALIGTO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.11f))
BANK_SWITCH_NT(static, static const) Vec3V Italigto_AmbientVolume_Offset = ITALIGTO_AMBIENT_OCCLUDER_OFFSET;

#define ITALIRSX_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.14f))
BANK_SWITCH_NT(static, static const) Vec3V ItaliRsx_AmbientVolume_Offset = ITALIRSX_AMBIENT_OCCLUDER_OFFSET;

#define JESTER3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.04f))
BANK_SWITCH_NT(static, static const) Vec3V Jester3_AmbientVolume_Offset = JESTER3_AMBIENT_OCCLUDER_OFFSET;

#define JESTER4_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.05f))
BANK_SWITCH_NT(static, static const) Vec3V Jester4_AmbientVolume_Offset = JESTER4_AMBIENT_OCCLUDER_OFFSET;

#define JUBILEE_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.08f))
BANK_SWITCH_NT(static, static const) Vec3V Jubilee_AmbientVolume_Offset = JUBILEE_AMBIENT_OCCLUDER_OFFSET;

#define KOMODA_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Komoda_AmbientVolume_Offset = KOMODA_AMBIENT_OCCLUDER_OFFSET;

#define KRIEGER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Krieger_AmbientVolume_Offset = KRIEGER_AMBIENT_OCCLUDER_OFFSET;

#define MANANA2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.16f))
BANK_SWITCH_NT(static, static const) Vec3V Manana2_AmbientVolume_Offset = MANANA2_AMBIENT_OCCLUDER_OFFSET;

#define MENACER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.025f))
BANK_SWITCH_NT(static, static const) Vec3V Menacer_AmbientVolume_Offset = MENACER_AMBIENT_OCCLUDER_OFFSET;

#define NEO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Neo_AmbientVolume_Offset = NEO_AMBIENT_OCCLUDER_OFFSET;

#define NEON_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.120f))
BANK_SWITCH_NT(static, static const) Vec3V Neon_AmbientVolume_Offset = NEON_AMBIENT_OCCLUDER_OFFSET;

#define NERO2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.200f))
BANK_SWITCH_NT(static, static const) Vec3V Nero2_AmbientVolume_Offset = NERO2_AMBIENT_OCCLUDER_OFFSET;

#define OPENWHEEL1_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V Openwheel1_AmbientVolume_Offset = OPENWHEEL1_AMBIENT_OCCLUDER_OFFSET;

#define OPENWHEEL2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V Openwheel2_AmbientVolume_Offset = OPENWHEEL2_AMBIENT_OCCLUDER_OFFSET;

#define PARAGON_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.19f))
BANK_SWITCH_NT(static, static const) Vec3V Paragon_AmbientVolume_Offset = PARAGON_AMBIENT_OCCLUDER_OFFSET;

#define PENETRATOR_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.200f))
BANK_SWITCH_NT(static, static const) Vec3V Penetrator_AmbientVolume_Offset = PENETRATOR_AMBIENT_OCCLUDER_OFFSET;

#define PENUMBRA2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Penumbra2_AmbientVolume_Offset = PENUMBRA2_AMBIENT_OCCLUDER_OFFSET;

#define PEYOTE3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.198f))
BANK_SWITCH_NT(static, static const) Vec3V Peyote3_AmbientVolume_Offset = PEYOTE3_AMBIENT_OCCLUDER_OFFSET;

#define POLBUFFALO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.22f))
BANK_SWITCH_NT(static, static const) Vec3V Polbuffalo_AmbientVolume_Offset = POLBUFFALO_AMBIENT_OCCLUDER_OFFSET;

#define POLGREENWOOD_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.03f))
BANK_SWITCH_NT(static, static const) Vec3V Polgreenwood_AmbientVolume_Offset = POLGREENWOOD_AMBIENT_OCCLUDER_OFFSET;

#define RAPIDGT3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.05f))
BANK_SWITCH_NT(static, static const) Vec3V RapidGT3_AmbientVolume_Offset = RAPIDGT3_AMBIENT_OCCLUDER_OFFSET;

#define REMUS_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.07f))
BANK_SWITCH_NT(static, static const) Vec3V Remus_AmbientVolume_Offset = REMUS_AMBIENT_OCCLUDER_OFFSET;

#define RT3000_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V RT3000_AmbientVolume_Offset = RT3000_AMBIENT_OCCLUDER_OFFSET;

#define RUSTON_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.110f))
BANK_SWITCH_NT(static, static const) Vec3V Ruston_AmbientVolume_Offset = RUSTON_AMBIENT_OCCLUDER_OFFSET;

#define S80_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V S80_AmbientVolume_Offset = S80_AMBIENT_OCCLUDER_OFFSET;

#define SC1_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.10f))
BANK_SWITCH_NT(static, static const) Vec3V SC1_AmbientVolume_Offset = SC1_AMBIENT_OCCLUDER_OFFSET;

#define SEASPARROW2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.04f))
BANK_SWITCH_NT(static, static const) Vec3V Seasparrow2_AmbientVolume_Offset = SEASPARROW2_AMBIENT_OCCLUDER_OFFSET;

#define SCHLAGEN_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.185f))
BANK_SWITCH_NT(static, static const) Vec3V Schlagen_AmbientVolume_Offset = SCHLAGEN_AMBIENT_OCCLUDER_OFFSET;

#define SM722_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.09f))
BANK_SWITCH_NT(static, static const) Vec3V SM722_AmbientVolume_Offset = SM722_AMBIENT_OCCLUDER_OFFSET;

#define STAFFORD_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.0250f))
BANK_SWITCH_NT(static, static const) Vec3V Stafford_AmbientVolume_Offset = STAFFORD_AMBIENT_OCCLUDER_OFFSET;

#define STROMBERG_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.075f))
BANK_SWITCH_NT(static, static const) Vec3V Stromberg_AmbientVolume_Offset = STROMBERG_AMBIENT_OCCLUDER_OFFSET;

#define SWINGER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.25f))
BANK_SWITCH_NT(static, static const) Vec3V Swinger_AmbientVolume_Offset = SWINGER_AMBIENT_OCCLUDER_OFFSET;

#define SUGOI_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.09f))
BANK_SWITCH_NT(static, static const) Vec3V Sugoi_AmbientVolume_Offset = SUGOI_AMBIENT_OCCLUDER_OFFSET;

#define TAILGATER2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Tailgater2_AmbientVolume_Offset = TAILGATER2_AMBIENT_OCCLUDER_OFFSET;

#define TAMPA3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.015f))
BANK_SWITCH_NT(static, static const) Vec3V Tampa3_AmbientVolume_Offset = TAMPA3_AMBIENT_OCCLUDER_OFFSET;

#define THRAX_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.22f))
BANK_SWITCH_NT(static, static const) Vec3V Thrax_AmbientVolume_Offset = THRAX_AMBIENT_OCCLUDER_OFFSET;

#define TIGON_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.19f))
BANK_SWITCH_NT(static, static const) Vec3V Tigon_AmbientVolume_Offset = TIGON_AMBIENT_OCCLUDER_OFFSET;

#define TOREADOR_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.224f))
BANK_SWITCH_NT(static, static const) Vec3V Toreador_AmbientVolume_Offset = TOREADOR_AMBIENT_OCCLUDER_OFFSET;

#define TORERO2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.145f))
BANK_SWITCH_NT(static, static const) Vec3V Torero2_AmbientVolume_Offset = TORERO2_AMBIENT_OCCLUDER_OFFSET;

#define TYRANT_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.18f))
BANK_SWITCH_NT(static, static const) Vec3V Tyrant_AmbientVolume_Offset = TYRANT_AMBIENT_OCCLUDER_OFFSET;

#define VAGNER_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.18f))
BANK_SWITCH_NT(static, static const) Vec3V Vagner_AmbientVolume_Offset = VAGNER_AMBIENT_OCCLUDER_OFFSET;

#define VAGRANT_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Vagrant_AmbientVolume_Offset = VAGRANT_AMBIENT_OCCLUDER_OFFSET;

#define VERUS_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.045f))
BANK_SWITCH_NT(static, static const) Vec3V Verus_AmbientVolume_Offset = VERUS_AMBIENT_OCCLUDER_OFFSET;

#define VETO2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.07f))
BANK_SWITCH_NT(static, static const) Vec3V Veto2_AmbientVolume_Offset = VETO2_AMBIENT_OCCLUDER_OFFSET;

#define VOODOO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.025f))
BANK_SWITCH_NT(static, static const) Vec3V Voodoo_AmbientVolume_Offset = VOODOO_AMBIENT_OCCLUDER_OFFSET;

#define VSTR_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.10f))
BANK_SWITCH_NT(static, static const) Vec3V Vstr_AmbientVolume_Offset = VSTR_AMBIENT_OCCLUDER_OFFSET;

#define WARRENER2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.05f))
BANK_SWITCH_NT(static, static const) Vec3V Warrener2_AmbientVolume_Offset = WARRENER2_AMBIENT_OCCLUDER_OFFSET;

#define WEEVIL_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.06f))
BANK_SWITCH_NT(static, static const) Vec3V Weevil_AmbientVolume_Offset = WEEVIL_AMBIENT_OCCLUDER_OFFSET;

#define WEEVIL2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.280f))
BANK_SWITCH_NT(static, static const) Vec3V Weevil2_AmbientVolume_Offset = WEEVIL2_AMBIENT_OCCLUDER_OFFSET;

#define XA21_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.075f))
BANK_SWITCH_NT(static, static const) Vec3V XA21_AmbientVolume_Offset = XA21_AMBIENT_OCCLUDER_OFFSET;

#define YOUGA3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, 0.22f))
BANK_SWITCH_NT(static, static const) Vec3V Youga3_AmbientVolume_Offset = YOUGA3_AMBIENT_OCCLUDER_OFFSET;

#define ZENO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.22f))
BANK_SWITCH_NT(static, static const) Vec3V Zeno_AmbientVolume_Offset = ZENO_AMBIENT_OCCLUDER_OFFSET;

#define ZION3_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.085f))
BANK_SWITCH_NT(static, static const) Vec3V Zion3_AmbientVolume_Offset = ZION3_AMBIENT_OCCLUDER_OFFSET;

#define ZORRUSSO_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.12f))
BANK_SWITCH_NT(static, static const) Vec3V Zorrusso_AmbientVolume_Offset = ZORRUSSO_AMBIENT_OCCLUDER_OFFSET;

#define ZR350_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.11f))
BANK_SWITCH_NT(static, static const) Vec3V ZR350_AmbientVolume_Offset = ZR350_AMBIENT_OCCLUDER_OFFSET;

#define ZR3802_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.14f))
BANK_SWITCH_NT(static, static const) Vec3V ZR3802_AmbientVolume_Offset = ZR3802_AMBIENT_OCCLUDER_OFFSET;

#define ZR3803_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.14f))
BANK_SWITCH_NT(static, static const) Vec3V ZR3803_AmbientVolume_Offset = ZR3803_AMBIENT_OCCLUDER_OFFSET;




//
//
//
CAutomobile::CAutomobile(const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType) : CVehicle(ownedBy, popType, vehType)
{
	m_nAutomobileFlags.bTaxiLight = false;
	m_nAutomobileFlags.bWaterTight = false;
	m_nAutomobileFlags.bIsBoggedDownInSand = false;
    m_nAutomobileFlags.bFireNetworkCannon = false;
    m_nAutomobileFlags.bBurnoutModeEnabled = false;
	m_nAutomobileFlags.bInBurnout = false;
    m_nAutomobileFlags.bReduceGripModeEnabled = false;
	m_nAutomobileFlags.bPlayerIsRevvingEngineThisFrame = false;
	m_nAutomobileFlags.bDropsMoneyWhenBlownUp = false;
	m_nAutomobileFlags.bHydraulicsBounceRaising = false;
	m_nAutomobileFlags.bHydraulicsBounceLanding = false;
	m_nAutomobileFlags.bHydraulicsJump			= false;
	m_nAutomobileFlags.bForceUpdateGroundClearance = false;
	m_nAutomobileFlags.bWheelieModeEnabled = false;
	m_nAutomobileFlags.bInWheelieMode = false;
	m_nAutomobileFlags.bInWheelieModeWheelsUp = false;

	m_nVehicleFlags.bIsVan = false;
	m_nVehicleFlags.bIsBig = false;
	m_nVehicleFlags.bIsBus = false;
	m_nVehicleFlags.bLowVehicle = false;

	m_nVehicleFlags.bUseDeformation = true;
	
	m_nAutoDoorTimer = 0;
	m_nAutoDoorStart = 0;

	m_fHeightAboveRoad = 0.0f;

	m_dummyRotationalConstraint.Reset();
	m_dummyStayOnRoadConstraintHandle.Reset();

	ResetRealConversionFailedData();

	m_fTimeInWater = 0.0f;

	for(int i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		m_pCarWheels[i] = NULL;
	}

	m_pWheelProbeResults = NULL;
	m_pWheelProbePreviousResults = NULL;

	m_fGroundClearance = 0.0f;
	m_fLastGroundClearance = 0.0f;
	m_fLastGroundClearanceHeightAboveGround = 0.0f;
	m_uLastGroundClearanceCheck = 0;
	m_nAutomobileFlags.bLastSpaceAvaliableToChangeBounds = true;

    m_bIgnoreWorldHeightMapForWheelProbes = false;

	m_fFakeRotationY = 0.0f;
    m_fHighSpeedRotationY = 0.0f;
    m_fHighSpeedDirectionY = 1.0f;
    m_fHighSpeedYTime = 0.0f;
    m_fBumpSeverityYDecay = 0.1f;

    m_fBumpSeverityY = 0.0f;
    m_fBumpRateY = 0.0f;

    m_fFakeSuspensionLowerAmount = 0.0f;
	m_fFakeSuspensionLowerAmountApplied = 0.0f; 
	m_fBurnoutRotationTime = 0.0f;
	m_fHydraulicsUpperBound = 0.0f;
	m_fHydraulicsLowerBound = 0.0f;
	m_fHydraulicsRate = 0.0f;
	m_iTimeHydraulicsModified = 0;

	m_fSteeringBias = 0.0f;
	m_fSteeringBiasLife = 0.0f;
	m_fSteeringBiasScalar = 1.0f;

	m_fLastDriveForce = 0.0f;

	m_vSuspensionDeformationLF = VEC3_ZERO;
	m_vSuspensionDeformationRF = VEC3_ZERO;

	m_pParachuteObject = NULL;
	m_wasCloneParachuting = false;

	m_iParachuteModelIndex = fwModelId::MI_INVALID;
	m_nParachuteTintIndex = 0;

	m_carParachuteState = CAR_NOT_PARACHUTING;

	m_fParachuteStickX = 0.0f;
	m_fParachuteStickY = 0.0f;

	m_fCloneParachuteStickX = 0.0f;
	m_fCloneParachuteStickY = 0.0f;

	m_fParachuteHatchOffset = 0.0f;

	m_bCanDeployParachute = false;

	m_bFinishParachutingRequested = false;
	m_bCanPlayerDetachParachute = true;

	m_bHasBeenParachuting = false;
}

CAutomobile::~CAutomobile()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	for(int i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
	{
		if(m_pCarWheels[i])
		{
			delete m_pCarWheels[i];
			m_pCarWheels[i] = NULL;
		}
	}

	delete [] m_pWheelProbeResults;
	delete [] m_pWheelProbePreviousResults;

	// Remove dummy constraints by switching the automobile back to VDM_REAL mode
	ChangeDummyConstraints(VDM_REAL,false);

	if(m_pParachuteObject)
	{
		DetachParachuteFromVehicle();
		DestroyParachute();
	}
}

void CAutomobile::RemoveDummyInst()
{
	RemoveConstraints();

	// This will call CPhysical::RemoveConstraints()
	CVehicle::RemoveDummyInst();
}

void CAutomobile::SetModelId(fwModelId modelId)
{
	CVehicle::SetModelId(modelId);

	if (/*m_sAllTaxiLights &&*/ CVehicle::IsTaxiModelId(modelId) && (fwRandom::GetRandomNumberInRange(0.0f, 1.0f)<0.65f) )
	{
		m_nAutomobileFlags.bTaxiLight = true;
		//Resetting the damage as it would be broken when we call CVehicle::SetModelId (which would call InitFragmentsExtra)
		//That function breaks all the extra lights. We need to reset the taxi light if the car is a taxi
		m_VehicleDamage.SetLightStateImmediate(TAXI_IDX, false);
	}
	else
	{
		m_nAutomobileFlags.bTaxiLight = false;
	}

	// Some cars have their door locked initially
	if (GetCarDoorLocks() == CARLOCK_UNLOCKED)
	{
		if (IsLawEnforcementVehicle())
			SetCarDoorLocks(CARLOCK_LOCKED_INITIALLY);
	}

	m_fOrigBuoyancyForceMult = m_Buoyancy.m_fForceMult;

	if (GetModelIndex() == MI_CAR_SECURICAR && !NetworkInterface::IsGameInProgress())
	{
		m_nAutomobileFlags.bDropsMoneyWhenBlownUp = true;
	}

	// TODO: MAYBE PUT THIS IN A FLAG SO OTHER VEHICLES CAN HAVE THEM
	if( MI_CAR_WASTELANDER2.IsValid() &&
		GetModelIndex() == MI_CAR_WASTELANDER2 )
	{
		SlipperyBoxInitialise();
		SlipperyBoxUpdateLimits( 0.01f );
	}

}

void CAutomobile::InitWheels()
{
	Assertf(m_nNumWheels == 0, "CAutomobile::InitWheels(): Expected 0 wheels, but found %i",m_nNumWheels);
	if(m_nNumWheels > 0)
	{
		return;
	}

	m_ppWheels = m_pCarWheels;

	// initialise wheels
	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();

	bool bSteerFrontWheels = true;
	bool bSteerRearWheels = false;
	const bool bPowerFrontWheels = pHandling->DriveFrontWheels();
	const bool bPowerRearWheels = pHandling->DriveRearWheels();
	const bool bPowerMidWheels = pHandling->DriveMidWheels();
	const bool bDontRenderWheelSteering = (pHandling->mFlags & MF_DONT_RENDER_STEER) != 0;

	if(pHandling->hFlags &HF_STEER_REARWHEELS)
	{
		bSteerFrontWheels = false;
		bSteerRearWheels = true;
	}
	else if(pHandling->hFlags &(HF_HANDBRAKE_REARWHEELSTEER|HF_STEER_ALL_WHEELS))
	{
		bSteerRearWheels = true;
	}

	if(pHandling->hFlags & HF_STEER_NO_WHEELS)
	{
		bSteerFrontWheels = false;
		bSteerRearWheels = false;
	}

	bool bSteerAllFrontWheels = false;
	if( ( pHandling->hFlags & HF_STEER_REARWHEELS ) && 
		( pHandling->hFlags & HF_STEER_ALL_WHEELS ) && 
		!( pHandling->mFlags & MF_HAS_TRACKS ) )
	{
		bSteerAllFrontWheels = true;
		bSteerFrontWheels = true;
		bSteerRearWheels = true;
	}

	// Set up an array of this struct so we can create the wheels in a nice loop
	struct sAutomobileWheelCreationStruct{
		eHierarchyId iHierarchyId;
		bool bSteer;
		bool bPowered;
		bool bIsFront;
		bool bIsLeft;
		bool bIsMiddle;
	};

	sAutomobileWheelCreationStruct wheelCreationData [NUM_VEH_CWHEELS_MAX] =
	{
		{VEH_WHEEL_LF, bSteerFrontWheels, bPowerFrontWheels, true, true, false},
		{VEH_WHEEL_RF, bSteerFrontWheels, bPowerFrontWheels, true, false, false},
		{VEH_WHEEL_LR,  bSteerRearWheels, bPowerRearWheels, false, true, false},
		{VEH_WHEEL_RR,  bSteerRearWheels, bPowerRearWheels, false, false, false},
		{VEH_WHEEL_LM1, bSteerRearWheels || bSteerAllFrontWheels, bPowerMidWheels, false, true , true},
		{VEH_WHEEL_RM1, bSteerRearWheels || bSteerAllFrontWheels, bPowerMidWheels, false, false, true},
		{VEH_WHEEL_LM2, bSteerRearWheels, bPowerMidWheels, false, true, true},
		{VEH_WHEEL_RM2, bSteerRearWheels, bPowerMidWheels, false, false, true},
		{VEH_WHEEL_LM3, bSteerRearWheels, bPowerMidWheels, false, true, true},
		{VEH_WHEEL_RM3, bSteerRearWheels, bPowerMidWheels, false, false, true}
	};

	// Need to do one loop over all wheel ids to figure out how many wheels exist
	// This is so we can inversely scale the suspension force with wheel number
	int iNumWheelsToCreate = 0;
	for(int iWheelIndex = 0; iWheelIndex < NUM_VEH_CWHEELS_MAX; iWheelIndex++)
	{
		if(GetBoneIndex(wheelCreationData[iWheelIndex].iHierarchyId) > -1)
		{
			iNumWheelsToCreate++;
		}
	}

#if __ASSERT

	// All cars should have a LF wheel
	Assertf(GetBoneIndex(VEH_WHEEL_LF) > -1 || InheritsFromTrailer(), "Missing WHEEL_LF");	
#endif

	m_nNumWheels = 0;
	for(int iWheelIndex = 0; iWheelIndex < NUM_VEH_CWHEELS_MAX; iWheelIndex++)
	{
		if(GetBoneIndex(wheelCreationData[iWheelIndex].iHierarchyId) > -1)
		{
			int nFlags = 0;
			if(wheelCreationData[iWheelIndex].bIsLeft) 
				nFlags |= WCF_LEFTWHEEL;
			if(!wheelCreationData[iWheelIndex].bIsFront) 
				nFlags |= WCF_REARWHEEL;
			if(wheelCreationData[iWheelIndex].bPowered)
				nFlags |= WCF_POWERED;
			if(wheelCreationData[iWheelIndex].bSteer)
				nFlags |= WCF_STEER;

			if(wheelCreationData[iWheelIndex].bIsFront)
			{
				if(pHandling->mFlags &MF_AXLE_F_SOLID)
					nFlags |= WCF_TILT_SOLID;
				else if(pHandling->mFlags &MF_AXLE_F_MCPHERSON)
					nFlags |= WCF_TILT_INDEP;
			}
			else
			{
				if(pHandling->mFlags &MF_AXLE_R_SOLID)
					nFlags |= WCF_TILT_SOLID;
				else if(pHandling->mFlags &MF_AXLE_R_MCPHERSON)
					nFlags |= WCF_TILT_INDEP;
			}
			

			if(GetVehicleType() == VEHICLE_TYPE_QUADBIKE || GetVehicleType() == VEHICLE_TYPE_DRAFT || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
			{
				nFlags |= WCF_QUAD_WHEEL;
			}
			if(GetVehicleType() == VEHICLE_TYPE_PLANE || (GetVehicleType() == VEHICLE_TYPE_HELI && static_cast< CHeli* >( this )->HasLandingGear()))
			{
				nFlags |= WCF_HIGH_FRICTION_WHEEL;
				nFlags |= WCF_PLANE_WHEEL;
			}
			if( GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
			{
				nFlags |= WCF_AMPHIBIOUS_WHEEL;
			}
			if( GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_RENDER_WHEELS_WITH_ZERO_COMPRESSION ))
			{
				nFlags |= WCF_RENDER_WITH_ZERO_COMPRESSION;
			}

			if(pHandling->mFlags & MF_HAS_TRACKS && 
				( pHandling->hFlags & HF_STEER_ALL_WHEELS ||
				  !wheelCreationData[iWheelIndex].bSteer ) )
			{
				nFlags |= WCF_TRACKED_WHEEL;
			}
			
			if(pHandling->hFlags & HF_STEER_ALL_WHEELS && pHandling->hFlags & HF_EXT_WHEEL_BOUNDS_COL)
			{
				nFlags |= WCF_ROTATE_BOUNDS;
			}

			//get the opposite wheel index
			int nOppositeIndex = -1;

			if(iWheelIndex % 2 == 0)
				nOppositeIndex = iWheelIndex+1;
			else
				nOppositeIndex = iWheelIndex-1;

			//make sure the opposite wheel index is a valid wheel
			if(nOppositeIndex >= 0 && nOppositeIndex < NUM_VEH_CWHEELS_MAX)
			{
				if(GetBoneIndex(wheelCreationData[nOppositeIndex].iHierarchyId) == -1)
					nOppositeIndex = -1;
			}
			else
			{
				nOppositeIndex = -1;//not a valid index so set to -1
			}

			const float fRimRadius = pModelInfo->GetRimRadius(wheelCreationData[iWheelIndex].bIsFront);

			float fFrontRearSelector = wheelCreationData[iWheelIndex].bIsFront ? 1.0f : -1.0f;
			if(wheelCreationData[iWheelIndex].bIsMiddle)
			{
				eHierarchyId iFront = wheelCreationData[iWheelIndex].bIsLeft ? VEH_WHEEL_LF : VEH_WHEEL_RF;
				eHierarchyId iRear = wheelCreationData[iWheelIndex].bIsLeft ? VEH_WHEEL_LR : VEH_WHEEL_RR;

				// For a middle wheel to work, must have corresponding front and rear wheels to calculate the front / rear selection
				// Check this here and assert meaningfully

				// If we are missing a front right / rear right, try and use a left
				// Fix for planes which just have one front wheel but mutliple rear wheels.
				if(!wheelCreationData[iWheelIndex].bIsLeft)
				{
					int iFrontBoneIndex = pModelInfo->GetBoneIndex(iFront);
					if(iFrontBoneIndex == -1)
					{
						iFront = VEH_WHEEL_LF;
					}
				}
#if __ASSERT
				Assertf(pModelInfo->GetBoneIndex(iFront) > -1 || InheritsFromTrailer(), "Missing front %s wheel!",wheelCreationData[iWheelIndex].bIsLeft ? "left" : "right");
				Assertf(pModelInfo->GetBoneIndex(iRear) > -1 || InheritsFromTrailer(), "Missing rear %s wheel!",wheelCreationData[iWheelIndex].bIsLeft ? "left" : "right");				
				// No need to verify since CalcFrontRearSelector ihandles this case
#endif				
				fFrontRearSelector = CalcFrontRearSelector(wheelCreationData[iWheelIndex].iHierarchyId,iFront,iRear);
			}

			m_ppWheels[m_nNumWheels] = rage_new CWheel();
			if (Verifyf(m_ppWheels[m_nNumWheels],"Could not create a wheel! Wheel pool size probably needs increasing"))
			{
				m_ppWheels[m_nNumWheels]->Init(this, wheelCreationData[iWheelIndex].iHierarchyId, fRimRadius, pHandling->GetSuspensionBias(fFrontRearSelector) / (float)iNumWheelsToCreate, nFlags, s8(nOppositeIndex) );
				m_ppWheels[m_nNumWheels]->SetFrontRearSelector(fFrontRearSelector);
				if(bDontRenderWheelSteering)
				{
					m_ppWheels[m_nNumWheels]->GetConfigFlags().SetFlag(WCF_DONT_RENDER_STEER);
				}
				m_nNumWheels++;
			}
		}
	}

	if( m_nNumWheels == 3 &&
		( GetVehicleType() == VEHICLE_TYPE_CAR || 
		  GetVehicleType() == VEHICLE_TYPE_QUADBIKE ) )
	{
		for( int iWheelIndex = 0; iWheelIndex < m_nNumWheels; iWheelIndex++ )
		{	
			eHierarchyId oppositeWheelId = VEH_WHEEL_LF;

			switch( m_ppWheels[ iWheelIndex ]->GetHierarchyId() )
			{
				case VEH_WHEEL_LF:
				{
					oppositeWheelId = VEH_WHEEL_RF;
					break;
				}
				case VEH_WHEEL_RF:
				{
					oppositeWheelId = VEH_WHEEL_LF;
					break;
				}
				case VEH_WHEEL_LR:
				{
					oppositeWheelId = VEH_WHEEL_RR;
					break;
				}
				case VEH_WHEEL_RR:
				{
					oppositeWheelId = VEH_WHEEL_LR;
					break;
				}
				default:
					break;
			}

			bool oppositeWheelFound = false;
			for( s8 oppositeWheelIndex = 0; oppositeWheelIndex < m_nNumWheels; oppositeWheelIndex++ )
			{
				if( m_ppWheels[ oppositeWheelIndex ]->GetHierarchyId() == oppositeWheelId )
				{
					m_ppWheels[ iWheelIndex ]->SetOppositeWheelIndex( oppositeWheelIndex );
					oppositeWheelFound = true;
					break;
				}
			}

			if( !oppositeWheelFound )
			{
				m_ppWheels[ iWheelIndex ]->GetConfigFlags().SetFlag( WCF_CENTRE_WHEEL );
			}
		}
	}

	if( ( pHandling->mFlags & MF_HAS_TRACKS ) || ( pHandling->hFlags & HF_ARMOURED ) )
	{
		m_nVehicleFlags.bCanBreakOffWheelsWhenBlowUp = false;
	}

	// now need to setup wheels (suspension and stuff)
	const void *basePtr = NULL;
	if(GetVehicleDamage() && GetVehicleDamage()->GetDeformation() && GetVehicleDamage()->GetDeformation()->HasDamageTexture())
	{
		basePtr = GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead); //Lock the texture once for all wheels
	}

	SetupWheels(basePtr);

	if (basePtr)
	{
		GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
	}

	InitCacheWheelsById();

#ifdef CHECK_VEHICLE_SETUP
#if !__NO_OUTPUT
    if( m_WheelIndexById[ 0 ] != (u8)( -1 ) && // check that we have at least one valid wheel index before calling these functions or it will assert
		GetWheelFromId(VEH_WHEEL_LF) && GetWheelFromId(VEH_WHEEL_LR))
    {
        float fFrontLength = rage::Abs(GetWheelFromId(VEH_WHEEL_LF)->GetProbeSegment().A.y);
        float fRearLength = rage::Abs(GetWheelFromId(VEH_WHEEL_LR)->GetProbeSegment().A.y);
 
        float fWheelbase = fFrontLength + fRearLength;

        float fSuspensionBiasChange = (fWheelbase+pHandling->m_vecCentreOfMassOffset.GetYf())/fWheelbase;
        
        modelinfoDisplayf( "%s If Centered Suggested Suspension Bias: %.2f", GetModelName(), fSuspensionBiasChange-0.5f);
        modelinfoDisplayf( "%s Suggested Suspension Bias: %.2f Current: %.2f ", GetModelName(), (fRearLength+pHandling->m_vecCentreOfMassOffset.GetYf())/fWheelbase, pHandling->m_fSuspensionBiasFront*0.5f );
    }
#endif // !__NO_OUTPUT
#endif

	float fTempHeight;
	CVehicle::CalculateHeightsAboveRoad(GetModelId(), &m_fHeightAboveRoad, &fTempHeight);
}

void CAutomobile::SetupWheels(const void* pDamageTexture)
{
	for(int i=0; i<GetNumWheels(); i++)
	{
		GetWheel(i)->SuspensionSetup(pDamageTexture);
	}

	if(!IsColliderArticulated())
	{	
		// If the suspension changed, the maximum extents used by non-articulated vehicles needs to changes as well.
		CalculateNonArticulatedMaximumExtents();
	}
}



//
// name:		ProcessFlightHandling
// description:	All the code to handle the control process of flying vehicles
//				that used to reside in ProcessControl
//
void CAutomobile::ProcessFlightHandling(float UNUSED_PARAM(fTimeStep))
{
/*
	if(GetStatus()==STATUS_PLAYER || GetStatus()==STATUS_PHYSICS)
	{
		if (CCheat::IsCheatActive(CCheat::FLYINGCARS_CHEAT) && GetVelocity().Mag() > 0.0f && fTimeStep > 0.0f)
		{
			FlyingControl(FLIGHTMODEL_PLANE, fTimeStep);
		}
	}
*/
}

float CAutomobile::CAR_BURNOUT_SPEED = 3.0f;

float CAutomobile::CAR_INAIR_ROT_X = 1.3f;
float CAutomobile::CAR_INAIR_ROT_Y = 1.3f;
float CAutomobile::CAR_INAIR_ROT_Z = -1.0f;

float CAutomobile::CAR_STUCK_ROT_X = 1.5f;
float CAutomobile::CAR_STUCK_ROT_Y = 3.0f;
float CAutomobile::CAR_STUCK_ROT_Y_SELF_RIGHT = 15.0f; // Torque only allowed in self righting direction

float CAutomobile::CAR_INAIR_ROTLIM = 1.0f;
float CAutomobile::CAR_INAIR_REDUCED_ROTLIM = 0.2f;

float CAutomobile::CAR_BURNOUT_ROT_Z = -2.0f;
float CAutomobile::CAR_BURNOUT_ROTLIM = 0.8f;
float CAutomobile::CAR_BURNOUT_WHEELS_OFFGROUND_MULT = 1.5f;

float CAutomobile::BIG_CAR_BURNOUT_ROT_Z = -1.0f;
float CAutomobile::BIG_CAR_BURNOUT_ROTLIM = 0.35f;
float CAutomobile::BIG_CAR_BURNOUT_WHEEL_SPEED_MULT = 0.015f;

float CAutomobile::QUADBIKE_BURNOUT_ROT_Z = -4.5f;
float CAutomobile::QUADBIKE_BURNOUT_ROTLIM = 2.0f;
float CAutomobile::QUADBIKE_BURNOUT_WHEEL_SPEED_MULT = 0.1f;

float CAutomobile::OFFROAD_VEH_BURNOUT_ROT_Z = -4.0f;
float CAutomobile::OFFROAD_VEH_BURNOUT_ROTLIM = 1.0f;
float CAutomobile::OFFROAD_VEH_BURNOUT_WHEEL_SPEED_MULT = 0.1f;

float CAutomobile::BURNOUT_WHEEL_SPEED_MULT = 0.04f;
float CAutomobile::BURNOUT_TORQUE_MULT = 0.5f;

float CAutomobile::POSSIBLY_STUCK_SPEED = 0.7f;
float CAutomobile::COMPLETELY_STUCK_SPEED = 0.1f;

//
void CAutomobile::ProcessDriverInputsForStability(float fTimeStep)
{
	Assert(GetStatus()==STATUS_PLAYER);
	Assert(GetVehicleType()==VEHICLE_TYPE_CAR || GetVehicleType()==VEHICLE_TYPE_QUADBIKE || GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR || GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);

	m_nAutomobileFlags.bPlayerIsRevvingEngineThisFrame = false;

	if(!GetDriver() || !GetDriver()->IsPlayer())
		return;

	CControl *pControl = NULL;
	if(GetDriver())	pControl = GetDriver()->GetControlFromPlayer();
	if(pControl==NULL)
		return;
	
	int nNumContactWheels = GetNumContactWheels();
	float fVelocityIncludingReferenceFrameMag2 = GetVelocityIncludingReferenceFrame().Mag2();
	bool bPossiblyStuck = nNumContactWheels < 3 /*&& GetCollisionImpulseMag() > 0.0*/ &&  fVelocityIncludingReferenceFrameMag2 < square(POSSIBLY_STUCK_SPEED) && ( !HasHydraulicSuspension() || nNumContactWheels < 2 );
	bool bCompletelyStuckUpsideDown = CVehicle::sm_bUseNewSelfRighting && IsUpsideDown() && fVelocityIncludingReferenceFrameMag2 < square(COMPLETELY_STUCK_SPEED);

	float fRotSpeed, fRotForce;
	float fStickX = 0.0f;
	float fStickY = 0.0f;

	const bool bExitVehicleTaskRunning = GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE, true);
	bool bIsUpright = (!IsUpsideDown() && !IsOnItsSide());
	bool bCanPlayerUseStick = (bIsUpright ||
		!bExitVehicleTaskRunning ||
		IsInAir());

	TUNE_GROUP_BOOL(VEHICLE_DEBUG, DISALLOW_VEHICLE_STICK_CONTROL_FOR_NON_UPRIGHT_EXIT, true);
	if (DISALLOW_VEHICLE_STICK_CONTROL_FOR_NON_UPRIGHT_EXIT)
	{
		if (bExitVehicleTaskRunning)
		{
			const s32 iExitVehicleState = GetDriver()->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE);
			if (iExitVehicleState == CTaskExitVehicle::State_WaitForCarToSettle)
			{
				bCanPlayerUseStick = false;
			}
		}

		if (GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsExitingOnsideVehicle) || GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsExitingUpsideDownVehicle))
		{
			bCanPlayerUseStick = false;
		}
	}

	if( m_specialFlightModeRatio > 0.5f )
	{
		bCanPlayerUseStick = false;
	}

	if(bCanPlayerUseStick)
	{
		GetVehicleMovementStickInput(*pControl, fStickX, fStickY);

#if RSG_PC
		bool bMouseSteeringInput = false;
		bool bMouseOrKeyboardSteeringInput = false;
		if(pControl->WasKeyboardMouseLastKnownSource())
		{
			bMouseOrKeyboardSteeringInput = true;
			if(pControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
			{
				bMouseSteeringInput = true;
			}
		}

		bool bMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == CControl::eMSM_Vehicle;
		bool bCameraMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == CControl::eMSM_Camera;

		if( ((fabs(fStickX) <= 0.01f && bMouseOrKeyboardSteeringInput) || bMouseSteeringInput) && ((bMouseSteering && !CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()) || (bCameraMouseSteering && CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())))
		{
			fStickX = -m_fSteerInput;
		}
#endif	
	}

	// Scale the in air ability by the players stats.
	float fInAirAbilityMult = 1.0f;
	if(nNumContactWheels==0 &&GetDriver() && GetDriver()->IsLocalPlayer())
	{
		StatId stat = STAT_WHEELIE_ABILITY.GetStatId();
		float fWheelieStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
		float fMinWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MinWheelieAbility;
		float fMaxWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MaxWheelieAbility;

		fInAirAbilityMult = ((1.0f - fWheelieStatValue) * fMinWheelieAbility + fWheelieStatValue * fMaxWheelieAbility)/100.0f;
	}

	if(GetAttachParentVehicle() && GetAttachParentVehicle()->InheritsFromHeli() && ((CHeli *)GetAttachParentVehicle())->GetIsCargobob())
	{
		fInAirAbilityMult = 0.0f;
	}

	f32 rotationalForceX = 0.0f;
	f32 rotationalForceY = 0.0f;
	float fAftertouchScaleFactor = 1.0f;

	if(!bCompletelyStuckUpsideDown && (bPossiblyStuck || nNumContactWheels==0) && ( m_carParachuteState == CAR_NOT_PARACHUTING ) && !m_shuntModifierActive )
	{
		// We might not want to allow such severe "aftertouch" forces on pitch and roll on some vehicles e.g. forklift with
		// its short wheel base and low centre of mass.
		if(GetModelIndex() == MI_CAR_FORKLIFT || (pHandling && pHandling->mFlags & MF_HAS_TRACKS) || (GetVehicleType() == VEHICLE_TYPE_QUADBIKE) //|| GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE
			|| IsBusModelId(GetModelId()))
        {
			fAftertouchScaleFactor = 0.3f;
        }

		// Reduce "aftertouch" when a vehicle is in water. Stops cars flipping around while they are sinking.
		if(GetIsInWater() && !InheritsFromSubmarineCar())
        {
			fAftertouchScaleFactor *= 0.25f;
        }

		if(pHandling && pHandling->hFlags & HF_IMPROVED_RIGHTING_FORCE)//Some cars are harder to right due to their bounds so give them a helping hand.
		{
			fAftertouchScaleFactor = 2.0f;
		}
		else if( ( pHandling && pHandling->hFlags & HF_REDUCED_RIGHTING_FORCE ) ||
	 			 ( MI_CAR_BRICKADE.IsValid() && GetModelIndex() == MI_CAR_BRICKADE.GetModelIndex() ) )
		{
			static float sf_ReducedAfterTouchScale = 0.45f;
			fAftertouchScaleFactor = sf_ReducedAfterTouchScale;
		}
		else if(pHandling && ((pHandling->hFlags & HF_OFFROAD_ABILITIES_X2) || (pHandling->hFlags & HF_OFFROAD_INCREASED_GRAVITY_NO_FOLIAGE_DRAG)) )//Off road vehicles need to be able to cope with going in the air
		{
			fAftertouchScaleFactor = 1.5f;
		}

		if( MI_CAR_WASTELANDER.IsValid() && MI_CAR_WASTELANDER == GetModelIndex() )
		{
			static dev_float sfWastelanderAftertouchScale = 1.0f;
			fAftertouchScaleFactor = sfWastelanderAftertouchScale;
		}

		if( MI_CAR_VETO2.IsValid() && GetModelIndex() == MI_CAR_VETO2 )
		{
			static dev_float sfVetoAftertouchScale = 2.5f;
			fAftertouchScaleFactor = sfVetoAftertouchScale;
		}

		if( CPhysics::ms_bInArenaMode )
		{
			TUNE_GROUP_FLOAT( ARENA_MODE, AfterTouchScale, 1.5f, 1.0f, 10.0f, 0.1f );
			fAftertouchScaleFactor *= AfterTouchScale;
		}

		if( pHandling->mFlags & MF_IS_RC )
		{
			static dev_float sfRCCarAftertouchScale = 2.5f;
			fAftertouchScaleFactor = sfRCCarAftertouchScale;
		}

		//if( CVehicle::sm_bInDetonationMode )
		//{
		//	TUNE_GROUP_FLOAT( ARENA_MODE, detonationModeAfterTouchPitchScale, 1.5f, 1.0f, 10.0f, 0.1f );
		//	fAftertouchScaleFactor *= detonationModeAfterTouchPitchScale;
		//	fAftertouchScaleFactor *= 1.0f - Abs( fStickX );
		//}
		// Rotation about X (pitch)
		{
			const Vector3 vecRight(VEC3V_TO_VECTOR3(GetTransform().GetA()));

			fRotSpeed = DotProduct(GetAngVelocity(), vecRight);
			fRotForce = bPossiblyStuck ? CAR_STUCK_ROT_X : CAR_INAIR_ROT_X;

			if(fRotSpeed > 0.0f && fStickY < 0.0f)
			{
				fRotForce *= 1.0f + fRotSpeed/CAR_INAIR_ROTLIM;
			}
			else if(fRotSpeed < 0.0f && fStickY > 0.0f)
			{
				fRotForce *= 1.0f + fRotSpeed/-CAR_INAIR_ROTLIM;
			}

			float absRotationSpeed = fabs(fRotSpeed);
			// make sure we don't rotate faster than the limit
			if( (absRotationSpeed < CAR_INAIR_ROTLIM) || ( pHandling->mFlags & MF_IS_RC ) || fStickY != Sign(fRotSpeed) )
			{
				ApplyInternalTorque( fAftertouchScaleFactor * fStickY * fRotForce * GetAngInertia().x * vecRight * fInAirAbilityMult);
				rotationalForceX = fStickY * fRotForce;			
			}
		}

		// Rotation about Y (roll)
		{
			fRotSpeed = DotProduct(GetAngVelocity(), VEC3V_TO_VECTOR3(GetTransform().GetB()));

			if(!bPossiblyStuck)
			{
				if( ( /*!CVehicle::sm_bInDetonationMode &&*/ GetIsHandBrakePressed(pControl) ) )//||
					//( CVehicle::sm_bInDetonationMode && !GetIsHandBrakePressed(pControl) ) )
				{
					// if the handbrake is pressed we apply yaw to the car instead of roll
					fRotForce = 0.0f;
				}
				else
				{
					fRotForce = CAR_INAIR_ROT_Y;

					if(fRotSpeed > 0.0f && fStickX < 0.0f)
					{
						fRotForce *= 1.0f + fRotSpeed/CAR_INAIR_ROTLIM;
					}
					else if(fRotSpeed < 0.0f && fStickX > 0.0f)
					{
						fRotForce *= 1.0f + fRotSpeed/-CAR_INAIR_ROTLIM;
					}
				}
			}
			else if(Sign(fStickX) == Sign(GetTransform().GetA().GetZf()) || abs(GetTransform().GetA().GetZf()) < 0.1f)
			{
				// Trying to self right car
				// if a.z is negative then we need a negative torque to self right
				fRotForce = CAR_STUCK_ROT_Y_SELF_RIGHT * CVehicle::ms_fGravityScale;		// Scale this by gravity because it looks crazy on the moon
			}
			else
			{
				// We're stuck but pushing stick in opposite direction to what we think is sensible
				fRotForce = CAR_STUCK_ROT_Y;
			}

			// when the vehicle is upright, flip roll control so it matches normal steering input
			//if(GetC().z > 0.0f)
			//	fRotForce *= -1.0f;

			if( fRotForce != 0.0f )
			{
				float absRotationSpeed = fabs(fRotSpeed);
				float rotSpeedLimit = !pHandling->GetCarHandlingData() || !( pHandling->GetCarHandlingData()->aFlags & CF_REDUCED_SELF_RIGHTING_SPEED ) ? CAR_INAIR_ROTLIM : CAR_INAIR_REDUCED_ROTLIM;

				// make sure we don't rotate faster than the limit
				if( (absRotationSpeed < rotSpeedLimit ) || Sign(fStickX) != Sign(fRotSpeed) )
				{
					ApplyInternalTorque( fAftertouchScaleFactor * fStickX * fRotForce * GetAngInertia().y * VEC3V_TO_VECTOR3(GetTransform().GetB()) * fInAirAbilityMult);
					rotationalForceY = fStickX * fRotForce;				
				}
			}
		}

		//if( CVehicle::sm_bInDetonationMode &&
		//	fStickX == 0.0f &&
		//	fStickY == 0.0f &&
		//	( bIsUpright || !pControl->GetVehicleArenaModeModifier().HistoryHeldDown( 250 ) ) )
		//{
		//	Vector3 extraAngularDamping = -GetAngVelocity() * GetAngInertia() * fwTimer::GetInvTimeStep();
		//	static dev_float dampingScale = 0.25f;
		//	ApplyInternalTorque( extraAngularDamping * dampingScale );
		//}
	}

	//if( !IsRightWayUp() &&
	//	fStickX == 0.0f &&
	//	fStickY == 0.0f &&
	//	pControl->GetVehicleArenaModeModifier().HistoryHeldDown( 250 ) &&
	//	CVehicle::sm_bInDetonationMode )
	//{
	//	Vector3 rotationAxis = VEC3V_TO_VECTOR3( GetTransform().GetForward() );
	//	static dev_float torqueScale = 250.0f;
	//	Vector3 rightAxis = VEC3V_TO_VECTOR3( GetTransform().GetRight() );

	//	float rotationAmount = ( 1.25f - Abs( rightAxis.z ) );
	//	rotationAmount *= torqueScale * rotationAmount;
	//	float fZRotSpeed = GetAngVelocity().Dot( VEC3V_TO_VECTOR3( GetTransform().GetForward() ) );

	//	rotationAmount *= Max( 0.0f, 1.0f - ( Abs( fZRotSpeed ) / CAR_INAIR_ROTLIM ) );

	//	rotationAxis *= rotationAmount;
	//	rotationAxis *= GetAngInertia().y;

	//	if( rightAxis.z < 0.0f )
	//	{
	//		rotationAxis *= -1.0f;
	//	}

	//	ApplyTorque( rotationAxis );

	//}

	m_VehicleAudioEntity->SetInAirRotationalForces(rotationalForceX, rotationalForceY);

	bool bRotatingInBurnout = false;
	// Rotation about Z (yaw - handBrake)
	if( nNumContactWheels==0 && 
		/*(*/ GetIsHandBrakePressed(pControl) //|| 
		  //( CVehicle::sm_bInDetonationMode &&
			/*!GetIsHandBrakePressed(pControl) ) ))*/ && 
		GetTransform().GetC().GetZf() > 0.0f && 
		!m_nAutomobileFlags.bBurnoutModeEnabled && 
		( m_carParachuteState == CAR_NOT_PARACHUTING ) &&
		!m_shuntModifierActive )//don't let people spin the car when in burnout script mode
	{
		Vector3 vecUp(VEC3V_TO_VECTOR3(GetTransform().GetC()));
		fRotSpeed = DotProduct(GetAngVelocity(), vecUp);
		fRotForce = CAR_INAIR_ROT_Z;

		float absRotationSpeed = fabs(fRotSpeed);
		// make sure we don't rotate faster than the limit
		if( (absRotationSpeed < CAR_INAIR_ROTLIM) || fStickX != Sign(fRotSpeed) )
		{
			//if( CVehicle::sm_bInDetonationMode )
			//{
			//	TUNE_GROUP_FLOAT( ARENA_MODE, detonationModeAfterTouchScale, 10.0f, 1.0f, 100.0f, 0.1f );
			//	TUNE_GROUP_FLOAT( ARENA_MODE, detonationModeAfterTouchMaxYawSpeed, 3.0f, 1.0f, 100.0f, 0.1f );
			//	fAftertouchScaleFactor = 1.0f;
			//	fAftertouchScaleFactor *= detonationModeAfterTouchScale;

			//	//vecUp = Vector3( 0.0f, 0.0f, 1.0f );

			//	if( fRotSpeed > 0.0f && fStickX < 0.0f )
			//	{
			//		fRotForce *= 1.0f - fRotSpeed / detonationModeAfterTouchMaxYawSpeed;
			//	}
			//	else if( fRotSpeed < 0.0f && fStickX > 0.0f )
			//	{
			//		fRotForce *= 1.0f - fRotSpeed / -detonationModeAfterTouchMaxYawSpeed;
			//	}

			//	fRotForce *= fAftertouchScaleFactor;
			//}
			ApplyInternalTorque(fStickX * fRotForce * GetAngInertia().z * vecUp * fInAirAbilityMult);		
		}
	}
	// burnout controls (lets you rotate on the spot)
	else if((GetThrottle() > 0.9f && (GetBrake() > 0.9f||GetReduceGripMode())) && !GetHandBrake() && GetVehicleUpDirection().GetZf() > 0.0f && m_nVehicleFlags.bEngineOn && !m_nAutomobileFlags.bBurnoutModeEnabled && !(pHandling->mFlags & MF_HAS_TRACKS))//don't let people spin the car when in burnout script mode
	{
		m_nAutomobileFlags.bInBurnout = true;

		//Reduce how much the vehicle rotated bases on how many wheels are burst
		int iBurnoutWheels = 0;
		int iBurnoutWheelsNotOnGround = 0;
		float fBurnoutWheelHealth = 0;
		float fWheelRotSpeed = 0.0f;
		
		for(int i = 0; i < GetNumWheels(); i++)
		{
			CWheel *pWheel = GetWheel(i);
			if(pWheel->GetConfigFlags().IsFlagSet(WCF_POWERED) && (pWheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL) || pHandling->m_fDriveBiasRear < 0.1f))
			{
				fBurnoutWheelHealth += pWheel->GetTyreHealth();
				fWheelRotSpeed += pWheel->GetRotSpeed();
				iBurnoutWheels++;

				if(!pWheel->GetIsTouching())
				{
					iBurnoutWheelsNotOnGround++;
				}
			}
		}

		float fWheelsOffGroundTorqueMult = 1.0f;
		float fBurnoutForceMult = 1.0f;
		if(iBurnoutWheels > 0)
		{
			fBurnoutForceMult = fBurnoutWheelHealth / (TYRE_HEALTH_DEFAULT * iBurnoutWheels);
			fWheelRotSpeed = fWheelRotSpeed / iBurnoutWheels;

			//increase the torque used in burnout when wheels are off the ground.
			fWheelsOffGroundTorqueMult += CAR_BURNOUT_WHEELS_OFFGROUND_MULT * 1.0f - (iBurnoutWheelsNotOnGround / iBurnoutWheels);
		}

		const Vector3 vecUp(VEC3V_TO_VECTOR3(GetTransform().GetC()));
		fRotSpeed = DotProduct(GetAngVelocity(), vecUp);

		float fBurnoutRotZ = rage::Max(CAR_BURNOUT_ROT_Z, fWheelRotSpeed * BURNOUT_WHEEL_SPEED_MULT);
        float fBurnoutRotLimit = CAR_BURNOUT_ROTLIM;

        if(GetVehicleType() == VEHICLE_TYPE_QUADBIKE ||
			GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
        {
			fBurnoutRotZ = rage::Max(QUADBIKE_BURNOUT_ROT_Z, fWheelRotSpeed * QUADBIKE_BURNOUT_WHEEL_SPEED_MULT);
            fBurnoutRotLimit = QUADBIKE_BURNOUT_ROTLIM;

			if( GetNumWheels() == 3 )
			{
				static dev_float trikeBurnoutMult = 0.5f;

				fBurnoutRotZ		*= trikeBurnoutMult;
				fBurnoutRotLimit	*= trikeBurnoutMult;
			}
        }
		else if(pHandling && ((pHandling->hFlags & HF_OFFROAD_ABILITIES_X2) || (pHandling->hFlags & HF_OFFROAD_INCREASED_GRAVITY_NO_FOLIAGE_DRAG)))
		{
			fBurnoutRotZ = rage::Max(OFFROAD_VEH_BURNOUT_ROT_Z, fWheelRotSpeed * OFFROAD_VEH_BURNOUT_WHEEL_SPEED_MULT);
			fBurnoutRotLimit = OFFROAD_VEH_BURNOUT_ROTLIM;
		}
		else if( GetNumWheels() == 3 )
		{
			static dev_float threeWheelerBurnoutMult = 2.5f;
			fBurnoutForceMult *= threeWheelerBurnoutMult;
		}
		else if( ( MI_CAR_RUSTON.IsValid() && GetModelIndex() == MI_CAR_RUSTON ) ||
                 ( MI_CAR_BRIOSO2.IsValid() && GetModelIndex() == MI_CAR_BRIOSO2 ) )
		{
			static dev_float rustonBurnoutMult = 2.0f;
			fBurnoutForceMult *= rustonBurnoutMult;
		}

		if(m_nVehicleFlags.bIsBig)
		{
			if( !HasRammingScoop() )
			{
				fBurnoutRotZ = rage::Max(BIG_CAR_BURNOUT_ROT_Z, fWheelRotSpeed * BIG_CAR_BURNOUT_WHEEL_SPEED_MULT);
			}
			else
			{
				fBurnoutRotZ = rage::Max(BIG_CAR_BURNOUT_ROT_Z, fWheelRotSpeed * QUADBIKE_BURNOUT_WHEEL_SPEED_MULT);
			}
			fBurnoutRotLimit = BIG_CAR_BURNOUT_ROTLIM;
		}

		if( fabs(fStickX) > 0.001f )
		{
			m_fBurnoutRotationTime += fTimeStep * fStickX;
			m_fBurnoutRotationTime = rage::Clamp( m_fBurnoutRotationTime, -1.0f, 1.0f );
			bRotatingInBurnout = true;
			
			float absRotationSpeed = fabs(fRotSpeed);
			// make sure we don't rotate faster than the limit
			if( (absRotationSpeed < fBurnoutRotLimit) || fStickX == Sign(fRotSpeed) )
			{
				ApplyInternalTorque(m_fBurnoutRotationTime * fWheelsOffGroundTorqueMult * fBurnoutForceMult * fBurnoutRotZ * GetAngInertia().z * vecUp);
			}
		}
		else
		{
			bRotatingInBurnout = false;
		}
	}
	else
	{
		m_nAutomobileFlags.bInBurnout = false;
		bRotatingInBurnout = false;
	}

	if( !m_nVehicleFlags.bScriptSetHandbrake &&
		( GetVehicleModelInfo()->GetVehicleClass() == VC_MUSCLE ||
          ( pHandling->GetCarHandlingData() && pHandling->GetCarHandlingData()->aFlags & CF_CAN_WHEELIE ) ||
          GetModelIndex() == MI_CAR_TORNADO6 ) &&
		( GetThrottle() > 0.9f || ( m_nAutomobileFlags.bWheelieModeEnabled && GetThrottle() >= 0.0 ) ) &&
		  ( m_Transmission.GetRevRatio() > 0.8f || ( m_nAutomobileFlags.bWheelieModeEnabled &&  m_Transmission.GetRevRatio() > 0.3f ) ) &&
		  m_nVehicleFlags.bEngineOn && GetNumWheels() == GetNumContactWheels() && fVelocityIncludingReferenceFrameMag2 < 1.0f )
	{
		if( GetHandBrake() )
		{
			m_nAutomobileFlags.bWheelieModeEnabled = true;
		}
		else if( m_nAutomobileFlags.bWheelieModeEnabled )
		{
			m_nAutomobileFlags.bInWheelieMode = true;
			m_nAutomobileFlags.bWheelieModeEnabled = false;
			m_nAutomobileFlags.bInWheelieModeWheelsUp = false;
		}
	}
	else
	{
		m_nAutomobileFlags.bWheelieModeEnabled = false;
	}

	int numContactWheels = GetNumContactWheels();

	if( GetBrake() > 0.2f ||
		( GetThrottle() == 0.0f && 
		  ( fVelocityIncludingReferenceFrameMag2 < 10.0f ||
			( numContactWheels == GetNumWheels() &&
			  Abs( GetSteerAngle() ) > 0.5f ) ) ) )
	{
		m_nAutomobileFlags.bInWheelieMode = false;
		m_nAutomobileFlags.bInWheelieModeWheelsUp = false;
	}

	if( m_nAutomobileFlags.bInWheelieMode )
	{
		if( ( m_Transmission.GetGear() > 2 &&
			  numContactWheels == GetNumWheels() ) )// ||
			//numContactWheels == 0 )
		{
			m_nAutomobileFlags.bInWheelieMode = false;
			m_nAutomobileFlags.bInWheelieModeWheelsUp = false;
		}

		if( numContactWheels > 0 )
		{
			Vector3 localWheelOffset = CWheel::GetWheelOffset( GetVehicleModelInfo(), VEH_WHEEL_LR );
			CWheel* wheel = GetWheelFromId( VEH_WHEEL_LR );

			if( !wheel->GetIsTouching() )
			{
				wheel = GetWheelFromId( VEH_WHEEL_RR );
			}

			if( wheel->GetIsTouching() )
			{
				if( numContactWheels == 2 )
				{
					m_nAutomobileFlags.bInWheelieModeWheelsUp = true;
				}
				else if( numContactWheels > 2 )
				{
					m_nAutomobileFlags.bInWheelieModeWheelsUp = false;
				}

				Vector3 hitNormal = wheel->GetHitNormal();
				Vector3 vehicleUp = VEC3V_TO_VECTOR3( GetTransform().GetUp() );

				localWheelOffset.x = 0.0f;
				static dev_float sfMinYOffset = -1.75f;
				static dev_float sfMinZOffset = -0.175f;
				localWheelOffset.y = Max( localWheelOffset.y, sfMinYOffset );
				localWheelOffset.z = Max( localWheelOffset.z, sfMinZOffset );

				Vector3 wheelOffset = localWheelOffset;
				wheelOffset = VEC3V_TO_VECTOR3( GetTransform().Transform3x3( VECTOR3_TO_VEC3V( wheelOffset ) ) );

				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfWheeliTorqueScale, 2.6f, 0.0f, 10.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfwheelieTorqueMag, 8.0f, 0.0f, 20.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfWheelitAngleTorqueScaleMin, 0.0f, 0.0f, 20.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfWheelitAngleTorqueScaleMax, 1.0f, 0.0f, 20.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfOptimumThrottleThreshold, 0.15f, 0.0f, 20.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfOptimumThrottleTorqueScale, 0.5f, 0.0f, 20.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_WHEELIE, sfMinEngineMod, 0.8f, 0.0f, 20.0f, 0.1f );

				float angleToqueScale = Clamp( 1.0f - Abs( ( hitNormal.z - vehicleUp.z ) * 2.0f ), 0.0f, 1.0f );

				if( pHandling->GetCarHandlingData() )
				{
					for( int i = 0; i < pHandling->GetCarHandlingData()->m_AdvancedData.GetCount(); i++ )
					{
						int advancedDataModSlot = pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Slot;

						if( advancedDataModSlot != -1 )
						{
							bool variation = false;
							int modIndex = (int)GetVariationInstance().GetModIndexForType( (eVehicleModType)advancedDataModSlot, this, variation );
					
							if( modIndex == pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Index )
							{
								float advancedDataTorqueScale = pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Value;
								angleToqueScale *= advancedDataTorqueScale;
							}
						}
					}
				}

				angleToqueScale *= angleToqueScale;
				angleToqueScale = sfWheelitAngleTorqueScaleMin + ( sfWheelitAngleTorqueScaleMax * angleToqueScale );

				float optimumThrottle = angleToqueScale;
				float lastDriveForce = m_Transmission.GetLastCalculatedDriveForce() / rage::Min( 1.0f, 2.0f * m_Transmission.GetClutchRatio() ); // scale the drive force back up by the clutch ratio so that it continues to wheelie while changing gear

				float torqueScaleWithEngineMod = Min( 1.0f, sfMinEngineMod + ( (float)GetVariationInstance().GetEngineModifier() * 0.002f ) );
				float maxWheelieForce = Min( sfwheelieTorqueMag, lastDriveForce * sfWheeliTorqueScale * angleToqueScale * torqueScaleWithEngineMod );

				if( Abs( GetThrottle() - optimumThrottle ) < sfOptimumThrottleThreshold )
				{
					lastDriveForce += ( lastDriveForce * sfOptimumThrottleTorqueScale ) * ( Abs( GetThrottle() - optimumThrottle ) / sfOptimumThrottleThreshold );
				}

				Vector3 wheelieTorque( maxWheelieForce, 0.0f, 0.0f );
				wheelieTorque *= GetAngInertia();
				wheelieTorque = VEC3V_TO_VECTOR3( GetTransform().Transform3x3( VECTOR3_TO_VEC3V( wheelieTorque ) ) );

				localWheelOffset = VEC3V_TO_VECTOR3( GetTransform().Transform( VECTOR3_TO_VEC3V( localWheelOffset ) ) );

				ApplyInternalTorqueAndForce( wheelieTorque, localWheelOffset );
			}
		}
		else if( fVelocityIncludingReferenceFrameMag2 < 2.0f ||
				 fVelocityIncludingReferenceFrameMag2 > 15.0f )
		{ 
			m_nAutomobileFlags.bInWheelieMode = false; 
			m_nAutomobileFlags.bInWheelieModeWheelsUp = false;
		}
	}

	//reduce rotating burnout timers if not rotating.
	if(bRotatingInBurnout == false)
	{
		float fOriginalBurnoutTime = m_fBurnoutRotationTime;
		if(fOriginalBurnoutTime > 0.0f)
		{
			m_fBurnoutRotationTime -= fTimeStep;
			if(fOriginalBurnoutTime < 0.0f)
			{
				m_fBurnoutRotationTime = 0.0f;
			}
		}
		else
		{
			m_fBurnoutRotationTime += fTimeStep;
			if(fOriginalBurnoutTime > 0.0f)
			{
				m_fBurnoutRotationTime = 0.0f;
			}
		}
	}

	m_fParachuteStickX = fStickX;
	m_fParachuteStickY = fStickY;	
}



PF_PAGE(GTAAutos, "GTA Autos");
PF_GROUP(AutosProcessControl);
PF_LINK(GTAAutos, AutosProcessControl);

PF_TIMER(Misc1, AutosProcessControl);
PF_TIMER(ProcessVehicle, AutosProcessControl);
PF_TIMER(Misc2, AutosProcessControl);
PF_TIMER(ControlOptions, AutosProcessControl);
PF_TIMER(ProcessDriverInputs, AutosProcessControl);
PF_TIMER(ProcessIntelligence, AutosProcessControl);
PF_TIMER(Damage, AutosProcessControl);

PF_GROUP(AutosAI);
PF_LINK(GTAAutos, AutosAI);
PF_TIMER(FindTargetCoorsAndSpeed, AutosAI);

// remember there's a bike version of this code as well
dev_float sfCarNotSlowResistanceMult = 0.9f;
dev_float sfCarSlowResistanceMult = 1.1f;
#if __PS3
dev_float sfCarSlowResistanceMultNetwork = 1.1f;
#else
dev_float sfCarSlowResistanceMultNetwork = 1.1f;
#endif
//dev_float sfCarSlowResistanceMultCheat1 = 1.5f;
//dev_float sfCarSlowResistanceMultCheat2 = 1.25f;

//
void CAutomobile::UpdateAirResistance() 
{
	CPed *pDriver = GetDriver();
	CPlayerInfo *pPlayer = NULL;
	if(pDriver)
	{
		pPlayer = pDriver->GetPlayerInfo();
	}

	// If it's not driven by a player, 
	fragInst* pFragInst = GetVehicleFragInst();
	if(!pPlayer)
	{
		const int lod = pFragInst->GetCurrentPhysicsLOD();

		// Compute some value representing the state of the vehicle that would affect it's
		// damping constants, and check if that's any different than the values we have already set.
		// The addition of one is here because we need to make sure that it never computes to zero,
		// so that setting m_AirResistanceState to zero always refreshes the air resistance. The
		// shift by five bits of bInSlownessZone is also fairly arbitrary, we just need it to be
		// large enough to not overlap with the lod, and small enough so it still fits in a byte.
		unsigned int airResState = lod + 1 + (m_nVehicleFlags.bInSlownessZone << 5);
		if(m_AirResistanceState == airResState)
		{
			Assert(GetSlipStreamEffect() == 0.0f);
			return;
		}
		Assert(airResState <= 0xff);
		m_AirResistanceState = (u8)airResState;
	}
	else
	{
		// Make sure we refresh once the player is no longer the driver.
		m_AirResistanceState = 0;
	}

	float fAirResistance = m_fDragCoeff * sfCarNotSlowResistanceMult;

	if( m_nVehicleFlags.bInSlownessZone BANK_ONLY(|| CVehicle::ms_bForceSlowZone) )
	{
		if(NetworkInterface::IsGameInProgress())
			fAirResistance = m_fDragCoeff*sfCarSlowResistanceMultNetwork;
		else
			fAirResistance = m_fDragCoeff*sfCarSlowResistanceMult;
	}

	float fSlipStreamEffect =  0.0f;
	if(pPlayer)
	{
		if (pPlayer->m_fForceAirDragMult > 0.0f)
		{
			float fNewAirResistance = fAirResistance * pPlayer->m_fForceAirDragMult;
			if(fNewAirResistance > fAirResistance)
			{
				fAirResistance = fNewAirResistance;
			}
		}

		// Update air resistance for slip stream
		fSlipStreamEffect = GetSlipStreamEffect();
	}
	else
	{
		// For performance, we are taking advantage of the current fact that GetSlipStreamEffect()
		// doesn't return varying values for AI, and in fact, hopefully we won't have to call it
		// at all. But, this assert would tell us if somebody tries to use it for AI:
		Assert(GetSlipStreamEffect() == 0.0f);
	}

    if( pHandling->GetCarHandlingData() &&
        pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
    {
        float fDownforceModifierFront = m_fDownforceModifierFront;
        float fDownforceModifierRear = m_fDownforceModifierRear;

        static dev_float sfDownforceReductionForBrokenWing = 0.3f; // THIS IS A DIFFERENT VALUE TO THE ONE IN WHEEL AS WE WANT TO REDUCE DOWNFORCE BY MORE THAN WE REDUCE DRAG

        int frontWingBoneIndex = GetBoneIndex( VEH_EXTRA_6 );
        int childIndex = frontWingBoneIndex == -1 ? -1 : GetVehicleFragInst()->GetComponentFromBoneIndex( frontWingBoneIndex );

        if( !GetIsExtraOn( VEH_EXTRA_6 ) ||
            ( childIndex != -1 &&
              GetVehicleFragInst()->GetChildBroken( childIndex ) ) )
        {
            fDownforceModifierFront *= sfDownforceReductionForBrokenWing;
        }

        int rearWingBoneIndex = GetBoneIndex( VEH_EXTRA_1 );
        childIndex = rearWingBoneIndex == -1 ? -1 : GetVehicleFragInst()->GetComponentFromBoneIndex( rearWingBoneIndex );

        if( !GetIsExtraOn( VEH_EXTRA_1 ) ||
            ( childIndex != -1 &&
              GetVehicleFragInst()->GetChildBroken( childIndex ) ) )
        {
            fDownforceModifierRear *= sfDownforceReductionForBrokenWing;
        }

        float averageDownforce = ( ( fDownforceModifierFront + fDownforceModifierRear ) * 0.5f ) - 1.0f;
        TUNE_GROUP_FLOAT( VEHICLE_DOWNFORCE_TUNE, DRAG_INCREASE_FACTOR, 0.35f, 0.0f, 10.0f, 0.01f );

        fAirResistance += fAirResistance * averageDownforce * DRAG_INCREASE_FACTOR;
    }

	fragPhysicsLOD* pFragPhysics = pFragInst->GetTypePhysics();
	phArchetypeDamp* pArch = (phArchetypeDamp*)pFragInst->GetArchetype();

	Vector3 vecLinearCDamping = pFragPhysics->GetDampingConstant(phArchetypeDamp::LINEAR_C);
	Vector3 vecLinearVDamping = pFragPhysics->GetDampingConstant(phArchetypeDamp::LINEAR_V);
	Vector3 vecLinearV2Damping;
	vecLinearV2Damping.Set(fAirResistance, fAirResistance, fAirResistance);

	vecLinearCDamping -= (vecLinearCDamping * fSlipStreamEffect);
	vecLinearVDamping -= (vecLinearVDamping * fSlipStreamEffect);
	vecLinearV2Damping -= (vecLinearV2Damping * fSlipStreamEffect);

	pArch->ActivateDamping(phArchetypeDamp::LINEAR_C, vecLinearCDamping);
	pArch->ActivateDamping(phArchetypeDamp::LINEAR_V, vecLinearVDamping);
	TUNE_GROUP_FLOAT(VEHICLE_DAMPING, AUTOMOBILE_V2_DAMP_MULT, 1.0f, 0.0f, 5.0f, 0.01f);

	pArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecLinearV2Damping * AUTOMOBILE_V2_DAMP_MULT);

	// Note: for the dummy instance, changing these values is a bit dubious since
	// multiple instances actually point to the same phArchetypeDamp object. In practice,
	// there are no known problems. We might want to not even do it at all, but now
	// when we don't have to do this on each frame for all AI vehicles, it shouldn't matter much.
	phInstGta* pDummyInst = GetDummyInst();
	if(pDummyInst)
	{	
		phArchetypeDamp* pDummyArch = ((phArchetypeDamp*)pDummyInst->GetArchetype());
		pDummyArch->ActivateDamping(phArchetypeDamp::LINEAR_C, vecLinearCDamping);
		pDummyArch->ActivateDamping(phArchetypeDamp::LINEAR_V, vecLinearVDamping);
		pDummyArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecLinearV2Damping);
	}
}

dev_float RANDOM_SIREN_ACTIVATION_PROB = 0.1f;
static dev_float sfGroundClearanceFastVelocitySquared = 7.0f*7.0f;
static dev_float sfFastGroundClearance = 0.50f;
static dev_float sfRCCarFastGroundClearance = 0.30f;
static dev_float sfFastGroundClearanceForHydraulics = 0.35f;
static dev_float sfSlowGroundClearance = 0.0f;
static dev_float sfSlowGroundClearanceForHydraulics = 0.25f;
//
void CAutomobile::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
//////////////////////////////
// Do some debug stuff first (not that useful since rarely play Debug build)
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
///////////////////////////////

	if(fullUpdate)
	{
		PF_START(Misc1);

		//reset
		m_nAutomobileFlags.bIsBoggedDownInSand = false;
		
		// Randomly activate the siren if set to stop them all being activated at the same time
		if( m_nVehicleFlags.GetRandomlyDelaySiren() && (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < RANDOM_SIREN_ACTIVATION_PROB ) )
			 m_nVehicleFlags.SetRandomlyDelaySiren(false);
		
		// service the audio entity - this should be done once a frame 
		// we want to ALWAYS do it, even if car is totally stationary and inert
		//	m_VehicleAudioEntity.Update();

		if (PopTypeGet() == POPTYPE_RANDOM_PARKED && GetDriver() && !GetDriver()->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			// The vehicle now has a driver so it is no longer just a placed vehicle (as it can drive away now).
			PopTypeSet(POPTYPE_RANDOM_AMBIENT);
		}

		// Generate a horn shocking event, if the horn is being played
		if(IsHornOn() && ShouldGenerateMinorShockingEvent())
		{
			CEventShockingHornSounded ev(*this);
			CShockingEventsManager::Add(ev);
		}
			
		// need to process a few things now in case fn returns e.g. for simple cars
		if(m_nVehicleFlags.bIsBus)
			ProcessAutoBusDoors();
		else if (GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
			ProcessAutoTurretDoors();

		ProcessCarAlarm(fFullUpdateTimeStep);
		UpdateInSlownessZone();

		if((GetVehicleType()==VEHICLE_TYPE_CAR || GetVehicleType()==VEHICLE_TYPE_QUADBIKE || GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
		{
			UpdateAirResistance();

			for( int i = 0; i < m_nNumWheels; i++ )
			{
				if( m_ppWheels[ i ] )
				{
					m_ppWheels[ i ]->UpdateDownforce();
				}
			}

		}


		PF_STOP(Misc1);

		// process special options for cars (vehicle specific controls and other misc bits)
		PF_START(ControlOptions);
		ProcessDriverInputOptions();
		PF_STOP(ControlOptions);
	}

	//process the tasks (unless we were just created on this frame, in which case we've already done this via CVehiclePopulation::MakeVehicleIntoDummyBasedOnDistance)
	if (!m_nVehicleFlags.bHasProcessedAIThisFrame)
	{
		//Process AI and player tasks, this also processes player controls
		PF_START(ProcessIntelligence);
		ProcessIntelligence(fullUpdate, fFullUpdateTimeStep);
		PF_STOP(ProcessIntelligence);
	}

	const bool updateVehicleDamageEveryFrame = ShouldUpdateVehicleDamageEveryFrame();
	if(fullUpdate)
	{
		//modify ground clearance depending on speed
		float fVehSpeedSq = GetVelocityIncludingReferenceFrame().Mag2();

		float fFastGroundClearance = sfFastGroundClearance;
		float fSlowGroundClearance = sfSlowGroundClearance;

        static dev_float sfOutriggerVehicleFastGroundClearance = sfFastGroundClearance * 1.5f;
        static dev_float sfOutriggerVehicleSlowGroundClearance = 0.35f;

        if( GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_OUTRIGGER_LEGS ) )
        {
            fFastGroundClearance = sfOutriggerVehicleFastGroundClearance;
            fSlowGroundClearance = sfOutriggerVehicleSlowGroundClearance;
        }
        if( GetModelIndex() == MI_TANK_KHANJALI )
        {
            fFastGroundClearance = sfOutriggerVehicleFastGroundClearance;
        }
		if( pHandling->mFlags & MF_IS_RC )
		{
			fSlowGroundClearance = 0.0f;
			fFastGroundClearance = sfRCCarFastGroundClearance;
		}
		if(GetModelIndex() == MI_CAR_VETO || GetModelIndex() == MI_CAR_VETO2)
		{
			fFastGroundClearance = 0.30f;
		}

		if(HasHydraulicSuspension())
		{
			fFastGroundClearance = sfFastGroundClearanceForHydraulics;

			bool hydraulicsActive = (	m_nVehicleFlags.bPlayerModifiedHydraulics || 
										m_nAutomobileFlags.bHydraulicsBounceRaising || 
										m_nAutomobileFlags.bHydraulicsBounceLanding || 
										m_nAutomobileFlags.bHydraulicsJump );

			if( !hydraulicsActive )
			{
				fSlowGroundClearance = sfSlowGroundClearanceForHydraulics;
			}
		}

		float fGroundClearance = Selectf(fVehSpeedSq-sfGroundClearanceFastVelocitySquared,fFastGroundClearance,fSlowGroundClearance);
#if __BANK
		if(ms_fForcedGroundClearance != 0.0f)// if force ground clearance has been set then use that
		{
			fGroundClearance = ms_fForcedGroundClearance;
		}
#endif
		bool brokenWheels = false;

		if( GetWheelBrokenIndex() != 0 )
		{
			fGroundClearance = sfSlowGroundClearance;
			brokenWheels = true;
		}

		// B*1853026: Make sure the bounds are lowered if we're in the air so we don't land on something and get stuck due to raised bounds.
		if( IsInAir() && 
			!CPhysics::ms_bInStuntMode &&
			( !InheritsFromTrailer() ||
			  !GetAttachParent() ) ) 
		{
			fGroundClearance = fSlowGroundClearance;
		}

		if(InheritsFromAmphibiousQuadBike() && !static_cast<CAmphibiousQuadBike*>(this)->IsWheelsFullyOut())
		{
			fGroundClearance = fSlowGroundClearance;
		}

		bool forceUpdateGroundClearance = m_nAutomobileFlags.bForceUpdateGroundClearance;

		// for the submarine car we need to force the bounds back down when the wheels move up to stop the vehicle getting stopped a long way in the ground
		if( InheritsFromSubmarineCar() )
		{
			CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( this );
			float animPhase = submarineCar->GetAnimationPhase();

            float maxGroundClearance = sfFastGroundClearance;
            fSlowGroundClearance = sfSlowGroundClearanceForHydraulics;

            if( submarineCar->GetAnimationState() == CSubmarineCar::State_MoveWheelsUp ||
                submarineCar->GetAnimationState() == CSubmarineCar::State_MoveWheelsDown )
            {
                maxGroundClearance = ( 1.0f - Min( 1.0f, submarineCar->GetAnimationPhase() * 2.0f ) ) * fSlowGroundClearance;
            }

			if( !submarineCar->AreWheelsActive() )
			{
				animPhase = 1.0f;
                maxGroundClearance = 0.0f;
			}

			fGroundClearance = fSlowGroundClearance + ( Max( 0.0f, ( fGroundClearance - fSlowGroundClearance ) ) * ( 1.0f - animPhase ) );
            fGroundClearance = Min( maxGroundClearance, fGroundClearance );
			forceUpdateGroundClearance = animPhase > 0.0f && fGroundClearance < sfFastGroundClearance && fGroundClearance != m_fGroundClearance;
		}
		if( GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_OUTRIGGER_LEGS ) )
		{
			float animPhase = GetOutriggerDeployRatio();
			float maxGroundClearance = sfOutriggerVehicleFastGroundClearance;

			if( animPhase > 0.0f )
			{
				maxGroundClearance *= ( 1.0f - Min( 1.0f, animPhase * 2.0f ) );
			}

			fGroundClearance = Min( maxGroundClearance, fGroundClearance );
			forceUpdateGroundClearance = animPhase > 0.0f && fGroundClearance < sfOutriggerVehicleFastGroundClearance && fGroundClearance != m_fGroundClearance;
		}

		bool bBoundsNeedUpdating = UpdateDesiredGroundGroundClearance(fGroundClearance, brokenWheels, forceUpdateGroundClearance );
		if( bBoundsNeedUpdating )
		{
			m_nAutomobileFlags.bForceUpdateGroundClearance = false;
		}

		// If we raise the ground clearance, also make sure the bumper collision is turned off if applicable to this vehicle.
		if(bBoundsNeedUpdating)
		{
			const CVehicleModelInfo* pVehicleModelInfo = GetVehicleModelInfo();
			physicsFatalAssertf(pVehicleModelInfo, "This vehicle (%s) has no model info. It is unsafe to continue.", GetModelName());
			const CVehicleModelInfoBumperCollision* pBumperColExtension = pVehicleModelInfo->GetExtension<CVehicleModelInfoBumperCollision>();
			const s32 NUM_BUMPERS_TO_ENABLE_COLLISION = 2;
			static eHierarchyId aBumperPartsToEnableCollision[NUM_BUMPERS_TO_ENABLE_COLLISION] = 
			{
				VEH_BUMPER_F,
				VEH_BUMPER_R,
			};

			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());
			const fragPhysicsLOD* physicsLOD = GetVehicleFragInst()->GetTypePhysics();
			for(u32 i = 0; i < NUM_BUMPERS_TO_ENABLE_COLLISION; ++i)
			{
				int nBoneIdx = GetBoneIndex(aBumperPartsToEnableCollision[i]);
				if(nBoneIdx!=-1)
				{
					int nGroup = GetVehicleFragInst()->GetGroupFromBoneIndex(nBoneIdx);
					if(nGroup > -1)
					{
						fragTypeGroup* pGroup = physicsLOD->GetGroup(nGroup);
						int iChild = pGroup->GetChildFragmentIndex();

						for(int k = 0; k < pGroup->GetNumChildren(); ++k)
						{
							u32 nIncludeFlags = ArchetypeFlags::GTA_CAR_BUMPER_INCLUDE_TYPES;

							// Decide if the bound is being raised or lowered.
							if(fGroundClearance >= fFastGroundClearance)
							{
								nIncludeFlags &= ~ArchetypeFlags::GTA_RAGDOLL_TYPE;
								nIncludeFlags &= ~ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
								nIncludeFlags &= ~ArchetypeFlags::GTA_PED_TYPE;
							}
							else
							{
								//Assert(fGroundClearance <= fSlowGroundClearance);
								if( ( pBumperColExtension && pBumperColExtension->GetBumpersNeedMapCollision() ) )
								{
									nIncludeFlags = ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;
								}
							}

							if( HasRamp() )
							{
								nIncludeFlags |= ArchetypeFlags::GTA_VEHICLE_BVH_TYPE;
								nIncludeFlags |= ArchetypeFlags::GTA_VEHICLE_TYPE;
							}
							pBoundComp->SetIncludeFlags(iChild+k, nIncludeFlags);
						}
					}
				}
			} // Loop over bumpers.
		}

		//Clear vehicles flagged to raise bounds due to towing, if they are now back at regular clearance
		if(m_nVehicleFlags.bTowedVehBoundsCanBeRaised && m_fGroundClearance == fSlowGroundClearance && !GetIsBeingTowed())
		{
			m_nVehicleFlags.bTowedVehBoundsCanBeRaised = false;
		}

		// check for and apply vehicle damage
		PF_START(Damage);
		float damageTimeStep = updateVehicleDamageEveryFrame ? fwTimer::GetTimeStep() : fFullUpdateTimeStep;
		GetVehicleDamage()->Process(damageTimeStep, bBoundsNeedUpdating);
		PF_STOP(Damage);

		PF_START(Misc2);
		ProcessSirenAndHorn(true) /*&& ! CCheat::IsCheatActive(CCheat::STRONGGRIP_CHEAT)*/;
		if(GetStatus() == STATUS_PLAYER)
		{
			ProcessDirtLevel();
		}

		// Update the timer determining how dangerously the vehicle is being driven
		UpdateDangerousDriverEvents(fFullUpdateTimeStep);

		PF_STOP(Misc2);
	}
	else if(updateVehicleDamageEveryFrame)
	{
		const bool bBoundsNeedUpdating = false;		// Hopefully this should be OK so we don't have to call UpdateDesiredGroundGroundClearance().
		GetVehicleDamage()->Process(fwTimer::GetTimeStep(), bBoundsNeedUpdating);
	}

	// Don't allow hydraulic modifications and reset the tilt if a ped is trying to enter, exit or jack the vehicle (ie a vehicle seat has been reserved).
	if (HasHydraulicSuspension())
	{
		m_bBlockDueToEnterExit = false;
		const CModelSeatInfo* pModelSeatInfo = GetVehicleModelInfo() ? GetVehicleModelInfo()->GetModelSeatInfo() : NULL;

		if (pModelSeatInfo && GetSeatManager())
		{
			for (int i = 0; i < pModelSeatInfo->GetNumSeats(); i++)
			{
				CPed* pPedInSeat = GetSeatManager()->GetPedInSeat(i);
				CComponentReservationManager* pCompMgr = GetComponentReservationMgr();
				CComponentReservation* pReservation = pCompMgr ? pCompMgr->GetSeatReservationFromSeat(i) : NULL;

				// Don't allow hydraulic modification if a ped is trying to enter the vehicle (ie a vehicle seat has been reserved) or jack someone from it.
				if (pReservation->IsComponentInUse() && (!pPedInSeat || pPedInSeat != pReservation->GetPedUsingComponent()))
				{
					m_bBlockDueToEnterExit = true;
				}
				// ... or if ped is exiting the vehicle
				if (pPedInSeat && pPedInSeat->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
				{
					m_bBlockDueToEnterExit = true;
				}
			}
		}

		if (m_bBlockDueToEnterExit)
		{
			// Reset suspension height so it doesn't mess with enter/exit anims that much
			for(int i=0; i < GetNumWheels(); i++)
			{
				CWheel* pWheel = GetWheel(i);
				pWheel->SetSuspensionTargetRaiseAmount(0.0f,1.0f);
				pWheel->SetSuspensionHydraulicTargetState(WHS_FREE);
			}
			ActivatePhysics();
			CAutomobile* pAuto = static_cast<CAutomobile*>(this);
			pAuto->SetTimeHyrdaulicsModified( fwTimer::GetTimeInMilliseconds() );
		}
	}

	CVehicle::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
}//end of CAutomobile::ProcessControl()...

#if __BANK
void CAutomobile::ValidateDummyConstraints(Vec3V_In vVehPos, Vec3V_In vConstraintPos, float fConstraintLength)
{
	if(!CVehicleAILodManager::ms_bValidateDummyConstraints)
	{
		return;
	}
	// Check the constraint and report if it is starting off unsatisfied
	const ScalarV fConstraintLengthScalar(fConstraintLength);
	const ScalarV fOverconstrainedThreshold(CVehicleAILodManager::ms_fValidateDummyConstraintsBuffer);
	const ScalarV fVehDist = Dist(vVehPos,vConstraintPos);
	if(IsGreaterThanAll(fVehDist-fConstraintLengthScalar,fOverconstrainedThreshold))
	{
		Warningf("Dummy constraint is too short for veh at (%0.2f,%0.2f), current LOD = %d, fVehDist = %0.2f, fConstraintLength = %0.2f",vVehPos.GetXf(),vVehPos.GetYf(),GetVehicleAiLod().GetDummyMode(),fVehDist.Getf(),fConstraintLength);
		bool bCullingWasDisabled = grcDebugDraw::GetDisableCulling();
		grcDebugDraw::SetDisableCulling(true);
		const Vec3V vConstraintEnd = Lerp(fConstraintLengthScalar/fVehDist,vConstraintPos,vVehPos);
		grcDebugDraw::Line(vConstraintPos,vConstraintEnd,Color_blue,-20);
		grcDebugDraw::Line(vConstraintEnd,vVehPos,Color_red,-20);
		grcDebugDraw::SetDisableCulling(bCullingWasDisabled);
	}
}
#endif

float CAutomobile::GetMaxLengthForDummyConstraintsAfterFailedConversion(bool& bMaxShrinkage) const
{
	//don't reduce limits when moving slowly as it causes the vehicle to noticably slide across the road
	if (m_TimeStartedFailingRealConversion == 0 || m_fConstraintDistanceWhenRealConversionFailed < 0.0f || GetVelocity().Mag2() < 9.0f)
	{
		bMaxShrinkage = false;
		return -1.0f;
	}

	const u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();
	const dev_float s_fSpeedToReduceConstraints = 0.5f; //in m/s

	const float fTimeDelta = (nCurrentTime - m_TimeStartedFailingRealConversion) * 0.001f;

	const float fReduceAmount = fTimeDelta * s_fSpeedToReduceConstraints;

	const float fMaxLength = rage::Max(GetHeightAboveRoad()+0.1f, m_fConstraintDistanceWhenRealConversionFailed - fReduceAmount);

	//have we gone as far as we can go?
	bMaxShrinkage = fReduceAmount >= m_fConstraintDistanceWhenRealConversionFailed;

	AI_LOG_WITH_ARGS("VEHICLE DUMMY: %s Shrinking constraint limits by %.3f. %.3f -> %.3f. At max: %s\n", AILogging::GetEntityLocalityNameAndPointer(this).c_str(), fReduceAmount, 
						m_fConstraintDistanceWhenRealConversionFailed, m_fConstraintDistanceWhenRealConversionFailed - fReduceAmount, bMaxShrinkage ? "TRUE" : "FALSE");

	return fMaxLength;
}

void CAutomobile::ProcessTimeStartedFailingRealConversion()
{
	//if this is unset, register that we failed here.
	//if it's set, we want to keep the older time, so we 
	//can shorten the constraints the appropriate amount
	if (m_TimeStartedFailingRealConversion == 0)
	{
		SetStartedFailingRealConversion();
	}
}

void CAutomobile::SetStartedFailingRealConversion()
{
	//set the time
	m_TimeStartedFailingRealConversion = fwTimer::GetTimeInMilliseconds();

	//now measure the distance from our constraint and save that off
	phConstraintDistance * pConstraint = (phConstraintDistance*) CPhysics::GetConstraintManager()->GetTemporaryPointer(m_dummyStayOnRoadConstraintHandle);
	if(pConstraint)
	{
		const Vec3V& posA = pConstraint->GetWorldPosA();	//the vehicle
		const Vec3V& posB = pConstraint->GetWorldPosB();	//the road

		const ScalarV sfDistance = Dist(posA, posB);

		m_fConstraintDistanceWhenRealConversionFailed = sfDistance.Getf();
	}
	else
	{
		m_fConstraintDistanceWhenRealConversionFailed = -1.0f;
	}
}

void CAutomobile::UpdateDummyConstraints(const Vec3V * pDistanceConstraintPos, const float * pDistanceConstraintMaxLength)
{
	if (IsNetworkClone())
	{
		ChangeDummyConstraints(VDM_REAL, false);
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	//--------------------------------------------------------
	// If using physical distance constraint, update it here

	if(CVehicleAILodManager::ms_bDisableConstraintsForSuperDummyInactive && GetVehicleAiLod().GetDummyMode()==VDM_SUPERDUMMY && GetIsStatic())
	{
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	const bool bDummyParentIsTrailer = m_nVehicleFlags.bHasParentVehicle && GetDummyAttachmentParent() && GetDummyAttachmentParent()->InheritsFromTrailer();
	const bool bRealParentIsTrailer =  m_nVehicleFlags.bHasParentVehicle && CVehicle::IsEntityAttachedToTrailer(this);
	if (bDummyParentIsTrailer || bRealParentIsTrailer)
	{
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	if(InheritsFromTrailer())
	{
		// Don't constrain trailers to the road.  They don't have their own path, and if the parent's path is used it 
		// might be improperly constrained.  Trailers are attached to the parent vehicle and that should be enough.
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	eVehicleDummyMode dummyMode = GetVehicleAiLod().GetDummyMode();
	const bool bUseDistanceConstraint =
		(dummyMode==VDM_DUMMY &&
			(CVehicleAILodManager::GetDummyConstraintMode() == CVehicleAILodManager::DCM_PhysicalDistance ||
				CVehicleAILodManager::GetDummyConstraintMode() == CVehicleAILodManager::DCM_PhysicalDistanceAndRotation))
		|| (dummyMode==VDM_SUPERDUMMY &&
			(CVehicleAILodManager::GetSuperDummyConstraintMode() == CVehicleAILodManager::DCM_PhysicalDistance ||
				CVehicleAILodManager::GetSuperDummyConstraintMode() == CVehicleAILodManager::DCM_PhysicalDistanceAndRotation) );

	if(!bUseDistanceConstraint)
	{
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	// If we don't have a dummy constraint, then try to create it.
	if(!m_dummyStayOnRoadConstraintHandle.IsValid())
	{
		ChangeDummyConstraints(dummyMode,GetIsStatic());
		if(!m_dummyStayOnRoadConstraintHandle.IsValid())
		{
			// TODO: We may need to handle this situation differently.  Find out when this is happening and make sure that
			// it is being handled gracefully.  E.g. If just dead cars, then they should fall physically or artificially.
			return;
		}
	}

	// Constraint is valid and desired to be used.

	Vec3V vConstraintPos;
	float constraintMaxLength;

	bool validConstraintFound = (pDistanceConstraintPos && pDistanceConstraintPos);

	if(!validConstraintFound)
	{
		const CVehicle* pVehicleConstraintsToUse = this;
		if (InheritsFromTrailer() && m_nVehicleFlags.bHasParentVehicle)
		{
			CPhysical* pAttachParent = (CPhysical *) GetAttachParent();	
			if (pAttachParent && pAttachParent->GetIsTypeVehicle())
			{
				pVehicleConstraintsToUse = (CVehicle*)pAttachParent;
			}
			else if (GetDummyAttachmentParent())
			{
				pVehicleConstraintsToUse = GetDummyAttachmentParent();
			}
		}

		validConstraintFound = CVirtualRoad::CalculatePathInfo(*pVehicleConstraintsToUse,false,false,vConstraintPos,constraintMaxLength, false);
	}
	else
	{
		vConstraintPos = *pDistanceConstraintPos;
		constraintMaxLength = *pDistanceConstraintMaxLength;
	}

	if(validConstraintFound)
	{
		phConstraintDistance * pConstraint = (phConstraintDistance*) CPhysics::GetConstraintManager()->GetTemporaryPointer(m_dummyStayOnRoadConstraintHandle);
		if(pConstraint)
		{
			const Vec3V vVehPos = GetTransform().GetPosition();
			pConstraint->SetWorldPosA(vVehPos);
			pConstraint->SetWorldPosB(vConstraintPos);
#if __BANK
			ValidateDummyConstraints(vVehPos,vConstraintPos,constraintMaxLength);
			// If validating constraints, add a buffer to the actual physical constraint to allow the vehicle to exist outside of the bounds while the situation is debugged.
			constraintMaxLength += (CVehicleAILodManager::ms_bValidateDummyConstraints) ? 10.0f : 0.0f;
#endif

			//if we're reducing the constraint due to failed conversions, do so here
			bool bMaxShrinkage = false;
			const float fMaxLengthAfterFailedDummyConvert = GetMaxLengthForDummyConstraintsAfterFailedConversion(bMaxShrinkage);
			if (fMaxLengthAfterFailedDummyConvert >= 0.0f)
			{
				constraintMaxLength = rage::Min(constraintMaxLength, fMaxLengthAfterFailedDummyConvert);

				//ensure we don't set a distance that'll make us assert straight away
				float distToConstraint = Dist(vVehPos, vConstraintPos).Getf();
				float minAllowableLength = distToConstraint - PH_CONSTRAINT_ASSERT_DEPTH + 0.25f;
				constraintMaxLength = rage::Max(minAllowableLength, constraintMaxLength);
			}

			pConstraint->SetMaxDistance(constraintMaxLength);
		}
	}
}

bool CAutomobile::RemoveConstraints()
{
	bool bRemovedConstraint = false;
	// Clear any stay-on-road constraint.
	if(m_dummyStayOnRoadConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_dummyStayOnRoadConstraintHandle);
		m_dummyStayOnRoadConstraintHandle.Reset();
		bRemovedConstraint = true;
	}
	// Clear any rotational constraint.
	if(m_dummyRotationalConstraint.IsValid())
	{
		PHCONSTRAINT->Remove(m_dummyRotationalConstraint);
		m_dummyRotationalConstraint.Reset();
		bRemovedConstraint = true;
	}

	ResetRealConversionFailedData();
	m_nVehicleFlags.bWasOffroadWithConstraint = false;

	return bRemovedConstraint;
}

#if __BANK

Vector3 vDummyWheelForces[NUM_VEH_CWHEELS_MAX];
Vector3 vDummyWheelForcesInPlane[NUM_VEH_CWHEELS_MAX];
Vector3 vDummyForce;
Vector3 vDummyForceInPlane;
Vector3 vDummyAngVel;

void CAutomobile::RenderDebug() const
{
	CVehicle::RenderDebug();

	// Health Info for player car
    if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_DAMAGE && (GetStatus()==STATUS_PLAYER || this == CDebugScene::FocusEntities_Get(0)) ) 
	{
		char vehDebugString[1024];

		grcDebugDraw::AddDebugOutput("");

		sprintf(vehDebugString,"Overall      %.2f", m_VehicleDamage.GetOverallHealth());
		grcDebugDraw::AddDebugOutput(vehDebugString);

		sprintf(vehDebugString,"Body         %.2f", m_VehicleDamage.GetBodyHealth());
		grcDebugDraw::AddDebugOutput(vehDebugString);

		sprintf(vehDebugString,"Engine       %.2f", m_VehicleDamage.GetEngineHealth());
		grcDebugDraw::AddDebugOutput(vehDebugString);

        sprintf(vehDebugString,"Oil Level  %.2f", m_VehicleDamage.GetOilLevel());
		grcDebugDraw::AddDebugOutput(vehDebugString);

		sprintf(vehDebugString,"PetrolTank   %.2f", m_VehicleDamage.GetPetrolTankHealth());
		grcDebugDraw::AddDebugOutput(vehDebugString);

        sprintf(vehDebugString,"PetrolTank Level  %.2f", m_VehicleDamage.GetPetrolTankLevel());
		grcDebugDraw::AddDebugOutput(vehDebugString);

        if(m_Transmission.GetCurrentlyMissFiring())
        {
            sprintf(vehDebugString,"MISS FIRE!!");
			grcDebugDraw::AddDebugOutput(vehDebugString);
        }
        else if(m_Transmission.GetCurrentlyRecoveringFromMissFiring())
        {
            sprintf(vehDebugString,"RECOVERING MISS FIRE!!");
			grcDebugDraw::AddDebugOutput(vehDebugString);
        }

		for(int nWheel=0; nWheel < GetNumWheels(); nWheel++)
		{
			if(GetWheel(nWheel))
			{
                sprintf(vehDebugString,"Wheel %d  %.2f, %.2f, temp: %.2f, tyre drag: %.2f", nWheel, GetWheel(nWheel)->GetSuspensionHealth(), GetWheel(nWheel)->GetFrictionDamage(), GetWheel(nWheel)->GetTyreTemp(), GetWheel(nWheel)->GetTyreDrag());
				grcDebugDraw::AddDebugOutput(vehDebugString);
			}
		}
	}

	// draw a speedo for player car
	if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
		&& ((GetDriver() && GetDriver()->IsLocalPlayer()) || this == CDebugScene::FocusEntities_Get(0)))
	{
		char vehDebugString[1024];
		float fGroundClearance = 10.0f;

		Vector3 vertPos;
		phBoundComposite* pBoundComp = GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds();
		for(int nBound=0; nBound < pBoundComp->GetNumBounds(); nBound++)
		{
			if(pBoundComp->GetBound(nBound) && pBoundComp->GetBound(nBound)->GetType()==phBound::GEOMETRY)
			{
				phBoundGeometry* pBoundGeom = static_cast<phBoundGeometry*>(pBoundComp->GetBound(nBound));
				const Matrix34& boundMatrix = RCC_MATRIX34(pBoundComp->GetCurrentMatrix(nBound));

				for(int nVert=0; nVert < pBoundGeom->GetNumVertices(); nVert++)
				{
					// want to deform the shrunk geometry vertices, ignore the preshrunk ones since we want shape tests to hit the original bike
					if(pBoundGeom->GetShrunkVertexPointer())
					{
						vertPos.Set(VEC3V_TO_VECTOR3(pBoundGeom->GetShrunkVertex(nVert)));
						vertPos += boundMatrix.d;
						if(vertPos.z - pBoundGeom->GetMargin() < fGroundClearance)
							fGroundClearance = vertPos.z - pBoundGeom->GetMargin();
					}
				}
			}
		}
		fGroundClearance = fGroundClearance + GetHeightAboveRoad();
		//float fGroundClearance = GetHeightAboveRoad() + GetVehicleFragInst()->GetType()->GetCompositeBounds()->GetBound(0)->GetBoundingBoxMin().z;

		sprintf(vehDebugString, "Mass=%g Inertia=%1.1f, %1.1f, %1.1f GroundClearance(%1.3f)", GetMass(), GetAngInertia().GetX(), GetAngInertia().GetY(), GetAngInertia().GetZ(), fGroundClearance);
		grcDebugDraw::AddDebugOutput(vehDebugString);

		if(GetNumWheels() >= 3)
		{
			float fSuspensionWidth = GetWheel(2)->GetProbeSegment().A.x - GetWheel(0)->GetProbeSegment().A.x + GetWheel(0)->GetWheelWidth();
			float fBaseSFF = 0.5f*fSuspensionWidth / GetHeightAboveRoad();
			float fHandlingSFF = 0.5f*fSuspensionWidth / (GetHeightAboveRoad() + pHandling->m_vecCentreOfMassOffset.GetZf());

			sprintf(vehDebugString, "SFF: Model(%1.3f) With COG Moved(%1.3f)", fBaseSFF, fHandlingSFF);
			grcDebugDraw::AddDebugOutput(vehDebugString);

			float fBaseWeightDistribution = 100.0f*GetWheel(0)->GetProbeSegment().A.y / (GetWheel(0)->GetProbeSegment().A.y - GetWheel(1)->GetProbeSegment().A.y);
			float fHandlingWeightDistribution = 100.0f*(GetWheel(0)->GetProbeSegment().A.y - pHandling->m_vecCentreOfMassOffset.GetYf()) / (GetWheel(0)->GetProbeSegment().A.y - GetWheel(1)->GetProbeSegment().A.y);
			sprintf(vehDebugString, "Weight Balance %2.1f:%2.1f With COG Moved(%2.1f:%2.1f)", (100.0f - fBaseWeightDistribution), fBaseWeightDistribution, (100.0f - fHandlingWeightDistribution), fHandlingWeightDistribution);
			grcDebugDraw::AddDebugOutput(vehDebugString);

			float fMinSusLength = 0.25f / pHandling->m_fSuspensionForce;
			sprintf(vehDebugString, "Min sus extension %1.3f (%1.3f)", -fMinSusLength, pHandling->m_fSuspensionLowerLimit);
			grcDebugDraw::AddDebugOutput(vehDebugString);

            sprintf(vehDebugString, "Current sus extension F=%1.3f, R=%1.3f", GetWheel(0)->GetCompression(), GetWheel(2)->GetCompression());
            grcDebugDraw::AddDebugOutput(vehDebugString);
		}

        sprintf(vehDebugString, "Cheat power Increase=%1.3f", 1.0f - GetCheatPowerIncrease());
        grcDebugDraw::AddDebugOutput(vehDebugString);

        sprintf(vehDebugString, "MODS: Engine power Increase=%1.3f", (1.0f + (CVehicle::ms_fEngineVarianceMaxModifier * (GetVariationInstance().GetEngineModifier()/100.0f))) );
        grcDebugDraw::AddDebugOutput(vehDebugString);
        sprintf(vehDebugString, "MODS: Brake Force Increase=%1.3f", ((ms_fBrakeVarianceMaxModifier * GetVariationInstance().GetBrakesModifier()/100.0f)));
        grcDebugDraw::AddDebugOutput(vehDebugString);
        sprintf(vehDebugString, "MODS: Armour damage multiplier=%1.3f", GetVariationInstance().GetArmourDamageMultiplier());
        grcDebugDraw::AddDebugOutput(vehDebugString);
		// NOTE: need to const_cast here because it uses GetCarHandlingData() which doesn't have a const variant, so easiest thing to do is just const_cast for this function even though GetTurboPowerModifier doesn't modify any state.
        sprintf(vehDebugString,"MODS: Turbo power Increase=%1.3f",  GetVariationInstance().IsToggleModOn(VMT_TURBO) ? CTransmission::GetTurboPowerModifier(const_cast<CAutomobile*>(this)) : 0.0f);
        grcDebugDraw::AddDebugOutput(vehDebugString);
        sprintf(vehDebugString,"MODS: Gearbox Increase=%1.3f",  GetVariationInstance().GetGearboxModifier()/100.0f);
        grcDebugDraw::AddDebugOutput(vehDebugString);
		sprintf(vehDebugString,"KERS=%1.3f",  m_Transmission.GetKERSRemaining());
		grcDebugDraw::AddDebugOutput(vehDebugString);
		sprintf( vehDebugString, "WheelieMode=%d", (int)m_nAutomobileFlags.bInWheelieMode );
		grcDebugDraw::AddDebugOutput( vehDebugString );

        
		phArchetypeDamp* pCurrentArchetype = (phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype();

		formatf(vehDebugString,512, "LINEAR_C  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).x,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).y,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).z);
		grcDebugDraw::AddDebugOutput(vehDebugString);

		formatf(vehDebugString,512, "LINEAR_V  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).x,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).y,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).z);
		grcDebugDraw::AddDebugOutput(vehDebugString);

		if(GetDriver() && GetDriver()->IsPlayer())
		{
			float fScriptedDragMult = 0.0f;
			CPlayerInfo *pPlayer = GetDriver()->GetPlayerInfo();
			if (pPlayer && pPlayer->m_fForceAirDragMult > 0.0f)
			{
				fScriptedDragMult = pPlayer->m_fForceAirDragMult;
			}
			bool bIsActive = pPlayer->m_fForceAirDragMult > 0.0f && pPlayer->m_fForceAirDragMult != 1.0f;

			formatf(vehDebugString,512, "LINEAR_V2  %2.5f, %2.5f, %2.5f (DragCoeff %2.5f, ScriptDragMult %2.5f, ScriptedDrag %s SlipstreamT: %f SlipstreamAbsT %f)", 
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).x,
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).y,
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).z,
				m_fDragCoeff, fScriptedDragMult,
				bIsActive ? "YES" : "NO",
				m_fTimeInSlipStream,
				m_fSlipStreamRechargeAndDechargeTimer);
		}
		else
		{
			formatf(vehDebugString,512, "LINEAR_V2  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).x,
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).y,
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).z);
		}
		grcDebugDraw::AddDebugOutput(vehDebugString);

		formatf(vehDebugString,512, "ANGULAR_C  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_C).x,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_C).y,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_C).z);
		grcDebugDraw::AddDebugOutput(vehDebugString);

		formatf(vehDebugString,512, "ANGULAR_V  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V).x,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V).y,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V).z);
		grcDebugDraw::AddDebugOutput(vehDebugString);

		formatf(vehDebugString,512, "ANGULAR_V2  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V2).x,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V2).y,
			pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V2).z);
		grcDebugDraw::AddDebugOutput(vehDebugString);

		static u16 TEST_STUCK_CHECK = 10000;
		if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_JAMMED, TEST_STUCK_CHECK))
        {
			grcDebugDraw::AddDebugOutput("Stuck - Jammed");
        }
        else
        {
            sprintf(vehDebugString,"Stuck - Jammed Timer %1.2f", GetVehicleDamage()->GetStuckTimer(VEH_STUCK_JAMMED));
            grcDebugDraw::AddDebugOutput(vehDebugString);
        }

		if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_SIDE, TEST_STUCK_CHECK))
        {
			grcDebugDraw::AddDebugOutput("Stuck - OnSide");
        }
        else
        {
            sprintf(vehDebugString,"Stuck - OnSide Timer %1.2f", GetVehicleDamage()->GetStuckTimer(VEH_STUCK_ON_SIDE));
            grcDebugDraw::AddDebugOutput(vehDebugString);
        }

		if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_ROOF, TEST_STUCK_CHECK))
        {
			grcDebugDraw::AddDebugOutput("Stuck - OnRoof");
        }
        else
        {
            sprintf(vehDebugString,"Stuck - OnRoof Timer %1.2f", GetVehicleDamage()->GetStuckTimer(VEH_STUCK_ON_ROOF));
            grcDebugDraw::AddDebugOutput(vehDebugString);
        }

		if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_HUNG_UP, TEST_STUCK_CHECK))
        {
			grcDebugDraw::AddDebugOutput("Stuck - HungUp");
        }
        else
        {
            int bDriveWheelOnGround = 0;
            for(int i=0; i<GetNumWheels(); i++)
            {
                if(GetWheel(i)->GetConfigFlags().IsFlagSet(WCF_POWERED) && (GetWheel(i)->GetIsTouching() || GetWheel(i)->GetWasTouching()) )
                    bDriveWheelOnGround++;
            }

            sprintf(vehDebugString,"Stuck - HungUp Timer %1.2f Wheels on ground %d", GetVehicleDamage()->GetStuckTimer(VEH_STUCK_HUNG_UP), bDriveWheelOnGround);
            grcDebugDraw::AddDebugOutput(vehDebugString);
        }

		grcDebugDraw::AddDebugOutput("");

		// Draw handbrake
		if(GetHandBrake())
		{
			static float x = 0.75f;
			static float y = 0.75f;
			static float fLength = 0.2f;

			float fHandbrakeForce = GetHandBrakeForce() / pHandling->m_fHandBrakeForce;
			char strTitle[32];
			formatf(strTitle, "Handbrake: %.2f", fHandbrakeForce);

			grcDebugDraw::Meter(Vector2(x,y), Vector2(0.0f, -1.0f), fLength, 0.05f * fLength, Color_red, strTitle);
			grcDebugDraw::MeterValue(Vector2(x,y), Vector2(0.0f, -1.0f), fLength, fHandbrakeForce, 0.05f * fLength, Color_white, true);
		}

		// Draw handbrake grip fall off
		if(GetHandBrakeGripMult() != 1.0f)
		{
			static float x = 0.65f;
			static float y = 0.75f;
			static float fLength = 0.2f;

			float fHandbrakeForce = GetHandBrakeGripMult();
			char strTitle[32];
			formatf(strTitle, "Handbrake grip: %.2f", fHandbrakeForce);

			grcDebugDraw::Meter(Vector2(x,y), Vector2(0.0f, -1.0f), fLength, 0.05f * fLength, Color_red, strTitle);
			grcDebugDraw::MeterValue(Vector2(x,y), Vector2(0.0f, -1.0f), fLength, fHandbrakeForce, 0.05f * fLength, Color_white, true);
		}

		if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE) 
		{
			DrawGForce();
			DrawAngularAcceleration();
		}
	}

	bool displayDebugInfo = (!CVehicleIntelligence::ms_debugDisplayFocusVehOnly || CDebugScene::FocusEntities_IsInGroup(this));
	if(displayDebugInfo)
	{
		if(CVehicleAILodManager::ms_displayDummyVehicleMarkers)
		{
			if(IsDummy())
			{
				// Draw our position.
				const Vector3 pos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				Color32 col = m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy) ? Color_DarkOrchid1 : Color_SeaGreen1;
				grcDebugDraw::Line(pos, Vector3(pos.x, pos.y,pos.z + 4.0f), col);
			}
		}

#if __DEV
		if(CVehicleAILodManager::ms_displayDummyVehicleMarkers)
		{
			if(IsDummy())
			{
				const Vector3 pos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				grcDebugDraw::Sphere(Vector3(pos.x, pos.y,pos.z + 4.0f), 0.3f, Color_green, true, -1, 3);
			}
		}

		if(CVehiclePopulation::ms_displayVehicleDirAndPath)
		{
			// Draw our direction.
			const Vector3 pos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			const Vector3 dir = VEC3V_TO_VECTOR3(GetTransform().GetB());
			grcDebugDraw::Line(pos,(pos+(dir*4.0f)),Color32(0xff,0xc0,0xc0,0xff));

			CVehicleNodeList * pNodeList = GetIntelligence()->GetNodeList();
			if(pNodeList)
			{
				// Draw our current path.
				// Go through all of the path segments and render them.
				for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1; i++)
				{
					const CNodeAddress addr1 = pNodeList->GetPathNodeAddr(i);
					const CNodeAddress addr2 = pNodeList->GetPathNodeAddr(i+1);

					if(	!addr1.IsEmpty() && ThePaths.IsRegionLoaded(addr1) &&
						!addr2.IsEmpty() && ThePaths.IsRegionLoaded(addr2))
					{
						Vector3 node1Coors;
						const CPathNode* pNode1 = ThePaths.FindNodePointer(addr1);
						Assert(pNode1);
						pNode1->GetCoors(node1Coors); 

						static float radius = 0.5f;
						grcDebugDraw::Sphere(node1Coors, radius, Color32(1.0f,1.0f,0.0f,0.5f));

						Vector3 node2Coors;
						const CPathNode* pNode2 = ThePaths.FindNodePointer(addr2);
						Assert(pNode2);
						pNode2->GetCoors(node2Coors);

						static float radius2 = 0.25f;
						grcDebugDraw::Sphere(node2Coors, radius2, Color32(0.0f,1.0f,1.0f,1.0f));

						const float p1 = static_cast<float>(i) / static_cast<float>(CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
						const float p2 = static_cast<float>(i+1) / static_cast<float>(CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
						grcDebugDraw::Line(node1Coors, node2Coors, Color32(0.0f,1.0f-p1,p1,1.0f),Color32(0.0f,1.0f-p2,p2,1.0f));
					}
				}
			}
		}
#endif // __DEV

#if __BANK && DEBUG_DRAW
		if(CVehicleAILodManager::ms_displayDummyVehicleNonConvertReason)
		{
			if(!IsDummy())
			{
				// only want to do text for vehicles close to camera
				// Set origin to be the debug cam, or the player..
				//camDebugDirector& debugDirector = camInterface::GetDebugDirector();
				//Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());

				const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				Vector3 vDiff = vThisPosition - camInterface::GetPos();
				float fDist = vDiff.Mag();
				const float vehicleVisualizationRange = 60.0f;
				if(fDist < vehicleVisualizationRange)
				{
					Vector3 WorldCoors = vThisPosition + Vector3(0,0,1.0f);

					float fScale = 1.0f - (fDist / vehicleVisualizationRange);
					u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);
					Color32 colour = CRGBA(255,192,96,iAlpha);

					char vehDebugString[1024];
					sprintf(vehDebugString,"Nonconvert Reason: %s", GetNonConversionReason());
					grcDebugDraw::Text( WorldCoors, colour, vehDebugString);
				}
			}
		}
#endif // __BANK && DEBUG_DRAW

	}
}

void CAutomobile::DrawGForce() const
{
#if __DEV
	if(GetCollider() && ((GetDriver() && GetDriver()->IsLocalPlayer()) || this == CDebugScene::FocusEntities_Get(0))) // This will only work for one vehicle at a time as I'm using a static.
	{	
		static Vec3V vPreviousVelcity; // Static to hold previous velocity as we are in a const function

		Color32 drawColour(1.0f, 1.0f, 0.0f, 1.0f);
		Color32 drawGreen(0.0f, 1.0f, 0.0f, 0.5f);
		Color32 drawRed(255,0,0,255);

		static const float fScreenWidth = 1.0f;
		static const float fScreenHeight = 1.0f;

		float fLineWidthMult = 0.04f;
		float fLineHeightMult = 0.04f;

		float fIndicatorWidthMult = 0.01f;
		float fIndicatorHeightMult = 0.01f;

		Vector2 vecStart(0.35f, 0.8f);
		vecStart.x *= fScreenWidth;
		vecStart.y *= fScreenHeight;

		const int iExpiryTime = (fwTimer::GetTimeStepInMilliseconds() * 3) / 2;

		Vec3V vCurrentVelocity = GetCollider()->GetVelocity();
		Vec3V vAcceleration = InvScale(Subtract(vCurrentVelocity, vPreviousVelcity), ScalarV(fwTimer::GetTimeStep()));

		vPreviousVelcity = vCurrentVelocity;

		Vec3V vGForce = InvScale(vAcceleration, ScalarV(-GRAVITY)); // hard coded gravity as we are interested in seeing gforce with regards to Earth.

		vGForce = UnTransform3x3Full(GetCollider()->GetMatrix(), vGForce);

		float fEndPosX = fLineHeightMult * fScreenWidth;
		float fPeakPosX = fLineHeightMult * fScreenWidth;
		float fEndPosY = fLineWidthMult * fScreenHeight;
		float fPeakPosY = fLineWidthMult * fScreenHeight;

		static float sfGforceScale = 0.5f;

		float fCurrentPosX = vGForce.GetXf() * fLineHeightMult * fScreenHeight * sfGforceScale;
		fCurrentPosX = Clamp(fCurrentPosX, -fEndPosX*2.0f, fEndPosX*2.0f);


		float fCurrentPosY = vGForce.GetYf() * fLineWidthMult * fScreenHeight;
		fCurrentPosY = Clamp(fCurrentPosY, -fEndPosY*2.0f, fEndPosY*2.0f);


		// Draw Vertical lines
		{
			float fBoxHeight = fLineHeightMult * fScreenHeight;
			float fMarkerWidth = fIndicatorHeightMult * fScreenHeight;

			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fPeakPosX * 0.5f, vecStart.y - fBoxHeight * 0.5f),Vector2( vecStart.x + fPeakPosX * 0.5f, vecStart.y + fBoxHeight * 0.5f), drawGreen,iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fPeakPosX * 0.5f, vecStart.y - fBoxHeight * 0.5f),Vector2( vecStart.x - fPeakPosX * 0.5f, vecStart.y + fBoxHeight * 0.5f), drawGreen,iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fPeakPosX, vecStart.y - fBoxHeight),Vector2( vecStart.x + fPeakPosX, vecStart.y + fBoxHeight), drawColour,iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fPeakPosX, vecStart.y - fBoxHeight),Vector2( vecStart.x - fPeakPosX, vecStart.y + fBoxHeight), drawColour,iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fCurrentPosX, vecStart.y + fCurrentPosY - fMarkerWidth),Vector2( vecStart.x + fCurrentPosX, vecStart.y + fCurrentPosY + fMarkerWidth), drawRed,iExpiryTime);
		}

		// Draw Horizontal lines
		{
			float fBoxWidth = fLineWidthMult * fScreenWidth;
			float fMarkerWidth = fIndicatorWidthMult * fScreenWidth;

			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth * 0.5f, vecStart.y + fPeakPosY * 0.5f),Vector2( vecStart.x + fBoxWidth * 0.5f, vecStart.y + fPeakPosY * 0.5f), drawGreen,iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth * 0.5f, vecStart.y - fPeakPosY * 0.5f),Vector2( vecStart.x + fBoxWidth * 0.5f, vecStart.y - fPeakPosY * 0.5f), drawGreen,iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth, vecStart.y + fPeakPosY),Vector2( vecStart.x + fBoxWidth, vecStart.y + fPeakPosY), drawColour, iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth, vecStart.y - fPeakPosY),Vector2( vecStart.x + fBoxWidth, vecStart.y - fPeakPosY), drawColour, iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fCurrentPosX - fMarkerWidth, vecStart.y + fCurrentPosY),Vector2( vecStart.x + fCurrentPosX + fMarkerWidth, vecStart.y + fCurrentPosY), drawRed,iExpiryTime);
		}


		// Turn Radius
		Vec3V vCurrentAngVelocity = GetCollider()->GetAngVelocity();
		Vec3V vRadiusVel = vCurrentVelocity / vCurrentAngVelocity;

		if(ABS(vRadiusVel.GetZf()) > 2.0f*PI) // clamp the radius.
		{
			vRadiusVel.SetZf(0.0f);
		}

		char vehDebugString[1024];

		sprintf(vehDebugString,"Turn Radius  %.2f  Degrees  %.2f", vRadiusVel.GetZf(), RADIANS_TO_DEGREES(vRadiusVel.GetZf()));
		grcDebugDraw::AddDebugOutput(vehDebugString);

		float fGforceMag = Mag(vGForce).Getf();
		if(fGforceMag > CAutomobile::ms_fHighestGForce)
		{
			CAutomobile::ms_fHighestGForce = fGforceMag;
		}

		sprintf(vehDebugString,"G force Current %.2f  Max %.2f", fGforceMag, CAutomobile::ms_fHighestGForce);
		grcDebugDraw::AddDebugOutput(vehDebugString);
	}
#endif
}

void CAutomobile::DrawAngularAcceleration() const
{
#if __DEV
	if(GetCollider() && ((GetDriver() && GetDriver()->IsLocalPlayer()) || this == CDebugScene::FocusEntities_Get(0))) // This will only work for one vehicle at a time as I'm using a static.
	{	
		static Vec3V vPreviousAngVelcity; // Static to hold previous velocity as we are in a const function

		Color32 drawColour(1.0f, 1.0f, 0.0f, 1.0f);
		Color32 drawGreen(0.0f, 1.0f, 0.0f, 0.5f);
		Color32 drawRed(255,0,0,255);

		static const float fScreenWidth = 1.0f;
		static const float fScreenHeight = 1.0f;

		float fLineWidthMult = 0.04f;
		float fLineHeightMult = 0.04f;

		float fIndicatorWidthMult = 0.01f;
		float fIndicatorHeightMult = 0.01f;

		Vector2 vecStart(0.75f, 0.8f);
		vecStart.x *= fScreenWidth;
		vecStart.y *= fScreenHeight;

		const int iExpiryTime = (fwTimer::GetTimeStepInMilliseconds() * 3) / 2;

		Vec3V vCurrentAngVelocity = GetCollider()->GetAngVelocity();
		Vec3V vAngAcceleration = vCurrentAngVelocity;//InvScale(Subtract(vCurrentAngVelocity, vPreviousAngVelcity), ScalarV(fwTimer::GetTimeStep()));

		vPreviousAngVelcity = vCurrentAngVelocity;


		vAngAcceleration = UnTransform3x3Full(GetCollider()->GetMatrix(), vAngAcceleration);

		float fEndPosX = fLineHeightMult * fScreenWidth;
		float fPeakPosX = fLineHeightMult * fScreenWidth;
		float fEndPosY = fLineWidthMult * fScreenHeight;
		float fPeakPosY = fLineWidthMult * fScreenHeight;

		static float sfGforceScale = 0.5f;

		float fCurrentPosX = vAngAcceleration.GetYf() * fLineHeightMult * fScreenHeight * sfGforceScale;
		fCurrentPosX = Clamp(fCurrentPosX, -fEndPosX*2.0f, fEndPosX*2.0f);


		float fCurrentPosY = vAngAcceleration.GetXf() * fLineWidthMult * fScreenHeight;
		fCurrentPosY = Clamp(fCurrentPosY, -fEndPosY*2.0f, fEndPosY*2.0f);


		// Draw Vertical anglines
		{
			float fBoxHeight = fLineHeightMult * fScreenHeight;
			float fMarkerWidth = fIndicatorHeightMult * fScreenHeight;

			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fPeakPosX * 0.5f, vecStart.y - fBoxHeight * 0.5f),Vector2( vecStart.x + fPeakPosX * 0.5f, vecStart.y + fBoxHeight * 0.5f), drawGreen,iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fPeakPosX * 0.5f, vecStart.y - fBoxHeight * 0.5f),Vector2( vecStart.x - fPeakPosX * 0.5f, vecStart.y + fBoxHeight * 0.5f), drawGreen,iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fPeakPosX, vecStart.y - fBoxHeight),Vector2( vecStart.x + fPeakPosX, vecStart.y + fBoxHeight), drawColour,iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fPeakPosX, vecStart.y - fBoxHeight),Vector2( vecStart.x - fPeakPosX, vecStart.y + fBoxHeight), drawColour,iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fCurrentPosX, vecStart.y + fCurrentPosY - fMarkerWidth),Vector2( vecStart.x + fCurrentPosX, vecStart.y + fCurrentPosY + fMarkerWidth), drawRed,iExpiryTime);
		}

		// Draw Horizontal lines
		{
			float fBoxWidth = fLineWidthMult * fScreenWidth;
			float fMarkerWidth = fIndicatorWidthMult * fScreenWidth;

			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth * 0.5f, vecStart.y + fPeakPosY * 0.5f),Vector2( vecStart.x + fBoxWidth * 0.5f, vecStart.y + fPeakPosY * 0.5f), drawGreen,iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth * 0.5f, vecStart.y - fPeakPosY * 0.5f),Vector2( vecStart.x + fBoxWidth * 0.5f, vecStart.y - fPeakPosY * 0.5f), drawGreen,iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth, vecStart.y + fPeakPosY),Vector2( vecStart.x + fBoxWidth, vecStart.y + fPeakPosY), drawColour, iExpiryTime);
			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fBoxWidth, vecStart.y - fPeakPosY),Vector2( vecStart.x + fBoxWidth, vecStart.y - fPeakPosY), drawColour, iExpiryTime);


			CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fCurrentPosX - fMarkerWidth, vecStart.y + fCurrentPosY),Vector2( vecStart.x + fCurrentPosX + fMarkerWidth, vecStart.y + fCurrentPosY), drawRed,iExpiryTime);
		}


		char vehDebugString[1024];

		sprintf(vehDebugString,"Ang Acceleration Current %.2f", vAngAcceleration.GetXf());
		grcDebugDraw::AddDebugOutput(vehDebugString);
	}
#endif
}


u32 CAutomobile::GetAsyncQueueCount()
{
	u32 count = 0;
	for (s32 i = 0; i < ms_placeOnRoadArray.GetCount(); ++i)
		if (ms_placeOnRoadArray[i].id != INVALID_ASYNC_ENTRY)
			count++;

	return count;
}
#endif // __BANK

RAGETRACE_DECL(CAutomobile_ProcessPhysics);
RAGETRACE_DECL(CAuto_Phys_Transmission);
RAGETRACE_DECL(CAuto_Phys_Doors);
RAGETRACE_DECL(CAuto_Phys_Bouyancy);
RAGETRACE_DECL(CAuto_Phys_Wheels);
RAGETRACE_DECL(CAuto_Phys_FlyingCar);
RAGETRACE_DECL(CAuto_Phys_DummyConstraints);

#define TRACK_VEHICLE_GRADIENT (0)

#if TRACK_VEHICLE_GRADIENT
PF_PAGE(vehicleGradientPage, "Vehicle gradient group");
PF_GROUP(vehicleGradientGroup);
PF_LINK(vehicleGradientPage, vehicleGradientGroup);

PF_VALUE_FLOAT(vehicleGradientDeg,vehicleGradientGroup);
#endif

dev_float sfPlayerSpeedSteerMult = 0.075f;
dev_float sfPlayerSpeedSteerFwdThreshold = 5.0f;
dev_float sfPlayerSpeedSteerSideRatioThreshold = 0.1f;
dev_bool sbPlayerSpeedSteerAutoCentre = true;
dev_float sfPlayerSpeedSteerAutoCentreMax = ( DtoR * 15.0f);

void CAutomobile::Fix(bool resetFrag, bool allowNetwork)
{		
    CVehicle::Fix(resetFrag, allowNetwork);

    //reset the ground clearance, as the bounds will have been reset by the damage code
    m_fGroundClearance = 0.0f;
	m_fLastGroundClearance = 0.0f;
	m_fLastGroundClearanceHeightAboveGround = 0.0f;
	m_uLastGroundClearanceCheck = 0;
	m_nAutomobileFlags.bLastSpaceAvaliableToChangeBounds = true;

	RefreshAirResistance();	// Not sure if needed.

	// Loop through all the attached stick bombs and update their positions
	if(fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		const void *basePtr = NULL;
		if(GetVehicleDamage() && GetVehicleDamage()->GetDeformation() && GetVehicleDamage()->GetDeformation()->HasDamageTexture())
		{
			basePtr = GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead); //Lock the texture once for all wheels
		}
		if(basePtr)
		{

			CPhysical* pCurChild = static_cast<CPhysical*>(extension->GetChildAttachment());
			while(pCurChild)
			{
				if(pCurChild->GetIsTypeObject())
				{
					if(CProjectile *pProjectile = ((CObject *)pCurChild)->GetAsProjectile())
					{
						pProjectile->ApplyDeformation(this, basePtr);
					}
				}
				fwAttachmentEntityExtension &curChildAttachExt = pCurChild->GetAttachmentExtensionRef();
				pCurChild = static_cast<CPhysical*>(curChildAttachExt.GetSiblingAttachment());
			}

			GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
		}
	}
}

static u32 suGroundClearanceInterval = 500;
// Work out whether we should move the ground clearance
// returns true if bounds should be modified
bool CAutomobile::UpdateDesiredGroundGroundClearance(float fGroundClearance, bool brokenWheels, bool force)
{
	bool updateTrailerGroundClearance = ( InheritsFromTrailer() &&
	 									  ( ( MI_TRAILER_TRAILERLARGE.IsValid() && GetModelIndex() == MI_TRAILER_TRAILERLARGE ) ||
										    ( MI_TRAILER_TRAILERSMALL2.IsValid() && GetModelIndex() == MI_TRAILER_TRAILERSMALL2 ) ) );

    if( (GetVehicleType()==VEHICLE_TYPE_CAR || GetVehicleType()==VEHICLE_TYPE_QUADBIKE || InheritsFromAmphibiousAutomobile() || GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR || updateTrailerGroundClearance ) && 
        !(pHandling->hFlags & HF_DONT_RAISE_BOUNDS_AT_SPEED || 
          (pHandling->mFlags & MF_DONT_FORCE_GRND_CLEARANCE && !HasHydraulicSuspension())) )//some vehicles explicitly state they don't want their ground clearance enforced 
    {
        if((GetDriver() && (GetDriver()->IsPlayer() || HasHydraulicSuspension())) || m_nVehicleFlags.bAllowBoundsToBeRaised || m_nVehicleFlags.bTowedVehBoundsCanBeRaised || brokenWheels || updateTrailerGroundClearance) // Only modify the bounds of the players vehicle.
        {
			float fHeightAboveRoad = m_fHeightAboveRoad;
			bool bAdjustBounds = false;

			//If we have hydraulic suspension we only want to raise the suspension when the vehicle is slammed on the ground.
			if( HasHydraulicSuspension() )
			{
				// Check whether the hydraulics have moved or the ground clearance has been changed
				if(m_uLastGroundClearanceCheck < m_iTimeHydraulicsModified || fGroundClearance != m_fGroundClearance)
				{
					float fTempHeight;
					CVehicle::CalculateHeightsAboveRoad(GetModelId(), &fHeightAboveRoad, &fTempHeight);

					float fSuspensionRaise = 0.0f;
					for (int i=0; i<GetNumWheels(); i++)
					{
						float fSuspensionRaiseForWheel = GetWheel(i)->GetSuspensionRaiseAmount();
						if(fSuspensionRaiseForWheel > fSuspensionRaise)
						{
							fSuspensionRaise = fSuspensionRaiseForWheel;
						}
					}

					fHeightAboveRoad += fSuspensionRaise;
				
					TUNE_FLOAT(sfHeightAboveRoadChangeForceUpdate, 0.05f, 0.0f, 5.0f, 0.001f)
					// If the ground clearance has changed significantly force the ground clearance to be checked.
					if(fabs(fHeightAboveRoad - m_fLastGroundClearanceHeightAboveGround) > sfHeightAboveRoadChangeForceUpdate )
					{
						m_fHeightAboveRoad = fHeightAboveRoad;
						bAdjustBounds = true;
					}
					else if(fGroundClearance == m_fGroundClearance)	// Don't need to update the bounds 
					{
						m_uLastGroundClearanceCheck = m_iTimeHydraulicsModified;	// Reset the ground check timer
					}
				}
			}

            if(fGroundClearance != m_fGroundClearance || force || bAdjustBounds )
            { 
			    if( GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType() != phBound::COMPOSITE )
			    {
				    return false;
				}

                const phBoundComposite* pBoundCompOrig = static_cast<const phBoundComposite*>(GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds());

                bool bSpaceAvaliableToChangeBounds = true;

				u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
				if(m_fLastGroundClearance == fGroundClearance && (uCurrentTime - m_uLastGroundClearanceCheck) < suGroundClearanceInterval)
					return m_nAutomobileFlags.bLastSpaceAvaliableToChangeBounds;

                if(fGroundClearance < m_fGroundClearance || bAdjustBounds)// if we are reducing our ground clearance do a test to see if we have enough space.
                { 
					m_uLastGroundClearanceCheck = uCurrentTime;

                    const int iBoneIndexChassisLowLod=GetBoneIndex(VEH_CHASSIS_LOWLOD);
                    if( !force &&
						iBoneIndexChassisLowLod != -1 )
                    {
                        WorldProbe::CShapeTestBoundDesc boundTestDesc;

                        int iComponent = GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndexChassisLowLod);
                        if(iComponent > -1)
                        {
                            boundTestDesc.SetBound(pBoundCompOrig->GetBound(iComponent));

                            boundTestDesc.SetTransform(&RCC_MATRIX34(GetCurrentPhysicsInst()->GetMatrix()));
                            boundTestDesc.SetExcludeEntity(this);
                            boundTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
                            boundTestDesc.SetTypeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);

                            if(WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc))
                            {
                                bSpaceAvaliableToChangeBounds = false;
                            }
                        }
                    }
					
					m_nAutomobileFlags.bLastSpaceAvaliableToChangeBounds = bSpaceAvaliableToChangeBounds;
                }
				else if(!HasContactWheels() || GetFrameCollisionHistory()->GetCollisionImpulseMagSum() > 0.0f)
				{
					bSpaceAvaliableToChangeBounds = false;
				}

				m_fLastGroundClearance = fGroundClearance;
 
                if(bSpaceAvaliableToChangeBounds)
                {
                    m_fGroundClearance = fGroundClearance;
					m_fLastGroundClearanceHeightAboveGround = m_fHeightAboveRoad;

                    return true;// bounds should be modified
                }
            }
        }
    }

    return false;
}

static dev_float sfSideImpactGripReduction = 1.0f;
static dev_float sfSideImpactGripAddition = 3.0f;
static dev_float sfPITGripMulti = 0.1f;
float sfEngineRevVelocityThreshold = 0.1f;
float sfEngineRevAcceleration = 2.0f;
float sfEngineRevMinRevs = 0.55f;
float sfEngineRevRandomMin = 0.0f;
bank_float sfEngineRevStandardSuspensionStiffness = 4.5f;
bank_float sfEngineRevStandardHeightOfVehicle = 0.85f;
bank_float sfEngineRevStandardEngineForce = 0.20f;
bank_float sfEngineRevStandardEngineForceMax = 0.25f;


float bfSteeringBiasWhenWheelPopped = 90.0f;
float bfSteeringBiasLifeAfterWheelPopped = 1.0f;

float bfSteeringBiasWhenHitCarOnTheSide = 45.0f;
float bfSteeringBiasLifeAfterHitCarOnTheSide = 0.5f;

bank_float bfSteeringBiasWhenBeingPIT = 45.0f;
bank_float bfSteeringBiasLifeAfterBeingPIT_PlayerDefault = 0.75f;
bank_float bfSteeringBiasLifeAfterBeingPIT_PlayerMission = 0.15f;
bank_float bfSteeringBiasLifeAfterBeingPIT_AI = 1.5f;
dev_float fFullPITImpactDeltaSpeed = 0.5f;

dev_float VEHICLE_MIN_SPEED_TO_PIT = 16.67f; //about 60km/h
extern float sfMaximumMassForPushingVehicles;

PF_PAGE(AutomobileWheelForcesPage, "Wheel forces");
PF_GROUP(AutomobileWheelForces);
PF_LINK(AutomobileWheelForcesPage, AutomobileWheelForces);

PF_VALUE_FLOAT(TyreLoadLF, AutomobileWheelForces);
PF_VALUE_FLOAT(TyreLoadRF, AutomobileWheelForces);
PF_VALUE_FLOAT(TyreLoadLR, AutomobileWheelForces);
PF_VALUE_FLOAT(TyreLoadRR, AutomobileWheelForces);

PF_VALUE_FLOAT(WheelSpeedLF, AutomobileWheelForces);
PF_VALUE_FLOAT(WheelSpeedRF, AutomobileWheelForces);
PF_VALUE_FLOAT(WheelSpeedLR, AutomobileWheelForces);
PF_VALUE_FLOAT(WheelSpeedRR, AutomobileWheelForces);

PF_VALUE_FLOAT(SpeedDifferenceOnAxleF, AutomobileWheelForces);
PF_VALUE_FLOAT(SpeedDifferenceOnAxleR, AutomobileWheelForces);

PF_VALUE_FLOAT(SpeedDifferenceOnAxlePercentF, AutomobileWheelForces);
PF_VALUE_FLOAT(SpeedDifferenceOnAxlePercentR, AutomobileWheelForces);

PF_VALUE_FLOAT(DriveForceLF, AutomobileWheelForces);
PF_VALUE_FLOAT(DriveForceRF, AutomobileWheelForces);
PF_VALUE_FLOAT(DriveForceLR, AutomobileWheelForces);
PF_VALUE_FLOAT(DriveForceRR, AutomobileWheelForces);

PF_VALUE_FLOAT(DriveForceScaleFront, AutomobileWheelForces);
PF_VALUE_FLOAT(DriveForceScaleRear, AutomobileWheelForces);

PF_VALUE_FLOAT( EngineRPM, AutomobileWheelForces );

ePhysicsResult CAutomobile::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		ProcessReplayPhysics(fTimeStep, nTimeSlice);
		return PHYSICS_DONE;
	}
#endif // GTA_REPLAY

	RAGETRACE_SCOPE(CAutomobile_ProcessPhysics);

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	// Prefetch the wheel point array
	int iNumWheels = GetNumWheels();
	CWheel * const * pWheels = GetWheels();
	PrefetchObject(pWheels);

	//when changing ownership, a vehicle can sometimes
	//be left with its dummy constraints still active
	//until the next time it updates its lod
	if (IsNetworkClone() && HasDummyConstraint())
	{
		ChangeDummyConstraints(VDM_REAL, false);
	}

	if (HasParachute())
	{
		ProcessCarParachuteTint();
	}

	if(m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		if (GetVehicleAudioEntity() && (GetVehicleAudioEntity()->GetVehicleLOD() == AUD_VEHICLE_LOD_REAL))
		{
			ProcessLowLODAudioRequirements(fTimeStep);
		}
		return PHYSICS_DONE;
	}
	//------------------------------------------------------------------------------------------------------------
	// Prefetch the wheels
	for(int wheelIndex = 0; wheelIndex < iNumWheels; ++wheelIndex)
	{
		PrefetchObject(pWheels[wheelIndex]);
	}

	if(CVehicle::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice)==PHYSICS_POSTPONE)
	{
		return PHYSICS_POSTPONE;
	}


#if __BANK
    if( GetDriver() &&
        GetDriver()->IsLocalPlayer() )
    {
        float wheelSpeedLF = 0.0f;
        float wheelSpeedRF = 0.0f;
        float wheelSpeedLR = 0.0f;
        float wheelSpeedRR = 0.0f;

        for(int i = 0; i < iNumWheels && i < 4; i++ )
        {
            if( pWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_LEFTWHEEL ) )
            {
                if( pWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
                {
                    PF_SET( TyreLoadLR, pWheels[ i ]->GetTyreLoad() );
                    PF_SET( WheelSpeedLR, pWheels[ i ]->GetRotSpeed() );

                    wheelSpeedLR = pWheels[ i ]->GetRotSpeed();
                }
                else
                {
                    PF_SET( TyreLoadLF, pWheels[ i ]->GetTyreLoad() );
                    PF_SET( WheelSpeedLF, pWheels[ i ]->GetRotSpeed() );

                    wheelSpeedLF = pWheels[ i ]->GetRotSpeed();
                }
            }
            else
            {
                if( pWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
                {
                    PF_SET( TyreLoadRR, pWheels[ i ]->GetTyreLoad() );
                    PF_SET( WheelSpeedRR, pWheels[ i ]->GetRotSpeed() );

                    wheelSpeedRR = pWheels[ i ]->GetRotSpeed();
                }
                else
                {
                    PF_SET( TyreLoadRF, pWheels[ i ]->GetTyreLoad() );
                    PF_SET( WheelSpeedRF, pWheels[ i ]->GetRotSpeed() );

                    wheelSpeedRF = pWheels[ i ]->GetRotSpeed();
                }
            }
        }

        PF_SET( SpeedDifferenceOnAxleF, wheelSpeedLF - wheelSpeedRF );
        PF_SET( SpeedDifferenceOnAxleR, wheelSpeedLR - wheelSpeedRR );

        float averageWheelSpeed = wheelSpeedLF - wheelSpeedRF;
        if( wheelSpeedLF + wheelSpeedRF != 0.0f )
        {
            averageWheelSpeed = ( averageWheelSpeed / ( wheelSpeedLF + wheelSpeedRF ) ) * 100.0f;
        }

        PF_SET( SpeedDifferenceOnAxlePercentF, averageWheelSpeed );

        averageWheelSpeed = wheelSpeedLR - wheelSpeedRR;
        if( wheelSpeedLR + wheelSpeedRR != 0.0f )
        {
            averageWheelSpeed = ( averageWheelSpeed / ( wheelSpeedLR + wheelSpeedRR ) ) * 100.0f;
        }
        PF_SET( SpeedDifferenceOnAxlePercentR, averageWheelSpeed );

		PF_SET( EngineRPM, m_Transmission.GetRevRatio() );
    }
#endif // #if __BANK

	if(GetCurrentPhysicsInst()==NULL || !GetCurrentPhysicsInst()->IsInLevel())
		return PHYSICS_DONE;

	// postpone if we're sitting on something physical
	if(bCanPostpone)
	{
		for(int i=0; i<iNumWheels; i++)
		{
			if(pWheels[i]->GetHitPhysical())
			{
				return PHYSICS_POSTPONE;
			}
		}
	}

	// Cache out some values
	phCollider* pCollider = GetCollider();
	const Vector3 vVehVelocity(GetVelocityIncludingReferenceFrame());
	int iNumDoors = GetNumDoors();
	float fBrakeForce = GetBrakeForce()*GetBrake();

	for(int doorIndex = 0; doorIndex < iNumDoors; ++doorIndex)
	{
		PrefetchObject(&m_pDoors[doorIndex]);
	}
	PrefetchObject(pCollider);

	// if network has re-spotted this car and we don't want it to collide with other network vehicles for a while
	ProcessRespotCounter(fTimeStep);

	RAGETRACE_START(CAuto_Phys_Transmission);

	// do transmission stuff before we skip out because want to process every frame
	float fDriveForce = 0.0f;
	bool shouldUpdateTransmission = false;
	float fTransmissionTimeStep = fTimeStep;
	if(m_nVehicleFlags.bEngineOn)
	{
		if(CVehicleAILodManager::ms_bDeTimesliceTransmissionAndSleep)
		{
			shouldUpdateTransmission = (nTimeSlice == 0);
			fTransmissionTimeStep = fwTimer::GetTimeStep();
		}
		else
		{
			shouldUpdateTransmission = true;
		}
		if(!shouldUpdateTransmission)
		{
			fDriveForce = m_fLastDriveForce;
		}
	}
	else
	{
		m_fLastDriveForce = fDriveForce;
	}

	if(shouldUpdateTransmission)
	{
		float fDriveWheelRotSpeedsAverage = 0.0f;
		Vector3 vDriveWheelGroundVelocityAverage(Vector3::ZeroType);
		int nNumDriveWheels = 0;
		int nNumDriveWheelsOnGround = 0;
		int nNumDriveWheelsBrokenOff = 0;
		for(int i=0; i<iNumWheels; i++)
		{
			const CWheel & wheel = *pWheels[i];
			if(wheel.GetIsDriveWheel())
			{
				bool bCountWheelAsADriveWheel = true;
				if(m_nAutomobileFlags.bInBurnout)// if we're doing a burnout just ignore the locked wheels.
				{
					if(!(wheel.GetConfigFlags().IsFlagSet(WCF_REARWHEEL) || pHandling->m_fDriveBiasRear < 0.1f))
					{
						bCountWheelAsADriveWheel = false;
					}
				}

				if(bCountWheelAsADriveWheel)
				{
					//if( pHandling->GetCarHandlingData() &&
					//	pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT &&
					//	m_Transmission.GetGear() == 0 )
					//{
					//	fDriveWheelRotSpeedsAverage += Abs( wheel.GetGroundSpeed() );
					//}
					//else
					{
						fDriveWheelRotSpeedsAverage += wheel.GetGroundSpeed();
					}
					vDriveWheelGroundVelocityAverage += *(wheel.GetGroundBeneathWheelsVelocity());
					nNumDriveWheels++;
					if(wheel.GetIsTouching())
						nNumDriveWheelsOnGround++;
				}

				if(wheel.GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF))
				{
					// B*2531522: Make sure that the broken wheel count does not exceed the total drive wheel count.
					// Issue was that the drive force reduction due to broken wheels would generate a non-zero throttle if you tried to do a burnout in a vehicle with all four wheels missing.
					nNumDriveWheelsBrokenOff = Clamp(++nNumDriveWheelsBrokenOff, 0, nNumDriveWheels);
				}
			}
		}
		float fInvNumDriveWheels = 1.0f/nNumDriveWheels;
		fDriveWheelRotSpeedsAverage *= fInvNumDriveWheels;
		vDriveWheelGroundVelocityAverage *= fInvNumDriveWheels;
		const Mat34V matrix = GetCurrentPhysicsInst()->GetMatrix();
		bool isUnderLoad = false;

		if(m_Transmission.GetGear() <= 3)
		{
			isUnderLoad = GetVehicleAudioEntity()->HasHeavyRoadNoise();
			
			if(!isUnderLoad)
			{
				for( int i = 0; i < m_pVehicleGadgets.size(); i++)
				{
					if(m_pVehicleGadgets[i]->GetType()==VGT_TOW_TRUCK_ARM)
					{
						CVehicleGadgetTowArm * pTowArm = static_cast<CVehicleGadgetTowArm*>(m_pVehicleGadgets[i]);

						if(pTowArm->GetAttachedVehicle() && pTowArm->GetAttachedVehicle()->GetVelocity().Mag2() > 1.0f)
						{
							isUnderLoad = true;
							break;
						}
					}
				}
			}
		}

		m_Transmission.SetEngineUnderLoad(isUnderLoad);
		fDriveForce = m_Transmission.Process(this, matrix, RCC_VEC3V(GetVelocity()), fDriveWheelRotSpeedsAverage, vDriveWheelGroundVelocityAverage, nNumDriveWheels, nNumDriveWheelsOnGround, fTransmissionTimeStep);
		if(InheritsFromHeli())
		{
			fDriveForce = 0.0f;
		}

		fDriveForce *= ((nNumDriveWheels-nNumDriveWheelsBrokenOff) * fInvNumDriveWheels);//Reduce drive force if wheels are broken off.

		m_fLastDriveForce = fDriveForce;
	}

	RAGETRACE_START(CAuto_Phys_Doors);

	// process doors (doors can keep us awake?)
	bool bColliderIsArticulated = pCollider && pCollider->IsArticulated();
	u32 flagsToProcessWhenNotArticulated =
		CCarDoor::DRIVEN_AUTORESET | CCarDoor::DRIVEN_NORESET | CCarDoor::DRIVEN_GAS_STRUT | CCarDoor::DRIVEN_SWINGING | CCarDoor::DRIVEN_SMOOTH | CCarDoor::DRIVEN_SPECIAL | CCarDoor::PROCESS_FORCE_AWAKE | CCarDoor::RELEASE_AFTER_DRIVEN;
	for(int i=0; i<iNumDoors; i++)
	{
		CCarDoor & door = m_pDoors[i];
		if(!m_nVehicleFlags.bAnimateJoints || door.GetFlag(CCarDoor::DRIVEN_BY_PHYSICS))
		{
			if((bColliderIsArticulated || door.GetFlag(flagsToProcessWhenNotArticulated)) || door.GetBreakOffNextUpdate() )
			{
				door.ProcessPhysics(this, fTimeStep, nTimeSlice);
			}
		}
	}

	// skip default physics update, gravity and air resistance applied here
	if (ProcessIsAsleep())
	{
		bool updateAsleep = true;
		float fAsleepTimeStep = fTimeStep;
		if(CVehicleAILodManager::ms_bDeTimesliceTransmissionAndSleep && !IsRunningCarRecording())
		{
			updateAsleep = (nTimeSlice == 0);
			fAsleepTimeStep = fwTimer::GetTimeStep();
		}

		if(!updateAsleep)
		{
			return PHYSICS_DONE;
		}

		float fApplySteerAngle = GetSteerAngle();
		if(m_fSteeringBiasLife > 0.0f)
		{
			fApplySteerAngle = rage::Clamp(fApplySteerAngle + m_fSteeringBias * pHandling->m_fSteeringLock, -pHandling->m_fSteeringLock, pHandling->m_fSteeringLock);
			if(m_fSteeringBiasLife > (fAsleepTimeStep * 2.0f))
			{
				m_fSteeringBias *= (m_fSteeringBiasLife - fAsleepTimeStep) / m_fSteeringBiasLife;
			}
			else
			{
				m_fSteeringBias = 0.0f;
			}
			m_fSteeringBiasLife -= fAsleepTimeStep;
		}

		float gravityForce = m_fGravityForWheelIntegrator*GetMass();
		bool isInactiveRecording = IsRunningCarRecording();
		for(int i=0; i<iNumWheels; i++)
		{
			CWheel & wheel = *pWheels[i];
			const float fApplySteerAngleForWheel = fApplySteerAngle * wheel.GetFrontRearSelector();
			wheel.SetSteerAngle(fApplySteerAngleForWheel);
			wheel.SetBrakeAndDriveForce( fBrakeForce, fDriveForce, GetThrottle(), 0.0f );

			if(pHandling->hFlags & HF_HANDBRAKE_REARWHEELSTEER)
			{				
				// Form circular wheel formation
				if(wheel.GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
				{
					const float f2ndSteerAngle = GetSecondSteerAngle()* wheel.GetFrontRearSelector();
					wheel.SetSteerAngle(f2ndSteerAngle);
				}				
			}
			else if(GetHandBrake() && !(pHandling->hFlags & HF_NO_HANDBRAKE))
			{
				wheel.SetHandBrakeForce(GetHandBrakeForce());
			}
			wheel.ProcessAsleep(vVehVelocity, fAsleepTimeStep, gravityForce, isInactiveRecording);
		}
		m_vecInternalForce.Zero();
		m_vecInternalTorque.Zero();

		// If the vehicle is inactive and playing back the recording, we still want to update it's suspension
		if((CVehicleRecordingMgr::IsPlaybackGoingOnForCar(this) || GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE) && CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()) && iNumWheels > 0)
		{
			ProcessProbes(MAT34V_TO_MATRIX34(GetMatrix()));
		}
		return PHYSICS_DONE;
	}
	else
	{
		// If we activated as a result of ProcessIsAsleep() our ptr to the collider may be out of date, so refresh it here.
		pCollider = GetCollider();
	}

	const CPed * pDriver = GetDriver();
	float fVehSpeedSq = vVehVelocity.Mag2();

	bool bIsUpsideDownAmphiCar = InheritsFromAmphibiousAutomobile() && VEC3V_TO_VECTOR3(GetTransform().GetC()).Dot(Vector3(0.0f, 0.0f, 1.0f)) < 0.0f;
    bool bSubmarineCarUpsideDownNotInSubmarineMode = GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR && !IsInSubmarineMode() && VEC3V_TO_VECTOR3(GetTransform().GetC()).Dot(Vector3(0.0f, 0.0f, 1.0f)) < 0.0f;
	
	bool bIsCarOrQuadBike = ( GetVehicleType()==VEHICLE_TYPE_CAR || 
							  GetVehicleType()==VEHICLE_TYPE_QUADBIKE ||
							  ( !m_nFlags.bPossiblyTouchesWater &&
								( GetVehicleType()==VEHICLE_TYPE_SUBMARINECAR ||
								  GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE ||
								  GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ) ) ||
							  bIsUpsideDownAmphiCar ||
                              bSubmarineCarUpsideDownNotInSubmarineMode );

	bool bDriverIsPlayer = pDriver && pDriver->IsPlayer();

	CVehicleModelInfo* pModelInfo = GetVehicleModelInfo();

	RAGETRACE_START(CAuto_Phys_Bouyancy);

	// Make sure the possbilyTouchesWater flag is up to date before acting on it...
	if(m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
	{
		ProcessPossiblyTouchesWater(fTimeStep, nTimeSlice, pModelInfo);
	}

	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[i]->ProcessPhysics(this,fTimeStep,nTimeSlice);
	}

	if(m_pVehicleWeaponMgr)
	{
		m_pVehicleWeaponMgr->ProcessPhysics(this,fTimeStep,nTimeSlice);
	}
	else if( GetStatus() == STATUS_WRECKED )
    {
        if( IsTank() )// Make sure the tank turret stops spinning when blown up.
	    {
            fragInstGta* pFragInst = GetVehicleFragInst();
            int iBoneIndex = GetBoneIndex(VEH_TURRET_1_BASE);
            if(iBoneIndex != -1 && pFragInst)
            {
                int nChildIndex = pFragInst->GetComponentFromBoneIndex(iBoneIndex);
                if(nChildIndex != -1)
                {
				    Freeze1DofJointInCurrentPosition(pFragInst, nChildIndex);
			    }
		    }
	    }
        else if( GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_OUTRIGGER_LEGS ) )
        {
            fragInstGta* pFragInst = GetVehicleFragInst();

            if( pFragInst )
            {
                int iBoneIndex = GetBoneIndex(VEH_TURRET_1_BARREL);
                if(iBoneIndex != -1)
                {
                    int nChildIndex = pFragInst->GetComponentFromBoneIndex(iBoneIndex);
                    if(nChildIndex != -1)
                    {
                        Freeze1DofJointInCurrentPosition(pFragInst, nChildIndex, true);

                        //u8 groupIndex = GetVehicleFragInst()->GetTypePhysics()->GetChild( nChildIndex )->GetOwnerGroupPointerIndex();
                        //if( GetVehicleFragInst()->GetCacheEntry() &&
                        //    GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->latchedJoints->IsClear(groupIndex) )
                        //{
                        //    GetVehicleFragInst()->GetTypePhysics()->GetGroup( groupIndex )->SetLatchStrength( -1.0f );
                        //
                        //    GetVehicleFragInst()->CloseLatchAbove( nChildIndex );

                        //    static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound())->SetIncludeFlags( nChildIndex, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES );
                        //}
                    }
                }
            }
        }
    }

	RAGETRACE_START(CAuto_Phys_FlyingCar);
	ProcessFlightHandling(fTimeStep);

	ModifyControlsIfDucked(pDriver, fTimeStep);

	// give player in-air control, and chance to roll car back on it's wheels when it crashes
	if(GetStatus()==STATUS_PLAYER && bIsCarOrQuadBike )
	{
		ProcessDriverInputsForStability(fTimeStep);

		// Add some tiny torque to the vehicle that's generate from the engine rev
		if(bDriverIsPlayer && IsEngineOn() && fVehSpeedSq < (sfEngineRevVelocityThreshold * sfEngineRevVelocityThreshold) && pModelInfo && pHandling)
		{
			const float fRevRatio = m_Transmission.GetRevRatio();
			if( fRevRatio > sfEngineRevMinRevs )
			{
				float fScale = (fRevRatio - sfEngineRevMinRevs) / (1.0f - sfEngineRevMinRevs);
				fScale *= fwRandom::GetRandomNumberInRange(sfEngineRevRandomMin, 1.0f);
				const Vector3 vAxis(VEC3V_TO_VECTOR3(GetTransform().GetB()));

				float fSuspensionForce = Clamp(pHandling->m_fSuspensionForce + pHandling->m_fAntiRollBarForce, 1.0f, sfEngineRevStandardSuspensionStiffness);
				float fSuspensionForceScale = fSuspensionForce/ sfEngineRevStandardSuspensionStiffness;

				float fHeightOfVehicle = Clamp(pModelInfo->GetBoundingBoxMax().z, sfEngineRevStandardHeightOfVehicle, 5.0f);
				float fHeightOfVehicleScale = ( sfEngineRevStandardHeightOfVehicle / fHeightOfVehicle);

				float fEngineForce = Clamp(m_Transmission.GetDriveForce(), 0.0f, sfEngineRevStandardEngineForce);
				float fEngineForceScale = fEngineForce / sfEngineRevStandardEngineForceMax;

				ApplyInternalTorque(fScale * fHeightOfVehicleScale * fEngineForceScale * fSuspensionForceScale * sfEngineRevAcceleration * GetAngInertia().x * vAxis);
			}
		}

		if(pHandling && (pHandling->hFlags & HF_OFFROAD_ABILITIES_X2))// Do some auto leveling if in the air.
		{
			if(IsInAir() && !GetFrameCollisionHistory()->GetMostSignificantCollisionRecord())
			{
				bool bDoAutoLeveling = true;
				if(GetDriver() && GetDriver()->IsAPlayerPed())
				{
					if( ( !MI_CAR_BRICKADE.IsValid() || GetModelIndex() != MI_CAR_BRICKADE.GetModelIndex() ) &&
						( !MI_CAR_APC.IsValid() || GetModelIndex() != MI_CAR_APC.GetModelIndex() ) )
					{
						Vec3V vPosition = GetTransform().GetPosition();
						float fMapMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPosition.GetXf(), vPosition.GetYf());

						//Let the player perform rolls when really high in the air
						Vec3V vBBMinZGlobal = GetTransform().Transform(Vec3V(0.0f, 0.0f, GetBoundingBoxMin().z));
						if(vBBMinZGlobal.GetZf() > fMapMaxZ)
						{
							bDoAutoLeveling = false;
						}
					}
					else
					{
						bDoAutoLeveling = false;
					}
				}

				if(bDoAutoLeveling)
				{
					static dev_float sfAutoLevelMult = -0.5f;
					AutoLevelAndHeading( sfAutoLevelMult, true );
				}
			}
		}
	}

	float fApplySteerAngle = GetSteerAngle();
	float fFwdSpeed = DotProduct(vVehVelocity, VEC3V_TO_VECTOR3(GetTransform().GetB()));

	// Only do this for player car as it is nice to be able to see what ai cars are trying to do.
	// For ai cars there already is some code to stop extreme steering at high speed in carai.cpp
	if(bDriverIsPlayer)
	{
		// do speed based steering angle
		float fSideSpeed = DotProduct(vVehVelocity, VEC3V_TO_VECTOR3(GetTransform().GetA()));

		//if(fSideSpeed * fApplySteerAngle > 0.0f)
		//{
		//	fApplySteerAngle /= pHandling->m_fSteeringLock;
		//	fApplySteerAngle *= GetWheel(0)->GetSteerAngleForMaxTraction(GetVelocity().Mag());
		//}

		//static float SPEED_STEER_REDUCTION_MULT = 0.95f;
		//float fMoveSpeedMag = GetVelocity().Mag();
		//if(fSideSpeed * fApplySteerAngle >= -0.05f*fMoveSpeedMag && fFwdSpeed > 0.0f)
		//{
		//	fApplySteerAngle *= rage::Powf(SPEED_STEER_REDUCTION_MULT, fFwdSpeed);
		//}

		if(fFwdSpeed > sfPlayerSpeedSteerFwdThreshold && !(fSideSpeed * fApplySteerAngle < -sfPlayerSpeedSteerSideRatioThreshold*rage::Abs(fFwdSpeed)))
		{
			fApplySteerAngle /= 1.0f + sfPlayerSpeedSteerMult * (fFwdSpeed - sfPlayerSpeedSteerFwdThreshold);
		}

		if(fFwdSpeed > 1.0f && sbPlayerSpeedSteerAutoCentre && !(pHandling->hFlags & HF_STEER_REARWHEELS))
		{
			float fAutoCentreSteerAngle = rage::Atan2f(-fSideSpeed, fFwdSpeed);
			fAutoCentreSteerAngle = rage::Clamp(fAutoCentreSteerAngle, -sfPlayerSpeedSteerAutoCentreMax, sfPlayerSpeedSteerAutoCentreMax);
			fApplySteerAngle = rage::Clamp(fApplySteerAngle + fAutoCentreSteerAngle, -pHandling->m_fSteeringLock, pHandling->m_fSteeringLock);
		}

		// do extra stability while braking (like a rudder effect)
		if(GetVehicleType() == VEHICLE_TYPE_QUADBIKE ||
			GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
		{
			if(m_vehControls.m_brake > 0.0f && vVehVelocity.Mag2() > 0.1f)
			{
				Vector3 vecBrakeStabiliseTorque(VEC3V_TO_VECTOR3(GetTransform().GetC()));
				static dev_float sfBrakingStability = -1.0f;
				vecBrakeStabiliseTorque.Scale(fSideSpeed * m_vehControls.m_brake * sfBrakingStability * GetAngInertia().z);

				ApplyInternalTorque(vecBrakeStabiliseTorque);
			}
		}

		if(InheritsFromPlane())
		{
			CPlane *pThisPlane = (CPlane *)this;
			fApplySteerAngle *= pThisPlane->GetAircraftDamage().GetYawMult(pThisPlane);
		}
	}

	if(m_fSteeringBiasLife > 0.0f)
	{
		fApplySteerAngle = rage::Clamp(fApplySteerAngle + m_fSteeringBias * pHandling->m_fSteeringLock, -pHandling->m_fSteeringLock, pHandling->m_fSteeringLock);
		if(m_fSteeringBiasLife > (fTimeStep * 2.0f))
		{
			m_fSteeringBias *= (m_fSteeringBiasLife - fTimeStep) / m_fSteeringBiasLife;
		}
		else
		{
			m_fSteeringBias = 0.0f;
		}
		m_fSteeringBiasLife -= fTimeStep;
	}

	// setup cheat flags
	u32 nResetWheelFlags = WF_ABS|WF_RESET_CHEATS|WF_REDUCE_GRIP;
	if(CPhysics::GetIsFirstTimeSlice(nTimeSlice))
		nResetWheelFlags |= WF_ABS_ACTIVE;

	u32 nWheelCheatFlags = 0;

	// Anti-lock brakes
	if(GetCheatFlag(VEH_CHEAT_ABS|VEH_SET_ABS))
		nWheelCheatFlags |= WF_ABS;
	else if(GetCheatFlag(VEH_SET_ABS_ALT))
		nWheelCheatFlags |= WF_ABS_ALT;
	else if(GetStatus()==STATUS_PHYSICS)
		nWheelCheatFlags |= WF_ABS;

	// Usually-off cheats.
	u32 nAnyCheatFlags = VEH_CHEAT_TC|VEH_SET_TC|VEH_CHEAT_SC|VEH_SET_SC|VEH_CHEAT_GRIP1|VEH_SET_GRIP1|VEH_CHEAT_GRIP2|VEH_SET_GRIP2;
	if(GetCheatFlag(nAnyCheatFlags))
	{
		// Traction control
		if(GetCheatFlag(VEH_CHEAT_TC|VEH_SET_TC))
			nWheelCheatFlags |= WF_CHEAT_TC;
		// Stability control
		if(GetCheatFlag(VEH_CHEAT_SC|VEH_SET_SC))
			nWheelCheatFlags |= WF_CHEAT_SC;
		// Extra grip
		if(GetCheatFlag(VEH_CHEAT_GRIP1|VEH_SET_GRIP1))
			nWheelCheatFlags |= WF_CHEAT_GRIP1;
		// Extra extra grip
		if(GetCheatFlag(VEH_CHEAT_GRIP2|VEH_SET_GRIP2))
			nWheelCheatFlags |= WF_CHEAT_GRIP2;
	}

	// Lower traction for a moment after a handbrake turn
	float fGripMult = GetHandBrakeGripMult();

	if(!m_nVehicleFlags.bIsBig && !m_nVehicleFlags.bIsBus && GetMass() < sfMaximumMassForPushingVehicles)
	{
		if(const CCollisionRecord *pVehicleCollision = GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE))
		{
			if(pVehicleCollision->m_pRegdCollisionEntity && pVehicleCollision->m_pRegdCollisionEntity->GetIsTypeVehicle())
			{
				CVehicle *pOtherVehicle = (CVehicle *) pVehicleCollision->m_pRegdCollisionEntity.Get();
				
				// only player, cop, AI that's ramming can perform PIT maneuver
				bool bCanPIT = pOtherVehicle->GetDriver() && pOtherVehicle->GetDriver()->IsPlayer() && m_nVehicleFlags.bCarAgainstCarSideCollision;
				bCanPIT |= pOtherVehicle->GetDriver() && (pOtherVehicle->GetDriver()->GetPedType() == PEDTYPE_COP || pOtherVehicle->GetDriver()->GetPedType() == PEDTYPE_SWAT);
				bCanPIT |= pOtherVehicle->GetIntelligence()->GetActiveTask() && pOtherVehicle->GetIntelligence()->GetActiveTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_RAM;
				bCanPIT |= (CTimeHelpers::GetTimeSince(pOtherVehicle->GetIntelligence()->GetLastTimeRamming()) < 100);

				if (bCanPIT)
				{
					const CPhysical* pOtherAvoidanceTarget = pOtherVehicle->GetIntelligence()->GetAvoidanceCache().m_pTarget;
					const CPhysical* pOurAvoidanceTarget = GetIntelligence()->GetAvoidanceCache().m_pTarget;
					if (pOtherAvoidanceTarget && (pOurAvoidanceTarget == pOtherAvoidanceTarget))
					{
						bCanPIT = false;
					}
				}

				if(bCanPIT)
				{
 					Vector3 pOtherPosLocal = pVehicleCollision->m_OtherCollisionPos;
					MAT34V_TO_MATRIX34(PHSIM->GetLastInstanceMatrix(pOtherVehicle->GetCurrentPhysicsInst())).UnTransform(pOtherPosLocal);
					if(pVehicleCollision->m_fCollisionImpulseMag > SMALL_FLOAT && pVehicleCollision->m_MyCollisionPosLocal.y < (GetBoundingBoxMin().y * 0.5f) && pOtherPosLocal.y > (pOtherVehicle->GetBoundingBoxMax().y * 0.5f))
					{
						float fSteeringMult = Clamp(pVehicleCollision->m_fCollisionImpulseMag / (GetMass() * fFullPITImpactDeltaSpeed), 0.0f, 1.0f);
						float fSpeedMult = Clamp(vVehVelocity.Mag() / VEHICLE_MIN_SPEED_TO_PIT, 0.0f, 1.0f);
						float fNewSteeringBias = bfSteeringBiasWhenBeingPIT * DtoR * fSteeringMult * fSpeedMult * Sign(pVehicleCollision->m_MyCollisionNormal.Dot(VEC3V_TO_VECTOR3(GetMatrix().GetCol0())));
						if(m_fSteeringBiasLife <= 0.0f || Sign(m_fSteeringBias) != Sign(fNewSteeringBias) || Abs(fNewSteeringBias) > (Abs(m_fSteeringBias) + SMALL_FLOAT))
						{
							m_fSteeringBias = fNewSteeringBias;
							if(bDriverIsPlayer)
							{
								if(CTheScripts::GetPlayerIsOnAMission())
								{
									m_fSteeringBiasLife = bfSteeringBiasLifeAfterBeingPIT_PlayerMission * fSteeringMult;
								}
								else
								{
									m_fSteeringBiasLife = bfSteeringBiasLifeAfterBeingPIT_PlayerDefault * fSteeringMult;
								}
							}
							else
							{
								m_fSteeringBiasLife = bfSteeringBiasLifeAfterBeingPIT_AI * fSteeringMult;
							}
						}
						fGripMult *= sfPITGripMulti;
						m_nVehicleFlags.bCarBrushAgainstCarSideCollision = false; //disable able the brushing against car effects, which steers car in the opposite direction
					}
				}
			}
		}
	}
	
	if( m_nVehicleFlags.bCarAgainstCarSideCollision )
    {
        fGripMult *= sfSideImpactGripReduction;
        m_nVehicleFlags.bCarAgainstCarSideCollision = false;
    }

	if(m_nVehicleFlags.bCarBrushAgainstCarSideCollision)
	{
		if(GetDriver() && !GetDriver()->IsPlayer())
		{
			if(const CCollisionRecord *pVehicleCollision = GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE))
			{

				if(pVehicleCollision->m_fCollisionImpulseMag > SMALL_FLOAT)
				{
					bool bApplySteeringBias = true;
					CVehicle *pOtherVehicle = (CVehicle *) pVehicleCollision->m_pRegdCollisionEntity.Get();
					if(pOtherVehicle->GetDriver())
					{
						const CPhysical* pOtherAvoidanceTarget = pOtherVehicle->GetIntelligence()->GetAvoidanceCache().m_pTarget;
						const CPhysical* pOurAvoidanceTarget = GetIntelligence()->GetAvoidanceCache().m_pTarget;
						if (pOtherAvoidanceTarget && (pOurAvoidanceTarget == pOtherAvoidanceTarget))
						{
							bApplySteeringBias = false;
						}
					}

					if(bApplySteeringBias)
					{
						float fNewSteeringBias = bfSteeringBiasWhenHitCarOnTheSide * DtoR * Sign(pVehicleCollision->m_MyCollisionNormal.Dot(VEC3V_TO_VECTOR3(GetMatrix().GetCol0()))) * -1.0f;
						if((m_fSteeringBiasLife <= 0.0f || Sign(m_fSteeringBias) != Sign(fNewSteeringBias)))
						{
							m_fSteeringBias = fNewSteeringBias;
							m_fSteeringBiasLife = bfSteeringBiasLifeAfterHitCarOnTheSide;
						}
					}
				}
			}
		}

		m_nVehicleFlags.bCarBrushAgainstCarSideCollision = false;
	}

	if( HasParachute() )
	{
		ProcessCarParachute();
	}

    if( m_nVehicleFlags.bCarHitByHeavyVehicle )
    {
        fGripMult *= sfSideImpactGripAddition;
        m_nVehicleFlags.bCarHitByHeavyVehicle = false;
    }

	u32 nWheelSetFlags = nWheelCheatFlags | (m_nAutomobileFlags.bReduceGripModeEnabled || GetExplosionEffectSlick() ? WF_REDUCE_GRIP : 0);

	float fMassMultForAcceleration = 1.0f + (m_sfPassengerMassMult * static_cast<float>(GetNumberOfPassenger()));

	phBound * pVehBound = (GetVehicleFragInst()->GetArchetype() ? GetVehicleFragInst()->GetArchetype()->GetBound() : NULL);
	phBoundComposite * pVehCompositeBound = (pVehBound && pVehBound->GetType()==phBound::COMPOSITE ? static_cast<phBoundComposite*>(pVehBound) : NULL);
	bool resetWheelCollision = false;

	// update inputs for each wheel
	bool	bRestingOnPhysical = false;
	float	fMaxGripMult = fGripMult;

	TUNE_GROUP_FLOAT( VEHICLE_SHUNT_TUNE, FRICTION_AFTER_SHUNT, 0.1f, 0.0f, 10.0f, 0.02f );
	TUNE_GROUP_FLOAT( VEHICLE_SHUNT_TUNE, FRICTION_INCREASE_TIME_MODIFIER, 1.0f, 0.0f, 10.0f, 0.2f );
	static dev_float sf_MaxAngularVelocityStart = 2.0f;

	float velocityAroundZ = GetAngVelocity().Dot( VEC3V_TO_VECTOR3( GetTransform().GetUp() ) );
	float velocityAroundZAbs = Abs( velocityAroundZ );

	for(int i=0; i<iNumWheels; i++)
	{
		CWheel & wheel = *pWheels[i];

		if( m_nVehicleFlags.bCarHitWhileInRoadBlock )
		{
			float fPrevGripMult = wheel.GetGripMult();
			if( fPrevGripMult >= fGripMult )
			{
				fGripMult = FRICTION_AFTER_SHUNT;
			}
			else
			{
				fGripMult = fPrevGripMult + ( fTimeStep * FRICTION_INCREASE_TIME_MODIFIER );

				if( fGripMult >= fMaxGripMult )
				{
					m_nVehicleFlags.bCarHitWhileInRoadBlock = false;
				}
			}
		}

		float fApplySteerAngleForWheel = fApplySteerAngle * wheel.GetFrontRearSelector();

		if( GetModelIndex() == MI_CAR_CHERNOBOG &&
			wheel.GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
		{
			static dev_float sfChernobogRearSteerReduction = 0.75f;
			static dev_float sfChernobogMaxSpeedForRearSteer = ( 1.0f / 20.0f ) * sfChernobogRearSteerReduction;
			static dev_float sfChernobogMaxReverseSpeedForRearSteer = ( 1.0f / 10.0f ) * sfChernobogRearSteerReduction;

			float speedReductionScale = sfChernobogMaxSpeedForRearSteer;

			if( fFwdSpeed < 0.0f )
			{
				speedReductionScale = sfChernobogMaxReverseSpeedForRearSteer;
			}
			//else
			{
				float steeringReduction = Max( 0.0f, sfChernobogRearSteerReduction - ( Abs( fFwdSpeed ) * speedReductionScale ) );
				fApplySteerAngleForWheel *= steeringReduction;
			}
		}
		//if( pHandling->GetCarHandlingData() &&
		//	pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT &&
		//	m_Transmission.GetGear() == 0 )
		//{
		//	wheel.SetSteerAngle( -fApplySteerAngleForWheel );
		//}
		//else
		{
			wheel.SetSteerAngle( fApplySteerAngleForWheel );
		}
		
		wheel.SetMaterialFlags();

		float fTankDriveForce = fDriveForce;
		float throttle = GetThrottle();
		static dev_float sf_MaxSpeedForSteeringTorque = 15.0f;

		if( ( pHandling->GetCarHandlingData() &&
			  pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT ) &&
			m_Transmission.GetGear() > 0 &&
			m_vehControls.GetThrottle() >= 0.0f &&
			Abs( fFwdSpeed ) < sf_MaxSpeedForSteeringTorque )
		{
			float steeringAngle = m_vehControls.GetSteerAngle() * ( wheel.GetConfigFlags().IsFlagSet( WCF_LEFTWHEEL ) ? -1.0f : 1.0f );
			if( Abs( steeringAngle ) > 0.1f )
			{
				float throttleForWheel = Clamp( m_vehControls.GetThrottle() + ( ( 1.0f - m_vehControls.GetThrottle() ) * ( steeringAngle ) ), -1.0f, 1.0f );

				if( GetThrottle() != 0.0f )
				{
					fTankDriveForce /= GetThrottle();
				}

				fTankDriveForce *= throttleForWheel;

				static dev_float sf_ScaleDriveForceMin = 0.15f;
				static dev_float sf_ScaleDriveForceMax = 1.0f;
				static dev_float sf_MaxAngularVelocityEnd = 4.0f;

				fTankDriveForce += fTankDriveForce * ( 1.0f - m_vehControls.GetThrottle() ) * ( sf_ScaleDriveForceMin + ( ( sf_ScaleDriveForceMax - sf_ScaleDriveForceMin ) * Min( 1.0f, 1.0f - Abs( fFwdSpeed ) / sf_MaxSpeedForSteeringTorque ) ) );

				if( velocityAroundZAbs > sf_MaxAngularVelocityStart )
				{
					fTankDriveForce *= 1.0f - Min( 2.0f, ( velocityAroundZAbs - sf_MaxAngularVelocityStart ) / sf_MaxAngularVelocityEnd );
				}
				throttle = Abs( throttleForWheel );

				wheel.SetSteerAngle( fApplySteerAngleForWheel * Abs( m_vehControls.GetThrottle() ) );
			}
		}

		if( ( pHandling->GetCarHandlingData() &&
			  pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT ) &&
			Abs( GetThrottle() ) < 0.05f )
		{
			static dev_float minBrakeForce = 0.025f;
			fBrakeForce = Max( minBrakeForce, fBrakeForce );
		}

		// GTAV - B*1877402 - Stop the plane wheels spinning when it's in the air.
		if( ( InheritsFromPlane() || InheritsFromHeli() ) && GetNumContactWheels() == 0 )
		{
			wheel.SetBrakeAndDriveForce( 0.1f, fDriveForce, GetThrottle(), fVehSpeedSq );
		}
		else if(InheritsFromAmphibiousAutomobile() && static_cast<CAmphibiousAutomobile*>(this)->IsPropellerSubmerged())
		{
			TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_WATER_BRAKE_FORCE, 0.001f, 0.0f, 10.0f, 0.1f);
			// Apply a small brake force on the wheels when in water for amphibious quads so that they don't spin indefinitely
			wheel.SetBrakeAndDriveForce( WHEEL_WATER_BRAKE_FORCE, fDriveForce, GetThrottle(), fVehSpeedSq );
		}
		else
		{
			wheel.SetBrakeAndDriveForce( fBrakeForce, fTankDriveForce, throttle, fVehSpeedSq );
		}

		wheel.GetDynamicFlags().ClearFlag(nResetWheelFlags);
		wheel.GetDynamicFlags().SetFlag(nWheelSetFlags);
        wheel.SetGripMult(fGripMult);

        wheel.SetMassMultForAcceleration( fMassMultForAcceleration );

        wheel.UpdateGroundVelocity();

		bRestingOnPhysical = ( wheel.GetHitPhysical() != NULL || bRestingOnPhysical );

        if(m_nAutomobileFlags.bBurnoutModeEnabled)
        {
            wheel.SetBrakeAndDriveForce(1.0f, fDriveForce, 1.0f, 0.0f);//force into burnout mode
        }


		if(pHandling->hFlags & HF_HANDBRAKE_REARWHEELSTEER)
		{
			// Form circular wheel formation
			if(wheel.GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				const float f2ndSteerAngle = GetSecondSteerAngle()* wheel.GetFrontRearSelector();
				wheel.SetSteerAngle(f2ndSteerAngle);
			}			
		}
		else if(GetHandBrake() && !(pHandling->hFlags & HF_NO_HANDBRAKE))
		{
			wheel.SetHandBrakeForce(GetHandBrakeForce());
		}

		// B*1858054: Make sure to flag if wheels need to have their collision re-enabled because the vehicle went from being on its side/upside down to the right way up or vice versa.	
		// This only needs to be done for vehicles that do not have the wheel timeslicing optimisation enabled; vehicles that do use it will have their wheel collision reset every other frame anyway.
		if(pModelInfo && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_TIMESLICE_WHEELS))
		{
			if(pVehCompositeBound)
			{
				int mainWheelComponentIndex = wheel.GetFragChild(0);
				if(mainWheelComponentIndex >= 0)
				{
					bool usingVehicleCollisionArchetypeFlags = pVehCompositeBound->GetIncludeFlags(mainWheelComponentIndex) == ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;

					// We are checking the state of the vehicle (on its side, upside down, normal) and seeing if the appropriate wheel collision flags are set for that state.
					// If the incorrect flags are set (e.g. upside down, but not set to use the standard vehicle collision archetype flags), the flag is set so the wheel collision is setup correctly.
					if(!(IsOnItsSide() || IsUpsideDown()) && usingVehicleCollisionArchetypeFlags)
					{
						resetWheelCollision = true;
					}
					else if((IsOnItsSide() || IsUpsideDown()) && !usingVehicleCollisionArchetypeFlags)
					{
						resetWheelCollision = true;
					}
				}				
			}
		}		
	}

	if( ( pHandling->GetCarHandlingData() &&
		  pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT ) &&
		m_Transmission.GetGear() > 0 )
	{
		//float totalBrakeForce = 0.0f;
		//float totalDriveForce = 0.0f;

		//for( int i = 0; i < GetNumWheels(); i++ )
		//{
		//	totalBrakeForce += GetWheel( i )->GetBrakeForce();
		//	totalDriveForce += GetWheel( i )->GetDriveForce();
		//}

		//float targetDriveForce = fDriveForce * m_vehControls.GetThrottle() * GetNumWheels();

		//if( targetDriveForce != totalDriveForce )
		//{
		//	float driveForceDelta = targetDriveForce - totalDriveForce;

		//	for( int i = 0; i < GetNumWheels(); i++ )
		//	{
		//		float newBrakeForce = GetWheel( i )->GetBrakeForce();
		//		float newDriveForce = GetWheel( i )->GetDriveForce();

		//		if( Sign( newDriveForce ) != Sign( newDriveForce + driveForceDelta ) )
		//		{
		//			newBrakeForce += -( newDriveForce + driveForceDelta );
		//			newDriveForce = 0.0f;
		//		}
		//		else
		//		{
		//			newDriveForce += driveForceDelta;
		//		}

		//		GetWheel( i )->SetBrakeForce( newBrakeForce );
		//		GetWheel( i )->SetDriveForce( newDriveForce );
		//	}
		//}

		if( GetThrottle() == 0.0f )
		{
			static dev_float slowDownScale = 0.1f;
			Vector3 slowDownTorque = Vector3( 0.0f, 0.0f, ( ( GetAngInertia().z * velocityAroundZ * slowDownScale ) / ( GetNumWheels() * fTimeStep ) ) );

			for( int i = 0; i < GetNumWheels(); i++ )
			{
				//if( ( velocityAroundZ > 0.0f && GetWheel( i )->IsFlagSet( WCF_LEFTWHEEL ) ) || 
				//	( velocityAroundZ < 0.0f && !GetWheel( i )->IsFlagSet( WCF_LEFTWHEEL ) ) )
				{
					Vector3 localWheelOffset = CWheel::GetWheelOffset( GetVehicleModelInfo(), GetWheel( i )->GetHierarchyId() );
					Vector3 slowDownForce;
					slowDownForce.Cross( localWheelOffset, slowDownTorque );
					GetWheel( i )->SetBrakeForce( GetWheel( i )->GetBrakeForce() + ( -slowDownForce.y * GetInvMass() ) );
				}
			}
		}

		if( GetNumContactWheels() > ( int )( (float)GetNumWheels() * 0.5f ) )
		{
			static dev_float sfTurnOnSpotMaxTurnSpeed = 2.0f;
			float targetRotationSpeed = GetSteerAngle() * sfTurnOnSpotMaxTurnSpeed;

			if( Abs( targetRotationSpeed ) > velocityAroundZAbs &&
				Sign( targetRotationSpeed ) == Sign( velocityAroundZ ) )
			{
				static dev_float maxForwardSpeedForExtraTorque = 2.5f;
				static dev_float maxTorque = 2.0f;
				float torqueReduction = 1.0f - Min( 1.0f, Abs( fFwdSpeed ) / maxForwardSpeedForExtraTorque );

				static dev_float extraTorqueScale = 1.0f;
				float extraTorqueFactor = Clamp( ( targetRotationSpeed - velocityAroundZ ) * extraTorqueScale * torqueReduction * ( 1.0f - m_vehControls.GetThrottle() ) / fTimeStep, -maxTorque, maxTorque );
				Vector3 extraTorque = VEC3V_TO_VECTOR3( GetTransform().GetUp() ) * GetAngInertia().z * extraTorqueFactor;
				ApplyInternalTorque( extraTorque );
			}
		}
	}

	//if( CVehicle::sm_bInDetonationMode &&
	//	m_sideShuntForce == 0.0f )
	//{
	//	TUNE_GROUP_FLOAT( ARENA_MODE, EXTRA_LATERAL_DAMPING, -0.75f, -1.0f, 1.0f, 0.1f );
	//	Vector3 extraDamping = VEC3V_TO_VECTOR3( GetTransform().GetRight() );
	//	extraDamping *= GetVelocity().Dot( extraDamping ) * EXTRA_LATERAL_DAMPING;
	//	ApplyInternalForceCg( extraDamping * GetMass() );
	//}

    ApplyDifferentials();

    if(iNumWheels > 0)
    {
		// if speed is Zero collision will not have been processed -> need to force if not skipping physics
		if(!m_nVehicleFlags.bVehicleColProcessed)
		{
			ProcessProbes(MAT34V_TO_MATRIX34(GetMatrix()));

			if(CVehicleAILodManager::ms_bDeTimesliceWheelCollisions)
			{
				if(!bDriverIsPlayer && pModelInfo && !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_TIMESLICE_WHEELS))
				{
					static dev_float sfMaxSlopeAngle = 0.90f;
					if(!m_nVehicleFlags.bWheelsDisabled && !m_nVehicleFlags.bDriveMusclesToAnim && !bRestingOnPhysical && GetMatrix().GetCol2().GetZf() > sfMaxSlopeAngle)// if we're driving muscles we want as much stability as we can get. Also if we are on a steep slope don't timeslice as it can hurt stability.
					{
						DisableWheelCollisions();
					}
					else
					{
						EnableWheelCollisions();
					}
				}
				// resetWheelCollision indicates that vehicle needs to re-enable wheel collisions to make sure they are setup correctly after the vehicle changes orientation.
				else if(m_nVehicleFlags.bWheelsDisabled || resetWheelCollision)
				{
					EnableWheelCollisions();
				}
				m_nVehicleFlags.bWheelsDisabledOnDeactivation = false;
			}
		}

        // Make sure the wheels are setup for Franklin's special ability.
        ProcessSlowMotionSpecialAbilityPrePhysics(pDriver);

		RAGETRACE_START(CAuto_Phys_Wheels);

		// Sanity check for optimisation of caching the current collider above.
		Assertf(pCollider==GetCollider(), "Cached pCollider doesn't match current collider.");
	    if(pCollider)
	    {
			if(!InheritsFromTrailer())
			{
				// Trailers are applying forces to themselves in CTrailer::ProcessPhysics after calling this function
				// The function hierarchy here is really ugly...
				StartWheelIntegratorTask(pCollider,fTimeStep);
			}
		    return PHYSICS_NEED_SECOND_PASS;
	    }
    }

	return ProcessPhysics2(fTimeStep, nTimeSlice);	
}

//
//
//
#if GTA_REPLAY
void CAutomobile::ProcessReplayPhysics(float fTimeStep, int nTimeSlice)
{
	// On replay we still need to process the tow truck physics so the arm updates
	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[i]->ProcessPhysics(this,fTimeStep,nTimeSlice);
	}

	// On replay we still need to process the vehicle doors physics so the bounds can be updated for visibility. 
	phCollider* pCollider = GetCollider();
	bool bColliderIsArticulated = pCollider && pCollider->IsArticulated();
	int iNumDoors = GetNumDoors();
	u32 flagsToProcessWhenNotArticulated =
		CCarDoor::DRIVEN_AUTORESET | CCarDoor::DRIVEN_NORESET | CCarDoor::DRIVEN_GAS_STRUT | CCarDoor::DRIVEN_SWINGING | CCarDoor::DRIVEN_SMOOTH | CCarDoor::DRIVEN_SPECIAL | CCarDoor::PROCESS_FORCE_AWAKE | CCarDoor::RELEASE_AFTER_DRIVEN;
	for(int i=0; i<iNumDoors; i++)
	{
		CCarDoor & door = m_pDoors[i];
		if(!m_nVehicleFlags.bAnimateJoints || door.GetFlag(CCarDoor::DRIVEN_BY_PHYSICS))
		{
			if((bColliderIsArticulated || door.GetFlag(flagsToProcessWhenNotArticulated)) || door.GetBreakOffNextUpdate() )
			{
				door.ReplayUpdateDoors(this);
			}
		}
	}

	// update vehicle bounds
	if(GetVehicleFragInst()->GetCached() &&
        GetModelIndex() != MI_PLANE_AVENGER )
	{
		static_cast<phBoundComposite*>(GetVehicleFragInst()->GetCacheEntry()->GetBound())->CalculateCompositeExtents(true);
		if(CVehicle::ms_bUseAutomobileBVHupdate)
		{
			CPhysics::GetLevel()->UpdateCompositeBvh(GetVehicleFragInst()->GetLevelIndex());
		}
		else
		{
			CPhysics::GetLevel()->RebuildCompositeBvh(GetVehicleFragInst()->GetLevelIndex());
		}

		CPhysics::GetLevel()->UpdateObjectLocationAndRadius(GetVehicleFragInst()->GetLevelIndex(),(Mat34V_Ptr)(NULL));
	}
}
#endif

//
//
//
void CAutomobile::ProcessPossiblyTouchesWater(float fTimeStep, int nTimeSlice, const CVehicleModelInfo* pModelInfo)
{
	if( InheritsFromSubmarineCar() || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE )
	{
		float fBuoyancyAccel = 0.0f;
		SetIsInWater( m_Buoyancy.Process(this, fTimeStep, false, CPhysics::GetIsLastTimeSlice(nTimeSlice), &fBuoyancyAccel) );
		return;
	}

	if(InheritsFromAmphibiousQuadBike())
	{
		return;
	}

	// If bPossiblyTouchesWater is false we are either well above the surface of the water or in a tunnel.
	if (!m_nFlags.bPossiblyTouchesWater)
	{
		// we can't set these flags for network clones of script objects directly as they are synced
		if(!IsNetworkClone() || !(GetNetworkObject() && GetNetworkObject()->IsScriptObject(true)))
		{
			SetIsInWater( false );
			m_nVehicleFlags.bIsDrowning = false;
		}
		m_Buoyancy.ResetBuoyancy();
		m_fTimeInWater = 0.0f;
		m_Buoyancy.m_fForceMult = m_fOrigBuoyancyForceMult;

		if(pHandling->GetSeaPlaneHandlingData())
		{
			if(CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CPlane*>(this)->GetExtension<CSeaPlaneExtension>())
			{
				pSeaPlaneExtension->m_fTimeOnWater = 0.0f;
			}
		}
	}
	// else process the buoyancy class
	else 
	{
		int iNumContactWheels = GetNumContactWheels();
		float fBuoyancyAccel = 0.0f;
		SetIsInWater( m_Buoyancy.Process(this, fTimeStep, false, CPhysics::GetIsLastTimeSlice(nTimeSlice), &fBuoyancyAccel) );

		// we can't set these flags for network clones of script objects directly as they are synced
		if(!IsNetworkClone() || !(GetNetworkObject() && GetNetworkObject()->IsScriptObject(true)))
		{
			m_nVehicleFlags.bIsDrowning = false;
		}

		bool bCanStartSinking = true;
		bool bEngineUnderWater = false;
		float fSinkTime = InheritsFromTrailer() ? TRAILER_SINK_TIME : AUTOMOBILE_SINK_TIME;
		float fEngineOffTime = AUTOMOBILE_SINK_TIME;
		if(InheritsFromPlane() && pModelInfo)
		{
			s32 nSingleEngineBoneId = pModelInfo->GetBoneIndex(VEH_ENGINE);
			s32 nLeftEngineBoneId = pModelInfo->GetBoneIndex(PLANE_ENGINE_L);
			s32 nRightEngineBoneId = pModelInfo->GetBoneIndex(PLANE_ENGINE_R);
			if(nSingleEngineBoneId != -1)
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nSingleEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}
			else if(nLeftEngineBoneId != -1)
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nLeftEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}
			else if(nRightEngineBoneId != -1)
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nRightEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}
			else
			{
				Assertf(0, "Plane without recognised engine bone - %s.", GetModelName());
			}

			fEngineOffTime = PLANE_ENGINE_OFF_TIME;
			if(pHandling->GetSeaPlaneHandlingData())
			{
				bCanStartSinking = false;
				if(bEngineUnderWater)
				{
					m_fTimeInWater += fTimeStep;

					if(m_fTimeInWater > AUTOMOBILE_SINK_TIME)
					{
						fEngineOffTime = 0.0f;
						bCanStartSinking = true; // doesn't actually sink but we need to set this so the engine gets turned off
					}
				}

				// Force the seaplane landing gear to retract if floating in water for a short time.
				if(GetIsInWater() && !HasContactWheels())
				{
					CPlane* pPlane = static_cast<CPlane*>(this);
					if(CSeaPlaneExtension* pSeaPlaneExtension = pPlane->GetExtension<CSeaPlaneExtension>())
					{
						pSeaPlaneExtension->m_fTimeOnWater += fTimeStep;
						dev_float kfSeaPlaneGearRetractInWaterTime = 1.0f;
						if(pSeaPlaneExtension->m_fTimeOnWater > kfSeaPlaneGearRetractInWaterTime)
						{	
							pPlane->GetLandingGear().ControlLandingGear(this, CLandingGear::COMMAND_RETRACT);
						}
					}
				}
			}
			else
			{
				bCanStartSinking = (fBuoyancyAccel < m_fGravityForWheelIntegrator*1.25f && iNumContactWheels <= 0) || bEngineUnderWater
					|| m_fTimeInWater > PLANE_ENGINE_OFF_TIME || (pHandling->m_fPercentSubmerged > 100.0f && m_Buoyancy.GetSubmergedLevel()>0.1f);
			}

			// Planes sink immediately (without bobbing upwards first) if they go in fast and get below a certain depth.
			Vec3V vPosition = GetTransform().GetPosition();
			const float kfDepthToSinkPlaneImmediately = 3.0f;
			if(m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame() && !pHandling->GetSeaPlaneHandlingData()
				&& m_Buoyancy.GetAvgAbsWaterLevel() - vPosition.GetZf() > kfDepthToSinkPlaneImmediately)
			{
				bCanStartSinking = true;
				fEngineOffTime = 0.0f;
				fSinkTime = 0.0f;
				m_Buoyancy.m_fForceMult = 0.0f;
			}
		}
		else if(InheritsFromBlimp())
		{
			fEngineOffTime = PLANE_ENGINE_OFF_TIME;
			bCanStartSinking = m_fTimeInWater > PLANE_ENGINE_OFF_TIME || m_Buoyancy.GetSubmergedLevel()>0.5f;

			s32 nLeftEngineBoneId = pModelInfo->GetBoneIndex(BLIMP_ENGINE_L);

			if(nLeftEngineBoneId != -1)
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nLeftEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}

			s32 nRightEngineBoneId = pModelInfo->GetBoneIndex(BLIMP_ENGINE_R);
			if(!bEngineUnderWater && nRightEngineBoneId != -1)
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nRightEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}

			if( GetStatus() == STATUS_WRECKED &&
				m_nPhysicalFlags.bRenderScorched )
			{
				bCanStartSinking = true;
				fEngineOffTime = 0.0f;
				fSinkTime = 0.0f;
				m_Buoyancy.m_fForceMult = 0.0f;
			}

			Assertf(nRightEngineBoneId != -1 || nLeftEngineBoneId != -1, "Blimp without recognised engine bone - %s.", GetModelName());
		}
		else if(InheritsFromHeli())
		{
			if( ( static_cast< CHeli* >( this )->GetIsDrone() ) )
			{
				bCanStartSinking = m_fTimeInWater > PLANE_ENGINE_OFF_TIME;
			}
			else
			{			
				fEngineOffTime = PLANE_ENGINE_OFF_TIME;
				bCanStartSinking = m_fTimeInWater > PLANE_ENGINE_OFF_TIME || m_Buoyancy.GetSubmergedLevel()>0.5f;
			}

			s32 nEngineBoneId = pModelInfo->GetBoneIndex(VEH_ENGINE);
			if(Verifyf(nEngineBoneId != -1, "Heli without engine bone - %s.", GetModelName()))
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}

			if( pHandling->GetSeaPlaneHandlingData() )
			{
				bCanStartSinking = false;
				if( bEngineUnderWater )
				{
					m_fTimeInWater += fTimeStep;

					if( m_fTimeInWater > AUTOMOBILE_SINK_TIME )
					{
						fEngineOffTime = 0.0f;
						bCanStartSinking = true; // doesn't actually sink but we need to set this so the engine gets turned off
					}
				}
			}
		}
		else
		{
			if(!InheritsFromTrailer() && pModelInfo)
			{
				s32 nEngineBoneIdx = pModelInfo->GetBoneIndex(VEH_ENGINE);
				if(Verifyf(nEngineBoneIdx != -1, "Car without engine bone - %s.", GetModelName()))
				{
					Matrix34 matEngineBoneLocal = GetObjectMtx(nEngineBoneIdx);
					Matrix34 vehicleMatrix = MAT34V_TO_MATRIX34(GetMatrix());
					Vector3 vEnginePosLocal = matEngineBoneLocal.d;
					Vector3 vQueryPosLocal = vEnginePosLocal;

					s32 nWheelBoneIdx = pModelInfo->GetBoneIndex(VEH_WHEEL_LF);
					if(Verifyf(nWheelBoneIdx != -1 && GetWheelFromId(VEH_WHEEL_LF), "Car without front left wheel - %s.", GetModelName()))
					{
						Vector3 vWheelPosLocal(VEC3_ZERO);
						GetDefaultBonePosition(nWheelBoneIdx, vWheelPosLocal);
						vWheelPosLocal.z += GetWheelFromId(VEH_WHEEL_LF)->GetWheelRadius();

						vQueryPosLocal.z = rage::Max(vEnginePosLocal.z, vWheelPosLocal.z);
					}
					s32 nExhaustBoneIdx = pModelInfo->GetBoneIndex(VEH_EXHAUST);
					if( nExhaustBoneIdx != -1 )
					{
						Vector3 vExhaustPosLocal(VEC3_ZERO);
						GetDefaultBonePosition( nExhaustBoneIdx, vExhaustPosLocal );

						vQueryPosLocal.z = rage::Max( vEnginePosLocal.z, vExhaustPosLocal.z );
					}

					if( MI_CAR_WASTELANDER.IsValid() && 
						GetModelIndex() == MI_CAR_WASTELANDER )
					{
						vQueryPosLocal.z += 2.0f;
					}

					Vector3 vQueryPosWorld;
					vehicleMatrix.Transform(vQueryPosLocal, vQueryPosWorld);

					if(m_Buoyancy.GetAbsWaterLevel() > vQueryPosWorld.z)
					{
						bEngineUnderWater = true;
					}
				}
			}
			else if( MI_TRAILER_TRAILERSMALL2.IsValid() && GetModelIndex() == MI_TRAILER_TRAILERSMALL2 ) 
			{
				s32 nEngineBoneIdx = pModelInfo->GetBoneIndex(VEH_ENGINE);

				Matrix34 matEngineBoneLocal = GetObjectMtx(nEngineBoneIdx);
				Matrix34 vehicleMatrix = MAT34V_TO_MATRIX34(GetMatrix());
				Vector3 vQueryPosLocal = matEngineBoneLocal.d;
				vQueryPosLocal.z += 0.5f;

				Vector3 vQueryPosWorld;
				vehicleMatrix.Transform(vQueryPosLocal, vQueryPosWorld);

				if(m_Buoyancy.GetAbsWaterLevel() > vQueryPosWorld.z)
				{
					bEngineUnderWater = true;
				}
			}

			bCanStartSinking = (fBuoyancyAccel < m_fGravityForWheelIntegrator*1.25f && iNumContactWheels < GetNumWheels()) || bEngineUnderWater
				|| m_fTimeInWater > AUTOMOBILE_SINK_TIME || (pHandling->m_fPercentSubmerged > 100.0f && m_Buoyancy.GetSubmergedLevel()>0.2f);
		}

		// Riders of quadbikes get ejected as soon as we are unsupported in water.
		if(InheritsFromQuadBike() && fBuoyancyAccel > m_fGravityForWheelIntegrator * 0.75f && iNumContactWheels==0)
		{
			// Use a damage event to knock the passengers off the quad.
			for( s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); iSeat++ )
			{
				CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat);
				if( pPassenger && !pPassenger->IsInjured() )
				{
					pPassenger->KnockPedOffVehicle(false);
				}
			}
		}

		if (pHandling->m_fPercentSubmerged > AUTOMOBILE_SINK_EVENT_SUBMERGE_PERCENT )
		{
			// Throw an event so world peds will have something to react against.
			if (GetDriver())
			{
				CEventShockingCarCrash carCrashEvent(*this, GetDriver());
				carCrashEvent.SetVisualReactionRangeOverride(AUTOMOBILE_SINK_CRASH_EVENT_RANGE);	//MAGIC - need a higher range than normal so peds on the beach can see this
				carCrashEvent.SetAudioReactionRangeOverride(AUTOMOBILE_SINK_CRASH_EVENT_RANGE);
				CShockingEventsManager::Add(carCrashEvent);
			}
		}

		if(bCanStartSinking)
		{
			// Update time in water
			m_fTimeInWater += fTimeStep;	// This will accumulate until the car is driven to dry land

			// Should we cut the engine and make this vehicle unusable?
			bool bCutEngine;
			if(InheritsFromHeli())
			{
				bCutEngine = m_fTimeInWater>fEngineOffTime && m_Buoyancy.GetSubmergedLevel()>0.6f;
			}
			else if(InheritsFromPlane())
			{
				bCutEngine = (m_fTimeInWater>fEngineOffTime&&bEngineUnderWater) || (!HasContactWheels()&&m_Buoyancy.GetStatus()==FULLY_IN_WATER);
			}
			else
			{
				bCutEngine = ( m_fTimeInWater > fEngineOffTime&&bEngineUnderWater ) || 
							 ( !HasContactWheels() && m_Buoyancy.GetStatus() == FULLY_IN_WATER && 
							   ( m_specialFlightModeRatio == 0.0f || bEngineUnderWater ) );
			}
			if(bCutEngine && !IsNetworkClone())
			{
				// Turn engine off here
				if(m_nVehicleFlags.bEngineOn)
					SwitchEngineOff();

				const bool wasWrecked = (GetStatus() == STATUS_WRECKED);

				// Don't allow the engine to be restarted.
				if(!wasWrecked)
				{
					SetIsWrecked();
				}

				//It was not wrecked before and isWrecked from sink lets check if the plane was damaged by a ped.
				if (GetNetworkObject() && !wasWrecked && GetStatus() == STATUS_WRECKED)
				{
					bool triggerNetworkEvent = false;
					if (InheritsFromPlane())
					{
						if (((CPlane*)this)->GetDestroyedByPed())
						{
							triggerNetworkEvent = true;
						}
						//Attribute it to the driver...
						else if (GetDriver() && GetDriver()->IsPlayer())
						{
							SetWeaponDamageInfo(GetDriver(), WEAPONTYPE_UNARMED, fwTimer::GetTimeInMilliseconds());
							triggerNetworkEvent = true;
						}
					}
					else if(GetIsRotaryAircraft() && ((CRotaryWingAircraft*)this)->GetHeliRotorDestroyedByPed())
					{
						triggerNetworkEvent = true;
					}
					else if (MI_JETPACK_THRUSTER.IsValid() && GetModelIndex() == MI_JETPACK_THRUSTER)
					{
						// Special case here for the jetpack to send damage event correctly. Although it is a rotary aircraft, it has no rotors so won't pass the check above if it's never been damaged.
						if (GetDriver() && GetDriver()->IsPlayer())
						{
							SetWeaponDamageInfo(GetDriver(), WEAPONTYPE_UNARMED, fwTimer::GetTimeInMilliseconds());
							triggerNetworkEvent = true;
						}
					}
					else if (GetIsLandVehicle())
					{
						//Make sure if a remote driver is driving your vehicle that 
						//  he gets charged with the correct insurance amount.
						CPed* damager = GetVehicleWasSunkByAPlayerPed();
						if (damager)
						{
							SetWeaponDamageInfo(damager, WEAPONTYPE_UNARMED, fwTimer::GetTimeInMilliseconds());
							triggerNetworkEvent = true;
						}
					}

					if(triggerNetworkEvent)
					{
						CNetObjPhysical::NetworkDamageInfo damageInfo(GetWeaponDamageEntity(),GetHealth(),0.0f,0.0f,false,GetWeaponDamageHash(),0,false,true,false);
						static_cast<CNetObjPhysical*>(GetNetworkObject())->UpdateDamageTracker(damageInfo);
					}
				}
			}

			// Decrease buoyancy factor if been in water for long time. We don't want the blimp sinking ever (it's derived from CHeli).
			if( m_fTimeInWater > fSinkTime && 
				!InheritsFromBlimp() && 
				!InheritsFromAmphibiousAutomobile() &&
				!pHandling->GetSeaPlaneHandlingData() )
			{
				// we can't set these flags for network clones of script objects directly as they are synced
				if((bEngineUnderWater || InheritsFromTrailer()) && (!IsNetworkClone() || !(GetNetworkObject() && GetNetworkObject()->IsScriptObject(true))))
				{
					m_nVehicleFlags.bIsDrowning = true;
				}

				m_Buoyancy.m_fForceMult -= AUTOMOBILE_SINK_STEP;
				if(m_Buoyancy.m_fForceMult < AUTOMOBILE_SINK_FORCE_MULT_MULTIPLIER * m_fOrigBuoyancyForceMult)
					m_Buoyancy.m_fForceMult = AUTOMOBILE_SINK_FORCE_MULT_MULTIPLIER * m_fOrigBuoyancyForceMult;

				if(m_nVehicleFlags.GetIsSirenOn())
					TurnSirenOn(false,false);

				// CS: Removed due to underwater swimming
				//					// Check for vehicle well below water surface and then fix car if below 10m
				// 					if(GetPosition().z < ms_fVehicleSubmergeSleepGlobalZ)
				// 						DeActivatePhysics();
			}
		}

		if(!GetIsInWater())
		{
			m_fTimeInWater = 0.0f;		// Reset timer if we aren't in water
			m_Buoyancy.m_fForceMult = m_fOrigBuoyancyForceMult;
		}

	}
}

BANK_ONLY(bool sbDisableMidairWheelIntegrator = false;)
void CAutomobile::StartWheelIntegratorTask(phCollider* pCollider, float fTimeStep)
{
	// Only run the full integrator if we're touching the ground
	// NOTE: I think this could be run for non-Helis it's just that we would almost never benefit from it and HasContactWheels isn't cheap.
	if(!InheritsFromHeli() || HasContactWheels() BANK_ONLY(|| sbDisableMidairWheelIntegrator))
	{
		if(pCollider->IsArticulated())
		{
			pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, true);
		}
		else
		{
			pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
		}

		CWheelIntegrator::ProcessIntegration(pCollider, this, m_WheelIntegrator, fTimeStep, fwTimer::GetFrameCount());
	}
	else
	{
		// Enable gravity
		pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, false);
		pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
		
		// Ensure the user gravity is being used
		pCollider->SetGravityFactor(m_fGravityForWheelIntegrator/-GRAVITY);

		// Apply internal forces
		ScalarV vTimeStep = ScalarVFromF32(fTimeStep);
		pCollider->ApplyForceCenterOfMass(m_vecInternalForce,vTimeStep.GetIntrin128ConstRef());
		pCollider->ApplyTorque(m_vecInternalTorque,vTimeStep.GetIntrin128ConstRef());
	}
}

RAGETRACE_DECL(CAutomobile_ProcessPostPhysics);

ePhysicsResult CAutomobile::ProcessPhysics2(float fTimeStep, int nTimeSlice)
{
	Assert(!m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy));

	RAGETRACE_SCOPE(CAutomobile_ProcessPostPhysics);

	if(GetNumWheels() > 0)
	{
		phCollider* pCollider = GetCollider();
		if (pCollider)
		{
			CWheelIntegrator::ProcessResult(m_WheelIntegrator, 0);
#if PHARTICULATEDBODY_MULTITHREADED_VALDIATION
			if(pCollider->IsArticulated())
			{
				static_cast<phArticulatedCollider*>(pCollider)->GetBody()->SetReadOnlyOnMainThread(false);
			}
#endif // PHARTICULATEDBODY_MULTITHREADED_VALDIATION
		}

		if(GetSkeleton() && !IsDummy())
		{
			Mat34V matrix = GetVehicleFragInst()->GetMatrix();
			float fWheelExtension = CWheel::ms_fWheelExtensionRate*fTimeStep;

			bool bUpdateWheelsWithDamage	= false;
			m_nAutomobileFlags.bHydraulicsBounceRaising = false;
			m_nAutomobileFlags.bHydraulicsBounceLanding = false;

			for(int i=0; i<GetNumWheels() && m_nVehicleFlags.bCanDeformWheels; i++)
			{
				CWheel* wheel = GetWheel(i);

				if( wheel )
				{
					wheel->UpdateHydraulics( nTimeSlice );

					if ( (wheel->GetFragChild() > -1) && wheel->GetConfigFlags().IsFlagSet(WCF_UPDATE_SUSPENSION) )
					{
						bUpdateWheelsWithDamage = true;
					}
				}
			}

			const void *basePtr = NULL; //Lock the texture once for all wheels
			if(bUpdateWheelsWithDamage && GetVehicleDamage() && GetVehicleDamage()->GetDeformation() && GetVehicleDamage()->GetDeformation()->HasDamageTexture())
			{
				basePtr = GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead);
			}

			bool wheelFlagsUpdated = false;
			for(int i=0; i<GetNumWheels(); i++)
			{
				CWheel* wheel = GetWheel(i);

				if ((wheel != NULL) && (wheel->GetFragChild() > -1))
				{
					wheel->ProcessPreSimUpdate( basePtr, pCollider, matrix, fWheelExtension);
				}

				if( !wheelFlagsUpdated &&
					wheel->UpdateHydraulicsFlags( this ) )
				{
					wheelFlagsUpdated = true;
				}
			}
			if(basePtr)
			{
				GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
			}
		}

        // Make sure the wheels are reset from Franklin's special ability.
        ProcessSlowMotionSpecialAbilityPostPhysics();
	}

	// Non-articulated vehicles already should have maximum extents calculated.
	// Articulated vehicles will be out of date until they are integrated prior to collision. Since this 
	//   is called from the PreSimUpdate there isn't anything testing against the extents until collision.
	if(GetVehicleFragInst()->GetCached() && CVehicle::ms_bAlwaysUpdateCompositeBound)
	{
		static_cast<phBoundComposite*>(GetVehicleFragInst()->GetCacheEntry()->GetBound())->CalculateCompositeExtents(true);
		if(CVehicle::ms_bUseAutomobileBVHupdate)
		{
			CPhysics::GetLevel()->UpdateCompositeBvh(GetVehicleFragInst()->GetLevelIndex());
		}
		else
		{
			CPhysics::GetLevel()->RebuildCompositeBvh(GetVehicleFragInst()->GetLevelIndex());
		}

		CPhysics::GetLevel()->UpdateObjectLocationAndRadius(GetVehicleFragInst()->GetLevelIndex(),(Mat34V_Ptr)(NULL));
	}

	// wheel ratios have been reset, so ProcessEntityColision need to be 
	// called again before wheel ratios can been used
	m_nVehicleFlags.bVehicleColProcessed = false;
	m_nVehicleFlags.bRestingOnPhysical = false;

	m_vecInternalForce.Zero();
	m_vecInternalTorque.Zero();

	// Pick-up hook and magnet need a third pass so they can apply forces and impulses to other vehicles without conflicting with the wheel integrator task.
	bool bHasMagnetGadget = false;
    for( int i = 0; i < m_pVehicleGadgets.size(); i++)
    {
        m_pVehicleGadgets[i]->ProcessPhysics2(this,fTimeStep);

		if(m_pVehicleGadgets[i]->GetType() == VGT_PICK_UP_ROPE || m_pVehicleGadgets[i]->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			bHasMagnetGadget = true;
    }

#if TRACK_VEHICLE_GRADIENT
	if(GetStatus() == STATUS_PLAYER)
	{
		float fAverageGradient = 0.0f;
		int iHitWheels = 0;
		
		for(int i =0; i < GetNumWheels(); i++)
		{
			if(GetWheel(i) && GetWheel(i)->GetWasTouching())
			{
				Vector3 vHitNorm = GetWheel(i)->GetHitNormal();
				Vector3 vSideWays = GetA();
				Vector3 vFront;
				vFront.Cross(vHitNorm,vSideWays);
				vFront.NormalizeSafe();

				fAverageGradient += -rage::Asinf(vFront.z);
				iHitWheels ++;
			}
		}

		if(iHitWheels > 0)
		{
			fAverageGradient *= 180.0f / (PI * (float)iHitWheels);			
		}
		

		PF_SET(vehicleGradientDeg, fAverageGradient);
	}
#endif

	// Running a no-op third pass if we need to run the net blender update, 
	// to make sure the articulated body is unlocked from the wheel process 
	// before we accessing/modifying it from the net blender update.
	if(CPhysics::GetIsFirstTimeSlice(nTimeSlice))
	{
		netObject *networkObject = GetNetworkObject();

		if (networkObject && networkObject->IsClone() && networkObject->GetNetBlender() && networkObject->CanBlend())
		{
			return PHYSICS_NEED_THIRD_PASS;
		}
	}

	if(m_nVehicleFlags.bMayHaveWheelGroundForcesToApply || bHasMagnetGadget)
	{
		return PHYSICS_NEED_THIRD_PASS;
	}
	else
	{
		return PHYSICS_DONE;
	}
}

//
ePhysicsResult CAutomobile::ProcessPhysics3(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	// If the ground wheel force application has been deferred, do it now
	if(m_nVehicleFlags.bMayHaveWheelGroundForcesToApply)
	{
		if(phCollider* pCollider = GetCollider())
		{
			CWheelIntegrator::ProcessResultsFinal( pCollider, m_ppWheels, GetNumWheels(), ScalarVFromF32(fTimeStep) );
		}
		m_nVehicleFlags.bMayHaveWheelGroundForcesToApply = false;
	}

	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[i]->ProcessPhysics3(this,fTimeStep);
	}

	return PHYSICS_DONE;
}

int CAutomobile::InitPhys()
{	
	if( (GetModelIndex() == MI_CAR_VISERIS.GetModelIndex() &&
		pHandling->GetCarHandlingData()->aFlags == 0) ||
		IsTunerVehicle())
	{
		pHandling->GetCarHandlingData()->aFlags |= CF_HARD_REV_LIMIT;
	}
	return CVehicle::InitPhys();
}

void CAutomobile::InitDummyInst()
{
	Assert(!m_pDummyInst);
	Assert(GetVehicleModelInfo()->GetPhysics());

	// Check to see if this vehicle has the dummy bound already in the frag type, in which case we don't need a dummy inst
	if (HasDummyBound())
	{
		return;
	}

	const Matrix34 matrix = MAT34V_TO_MATRIX34(GetMatrix());
	phInstGta* pNewInst = rage_new phInstGta(PH_INST_VEH);
	Assert(pNewInst);
	pNewInst->Init(*GetVehicleModelInfo()->GetPhysics(), matrix);
	Assert(pNewInst->GetArchetype());
	Assert(pNewInst->GetArchetype()->GetTypeFlags() == ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);

	m_pDummyInst = pNewInst;
	m_pDummyInst->SetUserData((void*)this);
	m_pDummyInst->SetInstFlag(phfwInst::FLAG_USERDATA_ENTITY, true);

	// This ensures that UpdateAirResistance() sets the damping parameters properly the next time it's called.
	RefreshAirResistance();

	// Make sure Dummy automobiles don't flip over.
	/*
	if(CVehicleAILodManager::ms_dummyUseUseConstrainedRotation)
	{
		static dev_float DUMMY_PITCH_AND_ROLL_LIMIT = 45.0f*DtoR;

		phConstraint* pConstraint = CPhysics::GetSimulator()->GetConstraintMgr()->AttachObjectToWorldRotation('CAIP', *m_pDummyInst);
		if(pConstraint)
		{
			pConstraint->SetDegreesConstrained(2,ZAXIS);
			pConstraint->SetHardLimitMin(-DUMMY_PITCH_AND_ROLL_LIMIT);
			pConstraint->SetHardLimitMax(DUMMY_PITCH_AND_ROLL_LIMIT);
			pConstraint->SetFixedRotation();
		}
	}
	*/


}

void CAutomobile::ChangeDummyConstraints(const eVehicleDummyMode dummyMode, bool bIsStatic)
{
	//don't use dummy constraints for network clones
	if (IsNetworkClone())
	{
		const bool bRemovedConstraint = RemoveConstraints();
		if (bRemovedConstraint)
		{
			DetachFromParentAndChildren(DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS);
		}
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	// Don't use constraints for vehicles on a trailer or for trailers themselves.
	const bool bDummyParentIsTrailer = m_nVehicleFlags.bHasParentVehicle && GetDummyAttachmentParent() && GetDummyAttachmentParent()->InheritsFromTrailer();
	const bool bRealParentIsTrailer = m_nVehicleFlags.bHasParentVehicle && CVehicle::IsEntityAttachedToTrailer(this);
	if (bDummyParentIsTrailer || bRealParentIsTrailer || InheritsFromTrailer())
	{
		Assert(!m_dummyStayOnRoadConstraintHandle.IsValid());
		return;
	}

	s32 iNewConstraintsMode;

	switch(dummyMode)
	{
		case VDM_REAL : case VDM_NONE : default : { iNewConstraintsMode = CVehicleAILodManager::DCM_None; break; }
		case VDM_DUMMY : { iNewConstraintsMode = CVehicleAILodManager::GetDummyConstraintMode(); break; }
		case VDM_SUPERDUMMY :
		{
			iNewConstraintsMode = (bIsStatic && CVehicleAILodManager::ms_bDisableConstraintsForSuperDummyInactive) ?
				CVehicleAILodManager::DCM_None : CVehicleAILodManager::GetSuperDummyConstraintMode();
			break;
		}
	}

	//-----------------------------------------
	// Set up a physical distance constraint

	if(iNewConstraintsMode == CVehicleAILodManager::DCM_PhysicalDistance || iNewConstraintsMode == CVehicleAILodManager::DCM_PhysicalDistanceAndRotation)
	{
		if(!m_dummyStayOnRoadConstraintHandle.IsValid() || !CPhysics::GetConstraintManager()->GetTemporaryPointer(m_dummyStayOnRoadConstraintHandle))
		{
			// Make sure dummy automobiles don't go off the roads.
			// do this by applying a stay on roads constraint.
			// This acts like a cable with some slack attached to the centre of the road.
			// The vehicle should be able to move freely under normal physics control unless
			// the get too far from the centre of the road at which point the cable will
			// prevent them form moving further
			const CVehicle* pVehicleConstraintsToUse = this;
			if (InheritsFromTrailer() && m_nVehicleFlags.bHasParentVehicle)
			{
				CPhysical* pAttachParent = (CPhysical *) GetAttachParent();	
				if (pAttachParent && pAttachParent->GetIsTypeVehicle())
				{
					pVehicleConstraintsToUse = (CVehicle*)pAttachParent;
				}
				else if (GetDummyAttachmentParent())
				{
					pVehicleConstraintsToUse = GetDummyAttachmentParent();
				}
			}

			bool validConstraintFound = false;
			Vec3V vConstraintPos;
			float fConstraintMaxLength = 0.0f;

			// Unless the follow route has m_bAllowUseForPhysicalConstraint == false, indicating a recent
			// script teleport, let CalculatePathInfo() compute the constraint position/length.
			const CVehicleFollowRouteHelper* pFollowRoute = pVehicleConstraintsToUse->GetIntelligence()->GetFollowRouteHelper();
			if(!pFollowRoute || pFollowRoute->ShouldAllowUseForPhysicalConstraint())
			{
				validConstraintFound = CVirtualRoad::CalculatePathInfo(*pVehicleConstraintsToUse, false, false, vConstraintPos, fConstraintMaxLength, false);
			}

			if(validConstraintFound)
			{
				const Vec3V vVehPos = GetTransform().GetPosition();
				phConstraintDistance::Params dummyStayOnRoadConstraintParams;
				dummyStayOnRoadConstraintParams.worldAnchorA = vVehPos;
				dummyStayOnRoadConstraintParams.worldAnchorB = vConstraintPos;
				dummyStayOnRoadConstraintParams.instanceA = GetCurrentPhysicsInst();

				float distToConstraint = Dist(vVehPos, vConstraintPos).Getf();
				float minAllowableLength = distToConstraint - PH_CONSTRAINT_ASSERT_DEPTH + 0.25f;
#if __BANK
				if(minAllowableLength > fConstraintMaxLength)
				{
					Displayf("Constraint being created with length that doesn't meet PH_CONSTRAINT_ASSERT_DEPTH");
				}
				ValidateDummyConstraints(vVehPos,vConstraintPos,fConstraintMaxLength);
				// If validating constraints, add a buffer to the actual physical constraint to allow the vehicle to exist outside of the bounds while the situation is debugged.
				fConstraintMaxLength += (CVehicleAILodManager::ms_bValidateDummyConstraints) ? 10.0f : 0.0f;
#endif

				fConstraintMaxLength = rage::Max(fConstraintMaxLength, minAllowableLength);
				dummyStayOnRoadConstraintParams.maxDistance = fConstraintMaxLength;

				m_dummyStayOnRoadConstraintHandle = PHCONSTRAINT->Insert(dummyStayOnRoadConstraintParams);
				physicsAssertf( m_dummyStayOnRoadConstraintHandle.IsValid(), "Unable to construct the dummy-stay-on-road constraint" );
			}
		}
	}

	//-----------------------------------------------------------------
	// Set up a rotational distance constraint

	if(iNewConstraintsMode == CVehicleAILodManager::DCM_PhysicalDistanceAndRotation)
	{
		if(!m_dummyRotationalConstraint.IsValid())
		{
			static dev_float DUMMY_PITCH_AND_ROLL_LIMIT = 40.0f*DtoR;

			phConstraintCylindrical::Params rotationalConstraint;
			rotationalConstraint.instanceA = GetCurrentPhysicsInst();
			rotationalConstraint.worldAxis = Vec3V(V_Z_AXIS_WZERO);
			rotationalConstraint.maxLimit = DUMMY_PITCH_AND_ROLL_LIMIT;
			m_dummyRotationalConstraint = PHCONSTRAINT->Insert(rotationalConstraint);
		}
	}

	//------------------------------------------------
	// Switch off distance & rotational constraints

	if(iNewConstraintsMode == CVehicleAILodManager::DCM_None)
	{
		RemoveConstraints();
        DetachFromParentAndChildren(DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS);
	}
}

bool CAutomobile::UsesDummyPhysics(const eVehicleDummyMode vdm) const
{
	switch(vdm)
	{
		case VDM_REAL:
			return false;
		case VDM_DUMMY:
		{
			if(GetAttachedTrailer())
				return false;
			return true;
		}
		case VDM_SUPERDUMMY:
			return true;
		default:
			Assert(false);
			return false;
	}
}

void CAutomobile::InitCompositeBound()
{
	CVehicle::InitCompositeBound();

	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());	
	Assert(pBoundComp);

	if(pBoundComp->GetTypeAndIncludeFlags())
	{
		for(int nSiren=VEH_SIREN_GLASS_1; nSiren <= VEH_SIREN_GLASS_MAX; nSiren++)
		{
			if(GetBoneIndex((eHierarchyId)nSiren)!=-1)
			{
				int nChildIndex = GetVehicleFragInst()->GetComponentFromBoneIndex(GetBoneIndex((eHierarchyId)nSiren));
				if(nChildIndex != -1)
				{
					pBoundComp->SetIncludeFlags(nChildIndex, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
				}
			}
		}
	}
}


void CAutomobile::ProcessDriverInputOptions()
{
	//TODO
	// If this vehicle has its siren switched on other cars should get out of the way
/*	if (m_nVehicleFlags.GetIsSirenOn() && ((fwTimer::GetSystemFrameCount() & 7) == 5) && UsesSiren() && GetModelIndex() != MI_CAR_ICEVAN && FindPlayerVehicle() == this)
	{
		CCarAI::MakeWayForCarWithSiren(this);
	}

	//Now done as ped scanning for vehicle threat events.

	// if peds haven't been warned of this cars approach by now - do it!
	if(!m_nVehicleFlags.bWarnedPeds && GetVelocity().Mag2() > 1.0f)
	{
		CCarAI::ScanForPedDanger(this, GetIntelligence()->FindUberMissionForCar() );
	}
*/
}


ePrerenderStatus CAutomobile::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	const bool bIsDummy = IsDummy();

	if(!bIsDummy)
	{
		GetVehicleDamage()->PreRender();
	}

	//////////////////////////////////////////////////////////////////
	// Matrix stuff, modify local skeleton matrices before we do UpdateSkeleton()

	if(!bIsDummy)
	{
		for(int i=0; i<GetNumDoors(); i++)
		{
			GetDoor(i)->ProcessPreRender(this);
		}
	}

	if( GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR )
	{
		CSubmarineCar* pSubmarineCar = static_cast< CSubmarineCar* >( this );
		if( !pSubmarineCar->AreWheelsActive() )
		{
			return CVehicle::PreRender(bIsVisibleInMainViewport);;
		}
	}

#if GTA_REPLAY
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		if((!bIsDummy || !m_nVehicleFlags.bLodFarFromPopCenter))
		{
			if(crSkeleton * pSkeleton = GetSkeleton())
			{
				for(int i=0; i<GetNumWheels(); i++)
				{
					PrefetchObject(m_ppWheels[i]);
				}
				for(int i=0; i<GetNumWheels(); i++)
				{
					if(!m_ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) || GetModelIndex() == MI_TRIKE_RROCKET)
					{
						m_ppWheels[i]->ProcessWheelMatrixForAutomobile(*pSkeleton);
					}
					else
					{
						m_ppWheels[i]->ProcessWheelMatrixForTrike();
					}
				}
			}
		}
	}
		
	ePrerenderStatus status = CVehicle::PreRender(bIsVisibleInMainViewport);

	// Swing arm code for the Raptor vehicle
	if( IsReverseTrike() ) 
	{
		m_nVehicleFlags.bDisableSuperDummy = true;

		CWheel* pWheelR		= GetWheelFromId( BIKE_WHEEL_R );
		int nWheelIndexR	= GetBoneIndex( BIKE_WHEEL_R );
		crSkeleton* pSkel	= GetSkeleton();

		const crBoneData* pBoneDataR = pSkel->GetSkeletonData().GetBoneData( nWheelIndexR );
		const crBoneData* pParentBoneDataR = pBoneDataR->GetParent();

		if( pParentBoneDataR )
		{
			Matrix34& parentBoneMatrixR = RC_MATRIX34(pSkel->GetLocalMtx(pParentBoneDataR->GetIndex()));

			// Reset the parent bone translation (since it gets moved around)
			parentBoneMatrixR.d = VEC3V_TO_VECTOR3(pParentBoneDataR->GetDefaultTranslation());
			parentBoneMatrixR.FromQuaternion( RCC_QUATERNION( pParentBoneDataR->GetDefaultRotation() ) );

			// We will want to rotate the swing arm, so first calculate the correct wheel position (where the wheel should end up)
			Vector3 v3CorrectParentPosition = VEC3V_TO_VECTOR3(pParentBoneDataR->GetDefaultTranslation()) + pWheelR->GetSuspensionAxis() * pWheelR->GetWheelCompressionFromStaticIncBurstAndSink();

			Vector3 vCorrectWheelTranslation = VEC3V_TO_VECTOR3(pBoneDataR->GetDefaultTranslation()) + ( v3CorrectParentPosition - VEC3V_TO_VECTOR3(pParentBoneDataR->GetDefaultTranslation()) );

			// Rotate the swing arm (parent bone of the wheel)
			float fAng = pWheelR->GetWheelCompressionFromStaticIncBurstAndSink() / RCC_VECTOR3( pBoneDataR->GetDefaultTranslation() ).Mag();
			fAng = rage::Asinf( Clamp( -fAng, -1.0f, 1.0f ) );

			parentBoneMatrixR.RotateLocalX( fAng );

			// Figure out where the wheel would end up after the rotation
			Vector3 vRotatedWheelTranslation;

			parentBoneMatrixR.Transform3x3(VEC3V_TO_VECTOR3(pBoneDataR->GetDefaultTranslation()), vRotatedWheelTranslation);

			// Calculate the diff between the correct wheel position and the actual wheel position and move the swing arm so that it matches up
			Vector3 vDiff = vRotatedWheelTranslation - vCorrectWheelTranslation;

			parentBoneMatrixR.d -= vDiff;
		}
	}

	return status;
}

void CAutomobile::PreRender2(const bool bIsVisibleInMainViewport)
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(if (GetHasFragCacheEntry()))
	{
		if (m_StartedAnimDirectorPreRenderUpdate)
		{
			GetAnimDirector()->WaitForPreRenderUpdateToComplete();
			m_StartedAnimDirectorPreRenderUpdate = false;
		}

		// Landing gear deformation update needs to be done after the CVehicle::Prerender and have to update the wheel matrices again as they
		// have dependency with landing gear bones. This mess is caused by that we don't have deformation support for articulated body, once
		// the plane/heli gets deformed, its landing gear bones are out of sync with articulated body. CVehicle::PreRender calls SyncSkeletonToArticulatedBody,
		// which will overwrite the skeleton to the articulated body matrices. Therefore the landing gear deformation has to be applied after.
	#if GTA_REPLAY
		if(!CReplayMgr::IsEditModeActive())
	#endif
		{
			if( ( InheritsFromPlane() || (InheritsFromHeli() && static_cast<CHeli*>(this)->HasLandingGear())) &&
                !IsAnimated() )
			{
				if(InheritsFromPlane())
				{
					static_cast<CPlane*>(this)->GetLandingGear().PreRenderWheels(this);
				}
				else
				{
					static_cast<CHeli*>(this)->GetLandingGear().PreRenderWheels(this);
				}

				// This hacky code here is caused by the update order conflict. The wheels matrices need to be processed before CVehicle::PreRender,
				// and landing gear update has to be done after the CVehicle::PreRender, and then wheels bones have dependency with landing gear bones.
				// So here we have to recompute the wheel matrices again...
				if((!IsDummy() || !m_nVehicleFlags.bLodFarFromPopCenter))
				{
					if(crSkeleton * pSkeleton = GetSkeleton())
					{
						for(int i=0; i<GetNumWheels(); i++)
						{
							PrefetchObject(m_ppWheels[i]);
						}
						for(int i=0; i<GetNumWheels(); i++)
						{
							m_ppWheels[i]->ProcessWheelMatrixForAutomobile(*pSkeleton);
						}
					}
				}

			}
		}

		// Process door deformation here in PreRender2(), since we need animations to be done 
		// to do the car door deformation. Previously it was in PreRender(), immediately after 
		// we started the vehicles's PreRender animation update, which would cause a guaranteed 
		// block on the main thread as we waited for the animation we just launched to complete.
		// Doing this in PreRender2() gives plenty of time for the animation to complete.
		if(bIsVisibleInMainViewport && !IsDummy())
		{
			for(int i=0; i<GetNumDoors(); i++)
			{
				CCarDoor* door = GetDoor(i);
				if (door->GetFlag(CCarDoor::IS_BROKEN_OFF))
					continue;

				door->ProcessPreRender2(this);
			}
		}
		// If you change the way this condition for the fake body movement works, please update CVfxHelper::GetDefaultToWorldMatrix as well!
 		if(bIsVisibleInMainViewport && !IsDummy() &&
			m_specialFlightModeRatio < 1.0f )
		{
			if(!camInterface::GetCinematicDirector().IsRenderingCinematicCameraInsideVehicle() && !camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera())
			{
				if( (GetDriver() && GetDriver()->IsPlayer()) || 
					(GetLastDriver() && GetLastDriver()->IsPlayer()) || 
					(GetVehicleAudioEntity() && GetVehicleAudioEntity()->IsInCarModShop()) ) 
				{
				REPLAY_ONLY(if(CReplayMgr::IsEditModeActive() == false))
					ProcessFakeBodyMovement(fwTimer::IsGamePaused());
				}
			}
			PreRenderSuspensionMod(m_fFakeSuspensionLowerAmount);

			m_fFakeSuspensionLowerAmountApplied = m_fFakeSuspensionLowerAmount;
		}

		if(bIsVisibleInMainViewport && !IsDummy())
		{
			if(crSkeleton * pSkeleton = GetSkeleton())
			{
				for(int i=0; i<GetNumWheels(); i++)
				{
					PrefetchObject(m_ppWheels[i]);
				}
				for(int i=0; i<GetNumWheels(); i++)
				{
					m_ppWheels[i]->ProcessAnimatedWheelDeformationForAutomobile(*pSkeleton);
				}
			}
		}

		// ground disturbance vfx
		if (MI_CAR_DELUXO.IsValid() && GetModelIndex()==MI_CAR_DELUXO)
		{
			if (m_nVehicleFlags.bEngineOn || IsRunningCarRecording())
			{
				if (GetSpecialFlightModeRatio()==1.0f)
				{
					g_vfxVehicle.UpdatePtFxGlideHaze(this, ATSTRINGHASH("veh_xm_deluxo_glide_haze", 0x5C7B51B9));
					g_vfxVehicle.UpdatePtFxPlaneAfterburner(this, VEH_ROCKET_BOOST, 0);
					g_vfxVehicle.ProcessGroundDisturb(this);
				}
			}
		}

		// Call this after the door deformation to make sure any broken glass stays hidden (ProcessPreRenderDeformation restores the matrix for the window)
		CVehicle::PreRender2(bIsVisibleInMainViewport);
	}

	u32 iFlags = 0;
	if( m_nVehicleFlags.bForceBrakeLightOn )
	{
		iFlags |= LIGHTS_FORCE_BRAKE_LIGHTS;
	}

	if( m_bInteriorLightForceOn )
	{
		iFlags |= LIGHTS_FORCE_INTERIOR_LIGHT;
	}

	Vec4V ambientVolumeParams = Vec4V(V_ZERO_WONE);

	// lower AO Volume for selected vehicles:
	const u32 vehNameHash = GetBaseModelInfo()->GetModelNameHash();

	if (vehNameHash == MID_ASBO)
	{
		ambientVolumeParams = Vec4V(Asbo_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ARDENT)
	{
		ambientVolumeParams = Vec4V(Ardent_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_BRIOSO2)
	{
		ambientVolumeParams = Vec4V(Brioso2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_BRIOSO3)
	{
		ambientVolumeParams = Vec4V(Brioso3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_CHAMPION)
	{
		ambientVolumeParams = Vec4V(Champion_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_CHEETAH2)
	{
		ambientVolumeParams = Vec4V(Cheetah2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_CLIQUE)
	{
		ambientVolumeParams = Vec4V(Clique_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_COMET4)
	{
		ambientVolumeParams = Vec4V(Comet4_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_COMET6)
	{
		ambientVolumeParams = Vec4V(Comet6_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_COMET7)
	{
		ambientVolumeParams = Vec4V(Comet7_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_COQUETTE4)
	{
		ambientVolumeParams = Vec4V(Coquette4_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_CORSITA)
	{
		ambientVolumeParams = Vec4V(Corsita_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_CYCLONE2)
	{
		ambientVolumeParams = Vec4V(Cyclone2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_CYPHER)
	{
		ambientVolumeParams = Vec4V(Cypher_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DEVESTE)
	{
		ambientVolumeParams = Vec4V(Deveste_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DOMINATOR8)
	{
		ambientVolumeParams = Vec4V(Dominator8_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DRAFTER)
	{
		ambientVolumeParams = Vec4V(Drafter_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DRAUGUR)
	{
		ambientVolumeParams = Vec4V(Draugur_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DUNE4)
	{
		ambientVolumeParams = Vec4V(Dune4_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DUNE5)
	{
		ambientVolumeParams = Vec4V(Dune5_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DUKES3)
	{
		ambientVolumeParams = Vec4V(Dukes3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_DYNASTY)
	{
		ambientVolumeParams = Vec4V(Dynasty_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ELLIE)
	{
		ambientVolumeParams = Vec4V(Ellie_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_EUROS)
	{
		ambientVolumeParams = Vec4V(Euros_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_FLASHGT)
	{
		ambientVolumeParams = Vec4V(FlashGT_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_FMJ)
	{
		ambientVolumeParams = Vec4V(Fmj_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_FORMULA)
	{
		ambientVolumeParams = Vec4V(Formula_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_FURIA)
	{
		ambientVolumeParams = Vec4V(Furia_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_IGNUS)
	{
		ambientVolumeParams = Vec4V(Ignus_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_IGNUS2)
	{
		ambientVolumeParams = Vec4V(Ignus2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_GB200)
	{
		ambientVolumeParams = Vec4V(GB200_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_GP1)
	{
		ambientVolumeParams = Vec4V(GP1_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_GAUNTLET4)
	{
		ambientVolumeParams = Vec4V(Gauntlet4_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_GAUNTLET5)
	{
		ambientVolumeParams = Vec4V(Gauntlet5_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_GROWLER)
	{
		ambientVolumeParams = Vec4V(Growler_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_HOTRING)
	{
		ambientVolumeParams = Vec4V(Hotring_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_HUSTLER)
	{
		ambientVolumeParams = Vec4V(Hustler_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_IMORGON)
	{
		ambientVolumeParams = Vec4V(Imorgon_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_INFERNUS2)
	{
		ambientVolumeParams = Vec4V(Infernus2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ISSI3)
	{
		ambientVolumeParams = Vec4V(Issi3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ISSI7)
	{
		ambientVolumeParams = Vec4V(Issi7_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ITALIGTO)
	{
		ambientVolumeParams = Vec4V(Italigto_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ITALIRSX)
	{
		ambientVolumeParams = Vec4V(ItaliRsx_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_JESTER3)
	{
		ambientVolumeParams = Vec4V(Jester3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_JESTER4)
	{
		ambientVolumeParams = Vec4V(Jester4_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_JUBILEE)
	{
		ambientVolumeParams = Vec4V(Jubilee_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_KOMODA)
	{
		ambientVolumeParams = Vec4V(Komoda_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_KRIEGER)
	{
		ambientVolumeParams = Vec4V(Krieger_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_MANANA2)
	{
		ambientVolumeParams = Vec4V(Manana2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_MENACER)
	{
		ambientVolumeParams = Vec4V(Menacer_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_NEO)
	{
		ambientVolumeParams = Vec4V(Neo_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_NEON)
	{
		ambientVolumeParams = Vec4V(Neon_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_NERO2)
	{
		ambientVolumeParams = Vec4V(Nero2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_OPENWHEEL1)
	{
		ambientVolumeParams = Vec4V(Openwheel1_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_OPENWHEEL2)
	{
		ambientVolumeParams = Vec4V(Openwheel2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_PARAGON)
	{
		ambientVolumeParams = Vec4V(Paragon_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_PENETRATOR)
	{
		ambientVolumeParams = Vec4V(Penetrator_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_PENUMBRA2)
	{
		ambientVolumeParams = Vec4V(Penumbra2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_PEYOTE3)
	{
		ambientVolumeParams = Vec4V(Peyote3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_POLBUFFALO)
	{
		ambientVolumeParams = Vec4V(Polbuffalo_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_POLGREENWOOD)
	{
		ambientVolumeParams = Vec4V(Polgreenwood_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_RAPIDGT3)
	{
		ambientVolumeParams = Vec4V(RapidGT3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_REMUS)
	{
		ambientVolumeParams = Vec4V(Remus_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_RT3000)
	{
		ambientVolumeParams = Vec4V(RT3000_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_RUSTON)
	{
		ambientVolumeParams = Vec4V(Ruston_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_S80)
	{
		ambientVolumeParams = Vec4V(S80_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_SC1)
	{
		ambientVolumeParams = Vec4V(SC1_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_SEASPARROW2)
	{
		ambientVolumeParams = Vec4V(Seasparrow2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_SCHLAGEN)
	{
		ambientVolumeParams = Vec4V(Schlagen_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_SM722)
	{
		ambientVolumeParams = Vec4V(SM722_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_STAFFORD)
	{
		ambientVolumeParams = Vec4V(Stafford_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_STROMBERG)
	{
		ambientVolumeParams = Vec4V(Stromberg_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_SWINGER)
	{
		ambientVolumeParams = Vec4V(Swinger_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_SUGOI)
	{
		ambientVolumeParams = Vec4V(Sugoi_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_TAILGATER2)
	{
		ambientVolumeParams = Vec4V(Tailgater2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_TAMPA3)
	{
		ambientVolumeParams = Vec4V(Tampa3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_THRAX)
	{
		ambientVolumeParams = Vec4V(Thrax_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_TIGON)
	{
		ambientVolumeParams = Vec4V(Tigon_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_TOREADOR)
	{
		ambientVolumeParams = Vec4V(Toreador_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_TORERO2)
	{
		ambientVolumeParams = Vec4V(Torero2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_TYRANT)
	{
		ambientVolumeParams = Vec4V(Tyrant_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_VAGNER)
	{
		ambientVolumeParams = Vec4V(Vagner_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_VAGRANT)
	{
		ambientVolumeParams = Vec4V(Vagrant_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_VERUS)
	{
		ambientVolumeParams = Vec4V(Verus_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_VETO2)
	{
		ambientVolumeParams = Vec4V(Veto2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_VOODOO)
	{
		ambientVolumeParams = Vec4V(Voodoo_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_VSTR)
	{
		ambientVolumeParams = Vec4V(Vstr_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_WARRENER2)
	{
		ambientVolumeParams = Vec4V(Warrener2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_WEEVIL)
	{
		ambientVolumeParams = Vec4V(Weevil_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_WEEVIL2)
	{
		ambientVolumeParams = Vec4V(Weevil2_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_XA21)
	{
		ambientVolumeParams = Vec4V(XA21_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_YOUGA3)
	{
		ambientVolumeParams = Vec4V(Youga3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ZENO)
	{
		ambientVolumeParams = Vec4V(Zeno_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ZION3)
	{
		ambientVolumeParams = Vec4V(Zion3_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ZORRUSSO)
	{
		ambientVolumeParams = Vec4V(Zorrusso_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ZR350)
	{
		ambientVolumeParams = Vec4V(ZR350_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ZR3802)
	{
		ambientVolumeParams = Vec4V(ZR3802_AmbientVolume_Offset, ScalarV(V_ONE));
	}
	else if (vehNameHash == MID_ZR3803)
	{
		ambientVolumeParams = Vec4V(ZR3803_AmbientVolume_Offset, ScalarV(V_ONE));
	}

	DoVehicleLights(iFlags, ambientVolumeParams);

	// Sets the indicator bits in the Vehicle flags straight after they have been queried for the lights.
	m_nVehicleFlags.bForceBrakeLightOn = false;
}


void CAutomobile::ProcessSuspensionTransforms() 
{
	const bool bIsDummy = IsDummy();
#if GTA_REPLAY
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		if(!fwTimer::IsGamePaused() && !bIsDummy && !m_nVehicleFlags.bAnimateWheels && GetIsVisibleInSomeViewportThisFrame())
		{
			SuspensionType nType = GetSuspensionType(false);

            bool bIsMiniTank = GetModelIndex() == MI_CAR_MINITANK.GetModelIndex();
            if( bIsMiniTank )
            {
                ProcessSuspensionTransformsForMiniTank();
                return; 
            }

			if(!IsTrike())
			{
				SetSuspensionTransformation(VEH_SUSPENSION_LF, VEH_WHEEL_LF, nType, &RCC_VEC3V(m_vSuspensionDeformationLF));
				SetSuspensionTransformation(VEH_SUSPENSION_RF, VEH_WHEEL_RF, nType, &RCC_VEC3V(m_vSuspensionDeformationRF));

				//All vehicles with springs currently use Sus_Solid (squashing type) (Quadbikes, RC Cars)
				//If this changes we'll need to move some model flags around to support it as they're currently full
				SetSuspensionTransformation(VEH_SPRING_LF, VEH_WHEEL_LF, Sus_Solid);
				SetSuspensionTransformation(VEH_SPRING_RF, VEH_WHEEL_RF, Sus_Solid);
			}
			else if (GetBoneIndex(VEH_TRANSMISSION_F) != -1)
			{
				SetTransmissionRotationForTrike(VEH_TRANSMISSION_F, VEH_WHEEL_LF);
			}

			nType = GetSuspensionType(true);
			
			SetSuspensionTransformation(VEH_SUSPENSION_LR, VEH_WHEEL_LR, nType);
			SetSuspensionTransformation(VEH_SUSPENSION_RR, VEH_WHEEL_RR, nType);

			if( GetBoneIndex( VEH_TRANSMISSION_M1 ) == -1 )
			{
				SetSuspensionTransformation(VEH_SUSPENSION_LM, VEH_WHEEL_LM1, nType);
				SetSuspensionTransformation(VEH_SUSPENSION_RM, VEH_WHEEL_RM1, nType);	
			}
			else
			{
				SetSuspensionTransformation(VEH_SUSPENSION_LM1, VEH_WHEEL_LM1, nType);
				SetSuspensionTransformation(VEH_SUSPENSION_RM1, VEH_WHEEL_RM1, nType);	
				SetSuspensionTransformation(VEH_SUSPENSION_LM, VEH_WHEEL_LM2, nType);
				SetSuspensionTransformation(VEH_SUSPENSION_RM, VEH_WHEEL_RM2, nType);
			}

			SetSuspensionTransformation(VEH_SPRING_LR, VEH_WHEEL_LR, Sus_Solid);
			SetSuspensionTransformation(VEH_SPRING_RR, VEH_WHEEL_RR, Sus_Solid);

			//Trailers can be setup without front wheels and multiple middle/rear wheels
			if(GetBoneIndex(VEH_WHEEL_LF) != -1 && GetBoneIndex(VEH_WHEEL_RF) != -1)
			{
				SetTransmissionRotation(VEH_TRANSMISSION_F, VEH_WHEEL_LF, VEH_WHEEL_RF);

				if( GetBoneIndex( VEH_TRANSMISSION_M1 ) == -1 )
				{
					SetTransmissionRotation(VEH_TRANSMISSION_M, VEH_WHEEL_LM1, VEH_WHEEL_RM1);
				}
				else
				{
					SetTransmissionRotation(VEH_TRANSMISSION_M1, VEH_WHEEL_LM1, VEH_WHEEL_RM1);
					SetTransmissionRotation(VEH_TRANSMISSION_M, VEH_WHEEL_LM2, VEH_WHEEL_RM2);
				}
			}
			else
			{
				if(GetBoneIndex(VEH_WHEEL_LM2) != -1 && GetBoneIndex(VEH_WHEEL_RM2) != -1)
				{	
					SetTransmissionRotation(VEH_TRANSMISSION_F, VEH_WHEEL_LM1, VEH_WHEEL_RM1);
					SetTransmissionRotation(VEH_TRANSMISSION_M, VEH_WHEEL_LM2, VEH_WHEEL_RM2);
				}
				else
				{
					SetTransmissionRotation(VEH_TRANSMISSION_F, VEH_WHEEL_LM1, VEH_WHEEL_RM1);
					SetTransmissionRotation(VEH_TRANSMISSION_M, VEH_WHEEL_LM1, VEH_WHEEL_RM1);
				}
			}

			SetTransmissionRotation(VEH_TRANSMISSION_R, VEH_WHEEL_LR, VEH_WHEEL_RR);
		}
	}
}

void CAutomobile::ProcessSuspensionTransformsForMiniTank()
{
    if( crSkeleton* pSkeleton = GetSkeleton() )
    {
        u16 trackHash = atHash16U( "track" );

        int boneIdx = -1;
        pSkeleton->GetSkeletonData().ConvertBoneIdToIndex( trackHash, boneIdx );
        eHierarchyId miscBoneHierachyIds[ 4 ] = { VEH_MISC_A, VEH_MISC_C, VEH_MISC_B, VEH_MISC_D };
        eHierarchyId wheelBoneHierachyIds[ 4 ] = { VEH_WHEEL_LF, VEH_WHEEL_RF, VEH_WHEEL_LR, VEH_WHEEL_RR };

        Vector3 wheelPosnL = RCC_VECTOR3( pSkeleton->GetSkeletonData().GetBoneData( boneIdx )->GetDefaultTranslation() );
        Matrix34 &wheelMatL = RC_MATRIX34( pSkeleton->GetLocalMtx( boneIdx ) );
        Vector3 wheelPositions[ 4 ];
        float averageSuspensionLength = 0.0f;
        static dev_float sfWheelOffsetToGround = 0.08f;

        for( int i = 0; i < 4; i++ )
        {
            GetWheel( i )->GetWheelPosAndRadius( wheelPositions[ i ] );
            wheelPositions[ i ] = VEC3V_TO_VECTOR3( GetTransform().UnTransform( VECTOR3_TO_VEC3V( wheelPositions[ i ] ) ) );
            wheelPositions[ i ].SetZ( wheelPositions[ i ].GetZ() - ( GetWheel( i )->GetWheelRadius() ) );

            float suspensionCompression = GetWheel( i )->GetWheelCompressionFromStaticIncBurstAndSink() - ( GetWheel( i )->GetWheelImpactOffset() + sfWheelOffsetToGround );
            averageSuspensionLength += suspensionCompression;

            int nWheelBoneIndex = GetBoneIndex( wheelBoneHierachyIds[ i ] );
            int nMiscBoneIndex = GetBoneIndex( wheelBoneHierachyIds[ i ] );
            Vector3 defaultWheelPos = VEC3V_TO_VECTOR3( pSkeleton->GetSkeletonData().GetBoneData( nWheelBoneIndex )->GetDefaultTranslation() );
            Vector3 defaultMiscBonePos = VEC3V_TO_VECTOR3( pSkeleton->GetSkeletonData().GetBoneData( nMiscBoneIndex )->GetDefaultTranslation() );

            float zDiffBetweenWheelAndMiscBone = defaultWheelPos.GetZ() - defaultMiscBonePos.GetZ();
            Vector3 wheelOffset( 0.0f, 0.0f, suspensionCompression - zDiffBetweenWheelAndMiscBone );
            SetComponentRotation( miscBoneHierachyIds[ i ], ROT_AXIS_LOCAL_X, GetWheel( i )->GetRotAngle(), true, NULL, &wheelOffset );
        }
        averageSuspensionLength *= 0.25f;

        Vector3 suspensionOffset = Vector3( 0.0f, 0.0f, averageSuspensionLength );

        Vector3 vecForward = ( ( wheelPositions[ 0 ] + wheelPositions[ 1 ] ) - ( wheelPositions[ 2 ] + wheelPositions[ 3 ] ) ) * 0.5f;
        Vector3 vecRight = ( ( wheelPositions[ 1 ] + wheelPositions[ 3 ] ) - ( wheelPositions[ 0 ] + wheelPositions[ 2 ] ) ) * 0.5f;
        Vector3 vecUp = vecRight;
            
        vecUp.Cross( vecForward );

        vecForward.Normalize();
        vecRight.Normalize();
        vecUp.Normalize();

        wheelMatL.a = vecRight;
        wheelMatL.b = vecForward;
        wheelMatL.c = vecUp;
        wheelMatL.d = wheelPosnL + suspensionOffset;
    }
}

void CAutomobile::PreRenderSuspensionMod(const float fFakeSuspensionLowerAmount)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif //GTA_REPLAY

	if(GetVehicleType() == VEHICLE_TYPE_CAR && fFakeSuspensionLowerAmount != 0.0f)
	{
		crSkeleton* skel = GetSkeleton();
		const u32 boneCount = skel->GetBoneCount();
		int wheelCount = GetNumWheels();
		int hubCount = VEH_WHEELHUB_RM3 - VEH_WHEELHUB_LF + 1;

		static bool bIgnoreLists [VEH_NUM_NODES];
		sysMemSet(bIgnoreLists, 0, sizeof(bool) * boneCount);

		// Make sure we don't modify the wheel matrices
		for (int k=0; k<wheelCount; k++)
		{
			if(GetWheel(k))
			{
				int iBone = GetBoneIndex(GetWheel(k)->GetHierarchyId());
				if(iBone >= 0)
				{
					bIgnoreLists [iBone] = true;
				}
			}
		}

		// ... or broken off doors
		for (s32 f = 0; f < GetNumDoors(); ++f)
		{
			CCarDoor* door = GetDoor(f);
			if (door->GetFlag(CCarDoor::IS_BROKEN_OFF) && door->GetBoneIndex() >= 0)
			{
				bIgnoreLists [door->GetBoneIndex()] = true;
			}
		}

		for (int k=0; k<hubCount; k++)
		{
			eHierarchyId boneId = (eHierarchyId) (k + VEH_WHEELHUB_LF);
			if (GetBoneIndex(boneId) >= 0)
			{
				bIgnoreLists [GetBoneIndex(boneId)] = true;
			}
		}


		for(int i = 0; i < boneCount; i++)
		{
			if (bIgnoreLists[i])
				continue;

			Matrix34& boneMatrix1 = RC_MATRIX34(skel->GetObjectMtx(i));
			boneMatrix1.Translate(0.0f, 0.0f, -fFakeSuspensionLowerAmount);
		}
	}
}

static dev_float velToStartFakeBodyMovement = 10.0f;
static dev_float velToEndFakeBodyMovement = 40.0f;

static dev_float sfRandomChanceToBump = 1.5f;

static dev_float sfBumpSeverityY = 0.02f;
//static dev_float sfBumpSeverityX = 0.0125f;

static dev_float sfBumpRateStart = 10.0f;
static dev_float sfBumpRateStop = 20.0f;

static dev_float sfBumpTimeStart = 0.2f;
static dev_float sfBumpTimeStop = 1.8f;
static dev_float sfBumpRandomNoiceScale = 0.5f;

static dev_float sfEngineRevSeverityY = 0.02f;
static dev_float sfEngineRevBumpSeverityY = 0.008f;
static dev_float sfEngineRevBumpRateStart = 50.0f;
static dev_float sfEngineRevBumpRateStop = 150.0f;
static dev_float sfEngineRevBumpTimeStart = 0.35f;
static dev_float sfEngineRevBumpTimeStop = 0.6f;
static dev_float sfEngineRevRandomNoiceScale = 0.0f;
static dev_float sfEngineRevMovementStart = 0.25f;

void CAutomobile::ProcessFakeBodyMovement(bool bIsGamePaused)
{
	if(bIsGamePaused)
	{
		return;
	}

	TUNE_GROUP_BOOL(TURRET_TUNE, DISABLE_FAKE_BODY_MOVEMENT_FOR_LOCAL_PLAYER_IN_TURRET_SEAT, true);
	// B*2150619
	if (DISABLE_FAKE_BODY_MOVEMENT_FOR_LOCAL_PLAYER_IN_TURRET_SEAT)
	{
		if (GetVehicleModelInfo() && GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
		{
			const CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (pPlayer && pPlayer->GetIsInVehicle() && pPlayer->GetMyVehicle() == this)
			{
				const s32 iPlayersSeatIndex = GetSeatManager()->GetPedsSeatIndex(pPlayer);
				if (IsTurretSeat(iPlayersSeatIndex))
				{
					return;
				}
			}
		}
	}

    if(GetVehicleType() == VEHICLE_TYPE_CAR)
    {
        float speed = GetVelocityIncludingReferenceFrame().Mag();

        float velModifier = (speed - velToStartFakeBodyMovement) / (velToEndFakeBodyMovement - velToStartFakeBodyMovement);

		velModifier = Clamp(velModifier, 0.0f, 1.0f);

		const float fRevRatio = m_Transmission.GetRevRatio();
		float fRevModifier = (fRevRatio - sfEngineRevMovementStart) / (1.0f - sfEngineRevMovementStart);
		float fRevRotationBase = 0.0f;
		float fRandomNoise = 0.0f;
		bool bShouldPlayEngineRev = GetIsStatic() && (GetStatus()==STATUS_PLAYER) && (fRevRatio > sfEngineRevMovementStart);

        // if we're going fast enough then add a bump
        if (speed > velToStartFakeBodyMovement)
        {
            //make sure were not already doing a bump
            if( m_fHighSpeedYTime <= 0.0f && fwRandom::GetRandomNumberInRange(0.0f,1.0f) < sfRandomChanceToBump * velModifier)
            {
                m_fBumpSeverityY = fwRandom::GetRandomNumberInRange( 0.0f, sfBumpSeverityY * velModifier * ms_fBumpSeverityMulti);
                m_fBumpRateY = fwRandom::GetRandomNumberInRange( sfBumpRateStart, sfBumpRateStop );
                m_fHighSpeedYTime = fwRandom::GetRandomNumberInRange( sfBumpTimeStart, sfBumpTimeStop ); 

                m_fBumpSeverityYDecay = m_fBumpSeverityY/m_fHighSpeedYTime;

                // do we want to bump from the left or right?
                if(fwRandom::GetRandomTrueFalse())
                {
                    m_fHighSpeedRotationY = 2.0f*PI;
                    m_fHighSpeedDirectionY = -1.0f;
                }
                else
                {
                    m_fHighSpeedRotationY = 0.0f;
                    m_fHighSpeedDirectionY = 1.0f;
                }
            }
			fRandomNoise = rage::Sinf(fwRandom::GetRandomNumberInRange( -PI, PI )) * sfBumpRandomNoiceScale;
        }
		// if engine revs while car sits still
		else if(bShouldPlayEngineRev)
		{
			//make sure were not already doing a bump
			if( m_fHighSpeedYTime <= 0.0f && fwRandom::GetRandomNumberInRange(0.0f,1.0f) < sfRandomChanceToBump * fRevModifier)
			{
				m_fBumpSeverityY = fwRandom::GetRandomNumberInRange( 0.0f, sfEngineRevBumpSeverityY * fRevModifier);
				m_fBumpRateY = fwRandom::GetRandomNumberInRange( sfEngineRevBumpRateStart, sfEngineRevBumpRateStop );
				m_fHighSpeedYTime = fwRandom::GetRandomNumberInRange( sfEngineRevBumpTimeStart, sfEngineRevBumpTimeStop ); 

				m_fBumpSeverityYDecay = m_fBumpSeverityY/m_fHighSpeedYTime;

				// do we want to bump from the left or right?
				if(fwRandom::GetRandomTrueFalse())
				{
					m_fHighSpeedRotationY = 2.0f*PI;
					m_fHighSpeedDirectionY = -1.0f;
				}
				else
				{
					m_fHighSpeedRotationY = 0.0f;
					m_fHighSpeedDirectionY = 1.0f;
				}
			}

			fRevRotationBase = sfEngineRevSeverityY * fRevModifier;
			fRandomNoise = rage::Sinf(fwRandom::GetRandomNumberInRange( -PI, PI )) * sfEngineRevRandomNoiceScale;
		}
        
        if(m_fHighSpeedYTime > 0.0f)
        {     
            m_fHighSpeedYTime -= fwTimer::GetTimeStep();

            m_fBumpSeverityY -= m_fBumpSeverityYDecay * fwTimer::GetTimeStep();
            if(m_fBumpSeverityY < 0.0f)
                m_fBumpSeverityY = 0.0f;

            m_fHighSpeedRotationY += (m_fBumpRateY * fwTimer::GetTimeStep()) * m_fHighSpeedDirectionY;
            if(m_fHighSpeedRotationY > 2.0f*PI)
            {
                m_fHighSpeedRotationY = 0.0f;
            }
            else if(m_fHighSpeedRotationY < 0.0f)
            {
                m_fHighSpeedRotationY = 2.0f*PI;
            }
        }
        else
        {
            m_fHighSpeedRotationY = 0.0f;
        }

		bool bRotationY = m_fHighSpeedRotationY > 0.0f || fRevRotationBase > 0.0f;

		if (!bRotationY)
			return;

        int wheelCount = VEH_WHEEL_LAST_WHEEL - VEH_WHEEL_FIRST_WHEEL + 1;
        int hubCount = VEH_WHEELHUB_RM3 - VEH_WHEELHUB_LF + 1;

		m_fFakeRotationY = 0.f;
		if (bRotationY)
		{
			m_fFakeRotationY = (rage::Sinf(m_fHighSpeedRotationY) + fRandomNoise) * m_fBumpSeverityY;
		}
		m_fFakeRotationY += fRevRotationBase;

		crSkeleton* skel = GetSkeleton();
		const u32 boneCount = skel->GetBoneCount();
        for(int i = 0; i < boneCount; i++)
        {
            bool bIgnoreBone = false;

            // Make sure we don't modify the wheel matrices
            for (int k=0; k<wheelCount; k++)
            {
                eHierarchyId boneId = (eHierarchyId) (k + VEH_WHEEL_FIRST_WHEEL);
                if (GetBoneIndex(boneId) == i)
                {
                    bIgnoreBone = true;
					break;
                }
            }

			// ... or broken off doors
			for (s32 f = 0; f < GetNumDoors(); ++f)
			{
				CCarDoor* door = GetDoor(f);
				if (door->GetFlag(CCarDoor::IS_BROKEN_OFF) && door->GetBoneIndex() == i)
				{
					bIgnoreBone = true;
					break;
				}
			}

			if (bIgnoreBone)
				continue;

            for (int k=0; k<hubCount; k++)
            {
                eHierarchyId boneId = (eHierarchyId) (k + VEH_WHEELHUB_LF);
                if (GetBoneIndex(boneId) == i)
                {
                    bIgnoreBone = true;
					break;
                }
            }

			for( int k = (int)VEH_TURRET_FIRST_MOD; k < (int)VEH_TURRET_LAST_MOD; k++ )
			{
				eHierarchyId boneId = (eHierarchyId) (k);

				if (GetBoneIndex(boneId) == i)
				{
					bIgnoreBone = true;
					break;
				}
			}

			for( int k = (int)VEH_TURRET_A_FIRST_MOD; k < (int)VEH_TURRET_A_LAST_MOD; k++ )
			{
				eHierarchyId boneId = (eHierarchyId) (k);

				if (GetBoneIndex(boneId) == i)
				{
					bIgnoreBone = true;
					break;
				}
			}

			for( int k = (int)VEH_TURRET_B_FIRST_MOD; k < (int)VEH_TURRET_B_LAST_MOD; k++ )
			{
				eHierarchyId boneId = (eHierarchyId) (k);

				if (GetBoneIndex(boneId) == i)
				{
					bIgnoreBone = true;
					break;
				}
			}
            if(bIgnoreBone == false)
            {
                Matrix34& boneMatrix1 = RC_MATRIX34(skel->GetObjectMtx(i));

                if(bRotationY)
                {
                    boneMatrix1.RotateFullY(m_fFakeRotationY);
                }
            }
        }
    }
}

dev_float dfWheelPoppingSteeringBiasMaxSpeed = 16.67f; //about 60km/h

void CAutomobile::NotifyWheelPopped(CWheel *pWheel)
{
	float fBiasMulti = Clamp(GetVelocityIncludingReferenceFrame().Mag() / dfWheelPoppingSteeringBiasMaxSpeed, 0.0f, 1.0f);
	fBiasMulti *= m_fSteeringBiasScalar;
	Vector3 vOffset = CWheel::GetWheelOffset(GetVehicleModelInfo(), pWheel->GetHierarchyId());
	bool bIsLeftWheel = vOffset.x > 0.0f;
	float fSteeringBias = bfSteeringBiasWhenWheelPopped * DtoR * (bIsLeftWheel ? 1.0f : -1.0f) * fBiasMulti;
	float fSteeringBiasLife = bfSteeringBiasLifeAfterWheelPopped * fBiasMulti;

	ApplySteeringBias(fSteeringBias, fSteeringBiasLife);
}

void CAutomobile::ApplySteeringBias(float bias, float lifetime)
{
	m_fSteeringBias = bias;
	m_fSteeringBiasLife = lifetime;
}

#define NUM_FX_BONES (22)
static eHierarchyId aFxBones[NUM_FX_BONES] =
{
	VEH_EXHAUST,		// 0
	VEH_EXHAUST_2,		// 1
	VEH_EXHAUST_3,		// 2
	VEH_EXHAUST_4,		// 3
	VEH_EXHAUST_5,		// 4
	VEH_EXHAUST_6,		// 5
	VEH_EXHAUST_7,		// 6
	VEH_EXHAUST_8,		// 7
	VEH_EXHAUST_9,		// 8
	VEH_EXHAUST_10,		// 9
	VEH_EXHAUST_11,		// 10
	VEH_EXHAUST_12,		// 11
	VEH_EXHAUST_13,		// 12
	VEH_EXHAUST_14,		// 13
	VEH_EXHAUST_15,		// 14
	VEH_EXHAUST_16,		// 15
	VEH_OVERHEAT,		// 16
	VEH_OVERHEAT_2,		// 17
	VEH_ENGINE,			// 18
	VEH_PETROLCAP,		// 19
	VEH_TOW_MOUNT_A,	// 20
	VEH_TOW_MOUNT_B,	// 21
};

void CAutomobile::ApplyDeformationToBones(const void* basePtr)
{
	for(int i=0; i<GetNumDoors(); i++)
	{
		GetDoor(i)->ApplyDeformation(this, basePtr);
	}

	GetVariationInstance().ApplyDeformation(this, basePtr);

	if(basePtr)
	{
		// FXs : we deform those FX nodes so that smokes, flames and other details will come from the right point.
		for(int i=0; i<NUM_FX_BONES; i++)
		{
			// GTA V hack - If the vehicle has rocket boost, do not deform the exhaust bone since the booster is not skinned to the exhaust bone
			// but the VFX is
			if(HasRocketBoost() && aFxBones[i] == VEH_EXHAUST)
			{
				continue;
			}

			int boneIdx = GetBoneIndex(aFxBones[i]);
			if( boneIdx != -1 )
			{
				Vector3 vertPos;
				GetDefaultBonePositionSimple(boneIdx,vertPos);

				Vector3 damageLocal = VEC3V_TO_VECTOR3(GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vertPos)));
				int nParentBoneIndex = GetSkeletonData().GetBoneData(boneIdx)->GetParent()->GetIndex();
				if(nParentBoneIndex != -1)
				{
					const Matrix34 &matParentBone = GetObjectMtx(nParentBoneIndex);
					matParentBone.UnTransform3x3(damageLocal);
				}

				Matrix34 &mtxLocal = GetLocalMtxNonConst(boneIdx);
				mtxLocal.d = VEC3V_TO_VECTOR3(GetSkeletonData().GetBoneData(boneIdx)->GetDefaultTranslation()) + damageLocal;
			}
		}

		// Loop through all the attached stick bombs and update their positions
		if(fwAttachmentEntityExtension *extension = GetAttachmentExtension())
		{
			CPhysical* pCurChild = static_cast<CPhysical*>(extension->GetChildAttachment());
			while(pCurChild)
			{
				if(pCurChild->GetIsTypeObject())
				{
					if(CProjectile *pProjectile = ((CObject *)pCurChild)->GetAsProjectile())
					{
						pProjectile->ApplyDeformation(this, basePtr);
					}
				}
				fwAttachmentEntityExtension &curChildAttachExt = pCurChild->GetAttachmentExtensionRef();
				pCurChild = static_cast<CPhysical*>(curChildAttachExt.GetSiblingAttachment());
			}
		}

		int iBone = GetBoneIndex(VEH_SUSPENSION_LF);
		if(iBone != -1)
		{
			Vector3 vertPos;
			GetDefaultBonePositionSimple(iBone,vertPos);
			m_vSuspensionDeformationLF = VEC3V_TO_VECTOR3(GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vertPos)));
		}

		iBone = GetBoneIndex(VEH_SUSPENSION_RF);
		if(iBone != -1)
		{
			Vector3 vertPos;
			GetDefaultBonePositionSimple(iBone,vertPos);
			m_vSuspensionDeformationRF = VEC3V_TO_VECTOR3(GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vertPos)));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a car
///////////////////////////////////////////////////////////////////////////////////
void CAutomobile::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion)
{
	weaponDebugf3("CAutomobile::BlowUpCar bNetCall[%d] GetStatus()[%d]",bNetCall,GetStatus());

	CVehicle::BlowUpCar(pCulprit);
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	if (GetStatus() == STATUS_WRECKED)
	{
		// Don't blow cars up a 2nd time (but make sure we kill any occupants in case they aren't already dead -- e.g. car drowned).
		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);
		return;
	}

	// we can't blow up cars controlled by another machine
    // but we still have to change their status to wrecked
    // so the car doesn't blow up if we take control of an
    // already blown up car
	if (IsNetworkClone())
    {
		if (!bNetCall)
		{
			weaponWarningf("CAutomobile::BlowUpCar--Note--IsNetworkClone--!bNetCall-->return");
			return;
		}

		KillPedsInVehicle(pCulprit, weaponHash);
    	KillPedsGettingInVehicle(pCulprit);

		// knock bits off the car
		GetVehicleDamage()->BlowUpCarParts(pCulprit);

		// Break lights, windows and sirens
		GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

		if( bAddExplosion )
		{
			m_nPhysicalFlags.bRenderScorched = TRUE;  
		}
		else
		{
			// GTAV - B*1948742 - If we're not adding an explosion
			// check the last damage has to see if we need to render scorched.
			u32 weaponDamageHash = GetWeaponDamageHash();

			if( weaponDamageHash == WEAPONTYPE_FIRE ||
				weaponDamageHash == WEAPONTYPE_EXPLOSION )
			{
				m_nPhysicalFlags.bRenderScorched = TRUE;
			}
		}
		
		SetTimeOfDestruction();
		SetHealth(0.0f, true);	
		SetIsWrecked();

		// Switch off the engine. (For sound purposes)
		this->SwitchEngineOff(false);
		this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
		this->m_nVehicleFlags.bLightsOn = FALSE;
		this->TurnSirenOn(FALSE);
		this->m_nVehicleFlags.bApproachingJunctionWithSiren = FALSE;
		this->m_nVehicleFlags.bPreviousApproachingJunctionWithSiren = FALSE;
		this->m_nAutomobileFlags.bTaxiLight = FALSE;

		// set the engine temp super high so we get nice sounds as the chassis cools down
		m_EngineTemperature = MAX_ENGINE_TEMPERATURE;

		g_decalMan.Remove(this);

		//Check to see that it is the player or a vehicle driven by the player
		if( (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
			|| (pCulprit && pCulprit->GetIsTypeVehicle() && ((CVehicle*)pCulprit)->GetDriver() == CGameWorld::FindLocalPlayer()) )
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
		}

		return;
    }

	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	//	If the flag is set for don't damage this car (usually during a cutscene)
	}

	if (NetworkUtils::IsNetworkCloneOrMigrating(this))
	{
		// the vehicle is migrating. Create a weapon damage event to blow up the vehicle, which will be sent to the new owner. If the migration fails
		// then the vehicle will be blown up a little later.
		CBlowUpVehicleEvent::Trigger(*this, pCulprit, bAddExplosion, weaponHash, bDelayedExplosion);
		return;
	}

	//Total damage done for the damage trackers
	float totalDamage = GetHealth() + m_VehicleDamage.GetEngineHealth() + m_VehicleDamage.GetPetrolTankHealth();
	for(s32 i=0; i<GetNumWheels(); i++)
	{
		totalDamage += m_VehicleDamage.GetTyreHealth(i);
		totalDamage += m_VehicleDamage.GetSuspensionHealth(i);
	}
	totalDamage = totalDamage > 0.0f ? totalDamage : 1000.0f;

	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		// If this is the kill shot we need to report the destroying vehicle crime
		if(GetHealth() > 0.0f)
		{
			CPed* pInflictorPed = pCulprit->GetIsTypeVehicle() ? static_cast<CVehicle*>(pCulprit)->GetDriver() : static_cast<CPed*>(pCulprit);
			CCrime::ReportDestroyVehicle(this, pInflictorPed);
		}

		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->HavocCaused += HAVOC_BLOWUPCAR;
	}

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);

		if (CVehicleDeformation::ms_iExtraExplosionDeformations > 0)
		{
			CVehicleDamage::AddVehicleExplosionDeformations(this, pCulprit, DAMAGE_TYPE_EXPLOSIVE, CVehicleDeformation::ms_iExtraExplosionDeformations, CVehicleDeformation::ms_fExtraExplosionsDamage);
		}
	}
	else
	{
		m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off
	}

	// knock bits off the car
	GetVehicleDamage()->BlowUpCarParts(pCulprit, CVehicleDamage::Break_Off_Car_Parts_Pending_Bound_Update);

	// Break lights, windows and sirens
	GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

	SetIsWrecked();

    //Set the destruction information.
    SetDestructionInfo(pCulprit, weaponHash);

	SetHealth(0.0f);	// Make sure this happens before AddExplosion or it will blow up twice

 	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	
//	Vector3	Temp = GetPosition();

	KillPedsInVehicle(pCulprit, weaponHash);
	KillPedsGettingInVehicle(pCulprit);


	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;
	this->TurnSirenOn(FALSE);
	this->m_nVehicleFlags.bApproachingJunctionWithSiren = FALSE;
	this->m_nVehicleFlags.bPreviousApproachingJunctionWithSiren = FALSE;
	this->m_nAutomobileFlags.bTaxiLight = FALSE;

	// set the engine temp super high so we get nice sounds as the chassis cools down
	m_EngineTemperature = MAX_ENGINE_TEMPERATURE;

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this, (pCulprit == CGameWorld::FindLocalPlayerVehicle()) ? totalDamage : 0.0f);
	}

    if(GetAttachedTrailer())
    {
		if( !MI_TRAILER_TRAILERLARGE.IsValid() ||
			GetAttachedTrailer()->GetModelIndex() != MI_TRAILER_TRAILERLARGE )
        {
			GetAttachedTrailer()->BlowUpCar( pCulprit, bInACutscene, bAddExplosion, bNetCall, weaponHash);
		}
    }

	if (m_nAutomobileFlags.bDropsMoneyWhenBlownUp)
	{
		for (s32 i = 0; i < 5; i++)
		{
			Vector3 coors = vThisPosition + Vector3(fwRandom::GetRandomNumberInRange(-6.0f, 6.0f), fwRandom::GetRandomNumberInRange(-6.0f, 6.0f), 0.0f);

			phIntersection testIntersection;
			static phSegment testSegment;
			testSegment.A = coors + Vector3(0.0f, 0.0f, 3.0f);
			testSegment.B = coors + Vector3(0.0f, 0.0f, -3.0f);

			if (CPhysics::GetLevel()->TestProbe(testSegment, &testIntersection, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER))
			{
				coors.z = testIntersection.GetPosition().GetZf();
				CPickupManager::CreateSomeMoney(coors, fwRandom::GetRandomNumberInRange(25, 50), 8);
			}
		}
	}

	if (GetModelIndex() == MI_CAR_SECURICAR)
	{
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CPed * pCulpritPlayer = (CPed*)pCulprit;
			pCulpritPlayer->GetPlayerWanted()->SetWantedLevelNoDrop(vThisPosition, WANTED_LEVEL2, 0, WL_FROM_LAW_RESPONSE_EVENT);
		}
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);

	// Set the CG offset so that the collider rotates about the instance position
	// NOTE: Modifying the CG offset doesn't work on articulated colliders at the moment. Do the modification at the
	//       end of the function so that we have a chance to make the vehicle simple-articulated by blowing up all the parts
	if(!IsColliderArticulated())
	{
		GetVehicleFragInst()->GetArchetype()->GetBound()->SetCGOffset(Vec3V(V_ZERO));
		phCollider *pCollider = GetCollider();
		if(pCollider)
			pCollider->SetColliderMatrixFromInstance();
	}
}

//////////////////////////////////////////////////////////////////////////
// Damage the vehicle even more, close to these bones during BlowUpCar
//////////////////////////////////////////////////////////////////////////
//const int CAR_BONE_COUNT_TO_DEFORM = 6;
//const eHierarchyId ExtraCarBones[CAR_BONE_COUNT_TO_DEFORM] = { VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS };

const eHierarchyId* CAutomobile::GetExtraBonesToDeform(int& extraBoneCount)
{
	//Leave extra deformations off for cars, they are damaged enough from the main chassis
	//extraBoneCount = CAR_BONE_COUNT_TO_DEFORM;
	//return ExtraCarBones;
	extraBoneCount = 0;
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayCarHorn
// PURPOSE :  Play the car horn if not already triggered.  Also shouts abuse
/////////////////////////////////////////////////////////////////////////////////

#define MIN_SPEAK_TIME			1500
#define SPEAK_TIME_OFFSET		3000

#define MIN_NO_HORN_TIME		150

void CAutomobile::PlayCarHorn(bool /*bOneShot*/, u32 hornTypeHash)
{
	//u32 r;

	if(!IsAlarmActivated())
	{
		//if(!IsHornOn()) 
		{
// 			if(m_NoHornCount > 0 && !bOneShot)
// 			{
// 				m_NoHornCount--;
// 				return;
// 			}

			//m_NoHornCount = u8(MIN_NO_HORN_TIME + (fwRandom::GetRandomNumber() & 0x7f));
			
	//		r = fwRandom::GetRandomNumber() & 0xf;	// 
			
			//r = m_NoHornCount & 0x7;



			//if(r < 2 || r < 4)
			{
				// Horn only
				if(m_VehicleAudioEntity)
				{
					//@@: location CAUTOMOBILE_PLAYCARHORN
					m_VehicleAudioEntity->SetHornType(hornTypeHash);
					m_VehicleAudioEntity->PlayVehicleHorn(0.96f, true);
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CleanupMissionState
// PURPOSE :  Cleans up any state set while this auto was a mission entity. 
/////////////////////////////////////////////////////////////////////////////////

void CAutomobile::CleanupMissionState()
{
	CVehicle::CleanupMissionState();

	m_nAutomobileFlags.bWaterTight = FALSE;
    m_nAutomobileFlags.bBurnoutModeEnabled = FALSE;
    m_nAutomobileFlags.bReduceGripModeEnabled = FALSE;
}

// Name			:	ScanForCrimes
// Purpose		:	Checks area surrounding car for crimes being committed. These are reported immediately
// Parameters	:	None
// Returns		:	Nothing

#define COP_CAR_CRIME_DETECTION_RANGE (20.0f)

RAGETRACE_DECL(CAutomobile_ScanForCrimes);

void CAutomobile::ScanForCrimes()
{
	RAGETRACE_SCOPE(CAutomobile_ScanForCrimes);
	
	// If the player drives a cop with car alarm activated we set the wanted status
	// to a set minimum in one go.
	if (CGameWorld::FindLocalPlayerVehicle() && CGameWorld::FindLocalPlayerVehicle()->InheritsFromAutomobile())
	{
		if ( ((CAutomobile *)CGameWorld::FindLocalPlayerVehicle())->IsAlarmActivated() )
		{
			// Test whether we are close enough to notice player
			Vector3	Dist = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayerVehicle()->GetTransform().GetPosition() - this->GetTransform().GetPosition());
			if (Dist.Mag2() < COP_CAR_CRIME_DETECTION_RANGE*COP_CAR_CRIME_DETECTION_RANGE)
			{
				CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
				if (pPlayerPed && pPlayerPed->GetPlayerWanted()->m_fMultiplier > 0.0f)
				{
					pPlayerPed->GetPlayerWanted()->SetOneStarParoleNoDrop();
				}
			}
		}
	}
}

void CAutomobile::SetAutoDoorTimer(u32 nTime, eStatus status, u32 uDoorTimeToOpen, u32 uDoorTimeToClose)
{
	if(nTime < (uDoorTimeToOpen + uDoorTimeToClose))
	{
		nTime = uDoorTimeToOpen + uDoorTimeToClose;
	}
	
	if(status == OPEN_THEN_CLOSE)
	{
		m_nAutoDoorStart = fwTimer::GetTimeInMilliseconds();
	}
	else
		m_nAutoDoorStart = fwTimer::GetTimeInMilliseconds()-uDoorTimeToOpen;
	
	m_nAutoDoorTimer = m_nAutoDoorStart + nTime;
}

void CAutomobile::ProcessAutoBusDoors()
{
	if(m_nAutoDoorTimer > fwTimer::GetTimeInMilliseconds())
	{
		float fRatioOpen;// fully open
		
//		GetHealth() = 1000;
		// keep opening
		if((m_nAutoDoorTimer > 0)&&(fwTimer::GetTimeInMilliseconds() > m_nAutoDoorTimer-DEFAULT_DOOR_TIME_TO_CLOSE))
		{
			// keep going till closed
			CCarDoor* pDoor = GetDoorFromId(VEH_DOOR_DSIDE_F);
			if(pDoor)
			{
				if(!pDoor->GetIsClosed())
					fRatioOpen = 1.0f - ((float)(fwTimer::GetTimeInMilliseconds()-(m_nAutoDoorTimer-DEFAULT_DOOR_TIME_TO_CLOSE))) / DEFAULT_DOOR_TIME_TO_CLOSE;
				else
				{
					m_nAutoDoorTimer = fwTimer::GetTimeInMilliseconds();
					fRatioOpen = 0.0f;
				}
	
				pDoor->SetTargetDoorOpenRatio(fRatioOpen, 0);
			}
			// keep going till closed
			pDoor = GetDoorFromId(VEH_DOOR_PSIDE_F);
			if(pDoor)
			{
				if(!pDoor->GetIsClosed())
					fRatioOpen = 1.0f - ((float)(fwTimer::GetTimeInMilliseconds()-(m_nAutoDoorTimer-DEFAULT_DOOR_TIME_TO_CLOSE))) / DEFAULT_DOOR_TIME_TO_CLOSE;
				else
				{
					m_nAutoDoorTimer = fwTimer::GetTimeInMilliseconds();
					fRatioOpen = 0.0f;
				}
	
				pDoor->SetTargetDoorOpenRatio(fRatioOpen, 0);
			}
		}
	}
	else
	{
		if(m_nAutoDoorStart != 0)
		{
			if(GetDoorFromId(VEH_DOOR_DSIDE_F))
				GetDoorFromId(VEH_DOOR_DSIDE_F)->SetShutAndLatched(this);
			if(GetDoorFromId(VEH_DOOR_PSIDE_F))
				GetDoorFromId(VEH_DOOR_PSIDE_F)->SetShutAndLatched(this);
			
			m_nAutoDoorStart = 0;
			m_nAutoDoorTimer = 0;
		}
		// door should be closed		
	}
}

void CAutomobile::ProcessAutoTurretDoors()
{
	if (!GetVehicleModelInfo())
		return;

	const CModelSeatInfo* pModelSeatInfo = GetVehicleModelInfo()->GetModelSeatInfo();
	if (!pModelSeatInfo)
		return;

	const u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	if (m_nAutoDoorTimer < uCurrentTime)
		return;

	if ((m_nAutoDoorTimer > 0) && (uCurrentTime > m_nAutoDoorTimer-DEFAULT_DOOR_TIME_TO_CLOSE))
	{
		// Find turret seat
		const s32 iNumSeats = pModelSeatInfo->GetNumSeats();
		for (s32 iSeatIndex = 0; iSeatIndex < iNumSeats; ++iSeatIndex)
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = GetSeatAnimationInfo(iSeatIndex);
			if (pSeatAnimInfo && pSeatAnimInfo->IsTurretSeat())
			{
				if (!CTaskVehicleFSM::GetPedInOrUsingSeat(*this, iSeatIndex))
				{
					const s32 iEntryPointIndex = pModelSeatInfo->GetEntryPointIndexForSeat(iSeatIndex, this);
					if (IsEntryIndexValid(iEntryPointIndex))
					{
						CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(this, iEntryPointIndex);
						if( pDoor &&
							!pDoor->GetFlag( CCarDoor::DRIVEN_SPECIAL ) )
						{
							float fRatioOpen;
							if (!pDoor->GetIsClosed())
							{
								fRatioOpen = 1.0f - ((float)(uCurrentTime-(m_nAutoDoorTimer-DEFAULT_DOOR_TIME_TO_CLOSE))) / DEFAULT_DOOR_TIME_TO_CLOSE;
							}
							else
							{
								m_nAutoDoorTimer = uCurrentTime;
								fRatioOpen = 0.0f;
							}

							pDoor->SetTargetDoorOpenRatio(fRatioOpen, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN|CCarDoor::DRIVEN_SPECIAL);
						}
					}
				}
			}
		}
	}
	else
	{
		m_nAutoDoorStart = 0;
		m_nAutoDoorTimer = 0;
	}
}

void CAutomobile::GetVehicleMovementStickInput(const CControl& rControl, float& fStickX, float& fStickY)
{
	fStickX = rControl.GetVehicleSteeringLeftRight().GetNorm();
	fStickY = rControl.GetVehicleSteeringUpDown().GetNorm();
#if USE_SIXAXIS_GESTURES
	if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_AFTERTOUCH))
	{
		CPadGesture* gesture = CControlMgr::GetPlayerPad()->GetPadGesture();
		if(gesture)
		{
			if(fStickX == 0.0f && rControl.GetVehicleSteeringLeftRight().IsEnabled())
			{
				fStickX = gesture->GetPadRollInRange(CVehicle::ms_fDefaultMotionContolRollMin,CVehicle::ms_fDefaultMotionContolRollMax,true) * CVehicle::ms_fDefaultMotionContolAftertouchMult;
			}
			if(fStickY == 0.0f && rControl.GetVehicleSteeringUpDown().IsEnabled())
			{
				fStickY = gesture->GetPadPitchInRange(CVehicle::ms_fDefaultMotionContolPitchMin,CVehicle::ms_fDefaultMotionContolPitchMax,true,true) * CVehicle::ms_fDefaultMotionContolAftertouchMult;
			}
		}
	}
#endif // USE_SIXAXIS_GESTURES
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlaceOnRoadProperly
// PURPOSE :  If the car is placed kinda on the road this function will
//			  do it properly
// RETURNS :  true if collision was actually found for front AND rear point.
/////////////////////////////////////////////////////////////////////////////////
PF_PAGE(GTAVehPlaceOnRoad,"GTA Veh place on road");
PF_GROUP(DoingPlaceOnRoad);
PF_LINK(GTAVehPlaceOnRoad, DoingPlaceOnRoad);
PF_TIMER(PlaceIt, DoingPlaceOnRoad);
bool CAutomobile::PlaceOnRoadProperly(CAutomobile* pAutomobile, Matrix34 *pMat, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, bool& setWaitForAllCollisionsBeforeProbe, u32 MI, phInst* pTestException, bool useVirtualRoad, CNodeAddress* aNodes, s16 * aLinks, s32 numNodes, float HeightSampleRangeUp, float HeightSampleRangeDown)
{
	PF_FUNC(PlaceIt);

	setWaitForAllCollisionsBeforeProbe = false;

	const ScalarV fOne(V_ONE);
	fwModelId modelId((strLocalIndex(MI)));
	Assert(modelId.IsValid());
	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);
	Assertf(pModelInfo->GetIsAutomobile() || pModelInfo->GetIsTrailer() || pModelInfo->GetIsBike(), "Unrecognized type for '%s'", pModelInfo->GetModelName());

	// Deal with the vehicle being upside down. Flip it round.
	if (pMat->c.z < 0.0f)
	{
		pMat->a = -pMat->a;
		pMat->c = -pMat->c;
	}

	// Determine if we are forced to use the real road surface instead of the virtual road.
	// This happens when the nodes are in tunnels.
	if(useVirtualRoad)
	{
		Assert(aNodes);

		bool inTunnel = false;
		for(int i = 0; i < numNodes; ++i)
		{
			const CPathNode* pNode = ThePaths.FindNodePointer(aNodes[i]);
			Assert(pNode);
			if(pNode->m_2.m_inTunnel)
			{
				inTunnel = true;
				break;
			}
		}

		if(inTunnel)
		{
			// We will need to store data about interior and that data is only in the collision info so
			// we need to use the world collision mesh and not the virtual road thus we also need to make
			// sure the world collision mesh around us is loaded.
			const bool worldCollisionLoadedHere = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(RCC_VEC3V(pMat->d), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
			if(worldCollisionLoadedHere)
			{
				useVirtualRoad = false;
			}
			else
			{
				if(pAutomobile)
				{
					pAutomobile->GetPortalTracker()->SetWaitForAllCollisionsBeforeProbe(true);
				}
				else
				{
					setWaitForAllCollisionsBeforeProbe = true;
				}
			}
		}
	}

	float frontHeightAboveGround, rearHeightAboveGround;
	CVehicle::CalculateHeightsAboveRoad(modelId, &frontHeightAboveGround, &rearHeightAboveGround);

    if( pAutomobile && pAutomobile->GetVehicleModelInfo() && pAutomobile->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED ) &&
        pAutomobile->GetVelocity().Mag2() < 1.0f )
    {
        frontHeightAboveGround -= pAutomobile->pHandling->m_fSuspensionRaise;
        rearHeightAboveGround -= pAutomobile->pHandling->m_fSuspensionRaise;
    }

	CVehicleWeaponHandlingData* pWeaponHandling = (pAutomobile && pAutomobile->pHandling) ? pAutomobile->pHandling->GetWeaponHandlingData() : NULL;
	float fWheelImpactOffset = (pAutomobile && pAutomobile->GetWheel(0)) ? pAutomobile->GetWheel(0)->GetWheelImpactOffset() : 0.0f;
	if(pWeaponHandling)// weapon wheel impact offset is already taken into account.
	{
		fWheelImpactOffset -= pWeaponHandling->GetWheelImpactOffset();
	}
	frontHeightAboveGround += fWheelImpactOffset;
	rearHeightAboveGround += fWheelImpactOffset;

	// do a single probe first to find rough height first (not necessary if using virtual road as it doesn't really have layers
	if(!useVirtualRoad)
	{
		const int nInitialNumResults = 8;

		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<nInitialNumResults> probeResults;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(pMat->d + HeightSampleRangeUp*ZAXIS, pMat->d - HeightSampleRangeDown*ZAXIS);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		int nBestHit = -1;
		float fBestDeltaZ = LARGE_FLOAT;

		WorldProbe::ResultIterator it;
		int i = 0;
		for(it = probeResults.begin(); it < probeResults.end(); ++it, ++i)
		{
			if(it->GetHitDetected() && rage::Abs((it->GetHitPosition() - pMat->d).z) < fBestDeltaZ)
			{
				nBestHit = i;
				fBestDeltaZ = rage::Abs((it->GetHitPosition() - pMat->d).z);
			}
		}

		if(nBestHit > -1)
			pMat->d.z = probeResults[nBestHit].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN + 0.5f*(frontHeightAboveGround+rearHeightAboveGround);
	}


	const int MAX_NUM_WHEELS = 10;
	const int MAX_NUM_TRAILER_WHEELS = 6;
	Vector3 aWheelPos[MAX_NUM_WHEELS];
	eHierarchyId aCarWheelIds[MAX_NUM_WHEELS] = {VEH_WHEEL_LF,VEH_WHEEL_LR,VEH_WHEEL_RF,VEH_WHEEL_RR, VEH_WHEEL_LM1, VEH_WHEEL_RM1, VEH_WHEEL_LM2, VEH_WHEEL_RM2, VEH_WHEEL_LM3, VEH_WHEEL_RM3};
	eHierarchyId aTrailerWheelIds[MAX_NUM_TRAILER_WHEELS] = {VEH_WHEEL_LM1, VEH_WHEEL_LR, VEH_WHEEL_RM1, VEH_WHEEL_RR, VEH_WHEEL_LM2, VEH_WHEEL_RM2};

	eHierarchyId *aWheelIds = aCarWheelIds;
	if(pAutomobile && pAutomobile->InheritsFromTrailer())
	{
		aWheelIds = aTrailerWheelIds;
	}
	
	int numValidWheels = 0;
	for(int i = 0; i < 4; i++)
	{
		// Fall back to LF wheel if this wheel doesn't exist
		int iBoneId = pModelInfo->GetBoneIndex(aWheelIds[i]);
		if(iBoneId > -1)
		{
            aWheelPos[i] = CWheel::GetWheelOffset(pModelInfo, aWheelIds[i]);
			numValidWheels++;
		}
        else if(pAutomobile && pAutomobile->GetIsAircraft() && (i == 2 || i == 3))//planes have very large bounds so we're better off using the front left or rear left wheel if we are missing a wheel
        {
            if(i == 2)
            {
                aWheelPos[i] = aWheelPos[0];
            }
            else if(i == 3)
            {
                aWheelPos[i] = aWheelPos[1];
            }
        }
		else
		{
			// No wheel here, use bounding box instead
			aWheelPos[i] = pModelInfo->GetBoundingBoxMin();
			
			if(i == 0 || i == 2)
			{
				// Front
				aWheelPos[i].y += pModelInfo->GetBoundingBoxMax().y;
			}

			if(i == 2 || i == 3)
			{
				// Right
				aWheelPos[i].x += pModelInfo->GetBoundingBoxMax().x;
			}
		}
	}

	// the schnuppe only has two wheels so it fails to generate a valid matrix from the hit results
	// so slightly offset the x values of all the wheels.
	if( numValidWheels < 4 &&
		pAutomobile && 
		pAutomobile->GetIsAircraft() )
	{
		for( int i = 0; i < 4; i++ )
		{
			if( Abs( aWheelPos[ i ].x ) <= 0.01f )
			{
				if( i < 2 )
				{
					aWheelPos[ i + 2 ] = aWheelPos[ i ];
					aWheelPos[ i ].x -= 0.1f;
					aWheelPos[ i + 2 ].x += 0.1f;
				}
				else
				{
					aWheelPos[ i - 2 ] = aWheelPos[ i ];
					aWheelPos[ i ].x += 0.1f;
					aWheelPos[ i - 2 ].x -= 0.1f;
				}
			}
		}
	}

	float	averageLocalWheelPosB = (aWheelPos[0].y + aWheelPos[1].y + aWheelPos[2].y + aWheelPos[3].y) * 0.25f;

	WorldProbe::CShapeTestFixedResults<MAX_NUM_WHEELS> probeResults;
	const CVehicleFollowRouteHelper* pFollowRoute = NULL;

	int nExtraWheelsToSetCompressionOn = 0;

	if(pAutomobile)
	{		
		if(!pAutomobile->InheritsFromTrailer())
		{
			for(int i = 4; i < MAX_NUM_WHEELS; i++)//check if we have extra wheels
			{
				int iBoneId = pModelInfo->GetBoneIndex(aWheelIds[i]);
				if(iBoneId > -1)
				{
					aWheelPos[i] = CWheel::GetWheelOffset(pModelInfo, aWheelIds[i]);
					nExtraWheelsToSetCompressionOn++;
				}
			}
		}
		else if(pAutomobile->InheritsFromTrailer())
		{
			for(int i = 4; i < MAX_NUM_TRAILER_WHEELS; i++)//check if we have extra wheels
			{
				int iBoneId = pModelInfo->GetBoneIndex(aWheelIds[i]);
				if(iBoneId > -1)
				{
					aWheelPos[i] = CWheel::GetWheelOffset(pModelInfo, aWheelIds[i]);
					nExtraWheelsToSetCompressionOn++;
				}
			}
		}

		// Trailers use their parent's follow route
		if (pAutomobile->m_nVehicleFlags.bHasParentVehicle && CVehicle::IsEntityAttachedToTrailer(pAutomobile) && pAutomobile->GetDummyAttachmentParent()
			&& pAutomobile->GetDummyAttachmentParent()->GetDummyAttachmentParent())
		{
			pFollowRoute = pAutomobile->GetDummyAttachmentParent()->GetDummyAttachmentParent()->GetIntelligence()->GetFollowRouteHelper();
		}
		else if(pAutomobile->m_nVehicleFlags.bHasParentVehicle && pAutomobile->InheritsFromTrailer() && pAutomobile->GetDummyAttachmentParent())
		{
			pFollowRoute = pAutomobile->GetDummyAttachmentParent()->GetIntelligence()->GetFollowRouteHelper();
		}
		else
		{
			pFollowRoute = pAutomobile->GetIntelligence()->GetFollowRouteHelper();
		}
	}

	int nNumberOfWheelsToSample(4 + nExtraWheelsToSetCompressionOn);

	if(useVirtualRoad && pFollowRoute)
	{
		phSegment testSegments[MAX_NUM_WHEELS];
		for(int i=0; i<nNumberOfWheelsToSample; i++)
		{
			testSegments[i].Set(aWheelPos[i], aWheelPos[i]);
			testSegments[i].Transform(*pMat);
			testSegments[i].A.Add(HeightSampleRangeUp*ZAXIS);
			testSegments[i].B.Subtract(HeightSampleRangeDown*ZAXIS);
		}
		CVirtualRoad::TestProbesToVirtualRoadFollowRouteAndCenterPos(*pFollowRoute, pAutomobile->GetVehiclePosition(), probeResults, testSegments, nNumberOfWheelsToSample, fOne, pAutomobile->ShouldTryToUseVirtualJunctionHeightmaps());
	}
	else if(useVirtualRoad && numNodes==3) // as used by vehicle population code
	{
		phSegment testSegments[MAX_NUM_WHEELS];
		for(int i=0; i<nNumberOfWheelsToSample; i++)
		{
			testSegments[i].Set(aWheelPos[i], aWheelPos[i]);
			testSegments[i].Transform(*pMat);
			testSegments[i].A.Add(HeightSampleRangeUp*ZAXIS);
			testSegments[i].B.Subtract(HeightSampleRangeDown*ZAXIS);
		}

		CVehicleNodeList nodeList;
		nodeList.SetPathNodeAddr(NODE_OLD, aNodes[0]);
		nodeList.SetPathLinkIndex(LINK_OLD_TO_NEW, aLinks[0]);
		nodeList.SetPathNodeAddr(NODE_NEW, aNodes[1]);
		nodeList.SetPathLinkIndex(LINK_NEW_TO_FUTURE, aLinks[1]);
		nodeList.SetPathNodeAddr(NODE_FUTURE, aNodes[2]);
		nodeList.SetTargetNodeIndex(NODE_NEW);

		CVirtualRoad::TestProbesToVirtualRoadNodeListAndCenterNode(nodeList, NODE_NEW, probeResults, testSegments, nNumberOfWheelsToSample, fOne);
	}
	else
	{
		const float fMaxOffset = 2.0f;

		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResults);

		atArray<const phInst*> excludeInstances;
		excludeInstances.Grow() = pTestException;

		// Loop through all the attached objects and exclude them from this test.
		if(pAutomobile) 
		{
			if(fwAttachmentEntityExtension *extension = pAutomobile->GetAttachmentExtension())
			{
				CPhysical* pCurChild = static_cast<CPhysical*>(extension->GetChildAttachment());
				while(pCurChild)
				{
					if(pCurChild->GetIsTypeObject() && pCurChild->GetCurrentPhysicsInst())
					{
						excludeInstances.Grow() = pCurChild->GetCurrentPhysicsInst();
					}
					fwAttachmentEntityExtension &curChildAttachExt = pCurChild->GetAttachmentExtensionRef();
					pCurChild = static_cast<CPhysical*>(curChildAttachExt.GetSiblingAttachment());
				}
			}
		}

		const int numExcludeInstances = excludeInstances.GetCount();
		probeDesc.SetExcludeInstances(excludeInstances.GetElements(), numExcludeInstances);


		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE|ArchetypeFlags::GTA_OBJECT_TYPE);
		probeDesc.SetTypeFlags(TYPE_FLAGS_ALL);
		for(int i=0; i<nNumberOfWheelsToSample; i++)
		{
			phSegment testSegment(aWheelPos[i] + fMaxOffset*ZAXIS, aWheelPos[i] - fMaxOffset*ZAXIS);
			testSegment.Transform(*pMat);

			probeDesc.SetStartAndEnd(testSegment.A, testSegment.B);
			probeDesc.SetFirstFreeResultOffset(i);
			probeDesc.SetMaxNumResultsToUse(1);

			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		}
	}

	Vector3 vPosns[MAX_NUM_WHEELS];
	WorldProbe::ResultIterator it = probeResults.begin();
	for(int i=0; i<nNumberOfWheelsToSample; ++i, ++it)
	{
		bool bFrontWheel = (i==0 || i==2);


		if(it->GetHitDetected())
		{
			vPosns[i].Set(it->GetHitPosition());
			vPosns[i].z += CONCAVE_DISTANCE_MARGIN;
		}
		else
		{
			vPosns[i].Set(aWheelPos[i]);
			vPosns[i].z -= pModelInfo->GetTyreRadius(bFrontWheel);

			pMat->Transform(vPosns[i]);
		}

		CWheel* pWheel = NULL;
		if(pAutomobile)
		{
			pWheel = pAutomobile->GetWheelFromId(aWheelIds[i]);
		}

		if(pWheel)
		{
			// add on car height above road to intersection positions
			float fHeightAboveGround = Selectf(pWheel->GetFrontRearSelector(), frontHeightAboveGround, rearHeightAboveGround);
			if(pWheel->GetTyreHealth() < TYRE_HEALTH_DEFAULT)
				fHeightAboveGround -= pWheel->GetTyreBurstCompression();
			vPosns[i].Add( fHeightAboveGround*it->GetHitNormal() );
		}
		else
		{
			if(bFrontWheel)
				vPosns[i].Add(frontHeightAboveGround*ZAXIS);
			else
				vPosns[i].Add(rearHeightAboveGround*ZAXIS);
		}
	}

	// fwd  =              front          -              back
	pMat->b = (vPosns[0] + vPosns[2]) - (vPosns[1] + vPosns[3]);
	pMat->b.Normalize();
	// right =             right          -              left
	pMat->a = (vPosns[2] + vPosns[3]) - (vPosns[0] + vPosns[1]);
	pMat->c.Cross(pMat->a, pMat->b);
	pMat->c.Normalize();
	pMat->a.Cross(pMat->b, pMat->c);

	// pos  = average
	pMat->d = vPosns[0] + vPosns[1] + vPosns[2] + vPosns[3];
	pMat->d.Scale(0.25f);
	pMat->d -= pMat->b * averageLocalWheelPosB;

	if( pModelInfo->GetIsTrailer() &&
		Abs( pMat->c.z ) < 0.2f )
	{
		pMat->c = Vector3( 0.0f, 0.0f, 1.0f );
		pMat->a.Cross( pMat->c, pMat->b );
		pMat->a.Normalize();
		pMat->b.Cross( pMat->a, pMat->c );
		pMat->b.Normalize();
	}

	// fix up wheel compression ratios so the wheels touch the intersection points correctly
	if(pAutomobile)
	{
        float suspensionRaiseAmount = 0.0f;
        if( pAutomobile->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED ) &&
            pAutomobile->GetVelocity().Mag2() < 1.0f )
        {
            suspensionRaiseAmount = -pAutomobile->pHandling->m_fSuspensionRaise;
        }

		// fix up wheel position
		it = probeResults.begin();
		for(int i=0; i<nNumberOfWheelsToSample; ++i, ++it)
		{
			CWheel* pWheel = pAutomobile->GetWheelFromId(aWheelIds[i]);
			if(pWheel)
			{
				Vector3 vHitPosition = it->GetHitPosition();
				vHitPosition.z += CONCAVE_DISTANCE_MARGIN;
				pWheel->SetCompressionFromHitPos(pMat, vHitPosition, it->GetHitDetected(), it->GetHitNormal());

                if( pAutomobile->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED ) )
                {
                    pWheel->SetSuspensionRaiseAmount( suspensionRaiseAmount );
                    pWheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
                }
			}
		}
	}
	
	// store data about interior if any was found from the front probes (for when adding vehicle to world)
	if(!useVirtualRoad)
	{
		it = probeResults.begin();
		for(int i=0; i<nNumberOfWheelsToSample; ++i, ++it)
		{
			if(it->GetHitDetected())
			{
				s32 roomIdx = PGTAMATERIALMGR->UnpackRoomId(it->GetHitMaterialId());
				if( (roomIdx > 0) && (it->GetHitInst()!=NULL) )
				{
					CEntity* pHitEntity  = reinterpret_cast<CEntity*>(it->GetHitInst()->GetUserData());
					CInteriorInst* pIntInst = NULL;
					CInteriorProxy* pInteriorProxy = CInteriorProxy::GetInteriorProxyFromCollisionData(pHitEntity, &it->GetHitInst()->GetPosition());
					Assert(pInteriorProxy);

					if (pInteriorProxy)
					{
						pIntInst = pInteriorProxy->GetInteriorInst();
					}

					if(pIntInst && pIntInst->CanReceiveObjects())
					{
						pDestInteriorInst = pIntInst;
						destRoomIdx = roomIdx;
						break;
					} else
					{
						return(false);		// if interior not ready then immediately bail out
					}
				}
			}
		}
	}

	if(probeResults[0].GetHitDetected())
	{
		Assert(pMat->IsOrthonormal());
		return true;
	}
	return false;

	// JB : From here I removed old commented-out version in the interests of neatness
	// If required, search P4 history of this file for "OLD VERSION - USED 2 SAMPLES, BACK AND FRONT, PLUS SURFACE NORMAL"
}

s32 CAutomobile::CleanUpPlacementArray(bool getFirstFree)
{
	// find first available slot and invalidate any with no vehicle pointer (in case vehicle has been destroyed)
	s32 availableIndex = -1;
	for (s32 i = 0; i < ms_placeOnRoadArray.GetCount(); ++i)
	{
		if (ms_placeOnRoadArray[i].id == INVALID_ASYNC_ENTRY || !ms_placeOnRoadArray[i].automobile)
		{
			if (ms_placeOnRoadArray[i].roughHeightResults)
				delete ms_placeOnRoadArray[i].roughHeightResults;
			ms_placeOnRoadArray[i].roughHeightResults = NULL;

			if (ms_placeOnRoadArray[i].wheelResults)
				delete[] ms_placeOnRoadArray[i].wheelResults;
			ms_placeOnRoadArray[i].wheelResults = NULL;

			ms_placeOnRoadArray[i].id = INVALID_ASYNC_ENTRY;

			if (availableIndex == -1)
			{
				availableIndex = i;
			}
		}
	}

	if (getFirstFree && availableIndex == -1)
	{
		if (ms_placeOnRoadArray.GetCount() < ms_placeOnRoadArray.GetMaxCount())
		{
			ms_placeOnRoadArray.Append();
		}
	}
	return availableIndex;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlaceOnRoadProperlyAsync
// PURPOSE :  If the car is placed kinda on the road this function will
//			  do it properly, the async version. if function returns INVALID_ASYNC_ENTRY client code should wait and retry.
//			  If the function returns a valid id, HasPlaceOnRoadProperlyFinished needs to be called every frame to poll
//			  the result, otherwise it will block the queue.
//			  NOTE: does not support virtual roads
// RETURNS :  true if scheduling was successful
/////////////////////////////////////////////////////////////////////////////////
u32 CAutomobile::PlaceOnRoadProperlyAsync(CAutomobile* pAutomobile, Matrix34 *pMat, u32 MI, float HeightSampleRangeUp, float HeightSampleRangeDown)
{
	s32 availableIndex = CleanUpPlacementArray(true);
	if (availableIndex == -1)
		return INVALID_ASYNC_ENTRY;

	Assert(MI != fwModelId::MI_INVALID);
	ASSERT_ONLY(CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId((strLocalIndex(MI)))));
	Assert(pModelInfo);
	Assert(pModelInfo->GetIsAutomobile() || pModelInfo->GetIsTrailer() || pModelInfo->GetIsBike());

	// Deal with the vehicle being upside down. Flip it round.
	if (pMat->c.z < 0.0f)
	{
		pMat->a = -pMat->a;
		pMat->c = -pMat->c;
	}

	PlaceOnRoadAsyncData& newData = ms_placeOnRoadArray[availableIndex];
	newData.matrix = pMat;
	newData.modelInfo = MI;
	newData.roughHeightResults = rage_new PlaceOnRoadAsyncData::RoughHeightResults(8);
	newData.id = (u32)availableIndex;
	newData.automobile = pAutomobile;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(newData.roughHeightResults);
	probeDesc.SetStartAndEnd(pMat->d + HeightSampleRangeUp*ZAXIS, pMat->d - HeightSampleRangeDown*ZAXIS);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

	if (!newData.roughHeightResults->GetWaitingOnResults())
	{
		if (!Verifyf(newData.roughHeightResults->GetResultsReady(), "CAutomobile::PlaceOnRoadProperlyAsync failed to schedule rought height probe, status: %d", newData.roughHeightResults->GetResultsStatus()))
		{
			delete newData.roughHeightResults;
			newData.roughHeightResults = NULL;
			newData.wheelResults = NULL;
			newData.id = INVALID_ASYNC_ENTRY;
			return INVALID_ASYNC_ENTRY;
		}
	}
	
	return newData.id;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : HasPlaceOnRoadProperlyFinished
// PURPOSE :  Needs to be called every frame after PlaceOnRoadProperlyAsync was called and returned a valid id.
// RETURNS :  true when the work has finished, false otherwise
//			  failed will be set to true when the placement of the vehicle has failed.
/////////////////////////////////////////////////////////////////////////////////
bool CAutomobile::HasPlaceOnRoadProperlyFinished(u32 id, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, u32& failed, RegdEnt* hitEntities, float* hitPos)
{
	eHierarchyId aCarWheelIds[4] = {VEH_WHEEL_LF,VEH_WHEEL_LR,VEH_WHEEL_RF,VEH_WHEEL_RR};
	eHierarchyId aTrailerWheelIds[4] = {VEH_WHEEL_LM1, VEH_WHEEL_LR, VEH_WHEEL_RM1, VEH_WHEEL_RR};
	eHierarchyId aBlimpWheelIds[4] = {VEH_WHEEL_LF, VEH_WHEEL_LR, VEH_WHEEL_LF, VEH_WHEEL_RR};

	eHierarchyId* aWheelIds = aCarWheelIds;

	failed = 0;

	CleanUpPlacementArray(false);

	if (!Verifyf(id < ms_placeOnRoadArray.GetCount(), "Invalid index %d passed to HasPlaceOnRoadProperlyFinished", id))
		return false;

	PlaceOnRoadAsyncData& top = ms_placeOnRoadArray[id];

	fwModelId topModelId((strLocalIndex(top.modelInfo)));
	Assert(topModelId.IsValid());
	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(topModelId);
	if(!Verifyf(pModelInfo && topModelId.IsValid(), "CAutomobile::HasPlaceOnRoadProperlyFinished: Invalid modelinfo index %d", top.modelInfo))
	{
		failed = 1;
		return true;
	}

	// rough height phase
	if (top.roughHeightResults != NULL)
	{
		if (!top.roughHeightResults->GetWaitingOnResults())
		{
			if (!Verifyf(top.roughHeightResults->GetResultsReady(), "Bad state for rough height probe results (%d)", top.roughHeightResults->GetResultsStatus()))
			{
				top.id = INVALID_ASYNC_ENTRY;
				failed = 2;
				return true;
			}
		}
		else
		{
			// results not ready
			return false;
		}

		if (top.automobile)
		{
			if (top.automobile->InheritsFromTrailer())
			{
				aWheelIds = aTrailerWheelIds;
			}
			else if (top.automobile->GetVehicleType() == VEHICLE_TYPE_BLIMP)
			{
				aWheelIds = aBlimpWheelIds;
			}
		}

		int nBestHit = -1;
		float fBestDeltaZ = LARGE_FLOAT;

		WorldProbe::ResultIterator it;
		int i = 0;
		for(it = top.roughHeightResults->begin(); it < top.roughHeightResults->end(); ++it, ++i)
		{
			if(it->GetHitDetected() && rage::Abs((it->GetHitPosition() - top.matrix->d).z) < fBestDeltaZ)
			{
				nBestHit = i;
				fBestDeltaZ = rage::Abs((it->GetHitPosition() - top.matrix->d).z);
			}
		}

		CVehicle::CalculateHeightsAboveRoad(topModelId, &top.frontHeightAboveGround, &top.rearHeightAboveGround);

		if(nBestHit > -1)
			top.matrix->d.z = (*top.roughHeightResults)[nBestHit].GetHitPosition().z + 0.5f*(top.frontHeightAboveGround+top.rearHeightAboveGround);

		for(int i = 0; i < 4; i++)
		{
			// Fall back to LF wheel if this wheel doesn't exist
			int iBoneId = pModelInfo->GetBoneIndex(aWheelIds[i]);
			if(iBoneId > -1)
			{
				top.aWheelPos[i] = CWheel::GetWheelOffset(pModelInfo, aWheelIds[i]);
			}
			else if(top.automobile && (top.automobile->InheritsFromPlane() || top.automobile->InheritsFromBlimp())  && (i == 2 || i == 3))//planes have very large bounds so we're better off using the front left or rear left wheel if we are missing a wheel
			{
				if(i == 2)
				{
					top.aWheelPos[i] = top.aWheelPos[0];
				}
				else if(i == 3)
				{
					top.aWheelPos[i] = top.aWheelPos[1];
				}
			}
			else
			{
				// No wheel here, use bounding box instead
				top.aWheelPos[i] = pModelInfo->GetBoundingBoxMin();

				if(i == 0 || i == 2)
				{
					// Front
					top.aWheelPos[i].y += pModelInfo->GetBoundingBoxMax().y;
				}

				if(i == 2 || i == 3)
				{
					// Right
					top.aWheelPos[i].x += pModelInfo->GetBoundingBoxMax().x;
				}
			}
		}

		// free rough height results, we're done with that one
		delete top.roughHeightResults;
		top.roughHeightResults = NULL;

		const float fMaxOffset = 2.0f;

		top.wheelResults = rage_new PlaceOnRoadAsyncData::WheelResults[4];
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
		probeDesc.SetTypeFlags(TYPE_FLAGS_ALL);
		for(int i=0; i<4; i++)
		{
			phSegment testSegment(top.aWheelPos[i] + fMaxOffset*ZAXIS, top.aWheelPos[i] - fMaxOffset*ZAXIS);
			testSegment.Transform(*top.matrix);

			probeDesc.SetStartAndEnd(testSegment.A, testSegment.B);
			probeDesc.SetResultsStructure(&top.wheelResults[i]);

			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

			if (!top.wheelResults[i].GetWaitingOnResults())
			{
				if (!Verifyf(top.wheelResults[i].GetResultsReady(), "CAutomobile::HasPlaceOnRoadProperlyFinished failed to schedule wheel probe"))
				{
					delete[] top.wheelResults;
					top.wheelResults = NULL;
					top.id = INVALID_ASYNC_ENTRY;

					// return true, no point in keeping calling this function
					failed = 3;
					return true;
				}
			}
		}

		// not done yet
		return false;
	}

	Assert(top.wheelResults && !top.roughHeightResults);

	// all results need to be finished before we continue
	for (s32 i = 0; i < 4; ++i)
		if (!top.wheelResults[i].GetResultsReady())
			return false;

	float	averageLocalWheelPosB = (top.aWheelPos[0].y + top.aWheelPos[1].y + top.aWheelPos[2].y + top.aWheelPos[3].y) * 0.25f;

	Vector3 matPosns[4];
	for(int i=0; i<4; ++i)
	{
		WorldProbe::ResultIterator it = top.wheelResults[i].begin();

		bool bFrontWheel = (i==0 || i==2);
		if(hitEntities)
			hitEntities[i] = NULL;

		if(it->GetHitDetected())
		{
			matPosns[i].Set(it->GetHitPosition());
			matPosns[i].z += CONCAVE_DISTANCE_MARGIN;
			if (hitEntities && it->GetHitInst() && i < 4)
				hitEntities[i] = CPhysics::GetEntityFromInst(it->GetHitInst());
		}
		else
		{
			matPosns[i].Set(top.aWheelPos[i]);
			matPosns[i].z -= pModelInfo->GetTyreRadius(bFrontWheel);

			top.matrix->Transform(matPosns[i]);
		}

		// add on car height above road to intersection positions
		if(bFrontWheel)
			matPosns[i].Add(top.frontHeightAboveGround*ZAXIS);
		else
			matPosns[i].Add(top.rearHeightAboveGround*ZAXIS);
	}

	// fwd  =              front          -              back
	top.matrix->b = (matPosns[0] + matPosns[2]) - (matPosns[1] + matPosns[3]);
	top.matrix->b.Normalize();
	// right =             right          -              left
	top.matrix->a = (matPosns[2] + matPosns[3]) - (matPosns[0] + matPosns[1]);
	top.matrix->c.Cross(top.matrix->a, top.matrix->b);
	top.matrix->c.Normalize();
	top.matrix->a.Cross(top.matrix->b, top.matrix->c);

	// pos  = average
	top.matrix->d = matPosns[0] + matPosns[1] + matPosns[2] + matPosns[3];
	top.matrix->d.Scale(0.25f);
	top.matrix->d -= top.matrix->b * averageLocalWheelPosB;

	// fix up wheel compression ratios so the wheels touch the intersection points correctly
	float fWheelImpactOffset = 0.0f;
	if(top.automobile)
	{
		CVehicleWeaponHandlingData* pWeaponHandling = top.automobile->pHandling ? top.automobile->pHandling->GetWeaponHandlingData() : NULL;
		
		//fix up car position
		for(int i=0; i<4; ++i)
		{
			// WorldProbe::ResultIterator it = top.wheelResults[i].begin();
			CWheel* pWheel = top.automobile->GetWheelFromId(aWheelIds[i]);
			if(pWheel)
			{
				fWheelImpactOffset += pWheel->GetWheelImpactOffset();
				if(pWeaponHandling)// weapon wheel impact offset is already taken into account.
				{
					fWheelImpactOffset -= pWeaponHandling->GetWheelImpactOffset();
				}
			}
		}
		fWheelImpactOffset *= 0.25f; 
		top.matrix->d.z += fWheelImpactOffset;

		// fix up wheel position
		for(int i=0; i<4; ++i)
		{
			WorldProbe::ResultIterator it = top.wheelResults[i].begin();
			CWheel* pWheel = top.automobile->GetWheelFromId(aWheelIds[i]);
			if(pWheel)
			{
				Vector3 vHitPosition = it->GetHitPosition();
				vHitPosition.z += CONCAVE_DISTANCE_MARGIN;
				pWheel->SetCompressionFromHitPos(top.matrix, vHitPosition, it->GetHitDetected());
			}
		}
	}

	// Hack for B1489355 - Forklift invisible in this corridor
	const u32 forkliftNameHash = ATSTRINGHASH("forklift", 0x58e49664);
	if (pModelInfo->GetModelNameHash() == forkliftNameHash)
	{
		const Vector3 forkliftCarGen(-921.0f, -2947.6f, 15.3f);
		float magSqrd = (top.matrix->d - forkliftCarGen).Mag2();
		if (magSqrd < 4.0f)
		{
			if (top.automobile)
			{
				CPortalTracker *pPortalTracker = top.automobile->GetPortalTracker();
				if (pPortalTracker)
				{ 
					pPortalTracker->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);
					pPortalTracker->ScanUntilProbeTrue();
				}
			}
		}
	}

	// store data about interior if any was found from the front probes (for when adding vehicle to world)
	for(int i=0; i<4; ++i)
	{
		WorldProbe::ResultIterator it = top.wheelResults[i].begin();
		if(it->GetHitDetected())
		{
			s32 roomIdx = PGTAMATERIALMGR->UnpackRoomId(it->GetHitMaterialId());
			if( (roomIdx > 0) && (it->GetHitInst()!=NULL) )
			{
				CEntity* pHitEntity  = reinterpret_cast<CEntity*>(it->GetHitInst()->GetUserData());
				CInteriorProxy* pInteriorProxy = CInteriorProxy::GetInteriorProxyFromCollisionData(pHitEntity, &it->GetHitInst()->GetPosition());
				Assert(pInteriorProxy);

				CInteriorInst* pIntInst = NULL;
				if (pInteriorProxy){
					pIntInst = pInteriorProxy->GetInteriorInst();
				}

				if(pIntInst && pIntInst->CanReceiveObjects())
				{
					pDestInteriorInst = pInteriorProxy->GetInteriorInst();
					destRoomIdx = roomIdx;
				} else
				{
					return(false);			// quit immediately if interior not ready
				}
			}
			break;
		}
	}

	// we're done with this entry so drop it
	top.id = INVALID_ASYNC_ENTRY;

	if(!top.wheelResults[0][0].GetHitDetected())
	{
		failed = 4;
	}
	else
	{
		Assert(top.matrix->IsOrthonormal());
	}

	// FA: temp to catch floating cars issue
	if(hitPos)
	{
		hitPos[0] = 0.f;
		hitPos[1] = 0.f;
		hitPos[2] = 0.f;
		hitPos[3] = 0.f;
		hitPos[4] = top.matrix->d.z;
	}
	if (top.wheelResults[0][0].GetHitDetected())
	{
		if (hitPos)
			hitPos[0] = top.wheelResults[0][0].GetHitPosition().z;
		failed |= 1 << 8;
	}
	if (top.wheelResults[1][0].GetHitDetected())
	{
		if (hitPos)
			hitPos[1] = top.wheelResults[1][0].GetHitPosition().z;
		failed |= 1 << 9;
	}
	if (top.wheelResults[2][0].GetHitDetected())
	{
		if (hitPos)
			hitPos[2] = top.wheelResults[2][0].GetHitPosition().z;
		failed |= 1 << 10;
	}
	if (top.wheelResults[3][0].GetHitDetected())
	{
		if (hitPos)
			hitPos[3] = top.wheelResults[3][0].GetHitPosition().z;
		failed |= 1 << 11;
	}

	delete[] top.wheelResults;
	top.wheelResults = NULL;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlaceOnRoadAdjustInternal
// PURPOSE :  If the car is placed kinda on the road this function will
//			  do it properly
/////////////////////////////////////////////////////////////////////////////////

bool CAutomobile::PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression)
{
	ResetSuspension();
	Matrix34 Mat = MAT34V_TO_MATRIX34(GetMatrix());
	CInteriorInst*	pDestInteriorInst = 0;		
	s32			destRoomIdx = -1;
	bool setWaitForAllCollisionsBeforeProbe = false;

	float fHeightSampleRangeUp = IsPlaceOnRoadHeightOverriden() ? GetOverridenPlaceOnRoadHeight() : HeightSampleRangeUp;

	bool bResult = PlaceOnRoadProperly(this, &Mat, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, GetModelIndex(), GetCurrentPhysicsInst(), 
																false, NULL, NULL, 0, fHeightSampleRangeUp, HeightSampleRangeDown);

	if(bResult && !bJustSetCompression)
	{
		ResetOverridenPlaceOnRoadHeight();
		SetMatrix(Mat);
	}
	return bResult;
}

dev_float	AutoLights_Pos_PullTowardCam = 0.2f;
dev_float	AutoLights_White_PullTowardCam = 0.2f;

void CAutomobile::DoWhiteLightEffects(s32 boneIdx, const ConfigVehicleWhiteLightSettings &lightParam, bool frontLight, float distFade)
{
	CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
	const s32 boneId = pModelInfo->GetBoneIndex(boneIdx);
	if (boneId > -1)
	{
		Matrix34 worldMtx;
		GetGlobalMtx(boneId, worldMtx);

		const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
		float strength = Lerp(distFade,lightParam.nearStrength,lightParam.farStrength) * currKeyframe.GetVar(TCVAR_SPRITE_BRIGHTNESS);
		float size = Lerp(distFade,lightParam.nearSize,lightParam.farSize) * currKeyframe.GetVar(TCVAR_SPRITE_SIZE);

		g_coronas.Register(RCC_VEC3V(worldMtx.d),
			size,
			Color32(lightParam.color),
			strength,
			AutoLights_White_PullTowardCam,
			Vec3V(V_ZERO),
			1.0f,
			CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
		    CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
			CORONA_DONT_REFLECT);

		static dev_float innerConeAngle = 0;
		static dev_float outerConeAngle = 90.0f;
		static dev_float lightOffsetC = 0.75f;

		const float sign = (true == frontLight) ? 1.0f : 0.0f;

		const float fade = (0.5f - distFade) * 2.0f;

		Vector3 worldDir = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(-Vec3V(V_Z_AXIS_WZERO)));
		Vector3 worldTan = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(Vec3V(V_Y_AXIS_WZERO)));

		fwInteriorLocation interiorLocation = this->GetInteriorLocation();

		CLightSource* pLightSource = CAsyncLightOcclusionMgr::AddLight();
		if (pLightSource)
		{
			pLightSource->Reset();
			pLightSource->SetCommon(
				LIGHT_TYPE_SPOT, 
				LIGHTFLAG_VEHICLE | LIGHTFLAG_NO_SPECULAR, 
				worldMtx.d + sign * lightOffsetC * VEC3V_TO_VECTOR3(GetTransform().GetC()), 
				VEC3V_TO_VECTOR3(lightParam.color),
				lightParam.intensity * fade, 
				LIGHT_ALWAYS_ON);
			pLightSource->SetDirTangent(worldDir, worldTan);
			pLightSource->SetRadius(lightParam.radius);
			pLightSource->SetSpotlight(innerConeAngle, outerConeAngle);
			pLightSource->SetInInterior(interiorLocation);
			// NOTE: we don't want to call Lights::AddSceneLight directly - the AsyncLightOcclusionMgr will handle that for us
		}
	}
}

dev_float lightOffset_Mav = 0.5f;
dev_float lightOffset_Ann = 0.0f;

void CAutomobile::DoPosLightEffects(s32 boneIdx, const ConfigVehiclePositionLightSettings &lightParam, bool isRight, bool isAnnihilator, float distFade)
{
	CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
	const s32 boneId = pModelInfo->GetBoneIndex(boneIdx);
	if (boneId > -1)
	{
		Matrix34 worldMtx;
		GetGlobalMtx(boneId, worldMtx);

		float sign;
		Vec3V col;
		if( isRight )
		{
			sign = 1.0f;
			col = lightParam.rightColor;
		}
		else
		{
			sign = -1.0f;
			col = lightParam.leftColor;
		}
		
		const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
		float strength = Lerp(distFade,lightParam.nearStrength,lightParam.farStrength) * currKeyframe.GetVar(TCVAR_SPRITE_BRIGHTNESS);
		float size = Lerp(distFade,lightParam.nearSize,lightParam.farSize) * currKeyframe.GetVar(TCVAR_SPRITE_SIZE);

		g_coronas.Register(RCC_VEC3V(worldMtx.d),
			size,
			Color32(col),
			strength,
			AutoLights_Pos_PullTowardCam,
			Vec3V(V_ZERO),
			1.0f,
			CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
		    CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
			CORONA_DONT_REFLECT);

		static dev_float innerConeAngle = 0;
		static dev_float outerConeAngle = 90.0f;

		float lightOffset;
		float intensity;
		const float fade = (0.5f - distFade) * 2.0f;

		if( isAnnihilator	)
		{
			lightOffset = lightOffset_Ann;
			intensity = lightParam.intensity_typeA * fade;
		}
		else
		{
			lightOffset = lightOffset_Mav;
			intensity = lightParam.intensity_typeB * fade;
		}

		Vector3 worldDir = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(-Vec3V(V_Z_AXIS_WZERO)));
		Vector3 worldTan = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(Vec3V(V_Y_AXIS_WZERO)));

		CLightSource light(
			LIGHT_TYPE_SPOT, 
			LIGHTFLAG_VEHICLE | LIGHTFLAG_NO_SPECULAR,
			worldMtx.d + sign * lightOffset * VEC3V_TO_VECTOR3(GetTransform().GetA()) + lightOffset * VEC3V_TO_VECTOR3(GetTransform().GetC()),
			VEC3V_TO_VECTOR3(col), 
			intensity * (1.0f - distFade), 
			LIGHT_ALWAYS_ON);
		light.SetDirTangent(worldDir, worldTan);
		light.SetRadius(lightParam.radius);
		light.SetSpotlight(innerConeAngle, outerConeAngle);
		light.SetInInterior(GetInteriorLocation());
		light.SetFalloffExponent(32.0f);
		Lights::AddSceneLight(light);
	}
}


bool CAutomobile::ShouldUpdateVehicleDamageEveryFrame() const
{
	// The petrol tank fires need to be updated every frame, otherwise they get
	// restarted and don't work properly.
	if(GetVehicleDamage()->GetPetrolTankOnFire())
	{
		return true;
	}

	// Need update the damage when there are collision impacts
	if(GetFrameCollisionHistory()->GetCollisionImpulseMagSum() > 0.0f)
	{
		return true;
	}

	return false;
}

void CAutomobile::ModifyControlsIfDucked(const CPed* pDriver, float fTimeStep)
{
	TUNE_GROUP_BOOL(DUCK_IN_VEHICLE_TUNE, DISABLE_CONTROL_MODIFICATION, false);
	if (DISABLE_CONTROL_MODIFICATION)
		return;

	if (!pDriver || !pDriver->IsLocalPlayer())
		return;

	CPlayerInfo* pPlayerInfo = pDriver->GetPlayerInfo();
	if (!pPlayerInfo)
		return;

	const bool bIsDucking = pDriver->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle);
	if (!bIsDucking)
	{
		return;
	}
	
	const CTaskMotionInAutomobile* pAutomobileTask = static_cast<const CTaskMotionInAutomobile*>(pDriver->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
	if (!pAutomobileTask)
	{
		return;
	}

	// Allow a brief period of total control when first ducking, before penalising the player if they continue to duck
	const float fTimeDucked = pAutomobileTask ? pAutomobileTask->GetTimeDucked() : 0.0f;
	TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, NO_CONTROL_MODIFICATION_TIME, 3.0f, 0.0f, 10.0f, 0.01f);
	if (fTimeDucked < NO_CONTROL_MODIFICATION_TIME)
		return;

	TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, LAGGED_CONTROL_BLEND_DURATION, 0.125f, 0.0f, 10.0f, 0.01f);
	const float fIdleBlend = 1.0f - Clamp((fTimeDucked - NO_CONTROL_MODIFICATION_TIME) / LAGGED_CONTROL_BLEND_DURATION, 0.0f, 1.0f);

	// Lag steering control input
	const float fLaggedSteerControl = pPlayerInfo->GetLaggedSteerControl();
	float fSteerControlUnmodified = m_vehControls.GetSteerAngle();
	float fSteerControlModified = fSteerControlUnmodified;

	TUNE_GROUP_BOOL(DUCK_IN_VEHICLE_TUNE, DISABLE_CONTROL_LAGGING, false);
	float fControlLaggingBlendingRate = -1.0f;
	if (!DISABLE_CONTROL_LAGGING)
	{
		TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, AUTOMOBILE_MIN_LAGGING_BLEND_RATE, 5.0f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, AUTOMOBILE_MAX_LAGGING_BLEND_RATE, 20.0f, 0.0f, 40.0f, 0.01f);

		fControlLaggingBlendingRate = (1.0f - fIdleBlend) * AUTOMOBILE_MIN_LAGGING_BLEND_RATE + fIdleBlend * AUTOMOBILE_MAX_LAGGING_BLEND_RATE;
		fSteerControlModified = Clamp(fSteerControlModified, fLaggedSteerControl - fControlLaggingBlendingRate * fTimeStep, fLaggedSteerControl + fControlLaggingBlendingRate * fTimeStep);
		pPlayerInfo->SetLaggedSteerControl(fSteerControlModified);
	}

	// Random control input modifier after having been settled (not moving stick)
	TUNE_GROUP_BOOL(DUCK_IN_VEHICLE_TUNE, DISABLE_RANDOM_STEER_IMPULSE, false);
	float fRandomControlSteerModifier = pPlayerInfo->GetRandomControlSteerModifier();
	float fControlSettledTime = pPlayerInfo->GetControlSettledTime();
	const float fDecayTime = pPlayerInfo->GetTimeSinceApplyingRandomControl();
	TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, RANDOM_CONTROL_MODIFIER_MAG, 1.0f, 0.0f, 3.0f, 0.1f);

	if (!DISABLE_RANDOM_STEER_IMPULSE && pDriver->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
	{
		// If not applying steer impulse
		TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, IS_RANDOM_STEER_DECAYED_THRESHOLD, 0.05f, 0.0f, 1.0f, 0.01f);
		if (Abs(fRandomControlSteerModifier) < IS_RANDOM_STEER_DECAYED_THRESHOLD)
		{
			// Process settled timers
			TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, STEER_CONSIDER_SETTLED_TIME_THRESHOLD, 0.5f, 0.0f, 2.0f, 0.01f);
			TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, MAX_STEER_DELTA_FOR_SETTLED, 0.1f, 0.0f, 1.0f, 0.01f);
			const bool bWasSettled = fControlSettledTime > STEER_CONSIDER_SETTLED_TIME_THRESHOLD;
			const float fPreviousSteerValue = pPlayerInfo->GetPreviousSteerValue();
			const float fDeltaSteerAngle = Abs(fPreviousSteerValue - fSteerControlUnmodified);
			const bool bWithinSettleTolerance = fDeltaSteerAngle < MAX_STEER_DELTA_FOR_SETTLED;
			if (bWithinSettleTolerance)
			{
				fControlSettledTime += fwTimer::GetTimeStep();
			}
			else
			{
				fControlSettledTime = 0.0f;
			}
			pPlayerInfo->SetControlSettledTime(fControlSettledTime);
			pPlayerInfo->SetTimeSinceApplyingRandomControl(0.0f);

			// If was settled but move out of range initiate steer impulse
			if (bWasSettled && !bWithinSettleTolerance)
			{
				// Set a modifier impulse in the opposite direction to the steer direction
				const float fControlSteer = pDriver->GetControlFromPlayer() ? pDriver->GetControlFromPlayer()->GetVehicleSteeringLeftRight().GetNorm() : 0.0f;
				pPlayerInfo->SetRandomControlSteerModifier(fControlSteer < 0.0f ? -1.0f : 1.0f);	
			}
			else
			{
				pPlayerInfo->SetRandomControlSteerModifier(0.0f);
			}
		}
		// Else process steer impulse decay
		else
		{
			TUNE_GROUP_FLOAT(DUCK_IN_VEHICLE_TUNE, EXP_DECAY_RATE, 10.0f, 0.0f, 100.0f, 0.1f);
			fRandomControlSteerModifier = Sign(fRandomControlSteerModifier) * expf(-EXP_DECAY_RATE * fDecayTime);

			pPlayerInfo->SetRandomControlSteerModifier(fRandomControlSteerModifier);
			pPlayerInfo->SetTimeSinceApplyingRandomControl(fDecayTime + fwTimer::GetTimeStep());

			fControlSettledTime = 0.0f;
		}

		fSteerControlModified = Clamp(fSteerControlModified + RANDOM_CONTROL_MODIFIER_MAG * fRandomControlSteerModifier, -1.0f, 1.0f);
	}

	pPlayerInfo->SetPreviousSteerValue(fSteerControlUnmodified);

	// Reapply modified steering control
	m_vehControls.SetSteerAngle(fSteerControlModified);

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(DUCK_IN_VEHICLE_TUNE, DRAW_DEBUG, false);
	if (DRAW_DEBUG)
	{
		static Vector2 svDrawPos(0.1f, 0.3f);
		static float sfVerticalSpaceBetweenTexts = 0.025f;
		s32 iNumTexts = 0;
		char szText[128];

		formatf(szText, "fIdleBlend = %.2f", fIdleBlend);
		Vec3V vDrawPos(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_cyan, 100, atStringHash("DUCK_fIdleBlend"));

		formatf(szText, "fControlLaggingBlendingRate = %.2f", fControlLaggingBlendingRate);
		vDrawPos = Vec3V(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_cyan, 100, atStringHash("DUCK_fControlLaggingBlendingRate"));
		
		formatf(szText, "fRandomControlSteerModifier = %.2f (%.2f)", fRandomControlSteerModifier, RANDOM_CONTROL_MODIFIER_MAG * fRandomControlSteerModifier);
		vDrawPos = Vec3V(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_cyan, 100, atStringHash("DUCK_fRandomControlSteerModifier"));
		
		formatf(szText, "fControlSettledTime = %.2f", fControlSettledTime);
		vDrawPos = Vec3V(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_green, 100, atStringHash("DUCK_fControlSettledTime"));
		
		formatf(szText, "fSteerControlUnmodified = %.2f", fSteerControlUnmodified);
		vDrawPos = Vec3V(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_green, 100, atStringHash("DUCK_fSteerControlUnmodified"));

		formatf(szText, "fSteerControlModified = %.2f", fSteerControlModified);
		vDrawPos = Vec3V(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_green, 100, atStringHash("DUCK_fSteerControlModified"));

		formatf(szText, "fDecayTime = %.2f", fDecayTime);
		vDrawPos = Vec3V(svDrawPos.x, svDrawPos.y + sfVerticalSpaceBetweenTexts * iNumTexts++, 0.0f);
		CTask::ms_debugDraw.Add2DText(vDrawPos, szText, Color_green, 100, atStringHash("DUCK_fDecayTime"));

		static float sfModifierRenderHeight = 0.3f;
		static Vector2 svModifierDrawPosStart(0.4f, sfModifierRenderHeight);
		static Vector2 svModifierDrawPosEnd(0.5f, sfModifierRenderHeight);
		static float fMarkerHeight = 0.05f;

		CTask::ms_debugDraw.AddLine(svModifierDrawPosStart, svModifierDrawPosEnd, Color_black, 100, atStringHash("DUCK_Bar"));

		const float fHalfWidth = (svModifierDrawPosEnd.x - svModifierDrawPosStart.x) * 0.5f;
		const float fMidXPoint = svModifierDrawPosStart.x + fHalfWidth;
		const float fMarkerXPos = fMidXPoint + fRandomControlSteerModifier * fHalfWidth;
		const float fMarkerYStartPos = sfModifierRenderHeight - fMarkerHeight;
		const float fMarkerYEndPos = sfModifierRenderHeight + fMarkerHeight;

		CTask::ms_debugDraw.AddLine(Vector2(fMarkerXPos, fMarkerYStartPos), Vector2(fMarkerXPos, fMarkerYEndPos), Color32(0, (int)(255.0f*fRandomControlSteerModifier), 0), 100, atStringHash("DUCK_Marker"));
	}
#endif // DEBUG_DRAW
}

void CAutomobile::EnableBurnoutMode(bool bEnableBurnout)
{ 
	m_nAutomobileFlags.bBurnoutModeEnabled = bEnableBurnout; 
	if(bEnableBurnout)
	{
		// Turn off the hand brake or else we won't get drive force in CTransmission::ProcessEngine
		SetHandBrake(false);
	}
}

void CAutomobile::SetReduceGripLevel( int reducedGripLevel )
{
	for( int i = 0; i < m_nNumWheels; i++ )
	{
		if( m_ppWheels[ i ] )
		{
			m_ppWheels[ i ]->SetReducedGripLevel( reducedGripLevel );
		}
	}
}

void CAutomobile::SetReducedSuspensionForce( bool bReduceSuspensionForce )
{
	for( int i = 0; i < m_nNumWheels; i++ )
	{
		if( m_ppWheels[ i ] )
		{
			m_ppWheels[ i ]->SetReducedSuspensionForce( bReduceSuspensionForce );
		}
	}

	//Activate physics so the vehicle moves to its correct ride hide after this has been called.
	ActivatePhysics();

	if(m_VehicleAudioEntity)
	{
		m_VehicleAudioEntity->OnReducedSuspensionForceSet(bReduceSuspensionForce);
	}
}

bool CAutomobile::GetReducedSuspensionForce()
{
	for(int i = 0; i < m_nNumWheels; i++)
	{
		if(m_ppWheels[i])
		{
			return m_ppWheels[i]->GetReducedSuspensionForce();
		}
	}
	return false;
}


static dev_float fHandbrakeFallOffMult = 0.0025f;
static dev_float fHandbrakeFalloffMin = 0.5f;	// Handbrake decays from 1x to this value
float CAutomobile::GetHandBrakeForce() const 
{
	float fTime = (float) (fwTimer::GetTimeInMilliseconds() - m_uHandbrakeOnTime);
	Assert(fTime >= 0.0f);

	float fMult = exp(-fHandbrakeFallOffMult * fTime);
	fMult = (fMult * (1.0f - fHandbrakeFalloffMin) ) + fHandbrakeFalloffMin;
	return pHandling->m_fHandBrakeForce * fMult ;
}

static dev_float sfHandbrakeGripFalloffTime = 1500.0f;	// milliseconds
static dev_float sfHandbrakeGripMultBase = 0.65f;	// We drop to this much grip as soon as handbrake is pulled
float CAutomobile::GetHandBrakeGripMult() const
{
	float fGripMult = 1.0f;

	float fTime = (float) (fwTimer::GetTimeInMilliseconds() - m_uHandbrakeOnTime);

	if(fTime >= 0.0f && fTime < sfHandbrakeGripFalloffTime)
	{
		fTime /= sfHandbrakeGripFalloffTime;
		fGripMult = sfHandbrakeGripMultBase + (1.0f - sfHandbrakeGripMultBase) * fTime;
	}

	return fGripMult;
}

void CAutomobile::ResetHydraulics()
{
	if( HasHydraulicSuspension() )
	{
		for( int i = 0; i < GetNumWheels(); i++ )
		{
			CWheel* pWheel = GetWheel( i );

			if( pWheel )
			{
				pWheel->SetSuspensionRaiseAmount( 0.0f );
				pWheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
				pWheel->m_bSuspensionForceModified = false;
				pWheel->SetSuspensionHydraulicTargetState(WHS_IDLE);
			}
		}
		
		m_nVehicleFlags.bAreHydraulicsAllRaised = false;
	}
}

RAGETRACE_DECL(CAutomobile_ProcessProbes);

PF_GROUP(CAutomobileProbes);
PF_PAGE(ProcessProbes, "CAutomobile::ProcessProbes");
PF_LINK(ProcessProbes, CAutomobileProbes);
PF_TIMER(Probes, CAutomobileProbes);

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION		:	ProcessProbes
// PURPOSE		:	Handles the wheel intersections against the ground (whether
//					the ground surface is real or virtual) and processes the
//					wheel physics.
// PARAMETERS	:	None.
// RETURNS		:	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CAutomobile::ProcessProbes(const Matrix34& testMat)
{
	PF_START(Probes);
	if( m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy) || 
		GetNumWheels() <= 0)
	{
		PF_STOP(Probes);
		return;
	}

	bool bIsDummy = IsDummy();
	bool bIsInactivePlayback = (CVehicleRecordingMgr::IsPlaybackGoingOnForCar(this) || GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE) && CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex());

	RAGETRACE_SCOPE(CAutomobile_ProcessProbes);

	if(!bIsDummy && !bIsInactivePlayback && GetWheel(0)->GetFragChild() > 0)
	{
		m_nVehicleFlags.bDontSleepOnThisPhysical = false;
		for(int i=0; i<GetNumWheels(); i++)
			GetWheel(i)->ProcessImpactResults();

		PF_STOP(Probes);
		return;
	}

	if(GetIsAircraft() && GetDriver() && !GetDriver()->IsAPlayerPed() && !m_bIgnoreWorldHeightMapForWheelProbes )
	{
		Vec3V vPosition = GetTransform().GetPosition();
		float fMapMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPosition.GetXf(), vPosition.GetYf());

		Vec3V vBBMinZGlobal = GetTransform().Transform(Vec3V(0.0f, 0.0f, GetBoundingBoxMin().z));
		if(vBBMinZGlobal.GetZf() > fMapMaxZ)
		{
			for(int i=0; i<GetNumWheels(); i++)
			{
				GetWheel(i)->Reset(true);
			}
			PF_STOP(Probes);
			return;
		}
	}

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	static phSegment testSegments[NUM_VEH_CWHEELS_MAX];
	static WorldProbe::CShapeTestFixedResults<NUM_VEH_CWHEELS_MAX> probeResults;
	WorldProbe::CShapeTestHitPoint* resultsToProcess = &probeResults[0];

	// Init the segment and intersection data.
	probeResults.Reset();

	// Setup the segments to test against the ground surface.
	for(int i=0; i<GetNumWheels(); i++)
	{
		const CWheel& wheel = *GetWheel(i);
		if(wheel.GetFragChild() == -1 && InheritsFromHeli())
		{
			// Rotate heli skid probes inwards to prevent large vertical changes in ground height from causing oscillation. 
			const ScalarV probeRotationAngle = ScalarVFromF32(25.0f*DtoR);
			const Vec3V localBot = RCC_VEC3V(wheel.GetProbeSegment().B);
			const Vec3V localTopOriginal = RCC_VEC3V(wheel.GetProbeSegment().A);
			const ScalarV inwardsRotationAngle = SelectFT(IsGreaterThan(localBot.GetX(),ScalarV(V_ZERO)),probeRotationAngle,-probeRotationAngle);
			const Vec3V localTopRotated = rage::Add(localBot, RotateAboutYAxis(Subtract(localTopOriginal,localBot),inwardsRotationAngle));
			testSegments[i].Set(VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(testMat),localTopRotated)), VEC3V_TO_VECTOR3(Transform(RCC_MAT34V(testMat),localBot)));
		}
		else
		{
			GetWheel(i)->ProbeGetTransfomedSegment(&testMat, testSegments[i]);
		}
	}

	// Dummy vehicles drive on a fixed simplified surface.
	if(bIsDummy)
	{
		Assert( !m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy) );

		const CVehicleFollowRouteHelper* pFollowRoute = NULL;
		const CVehicleNodeList* pNodeList = NULL;

		// Trailers use their parent's follow route
		if (m_nVehicleFlags.bHasParentVehicle && CVehicle::IsEntityAttachedToTrailer(this) && GetDummyAttachmentParent()
			&& GetDummyAttachmentParent()->GetDummyAttachmentParent())
		{
			pFollowRoute = GetDummyAttachmentParent()->GetDummyAttachmentParent()->GetIntelligence()->GetFollowRouteHelper();
			pNodeList = GetDummyAttachmentParent()->GetDummyAttachmentParent()->GetIntelligence()->GetNodeList();
		}
		else if(m_nVehicleFlags.bHasParentVehicle && InheritsFromTrailer() && GetDummyAttachmentParent())
		{
			pFollowRoute = GetDummyAttachmentParent()->GetIntelligence()->GetFollowRouteHelper();
			pNodeList = GetDummyAttachmentParent()->GetIntelligence()->GetNodeList();
		}
		else
		{
			pFollowRoute = GetIntelligence()->GetFollowRouteHelper();
			pNodeList = GetIntelligence()->GetNodeList();
		}

		bool bUseTwoVirtualRoadCenters = false;

		const Vec3V vVehCenter = GetVehiclePosition();
		const ScalarV fVehBoxMaxY = RCC_VEC3V(GetBoundingBoxMax()).GetY();
		const ScalarV fVehBoxMinY = RCC_VEC3V(GetBoundingBoxMin()).GetY();
		const Vec3V vVehForward = GetVehicleForwardDirection();
		static const ScalarV fOffsetScale = ScalarVConstant<FLOAT_TO_INT(0.75f)>(); // Go most of the way toward the ends of the bounding box.
		const Vec3V vVehFront = vVehCenter + (fVehBoxMaxY * fOffsetScale) * vVehForward;
		const Vec3V vVehRear = vVehCenter + (fVehBoxMinY * fOffsetScale) * vVehForward;
		//const Vec3V vOffset(0.0f,0.0f,3.0f);
		//grcDebugDraw::Line(vVehFront,vVehFront+vOffset,Color_red,-1);
		//grcDebugDraw::Line(vVehRear,vVehRear+vOffset,Color_red,-1);

		static const ScalarV fLargeVehThreshold = ScalarV(V_NINE);

		if(CVehicleAILodManager::ms_bAllowAltRouteInfoForRearWheels && IsGreaterThanAll(fVehBoxMaxY - fVehBoxMinY, fLargeVehThreshold))
		{
			if(IsGreaterThanAll((fVehBoxMaxY-fVehBoxMinY),fLargeVehThreshold))
			{
				bUseTwoVirtualRoadCenters = true;
			}
		}

		if(pFollowRoute && pFollowRoute->GetNumPoints() > 1)
		{
			CVirtualRoad::TRouteInfo frontRouteInfo;
			CVirtualRoad::TRouteInfo rearRouteInfo;

			const bool frontRouteInfoCreated = CVirtualRoad::HelperCreateRouteInfoFromFollowRouteAndCenterPos(*pFollowRoute, vVehCenter, frontRouteInfo, ShouldTryToUseVirtualJunctionHeightmaps(), false);

			if(frontRouteInfoCreated)
			{
				bUseTwoVirtualRoadCenters |= (frontRouteInfo.m_bConsiderJunction && CVehicleAILodManager::ms_bAllowAltRouteInfoForRearWheels);

				if(bUseTwoVirtualRoadCenters)
				{
					const bool rearRouteInfoCreated = CVirtualRoad::HelperCreateRouteInfoFromFollowRouteAndCenterPos(*pFollowRoute, vVehRear, rearRouteInfo, ShouldTryToUseVirtualJunctionHeightmaps(), false);

					if(rearRouteInfoCreated)
					{
						CVirtualRoad::TestProbesToVirtualRoadTwoRouteInfos(frontRouteInfo, vVehFront, rearRouteInfo, vVehRear, probeResults, testSegments, GetNumWheels(), GetTransform().GetC().GetZ());
					}
				}
				else
				{
					CVirtualRoad::TestProbesToVirtualRoadRouteInfo(frontRouteInfo, probeResults, testSegments, GetNumWheels(), GetTransform().GetC().GetZ());
				}
			}

			HandleDummyProbesBottomingOut(probeResults);
			UpdateDummyConstraints();
		}
		else if (pNodeList && pNodeList->FindNumPathNodes() > 1)
		{
			Assertf(0,"CAutomobile::ProcessProbes with no follow route.");
#if __DEV
			GetIntelligence()->PrintTasks();

			//print the nodelist
			Displayf("Target Index: %d", pNodeList->GetTargetNodeIndex());
			for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
			{
				Displayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, pNodeList->GetPathNodeAddr(i).GetRegion(), pNodeList->GetPathNodeAddr(i).GetIndex(), i, pNodeList->GetPathLinkIndex(i));
			}
#endif //__DEV
			if(bUseTwoVirtualRoadCenters)
			{
				CVirtualRoad::TestProbesToVirtualRoadNodeListAndTwoPoints(*pNodeList, vVehFront, vVehRear, probeResults, testSegments, GetNumWheels(), GetTransform().GetC().GetZ() );
			}
			else
			{
				CVirtualRoad::TestProbesToVirtualRoadNodeListAndCenterPos(*pNodeList, vVehCenter, probeResults, testSegments, GetNumWheels(), GetTransform().GetC().GetZ() );
			}
			HandleDummyProbesBottomingOut(probeResults);
			UpdateDummyConstraints();
		}
#if (0 && (AI_OPTIMISATIONS_OFF || VEHICLE_OPTIMISATIONS_OFF || AI_VEHICLE_OPTIMISATIONS_OFF))
		else
		{
#if __DEV
			GetIntelligence()->PrintTasks();
#endif //__DEV
			Assertf(0, "CAutomobile::ProcessProbes with no valid path. Is there a navmesh path or something else we should be using here?");
		}
#endif
	}
	else 
	{
		int nTestTypes = (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
		static bool USE_PROBE_BATCH = false;
		if(USE_PROBE_BATCH)
		{
			WorldProbe::CShapeTestBatchDesc batchProbeDesc;
			batchProbeDesc.SetExcludeEntity(this);
			batchProbeDesc.SetIncludeFlags(nTestTypes);
			batchProbeDesc.SetTypeFlags(ArchetypeFlags::GTA_WHEEL_TEST);
			ALLOC_AND_SET_PROBE_DESCRIPTORS(batchProbeDesc,GetNumWheels());
			for(int i=0; i<GetNumWheels(); i++)
			{
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&probeResults);
				probeDesc.SetFirstFreeResultOffset(i);
				probeDesc.SetMaxNumResultsToUse(1);
				probeDesc.SetStartAndEnd(testSegments[i].A, testSegments[i].B);

				batchProbeDesc.AddLineTest(probeDesc);
			}
			WorldProbe::GetShapeTestManager()->SubmitTest(batchProbeDesc);
		}
		else
		{
			if(ms_bUseAsyncWheelProbes)
			{
				const int numWheels = GetNumWheels();
				if(m_pWheelProbeResults == NULL)
				{
					// Allocate the results structure for async tests
					m_pWheelProbeResults = rage_new WorldProbe::CShapeTestSingleResult[numWheels];
					Assert(m_pWheelProbePreviousResults == NULL);
					m_pWheelProbePreviousResults = rage_new WorldProbe::CShapeTestHitPoint[numWheels];
				}
				resultsToProcess = m_pWheelProbePreviousResults;

				for(int wheelIndex = 0; wheelIndex < numWheels; ++wheelIndex)
				{
					WorldProbe::CShapeTestSingleResult& wheelProbeResult = m_pWheelProbeResults[wheelIndex];
					WorldProbe::CShapeTestHitPoint& wheelProbePreviousResult = m_pWheelProbePreviousResults[wheelIndex];

					// Check if we're in the middle of a test
					if(!wheelProbeResult.GetWaitingOnResults())
					{
						// If we aren't waiting on results but don't have results ready it means we have no results to process.
						// We need to do a synchronous test first so the wheels have something this frame. There is no point doing
						//  an async test at the same time since it would be the exact same probes. 
						bool performTestSynchronously = true;
						if(wheelProbeResult.GetResultsReady())
						{
							// We sent off a probe and got results for it
							wheelProbePreviousResult = wheelProbeResult[0];
							performTestSynchronously = false;
						}

						if(GetVehicleType() == VEHICLE_TYPE_HELI && ContainsLocalPlayer())
						{
							Vector3 vVelocity = GetVelocity();
							static dev_float sfSuspensionLimitMult = 0.5f;
							if(vVelocity.z*fwTimer::GetTimeStep() <= -pHandling->m_fSuspensionUpperLimit*sfSuspensionLimitMult)// If we are moving down faster than our suspension length use a sync test, otherwise we may miss the ground.
							{
								performTestSynchronously = true;
							}

							// Use sync probe if we sitting on a vehicle, this will prevent inconsistent probe result
							if(wheelProbePreviousResult.GetHitEntity() && wheelProbePreviousResult.GetHitEntity()->GetIsTypeVehicle())
							{
								performTestSynchronously = true;
							}
						}

						// There isn't a test in progress so we should start one
						WorldProbe::CShapeTestProbeDesc probeDesc;
						probeDesc.SetResultsStructure(&wheelProbeResult);
						probeDesc.SetExcludeEntity(this);
						probeDesc.SetIncludeFlags(nTestTypes);
						probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WHEEL_TEST);
						probeDesc.SetContext(WorldProbe::EVehicle);
						probeDesc.SetStartAndEnd(testSegments[wheelIndex].A, testSegments[wheelIndex].B);
						WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, performTestSynchronously ? WorldProbe::PERFORM_SYNCHRONOUS_TEST : WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
						
						if(performTestSynchronously)
						{
							// If we did a synchronous test, store the results right away so we can use them this frame
							wheelProbePreviousResult = wheelProbeResult[0];
						}
					}

					// It looks like we are touching the ground, but the probe out of date, we need to recompute the intersection
					if(wheelProbePreviousResult.IsAHit())
					{
						// Treat the most recent ground as an infinite plane, and intersect the current suspension probe with it
						const Vec3V groundNormal = wheelProbePreviousResult.GetNormal();
						const Vec3V pointOnGround = wheelProbePreviousResult.GetPosition();

						const Vec3V probeStart = RCC_VEC3V(testSegments[wheelIndex].A);
						const Vec3V probeStartToEnd = Subtract(RCC_VEC3V(testSegments[wheelIndex].B),probeStart);

						const ScalarV tValue = InvScaleSafe(Dot(groundNormal, Subtract(pointOnGround,probeStart)), Dot(groundNormal, probeStartToEnd), ScalarV(V_NEGONE));

						if(And(IsGreaterThanOrEqual(tValue,ScalarV(V_ZERO)), IsLessThanOrEqual(tValue,ScalarV(V_ONE))).Getb())
						{
							// The current suspension hit the infinite plane so update the intersection
							wheelProbePreviousResult.SetT(tValue);
							wheelProbePreviousResult.SetPosition(AddScaled(probeStart,probeStartToEnd,tValue));
						}
						else
						{
							// The suspension didn't hit the previous ground plane so just clear it
							wheelProbePreviousResult.Reset();
						}
					}
				}
			}
			else
			{
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&probeResults);
				probeDesc.SetExcludeEntity(this);
				probeDesc.SetIncludeFlags(nTestTypes);
				probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WHEEL_TEST);

				for(int i=0; i<GetNumWheels(); i++)
				{
					probeDesc.SetStartAndEnd(testSegments[i].A, testSegments[i].B);
					probeDesc.SetFirstFreeResultOffset(i);
					probeDesc.SetMaxNumResultsToUse(1);
					WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
				}	
			}
		}
	}

	bool bHighCompression = false;
	bool bNoCompression = false;
	// Process the results of the probes.
	for(int i=0; i<GetNumWheels(); i++)
	{
#if __BANK
		if(ms_bDrawWheelProbeResults && resultsToProcess[i].IsAHit())
		{
			const Vec3V resultPosition = resultsToProcess[i].GetPosition();
			const Vec3V resultNormal = resultsToProcess[i].GetNormal();
			const float normalDrawLength = 1.0f;
			const float drawRadius = 0.06f;
			grcDebugDraw::Sphere(resultPosition, drawRadius, Color_red);
			grcDebugDraw::Arrow(resultPosition,AddScaled(resultPosition,resultNormal,ScalarVFromF32(normalDrawLength)),drawRadius,Color_red);
		}
#endif // __BANK
		GetWheel(i)->ProbeProcessResults(&testMat, resultsToProcess[i]);

		if(NetworkInterface::IsGameInProgress())
		{
			if(GetWheel(i)->GetCompression() > GetWheel(i)->GetSuspensionLength())
			{
				bHighCompression = true;
			}
			else if(!GetWheel(i)->GetIsTouching())
			{
				bNoCompression = true;
			}
		}

	}

	if(NetworkInterface::IsGameInProgress() && bHighCompression && bNoCompression && IsDummy())// Convert to real if we have one wheel through the floor and one wheel off the ground. Also limiting this to network games for the time being.
	{
		TryToMakeFromDummy(true);
	}

	// Check if we should move the vehicle bounding box to prevent ground penetrations.
	if(CVehicleAILodManager::ms_convertUseFixupCollisionWithWheelSurface)
	{
		if(bIsDummy)
		{
			FixupIntersectionWithWheelSurface(probeResults);
		}
	}

	// Mark the we processed the wheel collisions.
	m_nVehicleFlags.bVehicleColProcessed = true;
	PF_STOP(Probes);
}

static dev_float  svVelTolerance2 = 0.005f;
bool CAutomobile::IsInAir(bool bCheckForZeroVelocity) const
{
	if (HasContactWheels() || GetIsInWater() || (GetVelocityIncludingReferenceFrame().Mag2() < svVelTolerance2 && bCheckForZeroVelocity))
		return false;

	if(GetIsAnyFixedFlagSet())
		return false;
	
	return true;
}		


static atHashString sHashTruck("TRUCK",0x428100C5);

void CAutomobile::InitDoors()
{
	CVehicle::InitDoors();
	Assert(GetNumDoors()==0);
	m_pDoors = m_aCarDoors;
	m_nNumDoors = 0;

	float fOpenDoorAngle = 0.4f*PI;
	if(m_nVehicleFlags.bIsBus) 
		fOpenDoorAngle = 0.46f*PI;

	if(GetBoneIndex(VEH_DOOR_DSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_F, -fOpenDoorAngle, 0.0f, CCarDoor::AXIS_Z|CCarDoor::WILL_LOCK_SWINGING);
		if(pHandling->dFlags & DF_DRIVER_SIDE_FRONT_DOOR)
		{
			m_pDoors[m_nNumDoors].SetFlag(CCarDoor::DONT_BREAK);
		}
		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_PSIDE_F) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_F, fOpenDoorAngle, 0.0f, CCarDoor::AXIS_Z|CCarDoor::WILL_LOCK_SWINGING);
		if(pHandling->dFlags & DF_DRIVER_PASSENGER_SIDE_FRONT_DOOR)
		{
			m_pDoors[m_nNumDoors].SetFlag(CCarDoor::DONT_BREAK);
		}
		m_nNumDoors++;
	}

	fOpenDoorAngle = 0.4f*PI;
	if(m_nVehicleFlags.bIsVan)
		fOpenDoorAngle = HALF_PI;

	if(GetBoneIndex(VEH_DOOR_DSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_DSIDE_R, -fOpenDoorAngle, 0.0f, CCarDoor::AXIS_Z|CCarDoor::WILL_LOCK_SWINGING);
		if(pHandling->dFlags & DF_DRIVER_SIDE_REAR_DOOR)
		{
			m_pDoors[m_nNumDoors].SetFlag(CCarDoor::DONT_BREAK);
		}

		m_nNumDoors++;
	}
	if(GetBoneIndex(VEH_DOOR_PSIDE_R) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_DOOR_PSIDE_R, fOpenDoorAngle, 0.0f, CCarDoor::AXIS_Z|CCarDoor::WILL_LOCK_SWINGING);
		if(pHandling->dFlags & DF_DRIVER_PASSENGER_SIDE_REAR_DOOR)
		{
			m_pDoors[m_nNumDoors].SetFlag(CCarDoor::DONT_BREAK);
		}
		m_nNumDoors++;
	}
	
	bool linkBoot2 = false;
	if( GetBoneIndex( VEH_BOOT_2 ) > -1 )
	{
		u32 uInitFlags = CCarDoor::AXIS_X | CCarDoor::GAS_BOOT;
		if( GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_FRONT_BOOT ) )
		{
			uInitFlags |= CCarDoor::AERO_BONNET;
		}

		m_pDoors[ m_nNumDoors ].Init( this, VEH_BOOT_2, 0.1f*PI, 0.0f, uInitFlags );
		if( pHandling->dFlags & DF_BOOT )
		{
			m_pDoors[ m_nNumDoors ].SetFlag( CCarDoor::DONT_BREAK );
		}

		if( !GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_FRONT_BOOT ) )
		{
			linkBoot2 = true;
		}
		m_nNumDoors++;
	}

	if(GetBoneIndex(VEH_BONNET) > -1)
	{
		m_pDoors[m_nNumDoors].Init(this, VEH_BONNET, 0.3f*PI, 0.0f, CCarDoor::AXIS_X|CCarDoor::AERO_BONNET);
		if(pHandling->dFlags & DF_BONNET)
		{
			m_pDoors[m_nNumDoors].SetFlag(CCarDoor::DONT_BREAK);
		}

		if( linkBoot2 &&
			GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_TWO_BONNET_BONES ) )
		{
			m_pDoors[ m_nNumDoors ].SetLinkedDoor( VEH_BOOT_2 );
		}

		m_nNumDoors++;
	}


	if(GetBoneIndex(VEH_BOOT) > -1)
	{
		u32 uInitFlags = CCarDoor::AXIS_X|CCarDoor::GAS_BOOT;
		if(GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_FRONT_BOOT))
		{
			uInitFlags |= CCarDoor::AERO_BONNET;
		}

		m_pDoors[m_nNumDoors].Init(this, VEH_BOOT, -0.3f*PI, 0.0f, uInitFlags);
		if(pHandling->dFlags & DF_BOOT)
		{
			m_pDoors[m_nNumDoors].SetFlag(CCarDoor::DONT_BREAK);
		}

		if( linkBoot2 &&
			!GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_TWO_BONNET_BONES ) )
		{
			m_pDoors[m_nNumDoors].SetLinkedDoor( VEH_BOOT_2 );
		}

		m_nNumDoors++;
	}


}


bool CAutomobile::IsCarFloating()
{
	if(GetIsInWater() &&
		m_fTimeInWater > 0.0f && 
		m_fTimeInWater < AUTOMOBILE_SINK_TIME)
		return true;
	else
		return false;
}


void CAutomobile::SetMinHydraulicsGroundClearance()
{
	if( (float)GetVariationInstance().GetHydraulicsModifier() > 0.0f )
	{
		bool hydraulicsActive = (	m_nVehicleFlags.bPlayerModifiedHydraulics || 
									m_nAutomobileFlags.bHydraulicsBounceRaising || 
									m_nAutomobileFlags.bHydraulicsBounceLanding || 
									m_nAutomobileFlags.bHydraulicsJump );

		if( !hydraulicsActive )
		{
			m_nAutomobileFlags.bForceUpdateGroundClearance = 1;
		}
	}
}

void CAutomobile::StreamInParachute()
{
	if(GetDriver())
	{
		//Check if the parachute model index is invalid.
		if(m_iParachuteModelIndex == fwModelId::MI_INVALID)
		{
			fwModelId iModelId;
			CModelInfo::GetBaseModelInfoFromHashKey( GetParachuteModelOverride(), &iModelId );
			m_iParachuteModelIndex = iModelId.GetModelIndex();
		}

		//Ensure the parachute model is valid.
		fwModelId iModelId((strLocalIndex(m_iParachuteModelIndex)));
		if(Verifyf(iModelId.IsValid(), "Parachute model is invalid."))
		{
			//Check if the parachute model request is not valid.
			if(!m_ModelRequestHelper.IsValid())
			{
				//Stream the parachute model.
				strLocalIndex transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId);
				m_ModelRequestHelper.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
			}
		}
	}
}

static CCustomShaderEffectProp* GetCsePropFromObject(CObject *pObj)
{
	fwCustomShaderEffect *fwCSE = pObj->GetDrawHandler().GetShaderEffect();
	if(fwCSE && fwCSE->GetType()==CSE_PROP)
	{
		return static_cast<CCustomShaderEffectProp*>(fwCSE);
	}

	return NULL;
}

void CAutomobile::CreateParachute()
{
	if(GetDriver())
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey( GetParachuteModelOverride(), &iModelId );

		//Ensure the model id is valid.
		if(Verifyf(iModelId.IsValid(), "Invalid parachute model"))
		{
			//Create the input.
			CObjectPopulation::CreateObjectInput input(iModelId, ENTITY_OWNEDBY_GAME, true);
			input.m_bForceClone = true;

			//Create the parachute object.
			Assert(!m_pParachuteObject);
			m_pParachuteObject = CObjectPopulation::CreateObject(input);

			if(Verifyf(m_pParachuteObject, "Failed to create parachute"))
			{
				m_pParachuteObject->SetIsParachute(true);
				m_pParachuteObject->SetIsCarParachute(true);

				REPLAY_ONLY(CReplayMgr::RecordObject(m_pParachuteObject));

				//Make the parachute invisible.
				m_pParachuteObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false);

				m_pParachuteObject->m_nObjectFlags.bIsVehicleGadgetPart = true;

				//Grab the ped.
				CPed* pPed = GetDriver();

				//Add the parachute to the world.
				CGameWorld::Add(m_pParachuteObject, pPed->GetInteriorLocation(), false);

				// Add the parachuteobject task to the parachute...
				if(m_pParachuteObject->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY) == NULL)
				{
					// TaskParachuteObject is now a cloned task so only create it on the owner....
					if(GetDriver() && !GetDriver()->IsNetworkClone())
					{
						//Give the parachute its task.
						CTaskParachuteObject* parachuteObjectTask = rage_new CTaskParachuteObject;
						m_pParachuteObject->SetTask(parachuteObjectTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY);

						//Update the task immediately.
						if(m_pParachuteObject->GetObjectIntelligence())
						{
							m_pParachuteObject->GetObjectIntelligence()->ForcePostCameraTaskUpdate();
						}
					}
				}

				m_pParachuteObject->EnableCollision();

				//Set the mass.
				m_pParachuteObject->SetMass(100.0f);

				//Prevent the camera shapetests from detecting this object, as we don't want the camera to pop if it passes near to the camera
				//or collision root position.
				phInst* pParachutePhysicsInst = m_pParachuteObject->GetCurrentPhysicsInst();
				if(pParachutePhysicsInst)
				{
					if(pParachutePhysicsInst->IsInLevel())
					{
						CPhysics::GetLevel()->SetInstanceIncludeFlags(pParachutePhysicsInst->GetLevelIndex(), ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);

						//Grab the level index.
						u16 uLevelIndex = pParachutePhysicsInst->GetLevelIndex();

						//Set inactive collisions.
						PHLEVEL->SetInactiveCollidesAgainstInactive(uLevelIndex, true);
						PHLEVEL->SetInactiveCollidesAgainstFixed(uLevelIndex, true);
					}

					phArchetype* pParachuteArchetype = pParachutePhysicsInst->GetArchetype();
					if(pParachuteArchetype)
					{
						//NOTE: This include flag change will apply to all new instances that use this archetype until it is streamed out.
						//We can live with this for camera purposes, but it's worth noting in case it causes a problem.
						pParachuteArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
					}

					phArchetypeDamp* pPhysicsArchetype = static_cast<phArchetypeDamp*>( pParachuteArchetype );
					pPhysicsArchetype->SetBuoyancyFactor( 0.0f );
				}

				// tint parachute:
				m_pParachuteObject->SetTintIndex(m_nParachuteTintIndex);
				CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(m_pParachuteObject);
				if(pPropCSE)
				{
					pPropCSE->SelectTintPalette((u8)m_nParachuteTintIndex, m_pParachuteObject);
				}

			} // End Verifyf(m_pParachuteObject, "Failed to create parachute")
		} // End Verifyf(iModelId.IsValid(), "Invalid parachute model")

	}
}

void CAutomobile::AttachParachuteToVehicle()
{
	//Ensure the parachute is valid.
	if(Verifyf(m_pParachuteObject, "Parachute is invalid."))
	{
		//Ensure the parachute is not attached.
		if(m_pParachuteObject->GetIsAttached())
		{
			m_pParachuteObject->DetachFromParent(0);
		}

		//Calculate the attach offset.

		Assertf(GetBoneIndex(VEH_PARACHUTE_DEPLOY) > -1, "Vehicle with parachute is missing the parachute attachment bone 'parachute_deploy'!");
		
		Vector3 vAttachOffset = VEC3_ZERO;

		TUNE_GROUP_FLOAT(VEHICLE_PARACHUTE_TUNE, PARACHUTE_Z_OFFSET, 2.25f, -10.0f, 10.0f, 0.1f);
		vAttachOffset.z += PARACHUTE_Z_OFFSET;

		TUNE_GROUP_FLOAT(VEHICLE_PARACHUTE_TUNE, PARACHUTE_Y_OFFSET, -0.15f, -10.0f, 10.0f, 0.05f);
		vAttachOffset.y += PARACHUTE_Y_OFFSET;

		//Attach the parachute to the ped.
		m_pParachuteObject->AttachToPhysicalBasic(this, (rage::s16)GetBoneIndex(VEH_PARACHUTE_DEPLOY), ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP | ATTACH_FLAG_COL_ON, &vAttachOffset, NULL);
	}
}

void CAutomobile::DetachParachuteFromVehicle()
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}

	//Ensure the parachute is attached.
	if(!m_pParachuteObject->GetIsAttached())
	{
		return;
	}

	vehicleDisplayf("CAutomobile::DetachParachuteFromVehicle() Detaching parachute %s", m_pParachuteObject->GetNetworkObject() ? m_pParachuteObject->GetNetworkObject()->GetLogName() : "Parachute Not Networked!");

	//Detach the parachute from the ped.
	m_pParachuteObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);
}

void CAutomobile::DestroyParachute()
{
	//Ensure the parachute is valid.
	if(m_pParachuteObject)
	{
		vehicleDisplayf("CAutomobile::DestroyParachute() Destroying parachute %s", m_pParachuteObject->GetNetworkObject() ? m_pParachuteObject->GetNetworkObject()->GetLogName() : "Parachute Not Networked!");

		//Fade out the object.
		m_pParachuteObject->m_nObjectFlags.bFadeOut = true;

		//Disable collision.  Having collision enabled here has caused issues
		//with peds landing in MP games -- they end up intersecting with it on
		//detach, and get shot to the side.  I have seen this happen in SP as well.
		m_pParachuteObject->DisableCollision(NULL, true);

		//Remove the object from the world when it is not visible.
		m_pParachuteObject->m_nObjectFlags.bRemoveFromWorldWhenNotVisible = true;
	}
}

static dev_float sfParchuteHatchOpenOffset = 0.44f;
static dev_float sfParachuteHatchMoveSpeedMult = 1.5f;
static dev_float sfMaxStabiliseTorque = 20.0f;
static dev_float sfMaxDeployAngle = 1.0f;
static dev_float sfMaxAngVelForDeploy = 1.0f;
void CAutomobile::ProcessCarParachute()
{
	Assertf(HasParachute(), "Process car parachute called for a car that can not have a parachute!\n");

	// Update whether the player can deploy the parachute
	bool bCollidingWithMap = false;
	if(GetFrameCollisionHistory())
	{
		bCollidingWithMap = GetFrameCollisionHistory()->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING);
	}

	// And wheels not touching the ground
	// And has a driver
	// And is not colliding with map
	// And is not attached to anything
	m_bCanDeployParachute = (
		GetNumContactWheels() == 0 && 
		GetDriver() && 
		!bCollidingWithMap &&
		!GetIsAttached() && 
		!GetIsInWater() &&
		GetStatus() != STATUS_WRECKED );

	switch(m_carParachuteState)
	{
	case CAR_NOT_PARACHUTING:
		{
			CNetObjVehicle* netObjVehicle = SafeCast(CNetObjVehicle, GetNetworkObject());
			if (netObjVehicle && netObjVehicle->GetPendingParachuteObjectId() != NETWORK_INVALID_OBJECT_ID)
			{
				// Clear the pending parachute object ID if we are not parachuting
				netObjVehicle->SetPendingParachuteObjectId(NETWORK_INVALID_OBJECT_ID);
			}

			if (m_wasCloneParachuting && m_pParachuteObject)
			{
				m_pParachuteObject->m_nObjectFlags.bFadeOut = true;
				m_pParachuteObject->DisableCollision(NULL, true);
			}

			m_wasCloneParachuting = false;
		}
		break;
	case CAR_STABILISING:
		{
			// Stop jumping if parachuting
			if( HasJump() )
			{
				if( GetIsDoingJump() )
				{
					vehicleDisplayf( "CAutomobile::ProcessCarParachute - Car Jump: %s, cancelled due to parachute stabilising", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "?" );
				}

				SetIsDoingJump(false);
			}

			// If we're suddenly in a non-valid state for parachuting, abort
			if(!m_bCanDeployParachute)
			{
				vehicleDisplayf("CAutomobile::ProcessCarParachute() - CAR_STABILISING - RequestFinishParachuting: Name[%s] Network log name[%s]", GetDebugName(), GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown" );

				RequestFinishParachuting();
				break;
			}

			// Adjust desired up vector by stick input
			Vector3 vCurrentUp = VEC3V_TO_VECTOR3(GetTransform().GetC());
			Vector3 vDesiredUp = Vector3(0.0f, 0.0f, 1.0f);

			// Get the angle between the current up and the desired up
			float fAngle = AcosfSafe(Dot(vCurrentUp, vDesiredUp));

			// Avoid cross product breaking down
			if(Abs(fAngle) > 0.001f)
			{
				// Calculate the rotation axis by crossing the current and desired up vectors
				Vector3 vAxisToRotateOn = vCurrentUp;
				vAxisToRotateOn.Cross(vDesiredUp);

				// Subtract the current angular velocity from the rotation axis to keep the vehicle from spinning out of control
				vAxisToRotateOn -= GetAngVelocity();

				// Stabilise pitch and roll
				ApplyParachuteStabilisation( vAxisToRotateOn, fAngle, sfMaxStabiliseTorque );
			}

			float fAngVelocityNoYaw = GetAngVelocity().Mag() - Abs(GetAngVelocity().Dot(vCurrentUp));

			// Can deploy parachute if leveled enough
			// And not spinning too fast
			if(Abs(fAngle) < sfMaxDeployAngle && 
				fAngVelocityNoYaw < sfMaxAngVelForDeploy )
			{
				m_carParachuteState = CAR_CREATING_PARACHUTE;
			}

		}

		break;
	case CAR_CREATING_PARACHUTE:
		{
			StreamInParachute();

			if(IsParachuteStreamedIn())
			{
				CreateParachute();

				AttachParachuteToVehicle();

				if(m_pParachuteObject)
				{
					m_carParachuteState = CAR_DEPLOYING_PARACHUTE;
				}
				// Failed to create parachute, abort
				else
				{
					m_carParachuteState = CAR_NOT_PARACHUTING;
				}
			}
			// Can no longer deploy parachute, abort
			else if(!m_bCanDeployParachute)
			{
				m_carParachuteState = CAR_NOT_PARACHUTING;
			}
		}

		break;
	case CAR_DEPLOYING_PARACHUTE:
		{
			if(m_pParachuteObject)
			{
				//Ensure the task is valid.
				CTask* pTask = m_pParachuteObject->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY);
				if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_PARACHUTE_OBJECT)
				{
					CTaskParachuteObject* pParachuteObjTask = static_cast<CTaskParachuteObject *>(pTask);

					if(pParachuteObjTask->CanDeploy() )
					{
						pParachuteObjTask->Deploy();

						if(m_VehicleAudioEntity)
						{
							m_VehicleAudioEntity->OnVehicleParachuteDeploy();
						}

						m_bHasBeenParachuting = true;
						m_carParachuteState = CAR_PARACHUTING;
					}
				}
			}

			break;
		}
	case CAR_PARACHUTING:
		{
			if(!m_bFinishParachutingRequested)
			{
				ProcessParachutePhysics();
			}

			if (GetStatus() == STATUS_WRECKED)
			{
				RequestFinishParachuting();
			}

			if( GetNumContactWheels() != 0 || GetIsInWater() )
			{
				vehicleDisplayf("CAutomobile::ProcessCarParachute() - CAR_PARACHUTING - RequestFinishParachuting: Contact Wheels [%d] In Water[%d] Name[%s] Network Log Name[%s]",
					GetNumContactWheels(),
					(int)GetIsInWater(),
					GetDebugName(), 
					GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown" );

				bool dontFinishParachuting = CObject::ms_bDisableCarCollisionsWithCarParachute && 
											 ( IsWheelContactPhysicalMoving() ||
											   GetReferenceFrameVelocity().Mag2() > 0.0f );
											  
				if( !dontFinishParachuting )
				{
					RequestFinishParachuting();
				}
			}
		}
		break;

	case CLONE_CAR_PARACHUTING:
		if (!m_pParachuteObject && GetNetworkObject())
		{
			CNetObjVehicle* netObjVehicle = static_cast<CNetObjVehicle*>(GetNetworkObject());

			if (netObjVehicle->GetPendingParachuteObjectId() != NETWORK_INVALID_OBJECT_ID)
			{
				netObject* netObj = NetworkInterface::GetObjectManager().GetNetworkObject(netObjVehicle->GetPendingParachuteObjectId());

				if (netObj && netObj->GetObjectType() == NET_OBJ_TYPE_OBJECT)
				{
					CNetObjObject* parachuteNetObj = SafeCast(CNetObjObject, netObj);
					if (parachuteNetObj)
					{
						m_pParachuteObject = parachuteNetObj->GetCObject();

						if (m_pParachuteObject)
						{
							m_wasCloneParachuting = true;
							m_pParachuteObject->SetIsCarParachute(true);

							phInst* pParachutePhysicsInst = m_pParachuteObject->GetCurrentPhysicsInst();
							phArchetype* pParachuteArchetype = pParachutePhysicsInst->GetArchetype();
							if(pParachuteArchetype)
							{
								pParachuteArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
							}

							// tint parachute
							m_pParachuteObject->SetTintIndex(m_nParachuteTintIndex);
							CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(m_pParachuteObject);
							if(pPropCSE)
							{
								pPropCSE->SelectTintPalette((u8)m_nParachuteTintIndex, m_pParachuteObject);
							}
						}						
					}				
				}
			}		
		}

		if (m_pParachuteObject && GetNetworkObject())
		{
			ProcessParachutePhysics();

			if(!m_pParachuteObject->GetIsAttached() && m_wasCloneParachuting)
			{
				m_pParachuteObject->m_nObjectFlags.bFadeOut = true;
				m_pParachuteObject->DisableCollision(NULL, true);
			}
		}
		break;

	default:
		break;
	}

	if(m_carParachuteState != CAR_NOT_PARACHUTING && m_carParachuteState != CLONE_CAR_PARACHUTING)
	{
		// If in stunt camera mode, disable it
		if(SStuntJumpManager::IsInstantiated() && CStuntJumpManager::IsAStuntjumpInProgress())
		{
			SStuntJumpManager::GetInstance().AbortStuntJumpInProgress();
		}
	}

	Assertf(GetBoneIndex(VEH_PARACHUTE_OPEN) > -1, "Parachute car does not have a parachute_open bone!\n");

	// Trap door movement
	if(GetBoneIndex(VEH_PARACHUTE_OPEN) > -1)
	{
		float fDesiredHatchOffset = 0.0f;
		if( IsParachuting() )
		{
			fDesiredHatchOffset = sfParchuteHatchOpenOffset;
		}

		SlideMechanicalPart(GetBoneIndex(VEH_PARACHUTE_OPEN), fDesiredHatchOffset, m_fParachuteHatchOffset, Vector3(0.0f, -1.0f, 0.0f), sfParachuteHatchMoveSpeedMult);
	}

	// Process requests to finish parachuting
	if(m_bFinishParachutingRequested && !IsNetworkClone())
	{
		FinishParachuting();
	}

	// Make sure that after parachuting, when the car lands, we are in the appropriate gear for our speed
	if( m_bHasBeenParachuting && GetNumContactWheels() > 0 )
	{
		m_bHasBeenParachuting = false;
		SelectAppropriateGearForSpeed();
	}
}

void CAutomobile::ProcessCarParachuteTint()
{
	CNetObjVehicle* netObjVehicle = static_cast<CNetObjVehicle*>(GetNetworkObject());

	if (netObjVehicle && netObjVehicle->GetPendingParachuteObjectId() != NETWORK_INVALID_OBJECT_ID)
	{
		netObject* netObj = NetworkInterface::GetObjectManager().GetNetworkObject(netObjVehicle->GetPendingParachuteObjectId());

		if (netObj && netObj->GetObjectType() == NET_OBJ_TYPE_OBJECT)
		{
			CNetObjObject* parachuteNetObj = SafeCast(CNetObjObject, netObj);
			if (parachuteNetObj && m_pParachuteObject && m_pParachuteObject->GetTintIndex() != m_nParachuteTintIndex)
			{				
				// tint parachute
				m_pParachuteObject->SetTintIndex(m_nParachuteTintIndex);
				CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(m_pParachuteObject);
				if(pPropCSE)
				{
					pPropCSE->SelectTintPalette((u8)m_nParachuteTintIndex, m_pParachuteObject);
				}									
			}				
		}
	}		
}

bool CAutomobile::IsParachuteDeployed() const
{
	//Ensure the task is valid.
	const CTask* pTask = m_pParachuteObject ? m_pParachuteObject->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY) : nullptr;
	if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_PARACHUTE_OBJECT)
	{
		const CTaskParachuteObject* pParachuteObjTask = static_cast<const CTaskParachuteObject *>(pTask);

		if(pParachuteObjTask->IsOut() )
		{
			return true;
		}
	}

	return false;
}

static dev_float sfHeightForParachuteThreshold = 3.0f;
void CAutomobile::StartParachuting()
{
	Assertf(HasParachute(), "Calling start parachute on car that does not have a parachute!\n");

	if(!m_pParachuteObject && m_carParachuteState == CAR_NOT_PARACHUTING )
	{
		float fCarZ = GetTransform().GetPosition().GetZ().Getf();
		float fHeightAboveGround = fCarZ - WorldProbe::FindGroundZFor3DCoord(0.0f, VEC3V_TO_VECTOR3(GetTransform().GetPosition()));

		if( fHeightAboveGround > sfHeightForParachuteThreshold )
		{
			if(m_bCanDeployParachute && m_VehicleAudioEntity)
			{
				m_VehicleAudioEntity->OnVehicleParachuteStabilize();
			}
			
			m_carParachuteState = CAR_STABILISING;
		}
	}
}

void CAutomobile::FinishParachuting()
{
	Assertf(HasParachute(), "Calling finish parachute on car that does not have a parachute!\n");

	bool bCanFinish = false;
	if((m_carParachuteState == CAR_PARACHUTING || m_carParachuteState == CLONE_CAR_PARACHUTING) && m_pParachuteObject)
	{
		CTask* pTask = m_pParachuteObject->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY);
		if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_PARACHUTE_OBJECT)
		{
			CTaskParachuteObject* pParachuteObjTask = static_cast<CTaskParachuteObject *>(pTask);

			if( pParachuteObjTask->IsOut() )
			{
				if(m_VehicleAudioEntity)
				{
					m_VehicleAudioEntity->OnVehicleParachuteDetach();
				}
				
				DetachParachuteFromVehicle();
				DestroyParachute();
				bCanFinish = true;
			}
		}
	}
	else if( m_carParachuteState == CAR_STABILISING && m_bFinishParachutingRequested )
	{
		bCanFinish = true;
	}

	if(bCanFinish)
	{
		// Reset state
		m_carParachuteState = CAR_NOT_PARACHUTING;
		m_fParachuteStickX = 0.0f;
		m_fParachuteStickY = 0.0f;
		m_fCloneParachuteStickX = 0.0f;
		m_fCloneParachuteStickY = 0.0f;
		m_bFinishParachutingRequested = false;
		m_bCanPlayerDetachParachute = true;
	}
}

static dev_float sfTargetBaseDownVelocity = -5.0f;
static dev_float sfPitchBackwardsUpMult = 3.75f;
static dev_float sfPitchForwardsDownMult = 10.0f;
static dev_float sfTargetBaseFwdVelocity = 19.0f;
static dev_float sfFwdVelToBackPitchMult = -7.0f;
static dev_float sfFwdVelToFwdPitchMult = -9.0f;
static dev_float sfFwdVelocityMult = -1.0f;
static dev_float sfFwdLeanMult = 0.3f;
static dev_float sfBackLeanMult = 0.5f;
static dev_float sfRollMult = 1.0f;
static dev_float sfSwingOffset = 5.0f;
static dev_float sfYawStabilisationMult = -10.0f;
static dev_float sfNegateLateralMult = -0.05f;
static dev_float sfSteerMult = -5000.0f;
void CAutomobile::ProcessParachutePhysics()
{
	// If vehicle has jump, cancel it since its stabilisation can mess with the parachute controls
	if(HasJump())
	{
		if( GetIsDoingJump() )
		{
			vehicleDisplayf( "CAutomobile::ProcessParachutePhysics - Car Jump: %s, cancelled due to parachuting", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "?" );
		}
		SetIsDoingJump(false);
	}

	// Copy the inputs and flush them
	float fStickX;
	float fStickY;

	netObject *networkObject = GetNetworkObject();
	if (networkObject && networkObject->IsClone())
	{
		fStickX = m_fCloneParachuteStickX;
		fStickY = m_fCloneParachuteStickY;
	}
	else
	{
		fStickX = m_fParachuteStickX;
		fStickY = m_fParachuteStickY;

		m_fCloneParachuteStickX = fStickX;
		m_fCloneParachuteStickY = fStickY;
	}
		
	m_fParachuteStickX = 0.0f;
	m_fParachuteStickY = 0.0f;

	float fPitch = GetTransform().GetPitch();
	float fPitchNorm = fPitch > 0.0f ? ( fPitch / sfFwdLeanMult ) : ( fPitch / sfBackLeanMult );

	// UP FORCE

	// Adjust target down velocity by pitch
	float fTargetDownVelocity = sfTargetBaseDownVelocity;

	float fPitchUpDownMult = 0.0f;
	if(fPitchNorm > 0.0f)
	{
		fPitchUpDownMult = sfPitchBackwardsUpMult;
	}
	else if(fPitchNorm < 0.0f)
	{
		fPitchUpDownMult = sfPitchForwardsDownMult;
	}

	fTargetDownVelocity -= fPitchNorm * fPitchUpDownMult;

	float fCurrentDownVelocity = GetVelocity().Dot(Vector3(0.0f, 0.0f, -1.0f));

	// Apply a force up if we're going down faster than we'd like to
	if( fCurrentDownVelocity > fTargetDownVelocity )
	{
		// Calculate the difference between current and target velocities and apply a force up scaled by this
		float fDiffBetweenCurrentAndTargetVelocity = fCurrentDownVelocity - fTargetDownVelocity;

		ApplyForce(Vector3(0.0f, 0.0f, 1.0f) * fDiffBetweenCurrentAndTargetVelocity * GetMass(), Vector3(0.0f, 0.0f, 0.0f));

	}

	// FWD FORCE

	// Adjust the target velocity by pitch
	float fTargetFwdVelocity = sfTargetBaseFwdVelocity;

	float fPitchFwdMult = 0.0f;
	if(fPitchNorm > 0.0f)
	{
		fPitchFwdMult = sfFwdVelToBackPitchMult;
	}
	else
	{
		fPitchFwdMult = sfFwdVelToFwdPitchMult;
	}

	fTargetFwdVelocity += fPitchNorm * fPitchFwdMult;

	// Apply a force forwards based on the difference between the current and target forward velocities
	float fCurrentFwdVelocity = GetVelocity().Dot(VEC3V_TO_VECTOR3(GetTransform().GetB()));

	float fDiffBetweenCurrentAndTargetFwdVel = fCurrentFwdVelocity - fTargetFwdVelocity;

	Vector3 vFwdForceAxis = VEC3V_TO_VECTOR3(GetTransform().GetB());

	ApplyForce(vFwdForceAxis * fDiffBetweenCurrentAndTargetFwdVel * sfFwdVelocityMult * GetMass(), Vector3(0.0f, 0.0f, 0.0f) );

	// STABILISATION/LEAN

	// Adjust desired up vector by stick input
	Vector3 vCurrentUp = VEC3V_TO_VECTOR3(GetTransform().GetC());
	Vector3 vDesiredUp = Vector3(0.0f, 0.0f, 1.0f);

	float fLeanMult = 0.0f;

	if(fStickY > 0.0f)
	{
		fLeanMult = sfFwdLeanMult;
	}
	else if(fStickY < 0.0f)
	{
		fLeanMult = sfBackLeanMult;
	}

	// Pitch and roll the desired up vector based on stick input
	Quaternion q;

	q.FromRotation(VEC3V_TO_VECTOR3(GetTransform().GetA()), fStickY * fLeanMult);

	q.Transform(vDesiredUp);

	q.FromRotation(VEC3V_TO_VECTOR3(GetTransform().GetB()), fStickX * sfRollMult);

	q.Transform(vDesiredUp);

	// Get the angle between the current up and the desired up
	float fAngle = AcosfSafe(Dot(vCurrentUp, vDesiredUp));

	// Avoid cross product breaking down
	if(Abs(fAngle) > 0.001f)
	{
		// Calculate the rotation axis by crossing the current and desired up vectors
		Vector3 vAxisToRotateOn = vCurrentUp;
		vAxisToRotateOn.Cross(vDesiredUp);

		// LEAN

		// Subtract the current angular velocity from the rotation axis to keep the vehicle from spinning out of control
		vAxisToRotateOn -= GetAngVelocity();

		static dev_float sfMaxPitchRollStabiliseTorque = 20.0f;

		// Stabilise the pitch and roll
		ApplyParachuteStabilisation( vAxisToRotateOn, fAngle, sfMaxPitchRollStabiliseTorque );

		// Apply a yaw stabilisation force so that we don't spin out of control
		Vector3 vUpAxis = Vector3(0.0f, 0.0f, 1.0f);
		Vector3 vUpRotation = GetAngVelocity().Dot(vUpAxis) * vUpAxis;
		Vector3 torque = vUpRotation * sfYawStabilisationMult;
		const Vector3 maxTorque( 149.0f, 149.0f, 149.0f );
		torque.Max( -maxTorque, torque );
		torque.Min( maxTorque, torque );

		ApplyInternalTorque(torque * GetAngInertia().z);
	}

	// STEER

	Vector3 vFwdFlattened = VEC3V_TO_VECTOR3(GetTransform().GetB());
	vFwdFlattened.SetZ(0.0f);
	vFwdFlattened.Normalize();

	Vector3 vRightFlattened = VEC3V_TO_VECTOR3(GetTransform().GetA());
	vRightFlattened.SetZ(0.0f);
	vRightFlattened.Normalize();

	// Steer on the global Z axis
	Vector3 torque = vFwdFlattened * fStickX * sfSteerMult;
	const Vector3 maxTorque( 149.0f * GetAngInertia().z, 149.0f * GetAngInertia().z, 149.0f * GetAngInertia().z );
	torque.Max( -maxTorque, torque );
	torque.Min( maxTorque, torque );

	ApplyInternalTorque( torque, vRightFlattened * sfSwingOffset );

	// Negate a portion of the lateral velocity so that we don't strafe due to swinging or slide due to steering
	Vector3 vRight = VEC3V_TO_VECTOR3(GetTransform().GetA());
	vRight.SetZ(0.0f);
	vRight.Normalize();

	float sfLateralVelocity = GetVelocity().Dot(vRight);

	ApplyImpulse(vRight * sfNegateLateralMult * sfLateralVelocity * GetMass(), Vector3(0.0f, 0.0f, 0.0f) );
}

static const int SLIPPERY_BOX_NUM_LINK_BONES = 2;
static const eHierarchyId SLIPPERY_BOX_LINK_BONES[ SLIPPERY_BOX_NUM_LINK_BONES ] = { VEH_MISC_B, VEH_MISC_C };
static dev_float SLIPPERY_BOX_MIN_STIFFNESS = 0.5f;

void CAutomobile::SlipperyBoxInitialise()
{
	GetVehicleFragInst()->ForceArticulatedColliderMode();
	fragCacheEntry* cacheEntry = GetVehicleFragInst()->GetCacheEntry();

	if( cacheEntry )
	{
		fragHierarchyInst* pHierInst = cacheEntry->GetHierInst();

		if( pHierInst &&
			pHierInst->body )
		{
			phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

			if( pArticulatedCollider &&
				GetSkeleton() )
			{
				for( int i = 0; i < SLIPPERY_BOX_NUM_LINK_BONES; i++ )
				{
					s8 boneIndex = (s8)GetBoneIndex( SLIPPERY_BOX_LINK_BONES[ i ] );
					s8 fragChild = (s8)GetVehicleFragInst()->GetComponentFromBoneIndex( boneIndex );
					int linkIndex = pArticulatedCollider->GetLinkFromComponent(fragChild);

					const crBoneData* pBoneData = GetSkeletonData().GetBoneData( boneIndex );

					if(linkIndex > 0)
					{
						if( pBoneData && 
							pBoneData->GetDofs() & crBoneData::HAS_TRANSLATE_LIMITS )
						{
							const crJointData* pJointData = GetDrawable()->GetJointData();
							Vec3V dofMin, dofMax;

							pJointData->GetTranslationLimits(*pBoneData, dofMin, dofMax);

							if( dofMin.GetXf() != 0.0f || dofMax.GetXf() != 0.0f )
							{
								m_fSlipperyBoxLimitX = Max( Abs( dofMax.GetXf() ), Abs( dofMin.GetXf() ) );
							}
							if( dofMin.GetYf() != 0.0f || dofMax.GetYf() != 0.0f )
							{
								m_fSlipperyBoxLimitY = Max( Abs( dofMax.GetYf() ), Abs( dofMin.GetYf() ) );
							}
						}
					}
				}
			}
		}
	}
}

void CAutomobile::SlipperyBoxUpdateLimits( float limitModifier )
{
	GetVehicleFragInst()->ForceArticulatedColliderMode();
	fragCacheEntry* cacheEntry = GetVehicleFragInst()->GetCacheEntry();

	if( cacheEntry &&
		GetSkeleton() )
	{
		fragHierarchyInst* pHierInst = cacheEntry->GetHierInst();

		if( pHierInst &&
			pHierInst->body )
		{
			phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

			if( pArticulatedCollider )
			{
				for( int i = 0; i < SLIPPERY_BOX_NUM_LINK_BONES; i++ )
				{
					s8 boneIndex = (s8)GetBoneIndex( SLIPPERY_BOX_LINK_BONES[ i ] );
					s8 fragChild = (s8)GetVehicleFragInst()->GetComponentFromBoneIndex( boneIndex );
					s32 linkIndex = pArticulatedCollider->GetLinkFromComponent( fragChild );

					const crBoneData* pBoneData = GetSkeletonData().GetBoneData( boneIndex );
					const crJointData* pJointData = GetDrawable()->GetJointData();
					
					if( linkIndex > 0 &&
						pBoneData &&
						pBoneData->GetDofs() & crBoneData::HAS_TRANSLATE_LIMITS )
					{
						phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
						Vec3V dofMin, dofMax;

						pJointData->GetTranslationLimits(*pBoneData, dofMin, dofMax);

						if( dofMin.GetXf() != 0.0f || dofMax.GetXf() != 0.0f )
						{
							pJoint->SetAngleLimits( limitModifier * -m_fSlipperyBoxLimitX, limitModifier * m_fSlipperyBoxLimitX );
						}
						else if( dofMin.GetYf() != 0.0f || dofMax.GetYf() != 0.0f )
						{
							pJoint->SetAngleLimits( limitModifier * -m_fSlipperyBoxLimitY, limitModifier * m_fSlipperyBoxLimitY );
						}

						pJoint->SetStiffness( SLIPPERY_BOX_MIN_STIFFNESS + ( ( 1.0f - limitModifier ) * ( 1.0f - SLIPPERY_BOX_MIN_STIFFNESS ) ) );
					}
				}
			}
		}
	}
}

void CAutomobile::SlipperyBoxBreakOff()
{
	GetVehicleFragInst()->ForceArticulatedColliderMode();
	fragCacheEntry* cacheEntry = GetVehicleFragInst()->GetCacheEntry();

	if( cacheEntry &&
		GetSkeleton() )
	{
		fragHierarchyInst* pHierInst = cacheEntry->GetHierInst();

		if( pHierInst &&
			pHierInst->body )
		{
			for( int i = SLIPPERY_BOX_NUM_LINK_BONES - 1; i >= 0; i-- )
			{
				s8 boneIndex = (s8)GetBoneIndex( SLIPPERY_BOX_LINK_BONES[ i ] );
				s8 fragChild = (s8)GetVehicleFragInst()->GetComponentFromBoneIndex( boneIndex );

				if( fragChild > -1 )
				{
					GetFragInst()->BreakOffAbove( fragChild );
					return;
				}
			}
		}
	}

}

void CAutomobile::ApplyDifferentials()
{
    CCarHandlingData* carHandling = pHandling->GetCarHandlingData();

    if( !carHandling )
    {
        return;
    }

    u32 hasFrontDiff		= ( carHandling->aFlags & CF_DIFF_FRONT );
	u32 hasLimitedFrontDiff = ( carHandling->aFlags & CF_DIFF_LIMITED_FRONT );
    u32 hasRearDiff			= ( carHandling->aFlags & CF_DIFF_REAR );
	u32 hasLimitedRearDiff  = ( carHandling->aFlags & CF_DIFF_LIMITED_REAR );
    u32 hasCentreDiff		= ( carHandling->aFlags & CF_DIFF_CENTRE );
    u32 hasViscousCoupling	= hasCentreDiff + ( carHandling->aFlags & CF_DIFF_LIMITED_CENTRE );

    if( !hasCentreDiff && !hasFrontDiff && !hasRearDiff )
    {
        return;
    }

    float frontRearDifferential = 1.0f;

    if( hasCentreDiff )
    {
        int numFrontWheels = 0;
        int numRearWheels = 0;
        float averageFrontWheelSpeed = 0.0f;
        float averageRearWheelSpeed = 0.0f;

        for( int i = 0; i < m_nNumWheels; i++ )
        {
            CWheel* wheel = GetWheel( i );

            if( wheel )
            {
                if( wheel->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
                {
                    averageRearWheelSpeed += wheel->GetRotSpeed();
                    numRearWheels++;
                }
                else
                {
                    averageFrontWheelSpeed += wheel->GetRotSpeed();
                    numFrontWheels++;
                }
            }
        }

        averageFrontWheelSpeed = Abs( numFrontWheels > 0 ? averageFrontWheelSpeed / (float)numFrontWheels : 0.0f );
        averageRearWheelSpeed  = Abs( numFrontWheels > 0 ? averageRearWheelSpeed / (float)numRearWheels   : 0.0f );

        if( averageFrontWheelSpeed != averageRearWheelSpeed )
        {
            frontRearDifferential = averageFrontWheelSpeed / ( averageRearWheelSpeed + averageFrontWheelSpeed );

            // with a viscous coupling we'll send more torque to the wheels that are moving slower
            if( hasViscousCoupling &&
				!m_nAutomobileFlags.bInBurnout )
            {
                frontRearDifferential = 1.0f - frontRearDifferential;
            }

            frontRearDifferential *= 2.0f;
        }

        if( carHandling->m_fMaxDriveBiasTransfer != -1.0f )
        {
            float frontBias     = pHandling->m_fDriveBiasFront * 2.0f;
            float biasTransfer  = carHandling->m_fMaxDriveBiasTransfer * 2.0f;

            if( frontBias > biasTransfer )
            {
                frontRearDifferential = Clamp( frontRearDifferential, frontBias - biasTransfer, frontBias );
            }
            else
            {
                frontRearDifferential = Clamp( frontRearDifferential, frontBias, frontBias + biasTransfer );
            }
        }
    }

    float perWheelDiffFactor = 1.0f / (float)m_nNumWheels;

    for( int i = 0; i < m_nNumWheels; i++ )
    {
        CWheel* wheel = GetWheel( i );
        float diffFactor = perWheelDiffFactor;

        if( wheel )
        {
            bool isRearWheel = wheel->GetConfigFlags().IsFlagSet( WCF_REARWHEEL );

            if( isRearWheel )
            {
				diffFactor = perWheelDiffFactor * ( 2.0f - frontRearDifferential );
            }
            else
            {
                diffFactor = perWheelDiffFactor * frontRearDifferential;
            }

            if( ( isRearWheel &&
                  hasRearDiff ) ||
                ( !isRearWheel &&
                  hasFrontDiff ) )
            {
                CWheel* oppositeWheel = GetWheel( wheel->GetOppositeWheelIndex() );
                float wheelSpeed = Abs( wheel->GetRotSpeed() );
                float oppositeWheelSpeed = Abs( oppositeWheel ? oppositeWheel->GetRotSpeed() : 0.0f );

                if( wheelSpeed != oppositeWheelSpeed )
                {
					float diffScale = ( wheelSpeed / ( wheelSpeed + oppositeWheelSpeed ) );
					static dev_float sfLimitedDiffStartRatio = 0.12f;

					if( Abs( diffScale - 0.5f ) > sfLimitedDiffStartRatio &&
						( ( isRearWheel &&
						    hasLimitedRearDiff )||
						  ( !isRearWheel && 
						    hasLimitedFrontDiff ) ) )
					{
						float limitedDiffScale = 1.0f - diffScale;

						static dev_float sfMaxDiff = 0.25f;
						float diffDifferences = limitedDiffScale - diffScale;

						diffScale += diffDifferences * Min( 1.0f, Abs( diffScale - 0.5f ) / sfMaxDiff );
					}

					diffScale *= 2.0f;
					diffFactor *= diffScale;
                }
            }

            diffFactor *= m_nNumWheels;
            wheel->ScaleDriveForce( diffFactor );
        }
    }

	int j = 0;
    for(int i = 0; i < m_nNumWheels && j < 4; i++ )
    {
        if( m_ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_LEFTWHEEL ) )
        {
            if( m_ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
            {
                PF_SET( DriveForceLR, m_ppWheels[ i ]->GetDriveForce() );
				j++;
            }
            else
            {
                PF_SET( DriveForceLF, m_ppWheels[ i ]->GetDriveForce() );
				j++;
            }
        }
        else
        {
            if( m_ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
            {
                PF_SET( DriveForceRR, m_ppWheels[ i ]->GetDriveForce() );
				j++;
            }
            else
            {
                PF_SET( DriveForceRF, m_ppWheels[ i ]->GetDriveForce() );
				j++;
            }
        }
    }
}

//
//
//
CQuadBike::CQuadBike(const eEntityOwnedBy ownedBy, u32 popType) : CAutomobile(ownedBy, popType, VEHICLE_TYPE_QUADBIKE)
{
}

//
//
//
CQuadBike::~CQuadBike()
{

}

//
//
// physics
ePhysicsResult CQuadBike::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	if( m_nNumWheels == 3 &&
		GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && (m_nVehicleFlags.bAnimateWheels==0))
	{
		m_nVehicleFlags.bDisableSuperDummy = true;

		float fSteeringWheelMult = GetVehicleModelInfo()->GetSteeringWheelMult( GetDriver() );
		SetComponentRotation( QUADBIKE_FORKS_U, ROT_AXIS_LOCAL_Z, GetSteerAngle() * fSteeringWheelMult, true ); 
	}

    //Use the fallen collider flag on quads all the time as they are open wheeled and not constrained
    if(!GetDriver())
    {
        for(int i=0; i<GetNumWheels(); i++)
        {
            GetWheel(i)->GetConfigFlags().SetFlag(WCF_BIKE_FALLEN_COLLIDER);
        }

		// Allow peds to flip overturned bikes by walking into the side
		if(const CCollisionRecord* pedCollisionRecord = GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_PED))
		{
			const Mat34V quadBikeMatrix = GetMatrix();
			const Vec3V quadBikeUp = quadBikeMatrix.GetCol2();
			const Vec3V quadBikeForward = quadBikeMatrix.GetCol1();
			const Vec3V collisionNormal = RCC_VEC3V(pedCollisionRecord->m_MyCollisionNormal);

			// Only apply torques if the impact isn't with the front or bottom of the bike
			const ScalarV frontImpactAngleCosine = ScalarVFromF32(0.766f); // 40 degrees 
			const ScalarV bottomImpactAngleCosine = ScalarV(V_HALF); // 60 degrees
			if(And(IsLessThan(Abs(Dot(quadBikeForward,collisionNormal)),frontImpactAngleCosine),IsLessThan(Dot(quadBikeUp,collisionNormal),bottomImpactAngleCosine)).Getb())
			{
				// Only bother applying torque if we're offset to begin with
				const float minRollForSelfRighting = 20.0f*DtoR;
				float currentRoll = GetTransform().GetRoll();
				if(abs(currentRoll) > minRollForSelfRighting)
				{
					// The roll angle favors whichever direction gets us to the z-axis quicker but we always want to rotate
					//  so that the top of the bike goes towards the ped.
					Vec3V torque = Scale(quadBikeForward,ScalarVFromF32(currentRoll));
					if(Cross(collisionNormal,torque).GetZf() < 0)
					{
						// Negate the rotation axis and increase how much roll we need to cover since we're going the long way. 
						torque = Scale(Negate(quadBikeForward), ScalarVFromF32(currentRoll > 0 ? 2.0f*PI - currentRoll : -2.0f*PI - currentRoll));
					}

					// Apply a torque around the forward axis, relative to the roll error and angular inertia around the forward axis. 
					ApplyInternalTorque(RCC_VECTOR3(torque) * ms_fQuadBikeSelfRightingTorqueScale * GetAngInertia().y);
				}
			}
		}
    }

	
	if(GetDriver())// Do some autoleveling if in the air.
	{
		if( IsInAir() && pHandling->GetBikeHandlingData() && !GetFrameCollisionHistory()->GetMostSignificantCollisionRecord() && GetNumWheels() != 3 )
		{
			bool bDoAutoLeveling = true;
			if(GetDriver()->IsAPlayerPed())
			{
				Vec3V vPosition = GetTransform().GetPosition();
				float fMapMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPosition.GetXf(), vPosition.GetYf());
				
				//Let the player perform rolls when really high in the air
				Vec3V vBBMinZGlobal = GetTransform().Transform(Vec3V(0.0f, 0.0f, GetBoundingBoxMin().z));
				if(vBBMinZGlobal.GetZf() > fMapMaxZ)
				{
					bDoAutoLeveling = false;
				}
			}

			if(bDoAutoLeveling)
			{
				AutoLevelAndHeading( pHandling->GetBikeHandlingData()->m_fInAirSteerMult );
			}
		}
		else if(GetNumContactWheels() != GetNumWheels() && GetCollider()) // Roll over protection
		{
			static dev_float sfRollOverProtectionVelocity = 1.5f;

			Vec3V vCurrentAngVelocity = GetCollider()->GetAngVelocity();

			vCurrentAngVelocity = UnTransform3x3Full(GetCollider()->GetMatrix(), vCurrentAngVelocity);

			if(rage::Abs(vCurrentAngVelocity.GetYf()) > sfRollOverProtectionVelocity)
			{
				float fAngVelocityY = rage::Clamp(vCurrentAngVelocity.GetYf(), -3.0f, 3.0f);
				const Mat34V quadBikeMatrix = GetMatrix();
				const Vec3V quadBikeForward = quadBikeMatrix.GetCol1();

				ApplyInternalTorque(RCC_VECTOR3(quadBikeForward) * fAngVelocityY * ms_fQuadBikeAntiRollOverTorqueScale * GetAngInertia().y);
			}
		}
	}

    return CAutomobile::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);
}

void CQuadBike::ProcessPostPhysics()
{
	CAutomobile::ProcessPostPhysics();

	m_fVerticalVelocityToKnockOffThisBike = CVehicle::ms_fVerticalVelocityToKnockOffVehicle;
}

dev_float sfTrikeLeanMinSpeed = 1.0f;
dev_float sfTrikeLeanBlendSpeed = 10.0f;
dev_float sfTrikeLeanForceSpeedMult = 0.2f;
dev_float sfTrikeWheelieForceCgMult = -0.5f;
dev_float sfTrikeWheeliePlusTrigger = 0.5f;
dev_float sfTrikeWheeliePlusCap = 2.0f;

void CQuadBike::ProcessDriverInputsForStability( float fTimeStep )
{
	CAutomobile::ProcessDriverInputsForStability( fTimeStep );

	if( IsTrike() ||
		( MI_QUADBIKE_BLAZER4.IsValid() && GetModelIndex() == MI_QUADBIKE_BLAZER4 ) )
	{
		if(!GetDriver() || !GetDriver()->IsLocalPlayer())
		{
			return;
		}

		bool bMoving = false;
		float fSpeedBlend = 0.0f;
		float fFwdSpeed = DotProduct( VEC3V_TO_VECTOR3(GetTransform().GetB() ), GetVelocity() );

		bool bAllWheelsOffGround = (!GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching() && !GetWheel(1)->GetIsTouching());

		if(fFwdSpeed > sfTrikeLeanMinSpeed)
		{
			bMoving = true;
			if(fFwdSpeed > sfTrikeLeanBlendSpeed) 
				fSpeedBlend = 1.0f;
			else
				fSpeedBlend = fFwdSpeed / sfTrikeLeanBlendSpeed;
		}

		// Scale the wheelie ability by the players stats.
		float fWheelieAbilityMult = 1.0f;
		float fInAirAbilityMult = 1.0f;
		
		StatId stat = STAT_WHEELIE_ABILITY.GetStatId();
		float fWheelieStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
		float fMinWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MinWheelieAbility;
		float fMaxWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MaxWheelieAbility;

		fInAirAbilityMult = fWheelieAbilityMult = ((1.0f - fWheelieStatValue) * fMinWheelieAbility + fWheelieStatValue * fMaxWheelieAbility)/100.0f;

		//We don't want the lean back and forward forces set by player stats to be as different.
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, REDUCE_WHEELIE_ABILITY_DELTA_MULT, 0.5f, 0.0f, 1.0f, 0.01f);
		fWheelieAbilityMult = (fMaxWheelieAbility/100.0f) - (((fMaxWheelieAbility/100.0f) - fWheelieAbilityMult) * REDUCE_WHEELIE_ABILITY_DELTA_MULT);
		fWheelieAbilityMult = rage::Clamp(fWheelieAbilityMult, 0.1f, 2.0f);

		CControl *pPlayerControl = GetDriver()->GetControlFromPlayer();

		pPlayerControl->SetVehicleSteeringExclusive();
		float fFwdInput = pPlayerControl->GetVehicleSteeringUpDown().GetNorm();

		// apply torque from rider leaning fwd/back
		if( bMoving && !bAllWheelsOffGround )
		{
			if(fFwdInput < 0.0f)
			{	
				if( GetBrake() > 0.0f ) // Don't let people lean forward unless they are braking or their wheels are off the ground.
				{
					float fLeanForce = fFwdInput;
					float fRotSpeed = DotProduct(GetAngVelocity(), VEC3V_TO_VECTOR3(GetTransform().GetA())) * sfTrikeLeanForceSpeedMult;
					if(fRotSpeed < 0.0f)
					{
						fLeanForce = rage::Min(0.0f, fLeanForce - fRotSpeed);
					}

					float fSpeedBlendMult = fSpeedBlend;

					fLeanForce *= GetAngInertia().x * pHandling->GetBikeHandlingData()->m_fLeanFwdForceMult * fSpeedBlendMult * fWheelieAbilityMult;
					ApplyInternalTorque(fLeanForce * VEC3V_TO_VECTOR3(GetTransform().GetA()));

					if( GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() || GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() )
					{
						if( GetWheelFromId(VEH_WHEEL_LR)->GetTyreHealth() > TYRE_HEALTH_FLAT )
						{
							ApplyInternalForceCg(fLeanForce*sfTrikeWheelieForceCgMult*ZAXIS*ms_fBikeGravityMult);
						}
					}
				}
			}
			else
			{
				float fLeanForce = fFwdInput;
				float fRotSpeed = DotProduct(GetAngVelocity(), VEC3V_TO_VECTOR3(GetTransform().GetA())) * sfTrikeLeanForceSpeedMult;
				if(fRotSpeed > 0.0f)
				{
					fLeanForce = rage::Max(0.0f, fLeanForce - fRotSpeed);
				}

				// Reduce the lean force if the rear wheel is significantly underwater. Buoyancy and resistance forces make doing wheelies really easy
				//   non-shallow water. 
				if( m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && m_nFlags.bPossiblyTouchesWater )
				{
					float fWaterZ = 0.0f;
					Vec3V rearWheelPosition = Transform( GetMatrixRef(), VECTOR3_TO_VEC3V( CWheel::GetWheelOffset( GetVehicleModelInfo(), VEH_WHEEL_LR ) ) );
					if( m_Buoyancy.GetWaterLevelIncludingRivers( VEC3V_TO_VECTOR3( rearWheelPosition ), &fWaterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL ) )
					{
						static dev_float sfWaterLevelAboveWheelCenterSubmerged = 0.1f;
						static dev_float sfWheelSubmergedLeaningMult = 0.1f;
						if(rearWheelPosition.GetZf() + sfWaterLevelAboveWheelCenterSubmerged < fWaterZ)
						{
							fLeanForce *= sfWheelSubmergedLeaningMult;
						}
					}
				}

				float fForceMult = rage::Abs(GetThrottle());

				if(pHandling->hFlags & HF_LOW_SPEED_WHEELIES)
				{
					fForceMult = 1.0f;

					static dev_float sfWheelieSpeedBlendMinSpeed = 25.0f;

					if(fFwdSpeed > sfWheelieSpeedBlendMinSpeed)
					{
						fLeanForce = 0.0f;
					}
				}

				fLeanForce *= GetAngInertia().x*pHandling->GetBikeHandlingData()->m_fLeanBakForceMult * fWheelieAbilityMult * fForceMult;

				ApplyInternalTorque( fLeanForce* VEC3V_TO_VECTOR3(GetTransform().GetA())*ms_fBikeGravityMult);

				if( GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() || GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() )
				{
					ApplyInternalForceCg(-fLeanForce*sfTrikeWheelieForceCgMult*ZAXIS*ms_fBikeGravityMult);
				}
			}
		}

		Assert(GetStatus()==STATUS_PLAYER);

		CBikeHandlingData* pBikeHandling = pHandling->GetBikeHandlingData();

		// back wheels on ground, front wheel not = Wheelie
		if( bMoving &&
			!GetWheelFromId(VEH_WHEEL_LF)->GetIsTouching() && 
			GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() && 
			GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() )
		{
			Vector3 vBikeForwardInGroundLocal = VEC3V_TO_VECTOR3( GetTransform().GetB() );
			Vector3 vGroundNormalAtRearWheel = ( GetWheelFromId(VEH_WHEEL_LR)->GetHitNormal() + GetWheelFromId(VEH_WHEEL_RR)->GetHitNormal() ) * 0.5f;
			float fBikeForwardDotGroundNormal = vBikeForwardInGroundLocal.Dot( vGroundNormalAtRearWheel );
			float fWheelieAngle = rage::Asinf( fBikeForwardDotGroundNormal ) - pBikeHandling->m_fWheelieBalancePoint;

			float fWheelieStabiliseForce = 0.0f;
			if(fWheelieAngle > sfTrikeWheeliePlusTrigger)
			{
				fWheelieStabiliseForce = ( ( fWheelieAngle - sfTrikeWheeliePlusTrigger ) / ( sfTrikeWheeliePlusCap - sfTrikeWheeliePlusTrigger ) );
				fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
				fWheelieStabiliseForce *= -1.0f;
			}

			// blend stabilization force out smoothly at low speeds
			fWheelieStabiliseForce *= pBikeHandling->m_fRearBalanceMult * GetAngInertia().x * fSpeedBlend;

			ApplyInternalTorque(fWheelieStabiliseForce* VEC3V_TO_VECTOR3(GetTransform().GetA())*ms_fBikeGravityMult);
		}
	}
}

ePrerenderStatus CQuadBike::PreRender( const bool bIsVisibleInMainViewport )
{
	if(GetModelIndex() == MI_TRIKE_CHIMERA)
	{
		ePrerenderStatus status = CVehicle::PreRender(bIsVisibleInMainViewport);
		crSkeleton* pSkel = GetSkeleton();
		const bool bIsDummy = IsDummy();

#if GTA_REPLAY
		if(!CReplayMgr::IsEditModeActive())
#endif
		{
			if((!bIsDummy || !m_nVehicleFlags.bLodFarFromPopCenter))
			{
				for(int i=0; i<GetNumWheels(); i++)
				{
					PrefetchObject(m_ppWheels[i]);
				}
				for(int i=0; i<GetNumWheels(); i++)
				{
					if( m_ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) && GetModelIndex() != MI_TRIKE_RROCKET)
					{
						m_ppWheels[i]->ProcessWheelMatrixForTrike();
					}
					else
					{
						m_ppWheels[i]->ProcessWheelMatrixForAutomobile( *pSkel );
					}
					
				}
			}
		}


		// Front wheel first
		//
		CWheel* pWheelF = GetWheelFromId( BIKE_WHEEL_F );

		int nWheelIndexF = GetBoneIndex( BIKE_WHEEL_F );
		const crBoneData* pBoneDataF = pSkel->GetSkeletonData().GetBoneData( nWheelIndexF );
		const crBoneData* pParentBoneDataF = pBoneDataF->GetParent();

		float fZDiff = pWheelF->GetWheelCompressionFromStaticIncBurstAndSink();

		const crBoneData* pLowerForkBoneData = NULL;
		if( GetBoneIndex( QUADBIKE_FORKS_L ) != -1 )
		{
			pLowerForkBoneData = pSkel->GetSkeletonData().GetBoneData(GetBoneIndex(QUADBIKE_FORKS_L));
		}

		if( pLowerForkBoneData )
		{
			Matrix34& lowerForkBoneMatrix = RC_MATRIX34( pSkel->GetLocalMtx( pLowerForkBoneData->GetIndex() ) );
			lowerForkBoneMatrix.FromQuaternion( RCC_QUATERNION( pLowerForkBoneData->GetDefaultRotation() ) );
			lowerForkBoneMatrix.d = RCC_VECTOR3( pLowerForkBoneData->GetDefaultTranslation() );
		}

		const crBoneData* pForkBoneData = NULL;
		if( GetBoneIndex( QUADBIKE_FORKS_U ) != -1 )
		{
			pForkBoneData = pSkel->GetSkeletonData().GetBoneData(GetBoneIndex(QUADBIKE_FORKS_U));
		}

		if( pForkBoneData && pForkBoneData != pParentBoneDataF )
		{
			Matrix34 forkBoneMatrix( RC_MATRIX34( pSkel->GetLocalMtx( pForkBoneData->GetIndex() ) ) );
			forkBoneMatrix.FromQuaternion( RCC_QUATERNION( pForkBoneData->GetDefaultRotation() ) );

			if( forkBoneMatrix.c.z > 0.0f )
			{
				fZDiff /= forkBoneMatrix.c.z;
			}
		}

		Matrix34& parentBoneMatrixF = RC_MATRIX34( pSkel->GetLocalMtx(pParentBoneDataF->GetIndex() ) );
		parentBoneMatrixF.d = RCC_VECTOR3( pParentBoneDataF->GetDefaultTranslation() );
		parentBoneMatrixF.d += parentBoneMatrixF.c * fZDiff;

		return status;
	}
	else
	{
		return CAutomobile::PreRender( bIsVisibleInMainViewport );
	}
}

#if __BANK
void CQuadBike::InitWidgets(bkBank& bank)
{
	bank.PushGroup("QuadBike");
		bank.AddSlider("Self Righting Torque Scale",&ms_fQuadBikeSelfRightingTorqueScale,0.0f,100.0f,0.1f);
		bank.AddSlider("Anti Rollover Torque Scale",&ms_fQuadBikeAntiRollOverTorqueScale,-100.0f,100.0f,0.1f);
	bank.PopGroup();
}
#endif // __BANK
