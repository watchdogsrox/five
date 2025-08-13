//
// ai/EntityScanner.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef ENTITY_SCANNER_H
#define ENTITY_SCANNER_H

// Rage headers
#include "atl/queue.h"
#include "fwscene/search/Search.h"

// Game headers
#include "AI/ExpensiveProcess.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/TaskHelpers.h"
#include "Peds/Ped.h"

////////////////////////////////////////////////////////////////////////////////

class CEntityScannerIterator
{
public:

	CEntityScannerIterator()
		: m_pScanner(NULL)
		, m_pCurrent(NULL)
		, m_pEnd(NULL)
	{
	}

	CEntityScannerIterator(const CEntityScanner* pScanner);

	// Get the first entity
	const CEntity* GetFirst() const;

	// Get the first entity
	CEntity* GetFirst();

	// Get the next entity
	const CEntity* GetNext() const;

	// Get the next entity
	CEntity* GetNext();

	// Get the specified entry
	const CEntity* GetEntity(s32 iIndex) const;

	// Get the specified entry
	CEntity* GetEntity(s32 iIndex);

	// Get the number of entities
	const s32 GetCount() const;

private:
	static const RegdEnt* GetNextNonNull(const RegdEnt* curr, const RegdEnt* end);

	//
	// Members
	//

	// The entities
	const CEntityScanner* m_pScanner;
	mutable const RegdEnt* m_pCurrent;
	const RegdEnt* m_pEnd;
};

////////////////////////////////////////////////////////////////////////////////

class CEntityScanner : public CExpensiveProcess
{
	friend CEntityScannerIterator;
public:

	// Constants
	static const s32 MAX_NUM_ENTITIES = 16;

	CEntityScanner(ProcessType processType, s32 iScanPeriod = 16);
	virtual ~CEntityScanner();

	// Get the current closest entity - m_Entities is a sorted array by distance, so return element 0
	const CEntity* GetClosestEntityInRange() const { return m_Entities.GetCount() > 0 ? m_Entities[0].Get() : NULL; }
	CEntity* GetClosestEntityInRange() { return m_Entities.GetCount() > 0 ? m_Entities[0].Get() : NULL; }

	// Get an iterator to the entity list
	const CEntityScannerIterator GetIterator() const { return CEntityScannerIterator(this); }
	CEntityScannerIterator GetIterator() { return CEntityScannerIterator(this); }

	// Attempt to scan for entities
	void ScanForEntitiesInRange(CEntity& scanningEntity, bool bForceUpdate=false);

	// Clear the entity storage
	virtual void Clear();

	// Check if empty
	bool IsEmpty() const { return m_Entities.GetCount() == 0; }

	// Access the timer
	CTickCounter& GetTimer() { return m_Timer; }

	int GetNumEntities() const { return m_Entities.GetCount(); }
	CEntity* GetEntityByIndex(int index) const { return m_Entities[index]; }

#if DEBUG_DRAW
	// Debug rendering
	void DebugRender(const Color32& col) const;
#endif // DEBUG_DRAW

protected:

	// Scan entity structure
	struct sEntityInfo
	{
		sEntityInfo() {}
		sEntityInfo(CEntity* pEntity, const float fDistSqr) : pEntity(pEntity), fDistSqr(fDistSqr) {}

		// For sorting the array
		static s32 Compare(const sEntityInfo* a, const sEntityInfo* b) { return a->fDistSqr < b->fDistSqr ? -1 : 1; }
		static bool LessThan(const sEntityInfo& a, const sEntityInfo& b) { return PositiveFloatLessThan_Unsafe(a.fDistSqr, b.fDistSqr); }

		CEntity* pEntity;	// Entity
		float fDistSqr;		// Squared distance from scanning entity
	};
	typedef atFixedArray<sEntityInfo, MAX_NUM_ENTITIES> EntityInfos;

	// Per scanner implementation of entity searching
	virtual void ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities) = 0;

	// Callback data structure
	struct sEntityScannerCallbackData
	{
	public:

		sEntityScannerCallbackData(CEntity& scanningEntity, EntityInfos& entities, const Vector3& vCentre, const float fRadiusSqr)
			: scanningEntity(scanningEntity)
			, entities(entities)
			, vCentre(RCC_VEC3V(vCentre))
			, vRadiusSqr(fRadiusSqr)
		{
		}

		CEntity&		scanningEntity;
		EntityInfos&	entities;
		Vec3V			vCentre;
		ScalarV			vRadiusSqr;
	};

	// Per entity callback to build a list of nearby entities
	static bool ScanForEntitiesInRange_PerEntityCallback(CEntity* pEntity, void* pData);

	// Should we scan?
	virtual bool ShouldPerformScan(CEntity& scanningEntity) const;

	// Get the radius from the scanning entity
	float ComputeRadius(CEntity& scanningEntity) const;

	// Sort and store the passed in entities array
	void SortAndStoreEntities(EntityInfos& entities);

	//
	// Members
	//

	// Current list of entities
	typedef atFixedArray<RegdEnt, MAX_NUM_ENTITIES> Entities;
	Entities m_Entities;

	// Timer
	CTickCounter m_Timer;
};

////////////////////////////////////////////////////////////////////////////////

class CPedScanner : public CEntityScanner
{
public:

	CPedScanner(ProcessType processType);
	virtual ~CPedScanner();

	// Get the current closest ped
	const CPed* GetClosestPedInRange() const { return static_cast<const CPed*>(GetClosestEntityInRange()); }
	CPed* GetClosestPedInRange() { return static_cast<CPed*>(GetClosestEntityInRange()); }

protected:

	// Per scanner implementation of entity searching
	virtual void ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities);

	// Callback data structure
	struct sPedScannerCallbackData : public sEntityScannerCallbackData
	{
		sPedScannerCallbackData(CEntity& scanningEntity, EntityInfos& entities, const Vector3& vCentre, const float fRadiusSqr, const bool bIgnoreDeadPeds)
			: sEntityScannerCallbackData(scanningEntity, entities, vCentre, fRadiusSqr)
			, bIgnoreDeadPeds(bIgnoreDeadPeds)
		{
		}

		bool bIgnoreDeadPeds;
	};

	// Per entity callback to build a list of nearby entities
	static bool ScanForEntitiesInRange_PerEntityCallback(CEntity* pEntity, void* pData);
};

////////////////////////////////////////////////////////////////////////////////

class CVehicleScanner : public CEntityScanner
{
public:

	CVehicleScanner(ProcessType processType);
	virtual ~CVehicleScanner();

	// Get the current closest vehicle
	const CVehicle* GetClosestVehicleInRange() const { return static_cast<const CVehicle*>(GetClosestEntityInRange()); }
	CVehicle* GetClosestVehicleInRange() { return static_cast<CVehicle*>(GetClosestEntityInRange()); }

	// Should we scan?
	virtual bool ShouldPerformScan(CEntity& scanningEntity) const;

protected:

	// Per scanner implementation of entity searching
	virtual void ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities);

private:
	float m_fTimeLastUpdated;
};

////////////////////////////////////////////////////////////////////////////////

class CDoorScanner : public CEntityScanner
{
public:
	CDoorScanner(ProcessType processType);
	virtual ~CDoorScanner();

	// Get the current closest door
	const CDoor* GetClosestDoorInRange() const { return (const CDoor*)GetClosestEntityInRange(); }
	CDoor* GetClosestDoorInRange() { return (CDoor*)GetClosestEntityInRange(); }

protected:

	// Per scanner implementation of entity searching
	virtual void ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities);

	// Callback data structure
	struct sDoorScannerCallbackData : public sEntityScannerCallbackData
	{
		sDoorScannerCallbackData(CEntity& scanningEntity, EntityInfos& entities, const Vector3& vCentre, const float fRadiusSqr)
			: sEntityScannerCallbackData(scanningEntity, entities, vCentre, fRadiusSqr)
		{
		}
	};

	// Per entity callback to build a list of nearby entities
	static bool ScanForEntitiesInRange_PerEntityCallback(CEntity* pEntity, void* pData);

private:

	// Used to probe the world for door objects
	static dev_float ms_fScanningRadius;
};

////////////////////////////////////////////////////////////////////////////////

class CObjectScanner : public CEntityScanner
{
public:

	CObjectScanner(ProcessType processType);
	virtual ~CObjectScanner();

	// Get the current closest object
	const CObject* GetClosestObjectInRange() const { return static_cast<const CObject*>(GetClosestEntityInRange()); }
	CObject* GetClosestObjectInRange() { return static_cast<CObject*>(GetClosestEntityInRange()); }

	const CEntityScannerIterator GetIterator() const { return CEntityScannerIterator(this); }
	CEntityScannerIterator GetIterator() { return CEntityScannerIterator(this); }

	// Attempt to scan for entities
	void ScanForEntitiesInRange(CEntity& scanningEntity, bool bForceUpdate=false);

	void ProcessScanningEntityPosition(CEntity& scanningEntity, Vec3V_Ref vScanPositionInOut);

	virtual void Clear();

	static void StartAsyncUpdateOfObjectScanners();
	static void EndAsyncUpdateOfObjectScanners();

	// Since we're only doing 1 async object scanner update per frame, the object scanner 
	// queue size effectively determines the max delay (in frames) that the object scanner 
	// could have when an re-scan/update is requested. We'll keep this small to minimize 
	// delay, and if the queue is full, we'll just do an immediate scan (should be rare).
	#define OBJECT_SCANNER_QUEUE_SIZE 64
	static atQueue<CObjectScanner*,OBJECT_SCANNER_QUEUE_SIZE> ms_ObjectScannerUpdateQueue;

protected:
	RegdEnt m_LastScanningEntity;

	// Per scanner implementation of entity searching
	void ScanImmediately();
	virtual void ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities);

private:

	void RemoveObjectScannerFromUpdateQueue();

	bool StartAsyncUpdate(fwSearch* pSearchToUse);
	void EndAsyncUpdate(fwSearch* pSearchToUse);

	static bool AsyncUpdateResultsCB(fwEntity* pEntity, void* pData);

	#define OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES 16
	static CObjectScanner* ms_ObjectScannerCurrentlySearching[OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES];
	static fwSearch ms_ObjectScannerSearch[OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES];

	float m_fTimeLastUpdated;
};

////////////////////////////////////////////////////////////////////////////////

inline CEntityScannerIterator::CEntityScannerIterator(const CEntityScanner* pScanner)
	: m_pScanner(pScanner)
	, m_pCurrent(NULL)		   // Need to call GetFirst() before the iterator is valid
	, m_pEnd(pScanner->m_Entities.end())
{
	
}

inline const RegdEnt* CEntityScannerIterator::GetNextNonNull(const RegdEnt* curr, const RegdEnt* end)
{
	while(curr->Get() == NULL && curr != end)
	{
		curr++;
	}
	return curr;
}

inline const CEntity* CEntityScannerIterator::GetFirst() const
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");
	const RegdEnt* end = m_pEnd;
	const RegdEnt* curr = GetNextNonNull(m_pScanner->m_Entities.begin(), end);
	m_pCurrent = curr;
	return (curr == end) ? NULL : curr->Get();
}

////////////////////////////////////////////////////////////////////////////////

inline CEntity* CEntityScannerIterator::GetFirst()
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");
	const RegdEnt* end = m_pEnd;
	const RegdEnt* curr = GetNextNonNull(m_pScanner->m_Entities.begin(), end);
	m_pCurrent = curr;
	return (curr == end) ? NULL : curr->Get();
}

////////////////////////////////////////////////////////////////////////////////

inline const CEntity* CEntityScannerIterator::GetNext() const
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");	
	FastAssert(m_pCurrent);
	const RegdEnt* end = m_pEnd;
	const RegdEnt* curr = GetNextNonNull(m_pCurrent+1, end);
	m_pCurrent = curr;
	return (curr == end) ? NULL : curr->Get();
}

////////////////////////////////////////////////////////////////////////////////

inline CEntity* CEntityScannerIterator::GetNext()
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");
	FastAssert(m_pCurrent);
	const RegdEnt* end = m_pEnd;
	const RegdEnt* curr = GetNextNonNull(m_pCurrent+1, end);
	m_pCurrent = curr;
	return (curr == end) ? NULL : curr->Get();
}

////////////////////////////////////////////////////////////////////////////////

inline const CEntity* CEntityScannerIterator::GetEntity(s32 iIndex) const
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");
	aiFatalAssertf(iIndex >= 0 && iIndex < m_pScanner->m_Entities.GetCount(), "Index [%d] out of range 0..%d", iIndex, m_pScanner->m_Entities.GetCount());
	return m_pScanner->m_Entities[iIndex].Get();
}

////////////////////////////////////////////////////////////////////////////////

inline CEntity* CEntityScannerIterator::GetEntity(s32 iIndex)
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");
	aiFatalAssertf(iIndex >= 0 && iIndex < m_pScanner->m_Entities.GetCount(), "Index [%d] out of range 0..%d", iIndex, m_pScanner->m_Entities.GetCount());
	return m_pScanner->m_Entities[iIndex].Get();
}

////////////////////////////////////////////////////////////////////////////////

inline const s32 CEntityScannerIterator::GetCount() const
{
	aiFatalAssertf(m_pScanner, "Scanner not initialised");
	return m_pScanner->m_Entities.GetCount();
}

#endif // ENTITY_SCANNER_H
