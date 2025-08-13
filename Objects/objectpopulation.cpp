
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    objectpopulation.cpp 
// PURPOSE : Deals with creation and removal of objects
// AUTHOR :  Obbe.
// CREATED : 10/05/06
//
/////////////////////////////////////////////////////////////////////////////////

// rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "vector/vector3.h"
#include "fragment/instance.h"
#include "fwscene/stores/mapdatastore.h"

#if __BANK
#include "string/stringutil.h"
#endif

// game includes
#include "camera/camInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "core/game.h"
#include "Control/replay/Replay.h"
#include "control/trafficlights.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "debug/vectormap.h"
#include "frontend/MobilePhone.h"
#include "objects/objectpopulation.h"
#include "Task/System/Task.h"
#include "Task/General/TaskSecondary.h"
#include "peds/PlayerInfo.h"
#include "network/NetworkInterface.h"
#include "objects/door.h"
#include "objects/object.h"
#include "objects/ObjectIntelligence.h"
#include "objects/dummyobject.h"
#include "pathserver/ExportCollision.h"
#include "peds/Ped.h"
#include "peds/pedintelligence.h"
#include "physics/Breakable.h"
#include "physics/Deformable.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/Tunable.h"
#include "scene/entities/compEntity.h"
#include "scene/FocusEntity.h"
#include "scene/lod/LodMgr.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "timecycle/TimeCycleAsyncQueryMgr.h"
#include "vfx/particles/PtFxManager.h"
#include "renderer/lights/LightEntity.h"
#include "renderer/Entities/ObjectDrawHandler.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/Types/TaskUseScenario.h"

#include "glassPaneSyncing/GlassPaneManager.h"
#include "network/Objects/entities/NetObjGlassPane.h"

//fw headers
#include "fwscene/stores/mapdatastore.h"


#if __STATS
EXT_PF_PAGE(WorldPopulationPage);
PF_GROUP(ObjectPopulation);
PF_LINK(WorldPopulationPage, ObjectPopulation);

PF_TIMER(ManageObjectPopulation, ObjectPopulation);
#endif // __STATS


#define OBJECT_POPULATION_CULL_RANGE					(54.5f)				// objects outside this range will be removed
#define OBJECT_POPULATION_CULL_RANGE_SQR				(OBJECT_POPULATION_CULL_RANGE*OBJECT_POPULATION_CULL_RANGE)
#define OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN			(20.0f)		// objects outside this range will be removed
#define OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN_SQR		(OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN*OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN)


SCENE_OPTIMISATIONS()

atArray<entitySelectID> CObjectPopulation::ms_dummiesThatNeedAmbientScaleUpdate;
atArray<RegdDummyObject> CObjectPopulation::ms_suppressedDummyObjects;
atArray<RegdDummyObject> CObjectPopulation::ms_highPriorityDummyObjects;
bool CObjectPopulation::ms_bAllowPinnedObjects = true;

sCloseDummy CObjectPopulation::ms_closeDummiesToProcess[NUM_CLOSE_DUMMIES_PER_FRAME];

bool CObjectPopulation::ms_bScriptRequiresForceConvert = false;
spdSphere CObjectPopulation::ms_volumeForScriptForceConvert;

u16 CObjectPopulation::ms_numNeverDummies = 0;
u16 CObjectPopulation::ms_neverDummyList[NUM_NEVERDUMMY_PROCESSED];

atVector<u32> CObjectPopulation::ms_blacklistForMP;

#if __BANK
float CObjectPopulation::ms_currentRealConversionsPerFrame = 0.f;
float CObjectPopulation::ms_currentDummyConversionsPerFrame = 0.f;
u32	CObjectPopulation::ms_realConversionsThisFrame = 0;
u32	CObjectPopulation::ms_dummyConversionsThisFrame= 0;
bool CObjectPopulation::ms_objectPopDebugInfo = false;
bool CObjectPopulation::ms_objectPopDebugDrawingVM = false;
bool CObjectPopulation::ms_drawRealObjectsOnVM = false;
bool CObjectPopulation::ms_drawDummyObjectsOnVM = false;
bool CObjectPopulation::ms_showRealToDummyObjectConversionsOnVM = false;
bool CObjectPopulation::ms_showDummyToRealObjectConversionsOnVM = false;

bool CObjectPopulation::ms_showMostExpensiveRealObjectConversions = false;
CObjectPopulation::ObjectConversionStruct* CObjectPopulation::ms_pExpensiveObjectConversions = NULL;
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CObjectPopulation::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
		CDoorSystem::Init();

		CObject::ResetStartTimerForCollisionAgitatedEvent();
    }

	ms_suppressedDummyObjects.Reset();
	ms_dummiesThatNeedAmbientScaleUpdate.Reset(0);
	ms_dummiesThatNeedAmbientScaleUpdate.Reserve(300);

	for (s32 i = 0; i < NUM_CLOSE_DUMMIES_PER_FRAME; ++i)
	{
		ms_closeDummiesToProcess[i].dist = 99999.f;
		ms_closeDummiesToProcess[i].ent = NULL;
	}

#if __BANK
	// Allocate storage for keeping track of most expensive object conversions
	if(!ms_pExpensiveObjectConversions)
	{
		sysMemStartDebug();
		ms_pExpensiveObjectConversions = rage_new CObjectPopulation::ObjectConversionStruct[EXPENSIVE_OBJECT_CONVERSIONS_TO_TRACK];		
		sysMemEndDebug();
	}
#endif

	// initialise the object blacklist
	ms_blacklistForMP.Reset();
	ms_blacklistForMP.reserve(150);
	ms_blacklistForMP.Push(ATSTRINGHASH("ng_proc_temp",0x74f24de));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_brittlebush_01",0x1c725a1));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_desert_sage_01",0x81fb3ff0));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_dry_plants_01",0x79b41171));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_drygrasses01",0x781e451d));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_drygrasses01b",0xa5e3d471));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_drygrassfronds01",0x6a27feb1));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_dryplantsgrass_01",0x861d914d));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_dryplantsgrass_02",0x8c049d17));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_forest_grass_01",0xffba70aa));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_forest_ivy_01",0xe65ec0e4));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_grassdandelion01",0xc3c00861));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_grasses01",0x5f5dd65c));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_grasses01b",0xc07792d4));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_grassfronds01",0x53cfe80a));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_grassplantmix_01",0xd9f4474c));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_grassplantmix_02",0xcb2acc8));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_indian_pbrush_01",0xc6899cde));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_leafybush_01",0xd14b5ba3));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_leafyplant_01",0x32a9996c));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_lizardtail_01",0x69d4f974));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_lupins_01",0xc2e75a21));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_meadowmix_01",0x1075651));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_meadowpoppy_01",0xe1aeb708));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_sage_01",0xcbbb6c39));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_scrub_bush01",0x6fa03a5e));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_sml_reeds_01",0xcf7a9a9d));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_sml_reeds_01b",0x34315488));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_sml_reeds_01c",0x45ef7804));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_stones_01",0xac3de147));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_stones_02",0xcafc1ec3));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_stones_03",0xd971bbae));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_stones_04",0xe764d794));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_stones_05",0xf51f7309));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_stones_06",0x1e78c9d));
	ms_blacklistForMP.Push(ATSTRINGHASH("proc_wildquinine",0xa49658fc));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_dandy_b",0x4f2526da));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_dryweed_001_a",0x576ab4f6));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_dryweed_002_a",0xd20b1778));

	ms_blacklistForMP.Push(ATSTRINGHASH("prop_fern_01",0x54bc1cd8));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_fernba",0xce109c5c));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_fernbb",0xe049c0ce));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_flowerweed_005_a",0x78de93d1));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_grass_001_a",0xe5b89d31));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_grass_ca",0x5850e7b3));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_grass_da",0x6aed0e4b));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_log_aa",0xc50a4285));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_log_ab",0xb648a502));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_log_ac",0x5e497511));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_log_ad",0x47c14801));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_log_ae",0xfd8bb397));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_log_af",0xef541728));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_saplin_001_b",0xc2cc99d8));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_saplin_001_c",0x8fb233a4));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_saplin_002_b",0x24e08e1f));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_saplin_002_c",0x337b2b54));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_small_bushyba",0x7367d224));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_tall_drygrass_aa",0x919d9255));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_thindesertfiller_aa",0x4484243f));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_weed_001_aa",0x3c91d42d));
	ms_blacklistForMP.Push(ATSTRINGHASH("prop_weed_002_ba",0x3353525a));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbandryfrnds_01",0xc175f658));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbandrygrass_01",0x762657c6));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbangrnfrnds_01",0x94ac15b3));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbangrngrass_01",0x28014a56));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbanweeds01",0xe0a8bfc9));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbanweeds01_l1",0x3a559c31));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbanweeds02",0x5fc8a70));
	ms_blacklistForMP.Push(ATSTRINGHASH("urbanweeds02_l2",0x3b545487));
	ms_blacklistForMP.Push(ATSTRINGHASH("v_proc2_temp",0xb9402f87));

	ms_blacklistForMP.Push(ATSTRINGHASH("barracks",0xceea3f4b));
	ms_blacklistForMP.Push(ATSTRINGHASH("barracks3",0x2592b5cf));
	ms_blacklistForMP.Push(ATSTRINGHASH("dune",0x9cf21e0f));
	ms_blacklistForMP.Push(ATSTRINGHASH("marquis",0xc1ce1183));
	ms_blacklistForMP.Push(ATSTRINGHASH("marshall",0x49863e9c));
	ms_blacklistForMP.Push(ATSTRINGHASH("monster",0xcd93a7db));
	ms_blacklistForMP.Push(ATSTRINGHASH("tug",0x82cac433));

	std::sort(ms_blacklistForMP.begin(), ms_blacklistForMP.end());
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shutdown
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CObjectPopulation::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
		CDoorSystem::Shutdown();
	    // purge all the objects in the pool
	    CObject::Pool* pObjectPool = CObject::GetPool();
	    s32 			i = (s32) pObjectPool->GetSize();
	    CObject*		pObject = NULL;

	    while(i--)
	    {
		    pObject = pObjectPool->GetSlot(i);
		    if(pObject)
		    {
			    ConvertToDummyObject(pObject, true);
		    }
	    }
    }
	else if(shutdownMode == SHUTDOWN_CORE)
	{
#if __BANK
		if(ms_pExpensiveObjectConversions)
		{
			// clear up our debug memory
			sysMemStartDebug();		
			delete[] ms_pExpensiveObjectConversions;
			ms_pExpensiveObjectConversions = NULL;
			sysMemEndDebug();		
		}
#endif
	}

}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Process
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CObjectPopulation::Process()
{
	ManageObjectPopulation(false);
}


//
// name:		CreateObject
// description:	Create a new Object
//				Could be a CObject or one of the classes derived from CObject (eg CDoor)
//				If there is no room in CObject pool deletes temporary objects before
//				to make space for the object
// Parameters:	nModelIndex
//				bCreateDrawable
//				bClone - if true the object will be cloned on other machines if a network game is running
//				bPersistantOwner - if this is true the object is to remain on the server and not migrate (used for script created objects)
CObject* CObjectPopulation::CreateObject(fwModelId modelId, const eEntityOwnedBy ownedBy, bool bCreateDrawable, bool bInitPhys, bool bClone, s32 PoolIndex, bool bCalcAmbientScales)
{
	//Create the input.
	CreateObjectInput input(modelId, ownedBy, bCreateDrawable);
	input.m_bInitPhys = bInitPhys;
	input.m_bClone = bClone;
	input.m_iPoolIndex = PoolIndex;
	input.m_bCalcAmbientScales = bCalcAmbientScales;
	
	//Create the object.
	return CreateObject(input);
}

CObject* CObjectPopulation::CreateObject(const CreateObjectInput& rInput)
{
#if !__FINAL
	CObject::bInObjectCreate = true;
#endif

	fwModelId modelId = rInput.m_iModelId;
	eEntityOwnedBy ownedBy = rInput.m_nEntityOwnedBy;
	bool bCreateDrawable = rInput.m_bCreateDrawable;
	bool bInitPhys = rInput.m_bInitPhys;
	bool bClone = rInput.m_bClone;
	bool bForceClone = rInput.m_bForceClone;
	bool bCalcAmbientScales = rInput.m_bCalcAmbientScales;

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);

	bool createDoor = !rInput.m_bForceObjectType && pModelInfo && pModelInfo->GetUsesDoorPhysics() && ownedBy != ENTITY_OWNEDBY_FRAGMENT_CACHE && ownedBy != ENTITY_OWNEDBY_CUTSCENE ;

	s32 nModelIndex = modelId.GetModelIndex();

	CObject* pObject = NULL;
	// CObject::GetPool()->SetCanDealWithNoMemory(true);

	if (createDoor)
		pObject = rage_checked_pool_new(CDoor) (ownedBy, nModelIndex, bCreateDrawable, bInitPhys);
	else
		pObject = rage_checked_pool_new(CObject) (ownedBy, nModelIndex, bCreateDrawable, bInitPhys, bCalcAmbientScales);
	// CObject::GetPool()->SetCanDealWithNoMemory(false);

	if(pObject == NULL)
	{
		TryToFreeUpTempObjects(5);
//RAGE		g_waterCreatureMan.TryToFreeUpWaterCreatures(5);

		if (createDoor)
			pObject = rage_checked_pool_new(CDoor) (ownedBy, nModelIndex, bCreateDrawable, bInitPhys);
		else
			pObject = rage_checked_pool_new(CObject) (ownedBy, nModelIndex, bCreateDrawable, bInitPhys, bCalcAmbientScales);

#if !__FINAL && !__GAMETOOL
		if(!pObject)
		{
			Errorf("Was unable to allocate a new CObject.\n");
			// Need to manually call it since the allocation function won't fail and trigger the callback function with rage_checked_pool_new.
			CObject::DumpObjectPool();
		}
#endif
	}

	// ******* NETWORK REGISTRATION HAS TO BE DONE BEFORE ANY FURTHER STATE CHANGES ON THE OBJECT! *******************

	if (pObject && bClone && NetworkInterface::IsGameInProgress())
	{
		if (bForceClone || CNetObjObject::HasToBeCloned(pObject))
		{
			bool bScriptObject = (ownedBy == ENTITY_OWNEDBY_SCRIPT);
			NetObjFlags globalFlags = bScriptObject ? CNetObjGame::GLOBALFLAG_SCRIPTOBJECT : 0;

			Assert(!(createDoor && bScriptObject));

			NetworkInterface::RegisterObject(pObject, 0, globalFlags, bScriptObject);
		}
	}

	if(pObject)
	{
		if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pModelInfo->GetModelNameHash()))
		{
			float tuningHelath = pTuning->GetMaxHealth();
			if(tuningHelath != -1.0f && Verifyf(tuningHelath > 0, "Cannot set a max health of %f on '%s'. Check 'build/dev/common/data/tunableobjects.meta'",tuningHelath,pObject->GetModelName()))
			{
				pObject->SetMaxHealth(tuningHelath);
				pObject->SetHealth(tuningHelath);
			}
		}

#if DEFORMABLE_OBJECTS
		CDeformableObjectEntry* pDeformableData = NULL;
		if(CDeformableObjectManager::GetInstance().IsModelDeformable(pModelInfo->GetModelNameHash(), &pDeformableData))
		{
			pObject->SetDeformableData(pDeformableData);
		}
#endif // DEFORMABLE_OBJECTS
	}
	
#if !__FINAL
	CObject::bInObjectCreate = false;
#endif

#if GTA_REPLAY
	if(CReplayMgr::ShouldForceHideEntity(pObject))
	{
		// Use the script module here....this shouldn't be used at all during replay
		// and using the Replay module would be vulnerable to being overridden
		pObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT,false,false);
	}
#endif // GTA_REPLAY

	return pObject;
}


//
// name:		CreateObject
// description:	Create a new Object from the given dummy object
//				Could be a CObject or one of the classes derived from CObject (eg CDoor)
//				If there is no room in CObject pool deletes temporary objects before
//				to make space for the object
CObject* CObjectPopulation::CreateObject(CDummyObject* pDummy, const eEntityOwnedBy ownedBy)
{
#if !__FINAL
	CObject::bInObjectCreate = true;
#endif

#if NAVMESH_EXPORT
	const bool bExportingCollision = CNavMeshDataExporter::WillExportCollision();
#else
	const bool bExportingCollision = false;
#endif

	CBaseModelInfo* pModelInfo = pDummy->GetBaseModelInfo();
	Assert(pModelInfo);

	CObject* pObject = NULL;
	// CObject::GetPool()->SetCanDealWithNoMemory(true);
	if (pModelInfo->GetUsesDoorPhysics())
		pObject = rage_checked_pool_new(CDoor) (ownedBy, pDummy);
	else
		pObject = rage_checked_pool_new(CObject) (ownedBy, pDummy);
	// CObject::GetPool()->SetCanDealWithNoMemory(false);

	if(pObject == NULL)
	{
		TryToFreeUpTempObjects(5);
//RAGE		g_waterCreatureMan.TryToFreeUpWaterCreatures(5);
		if (pModelInfo->GetUsesDoorPhysics())
			pObject = rage_checked_pool_new(CDoor) (ownedBy, pDummy);
		else
			pObject = rage_checked_pool_new(CObject) (ownedBy, pDummy);

#if !__FINAL && !__GAMETOOL
		if(!pObject)
		{
			Errorf("Was unable to allocate a new CObject.\n");
			// Need to manually call it since the allocation function won't fail and trigger the callback function with rage_checked_pool_new.
			CObject::DumpObjectPool();
		}
#endif
	}

#if DEFORMABLE_OBJECTS
	if(!bExportingCollision)
	{
		if(pObject)
		{
			CDeformableObjectEntry* pDeformableData = NULL;
			if(CDeformableObjectManager::GetInstance().IsModelDeformable(pModelInfo->GetModelNameHash(), &pDeformableData))
			{
				pObject->SetDeformableData(pDeformableData);
			}
		}
	}
#endif // DEFORMABLE_OBJECTS

	if (pObject)
	{
		pObject->SetLodDistance(pDummy->GetLodDistance());	// important to call thsi before create drawable!
		pObject->SetAlpha(pDummy->GetAlpha());
		pObject->GetLodData().SetResetDisabled(true);	// don't want to reset alpha to 0 on drawable creation
		pObject->m_nFlags.bNeverDummy = pDummy->m_nFlags.bNeverDummy;
		if (pDummy->IsBaseFlagSet(fwEntity::IS_FIXED))
		{
			pObject->SetBaseFlag(fwEntity::IS_FIXED);
		}

#if __BANK
		pObject->SetDebugPriority(pDummy->GetDebugPriority(), pDummy->GetDebugIsFixed());
#endif	//__BANK

		pObject->GetRenderPhaseVisibilityMask() = pDummy->GetRenderPhaseVisibilityMask();

		pObject->SetUseAmbientScale(pDummy->GetUseAmbientScale());
		pObject->SetUseDynamicAmbientScale(pDummy->GetUseDynamicAmbientScale());
		pObject->SetNaturalAmbientScale(pDummy->GetNaturalAmbientScale());
		pObject->SetArtificialAmbientScale(pDummy->GetArtificialAmbientScale());
		pObject->SetSetupAmbientScaleFlag(false);

		pObject->SetCalculateAmbientScaleFlag(false); // disable calculating the ambient scale inside of CreateDrawable, as we just passed it in
		{
			pObject->CreateDrawable();
		}
		pObject->SetCalculateAmbientScaleFlag(true); // enable calculating the dynamic ambient scale as needed
	}
#if !__FINAL
	CObject::bInObjectCreate = false;
#endif

#if GTA_REPLAY
	if(CReplayMgr::ShouldForceHideEntity(pObject))
	{
		// Use the script module here....this shouldn't be used at all during replay
		// and using the Replay module would be vulnerable to being overridden
		pObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT,false,false);
	}
#endif // GTA_REPLAY

	return pObject;
}

// name:		Destroy
// description:	

void CObjectPopulation::DestroyObject(CObject* pObject, bool REPLAY_ONLY(convertedToDummy))
{
	// Needed to catch distance teleports or leaving MP where objects aren't converted back to dummys....
	if(pObject)
	{
		CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
		Assert(pModelInfo);

		if(pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0)
		{
			CGlassPaneManager::UnregisterGlassGeometryObject(pObject);
		}
	}

#if !__FINAL
	// Track objects being double-destroyed for debugging purposes
	static CObject* pDestroyingObject = 0;
	if(!pDestroyingObject)
	{
		pDestroyingObject = pObject;
	}
	else if(pObject == pDestroyingObject)
	{
		// NB: Quitf to cause a crash dump
		Quitf("Object %p being double-destroyed! See B* 1854346", pObject);
	}
	++CObject::inObjectDestroyCount;
#endif

	CTheScripts::UnregisterEntity(pObject, false);

	if (pObject->GetNetworkObject())
	{
		bool bClone = pObject->IsNetworkClone();
		bool bScriptObject = pObject->GetNetworkObject()->IsScriptObject();
		bool bAmbientObject = CNetObjObject::IsAmbientObjectType(pObject->GetOwnedBy());
		bool bUnregister = true;
		bool bFail = false;

		if (pObject->IsADoor())
		{
			// script doors network objects persist when the physical door streams out
			if (bScriptObject && static_cast<CNetObjDoor*>(pObject->GetNetworkObject())->GetDoorSystemData())
			{
				bUnregister = false;
			}
		}
		else if (bAmbientObject && !pObject->m_nObjectFlags.bIsPickUp && !pObject->GetIsAttached())
		{
			// the network objects for ambient objects persist and will try to migrate themselves if local
			if (CNetObjObject::UnassignAmbientObject(*pObject))
			{
				bUnregister = false;
			}
		}
		else if (bClone)
		{
			Assertf(0, "Trying to destroy clone %s! Owned by: %d", pObject->GetNetworkObject()->GetLogName(), pObject->GetOwnedBy());

			bFail = true;
		} 
		else if (pObject->GetNetworkObject()->IsPendingOwnerChange())
		{
			Assertf(0, "Trying to destroy %s while it is migrating", pObject->GetNetworkObject()->GetLogName());
			bFail = true;
		}

		if (bFail)
		{
#if !__FINAL
			if(pDestroyingObject == pObject)
			{
				pDestroyingObject = 0;
			}
			--CObject::inObjectDestroyCount;
#endif
			return;
		}

		if (bUnregister)
		{
			static_cast<CNetObjObject*>(pObject->GetNetworkObject())->SetGameObjectBeingDestroyed();
			NetworkInterface::UnregisterObject(pObject);
		}
	}

	if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_COMPENTITY)
	{
		Assertf(CCompEntity::IsAllowedToDelete(),"Destroying compEntity owned object, when deletion is not allowed...");
	}

	// We don't delete frag cache objects since these were created by the physics simulator... call remove on the physInst instead
	if(pObject->GetCurrentPhysicsInst() && pObject->GetCurrentPhysicsInst()->GetClassType() == PH_INST_FRAG_CACHE_OBJECT)
	{
		((fragInst *)pObject->GetCurrentPhysicsInst())->Remove();
	}
	else
	{
		// Need to clear any object tasks here. Otherwise, tasks like synchronised scene might try and reinsert the object
		// into the physics level after it gets removed in CGameWorld::Remove().
		if(pObject->GetObjectIntelligence())
			pObject->GetObjectIntelligence()->ClearTasks();
		CGameWorld::Remove(pObject);

		Assert(!pObject->GetIsRetainedByInteriorProxy());
		Assertf(!pObject->GetOwnerEntityContainer(),
			"Object %s was removed from world, but still has an owner entity container! bInMloRoom:%d, validInterior:%d",
			pObject->GetModelName(),
			pObject->m_nFlags.bInMloRoom,
			pObject->GetInteriorLocation().IsValid()
		);

#if GTA_REPLAY
		if(convertedToDummy)
		{
			ReplayObjectExtension* pExtension = ReplayObjectExtension::GetExtension(pObject);
			if(pExtension)
			{
				pExtension->SetDeletedAsConvertedToDummy();
			}
		}

		CReplayMgr::OnDelete(pObject);
#endif // GTA_REPLAY

		delete pObject;
	}
#if !__FINAL
	if(pDestroyingObject == pObject)
	{
		pDestroyingObject = 0;
	}
	--CObject::inObjectDestroyCount;
#endif
}


//
// name:		TryToFreeUpTempObjects
// description:	Try to free up space in Object Pool by removing temp
//				objects offscreen
void CObjectPopulation::TryToFreeUpTempObjects(s32 num)
{
	CObject *pObj = NULL;
	CObject::Pool* pObjPool = CObject::GetPool();
	s32 i = (s32) pObjPool->GetSize();

	while(i-- && num > 0)
	{
		pObj = pObjPool->GetSlot(i);

		if(pObj)
		{			
			if(pObj->GetOwnedBy() == ENTITY_OWNEDBY_TEMP && !pObj->IsVisible())
			{
				CObjectPopulation::DestroyObject(pObj);
				num--;
			}
		}
	}
}

//
//
//
//
void CObjectPopulation::DeleteAllTempObjects()
{
	CObject *pObj = NULL;
	CObject::Pool* pObjPool = CObject::GetPool();
	s32 nPoolSize = (s32) pObjPool->GetSize();
	for(s32 i=0; i<nPoolSize; i++)
	{
		pObj = pObjPool->GetSlot(i);

		if(pObj)
		{	
			bool bNetAllowDelete = true;

			if(pObj->GetNetworkObject())
			{
				CNetObjObject *pNetObj =(CNetObjObject*)pObj->GetNetworkObject();
				bNetAllowDelete = pNetObj->CanDelete();
				if(!bNetAllowDelete)
				{
					pNetObj->FlagForDeletion();
				}
			}

			if(pObj->GetOwnedBy() == ENTITY_OWNEDBY_TEMP && !pObj->IsNetworkClone() && bNetAllowDelete)
			{
				CObjectPopulation::DestroyObject(pObj);
			}
		}
	}

}//end of CObject::DeleteAllTempObjects()...




//
//
//
//
void CObjectPopulation::DeleteAllMissionObjects()
{
	CObject *pObj = NULL;
	CObject::Pool* pObjPool = CObject::GetPool();
	s32 nPoolSize = (s32) pObjPool->GetSize();
	for(s32 i=0; i<nPoolSize; i++)
	{
		pObj = pObjPool->GetSlot(i);

		if(pObj)
		{			
			if (pObj->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)	//	|| (pObj->GetOwnedBy() == MISSION_BRAIN_OBJECT) )
			{
				CObjectPopulation::DestroyObject(pObj);
			}
		}
	}
}



//
//
//
//
void CObjectPopulation::DeleteAllTempObjectsInArea(const Vector3& Centre, float Radius)
{
	CObject *pObj = NULL;
	CObject::Pool* pObjPool = CObject::GetPool();
	s32 nPoolSize = (s32) pObjPool->GetSize();

	for(s32 i=0; i<nPoolSize; i++)
	{
		pObj = pObjPool->GetSlot(i);

		if (pObj && (pObj->GetOwnedBy() == ENTITY_OWNEDBY_TEMP || pObj->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE))
		{	
			bool bScriptObject = pObj->GetNetworkObject() && pObj->GetNetworkObject()->IsScriptObject();

			if (!bScriptObject)
			{
#if __ASSERT
				if (pObj->GetIsAnyManagedProcFlagSet())
				{
					Assertf(0, "Trying to delete a procedural object that is now a ENTITY_OWNEDBY_TEMP");
				}
				else
#endif
				{
					const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
					Vector3 diff = vObjectPosition - Centre;

					if (diff.Mag2() < Radius*Radius)
					{
						if( !NetworkUtils::IsNetworkCloneOrMigrating(pObj))
						{
							CObjectPopulation::DestroyObject(pObj);
						}
						else if (pObj->GetNetworkObject())
						{
							// Flag the object so it gets deleted later in whatever machine getting its ownership
							static_cast<CNetObjObject*>(pObj->GetNetworkObject())->FlagForDeletion();
						}
					}
				}
			}
		}
	}
}


//
// name:		UpdateAllDummysAndObjects
// description:	Convert all the nearby dummys to objects and all the faraway objects to dummys
//

#define FRAMESTOCOMPLETEMANAGEOBJECTPOPULATION (8)			// Spread the update for objects out over this many frames (^2)
#define FRAMESTOCOMPLETEMANAGEDUMMYPOPULATION (32)			// Spread the update for dummies out over this many frames (^2)

void CObjectPopulation::ManageObjectPopulation(bool bAllObjects, bool cleanFrags)
{
	PF_FUNC(ManageObjectPopulation);

#if !__FINAL
	if (fwTimer::IsGamePaused() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		return;
	}
#endif

#if NAVMESH_EXPORT
	// If exporting collision data we want to convert all the objects asap
	if(CNavMeshDataExporter::IsExportingCollision())
		bAllObjects = true;
#endif

	Vector3 vecCentre = CFocusEntityMgr::GetMgr().GetPos();

	bool bScreenFadedOut = camInterface::IsFadedOut();

#if !__GAMETOOL
	// sniper rifle promotes dummy objects to real objects so they can be shot. the phone cam doesn't.
	bool bIsFirstPersonAiming = ( (camInterface::GetGameplayDirector().IsFirstPersonAiming() && !CPhoneMgr::CamGetState()) REPLAY_ONLY(|| CReplayMgr::FakeSnipingMode()));
#else
	bool bIsFirstPersonAiming = false;
#endif

	CObject::Pool *objectPool = CObject::GetPool();
	s32 PoolSize = (s32) objectPool->GetSize();

	static u32 currentObject = 0;
	s32 numObjectsThisFrame = (s32) objectPool->GetNoOfUsedSpaces() / FRAMESTOCOMPLETEMANAGEOBJECTPOPULATION;

	if (currentObject >= PoolSize)
		currentObject = 0;

	if (bAllObjects)
	{
		currentObject = 0;
		numObjectsThisFrame = PoolSize;
	}

	PF_PUSH_TIMEBAR("Manage Real Objects");
	for (s32 i = currentObject; (i < PoolSize) && (numObjectsThisFrame > 0); i++, currentObject++)
	{
		static CObject* pObject;			// Obbe:Made this a static so we can see what's going on when crashing deeper in the code.
		pObject = objectPool->GetSlot(i);
		if (pObject)
		{
			if(pObject->isRageEnvCloth() && pObject->GetObjectAudioEntity() && !pObject->GetObjectAudioEntity()->HasRegisteredWithController())
			{
				pObject->GetObjectAudioEntity()->Init(pObject);
			}
			if (ms_bAllowPinnedObjects && pObject->m_nFlags.bNeverDummy)
				continue;
			fwEntity *attachParent = pObject->GetAttachParent();
			// don't delete any object a player is holding
			if(attachParent && ((CPhysical *) attachParent)->GetIsTypePed() && ((CPed*)attachParent)->IsPlayer())
				continue;

			// don't delete any procedural objects (the procobj code must be in total control of these)
			if (pObject->GetIsAnyManagedProcFlagSet())
			{
				continue;
			}

			ManageObject(pObject, vecCentre, bScreenFadedOut, cleanFrags, bIsFirstPersonAiming);
			numObjectsThisFrame--;

			if(!objectPool->GetIsFree(i))
			{
				pObject->m_nFlags.bFoundInPvs = false;
			}
		}
	}
	PF_POP_TIMEBAR();

#if __BANK
	// don't convert any dummys to real objects
	if(CPhysics::ms_bForceDummyObjects)
		return;
#endif

	PF_PUSH_TIMEBAR("Manage Dummy Objects");
	// Go through the dummy pool
	s32 numHighPrio = ManageHighPriorityDummyObjects(vecCentre, bIsFirstPersonAiming);

	CDummyObject::Pool *dummyPool = CDummyObject::GetPool();
	PoolSize = dummyPool->GetSize();

	static u32 currentDummy = 0;
	s32 numDummiesThisFrame = (dummyPool->GetNoOfUsedSpaces() / FRAMESTOCOMPLETEMANAGEDUMMYPOPULATION) - numHighPrio;

	if (bAllObjects)
	{
		currentDummy = 0;
		numDummiesThisFrame = PoolSize;
	}

	if (numDummiesThisFrame > 0)
	{
		if (currentDummy >= PoolSize)
			currentDummy = 0;

		for (s32 i = currentDummy; (i < PoolSize) && (numDummiesThisFrame > 0); i++, currentDummy++)
		{
			CDummyObject* pDummy = dummyPool->GetSlot(i);
			if(pDummy)
			{
				ManageDummy(pDummy, vecCentre, bIsFirstPersonAiming);
				numDummiesThisFrame--;
				pDummy->m_nFlags.bFoundInPvs = false;
			}
		}
	}

	UpdateSuppressedDummyObjects();
	PF_POP_TIMEBAR();

	ForceConvertForScript();

	ForceConvertDummyObjectsMarkedAsNeverDummy();
}

//
// name:		ManageObject
// description:	Code to manage a single object
//

void CObjectPopulation::ManageObject(CObject* pObj, const Vector3& vecCentre, bool bScreenFadedOut, bool cleanFrags, bool bIsFirstPersonAiming)
{
	if(pObj->CanBeDeleted() && !pObj->RecentlySniped())
	{
#if __BANK
		if(CPhysics::ms_bForceDummyObjects)
		{
			if(pObj->GetOwnedBy() == ENTITY_OWNEDBY_TEMP || pObj->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
				DestroyObject(pObj);
			else
				ConvertToDummyObject(pObj);
			return;
		}
#endif

		if(pObj->GetOwnedBy() == ENTITY_OWNEDBY_TEMP)
		{
			// only clean up temp objects when they are off screen or out of drawing range
			bool bCanBeRemoved = ( !g_LodMgr.WithinVisibleRange(pObj) || ( !pObj->m_nObjectFlags.bOnlyCleanUpWhenOutOfRange && !pObj->GetIsVisibleInSomeViewportThisFrame() ) );

			// in MP we only want to remove temp objects when they are far away and not visible to other players
			if (!pObj->GetNetworkObject() && fwTimer::GetTimeInMilliseconds() > pObj->m_nEndOfLifeTimer + 30000)
			{
				// exception - some objects are permitted to be cleaned up in full view, if they fade down first
				if ( pObj->m_nObjectFlags.bSkipTempObjectRemovalChecks && !bCanBeRemoved)
				{
					// fade down to zero before removal
					pObj->m_nObjectFlags.bFadeOut = true;
					pObj->GetLodData().SetFadeToZero(true);

					bCanBeRemoved = (pObj->GetAlpha()==LODTYPES_ALPHA_INVISIBLE);
				}
				
				if ( bCanBeRemoved )
				{
					DestroyObject(pObj);
				}
			}
			//Check if the object has faded out.
			else if(pObj->m_nObjectFlags.bFadeOut && (pObj->GetAlpha() == 0))
			{
				DestroyObject(pObj);
			}
			else
			{
				if (!bScreenFadedOut && pObj->m_nObjectFlags.bOnlyCleanUpWhenOutOfRange && !bCanBeRemoved)
				{
					return;
				}
				if (!IsObjectRequiredBySniper(pObj, bIsFirstPersonAiming))
				{
					// Found an object - find out how far it is from the camera
					const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
					Vector3 vecTargetObject = vObjectPosition - vecCentre;
					float fDistanceSqr = vecTargetObject.Mag2();

					if(fDistanceSqr > OBJECT_POPULATION_CULL_RANGE_SQR)
					{
						// this object is still near to other players so it must have its control passed to the nearest
						if (NetworkInterface::IsGameInProgress() && pObj->GetNetworkObject() && !pObj->IsNetworkClone() 
							&& !((CNetObjObject*)pObj->GetNetworkObject())->TryToPassControlOutOfScope())
						{
							return;
						}
						DestroyObject(pObj);
					}
					else if (fDistanceSqr > OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN_SQR && (bScreenFadedOut || !pObj->GetIsVisibleInSomeViewportThisFrame()))
					{
						// can't remove the object if it is visible or close to another player
						if (NetworkInterface::IsGameInProgress()
							&& NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vObjectPosition, pObj->GetBoundRadius(),
										OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN, OBJECT_POPULATION_CULL_RANGE))
						{
							return;
						}
						DestroyObject(pObj);
					}
				}
			}
		}
		else if(pObj->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
		{
			if(pObj->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) || cleanFrags)
			{
				DestroyObject(pObj);
			}
			else
			{
				// Found an object - find out how far it is from the camera
				const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
				Vector3 vecTargetObject = vObjectPosition - vecCentre;
				float fDistanceSqr = vecTargetObject.Mag2();

				int nNumActive = CBreakable::GetNumFragCacheObjects();
				u32 nOnScreenLife = 300000;
				u32 nOffScreenLife = 150000;
				if(nNumActive > 30)
				{
					nOnScreenLife = 60000;
					nOffScreenLife = 20000;
				}
				else if(nNumActive > 10)
				{
					nOnScreenLife = 100000;
					nOffScreenLife = 50000;
				}

  				CBaseModelInfo* pModelInfo = pObj->GetBaseModelInfo();
				if(REPLAY_ONLY(!CReplayMgr::ShouldIncreaseFidelity() &&) (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT) || pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_STREET_LIGHT)))
  				{
  					nOnScreenLife = 10000;
  					nOffScreenLife = 5000;
  				}

				if( fDistanceSqr > OBJECT_POPULATION_RESET_RANGE_SQR )
				{
					// don't magically repair objects while in full view
					if (pObj->m_nFlags.bRenderDamaged && g_LodMgr.WithinVisibleRange(pObj) && pObj->GetIsVisibleInSomeViewportThisFrame())
					{
						return;
					}
					DestroyObject(pObj);
				}
				else if ((fwTimer::GetTimeInMilliseconds() > pObj->m_nEndOfLifeTimer + nOffScreenLife && (bScreenFadedOut || !pObj->GetIsVisibleInSomeViewportThisFrame())) ||
						(fwTimer::GetTimeInMilliseconds() > pObj->m_nEndOfLifeTimer + nOnScreenLife && fDistanceSqr > OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN_SQR))
				{
					// can't remove the object if it is visible or close to another player
					if (NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(vObjectPosition, pObj->GetBoundRadius(), OBJECT_POPULATION_CULL_RANGE_OFF_SCREEN, OBJECT_POPULATION_CULL_RANGE))
					{
						return;
					}
					DestroyObject(pObj);
				}
			}
		}
		else 
		{
#if NAVMESH_EXPORT
			if(CNavMeshDataExporter::IsExportingCollision())
			{
				ManageObjectForNavMeshExport(pObj);
				return;
			}
#endif

			if (pObj->m_nFlags.bFoundInPvs)
				return;

			// don't magically repair objects while in full view
			if (pObj->m_nFlags.bRenderDamaged && g_LodMgr.WithinVisibleRange(pObj) && pObj->GetIsVisibleInSomeViewportThisFrame())
				return;

			// Found an object - find out how far it is from the camera
			const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
			Vector3 vecTargetObject = vObjectPosition - vecCentre;
			float fDistanceSqr = vecTargetObject.Mag2();

			float fResetDistSqr = 100000.0f;
			if(pObj->GetRelatedDummy())
			{
				Vector3 vecResetDist = VEC3V_TO_VECTOR3(pObj->GetRelatedDummy()->GetTransform().GetPosition()) - vecCentre;
				fResetDistSqr = vecResetDist.Mag2();
			}
		
			if (  (fDistanceSqr > OBJECT_POPULATION_RESET_RANGE_SQR) &&
				  (fResetDistSqr > OBJECT_POPULATION_RESET_RANGE_SQR) &&
				  !IsObjectRequiredBySniper(pObj, bIsFirstPersonAiming) &&
				  !IsObjectRequiredByScript(pObj) )
			{
				ConvertToDummyObject(pObj);
			}
		}	
	}
}

//
// name:		ManageDummy
// description:	Code to manage a single dummy
//

void CObjectPopulation::ManageDummy(CDummyObject* pDummyObject, const Vector3& vecCentre, bool bIsFirstPersonAiming)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
	{
		ManageDummyForNavMeshExport(pDummyObject);
		return;
	}
#endif

	// is in current area and has been added to the world
	if (pDummyObject->GetIsVisible() && pDummyObject->GetIsAddedToWorld())
	{	
		// found a dummy object - find out how far it is from the camera
		Vector3 vecTargetObject = VEC3V_TO_VECTOR3(pDummyObject->GetTransform().GetPosition()) - vecCentre;
		float fDistanceSqr = vecTargetObject.Mag2();
	
		if (fDistanceSqr<OBJECT_POPULATION_RESET_RANGE_SQR || IsObjectRequiredBySniper(pDummyObject, bIsFirstPersonAiming)
			|| (ms_bAllowPinnedObjects && pDummyObject->m_nFlags.bNeverDummy) )
		{
			//////////////////////////////////////////////////////////////////////////
			// only convert dummy objects to real objects if their assets are fully loaded
			// exceptions:
			// - objects which aren't in a lod hierarchy
			// - never-dummy objects which don't have an auto-anim
			{
				CBaseModelInfo* pModelInfo = pDummyObject->GetBaseModelInfo();
				const bool bNeverDummyWithoutAutoAnim = ( pDummyObject->m_nFlags.bNeverDummy && pModelInfo && !pModelInfo->GetAutoStartAnim() );
				if ( !pDummyObject->GetLodData().HasLod() || (pModelInfo && pModelInfo->GetHasLoaded()) || bNeverDummyWithoutAutoAnim )
				{
					ConvertToRealObject(pDummyObject);
				}
			}
			//////////////////////////////////////////////////////////////////////////
		}
	}
}

//
// name:		IsObjectRequiredBySniper
// description:	returns true if the specified entity (either a dummy or an object) is
//				required by the player using a sniper rifle or similar weapon. this is
//				used to drive dummy-object conversion above and beyond the usual spherical tests
//
bool CObjectPopulation::IsObjectRequiredBySniper(CEntity* pEntity, bool bIsFirstPersonAiming)
{
	// MP keeps more objects alive than SP, so don't allow sniper rifle to promote
	// objects except for those with lights attached
// 	if (NetworkInterface::IsGameInProgress() && !pEntity->m_nFlags.bLightObject)
// 		return false;
	
	bool bRequired = false;
	if (bIsFirstPersonAiming && g_LodMgr.WithinVisibleRange(pEntity)
		/*&& camInterface::GetFov()<25.0f*/)
	{
		Vector3 vCentre;
		float fRadius = pEntity->GetBoundCentreAndRadius(vCentre);
		bRequired = camInterface::IsSphereVisibleInGameViewport(vCentre, fRadius);
	}
	return bRequired;
}

//
// name:		ManageObjectForNavMeshExport
// description:	When exporting navmeshes we need to check against navmesh bounds for object creation/destruction
//
#if NAVMESH_EXPORT
void
CObjectPopulation::ManageObjectForNavMeshExport(CObject* pObj)
{
	// If model is not loaded
	if(!CModelInfo::HaveAssetsLoaded(pObj->GetBaseModelInfo()))
	{
		CModelInfo::RequestAssets(pObj->GetBaseModelInfo(), STRFLAG_FORCE_LOAD);
	}
	else
	{
		if(!CNavMeshDataExporter::EntityIntersectsCurrentNavMesh(pObj))
		{
			ConvertToDummyObject(pObj, false);
		}
	}
}

//
// name:		ManageDummyForNavMeshExport
// description:	When exporting navmeshes we need to check against navmesh bounds for object creation/destruction
//
void
CObjectPopulation::ManageDummyForNavMeshExport(CDummyObject* pDummyObj)
{
	// is in current area and has been added to the world
	if (pDummyObj->GetIsVisible() && pDummyObj->GetIsAddedToWorld())
	{	
		/*
		// Ensure this dummy has not already been converted
		CObject::Pool *objectPool = CObject::GetPool();
		for (s32 o = 0; o<objectPool->GetSize(); o++)
		{
			CObject* pObject = objectPool->GetSlot(o);
			if (pObject && pObject->GetRelatedDummy()==pDummyObj)
			{
				return;
			}
		}
		*/

		CBaseModelInfo * pModelInfo = pDummyObj->GetBaseModelInfo();
		if(pModelInfo)
		{
			gtaFragType * pFragType = pModelInfo->GetFragType();

			// Don't export cloth objects - for some reason this crashes the game allocating
			// memory for the index buffer.
			if(pFragType && pModelInfo->GetFragType()->GetNumEnvCloths())
				return;
	/*
			CBaseModelInfo* pModelInfo = pDummyObj->GetBaseModelInfo();

			// Climbable objects must make it through to the exporter, so we can cut navmesh along their major axis
			const bool bClimbableObject = pModelInfo->GetIsClimbableByAI();

			const bool bFixedFlagSet = pDummyObj->IsBaseFlagSet(fwEntity::IS_FIXED) || pModelInfo->GetIsFixed();

			if(!bFixedFlagSet && !pModelInfo->GetIsFixedForNavigation() && !pModelInfo->GetIsLadder() && !bClimbableObject)
				return;
	*/
			if(CNavMeshDataExporter::EntityIntersectsCurrentNavMesh(pDummyObj))
			{
				ConvertToRealObject(pDummyObj);
			}
		}
	}
}
#endif

void CObjectPopulation::PropagateExtensionsToRealObject(CDummyObject* pDummy, CObject* pObject)
{
	if ( pDummy->GetExtension<CDoorExtension>() )
	{
		CDoorExtension*		realDoorExt = rage_new CDoorExtension;
		*realDoorExt = *pDummy->GetExtension<CDoorExtension>();
		pObject->GetExtensionList().Add( *realDoorExt );
	}

	if (pDummy->GetExtension<CLightExtension>())
	{
		CLightExtension *pInstance = pDummy->GetExtension<CLightExtension>();
		pDummy->GetExtensionList().Unlink(pInstance);
		pObject->GetExtensionList().Add(*pInstance);
	}

	if (pDummy->GetExtension<CSpawnPointOverrideExtension>())
	{
		CSpawnPointOverrideExtension *pInstance = pDummy->GetExtension<CSpawnPointOverrideExtension>();
		pDummy->GetExtensionList().Unlink(pInstance);
		pObject->GetExtensionList().Add(*pInstance);
	}

	CTrafficLights::TransferTrafficLightInfo(pDummy, pObject);
}

void CObjectPopulation::PropagateExtensionsToDummyObject(CObject* pObject, CDummyObject* pDummy)
{
	CTrafficLights::TransferTrafficLightInfo(pObject, pDummy);

	if (pObject->GetExtension<CLightExtension>())
	{
		CLightExtension *pInstance = pObject->GetExtension<CLightExtension>();
		pObject->GetExtensionList().Unlink(pInstance);
		pDummy->GetExtensionList().Add(*pInstance);
	}

	if (pObject->GetExtension<CSpawnPointOverrideExtension>())
	{
		CSpawnPointOverrideExtension *pInstance = pObject->GetExtension<CSpawnPointOverrideExtension>();
		pObject->GetExtensionList().Unlink(pInstance);
		pDummy->GetExtensionList().Add(*pInstance);
	}
}

// Name			:	ConvertToRealObject
// Purpose		:	Converts a dummy object to a 'real' object
// Parameters	:	pDummy - ptr to the object ped to convert
// Returns		:	Nothing

CObject* CObjectPopulation::ConvertToRealObject(CDummyObject* pDummyObject)
{
#if RAGE_TIMEBARS
#if INCLUDE_DETAIL_TIMEBARS // Only if we have access to the model name and have detail timebars on
	char timebarName[32];
	formatf(timebarName, "ToReal: %s", pDummyObject->GetModelName());
	pfAutoMarker convertTimebar(timebarName, 32);
#endif // INCLUDE_DETAIL_TIMEBARS
#endif // RAGE_TIMEBARS
	

	BANK_ONLY(sysTimer convertTimer);
	bool bWillSpawnPeds = pDummyObject->m_nFlags.bWillSpawnPeds;		//maintain this flag between objects

	// create object in dummy position
	//CObject* pObject = new CObject(pDummyObject->GetModelIndex());
	//pObject->SetMatrix(pDummyObject->GetMatrix());
	if(!TestSafeForRealObject(pDummyObject))
		return NULL;

	// don't create objects for map sections which are culled. may not be needed so leaving commented out for now.
 	//if (g_MapDataStore.GetIsCulled(pDummyObject->GetIplIndex()))
 	//	return NULL;

	CObject* pObject = pDummyObject->CreateObject();
	if (pObject)
	{
		// copy over interior data
		pObject->m_nFlags.bWillSpawnPeds = bWillSpawnPeds;

		fwInteriorLocation	interiorLocation = pDummyObject->GetInteriorLocation();

		// make the dummy not visibile and not searcheable
		pDummyObject->Disable();
		
		// Propagate extensions
		CObjectPopulation::PropagateExtensionsToRealObject( pDummyObject, pObject );

		//delete pDummyObject;
		// Resolve references (or garage doors are not aware of the removal of the dummy

		// Have to do this after the ResolveReferences() because the dummy pointer would just get 
		// set back to NULL if I did it before
		pObject->SetRelatedDummy(pDummyObject, interiorLocation);

		// Add to the world
		CGameWorld::Add(pObject, interiorLocation );		// if dummy was inside, then so is created object

		// This is needed (in MP, mostly) for dealing with scenario peds that are currently using the object
		// that just turned into a CObject from a CDummyObject. They need to connect back to the scenario points,
		// and re-attach themselves and update the physical properties of the CObject, if appropriate.
		CTaskUseScenarioEntityExtension* pUseScenarioEntityExt = pDummyObject->GetExtension<CTaskUseScenarioEntityExtension>();
		if(pUseScenarioEntityExt)
		{
			pUseScenarioEntityExt->NotifyTasksOfNewObject(*pObject);
		}

#if __BANK
		if(ms_objectPopDebugDrawingVM && ms_showDummyToRealObjectConversionsOnVM)
		{
			const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pDummyObject->GetTransform().GetPosition()); 
			CVectorMap::MakeEventRipple(vObjectPosition, 10.0f, 1000, Color_white);			
		}		
		if(ms_showMostExpensiveRealObjectConversions)
		{			
			TrackObjectConversion(pObject, convertTimer.GetMsTime());
		}
		ms_realConversionsThisFrame++;
#endif

		//-----------------------------------------------------------
		// Deal with intersecting peds.
		// see url:bugstar:2068426

		CPed::Pool *pPool = CPed::GetPool();
		const int iNumSlots = pPool->GetSize();

		spdAABB aabbObj, aabbPed;
		pObject->GetAABB(aabbObj);

		for(s32 p=0; p<iNumSlots; p++)
		{
			CPed* pPed = pPool->GetSlot(p);
			if(pPed)
			{
				// For safety's sake let's restrict this to ambient peds; we might want to reevaluate this later
				if(pPed->PopTypeIsRandom())
				{
					// Approximate AABB test
					pPed->GetAABB(aabbPed);
					if(aabbObj.IntersectsAABB(aabbPed))
					{
						// More thorough test between object's OOBB and ped's bounding radius
						CEntityBoundAI boundAI;
						boundAI.Set(*pObject, pPed->GetTransform().GetPosition().GetZf(), pPed->GetCapsuleInfo()->GetHalfWidth(), false, NULL);

						if(boundAI.LiesInside(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())))
						{
							// Disable collision from this ped to the object, until the ped is out of collision
							pPed->SetNoCollision(pObject, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
						}
					}
				}
			}
		}

	}

	return(pObject);
}




// Name			:	ConvertToDummyObject
// Purpose		:	Converts a object to a dummy object
// Parameters	:	None
// Returns		:	Nothing


CEntity* CObjectPopulation::ConvertToDummyObject(CObject* pObject, bool bForce)
{
#if NAVMESH_EXPORT
	const bool bNavTool = CNavMeshDataExporter::IsExportingCollision();
#else
	static const bool bNavTool = false;
#endif

#if RAGE_TIMEBARS
#if INCLUDE_DETAIL_TIMEBARS // Only if we have access to the model name and have detail timebars on
	char timebarName[32];
	formatf(timebarName, "ToDummy: %s", pObject->GetModelName());
	pfAutoMarker convertTimebar(timebarName, 32);
#endif // INCLUDE_DETAIL_TIMEBARS
#endif // RAGE_TIMEBARS

	// don't every convert a procedural object into a dummy object - the procedural code should be in complete control of these
	if (pObject->GetIsAnyManagedProcFlagSet() || pObject->GetOwnedBy() == ENTITY_OWNEDBY_CUTSCENE)
	{
		return NULL;
	}

#if __ASSERT
	if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
	{
		bool bObjectGrabbedFromTheWorldByScript = false;
		CScriptEntityExtension* pExtension = pObject->GetExtension<CScriptEntityExtension>();
		if (Verifyf(pExtension, "CObjectPopulation::ConvertToDummyObject - The object '%s' is ENTITY_OWNEDBY_SCRIPT but doesn't have a script extension", pObject->GetModelName()))
		{
			if (pExtension->GetScriptObjectWasGrabbedFromTheWorld())
			{
				bObjectGrabbedFromTheWorldByScript = true;
			}
		}

		if (bObjectGrabbedFromTheWorldByScript)
		{
			Assertf(0, "CObjectPopulation::ConvertToDummyObject - %s object is a script object. It has been grabbed from the world by the GET_CLOSEST_OBJECT_OF_TYPE command", pObject->GetModelName());
		}
		else
		{
			Assertf(0, "CObjectPopulation::ConvertToDummyObject - %s object is a script object", pObject->GetModelName());
		}
	}
#endif	//	__ASSERT

	CDummyObject* pDummyObject = pObject->GetRelatedDummy();

	// If IPL is not loaded then don't recreate the dummy (or if interior which created dummy not available either)
	if((!(pObject->m_nFlags.bInMloRoom) && INSTANCE_STORE.Get(strLocalIndex(pObject->GetIplIndex())) == NULL) || 
		( pDummyObject && pDummyObject->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR && !pObject->GetRelatedDummyLocation().IsValid()))
	{
		// remove object
		if(!bNavTool)
		{
			g_ptFxManager.RemovePtFxFromEntity(pObject);
		}
		if ( pDummyObject )
			pDummyObject->Disable();
		pObject->ClearRelatedDummy();
		DestroyObject(pObject, true);
		return NULL;
	}

	if(pDummyObject)
	{
		if (pObject->m_nFlags.bRenderDamaged && !TestRoomForDummyObject(pObject) && !bForce)
			return NULL;

		pDummyObject->UpdateFromObject(pObject, true);
		pDummyObject->Enable();
		CObjectPopulation::PropagateExtensionsToDummyObject(pObject, pDummyObject);
	}

	fwInteriorLocation loc = pObject->GetRelatedDummyLocation();
	pObject->ClearRelatedDummy();

	// remove object
	if(!bNavTool)
	{
		g_ptFxManager.RemovePtFxFromEntity(pObject);
	}
	DestroyObject(pObject, true);

	// delete dummies which were created by interior populating but interior is no longer available
	if(pDummyObject && (pDummyObject->GetIplIndex() == 0))
	{
		if (!(loc.IsValid()) && pDummyObject->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR)
		{
			pDummyObject->Disable();
			return NULL;
		}
	}

#if __BANK
	if(ms_objectPopDebugDrawingVM && ms_showRealToDummyObjectConversionsOnVM)
	{
		const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pDummyObject->GetTransform().GetPosition()); 
		CVectorMap::MakeEventRipple(vObjectPosition,10.0f, 1000, Color_black);
	}
	ms_dummyConversionsThisFrame++;
#endif

	return(pDummyObject);
}


// Name			:	TestRoomForDummyObject
// Purpose		:	Tests whether there would be room for this dummy object.
//					This might not be the case if there is a car or ped blocking
//					its position.
// Parameters	:	None
// Returns		:	Nothing

bool CObjectPopulation::TestRoomForDummyObject(CObject* pObject)
{
	Vector3		Center;
	float		Radius;
	s32		Num;
	
	Assert(pObject->GetRelatedDummy());
	
	//have to assume there is always room to convert an interior object to a dummy
	if (pObject->m_nFlags.bInMloRoom){
		return(true);
	}

	Center = pObject->GetRelatedDummy()->GetBoundCentre();
	Radius = pObject->GetBaseModelInfo()->GetBoundingSphereRadius();

	// We test for peds and car that collide with our bounding bubble
	CGameWorld::FindObjectsKindaColliding(Center, Radius, false, false, &Num, 2, NULL, 0, 1, 1, 0, 0);	// Just Cars and Peds we're interested in

	if (Num)
	{
		return false;
	}
	return true;
}

//
// name:		TestSafeForRealObject
// description:
//

bool CObjectPopulation::TestSafeForRealObject(CDummyObject* UNUSED_PARAM(pDummy))
{
	MUST_FIX_THIS(sandy - replace with some rage version);
	MUST_FIX_THIS(possibly using TestCollision function which should test with the world);

	return true;
}



//
// name:		ConvertAllObjectsToDummyObjects
// description:	Convert all the object back to dummy objects so that the map is back in its initial state
//

void CObjectPopulation::ConvertAllObjectsToDummyObjects(bool bConvertObjectsWithBrains)
{
	CObject* pObj;
	s32 i = (s32) CObject::GetPool()->GetSize();
	
	while(i--)
	{
		pObj = CObject::GetPool()->GetSlot(i);

		if (pObj && pObj->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM  && (bConvertObjectsWithBrains || (pObj->m_nObjectFlags.ScriptBrainStatus != CObject::OBJECT_RUNNING_SCRIPT_BRAIN) ) )
		{
			ConvertToDummyObject(pObj);
		}
	}

}

void CObjectPopulation::ConvertAllObjectsInAreaToDummyObjects(const Vector3& Centre, float Radius, bool bConvertObjectsWithBrains, CObject *pExceptionObject, bool bConvertDoors, bool bForce/*=false*/, bool bExcludeLadders/*=false*/)
{
	CObject* pObj;
	s32 i = (s32) CObject::GetPool()->GetSize();
	
	while(i--)
	{
		pObj = CObject::GetPool()->GetSlot(i);

		if (pObj && pObj!=pExceptionObject && pObj->GetRelatedDummy() && !pObj->m_nFlags.bNeverDummy)
		{
			if ((pObj->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM || pObj->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR)
				&& (bConvertObjectsWithBrains || (pObj->m_nObjectFlags.ScriptBrainStatus != CObject::OBJECT_RUNNING_SCRIPT_BRAIN) ) 
				&&  (bConvertDoors || !pObj->IsADoor())
				&& pObj->GetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE)
				&& (!bExcludeLadders || !pObj->GetBaseModelInfo()->GetIsLadder()))
			{
				const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
				float DistSqr = (Centre.x - vObjectPosition.x)*(Centre.x - vObjectPosition.x) +
							(Centre.y - vObjectPosition.y)*(Centre.y - vObjectPosition.y) +
							(Centre.z - vObjectPosition.z)*(Centre.z - vObjectPosition.z);

				if (DistSqr < Radius*Radius)
				{
					// If this is an object that hasn't been uprooted anyway, don't touch it if a mission
					// ped is attached to it. This is done to cover the case of mission peds using
					// prop-attached scenarios, if we remove the object the scenario point will go away with it.
					if(!pObj->m_nObjectFlags.bHasBeenUprooted)
					{
						bool allowedToReset = true;

						const fwEntity* childAttachment = pObj->GetChildAttachment();
						while(childAttachment)
						{
							if(static_cast<const CEntity*>(childAttachment)->GetIsTypePed())
							{
								const CPed* pPed = static_cast<const CPed*>(childAttachment);
								if(pPed->PopTypeIsMission())
								{
									allowedToReset = false;
									break;
								}
							}
							childAttachment = childAttachment->GetSiblingAttachment();
						}

						if(!allowedToReset)
						{
							continue;
						}
					}

					bool bAmbientScaleUpdateRequired = pObj->m_nObjectFlags.bHasBeenUprooted || pObj->m_nDEflags.bHasMovedSinceLastPreRender;

					CNetObjObject* pNetObj = SafeCast(CNetObjObject, pObj->GetNetworkObject());

					if (pNetObj)
					{
						if (!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange())
						{
							pNetObj->SetDontAddToUnassignedList();
						}
						else
						{
							pNetObj->FlagForDeletion();
						}
					}

					CDummyObject* pDummy = pObj->GetRelatedDummy();
					ConvertToDummyObject(pObj, bForce);

					// Damaged objects need to be cleared of damage
					pDummy->m_nFlags.bRenderDamaged = false;

					// If the dummy ends up being visible this frame, it will need its ambient scales updated if
					// the real object has been uprooted or if the real object would have needed an ambient scale update
					if( pDummy->GetUseAmbientScale() && pDummy->GetUseDynamicAmbientScale() && bAmbientScaleUpdateRequired)
					{
						ms_dummiesThatNeedAmbientScaleUpdate.PushAndGrow(CEntityIDHelper::ComputeEntityID(pDummy));
					}

					//////////////////////////////////////////////////////////////////////////
					// if the object has moved from the dummy position, re-instating the dummy can cause a visible pop if near the player.
					// so we need to check if the dummy is on screen - if it is, suppress the dummy until it goes off-screen.
					if (!bForce)
					{
						const Vector3 vDummyPosition = VEC3V_TO_VECTOR3(pDummy->GetTransform().GetPosition());
						if (vObjectPosition!=vDummyPosition && pDummy->IsBaseFlagSet(fwEntity::IS_VISIBLE))
						{
							Vector3 vCentre;
							float fRadius = pDummy->GetBoundCentreAndRadius(vCentre);
							if ( camInterface::IsSphereVisibleInGameViewport(vCentre, fRadius) && camInterface::IsFadedIn() )
							{
								SuppressDummyUntilOffscreen(pDummy);
							}
						}
					}
					//////////////////////////////////////////////////////////////////////////
				}


			}
		}
	}

	CNetObjObject::RemoveUnassignedAmbientObjectsInArea(Centre, Radius);
}

void CObjectPopulation::SubmitDummyObjectsForAmbientScaleCalc()
{
	int count = ms_dummiesThatNeedAmbientScaleUpdate.GetCount();
	for(int i = 0; i < count; i++)
	{
		CDummyObject *pDummy = static_cast<CDummyObject*>(CEntityIDHelper::GetEntityFromID(ms_dummiesThatNeedAmbientScaleUpdate[i]));
		if(pDummy)
		{
			CTimeCycleAsyncQueryMgr::AddNonPedEntity(pDummy);
		}
	}

	ms_dummiesThatNeedAmbientScaleUpdate.ResetCount();
}

void CObjectPopulation::SuppressDummyUntilOffscreen(CDummyObject* pDummyObject)
{
	pDummyObject->SetHidden(true);
	ms_suppressedDummyObjects.Grow() = pDummyObject;
}

void CObjectPopulation::UpdateSuppressedDummyObjects()
{
	for (s32 i=0; i<ms_suppressedDummyObjects.GetCount(); i++)
	{
		CDummyObject* pDummyObject = ms_suppressedDummyObjects[i];

		bool bCanFlush = true;
		if ( pDummyObject )
		{
			Vector3 vCentre;
			float fRadius = pDummyObject->GetBoundCentreAndRadius(vCentre);
			bCanFlush = ( !camInterface::IsSphereVisibleInGameViewport(vCentre, fRadius) );
		}

		if (bCanFlush)
		{
			if (pDummyObject)
			{
				pDummyObject->SetHidden(false);
			}
			ms_suppressedDummyObjects.DeleteFast(i);
			i--;
		}
	}
}

void CObjectPopulation::FlushAllSuppressedDummyObjects()
{
	for (s32 i=0; i<ms_suppressedDummyObjects.GetCount(); i++)
	{
		if (ms_suppressedDummyObjects[i] != NULL)
		{
			ms_suppressedDummyObjects[i]->SetHidden(false);
		}
	}
	ms_suppressedDummyObjects.Reset();
}

bool CObjectPopulation::FoundHighPriorityDummyCB(CEntity* pEntity, void* UNUSED_PARAM(pData))
{
	if (pEntity)
	{
		ms_highPriorityDummyObjects.Grow() = (CDummyObject*) pEntity;
	}
	return (ms_highPriorityDummyObjects.GetCount() < NUM_PRIORITY_DUMMIES_PER_FRAME);
}

void CObjectPopulation::SetHighPriorityDummyObjects()
{
	ms_highPriorityDummyObjects.ResetCount();

	TUNE_GROUP_FLOAT ( OBJECT_POP, fPriorityActivationSpeed, 20.0f, 1.0f, 200.0f, 1.00f);
	TUNE_GROUP_FLOAT ( OBJECT_POP, fPriorityRange, 45.0f, 1.0f, 200.0f, 1.00f);

	if (CFocusEntityMgr::GetMgr().IsPlayerPed())
	{
		camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector();
		if (camInterface::IsDominantRenderedDirector(cinematicDirector))
		{
			//////////////////////////////////////////////////////////////////////////
			// sometimes the cinematic camera can roam quite some distance away from the player ped
			// which some neuters all of our visibility-driven priority dummy object identification
			// (because dummy objects near the player may be far from the camera)
			//
			// in this case we do a small world search around the player position for any urgently required object
			// promotion

			spdSphere searchSphere( VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPos()), ScalarV(10.0f) );
			fwIsSphereIntersecting searchVolume( searchSphere );

			CGameWorld::ForAllEntitiesIntersecting
			(
				&searchVolume,
				FoundHighPriorityDummyCB,
				NULL,
				ENTITY_TYPE_MASK_DUMMY_OBJECT,
				SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,
				SEARCH_LODTYPE_HIGHDETAIL,
				SEARCH_OPTION_FORCE_PPU_CODEPATH,
				WORLDREP_SEARCHMODULE_STREAMING
			);

			//////////////////////////////////////////////////////////////////////////
		}
		else
		{
			//////////////////////////////////////////////////////////////////////////
			/// 
			Vector3 vDir = CFocusEntityMgr::GetMgr().GetVel();
			Vector3 vCamDir = camInterface::GetFront();

			const float fVelMag = vDir.Mag2();
			vCamDir.NormalizeSafe();
			vDir.NormalizeSafe();

			const float fSqrThresh = fPriorityActivationSpeed*fPriorityActivationSpeed;

			if (vCamDir.Dot(vDir)>0.0f && fVelMag>=fSqrThresh)
			{
				s32 renderListIndex = g_SceneToGBufferPhaseNew->GetEntityListIndex();
				fwRenderListDesc& gbufRenderListDesc = gRenderListGroup.GetRenderListForPhase(renderListIndex);
				u32 numEntities = gbufRenderListDesc.GetNumberOfEntities(RPASS_VISIBLE);
				for (u32 i=0; i<numEntities; i++)
				{
					CEntity* pEntity = (CEntity*) gbufRenderListDesc.GetEntity(i, RPASS_VISIBLE);
					float fDist = gbufRenderListDesc.GetEntitySortVal(i, RPASS_VISIBLE);
					if (fDist > fPriorityRange)
					{
						return;
					}
					else
					{
						if (pEntity && pEntity->GetIsTypeDummyObject())
						{
							ms_highPriorityDummyObjects.Grow() = (CDummyObject*) pEntity;

							if (ms_highPriorityDummyObjects.GetCount() >= NUM_PRIORITY_DUMMIES_PER_FRAME)
							{
								return;
							}
						}
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
		}

	}
}

s32 CObjectPopulation::ManageHighPriorityDummyObjects(const Vector3& vPos, bool bIsFirstPersonAiming)
{
#if NAVMESH_EXPORT
	// If exporting collision data we want to convert all the objects asap
	if(CNavMeshDataExporter::IsExportingCollision())
		return 0;
#endif

	PF_AUTO_PUSH_DETAIL("Manage High Priority Dummies");
	s32 numHighPrio = 0;
	for (s32 i=0; i<ms_highPriorityDummyObjects.GetCount(); i++)
	{
		CDummyObject* pDummy = ms_highPriorityDummyObjects[i];
		if (pDummy)
		{
			ManageDummy(pDummy, vPos, bIsFirstPersonAiming);
			pDummy->m_nFlags.bFoundInPvs = false;
			numHighPrio++;
		}
	}

	for (s32 i = 0; i < NUM_CLOSE_DUMMIES_PER_FRAME; ++i)
	{
		CEntity* ent = ms_closeDummiesToProcess[i].ent;
		if (ent && Verifyf(ent->GetIsTypeDummyObject(), "Non dummy object found in closest dummy list!"))
		{
			ManageDummy((CDummyObject*)ent, vPos, bIsFirstPersonAiming);
			ent->m_nFlags.bFoundInPvs = false;
			numHighPrio++;
		}

		ms_closeDummiesToProcess[i].ent = NULL;
		ms_closeDummiesToProcess[i].dist = 99999.f;
	}

	return numHighPrio;
}

void CObjectPopulation::InsertSortedDummyClose(CEntity* ent, float dist)
{
	for (s32 i = 0; i < NUM_CLOSE_DUMMIES_PER_FRAME; ++i)
	{
		if (dist < ms_closeDummiesToProcess[i].dist)
		{
			for (s32 f = NUM_CLOSE_DUMMIES_PER_FRAME - 1; f > i; --f)
				ms_closeDummiesToProcess[f] = ms_closeDummiesToProcess[f - 1];

			ms_closeDummiesToProcess[i].dist = dist;
			ms_closeDummiesToProcess[i].ent = ent;
			break;
		}
	}
}

bool CObjectPopulation::ForceConvertDummyCB(CEntity* pEntity, void* UNUSED_PARAM(pData))
{
	if (pEntity)
	{
		CDummyObject* pDummy = (CDummyObject*) pEntity;
		if (pDummy->GetIsVisible() && pDummy->GetIsAddedToWorld())
		{	
			ConvertToRealObject(pDummy);
		}
	}
	return true;
}

void CObjectPopulation::ForceConvertForScript()
{
	if (ms_bScriptRequiresForceConvert)
	{
		ms_bScriptRequiresForceConvert = false;
		fwIsSphereIntersecting	testSphere( ms_volumeForScriptForceConvert );

		CGameWorld::ForAllEntitiesIntersecting
		(
			&testSphere,
			ForceConvertDummyCB,
			NULL,
			ENTITY_TYPE_MASK_DUMMY_OBJECT,
			SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,
			SEARCH_LODTYPE_HIGHDETAIL,
			SEARCH_OPTION_FORCE_PPU_CODEPATH,
			WORLDREP_SEARCHMODULE_DEFAULT
		);
	}
}


void CObjectPopulation::RegisterDummyObjectMarkedAsNeverDummy(CDummyObject* pDummy)
{
	if (pDummy && ms_numNeverDummies<NUM_NEVERDUMMY_PROCESSED)
	{
		s32 dummyIndex = CDummyObject::GetPool()->GetJustIndexFast(pDummy);
		ms_neverDummyList[ms_numNeverDummies++] = (u16) dummyIndex;
	}
}

void CObjectPopulation::ForceConvertDummyObjectsMarkedAsNeverDummy()
{
	for (s32 i=0; i<ms_numNeverDummies; i++)
	{
		s32 dummyIndex = ms_neverDummyList[i];
		CDummyObject* pDummy = CDummyObject::GetPool()->GetSlot(dummyIndex);
		if ( pDummy && pDummy->m_nFlags.bNeverDummy && pDummy->GetIsVisible() && pDummy->GetIsAddedToWorld() )
		{
			//////////////////////////////////////////////////////////////////////////
			// only convert dummy objects to real objects if their assets are fully loaded
			// exceptions:
			// - objects which aren't in a lod hierarchy
			// - never-dummy objects which don't have an auto-anim
			{
				CBaseModelInfo* pModelInfo = pDummy->GetBaseModelInfo();
				const bool bAutoAnimObject = ( pDummy->m_nFlags.bNeverDummy && pModelInfo && pModelInfo->GetAutoStartAnim() );
				if ( !pDummy->GetLodData().HasLod() || (pModelInfo && pModelInfo->GetHasLoaded()) || !bAutoAnimObject )
				{
#if GTA_REPLAY
					CObject* pObject = ConvertToRealObject(pDummy);
					if(pObject)
					{
						CReplayMgr::RecordMapObject(pObject);
					}
#else
					ConvertToRealObject(pDummy);
#endif // GTA_REPLAY
				}
			}
			//////////////////////////////////////////////////////////////////////////
		}
	}
	ms_numNeverDummies = 0;
}

//
//
//
//
bool CObjectPopulation::sm_bCObjectsAlbedoSupportEnabled = false;
bool CObjectPopulation::sm_bCObjectsAlbedoRenderingActive = false;


//
//
//
//
void CObjectPopulation::SetCObjectsAlbedoRenderState()
{
	grcBindTexture(NULL);
	grcWorldIdentity();

	grcLightState::SetEnabled(false);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
}

//
//
//
//
void CObjectPopulation::RenderAllAlbedoObjects()
{
CObject::Pool* pObjPool = CObject::GetPool();
const s32 nPoolSize = (s32) pObjPool->GetSize();

	extern bool g_prototype_batch;
	const bool bOldProtoTypeBatchVal = g_prototype_batch;
	g_prototype_batch = false;

	for(s32 i=0; i<nPoolSize; i++)
	{
		CObject *pObject = pObjPool->GetSlot(i);
		if(pObject)
		{			
			if(	(pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)	&&
				(pObject->GetIsAlbedoAlpha())						)
			{
				CObjectDrawHandler* pDrawHandler = (CObjectDrawHandler*)pObject->GetDrawHandlerPtr();
				if(pDrawHandler)
				{
					CDrawDataAddParams params;
					pDrawHandler->AddToDrawList(pObject, &params);
				}
			}
		}
	}

	g_prototype_batch = bOldProtoTypeBatchVal;
}

//
//
//
//
void CObjectPopulation::RenderCObjectsAlbedo()
{
	PF_PUSH_TIMEBAR_DETAIL("CObjectsAlbedo");


	const bool doRender = !CPhoneMgr::CamGetState();

	if(doRender)
	{
		if(IsCObjectsAlbedoSupportEnabled())
		{
			DLC_Add( CObjectPopulation::SetCObjectsAlbedoRenderState );
			
			SetCObjectsAlbedoRenderingActive(true);
				CObjectPopulation::RenderAllAlbedoObjects();
			SetCObjectsAlbedoRenderingActive(false);
		}
	}

	PF_POP_TIMEBAR_DETAIL();
}


#if __BANK
void CObjectPopulation::InitWidgets()
{
	// Set up some bank stuff.
	bkBank &bank = BANKMGR.CreateBank("Object Population");
	bank.PushGroup("Debugging", true);
	{
		bank.AddToggle("Object Population Debug Info", &ms_objectPopDebugInfo);
		bank.AddToggle("Object Population Debug Vector Map", &ms_objectPopDebugDrawingVM);
		bank.AddToggle("Draw Real Objects (on VM)", &ms_drawRealObjectsOnVM);
		bank.AddToggle("Draw Dummy Objects (on VM)", &ms_drawDummyObjectsOnVM);
		bank.AddToggle("Show Real Object Conversions (on VM)", &ms_showDummyToRealObjectConversionsOnVM);
		bank.AddToggle("Show Dummy Object Converions (on VM)", &ms_showRealToDummyObjectConversionsOnVM);
		bank.AddToggle("Show Most Expensive Object Conversions", &ms_showMostExpensiveRealObjectConversions);
		bank.AddButton("Clear Most Expensive Object Conversions", datCallback(ClearTrackedObjectConversions));
		bank.AddButton("Convert Objects Around Player To Dummy", datCallback(ConvertObjectsAroundPlayerToDummy));
		
	}
	bank.PopGroup();

}

void CObjectPopulation::UpdateDebugDrawing()
{
	if(ms_objectPopDebugDrawingVM || ms_objectPopDebugInfo)
	{
		// define the distances we care about
#define OBJECT_POPULDATION_DEBUG_DISTANCES (3)
		float distances[OBJECT_POPULDATION_DEBUG_DISTANCES] = {80.f,40.f,20.f};

		Vector3 vecCentre = CFocusEntityMgr::GetMgr().GetPos();
		if(ms_objectPopDebugDrawingVM)
		{
			// spot for the player
			CVectorMap::DrawCircle(vecCentre, 1, Color_white, false);
			// careful here when changing distances
			CVectorMap::DrawCircle(vecCentre, distances[0], Color_red, false);
			CVectorMap::DrawCircle(vecCentre, distances[1], Color_blue, false);
			CVectorMap::DrawCircle(vecCentre, distances[2], Color_green, false);					
		}
		
		if(ms_objectPopDebugDrawingVM && ms_drawRealObjectsOnVM)
		{
			// Draw Real Objects in Green
			CObject::Pool *objectPool = CObject::GetPool();
			s32 PoolSize = (s32) objectPool->GetSize();
			
			for (s32 i = 0; i < PoolSize; i++)
			{
				CObject* obj = objectPool->GetSlot(i);
				if(obj)
				{
					const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(obj->GetTransform().GetPosition());
					CVectorMap::DrawMarker(vObjectPosition, Color_green, .5f);
				}
			}
		}
		
		// count dummy objects within a given distance to the player
		
		u32 dummiesWithin[OBJECT_POPULDATION_DEBUG_DISTANCES];
		for(int i = 0; i < OBJECT_POPULDATION_DEBUG_DISTANCES; i++)
		{
			dummiesWithin[i] = 0;
		}
		
		// Draw Dummy Objects Purple
		// Draw Real Objects in Green
		CDummyObject::Pool *dummyObjectPool = CDummyObject::GetPool();
		s32 PoolSize = dummyObjectPool->GetSize();

		for (s32 i = 0; i < PoolSize; i++)
		{
			CDummyObject* pDummy = dummyObjectPool->GetSlot(i);			
			if(pDummy && pDummy->GetIsVisible() && pDummy->GetIsAddedToWorld())
			{
				const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pDummy->GetTransform().GetPosition());
				float dist2 = vecCentre.Dist2(vObjectPosition);
				for(int d = 0; d < OBJECT_POPULDATION_DEBUG_DISTANCES; d++)
				{
					if(dist2 < rage::square(distances[d]))
					{
						dummiesWithin[d]++;
					}
				}

				if(ms_objectPopDebugDrawingVM && ms_drawDummyObjectsOnVM)
				{
					CVectorMap::DrawMarker(vObjectPosition, Color_purple, .5f);
				}
			}
		}

		// high priority dummies
		for (s32 i=0; i<ms_highPriorityDummyObjects.GetCount(); i++)
		{
			CDummyObject* pDummy = ms_highPriorityDummyObjects[i];
			if (pDummy && pDummy->GetIsVisible() && pDummy->GetIsAddedToWorld())
			{
				const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pDummy->GetTransform().GetPosition());
				float dist2 = vecCentre.Dist2(vObjectPosition);
				for(int d = 0; d < OBJECT_POPULDATION_DEBUG_DISTANCES; d++)
				{
					if(dist2 < rage::square(distances[d]))
					{
						dummiesWithin[d]++;
					}
				}
				if(ms_objectPopDebugDrawingVM && ms_drawDummyObjectsOnVM)
				{					
					CVectorMap::DrawMarker(vObjectPosition, Color_red, .5f);
				}					
			}
		}

		// Close Dummies
		for (s32 i = 0; i < NUM_CLOSE_DUMMIES_PER_FRAME; ++i)
		{
			CEntity* ent = ms_closeDummiesToProcess[i].ent;
			if (ent)
			{
				const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(ent->GetTransform().GetPosition());
				float dist2 = vecCentre.Dist2(vObjectPosition);
				for(int d = 0; d < OBJECT_POPULDATION_DEBUG_DISTANCES; d++)
				{
					if(dist2 < rage::square(distances[d]))
					{
						dummiesWithin[d]++;
					}
				}
				if(ms_objectPopDebugDrawingVM && ms_drawDummyObjectsOnVM)
				{					
					CVectorMap::DrawMarker(vObjectPosition, Color_red, .5f);
				}
			}
		}

		// Give some debug info
		if(ms_objectPopDebugInfo)
		{
			bool curr = grcDebugDraw::GetDisplayDebugText();		
			grcDebugDraw::SetDisplayDebugText(true);
			char debugText[256];

			formatf(debugText, "Real: %u/%u", CObject::GetPool()->GetNoOfUsedSpaces(), CObject::GetPool()->GetSize());
			grcDebugDraw::AddDebugOutput(debugText);
			formatf(debugText, "Dummy: %u/%u", CDummyObject::GetPool()->GetNoOfUsedSpaces(), CDummyObject::GetPool()->GetSize());
			grcDebugDraw::AddDebugOutput(debugText);
			formatf(debugText, "%.3f Dummy->Real Obj Per Frame", ms_currentRealConversionsPerFrame);
			grcDebugDraw::AddDebugOutput(debugText);
			formatf(debugText, "%.3f Real->Dummy Obj Per Frame", ms_currentDummyConversionsPerFrame);
			grcDebugDraw::AddDebugOutput(debugText);

			for(int d = 0; d < OBJECT_POPULDATION_DEBUG_DISTANCES; d++)
			{
				formatf(debugText, "%u Dummies Within %.0fm", dummiesWithin[d], distances[d]);
				grcDebugDraw::AddDebugOutput(debugText);				
			}

			if(ms_showMostExpensiveRealObjectConversions && ms_pExpensiveObjectConversions)
			{
				formatf(debugText, "%s", "");
				grcDebugDraw::AddDebugOutput(debugText);
				// More debug info about expensive object conversions
				for(int i = 0; i < EXPENSIVE_OBJECT_CONVERSIONS_TO_TRACK; i++)
				{
					formatf(debugText, "%u. %s - %.4f", i+1, ms_pExpensiveObjectConversions[i].m_objectName, ms_pExpensiveObjectConversions[i].m_conversionTime);
					grcDebugDraw::AddDebugOutput(debugText);
				}
			}

			grcDebugDraw::SetDisplayDebugText(curr);
		}
		
	}	

	// Update our conversion counts
	ms_currentRealConversionsPerFrame = ms_currentRealConversionsPerFrame*.97f + (ms_realConversionsThisFrame*.03f);
	ms_realConversionsThisFrame = 0;
	ms_currentDummyConversionsPerFrame = ms_currentDummyConversionsPerFrame*.97f + (ms_dummyConversionsThisFrame*.03f);
	ms_dummyConversionsThisFrame = 0;

}

// See if the given conversion ranks in our list of most expensive conversions
void CObjectPopulation::TrackObjectConversion(CObject* pObject, float msTime)
{
	if(ms_showMostExpensiveRealObjectConversions && ms_pExpensiveObjectConversions)
	{
		for(int i = 0; i < EXPENSIVE_OBJECT_CONVERSIONS_TO_TRACK; i++)
		{
			// keep the list sorted
			if (msTime > ms_pExpensiveObjectConversions[i].m_conversionTime)
			{
				if(StringEndsWithI(ms_pExpensiveObjectConversions[i].m_objectName, pObject->GetDebugName()) != false)
				{
					// Same object name, update the value
					ms_pExpensiveObjectConversions[i].m_conversionTime = msTime;
					break;
				}

				CObjectPopulation::ObjectConversionStruct tempStruct;
				tempStruct = ms_pExpensiveObjectConversions[i];
				
				formatf(ms_pExpensiveObjectConversions[i].m_objectName, "%s", pObject->GetDebugName());
				ms_pExpensiveObjectConversions[i].m_conversionTime = msTime;

				// Push the rest of the list down
				CObjectPopulation::ObjectConversionStruct tempStruct2;
				for(int j = i+1; j < EXPENSIVE_OBJECT_CONVERSIONS_TO_TRACK; j++)
				{
					tempStruct2 = ms_pExpensiveObjectConversions[j];
										
					ms_pExpensiveObjectConversions[j] = tempStruct;
					
					tempStruct = tempStruct2;
				}
				break;
			}
		}
	}
}

// Clear all of the tracked conversions
void CObjectPopulation::ClearTrackedObjectConversions()
{
	if(ms_pExpensiveObjectConversions)
	{
		// New struct have name="" and time=0
		CObjectPopulation::ObjectConversionStruct newStruct;
		for(int i = 0; i < EXPENSIVE_OBJECT_CONVERSIONS_TO_TRACK; i++)
		{
			ms_pExpensiveObjectConversions[i] = newStruct; // copy this info into every slot
		}
	}
}

void CObjectPopulation::ConvertObjectsAroundPlayerToDummy()
{
	Vector3 vecCentre = CFocusEntityMgr::GetMgr().GetPos();
	ConvertAllObjectsInAreaToDummyObjects(vecCentre, 100.f, true, NULL, false);
}
#endif // __BANK
