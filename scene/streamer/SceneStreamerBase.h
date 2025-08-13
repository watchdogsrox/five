/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamerBase.h
// PURPOSE : base class defining interface to scene streamer
// AUTHOR :  John Whyte, Ian Kiigan
// CREATED : 06/08/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_STREAMER_SCENESTREAMERBASE_H_
#define _SCENE_STREAMER_SCENESTREAMERBASE_H_

#include "fwtl/Pool.h"
#if !__SPU
#include "pickups/Pickup.h"
#include "scene/Entity.h"
#endif // !__SPU
#include "atl/array.h"
#include "vector/vector3.h"

#define THOROUGH_VALIDATION (!__PS3 && 0)

class CRenderPhase;
class CEntity;
class CPortalInst;
class CSceneStreamerList;

// Streaming bucket identifiers. Be sure to keep this in sync with s_StreamingBucketNames.
enum StreamBucket
{
	STREAMBUCKET_ACTIVE_OUTSIDE_PVS,	// In active list, but not in the PVS/PSS.
	STREAMBUCKET_INVISIBLE_FAR,			// delete objects that are not in the frustum and are far enough away that they are not visible
	STREAMBUCKET_INVISIBLE_EXTERIOR,	// delete objects that are not in the frustum and in the exterior if you are in an interior
	STREAMBUCKET_VEHICLESANDPEDS_CUTSCENE,	// deletes vehicles and peds while in a cutscene
	STREAMBUCKET_INVISIBLE_OTHERROOMS,	// delete objects that are not in the frustum and in the current interior but not in the current room
	STREAMBUCKET_INVISIBLE,				// delete objects that are not in the frustum
	STREAMBUCKET_VISIBLE_FAR,			// delete objects that are in the frustum and are far enough away that they are not visible
	STREAMBUCKET_VEHICLESANDPEDS,		// delete vehicles and peds
	STREAMBUCKET_VISIBLE,				// delete everything
	STREAMBUCKET_ACTIVE_PED,			// Active peds

	STREAMBUCKET_COUNT
};

// base class for all scene streamers, defines common interface
class CSceneStreamerBase
{
public:

	enum
	{
		PRIORITYCOUNT_MIN	= 0,
		SECONDARYLIST_MAX	= 200,
	};

						CSceneStreamerBase();
	virtual				~CSceneStreamerBase() {}

	void				Reset();
	virtual bool		IsHighPriorityEntity(const CEntity* RESTRICT pEntity, const Vector3& vCamDir, const Vector3& vCamPos) const = 0;

	void SetEnableIssueRequests(bool enabled)	{ m_EnableIssueRequests = enabled; }
	bool GetEnableIssueRequests() const			{ return m_EnableIssueRequests; }

protected:
	static float		ms_fScreenYawForPriorityConsideration;
	static float		ms_fHighPriorityYaw;

	s32 m_nPriorityCount;
	atFixedArray<CEntity*, SECONDARYLIST_MAX> m_secondaryEntityList;

	bool m_EnableIssueRequests;
};

// data used by sortbucketsspu when adding active entities to the list
struct AddActiveEntitiesData
{
	u32*	m_EntityListCountPtr;
	void*	m_pStore;
	void*	m_pFirst;
	void*	m_pLast;
	u32		m_frameCount;
#if !__FINAL
	u32*	m_DumpAllEntitiesPtr;
#endif
#if __BANK
	u32*	m_BucketCounts;
#endif
#if __ASSERT
	u32*	m_DataInUsePtr;
	bool	m_bIsAnyVolumeActive;
#endif
};

struct BucketEntry
{
	enum
	{
		// Max number of bucket entries in a streaming list
		ENTITY_TYPE_OBJECT_PICKUP = ENTITY_TYPE_TOTAL,

		BUCKETENTRY_MAX_ENTITY_TYPES,

		// Number of BucketEntry's added in a single batch from
		// the sortbuckets job
		BUCKET_ENTRY_BATCH_SIZE	= 1024
	};

	typedef atArray<BucketEntry, 16, u32> BucketEntryArray;

#if !__SPU
	static u32 CalculateEntityID(fwEntity* entity);

	bool IsStillValid() const;

	CEntity *GetValidatedEntity() const		{ return (IsStillValid()) ? m_Entity : NULL; }
#endif // !__SPU

	void SetEntity(CEntity *entity, u32 entityID);

	CEntity *GetEntity() const				{ return m_Entity; }

	// Score of this bucket. The integer is the bucket index, the decimal is the
	// importance within the bucket, 0.0 being the least important and most likely to get removed.
	float m_Score;

private:
	CEntity *m_Entity;

	// Entity ID to verify the entry.
	u32 m_EntityID;

#if THOROUGH_VALIDATION
	// The pool that this entity is stored in.
	// Vehicles and Objects are not fwBasePool-derived anymore so this may be NULL!
	fwBasePool *m_Pool;
#endif

};

__forceinline void BucketEntry::SetEntity(CEntity *entity, u32 entityID)
{
	m_Entity = entity;
	m_EntityID = entityID;

	if (entity)
	{
#if THOROUGH_VALIDATION
		u32 entityType = entity->GetType();
		fwBasePool* pPool = NULL;
		Assert(entityType != ENTITY_TYPE_VEHICLE);
		if (entityType == ENTITY_TYPE_OBJECT && ((CObject*)entity)->m_nObjectFlags.bIsPickUp)
		{
			entityType = ENTITY_TYPE_OBJECT_PICKUP;
			pPool = CPickup::GetPool();
		}
#if __CONSOLE
		else if (entityType == ENTITY_TYPE_OBJECT)
		{
			// leave pPool as NULL because objects are not in a fwBasePool
		}
#endif
		else
		{
			pPool = GetEntityPoolFromType(entityType);
		}

		m_Pool = pPool;
		Assert(m_EntityID == CalculateEntityID(entity));
#endif // THOROUGH_VALIDATION

#if !__SPU
		Assert(IsStillValid());
#endif

	}
}

#if !__SPU
inline bool BucketEntry::IsStillValid() const
{
	if(!m_Entity)
	{
		return false;
	}

	u32 id = m_EntityID;
	u32 poolSlot = id & 0x00ffffff;
	u32 entityType = id >> 24;
	fwBasePool* pPool = NULL;

	Assert(entityType > ENTITY_TYPE_NOTHING && entityType < BUCKETENTRY_MAX_ENTITY_TYPES);
	Assert(entityType != ENTITY_TYPE_VEHICLE);

	if (entityType == ENTITY_TYPE_OBJECT_PICKUP)
	{
		pPool = CPickup::GetPool();
	}
	// HACK: Vehicle and Object aren't derived from fwBasePool anymore!
	else if (entityType == ENTITY_TYPE_OBJECT)
	{
		CEntity* pEntity = static_cast<CEntity*>(CObject::GetPool()->GetAt(poolSlot));
		if (pEntity)
		{
			Assertf(CObject::GetPool()->IsValidPtr(pEntity), "CEntitySelect::GetEntityFromID: entity %p isn't valid type %d, this is bad!", pEntity, id >> 24);
			Assertf(pEntity == m_Entity, "Entity %p isn't equal to entity %p, this is bad!", pEntity, m_Entity);
			return true;
		}
		
		return false;
	}
	else
	{
		pPool = GetEntityPoolFromType(entityType);
	}

#if THOROUGH_VALIDATION
	Assert(pPool == m_Pool);
#endif // THOROUGH_VALIDATION

	if (pPool) 
	{
		CEntity *pEntity = static_cast<CEntity*>(pPool->GetAt(poolSlot));
		if (pEntity)
		{
			Assertf(pPool->IsValidPtr(pEntity), "CEntitySelect::GetEntityFromID: entity %p isn't valid type %d, this is bad!", pEntity, id >> 24);
			Assert(pEntity == m_Entity);
			return true;
		}
	}

	return false;
}

#endif // !__SPU

#endif //_SCENE_STREAMER_SCENESTREAMERBASE_H_
