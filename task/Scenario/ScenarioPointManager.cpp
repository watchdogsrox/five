
#include "ScenarioPointManager.h"

// Rage includes
#include "bank/msgbox.h"
#include "parser/macros.h"
#include "parser/manager.h"
#include "parser/visitorxml.h"
#include "parsercore/settings.h"
#include "parsercore/stream.h"
#include "profile/profiler.h"
#if SCENARIO_DEBUG
#include "system/exec.h"
#endif // SCENARIO_DEBUG
#include "vector/geometry.h"

// Framework Includes
#include "fwscene/stores/psostore.h"
#include "fwscene/world/WorldLimits.h"
#include "fwsys/fileExts.h"
#include "fwsys/metadatastore.h"
#include "streaming/streamingengine.h"
#include "streaming/packfilemanager.h"

// Game includes
#include "ai/stats.h"
#include "control/gamelogic.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/ModelInfo_Factories.h"
#include "pathserver/ExportCollision.h"
#include "physics/gtaArchetype.h"
#include "scene/DataFileMgr.h"
#include "scene/ExtraContent.h"
#include "scene/scene.h"
#include "scene/loader/MapData_Extensions.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/portals/InteriorInst.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioClustering.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPointRegion.h"
#include "peds/pedpopulation.h"

// Parser
#include "ScenarioPointManager_parser.h"

PARAM(scenarioedit, "Activate extra scenario editor features.");
PARAM(scenariodir, "Override scenario point export directory.");
PARAM(notscenariopointsstreaming, "If present stops the scenarion point system from using streaming for loading of needed scenario points");

AI_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

using namespace AIStats;

#if SCENARIO_DEBUG
int CScenarioPointManager::sm_VersionCheckVersionNumber = 0;
int CScenarioPointManager::m_sWorkingManifestIndex = 0;
#endif	// SCENARIO_DEBUG

bool s_UseNonRegionPointSpatialArray = true;

class ScenarioPointMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::SCENARIO_POINTS_FILE:
			SCENARIOPOINTMGR.Append(file.m_filename,false);
			return true;
		case CDataFileMgr::SCENARIO_POINTS_PSO_FILE:
			SCENARIOPOINTMGR.Append(file.m_filename,true);
			return true;
		case CDataFileMgr::SCENARIO_POINTS_OVERRIDE_FILE:
			SCENARIOPOINTMGR.Reset();
			SCENARIOPOINTMGR.Append(file.m_filename,false);	
#if __BANK
			SCENARIOPOINTMGR.OverrideAdded();
#endif	
			return true;
		case CDataFileMgr::SCENARIO_POINTS_OVERRIDE_PSO_FILE:
			SCENARIOPOINTMGR.Reset();
			SCENARIOPOINTMGR.Append(file.m_filename,true);
#if __BANK
			SCENARIOPOINTMGR.OverrideAdded();
#endif		
			return true;
		default:
			return false;
		}
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::SCENARIO_POINTS_FILE:
			SCENARIOPOINTMGR.Remove(file.m_filename,false);
			return;
		case CDataFileMgr::SCENARIO_POINTS_PSO_FILE:
			SCENARIOPOINTMGR.Remove(file.m_filename,true);
			return;
		case CDataFileMgr::SCENARIO_POINTS_OVERRIDE_FILE:
			SCENARIOPOINTMGR.Load();
		case CDataFileMgr::SCENARIO_POINTS_OVERRIDE_PSO_FILE:
			SCENARIOPOINTMGR.Load();
		default:
			return;		
		}
	}

} g_ScenarioPointsMounter;


CScenarioPointManifest::CScenarioPointManifest()
		: m_VersionNumber(0)
{
}
CScenarioPointManager::CScenarioPointManager() :  m_CachedRegionPtrsValid(false)
												, m_NumScenarioPointsEntity(0)
												, m_NumScenarioPointsWorld(0)
												, m_ExclusivelyEnabledScenarioGroupIndex(-1)
												, m_LastNonRegionPointUpdate(0)
												, m_SpatialArray(NULL)
#if SCENARIO_DEBUG
												, m_ScenarioGroupsWidgetGroup(NULL)
												, m_ManifestNeedSave(false)
#endif // SCENARIO_DEBUG
{
	Init();
}

CScenarioPointManager::~CScenarioPointManager()
{
	Shutdown();
}

void CScenarioPointManager::Init()
{
	taskAssert(!m_NumScenarioPointsEntity && !m_NumScenarioPointsWorld);

	int poolSizeTotal = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ScenarioPoint", 0x14984ee1), CONFIGURED_FROM_FILE);

	// Temp - could remove this once everybody has just ScenarioPoint in the gameconfig file.
	if(poolSizeTotal == CONFIGURED_FROM_FILE)
	{
		int maxScenarioPointsEntity = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ScenarioPointEntity", 0xbd135441), CONFIGURED_FROM_FILE);
		int maxScenarioPointsWorld = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ScenarioPointWorld", 0x2474a15b), CONFIGURED_FROM_FILE);
		poolSizeTotal = maxScenarioPointsEntity + maxScenarioPointsWorld;
	}

	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCENARIO_POINTS_FILE, &g_ScenarioPointsMounter,eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCENARIO_POINTS_OVERRIDE_FILE, &g_ScenarioPointsMounter,eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCENARIO_POINTS_PSO_FILE, &g_ScenarioPointsMounter,eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCENARIO_POINTS_OVERRIDE_PSO_FILE, &g_ScenarioPointsMounter,eDFMI_UnloadFirst);
    CScenarioPoint::InitPool(poolSizeTotal, MEMBUCKET_GAMEPLAY);
	CScenarioPointChainUseInfo::InitPool( MEMBUCKET_GAMEPLAY );
	CScenarioPointExtraData::InitPool(MEMBUCKET_GAMEPLAY);
	CScenarioClusterSpawnedTrackingData::InitPool(MEMBUCKET_GAMEPLAY);
	CSPClusterFSMWrapper::InitPool(MEMBUCKET_GAMEPLAY);
	const int numExtra = CScenarioPointExtraData::GetPool()->GetSize();
	Assert(!m_ExtraScenarioData.GetCount());
	Assert(!m_ExtraScenarioDataPoints.GetCount());
	m_ExtraScenarioData.Reset();
	m_ExtraScenarioDataPoints.Reset();
	m_ExtraScenarioData.Reserve(numExtra);
	m_ExtraScenarioDataPoints.Reserve(numExtra);

	Assert(!m_CachedRegionPtrs.GetCount());
	Assert(!m_CachedActiveRegionPtrs.GetCount());
	m_CachedRegionPtrsValid = false;

	m_dirtyStreamerTrees = false;

	//THIS NUMBER MUST BE A FACTOR OF 4 so the storage array can be 16 byte aligned.
	const u32 max_num_spatial_objects = (u32)(fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("MaxNonRegionScenarioPointSpatialObjects", 0x8322badd), CONFIGURED_FROM_FILE) + 3) & ~0x3;;
	Assert(!m_SpatialArray);
	char* spatialArrayStorage = rage_aligned_new(16) char[CSpatialArray::kSizePerNode * max_num_spatial_objects];
	m_SpatialArray = rage_new CSpatialArray(spatialArrayStorage, max_num_spatial_objects);
	m_SpatialNodeList.Reserve(max_num_spatial_objects);

	//////////////////////////////////////////////////////////////////////////
	//When doing in-place loading no constructors are called for the objects which results in non of the "padding" data being inited to 
	// default values ... adding this delegate will allow us to init the data.
	if (CScenarioEntityOverride::parser_GetStaticStructure())
	{
		atDelegate< void (void*) > PostPsoPlaceDelegate(CScenarioEntityOverride::PostPsoPlace);
		CScenarioEntityOverride::parser_GetStaticStructure()->AddDelegate("PostPsoPlace", PostPsoPlaceDelegate);
	}

	if (CScenarioPointContainer::parser_GetStaticStructure())
	{
		atDelegate< void (void*) > PostPsoPlaceDelegate(CScenarioPointContainer::PostPsoPlace);
		CScenarioPointContainer::parser_GetStaticStructure()->AddDelegate("PostPsoPlace", PostPsoPlaceDelegate);
	}

	if (CScenarioPoint::parser_GetStaticStructure())
	{
		atDelegate< void (void*) > PostPsoPlaceDelegate(CScenarioPoint::PostPsoPlace);
		CScenarioPoint::parser_GetStaticStructure()->AddDelegate("PostPsoPlace", PostPsoPlaceDelegate);
	}

	if (CScenarioChain::parser_GetStaticStructure())
	{
		atDelegate< void (void*) > PostPsoPlaceDelegate(CScenarioChain::PostPsoPlace);
		CScenarioChain::parser_GetStaticStructure()->AddDelegate("PostPsoPlace", PostPsoPlaceDelegate);
	}

	if (CScenarioChainingGraph::parser_GetStaticStructure())
	{
		atDelegate< void (void*) > PostPsoPlaceDelegate(CScenarioChainingGraph::PostPsoPlace);
		CScenarioChainingGraph::parser_GetStaticStructure()->AddDelegate("PostPsoPlace", PostPsoPlaceDelegate);
	}

	//When doing in-place loading no destructors adding this delegate will allow us to cleanup the data.
	if (CScenarioPointRegion::parser_GetStaticStructure())
	{
		CScenarioPointRegion* dummy = NULL;
		atDelegate< void (void)> RemoveFromStoreDelegate(dummy, &CScenarioPointRegion::RemoveFromStore);
		CScenarioPointRegion::parser_GetStaticStructure()->AddDelegate("RemoveFromStore", RemoveFromStoreDelegate);
	}

	//Since the PSO system allows partial in-place loading on a per structure basis saving/editing the region data
	// causes a crash as the accel grid is cleared and then rebuilt ... so if we want to do editing then
	// disable the fast loading ... ONLY IN BANK :)
#if SCENARIO_DEBUG
	if(PARAM_scenarioedit.Get())
	{
		if (CScenarioPointRegion::parser_GetStaticStructure())
		{
			g_fwMetaDataStore.SetAllowLoadInResourceMem(false);
			CScenarioPointRegion::parser_GetStaticStructure()->GetFlags().Set(parStructure::NOT_IN_PLACE_LOADABLE);
		}
	}
	m_ManifestOverrideIndex = 0;
#endif // SCENARIO_DEBUG
	//////////////////////////////////////////////////////////////////////////
}

void CScenarioPointManager::InitSession(u32 initMode)
{
	SCENARIOPOINTMGR.m_LastNonRegionPointUpdate = 0;
	if(initMode==INIT_SESSION)
	{
		if(SCENARIOPOINTMGR.m_dirtyStreamerTrees)
		{
#if SCRATCH_ALLOCATOR
			sysMemManager::GetInstance().GetScratchAllocator()->Reset();
#endif 

			g_fwMetaDataStore.GetBoxStreamer().PostProcess();
			SCENARIOPOINTMGR.m_dirtyStreamerTrees = false;
		}
	}
}

void CScenarioPointManager::ShutdownSession(u32 UNUSED_PARAM(initMode))
{
#if SCENARIO_DEBUG
// 	for (int i=0;i<SCENARIOPOINTMGR.m_dDLCSources.GetCount();i++)
// 	{
// 		CScenarioPointManifest& man = SCENARIOPOINTMGR.m_dDLCSources[i].manifest;
// 		man.m_RegionDefs = atArray<CScenarioPointRegionDef*>();
// 		man.m_Groups = atArray<CScenarioPointGroup*>();
// 		man.m_InteriorNames = atArray<atHashWithStringBank>();
// 	}
// 	SCENARIOPOINTMGR.m_dDLCSources.Reset();
#endif // SCENARIO_DEBUG
	Displayf("shutdown!");
}

// Helper function for shutting down the array, avoids code duplication
/* static s32 ShutdownScenarioPointArray(atArray<CScenarioPoint *>& scenarioPoints, s32 count) 
{
	for(s32 i = 0; i < count; i++)
	{
		delete scenarioPoints[i];
	}
	scenarioPoints.Reset();
	return 0;
} */

void CScenarioPointManager::Shutdown()
{
    //Delete any left over scenario points ... 
    // SHOULD THIS ASSERT if leftovers after world points deleted?
    s32 count = m_EntityPoints.GetCount();
    for(s32 i = 0; i < count; i++)
    {
		CScenarioPoint::DestroyFromPool(*m_EntityPoints[i]);
    }
    m_EntityPoints.Reset();

	m_NumScenarioPointsEntity = 0;
	m_NumScenarioPointsWorld = 0;

	// These arrays should have been cleared out already, since they are associated
	// with CScenarioPoint objects that should have been destroyed by now.
	Assert(m_ExtraScenarioData.GetCount() == 0);
	Assert(m_ExtraScenarioDataPoints.GetCount() == 0);
	m_ExtraScenarioData.Reset();
	m_ExtraScenarioDataPoints.Reset();

	m_CachedActiveRegionPtrs.Reset();
	m_CachedRegionPtrs.Reset();
	m_CachedRegionPtrsValid = false;

	delete m_SpatialArray;
	m_SpatialArray = NULL;

	CScenarioPointExtraData::ShutdownPool();
	CScenarioPointChainUseInfo::ShutdownPool();
    CScenarioPoint::ShutdownPool();	
	CScenarioClusterSpawnedTrackingData::ShutdownPool();
	CSPClusterFSMWrapper::ShutdownPool();
}

void CScenarioPointManager::DeletePointsWithAttachedEntity(CEntity* attachedEntity)
{
    Assert(attachedEntity);

    for(s32 i = 0; i < m_EntityPoints.GetCount(); i++)
    {
		CScenarioPoint* pt = m_EntityPoints[i];
        Assert(pt);
        if (pt->GetEntity() == attachedEntity)
        {
			RemoveEntityPointAtIndex(i);
			CScenarioPoint::DestroyFromPool(*pt);
            i--; //step back.

			// Note: it should not be necessary to go through and update m_NodeTo2dEffectMapping[]
			// here. Since we use RegdScenarioPnt now, any entry pointing to the deleted CScenarioPoint
			// should have been cleared out already, which is all that needs to be done. If we wanted
			// to save on reference objects, we could use straight pointers and loop through the
			// array here, though.
        }
    }

	taskAssert((m_NumScenarioPointsEntity + m_NumScenarioPointsWorld) == (m_EntityPoints.GetCount() + m_WorldPoints.GetCount()));
}

void CScenarioPointManager::DeletePointsWithAttachedEntities(const CEntity* const* attachedEntities, u32 numEntities)
{
	for(u32 start = 0; start < numEntities; start += 8)
	{
		for(s32 i = 0; i < m_EntityPoints.GetCount(); i++)
		{
			CScenarioPoint* pt = m_EntityPoints[i];
			Assert(pt);

			const CEntity* pEntity = pt->GetEntity();

			if(    (							 pEntity == attachedEntities[start])
				|| (((start+1) < numEntities) && pEntity == attachedEntities[start+1])
				|| (((start+2) < numEntities) && pEntity == attachedEntities[start+2])
				|| (((start+3) < numEntities) && pEntity == attachedEntities[start+3])
				|| (((start+4) < numEntities) && pEntity == attachedEntities[start+4])
				|| (((start+5) < numEntities) && pEntity == attachedEntities[start+5])
				|| (((start+6) < numEntities) && pEntity == attachedEntities[start+6])
				|| (((start+7) < numEntities) && pEntity == attachedEntities[start+7]))
			{
				RemoveEntityPointAtIndex(i);
				CScenarioPoint::DestroyFromPool(*pt);//delete the scenario point ... 
				i--; //step back.

				// Note: it should not be necessary to go through and update m_NodeTo2dEffectMapping[]
				// here. Since we use RegdScenarioPnt now, any entry pointing to the deleted CScenarioPoint
				// should have been cleared out already, which is all that needs to be done. If we wanted
				// to save on reference objects, we could use straight pointers and loop through the
				// array here, though.
			}
		}
	}

	taskAssert((m_NumScenarioPointsEntity + m_NumScenarioPointsWorld) == (m_EntityPoints.GetCount() + m_WorldPoints.GetCount()));
}

bool CScenarioPointManager::ApplyOverridesForEntity(CEntity& entity)
{
	PF_FUNC(ApplyOverridesForEntity);

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
		return false;
#endif

	const int activeRegionCnt = GetNumActiveRegions();
	for(int i = 0; i < activeRegionCnt; i++)
	{
		CScenarioPointRegion& region = GetActiveRegion(i);

		// Only apply the overrides for regions that processed this entity already
		if(!CScenarioEntityOverridePostLoadUpdateQueue::GetInstance().IsEntityPending(region, entity))
		{
			if(region.ApplyOverridesForEntity(entity, false, false))
			{
				return true;
			}
		}
	}

	return false;
}

s32 CScenarioPointManager::GetNumEntityPoints() const
{
	return m_NumScenarioPointsEntity;
}

CScenarioPoint* CScenarioPointManager::FindClosestPointWithAttachedEntity(CEntity* entity, Vec3V_In position, int& scenarioTypeRealOut, CPed* pPedUser)
{
	scenarioTypeRealOut = -1;

	//NOTE: Only need to check entity point array because the region/world points don't have entities.
	ScalarV curD2(V_ZERO);
	CScenarioPoint* found = NULL;
	s32 count = m_EntityPoints.GetCount();
	for(s32 i = 0; i < count; i++)
	{
		Assert(m_EntityPoints[i] && m_EntityPoints[i]->GetEntity());
		if (m_EntityPoints[i]->GetEntity() != entity) 
		{
			continue;
		}

		Vec3V_In spPos = m_EntityPoints[i]->GetPosition();
		ScalarV d2 = DistSquared(position, spPos);
		if (!found || IsLessThanAll(d2, curD2))
		{
			found = m_EntityPoints[i];
			curD2 = d2;
		}
	}

	if(found)
	{
		scenarioTypeRealOut = CScenarioManager::ChooseRealScenario(found->GetScenarioTypeVirtualOrReal(), pPedUser);
	}

	return found;
}

u32	CScenarioPointManager::FindUnusedPointsWithAttachedEntity(CEntity* entity, CScenarioPoint** destArray, u32 maxInDestArray)
{
	Assert(destArray);

	//NOTE: Only need to check entity point array because the region/world points don't have entities.
	s32 count = m_EntityPoints.GetCount();
	s32 iNumAdded = 0;
	for(s32 i = 0; i < count && iNumAdded < maxInDestArray; i++)
	{
		Assert(m_EntityPoints[i] && m_EntityPoints[i]->GetEntity());
		if (m_EntityPoints[i]->GetEntity() != entity) 
		{
			continue;
		}
		if (m_EntityPoints[i]->GetUses() > 0)
		{
			continue;
		}
		
		destArray[iNumAdded++] = m_EntityPoints[i];
	}

	return iNumAdded;
}


void CScenarioPointManager::AddScenarioPoint(CScenarioPoint& spoint, bool callOnAdd) 
{
	if (spoint.GetEntity()) 
	{
		AddEntityPoint(spoint, callOnAdd);
	} 
	else 
	{
		AddWorldPoint(spoint, callOnAdd);
	}
}

void CScenarioPointManager::AddEntityPoint(CScenarioPoint& spoint, bool callOnAdd)
{
	Assertf(spoint.GetEntity(), "ScenarioPoint passed to AddEntityPoint()  must have an attached Entity!");
	Assertf(CScenarioPoint::GetPool()->IsValidPtr(&spoint), "Scenario point passed to AddEntityPoint() should come from the pool.");
    m_EntityPoints.PushAndGrow(&spoint);

	const int numActiveRegions = GetNumActiveRegions();
	for (int j = 0; j < numActiveRegions; j++)
	{
		CScenarioPointRegion& region = GetActiveRegion(j);
		region.UpdateChainedNodeToScenarioMappingForAddedPoint(spoint);
	}
	
	m_NumScenarioPointsEntity++;
	if(callOnAdd)
	{
		CScenarioManager::OnAddScenario(spoint);
	}
	taskAssert((m_NumScenarioPointsEntity + m_NumScenarioPointsWorld) == (m_EntityPoints.GetCount() + m_WorldPoints.GetCount()));

	// Add to the spatial array, so we don't have to wait for the next call to
	// (and internal delay in) UpdateNonRegionPointSpatialArray(). Otherwise, scripts
	// may not be able to find the scenario points that they need.
	const int cnt = m_SpatialNodeList.GetCount();
	if(cnt < m_SpatialNodeList.GetCapacity())
	{
		AddToSpatialArray(cnt, cnt, spoint);
	}
}

void CScenarioPointManager::AddWorldPoint(CScenarioPoint& spoint, bool callOnAdd) 
{
	Assertf(spoint.GetEntity() == NULL, "ScenarioPoint passed to AddEntityPoint()  must NOT have an attached Entity!");
	m_WorldPoints.PushAndGrow(&spoint);
	m_NumScenarioPointsWorld++;
	if(callOnAdd)
	{
		CScenarioManager::OnAddScenario(spoint);
	}
	taskAssert((m_NumScenarioPointsEntity + m_NumScenarioPointsWorld) == (m_EntityPoints.GetCount() + m_WorldPoints.GetCount()));
}

bool CScenarioPointManager::RemoveScenarioPoint(CScenarioPoint& spoint) 
{
	return (spoint.GetEntity()) ? RemoveEntityPoint(spoint) : RemoveWorldPoint(spoint);
}

bool CScenarioPointManager::RemoveEntityPoint(CScenarioPoint& spoint) 
{
	Assertf(spoint.GetEntity(), "ScenarioPoint passed to RemoveEntityPoint()  must have an attached Entity!");
	for (int i = 0, count = m_EntityPoints.GetCount(); i < count; ++i) {
		if (m_EntityPoints[i] == &spoint) 
		{
			RemoveEntityPointAtIndex(i);
			return true;
		}
	}
	return false;
}

bool CScenarioPointManager::RemoveWorldPoint(CScenarioPoint& spoint) {
	Assertf(spoint.GetEntity() == NULL, "ScenarioPoint passed to RemoveWorldPoint()  must NOT have an attached Entity!");
	for (int i = 0, count = m_WorldPoints.GetCount(); i < count; ++i) {
		if (m_WorldPoints[i] == &spoint) 
		{
			RemoveWorldPointAtIndex(i);
			return true;
		}
	}
	return false;
}

//NOTE: Function is not thread safe ... careful when using in render thread.
u32 CScenarioPointManager::FindPointsInArea(Vec3V_In centerPos, ScalarV_In radius, CScenarioPoint** destArray, u32 maxInDestArray,
		ScalarV_In radiusForExtendedRangeScenarios, bool includeClusteredPoints)
{
	PF_FUNC(FindPointsInArea);

    Assert(destArray);
	ScalarV trueRadiusForExtendedRangeScenarios = Max(radius, radiusForExtendedRangeScenarios);

	u32 curWanted = 0;

	spdSphere sphere(centerPos, radius);
	spdSphere exsphere(centerPos, trueRadiusForExtendedRangeScenarios);

	//Check the regions ... first ... 
	// Note: can't use GetNumActiveRegions() here, since then we wouldn't have the AABB. We could just
	// store the indices of the active regions in the array instead, but then we would have to do an additional
	// lookup in all other places where GetActiveRegion() is used.
	{
		PF_FUNC(FindPointsInArea_Region);
		const int regionCnt = GetNumRegions();
		for (int r = 0; r < regionCnt; r++)
		{
			CScenarioPointRegion* region = GetRegionFast(r);
			if (region) //perhaps it is streamed out ... 
			{
				spdAABB& aabb = m_Manifest.m_RegionDefs[r]->m_AABB;
				if (aabb.IntersectsSphere(exsphere))
				{
					const int numFoundInRegion = region->FindPointsInArea(sphere, exsphere, destArray + curWanted, maxInDestArray - curWanted, includeClusteredPoints);
					curWanted += numFoundInRegion;
				}
			}
		}
	}

	{
		PF_FUNC(FindPointsInArea_Entity);

		if (m_SpatialArray && !m_SpatialArray->IsEmpty() && s_UseNonRegionPointSpatialArray)
		{ 
			static const u32 MAX_RESULTS = 384;
			CSpatialArrayNode* results[MAX_RESULTS];

			//Find the ones that are extended range.
			u32 flag = CSpatialArrayNode_SP::kSpatialArrayTypeFlagExtendedRange;
			int numFound = m_SpatialArray->FindInSphereOfType(centerPos, trueRadiusForExtendedRangeScenarios, results, MAX_RESULTS, flag, flag);
			
			//Find the ones that are not extended range.
			numFound += m_SpatialArray->FindInSphereOfType(centerPos, radius, results+numFound, MAX_RESULTS-numFound, flag, 0);

			taskAssertf(numFound < MAX_RESULTS, "CScenarioPointManager::FindScenarioPointsInArea: Found at least %d non region points in your search area. Range was %.1f centered at (%.1f, %.1f, %.1f)",
				MAX_RESULTS, radius.Getf(), centerPos.GetXf(), centerPos.GetYf(), centerPos.GetZf());
			
			for(int i = 0; i < numFound; i++)
			{
				CScenarioPoint* pt = CSpatialArrayNode_SP::GetScenarioPointFromSpatialNode(*(results[i]));
				if(pt)
				{
					if (curWanted < maxInDestArray)
					{
						destArray[curWanted] = pt;
						curWanted++;
					}
					else
					{
						break; //... no more space for points so get out.
					}
				}
			}			
		}
	}
		// Fail an assert if we filled up. We don't know for sure that we had to drop anything, but it's very likely in practice.
		// MK: don't assert during a player switch
		taskAssertf(g_PlayerSwitch.IsActive() || curWanted < maxInDestArray, "CScenarioPointManager::FindScenarioPointsInArea: Found at least %d points in your search area. Range was %.1f centered at (%.1f, %.1f, %.1f)",
			maxInDestArray, radius.Getf(), centerPos.GetXf(), centerPos.GetYf(), centerPos.GetZf());

    return curWanted;
}

u32 CScenarioPointManager::FindPointsOfTypeForEntity(unsigned scenarioType, CEntity* attachedEntity, CScenarioPoint** destArray, u32 maxInDestArray)
{
	Assert(attachedEntity);
	Assert(destArray);

	u32 curFound = 0;
	u32 totalFound = 0;

	//NOTE: only need to check the entity points cause regions don't have attached entity points
	s32 count = m_EntityPoints.GetCount();
	for(s32 i = 0; i < count; i++)
	{
		CScenarioPoint* pt = m_EntityPoints[i];
		Assert(pt);
		Assertf(pt->GetEntity(), "CScenarioPoint added to the wrong list!");
		if (pt->GetEntity() == attachedEntity)
		{
			if (pt->GetScenarioTypeVirtualOrReal() == scenarioType)
			{
				if (curFound < maxInDestArray)
				{
					destArray[curFound] = pt;
					curFound++;
				}
				totalFound++;
			}
		}
	}

	taskAssertf(totalFound <= maxInDestArray, "CScenarioPointManager::FindPointsOfTypeForEntity: Found %d points but the expected maximum was %d for entity '%s'",
		totalFound, maxInDestArray, attachedEntity->GetModelName());

	return curFound;
}

u32 CScenarioPointManager::FindPointsInGroup(unsigned groupId, CScenarioPoint **destArray, u32 maxInDestArray)
{
	u32 uNumFound = 0;
	if (groupId != CScenarioPointManager::kNoGroup)
	{
		// do a linear search over all points and find ones belonging to our group
		// push them into one array and select a random index in that array
		u32 i, count;
		// add world scenario points with the given group type to our array
		for (i = 0, count = m_NumScenarioPointsWorld; i < count && uNumFound < maxInDestArray; ++i)
		{
			if (m_WorldPoints[i]->GetScenarioGroup() == groupId)
			{
				destArray[uNumFound++] = m_WorldPoints[i];
			}
		}
		// add entity scenario points with the given group type to our array
		for (i = 0, count = m_NumScenarioPointsEntity; i < count && uNumFound < maxInDestArray; ++i)
		{
			if (m_EntityPoints[i]->GetScenarioGroup() == groupId)
			{
				destArray[uNumFound++] = m_EntityPoints[i];
			}
		}
		// add region scenario points with the given group type to our array
		for (i = 0, count = GetNumRegions(); i < count && uNumFound < maxInDestArray; ++i)
		{
			CScenarioPointRegion *pRegion = GetRegion(i);
			if (pRegion)
			{
				for (u32 j = 0, countInRegion = pRegion->GetNumPoints(); j < countInRegion && uNumFound < maxInDestArray; ++j)
				{
					CScenarioPoint &curPt = pRegion->GetPoint(j);
					if (curPt.GetScenarioGroup() == groupId)
					{
						destArray[uNumFound++] = &curPt;
					}
				}
			}
		}
	}
	Assertf(uNumFound <= maxInDestArray, "Max capacity of the destination array has been reached!  Consider increasing the size of the destination buffer from its current size of: %u", maxInDestArray);
	return uNumFound;
}

static void OnAddScenarioPointArray(atArray<CScenarioPoint *>& scenarioPoints, s32 count) {
	for (int p = 0; p < count; p++ )
	{
		Assert(scenarioPoints[p]);
		CScenarioManager::OnAddScenario(*(scenarioPoints[p]));
	}
}

//For all points that are currently loaded ... do the onAddcall... 
// NOTE: this should only be called by CScenarioManager::Init
//		to properly setup currently loaded points
void CScenarioPointManager::HandleInitOnAddCalls()
{
	//For all loaded regions
	const int activeRegionCount = GetNumActiveRegions();
	for (int i = 0; i < activeRegionCount; i++ )
	{
		CScenarioPointRegion& region = GetActiveRegion(i);
		region.CallOnAddScenarioForPoints();
	}
	
	OnAddScenarioPointArray(m_EntityPoints, m_EntityPoints.GetCount());
	OnAddScenarioPointArray(m_WorldPoints, m_WorldPoints.GetCount());
}

//For all points that are currently loaded ... do the onRemovecall... 
// NOTE: this should only be called by CScenarioManager::Shutdown
//		to properly clean up currently loaded points
void CScenarioPointManager::HandleShutdownOnRemoveCalls()
{
	//For all loaded regions
	const int activeRegionCount = GetNumActiveRegions();
	for (int i = 0; i < activeRegionCount; i++ )
	{
		CScenarioPointRegion& region = GetActiveRegion(i);
		region.CallOnRemoveScenarioForPoints();
	}

	//For all attached points
	const int pointCount = m_EntityPoints.GetCount();
	for (int p = 0; p < pointCount; p++ )
	{
		Assert(m_EntityPoints[p]);
		CScenarioManager::OnRemoveScenario(*m_EntityPoints[p]);
	}
}

#if SCENARIO_DEBUG
int SortAlphabeticalNoCase(CScenarioPointRegionDef* const * p_A, CScenarioPointRegionDef* const* p_B)
{
	return stricmp((*p_A)->m_Name.GetCStr(), (*p_B)->m_Name.GetCStr());
}
int SortAlphabeticalNoCase(CScenarioPointGroup* const* p_A, CScenarioPointGroup* const* p_B)
{
	return stricmp((*p_A)->GetName(), (*p_B)->GetName());
}
int SortAlphabeticalNoCase(const atHashWithStringBank* p_A, const atHashWithStringBank* p_B)
{
	return stricmp(p_A->GetCStr(), p_B->GetCStr());
}
bkCombo* CScenarioPointManager::AddWorkingManifestCombo(bkBank* bank)
{
	bkCombo* ret = bank->AddCombo("Current working manifest", &m_sWorkingManifestIndex, m_manifestSources.GetCount(),m_manifestSources.GetElements());/* datCallback(MFA(CScenarioPointManager::WorkingManifestChanged),this));*/
	return ret;
}
void CScenarioPointManager::WorkingManifestChanged()
{
	Reset();
	Append(m_manifestSources[m_sWorkingManifestIndex],m_manifestSourceInfos[m_sWorkingManifestIndex]);
}
#endif // SCENARIO_DEBUG

void CScenarioPointManager::RegisterRegion(CScenarioPointRegionDef* rDef)
{
	char PlatformFilename[RAGE_MAX_PATH];
	char ExpandedName[RAGE_MAX_PATH] = {0};
	DATAFILEMGR.ExpandFilename(rDef->m_Name.GetCStr(),ExpandedName,RAGE_MAX_PATH);
	formatf(PlatformFilename,"%s.%s", ExpandedName, META_FILE_EXT);
	strPackfileManager::RegisterIndividualFile(PlatformFilename);
	const fwAssetDef<fwMetaDataFile>::strObjectNameType RegionNameHash(ExpandedName);
	rDef->m_SlotId = g_fwMetaDataStore.MakeSlotStreamable(RegionNameHash, rDef->m_AABB);
}

void CScenarioPointManager::Append(const char* filename,bool isPso)
{
	CScenarioPointManifest tempManifest;
	bool loaded=false;
	if(isPso)
	{
		loaded=LoadPso(filename,tempManifest);
	}
	else
	{
		loaded=LoadXml(filename,tempManifest);		
	}
	if (loaded)
	{
		for (int i = 0; i < tempManifest.m_RegionDefs.GetCount(); i++)
		{
			CScenarioPointRegionDef* rDef = tempManifest.m_RegionDefs[i];
			RegisterRegion(rDef);
			m_Manifest.m_RegionDefs.PushAndGrow(rDef);
		}
		for (int i = 0; i < tempManifest.m_Groups.GetCount(); i++)
		{
			m_Manifest.m_Groups.PushAndGrow(tempManifest.m_Groups[i]);
		}
		for (int i = 0; i < tempManifest.m_InteriorNames.GetCount(); i++)
		{
			m_Manifest.m_InteriorNames.PushAndGrow(tempManifest.m_InteriorNames[i]);
		}
	}

	PostLoad();
}

void CScenarioPointManager::Remove(const char* fileName, bool isPso)
{
	CScenarioPointManifest tempManifest;
	bool loaded=false;
	if(isPso)
		loaded=LoadPso(fileName,tempManifest);
	else
		loaded=LoadXml(fileName,tempManifest);
	if (loaded)
	{
		for(int i=0;i<tempManifest.m_RegionDefs.GetCount();i++)
		{
			CScenarioPointRegionDef* rDef = tempManifest.m_RegionDefs[i];
			for (int j=0;j<m_Manifest.m_RegionDefs.GetCount();j++)
			{
				CScenarioPointRegionDef* pDef = m_Manifest.m_RegionDefs[j];
				if(rDef->m_Name==pDef->m_Name)
				{
					InvalidateRegion(pDef);
#if SCENARIO_DEBUG
					m_regionSourceMap.Remove(m_regionSourceMap.GetIndexFromDataPtr(m_regionSourceMap.Access(pDef->m_Name)));
#endif // SCENARIO_DEBUG
					delete pDef;
					pDef=NULL;
					m_Manifest.m_RegionDefs.Delete(j--);
					break;
				}
			}
			delete rDef;
			rDef=NULL;
		}
		for(int i=0;i<tempManifest.m_Groups.GetCount();i++)
		{
			CScenarioPointGroup* rDef = tempManifest.m_Groups[i];
			for (int j=0;j<m_Manifest.m_Groups.GetCount();j++)
			{
				CScenarioPointGroup* pDef = m_Manifest.m_Groups[j];
				if(rDef->GetNameHashString()==pDef->GetNameHashString())
				{
#if SCENARIO_DEBUG
					m_groupSourceMap.Remove(m_groupSourceMap.GetIndexFromDataPtr(m_groupSourceMap.Access(pDef->GetNameHashString())));
#endif // SCENARIO_DEBUG
					delete pDef;
					pDef=NULL;
					m_Manifest.m_Groups.Delete(j--);
					break;
				}
			}
			delete rDef;
			rDef=NULL;
		}
		for(int i=0;i<tempManifest.m_InteriorNames.GetCount();i++)
		{
			for (int j=0;j<m_Manifest.m_InteriorNames.GetCount();j++)
			{
				if(tempManifest.m_InteriorNames[i]==m_Manifest.m_InteriorNames[j])
				{
#if SCENARIO_DEBUG
					m_interiorSourceMap.Remove(m_interiorSourceMap.GetIndexFromDataPtr(m_interiorSourceMap.Access(m_Manifest.m_InteriorNames[j])));
#endif // SCENARIO_DEBUG
					m_Manifest.m_InteriorNames.Delete(j--);
					break;
				}
			}
		}
	}
	m_CachedActiveRegionPtrs.Reset();
	m_CachedRegionPtrs.Reset();
	m_CachedRegionPtrsValid = false;
	InitCachedRegionPtrs();
	UpdateCachedRegionPtrs();
	m_dirtyStreamerTrees = true;
	PostLoad();
}

#if SCENARIO_DEBUG

void CScenarioPointManager::HandleFileSources(const char* fileToLoad,bool isPso)
{
	bool bFound = false;
	for (int i=0;i<m_manifestSources.GetCount();i++)
	{
		if(stricmp(m_manifestSources[i],fileToLoad) == 0)
		{
			bFound = true;
			break;
		}
	}
	if(!bFound)
	{
		char* newSource = rage_new char[RAGE_MAX_PATH];
		strcpy(newSource,fileToLoad);
		m_manifestSources.PushAndGrow(newSource);
		m_manifestSourceInfos.PushAndGrow(isPso);
	}
}

void CScenarioPointManager::BankLoad()
{
	Reset();
	for(int i=m_ManifestOverrideIndex;i<m_manifestSourceInfos.GetCount();i++)
	{
		Append(m_manifestSources[i],m_manifestSourceInfos[i]);
	}
}
void CScenarioPointManager::PopulateSourceData(const char* source, CScenarioPointManifest& manifest)
{
	atHashString fileHash(source);
	for(int i=0;i<manifest.m_RegionDefs.GetCount();i++)
	{
		if(!m_regionSourceMap.Has(manifest.m_RegionDefs[i]->m_Name))
		{
			m_regionSourceMap.Insert(manifest.m_RegionDefs[i]->m_Name,fileHash);
		}
	}
	m_regionSourceMap.FinishInsertion();
	for(int i=0;i<manifest.m_Groups.GetCount();i++)
	{
		if(!m_groupSourceMap.Has(manifest.m_Groups[i]->GetNameHashString()))
		{
			m_groupSourceMap.Insert(manifest.m_Groups[i]->GetNameHashString(),fileHash);
		}
	}
	m_groupSourceMap.FinishInsertion();
	for(int i=0;i<manifest.m_InteriorNames.GetCount();i++)
	{
		if(!m_interiorSourceMap.Has(manifest.m_InteriorNames[i]))
		{
			m_interiorSourceMap.Insert(manifest.m_InteriorNames[i],fileHash);
		}		
	}
	m_interiorSourceMap.FinishInsertion();
}

#endif // SCENARIO_DEBUG
void CScenarioPointManager::InvalidateRegion(CScenarioPointRegionDef* rDef)
{
	if(rDef&&rDef->m_SlotId != -1)
	{
		//if it was edited then we need to remove the extra ref.
		if (g_fwMetaDataStore.GetBoxStreamer().GetIsPinnedByUser(rDef->m_SlotId))
		{
			g_fwMetaDataStore.GetBoxStreamer().SetIsPinnedByUser(rDef->m_SlotId, false);
			g_fwMetaDataStore.RemoveRef(strLocalIndex(rDef->m_SlotId), REF_OTHER);
		}
		g_fwMetaDataStore.GetBoxStreamer().SetIsIgnored(rDef->m_SlotId, true);
		g_fwMetaDataStore.RemoveLoaded(strLocalIndex(rDef->m_SlotId));
		char PlatformFilename[RAGE_MAX_PATH];
		char ExpandedName[RAGE_MAX_PATH] = {0};
		DATAFILEMGR.ExpandFilename(rDef->m_Name.GetCStr(),ExpandedName,RAGE_MAX_PATH);
		formatf(PlatformFilename,"%s.%s", ExpandedName, META_FILE_EXT);
		strPackfileManager::InvalidateIndividualFile(PlatformFilename);
		strStreamingEngine::GetInfo().UnregisterObject(g_fwMetaDataStore.GetStreamingIndex(strLocalIndex(rDef->m_SlotId)));
		rDef->m_SlotId=-1;
	}
}

void CScenarioPointManager::Reset()
{
#if SCENARIO_DEBUG
	PF_START_STARTUPBAR("Streaming out old data");
	m_ManifestNeedSave = false;//Manifest needs saving
#endif // SCENARIO_DEBUG
	//Stream out any previous data ... 
	if (m_Manifest.m_RegionDefs.GetCount())
	{
		for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
		{
			InvalidateRegion(m_Manifest.m_RegionDefs[i]);
			delete m_Manifest.m_RegionDefs[i];
			m_Manifest.m_RegionDefs[i]=NULL;
		}
		for(int i=0;i<m_Manifest.m_Groups.GetCount();i++)
		{
			delete m_Manifest.m_Groups[i];
			m_Manifest.m_Groups[i]=NULL;
		}
		m_Manifest.m_RegionDefs.Reset();
		m_Manifest.m_Groups.Reset();
		m_Manifest.m_InteriorNames.Reset();
#if SCENARIO_DEBUG
		m_regionSourceMap.Reset();
		m_groupSourceMap.Reset();
		m_interiorSourceMap.Reset();
#endif // SCENARIO_DEBUG
	}	
	m_CachedActiveRegionPtrs.Reset();
	m_CachedRegionPtrs.Reset();
	m_CachedRegionPtrsValid = false;
}
bool CScenarioPointManager::LoadXml(const char* filename, CScenarioPointManifest& instance)
{	
	if(ASSET.Exists(filename,""))
	{
		if (Verifyf(PARSER.LoadObject(filename, "", instance), "Unable to load file %s", filename))
		{
#if SCENARIO_DEBUG
			HandleFileSources(filename,false);
			PopulateSourceData(filename, instance);
#endif // SCENARIO_DEBUG
			return true;
		}
	}
	return false;
}
bool CScenarioPointManager::LoadPso(const char* filename, CScenarioPointManifest& instance)
{
	// Make sure we have the right extension (it's listed as .#mt, but doesn't
	// seem to get automatically patched up to match the current platform).
	char buff[RAGE_MAX_PATH];
	fiAssetManager::RemoveExtensionFromPath(buff, sizeof(buff), filename);
	char fileToLoad[RAGE_MAX_PATH];
	formatf(fileToLoad, "%s.%s", buff, META_FILE_EXT);
	if(ASSET.Exists(fileToLoad,META_FILE_EXT))
	{
		fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CScenarioPointManifest>();
		loader.GetFlagsRef().Set(fwPsoStoreLoader::RUN_POSTLOAD_CALLBACKS);
		fwPsoStoreLoadInstance inst(&instance);
		loader.Load(fileToLoad, inst);
		if(Verifyf(inst.IsLoaded(), "Unable to load file %s", fileToLoad))
		{
#if SCENARIO_DEBUG
			HandleFileSources(filename,true);
			PopulateSourceData(filename, instance);
#endif // SCENARIO_DEBUG
			return true;
		}
	}
	return false;
}

void CScenarioPointManager::Load()
{
	Reset();
#if SCENARIO_DEBUG
	m_ManifestOverrideIndex = 0;
#endif
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::SCENARIO_POINTS_PSO_FILE);
	bool loaded = false;
	if(DATAFILEMGR.IsValid(pData))
	{
		loaded = LoadPso(pData->m_filename,m_Manifest);
	}
	if(!loaded)
	{
		pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::SCENARIO_POINTS_FILE);
		if(DATAFILEMGR.IsValid(pData))
		{
			loaded = LoadXml(pData->m_filename,m_Manifest);
		}
	}
	if(loaded)
	{
		PF_START_STARTUPBAR("After parsing");
		for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
		{
			RegisterRegion(m_Manifest.m_RegionDefs[i]);
		}
	}

	PF_START_STARTUPBAR("Post Load");
	PostLoad();

}

void CScenarioPointManager::PostLoad()
{
#if SCENARIO_DEBUG
	m_Manifest.m_RegionDefs.QSort(0, -1, SortAlphabeticalNoCase);
	m_Manifest.m_Groups.QSort(0, -1, SortAlphabeticalNoCase);
	m_Manifest.m_InteriorNames.QSort(0, -1, SortAlphabeticalNoCase);
	ClearScenarioGroupWidgets();
#endif // SCENARIO_DEBUG
	m_CachedActiveRegionPtrs.Reset();
	m_CachedRegionPtrs.Reset();
	m_CachedRegionPtrsValid = false;
	InitCachedRegionPtrs();
	m_GroupsEnabled.Reset();
	const int numGroups = m_Manifest.m_Groups.GetCount();
	if(numGroups > 0)
	{
		m_GroupsEnabled.Reserve(numGroups);
		m_GroupsEnabled.Resize(numGroups);
		ResetGroupsEnabledToDefaults();
	}
#if SCRATCH_ALLOCATOR
	sysMemManager::GetInstance().GetScratchAllocator()->Reset();
#endif 
	g_fwMetaDataStore.GetBoxStreamer().PostProcess();

	CScenarioManager::CarGensMayNeedToBeUpdated();

#if SCENARIO_DEBUG
	CScenarioDebug::ms_Editor.PostLoadData();
	UpdateScenarioGroupWidgets();
	// If -scenarioedit is used, do extra checks to make sure we don't later
	// accidentally overwrite scenario data.
	if(PARAM_scenarioedit.Get())
	{
		PF_START_STARTUPBAR("Verify file versions");
		VerifyFileVersions();
	}
#endif	// SCENARIO_DEBUG
#if __DEV
	if (PARAM_notscenariopointsstreaming.Get())
	{
		PF_START_STARTUPBAR("Stream all in");
		//Stream all in
		for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
		{
			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.GetBoxStreamer().SetIsPinnedByUser(m_Manifest.m_RegionDefs[i]->m_SlotId, true);
			g_fwMetaDataStore.StreamingRequest(strLocalIndex(m_Manifest.m_RegionDefs[i]->m_SlotId), STRFLAG_DONTDELETE);
		}
	}
#endif // __DEV
}

void  CScenarioPointManager::RemoveEntityPointAtIndex(s32 pointIndex)
{
	Assert(pointIndex >= 0);
	Assert(pointIndex < m_EntityPoints.GetCount());
	
	CScenarioPoint* pt = m_EntityPoints[pointIndex];
	if (taskVerifyf(pt && pt->GetEntity(), "RemoveEntityPointAtIndex: %d identifies a CScenarioPoint which is NULL or does not have an attached entity!", pointIndex)) 
	{
		if(pt->GetOnAddCalled())
		{
			CScenarioManager::OnRemoveScenario(*pt);
		}

		taskAssert(m_NumScenarioPointsEntity > 0);	
		m_NumScenarioPointsEntity--;
		m_EntityPoints.DeleteFast(pointIndex);
	}
}

void  CScenarioPointManager::RemoveWorldPointAtIndex(s32 pointIndex)
{
	Assert(pointIndex >= 0);
	Assert(pointIndex < m_WorldPoints.GetCount());

	CScenarioPoint* pt = m_WorldPoints[pointIndex];
	if (taskVerifyf(pt, "RemoveEntityPoint index: %d identifies a CScenarioPoint which is NULL!", pointIndex)) 
	{
		if(pt->GetOnAddCalled())
		{
			CScenarioManager::OnRemoveScenario(*pt);
		}

		taskAssert(m_NumScenarioPointsWorld > 0);	
		m_NumScenarioPointsWorld--;
		m_WorldPoints.DeleteFast(pointIndex);
	}
}

void CScenarioPointManager::ResetGroupsEnabledToDefaults()
{
	const int numGroups = m_Manifest.m_Groups.GetCount();
	Assert(m_GroupsEnabled.GetCount() == numGroups);
	for(int i = 0; i < numGroups; i++)
	{
		SetGroupEnabled(i, m_Manifest.m_Groups[i]->GetEnabledByDefault());
	}

	m_ExclusivelyEnabledScenarioGroupIndex = -1;
}


int CScenarioPointManager::FindGroupByNameHash(atHashString athash, bool ASSERT_ONLY(mayNotExist)) const
{
	if(!athash.GetHash())
	{
		return CScenarioPointManager::kNoGroup;
	}

	const int numGroups = m_Manifest.m_Groups.GetCount();
	for(int i = 0; i < numGroups; i++)
	{
		if(m_Manifest.m_Groups[i]->GetNameHash() == athash.GetHash())
		{
			return i + 1;
		}
	}

	Assertf(mayNotExist, "Failed to find scenario group with hash %08x. - %s", athash.GetHash(), athash.GetCStr());

	return CScenarioPointManager::kNoGroup;
}


int CScenarioPointManager::FindChainedScenarioPoints(const CScenarioPoint& pt, CScenarioChainSearchResults* results, const int maxInDestArray) const
{
	if(!pt.IsChained())
		return 0;

	int numFound = 0;
	const int activeRegionCount = GetNumActiveRegions();
	for (int j = 0; j < activeRegionCount; j++)
	{
		const CScenarioPointRegion& region = GetActiveRegion(j);

		//Just in case we have chains that are coming from the same point but are in different regions ... 
		// we need to properly adjust the pointer for the results before we pass it into the next chaining graph
		// to prevent overriding of data.
		const int maxLeft = maxInDestArray - numFound;
		CScenarioChainSearchResults* placeInResults = &results[numFound];
		numFound += region.FindChainedScenarioPointsFromScenarioPoint(pt, placeInResults, maxLeft);

		if(!Verifyf(numFound < maxInDestArray, "Filled up destination array while attempting to find points chained to a point, consider increasing."))
		{
			break;
		}
	}

	return numFound;
}

bool CScenarioPointManager::IsChainedWithIncomingEdges(const CScenarioPoint& pt) const
{
	//Search through all regions
	const int activeRegionCount = GetNumActiveRegions();
	for (int j = 0; j < activeRegionCount; j++)
	{
		const CScenarioPointRegion& region = GetActiveRegion(j);
		if (region.IsChainedWithIncomingEdges(pt))
		{
			return true;
		}
	}

	return false;
}

#if __ASSERT

bool CScenarioPointManager::IsChainEndPoint(const CScenarioPoint& pt) const
{
	// Check if it's even in a chain.
	if(!pt.IsChained())
	{
		return false;
	}

	// Search through all regions.
	const int activeRegionCount = GetNumActiveRegions();
	for (int j = 0; j < activeRegionCount; j++)
	{
		const CScenarioPointRegion& region = GetActiveRegion(j);
		const CScenarioChainingGraph& graph = region.GetChainingGraph();

		// Check if it's a node in a chain in this region.
		const int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(pt);
		if(nodeIndex >= 0)
		{
			// It's an end point if it's not a node with outgoing edges.
			return !graph.GetNode(nodeIndex).m_HasOutgoingEdges;
		}
	}

	// Probably shouldn't hit this case.
	return false;
}

#endif	// __ASSERT

/*
PURPOSE
	Get the index in m_ExtraScenarioDataPoints/m_ExtraScenarioData corresponding to
	a particular point, if it exists.
PARAMS
	pt		- The point to check.
RETURNS
	A valid index, or -1 if this point doesn't have any extra data.
*/
int CScenarioPointManager::FindExtraDataIndex(const CScenarioPoint& pt) const
{
	// If this bool isn't set, we shouldn't have any data associated with this point,
	// so there is no need to search for it.
	if(!pt.IsRunTimeFlagSet(CScenarioPointRuntimeFlags::HasExtraData))
	{
		return -1;
	}

	// Get the index of the point. This probably involves integer division, but
	// if we have a large number of objects we should make up for that by having
	// a more cache-friendly array (16 bits rather than 32 or 64 bit pointers).
	const int pointIndex = CScenarioPoint::GetPool()->GetJustIndexFast(&pt);
	Assert(pointIndex >= 0);	// Make sure we found an index.

	// Perform a binary search using std::lower_bound() in m_ExtraScenarioDataPoints,
	// the sorted array of indices of CScenarioPoint objects with extra data.
	const int cnt = m_ExtraScenarioDataPoints.GetCount();
	Assert(pointIndex <= 0xffff);	// Make sure it fits in the u16's we use.
	const u16 pointIndex16 = (u16)pointIndex;
	const u16* start = m_ExtraScenarioDataPoints.GetElements();
	const u16* end = start + cnt;
	const u16* found = std::lower_bound(start, end, pointIndex16);
	const int index = (int)(found - start);

	// Since we keep track of which points have extra data, using m_bHasExtraData,
	// if we got here and didn't find anything, there was probably a management error
	// of some sort.
	if(Verifyf(index < cnt, "Unexpectedly failed to find index of scenario point extra data (%p, %d/%d)", &pt, index, cnt))
	{
		Assert(m_ExtraScenarioDataPoints[index] == pointIndex);
		return index;
	}
	return -1;
}

/*
PURPOSE
	Find the CScenarioPointExtraData object associated with a given CScenarioPoint,
	if it exists.
PARAMS
	pt		- The point to get the extra data for.
RETURNS
	Pointer to the extra data if it already exists for this point, or NULL otherwise.
*/
CScenarioPointExtraData* CScenarioPointManager::FindExtraData(const CScenarioPoint& pt) const
{
	int index = FindExtraDataIndex(pt);
	if(index >= 0)
	{
		return m_ExtraScenarioData[index];
	}
	return NULL;
}

/*
PURPOSE
	If a CScenarioPointExtraData object already exists for a given point, return that.
	If it doesn't, create one and set up the data structures for the association.
PARAMS
	pt		- The point to find or create data for.
RETURNS
	Pointer to the CScenarioPointExtraData object now associated with this point. The only
	case when it returns NULL is if running out of pool space, but if so, it may do so silently,
	and the calling code may want to assert.
*/
CScenarioPointExtraData* CScenarioPointManager::FindOrCreateExtraData(CScenarioPoint& pt)
{
	// Get the index of the point. This probably involves integer division, but
	// if we have a large number of objects we should make up for that by having
	// a more cache-friendly array (16 bits rather than 32 or 64 bit pointers).
	const int pointIndex = CScenarioPoint::GetPool()->GetJustIndexFast(&pt);
	Assert(pointIndex >= 0);

	// Perform a binary search using std::lower_bound() in m_ExtraScenarioDataPoints,
	// the sorted array of indices of CScenarioPoint objects with extra data.
	const int cnt = m_ExtraScenarioDataPoints.GetCount();
	Assert(pointIndex <= 0xffff);	// Make sure it fits in the u16's we use.
	const u16 pointIndex16 = (u16)pointIndex;
	const u16* start = m_ExtraScenarioDataPoints.GetElements();
	const u16* end = start + cnt;
	const u16* found = std::lower_bound(start, end, pointIndex16);
	const int index = (int)(found - start);

	// We had to do the binary search regardless of whether the point already had
	// data or if we need to create it, but now we can check if it should already have had data.
	if(pt.IsRunTimeFlagSet(CScenarioPointRuntimeFlags::HasExtraData))
	{
		// If m_bHasExtraData was set, the binary search above should have found the
		// index of it.
		if(Verifyf(index < cnt, "Unexpectedly failed to find index of scenario point extra data (%p, %d/%d)", &pt, index, cnt))
		{
			Assert(m_ExtraScenarioDataPoints[index] == pointIndex);
			return m_ExtraScenarioData[index];
		}
	}

	// At this point, the index should either be past the end of the array, or
	// the element where we are about to insert should be larger than the value
	// we are about to insert.
	Assert(index == cnt || m_ExtraScenarioDataPoints[index] > pointIndex16);

	// Check if the pool is full.
	if(!CScenarioPointExtraData::GetPool()->GetNoOfFreeSpaces())
	{
		return NULL;
	}

	// Get a new object from the pool.
	CScenarioPointExtraData* pData = rage_new CScenarioPointExtraData;

	// Insert it at the point we found in the binary search, in the two parallel
	// arrays. Note that this will involve shifting existing data in the arrays,
	// and it's not entirely clear if it's best to always keep the arrays sorted
	// or try to sort them after inserting multiple points.
	m_ExtraScenarioDataPoints.Insert(index) = pointIndex16;
	m_ExtraScenarioData.Insert(index) = pData;

	// Remember that this point now has data.
	pt.SetRunTimeFlag(CScenarioPointRuntimeFlags::HasExtraData, true);

	return pData;
}

/*
PURPOSE
	If the scenario point has an associated CScenarioPointExtraData object, destroy it.
	If not, do nothing.
PARAMS
	pt		- The point to destroy the extra data for.
*/
void CScenarioPointManager::DestroyExtraData(CScenarioPoint& pt)
{
	// Look for it. This should check m_bHasExtraData internally.
	const int index = FindExtraDataIndex(pt);
	if(index < 0)
	{
		return;
	}

	// Delete the CScenarioPointExtraData object (return it to the pool).
	delete m_ExtraScenarioData[index];

	// Delete from the arrays. Note that this involves shifting data over.
	// We could not blindly switch these to DeleteFast(), because the arrays
	// are sorted.
	m_ExtraScenarioData.Delete(index);
	m_ExtraScenarioDataPoints.Delete(index);

	// Maintain the m_bHasExtraData flag.
	Assert(pt.IsRunTimeFlagSet(CScenarioPointRuntimeFlags::HasExtraData));
	pt.SetRunTimeFlag(CScenarioPointRuntimeFlags::HasExtraData, false);
}

int CScenarioPointManager::FindOrAddRequiredIMapByHash(u32 nameHash)
{
	int foundId = m_RequiredIMapHashes.Find(nameHash);
	if (foundId == -1)
	{
		foundId = m_RequiredIMapHashes.GetCount();

		//Add it as new ... 
		m_RequiredIMapHashes.PushAndGrow(nameHash);

		u16 id = CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound;
		const int mapDataSlot = fwMapDataStore::GetStore().FindSlotFromHashKey(nameHash).Get();
		if(mapDataSlot >= 0)
		{
			Assign(id, mapDataSlot);
		}
		m_RequiredIMapStoreSlots.PushAndGrow(id);
	}
	return foundId;
}

u32 CScenarioPointManager::GetRequiredIMapHash(unsigned index) const
{
	Assert(index != CScenarioPointManager::kNoRequiredIMap);
	Assert(index < m_RequiredIMapHashes.GetCount());
	return m_RequiredIMapHashes[index];
}

unsigned CScenarioPointManager::GetRequiredIMapSlotId(unsigned index) const
{
	Assert(index != CScenarioPointManager::kNoRequiredIMap);
	Assert(index < m_RequiredIMapStoreSlots.GetCount());
	return m_RequiredIMapStoreSlots[index];
}

const CScenarioPointGroup& CScenarioPointManager::GetGroupByIndex(int groupIndex) const
{	
	Assert(groupIndex >=0);
	Assert(groupIndex < m_Manifest.m_Groups.GetCount());
	return *m_Manifest.m_Groups[groupIndex];
}

void CScenarioPointManager::SetGroupEnabled(int groupIndex, bool b)
{
	if(m_GroupsEnabled[groupIndex] == b)
	{
		return;
	}

	m_GroupsEnabled[groupIndex] = b;

	CScenarioManager::CarGensMayNeedToBeUpdated();
}

int CScenarioPointManager::GetNumGroups() const
{	
	return m_Manifest.m_Groups.GetCount();
}

u32 CScenarioPointManager::GetNumRegions() const 
{ 
	if(!m_CachedRegionPtrsValid)
	{
		UpdateCachedRegionPtrs();
	}

	// Should hold up:
	//	Assert(m_Manifest.m_RegionDefs.GetCount() == m_CachedRegionPtrs.GetCount());

	return m_CachedRegionPtrs.GetCount();
}


CScenarioPointRegion* CScenarioPointManager::GetRegion(u32 idx) const
{
	if(!m_CachedRegionPtrsValid)
	{
		UpdateCachedRegionPtrs();
	}

	// Should hold up:
	//	Assert(FindRegionByIndex(idx) == m_CachedRegionPtrs[idx]);
	if(idx<m_CachedRegionPtrs.GetCount())
		return m_CachedRegionPtrs[idx];
	else
		return NULL;
}


CScenarioPointRegion* CScenarioPointManager::FindRegionByIndex(u32 idx) const
{ 
	//It is possible for this region to be streamed out ... 
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());
	if(Verifyf(m_Manifest.m_RegionDefs[idx], "Region Def [%d] is NULL", idx))
	{
#if SCENARIO_DEBUG
		//Editor added regions need to be taken into account.
		if (m_Manifest.m_RegionDefs[idx]->m_SlotId == -1)
		{
			//Non-streamed object cause it is editor added
			return m_Manifest.m_RegionDefs[idx]->m_NonStreamPointer;
		}
		else
#endif // SCENARIO_DEBUG
		{
			fwAssetDef<fwMetaDataFile>* slotData = g_fwMetaDataStore.GetSlot(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId));
			if (AssertVerify(slotData) && slotData->m_pObject && slotData->m_pObject->GetObject<CScenarioPointRegion>())
			{
				return slotData->m_pObject->GetObject<CScenarioPointRegion>();
			}
		}
	}

	return NULL;
}


void CScenarioPointManager::InitCachedRegionPtrs()
{
	const int sz = m_Manifest.m_RegionDefs.GetCount();

	m_CachedRegionPtrs.Reset();
	m_CachedRegionPtrs.Reserve(sz);
	m_CachedRegionPtrs.Resize(sz);
	for(int i = 0; i < sz; i++)
	{
		m_CachedRegionPtrs[i] = NULL;
	}

	m_CachedActiveRegionPtrs.Reset();
	m_CachedActiveRegionPtrs.Reserve(Min(sz, 32));	// Fairly arbitrary, hopefully enough to avoid reallocation except for extreme cases.

	m_CachedRegionPtrsValid = false;
}


void CScenarioPointManager::InitManifest()
{
#if __BANK
	m_Manifest.m_RegionDefs.QSort(0, -1, SortAlphabeticalNoCase);
	m_Manifest.m_Groups.QSort(0, -1, SortAlphabeticalNoCase);
	m_Manifest.m_InteriorNames.QSort(0, -1, SortAlphabeticalNoCase);
#endif
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		char PlatformFilename[RAGE_MAX_PATH];
		formatf(PlatformFilename,"%s.%s", m_Manifest.m_RegionDefs[i]->m_Name.GetCStr(), META_FILE_EXT);
		strPackfileManager::RegisterIndividualFile(PlatformFilename);
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();

		//The packfile is registered as the full path of the file minus the ext
		const fwAssetDef<fwMetaDataFile>::strObjectNameType RegionNameHash(m_Manifest.m_RegionDefs[i]->m_Name.GetCStr());
		m_Manifest.m_RegionDefs[i]->m_SlotId = g_fwMetaDataStore.MakeSlotStreamable(RegionNameHash, m_Manifest.m_RegionDefs[i]->m_AABB);
	}
}


u32 CScenarioPointManager::AddToSpatialArray( u32 curIndex, const u32 maxSNodes, CScenarioPoint& pt )
{
	CSpatialArrayNode_SP* node = NULL;

	if (curIndex >= maxSNodes)
	{
		node = &m_SpatialNodeList.Append(); // use a new node
	}
	else
	{
		node = &m_SpatialNodeList[curIndex]; //reuse a node
	}   
	curIndex++;

	node->ResetOffs();
	node->m_Point = &pt;

	u32 flags = 0;
	if (pt.HasExtendedRange())
	{
		flags |= CSpatialArrayNode_SP::kSpatialArrayTypeFlagExtendedRange;
	}
	m_SpatialArray->Insert(*node, flags);

	Vec3V pos = pt.GetPosition();
	m_SpatialArray->Update(*node, pos.GetXf(), pos.GetYf(), pos.GetZf());	

	return curIndex;
}

void CScenarioPointManager::UpdateNonRegionPointSpatialArray()
{
	PF_FUNC(NR_SP_SpatialArrayUpdate);

	if (!m_SpatialArray)
		return;

	const u32 curtime = fwTimer::GetTimeInMilliseconds();
	const u32 timedif = curtime - m_LastNonRegionPointUpdate;
	static u32 TEMP_MS_UPDATE = 1000;
	if (timedif < TEMP_MS_UPDATE)
		return;

	if (!s_UseNonRegionPointSpatialArray)
		return;

	m_LastNonRegionPointUpdate = curtime;

	const s32 epcount = m_EntityPoints.GetCount();
	const s32 wpcount = m_WorldPoints.GetCount();
	const s32 tcount = epcount + wpcount;

	//////////////////////////////////////////////////////////////////////////
	//NOTE: 
	//
	// 1. The "removal" of old nodes to rebuild the spatial array is done not by
	// removing the node directly we just reset the offs member variable in the
	// CSpatialArrayNode_SP object which "removes" it from the array. 
	//
	// 2. As we go through the m_EntityPoints array we insert the points into 
	// m_SpatialArray effectively overriding the memory at that offset. Also we 
	// append/or grab an existing node in the m_SpatialNodeList effectively repurposing the node that
	// was in that array location.
	//
	// 3. Finally we clean up the left over nodes if any exist from last time we updated
	// and we set the count to the newly found number of points
	//
	// Doing this results in a faster rebuild of the spatial array. 
	//////////////////////////////////////////////////////////////////////////

	const u32 socount = m_SpatialNodeList.GetCount();
	m_SpatialArray->Reset();

	if (tcount < m_SpatialNodeList.GetCapacity())
	{
		u32 curIndex = 0;

		// Entity Points ... 
		for(s32 i = 0; i < epcount; i++)
		{
			CScenarioPoint* pt = m_EntityPoints[i];
			Assert(pt);

			if (pt->GetEntity()) //Only deal with entity attached guys... still needed?
			{
				curIndex = AddToSpatialArray(curIndex, socount, *pt);
			}
		}

		// World Points ... 
		for(s32 i = 0; i < wpcount; i++)
		{
			CScenarioPoint* pt = m_WorldPoints[i];
			Assert(pt);

			curIndex = AddToSpatialArray(curIndex, socount, *pt);
		}

		//If the curIndex is < the overall node count then we need to reset the rest of the items in the array.
		if (curIndex < socount)
		{
			for(u32 i = curIndex; i < socount; i++)
			{
				m_SpatialNodeList[i].ResetOffs();
				m_SpatialNodeList[i].m_Point = NULL;
			}
		}
		//Set the count to the new amount.
		m_SpatialNodeList.Resize(curIndex);
	}
	else
	{
		Assertf(0, "CScenarioPointManager::m_SpatialArray does not have enough elements. Have %d Needed %d", m_SpatialNodeList.GetCapacity(), tcount);
	}
}

void CScenarioPointManager::UpdateCachedRegionPtrs() const
{
	const int cnt = m_CachedRegionPtrs.GetCount();
	Assert(cnt == m_Manifest.m_RegionDefs.GetCount());
	m_CachedActiveRegionPtrs.ResetCount();
	for(int i = 0; i < cnt; i++)
	{
		CScenarioPointRegion* pReg = FindRegionByIndex(i);
		m_CachedRegionPtrs[i] = pReg;
		if(pReg)
		{
			m_CachedActiveRegionPtrs.Grow() = pReg;
		}
	}

	m_CachedRegionPtrsValid = true;
}


int CScenarioPointManager::GetNumActiveRegions() const
{
	if(!m_CachedRegionPtrsValid)
	{
		UpdateCachedRegionPtrs();
	}

	return m_CachedActiveRegionPtrs.GetCount();
}


unsigned int CScenarioPointManager::FindOrAddInteriorName(u32 nameHash)
{
	const int num = m_Manifest.m_InteriorNames.GetCount();
	for(int i = 0; i < num; i++)
	{
		if(m_Manifest.m_InteriorNames[i].GetHash() == nameHash)
		{
			return i + 1;
		}
	}

#if SCENARIO_DEBUG
	SetManifestNeedSave();
#endif	// SCENARIO_DEBUG

	// Note: if the manifest has been saved out correctly, in non-dev builds we
	// shouldn't be getting here, so the Grow() here should really only allocate
	// anything during development.
	m_Manifest.m_InteriorNames.Grow() = atHashWithStringBank(nameHash);
#if SCENARIO_DEBUG
	m_interiorSourceMap.SafeInsert(nameHash, atHashString(m_manifestSources[m_sWorkingManifestIndex]));
	m_interiorSourceMap.FinishInsertion();
#endif	// SCENARIO_DEBUG
	return num + 1;
}


bool CScenarioPointManager::IsInteriorStreamedIn(unsigned int index) const
{
	// TODO: Maybe try to find a cheaper way of checking if interiors are
	// streamed in. If nothing else, CScenarioPointManager could maintain an
	// array of bools or some other sort of caching mechanism.

	u32 hash = m_Manifest.m_InteriorNames[index].GetHash();

	fwPool<CInteriorInst>* pPool = CInteriorInst::GetPool();
	int poolSize = pPool->GetSize();
	for(int i = 0; i < poolSize; i++)
	{
		const CInteriorInst* pInst = pPool->GetSlot(i);
		if(!pInst)
		{
			continue;
		}
		if(pInst->GetBaseModelInfo()->GetModelNameHash() == hash)
		{
			return pInst->CanReceiveObjects();
		}
	}

	return false;
}

#if SCENARIO_DEBUG

void CScenarioPointManager::RenderPointSummary() const
{
	char dbtxt[RAGE_MAX_PATH];
	Color32 txtCol = Color_white;

	grcDebugDraw::AddDebugOutputEx(false, txtCol, "//----------------------------------------------------------------------------");
	grcDebugDraw::AddDebugOutputEx(false, txtCol, "Region Points - %d Active", GetNumActiveRegions());

	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%8s%8s%7s%7s%17s%9s%15s", "Name", "Points", "Chains", "Edges", "Nodes", "Entity Overrides", "Clusters", "Cluster Points" );
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	const int rCount = GetNumRegions();
	for (int r = 0; r < rCount; r++)
	{
		const CScenarioPointRegion* region = GetRegion(r);
		if (region)
		{
			const CScenarioPointRegionEditInterface bankRegion(*region);

			const char* rname = GetRegionName(r);
			int length = istrlen(rname);
			int noff = 0;
			for (int c = 0; c < length; c++)
			{
				if (rname[c] == '/' || rname[c] == '\\')
				{
					noff = c+1;
				}
			}
			const char* name = &rname[noff];

			const u32 pcount = region->GetNumPoints();
			const CScenarioChainingGraph& graph = region->GetChainingGraph();
			const u32 ccount = graph.GetNumChains(); 
			const u32 ecount = graph.GetNumEdges();
			const u32 ncount = graph.GetNumNodes();
			const u32 eocount = region->GetNumEntityOverrides();
			const u32 clcount = bankRegion.GetNumClusters();
			const u32 clpcount = bankRegion.GetTotalNumClusteredPoints();
			
			formatf(dbtxt, RAGE_MAX_PATH, "%-32s%8d%8d%7d%7d%17d%9d%15d", name, pcount, ccount, ecount, ncount, eocount, clcount, clpcount );
			grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);
		}
	}

	grcDebugDraw::AddDebugOutputEx(false, txtCol, "//----------------------------------------------------------------------------");
	grcDebugDraw::AddDebugOutputEx(false, txtCol, "Non Region Points");
	grcDebugDraw::AddDebugOutputEx(false, txtCol, "Entity %d", m_EntityPoints.GetCount());

	if (CScenarioDebug::ms_bRenderPopularTypesInSummary)
	{
		RenderPointPopularity(m_EntityPoints, 10, txtCol);
	}

	grcDebugDraw::AddDebugOutputEx(false, txtCol, "World %d", m_WorldPoints.GetCount());

	if (CScenarioDebug::ms_bRenderPopularTypesInSummary)
	{
		RenderPointPopularity(m_WorldPoints, 10, txtCol);
	}

	grcDebugDraw::AddDebugOutputEx(false, txtCol, "//----------------------------------------------------------------------------");
	
	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6s%7s", "Usage Limits", "Used", "Total");
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6d%7d", "Point (Pool)", CScenarioPoint::_ms_pPool->GetNoOfUsedSpaces(), CScenarioPoint::_ms_pPool->GetSize());
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6d%7s", "Point (Debug Heap)", CScenarioPoint::sm_NumEditorPoints, "-");
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6d%7d", "Non Region Point Spatial Array", m_SpatialNodeList.GetCount(), m_SpatialNodeList.GetCapacity());
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);
	
	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6d%7d", "Chain User", CScenarioPointChainUseInfo::_ms_pPool->GetNoOfUsedSpaces(), CScenarioPointChainUseInfo::_ms_pPool->GetSize());
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6d%7d", "Cluster", CSPClusterFSMWrapper::_ms_pPool->GetNoOfUsedSpaces(), CSPClusterFSMWrapper::_ms_pPool->GetSize());
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	formatf(dbtxt, RAGE_MAX_PATH, "%-32s%6d%7d", "Cluster tracking data", CScenarioClusterSpawnedTrackingData::_ms_pPool->GetNoOfUsedSpaces(), CScenarioClusterSpawnedTrackingData::_ms_pPool->GetSize());
	grcDebugDraw::AddDebugOutputEx(false, txtCol, dbtxt);

	grcDebugDraw::AddDebugOutputEx(false, txtCol, "//----------------------------------------------------------------------------");
	
}

int	CScenarioPointManager::FindPointsClusterIndex(const CScenarioPoint& pt) const
{
	Assertf(pt.IsInCluster(), "Point is not in a cluster");

	int retval = CScenarioPointCluster::INVALID_ID;

	const int activeCount = GetNumActiveRegions();
	for (int i = 0; i < activeCount; i++)
	{
		const CScenarioPointRegionEditInterface bankRegion(GetActiveRegion(i));
		retval = bankRegion.FindPointsClusterIndex(pt);
		if (retval != CScenarioPointCluster::INVALID_ID)
		{
			//FOUND IT !!!
			break;
		}
	}

	return retval;
}

int	CScenarioPointManager::FindPointsChainedIndex(const CScenarioPoint& pt) const
{
	Assertf(pt.IsChained(), "Point is not chained");

	const int activeCount = GetNumActiveRegions();
	for (int i = 0; i < activeCount; i++)
	{
		const CScenarioPointRegionEditInterface bankRegion(GetActiveRegion(i));
		const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
		int nodeidx = graph.FindChainingNodeIndexForScenarioPoint(pt);
		if (nodeidx != -1)
		{
			return graph.GetNodesChainIndex(nodeidx);
		}
	}

	return -1;
}

const CInteriorInst* CScenarioPointManager::FindInteriorForPoint(const CScenarioPoint& pt)
{
	const Vec3V posV = pt.GetPosition();
	CInteriorInst* pIntInst = NULL;
	s32 roomIdx = -1;
	if(CPortalTracker::ProbeForInterior(RCC_VECTOR3(posV), pIntInst, roomIdx, NULL, CPortalTracker::SHORT_PROBE_DIST))
	{
		return pIntInst;
	}
	return NULL;
}


bool CScenarioPointManager::UpdateInteriorForPoint(CScenarioPoint& pt)
{
	if(pt.GetInterior() != CScenarioPointManager::kNoInterior)
	{
		// Already got an interior, nothing changed.
		return false;
	}

	const CInteriorInst* pIntInst = FindInteriorForPoint(pt);
	if(!pIntInst)
	{
		// We still haven't confirmed the presence of an interior,
		// so we still consider ourselves potentially outside, nothing has changed.
		return false;
	}

	// Get the name of the interior and add it to the list if it's not
	// already there. If we found a new one, the manifest will get marked
	// as changed by FindOrAddInteriorName().
	const u32 nameHash = pIntInst->GetBaseModelInfo()->GetModelNameHash();
	unsigned int interiorId = FindOrAddInteriorName(nameHash);
	pt.SetInterior(interiorId);

	return true;
}


bool CScenarioPointManager::UpdateInteriorForRegion(int regionIndex)
{
	bool changed = false;

	CScenarioPointRegion* pRegion = GetRegion(regionIndex);
	if(!pRegion)
	{
		return changed;
	}

	CScenarioPointRegionEditInterface bankRegion(*pRegion);
	changed = bankRegion.UpdateInteriorForPoints();

	if(changed)
	{
		// Something changed in the interior info, flag this region as edited.
		FlagRegionAsEdited(regionIndex);
	}

	return changed;
}


s32 CScenarioPointManager::GetNumWorldPoints() const
{
	return m_NumScenarioPointsWorld;
}

CScenarioPoint& CScenarioPointManager::GetWorldPoint(s32 index) {
	Assert(index >= 0 && index < m_NumScenarioPointsWorld);
	Assert(m_WorldPoints[index]);
	return *(m_WorldPoints[index]);
}

const CScenarioPoint& CScenarioPointManager::GetWorldPoint(s32 index) const {
	Assert(index >= 0 && index < m_NumScenarioPointsWorld);
	Assert(m_WorldPoints[index]);
	return *(m_WorldPoints[index]);
}

void CScenarioPointManager::RemoveEntityPoint(s32 index)
{
	RemoveEntityPointAtIndex(index);
}

const char* CScenarioPointManager::GetRegionName(u32 idx) const
{ 
	Assert(idx < m_Manifest.m_RegionDefs.GetCount()); 
	return m_Manifest.m_RegionDefs[idx]->m_Name.GetCStr();
}

const char* CScenarioPointManager::GetRegionSource(u32 idx) const
{ 
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());
	if(const atHashString* str = m_regionSourceMap.Access(m_Manifest.m_RegionDefs[idx]->m_Name))
	{
		return str->GetCStr();
	}
	return "";
}

CScenarioPointRegion* CScenarioPointManager::GetRegionByNameHash(u32 nameHash)
{
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		if (m_Manifest.m_RegionDefs[i]->m_Name == nameHash)
		{
			return GetRegion(i);
		}
	}
	return NULL;
}

const spdAABB& CScenarioPointManager::GetRegionAABB(u32 idx) const
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount()); 
	return m_Manifest.m_RegionDefs[idx]->m_AABB;
}

void CScenarioPointManager::ReloadRegion(u32 idx)
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());

	//Just need to remove it from the world ... the streaming system will bring it back once it is "unloaded"
	if (m_Manifest.m_RegionDefs[idx]->m_SlotId != -1)
	{
		if (g_fwMetaDataStore.GetBoxStreamer().GetIsPinnedByUser(m_Manifest.m_RegionDefs[idx]->m_SlotId))
		{
			g_fwMetaDataStore.GetBoxStreamer().SetIsPinnedByUser(m_Manifest.m_RegionDefs[idx]->m_SlotId, false);
			g_fwMetaDataStore.RemoveRef(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId), REF_OTHER); //remove a ref so it can be streamed out.
		}
		g_fwMetaDataStore.RemoveLoaded(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId));
	}
}

void CScenarioPointManager::SaveRegion(u32 idx, bool resourceSaved)
{
	if (RegionIsEdited(idx))
	{
		//Any region that is considered edited is not allowed to be streamed out.
		CScenarioPointRegion* region = GetRegion(idx);
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		const char* levelName = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex());
		char subDir[RAGE_MAX_PATH];
		formatf(subDir, "levels/%s/scenario", levelName);

		char rootPath[RAGE_MAX_PATH]={0};

		atHashString hash = m_regionSourceMap[m_Manifest.m_RegionDefs[idx]->m_Name];
		const bool& isDLC = strnicmp("dlc",hash.GetCStr(),3)==0;//m_manifestSourceInfos[m_sWorkingManifestIndex];
		if(isDLC)
		{
			char assetPath[RAGE_MAX_PATH]={0};
			strncpy(assetPath,hash.GetCStr(),static_cast<int>(strcspn(hash.GetCStr(),":")+2));
			EXTRACONTENT.GetAssetPathFromDevice(assetPath);
			formatf(rootPath, "%sexport/", assetPath);
		}
		else
		{
			const char* pScenarioDir = NULL;
			if(PARAM_scenariodir.Get(pScenarioDir))
			{
				formatf(rootPath, "%s", pScenarioDir);
			}
			else
			{
				formatf(rootPath, "%sexport/", CFileMgr::GetAssetsFolder());
			}
		}

		char FileName[RAGE_MAX_PATH];
		formatf(FileName, "%s%s/%s.pso.meta", rootPath, subDir, ASSET.FileName(m_Manifest.m_RegionDefs[idx]->m_Name.GetCStr()));

		// Before saving, check if we have any existing data and make sure that we don't
		// silently overwrite it if it appears be newer than what's in memory.
		int existingVersionNumber = 0;
		if(ReadVersionNumber(FileName, existingVersionNumber))
		{
			// This is before we have incremented the version number in memory, so if
			// the file was converted before it got loaded by the game, the version numbers
			// should match. The version number in memory being higher than on disc
			// should be OK too.
			if(existingVersionNumber > bankRegion.GetVersionNumber())
			{
				char buff[1024];
				formatf(buff,
					"Data in '%s' is about to get overwritten by what may be an older version. Are you really sure you want to do this?",
					FileName);
				if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
				{
					// Don't save.
					return;
				}
			}
		}

		if (bankRegion.Save(FileName))
		{
			if (resourceSaved)
			{
				// conversion script
				char script[RAGE_MAX_PATH];
				formatf(script, "%s%s", CFileMgr::GetToolFolder(), "lib\\util\\data_convert_file.rb");

				if (Verifyf(strlen(script) + strlen(FileName) < RAGE_MAX_PATH, "Command line is too big for file [%s]", FileName))
				{
					Displayf("%s %s", script, FileName);

					char command[RAGE_MAX_PATH];
					formatf(command, "%s %s", script, FileName);
					//Launch the conversion script ... 
					int retval = sysExec(command);
					if (!retval)
					{
						//if it was edited then this data is now no longer needed to be kept around
						if (m_Manifest.m_RegionDefs[idx]->m_SlotId != -1 && g_fwMetaDataStore.GetBoxStreamer().GetIsPinnedByUser(m_Manifest.m_RegionDefs[idx]->m_SlotId))
						{
							g_fwMetaDataStore.GetBoxStreamer().SetIsPinnedByUser(m_Manifest.m_RegionDefs[idx]->m_SlotId, false);
							g_fwMetaDataStore.RemoveRef(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId), REF_OTHER);
						}
						else
						{
							//delete the pointer to the data if it is needed the streaming system will bring it back.
							// NOTE: assumes that you are saving the manifest as well here.
							delete m_Manifest.m_RegionDefs[idx]->m_NonStreamPointer;
							m_Manifest.m_RegionDefs[idx]->m_NonStreamPointer = NULL;
						}
					}
					else
					{
						Errorf("Command Returned code %d", retval);
					}
				}
			}
		}
		else
		{
			char buff[1024];
			formatf(buff,
				"Unable to save file '%s'. Please make sure the file is checked out and try again.",
				FileName);
			bkMessageBox("Scenario Editor", buff, bkMsgOk, bkMsgError);
		}
	}
}

void  CScenarioPointManager::LoadSaveAllRegion(const bool resourceSaved)
{
	const int regioncount = m_Manifest.m_RegionDefs.GetCount();
	for (int i = 0; i < regioncount; i++)
	{
		bool forced = false;
		CScenarioPointRegion* region = GetRegion(i);
		if (!region)
		{
			ForceLoadRegion(i);

			UpdateRegionAABB(i);

			region = GetRegion(i);
			forced = true;
		}
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		{
			FlagRegionAsEdited(i);
			SaveRegion(i, resourceSaved);

			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.GetBoxStreamer().SetIsPinnedByUser(m_Manifest.m_RegionDefs[i]->m_SlotId, false);
			g_fwMetaDataStore.RemoveRef(strLocalIndex(m_Manifest.m_RegionDefs[i]->m_SlotId), REF_OTHER);
		}
		

		if (forced)
		{
			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.ClearRequiredFlag(m_Manifest.m_RegionDefs[i]->m_SlotId, STRFLAG_DONTDELETE);

			//Need to release it right away because not doing so could fill the 
			// pool and cause other regions to not be able to be loaded.
			g_fwMetaDataStore.RemoveUnrequired();
		}
	}
}

void CScenarioPointManager::AddRegion(const char* name)
{
	CScenarioPointRegionDef* newRegion = rage_new CScenarioPointRegionDef;
	m_Manifest.m_RegionDefs.PushAndGrow(newRegion);
	//Append the full path
	const char* levelName = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex());
	char PlatformFilename[RAGE_MAX_PATH];
	bool isDLC = strnicmp(m_manifestSources[m_sWorkingManifestIndex],"dlc",3)==0;
	if(isDLC)
	{
		char dlcDevice[RAGE_MAX_PATH]={0};
		strncpy(dlcDevice,m_manifestSources[m_sWorkingManifestIndex],static_cast<int>(strcspn(m_manifestSources[m_sWorkingManifestIndex],":")+2));
		formatf(PlatformFilename,"%s%%PLATFORM%%/levels/%s/scenario/%s",dlcDevice,levelName, name);
	}
	else
	{
		formatf(PlatformFilename,"platform:/levels/%s/scenario/%s",levelName, name);
	}
	newRegion->m_Name = PlatformFilename;
	m_regionSourceMap.SafeInsert(atFinalHashString(PlatformFilename),atHashString(m_manifestSources[m_sWorkingManifestIndex]));
	m_regionSourceMap.FinishInsertion();

	//Cant set this up to be streamed at the current time ... 
	newRegion->m_NonStreamPointer = rage_new CScenarioPointRegion();
	SetManifestNeedSave();//Manifest needs saving

	InitCachedRegionPtrs();
}

void CScenarioPointManager::DeleteRegionByIndex(u32 idx)
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());
	m_regionSourceMap.Remove(m_regionSourceMap.GetIndexFromDataPtr(m_regionSourceMap.Access(m_Manifest.m_RegionDefs[idx]->m_Name)));
	if (m_Manifest.m_RegionDefs[idx]->m_SlotId != -1)
	{
		//Remove the file from the system
		char PlatformFilename[RAGE_MAX_PATH];
		formatf(PlatformFilename,"%s.%s", m_Manifest.m_RegionDefs[idx]->m_Name.GetCStr(), META_FILE_EXT);
		strPackfileManager::InvalidateIndividualFile(PlatformFilename);
		strStreamingEngine::GetInfo().UnregisterObject(g_fwMetaDataStore.GetStreamingIndex(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId)));

		//reset the slot
		g_fwMetaDataStore.RemoveSlot(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId));
	}
	else
	{
		delete m_Manifest.m_RegionDefs[idx]->m_NonStreamPointer;
	}
	delete m_Manifest.m_RegionDefs[idx];
	m_Manifest.m_RegionDefs.Delete(idx);
	SetManifestNeedSave();//Manifest needs saving

	// Make sure the cached pointer arrays have the appropriate size.
	InitCachedRegionPtrs();
}

void CScenarioPointManager::RenameRegionByIndex(u32 idx, const char* name)
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());
	//NOTE: Doing the rename without adjusting the resource file mappings allows
	// for a pseudo 'alias' setup so regions can still stream in and out even though 
	// the names are different now. Post a save of the manifest file this region will be
	// fully renamed and expect a resource of the correct name.
	//Append the full path
	m_regionSourceMap.Remove(m_regionSourceMap.GetIndexFromDataPtr(m_regionSourceMap.Access(m_Manifest.m_RegionDefs[idx]->m_Name)));
	const char* levelName = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex());
	char PlatformFilename[RAGE_MAX_PATH];
	bool isDLC = strnicmp(m_manifestSources[m_sWorkingManifestIndex],"dlc",3)==0;
	if(isDLC)
	{
		char dlcDevice[RAGE_MAX_PATH]={0};
		strncpy(dlcDevice,m_manifestSources[m_sWorkingManifestIndex],static_cast<int>(strcspn(m_manifestSources[m_sWorkingManifestIndex],":")+2));
		formatf(PlatformFilename,"%s%%PLATFORM%%/levels/%s/scenario/%s",dlcDevice,levelName, name);
	}
	else
	{
		formatf(PlatformFilename,"platform:/levels/%s/scenario/%s",levelName, name);
	}
	m_Manifest.m_RegionDefs[idx]->m_Name = PlatformFilename;
	m_regionSourceMap.SafeInsert(atFinalHashString(PlatformFilename),atHashString(m_manifestSources[m_sWorkingManifestIndex]));
	m_regionSourceMap.FinishInsertion();

	SetManifestNeedSave();//Manifest needs saving
}

void CScenarioPointManager::UpdateRegionAABB(u32 idx)
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());

	CScenarioPointRegion* region = GetRegion(idx);
	if (region)
	{
		CScenarioPointRegionEditInterface bankRegion(*region);
		spdAABB temp = bankRegion.CalculateAABB();
		//If the aabb is the same then we dont need to update it, but if we
		//	use the m_NonStreamPointer we should always keep indicate that the 
		//  manifest should be saved.
		// NOTE: the spdAABB == and != operators dont use an epsilon so 
		//		had to do it this way instead.
		if	(
				m_Manifest.m_RegionDefs[idx]->m_NonStreamPointer ||
				!(
					IsCloseAll(temp.GetMin(), m_Manifest.m_RegionDefs[idx]->m_AABB.GetMin(), ScalarV(0.001f)) 
					&&
					IsCloseAll(temp.GetMax(), m_Manifest.m_RegionDefs[idx]->m_AABB.GetMax(), ScalarV(0.001f))
				)
			
			)
		{
			m_Manifest.m_RegionDefs[idx]->m_AABB = temp;
			SetManifestNeedSave();//Manifest needs saving
		}
	} 
}

void CScenarioPointManager::UpdateRegionAABB(const CScenarioPointRegion& region)
{
	for (int idx = 0; idx < m_Manifest.m_RegionDefs.GetCount(); idx++)
	{
		if (GetRegion(idx) == &region)
		{
			UpdateRegionAABB(idx);
			break;
		}
	}
}

void CScenarioPointManager::FlagRegionAsEdited(u32 idx)
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());
	//New regions dont have valid slots ... They are always considered "edited" 
	if (m_Manifest.m_RegionDefs[idx]->m_SlotId != -1)
	{
		//if the region is already marked as edited then dont need to set it again.
		if (!RegionIsEdited(idx))
		{
			g_fwMetaDataStore.GetBoxStreamer().SetIsPinnedByUser(m_Manifest.m_RegionDefs[idx]->m_SlotId, true);
			g_fwMetaDataStore.AddRef(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId), REF_OTHER);//addref so it can not be streamed out.
		}
	}
}

bool CScenarioPointManager::RegionIsEdited(u32 idx) const
{
	Assert(idx < m_Manifest.m_RegionDefs.GetCount());
	//New regions dont have valid slots ... 
	if (m_Manifest.m_RegionDefs[idx]->m_SlotId != -1)
	{
		return g_fwMetaDataStore.GetBoxStreamer().GetIsPinnedByUser(m_Manifest.m_RegionDefs[idx]->m_SlotId);
	}

	//Returning true here because New Regions dont have valid slots so they are always considered edited
	// and never streamed out.
	return true;
}

void CScenarioPointManager::ForceLoadRegion(u32 idx)
{
	CScenarioPointRegion* region = GetRegion(idx);
	if (!region) //make sure it is not already loaded.
	{
		if(idx<m_Manifest.m_RegionDefs.GetCount())
		{
			if (Verifyf(m_Manifest.m_RegionDefs[idx]->m_SlotId != -1, "Trying to force load region %s but it does not have a valid streaming slot to force load into.", GetRegionName(idx)))
			{
				g_fwMetaDataStore.StreamingBlockingLoad(strLocalIndex(m_Manifest.m_RegionDefs[idx]->m_SlotId), STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
			}
		}
	}
}

void CScenarioPointManager::DeleteLoosePointsWithinRegion(u32 idx)
{
	const spdAABB& curRegion = GetRegionAABB(idx);
	Displayf("Deleting loose points falling within region %s", GetRegionName(idx));

	u32 totDelete = 0;
	//This does some assumptions about what you want to have happen and will
	// stream in all known regions to execute the operation ... 
	// NOTE: need to make sure that all regions are checked out for this kind of operation.
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		if ((u32)i == idx)
			continue;

		bool forced = false;
		CScenarioPointRegion* region = GetRegion(i);
		if (!region)
		{
			ForceLoadRegion(i);
			region = GetRegion(i);
			forced = true;
		}
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		//Execute the operation ... 
		bool changed = bankRegion.DeletePointsInAABB(curRegion);

		if (forced && !changed)
		{
			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.ClearRequiredFlag(m_Manifest.m_RegionDefs[i]->m_SlotId, STRFLAG_DONTDELETE);

			//Need to release it right away because not doing so could fill the 
			// pool and cause other regions to not be able to be loaded.
			g_fwMetaDataStore.RemoveUnrequired();
		}
		else if (changed)
		{
			//If we changed any data then we need to keep this region around an mark it as edited.
			FlagRegionAsEdited(i);
			// We also need to update the AABB and the chain graph.
			UpdateRegionAABB(i);
			bankRegion.UpdateChainingGraphMappings();
		}
	}

	Displayf("Deleted [%d] loose points", totDelete);
	if (totDelete)
	{
		// We also need to update the AABB and the chain graph.
		UpdateRegionAABB(idx);
		CScenarioPointRegion* region = GetRegion(idx);
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);
		bankRegion.UpdateChainingGraphMappings();
	}
}

bool CScenarioPointManager::RemoveDuplicateChainingEdges()
{
	bool removed = false;
	//This does some assumptions about what you want to have happen and will
	// stream in all known regions to execute the operation ... 
	// NOTE: need to make sure that all regions are checked out for this kind of operation.
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		bool forced = false;
		CScenarioPointRegion* region = GetRegion(i);
		if (!region)
		{
			ForceLoadRegion(i);
			region = GetRegion(i);
			forced = true;
		}
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		//Execute the operation ... 
		bool changed = false;
		int count = bankRegion.RemoveDuplicateChainEdges();
		if (count)
		{
			changed = true;
			removed = true;
			Displayf("Removed %d duplicates from region %s", count, GetRegionName(i));
		}

		if (forced && !changed)
		{
			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.ClearRequiredFlag(m_Manifest.m_RegionDefs[i]->m_SlotId, STRFLAG_DONTDELETE);

			//Need to release it right away because not doing so could fill the 
			// pool and cause other regions to not be able to be loaded.
			g_fwMetaDataStore.RemoveUnrequired();
		}
		else if (changed)
		{
			//If we changed any data then we need to keep this region around an mark it as edited.
			FlagRegionAsEdited(i);
		}
	}

	return removed;
}

static bool sGenerateFilenameForManifestPsoSource(char* filenameOut, int filenameOutSize, const char* psoFilename)
{
	const char* ptr = psoFilename;
	if(strnicmp("platform:/", ptr, 10) == 0)
	{
		ptr += 10;

		const char* pScenarioDir = NULL;
		if(PARAM_scenariodir.Get(pScenarioDir))
		{
			formatf(filenameOut, filenameOutSize, "%s%s", pScenarioDir, ptr);
		}
		else
		{
			formatf(filenameOut, filenameOutSize, "%sexport/%s", CFileMgr::GetAssetsFolder(), ptr);
		}

		char buff[RAGE_MAX_PATH];
		fiAssetManager::RemoveExtensionFromPath(buff, sizeof(buff), filenameOut);
		formatf(filenameOut, filenameOutSize, "%s.pso.meta", buff);

		return true;
	}
	const char* path = strstr(ptr,RSG_PLATFORM);
	if(path != NULL)
	{
		path += sizeof(RSG_PLATFORM);
		char assetPath[RAGE_MAX_PATH]={0};
		strncpy(assetPath,psoFilename,static_cast<int>(strcspn(psoFilename,":")+2));
		if(!EXTRACONTENT.GetAssetPathFromDevice(assetPath))
			return false;
		formatf(filenameOut, filenameOutSize, "%sexport/%s",assetPath , path);
		char buff[RAGE_MAX_PATH];
		fiAssetManager::RemoveExtensionFromPath(buff, sizeof(buff), filenameOut);
		formatf(filenameOut, filenameOutSize, "%s.pso.meta", buff);

		return true;
	}
	filenameOut[0] = '\0';
	return false;
}

void CScenarioPointManager::SaveManifest(CScenarioPointManifest& manifest, const char* file)
{
	//Alpha sort ... 
	manifest.m_RegionDefs.QSort(0, -1, SortAlphabeticalNoCase);
	manifest.m_Groups.QSort(0, -1, SortAlphabeticalNoCase);
	manifest.m_InteriorNames.QSort(0, -1, SortAlphabeticalNoCase);

	//////////////////////////////////////////////////////////////////////////
	// 	const char* filename = m_sWorkingManifestIndex == 0 ?
	// 		DATAFILEMGR.GetFirstFile(CDataFileMgr::SCENARIO_POINTS_FILE)->m_filename :
	// 	m_manifestSources[m_sWorkingManifestIndex];

	char path[RAGE_MAX_PATH]={0};
	bool isPso = sGenerateFilenameForManifestPsoSource(path,RAGE_MAX_PATH,file);
	if(!isPso)
		fiDevice::GetDevice(file)->FixRelativeName(path,RAGE_MAX_PATH,file);
	fiStream* pFile = ASSET.Create(path, "");
	if(Verifyf(pFile, "Unable to save '%s'.", path))
	{
		parXmlWriterVisitor v2(pFile);
		v2.Visit(manifest);
		Displayf("Saved '%s'", pFile->GetName());
		pFile->Close();
		pFile = NULL;
	}
}

void CScenarioPointManager::Save(bool resourceSaved)
{
	CScenarioDebug::ms_Editor.PreSaveData();
	//Assertf(m_sWorkingManifestIndex>0,"You're trying to save all combined data to a single manifest, it will save over the disk data");
	if (m_ManifestNeedSave)
	{
		//////////////////////////////////////////////////////////////////////////
		ScalarV zero(V_ZERO);
		for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
		{
			UpdateRegionAABB(i);
			m_Manifest.m_RegionDefs[i]->m_AABB.SetUserFloat1(zero);
			m_Manifest.m_RegionDefs[i]->m_AABB.SetUserFloat2(zero);
		}

		//Build a clone for saving purposes ... so we dont have to loose our current game data state.
		atArray<SaveData> saveables;
		saveables.Reserve(m_manifestSources.GetCount());
		for(int i=0;i<m_Manifest.m_RegionDefs.GetCount();i++)
		{
			atHashString& sourceManifest = m_regionSourceMap[m_Manifest.m_RegionDefs[i]->m_Name];
			int saveableIndex = -1;
			for(int j=0;j<saveables.GetCount();j++)
			{
				if(saveables[j]==sourceManifest)
				{
					saveableIndex = j;
					break;
				}
			}
			if(saveableIndex ==-1)
			{
				saveables.Grow().sourceFile = sourceManifest;
				saveableIndex = saveables.GetCount()-1;
			}
			saveables[saveableIndex].manifest.m_RegionDefs.PushAndGrow(m_Manifest.m_RegionDefs[i]);
		}
		for(int i=0;i<m_Manifest.m_Groups.GetCount();i++)
		{
			atHashString& sourceManifest = m_groupSourceMap[m_Manifest.m_Groups[i]->GetNameHashString()];
			int saveableIndex = -1;
			for(int j=0;j<saveables.GetCount();j++)
			{
				if(saveables[j]==sourceManifest)
				{
					saveableIndex = j;
					break;
				}
			}
			if(saveableIndex ==-1)
			{
				saveables.Grow().sourceFile = sourceManifest;
				saveableIndex = saveables.GetCount()-1;
			}
			saveables[saveableIndex].manifest.m_Groups.PushAndGrow(m_Manifest.m_Groups[i]);
		}
		for(int i=0;i<m_Manifest.m_InteriorNames.GetCount();i++)
		{
			atHashString& sourceManifest = m_interiorSourceMap[m_Manifest.m_InteriorNames[i]];
			int saveableIndex = -1;
			for(int j=0;j<saveables.GetCount();j++)
			{
				if(saveables[j]==sourceManifest)
				{
					saveableIndex = j;
					break;
				}
			}
			if(saveableIndex ==-1)
			{
				saveables.Grow().sourceFile = sourceManifest;
				saveableIndex = saveables.GetCount()-1;
			}
			saveables[saveableIndex].manifest.m_InteriorNames.PushAndGrow(m_Manifest.m_InteriorNames[i]);
		}
		for(int i=0;i<saveables.GetCount();i++)
		{
			SaveManifest(saveables[i].manifest,saveables[i].sourceFile.GetCStr());
		}
		m_ManifestNeedSave = false;

	}

	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		SaveRegion(i, resourceSaved);
	}
}


void CScenarioPointManager::AddWidgets(bkBank* bank)
{
	bank->PushGroup("Stats", false);
	const int maxNum = CScenarioPoint::GetPool()->GetSize();
	bank->AddSlider("Number of Attached Scenario Points", &m_NumScenarioPointsEntity, 0, maxNum, 0);
	bank->AddSlider("Number of World Scenario Points", &m_NumScenarioPointsWorld, 0, maxNum, 0);
	bank->PopGroup();

	m_ScenarioGroupsWidgetGroup = bank->PushGroup("Scenario Groups");
	bank->PopGroup();

	UpdateScenarioGroupWidgets();
}


void CScenarioPointManager::ClearScenarioGroupWidgets()
{
	if(!m_ScenarioGroupsWidgetGroup)
	{
		return;
	}

	while(m_ScenarioGroupsWidgetGroup->GetChild())
	{
		m_ScenarioGroupsWidgetGroup->Remove(*m_ScenarioGroupsWidgetGroup->GetChild());
	}
}


void CScenarioPointManager::UpdateScenarioGroupWidgets()
{
	ClearScenarioGroupWidgets();

	if(!m_ScenarioGroupsWidgetGroup)
	{
		return;
	}

	m_ScenarioGroupsWidgetGroup->AddTitle("Currently Enabled:");

	m_ScenarioGroupsWidgetGroup->AddSlider("Exclusively Enabled", &m_ExclusivelyEnabledScenarioGroupIndex, -1, 10000, 1);

	const int numGroups = GetNumGroups();
	for(int i = 0; i < numGroups; i++)
	{
		const CScenarioPointGroup& grp = GetGroupByIndex(i);
		m_ScenarioGroupsWidgetGroup->AddToggle(grp.GetName(), &m_GroupsEnabled[i], datCallback(CFA(CScenarioPointManager::GroupsEnabledChangedCB)));
	}
}

void CScenarioPointManager::GroupsEnabledChangedCB()
{
	CScenarioManager::CarGensMayNeedToBeUpdated();
}

//UpdateScenarioTypeIds
static void UpdateTypeIdsScenarioPointArray(atArray<CScenarioPoint *>& scenarioPoints, s32 count, const atArray<u32>& idMapping) 
{
	for (int p = 0; p < count; p++ )
	{
		Assert(scenarioPoints[p]);
		unsigned remapped = scenarioPoints[p]->GetScenarioTypeVirtualOrReal();
		remapped = SCENARIOINFOMGR.GetScenarioIndex(idMapping[remapped], true, true);
		scenarioPoints[p]->SetScenarioType(remapped);
	}
}

void CScenarioPointManager::UpdateScenarioTypeIds(const atArray<u32>& idMapping)
{
	//For all loaded regions
	int count = m_Manifest.m_RegionDefs.GetCount();
	for (int r = 0; r < count; r++ )
	{
		CScenarioPointRegion* region = GetRegion(r);
		if (region)
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			bankRegion.RemapScenarioTypeIds(idMapping);
		}
	}

	//For all attached points
	count = m_EntityPoints.GetCount();
	for (int p = 0; p < count; p++ )
	{
		Assert(m_EntityPoints[p]);
		unsigned remapped = m_EntityPoints[p]->GetScenarioTypeVirtualOrReal();
		remapped = SCENARIOINFOMGR.GetScenarioIndex(idMapping[remapped], true, true);
		m_EntityPoints[p]->SetScenarioType(remapped);
	}
	
	UpdateTypeIdsScenarioPointArray(m_EntityPoints, m_EntityPoints.GetCount(), idMapping);
	UpdateTypeIdsScenarioPointArray(m_WorldPoints, m_WorldPoints.GetCount(), idMapping);
}

void CScenarioPointManager::VerifyFileVersions()
{
	// Check the manifest file, if it's in PSO file format. Unlike for the scenario point files,
	// we would have already loaded it, so we just need to compare with what's in the unconverted file.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::SCENARIO_POINTS_PSO_FILE);
	if(DATAFILEMGR.IsValid(pData))
	{
		char filename[RAGE_MAX_PATH];
		if(sGenerateFilenameForManifestPsoSource(filename, sizeof(filename), pData->m_filename))
		{
			int unconvVersion = 0;
			if(ReadVersionNumber(filename, unconvVersion))
			{
				const int convVersion = m_Manifest.m_VersionNumber;
				if(unconvVersion > convVersion)
				{
					char buff[1024];
					formatf(buff, "The manifest file ('%s') has not been converted. It's not safe to edit and save scenario points. Continue anyway?",
							filename);
					if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
					{
						Quitf("Please right-click to convert the file ('%s') and try again.", filename);
					}
				}
			}
		}
	}

	// Initialize a buffer for listing files with errors.
	char mismatchListBuff[1024];
	mismatchListBuff[0] = '\0';
	char* pMismatchListBuffPtr = mismatchListBuff;
	int mismatchListSizeRemaining = sizeof(mismatchListBuff);

	static const int kMaxToList = 10;
	int numMismatches = 0;

	const char* levelName = CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetRequestedLevelIndex());
	char subDir[RAGE_MAX_PATH];
	formatf(subDir, "levels/%s/scenario", levelName);

	char rootPath[RAGE_MAX_PATH]={0};
	const char* pScenarioDir = NULL;
	if(PARAM_scenariodir.Get(pScenarioDir))
	{
		formatf(rootPath, "%s", pScenarioDir);
	}
	else
	{
		formatf(rootPath, "%sexport", CFileMgr::GetAssetsFolder());
	}

	// Loop over all the region files.
	for(int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		const char* pRegionName = m_Manifest.m_RegionDefs[i]->m_Name.GetCStr();

		// Try to load the unconverted file.
		char fileName[RAGE_MAX_PATH];
		formatf(fileName, "%s%s/%s.pso.meta", rootPath, subDir, ASSET.FileName(pRegionName));

		int unconvVersion = 0;
		if(ReadVersionNumber(fileName, unconvVersion))
		{
			// We found the unconverted file, load the converted one.
			ForceLoadRegion(i);

			char err[1024];
			err[0] = '\0';

			CScenarioPointRegion* pConverted = GetRegion(i);
			if(pConverted)
			{
				// We have both an unconverted and converted file, compare the version numbers.
				// If the unconverted version is higher than the converted one, we have a problem.
				// If the converted version is higher than the unconverted one, we're probably fine,
				// it probably just means that we haven't updated to the latest unconverted file -
				// this shouldn't be a source of data loss.
				CScenarioPointRegionEditInterface bankRegion(*pConverted);
				const int convVersion = bankRegion.GetVersionNumber();
				if(unconvVersion > convVersion)
				{
					formatf(err, "%s (unconv: %d, conv: %d)", ASSET.FileName(pRegionName), unconvVersion, convVersion);
				}
			}
			else
			{
				// If the converted file doesn't exist we should probably also generate an error,
				// because it could be that a new region was added but hasn't been converted, and
				// we wouldn't want to risk overwriting the unconverted file that apparently exists.
				formatf(err, "%s (converted file missing)", ASSET.FileName(pRegionName));
			}

			// If we got an error, add it to the buffer (up to a number), and dump it to TTY.
			if(err[0])
			{
				numMismatches++;
				if(numMismatches <= kMaxToList)
				{
					formatf(pMismatchListBuffPtr, mismatchListSizeRemaining, "    %s\n", err);
					const int numWritten = istrlen(pMismatchListBuffPtr);
					pMismatchListBuffPtr += numWritten;
					mismatchListSizeRemaining -= numWritten;
				}
				Displayf("%s", err);
			}

			// Unload the converted file again.
			g_fwMetaDataStore.RemoveLoaded(strLocalIndex(m_Manifest.m_RegionDefs[i]->m_SlotId));
		}
		else
		{
			// In this case, the unconverted file doesn't exist locally. That's probably not a problem.
		}
	
	}

	// If we found any errors, ask the user if they really want to proceed.
	if(numMismatches)
	{
		char buff[1024];
		formatf(buff, "%d scenario files in '%s' have not been converted:\n%s\nIt's not safe to edit and save scenario points. Continue anyway?",
				numMismatches, subDir, mismatchListBuff);
		if(bkMessageBox("Scenario Editor", buff, bkMsgOkCancel, bkMsgQuestion) != 1)
		{
			Quitf("Please convert the files and try again.");
		}
	}
}

bool CScenarioPointManager::ReadVersionNumber(const char* fileName, int& versionNumberOut)
{
	sm_VersionCheckVersionNumber = 0;
	parStreamIn* pStreamIn = PARSER.OpenInputStream(fileName, "");
	if(pStreamIn)
	{
		pStreamIn->SetBeginElementCallback(parStreamIn::BeginElementCB(&VersionCheckOnBeginCB));
		pStreamIn->SetEndElementCallback(parStreamIn::EndElementCB(&VersionCheckOnEndCB));
		pStreamIn->SetDataCallback(parStreamIn::DataCB(&VersionCheckOnDataCB));
		pStreamIn->ReadWithCallbacks();

		pStreamIn->Close();
		delete pStreamIn;

		versionNumberOut = sm_VersionCheckVersionNumber;
		sm_VersionCheckVersionNumber = 0;
		return true;
	}
	return false;
}

void CScenarioPointManager::VersionCheckOnBeginCB(parElement& elt, bool isLeaf)
{
	if(!isLeaf || sm_VersionCheckVersionNumber != 0)
	{
		return;
	}

	const char* eltName = elt.GetName();
	if(strcmp(eltName, "VersionNumber") == 0)
	{
		sm_VersionCheckVersionNumber = elt.FindAttributeIntValue("value", 0, true);
	}
}

void CScenarioPointManager::VersionCheckOnEndCB(bool /*isLeaf*/)
{
}

void CScenarioPointManager::VersionCheckOnDataCB(char* /*data*/, int /*size*/, bool /*dataIncomplete*/)
{
}

CScenarioPointGroup& CScenarioPointManager::GetGroupByIndex(int groupIndex)
{	
	Assert(groupIndex >=0);
	Assert(groupIndex < m_Manifest.m_Groups.GetCount());
	return *m_Manifest.m_Groups[groupIndex];
}

CScenarioPointGroup& CScenarioPointManager::AddGroup(const char* name)
{
	ClearScenarioGroupWidgets();
	CScenarioPointGroup* grp = rage_new CScenarioPointGroup;
	m_GroupsEnabled.Grow() = grp->GetEnabledByDefault();
	grp->SetName(name);
	m_Manifest.m_Groups.PushAndGrow(grp);
	m_groupSourceMap.SafeInsert(atHashString(name),atHashString(m_manifestSources[m_sWorkingManifestIndex]));
	m_groupSourceMap.FinishInsertion();
	UpdateScenarioGroupWidgets();
	SetManifestNeedSave();//Manifest needs to be saved
	return *grp;
}

void CScenarioPointManager::DeleteGroupByIndex(int index)
{
	ClearScenarioGroupWidgets();
	if(m_groupSourceMap.Has(m_Manifest.m_Groups[index]->GetNameHashString()))
		m_groupSourceMap.Remove(m_groupSourceMap.GetIndexFromDataPtr(m_groupSourceMap.Access(m_Manifest.m_Groups[index]->GetNameHashString())));
	delete m_Manifest.m_Groups[index];
	m_Manifest.m_Groups.Delete(index);
	m_GroupsEnabled.Delete(index);

	//Need to update all the group IDs for anything that is loaded.
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		CScenarioPointRegion* region = GetRegion(i);
		if (region)
		{
			CScenarioPointRegionEditInterface bankRegion(*region);
			bankRegion.AdjustGroupIdForDeletedGroupId((u8)index);
		}
	}

	UpdateScenarioGroupWidgets();
	SetManifestNeedSave();//Manifest needs to be saved
}

void CScenarioPointManager::RenameGroup(u32 oldGroupID, u32 newGroupID)
{
	//This does some assumptions about what you want to have happen and will
	// stream in all known regions to execute the rename operation ... 
	// NOTE: need to make sure that all regions are checked out for this kind of operation.
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		bool forced = false;
		CScenarioPointRegion* region = GetRegion(i);
		if (!region)
		{
			ForceLoadRegion(i);
			region = GetRegion(i);
			forced = true;
		}
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		//Execute the rename ... 
		bool renamed = bankRegion.ChangeGroup((u8)oldGroupID, (u8)newGroupID);

		if (forced && !renamed)
		{
			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.ClearRequiredFlag(m_Manifest.m_RegionDefs[i]->m_SlotId, STRFLAG_DONTDELETE);

			//Need to release it right away because not doing so could fill the 
			// pool and cause other regions to not be able to be loaded.
			g_fwMetaDataStore.RemoveUnrequired();
		}
		else if (renamed)
		{
			//If we changed any group data then we need to keep this region around an mark it as edited.
			FlagRegionAsEdited(i);
		}
	}
	SetManifestNeedSave();//Manifest needs to be saved
}

void CScenarioPointManager::ResetGroupForUsersOf(u32 groupName)
{
	int groupId = FindGroupByNameHash(groupName);
	//This does some assumptions about what you want to have happen and will
	// stream in all known regions to execute the rename operation ... 
	// NOTE: need to make sure that all regions are checked out for this kind of operation.
	for (int i = 0; i < m_Manifest.m_RegionDefs.GetCount(); i++)
	{
		bool forced = false;
		CScenarioPointRegion* region = GetRegion(i);
		if (!region)
		{
			ForceLoadRegion(i);
			region = GetRegion(i);
			forced = true;
		}
		Assert(region);
		CScenarioPointRegionEditInterface bankRegion(*region);

		//Execute the clearing ... 
		bool changed = bankRegion.ResetGroupForUsersOf((u8)groupId);

		if (forced && !changed)
		{
			Assert(m_Manifest.m_RegionDefs[i]->m_SlotId != -1);
			g_fwMetaDataStore.ClearRequiredFlag(m_Manifest.m_RegionDefs[i]->m_SlotId, STRFLAG_DONTDELETE);

			//Need to release it right away because not doing so could fill the 
			// pool and cause other regions to not be able to be loaded.
			g_fwMetaDataStore.RemoveUnrequired();
		}
		else if (changed)
		{
			//If we changed any group data then we need to keep this region around an mark it as edited.
			FlagRegionAsEdited(i);
		}
	}
	SetManifestNeedSave();//Manifest needs to be saved
}

void CScenarioPointManager::RestoreGroupFromUndoData(int groupIndex, const CScenarioPointGroup& undoData)
{
	CScenarioPointGroup& dest = GetGroupByIndex(groupIndex);

	dest.SetEnabledByDefault(undoData.GetEnabledByDefault());

	Assert(dest.GetNameHash() == undoData.GetNameHash());
}

void CScenarioPointManager::RenderPointPopularity(const atArray<CScenarioPoint*>& pointArray, u32 numToShow, Color32 color) const
{
	atArray<PopularScenarioType> sortedArray;
	GatherPopularUsedScenarioTypes(pointArray, &sortedArray);

	static const u32 txtSz = 64;
	char ptype[txtSz];

	for (u32 t = 0; t < numToShow; t++)
	{
		//Dont try to render more than we have ... 
		if (t >= sortedArray.GetCount())
		{
			break;
		}

		if (sortedArray[t].m_TimesPresent != 0)
		{
			formatf(ptype, txtSz, "    %s{%d}", SCENARIOINFOMGR.GetNameForScenario(sortedArray[t].m_Index), sortedArray[t].m_TimesPresent);
			grcDebugDraw::AddDebugOutputEx(false, color, ptype);
		}
	}
}

int CScenarioPointManager::PopularScenarioType::SortByCountCB(const PopularScenarioType* p_A, const PopularScenarioType* p_B)
{
	return (p_A->m_TimesPresent > p_B->m_TimesPresent) ? -1 : 1;
}

int CScenarioPointManager::PopularScenarioType::SortByTypeNameNoCaseCB(const PopularScenarioType* p_A, const PopularScenarioType* p_B)
{
	Assert(p_A->m_Index != 0XFFFFFFFF);
	Assert(p_B->m_Index != 0XFFFFFFFF);

	return stricmp(SCENARIOINFOMGR.GetNameForScenario(p_A->m_Index), SCENARIOINFOMGR.GetNameForScenario(p_B->m_Index));
}

void CScenarioPointManager::GatherPopularUsedScenarioTypes(const atArray<CScenarioPoint*>& pointArray, atArray<PopularScenarioType>* out_sortedArray) const
{
	atArray<PopularScenarioType>& arrayRef = *out_sortedArray;

	for (int p = 0; p < pointArray.GetCount(); p++)
	{
		Assert(pointArray[p]);
		u32 type = pointArray[p]->GetScenarioTypeVirtualOrReal();

		//see if this type is already logged.
		bool found = false;
		for (int fna = 0; fna < arrayRef.GetCount(); fna++)
		{
			if (arrayRef[fna].m_Index == type)
			{
				arrayRef[fna].m_TimesPresent++;
				found = true;
			}
		}

		if (!found)
		{
			PopularScenarioType temp;
			temp.m_Index = type;
			temp.m_TimesPresent = 1;
			arrayRef.PushAndGrow(temp);
		}
	}

	arrayRef.QSort(0, -1, PopularScenarioType::SortByCountCB);
}

#endif // SCENARIO_DEBUG


///////////////////////////////////////////////////////////////////////


CScenarioPointPriorityManager CScenarioPointPriorityManager::sm_Instance;

// Called by script to change the priority of a given scenario point.
void CScenarioPointPriorityManager::ForceScenarioPointGroupPriority(u16 iGroup, bool bIsHigh)
{
	int iIndex = m_aAlteredGroups.Find(iGroup);
	if (iIndex == -1)
	{
		// Not found so append if possible
		if (Verifyf(!m_aAlteredGroups.IsFull(), "Already forcing priority for the maximum number of groups."))
		{
			m_aAlteredGroups.Append() = iGroup;
			m_aForcedGroupPriorities.Append() = bIsHigh;
		}
	}
	else
	{
		// Group was already being changed to some value - force it to the new value
		m_aForcedGroupPriorities[iIndex] = bIsHigh;
	}
}

bool CScenarioPointPriorityManager::CheckPriority(const CScenarioPoint& rPoint) 
{ 
	return m_aAlteredGroups.GetCount() == 0 ? rPoint.IsTrulyHighPriority() : CheckScenarioPointGroupPriority(rPoint); 
}

bool CScenarioPointPriorityManager::CheckScenarioPointGroupPriority(const CScenarioPoint& rPoint) const
{
	unsigned iGroup = rPoint.GetScenarioGroup();
	if (iGroup == CScenarioPointManager::kNoGroup)
	{
		// Point has no group, return its basic priority value
		return rPoint.IsTrulyHighPriority();
	}
	else
	{
		int iIndex = m_aAlteredGroups.Find((u16)iGroup);
		if (iIndex != -1)
		{
			return m_aForcedGroupPriorities[iIndex];
		}
		else
		{
			// Point has no managed group, return its basic priority value.
			return rPoint.IsTrulyHighPriority();
		}
	}
}

// Clear the groups that are being forced to a specific priority.
void CScenarioPointPriorityManager::RestoreGroupsToOriginalPriorities()
{
	m_aAlteredGroups.Reset();
	m_aForcedGroupPriorities.Reset();
}
