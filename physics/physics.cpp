//
// filename:	physics.cpp
// description:	Class controlling physics
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "grcore/light.h"
#include "breakableglass/glassmanager.h" 
#include "breakableglass/piecegeometry.h"
#include "breakableglass/breakable.h"
#include "breakableglass/bgdrawable.h"
#include "crskeleton/skeleton.h"
#include "diag/art_channel.h"
#include "fragment/manager.h"
#include "fragment/tune.h"
#include "fragmentnm/instance.h"
#include "fragmentnm/manager.h"
#include "fragmentnm/messageparams.h"
#include "fwdebug/vectormap.h"
#include "fwgeovis/geovis.h"
#include "fwpheffects/taskmanager.h"
#include "fwpheffects/clothmanager.h"
#include "fwpheffects/ropemanager.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwdebug/picker.h"
#include "input/keyboard.h"
#include "input/keys.h"
#if __BANK && !__RESOURCECOMPILER
#include "audio/pedfootstepaudio.h"
#include "physics/Rope.h"
#endif
#include "pharticulated/articulatedcollider.h"
#include "pheffects/mouseinput.h"
#include "phglass/glassinstance.h"
#include "phsolver/contactmgr.h"
#include "phbound/Support.h"
#include "phcore/pool.h"
#include "phcore/surface.h"
#include "physics/broadphase.h"
#include "physics/overlappingpairarray.h"
#include "physics/btBroadphaseProxy.h"
#include "physics/btAxisSweep3.h"
#include "physics/levelnew.h"
#include "physics/sleep.h"
#include "physics/debugevents.h"
#include "profile/timebars.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Bike.h"
#include "vehicles/propeller.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "system/alloca.h"
#include "system/cache.h"
#include "system/criticalsection.h"
#include "system/dependency.h"
#include "grprofile/pix.h"
#if __BANK
#include "task/Physics/TaskNMRiverRapids.h"
#endif // __BANK
#include "weapons/Weapon.h"
#include "vfx/vfxutil.h"
#include "control/record.h"

#if __BANK
	#include "bank/bkmgr.h"
	#include "bank/bank.h"
	#include "bank/combo.h"
	#include "bank/slider.h"
	#include "phbound/boundtaperedcapsule.h"
#endif

// Framework Header
#include "fwmaths/angle.h"
#include "fwnet/netblender.h"
#include "fwscene/search/Search.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Cloth/ClothMgr.h"
#include "Cloth/ClothArchetype.h"
#include "control/gamelogic.h"
#include "core/game.h"
#include "camera/debug/debugdirector.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/DebugScene.h"
#include "debug/DebugGlobals.h"
#include "event/Events.h"
#include "game/ModelIndices.h"
#include "modelinfo/modelinfo.h"
#include "pathserver/exportcollision.h"
#include "pathserver/NavGenParam.h"
#include "pathserver/pathserver.h"
#include "peds/Horse/horseComponent.h"
#include "peds/PedFactory.h"
#include "Peds/PedGeometryAnalyser.h"
#include "peds/Ped.h"
#include "peds/PedCloth.h"
#include "peds/PedIntelligence.h"
#include "Peds/pedpopulation.h"
#include "performance/clearinghouse.h"
#include "physics/CollisionRecords.h"
#include "physics/breakable.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "physics/Tunable.h"
#include "Physics/Rope.h"
#include "ModelInfo/BaseModelInfo.h"
#include "renderer/RenderThread.h"
#include "renderer/River.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/ContinuityMgr.h"
#include "scene/FocusEntity.h"
#include "scene/LoadScene.h"
#include "scene/scene.h"
#include "scene/portals/Portal.h"
#include "scene/RegdRefTypes.h"
#include "scene/world/VisibilityMasks.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "streaming/BoxStreamer.h"
#include "streaming/CacheLoader.h"
#include "streaming/streamingrequestlist.h"
#include "system/controlMgr.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMBalance.h"
#include "tools/SectorTools.h"
#if __BANK
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "vehicleAi/VehicleIntelligence.h"
#endif // __BANK
#include "vehicles/heli.h"
#include "vehicles/vehicle.h"
#include "vehicles/VehicleFactory.h"
#include "weapons/Bullet.h"
#include "weapons/explosion.h"
#include "weapons/projectiles/Projectile.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "vehicles/wheel.h"
#include "Vfx/VisualEffects.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Puddles.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Game/Weather.h"

#if __DEV
	#include "peds/PedDebugVisualiser.h"
#endif

// network headers
#include "network/NetworkInterface.h"
#include "network/general/NetworkUtil.h"

#if GPU_DAMAGE_WRITE_ENABLED
rage::sysIpcMutex CPhysics::sm_VehicleMutex = NULL;
rage::atArray<CVehicle*> CPhysics::ms_pDamagedVehicles;
#endif

#define DEFAULT_NUM_PHYS_TIMESLICES 2
#define DEFAULT_NUM_PHYS_TIMESLICES_IN_STUNT_MODE 6

PHYSICS_OPTIMISATIONS()

PARAM(physicsrigsatload, "Load human physics rigs directly from XML files.  Only used for local testing new rigs.");
PARAM(mapMoverOnlyInMultiplayer, "Use to only use MOVER map collision when in multiplayer games; MOVER will be set to collide with weapon shapetests.");
PARAM(pfDrawDefaultSyncPoint, "Set the pfDraw render synchronisation point (see PHYSICS_RENDER_UPDATE_SYNC_POINT_STRINGS for options). Defaults to NONE");
PARAM(setMaterialDensityScale, "Scale the density of all materials defined in materials.dat by this value.");
PARAM(maxphysicalglassshards, "The maximum number of physical glass shards that can exist at a given time.");

namespace rage
{
	XPARAM(nopopups);
#if __PFDRAW
	EXT_PFD_DECLARE_GROUP(Physics);
	EXT_PFD_DECLARE_GROUP(Bounds);

	EXT_PFD_DECLARE_ITEM(Solid);
	EXT_PFD_DECLARE_ITEM(Wireframe);
	EXT_PFD_DECLARE_ITEM(EdgeAngles);
	EXT_PFD_DECLARE_ITEM(Active);
	EXT_PFD_DECLARE_ITEM(Inactive);
	EXT_PFD_DECLARE_ITEM(Fixed);

	EXT_PFD_DECLARE_GROUP(PolygonDensity);
	EXT_PFD_DECLARE_ITEM(PolygonAngleDensity);
	EXT_PFD_DECLARE_ITEM_SLIDER_INT_FULL(NumRecursions);
	EXT_PFD_DECLARE_ITEM_SLIDER(ColorMidPoint);
	EXT_PFD_DECLARE_ITEM_SLIDER_FULL(ColorExpBias);
	EXT_PFD_DECLARE_ITEM(MaxNeighborAngleEnabled);
	EXT_PFD_DECLARE_ITEM_SLIDER_FULL(MaxNeighborAngle);
	EXT_PFD_DECLARE_GROUP(PrimitiveDensity);
	EXT_PFD_DECLARE_ITEM(IncludePolygons);

	EXT_PFD_DECLARE_GROUP(TypeFlagFilter);

	EXT_PFD_DECLARE_ITEM(TypeFlag1);
	EXT_PFD_DECLARE_ITEM(TypeFlag2);
	EXT_PFD_DECLARE_ITEM(TypeFlag3);
	EXT_PFD_DECLARE_ITEM(TypeFlag4);
	EXT_PFD_DECLARE_ITEM(TypeFlag5);
	EXT_PFD_DECLARE_ITEM(TypeFlag30);

	EXT_PFD_DECLARE_ITEM(ComponentIndices);

	EXT_PFD_DECLARE_ITEM_SLIDER(BoundDrawDistance);
	EXT_PFD_DECLARE_ITEM(DrawBoundMaterials);

	EXT_PFD_DECLARE_ITEM(SolidBoundLighting);
	EXT_PFD_DECLARE_ITEM(SolidBoundRandom);
#endif
}

namespace gtaWorldStats
{
	EXT_PF_TIMER(Phys_Total);
	EXT_PF_TIMER(Phys_Requests);
	EXT_PF_TIMER(Phys_Pre);
	EXT_PF_TIMER(Phys_Pre_Sim);
	EXT_PF_TIMER(Phys_Sim);
	EXT_PF_TIMER(Phys_Post_Sim);
	EXT_PF_TIMER(Phys_Iterate);
	EXT_PF_TIMER(Phys_Post);

	EXT_PF_TIMER(Phys_RageSimUpdate);
	EXT_PF_TIMER(Phys_RageFragUpdate);
	EXT_PF_TIMER(Phys_RageClothUpdate);
	EXT_PF_TIMER(Phys_RageRopeUpdate);
	EXT_PF_TIMER(Phys_RageVerletWaitForTasks);
	EXT_PF_TIMER(Phys_RagInstBehvrs);
    EXT_PF_TIMER(Proc_VehRecording);
};
using namespace gtaWorldStats;

namespace gtaPhysicsObjects
{
	PF_PAGE(GTA_Phys_Objects, "Gta Physics Objects");

	PF_GROUP(Vehicles);
	PF_LINK(GTA_Phys_Objects, Vehicles);
	PF_VALUE_INT(RealVehicles, Vehicles);
	PF_VALUE_INT(DummyVehicles, Vehicles);
	PF_VALUE_INT(Helicopters, Vehicles);

	PF_GROUP(Peds);
	PF_LINK(GTA_Phys_Objects, Peds);
	PF_VALUE_INT(Peds, Peds);
	PF_VALUE_INT(NMRagdolls, Peds);
	PF_VALUE_INT(HighLodRagdolls, Peds);
	PF_VALUE_INT(MedLodRagdolls, Peds);
	PF_VALUE_INT(LowLodRagdolls, Peds);

	PF_GROUP(Misc);
	PF_LINK(GTA_Phys_Objects, Misc);
	PF_VALUE_INT(Explosions, Misc);
	PF_VALUE_INT(ArticulatedBodies, Misc);
	PF_VALUE_INT(RigidBodies, Misc);
}
using namespace gtaPhysicsObjects;

#if __DEV
// This command line parameter has external linkage here because it is defined
// in the RAGE Natural Motion project (see fragmentnm/manager.cpp).
extern ::rage::sysParam PARAM_nmfolder;
#endif

enum
{
	PHYSICS_RENDER_UPDATE_SYNC_NONE = 0,
	PHYSICS_RENDER_UPDATE_SYNC_BEFORE_PRE_PHYSICS_UPDATE,
	PHYSICS_RENDER_UPDATE_SYNC_BEFORE_PRESIM,
	PHYSICS_RENDER_UPDATE_SYNC_AFTER_FIRST_UPDATE,
	PHYSICS_RENDER_UPDATE_SYNC_AFTER_ALL_UPDATES,
	PHYSICS_RENDER_UPDATE_SYNC_AFTER_POST_UPDATE,

	PHYSICS_RENDER_UPDATE_SYNC_POINT_COUNT
};

#if __PFDRAW
const char* PHYSICS_RENDER_UPDATE_SYNC_POINT_STRINGS[] =
{
	"NONE",
	"BEFORE_PRE_PHYSICS_UPDATE",
	"BEFORE_PRESIM",
	"AFTER_FIRST_UPDATE",
	"AFTER_ALL_UPDATES",
	"AFTER_POST_UPDATE"
};

static int snPhysicsSynchronizeRenderAndUpdateThreads = PHYSICS_RENDER_UPDATE_SYNC_NONE;
#endif // __PFDRAW

extern atFixedArray<RegdPed, 40> g_SettledPeds;

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

struct CGTAStaticBoundsStoreInterface : public fwStaticBoundsStoreInterface
{
	virtual phInst* AddBoundToPhysicsLevel(phBound* pBound, s32 index, bool& bContainsMover, u32 nTypeFlags, u32 nIncludeFlags,
		bool bSlotIsDummy, bool bSlotIsDependency)
	{
		if (nTypeFlags)
		{
			// by default, assume mover collision
			bContainsMover = (nTypeFlags != ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
		}

		// Allow ragdoll and ped types to collide with deep surface bounds.  This is mainly to prevent the ped's head (and therfore the camera in first-person mode)
		// from going through the ground while ragdolled and when getting up
		if (nTypeFlags & ArchetypeFlags::GTA_DEEP_SURFACE_TYPE)
		{
			nIncludeFlags |= ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PED_TYPE;
		}

		// Create physics archetype.
		phArchetype* pArch = rage_new phArchetype;

		// Make sure any additional type or include flags are added to the main archetype flags too.
		pArch->SetTypeFlags(nTypeFlags);
		pArch->SetIncludeFlags(nIncludeFlags);

		if (nTypeFlags&ArchetypeFlags::GTA_RIVER_TYPE)
		{
			g_StaticBoundsStore.GetSlot(strLocalIndex(index))->m_bHasRiverBounds = true;
		}

		bool bStaticInteriorCollider = false;
		Matrix34 mat;
		
		if (bSlotIsDependency)
		{
			pArch->SetFilename("Archetype");

			mat.Identity();
		}
		else if (bSlotIsDummy)
		{
			// Give a name so we can tell static things apart from other unnamed things in the level
			pArch->SetFilename("Instanced");

			//mat.Identity();
			//mat.d.z += 1.0f;

			InteriorProxyIndex proxyID;
			strLocalIndex depSlot;
			g_StaticBoundsStore.GetDummyBoundData(strLocalIndex(index), proxyID, depSlot);

			CInteriorProxy* pProxy = CInteriorProxy::GetPool()->GetSlot(proxyID);
			Assert(pProxy);

			if (pProxy)
			{
				Mat34V tempMatrix = pProxy->GetMatrix();
				mat = RCC_MATRIX34(tempMatrix);

				bStaticInteriorCollider = true;
			}
		}
		else
		{
			// Give a name so we can tell static things apart from other unnamed things in the level
			pArch->SetFilename("Static");

			mat.Identity();
		}

		// Create physics instance and add to physics world.
		phInst* pInst = rage_new phInstGta(PH_INST_MAPCOL);
		pArch->SetBound(pBound);

		pInst->Init(*pArch, mat);

		if (!bSlotIsDependency && nTypeFlags)
		{
			CPhysics::GetSimulator()->AddFixedObject(pInst);

			// Create a GTA entity to wrap it all up (for regref'ing mainly).
			CBuilding *pBuilding = rage_new CBuilding( ENTITY_OWNEDBY_STATICBOUNDS );
			// Use IPL index as the index of the static bound.
			pBuilding->SetIplIndex(index);
			pBuilding->SetPhysicsInst(pInst, false);

			pBuilding->SetIsInteriorStaticCollider(bStaticInteriorCollider);
		}

		return pInst;
	}

	virtual void RemoveInstFromPhysicsWorld(phInst* pInst)
	{
		// delete instance from physics world
		if (pInst->GetLevelIndex() != phInst::INVALID_INDEX)
		{
			CPhysics::GetSimulator()->DeleteObject(pInst->GetLevelIndex());
		} 

		if(pInst->GetUserData())
		{
			((CEntity *)pInst->GetUserData())->ClearCurrentPhysicsInst();
			delete ((CBuilding *)pInst->GetUserData());
		}

		// deleting inst also deletes archetype
		pInst->SetArchetype(NULL, true);
		delete pInst;
	}

};


bool CPhysics::AdditionalSetupModelInfoPhysics(phArchetype& archetype)
{
	if (!archetype.GetPropertyFlag(ArchetypeProperties::IS_ALREADY_PROCESSED))
	{
		phBound* pBound = archetype.GetBound();
		Assert(pBound);

		if(pBound->GetType() == phBound::COMPOSITE)
		{
			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);

			// Allow collision to be tagged as MOVER, CAMERA, WEAPON map type on individual bounds of a phCompositeBound.
			if(pBoundComp->GetTypeAndIncludeFlags())
			{
				u32 nAdditionalArchTypeFlags = 0;
				u32 nAdditionalArchIncludeFlags = 0;
				for(int i = 0; i < pBoundComp->GetNumBounds(); ++i)
				{
					u32 nBoundTypeFlags = pBoundComp->GetTypeFlags(i);
					// I don't think we need this assert
					//artAssertf((nBoundTypeFlags&ArchetypeFlags::GTA_RIVER_TYPE)==0, "Filename = (%s) Trying to set river bound collision type on a CObject.", archetype.GetFilename());

					nAdditionalArchTypeFlags |= nBoundTypeFlags;
					nAdditionalArchIncludeFlags |= pBoundComp->GetIncludeFlags(i);
				}
				// Make sure any additional type or include flags are added to the main archetype flags too.
				archetype.SetTypeFlags(archetype.GetTypeFlags()|nAdditionalArchTypeFlags);
				archetype.SetIncludeFlags(archetype.GetIncludeFlags()|nAdditionalArchIncludeFlags);
			}
		}
		else
		{
			// Nothing else needs to be done if the bound isn't a composite.
		}
		archetype.SetPropertyFlag(ArchetypeProperties::IS_ALREADY_PROCESSED);
	}

	return false;
}


// --- Globals ------------------------------------------------------------------

RAGE_DEFINE_CHANNEL(physics)

static CGTAStaticBoundsStoreInterface g_StaticBoundsStoreInterface;
static CGTABoxStreamerInterfaceNew g_BoxStreamerInterfaceNew;

// --- Static Globals -----------------------------------------------------------
bool CPhysics::ms_bIgnoreForcedSingleStepThisFrame = false;

#if __BANK
	static bool gbPhysicsInstCounters = false;
	static bool gbPrintPhysicsActive = false;
	static bool gbPrintPhysicsInActive = false;
	static bool gbPrintPhysicsFixed = false;
	static bool gbDisplayVectorMapPhysicsInsts = false;
	static bool g_NoPopups = false;
	static bool g_PauseUnpauseGame = false;

	bool CPhysics::ms_bDisplaySelectedPhysicalCollisionRecords = false;
	bool CPhysics::ms_bDisplayAllCollisionRecords = false;

	bool CPhysics::ms_bDisplayNumberOfBrokenPartsFocusFrag = false;

	bool CPhysics::ms_bDisplayPedHealth = false;
	bool CPhysics::ms_bDisplayPedFallHeight = false;

	bool CPhysics::ms_bDoVehicleDamage = true;
	bool CPhysics::ms_bDoCollisionEffects = true;
	bool CPhysics::ms_bDoWeaponImpactEffects = true;

	bool CPhysics::ms_bPrintBreakingImpulsesForPeds = false;
	bool CPhysics::ms_bPrintBreakingImpulsesForVehicles = false;
	bool CPhysics::ms_bPrintBreakingImpulsesApplied = false;
	int CPhysics::ms_iSelectedGravLevel = CPhysics::GRAV_EARTH;

	// A list of objects which can be excluded from shape tests.
	static const CEntity* g_pExcludeEntityList[PROCESS_LOS_MAX_EXCEPTIONS];
	static int g_nNumExcludedEntities = 0;

	bool CPhysics::ms_OverrideCrackSelection = false;
	s32 CPhysics::ms_OverrideSelectedCrack = 0;

	bool CPhysics::ms_MouseBreakEnable = false;
	float CPhysics::ms_MouseBreakDamage = 300.0f;
	float CPhysics::ms_MouseBreakImpulse = 1.0f;
	float CPhysics::ms_MouseBreakRadius = 0.1f;
	int CPhysics::ms_MouseBreakCrackType = 0;
	bool CPhysics::ms_MouseBreakHiddenHit = false;
	Vector3 CPhysics::ms_MouseBreakPosition;
	Vector3 CPhysics::ms_MouseBreakVelocity;
#endif

#if __DEV
	CDebugDrawStore CPhysics::ms_debugDrawStore(256);
	bool CPhysics::ms_bDoPedProbesWithImpacts = true; 
#endif




// --- Static class members -----------------------------------------------------

phLevelNew* CPhysics::ms_PhysLevel = NULL;
phSimulator* CPhysics::ms_PhysSim = NULL;
fragManager* CPhysics::ms_FragManager = NULL;
phConstraintMgr* CPhysics::ms_ConstraintManager = NULL;
verletTaskManager* CPhysics::ms_TasksManager = NULL;
clothInstanceTaskManager* CPhysics::ms_InstanceTasksManager = NULL;
clothManager* CPhysics::ms_ClothManager = NULL;
ropeManager* CPhysics::ms_RopeManager = NULL;
#if __BANK && !__RESOURCECOMPILER
ropeBankManager* CPhysics::ms_RopeBankManager = NULL;
#endif
fragInstGta* CPhysics::ms_pSuppliedFragCacheInsts = NULL;
grmShader* CPhysics::ms_GlassShader = NULL;
s32 CPhysics::ms_GlassTxd = -1;
s32 CPhysics::ms_SelectedCrack = 0;
grmShader* CPhysics::ms_RopeShader = NULL;

u32 CPhysics::ms_TimeSliceId = 0;
int CPhysics::ms_CurrentTimeSlice = -1;
int CPhysics::ms_NumTimeSlices = DEFAULT_NUM_PHYS_TIMESLICES;
bool CPhysics::ms_OverriderNumTimeSlices = false;
bool CPhysics::ms_bInStuntMode = false;
bool CPhysics::ms_bInArenaMode = false;
bool CPhysics::ms_OnlyPushOnLastUpdate = true;
bool CPhysics::ms_DisableOnlyPushOnLastUpdateForOneFrame = false;

bool CPhysics::ms_bSimUpdateActive = false;

float CPhysics::ms_fRagdollActivationAnimVelClamp = 3.0f;
float CPhysics::ms_fRagdollActivationAnimAngVelClamp = 6.0f;

bool CPhysics::ms_bClearManifoldsEveryFrame = false;
int CPhysics::ms_ClearManifoldsFramesLeft = 0;

float CPhysics::ms_fGravitationalAccleration = -GRAVITY;

#if PHYSICS_LOD_GROUP_SHARING
CPhysics::ManagedPhysicsLODGroup CPhysics::m_ManagedPhysicsLODGroup[CPhysics::kNumManagedPhysicsLODGroups];
#endif

#if __DEV
	static bool sbSwitchPedToNM = false;
#endif

#if USE_FRAG_TUNE && __BANK
	#define MAX_FRAG_TYPES (100)
	// frag combo box variables
	static u32 fragSorts[MAX_FRAG_TYPES];
	static atArray<const char*> fragNames;
	static s32	numFragNames = 0;
	static bkCombo*	pFragCombo = NULL;
	static s32	currFragNameSelection = 0;
//	static s32	fragModelIndex = 0;
	// end of frag name combo box vars
#endif

PARAM(phbroadphase, "Specify the type of broadphase to use (level, axisSweep1, or axisSweep3.");
PARAM(physicsupdates, "Number of times physics is updated per frame");
PARAM(phscratchpad, "Size of physics scratchpad (bytes)");

namespace rage { extern bool g_CreateMatrixSetsInFragCache; } // Set to false to turn off matrix sets in the fragment cache

// --- Code --------------------------------------------------------------------

namespace
{
#if RAGE_GLASS_USE_PRIVATE_HEAP
	int sm_bufferMem = 1;
#endif
	int sm_maxPanes = 1;
}

#define MAX_GLASS_PANES 10
void GlassDefaultMemory()
{
#if RAGE_GLASS_USE_PRIVATE_HEAP
	sm_bufferMem = 1200;
#endif
	sm_maxPanes = MAX_GLASS_PANES; // Amount of panes that can be broken at the same time
}

int GetGlassHeapMem()
{
#if BREAKABLE_GLASS_USE_BVH
	static const int iMemPerPane = 31; // We need around 31k per pane
#else
	static const int iMemPerPane = 16; // We need around 16k per pane
#endif // BREAKABLE_GLASS_USE_BVH
	return iMemPerPane * sm_maxPanes; // Derive the total memory for the amount of panes and the size of each pane
}

#if __BANK
#if RAGE_GLASS_USE_PRIVATE_HEAP
void GlassReallocMemory()
{
	bgGlassManager::Free();
	bgGlassManager::Malloc(sm_maxPanes, sm_bufferMem, GetGlassHeapMem());
}
#endif // RAGE_GLASS_USE_PRIVATE_HEAP

void GlassResetAll()
{
	CObject* pObj;
	s32 i = (s32) CObject::GetPool()->GetSize();
	
	while(i--)
	{
		pObj = CObject::GetPool()->GetSlot(i);

		if( pObj && pObj->m_nDEflags.bIsBreakableGlass )
			CObjectPopulation::ConvertToDummyObject(pObj);
	}
}

#endif

int CPhysics::GlassSelectCrack()
{
#if __BANK
	if( ms_OverrideCrackSelection )
		ms_SelectedCrack = ms_OverrideSelectedCrack;
#endif // __BANK		
	int maxType = bgGlassManager::GetCrackTypeCount() - 1;

	int result = ms_SelectedCrack > maxType ? maxType : ms_SelectedCrack;
	ms_SelectedCrack = 0; // Reset to default crack type.
	
	return result;
}

#if __OPTIMIZED
namespace rage { XPARAM(nomtphys); }
#endif

#if __BANK
namespace rage
{
	extern bool g_FixRagdollStuckInGeometry;
	extern bool g_FixRagdollStuckInGeometryElementMatch;
	extern float g_FixRagdollStuckInGeometryDepth;
	extern bool g_PreventRagdollPushCollisions;
}
#endif // __BANK

//
// name:		CPhysics::Init
// description:	
void CPhysics::InitCore()
{
	USE_MEMBUCKET(MEMBUCKET_PHYSICS);
	Assert(ms_PhysLevel==NULL);
	Assert(ms_PhysSim==NULL);
	Assert(ms_FragManager==NULL);
	Assert(ms_ConstraintManager==NULL);
	Assert(ms_ClothManager==NULL);
	Assert(ms_RopeManager==NULL);
#if __BANK && !__RESOURCECOMPILER
	Assert(ms_RopeBankManager==NULL);
#endif
	Assert(ms_TasksManager==NULL);
	Assert(ms_InstanceTasksManager==NULL);

#if !__FINAL
	ArchetypeFlags::SetupCustomBoundFlagNames();
#endif

	// GTA_RAGE_PHYSICS	
#if __PFDRAW
// Don't really want to turn these (debug collision bounds rendering etc..) by default
	PFD_GROUP_ENABLE(Physics, false);
	PFD_GROUP_ENABLE(Bounds, true);

	PFD_ITEM_ENABLE(Solid, false);
	PFD_ITEM_ENABLE(EdgeAngles, false);
	PFD_ITEM_ENABLE(Active, true);
	PFD_ITEM_ENABLE(Inactive, true);
	PFD_ITEM_ENABLE(Fixed, false);
	PFD_ITEM_SLIDER_SET_VALUE(BoundDrawDistance, 30.0f);
	PFD_ITEM_ENABLE(DrawBoundMaterials, false);

	PFD_ITEM_ENABLE(SolidBoundLighting, false);
	PFD_ITEM_ENABLE(SolidBoundRandom, true);
#endif

	const s32 maxPeds = CPed::GetPool()->GetSize();
	const s32 maxVehicles = (s32) CVehicle::GetPool()->GetSize();
	s32 maxActiveObjects = MAX_ACTIVE_COBJECTS;
	s32 maxConstraints = 562;

#if SECTOR_TOOLS_EXPORT
	// This causes us to run out of Game Virtual - used to work for GTAIV though. Not essential right now (Object posoiton checker ) so disabled.
	//	if ( CSectorTools::GetEnabled() )
	//		maxActiveObjects = 256;
#endif

	static s32 iMaxNumOctreeNodes = MAX_OCTREE_NODES;
	static s32 iMaxActiveObjects = maxPeds*2 + maxVehicles*2 + maxActiveObjects + MAX_FRAGMENT_CACHE_OBJECTS;

#if NAVMESH_EXPORT
	const bool bExtraPoolSize = CNavMeshDataExporter::WillExportCollision();
	const bool bDelayedSAP = !CNavMeshDataExporter::WillExportCollision();
	if(CNavMeshDataExporter::WillExportCollision()) maxConstraints = 1024;
#else
	const bool bExtraPoolSize = false;
	const bool bDelayedSAP = true;
#endif

	// These values used to be computed by the FW_INSTANTIATE_... macros,
	// but since MAXNOOFPEDS and VEHICLE_POOL_SIZE aren't known then, the
	// computations had to move.
	const s32 maxNumPhInstGta = 2234;
	const s32 maxNumFragInstGta = 1600;
	const s32 maxNumNMInstGta	= (MAXNOOFPEDS);
	
	u32 maxGlassShards = 25; 
	PARAM_maxphysicalglassshards.Get(maxGlassShards);

#if __BANK
	if(PARAM_physicsupdates.Get())
		 PARAM_physicsupdates.Get(CPhysics::ms_NumTimeSlices);
#endif // __BANK

#if __PFDRAW
	const char* pSyncPointName = NULL;
	if(PARAM_pfDrawDefaultSyncPoint.Get())
	{
		PARAM_pfDrawDefaultSyncPoint.Get(pSyncPointName);

		for(int i = 0; i < PHYSICS_RENDER_UPDATE_SYNC_POINT_COUNT; ++i)
		{
			if(stricmp(pSyncPointName, PHYSICS_RENDER_UPDATE_SYNC_POINT_STRINGS[i]) == 0)
			{
				snPhysicsSynchronizeRenderAndUpdateThreads = i;
				break;
			}
		}
	}
#endif // __PFDRAW

	if(bExtraPoolSize)
	{
		phInstGta::InitPool( 35000, MEMBUCKET_PHYSICS );
		fragInstGta::InitPool( maxNumFragInstGta * 8, MEMBUCKET_PHYSICS );
		fragInstNMGta::InitPool( maxNumNMInstGta, MEMBUCKET_PHYSICS );

		iMaxNumOctreeNodes *= 3;
		iMaxActiveObjects *= 3;
	}
	else
	{
		phInstGta::InitPool( maxNumPhInstGta, MEMBUCKET_PHYSICS );
		fragInstGta::InitPool( maxNumFragInstGta, MEMBUCKET_PHYSICS );
		fragInstNMGta::InitPool( maxNumNMInstGta, MEMBUCKET_PHYSICS );
	}
	
#if RSG_PC
	int nNumCacheEntries = maxPeds + maxVehicles + 128;
#else
	int nNumCacheEntries = maxPeds + maxVehicles + 64;
#endif

	static const s32 iMaxManagedColliders = 100 + maxActiveObjects + MAX_FRAGMENT_CACHE_OBJECTS;
	static const s32 iMaxManifolds = iMaxActiveObjects * MAX_MANIFOLDS_PER_ACTIVE_OBJECT;
	static const s32 iMaxCompositeManifolds = iMaxManifolds / COMPOSITE_MANIFOLD_RATIO;
	static const s32 iMaxNonCompositeManifolds = iMaxManifolds - iMaxCompositeManifolds;
	static const s32 iMaxContacts = iMaxNonCompositeManifolds * MAX_CONTACTS_PER_MANIFOLD;

	// Compute a maximum phInst-in-level count based on the maximum number of phInsts we can allocate. 
	static s32 iMaxObjects =	phInstGta::GetPool()->GetSize() + 
								fragInstGta::GetPool()->GetSize() + 
								fragInstNMGta::GetPool()->GetSize() + 
								nNumCacheEntries + 
								maxGlassShards + 
								MAX_GLASS_PANES;

	// Not sure why this isn't 65k but we won't need 32k+ for while
	iMaxObjects = Min(iMaxObjects, 32766);

	//////////////////////
	// create physics level
	//////////////////////

	phLevelNew* pLevelNew = rage_new phLevelNew;
	// immediately set this level to be the active one
	pLevelNew->SetActiveInstance();



	pLevelNew->SetExtents(Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX));
//	pLevelNew->SetExtents(Vector3(-10000.0f, -10000.0f, -3000.0f), Vector3(10000.0f, 10000.0f, 3000.0f));
	pLevelNew->SetMaxObjects(iMaxObjects);
	pLevelNew->SetMaxActive(iMaxActiveObjects);
	pLevelNew->SetMaxCollisionPairs(4000);
	pLevelNew->SetNumOctreeNodes(iMaxNumOctreeNodes);
	pLevelNew->SetMaxInstLastMatrices(phLevelNew::MAX_INST_LAST_MATRICES);

	const char* bpChoice = "axisSweep3";
	PARAM_phbroadphase.Get(bpChoice);

	if (stricmp(bpChoice, "level") == 0)
	{
		pLevelNew->SetBroadPhaseType(phLevelNew::LEVEL);
	}
	else if (stricmp(bpChoice, "axisSweep1") == 0 || stricmp(bpChoice, "axis1") == 0)
	{
		pLevelNew->SetBroadPhaseType(phLevelNew::AXISSWEEP1);
	}
	else if (stricmp(bpChoice, "axisSweep3") == 0 || stricmp(bpChoice, "axis3") == 0)
	{
		pLevelNew->SetBroadPhaseType(phLevelNew::AXISSWEEP3);
	}

#if LEVELNEW_ENABLE_DEFERRED_OCTREE_UPDATE
	pLevelNew->SetMaxDeferredInstanceCount(iMaxActiveObjects * 4);
#endif	// LEVELNEW_ENABLE_DEFERRED_OCTREE_UPDATE
	pLevelNew->Init();
	ms_PhysLevel = pLevelNew;

	// Setup the BP pair keeper function callback
	if(Verifyf(dynamic_cast<btAxisSweep3*>(pLevelNew->GetBroadPhase()), "The game relies a broadphase with KeepPairFunc implemented."))
	{
		static_cast<btAxisSweep3*>(pLevelNew->GetBroadPhase())->SetKeepPairFunc(MakeFunctorRet(CPhysics::KeepBroadphasePair));
	}

	//////////////////////
	// create physics simulator
	//////////////////////
	phSimulator::InitClass();

	ms_PhysSim = rage_new phSimulator;
	// immediately set this simulator to be the active one
	ms_PhysSim->SetActiveInstance();

#if __64BIT
	// In decreasing this from 2 MB to 1 MB on other platforms, I didn't test 64 bit and it's likely to need a bit more space anyway.
	int scratchPadSize = 2*1024*1024;
#else
	int scratchPadSize = 1*1024*1024;
#endif

	PARAM_phscratchpad.Get(scratchPadSize);
	phSimulator::InitParams params;
	params.maxManagedColliders = iMaxManagedColliders;
	params.scratchpadSize = scratchPadSize;
	params.maxManifolds = iMaxManifolds; 
	params.highPriorityManifolds = static_cast<int>((maxPeds + maxVehicles) * 0.5f * MAX_MANIFOLDS_PER_ACTIVE_OBJECT);
	params.maxCompositeManifolds = iMaxCompositeManifolds;
	params.maxContacts = iMaxContacts;
	params.maxExternVelManifolds = EXTERN_VELOCITY_MANIFOLDS;
	params.maxInstBehaviors = MAX_PHYS_INST_BEHAVIOURS;
	params.maxConstraints = maxConstraints;

	// Zero means, allocate the new kinds of constraints from the game heap
	params.maxBigConstraints = 0;
	params.maxLittleConstraints = 0;

	ms_PhysSim->Init(pLevelNew, params);

	// The constraint manager is instantiated during phSimulator::Init().
	ms_ConstraintManager = ms_PhysSim->GetConstraintMgr();

	// Install the explosion's process collisions as the simulator's post collision callback
	ms_PhysSim->SetAllCollisionsDoneCallback(MakeFunctor(phInstBehaviorExplosionGta::ProcessAllCollisions));

	ms_PhysSim->SetAllowBreakingCallback(MakeFunctorRet(CBreakable::AllowBreaking));

	SetGravitationalAcceleration(-GRAVITY);
	SetUnitUp(ZAXIS);

	// old gta gravity levels
	//m_PhysSim->SetGravity(Vector3(0.0f,0.0f,-PHYSICAL_GRAVITYFORCE*FRAMES_PER_SECOND*FRAMES_PER_SECOND));

	//////////////////////
	// Initialise materials manager
	phMaterialMgrGta::Create();
	MATERIALMGR.Load();

	// initialise articulated collider for ragdolls
	// AF: Does nothing and has been deprecated
	//phArticulatedCollider::InitClass();

	// enable rage physics reference counting
	phConfig::EnableRefCounting(true);
	phConfig::FreezeRefCounting();

	// set max force solver phases, based on scratchpad size and avoiding the warning "Physics force solver
	// max phases too large, reducing from X to X due to scratch pad size limitations."
	phConfig::SetForceSolverMaxPhases(1024);

	// activate delayed SAP add, to prevent streaming from causing spikes in the physics
	phSimulator::SetDelaySAPAddRemoveEnabled(bDelayedSAP);

	// set the collision system to aggressively throw away polygons facing in the direction of motion
	phSimulator::SetMinimumConcaveThickness(0.001f);

	// We can't use delayed collision activations at the moment because SplitPairs runs before those activations and needs to know which objects are active
	// Delayed collision activations are no longer an option but they now happen later anyway so, if you wanted to reap the rewards of 'late' collision
	//   activation (primarily objects will not be activated due to contacts that have gotten disabled), you're in luck.
//	phSimulator::SetDelayCollisionActivations(false);

	// Set the default number of force solver iterations
	phContactMgr::SetLCPSolverIterations(2);
	phContactMgr::SetLCPSolverIterationsFinal(2);

#if CONTACT_VALIDATION
	phContact::sm_MinPosition = g_WorldLimits_AABB.GetMin();
	phContact::sm_MaxPosition = g_WorldLimits_AABB.GetMax();
#endif // CONTACT_VALIDATION

	fwBoxStreamer::SetInterface(&g_BoxStreamerInterfaceNew);

	g_StaticBoundsStore.SetInterface(&g_StaticBoundsStoreInterface);

	g_StaticBoundsStore.Init();

	WorldProbe::GetShapeTestManager()->Init(INIT_CORE);

#if __PPU
	phBound::DisableMessages(true);
#endif //__PPU

#if USE_FRAG_TUNE
	fragTune::Instantiate();
	FRAGTUNE->SetForcedTuningPath("assets:/fragments");
	FRAGTUNE->Load("common:/data/fragment.xml");
#if __BANK
	ms_mouseInput = rage_new phMouseInput;
	ms_mouseInput->SetBoxTypeIncludeFlags(ArchetypeFlags::GTA_OBJECT_TYPE, ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
#endif

#endif // USE_FRAG_TUNE

	//////////////////////
	// create fragment manager
	//////////////////////

	CPedClothCollision::Init();

	fragManager::ClothInterface clthInterface;

	ms_TasksManager = rage_new verletTaskManager;
	Assert(ms_TasksManager);

	ms_InstanceTasksManager = rage_new clothInstanceTaskManager;
	Assert(ms_InstanceTasksManager);

	ms_ClothManager = rage_new clothManager;
	Assert(ms_ClothManager);

	ms_ClothManager->Init(ms_TasksManager, ms_InstanceTasksManager);

	{
		// This one is owned by the rope system, so no need to delete it.
		RAGE_TRACK(RopeShader);
		ASSET.PushFolder("common:/shaders");
		ms_RopeShader = rage_new grmShader;
		ms_RopeShader->Load("rope_default");
		ASSET.PopFolder();
	}

	ms_RopeManager = rage_new ropeManager;
	Assert(ms_RopeManager);
	ms_RopeManager->Init(ms_TasksManager,ms_RopeShader);

#if __BANK && !__RESOURCECOMPILER

#if USE_ROPE_DEBUG
	ms_RopeManager->ms_MouseInput = CPhysics::ms_mouseInput;
#endif

	ms_RopeBankManager = rage_new ropeBankManager;
	Assert(ms_RopeBankManager);
	ms_RopeBankManager->InitWidgets(ms_RopeManager);
	
  #if USE_CLOTHMANAGER_BANK
	ms_ClothManager->ms_MouseInput = CPhysics::ms_mouseInput;
  #endif
#endif

	ms_ClothManager->InitInterfaceCallbacks( clthInterface );	

	ms_ClothManager->LoadGlobalCustomBounds();

	Assert((unsigned)nNumCacheEntries < 65536);
	ms_FragManager = rage_new fragManager(
		fragManager::UserDataInit::NullFunctor(),							// callback
		MakeFunctor(&CBreakable::Insert),						// insertMovable
		MakeFunctor(&CBreakable::Remove),						// removeMovable
		MakeFunctor(&CBreakable::InitFragInst),				// newMovable
		MakeFunctorRet(&CBreakable::AddToDrawBucketFragCallback),			// drawCallback
		&clthInterface, 													// clothInterface
		MakeFunctorRet(fragManager::ReturnEntityClassZero),					// convertEntityClass
		(rage::u32)nNumCacheEntries,										// numCacheEntries
		(rage::u16)nNumCacheEntries,										// numTypeStubs
		fragManager::TypePagedInFunctor::NullFunctor(),						// pagedInCallback
		fragManager::TypePagingOutFunctor::NullFunctor(),					// pagingOutCallback
		true																// skipInitCacheMgrInit
		);

	// want to put our own fragInsts in the cache
	ms_pSuppliedFragCacheInsts = rage_new fragInstGta[nNumCacheEntries];
	fragInst** pFragCacheInsts = rage_new fragInst*[nNumCacheEntries];
	for(int i=0; i<nNumCacheEntries; i++)
	{
		ms_pSuppliedFragCacheInsts[i].SetClassType(PH_INST_FRAG_CACHE_OBJECT);
		pFragCacheInsts[i] = &ms_pSuppliedFragCacheInsts[i];
	}

	ms_FragManager->InitCacheMgr(pFragCacheInsts);

	delete [] pFragCacheInsts;
	pFragCacheInsts = NULL;

	ms_FragManager->SetDefaultTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);

	InitSharedRagdollFragTypeData();


	phBound::DisableMessages();

#if __BANK
	PGTAMATERIALMGR->GetDebugInterface().Init();
#endif

	CExplosionManager::Init();
#if NORTH_CLOTHS
	CClothMgr::Init();
#endif

	// Glass textures, also in txd version.
	ms_GlassTxd = vfxUtil::InitTxd("breakableglass");
	Assert( ms_GlassTxd != -1 );

	RAGE_TRACK(BreakableGlass);
	ASSET.PushFolder("common:/data/glass");

    g_TxdStore.PushCurrentTxd();
    g_TxdStore.SetCurrentTxd(strLocalIndex(ms_GlassTxd));
	bgGlassManager::InitClass();
	g_TxdStore.PopCurrentTxd();
		
	bgGlassManager::SetMaxLod(bgLod::LOD_MED);

	bgInitGlassBoundForGameFunctor initGlassBoundFunctor;
	initGlassBoundFunctor.Bind(&CBreakable::InitGlassBound);
	bgGlassManager::SetInitGlassBoundForGameFunctor(initGlassBoundFunctor);

	bgInitGlassInstForGameFunctor initGlassInstFunctor;
	initGlassInstFunctor.Bind(&CBreakable::InitGlassInst);
	bgGlassManager::SetInitGlassInstForGameFunctor(initGlassInstFunctor);
 	
	bgInitShardInstForGameFunctor initShardFunctor;
 	initShardFunctor.Bind(&CBreakable::InitShardInst);
 	bgGlassManager::SetInitShardInstForGameFunctor(initShardFunctor);

	fragManager::ComputeGlassCrackTypeCallbackFunctor glassSelectCrack;
	glassSelectCrack.Bind(&CPhysics::GlassSelectCrack);
	//@@: location CPHYSICSCORE_INIT
	ms_FragManager->SetComputeGlassCrackTypeCallback(glassSelectCrack);

	bgGlassManager::InitPhysicsInstPool((u16)maxGlassShards);
	ASSET.PopFolder();
	{
		RAGE_TRACK(GlassShader);
		ASSET.PushFolder("common:/shaders");
		ms_GlassShader = rage_new grmShader;
		ms_GlassShader->Load("glass_breakable_crack");
		ms_GlassShader->SetDrawBucketMask(CRenderer::GenerateBucketMask(CRenderer::RB_ALPHA));
		bgGlassManager::SetGlassShader(ms_GlassShader);
		ASSET.PopFolder();
	}
#if __BANK
	bkBank& bank = BANKMGR.CreateBank("Breakable Glass");
#if RAGE_GLASS_USE_PRIVATE_HEAP
	bank.PushGroup("Memory");
	bank.AddSlider("Maximum Panes", &sm_maxPanes, 1, 100, 1);
	bank.AddSlider("Buffer Memory (KB)", &sm_bufferMem, 1, 4194304, 1);
	bank.AddButton("Reallocate", &GlassReallocMemory);
	bank.PopGroup();
#endif

	bank.AddButton("Reset All Glass", &GlassResetAll);
	bank.PushGroup("Mouse Control");
	bank.AddToggle("Enable",&ms_MouseBreakEnable);
	bank.AddSlider("Damage Size", &ms_MouseBreakDamage, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("Detection Radius", &ms_MouseBreakRadius, 0.0f, 20.0f, 0.1f);
	bank.AddSlider("Impulse", &ms_MouseBreakImpulse, 0.0f, 20.0f, 0.1f);
	bank.AddSlider("Crack Selection", &ms_MouseBreakCrackType,0,20,1);
	bank.AddToggle("Hidden Hit", &ms_MouseBreakHiddenHit);
	bank.PushGroup("Display Values");
	bank.AddText("Impact position X",&ms_MouseBreakPosition.x,true);
	bank.AddText("Impact position Y",&ms_MouseBreakPosition.y,true);
	bank.AddText("Impact position Z",&ms_MouseBreakPosition.z,true);
	bank.AddText("Impact direction X",&ms_MouseBreakVelocity.x,true);
	bank.AddText("Impact direction Y",&ms_MouseBreakVelocity.y,true);
	bank.AddText("Impact direction Z",&ms_MouseBreakVelocity.z,true);
	bank.PopGroup();
	bank.PopGroup();
	
	bank.AddToggle("Override Crack selection",&ms_OverrideCrackSelection);
	bank.AddSlider("Crack index",&ms_OverrideSelectedCrack,0,20,1);

	bgGlassManager::AddWidgets(bank);
#endif

#if __OPTIMIZED
	if (!PARAM_nomtphys.Get())
	{
	#if __XENON
		phConfig::SetWorkerThreadCount(4);
	#elif __PS3
		phConfig::SetWorkerThreadCount(3);
	#elif RSG_DURANGO
		phConfig::SetWorkerThreadCount(4);
	#else
		phConfig::SetWorkerThreadCount(sysTaskManager::GetNumThreads());
	#endif
	}
#endif

#if !USER_JBN	// Disable having the main thread do work for spu profiling.
	// It seems to be some benefit, with little risk, to let the PPU do some work while it would otherwise be idle waiting on SPU collisions
	phConfig::SetMainThreadAlsoWorks(true);
#if __PS3
	// However, on the PS3, there are some issues with the force solver helping on the main thread, so simply disable it for now.
	phConfig::SetForceSolverMainThreadAlsoWorks(false);
#endif // __PS3
#endif // !USER_JBN

	fragInstGta::LoadVehicleFragImpulseFunction();

#if GPU_DAMAGE_WRITE_ENABLED
	sm_VehicleMutex = rage::sysIpcCreateMutex();
#endif

	//Init Perlin smoothed noise static texture for use in all vehicles
	CVehicleDeformation::InitSmoothedPerlinNoise();
}

//
// name:		CPhysics::Shutdown
// description:	Shutdown physics systems
void CPhysics::ShutdownCore()
{
#if GPU_DAMAGE_WRITE_ENABLED
	sysIpcDeleteMutex(sm_VehicleMutex);
#endif

	fragInstGta::ReleaseVehicleFragImpulseFunction();

	bgGlassManager::ShutdownClass();

	delete ms_GlassShader;
	ms_GlassShader = 0;

#if NORTH_CLOTHS
	// Shutdown the cloth manager.
	CClothMgr::Shutdown();
#endif
	// Shutdown the physics explosion manager
	CExplosionManager::Shutdown();

	WorldProbe::GetShapeTestManager()->Shutdown(SHUTDOWN_CORE);

	ShutdownSharedRagdollFragTypeData();

#if USE_FRAG_TUNE && __BANK
	fragNames.Reset();
#endif //USE_FRAG_TUNE && __BANK

#if USE_FRAG_TUNE
	fragTune::Destroy();
#endif

	Assert( ms_ClothManager );
	ms_ClothManager->Shutdown();
	delete ms_ClothManager;
	ms_ClothManager = NULL;

	Assert( ms_RopeManager );
	ms_RopeManager->Shutdown();
	delete ms_RopeManager;
	ms_RopeManager = NULL;

#if __BANK && !__RESOURCECOMPILER
	Assert( ms_RopeBankManager );
	ms_RopeBankManager->DestroyWidgets();
	delete ms_RopeBankManager;
	ms_RopeBankManager = NULL;
#endif

	Assert( ms_TasksManager );
	delete ms_TasksManager;
	ms_TasksManager = NULL;

	delete [] ms_pSuppliedFragCacheInsts;
	delete ms_FragManager;
	ms_FragManager = NULL;

	// g_breakable.Shutdown();

	// AF: deprecated
	//phArticulatedCollider::ShutdownClass();

#if __PPU
	phSimulator::ShutdownClass();
#endif //__PPU...

	g_StaticBoundsStore.Shutdown();

	ms_PhysLevel->Shutdown();
	delete ms_PhysLevel;
	ms_PhysLevel = NULL;

	//delete ms_PhysSim;
	//ms_PhysSim = NULL;

	MATERIALMGR.Destroy();
}

void CPhysics::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_PHYSICS);
	if(initMode == INIT_CORE)
	{
		InitCore();

#if PHYSICS_REQUEST_LIST_ENABLED
		s32 iPhysicsReqListSize = PHYSIC_REQUEST_LIST_SIZE;

#if NAVMESH_EXPORT
		if(CNavMeshDataExporter::WillExportCollision())
		{
			iPhysicsReqListSize*=8;				//= 50000;
		}
#endif // NAVMESH_EXPORT

		ms_physicsReqList.Init(iPhysicsReqListSize);
#endif	//PHYSICS_REQUEST_LIST_ENABLED

	}
	else if(initMode == INIT_SESSION)
	{
#if __DEV
		ms_debugDrawStore.ClearAll();
#endif

#if PHYSICS_REQUEST_LIST_ENABLED
		ms_physicsReqList.Clear();
#endif	//PHYSICS_REQUEST_LIST_ENABLED

		SetGravityLevel(GRAV_EARTH);
		CWheel::uXmasWeather = g_weather.GetTypeIndex(ATSTRINGHASH("XMAS", 0xaac9c895));// HACK cache off the xmas weather.
	}
	else if (initMode == INIT_BEFORE_MAP_LOADED)
	{
		GlassDefaultMemory();
#if RAGE_GLASS_USE_PRIVATE_HEAP
		bgGlassManager::Malloc(sm_maxPanes, sm_bufferMem, GetGlassHeapMem());
#elif RAGE_GLASS_USE_USER_HEAP
		bgGlassManager::Malloc(sm_maxPanes, &strStreamingEngine::GetAllocator(), GetGlassHeapMem());
#else
		bgGlassManager::Malloc(sm_maxPanes, GetGlassHeapMem());
#endif
	}
}

//
// name:		CPhysics::Shutdown
// description:	Before the world is emptied.
void CPhysics::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
#if PHYSICS_REQUEST_LIST_ENABLED
		ms_physicsReqList.Shutdown();
#endif	//PHYSICS_REQUEST_LIST_ENABLED

        ShutdownCore();
    }
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        g_StaticBoundsStore.Shutdown();
		bgGlassManager::Free();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    CExplosionManager::Shutdown(SHUTDOWN_SESSION);
    }
}

#if GPU_DAMAGE_WRITE_ENABLED
void CPhysics::QueueDamagedVehicleByGPU(CVehicle *pVehicle, bool &bUpdatedByGPU)
{	
	rage::sysIpcLockMutex(sm_VehicleMutex);
	if(!bUpdatedByGPU)
	{
		bUpdatedByGPU = true;
		Assertf(ms_pDamagedVehicles.Find(pVehicle) == -1, "[VEHICLE_DEFORMATION]Duplicated entry is entered to CPhysics::ms_pDamagedVehicles");
		ms_pDamagedVehicles.PushAndGrow(pVehicle);
	}
	rage::sysIpcUnlockMutex(sm_VehicleMutex);
}

void CPhysics::ProcessDamagedVehicles()
{
	rage::sysIpcLockMutex(sm_VehicleMutex);

	int iVehicleIndex = 0;
	int iCount = ms_pDamagedVehicles.GetCount();
	for (int i=0; i < iCount; ++i)
	{
		CVehicle* pVehicle = ms_pDamagedVehicles[iVehicleIndex];

		if(i < MAX_BOUND_DEFORMATION_PER_FRAME || (pVehicle && pVehicle->HasBoundUpdatePending()))
		{
			if (Verifyf(pVehicle && pVehicle->WasDamageUpdatedByGPU(), "[VEHICLE_DEFORMATION]vehicle in CPhysics::ms_pDamagedVehicles should have deformation texture updated, 0x%p, %s", pVehicle, pVehicle->GetVehicleModelInfo()->GetModelName()))
			{
				pVehicle->HandleDamageUpdatedByGPU();
			}
			ms_pDamagedVehicles.Delete(iVehicleIndex);
			// Do not increment the iVehicleIndex if we deleted an entry
		}
		else
		{
			iVehicleIndex++;
		}
	}

	rage::sysIpcUnlockMutex(sm_VehicleMutex);
}

void CPhysics::RemoveVehicleDeformation(CVehicle* pVehicle)
{
	rage::sysIpcLockMutex(sm_VehicleMutex);	
	int idx = ms_pDamagedVehicles.Find(pVehicle);

	if(idx != -1)
	{
		ms_pDamagedVehicles.Delete(idx);
	}

	rage::sysIpcUnlockMutex(sm_VehicleMutex);
}
#endif

void CPhysics::PreSimUpdate(float fTimeStep, int nSlice)
{
#if GPU_DAMAGE_WRITE_ENABLED
	if(CPhysics::GetIsFirstTimeSlice(nSlice))
	{
		ProcessDamagedVehicles();
	}
#endif

	if(CVehicleRecordingMgr::sm_bUpdateWithPhysics && CVehicleRecordingMgr::sm_bUpdateBeforePreSimUpdate)
	{
		PF_START(Proc_VehRecording);
		CVehicleRecordingMgr::RetrieveDataForThisFrame();
		PF_STOP(Proc_VehRecording);
	}

	PreSimControl preSimControl;

	preSimControl.nNumPostponedEntities = 0;
	preSimControl.nNumPostponedEntities2 = 0;
	preSimControl.nNumSecondPassEntities = 0;
	preSimControl.nNumThirdPassEntities = 0;
	preSimControl.fTimeStep = fTimeStep;
	preSimControl.nSlice = nSlice;

	const int maxSecondPassEntities = PreSimControl::GetMaxSecondPassEntities();

	preSimControl.aSecondPassEntities = Alloca(CPhysical*, maxSecondPassEntities);

	const int maxThirdPassEntities = PreSimControl::GetMaxThirdPassEntities();

	preSimControl.aThirdPassEntities = Alloca(CPhysical*, maxThirdPassEntities);

	fwSceneUpdate::Update(CGameWorld::SU_PRESIM_PHYSICS_UPDATE, &preSimControl);

#if __BANK
	// Clear the debug draw impact arrays before they get populated in IterateOverManifolds().
	CDeformableObjectManager::GetInstance().ResetImpactArrays();
#endif // __BANK

	CPhysical** aPostponedEntities = preSimControl.aPostponedEntities;
	int nNumPostponedEntities = preSimControl.nNumPostponedEntities;
	CPhysical** aSecondPassEntities = preSimControl.aSecondPassEntities;
	int nNumSecondPassEntities = preSimControl.nNumSecondPassEntities;
	CPhysical** aThirdPassEntities = preSimControl.aThirdPassEntities;
	int nNumThirdPassEntities = preSimControl.nNumThirdPassEntities;
	CPhysical** aPostponedEntities2 = preSimControl.aPostponedEntities2;
	int nNumPostponedEntities2 = preSimControl.nNumPostponedEntities2;

	// now process postponed entities
	for(int i=0; i<nNumPostponedEntities; i++)
	{
		CPhysical* pEntity = aPostponedEntities[i];
		bool networkBlendFirst = GetShouldNetworkBlendBeforePhysics( pEntity );

		if( networkBlendFirst )
		{
			netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

			if (networkObject && networkObject->IsClone() && networkObject->GetNetBlender())
			{
				// update the network blender that overrides the physics and blends the entity from
				// one position to another
				if (networkObject->CanBlend())
				{
					networkObject->GetNetBlender()->Update();
				}
			}
		}

		ePhysicsResult nReturn = pEntity->ProcessPhysics(fTimeStep, false, nSlice);
		Assertf(nReturn != PHYSICS_NEED_THIRD_PASS, "Entity type: %i | Entity name: %s", pEntity->GetType(), pEntity->GetDebugName());// why are we requesting a third pass when we aren't doing a second?
		if (nReturn == PHYSICS_NEED_SECOND_PASS)
		{
			Assert(nNumSecondPassEntities < maxSecondPassEntities);
			aSecondPassEntities[nNumSecondPassEntities++] = pEntity;
		}
        else
        {
            if(CPhysics::GetIsFirstTimeSlice(nSlice) && !networkBlendFirst)
            {
                netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

		        if (networkObject && networkObject->IsClone() && networkObject->GetNetBlender())
		        {
			        // update the network blender that overrides the physics and blends the entity from
			        // one position to another
			        if (networkObject->CanBlend())
				        networkObject->GetNetBlender()->Update();
		        }
            }
        }
	}

	// and anything that needs a second pass (i.e. vehicles waiting on SPU wheel integrator)
	for(int i=0; i<nNumSecondPassEntities; i++)
	{
		CPhysical* pEntity = aSecondPassEntities[i];
		ePhysicsResult nReturn = pEntity->ProcessPhysics2(fTimeStep, nSlice);
		if (nReturn == PHYSICS_NEED_THIRD_PASS)
		{
			Assert(nNumThirdPassEntities < maxThirdPassEntities);
			aThirdPassEntities[nNumThirdPassEntities++] = pEntity;
		}
		else
		{
			bool networkBlendFirst = GetShouldNetworkBlendBeforePhysics( pEntity );

			if( !networkBlendFirst &&
				CPhysics::GetIsFirstTimeSlice(nSlice))
			{
				netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(aSecondPassEntities[i]);

				if (networkObject && networkObject->IsClone() && networkObject->GetNetBlender())
				{
					// update the network blender that overrides the physics and blends the entity from
					// one position to another
					if (networkObject->CanBlend())
						networkObject->GetNetBlender()->Update();
				}
			}
		}
	}

	// and anything that needs a third pass (i.e. vehicles applying forces from their wheels onto other vehicles)
	for(int i=0; i<nNumThirdPassEntities; i++)
	{
		aThirdPassEntities[i]->ProcessPhysics3(fTimeStep, nSlice);

		if(CPhysics::GetIsFirstTimeSlice(nSlice))
		{
			bool networkBlendFirst = GetShouldNetworkBlendBeforePhysics( aThirdPassEntities[i] );

			netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(aThirdPassEntities[i]);

			if( !networkBlendFirst && networkObject && networkObject->IsClone() && networkObject->GetNetBlender() )
			{
				// update the network blender that overrides the physics and blends the entity from
				// one position to another
				if (networkObject->CanBlend())
					networkObject->GetNetBlender()->Update();
			}
		}
	}

	// now process postponed 2 entities. 
	// Note: we don't currently support doing a second or third pass on these entities. This list was added to support processing physics on objects that are on top of 
	// clone objects that require 2 passes (as these objects don't update the network blender until after the 2nd pass, and we require this information before proceeding).
	for(int i=0; i<nNumPostponedEntities2; i++)
	{
		CPhysical* pEntity = aPostponedEntities2[i];
		ASSERT_ONLY(ePhysicsResult nReturn = ) pEntity->ProcessPhysics(fTimeStep, false, nSlice);
		Assert(nReturn != PHYSICS_NEED_SECOND_PASS && nReturn != PHYSICS_NEED_THIRD_PASS);

		if(CPhysics::GetIsFirstTimeSlice(nSlice))
		{
			netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

			if (networkObject && networkObject->IsClone() && networkObject->GetNetBlender())
			{
				// update the network blender that overrides the physics and blends the entity from
				// one position to another
				if (networkObject->CanBlend())
					networkObject->GetNetBlender()->Update();
			}
		}
	}

	FRAGNMASSETMGR->StepPhase1(fTimeStep);

	ms_PhysSim->PreUpdateInstanceBehaviors(fTimeStep);

	if(ms_bClearManifoldsEveryFrame || ms_ClearManifoldsFramesLeft > 0)
	{
		// This was implemented for pool game, if we aren't in an interior then something has probably gone wrong
		// Allowed if using the frame countdown version
		Assertf(FindPlayerPed()->GetIsInInterior() || ms_ClearManifoldsFramesLeft > 0,"Warning: Manifolds being cleared when player not in interior: Check for script not cleaning up properly");
		btBroadphasePair *prunedPairs = ms_PhysLevel->GetBroadPhase()->getPrunedPairs();
		const int iPrunedPairCount = ms_PhysLevel->GetBroadPhase()->getPrunedPairCount();

		for(int iPairIndex = 0; iPairIndex < iPrunedPairCount; ++iPairIndex)
		{
			btBroadphasePair *pPair = prunedPairs + iPairIndex;

			if(pPair->GetManifold())
			{
				pPair->GetManifold()->Reset();
			}
		}
	}
	if(ms_ClearManifoldsFramesLeft > 0)
	{
		ms_ClearManifoldsFramesLeft--;
	}

#if __ASSERT
	// Check CCD handles rotation is not on outside interior - dumb check to see if activity scripts have turned this off after shutting down
//	if(FindPlayerPed() && ms_PhysSim->GetAutoCCDFirstContactOnlyEnabled())
//	{
//		Assertf(FindPlayerPed()->GetIsInInterior(),"Warning: AutoCCDFirstContactOnly is on when player is not inside an interior. Did an activity forget to clean up?");
//	}
#endif

	// Mark the collision records as out-of-date (But only on the first simulator iteration of a game frame)
	// We won't repopulate them till IterateOverManifolds() in SimUpdate()
	// but we have to wipe here before PreSimUpdate() marks active objects via NotifyHistoryWillBeCompleteThisTick()
	// - Otherwise for single iteration frames m_LastValidFrame of records won't match the global ms_CurrentFrame
	//  (And even multi-iteration frames would still have those out of sync until the next PreSimUpdate call which could result in wiped records)
	if(CPhysics::GetIsFirstTimeSlice(nSlice))
	{
		CCollisionHistory::ClearRecords();
	}

	//This must occur after the ProcessPhysics calls, as they can activate insts
	CCollisionHistory::PreSimUpdate();

	ms_RopeManager->UpdateViewport(gVpMan.GetCurrentViewport()->GetGrcViewport());
}


bool CPhysics::BreakGlass(CDynamicEntity *entity, Vec3V_In position, float radius, Vec3V_In impulse, float damage, int crackType, bool forceStoreHit)
{
	fragInst* pFragInst = (fragInst*)entity->GetCurrentPhysicsInst();

	if( pFragInst == NULL )
		return false;
		
	if( pFragInst->GetCached() == false)
	{
		pFragInst->PutIntoCache();
	}
	
	if( pFragInst->GetCached() == false ) // Still not in the cache ? abort!
		return false; 
		
	const fragCacheEntry* entry = pFragInst->GetCacheEntry();
	const fragHierarchyInst* hierInst = entry->GetHierInst();

	const fragHierarchyInst::GlassInfo* glassInfos = hierInst->glassInfos;
	
	bool hit = false;
	
	for(int glassIndex = 0; glassIndex != hierInst->numGlassInfos; ++ glassIndex)
	{
		const bgGlassHandle handle = glassInfos[glassIndex].handle;

		if(handle != bgGlassHandle_Invalid)						
		{
			const bgBreakable &breakable = bgGlassManager::GetGlassBreakable(handle);
			const Mat34V breakableMatrix = RCC_MAT34V(breakable.GetTransform());
			
			const Vec3V bsPos = UnTransformOrtho(breakableMatrix,position);
			
			bgBreakable::TriangleId breakTriId = bgBreakable::kInvalidTriangle;
			breakTriId = breakable.GetNextPaneTriangle(breakTriId);
			if(forceStoreHit) // Skip the intersection tests and just store this hit
			{
				bgGlassManager::HitGlass(handle, crackType, position, impulse);
				GLASS_RECORDER_ONLY(if(glassRecordingRage::IsActive()) { glassRecordingRage::Get()->OnHitGlass(handle, crackType, position, impulse); })
				hit = true;
			}
			else
			{
				while (breakTriId != bgBreakable::kInvalidTriangle)
				{
					Vector3 verts[3];
					Vector3 normal;

					if (breakable.GetPaneTriangleInfo(breakTriId, verts[0], verts[1], verts[2], normal))
					{
						const bool impact = geomSpheres::TestSphereToTriangle(Vec4V(bsPos, ScalarV(radius)), (const Vec3V*)verts, RCC_VEC3V(normal));
						if( impact )
						{
							bgGlassManager::HitGlass(handle, crackType, position, impulse);
							GLASS_RECORDER_ONLY(if(glassRecordingRage::IsActive()) { glassRecordingRage::Get()->OnHitGlass(handle, crackType, position, impulse); })
							hit = true;
							break; // No point in adding the same hit multiple times
						}
					}

					breakTriId = breakable.GetNextPaneTriangle(breakTriId);
				}
			}
		}
	}

	if( hit )
		return true;
		
	const fragPhysicsLOD* physics = pFragInst->GetTypePhysics();
	int numGroup = physics->GetNumChildGroups();
	const Mat34V entityMtx = entity->GetMatrix();
	
	hit = false;
	
	for(int groupIdx=0;groupIdx<numGroup;groupIdx++)
	{
		const fragTypeGroup *group = physics->GetAllGroups()[groupIdx];
		if (group->GetMadeOfGlass())
		{
			const int childFragIdx = group->GetChildFragmentIndex();
			const int numChildren = group->GetNumChildren();
			const phBoundComposite *bounds = entry->GetBound();
			
			for (int groupChildIndex = childFragIdx; groupChildIndex < childFragIdx + numChildren; groupChildIndex ++)
			{
				const phBound * bound = bounds->GetBound(groupChildIndex);
				if( bound )
				{
					const Vec3V boundMin = bound->GetBoundingBoxMin();
					const Vec3V boundMax = bound->GetBoundingBoxMax();
					
					const Mat34V objectMtx = bounds->GetCurrentMatrix(groupChildIndex);
					Mat34V mtx;
					Transform(mtx,entityMtx, objectMtx);
					
					const bool impact = geomBoxes::TestSphereToBox(position,ScalarV(radius),boundMin,boundMax,RCC_MATRIX34(mtx));
					
					if( impact )
					{
						const float health = group->GetDamageHealth();
						ms_SelectedCrack = crackType;
						pFragInst->ReduceGroupDamageHealth(	groupIdx, 
															health - damage, 
															position, 
															impulse);
															
						hit = true;
					}
				}
			}
		}
	}
	
	return hit;
}

//
// name:		CPhysics::InitSharedRagdollFragTypeData
// description:	
//
void CPhysics::InitSharedRagdollFragTypeData()
{
#if PHYSICS_LOD_GROUP_SHARING
	//////////////////////
	// Add reference fragTypes for types that share physics data
	//////////////////////
	strLocalIndex fred = g_FragmentStore.FindSlot(strStreamingObjectName("platform:/models/z_z_fred",0xCF8CE587));
	strLocalIndex wilma = g_FragmentStore.FindSlot(strStreamingObjectName("platform:/models/z_z_wilma",0x3E640874));
	strLocalIndex fred_large = g_FragmentStore.FindSlot(strStreamingObjectName("platform:/models/z_z_fred_large",0x8063EF5));
	strLocalIndex wilma_large = g_FragmentStore.FindSlot(strStreamingObjectName("platform:/models/z_z_wilma_large",0x9D471AB));
	strLocalIndex alien = g_FragmentStore.FindSlot(strStreamingObjectName("platform:/models/z_z_alien",0xE89296D8));
	CStreaming::LoadObject(fred, g_FragmentStore.GetStreamingModuleId());
	CStreaming::LoadObject(wilma, g_FragmentStore.GetStreamingModuleId());
	CStreaming::LoadObject(fred_large, g_FragmentStore.GetStreamingModuleId());
	CStreaming::LoadObject(wilma_large, g_FragmentStore.GetStreamingModuleId());
	CStreaming::LoadObject(alien, g_FragmentStore.GetStreamingModuleId());
	g_FragmentStore.AddRef(fred, REF_OTHER);
	g_FragmentStore.AddRef(wilma, REF_OTHER);
	g_FragmentStore.AddRef(fred_large, REF_OTHER);
	g_FragmentStore.AddRef(wilma_large, REF_OTHER);
	g_FragmentStore.AddRef(alien, REF_OTHER);

	m_ManagedPhysicsLODGroup[0].Init(g_FragmentStore.Get(fred)->GetPhysicsLODGroup(), fred.Get(), CreateInvalidRagdollFilter(*g_FragmentStore.Get(fred)));
	m_ManagedPhysicsLODGroup[1].Init(g_FragmentStore.Get(wilma)->GetPhysicsLODGroup(), wilma.Get(), CreateInvalidRagdollFilter(*g_FragmentStore.Get(wilma)));
	m_ManagedPhysicsLODGroup[2].Init(g_FragmentStore.Get(fred_large)->GetPhysicsLODGroup(), fred_large.Get(), CreateInvalidRagdollFilter(*g_FragmentStore.Get(fred_large)));
	m_ManagedPhysicsLODGroup[3].Init(g_FragmentStore.Get(wilma_large)->GetPhysicsLODGroup(), wilma_large.Get(), CreateInvalidRagdollFilter(*g_FragmentStore.Get(wilma_large)));
	m_ManagedPhysicsLODGroup[4].Init(g_FragmentStore.Get(alien)->GetPhysicsLODGroup(), alien.Get(), CreateInvalidRagdollFilter(*g_FragmentStore.Get(alien)));

#if __DEV
	if (PARAM_physicsrigsatload.Get())
	{
		// Load directly from XML files.  The physics LOD groups will not get released
		// when the fragType does due to PHYSICS_LOD_GROUP_SHARING.
		fragType *fredType = rage_new fragType;
		fragType *wilmaType = rage_new fragType;
		fragType *fredLargeType = rage_new fragType;
		fragType *wilmaLargeType = rage_new fragType;
		fragType *alienType = rage_new fragType;
		const char* fredEntity = "C:\\VRigs\\Fred\\entity\\entity.type";
		const char* wilmaEntity = "C:\\VRigs\\Wilma\\entity\\entity.type";
		const char* fredLargeEntity = "C:\\VRigs\\FredLarge\\entity\\entity.type";
		const char* wilmaLargeEntity = "C:\\VRigs\\WilmaLarge\\entity\\entity.type";
		const char* alienEntity = "C:\\VRigs\\Alien\\entity\\entity.type";
		const char* fredRig = "common:/data/ragdoll/ragdoll_type_male";
		const char* wilmaRig = "common:/data/ragdoll/ragdoll_type_female";
		const char* fredLargeRig = "common:/data/ragdoll/ragdoll_type_male_large";
		const char* wilmaLargeRig = "common:/data/ragdoll/ragdoll_type_female_large";
		const char* alienRig = "common:/data/ragdoll/ragdoll_type_alien";
		fredType->LoadWithXML(fredEntity, fredRig);
		wilmaType->LoadWithXML(wilmaEntity, wilmaRig);
		fredLargeType->LoadWithXML(fredLargeEntity, fredLargeRig);
		wilmaLargeType->LoadWithXML(wilmaLargeEntity, wilmaLargeRig);
		alienType->LoadWithXML(alienEntity, alienRig);
		m_ManagedPhysicsLODGroup[0].SetPhysicsLODGroup(fredType->GetPhysicsLODGroup());  // overwriting previous values but that's fine
		m_ManagedPhysicsLODGroup[1].SetPhysicsLODGroup(wilmaType->GetPhysicsLODGroup());
		m_ManagedPhysicsLODGroup[2].SetPhysicsLODGroup(fredLargeType->GetPhysicsLODGroup());
		m_ManagedPhysicsLODGroup[3].SetPhysicsLODGroup(wilmaLargeType->GetPhysicsLODGroup());
		m_ManagedPhysicsLODGroup[4].SetPhysicsLODGroup(alienType->GetPhysicsLODGroup());
		delete fredType;  // ditch the non-physics component of the types
		delete wilmaType;
		delete fredLargeType;  
		delete wilmaLargeType;  
		delete alienType; 
	}
#endif

	// Assign materials
	for (int humanType = 0; humanType <= 4; humanType++)
	{
		// Load high ragdoll LOD materials
		fragPhysicsLOD *fragLOD = m_ManagedPhysicsLODGroup[humanType].GetPhysicsLODGroup()->GetLODByIndex(0);
		phBoundComposite *composite = fragLOD->GetCompositeBounds();
		for (int ibound = 0; ibound < composite->GetNumBounds(); ibound++)
			composite->GetBound(ibound)->SetMaterial(PGTAMATERIALMGR->g_idButtocks + ibound);

		// Load medium ragdoll LOD materials
		fragLOD = m_ManagedPhysicsLODGroup[humanType].GetPhysicsLODGroup()->GetLODByIndex(1);
		if (fragLOD)
		{
			composite = fragLOD->GetCompositeBounds();
			for (int ibound = 0; ibound < composite->GetNumBounds(); ibound++)
			{
				int ragdollComponent = fragInstNM::MapRagdollLODComponentCurrentToHigh(ibound, 1);
				if (Verifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent))
				{
					composite->GetBound(ibound)->SetMaterial(PGTAMATERIALMGR->g_idButtocks + ragdollComponent);
				}
			}
		}

		// Load low ragdoll LOD materials
		fragLOD = m_ManagedPhysicsLODGroup[humanType].GetPhysicsLODGroup()->GetLODByIndex(2);
		if (fragLOD)
		{
			composite = fragLOD->GetCompositeBounds();
			for (int ibound = 0; ibound < composite->GetNumBounds(); ibound++)
			{
				int ragdollComponent = fragInstNM::MapRagdollLODComponentCurrentToHigh(ibound, 2);
				if (Verifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent))
				{
					composite->GetBound(ibound)->SetMaterial(PGTAMATERIALMGR->g_idButtocks + ragdollComponent);
				}
			}
		}

	}
#endif

	//////////////////////
	// create Natural Motion asset manager
	//////////////////////
	const char* artFileFred = "fred";
	const char* artFileWilma = "wilma";
	const char* artFileFredLarge = "fred_large";
	const char* artFileWilmaLarge = "wilma_large";
	const char* artFileAlien = "alien";
	const char *pPath = "platform:/data/naturalmotion";
#if __DEV
	PARAM_nmfolder.Get(pPath);
#endif
	CFileMgr::SetDir(pPath);

	fragNMAssetManager::Create();

	// different number of agents on different platforms
	FRAGNMASSETMGR->Load(artFileFred);
	FRAGNMASSETMGR->Load(artFileWilma);
	FRAGNMASSETMGR->Load(artFileFredLarge);
	FRAGNMASSETMGR->Load(artFileWilmaLarge);
	FRAGNMASSETMGR->Load(artFileAlien);
	CFileMgr::SetDir("");

	// load NM task parameters
	CTaskNMBehaviour::LoadTaskParameters();

	FRAGNMASSETMGR->SetIncomingAnimVelocityScale(1.0f);
	FRAGNMASSETMGR->SetFromAnimationMaxSpeed(ms_fRagdollActivationAnimVelClamp);
	FRAGNMASSETMGR->SetFromAnimationMaxAngSpeed(ms_fRagdollActivationAnimAngVelClamp);

	// Initialize the self-collision groups 
	FRAGNMASSETMGR->InitSelfCollisionsGroups(m_ManagedPhysicsLODGroup[0].GetPhysicsLODGroup()->GetLODByIndex(0));

	// test lower friction for ragdoll feet
	//const_cast<phMaterial*>(&PGTAMATERIALMGR->GetMaterial(PGTAMATERIALMGR->g_idFootLeft))->SetFriction(1.0f);
	//const_cast<phMaterial*>(&PGTAMATERIALMGR->GetMaterial(PGTAMATERIALMGR->g_idFootRight))->SetFriction(1.0f);
}

void CPhysics::ShutdownSharedRagdollFragTypeData()
{
#if PHYSICS_LOD_GROUP_SHARING
	for (int i = 0; i < kNumManagedPhysicsLODGroups; i++)
	{
		m_ManagedPhysicsLODGroup[i].Shutdown();
	}
#endif

	fragNMAssetManager::Destroy();
}

#if PHYSICS_LOD_GROUP_SHARING
CPhysics::ManagedPhysicsLODGroup& CPhysics::GetManagedPhysicsLODGroup(int iAssetID)
{
	physicsAssertf(iAssetID >= 0 && iAssetID < kNumManagedPhysicsLODGroups, "CPhysics::GetManagedPhysicsLODGroup: Invalid asset ID");
	
	return m_ManagedPhysicsLODGroup[iAssetID];
}
#endif

crFrameFilter* CPhysics::CreateInvalidRagdollFilter(const fragType& type)
{
	CMovePed::RagdollFilter* pInvalidRagdollFilter = rage_new CMovePed::RagdollFilter();

	const crSkeletonData& skeletonData = type.GetSkeletonData();

	pInvalidRagdollFilter->Init(skeletonData);

	fragPhysicsLOD* pPhysicsLOD = type.GetPhysics(0);

	int numChildren = pPhysicsLOD->GetNumChildren();
	int numBones = skeletonData.GetNumBones();

	// Unfortunately this information is only set up on the cache entry which hasn't been created yet
	// Should this information not live on the fragType and be set up at resource time??
	int* boneIndexToComponentMap = Alloca(int, numBones);
	int* componentToBoneIndexMap = Alloca(int, numChildren);
	sysMemSet(boneIndexToComponentMap, -1, sizeof(int) * numBones);

	// Find the bone index of each child
	for (int childIndex = 0; childIndex < numChildren; childIndex++)
	{
		int boneIndex;
		if (fragVerifyf(skeletonData.ConvertBoneIdToIndex(pPhysicsLOD->GetChild(childIndex)->GetBoneID(), boneIndex), "CPhysics::CreateInvalidRagdollFilter: Failed to convert component %i bone id %i to a bone index", childIndex, pPhysicsLOD->GetChild(childIndex)->GetBoneID()))
		{
			fragAssert(boneIndex < numBones && boneIndex >= 0);
			componentToBoneIndexMap[childIndex] = boneIndex;
			if (boneIndexToComponentMap[boneIndex] == -1)
			{
				boneIndexToComponentMap[boneIndex] = childIndex;
			}
		}
		else
		{
			componentToBoneIndexMap[childIndex] = -1;
		}
	}

	// Find the component index of any bone that doesn't have a direct link with a child
	for (int boneIndex = 0; boneIndex < numBones; boneIndex++)
	{
		for (int parentBoneIndex = skeletonData.GetParentIndex(boneIndex); parentBoneIndex != -1 && boneIndexToComponentMap[boneIndex] == -1; parentBoneIndex = skeletonData.GetParentIndex(parentBoneIndex))
		{
			boneIndexToComponentMap[boneIndex] = boneIndexToComponentMap[parentBoneIndex];
		}
	}

	for (int childIndex = 0; childIndex < numChildren; childIndex++)
	{
		if (componentToBoneIndexMap[childIndex] != -1)
		{
			// Walk up the hierarchy until we either get to the root bone or a bone that is mapped to the ragdoll rig
			// We're essentially looking for bones that aren't mapped to the ragdoll rig but that have a child AND parent bone that is mapped to the ragdoll rig
			// We add these bones to a filter so that animation on those bones can be blended out (blended to the default pose) when going into ragdoll
			// since the ragdoll rig doesn't handle non-mapped parent bones that aren't in the default pose
			for (int boneIndex = skeletonData.GetParentIndex(componentToBoneIndexMap[childIndex]); (boneIndex != -1 && boneIndexToComponentMap[boneIndex] != -1 && componentToBoneIndexMap[boneIndexToComponentMap[boneIndex]] != boneIndex) || boneIndex == 0; boneIndex = skeletonData.GetParentIndex(boneIndex))
			{
				pInvalidRagdollFilter->AddBoneIndex(boneIndex);
			}
		}
	}

	return pInvalidRagdollFilter;
}

//
// name:		CPhysics::Update
// description:	
//
void CPhysics::UpdateBreakableGlass()
{
#if __BANK
	if( ms_MouseBreakEnable )
	{
		Vector3 pos;
		CEntity * entity = CDebugScene::GetEntityUnderMouse(&pos);
		
		if( entity && entity->GetIsDynamic() )
		{
			CDynamicEntity *dyna = (CDynamicEntity*)entity;
			if( dyna->m_nDEflags.bIsBreakableGlass )
			{
				
				grcDebugDraw::Sphere(pos,ms_MouseBreakRadius,Color32(0xffffffff),false);
				if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
				{
					if(ms_MouseBreakHiddenHit)
					{
						bgGlassManager::SetSilentHit(true);
					}

					Vec3V direction = RCC_VEC3V(pos) - RCC_VEC3V(camInterface::GetPos());
					Vec3V velocity = Normalize(direction);
					ScalarV impulse(ms_MouseBreakImpulse);
					
					BreakGlass(dyna,RCC_VEC3V(pos),ms_MouseBreakRadius, velocity*impulse, ms_MouseBreakDamage, ms_MouseBreakCrackType);
					ms_MouseBreakPosition = pos;
					ms_MouseBreakVelocity = RCC_VECTOR3(velocity);

					if(bgGlassManager::IsSilentHit())
					{
						bgGlassManager::SetSilentHit(false);
					}
				}
			}
		}
	}
#endif // __BANK
	
	// breakable glass
	bgGlassManager::UpdateBuffers();

	bgGlassManager::SyncBreakableGlass();


}

#if RAGE_TIMEBARS && INCLUDE_DETAIL_TIMEBARS
static const char* s_Timeslices[] = {
	"Timeslice 1",
	"Timeslice 2",
	"Timeslice 3",
	"Timeslice 4",
	"Timeslice X"
};
#endif // RAGE_TIMEBARS

#if __PFDRAW
static sysIpcSema sPhysicsPFDDrawReady = NULL;
static sysIpcSema sPhysicsPFDDrawDone = NULL;
static sysCriticalSectionToken sPhysicsPFDSemaCritSec;
static u32 sPhysicsSynchronizationMaxWait = 200;

void UpdateSynchronizeRenderAndUpdateThreadSemas()
{
	sysCriticalSection critSec(sPhysicsPFDSemaCritSec);

	bool needsDrawReadySema = snPhysicsSynchronizeRenderAndUpdateThreads != 0;
	if (!PFDGROUP_Physics.WillDraw())
	{
		needsDrawReadySema = false;
	}

	if (!sPhysicsPFDDrawReady && needsDrawReadySema)
	{
		sPhysicsPFDDrawReady = sysIpcCreateSema(0);
	}
	else if (sPhysicsPFDDrawReady && !needsDrawReadySema)
	{
		sysIpcDeleteSema(sPhysicsPFDDrawReady);
		sPhysicsPFDDrawReady = 0;
	}

	bool needsDrawDoneSema = snPhysicsSynchronizeRenderAndUpdateThreads != 0;
	if (!PFDGROUP_Physics.WillDraw())
	{
		needsDrawDoneSema = false;
	}

	if (!sPhysicsPFDDrawDone && needsDrawDoneSema)
	{
		sPhysicsPFDDrawDone = sysIpcCreateSema(0);
	}
	else if (sPhysicsPFDDrawDone && !needsDrawDoneSema)
	{
		sysIpcDeleteSema(sPhysicsPFDDrawDone);
		sPhysicsPFDDrawDone = 0;
	}
}

void PhysicsSynchronizationPoint(int syncPoint)
{
	if (syncPoint == snPhysicsSynchronizeRenderAndUpdateThreads)
	{
		if (sPhysicsPFDDrawReady)
		{
			sysIpcSignalSema(sPhysicsPFDDrawReady);
		}

		if (sPhysicsPFDDrawDone)
		{
			if (!sysIpcWaitSemaTimed(sPhysicsPFDDrawDone, sPhysicsSynchronizationMaxWait))
			{
				sysCriticalSection critSec(sPhysicsPFDSemaCritSec);

				sysIpcDeleteSema(sPhysicsPFDDrawDone);
				sPhysicsPFDDrawDone = NULL;
			}
		}
	}
}
#else
__forceinline void UpdateSynchronizeRenderAndUpdateThreadSemas()
{
}

__forceinline void PhysicsSynchronizationPoint(int)
{
}
#endif

void UpdatePhysicsObjectsEKG()
{
#if __STATS
	int realVehicles = 0;
	int dummyVehicles = 0;
	int helicopters = 0;
	int peds = 0;
	int nmRagdolls = 0;
	int highLodRagdolls = 0;
	int medLodRagdolls = 0;
	int lowLodRagdolls = 0;
	int articulatedBodies = 0;
	int rigidBodies = 0;

	for( int activeIndex = PHLEVEL->GetNumActive() - 1; activeIndex >= 0; --activeIndex )
	{
		int levelIndex = PHLEVEL->GetActiveLevelIndex(activeIndex);
		phCollider* collider = PHSIM->GetCollider(levelIndex);
		if (collider->IsArticulated())
		{
			articulatedBodies++;
		}
		else
		{
			rigidBodies++;
		}

		phInst* inst = PHLEVEL->GetInstance(levelIndex);
		if (CEntity* entity = CPhysics::GetEntityFromInst(inst))
		{
			if (entity->GetIsTypePed())
			{
				CPed* ped = (CPed*)entity;
				if (ped->GetUsingRagdoll())
				{
					const fragInstNM *instNM = dynamic_cast<const fragInstNM*>(inst);
					if (instNM && instNM->GetNMAgentID() >= 0)
					{
						nmRagdolls++;
					}
					else
					{
						fragInst* frag = static_cast<fragInst*>(inst);
						switch(frag->GetCurrentPhysicsLOD())
						{
						case fragInst::RAGDOLL_LOD_HIGH:
							highLodRagdolls++;
							break;

						case fragInst::RAGDOLL_LOD_MEDIUM:
							medLodRagdolls++;
							break;

						case fragInst::RAGDOLL_LOD_LOW:
							lowLodRagdolls++;
							break;

						default:
							break;
						}
					}
				}
				else
				{
					peds++;
				}
			}
			else if (entity->GetIsTypeVehicle())
			{
				CVehicle* vehicle = (CVehicle*)entity;

				if (vehicle->IsDummy())
				{
					dummyVehicles++;
				}
				else
				{
					realVehicles++;
				}

				if (vehicle->InheritsFromHeli())
				{
					helicopters++;
				}
			}
		}
	}

	PF_SET(RealVehicles, realVehicles);
	PF_SET(DummyVehicles, dummyVehicles);
	PF_SET(Helicopters, helicopters);
	PF_SET(Peds, peds);
	PF_SET(NMRagdolls, nmRagdolls);
	PF_SET(HighLodRagdolls, highLodRagdolls);
	PF_SET(MedLodRagdolls, medLodRagdolls);
	PF_SET(LowLodRagdolls, lowLodRagdolls);
	PF_SET(Explosions, CExplosionManager::CountExplosions());
	PF_SET(ArticulatedBodies, articulatedBodies);
	PF_SET(RigidBodies, rigidBodies);
#endif
}

//
// name:		CPhysics::Update
// description:	
//
void CPhysics::Update()
{
	PF_FUNC(Phys_Total);

	PF_PUSH_TIMEBAR_DETAIL("PrePhysicsUpdate");

	bool disableRopeUpdate = false;
#if GTA_REPLAY
	//On replay playback we need to disable to rope update when we jump around the timeline
	//but still process the rest of the physics. (B*2156112) but not when fine scrubbing which is classed as a jump
	if( CReplayMgr::IsReplayCursorJumping() && !CReplayMgr::IsFineScrubbing() )
		disableRopeUpdate = true;
#endif //GTA_REPLAY

#if !USER_JBN	// Always time slice for profiling.
	NOTFINAL_ONLY(if(!PARAM_physicsupdates.Get() && !ms_OverriderNumTimeSlices))
	{
		// Have to restore number of timeslices first!
		// In case we ran more than 60 fps last update, but are less this update.
		ms_NumTimeSlices = DEFAULT_NUM_PHYS_TIMESLICES;
	}
#endif // !USER_JBN

	//if( ms_bInStuntMode &&
	//	!ms_OverriderNumTimeSlices )
	//{
	//	ms_NumTimeSlices = DEFAULT_NUM_PHYS_TIMESLICES_IN_STUNT_MODE;
	//}

	// Calculate the timestep and call fwTimer to set it here so that it 
	// will be available for use by anyone, specifically anyone that may 
	// get called back in the physics pre-update.
	float fTimeMult = 1.0f / float(ms_NumTimeSlices);
	const float fTimeStep = fwTimer::GetTimeStep();
	float fSimUpdateTimeStep = fTimeStep * fTimeMult;
	fwTimer::SetRagePhysicsUpdateTimeStep(fSimUpdateTimeStep);
#if !USER_JBN	// Always time slice for profiling.
	NOTFINAL_ONLY(if(!PARAM_physicsupdates.Get()))
	{
		// If time delta are less than a 60th of a second only update physics for one time slice
		const float fSingleSteppingMaxTimeSlice = (1.0f / 30.0f);

		const float fMinTimeSlice = 1.0f/60.0f;
		// Need to check the non-scaled time step here so this doesn't fire due to a time-warp
		if (fwTimer::GetSystemTimeStep() < fMinTimeSlice)
		{
			if (!ms_bIgnoreForcedSingleStepThisFrame)
			{
				ms_NumTimeSlices = 1;
				fSimUpdateTimeStep = rage::Min(fTimeStep, fSingleSteppingMaxTimeSlice);
				fwTimer::SetRagePhysicsUpdateTimeStep(fSimUpdateTimeStep);
			}
			ms_bIgnoreForcedSingleStepThisFrame = false;
		}
	}
#endif // !USER_JBN
	UpdateSynchronizeRenderAndUpdateThreadSemas();

	USE_MEMBUCKET(MEMBUCKET_PHYSICS);

	PhysicsSynchronizationPoint(PHYSICS_RENDER_UPDATE_SYNC_BEFORE_PRE_PHYSICS_UPDATE);

	perfClearingHouse::SetValue(perfClearingHouse::CHARACTER_CLOTHS, GetClothManager()->GetTotalActiveCharacterClothCount());
	perfClearingHouse::SetValue(perfClearingHouse::CHARACTER_CLOTH_VERTICES, GetClothManager()->GetTotalActiveCharacterClothVertexCount());

	perfClearingHouse::SetValue(perfClearingHouse::ENVIRONMENT_CLOTHS, GetClothManager()->GetTotalActiveEnvironmentClothCount());
	perfClearingHouse::SetValue(perfClearingHouse::ENVIRONMENT_CLOTH_VERTICES, GetClothManager()->GetTotalActiveEnvironmentClothVertexCount());
	
#if __BANK
	FRAGNMASSETMGR->ResetProfileTimers();
#endif

	UpdatePhysicsObjectsEKG();

	PF_START(Phys_Pre);
	// 8/7/12 - cthomas - We know only the main thread would possibly be updating physics at this 
	// point in time, so set the flag in the physics system to indicate that. This is an optimization 
	// that will allow us to skip some locking overhead in the physics system as we do various 
	// "callbacks" during the physics update - pre-physics, post-physics, and the pre- and post-sim 
	// update for each timeslice call back to various game objects to update them, which will only 
	// be done in the main thread. The physics system is not updating during this time.
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
	// Call pre physics updates on all physics entities
	fwSceneUpdate::Update(CGameWorld::SU_PHYSICS_PRE_UPDATE);
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
	PF_STOP(Phys_Pre);

	ms_RopeManager->ResetLength(ROPE_UPDATE_WITHSIM);

	PhysicsSynchronizationPoint(PHYSICS_RENDER_UPDATE_SYNC_BEFORE_PRESIM);

	// DW - just an idea for neatness, would be better if you passed me a viewport which you want populated with a suitable fragmanager viewport rather than I store it.
	if (const grcViewport *pViewport = gVpMan.CalcViewportForFragManager())
	{
		ms_FragManager->SetInterestFrustum(*pViewport, *gVpMan.GetFragManagerMatrix());
		bgGlassManager::SetViewport(pViewport);
	}

	PF_POP_TIMEBAR_DETAIL();

	for(int i=0; i<ms_NumTimeSlices; i++)
	{
		ms_TimeSliceId++;
		ms_CurrentTimeSlice = i;
		PDR_ONLY(debugPlayback::RecordNewPhysicsFrame();)

		PF_PUSH_TIMEBAR_DETAIL(s_Timeslices[rage::Min(i, NELEM(s_Timeslices)-1)]);

		if( !disableRopeUpdate )
			ms_RopeManager->UpdatePhysics(fwTimer::GetTimeStep(), ROPE_UPDATE_WITHSIM);

		PF_START_TIMEBAR_BUDGETED("PreSimUpdate",1.0f);
		PF_START(Phys_Pre_Sim);
		PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
		PreSimUpdate(fSimUpdateTimeStep, i);
		PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
		PF_STOP(Phys_Pre_Sim);

		PF_PUSH_TIMEBAR_BUDGETED("SimUpdate",3.5f);
		SimUpdate(fSimUpdateTimeStep, i);
		PF_POP_TIMEBAR();

		PF_START_TIMEBAR("PostSimUpdate");
		PF_START(Phys_Post_Sim);
		PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
		PostSimUpdate(i, fSimUpdateTimeStep);
		PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
		PF_STOP(Phys_Post_Sim);

		PF_POP_TIMEBAR_DETAIL();

		if (i == 0)
		{
			PhysicsSynchronizationPoint(PHYSICS_RENDER_UPDATE_SYNC_AFTER_FIRST_UPDATE);
		}
	}
	ms_CurrentTimeSlice = -1;
	ms_DisableOnlyPushOnLastUpdateForOneFrame = false;
	
#if USE_GJK_CACHE
	GJKCacheSystemPostCollisionUpdate(GJKCACHESYSTEM);
#endif

	PF_START_TIMEBAR_DETAIL("PostProcessAfterAllUpdates");

	PhysicsSynchronizationPoint(PHYSICS_RENDER_UPDATE_SYNC_AFTER_ALL_UPDATES);

	PF_START(Phys_Post);
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
	fwSceneUpdate::Update(CGameWorld::SU_PHYSICS_POST_UPDATE);
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
	PF_STOP(Phys_Post);

	BANK_ONLY(if(!CPhysics::ms_bFragManagerUpdateEachSimUpdate))
	{
		PF_START(Phys_RageFragUpdate);
		FRAGMGR->Update(fTimeStep,ms_NumTimeSlices);
		PF_STOP(Phys_RageFragUpdate);
	}

// NOTE: moved in void CGtaRenderThreadGameInterface::PerformSafeModeOperations()
//	ms_InstanceTasksManager->WaitForTasks();

	VerletSimPreUpdate( fwTimer::GetTimeStep() );
#if GTA_REPLAY
	CReplayMgr::PreClothSimUpdate(CGameWorld::FindLocalPlayer());
#endif // GTA_REPLAY

	VerletSimUpdate( clothManager::Environment | clothManager::Vehicle );

	if( !disableRopeUpdate )
	{
		RopeSimUpdate( fwTimer::GetTimeStep(), ROPE_UPDATE_EARLY );
	}

	
// NOTE: fix for B*1824939. this wait is not needed most likely anymore. there is another wait in core/game.cpp
//	WaitForVerletTasks();

	PhysicsSynchronizationPoint(PHYSICS_RENDER_UPDATE_SYNC_AFTER_POST_UPDATE);

#if __DEV
	if(ms_bRenderFocusImpulses)
	{
		RenderFocusEntityExternalForces();
	}
#endif	

//	TIME.SetSeconds(fOriginalSeconds, fOriginalInvSeconds);							// Can be removed once rage doesn't use TIME. anymore
}

#if NAVMESH_EXPORT

namespace
{
	void sNavMeshExportAppendToActiveBuildingsArray(atArray<CEntity*>& entityArray)
	{
		CNavMeshDataExporter::GetActiveExporter()->AppendToActiveBuildingsArray(entityArray);
	}
}

#endif	// NAVMESH_EXPORT

void CPhysics::UpdateRequests()
{
	// This code was moved out of CPhysics::Update() so it can be shared with
	// the NavMeshGenerator tool. /FF

	PF_START(Phys_Requests);

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
	{
		ScanAreaPtrArray(&sNavMeshExportAppendToActiveBuildingsArray);
	}
#endif

#if PHYSICS_REQUEST_LIST_ENABLED
	PF_START_TIMEBAR_DETAIL("UpdateRequestList");

	UpdatePhysicsRequestList();

	PF_STOP(Phys_Requests);
#endif	//PHYSICS_REQUEST_LIST_ENABLED

}

void CPhysics::VerletSimPreUpdate(float TimeSeconds)
{
	const grcViewport& grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();
	ms_ClothManager->SetCamPos( grcVp.GetCameraPosition() );
	ms_ClothManager->PreUpdate(grcVp, TimeSeconds);
}

void CPhysics::RopeSimUpdate(float TimeSeconds, int updateOrder )
{
	ms_RopeManager->UpdatePhysics(TimeSeconds, (enRopeUpdateOrder)updateOrder);

#if __BANK && !__RESOURCECOMPILER
	ms_RopeBankManager->Update( TimeSeconds );
#endif

	PF_START_TIMEBAR_DETAIL("RageRopeUpdate");
	PF_START(Phys_RageRopeUpdate);
	ms_RopeManager->UpdateCloth((enRopeUpdateOrder)updateOrder);
	PF_STOP(Phys_RageRopeUpdate);
}

void CPhysics::VerletSimUpdate( int verletType )
{
#if __BANK && !__RESOURCECOMPILER
	clothBankManager::PickVehicle();
	clothBankManager::PickPed();
#endif

	PF_START_TIMEBAR_DETAIL("RageClothUpdate");
	PF_START(Phys_RageClothUpdate);
	ms_ClothManager->Update( (clothManager::clothType) verletType );
	PF_STOP(Phys_RageClothUpdate);
}

void CPhysics::WaitForVerletTasks()
{
	PF_START_TIMEBAR_DETAIL("RageVerletWaitForTasks");
	PF_START(Phys_RageVerletWaitForTasks);
	ms_TasksManager->WaitForTasks();
	PF_STOP(Phys_RageVerletWaitForTasks);
}

void CPhysics::VerletSimPostUpdate()
{
	ms_ClothManager->PostUpdate();
	ms_RopeManager->PostUpdate();				// empty func ... do something ?!
}


void CPhysics::SimUpdate(float TimeSeconds, int nSlice)
{
	ms_bSimUpdateActive = true;
	
	// Update Rage Physics
	PF_START_TIMEBAR_DETAIL("RageSimUpdate");
	PF_START(Phys_RageSimUpdate);
	bool onlyPushOnLastUpdate = !ms_DisableOnlyPushOnLastUpdateForOneFrame && ms_OnlyPushOnLastUpdate && !ms_bInStuntMode;
	ms_PhysSim->Update(TimeSeconds, !onlyPushOnLastUpdate || nSlice == ms_NumTimeSlices - 1);
	PF_STOP(Phys_RageSimUpdate);

	ms_bSimUpdateActive = false;

	PF_START_TIMEBAR_DETAIL("IterateOverManifolds");
	PF_START(Phys_Iterate);
	IterateOverManifolds();
	PF_STOP(Phys_Iterate);

	PF_START_TIMEBAR_DETAIL("RageFragUpdate");
	PF_START(Phys_RageFragUpdate);
	FRAGMGR->SimUpdate(TimeSeconds);
#if __BANK
	if(CPhysics::ms_bFragManagerUpdateEachSimUpdate)
	{
		FRAGMGR->Update(TimeSeconds,1);
	}
#endif // __BANK
	PF_STOP(Phys_RageFragUpdate);
	
	PF_START_TIMEBAR_DETAIL("RageInstBehvrs");
	PF_START(Phys_RagInstBehvrs);
	ms_PhysSim->UpdateInstanceBehaviors(TimeSeconds);
	PF_STOP(Phys_RagInstBehvrs);

	if(CVehicleRecordingMgr::sm_bUpdateWithPhysics && !CVehicleRecordingMgr::sm_bUpdateBeforePreSimUpdate)
	{
		PF_START(Proc_VehRecording);
		CVehicleRecordingMgr::RetrieveDataForThisFrame();
		PF_STOP(Proc_VehRecording);
	}

#if __DEV
	if(sbSwitchPedToNM)
	{
		CTask* pTaskNM = rage_new CTaskNMBalance(3000, 6000, NULL, 0);
		CPedDebugVisualiserMenu::SwitchPedToNMBehaviour(pTaskNM);
		sbSwitchPedToNM = false;
	}
#endif
}

#define MAX_RAGDOLLS_TO_SWITCH (32)
dev_float sfMinKnockOffVehicleImpact = 5.0f;
dev_float sfMinKnockOffVehicleImpactInStuntMode = 30000.0f;
dev_float dfBackEndPopUpMaxXRange = 0.3f;
dev_float dfBackEndPopUpMinXRange = 0.6f;
dev_float dfBackEndPopUpMinOffsetMulti = 0.25f;

void CPhysics::PostSimUpdate(int nLoop, float fTimeStep)
{
	// the switch to ragdoll removes the animated inst from the physics world (and adds the ragdoll to the active list potentially)
	// so save a list of the inst's that want to activate, and do it later.
	phInst* aRagdollInstsToSwitch[MAX_RAGDOLLS_TO_SWITCH];
	int nNumRagdollInstsToSwitch = 0;
	CProjectile* aProjectiles[CProjectileManager::MAX_STORAGE];
	int nNumProjectilesToProcess = 0;
	ASSERT_ONLY(const int initialNumActive = CPhysics::GetLevel()->GetNumActive());
	for (int levelIndex = CPhysics::GetLevel()->GetFirstActiveIndex(); levelIndex != phInst::INVALID_INDEX; levelIndex = CPhysics::GetLevel()->GetNextActiveIndex())
	{
		phInst* pInst = CPhysics::GetLevel()->GetInstance(levelIndex);
		CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);

		if(pEntity)
		{
			if(pEntity->GetIsTypePed())
			{
				CPed* pPed = (CPed *)pEntity;
				CCollisionHistory* pCollisionHistory = pPed->GetFrameCollisionHistory();
				//run over all the currently active ragdolls
				if(pInst == pPed->GetRagdollInst() && pPed->GetRagdollState() > RAGDOLL_STATE_ANIM_DRIVEN && !pPed->GetIsInPopulationCache())
				{
					const CCollisionRecord* pMostSignificantRecord = pCollisionHistory->GetMostSignificantCollisionRecord();

					// has a collision with another object happened this frame (e.g. hit by a car / flying object)
					if(pMostSignificantRecord)
					{
						// Process the damage and, if necessary, generate an appropriate ragdoll event based on the impact.
						pPed->GetPedIntelligence()->GetEventScanner()->GetCollisionEventScanner().ProcessRagdollImpact(pPed, 
							pCollisionHistory->GetCollisionImpulseMagSum(), 
							pMostSignificantRecord->m_pRegdCollisionEntity.Get(), 
							pMostSignificantRecord->m_MyCollisionNormal,
							pMostSignificantRecord->m_MyCollisionComponent,
							pMostSignificantRecord->m_OtherCollisionMaterialId);
						pCollisionHistory->SetZeroCollisionImpulseMagSum();

						Assertf(pPed->VerifyRagdollHandled(), "A ragdoll has been activated by a collision record, but ProcessRagdollImpact didn't return a ragdoll event!");

						if(!pPed->GetUsingRagdoll())
						{
							if(nNumRagdollInstsToSwitch < MAX_RAGDOLLS_TO_SWITCH)
							{
								aRagdollInstsToSwitch[nNumRagdollInstsToSwitch] = pInst;
								nNumRagdollInstsToSwitch++;
							}
							else
								Assertf(false, "ran out of ragdolls to switch on");
						}
					}
					else if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE)
					{
						// No impact recorded, but the ragdoll has been activated during this physics step anyway.
						// if a ragdoll event hasn't been provided yet, we must add a default one here.
						pPed->GetPedIntelligence()->GetEventScanner()->GetCollisionEventScanner().ProcessRagdollImpact(
							pPed, 0.0f, NULL, Vector3(Vector3::ZeroType), 0, MATERIALMGR.GetMaterialId(MATERIALMGR.GetDefaultMaterial()));

						Assertf(pPed->VerifyRagdollHandled(), "A ragdoll has been newly activated, but ProcessRagdollImpact didn't return a ragdoll event!");
						
						pCollisionHistory->SetZeroCollisionImpulseMagSum();

						if(!pPed->GetUsingRagdoll())
						{
							if(nNumRagdollInstsToSwitch < MAX_RAGDOLLS_TO_SWITCH)
							{
								aRagdollInstsToSwitch[nNumRagdollInstsToSwitch] = pInst;
								nNumRagdollInstsToSwitch++;
							}
							else
								Assertf(false, "ran out of ragdolls to switch on");
						}
					}
					else
					{
						// The ragdoll should have been already activated and switched to nm
						Assertf(pPed->VerifyRagdollHandled(), "Unmanaged active ragdoll detected!");
					}
				}
			}
			else if(pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
				const CCollisionHistory* pCollisionHistory = pVehicle->GetFrameCollisionHistory();

				// Want all bike impacts to be processed
				bool easyToLand = pVehicle->m_easyToLand &&
									( !CPhysics::ms_bInArenaMode ||
									  CVehicle::GetVehicleOrientation( *pVehicle ) != CVehicle::VO_UpsideDown ||
									  pVehicle->GetNumContactWheels() > 0 );

				bool bIsLikeABikeForPassengerKnockOff = !easyToLand &&
														( ( pVehicle->InheritsFromBike() && static_cast< CBike* >( pVehicle )->m_fOffStuntTime == 0.0f ) || 
														  pVehicle->InheritsFromQuadBike() || 
														  pVehicle->GetIsJetSki() ||
														  pVehicle->InheritsFromAmphibiousQuadBike() );


				bool bIngoreUpsideDownCheck = easyToLand &&
												pVehicle->InheritsFromAmphibiousQuadBike() &&
												!pVehicle->IsInAir();

				float fKnockoffMinImpulse = bIsLikeABikeForPassengerKnockOff ? 0.0f : sfMinKnockOffVehicleImpact;

				const CCollisionRecord* pMostSignificantRecord = pCollisionHistory->GetMostSignificantCollisionRecord();

				if( easyToLand ||
					( pVehicle->InheritsFromBike() &&
					  !pVehicle->HasGlider() &&
					  pVehicle->GetSpecialFlightModeRatio() > 0.5f ) )
				{
					fKnockoffMinImpulse = sfMinKnockOffVehicleImpactInStuntMode;
				}
				else if( pMostSignificantRecord &&
						 pMostSignificantRecord->m_pRegdCollisionEntity.Get() &&
						 pMostSignificantRecord->m_pRegdCollisionEntity.Get()->GetIsTypeVehicle() &&
						 static_cast< CVehicle* >( pMostSignificantRecord->m_pRegdCollisionEntity.Get() )->InheritsFromBike() )
				{
					static dev_float minImpulseBikeVsBike = 100.0f;
					fKnockoffMinImpulse = minImpulseBikeVsBike;
				}

				audVehicleAudioEntity * audio = pVehicle->GetVehicleAudioEntity();
				if(naVerifyf(audio, "Vehicle missing audio entity during post sim update"))
				{
					audio->GetCollisionAudio().PostProcessImpacts();
				}

				static dev_float sfMinKnockOffVehicleImpactWithRampCar = 30000.0f;
				if( bIsLikeABikeForPassengerKnockOff && pVehicle->GetDriver() && pVehicle->GetDriver()->IsAPlayerPed() && pMostSignificantRecord )
				{
					CEntity* pCollisionEntity = pMostSignificantRecord->m_pRegdCollisionEntity.Get();

					if(pCollisionEntity->GetIsTypeVehicle())
					{
						CVehicle* pCollisionVehicle = static_cast<CVehicle*>(pCollisionEntity);

						if(pCollisionVehicle->HasRamp() )
						{
							fKnockoffMinImpulse = sfMinKnockOffVehicleImpactWithRampCar;
						}
					}
				}

				if(pMostSignificantRecord
				&& (pCollisionHistory->GetCollisionImpulseMagSum() > fKnockoffMinImpulse * pVehicle->GetMass() || ( !bIngoreUpsideDownCheck && pVehicle->GetTransform().GetC().GetZf() < 0.0f ) ) )
				{

					bool bFellOff = false;

					for( s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++ )
					{
						CPed* pRiderPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
						if(pRiderPed)
						{
							if(pRiderPed->GetPedIntelligence()->GetEventScanner()->GetCollisionEventScanner().ProcessVehicleImpact(pRiderPed,
										pMostSignificantRecord->m_pRegdCollisionEntity.Get(), 
										pCollisionHistory->GetCollisionImpulseMagSum(), 
										pMostSignificantRecord->m_MyCollisionNormal, 
										pMostSignificantRecord->m_MyCollisionComponent, bFellOff, i))
							{
								// if one ped falls off, force the other to as well (we do the driver first, so this just means to avoid expensive checks we already did for the driver)
								bFellOff = true;

								if(!pRiderPed->GetUsingRagdoll())
								{
									// ok we want to get knocked off and switch to ragdoll - do it after this loop
									if(nNumRagdollInstsToSwitch < MAX_RAGDOLLS_TO_SWITCH)
									{
										aRagdollInstsToSwitch[nNumRagdollInstsToSwitch] = pRiderPed->GetRagdollInst();
										nNumRagdollInstsToSwitch++;
									}
									else
										Assertf(false, "ran out of ragdolls to switch on");
								}

								Assertf(pRiderPed->VerifyRagdollHandled(), "A rider ragdoll has been activated by a vehicle collision, but no ragdoll event was generated!");

							}
						}
					}
				}

				// Make the back end of the car tilting up a bit when receives head on collision
				if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR && pVehicle->pHandling->GetCarHandlingData()  && !pVehicle->GetIsAnyFixedFlagSet() && pVehicle->GetCollider() && pMostSignificantRecord)
				{
					const Matrix34& matMyMatrix = RCC_MATRIX34(pVehicle->GetMatrixRef());
					// if car is running forward
					if(VEC3V_TO_VECTOR3(pVehicle->GetCollider()->GetLastVelocity()).Dot(matMyMatrix.b) > 0.0f)
					{
						if(pMostSignificantRecord->m_MyCollisionPosLocal.y > 0.0f)
						{
							CEntity *pHitEnt = pMostSignificantRecord->m_pRegdCollisionEntity.Get();
							if((pHitEnt && (pHitEnt->GetIsTypeVehicle() && ((CVehicle *)pHitEnt)->GetVehicleType() == VEHICLE_TYPE_CAR) && !pHitEnt->GetIsAnyFixedFlagSet()) || 
								((pHitEnt->GetCurrentPhysicsInst() && pHitEnt->GetCurrentPhysicsInst()->GetClassType() == PH_INST_FRAG_BUILDING) || pHitEnt->GetCurrentPhysicsInst()->GetClassType() == PH_INST_BUILDING))
							{
								Vector3 vTransferredImpulseOffset = pMostSignificantRecord->m_MyCollisionPosLocal;
								vTransferredImpulseOffset.x *= -1.0f;
								vTransferredImpulseOffset.y *= -1.0f;
								matMyMatrix.Transform3x3(vTransferredImpulseOffset); // transform to global rotational space
								if(vTransferredImpulseOffset.Mag2() > square(pVehicle->GetBoundRadius()))
								{
									vTransferredImpulseOffset *= pVehicle->GetBoundRadius() / vTransferredImpulseOffset.Mag();
								}
								Vector3 vTransferredImpulse(VEC3_ZERO);
								float fImpulseOffsetMult = 1.0f - Clamp((Abs(pMostSignificantRecord->m_MyCollisionPosLocal.x) - dfBackEndPopUpMinXRange) / (dfBackEndPopUpMaxXRange - dfBackEndPopUpMinXRange), 0.0f, 1.0f);
								fImpulseOffsetMult = dfBackEndPopUpMinOffsetMulti + (1.0f - dfBackEndPopUpMinOffsetMulti) * fImpulseOffsetMult;
								if(pHitEnt && pHitEnt->GetIsTypeVehicle() && ((CVehicle *)pHitEnt)->GetVehicleType() == VEHICLE_TYPE_CAR)
								{
									if( !( static_cast< CVehicle* >( pHitEnt )->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_RAMMING_SCOOP ) ) )
									{
										vTransferredImpulse.z = pMostSignificantRecord->m_fCollisionImpulseMag * pVehicle->pHandling->GetCarHandlingData()->m_fBackEndPopUpCarImpulseMult * fImpulseOffsetMult;
									}
								}
								else
								{
									vTransferredImpulse.z = pMostSignificantRecord->m_fCollisionImpulseMag * pVehicle->pHandling->GetCarHandlingData()->m_fBackEndPopUpBuildingImpulseMult * fImpulseOffsetMult;
								}
								vTransferredImpulse.z = Min(vTransferredImpulse.z, pVehicle->pHandling->GetCarHandlingData()->m_fBackEndPopUpMaxDeltaSpeed * pVehicle->GetMass());
								pVehicle->ApplyImpulse(vTransferredImpulse, vTransferredImpulseOffset);
								Vector3 vCGPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
								if(CPhysics::GetLevel()->IsInactive(pVehicle->GetVehicleFragInst()->GetLevelIndex()) && pVehicle->GetCollider())
								{
									vCGPosition = VEC3V_TO_VECTOR3(pVehicle->GetCollider()->GetPosition());
								}
								// Apply equal but opposite impulse to the centre of mass to prevent car being launched off
								pVehicle->ApplyImpulse(vTransferredImpulse * -1.0f, VEC3_ZERO);
							}
						}
					}
				}

				// Check for an opportunity to function as a simple collider
				const phCollider* collider = NULL;
				if (pVehicle->GetVehicleFragInst() && pVehicle->GetVehicleFragInst()->GetCacheEntry() && pVehicle->GetVehicleFragInst()->GetCacheEntry()->GetHierInst())
				{
					collider = pVehicle->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider;
					if(collider)
					{
						int initialType = collider->GetType();
						pVehicle->GetVehicleFragInst()->CheckSimpleColliderMode();
						int finalType = collider->GetType();
						if(finalType == phCollider::TYPE_RIGID_BODY && initialType != phCollider::TYPE_RIGID_BODY && !CVehicle::ms_bAlwaysUpdateCompositeBound)
						{
							pVehicle->CalculateNonArticulatedMaximumExtents();
						}
					}
				}
			}
			else if(pEntity->GetIsTypeObject())
			{
				CObject* pObj = static_cast<CObject*>(pEntity);
				CProjectile* pProj = pObj->GetAsProjectile();
				if(pProj)
				{
					aProjectiles[nNumProjectilesToProcess++] = pProj;
				}
			}
		}
		// If this assert fails you need to either a) figure out who's changing the number of active objects and make them stop doing that or b) cache
		//   the active objects beforehand and use the cached list like it done in the loop below.
//		Assertf(CPhysics::GetLevel()->GetNumActive() <= initialNumActive, "Somebody increased the number of active object during iteration!");
		Assertf(CPhysics::GetLevel()->GetNumActive() >= initialNumActive, "Somebody decreased the number of active object during iteration!");
	}
	for(int i=0; i<nNumRagdollInstsToSwitch; i++)
	{
		CEntity* pEntity = CPhysics::GetEntityFromInst(aRagdollInstsToSwitch[i]);
		CPed* pPed = (CPed *)pEntity;
		if (pPed->GetIsInPopulationCache())
			continue;

		Assertf(pPed->VerifyRagdollHandled(), "Performing a deferred activation of the ragdoll, but no task or event exists to handle it!");
		// special case function that checks for the specific RAGDOLL_STATE_PHYS_ACTIVATE status
		// should only be called here.
		// Our switch to nm event will have been sent at the point where the above flag was set.
		pPed->SwitchToRagdollPostPhysicsSimUpdate();

		// get clone peds to try to balance for starters when their ragdoll gets activated
		if(pPed->IsNetworkClone() && pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetNMAgentID() > -1)
		{
			ART::MessageParams msg;
			msg.addBool(NMSTR_PARAM(NM_START), true);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_MSG), &msg);
		}
	}

	for(int j=0; j<nNumProjectilesToProcess; j++)
	{
		aProjectiles[j]->PostSimUpdate();
	}

	// Cache off a list of the active objects first rather than use the iterator 'live' because the UpdateEntityFromPhysics() call below can deactivate
	//   objects which will mess with the active object list and confuse the iteration.
	const phLevelNew *physicsLevel = CPhysics::GetLevel();
	const int numActive = physicsLevel->GetNumActive();
	int *activeLevelIndices = Alloca(int, numActive);
	for(int activeIndex = 0; activeIndex < numActive; ++activeIndex)
	{
		activeLevelIndices[activeIndex] = physicsLevel->GetActiveLevelIndex(activeIndex);
	}

	// need to update entity matrices after physics update
	int activeIndex = 0;
	int levelIndex = activeLevelIndices[0];
	for (; activeIndex < numActive; ++activeIndex, levelIndex = activeLevelIndices[activeIndex])
	{
		phInst* pInst = CPhysics::GetLevel()->GetInstance(levelIndex);
		CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);

		if(pEntity)
		{
            // run the network blender on network clones on the last physics update loop
			if(CPhysics::GetIsLastTimeSlice(nLoop))
            {
                netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

			    if (networkObject && networkObject->IsClone() && networkObject->GetNetBlender())
			    {
				    if (networkObject->CanBlend())
                    {
					    networkObject->GetNetBlender()->ProcessPostPhysics();
                    }
			    }
            }
			// Still want to call UpdateEntityFromPhysics for asleep insts because they can move a small amount each frame
			// e.g. if they're getting pushed.  We're expecting them to become inactive soon after going to sleep anyway.
			// if(!CPhysics::GetSimulator()->GetCollider(levelIndex)->GetSleep()
			// || !CPhysics::GetSimulator()->GetCollider(levelIndex)->GetSleep()->IsAsleep())
			if(Verifyf(pEntity->GetIsPhysical(), "Active entity is not physical."))
			{
				// update entity's matrix 
				static_cast<CPhysical*>(pEntity)->UpdateEntityFromPhysics(pInst, nLoop);
#if ENABLE_PIX_TAGGING
				// time ped and vehicle probes separately
				if(pEntity->GetIsTypePed())
					PIXBeginC(1, 0xFFFF,"CPed::ProcessProbes");
				else if(pEntity->GetIsTypeVehicle())
					PIXBeginC(1, 0xFFFF,"CVehicle::ProcessProbes");
#endif
				// do peds standing, vehicle suspension lines etc..
				((CPhysical *)pEntity)->ProcessProbes(MAT34V_TO_MATRIX34(pEntity->GetMatrix()));

#if ENABLE_PIX_TAGGING
				// end timers
				if(pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle())
					PIXEnd();
#endif
			}

			if(pEntity->GetIsPhysical() && ((CPhysical*)pEntity)->GetNoCollisionEntity())
			{
				((CPhysical*)pEntity)->ResetNoCollision();
			}

			// Handle the adding of objects to the pathfinder, which were not added previously
			// but have not moved/reorientated since then.
			if(pEntity->WasUnableToAddToPathServer())
			{
				CPathServerGta::MaybeAddDynamicObject(pEntity);
			}
		}
	}

	FRAGNMASSETMGR->StepPhase2(fTimeStep); //fwTimer::GetTimeStep());

	CExplosionManager::Update(fTimeStep);

	CPropellerCollisionProcessor::GetInstance().ProcessImpacts(fTimeStep);

	// Switch sleeping corpses to animated
	for (int i = 0; i < g_SettledPeds.GetCount(); i++)
	{
		CPed* pPed = g_SettledPeds[i];
		if (pPed)
		{
			Assert(pPed->GetRagdollInst());
			pPed->DeactivatePhysicsAndDealWithRagdoll(true);
		}
	}
	g_SettledPeds.Reset();
}

void CPhysics::CommitDeferredOctreeUpdates()
{
	GetLevel()->CommitDeferredOctreeUpdates();
}

void CPhysics::ProcessDeferredCompositeBvhUpdates()
{
#if LEVELNEW_ENABLE_DEFERRED_COMPOSITE_BVH_UPDATE
	GetLevel()->ProcessDeferredCompositeBvhUpdates();
#endif // LEVELNEW_ENABLE_DEFERRED_COMPOSITE_BVH_UPDATE
}

// name:		IterateManifold
// description:	Extract information from one manifold for game events.
// notes:
//   1. Primary purposes are expected to be generating sounds and particles, replacing what was once
//      done in PreApplyImpacts.
//   2. Each composite part of every object will get its own manifold when colliding with something.
//   3. Manifolds are allocated as soon as a pair of objects overlaps in the broadphase, whether they
//      are colliding or not.

void CPhysics::IterateManifold(phManifold* manifold, CEntity* pEntityA, CEntity* pEntityB, phInst* pInstA, phInst* pInstB)
{
	// Here is where you would put code that is related to the entire manifold (aka contact patch)
	// between the two objects. Some things you can find out at this level:
	//		static vs. sliding friction (scraping vs. rolling or bouncing)
	//		the two objects involved in the collision or constraint, and their component part numbers
	//		which contact point was most recently addedgta
	//		a composite of the normal and tangential forces over the manifold

//	s32 newestContactPointIndex = manifold->GetNewestContactPoint();
//	if (newestContactPointIndex==-1)
//	{
//		return;
//	}

	Assert(pInstA && pInstA->IsInLevel() && pInstA->GetLevelIndex() == manifold->GetLevelIndexA());
	Assert(pInstB && pInstB->IsInLevel() && pInstB->GetLevelIndex() == manifold->GetLevelIndexB());

	const ScalarV svOne(V_ONE);

	// First contact loop, find the accumulated and average impulse
	ScalarV vAccumImpulse = ScalarV(V_ZERO);
	ScalarV vAvgActiveImpulse = ScalarV(V_ZERO);
	ScalarV numContactsActive = ScalarV(V_ZERO);
	bool anyActiveContacts = false;
	int numContacts = manifold->GetNumContacts();
	for (int i=0; i<numContacts; i++)
	{
		phContact& contact = manifold->GetContactPoint(i);

		if (contact.IsContactActive())
		{
			// accumulate the impulses across all active contacts
			vAccumImpulse += Abs(contact.GetAccumImpulse());
			numContactsActive = Add(numContactsActive,svOne);
			anyActiveContacts = true;
		}
	}

	// Only continue if we found active contacts
	if (anyActiveContacts)
	{
		vAvgActiveImpulse = InvScale(vAccumImpulse,numContactsActive);

		Mat34V transformA;
		Mat34V transformB;

		// Check that the component indices makes sense before continuing. The indices could be invalid
		//   if the object changes LOD or breaks without calling phContactMgr::ReplaceInstanceComponent or RemoveAllContactsWithInstance.
		// This shouldn't cost that much since phManifold::GetLocalToWorldTransforms has to look at the composite anyways. 
		const phBound* boundA = pInstA->GetArchetype()->GetBound();
		if(boundA->GetType() == phBound::COMPOSITE)
		{
			int componentA = manifold->GetComponentA();
			const phBoundComposite* compositeBoundA = static_cast<const phBoundComposite*>(boundA);
			if(!Verifyf(componentA >= 0 && componentA < compositeBoundA->GetMaxNumBounds() && compositeBoundA->GetBound(componentA), "Invalid component index %i for '%s' on manifold. Num Components: %i.", componentA, pInstA->GetArchetype()->GetFilename(), compositeBoundA->GetNumBounds()))
			{
				return;
			}
			Transform(transformA,pInstA->GetMatrix(),compositeBoundA->GetCurrentMatrix(componentA));
		}
		else
		{
			transformA = pInstA->GetMatrix();
		}
		const phBound* boundB = pInstB->GetArchetype()->GetBound();
		if(boundB->GetType() == phBound::COMPOSITE)
		{
			int componentB = manifold->GetComponentB();
			const phBoundComposite* compositeBoundB = static_cast<const phBoundComposite*>(boundB);
			if(!Verifyf(componentB >= 0 && componentB < compositeBoundB->GetMaxNumBounds() && compositeBoundB->GetBound(componentB), "Invalid component index %i for '%s' on manifold. Num Components: %i.", componentB, pInstB->GetArchetype()->GetFilename(), compositeBoundB->GetNumBounds()))
			{
				return;
			}
			Transform(transformB,pInstB->GetMatrix(),compositeBoundB->GetCurrentMatrix(componentB));
		}
		else
		{
			transformB = pInstB->GetMatrix();
		}

#if DEFORMABLE_OBJECTS
		bool shouldProcessDeformation = pEntityA != pEntityB &&
			((pEntityA && pEntityA->GetIsTypeObject() && static_cast<CObject*>(pEntityA)->HasDeformableData())
			|| (pEntityB && pEntityB->GetIsTypeObject() && static_cast<CObject*>(pEntityB)->HasDeformableData()));
#endif // DEFORMABLE_OBJECTS

		// Second contact loop (requires average and accumulated impulse)
		for(int contactIndex = 0; contactIndex < manifold->GetNumContacts(); contactIndex++)
		{
			phContact& contact = manifold->GetContactPoint(contactIndex);

			// This is where you would put code that needs to operate on the actual points of contact within
			// the manifold. You will find this information here:
			//		the positions of contact points between the two objects
			//		the lifetime (in sim iterations) of each contact
			//		the normal direction of the contact
			//		whether the contact is active (e.g. ped probe contacts will be disabled)
			//		the element number (polygon index) when colliding with world polygons
			//		the depth of the contact
			//		the impulse computed by the force solver most recently
			//		the push applied by the push solver most recently

			// Some test code to make sure this is working
			// int youth = Max(0, 255 - contact.m_LifeTime);
			// grcDebugDraw::Sphere(contact.GetWorldPosA(), 0.02f, Color32(youth, 0, 255 - youth), false);

			if(contact.IsContactActive())
			{
				contact.RefreshContactPoint(RCC_MATRIX34(transformA), RCC_MATRIX34(transformB), manifold);

				// check the validity of the normal
#if __ASSERT
				float normalMag = Mag(contact.GetWorldNormal()).Getf();
				if(normalMag < 0.97f || normalMag > 1.03f)
				{
					manifold->AssertContactNormalVerbose(contact, __FILE__, __LINE__);
				}
#endif // __ASSERT

 				// process vfx per contact
#if __BANK
				if(g_vfxMaterial.GetColnVfxProcessPerContact())
				{
					// only process non ped collisions
					if (pInstA->GetClassType()!=PH_INST_PED && 
						pInstB->GetClassType()!=PH_INST_PED)
					{
						// don't process any collisions with inst that don't have an entity
						if (pEntityA==NULL || pEntityB==NULL)
						{
							continue;
						}

						// don't process any collisions with non frag insts when there is a frag inst
						if ((pEntityA->m_nFlags.bIsFrag && !IsFragInst(pInstA)) || 
							(pEntityB->m_nFlags.bIsFrag && !IsFragInst(pInstB)))
						{
							continue;
						}

						VfxCollisionInfo_s vfxColnInfo;

						vfxColnInfo.pEntityA = pEntityA;
						vfxColnInfo.pEntityB = pEntityB;
						vfxColnInfo.componentIdA = manifold->GetComponentA();
						vfxColnInfo.componentIdB = manifold->GetComponentB();
						vfxColnInfo.pColliderA = manifold->GetColliderA();
						vfxColnInfo.pColliderB = manifold->GetColliderB();
						vfxColnInfo.vRelVelocity = phContact::ComputeRelVelocity( contact.GetWorldPosA(), contact.GetWorldPosB(), manifold);

						vfxColnInfo.vWorldPosA = contact.GetWorldPosA();
						vfxColnInfo.vWorldPosB = contact.GetWorldPosB();
						vfxColnInfo.vWorldNormalA = contact.GetWorldNormal();
						vfxColnInfo.vWorldNormalB = -contact.GetWorldNormal();
						vfxColnInfo.mtlIdA = PGTAMATERIALMGR->UnpackMtlId(contact.GetMaterialIdA());
						vfxColnInfo.mtlIdB = PGTAMATERIALMGR->UnpackMtlId(contact.GetMaterialIdB());
						vfxColnInfo.vAccumImpulse = Abs(contact.GetAccumImpulse());

						if (g_vfxMaterial.GetColnVfxRecomputePositions())
						{
							vfxColnInfo.vWorldPosA = UnTransformOrtho(pInstA->GetMatrix(), contact.GetWorldPosA());
							vfxColnInfo.vWorldPosA = VECTOR3_TO_VEC3V(vfxColnInfo.pEntityA->TransformIntoWorldSpace(RCC_VECTOR3(vfxColnInfo.vWorldPosA)));
							vfxColnInfo.vWorldPosB = UnTransformOrtho(pInstB->GetMatrix(), contact.GetWorldPosB());
							vfxColnInfo.vWorldPosB = VECTOR3_TO_VEC3V(vfxColnInfo.pEntityB->TransformIntoWorldSpace(RCC_VECTOR3(vfxColnInfo.vWorldPosB)));
						}

						CPhysics::ProcessCollisionVfxandSfx(vfxColnInfo);
					}
				}
#endif // __BANK

				// Do this after VFX because this can actually invalidate the contact we're on,
				// if an instance is deleted in here (e.g. from disappears when dead).
				CPhysics::ProcessCollisionData(contact, *manifold, pEntityA, pEntityB, vAvgActiveImpulse);

#if DEFORMABLE_OBJECTS
				if(shouldProcessDeformation)
				{
					CDeformableObjectManager& refDeformableObjMgr = CDeformableObjectManager::GetInstance();

					// Need to process each individual contact's position to see if it falls within the sphere of influence of
					// a deformable bone on either instance of the collision pair.
					refDeformableObjMgr.ProcessDeformableBones(pInstA, Scale(contact.GetWorldNormal(0), vAvgActiveImpulse),
						contact.GetWorldPosA(), pEntityB, manifold->GetComponentA(), manifold->GetComponentB());
					refDeformableObjMgr.ProcessDeformableBones(pInstB, Scale(contact.GetWorldNormal(1), vAvgActiveImpulse),
						contact.GetWorldPosB(), pEntityA, manifold->GetComponentB(), manifold->GetComponentA());
				}
#endif // DEFORMABLE_OBJECTS
			}
		} // Loop over individual contacts.


#if __BANK
		if ((g_vfxMaterial.GetColnVfxDisableTimeSlice1() && CPhysics::GetCurrentTimeSlice()==0) || 
			(g_vfxMaterial.GetColnVfxDisableTimeSlice2() && CPhysics::GetCurrentTimeSlice()==1))
		{
			return;
		}
#endif // __BANK

		// process vfx per manifold 
#if __BANK
		if (g_vfxMaterial.GetColnVfxProcessPerContact()==false)
#endif // __BANK
		{
			// A non-zero accumulated impulse is required
			if(vAccumImpulse.Getf()>0.0f)
			{
				// check if we should process this collision
				bool shouldProcessVfx = false, shouldProcessAudio = false;

				// only process collisions involving ped capsules if the ped is swimming and the camera is underwater
				bool isPedCapsuleColn = false;
				if (pInstA->GetClassType()==PH_INST_PED)
				{
					isPedCapsuleColn = true;
					if (pEntityA && CVfxHelper::GetGameCamWaterDepth()>0.0f)
					{
						CPed* pPed = static_cast<CPed*>(pEntityA);
						if (pPed->GetIsSwimming())
						{
							shouldProcessVfx = true;
						}
					}
				}
				else if (pInstB->GetClassType()==PH_INST_PED)
				{
					isPedCapsuleColn = true;
					if (pEntityB && CVfxHelper::GetGameCamWaterDepth()>0.0f)
					{
						CPed* pPed = static_cast<CPed*>(pEntityB);
						if (pPed->GetIsSwimming())
						{
							shouldProcessVfx = true;
						}
					}
				}
				// non ped capsule collision should all be processed
				else
				{
					shouldProcessVfx = true;
					shouldProcessAudio = true;
				}

				if( (pEntityA && pEntityA->GetIsTypeObject() && !pInstA->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE)) || (pEntityB && pEntityB->GetIsTypeObject() && !pInstB->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE)) )
				{
					shouldProcessAudio = true;
				}

				if(pEntityA && pEntityA->GetIsTypePed() && ((CPed*)pEntityA)->IsLocalPlayer() && pEntityB && (pEntityB->GetIsTypeObject() || pEntityB->GetIsTypePed() || pEntityB->GetIsTypeVehicle()))
				{
					g_CollisionAudioEntity.HandlePlayerEvent(pEntityB);
				}

				if(pEntityB && pEntityB->GetIsTypePed() && ((CPed*)pEntityB)->IsLocalPlayer() && pEntityA && (pEntityA->GetIsTypeObject() || pEntityA->GetIsTypePed() || pEntityA->GetIsTypeVehicle()))
				{
					g_CollisionAudioEntity.HandlePlayerEvent(pEntityA);
				}


				// the audio want to detect shards of glass colliding with the ground
				bool breakableCollisionA = (pInstB->GetClassType() == PH_INST_MAPCOL && pInstA->GetArchetype()->GetTypeFlags() == ArchetypeFlags::GTA_GLASS_TYPE);
				bool breakableCollisionB = (pInstA->GetClassType() == PH_INST_MAPCOL && pInstB->GetArchetype()->GetTypeFlags() == ArchetypeFlags::GTA_GLASS_TYPE);

				// check for not wanting to continue processing
				if (shouldProcessVfx || shouldProcessAudio)
				{
					// don't process any collisions with insts that don't have an entity (unless they are breakable glass)
					if ((pEntityA==NULL || pEntityB==NULL) && !breakableCollisionA && !breakableCollisionB)
					{
						shouldProcessVfx = false;
						shouldProcessAudio = false;
					}
					// don't process any collisions with non frag insts when there is a frag inst (unless this is a ped capsule collision)
					else if (isPedCapsuleColn==false && 
								((pEntityA && (pEntityA->m_nFlags.bIsFrag && !IsFragInst(pInstA))) || 
								(pEntityB && (pEntityB->m_nFlags.bIsFrag && !IsFragInst(pInstB)))))		
					{
						shouldProcessVfx = false;
						shouldProcessAudio = false;
					}
				}

				if (shouldProcessVfx || shouldProcessAudio)
				{
					// Third contact loop for audio/vfx only values. Only do this if necessary.
					Vec3V vAvgNewWorldPosA = Vec3V(V_ZERO);
					Vec3V vAvgNewWorldPosB = Vec3V(V_ZERO);
					Vec3V vAvgNewWorldNormal = Vec3V(V_ZERO);
					phMaterialMgr::Id firstNewMtlIdA = (phMaterialMgr::Id)0;
					phMaterialMgr::Id firstNewMtlIdB = (phMaterialMgr::Id)0;
					ScalarV numContactsNew = ScalarV(V_ZERO);
					bool anyNewContacts = false;
					for (int contactIndex=0; contactIndex<manifold->GetNumContacts(); contactIndex++)
					{
						phContact& contact = manifold->GetContactPoint(contactIndex);
						// accumulate the positions and normals across all new contacts
						if (contact.IsContactActive())
						{
							if (!anyNewContacts)
							{
								anyNewContacts = true;
								// set up common data
								firstNewMtlIdA = PGTAMATERIALMGR->UnpackMtlId(contact.GetMaterialIdA());
								firstNewMtlIdB = PGTAMATERIALMGR->UnpackMtlId(contact.GetMaterialIdB());
							}

							vAvgNewWorldPosA += contact.GetWorldPosA();
							vAvgNewWorldPosB += contact.GetWorldPosB();
							vAvgNewWorldNormal += contact.GetWorldNormal();

							numContactsNew = Add(numContactsNew,svOne);
						}
					}

					// only process if new contacts were found
					if (anyNewContacts)
					{
						// average out the positions and normals
						ScalarV invNumContactsNew = Invert(numContactsNew);
						vAvgNewWorldPosA *= invNumContactsNew;
						vAvgNewWorldPosB *= invNumContactsNew;
						vAvgNewWorldNormal = NormalizeSafe(vAvgNewWorldNormal, Vec3V(V_X_AXIS_WZERO));

						Vec3V vAvgNewRelVelocity = phContact::ComputeRelVelocity(vAvgNewWorldPosA,vAvgNewWorldPosB,manifold);

						// only process if the accumulated impulse, relative velocity and world normal are non zero
						// a valid matrix will not be able to be calculated if they aren't
						if (IsZeroAll(vAvgNewRelVelocity)==false)
						{
							VfxCollisionInfo_s vfxColnInfo;

							vfxColnInfo.pEntityA = pEntityA;
							vfxColnInfo.pEntityB = pEntityB;
							vfxColnInfo.componentIdA = manifold->GetComponentA();
							vfxColnInfo.componentIdB = manifold->GetComponentB();
							vfxColnInfo.pColliderA = manifold->GetColliderA();
							vfxColnInfo.pColliderB = manifold->GetColliderB();
							vfxColnInfo.vRelVelocity = vAvgNewRelVelocity;

							vfxColnInfo.vWorldPosA = vAvgNewWorldPosA;
							vfxColnInfo.vWorldPosB = vAvgNewWorldPosB;
							vfxColnInfo.vWorldNormalA = vAvgNewWorldNormal;
							vfxColnInfo.vWorldNormalB = -vAvgNewWorldNormal;
							vfxColnInfo.mtlIdA = firstNewMtlIdA;
							vfxColnInfo.mtlIdB = firstNewMtlIdB;
							vfxColnInfo.vAccumImpulse = vAccumImpulse;

#if __BANK
							if (g_vfxMaterial.GetColnVfxRecomputePositions())
							{
								vfxColnInfo.vWorldPosA = UnTransformOrtho(pInstA->GetMatrix(), vAvgNewWorldPosA);
								vfxColnInfo.vWorldPosA = VECTOR3_TO_VEC3V(vfxColnInfo.pEntityA->TransformIntoWorldSpace(RCC_VECTOR3(vfxColnInfo.vWorldPosA)));
								vfxColnInfo.vWorldPosB = UnTransformOrtho(pInstB->GetMatrix(), vAvgNewWorldPosB);
								vfxColnInfo.vWorldPosB = VECTOR3_TO_VEC3V(vfxColnInfo.pEntityB->TransformIntoWorldSpace(RCC_VECTOR3(vfxColnInfo.vWorldPosB)));
							}
#endif
							CPhysics::ProcessCollisionVfxandSfx(vfxColnInfo, shouldProcessVfx);

							g_CollisionAudioEntity.UpdateManifold(vfxColnInfo, *manifold, breakableCollisionA, breakableCollisionB);
						}
					}
				}
			}
		}
	}
}

bool ValidateAndRefreshRootManifold(phManifold* pManifold, phInst*& pInstA, phInst*& pInstB, CEntity*& pEntityA, CEntity*& pEntityB)
{
	const u32 levelIndexA = pManifold->GetLevelIndexA();
	const u32 levelIndexB = pManifold->GetLevelIndexB();
	if(levelIndexA != phInst::INVALID_INDEX && levelIndexB != phInst::INVALID_INDEX)
	{
		// Make sure neither instance has been removed from the level
		if(PHLEVEL->IsLevelIndexGenerationIDCurrent(levelIndexA,pManifold->GetGenerationIdA()) && PHLEVEL->IsLevelIndexGenerationIDCurrent(levelIndexB,pManifold->GetGenerationIdB()))
		{
			Assert(pManifold->GetInstanceA() && pManifold->GetInstanceB());
			Assert((pManifold->GetInstanceA() == PHLEVEL->GetInstance(levelIndexA)) && (pManifold->GetInstanceB() == PHLEVEL->GetInstance(levelIndexB)));

			pInstA = pManifold->GetInstanceA();
			pInstB = pManifold->GetInstanceB();
			pEntityA = CPhysics::GetEntityFromInst(pInstA);
			pEntityB = CPhysics::GetEntityFromInst(pInstB);

			// Make sure we aren't looking at stale collider pointers. We could skip over these stale manifolds but since the simulator won't skip them
			//   it might cause inconsistencies. 
			pManifold->RefreshColliderPointers();
			return true;
		}
	}
	return false;
}

// name:		IterateOverManifolds
// description:	Go over all the collisions in the physics, extracting information for game events.
void CPhysics::IterateOverManifolds()
{
#if USER_JBN
	static bool g_DisableIterateOverManifolds = false;
	if (g_DisableIterateOverManifolds)
		return;
#endif // USER_JBN

	phOverlappingPairArray::PairArray& pairArray = GetSimulator()->GetOverlappingPairArray()->pairs;
	int numPairs = pairArray.GetCount();

	// Go over all the pairs of objects overlapping in the broadphase
	for(int pairIndex = 0; pairIndex < numPairs; ++pairIndex)
	{
		// A pair will only have a manifold if the are actively colliding
		if (phManifold* manifold = pairArray[pairIndex].manifold)
		{
			if(!manifold->IsConstraint())
			{
				phInst* pInstA = NULL;
				phInst* pInstB = NULL;
				CEntity* pEntityA = NULL;
				CEntity* pEntityB = NULL;
				if (manifold->CompositeManifoldsEnabled())
				{
					bool isManifoldKnownToBeValid = false;
					for (int manifoldIndex = 0; manifoldIndex < manifold->GetNumCompositeManifolds(); ++manifoldIndex)
					{
						phManifold* compositeManifold = manifold->GetCompositeManifold(manifoldIndex);
						if(compositeManifold->GetNumContacts()>0)
						{
							if(!isManifoldKnownToBeValid)
							{
								// Try to wait as long as possible to check for a valid manifold. It's very expensive looking stuff up in the phLevel/phSimulator and a significant number of
								//   manifolds have 0 contacts. 
								if(ValidateAndRefreshRootManifold(manifold,pInstA,pInstB,pEntityA,pEntityB))
								{
									isManifoldKnownToBeValid = true;
								}
								else
								{
									break;
								}
							}
							FastAssert(pInstA && pInstB);
							IterateManifold(compositeManifold,pEntityA,pEntityB,pInstA,pInstB);
						}
					}
				}
				else
				{
					if(manifold->GetNumContacts()>0 && ValidateAndRefreshRootManifold(manifold,pInstA,pInstB,pEntityA,pEntityB))
					{
						FastAssert(pInstA && pInstB);
						IterateManifold(manifold,pEntityA,pEntityB,pInstA,pInstB);
					}
				}
			}
		}
	}
}

bool CPhysics::IsWheelComponent(const CEntity* pEntity, s32 componentId)
{
	if (pEntity->GetIsTypeVehicle())
	{
		const phArchetype* pArch = pEntity->GetPhysArch();
		if (pArch)
		{
			const phBound* pBound = pArch->GetBound();
			if (pBound)
			{
				if (pBound->GetType()==phBound::COMPOSITE)
				{
					const phBoundComposite* pBoundComp = static_cast<const phBoundComposite*>(pBound);
					Assert(pBoundComp->GetTypeAndIncludeFlags());
					if (pBoundComp->GetTypeFlags(componentId) & ArchetypeFlags::GTA_WHEEL_TEST)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool CPhysics::IsAttachedTo(const CEntity* pEntityA, const CEntity* pEntityB)
{
	if (pEntityA->GetIsPhysical())
	{
		const CPhysical* pPhysicalA = static_cast<const CPhysical*>(pEntityA);
		if (pPhysicalA->GetAttachParent()==pEntityB)
		{
			return true;
		}
	}

	return false;
}

bool CPhysics::ShouldProcessWheelCollisionVfx(const CEntity* pEntity, Vec3V_In vCollNormal)
{
#if __BANK
	// don't process if disabled in the widgets
	if (g_vfxMaterial.GetColnVfxDisableWheels())
	{
		return false;
	}
#endif

	// don't process if the collision normal is too close to the car's up vector
	if (Abs(Dot(pEntity->GetTransform().GetC(), vCollNormal).Getf())>VFXMATERIAL_WHEEL_COLN_DOT_THRESH)
	{
		return false;
	}

	return true;
}

//Only used to limit vfx 
bool CPhysics::ShouldProcessCollisionVfx(VfxCollisionInfo_s& vfxColnInfo)
{
	// ignore weapon collisions
	if (vfxColnInfo.pEntityA->GetIsTypeObject())
	{
		CObject* pObjectA = static_cast<CObject*>(vfxColnInfo.pEntityA);
		if (pObjectA->GetWeapon())
		{
			return false;
		}
	}

	if (vfxColnInfo.pEntityB->GetIsTypeObject())
	{
		CObject* pObjectB = static_cast<CObject*>(vfxColnInfo.pEntityB);
		if (pObjectB->GetWeapon())
		{
			return false;
		}
	}

	// early rejection threshold tests - if neither bangs nor scrapes would produce vfx we can skip 
	bool passedThresholdsBangs = false;
	VfxMaterialInfo_s* pFxMaterialInfo = g_vfxMaterial.GetBangInfo(PGTAMATERIALMGR->GetMtlVfxGroup(vfxColnInfo.mtlIdA), PGTAMATERIALMGR->GetMtlVfxGroup(vfxColnInfo.mtlIdB));
	if (pFxMaterialInfo)
	{
#if __BANK
		if (g_vfxMaterial.GetBangVfxAlwaysPassThresholds())
		{
			passedThresholdsBangs = true;
		}
		else
#endif
		{
			if (vfxColnInfo.pEntityA->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(vfxColnInfo.pEntityA);
				if (pVehicle->InheritsFromAutomobile())
				{
					CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);
					if (pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising || pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding)
					{
						passedThresholdsBangs = true;
					}
				}
			}
			else if (vfxColnInfo.pEntityB->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(vfxColnInfo.pEntityB);
				if (pVehicle->InheritsFromAutomobile())
				{
					CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);
					if (pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising || pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding)
					{
						passedThresholdsBangs = true;
					}
				}
			}

			if (!passedThresholdsBangs)
			{
				passedThresholdsBangs = vfxColnInfo.bangMagA>=pFxMaterialInfo->velThreshMin && vfxColnInfo.vAccumImpulse.Getf()>=pFxMaterialInfo->impThreshMin;
			}
		}
	}

	bool passedThresholdsScrapes = false;
	pFxMaterialInfo = g_vfxMaterial.GetScrapeInfo(PGTAMATERIALMGR->GetMtlVfxGroup(vfxColnInfo.mtlIdA), PGTAMATERIALMGR->GetMtlVfxGroup(vfxColnInfo.mtlIdB));
	if (pFxMaterialInfo)
	{
#if __BANK
		if (g_vfxMaterial.GetScrapeVfxAlwaysPassThresholds())
		{
			passedThresholdsScrapes = true;
		}
		else
#endif
		{
			passedThresholdsScrapes = vfxColnInfo.scrapeMagA>=pFxMaterialInfo->velThreshMin && vfxColnInfo.vAccumImpulse.Getf()>=pFxMaterialInfo->impThreshMin;
		}
	}

	if (!passedThresholdsBangs && !passedThresholdsScrapes)
	{
		return false;
	}

	return true;
}

//Limits both vfx and sfx
bool CPhysics::ShouldProcessCollisionVfxAndSfx(VfxCollisionInfo_s& vfxColnInfo)
{
	// ignore entities attached to each other
	if (IsAttachedTo(vfxColnInfo.pEntityA, vfxColnInfo.pEntityB) ||
		IsAttachedTo(vfxColnInfo.pEntityB, vfxColnInfo.pEntityA))
	{
		return false;
	}

	// ignore wheel collisions in certain cases
	if (IsWheelComponent(vfxColnInfo.pEntityA, vfxColnInfo.componentIdA))
	{
		return ShouldProcessWheelCollisionVfx(vfxColnInfo.pEntityA, vfxColnInfo.vWorldNormalA);
	}
	else if (IsWheelComponent(vfxColnInfo.pEntityB, vfxColnInfo.componentIdB))
	{
		return ShouldProcessWheelCollisionVfx(vfxColnInfo.pEntityB, vfxColnInfo.vWorldNormalB);
	}

 	// ignore collision with certain materials
	if (vfxColnInfo.mtlIdA==PGTAMATERIALMGR->g_idPhysVehicleSpeedUp || 
		vfxColnInfo.mtlIdA==PGTAMATERIALMGR->g_idPhysVehicleSlowDown ||
		vfxColnInfo.mtlIdA==PGTAMATERIALMGR->g_idPhysVehicleRefill ||
		vfxColnInfo.mtlIdA==PGTAMATERIALMGR->g_idPhysVehicleBoostCancel ||
		vfxColnInfo.mtlIdA==PGTAMATERIALMGR->g_idPhysPropPlacement 
		)
	{
		return false;
	}

	if (vfxColnInfo.mtlIdB==PGTAMATERIALMGR->g_idPhysVehicleSpeedUp || 
		vfxColnInfo.mtlIdB==PGTAMATERIALMGR->g_idPhysVehicleSlowDown ||
		vfxColnInfo.mtlIdB == PGTAMATERIALMGR->g_idPhysVehicleRefill ||
		vfxColnInfo.mtlIdA == PGTAMATERIALMGR->g_idPhysVehicleBoostCancel ||
		vfxColnInfo.mtlIdA==PGTAMATERIALMGR->g_idPhysPropPlacement 
		)
	{
		return false;
	}

	// none of the above - process this collision
	return true;
}

void CPhysics::ProcessCollisionVfxandSfx(VfxCollisionInfo_s& vfxColnInfo, bool shouldProcessVfx)
{
	// return if either entity isn't valid
	if (vfxColnInfo.pEntityA==NULL || vfxColnInfo.pEntityB==NULL)
	{
		return;
	}

	// check that entity a is a physical object (b may or may not be)
	Assertf(vfxColnInfo.pEntityA->GetIsPhysical() || vfxColnInfo.pEntityB->GetIsPhysical(), "Neither entity is physical (A:%s - B:%s)", vfxColnInfo.pEntityA->GetModelName(), vfxColnInfo.pEntityB->GetModelName());

	// check if we should continue with this collision
	if (!ShouldProcessCollisionVfxAndSfx(vfxColnInfo))
	{
		return;
	}

	// return if we have two invalid colliders
	if (vfxColnInfo.pColliderA==NULL && vfxColnInfo.pColliderB==NULL)
	{
		return;
	}

	// calc the collision velocity (the impulse is mass x velocity so divide by the mass to get a velocity)
	vfxColnInfo.vColnVel = -vfxColnInfo.vWorldNormalA * vfxColnInfo.vAccumImpulse;
	if (vfxColnInfo.pColliderA)
	{
		vfxColnInfo.vColnVel = Scale(vfxColnInfo.vColnVel, ScalarVFromF32(vfxColnInfo.pColliderA->GetInvMass()));
		vfxColnInfo.vColnVel += vfxColnInfo.vRelVelocity;
	}
	else
	{
		vfxColnInfo.vColnVel = Scale(vfxColnInfo.vColnVel, ScalarVFromF32(vfxColnInfo.pColliderB->GetInvMass()));
		vfxColnInfo.vColnVel -= vfxColnInfo.vRelVelocity;
	}

	Assertf(FPIsFinite(vfxColnInfo.vColnVel.GetXf() && vfxColnInfo.vColnVel.GetYf() && vfxColnInfo.vColnVel.GetZf()), "collision velocity is not valid");

	// return if the calculated collision velocity is close to zero (we won't be able to create a valid matrix if it is)
	const ScalarV vEpsilon = ScalarV(V_FLT_SMALL_4);
	if (IsLessThanAll(Mag(vfxColnInfo.vColnVel), vEpsilon))
	{
		return;
	}

	// calc the bang and scrape magnitudes
	ScalarV dotNV = Dot(vfxColnInfo.vWorldNormalA, vfxColnInfo.vColnVel);
	Vec3V vColnVelInN = vfxColnInfo.vWorldNormalA * dotNV;
	Vec3V vColnVelPerpToN = vfxColnInfo.vColnVel-vColnVelInN;
	vfxColnInfo.bangMagA = Mag(vColnVelInN).Getf();
	vfxColnInfo.scrapeMagA = Mag(vColnVelPerpToN).Getf();

	dotNV = Dot(vfxColnInfo.vWorldNormalB, vfxColnInfo.vColnVel);
	vColnVelInN = vfxColnInfo.vWorldNormalB * dotNV;
	vColnVelPerpToN = vfxColnInfo.vColnVel-vColnVelInN;
	vfxColnInfo.bangMagB = Mag(vColnVelInN).Getf();
	vfxColnInfo.scrapeMagB = Mag(vColnVelPerpToN).Getf();

#if __BANK
	if (CPhysics::ms_bDoCollisionEffects)
#endif
	{
		// get the collision direction
		vfxColnInfo.vColnDir = Vec3V(V_ZERO);

		if (vfxColnInfo.pEntityA->GetIsPhysical())
		{
			const CPhysical* pCollPhysical = static_cast<const CPhysical*>(vfxColnInfo.pEntityA);
			vfxColnInfo.vColnDir = VECTOR3_TO_VEC3V(pCollPhysical->GetLocalSpeed(RCC_VECTOR3(vfxColnInfo.vWorldPosA), true, vfxColnInfo.componentIdA) * fwTimer::GetTimeStep());
		}
		else
		{
			Assertf(vfxColnInfo.pEntityB->GetIsPhysical(), "Neither collision entity is physical");
			const CPhysical* pCollPhysical = static_cast<const CPhysical*>(vfxColnInfo.pEntityB);
			vfxColnInfo.vColnDir = VECTOR3_TO_VEC3V(pCollPhysical->GetLocalSpeed(RCC_VECTOR3(vfxColnInfo.vWorldPosB), true, vfxColnInfo.componentIdB) * fwTimer::GetTimeStep());
		}

		// process vfx for the entity A
#if __BANK
		if (!g_vfxMaterial.GetColnVfxDisableEntityA())
#endif
		{
			if (vfxColnInfo.pEntityA->GetIsTypePed())
			{
				PuddlePassSingleton::InstanceRef().AddPlayerRipple(vfxColnInfo.vWorldPosA, Mag(vfxColnInfo.vColnVel).Getf());
			}

			if (shouldProcessVfx && ShouldProcessCollisionVfx(vfxColnInfo))
			{
				vfxColnInfo.pEntityA->ProcessCollisionVfx(vfxColnInfo);
			}
		}

		// process vfx for the entity B
#if __BANK
		if (!g_vfxMaterial.GetColnVfxDisableEntityB())
#endif
		{		
			if (vfxColnInfo.pEntityB->GetIsTypePed())
			{
				PuddlePassSingleton::InstanceRef().AddPlayerRipple(vfxColnInfo.vWorldPosA, Mag(vfxColnInfo.vColnVel).Getf());
			}

			if (shouldProcessVfx)
			{
				float bangMagTemp = vfxColnInfo.bangMagA;
				vfxColnInfo.bangMagA = vfxColnInfo.bangMagB;
				vfxColnInfo.bangMagB = bangMagTemp;

				float scrapeMagTemp = vfxColnInfo.scrapeMagA;
				vfxColnInfo.scrapeMagA = vfxColnInfo.scrapeMagB;
				vfxColnInfo.scrapeMagB = scrapeMagTemp;

				if (ShouldProcessCollisionVfx(vfxColnInfo))
				{
					CEntity* pEntityTemp = vfxColnInfo.pEntityA;
					vfxColnInfo.pEntityA = vfxColnInfo.pEntityB;
					vfxColnInfo.pEntityB = pEntityTemp;

					int componentIdTemp = vfxColnInfo.componentIdA;
					vfxColnInfo.componentIdA = vfxColnInfo.componentIdB;
					vfxColnInfo.componentIdB = componentIdTemp;

					phCollider* pColliderTemp = vfxColnInfo.pColliderA;
					vfxColnInfo.pColliderA = vfxColnInfo.pColliderB;
					vfxColnInfo.pColliderB = pColliderTemp;

					Vec3V vWorldPosTemp = vfxColnInfo.vWorldPosA;
					vfxColnInfo.vWorldPosA = vfxColnInfo.vWorldPosB;
					vfxColnInfo.vWorldPosB = vWorldPosTemp;

					Vec3V vWorldNormalTemp = vfxColnInfo.vWorldNormalA;
					vfxColnInfo.vWorldNormalA = vfxColnInfo.vWorldNormalB;
					vfxColnInfo.vWorldNormalB = vWorldNormalTemp;

					phMaterialMgr::Id mtlTemp = vfxColnInfo.mtlIdA;
					vfxColnInfo.mtlIdA = vfxColnInfo.mtlIdB;
					vfxColnInfo.mtlIdB = mtlTemp;

					vfxColnInfo.pEntityA->ProcessCollisionVfx(vfxColnInfo);
				}
			}
		}

#if __BANK
		// debug stuff
		if (g_vfxMaterial.GetColnVfxRenderDebugVectors())
 		{
 			Color32 colYellow(1.0f, 1.0f, 0.0f, 1.0f);
 			grcDebugDraw::Line(RCC_VECTOR3(vfxColnInfo.vWorldPosA), VEC3V_TO_VECTOR3(vfxColnInfo.vWorldPosA+vfxColnInfo.vColnVel), colYellow, -1);
 		}

		if (g_vfxMaterial.GetColnVfxRenderDebugImpulses())
		{
			static float debugImpulseMax = 200.0f;

			// show zero impulses (red), low-high impulses (yellow->green)
			Color32 col;
			if (vfxColnInfo.vAccumImpulse.Getf()==0.0f)
			{
				col = Color32(1.0f, 0.0f, 0.0f, 1.0f);
			}
			else 
			{
				float t = Min(1.0f, vfxColnInfo.vAccumImpulse.Getf()/debugImpulseMax);
				col = Color32(1.0f-t, 1.0f, 0.0f, 1.0f);
			}

			if (g_vfxMaterial.GetColnVfxDisableEntityA()==false)
			{
				grcDebugDraw::Cross(vfxColnInfo.vWorldPosA, 0.1f, col, -1);
			}

			if (g_vfxMaterial.GetColnVfxDisableEntityB()==false)
			{
				grcDebugDraw::Cross(vfxColnInfo.vWorldPosB, 0.1f, col, -1);
			}
		}

		if (g_vfxMaterial.GetColnVfxOutputDebug())
		{
			char mtlIdAName[128];
			char mtlIdBName[128];
			PGTAMATERIALMGR->GetMaterialName(vfxColnInfo.mtlIdA, mtlIdAName, 128);
			PGTAMATERIALMGR->GetMaterialName(vfxColnInfo.mtlIdB, mtlIdBName, 128);

			vfxDebugf1("Vfx Collision Info (%d)", fwTimer::GetFrameCount());
			vfxDebugf1("  %s v %s", mtlIdAName, mtlIdBName);
			vfxDebugf1("  Accum Impulse %.3f", vfxColnInfo.vAccumImpulse.Getf());
			vfxDebugf1("  Rel Speed %.3f", Mag(vfxColnInfo.vRelVelocity).Getf());
		}
#endif
	}
}

void CPhysics::ProcessCollisionData(phContact& contact, phManifold& manifold, CEntity* pEntityA, CEntity* pEntityB, const ScalarV& vAvgImpulseMag)
{
	// extract info from the contact
	const Vec3V vCollPosA = contact.GetWorldPosA();
	const Vec3V vCollPosB = contact.GetWorldPosB();

	const int collComponentA = manifold.GetComponentA();
	const int collComponentB = manifold.GetComponentB();

	// Calculate the impulse vectors.
	physicsAssert(IsGreaterThanOrEqualAll(vAvgImpulseMag, ScalarV(V_ZERO)));
	float fAvgImpulseMag = vAvgImpulseMag.Getf();

#if __ASSERT
	Assertf(IsFiniteAll(contact.GetWorldNormal()*vAvgImpulseMag),"%p: Invalid impact in phInstGta::PreApplyImpacts",&contact);
	// catch this here, early, instead of in the vfx code
	float normalMag = Mag(contact.GetWorldNormal()).Getf();
	if(normalMag < 0.97f || normalMag > 1.03f)
	{
		manifold.AssertContactNormalVerbose(contact, __FILE__, __LINE__);
	}
#endif

	//Tell the CPhysicals about all impulses applied (for damage, deformation and breaking and collision recording)
	if(pEntityA != pEntityB)
	{
		// Cache the info from contact, as it can be deleted from ProcessCollision
		const Vec3V vContactWorldNormal = contact.GetWorldNormal();
		const phMaterialMgr::Id iContactMaterialIdA = contact.GetMaterialIdA();
		const phMaterialMgr::Id iContactMaterialIdB = contact.GetMaterialIdB();
		const bool bContactIsPositiveDepth = contact.IsPositiveDepth();
		const bool bIsNewContact = contact.GetLifetime() == 1;

		if(pEntityA && pEntityA->GetIsPhysical())
		{
			const Vec3V vCollNormalA = vContactWorldNormal;
			static_cast<CPhysical*>(pEntityA)->ProcessCollision(manifold.GetInstanceA(), pEntityB, manifold.GetInstanceB(),
				RCC_VECTOR3(vCollPosA), RCC_VECTOR3(vCollPosB), fAvgImpulseMag, RCC_VECTOR3(vCollNormalA), collComponentA, collComponentB,
				iContactMaterialIdB, bContactIsPositiveDepth, bIsNewContact);
		}

		// For B*299855, since we could have deleted an object in the ProcessCollision call above.
		// Note that this doesn't check if the object has been deleted, which might be a bad thing...
		if(pEntityB && pEntityB->GetIsPhysical() && (!manifold.GetInstanceA() || manifold.GetInstanceA()->IsInLevel()))
		{
			const Vec3V vCollNormalB = Negate(vContactWorldNormal);
			static_cast<CPhysical*>(pEntityB)->ProcessCollision(manifold.GetInstanceB(), pEntityA, manifold.GetInstanceA(),
				RCC_VECTOR3(vCollPosB), RCC_VECTOR3(vCollPosA), fAvgImpulseMag, RCC_VECTOR3(vCollNormalB), collComponentB, collComponentA,
				iContactMaterialIdA, bContactIsPositiveDepth, bIsNewContact);
		}
	}
}

// Verify we don't intend these types to collide with MAP_TYPE_WEAPON
CompileTimeAssert(((ArchetypeFlags::GTA_PED_INCLUDE_TYPES | ArchetypeFlags::GTA_PARACHUTING_PED_INCLUDE_TYPES) & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0);
CompileTimeAssert((ArchetypeFlags::GTA_RAGDOLL_INCLUDE_TYPES & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0);
CompileTimeAssert((ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0);
CompileTimeAssert((ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0);
CompileTimeAssert((ArchetypeFlags::GTA_BOX_VEHICLE_INCLUDE_TYPES & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0);
//CompileTimeAssert(((ArchetypeFlags::GTA_PROJECTILE_INCLUDE_TYPES | ArchetypeFlags::GTA_PROJECTILE_NEAR_INCLUDE_TYPES) & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0);

bool TypeFlagsNeverCollideHelper(u32 typeFlagsA, u32 typeFlagsB)
{
	// If one instance is pure weapon collision then we know many things will never get past type/include flag checks.
	if(typeFlagsA == ArchetypeFlags::GTA_MAP_TYPE_WEAPON)
	{
		const u32 typesThatNeverCollideWithMapTypeWeapon =	ArchetypeFlags::GTA_PED_TYPE | 
															ArchetypeFlags::GTA_RAGDOLL_TYPE | 
															ArchetypeFlags::GTA_HORSE_TYPE |
															ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE |
															ArchetypeFlags::GTA_VEHICLE_TYPE |
															ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | 
															ArchetypeFlags::GTA_OBJECT_TYPE;
															//ArchetypeFlags::GTA_PROJECTILE_TYPE projectiles collide with weapon collision when near the player, see GTA_PROJECTILE_NEAR_INCLUDE_TYPES
		if((typeFlagsB & typesThatNeverCollideWithMapTypeWeapon) != 0)
		{
			return true;
		}
	}
	return false;
}

// NOTES:
//   This function should get called whenever 2 objects start overlapping in the broadphase. Dynamic conditions should probably be avoided
//     since you will be at the mercy of the broadphase if you want to re-add the broadphase pair. 
bool CPhysics::KeepBroadphasePair(int levelIndexA, int levelIndexB)
{
	// Keep self collisions
	if(levelIndexA != levelIndexB)
	{
		phInst* pInstA = PHLEVEL->GetInstance(levelIndexA);
		phInst* pInstB = PHLEVEL->GetInstance(levelIndexB);

		// Get rid of collisions between multiple instances on a CEntity (ped capsules and ragdolls)
		if(pInstB->GetUserData() != NULL && pInstA->GetUserData() == pInstB->GetUserData())
		{
			return false;
		}

		// Certain types never collide
		u32 typeFlagsA = PHLEVEL->GetInstanceTypeFlags(levelIndexA);
		u32 typeFlagsB = PHLEVEL->GetInstanceTypeFlags(levelIndexB);
		if(TypeFlagsNeverCollideHelper(typeFlagsA,typeFlagsB) || TypeFlagsNeverCollideHelper(typeFlagsB,typeFlagsA))
		{
			return false;
		}
	}

	return true;
}

void CPhysics::ClearManifoldsForInst(phInst* pInst)
{
	btBroadphasePair* prunedPairs = PHLEVEL->GetBroadPhase()->getPrunedPairs();
	const int prunedPairCount = PHLEVEL->GetBroadPhase()->getPrunedPairCount();

	// Go over all the pairs of objects overlapping in the broadphase
	for( int pairIndex = 0; pairIndex < prunedPairCount; ++pairIndex )
	{
		btBroadphasePair *pair = prunedPairs + pairIndex;

		// A pair will only have a manifold if the are actively colliding
		if (phManifold* manifold = pair->GetManifold())
		{
#if 0
			if (manifold->CompositeManifoldsEnabled())
			{
				for (int manifoldIndex = 0; manifoldIndex < manifold->GetNumCompositeManifolds(); ++manifoldIndex)
				{
					phManifold* manifoldInComposite = manifold->GetCompositeManifold(manifoldIndex);

					if(manifoldInComposite->GetInstanceA()==pInst || manifoldInComposite->GetInstanceB()==pInst)
						manifoldInComposite->Reset();
				}
			}
			else
			{
				if(manifold->GetInstanceA()==pInst || manifold->GetInstanceB()==pInst)
					manifold->Reset();
			}
#else
			if(manifold->GetInstanceA()==pInst || manifold->GetInstanceB()==pInst)
			{
				manifold->RemoveAllContacts();
			}
#endif
		}

	}
}

void CPhysics::WarpInstance(phInst* pInst, Vec3V_In pos)
{
	Mat34V mat = pInst->GetMatrix();
	mat.SetCol3(pos);
	PHSIM->TeleportObject(*pInst,RCC_MATRIX34(mat));
}

//
// name:		CPhysics::StreamData
// description:	streams all the physics data required
void CPhysics::UpdateStreaming()
{
	// In multi-player mode we only want to load the MOVER map collision to save on memory. The
	// MOVER collision will be flagged to collide with weapon shapetests in AddBoundToPhysicsLevel() for
	// the static bounds store.
	if(PARAM_mapMoverOnlyInMultiplayer.Get())
		g_StaticBoundsStore.SetWeaponBounds(!NetworkInterface::IsGameInProgress());

	Vector3 playerPosn = CFocusEntityMgr::GetMgr().GetPos();
	LoadAboutPosition(RCC_VEC3V(playerPosn));
}

// query streaming etc to see if it is a good time to start increasing the range at which
// we load mover static collisions. this is subject to warm up / cool off timers so
// most discontinuities are smoothed out.
bool CPhysics::SafeToIncreaseStreamingRangeForMoverCollision()
{
	const float fMaxVelocity = 8.0f;
	const s32 maxPrioRequests = 0;
	const s32 maxNormalRequests = 5;		// because static bounds store will be generating new requests

	const bool bLoadingScene			= g_LoadScene.IsActive();
	const bool bSwitching				= g_PlayerSwitch.IsActive();
	const bool bCutsceneSrlStreaming	= gStreamingRequestList.IsActive();
	const bool bPlayerOnFoot			= g_ContinuityMgr.IsPlayerOnFoot();
	const bool bPlayerHasLowVelocity	= g_ContinuityMgr.GetPlayerVelocity() <= fMaxVelocity;
	const bool bLowStreamingPressure	= ( strStreamingEngine::GetInfo().GetNumberPriorityObjectsRequested()<=maxPrioRequests
											&& strStreamingEngine::GetInfo().GetNumberObjectsRequested()<=maxNormalRequests );
	const bool bFocusIsDefault			= CFocusEntityMgr::GetMgr().IsPlayerPed();

	return (bLowStreamingPressure && !bLoadingScene && !bSwitching && !bCutsceneSrlStreaming
		&& bPlayerOnFoot && bPlayerHasLowVelocity && bFocusIsDefault);
}

//
// name:		CPhysics::LoadAboutPosition
// description:	Load physics bounds about a position
void CPhysics::LoadAboutPosition(Vec3V_In posn)
{
	// request static bounds
	g_StaticBoundsStore.UpdateStreamingRanges( SafeToIncreaseStreamingRangeForMoverCollision() );
	g_StaticBoundsStore.GetBoxStreamer().RequestAboutPos(posn);

	const bool bCutsceneAllowPauseForStreaming = CutSceneManager::GetInstance() ? CutSceneManager::GetInstance()->GetAllowGameToPauseForStreaming() : true;
	const bool bPlayerSwitchAllowPauseForStreaming = !( g_PlayerSwitch.GetLongRangeMgr().IsActive() && !CFocusEntityMgr::GetMgr().IsPlayerPed() );

	// ensure in memory
	if (CTheScripts::GetAllowGameToPauseForStreaming() && bCutsceneAllowPauseForStreaming && bPlayerSwitchAllowPauseForStreaming REPLAY_ONLY(&& !CReplayMgr::IsReplayInControlOfWorld()))
	{
		g_StaticBoundsStore.GetBoxStreamer().EnsureLoadedAboutPos(posn);
	}
#if DEBUG_DRAW
	else
	{
		grcDebugDraw::AddDebugOutput("!!! Collision streaming: EnsureLoadedAboutPos is disabled !!!");
	}
#endif	//DEBUG_DRAW
}

//
// name:		CPhysics::ScanAreaPtrArray
// description:	Scan the ptrlist for entities that need bounds loaded
void CPhysics::ScanAreaPtrArray(void appendFunc (atArray<CEntity*>&) )
{
	CEntity *entityArrayContents[MAX_ACTIVE_INTERIOR_PHYSICS];
	atUserArray<CEntity*> entityArray(entityArrayContents,MAX_ACTIVE_INTERIOR_PHYSICS);

	appendFunc(entityArray);

	u32 count = entityArray.GetCount();
	if (count == 0){
		return;
	}

	for(u32 i=0; i<count; i++)
	{
		CEntity* pEntity = entityArray[i];
		if (pEntity)
		{
			// If entity has a physics dictionary and hasnt had its physics instance setup
			if(pEntity->GetCurrentPhysicsInst() == NULL && 
				(pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeMLO()) &&
				pEntity->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT) && !pEntity->IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS))
			{
				{
					pEntity->InitPhys();
					if(pEntity->GetCurrentPhysicsInst())
					{
						pEntity->AddPhysics();
					}
				}
			}
		}
	}
}

float afGravityLevels[4] =
{
	9.8f,
	2.4f,
	0.1f,
	0.0f
};
CompileTimeAssert(CPhysics::NUM_GRAVITY_LEVELS == 4);

void CPhysics::SetGravityLevel(eGravityLevel nLevel)
{
	if(physicsVerifyf(nLevel >= 0 && nLevel < NUM_GRAVITY_LEVELS,"Invalid gravity level"))
	{
		SetGravitationalAcceleration(afGravityLevels[nLevel]);
	}
}

CPhysics::eGravityLevel CPhysics::GetGravityLevel()
{
	float fGravity = GetGravitationalAcceleration();
	for(s32 i = 0; i < NUM_GRAVITY_LEVELS; i++)
	{
		static dev_float EPSILON = 0.01f;
		if(rage::IsNearZero(fGravity - afGravityLevels[i], EPSILON))
		{
			return static_cast<eGravityLevel>(i);
		}
	}

	return GRAV_EARTH;
}

void CPhysics::SetGravitationalAcceleration(const float fGravitationalAcceleration)
{
	Assert(fGravitationalAcceleration>=0.0f);
	ms_fGravitationalAccleration=fGravitationalAcceleration;

	//Set game gravity.
	CVehicle::SetGravitationalAcceleration(ms_fGravitationalAccleration);

	//Set rage gravity.
	SetRageGravityVector();
	SetRageSleepTolerances();
}

void CPhysics::SetRageGravityVector()
{
	Assert(ms_fGravitationalAccleration>=0.0f);

	Vector3 vecSetGravity(0,0,-ms_fGravitationalAccleration);
	GetSimulator()->SetGravity(vecSetGravity);
}


float CPhysics::GetFoliageRadius( phInst* pOtherInstance, int component, int element )
{
	float fFoliageBoundRadius = 0.0f;
	phBound* pBound = pOtherInstance->GetArchetype()->GetBound();
	int nBoundType = pBound->GetType();
	if(nBoundType==phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pBound);
		phBound* pFoliageBound = pBoundComposite->GetBound(component);
		artAssertf(pFoliageBound->GetType()==phBound::SPHERE||pFoliageBound->GetType()==phBound::CAPSULE||pFoliageBound->GetType()==phBound::BVH,
			"Foliage bounds can only be sphere / capsule. Object:'%s' at (%5.2f, %5.2f, %5.2f).", pOtherInstance->GetArchetype()->GetFilename(),
			pOtherInstance->GetPosition().GetXf(), pOtherInstance->GetPosition().GetYf(), pOtherInstance->GetPosition().GetZf());
		// There may be multiple foliage bounds within our composite in which case they will be stored in a BVH structure or we may
		// have a single capsule or sphere foliage bound. Let's extract the bound and let the code below handle it.
		pBound = pFoliageBound;
		nBoundType = pBound->GetType();
	}

	if(nBoundType==phBound::BVH)
	{
		phBoundPolyhedron* pBvhBound = static_cast<phBoundPolyhedron*>(pBound);
		phPrimitive bvhPrimitive = pBvhBound->GetPolygon(element).GetPrimitive();
		artAssertf(bvhPrimitive.GetType()==PRIM_TYPE_SPHERE||bvhPrimitive.GetType()==PRIM_TYPE_CAPSULE,
			"Foliage bounds can only be sphere / capsule. Object:'%s' at (%5.2f, %5.2f, %5.2f).", pOtherInstance->GetArchetype()->GetFilename(),
			pBvhBound->GetCentroidOffset().GetXf(), pBvhBound->GetCentroidOffset().GetYf(), pBvhBound->GetCentroidOffset().GetZf());
		if(bvhPrimitive.GetType()==PRIM_TYPE_CAPSULE)
		{
			fFoliageBoundRadius = bvhPrimitive.GetCapsule().GetRadius();
		}
		else if(bvhPrimitive.GetType()==PRIM_TYPE_SPHERE)
		{
			fFoliageBoundRadius = bvhPrimitive.GetSphere().GetRadius();
		}
	}
	else if(nBoundType==phBound::SPHERE || nBoundType==phBound::CAPSULE)
	{
		fFoliageBoundRadius = pBound->GetRadiusAroundCentroid();
	}
	else
	{
		Assertf(false, "Unexpected bound type (%d) processing foliage collision at (%5.2f, %5.2f, %5.2f).", nBoundType,
			pOtherInstance->GetPosition().GetXf(), pOtherInstance->GetPosition().GetYf(), pOtherInstance->GetPosition().GetZf());
	}
	return fFoliageBoundRadius;
}

void CPhysics::SetRageSleepTolerances()
{
	const float dt=FRAME_LIMITER_MIN_FRAME_TIME/(1.0f*CPhysics::ms_NumTimeSlices);

	//There are some magic numbers in the following code (0.005, 0.01, and 0.2).
	//These numbers are copied from rage and used in a way that guarantees 
	//that the threshold values match the default rage sleep thresholds for gravity=9.81.

	{
	const float fAlphaVel0=Sqrtf(0.005f)/(-GRAVITY*dt);
	const float fAlphaVel=fAlphaVel0*ms_fGravitationalAccleration*dt;
	phSleep::SetDefaultVelTolerance2(fAlphaVel*fAlphaVel);
	}

	{
	const float fAlphaOmega0=Sqrtf(0.01f)/(-GRAVITY*dt);
	const float fAlphaOmega=fAlphaOmega0*ms_fGravitationalAccleration*dt;
	phSleep::SetDefaultAngVelTolerance2(fAlphaOmega*fAlphaOmega);
	}

	{
	const float fAlphaInternalVel0=Sqrtf(0.2f)/(-GRAVITY*dt);
	const float fAlphaInternalVel=fAlphaInternalVel0*ms_fGravitationalAccleration*dt;
	phSleep::SetDefaultInternalVelTolerance2(fAlphaInternalVel*fAlphaInternalVel);
	}
}

static dev_float sfGlassCollisionStrength = 300.0f;

#if __DEV
static dev_bool sbRenderHits = false;
#endif

void CPhysics::CollideInstAgainstGlass(phInst* pInst)
{	
	PF_PUSH_TIMEBAR_DETAIL("Glass test");
	Assert(pInst);
	if (!pInst)
		return;
	PF_START_TIMEBAR_DETAIL("Glass test init");

	// Check ragdoll for collision against glass
	phShapeTest<phShapeObject> shapeTest;

	u32 nIncludeFlags = ArchetypeFlags::GTA_OBJECT_TYPE;
	u32 nTypeFlag = ms_PhysLevel ? ms_PhysLevel->GetInstanceTypeFlags(pInst->GetLevelIndex()) : 0;

	const int iNumIntersections = 16;
	phIntersection aIntersections[iNumIntersections];
	int iNumHits = 0;

	Assert(pInst->GetArchetype());
	if (!pInst->GetArchetype())
		return;

	shapeTest.InitObject(*pInst->GetArchetype()->GetBound(), MAT34V_TO_MATRIX34(pInst->GetMatrix()), aIntersections, iNumIntersections);

	PF_START_TIMEBAR_DETAIL("Glass TestInLevel");
	iNumHits = shapeTest.TestInLevel(NULL, nIncludeFlags, nTypeFlag );	

	// Search for glass
	PF_START_TIMEBAR_DETAIL("Glass Post TestInLevel");
	for(int i = 0; i < iNumHits; i++)
	{
		phInst* pHitInst = aIntersections[i].GetInstance();
		if (pHitInst)
		{
			int iComponent = aIntersections[i].GetComponent();
			Vector3 vForce = -RCC_VECTOR3(aIntersections[i].GetNormal());
			vForce.Scale(sfGlassCollisionStrength);

			if(pHitInst->GetClassType() >= PH_INST_FRAG_GTA)
			{
				fragInst* pFragInst = static_cast<fragInst*>(pHitInst);
				if (pFragInst && pFragInst->GetTypePhysics())
				{
					Assert(iComponent < pFragInst->GetTypePhysics()->GetNumChildren());
					if (iComponent < pFragInst->GetTypePhysics()->GetNumChildren())
					{
						int iGroup = pFragInst->GetTypePhysics()->GetAllChildren()[iComponent]->GetOwnerGroupPointerIndex();
						fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetAllGroups()[iGroup];
						Assert(pGroup);

						if(pGroup && pGroup->GetMadeOfGlass())
						{
							// We've found some unbroken glass
							// Apply breaking force to the frag which will get passed down to the glass

							// Really want to anim drive this but if we test the whole composite bound at once we can't tell what our component is
							// So just base it off the normal

							pFragInst->ApplyImpulse(vForce, RCC_VECTOR3(aIntersections[i].GetPosition()), aIntersections[i].GetComponent(), aIntersections[i].GetPartIndex(), NULL, 1.0f);
						}
					}
				}
			}
			else if(pHitInst->GetClassType() == phInst::PH_INST_GLASS)
			{
				// This is an already fractured glass pane.
				// Want to fracture it more to break out any additional pieces
				pHitInst->ApplyImpulse(vForce, RCC_VECTOR3(aIntersections[i].GetPosition()), aIntersections[i].GetComponent(), aIntersections[i].GetPartIndex(), NULL, 1.0f);
			}
		}

#if __DEV
		PF_START_TIMEBAR_DETAIL("Glass hit debug render");
		if(sbRenderHits)
		{
			CPhysics::ms_debugDrawStore.AddSphere(aIntersections[i].GetPosition(),0.05f, Color_red, fwTimer::GetTimeStepInMilliseconds() *2);
		}
#endif
	}
	PF_POP_TIMEBAR_DETAIL();
}

void CPhysics::SetClearManifoldsEveryFrame(bool bClear)
{
	ms_bClearManifoldsEveryFrame = bClear;
}

void CPhysics::SetClearManifoldsFramesLeft(int framesToClear)
{
	Assertf(framesToClear < 1800, "CleatManifolds being set on for %d frames - Really need it left on that long?",framesToClear);
	ms_ClearManifoldsFramesLeft = framesToClear > ms_ClearManifoldsFramesLeft ? framesToClear : ms_ClearManifoldsFramesLeft;
}

bool CPhysics::CanUseRagdolls()
{
	// no ragdolls in a network game for now
	if(NetworkInterface::IsGameInProgress() && !NetworkInterface::AllowNaturalMotion())
		return false;

#if GTA_REPLAY
	//Dont allow ragdolls when replay is in control of the world - all the bones should be recorded.
	if(CReplayMgr::IsReplayInControlOfWorld())
		return false;
#endif

#if !__FINAL
	return CPedPopulation::ms_bAllowRagdolls;
#else
	return true;
#endif
};

void CPhysics::SetAllowedPenetration( float fAllowedPenetration )
{
	phSimulator::SetAllowedPenetration(fAllowedPenetration);
}


PF_DRAW_ONLY(static void ToggleDrawMoverMapCollisionCB());

//
// name:		CPhysics::RenderDebug
// description:	Render physics debug 
void CPhysics::RenderDebug()
{
#if __PFDRAW
	sysCriticalSection critSec(sPhysicsPFDSemaCritSec);

	if (sPhysicsPFDDrawReady)
	{
		if (!sysIpcWaitSemaTimed(sPhysicsPFDDrawReady, sPhysicsSynchronizationMaxWait))
		{
			sysIpcDeleteSema(sPhysicsPFDDrawReady);
			sPhysicsPFDDrawReady = NULL;
		}
	}

	grcDebugDraw::SetDefaultDebugRenderStates();
	// Override lighting mode
	grcLightState::SetEnabled(true);

	const Matrix34 & cameraMtx = RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx());
	GetRageProfileDraw().SendDrawCameraForServer(cameraMtx);

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_C, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT)) // shortcut to toggle mover bounds
	{
		CPhysics::ms_bDrawMoverMapCollision = !CPhysics::ms_bDrawMoverMapCollision;
		ToggleDrawMoverMapCollisionCB();
	}

	if(PFDGROUP_Physics.WillDraw())
		ms_PhysSim->ProfileDraw();

	ms_ClothManager->ProfileDraw();
	ms_RopeManager->ProfileDraw();
	bgGlassManager::ProfileDraw();

	// Already called from CRenderPhaseDebug3d::StaticRender
	// I'm not aware of a reason to draw twice
//	ms_mouseInput->Draw();
#if DR_ENABLED
	if (grcViewport::GetCurrent())
	{
		debugPlayback::SetCameraCullingInfo( grcViewport::GetCurrent()->GetFrustumLRTB() );
	}
	debugPlayback::DebugDraw(fwTimer::IsUserPaused());
#endif
	if (sPhysicsPFDDrawDone)
	{
		sysIpcSignalSema(sPhysicsPFDDrawDone);
	}
#endif

#if __BANK
	if(gbPrintPhysicsActive || gbPrintPhysicsInActive || gbPrintPhysicsFixed || gbPhysicsInstCounters)
		PrintPhysicsInstList(gbPrintPhysicsActive || gbPrintPhysicsInActive || gbPrintPhysicsFixed);
	if(gbDisplayVectorMapPhysicsInsts)
		DisplayVectorMapPhysicsInsts();

	if(ms_bDebugRagdollCount)
		DisplayRagdollCount();

	if(CBuoyancy::ms_bDebugDisplaySubmergedLevels)
	{
		CBuoyancy::DisplayBuoyancyInfo();
	}

	if(ms_bDisplaySelectedPhysicalCollisionRecords || ms_bDisplayAllCollisionRecords )
		DisplayCollisionRecords();

	if(ms_bDisplayNumberOfBrokenPartsFocusFrag)
		DisplayNumPartsBrokenOffFocusFrag();

	if(ms_bDisplayPedHealth || ms_bDisplayPedFallHeight)
	{
		DisplayRagdollDamageInfo();
	}
#endif //__BANK

#if NORTH_CLOTHS
	CClothMgr::RenderDebug();
#endif
#if RAGE_CLOTHS
	CClothRageMgr::RenderDebug();
#endif

}


#if __BANK
void CPhysics::PrintPhysicsInstList(bool bIncludeList)
{
	atMap<u32, s32> objMap;
	s32 instCount=0;
	s32 instActiveCount=0;
	s32 instFixedCount=0;
	s32 instInactiveCount=0;
	for(int i = 0; i < ms_PhysLevel->GetMaxObjects(); ++i)
	{
		if(!ms_PhysLevel->IsNonexistent(i))
		{
			phInst* pInst = ms_PhysLevel->GetInstance(i);
			CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
			if(pEntity && pInst->GetClassType() != PH_INST_MAPCOL)
			{
				bool bAddToObjMap = false;
				if(ms_PhysLevel->IsActive(pInst->GetLevelIndex()))
				{
					instActiveCount++;
					if(gbPrintPhysicsActive)
						bAddToObjMap = true;
				}
				if(ms_PhysLevel->IsInactive(pInst->GetLevelIndex()))
				{
					instInactiveCount++;
					if(gbPrintPhysicsInActive)
						bAddToObjMap = true;
				}
				if(ms_PhysLevel->IsFixed(pInst->GetLevelIndex()))
				{
					instFixedCount++;
					if(gbPrintPhysicsFixed)
						bAddToObjMap = true;
				}
				instCount++;

				if(bAddToObjMap)
				{
					s32 modelIndex = pEntity->GetModelIndex();
					if(objMap.Access(modelIndex))
						objMap[modelIndex]++;
					else
						objMap[modelIndex]=1;
				}
			}
		}
	}

	grcDebugDraw::AddDebugOutput("%d physics instances, active %d, inactive %d, fixed %d", instCount, instActiveCount, instInactiveCount, instFixedCount);

	if(bIncludeList)
	{
		for(s32 i=0; i<objMap.GetNumSlots(); i++)
		{
			const atMap<u32, s32>::Entry* pEntry = objMap.GetEntry(i);
			if(pEntry)
				grcDebugDraw::AddDebugOutput("%s: %d", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(pEntry->key))), pEntry->data);
		}
	}
}

//
// name:		CPhysics::DisplayVectorMapPhysicsInsts
// description:	Display physics insts on VectorMap
void CPhysics::DisplayVectorMapPhysicsInsts()
{
	Color32 activeColour(0xff, 0x00, 0x00);
	Color32 inactiveColour(0x00, 0xff, 0x00);
	Color32 fixedColour(0x00, 0x00, 0xff);
	for(int i = 0; i < ms_PhysLevel->GetMaxObjects(); ++i)
	{
		if(!ms_PhysLevel->IsNonexistent(i))
		{
			phInst* pInst = ms_PhysLevel->GetInstance(i);
			CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
			if(pEntity && pInst->GetClassType() != PH_INST_MAPCOL)
			{
				spdAABB tempBox;
				const spdAABB &bb = pEntity->GetBoundBox(tempBox);
				if(ms_PhysLevel->IsActive(pInst->GetLevelIndex()))
				{
					fwVectorMap::Draw3DBoundingBoxOnVMap(bb, activeColour);
				}
				else if(ms_PhysLevel->IsInactive(pInst->GetLevelIndex()))
				{
					fwVectorMap::Draw3DBoundingBoxOnVMap(bb, inactiveColour);
				}
				else if(ms_PhysLevel->IsFixed(pInst->GetLevelIndex()))
				{
					fwVectorMap::Draw3DBoundingBoxOnVMap(bb, fixedColour);
				}
			}
		}
	}

}

#if USE_FRAG_TUNE

void AddFragTuneWidgetCB()
{
	if(numFragNames > 0 && currFragNameSelection < numFragNames)
	{
		CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId((strLocalIndex(fragSorts[currFragNameSelection]))));
		if(pModelInfo->GetFragType())
		{
			BANK_ONLY(FRAGTUNE->AddArchetypeWidgets(pModelInfo->GetFragType()));
		}
	}
}

//extern s32 carModelId;

void TestBoundPerformaceCB()
{
	fwModelId carModelId((strLocalIndex(CVehicleFactory::carModelId)));

	if(carModelId.IsValid())
	{
		bool bForceLoad = false;
		if(!CModelInfo::HaveAssetsLoaded(carModelId))
		{
			CModelInfo::RequestAssets(carModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			bForceLoad = true;
		}
		if(bForceLoad)
		{
			CStreaming::LoadAllRequestedObjects(true);
		}
		if(!CModelInfo::HaveAssetsLoaded(carModelId))
		{
			return;
		}

		float fTimeCreate, fTimeTest, fTimeDestroy;
		sysTimer boundPerfTimer;

		Matrix34 tempMat;
		tempMat.Identity();

		CVehicle* pVeh1 = CVehicleFactory::GetFactory()->Create(carModelId, ENTITY_OWNEDBY_PHYSICS, POPTYPE_RANDOM_AMBIENT, &tempMat);
		CVehicle* pVeh2 = CVehicleFactory::GetFactory()->Create(carModelId, ENTITY_OWNEDBY_PHYSICS, POPTYPE_RANDOM_AMBIENT, &tempMat);

		fTimeCreate = boundPerfTimer.GetMsTime();

		WorldProbe::CShapeTestBoundDesc boundTestDesc;
		boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		boundTestDesc.SetBoundFromEntity(pVeh1);
		boundTestDesc.SetIncludeEntity(pVeh2);
		boundTestDesc.SetTransform(&tempMat);
		// RA: I've changed this test over to the new API. Note that the original didn't do anything
		// with the result of the test either!
		WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc);

		fTimeTest = boundPerfTimer.GetMsTime() - fTimeCreate;

		CVehicleFactory::GetFactory()->Destroy(pVeh1);
		CVehicleFactory::GetFactory()->Destroy(pVeh2);

		fTimeDestroy = boundPerfTimer.GetMsTime() - fTimeTest;

		Displayf("Bound Perf Time - Create: %f, Test: %f, Destroy: %f", fTimeCreate, fTimeTest, fTimeDestroy);
	}
}

int CompareModelInfosCB(const void* pVoidA, const void* pVoidB)
{
	const u32* pA = (const u32*)pVoidA;
	const u32* pB = (const u32*)pVoidB;

	return stricmp(CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(*pA))),CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(*pB))));
}

bool AddToFragListCB(CEntity* pEntity, void* UNUSED_PARAM(data))
{
	if(pEntity && pEntity->IsArchetypeSet())
	{
		CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
		// check that this model is a fragment type
		if(!pModelInfo || pModelInfo->GetFragType()==NULL)
			return true;

		for(int i=0; i<numFragNames; i++)
		{
			if(fragSorts[i]==pEntity->GetModelIndex())
				return true;
		}

		if(numFragNames < MAX_FRAG_TYPES)
		{
			fragSorts[numFragNames] = pEntity->GetModelIndex();
			numFragNames++;
		}
	}

	// return true to keep looping through all entities
	return true;
}

void UpdateFragList()
{
	// fill the list of local model indicies
	numFragNames = 0;
	fwIsSphereIntersecting testSphere(spdSphere(VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors()), ScalarV(50.0f)));
	CGameWorld::ForAllEntitiesIntersecting(&testSphere, AddToFragListCB, NULL, 
		(ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT), SEARCH_LOCATION_EXTERIORS,
		SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_DEBUG);
		/*false, true, true, true, true, false, false, false);*/

	// sort this list
	qsort(fragSorts, numFragNames, sizeof(s32), CompareModelInfosCB);

	// reset names array, and re-fill
	fragNames.Reset();
	

	for(int i=0; i<numFragNames; i++)
	{
		fragNames.PushAndGrow(CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(fragSorts[i]))));
	}

	currFragNameSelection = 0;
	if(numFragNames == 0)
	{
		pFragCombo->UpdateCombo("Local Frag Types", &currFragNameSelection, numFragNames, NULL);
	}
	else
	{
		pFragCombo->UpdateCombo("Local Frag Types", &currFragNameSelection, numFragNames, &fragNames[0]);
	}
}
#endif // USE_FRAG_TUNE

bool CPhysics::ms_bDebugMeasuringTool = false;
s32 CPhysics::ms_iNumInstances = 1;
float CPhysics::ms_fWidth = 0.0f;
float CPhysics::ms_fCapsuleWidth = 0.1f;
float CPhysics::ms_fCapsuleWidth2 = 0.1f;
float CPhysics::ms_fBoundingBoxWidth = 2.0f;
float CPhysics::ms_fBoundingBoxLength = 1.0f;
float CPhysics::ms_fBoundingBoxHeight = 1.0f;
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
float CPhysics::ms_fSecondSurfaceInterp = 0.0f;
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
bool CPhysics::ms_bDoLineProbeTest = false;
bool CPhysics::ms_bDoAsyncLOSTest = false;
bool CPhysics::ms_bDoAsyncSweptSphereTest = false;
bool CPhysics::ms_bDoLineProbeWaterTest = false;
bool CPhysics::ms_bDoCapsuleTest = false;
bool CPhysics::ms_bDoCapsuleBatchShapeTest = false;
bool CPhysics::ms_bDoBoundingBoxTest = false;
bool CPhysics::ms_bDoTaperedCapsuleBatchShapeTest = false;
bool CPhysics::ms_bDoProbeBatchShapeTest = false;
bool CPhysics::ms_bDoBooleanTest = false;
bool CPhysics::ms_bTestCapsuleAgainstFocusEntity = false;
bool CPhysics::ms_bTestCapsuleAgainstFocusEntityBBoxes = false;
bool CPhysics::ms_bDoBringVehicleToHaltTest = false;
bool CPhysics::ms_bDoPlayerEntityTest = false;
bool CPhysics::ms_bRenderAllHistories = false;
bool CPhysics::ms_bRenderFocusHistory = false;
bool CPhysics::ms_bDebugRagdollCount = false;
bool CPhysics::ms_bRenderFocusImpulses = false;
bool CPhysics::ms_bLOSIgnoreSeeThru = false;
bool CPhysics::ms_bLOSIgnoreShootThru = false;
bool CPhysics::ms_bLOSIgnoreShootThruFX = false;
bool CPhysics::ms_bLOSGoThroughGlass = false;
int CPhysics::ms_iNumIsecsTaperedCapsule = 64;
bool CPhysics::ms_bDoInitialSphereTestForCapsuleTest = false;
bool CPhysics::ms_bDoSphereTest = false;
bool CPhysics::ms_bDoSphereTestNotFixed = false;
bool CPhysics::ms_bShowFoliageImpactsPeds = false;
bool CPhysics::ms_bShowFoliageImpactsVehicles = false;
bool CPhysics::ms_bShowFoliageImpactsRagdolls = false;
float CPhysics::ms_fSphereRadius = 1.0f;

Vector3 CPhysics::g_vClickedPos[2];
void* CPhysics::g_pPointers[2];
Vector3 CPhysics::g_vClickedNormal[2] = {Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f)};
bool CPhysics::g_bHasPosition[2] = {false, false};

char CPhysics::ms_Pos1[256] = "\0";
char CPhysics::ms_Ptr1[20] = "\0";
char CPhysics::ms_Pos2[256] = "\0";
char CPhysics::ms_Ptr2[20] = "\0";
char CPhysics::ms_Diff[256] = "\0";
char CPhysics::ms_HeadingDiffRadians[256] = "\0";
char CPhysics::ms_HeadingDiffDegrees[256] = "\0";
char CPhysics::ms_Distance[256] = "\0";
char CPhysics::ms_HorizDistance[256] = "\0";
char CPhysics::ms_VerticalDistance[256] = "\0";
char CPhysics::ms_Normal1[256] = "\0";
char CPhysics::ms_Normal2[256] = "\0";
char CPhysics::ms_PlayerEntityTestResult[256] = "\0";
char CPhysics::ms_zExcludedEntityList[352] = "\0";

void UpdatePos1(void)
{
	Vector3 vecPos(0.0f,0.0f,0.0f);
	if(sscanf(CPhysics::ms_Pos1, "%f, %f, %f", &vecPos.x, &vecPos.y, &vecPos.z))
	{
		CPhysics::g_vClickedPos[0] = vecPos;
		CPhysics::g_bHasPosition[0] = true;
	}
}

void UpdatePos2(void)
{
	Vector3 vecPos(0.0f,0.0f,0.0f);
	if(sscanf(CPhysics::ms_Pos2, "%f, %f, %f", &vecPos.x, &vecPos.y, &vecPos.z))
	{
		CPhysics::g_vClickedPos[1] = vecPos;
		CPhysics::g_bHasPosition[1] = true;
	}
}

void UpdateNormal1(void)
{
	Vector3 vecPos(0.0f,0.0f,0.0f);
	if(sscanf(CPhysics::ms_Normal1, "%f, %f, %f", &vecPos.x, &vecPos.y, &vecPos.z))
	{
		CPhysics::g_vClickedNormal[0] = vecPos;
	}
}

void UpdateNormal2(void)
{
	Vector3 vecPos(0.0f,0.0f,0.0f);
	if(sscanf(CPhysics::ms_Normal2, "%f, %f, %f", &vecPos.x, &vecPos.y, &vecPos.z))
	{
		CPhysics::g_vClickedNormal[1] = vecPos;
	}
}

bool CPhysics::GetMeasuringToolPos(s32 iPos, Vector3& vOutPos)
{
	if( CPhysics::g_bHasPosition[iPos] )
	{
		vOutPos = CPhysics::g_vClickedPos[iPos];
		return true;
	}
	return false;
}

PF_PAGE(GTAphysicstests, "Gta physics tests");
PF_GROUP(PhysicsTests);
PF_LINK(GTAphysicstests, PhysicsTests);
PF_TIMER(IndividualLineProbes, PhysicsTests);
PF_TIMER(IndividualCapsules, PhysicsTests);
PF_TIMER(IndividualSpheres, PhysicsTests);
PF_TIMER(LineProbeBatch, PhysicsTests);
PF_TIMER(CapsuleBatch, PhysicsTests);


void CPhysics::UpdateDebugMeasuringTool()
{
	Vector3 vPos;
	Vector3 vNormal;
	void *entity;
	bool bHasPosition = CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_ALL_TYPES_MOVER, &vNormal, &entity);
	static bool sbLeftButtonHeld = false;
	static bool sbRightButtonHeld = false;
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		sbLeftButtonHeld = true;
	}
	if( sbLeftButtonHeld &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT )
	{
		sbLeftButtonHeld = false;
	}
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		sbRightButtonHeld = true;
	}
	if( sbRightButtonHeld &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT )
	{
		sbRightButtonHeld = false;
	}

	if( bHasPosition )
	{
		if( sbLeftButtonHeld )
		{
			g_vClickedPos[0] = vPos;
			g_vClickedNormal[0] = vNormal;
			g_pPointers[0] = entity;
			g_bHasPosition[0] = true;
			sprintf(ms_Pos1, "%.5f, %.5f, %.5f", g_vClickedPos[0].x, g_vClickedPos[0].y, g_vClickedPos[0].z);
			sprintf(ms_Ptr1, "%p", g_pPointers[0]);
			sprintf(ms_Normal1, "%.5f, %.5f, %.5f", g_vClickedNormal[0].x, g_vClickedNormal[0].y, g_vClickedNormal[0].z);
		}
		else if( sbRightButtonHeld )
		{
			g_vClickedPos[1] = vPos;
			g_vClickedNormal[1] = vNormal;
			g_pPointers[1] = entity;
			g_bHasPosition[1] = true;
			sprintf(ms_Pos2, "%.5f, %.5f, %.5f", g_vClickedPos[1].x, g_vClickedPos[1].y, g_vClickedPos[1].z);
			sprintf(ms_Ptr2, "%p", g_pPointers[1]);
			sprintf(ms_Normal2, "%.5f, %.5f, %.5f", g_vClickedNormal[1].x, g_vClickedNormal[1].y, g_vClickedNormal[1].z);
		}
		else if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_MIDDLE )
		{
			g_bHasPosition[0] = false;
			g_bHasPosition[1] = false;
			sbLeftButtonHeld = false;
			sbRightButtonHeld = false;
			ms_Pos1[0] = '\0';
			ms_Pos2[0] = '\0';
			ms_Diff[0] = '\0';
			ms_HeadingDiffRadians[0] = '\0';
			ms_HeadingDiffDegrees[0] = '\0';
			ms_Distance[0] = '\0';
			ms_HorizDistance[0] = '\0';
			ms_VerticalDistance[0] = '\0';
			ms_Normal1[0] = '\0';
			ms_Normal2[0] = '\0';
		}
	}

	if( g_bHasPosition[0] )
	{
		grcDebugDraw::Sphere(g_vClickedPos[0], 0.1f, Color32(0.0f, 0.0f, 1.0f) );
		grcDebugDraw::Line(g_vClickedPos[0], g_vClickedPos[0]+(g_vClickedNormal[0]*0.4f), Color32(0.0f, 0.0f, 1.0f) );

		updatePlayerEntityTest();

		if(ms_bDoBringVehicleToHaltTest)
		{
			DoBringVehicleToHaltTest(g_vClickedPos[0]);
		}
	}
	if( g_bHasPosition[1] )
	{
		grcDebugDraw::Sphere(g_vClickedPos[1], 0.05f, Color32(0.0f, 1.0f, 0.0f) );
		grcDebugDraw::Line(g_vClickedPos[1], g_vClickedPos[1]+(g_vClickedNormal[1]*0.4f), Color32(0.0f, 1.0f, 0.0f) );
	}
	if( g_bHasPosition[0] && g_bHasPosition[1] )
	{
		grcDebugDraw::Line(g_vClickedPos[0], g_vClickedPos[1], Color32(0.0f, 0.0f, 1.0f), Color32(0.0f, 1.0f, 0.0f) );
		Vector3 vDiff = g_vClickedPos[1] - g_vClickedPos[0];
		Vector3 vDiffNorm = vDiff;
		vDiffNorm.z = 0.0f;
		vDiffNorm.Normalize();
		sprintf(ms_Diff, "%.2f, %.2f, %.2f", vDiff.x, vDiff.y, vDiff.z);
		sprintf(ms_HeadingDiffRadians, "%.2f", rage::Atan2f(-vDiffNorm.x, vDiffNorm.y));
		sprintf(ms_HeadingDiffDegrees, "%.2f", rage::Atan2f(-vDiffNorm.x, vDiffNorm.y)*180.0f/(float)PI);
		sprintf(ms_Distance, "%.2f", vDiff.Mag());
		sprintf(ms_HorizDistance, "%.2f", vDiff.XYMag());
		sprintf(ms_VerticalDistance, "%.2f", vDiff.z);
	}

	if( g_bHasPosition[0] && g_bHasPosition[1] )
	{
		PF_PUSH_TIMEBAR_DETAIL("PhysicsShapeTests");
		// Do one tapered capsule test now and then retest intersections during a loop
		// This simulates the camera collision code
		if(ms_bDoTaperedCapsuleBatchShapeTest)
		{
#if USE_TAPERED_CAPSULE
			DoMeasuringToolTaperedCapsuleTests();
#endif
		}

		if( CPhysics::ms_bDoLineProbeTest || CPhysics::ms_bDoLineProbeWaterTest || CPhysics::ms_bDoCapsuleBatchShapeTest || 
			CPhysics::ms_bDoProbeBatchShapeTest || CPhysics::ms_bDoCapsuleTest || CPhysics::ms_bDoSphereTest || CPhysics::ms_bDoBoundingBoxTest )
		{
			DoMeasuringToolBatchTests();
		}
		PF_POP_TIMEBAR_DETAIL();

		static WorldProbe::CShapeTestFixedResults<> sShapeTestHandleProbe;
		if(ms_bDoAsyncLOSTest)
		{
			static Vector3 vResult = VEC3_ZERO;
			static bool bHitSomething = false;
			if(!sShapeTestHandleProbe.GetResultsWaitingOrReady())
			{
				s32 nInclude = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
				int nOptions = 0;
				if(CPhysics::ms_bLOSIgnoreSeeThru)
				{
					nOptions |= WorldProbe::LOS_IGNORE_SEE_THRU;
				}
				if(CPhysics::ms_bLOSIgnoreShootThru)
				{			
					nOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU;
				}
				if(CPhysics::ms_bLOSIgnoreShootThruFX)
				{
					nOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU_FX;
				}
				if(CPhysics::ms_bLOSGoThroughGlass)
				{
					nOptions |= WorldProbe::LOS_GO_THRU_GLASS;
				}

				sShapeTestHandleProbe.Reset();
				WorldProbe::CShapeTestProbeDesc probeData;
				probeData.SetResultsStructure(&sShapeTestHandleProbe);
				probeData.SetStartAndEnd(g_vClickedPos[0],g_vClickedPos[1]);
				probeData.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
				probeData.SetContext(WorldProbe::ENotSpecified);
				probeData.SetIncludeFlags(nInclude);
				probeData.SetOptions(nOptions);
				probeData.SetIsDirected(true);
				WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
				bHitSomething = false;
			}
			else if(sShapeTestHandleProbe.GetResultsReady())
			{
				bHitSomething = sShapeTestHandleProbe.GetNumHits() > 0u;

				if(bHitSomething)
				{
					vResult = sShapeTestHandleProbe[0].GetHitPosition();
				}
			}
				
			if(bHitSomething)
			{
				grcDebugDraw::Sphere(vResult, 0.2f, Color_red);
			}
		}
		else
		{
			sShapeTestHandleProbe.Reset();
		}

		static WorldProbe::CShapeTestFixedResults<> sShapeTestHandleSweptSphere;
		if(ms_bDoAsyncSweptSphereTest)
		{
			static Vector3 vResult = VEC3_ZERO;
			static bool bHitSomething = false;
			if(!sShapeTestHandleSweptSphere.GetResultsWaitingOrReady())
			{
				s32 nInclude = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
				int nOptions = 0;
				if(CPhysics::ms_bLOSIgnoreSeeThru)
				{
					nOptions |= WorldProbe::LOS_IGNORE_SEE_THRU;
				}
				if(CPhysics::ms_bLOSIgnoreShootThru)
				{			
					nOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU;
				}
				if(CPhysics::ms_bLOSIgnoreShootThruFX)
				{
					nOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU_FX;
				}
				if(CPhysics::ms_bLOSGoThroughGlass)
				{
					nOptions |= WorldProbe::LOS_GO_THRU_GLASS;
				}

				sShapeTestHandleSweptSphere.Reset();
				WorldProbe::CShapeTestCapsuleDesc sweptSphereDesc;
				sweptSphereDesc.SetResultsStructure(&sShapeTestHandleSweptSphere);
				sweptSphereDesc.SetCapsule(g_vClickedPos[0], g_vClickedPos[1], ms_fCapsuleWidth);
				sweptSphereDesc.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
				sweptSphereDesc.SetContext(WorldProbe::ENotSpecified);
				sweptSphereDesc.SetIncludeFlags(nInclude);
				sweptSphereDesc.SetOptions(nOptions);
				sweptSphereDesc.SetIsDirected(true);
				WorldProbe::GetShapeTestManager()->SubmitTest(sweptSphereDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
				bHitSomething = false;
			}
			else if(sShapeTestHandleSweptSphere.GetResultsReady())
			{
				bHitSomething = sShapeTestHandleSweptSphere.GetNumHits() > 0;

				if(bHitSomething)
				{
					vResult = sShapeTestHandleSweptSphere[0].GetHitPosition();
				}
			}

			if(bHitSomething)
			{
				grcDebugDraw::Sphere(vResult, 0.2f, Color_red);
			}
		}
		else
		{
			sShapeTestHandleSweptSphere.Reset();
		}

		if(ms_bTestCapsuleAgainstFocusEntity)
		{
			for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
			{
				CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
				for( s32 i = 0; i < ms_iNumInstances; i++ )
				{
					if(pEnt)
					{
						const u32 nNumIntersections = 10;
						WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
						WorldProbe::CShapeTestFixedResults<nNumIntersections> capsuleResults;
						capsuleDesc.SetResultsStructure(&capsuleResults);
						capsuleDesc.SetCapsule(g_vClickedPos[0], g_vClickedPos[1], ms_fCapsuleWidth);
						capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
						capsuleDesc.SetIncludeEntity(pEnt);
						capsuleDesc.SetDoInitialSphereCheck(false);
						capsuleDesc.SetIsDirected(true);
						if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
						{
							for(WorldProbe::ResultIterator it = capsuleResults.begin(); it < capsuleResults.last_result(); ++it)
							{
								if(it->GetHitDetected())
								{
									grcDebugDraw::Cross(RCC_VEC3V(it->GetHitPosition()),0.05f,Color_red);
									char partStr[4];
									formatf(partStr,4,"%i",it->GetHitPartIndex());
									grcDebugDraw::Text(it->GetHitPosition(),Color_white,partStr,false);
								}
							}
						}
					}
				}
			}
		}
		if(ms_bTestCapsuleAgainstFocusEntityBBoxes)
		{
			for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
			{
				CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
				if(pEnt && (pEnt->GetType()==ENTITY_TYPE_VEHICLE))
				{
					CVehicle * pVehicle = (CVehicle*)pEnt;
					static const u32 iNumComponents = 5;
					static const eHierarchyId iIDs[iNumComponents] =
					{
						VEH_CHASSIS, VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R
					};

					int iComponents[8];
					int iNum = 0;
					for(unsigned int i=0; i<iNumComponents; i++)
					{
						const int iComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(iIDs[i]));
						if(iComponent >= 0)
							iComponents[iNum++] = iComponent;
					}

					for( s32 i = 0; i < ms_iNumInstances; i++ )
					{
						phIntersection intersection;
						CPedGeometryAnalyser::TestCapsuleAgainstComposite(pEnt, g_vClickedPos[0], g_vClickedPos[1], ms_fCapsuleWidth, true, 0, NULL, iNum, iComponents);
					}
				}
			}
		}
	}

	// Visualise a river's velocity vector field around the debug point.
	if(CBuoyancy::ms_bVisualiseRiverFlowField && g_bHasPosition[0])
	{
		for(int i = 0; i < CBuoyancy::ms_nGridSizeX; ++i)
		{
			for(int j = 0; j < CBuoyancy::ms_nGridSizeY; ++j)
			{
				Vector3 vGridPos = g_vClickedPos[0];
				float fOffsetX = i*2.0f - float(CBuoyancy::ms_nGridSizeX);
				float fOffsetY = j*2.0f - float(CBuoyancy::ms_nGridSizeY);
				vGridPos.x += fOffsetX*CBuoyancy::ms_fGridSpacing;
				vGridPos.y += fOffsetY*CBuoyancy::ms_fGridSpacing;

				Vector2 vFlow2D;
				River::GetRiverFlowFromPosition(RCC_VEC3V(vGridPos), vFlow2D);
				vFlow2D.Scale(1.0f/River::GetRiverPushForce());
				Vector3 vFlow3D(vFlow2D.x, vFlow2D.y, 0.0f);

				Vector3 vArrowPos(vGridPos.x, vGridPos.y, vGridPos.z+CBuoyancy::ms_fHeightOffset);
				Vector3 vArrowDir = vFlow3D;
				vArrowDir.Scale(CBuoyancy::ms_fScaleFactor);
				Color32 colour = Color_yellow;
				const float fRapidSpeed = CTaskNMRiverRapids::sm_Tunables.m_fMinRiverFlowForRapids;
				Vector3 vRiverSpeed = vFlow3D;
				vRiverSpeed.Scale(CBuoyancy::GetDefaultRiverFlowVelocityScaleFactor());
				if(vRiverSpeed.Mag2() > fRapidSpeed*fRapidSpeed)
					colour = Color_red;
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vArrowPos), VECTOR3_TO_VEC3V(vArrowPos+vArrowDir), 0.1f, colour);
			}
		}
	}
}

#if USE_TAPERED_CAPSULE
void CPhysics::DoMeasuringToolTaperedCapsuleTests()
{
	// Do one tapered capsule test now and then retest intersections during a loop
	// This simulates the camera collision code

	
			Vector3 vMainStart = g_vClickedPos[0];
			Vector3 vMainEnd = g_vClickedPos[1];
			Vector3 vStartToEnd = vMainEnd - vMainStart;
			vStartToEnd.Normalize();
			Vector3 vCross;
			if( ABS(vStartToEnd.z) > 0.95f )
			{
				vCross.Cross(vStartToEnd, XAXIS);
			}
			else
			{
				vCross.Cross(vStartToEnd, ZAXIS);
			}

	// Set up tapered capsule tester
	phShapeTest<phShapeObject> taperedCapsuleTester;
	phBoundTaperedCapsule taperedCapsuleBound;
	Matrix34 taperedCapsuleMat;

	phIntersection* taperedCapsuleIntersections = Alloca(phIntersection,ms_iNumIsecsTaperedCapsule);

	taperedCapsuleMat.b.Subtract(vMainStart,vMainEnd);
	taperedCapsuleMat.b.NormalizeSafe();
	taperedCapsuleMat.b.MakeOrthonormals(taperedCapsuleMat.a, taperedCapsuleMat.c);
	taperedCapsuleMat.d.Average(vMainStart,vMainEnd);			

	const float fLength = (vMainStart - vMainEnd).Mag();
	Vector3 vDefaultOffset;
	vDefaultOffset.Cross(vCross, vStartToEnd);	

	taperedCapsuleBound.SetCapsuleSize(ms_fCapsuleWidth+ms_fWidth,ms_fCapsuleWidth2+ms_fWidth,fLength);
	taperedCapsuleTester.KeepNextCullData();
	taperedCapsuleTester.InitObject(taperedCapsuleBound,taperedCapsuleMat,taperedCapsuleIntersections,ms_iNumIsecsTaperedCapsule);
	taperedCapsuleTester.GetShape().SetCullType(phCullShape::PHCULLTYPE_CAPSULE);
	int iNumHits = taperedCapsuleTester.TestInLevel(NULL, ArchetypeFlags::GTA_ALL_TYPES_MOVER, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL,0,CPhysics::GetLevel());

	grcDebugDraw::Line(vMainStart, vMainEnd, Color_green, Color_green);

	for(int i =0; i < iNumHits; i++)
	{
		if(taperedCapsuleIntersections[i].IsAHit())
		{
			grcDebugDraw::Sphere(RCC_VECTOR3(taperedCapsuleIntersections[i].GetPosition()), 0.2f, Color_red);
		}
	}

	// Loop over the intersections again using retestInteresections
	if(ms_iNumInstances > 1)
	{
		for( s32 i = 0; i < ms_iNumInstances; i++ )
		{
			Matrix34 mat;
			mat.Identity();
			mat.MakeRotate(vStartToEnd, ((float)i/(float)ms_iNumInstances) * TWO_PI);
			Vector3 vThisOffset = vDefaultOffset;
			mat.Transform3x3(vThisOffset);
			vThisOffset.Scale(ms_fWidth);
			Vector3 vStart = vMainStart + vThisOffset;
			Vector3 vEnd = vMainEnd + vThisOffset;

			if( CPhysics::ms_bDoTaperedCapsuleBatchShapeTest)
			{					
				taperedCapsuleMat.d.Average(vStart,vEnd);

				// For each test we want to retest the original main intersection
				// so need to copy these into a temp array to avoid stomping over them
				phIntersection* taperedCapsuleLoopIntersections = Alloca(phIntersection,ms_iNumIsecsTaperedCapsule);
				for(int iIntersectionIndex = 0; iIntersectionIndex < ms_iNumIsecsTaperedCapsule; iIntersectionIndex++)
				{
					taperedCapsuleLoopIntersections[iIntersectionIndex].Copy(taperedCapsuleIntersections[iIntersectionIndex]);
				}

				const float fLength = (vStart - vEnd).Mag();
				taperedCapsuleBound.SetCapsuleSize(ms_fCapsuleWidth,ms_fCapsuleWidth2,fLength);
				taperedCapsuleTester.InitObject(taperedCapsuleBound,taperedCapsuleMat);
				taperedCapsuleTester.GetShape().SetCullType(phCullShape::PHCULLTYPE_CAPSULE);
				taperedCapsuleTester.GetShape().SetIntersections(taperedCapsuleLoopIntersections,ms_iNumIsecsTaperedCapsule);
				iNumHits = taperedCapsuleTester.RetestIntersections();

				grcDebugDraw::Line(vStart, vEnd, Color_maroon2, Color_maroon2);

				for(int i =0; i < iNumHits; i++)
				{
					if(taperedCapsuleLoopIntersections[i].IsAHit())
					{
						grcDebugDraw::Sphere(RCC_VECTOR3(taperedCapsuleLoopIntersections[i].GetPosition()), 0.2f, Color_red);
					}
				}
			}
		}
	}
	taperedCapsuleBound.Release(false);
}
#endif // USE_TAPERED_CAPSULE

void CPhysics::AddFocusEntityToExclusionList()
{
	// Called by a RAG button widget to build up a list of CEntity objects which
	// should be excluded from the shape tests done in DoMeasuringToolBatchTests().

	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if(!pEnt)
		return;

	// Search through list and see if that entity is already in the list.
	bool bAlreadyInList = false;
	for(int i = 0; i < g_nNumExcludedEntities; ++i)
	{
		if(g_pExcludeEntityList[i] == pEnt)
		{
			bAlreadyInList = true;
			break;
		}
	}

	if(!bAlreadyInList)
	{
		g_pExcludeEntityList[g_nNumExcludedEntities++] = pEnt;
	}

	for(int i = 0; i < g_nNumExcludedEntities; ++i)
	{
		sprintf(ms_zExcludedEntityList+11*i, "0x%p ", g_pExcludeEntityList[i]);
	}
	ms_zExcludedEntityList[g_nNumExcludedEntities*11 - 1] = '\0';
}

void CPhysics::ClearEntityExclusionList()
{
	for(int i = 0; i < PROCESS_LOS_MAX_EXCEPTIONS; ++i)
	{
		g_pExcludeEntityList[i] = 0;
	}
	g_nNumExcludedEntities = 0;
	ms_zExcludedEntityList[0] = '\0';
}

void CPhysics::DoMeasuringToolBatchTests()
{
	int nOptions = 0;
	if(CPhysics::ms_bLOSIgnoreSeeThru)
	{
		nOptions |= WorldProbe::LOS_IGNORE_SEE_THRU;
	}
	if(CPhysics::ms_bLOSIgnoreShootThru)
	{
		nOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU;
	}
	if(CPhysics::ms_bLOSIgnoreShootThruFX)
	{
		nOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU_FX;
	}
	if(CPhysics::ms_bLOSGoThroughGlass)
	{
		nOptions |=WorldProbe::LOS_GO_THRU_GLASS;
	}

	Vector3 vMainStart = g_vClickedPos[0];
	Vector3 vMainEnd = g_vClickedPos[1];
	Vector3 vStartToEnd = vMainEnd - vMainStart;
	vStartToEnd.Normalize();
	Vector3 vCross;
	if( ABS(vStartToEnd.z) > 0.95f )
	{
		vCross.Cross(vStartToEnd, XAXIS);
	}
	else
	{
		vCross.Cross(vStartToEnd, ZAXIS);
	}

	const int maxBatchedCapsules = 128;
	const int maxBatchedProbes = 128;

	WorldProbe::CShapeTestHitPoint aProbeIntersections[maxBatchedProbes];
	phSegment aProbeSegments[maxBatchedProbes];

	phShapeTest< phShapeBatch > batchCapsuleTester;
	batchCapsuleTester.GetShape().AllocateSweptSpheres(maxBatchedProbes);
	phIntersection aCapsuleIntersections[maxBatchedCapsules];
	phSegment aCapsuleSegments[maxBatchedCapsules];

	WorldProbe::CShapeTestBatchDesc batchProbeDesc;
	batchProbeDesc.SetOptions(0);
	batchProbeDesc.SetIncludeFlags(INCLUDE_FLAGS_ALL);
	batchProbeDesc.SetTypeFlags(TYPE_FLAGS_ALL);
	batchProbeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
	ALLOC_AND_SET_PROBE_DESCRIPTORS(batchProbeDesc,ms_iNumInstances);

	Vector3 vDefaultOffset;
	vDefaultOffset.Cross(vCross, vStartToEnd);
	for( s32 i = 0; i < ms_iNumInstances; i++ )
	{
		Matrix34 mat;
		mat.Identity();
		mat.MakeRotate(vStartToEnd, ((float)i/(float)ms_iNumInstances) * TWO_PI);
		Vector3 vThisOffset = vDefaultOffset;
		mat.Transform3x3(vThisOffset);
		vThisOffset.Scale(ms_fWidth);
		Vector3 vStart = vMainStart + vThisOffset;
		Vector3 vEnd = vMainEnd + vThisOffset;
		if(CPhysics::ms_bDoLineProbeTest)
		{

			const int nNumResults = 16;
			PF_START(IndividualLineProbes);
			WorldProbe::CShapeTestProbeDesc shapeTestDesc;
			WorldProbe::CShapeTestFixedResults<nNumResults> probeResults;
			shapeTestDesc.SetStartAndEnd(vStart, vEnd);
			shapeTestDesc.SetResultsStructure(CPhysics::ms_bDoBooleanTest ? NULL : &probeResults);
			shapeTestDesc.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
			shapeTestDesc.SetSecondSurfaceInterp(ms_fSecondSurfaceInterp);
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
			shapeTestDesc.SetIncludeFlags(INCLUDE_FLAGS_ALL);
			shapeTestDesc.SetOptions(nOptions);

			// Register this shape test with the manager.
			bool foundHits = WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			PF_STOP(IndividualLineProbes);

			Color32 color;
			if(foundHits && CPhysics::ms_bDoBooleanTest)
			{
				color = Color_yellow;
			}
			else
			{
				color = Color_green;
			}
			grcDebugDraw::Line(vStart, vEnd, color);

			// Visualise probe and collision results.
			if(!CPhysics::ms_bDoBooleanTest)
			{
				physicsAssert(probeResults.GetResultsReady());
				WorldProbe::ResultIterator it;
				for(it = probeResults.begin(); it < probeResults.end(); ++it)
				{
					if(it->GetHitDetected())
					{
						grcDebugDraw::Sphere(it->GetHitPosition(), 0.1f, Color_yellow);
					}
				}
			}
		}
		if(CPhysics::ms_bDoLineProbeWaterTest)
		{
			Vector3 vEndOfWaterProbeArrow = vStart - Vector3(0.0f, 0.0f, 1.0f);
			grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEndOfWaterProbeArrow), 0.3f, Color_red2);

			PF_START(IndividualLineProbes);
			float fWaterHeight = -1.0f;
			static bool sbConsiderWaves = true;
			bool bFoundWater = CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vStart, &fWaterHeight, sbConsiderWaves);
			PF_STOP(IndividualLineProbes);

			// Visualise results.
			if(bFoundWater)
			{
				Vector3 vHitPosition = vStart;
				vHitPosition.z = fWaterHeight;
				grcDebugDraw::Sphere(vHitPosition, 0.1f, Color_yellow);
			}
		}
		if( CPhysics::ms_bDoCapsuleTest )
		{
			grcDebugDraw::Line(vStart, vEnd, Color_maroon2);
			
			PF_START(IndividualCapsules);
			WorldProbe::CShapeTestCapsuleDesc shapeTestDesc;
			WorldProbe::CShapeTestFixedResults<64> capsuleResults;
			shapeTestDesc.SetCapsule(vStart, vEnd, ms_fCapsuleWidth);
			shapeTestDesc.SetResultsStructure(&capsuleResults);
			shapeTestDesc.SetOptions(nOptions);
			shapeTestDesc.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
			shapeTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			shapeTestDesc.SetTypeFlags(TYPE_FLAGS_ALL);
			shapeTestDesc.SetIsDirected(true);
			shapeTestDesc.SetDoInitialSphereCheck(ms_bDoInitialSphereTestForCapsuleTest);
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
			shapeTestDesc.SetSecondSurfaceInterp(ms_fSecondSurfaceInterp);
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

			WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			PF_STOP(IndividualCapsules);

			// Visualise probe and collision results.
			physicsAssert(capsuleResults.GetResultsReady());
			WorldProbe::ResultIterator it;
			for(it = capsuleResults.begin(); it < capsuleResults.end(); ++it)
			{
				if(it->GetHitDetected())
				{
					grcDebugDraw::Sphere(it->GetHitPosition(), 0.1f, Color_yellow);
				}
			}
		}
		if( CPhysics::ms_bDoSphereTest )
		{
			grcDebugDraw::Sphere(vStart, ms_fSphereRadius, Color_orange, false);

			u32 nFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
			if(CPhysics::ms_bDoSphereTestNotFixed)
			{
				nFlags &= ~ArchetypeFlags::GTA_ALL_MAP_TYPES;
			}

			PF_START(IndividualSpheres);
			const u8 nNumSphereResults = 64;
			WorldProbe::CShapeTestSphereDesc shapeTestDesc;
			WorldProbe::CShapeTestFixedResults<nNumSphereResults> sphereResults;
			shapeTestDesc.SetSphere(vStart, ms_fSphereRadius);
			shapeTestDesc.SetResultsStructure(&sphereResults);
			shapeTestDesc.SetOptions(nOptions);
			shapeTestDesc.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
			shapeTestDesc.SetIncludeFlags(nFlags);
			shapeTestDesc.SetTypeFlags(TYPE_FLAGS_ALL);
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
			shapeTestDesc.SetSecondSurfaceInterp(ms_fSecondSurfaceInterp);
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

			WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			PF_STOP(IndividualSpheres);

			// Visualise results.
			physicsAssert(sphereResults.GetResultsReady());
			WorldProbe::ResultIterator it;
			for(it = sphereResults.begin(); it < sphereResults.last_result(); ++it)
			{
				if(it->GetHitDetected())
				{
					grcDebugDraw::Sphere(it->GetHitPosition(), 0.1f, Color_yellow);
				}
			}
		}
		if(CPhysics::ms_bDoBoundingBoxTest)
		{
			phBoundBox* pBox = rage_new phBoundBox(Vector3(ms_fBoundingBoxWidth, ms_fBoundingBoxLength, ms_fBoundingBoxHeight));
			Matrix34 testMatrix(Matrix34::IdentityType);
			testMatrix.d = vStart;

			WorldProbe::CShapeTestBoundingBoxDesc shapeTestDesc;
			WorldProbe::CShapeTestFixedResults<1> results;
			shapeTestDesc.SetBound(pBox);
			shapeTestDesc.SetTransform(&testMatrix);
			shapeTestDesc.SetResultsStructure(&results);
			shapeTestDesc.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
			u32 nIncludeFlags = INCLUDE_FLAGS_ALL & ~(ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_MAP_TYPE_MOVER
				| ArchetypeFlags::GTA_MAP_TYPE_HORSE | ArchetypeFlags::GTA_MAP_TYPE_COVER | ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
			shapeTestDesc.SetIncludeFlags(nIncludeFlags);
			shapeTestDesc.SetOptions(nOptions);

			// Register this shape test with the manager and visualise the results.
			Color32 colour = Color_blue;
			if(WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				colour = Color_yellow;
				physicsAssert(results.GetResultsReady());
				WorldProbe::ResultIterator it;
				for(it = results.begin(); it < results.end(); ++it)
				{
					if(it->GetHitDetected())
					{
						grcDebugDraw::Sphere(it->GetHitPosition(), 0.1f, Color_yellow);
					}
				}
			}
			grcDebugDraw::BoxOriented(pBox->GetBoundingBoxMin(), pBox->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(testMatrix), colour, false);
			pBox->Release();
		}

		aProbeSegments[i].Set(vStart, vEnd);
		aCapsuleSegments[i].Set(vStart, vEnd);
		aProbeIntersections[i].Reset();
		aCapsuleIntersections[i].Reset();
		if( CPhysics::ms_bDoCapsuleBatchShapeTest && i < 128 )
		{
			grcDebugDraw::Line(vStart, vEnd, Color_blue);
			batchCapsuleTester.InitSweptSphere(aCapsuleSegments[i],ms_fCapsuleWidth,&aCapsuleIntersections[i], BAD_INDEX);
		}
	}

	Matrix34 batchMatrix;
	batchMatrix.b = vMainEnd - vMainStart;
	batchMatrix.d = vMainStart + 0.5f*batchMatrix.b;
	batchMatrix.b.Normalize();
	batchMatrix.a.Cross(batchMatrix.b, Vector3(0.0f,0.0f,1.0f));
	batchMatrix.a.Normalize();
	batchMatrix.c.Cross(batchMatrix.a, batchMatrix.b);

	if( CPhysics::ms_bDoProbeBatchShapeTest )
	{
		WorldProbe::CShapeTestResults batchTestResults(aProbeIntersections, maxBatchedProbes);
		for(int i=0; i < ms_iNumInstances && i < 128; ++i)
		{
			grcDebugDraw::Line(aProbeSegments[i].A, aProbeSegments[i].B, Color_orange);

			WorldProbe::CShapeTestProbeDesc probeTestDesc;
			probeTestDesc.SetStartAndEnd(aProbeSegments[i].A, aProbeSegments[i].B);
			probeTestDesc.SetResultsStructure(&batchTestResults);
			probeTestDesc.SetFirstFreeResultOffset(i);
			probeTestDesc.SetMaxNumResultsToUse(1);
			probeTestDesc.SetExcludeEntities(g_pExcludeEntityList, g_nNumExcludedEntities);
			probeTestDesc.SetOptions(nOptions);

			batchProbeDesc.AddLineTest(probeTestDesc);
		}

		// Specify a custom cull-volume for this batch test.
		WorldProbe::CCullVolumeBoxDesc cullVolDesc;
		Vector3 vecHalfWidth;
		vecHalfWidth.x = ms_fWidth;
		vecHalfWidth.y = 0.5f*(vMainStart - vMainEnd).Mag();
		vecHalfWidth.z = ms_fWidth;
		cullVolDesc.SetBox(batchMatrix, vecHalfWidth);
		batchProbeDesc.SetCustomCullVolume(&cullVolDesc);

		WorldProbe::GetShapeTestManager()->SubmitTest(batchProbeDesc);

		// Visualise results.
		physicsAssert(batchTestResults.GetResultsReady());
		WorldProbe::ResultIterator it;
		for(it = batchTestResults.begin(); it < batchTestResults.last_result(); ++it)
		{
			if(it->GetHitDetected())
			{
				grcDebugDraw::Sphere(it->GetHitPosition(), 0.1f, Color_yellow);
			}
		}
	}

	if( CPhysics::ms_bDoCapsuleBatchShapeTest )
	{
		Vector3 vecHalfWidth;
		vecHalfWidth.x = ms_fWidth + ms_fCapsuleWidth;
		vecHalfWidth.y = (vMainStart - vMainEnd).Mag() + ms_fCapsuleWidth;
		vecHalfWidth.z = ms_fWidth + ms_fCapsuleWidth;
		batchCapsuleTester.GetShape().SetCullBox(batchMatrix, vecHalfWidth);

		PF_START(CapsuleBatch);
		batchCapsuleTester.TestInLevel(NULL, ArchetypeFlags::GTA_ALL_TYPES_MOVER, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL,0,CPhysics::GetLevel());
		PF_STOP(CapsuleBatch);

		for( s32 i = 0; i < ms_iNumInstances; i++ )
		{
			if( aCapsuleIntersections[i].IsAHit() )
				grcDebugDraw::Sphere(RCC_VECTOR3(aCapsuleIntersections[i].GetPosition()), 0.2f, Color_red);	
		}
	}
}

void CPhysics::updatePlayerEntityTest()
{

	// Should we be doing this test?
	if(!ms_bDoPlayerEntityTest){
		ms_PlayerEntityTestResult[0] = '\0';
		return;
	}

	// Test player's entity against clicked point using testEntityAgainstEntity
	CEntity* playerEntity = CGameWorld::FindLocalPlayer();
	CEntity* targetEntity = static_cast<CEntity*>(g_pPointers[0]);
	Vector3 targetPosition = g_vClickedPos[0];

	// Check we have target
	if(playerEntity == NULL){
		return;
	}

	// Move current player matrix to target position
	Matrix34 testPlayerMatrix = MAT34V_TO_MATRIX34(playerEntity->GetMatrix());
	testPlayerMatrix.d = targetPosition;

	WorldProbe::CShapeTestBoundDesc boundTestDesc;
	boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	boundTestDesc.SetBoundFromEntity(playerEntity);
	boundTestDesc.SetIncludeEntity(targetEntity);
	boundTestDesc.SetTransform(&testPlayerMatrix);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc))
	{
		sprintf(ms_PlayerEntityTestResult,"Player does overlap");
	}
	else{
		sprintf(ms_PlayerEntityTestResult,"Player doesn't overlap");
	}

}

static dev_float sfStoppingDistance = 5.0f;
static dev_u32 snTimeToStopFor = 1;
static dev_float sfTriggerDistFromLocate = 2.0f;
static dev_bool sbControlVerticalVelocity = false;
void CPhysics::DoBringVehicleToHaltTest(const Vector3& vLocatePos)
{
	// Find the player's vehicle if one exists:
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer || !pPlayer->GetIsDrivingVehicle())
		return;

	CVehicle* pVehicle = pPlayer->GetMyVehicle();
	if(!pVehicle)
		return;

	if((VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()) - vLocatePos).Mag2() < rage::square(sfTriggerDistFromLocate))
	{
		CTask* pTask = rage_new CTaskBringVehicleToHalt(sfStoppingDistance, snTimeToStopFor, sbControlVerticalVelocity);
		if(pTask)
		{
			pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_SECONDARY, pTask, VEHICLE_TASK_SECONDARY_ANIM, true);
		}
	}
}


bool CPhysics::ms_bForceDummyObjects = false;
bool CPhysics::ms_bDrawMoverMapCollision = false;
bool CPhysics::ms_bDrawWeaponMapCollision = false;
bool CPhysics::ms_bDrawHorseMapCollision = false;
bool CPhysics::ms_bDrawCoverMapCollision = false;
bool CPhysics::ms_bDrawVehicleMapCollision = false;
bool CPhysics::ms_bDrawStairsSlopeMapCollision = false;
bool CPhysics::ms_bDrawDynamicCollision = false;
bool CPhysics::ms_bDrawPedDensity = false;
bool CPhysics::ms_bDrawPavement = false;
bool CPhysics::ms_bDrawPolygonDensity = false;
bool CPhysics::ms_bDrawEdgeAngles = false;
bool CPhysics::ms_bDrawPrimitiveDensity = false;
bool CPhysics::ms_bIncludePolygons = false;
bool CPhysics::ms_bFragManagerUpdateEachSimUpdate = false;
bool CPhysics::ms_bDebugPullThingsAround = false;
float CPhysics::ms_pullForceMultiply = 7.5f;
float CPhysics::ms_pullDistMax = 100.0f;
phMouseInput* CPhysics::ms_mouseInput = NULL;
static bool g_FragDrawToggle = false;
static RegdEnt g_pThingToPullAround;
static s32 g_iComponent= s32(NULL);
static Vector3 g_vOffset = Vector3( 0.0f, 0.0f, 0.0f );
static s32 iButtonClicked = 0;

void CPhysics::UpdatePullingThingsAround()
{
	static float fDistanceFromCamera = 0.0f;
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT || 
		ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT ||
		ioMouse::GetPressedButtons() & ioMouse::MOUSE_MIDDLE )
	{
		if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
		{
			iButtonClicked = ioMouse::MOUSE_LEFT;
		}
		else if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
		{
			iButtonClicked = ioMouse::MOUSE_RIGHT;
		}
		else
		{
			iButtonClicked = ioMouse::MOUSE_MIDDLE;
		}

		if( !g_pThingToPullAround )
		{
			//CEntity* pEntity = CDebugScene::GetEntityUnderMouse( &g_vOffset, &g_iComponent, ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);//ArchetypeFlags::GTA_ALL_TYPES|ArchetypeFlags::GTA_RAGDOLL_TYPE|ArchetypeFlags::GTA_PROJECTILE_TYPE);
			CEntity* pEntity = CDebugScene::GetEntityUnderMouse( &g_vOffset, &g_iComponent, ArchetypeFlags::GTA_ALL_TYPES_MOVER|ArchetypeFlags::GTA_RAGDOLL_TYPE|ArchetypeFlags::GTA_PROJECTILE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);
			if(pEntity && pEntity->GetIsPhysical())
			{
				g_pThingToPullAround= pEntity;
				// Convert the offset into a local offset
				const Vector3 vThingPosition = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().GetPosition());
				g_vOffset = g_vOffset - vThingPosition;
				g_vOffset = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(g_vOffset)));

				// Work out how far the object was from the camera initially
				Vector3 vMouseNear, vMouseFar;
				CDebugScene::GetMousePointing( vMouseNear, vMouseFar );
				fDistanceFromCamera = vMouseNear.Dist(vThingPosition);
			}
		}
	}
	// LEFT BUTTON PULLS THE PART OF THE OBJECT INITIALLY GRABBED TO THE BIT NOW POINTED AT
	else if( !(ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) && iButtonClicked == ioMouse::MOUSE_LEFT )
	{
		Vector3	Pos;
		if(g_pThingToPullAround && g_pThingToPullAround->GetCurrentPhysicsInst() && g_pThingToPullAround->GetCurrentPhysicsInst()->IsInLevel() &&
			CDebugScene::GetWorldPositionUnderMouse(Pos, ArchetypeFlags::GTA_MAP_TYPE_WEAPON|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE))
		{
			Vector3 vWorldOffset = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(g_vOffset)));
			const Vector3 vThingPosition = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().GetPosition());
			Vector3 vForceDir = (Pos - ( vWorldOffset + vThingPosition));
			float fForce = MIN( vForceDir.Mag(), CPhysics::ms_pullDistMax );
			vForceDir.Normalize();
			fForce *= CPhysics::ms_pullForceMultiply * ((CPhysical*)g_pThingToPullAround.Get())->GetMass();
			vForceDir *= fForce;
			((CPhysical*)g_pThingToPullAround.Get())->ApplyExternalForce(vForceDir, vWorldOffset, g_iComponent);
			grcDebugDraw::Sphere( vWorldOffset + vThingPosition, 0.1f, Color32(1.0f, 0.0f, 0.0f) );
			grcDebugDraw::Line( vWorldOffset + vThingPosition, Pos, Color32(1.0f, 0.0f, 0.0f) );

			char buff[256];
			sprintf( buff, "0x%p", g_pThingToPullAround->GetCurrentPhysicsInst() );
			grcDebugDraw::Text( vThingPosition, Color32(1.0f, 1.0f, 1.0f), 0,0, buff );
		}
	}
	// RIGHT BUTTON PULLS THE PART OF THE OBJECT INITIALLY GRABBED TO A POSITION RELATIVE TO THE CAMERA
	else if( !(ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) && iButtonClicked == ioMouse::MOUSE_RIGHT )
	{
		if( g_pThingToPullAround && g_pThingToPullAround->GetCurrentPhysicsInst() && g_pThingToPullAround->GetCurrentPhysicsInst()->IsInLevel())
		{
			// Work out the position to aim for from where the mouse is pointing
			Vector3 vPos, vMouseNear, vMouseFar;
			CDebugScene::GetMousePointing( vMouseNear, vMouseFar );
			Vector3 vWorldOffset = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(g_vOffset)));

			// Shift and control move the object forward and backward
			static float fUpdatePerFrame = 1.0f;
			if (ioMapper::DebugKeyDown(KEY_SHIFT))
				fDistanceFromCamera += fUpdatePerFrame;
			else if (ioMapper::DebugKeyDown(KEY_CONTROL))
				fDistanceFromCamera -= fUpdatePerFrame;
			
			fDistanceFromCamera = MAX( fDistanceFromCamera, 5.0f );
			Vector3 vDirection = vMouseFar - vMouseNear;
			vDirection.Normalize();
			vDirection.Scale(fDistanceFromCamera);
			vPos = vDirection + vMouseNear;
	
			const Vector3 vThingPosition = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().GetPosition());
			Vector3 vForceDir = (vPos - ( vWorldOffset + vThingPosition));
			float fForce = MIN( vForceDir.Mag(), CPhysics::ms_pullDistMax );
			vForceDir.Normalize();
			fForce *= CPhysics::ms_pullForceMultiply * ((CPhysical*)g_pThingToPullAround.Get())->GetMass();
			vForceDir *= fForce;
			
			((CPhysical*)g_pThingToPullAround.Get())->ApplyExternalForce(vForceDir, vWorldOffset, g_iComponent);
			grcDebugDraw::Sphere( vWorldOffset + vThingPosition, 0.1f, Color32(0.0f, 0.0f, 1.0f) );
			grcDebugDraw::Line( vWorldOffset + vThingPosition, vPos, Color32(0.0f, 0.0f, 1.0f) );
		
		}
	}
	// MIDDLE BUTTON PULLS THE CENTRE OF THE OBJECT INITIALLY GRABBED TO A POSITION RELATIVE TO THE CAMERA
	else if( !(ioMouse::GetReleasedButtons() & ioMouse::MOUSE_MIDDLE) && iButtonClicked == ioMouse::MOUSE_MIDDLE  )
	{
		if( g_pThingToPullAround && g_pThingToPullAround->GetCurrentPhysicsInst() && g_pThingToPullAround->GetCurrentPhysicsInst()->IsInLevel())
		{
			CPhysical* pPhysical = ((CPhysical*)g_pThingToPullAround.Get());
			// Work out the position to aim for from where the mouse is pointing
			Vector3 vPos, vMouseNear, vMouseFar;
			CDebugScene::GetMousePointing( vMouseNear, vMouseFar );

			// Shift and control move the object forward and backward
			static float fUpdatePerFrame = 0.25f;
			if (ioMapper::DebugKeyDown(KEY_SHIFT))
				fDistanceFromCamera += fUpdatePerFrame;
			else if (ioMapper::DebugKeyDown(KEY_CONTROL))
				fDistanceFromCamera -= fUpdatePerFrame;

			fDistanceFromCamera = MAX( fDistanceFromCamera, 5.0f );
			Vector3 vDirection = vMouseFar - vMouseNear;
			vDirection.Normalize();
			vDirection.Scale(fDistanceFromCamera);
			vPos = vDirection + vMouseNear;
			// Apply the pulling force to the object
			Vector3 vForceDir = (vPos - ( VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().GetPosition()) ));
			float fForce = MIN( vForceDir.Mag(), CPhysics::ms_pullDistMax );
			vForceDir.Normalize();
			fForce *= CPhysics::ms_pullForceMultiply * pPhysical->GetMass();
			vForceDir *= fForce;
			pPhysical->ApplyForceCg(vForceDir);


			if (ioMapper::DebugKeyDown(KEY_C))
			{
				float fHeading = pPhysical->GetTransform().GetHeading();
				Matrix34 matNew = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
				matNew.Identity3x3();
				pPhysical->SetMatrix(matNew);
				pPhysical->SetHeading(fHeading);
			}
			else if( ioMapper::DebugKeyDown(KEY_S) )
			{
				pPhysical->SetHeading(0.0f);
			}
			else
			{
				// Apply torque if z or x are held
				float fTorque = 0.0f;
				if (ioMapper::DebugKeyDown(KEY_X))
					fTorque = 1.0f;
				else if (ioMapper::DebugKeyDown(KEY_Z))
					fTorque = -1.0f;

				if( fTorque != 0.0f )
				{
					if( pPhysical->GetIsTypePed() )
					{
						static float fPedRotationSpeed = PI/30.0f;
						CPed* pPed = (CPed*)pPhysical;
						pPed->SetDesiredHeading( pPed->GetCurrentHeading() + ( fPedRotationSpeed * fTorque ) );
						pPed->SetHeading( pPed->GetDesiredHeading() );
					}
					else
					{
						static float fTorqueMultiplier = 10.0f;
						fTorque *= fTorqueMultiplier * pPhysical->GetMass();
						pPhysical->ApplyTorque( fTorque*camInterface::GetRight(), camInterface::GetFront() );
					}
				}	
			}


			const Vector3 vThingPosition = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().GetPosition());
			grcDebugDraw::Sphere( vThingPosition, 0.1f, Color32(0.0f, 1.0f, 0.0f) );
			grcDebugDraw::Line( vThingPosition, vPos, Color32(0.0f, 1.0f, 0.0f) );
		}
	}
	else
	{		
		if( g_pThingToPullAround )
		{
#if __BANK
			const Vector3 vThingPosition = VEC3V_TO_VECTOR3(g_pThingToPullAround->GetTransform().GetPosition());

			static char debug_string[128];
			sprintf( debug_string, "Position of entity when released (%.2f, %.2f, %.2f) heading (%.2f)\n", 
				vThingPosition.x, vThingPosition.y, vThingPosition.z, 
				( RtoD * g_pThingToPullAround->GetTransform().GetHeading()) );

			Displayf( "%s", debug_string );

			FileHandle fileId;
			s32 temp_string_length;
			fileId = CFileMgr::OpenFileForAppending(CScriptDebug::GetNameOfScriptDebugOutputFile());
			if(CFileMgr::IsValidFileHandle(fileId))
			{
				temp_string_length = istrlen(debug_string);	//	debug_string);
				ASSERT_ONLY(s32 NumBytes =) CFileMgr::Write(fileId, (char *) debug_string, temp_string_length);
				Assertf(NumBytes > 0, "CPhysics::UpdatePullingThingsAround - Wrote 0 bytes to temp_debug.txt");
				CFileMgr::CloseFile(fileId);
			}	
#endif	//	__BANK
			g_pThingToPullAround = NULL;
		}
	}
}

bool CPhysics::ms_bMouseShooter= false;
bool CPhysics::ms_bAdvancedMouseShooter= false;
bool CPhysics::ms_bAttachedShooterMode= false;
bool CPhysics::ms_bOverrideMouseShooterWeapon = false;
s32 CPhysics::ms_iWeaponComboIndex = 0;


bool s_bRightClicked= false;
bool s_bLeftClicked= false;
bool s_bMiddleClicked= false;
Vector3 s_vPosition(0.0f, 0.0f, 0.0f);
Vector3 s_vOffset(0.0f, 0.0f, 0.0f);
Vector3 s_vOrientation(0.0f, 1.0f, 0.0f);
Vector3 s_vLocalOrientation(0.0f, 0.0f, 0.0f);
static float SHOT_LENGTH = 3.0f;
static float EXIT_WOUND_LENGTH = 2.0f;
static float FAST_ROTATION_SPEED = TWO_PI;
static float SLOW_ROTATION_SPEED = QUARTER_PI;
static RegdEnt g_pShootAttachedEntity(NULL);
static s32	g_iAttachedComponent = 0;

void CPhysics::UpdateMouseShooter()
{
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		s_bLeftClicked = true;
	}
	if( s_bLeftClicked &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT )
	{
		s_bLeftClicked = false;
	}

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		s_bRightClicked = true;
	}
	if( s_bRightClicked &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT )
	{
		s_bRightClicked = false;
	}
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_MIDDLE )
	{
		s_bMiddleClicked = true;
	}
	if( s_bMiddleClicked &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_MIDDLE )
	{
		s_bMiddleClicked = false;
	}

	if( ms_bAdvancedMouseShooter )
	{
		const float fRotationSpeed = ( ( s_bMiddleClicked || ioMapper::DebugKeyDown(KEY_SPACE) )? SLOW_ROTATION_SPEED : FAST_ROTATION_SPEED ) * fwTimer::GetTimeStep();
		if(ioMapper::DebugKeyDown(KEY_Z))
		{
			s_vOrientation.RotateZ(fRotationSpeed);
		}
		else if( ioMapper::DebugKeyDown(KEY_X) )
		{
			s_vOrientation.RotateZ(-fRotationSpeed);
		}

		if( ioMapper::DebugKeyDown(KEY_W) )
		{
			Vector3 vCross = s_vOrientation;
			vCross.Cross(ZAXIS);
			s_vOrientation.RotateX(vCross.x*fRotationSpeed);
			s_vOrientation.RotateY(vCross.y*fRotationSpeed);
		}
		else if( ioMapper::DebugKeyDown(KEY_S) )
		{
			Vector3 vCross = s_vOrientation;
			vCross.Cross(ZAXIS);
			s_vOrientation.RotateX(vCross.x*-fRotationSpeed);
			s_vOrientation.RotateY(vCross.y*-fRotationSpeed);
		}

		s_vOrientation.Normalize();

		if( s_bRightClicked )
		{
			Vector3 vWorldPos;
			if( CDebugScene::GetWorldPositionUnderMouse(vWorldPos, ArchetypeFlags::GTA_MAP_TYPE_WEAPON|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_RAGDOLL_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE) )
			{
				s_vPosition = vWorldPos;
			}
			if( g_pShootAttachedEntity )
			{
				g_pShootAttachedEntity = NULL;
			}
			if( ms_bAttachedShooterMode )
			{
				Vector3 vOffset;
				CEntity* pEntity = CDebugScene::GetEntityUnderMouse(&vOffset, &g_iAttachedComponent, ArchetypeFlags::GTA_MAP_TYPE_WEAPON|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_RAGDOLL_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
				if( pEntity )
				{
					g_pShootAttachedEntity = pEntity;
					if( pEntity->GetIsTypePed() )
					{
						CPed* pPed = (CPed*)pEntity;
						Matrix34 mBone;
						pPed->GetBoneMatrixFromRagdollComponent(mBone, g_iAttachedComponent);
						
						// Convert the offset into a local offset
						vOffset = vOffset - mBone.d;
						mBone.UnTransform3x3(vOffset);
						s_vOffset = vOffset;
						s_vLocalOrientation = s_vOrientation;
						mBone.UnTransform3x3(s_vLocalOrientation);
						s_vLocalOrientation.Normalize();
					}
					else
					{
						// Convert the offset into a local offset
						vOffset = vOffset - VEC3V_TO_VECTOR3(g_pShootAttachedEntity->GetTransform().GetPosition());
						vOffset = VEC3V_TO_VECTOR3(g_pShootAttachedEntity->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vOffset)));
						s_vOffset = vOffset;
						s_vLocalOrientation = VEC3V_TO_VECTOR3(g_pShootAttachedEntity->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(s_vOrientation)));
						s_vLocalOrientation.Normalize();
					}
				}
			}
		}

		if( g_pShootAttachedEntity && g_pShootAttachedEntity->GetIsTypePed() )
		{
			CPed* pPed = (CPed*)g_pShootAttachedEntity.Get();
			Matrix34 mBone;
			pPed->GetBoneMatrixFromRagdollComponent(mBone, g_iAttachedComponent);

			s_vPosition = s_vOffset;
			mBone.Transform3x3(s_vPosition);
			s_vPosition += mBone.d;
			s_vOrientation = s_vLocalOrientation;
			mBone.Transform3x3(s_vOrientation);
		}
		else if( g_pShootAttachedEntity )
		{
			s_vPosition = VEC3V_TO_VECTOR3(g_pShootAttachedEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(s_vOffset)));
			s_vPosition += VEC3V_TO_VECTOR3(g_pShootAttachedEntity->GetTransform().GetPosition());
			s_vOrientation = VEC3V_TO_VECTOR3(g_pShootAttachedEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(s_vLocalOrientation)));
		}

		if( s_bLeftClicked )
		{
			Vector3 TempCoors = s_vPosition - (s_vOrientation * SHOT_LENGTH);
			Vector3 NewCoors = s_vPosition + (s_vOrientation * 100.0f);

			//FireOneInstantHitRound(&TempCoors, &NewCoors, DamageCaused);
			u32 WEAPON_HASH = WEAPONTYPE_ASSAULTRIFLE;
			if( ms_bOverrideMouseShooterWeapon )
			{
				CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames( CWeaponInfoBlob::IT_Weapon );
				if( ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount() )
				{
					WEAPON_HASH = atStringHash(weaponNames[ms_iWeaponComboIndex]);
				}
			}

			CWeapon tempWeapon(WEAPON_HASH);

			Matrix34 tempMatrix;
			tempMatrix.Identity();
			tempMatrix.d = TempCoors;

			tempMatrix.b = NewCoors - TempCoors;
			tempMatrix.b.Normalize();

			tempMatrix.a.Cross(tempMatrix.b, Vector3(0.0f, 0.0f, 1.0f));
			tempMatrix.a.Normalize();

			tempMatrix.c.Cross(tempMatrix.a, tempMatrix.b);
			tempMatrix.c.Normalize();

			tempWeapon.Fire(CWeapon::sFireParams(NULL, tempMatrix, &TempCoors, &NewCoors));
		}

		grcDebugDraw::Line(s_vPosition - (s_vOrientation * SHOT_LENGTH), s_vPosition, Color32(0.0f, 1.0f, 0.0f) );
		grcDebugDraw::Line(s_vPosition + (s_vOrientation * EXIT_WOUND_LENGTH), s_vPosition, Color32(1.0f, 0.0f, 0.0f) );
	}
	else
	{
		if( s_bLeftClicked )
		{
			Vector3 TempCoors;
			Vector3 NewCoors;
			CDebugScene::GetMousePointing( TempCoors, NewCoors );

			//FireOneInstantHitRound(&TempCoors, &NewCoors, DamageCaused);
			u32 WEAPON_HASH = WEAPONTYPE_ASSAULTRIFLE;
			if( ms_bOverrideMouseShooterWeapon )
			{
				CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames( CWeaponInfoBlob::IT_Weapon );
				if( ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount() )
				{
					WEAPON_HASH = atStringHash(weaponNames[ms_iWeaponComboIndex]);
				}
			}

			CWeapon tempWeapon(WEAPON_HASH);

			Matrix34 tempMatrix;
			tempMatrix.Identity();
			tempMatrix.d = TempCoors;

			tempMatrix.b = NewCoors - TempCoors;
			tempMatrix.b.Normalize();

			tempMatrix.a.Cross(tempMatrix.b, Vector3(0.0f, 0.0f, 1.0f));
			tempMatrix.a.Normalize();

			tempMatrix.c.Cross(tempMatrix.a, tempMatrix.b);
			tempMatrix.c.Normalize();

			tempWeapon.Fire(CWeapon::sFireParams(NULL, tempMatrix, &TempCoors, &NewCoors));
		}
	}
}

void CPhysics::UpdateDebugPositionOne()
{
	Vector3 vPosFromText(Vector3::ZeroType);
	if( sscanf(ms_Pos1, "%f, %f, %f", &vPosFromText.x, &vPosFromText.y, &vPosFromText.z) == 3 )
	{
		g_vClickedPos[0] = vPosFromText;
		g_vClickedNormal[0] = Vector3(0.0f,0.0f,1.0f);
		g_pPointers[0] = NULL;
		g_bHasPosition[0] = true;
		sprintf(ms_Ptr1, "");
		sprintf(ms_Normal1, "");
	}
}

void CPhysics::UpdateDebugPositionTwo()
{
	Vector3 vPosFromText(Vector3::ZeroType);
	if( sscanf(ms_Pos2, "%f, %f, %f", &vPosFromText.x, &vPosFromText.y, &vPosFromText.z) == 3 )
	{
		g_vClickedPos[1] = vPosFromText;
		g_vClickedNormal[1] = Vector3(0.0f,0.0f,1.0f);
		g_pPointers[1] = NULL;
		g_bHasPosition[1] = true;
		sprintf(ms_Ptr2, "");
		sprintf(ms_Normal2, "");
	}
}

void ActivateFocusEntityCB()
{
	if(CEntity* pEntity = CDebugScene::FocusEntities_Get(0))
	{
		if(pEntity->GetIsPhysical())
		{
			static_cast<CPhysical*>(pEntity)->ActivatePhysics();
		}
	}
}

void ActivatePickedEntityCB()
{
	CEntity* pEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
	if (pEntity && pEntity->GetIsPhysical())
	{
		static_cast<CPhysical*>(pEntity)->ActivatePhysics();
	}
}

void ToggleFragDraw()
{
#if __PFDRAW
	PFDGROUP_Physics.SetEnabled(g_FragDrawToggle);
	PFD_ComponentIndices.SetEnabled(g_FragDrawToggle);

	if (g_FragDrawToggle)
	{
		gVpMan.ClearVisibilityFlagForAllPhases( VIS_ENTITY_MASK_OBJECT );
	}
	else
	{
		gVpMan.SetVisibilityFlagForAllPhases( VIS_ENTITY_MASK_OBJECT );
	}
#endif // __PFDRAW
}

#if __PFDRAW

void UpdateDrawCollisionOptions()
{
	bool bAnyMapFlagsOn =
		CPhysics::ms_bDrawMoverMapCollision|
		CPhysics::ms_bDrawWeaponMapCollision|
		CPhysics::ms_bDrawHorseMapCollision|
		CPhysics::ms_bDrawCoverMapCollision|
		CPhysics::ms_bDrawVehicleMapCollision|
		CPhysics::ms_bDrawStairsSlopeMapCollision;
	PFDGROUP_Physics.SetEnabled(bAnyMapFlagsOn|CPhysics::ms_bDrawDynamicCollision);
	PFD_Active.SetEnabled(CPhysics::ms_bDrawDynamicCollision);
	PFD_Inactive.SetEnabled(CPhysics::ms_bDrawDynamicCollision);
	PFD_Fixed.SetEnabled(bAnyMapFlagsOn);

	if(bAnyMapFlagsOn)
	{
		PFDGROUP_TypeFlagFilter.DisableChildren();

		PFD_TypeFlag1.SetEnabled(CPhysics::ms_bDrawWeaponMapCollision);
		PFD_TypeFlag2.SetEnabled(CPhysics::ms_bDrawMoverMapCollision);
		PFD_TypeFlag3.SetEnabled(CPhysics::ms_bDrawHorseMapCollision);
		PFD_TypeFlag4.SetEnabled(CPhysics::ms_bDrawCoverMapCollision);
		PFD_TypeFlag5.SetEnabled(CPhysics::ms_bDrawVehicleMapCollision);
		PFD_TypeFlag30.SetEnabled(CPhysics::ms_bDrawStairsSlopeMapCollision);
	}
	else
	{
		PFDGROUP_TypeFlagFilter.EnableChildren();
	}
}

void ToggleDrawMoverMapCollisionCB()
{
	UpdateDrawCollisionOptions();
}
#endif // __PFDRAW

void ToggleDrawWeaponMapCollisionCB()
{
#if __PFDRAW
	UpdateDrawCollisionOptions();
#endif // __PFDRAW
}

void ToggleDrawHorseMapCollisionCB()
{
#if __PFDRAW
	UpdateDrawCollisionOptions();
#endif // __PFDRAW
}

void ToggleDrawCoverMapCollisionCB()
{
#if __PFDRAW
	UpdateDrawCollisionOptions();
#endif // __PFDRAW
}

void ToggleDrawVehicleMapCollisionCB()
{
#if __PFDRAW
	UpdateDrawCollisionOptions();
#endif // __PFDRAW
}

void ToggleDrawStairsSlopeMapCollisionCB()
{
#if __PFDRAW
	UpdateDrawCollisionOptions();
#endif // __PFDRAW
}

void ToggleDrawDynamicCollisionCB()
{
#if __PFDRAW
	UpdateDrawCollisionOptions();
#endif // __PFDRAW
}

void ToggleDrawPolygonDensityCB()
{
#if __PFDRAW
	PFDGROUP_PolygonDensity.SetEnabled(CPhysics::ms_bDrawPolygonDensity);
	PFD_Solid.SetEnabled(CPhysics::ms_bDrawPrimitiveDensity || CPhysics::ms_bDrawPolygonDensity);
	PFDGROUP_Physics.SetEnabled(CPhysics::ms_bDrawPrimitiveDensity || CPhysics::ms_bDrawPolygonDensity);
	if (CPhysics::ms_bDrawPolygonDensity) 
	{
		bool bAnyMapFlagsOn =
			CPhysics::ms_bDrawMoverMapCollision|
			CPhysics::ms_bDrawWeaponMapCollision|
			CPhysics::ms_bDrawHorseMapCollision|
			CPhysics::ms_bDrawCoverMapCollision|
			CPhysics::ms_bDrawVehicleMapCollision|
			CPhysics::ms_bDrawStairsSlopeMapCollision;

		if (!bAnyMapFlagsOn)
		{
			CPhysics::ms_bDrawMoverMapCollision = true; // turn on mover collision draw if nothing else
			ToggleDrawMoverMapCollisionCB();
		}
	}
#endif // __PFDRAW
}

void ToggleDrawPedDensityCB()
{
	PGTAMATERIALMGR->GetDebugInterface().SetRenderPedDensity(CPhysics::ms_bDrawPedDensity);
}

void ToggleDrawPavementCB()
{
	PGTAMATERIALMGR->GetDebugInterface().SetRenderPavement(CPhysics::ms_bDrawPavement);
}

void ToggleDrawEdgeAnglesCB()
{
#if __PFDRAW
	PFD_EdgeAngles.SetEnabled(CPhysics::ms_bDrawEdgeAngles);
#endif // __PFDRAW
}

void ToggleDrawPrimitiveDensityCB()
{
#if __PFDRAW
	PFDGROUP_PrimitiveDensity.SetEnabled(CPhysics::ms_bDrawPrimitiveDensity);
	PFD_Solid.SetEnabled(CPhysics::ms_bDrawPrimitiveDensity || CPhysics::ms_bDrawPolygonDensity);
	PFDGROUP_Physics.SetEnabled(CPhysics::ms_bDrawPrimitiveDensity || CPhysics::ms_bDrawPolygonDensity);

	if (CPhysics::ms_bDrawPrimitiveDensity) 
	{
		bool bAnyMapFlagsOn =
			CPhysics::ms_bDrawMoverMapCollision|
			CPhysics::ms_bDrawWeaponMapCollision|
			CPhysics::ms_bDrawHorseMapCollision|
			CPhysics::ms_bDrawCoverMapCollision|
			CPhysics::ms_bDrawVehicleMapCollision|
			CPhysics::ms_bDrawStairsSlopeMapCollision;

		if (!bAnyMapFlagsOn)
		{
			CPhysics::ms_bDrawMoverMapCollision = true; // turn on mover collision draw if nothing else
			ToggleDrawMoverMapCollisionCB();
		}
	}
#endif // __PFDRAW
}

void ToggleIncludePolygonsCB()
{
#if __PFDRAW
	PFD_IncludePolygons.SetEnabled(CPhysics::ms_bIncludePolygons);
#endif // __PFDRAW
}

void ToggleDrawSelectedPhysicalCollisionRecordsCB()
{
#if __PFDRAW
	PFDGROUP_Physics.SetEnabled(CPhysics::ms_bDisplaySelectedPhysicalCollisionRecords);
#endif // __PFDRAW
}

void ToggleDrawAllCollisionRecordsCB()
{
#if __PFDRAW
	PFDGROUP_Physics.SetEnabled(CPhysics::ms_bDisplayAllCollisionRecords);
#endif // __PFDRAW
}

void FixAllCVehiclesCB()
{
	for (s32 vehicleIndex = 0; vehicleIndex < CVehicle::GetPool()->GetSize(); ++vehicleIndex)
	{
		if(CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(vehicleIndex))
		{
			pVehicle->Fix();
		}
	}
}

void FixAllCObjectsCB()
{
	for (s32 objectIndex = 0; objectIndex < CObject::GetPool()->GetSize(); ++objectIndex)
	{
		if(CObject* pObject = CObject::GetPool()->GetSlot(objectIndex))
		{
			pObject->Fix();
		}
	}
}

void FixAllCPhysicalsCB()
{
	FixAllCObjectsCB();
	FixAllCVehiclesCB();
}

void CPhysics::UpdateEntityFromFrag(void* userData)
{
	if (userData)
	{
		CEntity* pEntity = static_cast<CEntity*>(userData);
		if (pEntity->GetIsPhysical())
		{
			static_cast<CPhysical*>(pEntity)->UpdateEntityFromPhysics(pEntity->GetCurrentPhysicsInst(), 0);
		}
	}
}

void CPhysics::MarkEntityDirty(void* userData)
{
	if (CEntity* pEntity = static_cast<CEntity*>(userData))
	{
		pEntity->GetBaseModelInfo()->SetDrawableDependenciesAsDirty();
	}
}

#if WORLD_PROBE_DEBUG
void CPhysics::SetAllWorldProbeContextDrawEnabled()
{
	WorldProbe::CShapeTestManager* pShapeTestMgr = WorldProbe::GetShapeTestManager();
	for(int i = 0 ; i < WorldProbe::LOS_NumContexts; ++i)
	{
		pShapeTestMgr->ms_abContextFilters[i] = true;
	}
}

void CPhysics::SetAllWorldProbeContextDrawDisabled()
{
	WorldProbe::CShapeTestManager* pShapeTestMgr = WorldProbe::GetShapeTestManager();
	for(int i = 0 ; i < WorldProbe::LOS_NumContexts; ++i)
	{
		pShapeTestMgr->ms_abContextFilters[i] = false;
	}
}
#endif // WORLD_PROBE_DEBUG

bkBank *CPhysics::ms_pPhysicsBank = NULL;
bkButton *CPhysics::ms_pCreatePhysicsBankButton = NULL;

bkBank *CPhysics::ms_pFragTuneBank = NULL;
bkButton *CPhysics::ms_pCreateFragTuneBankButton = NULL;

int GetLargestBitIndex(u32 bits)
{
	int index = 0;
	while((bits >>= 1) > 0) ++index;
	return index;
}

void CPhysics::InitWidgets()
{
	bkBank *pBankOptimize = BANKMGR.FindBank("Optimization");

	pBankOptimize->AddToggle("Force Dummy Objects",			&ms_bForceDummyObjects);
	pBankOptimize->AddToggle("Draw map collision (MOVER)",	&ms_bDrawMoverMapCollision, ToggleDrawMoverMapCollisionCB);
	pBankOptimize->AddToggle("Draw map collision (WEAPON)",	&ms_bDrawWeaponMapCollision, ToggleDrawWeaponMapCollisionCB);
	pBankOptimize->AddToggle("Draw map collision (HORSE)",	&ms_bDrawHorseMapCollision, ToggleDrawHorseMapCollisionCB);
	pBankOptimize->AddToggle("Draw map collision (COVER)",	&ms_bDrawCoverMapCollision, ToggleDrawCoverMapCollisionCB);
	pBankOptimize->AddToggle("Draw map collision (VEHICLE)",&ms_bDrawVehicleMapCollision, ToggleDrawVehicleMapCollisionCB);
	pBankOptimize->AddToggle("Draw map collision (STAIRS SLOPE)",	&ms_bDrawStairsSlopeMapCollision, ToggleDrawStairsSlopeMapCollisionCB);
	pBankOptimize->AddToggle("Draw dynamic collision",		&ms_bDrawDynamicCollision, ToggleDrawDynamicCollisionCB);
	pBankOptimize->AddToggle("Draw pavement",				&ms_bDrawPavement, ToggleDrawPavementCB);
	pBankOptimize->AddToggle("Draw ped density",			&ms_bDrawPedDensity, ToggleDrawPedDensityCB);
#if __PFDRAW
	pBankOptimize->PushGroup("Polygon Density",false);
	pBankOptimize->AddToggle("Draw polygon density",		&ms_bDrawPolygonDensity, ToggleDrawPolygonDensityCB);
	pBankOptimize->AddToggle("Draw edge angles",			&ms_bDrawEdgeAngles, ToggleDrawEdgeAnglesCB);

	pBankOptimize->AddTitle("Rage Profile Draw Aliases");
	{
		PFD_Solid.AddWidgets(*pBankOptimize);
		PFD_Wireframe.AddWidgets(*pBankOptimize);
		PFD_ColorMidPoint.AddWidgets(*pBankOptimize);
		PFD_ColorExpBias.AddWidgets(*pBankOptimize);
		PFD_PolygonAngleDensity.AddWidgets(*pBankOptimize);
		PFD_NumRecursions.AddWidgets(*pBankOptimize);
		PFD_MaxNeighborAngleEnabled.AddWidgets(*pBankOptimize);
		PFD_MaxNeighborAngle.AddWidgets(*pBankOptimize);
	}

	pBankOptimize->AddSeparator();

	phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_MOVER)] = 3.5f;
	phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_WEAPON)] = 15.0f;
	phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_HORSE)] = 3.5f;
	phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_COVER)] = 3.5f;
	phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE)] = 3.5f;
	pBankOptimize->AddSlider("Rating Multiplier (MOVER)",	&phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_MOVER)], 0.1f, 20.0f, 0.1f);
	pBankOptimize->AddSlider("Rating Multiplier (WEAPON)",	&phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_WEAPON)], 0.1f, 20.0f, 0.1f);
	pBankOptimize->AddSlider("Rating Multiplier (HORSE)",	&phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_HORSE)], 0.1f, 20.0f, 0.1f);
	pBankOptimize->AddSlider("Rating Multiplier (COVER)",	&phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_COVER)], 0.1f, 20.0f, 0.1f);
	pBankOptimize->AddSlider("Rating Multiplier (VEHICLE)",	&phBoundPolyhedron::sm_TypeFlagDensityScaling[GetLargestBitIndex(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE)], 0.1f, 20.0f, 0.1f);
	pBankOptimize->PopGroup();

	pBankOptimize->PushGroup("Primitive Density",false);
	pBankOptimize->AddToggle("Draw Primitive Density",		&ms_bDrawPrimitiveDensity, ToggleDrawPrimitiveDensityCB);
	pBankOptimize->AddToggle("Include Include Polygons",	&ms_bIncludePolygons, ToggleIncludePolygonsCB);
	pBankOptimize->PopGroup();

#endif // __PFDRAW
	pBankOptimize->AddToggle("Toggle AllowVehs",			&gbAllowVehGenerationOrRemoval);
	pBankOptimize->AddToggle("Toggle AllowPeds",			&gbAllowPedGeneration);
	pBankOptimize->AddToggle("Toggle AllowAmbientPeds",		&gbAllowAmbientPedGeneration);
	pBankOptimize->AddToggle("Toggle AllowScriptBrains",	&gbAllowScriptBrains);

	ms_pPhysicsBank = &BANKMGR.CreateBank("Physics");

	if(physicsVerifyf(ms_pPhysicsBank, "Failed to create Physics bank"))
	{
		ms_pCreatePhysicsBankButton = ms_pPhysicsBank->AddButton("Create physics widgets", &CPhysics::CreatePhysicsBank);
	}

	ms_pFragTuneBank = &BANKMGR.CreateBank("Rage Frag Tune");

	if(physicsVerifyf(ms_pFragTuneBank, "Failed to create Rage Frag Tune bank"))
	{
		ms_pCreateFragTuneBankButton = ms_pFragTuneBank->AddButton("Create fragment tune widgets", &CPhysics::CreateFragTuneBank);
	}

	fragTuneStruct::UpdateEntityFunctor updateEntityFunctor;
	updateEntityFunctor.Bind(&CPhysics::UpdateEntityFromFrag);
	FRAGTUNE->SetUpdateEntityFunctor(updateEntityFunctor);

	fragTuneStruct::SelectEntityFunctor selectEntityFunctor;
	selectEntityFunctor.Bind(&CPhysics::MarkEntityDirty);
	FRAGTUNE->SetSelectEntityFunctor(selectEntityFunctor);
}

bool gbVehicleImpulseModification = true;

void CPhysics::DisplayNumPartsBrokenOffFocusFrag()
{
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if(!pEnt)
		return;

	fragInst* pFragInst = pEnt->GetFragInst();
	if(!pFragInst)
		return;

	int nNumBrokenOffParts = 0;
	int nTotalNumParts = pFragInst->GetTypePhysics()->GetNumChildren();
	for(int nChild = 0; nChild < nTotalNumParts; ++nChild)
	{
		if(pFragInst->GetChildBroken(nChild))
			++nNumBrokenOffParts;
	}

	char zText[255];
	sprintf(zText, "num broken parts/total: %d/%d", nNumBrokenOffParts, nTotalNumParts);
	Color32 colour = Color_green;
	if(pFragInst->GetCached() && !pFragInst->GetCacheEntry()->IsFurtherBreakable())
		colour = Color_red;
	grcDebugDraw::Text(VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition())+Vector3(0.0f,0.0f,1.0f), colour, zText);
}

void DisplayRagdollDamageInfoForThisPed(const CPed* pPed)
{
	Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())+Vector3(0.0f,0.0f,1.0f);
	u32 nNoOfLines = 0;
	char zText[256];
	Color32 colour = pPed->IsDead() ? Color_red : Color_green;
	colour.SetAlpha(180);

	if(CPhysics::ms_bDisplayPedHealth)
	{
		sprintf(zText, "Health: %f", pPed->GetHealth());
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;
	}

	if(CPhysics::ms_bDisplayPedFallHeight)
	{
		sprintf(zText, "Fall height: %f", pPed->GetFallingHeight());
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;
	}
}

void CPhysics::DisplayRagdollDamageInfo()
{
	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(!pLocalPlayer)
		return;

	DisplayRagdollDamageInfoForThisPed(pLocalPlayer);

	CEntityScannerIterator entityList = pLocalPlayer->GetPedIntelligence()->GetNearbyPeds();
	CEntity* pEnt = entityList.GetFirst();
	while(pEnt)
	{
		physicsAssert(pEnt->GetIsTypePed());
		CPed* pPed = static_cast<CPed*>(pEnt);

		DisplayRagdollDamageInfoForThisPed(pPed);

		// Advance to the next nearby ped.
		pEnt = entityList.GetNext();
	}
}

bool noBreaking = true;

void CPhysics::ToggleNoPopups()
{
	if (g_NoPopups)
	{
		PARAM_nopopups.Set("");
	}
	else
	{
		PARAM_nopopups.Set(NULL);
	}
}

void CPhysics::TogglePauseUnpauseGame()
{
	if (g_PauseUnpauseGame)
	{
		fwTimer::StartUserPause(true);
	}
	else
	{
		fwTimer::EndUserPause();
	}
}

void CPhysics::CreatePhysicsBank()
{
	Assertf(ms_pPhysicsBank, "Physics bank needs to be created first");

	if(ms_pCreatePhysicsBankButton)//delete the create bank button
	{
		ms_pCreatePhysicsBankButton->Destroy();
		ms_pCreatePhysicsBankButton = NULL;
	}
	else
	{
		//bank must already be setup as the create button doesn't exist so just return.
		return;
	}

	ms_pPhysicsBank->AddToggle("Toggle NoPopups", &g_NoPopups, datCallback(CFA(CPhysics::ToggleNoPopups)), "Toggle nopopups" );
	if( !PARAM_nopopups.Get() )
	{
		g_NoPopups = false;
	}
	else
	{
		g_NoPopups = true;
	}

	ms_pPhysicsBank->AddToggle("Toggle Pause/Unpause Game", &g_PauseUnpauseGame, datCallback(CFA(CPhysics::TogglePauseUnpauseGame)), "Toggle pause/unpause game");

	ms_pPhysicsBank->PushGroup("Performance tests");
		ms_pPhysicsBank->AddButton("Switch player to ragdoll", SwitchPlayerToRagDollCB);
		ms_pPhysicsBank->AddText("Crate Stack",ms_stackModelName, sizeof(ms_stackModelName), false, &CreateStackCB);
		//pBank->AddButton("Crate Stack", CreateStackCB);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Rage cloth");
		ms_pPhysicsBank->AddButton("Attach flag to vehicle", AttachFlagToVehicleCB);
		ms_pPhysicsBank->AddButton("Attach cape to char", AttachCapeToCharCB);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Foliage");
		ms_pPhysicsBank->AddToggle("Show foliage impacts for peds", &CPhysics::ms_bShowFoliageImpactsPeds);
		ms_pPhysicsBank->AddToggle("Show foliage impacts for ragdolls", &CPhysics::ms_bShowFoliageImpactsRagdolls);
		ms_pPhysicsBank->AddToggle("Show foliage impacts for vehicles", &CPhysics::ms_bShowFoliageImpactsVehicles);
		ms_pPhysicsBank->AddSlider("Min vehicle speed to apply foliage drag", &CVehicle::ms_fMinVehicleSpeedForFoliageDrag, 0.0f, 100.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Max angular accel induced by foliage drag", &CVehicle::ms_fMaxAngAccel, 0.0, 100.0f, 0.01f);
		ms_pPhysicsBank->AddSlider("Vehicle drag coefficient", &CVehicle::ms_fFoliageVehicleDragCoeff, 0.0f, 20.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Vehicle drag distance scaling coeff", &CVehicle::ms_fFoliageDragDistanceScaleCoeff, 0.0f, 10.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Vehicle drag bound radius scaling coeff", &CVehicle::ms_fFoliageBoundRadiusScaleCoeff, 0.0f, 100.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Vehicle door force scale coeff", &CVehicle::ms_fFoliageDoorForceScaleCoeff, 0.0f, 20.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Vehicle door max accel", &CVehicle::ms_fFoliageMaxDoorAccel, 0.0f, 100.0f, 0.01f);
		ms_pPhysicsBank->AddSlider("Bike drag coefficient", &CBike::ms_fFoliageBikeDragCoeff, 0.0f, 20.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Bike drag distance scaling coeff", &CBike::ms_fFoliageBikeDragDistanceScaleCoeff, 0.0f, 10.0f, 0.1f);
		ms_pPhysicsBank->AddSlider("Bike drag bound radius scaling coeff", &CBike::ms_fFoliageBikeBoundRadiusScaleCoeff, 0.0f, 100.0f, 0.1f);
	ms_pPhysicsBank->PopGroup(); // "Foliage"

	ms_pPhysicsBank->PushGroup("Gravity");
		const char* strGravLevelNames[] =
		{
			"Earth",
			"Moon",
			"Low",
			"ZeroG"
		};
		CompileTimeAssert(NUM_GRAVITY_LEVELS == 4);

		ms_pPhysicsBank->AddCombo("Gravity level",&ms_iSelectedGravLevel,NUM_GRAVITY_LEVELS,strGravLevelNames,SetGravityLevelCB);
		ms_pPhysicsBank->AddSlider("Gravity/ ms^-2",  &ms_fGravitationalAccleration, 0.0f, 20.0f, 0.1f, &SetGravitationalAccelerationCB, "Change the grav. accel.");
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Debug",false);
		ms_pPhysicsBank->AddToggle("Physics counters", &gbPhysicsInstCounters);
		ms_pPhysicsBank->AddToggle("Print active", &gbPrintPhysicsActive);
		ms_pPhysicsBank->AddToggle("Print inactive", &gbPrintPhysicsInActive);
		ms_pPhysicsBank->AddToggle("Print fixed", &gbPrintPhysicsFixed);
		ms_pPhysicsBank->AddToggle("Display insts on vectormap", &gbDisplayVectorMapPhysicsInsts);
//		ms_pPhysicsBank->AddToggle("Force dummy objects", &ms_bForceDummyObjects);

//		ms_pPhysicsBank->AddToggle("Do Composite Octree Cull", &phBoundComposite::ms_bDoOctreePreCull);
//		ms_pPhysicsBank->AddToggle("Do Composite Whole BBox Test", &phBoundComposite::ms_bDoWholeBBoxTest);
//		ms_pPhysicsBank->AddToggle("Do Composite Indv BBox Test", &phBoundComposite::ms_bDoIndividualBBoxTest);
//		ms_pPhysicsBank->AddToggle("Do Level Probe BBox Test", &phLevelNew::ms_bDoTestProbeBBoxTest);
//		ms_pPhysicsBank->AddToggle("Do PolyBound PolyBBox Test", &phBoundPolyhedron::ms_bDoPolyBBoxCull);

		ms_pPhysicsBank->AddToggle("Do Vehicle Damage", &ms_bDoVehicleDamage);
		ms_pPhysicsBank->AddToggle("Do Collision Effects", &ms_bDoCollisionEffects);
		ms_pPhysicsBank->AddToggle("Do Weapon Impact Effects", &ms_bDoWeaponImpactEffects);
#if __DEV
		ms_pPhysicsBank->AddToggle("Do Ped Probes With Impacts", &ms_bDoPedProbesWithImpacts);
#endif
		ms_pPhysicsBank->AddButton("Test Bound Performance", TestBoundPerformaceCB);
#if __DEV
		ms_pPhysicsBank->AddToggle("Switch Ped to NM Ragdoll", &sbSwitchPedToNM);
#endif
		ms_pPhysicsBank->AddToggle("Vehicle impact mods", &gbVehicleImpulseModification);
#if __PFDRAW
		ms_pPhysicsBank->AddCombo("Synchronize render and update threads", &snPhysicsSynchronizeRenderAndUpdateThreads, PHYSICS_RENDER_UPDATE_SYNC_POINT_COUNT, PHYSICS_RENDER_UPDATE_SYNC_POINT_STRINGS);
#endif
	ms_pPhysicsBank->PopGroup();

	PGTAMATERIALMGR->GetDebugInterface().InitWidgets(*ms_pPhysicsBank);
	if (PGTAMATERIALMGR->GetDebugInterface().GetProcObjComboNeedsUpdate())
	{
		PGTAMATERIALMGR->GetDebugInterface().UpdateProcObjComboBox();
	}

	// standard rage physics widgets
	ms_pPhysicsBank->PushGroup("Level",false);
		phLevelNew::AddWidgets(*ms_pPhysicsBank);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Simulator",false);
		phSimulator::AddWidgets(*ms_pPhysicsBank);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Fragments", false);
		ms_pPhysicsBank->AddToggle("noBreaking",&noBreaking);
		ms_pPhysicsBank->AddToggle("fragManager::Update each SimUpdate",&CPhysics::ms_bFragManagerUpdateEachSimUpdate);
		ms_pPhysicsBank->AddButton("Fix all CPhysicals",&FixAllCPhysicalsCB);
		ms_pPhysicsBank->AddButton("Fix all CVehicles",&FixAllCVehiclesCB);
		ms_pPhysicsBank->AddButton("Fix all CObjects",&FixAllCObjectsCB);
		FRAGMGR->AddWidgets(*ms_pPhysicsBank);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Materials",false);
		MATERIALMGR.AddWidgets(*ms_pPhysicsBank);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("Breakable glass", false);
		bgGlassManager::AddWidgets(*ms_pPhysicsBank);
	ms_pPhysicsBank->PopGroup();

	PDR_ONLY(debugPlayback::AddWidgets(*ms_pPhysicsBank));

	ms_pPhysicsBank->AddToggle("Override number of Timeslices", &CPhysics::ms_OverriderNumTimeSlices);
	ms_pPhysicsBank->AddSlider("GTA Physics Timeslices", &CPhysics::ms_NumTimeSlices, 1, 8, 1);
	ms_pPhysicsBank->AddToggle("Only push on last update", &CPhysics::ms_OnlyPushOnLastUpdate);

#if __DEV
	CPedModelInfo::InitPedBuoyancyWidget();
#endif

#if __BANK
	// Setup Explosion widgets
	if(CExplosionManager::ms_needToSetupRagWidgets)
		CExplosionManager::AddWidgets();
	else
		CExplosionManager::ms_needToSetupRagWidgets = true;
#endif

#if USE_FRAG_TUNE

	ms_pPhysicsBank->PushGroup("Ragdolls");
	ms_pPhysicsBank->AddSlider("Extra Allowed Penetration", &fragInstNM::sm_ExtraAllowedRagdollPenetration, 0.0f, 1.0f, 0.01f);
	ms_pPhysicsBank->AddToggle("Prevent Push Collisions", &g_PreventRagdollPushCollisions);
	ms_pPhysicsBank->AddToggle("Manual ragdoll skeleton update",&fragInstNM::ms_UseManualSkeletonUpdate);
	ms_pPhysicsBank->PushGroup("Fix Stuck Ragdolls");
	ms_pPhysicsBank->AddToggle("Enable Fix Stuck Ragdolls", &g_FixRagdollStuckInGeometry);
	ms_pPhysicsBank->AddToggle("Disregard min depth for fix stuck ragdolls", &CPed::ms_bAlwaysFixStuckInGeometry);
	ms_pPhysicsBank->AddSlider("Min depth for fix stuck ragdolls",&CPed::ms_fMinDepthForFixStuckInGeometry,0.0f,1.0f,0.01f);
	ms_pPhysicsBank->AddToggle("Element Match", &g_FixRagdollStuckInGeometryElementMatch);
	ms_pPhysicsBank->AddSlider("Fake Depth", &g_FixRagdollStuckInGeometryDepth, 0.0f, 1.0f, 0.01f);
	ms_pPhysicsBank->PopGroup();
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->PushGroup("GTA Frag Tune",false);

		ms_pPhysicsBank->AddSlider("No. Local Frags", &numFragNames, 1, 256, 0);
		ms_pPhysicsBank->AddButton("Update Local Frags", UpdateFragList);

		fragNames.Reset();
		fragNames.PushAndGrow("Inactive");
		fragNames.PushAndGrow("Activate");

		pFragCombo = ms_pPhysicsBank->AddCombo("Local Frag Types", &currFragNameSelection, 2, &fragNames[0], UpdateFragList);
	
		ms_pPhysicsBank->AddButton("Add Fragtune Widget", AddFragTuneWidgetCB);
	ms_pPhysicsBank->PopGroup();

	ms_pPhysicsBank->AddToggle( "Pull things around", &CPhysics::ms_bDebugPullThingsAround );
	ms_pPhysicsBank->AddSlider( "Pull Force Multiply", &CPhysics::ms_pullForceMultiply, 0.0f, 10000.0f, 1.0f);
	ms_pPhysicsBank->AddSlider( "Pull Dist Max", &CPhysics::ms_pullDistMax, 0.0f, 10000.0f, 1.0f);

	ms_pPhysicsBank->PushGroup("Physics debug tools",false);
		ms_pPhysicsBank->PushGroup("Mouse input",false);
			phMouseInput::AddClassWidgets(*ms_pPhysicsBank);
			phMouseInput::SetBoundFlagNameFunc(ArchetypeFlags::GetBoundFlagName);
		ms_pPhysicsBank->PopGroup();		
		ms_pPhysicsBank->PushGroup("Point and click gun/footsteps",false);
			ms_pPhysicsBank->AddToggle( "Enable gun", &CPhysics::ms_bMouseShooter );
			ms_pPhysicsBank->AddToggle( "Advanced mode", &CPhysics::ms_bAdvancedMouseShooter );
			ms_pPhysicsBank->AddToggle( "Attach to entities", &CPhysics::ms_bAttachedShooterMode );
			ms_pPhysicsBank->AddToggle( "Override Weapon", &CPhysics::ms_bOverrideMouseShooterWeapon );
			CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
			if(weaponNames.GetCount() > 0)
			{
				ms_pPhysicsBank->AddCombo( "Weapon", &CPhysics::ms_iWeaponComboIndex, weaponNames.GetCount(), &weaponNames[0]);
			}
			ms_pPhysicsBank->AddToggle( "Test Mover Collision",&CBullet::ms_CollideWithMoverCollision);
			ms_pPhysicsBank->AddToggle( "Enable footstep mouse triggering", &audPedFootStepAudio::sm_bMouseFootstep );
			ms_pPhysicsBank->AddToggle( "Do both, bullet and footstep", &audPedFootStepAudio::sm_bMouseFootstepBullet );
		ms_pPhysicsBank->PopGroup();
		ms_pPhysicsBank->PushGroup("Measuring Tool",false);
			ms_pPhysicsBank->AddToggle( "Turn on tool", &CPhysics::ms_bDebugMeasuringTool );
			ms_pPhysicsBank->AddText("Pos1", CPhysics::ms_Pos1, sizeof(CPhysics::ms_Pos1), false, UpdatePos1);
			ms_pPhysicsBank->AddText("Ptr1", CPhysics::ms_Ptr1, sizeof(CPhysics::ms_Ptr1), false);
			ms_pPhysicsBank->AddText("Pos2", CPhysics::ms_Pos2, sizeof(CPhysics::ms_Pos2), false, UpdatePos2);
			ms_pPhysicsBank->AddText("Ptr2", CPhysics::ms_Ptr2, sizeof(CPhysics::ms_Ptr2), false);
			ms_pPhysicsBank->AddText("Diff", CPhysics::ms_Diff, sizeof(CPhysics::ms_Diff), false);
			ms_pPhysicsBank->AddText("HeadingBetween (Radians)", CPhysics::ms_HeadingDiffRadians, sizeof(CPhysics::ms_HeadingDiffRadians), false);
			ms_pPhysicsBank->AddText("HeadingBetween (Degrees)", CPhysics::ms_HeadingDiffDegrees, sizeof(CPhysics::ms_HeadingDiffDegrees), false);
			ms_pPhysicsBank->AddText("Distance", CPhysics::ms_Distance, sizeof(CPhysics::ms_Distance), false);
			ms_pPhysicsBank->AddText("Horizontal dist", CPhysics::ms_HorizDistance, sizeof(CPhysics::ms_HorizDistance), false);
			ms_pPhysicsBank->AddText("Vertical dist", CPhysics::ms_VerticalDistance, sizeof(CPhysics::ms_VerticalDistance), false);
			ms_pPhysicsBank->AddText("Normal1", CPhysics::ms_Normal1, sizeof(CPhysics::ms_Normal1), false, UpdateNormal1);
			ms_pPhysicsBank->AddText("Normal2", CPhysics::ms_Normal2, sizeof(CPhysics::ms_Normal2), false, UpdateNormal2);

			ms_pPhysicsBank->PushGroup("Physics tests",false);
				ms_pPhysicsBank->PushGroup("Script command tests");
					ms_pPhysicsBank->AddToggle("BRING_VEHICLE_TO_HALT at Pos1", &CPhysics::ms_bDoBringVehicleToHaltTest);
				ms_pPhysicsBank->PopGroup(); // "Script command tests"

				ms_pPhysicsBank->PushGroup("Shape tests");
					ms_pPhysicsBank->PushGroup("LOS Ignore options");
						ms_pPhysicsBank->AddToggle("Ignore see thru",&CPhysics::ms_bLOSIgnoreSeeThru);
						ms_pPhysicsBank->AddToggle("Ignore shoot thru",&CPhysics::ms_bLOSIgnoreShootThru);
						ms_pPhysicsBank->AddToggle("Ignore shoot thru FX",&CPhysics::ms_bLOSIgnoreShootThruFX);
						ms_pPhysicsBank->AddToggle("Shoot thru glass",&CPhysics::ms_bLOSGoThroughGlass);
					ms_pPhysicsBank->PopGroup();
					ms_pPhysicsBank->AddButton( "Add focus entity to exclusion list (new API only)", AddFocusEntityToExclusionList);
					ms_pPhysicsBank->AddText  ( "Excluded entity list:", CPhysics::ms_zExcludedEntityList, sizeof(CPhysics::ms_zExcludedEntityList), true);
					ms_pPhysicsBank->AddButton( "Clear entity exclusion list", ClearEntityExclusionList);
					ms_pPhysicsBank->AddToggle( "Los test", &CPhysics::ms_bDoLineProbeTest );
					ms_pPhysicsBank->AddToggle( "ASync LOS test", &CPhysics::ms_bDoAsyncLOSTest);
					ms_pPhysicsBank->AddToggle( "Probe for water (synchronous)", &CPhysics::ms_bDoLineProbeWaterTest );
					ms_pPhysicsBank->AddToggle( "capsule test", &CPhysics::ms_bDoCapsuleTest );
					ms_pPhysicsBank->AddToggle( "bounding box test", &CPhysics::ms_bDoBoundingBoxTest );
					ms_pPhysicsBank->AddToggle( "ASync swept-sphere test", &CPhysics::ms_bDoAsyncSweptSphereTest );
					ms_pPhysicsBank->AddToggle( "tapered capsule batch test", &CPhysics::ms_bDoTaperedCapsuleBatchShapeTest);
					ms_pPhysicsBank->AddToggle( "capsule shape batch test", &CPhysics::ms_bDoCapsuleBatchShapeTest );
					ms_pPhysicsBank->AddToggle( "probe batch shape test", &CPhysics::ms_bDoProbeBatchShapeTest );
					ms_pPhysicsBank->AddToggle( "test capsule against focus", &CPhysics::ms_bTestCapsuleAgainstFocusEntity );
					ms_pPhysicsBank->AddToggle( "test capsule against focus bboxes", &CPhysics::ms_bTestCapsuleAgainstFocusEntityBBoxes );
					ms_pPhysicsBank->AddToggle( "do boolean test", &CPhysics::ms_bDoBooleanTest);

					ms_pPhysicsBank->AddSlider( "No. tests", &CPhysics::ms_iNumInstances, 1, 128, 1);
					ms_pPhysicsBank->AddSlider( "test width", &CPhysics::ms_fWidth, 0.0f, 10000.0f, 0.5f);
					ms_pPhysicsBank->AddSlider( "capsule width", &CPhysics::ms_fCapsuleWidth, 0.0f, 10000.0f, 0.5f);
					ms_pPhysicsBank->AddSlider( "capsule width 2", &CPhysics::ms_fCapsuleWidth2, 0.0f, 10000.0f, 0.5f);
					ms_pPhysicsBank->AddSlider( "bounding box test width", &CPhysics::ms_fBoundingBoxWidth, 0.0f, 10000.0f, 0.5f);
					ms_pPhysicsBank->AddSlider( "bounding box test length", &CPhysics::ms_fBoundingBoxLength, 0.0f, 10000.0f, 0.5f);
					ms_pPhysicsBank->AddSlider( "bounding box test height", &CPhysics::ms_fBoundingBoxHeight, 0.0f, 10000.0f, 0.5f);
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
					ms_pPhysicsBank->AddSlider( "second surface interp", &CPhysics::ms_fSecondSurfaceInterp, 0.0f,1.0f,0.001f);
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
					ms_pPhysicsBank->AddToggle( "Player entity overlap on selected test", &CPhysics::ms_bDoPlayerEntityTest);
					ms_pPhysicsBank->AddSlider( "Isec num tapered capsule", &ms_iNumIsecsTaperedCapsule,1,128,1);
					ms_pPhysicsBank->AddText  ( "Overlap test result",CPhysics::ms_PlayerEntityTestResult,sizeof(CPhysics::ms_PlayerEntityTestResult),false);
					ms_pPhysicsBank->AddToggle( "Do initial sphere check for capsule test",&CPhysics::ms_bDoInitialSphereTestForCapsuleTest);
					ms_pPhysicsBank->AddToggle( "sphere test", &CPhysics::ms_bDoSphereTest );
					ms_pPhysicsBank->AddSlider( "sphere radius", &CPhysics::ms_fSphereRadius, 0.0f, 10000.0f, 0.01f );
					ms_pPhysicsBank->AddToggle( "sphere test ignore fixed collision", &CPhysics::ms_bDoSphereTestNotFixed );
				ms_pPhysicsBank->PopGroup(); // "Shape tests"
			ms_pPhysicsBank->PopGroup();

		ms_pPhysicsBank->PopGroup();

		ms_pPhysicsBank->PushGroup("Ped and vehicle history (need to run with USE_PHYSICAL_HISTORY defined)", false);
			ms_pPhysicsBank->AddToggle( "Render all histories", &CPhysics::ms_bRenderAllHistories);
			ms_pPhysicsBank->AddToggle( "Render focus histories", &CPhysics::ms_bRenderFocusHistory);
		ms_pPhysicsBank->PopGroup();

		ms_pPhysicsBank->AddButton("Activate focus entity", ActivateFocusEntityCB);
		ms_pPhysicsBank->AddButton("Activate picked entity", ActivatePickedEntityCB);
		ms_pPhysicsBank->AddToggle("Display number of broken parts on focus frag", &CPhysics::ms_bDisplayNumberOfBrokenPartsFocusFrag);
		ms_pPhysicsBank->AddToggle("Display number of available ragdolls", &CPhysics::ms_bDebugRagdollCount);
		ms_pPhysicsBank->AddToggle("Render focus entity external forces", &CPhysics::ms_bRenderFocusImpulses);

		ms_pPhysicsBank->PushGroup("Collision records");
			ms_pPhysicsBank->AddToggle("Display collision record contacts for all active CPhysicals", &CPhysics::ms_bDisplayAllCollisionRecords, ToggleDrawAllCollisionRecordsCB);
			ms_pPhysicsBank->AddToggle("Display collision records for selected CPhysical", &CPhysics::ms_bDisplaySelectedPhysicalCollisionRecords, ToggleDrawSelectedPhysicalCollisionRecordsCB);
			ms_pPhysicsBank->AddSlider("Contact radius", &CCollisionHistory::ms_fDebugRenderContactRadius, 0.0f, 100.0f, 0.001f);
			ms_pPhysicsBank->AddSlider("Contact normal length", &CCollisionHistory::ms_fDebugRenderContactNormal, 0.0f, 100.0f, 0.001f);
			ms_pPhysicsBank->AddSlider("Contact render persist frames", &CCollisionHistory::ms_nDebugRenderPersistFrames, 1, 100000, 1);
		ms_pPhysicsBank->PopGroup();

		ms_pPhysicsBank->PushGroup("Deformable Prop Objects");
			CDeformableObjectManager::GetInstance().AddWidgetsToBank(*ms_pPhysicsBank);
		ms_pPhysicsBank->PopGroup();

		ms_pPhysicsBank->PushGroup("Tunable Prop Objects");
			CTunableObjectManager::GetInstance().AddWidgetsToBank(*ms_pPhysicsBank);
		ms_pPhysicsBank->PopGroup();

		ms_pPhysicsBank->PushGroup("Buoyancy");
			CBuoyancy::AddWidgetsToBank(*ms_pPhysicsBank);
		ms_pPhysicsBank->PopGroup();

#if WORLD_PROBE_DEBUG
		WorldProbe::CShapeTestManager* pShapeTestMgr = WorldProbe::GetShapeTestManager();
		ms_pPhysicsBank->PushGroup("WorldProbe debug tools", false);
#if WORLD_PROBE_FREQUENCY_LOGGING
			ms_pPhysicsBank->AddButton("Dump shape test frequency stats to TTY", WorldProbe::CShapeTestManager::DumpFreqLogToTTY);
			ms_pPhysicsBank->AddButton("Clear shape test frequency stats", WorldProbe::CShapeTestManager::ClearFreqLog);
#endif // WORLD_PROBE_FREQUENCY_LOGGING
			ms_pPhysicsBank->AddButton("Perform bound test using selected entity", DoObjectBoundTestCB);
			ms_pPhysicsBank->AddToggle("Force synchronous shapetests", &WorldProbe::CShapeTestManager::ms_bForceSynchronousShapeTests);
			ms_pPhysicsBank->AddToggle("Enable synchronous test debug rendering", &WorldProbe::CShapeTestManager::ms_bDebugDrawSynchronousTestsEnabled);
			ms_pPhysicsBank->AddToggle("Enable asynchronous test debug rendering", &WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncTestsEnabled);
			ms_pPhysicsBank->AddToggle("Enable debug async queue rendering", &WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncQueueSummary);
			ms_pPhysicsBank->PushGroup("Intersection Draw Options",false);
				ms_pPhysicsBank->AddSlider("Max Intersections per shape", &WorldProbe::CShapeTestManager::ms_bMaxIntersectionsToDraw,0,MAX_DEBUG_DRAW_INTERSECTIONS_PER_SHAPE,1);
				ms_pPhysicsBank->AddSlider("Only Intersection index to draw", &WorldProbe::CShapeTestManager::ms_bIntersectionToDrawIndex,-1,MAX_DEBUG_DRAW_INTERSECTIONS_PER_SHAPE,1);
				ms_pPhysicsBank->AddToggle("Draw Hit Point", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitPoint);
				ms_pPhysicsBank->AddToggle("Draw Hit Normal", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitNormal);
				ms_pPhysicsBank->AddToggle("Draw Hit Poly Normal", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitPolyNormal);
				ms_pPhysicsBank->AddToggle("Draw Hit Instance Name", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitInstanceName);
				ms_pPhysicsBank->AddToggle("Draw Hit Handle", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitHandle);
				ms_pPhysicsBank->AddToggle("Draw Hit Component Index", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitComponentIndex);
				ms_pPhysicsBank->AddToggle("Draw Hit Part Index", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitPartIndex);
				ms_pPhysicsBank->AddToggle("Draw Hit Material Name", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitMaterialName);
				ms_pPhysicsBank->AddToggle("Draw Hit Material Id", &WorldProbe::CShapeTestManager::ms_bDebugDrawHitMaterialId);
				ms_pPhysicsBank->AddSlider("Hit point radius", &WorldProbe::CShapeTestManager::ms_fHitPointRadius, 0.01f, 10.0f, 0.01f);
				ms_pPhysicsBank->AddSlider("Hit point normal length", &WorldProbe::CShapeTestManager::ms_fHitPointNormalLength, 0.01f, 10.0f, 0.01f);
			ms_pPhysicsBank->PopGroup();
#if ENABLE_ASYNC_STRESS_TEST
			ms_pPhysicsBank->AddSlider("Stress test probe count", &WorldProbe::CShapeTestManager::ms_uStressTestProbeCount, 1, MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS, 1u);
			ms_pPhysicsBank->AddToggle("Stress test every frame", &WorldProbe::CShapeTestManager::ms_bAsyncStressTestEveryFrame);
			ms_pPhysicsBank->AddToggle("Stress test immediate abortion", &WorldProbe::CShapeTestManager::ms_bAbortAllTestsInReverseOrderImmediatelyAfterStarting);
			ms_pPhysicsBank->AddButton("Stress test once", &WorldProbe::DebugAsyncStressTestOnce);
#endif // ENABLE_ASYNC_STRESS_TEST
			ms_pPhysicsBank->PushGroup("Context filtering",true);
				ms_pPhysicsBank->AddButton("Enable all context types", SetAllWorldProbeContextDrawEnabled);
				ms_pPhysicsBank->AddButton("Disable all context types", SetAllWorldProbeContextDrawDisabled);
				for(int i = 0 ; i < WorldProbe::LOS_NumContexts; i++)
				{
					ms_pPhysicsBank->AddToggle(WorldProbe::GetContextString(static_cast<WorldProbe::ELosContext>(i)), &pShapeTestMgr->ms_abContextFilters[i]);
				}
			ms_pPhysicsBank->PopGroup();// Context filtering
			ms_pPhysicsBank->PushGroup("Batched tests",true);
				ms_pPhysicsBank->AddToggle("Use custom cull volume", &WorldProbe::CShapeTestManager::ms_bUseCustomCullVolume);
				ms_pPhysicsBank->AddToggle("Draw cull volume", &WorldProbe::CShapeTestManager::ms_bDrawCullVolume);
			ms_pPhysicsBank->PopGroup();// Batched tests.
		ms_pPhysicsBank->PopGroup();// WorldProbe debug tools.

	ms_pPhysicsBank->PopGroup(); // Physics debug group.
#endif // WORLD_PROBE_DEBUG

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	ms_pPhysicsBank->PushGroup("2nd surface physics", false);
		CPhysical::sm_defaultSecondSurfaceConfig.AddWidgetsToBank(*ms_pPhysicsBank);
	ms_pPhysicsBank->PopGroup();//2nd surface physics
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

#if NORTH_CLOTHS
	CClothMgr::InitWidgets();
#endif

#endif //USE_FRAG_TUNE
}

void CPhysics::CreateFragTuneBank()
{
	if (ms_pCreateFragTuneBankButton)
	{
		Assertf(ms_pFragTuneBank, "Physics bank needs to be created first");
		Assertf(ms_pCreateFragTuneBankButton, "Physics bank needs to be created first");

		ms_pCreateFragTuneBankButton->Destroy();
		ms_pCreateFragTuneBankButton = NULL;

		ms_pFragTuneBank->AddToggle("Frag Debug Draw", &g_FragDrawToggle, ToggleFragDraw);
		ms_pFragTuneBank->AddToggle("Print breaking impulses for peds", &CPhysics::ms_bPrintBreakingImpulsesForPeds);
		ms_pFragTuneBank->AddToggle("Print breaking impulses for vehicles", &CPhysics::ms_bPrintBreakingImpulsesForVehicles);
		ms_pFragTuneBank->AddToggle("Print applied breaking impulses", &CPhysics::ms_bPrintBreakingImpulsesApplied);
		fragInstGta::AddVehicleFragImpulseFunctionWidgets(*ms_pFragTuneBank);
		ms_pFragTuneBank->AddButton("Display and place on ground", &CDebug::DisplayObjectAndPlaceOnGround);
		FRAGTUNE->AddWidgets(*ms_pFragTuneBank);
	}
}

#if USE_PHYSICAL_HISTORY
static float DISTANCE_TO_RENDER_AROUND_CAM = 50.0f;
#endif //USE_PHYSICAL_HISTORY

void CPhysics::UpdateHistoryRendering()
{
#if USE_PHYSICAL_HISTORY

	if( ms_bRenderAllHistories )
	{
		CEntityIterator iterator(CEntityIterator::IteratePeds|CEntityIterator::IterateVehicles, NULL, camInterface::GetPos(), DISTANCE_TO_RENDER_AROUND_CAM);

		CEntity* pEntity = iterator.GetNext();
		while( pEntity )
		{
			if( pEntity->GetIsTypePed())
			{
				static_cast<CPed*>(pEntity)->m_physicalHistory.RenderHistory(pEntity);
			}
			else if( pEntity->GetIsTypeVehicle() )
			{
				static_cast<CVehicle*>(pEntity)->m_physicalHistory.RenderHistory(pEntity);
			}
			pEntity = iterator.GetNext();
		}
	}
	else if( ms_bRenderFocusHistory )
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pEntity =CDebugScene::FocusEntities_Get(i);
		if( pEntity )
		{
			if( pEntity->GetIsTypePed())
			{
				static_cast<CPed*>(pEntity)->m_physicalHistory.RenderHistory(pEntity);
			}
			else if( pEntity->GetIsTypeVehicle() )
			{
				static_cast<CVehicle*>(pEntity)->m_physicalHistory.RenderHistory(pEntity);
			}
		}
	}
	}
#endif

	// Needs to go here and not in render debug function because it uses the batch renderer
	// so no need for it to be thread safe
	// if we put in render debug we might cause blocks on main thread when trying to add to debugdrawstore
	// as its being rendered
#if __DEV
	ms_debugDrawStore.Render();
#endif
}

void CPhysics::DisplayRagdollCount()
{
	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		const s32 nTotalNumMaleRagdolls = FRAGNMASSETMGR->GetAgentCapacity(pPlayer->GetRagdollInst()->GetType()->GetARTAssetID());
		const s32 nNumAvail = FRAGNMASSETMGR->GetAgentCount(pPlayer->GetRagdollInst()->GetType()->GetARTAssetID());

		grcDebugDraw::AddDebugOutput("\n\nPlayer type ragdolls in use: %i of %i\n",(nTotalNumMaleRagdolls - nNumAvail),nTotalNumMaleRagdolls);
	}
}

void CPhysics::DisplayCollisionRecords()
{
	if( ms_bDisplaySelectedPhysicalCollisionRecords )
	{
		// Null this pointer in case there is no focus ped.
		CPhysical* pFocusPhysical = 0;

		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
		// Early out if no CPhysical is selected.
		if(!pFocusEntity) return;
		if(!pFocusEntity->GetIsPhysical()) return;

		// We must have a selected CPhysical by this stage:
		pFocusPhysical = static_cast<CPhysical*>(pFocusEntity);
		physicsAssertf(pFocusPhysical, "A CPhysical should be selected but pointer is NULL.");

		// Have the CPhysical instance print its records on-screen.
		pFocusPhysical->GetFrameCollisionHistory()->DebugRenderCollisionRecords(pFocusPhysical, true);
	}

	if( ms_bDisplayAllCollisionRecords )
	{
#if ENABLE_PHYSICS_LOCK
		phIterator activeIterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
		phIterator activeIterator;
#endif	// ENABLE_PHYSICS_LOCK
		activeIterator.SetStateIncludeFlags(phLevelBase::STATE_FLAG_ACTIVE);
		activeIterator.InitCull_All();

		for(int levelIndex = activeIterator.GetFirstLevelIndex(CPhysics::GetLevel()) ;
				levelIndex != phInst::INVALID_INDEX ;
				levelIndex = activeIterator.GetNextLevelIndex(CPhysics::GetLevel()) )
		{
			phInst* pInst = CPhysics::GetLevel()->GetInstance(levelIndex);
			CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);

			if(pEntity && pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
				pPhysical->GetFrameCollisionHistory()->DebugRenderCollisionRecords(pPhysical, false);
			}
		}
	}
}

#if __DEV
void CPhysics::RenderFocusEntityExternalForces()
{

	static dev_float fForceDrawScale = 1.0f;
	static dev_float fTorqueDrawScale = 1.0f;
	static dev_s32 iExpiry = 500;

	// TODO: Would be nice to have a TweakScalarV.
	ScalarV fForceDrawScaleV = ScalarVFromF32(fForceDrawScale);
	ScalarV fTorqueDrawScaleV = ScalarVFromF32(fTorqueDrawScale);

	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; i++)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if(pFocusEntity)
		{
			phCollider* pCollider = pFocusEntity->GetCollider();
			if(pCollider)
			{				
				Vec3V vForce; 
				Vec3V vTorque;

				float fInvTimestep = 1.0f / fwTimer::GetRagePhysicsUpdateTimeStep();
				ScalarV vInvTimestep = ScalarVFromF32(fInvTimestep);
				vForce = pCollider->GetForce(vInvTimestep.GetIntrin128());
				vTorque = pCollider->GetTorque(vInvTimestep.GetIntrin128());

				Vec3V vPosition = pCollider->GetPosition();

				ms_debugDrawStore.AddLine(vPosition,vPosition+ fForceDrawScaleV*vForce,Color_purple,iExpiry);
				ms_debugDrawStore.AddLine(vPosition,vPosition+ fTorqueDrawScaleV*vTorque,Color_cyan,iExpiry);
			}
		}
	}
}
#endif

#if __BANK
void CPhysics::SwitchPlayerToRagDollCB()
{
	CEventSwitch2NM event(10000, rage_new CTaskNMRelax(1000, 10000));
	FindPlayerPed()->SwitchToRagdoll(event);
}

// Performs a bound shape test using the bound obtained from the selected entity if any.
void CPhysics::DoObjectBoundTestCB()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if(!pEntity)
		return;

	const Matrix34 trans = MAT34V_TO_MATRIX34( pEntity->GetMatrix() );

	WorldProbe::CShapeTestBoundDesc boundTestDesc;
	boundTestDesc.SetDomainForTest(WorldProbe::TEST_IN_LEVEL);
	boundTestDesc.SetBoundFromEntity(pEntity);
	boundTestDesc.SetTransform(&trans);
	boundTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	boundTestDesc.SetContext(WorldProbe::LOS_Unspecified);

	// Issue the test. Results can be visualised using the WorldProbe debug bank in RAG.
	WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc);
}

char CPhysics::ms_stackModelName[STORE_NAME_LENGTH];

void CPhysics::CreateStackCB()
{
	//Get the crate model and test it is loaded.
	/*
	char sCurrentDisplayObject[256];
	sprintf(sCurrentDisplayObject,"CJ_ALP_CRATE_1");
	*/
	fwModelId nModelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(ms_stackModelName, &nModelId);
	if(pModelInfo==NULL)
		return;

	//Compute the height of the model's bounding box.
	const float fHeightSeparation=0.2f+pModelInfo->GetBoundingBoxMax().z-pModelInfo->GetBoundingBoxMin().z;
	//Set the number of crates we're going to use.
	const int iNumCrates=10;
	//Create all the crates, separated vertically by fHeightSeparation, and offset from the player.
	int i;
	float fHeightCount=1;

	const Vector3 vPlayerHeading=VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetB());
	for(i=0;i<iNumCrates;i++,fHeightCount++)
	{
		//Set the rotation of the body to be identity.
		Matrix34 bodyMat;
		bodyMat.Identity();
		//Set the position of the body to be offset from the player and incremented in height.
		const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
		bodyMat.d=vPlayerPos;
		bodyMat.d+=vPlayerHeading*2.0f;
		const float fHeight=fHeightCount*fHeightSeparation;
		bodyMat.d.z+=fHeight;

		//Forced load of the object.
		bool bForceLoad = false;
		if(!CModelInfo::HaveAssetsLoaded(nModelId))
		{
			CModelInfo::RequestAssets(nModelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			bForceLoad = true;
		}

		if(bForceLoad)
		{
			CStreaming::LoadAllRequestedObjects(true);
		}

		//Instantiate the entity.
		CEntity* pDisplayObject=0;
		if(pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
		{
			pDisplayObject = CVehicleFactory::GetFactory()->Create(nModelId, ENTITY_OWNEDBY_PHYSICS, POPTYPE_RANDOM_AMBIENT, &bodyMat);
		}
		else if(pModelInfo->GetModelType()==MI_TYPE_PED)
		{
			const CControlledByInfo localNpcControl(false, false);
			pDisplayObject = CPedFactory::GetFactory()->CreatePed(localNpcControl, nModelId, &bodyMat, true, true, false);
		}
		else if(pModelInfo->GetIsTypeObject())
		{
			pDisplayObject = CObjectPopulation::CreateObject(nModelId, ENTITY_OWNEDBY_RANDOM, true);
		}
		else
		{
			pDisplayObject = rage_new CBuilding( ENTITY_OWNEDBY_PHYSICS );

			pDisplayObject->SetModelId(nModelId);
		}

		//Set up the entity's physics.
		if(pDisplayObject)
		{
			pDisplayObject->InitPhys();
			pDisplayObject->SetMatrix(bodyMat);
			Printf("Test mat pos %f %f %f \n", VEC3V_ARGS(pDisplayObject->GetCurrentPhysicsInst()->GetPosition()));
			CGameWorld::Add(pDisplayObject, CGameWorld::OUTSIDE );
			phCollider* pCollider=PHSIM->ActivateObject(pDisplayObject->GetCurrentPhysicsInst()->GetLevelIndex());
			Assertf(pCollider,"Couldn't activate object");
			pCollider=0;
		}
	}
}

void CPhysics::AttachCapeToCharCB()
{
	// do something ?? ( old code removed ... it was VERY old )
}

void CPhysics::SetGravityLevelCB()
{
	SetGravityLevel((eGravityLevel)ms_iSelectedGravLevel);
}


void CPhysics::AttachFlagToVehicleCB()
{
#if 1
	//Get the vehicle closest to the player.
	CVehicle* pVehicle=FindPlayerPed()->GetPedIntelligence()->GetClosestVehicleInRange();
	if(pVehicle)
	{
		//Get the cloth model info by hard-coded name.
		fwModelId nModelId;

		ASSERT_ONLY( CBaseModelInfo* pModelInfo = ) CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("cloth_flag",0x78475A17), &nModelId);

		//Forced load of the object.
		bool bForceLoad = false;
		if(!CModelInfo::HaveAssetsLoaded(nModelId))
		{
			CModelInfo::RequestAssets(nModelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			bForceLoad = true;
		}

		if(bForceLoad)
		{
			CStreaming::LoadAllRequestedObjects(true);
		}

		//Create the cloth object.
		Assert(pModelInfo->GetIsTypeObject());
		CObject* pObject = CObjectPopulation::CreateObject(nModelId, ENTITY_OWNEDBY_RANDOM, true);
		Assert(pObject);

		//Set up the entity's physics and attach it to the vehicle.
		if(pObject)
		{
			pObject->InitPhys();

			Matrix34 mat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
			pObject->SetMatrix(mat);
			CGameWorld::Add(pObject, CGameWorld::OUTSIDE );

			//Attach cloth object as child of vehicle.
			const int iVehicleBoneIndex=pVehicle->GetBoneIndex(VEH_CHASSIS);
			const Vector3 vVehicleOffset(1.0f,2.0f,1.5f);
			pObject->AttachToPhysicalBasic(pVehicle, (s16)iVehicleBoneIndex, ATTACH_STATE_BASIC, &vVehicleOffset, 0);
			/*
			//Process child attachments of vehicle to force a setting of the cloth object transform.
			pVehicle->ProcessAllAttachments();
			//Now transform the cloth verts to the frame of the cloth object.
			phInst* pObjectInst=pObject->GetCurrentPhysicsInst();
			Assertf(dynamic_cast<fragInst*>(pObjectInst), "Object with cloth must be a frag");
			fragInst* pObjectFragInst=static_cast<fragInst*>(pObjectInst);
			Assertf(pObjectFragInst->GetCached(), "Cloth object has no cache entry");
			if(pObjectFragInst->GetCached())
			{
				fragCacheEntry* pObjectCacheEntry=pObjectFragInst->GetCacheEntry();
				Assertf(pObjectCacheEntry, "Cloth object has a null cache entry");
				fragHierarchyInst* pObjectHierInst=pObjectCacheEntry->GetHierInst();
				Assertf(pObjectHierInst, "Cloth object has a null hier inst");
				Assertf(1==pObjectHierInst->numEnvClothes, "Cloth object should have 1 env cloth");
				environmentCloth* pEnvCloth=pObjectHierInst->envClothes[0];
				pEnvCloth->GetCloth()->GetClothData().TransformVertexPositions(pObject->GetMatrix());
			}
			*/
		}
	}
#endif
}

#endif

#if __DEV

void CPhysics::ReloadFocusEntityWaterSamplesCB()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt)
		{
			pEnt->GetBaseModelInfo()->ReloadWaterSamples();
		}
	}
}

#endif

#endif //__BANK

#if RSG_PC
bool CPhysics::GetShouldNetworkBlendBeforePhysics( CPhysical* pEntity )
{
	if( CPhysics::GetNumTimeSlices() == 1 &&
		pEntity->GetCollider() &&
		pEntity->GetCollider()->IsArticulated() )
	{
		if( ( MI_CAR_TECHNICAL2.IsValid() && 
			  pEntity->GetModelIndex() == MI_CAR_TECHNICAL2 ) || 
			( MI_CAR_APC.IsValid() &&
			  pEntity->GetModelIndex() == MI_CAR_APC ) ||
			( MI_BOAT_PATROLBOAT.IsValid() && 
			  pEntity->GetModelIndex() == MI_BOAT_PATROLBOAT ) ||
			( MI_BOAT_DINGHY5.IsValid() && 
			  pEntity->GetModelIndex() == MI_BOAT_DINGHY5 ) ||
			( pEntity->GetModelIndex() == MI_PLANE_TULA ) ||
			( pEntity->GetModelIndex() == MI_PLANE_SEABREEZE ) ||
            ( pEntity->GetModelIndex() == MI_CAR_ZHABA ) )
		{
			CVehicle* pVehicle = static_cast< CVehicle* >( pEntity );

			if( pVehicle->GetIsInWater() ||
				( pVehicle->GetNetworkObject() &&
				  static_cast<CNetObjPhysical *>( pVehicle->GetNetworkObject() )->IsNetworkObjectInWater() ) )
			{
				if( fwTimer::GetFrameCount() % 2 == 0 )
				{
					return true;
				}
			}
		}
	}
	return false;
}
#else 
bool CPhysics::GetShouldNetworkBlendBeforePhysics( CPhysical* UNUSED_PARAM( pEntity ) )
{
	return false;
}
#endif


#if PHYSICS_REQUEST_LIST_ENABLED

CLinkList<RegdEnt> CPhysics::ms_physicsReqList;

//
// name:		CPhysics::AddToPhysicsRequestList
// description:	Add object to the physics request list
void CPhysics::AddToPhysicsRequestList(CEntity* pEntity)
{
	// some entities have scaling, or are baked into the static collision mesh. in
	// such cases, we do not want to add them to the physics request list
	if (pEntity->IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS))
	{
		return;
	}

	// don't add buildings to physics request list. Just request physics directly
	if(pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeMLO())
	{
		// TIDY
	}
	else
	{
		RegdEnt reggedEnt(pEntity);
		CLink<RegdEnt>* pLink = ms_physicsReqList.Insert(reggedEnt);

#if !__FINAL
		if (!pLink)
		{
			Warningf("Ran out of space in physics request list. Probably due to lack of memory causing a backlog of requests");
		}
#endif

		pLink=NULL;
	}
}

//
// name:		UpdateRequestList
// description:	Go through request list and check if physics is in memory and then add object to physics world
void CPhysics::UpdatePhysicsRequestList()
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::PHYSICS);

	CLink<RegdEnt>* pLink = ms_physicsReqList.GetFirst()->GetNext();
	while(pLink != ms_physicsReqList.GetLast())
	{
		RegdEnt pEntity(pLink->item);
		CLink<RegdEnt>* pLastLink = pLink;

		pLink = pLink->GetNext();

		if(pEntity && !pEntity->GetCurrentPhysicsInst())
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

			// If physics have loaded
			if (pModelInfo->GetHasLoaded() && (pModelInfo->GetHasBoundInDrawable() || pModelInfo->GetFragType()) )
			{
				// Initialise physics and remove from list
				int res = pEntity->InitPhys();
				if( res == CPhysical::INIT_OK )
				{
					pEntity->AddPhysics();
					ms_physicsReqList.Remove(pLastLink);
				}
			}
			else
			{
				CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), 0);	// we may prefer to let scene streaming handle this - IanK
			}

			Assert(!pEntity->GetIsTypeBuilding());
		}
		else
		{
			ms_physicsReqList.Remove(pLastLink);
		}
	}
}

#endif	//PHYSICS_REQUEST_LIST_ENABLED
