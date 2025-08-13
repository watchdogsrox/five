//
// scene/MapChange.cpp
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#include "scene/MapChange.h"

#include "fwscene/mapdata/mapdatadef.h"
#include "fwscene/stores/mapdatastore.h"
#include "control/replay/replay.h"
#include "Objects/door.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "scene/world/GameWorld.h"
#include "script/gta_thread.h"
#include "renderer/Lights/LightEntity.h"

#if __BANK
#include "grcore/debugdraw.h"
#endif	//__BANK

CMapChangeMgr g_MapChangeMgr;

// world search callback and data struct
struct SSearchData
{
	u32 m_searchModel;
	CMapChange* m_pChange;
	atArray<CEntity*>* m_pEntities;
	bool bIncludeScriptEntities;
};
static bool FindMatchingCB(CEntity* pEntity, void* data)
{
	if (pEntity && data && pEntity->GetOwnedBy()!=ENTITY_OWNEDBY_FRAGMENT_CACHE)
	{
		SSearchData* pSearchData = (SSearchData*) data;
		if (pEntity->GetBaseModelInfo()->GetModelNameHash() == pSearchData->m_searchModel)
		{
			if (pEntity->GetOwnedBy()!=ENTITY_OWNEDBY_SCRIPT || pSearchData->bIncludeScriptEntities)
			{
				pSearchData->m_pEntities->PushAndGrow(pEntity);
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:		initializes map change and sets it to active
//////////////////////////////////////////////////////////////////////////
void CMapChange::Init(const u32 oldHash, const u32 newHash, const spdSphere& searchVolume, bool bSurviveMapReload, eType changeType, bool bAffectsScriptObjects)
{
	m_changeType = changeType;
	m_oldHash = oldHash;
	m_newHash = newHash;
	m_searchVolume = searchVolume;
	m_bScriptOwned = false;
	m_surviveMapReload = bSurviveMapReload;
	m_bAffectsHighDetailOnly = ( changeType!=TYPE_SWAP );
	m_bAffectsScriptObjects = bAffectsScriptObjects;
	m_bExecuted = false;

	// newHash is only meaningful for model swaps
	if (changeType!=TYPE_SWAP)
	{
		m_newHash = oldHash;
	}

	SetActive(true);

	m_mapDataSlots.Reset();

	// mark map data files which may require this change be applied on loading
	if (bSurviveMapReload)
	{
		fwMapDataStore& store = fwMapDataStore::GetStore();
		store.GetBoxStreamer().GetIntersectingAABB(spdAABB(searchVolume), fwBoxStreamerAsset::MASK_MAPDATA, m_mapDataSlots);

		const u32 contentMask = ( m_bAffectsHighDetailOnly ? fwMapData::CONTENTFLAG_ENTITIES_HD : fwMapData::CONTENTFLAG_MASK_ENTITIES );

		for (s32 i=0; i<m_mapDataSlots.GetCount(); i++)
		{
			fwMapDataDef* pDef = store.GetSlot(strLocalIndex((s32) m_mapDataSlots[i]));
			if (pDef && (pDef->GetContentFlags()&contentMask)!=0)
			{
				pDef->AddMapChange();
			}
			else
			{
				m_mapDataSlots.DeleteFast(i);
				i--;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Shutdown
// PURPOSE:		shuts down and de-registers map change
//////////////////////////////////////////////////////////////////////////
void CMapChange::Shutdown()
{
	fwMapDataStore& store = fwMapDataStore::GetStore();
	for (s32 i=0; i<m_mapDataSlots.GetCount(); i++)
	{
		fwMapDataDef* pDef = store.GetSlot(strLocalIndex((s32) m_mapDataSlots[i]));
		pDef->RemoveMapChange();
	}
	m_mapDataSlots.Reset();

	SetActive(false);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Matches
// PURPOSE:		returns true if equal
//////////////////////////////////////////////////////////////////////////
bool CMapChange::Matches(u32 oldHash, u32 newHash, const spdSphere& searchVolume, eType changeType) const
{
	return (oldHash==m_oldHash && newHash==m_newHash && searchVolume==m_searchVolume && changeType==m_changeType);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IncludesMapData
// PURPOSE:		returns true if specified map slot is included in list
//////////////////////////////////////////////////////////////////////////
bool CMapChange::IncludesMapData(s32 mapDataSlotIndex) const
{
	for (s32 i=0; i<m_mapDataSlots.GetCount(); i++)
	{
		if ((s32)m_mapDataSlots[i] == mapDataSlotIndex)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Execute
// PURPOSE:		executes a map change on the specified entity. returns true if successful
//////////////////////////////////////////////////////////////////////////
bool CMapChange::Execute(CEntity* pEntity, bool bReverse)
{
	if (!pEntity)
		return false;
	
	u32 oldModelHash = bReverse ? m_newHash : m_oldHash;
	u32 newModelHash = bReverse ? m_oldHash : m_newHash;

	if (pEntity->GetBaseModelInfo()->GetModelNameHash() != oldModelHash)
		return false;

	if (!m_searchVolume.IntersectsSphere(pEntity->GetBoundSphere()))
		return false;

#if GTA_REPLAY
	if(pEntity->GetIsTypeObject())
	{
		CReplayMgr::RecordMapObject((CObject*)pEntity);
	}
#endif // GTA_REPLAY

	// perform a map change on entity
	switch (m_changeType)
	{
	case TYPE_HIDE:
		{
			REPLAY_ONLY(if(pEntity->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
			{
				gnetDebug3("CMapChange::Execute is hiding entity: 0x%p[%s]", pEntity, pEntity->GetLogName());
				pEntity->SetHidden(!bReverse);
			}
		}
		break;
	case TYPE_SWAP:
		{
			fwModelId newModelId;
			CModelInfo::GetBaseModelInfoFromHashKey(newModelHash, &newModelId);		//	ignores return value
			if (newModelId.IsValid())
			{
				pEntity->SwapModel(newModelId.GetModelIndex());
			}
		}
		break;
	case TYPE_FORCEOBJ:
		{
			pEntity->m_nFlags.bNeverDummy = !bReverse;
		}
		break;
	default:
		break;
	}
	
	m_bExecuted = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Execute
// PURPOSE:		executes a map change using world search
//////////////////////////////////////////////////////////////////////////
void CMapChange::Execute(bool reverse)
{
	atArray<CEntity*> entities;
	FindEntitiesFromWorldSearch(entities, m_newHash);
	for (s32 j=0; j<entities.GetCount(); j++)
		Execute(entities[j], reverse);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindEntitiesFromWorldSearch
// PURPOSE:		performs a world search to find any entities which
//				match the search params
//////////////////////////////////////////////////////////////////////////
void CMapChange::FindEntitiesFromWorldSearch(atArray<CEntity*>& entityList, u32 searchModel)
{
	SSearchData searchData;
	searchData.m_pEntities = &entityList;
	searchData.m_searchModel = searchModel;
	searchData.m_pChange = this;
	searchData.bIncludeScriptEntities = m_bAffectsScriptObjects;

	fwIsSphereIntersecting searchSphere(m_searchVolume);
	CGameWorld::ForAllEntitiesIntersecting(&searchSphere, FindMatchingCB, (void*) &searchData, 
		ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT|ENTITY_TYPE_MASK_ANIMATED_BUILDING,
		SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS,
		( m_bAffectsHighDetailOnly ? SEARCH_LODTYPE_HIGHDETAIL : SEARCH_LODTYPE_ALL ),
		SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_SCRIPT);

	// for any objects in list, check if they have a non-searchable related dummy that also
	// needs to be changed
	atArray<CEntity*> relatedDummyList;
	for (s32 i=0; i<entityList.GetCount(); i++)
	{
		CEntity* pEntity = entityList[i];
		if (pEntity && pEntity->GetIsTypeObject())
		{
			CObject* pObject = (CObject*) pEntity;
			CDummyObject* pDummyObject = pObject->GetRelatedDummy();
			if (pDummyObject && !pDummyObject->GetIsSearchable())
			{
				relatedDummyList.PushAndGrow(pDummyObject);
			}
		}
	}
	for (s32 i=0; i<relatedDummyList.GetCount(); i++)
	{
		entityList.PushAndGrow(relatedDummyList[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:		clears any active changes
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::Reset()
{
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		m_activeChanges[i].Shutdown();
	}
	m_activeChanges.Reset();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Add
// PURPOSE:		registers a new map change, and executes it immediately if required
//////////////////////////////////////////////////////////////////////////
CMapChange* CMapChangeMgr::Add(u32 oldHash, u32 newHash, const spdSphere& searchVol, bool bSurviveMapReload, CMapChange::eType changeType, bool bLazy, bool bIncludeScriptObjects)
{
	if (AlreadyExists(oldHash, newHash, searchVol, changeType))
		return NULL;

	CMapChange& newChange = m_activeChanges.Grow();
	newChange.Init(oldHash, newHash, searchVol, bSurviveMapReload, changeType, bIncludeScriptObjects);

	if (!bLazy)
	{
		// for each entity, perform map change
		atArray<CEntity*> entities;
		newChange.FindEntitiesFromWorldSearch(entities, oldHash);
		for (s32 i=0; i<entities.GetCount(); i++)
		{
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive() && CReplayMgr::ShouldPreventMapChangeExecution(entities[i]))
			{
				continue;
			}
#endif // GTA_REPLAY

			// bugstar://7713222 - Error: Incorrect 2deffect index when accessing light
			//	Calls to CommandCreateModelSwap which refer to entities in the LightEntity pool could possibly
			//	swap to a drawable that doesn't have any lights, yet retain their slot in the pool. We explicitly
			//	remove the entity from the pool (if it's in there) and let the call to CEntity::CreateDrawable()
			//	add it back into the pool if necessary.
			if ( changeType == CMapChange::TYPE_SWAP )
			{
				LightEntityMgr::RemoveAttachedLights(entities[i]);
			}

			newChange.Execute(entities[i]);
		}
	}

	// inform the door system in case this model is a door model
	if (changeType == CMapChange::TYPE_SWAP)
	{
		Vector3 center = VEC3V_TO_VECTOR3(searchVol.GetCenter());
		CDoorSystem::SwapModel(oldHash, newHash, center, searchVol.GetRadiusSquaredf());
	}

	return &newChange;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Remove
// PURPOSE:		unregister specified map change, and reverses it immediately if required
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::Remove(u32 oldHash, u32 newHash, const spdSphere& searchVol, CMapChange::eType changeType, bool bLazy)
{
	// for all matching map changes
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		CMapChange& change = m_activeChanges[i];
		if (change.Matches(oldHash, newHash, searchVol, changeType))
		{
			if (!bLazy)
			{
				// for each entity, undo change
				atArray<CEntity*> entities;
				change.FindEntitiesFromWorldSearch(entities, newHash);
				for (s32 j=0; j<entities.GetCount(); j++)
				{
#if GTA_REPLAY
					if(CReplayMgr::IsEditModeActive() && CReplayMgr::ShouldPreventMapChangeExecution(entities[j]))
					{
						continue;
					}
#endif // GTA_REPLAY
					change.Execute(entities[j], true);
				}
			}

			change.Shutdown();
		}
	}

	// remove inactive changes from active changes list
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		CMapChange& change = m_activeChanges[i];
		if (!change.IsActive())
		{
			m_activeChanges.DeleteFast(i);
			i--;
		}
	}

	// inform the door system in case this model is a door model
	if (changeType == CMapChange::TYPE_SWAP)
	{
		Vector3 center = VEC3V_TO_VECTOR3(searchVol.GetCenter());
		CDoorSystem::SwapModel(newHash, oldHash, center, searchVol.GetRadiusSquaredf());
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RemoveAllOwnedByScript
// PURPOSE:		unregister any map changes owned by the specified thread
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::RemoveAllOwnedByScript(scrThreadId threadId)
{
	// for all matching changes
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		CMapChange& change = m_activeChanges[i];
		if (change.IsScriptOwned() && change.GetOwnerThreadId()==threadId)
		{
			// for each entity, undo change
			atArray<CEntity*> entities;
			change.FindEntitiesFromWorldSearch(entities, change.GetNewHash());
			for (s32 j=0; j<entities.GetCount(); j++)
				change.Execute(entities[j], true);

			change.Shutdown();
		}
	}

	// remove inactive changes from active changes list
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		CMapChange& change = m_activeChanges[i];
		if (!change.IsActive())
		{
			m_activeChanges.DeleteFast(i);
			i--;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyChangesToMapData
// PURPOSE:		execute persistent map changes on newly loaded map data
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::ApplyChangesToMapData(s32 mapDataSlotIndex)
{
	fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(mapDataSlotIndex));
	if (pDef && pDef->IsLoaded() && pDef->GetNumMapChanges())
	{
		fwEntity** entities = pDef->GetEntities();
		u32 numEntities = pDef->GetNumEntities();
		if (numEntities && entities)
		{
			// find matching map changes
			// TODO speed up search with a hash map registry (key: slot, data:list of changes)
			for (s32 i=0; i<m_activeChanges.GetCount(); i++)
			{
				CMapChange& change = m_activeChanges[i];
				if (change.IncludesMapData(mapDataSlotIndex))
				{
					for (s32 j=0; j<numEntities; j++)
						change.Execute((CEntity*) entities[j]);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyChangesToEntity
// PURPOSE:		performs any required changes on the specified entity, if any.
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::ApplyChangesToEntity(CEntity* pEntity)
{
	if (pEntity)
	{
		for (s32 i=0; i<m_activeChanges.GetCount(); i++)
		{
			CMapChange& change = m_activeChanges[i];
			if (change.Execute(pEntity))
				return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AlreadyExists
// PURPOSE:		returns true if the specified map change already exists on the active list
//////////////////////////////////////////////////////////////////////////
bool CMapChangeMgr::AlreadyExists(u32 oldHash, u32 newHash, const spdSphere& searchVol, CMapChange::eType changeType) const
{
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		if (m_activeChanges[i].Matches(oldHash, newHash, searchVol, changeType))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IntersectsWithActiveChange
// PURPOSE:		returns true if the specified sphere intersects with the search
//				volume of an active map change sphere
//////////////////////////////////////////////////////////////////////////
bool CMapChangeMgr::IntersectsWithActiveChange(const spdSphere& sphere)
{
	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		CMapChange& change = m_activeChanges[i];
		if (change.IsActive() && change.Intersects(sphere))
		{
			return true;
		}
	}
	return false;
}


#if GTA_REPLAY

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetNoOfActiveMapChanges
// PURPOSE:		return the number of active map changes.
//////////////////////////////////////////////////////////////////////////
u32 CMapChangeMgr::GetNoOfActiveMapChanges() const
{
	return m_activeChanges.GetCount();
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetActiveMapChangeDetails
// PURPOSE:		supplies details of the i-th active map change.
//////////////////////////////////////////////////////////////////////////

void CMapChangeMgr::GetActiveMapChangeDetails(u32 i, u32 &oldHash, u32 &newHash, spdSphere& searchVol, bool &surviveMapReload, CMapChange::eType &changeType, bool &bAffectsScriptObjects, bool &bExecuted) const
{
	const CMapChange &change = m_activeChanges[i];
	oldHash = change.GetOldHash();
	newHash = change.GetNewHash();
	searchVol = change.GetSearchVolume();
	surviveMapReload = change.GetSurviveMapReload();
	changeType = change.m_changeType;
	bAffectsScriptObjects = change.m_bAffectsScriptObjects;
	bExecuted = change.HasBeenExecuted();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetActiveMapChange
// PURPOSE:		supplies CMapChange if exists in active array
//////////////////////////////////////////////////////////////////////////
CMapChange* CMapChangeMgr::GetActiveMapChange(u32 oldHash, u32 newHash, const spdSphere& searchVol, CMapChange::eType changeType)
{
	for (s32 i=0; i< m_activeChanges.GetCount(); i++)
	{
		if (m_activeChanges[i].Matches(oldHash, newHash, searchVol, changeType))
		{
			return &m_activeChanges[i];
		}
	}
	return NULL;
}

#endif // GTA_REPLAY

#if __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DbgDisplayActiveChanges
// PURPOSE:		debug display all active map changes
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::DbgDisplayActiveChanges()
{
	char achOldName[RAGE_MAX_PATH];
	char achNewName[RAGE_MAX_PATH];

	fwMapDataStore& mapStore = fwMapDataStore::GetStore();

	for (s32 i=0; i<m_activeChanges.GetCount(); i++)
	{
		const CMapChange& change = m_activeChanges[i];
		if (change.IsActive())
		{
			const spdSphere& searchVol = change.GetSearchVolume();
			Vec3V vPos = searchVol.GetCenter();
			float fRadius = searchVol.GetRadiusf();
			u32 index;
			fwArchetype* pOldArch = fwArchetypeManager::GetArchetypeFromHashKey(change.GetOldHash(), &index);
			sprintf(achOldName, "%s", pOldArch!=NULL ? pOldArch->GetModelName() : "UNKNOWN");
			fwArchetype* pNewArch = fwArchetypeManager::GetArchetypeFromHashKey(change.GetNewHash(), &index);
			sprintf(achNewName, "%s", pNewArch!=NULL ? pNewArch->GetModelName() : "UNKNOWN");

			switch (change.m_changeType)
			{
			case CMapChange::TYPE_SWAP:
				{
					grcDebugDraw::AddDebugOutput(
						"Swap %d: %s to %s <%4.2f, %4.2f, %4.2f> radius %4.2f - owner %s",
						i, achOldName, achNewName, vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), fRadius,
						!change.IsScriptOwned() ? "CODE" : GtaThread::GetThreadWithThreadId(change.GetOwnerThreadId())->GetScriptName()
					);
				}
				break;
			case CMapChange::TYPE_HIDE:
				{
					grcDebugDraw::AddDebugOutput(
						"Hide %d: %s <%4.2f, %4.2f, %4.2f> radius %4.2f - owner %s",
						i, achOldName, vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), fRadius,
						!change.IsScriptOwned() ? "CODE" : GtaThread::GetThreadWithThreadId(change.GetOwnerThreadId())->GetScriptName()
					);
				}
				break;
			case CMapChange::TYPE_FORCEOBJ:
				{
					grcDebugDraw::AddDebugOutput(
						"ForceObj %d: %s <%4.2f, %4.2f, %4.2f> radius %4.2f - owner %s",
						i, achOldName, vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), fRadius,
						!change.IsScriptOwned() ? "CODE" : GtaThread::GetThreadWithThreadId(change.GetOwnerThreadId())->GetScriptName()
					);
				}
				break;

			default:
				break;
			}

			for (s32 j=0; j<change.m_mapDataSlots.GetCount(); j++)
			{
				grcDebugDraw::AddDebugOutput("... registered with %s", mapStore.GetName(strLocalIndex(change.m_mapDataSlots[j])));
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DbgRemoveByIndex
// PURPOSE:		debug change removal fn, specified by index in array (which is not
//				a reliable way to refer to a map change ordinarily)
//////////////////////////////////////////////////////////////////////////
void CMapChangeMgr::DbgRemoveByIndex(s32 index, bool bLazy)
{
	if ( index<0 || index>=m_activeChanges.GetCount() )
		return;

	CMapChange& change = m_activeChanges[index];

	if (!bLazy)
	{
		// for each entity, undo map change
		atArray<CEntity*> entities;
		change.FindEntitiesFromWorldSearch(entities, change.GetNewHash());
		for (s32 j=0; j<entities.GetCount(); j++)
			change.Execute(entities[j], true);
	}

	change.Shutdown();

	m_activeChanges.DeleteFast(index);
}

#endif	//__BANK
