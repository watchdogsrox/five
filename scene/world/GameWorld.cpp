/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/world/GameWorld.cpp
// PURPOSE : Store data about the world, or specific things in the world. But not
//			any form of representation of the world!
// AUTHOR :  John W.
// CREATED : 1/10/08
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/world/gameWorld.h"
#include "scene/world/GameWorldHeightMap.h"

// Rage headers
#include "system/nelem.h"
#include "system/threadtype.h"
#include "spatialdata/sphere.h"
#include "phbound/BoundBox.h"
#include "profile/timebars.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwanimation/facialclipsetgroups.h"
#include "fwnet/netblender.h"
#include "fwpheffects/ropemanager.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscene/world/worldMgr.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/scan/ScanDebug.h"
#include "streaming/streamingvisualize.h"

// Game headers
#include "ai/aichannel.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/debug/AnimViewer.h"
#include "animation/FacialData.h"
#include "audio/northaudioengine.h"
#include "audio/conductorsutil.h"
#include "audio/superconductor.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/system/CameraManager.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "control/WaypointRecording.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "control/replay/Effects/ParticlePedFxPacket.h"
#include "core/game.h"
#include "debug/Debug.h"
#include "debug/DebugScene.h"
#include "event/eventdamage.h"
#include "event/events.h"
#include "debug/GtaPicker.h"
#include "event/ShockingEvents.h"
#include "frontend/NewHud.h"
#include "game/Dispatch/IncidentManager.h"
#include "modelinfo/mlomodelinfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "network/general/NetworkUtil.h"
#include "network/Live/livemanager.h"
#include "network/NetworkInterface.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetworkPlayerMgr.h"
#include "objects/Door.h"
#include "objects/DummyObject.h"
#include "objects/ObjectIntelligence.h"
#include "pathserver/ExportCollision.h"
#include "pedgroup/PedGroup.h"
#include "peds/PedEventScanner.h"
#include "peds/pedfactory.h"
#include "peds/PedGeometryAnalyser.h"
#include "peds/PedIntelligence.h"
#include "peds/pedpopulation.h"
#include "peds/pedtaskrecord.h"
#include "peds/Ped.h"
#include "peds/PlayerInfo.h"
#include "Peds/HealthConfig.h"
#include "physics/breakable.h"
#include "physics/Collider.h"
#include "physics/gtaArchetype.h"
#include "physics/GtaInst.h"
#include "physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/PickupManager.h"
#include "pickups/Pickup.h"
#include "renderer/lights/lights.h"
#include "Renderer/Lights/LightEntity.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Mirrors.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "scene/scene_channel.h"
#include "scene/Building.h"
#include "scene/Entity.h"
#include "scene/InstancePriority.h"
#include "scene/lod/LodMgr.h"
#include "scene/RegdRefTypes.h"
#include "scene/entities/compEntity.h"
#include "scene/world/WorldDebugHelper.h"
#include "script/script.h"
#include "script/script_areas.h"
#include "script/script_cars_and_peds.h"
#include "script/Handlers/GameScriptEntity.h"
#include "streaming/streaming.h"		// For CStreaming::HasObjectLoaded(), etc.
#include "system/Timer.h"
#include "system/TamperActions.h"
#include "Task/Combat/CombatManager.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskWander.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/ScenarioVehicleManager.h"
#include "vehicleAi/task/DeferredTasks.h"
#include "vehicles/train.h"
#include "vehicles/Vehicle.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Misc/WaterCannon.h"
#include "Vfx/Misc/LensFlare.h"
#include "vfx/particles/PtFxManager.h"
#include "weapons/Explosion.h"
#include "weapons/WeaponManager.h"
#include "weapons/projectiles/projectile.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "stats/StatsInterface.h"
#include "game/ModelIndices.h"

#if __BANK
#include "fwDebug/Picker.h"
#endif

AI_COVER_OPTIMISATIONS()

// All this is a workaround because we had trouble getting the marker for UpdateThroughAnimQueue_Stall
// to appear in Tuner captures, for some reason.
#if !__STATS
#define PF_START_EXTRA_MARKER(name)
#define PF_STOP_EXTRA_MARKER(name)
#else
#if __PPU
extern "C" {
int snPushMarker(const char *pText);
int snPopMarker();
}
extern bool g_EnableTunerAnnotation;
#define PF_START_EXTRA_MARKER(x)	do { PF_START(x); if(g_EnableTunerAnnotation) { snPushMarker(#x "M"); } } while(0)
#define PF_STOP_EXTRA_MARKER(x)		do { if(g_EnableTunerAnnotation) { snPopMarker(); } PF_STOP(x); } while(0)
#else	// __PPU
#define PF_START_EXTRA_MARKER(x)	PF_START(x)
#define PF_STOP_EXTRA_MARKER(x)		PF_STOP(x)
#endif	// __PPU
#endif

namespace gtaWorldStats
{
	PF_PAGE(GTA_World_Proc, "Gta World Process");
	PF_GROUP(World_Proc);
	PF_LINK(GTA_World_Proc, World_Proc);
	PF_TIMER(Proc_Global_Events, World_Proc);
	PF_TIMER(Proc_Animation, World_Proc);

	PF_GROUP(Proc_Control);
	PF_LINK(GTA_World_Proc, Proc_Control);
	PF_TIMER(Ped_ProcCont, Proc_Control);
	PF_TIMER(Veh_ProcCont, Proc_Control);
	PF_TIMER(DummyVeh_ProcCont, Proc_Control);
	PF_TIMER(Obj_ProcCont, Proc_Control);

	PF_TIMER(Proc_Control2, World_Proc);
	PF_TIMER(Proc_PedGroups, World_Proc);
	PF_TIMER(Proc_CombatManager, World_Proc);

	PF_GROUP(Proc_Physics);
	PF_LINK(GTA_World_Proc,Proc_Physics);

	PF_TIMER(Phys_Total, Proc_Physics);
	PF_TIMER(Phys_Requests, Proc_Physics);
	PF_TIMER(Phys_Pre, Proc_Physics);
	PF_TIMER(Phys_Pre_Sim, Proc_Physics);
	PF_TIMER(Phys_Sim, Proc_Physics);
	PF_TIMER(Phys_Post_Sim, Proc_Physics);
	PF_TIMER(Phys_Iterate, Proc_Physics);
	PF_TIMER(Phys_Post, Proc_Physics);

	PF_TIMER(Phys_RageSimUpdate,Proc_Physics);
	PF_TIMER(Phys_RageFragUpdate,Proc_Physics);
	PF_TIMER(Phys_RageClothUpdate,Proc_Physics);
	PF_TIMER(Phys_RageRopeUpdate,Proc_Physics);
	PF_TIMER(Phys_RageVerletWaitForTasks,Proc_Physics);
	PF_TIMER(Phys_RagInstBehvrs,Proc_Physics);
	PF_TIMER(Phys_RageClothAttachmentFrameUpdate,Proc_Physics);


	PF_TIMER(Proc_VehRecording, World_Proc);
	PF_TIMER(Proc_PedRecordingAlways, World_Proc);
	PF_TIMER(Proc_Players, World_Proc);
	PF_TIMER(Proc_ScriptRecTasks, World_Proc);
	PF_TIMER(Proc_RemoveFallen, World_Proc);
	PF_TIMER(Proc_Replay, World_Proc);


	PF_GROUP(Scene_Update);
	PF_LINK(GTA_World_Proc, Scene_Update);
	PF_TIMER_OFF(GameWorldPedsAndObjects, Scene_Update);
	PF_TIMER(SU_UPDATE, Scene_Update);
	PF_TIMER(SU_UPDATE_VEHICLE, Scene_Update);
	PF_TIMER(UpdateThroughAnimQueue, Scene_Update);
	PF_TIMER(UpdateThroughAnimQueue_Stall, Scene_Update);
	PF_TIMER(ProcessVehicleEvents, Scene_Update);

	PF_GROUP_OFF(Scene_Update_Counters);
	PF_LINK(GTA_World_Proc, Scene_Update_Counters);
	PF_VALUE_INT(UpdateThroughAnimQueue_StallCount, Scene_Update_Counters);
};

SCENE_OPTIMISATIONS()
AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

PARAM(noPlayer, "[PlayerPed] Game never creates the player.");
PARAM(dynamicAddRemoveSpew, "Debug spew for game world Add/Remove for dynamic entities");

PARAM(logClearCarFromArea, "[GameWorld] print the reason for a clear_area failing to remove a vehicle");
PARAM(logClearCarFromAreaOutOfRange, "[GameWorld] enable the print that gets output if a clear_area fails to remove a vehicle due to the vehicle being out of range");

using namespace gtaWorldStats;

bool CGameWorld::ms_bAllowDelete = true;
#if __BANK
bool CGameWorld::ms_detailedOutOrMapSpew = false;
bool ms_UpdateEntityDesc_StopOnSelectedEntity = false;
#endif

fwInteriorLocation CGameWorld::OUTSIDE = fwInteriorLocation();

u8 CGameWorld::ms_DynEntVisCountdownToRejuvenate = 0;
bool CGameWorld::ms_freezeExteriorVehicles = false;
bool CGameWorld::ms_freezeExteriorPeds = false;
bool CGameWorld::ms_overrideFreezeFlags = false;
#if !__FINAL
bool CGameWorld::ms_busyWaitIfAnimQueueExhausted = false;
#endif	// !__FINAL
bool CGameWorld::ms_updateObjectsThroughAnimQueue = false;
bool CGameWorld::ms_updatePedsThroughAnimQueue = false;
bool CGameWorld::ms_updateVehiclesThroughAnimQueue = false;
bank_bool CGameWorld::ms_makeFrozenEntitiesInvisible = true;
atArray< CPhysical* > CGameWorld::ms_frozenEntities;
atArray< CPhysical* > CGameWorld::ms_unfrozenEntities;
atArray< CPed* > CGameWorld::ms_pedsRunningInstantAnimUpdate;
bool CGameWorld::ms_playerFallbackPosSet = false;
Vector3 CGameWorld::ms_playerFallbackPos(0.0f, 0.0f, 0.0f);
u16 CGameWorld::ms_nNumDelayedRootAttachments = 0;
RegdPhy CGameWorld::ms_delayedRootAttachments[MAX_NUM_POST_PRE_RENDER_ATTACHMENTS];
const CEntity* CGameWorld::ms_pEntityBeingRemovedAndAdded = NULL;
const CEntity* CGameWorld::ms_pEntityBeingRemoved = NULL;
const CPed* CGameWorld::ms_pPedBeingDeleted = NULL;
u32 CGameWorld::m_LastLensFlareModelHash = 0;
const char* CGameWorld::sm_FollowPlayerCarName=NULL;

s32				CGameWorld::sm_localPlayerWanted=0;
eArrestState	CGameWorld::sm_localPlayerArrestState=eArrestState::ArrestState_None;
eDeathState		CGameWorld::sm_localPlayerDeathState=eDeathState::DeathState_Alive;
float			CGameWorld::sm_localPlayerHealth=0.0f;
float			CGameWorld::sm_localPlayerMaxHealth=0.0f;

// --- CFindData : helper class to hold list of found objects during a search
//
// name:		CFindData
// description:	Structure filled out with entity data for find functions
class CFindData 
{
public:
	enum {
		MAX_FIND_ENTITIES = 32
	};
	CFindData(CEntity** pArray, s32 size) : m_size(size), m_number(0), m_pEntities(pArray) {}
	CFindData(CEntity** pArray, s32 size, u32 data) : m_size(size), m_number(0), m_data(data), m_pEntities(pArray) {}

	// name:		AddEntity
	// description:	Add entity to list
	bool AddEntity(CEntity* pEntity) 
	{
		if(m_number < m_size)
		{
			if(m_pEntities)
				m_pEntities[m_number] = pEntity;
			m_number++;
			return true;
		}
		return false;
	}
	CEntity* GetEntity(u32 i) {FastAssert(i<m_number); return m_pEntities[i];}
	u32 GetNumEntities() {return m_number;}
	u32 GetData() {return m_data;}
	void Reset() {m_number = 0;}

private:
	u32 m_size;
	u32 m_number;
	u32 m_data;
	CEntity** m_pEntities;
};

struct SceneUpdateParameters
{
	RegdEnt *apEntitiesToDelete;
	s32 iNumEntitiesToDelete;

	// some object may need to be removed from to the process control list instead of processing them
	RegdDyn *apEntitiesToRemoveFromProcContList;
	s32 iNumEntitiesToRemoveFromProcCont;

#if __DEV
	bool bPrintProcessList;
#endif // __DEV
};


//
// CGameWorld methods
//

void CGameWorld::ResetLineTestOptions()
{
}

// Name			:	ShutdownLevelWithMapUnloaded
// Purpose		:	clean out lists and delete everything
// Parameters	:	none
// Returns		:	none

void CGameWorld::Shutdown2(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
		const u32		maxEntityCount = 10000;
		CEntity**		entityArray = rage_new CEntity*[ maxEntityCount ];
		u32				entityCount;

		gWorldMgr.GetAll( reinterpret_cast< fwEntity** >( entityArray ), &entityCount );

		// WARNING: The array is traversed from end to start because
		// it happens to respect entity dependencies. Thin Ice!

		for (s32 i = entityCount - 1; i >= 0; i--)
			if ( entityArray[i] )
			{
				CGameWorld::Remove( entityArray[i] );
				delete entityArray[i];
			}

		delete entityArray;
    }
}

//
// name:		Shutdown3
// description:	Used so we can shutdown the pools post everything else having been shutdown.
//                  mapdata was crashing because of double shutdown for some CBuilding objects.
void CGameWorld::Shutdown3(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        // Remove remaining Buildings's that weren't in the world.
        CBuilding::GetPool()->DeleteAll();
        CDummyObject::GetPool()->DeleteAll();
    }
}

//
// name:		InitLevelWithMapLoaded
// description:	Initialise world for a new level
void CGameWorld::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		fwSceneUpdate::Init();
		fwSceneUpdate::RegisterSceneUpdate(SU_ADD_SHOCKING_EVENTS, SceneUpdateAddShockingEventsCb, "ShockEv");
		fwSceneUpdate::RegisterSceneUpdate(SU_START_ANIM_UPDATE_PRE_PHYSICS, SceneUpdateStartAnimUpdatePrePhysicsCb, "StrtAnm");
		fwSceneUpdate::RegisterSceneUpdate(SU_END_ANIM_UPDATE_PRE_PHYSICS, SceneUpdateEndAnimUpdatePrePhysicsCb, "EndAnm");
		fwSceneUpdate::RegisterSceneUpdate(SU_UPDATE_PAUSED, SceneUpdateUpdatePausedCb, "UpdPaus");
		fwSceneUpdate::RegisterSceneUpdate(SU_START_ANIM_UPDATE_POST_CAMERA, SceneUpdateStartAnimUpdatePostCameraCb, "AnAfCam");
		fwSceneUpdate::RegisterSceneUpdate(SU_UPDATE, SceneUpdateUpdateCb, "Update");
		fwSceneUpdate::RegisterSceneUpdate(SU_AFTER_ALL_MOVEMENT, SceneUpdateAfterAllMovementCb, "AftMove");
		fwSceneUpdate::RegisterSceneUpdate(SU_RESET_VISIBILITY, SceneUpdateResetVisibilityCb, "RstVis");
		fwSceneUpdate::RegisterSceneUpdate(SU_TRAIN, NULL, "Train");
		fwSceneUpdate::RegisterSceneUpdate(SU_UPDATE_PERCEPTION, SceneUpdateUpdatePerceptionCb, "UpdPrcp");
		fwSceneUpdate::RegisterSceneUpdate(SU_PRESIM_PHYSICS_UPDATE, SceneUpdatePreSimPhysicsUpdateCb, "PreSim");
		fwSceneUpdate::RegisterSceneUpdate(SU_PHYSICS_PRE_UPDATE, SceneUpdatePhysicsPreUpdateCb, "PreUpd");
		fwSceneUpdate::RegisterSceneUpdate(SU_PHYSICS_POST_UPDATE, SceneUpdatePhysicsPostUpdateCb, "PostUpd");
		fwSceneUpdate::RegisterSceneUpdate(SU_UPDATE_VEHICLE, SceneUpdateUpdateCb, "UpdVeh");
		fwSceneUpdate::RegisterSceneUpdate(SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2, SceneUpdateStartAnimUpdatePrePhysicsCb, "StrtAnmPass2");

		// Set the Entity -> EntityDesc function
		fwEntityContainer::SetUpdateEntityDescFunc( CGameWorld::UpdateEntityDesc );

        // create our default world rep
		fwWorldRepMulti*	pDefaultRep = rage_new fwWorldRepMulti;

	    // set the types of objects handled by this representation
	    pDefaultRep->SetHandlingTypes
		    (
		    ENTITY_TYPE_MASK_BUILDING			|
		    ENTITY_TYPE_MASK_ANIMATED_BUILDING	|
		    ENTITY_TYPE_MASK_VEHICLE			|
		    ENTITY_TYPE_MASK_PED				|
		    ENTITY_TYPE_MASK_OBJECT				|
		    ENTITY_TYPE_MASK_DUMMY_OBJECT		|
		    ENTITY_TYPE_MASK_PORTAL				|
		    ENTITY_TYPE_MASK_MLO				|
		    ENTITY_TYPE_MASK_NOTINPOOLS			|
		    ENTITY_TYPE_MASK_PARTICLESYSTEM		|
		    ENTITY_TYPE_MASK_LIGHT				|
			ENTITY_TYPE_MASK_COMPOSITE			|

		    ENTITY_TYPE_MASK_NOTHING
		    );

	    // set the operations which are routed through this representation
	    pDefaultRep->SetOperations
		    (
		    USAGE_MASK_ADD_REMOVE	|
		    USAGE_MASK_PRESCAN		|
		    USAGE_MASK_SCAN			|
		    USAGE_MASK_POSTSCAN		|	
		    USAGE_MASK_SEARCH		|
		    USAGE_MASK_UPDATE		|
		    USAGE_MASK_MISC			|
		    USAGE_MASK_DEBUG
		    );

	    gWorldMgr.Initialise(pDefaultRep);

#if !__NO_OUTPUT
		if (PARAM_logClearCarFromArea.Get())
		{
			ms_bLogClearCarFromArea = true;
		}

		if (PARAM_logClearCarFromAreaOutOfRange.Get())
		{
			ms_bLogClearCarFromArea_OutOfRange = true;
		}
#endif	//	!__NO_OUTPUT
    }
    else if(initMode == INIT_BEFORE_MAP_LOADED)
    {
		CInstancePriority::Init();
		//CGameWorldHeightMap::Clear(); -- don't clear the heightmap, i've initialised it with static data
    }
    else if(initMode == INIT_SESSION)
    {
	    ms_freezeExteriorPeds = false;
	    ms_freezeExteriorVehicles = false;
	    ms_overrideFreezeFlags = false;

		ms_updateObjectsThroughAnimQueue = true;
		ms_updatePedsThroughAnimQueue = true;
		ms_updateVehiclesThroughAnimQueue = true;
    }
}


// Name			:	ShutdownLevelWithMapLoaded
// Purpose		:	only cleans out the lists and removes the stuff from the world
//				:   Used for restart/load game and When you have been arrested
// Parameters	:	none
// Returns		:	none

void CGameWorld::Shutdown1(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        gWorldMgr.Shutdown();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
	    // Use the pools to remove all vehicles and peds. Safer as drivers weren't removed with the code above.
	    CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	    CVehicle* pVehicle;
	    s32 i = (s32) VehiclePool->GetSize();
	    while(i--)
	    {
		    pVehicle = VehiclePool->GetSlot(i);
		    if(pVehicle)
		    {
			    if(pVehicle->IsNetworkClone())
			    {
				    NetworkInterface::UnregisterObject(pVehicle, true);
			    }
			    else
			    {
				    CGameWorld::Remove(pVehicle);
				    CVehicleFactory::GetFactory()->Destroy(pVehicle);
			    }
		    }
	    }

	    //	ensure all dummy peds are removed
	    CPedPopulation::RemoveAllRandomPeds();

	    CPed::Pool *PedPool = CPed::GetPool();
	    CPed* pPed;
	    i=PedPool->GetSize();
	    while(i--)
	    {
		    pPed = PedPool->GetSlot(i);
		    if(pPed)
		    {
			    if(pPed->IsNetworkClone())
			    {
				    NetworkInterface::UnregisterObject(pPed, true);
			    }
			    else
			    {
				    if(pPed->IsPlayer())
				    {
					    CPedFactory::GetFactory()->DestroyPlayerPed(pPed);
				    }
				    else
				    {
					    CPedFactory::GetFactory()->DestroyPed(pPed);
				    }
			    }
		    }
	    }

	    //	don't want to remove dummy objects (those that are part of the map)
	    //	Any that have been converted to real objects have to be converted back to dummies before all
	    //	the real objects are destroyed
	    CObjectPopulation::ConvertAllObjectsToDummyObjects(true);

	    CObject::Pool *ObjPool = CObject::GetPool();
	    CObject* pObj;
	    i = (s32) ObjPool->GetSize();
	    while (i--)
	    {
		    pObj = ObjPool->GetSlot(i);
		    if (pObj)
		    {
			    if(pObj->IsNetworkClone())
			    {
				    NetworkInterface::UnregisterObject(pObj, true);
			    }
			    else
			    {
				    CObjectPopulation::DestroyObject(pObj);
			    }
		    }
	    }

        //gWorldMgr.ShutdownLevelWithMapUnloaded();

		CPhysics::GetRopeManager()->Reset();
    	
#if __ASSERT
		const u32		maxEntityCount = 10000;
		CEntity**		entityArray = rage_new CEntity*[ maxEntityCount ];
		u32				entityCount;

		gWorldMgr.GetAll( reinterpret_cast< fwEntity** >( entityArray ), &entityCount );

		for (s32 i = 0; i < entityCount; ++i)
			if ( entityArray[i] )
			{
				Assert( static_cast< CEntity* >( entityArray[i] )->GetIsTypePed() == false );
				Assert( static_cast< CEntity* >( entityArray[i] )->GetIsTypeVehicle() == false );
			}

		delete entityArray;
#endif

#if !__FINAL
		ValidateShutdown();
#endif // !__FINAL
    }
}

#if !__FINAL
void CGameWorld::PrintProcessControlList()
{
	fwSceneUpdate::PrintSceneUpdateList();
}

void CGameWorld::ValidateShutdown()
{
	CPed::Pool *PedPool = CPed::GetPool();
	if (PedPool->GetNoOfUsedSpaces() != 0)
	{
		int i=PedPool->GetSize();
		while(i--)
		{
			CPed *pPed = PedPool->GetSlot(i);
			if(pPed)
			{
				Errorf("Left-over ped: %p (%s)", pPed, pPed->GetModelName());
			}
		}

		Assertf(false, "Failed to free up all peds - see TTY for list");
	}

	CObject::Pool *ObjPool = CObject::GetPool();
	if (ObjPool->GetNoOfUsedSpaces() != 0)
	{
		int i = (s32) ObjPool->GetSize();
		while (i--)
		{
			CObject *pObj = ObjPool->GetSlot(i);
			if (pObj)
			{
				Errorf("Left-over object: %p (%s)", pObj, pObj->GetModelName());
			}
		}

		Assertf(false, "Failed to free up all objects - see TTY for list");
	}
}
#endif

// some debug tests we want stuff to pass before entering the world
#if __DEV
void CGameWorld::DebugTestsForAdd(CEntity* pEntity){

	// Try to catch entities with duff matrix
	const Matrix34 mat = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
	Assert(rage::FPIsFinite(mat.a.x) && rage::FPIsFinite(mat.a.y) && rage::FPIsFinite(mat.a.z));
	Assert(rage::FPIsFinite(mat.b.x) && rage::FPIsFinite(mat.b.y) && rage::FPIsFinite(mat.b.z));
	Assert(rage::FPIsFinite(mat.c.x) && rage::FPIsFinite(mat.c.y) && rage::FPIsFinite(mat.c.z));

	// try to catch peds with unitialised shaders getting into the world
	if (pEntity->GetIsTypePed()){
		CPedModelInfo* pPMI = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
		Assert(pPMI);
		if (!pPMI->GetIsStreamedGfx()){
			CPed* pPed = static_cast<CPed*>(pEntity);
			sceneAssertf(pPed->GetPedDrawHandler().GetVarData().IsShaderSetup(), "Adding an instance of ped <%s> to world, but shader is not setup!", pPMI->GetModelName());
		}
	}

	if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR)
	{
		if (pEntity->GetIsTypePed()){
			Assertf(false, "Info : Peds or dummy peds shouldn't have interior info idx set\n");
		}

		if (pEntity->GetIsTypeLight())
		{
			pEntity->m_nFlags.bLightObject = true;
		}
	}


}
#else //__DEV
void CGameWorld::DebugTestsForAdd(CEntity*) {}
#endif //__DEV


// Name			:	Add
// Purpose		:	adds entity to world
// Parameters	:	entity
// Returns		:	none

void CGameWorld::Add(CEntity* pEntity, fwInteriorLocation interiorLocation, bool bIgnorePhysicsWorld)
{
	Assert(pEntity);
	Assertf(pEntity != ms_pEntityBeingRemoved, "%s : don't add this entity back into world whilst it is being removed!", pEntity->GetModelName());

	Assertf(!pEntity->GetIsTypePed() || sysThreadType::IsUpdateThread(), "%s: CGameWorld::Add() called from some other thread, this is probably not safe.", pEntity->GetModelName());
	Assertf(pEntity != ms_pPedBeingDeleted, "%s: a ped is being added to the game world when it's about to be deleted.", pEntity->GetModelName());

#if __BANK
	if(pEntity->GetIsTypePed() && NetworkInterface::IsNetworkOpen())
	{
		CPed* pPlayerPed = static_cast<CPed*>(pEntity);

		CBaseModelInfo* pBaseModelInfo = pPlayerPed->GetBaseModelInfo();
		CBuoyancyInfo* pBuoyancyInfo = pBaseModelInfo->GetBuoyancyInfo();
		if(!Verifyf(pBuoyancyInfo && pBuoyancyInfo->m_WaterSamples, "Adding ped to world without water samples!"))
		{
			Displayf("[CGameWorld::Add() - start] %s", pBaseModelInfo->GetModelName());
			Displayf("Player num water samples: %d, water sample ptr: 0x%p", pBuoyancyInfo->m_nNumWaterSamples, pBuoyancyInfo->m_WaterSamples);
			sysStack::PrintStackTrace();
		}
	}
#endif // __BANK

#if __BANK
	if ( PARAM_dynamicAddRemoveSpew.Get() && pEntity->GetIsDynamic() )
	{
		const char*		debugName = ASSERT_ONLY( pEntity->GetIsPhysical() ? static_cast< CPhysical* >(pEntity)->GetDebugName() : ) "none";
		worldDebugf1( "CGameWorld::Add called on entity %p (model %s, debugname %s)", pEntity, pEntity->GetModelName(), debugName );
		worldDebugf1( "Start stack trace" );
		worldDebugf1StackTrace();
		worldDebugf1( "End stack trace" );
	}
#endif

#if __ASSERT
	if (pEntity->GetIsTypeVehicle())
	{
		CVehicle* veh = (CVehicle*)pEntity;
		Assert(!veh->GetIsInPopulationCache());
	}
#endif

	Assert(pEntity->GetBaseModelInfo() != NULL);
	
	const bool	alreadyInWorld = pEntity->GetIsRetainedByInteriorProxy() || pEntity->GetOwnerEntityContainer() != NULL;
	Assertf( !alreadyInWorld, "Trying to add entity %s to the world twice", pEntity->GetModelName() );
	if ( alreadyInWorld )
		return;

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnWorldAddOfFocusEntity(), pEntity );
#if __DEV
	Vector3 vEntPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
#endif	//	__DEV
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfWorldAddCallingEntity(), vEntPosition );
	DEV_BREAK_ON_PROXIMITY( pEntity->GetIsTypePed() && CDebugScene::ShouldDebugBreakOnProximityOfWorldAddCallingPed(), vEntPosition );

	DebugTestsForAdd(pEntity);

	bool bAddToInterior;
	
	if ( pEntity->GetIsTypeDummyObject() && pEntity->m_iplIndex > 0 )
		bAddToInterior = false;
	else
		bAddToInterior = interiorLocation.IsValid();

	// objects in interiors aren't added to world sector lists - they are added to the CInteriorInst entity instead.
	// the CInteriorInst entity does appear in the world sector list.
	if (!bAddToInterior){
		pEntity->Add();
		Assert(!pEntity->m_nFlags.bInMloRoom);	
	} else {
		pEntity->AddToInterior( interiorLocation );
		Assert(pEntity->m_nFlags.bInMloRoom);		// check that it went in!
	}

	// Need to cache fwInteriorLocation because it's unsafe to get it from the CEntity on the northaudioupdate thread
	pEntity->SetAudioInteriorLocation(interiorLocation);

	CGameWorld::PostAdd( pEntity, bIgnorePhysicsWorld );

#if __BANK
	if(pEntity->GetIsTypePed() && NetworkInterface::IsNetworkOpen())
	{
		CPed* pPlayerPed = static_cast<CPed*>(pEntity);

		CBaseModelInfo* pBaseModelInfo = pPlayerPed->GetBaseModelInfo();
		CBuoyancyInfo* pBuoyancyInfo = pBaseModelInfo->GetBuoyancyInfo();
		if(!Verifyf(pBuoyancyInfo && pBuoyancyInfo->m_WaterSamples, "Adding ped to world without water samples!"))
		{
			Displayf("[CGameWorld::Add() - end] %s", pBaseModelInfo->GetModelName());
			Displayf("Local player num water samples: %d, water sample ptr: 0x%p", pBuoyancyInfo->m_nNumWaterSamples, pBuoyancyInfo->m_WaterSamples);
			sysStack::PrintStackTrace();
		}
	}
#endif // __BANK
}

void CGameWorld::PostAdd(CEntity* pEntity, bool bIgnorePhysicsWorld)
{
	ASSERT_ONLY( spdAABB worldAabb( Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX) ); )
	Assertf( worldAabb.ContainsPoint( pEntity->GetTransform().GetPosition() ), "Adding an entity %s outside world limits (at %f,%f,%f)", pEntity->GetModelName(),
		pEntity->GetTransform().GetPosition().GetXf(), pEntity->GetTransform().GetPosition().GetYf(), pEntity->GetTransform().GetPosition().GetZf() );

	if(pEntity->RequiresProcessControl() && pEntity->GetIsDynamic())
	{
		//if(!((CPhysical *)pEntity)->GetIsOnSceneUpdate()){
		{
			((CDynamicEntity*) pEntity)->AddToSceneUpdate();
		}
	} 

	// GTA_RAGE_PHYSICS
	if(!bIgnorePhysicsWorld)
		pEntity->AddPhysics();

	//////////////////////////////////////////////////////////////////////////
	// scenario spawn points
	AddScenarioPointsForEntity(*pEntity, true);

	//////////////////////////////////////////////////////////////////////////
	//ladders
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	if (pModelInfo->GetIsLadder() && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeAnimatedBuilding()))
	{
		CTaskGoToAndClimbLadder::AddLadder(pEntity); 
	}

	if(pEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEntity);

		CSpatialArrayNode& node = pPed->GetSpatialArrayNode();
		u32 typeFlags = pPed->FindDesiredSpatialArrayTypeFlags();
		CPed::GetSpatialArray().Insert(node, typeFlags);
		pPed->UpdateInSpatialArray();

		pPed->UpdateCachedSceneUpdateFlags();
	}

    netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

    if(networkObject)
    {
        NetworkInterface::OnAddedToGameWorld(networkObject);
    }
}


void CGameWorld::AddScenarioPointsForEntity(CEntity& entity, bool useOverrides)
{
	if(entity.GetIsTypeDummyObject())
	{
		return;
	}

	if(IsEntityBeingAddedAndRemoved(&entity))
	{
		return;
	}

	if(useOverrides)
	{
		// Currently, only CObject and CBuilding objects can have overrides
		// (see CScenarioEditor::IsEntityValidForAttachment()), so don't
		// bother calling ApplyOverridesForEntity() for anything else.
		if(entity.GetIsTypeObject() || entity.GetIsTypeBuilding())
		{
			if(SCENARIOPOINTMGR.ApplyOverridesForEntity(entity))
			{
				return;
			}
		}
	}

	if(entity.m_nFlags.bHasSpawnPoints)
	{
		if (entity.GetOwnerEntityContainer() && entity.GetOwnerEntityContainer()->GetOwnerSceneNode())
		{
			entity.GetOwnerEntityContainer()->GetOwnerSceneNode()->SetContainsSpawnPoints(true);
		}

		CBaseModelInfo* pModelInfo = entity.GetBaseModelInfo();
		if(pModelInfo)
		{
			GET_2D_EFFECTS_NOLIGHTS(pModelInfo);
			for (s32 j=0; j<iNumEffects; ++j)
			{
				C2dEffect* pEffect = (*pa2dEffects)[j];
				if(pEffect->GetType() == ET_SPAWN_POINT)
				{
					CPedPopulation::AddScenarioPointToEntityFromEffect(pEffect, &entity);
				}
			}
		}
	}
}

// Name			:	Remove
// Purpose		:	removes entity from world
// Parameters	:	entity
// Returns		:	none

void CGameWorld::Remove(CEntity* pEntity, bool bIgnorePhysicsWorld, bool bSkipPostRemove)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnWorldRemoveOfFocusEntity(), pEntity );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfWorldRemoveCallingEntity(), VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) );



	if (pEntity != ms_pEntityBeingRemovedAndAdded)
	{
		Assert(ms_pEntityBeingRemoved == NULL);
	}

	ms_pEntityBeingRemoved = pEntity;

#if __BANK
	if ( PARAM_dynamicAddRemoveSpew.Get() && pEntity->GetIsDynamic() )
	{
		const char*		debugName = ASSERT_ONLY( pEntity->GetIsPhysical() ? static_cast< CPhysical* >(pEntity)->GetDebugName() : ) "none";
		worldDebugf1( "CGameWorld::Remove called on entity %p (model %s, debugname %s)", pEntity, pEntity->GetModelName(), debugName );
		worldDebugf1( "Start stack trace" );
		worldDebugf1StackTrace();
		worldDebugf1( "End stack trace" );
	}

	// the MP carrier has a peculiar problem where the 2 hangars meet up
	//if ( pLocalPlayer && (pLocalPlayer != m_pOwner) && pLocalPlayer->GetIsInInterior() && NetworkInterface::IsGameInProgress())
	if (pEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEntity);

		if (pPed->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT || 	pPed == CGameWorld::FindLocalPlayer())
		{
			static spdAABB carrierHangarVolumeDebugBox(Vec3V(3062.0f, -4686.0f,  7.0f), Vec3V(3073.0f, -4673.0f, 12.0f));
			Vec3V pos = pPed->GetTransform().GetPosition();
			if (carrierHangarVolumeDebugBox.ContainsPoint(pos))
			{
				const char*		debugName = ASSERT_ONLY( pEntity->GetIsPhysical() ? static_cast< CPhysical* >(pEntity)->GetDebugName() : ) "none";
				worldDebugf1( "<PT DEBUG> CGameWorld::Remove called on script ped in debug volume (model %s, debugname %s)", pEntity->GetModelName(), debugName );
				worldDebugf1( "Start stack trace" );
				worldDebugf1StackTrace();
				worldDebugf1( "End stack trace" );
			}
		}
	}
#endif

	if (pEntity->GetIsRetainedByInteriorProxy()){
		CInteriorProxy::CleanupRetainedEntity(pEntity);
	} else if ( pEntity->GetOwnerEntityContainer() )
	{
		if (pEntity->m_nFlags.bInMloRoom){
			pEntity->RemoveFromInterior();
		} else {
			pEntity->Remove();
		}
	}

	if ( !bSkipPostRemove )
		CGameWorld::PostRemove( pEntity, bIgnorePhysicsWorld );

	if( !bIgnorePhysicsWorld && pEntity->IsInPathServer() )
	{
		CPathServerGta::MaybeRemoveDynamicObject(pEntity);
	}

	Assertf(pEntity->m_nFlags.bInMloRoom == false, "Object %s : still in room", pEntity->GetModelName());
	Assertf(pEntity->GetOwnerEntityContainer() == NULL, "Object %s : still in container", pEntity->GetModelName());

	ms_pEntityBeingRemoved = NULL;
}

void CGameWorld::PostRemove(CEntity* pEntity, bool bIgnorePhysicsWorld, bool bTestScenarioPoints)
{
	if(pEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		CSpatialArrayNode& node = pPed->GetSpatialArrayNode();
		if(node.IsInserted())
		{
			CPed::GetSpatialArray().Remove(node);
		}
	}

	if (pEntity->GetIsDynamic())
	{
		((CPhysical*) pEntity)->RemoveFromSceneUpdate();
	}

	if(!bIgnorePhysicsWorld)
	{
		if(pEntity->GetIsTypeBuilding()
			|| pEntity->GetIsTypeAnimatedBuilding()
			|| pEntity->GetIsTypeDummyObject())
		{
			pEntity->CEntity::RemovePhysics();
		}
		else
		{
			pEntity->RemovePhysics();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// scenario spawn points
	// same logic should be here as well: CMapDataContents::Entities_PostRemove
	if (bTestScenarioPoints && pEntity->m_nFlags.bHasSpawnPoints && !IsEntityBeingAddedAndRemoved(pEntity)) 
	{
		CPedPopulation::RemoveScenarioPointsFromEntity(pEntity);
	}
	//////////////////////////////////////////////////////////////////////////

	//ladders let remove any 
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	if (pModelInfo->GetIsLadder() && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeAnimatedBuilding()))
	{
		CTaskGoToAndClimbLadder::RemoveLadder(pEntity); 

#if __BANK
		CTaskClimbLadder::VerifyNoPedsClimbingDeletedLadder(pEntity); 
#endif
	}

#if __ASSERT
	else
	{
		for(int i=0; i < CTaskGoToAndClimbLadder::ms_Ladders.GetCount(); i++)
		{
			if(CTaskGoToAndClimbLadder::ms_Ladders[i] == pEntity)
			{
				Assertf(0, "B* 527265, Call Robert Traffe please, model: %s", pEntity->GetModelName());
				break; 
			}
		}		
	}
#endif
}

void CGameWorld::RemoveAndAdd(CEntity* pEntity, fwInteriorLocation interiorLocation, bool bIgnorePhysicsWorld)
{
	//Set the entity we are removing and adding.
	ms_pEntityBeingRemovedAndAdded = pEntity;
	
	//Remove the entity.
	Remove(pEntity, bIgnorePhysicsWorld);
	
	//Add the entity.
	Add(pEntity, interiorLocation, bIgnorePhysicsWorld);
	
	//Clear the entity being removed and added.
	ms_pEntityBeingRemovedAndAdded = NULL;
}

bool CGameWorld::HasEntityBeenAdded(CEntity *pEntity)
{
    bool bAddedToWorld = false;

    if(pEntity && (pEntity->GetIsRetainedByInteriorProxy() || pEntity->GetOwnerEntityContainer() != NULL))
    {
        bAddedToWorld = true;
    }

    return bAddedToWorld;
}

#if __BANK
//
// name:		CGameWorld::InitWidgets
// description:	
void CGameWorld::AddWidgets(bkBank* pBank)
{
	if (pBank){
		// put this option into the optimization bank so the artists can see how much exterior peds/cars cose
		pBank->PushGroup("Optimisation");
		CEntity::AddPriorityWidget(pBank);
		pBank->AddToggle("Freeze exterior objects", &ms_overrideFreezeFlags);
		pBank->AddToggle("Make frozen entities invisible", &ms_makeFrozenEntitiesInvisible);
		pBank->AddToggle("Update objects through animation update queue", &ms_updateObjectsThroughAnimQueue);
		pBank->AddToggle("Update peds through animation update queue", &ms_updatePedsThroughAnimQueue);
		pBank->AddToggle("Update vehicles through animation update queue", &ms_updateVehiclesThroughAnimQueue);
		pBank->AddToggle("Busy-wait if anim queue exhausted", &ms_busyWaitIfAnimQueueExhausted);
		pBank->PopGroup();

		pBank->PushGroup("Debug");
		pBank->AddToggle("Detailed out-of-map spew", &ms_detailedOutOrMapSpew );
		pBank->AddToggle("Break on UpdateEntityDesc for selected entity", &ms_UpdateEntityDesc_StopOnSelectedEntity);

		pBank->AddToggle("Log Clear Car From Area", &ms_bLogClearCarFromArea );
		pBank->AddToggle("Log Clear Car From Area - Out Of Range", &ms_bLogClearCarFromArea_OutOfRange );
		pBank->PopGroup();
	}
}

#endif


void CGameWorld::ProcessPaused()
{
	// Process
	fwSceneUpdate::Update(SU_UPDATE_PAUSED);
	
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		//we still need to update this for the lod to work correctly during replay playback
		if (const grcViewport *pViewport = gVpMan.CalcViewportForFragManager())
		{
			FRAGMGR->SetInterestFrustum(*pViewport, *gVpMan.GetFragManagerMatrix());
			bgGlassManager::SetViewport(pViewport);
		}
		// Update any Replay controlled cloth regardless of paused state.
		CPhysics::VerletSimPreUpdate(0.0f); // Sets camera etc for cloth LODing.
		CReplayMgr::PreClothSimUpdate(CGameWorld::FindLocalPlayer());
		CPhysics::VerletSimUpdate(clothManager::ReplayControlled);
	}
#endif //GTA_REPLAY
}

PF_PAGE(GTAPedEvents, "Gta Ped Events");
PF_GROUP(Events);
PF_LINK(GTAPedEvents, Events);
PF_VALUE_INT(Total, Events);
PF_VALUE_INT(AddedToGlobal, Events);
PF_VALUE_INT(FocusPedTotal, Events);


void CGameWorld::ProcessGlobalEvents()
{
	// update SHOCKING events first, as some SHOCKING events need to be checked to see if they
	// need reacting to this frame or not, and its cheaper to check that once than for every ped!
	//CShockingEventsManager::Update();

	if(GetEventGlobalGroup()->GetNumEvents())
	{
		//Add global events to each ped's local event list before we flush them
		fwSceneUpdate::Update(SU_ADD_SHOCKING_EVENTS);
	}

	//Add global events to the script AI queue if they are of interest to the scripts
	GetEventGlobalGroup()->AddEventsToScriptGroup();

	ProcessVehicleEvents();

	CInformFriendsEventQueue::Process();
	CInformGroupEventQueue::Process();

	PF_SET(AddedToGlobal, GetEventGlobalGroup()->GetNumEvents() );
	PF_SET(Total, CEvent::GetPool()->GetNoOfUsedSpaces() );

	// need to call this even if we're skipping or events will hang around
	GetEventGlobalGroup()->ProcessAndFlush();
}

void CGameWorld::ProcessVehicleEvents()
{
	PF_FUNC(ProcessVehicleEvents);
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();	
	while(i--)
	{
		CVehicle* pVehicle = pool->GetSlot(i);
		if(pVehicle)
		{	
			if (1)//(CVehicleAILodManager::ShouldDoFullUpdate(*pVehicle))
			{
				GetEventGlobalGroup()->AddEventsToVehicle(pVehicle);
			}
		}
	}
}

void CGameWorld::StartAnimationUpdate()
{
	// TODO: Check if we can get away without this, hopefully we would have gone
	// through the queue on the last frame, and shouldn't have inserted anything else
	// yet.
	fwAnimDirector::GetPrePhysicsQueue().Clear();

	// Set the animdirector as being locked (prior to the multi threaded update)
	// You cannot do any of the following during this process :
	// Get or set the local matrices of any skeleton (with an animation director)
	// Remove any dynamic entity (with an animation director)
	fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePrePhysics);

	// Update the synchronized scene phases before the anim buffer is flipped as part of the scene update
	fwAnimDirectorComponentSyncedScene::UpdateSyncedScenes(fwTimer::GetTimeStep());

	// Start anim pre physics update
	fwSceneUpdate::Update(SU_START_ANIM_UPDATE_PRE_PHYSICS);

	fwSceneUpdate::Update(SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2);
}

void CGameWorld::EndAnimationUpdate()
{
	// Block the main thread until the animation threads have completed
	fwAnimDirector::WaitForAllUpdatesToComplete();

	// End the anim pre physics update
	fwSceneUpdate::Update(SU_END_ANIM_UPDATE_PRE_PHYSICS);

	// Set the animdirector as being unlocked (post the multi threaded update)
	fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

	// Update the motion blur processing for event tags
	CClipEventTags::CMotionBlurEventTag::Update();
}

void CGameWorld::UpdateEntitiesThroughAnimQueue(SceneUpdateParameters& params)
{
	PF_FUNC(UpdateThroughAnimQueue);

#if __STATS
	int numStalls = 0;
#endif	// __STATS

	fwAnimDirector::Queue& queue = fwAnimDirector::GetPrePhysicsQueue();
	while(1)
	{
		CEntity* pEntity;
#if !__FINAL
		if(!ms_busyWaitIfAnimQueueExhausted)
		{
#endif	// !__FINAL
			PF_PUSH_TIMEBAR_BUDGETED("UpdateThroughAnimQueue_Stall - Pop", 1.0f);
			PF_START_EXTRA_MARKER(UpdateThroughAnimQueue_Stall);
			pEntity = static_cast<CEntity*>(queue.Pop());
			PF_STOP_EXTRA_MARKER(UpdateThroughAnimQueue_Stall);
			PF_POP_TIMEBAR(); // "UpdateThroughAnimQueue_Stall"

			// Would be nice to do this if we stalled:
			//	numStalls++;
			// but unfortunately the call to Pop() doesn't tell us.
#if !__FINAL
		}
		else
		{
#if __STATS
			bool stalled = false;
#endif	// __STATS
			PF_PUSH_TIMEBAR("UpdateThroughAnimQueue_Stall");
			PF_START_EXTRA_MARKER(UpdateThroughAnimQueue_Stall);
			while(1)
			{
				bool queueIsEmpty = false;
				pEntity = static_cast<CEntity*>(queue.PopIfAvailable(queueIsEmpty));
				if(pEntity)
				{
					Assert(!queueIsEmpty);	// Not expected if we got an entity back.
					break;
				}
				else
				{
					if(queueIsEmpty)
					{
						break;
					}

#if __STATS
					stalled = true;
#endif	// __STATS

					// May be best to at least try to do some nops here so we don't slow
					// down the other threads too much by overloading the bus, eating CPU
					// cycles is bad enough.
#if __PS3
					for(int i = 0; i < 64; i++)
					{
						__nop();
					}
#endif	// !__PS3
					// Could try to do some useful processing here, if we had something to do...
				}
			}
			PF_STOP_EXTRA_MARKER(UpdateThroughAnimQueue_Stall);
			PF_POP_TIMEBAR(); // "UpdateThroughAnimQueue_Stall"

#if __STATS
			if(stalled)
			{
				numStalls++;
			}
#endif	// __STATS
		}
#endif	// !__FINAL

		if(!pEntity)
		{
			break;
		}

		// If this entity shouldn't currently be updated through the queue, skip it.
		// fwAnimDirector doesn't know which entities to push into the queue, so this
		// check is needed unless we want all animated entities to use the queue
		// (probably true in the final game, but not if we want to support toggling
		// the updates during development).
		if(!pEntity->GetUpdatingThroughAnimQueue())
		{
			continue;
		}

		{ PF_AUTO_PUSH_TIMEBAR("SceneUpdateUpdateCb");
			SceneUpdateUpdateCb(*pEntity, (void*)&params);
		}

		if(!ShouldUpdateThroughAnimQueue(*pEntity))
		{
			PF_PUSH_TIMEBAR("StopUpdatingThroughAnimQueue");
			Assert(pEntity->GetIsDynamic());
			static_cast<CDynamicEntity*>(pEntity)->StopUpdatingThroughAnimQueue();
			PF_POP_TIMEBAR();
		}
	}

	PF_SET(UpdateThroughAnimQueue_StallCount, numStalls);
}

void CGameWorld::StartPostCameraAnimationUpdate()
{
	CAnimSceneManager::Update(kAnimSceneUpdatePostCamera);
	// Determine if we can trust CDynamicEntity::GetIsVisibleInSomeViewportThisFrame() during this update.
	// This is lagging one frame behind for the upcoming fraction of the frame, but this is mostly a problem
	// when a camera cut is done, so that's what we are basing it on.
	const bool cameraCut = camInterface::GetFrame().GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
	CDynamicEntity::SetCanUseCachedVisibilityThisFrame(!cameraCut);

	fwSceneUpdate::Update(SU_START_ANIM_UPDATE_POST_CAMERA);
}

void CGameWorld::EndPostCameraAnimationUpdate()
{
	// Block the main thread until the animation threads have completed
	fwAnimDirector::WaitForAllUpdatesToComplete();

	// Set the animblender as being unlocked (post the multi threaded update)
	fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);
}

bool CGameWorld::WaitingForCollision( CEntity* pEntity )
{
	if(pEntity->GetIsTypePed() && pEntity->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) )
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() )
		{
			if( pPed->GetMyVehicle()->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) && !pPed->GetMyVehicle()->IsRunningCarRecording() )
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

void CGameWorld::OverrideFreezeFlags(bool bOverride)
{
	ms_overrideFreezeFlags = bOverride;
}


// this is a moderately Dodgy way to decouple fwWorldMgr from  CEntity (which is used by IntersectingCB)
void CGameWorld::ForAllEntitiesIntersecting(fwSearchVolume* pColVol, IntersectingCB cb, void* data, s32 typeFlags, s32 locationFlags, s32 lodFlags, s32 optionFlags, u32 module) {
	CGameWorld::ForAllEntitiesIntersecting(pColVol, (fwIntersectingCB)cb, data, typeFlags, locationFlags, lodFlags, optionFlags, module);
}

void CGameWorld::ForAllEntitiesIntersecting(fwSearchVolume* pColVol, fwIntersectingCB cb, void* data, s32 typeFlags, s32 locationFlags, s32 lodFlags, s32 optionFlags, u32 module)
{
	const int	dynamicEntityTypes =
		ENTITY_TYPE_MASK_ANIMATED_BUILDING	|
		ENTITY_TYPE_MASK_VEHICLE			|
		ENTITY_TYPE_MASK_PED				|
		ENTITY_TYPE_MASK_OBJECT;

	if ( !( typeFlags & ~dynamicEntityTypes ) )
		optionFlags |= SEARCH_OPTION_DYNAMICS_ONLY;

	gWorldMgr.ForAllEntitiesIntersecting(pColVol, cb, data, typeFlags, locationFlags, lodFlags, optionFlags, module);
}

// name:		FindEntitiesCB
// description:	Find entity callback used by intersecting functions
bool CGameWorld::FindEntitiesCB(fwEntity* pEntity, void* data)
{
	CFindData* pFindData = (CFindData*)data;
	return pFindData->AddEntity(static_cast<CEntity*>(pEntity));
}

//
// name:		FindEntitiesCB
// description:	Find entity callback used by intersecting functions
bool CGameWorld::FindEntitiesOfTypeCB(fwEntity* pEntity, void* data)
{
	CFindData* pFindData = (CFindData*)data;
	if(pEntity->GetModelIndex() == static_cast<s32>(pFindData->GetData()))
	{
		return pFindData->AddEntity(static_cast<CEntity*>(pEntity));
	}
	return true;
}

//
// name:		FindEntitiesCB
// description:	Find entity callback used by intersecting functions
bool CGameWorld::FindMissionEntitiesCB(fwEntity* pFWEntity, void* data)
{
	CEntity* pEntity = static_cast<CEntity*>(pFWEntity);

	if(pEntity->GetIsTypeVehicle() ||
		pEntity->GetIsTypePed() ||
		pEntity->GetIsTypeObject())
	{
		CFindData* pFindData = (CFindData*)data;
		return pFindData->AddEntity(pEntity);
	}
	return true;
}

//
// name:		CGameWorld::SetFreezeFlags
// description:	Set the peds and vehicle freeze flags
void CGameWorld::SetFreezeFlags()
{
	CInteriorInst*		pInterior = CPortalVisTracker::GetPrimaryInteriorInst();
	s32				roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
	if(pInterior && (roomIdx > 0) && !NetworkInterface::IsGameInProgress() && ms_overrideFreezeFlags == false)
	{
		CMloModelInfo* pMloModelInfo = pInterior->GetMloModelInfo();
		u32 roomFlags = 0;

		worldAssert(pMloModelInfo->GetIsStreamType());
		
		roomFlags = pMloModelInfo->GetRoomFlags(roomIdx);

		bool bFreeze = (roomFlags & (ROOM_FREEZEPEDS | ROOM_FREEZEVEHICLES)) != 0;

		if(bFreeze)
		{
			s32 num = pInterior->GetNumPortalsInRoom(0);
			s32 i;

			// only do this if portals ardebe instanced
			if(pInterior->m_bIsPopulated)
			{
				// go through all the portals in room 0 (all the exterior portals).
				for(i=0; i<num; i++)
				{
					if(pInterior->IsLinkPortal(0, i))
						continue;
					// If portal has no closed door objects attached  or any of the doors attached 
					// are open then set it can see outside
					s32 numDoors=0;
					s32 numClosedDoors=0;
					s32 globalPortalIdx = pInterior->GetPortalIdxInRoom(0, i);
					for(u32 j=0; j<pInterior->GetNumObjInPortal(globalPortalIdx); j++)
					{
						CEntity* pEntity = pInterior->GetPortalObjInstancePtr(0, i, j);
						if(pEntity && pEntity->GetIsTypeObject())
						{
							CObject* pObject = (CObject*)pEntity;
							if(pObject->IsADoor())
							{
								CDoor * pDoor = static_cast<CDoor*>(pObject);
								numDoors++;
								if(pDoor->IsDoorShut())
									numClosedDoors++;
							}
						}
					}
					// if some of the doors aren't closed then don't freeze
					if(numDoors != numClosedDoors)
						break;
				}
				if(i == num)
				{
					ms_freezeExteriorPeds = (roomFlags & ROOM_FREEZEPEDS) != 0;
					ms_freezeExteriorVehicles = (roomFlags & ROOM_FREEZEVEHICLES) != 0;
					return;
				}
			}
		}
	}
	ms_freezeExteriorPeds = ms_overrideFreezeFlags;
	ms_freezeExteriorVehicles = ms_overrideFreezeFlags;

#if __ASSERT
	if(ms_overrideFreezeFlags)
		Assertf(GetMainPlayerInfo()->AreControlsDisabled() || FindLocalPlayer()->m_nFlags.bInMloRoom, "ms_overrideFreezeFlags has not been reset by script");
#endif
}

bool CGameWorld::IsEntityFrozen(CEntity* pEntity) { return ((CDynamicEntity*)pEntity)->m_nDEflags.bFrozen; }

//
// name:		CGameWorld::IsEntityFrozen
// description:	Return if an entity is frozen
bool CGameWorld::CheckEntityFrozenByInterior(CEntity* pEntity)
{
	bool bFreeze = false;

	if(pEntity->GetIsTypePed())
	{
		CPed* pPed = (CPed*)pEntity;
		if(!pEntity->GetIsInInterior())
		{
			// if ped isn't in an interior, count down timer that stops them being frozen
			if(pPed->GetInteriorFreezeWasInsideCounter() > 0)
			{
				if(pPed->GetInteriorFreezeWasInsideCounter() > fwTimer::GetTimeStepInMilliseconds())
					pPed->SetInteriorFreezeWasInsideCounter( pPed->GetInteriorFreezeWasInsideCounter() - (u16)fwTimer::GetTimeStepInMilliseconds() );
				else
					pPed->SetInteriorFreezeWasInsideCounter(0);
			}

			if(ms_freezeExteriorPeds)
			{
				// Block freezing of mission peds and cops
				const bool bBlockPedFreezing = pPed->PopTypeIsMission() || pPed->GetPedType() == PEDTYPE_COP || pPed->GetInteriorFreezeWasInsideCounter() REPLAY_ONLY(|| CReplayMgr::IsReplayInControlOfWorld());
				if(!bBlockPedFreezing )
				{
					bFreeze = true;
				}
			}
		}
		// set a timer so when peds run out from inside interiors they don't get frozen in the doorway
		else
			pPed->SetInteriorFreezeWasInsideCounter(10000);
	}
	else if(pEntity->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = (CVehicle*)pEntity;
		if(ms_freezeExteriorVehicles && !pEntity->GetIsInInterior())
		{
		
			// Block freezing of mission vehicles and cop cars or cars driver by player
			bool bBlockVehicleFreezing = pVehicle->PopTypeIsMission() || pVehicle->IsLawEnforcementVehicle() || (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI) || (pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE);

			if (!bBlockVehicleFreezing)
			{
				for(s32 i = 0; i < MAX_VEHICLE_SEATS; i++)
				{
					//Ensure the ped is valid.
					CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
					if(!pPed)
					{
						continue;
					}
					if (pPed->PopTypeIsMission() || pPed->IsPlayer())
					{
						bBlockVehicleFreezing  = true;
						break;
					}
				}

			}

			if(!bBlockVehicleFreezing )
			{
				bFreeze = true;
			}
		}
	}

	if (bFreeze)
	{
		Assert(!NetworkInterface::IsGameInProgress());
	}

	return bFreeze;
}


void CGameWorld::ClearFrozenEntitiesLists()
{
	ms_frozenEntities.Reset();
	ms_unfrozenEntities.Reset();
}

void CGameWorld::FlushFrozenEntitiesLists()
{
	for (int i = 0; i < ms_frozenEntities.GetCount(); ++i)
	{
		CPhysical*	pPhysical = ms_frozenEntities[i];

		pPhysical->SetFixedPhysics(true);
		pPhysical->m_nDEflags.bFrozenByInterior = true;
		pPhysical->RemoveSceneUpdateFlags(CGameWorld::SU_PRESIM_PHYSICS_UPDATE);

		// Update the cached scene update flags if we are a ped, since we messed with SU_PRESIM_PHYSICS_UPDATE.
		if(pPhysical->GetIsTypePed())
		{
			static_cast<CPed*>(pPhysical)->UpdateCachedSceneUpdateFlags();
		}
		
		if ( ms_makeFrozenEntitiesInvisible )
			pPhysical->SetIsVisibleForModule( SETISVISIBLE_MODULE_WORLD, false, true );
	}

	for (int i = 0; i < ms_unfrozenEntities.GetCount(); ++i)
	{
		CPhysical*	pPhysical = ms_unfrozenEntities[i];

		pPhysical->SetFixedPhysics(false);
		pPhysical->m_nDEflags.bFrozenByInterior = false;
		pPhysical->AddSceneUpdateFlags(CGameWorld::SU_PRESIM_PHYSICS_UPDATE);

		// Update the cached scene update flags if we are a ped, since we messed with SU_PRESIM_PHYSICS_UPDATE.
		if(pPhysical->GetIsTypePed())
		{
			static_cast<CPed*>(pPhysical)->UpdateCachedSceneUpdateFlags();
		}

		if ( ms_makeFrozenEntitiesInvisible )
			pPhysical->SetIsVisibleForModule( SETISVISIBLE_MODULE_WORLD, true, true );
	}
}

void CGameWorld::UpdateLensFlareAfterPlayerSwitch(const CPed* newPlayerPed)
{
	//update the character specific lens flare
	u32 newPedModelHash = newPlayerPed->GetPedModelInfo()->GetModelNameHash();

	//if the model is the same has hash we don't need to update the lens flare
	if(newPedModelHash != m_LastLensFlareModelHash)
	{
		if(newPedModelHash == MI_PLAYERPED_PLAYER_ZERO.GetName())
		{
			g_LensFlare.ChangeSettings(0); // Micheal
		}
		else if(newPedModelHash == MI_PLAYERPED_PLAYER_ONE.GetName())
		{
			g_LensFlare.ChangeSettings(1); // Franklin
		}
		else if(newPedModelHash == MI_PLAYERPED_PLAYER_TWO.GetName())
		{
			g_LensFlare.ChangeSettings(2); // Trevor
		}
		else
		{
			g_LensFlare.TransitionSettingsRandom(); // Multiplayer
		}

		m_LastLensFlareModelHash = newPedModelHash;
	}
}

bool CGameWorld::ShouldUpdateThroughAnimQueue(const CEntity& entity)
{
	// TODO: probably inline in the header file if it doesn't require new dependencies.
	return (ms_updateObjectsThroughAnimQueue && entity.GetIsTypeObject())
			|| (ms_updatePedsThroughAnimQueue && entity.GetIsTypePed())
			|| (ms_updateVehiclesThroughAnimQueue && entity.GetIsTypeVehicle());
}

//
// name:		CGameWorld::UpdateEntityFrozenByInterior
// description:	Set an entity physics to fixed if it is frozen by the camera being in an interior and set an entity to be active 
//				if it is fixed and not frozen by an interior
void CGameWorld::UpdateEntityFrozenByInterior(CEntity* pEntity)
{
	//Don't add trains to the frozen entities list as we never want to unfreeze them.
	if(pEntity->GetIsPhysical() && !(pEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pEntity)->GetVehicleType() == VEHICLE_TYPE_TRAIN))
	{
		CPhysical* pPhysical = (CPhysical*)pEntity;

		// If entity is frozen and physics hasn't been fixed already then fix physics and set frozen by interior flag
		if(CheckEntityFrozenByInterior(pPhysical))
		{

			if(!pPhysical->GetIsAnyFixedFlagSet() || pPhysical->IsVisible())
				ms_frozenEntities.Grow() = pPhysical;
		}
		// otherwise if frozen by an interior activate again and clear frozen flag
		else
		{
			if(pPhysical->m_nDEflags.bFrozenByInterior)
				ms_unfrozenEntities.Grow() = pPhysical;
		}
	}
}

// Name			:	Process
// Purpose		:	processes moving entities
// Parameters	:	none
// Returns		:	none
//
float WORLD_PLAYER_SHIFT_DAMPING = (0.707f);
#define MAX_ENTITIES_TO_DELETE_PER_FRAME			(512)
#define MAX_ENTITIES_TO_REMOVE_FROM_PROCCONT_LIST	(16)
//
#if !__FINAL
CEntity* CGameWorld::ms_pProcessingEntity = NULL;
#endif

#if !__NO_OUTPUT
bool CGameWorld::ms_bLogClearCarFromArea = false;
bool CGameWorld::ms_bLogClearCarFromArea_OutOfRange = false;
#endif	//	!__NO_OUTPUT

#if RAGE_TIMEBARS
static void Entity_PF_Start(CEntity *pEntity)
{
	if(pEntity->GetIsTypePed())
		PF_START(Ped_ProcCont);
	else if(pEntity->GetIsTypeVehicle())
	{
		CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
		if(!pVeh->IsDummy())
		{
			PF_START(Veh_ProcCont);
		}
		else
		{
			PF_START(DummyVeh_ProcCont);
		}
	}
	else
		PF_START(Obj_ProcCont);
}

static void Entity_PF_Stop(CEntity *pEntity)
{
	if(pEntity->GetIsTypePed())
	{
		PF_STOP(Ped_ProcCont);
	}
	else if(pEntity->GetIsTypeVehicle())
	{
		CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
		if(!pVeh->IsDummy())
		{
			PF_STOP(Veh_ProcCont);
		}
		else
		{
			PF_STOP(DummyVeh_ProcCont);
		}
	}
	else
	{
		PF_STOP(Obj_ProcCont);
	}
}

#else
#define Entity_PF_Start(pEntity) /* No op*/
#define Entity_PF_Stop(pEntity) /* No op*/
#endif // RAGE_TIMEBARS

#if __BANK

extern	bool s_bPreventPlayerProcessForTest;	// Debug.cpp

static bool debugBlockProcessControlOfEntity(CEntity *pEntity)
{
	bool debugBlockProcessControlOfEntity = false;
	if(pEntity != FindPlayerPed())
	{
		const CEntity*	pFocusEntity0	= CDebugScene::FocusEntities_Get(0);
		const bool		isFocus0			= pFocusEntity0 && (pFocusEntity0 == pEntity);
		const bool		sameTypeAsFocus0	= pFocusEntity0 && (pFocusEntity0->GetType() == pEntity->GetType());

		if(CDebugScene::ShouldStopProcessCtrlAllEntities())
		{
			debugBlockProcessControlOfEntity = true;
		}
		else if(CDebugScene::ShouldStopProcessCtrlAllExceptFocusEntity0())
		{
			if(!isFocus0)
			{
				debugBlockProcessControlOfEntity = true;
			}
		}
		else if(CDebugScene::ShouldStopProcessCtrlAllEntitiesOfFocus0Type())
		{
			if(sameTypeAsFocus0)
			{
				debugBlockProcessControlOfEntity = true;
			}
		}
		else if(CDebugScene::ShouldStopProcessCtrlAllEntitiesOfFocus0TypeExceptFocus0())
		{
			if(sameTypeAsFocus0 && !isFocus0)
			{
				debugBlockProcessControlOfEntity = true;
			}
		}
	}
	else
	{
		if(s_bPreventPlayerProcessForTest)
		{
			debugBlockProcessControlOfEntity = true;
		}

	}

	return debugBlockProcessControlOfEntity;
}
#else
#define debugBlockProcessControlOfEntity(entity) (false)
#endif // __BANK

#if __BANK
namespace rage
{
	extern bool g_DisplaySceneUpdateListByType;
}
#endif // __BANK

#if __STATS
EXT_PF_TIMER(DeferredAi_PedsVehiclesObjects);
#endif // __STATS

void CGameWorld::Process()
{
#if __BANK
	// Let the scene update system know about the selected entity.
	// This information is not available to RAGE.
	fwSceneUpdate::SetSelectedEntity(g_PickerManager.GetSelectedEntity());
#endif // __BANK
	
	//	CObject* pObj;

	PF_START_TIMEBAR("World Process");

#if GTA_REPLAY
	// Read before pause test, so we reset regardless
	bool bForceAnimUpdateWhenPaused = CCompEntity::ShouldForceAnimUpdateWhenPaused();
	CCompEntity::ResetForceAnimUpdateWhenPaused();
#endif

	if(fwTimer::IsGamePaused())
	{
		ProcessPaused();

#if GTA_REPLAY
		if(bForceAnimUpdateWhenPaused)
		{
			StartAnimationUpdate();
			EndAnimationUpdate();
		}
#endif	//GTA_REPLAY

		return;
	}
#if __BANK
	camManager::ResetDebugVariables();
#endif // __BANK

	SetFreezeFlags();

	CExpensiveProcessDistributer::Update();

	CShockingEventScanner::UpdateClass();

	PF_START(Proc_Global_Events);
	ProcessGlobalEvents();
	PF_STOP(Proc_Global_Events);

	ClearFrozenEntitiesLists();

	//******************************************************************
	// Anim update (prior to the camera update)
	// Updates the animation blenders on dynamic entities 
	// All dynamic entities will have their mover updated
	// Dynamic entities that are visible in any of the active viewports
	// will have their skeleton updated
	//******************************************************************

	PF_START_TIMEBAR_BUDGETED ("Animation",2.0f);
	PF_START(Proc_Animation);

	StartAnimationUpdate();

	PF_STOP(Proc_Animation);

	TUNE_BOOL(bBlockAsyncAnimUpdate, false)
	if (bBlockAsyncAnimUpdate)
	{
		EndAnimationUpdate();
	}

	FlushFrozenEntitiesLists();

	// Collects Los results from CGameWorldProbeAsync, used for async ped<->ped line tests
	CPedGeometryAnalyser::Process();

	//! Update special ability manager before all entities start processing input.
	CPlayerSpecialAbilityManager::UpdateSpecialAbilityTrigger(FindLocalPlayer());

	PF_PUSH_TIMEBAR_BUDGETED("AI Update",NetworkInterface::IsGameInProgress()?6.0f:7.5f);

	// Add the entity to a removal list rather than deleting it directly.
	// Deleting directly during this process can cause crashes.
	RegdEnt apEntitiesToDelete[MAX_ENTITIES_TO_DELETE_PER_FRAME];
	s32 iNumEntitiesToDelete = 0;

	// some object may need to be removed from to the process control list instead of processing them
	RegdDyn apEntitiesToRemoveFromProcContList[MAX_ENTITIES_TO_REMOVE_FROM_PROCCONT_LIST];
	s32 iNumEntitiesToRemoveFromProcCont = 0;

#if __DEV
	static bool bPrintProcessList = false;
#endif
	bool prevAllowDelete = ms_bAllowDelete;
	AllowDelete(false);

	SceneUpdateParameters params;
	params.apEntitiesToDelete = apEntitiesToDelete;
	params.iNumEntitiesToDelete = iNumEntitiesToDelete;
	params.apEntitiesToRemoveFromProcContList = apEntitiesToRemoveFromProcContList;
	params.iNumEntitiesToRemoveFromProcCont = iNumEntitiesToRemoveFromProcCont;

#if __DEV
	params.bPrintProcessList = bPrintProcessList;
#endif // __DEV

	// 8/7/12 - cthomas - We know only the main thread would possibly be updating physics at this 
	// point in time, so set the flag in the physics system to indicate that. This is an optimization 
	// that will allow us to skip some locking overhead in the physics system as we update the locations 
	// of objects during the AI update - low lod vehicles, for instance, update their location during 
	// their AI update.
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);
	PF_START(GameWorldPedsAndObjects);

	CAnimSceneManager::Update(kAnimSceneUpdatePreAi);

#if RSG_ORBIS
	Water::UpdateCachedWorldBaseCoords();
#endif // RSG_ORBIS

#if __BANK
	if (g_DisplaySceneUpdateListByType)
	{
		PF_FUNC(SU_UPDATE);

		PF_START_TIMEBAR_DETAIL("Peds");
		CPed::StartAiUpdate();
		CVehicle::PreGameWorldAiUpdate();
		//CVehicle::StartAiUpdate() is called at the top of the CVehiclePopulation update now
		//so newly-created cars can run a full AI update in-place there
		fwSceneUpdate::UpdateEntityType(ENTITY_TYPE_PED, SU_UPDATE, &params);
		PF_START_TIMEBAR_DETAIL("Vehicles");
		CScenarioManager::GetVehicleScenarioManager().Update();
		fwSceneUpdate::UpdateEntityType(ENTITY_TYPE_VEHICLE, SU_UPDATE, &params);
		PF_START_TIMEBAR_DETAIL("Objects");
		fwSceneUpdate::UpdateEntityType(ENTITY_TYPE_OBJECT, SU_UPDATE, &params);
		PF_START_TIMEBAR_DETAIL("Animated objects");
		fwSceneUpdate::UpdateEntityType(ENTITY_TYPE_ANIMATED_BUILDING, SU_UPDATE, &params);

		PF_START_TIMEBAR("GameWorld Update Through Anim Queue");
		UpdateEntitiesThroughAnimQueue(params);

		CVehicle::FinishAiUpdate();
	}
	else
#endif // __BANK
	{
		PF_START_TIMEBAR("GameWorld Peds & Objects");
		PF_START(DeferredAi_PedsVehiclesObjects);
		CPed::StartAiUpdate();
		CVehicle::PreGameWorldAiUpdate();
		//CVehicle::StartAiUpdate() is called at the top of the CVehiclePopulation update now
		//so newly-created cars can run a full AI update in-place there
		PF_START(SU_UPDATE);
		fwSceneUpdate::Update(SU_UPDATE, &params);
		PF_STOP(SU_UPDATE);
		PF_START(SU_UPDATE_VEHICLE);
		PF_START_TIMEBAR("GameWorld Vehicles");
		CScenarioManager::GetVehicleScenarioManager().Update();
		fwSceneUpdate::Update(SU_UPDATE_VEHICLE, &params);
		PF_STOP(SU_UPDATE_VEHICLE);

		PF_START_TIMEBAR("GameWorld Update Through Anim Queue");
		UpdateEntitiesThroughAnimQueue(params);

		PF_START_TIMEBAR("aiDeferredTasks::RunAll");
		aiDeferredTasks::RunAll();

		CVehicle::FinishAiUpdate();
		PF_STOP(DeferredAi_PedsVehiclesObjects);
	}
	PF_STOP(GameWorldPedsAndObjects);
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);

	PF_START_TIMEBAR_DETAIL("GameWorld Other");


#if !__FINAL
	CGameWorld::ms_pProcessingEntity = NULL;
#endif

	// Update melee combat AI.
	PF_START(Proc_CombatManager);
	CCombatManager::GetInstance().Update();
	PF_STOP(Proc_CombatManager);

	// Update combat
	CTaskCombat::UpdateClass();

	// Update Wander
	CTaskWander::UpdateClass();

	// Update Smart Flee
	CTaskSmartFlee::UpdateClass();

	// Update our relationship groups
	CRelationshipManager::UpdateRelationshipGroups();

	PF_POP_TIMEBAR();

	PF_START_TIMEBAR_IDLE( "End Animation");
	EndAnimationUpdate();

	CAnimSceneManager::Update(kAnimSceneUpdatePostAi);

	PF_PUSH_TIMEBAR_DETAIL("GameWorld entity removal");

	iNumEntitiesToDelete = params.iNumEntitiesToDelete;
	iNumEntitiesToRemoveFromProcCont = params.iNumEntitiesToRemoveFromProcCont;

	AllowDelete(true);

	// Remove any entities added to the removal list
	for( s32 i = 0; i < iNumEntitiesToDelete; i++ )
	{
		if( apEntitiesToDelete[i] )
		{
			RemoveEntity(apEntitiesToDelete[i]);
		}
	}

	AllowDelete(false);

	// Remove any entities from the process control list that don't need processed any more
	for(int i=0; i<iNumEntitiesToRemoveFromProcCont; i++)
	{
		if(apEntitiesToRemoveFromProcContList[i])
		{
			apEntitiesToRemoveFromProcContList[i]->RemoveFromSceneUpdate();
		}
	}

	AllowDelete(prevAllowDelete);

	PF_START(Proc_PedGroups);
	CPedGroups::Process(); // assigns new leaders to the group if necessary
	PF_STOP(Proc_PedGroups);

	PF_POP_TIMEBAR_DETAIL();


	//******************************************************************
	//	Physics::Update()
	//	This runs the physics simulation
	//******************************************************************



	// This is a bit of a hack to get the broken glass to use vertex shader lighting when directional light/shadows are off
	bgBreakable::SetSafeToUseVSLight(Lights::GetUpdateDirectionalLight().GetIntensity() == 0 || !Lights::GetUpdateDirectionalLight().IsCastShadows());


	PF_PUSH_TIMEBAR( "Physics streaming" );
	CPhysics::UpdateRequests();
	PF_POP_TIMEBAR();

#if SCRATCH_ALLOCATOR
	// Reset the scratch allocator	
	sysMemManager::GetInstance().GetScratchAllocator()->Reset();	
#endif
	PF_PUSH_TIMEBAR_BUDGETED( "Physics", NetworkInterface::IsGameInProgress()?6.0f:7.5f);
	CPhysics::Update();
	PF_POP_TIMEBAR();

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		//we still need to update this for the lod to work correctly
		if (const grcViewport *pViewport = gVpMan.CalcViewportForFragManager())
		{
			FRAGMGR->SetInterestFrustum(*pViewport, *gVpMan.GetFragManagerMatrix());
			bgGlassManager::SetViewport(pViewport);
		}
	}
#endif //GTA_REPLAY

	CVehicleDamage::ResetVehicleExplosionBreakOffPartsFlag();
#if GTA_REPLAY
	// We shouldn't need to do this while a replay is playing back...
	// Having this on while a replay is active causes a leak in tasks somewhere
	if(!CReplayMgr::IsEditModeActive())
#endif // GTA_REPLAY
	CVehiclePopulation::UpdateLinksAfterGather();

#if __TRACK_PEDS_IN_NAVMESH
	PF_START_TIMEBAR_DETAIL ( "Track peds");
	// Keep a track of all the peds positions in the navmesh system
	{
		CPathServer::UpdatePedTracking();
	}
#endif

	PF_START_TIMEBAR ( "Ped ai");

	//NOTE: We always capture vehicle recording data in the game update, but we can optionally playback this data in either the individual physics
	//time-slices or the game update.
#if !__FINAL
	CVehicleRecordingMgr::SaveDataForThisFrame();
#endif // !__FINAL

    if(!CVehicleRecordingMgr::sm_bUpdateWithPhysics)
    {
	    PF_START(Proc_VehRecording);
		CVehicleRecordingMgr::RetrieveDataForThisFrame();
	    PF_STOP(Proc_VehRecording);
    }

	// Waypoint-based ped movement recording
#if __BANK
	CWaypointRecording::UpdateRecording();
#endif

	//**********************************************************************
	//	ProcessAfterPhysicsUpdate()
	//	This does stuff which is needed to be done after objects have been
	//	moved.  This includes a pass on peds' AI, updating portal trackers,
	//	updating obstacle positions for pathfinding and positioning entities
	//	which are attached to other entities.
	//
	//	NB: Do this before peds CPhysics::Update() so that peds in vehicles
	//	that are attached to other moving things get positioned correctly
	//**********************************************************************

	//ProcessAfterPhysicsUpdate();

	PF_START(Proc_ScriptRecTasks);
	CPedScriptedTaskRecord::Process();
	PF_STOP(Proc_ScriptRecTasks);

	// moved before physics update to avoid peds ending up with no tasks
	// if the group leader is destroyed while group members still exist
	//CPedGroups::Process(); // assign's new leaders to the group if necessary

	// Counting events seems to crash the game during navmesh export, so I've disabled it in that situation
#if NAVMESH_EXPORT
	bool bCountEvents = (CNavMeshDataExporter::GetExportMode() != CNavMeshDataExporter::eMapExport);
#else
	static const bool bCountEvents = true;
#endif

	if(bCountEvents)
	{
		USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);
	}

	PF_START_TIMEBAR_DETAIL ( "Remove Fallen");
	PF_START(Proc_RemoveFallen);

	if ((fwTimer::GetSystemFrameCount() & 7) == 1)
		RemoveFallenPeds();
	else if ((fwTimer::GetSystemFrameCount() & 7) == 3)
		RemoveFallenCars();
	else
		RemoveFallenObjects();

	PF_STOP(Proc_RemoveFallen);

	CVehicle *pPlayerVehicle = FindFollowPlayerVehicle();
	if (pPlayerVehicle && pPlayerVehicle->GetVehicleModelInfo())
	{
		sm_FollowPlayerCarName = pPlayerVehicle->GetVehicleModelInfo()->GetGameName();
	}
	else
	{
		sm_FollowPlayerCarName = NULL;
	}

	// cache data to be accessed from the outside
	CWanted* wanted = FindLocalPlayerWanted_Safe();
	sm_localPlayerWanted      = wanted?wanted->GetWantedLevel():0;
	sm_localPlayerArrestState = FindLocalPlayerArrestState_Safe();
	sm_localPlayerDeathState  = FindLocalPlayerDeathState_Safe();
	sm_localPlayerHealth      = FindLocalPlayerHealth_Safe();
	sm_localPlayerMaxHealth   = FindLocalPlayerMaxHealth_Safe();
}



// Name			:	ProcessAfterAllMovement
// Purpose		:	processes everything that requires up to date positions
// Parameters	:	none
// Returns		:	none
void CGameWorld::ProcessAfterAllMovement()
{
	if (fwTimer::IsGamePaused())
	{
		return;
	}

	// We used to wait for this at the end of CPhysics::Update; the wait was moved here so we won't stall
	// the main thread waiting.  We need to make sure these tasks are done now because they can update physics
	// outside the main thread
	CPhysics::WaitForVerletTasks();

	// 8/7/12 - cthomas - We know only the main thread would possibly be updating physics at this 
	// point in time, so set the flag in the physics system to indicate that. This is an optimization 
	// that will allow us to skip some locking overhead in the physics system as we update the 
	// locations of objects in the physics sytem during ProcessAfterAllMovement() - things like 
	// attachments update their location at this time.
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);

	//	Update the positions of any entities in synchronized scenes.
	//	This needs to be done before the attachments are updated because these
	//	entities may have objects attached to them. Attaching synchronized scenes
	//	To entities that are themselves attached is currently unsupported.
	fwSceneUpdate::Update(SU_AFTER_ALL_MOVEMENT);
	CAnimSceneManager::Update(kAnimSceneUpdatePostMovement);

	PF_PUSH_TIMEBAR("Vehicles");
	s32 i = (s32) CVehicle::GetPool()->GetSize();
	CVehicle* pVehicle = NULL;
	CVehicle* pNextVehicle = NULL;
	while((pNextVehicle == NULL) && (i > 0))
	{
		pNextVehicle = CVehicle::GetPool()->GetSlot(--i);
	}

	while(pNextVehicle)
	{
		pVehicle = pNextVehicle;

		pNextVehicle = NULL;
		while((pNextVehicle == NULL) && (i > 0))
		{
			pNextVehicle = CVehicle::GetPool()->GetSlot(--i);
		}
		PrefetchObject(pNextVehicle);

		// skip scheduled vehicles, which are vehicles created but waiting to be correctly placed by cargens
		// we don't want to run the portal tracker for them since that might place them in the game world
		// also skip vehicles that are in the reuse pool, we don't want them in the world either
		if(pVehicle && !pVehicle->GetIsScheduled() && !pVehicle->GetIsInReusePool())
		{
			ProcessAttachmentAfterAllMovement(pVehicle);
			if(pVehicle->GetVehicleModelInfo()->GetHasSeatCollision())
			{
				pVehicle->ProcessSeatCollisionBound();
			}

			if(pVehicle->WasUnableToAddToPathServer())
			{
				CPathServerGta::MaybeAddDynamicObject(pVehicle);
			}
			pVehicle->UpdatePortalTracker();

			if ( pVehicle->m_nDEflags.bIsOutOfMap )
			{
				pVehicle->m_nDEflags.bIsOutOfMap = false;

				if( !pVehicle->InheritsFromTrain() || ((CTrain*)pVehicle)->m_nTrainFlags.bCreatedAsScriptVehicle )
				{
					CGameWorld::RemoveOutOfMapVehicle( pVehicle );
				}
			}
		}
	}
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Peds");
	//Process each ped's ai.
	i = CPed::GetPool()->GetSize();
	CPed *pPed = NULL;
	CPed *pNextPed = NULL;
	while((pNextPed == NULL) && (i > 0))
	{
		pNextPed = CPed::GetPool()->GetSlot(--i);
	}

	while(pNextPed) 
	{
		pPed = pNextPed;

		pNextPed = NULL;
		while((pNextPed == NULL) && (i > 0))
		{
			pNextPed = CPed::GetPool()->GetSlot(--i);
		}
		PrefetchObject(pNextPed);

		if(pPed && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
		{
			pPed->m_PedResetFlags.ResetPostMovement(pPed);
			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement) || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovementTimeSliced))
			{
				pPed->GetPedIntelligence()->ProcessPostMovement();
			}

			pPed->GetMotionData()->ProcessPostMovement();

			ProcessAttachmentAfterAllMovement(pPed);

			pPed->UpdatePortalTracker();

			pPed->ProcessPreCamera();

			if ( pPed->m_nDEflags.bIsOutOfMap )
			{
				// The dynamic entity flag 'bIsOutOfMap' is only ever set from the portal tracker, when a ped
				// has traversed a portal with no destination.  This is handy because we can use this fact to
				// help narrow down why mission may have fallen through map, potentially breaking game progress..
				bool bIsOutOfMap = pPed->m_nDEflags.bIsOutOfMap;
				pPed->m_nDEflags.bIsOutOfMap = false;
				CGameWorld::RemoveOutOfMapPed( pPed, bIsOutOfMap );
			}
		}
	}
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Objects");
	bool bTrackerUpdate = false;
	i = (s32) CObject::GetPool()->GetSize();
	CObject* pObject = NULL;
	CObject* pNextObject = NULL;
	while((pNextObject == NULL) && (i > 0))
	{
		pNextObject = CObject::GetPool()->GetSlot(--i);
	}

	while(pNextObject)
	{
		bTrackerUpdate = false;
		pObject = pNextObject;

		pNextObject = NULL;
		while((pNextObject == NULL) && (i > 0))
		{
			pNextObject = CObject::GetPool()->GetSlot(--i);
		}
		PrefetchObject(pNextObject);

		if (Verifyf(pObject && CObject::GetPool()->IsValidPtr(pObject), "Trying to access a dead object!"))
		{
			ProcessAttachmentAfterAllMovement(pObject);
			CObjectIntelligence* pObjectIntelligence = pObject->GetObjectIntelligence();
			if(pObjectIntelligence)
				pObjectIntelligence->ProcessPostMovement();

			if(pObject->WasUnableToAddToPathServer())
			{
				CPathServerGta::MaybeAddDynamicObject(pObject);
			}

			if (pObject->m_nFlags.bInMloRoom) {
				bTrackerUpdate = true;								// already in an interior
			} else if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT){ 	// any mission objects
				bTrackerUpdate = true;	
			} else if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE){
				bTrackerUpdate = true;
			} else if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY){
				bTrackerUpdate = true;
			} else if (!pObject->GetIsStatic()){
				bTrackerUpdate = true;								// anything moving
			}

			if (bTrackerUpdate)
			{
				pObject->UpdatePortalTracker();
			}

			if ( pObject->m_nDEflags.bIsOutOfMap )
			{
				pObject->m_nDEflags.bIsOutOfMap = false;
				CGameWorld::RemoveOutOfMapObject( pObject );
			}
		}
	}
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Pickups");
	i = CPickup::GetPool()->GetSize();
	CPickup *pPickup = NULL;
	CPickup *pNextPickup = NULL;
	while((pNextPickup == NULL) && (i > 0))
	{
		pNextPickup = CPickup::GetPool()->GetSlot(--i);
	}

	while(pNextPickup)
	{
		pPickup = pNextPickup;

		pNextPickup = NULL;
		while((pNextPickup == NULL) && (i > 0))
		{
			pNextPickup = CPickup::GetPool()->GetSlot(--i);
		}
		PrefetchObject(pNextPickup);

		if(pPickup)
		{
			ProcessAttachmentAfterAllMovement(pPickup);
			CObjectIntelligence* pObjectIntelligence = pPickup->GetObjectIntelligence();
			if(pObjectIntelligence)
				pObjectIntelligence->ProcessPostMovement();

			pPickup->UpdatePortalTracker();

			if ( pPickup->m_nDEflags.bIsOutOfMap )
			{
				pPickup->m_nDEflags.bIsOutOfMap = false;
				CGameWorld::RemoveOutOfMapObject( pPickup );
			}
		}
	}
	PF_POP_TIMEBAR();

    if (NetworkInterface::IsGameInProgress())
	{
		NetworkInterface::GetObjectManager().UpdateAfterAllMovement();
	}

	// Update clipset streaming 
	CPrioritizedClipSetRequestManager::GetInstance().Update(fwTimer::GetTimeStep());

	// Update physics octtree
	CPhysics::CommitDeferredOctreeUpdates();
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);
}

// Name			:	ProcessPedsEarlyAfterCameraUpdate
// Purpose		:	processes just peds early based on up to date camera orientation and position. See ProcessAfterCameraUpdate() for the rest.
// Parameters	:	none
// Returns		:	none
void CGameWorld::ProcessPedsEarlyAfterCameraUpdate()
{
	PF_AUTO_PUSH_TIMEBAR("ProcessPedsEarlyAfterCameraUpdate");

	if (!fwTimer::IsGamePaused())
	{
		// Set the animation director as being locked (prior to the multi threaded update)
		// You cannot do any of the following during this process :
		// Get or set the local matrices of any skeleton (with an animation director)
		// Remove any dynamic entity (with an animation director)
		fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePrePhysics);

		//Process each ped's ai.
		PF_PUSH_TIMEBAR("Peds");
		Assert(ms_pedsRunningInstantAnimUpdate.GetCount() == 0);
		CPed *pPed = NULL;
		s32 i = CPed::GetPool()->GetSize();
		while(i--)
		{
			pPed = CPed::GetPool()->GetSlot(i);
			if(pPed)
			{
				const bool bStartedInstantAnimUpdate = pPed->ProcessPostCamera();
				if(Unlikely(bStartedInstantAnimUpdate))
				{
					// We simply push the raw ped pointer onto this array for now. We're assuming 
					// that nothing can delete this ped between now and when we loop over this 
					// array and wait for the animations to finish in ProcessAfterCameraUpdate().
					ms_pedsRunningInstantAnimUpdate.PushAndGrow(pPed);
				}
			}
		}
		PF_POP_TIMEBAR();
	}
}


// Name			:	ProcessAfterCameraUpdate
// Purpose		:	processes everything that requires up to date camera orientation and position
// Parameters	:	none
// Returns		:	none
void CGameWorld::ProcessAfterCameraUpdate()
{
	PF_AUTO_PUSH_TIMEBAR("World ProcessAfterCameraUpdate");

	if (!fwTimer::IsGamePaused())
	{		
		CVehicle::ProcessAfterCameraUpdate();

		// Process each objects ai.
		PF_PUSH_TIMEBAR("Objects");
		// Loop over in the object post camera array starting from the end, as calls to 
		// ProcessPostCamera() may remove the object from the array, and removing an objects 
		// from the array will fill the slot with the last item in the array (DeleteFast()).
		int numPostCameraObjects = CObject::ms_ObjectsThatNeedProcessPostCamera.GetCount();
		for(int i = numPostCameraObjects - 1; i >= 0; i--)
		{
			CObject* pObject = CObject::ms_ObjectsThatNeedProcessPostCamera[i];
			pObject->ProcessPostCamera();
		}
		PF_POP_TIMEBAR();

		// Finish the post-camera update for any peds which needed to do an instant anim update.
		PF_PUSH_TIMEBAR("Peds with Instant Anim Updates");
		int numPedsWithInstantAnimUpdate = ms_pedsRunningInstantAnimUpdate.GetCount();
		for(int i = 0; i < numPedsWithInstantAnimUpdate; i++)
		{
			CPed* pPed = ms_pedsRunningInstantAnimUpdate[i];
			pPed->InstantAnimUpdateEnd();
		}
		ms_pedsRunningInstantAnimUpdate.ResetCount();
		PF_POP_TIMEBAR();
	}

	// This doesn't do much now except refreshing the frame count we use for visibility
	// on entities. It probably needs to happen even if we are not paused, since the
	// prerender code will run and do its thing either way.
	ProcessDynamicObjectVisibility();

	if (!fwTimer::IsGamePaused())
	{
		//******************************************************************
		// Anim update (post the camera update)
		// Updates the animation blenders on dynamic entities 
		// All dynamic entities will have already had their mover updated
		// Dynamic entities that were not visible in any active viewport
		// prior to the camera update but are visible post the camera update
		// will have their skeleton updated
		//******************************************************************
		PF_AUTO_PUSH_TIMEBAR("PostCameraAnimationUpdate");
		StartPostCameraAnimationUpdate();
		EndPostCameraAnimationUpdate();
	}

	if (!fwTimer::IsGamePaused() REPLAY_ONLY(|| CReplayMgr::IsReplayInControlOfWorld()))
	{
		PF_AUTO_PUSH_TIMEBAR("VfxHelper");
		const grcViewport *vp = gVpMan.GetCurrentGameGrcViewport();
		CVfxHelper::SetGameCamPos(vp->GetCameraPosition());
		CVfxHelper::SetGameCamForward(-vp->GetCameraMtx().GetCol2());
		CVfxHelper::SetGameCamFOV(vp->GetFOVY());
		CVfxHelper::UpdateCamInteriorLocation();
		CVfxHelper::UpdateCurrFollowCamViewMode();
		CVfxHelper::SetLastTimeStep(fwTimer::GetTimeStep());
	}

	if (!fwTimer::IsGamePaused())
	{
		PF_AUTO_PUSH_TIMEBAR("Process IK Solvers");
		CBaseIkManager::ProcessSolvers();
	}

	BANK_ONLY(fwSceneUpdate::DebugDraw3D());
}



void CGameWorld::ProcessAfterPreRender()
{
	// finish up ped damage render target allocations, now that we have all the priorities
	CPedDamageManager::GetInstance().PostPreRender();
	
	PF_PUSH_TIMEBAR("fwAnimDirector::WaitForAllUpdatesToComplete");
	// Block the main thread until the animation threads have completed
	fwAnimDirector::WaitForAllUpdatesToComplete();
	PF_POP_TIMEBAR();

	// Set the animblender as being unlocked (post the multi threaded update)
	fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

	CAnimSceneManager::Update(kAnimSceneUpdatePreRender);

	// turn on option that forces new objects onto render list if frag object breaks apart in this section of code
	// WARNING need to reset this flag before exiting this function
	CBreakable::SetInsertDirectToRenderList(true);

	//if(fwTimer::bSkipProcessThisFrame)
	//	return;

	// 8/7/12 - cthomas - We know only the main thread would possibly be updating physics at this 
	// point in time, so set the flag in the physics system to indicate that. This is an optimization 
	// that will allow us to skip some locking overhead in the physics system as we update locations 
	// of objects during this function.
	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(true);

	// Update the UI menu ped last
	if (CPauseMenu::IsActive() && CPauseMenu::DynamicMenuExists())
	{
		CPauseMenu::GetDisplayPed()->UpdatePedPose();
		CPauseMenu::GetLocalPlayerDisplayPed()->UpdatePedPose();
	}

	//Process each ped's ai.
	CPed *pPed = NULL;
	s32 i = CPed::GetPool()->GetSize();
	while(i--)
	{
		pPed = CPed::GetPool()->GetSlot(i);
		if(pPed)
		{
			pPed->PostPreRender();
		}
	}

#if __BANK
	CAnimViewer::OnDynamicEntityPreRender();
#endif // __BANK

	// Process all delayed attachments
	// After peds so any Ik etc has been done
	for(int iAttachIndex = 0; iAttachIndex < ms_nNumDelayedRootAttachments; iAttachIndex++)
	{
		if(ms_delayedRootAttachments[iAttachIndex])
		{
			ms_delayedRootAttachments[iAttachIndex]->ProcessAllAttachments();
			ms_delayedRootAttachments[iAttachIndex] = NULL;
		}
	}
	ms_nNumDelayedRootAttachments = 0;

	// Weapons done after attachments so matrices up to date
	i = CWeapon::GetPool()->GetSize();
	while(i--)
	{
		CWeapon* pWeapon = CWeapon::GetPool()->GetSlot(i);
		if(pWeapon)
		{
			pWeapon->PostPreRender();
		}
	}
	
	// Process each ped's after-attachments ai
	i = CPed::GetPool()->GetSize();
	while (i--)
	{
		pPed = CPed::GetPool()->GetSlot(i);
		if(pPed)
		{
			pPed->PostPreRenderAfterAttachments();
		}
	}

	// Projectiles done after attachments so matrices up to date
	CProjectileManager::PostPreRender();

	PHLEVEL->SetOnlyMainThreadIsUpdatingPhysics(false);

	PF_START_TIMEBAR_DETAIL("Weapon");
	CWeaponManager::Process();

	CPhysics::UpdateBreakableGlass();
	
	// WARNING - don't exit this function with out reseting this to false
	CBreakable::SetInsertDirectToRenderList(false);

	CClipEventTags::CMotionBlurEventTag::PostPreRender();

	PostFX::GetFPVPos();
}

void CGameWorld::SceneUpdateAddShockingEventsCb(fwEntity &entity, void *)
{
	Assert(entity.GetType() == ENTITY_TYPE_PED);

	GetEventGlobalGroup()->AddEventsToPed(static_cast<CPed *>(&entity));
}

void CGameWorld::SceneUpdateStartAnimUpdatePrePhysicsCb(fwEntity &entity, void *)
{
	// Calculate whether the dynamic entity should be processed this frame and set its frozen flag accordingly.
	// Network clones should not be frozen or their clone tasks will not be processed
	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;

	// B*1892279: Prevent damaged objects from doing animation updates. The anim update restores the undamaged skeleton that was previously zeroed out by the frag system.
	// This causes both the undamaged and the damaged model to be rendered for the object, which is obviously wrong!
	// Objects normally remove themselves from animation updates during PreRender, but this doesn't happen when an object is destroyed while it is not in any PreRender lists.
	// If an object doesn't prevent animation updates in the same frame that it is destroyed, animation updates will occur in subsequent frames until the object is back in a PreRender list.
	if(pDynamicEntity->GetIsTypeObject() && pDynamicEntity->m_nFlags.bRenderDamaged)
	{
		pDynamicEntity->RemoveFromSceneUpdate();
		return;
	}

	if ( pDynamicEntity->GetAnimDirector() )
	{
		pDynamicEntity->GetAnimDirector()->PreUpdate(fwTimer::GetTimeStep());
	}

	UpdateEntityFrozenByInterior(pDynamicEntity);
	bool bWaitingForCollision = WaitingForCollision(pDynamicEntity);
	if( (pDynamicEntity->m_nDEflags.bFrozenByInterior || bWaitingForCollision) && !pDynamicEntity->IsNetworkClone())
	{
		bool bFreeze = true;

		if(pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pDynamicEntity);
			bFreeze = pPed->CanBeFrozen();
		}

		if(bFreeze)
		{
			pDynamicEntity->m_nDEflags.bFrozen = true;

			pDynamicEntity->RemoveSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT | CGameWorld::SU_END_ANIM_UPDATE_PRE_PHYSICS);

			// In this case we won't be pushed onto the update queue, so we need to switch to regular updates.
			pDynamicEntity->StopUpdatingThroughAnimQueue();

			return;
		}
	}

	pDynamicEntity->m_nDEflags.bFrozen = false;

	u32 sceneUpdateFlags = CGameWorld::SU_END_ANIM_UPDATE_PRE_PHYSICS;
	if (pDynamicEntity->GetAnimDirector() && pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>())
		sceneUpdateFlags |= CGameWorld::SU_AFTER_ALL_MOVEMENT;
	pDynamicEntity->AddSceneUpdateFlags(sceneUpdateFlags);

	if (entity.IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
	{
		// In this case we won't be pushed onto the update queue, so we need to switch to regular updates.
		pDynamicEntity->StopUpdatingThroughAnimQueue();

		return;
	}

	if (pDynamicEntity->GetIsDynamic())
	{
		bool bDoAnimUpdate = true;
		if (pDynamicEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pDynamicEntity);
			
			//Update the reserve components
			if (!pVehicle->IsDummy())
			{
				pVehicle->GetComponentReservationMgr()->Update();
			}

			if (pVehicle->m_nVehicleFlags.bForcePostCameraAnimUpdate)
			{
				pVehicle->m_nVehicleFlags.bForcePostCameraAnimUpdate = 0;
				bDoAnimUpdate = false;
			}
		}

		if (bDoAnimUpdate)
		{
			pDynamicEntity->StartAnimUpdate(fwTimer::GetTimeStep());	
		}
		else
		{
			// In this case we won't be pushed onto the update queue, so we need to switch to regular updates.
			pDynamicEntity->StopUpdatingThroughAnimQueue();
		}
	}
}

void CGameWorld::SceneUpdateAfterAllMovementCb(fwEntity &entity, void *)
{
	if(entity.IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		return;

	Assert(((CEntity *) &entity)->GetIsDynamic());

	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;
	if (pDynamicEntity->GetAnimDirector() && pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>())
	{
		pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>()->UpdateSyncedSceneMover();
	}
}

void CGameWorld::SceneUpdateResetVisibilityCb(fwEntity &entity, void *)
{
	Assert(((CEntity *) &entity)->GetIsDynamic());

	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;

	// If we are not currently visible, reset the time stamp as if we had been seen
	// on the previous frame: this will maximize the time until we would get in trouble
	// for wrapping around (and before then, this code should have run again).
	if(!pDynamicEntity->GetIsVisibleInSomeViewportThisFrame())
	{
		pDynamicEntity->SetIsVisibleInSomeViewportThisFrame(false);
	}
}

void CGameWorld::SceneUpdateEndAnimUpdatePrePhysicsCb(fwEntity &entity, void *)
{
	if(entity.IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
	{
		return;
	}

	Assert(((CEntity *) &entity)->GetIsDynamic());
	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;

	pDynamicEntity->EndAnimUpdate(fwTimer::GetTimeStep());	
}

void CGameWorld::SceneUpdateUpdatePausedCb(fwEntity &entity, void *)
{
	if(IsEntityFrozen((CDynamicEntity*)&entity) || entity.IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		return;

	Assert(((CEntity *) &entity)->GetIsDynamic());

	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;
	pDynamicEntity->UpdatePaused();
}

void CGameWorld::SceneUpdateStartAnimUpdatePostCameraCb(fwEntity &entity, void *)
{
	if(Unlikely(IsEntityFrozen((CDynamicEntity*)&entity) || entity.IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD)))
	{
		return;
	}

	Assert(((CEntity *) &entity)->GetIsDynamic());

	CDynamicEntity* pDynamicEntity = (CDynamicEntity*)&entity;
	pDynamicEntity->StartAnimUpdateAfterCamera(fwTimer::GetTimeStep());
}

void CGameWorld::SceneUpdateUpdateCb(fwEntity &entity, void *userData)
{
	Assert(userData);
	SceneUpdateParameters *params = (SceneUpdateParameters *) userData;
	CEntity *pEntity = (CEntity *) &entity;

#if !__FINAL
	CGameWorld::ms_pProcessingEntity = pEntity;
#endif

	if (pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
	{
		if( params->iNumEntitiesToDelete < MAX_ENTITIES_TO_DELETE_PER_FRAME )
		{
			// Add the entity to a removal list rather than deleting it directly.
			// Deleting directly during this process can cause crashes.
			params->apEntitiesToDelete[params->iNumEntitiesToDelete] = pEntity;
			++(params->iNumEntitiesToDelete);
		}

		return;
	}

	// If the entity has been frozen 
	bool bFrozen = IsEntityFrozen(pEntity);
	if(bFrozen)
	{
		Assert(!CheckEntityFrozenByInterior(pEntity) || !NetworkUtils::IsNetworkClone(pEntity));
		pEntity->ProcessFrozen();
		return;
	}

#if __DEV
	if(params->bPrintProcessList)
		grcDebugDraw::AddDebugOutput("Processing %s", CModelInfo::GetBaseModelInfoName(entity.GetModelId()));
#endif

	if (Verifyf(!pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD), "CGameWorld::SceneUpdateUpdateCb - REMOVE_FROM_WORLD flag should already have been checked earlier in this function"))
	{
		Entity_PF_Start(pEntity);

		if( !debugBlockProcessControlOfEntity(pEntity) )  // always false in non __DEV build
		{ 
			// check for peds with NULL default tasks
#if __ASSERT
			if(pEntity->GetIsTypePed() && ((CPed *)pEntity)->GetPedIntelligence()->GetTaskDefault() == 0)
			{
				CNetObjPed *netobjPed = static_cast<CNetObjPed *>(NetworkUtils::GetNetworkObjectFromEntity(pEntity));
				Assertf(0, "%s has no default task!", netobjPed ? (const char *)netobjPed->GetLogName() : "Non-networked ped");
			}
#endif // __ASSERT

			netObject *pNetworkObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

			bool bRequiresProcessControl = false;

			if (pNetworkObject)
				bRequiresProcessControl = static_cast<CNetObjEntity *>(pNetworkObject)->ProcessControl();

			if (!NetworkUtils::IsNetworkClone(pEntity) REPLAY_ONLY(&& CReplayMgr::ShouldProcessControl(pEntity)))
			{
				bRequiresProcessControl = pEntity->ProcessControl();
			}

#if __BANK
            if (pEntity->GetIsTypePed())
			{
				CPed* pPed = (CPed*)pEntity;
				pPed->ProcessHeadBlendDebug();
				pPed->ProcessStreamingDebug();
			}
#endif // __BANK

			// ProcessControl should return false if it doesn't need to be on the process control list any more
			// most objects don't need to when they come to rest
			if(!bRequiresProcessControl)
			{
				if(params->iNumEntitiesToRemoveFromProcCont < MAX_ENTITIES_TO_REMOVE_FROM_PROCCONT_LIST)
				{
					Assert(pEntity->GetIsDynamic());
					params->apEntitiesToRemoveFromProcContList[params->iNumEntitiesToRemoveFromProcCont] = (CDynamicEntity*)pEntity;
					params->iNumEntitiesToRemoveFromProcCont++;
				}
			}
		}

		Entity_PF_Stop(pEntity);
	}
}

void CGameWorld::SceneUpdateUpdatePerceptionCb(fwEntity &entity, void *userData)
{
	CPed *pPed = (CPed*)&entity;
	if(pPed->PopTypeIsMission())
	{
		FastAssert(userData);
		float SenseRange = *((float *) userData);

		pPed->GetPedIntelligence()->GetPedPerception().SetSeeingRange(SenseRange);
		pPed->GetPedIntelligence()->GetPedPerception().SetHearingRange(SenseRange);
	}
}

void CGameWorld::SceneUpdatePreSimPhysicsUpdateCb(fwEntity &entity, void *userData)
{
	CPhysics::PreSimControl *preSimControl = (CPhysics::PreSimControl *) userData;
	CPhysical *pEntity = (CPhysical*) &entity;
	Assert(pEntity->GetIsPhysical());

	FastAssert(preSimControl);

	if(!pEntity->m_nDEflags.bFrozenByInterior)
	{
		// if we run out of space in the postponed list, then stop entities from postponing. Note: Add in 2nd postpone entities to preserve old behaviour (max is always less than MAX_POSTPONED_ENTITIES).
		bool bCanPostpone = (bool)( (preSimControl->nNumPostponedEntities + preSimControl->nNumPostponedEntities2) < CPhysics::PreSimControl::MAX_POSTPONED_ENTITIES);

		ePhysicsResult nReturn = pEntity->ProcessPhysics(preSimControl->fTimeStep, bCanPostpone, preSimControl->nSlice);

		// if entity was postponed then add it to the list
		if(nReturn == PHYSICS_POSTPONE)
		{
			//Assert(nNumPostponedEntities < MAX_POSTPONED_ENTITIES);
			if(preSimControl->nNumPostponedEntities < CPhysics::PreSimControl::MAX_POSTPONED_ENTITIES)
			{
				preSimControl->aPostponedEntities[preSimControl->nNumPostponedEntities] = pEntity;
				(preSimControl->nNumPostponedEntities)++;
			}
		}
		else if (nReturn == PHYSICS_NEED_SECOND_PASS)
		{
			Assert(preSimControl->nNumSecondPassEntities < CPhysics::PreSimControl::GetMaxSecondPassEntities());
			preSimControl->aSecondPassEntities[(preSimControl->nNumSecondPassEntities)++] = pEntity;
		}
		else if (nReturn == PHYSICS_POSTPONE2)
		{
			//Assert(nNumPostponedEntities2 < MAX_POSTPONED_ENTITIES);
			if(preSimControl->nNumPostponedEntities2 < CPhysics::PreSimControl::MAX_POSTPONED_ENTITIES)
			{
				preSimControl->aPostponedEntities2[preSimControl->nNumPostponedEntities2] = pEntity;
				(preSimControl->nNumPostponedEntities2)++;
			}
		}
        else
        {
            if(CPhysics::GetIsFirstTimeSlice(preSimControl->nSlice))
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

		// need to prepare the no collision needs reset, and reset here if it wasn't reset from the last frame
		if(pEntity->GetIsPhysical() && ((CPhysical*)pEntity)->GetNoCollisionEntity())
		{
			((CPhysical*)pEntity)->PrepareNoCollisionReset();
		}
	}
}

void CGameWorld::SceneUpdatePhysicsPreUpdateCb(fwEntity &entity, void *)
{
	CPhysical *pEntity = (CPhysical*) &entity;
	Assert(pEntity->GetIsPhysical());

	pEntity->ProcessPrePhysics();
}

void CGameWorld::SceneUpdatePhysicsPostUpdateCb(fwEntity &entity, void *)
{
	CPhysical *pEntity = (CPhysical*) &entity;
	Assert(pEntity->GetIsPhysical());

	pEntity->ProcessPostPhysics();
}

// Flag whether dynamic objects are visible in any active view ports this
// frame, and record the distance to the closest one if so.
void CGameWorld::ProcessDynamicObjectVisibility()
{
	PF_AUTO_PUSH_TIMEBAR("ProcessDynamicObjectVisibility");

	// We use an eight bit frame counter in CDynamicEntity to avoid having
	// to reset anything every frame. It will wrap around though, so if we are
	// not careful, entities that are not visible could appear to be visible for
	// one frame. To avoid that, we go through the entities and rejuvenate the
	// frame counts. This may take 0.1 ms or so, but since it only happens once
	// per several seconds, it shouldn't be bad compared to what we were doing before.
	if(!ms_DynEntVisCountdownToRejuvenate)
	{
		fwSceneUpdate::Update(SU_RESET_VISIBILITY);

		ms_DynEntVisCountdownToRejuvenate = 253;
	}
	else
	{
		ms_DynEntVisCountdownToRejuvenate--;
	}
}



#if __ASSERT
// Check Linkage
void CheckLinkage(CPhysical* pPhysical)
{
	fwAttachmentEntityExtension *extension = pPhysical->GetAttachmentExtension();

	if(extension && extension->GetIsRootParentAttachment())
	{
		fwEntity *pThisChild = extension->GetChildAttachment();
		if(pThisChild)
		{
			// Get the childs parent
			fwAttachmentEntityExtension *pChildExtension = pThisChild->GetAttachmentExtension();
			fwEntity *pChildParent = pChildExtension->GetAttachParentForced();

			const char *pParentName = pPhysical->GetModelName();
			const char *pChildName = pThisChild->GetModelName();
			const char *pChildParentName = pChildParent->GetModelName();

			if( pChildParent != pPhysical )
			{
				Printf("Linkage is broken, Child doesn't have correct Parent!!\n");
				Printf("Root %s, child %s, parent %s\n", pParentName, pChildName, pChildParentName);
			}
		}
	}
}

#endif	//__ASSERT



void CGameWorld::ProcessDelayedAttachmentsForPhysical(const CPhysical *const pPhysical)
{
	for(int iAttachIndex = 0; iAttachIndex < ms_nNumDelayedRootAttachments; iAttachIndex++)
	{
		if(ms_delayedRootAttachments[iAttachIndex] && ms_delayedRootAttachments[iAttachIndex] == pPhysical)
		{
			ms_delayedRootAttachments[iAttachIndex]->ProcessAllAttachments();

			// NULL out this entry so that it won't get processed again in ProcessAfterPreRender.
			ms_delayedRootAttachments[iAttachIndex] = NULL;

			break;
		}
	}
}


// Name			:	ProcessAttachmentAfterAllMovement
// Purpose		:	Process physical attachment if possible or add to list for processing later
// Paramaters	:	pPhysical: The physical to process
// Returns		:	none

void CGameWorld::ProcessAttachmentAfterAllMovement(CPhysical* pPhysical)
{
	fwAttachmentEntityExtension *extension = pPhysical->GetAttachmentExtension();

	if(pPhysical->m_nPhysicalFlags.bIsRootOfOtherAttachmentTree)
	{
		pPhysical->ProcessOtherAttachments();
	}

	if(extension && extension->GetIsRootParentAttachment())
	{
		if(extension->GetRequiresPostPreRender())
		{
			// This is a complex attachment and needs up to date skeletons before processing
			// so delay it for now
#if __ASSERT
			if ( ms_nNumDelayedRootAttachments >= MAX_NUM_POST_PRE_RENDER_ATTACHMENTS )
			{
				char buffer[512];
				diagLoggedPrintf( "MAX_NUM_POST_PRE_RENDER_ATTACHMENTS\n"
								  "-----------------------------------\n");
				for ( int i = 0; i < MAX_NUM_POST_PRE_RENDER_ATTACHMENTS; ++i )
				{
					CPhysical *pStorePhysical = ms_delayedRootAttachments[i];
					if ( pStorePhysical )
					{
						fwAttachmentEntityExtension *pPhysExtension = pStorePhysical->GetAttachmentExtension();

						fwEntity *pfwParentResult = pPhysExtension->GetAttachParentForced();
						fwEntity *pfwChildResult = pPhysExtension->GetChildAttachment();
						fwEntity *pfwSiblingResult = pPhysExtension->GetSiblingAttachment();

						const char *pParentString = "NULL";
						if(pfwParentResult)
							pParentString = pfwParentResult->GetModelName();
						const char *pChildString = "NULL";
						if(pfwChildResult)
							pChildString = pfwChildResult->GetModelName();
						const char *pSiblingString = "NULL";
						if(pfwSiblingResult)
							pSiblingString = pfwSiblingResult->GetModelName();

						sprintf( buffer, "[%2.2d] 0x%p : \"%s\" on (Parent)0x%p : \"%s\" (Child) 0x%p : \"%s\" (Sibling) 0x%p : \"%s\"\n", i, pStorePhysical, pStorePhysical->GetDebugName(), 
							pfwParentResult, pParentString,
							pfwChildResult, pChildString,
							pfwSiblingResult, pSiblingString);


						diagLoggedPrintf( "%s", buffer );

					}
				}

				for ( int i = 0; i < MAX_NUM_POST_PRE_RENDER_ATTACHMENTS; ++i )
				{
					CPhysical *pStorePhysical = ms_delayedRootAttachments[i];
					if ( pStorePhysical )
					{
						CheckLinkage( pStorePhysical );
					}
				}

			}
#endif
			if(Verifyf((ms_nNumDelayedRootAttachments < MAX_NUM_POST_PRE_RENDER_ATTACHMENTS),"Run out of space to store delayed attachments - see TTY for list"))
			{
				ms_delayedRootAttachments[ms_nNumDelayedRootAttachments++] = pPhysical;
				// don't return as attachments still need to be updated for camera
				//return;
			}
		}

		pPhysical->ProcessAllAttachments();
		return;
	}
}


// Name			:	FindObjectsInRange
// Purpose		:	Generates a list of entities within a certain range of a certain coordinate.
// Parameters	:
// Returns		:	none

void CGameWorld::FindObjectsInRange(const Vector3 &Coors, float Range, bool UNUSED_PARAM(Dist2D), s32 *pNum, s32 MaxNum, CEntity **ppResults, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies)
{
	CFindData findData(ppResults, MaxNum);

	s32 iTypeFlags = 0;
	if( bCheckBuildings )	{iTypeFlags |= ENTITY_TYPE_MASK_BUILDING;}
	if( bCheckVehicles )	{iTypeFlags |= ENTITY_TYPE_MASK_VEHICLE;}
	if( bCheckPeds )		{iTypeFlags |= ENTITY_TYPE_MASK_PED;}
	if( bCheckObjects )		{iTypeFlags |= ENTITY_TYPE_MASK_OBJECT;}
	if( bCheckDummies )		{iTypeFlags |= ENTITY_TYPE_MASK_DUMMY_OBJECT;}

	s32 iControlFlags = SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS;
	fwIsSphereInside intersection(spdSphere(RCC_VEC3V(Coors), ScalarV(Range)));
	// Scan through all intersecting entities finding all entities before
	CGameWorld::ForAllEntitiesIntersecting( &intersection,
		CGameWorld::FindEntitiesCB,
		&findData,
		iTypeFlags,
		iControlFlags );

	if( pNum )
	{
		*pNum = findData.GetNumEntities();
	}
}



// Name			:	FindObjectsKindaColliding
// Purpose		:	Generates a list of entities that we collide
//					with (only taking into account the bounding
//					sphere of that object).
// Parameters	:
// Returns		:	none

void CGameWorld::FindObjectsKindaColliding(const Vector3 &Coors, float Range, bool useBoundBoxes, bool UNUSED_PARAM(Dist2D), s32 *pNum, s32 MaxNum, CEntity **ppResults, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies)
{
	spdSphere testSphere(RCC_VEC3V(Coors), ScalarV(Range));
	CFindData findData(ppResults, MaxNum);

	s32 iTypeFlags = 0;
	if( bCheckBuildings )	{iTypeFlags |= ENTITY_TYPE_MASK_BUILDING;}
	if( bCheckVehicles )	{iTypeFlags |= ENTITY_TYPE_MASK_VEHICLE;}
	if( bCheckPeds )		{iTypeFlags |= ENTITY_TYPE_MASK_PED;}
	if( bCheckObjects )		{iTypeFlags |= ENTITY_TYPE_MASK_OBJECT;}
	if( bCheckDummies )		{iTypeFlags |= ENTITY_TYPE_MASK_DUMMY_OBJECT;}

	s32 iControlFlags = SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS;
	s32 iOptionFlags = 0;
	if (!bCheckBuildings && !bCheckDummies)
	{
		iOptionFlags |= SEARCH_OPTION_DYNAMICS_ONLY;
	}

	if( useBoundBoxes )
	{
		fwIsSphereIntersectingBBApprox intersection(testSphere);
		// Scan through all intersecting entities finding all entities before
		ForAllEntitiesIntersecting( &intersection,
			CGameWorld::FindEntitiesCB,
			&findData,
			iTypeFlags,
			iControlFlags,
			SEARCH_LODTYPE_HIGHDETAIL,
			iOptionFlags);
	}
	else
	{
		fwIsSphereIntersecting intersection(testSphere);

		// Scan through all intersecting entities finding all entities before
		ForAllEntitiesIntersecting( &intersection,
			CGameWorld::FindEntitiesCB,
			&findData,
			iTypeFlags,
			iControlFlags,
			SEARCH_LODTYPE_HIGHDETAIL,
			iOptionFlags);
	}

	if( pNum )
	{
		*pNum = findData.GetNumEntities();
	}
}

// Name			:	FindObjectsIntersectingCube
// Purpose		:	Generates a list of entities that at least
//					intersect with a cube
// Parameters	:
// Returns		:	none

void CGameWorld::FindObjectsIntersectingCube(Vector3::Param CoorsMin, Vector3::Param CoorsMax, s32 *pNum, s32 MaxNum, CEntity **ppResults, bool bCheckBuildings, bool bCheckVehicles, bool bCheckPeds, bool bCheckObjects, bool bCheckDummies)
{
	CFindData findData(ppResults, MaxNum);

	s32 iTypeFlags = 0;
	if( bCheckBuildings )	{iTypeFlags |= ENTITY_TYPE_MASK_BUILDING;}
	if( bCheckVehicles )	{iTypeFlags |= ENTITY_TYPE_MASK_VEHICLE;}
	if( bCheckPeds )		{iTypeFlags |= ENTITY_TYPE_MASK_PED;}
	if( bCheckObjects )		{iTypeFlags |= ENTITY_TYPE_MASK_OBJECT;}
	if( bCheckDummies )		{iTypeFlags |= ENTITY_TYPE_MASK_DUMMY_OBJECT;}

	if (pNum) *pNum = 0;

	s32 iOptionFlags = 0;
	if (!bCheckBuildings && !bCheckDummies)
	{
		iOptionFlags |= SEARCH_OPTION_DYNAMICS_ONLY;
	}

	s32 iControlFlags = SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS;
	fwIsBoxIntersectingApprox intersection(spdAABB(RCC_VEC3V(CoorsMin), RCC_VEC3V(CoorsMax)));
	// Scan through all intersecting entities finding all entities before
	ForAllEntitiesIntersecting( &intersection,
		CGameWorld::FindEntitiesCB,
		&findData,
		iTypeFlags,
		iControlFlags,
		SEARCH_LODTYPE_HIGHDETAIL,
		iOptionFlags);

	if( pNum )
	{
		*pNum = findData.GetNumEntities();
	}

}

#if __BANK
#define OutOfMapDebug(fmt,...)		{ if (ms_detailedOutOrMapSpew) Displayf( "[out of map] " fmt, ##__VA_ARGS__ ); }
#else
#define OutOfMapDebug(fmt,...)
#endif

// Name			:	RemoveFallenPeds
// Purpose		:	Removes peds that have fallen through the map and are about to crash the game.
// Parameters	:
// Returns		:
#define LOWESTPEDZ (-200.0f)
#define PEDS_TO_RETURN (192)

void CGameWorld::RemoveFallenPeds()
{
	CSpatialArrayNode* results[PEDS_TO_RETURN];
	int numFound = CPed::ms_spatialArray->FindBelowZ(ScalarV(LOWESTPEDZ), results, PEDS_TO_RETURN);
	CPed::Pool* pPedPool = CPed::GetPool(); 
	for(int i = 0; i < numFound; i++)
	{
		CPed* pPed = CPed::GetPedFromSpatialArrayNode(results[i]);
		if(pPed && pPedPool->IsValidPtr(pPed) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			CGameWorld::RemoveOutOfMapPed(pPed, false);
		}
	}
}

// NAME : RemoveOutOfMapPed
// PURPOSE : Deal with rescue or removal of peds who have fallen through the world
// We can identify ped who fell from interiors, so can use this to help locate script errors.
void CGameWorld::RemoveOutOfMapPed(CPed* pPed, bool ASSERT_ONLY(bOutOfInterior))
{
	const Vector3 vOriginalPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(pPed->IsNetworkClone() || (pPed->GetNetworkObject() && !pPed->GetNetworkObject()->CanDelete()))
	{
		// if this is a network clone we don't let it fall below the lowest position while
		// we wait for a network update once the vehicle has been repositioned on the owning machine
		Vector3 newPedPos = vOriginalPedPosition;
		newPedPos.z = LOWESTPEDZ;

		// need to set ragdolls back to animated before you can warp their position
		if(pPed->GetUsingRagdoll())
			pPed->SwitchToAnimated();

		pPed->SetPosition(newPedPos, true, true, true);
		netObject  *networkObject = pPed->GetNetworkObject();
		netBlender *blender    = networkObject ? networkObject->GetNetBlender() : 0;

		if(blender)
		{
			blender->Reset();
		}
	}
	else
	{
#if !__FINAL
		const char *name = pPed->GetModelName();
		if(pPed->GetNetworkObject())
		{
			name = pPed->GetNetworkObject()->GetLogName();
		}
		Displayf("&&&&&&Another ped %s has fallen through the map&&&&&&&&&& %f %f %f\n",
			name,
			vOriginalPedPosition.x, vOriginalPedPosition.y, vOriginalPedPosition.z);
#endif

		StatsInterface::FallenThroughTheMap(pPed);

		// This ped is to be placed back on the map.
		if (pPed->PopTypeIsRandomNonPermanent() && (!pPed->IsPlayer()))
		{
			// Just remove this guy
			CPedFactory::GetFactory()->DestroyPed(pPed);
		}
		else
		{
			// Road nodes are probably our safest positions to use, since they've been
			// placed by hand and will always be in sensible locations
			CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(vOriginalPedPosition);
			if (NearestNode.IsEmpty())
			{
				// Failing that check the navmesh for a position.  If this is the player, then it
				// will probably work since navmeshes stream in around his position.
				Vector3 vNear(vOriginalPedPosition.x, vOriginalPedPosition.y, 0.0f);
				Vector3 vSafePos;
				if(CPathServer::GetClosestPositionForPed(vNear, vSafePos, 10000.0f) != ENoPositionFound)
				{
					pPed->Teleport(vSafePos, pPed->GetCurrentHeading());
					pPed->SetVelocity(ORIGIN);
				}
				else
				{
#if __ASSERT
					if(pPed->PopTypeIsMission())
					{
						const char * networkName = pPed->GetDebugName(); //debug name resolves to network name if we have one.
						const char * modelName = pPed->GetModelName();
						const char * debugName = pPed->m_debugPedName ? pPed->m_debugPedName : "(none)";

						const char* scriptName = "unknown script";

						const CGameScriptId* scriptId = pPed->GetScriptThatCreatedMe();
						if (scriptId)
						{
							scriptName = scriptId->GetScriptName();
						}

						if(bOutOfInterior)
						{
							Assertf(false, "Mission ped (name:%s, model:%s, network:%s) has fallen out of an interior.  Ped owned by %s  Coords (%.1f,%.1f,%.1f)", debugName, modelName, networkName, scriptName, vOriginalPedPosition.x, vOriginalPedPosition.y, vOriginalPedPosition.z);
						}
						else
						{
							Assertf(false, "Mission ped (name:%s, model:%s, network:%s) has fallen through the bottom of the map.  Ped owned by %s  Coords (%.1f,%.1f,%.1f)", debugName, modelName, networkName, scriptName, vOriginalPedPosition.x, vOriginalPedPosition.y, vOriginalPedPosition.z);
						}
					}
#endif
					// Final chance to place the ped some where sensible.
					// This code just teleports the ped a few meters above the low cutoff position, at which point he
					// will fall below this cutoff again and thus come back into this code.

					pPed->Teleport(Vector3(vOriginalPedPosition.x, vOriginalPedPosition.y, LOWESTPEDZ + 5.0f), pPed->GetCurrentHeading());
					pPed->SetVelocity(VEC3_ZERO);
				}
			}
			else
			{
				// Road node found
				Vector3 nodePos(0.0f, 0.0f, 0.0f);
				ThePaths.FindNodePointer(NearestNode)->GetCoors(nodePos);
				nodePos.z += 1.0f;
				pPed->Teleport(nodePos, pPed->GetCurrentHeading());
				pPed->SetVelocity(ORIGIN);
			}
		}
	}
}

// Name			:	RemoveFallenCars
// Purpose		:	Removes cars that have fallen through the map and are about to crash the game.
// Parameters	:
// Returns		:
#define LOWESTCARZ (-200.0f)
#define CARS_TO_RETURN (192)

void CGameWorld::RemoveFallenCars()
{
	CSpatialArrayNode* results[CARS_TO_RETURN];
	int numFound = CVehicle::ms_spatialArray->FindBelowZ(ScalarV(LOWESTCARZ), results, CARS_TO_RETURN);
	CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
	for(int i = 0;i < numFound; i++)
	{
		CVehicle* pVehicle = CVehicle::GetVehicleFromSpatialArrayNode(results[i]);
		if(pVehicle && pVehiclePool->IsValidPtr(pVehicle) && !(pVehicle->GetIsInWater() && pVehicle->ContainsPlayer()))
			CGameWorld::RemoveOutOfMapVehicle( pVehicle );
	}
}

void CGameWorld::RemoveOutOfMapVehicle(CVehicle* pVehicle)
{
	const Vector3 vOriginalVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

#if !__NO_OUTPUT
	const char *name = "vehicle";
	if(pVehicle->GetNetworkObject())
	{
		name = pVehicle->GetNetworkObject()->GetLogName();
	}
	Displayf("&&&&&&Another %s has fallen through the map&&&&&&&&&& %f %f %f\n", name, vOriginalVehiclePosition.x, vOriginalVehiclePosition.y, vOriginalVehiclePosition.z);
#endif

	OutOfMapDebug( "Vehicle %s (debug name) %s has fallen off the map", pVehicle->GetModelName(), pVehicle->GetDebugName() );

	if(pVehicle->IsNetworkClone() || (pVehicle->GetNetworkObject() && !pVehicle->GetNetworkObject()->CanDelete()))
	{
		// if this is a network clone we don't let it fall below the lowest position while
		// we wait for a network update once the vehicle has been repositioned on the owning machine
		Vector3 newCarPos = vOriginalVehiclePosition;
		newCarPos.z = LOWESTCARZ;
		OutOfMapDebug( "Network vehicle, teleporting back to (%f,%f,%f)", newCarPos.x, newCarPos.y, newCarPos.z );
		pVehicle->SetPosition(newCarPos, true, true, true);
		netObject  *networkObject = pVehicle->GetNetworkObject();
		netBlender *blender    = networkObject ? networkObject->GetNetBlender() : 0;

		if(blender)
		{
			blender->Reset();
		}
	}
	else
	{
		OutOfMapDebug( "Teleport / destroy conditions:" );
		OutOfMapDebug( "PopTypeIsMission: %s", pVehicle->PopTypeIsMission() ? "true" : "false" );
		OutOfMapDebug( "RenderScorched: %s", pVehicle->m_nPhysicalFlags.bRenderScorched ? "true" : "false" );
		OutOfMapDebug( "PlayerVehicle: %s", (pVehicle == FindPlayerVehicle()) ? "true" : "false" );
		OutOfMapDebug( "HasMissionChars: %s", pVehicle->HasMissionCharsInIt() ? "true" : "false" );
		OutOfMapDebug( "DriverIsPlayer: %s", (pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer()) ? "true" : "false" );
		OutOfMapDebug( "PopTypeIsRandom: %s", pVehicle->PopTypeIsRandom() ? "true" : "false" );

#if __ASSERT
		const char* scriptName = "unknown script";

		const CGameScriptId* scriptId = pVehicle->GetScriptThatCreatedMe();
		if (scriptId)
		{
			scriptName = scriptId->GetScriptName();
		}
#endif	//	__ASSERT

		if( (pVehicle->PopTypeIsMission() && !pVehicle->m_nPhysicalFlags.bRenderScorched) || pVehicle == FindPlayerVehicle() || pVehicle->HasMissionCharsInIt() ||
			(pVehicle->GetDriver() && pVehicle->GetDriver()->IsLocalPlayer()) )
		{
			CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(vOriginalVehiclePosition);

#if __DEV
			static int s_NumOutOfWorldDumps = 0;
			if(s_NumOutOfWorldDumps++ < 10)
			{
				pVehicle->DumpOutOfWorldInfo();
			}
#endif // __DEV
			

			if (NearestNode.IsEmpty())
			{
				Assertf(false,"Vehicle (%p) (%s)(%s) has fallen through the world. Failed to find a nearby path node so warping to <<%f, %f, %f>> : pop(%s) : scorched(%d) : driveable(%d) : playerVehicle(%d) : hasMissionChar(%d), playerDriver(%d), created by %s, AllowFreezeIfNoCollision(%d), ShouldFixIfNoCollision(%d), Collider(%d), ShouldFixIfNoCollisionLoadedAroundPosition(%d)", 
					pVehicle, pVehicle->GetModelName(), pVehicle->GetDebugName(),
					vOriginalVehiclePosition.x, vOriginalVehiclePosition.y, 0.0f, 
					CTheScripts::GetPopTypeName(pVehicle->PopTypeGet()), pVehicle->m_nPhysicalFlags.bRenderScorched, pVehicle->m_nVehicleFlags.bIsThisADriveableCar, pVehicle == FindPlayerVehicle(), pVehicle->HasMissionCharsInIt(),
					(pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer()), scriptName,
					pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision, pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision,
					pVehicle->GetCollider() != NULL, pVehicle->ShouldFixIfNoCollisionLoadedAroundPosition());

				OutOfMapDebug( "Nearest node empty, teleporting vehicle to (%f %f %f)", vOriginalVehiclePosition.x, vOriginalVehiclePosition.y, 0.0f );
				pVehicle->Teleport(Vector3(vOriginalVehiclePosition.x, vOriginalVehiclePosition.y, 0.0f), 0.0f);
			}
			else
			{
				Vector3 nodePos(0.0f, 0.0f, 0.0f);
				ThePaths.FindNodePointer(NearestNode)->GetCoors(nodePos);
				nodePos.z += 3.0f;

				Assertf(false,"Vehicle (%p) (%s)(%s)(%s) has fallen through the world. Warping to nearby path node <<%f, %f, %f>> : pop(%s) : scorched(%d) : driveable(%d) : playerVehicle(%d) : hasMissionChar(%d), playerDriver(%d), created by %s, AllowFreezeIfNoCollision(%d), ShouldFixIfNoCollision(%d), Collider(%d), ShouldFixIfNoCollisionLoadedAroundPosition(%d)", 
					pVehicle, pVehicle->GetModelName(), pVehicle->GetDebugName(), 
					pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "",
					nodePos.x, nodePos.y, nodePos.z, 
					CTheScripts::GetPopTypeName(pVehicle->PopTypeGet()), pVehicle->m_nPhysicalFlags.bRenderScorched, pVehicle->m_nVehicleFlags.bIsThisADriveableCar, pVehicle == FindPlayerVehicle(), pVehicle->HasMissionCharsInIt(),
					(pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer()), scriptName, 
					pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision, pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision,
					pVehicle->GetCollider() != NULL, pVehicle->ShouldFixIfNoCollisionLoadedAroundPosition());

				OutOfMapDebug( "Nearest node found, teleporting vehicle to (%f %f %f)", nodePos.x, nodePos.y, nodePos.z );
				pVehicle->Teleport(nodePos, 0.0f, false);
			}

			pVehicle->SetVelocity(ORIGIN);
		}
		else if (pVehicle->PopTypeIsRandom())
		{
			OutOfMapDebug( "Destroying vehicle" );
			CVehicleFactory::GetFactory()->Destroy(pVehicle);
		} 
		else if ( pVehicle->PopTypeGet() == POPTYPE_REPLAY )
		{
			OutOfMapDebug( "Replay following out of world vehicle - will follow path of original recorded vehicle");
		} else
		{
			OutOfMapDebug( "Vehicle not teleported nor destroyed" );

			// script shouldn't maintain refs to burnt out vehicles, otherwise this can happen:
			Assertf(false,"Vehicle (%p) removal not handled (%s)(%s) : pop(%s) : scorched(%d) : driveable(%d) : playerVehicle(%d) : hasMissionChar(%d), driver(%s), playerDriver(%d), created by %s, AllowFreezeIfNoCollision(%d), ShouldFixIfNoCollision(%d), status(%d), cloned(%d)", 
				pVehicle, pVehicle->GetModelName(), pVehicle->GetDebugName(),
				CTheScripts::GetPopTypeName(pVehicle->PopTypeGet()), pVehicle->m_nPhysicalFlags.bRenderScorched, pVehicle->m_nVehicleFlags.bIsThisADriveableCar, pVehicle == FindPlayerVehicle(), pVehicle->HasMissionCharsInIt(),
				(pVehicle->GetDriver() ? pVehicle->GetDriver()->GetDebugName() : "0"), (pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer()), scriptName,
				pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision, pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision,
				pVehicle->GetStatus(), pVehicle->IsNetworkClone());
		}
	}
}

// Name			:	RemoveFallenObjects
// Purpose		:	Removes objects that have fallen through the map and are about to crash the game.
// Parameters	:
// Returns		:
#define LOWESTOBJECTZ (-200.0f)
#define FRAMES_TO_LOOP_THROUGH_OBJECTS (25)
void CGameWorld::RemoveFallenObjects()
{
	CObject::Pool *pObjectPool = CObject::GetPool();

	static s32 currentObject = (s32) pObjectPool->GetSize() - 1;

	s32 numObjects = (s32) pObjectPool->GetNoOfUsedSpaces();
	
	if(numObjects <= 0)
		return; // no objects to check

	s32 objsToProcess = MAX(numObjects / FRAMES_TO_LOOP_THROUGH_OBJECTS, 1); // timeslice the full slots, always at least one - since we know there are more than 0 objects in the pool

	while(objsToProcess)
	{
		CObject *pObject = pObjectPool->GetSlot(currentObject);
		if(pObject)
		{
			objsToProcess--;// this is a real object
			const Vector3 vOriginalObjectPosition = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
			if (vOriginalObjectPosition.z < LOWESTOBJECTZ)
				CGameWorld::RemoveOutOfMapObject( pObject );
		}

		currentObject--;
		if(currentObject<0)
		{
			// wrap pool
			currentObject = (s32) pObjectPool->GetSize() - 1;
		}
	}
}

void CGameWorld::RemoveOutOfMapObject(CObject* pObject)
{
	const Vector3 vOriginalObjectPosition = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());

	if(pObject->IsNetworkClone())
	{
		// if this is a network clone we don't let it fall below the lowest position while
		// we wait for a network update once the object has been repositioned on the owning machine
		Vector3 newObjectPos = vOriginalObjectPosition;
		newObjectPos.z = LOWESTOBJECTZ;

		// B*1854957: This assert is suppressed for object teleports below so it makes sense to do it here too.
		fwAttachmentEntityExtension* pExtension = pObject->GetAttachmentExtension();
		if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true); // Necessary to stop spurious assert in fwEntity::SetMatrix().

		pObject->SetPosition(newObjectPos, true, true, true);
		netObject  *networkObject = pObject->GetNetworkObject();
		netBlender *blender    = networkObject ? networkObject->GetNetBlender() : 0;

		if(blender)
		{
			blender->Reset();
		}
	}
	else
	{
#if !__FINAL
		const char *name = pObject->GetModelName();
		if(pObject->GetNetworkObject())
		{
			name = pObject->GetNetworkObject()->GetLogName();
		}
		Displayf("&&&&&&Another Object %s has fallen through the map&&&&&&&&&& %f %f %f\n",
			name,
			vOriginalObjectPosition.x, vOriginalObjectPosition.y, vOriginalObjectPosition.z);
#endif
		// This Object is to be placed back on the map.
		if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM)
		{
			// If there is a related dummy not attached to a streamed IPL then delete it
			CDummyObject* pDummy = pObject->GetRelatedDummy();
			if(pDummy && pDummy->GetIplIndex() == 0)
				delete pDummy;
			pObject->ClearRelatedDummy();
			CObjectPopulation::DestroyObject(pObject);
		}
		else if (pObject->GetAsProjectile())
		{
			static_cast<CProjectile*>(pObject)->Destroy();
		}
		else if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_TEMP && pObject->CanBeDeleted())
		{
			CObjectPopulation::DestroyObject(pObject);
		}
		else
		{
			// if this is a weapon that has an owner just ignore it
			// when the owner is moved they'll take the weapon with them
			if( pObject->GetWeapon() &&
				pObject->GetWeapon()->GetOwner() )
			{
				return;
			}

			CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(vOriginalObjectPosition);
			if (NearestNode.IsEmpty())
			{
				// This can happen when nodes aren't streamed in or there are no Object nodes nearby
				fwAttachmentEntityExtension* pExtension = pObject->GetAttachmentExtension();
				if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true); // Necessary to stop spurious assert in fwEntity::SetMatrix().
				pObject->Teleport(Vector3(vOriginalObjectPosition.x, vOriginalObjectPosition.y, LOWESTOBJECTZ + 5.0f));
				if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
				pObject->SetVelocity(ORIGIN);
			}
			else
			{
				Vector3 nodePos(0.0f, 0.0f, 0.0f);
				ThePaths.FindNodePointer(NearestNode)->GetCoors(nodePos);
				nodePos.z = 2.0f;
				fwAttachmentEntityExtension* pExtension = pObject->GetAttachmentExtension();
				if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true); // Necessary to stop spurious assert in fwEntity::SetMatrix().
				pObject->Teleport(nodePos);
				if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
				pObject->SetVelocity(ORIGIN);
			}
		}
	}
}

void CGameWorld::RemoveEntity(CEntity *pEntityToRemove)
{
	Assertf(pEntityToRemove, "RemoveEntity - Entity doesn't exist");

	switch (pEntityToRemove->GetType())
	{
	case ENTITY_TYPE_PED:
        {
            CPed *pPed = (CPed *) pEntityToRemove;
            if(pPed->IsPlayer())
            {
                CPedFactory::GetFactory()->DestroyPlayerPed(pPed);
            }
            else
            {
		        CPedFactory::GetFactory()->DestroyPed(pPed, true);
            }
        }
		break;
	case ENTITY_TYPE_OBJECT:
		{
			CObject* pObject = (CObject*)pEntityToRemove;
			pObject->ClearRelatedDummy();
			CObjectPopulation::DestroyObject(pObject);
		}
		break;
	case ENTITY_TYPE_VEHICLE:
		{
			CVehicle *pVehicle = (CVehicle *)pEntityToRemove;
			CVehicleFactory::GetFactory()->Destroy(pVehicle);
		}
		break;

	default:
		CGameWorld::Remove(pEntityToRemove);
		delete pEntityToRemove;
		break;
	}
}


//
// name:		ClearExcitingStuffFromArea
// description:	Clear peds/cars/fires/projectiles etc from area
void CGameWorld::ClearExcitingStuffFromArea(const Vector3 &Centre, float Radius, bool bRemoveProjectiles, bool bConvertObjectsWithBrainsToDummyObjects, bool bCheckInterestingVehicles, bool bLeaveCarGenCars, CObject *pExceptionObject, bool bClearLowPriorityPickupsOnly, bool bClearPedsAndVehs, bool bClampVehicleHealth )
{
	// Clear all peds
	if (bClearPedsAndVehs)
		ClearPedsFromArea(Centre, Radius);

	// Clear all vehicles
	if (bClearPedsAndVehs)
		ClearCarsFromArea(Centre, Radius, bCheckInterestingVehicles, bLeaveCarGenCars);

	if (bClearPedsAndVehs)
	{
		CTrain::RemoveTrainsFromArea(Centre, Radius);
	}

	// Delete all objects
	ClearObjectsFromArea(Centre, Radius, bConvertObjectsWithBrainsToDummyObjects, pExceptionObject);

	// Extinguish all fires in area
	g_fireMan.ExtinguishArea(RCC_VEC3V(Centre), Radius, true);

	// Clear all decals in area
	g_decalMan.RemoveInRange(RCC_VEC3V(Centre), ScalarV(Radius), NULL);

	ExtinguishAllCarFiresInArea(Centre, Radius, true, bClampVehicleHealth);

	// Remove all explosions in area
	CExplosionManager::RemoveExplosionsInArea(EXP_TAG_DONTCARE, Centre, Radius);

	spdAABB spdBounds = spdAABB(spdSphere(RCC_VEC3V(Centre), ScalarV(Radius)));

	// Remove all projectiles in the world
	// Also removes all shadows in the world (to get rid of pools of blood and scorch marks)
	if (bRemoveProjectiles)
	{
		ClearProjectilesFromArea(Centre,  Radius);
		//RAGE	CShadows::TidyUpShadows();
	}

	// Low priority pickups can also be removed
	// [SPHERE-OPTIMISE]
	CPickupManager::RemoveAllPickupsInBounds(spdBounds, bClearLowPriorityPickupsOnly);

	// What about objects?

	// Clear all shocking events in the area, except script-created ones.
	CShockingEventsManager::ClearEventsInSphere(RCC_VEC3V(Centre), Radius, true, false);

	// After we've cleared fire, give the incident manager a chance to clean up.
	// See B* 212897: "Fire Dept. call carried into replay." for a report of a bug
	// that this should help.
	CIncidentManager::GetInstance().ClearExcitingStuffFromArea(RCC_VEC3V(Centre), Radius);
}

void CGameWorld::ClearProjectilesFromArea(const Vector3 &Centre, float Radius)
{
	spdAABB spdBounds = spdAABB(spdSphere(RCC_VEC3V(Centre), ScalarV(Radius)));

	// Remove all projectiles in the world
	CProjectileManager::RemoveAllProjectilesInBounds(spdBounds);
	g_waterCannonMan.ClearAllWatercannonSprayInWorld();
}


#if !__NO_OUTPUT
void CGameWorld::LogClearCarFromArea(const char *pFunctionName, const CVehicle *pVehicle, const char *pReasonString)
{
	if(ms_bLogClearCarFromArea)
	{
		Displayf("%s - cannot delete vehicle with model %s at %.2f,%.2f,%.2f (Entity = 0x%p). Reason: %s", pFunctionName, pVehicle->GetModelName(), 
			pVehicle->GetTransform().GetPosition().GetXf(), pVehicle->GetTransform().GetPosition().GetYf(), pVehicle->GetTransform().GetPosition().GetZf(), 
			pVehicle, pReasonString);
	}
}
#endif	//	!__NO_OUTPUT


//	Return true if the vehicle was deleted, false if it wasn't
// For DISTANCE_CHECK_CENTRE_RADIUS, vecPoint1 is the Centre and vecPoint2 is ignored, fDistance is the radius
// For DISTANCE_CHECK_ANGLED_AREA, vecPoint1 is the first point defining a face of the volume, vecPoint1 is the second point, fDistance is the distance to the opposite face
bool CGameWorld::DeleteThisVehicleIfPossible(CVehicle *pVehicle, bool bOnlyDeleteCopCars, bool bTakeInterestingVehiclesIntoAccount, bool bAllowCopRemovalWithWantedLevel, eTypeOfDistanceCheck DistanceCheckType, Vector3 &vecPoint1, float fDistance, Vector3 &vecPoint2)
{
	if (bOnlyDeleteCopCars && !pVehicle->IsLawEnforcementVehicle())
	{
		return false;
	}

#if __BANK
	if(pVehicle->InheritsFromTrain() && CVehicleFactory::ms_bLogDestroyedVehicles)
	{
		Displayf("**************************************************************************************************");
		Displayf("CGameWorld::DeleteThisVehicleIfPossible() is about to delete a CTrain..");
		Displayf("  this could leave a train without an engine, or otherwise mess up train/carriage connections.");
		Displayf("**************************************************************************************************");
	}
#endif

	//Test if anyone in the vehicle is in the player's group.
	bool bIsInPlayerGroup=false;
	CPedGroup* pPlayerGroup=CGameWorld::FindLocalPlayerGroup();
	for(int i=0;i<pVehicle->GetSeatManager()->GetMaxSeats();i++)
	{
		CPed* pPassenger=pVehicle->GetSeatManager()->GetPedInSeat(i);
		if(pPassenger && !pVehicle->IsDriver(pPassenger) && pPlayerGroup && pPlayerGroup->GetGroupMembership()->IsFollower(pPassenger))
		{
			bIsInPlayerGroup=true;
			break;
		}
	}

	if(!bIsInPlayerGroup)
	{
		if ( !pVehicle->InheritsFromBoat() 
			|| pVehicle != CGameWorld::FindLocalPlayer()->GetGroundPhysical() 
			|| (CGameWorld::FindLocalPlayerVehicle() && CGameWorld::FindLocalPlayerVehicle()!=CGameWorld::FindLocalPlayer()->GetGroundPhysical())	// This line is here because GetEntityStandingOn() lingers even when player jumps off boat and enters another car. Caused the car wash script to get stuck.
			)
		{	//	Don't clear the vehicle if the player is standing on it

			bool bCanDeleteThisVehicle = true;

			if (pVehicle->m_nVehicleFlags.bCannotRemoveFromWorld)
			{
				OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "m_nVehicleFlags.bCannotRemoveFromWorld is TRUE for this vehicle"));
				bCanDeleteThisVehicle = false;
			}
			else
			{
				if (!(pVehicle->CanBeDeletedSpecial(true, false, true, true, bTakeInterestingVehiclesIntoAccount, bAllowCopRemovalWithWantedLevel, false, false, true)))
				{
					OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "CanBeDeletedSpecial() returned FALSE for this vehicle"));
					bCanDeleteThisVehicle = false;
				}
				else
				{
					if (CGarages::IsVehicleWithinHideOutGarageThatCanSaveIt(pVehicle))
					{
						OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "IsVehicleWithinHideOutGarageThatCanSaveIt() returned TRUE for this vehicle"));
						bCanDeleteThisVehicle = false;
					}
				}
			}

			if (bCanDeleteThisVehicle)
			{
				// Don't remove vehicles from garages. This results in vehicles being lost from garages.
				//	RAGE		if (!CGarages::IsPointWithinHideOutGarage(pVehicle->GetPosition()))
				{
					bool bWithinRange = false;

					switch (DistanceCheckType)
					{
					case DISTANCE_CHECK_NONE :
						bWithinRange = true;
						break;

					case DISTANCE_CHECK_CENTRE_RADIUS :
						{
							const Vector3 vecPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

							// [SPHERE-OPTIMISE]
							float DistSqr = ((vecPos.x - vecPoint1.x) * (vecPos.x - vecPoint1.x)) + ((vecPos.y - vecPoint1.y) * (vecPos.y - vecPoint1.y));

							if (DistSqr < (fDistance * fDistance))
							{
								bWithinRange = true;
							}
						}
						break;

					case DISTANCE_CHECK_ANGLED_AREA :
						{
							const Vector3 vecPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

							if (CScriptAreas::IsVecIn3dAngledArea(vecPos, vecPoint1, vecPoint2, fDistance, false))
							{
								bWithinRange = true;
							}
						}
						break;
					}

					if (bWithinRange)
					{
                        // flag the vehicle for deletion so that if it becomes locally owned in the near future it will be deleted
				        if (pVehicle->GetNetworkObject())
				        {
							bool bCanDeleteThisNetworkedVehicle = true;
							if (pVehicle->IsNetworkClone())
							{
								OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "It's a network clone"));
								bCanDeleteThisNetworkedVehicle = false;
							}
							else if (!pVehicle->GetNetworkObject()->CanDelete())
							{
								OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "GetNetworkObject()->CanDelete() returned false"));
								bCanDeleteThisNetworkedVehicle = false;
							}

							if (!bCanDeleteThisNetworkedVehicle)
					        {
						        static_cast<CNetObjEntity*>(pVehicle->GetNetworkObject())->FlagForDeletion();
						        return false;
					        }
				        }

						// remove car occupants (if any) from the world
						// (different from RemoveDriver & RemovePassenger which are called
						// when the ped gets out but still remains in the world)

						if(pVehicle->GetDriver())
						{
							pVehicle->GetDriver()->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
							CPedFactory::GetFactory()->DestroyPed(pVehicle->GetDriver(), true);
							// If the driver hasn't been removed (just marked to be, then flush their tasks to set them out as the vehicle is about to be deleted)
							if (pVehicle->GetDriver())
							{
								 pVehicle->GetDriver()->GetPedIntelligence()->FlushImmediately();
							}
							Assertf(!pVehicle->GetDriver(), "CGameWorld::DeleteThisVehicleIfPossible - expected the vehicle's driver to have been removed by CPedPopulation::RemovePed aborting his car task");
						}

						for(int i=0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
						{
							CPed *pTempPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
							if(pTempPed && i != pVehicle->GetDriverSeat())
							{
								pTempPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
								if(!CPedFactory::GetFactory()->DestroyPed(pTempPed, true))	//	the ped is deleted in here so pPassengers[i] will no longer be a valid ped pointer - this might cause problems in RemovePassenger below
								{
									pTempPed->GetPedIntelligence()->FlushImmediately();
								}
								Assertf(!pVehicle->GetSeatManager()->GetPedInSeat(i), "CGameWorld::DeleteThisVehicleIfPossible - expected the vehicle's passenger to have been removed by CPedPopulation::RemovePed aborting his car task");
							}
						}

						pVehicle->m_nVehicleFlags.bDontStoreAsPersistent = true;
						g_ptFxManager.RemovePtFxFromEntity(pVehicle);
						CVehicleFactory::GetFactory()->Destroy(pVehicle);
						return true;
					}
					else
					{
#if !__NO_OUTPUT
						if (ms_bLogClearCarFromArea_OutOfRange)
						{
							LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "It's out of the range specified for this clear_area");
						}
#endif	//	!__NO_OUTPUT
					}
				}
			}
		}
		else
		{
			OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "It's a boat and the player is standing on it"));
		}
	}
	else
	{
		OUTPUT_ONLY(LogClearCarFromArea("CGameWorld::DeleteThisVehicleIfPossible", pVehicle, "It contains members of the player's group"));
	}
	return false;
}


// [SPHERE-OPTIMISE] ClearCentre, ClearRadius could be a spdSphere
static float ClearRadius = 0.0f;	//	used in ClearCarFromAreaCB and RemovePedFromAreaCB
static Vector3 ClearCentre(0.0f, 0.0f, 0.0f);
static bool s_bCheckInterestingVehicles = false;
static bool s_bAllowCopRemovalWithWantedLevel = false;
static bool s_bOnlyCopCars = false;
static bool s_bLeaveCarGenCars = false;
static bool s_bCheckViewFrustum = false;
static bool s_bIfWrecked = false;
static bool s_bIfAbandoned = false;
static bool s_bIfEngineDestroyed = false;
static bool s_bKeepScriptTrain = false;

bool IsVehicleScriptTrain(CVehicle* pVehicle)
{
	if(pVehicle->InheritsFromTrain())
	{
		CTrain* pTrain = static_cast<CTrain*>(pVehicle);
		GtaThread* scriptThread = CTheScripts::GetCurrentGtaScriptThread();
		if(pTrain && scriptThread)
		{
			bool bScriptCreated = (pTrain->GetScriptThatCreatedMe() ? *pTrain->GetScriptThatCreatedMe() == scriptThread->m_Handler->GetScriptId() : false);
			if(pTrain->GetMissionScriptId() == scriptThread->GetThreadId() || bScriptCreated)
			{
				return true;
			}
		}
	}
	return false;
}

//
// name:		ClearCarFromAreaCB
// description:	Clear car from area callback
bool ClearCarFromAreaCB(void* pItem, void* UNUSED_PARAM(data))
{	
	CVehicle* pVehicle = static_cast<CVehicle*>(pItem);

	if (!(
			s_bLeaveCarGenCars && 
			( 
			  pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED || 
			  pVehicle->m_CarGenThatCreatedUs != -1
			)
		 )
	   )
	{
		if(s_bIfEngineDestroyed && pVehicle->GetVehicleDamage()->GetEngineHealth() > ENGINE_DAMAGE_ONFIRE)
			return true;
		if(s_bIfWrecked && pVehicle->GetStatus() != STATUS_WRECKED)
			return true;
		if(s_bIfAbandoned && pVehicle->GetStatus() != STATUS_ABANDONED)
			return true;
		if(s_bKeepScriptTrain && IsVehicleScriptTrain(pVehicle))
			return true;

		if(s_bCheckViewFrustum)
		{
			static const float fOnScreenDist = 100.0f;
			static const float fOffScreenDist = 100.0f;
			Vector3 vOrigin;
			float fRadius = pVehicle->GetBoundCentreAndRadius(vOrigin);
			if(camInterface::IsSphereVisibleInGameViewport(vOrigin, fRadius))
				return true;
			if (NetworkInterface::IsGameInProgress())
				if(NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vOrigin, fRadius, fOnScreenDist, fOffScreenDist))
					return true;
		}

		Vector3 vecDummyCentre(0.0f, 0.0f, 0.0);
		CGameWorld::DeleteThisVehicleIfPossible(pVehicle, s_bOnlyCopCars, s_bCheckInterestingVehicles, s_bAllowCopRemovalWithWantedLevel, DISTANCE_CHECK_CENTRE_RADIUS, ClearCentre, ClearRadius, vecDummyCentre);
	}

	return true;
}

//
// name:		ClearCarsFromArea
// description:	Clear all the cars from a 3d box
//	void CGameWorld::ClearCarsFromArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
void CGameWorld::ClearCarsFromArea(const Vector3 &Centre, float Radius, bool bCheckInterestingVehicles, bool bLeaveCarGenCars, bool bCheckViewFrustum, bool bIfWrecked, bool bIfAbandoned, bool bAllowCopRemovalWithWantedLevel, bool bIfEngineOnFire, bool bKeepScriptTrains)
{
	ClearRadius = Radius;
	ClearCentre = Centre;
	s_bCheckInterestingVehicles = bCheckInterestingVehicles;
	s_bAllowCopRemovalWithWantedLevel = bAllowCopRemovalWithWantedLevel;
	s_bOnlyCopCars = false;
	s_bLeaveCarGenCars = bLeaveCarGenCars;
	s_bIfWrecked = bIfWrecked;
	s_bIfAbandoned = bIfAbandoned;
	s_bCheckViewFrustum = bCheckViewFrustum;
	s_bIfEngineDestroyed = bIfEngineOnFire;
	s_bKeepScriptTrain = bKeepScriptTrains;

#if !__NO_OUTPUT
	if(ms_bLogClearCarFromArea)
	{
		Displayf("******************************************************************************************************************************");
		Displayf("CGameWorld::ClearCarsFromArea Position (%.1f,%.1f,%.1f) Radius:%.1f",
			ClearCentre.GetX(), ClearCentre.GetY(), ClearCentre.GetZ(),
			ClearRadius);
		Displayf("bCheckInterestingVehicles:%s, bAllowCopRemovalWithWantedLevel:%s, bOnlyCopCars:%s, bLeaveCarGenCars:%s",
			s_bCheckInterestingVehicles?"true":"false",
			s_bAllowCopRemovalWithWantedLevel?"true":"false",
			s_bOnlyCopCars?"true":"false",
			s_bLeaveCarGenCars?"true":"false");
		Displayf("bIfWrecked:%s, bIfAbandoned:%s, bCheckViewFrustum:%s, bIfEngineOnFire:%s, bKeepScriptTrains:%s",
			s_bIfWrecked?"true":"false",
			s_bIfAbandoned?"true":"false",
			s_bCheckViewFrustum?"true":"false",
			s_bIfEngineDestroyed?"true":"false",
			s_bKeepScriptTrain?"true":"false");
		Displayf("******************************************************************************************************************************");
	}
#endif	//	!__NO_OUTPUT

	CVehicle::GetPool()->ForAll(ClearCarFromAreaCB, NULL);
}


static Vector3 vecPoint1OfAngledArea(0.0f, 0.0f, 0.0f);
static Vector3 vecPoint2OfAngledArea(0.0f, 0.0f, 0.0f);
static float DistanceToOppositeFaceOfAngledArea = 0.0f;

//
// name:		ClearCarFromAngledAreaCB
// description:	Clear car from angled area callback
bool ClearCarFromAngledAreaCB(void* pItem, void* UNUSED_PARAM(data))
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pItem);

	if (!(
			s_bLeaveCarGenCars && 
			( 
				pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED || 
				pVehicle->m_CarGenThatCreatedUs != -1
			)
		 )
	   )
	{
		if(s_bIfEngineDestroyed && pVehicle->GetVehicleDamage()->GetEngineHealth() > ENGINE_DAMAGE_ONFIRE)
			return true;
		if(s_bIfWrecked && pVehicle->GetStatus() != STATUS_WRECKED)
			return true;
		if(s_bIfAbandoned && pVehicle->GetStatus() != STATUS_ABANDONED)
			return true;
		if(s_bKeepScriptTrain && IsVehicleScriptTrain(pVehicle))
			return true;


		if(s_bCheckViewFrustum)
		{
			static const float fOnScreenDist = 100.0f;
			static const float fOffScreenDist = 100.0f;
			Vector3 vOrigin;
			float fRadius = pVehicle->GetBoundCentreAndRadius(vOrigin);
			if(camInterface::IsSphereVisibleInGameViewport(vOrigin, fRadius))
				return true;
			if (NetworkInterface::IsGameInProgress())
				if(NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vOrigin, fRadius, fOnScreenDist, fOffScreenDist))
					return true;			
		}

		CGameWorld::DeleteThisVehicleIfPossible(pVehicle, s_bOnlyCopCars, s_bCheckInterestingVehicles, s_bAllowCopRemovalWithWantedLevel, DISTANCE_CHECK_ANGLED_AREA, vecPoint1OfAngledArea, DistanceToOppositeFaceOfAngledArea, vecPoint2OfAngledArea);
	}

	return true;
}


void CGameWorld::ClearCarsFromAngledArea(const Vector3 &vecPoint1, const Vector3 &vecPoint2, float DistanceToOppositeFace, bool bCheckInterestingVehicles, bool bLeaveCarGenCars, bool bCheckViewFrustum, bool bIfWrecked, bool bIfAbandoned, bool bAllowCopRemovalWithWantedLevel, bool bIfEngineOnFire, bool bKeepScriptTrains)
{
	vecPoint1OfAngledArea = vecPoint1;
	vecPoint2OfAngledArea = vecPoint2;
	DistanceToOppositeFaceOfAngledArea = DistanceToOppositeFace;
	s_bCheckInterestingVehicles = bCheckInterestingVehicles;
	s_bAllowCopRemovalWithWantedLevel = bAllowCopRemovalWithWantedLevel;
	s_bOnlyCopCars = false;
	s_bLeaveCarGenCars = bLeaveCarGenCars;
	s_bIfWrecked = bIfWrecked;
	s_bIfAbandoned = bIfAbandoned;
	s_bCheckViewFrustum = bCheckViewFrustum;
	s_bIfEngineDestroyed = bIfEngineOnFire;
	s_bKeepScriptTrain = bKeepScriptTrains;

#if !__NO_OUTPUT
	if(ms_bLogClearCarFromArea)
	{
		Displayf("******************************************************************************************************************************");
		Displayf("CGameWorld::ClearCarsFromAngledArea Pt1 (%.1f,%.1f,%.1f) Pt2 (%.1f,%.1f,%.1f) Dist:%.1f",
			vecPoint1OfAngledArea.GetX(), vecPoint1OfAngledArea.GetY(), vecPoint1OfAngledArea.GetZ(),
			vecPoint2OfAngledArea.GetX(), vecPoint2OfAngledArea.GetY(), vecPoint2OfAngledArea.GetZ(),
			DistanceToOppositeFaceOfAngledArea);
		Displayf("bCheckInterestingVehicles:%s, bAllowCopRemovalWithWantedLevel:%s, bOnlyCopCars:%s, bLeaveCarGenCars:%s",
			s_bCheckInterestingVehicles?"true":"false",
			s_bAllowCopRemovalWithWantedLevel?"true":"false",
			s_bOnlyCopCars?"true":"false",
			s_bLeaveCarGenCars?"true":"false");
		Displayf("bIfWrecked:%s, bIfAbandoned:%s, bCheckViewFrustum:%s, bIfEngineOnFire:%s, bKeepScriptTrains:%s",
			s_bIfWrecked?"true":"false",
			s_bIfAbandoned?"true":"false",
			s_bCheckViewFrustum?"true":"false",
			s_bIfEngineDestroyed?"true":"false",
			s_bKeepScriptTrain?"true":"false");
		Displayf("******************************************************************************************************************************");
	}
#endif	//	!__NO_OUTPUT

	CVehicle::GetPool()->ForAll(ClearCarFromAngledAreaCB, NULL);
}

//
// name:		RemovePedFromAreaCB
// description:	Remove ped from area
static bool RemovePedFromAreaCB(void* pItem, void* UNUSED_PARAM(data))
{
	//	spdAABB* pBox = (spdAABB*)data;
	//	Vector3 *pClearCentre = (Vector3*)data;
	CPed* pPed = static_cast<CPed*>(pItem);

	if (pPed->IsPlayer() == FALSE && pPed->CanBeDeleted(false, false))
	{
		//Don't remove peds in player's group.
		CPedGroup* pPlayerGroup=CGameWorld::FindLocalPlayerGroup();
		if(!pPlayerGroup || !pPlayerGroup->GetGroupMembership()->IsFollower(pPed))
		{
			Vector3 vecPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			// [SPHERE-OPTIMISE]
			float DistSqr = ((vecPos.x - ClearCentre.x) * (vecPos.x - ClearCentre.x)) +	((vecPos.y - ClearCentre.y) * (vecPos.y - ClearCentre.y));

			if (DistSqr < (ClearRadius * ClearRadius))
			{
                // flag the ped for deletion so that if it becomes locally owned in the near future it will be deleted
		        if (pPed->GetNetworkObject())
		        {
			        if (pPed->IsNetworkClone() || !pPed->GetNetworkObject()->CanDelete())
			        {
				        static_cast<CNetObjEntity*>(pPed->GetNetworkObject())->FlagForDeletion();
				        return true;
			        }
		        }

				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
				g_ptFxManager.RemovePtFxFromEntity(pPed);
				CPedFactory::GetFactory()->DestroyPed(pPed);
			}
		}
	}
	return true;
}

//
// name:		RemovePedFromAngledAreaCB
// description:	Remove ped from an angled area
static bool RemovePedFromAngledAreaCB(void* pItem, void* UNUSED_PARAM(data))
{
	CPed* pPed = static_cast<CPed*>(pItem);

	if (pPed->IsPlayer() == FALSE && pPed->CanBeDeleted(false, false))
	{
		//Don't remove peds in player's group.
		CPedGroup* pPlayerGroup=CGameWorld::FindLocalPlayerGroup();
		if(!pPlayerGroup || !pPlayerGroup->GetGroupMembership()->IsFollower(pPed))
		{
			Vector3 vecPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			if (CScriptAreas::IsVecIn3dAngledArea(vecPos, vecPoint1OfAngledArea, vecPoint2OfAngledArea, DistanceToOppositeFaceOfAngledArea, false))
			{
                // flag the ped for deletion so that if it becomes locally owned in the near future it will be deleted
		        if (pPed->GetNetworkObject())
		        {
			        if (pPed->IsNetworkClone() || !pPed->GetNetworkObject()->CanDelete())
			        {
				        static_cast<CNetObjEntity*>(pPed->GetNetworkObject())->FlagForDeletion();
				        return true;
			        }
		        }

				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
				CPedFactory::GetFactory()->DestroyPed(pPed);
			}
		}
	}
	return true;
}

//
// name:		RemoveCopPedFromAreaCB
// description:	Remove ped from area
static bool RemoveCopPedFromAreaCB(void* pItem, void* UNUSED_PARAM(data))
{
	CPed* pPed = static_cast<CPed*>(pItem);

	if (pPed->IsPlayer() == FALSE && pPed->CanBeDeleted() && pPed->GetPedType() == PEDTYPE_COP)
	{
		//Don't remove peds in player's group.
		CPedGroup* pPlayerGroup=CGameWorld::FindLocalPlayerGroup();
		if(!pPlayerGroup || !pPlayerGroup->GetGroupMembership()->IsFollower(pPed))
		{
			Vector3 vecPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			// [SPHERE-OPTIMISE]
			float DistSqr = ((vecPos.x - ClearCentre.x) * (vecPos.x - ClearCentre.x)) +	((vecPos.y - ClearCentre.y) * (vecPos.y - ClearCentre.y));

			if (DistSqr < (ClearRadius * ClearRadius))
			{
                // flag the ped for deletion so that if it becomes locally owned in the near future it will be deleted
		        if (pPed->GetNetworkObject())
		        {
			        if (pPed->IsNetworkClone() || !pPed->GetNetworkObject()->CanDelete())
			        {
				        static_cast<CNetObjEntity*>(pPed->GetNetworkObject())->FlagForDeletion();
				        return true;
			        }
		        }

				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
				CPedFactory::GetFactory()->DestroyPed(pPed);
			}
		}
	}
	return true;
}

//
// name:		ClearPedsFromArea
// description:	Clear all the peds from a 3d box
//	void CGameWorld::ClearPedsFromArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
void CGameWorld::ClearPedsFromAngledArea(const Vector3 &vecPoint1, const Vector3 &vecPoint2, float DistanceToOppositeFace)
{
	vecPoint1OfAngledArea = vecPoint1;
	vecPoint2OfAngledArea = vecPoint2;
	DistanceToOppositeFaceOfAngledArea = DistanceToOppositeFace;

	CPed::GetPool()->ForAll(RemovePedFromAngledAreaCB, NULL);
}


//
// name:		ClearPedsFromArea
// description:	Clear all the peds from a 3d box
//	void CGameWorld::ClearPedsFromArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
void CGameWorld::ClearPedsFromArea(const Vector3 &Centre, float Radius)
{
	ClearRadius = Radius;
	ClearCentre = Centre;
	//	spdAABB box(Vector3(MinX, MinY, MinZ), Vector3(MaxX, MaxY, MaxZ));
	CPed::GetPool()->ForAll(RemovePedFromAreaCB, NULL);
}

//
// name:		ClearCopsFromArea
// description:	Clear all the cop peds from a 3d box

void CGameWorld::ClearCopsFromArea(const Vector3 &Centre, float Radius)
{
	ClearRadius = Radius;
	ClearCentre = Centre;

	CPed::GetPool()->ForAll(RemoveCopPedFromAreaCB, NULL);

	s_bOnlyCopCars = true;
	s_bCheckInterestingVehicles = true;
	s_bAllowCopRemovalWithWantedLevel = true;
	CVehicle::GetPool()->ForAll(ClearCarFromAreaCB, NULL);

}

//
// name:		ClearObjectsFromArea
// description:	Clear all the cop peds from a 3d box

void CGameWorld::ClearObjectsFromArea(const Vector3 &Centre, float Radius, bool bConvertObjectsWithBrainsToDummyObjects/*=false*/,
	CObject *pExceptionObject/*=NULL*/, bool bForce/*=false*/,  bool bIncludeDoors/*=false*/,  bool bExcludeLadders/*=false*/)
{
	CObjectPopulation::DeleteAllTempObjectsInArea(Centre, Radius);

	CObjectPopulation::ConvertAllObjectsInAreaToDummyObjects(Centre, Radius, bConvertObjectsWithBrainsToDummyObjects, pExceptionObject, bIncludeDoors, bForce, bExcludeLadders);
}

//
// name:		SetCarDamagedCB
// description:	Set car to be damaged
bool SetCarDamagedCB(void* pItem, void* data)
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pItem);
	pVehicle->SetCanBeDamaged(data != NULL);
	return true;
}

//
// name:		SetAllCarsCanBeDamaged
// description:	Set whether all the cars can be damaged or not
void CGameWorld::SetAllCarsCanBeDamaged(bool DmgeFlag)
{
	CVehicle::GetPool()->ForAll(SetCarDamagedCB, (void*)DmgeFlag);
}

// Name			:	StopAllLawEnforcersInTheirTracks
// Purpose		:	Any law enforcer is stopped so that we don't get into trouble with the cut scenes
// Parameters	:
// Returns		:

bool SetLawEnforcerVehicleMoveSpeedCB(void* pItem, void* UNUSED_PARAM(data))
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pItem);
	if (pVehicle->IsLawEnforcementCar() && ((!pVehicle->GetDriver()) || (!pVehicle->GetDriver()->IsPlayer())))
	{
		pVehicle->SetVelocity(ORIGIN);
	}
	return true;
}

void CGameWorld::StopAllLawEnforcersInTheirTracks()
{
	if(!NetworkInterface::IsGameInProgress())
	{
		CVehicle::GetPool()->ForAll(SetLawEnforcerVehicleMoveSpeedCB, NULL);
	}
}

//
// name:		ExtinguishAllCarFiresInArea
// description:	Extinguish all car fires in area
void CGameWorld::ExtinguishAllCarFiresInArea(const Vector3& Centre, float Radius, bool finishPtFx, bool bClampVehicleHealth)
{
	s32 nPoolSize;
	s32 loop;

	CVehicle *pVehicle = NULL;
	CVehicle::Pool* pVehPool = CVehicle::GetPool();

	float	DistSqr;

	nPoolSize = (s32) pVehPool->GetSize();

	for (loop = 0; loop < nPoolSize; loop++)
	{
		pVehicle = pVehPool->GetSlot(loop);

		if (pVehicle)
		{
			// [SPHERE-OPTIMISE]
			const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
			DistSqr = (Centre.x - vVehiclePos.x)*(Centre.x - vVehiclePos.x) +
				(Centre.y - vVehiclePos.y)*(Centre.y - vVehiclePos.y) +
				(Centre.z - vVehiclePos.z)*(Centre.z - vVehiclePos.z);

			if (DistSqr < Radius*Radius)
			{
				pVehicle->ExtinguishCarFire(finishPtFx, bClampVehicleHealth);
			}
		}
	}
}

void CGameWorld::ClearCulpritFromAllCarFires(const CEntity* pCulprit)
{
	s32 nPoolSize;

	CVehicle *pVehicle = NULL;
	CVehicle::Pool* pVehPool = CVehicle::GetPool();
	nPoolSize = (s32) pVehPool->GetSize();

	for (s32 loop = 0; loop < nPoolSize; loop++)
	{
		pVehicle = pVehPool->GetSlot(loop);
		if(pVehicle)
		{
			pVehicle->ClearCulpritFromCarFires(pCulprit);
		}
	}
}

/////////////////////////////////////////////////////////////////////
// FUNCTION : Finds an empty slot in the Players array for a new player, returns -1 if none.
//////////////////////////////////////////////////////////////////////

struct SlotPred
{
	bool operator()(unsigned* a, unsigned* b) const
	{
		return *a < *b;
	}
};


//////////////////////////////////////////////////////////////////////
// FUNCTION : Adds player registered at the given index to the world at the given coords
//////////////////////////////////////////////////////////////////////
bool CGameWorld::AddPlayerToWorld(CPed* pPlayerPed, const Vector3& pos)
{
	Assert(pPlayerPed);
	Assert(pPlayerPed->GetPlayerInfo());

	pPlayerPed->SetPosition(pos);
	CScriptEntities::ClearSpaceForMissionEntity(pPlayerPed);

	Add(pPlayerPed, CGameWorld::OUTSIDE );

#if __DEV
	pPlayerPed->m_nDEflags.bCheckedForDead = TRUE;
#endif

	return true;
}


// PURPOSE
//    Creates the game player, if the player already exists sets the player health.
//    Player Zeta position is corrected for the right height to the floor.
//	  mjc-Moved from CPlayerPed
// RETURN
//    The slot number were the player was created.
int CGameWorld::CreatePlayer(Vector3& pos, u32 nameHash, float health)
{
	if (PARAM_noPlayer.Get())
		return 0;

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (!pPlayerPed)
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(nameHash, &modelId);

		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		// need to fix for network stuff:
		Matrix34 tempMat;
		tempMat.Identity();
		const CControlledByInfo localPlayerControl(false, true);
		pPlayerPed = CPedFactory::GetFactory()->CreatePlayerPed(localPlayerControl, modelId.GetModelIndex(), &tempMat);
		if (!Verifyf(pPlayerPed, "Can not find player ped"))
			return -1;

		StatsInterface::SetStatsModelPrefix( pPlayerPed );

		pPlayerPed->PopTypeSet(POPTYPE_PERMANENT);
		pPlayerPed->SetDefaultDecisionMaker();
		pPlayerPed->SetCharParamsBasedOnManagerType();
		pPlayerPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

		// Correct Zeta position to match the ground floor
		if (pos.z <= INVALID_MINIMUM_WORLD_Z)
		{
			pos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, pos.x, pos.y);
		}
		pos.z += pPlayerPed->GetDistanceFromCentreOfMassToBaseOfModel();

		CGameWorld::AddPlayerToWorld(pPlayerPed, pos);

		CModelInfo::SetAssetsAreDeletable(modelId);

		pPlayerPed->GetPedIntelligence()->AddTaskDefault(pPlayerPed->GetCurrentMotionTask()->CreatePlayerControlTask());

		pPlayerPed->InitSpecialAbility();
	}

	// DEBUG-PH 
	// rage: the player spawns with 0 health as default
	// temporarily increase this to 100.0f
	pPlayerPed->SetHealth(health);
	// END DEBUG

#if __DEV
	// This flag is used and checked on certain script commands.
	pPlayerPed->m_nDEflags.bCheckedForDead = true;
#endif

	if (pPlayerPed->GetPlayerInfo())
	{
		pPlayerPed->GetPlayerInfo()->SetPlayerControl(false, false);
	}

	// local player always created in slot 0 now
	return (0);
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Replaces a ped with a new ped. If the ped is a player ped this is
//			  done without affecting the player info structure.
//            This is temporary code until John Whyte implements the
//            ability to change a players model
// RETURNS	: pointer to the new player ped
//////////////////////////////////////////////////////////////////////
CPed *CGameWorld::ChangePedModel(CPed& ped, fwModelId newModelId)
{
	ConductorMessageData messageData;
	messageData.conductorName = SuperConductor;
	messageData.message = SwitchConductorsOff;

	CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData);)
	// cache the player info
	CPlayerInfo* pPlayerInfo = ped.GetPlayerInfo();

	// Cache the position of the ped
	Vector3 vecOldPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	float   fOldPedHeading    = ped.GetTransform().GetHeading();

	// cache the ped's tasks
	CTaskInfo *oldTaskInfos[MAX_TASK_INFOS_PER_PED];
	u32 numTaskInfos = 0;

	CTaskInfo *taskInfo = ped.GetPedIntelligence()->GetQueriableInterface()->GetFirstTaskInfo();

	while(taskInfo)
	{
		Assert(numTaskInfos < MAX_TASK_INFOS_PER_PED);

		if(numTaskInfos < MAX_TASK_INFOS_PER_PED)
		{
			oldTaskInfos[numTaskInfos++] = taskInfo;
		}

		ped.GetPedIntelligence()->GetQueriableInterface()->UnlinkTaskInfoFromNetwork(taskInfo);

		taskInfo = ped.GetPedIntelligence()->GetQueriableInterface()->GetFirstTaskInfo();
	}

	// cache the ped's task sequences
	TaskSequenceInfo* oldTaskSequenceInfos[CTaskTypes::MAX_NUM_TASK_TYPES];
	u32 numTaskSequenceInfos = 0;

	TaskSequenceInfo *taskSeqInfo = ped.GetPedIntelligence()->GetQueriableInterface()->GetFirstTaskSequenceInfo();

	while(taskSeqInfo)
	{
		if (AssertVerify(numTaskSequenceInfos < CTaskTypes::MAX_NUM_TASK_TYPES))
		{
			oldTaskSequenceInfos[numTaskSequenceInfos++] = taskSeqInfo;
		}

		ped.GetPedIntelligence()->GetQueriableInterface()->UnlinkTaskSequenceInfoFromNetwork(taskSeqInfo);

		taskSeqInfo = ped.GetPedIntelligence()->GetQueriableInterface()->GetFirstTaskSequenceInfo();
	}

	// Explicitly flush the ped's tasks, the tasks aren't recreated anyway and not flushing can leave the new ped in a bad state
	ped.GetPedIntelligence()->FlushImmediately();

	// reset the player info pointer (is not a non-player ped).  Needs to be done after the intelligence flush
	ped.SetPlayerInfo(NULL);

	// cache all of the important flags
	bool isFixed                    = ped.IsBaseFlagSet(fwEntity::IS_FIXED);
	bool usesCollision              = ped.IsCollisionEnabled();
	// cache wanted level multiplier
	float fWantedLevelMultiplier = pPlayerInfo ? pPlayerInfo->GetWanted().m_fMultiplier : 0.0f;
	
	// cache the relationship group
	CRelationshipGroup* pRelationshipGrp =  ped.GetPedIntelligence()->GetRelationshipGroup();

	// cache the control info
	const CControlledByInfo controlInfo(ped.GetControlledByInfo());

	// now create a new player ped with the specified model index
	Matrix34 tempMat = MAT34V_TO_MATRIX34(ped.GetMatrix());

    ASSERT_ONLY(CPlayerInfo::SetChangingPlayerModel(true));
	CPed* pNewPed = NULL;
	if(pPlayerInfo)
	{
		pNewPed = CPedFactory::GetFactory()->CreatePlayerPed(controlInfo, newModelId.GetModelIndex(), &tempMat, pPlayerInfo);
	}
	else
	{
		pNewPed = CPedFactory::GetFactory()->CreatePed(controlInfo, newModelId, &tempMat, true, true, false);
	}
	Assert(pNewPed);

	// FA: why do we randomize when we change model? we'd expect to set a required variation afterwards, no?
	// this breaks the MP blended characters. we could potential skip the randomization when previous head has a head blend if really needed
	//pNewPlayerPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL);

	pNewPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponAccuracy, 1.0f);
	pNewPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponDamageModifier, 1.0f);
	pNewPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanTauntInVehicle);

	// Reset our firing pattern
	pNewPed->GetPedIntelligence()->GetCombatBehaviour().SetFiringPattern(FIRING_PATTERN_FULL_AUTO);

	// Restore wanted level multiplier
	if(pNewPed->GetPlayerInfo())
	{
		pNewPed->GetPlayerInfo()->GetWanted().m_fMultiplier = fWantedLevelMultiplier;
	}

	// Player character should always be a permanent char
	pNewPed->PopTypeSet(POPTYPE_PERMANENT);
	pNewPed->SetDefaultDecisionMaker();
	pNewPed->SetCharParamsBasedOnManagerType();
	pNewPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

	// Set the position of the new player
	pNewPed->SetPosition(vecOldPedPosition);
	pNewPed->SetHeading(fOldPedHeading);
	pNewPed->SetDesiredHeading(fOldPedHeading);

	// restore the old player's tasks & sequence infos
	pNewPed->GetPedIntelligence()->GetQueriableInterface()->Reset();

	for(u32 index = 0; index < numTaskInfos; index++)
	{
		pNewPed->GetPedIntelligence()->GetQueriableInterface()->AddTaskInfoFromNetwork(oldTaskInfos[index]);
	}

	for(u32 index = 0; index < numTaskSequenceInfos; index++)
	{
		pNewPed->GetPedIntelligence()->GetQueriableInterface()->AddTaskSequenceInfo(oldTaskSequenceInfos[index]);
	}

	// restore the important flags
	if (isFixed){
		pNewPed->SetBaseFlag(fwEntity::IS_FIXED);
	} else {
		pNewPed->ClearBaseFlag(fwEntity::IS_FIXED);
	}

	pNewPed->CopyIsVisibleFlagsFrom(&ped, true);
	usesCollision ? pNewPed->EnableCollision() : pNewPed->DisableCollision();

	// Set the health of the new player
	const CHealthConfigInfo* pHealthInfo = pNewPed->GetPedHealthInfo();
	Assert( pHealthInfo );
	pNewPed->SetHealth( pHealthInfo->GetDefaultHealth() );

	// Add the new player to the world
	CGameWorld::Add(pNewPed, CGameWorld::OUTSIDE );

	CPedModelInfo* pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(newModelId);
	Assert(pModelInfo->GetModelType() == MI_TYPE_PED);
	Assertf(pModelInfo->GetDrawable() || pModelInfo->GetFragType(), "%s:Ped model is not loaded", pModelInfo->GetModelName());

	pNewPed->GetPrimaryMotionTask()->SetDefaultClipSets(*pNewPed);

	// Set the gesture anim group from the model info
	// Stored in the ped so that it can be overridden at runtime
	pNewPed->SetGestureClipSet(pModelInfo->GetDefaultGestureClipSet());

	// Set the facial anim group from the model info
	// Stored in the ped so that it can be overridden at runtime
	CFacialDataComponent* pFacialData = pNewPed->GetFacialData();
	if(pFacialData)
	{
		fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(pModelInfo->GetFacialClipSetGroupId());
		if (pFacialClipSetGroup)
		{
			fwMvClipSetId facialBaseClipsetId(pFacialClipSetGroup->GetBaseClipSetName());
			Assertf(fwClipSetManager::GetClipSet(facialBaseClipsetId), "%s:Unrecognised facial base clipset", pFacialClipSetGroup->GetBaseClipSetName().GetCStr());
			pFacialData->SetFacialClipSet(pNewPed, facialBaseClipsetId);
		}
	}

	// restore the player's relationship group
	pNewPed->GetPedIntelligence()->SetRelationshipGroup(pRelationshipGrp);

    // cache the network object before destroying the old player, also ensure the task trees
    // are restored before deletion
	CNetObjPlayer *pNetObjPlayer = static_cast<CNetObjPlayer *>(ped.GetNetworkObject());
    if(pNetObjPlayer && pNetObjPlayer->IsClone())
    {
        pNetObjPlayer->SetupPedAIAsLocal();
    }
    ped.SetNetworkObject(0);

	if(!pPlayerInfo)
	{
		CPedPopulation::RemovePedFromPopulationCount(&ped);
	}

	if(pNetObjPlayer)
	{
		pNetObjPlayer->ChangePed(pNewPed);
		pNewPed->SetNetworkObject(pNetObjPlayer);

		// restore the player's group
		pNewPed->GetPlayerInfo()->SetupPlayerGroup();
	}
    
	CPedFactory::GetFactory()->DestroyPlayerPed(&ped);
    ASSERT_ONLY(CPlayerInfo::SetChangingPlayerModel(false));

//	pNewPlayerPed->GetPedIntelligence()->AddTaskDefault(pNewPlayerPed->GetMoveBlender()->CreatePlayerControlTask());

	StatsInterface::SetStatsModelPrefix(pNewPed);
	CNewHud::HandleCharacterChange();

	pNewPed->InitSpecialAbility();

	if(pNewPed->IsLocalPlayer())
	{
		UpdateLensFlareAfterPlayerSwitch(pNewPed);
	}

	return pNewPed;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Changes the player control from one ped to another.
// RETURNS	: pointer to the new player ped
//////////////////////////////////////////////////////////////////////
bool CGameWorld::ChangePlayerPed(CPed& playerPed, CPed& newPlayerPed, bool bKeepScriptedTasks, bool bClearPedDamage)
{
    //@@: range CGAMEWORLD_CHANGEPLAYERPED {
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_DISABLEPHONE
	TamperActions::OnPlayerSwitched(playerPed, newPlayerPed);
#endif
    //@@: } CGAMEWORLD_CHANGEPLAYERPED
	const bool netGameInProgress = NetworkInterface::IsGameInProgress();

	bool bPlayerVisible = playerPed.GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT);

	// get the old player ped to drop any pickups he's carrying
	CPickupManager::DetachAllPortablePickupsFromPed(playerPed);

	//If the network Game is in progress we want to fix the pointers 
	//   for the game object and the pointers to the network object.
	if (netGameInProgress)
	{
		CNetObjPed* playerPedNetObj =    playerPed.GetNetworkObject() ? static_cast<CNetObjPed *> (playerPed.GetNetworkObject())    : NULL;
		CNetObjPed* pedNetObj       = newPlayerPed.GetNetworkObject() ? static_cast<CNetObjPed *> (newPlayerPed.GetNetworkObject()) : NULL;

		newPlayerPed.SetNetworkObject(NULL);
		newPlayerPed.SetNetworkObject(playerPedNetObj);
		if (playerPedNetObj)
		{
			playerPedNetObj->ChangePed(&newPlayerPed, true);

#if __BANK
			AI_LOG_WITH_ARGS("[CGameWorld::ChangePlayerPed] Changing the player %s ped %s [%p] to player %s ped %s. Vehicle Entry Config: [%p] changed to [%p]",
				AILogging::GetDynamicEntityIsCloneStringSafe(&playerPed), AILogging::GetDynamicEntityNameSafe(&playerPed), &playerPed, AILogging::GetDynamicEntityIsCloneStringSafe(&newPlayerPed),
				AILogging::GetDynamicEntityNameSafe(&newPlayerPed), playerPed.GetVehicleEntryConfig(), newPlayerPed.GetVehicleEntryConfig());
#endif
		}

		playerPed.SetNetworkObject(NULL);
		playerPed.SetNetworkObject(pedNetObj);
		if (pedNetObj)
		{
			//We can not SetupPedAIAsClone on the ped because we can't Flush clone task's.
			pedNetObj->ChangePed(&playerPed, true, false);

#if __BANK
			AI_LOG_WITH_ARGS("[CGameWorld::ChangePlayerPed] Changing the player %s ped %s [%p] to player %s ped %s. Vehicle Entry Config: [%p] changed to [%p]",
				AILogging::GetDynamicEntityIsCloneStringSafe(&newPlayerPed), AILogging::GetDynamicEntityNameSafe(&newPlayerPed), &newPlayerPed, AILogging::GetDynamicEntityIsCloneStringSafe(&playerPed),
				AILogging::GetDynamicEntityNameSafe(&playerPed), newPlayerPed.GetVehicleEntryConfig(), playerPed.GetVehicleEntryConfig());
#endif
		}
	}

	bool bFaceLeft = false;
	bool bNewAIPedGoIntoCover = false;
	const bool bNewPlayerPedWasReloading = newPlayerPed.GetPedResetFlag(CPED_RESET_FLAG_IsReloading);

	//Cache the ped type for exchange
	const ePedType playerPedType = playerPed.GetPedType();
	const ePedType pedType = newPlayerPed.GetPedType();

	// Don't bother doing any of the cover persisting stuff in multiplayer, we wouldn't see it and the change of 
	// ped type screws it up.
	// See if we're currently in cover, if so, when the ped turns into an ai one, it should be in cover
	if (!netGameInProgress && playerPed.GetPedModelInfo() && playerPed.GetPedModelInfo()->GetIsPlayerModel())
	{
		bNewAIPedGoIntoCover = StoreCoverTaskInfoForCurrentPlayer(playerPed, bFaceLeft);
	}

	bool bNewPlayerPedGoIntoCover = false;

	if (!netGameInProgress && !playerPed.IsNetworkClone())
	{
		// See if the ped we're switching to is in cover, if so, when we turn into this ped, we should be in cover
		if (newPlayerPed.GetPedModelInfo() && newPlayerPed.GetPedModelInfo()->GetIsPlayerModel())
		{
			bNewPlayerPedGoIntoCover = StoreCoverTaskInfoForAIPed(newPlayerPed);
		}

		// Reset the default tasks
		playerPed.GetPedIntelligence()->AddTaskDefault(playerPed.ComputeDefaultTask(playerPed));
		newPlayerPed.GetPedIntelligence()->AddTaskDefault(newPlayerPed.ComputeDefaultTask(newPlayerPed));
	}

    // cache the player info
    CPlayerInfo* pPlayerInfo = playerPed.GetPlayerInfo();
    Assert(pPlayerInfo);

    float fWantedLevelMultiplier = pPlayerInfo->GetWanted().m_fMultiplier;

    // Switch the control info
    const CControlledByInfo controlInfo(playerPed.GetControlledByInfo());

	// have to do this before setting the controlled by info to a player, because
	// CPedPopulation::UpdatePedCountIfNotAlready does an early out if
	// it's called on a player ped
	CPedPopulation::RemovePedFromPopulationCount(&newPlayerPed);

    newPlayerPed.SetControlledByInfo(controlInfo);
    const CControlledByInfo controlInfoOld(false, false);
    playerPed.SetControlledByInfo(controlInfoOld);
	
	//Set the proper Ped Type before setting the population type because it is important
	//for the population count in multiplayer.
	if (netGameInProgress)
	{
		playerPed.SetPedType(pedType);
		//Old player ped is left behind as a ambient ped in network games.
		playerPed.PopTypeSet(POPTYPE_RANDOM_AMBIENT);
	}
	else
	{
		// Setup the dimishing ammo rate for this ped 
		playerPed.SetPedConfigFlag( CPED_CONFIG_FLAG_UseDiminishingAmmoRate, true );
		playerPed.PopTypeSet(POPTYPE_MISSION);
	}
	playerPed.SetDefaultDecisionMaker();
    playerPed.SetCharParamsBasedOnManagerType();
	playerPed.GetPedIntelligence()->SetDefaultRelationshipGroup();

	// Set whether player can play in-car idles (should be true). Need to do this before call to CTheScripts::RegisterEntity as this resets the flag in CPed::SetupMissionState.
	newPlayerPed.SetCanPlayInCarIdles(playerPed.CanPlayInCarIdles());

	//Pass control of the old player ped to the script, it was previously controlled by the game
	if (POPTYPE_MISSION == playerPed.PopTypeGet() && CTheScripts::GetCurrentGtaScriptThread())
	{
		CTheScripts::RegisterEntity(&playerPed, false, true);
	}

	// reset the player into pointer (is not a non-player ped).  Needs to be done after the intelligence flush.
	// CTheScripts::RegisterEntity also replaces the default task, so do this after that
	playerPed.SetPlayerInfo(NULL);
	playerPed.m_nFlags.bAddtoMotionBlurMask = false;

	ASSERT_ONLY(CPlayerInfo::SetChangingPlayerModel(true));
    pPlayerInfo->SetPlayerPed(&newPlayerPed, bClearPedDamage);
    ASSERT_ONLY(CPlayerInfo::SetChangingPlayerModel(false));

	//Pass control of the ped from the script to the game
	if (POPTYPE_MISSION == newPlayerPed.PopTypeGet())
	{
		CTheScripts::UnregisterEntity(&newPlayerPed, false);
	}

	// Unregister the script extension from the new player
	CScriptEntityExtension* pExtension = newPlayerPed.GetExtension<CScriptEntityExtension>();
	if (pExtension)
	{
		newPlayerPed.GetExtensionList().Destroy(pExtension);
	}

#if FPS_MODE_SUPPORTED
	const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
	if (viewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
	{
		//B*2038645: Ensure we instantly process the FPS view mode change in CTaskPlayerOnFoot::ProcessViewModeSwitch
		newPlayerPed.SetPedConfigFlag(CPED_CONFIG_FLAG_SwitchingCharactersInFirstPerson, true);

		// B*2038645: If ped we're switching to is aiming and player is in FPS mode, reset the RNG timer so we don't start in the FPS_IDLE state
		if (newPlayerPed.GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
		{
			pPlayerInfo->ResetFPSRNGTimer();
			pPlayerInfo->ForceFPSRNGState(true);
			newPlayerPed.SetPedConfigFlag(CPED_CONFIG_FLAG_SimulatingAiming, true);
		}
	}
#endif	//FPS_MODE_SUPPORTED

    newPlayerPed.SetPlayerInfo(pPlayerInfo);

	newPlayerPed.GetPedIntelligence()->FlushEvents();

	newPlayerPed.ResetWaterTimers();

	// Reset the diminishing ammo rate
	newPlayerPed.SetPedConfigFlag( CPED_CONFIG_FLAG_UseDiminishingAmmoRate, false );

	// wanted peds will update their enclosed region
	playerPed.SetPedConfigFlag(CPED_CONFIG_FLAG_UpdateEnclosedSearchRegion, false);
	newPlayerPed.SetPedConfigFlag(CPED_CONFIG_FLAG_UpdateEnclosedSearchRegion, true);

	// unlock the ragdoll on the new player ped if it has been left locked by script, etc.
	if (newPlayerPed.GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED)
	{
		newPlayerPed.SetRagdollState(RAGDOLL_STATE_ANIM, true);
	}

	newPlayerPed.GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponAccuracy, 1.0f);
	newPlayerPed.GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponDamageModifier, 1.0f);
	newPlayerPed.GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanTauntInVehicle);
	playerPed.GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanTauntInVehicle);

	// Reset our firing pattern
	newPlayerPed.GetPedIntelligence()->GetCombatBehaviour().SetFiringPattern(FIRING_PATTERN_FULL_AUTO);

    // Restore wanted level multiplier
    aiFatalAssertf(newPlayerPed.GetPlayerInfo(), "Player ped must have playerinfo!");
    newPlayerPed.GetPlayerInfo()->GetWanted().m_fMultiplier = fWantedLevelMultiplier;
	//Set the proper Ped Type before setting the population type because it is important
	//for the population count in multiplayer
	if (netGameInProgress)
	{
		newPlayerPed.SetPedType(playerPedType);
	}
	// Player character should always be a permanent char
	newPlayerPed.PopTypeSet(POPTYPE_PERMANENT);
	newPlayerPed.SetDefaultDecisionMaker();
    newPlayerPed.SetCharParamsBasedOnManagerType();
	newPlayerPed.GetPedIntelligence()->SetDefaultRelationshipGroup();
    ASSERT_ONLY(CPlayerInfo::SetChangingPlayerModel(true));
    if(newPlayerPed.GetControlledByInfo().IsControlledByLocalPlayer())
    {
		g_WeaponAudioEntity.RemoveAllPlayerInventoryWeapons();
        CPed* pOldPlayerPed = CPedFactory::GetFactory()->GetLocalPlayer();
		if(pOldPlayerPed && pOldPlayerPed != &newPlayerPed)
		{
			pOldPlayerPed->UpdateFirstPersonFlags(false);
		}
        CPedFactory::GetFactory()->SetLocalPlayer(&newPlayerPed);
		g_WeaponAudioEntity.AddAllInventoryWeaponsOnPed(&newPlayerPed);
    }
    ASSERT_ONLY(CPlayerInfo::SetChangingPlayerModel(false));

    // Update default tasks on both peds:

	if (!playerPed.IsNetworkClone())
	{
		newPlayerPed.SetPedConfigFlag( CPED_CONFIG_FLAG_UseKinematicModeWhenStationary, false );

		// Update the local player
		if (bNewPlayerPedGoIntoCover)
		{
			RestoreCoverTaskInfoForNewPlayer(newPlayerPed);

			// Force a pre-camera AI Update to ensure we have a valid player cover state for the gameplay camera to use
			newPlayerPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAIUpdate, true);
			newPlayerPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		}
		else
		{
			if (!bKeepScriptedTasks || newPlayerPed.GetUsingRagdoll())
			{
				newPlayerPed.GetPedIntelligence()->ClearPrimaryTask();
				// Don't clear the ragdoll response task
				if (!newPlayerPed.GetUsingRagdoll())
				{
					newPlayerPed.GetPedIntelligence()->ClearTaskEventResponse();
				}
				else if (newPlayerPed.GetPedIntelligence()->FindTaskCombat())
				{
					newPlayerPed.GetPedIntelligence()->ClearTaskTempEventResponse();
				}
				newPlayerPed.GetPedIntelligence()->AddTaskDefault(newPlayerPed.ComputeDefaultTask(newPlayerPed));
			}
			else
			{
				newPlayerPed.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToStayInCoverDueToPlayerSwitch, true);
				newPlayerPed.SetPedConfigFlag(CPED_CONFIG_FLAG_WaitingForPlayerControlInterrupt, true);
				CPlayerInfo* pPlayerInfo = newPlayerPed.GetPlayerInfo();
				if (pPlayerInfo)
				{
					pPlayerInfo->TimeSinceSwitch = fwTimer::GetTimeInMilliseconds();
				}
			}

			if (newPlayerPed.GetPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_SKIP_AIM_INTRO))
			{
				newPlayerPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
				newPlayerPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			}
		}

		// Update the ped that we used to be (now under ai control)
		if (bNewAIPedGoIntoCover)
		{
			RestoreCoverTaskInfoForNewAIPed(playerPed, bFaceLeft);
			// Force a post-camera AI/Anim Update to ensure the pose is updated this frame
			playerPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
			playerPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		}

		playerPed.GetPedIntelligence()->AddTaskDefault(playerPed.ComputeDefaultTask(playerPed));
	}

	TUNE_GROUP_BOOL(PLAYER_SWITCH_TUNE, AUTO_SWITCH_WEAPON_IF_OUT_OF_AMMO, true);
	if (AUTO_SWITCH_WEAPON_IF_OUT_OF_AMMO)
	{
		if (newPlayerPed.GetWeaponManager() && newPlayerPed.GetInventory())
		{
			const CWeaponInfo* pWeaponInfo = newPlayerPed.GetWeaponManager()->GetEquippedWeaponInfo();
			if (pWeaponInfo && newPlayerPed.GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo))
			{
				newPlayerPed.GetWeaponManager()->EquipBestWeapon();
			}

			// Hack for B*1572354, insta-refill rpg ammo for the new player ped if they were reloading the rpg
			TUNE_GROUP_BOOL(PLAYER_SWITCH_HACK, USE_HACK_2, true);
			if (USE_HACK_2 && bNewPlayerPedWasReloading)
			{
				CWeapon* pWeapon = newPlayerPed.GetWeaponManager()->GetEquippedWeapon();
				if (pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetIsRpg())
				{
					CWeapon::RefillPedsWeaponAmmoInstant(newPlayerPed);
				}
			}
		}
	}

	CPlayerSpecialAbility* specialAbility = playerPed.GetSpecialAbility();
	if (specialAbility)
	{
		specialAbility->Deactivate();
	}

	// If the new player is in a vehicle, then ensure that this vehicle is real and not in any dummy mode
	if(newPlayerPed.GetVehiclePedInside() && newPlayerPed.GetVehiclePedInside()->GetVehicleAiLod().GetDummyMode()!=VDM_REAL)
	{
		const bool bSkipClearanceTest = true;
		newPlayerPed.GetVehiclePedInside()->TryToMakeFromDummy(bSkipClearanceTest);
		newPlayerPed.GetVehiclePedInside()->m_nVehicleFlags.uAllowedDummyConversion = 0;
		CVehicleAILodManager::DisableTimeslicingImmediately(*newPlayerPed.GetVehiclePedInside());
		Assertf(!newPlayerPed.GetVehiclePedInside()->IsDummy(), "Hotswapped player vehicle could not be converted from dummy to real.");
	}

	//This needs to be reset in the player - If this flag is set cop's dont scan for crooks and their wanted level's
	if (netGameInProgress && newPlayerPed.IsAPlayerPed())
	{
		newPlayerPed.SetBlockingOfNonTemporaryEvents(false);
	}

	StatsInterface::SetStatsModelPrefix(&newPlayerPed);
	CNewHud::HandleCharacterChange();

	// clear the entity pointer for any wanted incidents for this player, as the existing wanted incident will now be left pointing to the old player. This screws up the network syncing
	if (NetworkInterface::IsGameInProgress())
	{
		CIncidentManager::GetInstance().RemoveEntityFromIncidents(playerPed);

		Assertf(newPlayerPed.GetPedModelInfo()->GetSpecialAbilityType() == SAT_NONE, "Ped type %s should not have a special ability set!", newPlayerPed.GetModelName());
	}

	newPlayerPed.InitSpecialAbility(); // this uses the stats prefix set above

	if (playerPed.GetPedVfx())
	{
		playerPed.GetPedVfx()->Init(&playerPed);
	}

	if (newPlayerPed.GetPedVfx())
	{
		newPlayerPed.GetPedVfx()->Init(&newPlayerPed);
	}

	if (newPlayerPed.IsNetworkClone() && !bPlayerVisible)
	{
		playerPed.SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false);
		newPlayerPed.SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false);
	}

	// Update leg IK mode
	newPlayerPed.m_PedConfigFlags.SetPedLegIkMode(CIkManager::GetDefaultLegIkMode(&newPlayerPed));
	playerPed.m_PedConfigFlags.SetPedLegIkMode(CIkManager::GetDefaultLegIkMode(&playerPed));

	//Update local player scar data.
	if (NetworkInterface::IsGameInProgress() && newPlayerPed.IsLocalPlayer())
	{
		StatsInterface::ApplyPedScarData(&newPlayerPed, StatsInterface::GetCurrentMultiplayerCharaterSlot());
	}

	UpdateLensFlareAfterPlayerSwitch(&newPlayerPed);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && FindPlayerPed() == &newPlayerPed)
	{
		CReplayMgr::RecordFx(CPacketPlayerSwitch(), &newPlayerPed, &playerPed);
	}
#endif // GTA_REPLAY

	CVehicle::Pool* vehiclePool = CVehicle::GetPool();
	const u32 NUM_VEHICLES = (u32) vehiclePool->GetSize();
	for(unsigned vehicleIndex = 0; vehicleIndex < NUM_VEHICLES; vehicleIndex++)
	{
		CVehicle* pVeh = vehiclePool->GetSlot(vehicleIndex);
		if(pVeh)
		{
			for(int i = 0; i < CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; i++)
			{
				if(pVeh->GetExclusiveDriverPed(i) == &playerPed)
				{
					pVeh->SetExclusiveDriverPed(&newPlayerPed, i);
				}
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Stores the cover info for the player ped we're switching from
//////////////////////////////////////////////////////////////////////
bool CGameWorld::StoreCoverTaskInfoForCurrentPlayer(CPed& playerPed, bool& bFacingLeft)
{
	// Check if we are currently using cover, if so, store the cover point details in the static
	// as the local player normally stores a dynamic cover point, which won't exist for the new ped unless we store it elsewhere
	CCoverPoint* pOldPlayerCoverPoint = playerPed.GetCoverPoint();
	CTaskInCover* pCoverTask = static_cast<CTaskInCover*>(playerPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if (pCoverTask && pOldPlayerCoverPoint)
	{
		// Store the facing direction so we can put the ai ped in correctly
		bFacingLeft = pCoverTask->IsFacingLeft();
		// Release our existing cover point
		playerPed.ReleaseCoverPoint();
		// Copy our cover details into the peds switch coverpoint
		CTaskInCover::GetStaticCoverPointForPlayer(&playerPed).Copy(*pOldPlayerCoverPoint);
		// Mark the ped to go into cover when it turns to ai control
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Stores the cover info for the ai ped we're switching to
//////////////////////////////////////////////////////////////////////
bool CGameWorld::StoreCoverTaskInfoForAIPed(CPed& aiPed)
{
	// Check if the ped we're switching to is using cover
	CCoverPoint* pNewPedsCoverPoint = aiPed.GetCoverPoint();

	if (aiPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER) && pNewPedsCoverPoint)
	{
		// Release the peds existing cover point
		aiPed.ReleaseCoverPoint();
		// Copy our cover details into the peds switch coverpoint
		CTaskInCover::GetStaticCoverPointForPlayer(&aiPed).Copy(*pNewPedsCoverPoint);
		// Mark the ped to go into cover when it turns to player control
		return true;
	}
	else if (aiPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_COVER) && pNewPedsCoverPoint)
	{
		aiPed.SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Puts the new player ped into cover
//////////////////////////////////////////////////////////////////////
void CGameWorld::RestoreCoverTaskInfoForNewPlayer(CPed& newPlayerPed)
{
	// Clear out the cover entry entity, it's no longer valid as we're switching to another ped
	CPlayerInfo::ms_DynamicCoverHelper.SetCoverEntryEntity(NULL);
	newPlayerPed.GetPedIntelligence()->FlushImmediately();
	CTaskInCover::GetStaticCoverPointForPlayer(&newPlayerPed).ReserveCoverPointForPed(&newPlayerPed);
	newPlayerPed.SetCoverPoint(&CTaskInCover::GetStaticCoverPointForPlayer(&newPlayerPed));
	TUNE_GROUP_BOOL(COVER_TUNE, WARP_TO_COVER_POSITION_ON_SWITCH, true);
	if (WARP_TO_COVER_POSITION_ON_SWITCH)
	{
		Vector3 vPedPos;
		if (newPlayerPed.GetCoverPoint()->GetCoverPointPosition(vPedPos))
		{
			vPedPos.z += 1.0f;
			TUNE_GROUP_FLOAT(COVER_TUNE, MAX_DIFF_TO_SET_GROUNDZ, 0.25f, 0.0f, 1.0f, 0.01f);
			float fGroundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, vPedPos);
			if (Abs(fGroundZ - vPedPos.z) < MAX_DIFF_TO_SET_GROUNDZ)
			{
				vPedPos.z = fGroundZ;
			}
			newPlayerPed.SetPosition(vPedPos);

			TUNE_GROUP_FLOAT(COVER_TUNE, MAX_XY_DIST_TO_COVER, 0.25f, 0.0f, 1.0f, 0.01f);
			const float fXYDist2 = (VEC3V_TO_VECTOR3(newPlayerPed.GetTransform().GetPosition()) - vPedPos).XYMag2();
			const bool bConsideredAwayFromCover = fXYDist2 < square(MAX_XY_DIST_TO_COVER);

			Vector3 vCoverDirection = VEC3V_TO_VECTOR3(newPlayerPed.GetCoverPoint()->GetCoverDirectionVector());
			
			// Choose the closest heading to face the ped in cover
			float fPedHeading = rage::Atan2f(-vCoverDirection.x, vCoverDirection.y);
			const float fLeftCoverHeading = fPedHeading + HALF_PI;
			const float fRightCoverHeading = fPedHeading - HALF_PI;
			if (bConsideredAwayFromCover && newPlayerPed.GetCoverPoint()->GetUsage() == CCoverPoint::COVUSE_WALLTOLEFT)
			{
				fPedHeading = fRightCoverHeading;
			}
			else if (bConsideredAwayFromCover && newPlayerPed.GetCoverPoint()->GetUsage() == CCoverPoint::COVUSE_WALLTORIGHT)
			{
				fPedHeading = fLeftCoverHeading;
			}
			else
			{
				const float fLeftHeadingDelta = fwAngle::LimitRadianAngle(newPlayerPed.GetCurrentHeading() - fLeftCoverHeading);
				const float fRightHeadingDelta = fwAngle::LimitRadianAngle(newPlayerPed.GetCurrentHeading() - fRightCoverHeading);
				if (Abs(fLeftHeadingDelta) < Abs(fRightHeadingDelta))
				{
					fPedHeading = fLeftCoverHeading;
				}
				else
				{
					fPedHeading = fRightCoverHeading;
				}
			}
			fPedHeading = fwAngle::LimitRadianAngle(fPedHeading);
			newPlayerPed.SetHeading(fPedHeading);
		}
	}
	CTaskPlayerOnFoot* pPlayerTask = rage_new CTaskPlayerOnFoot();
	pPlayerTask->SetScriptedToGoIntoCover(true);
	newPlayerPed.GetPedIntelligence()->AddTaskDefault(pPlayerTask);
	newPlayerPed.SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);
	newPlayerPed.SetPlayerResetFlag(CPlayerResetFlags::PRF_SKIP_COVER_ENTRY_ANIM);
	newPlayerPed.SetIsInCover(2);
	//const bool bShouldFaceLeft = newPlayerPed.GetCoverPoint()->PedClosestToFacingLeft(newPlayerPed);
	//aiDisplayf("Frame %i, %s Ped %s %p Cover Info Restored, Facing Left ? %s", fwTimer::GetFrameCount(), newPlayerPed.IsLocalPlayer() ? "LOCAL PLAYER" : "AI PED", newPlayerPed.GetModelName(), &newPlayerPed, bShouldFaceLeft ? "TRUE" : "FALSE");
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Puts the new ai ped into cover
//////////////////////////////////////////////////////////////////////
void CGameWorld::RestoreCoverTaskInfoForNewAIPed(CPed& newAiPed, bool UNUSED_PARAM(bFaceLeft))
{
	newAiPed.GetPedIntelligence()->FlushImmediately();
	CTaskInCover::GetStaticCoverPointForPlayer(&newAiPed).ReserveCoverPointForPed(&newAiPed);
	newAiPed.SetCoverPoint(&CTaskInCover::GetStaticCoverPointForPlayer(&newAiPed));
	// Create a fake target
	Vector3 vCoverDirection = VEC3V_TO_VECTOR3(newAiPed.GetCoverPoint()->GetCoverDirectionVector()) * 5.0f;
	const bool bShouldFaceLeft = newAiPed.GetCoverPoint()->PedClosestToFacingLeft(newAiPed);
	s32 iCoverFlags = bShouldFaceLeft ? CTaskCover::CF_PutPedDirectlyIntoCover | CTaskCover::CF_SpecifyInitialHeading | CTaskCover::CF_SkipIdleCoverTransition | CTaskCover::CF_FacingLeft : CTaskCover::CF_PutPedDirectlyIntoCover | CTaskCover::CF_SpecifyInitialHeading | CTaskCover::CF_SkipIdleCoverTransition;
	iCoverFlags |= CTaskCover::CF_DisableAimingAndPeeking;
	vCoverDirection.RotateZ(bShouldFaceLeft ? QUARTER_PI : -QUARTER_PI);
	Vector3 vTargetPos = VEC3V_TO_VECTOR3(newAiPed.GetTransform().GetPosition()) + vCoverDirection;
	newAiPed.GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskCover(CWeaponTarget(vTargetPos), iCoverFlags), PED_TASK_PRIORITY_PRIMARY);
	newAiPed.SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);
	newAiPed.SetIsInCover(2);
	//aiDisplayf("Frame %i, %s Ped %s %p Cover Info Restored, Facing Left ? %s", fwTimer::GetFrameCount(), newAiPed.IsLocalPlayer() ? "LOCAL PLAYER" : "AI PED", newAiPed.GetModelName(), &newAiPed, bShouldFaceLeft ? "TRUE" : "FALSE");
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the player info of the local player
//////////////////////////////////////////////////////////////////////
CPlayerInfo* CGameWorld::GetMainPlayerInfo()
{
	CPed* pLocalPlayer = FindLocalPlayer();
	return (pLocalPlayer ? pLocalPlayer->GetPlayerInfo() : NULL);
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the player info of the local player
//////////////////////////////////////////////////////////////////////
CPlayerInfo* CGameWorld::GetMainPlayerInfoSafe()
{
	CPed* pLocalPlayer = FindLocalPlayerSafe();
	return (pLocalPlayer ? pLocalPlayer->GetPlayerInfo() : NULL);
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players group
//////////////////////////////////////////////////////////////////////
CPedGroup*	CGameWorld::FindLocalPlayerGroup()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return NULL;
	}
	return pPlayer->GetPedsGroup();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players vehicle
//////////////////////////////////////////////////////////////////////
CVehicle*	CGameWorld::FindLocalPlayerVehicle(bool UNUSED_PARAM(bReturnRemoteVehicle))
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
		return NULL;

	if (pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		//		if(bReturnRemoteVehicle && pPlayer->GetPlayerInfo()->GetRemoteVehicle())
		//			return pPlayer->GetPlayerInfo()->GetRemoteVehicle();
		//		else
		return (pPlayer->GetMyVehicle());
	}

	return NULL;
}

CPed*		CGameWorld::FindLocalPlayerMount()
{
	CPed* pPlayer = FindLocalPlayer();

	if (pPlayer && pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ))
	{
		return pPlayer->GetMyMount();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players train if they are in one
//////////////////////////////////////////////////////////////////////
CVehicle*	CGameWorld::FindLocalPlayerTrain()
{
	CVehicle* pVehicle = FindLocalPlayerVehicle();
	if( !pVehicle )
	{
		return NULL;
	}

	if (pVehicle->InheritsFromTrain())
	{
		return pVehicle;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players wanted level
//////////////////////////////////////////////////////////////////////
CWanted*		CGameWorld::FindLocalPlayerWanted()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return NULL;
	}

	return pPlayer->GetPlayerWanted();
}

CWanted*		CGameWorld::FindLocalPlayerWanted_Safe()
{
	CPed* pPlayer = FindLocalPlayerSafe();
	if( !pPlayer )
	{
		return NULL;
	}

	return pPlayer->GetPlayerWanted();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players arrest state
//////////////////////////////////////////////////////////////////////
eArrestState		CGameWorld::FindLocalPlayerArrestState()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return eArrestState::ArrestState_None;
	}

	return pPlayer->GetArrestState();
}

eArrestState		CGameWorld::FindLocalPlayerArrestState_Safe()
{
	CPed* pPlayer = FindLocalPlayerSafe();
	if( !pPlayer )
	{
		return eArrestState::ArrestState_None;
	}

	return pPlayer->GetArrestState();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players death state
//////////////////////////////////////////////////////////////////////
eDeathState		CGameWorld::FindLocalPlayerDeathState()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return eDeathState::DeathState_Alive;
	}

	return pPlayer->GetDeathState();
}

eDeathState		CGameWorld::FindLocalPlayerDeathState_Safe()
{
	CPed* pPlayer = FindLocalPlayerSafe();
	if( !pPlayer )
	{
		return eDeathState::DeathState_Alive;
	}

	return pPlayer->GetDeathState();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players health level
//////////////////////////////////////////////////////////////////////
float		CGameWorld::FindLocalPlayerHealth()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return 0.0f;
	}

	return pPlayer->GetHealth();
}

float		CGameWorld::FindLocalPlayerHealth_Safe()
{
	CPed* pPlayer = FindLocalPlayerSafe();
	if( !pPlayer )
	{
		return 0.0f;
	}

	return pPlayer->GetHealth();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players max health level
//////////////////////////////////////////////////////////////////////
float		CGameWorld::FindLocalPlayerMaxHealth()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return 0.0f;
	}

	return pPlayer->GetMaxHealth();
}

float		CGameWorld::FindLocalPlayerMaxHealth_Safe()
{
	CPed* pPlayer = FindLocalPlayerSafe();
	if( !pPlayer )
	{
		return 0.0f;
	}

	return pPlayer->GetMaxHealth();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players heading
//////////////////////////////////////////////////////////////////////
float		CGameWorld::FindLocalPlayerHeading()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return 0.0f;
	}

	return pPlayer->GetTransform().GetHeading();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players height
//////////////////////////////////////////////////////////////////////
float		CGameWorld::FindLocalPlayerHeight()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return 0.0f;
	}

	return pPlayer->GetTransform().GetPosition().GetZf();
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players position
//////////////////////////////////////////////////////////////////////
Vector3	CGameWorld::FindLocalPlayerCoors()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		Assertf( ms_playerFallbackPosSet, "FindLocalPlayerCoors called when no local player!" );
		return ms_playerFallbackPos;
	}

	return VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
}

Vector3	CGameWorld::FindLocalPlayerCoors_Safe()
{
	CPed* pPlayer = FindLocalPlayerSafe();
	if( !pPlayer )
	{
		Assertf( ms_playerFallbackPosSet, "FindLocalPlayerCoors called when no local player!" );
		return ms_playerFallbackPos;
	}

	return VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the local players speed
//////////////////////////////////////////////////////////////////////
Vector3		CGameWorld::FindLocalPlayerSpeed()
{
	CPed* pPlayer = FindLocalPlayer();
	if( !pPlayer )
	{
		return Vector3(0.0f, 0.0f, 0.0f);
	}
	CVehicle* vehicle = FindLocalPlayerVehicle();
	if (vehicle)
	{
		return (vehicle->GetVelocity());
	}
	else
	{
		return (pPlayer->GetVelocity());
	}
}

//////////////////////////////////////////////////////////////////////
// FUNCTION : Returns the Centre of the world for loading, population spawning etc...
//////////////////////////////////////////////////////////////////////
Vector3		CGameWorld::FindLocalPlayerCentreOfWorld()
{
	return FindLocalPlayerCoors();
}

CPed*	CGameWorld::FindPlayer(PhysicalPlayerIndex playerIndex)
{
	if (playerIndex == 0)
	{
		return FindLocalPlayer();
	}

	if (NetworkInterface::IsNetworkOpen())
	{
		CNetGamePlayer *pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(playerIndex);

		if (pPlayer)
		{
			return pPlayer->GetPlayerPed();
		}
	}

	return NULL;
}

void CGameWorld::SetPlayerFallbackPos(const Vector3 &pos)
{
	ms_playerFallbackPos = pos;
	ms_playerFallbackPosSet = true;
}

void CGameWorld::ClearPlayerFallbackPos()
{
	ms_playerFallbackPos.Zero();
	ms_playerFallbackPosSet = false;
}

CPed*	 
CGameWorld::FindFollowPlayer()
{
	if (NetworkInterface::IsInSpectatorMode() && AssertVerify(NetworkInterface::IsGameInProgress()))
	{
		netObject* netobj = NetworkInterface::GetNetworkObject(NetworkInterface::GetSpectatorObjectId());
		if (netobj)
		{
			NetworkObjectType objtype = netobj->GetObjectType();
			if (AssertVerify(objtype == NET_OBJ_TYPE_PLAYER || objtype == NET_OBJ_TYPE_PED))
			{
				CPed* ped = static_cast<CPed*>(static_cast<CNetObjPed*>(netobj)->GetGameObject());
				if (AssertVerify(ped))
				{
					return ped;
				}
			}
		}
		else
		{
			gnetDebug1("Reset local player spectator mode");
			NetworkInterface::SetSpectatorObjectId(NETWORK_INVALID_OBJECT_ID);
		}
	}

	return FindLocalPlayer();
}

CVehicle* 
CGameWorld::FindFollowPlayerVehicle()
{
	CPed* pPlayer = FindFollowPlayer();
	if (pPlayer && pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return (pPlayer->GetMyVehicle());
	}

	return FindLocalPlayerVehicle();
}

float         
CGameWorld::FindFollowPlayerHeading()
{
	CPed* pPlayer = FindFollowPlayer();
	if(pPlayer)
	{
		return pPlayer->GetTransform().GetHeading();
	}

	return FindLocalPlayerHeading();
}

float         
CGameWorld::FindFollowPlayerHeight()
{
	CPed* pPlayer = FindFollowPlayer();
	if(pPlayer)
	{
		return pPlayer->GetTransform().GetPosition().GetZf();
	}

	return FindLocalPlayerHeight();
}

Vector3		
CGameWorld::FindFollowPlayerCoors()
{
	CPed* pPlayer = FindFollowPlayer();
	if(pPlayer)
	{
		return VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	}

	return FindLocalPlayerCoors();
}

Vector3		
CGameWorld::FindFollowPlayerSpeed()
{
	CPed* pPlayer = FindFollowPlayer();

	// This part used to be:
	//	CVehicle* vehicle = FindFollowPlayerVehicle();
	// but doing it like this eliminates one of the calls to FindFollowPlayer(),
	// which may be somewhat expensive in MP.
	CVehicle* vehicle;
	if(pPlayer && pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		vehicle = pPlayer->GetMyVehicle();
	}
	else
	{
		vehicle = FindLocalPlayerVehicle();
	}

	if (vehicle)
	{
		return (vehicle->GetVelocity());
	}
	else if(pPlayer)
	{
		return pPlayer->GetVelocity();
	}

	return FindLocalPlayerSpeed();
}

Vector3		
CGameWorld::FindFollowPlayerCentreOfWorld()
{
	return FindFollowPlayerCoors();
}

///////////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateEntityDesc
// PURPOSE:		Caches data from entity to entity descriptor
///////////////////////////////////////////////////////////////////////////////

void CGameWorld::UpdateEntityDesc(fwEntity* entity, fwEntityDesc* entityDesc)
{
	SceneGraphLockedFatalAssert();

	const CEntity*	RESTRICT pEntity = static_cast< const CEntity *RESTRICT >( entity );

	SceneGraphAssertf( pEntity && entityDesc, "Trying to cache data from an pEntity to a descriptor but either of them is null" );
	if ( !( pEntity && entityDesc ) )
		return;

#if __BANK
	if (ms_UpdateEntityDesc_StopOnSelectedEntity && g_PickerManager.GetSelectedEntity() == entityDesc->GetEntity())
	{
		__debugbreak();
	}
#endif
	// set pEntity ptr
	entityDesc->SetEntity( entity );

	spdAABB		aabb;
	spdSphere	sphere;
	Vec3V		lodPos;

	// set bound shapes
	if( pEntity->GetIsDynamic() )
	{
		// Certain entities (mainly glass) uses wider larger difference bounds
		CDynamicEntity *dynamicEntity = (CDynamicEntity*)pEntity;
		dynamicEntity->GetExtendedBoundBox( aabb );							// DO NOT use GetBoundingBoxMin/Max, as they are not transformed to world coordinate space!
		Vector3 center;
		float radius = dynamicEntity->GetExtendedBoundCentreAndRadius( center );
		sphere.Set( VECTOR3_TO_VEC3V(center), ScalarV(radius) );
	}
	else if(pEntity->GetIsTypeLight() || pEntity->GetIsTypePortal())
	{
		// DO NOT use GetBoundingBoxMin/Max, as they are not transformed to world coordinate space!
		aabb = pEntity->GetBoundBox();
		sphere = pEntity->GetBoundSphere();
	}
	else
	{
		// Calling this function is faster than fetching each bound individually
		pEntity->GetBounds(aabb, sphere);
	}

	// set vec to parent lod position.
	Vector3 vBasisPivot;

	if ( pEntity->GetIsTypeLight() )
	{
		// At the moment all lights have an identity transform, that is worked around (probably for performance
		// reasons) in the rest of engine code, but that causes their pivot to be always placed at 0,0,0.
		// Use the center of their bounding box until that gets fixed. - AP
		vBasisPivot = VEC3V_TO_VECTOR3( aabb.GetCenter() );
	}
	else
		g_LodMgr.GetBasisPivot(entity, vBasisPivot);

	entityDesc->SetApproxBoundingBoxAndSphereAndVecToLod( aabb, sphere, RCC_VEC3V(vBasisPivot) );

	const fwFlags16 visFlags = pEntity->GetRenderPhaseVisibilityMask();

	// cache useful data from CEntity
	int entityType = pEntity->GetType();
	entityDesc->SetEntityType( entityType );
	entityDesc->SetRenderPhaseVisibilityMask( visFlags );
	entityDesc->SetVisibilityType( pEntity->GetVisibilityType() );
	entityDesc->SetDontCullSmallShadow( pEntity->IsBaseFlagSet(fwEntity::RENDER_SMALL_SHADOW) || pEntity->GetIsDynamic());
	entityDesc->SetIsVisible( pEntity->GetIsVisible() );
	entityDesc->SetIsSearchable( pEntity->GetIsSearchable() );
	entityDesc->SetNeverDummy( pEntity->m_nFlags.bNeverDummy );
	entityDesc->SetIsProcedural( pEntity->GetOwnedBy()==ENTITY_OWNEDBY_PROCEDURAL );

	if (entityType == ENTITY_TYPE_OBJECT || entityType == ENTITY_TYPE_DUMMY_OBJECT)
	{
		CBaseModelInfo *pModelInfo = (CBaseModelInfo *)pEntity->GetArchetype();
		Assertf(pModelInfo, "Entity has no modelinfo");
		entityDesc->SetIsADoor(pModelInfo->GetUsesDoorPhysics());
	}
	else
	{
		entityDesc->SetIsADoor(false);
	}

	if (pEntity->GetIsTypeComposite())
	{
		// stop composite entities coming through visibility pipeline - IanK
		entityDesc->SetIsVisible(false);
		entityDesc->SetIsSearchable(false);
	}

	// take copy of lodtree access details
	entityDesc->SetLodType( pEntity->GetLodData().GetLodType() );
	entityDesc->SetIsRootLod( pEntity->GetLodData().IsContainerLod() && !pEntity->GetLodData().HasLod() );

	// cache lod dist
	entityDesc->SetLodDistCached( (s16) pEntity->GetLodDistance() );

	bool pedIsInVehicle = pEntity->GetIsTypePed() && static_cast<const CPed *RESTRICT>(pEntity)->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle);
	entityDesc->SetIsPedInVehicle(pedIsInVehicle);

	entityDesc->SetHDTexCapable(pEntity->GetIsHDTxdCapable());

#if STREAMING_VISUALIZE
	bool strVsUpdate = true;
#endif

	entityDesc->SetStraddlingPortal(false);
	entityDesc->SetUseMaxDistanceForWaterReflection(pEntity->m_nFlags.bUseMaxDistanceForWaterReflection);

	if (pEntity->GetIsDynamic())
	{
		CDynamicEntity *RESTRICT pDE = static_cast<CDynamicEntity *RESTRICT>(entity);
		entityDesc->SetStraddlingPortal(pDE->m_nDEflags.bIsStraddlingPortal);
		if (pDE->m_nDEflags.bIsStraddlingPortal)
		{
			pDE->UpdateStraddledPortalEntityDesc();
#if STREAMING_VISUALIZE
			strVsUpdate = false;
#endif
		}
	}

#if STREAMING_VISUALIZE
	if (strVsUpdate)
	{
		STRVIS_UPDATE_ENTITY(entity);
	}
#endif
}

