// Title	:	RenderListGroup.h
// Authors	:	John Whyte, Alessandro Piva
// Started	:	31/07/2006

//////////////
// Includes //
//////////////
#include "renderListGroup.h"

#include "fwrenderer/renderlistgroup.h"

#include "renderer/Shadows/ParaboloidShadows.h"
#include "renderer/PostScan.h"
#include "scene/Entity.h"

#define MAX_NUM_DEFERRED_ENTITY_LISTS	(MAX_PARABOLOID_SHADOW_RENDERPHASES + 1 + 1 + 1 + 1) //1 for GBUF, 1 for cascaded shadows, 1 for draw scene alpha, 1 for mirror reflections, and rest from MAX_PARABOLOID_SHADOW_RENDERPHASES
#define MAX_NUM_DEFERRED_ENTITIES_PER_LIST	(600)

struct DeferredEntityData
{
	fwEntity*		m_pEntity;
	u32				m_subPhaseVisFlags;
	float			m_sortVal;
	u16				m_flags;
};

struct DeferredListToProcess
{
	int				m_List;
	fwRenderPassId	m_renderPassId;
	atArray<DeferredEntityData> m_entities;
};

static atArray<DeferredListToProcess> m_DeferredLists;
static int m_DeferredListsCount = 0;

void CGtaRenderListGroup::Init(unsigned initMode)
{
	gRenderListGroup.Init(initMode, MAX_NUM_RENDER_PHASES, RPTYPE_NUM_RENDER_PHASE_TYPES, RPASS_NUM_RENDER_PASSES);

	gRenderListGroup.SetRenderPhaseTypeInfo(RPTYPE_SHADOW,		9);	// 1 for cascade shadows + 8 for local shadows
	gRenderListGroup.SetRenderPhaseTypeInfo(RPTYPE_REFLECTION,	3); // 1 paraboloid + 1 water + 1 mirror
	gRenderListGroup.SetRenderPhaseTypeInfo(RPTYPE_MAIN,		3);
	gRenderListGroup.SetRenderPhaseTypeInfo(RPTYPE_STREAMING,	5);	// 1 hd sphere + 1 lod sphere + 1 interior sphere + 2 stream vols
	gRenderListGroup.SetRenderPhaseTypeInfo(RPTYPE_HEIGHTMAP,	1);
	gRenderListGroup.SetRenderPhaseTypeInfo(RPTYPE_DEBUG,		1);

	fwRenderListDesc::SetSortType(RPASS_VISIBLE,	fwRenderListDesc::SORT_ASCENDING_PEDS_BIASED);  // B*644704 Make peds in vehilces render after the vehicle
	fwRenderListDesc::SetSortType(RPASS_LOD,		fwRenderListDesc::SORT_ASCENDING);
	fwRenderListDesc::SetSortType(RPASS_CUTOUT,		fwRenderListDesc::SORT_DESCENDING);
	fwRenderListDesc::SetSortType(RPASS_DECAL,		fwRenderListDesc::SORT_ASCENDING);
	fwRenderListDesc::SetSortType(RPASS_FADING,		fwRenderListDesc::SORT_DESCENDING);
	fwRenderListDesc::SetSortType(RPASS_ALPHA,		fwRenderListDesc::SORT_DESCENDING);
	fwRenderListDesc::SetSortType(RPASS_WATER,		fwRenderListDesc::SORT_DESCENDING);
	fwRenderListDesc::SetSortType(RPASS_TREE,		fwRenderListDesc::SORT_ASCENDING);

	m_DeferredLists.Reserve(MAX_NUM_DEFERRED_ENTITY_LISTS);
	for(int i=0; i<MAX_NUM_DEFERRED_ENTITY_LISTS; i++)
	{
		DeferredListToProcess& deferredList = m_DeferredLists.Append();
		deferredList.m_entities.Reserve(MAX_NUM_DEFERRED_ENTITIES_PER_LIST);
	}
}

void CGtaRenderListGroup::DeferredAddEntity(int list, fwEntity* pEntity, u32 subphaseVisFlags, float sortVal, fwRenderPassId id, u16 flags)
{
	DeferredEntityData data;
	data.m_pEntity = pEntity;
	data.m_subPhaseVisFlags = subphaseVisFlags;
	data.m_sortVal = sortVal;
	data.m_flags = flags;

	int listCount = m_DeferredListsCount;
	for(int i = 0; i < listCount; i++)
	{
		if(m_DeferredLists[i].m_List == list && m_DeferredLists[i].m_renderPassId == id)
		{
			Assertf(m_DeferredLists[i].m_entities.GetCount() < MAX_NUM_DEFERRED_ENTITIES_PER_LIST, "Reached Max amount of deferred entities in a list. Please increase MAX_NUM_DEFERRED_ENTITIES_PER_LIST = %d currently", MAX_NUM_DEFERRED_ENTITIES_PER_LIST);
			if(m_DeferredLists[i].m_entities.GetCount() < MAX_NUM_DEFERRED_ENTITIES_PER_LIST)
			{
				m_DeferredLists[i].m_entities.Append() = data;
			}
			return;
		}
	}

	Assertf(m_DeferredListsCount < MAX_NUM_DEFERRED_ENTITY_LISTS, "Reached Max amount of deferred entity list. Please increase MAX_NUM_DEFERRED_ENTITY_LISTS = %d currently", MAX_NUM_DEFERRED_ENTITY_LISTS);
	if(m_DeferredListsCount < MAX_NUM_DEFERRED_ENTITY_LISTS)
	{
		DeferredListToProcess& newList = m_DeferredLists[m_DeferredListsCount++];
		newList.m_List = list;
		newList.m_renderPassId = id;
		newList.m_entities.ResetCount();
		newList.m_entities.Append() = data;
	}
}

void CGtaRenderListGroup::ProcessDeferredAdditions()
{
	int iListCount = m_DeferredListsCount;
	if(!iListCount)
	{
		return;
	}

#if SORT_RENDERLISTS_ON_SPU
	// We sometimes decide to add entities to the render list after we've already kicked off the sort render list job.
	// If we hit this point, we know that we have entities to add.  We will wait for the render list sort to complete
	// (if we don't, we risk the possibility of re-allocating the array as we add them and invalidating the addresses
	// already passed to the sort job)
	gPostScan.WaitForPostScanHelper();
	gRenderListGroup.WaitForSortEntityListJob();
#endif	//SORT_RENDERLISTS_ON_SPU

	// Now that we know the sort is done, we can add these entities to the render lists
	// and re-sort the lists that have been modified
	for(int iList = 0; iList < iListCount; iList++)
	{
		fwRenderListDesc& listDesc = gRenderListGroup.GetRenderListForPhase(m_DeferredLists[iList].m_List);
		fwRenderPassId renderPassId = m_DeferredLists[iList].m_renderPassId;
		int iEntityCount = m_DeferredLists[iList].m_entities.GetCount();
		
		for(int iEntity = 0; iEntity < iEntityCount; iEntity++)
		{
			DeferredEntityData &data = m_DeferredLists[iList].m_entities[iEntity];
			listDesc.AddEntity(data.m_pEntity, data.m_subPhaseVisFlags, data.m_sortVal, renderPassId, data.m_flags);
		}

		listDesc.SortList(renderPassId);		
	}

	gRenderListGroup.StartSortEntityListJob();

	for(int i=0; i<m_DeferredListsCount; i++)
	{
		m_DeferredLists[i].m_List = -1;
		m_DeferredLists[i].m_renderPassId = -1;
		m_DeferredLists[i].m_entities.ResetCount();
	}
	m_DeferredListsCount = 0;
}


