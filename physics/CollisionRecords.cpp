 // 
// physics/CollisionRecords.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "CollisionRecords.h"

//Rage headers
#include "fwsys\timer.h"
#include "physics/iterator.h"

//Game headers
#include "physics\physics.h"
#include "scene\Physical.h"
#include "Vehicles\vehicle.h"
#include "Peds/Ped.h"
#include "Objects/Object.h"
#include "Pickups/Pickup.h"

PHYSICS_OPTIMISATIONS()

#if __BANK
float CCollisionHistory::ms_fDebugRenderContactRadius = 0.1f;
float CCollisionHistory::ms_fDebugRenderContactNormal = 2.0f;
int   CCollisionHistory::ms_nDebugRenderPersistFrames = 1;
	#if DEBUG_MEASURE_COLLISION_RECORD_HIGH_WATER_MARK
		int CCollisionHistory::ms_nHighWaterMark = 0;
		int CCollisionHistory::ms_nHighWaterMarkLastFrame = 0;
	#endif
#endif

atFixedArray<CCollisionRecord, COLLISIONHISTORY_RECORDSTORESIZE> CCollisionHistory::ms_recordStore;
u32 CCollisionHistory::ms_CurrentFrame = 0;
//CCollisionHistory
CCollisionHistory::CCollisionHistory(bool bUseCollisionRecords)
{
	m_bHistoryRecorded = bUseCollisionRecords;
	m_mostRecentCollisionTime = COLLISION_HISTORY_TIMER_NEVER_HIT;
	Reset();
}

//Destruct and free the records
void CCollisionHistory::ClearRecords() 
{
	//Call destructor on and free every record
	CCollisionRecord* pRecordArray = ms_recordStore.GetElements();
	const int n = ms_recordStore.GetCount();

	for(int i = 0 ; i < n ; i++)
	{
		pRecordArray->~CCollisionRecord(); //To unregister the registered reference
		Assert( pRecordArray->m_pRegdCollisionEntity.Get() == NULL ); //Check that we certainly unregistered
		pRecordArray++;
	}

	#if DEBUG_MEASURE_COLLISION_RECORD_HIGH_WATER_MARK
		ms_nHighWaterMarkLastFrame = ms_recordStore.GetCount();
		ms_nHighWaterMark = Max(ms_nHighWaterMark, ms_recordStore.GetCount());
	#endif

	ms_recordStore.Reset();


	// Increase the current frame count so all collision records aren't current. This means
	//   they will have to be reset before they can be used. 
	ms_CurrentFrame++;

}

// Assumes that the entire CCollisionRecords store is also being freed as part of the reset process, so we just need to NULL the linked list heads.
// This is expected to be called each frame
void CCollisionHistory::Reset()
{
	m_pPedCollisions = NULL;
	m_pVehicleCollisions = NULL;
	m_pObjectCollisions = NULL;
	m_pBuildingCollisions = NULL;
	m_pOtherCollisions = NULL;
	m_pMostSignificantCollision = NULL;

	m_collidedEntityTypesThisFrame = 0;
	m_collidedEntityTypes = 0;
	m_numCollidedEntities = 0;
	m_fCollisionImpulseMagMax = 0.0f;
	m_fCollisionImpulseMagSum = 0.0f;

	m_bHistoryCompleteThisFrame = false;

	// Now that the history is reset it is valid for this frame. 
	m_LastValidFrame = ms_CurrentFrame;
}

void CCollisionHistory::NotifyHistoryWillBeCompleteThisTick()
{
	if( m_bHistoryRecorded )
		m_bHistoryCompleteThisFrame = true; //Regard complete information from one tick to be good enough for a frame's history to be regarded complete
}

//Returns NULL if no record exists
const CCollisionRecord* CCollisionHistory::FindCollisionRecord(const CEntity* pEntityToFind) const
{
	int type = pEntityToFind->GetType();
	CCollisionRecord* curRecord;

	if(!HasCollidedWithAnyOfTypes(1 << type))
	{
		return NULL;
	}

	switch(type)
	{
		case ENTITY_TYPE_BUILDING:
		case ENTITY_TYPE_ANIMATED_BUILDING:
			curRecord = m_pBuildingCollisions;
			break;

		case ENTITY_TYPE_VEHICLE:
			curRecord = m_pVehicleCollisions;
			break;

		case ENTITY_TYPE_PED:
			curRecord = m_pPedCollisions;
			break;

		case ENTITY_TYPE_OBJECT:
			curRecord = m_pObjectCollisions;
			break;

		default:
			curRecord = m_pOtherCollisions;
			break;
	}

	while(curRecord != NULL)
	{
		ptrdiff_t pointerDiff = curRecord->m_ListSortKey - pEntityToFind;

		if( pointerDiff > 0 )
		{
			return NULL;
		}
		else if( pointerDiff == 0 )
		{
			return curRecord;
		}
		else
		{
			curRecord = curRecord->m_pNext;
		}
	}

	return NULL;
}

//Returns a reference to the pointer that is to be made to point to the new collision record.
//If the record already exists then returns a reference to the pointer to it
//CCollisionRecords are hashed into buckets based on type.
//A bucket's data is stored in a linked list which is always sorted by CEntity address
CCollisionRecord*& CCollisionHistory::GetInsertionPoint(CEntity* pEntityToInsert)
{
	CCollisionRecord** ppRecord;

	int type = pEntityToInsert->GetType();

	switch(type)
	{
		case ENTITY_TYPE_BUILDING:
		case ENTITY_TYPE_ANIMATED_BUILDING:
			ppRecord = &m_pBuildingCollisions;
			break;

		case ENTITY_TYPE_VEHICLE:
			ppRecord = &m_pVehicleCollisions;
			break;

		case ENTITY_TYPE_PED:
			ppRecord = &m_pPedCollisions;
			break;

		case ENTITY_TYPE_OBJECT:
			ppRecord = &m_pObjectCollisions;
			break;

		default:
			ppRecord = &m_pOtherCollisions;
			break;
	}


	while(*ppRecord != NULL)
	{
		const CEntity* pCurrentEntity = (*ppRecord)->m_ListSortKey;
		ptrdiff_t pointerDiff = pCurrentEntity - pEntityToInsert;

		if( pointerDiff >= 0 ) //We've either found the current entity in a record or the current record is has a larger CEntity address so we insert before it
		{
			return *ppRecord;
		}
		else //Insertion entity's address is larger than this record's entity address (and all others before it), so go to the next record
		{
			ppRecord = &((*ppRecord)->m_pNext);
		}
	}

	return *ppRecord;
}

//This function needs to be really efficient due to a heavy load
//Query IsHistoryRecorded to decide whether to call this
void CCollisionHistory::AddCollisionRecord(phInst const * pMyInst, CEntity* pOtherEntity, const Vector3& vecImpulse, const Vector3& vecNormal,
										   const Vector3& vecMyPos, const Vector3& vecOtherPos, int nMyComp, int nOtherComp,
										   phMaterialMgr::Id nMat, bool bPositiveDepth, bool bIsNewContact)
{
#if DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS
	DebugValidateLinkedLists();
#endif
	Assertf(IsCurrent(),"Trying to add collision record to non-current collision history.");

	ASSERT_ONLY(if(!AssertVerify(pOtherEntity)) return;)

	// Only contacts with positive depth or non-zero impulse are to be considered.
	if(vecImpulse.IsZero() && !bPositiveDepth)
	{
		return;
	}

	float fImpulseMag = vecImpulse.Mag();
	CCollisionRecord*& pRecord = GetInsertionPoint(pOtherEntity); //Pointer that will be made to point to the new record (or pointer to the existing record)

	if( pRecord == NULL || pRecord->m_ListSortKey != pOtherEntity ) //No record involving this entity exists
	{

#if DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS
		if( pRecord != NULL && pRecord->m_ListSortKey != pOtherEntity )
		{
			//This new record is going to go before some other record. Check that the other record has a larger entity address
			Assert( pRecord->m_ListSortKey > pEntity );
		}
#endif
		
		// Create a new collision record here in the list and fill it in.
		CCollisionRecord* pNewRecord = ms_recordStore.IsFull() ? NULL : &ms_recordStore.Append();

		if( physicsVerifyf(pNewRecord, "Unable to allocate a CCollisionRecord") )
		{
			pNewRecord->m_ListSortKey = pOtherEntity;

			Assert( pNewRecord->m_pRegdCollisionEntity.Get() == NULL ); //Check that we are being allocated something fresh
			pNewRecord->m_pRegdCollisionEntity = pOtherEntity;

			pNewRecord->m_fCollisionImpulseMag = fImpulseMag;
			pNewRecord->m_MyCollisionNormal = vecNormal;
			pNewRecord->m_MyCollisionPos = vecMyPos;
			Matrix34 myMat = MAT34V_TO_MATRIX34(pMyInst->GetMatrix());
			myMat.UnTransform(vecMyPos, pNewRecord->m_MyCollisionPosLocal);
			pNewRecord->m_MyCollisionComponent = nMyComp;
			pNewRecord->m_OtherCollisionComponent = nOtherComp;
			pNewRecord->m_OtherCollisionMaterialId = nMat;
			pNewRecord->m_OtherCollisionPos = vecOtherPos;

			pNewRecord->m_pNext = pRecord;
			pRecord = pNewRecord;
		}
		else
		{
			m_bHistoryCompleteThisFrame = false;
		}

		m_numCollidedEntities++;
		SetCollidedWithTypes(1 << pOtherEntity->GetType());
	}
	else //A record already exists
	{
		//Replace if new collision has greater impulse magnitude
		if(pRecord->m_fCollisionImpulseMag < fImpulseMag)
		{
			pRecord->m_fCollisionImpulseMag = fImpulseMag;
			pRecord->m_MyCollisionNormal = vecNormal;
			pRecord->m_MyCollisionPos = vecMyPos;
			Matrix34 myMat = MAT34V_TO_MATRIX34(pMyInst->GetMatrix());
			myMat.UnTransform(vecMyPos, pRecord->m_MyCollisionPosLocal);
			pRecord->m_MyCollisionComponent = nMyComp;
			pRecord->m_OtherCollisionComponent = nOtherComp;
			pRecord->m_OtherCollisionMaterialId = nMat;
			pRecord->m_OtherCollisionPos = vecOtherPos;
		}
	}

	if(bIsNewContact)
	{
		SetCollidedWithTypesThisFrame(1 << pOtherEntity->GetType());
	}

	if( pRecord != NULL 
		&& ( m_pMostSignificantCollision == NULL || pRecord->m_fCollisionImpulseMag > m_pMostSignificantCollision->m_fCollisionImpulseMag ) )
	{
		m_pMostSignificantCollision = pRecord;
	}

	m_mostRecentCollisionTime = fwTimer::GetTimeInMilliseconds();

	m_fCollisionImpulseMagMax = Max(fImpulseMag, m_fCollisionImpulseMagMax);
	m_fCollisionImpulseMagSum += fImpulseMag;

#if DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS
	DebugValidateLinkedLists();
#endif
}

void CCollisionHistory::PreSimUpdate()
{
	for(int levelIndex = PHLEVEL->GetFirstActiveIndex();
		levelIndex != phInst::INVALID_INDEX;
		levelIndex = PHLEVEL->GetNextActiveIndex() )
	{
		phInst* pInst = CPhysics::GetLevel()->GetInstance(levelIndex);
		CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);

		if(pEntity && pEntity->GetIsPhysical())
		{
			CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
			pPhysical->GetFrameCollisionHistory()->NotifyHistoryWillBeCompleteThisTick();
		}
	}
}

const CCollisionRecord* CCollisionHistory::GetMostSignificantCollisionRecordOfType(u16 entityType) const
{
	if(!HasCollidedWithAnyOfTypes(1 << entityType))
	{
		return NULL;
	}

	float fMaxImpulse = 0.0f;
	CCollisionRecord* pMaxRecord = NULL;
	
	CCollisionRecord* pCurRecord;

	switch(entityType)
	{
		case ENTITY_TYPE_BUILDING:
		case ENTITY_TYPE_ANIMATED_BUILDING:
			pCurRecord = m_pBuildingCollisions;
			break;

		case ENTITY_TYPE_VEHICLE:
			pCurRecord = m_pVehicleCollisions;
			break;

		case ENTITY_TYPE_PED:
			pCurRecord = m_pPedCollisions;
			break;

		case ENTITY_TYPE_OBJECT:
			pCurRecord = m_pObjectCollisions;
			break;

		default:
			pCurRecord = m_pOtherCollisions;
			break;
	}

	while(pCurRecord != NULL)
	{
		CEntity* pEntity = pCurRecord->m_pRegdCollisionEntity.Get();

		if(pEntity && pEntity->GetType() == entityType
			&& fMaxImpulse < pCurRecord->m_fCollisionImpulseMag)
		{
			fMaxImpulse = pCurRecord->m_fCollisionImpulseMag;
			pMaxRecord = pCurRecord;
		}

		pCurRecord = pCurRecord->m_pNext;
	}

	return pMaxRecord;
}

float CCollisionHistory::GetMaxCollisionImpulseMagLastFrameOfType(u16 entityType) const
{
	const CCollisionRecord* pMaxRecord = GetMostSignificantCollisionRecordOfType(entityType);

	return pMaxRecord ? pMaxRecord->m_fCollisionImpulseMag : 0.0f;
}

const CCollisionRecord* CCollisionHistory::GetMostSignificantCollisionRecordOfTypes(u16 entityTypeMask) const
{
	if(!HasCollidedWithAnyOfTypes(entityTypeMask))
	{
		return NULL;
	}

	float fMaxImpulse = 0.0f;
	const CCollisionRecord* pMaxRecord = NULL;

	u16 type = 0;

	while(entityTypeMask)
	{
		if(entityTypeMask & 1u)
		{
			const CCollisionRecord* pMaxRecordOfType = GetMostSignificantCollisionRecordOfType(type);
			
			if( pMaxRecordOfType != NULL && fMaxImpulse < pMaxRecordOfType->m_fCollisionImpulseMag )
			{
				fMaxImpulse = pMaxRecordOfType->m_fCollisionImpulseMag;
				pMaxRecord = pMaxRecordOfType;
			}
		}

		type++;
		entityTypeMask >>= 1;
	}

	return pMaxRecord;
}

float CCollisionHistory::GetMaxCollisionImpulseMagLastFrameOfTypes(u16 entityTypeMask) const
{
	const CCollisionRecord* pMaxRecord = GetMostSignificantCollisionRecordOfTypes(entityTypeMask);

	return pMaxRecord ? pMaxRecord->m_fCollisionImpulseMag : 0.0f;
}

#if DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS
//Let's leave this in for a while whilst the collision record code settles; these were returning false sometimes
//because CCollisionHistory.Reset() wasn't being called on some CPhysicals every frame.
//We probably call Reset() on more than is necessary now.
bool CCollisionHistory::DebugValidateLinkedLists() const
{
	bool bValid = true;

	bValid &= DebugValidateLinkedList( GetFirstBuildingCollisionRecord(), ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING, "building" );
	bValid &= DebugValidateLinkedList( GetFirstVehicleCollisionRecord() , ENTITY_TYPE_MASK_VEHICLE, "vehicle" );
	bValid &= DebugValidateLinkedList( GetFirstPedCollisionRecord(), ENTITY_TYPE_MASK_PED, "ped" );
	bValid &= DebugValidateLinkedList( GetFirstObjectCollisionRecord(), ENTITY_TYPE_MASK_OBJECT, "object" );

	return bValid;
}

bool CCollisionHistory::DebugValidateLinkedList(const CCollisionRecord* pList, u16 entityTypesInList, const char* listName) const
{
	bool bValid = true;

	const CCollisionRecord* pIterator1;
	const CCollisionRecord* pIterator2;
	const CEntity* pPrevious = NULL;
	int counter = 0;

	//Empty list is fine
	if(pList == NULL)
	{
		return true;
	}

	//Check that the list contains no loops and is not extremely long
	pIterator1 = pList;
	pIterator2 = pList;
	
	while(true)
	{
		if(pIterator2->m_pNext == NULL || pIterator2->m_pNext->m_pNext == NULL)
		{
			//We got to the end of the list
			break;
		}

		pIterator1 = pIterator1->m_pNext;
		pIterator2 = pIterator2->m_pNext->m_pNext;

		if(pIterator1 == pIterator2)
		{
			Assertf(false, "Found a loop in a %s collision record linked list", listName);
			return false;
		}

		if(counter++ == 100)
		{
			Assertf(false, "A %s collision record linked list is incredibly long", listName);
			return false;
		}
	}
	
	//Check that list is in ascending order of entity address
	for(pIterator1 = pList; pIterator1 != NULL; pIterator1 = pIterator1->m_pNext)
	{
		const CEntity* pCurrent = pIterator1->m_ListSortKey;

		if(pCurrent <= pPrevious)
		{
			Assertf(false, "A %s collision record linked list is not in strictly ascending order of entity address", listName);
			bValid = false;
			break;
		}

		pPrevious = pCurrent;
	}

	//Check that everything in the list of the correct type for the list
	for(pIterator1 = pList; pIterator1 != NULL; pIterator1 = pIterator1->m_pNext)
	{
		const CEntity* pCurrent = pIterator1->m_pRegdCollisionEntity.Get();

		if(pCurrent != NULL)
		{
			u16 typeAsMask = 1 << pCurrent->GetType();

			if((typeAsMask & entityTypesInList) == 0u)
			{
				Assertf(false, "A %s collision record linked list contains an entity of the wrong type", listName);
				bValid = false;
				break;
			}
		}
	}

	return bValid;
}

#endif //DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS

#if __BANK

void CCollisionHistory::DebugRenderTextLineNormalisedScreenSpace(const char* text, Vec2V_In screenPos, const Color32 textCol, const int lineNum)
{
	Vec2V lineScreenPos = screenPos + Vec2V(0.0f, 0.0125f * lineNum);
	grcDebugDraw::Text(lineScreenPos, textCol, text);
}

void CCollisionHistory::DebugRenderTextLineWorldSpace(const char* text, const Vector3& worldPos, const Color32 textCol, const int lineNum, int persistFrames)
{
	grcDebugDraw::Text(worldPos, textCol, 0, (grcDebugDraw::GetScreenSpaceTextHeight()-1) * lineNum, text, true, persistFrames);
}

void CCollisionHistory::DebugRenderCollisionRecordList(const CCollisionRecord* pList, const char * listName, const Color32 recordCol, Vec2V_In summaryScreenPos, int summaryLineNum, const bool bRenderText, int persistFrames)
{
	//Each record drawn at its location
	for(const CCollisionRecord* pRecord = pList; pRecord != NULL; pRecord = pRecord->m_pNext)
	{
		//Draw a point and normal (both directions so there's a greater chance of seeing it)
		grcDebugDraw::Sphere(pRecord->m_MyCollisionPos, ms_fDebugRenderContactRadius, recordCol, /*bool solid =*/ true, persistFrames);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(pRecord->m_MyCollisionPos), VECTOR3_TO_VEC3V(pRecord->m_MyCollisionPos - ms_fDebugRenderContactNormal * pRecord->m_MyCollisionNormal), ms_fDebugRenderContactRadius, recordCol, persistFrames);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(pRecord->m_MyCollisionPos), VECTOR3_TO_VEC3V(pRecord->m_MyCollisionPos + ms_fDebugRenderContactNormal * pRecord->m_MyCollisionNormal), ms_fDebugRenderContactRadius, recordCol, persistFrames);
	}

	if(bRenderText)
	{
		const int tempStringLen = 512;
		char summaryLineString[tempStringLen];
		char recordLineString[tempStringLen];
		char materialName[tempStringLen];
		int i = 0;
		int currentCharNum = 0;
	
		currentCharNum += snprintf(summaryLineString, tempStringLen, "%ss: ", listName);

		for(const CCollisionRecord* pRecord = pList; pRecord != NULL; pRecord = pRecord->m_pNext, i++)
		{
			//Write the record details at the point
			snprintf(recordLineString, tempStringLen, "[%s %d]", listName, i);
			DebugRenderTextLineWorldSpace(recordLineString, pRecord->m_MyCollisionPos, Color_black, 1, persistFrames);
			
			snprintf(recordLineString, tempStringLen, "this CCollisionRecord: 0x%p. m_pNext: 0x%p", pRecord, pRecord->m_pNext);
			DebugRenderTextLineWorldSpace(recordLineString, pRecord->m_MyCollisionPos, Color_black, 2, persistFrames);

			snprintf(recordLineString, tempStringLen, "CEntity: 0x%p. RegdEnt: 0x%p. RegdEnt.GetType: 0x%x", pRecord->m_ListSortKey, pRecord->m_pRegdCollisionEntity.Get(), pRecord->m_pRegdCollisionEntity.Get() == NULL ? 0 : pRecord->m_pRegdCollisionEntity.Get()->GetType());
			DebugRenderTextLineWorldSpace(recordLineString, pRecord->m_MyCollisionPos, Color_black, 3, persistFrames);

			snprintf(recordLineString, tempStringLen, "ImpulseMag: %f", pRecord->m_fCollisionImpulseMag);
			DebugRenderTextLineWorldSpace(recordLineString, pRecord->m_MyCollisionPos, Color_black, 4, persistFrames);

			snprintf(recordLineString, tempStringLen, "My Component: %d. Other Component: %d", pRecord->m_MyCollisionComponent, pRecord->m_OtherCollisionComponent);
			DebugRenderTextLineWorldSpace(recordLineString, pRecord->m_MyCollisionPos, Color_black, 5, persistFrames);

			MATERIALMGR.GetMaterialName(pRecord->m_OtherCollisionMaterialId, materialName, tempStringLen);
			snprintf(recordLineString, tempStringLen, "Other Material Name: %s", materialName);
			DebugRenderTextLineWorldSpace(recordLineString, pRecord->m_MyCollisionPos, Color_black, 6, persistFrames);

			//Summary in the top left of the screen
			currentCharNum += snprintf(&summaryLineString[currentCharNum], tempStringLen - currentCharNum, "[&Rec:0x%p &Ent:0x%p &RegdEnt:0x%p pNext:0x%p] ",
										pRecord, pRecord->m_ListSortKey, pRecord->m_pRegdCollisionEntity.Get(), pRecord->m_pNext);
		}

		DebugRenderTextLineNormalisedScreenSpace(summaryLineString, summaryScreenPos, Color_black, summaryLineNum);
	}
}

void CCollisionHistory::DebugRenderCollisionRecords(const CPhysical* pThis, const bool bRenderText) const
{
	const Vec2V summaryScreenPos(0.03f, 0.05f);

	if( bRenderText )
	{
		const int tempStringLen = 512;
		char debugString[tempStringLen];

		//Summary for physical
		snprintf(debugString, tempStringLen, "[Frame CCollisionHistory of CPhysical 0x%p]", pThis);
		DebugRenderTextLineNormalisedScreenSpace(debugString, summaryScreenPos, Color_black, 0);

		snprintf(debugString, tempStringLen, "bHistoryRecorded: %d. bHistoryCompleteThisFrame: %d", m_bHistoryRecorded, m_bHistoryCompleteThisFrame);
		DebugRenderTextLineNormalisedScreenSpace(debugString, summaryScreenPos, Color_black, 1);
		
		snprintf(debugString, tempStringLen, "Num collided entities: %u. Collided entity type mask: 0x%X", m_numCollidedEntities, m_collidedEntityTypes);
		DebugRenderTextLineNormalisedScreenSpace(debugString, summaryScreenPos, Color_black, 2);
		
		snprintf(debugString, tempStringLen, "Impulse mag max: %f. Impulse mag sum: %f", m_fCollisionImpulseMagMax, m_fCollisionImpulseMagSum);
		DebugRenderTextLineNormalisedScreenSpace(debugString, summaryScreenPos, Color_black, 3);

		snprintf(debugString, tempStringLen, "Most significant record (largest impulse mag): 0x%p", m_pMostSignificantCollision);
		DebugRenderTextLineNormalisedScreenSpace(debugString, summaryScreenPos, Color_black, 4);

		snprintf(debugString, tempStringLen, "Persistent data: most recent collision time: %u", m_mostRecentCollisionTime);
		DebugRenderTextLineNormalisedScreenSpace(debugString, summaryScreenPos, Color_black, 5);
	}

	//All of the linked lists of records
	DebugRenderCollisionRecordList(GetFirstBuildingCollisionRecord(), "Building Collision Record", Color_GreenYellow, summaryScreenPos, 6, bRenderText, ms_nDebugRenderPersistFrames);
	DebugRenderCollisionRecordList(GetFirstVehicleCollisionRecord(), "Vehicle Collision Record", Color_MediumPurple3, summaryScreenPos, 7, bRenderText, ms_nDebugRenderPersistFrames);
	DebugRenderCollisionRecordList(GetFirstPedCollisionRecord(), "Ped Collision Record", Color_cyan3, summaryScreenPos, 8, bRenderText, ms_nDebugRenderPersistFrames);
	DebugRenderCollisionRecordList(GetFirstObjectCollisionRecord(), "Object Collision Record", Color_RoyalBlue, summaryScreenPos, 9, bRenderText, ms_nDebugRenderPersistFrames);
	DebugRenderCollisionRecordList(GetFirstOtherCollisionRecord(), "Other Collision Record", Color_tan1, summaryScreenPos, 10, bRenderText, ms_nDebugRenderPersistFrames);
}

#endif //_BANK
