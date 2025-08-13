// Title	:	DispatchServices.cpp
// Purpose	:	Defines the different dispatch services
// Started	:	13/05/2010
#ifndef INC_DISPATCH_SERVICES_H_
#define INC_DISPATCH_SERVICES_H_

// rage includes
#include "atl/array.h"
#include "ai/navmesh/datatypes.h"
#include "fwutil/Flags.h"

// Game includes
#include "game/Dispatch/DispatchEnums.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "game/dispatch/Orders.h"
#include "streaming/streamingrequest.h"
#include "task/System/TaskHelperFSM.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "vehicles/vehicle.h"

#if __BANK
#include "vector/colors.h"
#include "vector/color32.h"
#endif

// Forward declarations
class CConditionalVehicleSet;
class CIncident;
class CEntity;
class CDispatchResponse;
class CPedGroup;

#include "diag/channel.h"

//TODO: When this gets migrated to BOB, make it a sub-channel of dispatch.
RAGE_DECLARE_CHANNEL(dispatch_services)

#define dispatchServicesAssert(cond)						RAGE_ASSERT(dispatch_services,cond)
#define dispatchServicesAssertf(cond,fmt,...)				RAGE_ASSERTF(dispatch_services,cond,fmt,##__VA_ARGS__)
#define dispatchServicesFatalAssertf(cond,fmt,...)			RAGE_FATALASSERTF(dispatch_services,cond,fmt,##__VA_ARGS__)
#define dispatchServicesVerify(cond)						RAGE_VERIFY(dispatch_services,cond)
#define dispatchServicesVerifyf(cond,fmt,...)				RAGE_VERIFYF(dispatch_services,cond,fmt,##__VA_ARGS__)
#define dispatchServicesFatalf(fmt,...)						RAGE_FATALF(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesErrorf(fmt,...)						RAGE_ERRORF(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesWarningf(fmt,...)					RAGE_WARNINGF(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesDisplayf(fmt,...)					RAGE_DISPLAYF(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesDebugf1(fmt,...)					RAGE_DEBUGF1(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesDebugf2(fmt,...)					RAGE_DEBUGF2(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesDebugf3(fmt,...)					RAGE_DEBUGF3(dispatch_services,fmt,##__VA_ARGS__)
#define dispatchServicesLogf(severity,fmt,...)				RAGE_LOGF(dispatch_services,severity,fmt,##__VA_ARGS__)
#define dispatchServicesCondLogf(cond,severity,fmt,...)		RAGE_CONDLOGF(cond,dispatch_services,severity,fmt,##__VA_ARGS__)

class CDispatchService
{

protected:

	struct Resources
	{
		static const int MaxEntities = 16;

		Resources(int iCapacity)
		: m_iCapacity(Min(iCapacity, MaxEntities))
		{ aiAssertf(iCapacity <= MaxEntities, "Capacity: %d, Max: %d", iCapacity, MaxEntities); }

		CEntity* operator[](unsigned i) const
		{
			return m_Entities[i];
		}

		int GetCapacity() const
		{
			return m_iCapacity;
		}

		int GetCount() const
		{
			return m_Entities.GetCount();
		}

		int GetMaxCount() const
		{
			return (GetCapacity());
		}

		void Push(CEntity* pEntity)
		{
			m_Entities.Push(pEntity);
		}

	private:

		atFixedArray<CEntity *, MaxEntities> m_Entities;

		int m_iCapacity;
	};

public:
	enum {MaxResponseIncidents = 5, MinSpaceInPools = 10};

	CDispatchService(DispatchType dispatchType);
	virtual ~CDispatchService();

public:

	static u8 GetNumResourcesPerUnit(DispatchType nDispatchType);

public:

	DispatchType GetDispatchType() const { return m_iDispatchType; }

	static bank_u32 SEARCH_DISTANCE_IN_VEHICLE_LERP_TIME_MS;
	static bank_u32 SEARCH_DISTANCE_ON_FOOT_LERP_TIME_MS;
	static bank_float SEARCH_AREA_MOVE_SPEED;
	static bank_float SEARCH_AREA_MOVE_SPEED_HIGH_WANTED_LEVEL;
	static bank_float MINIMUM_SEARCH_AREA_RADIUS;
	static bank_float SEARCH_AREA_RADIUS;
	static bank_float SEARCH_AREA_HEIGHT;
	static bank_float DEFAULT_TIME_BETWEEN_SEARCH_POS_UPDATES;
	static bank_float MAX_SPEED_FOR_MODIFIER;
	static bank_float MIN_EMERGENCY_SERVICES_DISTANCE_FROM_WANTED_PLAYER;

	// Static helper functions for determining dispatch locations for police/swat
	static void UpdateCurrentSearchAreas(float& fTimeBetweenSearchPosUpdate);
	static bool IsLocationWithinSearchArea(const Vector3& vLocation);
	static float ComputeSpeedModifier(const CVehicle& vehicle);

	// static callback for delay-added peds
	static void DispatchDelayedSpawnStaticCallback(CEntity* pEntity, CIncident* pIncident, DispatchType iDispatchType, s32 pedGroupId, COrder* pOrder);

#if __BANK
	virtual Color32 GetColour() = 0;
	virtual const char* GetName() = 0;
	// Add widgets for this dispatch
	virtual void AddWidgets(bkBank *bank);
	virtual void AddExtraWidgets(bkBank *UNUSED_PARAM(bank)) {};
	virtual void Debug() const {};
	bool m_bRenderDebug;
	s32 m_iDebugRequestedNumbers;
#endif

	// Manages a scratchpad of prioritized response incidents that are currently being acted upon

	
	// Main Update fn
	virtual void Update();
	// Turns the dispatch service on and off
	void SetActive(const bool bEnable) {m_bDispatchActive = bEnable;}
	virtual bool IsActive() const { return m_bDispatchActive; }

	// Turns the dispatch service on and off
	void BlockResourceCreation(const bool bStop) {m_bBlockResourceCreation = bStop;}
	virtual bool IsResourceCreationBlocked() const { return m_bBlockResourceCreation; }

    // Returns the number of peds, vehicles and object this resource will require when they spawn
	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const = 0;

	// Should we allocate resources for the incident?
	virtual bool ShouldAllocateNewResourcesForIncident(const CIncident& rIncident) const;

	// Set spawn timer
	void SetSpawnTimer(float fTimeBetweenSpawns) { m_fSpawnTimer = fTimeBetweenSpawns; }
	float GetSpawnTimer() { return m_fSpawnTimer; }
	void ResetSpawnTimer();
	bool GetTimerTicking() const { return m_bTimerTicking; } 
		
	float GetTimeBetweenSpawnAttemptsOverride() const { return m_fTimeBetweenSpawnAttemptsOverride; }
	void ResetTimeBetweenSpawnAttemptsOverride() { m_fTimeBetweenSpawnAttemptsOverride = -1.0f; }
	void SetTimeBetweenSpawnAttemptsOverride(float fTimeBetweenSpawnAttempts) { m_fTimeBetweenSpawnAttemptsOverride = fTimeBetweenSpawnAttempts; }
	
	float GetTimeBetweenSpawnAttemptsMultiplierOverride() const { return m_fTimeBetweenSpawnAttemptsMultiplierOverride; }
	void ResetTimeBetweenSpawnAttemptsMultiplierOverride() { m_fTimeBetweenSpawnAttemptsMultiplierOverride = -1.0f; }
	void SetTimeBetweenSpawnAttemptsMultiplierOverride(float fTimeBetweenSpawnAttemptsMultiplier) { m_fTimeBetweenSpawnAttemptsMultiplierOverride = fTimeBetweenSpawnAttemptsMultiplier; }
	
	bool IsLawType() const;

	void Flush();
	
	static CDispatchSpawnHelpers& GetSpawnHelpers() { return ms_SpawnHelpers; }

public:

	static void Init();

public:

#if __BANK
	static const char* GetLastSpawnFailureReason(DispatchType nDispatchType);
#endif

private:
	// Internal update fns
	// Updates an internal list of incidents, sorted by priority
	void UpdateSortedIncidentList();
	// Calculates and sets the number of resources to be allocated to each incident
	void AllocateResourcesToIncidents();
	// Preserve still valid orders in the order list to give some continuity
	void PreserveStillValidOrders();
	// Timed resource spawning call
	void SpawnNewResourcesTimed(CIncident& rIncident, Resources& rResources);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources) = 0;
	// Assign orders to free resources
	virtual void AssignNewOrder(CEntity& rEntity, CIncident& rIncident);
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
	// Stream in required models
	virtual void StreamModels() = 0;
	// Get time between spawn attempts
	virtual float GetTimeBetweenSpawnAttempts() = 0;
	// Get time between spawn attempts multiplier
	virtual float GetTimeBetweenSpawnAttemptsMultiplier() const { return 1.0f; }
	// Updates our streaming requests (handles releasing and streaming)
	void UpdateModelStreamingRequests();
	// Manage resources for the incidents
	void ManageResourcesForIncidents();

	// Implemented by inheriting classes
	// Maximum response allocation
	virtual s32 GetMaximumResponseCount() = 0;

	virtual bool CanUseVehicleSet(const CConditionalVehicleSet& UNUSED_PARAM(rVehicleSet)) const { return true; }

	DispatchType m_iDispatchType;

protected:
	atArray<CIncident*> m_ResponseIncidents;

	// returns true if there is space for the peds/vehicles/objects needed
	virtual bool CanSpawnResources(const CIncident& rIncident, int iResourcesNeeded) const;
	
	// Is the passed entity valid for its order
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	
	// Is the passed entity valid for being assigned orders
	virtual bool GetIsEntityValidToBeAssignedOrder(CEntity* pEntity);

    // This is for verifying our vehicles/peds and getting the Ids we want to spawn
    bool GetAndVerifyModelIds(fwModelId &vehicleModelId, fwModelId &pedModelId, fwModelId& objModelId, int maxVehiclesInMap);

    // request all models in the response vehicle sets ... 
    void RequestAllModels(CDispatchResponse* pDispatchResponse);

	virtual void DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 groupId = -1, COrder* pOrder = NULL);

protected:

	Vec3V_Out	GenerateDispatchLocationForIncident(const CIncident& rIncident) const;

protected:

#if __BANK
		void ClearLastSpawnFailureReason();
		void SetLastSpawnFailureReason(const char* pReason) const;
#endif
    
private:

	void		AdjustResourceAllocations();
	void		AdjustResourceRequests();
	bool 		ArePedsInVehicleValidToBeAssignedOrder(const CVehicle& rVehicle, const Resources& rResources);
	bool		CanAcquireFreeResources(const CIncident& rIncident) const;
	void 		FindFreeResources(const CIncident& rIncident, Resources& rResources);
	void 		FindFreeResourcesFromNearby(const CIncident& rIncident, Resources& rResources);
	void 		FindFreeResourcesFromNearbyPedsInVehicles(const CIncident& rIncident, Resources& rResources);
	void 		FindFreeResourcesFromNearbyPedsOnFoot(const CIncident& rIncident, Resources& rResources);
	void 		FindFreeResourcesFromPedOnFoot(CPed& rPed, Resources& rResources);
	void 		FindFreeResourcesFromPedsInVehicle(const CVehicle& rVehicle, Resources& rResources);
	void		FindFreeResourcesFromPooled(const CIncident& rIncident, Resources& rResources);
	void		FindFreeResourcesFromPooledPedsInVehicles(const CIncident& rIncident, Resources& rResources);
	void		FindFreeResourcesFromPooledPedsOnFoot(const CIncident& rIncident, Resources& rResources);
	const CPed*	GetPedForIncident(const CIncident& rIncident) const;
	bool		IsPedValidToBeAssignedOrder(const CPed& rPed, const Resources& rResources);
	bool		ShouldStreamInModels() const;

protected:
	
	bool	m_bDispatchActive;  // Invalidates all current orders, everyone stops responding
	bool	m_bBlockResourceCreation; // Can continue to assign orders but new resources cannot be created

	bool	m_bTimerTicking;
	float	m_fSpawnTimer;
	float	m_fTimeBetweenSpawnAttemptsOverride;
	float	m_fTimeBetweenSpawnAttemptsMultiplierOverride;

	strRequestArray<12> m_streamingRequests;

    atArray<atHashWithStringNotFinal>* m_pDispatchVehicleSets;
    int m_CurrVehicleSet;
	int m_iForcedVehicleSet;

protected:

	static CDispatchSpawnHelpers	ms_SpawnHelpers;

private:

#if __BANK
	static const char* ms_aSpawnFailureReasons[DT_Max];
#endif
    
};

class CDispatchServiceWanted : public CDispatchService
{
public:

	CDispatchServiceWanted(DispatchType dispatchType)
		: CDispatchService(dispatchType)
	{}

	virtual void DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* pOrder = NULL);

protected:

	virtual float GetTimeBetweenSpawnAttemptsMultiplier() const;

    // Stream in required models
    virtual void StreamModels();

	// Stream in logic for wanted incidents
	void StreamWantedIncidentModels();

	// Get the CWanted of the incident ped (if there is one)
	static CWanted* GetIncidentPedWanted(const CIncident& rIncident);

	// Setup some variation information for the ped
	static void SetupPedVariation(CPed& rPed, eWantedLevel wantedLevel);
};

class CDispatchServiceEmergancy : public CDispatchService
{
public:

    CDispatchServiceEmergancy(DispatchType dispatchType)
        : CDispatchService(dispatchType)
    {}

protected:

	virtual bool IsActive() const;

    // Stream in required models
    virtual void StreamModels();

    // Stream in logic for service
    void RequestServiceModels();
};

class CPoliceAutomobileDispatch : public CDispatchServiceWanted
{

private:

	enum RunningFlags
	{
		RF_IsLastDispatchLocationValid	= BIT0,
	};

public:

	enum
	{
		MaxVehiclesInMap	= 12,
		MaxResponseCount	= 20,
	};

	CPoliceAutomobileDispatch(const DispatchType dispatchType = DT_PoliceAutomobile);
	virtual ~CPoliceAutomobileDispatch();
	
	virtual void AssignNewOrder(CEntity& rEntity, CIncident& rIncident);
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	virtual bool GetIsEntityValidToBeAssignedOrder(CEntity* pEntity);
	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	virtual void Update();
	
	static bool IsOrderValidForPed(const CPoliceOrder& rOrder, const CPed& rPed);
	static void RequestNewOrder(CPed& rPed, bool bCurrentOrderSucceeded);
	
#if __BANK
	virtual void	Debug() const;
	virtual Color32	GetColour() { return Color_blue; }
	virtual const char*	GetName() { return "PoliceAutomobile"; }
#endif
	
protected:

	virtual CVehicle* SpawnNewVehicleWithOrders(CDispatchSpawnHelper& rSpawnHelper, const fwModelId iModelId, CPoliceOrder::ePoliceDispatchOrderType& nDriverOrder, CPoliceOrder::ePoliceDispatchOrderType& nPassengerOrder);

private:

	virtual s32 GetMaximumResponseCount()
	{
		return MaxResponseCount;
	}

	virtual float GetTimeBetweenSpawnAttempts()
	{
		return 2.0f;
	}
	
private:

	bool		CanWaitCruising(const CDispatchSpawnHelper& rSpawnHelper) const;
	bool		CanWaitPulledOver(const CDispatchSpawnHelper& rSpawnHelper) const;
	bool		DoesAnyoneHaveOrder(int iPoliceOrderType, const CIncident& rIncident) const;
	Vec3V_Out	GenerateDispatchLocationForNewOrder(const CIncident& rIncident) const;
	float		GetChancesToForceWaitInFront(const CDispatchSpawnHelper& rSpawnHelper) const;
	bool		IsAnyoneWaitingInFront(const CDispatchSpawnHelper& rSpawnHelper) const;
	void		UpdateOrders();
	
protected:

	Vector3									m_vLastDispatchLocation;
	CPoliceOrder::ePoliceDispatchOrderType	m_nDriverOrderOverride;
	CPoliceOrder::ePoliceDispatchOrderType	m_nPassengerOrderOverride;
	float									m_fTimeBetweenSearchPosUpdate;
	fwFlags8								m_uRunningFlags;
	
public:

#if __BANK
	static CDebugDrawStore ms_debugDraw;
#endif

};

//////////////////////////////////////////////////////////////////////////
// Helicopter Dispatch base class
//////////////////////////////////////////////////////////////////////////

class CWantedHelicopterDispatch : public CDispatchServiceWanted
{

public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float	m_TimeBetweenSpawnAttempts;
		u32		m_MinSpawnTimeForPoliceHeliAfterDestroyed;

		PAR_PARSABLE;
	};
	
public:

	CWantedHelicopterDispatch(DispatchType dispatchType)
		: CDispatchServiceWanted(dispatchType)
		, m_iNumFailedClearAreaSearches(0)
		, m_fTimeUntilNextClearAreaSearch(0.0f)
		, m_bHasLocationForDropOff(false)
		, m_bRappelFromDropOffLocation(false)
		, m_bCheckingTacticalPointForDropOff(false)
		, m_bFailedToFindDropOffLocation(false)
	{}

	enum {MaxResponseCount = 5, MaxVehiclesInMap = 3};

	// Assign orders to free resources
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
	bool StartTacticalPointClearAreaSearch(const CEntity* pIncidentEntity, Vector3& rLocation);
	bool GetLocationForDropOff(const CEntity* pIncidentEntity, Vector3 &vSearchOrigin);

	virtual bool IsActive() const;
	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;
	virtual void DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* pOrder = NULL);
private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	virtual bool GetIsEntityValidToBeAssignedOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	float GetTimeBetweenSpawnAttempts();

private:

	bool CanDropOff(Vec3V_InOut vOrigin) const;
	bool IsPositionExcluded(Vec3V_In vPosition) const;
	bool IsPositionAlreadyInUse(Vec3V_In vPosition) const;
	void BuildExcludedOrderPositions();
	bool IsClearAreaPositionSafe(const Vector3& vPosition) const;

private:

	static const int sMaxPositionsToExclude = 8;
	static const int sMaxOrderPositionsToExclude = 4;
	atFixedArray<Vec3V, sMaxPositionsToExclude>	m_aExcludedDropOffPositions;
	atFixedArray<Vec3V, sMaxOrderPositionsToExclude> m_aExcludedOrderPositions;
	Vector3						m_vLocationForDropOff;
	float						m_fTimeUntilNextClearAreaSearch;
	CNavmeshClearAreaHelper		m_NavMeshClearAreaHelper;
	CNavmeshRouteSearchHelper	m_RouteSearchHelper;
	bool						m_bHasLocationForDropOff;
	bool						m_bRappelFromDropOffLocation;
	bool						m_bCheckingTacticalPointForDropOff;
	bool						m_bFailedToFindDropOffLocation;
	int							m_iNumFailedClearAreaSearches;
	
private:

	static Tunables sm_Tunables;
	
};

//////////////////////////////////////////////////////////////////////////
// Police helicopter dispatch (non-rappelling)
//////////////////////////////////////////////////////////////////////////

class CPoliceHelicopterDispatch : public CWantedHelicopterDispatch
{
public:
	CPoliceHelicopterDispatch() : CWantedHelicopterDispatch(DT_PoliceHelicopter) {}

#if __BANK
	virtual Color32 GetColour() { return Color_turquoise; }
	virtual const char* GetName() { return "PoliceHelicopter"; }
#endif
};

//////////////////////////////////////////////////////////////////////////
// Swat helicopter dispatch (w/ rappelling when possible)
//////////////////////////////////////////////////////////////////////////

class CSwatHelicopterDispatch : public CWantedHelicopterDispatch
{
public:
	CSwatHelicopterDispatch()
	: CWantedHelicopterDispatch(DT_SwatHelicopter)
	, m_fAdditionalPedsTimer(0.5f)
	{}

	virtual void Update();
	virtual void DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 groupId, COrder* pOrder = NULL);
#if __BANK
	virtual Color32 GetColour() { return Color_DarkOrange; }
	virtual const char* GetName() { return "SwatHelicopter"; }
#endif

private:
	bool SpawnAdditionalRappellingSwatPed(CVehicle& heli, int iSeatIndex, fwModelId pedModelId, s32 iGroupIndex, CSwatOrder& swatOrder);

	float m_fAdditionalPedsTimer;
};

//////////////////////////////////////////////////////////////////////////
// Swat automobile (non-heli) dispatch
//////////////////////////////////////////////////////////////////////////

class CSwatAutomobileDispatch : public CDispatchServiceWanted
{
public:

	enum
	{
		MaxResponseCount = 20,
		MaxVehiclesInMap = 12,
	};

public:
	CSwatAutomobileDispatch();

	// Assign orders to free resources
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);

	virtual void Update();

	void UpdateOrders();

	virtual void DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* pOrder = NULL);

#if __BANK
	virtual Color32 GetColour() { return Color_orange; }
	virtual const char* GetName() { return "SwatAutomobile"; }
	virtual void Debug() const;
	// Add widgets for this dispatch
	virtual void AddExtraWidgets(bkBank *bank);
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	float GetTimeBetweenSpawnAttempts() {return 2.0f;}

	Vector3						m_vLastDispatchLocation;
	float						m_fTimeBetweenSearchPosUpdate;
};

class CPoliceRidersDispatch : public CDispatchServiceWanted
{
public:
    CPoliceRidersDispatch();

    enum {MaxResponseCount = 6};	// Not sure.

    virtual void Update();

#if __BANK
    virtual Color32 GetColour() { return Color_blue; }
    virtual const char* GetName() { return "PoliceRiders"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

protected:
    virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
    virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
    // Spawn new resources if appropriate
    virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
    virtual float GetTimeBetweenSpawnAttempts();
};

class CAmbulanceDepartmentDispatch : public CDispatchServiceEmergancy
{
public:
	CAmbulanceDepartmentDispatch() : CDispatchServiceEmergancy(DT_AmbulanceDepartment) {}
	enum {MaxResponseCount = 2, MaxVehiclesInMap = 1};

#if __BANK
	virtual Color32 GetColour() { return Color_green; }
	virtual const char* GetName() { return "AmbulanceDepartment"; }
	virtual void AddExtraWidgets(bkBank *bank);
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	float GetTimeBetweenSpawnAttempts();

	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
};

class CFireDepartmentDispatch : public CDispatchServiceEmergancy
{
public:
    CFireDepartmentDispatch() : CDispatchServiceEmergancy(DT_FireDepartment) {}
	enum {MaxResponseCount = 8, MaxVehiclesInMap = 2};

#if __BANK
    virtual Color32 GetColour() { return Color_red; }
    virtual const char* GetName() { return "FireDepartment"; }
    virtual void AddExtraWidgets(bkBank *bank);
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
    virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
    virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
    // Spawn new resources if appropriate
    virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
    float GetTimeBetweenSpawnAttempts();
    virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
};


class CGangDispatch : public CDispatchServiceEmergancy
{
public:
	CGangDispatch() : CDispatchServiceEmergancy(DT_Gangs) {}
	enum {MaxResponseCount = 14, MaxVehiclesInMap = -1, NumPedsPerVehicle = 4};

#if __BANK
	virtual Color32 GetColour() { return Color_DarkOliveGreen; }
	virtual const char* GetName() { return "Gangs"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	float GetTimeBetweenSpawnAttempts() {return 1.0f;}
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
};

class CPoliceVehicleRequest : public CDispatchService
{
public:
	CPoliceVehicleRequest()
		: CDispatchService(DT_PoliceVehicleRequest)
	{}
	enum {MaxResponseCount = 4, MaxVehiclesInMap = 2};

	static void RequestNewOrder(CPed* pPed, bool bCurrentOrderSucceeded);

#if __BANK
	virtual Color32 GetColour() { return Color_green; }
	virtual const char* GetName() { return "PoliceVehicleRequest"; }
	// Add widgets for this dispatch
	virtual void AddExtraWidgets(bkBank *bank);
	virtual void Debug() const;
	static CDebugDrawStore ms_debugDraw;
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

protected:
	// Stream in required models
	virtual void StreamModels();

	virtual void Update();

	// Stream in logic for incidents
	void StreamWantedIncidentModels();

private:
	//void UpdateCurrentSearchAreas();
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	float GetTimeBetweenSpawnAttempts() {return 2.0f;}
	// Assign orders to free resources
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);

private:

	Vector3 m_vLastDispatchLocation;

};

class CPoliceRoadBlockDispatch : public CDispatchServiceWanted
{
public:

	CPoliceRoadBlockDispatch();
	enum {MaxResponseCount = 12, MaxVehiclesInMap = 12};

#if __BANK
	virtual Color32 GetColour() { return Color_blue; }
	virtual const char* GetName() { return "PoliceRoadBlock"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;
	virtual bool ShouldAllocateNewResourcesForIncident(const CIncident& rIncident) const;

protected:

	virtual bool CanUseVehicleSet(const CConditionalVehicleSet& rVehicleSet) const;
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	virtual void StreamModels();
	virtual float GetTimeBetweenSpawnAttempts();
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
	virtual void Update();
	
private:

	s32		GenerateType(const atArray<fwModelId>& aObjModelIds) const;
	bool	GetAndVerifyModelIds(atArray<fwModelId>& aVehModelIds, atArray<fwModelId>& aPedModelIds, atArray<fwModelId>& aObjModelIds);
	bool	GetAndVerifyModelIds(const atArray<atHashWithStringNotFinal>& aModelNames, atArray<fwModelId>& aModelIds) const;
	
private:

#if __BANK
	virtual void AddExtraWidgets(bkBank* pBank);
#endif
	
};

//=========================================================================
// CPoliceBoatDispatch
//=========================================================================

class CPoliceBoatDispatch : public CDispatchServiceWanted
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_TimeBetweenSpawnAttempts;

		PAR_PARSABLE;
	};
	
public:

	CPoliceBoatDispatch();
	enum {MaxResponseCount = 12, MaxVehiclesInMap = 12};

public:

	static bool IsInOrAroundWater(const CPed& rPed);
	static bool IsLocalPlayerInOrAroundWater();
	
public:

#if __BANK
	virtual Color32 GetColour() { return Color_blue; }
	virtual const char* GetName() { return "PoliceBoat"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;
	virtual bool ShouldAllocateNewResourcesForIncident(const CIncident& rIncident) const;

protected:

	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	virtual bool GetIsEntityValidToBeAssignedOrder(CEntity* pEntity);
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	virtual float GetTimeBetweenSpawnAttempts();
	virtual void AssignNewOrder(CEntity& rEntity, CIncident& rIncident);
	virtual void DispatchDelayedSpawnCallback(CEntity* pEntity, CIncident* pIncident, s32 pedGroupIndex, COrder* pOrder = NULL);
	virtual void StreamModels();
	
private:

	static Tunables sm_Tunables;

};

//=========================================================================
// CArmyVehicleDispatch
//=========================================================================

class CArmyVehicleDispatch : public CDispatchServiceEmergancy
{

public:

	CArmyVehicleDispatch() : CDispatchServiceEmergancy(DT_ArmyVehicle) {}
	enum {MaxResponseCount = 14, MaxVehiclesInMap = 16, NumPedsPerVehicle = 4};

#if __BANK
	virtual Color32 GetColour() { return Color_DarkOliveGreen; }
	virtual const char* GetName() { return "ArmyVehicle"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	virtual float GetTimeBetweenSpawnAttempts() {return 1.0f;}
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
};

//=========================================================================
// CBikerBackupDispatch
//=========================================================================

class CBikerBackupDispatch : public CDispatchServiceEmergancy
{

public:

	CBikerBackupDispatch() : CDispatchServiceEmergancy(DT_BikerBackup) {}
	enum {MaxResponseCount = 14, MaxVehiclesInMap = 16, NumPedsPerVehicle = 4};

#if __BANK
	virtual Color32 GetColour() { return Color_brown; }
	virtual const char* GetName() { return "BikerBackup"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	virtual float GetTimeBetweenSpawnAttempts() {return 1.0f;}
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);
};

//=========================================================================
// CAssassinsDispatch
//=========================================================================

class CAssassinsDispatch : public CDispatchServiceEmergancy
{

public:
	struct Tunables : CTuning
	{
		Tunables();

		float m_AssassinLvl1Accuracy;
		float m_AssassinLvl1ShootRate;
		float m_AssassinLvl1HealthMod;
		atHashString m_AssassinLvl1WeaponPrimary;
		atHashString m_AssassinLvl1WeaponSecondary;

		float m_AssassinLvl2Accuracy;
		float m_AssassinLvl2ShootRate;
		float m_AssassinLvl2HealthMod;
		atHashString m_AssassinLvl2WeaponPrimary; 
		atHashString m_AssassinLvl2WeaponSecondary;

		float m_AssassinLvl3Accuracy;
		float m_AssassinLvl3ShootRate;
		float m_AssassinLvl3HealthMod;
		atHashString m_AssassinLvl3WeaponPrimary; 
		atHashString m_AssassinLvl3WeaponSecondary;

		PAR_PARSABLE;
	};

	CAssassinsDispatch() : CDispatchServiceEmergancy(DT_Assassins) {}
	enum {MaxResponseCount = 14, MaxVehiclesInMap = 16, NumPedsPerVehicle = 4};

#if __BANK
	virtual Color32 GetColour() { return Color_bisque; }
	virtual const char* GetName() { return "Assassins"; }
#endif

	virtual void GetNumNetResourcesToReservePerIncident(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

private:
	virtual s32 GetMaximumResponseCount() {return MaxResponseCount;}
	virtual bool GetIsEntityValidForOrder(CEntity* pEntity);
	// Spawn new resources if appropriate
	virtual bool SpawnNewResources(CIncident& rIncident, Resources& rResources);
	virtual float GetTimeBetweenSpawnAttempts() {return 1.0f;}
	virtual void AssignNewOrders(CIncident& rIncident, const Resources& rResources);

	// Custom model request function
	bool GetAndVerifyModelIds(fwModelId &vehicleModelId, fwModelId &pedModelId, fwModelId& objModelId, int maxVehiclesInMap, int assassinsDispatchLevel);
	void SetupAssassinPed(CPed* pPed, int iAssassinsLevel, int pedCount);

	static Tunables ms_Tunables;
};

#endif
