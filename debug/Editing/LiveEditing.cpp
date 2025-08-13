//
// debug/editing/LiveEditing.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "debug/Editing/LiveEditing.h"

#if __BANK

#include "atl/hashstring.h"
#include "camera/debug/DebugDirector.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "debug/Debug.h"
#include "debug/GtaPicker.h"
#include "fwscene/mapdata/mapdatadef.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/world/StreamedSceneGraphNode.h"
#include "objects/objectpopulation.h"
#include "objects/object.h"
#include "objects/DummyObject.h"
#include "parser/restparserservices.h"
#include "scene/world/GameWorld.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/LodMgr.h"
#include "scene/FocusEntity.h"
#include "system/controlMgr.h"
#include "string/string.h"
#include "vector/vector3.h"
#include "vector/vector4.h"

extern void DisplayObject(bool bDeletePrevious);
extern RegdEnt gpDisplayObject;
extern char gCurrentDisplayObject[];

class CFreeCamAdjust
{
public:
	void Process();

	Vector3 m_vPos;
	Vector3 m_vFront;
	Vector3 m_vUp;

	PAR_SIMPLE_PARSABLE;
};

class CEntityPickEvent
{
public:
	void Process();

	u32 m_guid;
	atHashString m_mapSectionName;
	atHashString m_modelName;

	PAR_SIMPLE_PARSABLE;
};

class CAddedEntity 
{
public:
	RegdEnt m_pEntity;
	u32 m_guid;
};

class CEntityEdit
{
public:

	enum
	{
		INVALID_LODDIST = -1
	};
	CEntityEdit() : m_guid(0), m_bForceFullMatrix(false), m_lodDistance(INVALID_LODDIST), m_childLodDistance(INVALID_LODDIST),
		m_position(0.0f, 0.0f, 0.0f), m_rotation(0.0f, 0.0f, 0.0f, 1.0f), m_scaleXY(1.0f), m_scaleZ(1.0f)
	{
	}

	u32						m_guid;
	Vector3					m_position;
	Vector4 				m_rotation;
	float					m_scaleXY;
	float					m_scaleZ;
	float					m_lodDistance;
	float					m_childLodDistance;
	bool					m_bForceFullMatrix;

	void Process();
	void ApplyEdit(CEntity* pEntity);
	void PostEdit(CEntity* pEntity);

	PAR_SIMPLE_PARSABLE;
};

class CEntityDelete
{
public:
	CEntityDelete() : m_guid(0)	{ }
	void Process();

	u32						m_guid;

	PAR_SIMPLE_PARSABLE;
};

class CEntityFlagChange
{
public:

	CEntityFlagChange() :
		m_guid(0),
		m_bDontRenderInReflections(false), m_bOnlyRenderInReflections(false),
		m_bDontRenderInShadows(false), m_bOnlyRenderInShadows(false),
		m_bDontRenderInWaterReflections(false), m_bOnlyRenderInWaterReflections(false)
	{ }

	void Process();

	u32 m_guid;
	bool m_bDontRenderInShadows;
	bool m_bOnlyRenderInShadows;
	bool m_bDontRenderInReflections;
	bool m_bOnlyRenderInReflections;
	bool m_bDontRenderInWaterReflections;
	bool m_bOnlyRenderInWaterReflections;

	PAR_SIMPLE_PARSABLE;
};

class CEntityAdd
{
public:
	CEntityAdd() : m_guid(0) { }
	void Process();

	atHashString			m_name;
	u32						m_guid;

	static atArray<CAddedEntity> ms_addedEntities;

	PAR_SIMPLE_PARSABLE;
};

atArray<CAddedEntity> CEntityAdd::ms_addedEntities;

class CEntityEditSet
{
public:
	atHashString				m_mapSectionName;
	atArray<CEntityEdit>		m_edits;
	atArray<CEntityDelete>		m_deletes;
	atArray<CEntityAdd>			m_adds;
	atArray<CEntityFlagChange>	m_flagChanges;

	void Process();

	PAR_SIMPLE_PARSABLE;
};

class CMapLiveEditRestInterface
{
public:
	atArray<CEntityEditSet>	m_incomingEditSets;
	atArray<CFreeCamAdjust> m_incomingCameraChanges;
	atArray<CEntityPickEvent> m_incomingPickerSelect;

	void ProcessAndFlush();

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		apply flag changes to entity
//////////////////////////////////////////////////////////////////////////
void CEntityFlagChange::Process()
{
	if ( m_guid )
	{
		CEntity* pEntity = g_mapLiveEditing.FindEntity( m_guid );
		if ( pEntity )
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			fwFlags16& visibilityMask = pEntity->GetRenderPhaseVisibilityMask();

			visibilityMask.SetAllFlags();

			//////////////////////////////////////////////////////////////////////////
			// change flags for shadows
			const bool bDontRenderInShadows = pModelInfo->GetDontCastShadows() || m_bDontRenderInShadows;
			const bool bOnlyRenderInShadows = pModelInfo->GetIsShadowProxy() || m_bOnlyRenderInShadows;
			
			if (bDontRenderInShadows)
			{
				visibilityMask.ClearFlag( VIS_PHASE_MASK_SHADOWS );
			}
			else if (bOnlyRenderInShadows)
			{
				visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_SHADOWS|VIS_PHASE_MASK_STREAMING)) );
			}
			//////////////////////////////////////////////////////////////////////////
			// change flags  for reflections

			if (m_bDontRenderInReflections)
			{
				visibilityMask.ClearFlag( VIS_PHASE_MASK_REFLECTIONS );
			}
			else if (m_bOnlyRenderInReflections)
			{
				visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_REFLECTIONS|VIS_PHASE_MASK_STREAMING)) );
			}

			//////////////////////////////////////////////////////////////////////////
			// change flags  for water reflections

			if (m_bDontRenderInWaterReflections)
			{
				visibilityMask.ClearFlag( VIS_PHASE_MASK_WATER_REFLECTION );
			}
			else if (m_bOnlyRenderInWaterReflections)
			{
				visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_WATER_REFLECTION|VIS_PHASE_MASK_STREAMING)) );
			}

			pEntity->RequestUpdateInWorld();

			//////////////////////////////////////////////////////////////////////////

			// if entity is a dummy, apply same changes to any related objects
			if ( pEntity->GetIsTypeDummyObject() )
			{
				atArray<CObject*> relatedObjects;
				CMapLiveEditUtils::FindRelatedObjects(pEntity, relatedObjects);
				for (s32 i=0; i<relatedObjects.GetCount(); i++)
				{
					CObject* pObj = relatedObjects[i];
					if (pObj)
					{
						CObjectPopulation::ConvertToDummyObject(pObj, true);
					}
				}
				CObjectPopulation::ManageDummy((CDummyObject*)pEntity, CFocusEntityMgr::GetMgr().GetPos(), camInterface::GetGameplayDirector().IsFirstPersonAiming());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		apply camera change to free cam
//////////////////////////////////////////////////////////////////////////
void CFreeCamAdjust::Process()
{
	camDebugDirector& dbgDirector = camInterface::GetDebugDirector();
	CControlMgr::SetDebugPadOn(true);
	dbgDirector.ActivateFreeCam();
	camFrame& freeCamFrame = dbgDirector.GetFreeCamFrameNonConst();
	freeCamFrame.SetPosition(m_vPos);
	freeCamFrame.SetWorldMatrixFromFrontAndUp(m_vFront, m_vUp);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		set selected entity
//////////////////////////////////////////////////////////////////////////
void CEntityPickEvent::Process()
{
	g_mapLiveEditing.SetSelectedInPicker(m_mapSectionName.GetCStr(), m_guid);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		creates a new entity
//////////////////////////////////////////////////////////////////////////
void CEntityAdd::Process()
{
	// create entity
	CEntity* pEntity = NULL;

	strcpy(gCurrentDisplayObject, m_name.GetCStr());

	DisplayObject(false);
	if (gpDisplayObject)
	{
		pEntity = gpDisplayObject;
	}

	// push entity into list
	if (pEntity)
	{
		pEntity->SetOwnedBy(ENTITY_OWNEDBY_LIVEEDIT);
		CAddedEntity& addedEntity = ms_addedEntities.Grow();
		addedEntity.m_guid = m_guid;
		addedEntity.m_pEntity = pEntity;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		delete map object
//////////////////////////////////////////////////////////////////////////
void CEntityDelete::Process()
{
	if ( m_guid )
	{
		CEntity* pEntity = g_mapLiveEditing.FindEntity( m_guid );
		if ( pEntity && pEntity->GetLodData().IsOrphanHd() )
		{
			// if entity is a dummy, delete any related objects
			if ( pEntity->GetIsTypeDummyObject() )
			{
				atArray<CObject*> relatedObjects;
				CMapLiveEditUtils::FindRelatedObjects(pEntity, relatedObjects);
				for (s32 i=0; i<relatedObjects.GetCount(); i++)
				{
					CObject* pObj = relatedObjects[i];
					if (pObj)
					{
						CObjectPopulation::ConvertToDummyObject(pObj, true);
					}
				}
			}

			// remove from IPL entity array
			s32 iplIndex = pEntity->GetIplIndex();
			if (iplIndex > 0)
			{
				fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(iplIndex));
				if (pDef && pDef->GetIsValid() && pDef->IsLoaded())
				{
					fwEntity** entities = pDef->GetEntities();
					u32 numEntities = pDef->GetNumEntities();
					for (u32 i=0; i<numEntities; i++)
					{
						if (entities[i] == pEntity)
						{
							entities[i] = NULL;
						}
					}

					// remove from world
					fwEntityContainer* container = (fwEntityContainer*) pEntity->GetOwnerEntityContainer();
					if (container)
					{
						container->RemoveEntity(pEntity);
					}
				}

				// delete map object
				CGameWorld::PostRemove(pEntity);
				delete pEntity;
			}
			//////////////////////////////////////////////////////////////////////////
			else if (pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT)
			{
				
				CGameWorld::Remove(pEntity);

				if (pEntity->GetIsTypeObject())
				{
					CObjectPopulation::DestroyObject((CObject*)pEntity);
				}
				else
				{
					delete pEntity;
				}

				// remove from live edit created entity list
				for (s32 i=0; i<CEntityAdd::ms_addedEntities.GetCount(); i++)
				{
					CAddedEntity& addedEntity = CEntityAdd::ms_addedEntities[i];
					if (addedEntity.m_pEntity == pEntity)
					{
						CEntityAdd::ms_addedEntities.DeleteFast(i);
						break;
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////

		}
	}

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		execute edit on entity
//////////////////////////////////////////////////////////////////////////
void CEntityEdit::Process()
{
	if ( m_guid )
	{
		CEntity* pEntity = g_mapLiveEditing.FindEntity( m_guid );
		if ( pEntity )
		{
			ApplyEdit(pEntity);

			// if entity is a dummy, apply same changes to any related objects
			if ( pEntity->GetIsTypeDummyObject() )
			{
				atArray<CObject*> relatedObjects;
				CMapLiveEditUtils::FindRelatedObjects(pEntity, relatedObjects);
				for (s32 i=0; i<relatedObjects.GetCount(); i++)
				{
					CObject* pObj = relatedObjects[i];
					if (pObj)
					{
						CObjectPopulation::ConvertToDummyObject(pObj, true);
					}
				}
				CObjectPopulation::ManageDummy((CDummyObject*)pEntity, CFocusEntityMgr::GetMgr().GetPos(), camInterface::GetGameplayDirector().IsFirstPersonAiming());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyEdit
// PURPOSE:		makes changes to specified entity
//////////////////////////////////////////////////////////////////////////
void CEntityEdit::ApplyEdit(CEntity* pEntity)
{
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		//////////////////////////////////////////////////////////////////////////
		//1. transform changes
		fwEntityDef def;
		def.m_position = m_position;
		def.m_rotation = m_rotation;
		def.m_scaleXY = m_scaleXY;
		def.m_scaleZ = m_scaleZ;
		def.m_flags = 0;
		if (m_bForceFullMatrix)
		{
			def.m_flags |= fwEntityDef::FLAG_FULLMATRIX;
		}

		if (pEntity->GetIsTypeObject() && pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT)
		{
			fwTransform* pTrans = NULL;
			bool bScaled = ((def.m_scaleXY != 1.f) || (def.m_scaleZ != 1.f));
			QuatV q(-def.m_rotation.x, -def.m_rotation.y, -def.m_rotation.z, def.m_rotation.w);
			Vec3V pos(def.m_position);
			Mat34V m;
			Mat34VFromQuatV(m, q, pos);

#if ENABLE_MATRIX_MEMBER
			(void)pTrans;
			if (!bScaled)
			{
				pEntity->SetTransform(m);
				pEntity->SetTransformScale(1.0f, 1.0f);
			}
			else
			{
				pEntity->SetTransform(m);
				pEntity->SetTransformScale(def.m_scaleXY, def.m_scaleZ);
			}
#else
			if (!bScaled)
			{
				pTrans = rage_new fwMatrixTransform(m);
			}
			else
			{
				pTrans = rage_new fwMatrixScaledTransform(m);
				pTrans->SetScale(def.m_scaleXY, def.m_scaleZ);
			}
			pEntity->SetTransform(pTrans);
#endif
		}
		else
		{
			pEntity->InitTransformFromDefinition(&def, pModelInfo);
		}

		//////////////////////////////////////////////////////////////////////////
		//2. lod distance and child lod distance
		if ( m_lodDistance != INVALID_LODDIST )
		{
			CLodDistTweaker::ChangeEntityLodDist(pEntity, (u32) m_lodDistance);
		}
		if ( pEntity->GetLodData().HasChildren() && m_childLodDistance!=INVALID_LODDIST )
		{
			CLodDistTweaker::ChangeAllChildrenLodDists(pEntity, (u32) m_childLodDistance);
		}

		//////////////////////////////////////////////////////////////////////////
		PostEdit(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PostEdit
// PURPOSE:		update world and all the other data structures for visibility and streaming
//////////////////////////////////////////////////////////////////////////
void CEntityEdit::PostEdit(CEntity* pEntity)
{
	pEntity->RequestUpdateInWorld();
	pEntity->RequestUpdateInPhysics();

	s32 iplIndex = pEntity->GetIplIndex();
	if (iplIndex > 0)
	{
		spdAABB& physicalBounds = fwMapDataStore::GetStore().GetBoxStreamer().GetBounds(iplIndex);
		spdAABB entityBounds = pEntity->GetBoundBox();
		
		// if entity has moved beyond the bounds of the original imap file, a whole bunch of things need to be updated
		if ( !physicalBounds.ContainsAABB(entityBounds) )
		{
			//1. update the box streamer physical extents
			physicalBounds.GrowAABB(entityBounds);

			//2. update the box streamer streaming extents
			spdAABB& streamingBounds = fwMapDataStore::GetStore().GetBoxStreamer().GetStreamingBounds(iplIndex);
			Vector3 vBasisPivot;
			g_LodMgr.GetBasisPivot(pEntity, vBasisPivot);
			spdSphere visibleRange(VECTOR3_TO_VEC3V(vBasisPivot), ScalarV((float)  pEntity->GetLodDistance()));
			streamingBounds.GrowSphere(visibleRange);

			//3. update the scene graph node
			fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(iplIndex));
			if (pDef && pDef->IsLoaded())
			{
				fwStreamedSceneGraphNode* pSceneGraphNode = pDef->m_pObject->GetSceneGraphNode();
				if (pSceneGraphNode)
				{
					pSceneGraphNode->SetBoundingBox(physicalBounds);
				}
			}

			//4. rebuild the box streamer trees
			fwMapDataStore::GetStore().GetBoxStreamer().RebuildTrees();	
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		execute entity edits in this set
//////////////////////////////////////////////////////////////////////////
void CEntityEditSet::Process()
{
	g_mapLiveEditing.Start( m_mapSectionName.GetCStr() );

	for (s32 i=0; i<m_edits.GetCount(); i++)
	{
		m_edits[i].Process();
	}

	for (s32 i=0; i<m_deletes.GetCount(); i++)
	{
		m_deletes[i].Process();
	}

	for (s32 i=0; i<m_adds.GetCount(); i++)
	{
		m_adds[i].Process();
	}

	for (s32 i=0; i<m_flagChanges.GetCount(); i++)
	{
		m_flagChanges[i].Process();
	}

	g_mapLiveEditing.Stop();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ProcessAndFlush
// PURPOSE:		process any messages in the queue, and flush
//////////////////////////////////////////////////////////////////////////
void CMapLiveEditRestInterface::ProcessAndFlush()
{
	if ( m_incomingEditSets.GetCount() )
	{
		// process edits
		for (s32 i=0; i<m_incomingEditSets.GetCount(); i++)
		{
			m_incomingEditSets[i].Process();
		}
		m_incomingEditSets.Reset();
	}

	if ( m_incomingCameraChanges.GetCount() )
	{
		// process camera tweaks
		for (s32 i=0; i<m_incomingCameraChanges.GetCount(); i++)
		{
			m_incomingCameraChanges[i].Process();
		}
		m_incomingCameraChanges.Reset();
	}

	if ( m_incomingPickerSelect.GetCount() )
	{
		for (s32 i=0; i<m_incomingPickerSelect.GetCount(); i++)
		{
			m_incomingPickerSelect[i].Process();
		}
		m_incomingPickerSelect.Reset();
	}
}

#include "debug/editing/LiveEditing_parser.h"


CMapLiveEdit g_mapLiveEditing;
CMapLiveEditRestInterface g_mapLiveEditRestInterface;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CMapLiveEdit
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CMapLiveEdit::CMapLiveEdit() : m_bEnabled(false)
{
	m_mapSectionBeingEdited = "";
	m_guidMap.Reset();
	m_imapFiles.Reset();

	// rag widgets
	m_bDbgDisplay = false;
	m_dbgGuid = 0;
	m_bDbgQuery = false;
	m_bDbgDontRenderInShadows = false;
	m_bDbgOnlyRenderInShadows = false;
	m_bDbgDontRenderInReflections = false;
	m_bDbgOnlyRenderInReflections = false;
	m_bDbgDontRenderInWaterReflections = false;
	m_bDbgOnlyRenderInWaterReflections = false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		start live-editing specified map section
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::Start(const char* pszMapSection)
{
	Stop();

	if ( SetEditing(pszMapSection) )
	{
		// pin map data so it does not get deleted while editing
		for (s32 i=0; i<m_imapFiles.GetCount(); i++)
		{
			s32 slotIndex = m_imapFiles[i];
			fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByUser(slotIndex, true);
		}

		SetEnabled(true);
		RebuildGuidMap();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		stop live-editing current map data file (imap)
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::Stop()
{
	if ( GetEnabled() )
	{
		// unpin map data so it can be freed up as needed
		for (s32 i=0; i<m_imapFiles.GetCount(); i++)
		{
			s32 slotIndex = m_imapFiles[i];
			fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByUser(slotIndex, false);
		}

		SetEnabled(false);
		ClearGuidMap();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		checks if the guid map needs to be regenerated etc
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::Update()
{
	static bool bInitialised = false;
	if (!bInitialised)
	{
		Init();
		bInitialised = true;
	}

	// rebuild guid map if necessary
	if ( GetEnabled() && m_guidMap.GetNumUsed()==0)
	{
		RebuildGuidMap();
	}

	// process any pending incoming edits
	g_mapLiveEditRestInterface.ProcessAndFlush();

	// process any outgoing edits

	DebugUpdate();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindEntity
// PURPOSE:		returns the entity pointer for the specified guid, if found (null otherwise)
//////////////////////////////////////////////////////////////////////////
CEntity* CMapLiveEdit::FindEntity(const u32 guid, bool bPreferObjects/*=false*/)
{
	if ( GetEnabled() && guid )
	{
		CEntity** pResult = m_guidMap.Access(guid);
		if (pResult && *pResult)
		{
			if (bPreferObjects && (*pResult)->GetIsTypeDummyObject())
			{
				CObject::Pool* m_pObjectPool = CObject::GetPool();
				for(s32 i=0; i<m_pObjectPool->GetSize(); i++)
				{
					CObject* pObj = m_pObjectPool->GetSlot(i);
					if(pObj && pObj->GetRelatedDummy()==*pResult)
					{
						return pObj;
					}
				}
			}
			return *pResult;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DeleteMapEntity
// PURPOSE:		quick utility fn to delete a map object
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::DeleteMapEntity(const u32 guid, const char* pszMapSection)
{
	if (guid)
	{
		CEntityDelete entityDelete;
		entityDelete.m_guid = guid;

		CEntityEditSet editSet;
		editSet.m_mapSectionName = pszMapSection;
		editSet.m_deletes.Grow() = entityDelete;

		g_mapLiveEditRestInterface.m_incomingEditSets.Grow() = editSet;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetSelectedInPicker
// PURPOSE:		for the specified map section name and entity GUID, set that entity (if it exists)
//				as the currently selected entity in the picker.
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::SetSelectedInPicker(const char* pszMapSection, const u32 guid)
{
	Start(pszMapSection);
	CEntity* pEntity = FindEntity(guid, true);
	if (pEntity)
	{
		g_PickerManager.AddEntityToPickerResults(pEntity, true, true);
	}
	Stop();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:		registers rest interface
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::Init()
{
	parRestRegisterSingleton("Scene/MapLiveEdit", g_mapLiveEditRestInterface, NULL);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetEditing
// PURPOSE:		sets the map data file currently being live-edited, returns true if successful
//////////////////////////////////////////////////////////////////////////
bool CMapLiveEdit::SetEditing(const char* pszMapSection)
{
	strLocalIndex slotIndex = fwMapDataStore::GetStore().FindSlot(pszMapSection);

	bool bValidMapSection = (slotIndex.Get() != -1);
	bool bLiveEdit = (atStringHash(pszMapSection) == ATSTRINGHASH("liveedit", 0xB3EE5A2C));
	
	if ( Verifyf(bValidMapSection||bLiveEdit, "Invalid map section: %s", pszMapSection) )
	{
		m_mapSectionBeingEdited = pszMapSection;
		CMapLiveEditUtils::FindRelatedImapFiles(pszMapSection, m_imapFiles);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddGuidsForImap
// PURPOSE:		extract guids from specified imap (if loaded) and add to guid map
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::AddGuidsForImap(s32 imapSlot)
{
	fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(imapSlot));
	if ( pDef && pDef->IsLoaded() )
	{
		u32 numEntities = pDef->GetNumEntities();
		fwEntity** entities = pDef->GetEntities();

		// for each entity in the imap, get its guid. if valid insert it into the guid map
		// for speedy entity lookup.
		for (u32 i=0; i<numEntities; i++)
		{
			CEntity* pEntity = (CEntity*) entities[i];
			if (pEntity)
			{
				u32 guid;
				if ( pEntity->GetGuid(guid) )
				{
					if ( Verifyf(guid, "Entity %s does not have valid guid", pEntity->GetModelName()) )
					{
						// check for guid collision
						CEntity** pResult = m_guidMap.Access(guid);
						if (pResult && *pResult)
						{
							if (*pResult == pEntity)
							{
								Assertf(false, "Adding %s to GUID map, but is already present", pEntity->GetModelName());
							}
							else
							{
#if __ASSERT
								CEntity* pDupe = *pResult;
								Assertf(false, "GUID map collision: 0x%x is used by %s (0x%p) and %s (0x%p)",
									guid, pEntity->GetModelName(), pEntity, pDupe->GetModelName(), pDupe);
#endif
							}
						}
						m_guidMap.Insert(guid, pEntity);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddGuidsForLiveEditAdds
// PURPOSE:		add guids for live edit created entities to guid map
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::AddGuidsForLiveEditAdds()
{
	for (s32 i=0; i<CEntityAdd::ms_addedEntities.GetCount(); i++)
	{
		CAddedEntity& addedEntity = CEntityAdd::ms_addedEntities[i];
		if (addedEntity.m_pEntity)
		{
			CEntity* pEntity = addedEntity.m_pEntity;
			u32 guid = addedEntity.m_guid;

			// check for guid collision
			CEntity** pResult = m_guidMap.Access(guid);
			if (pResult && *pResult)
			{
				if (*pResult == pEntity)
				{
					Assertf(false, "Adding %s to GUID map, but is already present", pEntity->GetModelName());
				}
				else
				{
#if __ASSERT
					CEntity* pDupe = *pResult;
					Assertf(false, "GUID map collision: 0x%x is used by %s (0x%p) and %s (0x%p)",
						guid, pEntity->GetModelName(), pEntity, pDupe->GetModelName(), pDupe);
#endif
				}
			}
			m_guidMap.Insert(guid, pEntity);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RebuildGuidMap
// PURPOSE:		regenerate the guid atMap for the map data file being edited
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::RebuildGuidMap()
{
	if ( GetEnabled() )
	{
		ClearGuidMap();
		for (s32 imap=0; imap<m_imapFiles.GetCount(); imap++)
		{
			if (m_imapFiles[imap] != -1)
			{
				AddGuidsForImap(m_imapFiles[imap]);
			}
		}
		AddGuidsForLiveEditAdds();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		some debug widgets for testing live edit
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Map Live Edit");
	{
		bank.AddToggle("Display debug info", &m_bDbgDisplay);
		bank.AddToggle("Query selected via GUID", &m_bDbgQuery);
		bank.AddButton("Start edit for selected entity", DebugStartEditCb);
		bank.AddButton("Stop edit", DebugStopEditCb);
		bank.AddButton("Test Edit move selected up in Z axis", GenerateTestEditCb);
		bank.AddButton("Test Delete selected", GenerateTestDeleteCb);
		bank.AddButton("Test Add selected", GenerateTestAddCb);
		bank.AddButton("Test camera change", GenerateCamChangeCb);
		bank.AddToggle("Don't render in shadows", &m_bDbgDontRenderInShadows);
		bank.AddToggle("Only render in shadows", &m_bDbgOnlyRenderInShadows);
		bank.AddToggle("Don't render in reflections", &m_bDbgDontRenderInReflections);
		bank.AddToggle("Only render in reflections", &m_bDbgOnlyRenderInReflections);
		bank.AddToggle("Don't render in water reflections", &m_bDbgDontRenderInWaterReflections);
		bank.AddToggle("Only render in water reflections", &m_bDbgOnlyRenderInWaterReflections);
		bank.AddButton("Test flag change selected", GenerateFlagChangeCb);
	}
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugUpdate
// PURPOSE:		update for rag widgets etc
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::DebugUpdate()
{
	// debug draw
	if ( GetEnabled() && m_bDbgDisplay)
	{
		grcDebugDraw::AddDebugOutput("Map LiveEdit: %s", m_mapSectionBeingEdited.c_str());
		for (s32 i=0; i<m_imapFiles.GetCount(); i++)
		{
			grcDebugDraw::AddDebugOutput("IMAP: %s", fwMapDataStore::GetStore().GetName(strLocalIndex(m_imapFiles[i])));
		}

		if (m_bDbgQuery)
		{
			CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
			u32 selectedGuid = 0;
			if ( pSelectedEntity && pSelectedEntity->GetGuid(selectedGuid) )
			{
				grcDebugDraw::AddDebugOutput("... querying with GUID from %s: 0x%x", pSelectedEntity->GetModelName(), selectedGuid);
				CEntity* pQueryResultEntity = FindEntity(selectedGuid);
				if ( pQueryResultEntity )
				{
					grcDebugDraw::AddDebugOutput("... found entity 0x%p name %s", pQueryResultEntity, pQueryResultEntity->GetModelName());

					if ( pQueryResultEntity->GetIsTypeDummyObject() )
					{
						// find related objects and list those too
						atArray<CObject*> objArray;
						CMapLiveEditUtils::FindRelatedObjects(pQueryResultEntity, objArray);
						for (s32 i=0; i<objArray.GetCount(); i++)
						{
							CObject* pObject = objArray[i];
							if ( pObject )
							{
								grcDebugDraw::AddDebugOutput("... and object 0x%p name %s", pObject, pObject->GetModelName());
							}
						}
					}
				}
			}
			else
			{
				grcDebugDraw::AddDebugOutput("... no valid GUID selected");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugStartEditCb
// PURPOSE:		used by rag widget to start edit for map data containing the selected entity
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::DebugStartEditCb()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if ( pEntity )
	{
		strLocalIndex iplIndex = strLocalIndex(pEntity->GetIplIndex());
		if ( iplIndex.Get() > 0 )
		{
			const char* pszName = fwMapDataStore::GetStore().GetName(iplIndex);
			g_mapLiveEditing.Start(pszName);
		}
		else if ( pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT )
		{
			g_mapLiveEditing.Start("liveedit");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugStopEditCb
// PURPOSE:		used by rag widget to stop edit
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::DebugStopEditCb()
{
	g_mapLiveEditing.Stop();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateTestEditCb
// PURPOSE:		rag cb to move selected map object up by 5m in z-axis, via live editing
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::GenerateTestEditCb()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if ( pEntity )
	{
		//////////////////////////////////////////////////////////////////////////
		bool bHasGuid = false;
		u32 guid = 0;

		if ( pEntity->GetIplIndex() )
		{
			// retrieve guid from map data
			bHasGuid = pEntity->GetGuid(guid);
		}
		else if ( pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT )
		{
			// run over live edit adds to retrieve guid
			for (s32 i=0; i<CEntityAdd::ms_addedEntities.GetCount(); i++)
			{
				CAddedEntity& addedEntity = CEntityAdd::ms_addedEntities[i];
				if (addedEntity.m_pEntity == pEntity)
				{
					guid = addedEntity.m_guid;
					bHasGuid = true;
					break;
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////

		if ( bHasGuid )
		{
			if (guid)
			{
				Vector3 vPos = VEC3V_TO_VECTOR3( pEntity->GetTransform().GetPosition() );
				vPos.z += 5.0f;

				CEntityEdit entityEdit;
				entityEdit.m_position = vPos;
				entityEdit.m_guid = guid;

				CEntityEditSet editSet;
				editSet.m_mapSectionName = ( pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT ? "liveedit" : fwMapDataStore::GetStore().GetName(strLocalIndex(pEntity->GetIplIndex())) );
				editSet.m_edits.Grow() = entityEdit;

				g_mapLiveEditRestInterface.m_incomingEditSets.Grow() = editSet;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateTestDeleteCb
// PURPOSE:		rag cb to delete selected map object via live editing
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::GenerateTestDeleteCb()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if ( pEntity && pEntity->GetLodData().IsOrphanHd() )
	{
		bool bHasGuid = false;
		u32 guid = 0;

		if ( pEntity->GetIplIndex() )
		{
			// retrieve guid from map data
			bHasGuid = pEntity->GetGuid(guid);
		}
		else if ( pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT )
		{
			// run over live edit adds to retrieve guid
			for (s32 i=0; i<CEntityAdd::ms_addedEntities.GetCount(); i++)
			{
				CAddedEntity& addedEntity = CEntityAdd::ms_addedEntities[i];
				if (addedEntity.m_pEntity == pEntity)
				{
					guid = addedEntity.m_guid;
					bHasGuid = true;
					break;
				}
			}
		}

		if (bHasGuid)
		{
			CEntityDelete entityDelete;
			entityDelete.m_guid = guid;

			CEntityEditSet editSet;
			editSet.m_mapSectionName = ( pEntity->GetOwnedBy()==ENTITY_OWNEDBY_LIVEEDIT ? "liveedit" : fwMapDataStore::GetStore().GetName(strLocalIndex(pEntity->GetIplIndex())) );
			editSet.m_deletes.Grow() = entityDelete;
		
			g_mapLiveEditRestInterface.m_incomingEditSets.Grow() = editSet;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateTestAddCb
// PURPOSE:		rag cb to add an entity of the same type as the currently picked entity
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::GenerateTestAddCb()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if ( pEntity )
	{		
		CEntityAdd entityAdd;
		entityAdd.m_guid = rand();
		entityAdd.m_name = pEntity->GetModelName();

		CEntityEditSet editSet;
		editSet.m_mapSectionName = fwMapDataStore::GetStore().GetName( strLocalIndex(pEntity->GetIplIndex()) );
		editSet.m_adds.Grow() = entityAdd;

		g_mapLiveEditRestInterface.m_incomingEditSets.Grow() = editSet;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateFlagChangeCb
// PURPOSE:		rag cb to add a set of change flags for the currently picked entity
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::GenerateFlagChangeCb()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if ( pEntity && pEntity->GetIplIndex() )
	{	
		u32 guid = 0;
		if ( pEntity->GetGuid(guid) )
		{
			CEntityFlagChange entityFlagChange;
			entityFlagChange.m_guid = guid;
			entityFlagChange.m_bDontRenderInReflections = g_mapLiveEditing.m_bDbgDontRenderInReflections;
			entityFlagChange.m_bOnlyRenderInReflections = g_mapLiveEditing.m_bDbgOnlyRenderInReflections;
			entityFlagChange.m_bDontRenderInWaterReflections = g_mapLiveEditing.m_bDbgDontRenderInWaterReflections;
			entityFlagChange.m_bOnlyRenderInWaterReflections = g_mapLiveEditing.m_bDbgOnlyRenderInWaterReflections;
			entityFlagChange.m_bDontRenderInShadows = g_mapLiveEditing.m_bDbgDontRenderInShadows;
			entityFlagChange.m_bOnlyRenderInShadows = g_mapLiveEditing.m_bDbgOnlyRenderInShadows;

			CEntityEditSet editSet;
			editSet.m_mapSectionName = fwMapDataStore::GetStore().GetName( strLocalIndex(pEntity->GetIplIndex()) );
			editSet.m_flagChanges.Grow() = entityFlagChange;

			g_mapLiveEditRestInterface.m_incomingEditSets.Grow() = editSet;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateCamChangeCb
// PURPOSE:		generate a test camera adjustment for selected entity
//////////////////////////////////////////////////////////////////////////
void CMapLiveEdit::GenerateCamChangeCb()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if ( pEntity )
	{
		Vector3 vEntityPos = VEC3V_TO_VECTOR3( pEntity->GetTransform().GetPosition() );
		Vector3 vNewCamPos = vEntityPos;
		vNewCamPos.z += 50.0f;
		Vector3 vNewFront = vEntityPos - vNewCamPos;

		Matrix34 worldMatrix;
		camFrame::ComputeWorldMatrixFromFront(vNewFront, worldMatrix);
		Vector3 vNewUp = worldMatrix.c;

		CFreeCamAdjust camEdit;
		camEdit.m_vPos = vNewCamPos;
		camEdit.m_vFront = vNewFront;
		camEdit.m_vUp = vNewUp;

		g_mapLiveEditRestInterface.m_incomingCameraChanges.Grow() = camEdit;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindRelatedObjects
// PURPOSE:		utility fn to find objects related to specified dummy
//////////////////////////////////////////////////////////////////////////
void CMapLiveEditUtils::FindRelatedObjects(CEntity* pDummy, atArray<CObject*>& objects)
{
	if (!pDummy)
	{
		return;
	}

	CObject::Pool* m_pObjectPool = CObject::GetPool();
	for(s32 i=0; i<m_pObjectPool->GetSize(); i++)
	{
		CObject* pObject = m_pObjectPool->GetSlot(i);
		if (pObject && (CEntity*)(pObject->GetRelatedDummy())==pDummy)
		{
			objects.PushAndGrow(pObject);
		}	
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindRelatedImapFiles
// PURPOSE:		fills array with imap files related to specified slot
//////////////////////////////////////////////////////////////////////////
void CMapLiveEditUtils::FindRelatedImapFiles(const char* pszMapSection, atArray<s32>& relatedImaps)
{
	relatedImaps.Reset();

	strLocalIndex slotIndex = fwMapDataStore::GetStore().FindSlot(pszMapSection);
	if (slotIndex.Get() != -1)
	{
		RecursivelyAddChildren(slotIndex.Get(), relatedImaps);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RecursivelyAddChildren
// PURPOSE:		horrendous but necessary. for the specified map data slot, recursively
//				find all dependent map data slots and add them to the array
//////////////////////////////////////////////////////////////////////////
void CMapLiveEditUtils::RecursivelyAddChildren(s32 imapSlot, atArray<s32>& relatedImaps)
{
	fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(imapSlot));
	if ( pDef && pDef->IsLoaded() )
	{
		relatedImaps.Grow() = imapSlot;
		if ( pDef->GetNumLoadedChildren() )
		{
			for(s32 i=0; i<fwMapDataStore::GetStore().GetSize(); i++)
			{	
				fwMapDataDef* pChildDef = fwMapDataStore::GetStore().GetSlot(strLocalIndex(i));
				if (pChildDef && pChildDef!=pDef && pChildDef->GetIsValid()
					&& pChildDef->IsLoaded() && pChildDef->GetParentDef()==pDef)
				{
					RecursivelyAddChildren(i, relatedImaps);
				}
			}
		}
	}
}

#endif	//__BANK
