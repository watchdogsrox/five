//
// scene/loader/MapData.h
//
// Copyright (C) 1999-2010 Rockstar Games. All Rights Reserved.
//

#include "mapData.h"

// rage includes
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/msgbox.h"
#include "bank/text.h"
#include "diag/art_channel.h"
#include "parser/manager.h"
#include "grprofile/ekg.h"
#include "grprofile/profiler.h"
#include "profile/group.h"
#include "vector/vector4.h"

#include "fwsys/fileExts.h"
#include "fwsys/metadatastore.h"
#include "fwscene/stores/mapdatastore.h"

#include "MapData_Extensions.h"
#include "MapData_Interiors.h"
#include "MapData_Buildings.h"
#include "MapData_Composite.h"

#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/BlockView.h"
#include "Objects/object.h"
#include "objects/DummyObject.h"
#include "cloth/clothmgr.h"
#include "modelinfo/basemodelinfo.h" 
#include "modelinfo/compentitymodelinfo.h" 
#include "modelinfo/modelinfo.h"
#include "modelinfo/modelinfo_factories.h"
#include "peds/pedpopulation.h"
#include "scene/fileloader.h"
#include "scene/MapChange.h"
#include "scene/scene_channel.h"
#include "scene/world/GameWorld.h"
#include "scene/world/VisibilityMasks.h"
#include "renderer/occlusion.h"
#include "timecycle/timecycle.h"
#include "vehicles/cargen.h"
#include "pathserver/ExportCollision.h"
#include "pathserver/NavGenParam.h"

#include "MapData_parser.h" 

#if __ASSERT
#include "diag/art_channel.h"
#include "modelinfo/TimeModelInfo.h"
#endif

#define USE_SCENELOD_SYSTEM_FOR_INTERIORS (0)
 
SCENE_OPTIMISATIONS();

XPARAM(nolodlights);

#if __BANK
CMapDataGameInterface g_MapDataGameInterface;
#endif	//__BANK

#if __BANK

// Profiling
PF_PAGE(CMapDataContents, "Map data (GTA)");

PF_GROUP(Create);
PF_TIMER(EntityCreate, Create);
PF_TIMER(EntityAddToWorld, Create);
PF_TIMER(EntitiesPostAdd, Create);
PF_TIMER(Cargens, Create);
PF_TIMER(TimeCycs, Create);
PF_TIMER(Occluders, Create);

PF_GROUP(Destroy);
PF_TIMER(CleanPools, Destroy);
PF_TIMER(EntityRemoveFromWorld, Destroy);
PF_TIMER(EntitiesPostRemove, Destroy);
PF_TIMER(EntitiesDestroy, Destroy);
PF_TIMER(Des_Cargens, Destroy);
PF_TIMER(Des_TimeCycs, Destroy);
PF_TIMER(Des_Occluders, Destroy);

PF_LINK(CMapDataContents, Create);
PF_LINK(CMapDataContents, Destroy);

bool						CMapDataContents::bEkgEnabled = false;
#endif //__BANK

CMapData::~CMapData()
{
	m_carGenerators.Reset();
	m_timeCycleModifiers.Reset();
	m_boxOccluders.Reset();
	m_occludeModels.Reset();
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DestroyOccluders
// PURPOSE:		Destroy occluders - workaround to make sure that temp 
//				allocs for byteswap are deleted. 
//////////////////////////////////////////////////////////////////////////
void CMapData::DestroyOccluders()
{
	// clean up any occluder models that may still be hanging around
	for(int i = 0; i < m_occludeModels.GetCount(); i++)
	{
		m_occludeModels[i].Destroy();
	}
}

PARAM(simplemapdata, ""); // for heightmap/navmesh tools, we don't need map data contents beyond just the entities

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Load
// PURPOSE:		create all entities etc belonging to specified map data def
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::Load(fwMapDataDef* pDef, fwMapData* pMapData, strLocalIndex mapDataSlotIndex)
{
	Assert(pDef);

	if ( !pDef->RequiresTempMemory() )
	{
		SetMapData(pMapData);
	}

	// creation of entities inc lod attachment, and process any pending registered model swaps etc
	PF_FUNC(EntityCreate);
	Entities_Create(pMapData, pDef->GetParentContents(), mapDataSlotIndex.Get());
	if (pDef->GetNumMapChanges()>0)
		g_MapChangeMgr.ApplyChangesToMapData(mapDataSlotIndex.Get());

	PF_FUNC(EntityAddToWorld);
	Entities_AddToWorld(pMapData, pDef->GetPostponeWorldAdd(), pDef->IsCullPermitted());
	
	// run CGameWorld::PostAdd on each entity
	//if (pMapData->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD)
	{
		Entities_PostAdd(mapDataSlotIndex.Get(), pMapData->GetContentFlags()&fwMapData::CONTENTFLAG_ENTITIES_HD);
	}

	if (PARAM_simplemapdata.Get())
	{
		return;
	}

	// creation of car gens, timecycle modifier boxes, occluders
	CMapData* pGameMapData = smart_cast<CMapData*>(pMapData);

	CarGens_Create(pGameMapData, mapDataSlotIndex.Get());
	TimeCycs_Create(pGameMapData, mapDataSlotIndex.Get());
	Occluders_Create(pGameMapData, mapDataSlotIndex.Get());

	//PF_FUNC(LODLights);
	LODLights_Create(pGameMapData, mapDataSlotIndex.Get());
	DistantLODLights_Create(pGameMapData, mapDataSlotIndex.Get());

#if __ASSERT
	if (pDef->RequiresDebugValidation() && (pMapData->GetContentFlags()&fwMapData::CONTENTFLAG_MASK_ENTITIES) != 0)
	{
		DebugPostLoadValidation();
		pDef->SetRequiresDebugValidation(false);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Remove
// PURPOSE:		remove all entities etc belonging to specified map data def
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::Remove(strLocalIndex mapDataSlotIndex)
{
	fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(mapDataSlotIndex));

	PF_FUNC(CleanPools);
	//1. removal of related CObjects - this is (still) pretty awful...
	CObject::Pool* m_pObjectPool = CObject::GetPool();
	for(s32 i=0; i<m_pObjectPool->GetSize(); i++)
	{
		CEntity* pEntity = m_pObjectPool->GetSlot(i);
		if(pEntity && pEntity->GetIplIndex()==mapDataSlotIndex.Get())
		{
			CObject* m_pObject = (CObject*)pEntity;

			Assertf(m_pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT, "CMapDataContents::Remove - removing a script object <%s>. It could be an object grabbed from the world by GET_CLOSEST_OBJECT_OF_TYPE", m_pObject->GetBaseModelInfo()->GetModelName());

			if(m_pObject->GetRelatedDummy())
			{
				CDummyObject* pDummyObject = m_pObject->GetRelatedDummy();
				Assert(m_pObject->GetIplIndex() == pDummyObject->GetIplIndex());
				pDummyObject->Enable();
				m_pObject->ClearRelatedDummy();
			}
			else
			{
				Assertf(m_pObject->GetOwnedBy() != ENTITY_OWNEDBY_RANDOM,"object <%s> not owned by RANDOM - owned by %d",m_pObject->GetBaseModelInfo()->GetModelName(), m_pObject->GetOwnedBy());
			}
			Assert(m_pObject->GetOwnedBy() != ENTITY_OWNEDBY_FRAGMENT_CACHE);
			CObjectPopulation::DestroyObject(m_pObject);		// Call this so that the network code can deal with object being removed
		}	
	}

	//2. remove entities from world
	//3. run CGameWorld::PostRemove
	//4. destroy entities
	PF_FUNC(EntityRemoveFromWorld);
	Entities_RemoveFromWorld();

	PF_FUNC(EntitiesPostRemove);
	if (pDef->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD)
	{
		Entities_PostRemove();
	}

	PF_FUNC(EntitiesDestroy);
	Entities_Destroy();

	//5. destroy car gens
	//6. destroy time cycle modifiers
	PF_FUNC(Des_Cargens);
	CarGens_Destroy(mapDataSlotIndex.Get());

	PF_FUNC(Des_TimeCycs);
	TimeCycs_Destroy(mapDataSlotIndex.Get());

	PF_FUNC(Des_Occluders);
	Occluders_Destroy(mapDataSlotIndex.Get());

	//PF_FUNC(Des_LODLights);
	LODLights_Destroy(mapDataSlotIndex.Get());
	DistantLODLights_Destroy(mapDataSlotIndex.Get());
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Entities_PostAdd
// PURPOSE:		perform game-side steps after entities have been added to world
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::Entities_PostAdd(s32 mapDataSlotIndex, bool bIsHighDetail)
{
	PF_FUNC(EntitiesPostAdd);

	fwEntity** apEntities = GetEntities();
	for (s32 i=0; i<GetNumEntities(); i++)
	{
		CEntity* pEntity = (CEntity*)apEntities[i];
		if (pEntity)
		{
			pEntity->SetIplIndex(mapDataSlotIndex);	// required for debug(blockview) and for dummy/object management
			if (bIsHighDetail)
			{
				CGameWorld::PostAdd(pEntity);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Entities_PostRemove
// PURPOSE:		perform game-side steps after entities have been removed from world
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::Entities_PostRemove()
{
	const int maxEntities = 512;
	CEntity* scenarioPointEntities[maxEntities];
	u32 entityCount = 0;

	fwEntity** apEntities = GetEntities();
	for(s32 i=0; i<GetNumEntities(); i++)
	{
		CEntity* pEntity = (CEntity*)apEntities[i];
		if (pEntity)
		{
			Assert(!pEntity->GetOwnerEntityContainer());
			//don't test scenario points as we'll do this right here
			CGameWorld::PostRemove(pEntity, false, false);

			if(pEntity->m_nFlags.bHasSpawnPoints && !CGameWorld::IsEntityBeingAddedAndRemoved(pEntity)) 
			{
				if(Verifyf(entityCount < maxEntities, "More than %d spawnpoint entities in CMapDataContents::Entities_PostRemove GetNumEnt:%d. This will leave scenario points with dangling pointers and will probably crash.", entityCount, GetNumEntities()))
				{
					scenarioPointEntities[entityCount++] = pEntity;
				}
			}
		}
	}

	if(entityCount > 0)
	{
		if(entityCount == 1)
		{
			CPedPopulation::RemoveScenarioPointsFromEntity(scenarioPointEntities[0]);
		}
		else
		{
			//batch removal
			CPedPopulation::RemoveScenarioPointsFromEntities(scenarioPointEntities, entityCount);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CarGens_Create
// PURPOSE:		create car generators for specified mapdata
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::CarGens_Create(CMapData* pMapData, s32 mapDataSlotIndex)
{
	PF_FUNC(Cargens);

	const int count = pMapData->m_carGenerators.GetCount();
	for (u32 i=0; i<count; i++)
	{
		CCarGen* carGenerator = &(pMapData->m_carGenerators[i]);

		Assertf(((carGenerator->m_flags & 0x0F000000) >> 24) == 0,
				"Car generator at (%.1f, %.1f, %.1f) has scenario type %d specified through map data, this is not supported.",
				carGenerator->m_position.x,
				carGenerator->m_position.y,
				carGenerator->m_position.z,
				(int)((carGenerator->m_flags & 0x0F000000) >> 24)
				);

		s32 newCarGenIndex = CTheCarGenerators::CreateCarGenerator(
			carGenerator->m_position.x,
			carGenerator->m_position.y,
			carGenerator->m_position.z,
			carGenerator->m_orientX,
			carGenerator->m_orientY,
			carGenerator->m_perpendicularLength,
			carGenerator->m_carModel.GetHash(),
			carGenerator->m_bodyColorRemap1,
			carGenerator->m_bodyColorRemap2,
			carGenerator->m_bodyColorRemap3,
			carGenerator->m_bodyColorRemap4,
			/*carGenerator->m_bodyColorRemap5*/0,
			/*carGenerator->m_bodyColorRemap6*/0,
			carGenerator->m_flags,
			mapDataSlotIndex,
			false,
			((carGenerator->m_flags & 0xF0000000) >> 28),
			CARGEN_SCENARIO_NONE,
			carGenerator->m_popGroup,
			carGenerator->m_livery,
			/*livery2*/(s8)-1,
			false,
			true);

		if (newCarGenIndex >= 0)
		{
			CCarGenerator* carGenerator = CTheCarGenerators::GetCarGenerator(newCarGenIndex);
			carGenerator->SwitchOn();
			carGenerator->SwitchOffIfWithinSwitchArea();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CarGens_Destroy
// PURPOSE:		destroy car generators owned by specified mapdata
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::CarGens_Destroy(s32 mapDataSlotIndex)
{
	CTheCarGenerators::RemoveCarGenerators(mapDataSlotIndex);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	TimeCycs_Create
// PURPOSE:		creates time cycle modifier boxes for specified mapdata
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::TimeCycs_Create(CMapData* pMapData, s32 mapDataSlotIndex)
{
	PF_FUNC(TimeCycs);

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		return;
#endif

	const int count = pMapData->m_timeCycleModifiers.GetCount();
	if (count)
	{
		Assert(sysThreadType::IsUpdateThread());
		for (u32 i=0; i<count; i++)
		{
			const CTimeCycleModifier& timeCycleModifier =  pMapData->m_timeCycleModifiers[i];
			spdAABB aabb;
			aabb.Set(RCC_VEC3V(timeCycleModifier.m_minExtents), RCC_VEC3V(timeCycleModifier.m_maxExtents));

			const char* debugTxt = NULL;
	#if __DEV
			char msg[512];
			sprintf(msg,"%s - %s",pMapData->GetName().GetCStr(),timeCycleModifier.m_name.GetCStr());
			debugTxt = msg;
	#endif
			g_timeCycle.AddModifierBox(
				aabb,
				timeCycleModifier.m_name.GetHash(),
				timeCycleModifier.m_percentage,
				timeCycleModifier.m_range,
				timeCycleModifier.m_startHour,
				timeCycleModifier.m_endHour,
				mapDataSlotIndex,
				debugTxt
				);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	TimeCycs_Destroy
// PURPOSE:		destroy time cycle modifier boxes owned by this mapdata
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::TimeCycs_Destroy(s32 mapDataSlotIndex)
{
	Assert(sysThreadType::IsUpdateThread());
	g_timeCycle.RemoveModifierBoxes(mapDataSlotIndex);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Occluders_Create
// PURPOSE:		create occluders for specified mapdata
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::Occluders_Create(CMapData* pMapData, s32 mapDataSlotIndex)
{
	PF_FUNC(Occluders);

	if (pMapData->m_boxOccluders.GetCount() > 0)
	{
		COcclusion::AddBoxOccluder(pMapData->m_boxOccluders, mapDataSlotIndex);
	}

	if (pMapData->m_occludeModels.GetCount() > 0)
	{
		COcclusion::AddOccludeModel(pMapData->m_occludeModels, mapDataSlotIndex);

#if __DEV
		for (int i = 0; i < pMapData->m_occludeModels.GetCount(); i++)
		{
			const OccludeModel& model = pMapData->m_occludeModels[i];
			const u32 flags = model.GetFlags();

			if (flags & ~1UL)
			{
				const atString assetPath = AssetAnalysis::GetAssetPath(g_MapDataStore, mapDataSlotIndex);
				const char* path = assetPath.c_str() + strlen("x:/gta5/assets/unzipped/");

				Assertf(0, "### occluder %s[%d] has invalid m_flags (0x%08x)", path, i, flags);
			}
			else if (0)
			{
				const atString assetPath = AssetAnalysis::GetAssetPath(g_MapDataStore, mapDataSlotIndex);
				const char* path = assetPath.c_str() + strlen("x:/gta5/assets/unzipped/");
				const Vec3V bmin = model.GetMin();
				const Vec3V bmax = model.GetMax();

				Displayf("occluder %s[%d] m_flags=%d (verts=%d, bounds=[%.2f,%.2f,%.2f]..[%.2f,%.2f,%.2f]", path, i, flags, model.GetNumVerts(), bmin.GetXf(), bmin.GetYf(), bmin.GetZf(), bmax.GetXf(), bmax.GetYf(), bmax.GetZf());
			}
		}
#endif // __DEV
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Occluders_Destroy
// PURPOSE:		destroys occluders owned by this mapdata
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::Occluders_Destroy(s32 mapDataSlotIndex)
{
	COcclusion::RemoveBoxOccluder(mapDataSlotIndex);
	COcclusion::RemoveOccludeModel(mapDataSlotIndex);

	fwMapData* mapdata = GetMapData();
	if (mapdata)
	{
		mapdata->DestroyOccluders();
	}
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LODLights_Create
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::LODLights_Create(CMapData *pMapData, s32 mapDataSlotIndex)
{
	if( pMapData->m_LODLightsSOA.m_direction.GetCount() )
	{
		CLODLightManager::AddLODLight(pMapData->m_LODLightsSOA, mapDataSlotIndex, fwMapDataStore::GetStore().GetParentSlot(strLocalIndex(mapDataSlotIndex)).Get());
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LODLights_Destroy
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::LODLights_Destroy(s32 mapDataSlotIndex)
{
	CLODLightManager::RemoveLODLight(mapDataSlotIndex);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DistantLODLights_Create
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::DistantLODLights_Create(CMapData *pMapData, s32 mapDataSlotIndex)
{
	//if( pMapData->GetContentFlags() & rage::fwMapData::CONTENTFLAG_DISTANT_LOD_LIGHTS )
	if(pMapData->m_DistantLODLightsSOA.m_position.GetCount())
	{
		CLODLightManager::AddDistantLODLight(pMapData->m_DistantLODLightsSOA, mapDataSlotIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DistantLODLights_Destroy
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::DistantLODLights_Destroy(s32 mapDataSlotIndex)
{
	CLODLightManager::RemoveDistantLODLight(mapDataSlotIndex);
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ConstructInteriorProxies
// PURPOSE:		store a list of the MLO instances in the world (but wait until all type data finished loading!)
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::ConstructInteriorProxies(fwMapDataDef* UNUSED_PARAM(pDef), fwMapData* pMapData, strLocalIndex mapDataSlotIndex)
{
	for (s32 i=0; i<pMapData->m_entities.GetCount(); i++)
	{
		fwEntityDef* pEntityDef = pMapData->m_entities[i];
		bool bIsMloInst = pEntityDef->parser_GetStructure()->GetNameHash() == atLiteralStringHash("CMloInstanceDef");
		if (bIsMloInst)
		{

			fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(mapDataSlotIndex));
			// intercept interiors setup with .ityp dependencies here
			if (pDef->GetNumParentTypes() != 0)
			{
				bool bAlreadyCreated = (CInteriorProxy::FindProxy(mapDataSlotIndex.Get()) != NULL);
				Assertf(!bAlreadyCreated, "%s : already populated from the cache!", pMapData->GetName().GetCStr());

				CMloInstanceDef* pMLOInstDef = static_cast<CMloInstanceDef*>(pEntityDef);
				spdAABB bbox;
				pMapData->GetPhysicalExtents(bbox);

				if (!bAlreadyCreated)
				{
					CInteriorProxy::Add(pMLOInstDef, mapDataSlotIndex, bbox);
				} else
				{
					// should use the new data in preference to the data from the cache
					CInteriorProxy::Overwrite(pMLOInstDef, mapDataSlotIndex, bbox);
				}

			} else {
				Assertf(false,"%s contains an interior, but no .ityp dependencies...", pMapData->GetName().GetCStr());
			}
		}
	}
}

#if	 __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LoadDebug
// PURPOSE:		construct game-specific debug info (eg block data)
//////////////////////////////////////////////////////////////////////////
void CMapDataContents::LoadDebug(fwMapDataDef* pDef, fwMapData* pMapData)
{
	// debug block info
	if (pDef && pDef->GetInitState()!=fwMapDataDef::FULLY_INITIALISED
		&& pMapData->GetContentFlags()&fwMapData::CONTENTFLAG_BLOCKINFO)
	{
		CMapData* pGameMapData = smart_cast<CMapData*>(pMapData);

		if (pGameMapData->m_block.m_name.GetLength() > 0)
		{
			s32 blockIndex = CBlockView::GetNumberOfBlocks();

			spdAABB bounds;
			pGameMapData->GetPhysicalExtents(bounds);
			const Vector3& vMin = bounds.GetMinVector3();
			const Vector3& vMax = bounds.GetMaxVector3();

			Vector3 avPoints[4];
			avPoints[0] = Vector3(vMin.x, vMax.y, 0.0f);
			avPoints[1] = Vector3(vMin.x, vMin.y, 0.0f);
			avPoints[2] = Vector3(vMax.x, vMax.y, 0.0f);
			avPoints[3] = Vector3(vMax.x, vMin.y, 0.0f);

			CBlockView::AddBlock(
				pGameMapData->m_block.m_name.c_str(),
				pGameMapData->m_block.m_exportedBy.c_str(),
				pGameMapData->m_block.m_owner.c_str(),
				pGameMapData->m_block.m_time.c_str(),
				pGameMapData->m_block.m_flags,
				4,
				avPoints
			);

			// write back block index...
			pDef->SetBlockIndex(blockIndex);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateBlockInfoFromCache
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CMapDataGameInterface::CreateBlockInfoFromCache(fwMapDataDef* pDef, fwMapDataCacheBlockInfo* pBlockInfo)
{
	Assert(pDef && pBlockInfo);

	const strLocalIndex slotIndex = strLocalIndex(fwMapDataStore::GetStore().GetSlotIndex(pDef));
	s32 blockIndex = CBlockView::GetNumberOfBlocks();

	const spdAABB& bounds = fwMapDataStore::GetStore().GetPhysicalBounds(slotIndex);
	const Vector3& vMin = bounds.GetMinVector3();
	const Vector3& vMax = bounds.GetMaxVector3();

	Vector3 avPoints[4];
	avPoints[0] = Vector3(vMin.x, vMax.y, 0.0f);
	avPoints[1] = Vector3(vMin.x, vMin.y, 0.0f);
	avPoints[2] = Vector3(vMax.x, vMax.y, 0.0f);
	avPoints[3] = Vector3(vMax.x, vMin.y, 0.0f);

	char tempstring[256];
	safecpy(tempstring,pBlockInfo->m_name_owner_exportedBy_time);
	char name[64];
	char exported[64];
	char owner[64];
	char time[64];

	char* p_Found = strtok(tempstring, ",");
	if(p_Found) safecpy(name, p_Found);
	else name[0] = 0;
	p_Found = strtok(NULL, ",");
	if(p_Found) safecpy(owner, p_Found);
	else owner[0] = 0;
	p_Found = strtok(NULL, ",");
	if(p_Found) safecpy(exported,p_Found);
	else exported[0] = 0;
	p_Found = strtok(NULL, ",");
	if(p_Found) safecpy(time, p_Found);
	else time[0] = 0;

	CBlockView::AddBlock(
		name,
		exported,
		owner,
		time,
		pBlockInfo->m_flags,
		4,
		avPoints
	);

	// write back block index...
	pDef->SetBlockIndex(blockIndex);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateCacheFromBlockInfo
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CMapDataGameInterface::CreateCacheFromBlockInfo(fwMapDataCacheBlockInfo& output, s32 blockIndex)
{
	const char* pszName = CBlockView::GetBlockName(blockIndex);
	const char* pszExportedBy = CBlockView::GetBlockUser(blockIndex);
	const char* pszOwner = CBlockView::GetBlockOwner(blockIndex);
	const char* pszTime = CBlockView::GetTime(blockIndex);
	sprintf(output.m_name_owner_exportedBy_time, "%s,%s,%s,%s", pszName, pszOwner, pszExportedBy, pszTime);
	output.m_flags = (u32) CBlockView::GetBlockFlags(blockIndex); 
}

#if __STATS
void CMapDataContents::ToggleEkgActive()
{
	if (bEkgEnabled)
	{
		GetRageProfileRenderer().GetEkgMgr(0)->SetPage(&PFPAGE_CMapDataContents);
	}
	else
	{
		GetRageProfileRenderer().GetEkgMgr(0)->SetPage(NULL);
	}
}
#endif // __STATS

void CMapDataContents::AddBankWidgets(bkBank* pBank)
{
	Assert(pBank);

	pBank->PushGroup( "MapDataContents", false );
	STATS_ONLY( pBank->AddToggle("Enable EKG Profiler", &bEkgEnabled, datCallback(ToggleEkgActive)); )
	pBank->PopGroup();
}

#endif	//__BANK

#if __ASSERT

void CMapDataContents::DebugPostLoadValidation()
{
	fwEntity** apEntities = GetEntities();
	for (s32 i=0; i<GetNumEntities(); i++)
	{
		CEntity* pEntity = (CEntity*)apEntities[i];
		if (pEntity && pEntity->IsBaseFlagSet(fwEntity::IS_TIMED_ENTITY) && pEntity->GetLodData().HasLod())
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if (pModelInfo)
			{
				CTimeInfo* pTimeInfo = pModelInfo->GetTimeInfo();
				if (pTimeInfo)
				{
					s32 hoursOnOff = pTimeInfo->GetHoursOnOffMask();
					CEntity* pLod = (CEntity*) pEntity->GetLod();
					if (pLod && pLod->IsBaseFlagSet(fwEntity::IS_TIMED_ENTITY))
					{
						CBaseModelInfo* pLodModelInfo = pLod->GetBaseModelInfo();
						CTimeInfo* pLodTimeInfo = pLodModelInfo->GetTimeInfo();
						artAssertf(hoursOnOff==pLodTimeInfo->GetHoursOnOffMask(),
							"Timed models: %s has LOD %s, but they have different on/off times. Please bug Default Maps",
							pEntity->GetModelName(), pLod->GetModelName());
					}

				}
			}
		}
	}
}

#endif	//__ASSERT
