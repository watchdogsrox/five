// 
// physics/CollisionRecords.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef COLLISION_RECORDS_H
#define COLLISION_RECORDS_H

//Rage headers
#include "phcore\materialmgr.h"
#include "atl\array.h"

//Game headers
#include "scene\Entity.h"
#include "scene\RegdRefTypes.h"

//Parameters
#define COLLISIONHISTORY_RECORDSTORESIZE 600

//Build configuration defines
#define DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS (0 && __DEV)
#define DEBUG_MEASURE_COLLISION_RECORD_HIGH_WATER_MARK (1 && __BANK)

//Constants
const u32 COLLISION_HISTORY_TIMER_NEVER_HIT = 0xFFFFFFFF;

class CPhysical;

//There may be a multitude of phContacts between a pair of CEntities each frame
//We hold onto the one with the largest impulse magnitude and keep a sum of all of the phContacts' impulses
struct CCollisionRecord
{
	//Collided-with/other entity
	RegdEnt m_pRegdCollisionEntity;   //Use this when reading because entities may be deleted during the frame before the records get iterated over

	//A copy of the phContact this frame between the two entities that has the largest impulse. 
	//TODO: if possible (likely when single-stepping physics) replace this with phContact* m_pContact
	//struct
	//{
		int	m_MyCollisionComponent;
		int	m_OtherCollisionComponent;
		float m_fCollisionImpulseMag;  //The impulse that was applied to this CEntity at this phContact this frame by the solver
		Vector3 m_MyCollisionNormal;   //Points into me
		Vector3 m_MyCollisionPos;
		Vector3 m_MyCollisionPosLocal;
		Vector3 m_OtherCollisionPos;
		phMaterialMgr::Id m_OtherCollisionMaterialId;
	//}

	const CCollisionRecord* GetNext() const { return m_pNext; }
	
private:

	//Linked list
	CEntity* m_ListSortKey;	//Sort key for the linked lists (the CEntity address). The CEntity may have ceased existing since the record was made though, so use the RegdEnt when reading
	CCollisionRecord* m_pNext;

	friend class CCollisionHistory;
};

//Stores one CCollisionRecord for every entity that collides with the owning CPhysical
//Collision history is cleared every frame, so you can, for example, deduce if something is in the air this frame by seeing if the history is empty
//The history is built right after the phSimulator's integration.
//Because there are multiple sim updates per frame, the collision from all of these are merged
//History will be "complete" (has a record for every entity that this one is intersecting)
//  for a tick if at the start of the tick the inst had a collider, otherwise it can't be assumed to be more than partial
class CCollisionHistory
{
public:

	CCollisionHistory(bool bUseCollisionRecords = true);

	// Assumes that the entire CCollisionRecords store is also being freed as part of the reset process, so we just need to NULL the linked list heads.
	void Reset();
	
	void NotifyHistoryWillBeCompleteThisTick(); //Called on CPhysicals that have colliders just before the beginning of a phSimulator::Update
	bool IsHistoryComplete() const {return m_bHistoryCompleteThisFrame; } //Does the history for the frame contain every entity that this one is/was intersecting/touching?

	void SetHistoryRecorded(bool bHistoryRecorded=true) { m_bHistoryRecorded = bHistoryRecorded; }
	bool IsHistoryRecorded() const { return m_bHistoryRecorded; }

	u32	GetMostRecentCollisionTime() const { return m_mostRecentCollisionTime; } //Milliseconds since game start. COLLISION_HISTORY_TIMER_NEVER_HIT means never
	
	//Returns NULL if no record exists
	const CCollisionRecord* FindCollisionRecord(const CEntity* pEntityToFind) const;

	// Returns true if this collision history was updated this frame.
	bool IsCurrent() const { return m_LastValidFrame == ms_CurrentFrame; }

	//This function needs to be really efficient due to a heavy load
	//Ask IsHistoryEnabled whether to call this
	void AddCollisionRecord(phInst const * myInst, CEntity* pOtherEntity, const Vector3& vecImpulse, const Vector3& vecNormal, const Vector3& vecMyPos, 
								const Vector3& vecOtherPos, int nMyComp, int nOtherComp, phMaterialMgr::Id nMat, bool bPositiveDepth, bool bNewContact);
	
	//Max and total impulses last frame
	float GetMaxCollisionImpulseMagLastFrame() const { return m_fCollisionImpulseMagMax; }
	float GetMaxCollisionImpulseMagLastFrameOfType(u16 entityType) const;
	float GetMaxCollisionImpulseMagLastFrameOfTypes(u16 entityTypeMask) const;

	//A "global" sum of the impulses on the entity this frame maintained independently of the individual records
	float GetCollisionImpulseMagSum() const { return m_fCollisionImpulseMagSum; }
	void  SetZeroCollisionImpulseMagSum() { m_fCollisionImpulseMagSum = 0.0f; }	

	//Cheap ways of finding out if there have been any collisions
	u32 GetNumCollidedEntities() const { return m_numCollidedEntities; }
	bool HasCollidedWithAnyOfTypes(u16 entityTypeMask) const { return (entityTypeMask & m_collidedEntityTypes) != 0u; }
	bool HasCollidedWithAnyOfTypesThisFrame(u16 entityTypeMask) const { return (entityTypeMask & m_collidedEntityTypesThisFrame) != 0u; }
	
	//Collision record linked-list iteration based on type
	const CCollisionRecord* GetFirstBuildingCollisionRecord() const { return m_pBuildingCollisions; } //Buildings and animated buildings
	const CCollisionRecord* GetFirstVehicleCollisionRecord() const { return m_pVehicleCollisions; }
	const CCollisionRecord* GetFirstPedCollisionRecord() const { return m_pPedCollisions; }
	const CCollisionRecord* GetFirstObjectCollisionRecord() const { return m_pObjectCollisions; }
	const CCollisionRecord* GetFirstOtherCollisionRecord() const { return m_pOtherCollisions; } //None of the above

	//Most significant record (the one with the largest accumulated impulse this frame between this entity and the other one). 
	//Not for iterating (m_pNext doesn't take you to the 2nd most significant record)
	const CCollisionRecord* GetMostSignificantCollisionRecord() const { return m_pMostSignificantCollision; }
	const CCollisionRecord* GetMostSignificantCollisionRecordOfType(u16 entityType) const;
	const CCollisionRecord* GetMostSignificantCollisionRecordOfTypes(u16 entityTypeMask) const;

	static void ClearRecords(); //Wipe all the records ready for a new frame
	static void PreSimUpdate();		//Called once a tick after all activations and deactivations of insts have occurred

#if DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS
	bool DebugValidateLinkedLists() const;
#endif

#if __BANK
	void DebugRenderCollisionRecords(const CPhysical* pThis, const bool bRenderText) const;
#endif


private:

	void SetCollidedWithTypes(u16 entityTypeMask) { m_collidedEntityTypes |= entityTypeMask; }
	void SetCollidedWithTypesThisFrame(u16 entityTypeMask) { m_collidedEntityTypesThisFrame |= entityTypeMask; }

	//Returns a reference to the pointer that is to be made to point at the new collision record.
	//If the record already exists then returns a reference to the pointer to it
	//CCollisionRecords are hashed into buckets based on type.
	//A bucket's data is stored in a linked list which is always sorted by CEntity address
	CCollisionRecord*& GetInsertionPoint(CEntity* pEntityToInsert);

	//*** Data cleared every frame ***

	//Linked lists sorted by entity's address (as a CEntity) in ascending order
	CCollisionRecord* m_pBuildingCollisions; //Includes animated buildings too
	CCollisionRecord* m_pVehicleCollisions;
	CCollisionRecord* m_pPedCollisions;
	CCollisionRecord* m_pObjectCollisions;
	CCollisionRecord* m_pOtherCollisions;

	CCollisionRecord* m_pMostSignificantCollision; //The collision record (also in one of the linked lists) with the largest impulse mag sum

	float m_fCollisionImpulseMagMax;			//Largest impulse magnitude this frame
	float m_fCollisionImpulseMagSum;			//Sum of all impulse magnitudes this frame (i.e. summed over all phContacts with all other CEntities)

	u16 m_collidedEntityTypes;					//Mask of entity types collided with this frame
	u16 m_collidedEntityTypesThisFrame;			//Mask of entity types that got new contacts this frame
	u16 m_numCollidedEntities;

	bool m_bHistoryCompleteThisFrame;			//When insts are inactive at the start of phSimulator::Update only partial or no contact information will be received
												//If there are any ticks in which we receive complete contact history, then regard the combined history to be sufficiently complete

	//*** Persistent data ***
	bool m_bHistoryRecorded;					//Does entity record collisions with other entities?
	u32 m_mostRecentCollisionTime;				//0xFFFFFFFF means never
	u32 m_LastValidFrame;

	//Static data
	static atFixedArray<CCollisionRecord, COLLISIONHISTORY_RECORDSTORESIZE> ms_recordStore;
	
	static u32 ms_CurrentFrame;
#if DEBUG_VALIDATE_COLLISION_RECORD_LINKED_LISTS
	bool DebugValidateLinkedList(const CCollisionRecord* pList, u16 entityTypesInList, const char* listName) const;
#endif

#if __BANK
	static void DebugRenderTextLineNormalisedScreenSpace(const char* text, Vec2V_In screenPos, const Color32 textCol, const int lineNum);
	static void DebugRenderTextLineWorldSpace(const char* text, const Vector3& worldPos, const Color32 textCol, const int lineNum, int persistFrames);
	static void DebugRenderCollisionRecordList(const CCollisionRecord* pList, const char * listName, const Color32 recordCol, Vec2V_In summaryScreenPos, int summaryLineNum, const bool bRenderText, int persistFrames);

	friend class CPhysics;
	static float ms_fDebugRenderContactRadius;
	static float ms_fDebugRenderContactNormal;
	static int   ms_nDebugRenderPersistFrames;

	#if DEBUG_MEASURE_COLLISION_RECORD_HIGH_WATER_MARK
		public:
		static int	 ms_nHighWaterMark;
		static int	 ms_nHighWaterMarkLastFrame;
		private:
	#endif

#endif

};

//For if you want to iterate over some number of the internal linked lists
class CCollisionHistoryIterator
{
public:

	CCollisionHistoryIterator(const CCollisionHistory* pHistory, bool bIterateBuildings=true, bool bIterateVehicles=true, 
							  bool bIteratePeds=true, bool bIterateObjects=true, bool bIterateOthers=true)
							: m_pHistory(pHistory), m_pCurRecord(NULL), m_bIterateBuildings(bIterateBuildings), m_bIterateVehicles(bIterateVehicles), 
							  m_bIteratePeds(bIteratePeds), m_bIterateObjects(bIterateObjects), m_bIterateOthers(bIterateOthers) {}
	
	const CCollisionRecord* GetNext()
	{
		if(m_pCurRecord) //Get the next record in the list currently being iterated
		{
			m_pCurRecord = m_pCurRecord->GetNext();
			if(m_pCurRecord) return m_pCurRecord;
		}

		if(m_bIterateBuildings) //Start iterating over buildings
		{
			m_bIterateBuildings = false;
			m_pCurRecord = m_pHistory->GetFirstBuildingCollisionRecord();
			if(m_pCurRecord) return m_pCurRecord;
		}

		if(m_bIterateVehicles)
		{
			m_bIterateVehicles = false;
			m_pCurRecord = m_pHistory->GetFirstVehicleCollisionRecord();
			if(m_pCurRecord) return m_pCurRecord;
		}

		if(m_bIteratePeds)
		{
			m_bIteratePeds = false;
			m_pCurRecord = m_pHistory->GetFirstPedCollisionRecord();
			if(m_pCurRecord) return m_pCurRecord;
		}

		if(m_bIterateObjects)
		{
			m_bIterateObjects = false;
			m_pCurRecord = m_pHistory->GetFirstObjectCollisionRecord();
			if(m_pCurRecord) return m_pCurRecord;
		}

		if(m_bIterateOthers)
		{
			m_bIterateOthers = false;
			m_pCurRecord = m_pHistory->GetFirstOtherCollisionRecord();
			if(m_pCurRecord) return m_pCurRecord;
		}

		return NULL;
	}

private:

	const CCollisionHistory* m_pHistory;
	const CCollisionRecord* m_pCurRecord;

	bool m_bIterateBuildings : 1;
	bool m_bIterateVehicles : 1;
	bool m_bIteratePeds : 1;
	bool m_bIterateObjects : 1;
	bool m_bIterateOthers : 1;
};

#endif
