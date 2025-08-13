#ifndef _ENTITY_ITERATOR_H_
#define _ENTITY_ITERATOR_H_

#include "scene/entity.h"
#include "Objects/object.h"
#include "objects/DummyObject.h"
#include "scene/building.h"
#include "scene/AnimatedBuilding.h"
#include "Peds/ped.h"

#if !__SPU

enum EntityIteratorFlags
{
	IteratePeds					= (1<<0),
	IterateObjects				= (1<<1),
	IterateDummyObjects			= (1<<2),
	IterateBuildings			= (1<<3),
	IterateAnimatedBuildings	= (1<<4),
	IterateVehicles				= (1<<5),

	CheckDistance				= (1<<6),
	UseOnScreenPedDistance		= (1<<7),

	IterateAllTypes				= IteratePeds|IterateObjects|IterateDummyObjects|IterateBuildings|IterateAnimatedBuildings|IterateVehicles,
};

#define	CEntityIteratorDefaultCacheSize	(1120)		// Reflects the cumulated sizes of the pools for Peds, Objects & Vehicles on 25/5/2011

//-------------------------------------------------------------------------
// Handy class for iterating over certain entity types
//-------------------------------------------------------------------------
typedef	struct _CEntityAndDistance
{
	CEntity	*pEntity;
	float	fdistance;	// Cache distance, since we must calculate this anyway to account for fMaxDistance, otherwise 0
} CEntityAndDistance;

template <int cacheSize = CEntityIteratorDefaultCacheSize>
class CEntityIteratorImpl
{
public:

	CEntityIteratorImpl( s32 iteratorFlags, const CEntity* pException = NULL, const Vector3* pvStartPos = NULL, float fMaxDistance = 999.0f );
	~CEntityIteratorImpl();

	CEntity* GetNext();
	float GetLastSuppliedEntityDistanceSQ();
private:

	// Create some storage for nEntities ptr's
	atFixedArray< CEntityAndDistance, cacheSize > m_EntityCache;

	// Does the distance check, if required, and adds to the cache.
	void CheckDistanceAndAdd(CEntity *pEntity);

	// Makes the EntityAndDistance cache
	int	CacheMatchingEntities();

	s32				m_iCacheIndex;			// Used in GetNext()
	s32				m_iIteratorFlags;
	const CEntity*	m_pException;
	float			m_fMaxDistanceSq;
	Vector3			m_vStartPos;
};


//-------------------------------------------------------------------------
// Sorting compare func
//-------------------------------------------------------------------------
extern	int CEntityIterator_SortByDistanceLessThanFunc( const CEntityAndDistance *a, const CEntityAndDistance *b );


//-------------------------------------------------------------------------
// Constructor, sets up defaults including types to iterate over
//-------------------------------------------------------------------------
template <int cacheSize>
CEntityIteratorImpl<cacheSize>::CEntityIteratorImpl( s32 iteratorFlags, const CEntity* pException, const Vector3* pvStartPos, float fMaxDistance )
: m_vStartPos(0.0f, 0.0f, 0.0f)
{
	m_iIteratorFlags	= iteratorFlags;
	m_pException		= pException;
	m_fMaxDistanceSq	= fMaxDistance*fMaxDistance;
	if( pvStartPos )
	{
		m_iIteratorFlags |= CheckDistance;
		m_vStartPos		= *pvStartPos;
	}

	m_iCacheIndex = 0;

	// Fill it as much as we can
	s32	nCachedEntities = CacheMatchingEntities( );

	// Sort by whatever type
	if( m_iIteratorFlags & CheckDistance )
	{
		// Sort by distance less than
		m_EntityCache.QSort(0, nCachedEntities, CEntityIterator_SortByDistanceLessThanFunc );
	}
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
template <int cacheSize>
CEntityIteratorImpl<cacheSize>::~CEntityIteratorImpl()
{
}


template <int cacheSize>
void CEntityIteratorImpl<cacheSize>::CheckDistanceAndAdd(CEntity *pEntity)
{
	// pEntity == our entity or NULL
	if( pEntity )
	{
		float	distSquared = 0;

		// Ignore if the exception
		if( m_pException && pEntity == m_pException ) 
		{
			pEntity = NULL;
		}

		// Ignore if outside a specified distance
		else if( m_iIteratorFlags & CheckDistance )
		{
			distSquared = m_vStartPos.Dist2(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
			if( distSquared > m_fMaxDistanceSq )
			{
				pEntity = NULL;
			}
		}

		// His this been rejected?
		if( pEntity )
		{
			// NO, add to the cache
			CEntityAndDistance	temp;
			temp.fdistance = distSquared;
			temp.pEntity = pEntity;

			if( Verifyf( m_EntityCache.GetCount() != m_EntityCache.GetMaxCount(), "Entity Cache isn't large enough"))
			{
				m_EntityCache.Push(temp);
			}

		}
	}
}


//-------------------------------------------------------------------------
// Caches the Entities that match the passed flags
//-------------------------------------------------------------------------

template <int cacheSize>
int	CEntityIteratorImpl<cacheSize>::CacheMatchingEntities()
{
	CEntity *pEntity = NULL;

	// PEDS
	if( m_iIteratorFlags & IteratePeds )
	{
		CPed::Pool* pPool = CPed::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			pEntity = pPool->GetSlot(i);
			CheckDistanceAndAdd(pEntity);
		}
	}

	// OBJECTS
	if( m_iIteratorFlags & IterateObjects )
	{
		CObject::Pool* pPool = CObject::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			pEntity = pPool->GetSlot(i);
			CheckDistanceAndAdd(pEntity);
		}
	}

	// DUMMY OBJECTS
	if( m_iIteratorFlags & IterateDummyObjects )
	{
		CDummyObject::Pool* pPool = CDummyObject::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			pEntity = pPool->GetSlot(i);
			CheckDistanceAndAdd(pEntity);
		}
	}

	// BUILDINGS
	if( m_iIteratorFlags & IterateBuildings )
	{
		CBuilding::Pool* pPool = CBuilding::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			pEntity = pPool->GetSlot(i);
			CheckDistanceAndAdd(pEntity);
		}
	}

	// ANIMATED BUILDINGS
	if( m_iIteratorFlags & IterateAnimatedBuildings )
	{
		CAnimatedBuilding::Pool* pPool = CAnimatedBuilding::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			pEntity = pPool->GetSlot(i);
			CheckDistanceAndAdd(pEntity);
		}
	}


	// VEHICLES
	if( m_iIteratorFlags & IterateVehicles )
	{
		CVehicle::Pool* pPool = CVehicle::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			pEntity = pPool->GetSlot(i);
			CheckDistanceAndAdd(pEntity);
		}
	}
	return m_EntityCache.GetCount();		// The count of the number of entities in the cache
}

//-------------------------------------------------------------------------
// Returns the next entity to be iterated over
//-------------------------------------------------------------------------
template <int cacheSize>
CEntity* CEntityIteratorImpl<cacheSize>::GetNext()
{
	CEntity *pEntity = NULL;

	if( m_iCacheIndex < m_EntityCache.GetCount() )
	{
		pEntity = m_EntityCache[m_iCacheIndex].pEntity;
		m_iCacheIndex++;
	}
	return pEntity;
}

//-------------------------------------------------------------------------
// Returns the distance squared of the last entity returned from GetNext()
//-------------------------------------------------------------------------
template <int cacheSize>
float CEntityIteratorImpl<cacheSize>::GetLastSuppliedEntityDistanceSQ()
{
	int		index = m_iCacheIndex-1;
	float	distSQ = 0.0f;
	if( index >= 0 && index < m_EntityCache.GetCount() )
	{
		distSQ = m_EntityCache[index].fdistance;
	}
	return distSQ;
}

typedef	CEntityIteratorImpl<CEntityIteratorDefaultCacheSize>	CEntityIteratorSorted;





//**************************************************************************//
// The standard entity iterator, no cache and no sorting					//
//**************************************************************************//

class CEntityIterator
{
public:

	CEntityIterator( s32 iteratorFlags, const CEntity* pException = NULL, const Vec3V* pvStartPos = NULL, float fMaxDistance = 999.0f, float fMaxOnScreenDistance = 999.9f );
	~CEntityIterator();

	CEntity* GetNext();

private:

	enum IteratorStatus { 
		IS_NotStarted = 0, 
		IS_IteratingPeds,
		IS_IteratingObjects,
		IS_IteratingDummyObjects,
		IS_IteratingBuildings,
		IS_IteratingAnimatedBuildings,
		IS_IteratingVehicles,
		IS_Finished		
	};

	void AdvanceIteratorStatus();

	CEntity* GetNextPed();
	CEntity* GetNextObject();
	CEntity* GetNextDummyObject();
	CEntity* GetNextVehicle();
	CEntity* GetNextBuilding();
	CEntity* GetNextAnimatedBuilding();


	s32			m_iCurrentIndex;
	s32			m_iIteratorFlags;
	IteratorStatus	m_currentStatus;
	const CEntity*	m_pException;
	float			m_fMaxDistanceSq;
	float			m_fMaxOnScreenDistanceSq;
	Vec3V			m_vStartPos;
};

#endif	//!__SPU

#endif	// _ENTITY_ITERATOR_H_
