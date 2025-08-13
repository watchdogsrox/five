/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamerBase.cpp
// PURPOSE : base class defining interface to scene streamer
// AUTHOR :  John Whyte, Ian Kiigan
// CREATED : 06/08/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/streamer/SceneStreamerBase.h"

#include "game/Clock.h"
#include "modelinfo/TimeModelInfo.h"
#include "scene/lod/LodMgr.h"
#include "scene/portals/Portal.h"
#include "scene/portals/PortalInst.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "vector/vector3.h"

SCENE_OPTIMISATIONS();

float		CSceneStreamerBase::ms_fScreenYawForPriorityConsideration	= 5.0f;		//in degrees
float		CSceneStreamerBase::ms_fHighPriorityYaw						= 10.0f;	//in degrees

CSceneStreamerBase::CSceneStreamerBase()
: m_EnableIssueRequests(true)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:		clear counters and empty secondary array
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerBase::Reset()
{
	m_nPriorityCount = 0;
	m_secondaryEntityList.Reset();
}

u32 BucketEntry::CalculateEntityID(fwEntity* entity)
{
	if(!entity)
	{
		return 0;
	}

	u32 entityType = entity->GetType();
	u32 poolSlot = 0;
	fwBasePool* pPool = NULL;

	Assert(entityType != ENTITY_TYPE_VEHICLE);

	if (entityType == ENTITY_TYPE_OBJECT && ((CObject*)entity)->m_nObjectFlags.bIsPickUp)
	{
		entityType = ENTITY_TYPE_OBJECT_PICKUP;
		pPool = CPickup::GetPool();
	}
	else if (entityType == ENTITY_TYPE_OBJECT)
	{
		// leave pPool as NULL because objects don't come from a fwBasePool anymore
		poolSlot = CObject::GetPool()->GetIndex(entity);
#if THOROUGH_VALIDATION
		TrapNE(poolSlot & 0xff000000, 0U);
		Assert(static_cast<fwEntity*>(CObject::GetPool()->GetAt(poolSlot)) == entity);
		TrapNE((u32) (static_cast<fwEntity*>(CObject::GetPool()->GetAt(poolSlot)) == entity), (u32) 1);
		TrapLE((u32) entityType, (u32) ENTITY_TYPE_NOTHING);
		TrapGE((u32) entityType, (u32) BUCKETENTRY_MAX_ENTITY_TYPES);
#endif // THOROUGH_VALIDATION

	}

	else 
	{
		pPool = GetEntityPoolFromType(entityType);
	}

	if(pPool)
	{
		poolSlot = pPool->GetIndexFast(entity);

#if THOROUGH_VALIDATION
		TrapNE(poolSlot & 0xff000000, 0U);
		Assert(static_cast<fwEntity*>(pPool->GetAt(poolSlot)) == entity);
		TrapNE((u32) (static_cast<fwEntity*>(pPool->GetAt(poolSlot)) == entity), (u32) 1);
		TrapLE((u32) entityType, (u32) ENTITY_TYPE_NOTHING);
		TrapGE((u32) entityType, (u32) BUCKETENTRY_MAX_ENTITY_TYPES);
#endif // THOROUGH_VALIDATION
	}

	return ( (entityType << 24) | poolSlot);
}
