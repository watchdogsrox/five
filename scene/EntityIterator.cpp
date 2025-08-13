#include "EntityIterator.h"

//-------------------------------------------------------------------------
// Functions for sorting the entity cache , used by CEntityIteratorImpl
//-------------------------------------------------------------------------
int CEntityIterator_SortByDistanceLessThanFunc(const CEntityAndDistance *a, const CEntityAndDistance *b)
{
	if( a->fdistance < b->fdistance )	// Don't just sub 'em, as we return int, not float!
		return -1;
	else if( a->fdistance > b->fdistance )
		return 1;
	return 0;
}


//**************************************************************************//
// The standard entity iterator, no cache and no sorting					//
//**************************************************************************//
//-------------------------------------------------------------------------
// Constructor, sets up defaults including types to iterate over
//-------------------------------------------------------------------------
CEntityIterator::CEntityIterator( s32 iteratorFlags, const CEntity* pException, const Vec3V* pvStartPos, float fMaxDistance, float fMaxOnScreenDistance )
: m_vStartPos(V_ZERO)
{
	m_iIteratorFlags	= iteratorFlags;
	m_currentStatus		= IS_NotStarted;
	m_iCurrentIndex		= -1;
	m_pException		= pException;
	m_fMaxDistanceSq	= fMaxDistance*fMaxDistance;
	m_fMaxOnScreenDistanceSq = fMaxOnScreenDistance*fMaxOnScreenDistance;
	if( pvStartPos )
	{
		m_iIteratorFlags |= CheckDistance;
		m_vStartPos		= *pvStartPos;
	}
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CEntityIterator::~CEntityIterator()
{
}

//-------------------------------------------------------------------------
// Advances the iterator into its next mode
//-------------------------------------------------------------------------
void CEntityIterator::AdvanceIteratorStatus()
{
	while( m_currentStatus != IS_Finished )
	{
		m_currentStatus = (IteratorStatus)(m_currentStatus+1);

		switch( m_currentStatus )
		{
		case IS_IteratingPeds:
			if( m_iIteratorFlags & IteratePeds )
			{
				return;
			}
			break;
		case IS_IteratingObjects:
			if( m_iIteratorFlags & IterateObjects )
			{
				return;
			}
			break;
		case IS_IteratingDummyObjects:
			if( m_iIteratorFlags & IterateDummyObjects )
			{
				return;
			}
			break;
		case IS_IteratingBuildings:
			if( m_iIteratorFlags & IterateBuildings )
			{
				return;
			}
			break;
		case IS_IteratingAnimatedBuildings:
			if( m_iIteratorFlags & IterateAnimatedBuildings )
			{
				return;
			}
			break;
		case IS_IteratingVehicles:
			if( m_iIteratorFlags & IterateVehicles )
			{
				return;
			}
			break;
		case IS_Finished:
			return;
		case IS_NotStarted:
			Assert(0);
			break;
		}
	}
}


//-------------------------------------------------------------------------
// Gets the next ped depending on the current iterator index
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNextPed()
{
	CPed::Pool* pPool = CPed::GetPool();
	if( m_iCurrentIndex <= -1 )
	{
		m_iCurrentIndex = pPool->GetSize();
	}

	for( s32 i = m_iCurrentIndex - 1; i >= 0; i-- )
	{
		CPed* pPed = pPool->GetSlot(i);

		// Make sure it is targetable at all.
		if(	pPed )
		{
			m_iCurrentIndex = i;
			return (CEntity*) pPed;
		}
	}

	m_iCurrentIndex = -1;
	return NULL;
}


//-------------------------------------------------------------------------
// Advances the iterator into its next mode
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNextObject()
{
	CObject::Pool* pPool = CObject::GetPool();
	if( m_iCurrentIndex <= -1 )
	{
		m_iCurrentIndex = (s32) pPool->GetSize();
	}

	for( s32 i = m_iCurrentIndex - 1; i >= 0; i-- )
	{
		CObject* pObject = pPool->GetSlot(i);

		// Make sure it is targetable at all.
		if(	pObject )
		{
			m_iCurrentIndex = i;
			return (CEntity*) pObject;
		}
	}

	m_iCurrentIndex = -1;
	return NULL;
}

//-------------------------------------------------------------------------
// Advances the iterator into its next mode
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNextDummyObject()
{
	CDummyObject::Pool* pPool = CDummyObject::GetPool();
	if( m_iCurrentIndex <= -1 )
	{
		m_iCurrentIndex = pPool->GetSize();
	}

	for( s32 i = m_iCurrentIndex - 1; i >= 0; i-- )
	{
		CDummyObject* pObject = pPool->GetSlot(i);

		// Make sure it is targetable at all.
		if(	pObject )
		{
			m_iCurrentIndex = i;
			return (CEntity*) pObject;
		}
	}

	m_iCurrentIndex = -1;
	return NULL;
}

//-------------------------------------------------------------------------
// Advances the iterator into its next mode
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNextBuilding()
{
	CBuilding::Pool* pPool = CBuilding::GetPool();
	if( m_iCurrentIndex <= -1 )
	{
		m_iCurrentIndex = pPool->GetSize();
	}

	for( s32 i = m_iCurrentIndex - 1; i >= 0; i-- )
	{
		CBuilding* pObject = pPool->GetSlot(i);

		// Make sure it is targetable at all.
		if(	pObject )
		{
			m_iCurrentIndex = i;
			return (CEntity*) pObject;
		}
	}

	m_iCurrentIndex = -1;
	return NULL;
}

//-------------------------------------------------------------------------
// Advances the iterator into its next mode
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNextAnimatedBuilding()
{
	CAnimatedBuilding::Pool* pPool = CAnimatedBuilding::GetPool();
	if( m_iCurrentIndex <= -1 )
	{
		m_iCurrentIndex = pPool->GetSize();
	}

	for( s32 i = m_iCurrentIndex - 1; i >= 0; i-- )
	{
		CAnimatedBuilding* pObject = pPool->GetSlot(i);

		// Make sure it is targetable at all.
		if(	pObject )
		{
			m_iCurrentIndex = i;
			return (CEntity*) pObject;
		}
	}

	m_iCurrentIndex = -1;
	return NULL;
}


//-------------------------------------------------------------------------
// Advances the iterator into its next mode
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNextVehicle()
{
	CVehicle::Pool* pPool = CVehicle::GetPool();
	if( m_iCurrentIndex <= -1 )
	{
		m_iCurrentIndex = (s32) pPool->GetSize();
	}

	for( s32 i = m_iCurrentIndex - 1; i >= 0; i-- )
	{
		CVehicle* pVehicle = pPool->GetSlot(i);

		// Make sure it is targetable at all.
		if(	pVehicle )
		{
			m_iCurrentIndex = i;
			return (CEntity*) pVehicle;
		}
	}

	m_iCurrentIndex = -1;
	return NULL;
}


//-------------------------------------------------------------------------
// Returns the next entity to be iterated over
//-------------------------------------------------------------------------
CEntity* CEntityIterator::GetNext()
{
	const ScalarV scMaxDistSq = ScalarVFromF32(m_fMaxDistanceSq);
	const ScalarV scMaxOnScreenDistSq = ScalarVFromF32(m_fMaxOnScreenDistanceSq);
	CEntity* pEntity = NULL;
	while( m_currentStatus != IS_Finished && pEntity == NULL )
	{
		switch( m_currentStatus )
		{
		case IS_NotStarted:
			AdvanceIteratorStatus();
			break;
		case IS_IteratingPeds:
			pEntity = GetNextPed();
			if( pEntity == NULL )
			{
				AdvanceIteratorStatus();
			}
			break;
		case IS_IteratingObjects:
			pEntity = GetNextObject();
			if( pEntity == NULL )
			{
				AdvanceIteratorStatus();
			}
			break;
		case IS_IteratingDummyObjects:
			pEntity = GetNextDummyObject();
			if( pEntity == NULL )
			{
				AdvanceIteratorStatus();
			}
			break;
		case IS_IteratingBuildings:
			pEntity = GetNextBuilding();
			if( pEntity == NULL )
			{
				AdvanceIteratorStatus();
			}
			break;
		case IS_IteratingAnimatedBuildings:
			pEntity = GetNextAnimatedBuilding();
			if( pEntity == NULL )
			{
				AdvanceIteratorStatus();
			}
			break;
		case IS_IteratingVehicles:
			pEntity = GetNextVehicle();
			if( pEntity == NULL )
			{
				AdvanceIteratorStatus();
			}
			break;
		case IS_Finished:
			return NULL;
		}
		if( pEntity )
		{
			// Ignore if the exception
			if( m_pException && pEntity == m_pException ) 
			{
				pEntity = NULL;
			}
			// Ignore if outside a specified distance
			else if( ( m_iIteratorFlags & CheckDistance ) )
			{
				bool bUseOnScreenDistance = (m_iIteratorFlags & UseOnScreenPedDistance) && static_cast<CPed*>(pEntity)->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen);
				const ScalarV scDistSq = DistSquared(m_vStartPos, pEntity->GetTransform().GetPosition());
				if( bUseOnScreenDistance )
				{
					if(IsGreaterThanAll( scDistSq, scMaxOnScreenDistSq))
					{
						pEntity = NULL;
					}
				}
				else if(IsGreaterThanAll(scDistSq, scMaxDistSq))
				{
					pEntity = NULL;
				}
			}
		}
	}
	return pEntity;
}

