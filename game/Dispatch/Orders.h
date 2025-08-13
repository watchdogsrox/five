// Title	:	Orders.h
// Purpose	:	Defines an order that is assigned to a particular ped
// Started	:	13/05/2010

#ifndef INC_ORDER_H_
#define INC_ORDER_H_
// Framework headers
#include "scene/Entity.h"
// Game headers
#include "game/Dispatch/DispatchEnums.h"
#include "game/Dispatch/IncidentManager.h"
#include "scene/RegdRefTypes.h"

// Forward decleration
class CEntity;
class CIncident;

namespace rage
{
	class netPlayer;
}
// Single incident base class
class COrder : public fwRefAwareBase
{
public:

	FW_REGISTER_CLASS_POOL(COrder);

	enum OrderType {OT_Invalid, OT_Basic, OT_Police, OT_Swat, OT_Ambulance, OT_Fire, OT_Gang, OT_NumTypes};

	COrder();
	COrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident);
	virtual ~COrder();

	// derived order classes must overload these:
	virtual OrderType GetType() const { return OT_Basic; }
	virtual COrder* Clone() const { return rage_new COrder(GetDispatchType(), const_cast<CEntity*>(GetEntity()), GetIncident()); }

	virtual bool	IsPoliceOrder() const { return false; }

	// Flags to determine if this order is to be cleared
	bool			GetFlagForDeletion() const { return m_flagForDeletion; }
	void			SetFlagForDeletion(bool val);
	void			SetOrderIndex(u32 index) {  m_orderIndex = static_cast<u16>(index); }
	u32				GetOrderIndex() const { return static_cast<u32>(m_orderIndex); }
	void			SetOrderID(u32 id) {  m_orderID = static_cast<u16>(id); }
	u32				GetOrderID() const { return static_cast<u32>(m_orderID); }
	DispatchType	GetDispatchType() const { return m_dispatchType; }
	CIncident*		GetIncident() const;
	u32				GetIncidentIndex() const { return static_cast<u32>(m_incidentIndex); }
	void			SetIncidentIndex(u32 index) { m_incidentIndex = static_cast<u16>(index); }
	u32				GetIncidentID() const { return m_incidentID; }
	void			SetIncidentID(u32 id) { m_incidentID = id; }
	CEntity*		GetEntity() { return m_Entity.GetEntity(); }
	const CEntity*	GetEntity() const { return m_Entity.GetEntity(); }
	void			SetEntityID(ObjectId id) { m_Entity.SetEntityID(id); }
	bool			GetAssignedToEntity() const { return (m_assignedToEntity && GetEntity()); }
	void			ClearAssignedToEntity() { m_assignedToEntity = false; }
	u32				GetTimeOfAssignment() const { return m_uTimeOfAssignment; }
	bool			GetIsValid() const;

	bool GetIncidentHasBeenAddressed() const;
	void SetIncidentHasBeenAddressed(bool bHasBeenAddressed);

	virtual void AssignOrderToEntity();
	void RemoveOrderFromEntity();

#if __BANK
	const char*		GetDispatchTypeName() const;
#endif
	void SetEntityArrivedAtIncident();
	bool GetEntityArrivedAtIncident() const
	{
		return m_entityArrivedAtIncident;
	}

	void SetEntityFinishedWithIncident();
	bool GetEntityFinishedWithIncident() const
	{
		return m_entityFinishedWithIncident;
	}

	bool GetAllResourcesAreDone() const;

	// NETWORK METHODS:

	bool IsLocallyControlled() const;
	bool IsRemotelyControlled() const;

	// serialises the order data sent across the network
	virtual void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, (unsigned&)m_dispatchType, SIZEOF_DISPATCH_TYPE, "Dispatch type");

		SERIALISE_UNSIGNED(serialiser, m_incidentID, SIZEOF_INCIDENT_ID, "Incident id");

		bool bEntityArrivedAtIncident = m_entityArrivedAtIncident;

		SERIALISE_BOOL(serialiser, bEntityArrivedAtIncident, "Entity arrived");

		m_entityArrivedAtIncident = bEntityArrivedAtIncident;

		bool bEntityFinishedWithIncident = m_entityFinishedWithIncident;

		SERIALISE_BOOL(serialiser, bEntityFinishedWithIncident, "Entity Finished");

		m_entityFinishedWithIncident = bEntityFinishedWithIncident;

		SERIALISE_UNSIGNED(serialiser, m_uTimeOfAssignment, SIZEOF_TIME_OF_ASSIGNMENT, "Time of assignment");
	}

	// copies serialised order data to this order
	virtual void CopyNetworkInfo(const COrder& order);

	static const unsigned SIZEOF_DISPATCH_TYPE = datBitsNeeded<DT_Max-1>::COUNT;
	static const unsigned SIZEOF_INCIDENT_ID = SIZEOF_OBJECTID+8;
	static const unsigned SIZEOF_TIME_OF_ASSIGNMENT = 32;

private:
	bool			m_flagForDeletion : 1;
	bool			m_entityArrivedAtIncident : 1;
	bool			m_entityFinishedWithIncident : 1;
	bool			m_assignedToEntity : 1;
	DispatchType	m_dispatchType;
	CSyncedEntity	m_Entity;
	u16				m_orderID;				// the id of the network object associated with the order, used to sync the order
	u16				m_orderIndex;			// the index of the order in the incident manager
	u32				m_incidentID;			// the id of the network object associated with the incident, used to sync the incident
	u16				m_incidentIndex;		// the index of the incident in the incident manager
	u32				m_uTimeOfAssignment;
};

class CPoliceOrder : public COrder
{
public:

	enum ePoliceDispatchOrderType
	{
		PO_NONE,
		PO_ARREST_GOTO_DISPATCH_LOCATION_AS_DRIVER,
		PO_ARREST_GOTO_DISPATCH_LOCATION_AS_PASSENGER,
		PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_DRIVER,
		PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_PASSENGER,
		PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT,
		PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER,
		PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER,
		PO_WANTED_SEARCH_FOR_PED_ON_FOOT,
		PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE,
		PO_WANTED_ATTEMPT_ARREST,
		PO_WANTED_COMBAT,
		PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER,
		PO_WANTED_WAIT_PULLED_OVER_AS_PASSENGER,
		PO_WANTED_WAIT_CRUISING_AS_DRIVER,
		PO_WANTED_WAIT_CRUISING_AS_PASSENGER,
		PO_WANTED_WAIT_AT_ROAD_BLOCK,
		PO_NUM_ORDER_TYPES
	};

	CPoliceOrder();
	CPoliceOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident, ePoliceDispatchOrderType ePoliceDispatchOrder, const Vector3& vDispatchLocation);
	virtual ~CPoliceOrder();

	virtual OrderType GetType() const { return OT_Police; }
	virtual COrder* Clone() const { return rage_new CPoliceOrder(GetDispatchType(), const_cast<CEntity*>(GetEntity()), GetIncident(), m_ePoliceDispatchOrder, m_vDispatchLocation); }

	virtual bool IsPoliceOrder() const { return true; }

	// The location the ped was dispatched to
	Vector3 GetDispatchLocation() const { return m_vDispatchLocation; }
	void	SetDispatchLocation(const Vector3& vDispatchLocation) { m_vDispatchLocation = vDispatchLocation; }

	// The search location the ped is going to
	Vector3 GetSearchLocation() const { return m_vSearchLocation; }
	void	SetSearchLocation(const Vector3& vSearchLocation) { m_vSearchLocation = vSearchLocation; }

	// The order type
	ePoliceDispatchOrderType GetPoliceOrderType() const { return m_ePoliceDispatchOrder; }
	void	SetPoliceOrderType(ePoliceDispatchOrderType ot) { m_ePoliceDispatchOrder = ot; }

	bool	GetNeedsToUpdateLocation() const { return m_bNeedsToUpdateLocation; }
	void	SetNeedsToUpdateLocation(bool bUpdate) { m_bNeedsToUpdateLocation = bUpdate; }

	// NETWORK METHODS:

	void Serialise(CSyncDataBase& serialiser)
	{
		COrder::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_vDispatchLocation, "Dispatch location");
		SERIALISE_POSITION(serialiser, m_vSearchLocation, "Search location");
		SERIALISE_BOOL(serialiser, m_bNeedsToUpdateLocation, "Needs to update location");
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_ePoliceDispatchOrder), SIZE_OF_POLICE_DISPATCH_ORDER, "Police Dispatch Order");
	}

	// copies serialised order data to this order
	virtual void CopyNetworkInfo(const COrder& order)
	{
		COrder::CopyNetworkInfo(order);

		const CPoliceOrder* pPoliceOrder = SafeCast(const CPoliceOrder, &order);

		if (pPoliceOrder)
		{
			m_vDispatchLocation			= pPoliceOrder->m_vDispatchLocation;
			m_vSearchLocation			= pPoliceOrder->m_vSearchLocation;
			m_bNeedsToUpdateLocation	= pPoliceOrder->m_bNeedsToUpdateLocation;
			m_ePoliceDispatchOrder		= pPoliceOrder->m_ePoliceDispatchOrder;
		}
	}

	static const unsigned SIZE_OF_POLICE_DISPATCH_ORDER = datBitsNeeded<PO_NUM_ORDER_TYPES-1>::COUNT;

private:

	Vector3						m_vDispatchLocation;
	Vector3						m_vSearchLocation;
	bool						m_bNeedsToUpdateLocation;
	ePoliceDispatchOrderType	m_ePoliceDispatchOrder;
};

class CSwatOrder : public COrder
{
public:

	enum eSwatDispatchOrderType
	{
		SO_NONE,
		SO_WANTED_GOTO_STAGING_LOCATION_AS_DRIVER,
		SO_WANTED_GOTO_STAGING_LOCATION_AS_PASSENGER,
		SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER,
		SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER,
		SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER,
		SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER,
		SO_WANTED_APPROACH_TARGET_IN_PAIRS,
		SO_WANTED_APPROACH_TARGET_IN_LINE,
		SO_WANTED_COMBAT,
		SO_NUM_ORDER_TYPES
	};

	CSwatOrder();
	CSwatOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident, eSwatDispatchOrderType eSwatDispatchOrder, const Vector3& vStagingLocation, bool bAdditionalRappellingOrder);
	virtual ~CSwatOrder();

	virtual OrderType GetType() const { return OT_Swat; }
	virtual COrder* Clone() const
	{ 
		CSwatOrder* order = rage_new CSwatOrder(GetDispatchType(), const_cast<CEntity*>(GetEntity()), GetIncident(), GetSwatOrderType(), GetStagingLocation(), m_bAdditionalRappellingOrder);
		order->SetAllowAdditionalPed(GetAllowAdditionalPed());
		return order;
	}

	virtual void AssignOrderToEntity();

	// The location the ped was dispatched to
	const Vector3& GetStagingLocation() const { return m_vStagingLocation; }
	void	SetStagingLocation(const Vector3& vStagingLocation) { m_vStagingLocation = vStagingLocation; }

	// The order type
	eSwatDispatchOrderType GetSwatOrderType() const { return m_eSwatDispatchOrder; }
	void	SetSwatOrderType(eSwatDispatchOrderType ot) { m_eSwatDispatchOrder = ot; }

	bool	GetNeedsToUpdateLocation() const { return m_bNeedsToUpdateLocation; }
	void	SetNeedsToUpdateLocation(bool bUpdate) { m_bNeedsToUpdateLocation = bUpdate; }

	bool	GetAllowAdditionalPed() const { return m_bAllowAdditionalPed; }
	void	SetAllowAdditionalPed(bool bAllowAdditionalPed) { m_bAllowAdditionalPed = bAllowAdditionalPed; }

	// NETWORK METHODS:

	void Serialise(CSyncDataBase& serialiser)
	{
		COrder::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_vStagingLocation, "Staging location");
		SERIALISE_BOOL(serialiser, m_bNeedsToUpdateLocation, "Needs to update location");
		SERIALISE_BOOL(serialiser, m_bAdditionalRappellingOrder, "Additional rappelling order");
		SERIALISE_BOOL(serialiser, m_bAllowAdditionalPed, "Is last swat order");
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_eSwatDispatchOrder), SIZE_OF_SWAT_DISPATCH_ORDER, "Swat Dispatch Order");
	}

	// copies serialised order data to this order
	virtual void CopyNetworkInfo(const COrder& order)
	{
		COrder::CopyNetworkInfo(order);

		const CSwatOrder* pSwatOrder = SafeCast(const CSwatOrder, &order);

		if (pSwatOrder)
		{
			m_vStagingLocation				= pSwatOrder->m_vStagingLocation;
			m_bNeedsToUpdateLocation		= pSwatOrder->m_bNeedsToUpdateLocation;
			m_bAllowAdditionalPed			= pSwatOrder->m_bAllowAdditionalPed;
			m_bAdditionalRappellingOrder	= pSwatOrder->m_bAdditionalRappellingOrder;
			m_eSwatDispatchOrder			= pSwatOrder->m_eSwatDispatchOrder;
		}
	}

	static const unsigned SIZE_OF_SWAT_DISPATCH_ORDER = datBitsNeeded<SO_NUM_ORDER_TYPES-1>::COUNT;

private:

	Vector3						m_vStagingLocation;
	bool						m_bNeedsToUpdateLocation;
	bool						m_bAllowAdditionalPed;
	bool						m_bAdditionalRappellingOrder;
	eSwatDispatchOrderType		m_eSwatDispatchOrder;
};

class CAmbulanceOrder : public COrder
{
public:
	CAmbulanceOrder();
	CAmbulanceOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident);
	virtual ~CAmbulanceOrder();

	// derived order classes must overload these:
	virtual OrderType GetType() const { return OT_Ambulance; }
	virtual COrder* Clone() const { return rage_new CAmbulanceOrder(GetDispatchType(), const_cast<CEntity*>(GetEntity()), GetIncident()); }

	//interface for CInjuryIncident
	const CPed* GetDriver() const;
	const CPed* GetRespondingPed() const;
	CVehicle* GetEmergencyVehicle() const;

	void SetDriver(CPed* pDriver);
	void SetRespondingPed(CPed* pMedic);
	void SetEmergencyVehicle(CVehicle* pAmbulance);
	CInjuryIncident* GetInjuryIncident() const;
};

class CFireOrder : public COrder
{
public:
	CFireOrder();
	CFireOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident);
	virtual ~CFireOrder();

	// derived order classes must overload these:
	virtual OrderType GetType() const { return OT_Fire; }
	virtual COrder* Clone() const { return rage_new CFireOrder(GetDispatchType(), const_cast<CEntity*>(GetEntity()), GetIncident()); }

	CVehicle* GetFireTruck() const;
	void SetFireTruck(CVehicle* pFireTruck);
	CFire* GetBestFire(CPed* pPed) const;
	int GetNumFires() const;
	CEntity* GetEntityThatHasBeenOnFire() const;
	CFireIncident* GetFireIncident() const;

	void IncFightingFireCount(CFire* pFire, CPed* pPed);
	void DecFightingFireCount(CFire* pFire);
	CPed* GetPedRespondingToFire(CFire* pFire);
};

class CGangOrder : public COrder
{
public:
	CGangOrder();
	CGangOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident);
	virtual ~CGangOrder();

	// derived order classes must overload these:
	virtual OrderType GetType() const { return OT_Gang; }
	virtual COrder* Clone() const { return rage_new CGangOrder(GetDispatchType(), const_cast<CEntity*>(GetEntity()), GetIncident()); }

	virtual void AssignOrderToEntity();
};

#endif // INC_ORDER_H_
