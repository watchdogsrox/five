// Title	:	componentReserve.h
// Author	:	Bryan musson
// Started	:	06/04/2011
//
#ifndef _COMPONENTRESERVE_H_
#define _COMPONENTRESERVE_H_

// Game headers
#include "modelinfo/ModelSeatInfo.h"
#include "scene\RegdRefTypes.h"
#include "vehicles\VehicleDefines.h"

class CBaseModelInfo;
class CComponentReservationManager;
class CPed;

//-------------------------------------------------------------------------
// Class for reserving each component
//-------------------------------------------------------------------------
class CComponentReservation
{
public:
	enum {MAX_NUM_COMPONENTS = MAX_VEHICLE_SEATS + MAX_ENTRY_EXIT_POINTS};
	CComponentReservation();
	~CComponentReservation();

	void Init(CDynamicEntity* pDynamicEntity, CComponentReservationManager* pCRM);
	void SetBoneIndex( int iBoneIndex );
	

	// Reserves the use of the component for the ped passed
	bool	ReserveUseOfComponent		( CPed* pPed );
	// Un-reserves the use of the component for the ped passed
	bool	UnreserveUseOfComponent		( CPed* pPed );
	// Returns if this component is reserved for this ped
	bool	IsComponentReservedForPed	( const CPed* pPed ) const;
	// Lets the point know that the ped is still using this component
	void	SetPedStillUsingComponent	( CPed* pPed );
	// Returns if this component is already in use
	bool	IsComponentInUse			();
	// Update called each frame, to update component requests
	void	Update						();	

	int			 GetBoneIndex()	const	{ return m_iBoneIndex; }
	CPed*		 GetPedUsingComponent()	{ return m_pPedUsingComponent; };
	const CPed*	 GetPedUsingComponent()	const { return m_pPedUsingComponent; };

#if __BANK
	const CDynamicEntity* GetOwningEntity() const { return m_pMyEntity; }
	const CPed* GetLocalPedRequestingUseOfComponent() const { return m_pLocalPedRequestingUse; }
#endif // __BANK

	// Get index into the vehicles array that this component is stored in
	s32 GetVehicleArrayIndex();
	void SendRequest( bool bReserve );

	void ReceiveRequestResponse( CPed *pPed, bool bReserve, bool bGranted );
    void UpdatePedUsingComponentFromNetwork(CPed *pPed);

	static bank_bool DISPLAY_COMPONENT_USE;

protected:
	RegdPed			m_pPedUsingComponent;
	RegdPed			m_pLocalPedRequestingUse;	

	CComponentReservationManager*	m_pMyManager;
	CDynamicEntity*					m_pMyEntity; //for network access

private:
	u16				m_bAwaitingRequestResponse : 1;
	s16				m_requestTimer : 15;

protected:
	s16				m_iBoneIndex;
};

class CComponentReservationManager 
{
public:
	CComponentReservationManager();
	void Init(CDynamicEntity* pDynamicEntity, CBaseModelInfo* pModelInfo);

	// Should be called once per frame
	void Update();

	// Query functions for component reservation
	// Reserves arbitrary bones on the vehicle
	bool					GetAreAnyComponentsReserved();
	CComponentReservation*	FindComponentReservation(int iBoneIndex, bool bAddIfNotFound);
	s32						FindComponentReservationArrayIndex(int iBoneIndex);
	CComponentReservation*	FindComponentReservationByArrayIndex(s32 iReservationIndex);
	CComponentReservation*	AddComponentReservation(int iBoneIndex);
	CComponentReservation*	GetSeatReservation(s32 iEntryExitPoint, SeatAccess whichSeatAccess, s32 iSeat = -1);
	CComponentReservation*	GetDoorReservation(s32 iEntryExitPoint);
	CComponentReservation*	GetSeatReservationFromSeat( s32 whichSeat);
	u32						GetNumReserveComponents() const { return m_iNumReserveComponents; }

	static bool ReserveVehicleComponent( CPed* pPed, CDynamicEntity* pEntity, CComponentReservation* pComponentReservation );
	static void UnreserveVehicleComponent( CPed* pPed, CComponentReservation* pComponenetReservation );	
private:
	const CModelSeatInfo* GetModelSeatInfo();

	u32 m_iNumReserveComponents;
	CComponentReservation m_aComponentReservations[CComponentReservation::MAX_NUM_COMPONENTS];	

	CBaseModelInfo* m_pModelInfo;	
};


#endif // _COMPONENTRESERVE_H_


