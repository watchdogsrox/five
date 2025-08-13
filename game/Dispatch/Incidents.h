// Title	:	Incidents.h
// Purpose	:	Defines incidents that exist in the world such as accidents, crimes, fires etc...
// Started	:	13/05/2010

#ifndef INC_INCIDENTS_H_
#define INC_INCIDENTS_H_
// Rage headers
#include "Vector/Vector3.h"
// Framework headers
#include "fwutil/Flags.h"
#include "fwnet/nettypes.h"
// Game headers
#include "ai/aichannel.h"
#include "game/Dispatch/DispatchEnums.h"
#include "game/Dispatch/DispatchManager.h"
#include "network/General/NetworkUtil.h"
#include "peds/QueriableInterface.h"
#include "scene/RegdRefTypes.h"

class CFire;
class CEntity;
class CNetGamePlayer;

namespace rage
{
	class netPlayer;
}

// Single incident base class
class CIncident : public fwRefAwareBase
{
	friend class CIncidentManager;
public:

	FW_REGISTER_CLASS_POOL(CIncident);

	enum IncidentState {IS_Finished, IS_Active};

	enum IncidentType {IT_Invalid, IT_Wanted, IT_Fire, IT_Injury, IT_Scripted, IT_Arrest, IT_NumTypes};

	//based off the size of CIncident's resource array elements
	enum {MaxResourcesPerIncident = 256};

	CIncident(IncidentType eType);
	CIncident(CEntity* pEntity, const Vector3& vLocation, IncidentType eType, float fTime = 0.0f);
	virtual ~CIncident();

	__forceinline IncidentType GetType() const { return m_eType; }
	virtual CIncident* Clone() const = 0;

	void SetIncidentIndex(int index);
	s32 GetIncidentIndex() const { return m_incidentIndex; }

	void SetIncidentID(u32 id);
	u32 GetIncidentID() const { return m_incidentID; }

	// Get the number of resources requested/allocated and assigned
	inline s32 GetRequestedResources(s32 iDispatchType) const;
	inline s32 GetAllocatedResources(s32 iDispatchType) const;
	inline s32 GetAssignedResources(s32 iDispatchType) const;
	inline s32 GetArrivedResources(s32 iDispatchType) const;
	inline s32 GetFinishedResources(s32 iDispatchType) const;
	bool HasArrivedResources() const;

	inline void SetRequestedResources(s32 iDispatchType, s32 iCount);
	inline void SetAllocatedResources(s32 iDispatchType, s32 iCount);
	inline void SetAssignedResources(s32 iDispatchType, s32 iCount);
	inline void SetArrivedResources(s32 iDispatchType, s32 iCount);
	inline void SetFinishedResources(s32 iDispatchType, s32 iCount);

	void SetLocation( const Vector3& vLocation ) {m_vLocation = vLocation;}
	const Vector3& GetLocation() const { return m_vLocation;}

	float GetTimer() const { return m_fTimer; }

	// returns true if new resources should be assigned to the incident
	virtual bool GetShouldAllocateNewResourcesForIncident(s32 UNUSED_PARAM(iDispatchType)) const {return true;}

	bool HasEntity() const { return m_Entity.IsValid(); }
	CEntity* GetEntity() { return m_Entity.GetEntity(); }
	const CEntity* GetEntity() const { return m_Entity.GetEntity(); }
	void SetEntity(CEntity* pEntity) {m_Entity.SetEntity(pEntity);}
	void SetEntityID(ObjectId id) { m_Entity.SetEntityID(id); }

	const CPed* GetPed() const;

	bool GetHasBeenAddressed () const {return m_bHasBeenAddressed;}
	void SetHasBeenAddressed(bool bHasBeenAddressed) {m_bHasBeenAddressed = bHasBeenAddressed;}

	bool GetDisableResourceSpawning() const { return m_bDisableResourceSpawning; }
	void SetDisableResourceSpawning(bool bValue) { m_bDisableResourceSpawning = bValue; }

	bool GetIsIncidentValidIfDispatchDisabled() { return false; }

	bool IsEqual(const CIncident& otherIncident);

	bool ShouldCountFinishedResources() const;

	// NETWORK METHODS:
	bool IsLocallyControlled() const;
	const netPlayer* GetPlayerArbitrator() const;

	netObject* GetNetworkObject() const;

	void SetDirty();

	static u32 GetIncidentIDFromNetworkObjectID(netObject& netObj);
	static ObjectId GetNetworkObjectIDFromIncidentID(u32 id);

	// serialises the incident data sent across the network
	virtual void Serialise(CSyncDataBase& serialiser);

	// copies serialised incident data to this incident
	void CopyNetworkInfo(const CIncident& incident);

	// called when the incident has been created from a network update 
	virtual void NetworkInit() {}

	// called when arbitration of the incident migrates in a network game
	void Migrate(); 

	static void CopySelectiveResourceCounts(CIncident* pIncident, const CIncident* pIncidentCopyFrom);

#if __BANK
	virtual void DebugRender(Color32 color) const;
#endif //__BANK

protected:
	
	virtual void RequestDispatchServices() = 0;
	virtual IncidentState Update(float fTimeStep);
	virtual void UpdateRemote(float UNUSED_PARAM(fTimeStep)) {} // an update called on remotely controlled incidents in MP
	bool UpdateTimer(float fTimeStep);

private:

	// Entity and location associated with the incident
	CSyncedEntity m_Entity;
	Vector3 m_vLocation;
	float	m_fTimer;
	IncidentType m_eType;
	bool	m_bTimerSet : 1;
	bool	m_bHasEntity : 1;

	//we've done what we came to do, but aren't necessarily
	//ready to return to patrol
	bool	m_bHasBeenAddressed : 1;

	bool	m_bDisableResourceSpawning : 1;

	// the index this incident occupies in the incident manager array. If this is not set then the incident has probably been leaked!
	s16	m_incidentIndex;
	// this identifies the incident over the network, and is the network id of the entity the incident is associated with
	u32 m_incidentID;

	// Dispatch numbers per service
	u8 m_iDispatchAllocated[DT_Max]; // Number allocated by dispatcher
	u8 m_iDispatchAssigned[DT_Max];	// Number of resources actually assigned
	u8 m_iDispatchArrived[DT_Max];	// Number of resources actually arrived
	u8 m_iDispatchFinished[DT_Max];	// Number of resources finished responding

	// Dispatch numbers per service
	u8 m_iDispatchRequested[DT_Max]; // Number requested by incident
};

// Single incident base class
class CWantedIncident : public CIncident
{
private:

	enum RunningFlags
	{
		RF_IsTargetUnreachableViaRoad	= BIT0,
		RF_IsTargetUnreachableViaFoot	= BIT1,
	};

public:
	CWantedIncident();
	CWantedIncident(CEntity* pEntity, const Vector3& vLocation);
	~CWantedIncident();

	void AdjustResourceAllocations(DispatchType nDispatchType);
	void AdjustResourceRequests(DispatchType nDispatchType);
	void SetTimeLastPoliceHeliDestroyed(u32 uTime) { m_uTimeLastPoliceHeliDestroyed = uTime; }
	u32  GetTimeLastPoliceHeliDestroyed() { return m_uTimeLastPoliceHeliDestroyed; }
	int	GetWantedLevel() const;

	virtual IncidentType GetType() const { return IT_Wanted; }
	virtual CIncident* Clone() const { return rage_new CWantedIncident(const_cast<CEntity*>(GetEntity()), GetLocation()); }

	static bool IsIncidentNearby(const float fMaxDistance, const CEntity* pBaseEntity = NULL, const CIncident* pBaseIncident = NULL);
	static int	FindNearbyIncidents(const float fMaxDistance, CWantedIncident** aNearbyIncidents, const int iMaxNearbyIncidents, const CEntity* pBaseEntity = NULL, const CIncident* pBaseIncident = NULL);
	static bool ShouldIgnoreCombatEvent(const CPed& rPed, const CPed& rTarget);
	
	static float GetMaxDistanceForNearbyIncidents(const int iWantedLevel = WANTED_LEVEL_LAST - 1)
	{
		aiAssert((iWantedLevel >= WANTED_CLEAN) && (iWantedLevel <= WANTED_LEVEL5));

		return kMaxDistanceForNearbyIncidents[iWantedLevel];
	}

private:
	void	AdjustResourceAllocationsForMP(DispatchType nDispatchType);
	void	AdjustResourceRequestsForUnreachableLocation(DispatchType nDispatchType);

	bool	IsDispatchTypeUsed(DispatchType nDispatchType) const;
	bool	IsTargetUnreachableViaFoot() const;
	bool	IsTargetUnreachableViaRoad() const;
	void	UpdateTargetUnreachable();
	void	UpdateTargetUnreachableViaFoot();
	void	UpdateTargetUnreachableViaRoad();

	virtual void RequestDispatchServices();
	IncidentState Update(float UNUSED_PARAM(fTimeStep));
	void UpdateRemote(float UNUSED_PARAM(fTimeStep));

	static const float kMaxDistanceForNearbyIncidents[WANTED_LEVEL_LAST];

	u32			m_uTimeOfNextTargetUnreachableViaRoadUpdate;
	u32			m_uTimeOfNextTargetUnreachableViaFootUpdate;
	u32			m_uTimeLastPoliceHeliDestroyed;
	fwFlags8	m_uRunningFlags;

};


class CArrestIncident : public CIncident
{
public:
	CArrestIncident();
	CArrestIncident(CEntity* pEntity, const Vector3& vLocation);
	~CArrestIncident();

	virtual IncidentType GetType() const { return IT_Arrest; }
	virtual CIncident* Clone() const { return rage_new CArrestIncident(const_cast<CEntity*>(GetEntity()), GetLocation()); }

private:
	virtual void RequestDispatchServices();
	IncidentState Update(float fTimeStep);


};


class CFireIncident : public CIncident
{
public:
	enum {MaxFireIncidents = 32};

	enum {NumPedsPerDispatch = 4};

	enum {MaxFiresPerIncident = 16};

	enum {NumSpawnAttempts = 7};

	struct CFireInfo
	{
		CFireInfo()
		{
			pFire = NULL;
			pPedResponding = NULL;
		}
		CFireInfo(CFire* pNewFire)
		{
			pFire = pNewFire;
			pPedResponding = NULL;
		}
		bool operator ==(const CFireInfo& otherInfo) const
		{
			return pFire == otherInfo.pFire;
		}
		CFire* pFire;
		RegdPed pPedResponding;
	};
	typedef atFixedArray<CFireInfo, MaxFiresPerIncident> FireList;

	CFireIncident();
	CFireIncident(const FireList& rFireList, CEntity* pEntity, const Vector3& vLocation);
	~CFireIncident();
	CFire* GetBestFire(CPed* pPed = NULL) const;
	CFire* GetFire(s32 index) const;
	CPed* GetPedAttendingFire(s32 index) const;
	s32 GetNumFires() const;
	bool RemoveFire(CFire* pFire);
	void AddFire(CFire* pFire);
	void FindAllFires();
	bool AreAnyFiresActive() const;
	CVehicle* GetFireTruck() const {return m_pFireTruck;}
	void SetFireTruck(CVehicle* pFireTruck) {m_pFireTruck = pFireTruck;}
	bool ShouldClusterWith(CFire* pFire) const;
	static s32 GetCount() { return ms_iCount; }
	int GetNumFightingFire(CFire* pFire) const;
	int GetNumFightingFire(s32 index) const;
	void IncFightingFireCount(CFire* pFire, CPed* pPed);
	void DecFightingFireCount(CFire* pFire);
	CPed* GetPedRespondingToFire(CFire* pFire);
	CEntity* GetEntityThatHasBeenOnFire() const {return m_pRemainsForInvestigation;	}
	float GetFireIncidentRadius() const { return m_fFireIncidentRadius; }
	void IncrementSpawnAttempts() { ++m_iSpawnAttempts; }
	bool ExceededSpawnAttempts() const { return m_iSpawnAttempts > NumSpawnAttempts ? true : false; }

	virtual IncidentType GetType() const { return IT_Fire; }
	virtual CIncident* Clone() const 
	{ 
		CIncident* pIncident = rage_new CFireIncident(m_pFires, const_cast<CEntity*>(GetEntity()), GetLocation()); 
		return pIncident;
	}

	void NetworkInit() { FindAllFires(); }

#if __BANK
	virtual void DebugRender(Color32 color) const;
#endif //__BANK

private:
	virtual void RequestDispatchServices();
	IncidentState Update(float fTimeStep);
	virtual void UpdateRemote(float fTimeStep);
	void UpdateFires(float fTimeStep );
	FireList m_pFires;
	float m_fFireIncidentRadius;
	float m_fKeepFiresAliveTimer;
	
	//an interesting entity that we might want to inspect after a fire has gone out on it
	RegdEnt m_pRemainsForInvestigation;	
	RegdVeh	m_pFireTruck;
	static s32 ms_iCount;
	s32 m_iSpawnAttempts;

	//how close does a fire need to be to add it to the cluster?
	static const float sm_fClusterRangeXY;
	static const float sm_fClusterRangeZ;
};

class CInjuryIncident : public CIncident
{
public:
	CInjuryIncident();
	CInjuryIncident(CEntity* pEntity, const Vector3& vLocation);
	~CInjuryIncident();

	enum {NumPedsPerDispatch = 2};
	
	enum {NumSpawnAttempts = 7};

	virtual IncidentType GetType() const { return IT_Injury; }
	virtual CIncident* Clone() const 
	{ 
		CIncident* pIncident = rage_new CInjuryIncident(const_cast<CEntity*>(GetEntity()), GetLocation()); 
		return pIncident;
	}

	const CPed* GetRespondingPed() const {return m_pRespondingPed;	}
	void SetRespondingPed(CPed* pPed) {m_pRespondingPed = pPed;	}
	CVehicle* GetEmergencyVehicle() const {return m_pEmergencyVehicle;	}
	void SetEmergencyVehicle(CVehicle* pAmbulance) {m_pEmergencyVehicle = pAmbulance;	}
	const CPed* GetDriver() const {return m_pDrivingPed;	}
	void SetDriver(CPed* pDriver) {m_pDrivingPed = pDriver;	}

	static bool IsPedValidForInjuryIncident(const CPed* pPed, bool bVictimHasBeenHealed=false, bool bInitalCheck=true);

	void IncrementSpawnAttempts() { ++m_iSpawnAttempts; }
	bool ExceededSpawnAttempts() const { return m_iSpawnAttempts > NumSpawnAttempts ? true : false; }

private:
	bool ShouldDispatchAmbulance();
	virtual void RequestDispatchServices();
	IncidentState Update(float UNUSED_PARAM(fTimeStep));
	void UpdateRemote(float UNUSED_PARAM(fTimeStep));
	RegdPed m_pDrivingPed;
	RegdPed m_pRespondingPed;		//which ped's actually doing the healing
	RegdVeh m_pEmergencyVehicle;	//ambulance or other
	s32 m_iSpawnAttempts;
};

// Single incident base class
class CScriptIncident : public CIncident
{
public:
	CScriptIncident();
	CScriptIncident(CEntity* pEntity, const Vector3& vLocation, float fTime, bool bUseExistingPedsOnFoot = true, u32 uOverrideRelGroupHash = 0, int m_iAssassinDispatchLevel = AL_Invalid);
	~CScriptIncident();

	virtual IncidentType GetType() const { return IT_Scripted; }
	virtual CIncident* Clone() const 
	{ 
		CScriptIncident* pIncident = rage_new CScriptIncident(const_cast<CEntity*>(GetEntity()), GetLocation(), GetTimer(), m_UseExistingPedsOnFoot, m_uOverrideRelGroupHash, m_iAssassinDispatchLevel); 
		CopySelectiveResourceCounts(pIncident, this);
		return pIncident;
	}

	virtual bool GetShouldAllocateNewResourcesForIncident(s32 iDispatchType) const;

	float GetIdealSpawnDistance() const { return m_fIdealSpawnDistance; }
	void SetIdealSpawnDistance(float fValue) { m_fIdealSpawnDistance = fValue; }

	bool HasIdealSpawnDistance() const
	{
		return (m_fIdealSpawnDistance >= 0.0f);
	}

	bool GetUseExistingPedsOnFoot() const { return m_UseExistingPedsOnFoot; }
	void SetUseExistingPedsOnFoot(bool bValue) { m_UseExistingPedsOnFoot = bValue; }

	u32  GetOverrideRelationshipGroupHash() const { return m_uOverrideRelGroupHash; }
	void SetOverrideRelationshipGroupHash(u32 iHash) { m_uOverrideRelGroupHash = iHash; }

	int  GetAssassinDispatchLevel() const { return m_iAssassinDispatchLevel; }
	void SetAssassinDispatchLevel(int iLevel) { m_iAssassinDispatchLevel = iLevel; }

private:
	void RequestDispatchServices();
	IncidentState Update(float UNUSED_PARAM(fTimeStep));
	void UpdateRemote(float UNUSED_PARAM(fTimeStep));

private:

	float m_fIdealSpawnDistance;
	
	//mobile phone always sends a vehicle, don't use existing on foot peds
	bool	m_UseExistingPedsOnFoot : 1;

	u32 m_uOverrideRelGroupHash;

	int m_iAssassinDispatchLevel;

};

s32 CIncident::GetRequestedResources( s32 iDispatchType ) const
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	return m_iDispatchRequested[iDispatchType];
}

s32 CIncident::GetAllocatedResources( s32 iDispatchType ) const
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	return m_iDispatchAllocated[iDispatchType];
}

s32 CIncident::GetAssignedResources( s32 iDispatchType ) const
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	return m_iDispatchAssigned[iDispatchType];
}

s32 CIncident::GetArrivedResources( s32 iDispatchType ) const
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	return m_iDispatchArrived[iDispatchType];
}

s32 CIncident::GetFinishedResources( s32 iDispatchType ) const
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	return m_iDispatchFinished[iDispatchType];
}

void CIncident::SetAllocatedResources( s32 iDispatchType, s32 iCount )
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	aiAssertf(iCount < MaxResourcesPerIncident, "The count: %d is invalid, the maximum is: %d.", iCount, MaxResourcesPerIncident);
	m_iDispatchAllocated[iDispatchType] = (u8)iCount;
}

void CIncident::SetAssignedResources( s32 iDispatchType, s32 iCount )
{
	aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
	aiAssertf(iCount < MaxResourcesPerIncident, "The count: %d is invalid, the maximum is: %d.", iCount, MaxResourcesPerIncident);
	m_iDispatchAssigned[iDispatchType] = (u8)iCount;
}

void CIncident::SetRequestedResources( s32 iDispatchType, s32 iCount )
{
	if (m_iDispatchRequested[iDispatchType] != iCount)
	{
		aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
		aiAssert(IsLocallyControlled());
		aiAssertf(iCount < MaxResourcesPerIncident, "The count: %d is invalid, the maximum is: %d.", iCount, MaxResourcesPerIncident);
		m_iDispatchRequested[iDispatchType] = (u8)iCount;
		SetDirty();
	}
}

void CIncident::SetArrivedResources( s32 iDispatchType, s32 iCount )
{
	if (m_iDispatchArrived[iDispatchType] != iCount)
	{
		aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
		aiAssert(IsLocallyControlled());
		aiAssertf(iCount < MaxResourcesPerIncident, "The count: %d is invalid, the maximum is: %d.", iCount, MaxResourcesPerIncident);
		m_iDispatchArrived[iDispatchType] = (u8)iCount;
		SetDirty();
	}
}

void CIncident::SetFinishedResources( s32 iDispatchType, s32 iCount )
{
	if (m_iDispatchFinished[iDispatchType] != iCount)
	{
		aiAssert(iDispatchType >= 0 && iDispatchType < DT_Max);
		aiAssert(IsLocallyControlled());
		aiAssertf(iCount < MaxResourcesPerIncident, "The count: %d is invalid, the maximum is: %d.", iCount, MaxResourcesPerIncident);
		m_iDispatchFinished[iDispatchType] = (u8)iCount;
		SetDirty();
	}
}

#endif // INC_INCIDENT_H_
