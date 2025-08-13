/////////////////////////////////////////////////////////////////////////////////
// Title	:	Scene.cpp
// Author(s):	Obbe?, Alexander Illes, Adam Croston
// Started	:	??
//
// description:	class containing all scene information
/////////////////////////////////////////////////////////////////////////////////
#include "scene/scene.h"

// Rage headers
#include "profile/timebars.h"
#include "system/stack.h"

// Framework headers
#include "entity/dynamicarchetype.h"
#include "entity/dynamicentitycomponent.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/move.h"
#include "grcore/debugdraw.h"
#include "fwdebug/picker.h"
#include "fwutil/PtrList.h"
#include "fwutil/QuadTree.h"
#include "fwscene/lod/LodAttach.h"
#include "fwscene/mapdata/mapdatadebug.h"
#include "fwscene/mapdata/mapdatacontents.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/world/StreamedSceneGraphNode.h"
#include "fwscene/lod/ContainerLod.h"
#include "fwsys/metadatastore.h"
#include "fwtl/debugownerhistory.h"
#include "fwpheffects/ropedatamanager.h"

// Game headers
#include "ai\debug\system\AIDebugLogManager.h"
#include "animation/move.h"
#include "animation/MovePed.h"
#include "animation/MoveObject.h"
#include "animation/MoveVehicle.h"
#include "audio/northaudioengine.h"
//#include "audio/audiogeometry.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/ViewportManager.h"
#include "Cloth/Cloth.h"
#include "Cloth/ClothArchetype.h"
#include "Cloth/ClothRageInterface.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "control/route.h"
#include "control/trafficlights.h"
#include "core/game.h"
#include "debug/AssetAnalysis/StreamingIteratorManager.h"
#include "debug/BoundsDebug.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "debug/DebugDraw/DebugWindow.h"
#if __DEV
#include "debug/DebugDraw/DebugVolume2.h"
#include "debug/DebugDraw/DebugHangingCableTest.h"
#endif // __DEV
#include "debug/Editing/LiveEditing.h"
#include "debug/LightProbe.h"
#include "debug/Rendering/DebugView.h"
#include "debug/textureviewer/textureviewer.h"
#include "debug/UiGadget/UiEvent.h"
#include "debug/UiGadget/UiGadgetBase.h"
#include "event/events.h"
#include "event/eventnetwork.h"
#include "frontend/PauseMenu.h"
#include "frontend/GolfTrail.h"
#include "game/dispatch/Incidents.h"
#include "game/dispatch/Orders.h"
#include "game/user.h"
#include "ik/IkManager.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "objects/DummyObject.h"
#include "objects/object.h"
#include "objects/ObjectIntelligence.h"
#include "objects/objectpopulation.h"
#include "pathserver/ExportCollision.h"		// CNavMeshDataExporter::WillExportCollision()
#include "pathserver/pathserver.h"
#include "pathserver/navedit.h"
#include "peds/Ped.h"
#include "peds/PedDebugVisualiser.h"
#include "peds/PedGeometryAnalyser.h"
#include "peds/PedIntelligence.h"
#include "peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "peds/pedpopulation.h"
#include "peds/popcycle.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "physics/physics.h"
#include "glassPaneSyncing/GlassPaneManager.h"
#include "Pickups/PickupManager.h"
#include "pickups/Pickup.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/gtadrawable.h"
#include "renderer/lights/AsyncLightOcclusionMgr.h"
#include "renderer/lights/lights.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/MeshBlendManager.h"
#include "renderer/mirrors.h"
#include "renderer/occlusion.h"
#include "renderer/PlantsMgr.h"
#include "renderer/renderer.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/River.h"
#include "renderer/TreeImposters.h"
#include "renderer/water.h"
#include "renderer/HorizonObjects.h"
#include "renderer/shadows/shadows.h"
#include "renderer/RenderTargetMgr.h"
#include "renderer/Util/ShaderUtil.h"
#include "SaveLoad/GenericGameStorage.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/entities/compEntity.h"
#include "scene/DataFileMgr.h"
#include "scene/debug/MapOptimizeHelper.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/debug/SceneCostTracker.h"
#include "scene/debug/SceneIsolator.h"
#include "scene/ExtraContent.h"
#include "scene/EntityExt_AltGfx.h"
#include "scene/EntityBatch.h"
#include "scene/FileLoader.h"
#include "scene/FocusEntity.h"
#include "scene/InstancePriority.h"
#include "scene/ipl/IplCullBoxEditor.h"
#include "scene/LoadScene.h"
#include "scene/loader/ManagedImapGroup.h"
#include "scene/loader/mapTypes.h"
#include "scene/loader/MapData.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/AttributeDebug.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/portals/InteriorGroupManager.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/InteriorProxy.h"
#include "Scene/portals/Portal.h"
#include "scene/portals/portalInst.h"
#include "scene/world/GameScan.h"
#include "scene/debug/SceneStreamerDebug.h"
#include "scene/texLod.h"
#include "scene/volume/VolumeTool.h"
#include "scene/world/gameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/world/WorldDebugHelper.h"
#include "scene/worldpoints.h"
#include "scene/scene_channel.h"
#include "vfx/misc/LODLightManager.h"
#include "shaders/shaderlib.h"
#include "Shaders/ShaderEdit.h"
#include "streaming/packfilemanager.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "task/Combat/Cover/Cover.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/General/TaskBasic.h"
#include "task/Movement/TaskNavBase.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/TaskHelperFSM.h"
#include "TimeCycle/TimeCycleAsyncQueryMgr.h"
#include "vehicleAi/JunctionEditor.h"
#include "vehicleAI/vehicleintelligence.h"
#include "vehicles/cargen.h"
#include "vehicles/LandingGear.h"
#include "vehicles/Vehicle.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/VehicleLightAsyncMgr.h"
#include "vehicles/vehiclepopulation.h"
#include "Vfx/Misc/DistantLights.h"
#include "Vfx/Particles/PtFxEntity.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/vehicleglass/VehicleGlassComponent.h"
#include "weapons/explosion.h"
#include "weapons/explosionInst.h"
#include "weapons/firingpattern.h"
#include "weapons/thrownweaponinfo.h"
#include "Objects/Door.h"
#include "modelinfo/WeaponModelInfo.h"
#include "debug/Editing/MapEditRESTInterface.h"

#if RSG_PC
#include "core/app.h"
#endif // RSG_PC

#if __DEV
//OPTIMISATIONS_OFF()
#endif

#if !__FINAL
#include "animation/debug/AnimDebug.h"
#endif // !__FINAL
#if __PS3
#include "streaming/streaminginstall_psn.h"
#endif // __PS3

RAGE_DEFINE_CHANNEL(scene)
RAGE_DEFINE_SUBCHANNEL(scene, world)
RAGE_DEFINE_SUBCHANNEL(scene, visibility)

#if !__NO_OUTPUT
static void visibilityDebugf1StackLine(size_t addr, const char *sym, size_t offset) {
	visibilityDebugf1( "%8" SIZETFMT "x - %s+%" SIZETFMT "x", addr, sym, offset );
}

void visibilityDebugf1StackTraceImpl() {
	sysStack::PrintStackTrace( visibilityDebugf1StackLine );
}

static void worldDebugf1StackLine(size_t addr, const char *sym, size_t offset) {
	worldDebugf1( "%8" SIZETFMT "x - %s+%" SIZETFMT "x", addr, sym, offset );
}

void worldDebugf1StackTraceImpl() {
	sysStack::PrintStackTrace( worldDebugf1StackLine );
}
#endif

// PS3: When this define is set some of the game pools are
// allocated from the sysmemcontainer. When the sony utilities
// need the container they are temporarily saved out to the
// hard drive's cache partition.
#define POOLS_IN_SYSMEMCONTAINER (__PS3 && 0)
#if POOLS_IN_SYSMEMCONTAINER
#include "system/sysmemcontainer_psn.h"
#include "system/simpleallocator.h"
#include <sysutil/sysutil_syscache.h>
static char s_SysCachePath[CELL_SYSCACHE_PATH_MAX];
static void SavePoolsForSysUtil(bool save)
{
	void* addr = SYSMEMCONTAINER.GetPreallocatedArea();
	int size = SYSMEMCONTAINER.GetPreallocatedSize();
	static bool streamingDisabled;
	if(save)
	{
		streamingDisabled = strStreamingEngine::IsStreamingDisabled();
		strStreamingEngine::DisableStreaming();
		fiStream* str = fiStream::Create(s_SysCachePath);
		AssertVerify(size == str->Write(addr, size));
		str->Close();
	}
	else
	{
		fiStream* str = fiStream::Open(s_SysCachePath);
		AssertVerify(size == str->Read(addr, size));
		str->Close();
		if(!streamingDisabled)
			strStreamingEngine::EnableStreaming();
	}
}
#endif // POOLS_IN_SYSMEMCONTAINER


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// CScene
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

#define TITLEUPDATE_IMG	"update:/update.img"

CLevelData	CScene::m_levelData;
int			CScene::m_currentLevelIndex=-1;

#if __BANK
bkBank*	CScene::ms_pBank = NULL;
#endif //__BANK

#if !__FINAL
void gGTADataAddressResolverHelper( const void *p_Thing, atString &retName, const void *p_DestructingAddrRef )
{
	// List of arrays to check
	fwBasePool *arrays[] = { CEvent::_ms_pPool, 0 };

	fwBasePool **pPoolCursor = &arrays[0];

	char scratchBuffer[64];
	while ( (*pPoolCursor) != 0 )
	{
		fwBasePool *pool = *pPoolCursor;

		// In this pool?
		if ( p_Thing >= pool->LowestAddress() && p_Thing < pool->HighestAddress()  )
		{
			// What index?
			int index = ptrdiff_t_to_int((const u8*)p_Thing - (u8*)pool->LowestAddress()) / (int)pool->GetStorageSize();
			Assert( index < pool->GetSize() );

			// Get the instance
			void *instance = pool->GetSlot(index);

			// Check for our destructing addr ref
			bool inProcessOfDestruction( false );
			if ( instance && p_DestructingAddrRef )
			{
				if ( p_DestructingAddrRef >= instance && p_DestructingAddrRef < reinterpret_cast<u8*>(instance) + pool->GetStorageSize() )
				{
					inProcessOfDestruction = true;
				}
			}
			sprintf( scratchBuffer, "0x%p { ", p_Thing );
			retName += scratchBuffer;
			// If you can use par gen to resolve a member to name from the pool type, set this to true.
			// otherwise it'll use a local offset.
			bool bResolvedMember( false );
			if ( inProcessOfDestruction || !instance )
			{
				retName += "dead ";
				retName += pool->GetName();
				retName += " thing";
			}
			else
			{
				if ( pool == CEvent::_ms_pPool )
				{
					retName += reinterpret_cast<const CEvent *>(instance)->GetName();
				}
				else
				{
					retName += pool->GetName();
					retName += " thing";
				}
			}

			if ( !bResolvedMember )
			{
				int delta = ptrdiff_t_to_int((const u8*)p_Thing - (u8*)instance);

				sprintf( scratchBuffer," [%p] + 0x%x", instance, delta );
				retName += scratchBuffer;
			}
			retName += " }";
			return;
		}
		++pPoolCursor;
	}
	sprintf( scratchBuffer, "0x%p", p_Thing );
	retName += scratchBuffer;
}

void gGTADataAddressResolver( const void *p_Thing, atString &nameRet, bool bOutsideDestructor, bool bIsPointerAndContentsWanted )
{
	const void *potentialDestructingObject = (bOutsideDestructor) ? NULL : p_Thing;
	if ( !bIsPointerAndContentsWanted )
	{
		gGTADataAddressResolverHelper( p_Thing, nameRet, potentialDestructingObject );
	}
	else
	{
		typedef void * const tpConstVoid;
		atString output;
		gGTADataAddressResolverHelper( p_Thing, nameRet, potentialDestructingObject );
		nameRet += " (points at ";
		gGTADataAddressResolverHelper( *reinterpret_cast< tpConstVoid*>(p_Thing), nameRet, potentialDestructingObject );
		nameRet += ")";
	}
}


#endif

//
// name:		InitPools
// description:	Initialise scene pools
void CGtaSceneInterface::InitPools()
{
	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// clears any pools registered for lookup via handles
	/// PoolLookup_Init();

#if POOLS_IN_SYSMEMCONTAINER
	// stick the dummies, vehicles & animblenders in the SYSMEMCONTAINER
	// we save them to the hard drive when the system needs the mem container
	u32 size = 5*1024*1024;
	sysMemSimpleAllocator alloc(SYSMEMCONTAINER.Preallocate(size, &SavePoolsForSysUtil), size);

	CellSysCacheParam param = {0};
	safecpy(param.cacheId, XEX_TITLE_ID, sizeof(param.cacheId));
	int code = cellSysCacheMount(&param);
	if(code < 0)
	{
		if (CPauseMenu::IsActive())
		{
			CPauseMenu::Close();
		}
		return;
	}
	formatf(s_SysCachePath, NELEM(s_SysCachePath), "%s/sysutil.tmp", param.getCachePath);
#endif

#if NAVMESH_EXPORT
	// When exporting collision for navmeshes we'll need larger pools
	if(CNavMeshDataExporter::WillExportCollision())
	{
		fwEntityContainer::InitEntityDescPool( 65536 );
		fwSceneGraph::InitPools();

		CAnimatedBuilding::InitPool(MEMBUCKET_WORLD);
		CBuilding::InitPool(80000, MEMBUCKET_WORLD);			// pool initially 30000
		CObject::InitPool(11000, MEMBUCKET_WORLD);				// pool initially 1300, 5000
		CDummyObject::InitPool(30000, MEMBUCKET_WORLD);			// 14000+
		CCompEntity::InitPool(MEMBUCKET_WORLD);
	}
	else
#endif // NAVMESH_EXPORT
	{
		fwEntityContainer::InitEntityDescPool();
		fwSceneGraph::InitPools();

		CAnimatedBuilding::InitPool( MEMBUCKET_WORLD );
		CBuilding::InitPool( MEMBUCKET_WORLD );
		
#if (__XENON || __PS3)
		#define OBJECT_HEAP_SIZE (640 << 10)
#else
		#define OBJECT_HEAP_SIZE ((640 << 10) * 10)
#endif

		CObject::InitPool(MEMBUCKET_WORLD);

		CCompEntity::InitPool( MEMBUCKET_WORLD );
#if POOLS_IN_SYSMEMCONTAINER
		sysMemAllocator::SetCurrent(alloc);
#endif
		CDummyObject::InitPool( MEMBUCKET_WORLD );
	}
	fwDynamicArchetypeComponent::InitPool( MEMBUCKET_WORLD );
	fwDynamicEntityComponent::InitPool( MEMBUCKET_WORLD );
	fwKnownRefHolder::InitPool(MEMBUCKET_WORLD);
	CEntityBatch::InitPool( MEMBUCKET_WORLD );
	CGrassBatch::InitPool( MEMBUCKET_WORLD );
	CInteriorInst::InitPool( MEMBUCKET_WORLD );
	CInteriorProxy::InitPool( MEMBUCKET_WORLD );
	CPortalInst::InitPool( MEMBUCKET_WORLD );
	CPickupManager::InitPools();
	CGlassPaneManager::InitPools();
	fwAnimDirectorPooledObject::InitPool( MEMBUCKET_ANIMATION );
	CMovePedPooledObject::InitPool( MEMBUCKET_ANIMATION );
	CMoveObjectPooledObject::InitPool( MEMBUCKET_ANIMATION );
	CMoveVehiclePooledObject::InitPool( MEMBUCKET_ANIMATION );
	CMoveAnimatedBuildingPooledObject::InitPool( MEMBUCKET_ANIMATION );
	CBaseIkManager::InitPools();
#if POOLS_IN_SYSMEMCONTAINER
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
#endif
	CPlayerInfo::InitPool( MEMBUCKET_GAMEPLAY );
	
#if (__XENON || __PS3)
	#define TASK_HEAP_SIZE (512 << 10)
#else
	#define TASK_HEAP_SIZE ((512 << 10) * 10)
#endif

	const size_t poolSize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("CTask", 0xDFFC5283), CONFIGURED_FROM_FILE);
	CTask::InitPool(TASK_HEAP_SIZE, poolSize, MEMBUCKET_GAMEPLAY);

#if TASK_DEBUG_ENABLED
#if __BANK
	CTask::GetPool()->RegisterPoolCallback(CTask::PoolFullCallback);
#endif 
#endif // TASK_DEBUG_ENABLED

	CTaskSequenceList::InitPool( MEMBUCKET_GAMEPLAY );
	CObjectIntelligence::InitPool( MEMBUCKET_GAMEPLAY );
	CPatrolRoutes::InitPools();
	CEvent::InitPool( MEMBUCKET_GAMEPLAY );
	CEventNetwork::InitPool( MEMBUCKET_GAMEPLAY );
	CPointRoute::InitPool( MEMBUCKET_GAMEPLAY );
	CPatrolRoute::InitPool( MEMBUCKET_GAMEPLAY );
	CNavMeshRoute::InitPool( MEMBUCKET_GAMEPLAY );

	phGtaExplosionInst::InitPool( MEMBUCKET_GAMEPLAY );

#if PTFX_ALLOW_INSTANCE_SORTING	
	CPtFxSortedEntity::InitPool( MEMBUCKET_FX );
#endif // PTFX_ALLOW_INSTANCE_SORTING	

	CVehicleGlassComponentEntity::InitPool( MEMBUCKET_FX );

#if NORTH_CLOTHS
	CCloth::InitPool( MEMBUCKET_PHYSICS );
	CClothArchetype::InitPool( MEMBUCKET_PHYSICS );
	phInstBehaviourCloth::InitPool( MEMBUCKET_PHYSICS );
	phInstCloth::InitPool( MEMBUCKET_PHYSICS );
#endif //RAGE_CLOTHS
	CLightEntity::InitPool( MEMBUCKET_RENDER );

	camFactory::InitPools();
	CLandingGearPartBase::InitPool( MEMBUCKET_GAMEPLAY );
    fwQuadTreeNode::InitPool( MEMBUCKET_WORLD );
	CIncident::InitPool( MEMBUCKET_GAMEPLAY );
	COrder::InitPool( MEMBUCKET_GAMEPLAY );

	fwContainerLod::InitPool( MEMBUCKET_WORLD );
	fwMapDataLoadedNode::InitPool( MEMBUCKET_WORLD );

	fwAttachmentEntityExtension::InitPool(MEMBUCKET_GAMEPLAY);
	CTaskUseScenarioEntityExtension::InitPool(MEMBUCKET_GAMEPLAY);

	CEntity::InitPoolLookupArray();

#if !__FINAL
	SetDebugOwnerHistoryResolver(&gGTADataAddressResolver);
#endif

}

//
// name:		ShutdownPools
// description:	Shutdown scene pools
void CGtaSceneInterface::ShutdownPools()
{
#if !__FINAL
	SetDebugOwnerHistoryResolver(NULL);
#endif
	fwSceneGraph::ShutdownPools();
	CBuilding::ShutdownPool();
	CObject::ShutdownPool();
	CCompEntity::ShutdownPool();
	CEntityBatch::ShutdownPool();
	CGrassBatch::ShutdownPool();
	CPickupManager::ShutdownPools();
	CGlassPaneManager::ShutdownPools();
	CDummyObject::ShutdownPool();
	fwAnimDirectorPooledObject::ShutdownPool();
	CMovePedPooledObject::ShutdownPool();
	CMoveObjectPooledObject::ShutdownPool();
	CMoveVehiclePooledObject::ShutdownPool();
	CBaseIkManager::ShutdownPools();
	CPlayerInfo::ShutdownPool();
	CTask::ShutdownPool();
	CTaskSequenceList::ShutdownPool();
	CPatrolRoutes::ShutdownPools();
	CEvent::ShutdownPool();
	CEventNetwork::ShutdownPool();
	CPointRoute::ShutdownPool();
	CPatrolRoute::ShutdownPool();
	CNavMeshRoute::ShutdownPool();
//	CPersistentPed::ShutdownPool();
//	CPersistentVehicle::ShutdownPool();
	CInteriorInst::ShutdownPool();
	CInteriorProxy::ShutdownPool();
	CPortalInst::ShutdownPool();
	phGtaExplosionInst::ShutdownPool();
#if PTFX_ALLOW_INSTANCE_SORTING	
	CPtFxSortedEntity::ShutdownPool();
#endif // PTFX_ALLOW_INSTANCE_SORTING
	CVehicleGlassComponentEntity::ShutdownPool();
	fwDynamicEntityComponent::ShutdownPool();
	fwDynamicArchetypeComponent::ShutdownPool();
	fwKnownRefHolder::ShutdownPool();
#if NORTH_CLOTHS
	CCloth::ShutdownPool();
	CClothArchetype::ShutdownPool();
	phInstBehaviourCloth::ShutdownPool();
	phInstCloth::ShutdownPool();
#endif // RAGE_CLOTHS
	CLightEntity::ShutdownPool();
	camFactory::ShutdownPools();
	CIncident::ShutdownPool();
	COrder::ShutdownPool();
	CTaskUseScenarioEntityExtension::ShutdownPool();
	fwAttachmentEntityExtension::ShutdownPool();

}

// PURPOSE: This interface is used for some game-specific extensions to CPathServer.
static CPathServerGameInterfaceGta s_PathServerGameInterface;

#if !__FINAL
	PARAM(disableGiveMeCheckerTexture,	"Disable fake checkerboard texture.");
#endif

//
// name:		CScene::Init()
// description:	Initialise CScene class
void CScene::Init(unsigned /*initMode*/)
{
	USE_MEMBUCKET(MEMBUCKET_WORLD); // WHAT RELIES ON THIS? It should be moved into all Inits that use it

	fwScene::InitClass(rage_new CGtaSceneInterface());
	fwScene::Init();

#if !__FINAL
	if(PARAM_disableGiveMeCheckerTexture.Get())
	{
		// do nothing
	}
	else
	{
		grcTextureFactory::GetInstance().RegisterTextureReference("givemechecker", grcTextureFactory::GetNotImplementedTexture());
	}
#endif

	CCover::Init(); // uses MEMBUCKET_WORLD	

	g_MapTypesStore.Init(INIT_CORE);
	INSTANCE_STORE.Init(INIT_CORE);// uses MEMBUCKET_WORLD

	CPortal::Init(INIT_CORE);

#if __BANK
	fwMapDataStore::SetInterface(&g_MapDataGameInterface);
#endif	//__DEV

#if LIGHT_VOLUME_USE_LOW_RESOLUTION || USE_DOWNSAMPLED_PARTICLES
	UpsampleHelper::Init();
#endif

	CVehicleLightAsyncMgr::Init();
	CTimeCycleAsyncQueryMgr::Init();
	CAsyncLightOcclusionMgr::Init();
	Lights::Init(); 
	CMirrors::Init(); 
	CModelInfo::Init(INIT_CORE); 
	ThePaths.Init(INIT_CORE);
	CPathServer::Init(s_PathServerGameInterface, CPathServer::EMultiThreaded);

#if NORTH_CLOTHS
	CPlantTuning::Init();
#endif
	CInteriorGroupManager::Init();
#if __DEV
	CTaskNMBehaviour::InitStrings(); // does nothing (asserts only)
#endif // __DEV
#if USE_TREE_IMPOSTERS
	CTreeImposters::Init();
#endif // USE_TREE_IMPOSTERS
	
	CVehicle::InitSystem();

	m_levelData.Init();

	gAltGfxMgr.InitSystem();

	// DEPENDENCIES 1

	CGameWorld::Init(INIT_CORE); // depends on InitPools()
	CPhysics::Init(INIT_CORE); // depends on InitPools() 
	Water::Init(INIT_CORE); // dependent on Lights::Init()
	g_HorizonObjects.Init(INIT_CORE);
	River::Init();

	// DEPENDENCIES 2

	CPed::InitSystem(); // dependent on CPhysics::Init()

	CTexLod::Init();

	CInstancePriority::Init();
}

//
// name:		CScene::Shutdown()
// description:	shutdown CScene class
void CScene::Shutdown(unsigned /*shutdownMode*/)
{
	m_levelData.Shutdown();
	CInteriorGroupManager::Shutdown();
	INSTANCE_STORE.Shutdown();
	g_MapTypesStore.Shutdown();
	CModelInfo::Shutdown(SHUTDOWN_CORE);
	CCover::Shutdown();
	CPedGeometryAnalyser::Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	CPathServer::Shutdown();
	CMirrors::Shutdown();
#if USE_TREE_IMPOSTERS
	CTreeImposters::Shutdown();
#endif // USE_TREE_IMPOSTERS	
	Water::Shutdown(SHUTDOWN_CORE);
	g_HorizonObjects.Shutdown(SHUTDOWN_CORE);
	CShaderLib::Shutdown();
	Lights::Shutdown();
	CAsyncLightOcclusionMgr::Shutdown();
	CTimeCycleAsyncQueryMgr::Shutdown();
	CVehicleLightAsyncMgr::Shutdown();

	CVehicle::ShutdownSystem();
	CPed::ShutdownSystem();
//RAGE	CCollision::Shutdown();
	CGameWorld::Shutdown1(SHUTDOWN_CORE);
	CPhysics::Shutdown(SHUTDOWN_CORE);
	fwScene::Shutdown();
	fwScene::ShutdownClass();

	ThePaths.Shutdown(SHUTDOWN_CORE);

	gAltGfxMgr.ShutdownSystem();

#if NORTH_CLOTHS
	CPlantTuning::Shutdown();
#endif

	CTexLod::Shutdown();
}

atHashString CScene::GetCurrentLevelNameHash()
{
	return m_currentLevelIndex!=-1?atHashString(m_levelData.GetTitleName(m_currentLevelIndex)):atHashString::Null();
}

//
// name:		LoadMap
// description:	Load a map given a level number
void CScene::LoadMap(s32 levelNumber)
{
	m_currentLevelIndex=levelNumber;
	LoadMap(m_levelData.GetFilename(levelNumber));
}

//
// name:		LoadMap
// description:	Load a map 
void CScene::LoadMap(const char* pFile)
{
	PF_START_STARTUPBAR("Paths");

	fwScene::LoadMap(pFile);

	DATAFILEMGR.RecallSnapshot();
	//RAGE	CColAccel::startCache();


	// register update:/update.img if it exists. This is used to load title update resources
	if(ASSET.Exists(TITLEUPDATE_IMG, NULL))
	{
		strPackfileManager::SetExtraContentIndex(0xff);
		strPackfileManager::AddImageToList(TITLEUPDATE_IMG, true, -1, false, 0, false, false);
		strPackfileManager::SetExtraContentIndex(0);
	}

	/// TODO: Do we really need to reload a 300-500k file again here?  What extra content
	// are we talking about, and can we set the code up to only load that additional content instead?
	PF_START_STARTUPBAR("Data file");
	DATAFILEMGR.Load(pFile);

#if __PPU
	//	In GTA4, this was called after all the game files had been installed
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
//	This function only does things at the start of the game.
//	Every time it's called after that, it will just return immediately.
	CGenericGameStorage::QueueSlowPS3Scan();
#endif	//	__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
#endif	//	__PPU

	PF_START_STARTUPBAR("Read index mapping files");
	// Has to be done here - after the DATAFILEMGR is initialised, but prior to the streaming modules being registered
	CPathServer::ReadIndexMappingFiles();

	WIN32PC_ONLY(CApp::CheckExit());

	PF_START_STARTUPBAR("Load level instance info");
	CFileLoader::LoadLevelInstanceInfo(pFile);

	WIN32PC_ONLY(CApp::CheckExit());

#if DEBUGINFO_2DFX
	CModelInfo::DebugInfo2dEffect();
#endif //DEBUGINFO_2DFX

	//RAGE	CColAccel::endCache();

	PF_START_STARTUPBAR("Lights and paths");
	CTrafficLights::Init();

	g_distantLights.LoadData();

	CWorldPoints::SortWorld2dEffects();
}

//
// name:		CScene::Update()
// description:	Update CScene class
void CScene::Update()
{
	PF_PUSH_TIMEBAR_BUDGETED("Scene update",15.0f);

	if (CStreaming::IsPlayerPositioned())
	{
        PF_START_TIMEBAR_DETAIL("Portal Update");
        CPortal::Update();

        PF_START_TIMEBAR_DETAIL("CompEntity Update");
        CCompEntity::Update();

		//@@: range CSCENE_UPDATE_UPDATE_LOAD_SCENE {
		PF_START_TIMEBAR_DETAIL("LoadScene Update");
		g_LoadScene.Update();

		PF_START_TIMEBAR_DETAIL("PopCycle Update");
		CPopCycle::Update();
		//@@: } CSCENE_UPDATE_UPDATE_LOAD_SCENE


		PF_PUSH_TIMEBAR_DETAIL("GameWorld Process");
		CGameWorld::Process();
		ropeDataManager::UpdateRopeTextures();
		PF_POP_TIMEBAR_DETAIL();

		// hd requests should be deal with after we call CGameWorld::Process, this is where we add new requests from
		// entities and we'll get a faster turnaround time if we process them the same frame
		PF_START_TIMEBAR_DETAIL("Vehicle UpdateHDRequests");
		CVehicleModelInfo::UpdateHDRequests();

		PF_START_TIMEBAR_DETAIL("Weapon UpdateHDRequests");
		CWeaponModelInfo::UpdateHDRequests();

		PF_PUSH_TIMEBAR_DETAIL("EntityBatch ResetHdEntityVisibility");
		CEntityBatch::ResetHdEntityVisibility();
		PF_POP_TIMEBAR_DETAIL();

		PF_START_TIMEBAR_DETAIL("Paths Update");
		ThePaths.Update();

		PF_START_TIMEBAR_DETAIL("PlayerSwitch Update");
		g_PlayerSwitch.Update();

		PF_START_TIMEBAR("Pathserver Process");
		CPathServer::Process();

		PF_START_TIMEBAR("Cover");
		CCover::Update();
		PF_START_TIMEBAR("Misc");
		CUserDisplay::Process();
		CPickupManager::Update();
		CGlassPaneManager::Update();
#if TILED_LIGHTING_ENABLED
		PF_START_TIMEBAR_DETAIL("Tiled Lighting");
		CTiledLighting::Update();
#endif
		PF_START_TIMEBAR("Deferred Lighting");
		DeferredLighting::Update();
	}

	PF_START_TIMEBAR_DETAIL("Scan");
	CGameScan::Update();

	PF_START_TIMEBAR("Occlusion");
	COcclusion::Update();

	if (CStreaming::IsPlayerPositioned())
	{
		PF_START_TIMEBAR_DETAIL("Named Rendertargets");
		gRenderTargetMgr.Update();

		PF_START_TIMEBAR("References");
		TidyReferences();

		CDoorSystem::Update();
	}

	PF_START_TIMEBAR_DETAIL("Debug shit");
#if __BANK
	CFocusEntityMgr::GetMgr().UpdateDebug();
	g_SceneStreamerDebug.Update();
	gWorldMgr.UpdateDebug();
	CWorldDebugHelper::UpdateDebug();
	CSceneIsolator::Update();
	fwEntity::DebugDraw();
#if USE_OLD_DEBUGWINDOW
	CDebugWindowMgr::GetMgr().Update();
#endif // USE_OLD_DEBUGWINDOW
	g_UiEventMgr.Update(&g_UiGadgetRoot);
	g_UiGadgetRoot.UpdateAll();
	g_SceneCostTracker.Update();
	g_mapLiveEditing.Update();
	fwMapDataDebug::Update();
	CInstancePriority::UpdateDebug();
	CBoundsDebug::Update();
	g_ShaderEdit::InstanceRef().Update();
#if __DEV
	CDVGeomTestInterface::Get().Update(&gVpMan.GetCurrentViewport()->GetGrcViewport());
#if HANGING_CABLE_TEST
	CHangingCableTestInterface::Update();
#endif // HANGING_CABLE_TEST
#endif // __DEV
	CStreamingIteratorManager::Update();
	CDebugTextureViewerInterface::Update();
	LightProbe::Update();
	DebugView::GenerateHistogram();
	CLodDebug::Update();
	CAttributeTweaker::Update();
	g_PickerManager.Update();
	CMapOptimizeHelper::Update();
	CIplCullBoxEditor::UpdateDebug();
#if VOLUME_SUPPORT
    g_VolumeTool.Update();
#endif // VOLUME_SUPPORT
	g_LoadScene.UpdateDebug();
#endif // __BANK

#if __JUNCTION_EDITOR
	CJunctionEditor::Update();
#endif // __JUNCTION_EDITOR

#if __NAVEDITORS
	CNavResAreaEditor::Update();
	CNavDataCoverpointEditor::Update();
#endif // __NAVEDITORS

#if __BANK
	fwArchetypeManager::DebugDraw();
#endif // __BANK

    CVehicleModelInfo::Update();

	PF_POP_TIMEBAR();
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	TidyReferences
// Purpose		:	Goes through the building and dummy pool(little bit every frame)
//					and tidies up the unused references for each entity.
//					We need to go through these 2 pools as these entities don't
//					get created/removed so that the references don't get cleaned up naturally.
// Parameters	:	none
/////////////////////////////////////////////////////////////////////////////////
void CScene::TidyReferences()
{
#if REGREF_VALIDATION
	#define NUM_FRAMES_FOR_BUILDING_POOL (1500)			// Building pool is 44000
	#define NUM_FRAMES_FOR_DUMMY_POOL (500)			// Dummy pool is 15000
	s32 size, batch, i;

	// No we go through some of the buildings
	CBuilding::Pool *BuildingPool = CBuilding::GetPool();
	CBuilding* pBuilding;
	size=BuildingPool->GetSize();
	batch = fwTimer::GetSystemFrameCount() %(NUM_FRAMES_FOR_BUILDING_POOL);
	for(i = batch * size / NUM_FRAMES_FOR_BUILDING_POOL; i < (batch+1) * size / NUM_FRAMES_FOR_BUILDING_POOL; i++)
	{
		pBuilding = BuildingPool->GetSlot(i);
		if(pBuilding)
		{
			pBuilding->RemoveAllInvalidKnownRefs();
		}
	}

	// No we go through some of the dummy's
	CDummyObject::Pool *DummyPool = CDummyObject::GetPool();
	CDummyObject* pDummy;
	size=DummyPool->GetSize();
	batch = fwTimer::GetSystemFrameCount() %(NUM_FRAMES_FOR_DUMMY_POOL);
	for(i = batch * size / NUM_FRAMES_FOR_DUMMY_POOL; i < (batch+1) * size / NUM_FRAMES_FOR_DUMMY_POOL; i++)
	{
		pDummy = DummyPool->GetSlot(i);
		if(pDummy)
		{
			pDummy->RemoveAllInvalidKnownRefs();

		}
	}
#endif // REGREF_VALIDATION
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetLevelName
// Purpose		:	Returns the name of a level
// Parameters	:	Level number
/////////////////////////////////////////////////////////////////////////////////
const char* CScene::GetLevelName(s32 levelNumber)
{
	return m_levelData.GetFilename(levelNumber);
}


//
// name:		CScene::SetupRender()
// description:	Setup for rendering
void CScene::SetupRender(s32, u32 renderFlags, const grcViewport &, CRenderPhaseScanned *renderPhaseNew)
{
	PF_START_TIMEBAR_DETAIL("ConstructRenderList"); // TODO -- do we need this timebar? if not we can remove this function and call CRenderer::ConstructRenderListNew directly

	FastAssert(renderPhaseNew);
	CRenderer::ConstructRenderListNew(renderFlags, renderPhaseNew);
}

void PlantMgrRenderCB(void){
	CPlantMgr::Render();
}

#if DEBUG_DRAW

#if __BANK
void CScene::InitWidgets()
{
	ms_pBank = &BANKMGR.CreateBank("Scene", 0, 0, false); 
	if(Verifyf(ms_pBank, "Failed to create scene bank"))
	{
		ms_pBank->AddButton("Create Scene widgets", datCallback(CFA1(CScene::AddWidgets), ms_pBank));
	}
}

void CScene::AddWidgets()
{
	// destroy first widget which is the create button
	bkWidget* pWidget = BANKMGR.FindWidget("Scene/Create Scene widgets");
	if(pWidget == NULL)
		return;
	pWidget->Destroy();

	// General widgets
	CFocusEntityMgr::GetMgr().InitWidgets(*ms_pBank);
	CMapDataContents::AddBankWidgets(ms_pBank);
	fwEntity::AddWidgets(*ms_pBank);
	fwArchetypeManager::AddWidgets(*ms_pBank);
	CCompEntity::AddWidgets(ms_pBank);
	CGameWorld::AddWidgets(ms_pBank);
	CInteriorGroupManager::AddWidgets(ms_pBank);
	CPostScanDebug::AddWidgets(ms_pBank);
	CSceneIsolator::AddWidgets(ms_pBank);
	gWorldMgr.AddBankWidgets(ms_pBank);
	CWorldDebugHelper::AddBankWidgets(ms_pBank);
	g_SceneCostTracker.AddWidgets(ms_pBank);
	g_mapLiveEditing.InitWidgets(*ms_pBank);
	CLodDebug::AddWidgets(ms_pBank);
	CAttributeTweaker::AddWidgets(ms_pBank);
	g_SceneStreamerDebug.AddWidgets(ms_pBank);
	CMapOptimizeHelper::AddWidgets(*ms_pBank);
	CTexLod::AddWidgets(ms_pBank);
	CIplCullBoxEditor::InitWidgets(*ms_pBank);
	CManagedImapGroupMgr::InitWidgets(*ms_pBank);
	fwMapDataDebug::InitWidgets(*ms_pBank);
	CInstancePriority::InitWidgets(*ms_pBank);
	CBoundsDebug::InitWidgets(*ms_pBank);
	g_LoadScene.InitWidgets(*ms_pBank);
	g_fwMetaDataStore.InitWidgets(*ms_pBank);
	g_HorizonObjects.InitWidgets(ms_pBank);

	// init the REST interface used by the map edit widgets within scene.cpp
	// If other widgets outside this class need it, this'll have to move.
	g_MapEditRESTInterface.Init();
}

void CScene::ShutdownWidgets()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
}

#endif //__BANK

#endif // !__FINAL
